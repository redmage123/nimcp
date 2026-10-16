/**
 * @file nimcp_brain_training_integration.h
 * @brief Brain-Training Integration Module for NIMCP
 *
 * Phase TM-3: Integrates training modules (Loss Functions, Optimizers) with
 * the brain system and security framework.
 *
 * ARCHITECTURE:
 * +------------------------------------------------------------------+
 * |                    Brain-Training Integration                     |
 * +------------------------------------------------------------------+
 * |  Training Context  |  Event Handlers  |  Security Registration   |
 * +------------------------------------------------------------------+
 * |    Loss Functions  |    Optimizers    |    Event Bus Callbacks   |
 * +------------------------------------------------------------------+
 * |                      Brain Subsystems                            |
 * |  (Plasticity)  (Weight Updates)  (Learning Signals)              |
 * +------------------------------------------------------------------+
 *
 * Features:
 * - Automatic security registration for training modules
 * - Event bus integration for training events
 * - Connection to brain's plasticity system
 * - Weight update routing through training adapters
 * - Statistics tracking and monitoring
 *
 * @version 1.0.0
 * @author NIMCP Development Team
 */

#ifndef NIMCP_BRAIN_TRAINING_INTEGRATION_H
#define NIMCP_BRAIN_TRAINING_INTEGRATION_H

#include "middleware/training/nimcp_loss_functions.h"
#include "middleware/training/nimcp_optimizers.h"
#include "middleware/training/nimcp_lr_scheduler.h"
#include "middleware/training/nimcp_regularization.h"
#include "middleware/training/nimcp_gradient_manager.h"
#include "middleware/training/nimcp_training_callbacks.h"
#include "security/nimcp_security_integration.h"
#include "utils/validation/nimcp_common.h"
#include "utils/platform/nimcp_platform_tier.h"  /* For platform_tier_t (Portia integration) */

/* Forward declaration for plasticity bridge (Phase TPB-1) */
typedef struct tpb_context tpb_context_t;

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants and Configuration
 * ============================================================================ */

#define NIMCP_TRAINING_MAX_LOSS_CONTEXTS       16
#define NIMCP_TRAINING_MAX_OPTIMIZER_CONTEXTS  16
#define NIMCP_TRAINING_MAX_SCHEDULER_CONTEXTS  16
#define NIMCP_TRAINING_MAX_REGULARIZER_CONTEXTS 16
#define NIMCP_TRAINING_MAX_GRADMGR_CONTEXTS    16
#define NIMCP_TRAINING_MAX_PARAM_GROUPS        64

/* Training-specific error codes (8100-8199 range within cognitive errors) */
#define NIMCP_TRAINING_ERROR_GRAD_NAN    8100  /**< NaN detected in gradients */
#define NIMCP_TRAINING_ERROR_GRAD_INF    8101  /**< Infinity detected in gradients */
#define NIMCP_TRAINING_ERROR_DIVERGED    8102  /**< Training has diverged */
#define NIMCP_TRAINING_ERROR_EARLY_STOP  8103  /**< Early stopping triggered */

/**
 * @brief Training event types for event bus
 */
typedef enum nimcp_training_event_type {
    NIMCP_TRAINING_EVENT_NONE = 0,
    NIMCP_TRAINING_EVENT_EPOCH_START,      /**< Training epoch started */
    NIMCP_TRAINING_EVENT_EPOCH_END,        /**< Training epoch completed */
    NIMCP_TRAINING_EVENT_BATCH_START,      /**< Batch processing started */
    NIMCP_TRAINING_EVENT_BATCH_END,        /**< Batch processing completed */
    NIMCP_TRAINING_EVENT_LOSS_COMPUTED,    /**< Loss value computed */
    NIMCP_TRAINING_EVENT_GRADIENTS_READY,  /**< Gradients computed */
    NIMCP_TRAINING_EVENT_WEIGHTS_UPDATED,  /**< Weights updated */
    NIMCP_TRAINING_EVENT_LR_CHANGED,       /**< Learning rate changed */
    NIMCP_TRAINING_EVENT_CONVERGENCE,      /**< Training converged */
    NIMCP_TRAINING_EVENT_DIVERGENCE,       /**< Training diverged (loss explosion) */
    NIMCP_TRAINING_EVENT_GRAD_CLIPPED,     /**< Gradients were clipped */
    NIMCP_TRAINING_EVENT_GRAD_ACCUMULATED, /**< Gradients accumulated */
    NIMCP_TRAINING_EVENT_REGULARIZED,      /**< Regularization applied */
    NIMCP_TRAINING_EVENT_EARLY_STOP,       /**< Early stopping triggered */
    NIMCP_TRAINING_EVENT_COUNT
} nimcp_training_event_type_t;

/**
 * @brief Training mode
 */
typedef enum nimcp_training_mode {
    NIMCP_TRAINING_MODE_TRAIN = 0,   /**< Training mode (gradients enabled) */
    NIMCP_TRAINING_MODE_EVAL,         /**< Evaluation mode (gradients disabled) */
    NIMCP_TRAINING_MODE_INFERENCE     /**< Inference mode (minimal overhead) */
} nimcp_training_mode_t;

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/**
 * @brief Training event data structure
 */
typedef struct nimcp_training_event {
    nimcp_training_event_type_t type;  /**< Event type */
    uint64_t timestamp;                 /**< Event timestamp (ns) */
    uint64_t epoch;                     /**< Current epoch */
    uint64_t batch;                     /**< Current batch */
    float loss_value;                   /**< Loss value (if applicable) */
    float learning_rate;                /**< Current learning rate */
    float gradient_norm;                /**< Gradient norm (if applicable) */
    float regularization_loss;          /**< Regularization loss (if applicable) */
    float clip_ratio;                   /**< Gradient clip ratio (if applicable) */
    uint32_t accum_step;                /**< Current accumulation step */
    void* user_data;                    /**< User-defined data */
} nimcp_training_event_t;

