//=============================================================================
// nimcp_training_callbacks.h - Training Event Callback System
//=============================================================================
/**
 * @file nimcp_training_callbacks.h
 * @brief Event-driven callback system for training pipeline monitoring and control
 *
 * Phase TCB-1: Training Callbacks
 *
 * WHAT: Callback system for automatic training event handling
 * WHY:  Separates training logic from monitoring/control logic
 * HOW:  Register callbacks that fire automatically during training
 *
 * ARCHITECTURE:
 * +===========================================================================+
 * |                    Training Callback System                                |
 * +===========================================================================+
 * |                                                                           |
 * |  +---------------------------+     +---------------------------+          |
 * |  |   Training Pipeline       |     |   Callback Manager        |          |
 * |  +---------------------------+     +---------------------------+          |
 * |  | train_step()              |---->| on_step_complete         |          |
 * |  | compute_loss()            |---->| on_loss_computed         |          |
 * |  | update_weights()          |---->| on_weights_updated       |          |
 * |  | step_scheduler()          |---->| on_lr_changed            |          |
 * |  +---------------------------+     +---------------------------+          |
 * |                                              |                            |
 * |                                              v                            |
 * |  +----------------------------------------------------------+            |
 * |  |              User-Registered Callbacks                    |            |
 * |  |  logging | checkpointing | early_stop | metrics | alerts  |            |
 * |  +----------------------------------------------------------+            |
 * |                                                                           |
 * +===========================================================================+
 *
 * CALLBACK TYPES:
 * - on_step_complete:    After each training step (loss, gradients computed)
 * - on_epoch_complete:   After each epoch (all batches processed)
 * - on_loss_computed:    When loss value is calculated (before backprop)
 * - on_weights_updated:  After weights are modified
 * - on_lr_changed:       When learning rate is adjusted
 * - on_convergence:      When early stopping criteria met
 * - on_divergence:       When training instability detected
 * - on_checkpoint:       Periodic checkpoint trigger
 *
 * THREAD SAFETY:
 * - Callback registration/removal is mutex-protected
 * - Callbacks execute on training thread (synchronous)
 * - Async callback queue available for non-blocking handlers
 *
 * SECURITY:
 * - All callback contexts registered with security module
 * - Callbacks validated before execution
 * - Resource limits enforced on callback execution time
 *
 * @author NIMCP Development Team
 * @version 1.0.0
 * @date 2025-11-27
 */

#ifndef NIMCP_TRAINING_CALLBACKS_H
#define NIMCP_TRAINING_CALLBACKS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "utils/validation/nimcp_common.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "security/nimcp_security_integration.h"
#include "common/nimcp_export.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** Maximum number of callbacks per event type */
#define TCB_MAX_CALLBACKS_PER_EVENT 16

/** Maximum callback execution time before warning (microseconds) */
#define TCB_MAX_CALLBACK_TIME_US 100000

/** Default checkpoint interval (steps) */
#define TCB_DEFAULT_CHECKPOINT_INTERVAL 1000

/** Default logging interval (steps) */
#define TCB_DEFAULT_LOG_INTERVAL 100

/** Security module name */
#define TCB_SECURITY_MODULE_NAME "TrainingCallbacks"

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Training callback event types
 *
 * WHAT: Events that trigger callbacks
 * WHY:  Allow selective subscription to training events
 */
typedef enum {
    TCB_EVENT_STEP_COMPLETE = 0,   /**< Training step finished */
    TCB_EVENT_EPOCH_COMPLETE,       /**< Epoch finished */
    TCB_EVENT_LOSS_COMPUTED,        /**< Loss calculated */
    TCB_EVENT_WEIGHTS_UPDATED,      /**< Weights modified */
    TCB_EVENT_LR_CHANGED,           /**< Learning rate changed */
    TCB_EVENT_CONVERGENCE,          /**< Early stopping triggered */
    TCB_EVENT_DIVERGENCE,           /**< Training instability */
    TCB_EVENT_CHECKPOINT,           /**< Checkpoint trigger */
    TCB_EVENT_GRADIENT_CLIPPED,     /**< Gradients were clipped */
    TCB_EVENT_BATCH_START,          /**< Batch processing started */
    TCB_EVENT_BATCH_END,            /**< Batch processing ended */
    TCB_EVENT_VALIDATION,           /**< Validation phase */
    TCB_EVENT_COUNT
} tcb_event_type_t;

