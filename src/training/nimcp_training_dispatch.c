/**
 * @file nimcp_training_dispatch.c
 * @brief Training Dispatcher Implementation
 *
 * WHAT: Routes training steps to appropriate network-type-specific trainers
 * WHY:  Unified training interface for heterogeneous network architectures
 * HOW:  Switch-based dispatch to SNN/LNN/CNN/Adaptive training systems
 *
 * @author NIMCP Team
 * @date 2025-01-16
 */

#include "training/nimcp_training_dispatch.h"
#include "core/brain/nimcp_brain_internal.h"
#include "nimcp.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/tensor/nimcp_tensor.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_macros.h"

// SNN training includes
#include <math.h>
#include "snn/nimcp_snn_training.h"

/* SNN training dispatch constants */
#define SNN_LARGE_NETWORK_THRESHOLD   100000   /**< Force R-STDP above this neuron count */
#define SNN_RSTDP_DEFAULT_TRACE_DIM   256      /**< Fallback eligibility trace dimension */
#define SNN_RSTDP_ACTIVITY_THRESHOLD  0.01f    /**< Min activation for eligibility update */
#define SNN_RSTDP_TRACE_CLAMP         10.0f    /**< Eligibility trace clamping bound */
#define SNN_RSTDP_NORM_EPSILON        1e-6f    /**< Normalization denominator floor */

/* Public API: Set SNN input scaling factor at runtime.
 * Called from Python or C to adjust how aggressively SNN inputs are amplified.
 * Higher values = more spiking; lower = less spiking. Default = 45.0. */
static float _snn_global_input_scale = 45.0f;
void nimcp_snn_set_input_scale(float scale) {
    if (scale > 0.0f && scale < 10000.0f) {
        _snn_global_input_scale = scale;
    }
}
float nimcp_snn_get_input_scale(void) {
    return _snn_global_input_scale;
}
#include "snn/nimcp_snn_network.h"
#include "snn/nimcp_snn_types.h"
#include "core/neuralnet/nimcp_sparse_synapse.h"
#include "core/neuralnet/nimcp_neuralnet.h"

// GPU plasticity bridge for GPU-accelerated STDP/BCM/homeostatic
#include "gpu/plasticity/nimcp_gpu_plasticity_bridge.h"

// LNN training includes
#include "lnn/nimcp_lnn_training.h"
#include "lnn/nimcp_lnn_network.h"
#include "lnn/nimcp_lnn_types.h"

// CNN training includes
#include "training/nimcp_cnn_training.h"

// Backprop for adaptive networks
#include "core/neuralnet/nimcp_neuralnet_backprop.h"
#include "plasticity/adaptive/nimcp_adaptive.h"

#include <string.h>
#include <math.h>

//=============================================================================
// Health Agent Forward Declarations (Phase 8: Heartbeat for Long Operations)
// Avoid including full header to prevent type conflicts
//=============================================================================
struct nimcp_health_agent;
typedef struct nimcp_health_agent nimcp_health_agent_t;
extern void nimcp_health_agent_heartbeat_ex(nimcp_health_agent_t* agent,
                                             const char* operation,
                                             float progress);

/**
 * @brief Send heartbeat from training operation
 * @param brain Brain context with health agent
 * @param operation Operation name
 * @param progress Progress value [0.0-1.0]
 */
static inline void training_heartbeat(brain_t brain, const char* operation, float progress) {
    if (brain && brain->health_agent_enabled && brain->health_agent) {
        nimcp_health_agent_heartbeat_ex(brain->health_agent, operation, progress);
    }
}

//=============================================================================
// Internal Helper Functions
//=============================================================================

/**
 * @brief Initialize SNN training context
 */
