/**
 * @file nimcp_gradient_manager.h
 * @brief Gradient Management Module for NIMCP Training Pipeline
 *
 * Provides comprehensive gradient management:
 * - Gradient accumulation for large batch training
 * - Gradient scaling for mixed precision training
 * - Gradient checkpointing for memory efficiency
 * - Gradient statistics and monitoring
 * - Gradient synchronization for distributed training
 *
 * Key features:
 * - Efficient memory management with buffer pooling
 * - Support for sparse gradients
 * - Automatic NaN/Inf detection and handling
 * - Integration with optimizer and regularization modules
 *
 * @note Part of Phase TM-6: Gradient Management
 * @version 1.0.0
 * @author NIMCP Development Team
 */

#ifndef NIMCP_GRADIENT_MANAGER_H
#define NIMCP_GRADIENT_MANAGER_H

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

#define NIMCP_GRAD_MAX_ACCUM_STEPS 1024   /**< Maximum accumulation steps */
#define NIMCP_GRAD_MIN_SCALE 1e-10f       /**< Minimum gradient scale factor */
#define NIMCP_GRAD_MAX_SCALE 1e10f        /**< Maximum gradient scale factor */
#define NIMCP_GRAD_BACKOFF_FACTOR 0.5f    /**< Scale backoff on overflow */
#define NIMCP_GRAD_GROWTH_FACTOR 2.0f     /**< Scale growth factor */
#define NIMCP_GRAD_GROWTH_INTERVAL 2000   /**< Steps between growth attempts */

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Gradient accumulation modes
 */
typedef enum nimcp_grad_accum_mode {
    NIMCP_GRAD_ACCUM_SUM = 0,     /**< Sum gradients (default) */
    NIMCP_GRAD_ACCUM_MEAN,        /**< Average gradients after accumulation */
    NIMCP_GRAD_ACCUM_MODE_COUNT
} nimcp_grad_accum_mode_t;

/**
 * @brief Gradient scaling strategy
 */
typedef enum nimcp_grad_scale_strategy {
    NIMCP_GRAD_SCALE_NONE = 0,    /**< No scaling */
    NIMCP_GRAD_SCALE_FIXED,       /**< Fixed scale factor */
    NIMCP_GRAD_SCALE_DYNAMIC,     /**< Dynamic (loss) scaling */
    NIMCP_GRAD_SCALE_STRATEGY_COUNT
} nimcp_grad_scale_strategy_t;

/**
 * @brief Gradient health status
 */
typedef enum nimcp_grad_health {
    NIMCP_GRAD_HEALTHY = 0,       /**< All gradients are valid */
    NIMCP_GRAD_HAS_NAN,           /**< NaN detected */
    NIMCP_GRAD_HAS_INF,           /**< Infinity detected */
    NIMCP_GRAD_HAS_ZERO,          /**< All-zero gradients */
    NIMCP_GRAD_OVERFLOW,          /**< Gradient overflow detected */
    NIMCP_GRAD_UNDERFLOW          /**< Gradient underflow detected */
} nimcp_grad_health_t;

/* ============================================================================
 * Configuration Structures
 * ============================================================================ */

/**
 * @brief Gradient accumulation configuration
 */
typedef struct nimcp_grad_accum_config {
    uint32_t accumulation_steps;  /**< Number of steps to accumulate */
    nimcp_grad_accum_mode_t mode; /**< Accumulation mode */
    bool sync_on_step;            /**< Synchronize after each step */
} nimcp_grad_accum_config_t;

/**
 * @brief Gradient scaling configuration
 */
typedef struct nimcp_grad_scale_config {
    nimcp_grad_scale_strategy_t strategy; /**< Scaling strategy */
    float initial_scale;          /**< Initial scale factor */
    float min_scale;              /**< Minimum scale */
    float max_scale;              /**< Maximum scale */
    float backoff_factor;         /**< Factor to reduce scale on overflow */
    float growth_factor;          /**< Factor to increase scale */
    uint32_t growth_interval;     /**< Steps between growth attempts */
} nimcp_grad_scale_config_t;

/**
 * @brief Main gradient manager configuration
 */
