/**
 * @file nimcp_lr_scheduler.h
 * @brief Learning Rate Scheduler Module for NIMCP Training Pipeline
 *
 * Implements standard learning rate scheduling strategies:
 * - StepLR: Decay LR by gamma every step_size epochs
 * - ExponentialLR: Decay LR by gamma each epoch
 * - CosineAnnealingLR: Cosine annealing to T_max then restart
 * - LinearWarmup: Linear warmup from start_lr to target_lr
 * - MultiStepLR: Decay at specified milestones
 * - ReduceOnPlateau: Reduce when metric stops improving
 * - CyclicLR: Cyclical learning rate between bounds
 * - OneCycleLR: One-cycle policy (Smith 2018)
 *
 * All schedulers integrate with:
 * - nimcp_optimizer for automatic LR updates
 * - Security integration via nimcp_security module
 * - Event bus for LR change notifications
 *
 * @note Part of Phase TM-4: Learning Rate Scheduling
 * @version 1.0.0
 * @author NIMCP Development Team
 */

#ifndef NIMCP_LR_SCHEDULER_H
#define NIMCP_LR_SCHEDULER_H

#include "utils/validation/nimcp_common.h"
#include "security/nimcp_security_integration.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants and Limits
 * ============================================================================ */

#define NIMCP_LR_MAX_MILESTONES 32    /**< Maximum milestones for MultiStepLR */
#define NIMCP_LR_MIN_VALUE 1e-10f     /**< Minimum allowed learning rate */
#define NIMCP_LR_MAX_VALUE 10.0f      /**< Maximum allowed learning rate */

/* ============================================================================
 * Scheduler Types and Enumerations
 * ============================================================================ */

/**
 * @brief Supported learning rate scheduler types
 */
typedef enum nimcp_lr_scheduler_type {
    NIMCP_LR_CONSTANT = 0,        /**< Constant learning rate (no scheduling) */
    NIMCP_LR_STEP,                /**< Step decay every N epochs */
    NIMCP_LR_EXPONENTIAL,         /**< Exponential decay each epoch */
    NIMCP_LR_COSINE_ANNEALING,    /**< Cosine annealing to T_max */
    NIMCP_LR_LINEAR_WARMUP,       /**< Linear warmup then constant */
    NIMCP_LR_MULTI_STEP,          /**< Decay at specified milestones */
    NIMCP_LR_REDUCE_ON_PLATEAU,   /**< Reduce when metric stops improving */
    NIMCP_LR_CYCLIC,              /**< Cyclical between bounds */
    NIMCP_LR_ONE_CYCLE,           /**< One-cycle policy */
    NIMCP_LR_COSINE_WARMUP,       /**< Cosine annealing with warmup */
    NIMCP_LR_POLYNOMIAL,          /**< Polynomial decay */
    NIMCP_LR_CUSTOM,              /**< User-defined scheduler */
    NIMCP_LR_SCHEDULER_TYPE_COUNT
} nimcp_lr_scheduler_type_t;

/**
 * @brief Cyclic LR scaling modes
 */
typedef enum nimcp_cyclic_mode {
    NIMCP_CYCLIC_TRIANGULAR = 0,  /**< Triangular cycle */
    NIMCP_CYCLIC_TRIANGULAR2,     /**< Triangular2 (amplitude halves each cycle) */
    NIMCP_CYCLIC_EXP_RANGE        /**< Exponential range scaling */
} nimcp_cyclic_mode_t;

/**
 * @brief Plateau reduction modes
 */
typedef enum nimcp_plateau_mode {
    NIMCP_PLATEAU_MIN = 0,        /**< Reduce when metric stops decreasing */
    NIMCP_PLATEAU_MAX             /**< Reduce when metric stops increasing */
} nimcp_plateau_mode_t;

/* ============================================================================
 * Scheduler Configuration Structures
 * ============================================================================ */

/**
 * @brief Configuration for StepLR scheduler
 *
 * Decays learning rate by gamma every step_size epochs:
 * lr = initial_lr * gamma^(epoch // step_size)
 */