/**
 * @brief Callback execution modes
 */
typedef enum {
    TCB_MODE_SYNC = 0,              /**< Execute synchronously (blocks training) */
    TCB_MODE_ASYNC,                 /**< Queue for async execution */
    TCB_MODE_FIRE_AND_FORGET        /**< Non-blocking, no completion tracking */
} tcb_exec_mode_t;

/**
 * @brief Callback priority levels
 */
typedef enum {
    TCB_PRIORITY_LOW = 0,           /**< Execute after normal callbacks */
    TCB_PRIORITY_NORMAL,            /**< Default priority */
    TCB_PRIORITY_HIGH,              /**< Execute before normal callbacks */
    TCB_PRIORITY_CRITICAL           /**< Execute first, cannot be skipped */
} tcb_priority_t;

/**
 * @brief Callback return actions
 */
typedef enum {
    TCB_ACTION_CONTINUE = 0,        /**< Continue training normally */
    TCB_ACTION_STOP_TRAINING,       /**< Stop training loop */
    TCB_ACTION_SKIP_STEP,           /**< Skip current step (no weight update) */
    TCB_ACTION_ROLLBACK,            /**< Rollback to last checkpoint */
    TCB_ACTION_REDUCE_LR,           /**< Reduce learning rate */
    TCB_ACTION_INCREASE_LR          /**< Increase learning rate */
} tcb_action_t;

//=============================================================================
// Data Structures
//=============================================================================

/**
 * @brief Training metrics snapshot
 *
 * WHAT: Current training state passed to callbacks
 * WHY:  Provides all relevant info for decision making
 */
typedef struct {
    /* Step/epoch counters */
    uint64_t step;                  /**< Current training step */
    uint64_t epoch;                 /**< Current epoch */
    uint32_t batch;                 /**< Current batch in epoch */
    uint32_t total_batches;         /**< Total batches per epoch */

    /* Loss metrics */
    float loss;                     /**< Current loss value */
    float loss_delta;               /**< Change from previous loss */
    float loss_ema;                 /**< Exponential moving average of loss */
    float min_loss;                 /**< Minimum loss seen */
    float max_loss;                 /**< Maximum loss seen */

    /* Learning rate */
    float learning_rate;            /**< Current learning rate */
    float initial_lr;               /**< Initial learning rate */

    /* Gradient metrics */
    float gradient_norm;            /**< L2 norm of gradients */
    float gradient_max;             /**< Maximum gradient value */
    uint32_t gradients_clipped;     /**< Number of clipped gradients */

    /* Weight metrics */
    float weight_norm;              /**< L2 norm of weights */
    float weight_delta_norm;        /**< Norm of weight changes */

    /* Timing */
    uint64_t step_time_us;          /**< Time for current step (microseconds) */
    uint64_t total_time_us;         /**< Total training time */
    float steps_per_second;         /**< Training throughput */

    /* Validation (if available) */
    float val_loss;                 /**< Validation loss */
    float val_accuracy;             /**< Validation accuracy */
    bool has_validation;            /**< Whether validation metrics present */

    /* Flags */
    bool is_converging;             /**< Loss trending down */
    bool is_diverging;              /**< Loss trending up rapidly */
    bool gradients_exploding;       /**< Gradient norm very high */
    bool gradients_vanishing;       /**< Gradient norm very low */
} tcb_metrics_t;

/**
 * @brief Callback event data
 *
 * WHAT: Full event context passed to callback
 * WHY:  Callbacks need event type + metrics + user context
 */
typedef struct {
    tcb_event_type_t event_type;    /**< Type of event */
    tcb_metrics_t metrics;          /**< Current metrics snapshot */
    void* user_data;                /**< User-provided context */
    const char* checkpoint_path;    /**< Path for checkpoint events */
    uint64_t timestamp_ns;          /**< Event timestamp */
} tcb_event_t;

/**
 * @brief Callback function signature
 *
 * @param event Event data with metrics
 * @return Action for training loop to take
 */