static int init_snn_training(brain_t brain, const nimcp_training_config_t* config) {
    if (!brain || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "init_snn_training: required parameter is NULL (brain, config)");
        return -1;
    }

    // Check if SNN network exists
    if (!brain->snn_network) {
        NIMCP_LOGGING_WARN("SNN training requested but no SNN network present");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "init_snn_training: brain->snn_network is NULL");
        return -1;
    }

    // Create SNN training context based on method
    snn_training_ctx_t* ctx = NULL;

    // Get actual neuron count from SNN network config
    uint32_t n_neurons = brain->snn_network->config.n_inputs
                       + brain->snn_network->config.n_hidden
                       + brain->snn_network->config.n_outputs;

    /* Eligibility trace dimensions for training context allocation.
     * For large hierarchical SNNs (>100K neurons), the dense n×n eligibility
     * matrix would be infeasible (1.5M² = 9TB). Use the brain I/O dims instead
     * — the training context only needs traces for the input→output mapping,
     * not the full internal connectivity (internal plasticity is handled by
     * the per-synapse STDP in the SNN step function). */
    uint32_t trace_pre  = brain->snn_network->config.n_inputs;
    uint32_t trace_post = brain->snn_network->config.n_outputs;
    if (trace_pre == 0) trace_pre = SNN_RSTDP_DEFAULT_TRACE_DIM;
    if (trace_post == 0) trace_post = SNN_RSTDP_DEFAULT_TRACE_DIM;

    /* Force R-STDP for large SNN networks (>100K neurons).
     * BPTT/surrogate gradient methods are O(n²) in memory and infeasible
     * at this scale. R-STDP uses local eligibility traces + reward signal,
     * scaling linearly with neuron count. */
    nimcp_snn_train_method_t effective_method = config->snn_method;
    if (n_neurons > SNN_LARGE_NETWORK_THRESHOLD &&
        effective_method != NIMCP_SNN_TRAIN_R_STDP &&
        effective_method != NIMCP_SNN_TRAIN_STDP) {
        NIMCP_LOGGING_INFO("SNN has %u neurons (>100K) — forcing R-STDP "
                           "(was method=%d)", n_neurons, effective_method);
        effective_method = NIMCP_SNN_TRAIN_R_STDP;
    }

    switch (effective_method) {
        case NIMCP_SNN_TRAIN_STDP: {
            snn_stdp_config_t stdp_cfg;
            snn_stdp_config_default(&stdp_cfg);
            stdp_cfg.tau_plus = config->snn_eligibility_tau;
            stdp_cfg.tau_minus = config->snn_eligibility_tau * 1.05F;  // Slightly longer for LTD
            ctx = snn_training_create_stdp(&stdp_cfg);
            break;
        }

        case NIMCP_SNN_TRAIN_R_STDP: {
            snn_rstdp_config_t rstdp_cfg;
            snn_rstdp_config_default(&rstdp_cfg);
            rstdp_cfg.eligibility_tau = config->snn_eligibility_tau;
            rstdp_cfg.reward_tau = config->snn_reward_tau;
            ctx = snn_training_create_rstdp(&rstdp_cfg, trace_pre, trace_post);
            break;
        }

        case NIMCP_SNN_TRAIN_EPROP: {
            snn_eprop_config_t eprop_cfg;
            snn_eprop_config_default(&eprop_cfg);
            eprop_cfg.learning_rate = config->learning_rate;
            eprop_cfg.eligibility_tau = config->snn_eligibility_tau;
            ctx = snn_training_create_eprop(&eprop_cfg, trace_pre, trace_post);
            break;
        }

        case NIMCP_SNN_TRAIN_SURROGATE: {
            snn_surrogate_config_t surr_cfg;
            snn_surrogate_config_default(&surr_cfg);
            surr_cfg.beta = config->snn_surrogate_beta;
            surr_cfg.learning_rate = config->learning_rate;
            ctx = snn_training_create_surrogate(&surr_cfg, trace_pre, trace_post);
            break;
        }

        case NIMCP_SNN_TRAIN_HOMEOSTATIC: {
            // Homeostatic uses STDP as base
            snn_stdp_config_t stdp_cfg;
            snn_stdp_config_default(&stdp_cfg);
            ctx = snn_training_create_stdp(&stdp_cfg);
            break;
        }

        default:
            NIMCP_LOGGING_ERROR("Unknown SNN training method: %d", config->snn_method);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "init_snn_training: operation failed");
            return -1;
    }

    if (!ctx) {
        NIMCP_LOGGING_ERROR("Failed to create SNN training context");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "init_snn_training: ctx is NULL");
        return -1;
    }

    brain->snn_training_ctx = ctx;
    NIMCP_LOGGING_INFO("Initialized SNN training with method=%d (%u neurons)",
                       effective_method, n_neurons);
    return 0;
}

/**
 * @brief Initialize LNN training context
 */
static int init_lnn_training(brain_t brain, const nimcp_training_config_t* config) {
    if (!brain || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "init_lnn_training: required parameter is NULL (brain, config)");
        return -1;
    }

    // Check if LNN network exists
    if (!brain->lnn_network) {
        NIMCP_LOGGING_WARN("LNN training requested but no LNN network present");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "init_lnn_training: brain->lnn_network is NULL");
        return -1;
    }

    // Create LNN training config
    lnn_training_config_t lnn_cfg;
    if (lnn_training_config_default(&lnn_cfg) != 0) {
        NIMCP_LOGGING_ERROR("Failed to initialize LNN training config");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "init_lnn_training: validation failed");
        return -1;
    }

    lnn_cfg.learning_rate = config->learning_rate;
    lnn_cfg.gradient_clip_norm = config->gradient_clip_value;

    // Map training method
    switch (config->lnn_method) {
        case NIMCP_LNN_TRAIN_ADJOINT:
            lnn_cfg.lnn_train_mode = LNN_TRAIN_ADJOINT;
            lnn_cfg.use_adjoint_checkpointing = config->lnn_use_adjoint_checkpointing;
            break;
        case NIMCP_LNN_TRAIN_BPTT:
            lnn_cfg.lnn_train_mode = LNN_TRAIN_BPTT;
            lnn_cfg.bptt_truncation = config->lnn_bptt_truncation;
            break;
        case NIMCP_LNN_TRAIN_RTRL:
            lnn_cfg.lnn_train_mode = LNN_TRAIN_RTRL;
            break;
        case NIMCP_LNN_TRAIN_EPROP:
            lnn_cfg.lnn_train_mode = LNN_TRAIN_EPROP;
            break;
        default:
            lnn_cfg.lnn_train_mode = LNN_TRAIN_ADJOINT;
    }

    // Destroy existing LNN training context if present (prevent leak)
    if (brain->lnn_training_ctx) {
        NIMCP_LOGGING_DEBUG("Destroying existing LNN training context before re-creation");
        lnn_training_destroy(brain->lnn_training_ctx);
        brain->lnn_training_ctx = NULL;
    }

    // Create training context
    lnn_training_ctx_t* ctx = lnn_training_create(brain->lnn_network, &lnn_cfg);
    if (!ctx) {
        NIMCP_LOGGING_ERROR("Failed to create LNN training context");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "init_lnn_training: ctx is NULL");
        return -1;
    }

    brain->lnn_training_ctx = ctx;
    NIMCP_LOGGING_INFO("Initialized LNN training with method=%d", config->lnn_method);
    return 0;
}