typedef struct nimcp_step_lr_config {
    float initial_lr;             /**< Initial learning rate */
    uint32_t step_size;           /**< Decay period in epochs */
    float gamma;                  /**< Decay factor (default: 0.1) */
    float min_lr;                 /**< Minimum learning rate floor */
} nimcp_step_lr_config_t;

/**
 * @brief Configuration for ExponentialLR scheduler
 *
 * Decays learning rate exponentially each epoch:
 * lr = initial_lr * gamma^epoch
 */
typedef struct nimcp_exponential_lr_config {
    float initial_lr;             /**< Initial learning rate */
    float gamma;                  /**< Decay rate per epoch (e.g., 0.95) */
    float min_lr;                 /**< Minimum learning rate floor */
} nimcp_exponential_lr_config_t;

/**
 * @brief Configuration for CosineAnnealingLR scheduler
 *
 * Anneals learning rate following cosine curve:
 * lr = eta_min + (eta_max - eta_min) * (1 + cos(pi * T_cur / T_max)) / 2
 */
typedef struct nimcp_cosine_lr_config {
    float initial_lr;             /**< Initial (maximum) learning rate */
    uint32_t T_max;               /**< Maximum number of iterations */
    float eta_min;                /**< Minimum learning rate (default: 0) */
    bool restart;                 /**< Enable warm restarts */
    uint32_t T_mult;              /**< Period multiplier after restart (default: 1) */
} nimcp_cosine_lr_config_t;

/**
 * @brief Configuration for LinearWarmup scheduler
 *
 * Linear warmup from start_lr to target_lr over warmup_steps:
 * lr = start_lr + (target_lr - start_lr) * step / warmup_steps
 */
typedef struct nimcp_warmup_lr_config {
    float start_lr;               /**< Starting learning rate (often 0 or small) */
    float target_lr;              /**< Target learning rate after warmup */
    uint32_t warmup_steps;        /**< Number of warmup steps */
    bool hold_after_warmup;       /**< Hold at target_lr after warmup */
} nimcp_warmup_lr_config_t;

/**
 * @brief Configuration for MultiStepLR scheduler
 *
 * Decays learning rate at specified epoch milestones:
 * lr = initial_lr * gamma^(number of milestones passed)
 */
typedef struct nimcp_multi_step_lr_config {
    float initial_lr;             /**< Initial learning rate */
    uint32_t milestones[NIMCP_LR_MAX_MILESTONES]; /**< Epoch milestones */
    uint32_t num_milestones;      /**< Number of milestones */
    float gamma;                  /**< Decay factor at each milestone */
    float min_lr;                 /**< Minimum learning rate floor */
} nimcp_multi_step_lr_config_t;

/**
 * @brief Configuration for ReduceOnPlateau scheduler
 *
 * Reduces LR when a metric has stopped improving:
 * new_lr = lr * factor when metric doesn't improve for patience epochs
 */
typedef struct nimcp_plateau_lr_config {
    float initial_lr;             /**< Initial learning rate */
    nimcp_plateau_mode_t mode;    /**< min: reduce on metric stop decreasing */
    float factor;                 /**< Factor to reduce LR by (default: 0.1) */
    uint32_t patience;            /**< Epochs with no improvement before reduce */
    float threshold;              /**< Threshold for measuring improvement */
    uint32_t cooldown;            /**< Epochs to wait before resuming monitoring */
    float min_lr;                 /**< Minimum learning rate */
} nimcp_plateau_lr_config_t;

/**
 * @brief Configuration for CyclicLR scheduler
 *
 * Cycles learning rate between base_lr and max_lr:
 * Various modes control the shape of the cycle
 */
typedef struct nimcp_cyclic_lr_config {
    float base_lr;                /**< Lower learning rate boundary */
    float max_lr;                 /**< Upper learning rate boundary */
    uint32_t step_size_up;        /**< Steps in the increasing half */
    uint32_t step_size_down;      /**< Steps in the decreasing half (0 = same as up) */
    nimcp_cyclic_mode_t mode;     /**< Scaling mode */
    float gamma;                  /**< Scale factor for exp_range mode */
} nimcp_cyclic_lr_config_t;

