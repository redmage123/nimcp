/**
 * @file nimcp_lnn_training.h
 * @brief Training integration for Liquid Neural Networks
 *
 * WHAT: Integrates LNN networks with NIMCP training infrastructure
 * WHY:  Enables end-to-end training of LNN models with full pipeline support
 * HOW:  Wraps optimizer, gradient manager, loss functions, and training bridges
 *
 * BIOLOGICAL BASIS: Models learning as continuous-time adaptation of
 * liquid time constants, mirroring synaptic plasticity with temporal dynamics.
 *
 * @note Part of LNN Phase 1: Core Library
 * @version 1.0.0
 * @author NIMCP Development Team
 * @date 2025-12-20
 */

#ifndef NIMCP_LNN_TRAINING_H
#define NIMCP_LNN_TRAINING_H

#include "nimcp_lnn_types.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "nimcp_lnn_gradient.h"
#include "middleware/training/nimcp_optimizers.h"
#include "middleware/training/nimcp_gradient_manager.h"
#include "middleware/training/nimcp_loss_functions.h"
#include "utils/tensor/nimcp_tensor.h"

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * Forward Declarations
 *===========================================================================*/

typedef struct lnn_training_ctx_s lnn_training_ctx_t;
typedef struct cognitive_training_bridge cognitive_training_bridge_t;
typedef struct training_logic_bridge training_logic_bridge_t;
typedef struct training_immune_system training_immune_system_t;
typedef struct training_plasticity_bridge training_plasticity_bridge_t;

/*=============================================================================
 * Enumerations
 *===========================================================================*/

/**
 * @brief Learning rate scheduling strategies for LNN training
 *
 * WHAT: Different LR decay/warmup strategies
 * WHY:  LR schedules critical for continuous-time learning convergence
 * HOW:  Applied after each step/epoch based on configuration
 */
typedef enum {
    LNN_LR_SCHEDULE_CONSTANT = 0,      /**< Fixed learning rate */
    LNN_LR_SCHEDULE_STEP,              /**< Step decay at intervals */
    LNN_LR_SCHEDULE_EXPONENTIAL,       /**< Exponential decay */
    LNN_LR_SCHEDULE_COSINE,            /**< Cosine annealing */
    LNN_LR_SCHEDULE_WARMUP_COSINE,     /**< Warmup + cosine annealing */
    LNN_LR_SCHEDULE_REDUCE_ON_PLATEAU, /**< Reduce when metric plateaus */
    LNN_LR_SCHEDULE_COUNT
} lnn_lr_schedule_t;

/*=============================================================================
 * Configuration Structures
 *===========================================================================*/

/**
 * @brief Configuration for LNN training context
 *
 * WHAT: All training-related parameters for LNN network
 * WHY:  Single struct for complete training setup
 * HOW:  Passed to lnn_training_create(), copied internally
 */