/**
 * @brief Initialize CNN training (already part of cnn_trainer)
 */
static int init_cnn_training(brain_t brain, const nimcp_training_config_t* config) {
    if (!brain || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "init_cnn_training: required parameter is NULL (brain, config)");
        return -1;
    }

    // CNN trainer is created with the network, training is built-in
    if (!brain->cnn_trainer) {
        NIMCP_LOGGING_WARN("CNN training requested but no CNN trainer present");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "init_cnn_training: brain->cnn_trainer is NULL");
        return -1;
    }

    NIMCP_LOGGING_INFO("CNN trainer already initialized");
    return 0;
}

//=============================================================================
// SNN Training Step
//=============================================================================

int training_dispatch_snn_step(
    brain_t brain,
    const float* inputs,
    uint32_t num_inputs,
    const float* targets,
    uint32_t num_targets,
    training_dispatch_result_t* result)
{
    if (!brain->snn_network || !brain->snn_training_ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "training_dispatch_snn_step: required parameter is NULL (brain->snn_network, brain->snn_training_ctx)");
        return -1;
    }

    snn_network_t* snn = brain->snn_network;
    snn_training_ctx_t* ctx = brain->snn_training_ctx;

    /* Set inputs on SNN (average-pool if brain dims > SNN dims) */
    uint32_t snn_in = snn->config.n_inputs;
    uint32_t snn_out = snn->config.n_outputs;
    float* pooled_input = NULL;
    const float* input_ptr = inputs;
    uint32_t input_dim = num_inputs;

    if (num_inputs > snn_in) {
        /* Average-pool down to SNN input dimension */
        pooled_input = nimcp_calloc(snn_in, sizeof(float));
        if (pooled_input) {
            uint32_t stride = num_inputs / snn_in;
            for (uint32_t i = 0; i < snn_in; i++) {
                float sum = 0.0f;
                uint32_t start = i * stride;
                uint32_t end = (i + 1 < snn_in) ? (i + 1) * stride : num_inputs;
                for (uint32_t j = start; j < end; j++) sum += inputs[j];
                pooled_input[i] = sum / (float)(end - start);
            }
            input_ptr = pooled_input;
            input_dim = snn_in;
        }
    } else if (num_inputs < snn_in) {
        /* Zero-pad up to SNN input dimension */
        pooled_input = nimcp_calloc(snn_in, sizeof(float));
        if (pooled_input) {
            memcpy(pooled_input, inputs, num_inputs * sizeof(float));
            input_ptr = pooled_input;
            input_dim = snn_in;
        }
    }

    /* Apply SNN input scaling — boost weak inputs above firing threshold.
     * Uses max-abs normalization so the strongest feature reaches the full
     * scale, well above the ~20 mV firing threshold.
     *
     * IMPORTANT: The scaled buffer is kept alive until after
     * snn_backprop_forward, because BPTT internally calls
     * snn_network_set_inputs(net, inputs, ...) which OVERWRITES whatever
     * was set earlier. If we pass the raw input_ptr to BPTT, it replaces
     * our scaled values with unscaled ones → sub-threshold → silent SNN.
     * This was the root cause of persistent SNN silence during training. */
    float _snn_input_scale = _snn_global_input_scale;
    float* _snn_scaled_input = NULL;
    const float* snn_input_for_bptt = input_ptr;  /* fallback: raw */
    if (input_ptr && input_dim > 0) {
        _snn_scaled_input = nimcp_calloc(input_dim, sizeof(float));
        if (_snn_scaled_input) {
            float max_abs = 0.0f;
            for (uint32_t i = 0; i < input_dim; i++) {
                float a = fabsf(input_ptr[i]);
                if (a > max_abs) max_abs = a;
            }
            if (max_abs > 1e-8f) {
                float inv = _snn_input_scale / max_abs;
                for (uint32_t i = 0; i < input_dim; i++)
                    _snn_scaled_input[i] = input_ptr[i] * inv;
            }
            snn_network_set_inputs(snn, _snn_scaled_input, input_dim);
            snn_input_for_bptt = _snn_scaled_input;  /* BPTT gets scaled */
        } else {
            snn_network_set_inputs(snn, input_ptr, input_dim);
        }
    } else {
        snn_network_set_inputs(snn, input_ptr, input_dim);
    }
    /* NOTE: pooled_input is freed AFTER the training switch because
     * input_ptr (which may alias pooled_input) is used in R-STDP
     * eligibility accumulation. See the nimcp_free(pooled_input) below. */

    // Use BPTT forward if backprop context is available
    float dt = (snn->sim && snn->sim->dt_ms > 0.0f) ? snn->sim->dt_ms : 1.0F;
    int rc = 0;

    /* Check if SNN uses lightweight CSR storage — BPTT doesn't work with CSR
     * because it iterates the neural_net's sparse synapse system which has no
     * connections for lightweight populations. Use the single-step + R-STDP
     * path instead (the fallback else branch below). */
    bool snn_has_lightweight = false;
    for (uint32_t p = 0; p < snn->n_populations && !snn_has_lightweight; p++) {
        if (snn->populations[p] && snn->populations[p]->lightweight)
            snn_has_lightweight = true;
    }

    if (brain->snn_backprop_ctx && !snn_has_lightweight) {
        /* BPTT path: forward runs n_steps of simulation, backward computes gradients */
        extern int snn_backprop_forward(void* ctx, const float* inputs,
                                         uint32_t batch_size, float duration_ms,
                                         float* outputs);
        extern int snn_backprop_backward(void* ctx, const float* dl_doutput,
                                          uint32_t batch_size);
        extern void snn_backprop_step(void* ctx, float learning_rate);

        float* outputs = nimcp_calloc(snn_out, sizeof(float));
        if (outputs) {
            float duration = 100.0f;
            rc = snn_backprop_forward(brain->snn_backprop_ctx,
                                       snn_input_for_bptt,
                                       1, duration, outputs);
            if (rc == 0 && targets && num_targets > 0) {
                /* Compute output gradient: predictions - targets */
                uint32_t grad_dim = (num_targets < snn_out) ? num_targets : snn_out;
                float* grad = nimcp_calloc(grad_dim, sizeof(float));
                if (grad) {
                    float loss_val = 0.0f;
                    for (uint32_t i = 0; i < grad_dim; i++) {
                        grad[i] = outputs[i] - targets[i];
                        loss_val += grad[i] * grad[i];
                    }
                    loss_val /= (float)grad_dim;

                    snn_backprop_backward(brain->snn_backprop_ctx, grad, 1);
                    snn_backprop_step(brain->snn_backprop_ctx, 0.0f);

                    if (result) result->loss = loss_val;
                    nimcp_free(grad);
                }
            }
            nimcp_free(outputs);
        }
    } else {
        /* Fallback: single step + plasticity rule */
        rc = snn_network_step(snn, dt);
        if (rc < 0) {
            NIMCP_LOGGING_ERROR("SNN network step failed: %d", rc);
            nimcp_free(pooled_input);
            nimcp_free(_snn_scaled_input);
            _snn_scaled_input = NULL;
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                "training_dispatch_snn_step: SNN network step failed");
            return -1;
        }
    }

    /* Free the scaled input buffer now that BPTT is done */
    nimcp_free(_snn_scaled_input);
    _snn_scaled_input = NULL;

    // Apply training based on method (STDP, R-STDP, eProp, surrogate)
    snn_train_mode_t mode = ctx->mode;
    float loss = 0.0F;
    uint32_t updates = 0;

    /* Extract SNN output predictions (needed by R-STDP for eligibility
     * accumulation and by surrogate for loss gradient). */
    float* predictions = nimcp_calloc(snn_out, sizeof(float));
    if (predictions) {
        snn_network_get_outputs(snn, predictions, snn_out);
    }

    switch (mode) {
        case SNN_TRAIN_STDP:
            if (brain->gpu_enabled && brain->gpu_ctx && brain->gpu_plasticity_state) {
                extern uint32_t gpu_plasticity_stdp_apply(
                    struct nimcp_gpu_context_s*, struct gpu_plasticity_state_s*,
                    snn_network_t*, snn_training_ctx_t*);
                updates = gpu_plasticity_stdp_apply(
                    (struct nimcp_gpu_context_s*)brain->gpu_ctx,
                    brain->gpu_plasticity_state, snn, ctx);
                if (updates == 0)
                    updates = snn_stdp_apply_network(ctx, snn, 0.0F);
            } else {
                updates = snn_stdp_apply_network(ctx, snn, 0.0F);
            }
            break;

        case SNN_TRAIN_R_STDP: {
            /* 1. Decay existing eligibility traces */
            snn_rstdp_update_eligibility(ctx, dt);

            /* 2. Accumulate eligibility from input×output co-activation.
             * Uses continuous-valued input and SNN output predictions as
             * proxies for spike correlations (avoids opaque tensor access).
             * Both are normalized to [0,1] to keep traces bounded. */
            if (ctx->eligibility && input_ptr && predictions) {
                float* elig_data = (float*)nimcp_tensor_data(ctx->eligibility);
                const nimcp_tensor_shape_t* sh = nimcp_tensor_shape(ctx->eligibility);
                if (elig_data && sh && sh->rank >= 2) {
                    uint32_t elig_rows = sh->dims[0];
                    uint32_t elig_cols = sh->dims[sh->rank - 1];
                    uint32_t n_pre = (snn_in < elig_rows) ? snn_in : elig_rows;
                    uint32_t n_post = (snn_out < elig_cols) ? snn_out : elig_cols;

                    /* Find max activation for normalization */
                    float max_pre = 0.0f, max_post = 0.0f;
                    for (uint32_t i = 0; i < n_pre; i++) {
                        float v = fabsf(input_ptr[i]);
                        if (v > max_pre) max_pre = v;
                    }
                    for (uint32_t j = 0; j < n_post; j++) {
                        float v = fabsf(predictions[j]);
                        if (v > max_post) max_post = v;
                    }
                    float inv_pre  = (max_pre  > SNN_RSTDP_NORM_EPSILON) ? 1.0f / max_pre  : 0.0f;
                    float inv_post = (max_post > SNN_RSTDP_NORM_EPSILON) ? 1.0f / max_post : 0.0f;

                    for (uint32_t i = 0; i < n_pre; i++) {
                        float pre_act = input_ptr[i] * inv_pre;
                        if (pre_act < SNN_RSTDP_ACTIVITY_THRESHOLD) continue;
                        for (uint32_t j = 0; j < n_post; j++) {
                            float post_act = predictions[j] * inv_post;
                            if (post_act > SNN_RSTDP_ACTIVITY_THRESHOLD) {
                                float e = elig_data[i * elig_cols + j]
                                        + pre_act * post_act;
                                if (e > SNN_RSTDP_TRACE_CLAMP) e = SNN_RSTDP_TRACE_CLAMP;
                                if (e < -SNN_RSTDP_TRACE_CLAMP) e = -SNN_RSTDP_TRACE_CLAMP;
                                elig_data[i * elig_cols + j] = e;
                            }
                        }
                    }
                }
            }

            /* 3. Apply reward-modulated weight update */
            updates = snn_rstdp_apply(ctx, snn);

            /* 4. Homeostatic plasticity (Turrigiano-style synaptic scaling).
             * Runs every SNN_HOMEOSTATIC_INTERVAL R-STDP applies to pull
             * per-population firing rates back toward biological target.
             * Prevents runaway drift into silent or saturated regimes. */
            {
                static uint32_t _homeostatic_counter = 0;
                const uint32_t SNN_HOMEOSTATIC_INTERVAL = 10;
                _homeostatic_counter++;
                if (_homeostatic_counter >= SNN_HOMEOSTATIC_INTERVAL) {
                    _homeostatic_counter = 0;
                    extern uint32_t snn_homeostatic_apply(
                        snn_training_ctx_t*, snn_network_t*);
                    snn_homeostatic_apply(ctx, snn);
                }
            }
            break;
        }

        case SNN_TRAIN_EPROP:
            // eProp needs spike arrays
            updates = snn_eprop_apply(ctx, snn, 0.0F);
            break;

        case SNN_TRAIN_SURROGATE: {
            // Surrogate gradients - decode output spikes and compute loss gradient
            uint32_t grad_dim = (num_targets < snn_out) ? num_targets : snn_out;
            float* output_grad = nimcp_calloc(grad_dim, sizeof(float));
            float* membrane_v = nimcp_calloc(grad_dim, sizeof(float));
            float* input_grad = nimcp_calloc(grad_dim, sizeof(float));
            if (predictions && output_grad && membrane_v && input_grad && targets) {
                /* predictions already extracted above the switch */

                // Get membrane potentials from output population neurons
                if (snn->output_pop && snn->neural_net && snn->output_pop->neuron_ids) {
                    for (uint32_t i = 0; i < grad_dim && i < snn->output_pop->n_neurons; i++) {
                        neuron_t* n = neural_network_get_neuron(
                            snn->neural_net, snn->output_pop->neuron_ids[i]);
                        membrane_v[i] = n ? n->state : 0.0f;
                    }
                }

                // Compute MSE gradient against targets
                for (uint32_t i = 0; i < grad_dim; i++) {
                    float diff = predictions[i] - targets[i];
                    output_grad[i] = 2.0F * diff / (float)grad_dim;
                    loss += diff * diff;
                }
                loss /= (float)grad_dim;

                // Backprop with surrogate (now with real membrane_v and input_grad)
                snn_surrogate_backward(ctx, output_grad, membrane_v, grad_dim, input_grad);

                // Apply surrogate gradients to output population synapse weights
                if (snn->output_pop && snn->neural_net && snn->output_pop->neuron_ids) {
                    float lr = snn->config.learning_rate;
                    if (lr <= 0.0f) lr = 0.001f;
                    for (uint32_t i = 0; i < grad_dim && i < snn->output_pop->n_neurons; i++) {
                        neuron_t* n = neural_network_get_neuron(
                            snn->neural_net, snn->output_pop->neuron_ids[i]);
                        if (!n) continue;
                        uint32_t syn_count = sparse_synapse_count(&n->incoming);
                        for (uint32_t s = 0; s < syn_count; s++) {
                            synapse_handle_t* h = sparse_synapse_get(&n->incoming, s);
                            if (h) h->weight -= lr * input_grad[i];
                        }
                        updates++;
                    }
                }
            }
            nimcp_free(output_grad);
            nimcp_free(membrane_v);
            nimcp_free(input_grad);
            break;
        }

        default:
            break;
    }

    /* Deferred free: pooled_input may have been aliased by input_ptr,
     * which was used in the R-STDP eligibility accumulation above. */
    nimcp_free(pooled_input);
    pooled_input = NULL;

    /* For non-surrogate modes, compute MSE loss from output spike rates
     * so callers get meaningful loss metrics for all SNN training methods. */
    if (loss == 0.0f && mode != SNN_TRAIN_SURROGATE && targets && num_targets > 0
        && predictions) {
        uint32_t cmp_dim = (num_targets < snn_out) ? num_targets : snn_out;
        for (uint32_t i = 0; i < cmp_dim; i++) {
            float diff = predictions[i] - targets[i];
            loss += diff * diff;
        }
        loss /= (float)cmp_dim;
    }

    nimcp_free(predictions);

    if (result) {
        result->loss = isfinite(loss) ? loss : 1.0f;
        result->type_specific.snn.ltp_events = updates / 2;  // Approximate
        result->type_specific.snn.ltd_events = updates / 2;
    }

    return 0;
}