/**
 * @brief Training event callback function type
 */
typedef void (*nimcp_training_event_callback_t)(
    const nimcp_training_event_t* event,
    void* user_data
);

/**
 * @brief Brain-Training integration configuration
 */
typedef struct nimcp_brain_training_config {
    /* Loss function defaults */
    nimcp_loss_type_t default_loss_type;       /**< Default loss function type */
    nimcp_loss_reduction_t default_reduction;  /**< Default reduction mode */

    /* Optimizer defaults */
    nimcp_optimizer_type_t default_optimizer;  /**< Default optimizer type */
    float default_learning_rate;               /**< Default learning rate */

    /* Learning rate scheduler defaults */
    nimcp_lr_scheduler_type_t default_lr_scheduler; /**< Default LR scheduler type */
    bool enable_lr_scheduler;                  /**< Enable LR scheduling */

    /* Regularization defaults */
    nimcp_reg_type_t default_reg_type;         /**< Default regularization type */
    float default_reg_lambda;                  /**< Default regularization strength */
    float l1_lambda;                           /**< L1 regularization strength */
    float l2_lambda;                           /**< L2 regularization strength */
    float dropout_rate;                        /**< Dropout rate during training */
    nimcp_clip_mode_t default_clip_mode;       /**< Default gradient clipping mode */
    float default_clip_value;                  /**< Default gradient clip threshold */
    float gradient_clip_norm;                  /**< Max gradient norm for clipping */
    bool enable_regularization;                /**< Enable regularization */
    bool enable_gradient_clipping;             /**< Enable gradient clipping */

    /* Gradient management defaults */
    uint32_t gradient_accum_steps;             /**< Gradient accumulation steps (1 = no accumulation) */
    uint32_t gradient_accumulation_steps;      /**< Alias for gradient_accum_steps */
    float gradient_clip_value;                 /**< Max gradient value for clipping */
    bool enable_gradient_management;           /**< Enable gradient management module */
    bool enable_gradient_scaling;              /**< Enable dynamic gradient scaling */
    bool enable_gradient_health_check;         /**< Check for NaN/Inf in gradients */

    /* Early stopping */
    bool enable_early_stopping;                /**< Enable early stopping */
    uint32_t early_stop_patience;              /**< Patience for early stopping */
    float early_stop_min_delta;                /**< Minimum improvement delta */

    /* Training parameters */
    uint32_t max_epochs;                       /**< Maximum training epochs */
    uint32_t batch_size;                       /**< Default batch size */
    float convergence_threshold;               /**< Loss change threshold for convergence */
    float divergence_threshold;                /**< Loss threshold for divergence detection */

    /* Security integration */
    bool enable_security;                      /**< Enable security integration */
    bool register_with_bbb;                    /**< Register with Blood-Brain Barrier */

    /* Event callbacks */
    nimcp_training_event_callback_t event_callback; /**< Event notification callback */
    void* callback_user_data;                  /**< User data for callback */

    /* Memory management */
    bool use_memory_pool;                      /**< Use unified memory manager */
    unified_mem_strategy_t cow_strategy;       /**< CoW strategy */

    /* Plasticity Bridge integration (Phase TPB-1) */
    bool enable_plasticity_bridge;             /**< Enable Training-Plasticity Bridge */
    float rpe_to_da_gain;                      /**< RPE to dopamine conversion gain */
    float biological_lr_modulation;            /**< Strength of biological LR modulation (0-1) */

    /* Training Callbacks integration (Phase TCB-1) */
    bool enable_training_callbacks;            /**< Enable training callback system */
    tcb_config_t* callback_config;             /**< Callback configuration (NULL for defaults) */

    /* Second Messenger Cascade integration */
    bool enable_second_messengers;             /**< Enable second messenger cascade modulation */
    uint32_t num_neurons;                      /**< Number of neurons for second messenger system */

    /* Portia Resource Management integration */
    bool enable_portia_integration;            /**< Enable Portia resource-aware training */
    float min_batch_size_ratio;                /**< Minimum batch size ratio (0-1, def: 0.25) */
    bool allow_training_pause;                 /**< Allow pause in EMERGENCY mode (def: true) */
    bool adapt_to_tier_changes;                /**< Automatically adapt to tier changes (def: true) */
} nimcp_brain_training_config_t;

/**
 * @brief Brain-Training integration context (opaque)
 */
typedef struct nimcp_brain_training_ctx nimcp_brain_training_ctx_t;

/**
 * @brief Training session statistics
 */