/**
 * @brief Configuration for OneCycleLR scheduler
 *
 * Implements the 1cycle policy (Leslie Smith 2018):
 * - Warmup from div_factor*max_lr to max_lr
 * - Anneal from max_lr to final_div_factor*max_lr
 */
typedef struct nimcp_one_cycle_lr_config {
    float max_lr;                 /**< Maximum learning rate */
    uint32_t total_steps;         /**< Total training steps */
    float pct_start;              /**< Percent of cycle spent increasing LR (default: 0.3) */
    float div_factor;             /**< Initial lr = max_lr / div_factor (default: 25) */
    float final_div_factor;       /**< Final lr = max_lr / final_div_factor (default: 10000) */
    bool anneal_strategy_cos;     /**< Use cosine annealing (vs linear) */
} nimcp_one_cycle_lr_config_t;

/**
 * @brief Configuration for PolynomialLR scheduler
 *
 * Polynomial decay:
 * lr = (initial_lr - end_lr) * (1 - step/total_steps)^power + end_lr
 */
typedef struct nimcp_polynomial_lr_config {
    float initial_lr;             /**< Initial learning rate */
    float end_lr;                 /**< Final learning rate */
    uint32_t total_steps;         /**< Total training steps */
    float power;                  /**< Polynomial power (default: 1.0 = linear) */
} nimcp_polynomial_lr_config_t;

/**
 * @brief Custom scheduler step function signature
 */
typedef float (*nimcp_lr_scheduler_fn)(
    uint64_t step,
    uint64_t epoch,
    void* state,
    void* user_data
);

/**
 * @brief Configuration for custom scheduler
 */
typedef struct nimcp_custom_lr_config {
    float initial_lr;             /**< Initial learning rate */
    nimcp_lr_scheduler_fn step_fn; /**< Custom step function */
    void* user_data;              /**< User context */
    const char* name;             /**< Scheduler name */
} nimcp_custom_lr_config_t;

/**
 * @brief Main scheduler configuration structure
 */
typedef struct nimcp_lr_scheduler_config {
    nimcp_lr_scheduler_type_t type; /**< Scheduler type */

    union {
        nimcp_step_lr_config_t step;
        nimcp_exponential_lr_config_t exponential;
        nimcp_cosine_lr_config_t cosine;
        nimcp_warmup_lr_config_t warmup;
        nimcp_multi_step_lr_config_t multi_step;
        nimcp_plateau_lr_config_t plateau;
        nimcp_cyclic_lr_config_t cyclic;
        nimcp_one_cycle_lr_config_t one_cycle;
        nimcp_polynomial_lr_config_t polynomial;
        nimcp_custom_lr_config_t custom;
    } params;

    /* Common options */
    bool verbose;                 /**< Log LR changes */
    float lr_epsilon;             /**< Tolerance for LR comparisons */

    /* Security integration */
    nimcp_sec_integration_t* security_ctx; /**< Security context (optional) */
} nimcp_lr_scheduler_config_t;

/* ============================================================================
 * Opaque Context Type
 * ============================================================================ */

/**
 * @brief Opaque learning rate scheduler context
 */
typedef struct nimcp_lr_scheduler_ctx nimcp_lr_scheduler_ctx_t;

/* ============================================================================
 * Statistics Structure
 * ============================================================================ */

/**
 * @brief Learning rate scheduler statistics
 */
typedef struct nimcp_lr_scheduler_stats {
    uint64_t total_steps;         /**< Total steps executed */
    uint64_t total_epochs;        /**< Total epochs */
    float initial_lr;             /**< Initial learning rate */
    float current_lr;             /**< Current learning rate */
    float min_lr_seen;            /**< Minimum LR observed */
    float max_lr_seen;            /**< Maximum LR observed */
    uint32_t num_reductions;      /**< Number of LR reductions (plateau) */
    uint32_t num_cycles;          /**< Number of cycles completed (cyclic) */
    float best_metric;            /**< Best metric value (plateau) */
} nimcp_lr_scheduler_stats_t;

