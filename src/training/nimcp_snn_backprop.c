#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_snn_backprop.c - SNN Backpropagation Training Implementation
//=============================================================================
/**
 * @file nimcp_snn_backprop.c
 * @brief Implementation of gradient-based SNN training algorithms
 *
 * IMPLEMENTATION NOTES:
 * - Uses surrogate gradients to approximate spike derivatives
 * - Supports multiple training algorithms (BPTT, E-prop, RTRL, etc.)
 * - Integrates with NIMCP gradient manager and loss functions
 * - Memory-efficient temporal unrolling with optional truncation
 *
 * MEMORY LAYOUT:
 * - Activation buffers: [timesteps][batch_size][n_neurons]
 * - Gradient buffers: [n_synapses] accumulated over time
 * - Eligibility traces: [n_synapses] for E-prop/RTRL
 *
 * @author NIMCP Development Team
 * @date 2025-12-20
 * @version 1.0.0
 */

#include "training/nimcp_snn_backprop.h"
#include "training/nimcp_unified_training.h"
#include "snn/nimcp_snn_network.h"  /* snn_network_update_stats() */
#include "core/neuralnet/nimcp_neuralnet.h"
#include "constants/nimcp_constants.h"
#include "utils/validation/nimcp_common.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>
#include <float.h>
#include <stdint.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(snn_backprop)

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Activation recording buffer
 *
 * WHAT: Store network activations during forward pass
 * WHY:  Required for backpropagation through time
 * HOW:  Ring buffer of membrane potentials, spikes, currents
 */
typedef struct {
    nimcp_tensor_t* membrane_v;      /**< Membrane potentials [T][B][N] */
    nimcp_tensor_t* spikes;          /**< Spike outputs [T][B][N] */
    nimcp_tensor_t* currents;        /**< Synaptic currents [T][B][N] */
    nimcp_tensor_t* thresholds;      /**< Adaptive thresholds [T][B][N] */
    uint32_t timesteps;              /**< Number of timesteps recorded */
    uint32_t batch_size;             /**< Batch size */
    uint32_t n_neurons;              /**< Number of neurons */
} activation_buffer_t;

/**
 * @brief Gradient accumulation buffer
 *
 * WHAT: Accumulate weight gradients during backward pass
 * WHY:  Gradients from multiple timesteps/samples must be summed
 * HOW:  Tensor of gradients per synapse
 */
typedef struct {
    nimcp_tensor_t* weight_grads;    /**< Weight gradients [n_synapses] */
    nimcp_tensor_t* threshold_grads; /**< Threshold gradients [n_neurons] */
    nimcp_tensor_t* tau_grads;       /**< Time constant gradients [n_neurons] */
    uint32_t accumulation_count;     /**< Number of gradients accumulated */
} gradient_buffer_t;

/**
 * @brief Eligibility trace buffer (for E-prop/RTRL)
 *
 * WHAT: Local eligibility traces for online learning
 * WHY:  Approximate gradient computation without full BPTT
 * HOW:  Exponentially decaying traces per synapse
 */
typedef struct {
    nimcp_tensor_t* eligibility;     /**< Eligibility traces [n_synapses] */
    float tau;                       /**< Decay time constant (ms) */
    float dt;                        /**< Simulation timestep (ms) */
} eligibility_buffer_t;

/**
 * @brief SNN backprop trainer context
 */
struct snn_backprop_ctx_s {
    /* Configuration */
    snn_backprop_config_t config;

    /* Network reference */
    snn_network_t* network;          /**< SNN network being trained */

    /* Activation recording */
    activation_buffer_t* activations; /**< Forward pass activations */

    /* Gradient buffers */
    gradient_buffer_t* gradients;    /**< Accumulated gradients */
    eligibility_buffer_t* eligibility; /**< Eligibility traces (E-prop/RTRL) */

    /* Integration */
    nimcp_gradient_manager_ctx_t* grad_manager; /**< Gradient manager */
    nimcp_loss_context_t* loss_ctx;  /**< Loss function context */

    /* Statistics */
    snn_backprop_stats_t stats;

    /* Memory pool */
    void* memory_pool;               /**< Memory pool for buffers */

    /* Thread safety */
    void* mutex;                     /**< nimcp_mutex_t* */

    /* Anti-collapse (unified) */
    nimcp_anti_collapse_state_t anti_collapse;

    /* UTM management flag — when true, UTM owns gradient norm + diversity */
    bool managed_by_utm;

    /* Input gradient for cross-network gradient bridges */
    float* last_input_grad;          /**< dL/d_input from last backward pass */
    uint32_t input_grad_size;        /**< Size of last_input_grad array */

    /* BPTT neuron/synapse mapping (built lazily on first forward) */
    uint32_t total_neurons_flat;     /**< Total neurons across all populations */
    uint32_t total_synapses_actual;  /**< Actual incoming synapse count */
    uint32_t* neuron_id_to_flat;     /**< neuron_id -> flat index [0..N-1] */
    uint32_t* flat_to_neuron_id;     /**< flat index -> neuron_id (reverse) */
    uint32_t max_neuron_id;          /**< For array sizing */
    uint32_t* synapse_offset;        /**< flat neuron i -> start index in weight_grads */
    uint32_t* synapse_count;         /**< Number of incoming synapses per flat neuron */
    bool mapping_built;              /**< Whether synapse mapping has been built */

    /* Forward pass state for backward */
    float* last_outputs;             /**< Outputs from last forward [n_outputs] */
    uint32_t last_n_outputs;
    uint32_t last_timesteps;         /**< Timesteps used in last forward */
};

/**
 * @brief Training batch structure
 */
struct snn_batch_s {
    float* inputs;                   /**< Input data [batch_size × n_inputs] */
    float* targets;                  /**< Target data [batch_size × n_outputs] */
    uint32_t batch_size;             /**< Number of samples */
    uint32_t n_inputs;               /**< Input dimension */
    uint32_t n_outputs;              /**< Output dimension */
};

//=============================================================================
// Default Configuration Functions
//=============================================================================

snn_surrogate_config_t snn_surrogate_default_config(void) {
    snn_surrogate_config_t config = {
        .method = SNN_SURROGATE_SUPERSPIKE,
        .beta = SNN_SURROGATE_BETA_DEFAULT,
        .width = 1.0f,
        .adaptive_beta = false,
        .beta_min = 0.1f,
        .beta_max = 10.0f
    };
    return config;
}

snn_bptt_config_t snn_bptt_default_config(uint32_t sequence_length) {
    // Guard: Use 50-step default if sequence too short
    if (sequence_length == 0) {
        sequence_length = 50;
    }

    snn_bptt_config_t config = {
        .unroll_steps = sequence_length,
        .truncate = true,
        .truncation_length = (sequence_length > 100) ? 100 : sequence_length,
        .detach_spike_grad = false,
        .accumulate_over_time = true
    };
    return config;
}

snn_eprop_config_t snn_eprop_default_config(void) {
    snn_eprop_config_t config = {
        .eligibility_tau = SNN_ELIGIBILITY_TAU_DEFAULT,
        .learning_signal_delay = 0.0f,
        .use_symmetric_eprop = true,
        .adaptive_learning_signal = false,
        .kappa = 0.1f
    };
    return config;
}

snn_loss_config_t snn_loss_default_config(snn_loss_type_t loss_type) {
    snn_loss_config_t config = {
        .type = loss_type,
        .reduction = NIMCP_LOSS_REDUCE_MEAN,
        .target_rate = 10.0f,
        .rate_regularization = 0.001f,
        .timing_precision = 1.0f,
        .earliest_spike_time = 0.0f,
        .latest_spike_time = 100.0f,
        .tau_vr = 10.0f,
        .cost_vp = 1.0f,
        .custom_forward = NULL,
        .custom_backward = NULL,
        .custom_user_data = NULL
    };
    return config;
}

snn_backprop_config_t snn_backprop_default_config(snn_train_algorithm_t algorithm) {
    snn_backprop_config_t config;
    memset(&config, 0, sizeof(config));

    config.algorithm = algorithm;
    config.temporal_mode = SNN_TEMPORAL_BATCH;
    config.surrogate = snn_surrogate_default_config();

    // Algorithm-specific defaults
    config.bptt = snn_bptt_default_config(50);
    config.eprop = snn_eprop_default_config();
    config.rtrl.sparse_jacobian = true;
    config.rtrl.sparsity_threshold = 0.01f;
    config.rtrl.max_jacobian_rank = 100;

    config.loss = snn_loss_default_config(SNN_LOSS_RATE_CODED_MSE);

    config.learning_rate = NIMCP_LEARNING_RATE_FINE;
    config.weight_decay = NIMCP_WEIGHT_DECAY_DEFAULT;
    config.use_gradient_clipping = true;
    config.gradient_clip_norm = SNN_GRADIENT_CLIP_DEFAULT;

    config.batch_size = NIMCP_DEFAULT_BATCH_SIZE;
    config.sequence_length = 100;
    config.shuffle_batches = true;

    config.spike_regularization = 0.0f;
    config.membrane_regularization = 0.0f;
    config.use_homeostatic = true;
    config.target_population_rate = 10.0f;

    config.use_gradient_manager = true;
    config.grad_manager_config = nimcp_gradient_manager_default_config();

    config.preallocate_buffers = true;
    config.max_memory_bytes = 1024 * 1024 * 1024; // 1GB

    config.track_gradient_stats = true;
    config.verbose = false;

    config.diversity_loss_weight = 0.1f;
    config.use_gradient_normalization = true;

    return config;
}