typedef struct nimcp_training_session_stats {
    uint64_t total_epochs;            /**< Total epochs completed */
    uint64_t total_batches;           /**< Total batches processed */
    uint64_t total_samples;           /**< Total samples processed */
    double total_loss;                /**< Cumulative loss */
    double min_loss;                  /**< Minimum loss achieved */
    double max_loss;                  /**< Maximum loss encountered */
    double avg_loss;                  /**< Average loss */
    double loss_variance;             /**< Loss variance */
    double initial_lr;                /**< Initial learning rate */
    double final_lr;                  /**< Final learning rate */
    uint64_t weight_updates;          /**< Total weight updates */
    uint64_t gradient_clips;          /**< Total gradient clips */
    uint64_t training_time_ns;        /**< Total training time */
    bool converged;                   /**< Training converged */
    bool diverged;                    /**< Training diverged */

    /* LR Scheduler stats */
    uint64_t lr_steps;                /**< Number of LR scheduler steps */
    float lr_min;                     /**< Minimum LR achieved */
    float lr_max;                     /**< Maximum LR seen */

    /* Regularization stats */
    double total_reg_loss;            /**< Cumulative regularization loss */
    uint64_t dropout_masks;           /**< Number of dropout masks generated */

    /* Gradient management stats */
    uint64_t grad_accum_steps;        /**< Total gradient accumulation steps */
    uint64_t grad_nan_count;          /**< Number of NaN gradient detections */
    uint64_t grad_inf_count;          /**< Number of Inf gradient detections */
    uint64_t grad_scale_updates;      /**< Number of gradient scale adjustments */
    float current_grad_scale;         /**< Current gradient scale factor */

    /* Early stopping */
    uint32_t early_stop_wait_count;   /**< Current early stopping wait count */
    float best_loss;                  /**< Best loss for early stopping */
    bool early_stopped;               /**< Whether early stopping triggered */

    /* Plasticity Bridge stats (Phase TPB-1) */
    uint64_t rpe_computations;        /**< Number of RPE computations */
    uint64_t biological_updates;      /**< Weight updates routed through plasticity */
    float avg_dopamine_level;         /**< Average dopamine level during training */
    float avg_lr_modulation;          /**< Average learning rate modulation factor */

    /* Training Callbacks stats (Phase TCB-1) */
    uint64_t callback_events_fired;   /**< Total callback events fired */
    uint64_t callback_stop_requests;  /**< Times callback requested stop */
    uint64_t callback_lr_reductions;  /**< Times callback requested LR reduction */
    uint64_t callback_rollbacks;      /**< Times callback requested rollback */
} nimcp_training_session_stats_t;

/* ============================================================================
 * Default Configuration
 * ============================================================================ */

/**
 * @brief Get default brain-training integration configuration
 * @return Default configuration
 */
nimcp_brain_training_config_t nimcp_brain_training_default_config(void);

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

/**
 * @brief Create brain-training integration context
 * @param config Configuration (NULL for defaults)
 * @return Context or NULL on failure
 */
nimcp_brain_training_ctx_t* nimcp_brain_training_create(
    const nimcp_brain_training_config_t* config
);

/**
 * @brief Initialize brain-training integration
 *
 * Registers training modules with security system and sets up event handlers.
 *
 * @param ctx Brain-training context
 * @param security_ctx Security integration context (optional)
 * @param memory_mgr Memory manager (optional)
 * @return NIMCP_SUCCESS on success
 */
nimcp_result_t nimcp_brain_training_init(
    nimcp_brain_training_ctx_t* ctx,
    nimcp_sec_integration_t* security_ctx,
    unified_mem_manager_t memory_mgr
);

/**
 * @brief Destroy brain-training integration context
 * @param ctx Context to destroy
 */
void nimcp_brain_training_destroy(nimcp_brain_training_ctx_t* ctx);

/* ============================================================================
 * Security Registration
 * ============================================================================ */

/**
 * @brief Register training modules with security system
 *
 * Registers loss functions and optimizers modules with the NIMCP
 * security integration framework (Phase SC-4).
 *
 * @param ctx Brain-training context
 * @param security_ctx Security integration context
 * @return NIMCP_SUCCESS on success
 */
nimcp_result_t nimcp_brain_training_register_security(
    nimcp_brain_training_ctx_t* ctx,
    nimcp_sec_integration_t* security_ctx
);

/**
 * @brief Unregister training modules from security system
 * @param ctx Brain-training context
 * @return NIMCP_SUCCESS on success
 */
nimcp_result_t nimcp_brain_training_unregister_security(
    nimcp_brain_training_ctx_t* ctx
);

/**
 * @brief Check if training modules are registered with security
 * @param ctx Brain-training context
 * @return true if registered
 */
bool nimcp_brain_training_is_security_registered(
    const nimcp_brain_training_ctx_t* ctx
);

/* ============================================================================
 * Loss Function Management
 * ============================================================================ */

/**
 * @brief Create and register a loss function context
 * @param ctx Brain-training context
 * @param config Loss function configuration
 * @param loss_id Output: assigned loss context ID
 * @return NIMCP_SUCCESS on success
 */
nimcp_result_t nimcp_brain_training_create_loss(
    nimcp_brain_training_ctx_t* ctx,
    const nimcp_loss_config_t* config,
    uint32_t* loss_id
);

/**
 * @brief Get loss function context by ID
 * @param ctx Brain-training context
 * @param loss_id Loss context ID
 * @return Loss context or NULL if not found
 */
nimcp_loss_context_t* nimcp_brain_training_get_loss(
    nimcp_brain_training_ctx_t* ctx,
    uint32_t loss_id
);

/**
 * @brief Destroy a loss function context
 * @param ctx Brain-training context
 * @param loss_id Loss context ID
 * @return NIMCP_SUCCESS on success
 */
nimcp_result_t nimcp_brain_training_destroy_loss(
    nimcp_brain_training_ctx_t* ctx,
    uint32_t loss_id
);

/* ============================================================================
 * Optimizer Management
 * ============================================================================ */

/**
 * @brief Create and register an optimizer context
 * @param ctx Brain-training context
 * @param config Optimizer configuration
 * @param optimizer_id Output: assigned optimizer ID
 * @return NIMCP_SUCCESS on success
 */
nimcp_result_t nimcp_brain_training_create_optimizer(
    nimcp_brain_training_ctx_t* ctx,
    const nimcp_optimizer_config_t* config,
    uint32_t* optimizer_id
);

/**
 * @brief Get optimizer context by ID
 * @param ctx Brain-training context
 * @param optimizer_id Optimizer ID
 * @return Optimizer context or NULL if not found
 */
nimcp_optimizer_context_t* nimcp_brain_training_get_optimizer(
    nimcp_brain_training_ctx_t* ctx,
    uint32_t optimizer_id
);

/**
 * @brief Get the number of active optimizer contexts
 * @param ctx Brain-training context
 * @return Number of active optimizers (0 if ctx is NULL or no optimizers)
 */