typedef struct nimcp_gradient_manager_config {
    /* Accumulation settings */
    nimcp_grad_accum_config_t accumulation;
    bool use_accumulation;

    /* Scaling settings */
    nimcp_grad_scale_config_t scaling;
    bool use_scaling;

    /* Health monitoring */
    bool check_nan_inf;           /**< Check for NaN/Inf */
    bool skip_nan_gradients;      /**< Skip updates with NaN */
    bool replace_inf;             /**< Replace Inf with max_value */
    float max_grad_value;         /**< Maximum gradient magnitude */

    /* Memory settings */
    bool preallocate_buffers;     /**< Preallocate gradient buffers */
    size_t max_buffer_size;       /**< Maximum buffer size */

    /* Debugging */
    bool verbose;
    bool track_statistics;

    /* Security integration */
    nimcp_sec_integration_t* security_ctx; /**< Security context (optional) */
} nimcp_gradient_manager_config_t;

/* ============================================================================
 * Opaque Context Type
 * ============================================================================ */

/**
 * @brief Opaque gradient manager context
 */
typedef struct nimcp_gradient_manager_ctx nimcp_gradient_manager_ctx_t;

/* ============================================================================
 * Statistics Structures
 * ============================================================================ */

/**
 * @brief Gradient statistics
 */
typedef struct nimcp_grad_stats {
    /* Basic stats */
    uint64_t total_steps;         /**< Total gradient steps */
    uint64_t total_accum_steps;   /**< Total accumulation steps */
    uint64_t skipped_steps;       /**< Steps skipped due to NaN/Inf */

    /* Gradient magnitude stats */
    float min_grad_norm;          /**< Minimum gradient norm seen */
    float max_grad_norm;          /**< Maximum gradient norm seen */
    float avg_grad_norm;          /**< Average gradient norm */
    double sum_grad_norm;         /**< Sum for averaging */

    /* Health stats */
    uint64_t nan_count;           /**< Number of NaN detections */
    uint64_t inf_count;           /**< Number of Inf detections */
    uint64_t overflow_count;      /**< Number of overflow events */
    uint64_t underflow_count;     /**< Number of underflow events */

    /* Scaling stats (for dynamic scaling) */
    float current_scale;          /**< Current scale factor */
    uint64_t scale_increases;     /**< Number of scale increases */
    uint64_t scale_decreases;     /**< Number of scale decreases */
} nimcp_grad_stats_t;

/* ============================================================================
 * Default Configurations
 * ============================================================================ */

/**
 * @brief Get default gradient accumulation configuration
 * @param accumulation_steps Number of accumulation steps
 * @return Default configuration
 */
nimcp_grad_accum_config_t nimcp_grad_accum_default_config(uint32_t accumulation_steps);

/**
 * @brief Get default gradient scaling configuration
 * @param initial_scale Initial scale factor
 * @return Default configuration
 */
nimcp_grad_scale_config_t nimcp_grad_scale_default_config(float initial_scale);

/**
 * @brief Get default gradient manager configuration
 * @return Default configuration
 */
nimcp_gradient_manager_config_t nimcp_gradient_manager_default_config(void);

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

/**
 * @brief Create a gradient manager context
 * @param config Gradient manager configuration
 * @return Gradient manager context or NULL on failure
 */
nimcp_gradient_manager_ctx_t* nimcp_gradient_manager_create(
    const nimcp_gradient_manager_config_t* config
);

/**
 * @brief Destroy a gradient manager context
 * @param ctx Gradient manager context
 */
void nimcp_gradient_manager_destroy(nimcp_gradient_manager_ctx_t* ctx);

/* ============================================================================
 * Core Gradient Operations
 * ============================================================================ */

/**
 * @brief Accumulate gradients into internal buffer
 * @param ctx Gradient manager context
 * @param gradients Gradient array to accumulate
 * @param num_gradients Number of gradients
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_gradient_accumulate(
    nimcp_gradient_manager_ctx_t* ctx,
    const float* gradients,
    size_t num_gradients
);

/**
 * @brief Check if accumulation is complete
 * @param ctx Gradient manager context
 * @return True if ready to apply gradients
 */
bool nimcp_gradient_accum_ready(const nimcp_gradient_manager_ctx_t* ctx);

