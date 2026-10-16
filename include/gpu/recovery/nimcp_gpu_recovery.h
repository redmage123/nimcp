/**
 * @file nimcp_gpu_recovery.h
 * @brief GPU Self-Healing Recovery Infrastructure
 * @version 1.0.0
 * @date 2025-01-30
 *
 * WHAT: Self-healing recovery system for GPU operations
 * WHY:  Enable automatic recovery from GPU errors without halting the system
 * HOW:  Recovery strategies including CPU fallback, parameter correction, retry
 *
 * RECOVERY FLOW:
 * ```
 * GPU Operation Fails
 *        |
 *        v
 * +-------------------+
 * | Identify Error    |  (CUDA error, invalid params, OOM, etc.)
 * +-------------------+
 *        |
 *        v
 * +-------------------+
 * | Select Strategy   |  (Based on error type and context)
 * +-------------------+
 *        |
 *        v
 * +-------------------+
 * | Execute Recovery  |
 * |  1. Try primary   |  (e.g., clamp params)
 * |  2. Try fallback  |  (e.g., CPU execution)
 * |  3. Try degraded  |  (e.g., reduced batch size)
 * +-------------------+
 *        |
 *    Success?
 *    /     \
 *   Yes     No
 *   |       |
 *   v       v
 * Continue  Report to Immune
 *           (return failure)
 * ```
 *
 * RECOVERY STRATEGIES BY ERROR TYPE:
 * - Invalid Params  -> Clamp to valid range, retry
 * - Out of Memory   -> Reduce batch/dimensions, GC, CPU fallback
 * - CUDA Error      -> Reset device, CPU fallback
 * - Kernel Launch   -> Reduce block size, CPU fallback
 * - Numerical Error -> Add regularization, reduce precision, clamp values
 * - Timeout         -> Reduce workload, async execution
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_GPU_RECOVERY_H
#define NIMCP_GPU_RECOVERY_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include "common/nimcp_export.h"
#include "utils/error/nimcp_error_codes.h"

#ifdef NIMCP_ENABLE_CUDA
#include <cuda_runtime.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations - only if full definition not already included */
#ifndef NIMCP_GPU_CONTEXT_H
struct nimcp_gpu_context_s;
typedef struct nimcp_gpu_context_s nimcp_gpu_context_t;
#endif

/* ============================================================================
 * GPU Recovery Action Types
 * ============================================================================ */

/**
 * @brief GPU-specific recovery actions
 */
typedef enum nimcp_gpu_recovery_action {
    GPU_RECOVERY_NONE = 0,              /**< No recovery action */

    /* Parameter Correction (Tier 1 - Immediate) */
    GPU_RECOVERY_CLAMP_PARAMS,          /**< Clamp parameters to valid ranges */
    GPU_RECOVERY_SET_DEFAULTS,          /**< Reset to safe default values */
    GPU_RECOVERY_VALIDATE_FIX,          /**< Auto-fix detectable issues */

    /* Resource Adjustment (Tier 2 - Tactical) */
    GPU_RECOVERY_REDUCE_BATCH,          /**< Reduce batch size by 50% */
    GPU_RECOVERY_REDUCE_DIMENSIONS,     /**< Reduce tensor dimensions */
    GPU_RECOVERY_REDUCE_PRECISION,      /**< Switch from fp32 to fp16 or int8 */
    GPU_RECOVERY_FREE_CACHE,            /**< Free GPU memory caches */
    GPU_RECOVERY_RESET_DEVICE,          /**< Reset CUDA device state */

    /* Execution Fallback (Tier 3 - Strategic) */
    GPU_RECOVERY_CPU_FALLBACK,          /**< Execute on CPU instead of GPU */
    GPU_RECOVERY_ASYNC_SPLIT,           /**< Split work across multiple calls */
    GPU_RECOVERY_STREAM_SYNC,           /**< Force synchronous execution */

    /* Retry Strategies (Tier 4 - Retry) */
    GPU_RECOVERY_RETRY_IMMEDIATE,       /**< Retry immediately (transient error) */
    GPU_RECOVERY_RETRY_BACKOFF,         /**< Retry with exponential backoff */
    GPU_RECOVERY_RETRY_REDUCED,         /**< Retry with reduced workload */

    GPU_RECOVERY_MAX
} nimcp_gpu_recovery_action_t;