uint32_t nimcp_brain_training_get_optimizer_count(
    nimcp_brain_training_ctx_t* ctx
);

/**
 * @brief Destroy an optimizer context
 * @param ctx Brain-training context
 * @param optimizer_id Optimizer ID
 * @return NIMCP_SUCCESS on success
 */
nimcp_result_t nimcp_brain_training_destroy_optimizer(
    nimcp_brain_training_ctx_t* ctx,
    uint32_t optimizer_id
);

/* ============================================================================
 * Learning Rate Scheduler Management
 * ============================================================================ */

/**
 * @brief Create and register a learning rate scheduler
 * @param ctx Brain-training context
 * @param config LR scheduler configuration
 * @param scheduler_id Output: assigned scheduler ID
 * @return NIMCP_SUCCESS on success
 */
nimcp_result_t nimcp_brain_training_create_scheduler(
    nimcp_brain_training_ctx_t* ctx,
    const nimcp_lr_scheduler_config_t* config,
    uint32_t* scheduler_id
);

/**
 * @brief Get LR scheduler context by ID
 * @param ctx Brain-training context
 * @param scheduler_id Scheduler ID
 * @return Scheduler context or NULL if not found
 */
nimcp_lr_scheduler_ctx_t* nimcp_brain_training_get_scheduler(
    nimcp_brain_training_ctx_t* ctx,
    uint32_t scheduler_id
);

/**
 * @brief Destroy a LR scheduler context
 * @param ctx Brain-training context
 * @param scheduler_id Scheduler ID
 * @return NIMCP_SUCCESS on success
 */
nimcp_result_t nimcp_brain_training_destroy_scheduler(
    nimcp_brain_training_ctx_t* ctx,
    uint32_t scheduler_id
);

/**
 * @brief Step the LR scheduler and update optimizer learning rate
 * @param ctx Brain-training context
 * @param scheduler_id Scheduler ID
 * @param optimizer_id Optimizer ID to update
 * @return New learning rate
 */
float nimcp_brain_training_step_scheduler(
    nimcp_brain_training_ctx_t* ctx,
    uint32_t scheduler_id,
    uint32_t optimizer_id
);

/**
 * @brief Step scheduler with validation metric (for ReduceOnPlateau)
 * @param ctx Brain-training context
 * @param scheduler_id Scheduler ID
 * @param optimizer_id Optimizer ID to update
 * @param metric Validation metric value
 * @return New learning rate
 */
float nimcp_brain_training_step_scheduler_metric(
    nimcp_brain_training_ctx_t* ctx,
    uint32_t scheduler_id,
    uint32_t optimizer_id,
    float metric
);

/* ============================================================================
 * Gradient Management
 * ============================================================================ */

/**
 * @brief Create and register a gradient manager
 * @param ctx Brain-training context
 * @param config Gradient manager configuration
 * @param gradmgr_id Output: assigned gradient manager ID
 * @return NIMCP_SUCCESS on success
 */
nimcp_result_t nimcp_brain_training_create_gradient_manager(
    nimcp_brain_training_ctx_t* ctx,
    const nimcp_gradient_manager_config_t* config,
    uint32_t* gradmgr_id
);

/**
 * @brief Get gradient manager context by ID
 * @param ctx Brain-training context
 * @param gradmgr_id Gradient manager ID
 * @return Gradient manager context or NULL if not found
 */
nimcp_gradient_manager_ctx_t* nimcp_brain_training_get_gradient_manager(
    nimcp_brain_training_ctx_t* ctx,
    uint32_t gradmgr_id
);

/**
 * @brief Destroy a gradient manager context
 * @param ctx Brain-training context
 * @param gradmgr_id Gradient manager ID
 * @return NIMCP_SUCCESS on success
 */
nimcp_result_t nimcp_brain_training_destroy_gradient_manager(
    nimcp_brain_training_ctx_t* ctx,
    uint32_t gradmgr_id
);

/**
 * @brief Accumulate gradients using gradient manager
 * @param ctx Brain-training context
 * @param gradmgr_id Gradient manager ID
 * @param gradients Gradients to accumulate
 * @param count Number of gradients
 * @return NIMCP_SUCCESS on success
 */
nimcp_result_t nimcp_brain_training_accumulate_gradients(
    nimcp_brain_training_ctx_t* ctx,
    uint32_t gradmgr_id,
    const float* gradients,
    size_t count
);

/**
 * @brief Check if gradient accumulation is complete
 * @param ctx Brain-training context
 * @param gradmgr_id Gradient manager ID
 * @return true if ready to apply accumulated gradients
 */
bool nimcp_brain_training_gradients_ready(
    const nimcp_brain_training_ctx_t* ctx,
    uint32_t gradmgr_id
);

/**
 * @brief Get and reset accumulated gradients
 * @param ctx Brain-training context
 * @param gradmgr_id Gradient manager ID
 * @param output Output gradient array
 * @param count Number of gradients
 * @return NIMCP_SUCCESS on success
 */
nimcp_result_t nimcp_brain_training_get_accumulated_gradients(
    nimcp_brain_training_ctx_t* ctx,
    uint32_t gradmgr_id,
    float* output,
    size_t count
);

/* ============================================================================
 * Regularization Operations
 * ============================================================================ */

/**
 * @brief Apply L1/L2 regularization to weights
 * @param ctx Brain-training context
 * @param weights Weight array
 * @param gradients Gradient array (modified with regularization gradients)
 * @param count Number of weights
 * @param reg_type Regularization type
 * @param lambda Regularization strength
 * @return Regularization loss value
 */
float nimcp_brain_training_apply_regularization(
    nimcp_brain_training_ctx_t* ctx,
    const float* weights,
    float* gradients,
    size_t count,
    nimcp_reg_type_t reg_type,
    float lambda
);