/**
 * @brief Get accumulated gradients
 * @param ctx Gradient manager context
 * @param output Output gradient array
 * @param num_gradients Expected number of gradients
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_gradient_get_accumulated(
    nimcp_gradient_manager_ctx_t* ctx,
    float* output,
    size_t num_gradients
);

/**
 * @brief Reset accumulation buffer
 * @param ctx Gradient manager context
 */
void nimcp_gradient_reset_accum(nimcp_gradient_manager_ctx_t* ctx);

/**
 * @brief Get current accumulation step
 * @param ctx Gradient manager context
 * @return Current accumulation step (0 to accum_steps-1)
 */
uint32_t nimcp_gradient_get_accum_step(const nimcp_gradient_manager_ctx_t* ctx);

/* ============================================================================
 * Gradient Scaling Operations
 * ============================================================================ */

/**
 * @brief Scale gradients
 * @param ctx Gradient manager context
 * @param gradients Gradient array (modified in place)
 * @param num_gradients Number of gradients
 * @return Scale factor applied
 */
float nimcp_gradient_scale(
    nimcp_gradient_manager_ctx_t* ctx,
    float* gradients,
    size_t num_gradients
);

/**
 * @brief Unscale gradients (for loss scaling)
 * @param ctx Gradient manager context
 * @param gradients Gradient array (modified in place)
 * @param num_gradients Number of gradients
 * @return Inverse scale factor applied
 */
float nimcp_gradient_unscale(
    nimcp_gradient_manager_ctx_t* ctx,
    float* gradients,
    size_t num_gradients
);

/**
 * @brief Update scale factor based on gradient health
 * @param ctx Gradient manager context
 * @param health Gradient health status
 */
void nimcp_gradient_update_scale(
    nimcp_gradient_manager_ctx_t* ctx,
    nimcp_grad_health_t health
);

/**
 * @brief Get current scale factor
 * @param ctx Gradient manager context
 * @return Current scale factor
 */
float nimcp_gradient_get_scale(const nimcp_gradient_manager_ctx_t* ctx);

/**
 * @brief Set scale factor manually
 * @param ctx Gradient manager context
 * @param scale New scale factor
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_gradient_set_scale(
    nimcp_gradient_manager_ctx_t* ctx,
    float scale
);

/* ============================================================================
 * Gradient Health Checking
 * ============================================================================ */

/**
 * @brief Check gradient health (NaN, Inf, overflow)
 * @param gradients Gradient array
 * @param num_gradients Number of gradients
 * @return Gradient health status
 */
nimcp_grad_health_t nimcp_gradient_check_health(
    const float* gradients,
    size_t num_gradients
);

/**
 * @brief Check gradient health using context settings
 * @param ctx Gradient manager context
 * @param gradients Gradient array
 * @param num_gradients Number of gradients
 * @return Gradient health status
 */
nimcp_grad_health_t nimcp_gradient_check_health_ctx(
    nimcp_gradient_manager_ctx_t* ctx,
    const float* gradients,
    size_t num_gradients
);

/**
 * @brief Sanitize gradients (replace NaN/Inf)
 * @param gradients Gradient array (modified in place)
 * @param num_gradients Number of gradients
 * @param replace_value Value to use for replacement
 * @return Number of values replaced
 */
uint64_t nimcp_gradient_sanitize(
    float* gradients,
    size_t num_gradients,
    float replace_value
);

/* ============================================================================
 * Gradient Statistics
 * ============================================================================ */

/**
 * @brief Compute gradient L2 norm
 * @param gradients Gradient array
 * @param num_gradients Number of gradients
 * @return L2 norm
 */
float nimcp_gradient_l2_norm(const float* gradients, size_t num_gradients);

/**
 * @brief Compute gradient L1 norm
 * @param gradients Gradient array
 * @param num_gradients Number of gradients
 * @return L1 norm
 */
float nimcp_gradient_l1_norm(const float* gradients, size_t num_gradients);

/**
 * @brief Compute gradient max norm (infinity norm)
 * @param gradients Gradient array
 * @param num_gradients Number of gradients
 * @return Max absolute value
 */
float nimcp_gradient_max_norm(const float* gradients, size_t num_gradients);