typedef struct {
    /* === Optimizer Configuration === */
    nimcp_optimizer_type_t optimizer_type;  /**< Optimizer algorithm */
    float learning_rate;                     /**< Initial learning rate */
    float weight_decay;                      /**< L2 regularization coefficient */

    /* Adam-specific parameters */
    float beta1;                             /**< Adam first moment decay (default: 0.9) */
    float beta2;                             /**< Adam second moment decay (default: 0.999) */
    float epsilon;                           /**< Numerical stability (default: 1e-8) */

    /* === Gradient Manager Configuration === */
    float gradient_clip_norm;                /**< Max gradient norm (0 = disabled) */
    bool use_gradient_scaling;               /**< Enable gradient scaling (mixed precision) */
    nimcp_grad_accum_mode_t accum_mode;      /**< Gradient accumulation mode */
    uint32_t accumulation_steps;             /**< Steps to accumulate before update */

    /* === Loss Configuration === */
    nimcp_loss_type_t loss_type;             /**< Loss function type */
    nimcp_loss_reduction_t reduction;        /**< Loss reduction mode */

    /* === LNN-Specific Training === */
    lnn_train_mode_t lnn_train_mode;         /**< LNN gradient computation mode */
    uint32_t bptt_truncation;                /**< BPTT truncation length (if used) */
    bool use_adjoint_checkpointing;          /**< Enable checkpointing for adjoint */

    /* === Learning Rate Scheduling === */
    lnn_lr_schedule_t lr_schedule;           /**< LR schedule type */
    float lr_schedule_params[8];             /**< Schedule-specific parameters */
    uint32_t n_schedule_params;              /**< Number of schedule params */

    /* === Integration Enables === */
    bool enable_cognitive_integration;       /**< Enable cognitive-training bridge */
    bool enable_logic_integration;           /**< Enable training-logic bridge */
    bool enable_immune_integration;          /**< Enable training-immune system */
    bool enable_plasticity_integration;      /**< Enable training-plasticity bridge */
    bool enable_bio_async;                   /**< Enable bio-async messaging */

    /* === Training Behavior === */
    bool use_mixed_precision;                /**< Use FP16/FP32 mixed precision */
    bool validate_gradients;                 /**< Check for NaN/Inf in gradients */
    bool track_statistics;                   /**< Track detailed statistics */

    /* === Anti-Collapse Configuration === */
    float diversity_loss_weight;            /**< Weight for output diversity loss (default: 0.1) */
    uint32_t diversity_buffer_size;         /**< Number of recent outputs to compare (default: 16) */
    bool use_gradient_normalization;        /**< Normalize gradients to fixed norm instead of clipping */
    float gradient_target_norm;             /**< Target norm for gradient normalization (default: 1.0) */
    uint32_t max_adjoint_steps;             /**< Max backward ODE steps (0 = unlimited, default: 50) */
    float lr_scale_tau;                     /**< LR multiplier for tau parameters (default: 0.1) */
    float lr_scale_bias;                    /**< LR multiplier for bias parameters (default: 2.0) */

} lnn_training_config_t;

/*=============================================================================
 * Statistics Structures
 *===========================================================================*/

/**
 * @brief Training statistics for LNN network
 *
 * WHAT: Accumulated training metrics and diagnostics
 * WHY:  Monitor training progress and detect issues
 * HOW:  Updated during training, queryable via API
 */
typedef struct {
    /* Step counts */
    uint64_t step_count;                     /**< Total optimization steps */
    uint64_t epoch_count;                    /**< Total epochs completed */

    /* Loss metrics */
    float current_loss;                      /**< Most recent loss value */
    double total_loss;                       /**< Cumulative loss */
    float min_loss;                          /**< Minimum loss observed */
    float max_loss;                          /**< Maximum loss observed */
    float avg_loss;                          /**< Running average loss */

    /* Gradient metrics */
    double total_gradient_norm;              /**< Sum of gradient norms */
    float min_gradient_norm;                 /**< Minimum gradient norm */
    float max_gradient_norm;                 /**< Maximum gradient norm */
    float avg_gradient_norm;                 /**< Average gradient norm */
    uint64_t gradient_clips;                 /**< Number of gradient clips */

    /* Learning rate */
    float current_lr;                        /**< Current learning rate */
    float min_lr;                            /**< Minimum LR used */
    float max_lr;                            /**< Maximum LR used */
    uint64_t lr_updates;                     /**< Number of LR updates */

    /* Time constants (LNN-specific) */
    float avg_tau_network;                   /**< Average τ across network */
    float min_tau_network;                   /**< Minimum τ in network */
    float max_tau_network;                   /**< Maximum τ in network */

    /* Training instabilities */
    uint32_t nan_detections;                 /**< NaN in loss/gradients */
    uint32_t inf_detections;                 /**< Inf in loss/gradients */
    uint32_t explosion_events;               /**< Gradient explosions */
    uint32_t vanishing_events;               /**< Gradient vanishing */

    /* Timing */
    uint64_t total_forward_time_ms;          /**< Total forward pass time */
    uint64_t total_backward_time_ms;         /**< Total backward pass time */
    uint64_t total_optimizer_time_ms;        /**< Total optimizer step time */
    float avg_step_time_ms;                  /**< Average time per step */

    /* Integration status */
    bool cognitive_bridge_connected;
    bool logic_bridge_connected;
    bool immune_system_connected;
    bool plasticity_bridge_connected;

} lnn_training_stats_t;

/*=============================================================================
 * Training Context (Opaque)
 *===========================================================================*/

/**
 * @brief LNN training context
 *
 * WHAT: Complete training state for LNN network
 * WHY:  Encapsulates all training components in single handle
 * HOW:  Created via lnn_training_create(), destroyed via lnn_training_destroy()
 *
 * STRUCTURE (internal, opaque to users):
 * - LNN network reference
 * - Optimizer context
 * - Gradient manager context
 * - Loss context
 * - Training bridge references
 * - LR schedule state
 * - Statistics
 * - Callbacks
 */