/**
 * @brief GPU error categories for recovery selection
 */
typedef enum nimcp_gpu_error_category {
    GPU_ERROR_UNKNOWN = 0,
    GPU_ERROR_INVALID_PARAMS,           /**< Invalid parameters passed */
    GPU_ERROR_OUT_OF_MEMORY,            /**< GPU memory exhausted */
    GPU_ERROR_CUDA_RUNTIME,             /**< CUDA runtime error */
    GPU_ERROR_KERNEL_LAUNCH,            /**< Kernel launch failed */
    GPU_ERROR_NUMERICAL,                /**< NaN/Inf/overflow */
    GPU_ERROR_TIMEOUT,                  /**< Operation timed out */
    GPU_ERROR_CONTEXT_INVALID,          /**< GPU context not valid */
    GPU_ERROR_LIBRARY,                  /**< cuBLAS/cuSOLVER/cuRAND error */
    GPU_ERROR_DEVICE_NOT_AVAILABLE      /**< No GPU device available */
} nimcp_gpu_error_category_t;

/* ============================================================================
 * GPU Recovery Context
 * ============================================================================ */

/**
 * @brief Recovery configuration for GPU operations
 */
typedef struct nimcp_gpu_recovery_config {
    bool enable_cpu_fallback;           /**< Allow CPU fallback (default: true) */
    bool enable_param_correction;       /**< Auto-correct parameters (default: true) */
    bool enable_batch_reduction;        /**< Auto-reduce batch on OOM (default: true) */
    bool enable_retry;                  /**< Enable automatic retry (default: true) */
    uint32_t max_retries;               /**< Maximum retry attempts (default: 3) */
    uint32_t retry_delay_ms;            /**< Initial retry delay (default: 10) */
    float batch_reduction_factor;       /**< Batch reduction factor (default: 0.5) */
    float memory_threshold;             /**< Memory threshold for preemptive action (default: 0.9) */
} nimcp_gpu_recovery_config_t;

/**
 * @brief GPU recovery context (per-thread or per-operation)
 */
typedef struct nimcp_gpu_recovery_context {
    nimcp_gpu_recovery_config_t config; /**< Configuration */

    /* State tracking */
    uint32_t retry_count;               /**< Current retry count */
    uint32_t batch_reductions;          /**< Number of batch reductions applied */
    bool cpu_fallback_active;           /**< Currently using CPU fallback */

    /* Last error info */
    nimcp_gpu_error_category_t last_error_category;
    cudaError_t last_cuda_error;
    char last_error_message[256];

    /* Recovery statistics */
    uint64_t recoveries_attempted;
    uint64_t recoveries_succeeded;
    uint64_t cpu_fallbacks_used;
    uint64_t batch_reductions_applied;

    /* CPU fallback function pointers (set by module) */
    void* cpu_fallback_context;
    bool (*cpu_fallback_fn)(void* context, void* params, void* result);
} nimcp_gpu_recovery_context_t;

/**
 * @brief Result of a recovery attempt
 */
typedef struct nimcp_gpu_recovery_result {
    bool success;                       /**< Recovery succeeded */
    nimcp_gpu_recovery_action_t action_taken; /**< Action that succeeded */
    uint32_t retries_used;              /**< Retries consumed */
    bool using_fallback;                /**< Now using CPU fallback */
    float adjusted_batch_factor;        /**< Batch reduction applied (1.0 = no reduction) */
    char message[128];                  /**< Human-readable result message */
} nimcp_gpu_recovery_result_t;

/* ============================================================================
 * Parameter Validation/Correction Types
 * ============================================================================ */

/**
 * @brief Parameter range specification for auto-correction
 */
typedef struct nimcp_gpu_param_range {
    float min_value;                    /**< Minimum valid value */
    float max_value;                    /**< Maximum valid value */
    float default_value;                /**< Default if out of range */
    bool clamp_to_range;                /**< Clamp vs. use default */
} nimcp_gpu_param_range_t;

/**
 * @brief Common GPU parameter constraints
 */