//=============================================================================
// Surrogate Gradient Implementations
//=============================================================================

/**
 * @brief Compute SuperSpike surrogate gradient
 *
 * WHAT: σ'(x) = 1 / (β|x| + 1)²
 * WHY:  Smooth, bounded approximation of Dirac delta
 * HOW:  Reciprocal of quadratic in distance from threshold
 *
 * REFERENCE: Zenke & Ganguli 2018
 */
static inline float surrogate_superspike(float x, float beta) {
    float abs_x = fabsf(x);
    float denom = beta * abs_x + 1.0f;
    return 1.0f / (denom * denom);
}

/**
 * @brief Compute fast sigmoid surrogate gradient
 *
 * WHAT: σ'(x) = x / (1 + |x|)²
 * WHY:  Faster computation than SuperSpike
 * HOW:  Variant with x in numerator
 */
static inline float surrogate_fast_sigmoid(float x, float beta) {
    (void)beta; // Unused for fast sigmoid
    float abs_x = fabsf(x);
    float denom = 1.0f + abs_x;
    return x / (denom * denom);
}

/**
 * @brief Compute sigmoid surrogate gradient
 *
 * WHAT: σ'(x) = β·σ(βx)·(1 - σ(βx))
 * WHY:  Standard sigmoid derivative
 * HOW:  Logistic function derivative
 */
static inline float surrogate_sigmoid(float x, float beta) {
    float exp_term = expf(-beta * x);
    float sigmoid = 1.0f / (1.0f + exp_term);
    return beta * sigmoid * (1.0f - sigmoid);
}

/**
 * @brief Compute arctan surrogate gradient
 *
 * WHAT: σ'(x) = 1 / (1 + (πx)²)
 * WHY:  Smooth bell curve
 * HOW:  Derivative of arctan
 */
static inline float surrogate_arctan(float x, float beta) {
    (void)beta;
    float pi_x = M_PI * x;
    return 1.0f / (1.0f + pi_x * pi_x);
}

/**
 * @brief Compute triangular surrogate gradient
 *
 * WHAT: σ'(x) = max(0, 1 - |x|/a)
 * WHY:  Piecewise linear, simple
 * HOW:  Triangle function
 */
static inline float surrogate_triangular(float x, float width) {
    float abs_x = fabsf(x);
    if (abs_x >= width) {
        return 0.0f;
    }
    return 1.0f - abs_x / width;
}

/**
 * @brief Compute rectangular surrogate gradient (STE)
 *
 * WHAT: σ'(x) = 1 if |x| < a else 0
 * WHY:  Straight-through estimator
 * HOW:  Box function
 */
static inline float surrogate_rectangular(float x, float width) {
    return (fabsf(x) < width) ? 1.0f : 0.0f;
}

/**
 * @brief Compute exponential surrogate gradient
 *
 * WHAT: σ'(x) = β·exp(-β|x|)
 * WHY:  Exponential decay from threshold
 * HOW:  Double-sided exponential
 */
static inline float surrogate_exponential(float x, float beta) {
    return beta * expf(-beta * fabsf(x));
}

//=============================================================================
// Public Surrogate Gradient Functions
//=============================================================================

float snn_surrogate_gradient(
    const snn_backprop_ctx_t* ctx,
    float membrane_v
) {
    // Guard: Null check
    if (!ctx) {
        return 0.0f;
    }

    const snn_surrogate_config_t* cfg = &ctx->config.surrogate;

    switch (cfg->method) {
        case SNN_SURROGATE_SUPERSPIKE:
            return surrogate_superspike(membrane_v, cfg->beta);

        case SNN_SURROGATE_FAST_SIGMOID:
            return surrogate_fast_sigmoid(membrane_v, cfg->beta);

        case SNN_SURROGATE_SIGMOID:
            return surrogate_sigmoid(membrane_v, cfg->beta);

        case SNN_SURROGATE_ARCTAN:
            return surrogate_arctan(membrane_v, cfg->beta);

        case SNN_SURROGATE_TRIANGULAR:
            return surrogate_triangular(membrane_v, cfg->width);

        case SNN_SURROGATE_RECTANGULAR:
            return surrogate_rectangular(membrane_v, cfg->width);

        case SNN_SURROGATE_EXPONENTIAL:
            return surrogate_exponential(membrane_v, cfg->beta);

        default:
            NIMCP_LOGGING_ERROR("Unknown surrogate method");
            return 0.0f;
    }
}

nimcp_tensor_t* snn_surrogate_gradient_tensor(
    const snn_backprop_ctx_t* ctx,
    const nimcp_tensor_t* membrane_v
) {
    // Guard: Null checks
    if (!ctx || !membrane_v) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_surrogate_gradient_tensor: required parameter is NULL (ctx, membrane_v)");
        return NULL;
    }

    // Create output tensor with same shape
    const nimcp_tensor_shape_t* shape = nimcp_tensor_shape(membrane_v);
    if (!shape) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "shape is NULL");

        return NULL;
    }

    nimcp_tensor_t* grad = nimcp_tensor_create(
        shape->dims,
        shape->rank,
        NIMCP_DTYPE_F32
    );

    if (!grad) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "grad is NULL");


        return NULL;
    }

    // Apply surrogate gradient element-wise
    size_t numel = nimcp_tensor_numel(membrane_v);
    const float* v_data = (const float*)nimcp_tensor_data_const(membrane_v);
    float* grad_data = (float*)nimcp_tensor_data(grad);

    for (size_t i = 0; i < numel; i++) {
        grad_data[i] = snn_surrogate_gradient(ctx, v_data[i]);
    }

    return grad;
}

int snn_backprop_set_surrogate(
    snn_backprop_ctx_t* ctx,
    snn_surrogate_method_t method
) {
    // Guard: Null check
    if (!ctx) {
        return SNN_ERROR_NULL_POINTER;
    }

    // Guard: Validate method
    if (method >= SNN_SURROGATE_COUNT) {
        return SNN_ERROR_INVALID_STATE;
    }

    ctx->config.surrogate.method = method;
    return SNN_SUCCESS;
}

//=============================================================================
// Lifecycle Functions (Stubs)
//=============================================================================

/**
 * @brief Helper: Allocate activation buffer for recording forward pass state
 */
static activation_buffer_t* alloc_activation_buffer(
    uint32_t timesteps, uint32_t batch_size, uint32_t n_neurons)
{
    activation_buffer_t* buf = nimcp_calloc(1, sizeof(activation_buffer_t));
    if (!buf) return NULL;

    buf->timesteps = timesteps;
    buf->batch_size = batch_size;
    buf->n_neurons = n_neurons;

    uint64_t total = (uint64_t)timesteps * batch_size * n_neurons;
    if (total == 0) total = 1;
    if (total > UINT32_MAX) {
        nimcp_free(buf);
        return NULL;
    }

    uint32_t dims[3] = { timesteps, batch_size, n_neurons };

    buf->membrane_v = nimcp_tensor_create(dims, 3, NIMCP_DTYPE_F32);
    buf->spikes = nimcp_tensor_create(dims, 3, NIMCP_DTYPE_F32);
    buf->currents = nimcp_tensor_create(dims, 3, NIMCP_DTYPE_F32);
    buf->thresholds = nimcp_tensor_create(dims, 3, NIMCP_DTYPE_F32);

    if (!buf->membrane_v || !buf->spikes || !buf->currents || !buf->thresholds) {
        if (buf->membrane_v) nimcp_tensor_destroy(buf->membrane_v);
        if (buf->spikes) nimcp_tensor_destroy(buf->spikes);
        if (buf->currents) nimcp_tensor_destroy(buf->currents);
        if (buf->thresholds) nimcp_tensor_destroy(buf->thresholds);
        nimcp_free(buf);
        return NULL;
    }

    return buf;
}

/**
 * @brief Helper: Free activation buffer
 */
static void free_activation_buffer(activation_buffer_t* buf) {
    if (!buf) return;
    if (buf->membrane_v) nimcp_tensor_destroy(buf->membrane_v);
    if (buf->spikes) nimcp_tensor_destroy(buf->spikes);
    if (buf->currents) nimcp_tensor_destroy(buf->currents);
    if (buf->thresholds) nimcp_tensor_destroy(buf->thresholds);
    nimcp_free(buf);
}

/**
 * @brief Helper: Allocate gradient buffer
 */
static gradient_buffer_t* alloc_gradient_buffer(uint32_t n_synapses, uint32_t n_neurons) {
    gradient_buffer_t* buf = nimcp_calloc(1, sizeof(gradient_buffer_t));
    if (!buf) return NULL;

    buf->accumulation_count = 0;

    uint32_t syn_dims[1] = { n_synapses > 0 ? n_synapses : 1 };
    uint32_t nrn_dims[1] = { n_neurons > 0 ? n_neurons : 1 };

    buf->weight_grads = nimcp_tensor_create(syn_dims, 1, NIMCP_DTYPE_F32);
    buf->threshold_grads = nimcp_tensor_create(nrn_dims, 1, NIMCP_DTYPE_F32);
    buf->tau_grads = nimcp_tensor_create(nrn_dims, 1, NIMCP_DTYPE_F32);

    if (!buf->weight_grads || !buf->threshold_grads || !buf->tau_grads) {
        if (buf->weight_grads) nimcp_tensor_destroy(buf->weight_grads);
        if (buf->threshold_grads) nimcp_tensor_destroy(buf->threshold_grads);
        if (buf->tau_grads) nimcp_tensor_destroy(buf->tau_grads);
        nimcp_free(buf);
        return NULL;
    }

    return buf;
}