struct lnn_training_ctx_s {
    /* LNN network */
    lnn_network_t* network;

    /* NIMCP training components */
    nimcp_optimizer_context_t* optimizer;
    nimcp_gradient_manager_ctx_t* gradient_manager;
    nimcp_loss_context_t* loss_context;

    /* LNN gradient computation */
    lnn_gradient_ctx_t* gradient_ctx;
    nimcp_tensor_t* output_buffer;    /**< Buffer for network outputs */
    nimcp_tensor_t* loss_gradient;    /**< Buffer for dL/dx */

    /* Configuration */
    lnn_training_config_t config;

    /* Training state */
    uint64_t step_count;
    uint64_t epoch_count;
    float current_lr;
    float current_loss;

    /* LR schedule state */
    lnn_lr_schedule_t lr_schedule;
    float* lr_schedule_params;
    uint32_t n_schedule_params;
    uint64_t last_lr_update_step;
    float plateau_best_metric;
    uint32_t plateau_patience_count;

    /* Gradient accumulation */
    bool accumulate_gradients;
    uint32_t accumulation_steps;
    uint32_t current_accumulation;

    /* Training bridges (optional) */
    cognitive_training_bridge_t* cognitive_training_bridge;
    training_logic_bridge_t* training_logic_bridge;
    training_immune_system_t* training_immune_system;
    training_plasticity_bridge_t* training_plasticity_bridge;

    /* Bio-async */
    bool bio_async_enabled;
    void* bio_ctx;

    /* Callbacks */
    void (*on_step_complete)(void* user_data, uint64_t step, float loss);
    void (*on_epoch_complete)(void* user_data, uint64_t epoch, float avg_loss);
    void (*on_lr_change)(void* user_data, float old_lr, float new_lr);
    void* callback_user_data;

    /* Anti-collapse (unified) — forward declared in nimcp_unified_training.h */
    void* anti_collapse_state;            /**< nimcp_anti_collapse_state_t* */

    /* UTM management flag — when true, UTM owns diversity/gradient norm/optimizer */
    bool managed_by_utm;

    /* Statistics */
    lnn_training_stats_t stats;

    /* Thread safety */
    void* mutex;
};

/*=============================================================================
 * Lifecycle Functions
 *===========================================================================*/

/**
 * @brief Initialize configuration with default values
 *
 * WHAT: Populates config struct with sensible defaults
 * WHY:  Ensure all fields have valid initial values
 * HOW:  Sets Adam optimizer, MSE loss, adjoint training, no scheduling
 *
 * @param config Configuration to initialize (must not be NULL)
 * @return 0 on success, negative on error
 */
int lnn_training_config_default(lnn_training_config_t* config);

/**
 * @brief Create training context for LNN network
 *
 * WHAT: Allocates and initializes complete training pipeline
 * WHY:  One-stop setup for LNN training
 * HOW:  Creates optimizer, gradient manager, loss context from config
 *
 * BIOLOGICAL ANALOGY: Sets up the "learning rules" for the liquid network,
 * analogous to establishing neuromodulatory and plasticity mechanisms.
 *
 * @param network LNN network to train (must not be NULL)
 * @param config Training configuration (NULL uses defaults)
 * @return Training context or NULL on failure
 */
lnn_training_ctx_t* lnn_training_create(
    lnn_network_t* network,
    const lnn_training_config_t* config
);

/**
 * @brief Destroy training context
 *
 * WHAT: Frees all training-related resources
 * WHY:  Proper cleanup
 * HOW:  Destroys optimizer, gradient manager, loss context, frees memory
 *
 * @param ctx Training context to destroy (NULL-safe)
 */
void lnn_training_destroy(lnn_training_ctx_t* ctx);

/*=============================================================================
 * Training Step Functions
 *===========================================================================*/