/**
 * @brief Clip gradients by value, norm, or global norm
 * @param ctx Brain-training context
 * @param gradients Gradient array (modified in place)
 * @param count Number of gradients
 * @param mode Clipping mode
 * @param threshold Clip threshold
 * @return Clip ratio (original_norm / clipped_norm)
 */
float nimcp_brain_training_clip_gradients(
    nimcp_brain_training_ctx_t* ctx,
    float* gradients,
    size_t count,
    nimcp_clip_mode_t mode,
    float threshold
);

/**
 * @brief Apply dropout to activations
 * @param ctx Brain-training context
 * @param input Input activations
 * @param output Output activations (can be same as input)
 * @param mask Dropout mask (optional, generated if NULL)
 * @param count Number of elements
 * @param dropout_rate Dropout probability
 * @return Number of elements dropped
 */
uint64_t nimcp_brain_training_apply_dropout(
    nimcp_brain_training_ctx_t* ctx,
    const float* input,
    float* output,
    bool* mask,
    size_t count,
    float dropout_rate
);

/**
 * @brief Check early stopping condition
 * @param ctx Brain-training context
 * @param current_loss Current validation loss
 * @return true if training should stop
 */
bool nimcp_brain_training_check_early_stop(
    nimcp_brain_training_ctx_t* ctx,
    float current_loss
);

/**
 * @brief Reset early stopping state
 * @param ctx Brain-training context
 */
void nimcp_brain_training_reset_early_stop(nimcp_brain_training_ctx_t* ctx);

/* ============================================================================
 * Enhanced Training Operations
 * ============================================================================ */

/**
 * @brief Complete training step with all integrated modules
 *
 * This function performs a full training step with:
 * - Loss computation and backward pass
 * - Gradient health checking (NaN/Inf detection)
 * - Gradient accumulation (if enabled)
 * - Gradient clipping (if enabled)
 * - Weight regularization (if enabled)
 * - Optimization step
 * - Learning rate scheduling (if enabled)
 * - Early stopping check (if enabled)
 *
 * @param ctx Brain-training context
 * @param loss_id Loss context ID
 * @param optimizer_id Optimizer ID
 * @param scheduler_id LR scheduler ID (0 to skip)
 * @param gradmgr_id Gradient manager ID (0 to skip)
 * @param params Parameters to update
 * @param predictions Model predictions
 * @param targets Ground truth targets
 * @param batch_size Batch size
 * @param output_size Output dimension
 * @param param_count Number of parameters
 * @param loss_value Output: loss value (includes regularization)
 * @return NIMCP_SUCCESS on success, NIMCP_EARLY_STOP if early stopping triggered
 */
nimcp_result_t nimcp_brain_training_step_full(
    nimcp_brain_training_ctx_t* ctx,
    uint32_t loss_id,
    uint32_t optimizer_id,
    uint32_t scheduler_id,
    uint32_t gradmgr_id,
    float* params,
    const float* predictions,
    const float* targets,
    size_t batch_size,
    size_t output_size,
    size_t param_count,
    float* loss_value
);

/* ============================================================================
 * Training Operations
 * ============================================================================ */

/**
 * @brief Set training mode
 * @param ctx Brain-training context
 * @param mode Training mode
 * @return NIMCP_SUCCESS on success
 */
nimcp_result_t nimcp_brain_training_set_mode(
    nimcp_brain_training_ctx_t* ctx,
    nimcp_training_mode_t mode
);

/**
 * @brief Get current training mode
 * @param ctx Brain-training context
 * @return Current training mode
 */
nimcp_training_mode_t nimcp_brain_training_get_mode(
    const nimcp_brain_training_ctx_t* ctx
);

/**
 * @brief Compute loss and gradients
 * @param ctx Brain-training context
 * @param loss_id Loss context ID
 * @param predictions Model predictions
 * @param targets Ground truth targets
 * @param batch_size Batch size
 * @param output_size Output dimension
 * @param loss_value Output: loss value
 * @param gradients Output: gradients (pre-allocated)
 * @return NIMCP_SUCCESS on success
 */
nimcp_result_t nimcp_brain_training_compute_loss(
    nimcp_brain_training_ctx_t* ctx,
    uint32_t loss_id,
    const float* predictions,
    const float* targets,
    size_t batch_size,
    size_t output_size,
    float* loss_value,
    float* gradients
);

/**
 * @brief Perform optimization step
 * @param ctx Brain-training context
 * @param optimizer_id Optimizer ID
 * @param params Parameters to update
 * @param gradients Gradients for update
 * @param count Number of parameters
 * @return NIMCP_SUCCESS on success
 */
nimcp_result_t nimcp_brain_training_optimize(
    nimcp_brain_training_ctx_t* ctx,
    uint32_t optimizer_id,
    float* params,
    const float* gradients,
    size_t count
);

/**
 * @brief Complete training step (loss + optimize)
 * @param ctx Brain-training context
 * @param loss_id Loss context ID
 * @param optimizer_id Optimizer ID
 * @param params Parameters to update (size: param_count)
 * @param predictions Model predictions (size: batch_size * output_size)
 * @param targets Ground truth targets (size: batch_size * output_size)
 * @param batch_size Batch size
 * @param output_size Output dimension (for loss computation)
 * @param param_count Number of parameters (for optimization)
 * @param loss_value Output: loss value
 * @return NIMCP_SUCCESS on success
 */
nimcp_result_t nimcp_brain_training_step(
    nimcp_brain_training_ctx_t* ctx,
    uint32_t loss_id,
    uint32_t optimizer_id,
    float* params,
    const float* predictions,
    const float* targets,
    size_t batch_size,
    size_t output_size,
    size_t param_count,
    float* loss_value
);