/**
 * @brief Helper: Free gradient buffer
 */
static void free_gradient_buffer(gradient_buffer_t* buf) {
    if (!buf) return;
    if (buf->weight_grads) nimcp_tensor_destroy(buf->weight_grads);
    if (buf->threshold_grads) nimcp_tensor_destroy(buf->threshold_grads);
    if (buf->tau_grads) nimcp_tensor_destroy(buf->tau_grads);
    nimcp_free(buf);
}

/**
 * @brief Helper: Allocate eligibility trace buffer (for E-prop/RTRL)
 */
static eligibility_buffer_t* alloc_eligibility_buffer(
    uint32_t n_synapses, const snn_eprop_config_t* eprop_cfg)
{
    eligibility_buffer_t* buf = nimcp_calloc(1, sizeof(eligibility_buffer_t));
    if (!buf) return NULL;

    buf->tau = eprop_cfg ? eprop_cfg->eligibility_tau : SNN_ELIGIBILITY_TAU_DEFAULT;
    buf->dt = 1.0f;  /* Default 1ms timestep */

    uint32_t dims[1] = { n_synapses > 0 ? n_synapses : 1 };
    buf->eligibility = nimcp_tensor_create(dims, 1, NIMCP_DTYPE_F32);
    if (!buf->eligibility) {
        nimcp_free(buf);
        return NULL;
    }

    return buf;
}

/**
 * @brief Helper: Free eligibility trace buffer
 */
static void free_eligibility_buffer(eligibility_buffer_t* buf) {
    if (!buf) return;
    if (buf->eligibility) nimcp_tensor_destroy(buf->eligibility);
    nimcp_free(buf);
}

snn_backprop_ctx_t* snn_backprop_create(
    snn_network_t* network,
    const snn_backprop_config_t* config
) {
    // Guard: Null checks
    if (!network || !config) {
        NIMCP_LOGGING_ERROR("Null network or config");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_backprop_create: required parameter is NULL (network, config)");
        return NULL;
    }

    // Guard: Validate config
    if (snn_backprop_validate_config(config) != SNN_SUCCESS) {
        NIMCP_LOGGING_ERROR("Invalid configuration");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "snn_backprop_create: validation failed");
        return NULL;
    }

    /* Allocate context */
    snn_backprop_ctx_t* ctx = nimcp_calloc(1, sizeof(snn_backprop_ctx_t));
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "snn_backprop_create: failed to allocate context");
        return NULL;
    }

    /* Copy configuration */
    memcpy(&ctx->config, config, sizeof(snn_backprop_config_t));

    /* Store network reference */
    ctx->network = network;

    /* Create mutex for thread safety */
    mutex_attr_t mutex_attr = { .type = MUTEX_TYPE_NORMAL };
    ctx->mutex = nimcp_mutex_create(&mutex_attr);
    if (!ctx->mutex) {
        NIMCP_LOGGING_ERROR("snn_backprop_create: failed to create mutex");
        nimcp_free(ctx);
        return NULL;
    }

    /* Determine network dimensions from populations */
    uint32_t total_neurons = 0;
    uint32_t total_synapses = 0;
    for (uint32_t p = 0; p < network->n_populations; p++) {
        if (network->populations[p]) {
            total_neurons += network->populations[p]->n_neurons;
        }
    }
    /* Estimate synapses from neuron count (connectivity varies) */
    total_synapses = total_neurons * 10;  /* rough estimate */
    if (total_neurons == 0) total_neurons = 1;

    /* Allocate activation buffers if needed for BPTT */
    if (config->preallocate_buffers) {
        uint32_t timesteps = config->bptt.unroll_steps;
        if (timesteps == 0) timesteps = config->sequence_length;
        if (timesteps == 0) timesteps = 50;

        /* Cap timesteps to avoid excessive memory */
        if (timesteps > 1000) timesteps = 1000;

        uint32_t batch_size = config->batch_size;
        if (batch_size == 0) batch_size = 1;

        ctx->activations = alloc_activation_buffer(timesteps, batch_size, total_neurons);
        if (!ctx->activations) {
            NIMCP_LOGGING_WARN("snn_backprop_create: failed to allocate activation buffers, will allocate on demand");
        }
    }

    /* Allocate gradient buffers */
    ctx->gradients = alloc_gradient_buffer(total_synapses, total_neurons);
    if (!ctx->gradients) {
        NIMCP_LOGGING_WARN("snn_backprop_create: failed to allocate gradient buffers");
    }

    /* Allocate eligibility traces for E-prop/RTRL algorithms */
    if (config->algorithm == SNN_TRAIN_EPROP ||
        config->algorithm == SNN_TRAIN_RTRL) {
        ctx->eligibility = alloc_eligibility_buffer(total_synapses, &config->eprop);
        if (!ctx->eligibility) {
            NIMCP_LOGGING_WARN("snn_backprop_create: failed to allocate eligibility traces");
        }
    }

    /* Create gradient manager if configured */
    if (config->use_gradient_manager) {
        nimcp_gradient_manager_config_t gm_config = config->grad_manager_config;
        ctx->grad_manager = nimcp_gradient_manager_create(&gm_config);
        if (!ctx->grad_manager) {
            NIMCP_LOGGING_WARN("snn_backprop_create: failed to create gradient manager");
        }
    }

    /* Initialize statistics */
    memset(&ctx->stats, 0, sizeof(snn_backprop_stats_t));

    /* Anti-collapse init */
    {
        nimcp_anti_collapse_config_t ac_cfg = {
            .diversity_loss_weight = config->diversity_loss_weight,
            .diversity_buffer_size = 16,
            .use_gradient_normalization = config->use_gradient_normalization,
            .gradient_target_norm = 1.0f,
            .gradient_clip_value = config->gradient_clip_norm,
            .adaptive_gradient_target = true,
        };
        nimcp_anti_collapse_init(&ctx->anti_collapse, &ac_cfg);
    }

    NIMCP_LOGGING_INFO("SNN backprop trainer created: %u neurons, %u synapses, algorithm=%d",
                       total_neurons, total_synapses, (int)config->algorithm);

    return ctx;
}

void snn_backprop_destroy(snn_backprop_ctx_t* ctx) {
    // Guard: Null check
    if (!ctx) {
        return;
    }

    /* Free activation buffers */
    if (ctx->activations) {
        free_activation_buffer(ctx->activations);
        ctx->activations = NULL;
    }

    /* Free gradient buffers */
    if (ctx->gradients) {
        free_gradient_buffer(ctx->gradients);
        ctx->gradients = NULL;
    }

    /* Free eligibility traces */
    if (ctx->eligibility) {
        free_eligibility_buffer(ctx->eligibility);
        ctx->eligibility = NULL;
    }

    /* Destroy gradient manager */
    if (ctx->grad_manager) {
        nimcp_gradient_manager_destroy(ctx->grad_manager);
        ctx->grad_manager = NULL;
    }

    /* Destroy loss context */
    if (ctx->loss_ctx) {
        nimcp_loss_destroy(ctx->loss_ctx);
        ctx->loss_ctx = NULL;
    }

    /* Free input gradient buffer */
    if (ctx->last_input_grad) {
        nimcp_free(ctx->last_input_grad);
        ctx->last_input_grad = NULL;
        ctx->input_grad_size = 0;
    }

    /* Free BPTT mapping buffers */
    nimcp_free(ctx->neuron_id_to_flat);
    nimcp_free(ctx->flat_to_neuron_id);
    nimcp_free(ctx->synapse_offset);
    nimcp_free(ctx->synapse_count);
    ctx->neuron_id_to_flat = NULL;
    ctx->flat_to_neuron_id = NULL;
    ctx->synapse_offset = NULL;
    ctx->synapse_count = NULL;

    /* Free last_outputs buffer */
    nimcp_free(ctx->last_outputs);
    ctx->last_outputs = NULL;

    /* Destroy anti-collapse state */
    nimcp_anti_collapse_destroy(&ctx->anti_collapse);

    /* Destroy mutex */
    if (ctx->mutex) {
        nimcp_mutex_free(ctx->mutex);
        ctx->mutex = NULL;
    }

    /* Free memory pool if allocated */
    if (ctx->memory_pool) {
        nimcp_free(ctx->memory_pool);
        ctx->memory_pool = NULL;
    }

    nimcp_free(ctx);
}

int snn_backprop_reset(snn_backprop_ctx_t* ctx) {
    // Guard: Null check
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "snn_backprop_reset: ctx is NULL");
        return SNN_ERROR_NULL_POINTER;
    }

    /* Zero out gradient buffers */
    if (ctx->gradients) {
        if (ctx->gradients->weight_grads) {
            float* data = nimcp_tensor_data(ctx->gradients->weight_grads);
            if (data) {
                memset(data, 0, nimcp_tensor_numel(ctx->gradients->weight_grads) * sizeof(float));
            }
        }
        if (ctx->gradients->threshold_grads) {
            float* data = nimcp_tensor_data(ctx->gradients->threshold_grads);
            if (data) {
                memset(data, 0, nimcp_tensor_numel(ctx->gradients->threshold_grads) * sizeof(float));
            }
        }
        if (ctx->gradients->tau_grads) {
            float* data = nimcp_tensor_data(ctx->gradients->tau_grads);
            if (data) {
                memset(data, 0, nimcp_tensor_numel(ctx->gradients->tau_grads) * sizeof(float));
            }
        }
        ctx->gradients->accumulation_count = 0;
    }

    /* Zero out eligibility traces */
    if (ctx->eligibility && ctx->eligibility->eligibility) {
        float* data = nimcp_tensor_data(ctx->eligibility->eligibility);
        if (data) {
            memset(data, 0, nimcp_tensor_numel(ctx->eligibility->eligibility) * sizeof(float));
        }
    }

    /* Reset statistics */
    memset(&ctx->stats, 0, sizeof(snn_backprop_stats_t));

    NIMCP_LOGGING_INFO("SNN backprop trainer reset");

    return SNN_SUCCESS;
}