/**
 * @brief Execute one training step
 *
 * WHAT: Forward + backward + optimizer step
 * WHY:  Core training loop operation
 * HOW:
 *   1. Forward pass: lnn_network_forward_sequence()
 *   2. Compute loss: nimcp_loss_compute()
 *   3. Backward pass: lnn_gradient_compute_adjoint()
 *   4. Optimizer step: nimcp_optimizer_step()
 *   5. Update LR schedule
 *   6. Call callbacks
 *   7. Update training bridges
 *
 * BIOLOGICAL ANALOGY: One "learning trial" - present stimulus, compute
 * prediction error, update synaptic weights via continuous-time plasticity.
 *
 * @param ctx Training context
 * @param inputs Input sequence [seq_len, n_inputs]
 * @param targets Target sequence [seq_len, n_outputs]
 * @param loss_out Output: computed loss value (may be NULL)
 * @return 0 on success, negative on error
 */
int lnn_training_step(
    lnn_training_ctx_t* ctx,
    const nimcp_tensor_t* inputs,
    const nimcp_tensor_t* targets,
    float* loss_out
);

/**
 * @brief Execute training step on batch (parallel)
 *
 * WHAT: Train on multiple sequences in parallel
 * WHY:  Accelerate training via batch processing
 * HOW:  Processes batch in parallel, accumulates gradients, updates once
 *
 * @param ctx Training context
 * @param inputs Input batch [batch_size, seq_len, n_inputs]
 * @param targets Target batch [batch_size, seq_len, n_outputs]
 * @param batch_size Number of sequences in batch
 * @param loss_out Output: average loss over batch (may be NULL)
 * @return 0 on success, negative on error
 */
int lnn_training_step_batch(
    lnn_training_ctx_t* ctx,
    const nimcp_tensor_t* inputs,
    const nimcp_tensor_t* targets,
    uint32_t batch_size,
    float* loss_out
);

/**
 * @brief Train for one epoch
 *
 * WHAT: Single pass through entire dataset
 * WHY:  Standard training iteration unit
 * HOW:  Splits dataset into batches, calls lnn_training_step_batch() for each
 *
 * @param ctx Training context
 * @param dataset_inputs Full dataset inputs [n_samples, seq_len, n_inputs]
 * @param dataset_targets Full dataset targets [n_samples, seq_len, n_outputs]
 * @param n_samples Number of samples in dataset
 * @param batch_size Batch size for mini-batch training
 * @param avg_loss_out Output: average loss over epoch (may be NULL)
 * @return 0 on success, negative on error
 */
int lnn_training_epoch(
    lnn_training_ctx_t* ctx,
    const nimcp_tensor_t* dataset_inputs,
    const nimcp_tensor_t* dataset_targets,
    uint32_t n_samples,
    uint32_t batch_size,
    float* avg_loss_out
);

/*=============================================================================
 * Integration Connection Functions
 *===========================================================================*/

/**
 * @brief Connect to cognitive training bridge
 *
 * WHAT: Links LNN training to cognitive modulation
 * WHY:  Cognitive state (attention, emotion, etc.) modulates LR and gradients
 * HOW:  Stores bridge reference, queries effects during training
 *
 * BIOLOGICAL BASIS: Prefrontal-limbic circuits modulate learning rate
 * based on cognitive load, emotional state, and task relevance.
 *
 * @param ctx Training context
 * @param cognitive_bridge Cognitive training bridge (may be NULL to disconnect)
 * @return 0 on success, negative on error
 */
int lnn_training_connect_cognitive(
    lnn_training_ctx_t* ctx,
    cognitive_training_bridge_t* cognitive_bridge
);

/**
 * @brief Connect to training logic bridge
 *
 * WHAT: Links LNN training to logic-based control
 * WHY:  Logic gates determine when to adjust LR, checkpoint, etc.
 * HOW:  Stores bridge reference, queries decisions during training
 *
 * BIOLOGICAL BASIS: Executive control (DLPFC) uses rule-based decision
 * making to regulate learning intensity based on training stability.
 *
 * @param ctx Training context
 * @param logic_bridge Training logic bridge (may be NULL to disconnect)
 * @return 0 on success, negative on error
 */
int lnn_training_connect_logic(
    lnn_training_ctx_t* ctx,
    training_logic_bridge_t* logic_bridge
);

/**
 * @brief Connect to training immune system
 *
 * WHAT: Links LNN training to immune-based regulation
 * WHY:  Inflammation modulates LR (fever reduces plasticity)
 * HOW:  Stores immune reference, applies inflammation effects to LR
 *
 * BIOLOGICAL BASIS: Cytokines released during neural inflammation
 * suppress synaptic plasticity to conserve energy during immune response.
 *
 * @param ctx Training context
 * @param immune_system Training immune system (may be NULL to disconnect)
 * @return 0 on success, negative on error
 */
