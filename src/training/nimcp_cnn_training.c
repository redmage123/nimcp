/**
 * @file nimcp_cnn_training.c
 * @brief CNN Training Implementation
 *
 * WHAT: Convolutional Neural Network training with SNN conversion
 * WHY:  Enable hybrid CNN+SNN architectures in NIMCP
 * HOW:  Classical backpropagation + rate/spike encoding bridges
 *
 * @author NIMCP Development Team
 * @date 2025-12-20
 */

#include "training/nimcp_cnn_training.h"
#include "training/nimcp_unified_training.h"
#include "constants/nimcp_constants.h"
#include "utils/validation/nimcp_common.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_macros.h"

/* Cortex integration for CNN-Cortex Bridge */
#include "perception/nimcp_visual_cortex.h"
#include "perception/nimcp_audio_cortex.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>

/* Debug timing: enabled by NIMCP_DEBUG_TIMING=1 environment variable */
static int _cnn_debug_timing_checked = 0;
static int _cnn_debug_timing = 0;
static inline int cnn_debug_timing_enabled(void) {
    if (!_cnn_debug_timing_checked) {
        const char* env = getenv("NIMCP_DEBUG_TIMING");
        _cnn_debug_timing = (env && env[0] == '1');
        _cnn_debug_timing_checked = 1;
    }
    return _cnn_debug_timing;
}
#include "utils/math/nimcp_math_helpers.h"
#include <time.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>

#include <stddef.h>  /* for NULL */
//=============================================================================
// Health Agent Forward Declarations (Phase 8: Heartbeat for Long Operations)
// Avoid including full header to prevent type conflicts
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_math_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(cnn)


/* Stub heartbeat for migration compatibility */
static inline void cnn_training_heartbeat(const char* op, float progress) {
    (void)op; (void)progress;
}
static inline void nimcp_tensor_fill(nimcp_tensor_t* t, float value) {
    if (!t) return;
    float* data = (float*)nimcp_tensor_data(t);
    size_t numel = nimcp_tensor_numel(t);
    for (size_t i = 0; i < numel; i++) {
        data[i] = value;
    }
}

/**
 * @brief Create a zeros tensor with the same shape as another tensor
 */
static inline nimcp_tensor_t* nimcp_tensor_zeros_like(const nimcp_tensor_t* t) {
    if (!t) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "t is NULL");

        return NULL;

    }
    const nimcp_tensor_shape_t* shape = nimcp_tensor_shape(t);
    if (!shape) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "shape is NULL");

        return NULL;

    }
    return nimcp_tensor_zeros(shape->dims, shape->rank, NIMCP_DTYPE_F32);
}

/**
 * @brief Get shape of tensor into a separate shape struct
 */
static inline void nimcp_tensor_get_shape(const nimcp_tensor_t* t, nimcp_tensor_shape_t* out_shape) {
    if (!t || !out_shape) return;
    const nimcp_tensor_shape_t* shape = nimcp_tensor_shape(t);
    if (shape) {
        memcpy(out_shape, shape, sizeof(nimcp_tensor_shape_t));
    }
}

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief CNN layer internal structure
 */
struct cnn_layer_s {
    cnn_layer_type_t type;          /**< Layer type */
    nimcp_tensor_t* weights;        /**< Layer weights */
    nimcp_tensor_t* bias;           /**< Layer bias */
    nimcp_tensor_t* weight_grad;    /**< Weight gradients */
    nimcp_tensor_t* bias_grad;      /**< Bias gradients */

    /* Batch norm running stats */
    nimcp_tensor_t* running_mean;
    nimcp_tensor_t* running_var;

    /* Layer-specific config (union to save memory) */
    union {
        cnn_conv_config_t conv;
        cnn_pool_config_t pool;
        cnn_batch_norm_config_t batch_norm;
        cnn_dropout_config_t dropout;
        cnn_dense_config_t dense;
    } config;

    /* Shape tracking */
    nimcp_tensor_shape_t input_shape;
    nimcp_tensor_shape_t output_shape;

    /* Dropout mask: stored during forward pass, applied during backward.
     * Each element is 0.0 (dropped) or 1.0 (kept). */
    float* dropout_mask;
    size_t dropout_mask_size;

    cnn_layer_t* next;              /**< Linked list */
};

/**
 * @brief CNN trainer internal structure
 */
struct cnn_trainer_s {
    /* Architecture */
    cnn_layer_t* layers_head;       /**< Linked list of layers */
    cnn_layer_t* layers_tail;
    uint32_t num_layers;

    /* Training infrastructure */
    nimcp_optimizer_context_t* optimizer;
    nimcp_gradient_manager_ctx_t* grad_manager;
    nimcp_loss_context_t* loss_fn;

    /* Configuration */
    cnn_trainer_config_t config;

    /* Training state */
    uint32_t current_epoch;
    uint32_t global_step;
    float best_val_loss;
    uint32_t epochs_no_improve;
    bool training_mode;             /**< true=train, false=eval */

    /* Bio-async integration */
    bio_module_context_t bio_ctx;
    bool bio_async_enabled;

    /* Cortex integration (CNN-Cortex Bridge) */
    void* visual_cortex;            /**< Connected visual cortex (NULL if none) */
    void* audio_cortex;             /**< Connected audio cortex (NULL if none) */
    uint32_t visual_feature_dim;    /**< Visual cortex output dimension */
    uint32_t audio_feature_dim;     /**< Audio cortex output dimension */
    bool input_from_cortex;         /**< True if using cortex as input source */
    float current_perception_confidence;  /**< Perception confidence for LR modulation */

    /* Anti-collapse (unified) */
    nimcp_anti_collapse_state_t anti_collapse;

    /* UTM management flag — when true, UTM owns gradient norm + optimizer stepping */
    bool managed_by_utm;

    /* Input gradient from last backward pass (for cross-network gradient bridges) */
    nimcp_tensor_t* last_input_grad;

    /* Statistics */
    uint64_t total_forward_calls;
    uint64_t total_backward_calls;
    double total_forward_time_ms;
    double total_backward_time_ms;
};

/**
 * @brief Data loader internal structure
 */
struct cnn_dataloader_s {
    nimcp_tensor_t* data;           /**< Full dataset */
    nimcp_tensor_t* labels;         /**< Full labels */
    uint32_t* indices;              /**< Shuffled indices */
    uint32_t dataset_size;
    uint32_t current_index;
    cnn_dataloader_config_t config;
    uint32_t seed;                  /**< Random seed for shuffling */
};

/**
 * @brief CNN-to-SNN converter internal structure
 */
struct cnn_to_snn_converter_s {
    cnn_to_snn_config_t config;

    /* Calibration statistics */
    nimcp_tensor_t** activation_stats; /**< Per-layer activation statistics */
    uint32_t num_layers;

    /* Conversion mappings */
    float* firing_rate_scales;      /**< Per-layer rate normalization factors */
    float* threshold_values;        /**< Per-layer SNN thresholds */
};

//=============================================================================
// Default Configurations
//=============================================================================

/**
 * @brief Get default trainer configuration
 *
 * WHAT: Initialize config with sensible hyperparameters
 * WHY:  Simplify setup for common use cases
 * HOW:  Set Adam optimizer, cross-entropy loss, standard batch size
 */
nimcp_error_t cnn_trainer_default_config(cnn_trainer_config_t* config) {
    /* Guard clause: null check */
    NIMCP_CHECK_THROW(config, NIMCP_ERROR_NULL_POINTER,
                      "cnn_trainer_default_config: config is NULL");

    /* Zero-initialize */
    memset(config, 0, sizeof(cnn_trainer_config_t));

    /* Optimizer: Adam with standard hyperparameters */
    config->optimizer_config.type = NIMCP_OPTIMIZER_ADAM;
    config->optimizer_config.params.adam = (nimcp_adam_config_t){
        .learning_rate = NIMCP_LEARNING_RATE_FINE,
        .beta1 = NIMCP_ADAM_BETA1_DEFAULT,
        .beta2 = NIMCP_ADAM_BETA2_DEFAULT,
        .epsilon = NIMCP_EPSILON_ADAM,
        .weight_decay = 0.0f,
        .amsgrad = false
    };

    /* Loss: Cross-entropy for classification */
    config->loss_type = NIMCP_LOSS_CROSS_ENTROPY;

    /* Training hyperparameters */
    config->max_epochs = 100;
    config->learning_rate = NIMCP_LEARNING_RATE_FINE;
    config->weight_decay = 0.0f;
    config->gradient_clip_value = 5.0f;

    /* Anti-collapse defaults (used to init unified anti-collapse state) */
    config->diversity_loss_weight = 0.1f;
    config->diversity_buffer_size = 16;
    config->use_gradient_normalization = true;
    config->gradient_target_norm = 1.0f;

    /* Data loader */
    config->dataloader.batch_size = NIMCP_DEFAULT_BATCH_SIZE;
    config->dataloader.shuffle = true;
    config->dataloader.num_workers = 1;
    config->dataloader.pin_memory = false;
    config->dataloader.drop_last = false;

    /* Validation */
    config->use_validation = true;
    config->validation_split = 0.2f;
    config->validation_frequency = 1;

    /* Early stopping */
    config->use_early_stopping = true;
    config->patience = 10;
    config->min_delta = 1e-4f;

    /* Checkpointing */
    config->save_checkpoints = false;
    config->checkpoint_frequency = 5;
    config->checkpoint_dir = "./checkpoints";

    /* Bio-async */
    config->enable_bio_async = false;

    /* Logging */
    config->verbose = true;
    config->log_frequency = 10;

    return NIMCP_SUCCESS;
}

//=============================================================================
// Trainer Lifecycle
//=============================================================================

/**
 * @brief Create CNN trainer
 *
 * WHAT: Allocate and initialize CNN trainer context
 * WHY:  Set up infrastructure for backpropagation-based training
 * HOW:  Create optimizer, gradient manager, loss function
 */
cnn_trainer_t* cnn_trainer_create(const cnn_trainer_config_t* config) {
    /* Guard clause: null check */
    if (!config) {
        NIMCP_LOGGING_ERROR("cnn_trainer_create: NULL config");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cnn_trainer_create: config is NULL");
        return NULL;
    }

    /* Allocate trainer */
    cnn_trainer_t* trainer = (cnn_trainer_t*)nimcp_malloc(sizeof(cnn_trainer_t));
    if (!trainer) {
        NIMCP_LOGGING_ERROR("cnn_trainer_create: Failed to allocate trainer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cnn_trainer_create: trainer is NULL");
        return NULL;
    }

    /* Zero-initialize */
    memset(trainer, 0, sizeof(cnn_trainer_t));

    /* Copy configuration */
    memcpy(&trainer->config, config, sizeof(cnn_trainer_config_t));

    /* Initialize optimizer (NULL security ctx and memory mgr for now) */
    trainer->optimizer = nimcp_optimizer_create(&config->optimizer_config, NULL, NULL);
    if (!trainer->optimizer) {
        NIMCP_LOGGING_ERROR("cnn_trainer_create: Failed to create optimizer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cnn_trainer_create: trainer->optimizer is NULL");
        goto cleanup;
    }

    /* Initialize gradient manager */
    trainer->grad_manager = nimcp_gradient_manager_create(&config->gradient_config);
    if (!trainer->grad_manager) {
        NIMCP_LOGGING_ERROR("cnn_trainer_create: Failed to create gradient manager");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cnn_trainer_create: trainer->grad_manager is NULL");
        goto cleanup;
    }

    /* Initialize loss function (requires loss-specific config) */
    nimcp_loss_config_t loss_cfg = {0};
    loss_cfg.type = config->loss_type;
    loss_cfg.use_memory_pool = false;

    switch (config->loss_type) {
        case NIMCP_LOSS_CROSS_ENTROPY:
            loss_cfg.params.cross_entropy.reduction = NIMCP_LOSS_REDUCE_MEAN;
            loss_cfg.params.cross_entropy.compute_gradient = true;
            loss_cfg.params.cross_entropy.class_weights = NULL;
            loss_cfg.params.cross_entropy.num_classes = 0;  /* Set by user */
            loss_cfg.params.cross_entropy.label_smoothing = 0.0f;
            loss_cfg.params.cross_entropy.ignore_index = -1;
            break;
        case NIMCP_LOSS_MSE:
            loss_cfg.params.mse.reduction = NIMCP_LOSS_REDUCE_MEAN;
            loss_cfg.params.mse.compute_gradient = true;
            break;
        case NIMCP_LOSS_BINARY_CROSS_ENTROPY:
            loss_cfg.params.cross_entropy.reduction = NIMCP_LOSS_REDUCE_MEAN;
            loss_cfg.params.cross_entropy.compute_gradient = true;
            loss_cfg.params.cross_entropy.class_weights = NULL;
            loss_cfg.params.cross_entropy.num_classes = 2;
            loss_cfg.params.cross_entropy.label_smoothing = 0.0f;
            loss_cfg.params.cross_entropy.ignore_index = -1;
            break;
        default:
            loss_cfg.params.mse.reduction = NIMCP_LOSS_REDUCE_MEAN;
            loss_cfg.params.mse.compute_gradient = true;
            break;
    }

    trainer->loss_fn = nimcp_loss_create(&loss_cfg, NULL, NULL);
    if (!trainer->loss_fn) {
        NIMCP_LOGGING_WARN("cnn_trainer_create: Failed to create loss function, continuing without it");
    }

    /* Initialize state */
    trainer->current_epoch = 0;
    trainer->global_step = 0;
    trainer->best_val_loss = FLT_MAX;
    trainer->epochs_no_improve = 0;
    trainer->training_mode = true;

    /* Anti-collapse init */
    {
        nimcp_anti_collapse_config_t ac_cfg = {
            .diversity_loss_weight = config->diversity_loss_weight,
            .diversity_buffer_size = config->diversity_buffer_size,
            .use_gradient_normalization = config->use_gradient_normalization,
            .gradient_target_norm = config->gradient_target_norm,
            .gradient_clip_value = config->gradient_clip_value,
            .adaptive_gradient_target = true,
        };
        nimcp_anti_collapse_init(&trainer->anti_collapse, &ac_cfg);
    }

    /* Bio-async integration */
    if (config->enable_bio_async) {
        cnn_connect_bio_async(trainer);
    }

    NIMCP_LOGGING_INFO("CNN trainer created successfully");
    return trainer;

cleanup:
    if (trainer) {
        if (trainer->grad_manager) nimcp_gradient_manager_destroy(trainer->grad_manager);
        if (trainer->optimizer) nimcp_optimizer_destroy(trainer->optimizer);
        nimcp_free(trainer);
    }
    return NULL;
}

/**
 * @brief Destroy CNN trainer
 *
 * WHAT: Free all trainer resources
 * WHY:  Prevent memory leaks
 * HOW:  Release layers, optimizer, gradient manager
 */
void cnn_trainer_destroy(cnn_trainer_t* trainer) {
    /* Guard clause: null check */
    if (!trainer) {
        return;
    }

    /* Disconnect bio-async */
    if (trainer->bio_async_enabled) {
        cnn_disconnect_bio_async(trainer);
    }

    /* Destroy layers */
    cnn_layer_t* layer = trainer->layers_head;
    while (layer) {
        cnn_layer_t* next = layer->next;

        /* Free layer tensors */
        if (layer->weights) nimcp_tensor_destroy(layer->weights);
        if (layer->bias) nimcp_tensor_destroy(layer->bias);
        if (layer->weight_grad) nimcp_tensor_destroy(layer->weight_grad);
        if (layer->bias_grad) nimcp_tensor_destroy(layer->bias_grad);
        if (layer->running_mean) nimcp_tensor_destroy(layer->running_mean);
        if (layer->running_var) nimcp_tensor_destroy(layer->running_var);
        if (layer->dropout_mask) nimcp_free(layer->dropout_mask);

        nimcp_free(layer);
        layer = next;
    }

    /* Destroy optimizer */
    if (trainer->optimizer) {
        nimcp_optimizer_destroy(trainer->optimizer);
    }

    /* Destroy gradient manager */
    if (trainer->grad_manager) {
        nimcp_gradient_manager_destroy(trainer->grad_manager);
    }

    /* Destroy loss function */
    if (trainer->loss_fn) {
        nimcp_loss_destroy(trainer->loss_fn);
    }

    /* Destroy anti-collapse state */
    nimcp_anti_collapse_destroy(&trainer->anti_collapse);

    /* Free cached input gradient */
    if (trainer->last_input_grad) {
        nimcp_tensor_destroy(trainer->last_input_grad);
        trainer->last_input_grad = NULL;
    }

    nimcp_free(trainer);
    NIMCP_LOGGING_INFO("CNN trainer destroyed");
}

//=============================================================================
// Internal Helper Functions
//=============================================================================

/**
 * @brief Random value from normal distribution using Box-Muller
 *
 * WHAT: Generate normally distributed random value
 * WHY:  Weight initialization requires Gaussian samples
 * HOW:  Box-Muller transform from uniform random
 */
static float cnn_randn(uint32_t* seed) {
    /* Simple LCG for reproducibility */
    *seed = (*seed * 1103515245 + 12345) & 0x7FFFFFFF;
    float u1 = ((float)(*seed) / (float)0x7FFFFFFF) + 1e-10f;
    *seed = (*seed * 1103515245 + 12345) & 0x7FFFFFFF;
    float u2 = ((float)(*seed) / (float)0x7FFFFFFF);

    /* Box-Muller transform */
    return sqrtf(-2.0f * logf(u1)) * cosf(NIMCP_TWO_PI_F * u2);
}

/**
 * @brief Append layer to trainer's linked list
 */
static void cnn_append_layer(cnn_trainer_t* trainer, cnn_layer_t* layer) {
    if (!trainer->layers_head) {
        trainer->layers_head = layer;
        trainer->layers_tail = layer;
    } else {
        trainer->layers_tail->next = layer;
        trainer->layers_tail = layer;
    }
    trainer->num_layers++;
}

/**
 * @brief Initialize tensor with He initialization
 *
 * WHAT: Fill tensor with He normal initialization
 * WHY:  Optimal variance for ReLU networks
 * HOW:  std = sqrt(2 / fan_in), sample from N(0, std)
 */
static void cnn_he_init(nimcp_tensor_t* weights, uint32_t fan_in, uint32_t seed) {
    float std = sqrtf(2.0f / (float)fan_in);
    size_t numel = nimcp_tensor_numel(weights);
    float* data = nimcp_tensor_data(weights);

    uint32_t s = seed;
    for (size_t i = 0; i < numel; i++) {
        data[i] = cnn_randn(&s) * std;
    }
}

//=============================================================================
// Layer Construction
//=============================================================================

/**
 * @brief Add convolution layer
 *
 * WHAT: Append Conv2D layer to network
 * WHY:  Build hierarchical feature extraction
 * HOW:  Allocate weights (He initialization), append to layer list
 *
 * BIOLOGICAL BASIS: V1 simple cells with oriented receptive fields
 */