//=============================================================================
// BPTT Synapse Mapping Helper
//=============================================================================

/**
 * @brief Build flat neuron index and synapse offset tables for BPTT
 *
 * WHAT: Map population neuron IDs to dense [0..N-1] indices for efficient BPTT
 * WHY:  Activation/gradient buffers need contiguous indexing across populations
 * HOW:  Iterate populations, assign flat indices, count incoming synapses
 */
static int build_synapse_mapping(snn_backprop_ctx_t* ctx) {
    if (!ctx || !ctx->network) return -1;
    if (ctx->mapping_built) return 0;

    snn_network_t* net = ctx->network;
    neural_network_t nn = net->neural_net;

    /* Pass 1: count total neurons and find max neuron ID */
    uint32_t total_flat = 0;
    uint32_t max_nid = 0;
    for (uint32_t p = 0; p < net->n_populations; p++) {
        snn_population_t* pop = net->populations[p];
        if (!pop) continue;
        total_flat += pop->n_neurons;
        for (uint32_t i = 0; i < pop->n_neurons; i++) {
            if (pop->neuron_ids[i] > max_nid)
                max_nid = pop->neuron_ids[i];
        }
    }

    if (total_flat == 0) return -1;
    ctx->total_neurons_flat = total_flat;
    ctx->max_neuron_id = max_nid;

    /* Allocate mapping arrays */
    ctx->neuron_id_to_flat = (uint32_t*)nimcp_calloc(max_nid + 1, sizeof(uint32_t));
    ctx->flat_to_neuron_id = (uint32_t*)nimcp_calloc(total_flat, sizeof(uint32_t));
    ctx->synapse_count = (uint32_t*)nimcp_calloc(total_flat, sizeof(uint32_t));
    ctx->synapse_offset = (uint32_t*)nimcp_calloc(total_flat + 1, sizeof(uint32_t));

    if (!ctx->neuron_id_to_flat || !ctx->flat_to_neuron_id ||
        !ctx->synapse_count || !ctx->synapse_offset) {
        NIMCP_LOGGING_ERROR("build_synapse_mapping: allocation failed");
        return -1;
    }

    /* Init neuron_id_to_flat to UINT32_MAX ("not in SNN") */
    for (uint32_t i = 0; i <= max_nid; i++) {
        ctx->neuron_id_to_flat[i] = UINT32_MAX;
    }

    /* Pass 2: assign flat indices */
    uint32_t flat_idx = 0;
    for (uint32_t p = 0; p < net->n_populations; p++) {
        snn_population_t* pop = net->populations[p];
        if (!pop) continue;
        for (uint32_t i = 0; i < pop->n_neurons; i++) {
            uint32_t nid = pop->neuron_ids[i];
            ctx->neuron_id_to_flat[nid] = flat_idx;
            ctx->flat_to_neuron_id[flat_idx] = nid;
            flat_idx++;
        }
    }

    /* Pass 3: count incoming synapses per flat neuron */
    uint32_t total_syns = 0;
    for (uint32_t f = 0; f < total_flat; f++) {
        uint32_t nid = ctx->flat_to_neuron_id[f];
        neuron_t* neuron = neural_network_get_neuron(nn, nid);
        if (!neuron) continue;
        uint32_t count = neuron->incoming.embedded_count + neuron->incoming.overflow_count;
        ctx->synapse_count[f] = count;
        total_syns += count;
    }

    /* Pass 4: prefix-sum for synapse_offset */
    ctx->synapse_offset[0] = 0;
    for (uint32_t f = 0; f < total_flat; f++) {
        ctx->synapse_offset[f + 1] = ctx->synapse_offset[f] + ctx->synapse_count[f];
    }
    ctx->total_synapses_actual = total_syns;

    /* Reallocate gradient buffer if estimated size was too small */
    if (ctx->gradients && ctx->gradients->weight_grads) {
        size_t current_numel = nimcp_tensor_numel(ctx->gradients->weight_grads);
        if (current_numel < total_syns) {
            free_gradient_buffer(ctx->gradients);
            ctx->gradients = alloc_gradient_buffer(total_syns, total_flat);
            if (!ctx->gradients) {
                NIMCP_LOGGING_ERROR("build_synapse_mapping: gradient realloc failed");
                return -1;
            }
        }
    }

    ctx->mapping_built = true;
    NIMCP_LOGGING_INFO("SNN BPTT mapping: %u flat neurons, %u synapses",
                       total_flat, total_syns);
    return 0;
}

//=============================================================================
// Forward/Backward Pass Functions
//=============================================================================

int snn_backprop_forward(
    snn_backprop_ctx_t* ctx,
    const float* inputs,
    uint32_t batch_size,
    float duration_ms,
    float* outputs
) {
    /* Guard: Null checks */
    if (!ctx || !inputs) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "snn_backprop_forward: ctx or inputs is NULL");
        return SNN_ERROR_NULL_POINTER;
    }
    if (batch_size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "snn_backprop_forward: batch_size is 0");
        return SNN_ERROR_INVALID_STATE;
    }
    if (!ctx->network) return SNN_ERROR_INVALID_STATE;

    /* Lazy-build synapse mapping on first forward */
    if (!ctx->mapping_built) {
        if (build_synapse_mapping(ctx) != 0) {
            NIMCP_LOGGING_ERROR("snn_backprop_forward: failed to build synapse mapping");
            return SNN_ERROR_INVALID_STATE;
        }
    }

    snn_network_t* net = ctx->network;
    neural_network_t nn = net->neural_net;

    /* Compute timestep count */
    float dt = net->config.dt;
    if (dt <= 0.0f) dt = 1.0f;
    uint32_t unroll = ctx->config.bptt.unroll_steps;
    if (unroll == 0) unroll = 50;

    uint32_t n_steps;
    if (duration_ms > 0.0f) {
        n_steps = (uint32_t)(duration_ms / dt);
        if (n_steps > unroll) n_steps = unroll;
    } else {
        n_steps = unroll;
    }
    if (n_steps > SNN_BPTT_MAX_UNROLL) n_steps = SNN_BPTT_MAX_UNROLL;
    if (n_steps == 0) n_steps = 1;

    uint32_t N = ctx->total_neurons_flat;

    /* Allocate/reallocate activation buffer if needed */
    if (!ctx->activations || ctx->activations->timesteps < n_steps ||
        ctx->activations->n_neurons < N) {
        if (ctx->activations) free_activation_buffer(ctx->activations);
        ctx->activations = alloc_activation_buffer(n_steps, 1, N);
        if (!ctx->activations) {
            NIMCP_LOGGING_ERROR("snn_backprop_forward: activation buffer alloc failed");
            return SNN_ERROR_INVALID_STATE;
        }
    }

    float* act_v = (float*)nimcp_tensor_data(ctx->activations->membrane_v);
    float* act_s = (float*)nimcp_tensor_data(ctx->activations->spikes);
    if (!act_v || !act_s) return SNN_ERROR_INVALID_STATE;

    /* Zero activation buffers */
    memset(act_v, 0, (size_t)n_steps * N * sizeof(float));
    memset(act_s, 0, (size_t)n_steps * N * sizeof(float));

    /* Reset network state */
    snn_network_reset(net);

    /* Set inputs — use network's actual n_inputs, not batch_size.
     * Input layout is [batch_size × n_inputs]; we simulate sample 0. */
    uint32_t n_inputs = net->config.n_inputs;
    if (n_inputs > 0) {
        snn_network_set_inputs(net, inputs, n_inputs);
    }

    /* Simulate n_steps, recording activations at each step.
     * Track cumulative spikes for get_snn_stats visibility into training. */
    int fwd_total_spikes = 0;
    for (uint32_t t = 0; t < n_steps; t++) {
        int step_spikes = snn_network_step(net, dt);
        if (step_spikes >= 0) fwd_total_spikes += step_spikes;

        /* Snapshot membrane_v and spike_output from each population */
        float* v_row = act_v + (size_t)t * N;
        float* s_row = act_s + (size_t)t * N;

        for (uint32_t p = 0; p < net->n_populations; p++) {
            snn_population_t* pop = net->populations[p];
            if (!pop) continue;

            const float* pop_v = pop->membrane_v ?
                (const float*)nimcp_tensor_data_const(pop->membrane_v) : NULL;
            const float* pop_s = pop->spike_output ?
                (const float*)nimcp_tensor_data_const(pop->spike_output) : NULL;

            for (uint32_t i = 0; i < pop->n_neurons; i++) {
                uint32_t nid = pop->neuron_ids[i];
                if (nid > ctx->max_neuron_id) continue;
                uint32_t flat = ctx->neuron_id_to_flat[nid];
                if (flat == UINT32_MAX || flat >= N) continue;

                if (pop_v) v_row[flat] = pop_v[i];
                if (pop_s) s_row[flat] = pop_s[i];
            }
        }
    }

    /* Get decoded outputs */
    uint32_t n_out = net->output_pop ? net->output_pop->n_neurons : 0;
    if (n_out == 0) n_out = batch_size;

    /* (Re)allocate last_outputs */
    if (ctx->last_n_outputs < n_out) {
        nimcp_free(ctx->last_outputs);
        ctx->last_outputs = (float*)nimcp_calloc(n_out, sizeof(float));
        ctx->last_n_outputs = ctx->last_outputs ? n_out : 0;
    }

    if (ctx->last_outputs) {
        memset(ctx->last_outputs, 0, n_out * sizeof(float));
        snn_network_get_outputs(net, ctx->last_outputs, n_out);
    }

    /* Copy to caller's output buffer */
    if (outputs) {
        uint32_t copy_n = (batch_size < n_out) ? batch_size : n_out;
        if (ctx->last_outputs) {
            memcpy(outputs, ctx->last_outputs, copy_n * sizeof(float));
        } else {
            memset(outputs, 0, batch_size * sizeof(float));
        }
    }

    ctx->last_timesteps = n_steps;
    ctx->stats.total_forward_time_ms += (float)n_steps * dt;

    /* Update network->stats so get_snn_stats RPC reflects training activity,
     * not just inference. Previously, BPTT ran its own step loop and never
     * touched network->stats — making training-path spike rates invisible
     * and hiding the fact that BPTT was silently producing no spikes for
     * weeks. Uses the same counter pattern as snn_network_run(): accumulate
     * from snn_network_step()'s return value inside the step loop above. */
    {
        float sim_duration_ms = (float)n_steps * dt;
        snn_network_update_stats(net, fwd_total_spikes, sim_duration_ms);
    }

    return SNN_SUCCESS;
}