/* ============================================================================
 * Default Configurations
 * ============================================================================ */

/**
 * @brief Get default StepLR configuration
 * @param initial_lr Initial learning rate
 * @param step_size Decay period in epochs
 * @return Default configuration
 */
nimcp_step_lr_config_t nimcp_step_lr_default_config(float initial_lr, uint32_t step_size);

/**
 * @brief Get default ExponentialLR configuration
 * @param initial_lr Initial learning rate
 * @param gamma Decay rate per epoch
 * @return Default configuration
 */
nimcp_exponential_lr_config_t nimcp_exponential_lr_default_config(float initial_lr, float gamma);

/**
 * @brief Get default CosineAnnealingLR configuration
 * @param initial_lr Initial learning rate
 * @param T_max Maximum iterations
 * @return Default configuration
 */
nimcp_cosine_lr_config_t nimcp_cosine_lr_default_config(float initial_lr, uint32_t T_max);

/**
 * @brief Get default LinearWarmup configuration
 * @param target_lr Target learning rate
 * @param warmup_steps Warmup steps
 * @return Default configuration
 */
nimcp_warmup_lr_config_t nimcp_warmup_lr_default_config(float target_lr, uint32_t warmup_steps);

/**
 * @brief Get default ReduceOnPlateau configuration
 * @param initial_lr Initial learning rate
 * @return Default configuration
 */
nimcp_plateau_lr_config_t nimcp_plateau_lr_default_config(float initial_lr);

/**
 * @brief Get default CyclicLR configuration
 * @param base_lr Base learning rate
 * @param max_lr Maximum learning rate
 * @param step_size_up Steps in increasing phase
 * @return Default configuration
 */
nimcp_cyclic_lr_config_t nimcp_cyclic_lr_default_config(float base_lr, float max_lr, uint32_t step_size_up);

/**
 * @brief Get default OneCycleLR configuration
 * @param max_lr Maximum learning rate
 * @param total_steps Total training steps
 * @return Default configuration
 */
nimcp_one_cycle_lr_config_t nimcp_one_cycle_lr_default_config(float max_lr, uint32_t total_steps);

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

/**
 * @brief Create a learning rate scheduler
 * @param config Scheduler configuration
 * @return Scheduler context or NULL on failure
 */
nimcp_lr_scheduler_ctx_t* nimcp_lr_scheduler_create(const nimcp_lr_scheduler_config_t* config);

/**
 * @brief Destroy a learning rate scheduler
 * @param ctx Scheduler context
 */
void nimcp_lr_scheduler_destroy(nimcp_lr_scheduler_ctx_t* ctx);

/* ============================================================================
 * Core Operations
 * ============================================================================ */

/**
 * @brief Step the scheduler (call each step/batch)
 *
 * For step-based schedulers (cyclic, one_cycle), call after each batch.
 *
 * @param ctx Scheduler context
 * @return New learning rate
 */
float nimcp_lr_scheduler_step(nimcp_lr_scheduler_ctx_t* ctx);

/**
 * @brief Step the scheduler at epoch boundary
 *
 * For epoch-based schedulers (step, exponential, cosine), call after each epoch.
 *
 * @param ctx Scheduler context
 * @return New learning rate
 */
float nimcp_lr_scheduler_step_epoch(nimcp_lr_scheduler_ctx_t* ctx);

/**
 * @brief Update plateau scheduler with metric value
 *
 * Call after each epoch for ReduceOnPlateau scheduler.
 *
 * @param ctx Scheduler context
 * @param metric Current metric value
 * @return New learning rate
 */
float nimcp_lr_scheduler_step_metric(nimcp_lr_scheduler_ctx_t* ctx, float metric);

/**
 * @brief Get current learning rate
 * @param ctx Scheduler context
 * @return Current learning rate
 */
float nimcp_lr_scheduler_get_lr(const nimcp_lr_scheduler_ctx_t* ctx);

/**
 * @brief Set learning rate manually
 * @param ctx Scheduler context
 * @param lr New learning rate
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_lr_scheduler_set_lr(nimcp_lr_scheduler_ctx_t* ctx, float lr);

/**
 * @brief Get the learning rate for a specific step (without modifying state)
 * @param ctx Scheduler context
 * @param step Step number
 * @return Learning rate at that step
 */