int lnn_training_connect_immune(
    lnn_training_ctx_t* ctx,
    training_immune_system_t* immune_system
);

/**
 * @brief Connect to training plasticity bridge
 *
 * WHAT: Links LNN training to plasticity mechanisms
 * WHY:  Biological plasticity (STDP, BCM) can inform LNN parameter updates
 * HOW:  Stores plasticity reference, may incorporate plasticity signals
 *
 * BIOLOGICAL BASIS: Continuous-time LNN dynamics can be modulated by
 * spike-timing-dependent plasticity and other biological learning rules.
 *
 * @param ctx Training context
 * @param plasticity_bridge Training plasticity bridge (may be NULL to disconnect)
 * @return 0 on success, negative on error
 */
int lnn_training_connect_plasticity(
    lnn_training_ctx_t* ctx,
    training_plasticity_bridge_t* plasticity_bridge
);

/**
 * @brief Connect optimizer to network
 *
 * WHAT: Register optimizer for gradient updates
 * WHY:  Enable gradient-based learning with various optimizers
 * HOW:  Store optimizer reference and register for gradient updates
 *
 * @param network LNN network
 * @param optimizer Optimizer instance (may be NULL to disconnect)
 * @return 0 on success, negative on error
 */
int lnn_training_connect_optimizer(lnn_network_t* network, void* optimizer);

/*=============================================================================
 * Learning Rate Scheduling Functions
 *===========================================================================*/

/**
 * @brief Set learning rate schedule
 *
 * WHAT: Configure LR decay/warmup strategy
 * WHY:  LR schedules critical for training convergence
 * HOW:  Stores schedule type and parameters for lnn_training_update_lr()
 *
 * SCHEDULE PARAMETERS:
 * - CONSTANT: No params
 * - STEP: [step_size, gamma] (e.g., [100, 0.1] = decay by 0.1 every 100 steps)
 * - EXPONENTIAL: [gamma] (e.g., [0.95] = lr *= 0.95 each step)
 * - COSINE: [T_max] (e.g., [1000] = cosine over 1000 steps)
 * - WARMUP_COSINE: [warmup_steps, T_max] (e.g., [100, 1000])
 * - REDUCE_ON_PLATEAU: [factor, patience, threshold] (e.g., [0.5, 10, 0.01])
 *
 * @param ctx Training context
 * @param schedule Schedule type
 * @param params Schedule-specific parameters
 * @param n_params Number of parameters
 * @return 0 on success, negative on error
 */
int lnn_training_set_lr_schedule(
    lnn_training_ctx_t* ctx,
    lnn_lr_schedule_t schedule,
    float* params,
    uint32_t n_params
);

/**
 * @brief Update learning rate (call after each step/epoch)
 *
 * WHAT: Apply LR schedule update
 * WHY:  Automatic LR adjustment based on schedule
 * HOW:  Computes new LR based on schedule type and current step
 *
 * @param ctx Training context
 * @return 0 on success, negative on error
 */
int lnn_training_update_lr(lnn_training_ctx_t* ctx);

/**
 * @brief Get current learning rate
 *
 * WHAT: Query current LR value
 * WHY:  Monitoring and logging
 * HOW:  Returns ctx->current_lr
 *
 * @param ctx Training context
 * @return Current learning rate
 */
float lnn_training_get_lr(const lnn_training_ctx_t* ctx);

/**
 * @brief Set learning rate manually
 *
 * WHAT: Override current LR
 * WHY:  Manual control or external scheduling
 * HOW:  Updates ctx->current_lr and optimizer LR
 *
 * @param ctx Training context
 * @param lr New learning rate
 */
void lnn_training_set_lr(lnn_training_ctx_t* ctx, float lr);

/*=============================================================================
 * Callback Functions
 *===========================================================================*/

/**
 * @brief Set step completion callback
 *
 * WHAT: Register callback for each training step
 * WHY:  Custom logging, checkpointing, early stopping
 * HOW:  Callback invoked at end of lnn_training_step()
 *
 * @param ctx Training context
 * @param callback Callback function (may be NULL to clear)
 * @param user_data User data passed to callback
 */
void lnn_training_set_step_callback(
    lnn_training_ctx_t* ctx,
    void (*callback)(void* user_data, uint64_t step, float loss),
    void* user_data
);