int snn_backprop_backward(
    snn_backprop_ctx_t* ctx,
    const float* targets,
    uint32_t batch_size
) {
    /* Guard: Null checks */
    if (!ctx || !targets) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "snn_backprop_backward: ctx or targets is NULL");
        return SNN_ERROR_NULL_POINTER;
    }
    if (!ctx->network) return SNN_ERROR_INVALID_STATE;
    if (!ctx->mapping_built || !ctx->activations) return SNN_ERROR_INVALID_STATE;

    snn_network_t* net = ctx->network;
    neural_network_t nn = net->neural_net;
    snn_population_t* out_pop = net->output_pop;
    snn_population_t* in_pop = net->input_pop;
    uint32_t N = ctx->total_neurons_flat;
    uint32_t T = ctx->last_timesteps;
    if (T == 0 || N == 0) return SNN_ERROR_INVALID_STATE;

    /* Compute output error: dL/d_output = 2/N * (output - target) */
    uint32_t n_out = out_pop ? out_pop->n_neurons : 0;
    uint32_t out_dim = (batch_size < n_out) ? batch_size : n_out;

    float* output_error = (float*)nimcp_calloc(n_out > 0 ? n_out : 1, sizeof(float));
    if (!output_error) return SNN_ERROR_INVALID_STATE;

    /* Compute MSE gradient as output error */
    if (ctx->last_outputs && out_dim > 0) {
        float scale = 2.0f / (float)out_dim;
        for (uint32_t j = 0; j < out_dim; j++) {
            output_error[j] = scale * (ctx->last_outputs[j] - targets[j]);
        }
    }

    /* Compute loss for statistics */
    float total_loss = 0.0f;
    if (ctx->last_outputs) {
        for (uint32_t j = 0; j < out_dim; j++) {
            float diff = ctx->last_outputs[j] - targets[j];
            total_loss += diff * diff;
        }
        if (out_dim > 0) total_loss /= (float)out_dim;
    }

    /* Update loss statistics (total_steps incremented in snn_backprop_step) */
    ctx->stats.total_loss += total_loss;
    if (ctx->stats.total_steps == 0 || total_loss < ctx->stats.min_loss)
        ctx->stats.min_loss = total_loss;
    if (total_loss > ctx->stats.max_loss)
        ctx->stats.max_loss = total_loss;

    /* Get activation data pointers */
    const float* act_v = (const float*)nimcp_tensor_data_const(ctx->activations->membrane_v);
    const float* act_s = (const float*)nimcp_tensor_data_const(ctx->activations->spikes);
    if (!act_v || !act_s) {
        nimcp_free(output_error);
        return SNN_ERROR_INVALID_STATE;
    }

    /* Get gradient data pointers */
    float* weight_grads = ctx->gradients ?
        (float*)nimcp_tensor_data(ctx->gradients->weight_grads) : NULL;
    float* threshold_grads = ctx->gradients ?
        (float*)nimcp_tensor_data(ctx->gradients->threshold_grads) : NULL;
    if (!weight_grads || !threshold_grads) {
        nimcp_free(output_error);
        return SNN_ERROR_INVALID_STATE;
    }

    /* Allocate delta buffers (current and previous timestep) */
    float* delta_curr = (float*)nimcp_calloc(N, sizeof(float));
    float* delta_prev = (float*)nimcp_calloc(N, sizeof(float));
    if (!delta_curr || !delta_prev) {
        nimcp_free(output_error);
        nimcp_free(delta_curr);
        nimcp_free(delta_prev);
        return SNN_ERROR_INVALID_STATE;
    }

    /* Initialize delta_curr from output error at output population neurons */
    if (out_pop) {
        for (uint32_t j = 0; j < out_dim; j++) {
            uint32_t nid = out_pop->neuron_ids[j];
            if (nid <= ctx->max_neuron_id) {
                uint32_t flat = ctx->neuron_id_to_flat[nid];
                if (flat != UINT32_MAX && flat < N) {
                    delta_curr[flat] = output_error[j];
                }
            }
        }
    }

    /* Determine truncation bounds */
    uint32_t t_start = 0;
    if (ctx->config.bptt.truncate && ctx->config.bptt.truncation_length > 0 &&
        ctx->config.bptt.truncation_length < T) {
        t_start = T - ctx->config.bptt.truncation_length;
    }

    /* LIF membrane time constant for temporal recurrence */
    float tau_mem = net->config.tau_mem;
    if (tau_mem <= 0.0f) tau_mem = 20.0f;
    float dt = net->config.dt;
    if (dt <= 0.0f) dt = 1.0f;
    float leak_factor = 1.0f - dt / tau_mem;

    /* BPTT unroll: t = T-1 down to t_start */
    for (uint32_t t_rev = 0; t_rev < T - t_start; t_rev++) {
        uint32_t t = (T - 1) - t_rev;
        const float* v_t = act_v + (size_t)t * N;
        const float* s_t = act_s + (size_t)t * N;

        memset(delta_prev, 0, N * sizeof(float));

        for (uint32_t n = 0; n < N; n++) {
            float d = delta_curr[n];
            if (d == 0.0f) continue;

            /* Surrogate gradient at this neuron's membrane potential */
            uint32_t nid = ctx->flat_to_neuron_id[n];
            neuron_t* neuron = neural_network_get_neuron(nn, nid);
            if (!neuron) continue;

            float v_thresh = neuron->threshold;
            float surr = snn_surrogate_gradient(ctx, v_t[n] - v_thresh);

            /* Temporal recurrence: propagate delta back through LIF leak */
            delta_prev[n] += d * leak_factor * surr;

            /* Weight gradients: for each incoming synapse */
            uint32_t syn_off = ctx->synapse_offset[n];
            uint32_t syn_idx = 0;

            /* Iterate embedded synapses */
            uint32_t n_emb = neuron->incoming.embedded_count;
            for (uint32_t si = 0; si < n_emb && syn_idx < ctx->synapse_count[n]; si++) {
                synapse_handle_t* sh = &neuron->incoming.embedded[si];
                uint32_t src_nid = sh->target_neuron_id;

                float spike_src = 0.0f;
                if (src_nid <= ctx->max_neuron_id) {
                    uint32_t src_flat = ctx->neuron_id_to_flat[src_nid];
                    if (src_flat != UINT32_MAX && src_flat < N) {
                        spike_src = s_t[src_flat];
                        /* Spatial backprop: propagate delta to source neuron */
                        delta_prev[src_flat] += d * surr * sh->weight;
                    }
                }

                /* Accumulate weight gradient */
                weight_grads[syn_off + syn_idx] += d * surr * spike_src;
                syn_idx++;
            }

            /* Iterate overflow synapses */
            uint32_t n_ovf = neuron->incoming.overflow_count;
            for (uint32_t si = 0; si < n_ovf && syn_idx < ctx->synapse_count[n]; si++) {
                synapse_handle_t* sh = &neuron->incoming.overflow[si];
                uint32_t src_nid = sh->target_neuron_id;

                float spike_src = 0.0f;
                if (src_nid <= ctx->max_neuron_id) {
                    uint32_t src_flat = ctx->neuron_id_to_flat[src_nid];
                    if (src_flat != UINT32_MAX && src_flat < N) {
                        spike_src = s_t[src_flat];
                        delta_prev[src_flat] += d * surr * sh->weight;
                    }
                }

                weight_grads[syn_off + syn_idx] += d * surr * spike_src;
                syn_idx++;
            }

            /* Threshold gradient */
            threshold_grads[n] += -d * surr;
        }

        /* Clamp delta_prev to [-1e4, 1e4] to prevent explosion */
        for (uint32_t n = 0; n < N; n++) {
            if (delta_prev[n] > 1e4f) delta_prev[n] = 1e4f;
            if (delta_prev[n] < -1e4f) delta_prev[n] = -1e4f;
        }

        /* Swap delta_curr <-> delta_prev */
        float* tmp = delta_curr;
        delta_curr = delta_prev;
        delta_prev = tmp;
    }

    /* Extract input gradients from delta_curr at input population neurons */
    if (in_pop && in_pop->n_neurons > 0) {
        uint32_t n_in = in_pop->n_neurons;
        if (ctx->input_grad_size < n_in) {
            nimcp_free(ctx->last_input_grad);
            ctx->last_input_grad = (float*)nimcp_calloc(n_in, sizeof(float));
            ctx->input_grad_size = ctx->last_input_grad ? n_in : 0;
        }
        if (ctx->last_input_grad) {
            for (uint32_t i = 0; i < n_in; i++) {
                uint32_t nid = in_pop->neuron_ids[i];
                if (nid <= ctx->max_neuron_id) {
                    uint32_t flat = ctx->neuron_id_to_flat[nid];
                    if (flat != UINT32_MAX && flat < N) {
                        ctx->last_input_grad[i] = delta_curr[flat];
                    }
                }
            }
        }
    }

    /* Increment accumulation count */
    if (ctx->gradients) {
        ctx->gradients->accumulation_count++;
    }

    nimcp_free(output_error);
    nimcp_free(delta_curr);
    nimcp_free(delta_prev);
    return SNN_SUCCESS;
}