typedef struct nimcp_gpu_param_constraints {
    nimcp_gpu_param_range_t batch_size;
    nimcp_gpu_param_range_t learning_rate;
    nimcp_gpu_param_range_t num_inputs;
    nimcp_gpu_param_range_t num_outputs;
    nimcp_gpu_param_range_t num_iterations;
    nimcp_gpu_param_range_t tolerance;
    size_t max_memory_bytes;
    bool require_power_of_two_batch;
} nimcp_gpu_param_constraints_t;

/* ============================================================================
 * Initialization and Configuration API
 * ============================================================================ */

/**
 * @brief Get default recovery configuration
 */
NIMCP_EXPORT void nimcp_gpu_recovery_default_config(nimcp_gpu_recovery_config_t* config);

/**
 * @brief Initialize global GPU recovery system
 *
 * Call once at startup to enable GPU self-healing.
 *
 * @param config Configuration (NULL for defaults)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int nimcp_gpu_recovery_init(const nimcp_gpu_recovery_config_t* config);

/**
 * @brief Shutdown global GPU recovery system
 */
NIMCP_EXPORT void nimcp_gpu_recovery_shutdown(void);

/**
 * @brief Check if GPU recovery is initialized
 */
NIMCP_EXPORT bool nimcp_gpu_recovery_is_initialized(void);

/**
 * @brief Create a recovery context for an operation
 *
 * @param config Configuration (NULL for global defaults)
 * @return New recovery context (caller must free with destroy)
 */
NIMCP_EXPORT nimcp_gpu_recovery_context_t* nimcp_gpu_recovery_context_create(
    const nimcp_gpu_recovery_config_t* config);

/**
 * @brief Destroy a recovery context
 */
NIMCP_EXPORT void nimcp_gpu_recovery_context_destroy(nimcp_gpu_recovery_context_t* ctx);

/**
 * @brief Reset recovery context for new operation
 */
NIMCP_EXPORT void nimcp_gpu_recovery_context_reset(nimcp_gpu_recovery_context_t* ctx);

/* ============================================================================
 * Recovery Execution API
 * ============================================================================ */

/**
 * @brief Attempt recovery from GPU error
 *
 * WHAT: Try to recover from a GPU error using configured strategies
 * WHY:  Enable self-healing without manual intervention
 * HOW:  Select strategy based on error, execute recovery actions
 *
 * @param ctx Recovery context (NULL uses thread-local default)
 * @param error_category Category of error that occurred
 * @param cuda_error CUDA error code (cudaSuccess if not CUDA error)
 * @param result Output: recovery result
 * @return true if recovery succeeded and operation can be retried
 */
NIMCP_EXPORT bool nimcp_gpu_try_recover(
    nimcp_gpu_recovery_context_t* ctx,
    nimcp_gpu_error_category_t error_category,
    cudaError_t cuda_error,
    nimcp_gpu_recovery_result_t* result);

/**
 * @brief Execute specific recovery action
 *
 * @param ctx Recovery context
 * @param action Action to execute
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_execute_recovery_action(
    nimcp_gpu_recovery_context_t* ctx,
    nimcp_gpu_recovery_action_t action);

/**
 * @brief Select best recovery strategy for error
 *
 * @param error_category Error category
 * @param cuda_error CUDA error code
 * @param retry_count Current retry count
 * @return Recommended recovery action
 */
NIMCP_EXPORT nimcp_gpu_recovery_action_t nimcp_gpu_select_recovery_strategy(
    nimcp_gpu_error_category_t error_category,
    cudaError_t cuda_error,
    uint32_t retry_count);

/* ============================================================================
 * Parameter Correction API
 * ============================================================================ */

/**
 * @brief Get default parameter constraints
 */
NIMCP_EXPORT void nimcp_gpu_default_param_constraints(nimcp_gpu_param_constraints_t* constraints);

/**
 * @brief Validate and correct a float parameter
 *
 * @param value Pointer to value to validate/correct
 * @param range Valid range specification
 * @param param_name Parameter name for logging
 * @return true if value was corrected
 */
NIMCP_EXPORT bool nimcp_gpu_correct_param_float(
    float* value,
    const nimcp_gpu_param_range_t* range,
    const char* param_name);

/**
 * @brief Validate and correct an integer parameter
 *
 * @param value Pointer to value to validate/correct
 * @param min_val Minimum valid value
 * @param max_val Maximum valid value
 * @param default_val Default if out of range
 * @param param_name Parameter name for logging
 * @return true if value was corrected
 */