typedef tcb_action_t (*tcb_callback_fn)(const tcb_event_t* event);

/**
 * @brief Callback registration info
 */
typedef struct {
    tcb_callback_fn callback;       /**< Callback function */
    void* user_data;                /**< User context passed to callback */
    tcb_event_type_t event_type;    /**< Event type to subscribe */
    tcb_exec_mode_t mode;           /**< Execution mode */
    tcb_priority_t priority;        /**< Execution priority */
    const char* name;               /**< Callback name (for logging) */
    bool enabled;                   /**< Whether callback is active */
} tcb_callback_info_t;

/**
 * @brief Callback manager configuration
 */
typedef struct {
    /* Checkpoint settings */
    bool enable_auto_checkpoint;    /**< Enable automatic checkpointing */
    uint32_t checkpoint_interval;   /**< Steps between checkpoints */
    const char* checkpoint_dir;     /**< Directory for checkpoints */
    uint32_t max_checkpoints;       /**< Maximum checkpoints to keep */

    /* Logging settings */
    bool enable_auto_logging;       /**< Enable automatic logging */
    uint32_t log_interval;          /**< Steps between log outputs */
    bool log_to_file;               /**< Write logs to file */
    const char* log_file_path;      /**< Log file path */

    /* Early stopping */
    bool enable_early_stopping;     /**< Enable early stopping */
    uint32_t patience;              /**< Steps without improvement before stop */
    float min_delta;                /**< Minimum improvement to reset patience */
    float divergence_threshold;     /**< Loss increase ratio to trigger divergence */

    /* Performance */
    bool enable_async_callbacks;    /**< Use async callback queue */
    uint32_t async_queue_size;      /**< Size of async callback queue */
    uint32_t max_callback_time_us;  /**< Max callback execution time */

    /* Memory management */
    bool use_memory_pool;           /**< Use unified memory manager */
    unified_mem_strategy_t mem_strategy; /**< Memory strategy */

    /* Security */
    nimcp_sec_integration_t* security_ctx; /**< Security context */
} tcb_config_t;

/**
 * @brief Callback manager statistics
 */
typedef struct {
    uint64_t total_callbacks_fired;
    uint64_t callbacks_by_event[TCB_EVENT_COUNT];
    uint64_t total_execution_time_us;
    uint64_t max_execution_time_us;
    float avg_execution_time_us;
    uint32_t callbacks_timed_out;
    uint32_t callbacks_failed;
    uint32_t early_stops_triggered;
    uint32_t divergence_events;
    uint32_t checkpoints_saved;
    uint32_t rollbacks_performed;
} tcb_stats_t;

/**
 * @brief Callback manager context (opaque)
 */
typedef struct tcb_context tcb_context_t;

//=============================================================================
// Default Configuration
//=============================================================================

/**
 * @brief Get default callback manager configuration
 *
 * @return Default configuration with sensible values
 */
NIMCP_EXPORT tcb_config_t tcb_config_default(void);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create callback manager
 *
 * @param config Configuration (NULL for defaults)
 * @return Callback context or NULL on error
 */
NIMCP_EXPORT tcb_context_t* tcb_create(const tcb_config_t* config);

/**
 * @brief Destroy callback manager
 *
 * @param ctx Callback context
 */
NIMCP_EXPORT void tcb_destroy(tcb_context_t* ctx);

//=============================================================================
// Callback Registration
//=============================================================================

/**
 * @brief Register a callback for an event type
 *
 * @param ctx Callback context
 * @param info Callback registration info
 * @return Callback ID (>0) or 0 on error
 */
NIMCP_EXPORT uint32_t tcb_register(
    tcb_context_t* ctx,
    const tcb_callback_info_t* info
);

/**
 * @brief Register callback with simple parameters
 *
 * @param ctx Callback context
 * @param event_type Event to subscribe
 * @param callback Callback function
 * @param user_data User context
 * @param name Callback name
 * @return Callback ID or 0 on error
 */
NIMCP_EXPORT uint32_t tcb_register_simple(
    tcb_context_t* ctx,
    tcb_event_type_t event_type,
    tcb_callback_fn callback,
    void* user_data,
    const char* name
);