/**
 * @brief Get gradient manager statistics
 * @param ctx Gradient manager context
 * @param stats Output statistics structure
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_gradient_manager_get_stats(
    const nimcp_gradient_manager_ctx_t* ctx,
    nimcp_grad_stats_t* stats
);

/**
 * @brief Reset gradient manager statistics
 * @param ctx Gradient manager context
 */
void nimcp_gradient_manager_reset_stats(nimcp_gradient_manager_ctx_t* ctx);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Get accumulation mode name
 * @param mode Accumulation mode
 * @return String name
 */
const char* nimcp_grad_accum_mode_name(nimcp_grad_accum_mode_t mode);

/**
 * @brief Get scaling strategy name
 * @param strategy Scaling strategy
 * @return String name
 */
const char* nimcp_grad_scale_strategy_name(nimcp_grad_scale_strategy_t strategy);

/**
 * @brief Get health status name
 * @param health Health status
 * @return String name
 */
const char* nimcp_grad_health_name(nimcp_grad_health_t health);

/**
 * @brief Validate gradient manager configuration
 * @param config Configuration to validate
 * @return NIMCP_SUCCESS if valid
 */
nimcp_result_t nimcp_gradient_manager_validate_config(
    const nimcp_gradient_manager_config_t* config
);

/* ============================================================================
 * Gradient Operations for Distributed Training
 * ============================================================================ */

/**
 * @brief Prepare gradients for all-reduce (average)
 * @param gradients Gradient array
 * @param num_gradients Number of gradients
 * @param num_workers Number of distributed workers
 */
void nimcp_gradient_prepare_allreduce(
    float* gradients,
    size_t num_gradients,
    uint32_t num_workers
);

/**
 * @brief Apply all-reduce result
 * @param gradients Gradient array after all-reduce
 * @param num_gradients Number of gradients
 * @param num_workers Number of distributed workers
 */
void nimcp_gradient_finalize_allreduce(
    float* gradients,
    size_t num_gradients,
    uint32_t num_workers
);

/* ============================================================================
 * Tensor-Based Gradient Operations
 * ============================================================================ */

/* Forward declaration for tensor type */
#ifndef NIMCP_TENSOR_T_DEFINED
#define NIMCP_TENSOR_T_DEFINED
struct nimcp_tensor_s;
typedef struct nimcp_tensor_s nimcp_tensor_t;
#endif

/**
 * @brief Accumulate gradients using tensor operations
 *
 * More efficient than nimcp_gradient_accumulate() for large arrays.
 *
 * @param ctx Gradient manager context
 * @param grad_tensor Gradient tensor to accumulate
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_gradient_accumulate_tensor(
    nimcp_gradient_manager_ctx_t* ctx,
    const nimcp_tensor_t* grad_tensor
);

/**
 * @brief Scale gradients using tensor operations
 *
 * @param ctx Gradient manager context
 * @param grad_tensor Gradient tensor (modified in place)
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_gradient_scale_tensor(
    nimcp_gradient_manager_ctx_t* ctx,
    nimcp_tensor_t* grad_tensor
);

/**
 * @brief Unscale gradients using tensor operations
 *
 * @param ctx Gradient manager context
 * @param grad_tensor Gradient tensor (modified in place)
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_gradient_unscale_tensor(
    nimcp_gradient_manager_ctx_t* ctx,
    nimcp_tensor_t* grad_tensor
);

/**
 * @brief Compute gradient norm using tensor operations
 *
 * @param grad_tensor Input gradient tensor
 * @param p Norm order (1=L1, 2=L2, INFINITY=max)
 * @return Computed norm value
 */
float nimcp_gradient_tensor_norm(
    const nimcp_tensor_t* grad_tensor,
    double p
);

/**
 * @brief Clip gradients by norm using tensor operations
 *
 * Scales down gradients if the norm exceeds the threshold.
 *
 * @param grad_tensor Gradient tensor (modified in place)
 * @param max_norm Maximum allowed norm
 * @param norm_type Norm type (1, 2, or INFINITY)
 * @return Actual norm before clipping
 */
float nimcp_gradient_clip_norm_tensor(
    nimcp_tensor_t* grad_tensor,
    float max_norm,
    double norm_type
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_GRADIENT_MANAGER_H */