/* ============================================================================
 * Event Bus Integration
 * ============================================================================ */

/**
 * @brief Emit training event to event bus
 * @param ctx Brain-training context
 * @param event Training event to emit
 * @return NIMCP_SUCCESS on success
 */
nimcp_result_t nimcp_brain_training_emit_event(
    nimcp_brain_training_ctx_t* ctx,
    const nimcp_training_event_t* event
);

/**
 * @brief Register event callback
 * @param ctx Brain-training context
 * @param callback Callback function
 * @param user_data User data for callback
 * @return NIMCP_SUCCESS on success
 */
nimcp_result_t nimcp_brain_training_register_callback(
    nimcp_brain_training_ctx_t* ctx,
    nimcp_training_event_callback_t callback,
    void* user_data
);

/* ============================================================================
 * Statistics and Monitoring
 * ============================================================================ */

/**
 * @brief Get training session statistics
 * @param ctx Brain-training context
 * @param stats Output: statistics structure
 * @return NIMCP_SUCCESS on success
 */
nimcp_result_t nimcp_brain_training_get_stats(
    const nimcp_brain_training_ctx_t* ctx,
    nimcp_training_session_stats_t* stats
);

/**
 * @brief Reset training session statistics
 * @param ctx Brain-training context
 */
void nimcp_brain_training_reset_stats(nimcp_brain_training_ctx_t* ctx);

/**
 * @brief Update training session statistics
 *
 * Called by training loop to update stats after each step.
 *
 * @param ctx Brain-training context
 * @param samples_processed Number of samples processed in this step
 * @param loss_value Loss value for this step
 */
void nimcp_brain_training_update_stats(
    nimcp_brain_training_ctx_t* ctx,
    uint32_t samples_processed,
    float loss_value);

/**
 * @brief Check if training has converged
 * @param ctx Brain-training context
 * @return true if converged
 */
bool nimcp_brain_training_is_converged(const nimcp_brain_training_ctx_t* ctx);

/**
 * @brief Check if training has diverged
 * @param ctx Brain-training context
 * @return true if diverged
 */
bool nimcp_brain_training_is_diverged(const nimcp_brain_training_ctx_t* ctx);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Get training event type name
 * @param type Event type
 * @return String name
 */
const char* nimcp_training_event_type_name(nimcp_training_event_type_t type);

/**
 * @brief Get training mode name
 * @param mode Training mode
 * @return String name
 */
const char* nimcp_training_mode_name(nimcp_training_mode_t mode);

/**
 * @brief Validate brain-training configuration
 * @param config Configuration to validate
 * @return NIMCP_SUCCESS if valid
 */
nimcp_result_t nimcp_brain_training_validate_config(
    const nimcp_brain_training_config_t* config
);

/**
 * @brief Get security module IDs for training modules
 * @param ctx Brain-training context
 * @param loss_module_id Output: loss functions module ID
 * @param optimizer_module_id Output: optimizers module ID
 * @return NIMCP_SUCCESS on success
 */
nimcp_result_t nimcp_brain_training_get_security_ids(
    const nimcp_brain_training_ctx_t* ctx,
    uint32_t* loss_module_id,
    uint32_t* optimizer_module_id
);

/* ============================================================================
 * Plasticity Bridge Integration (Phase TPB-1)
 * ============================================================================ */

/**
 * @brief Connect plasticity bridge to training integration
 *
 * This connects the training module to the biological plasticity system,
 * enabling neuromodulator-based learning rate modulation and reward
 * prediction error computation.
 *
 * @param ctx Brain-training context
 * @param bridge Plasticity bridge context
 * @return NIMCP_SUCCESS on success
 */
nimcp_result_t nimcp_brain_training_connect_plasticity_bridge(
    nimcp_brain_training_ctx_t* ctx,
    tpb_context_t* bridge
);

/**
 * @brief Get connected plasticity bridge
 * @param ctx Brain-training context
 * @return Plasticity bridge context or NULL if not connected
 */
tpb_context_t* nimcp_brain_training_get_plasticity_bridge(
    const nimcp_brain_training_ctx_t* ctx
);

/**
 * @brief Perform biologically-modulated training step
 *
 * This function integrates biological modulation into the training loop:
 * 1. Computes loss and gradients
 * 2. Computes Reward Prediction Error (RPE) from loss change
 * 3. Modulates learning rate based on neuromodulator levels
 * 4. Routes weight updates through plasticity rules
 *
 * @param ctx Brain-training context
 * @param loss_id Loss context ID
 * @param optimizer_id Optimizer ID
 * @param params Parameters to update
 * @param predictions Model predictions
 * @param targets Ground truth targets
 * @param batch_size Batch size
 * @param output_size Output dimension
 * @param param_count Number of parameters
 * @param region_id Brain region ID for region-specific plasticity
 * @param loss_value Output: loss value
 * @return NIMCP_SUCCESS on success
 */
nimcp_result_t nimcp_brain_training_step_biological(
    nimcp_brain_training_ctx_t* ctx,
    uint32_t loss_id,
    uint32_t optimizer_id,
    float* params,
    const float* predictions,
    const float* targets,
    size_t batch_size,
    size_t output_size,
    size_t param_count,
    uint32_t region_id,
    float* loss_value
);

/**
 * @brief Set biological learning rate modulation strength
 * @param ctx Brain-training context
 * @param strength Modulation strength (0.0 = pure computational, 1.0 = full biological)
 * @return NIMCP_SUCCESS on success
 */
nimcp_result_t nimcp_brain_training_set_biological_modulation(
    nimcp_brain_training_ctx_t* ctx,
    float strength
);

/**
 * @brief Get current biological modulation strength
 * @param ctx Brain-training context
 * @return Current modulation strength
 */