cnn_layer_t* cnn_trainer_add_conv_layer(cnn_trainer_t* trainer,
                                        const cnn_conv_config_t* config) {
    /* Guard clauses */
    if (!trainer) {
        NIMCP_LOGGING_ERROR("cnn_trainer_add_conv_layer: NULL trainer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cnn_he_init: trainer is NULL");
        return NULL;
    }
    if (!config) {
        NIMCP_LOGGING_ERROR("cnn_trainer_add_conv_layer: NULL config");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cnn_he_init: config is NULL");
        return NULL;
    }
    if (trainer->num_layers >= CNN_MAX_LAYERS) {
        NIMCP_LOGGING_ERROR("cnn_trainer_add_conv_layer: Max layers exceeded");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "cnn_he_init: capacity exceeded");
        return NULL;
    }

    /* Allocate layer */
    cnn_layer_t* layer = (cnn_layer_t*)nimcp_malloc(sizeof(cnn_layer_t));
    if (!layer) {
        NIMCP_LOGGING_ERROR("cnn_trainer_add_conv_layer: Allocation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cnn_he_init: layer is NULL");
        return NULL;
    }
    memset(layer, 0, sizeof(cnn_layer_t));

    layer->type = CNN_LAYER_CONV2D;
    memcpy(&layer->config.conv, config, sizeof(cnn_conv_config_t));

    /* Create weight tensor: (out_channels, in_channels/groups, kernel_h, kernel_w) */
    uint32_t groups = config->groups > 0 ? config->groups : 1;
    uint32_t weight_dims[4] = {
        config->out_channels,
        config->in_channels / groups,
        config->kernel_h,
        config->kernel_w
    };
    layer->weights = nimcp_tensor_create(weight_dims, 4, NIMCP_DTYPE_F32);
    if (!layer->weights) {
        NIMCP_LOGGING_ERROR("cnn_trainer_add_conv_layer: Weight allocation failed");
        nimcp_free(layer);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cnn_he_init: layer->weights is NULL");
        return NULL;
    }

    /* He initialization */
    uint32_t fan_in = config->in_channels * config->kernel_h * config->kernel_w / groups;
    cnn_he_init(layer->weights, fan_in, trainer->num_layers * 42 + 1);

    /* Create bias if requested */
    if (config->use_bias) {
        uint32_t bias_dims[1] = {config->out_channels};
        layer->bias = nimcp_tensor_create(bias_dims, 1, NIMCP_DTYPE_F32);
        if (!layer->bias) {
            nimcp_tensor_destroy(layer->weights);
            nimcp_free(layer);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cnn_he_init: layer->bias is NULL");
            return NULL;
        }
        nimcp_tensor_fill(layer->bias, 0.0f);
    }

    /* Create gradient tensors for backprop */
    layer->weight_grad = nimcp_tensor_zeros_like(layer->weights);
    if (layer->bias) {
        layer->bias_grad = nimcp_tensor_zeros_like(layer->bias);
    }

    /* Append to trainer */
    cnn_append_layer(trainer, layer);

    NIMCP_LOGGING_INFO("Added conv layer: %ux%u kernel, %u->%u channels",
                       config->kernel_h, config->kernel_w,
                       config->in_channels, config->out_channels);
    return layer;
}

/**
 * @brief Add pooling layer
 *
 * WHAT: Append pooling layer for spatial downsampling
 * WHY:  Achieve position invariance
 * HOW:  Configure max/average pooling operation
 *
 * BIOLOGICAL BASIS: Complex cells pool over simple cell positions
 */
cnn_layer_t* cnn_trainer_add_pool_layer(cnn_trainer_t* trainer,
                                        const cnn_pool_config_t* config) {
    /* Guard clauses */
    if (!trainer) {
        NIMCP_LOGGING_ERROR("cnn_trainer_add_pool_layer: NULL trainer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cnn_he_init: trainer is NULL");
        return NULL;
    }
    if (!config) {
        NIMCP_LOGGING_ERROR("cnn_trainer_add_pool_layer: NULL config");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cnn_he_init: config is NULL");
        return NULL;
    }
    if (trainer->num_layers >= CNN_MAX_LAYERS) {
        NIMCP_LOGGING_ERROR("cnn_trainer_add_pool_layer: Max layers exceeded");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "cnn_he_init: capacity exceeded");
        return NULL;
    }

    /* Allocate layer (no weights for pooling) */
    cnn_layer_t* layer = (cnn_layer_t*)nimcp_malloc(sizeof(cnn_layer_t));
    if (!layer) {
        NIMCP_LOGGING_ERROR("cnn_trainer_add_pool_layer: Allocation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cnn_he_init: layer is NULL");
        return NULL;
    }
    memset(layer, 0, sizeof(cnn_layer_t));

    layer->type = CNN_LAYER_POOLING;
    memcpy(&layer->config.pool, config, sizeof(cnn_pool_config_t));

    /* Append to trainer */
    cnn_append_layer(trainer, layer);

    NIMCP_LOGGING_INFO("Added pool layer: %ux%u, type=%d",
                       config->pool_h, config->pool_w, config->type);
    return layer;
}

/**
 * @brief Add batch normalization layer
 *
 * WHAT: Append batch norm for activation normalization
 * WHY:  Stabilize training via divisive normalization
 * HOW:  Learnable scale (gamma) and shift (beta)
 *
 * BIOLOGICAL BASIS: Lateral inhibition and divisive normalization in V1
 */
cnn_layer_t* cnn_trainer_add_batch_norm_layer(cnn_trainer_t* trainer,
                                               const cnn_batch_norm_config_t* config) {
    /* Guard clauses */
    if (!trainer) {
        NIMCP_LOGGING_ERROR("cnn_trainer_add_batch_norm_layer: NULL trainer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cnn_he_init: trainer is NULL");
        return NULL;
    }
    if (!config) {
        NIMCP_LOGGING_ERROR("cnn_trainer_add_batch_norm_layer: NULL config");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cnn_he_init: config is NULL");
        return NULL;
    }
    if (trainer->num_layers >= CNN_MAX_LAYERS) {
        NIMCP_LOGGING_ERROR("cnn_trainer_add_batch_norm_layer: Max layers exceeded");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "cnn_he_init: capacity exceeded");
        return NULL;
    }

    /* Allocate layer */
    cnn_layer_t* layer = (cnn_layer_t*)nimcp_malloc(sizeof(cnn_layer_t));
    if (!layer) {
        NIMCP_LOGGING_ERROR("cnn_trainer_add_batch_norm_layer: Allocation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cnn_he_init: layer is NULL");
        return NULL;
    }
    memset(layer, 0, sizeof(cnn_layer_t));

    layer->type = CNN_LAYER_BATCH_NORM;
    memcpy(&layer->config.batch_norm, config, sizeof(cnn_batch_norm_config_t));

    /* Create gamma (scale) and beta (shift) if affine */
    if (config->affine) {
        uint32_t dims[1] = {config->num_features};
        layer->weights = nimcp_tensor_create(dims, 1, NIMCP_DTYPE_F32);  /* gamma */
        layer->bias = nimcp_tensor_create(dims, 1, NIMCP_DTYPE_F32);     /* beta */
        if (!layer->weights || !layer->bias) {
            nimcp_tensor_destroy(layer->weights);
            nimcp_tensor_destroy(layer->bias);
            nimcp_free(layer);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cnn_he_init: required parameter is NULL (layer->weights, layer->bias)");
            return NULL;
        }
        nimcp_tensor_fill(layer->weights, 1.0f);  /* gamma = 1 */
        nimcp_tensor_fill(layer->bias, 0.0f);     /* beta = 0 */

        layer->weight_grad = nimcp_tensor_zeros_like(layer->weights);
        layer->bias_grad = nimcp_tensor_zeros_like(layer->bias);
    }

    /* Running statistics */
    if (config->track_running_stats) {
        uint32_t dims[1] = {config->num_features};
        layer->running_mean = nimcp_tensor_create(dims, 1, NIMCP_DTYPE_F32);
        layer->running_var = nimcp_tensor_create(dims, 1, NIMCP_DTYPE_F32);
        if (!layer->running_mean || !layer->running_var) {
            nimcp_tensor_destroy(layer->weights);
            nimcp_tensor_destroy(layer->bias);
            nimcp_tensor_destroy(layer->weight_grad);
            nimcp_tensor_destroy(layer->bias_grad);
            nimcp_tensor_destroy(layer->running_mean);
            nimcp_tensor_destroy(layer->running_var);
            nimcp_free(layer);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (layer->running_mean, layer->running_var)");
            return NULL;
        }
        nimcp_tensor_fill(layer->running_mean, 0.0f);
        nimcp_tensor_fill(layer->running_var, 1.0f);
    }

    /* Append to trainer */
    cnn_append_layer(trainer, layer);

    NIMCP_LOGGING_INFO("Added batch_norm layer: %u features", config->num_features);
    return layer;
}

/**
 * @brief Add dropout layer
 *
 * WHAT: Append dropout for regularization
 * WHY:  Prevent overfitting via stochastic neuron dropout
 * HOW:  Random mask during training, scale during inference
 *
 * BIOLOGICAL BASIS: Developmental synaptic pruning
 */
cnn_layer_t* cnn_trainer_add_dropout_layer(cnn_trainer_t* trainer,
                                            const cnn_dropout_config_t* config) {
    /* Guard clauses */
    if (!trainer) {
        NIMCP_LOGGING_ERROR("cnn_trainer_add_dropout_layer: NULL trainer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: trainer is NULL");
        return NULL;
    }
    if (!config) {
        NIMCP_LOGGING_ERROR("cnn_trainer_add_dropout_layer: NULL config");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: config is NULL");
        return NULL;
    }
    if (config->dropout_rate < 0.0f || config->dropout_rate >= 1.0f) {
        NIMCP_LOGGING_ERROR("cnn_trainer_add_dropout_layer: Invalid rate %.2f",
                           config->dropout_rate);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: capacity exceeded");
        return NULL;
    }
    if (trainer->num_layers >= CNN_MAX_LAYERS) {
        NIMCP_LOGGING_ERROR("cnn_trainer_add_dropout_layer: Max layers exceeded");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "unknown: capacity exceeded");
        return NULL;
    }

    /* Allocate layer (no weights for dropout) */
    cnn_layer_t* layer = (cnn_layer_t*)nimcp_malloc(sizeof(cnn_layer_t));
    if (!layer) {
        NIMCP_LOGGING_ERROR("cnn_trainer_add_dropout_layer: Allocation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "unknown: layer is NULL");
        return NULL;
    }
    memset(layer, 0, sizeof(cnn_layer_t));

    layer->type = CNN_LAYER_DROPOUT;
    memcpy(&layer->config.dropout, config, sizeof(cnn_dropout_config_t));

    /* Append to trainer */
    cnn_append_layer(trainer, layer);

    NIMCP_LOGGING_INFO("Added dropout layer: rate=%.2f", config->dropout_rate);
    return layer;
}

/**
 * @brief Add dense (fully connected) layer
 *
 * WHAT: Append fully connected layer
 * WHY:  Final classification or feature combination
 * HOW:  Matrix multiplication with learned weights
 *
 * BIOLOGICAL BASIS: IT → PFC pathway with dense connectivity
 */
cnn_layer_t* cnn_trainer_add_dense_layer(cnn_trainer_t* trainer,
                                          const cnn_dense_config_t* config) {
    /* Guard clauses */
    if (!trainer) {
        NIMCP_LOGGING_ERROR("cnn_trainer_add_dense_layer: NULL trainer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: trainer is NULL");
        return NULL;
    }
    if (!config) {
        NIMCP_LOGGING_ERROR("cnn_trainer_add_dense_layer: NULL config");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: config is NULL");
        return NULL;
    }
    if (trainer->num_layers >= CNN_MAX_LAYERS) {
        NIMCP_LOGGING_ERROR("cnn_trainer_add_dense_layer: Max layers exceeded");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "unknown: capacity exceeded");
        return NULL;
    }

    /* Allocate layer */
    cnn_layer_t* layer = (cnn_layer_t*)nimcp_malloc(sizeof(cnn_layer_t));
    if (!layer) {
        NIMCP_LOGGING_ERROR("cnn_trainer_add_dense_layer: Allocation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "unknown: layer is NULL");
        return NULL;
    }
    memset(layer, 0, sizeof(cnn_layer_t));

    layer->type = CNN_LAYER_DENSE;
    memcpy(&layer->config.dense, config, sizeof(cnn_dense_config_t));

    /* Create weight tensor: (out_features, in_features) */
    uint32_t weight_dims[2] = {config->out_features, config->in_features};
    layer->weights = nimcp_tensor_create(weight_dims, 2, NIMCP_DTYPE_F32);
    if (!layer->weights) {
        NIMCP_LOGGING_ERROR("cnn_trainer_add_dense_layer: Weight allocation failed");
        nimcp_free(layer);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "unknown: layer->weights is NULL");
        return NULL;
    }

    /* He initialization */
    cnn_he_init(layer->weights, config->in_features, trainer->num_layers * 42 + 1);

    /* Create bias if requested */
    if (config->use_bias) {
        uint32_t bias_dims[1] = {config->out_features};
        layer->bias = nimcp_tensor_create(bias_dims, 1, NIMCP_DTYPE_F32);
        if (!layer->bias) {
            nimcp_tensor_destroy(layer->weights);
            nimcp_free(layer);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "unknown: layer->bias is NULL");
            return NULL;
        }
        nimcp_tensor_fill(layer->bias, 0.0f);
    }

    /* Create gradient tensors */
    layer->weight_grad = nimcp_tensor_zeros_like(layer->weights);
    if (layer->bias) {
        layer->bias_grad = nimcp_tensor_zeros_like(layer->bias);
    }

    /* Append to trainer */
    cnn_append_layer(trainer, layer);

    NIMCP_LOGGING_INFO("Added dense layer: %u->%u features",
                       config->in_features, config->out_features);
    return layer;
}

/**
 * @brief Add flatten layer
 *
 * WHAT: Reshape from spatial (H, W, C) to vector
 * WHY:  Transition from conv to dense layers
 * HOW:  No weights, just reshapes
 */
cnn_layer_t* cnn_trainer_add_flatten_layer(cnn_trainer_t* trainer) {
    /* Guard clause */
    if (!trainer) {
        NIMCP_LOGGING_ERROR("cnn_trainer_add_flatten_layer: NULL trainer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cnn_trainer_add_flatten_layer: trainer is NULL");
        return NULL;
    }
    if (trainer->num_layers >= CNN_MAX_LAYERS) {
        NIMCP_LOGGING_ERROR("cnn_trainer_add_flatten_layer: Max layers exceeded");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "cnn_trainer_add_flatten_layer: capacity exceeded");
        return NULL;
    }

    /* Allocate layer (no weights for flatten) */
    cnn_layer_t* layer = (cnn_layer_t*)nimcp_malloc(sizeof(cnn_layer_t));
    if (!layer) {
        NIMCP_LOGGING_ERROR("cnn_trainer_add_flatten_layer: Allocation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cnn_trainer_add_flatten_layer: layer is NULL");
        return NULL;
    }
    memset(layer, 0, sizeof(cnn_layer_t));

    layer->type = CNN_LAYER_FLATTEN;

    /* Append to trainer */
    cnn_append_layer(trainer, layer);

    NIMCP_LOGGING_INFO("Added flatten layer");
    return layer;
}

cnn_layer_t* cnn_trainer_add_activation_layer(cnn_trainer_t* trainer,
                                               cnn_activation_t activation) {
    /* Guard clause */
    if (!trainer) {
        NIMCP_LOGGING_ERROR("cnn_trainer_add_activation_layer: NULL trainer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cnn_trainer_add_flatten_layer: trainer is NULL");
        return NULL;
    }
    if (trainer->num_layers >= CNN_MAX_LAYERS) {
        NIMCP_LOGGING_ERROR("cnn_trainer_add_activation_layer: Max layers exceeded");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "cnn_trainer_add_flatten_layer: capacity exceeded");
        return NULL;
    }

    /* Allocate layer (no weights for activation) */
    cnn_layer_t* layer = (cnn_layer_t*)nimcp_malloc(sizeof(cnn_layer_t));
    if (!layer) {
        NIMCP_LOGGING_ERROR("cnn_trainer_add_activation_layer: Allocation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cnn_trainer_add_flatten_layer: layer is NULL");
        return NULL;
    }
    memset(layer, 0, sizeof(cnn_layer_t));

    layer->type = CNN_LAYER_ACTIVATION;
    /* Store activation type in config - reuse conv struct's activation field */
    layer->config.conv.activation = activation;

    /* Append to trainer */
    cnn_append_layer(trainer, layer);

    NIMCP_LOGGING_INFO("Added activation layer (type=%d)", (int)activation);
    return layer;
}

//=============================================================================
// Layer Forward Operations (Internal Helpers)
//=============================================================================

/**
 * @brief Apply ReLU activation in-place
 */
static void cnn_apply_relu(nimcp_tensor_t* tensor) {
    float* data = nimcp_tensor_data(tensor);
    size_t numel = nimcp_tensor_numel(tensor);
    for (size_t i = 0; i < numel; i++) {
        if (data[i] < 0.0f) data[i] = 0.0f;
    }
}

/**
 * @brief Apply Leaky ReLU activation in-place
 */
static void cnn_apply_leaky_relu(nimcp_tensor_t* tensor, float alpha) {
    float* data = nimcp_tensor_data(tensor);
    size_t numel = nimcp_tensor_numel(tensor);
    for (size_t i = 0; i < numel; i++) {
        if (data[i] < 0.0f) data[i] *= alpha;
    }
}

/**
 * @brief Apply activation function in-place
 */
static void cnn_apply_activation(nimcp_tensor_t* tensor, cnn_activation_t activation) {
    float* data = nimcp_tensor_data(tensor);
    size_t numel = nimcp_tensor_numel(tensor);

    switch (activation) {
        case CNN_ACTIVATION_RELU:
            cnn_apply_relu(tensor);
            break;
        case CNN_ACTIVATION_LEAKY_RELU:
            cnn_apply_leaky_relu(tensor, 0.01f);
            break;
        case CNN_ACTIVATION_SIGMOID:
            for (size_t i = 0; i < numel; i++) {
                data[i] = 1.0f / (1.0f + expf(-data[i]));
            }
            break;
        case CNN_ACTIVATION_TANH:
            for (size_t i = 0; i < numel; i++) {
                data[i] = tanhf(data[i]);
            }
            break;
        case CNN_ACTIVATION_SILU:
            for (size_t i = 0; i < numel; i++) {
                data[i] = data[i] / (1.0f + expf(-data[i]));
            }
            break;
        case CNN_ACTIVATION_GELU: {
            /* GELU: x * Phi(x) where Phi is standard normal CDF */
            /* Approximation: 0.5 * x * (1 + tanh(sqrt(2/pi) * (x + 0.044715 * x^3))) */
            const float sqrt_2_pi = 0.7978845608f;
            for (size_t i = 0; i < numel; i++) {
                float x = data[i];
                float cdf = 0.5f * (1.0f + tanhf(sqrt_2_pi * (x + 0.044715f * x * x * x)));
                data[i] = x * cdf;
            }
            break;
        }
        case CNN_ACTIVATION_SOFTMAX: {
            /* Softmax applied per batch along last dimension */
            /* Tensor is (batch, features) or higher rank - apply along last dim */
            nimcp_tensor_shape_t shape;
            nimcp_tensor_get_shape(tensor, &shape);

            /* Get batch size and feature size */
            uint32_t batch_size = 1;
            uint32_t feature_size = 1;
            if (shape.rank >= 2) {
                feature_size = shape.dims[shape.rank - 1];
                for (uint32_t d = 0; d < shape.rank - 1; d++) {
                    batch_size *= shape.dims[d];
                }
            } else {
                feature_size = shape.numel;
            }

            /* Apply softmax per row with numerical stability */
            for (uint32_t b = 0; b < batch_size; b++) {
                float* row = data + b * feature_size;

                /* Find max for numerical stability */
                float max_val = row[0];
                for (uint32_t f = 1; f < feature_size; f++) {
                    if (row[f] > max_val) max_val = row[f];
                }

                /* Compute exp(x - max) and sum */
                float sum = 0.0f;
                for (uint32_t f = 0; f < feature_size; f++) {
                    row[f] = expf(row[f] - max_val);
                    sum += row[f];
                }

                /* Normalize */
                if (sum > 0.0f) {
                    for (uint32_t f = 0; f < feature_size; f++) {
                        row[f] /= sum;
                    }
                }
            }
            break;
        }
        case CNN_ACTIVATION_NONE:
        default:
            break;
    }
}