/**
 * @brief Unregister a callback
 *
 * @param ctx Callback context
 * @param callback_id ID returned from tcb_register
 * @return NIMCP_SUCCESS or error code
 */
NIMCP_EXPORT nimcp_result_t tcb_unregister(
    tcb_context_t* ctx,
    uint32_t callback_id
);

/**
 * @brief Enable/disable a callback
 *
 * @param ctx Callback context
 * @param callback_id Callback ID
 * @param enabled Whether to enable
 * @return NIMCP_SUCCESS or error code
 */
NIMCP_EXPORT nimcp_result_t tcb_set_enabled(
    tcb_context_t* ctx,
    uint32_t callback_id,
    bool enabled
);

/**
 * @brief Unregister all callbacks for an event type
 *
 * @param ctx Callback context
 * @param event_type Event type (TCB_EVENT_COUNT for all)
 * @return Number of callbacks removed
 */
NIMCP_EXPORT uint32_t tcb_unregister_all(
    tcb_context_t* ctx,
    tcb_event_type_t event_type
);

//=============================================================================
// Event Firing
//=============================================================================

/**
 * @brief Fire callbacks for an event
 *
 * @param ctx Callback context
 * @param event Event data
 * @return Aggregate action from all callbacks
 */
NIMCP_EXPORT tcb_action_t tcb_fire(
    tcb_context_t* ctx,
    const tcb_event_t* event
);

/**
 * @brief Fire event with metrics (convenience function)
 *
 * @param ctx Callback context
 * @param event_type Event type
 * @param metrics Current metrics
 * @return Aggregate action
 */
NIMCP_EXPORT tcb_action_t tcb_fire_event(
    tcb_context_t* ctx,
    tcb_event_type_t event_type,
    const tcb_metrics_t* metrics
);

/**
 * @brief Update metrics from training result
 *
 * @param ctx Callback context
 * @param loss Current loss
 * @param learning_rate Current LR
 * @param step Current step
 * @param gradient_norm Gradient norm
 */
NIMCP_EXPORT void tcb_update_metrics(
    tcb_context_t* ctx,
    float loss,
    float learning_rate,
    uint64_t step,
    float gradient_norm
);

/**
 * @brief Get current metrics snapshot
 *
 * @param ctx Callback context
 * @param metrics Output metrics
 * @return NIMCP_SUCCESS or error code
 */
NIMCP_EXPORT nimcp_result_t tcb_get_metrics(
    const tcb_context_t* ctx,
    tcb_metrics_t* metrics
);

//=============================================================================
// Checkpoint Management
//=============================================================================

/**
 * @brief Trigger manual checkpoint
 *
 * @param ctx Callback context
 * @param path Checkpoint path (NULL for auto-generated)
 * @return NIMCP_SUCCESS or error code
 */
NIMCP_EXPORT nimcp_result_t tcb_checkpoint(
    tcb_context_t* ctx,
    const char* path
);

/**
 * @brief Set checkpoint callback (called when checkpoint should be saved)
 *
 * @param ctx Callback context
 * @param callback Checkpoint handler
 * @param user_data User context
 * @return NIMCP_SUCCESS or error code
 */
NIMCP_EXPORT nimcp_result_t tcb_set_checkpoint_handler(
    tcb_context_t* ctx,
    tcb_callback_fn callback,
    void* user_data
);

/**
 * @brief Get last checkpoint path
 *
 * @param ctx Callback context
 * @return Path or NULL if no checkpoint
 */
NIMCP_EXPORT const char* tcb_get_last_checkpoint(const tcb_context_t* ctx);

//=============================================================================
// Early Stopping
//=============================================================================

/**
 * @brief Check if early stopping should trigger
 *
 * @param ctx Callback context
 * @param current_loss Current loss value
 * @return true if should stop
 */
NIMCP_EXPORT bool tcb_should_stop(
    tcb_context_t* ctx,
    float current_loss
);

/**
 * @brief Reset early stopping counter
 *
 * @param ctx Callback context
 */
NIMCP_EXPORT void tcb_reset_early_stopping(tcb_context_t* ctx);