int snn_backprop_step(
    snn_backprop_ctx_t* ctx,
    float learning_rate
) {
    // Guard: Null check
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "snn_backprop_step: ctx is NULL");
        return SNN_ERROR_NULL_POINTER;
    }

    // Use config learning rate if not specified
    if (learning_rate == 0.0f) {
        learning_rate = ctx->config.learning_rate;
    }

    /* Apply accumulated gradients to network weights.
     * Uses gradient clipping and weight decay from config. */
    if (!ctx->network) return SNN_ERROR_INVALID_STATE;

    float clip = ctx->config.gradient_clip_norm;
    if (clip <= 0.0f) clip = 1.0f;

    float weight_decay = ctx->config.weight_decay;

    /* Apply gradients via gradient manager if available */
    if (ctx->gradients && ctx->gradients->weight_grads) {
        nimcp_tensor_t* wg = ctx->gradients->weight_grads;
        size_t numel = nimcp_tensor_numel(wg);
        float* g = (float*)nimcp_tensor_data(wg);

        if (g && numel > 0) {
            /* Gradient normalization / clipping (unified anti-collapse)
             * Skip when managed by UTM — UTM normalizes globally. */
            if (!ctx->managed_by_utm) {
                float* ga[1] = { g };
                size_t gs[1] = { numel };
                nimcp_anti_collapse_normalize_gradients(
                    &ctx->anti_collapse, ga, gs, 1);
            }

            /* Apply weight decay */
            if (weight_decay > 0.0f) {
                for (size_t i = 0; i < numel; i++) {
                    g[i] += weight_decay * g[i];
                }
            }
        }

        /* Apply weight gradients to actual synapse weights */
        if (ctx->mapping_built && g) {
            neural_network_t nn = ctx->network->neural_net;
            float* tg = ctx->gradients->threshold_grads ?
                (float*)nimcp_tensor_data(ctx->gradients->threshold_grads) : NULL;

            for (uint32_t f = 0; f < ctx->total_neurons_flat; f++) {
                uint32_t nid = ctx->flat_to_neuron_id[f];
                neuron_t* neuron = neural_network_get_neuron(nn, nid);
                if (!neuron) continue;

                /* Apply weight gradients to incoming synapses */
                uint32_t syn_off = ctx->synapse_offset[f];
                uint32_t syn_idx = 0;

                uint32_t n_emb = neuron->incoming.embedded_count;
                for (uint32_t si = 0; si < n_emb && syn_idx < ctx->synapse_count[f]; si++) {
                    synapse_handle_t* sh = &neuron->incoming.embedded[si];
                    float grad = g[syn_off + syn_idx];
                    if (!isnan(grad) && !isinf(grad)) {
                        sh->weight -= learning_rate * grad;
                        if (sh->weight > 10.0f) sh->weight = 10.0f;
                        if (sh->weight < -10.0f) sh->weight = -10.0f;
                    }
                    syn_idx++;
                }

                uint32_t n_ovf = neuron->incoming.overflow_count;
                for (uint32_t si = 0; si < n_ovf && syn_idx < ctx->synapse_count[f]; si++) {
                    synapse_handle_t* sh = &neuron->incoming.overflow[si];
                    float grad = g[syn_off + syn_idx];
                    if (!isnan(grad) && !isinf(grad)) {
                        sh->weight -= learning_rate * grad;
                        if (sh->weight > 10.0f) sh->weight = 10.0f;
                        if (sh->weight < -10.0f) sh->weight = -10.0f;
                    }
                    syn_idx++;
                }

                /* Apply threshold gradient */
                if (tg && !isnan(tg[f]) && !isinf(tg[f])) {
                    neuron->threshold -= learning_rate * 0.1f * tg[f];
                    if (neuron->threshold < -70.0f) neuron->threshold = -70.0f;
                    if (neuron->threshold > -30.0f) neuron->threshold = -30.0f;
                }
            }
        }

        /* Zero gradients after applying */
        if (g) memset(g, 0, numel * sizeof(float));
        if (ctx->gradients->threshold_grads) {
            float* tg = (float*)nimcp_tensor_data(ctx->gradients->threshold_grads);
            if (tg) {
                memset(tg, 0, nimcp_tensor_numel(ctx->gradients->threshold_grads) * sizeof(float));
            }
        }
        ctx->gradients->accumulation_count = 0;
    }

    /* Homeostatic regulation: adjust neuron thresholds to maintain
     * target population firing rate.  Operates on a slower timescale
     * than Hebbian learning (Turrigiano & Nelson, 2004).
     *
     * If neurons fire too much → increase threshold (reduce excitability)
     * If neurons fire too little → decrease threshold (increase excitability)
     */
    if (ctx->config.use_homeostatic && ctx->network) {
        float target_rate = ctx->config.target_population_rate;
        if (target_rate <= 0.0f) target_rate = 10.0f;

        for (uint32_t p = 0; p < ctx->network->n_populations; p++) {
            snn_population_t* pop = ctx->network->populations[p];
            if (!pop || pop->n_neurons == 0) continue;

            float pop_rate = pop->mean_rate;
            float rate_dev = pop_rate - target_rate;

            /* Only adjust if deviation exceeds 10% of target */
            if (fabsf(rate_dev) < target_rate * 0.1f) continue;

            /* Adjustment: small threshold shift proportional to deviation.
             * Scale: 0.01 mV per Hz deviation, clamped to ±0.5 mV/step */
            float thresh_shift = rate_dev * 0.01f;
            if (thresh_shift > 0.5f) thresh_shift = 0.5f;
            if (thresh_shift < -0.5f) thresh_shift = -0.5f;

            /* Apply to membrane_v threshold via population-level shift.
             * Positive shift = harder to spike (for over-firing neurons).
             * The shift is applied to the neural_network_t neurons that
             * back this population. */
            if (ctx->network->neural_net && pop->neuron_ids) {
                neural_network_t nn = ctx->network->neural_net;
                for (uint32_t ni = 0; ni < pop->n_neurons; ni++) {
                    uint32_t nid = pop->neuron_ids[ni];
                    neuron_t* neuron = neural_network_get_neuron(nn, nid);
                    if (neuron) {
                        neuron->threshold += thresh_shift;
                        /* Clamp to biologically reasonable range (-70 to -30 mV) */
                        if (neuron->threshold < -70.0f) neuron->threshold = -70.0f;
                        if (neuron->threshold > -30.0f) neuron->threshold = -30.0f;
                    }
                }
            }
        }
    }

    ctx->stats.total_steps++;
    if (ctx->stats.total_steps > 0) {
        ctx->stats.avg_loss = ctx->stats.total_loss / (double)ctx->stats.total_steps;
    }
    return SNN_SUCCESS;
}

int snn_backprop_zero_grad(snn_backprop_ctx_t* ctx) {
    // Guard: Null check
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "snn_backprop_zero_grad: ctx is NULL");
        return SNN_ERROR_NULL_POINTER;
    }

    /* Zero all gradient buffers */
    if (ctx->gradients) {
        if (ctx->gradients->weight_grads) {
            size_t numel = nimcp_tensor_numel(ctx->gradients->weight_grads);
            float* g = (float*)nimcp_tensor_data(ctx->gradients->weight_grads);
            if (g) memset(g, 0, numel * sizeof(float));
        }
        if (ctx->gradients->threshold_grads) {
            size_t numel = nimcp_tensor_numel(ctx->gradients->threshold_grads);
            float* g = (float*)nimcp_tensor_data(ctx->gradients->threshold_grads);
            if (g) memset(g, 0, numel * sizeof(float));
        }
        if (ctx->gradients->tau_grads) {
            size_t numel = nimcp_tensor_numel(ctx->gradients->tau_grads);
            float* g = (float*)nimcp_tensor_data(ctx->gradients->tau_grads);
            if (g) memset(g, 0, numel * sizeof(float));
        }
        ctx->gradients->accumulation_count = 0;
    }
    return SNN_SUCCESS;
}

//=============================================================================
// Complete Training Step (Stub)
//=============================================================================