float nimcp_lr_scheduler_get_lr_at_step(const nimcp_lr_scheduler_ctx_t* ctx, uint64_t step);

/**
 * @brief Get the learning rate for a specific epoch (without modifying state)
 * @param ctx Scheduler context
 * @param epoch Epoch number
 * @return Learning rate at that epoch
 */
float nimcp_lr_scheduler_get_lr_at_epoch(const nimcp_lr_scheduler_ctx_t* ctx, uint64_t epoch);

/* ============================================================================
 * State Management
 * ============================================================================ */

/**
 * @brief Reset scheduler to initial state
 * @param ctx Scheduler context
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_lr_scheduler_reset(nimcp_lr_scheduler_ctx_t* ctx);

/**
 * @brief Get current step count
 * @param ctx Scheduler context
 * @return Current step
 */
uint64_t nimcp_lr_scheduler_get_step(const nimcp_lr_scheduler_ctx_t* ctx);

/**
 * @brief Get current epoch count
 * @param ctx Scheduler context
 * @return Current epoch
 */
uint64_t nimcp_lr_scheduler_get_epoch(const nimcp_lr_scheduler_ctx_t* ctx);

/**
 * @brief Set current step (for resuming training)
 * @param ctx Scheduler context
 * @param step Step number
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_lr_scheduler_set_step(nimcp_lr_scheduler_ctx_t* ctx, uint64_t step);

/**
 * @brief Set current epoch (for resuming training)
 * @param ctx Scheduler context
 * @param epoch Epoch number
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_lr_scheduler_set_epoch(nimcp_lr_scheduler_ctx_t* ctx, uint64_t epoch);

/* ============================================================================
 * Statistics and Monitoring
 * ============================================================================ */

/**
 * @brief Get scheduler statistics
 * @param ctx Scheduler context
 * @param stats Output statistics structure
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_lr_scheduler_get_stats(
    const nimcp_lr_scheduler_ctx_t* ctx,
    nimcp_lr_scheduler_stats_t* stats
);

/**
 * @brief Reset statistics
 * @param ctx Scheduler context
 */
void nimcp_lr_scheduler_reset_stats(nimcp_lr_scheduler_ctx_t* ctx);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Get scheduler type name
 * @param type Scheduler type
 * @return String name
 */
const char* nimcp_lr_scheduler_type_name(nimcp_lr_scheduler_type_t type);

/**
 * @brief Get scheduler type from configuration
 * @param ctx Scheduler context
 * @return Scheduler type
 */
nimcp_lr_scheduler_type_t nimcp_lr_scheduler_get_type(const nimcp_lr_scheduler_ctx_t* ctx);

/**
 * @brief Validate scheduler configuration
 * @param config Configuration to validate
 * @return NIMCP_SUCCESS if valid
 */
nimcp_result_t nimcp_lr_scheduler_validate_config(const nimcp_lr_scheduler_config_t* config);

/**
 * @brief Create scheduler configuration from type with defaults
 * @param type Scheduler type
 * @param initial_lr Initial learning rate
 * @return Configuration with reasonable defaults
 */
nimcp_lr_scheduler_config_t nimcp_lr_scheduler_config_from_type(
    nimcp_lr_scheduler_type_t type,
    float initial_lr
);

/* ============================================================================
 * Chained/Composite Schedulers
 * ============================================================================ */

/**
 * @brief Create a warmup + main scheduler combination
 *
 * Creates a scheduler that applies warmup for warmup_steps, then switches
 * to the main scheduler.
 *
 * @param warmup_config Warmup configuration
 * @param main_config Main scheduler configuration
 * @return Combined scheduler context or NULL on failure
 */
nimcp_lr_scheduler_ctx_t* nimcp_lr_scheduler_create_with_warmup(
    const nimcp_warmup_lr_config_t* warmup_config,
    const nimcp_lr_scheduler_config_t* main_config
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_LR_SCHEDULER_H */