NIMCP_EXPORT bool nimcp_gpu_correct_param_int(
    int* value,
    int min_val,
    int max_val,
    int default_val,
    const char* param_name);

/**
 * @brief Validate and correct a size_t parameter
 */
NIMCP_EXPORT bool nimcp_gpu_correct_param_size(
    size_t* value,
    size_t min_val,
    size_t max_val,
    size_t default_val,
    const char* param_name);

/**
 * @brief Correct batch size based on available memory
 *
 * @param batch_size Pointer to batch size (will be reduced if needed)
 * @param element_size Size of each element in bytes
 * @param memory_per_element Additional memory per element
 * @return true if batch size was reduced
 */
NIMCP_EXPORT bool nimcp_gpu_correct_batch_for_memory(
    size_t* batch_size,
    size_t element_size,
    size_t memory_per_element);

/* ============================================================================
 * CPU Fallback API
 * ============================================================================ */

/**
 * @brief Check if CPU fallback is available
 */
NIMCP_EXPORT bool nimcp_gpu_cpu_fallback_available(void);

/**
 * @brief Set CPU fallback function for context
 *
 * @param ctx Recovery context
 * @param fallback_fn CPU implementation function
 * @param user_context User context passed to fallback
 */
NIMCP_EXPORT void nimcp_gpu_set_cpu_fallback(
    nimcp_gpu_recovery_context_t* ctx,
    bool (*fallback_fn)(void* context, void* params, void* result),
    void* user_context);

/**
 * @brief Execute CPU fallback
 *
 * @param ctx Recovery context with fallback set
 * @param params Operation parameters
 * @param result Output result buffer
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_execute_cpu_fallback(
    nimcp_gpu_recovery_context_t* ctx,
    void* params,
    void* result);

/* ============================================================================
 * Memory Management for Recovery
 * ============================================================================ */

/**
 * @brief Free GPU caches to reclaim memory
 *
 * @return Bytes freed
 */
NIMCP_EXPORT size_t nimcp_gpu_free_caches(void);

/**
 * @brief Get current GPU memory usage
 *
 * @param free_bytes Output: free memory
 * @param total_bytes Output: total memory
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_get_memory_info(size_t* free_bytes, size_t* total_bytes);

/**
 * @brief Check if GPU memory is critically low
 *
 * @param threshold Threshold ratio (0.0-1.0, e.g., 0.9 = 90% used)
 * @return true if memory usage exceeds threshold
 */
NIMCP_EXPORT bool nimcp_gpu_memory_critical(float threshold);

/**
 * @brief Attempt to free memory for allocation
 *
 * Tries progressive strategies to free enough memory:
 * 1. Clear caches
 * 2. Force synchronization
 * 3. Reset device pools
 *
 * @param required_bytes Bytes needed
 * @return true if enough memory is now available
 */
NIMCP_EXPORT bool nimcp_gpu_ensure_memory_available(size_t required_bytes);

/* ============================================================================
 * Statistics and Diagnostics
 * ============================================================================ */

/**
 * @brief Get global recovery statistics
 */
typedef struct nimcp_gpu_recovery_stats {
    uint64_t total_errors;
    uint64_t recoveries_attempted;
    uint64_t recoveries_succeeded;
    uint64_t cpu_fallbacks_used;
    uint64_t batch_reductions_applied;
    uint64_t param_corrections_applied;
    uint64_t memory_freed_bytes;
    float success_rate;
    float avg_retries_per_recovery;
} nimcp_gpu_recovery_stats_t;

NIMCP_EXPORT void nimcp_gpu_recovery_get_stats(nimcp_gpu_recovery_stats_t* stats);
NIMCP_EXPORT void nimcp_gpu_recovery_reset_stats(void);

/**
 * @brief Get human-readable name for recovery action
 */
NIMCP_EXPORT const char* nimcp_gpu_recovery_action_name(nimcp_gpu_recovery_action_t action);

/**
 * @brief Get human-readable name for error category
 */
NIMCP_EXPORT const char* nimcp_gpu_error_category_name(nimcp_gpu_error_category_t category);

/**
 * @brief Categorize CUDA error
 */
NIMCP_EXPORT nimcp_gpu_error_category_t nimcp_gpu_categorize_cuda_error(cudaError_t err);