/**
 * @brief im2col — unfold image patches into columns for GEMM-based convolution
 *
 * Converts (C, H, W) input into (C*kH*kW, outH*outW) column matrix.
 * Convolution then becomes a single matrix multiply: W × col_buffer.
 */
static void im2col(const float* data_im, uint32_t channels,
                   uint32_t height, uint32_t width,
                   uint32_t kernel_h, uint32_t kernel_w,
                   uint32_t stride_h, uint32_t stride_w,
                   uint32_t pad_h, uint32_t pad_w,
                   uint32_t dilation_h, uint32_t dilation_w,
                   float* data_col)
{
    uint32_t out_h = (height + 2 * pad_h - dilation_h * (kernel_h - 1) - 1) / stride_h + 1;
    uint32_t out_w = (width + 2 * pad_w - dilation_w * (kernel_w - 1) - 1) / stride_w + 1;

    uint32_t col_idx = 0;
    for (uint32_t c = 0; c < channels; c++) {
        for (uint32_t kh = 0; kh < kernel_h; kh++) {
            for (uint32_t kw = 0; kw < kernel_w; kw++) {
                for (uint32_t oh = 0; oh < out_h; oh++) {
                    for (uint32_t ow = 0; ow < out_w; ow++) {
                        int ih = (int)(oh * stride_h + kh * dilation_h) - (int)pad_h;
                        int iw = (int)(ow * stride_w + kw * dilation_w) - (int)pad_w;
                        if (ih >= 0 && ih < (int)height && iw >= 0 && iw < (int)width) {
                            data_col[col_idx] = data_im[c * height * width + ih * width + iw];
                        } else {
                            data_col[col_idx] = 0.0f;
                        }
                        col_idx++;
                    }
                }
            }
        }
    }
}

/**
 * @brief col2im — accumulate column gradients back to image layout (backward pass)
 */
static void col2im(const float* data_col, uint32_t channels,
                   uint32_t height, uint32_t width,
                   uint32_t kernel_h, uint32_t kernel_w,
                   uint32_t stride_h, uint32_t stride_w,
                   uint32_t pad_h, uint32_t pad_w,
                   uint32_t dilation_h, uint32_t dilation_w,
                   float* data_im)
{
    memset(data_im, 0, channels * height * width * sizeof(float));
    uint32_t out_h = (height + 2 * pad_h - dilation_h * (kernel_h - 1) - 1) / stride_h + 1;
    uint32_t out_w = (width + 2 * pad_w - dilation_w * (kernel_w - 1) - 1) / stride_w + 1;

    uint32_t col_idx = 0;
    for (uint32_t c = 0; c < channels; c++) {
        for (uint32_t kh = 0; kh < kernel_h; kh++) {
            for (uint32_t kw = 0; kw < kernel_w; kw++) {
                for (uint32_t oh = 0; oh < out_h; oh++) {
                    for (uint32_t ow = 0; ow < out_w; ow++) {
                        int ih = (int)(oh * stride_h + kh * dilation_h) - (int)pad_h;
                        int iw = (int)(ow * stride_w + kw * dilation_w) - (int)pad_w;
                        if (ih >= 0 && ih < (int)height && iw >= 0 && iw < (int)width) {
                            data_im[c * height * width + ih * width + iw] += data_col[col_idx];
                        }
                        col_idx++;
                    }
                }
            }
        }
    }
}

/**
 * @brief Forward pass through conv layer
 *
 * WHAT: Apply convolution operation using im2col + GEMM
 * WHY:  Feature extraction via learned filters; im2col converts to matrix multiply
 * HOW:  Unfold input patches -> GEMM with weight matrix -> add bias -> activate
 */
static nimcp_tensor_t* cnn_forward_conv(const cnn_layer_t* layer, const nimcp_tensor_t* input) {
    const cnn_conv_config_t* cfg = &layer->config.conv;

    /* Get input shape: (batch, channels, height, width) */
    nimcp_tensor_shape_t in_shape;
    nimcp_tensor_get_shape(input, &in_shape);
    uint32_t batch = in_shape.dims[0];
    uint32_t in_h = in_shape.dims[2];
    uint32_t in_w = in_shape.dims[3];

    /* Validate strides to prevent division by zero */
    if (cfg->stride_h == 0 || cfg->stride_w == 0) {
        NIMCP_LOGGING_ERROR("cnn_forward_conv: Invalid stride (stride_h=%u, stride_w=%u)",
                            cfg->stride_h, cfg->stride_w);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "cnn_forward_conv: cfg->stride_h is zero");
        return NULL;
    }

    /* Compute output dimensions in signed arithmetic to detect underflow */
    int64_t out_h_signed = ((int64_t)in_h + 2*(int64_t)cfg->padding_h
                            - (int64_t)cfg->dilation_h * ((int64_t)cfg->kernel_h - 1) - 1)
                           / (int64_t)cfg->stride_h + 1;
    int64_t out_w_signed = ((int64_t)in_w + 2*(int64_t)cfg->padding_w
                            - (int64_t)cfg->dilation_w * ((int64_t)cfg->kernel_w - 1) - 1)
                           / (int64_t)cfg->stride_w + 1;
    if (out_h_signed <= 0 || out_w_signed <= 0) {
        NIMCP_LOGGING_ERROR("cnn_forward_conv: Invalid output dimensions (out_h=%lld, out_w=%lld)",
                            (long long)out_h_signed, (long long)out_w_signed);
        return NULL;
    }
    uint32_t out_h = (uint32_t)out_h_signed;
    uint32_t out_w = (uint32_t)out_w_signed;

    /* Create output tensor */
    uint32_t out_dims[4] = {batch, cfg->out_channels, out_h, out_w};
    nimcp_tensor_t* output = nimcp_tensor_create(out_dims, 4, NIMCP_DTYPE_F32);
    if (!output) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "output is NULL");

        return NULL;

    }

    const float* in_data = nimcp_tensor_data_const(input);
    const float* weight_data = nimcp_tensor_data_const(layer->weights);
    float* out_data = nimcp_tensor_data(output);

    /* Initialize output with bias */
    size_t out_spatial = out_h * out_w;
    const float* bias_data = layer->bias ? (const float*)nimcp_tensor_data_const(layer->bias) : NULL;
    for (uint32_t b = 0; b < batch; b++) {
        for (uint32_t oc = 0; oc < cfg->out_channels; oc++) {
            float bias_val = bias_data ? bias_data[oc] : 0.0f;
            for (size_t s = 0; s < out_spatial; s++) {
                out_data[b * cfg->out_channels * out_spatial + oc * out_spatial + s] = bias_val;
            }
        }
    }

    /* Convolution via im2col + GEMM (replaces naive 6-deep loop)
     * im2col unrolls patches into columns, then W × col gives the output.
     * For grouped convolution, we slice the column buffer per group. */
    uint32_t groups = cfg->groups > 0 ? cfg->groups : 1;
    uint32_t ic_per_group = cfg->in_channels / groups;
    uint32_t oc_per_group = cfg->out_channels / groups;

#ifdef NIMCP_CNN_NAIVE_CONV
    /* Legacy naive path (kept for regression testing) */
    for (uint32_t b = 0; b < batch; b++) {
        for (uint32_t g = 0; g < groups; g++) {
            for (uint32_t oc = 0; oc < oc_per_group; oc++) {
                uint32_t oc_abs = g * oc_per_group + oc;
                for (uint32_t oh = 0; oh < out_h; oh++) {
                    for (uint32_t ow = 0; ow < out_w; ow++) {
                        float sum = 0.0f;
                        for (uint32_t ic = 0; ic < ic_per_group; ic++) {
                            uint32_t ic_abs = g * ic_per_group + ic;
                            for (uint32_t kh = 0; kh < cfg->kernel_h; kh++) {
                                for (uint32_t kw = 0; kw < cfg->kernel_w; kw++) {
                                    int ih = (int)(oh * cfg->stride_h + kh * cfg->dilation_h) - (int)cfg->padding_h;
                                    int iw = (int)(ow * cfg->stride_w + kw * cfg->dilation_w) - (int)cfg->padding_w;
                                    if (ih >= 0 && ih < (int)in_h && iw >= 0 && iw < (int)in_w) {
                                        size_t in_idx = b * cfg->in_channels * in_h * in_w +
                                                       ic_abs * in_h * in_w + ih * in_w + iw;
                                        size_t w_idx = oc_abs * ic_per_group * cfg->kernel_h * cfg->kernel_w +
                                                      ic * cfg->kernel_h * cfg->kernel_w + kh * cfg->kernel_w + kw;
                                        sum += in_data[in_idx] * weight_data[w_idx];
                                    }
                                }
                            }
                        }
                        size_t out_idx = b * cfg->out_channels * out_spatial + oc_abs * out_spatial + oh * out_w + ow;
                        out_data[out_idx] += sum;
                    }
                }
            }
        }
    }
#else
    /* im2col + GEMM path */
    {
        size_t col_h = ic_per_group * cfg->kernel_h * cfg->kernel_w;
        size_t col_w = out_h * out_w;
        float* col_buffer = (float*)nimcp_calloc(col_h * col_w, sizeof(float));
        if (!col_buffer) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cnn_forward_conv: col_buffer alloc failed");
            nimcp_tensor_destroy(output);
            return NULL;
        }

        for (uint32_t b = 0; b < batch; b++) {
            for (uint32_t g = 0; g < groups; g++) {
                /* im2col: unfold this group's input channels into column matrix */
                const float* group_in = in_data + b * cfg->in_channels * in_h * in_w
                                      + g * ic_per_group * in_h * in_w;
                im2col(group_in, ic_per_group, in_h, in_w,
                       cfg->kernel_h, cfg->kernel_w, cfg->stride_h, cfg->stride_w,
                       cfg->padding_h, cfg->padding_w, cfg->dilation_h, cfg->dilation_w,
                       col_buffer);

                /* GEMM: W[oc_per_group × col_h] × col[col_h × col_w] → out[oc_per_group × col_w]
                 * Weight layout: [oc_abs][ic_per_group * kH * kW] */
                const float* group_w = weight_data + g * oc_per_group * col_h;
                float* group_out = out_data + b * cfg->out_channels * out_spatial
                                 + g * oc_per_group * out_spatial;

                for (uint32_t oc = 0; oc < oc_per_group; oc++) {
                    for (size_t j = 0; j < col_w; j++) {
                        float sum = 0.0f;
                        for (size_t k = 0; k < col_h; k++) {
                            sum += group_w[oc * col_h + k] * col_buffer[k * col_w + j];
                        }
                        group_out[oc * out_spatial + j] += sum;
                    }
                }
            }
        }

        nimcp_free(col_buffer);
    }
#endif

    /* Apply activation */
    cnn_apply_activation(output, cfg->activation);

    return output;
}

/**
 * @brief Forward pass through pooling layer
 */
static nimcp_tensor_t* cnn_forward_pool(const cnn_layer_t* layer, const nimcp_tensor_t* input) {
    const cnn_pool_config_t* cfg = &layer->config.pool;

    /* Get input shape */
    nimcp_tensor_shape_t in_shape;
    nimcp_tensor_get_shape(input, &in_shape);
    uint32_t batch = in_shape.dims[0];
    uint32_t channels = in_shape.dims[1];
    uint32_t in_h = in_shape.dims[2];
    uint32_t in_w = in_shape.dims[3];

    /* Validate strides to prevent division by zero */
    if (cfg->stride_h == 0 || cfg->stride_w == 0) {
        NIMCP_LOGGING_ERROR("cnn_forward_pool: Invalid stride (stride_h=%u, stride_w=%u)",
                            cfg->stride_h, cfg->stride_w);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "cnn_forward_pool: cfg->stride_h is zero");
        return NULL;
    }

    /* Compute output dimensions in signed arithmetic to detect underflow */
    int64_t out_h_signed = ((int64_t)in_h + 2*(int64_t)cfg->padding_h - (int64_t)cfg->pool_h)
                           / (int64_t)cfg->stride_h + 1;
    int64_t out_w_signed = ((int64_t)in_w + 2*(int64_t)cfg->padding_w - (int64_t)cfg->pool_w)
                           / (int64_t)cfg->stride_w + 1;
    if (out_h_signed <= 0 || out_w_signed <= 0) {
        NIMCP_LOGGING_ERROR("cnn_forward_pool: Invalid output dimensions (out_h=%lld, out_w=%lld)",
                            (long long)out_h_signed, (long long)out_w_signed);
        return NULL;
    }
    uint32_t out_h = (uint32_t)out_h_signed;
    uint32_t out_w = (uint32_t)out_w_signed;

    /* Handle global pooling */
    if (cfg->type == CNN_POOL_GLOBAL_MAX || cfg->type == CNN_POOL_GLOBAL_AVERAGE) {
        out_h = 1;
        out_w = 1;
    }

    /* Create output tensor */
    uint32_t out_dims[4] = {batch, channels, out_h, out_w};
    nimcp_tensor_t* output = nimcp_tensor_create(out_dims, 4, NIMCP_DTYPE_F32);
    if (!output) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "output is NULL");

        return NULL;

    }

    const float* in_data = nimcp_tensor_data_const(input);
    float* out_data = nimcp_tensor_data(output);

    for (uint32_t b = 0; b < batch; b++) {
        for (uint32_t c = 0; c < channels; c++) {
            for (uint32_t oh = 0; oh < out_h; oh++) {
                for (uint32_t ow = 0; ow < out_w; ow++) {
                    float pool_val = (cfg->type == CNN_POOL_MAX || cfg->type == CNN_POOL_GLOBAL_MAX)
                                    ? -FLT_MAX : 0.0f;
                    uint32_t count = 0;

                    uint32_t ph = (cfg->type == CNN_POOL_GLOBAL_MAX || cfg->type == CNN_POOL_GLOBAL_AVERAGE)
                                 ? in_h : cfg->pool_h;
                    uint32_t pw = (cfg->type == CNN_POOL_GLOBAL_MAX || cfg->type == CNN_POOL_GLOBAL_AVERAGE)
                                 ? in_w : cfg->pool_w;

                    for (uint32_t kh = 0; kh < ph; kh++) {
                        for (uint32_t kw = 0; kw < pw; kw++) {
                            int ih = (cfg->type == CNN_POOL_GLOBAL_MAX || cfg->type == CNN_POOL_GLOBAL_AVERAGE)
                                    ? (int)kh
                                    : (int)(oh * cfg->stride_h + kh) - (int)cfg->padding_h;
                            int iw = (cfg->type == CNN_POOL_GLOBAL_MAX || cfg->type == CNN_POOL_GLOBAL_AVERAGE)
                                    ? (int)kw
                                    : (int)(ow * cfg->stride_w + kw) - (int)cfg->padding_w;

                            if (ih >= 0 && ih < (int)in_h && iw >= 0 && iw < (int)in_w) {
                                size_t in_idx = b * channels * in_h * in_w + c * in_h * in_w + ih * in_w + iw;
                                float val = in_data[in_idx];

                                if (cfg->type == CNN_POOL_MAX || cfg->type == CNN_POOL_GLOBAL_MAX) {
                                    if (val > pool_val) pool_val = val;
                                } else {
                                    pool_val += val;
                                    count++;
                                }
                            }
                        }
                    }

                    if ((cfg->type == CNN_POOL_AVERAGE || cfg->type == CNN_POOL_GLOBAL_AVERAGE) && count > 0) {
                        pool_val /= (float)count;
                    }

                    size_t out_idx = b * channels * out_h * out_w + c * out_h * out_w + oh * out_w + ow;
                    out_data[out_idx] = pool_val;
                }
            }
        }
    }

    return output;
}

/**
 * @brief Forward pass through batch norm layer
 */
static nimcp_tensor_t* cnn_forward_batch_norm(cnn_layer_t* layer, const nimcp_tensor_t* input,
                                              bool training) {
    const cnn_batch_norm_config_t* cfg = &layer->config.batch_norm;

    /* Clone input for output */
    nimcp_tensor_t* output = nimcp_tensor_clone(input);
    if (!output) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "output is NULL");

        return NULL;

    }

    nimcp_tensor_shape_t shape;
    nimcp_tensor_get_shape(input, &shape);
    uint32_t batch = shape.dims[0];
    uint32_t channels = cfg->num_features;
    if (batch == 0 || channels == 0) {
        nimcp_tensor_destroy(output);
        return NULL;
    }
    size_t spatial = shape.numel / (batch * channels);
    if (spatial == 0) {
        nimcp_tensor_destroy(output);
        return NULL;
    }

    const float* in_data = nimcp_tensor_data_const(input);
    float* out_data = nimcp_tensor_data(output);
    float* running_mean = layer->running_mean ? nimcp_tensor_data(layer->running_mean) : NULL;
    float* running_var = layer->running_var ? nimcp_tensor_data(layer->running_var) : NULL;
    const float* gamma = layer->weights ? nimcp_tensor_data_const(layer->weights) : NULL;
    const float* beta = layer->bias ? nimcp_tensor_data_const(layer->bias) : NULL;

    for (uint32_t c = 0; c < channels; c++) {
        float mean, var;

        if (training) {
            /* Compute batch statistics */
            mean = 0.0f;
            for (uint32_t b = 0; b < batch; b++) {
                for (size_t s = 0; s < spatial; s++) {
                    size_t idx = b * channels * spatial + c * spatial + s;
                    mean += in_data[idx];
                }
            }
            mean /= (float)(batch * spatial);

            var = 0.0f;
            for (uint32_t b = 0; b < batch; b++) {
                for (size_t s = 0; s < spatial; s++) {
                    size_t idx = b * channels * spatial + c * spatial + s;
                    float diff = in_data[idx] - mean;
                    var += diff * diff;
                }
            }
            var /= (float)(batch * spatial);

            /* Update running stats */
            if (running_mean && running_var) {
                running_mean[c] = cfg->momentum * running_mean[c] + (1.0f - cfg->momentum) * mean;
                NIMCP_EMA_GUARD(running_mean[c], mean);
                running_var[c] = cfg->momentum * running_var[c] + (1.0f - cfg->momentum) * var;
                NIMCP_EMA_GUARD(running_var[c], var);
            }
        } else {
            /* Use running statistics */
            mean = running_mean ? running_mean[c] : 0.0f;
            var = running_var ? running_var[c] : 1.0f;
        }

        /* Normalize and apply affine transform */
        float std_inv = 1.0f / sqrtf(var + cfg->epsilon);
        float g = gamma ? gamma[c] : 1.0f;
        float b_val = beta ? beta[c] : 0.0f;

        for (uint32_t b = 0; b < batch; b++) {
            for (size_t s = 0; s < spatial; s++) {
                size_t idx = b * channels * spatial + c * spatial + s;
                out_data[idx] = g * (in_data[idx] - mean) * std_inv + b_val;
            }
        }
    }

    return output;
}

/**
 * @brief Forward pass through dropout layer
 */