/**
 * @brief Get steps since last improvement
 *
 * @param ctx Callback context
 * @return Steps without improvement
 */
NIMCP_EXPORT uint32_t tcb_get_steps_without_improvement(const tcb_context_t* ctx);

//=============================================================================
// Statistics and Monitoring
//=============================================================================

/**
 * @brief Get callback statistics
 *
 * @param ctx Callback context
 * @param stats Output statistics
 * @return NIMCP_SUCCESS or error code
 */
NIMCP_EXPORT nimcp_result_t tcb_get_stats(
    const tcb_context_t* ctx,
    tcb_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param ctx Callback context
 */
NIMCP_EXPORT void tcb_reset_stats(tcb_context_t* ctx);

/**
 * @brief Get number of registered callbacks
 *
 * @param ctx Callback context
 * @param event_type Event type (TCB_EVENT_COUNT for total)
 * @return Number of callbacks
 */
NIMCP_EXPORT uint32_t tcb_get_callback_count(
    const tcb_context_t* ctx,
    tcb_event_type_t event_type
);

/**
 * @brief Print callback manager status
 *
 * @param ctx Callback context
 */
NIMCP_EXPORT void tcb_print_status(const tcb_context_t* ctx);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get event type name
 *
 * @param event_type Event type
 * @return String name
 */
NIMCP_EXPORT const char* tcb_event_type_name(tcb_event_type_t event_type);

/**
 * @brief Get action name
 *
 * @param action Action type
 * @return String name
 */
NIMCP_EXPORT const char* tcb_action_name(tcb_action_t action);

/**
 * @brief Validate callback configuration
 *
 * @param config Configuration to validate
 * @return NIMCP_SUCCESS if valid
 */
NIMCP_EXPORT nimcp_result_t tcb_validate_config(const tcb_config_t* config);

//=============================================================================
// Built-in Callbacks
//=============================================================================

/**
 * @brief Built-in logging callback
 *
 * Logs: step, loss, lr, gradient_norm, time
 */
NIMCP_EXPORT tcb_action_t tcb_builtin_logger(const tcb_event_t* event);

/**
 * @brief Built-in early stopping callback
 *
 * Returns TCB_ACTION_STOP_TRAINING when patience exceeded
 */
NIMCP_EXPORT tcb_action_t tcb_builtin_early_stopper(const tcb_event_t* event);

/**
 * @brief Built-in divergence detector callback
 *
 * Returns TCB_ACTION_STOP_TRAINING or TCB_ACTION_REDUCE_LR on divergence
 */
NIMCP_EXPORT tcb_action_t tcb_builtin_divergence_detector(const tcb_event_t* event);

/**
 * @brief Built-in gradient monitor callback
 *
 * Detects exploding/vanishing gradients
 */
NIMCP_EXPORT tcb_action_t tcb_builtin_gradient_monitor(const tcb_event_t* event);

/**
 * @brief Built-in progress bar callback
 *
 * Prints progress bar to stdout
 */
NIMCP_EXPORT tcb_action_t tcb_builtin_progress_bar(const tcb_event_t* event);

/**
 * @brief Context for built-in rubric evaluator callback
 */
typedef struct {
    void* brain;                       /**< nimcp_brain_t (void* to avoid header dep) */
    const float* validation_features;  /**< Features for predict (non-owning) */
    uint32_t num_features;
    uint32_t interval;                 /**< Evaluate every N steps (0 = every call) */
    float min_score;                   /**< Threshold (0 = no threshold) */
    /* Output */
    float last_score;
    char last_grade;
    uint64_t eval_count;
} tcb_rubric_context_t;

/**
 * @brief Built-in rubric evaluator callback
 *
 * Calls nimcp_brain_predict() + nimcp_brain_rubric() at the configured interval.
 * Returns TCB_ACTION_STOP_TRAINING if score is below threshold.
 *
 * @param event Event data (user_data must be tcb_rubric_context_t*)
 * @return TCB_ACTION_CONTINUE or TCB_ACTION_STOP_TRAINING
 */
NIMCP_EXPORT tcb_action_t tcb_builtin_rubric_evaluator(const tcb_event_t* event);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_TRAINING_CALLBACKS_H */