/**
 * @brief Report error to recovery system for tracking
 *
 * Logs the error and updates statistics without attempting recovery.
 * Use this when you want to track errors but handle them manually.
 *
 * @param error_category Category of the error
 * @param cuda_error CUDA error code
 * @param file Source file where error occurred
 * @param line Line number where error occurred
 */
NIMCP_EXPORT void nimcp_gpu_recovery_report_error(
    nimcp_gpu_error_category_t error_category,
    cudaError_t cuda_error,
    const char* file,
    int line);

/* ============================================================================
 * Recovery Macros for GPU Operations
 * ============================================================================
 *
 * These macros wrap GPU operations with automatic recovery support.
 * They attempt recovery on failure before returning error.
 *
 * Usage:
 *   NIMCP_GPU_RECOVER_OR_FAIL(cudaMalloc(&ptr, size), GPU_ERROR_OUT_OF_MEMORY);
 *   NIMCP_GPU_RECOVER_OR_NULL(result = compute_something(), GPU_ERROR_CUDA_RUNTIME);
 */

/**
 * @brief Execute GPU operation with recovery, return false on unrecoverable failure
 *
 * @param call CUDA call or operation
 * @param error_cat Error category for recovery selection
 */
#define NIMCP_GPU_RECOVER_OR_FAIL(call, error_cat) do { \
    cudaError_t _err = (call); \
    if (_err != cudaSuccess) { \
        nimcp_gpu_recovery_result_t _result; \
        if (!nimcp_gpu_try_recover(NULL, (error_cat), _err, &_result)) { \
            NIMCP_THROW_GPU(NIMCP_ERROR_GPU, 0, _err, \
                "GPU operation failed (unrecoverable): %s", cudaGetErrorString(_err)); \
            return false; \
        } \
        /* Recovery succeeded - retry the operation */ \
        _err = (call); \
        if (_err != cudaSuccess) { \
            NIMCP_THROW_GPU(NIMCP_ERROR_GPU, 0, _err, \
                "GPU operation failed after recovery: %s", cudaGetErrorString(_err)); \
            return false; \
        } \
    } \
} while(0)

/**
 * @brief Execute GPU operation with recovery, return NULL on unrecoverable failure
 */
#define NIMCP_GPU_RECOVER_OR_NULL(call, error_cat) do { \
    cudaError_t _err = (call); \
    if (_err != cudaSuccess) { \
        nimcp_gpu_recovery_result_t _result; \
        if (!nimcp_gpu_try_recover(NULL, (error_cat), _err, &_result)) { \
            NIMCP_THROW_GPU(NIMCP_ERROR_GPU, 0, _err, \
                "GPU operation failed (unrecoverable): %s", cudaGetErrorString(_err)); \
            return NULL; \
        } \
        _err = (call); \
        if (_err != cudaSuccess) { \
            NIMCP_THROW_GPU(NIMCP_ERROR_GPU, 0, _err, \
                "GPU operation failed after recovery: %s", cudaGetErrorString(_err)); \
            return NULL; \
        } \
    } \
} while(0)

/**
 * @brief Validate and correct parameters, logging any corrections
 */
#define NIMCP_GPU_CORRECT_PARAM(value, min, max, def, name) \
    nimcp_gpu_correct_param_int((int*)&(value), (min), (max), (def), (name))

#define NIMCP_GPU_CORRECT_PARAM_F(value, min, max, def, name) do { \
    nimcp_gpu_param_range_t _range = {(min), (max), (def), true}; \
    nimcp_gpu_correct_param_float(&(value), &_range, (name)); \
} while(0)

/**
 * @brief Check parameter and throw recoverable error if invalid
 */
#define NIMCP_GPU_CHECK_PARAM(cond, param_name) do { \
    if (!(cond)) { \
        nimcp_gpu_recovery_result_t _result; \
        if (!nimcp_gpu_try_recover(NULL, GPU_ERROR_INVALID_PARAMS, cudaSuccess, &_result)) { \
            NIMCP_THROW_GPU(NIMCP_ERROR_INVALID_PARAM, 0, 0, \
                "Invalid parameter: %s", param_name); \
            return NULL; \
        } \
    } \
} while(0)

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_GPU_RECOVERY_H */