int snn_backprop_train_step(
    snn_backprop_ctx_t* ctx,
    const float* inputs,
    const float* targets,
    uint32_t batch_size,
    float duration_ms,
    snn_train_result_t* result
) {
    // Guard: Null checks
    if (!ctx || !inputs || !targets) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "snn_backprop_train_step: ctx, inputs, or targets is NULL");
        return SNN_ERROR_NULL_POINTER;
    }

    /* Complete training step: forward → loss → backward → update */
    float lr = ctx->config.learning_rate;
    if (lr <= 0.0f) lr = 0.001f;

    /* 1. Zero gradients */
    snn_backprop_zero_grad(ctx);

    /* 2. Forward pass — allocate output buffer sized to batch */
    float* outputs = nimcp_calloc(batch_size, sizeof(float));
    if (!outputs) return SNN_ERROR_INVALID_STATE;

    int rc = snn_backprop_forward(ctx, inputs, batch_size, duration_ms, outputs);
    if (rc != SNN_SUCCESS) {
        nimcp_free(outputs);
        return rc;
    }

    /* 3. Compute loss */
    float loss = snn_backprop_compute_loss(ctx, outputs, targets, batch_size);

    /* 4. Backward pass */
    rc = snn_backprop_backward(ctx, targets, batch_size);
    if (rc != SNN_SUCCESS) {
        nimcp_free(outputs);
        return rc;
    }

    /* 5. Update weights */
    rc = snn_backprop_step(ctx, lr);

    /* 6. Fill result */
    if (result) {
        memset(result, 0, sizeof(snn_train_result_t));
        result->loss = loss;

        /* Compute gradient L2 norm from weight gradients */
        float grad_norm_sq = 0.0f;
        if (ctx->gradients && ctx->gradients->weight_grads) {
            const float* gdata = nimcp_tensor_data_const(ctx->gradients->weight_grads);
            uint32_t gcount = nimcp_tensor_numel(ctx->gradients->weight_grads);
            if (gdata && gcount > 0) {
                for (uint32_t gi = 0; gi < gcount; gi++) {
                    grad_norm_sq += gdata[gi] * gdata[gi];
                }
            }
        }
        result->gradient_norm = sqrtf(grad_norm_sq);

        /* Compute mean firing rate from forward pass spikes */
        float total_spikes = 0.0f;
        uint32_t total_neurons_time = 0;
        if (ctx->activations && ctx->activations->spikes) {
            const float* sdata = nimcp_tensor_data_const(ctx->activations->spikes);
            uint32_t scount = nimcp_tensor_numel(ctx->activations->spikes);
            if (sdata && scount > 0) {
                for (uint32_t si = 0; si < scount; si++) {
                    total_spikes += sdata[si];
                }
                total_neurons_time = scount;
            }
        }
        result->mean_firing_rate = (total_neurons_time > 0)
            ? (total_spikes / (float)total_neurons_time) : 0.0f;

        result->gradients_valid = (result->gradient_norm < 1e6f);
    }

    nimcp_free(outputs);
    return rc;
}

//=============================================================================
// Utility Functions
//=============================================================================

const char* snn_train_algorithm_name(snn_train_algorithm_t algorithm) {
    switch (algorithm) {
        case SNN_TRAIN_BPTT: return "BPTT";
        case SNN_TRAIN_TRUNCATED_BPTT: return "Truncated-BPTT";
        case SNN_TRAIN_EPROP: return "E-prop";
        case SNN_TRAIN_RTRL: return "RTRL";
        case SNN_TRAIN_SLAYER: return "SLAYER";
        case SNN_TRAIN_DECOLLE: return "DECOLLE";
        case SNN_TRAIN_HYBRID: return "Hybrid";
        default: return "Unknown";
    }
}

const char* snn_surrogate_method_name(snn_surrogate_method_t method) {
    switch (method) {
        case SNN_SURROGATE_SUPERSPIKE: return "SuperSpike";
        case SNN_SURROGATE_FAST_SIGMOID: return "FastSigmoid";
        case SNN_SURROGATE_SIGMOID: return "Sigmoid";
        case SNN_SURROGATE_ARCTAN: return "Arctan";
        case SNN_SURROGATE_TRIANGULAR: return "Triangular";
        case SNN_SURROGATE_RECTANGULAR: return "Rectangular";
        case SNN_SURROGATE_EXPONENTIAL: return "Exponential";
        default: return "Unknown";
    }
}

const char* snn_loss_type_name(snn_loss_type_t type) {
    switch (type) {
        case SNN_LOSS_SPIKE_COUNT: return "SpikeCount";
        case SNN_LOSS_FIRST_SPIKE_TIME: return "FirstSpikeTime";
        case SNN_LOSS_RATE_CODED_MSE: return "RateCodedMSE";
        case SNN_LOSS_RATE_CODED_CROSS_ENTROPY: return "RateCodedCrossEntropy";
        case SNN_LOSS_TEMPORAL_CROSS_ENTROPY: return "TemporalCrossEntropy";
        case SNN_LOSS_VAN_ROSSUM: return "vanRossum";
        case SNN_LOSS_VICTOR_PURPURA: return "VictorPurpura";
        case SNN_LOSS_MEMBRANE_POTENTIAL: return "MembranePotential";
        case SNN_LOSS_CUSTOM: return "Custom";
        default: return "Unknown";
    }
}

int snn_backprop_validate_config(const snn_backprop_config_t* config) {
    // Guard: Null check
    if (!config) {
        return SNN_ERROR_NULL_POINTER;
    }

    // Validate algorithm
    if (config->algorithm >= SNN_TRAIN_MODE_COUNT) {
        NIMCP_LOGGING_ERROR("Invalid training algorithm");
        return SNN_ERROR_INVALID_STATE;
    }

    // Validate surrogate method
    if (config->surrogate.method >= SNN_SURROGATE_COUNT) {
        NIMCP_LOGGING_ERROR("Invalid surrogate method");
        return SNN_ERROR_INVALID_STATE;
    }

    // Validate loss type
    if (config->loss.type >= SNN_LOSS_TYPE_COUNT) {
        NIMCP_LOGGING_ERROR("Invalid loss type");
        return SNN_ERROR_INVALID_STATE;
    }

    // Validate learning rate
    if (config->learning_rate <= 0.0f || config->learning_rate > 1.0f) {
        NIMCP_LOGGING_ERROR("Invalid learning rate");
        return SNN_ERROR_INVALID_STATE;
    }

    // Validate batch size
    if (config->batch_size == 0) {
        NIMCP_LOGGING_ERROR("Invalid batch size");
        return SNN_ERROR_INVALID_STATE;
    }

    // Validate sequence length
    if (config->sequence_length == 0) {
        NIMCP_LOGGING_ERROR("Invalid sequence length");
        return SNN_ERROR_INVALID_STATE;
    }

    return SNN_SUCCESS;
}

//=============================================================================
// Additional Stub Functions
//=============================================================================

int snn_backprop_forward_tensor(
    snn_backprop_ctx_t* ctx,
    const nimcp_tensor_t* input_tensor,
    nimcp_tensor_t* output_tensor
) {
    if (!ctx || !input_tensor) return SNN_ERROR_NULL_POINTER;

    const float* in_data = (const float*)nimcp_tensor_data_const(input_tensor);
    size_t in_numel = nimcp_tensor_numel(input_tensor);
    if (!in_data || in_numel == 0) return SNN_ERROR_INVALID_STATE;

    /* Determine batch size from tensor shape */
    const nimcp_tensor_shape_t* shape = nimcp_tensor_shape(input_tensor);
    uint32_t batch_size = (shape && shape->rank >= 2) ? shape->dims[0] : 1;

    float* out_data = output_tensor ? (float*)nimcp_tensor_data(output_tensor) : NULL;

    return snn_backprop_forward(ctx, in_data, batch_size, 0.0f, out_data);
}

int snn_backprop_backward_tensor(
    snn_backprop_ctx_t* ctx,
    const nimcp_tensor_t* target_tensor
) {
    if (!ctx || !target_tensor) return SNN_ERROR_NULL_POINTER;

    const float* target_data = (const float*)nimcp_tensor_data_const(target_tensor);
    const nimcp_tensor_shape_t* shape = nimcp_tensor_shape(target_tensor);
    uint32_t batch_size = (shape && shape->rank >= 2) ? shape->dims[0] : 1;

    return snn_backprop_backward(ctx, target_data, batch_size);
}

int snn_backprop_train_step_tensor(
    snn_backprop_ctx_t* ctx,
    const nimcp_tensor_t* input_tensor,
    const nimcp_tensor_t* target_tensor,
    snn_train_result_t* result
) {
    if (!ctx || !input_tensor || !target_tensor) return SNN_ERROR_NULL_POINTER;

    const float* in_data = (const float*)nimcp_tensor_data_const(input_tensor);
    const float* tgt_data = (const float*)nimcp_tensor_data_const(target_tensor);
    const nimcp_tensor_shape_t* shape = nimcp_tensor_shape(input_tensor);
    uint32_t batch_size = (shape && shape->rank >= 2) ? shape->dims[0] : 1;

    float duration = (float)(ctx->config.bptt.unroll_steps > 0 ?
                             ctx->config.bptt.unroll_steps : 100);

    return snn_backprop_train_step(ctx, in_data, tgt_data, batch_size, duration, result);
}