//=============================================================================
// LNN Training Step
//=============================================================================

int training_dispatch_lnn_step(
    brain_t brain,
    const float* inputs,
    uint32_t num_inputs,
    const float* targets,
    uint32_t num_targets,
    training_dispatch_result_t* result)
{
    if (!brain->lnn_network || !brain->lnn_training_ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "training_dispatch_lnn_step: required parameter is NULL (brain->lnn_network, brain->lnn_training_ctx)");
        return -1;
    }

    lnn_training_ctx_t* ctx = brain->lnn_training_ctx;

    // LNN may have smaller input/output dims than the brain (capped at 256
    // for O(n^2) adjoint efficiency). Average-pool inputs and truncate targets
    // to match the LNN's expected dimensions.
    uint32_t lnn_in = brain->lnn_network->n_inputs;
    uint32_t lnn_out = brain->lnn_network->n_outputs;
    uint32_t eff_inputs = (num_inputs > lnn_in) ? lnn_in : num_inputs;
    uint32_t eff_targets = (num_targets > lnn_out) ? lnn_out : num_targets;
    if (num_targets > lnn_out) {
        NIMCP_LOGGING_WARN("LNN target truncation: %u targets → %u (LNN output dim)",
                           num_targets, lnn_out);
    }

    uint32_t input_dims[1] = {eff_inputs};
    uint32_t target_dims[1] = {eff_targets};

    nimcp_tensor_t* input_tensor = nimcp_tensor_create(input_dims, 1, NIMCP_DTYPE_F32);
    nimcp_tensor_t* target_tensor = nimcp_tensor_create(target_dims, 1, NIMCP_DTYPE_F32);

    if (!input_tensor || !target_tensor) {
        if (input_tensor) nimcp_tensor_destroy(input_tensor);
        if (target_tensor) nimcp_tensor_destroy(target_tensor);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "training_dispatch_lnn_step: LNN training step failed");
        return -1;
    }

    float* in_data = (float*)nimcp_tensor_data(input_tensor);
    float* tgt_data = (float*)nimcp_tensor_data(target_tensor);
    if (in_data && tgt_data) {
        if (num_inputs > lnn_in) {
            // Average-pool: group brain inputs into lnn_in bins
            uint32_t stride = num_inputs / lnn_in;
            for (uint32_t i = 0; i < lnn_in; i++) {
                float sum = 0.0f;
                uint32_t start = i * stride;
                uint32_t end = (i + 1 < lnn_in) ? (i + 1) * stride : num_inputs;
                for (uint32_t j = start; j < end; j++) {
                    sum += inputs[j];
                }
                in_data[i] = sum / (float)(end - start);
            }
        } else {
            memcpy(in_data, inputs, num_inputs * sizeof(float));
        }
        // Targets: truncate to LNN output size (first eff_targets elements)
        memcpy(tgt_data, targets, eff_targets * sizeof(float));
    }

    // Run training step
    float loss = 0.0F;
    int rc = lnn_training_step(ctx, input_tensor, target_tensor, &loss);

    nimcp_tensor_destroy(input_tensor);
    nimcp_tensor_destroy(target_tensor);

    if (rc < 0) {
        NIMCP_LOGGING_ERROR("LNN training step failed: %d", rc);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "training_dispatch_lnn_step: LNN training step failed");
        return -1;
    }

    if (result) {
        result->loss = loss;
        result->learning_rate = lnn_training_get_lr(ctx);
    }

    return 0;
}