float nimcp_brain_training_get_biological_modulation(
    const nimcp_brain_training_ctx_t* ctx
);

/* ============================================================================
 * Training Callbacks Integration (Phase TCB-1)
 * ============================================================================ */

/**
 * @brief Connect training callback context to training integration
 *
 * This enables the callback system to receive events during training and
 * allows callbacks to control training flow (stop, reduce LR, rollback, etc.)
 *
 * @param ctx Brain-training context
 * @param callback_ctx Training callback context
 * @return NIMCP_SUCCESS on success
 */
nimcp_result_t nimcp_brain_training_connect_callbacks(
    nimcp_brain_training_ctx_t* ctx,
    tcb_context_t* callback_ctx
);

/**
 * @brief Get connected callback context
 * @param ctx Brain-training context
 * @return Callback context or NULL if not connected
 */
tcb_context_t* nimcp_brain_training_get_callbacks(
    const nimcp_brain_training_ctx_t* ctx
);

/**
 * @brief Create internal callback context with default configuration
 *
 * Creates and connects a callback context using the config provided
 * during brain training creation. Use this if you want the training
 * integration to manage the callback lifecycle.
 *
 * @param ctx Brain-training context
 * @return NIMCP_SUCCESS on success
 */
nimcp_result_t nimcp_brain_training_create_callbacks(
    nimcp_brain_training_ctx_t* ctx
);

/**
 * @brief Register a TCB callback for training events
 *
 * Convenience function that registers a callback with the connected
 * callback context. Maps training events to callback event types.
 *
 * @param ctx Brain-training context
 * @param event_type Training event type to listen for
 * @param callback Callback function
 * @param user_data User data passed to callback
 * @param name Callback name for debugging
 * @return Callback ID or 0 on failure
 */
uint32_t nimcp_brain_training_register_tcb_callback(
    nimcp_brain_training_ctx_t* ctx,
    nimcp_training_event_type_t event_type,
    tcb_callback_fn callback,
    void* user_data,
    const char* name
);

/**
 * @brief Unregister a TCB training callback
 * @param ctx Brain-training context
 * @param callback_id Callback ID from registration
 * @return NIMCP_SUCCESS on success
 */
nimcp_result_t nimcp_brain_training_unregister_tcb_callback(
    nimcp_brain_training_ctx_t* ctx,
    uint32_t callback_id
);

/**
 * @brief Register built-in callbacks for common training patterns
 *
 * Registers standard callbacks for:
 * - Progress logging
 * - Early stopping
 * - Divergence detection
 * - Gradient monitoring
 *
 * @param ctx Brain-training context
 * @param enable_progress Enable progress logger
 * @param enable_early_stop Enable early stopping
 * @param enable_divergence Enable divergence detection
 * @param enable_gradient_monitor Enable gradient monitoring
 * @return NIMCP_SUCCESS on success
 */
nimcp_result_t nimcp_brain_training_register_builtin_callbacks(
    nimcp_brain_training_ctx_t* ctx,
    bool enable_progress,
    bool enable_early_stop,
    bool enable_divergence,
    bool enable_gradient_monitor
);

/**
 * @brief Handle callback action during training
 *
 * Internal function called after firing callbacks. Handles the returned
 * action by modifying training state (stopping, reducing LR, etc.)
 *
 * @param ctx Brain-training context
 * @param action Action returned from callback
 * @param optimizer_id Optimizer ID for LR modifications
 * @return NIMCP_SUCCESS if training should continue, error code otherwise
 */
nimcp_result_t nimcp_brain_training_handle_callback_action(
    nimcp_brain_training_ctx_t* ctx,
    tcb_action_t action,
    uint32_t optimizer_id
);

/**
 * @brief Perform training step with callback integration
 *
 * Like nimcp_brain_training_step but fires callback events at each stage
 * and respects callback actions (stop, reduce LR, rollback).
 *
 * Event sequence:
 * 1. TCB_EVENT_LOSS_COMPUTED (after loss computation)
 * 2. TCB_EVENT_GRADIENT_CLIPPED (after gradient computation, for monitoring)
 * 3. TCB_EVENT_WEIGHTS_UPDATED (after optimization step)
 * 4. TCB_EVENT_STEP_COMPLETE (after full step)
 *
 * @param ctx Brain-training context
 * @param loss_id Loss context ID
 * @param optimizer_id Optimizer ID
 * @param params Parameters to update
 * @param predictions Model predictions
 * @param targets Ground truth targets
 * @param batch_size Batch size
 * @param output_size Output dimension
 * @param param_count Number of parameters
 * @param step Current training step
 * @param loss_value Output: loss value
 * @return NIMCP_SUCCESS on success, NIMCP_TRAINING_ERROR_EARLY_STOP if stopped
 */
nimcp_result_t nimcp_brain_training_step_with_callbacks(
    nimcp_brain_training_ctx_t* ctx,
    uint32_t loss_id,
    uint32_t optimizer_id,
    float* params,
    const float* predictions,
    const float* targets,
    size_t batch_size,
    size_t output_size,
    size_t param_count,
    uint64_t step,
    float* loss_value
);

/**
 * @brief Signal epoch completion to callbacks
 *
 * Fires TCB_EVENT_EPOCH_COMPLETE and handles actions.
 *
 * @param ctx Brain-training context
 * @param epoch Epoch number
 * @param epoch_loss Average loss for the epoch
 * @return NIMCP_SUCCESS if training should continue
 */
nimcp_result_t nimcp_brain_training_signal_epoch_complete(
    nimcp_brain_training_ctx_t* ctx,
    uint64_t epoch,
    float epoch_loss
);

/**
 * @brief Create checkpoint through callback system
 *
 * Fires TCB_EVENT_CHECKPOINT and records the checkpoint.
 *
 * @param ctx Brain-training context
 * @param checkpoint_name Checkpoint name/path
 * @return NIMCP_SUCCESS on success
 */