/**
 * @brief Set epoch completion callback
 *
 * WHAT: Register callback for each epoch
 * WHY:  Validation, checkpointing, logging
 * HOW:  Callback invoked at end of lnn_training_epoch()
 *
 * @param ctx Training context
 * @param callback Callback function (may be NULL to clear)
 * @param user_data User data passed to callback
 */
void lnn_training_set_epoch_callback(
    lnn_training_ctx_t* ctx,
    void (*callback)(void* user_data, uint64_t epoch, float avg_loss),
    void* user_data
);

/**
 * @brief Set learning rate change callback
 *
 * WHAT: Register callback for LR updates
 * WHY:  Monitor LR schedule behavior
 * HOW:  Callback invoked when lnn_training_update_lr() changes LR
 *
 * @param ctx Training context
 * @param callback Callback function (may be NULL to clear)
 * @param user_data User data passed to callback
 */
void lnn_training_set_lr_callback(
    lnn_training_ctx_t* ctx,
    void (*callback)(void* user_data, float old_lr, float new_lr),
    void* user_data
);

/*=============================================================================
 * Statistics Functions
 *===========================================================================*/

/**
 * @brief Get current step count
 *
 * @param ctx Training context
 * @return Current step count
 */
uint64_t lnn_training_get_step_count(const lnn_training_ctx_t* ctx);

/**
 * @brief Get current epoch count
 *
 * @param ctx Training context
 * @return Current epoch count
 */
uint64_t lnn_training_get_epoch_count(const lnn_training_ctx_t* ctx);

/**
 * @brief Get current loss value
 *
 * @param ctx Training context
 * @return Most recent loss value
 */
float lnn_training_get_current_loss(const lnn_training_ctx_t* ctx);

/**
 * @brief Get training statistics
 *
 * WHAT: Retrieve accumulated training metrics
 * WHY:  Monitoring, logging, debugging
 * HOW:  Copies ctx->stats to output
 *
 * @param ctx Training context
 * @param stats Output statistics structure (must not be NULL)
 * @return 0 on success, negative on error
 */
int lnn_training_get_stats(
    const lnn_training_ctx_t* ctx,
    lnn_training_stats_t* stats
);

/**
 * @brief Reset training statistics
 *
 * WHAT: Clear accumulated statistics
 * WHY:  Start fresh measurement period
 * HOW:  Zeros all stat counters in ctx->stats
 *
 * @param ctx Training context
 * @return 0 on success, negative on error
 */
int lnn_training_reset_stats(lnn_training_ctx_t* ctx);

/*=============================================================================
 * Utility Functions
 *===========================================================================*/

/**
 * @brief Get LR schedule type name
 *
 * @param schedule Schedule type
 * @return String name of schedule
 */
const char* lnn_lr_schedule_name(lnn_lr_schedule_t schedule);

/**
 * @brief Validate training configuration
 *
 * WHAT: Check config for validity
 * WHY:  Detect configuration errors early
 * HOW:  Validates ranges, combinations, dependencies
 *
 * @param config Configuration to validate
 * @return 0 if valid, negative on error
 */
int lnn_training_validate_config(const lnn_training_config_t* config);

/*=============================================================================
 * Substrate adapter tunables (Phase 2 biological-substrate wiring).
 *===========================================================================*/
void  lnn_tune_set_substrate_enabled(float v);
void  lnn_tune_set_substrate_update_period(float v);
void  lnn_tune_set_substrate_tau_compose_on(float v);

float lnn_tune_get_substrate_enabled(void);
float lnn_tune_get_substrate_update_period(void);
float lnn_tune_get_substrate_tau_compose_on(void);

/*=============================================================================
 * Thalamic adapter tunables (Phase 2 of multi-network thalamic rollout).
 *   enabled:            master switch
 *   input_gain_on:      scale input vector by channel attention gate
 *   burst_tau_clamp_on: clamp tau to <=1ms when channel in BURST mode
 *===========================================================================*/
void  lnn_tune_set_thalamic_enabled(float v);
void  lnn_tune_set_thalamic_input_gain_on(float v);
void  lnn_tune_set_thalamic_burst_tau_clamp_on(float v);

float lnn_tune_get_thalamic_enabled(void);
float lnn_tune_get_thalamic_input_gain_on(void);
float lnn_tune_get_thalamic_burst_tau_clamp_on(void);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_LNN_TRAINING_H */