//=============================================================================
// CNN Training Step
//=============================================================================

static int cnn_train_step(
    brain_t brain,
    const float* inputs,
    uint32_t num_inputs,
    const float* targets,
    uint32_t num_targets,
    training_dispatch_result_t* result)
{
    if (!brain->cnn_trainer) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cnn_train_step: brain->cnn_trainer is NULL");
        return -1;
    }

    cnn_trainer_t* trainer = brain->cnn_trainer;

    // Create input/target tensors as 2D (batch=1, features) — dense layers
    // interpret dims[0] as batch size, so 1D {N} would be misread as batch=N
    uint32_t input_dims[2] = {1, num_inputs};
    uint32_t target_dims[2] = {1, num_targets};

    nimcp_tensor_t* input_tensor = nimcp_tensor_create(input_dims, 2, NIMCP_DTYPE_F32);
    nimcp_tensor_t* target_tensor = nimcp_tensor_create(target_dims, 2, NIMCP_DTYPE_F32);

    if (!input_tensor || !target_tensor) {
        if (input_tensor) nimcp_tensor_destroy(input_tensor);
        if (target_tensor) nimcp_tensor_destroy(target_tensor);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "cnn_train_step: validation failed");
        return -1;
    }

    float* in_data = (float*)nimcp_tensor_data(input_tensor);
    float* tgt_data = (float*)nimcp_tensor_data(target_tensor);
    if (in_data && tgt_data) {
        memcpy(in_data, inputs, num_inputs * sizeof(float));
        memcpy(tgt_data, targets, num_targets * sizeof(float));
    }

    // Zero gradients
    cnn_trainer_zero_grad(trainer);

    // Forward pass
    cnn_forward_result_t fwd_result = {0};
    nimcp_error_t err = cnn_trainer_forward(trainer, input_tensor, &fwd_result);
    if (err != NIMCP_OK) {
        nimcp_tensor_destroy(input_tensor);
        nimcp_tensor_destroy(target_tensor);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "cnn_train_step: validation failed");
        return -1;
    }

    // Compute loss from output tensor and targets
    float loss = 0.0F;
    if (fwd_result.output) {
        float* out_data = (float*)nimcp_tensor_data(fwd_result.output);
        if (out_data && tgt_data) {
            for (uint32_t i = 0; i < num_targets; i++) {
                float diff = out_data[i] - tgt_data[i];
                loss += diff * diff;
            }
            loss /= (float)num_targets;
        }
    }

    // Backward pass
    err = cnn_trainer_backward(trainer, target_tensor, &fwd_result);
    if (err != NIMCP_OK) {
        nimcp_tensor_destroy(input_tensor);
        nimcp_tensor_destroy(target_tensor);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "cnn_train_step: validation failed");
        return -1;
    }

    // Optimizer step
    err = cnn_trainer_step(trainer);

    nimcp_tensor_destroy(input_tensor);
    nimcp_tensor_destroy(target_tensor);

    if (result) {
        result->loss = loss;
    }

    return (err == NIMCP_OK) ? 0 : -1;
}