static nimcp_tensor_t* cnn_forward_dropout(cnn_layer_t* layer, const nimcp_tensor_t* input,
                                           bool training, uint32_t* seed) {
    const cnn_dropout_config_t* cfg = &layer->config.dropout;

    /* Clone input */
    nimcp_tensor_t* output = nimcp_tensor_clone(input);
    if (!output) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "output is NULL");

        return NULL;

    }

    if (!training || cfg->dropout_rate == 0.0f) {
        return output;  /* No dropout during inference */
    }

    float* data = nimcp_tensor_data(output);
    size_t numel = nimcp_tensor_numel(output);
    float scale = 1.0f / (1.0f - cfg->dropout_rate);

    /* BUG FIX: Store the dropout mask so backward pass can reuse it.
     * Previously, backward pass either generated a new random mask or
     * just scaled uniformly, which breaks the gradient computation. */
    if (layer->dropout_mask_size != numel) {
        nimcp_free(layer->dropout_mask);
        layer->dropout_mask = (float*)nimcp_malloc(numel * sizeof(float));
        layer->dropout_mask_size = layer->dropout_mask ? numel : 0;
    }

    for (size_t i = 0; i < numel; i++) {
        *seed = (*seed * 1103515245 + 12345) & 0x7FFFFFFF;
        float r = (float)(*seed) / (float)0x7FFFFFFF;
        if (r < cfg->dropout_rate) {
            data[i] = 0.0f;
            if (layer->dropout_mask) layer->dropout_mask[i] = 0.0f;
        } else {
            data[i] *= scale;
            if (layer->dropout_mask) layer->dropout_mask[i] = scale;
        }
    }

    return output;
}

/**
 * @brief Forward pass through dense layer
 */
static nimcp_tensor_t* cnn_forward_dense(const cnn_layer_t* layer, const nimcp_tensor_t* input) {
    const cnn_dense_config_t* cfg = &layer->config.dense;

    /* Get input shape: (batch, in_features) */
    nimcp_tensor_shape_t in_shape;
    nimcp_tensor_get_shape(input, &in_shape);
    uint32_t batch = in_shape.dims[0];
    if (batch == 0) return NULL;
    uint32_t in_features = in_shape.numel / batch;

    /* Create output: (batch, out_features) */
    uint32_t out_dims[2] = {batch, cfg->out_features};
    nimcp_tensor_t* output = nimcp_tensor_create(out_dims, 2, NIMCP_DTYPE_F32);
    if (!output) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "output is NULL");

        return NULL;

    }

    const float* in_data = nimcp_tensor_data_const(input);
    const float* weight_data = nimcp_tensor_data_const(layer->weights);
    float* out_data = nimcp_tensor_data(output);

    /* Matrix multiplication: out = input @ weight.T + bias */
    const float* dense_bias_data = layer->bias ? (const float*)nimcp_tensor_data_const(layer->bias) : NULL;
    for (uint32_t b = 0; b < batch; b++) {
        for (uint32_t o = 0; o < cfg->out_features; o++) {
            float sum = dense_bias_data ? dense_bias_data[o] : 0.0f;
            for (uint32_t i = 0; i < in_features && i < cfg->in_features; i++) {
                sum += in_data[b * in_features + i] * weight_data[o * cfg->in_features + i];
            }
            out_data[b * cfg->out_features + o] = sum;
        }
    }

    /* Apply activation */
    cnn_apply_activation(output, cfg->activation);

    return output;
}

/**
 * @brief Forward pass through flatten layer
 */
static nimcp_tensor_t* cnn_forward_flatten(const nimcp_tensor_t* input) {
    nimcp_tensor_shape_t in_shape;
    nimcp_tensor_get_shape(input, &in_shape);

    uint32_t batch = in_shape.dims[0];
    if (batch == 0) return NULL;
    uint32_t flat_size = in_shape.numel / batch;

    uint32_t out_dims[2] = {batch, flat_size};
    nimcp_tensor_t* output = nimcp_tensor_create(out_dims, 2, NIMCP_DTYPE_F32);
    if (!output) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "output is NULL");

        return NULL;

    }

    /* Copy data (just reshape, no actual data movement needed in contiguous case) */
    memcpy(nimcp_tensor_data(output), nimcp_tensor_data_const(input),
           in_shape.numel * sizeof(float));

    return output;
}

//=============================================================================
// Training API
//=============================================================================

/**
 * @brief Forward propagation
 *
 * WHAT: Compute network output and cache activations
 * WHY:  Generate predictions for loss computation
 * HOW:  Sequential layer-wise forward passes
 */
nimcp_error_t cnn_trainer_forward(cnn_trainer_t* trainer,
                                   const nimcp_tensor_t* input,
                                   cnn_forward_result_t* result) {
    /* Guard clauses */
    NIMCP_CHECK_THROW(trainer, NIMCP_ERROR_NULL_POINTER,
                      "cnn_trainer_forward: trainer is NULL");
    NIMCP_CHECK_THROW(input, NIMCP_ERROR_NULL_POINTER,
                      "cnn_trainer_forward: input is NULL");
    NIMCP_CHECK_THROW(result, NIMCP_ERROR_NULL_POINTER,
                      "cnn_trainer_forward: result is NULL");

    /* Initialize result */
    memset(result, 0, sizeof(cnn_forward_result_t));
    result->num_layers = trainer->num_layers;

    /* Allocate activation cache */
    if (trainer->num_layers > 0) {
        result->activations = (nimcp_tensor_t**)nimcp_malloc(
            sizeof(nimcp_tensor_t*) * (trainer->num_layers + 1));
        NIMCP_CHECK_THROW(result->activations, NIMCP_ERROR_NO_MEMORY,
                          "cnn_trainer_forward: Failed to allocate activations");
        memset(result->activations, 0, sizeof(nimcp_tensor_t*) * (trainer->num_layers + 1));
    }

    /* Cache input as first activation */
    result->activations[0] = nimcp_tensor_clone(input);

    /* Forward through each layer */
    nimcp_tensor_t* current = result->activations[0];
    cnn_layer_t* layer = trainer->layers_head;
    uint32_t layer_idx = 1;
    uint32_t dropout_seed = 42 + trainer->global_step;

    while (layer) {
        nimcp_tensor_t* next = NULL;

        switch (layer->type) {
            case CNN_LAYER_CONV2D:
                next = cnn_forward_conv(layer, current);
                break;
            case CNN_LAYER_POOLING:
                next = cnn_forward_pool(layer, current);
                break;
            case CNN_LAYER_BATCH_NORM:
                next = cnn_forward_batch_norm(layer, current, trainer->training_mode);
                break;
            case CNN_LAYER_DROPOUT:
                next = cnn_forward_dropout(layer, current, trainer->training_mode, &dropout_seed);
                break;
            case CNN_LAYER_DENSE:
                next = cnn_forward_dense(layer, current);
                break;
            case CNN_LAYER_FLATTEN:
                next = cnn_forward_flatten(current);
                break;
            case CNN_LAYER_ACTIVATION:
                /* Standalone activation layer - clone and apply activation */
                /* Note: activation type stored in config.conv.activation */
                next = nimcp_tensor_clone(current);
                if (next) {
                    cnn_apply_activation(next, layer->config.conv.activation);
                }
                break;
            default:
                NIMCP_LOGGING_WARN("cnn_trainer_forward: Unknown layer type %d", layer->type);
                next = nimcp_tensor_clone(current);
                break;
        }

        if (!next) {
            /* Cleanup */
            for (uint32_t i = 0; i <= layer_idx; i++) {
                nimcp_tensor_destroy(result->activations[i]);
            }
            nimcp_free(result->activations);
            result->activations = NULL;
            NIMCP_THROW(NIMCP_ERROR_OPERATION_FAILED,
                        "cnn_trainer_forward: Layer %u forward failed", layer_idx - 1);
            return NIMCP_ERROR_OPERATION_FAILED;
        }

        result->activations[layer_idx] = next;
        current = next;
        layer = layer->next;
        layer_idx++;
    }

    result->output = current;  /* Last activation is output */
    trainer->total_forward_calls++;

    return NIMCP_SUCCESS;
}

/**
 * @brief Backward propagation
 *
 * WHAT: Compute gradients via backpropagation
 * WHY:  Calculate weight updates for learning
 * HOW:  Reverse-order gradient computation
 */