nimcp_result_t nimcp_brain_training_checkpoint(
    nimcp_brain_training_ctx_t* ctx,
    const char* checkpoint_name
);

/* ============================================================================
 * Second Messenger Cascade Integration
 * ============================================================================ */

/**
 * @brief Connect second messenger system to training integration
 *
 * WHAT: Links second messenger cascade to training for plasticity modulation
 * WHY:  PKA/CaMKII/CREB activities modulate learning rates and consolidation
 * HOW:  Stores reference and enables cascade-based learning rate modulation
 *
 * @param ctx Brain-training context
 * @param second_messengers Second messenger system handle
 * @return NIMCP_SUCCESS on success
 */
nimcp_result_t nimcp_brain_training_connect_second_messengers(
    nimcp_brain_training_ctx_t* ctx,
    void* second_messengers
);

/**
 * @brief Get cascade modulation factor for neuron
 *
 * WHAT: Query current plasticity modulation from second messenger cascades
 * WHY:  Determines effective learning rate based on kinase activities
 * HOW:  Retrieves modulation factor [0.5, 2.0] where 1.0 = baseline
 *
 * @param ctx Brain-training context
 * @param neuron_id Neuron ID to query
 * @return Modulation factor [0.5, 2.0], 1.0 on error or if disabled
 */
float nimcp_brain_training_get_cascade_modulation(
    const nimcp_brain_training_ctx_t* ctx,
    uint32_t neuron_id
);

/* ============================================================================
 * Portia Resource Management Integration
 * ============================================================================ */

/**
 * @brief Connect Portia context for resource-aware training
 *
 * WHAT: Link training system to Portia resource management
 * WHY:  Enable automatic adaptation to platform tier and resource constraints
 * HOW:  Store Portia reference, register for tier/degradation events
 *
 * @param ctx Brain-training context
 * @param portia_ctx Portia context (can be NULL to disconnect)
 * @return NIMCP_SUCCESS on success
 */
nimcp_result_t nimcp_brain_training_connect_portia(
    nimcp_brain_training_ctx_t* ctx,
    void* portia_ctx
);

/**
 * @brief Handle Portia tier change event
 *
 * WHAT: Adapt training parameters based on new platform tier
 * WHY:  Reduce computational load when tier degrades
 * HOW:  Adjust batch sizes, learning rates, skip optional processing
 *
 * TIER ADAPTATIONS:
 * - TIER_OPTIMAL:   full batch (100%), normal LR (100%), all features
 * - TIER_REDUCED:   75% batch, 90% LR, disable verbose logging
 * - TIER_DEGRADED:  50% batch, 75% LR, skip regularization
 * - TIER_EMERGENCY: pause training, save checkpoint
 *
 * @param ctx Brain-training context
 * @param new_tier New platform tier
 * @return NIMCP_SUCCESS on success
 */
nimcp_result_t nimcp_brain_training_on_tier_change(
    nimcp_brain_training_ctx_t* ctx,
    platform_tier_t new_tier
);

/**
 * @brief Handle Portia degradation event
 *
 * WHAT: Respond to graceful degradation events
 * WHY:  Proactively reduce training load before critical resource exhaustion
 * HOW:  Defer gradient accumulation, reduce precision, pause non-critical ops
 *
 * @param ctx Brain-training context
 * @param degradation_level New degradation level
 * @return NIMCP_SUCCESS on success
 */
nimcp_result_t nimcp_brain_training_on_degradation_event(
    nimcp_brain_training_ctx_t* ctx,
    uint32_t degradation_level
);

/**
 * @brief Check if training is paused due to resource constraints
 *
 * @param ctx Brain-training context
 * @return true if training paused
 */
bool nimcp_brain_training_is_paused(
    const nimcp_brain_training_ctx_t* ctx
);

/**
 * @brief Resume paused training when resources available
 *
 * @param ctx Brain-training context
 * @return NIMCP_SUCCESS on success
 */
nimcp_result_t nimcp_brain_training_resume(
    nimcp_brain_training_ctx_t* ctx
);

/**
 * @brief Get adjusted batch size based on current tier
 *
 * WHAT: Calculate effective batch size for current resource state
 * WHY:  Allow queries before training step execution
 * HOW:  Apply tier multiplier to base batch size
 *
 * @param ctx Brain-training context
 * @param base_batch_size Original batch size
 * @return Adjusted batch size
 */
size_t nimcp_brain_training_get_adjusted_batch_size(
    const nimcp_brain_training_ctx_t* ctx,
    size_t base_batch_size
);

/**
 * @brief Get adjusted learning rate based on current tier
 *
 * WHAT: Calculate effective learning rate for current resource state
 * WHY:  Reduce learning rate when tier degraded for stability
 * HOW:  Apply tier multiplier to base learning rate
 *
 * @param ctx Brain-training context
 * @param base_lr Original learning rate
 * @return Adjusted learning rate
 */
float nimcp_brain_training_get_adjusted_lr(
    const nimcp_brain_training_ctx_t* ctx,
    float base_lr
);

/**
 * @brief Send training resource request to Portia
 *
 * WHAT: Request resource allocation for training workload
 * WHY:  Inform Portia of training resource needs for better planning
 * HOW:  Send BIO_MSG_TRAINING_RESOURCE_REQUEST via bio-async
 *
 * @param ctx Brain-training context
 * @param batch_size Requested batch size
 * @param param_count Number of parameters to train
 * @return NIMCP_SUCCESS if message sent
 */
nimcp_result_t nimcp_brain_training_request_resources(
    nimcp_brain_training_ctx_t* ctx,
    size_t batch_size,
    size_t param_count
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BRAIN_TRAINING_INTEGRATION_H */