//=============================================================================
// Public API Implementation
//=============================================================================

int training_dispatch_init(brain_t brain, const nimcp_training_config_t* config) {
    if (!brain || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "training_dispatch_init: required parameter is NULL (brain, config)");
        return -1;
    }

    brain->active_network_type = config->network_type;

    switch (config->network_type) {
        case NIMCP_NETWORK_ADAPTIVE:
            // Adaptive uses standard backprop, no special init needed
            NIMCP_LOGGING_INFO("Training dispatch: ADAPTIVE (standard backprop)");
            return 0;

        case NIMCP_NETWORK_SNN:
            return init_snn_training(brain, config);

        case NIMCP_NETWORK_LNN:
            return init_lnn_training(brain, config);

        case NIMCP_NETWORK_CNN:
            return init_cnn_training(brain, config);

        case NIMCP_NETWORK_HYBRID:
            // Initialize all available network types
            NIMCP_LOGGING_INFO("Training dispatch: HYBRID mode");
            if (brain->snn_network) init_snn_training(brain, config);
            if (brain->lnn_network) init_lnn_training(brain, config);
            if (brain->cnn_trainer) init_cnn_training(brain, config);
            return 0;

        default:
            NIMCP_LOGGING_ERROR("Unknown network type: %d", config->network_type);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "training_dispatch_init: validation failed");
            return -1;
    }
}