snn_batch_t* snn_batch_create(
    const float* inputs,
    const float* targets,
    uint32_t batch_size,
    uint32_t n_inputs,
    uint32_t n_outputs
) {
    if (!inputs || !targets || batch_size == 0 || n_inputs == 0 || n_outputs == 0) {
        return NULL;
    }

    snn_batch_t* batch = nimcp_malloc(sizeof(snn_batch_t));
    if (!batch) return NULL;

    size_t in_bytes = (size_t)batch_size * n_inputs * sizeof(float);
    size_t out_bytes = (size_t)batch_size * n_outputs * sizeof(float);

    batch->inputs = nimcp_malloc(in_bytes);
    batch->targets = nimcp_malloc(out_bytes);
    if (!batch->inputs || !batch->targets) {
        nimcp_free(batch->inputs);
        nimcp_free(batch->targets);
        nimcp_free(batch);
        return NULL;
    }

    memcpy(batch->inputs, inputs, in_bytes);
    memcpy(batch->targets, targets, out_bytes);
    batch->batch_size = batch_size;
    batch->n_inputs = n_inputs;
    batch->n_outputs = n_outputs;

    return batch;
}

void snn_batch_destroy(snn_batch_t* batch) {
    if (!batch) return;
    nimcp_free(batch->inputs);
    nimcp_free(batch->targets);
    nimcp_free(batch);
}

int snn_backprop_train_batch(
    snn_backprop_ctx_t* ctx,
    const snn_batch_t* batch,
    float duration_ms,
    snn_train_result_t* result
) {
    if (!ctx || !batch) return SNN_ERROR_NULL_POINTER;
    if (!batch->inputs || !batch->targets) return SNN_ERROR_INVALID_STATE;

    if (duration_ms <= 0.0f) {
        duration_ms = (float)(ctx->config.bptt.unroll_steps > 0 ?
                             ctx->config.bptt.unroll_steps : 100);
    }

    return snn_backprop_train_step(ctx, batch->inputs, batch->targets,
                                    batch->batch_size, duration_ms, result);
}

float snn_backprop_compute_loss(
    snn_backprop_ctx_t* ctx,
    const float* outputs,
    const float* targets,
    uint32_t batch_size
) {
    if (!ctx || !outputs || !targets || batch_size == 0) return 0.0f;

    /* MSE loss over batch: 1/N * sum((output - target)^2) */
    float total = 0.0f;
    for (uint32_t i = 0; i < batch_size; i++) {
        float diff = outputs[i] - targets[i];
        total += diff * diff;
    }
    float mse_loss = total / (float)batch_size;

    /* Diversity loss (unified anti-collapse) — loss-only, no gradient here.
     * Skip when managed by UTM — UTM applies diversity loss centrally. */
    float diversity_loss = 0.0f;
    if (!ctx->managed_by_utm) {
        diversity_loss = nimcp_anti_collapse_diversity_loss(
            &ctx->anti_collapse, outputs, NULL, batch_size);
    }

    return mse_loss + diversity_loss;
}

int snn_backprop_compute_loss_grad(
    snn_backprop_ctx_t* ctx,
    const float* outputs,
    const float* targets,
    uint32_t batch_size,
    float* gradients
) {
    if (!ctx || !outputs || !targets || !gradients || batch_size == 0) {
        return SNN_ERROR_NULL_POINTER;
    }

    /* MSE gradient: d/d_output (1/N * sum((output - target)^2)) = 2/N * (output - target) */
    float scale = 2.0f / (float)batch_size;
    for (uint32_t i = 0; i < batch_size; i++) {
        gradients[i] = scale * (outputs[i] - targets[i]);
    }

    return SNN_SUCCESS;
}

int snn_backprop_connect_gradient_manager(
    snn_backprop_ctx_t* ctx,
    nimcp_gradient_manager_ctx_t* grad_manager
) {
    if (!ctx) return SNN_ERROR_NULL_POINTER;

    ctx->grad_manager = grad_manager;
    NIMCP_LOGGING_INFO("Connected gradient manager to SNN backprop context");
    return SNN_SUCCESS;
}

nimcp_gradient_manager_ctx_t* snn_backprop_get_gradient_manager(
    snn_backprop_ctx_t* ctx
) {
    return ctx ? ctx->grad_manager : NULL;
}

int snn_backprop_get_stats(
    const snn_backprop_ctx_t* ctx,
    snn_backprop_stats_t* stats
) {
    if (!ctx || !stats) {
        return SNN_ERROR_NULL_POINTER;
    }

    *stats = ctx->stats;
    return SNN_SUCCESS;
}

void snn_backprop_reset_stats(snn_backprop_ctx_t* ctx) {
    if (!ctx) {
        return;
    }

    memset(&ctx->stats, 0, sizeof(snn_backprop_stats_t));
}

float snn_backprop_get_gradient_norm(const snn_backprop_ctx_t* ctx) {
    if (!ctx || !ctx->gradients || !ctx->gradients->weight_grads) {
        return 0.0f;
    }

    /* L2 norm of weight gradients */
    size_t numel = nimcp_tensor_numel(ctx->gradients->weight_grads);
    const float* g = (const float*)nimcp_tensor_data_const(ctx->gradients->weight_grads);
    if (!g || numel == 0) return 0.0f;

    float norm_sq = 0.0f;
    for (size_t i = 0; i < numel; i++) {
        norm_sq += g[i] * g[i];
    }
    return sqrtf(norm_sq);
}

float snn_backprop_get_weight_norm(const snn_backprop_ctx_t* ctx) {
    if (!ctx || !ctx->network) {
        return 0.0f;
    }

    /* Estimate weight norm from population membrane potential magnitudes
     * as a proxy when direct weight access is not available */
    float norm_sq = 0.0f;
    for (uint32_t p = 0; p < ctx->network->n_populations; p++) {
        snn_population_t* pop = ctx->network->populations[p];
        if (!pop || !pop->membrane_v) continue;

        size_t numel = nimcp_tensor_numel(pop->membrane_v);
        const float* v = (const float*)nimcp_tensor_data_const(pop->membrane_v);
        if (!v) continue;

        for (size_t i = 0; i < numel; i++) {
            norm_sq += v[i] * v[i];
        }
    }
    return sqrtf(norm_sq);
}

void snn_backprop_set_managed_by_utm(snn_backprop_ctx_t* ctx, bool managed) {
    if (ctx) ctx->managed_by_utm = managed;
}

const float* snn_backprop_get_input_grad(const snn_backprop_ctx_t* ctx,
                                         uint32_t* out_size) {
    if (!ctx || !ctx->last_input_grad) {
        if (out_size) *out_size = 0;
        return NULL;
    }
    if (out_size) *out_size = ctx->input_grad_size;
    return ctx->last_input_grad;
}

//=============================================================================
// C2: Flat Weight Extraction/Setting for UTM Param Groups
//=============================================================================

float* snn_backprop_get_flat_weights(snn_backprop_ctx_t* ctx, size_t* out_count) {
    if (!ctx || !ctx->network || !ctx->network->neural_net || !out_count) {
        if (out_count) *out_count = 0;
        return NULL;
    }

    /* Count total synapses across all populations */
    snn_network_t* net = ctx->network;
    size_t total = 0;
    for (uint32_t p = 0; p < net->n_populations; p++) {
        snn_population_t* pop = net->populations[p];
        if (!pop) continue;
        for (uint32_t n = 0; n < pop->n_neurons; n++) {
            neuron_t* neuron = neural_network_get_neuron(net->neural_net, pop->neuron_ids[n]);
            if (!neuron) continue;
            total += sparse_synapse_count(&neuron->incoming);
        }
    }

    if (total == 0) { *out_count = 0; return NULL; }

    float* flat = (float*)nimcp_malloc(total * sizeof(float));
    if (!flat) { *out_count = 0; return NULL; }

    size_t idx = 0;
    for (uint32_t p = 0; p < net->n_populations; p++) {
        snn_population_t* pop = net->populations[p];
        if (!pop) continue;
        for (uint32_t n = 0; n < pop->n_neurons; n++) {
            neuron_t* neuron = neural_network_get_neuron(net->neural_net, pop->neuron_ids[n]);
            if (!neuron) continue;
            uint32_t sc = sparse_synapse_count(&neuron->incoming);
            for (uint32_t s = 0; s < sc; s++) {
                synapse_handle_t* h = sparse_synapse_get(&neuron->incoming, s);
                flat[idx++] = h ? h->weight : 0.0f;
            }
        }
    }

    *out_count = total;
    return flat;
}

int snn_backprop_set_flat_weights(snn_backprop_ctx_t* ctx, const float* weights, size_t count) {
    if (!ctx || !ctx->network || !ctx->network->neural_net || !weights) return -1;

    snn_network_t* net = ctx->network;
    size_t idx = 0;
    for (uint32_t p = 0; p < net->n_populations; p++) {
        snn_population_t* pop = net->populations[p];
        if (!pop) continue;
        for (uint32_t n = 0; n < pop->n_neurons; n++) {
            neuron_t* neuron = neural_network_get_neuron(net->neural_net, pop->neuron_ids[n]);
            if (!neuron) continue;
            uint32_t sc = sparse_synapse_count(&neuron->incoming);
            for (uint32_t s = 0; s < sc; s++) {
                if (idx >= count) return 0;
                synapse_handle_t* h = sparse_synapse_get(&neuron->incoming, s);
                if (h) h->weight = weights[idx];
                idx++;
            }
        }
    }
    return 0;
}

float* snn_backprop_get_flat_weight_grads(snn_backprop_ctx_t* ctx, size_t* out_count) {
    if (!ctx || !ctx->gradients || !ctx->gradients->weight_grads || !out_count) {
        if (out_count) *out_count = 0;
        return NULL;
    }
    float* data = (float*)nimcp_tensor_data(ctx->gradients->weight_grads);
    *out_count = (size_t)nimcp_tensor_numel(ctx->gradients->weight_grads);
    return data;
}