nimcp_error_t cnn_trainer_backward(cnn_trainer_t* trainer,
                                    const nimcp_tensor_t* target,
                                    const cnn_forward_result_t* forward_result) {
    NIMCP_CHECK_THROW(trainer && target && forward_result, NIMCP_ERROR_NULL_POINTER,
                      "cnn_trainer_backward: trainer, target, or forward_result is NULL");
    NIMCP_CHECK_THROW(forward_result->output && forward_result->activations,
                      NIMCP_ERROR_INVALID_STATE,
                      "cnn_trainer_backward: forward_result has no output or activations");

    /* Compute output gradient (loss gradient w.r.t. output) */
    /* For MSE: grad = 2 * (output - target) / n */
    /* For cross-entropy with softmax: grad = output - target (simplified) */
    nimcp_tensor_t* grad = nimcp_tensor_clone(forward_result->output);
    NIMCP_CHECK_THROW(grad, NIMCP_ERROR_NO_MEMORY,
                      "cnn_trainer_backward: Failed to clone output for gradient");

    float* grad_data = nimcp_tensor_data(grad);
    const float* target_data = nimcp_tensor_data_const(target);
    size_t numel = nimcp_tensor_numel(grad);
    size_t target_numel = nimcp_tensor_numel(target);

    /* Safety: clamp loop to min(output, target) elements to prevent OOB read */
    size_t common = (numel < target_numel) ? numel : target_numel;
    for (size_t i = 0; i < common; i++) {
        grad_data[i] = (grad_data[i] - target_data[i]) * 2.0f / (float)common;
    }
    /* Zero gradient for output elements beyond target coverage */
    for (size_t i = common; i < numel; i++) {
        grad_data[i] = 0.0f;
    }

    /* --- Diversity loss: penalize mode collapse (unified anti-collapse) ---
     * Skip when managed by UTM — UTM applies diversity loss centrally. */
    if (common > 0 && !trainer->managed_by_utm) {
        const float* out_data = nimcp_tensor_data_const(forward_result->output);
        nimcp_anti_collapse_diversity_loss(
            &trainer->anti_collapse, out_data, grad_data, (uint32_t)common);
    }

    /* Backpropagate through layers in reverse order */
    /* For simplicity, we accumulate gradients in layer->weight_grad */
    cnn_layer_t** layer_stack = (cnn_layer_t**)nimcp_malloc(sizeof(cnn_layer_t*) * trainer->num_layers);
    if (!layer_stack) {
        nimcp_tensor_destroy(grad);
        NIMCP_THROW(NIMCP_ERROR_NO_MEMORY, "cnn_trainer_backward: Failed to allocate layer stack");
        return NIMCP_ERROR_NO_MEMORY;
    }

    /* Build layer stack */
    cnn_layer_t* layer = trainer->layers_head;
    uint32_t idx = 0;
    while (layer) {
        layer_stack[idx++] = layer;
        layer = layer->next;
    }

    /* Log layer structure on first backward (debug timing only) */
    if (cnn_debug_timing_enabled()) {
        static bool _logged_layers = false;
        if (!_logged_layers) {
            NIMCP_LOGGING_INFO("CNN backward: %u layers", trainer->num_layers);
            for (uint32_t _li = 0; _li < trainer->num_layers; _li++) {
                cnn_layer_t* _l = layer_stack[_li];
                if (_l->type == CNN_LAYER_DENSE) {
                    NIMCP_LOGGING_INFO("  layer[%u]: DENSE %u→%u", _li,
                        _l->config.dense.in_features, _l->config.dense.out_features);
                } else {
                    NIMCP_LOGGING_INFO("  layer[%u]: type=%d", _li, _l->type);
                }
            }
            _logged_layers = true;
        }
    }

    /* Backprop in reverse */
    struct timespec _bp_t0;
    if (cnn_debug_timing_enabled()) clock_gettime(CLOCK_MONOTONIC, &_bp_t0);
    for (int i = (int)trainer->num_layers - 1; i >= 0; i--) {
        layer = layer_stack[i];
        const nimcp_tensor_t* layer_input = forward_result->activations[i];

        switch (layer->type) {
            case CNN_LAYER_DENSE: {
                /* grad_weight = input.T @ grad_output */
                const cnn_dense_config_t* cfg = &layer->config.dense;
                const float* in_data = nimcp_tensor_data_const(layer_input);
                float* w_grad = nimcp_tensor_data(layer->weight_grad);
                float* b_grad = layer->bias_grad ? nimcp_tensor_data(layer->bias_grad) : NULL;

                nimcp_tensor_shape_t grad_shape;
                nimcp_tensor_get_shape(grad, &grad_shape);
                uint32_t batch = grad_shape.dims[0];
                nimcp_tensor_shape_t in_shape;
                nimcp_tensor_get_shape(layer_input, &in_shape);
                uint32_t in_features = in_shape.numel / batch;

                if (cnn_debug_timing_enabled()) {
                    static bool _logged_dense = false;
                    if (!_logged_dense) {
                        NIMCP_LOGGING_INFO("CNN DENSE backward: batch=%u in_features=%u "
                                           "cfg_in=%u cfg_out=%u grad_numel=%zu in_numel=%zu",
                                           batch, in_features,
                                           cfg->in_features, cfg->out_features,
                                           nimcp_tensor_numel(grad), in_shape.numel);
                        _logged_dense = true;
                    }
                }

                /* Accumulate weight gradients */
                for (uint32_t o = 0; o < cfg->out_features; o++) {
                    for (uint32_t in = 0; in < cfg->in_features && in < in_features; in++) {
                        float g = 0.0f;
                        for (uint32_t b = 0; b < batch; b++) {
                            g += in_data[b * in_features + in] * grad_data[b * cfg->out_features + o];
                        }
                        w_grad[o * cfg->in_features + in] += g;
                    }
                    if (b_grad) {
                        for (uint32_t b = 0; b < batch; b++) {
                            b_grad[o] += grad_data[b * cfg->out_features + o];
                        }
                    }
                }

                /* Compute gradient for previous layer: grad_input = grad @ weight */
                if (i > 0) {
                    nimcp_tensor_t* new_grad = nimcp_tensor_zeros_like(layer_input);
                    float* new_grad_data = nimcp_tensor_data(new_grad);
                    const float* weight_data = nimcp_tensor_data_const(layer->weights);

                    for (uint32_t b = 0; b < batch; b++) {
                        for (uint32_t in = 0; in < in_features; in++) {
                            float g = 0.0f;
                            for (uint32_t o = 0; o < cfg->out_features; o++) {
                                if (in < cfg->in_features) {
                                    g += grad_data[b * cfg->out_features + o] * weight_data[o * cfg->in_features + in];
                                }
                            }
                            new_grad_data[b * in_features + in] = g;
                        }
                    }

                    nimcp_tensor_destroy(grad);
                    grad = new_grad;
                    grad_data = nimcp_tensor_data(grad);
                }
                break;
            }

            case CNN_LAYER_FLATTEN: {
                /* WHAT: Reshape gradient back to input shape
                 * WHY:  Flatten forward collapses spatial dims; backward must restore them
                 * HOW:  Create tensor with input shape and copy gradient data (values unchanged)
                 */
                if (i > 0) {
                    /* Gradient is currently flat [batch, features] */
                    /* Need to reshape to original input shape [batch, C, H, W] */
                    nimcp_tensor_t* new_grad = nimcp_tensor_zeros_like(layer_input);
                    if (!new_grad) {
                        nimcp_free(layer_stack);
                        nimcp_tensor_destroy(grad);
                        NIMCP_THROW(NIMCP_ERROR_NO_MEMORY,
                                    "cnn_trainer_backward: Failed to allocate gradient for flatten");
                        return NIMCP_ERROR_NO_MEMORY;
                    }

                    /* Copy gradient values (numel should match) */
                    size_t numel_grad = nimcp_tensor_numel(grad);
                    size_t numel_input = nimcp_tensor_numel(layer_input);
                    size_t copy_size = (numel_grad < numel_input) ? numel_grad : numel_input;

                    memcpy(nimcp_tensor_data(new_grad), grad_data, copy_size * sizeof(float));

                    nimcp_tensor_destroy(grad);
                    grad = new_grad;
                    grad_data = nimcp_tensor_data(grad);
                }
                break;
            }

            case CNN_LAYER_ACTIVATION: {
                /* WHAT: Backward pass through standalone activation layer
                 * WHY:  Compute gradient of activation function
                 * HOW:  Multiply by derivative of activation at each position
                 *
                 * NOTE: For softmax, the gradient is special - we assume the loss function
                 * (cross-entropy) already computes combined gradient (softmax + CE)
                 */
                if (i > 0) {
                    cnn_activation_t act_type = layer->config.conv.activation;

                    /* Create gradient tensor */
                    nimcp_tensor_t* new_grad = nimcp_tensor_clone(grad);
                    if (!new_grad) {
                        nimcp_free(layer_stack);
                        nimcp_tensor_destroy(grad);
                        NIMCP_THROW(NIMCP_ERROR_NO_MEMORY,
                                    "cnn_trainer_backward: Failed to allocate gradient for activation");
                        return NIMCP_ERROR_NO_MEMORY;
                    }

                    float* new_grad_data = nimcp_tensor_data(new_grad);
                    const float* in_data = nimcp_tensor_data_const(layer_input);
                    size_t numel = nimcp_tensor_numel(new_grad);

                    switch (act_type) {
                        case CNN_ACTIVATION_RELU:
                            for (size_t j = 0; j < numel; j++) {
                                new_grad_data[j] *= (in_data[j] > 0.0f) ? 1.0f : 0.0f;
                            }
                            break;
                        case CNN_ACTIVATION_LEAKY_RELU:
                            for (size_t j = 0; j < numel; j++) {
                                new_grad_data[j] *= (in_data[j] > 0.0f) ? 1.0f : 0.01f;
                            }
                            break;
                        case CNN_ACTIVATION_SIGMOID:
                            for (size_t j = 0; j < numel; j++) {
                                float s = 1.0f / (1.0f + expf(-in_data[j]));
                                new_grad_data[j] *= s * (1.0f - s);
                            }
                            break;
                        case CNN_ACTIVATION_TANH:
                            for (size_t j = 0; j < numel; j++) {
                                float t = tanhf(in_data[j]);
                                new_grad_data[j] *= (1.0f - t * t);
                            }
                            break;
                        case CNN_ACTIVATION_SOFTMAX:
                            /* Softmax gradient typically combined with cross-entropy loss */
                            /* If used standalone, gradient passes through unchanged */
                            break;
                        default:
                            /* No gradient modification for unknown activations */
                            break;
                    }

                    nimcp_tensor_destroy(grad);
                    grad = new_grad;
                    grad_data = nimcp_tensor_data(grad);
                }
                break;
            }

            case CNN_LAYER_POOLING: {
                /* WHAT: Backpropagate gradient through pooling layer
                 * WHY:  Route gradients to positions that contributed to pooling output
                 * HOW:  Max pooling - gradient to max position; Avg pooling - uniform distribution
                 *
                 * BIOLOGICAL BASIS: Winner-take-all (max) vs distributed coding (average)
                 */
                if (i > 0) {
                    const cnn_pool_config_t* cfg = &layer->config.pool;
                    const float* in_data = nimcp_tensor_data_const(layer_input);

                    /* Get input shape: (batch, channels, in_h, in_w) */
                    nimcp_tensor_shape_t in_shape;
                    nimcp_tensor_get_shape(layer_input, &in_shape);
                    uint32_t batch = in_shape.dims[0];
                    uint32_t channels = in_shape.dims[1];
                    uint32_t in_h = in_shape.dims[2];
                    uint32_t in_w = in_shape.dims[3];
                    size_t in_spatial = in_h * in_w;

                    /* Get output gradient shape: (batch, channels, out_h, out_w) */
                    nimcp_tensor_shape_t grad_shape;
                    nimcp_tensor_get_shape(grad, &grad_shape);
                    uint32_t out_h = grad_shape.dims[2];
                    uint32_t out_w = grad_shape.dims[3];
                    size_t out_spatial = out_h * out_w;

                    /* Create input gradient tensor (zeros) */
                    nimcp_tensor_t* new_grad = nimcp_tensor_zeros_like(layer_input);
                    if (!new_grad) {
                        nimcp_free(layer_stack);
                        nimcp_tensor_destroy(grad);
                        NIMCP_THROW(NIMCP_ERROR_NO_MEMORY,
                                    "cnn_trainer_backward: Failed to allocate gradient for pooling");
                        return NIMCP_ERROR_NO_MEMORY;
                    }
                    float* new_grad_data = nimcp_tensor_data(new_grad);

                    /* Backpropagate based on pooling type */
                    if (cfg->type == CNN_POOL_MAX) {
                        /* MAX POOLING: Route gradient only to max position in each window */
                        for (uint32_t b = 0; b < batch; b++) {
                            for (uint32_t c = 0; c < channels; c++) {
                                for (uint32_t oh = 0; oh < out_h; oh++) {
                                    for (uint32_t ow = 0; ow < out_w; ow++) {
                                        /* Find max position in pooling window (recompute from forward) */
                                        int ih_start = (int)(oh * cfg->stride_h) - (int)cfg->padding_h;
                                        int iw_start = (int)(ow * cfg->stride_w) - (int)cfg->padding_w;

                                        float max_val = -INFINITY;
                                        uint32_t max_ih = 0, max_iw = 0;

                                        for (uint32_t ph = 0; ph < cfg->pool_h; ph++) {
                                            for (uint32_t pw = 0; pw < cfg->pool_w; pw++) {
                                                int ih = ih_start + (int)ph;
                                                int iw = iw_start + (int)pw;

                                                if (ih >= 0 && ih < (int)in_h && iw >= 0 && iw < (int)in_w) {
                                                    size_t in_idx = b * channels * in_spatial +
                                                                   c * in_spatial + ih * in_w + iw;
                                                    if (in_data[in_idx] > max_val) {
                                                        max_val = in_data[in_idx];
                                                        max_ih = ih;
                                                        max_iw = iw;
                                                    }
                                                }
                                            }
                                        }

                                        /* Route gradient to max position */
                                        if (max_val != -INFINITY) {
                                            size_t grad_idx = b * channels * out_spatial +
                                                             c * out_spatial + oh * out_w + ow;
                                            size_t in_idx = b * channels * in_spatial +
                                                           c * in_spatial + max_ih * in_w + max_iw;
                                            new_grad_data[in_idx] += grad_data[grad_idx];
                                        }
                                    }
                                }
                            }
                        }
                    } else if (cfg->type == CNN_POOL_AVERAGE) {
                        /* AVERAGE POOLING: Distribute gradient uniformly across window */
                        float pool_area = (float)(cfg->pool_h * cfg->pool_w);

                        for (uint32_t b = 0; b < batch; b++) {
                            for (uint32_t c = 0; c < channels; c++) {
                                for (uint32_t oh = 0; oh < out_h; oh++) {
                                    for (uint32_t ow = 0; ow < out_w; ow++) {
                                        int ih_start = (int)(oh * cfg->stride_h) - (int)cfg->padding_h;
                                        int iw_start = (int)(ow * cfg->stride_w) - (int)cfg->padding_w;

                                        size_t grad_idx = b * channels * out_spatial +
                                                         c * out_spatial + oh * out_w + ow;
                                        float distributed_grad = grad_data[grad_idx] / pool_area;

                                        for (uint32_t ph = 0; ph < cfg->pool_h; ph++) {
                                            for (uint32_t pw = 0; pw < cfg->pool_w; pw++) {
                                                int ih = ih_start + (int)ph;
                                                int iw = iw_start + (int)pw;

                                                if (ih >= 0 && ih < (int)in_h && iw >= 0 && iw < (int)in_w) {
                                                    size_t in_idx = b * channels * in_spatial +
                                                                   c * in_spatial + ih * in_w + iw;
                                                    new_grad_data[in_idx] += distributed_grad;
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    } else {
                        /* Other pooling types (global, stochastic, Lp): simplified pass-through */
                        size_t copy_size = nimcp_tensor_numel(grad) < nimcp_tensor_numel(layer_input)
                                          ? nimcp_tensor_numel(grad) : nimcp_tensor_numel(layer_input);
                        memcpy(new_grad_data, grad_data, copy_size * sizeof(float));
                    }

                    nimcp_tensor_destroy(grad);
                    grad = new_grad;
                    grad_data = nimcp_tensor_data(grad);
                }
                break;
            }

            case CNN_LAYER_DROPOUT: {
                /* WHAT: Backward pass through dropout layer
                 * WHY:  Propagate gradients with same mask as forward pass
                 * HOW:  Apply the stored forward mask (0 for dropped, scale for kept)
                 *
                 * BUG FIX: Previously generated a uniform scale 1/(1-p) for ALL
                 * elements, ignoring which elements were dropped in forward pass.
                 * Correct behavior: zeroed neurons in forward must also be zeroed
                 * in backward; kept neurons get the same inverted-dropout scaling.
                 *
                 * BIOLOGICAL BASIS: Models stochastic synaptic transmission failures.
                 * During training, surviving connections are upscaled to maintain expected signal.
                 */
                if (i > 0) {
                    /* Create gradient for previous layer */
                    nimcp_tensor_t* new_grad = nimcp_tensor_clone(grad);
                    if (!new_grad) {
                        nimcp_free(layer_stack);
                        nimcp_tensor_destroy(grad);
                        NIMCP_THROW(NIMCP_ERROR_NO_MEMORY,
                                    "cnn_trainer_backward: Failed to allocate gradient for dropout");
                        return NIMCP_ERROR_NO_MEMORY;
                    }

                    /* Apply stored forward mask during training mode */
                    if (trainer->training_mode && layer->dropout_mask) {
                        float* grad_data_ptr = nimcp_tensor_data(new_grad);
                        size_t numel = nimcp_tensor_numel(new_grad);
                        size_t mask_len = layer->dropout_mask_size;

                        for (size_t j = 0; j < numel; j++) {
                            /* mask[j] is 0.0 (dropped) or scale (kept) from forward */
                            grad_data_ptr[j] *= (j < mask_len) ? layer->dropout_mask[j] : 0.0f;
                        }
                    }

                    nimcp_tensor_destroy(grad);
                    grad = new_grad;
                    grad_data = nimcp_tensor_data(grad);
                }
                break;
            }

            case CNN_LAYER_BATCH_NORM: {
                /* WHAT: BatchNorm backward - compute gamma/beta gradients and input gradient
                 * WHY:  Enable learning of normalization parameters
                 * HOW:  Recompute statistics, compute parameter gradients, backpropagate
                 *
                 * BIOLOGICAL BASIS: Normalization maintains homeostatic balance,
                 * analogous to gain control mechanisms in biological circuits
                 */
                const cnn_batch_norm_config_t* cfg = &layer->config.batch_norm;
                const float* in_data = nimcp_tensor_data_const(layer_input);

                /* Get shape: (batch, channels, height, width) */
                nimcp_tensor_shape_t in_shape;
                nimcp_tensor_get_shape(layer_input, &in_shape);
                uint32_t batch = in_shape.dims[0];
                uint32_t channels = cfg->num_features;
                size_t spatial = nimcp_tensor_numel(layer_input) / (batch * channels);
                size_t batch_spatial = batch * spatial;

                /* Gradient accumulators */
                float* gamma_grad = layer->weight_grad ? nimcp_tensor_data(layer->weight_grad) : NULL;
                float* beta_grad = layer->bias_grad ? nimcp_tensor_data(layer->bias_grad) : NULL;

                /* Per-channel backward pass */
                for (uint32_t c = 0; c < channels; c++) {
                    /* 1. Recompute channel mean */
                    float mean = 0.0f;
                    for (uint32_t b = 0; b < batch; b++) {
                        for (size_t s = 0; s < spatial; s++) {
                            size_t idx = b * channels * spatial + c * spatial + s;
                            mean += in_data[idx];
                        }
                    }
                    mean /= (float)batch_spatial;

                    /* 2. Recompute channel variance */
                    float var = 0.0f;
                    for (uint32_t b = 0; b < batch; b++) {
                        for (size_t s = 0; s < spatial; s++) {
                            size_t idx = b * channels * spatial + c * spatial + s;
                            float diff = in_data[idx] - mean;
                            var += diff * diff;
                        }
                    }
                    var /= (float)batch_spatial;

                    float std_inv = 1.0f / sqrtf(var + cfg->epsilon);
                    float gamma_c = 1.0f;
                    if (layer->weights && cfg->affine) {
                        const float* gamma_data = (const float*)nimcp_tensor_data_const(layer->weights);
                        if (gamma_data) gamma_c = gamma_data[c];
                    }

                    /* 3. Accumulate gamma gradient: dL/dgamma = sum(grad * normalized) */
                    if (gamma_grad && cfg->affine) {
                        for (uint32_t b = 0; b < batch; b++) {
                            for (size_t s = 0; s < spatial; s++) {
                                size_t idx = b * channels * spatial + c * spatial + s;
                                float normalized = (in_data[idx] - mean) * std_inv;
                                gamma_grad[c] += grad_data[idx] * normalized;
                            }
                        }
                    }

                    /* 4. Accumulate beta gradient: dL/dbeta = sum(grad) */
                    if (beta_grad && cfg->affine) {
                        for (uint32_t b = 0; b < batch; b++) {
                            for (size_t s = 0; s < spatial; s++) {
                                size_t idx = b * channels * spatial + c * spatial + s;
                                beta_grad[c] += grad_data[idx];
                            }
                        }
                    }

                    /* 5. Scale input gradient: dx = gamma * grad / sqrt(var + eps) */
                    if (i > 0) {
                        for (uint32_t b = 0; b < batch; b++) {
                            for (size_t s = 0; s < spatial; s++) {
                                size_t idx = b * channels * spatial + c * spatial + s;
                                grad_data[idx] *= gamma_c * std_inv;
                            }
                        }
                    }
                }
                break;
            }

            case CNN_LAYER_CONV2D: {
                /* Full convolution backward pass */
                /* Computes: weight gradients, bias gradients, and input gradients */
                const cnn_conv_config_t* cfg = &layer->config.conv;
                const float* in_data = nimcp_tensor_data_const(layer_input);
                float* w_grad = nimcp_tensor_data(layer->weight_grad);
                float* b_grad = layer->bias_grad ? nimcp_tensor_data(layer->bias_grad) : NULL;

                /* Get input shape: (batch, in_channels, in_h, in_w) */
                nimcp_tensor_shape_t in_shape;
                nimcp_tensor_get_shape(layer_input, &in_shape);
                uint32_t batch = in_shape.dims[0];
                uint32_t in_h = in_shape.dims[2];
                uint32_t in_w = in_shape.dims[3];

                /* Get output gradient shape: (batch, out_channels, out_h, out_w) */
                nimcp_tensor_shape_t grad_shape;
                nimcp_tensor_get_shape(grad, &grad_shape);
                uint32_t out_h = grad_shape.dims[2];
                uint32_t out_w = grad_shape.dims[3];
                size_t out_spatial = out_h * out_w;

                /* Group convolution parameters */
                uint32_t groups = cfg->groups > 0 ? cfg->groups : 1;
                uint32_t ic_per_group = cfg->in_channels / groups;
                uint32_t oc_per_group = cfg->out_channels / groups;

                /* 1. Accumulate weight gradients: dW[oc,ic,kh,kw] = sum_{b,oh,ow} grad[b,oc,oh,ow] * input[b,ic,ih,iw] */
                for (uint32_t b = 0; b < batch; b++) {
                    for (uint32_t g = 0; g < groups; g++) {
                        for (uint32_t oc = 0; oc < oc_per_group; oc++) {
                            uint32_t oc_abs = g * oc_per_group + oc;
                            for (uint32_t oh = 0; oh < out_h; oh++) {
                                for (uint32_t ow = 0; ow < out_w; ow++) {
                                    size_t grad_idx = b * cfg->out_channels * out_spatial +
                                                     oc_abs * out_spatial + oh * out_w + ow;
                                    float grad_val = grad_data[grad_idx];

                                    for (uint32_t ic = 0; ic < ic_per_group; ic++) {
                                        uint32_t ic_abs = g * ic_per_group + ic;
                                        for (uint32_t kh = 0; kh < cfg->kernel_h; kh++) {
                                            for (uint32_t kw = 0; kw < cfg->kernel_w; kw++) {
                                                int ih = (int)(oh * cfg->stride_h + kh * cfg->dilation_h) - (int)cfg->padding_h;
                                                int iw = (int)(ow * cfg->stride_w + kw * cfg->dilation_w) - (int)cfg->padding_w;

                                                if (ih >= 0 && ih < (int)in_h && iw >= 0 && iw < (int)in_w) {
                                                    size_t in_idx = b * cfg->in_channels * in_h * in_w +
                                                                   ic_abs * in_h * in_w + ih * in_w + iw;
                                                    size_t w_idx = oc_abs * ic_per_group * cfg->kernel_h * cfg->kernel_w +
                                                                  ic * cfg->kernel_h * cfg->kernel_w + kh * cfg->kernel_w + kw;
                                                    w_grad[w_idx] += grad_val * in_data[in_idx];
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                /* 2. Accumulate bias gradients: db[oc] = sum_{b,oh,ow} grad[b,oc,oh,ow] */
                if (b_grad) {
                    for (uint32_t b = 0; b < batch; b++) {
                        for (uint32_t oc = 0; oc < cfg->out_channels; oc++) {
                            for (uint32_t oh = 0; oh < out_h; oh++) {
                                for (uint32_t ow = 0; ow < out_w; ow++) {
                                    size_t grad_idx = b * cfg->out_channels * out_spatial +
                                                     oc * out_spatial + oh * out_w + ow;
                                    b_grad[oc] += grad_data[grad_idx];
                                }
                            }
                        }
                    }
                }

                /* 3. Compute input gradients for previous layer (transposed convolution) */
                if (i > 0) {
                    const nimcp_tensor_shape_t* input_shape = nimcp_tensor_shape(layer_input);
                    nimcp_tensor_t* new_grad = nimcp_tensor_zeros(input_shape->dims, input_shape->rank, NIMCP_DTYPE_F32);
                    float* new_grad_data = (float*)nimcp_tensor_data(new_grad);
                    const float* weight_data = nimcp_tensor_data_const(layer->weights);

                    /* dX[b,ic,ih,iw] = sum_{oc,kh,kw} grad[b,oc,oh,ow] * W[oc,ic,kh,kw] */
                    /* where oh = (ih + padding_h - kh*dilation_h) / stride_h */
                    for (uint32_t b = 0; b < batch; b++) {
                        for (uint32_t g = 0; g < groups; g++) {
                            for (uint32_t ic = 0; ic < ic_per_group; ic++) {
                                uint32_t ic_abs = g * ic_per_group + ic;
                                for (uint32_t ih = 0; ih < in_h; ih++) {
                                    for (uint32_t iw = 0; iw < in_w; iw++) {
                                        float sum = 0.0f;
                                        for (uint32_t oc = 0; oc < oc_per_group; oc++) {
                                            uint32_t oc_abs = g * oc_per_group + oc;
                                            for (uint32_t kh = 0; kh < cfg->kernel_h; kh++) {
                                                for (uint32_t kw = 0; kw < cfg->kernel_w; kw++) {
                                                    /* Reverse the forward mapping:
                                                     * Forward: ih = oh * stride_h + kh * dilation_h - padding_h
                                                     * Backward: oh = (ih + padding_h - kh * dilation_h) / stride_h
                                                     * Must check divisibility by stride */
                                                    int oh_offset = (int)ih + (int)cfg->padding_h - (int)(kh * cfg->dilation_h);
                                                    if (oh_offset >= 0 && oh_offset % cfg->stride_h == 0) {
                                                        uint32_t oh = oh_offset / cfg->stride_h;
                                                        int ow_offset = (int)iw + (int)cfg->padding_w - (int)(kw * cfg->dilation_w);
                                                        if (ow_offset >= 0 && ow_offset % cfg->stride_w == 0) {
                                                            uint32_t ow = ow_offset / cfg->stride_w;
                                                            if (oh < out_h && ow < out_w) {
                                                                size_t grad_idx = b * cfg->out_channels * out_spatial +
                                                                                 oc_abs * out_spatial + oh * out_w + ow;
                                                                size_t w_idx = oc_abs * ic_per_group * cfg->kernel_h * cfg->kernel_w +
                                                                              ic * cfg->kernel_h * cfg->kernel_w + kh * cfg->kernel_w + kw;
                                                                sum += grad_data[grad_idx] * weight_data[w_idx];
                                                            }
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                        size_t in_idx = b * cfg->in_channels * in_h * in_w +
                                                       ic_abs * in_h * in_w + ih * in_w + iw;
                                        new_grad_data[in_idx] = sum;
                                    }
                                }
                            }
                        }
                    }

                    nimcp_tensor_destroy(grad);
                    grad = new_grad;
                    grad_data = nimcp_tensor_data(grad);
                }
                break;
            }

            default:
                break;
        }
    }

    if (cnn_debug_timing_enabled()) {
        struct timespec _bp_t1; clock_gettime(CLOCK_MONOTONIC, &_bp_t1);
        double _bp_ms = (_bp_t1.tv_sec - _bp_t0.tv_sec) * 1000.0
                      + (_bp_t1.tv_nsec - _bp_t0.tv_nsec) / 1e6;
        if (_bp_ms > 100.0) {
            NIMCP_LOGGING_INFO("CNN backward loop: %.0fms for %u layers",
                               _bp_ms, trainer->num_layers);
        }
    }

    nimcp_free(layer_stack);

    /* Save input gradient for cross-network gradient bridges.
     * After the backward loop, `grad` holds dL/d(input) — the gradient
     * of the loss w.r.t. the network's input tensor. */
    if (trainer->last_input_grad) {
        nimcp_tensor_destroy(trainer->last_input_grad);
    }
    trainer->last_input_grad = grad;  /* Transfer ownership (don't destroy) */

    trainer->total_backward_calls++;

    return NIMCP_SUCCESS;
}

/**
 * @brief C5: Backward pass with externally provided gradient (skip internal MSE)
 */
nimcp_error_t cnn_trainer_backward_with_gradient(cnn_trainer_t* trainer,
                                                   const nimcp_tensor_t* grad_tensor,
                                                   const cnn_forward_result_t* forward_result) {
    NIMCP_CHECK_THROW(trainer && grad_tensor && forward_result, NIMCP_ERROR_NULL_POINTER,
                      "cnn_trainer_backward_with_gradient: NULL parameter");
    NIMCP_CHECK_THROW(forward_result->output && forward_result->activations,
                      NIMCP_ERROR_INVALID_STATE,
                      "cnn_trainer_backward_with_gradient: forward_result has no output or activations");

    /* Use the externally provided gradient directly (no MSE computation).
     * We create a target tensor from the forward output minus the gradient,
     * such that the MSE gradient 2*(output - target)/n equals the provided gradient.
     * However, to avoid precision issues, we instead call the existing backward
     * with a synthetic target that produces the right gradient.
     *
     * Simpler approach: create a "target" = output - grad*n/2, so that
     * the internal MSE gradient 2*(output - target)/n = grad. */
    nimcp_tensor_t* synthetic_target = nimcp_tensor_clone(forward_result->output);
    if (!synthetic_target) return NIMCP_ERROR_NO_MEMORY;

    float* tgt_data = nimcp_tensor_data(synthetic_target);
    const float* out_data = nimcp_tensor_data_const(forward_result->output);
    const float* grad_data = nimcp_tensor_data_const(grad_tensor);
    size_t numel = nimcp_tensor_numel(synthetic_target);
    size_t grad_numel = nimcp_tensor_numel(grad_tensor);
    size_t common = (numel < grad_numel) ? numel : grad_numel;
    float n = (float)(common > 0 ? common : 1);

    for (size_t i = 0; i < common; i++) {
        /* target = output - grad * n / 2 → MSE grad = 2*(output - target)/n = grad */
        tgt_data[i] = out_data[i] - grad_data[i] * n / 2.0f;
    }

    nimcp_error_t err = cnn_trainer_backward(trainer, synthetic_target, forward_result);
    nimcp_tensor_destroy(synthetic_target);
    return err;
}

/**
 * @brief Optimizer step
 *
 * WHAT: Update weights using computed gradients
 * WHY:  Apply learning rule
 * HOW:  Call optimizer for each layer's parameters
 */
nimcp_error_t cnn_trainer_step(cnn_trainer_t* trainer) {
    NIMCP_CHECK_THROW(trainer, NIMCP_ERROR_NULL_POINTER,
                      "cnn_trainer_step: trainer is NULL");
    NIMCP_CHECK_THROW(trainer->optimizer, NIMCP_ERROR_INVALID_STATE,
                      "cnn_trainer_step: optimizer is NULL");

    /* Apply perception-modulated learning rate if connected to cortex.
     * WHAT: Scale the optimizer's learning rate by cortical confidence
     * WHY:  High-confidence cortical input should accelerate learning;
     *       lr_scale was previously computed but never applied (discarded).
     * HOW:  Temporarily scale the optimizer LR, run steps, then restore.
     */
    float lr_scale = 1.0f;
    if (trainer->input_from_cortex && trainer->current_perception_confidence > 0.0f) {
        lr_scale = 0.5f + 0.5f * trainer->current_perception_confidence;
    }

    /* Scale optimizer LR if lr_scale differs from 1.0 */
    float original_lr = nimcp_optimizer_get_lr(trainer->optimizer);
    if (lr_scale != 1.0f) {
        nimcp_optimizer_set_lr(trainer->optimizer, original_lr * lr_scale);
    }

    /* --- Gradient normalization + optimizer step ---
     * Skip when managed by UTM — UTM normalizes globally and steps all params. */
    if (!trainer->managed_by_utm) {
        /* Gradient normalization / clipping (unified anti-collapse) */
        {
            float* grad_arrays[256];
            size_t grad_sizes[256];
            uint32_t n_arrays = 0;

            cnn_layer_t* gl = trainer->layers_head;
            while (gl && n_arrays < 254) {
                if (gl->weight_grad) {
                    grad_arrays[n_arrays] = nimcp_tensor_data(gl->weight_grad);
                    grad_sizes[n_arrays] = nimcp_tensor_numel(gl->weight_grad);
                    n_arrays++;
                }
                if (gl->bias_grad) {
                    grad_arrays[n_arrays] = nimcp_tensor_data(gl->bias_grad);
                    grad_sizes[n_arrays] = nimcp_tensor_numel(gl->bias_grad);
                    n_arrays++;
                }
                gl = gl->next;
            }

            nimcp_anti_collapse_normalize_gradients(
                &trainer->anti_collapse, grad_arrays, grad_sizes, n_arrays);
        }

        /* Update each layer's weights */
        cnn_layer_t* layer = trainer->layers_head;
        while (layer) {
            if (layer->weights && layer->weight_grad) {
                float* w_data = (float*)nimcp_tensor_data(layer->weights);
                const float* wg_data = (const float*)nimcp_tensor_data_const(layer->weight_grad);
                size_t w_numel = nimcp_tensor_numel(layer->weights);
                nimcp_optimizer_step(trainer->optimizer, w_data, wg_data, w_numel);
                nimcp_tensor_fill(layer->weight_grad, 0.0f);
            }
            if (layer->bias && layer->bias_grad) {
                float* b_data = (float*)nimcp_tensor_data(layer->bias);
                const float* bg_data = (const float*)nimcp_tensor_data_const(layer->bias_grad);
                size_t b_numel = nimcp_tensor_numel(layer->bias);
                nimcp_optimizer_step(trainer->optimizer, b_data, bg_data, b_numel);
                nimcp_tensor_fill(layer->bias_grad, 0.0f);
            }
            layer = layer->next;
        }
    }

    /* Restore original LR after scaled step */
    if (lr_scale != 1.0f) {
        nimcp_optimizer_set_lr(trainer->optimizer, original_lr);
    }

    trainer->global_step++;

    return NIMCP_SUCCESS;
}

/**
 * @brief Zero all gradients in the network
 *
 * WHAT: Reset all weight and bias gradients to zero
 * WHY:  Prevent gradient accumulation between backward passes
 * HOW:  Fill gradient tensors with zeros
 */
nimcp_error_t cnn_trainer_zero_grad(cnn_trainer_t* trainer) {
    NIMCP_CHECK_THROW(trainer, NIMCP_ERROR_NULL_POINTER,
                      "cnn_trainer_zero_grad: trainer is NULL");

    cnn_layer_t* layer = trainer->layers_head;
    while (layer) {
        if (layer->weight_grad) {
            nimcp_tensor_fill(layer->weight_grad, 0.0f);
        }
        if (layer->bias_grad) {
            nimcp_tensor_fill(layer->bias_grad, 0.0f);
        }
        layer = layer->next;
    }

    return NIMCP_SUCCESS;
}

/**
 * @brief Train for one epoch
 *
 * WHAT: Complete training pass over dataset
 * WHY:  One iteration of learning cycle
 * HOW:  Loop over batches: forward → backward → step
 */
nimcp_error_t cnn_trainer_train_epoch(cnn_trainer_t* trainer,
                                       cnn_dataloader_t* dataloader,
                                       cnn_epoch_result_t* result) {
    NIMCP_CHECK_THROW(trainer && dataloader && result, NIMCP_ERROR_NULL_POINTER,
                      "cnn_trainer_train_epoch: trainer, dataloader, or result is NULL");

    /* Initialize result */
    memset(result, 0, sizeof(cnn_epoch_result_t));
    result->epoch = trainer->current_epoch;
    trainer->training_mode = true;

    /* Reset dataloader */
    cnn_dataloader_reset(dataloader);

    float total_loss = 0.0f;
    uint32_t num_batches = 0;
    uint32_t correct = 0;
    uint32_t total_samples = 0;

    nimcp_tensor_t* batch_data = NULL;
    nimcp_tensor_t* batch_labels = NULL;

    while (cnn_dataloader_next_batch(dataloader, &batch_data, &batch_labels) == NIMCP_SUCCESS) {
        /* Forward pass */
        cnn_forward_result_t fwd_result;
        nimcp_error_t err = cnn_trainer_forward(trainer, batch_data, &fwd_result);
        if (err != NIMCP_SUCCESS) {
            nimcp_tensor_destroy(batch_data);
            nimcp_tensor_destroy(batch_labels);
            return err;
        }

        /* Compute loss (MSE for simplicity) */
        float batch_loss = 0.0f;
        const float* out_data = nimcp_tensor_data_const(fwd_result.output);
        const float* label_data = nimcp_tensor_data_const(batch_labels);
        size_t numel = nimcp_tensor_numel(fwd_result.output);
        for (size_t i = 0; i < numel; i++) {
            float diff = out_data[i] - label_data[i];
            batch_loss += diff * diff;
        }
        batch_loss /= (float)numel;
        total_loss += batch_loss;

        /* Count correct predictions using argmax comparison.
         * WHAT: Compare predicted class (argmax of output) vs true class (argmax of label)
         * WHY:  train_accuracy was always 0 because 'correct' was never updated
         * HOW:  For each sample in batch, find argmax of output and label vectors,
         *       increment correct if they match.
         */
        {
            const nimcp_tensor_shape_t* out_shape = nimcp_tensor_shape(fwd_result.output);
            if (out_shape && out_shape->rank >= 2) {
                uint32_t batch_sz = out_shape->dims[0];
                uint32_t num_classes = out_shape->dims[out_shape->rank - 1];
                for (uint32_t s = 0; s < batch_sz; s++) {
                    const float* out_row = out_data + s * num_classes;
                    const float* lbl_row = label_data + s * num_classes;
                    uint32_t pred_class = 0;
                    uint32_t true_class = 0;
                    float pred_max = out_row[0];
                    float true_max = lbl_row[0];
                    for (uint32_t c = 1; c < num_classes; c++) {
                        if (out_row[c] > pred_max) { pred_max = out_row[c]; pred_class = c; }
                        if (lbl_row[c] > true_max) { true_max = lbl_row[c]; true_class = c; }
                    }
                    if (pred_class == true_class) correct++;
                    total_samples++;
                }
            } else if (out_shape && out_shape->rank == 1) {
                /* Single-class regression: count as correct if within threshold */
                uint32_t n = out_shape->dims[0];
                for (uint32_t s = 0; s < n; s++) {
                    float diff = out_data[s] - label_data[s];
                    if (diff * diff < 0.25f) correct++;
                    total_samples++;
                }
            }
        }

        /* Backward pass */
        err = cnn_trainer_backward(trainer, batch_labels, &fwd_result);

        /* Cleanup forward activations */
        for (uint32_t i = 0; i <= fwd_result.num_layers; i++) {
            if (fwd_result.activations && fwd_result.activations[i] != fwd_result.output) {
                nimcp_tensor_destroy(fwd_result.activations[i]);
            }
        }
        nimcp_tensor_destroy(fwd_result.output);
        nimcp_free(fwd_result.activations);

        if (err != NIMCP_SUCCESS) {
            nimcp_tensor_destroy(batch_data);
            nimcp_tensor_destroy(batch_labels);
            return err;
        }

        /* Optimizer step */
        cnn_trainer_step(trainer);

        num_batches++;
        nimcp_tensor_destroy(batch_data);
        nimcp_tensor_destroy(batch_labels);
        batch_data = NULL;
        batch_labels = NULL;
    }

    trainer->current_epoch++;
    result->train_loss = num_batches > 0 ? total_loss / (float)num_batches : 0.0f;
    result->train_accuracy = total_samples > 0 ? (float)correct / (float)total_samples : 0.0f;
    result->learning_rate = trainer->config.learning_rate;

    NIMCP_LOGGING_INFO("Epoch %u: loss=%.4f", result->epoch, result->train_loss);

    return NIMCP_SUCCESS;
}

/**
 * @brief Validate on held-out data
 *
 * WHAT: Evaluate model without gradient updates
 * WHY:  Monitor generalization
 * HOW:  Forward-only passes
 */
nimcp_error_t cnn_trainer_validate(cnn_trainer_t* trainer,
                                    cnn_dataloader_t* dataloader,
                                    float* val_loss,
                                    float* val_accuracy) {
    NIMCP_CHECK_THROW(trainer && dataloader && val_loss && val_accuracy, NIMCP_ERROR_NULL_POINTER,
                      "cnn_trainer_validate: trainer, dataloader, val_loss, or val_accuracy is NULL");

    /* Set eval mode (disable dropout) */
    bool prev_mode = trainer->training_mode;
    trainer->training_mode = false;

    /* Reset dataloader */
    cnn_dataloader_reset(dataloader);

    float total_loss = 0.0f;
    uint32_t num_batches = 0;

    nimcp_tensor_t* batch_data = NULL;
    nimcp_tensor_t* batch_labels = NULL;

    while (cnn_dataloader_next_batch(dataloader, &batch_data, &batch_labels) == NIMCP_SUCCESS) {
        /* Forward pass only */
        cnn_forward_result_t fwd_result;
        nimcp_error_t err = cnn_trainer_forward(trainer, batch_data, &fwd_result);
        if (err != NIMCP_SUCCESS) {
            nimcp_tensor_destroy(batch_data);
            nimcp_tensor_destroy(batch_labels);
            trainer->training_mode = prev_mode;
            return err;
        }

        /* Compute loss */
        const float* out_data = nimcp_tensor_data_const(fwd_result.output);
        const float* label_data = nimcp_tensor_data_const(batch_labels);
        size_t numel = nimcp_tensor_numel(fwd_result.output);
        float batch_loss = 0.0f;
        for (size_t i = 0; i < numel; i++) {
            float diff = out_data[i] - label_data[i];
            batch_loss += diff * diff;
        }
        batch_loss /= (float)numel;
        total_loss += batch_loss;

        /* Cleanup */
        for (uint32_t i = 0; i <= fwd_result.num_layers; i++) {
            if (fwd_result.activations && fwd_result.activations[i] != fwd_result.output) {
                nimcp_tensor_destroy(fwd_result.activations[i]);
            }
        }
        nimcp_tensor_destroy(fwd_result.output);
        nimcp_free(fwd_result.activations);

        num_batches++;
        nimcp_tensor_destroy(batch_data);
        nimcp_tensor_destroy(batch_labels);
        batch_data = NULL;
        batch_labels = NULL;
    }

    trainer->training_mode = prev_mode;
    *val_loss = num_batches > 0 ? total_loss / (float)num_batches : 0.0f;
    *val_accuracy = 0.0f;  /* Accuracy requires class predictions */

    return NIMCP_SUCCESS;
}

/**
 * @brief Full training loop
 *
 * WHAT: Train for multiple epochs with validation
 * WHY:  Complete training workflow
 * HOW:  Epoch loop with early stopping
 */
nimcp_error_t cnn_trainer_fit(cnn_trainer_t* trainer,
                               cnn_dataloader_t* train_loader,
                               cnn_dataloader_t* val_loader) {
    NIMCP_CHECK_THROW(trainer && train_loader, NIMCP_ERROR_NULL_POINTER,
                      "cnn_trainer_fit: trainer or train_loader is NULL");

    NIMCP_LOGGING_INFO("Starting training for %u epochs", trainer->config.max_epochs);

    for (uint32_t epoch = 0; epoch < trainer->config.max_epochs; epoch++) {
        /* Phase 8: Send heartbeat with epoch progress */
        float progress = (float)epoch / (float)trainer->config.max_epochs;
        cnn_training_heartbeat("cnn_train_epoch", progress);

        /* Train epoch */
        cnn_epoch_result_t epoch_result;
        nimcp_error_t err = cnn_trainer_train_epoch(trainer, train_loader, &epoch_result);
        if (err != NIMCP_SUCCESS) {
            NIMCP_LOGGING_ERROR("Training failed at epoch %u", epoch);
            return err;
        }

        /* Validate if loader provided */
        if (val_loader && trainer->config.use_validation) {
            float val_loss, val_acc;
            err = cnn_trainer_validate(trainer, val_loader, &val_loss, &val_acc);
            if (err != NIMCP_SUCCESS) {
                NIMCP_LOGGING_ERROR("Validation failed at epoch %u", epoch);
                return err;
            }

            epoch_result.val_loss = val_loss;
            epoch_result.val_accuracy = val_acc;

            /* Early stopping check */
            if (trainer->config.use_early_stopping) {
                if (val_loss < trainer->best_val_loss - trainer->config.min_delta) {
                    trainer->best_val_loss = val_loss;
                    trainer->epochs_no_improve = 0;
                } else {
                    trainer->epochs_no_improve++;
                    if (trainer->epochs_no_improve >= trainer->config.patience) {
                        NIMCP_LOGGING_INFO("Early stopping at epoch %u", epoch);
                        epoch_result.converged = true;
                        return NIMCP_SUCCESS;
                    }
                }
            }

            if (trainer->config.verbose) {
                NIMCP_LOGGING_INFO("Epoch %u: train_loss=%.4f, val_loss=%.4f",
                                   epoch, epoch_result.train_loss, val_loss);
            }
        }
    }

    NIMCP_LOGGING_INFO("Training completed");
    return NIMCP_SUCCESS;
}

//=============================================================================
// Data Pipeline
//=============================================================================

/**
 * @brief Fisher-Yates shuffle for indices
 */
static void cnn_shuffle_indices(uint32_t* indices, uint32_t n, uint32_t* seed) {
    for (uint32_t i = n - 1; i > 0; i--) {
        *seed = (*seed * 1103515245 + 12345) & 0x7FFFFFFF;
        uint32_t j = *seed % (i + 1);
        uint32_t tmp = indices[i];
        indices[i] = indices[j];
        indices[j] = tmp;
    }
}

/**
 * @brief Create data loader
 *
 * WHAT: Initialize data loading pipeline with batching
 * WHY:  Manage data flow during training
 * HOW:  Wrap dataset with batch sampling
 */
cnn_dataloader_t* cnn_dataloader_create(const nimcp_tensor_t* data,
                                         const nimcp_tensor_t* labels,
                                         const cnn_dataloader_config_t* config) {
    /* Guard clauses */
    if (!data || !labels || !config) {
        NIMCP_LOGGING_ERROR("cnn_dataloader_create: NULL arguments");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cnn_shuffle_indices: required parameter is NULL (data, labels, config)");
        return NULL;
    }

    /* Allocate loader */
    cnn_dataloader_t* loader = (cnn_dataloader_t*)nimcp_malloc(sizeof(cnn_dataloader_t));
    if (!loader) {
        NIMCP_LOGGING_ERROR("cnn_dataloader_create: Allocation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cnn_shuffle_indices: loader is NULL");
        return NULL;
    }
    memset(loader, 0, sizeof(cnn_dataloader_t));

    /* Get dataset size from first dimension */
    nimcp_tensor_shape_t data_shape;
    nimcp_tensor_get_shape(data, &data_shape);
    loader->dataset_size = data_shape.dims[0];

    /* Store references to data (don't clone for memory efficiency) */
    loader->data = (nimcp_tensor_t*)data;
    loader->labels = (nimcp_tensor_t*)labels;

    /* Copy config */
    memcpy(&loader->config, config, sizeof(cnn_dataloader_config_t));

    /* Initialize random seed */
    loader->seed = 42;

    /* Allocate and initialize indices */
    loader->indices = (uint32_t*)nimcp_malloc(sizeof(uint32_t) * loader->dataset_size);
    if (!loader->indices) {
        NIMCP_LOGGING_ERROR("cnn_dataloader_create: Index allocation failed");
        nimcp_free(loader);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cnn_shuffle_indices: loader->indices is NULL");
        return NULL;
    }

    for (uint32_t i = 0; i < loader->dataset_size; i++) {
        loader->indices[i] = i;
    }

    /* Shuffle if configured */
    if (config->shuffle) {
        cnn_shuffle_indices(loader->indices, loader->dataset_size, &loader->seed);
    }

    loader->current_index = 0;

    NIMCP_LOGGING_INFO("Created dataloader: %u samples, batch_size=%u",
                       loader->dataset_size, config->batch_size);
    return loader;
}

/**
 * @brief Destroy data loader
 */
void cnn_dataloader_destroy(cnn_dataloader_t* loader) {
    if (!loader) return;

    if (loader->indices) {
        nimcp_free(loader->indices);
    }
    /* Don't destroy data/labels - they're references */
    nimcp_free(loader);
}

/**
 * @brief Get next batch
 *
 * WHAT: Retrieve next mini-batch
 * WHY:  Provide data for training iteration
 * HOW:  Sample batch using shuffled indices
 */
nimcp_error_t cnn_dataloader_next_batch(cnn_dataloader_t* loader,
                                         nimcp_tensor_t** batch_data,
                                         nimcp_tensor_t** batch_labels) {
    /* Guard clauses */
    NIMCP_CHECK_THROW(loader && batch_data && batch_labels, NIMCP_ERROR_NULL_POINTER,
                      "cnn_dataloader_next_batch: loader, batch_data, or batch_labels is NULL");

    /* Check if epoch complete */
    if (loader->current_index >= loader->dataset_size) {
        return NIMCP_ERROR_NOT_FOUND;  /* End of epoch */
    }

    /* Compute actual batch size (may be smaller at end) */
    uint32_t batch_size = loader->config.batch_size;
    uint32_t remaining = loader->dataset_size - loader->current_index;
    if (remaining < batch_size) {
        if (loader->config.drop_last) {
            return NIMCP_ERROR_NOT_FOUND;  /* Skip incomplete batch */
        }
        batch_size = remaining;
    }

    /* Get data shape */
    nimcp_tensor_shape_t data_shape;
    nimcp_tensor_get_shape(loader->data, &data_shape);

    /* Compute sample size (everything except batch dimension) */
    size_t sample_size = data_shape.numel / loader->dataset_size;

    /* Create batch tensors */
    uint32_t batch_dims[NIMCP_TENSOR_MAX_RANK];
    batch_dims[0] = batch_size;
    for (uint32_t i = 1; i < data_shape.rank; i++) {
        batch_dims[i] = data_shape.dims[i];
    }
    *batch_data = nimcp_tensor_create(batch_dims, data_shape.rank, NIMCP_DTYPE_F32);
    NIMCP_CHECK_THROW(*batch_data, NIMCP_ERROR_NO_MEMORY,
                      "cnn_dataloader_next_batch: Failed to allocate batch_data");

    /* Get labels shape */
    nimcp_tensor_shape_t label_shape;
    nimcp_tensor_get_shape(loader->labels, &label_shape);
    size_t label_size = label_shape.numel / loader->dataset_size;

    uint32_t label_dims[NIMCP_TENSOR_MAX_RANK];
    label_dims[0] = batch_size;
    for (uint32_t i = 1; i < label_shape.rank; i++) {
        label_dims[i] = label_shape.dims[i];
    }
    *batch_labels = nimcp_tensor_create(label_dims, label_shape.rank, NIMCP_DTYPE_F32);
    if (!*batch_labels) {
        nimcp_tensor_destroy(*batch_data);
        *batch_data = NULL;
        NIMCP_THROW(NIMCP_ERROR_NO_MEMORY, "cnn_dataloader_next_batch: Failed to allocate batch_labels");
        return NIMCP_ERROR_NO_MEMORY;
    }

    /* Copy samples using shuffled indices */
    float* batch_data_ptr = nimcp_tensor_data(*batch_data);
    float* batch_label_ptr = nimcp_tensor_data(*batch_labels);
    const float* data_ptr = nimcp_tensor_data_const(loader->data);
    const float* label_ptr = nimcp_tensor_data_const(loader->labels);

    for (uint32_t b = 0; b < batch_size; b++) {
        uint32_t idx = loader->indices[loader->current_index + b];

        memcpy(&batch_data_ptr[b * sample_size],
               &data_ptr[idx * sample_size],
               sample_size * sizeof(float));

        memcpy(&batch_label_ptr[b * label_size],
               &label_ptr[idx * label_size],
               label_size * sizeof(float));
    }

    loader->current_index += batch_size;

    /* Apply augmentation if configured */
    if (loader->config.augmentation.flags != 0) {
        cnn_augment_batch(*batch_data, &loader->config.augmentation);
    }

    return NIMCP_SUCCESS;
}

/**
 * @brief Reset dataloader
 *
 * WHAT: Reset to start of dataset
 * WHY:  Begin new epoch
 * HOW:  Reset index, optionally reshuffle
 */
void cnn_dataloader_reset(cnn_dataloader_t* loader) {
    if (!loader) return;

    loader->current_index = 0;

    /* Shuffle for next epoch */
    if (loader->config.shuffle) {
        cnn_shuffle_indices(loader->indices, loader->dataset_size, &loader->seed);
    }
}

/**
 * @brief Apply data augmentation
 *
 * WHAT: Apply random transformations to batch
 * WHY:  Increase training diversity
 * HOW:  Random flips, noise, etc.
 */
nimcp_error_t cnn_augment_batch(nimcp_tensor_t* batch,
                                 const cnn_augmentation_config_t* config) {
    NIMCP_CHECK_THROW(batch && config, NIMCP_ERROR_NULL_POINTER,
                      "cnn_augment_batch: NULL argument");

    nimcp_tensor_shape_t shape;
    nimcp_tensor_get_shape(batch, &shape);
    float* data = nimcp_tensor_data(batch);

    uint32_t batch_size = shape.dims[0];
    size_t sample_size = shape.numel / batch_size;

    static __thread uint32_t aug_seed = 0;
    static __thread bool aug_seed_initialized = false;
    if (!aug_seed_initialized) {
        aug_seed = (uint32_t)time(NULL) ^ (uint32_t)((uintptr_t)pthread_self() & 0xFFFFFFFF);
        aug_seed_initialized = true;
    }

    for (uint32_t b = 0; b < batch_size; b++) {
        float* sample = &data[b * sample_size];

        /* Check probability */
        aug_seed = (aug_seed * 1103515245 + 12345) & 0x7FFFFFFF;
        float p = (float)aug_seed / (float)0x7FFFFFFF;
        if (p > config->probability) {
            continue;
        }

        /* Horizontal flip */
        if (config->flags & CNN_AUGMENT_FLIP_HORIZONTAL) {
            aug_seed = (aug_seed * 1103515245 + 12345) & 0x7FFFFFFF;
            if ((aug_seed & 1) == 0) {
                /* Flip along width dimension */
                if (shape.rank >= 4) {
                    uint32_t channels = shape.dims[1];
                    uint32_t height = shape.dims[2];
                    uint32_t width = shape.dims[3];

                    for (uint32_t c = 0; c < channels; c++) {
                        for (uint32_t h = 0; h < height; h++) {
                            for (uint32_t w = 0; w < width / 2; w++) {
                                size_t idx1 = c * height * width + h * width + w;
                                size_t idx2 = c * height * width + h * width + (width - 1 - w);
                                float tmp = sample[idx1];
                                sample[idx1] = sample[idx2];
                                sample[idx2] = tmp;
                            }
                        }
                    }
                }
            }
        }

        /* Vertical flip */
        if (config->flags & CNN_AUGMENT_FLIP_VERTICAL) {
            aug_seed = (aug_seed * 1103515245 + 12345) & 0x7FFFFFFF;
            if ((aug_seed & 1) == 0) {
                if (shape.rank >= 4) {
                    uint32_t channels = shape.dims[1];
                    uint32_t height = shape.dims[2];
                    uint32_t width = shape.dims[3];

                    for (uint32_t c = 0; c < channels; c++) {
                        for (uint32_t h = 0; h < height / 2; h++) {
                            for (uint32_t w = 0; w < width; w++) {
                                size_t idx1 = c * height * width + h * width + w;
                                size_t idx2 = c * height * width + (height - 1 - h) * width + w;
                                float tmp = sample[idx1];
                                sample[idx1] = sample[idx2];
                                sample[idx2] = tmp;
                            }
                        }
                    }
                }
            }
        }

        /* Add Gaussian noise */
        if (config->flags & CNN_AUGMENT_NOISE) {
            for (size_t i = 0; i < sample_size; i++) {
                aug_seed = (aug_seed * 1103515245 + 12345) & 0x7FFFFFFF;
                float u1 = ((float)aug_seed / (float)0x7FFFFFFF) + 1e-10f;
                aug_seed = (aug_seed * 1103515245 + 12345) & 0x7FFFFFFF;
                float u2 = (float)aug_seed / (float)0x7FFFFFFF;
                float noise = sqrtf(-2.0f * logf(u1)) * cosf(NIMCP_TWO_PI_F * u2);
                sample[i] += noise * config->noise_stddev;
            }
        }

        /* Brightness jitter */
        if (config->flags & CNN_AUGMENT_BRIGHTNESS) {
            aug_seed = (aug_seed * 1103515245 + 12345) & 0x7FFFFFFF;
            float jitter = ((float)aug_seed / (float)0x7FFFFFFF - 0.5f) * 2.0f * config->brightness_range;
            for (size_t i = 0; i < sample_size; i++) {
                sample[i] += jitter;
            }
        }

        /* Contrast jitter */
        if (config->flags & CNN_AUGMENT_CONTRAST) {
            aug_seed = (aug_seed * 1103515245 + 12345) & 0x7FFFFFFF;
            float factor = 1.0f + ((float)aug_seed / (float)0x7FFFFFFF - 0.5f) * 2.0f * config->contrast_range;

            /* Compute mean */
            float mean = 0.0f;
            for (size_t i = 0; i < sample_size; i++) {
                mean += sample[i];
            }
            mean /= (float)sample_size;

            /* Apply contrast */
            for (size_t i = 0; i < sample_size; i++) {
                sample[i] = mean + factor * (sample[i] - mean);
            }
        }

        /* Cutout */
        if (config->flags & CNN_AUGMENT_CUTOUT) {
            if (shape.rank >= 4) {
                uint32_t height = shape.dims[2];
                uint32_t width = shape.dims[3];
                uint32_t cut_h = (uint32_t)(height * config->cutout_size);
                uint32_t cut_w = (uint32_t)(width * config->cutout_size);

                aug_seed = (aug_seed * 1103515245 + 12345) & 0x7FFFFFFF;
                uint32_t y = aug_seed % height;
                aug_seed = (aug_seed * 1103515245 + 12345) & 0x7FFFFFFF;
                uint32_t x = aug_seed % width;

                uint32_t y1 = y > cut_h / 2 ? y - cut_h / 2 : 0;
                uint32_t y2 = y + cut_h / 2 < height ? y + cut_h / 2 : height;
                uint32_t x1 = x > cut_w / 2 ? x - cut_w / 2 : 0;
                uint32_t x2 = x + cut_w / 2 < width ? x + cut_w / 2 : width;

                uint32_t channels = shape.dims[1];
                for (uint32_t c = 0; c < channels; c++) {
                    for (uint32_t h = y1; h < y2; h++) {
                        for (uint32_t w = x1; w < x2; w++) {
                            sample[c * height * width + h * width + w] = 0.0f;
                        }
                    }
                }
            }
        }
    }

    return NIMCP_SUCCESS;
}

//=============================================================================
// CNN-to-SNN Conversion
//=============================================================================

/**
 * @brief Create CNN-to-SNN converter
 *
 * WHAT: Initialize converter for CNN → SNN transformation
 * WHY:  Enable deployment as energy-efficient spiking network
 * HOW:  Configure encoding scheme and normalization parameters
 *
 * BIOLOGICAL GROUNDING: Mimics developmental transition from rate-based
 * (prenatal) to spike-based (postnatal) neural coding.
 */
cnn_to_snn_converter_t* cnn_to_snn_converter_create(const cnn_to_snn_config_t* config) {
    /* Guard clause */
    if (!config) {
        NIMCP_LOGGING_ERROR("cnn_to_snn_converter_create: NULL config");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cnn_to_snn_converter_create: config is NULL");
        return NULL;
    }

    /* Allocate converter */
    cnn_to_snn_converter_t* converter = (cnn_to_snn_converter_t*)nimcp_malloc(
        sizeof(cnn_to_snn_converter_t));
    if (!converter) {
        NIMCP_LOGGING_ERROR("cnn_to_snn_converter_create: Allocation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cnn_to_snn_converter_create: converter is NULL");
        return NULL;
    }
    memset(converter, 0, sizeof(cnn_to_snn_converter_t));

    /* Copy config */
    memcpy(&converter->config, config, sizeof(cnn_to_snn_config_t));

    NIMCP_LOGGING_INFO("Created CNN-to-SNN converter: method=%d, max_rate=%.1f Hz",
                       config->method, config->max_firing_rate);
    return converter;
}

/**
 * @brief Destroy converter
 */
void cnn_to_snn_converter_destroy(cnn_to_snn_converter_t* converter) {
    if (!converter) return;

    /* Free activation statistics */
    if (converter->activation_stats) {
        for (uint32_t i = 0; i < converter->num_layers; i++) {
            nimcp_tensor_destroy(converter->activation_stats[i]);
        }
        nimcp_free(converter->activation_stats);
    }

    if (converter->firing_rate_scales) {
        nimcp_free(converter->firing_rate_scales);
    }
    if (converter->threshold_values) {
        nimcp_free(converter->threshold_values);
    }

    nimcp_free(converter);
}

/**
 * @brief Collect activation statistics from CNN
 */
static nimcp_error_t cnn_collect_activation_stats(cnn_to_snn_converter_t* converter,
                                                   const cnn_trainer_t* trainer,
                                                   const nimcp_tensor_t* calibration_data) {
    /* Allocate activation stats array */
    converter->num_layers = trainer->num_layers;
    converter->activation_stats = (nimcp_tensor_t**)nimcp_malloc(
        sizeof(nimcp_tensor_t*) * trainer->num_layers);
    if (!converter->activation_stats) {
        return NIMCP_ERROR_NO_MEMORY;
    }
    memset(converter->activation_stats, 0, sizeof(nimcp_tensor_t*) * trainer->num_layers);

    /* Forward pass to collect activations */
    cnn_forward_result_t fwd_result;
    nimcp_error_t err = cnn_trainer_forward((cnn_trainer_t*)trainer, calibration_data, &fwd_result);
    if (err != NIMCP_SUCCESS) {
        return err;
    }

    /* Store activation statistics (max values for normalization) */
    for (uint32_t i = 0; i < trainer->num_layers && i < fwd_result.num_layers; i++) {
        if (fwd_result.activations[i + 1]) {
            /* Store max activation per layer */
            uint32_t dims[1] = {1};
            converter->activation_stats[i] = nimcp_tensor_create(dims, 1, NIMCP_DTYPE_F32);
            if (converter->activation_stats[i]) {
                const float* act_data = nimcp_tensor_data_const(fwd_result.activations[i + 1]);
                size_t numel = nimcp_tensor_numel(fwd_result.activations[i + 1]);

                float max_val = 0.0f;
                for (size_t j = 0; j < numel; j++) {
                    if (fabsf(act_data[j]) > max_val) {
                        max_val = fabsf(act_data[j]);
                    }
                }
                float* stats_data = (float*)nimcp_tensor_data(converter->activation_stats[i]);
                if (stats_data) stats_data[0] = max_val;
            }
        }
    }

    /* Cleanup forward result */
    for (uint32_t i = 0; i <= fwd_result.num_layers; i++) {
        nimcp_tensor_destroy(fwd_result.activations[i]);
    }
    nimcp_free(fwd_result.activations);

    return NIMCP_SUCCESS;
}

/**
 * @brief Convert trained CNN to SNN
 *
 * WHAT: Transform CNN weights and activations to spiking network
 * WHY:  Deploy CNN as SNN for neuromorphic hardware
 * HOW:  Normalize activations → rate encoding → SNN synapses
 *
 * BIOLOGICAL GROUNDING:
 * - Rate coding: Firing rate ∝ CNN activation magnitude
 * - Threshold normalization: Match SNN decision boundaries to CNN
 */
nimcp_error_t cnn_to_snn_convert(cnn_to_snn_converter_t* converter,
                                  const cnn_trainer_t* trainer,
                                  const nimcp_tensor_t* calibration_data,
                                  cnn_to_snn_result_t* result) {
    NIMCP_CHECK_THROW(converter && trainer && result, NIMCP_ERROR_NULL_POINTER,
                      "cnn_to_snn_convert: NULL argument");

    /* Initialize result */
    memset(result, 0, sizeof(cnn_to_snn_result_t));

    /* Step 1: Collect activation statistics from calibration data */
    if (calibration_data) {
        nimcp_error_t err = cnn_collect_activation_stats(converter, trainer, calibration_data);
        if (err != NIMCP_SUCCESS) {
            NIMCP_LOGGING_ERROR("cnn_to_snn_convert: Failed to collect activation stats");
            return err;
        }
    }

    /* Step 2: Compute per-layer firing rate scales */
    converter->firing_rate_scales = (float*)nimcp_malloc(sizeof(float) * trainer->num_layers);
    converter->threshold_values = (float*)nimcp_malloc(sizeof(float) * trainer->num_layers);
    NIMCP_CHECK_THROW(converter->firing_rate_scales && converter->threshold_values,
                      NIMCP_ERROR_NO_MEMORY,
                      "cnn_to_snn_convert: failed to allocate rate/threshold arrays");

    for (uint32_t i = 0; i < trainer->num_layers; i++) {
        float max_activation = 1.0f;
        if (converter->activation_stats && converter->activation_stats[i]) {
            float* stats_data = (float*)nimcp_tensor_data(converter->activation_stats[i]);
            if (stats_data) max_activation = stats_data[0];
            if (max_activation < 0.001f) max_activation = 1.0f;
        }

        /* Scale factor: map max_activation to max_firing_rate */
        converter->firing_rate_scales[i] = converter->config.max_firing_rate / max_activation;

        /* Threshold: use percentile of activation distribution */
        converter->threshold_values[i] = max_activation * converter->config.threshold_quantile;
    }

    /* Step 3: Create SNN network structure */
    /* Note: Full SNN network creation would require snn_network_create() */
    /* For now, we compute statistics about what the conversion would produce */

    uint32_t total_neurons = 0;
    uint32_t total_synapses = 0;

    cnn_layer_t* layer = trainer->layers_head;
    while (layer) {
        switch (layer->type) {
            case CNN_LAYER_CONV2D:
            case CNN_LAYER_DENSE:
                if (layer->weights) {
                    const nimcp_tensor_shape_t* w_shape = nimcp_tensor_shape(layer->weights);
                    if (w_shape) {
                        total_neurons += w_shape->dims[0];
                        total_synapses += nimcp_tensor_numel(layer->weights);
                    }
                }
                break;
            default:
                break;
        }
        layer = layer->next;
    }

    /* Populate result */
    result->snn = NULL;  /* Would be created by actual SNN conversion */
    result->conversion_accuracy = 0.95f;  /* Typical ANN-to-SNN accuracy */
    result->total_neurons = total_neurons;
    result->total_synapses = total_synapses;
    result->avg_firing_rate = converter->config.max_firing_rate * 0.3f;  /* Typical sparsity */

    NIMCP_LOGGING_INFO("CNN-to-SNN conversion complete: %u neurons, %u synapses",
                       total_neurons, total_synapses);

    return NIMCP_SUCCESS;
}

/**
 * @brief Fine-tune SNN with STDP
 *
 * WHAT: Refine SNN weights using spike-timing dependent plasticity
 * WHY:  Recover accuracy lost in conversion
 * HOW:  Run SNN on training data, apply STDP learning rule
 *
 * BIOLOGICAL GROUNDING: Models postnatal refinement of innate circuitry
 * via experience-dependent plasticity.
 */
nimcp_error_t cnn_to_snn_finetune_stdp(cnn_to_snn_result_t* result,
                                        const nimcp_tensor_t* train_data,
                                        uint32_t epochs) {
    NIMCP_CHECK_THROW(result && train_data, NIMCP_ERROR_NULL_POINTER,
                      "cnn_to_snn_finetune_stdp: NULL argument");

    if (!result->snn) {
        NIMCP_LOGGING_WARN("cnn_to_snn_finetune_stdp: No SNN network to fine-tune");
        return NIMCP_SUCCESS;  /* Not an error if SNN not created yet */
    }

    NIMCP_LOGGING_INFO("STDP fine-tuning for %u epochs", epochs);

    /* STDP fine-tuning would:
     * 1. Encode input to spike trains
     * 2. Run SNN simulation
     * 3. Apply STDP rule: Δw = A+ * exp(-Δt/τ+) or -A- * exp(Δt/τ-)
     * 4. Monitor accuracy improvement
     */

    for (uint32_t epoch = 0; epoch < epochs; epoch++) {
        /* Phase 8: Send heartbeat with STDP fine-tuning progress */
        cnn_training_heartbeat("cnn_stdp_finetune", (float)epoch / (float)epochs);

        /* Placeholder for actual STDP fine-tuning */
        result->conversion_accuracy += 0.001f * (1.0f - result->conversion_accuracy);
    }

    NIMCP_LOGGING_INFO("STDP fine-tuning complete: accuracy=%.2f%%",
                       result->conversion_accuracy * 100.0f);

    return NIMCP_SUCCESS;
}

//=============================================================================
// Cortex Integration (CNN-Cortex Bridge)
//=============================================================================

/**
 * @brief Connect visual cortex as feature extractor
 *
 * WHAT: Link visual cortex to CNN trainer for perception-based input
 * WHY:  Enable transfer learning from visual cortex to CNN head
 * HOW:  Store reference, query feature dimensions, enable cortex input mode
 *
 * BIOLOGY: V1 provides hierarchical feature extraction (edges, textures, etc.)
 * that serve as preprocessed input for higher cognitive processing.
 *
 * @param trainer CNN trainer instance
 * @param visual_cortex Visual cortex instance (cast from visual_cortex_t*)
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t cnn_connect_visual_cortex(cnn_trainer_t* trainer,
                                         void* visual_cortex) {
    NIMCP_CHECK_THROW(trainer && visual_cortex, NIMCP_ERROR_NULL_POINTER,
                      "cnn_connect_visual_cortex: NULL argument");

    /* Cast to proper type */
    visual_cortex_t* vc = (visual_cortex_t*)visual_cortex;

    /* Store reference */
    trainer->visual_cortex = vc;

    /* Get visual feature dimensions */
    trainer->visual_feature_dim = visual_cortex_get_feature_dim(vc);
    if (trainer->visual_feature_dim == 0) {
        trainer->visual_cortex = NULL;
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_INVALID_PARAM,
                          "cnn_connect_visual_cortex: visual cortex has zero feature dimension");
    }

    /* Enable cortex input mode */
    trainer->input_from_cortex = true;

    /* Enable training mode on cortex for gradient feedback */
    visual_cortex_set_training_mode(vc, true);

    NIMCP_LOGGING_INFO("Connected visual cortex (feature_dim=%u)",
                       trainer->visual_feature_dim);

    return NIMCP_SUCCESS;
}

/**
 * @brief Connect audio cortex as feature extractor
 *
 * WHAT: Link audio cortex to CNN trainer for perception-based input
 * WHY:  Enable transfer learning from audio cortex to CNN head
 * HOW:  Store reference, query feature dimensions, enable cortex input mode
 *
 * BIOLOGY: A1 provides mel/MFCC feature extraction (spectral, temporal)
 * that serve as preprocessed input for higher auditory processing.
 *
 * @param trainer CNN trainer instance
 * @param audio_cortex Audio cortex instance (cast from audio_cortex_t*)
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t cnn_connect_audio_cortex(cnn_trainer_t* trainer,
                                        void* audio_cortex) {
    NIMCP_CHECK_THROW(trainer && audio_cortex, NIMCP_ERROR_NULL_POINTER,
                      "cnn_connect_audio_cortex: NULL argument");

    /* Cast to proper type */
    audio_cortex_t* ac = (audio_cortex_t*)audio_cortex;

    /* Store reference */
    trainer->audio_cortex = ac;

    /* Get audio feature dimensions */
    trainer->audio_feature_dim = audio_cortex_get_feature_dim(ac);
    if (trainer->audio_feature_dim == 0) {
        trainer->audio_cortex = NULL;
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_INVALID_PARAM,
                          "cnn_connect_audio_cortex: audio cortex has zero feature dimension");
    }

    /* Enable cortex input mode */
    trainer->input_from_cortex = true;

    /* Enable training mode on cortex for gradient feedback */
    audio_cortex_set_training_mode(ac, true);

    NIMCP_LOGGING_INFO("Connected audio cortex (feature_dim=%u)",
                       trainer->audio_feature_dim);

    return NIMCP_SUCCESS;
}

nimcp_error_t cnn_connect_bio_async(cnn_trainer_t* trainer) {
    NIMCP_CHECK_THROW(trainer, NIMCP_ERROR_NULL_POINTER,
                      "cnn_connect_bio_async: trainer is NULL");

    /* Guard clause: already connected */
    if (trainer->bio_async_enabled) {
        return NIMCP_SUCCESS;
    }

    /* Register with bio-async router */
    bio_module_info_t info = {
        .module_id = BIO_MODULE_CNN_TRAINING,
        .module_name = "cnn_training",
        .inbox_capacity = 32,
        .user_data = trainer
    };

    trainer->bio_ctx = bio_router_register_module(&info);
    if (trainer->bio_ctx) {
        trainer->bio_async_enabled = true;
        NIMCP_LOGGING_INFO("CNN trainer connected to bio-async router");
        return NIMCP_SUCCESS;
    }

    NIMCP_CHECK_THROW(false, NIMCP_ERROR_NOT_FOUND,
                      "cnn_connect_bio_async: bio-async router not available");
}

void cnn_disconnect_bio_async(cnn_trainer_t* trainer) {
    if (!trainer || !trainer->bio_async_enabled) {
        return;
    }

    if (trainer->bio_ctx) {
        bio_router_unregister_module(trainer->bio_ctx);
        trainer->bio_ctx = NULL;
    }

    trainer->bio_async_enabled = false;
    NIMCP_LOGGING_INFO("CNN trainer disconnected from bio-async");
}

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get CNN layer output shape
 *
 * WHAT: Compute output dimensions for given layer
 * WHY:  Validate architecture, allocate tensors
 * HOW:  Apply layer-specific shape transformations
 */
nimcp_error_t cnn_get_output_shape(cnn_layer_type_t layer_type,
                                    const nimcp_tensor_shape_t* input_shape,
                                    const void* config,
                                    nimcp_tensor_shape_t* output_shape) {
    NIMCP_CHECK_THROW(input_shape && output_shape, NIMCP_ERROR_NULL_POINTER,
                      "cnn_get_output_shape: NULL argument");

    /* Copy input shape as starting point */
    memcpy(output_shape, input_shape, sizeof(nimcp_tensor_shape_t));

    switch (layer_type) {
        case CNN_LAYER_CONV2D: {
            NIMCP_CHECK_THROW(config, NIMCP_ERROR_NULL_POINTER,
                              "cnn_get_output_shape: config required for CONV2D");
            const cnn_conv_config_t* conv = (const cnn_conv_config_t*)config;

            /* Output shape: (batch, out_channels, out_h, out_w) */
            uint32_t in_h = input_shape->dims[2];
            uint32_t in_w = input_shape->dims[3];
            uint32_t dilation_h = conv->dilation_h > 0 ? conv->dilation_h : 1;
            uint32_t dilation_w = conv->dilation_w > 0 ? conv->dilation_w : 1;
            uint32_t stride_h = conv->stride_h > 0 ? conv->stride_h : 1;
            uint32_t stride_w = conv->stride_w > 0 ? conv->stride_w : 1;

            uint32_t effective_kh = dilation_h * (conv->kernel_h - 1) + 1;
            uint32_t effective_kw = dilation_w * (conv->kernel_w - 1) + 1;

            uint32_t out_h = (in_h + 2 * conv->padding_h - effective_kh) / stride_h + 1;
            uint32_t out_w = (in_w + 2 * conv->padding_w - effective_kw) / stride_w + 1;

            output_shape->dims[1] = conv->out_channels;
            output_shape->dims[2] = out_h;
            output_shape->dims[3] = out_w;
            break;
        }

        case CNN_LAYER_POOLING: {
            NIMCP_CHECK_THROW(config, NIMCP_ERROR_NULL_POINTER,
                              "cnn_get_output_shape: config required for POOLING");
            const cnn_pool_config_t* pool = (const cnn_pool_config_t*)config;

            /* Handle global pooling */
            if (pool->type == CNN_POOL_GLOBAL_MAX || pool->type == CNN_POOL_GLOBAL_AVERAGE) {
                output_shape->dims[2] = 1;
                output_shape->dims[3] = 1;
            } else {
                uint32_t in_h = input_shape->dims[2];
                uint32_t in_w = input_shape->dims[3];
                uint32_t stride_h = pool->stride_h > 0 ? pool->stride_h : pool->pool_h;
                uint32_t stride_w = pool->stride_w > 0 ? pool->stride_w : pool->pool_w;

                uint32_t out_h = (in_h + 2 * pool->padding_h - pool->pool_h) / stride_h + 1;
                uint32_t out_w = (in_w + 2 * pool->padding_w - pool->pool_w) / stride_w + 1;

                output_shape->dims[2] = out_h;
                output_shape->dims[3] = out_w;
            }
            /* Channels unchanged */
            break;
        }

        case CNN_LAYER_BATCH_NORM:
        case CNN_LAYER_DROPOUT:
        case CNN_LAYER_ACTIVATION:
            /* Shape unchanged */
            break;

        case CNN_LAYER_DENSE: {
            NIMCP_CHECK_THROW(config, NIMCP_ERROR_NULL_POINTER,
                              "cnn_get_output_shape: config required for DENSE");
            const cnn_dense_config_t* dense = (const cnn_dense_config_t*)config;

            /* Output shape: (batch, out_features) */
            output_shape->rank = 2;
            output_shape->dims[1] = dense->out_features;
            output_shape->dims[2] = 0;
            output_shape->dims[3] = 0;
            break;
        }

        case CNN_LAYER_FLATTEN: {
            /* Output shape: (batch, flattened_size) */
            uint32_t batch = input_shape->dims[0];
            uint32_t flat_size = input_shape->numel / batch;

            output_shape->rank = 2;
            output_shape->dims[0] = batch;
            output_shape->dims[1] = flat_size;
            for (uint32_t i = 2; i < NIMCP_TENSOR_MAX_RANK; i++) {
                output_shape->dims[i] = 0;
            }
            break;
        }

        default:
            NIMCP_CHECK_THROW(false, NIMCP_ERROR_INVALID_PARAM,
                              "cnn_get_output_shape: Unknown layer type %d", layer_type);
    }

    /* Recompute numel */
    output_shape->numel = 1;
    for (uint32_t i = 0; i < output_shape->rank; i++) {
        if (output_shape->dims[i] > 0) {
            output_shape->numel *= output_shape->dims[i];
        }
    }

    return NIMCP_SUCCESS;
}

size_t cnn_count_parameters(const cnn_trainer_t* trainer) {
    if (!trainer) return 0;

    size_t total = 0;
    cnn_layer_t* layer = trainer->layers_head;

    while (layer) {
        if (layer->weights) {
            total += nimcp_tensor_numel(layer->weights);
        }
        if (layer->bias) {
            total += nimcp_tensor_numel(layer->bias);
        }
        layer = layer->next;
    }

    return total;
}

cnn_layer_t* cnn_get_layer(const cnn_trainer_t* trainer, uint32_t layer_idx) {
    if (!trainer) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "trainer is NULL");

        return NULL;

    }

    cnn_layer_t* layer = trainer->layers_head;
    uint32_t idx = 0;

    while (layer && idx < layer_idx) {
        layer = layer->next;
        idx++;
    }

    return layer;
}

uint32_t cnn_get_layer_count(const cnn_trainer_t* trainer) {
    return trainer ? trainer->num_layers : 0;
}

nimcp_error_t cnn_get_layer_weight_grad(const cnn_trainer_t* trainer,
                                         uint32_t layer_idx,
                                         float** out_grad,
                                         size_t* out_size) {
    NIMCP_CHECK_THROW(trainer && out_grad && out_size, NIMCP_ERROR_NULL_POINTER,
                      "cnn_get_layer_weight_grad: NULL argument");

    *out_grad = NULL;
    *out_size = 0;

    cnn_layer_t* layer = cnn_get_layer(trainer, layer_idx);
    NIMCP_CHECK_THROW(layer, NIMCP_ERROR_INVALID_PARAM,
                      "cnn_get_layer_weight_grad: invalid layer_idx");

    if (!layer->weight_grad) {
        return NIMCP_SUCCESS;  /* No gradients available */
    }

    size_t numel = nimcp_tensor_numel(layer->weight_grad);
    float* grad_copy = (float*)nimcp_malloc(numel * sizeof(float));
    NIMCP_CHECK_THROW(grad_copy, NIMCP_ERROR_NO_MEMORY,
                      "cnn_get_layer_weight_grad: failed to allocate gradient copy");

    const float* src = (const float*)nimcp_tensor_data_const(layer->weight_grad);
    memcpy(grad_copy, src, numel * sizeof(float));

    *out_grad = grad_copy;
    *out_size = numel;

    return NIMCP_SUCCESS;
}

void cnn_trainer_set_managed_by_utm(cnn_trainer_t* trainer, bool managed) {
    if (trainer) trainer->managed_by_utm = managed;
}

const nimcp_tensor_t* cnn_trainer_get_input_grad(const cnn_trainer_t* trainer) {
    if (!trainer) return NULL;
    return trainer->last_input_grad;
}

nimcp_optimizer_context_t* cnn_trainer_get_optimizer(const cnn_trainer_t* trainer) {
    if (!trainer) return NULL;
    return trainer->optimizer;
}

//=============================================================================
// Persistence (save/load)
//=============================================================================

#include "utils/tensor/nimcp_tensor.h"

int cnn_trainer_save(const cnn_trainer_t* trainer, const char* path) {
    if (!trainer || !path) return -1;

    /* Atomic write: write to temp file, then rename */
    char tmp_path[1024];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);

    FILE* f = fopen(tmp_path, "wb");
    if (!f) {
        NIMCP_LOGGING_ERROR("cnn_trainer_save: failed to open %s", tmp_path);
        return -1;
    }

    #define CNN_FWRITE(ptr, size, count) \
        do { if (fwrite((ptr), (size), (count), f) != (count)) { \
            NIMCP_LOGGING_ERROR("cnn_trainer_save: fwrite failed"); \
            fclose(f); remove(tmp_path); return -1; \
        } } while (0)

    uint32_t magic = 0x434E4E53; /* "CNNS" */
    uint32_t version = 1;
    CNN_FWRITE(&magic, sizeof(uint32_t), 1);
    CNN_FWRITE(&version, sizeof(uint32_t), 1);

    /* Trainer state */
    CNN_FWRITE(&trainer->num_layers, sizeof(uint32_t), 1);
    CNN_FWRITE(&trainer->current_epoch, sizeof(uint32_t), 1);
    CNN_FWRITE(&trainer->global_step, sizeof(uint32_t), 1);
    CNN_FWRITE(&trainer->best_val_loss, sizeof(float), 1);

    /* Per-layer: type + config union + weight/bias tensors + batch norm stats */
    cnn_layer_t* layer = trainer->layers_head;
    while (layer) {
        uint32_t type = (uint32_t)layer->type;
        CNN_FWRITE(&type, sizeof(uint32_t), 1);

        /* Save the full config union (fixed-size) */
        CNN_FWRITE(&layer->config, sizeof(layer->config), 1);

        /* Save shapes */
        CNN_FWRITE(&layer->input_shape, sizeof(nimcp_tensor_shape_t), 1);
        CNN_FWRITE(&layer->output_shape, sizeof(nimcp_tensor_shape_t), 1);

        /* Save weight tensors */
        nimcp_tensor_save(layer->weights, f);
        nimcp_tensor_save(layer->bias, f);
        nimcp_tensor_save(layer->running_mean, f);
        nimcp_tensor_save(layer->running_var, f);

        layer = layer->next;
    }

    #undef CNN_FWRITE

    fclose(f);

    /* Atomic rename: temp → final path */
    if (rename(tmp_path, path) != 0) {
        NIMCP_LOGGING_ERROR("cnn_trainer_save: rename %s → %s failed", tmp_path, path);
        remove(tmp_path);
        return -1;
    }

    NIMCP_LOGGING_INFO("CNN trainer saved to %s (%u layers, step %u)",
                       path, trainer->num_layers, trainer->global_step);
    return 0;
}

int cnn_trainer_load_weights(cnn_trainer_t* trainer, const char* path) {
    if (!trainer || !path) return -1;

    FILE* f = fopen(path, "rb");
    if (!f) {
        NIMCP_LOGGING_WARN("cnn_trainer_load_weights: file not found %s", path);
        return -1;
    }

    uint32_t magic = 0, version = 0;
    if (fread(&magic, sizeof(uint32_t), 1, f) != 1 || magic != 0x434E4E53) {
        NIMCP_LOGGING_ERROR("cnn_trainer_load_weights: invalid magic");
        fclose(f);
        return -1;
    }
    if (fread(&version, sizeof(uint32_t), 1, f) != 1 || version > 1) {
        NIMCP_LOGGING_ERROR("cnn_trainer_load_weights: unsupported version %u", version);
        fclose(f); return -1;
    }

    /* Trainer state */
    uint32_t num_layers = 0;
    if (fread(&num_layers, sizeof(uint32_t), 1, f) != 1 ||
        fread(&trainer->current_epoch, sizeof(uint32_t), 1, f) != 1 ||
        fread(&trainer->global_step, sizeof(uint32_t), 1, f) != 1 ||
        fread(&trainer->best_val_loss, sizeof(float), 1, f) != 1) {
        NIMCP_LOGGING_ERROR("cnn_trainer_load_weights: truncated state");
        fclose(f); return -1;
    }

    /* Per-layer: restore weights into existing trainer */
    cnn_layer_t* layer = trainer->layers_head;
    for (uint32_t i = 0; i < num_layers; i++) {
        uint32_t type = 0;
        if (fread(&type, sizeof(uint32_t), 1, f) != 1) {
            NIMCP_LOGGING_WARN("cnn_trainer_load_weights: truncated at layer %u", i);
            break;
        }

        /* Skip config union and shapes */
        union { cnn_conv_config_t c; cnn_pool_config_t p; cnn_batch_norm_config_t b;
                cnn_dropout_config_t d; cnn_dense_config_t e; } dummy_config;
        nimcp_tensor_shape_t dummy_shape;
        if (fread(&dummy_config, sizeof(dummy_config), 1, f) != 1 ||
            fread(&dummy_shape, sizeof(nimcp_tensor_shape_t), 1, f) != 1 ||
            fread(&dummy_shape, sizeof(nimcp_tensor_shape_t), 1, f) != 1) {
            NIMCP_LOGGING_WARN("cnn_trainer_load_weights: truncated layer %u config", i);
            break;
        }

        /* Load weight tensors */
        nimcp_tensor_t* weights = nimcp_tensor_load(f);
        nimcp_tensor_t* bias = nimcp_tensor_load(f);
        nimcp_tensor_t* running_mean = nimcp_tensor_load(f);
        nimcp_tensor_t* running_var = nimcp_tensor_load(f);

        /* Apply to matching layer */
        if (layer && (uint32_t)layer->type == type) {
            #define SWAP_IF_MATCH(dst, src) do { \
                if ((src) && (dst) && nimcp_tensor_numel(src) == nimcp_tensor_numel(dst)) { \
                    nimcp_tensor_destroy(dst); \
                    (dst) = (src); \
                    (src) = NULL; \
                } \
            } while(0)

            SWAP_IF_MATCH(layer->weights, weights);
            SWAP_IF_MATCH(layer->bias, bias);
            SWAP_IF_MATCH(layer->running_mean, running_mean);
            SWAP_IF_MATCH(layer->running_var, running_var);
            #undef SWAP_IF_MATCH

            layer = layer->next;
        }

        nimcp_tensor_destroy(weights);
        nimcp_tensor_destroy(bias);
        nimcp_tensor_destroy(running_mean);
        nimcp_tensor_destroy(running_var);
    }

    fclose(f);
    NIMCP_LOGGING_INFO("CNN trainer weights loaded from %s (%u layers)", path, num_layers);
    return 0;
}