int training_dispatch_step(
    brain_t brain,
    const float* inputs,
    uint32_t num_inputs,
    const float* targets,
    uint32_t num_targets,
    training_dispatch_result_t* result)
{
    if (!brain || !inputs || !targets) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "training_dispatch_step: required parameter is NULL (brain, inputs, targets)");
        return -1;
    }

    /* Phase 8: Send heartbeat at start of training step */
    training_heartbeat(brain, "training_step", 0.0f);

    // Initialize result
    if (result) {
        memset(result, 0, sizeof(*result));
    }

    // Dispatch based on active network type
    switch (brain->active_network_type) {
        case NIMCP_NETWORK_ADAPTIVE:
            // Return -2 to signal caller should use standard backprop path
            // This maintains backward compatibility
            return -2;

        case NIMCP_NETWORK_SNN:
            return training_dispatch_snn_step(brain, inputs, num_inputs, targets, num_targets, result);

        case NIMCP_NETWORK_LNN:
            return training_dispatch_lnn_step(brain, inputs, num_inputs, targets, num_targets, result);

        case NIMCP_NETWORK_CNN:
            return cnn_train_step(brain, inputs, num_inputs, targets, num_targets, result);

        case NIMCP_NETWORK_HYBRID:
            // Train all available networks
            // Use whichever has lowest loss for final result
            // Non-fatal: if any network fails, skip it and continue
            {
                training_dispatch_result_t snn_res = {0}, lnn_res = {0}, cnn_res = {0};
                float min_loss = INFINITY;

                if (brain->snn_network && brain->snn_training_ctx) {
                    int rc = training_dispatch_snn_step(brain, inputs, num_inputs, targets, num_targets, &snn_res);
                    if (rc == 0 && snn_res.loss < min_loss) {
                        min_loss = snn_res.loss;
                        if (result) *result = snn_res;
                    }
                }

                if (brain->lnn_network && brain->lnn_training_ctx) {
                    int rc = training_dispatch_lnn_step(brain, inputs, num_inputs, targets, num_targets, &lnn_res);
                    if (rc == 0 && lnn_res.loss < min_loss) {
                        min_loss = lnn_res.loss;
                        if (result) *result = lnn_res;
                    }
                }

                if (brain->cnn_trainer) {
                    int rc = cnn_train_step(brain, inputs, num_inputs, targets, num_targets, &cnn_res);
                    if (rc == 0 && cnn_res.loss < min_loss) {
                        if (result) *result = cnn_res;
                    }
                }

                return 0;
            }

        default:
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "training_dispatch_step: validation failed");
            return -1;
    }
}

int training_dispatch_set_reward(brain_t brain, float reward) {
    if (!brain) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain is NULL");

        return -1;

    }

    if (brain->active_network_type == NIMCP_NETWORK_SNN && brain->snn_training_ctx) {
        snn_rstdp_set_reward(brain->snn_training_ctx, reward);
        return 0;
    }

    return -1;  // Not SNN or no training context
}

int training_dispatch_get_stats(
    brain_t brain,
    uint64_t* total_steps,
    float* total_loss,
    float* current_lr)
{
    if (!brain) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain is NULL");

        return -1;

    }

    switch (brain->active_network_type) {
        case NIMCP_NETWORK_SNN:
            if (brain->snn_training_ctx) {
                uint64_t weight_updates = 0, training_steps = 0;
                float total_delta_w = 0.0F;
                snn_training_get_stats(brain->snn_training_ctx,
                    &weight_updates, &training_steps, &total_delta_w);
                if (total_steps) *total_steps = training_steps;
                if (total_loss) *total_loss = 0.0F;  // SNN doesn't track loss same way
                if (current_lr) {
                    /* Read LR from SNN training context or network config */
                    if (brain->snn_training_ctx) {
                        *current_lr = brain->snn_network->config.learning_rate;
                    } else {
                        *current_lr = 0.0F;
                    }
                }
                return 0;
            }
            break;

        case NIMCP_NETWORK_LNN:
            if (brain->lnn_training_ctx) {
                if (total_steps) *total_steps = lnn_training_get_step_count(brain->lnn_training_ctx);
                if (total_loss) *total_loss = lnn_training_get_current_loss(brain->lnn_training_ctx);
                if (current_lr) *current_lr = lnn_training_get_lr(brain->lnn_training_ctx);
                return 0;
            }
            break;

        case NIMCP_NETWORK_CNN:
            // CNN stats would come from trainer
            break;

        default:
            break;
    }

    return -1;
}

int training_dispatch_reset(brain_t brain) {
    if (!brain) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain is NULL");

        return -1;

    }

    switch (brain->active_network_type) {
        case NIMCP_NETWORK_SNN:
            if (brain->snn_training_ctx) {
                snn_training_reset(brain->snn_training_ctx);
                snn_training_reset_stats(brain->snn_training_ctx);
            }
            break;

        case NIMCP_NETWORK_LNN:
            if (brain->lnn_training_ctx) {
                lnn_training_reset_stats(brain->lnn_training_ctx);
            }
            break;

        default:
            break;
    }

    return 0;
}

void training_dispatch_destroy(brain_t brain) {
    if (!brain) return;

    if (brain->snn_training_ctx) {
        snn_training_destroy(brain->snn_training_ctx);
        brain->snn_training_ctx = NULL;
    }

    if (brain->lnn_training_ctx) {
        lnn_training_destroy(brain->lnn_training_ctx);
        brain->lnn_training_ctx = NULL;
    }

    // CNN trainer is destroyed with the brain's CNN network
}

const char* training_dispatch_type_name(uint8_t network_type) {
    switch (network_type) {
        case NIMCP_NETWORK_ADAPTIVE: return "ADAPTIVE";
        case NIMCP_NETWORK_SNN:      return "SNN";
        case NIMCP_NETWORK_LNN:      return "LNN";
        case NIMCP_NETWORK_CNN:      return "CNN";
        case NIMCP_NETWORK_HYBRID:   return "HYBRID";
        default:                     return "UNKNOWN";
    }
}

bool training_dispatch_is_supported(uint8_t network_type) {
    switch (network_type) {
        case NIMCP_NETWORK_ADAPTIVE:
        case NIMCP_NETWORK_SNN:
        case NIMCP_NETWORK_LNN:
        case NIMCP_NETWORK_CNN:
        case NIMCP_NETWORK_HYBRID:
            return true;
        default:
            return false;
    }
}
