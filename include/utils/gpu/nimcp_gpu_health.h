/**
 * @file nimcp_gpu_health.h
 * @brief GPU Health Monitoring and Resilience System
 *
 * Comprehensive GPU integration for health monitoring and GPU-accelerated
 * resilience operations. Provides:
 * 1. GPU Health Monitoring - Monitor GPU state, detect errors, coordinate recovery
 * 2. GPU-Accelerated Health Operations - Fast anomaly detection, tensor validation
 *
 * Part of Phase 7 (Section 26) of the NIMCP Self-Contained Resilience System.
 *
 * @copyright Copyright (c) 2025 NIMCP Project
 */

#ifndef NIMCP_GPU_HEALTH_H
#define NIMCP_GPU_HEALTH_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*==============================================================================
 * GPU HEALTH STATUS AND METRICS
 *============================================================================*/

/**
 * @brief GPU health status levels
 */
typedef enum {
    GPU_HEALTH_OPTIMAL = 0,    /**< All metrics within normal range */
    GPU_HEALTH_GOOD,           /**< Minor deviations, no action needed */
    GPU_HEALTH_WARNING,        /**< Approaching thresholds, monitoring */
    GPU_HEALTH_DEGRADED,       /**< Performance impacted, consider action */
    GPU_HEALTH_CRITICAL,       /**< Immediate action required */
    GPU_HEALTH_FAILED          /**< GPU unusable, failover initiated */
} gpu_health_status_t;

/**
 * @brief GPU clock throttle reasons (bitmask)
 */
typedef enum {
    GPU_THROTTLE_NONE            = 0x00,
    GPU_THROTTLE_THERMAL         = 0x01,  /**< Temperature limit */
    GPU_THROTTLE_POWER           = 0x02,  /**< Power limit */
    GPU_THROTTLE_SYNC_BOOST      = 0x04,  /**< Multi-GPU sync */
    GPU_THROTTLE_SW_THERMAL      = 0x08,  /**< Software thermal limit */
    GPU_THROTTLE_HW_SLOWDOWN     = 0x10,  /**< Hardware slowdown */
    GPU_THROTTLE_HW_THERMAL      = 0x20,  /**< HW thermal slowdown */
    GPU_THROTTLE_HW_POWER        = 0x40   /**< HW power brake */
} gpu_throttle_reason_t;

/**
 * @brief GPU health metrics structure
 */
typedef struct {
    /* Hardware identification */
    int device_id;
    char device_name[256];
    int compute_capability_major;
    int compute_capability_minor;

    /* Temperature and power */
    float temperature_celsius;
    float temperature_threshold;
    float power_watts;
    float power_limit;
    uint32_t clock_throttle_reasons;      /**< Bitmask of gpu_throttle_reason_t */

    /* Memory state */
    size_t memory_total;
    size_t memory_used;
    size_t memory_free;
    float memory_utilization;             /**< 0.0 to 1.0 */

    /* Compute state */
    float gpu_utilization;                /**< 0.0 to 1.0 */
    float memory_bandwidth_utilization;   /**< 0.0 to 1.0 */
    uint32_t active_kernels;
    uint32_t pending_kernels;

    /* Error counters */
    uint64_t ecc_errors_correctable;
    uint64_t ecc_errors_uncorrectable;
    uint64_t cuda_errors_total;
    uint64_t kernel_failures;
    uint64_t memory_allocation_failures;
    uint64_t timeout_errors;

    /* Performance metrics */
    float avg_kernel_time_ms;
    float peak_kernel_time_ms;
    uint64_t total_kernels_executed;
    float throughput_gflops;

    /* Health score */
    float health_score;                   /**< 0.0 = critical, 1.0 = healthy */
    gpu_health_status_t status;

    /* Timestamp */
    uint64_t timestamp_us;

} gpu_health_metrics_t;

/**
 * @brief GPU health monitor configuration
 */
typedef struct {
    /* Monitoring intervals (milliseconds) */
    uint32_t poll_interval_ms;            /**< How often to poll GPU state */
    uint32_t deep_check_interval_ms;      /**< Full diagnostic check interval */

    /* Temperature thresholds */
    float temp_warning_celsius;           /**< Temperature warning threshold */
    float temp_critical_celsius;          /**< Temperature critical threshold */
    float temp_shutdown_celsius;          /**< Temperature shutdown threshold */

    /* Memory thresholds (0.0 to 1.0) */
    float memory_warning_pct;             /**< Memory usage warning */
    float memory_critical_pct;            /**< Memory usage critical */

    /* Error thresholds */
    uint64_t ecc_warning_count;           /**< ECC errors warning threshold */
    uint32_t max_cuda_errors_per_min;     /**< Max CUDA errors before action */
    uint32_t kernel_timeout_ms;           /**< Kernel timeout threshold */

    /* Performance thresholds */
    float utilization_low_threshold;      /**< Underutilization warning */
    float utilization_high_threshold;     /**< Overutilization warning */

    /* Recovery options */
    bool enable_auto_recovery;            /**< Enable automatic recovery */
    bool enable_thermal_throttling;       /**< Auto-throttle on thermal issues */
    bool enable_memory_defrag;            /**< Enable memory defragmentation */
    bool enable_cpu_fallback;             /**< Fall back to CPU on failure */
    bool enable_tdr_detection;            /**< Detect Windows TDR events */

    /* Multi-GPU options */
    bool enable_multi_gpu_balancing;      /**< Load balance across GPUs */
    int preferred_device_id;              /**< -1 for auto-select */
    int max_devices;                      /**< Max GPUs to use, 0 = all */

} gpu_health_config_t;

/*==============================================================================
 * GPU ERROR TYPES AND DETECTION
 *============================================================================*/

/**
 * @brief GPU error types
 */
typedef enum {
    GPU_ERROR_NONE = 0,

    /* CUDA runtime errors */
    GPU_ERROR_CUDA_INIT_FAILED,
    GPU_ERROR_CUDA_OOM,
    GPU_ERROR_CUDA_INVALID_DEVICE,
    GPU_ERROR_CUDA_KERNEL_LAUNCH,
    GPU_ERROR_CUDA_SYNC_FAILED,
    GPU_ERROR_CUDA_ILLEGAL_ADDRESS,
    GPU_ERROR_CUDA_ASSERT,

    /* Hardware errors */
    GPU_ERROR_ECC_CORRECTABLE,
    GPU_ERROR_ECC_UNCORRECTABLE,
    GPU_ERROR_THERMAL_THROTTLE,
    GPU_ERROR_POWER_THROTTLE,
    GPU_ERROR_HARDWARE_FAULT,

    /* Timeout errors */
    GPU_ERROR_KERNEL_TIMEOUT,
    GPU_ERROR_TDR_RESET,

    /* Memory errors */
    GPU_ERROR_MEMORY_CORRUPTION,
    GPU_ERROR_NAN_INF_DETECTED,

    /* Communication errors */
    GPU_ERROR_PCIE_ERROR,
    GPU_ERROR_NVLINK_ERROR,
    GPU_ERROR_PEER_ACCESS_FAILED,

    GPU_ERROR_COUNT
} gpu_error_type_t;

/**
 * @brief GPU error severity (maps to health agent severity)
 */
typedef enum {
    GPU_ERROR_SEV_INFO = 0,       /**< Informational, no action needed */
    GPU_ERROR_SEV_WARNING,        /**< Warning, monitor closely */
    GPU_ERROR_SEV_ERROR,          /**< Error, may need intervention */
    GPU_ERROR_SEV_CRITICAL,       /**< Critical, immediate action required */
    GPU_ERROR_SEV_FATAL           /**< Fatal, GPU unusable */
} gpu_error_severity_t;

/**
 * @brief GPU error event
 */
typedef struct {
    gpu_error_type_t type;
    int device_id;
    uint64_t timestamp_us;

    /* Error context */
    char kernel_name[128];        /**< Kernel that caused error (if known) */
    void* fault_address;          /**< Memory address (if applicable) */
    int cuda_error_code;          /**< Original CUDA error code */
    int driver_error_code;        /**< Driver error code (if available) */

    /* Severity */
    gpu_error_severity_t severity;
    bool is_recoverable;

    /* Diagnostic info */
    char description[256];
    char stack_trace[1024];       /**< If available */

    /* Correlation */
    uint64_t correlation_id;      /**< For tracking related errors */
    uint32_t occurrence_count;    /**< How many times this error occurred */

} gpu_error_event_t;

/**
 * @brief GPU error callback signature
 */
typedef void (*gpu_error_callback_t)(
    const gpu_error_event_t* error,
    void* user_data
);

/*==============================================================================
 * GPU-IMMUNE BRIDGE TYPES
 *============================================================================*/

/**
 * @brief GPU antigen source types (maps to immune system)
 */
typedef enum {
    GPU_ANTIGEN_THERMAL = 0,      /**< Thermal-related issues */
    GPU_ANTIGEN_MEMORY,           /**< GPU memory issues */
    GPU_ANTIGEN_COMPUTE,          /**< Compute/kernel issues */
    GPU_ANTIGEN_HARDWARE,         /**< Hardware failures */
    GPU_ANTIGEN_COMMUNICATION,    /**< PCIe/NVLink issues */
    GPU_ANTIGEN_CORRUPTION,       /**< Data corruption (NaN/Inf) */
    GPU_ANTIGEN_TIMEOUT,          /**< Timeout issues */
    GPU_ANTIGEN_COUNT
} gpu_antigen_type_t;

/**
 * @brief GPU recovery actions (antibodies)
 */
typedef enum {
    GPU_RECOVERY_NONE = 0,
    GPU_RECOVERY_RETRY,           /**< Retry failed operation */
    GPU_RECOVERY_REDUCE_BATCH,    /**< Reduce batch size */
    GPU_RECOVERY_CLEAR_CACHE,     /**< Clear GPU caches */
    GPU_RECOVERY_DEFRAG_MEMORY,   /**< Defragment GPU memory */
    GPU_RECOVERY_RESET_CONTEXT,   /**< Reset CUDA context */
    GPU_RECOVERY_THROTTLE,        /**< Reduce GPU frequency */
    GPU_RECOVERY_CHECKPOINT,      /**< Save checkpoint */
    GPU_RECOVERY_MIGRATE_GPU,     /**< Move to different GPU */
    GPU_RECOVERY_FALLBACK_CPU,    /**< Fall back to CPU */
    GPU_RECOVERY_QUARANTINE_GPU,  /**< Mark GPU as unusable */
    GPU_RECOVERY_COUNT
} gpu_recovery_action_t;

/**
 * @brief GPU immune response structure
 */
typedef struct {
    gpu_antigen_type_t antigen_type;
    gpu_recovery_action_t suggested_recovery;
    gpu_recovery_action_t fallback_recovery;
    float urgency;                /**< 0.0 to 1.0, higher = more urgent */
    bool requires_checkpoint;     /**< Should checkpoint before recovery */
    bool requires_sync;           /**< Should sync all streams before recovery */
    char reason[256];             /**< Human-readable reason */
} gpu_immune_response_t;

/*==============================================================================
 * GPU MEMORY POOL TYPES
 *============================================================================*/

/**
 * @brief Opaque GPU memory pool handle
 */
typedef struct gpu_memory_pool gpu_memory_pool_t;

/**
 * @brief GPU memory pool configuration
 */
typedef struct {
    int device_id;
    size_t initial_size;          /**< Initial pool size in bytes */
    size_t max_size;              /**< Maximum pool size in bytes */
    bool enable_defragmentation;  /**< Enable auto-defragmentation */
    bool enable_overflow_to_host; /**< Use pinned host memory as overflow */
    float high_water_mark;        /**< Trigger cleanup at this usage (0.0-1.0) */
    float critical_water_mark;    /**< Emergency cleanup threshold (0.0-1.0) */
} gpu_memory_pool_config_t;

/**
 * @brief GPU memory pool statistics
 */
typedef struct {
    size_t total_size;
    size_t used_size;
    size_t free_size;
    size_t largest_free_block;
    uint32_t num_allocations;
    uint32_t num_fragments;
    float fragmentation_ratio;    /**< 0.0 = no fragmentation, 1.0 = severe */
    uint64_t total_allocs;
    uint64_t total_frees;
    uint64_t alloc_failures;
    uint64_t defrag_count;
    uint64_t overflow_to_host_count;
} gpu_memory_pool_stats_t;

/*==============================================================================
 * GPU HEALTH MONITOR HANDLE
 *============================================================================*/

/**
 * @brief Opaque GPU health monitor handle
 */
typedef struct gpu_health_monitor gpu_health_monitor_t;

/*==============================================================================
 * GPU HEALTH MONITOR API
 *============================================================================*/

/**
 * @brief Get default GPU health configuration
 *
 * @param[out] config Configuration to initialize with defaults
 */
void gpu_health_get_default_config(gpu_health_config_t* config);

/**
 * @brief Create GPU health monitor
 *
 * @param[in] config Configuration (NULL for defaults)
 * @return Health monitor handle, or NULL on failure
 */
gpu_health_monitor_t* gpu_health_monitor_create(const gpu_health_config_t* config);

/**
 * @brief Destroy GPU health monitor
 *
 * @param[in] monitor Health monitor to destroy
 */
void gpu_health_monitor_destroy(gpu_health_monitor_t* monitor);

/**
 * @brief Start GPU health monitoring
 *
 * @param[in] monitor Health monitor
 * @return 0 on success, -1 on error
 */
int gpu_health_monitor_start(gpu_health_monitor_t* monitor);

/**
 * @brief Stop GPU health monitoring
 *
 * @param[in] monitor Health monitor
 * @return 0 on success, -1 on error
 */
int gpu_health_monitor_stop(gpu_health_monitor_t* monitor);

/**
 * @brief Check if monitoring is active
 *
 * @param[in] monitor Health monitor
 * @return true if monitoring, false otherwise
 */
bool gpu_health_monitor_is_running(const gpu_health_monitor_t* monitor);

/*==============================================================================
 * GPU HEALTH METRICS API
 *============================================================================*/

/**
 * @brief Get current GPU health metrics for a specific device
 *
 * @param[in] monitor Health monitor
 * @param[in] device_id GPU device ID
 * @param[out] metrics Metrics output
 * @return 0 on success, -1 on error
 */
int gpu_health_get_metrics(
    gpu_health_monitor_t* monitor,
    int device_id,
    gpu_health_metrics_t* metrics
);

/**
 * @brief Get health metrics for all GPUs
 *
 * @param[in] monitor Health monitor
 * @param[out] metrics_array Array to fill with metrics
 * @param[in,out] num_devices In: array size, Out: number of devices
 * @return 0 on success, -1 on error
 */
int gpu_health_get_all_metrics(
    gpu_health_monitor_t* monitor,
    gpu_health_metrics_t* metrics_array,
    int* num_devices
);

/**
 * @brief Check GPU health status (quick check)
 *
 * @param[in] monitor Health monitor
 * @param[in] device_id GPU device ID (-1 for overall)
 * @return Health status
 */
gpu_health_status_t gpu_health_check(
    gpu_health_monitor_t* monitor,
    int device_id
);

/**
 * @brief Get aggregated health score
 *
 * @param[in] monitor Health monitor
 * @param[in] device_id GPU device ID (-1 for overall)
 * @return Health score 0.0 to 1.0
 */
float gpu_health_get_score(
    gpu_health_monitor_t* monitor,
    int device_id
);

/**
 * @brief Predict GPU failure probability
 *
 * @param[in] monitor Health monitor
 * @param[in] device_id GPU device ID
 * @param[in] time_horizon_minutes Prediction horizon
 * @return Failure probability 0.0 to 1.0
 */
float gpu_health_predict_failure_probability(
    gpu_health_monitor_t* monitor,
    int device_id,
    uint32_t time_horizon_minutes
);

/**
 * @brief Get number of available GPUs
 *
 * @param[in] monitor Health monitor
 * @return Number of GPUs, or -1 on error
 */
int gpu_health_get_device_count(gpu_health_monitor_t* monitor);

/*==============================================================================
 * GPU ERROR DETECTION API
 *============================================================================*/

/**
 * @brief Register GPU error callback
 *
 * @param[in] monitor Health monitor
 * @param[in] callback Error callback function
 * @param[in] user_data User data passed to callback
 * @return Callback ID for unregistration, or -1 on error
 */
int gpu_error_register_callback(
    gpu_health_monitor_t* monitor,
    gpu_error_callback_t callback,
    void* user_data
);

/**
 * @brief Unregister GPU error callback
 *
 * @param[in] monitor Health monitor
 * @param[in] callback_id Callback ID from registration
 * @return 0 on success, -1 on error
 */
int gpu_error_unregister_callback(
    gpu_health_monitor_t* monitor,
    int callback_id
);

/**
 * @brief Check for async CUDA errors
 *
 * @param[in] monitor Health monitor
 * @param[in] device_id GPU device ID
 * @param[out] error Error event if error detected
 * @return 1 if error found, 0 if no error, -1 on failure
 */
int gpu_error_check_async(
    gpu_health_monitor_t* monitor,
    int device_id,
    gpu_error_event_t* error
);

/**
 * @brief Get error type name
 *
 * @param[in] type Error type
 * @return Human-readable error type name
 */
const char* gpu_error_type_name(gpu_error_type_t type);

/**
 * @brief Get error severity name
 *
 * @param[in] severity Error severity
 * @return Human-readable severity name
 */
const char* gpu_error_severity_name(gpu_error_severity_t severity);

/*==============================================================================
 * GPU TENSOR VALIDATION API
 *============================================================================*/

/**
 * @brief Validate GPU tensor for NaN/Inf values
 *
 * @param[in] monitor Health monitor
 * @param[in] device_ptr GPU device pointer to tensor data
 * @param[in] num_elements Number of elements in tensor
 * @param[in] element_size Size of each element in bytes (4 for float, 8 for double)
 * @param[out] nan_count Number of NaN values found
 * @param[out] inf_count Number of Inf values found
 * @return 0 on success, -1 on error
 */
int gpu_tensor_validate(
    gpu_health_monitor_t* monitor,
    const void* device_ptr,
    size_t num_elements,
    size_t element_size,
    uint32_t* nan_count,
    uint32_t* inf_count
);

/**
 * @brief Sanitize GPU tensor (replace NaN/Inf with safe values)
 *
 * @param[in] monitor Health monitor
 * @param[in,out] device_ptr GPU device pointer to tensor data
 * @param[in] num_elements Number of elements
 * @param[in] element_size Size of each element
 * @param[in] nan_replacement Value to replace NaN with
 * @param[in] inf_replacement Value to replace Inf with
 * @return Number of values replaced, or -1 on error
 */
int gpu_tensor_sanitize(
    gpu_health_monitor_t* monitor,
    void* device_ptr,
    size_t num_elements,
    size_t element_size,
    float nan_replacement,
    float inf_replacement
);

/*==============================================================================
 * GPU-IMMUNE BRIDGE API
 *============================================================================*/

/**
 * @brief Map GPU error to antigen type
 *
 * @param[in] error_type GPU error type
 * @return Corresponding antigen type
 */
gpu_antigen_type_t gpu_error_to_antigen(gpu_error_type_t error_type);

/**
 * @brief Get recommended recovery action for error
 *
 * @param[in] monitor Health monitor
 * @param[in] error Error event
 * @param[out] response Immune response with recovery recommendation
 * @return 0 on success, -1 on error
 */
int gpu_immune_get_response(
    gpu_health_monitor_t* monitor,
    const gpu_error_event_t* error,
    gpu_immune_response_t* response
);

/**
 * @brief Execute GPU recovery action
 *
 * @param[in] monitor Health monitor
 * @param[in] device_id GPU device ID
 * @param[in] action Recovery action to execute
 * @return 0 on success, -1 on error
 */
int gpu_immune_execute_recovery(
    gpu_health_monitor_t* monitor,
    int device_id,
    gpu_recovery_action_t action
);

/**
 * @brief Get recovery action name
 *
 * @param[in] action Recovery action
 * @return Human-readable action name
 */
const char* gpu_recovery_action_name(gpu_recovery_action_t action);

/*==============================================================================
 * GPU MEMORY POOL API
 *============================================================================*/

/**
 * @brief Get default memory pool configuration
 *
 * @param[out] config Configuration to initialize
 */
void gpu_memory_pool_get_default_config(gpu_memory_pool_config_t* config);

/**
 * @brief Create GPU memory pool
 *
 * @param[in] monitor Health monitor (for health-aware allocation)
 * @param[in] config Pool configuration
 * @return Memory pool handle, or NULL on failure
 */
gpu_memory_pool_t* gpu_memory_pool_create(
    gpu_health_monitor_t* monitor,
    const gpu_memory_pool_config_t* config
);

/**
 * @brief Destroy GPU memory pool
 *
 * @param[in] pool Memory pool to destroy
 */
void gpu_memory_pool_destroy(gpu_memory_pool_t* pool);

/**
 * @brief Allocate from memory pool
 *
 * @param[in] pool Memory pool
 * @param[in] size Size in bytes
 * @return Device pointer, or NULL on failure
 */
void* gpu_memory_pool_alloc(
    gpu_memory_pool_t* pool,
    size_t size
);

/**
 * @brief Free memory back to pool
 *
 * @param[in] pool Memory pool
 * @param[in] ptr Device pointer to free
 */
void gpu_memory_pool_free(
    gpu_memory_pool_t* pool,
    void* ptr
);

/**
 * @brief Get memory pool statistics
 *
 * @param[in] pool Memory pool
 * @param[out] stats Statistics output
 * @return 0 on success, -1 on error
 */
int gpu_memory_pool_get_stats(
    gpu_memory_pool_t* pool,
    gpu_memory_pool_stats_t* stats
);

/**
 * @brief Trigger memory defragmentation
 *
 * @param[in] pool Memory pool
 * @return Number of blocks moved, or -1 on error
 */
int gpu_memory_pool_defrag(gpu_memory_pool_t* pool);

/**
 * @brief Clear all allocations (emergency cleanup)
 *
 * @param[in] pool Memory pool
 * @return 0 on success, -1 on error
 */
int gpu_memory_pool_clear(gpu_memory_pool_t* pool);

/*==============================================================================
 * GPU CHECKPOINT API
 *============================================================================*/

/**
 * @brief Create GPU state checkpoint
 *
 * Asynchronously copies GPU tensor state to host memory for checkpointing.
 *
 * @param[in] monitor Health monitor
 * @param[in] device_id GPU device ID
 * @param[in] tensors Array of device pointers to checkpoint
 * @param[in] sizes Array of tensor sizes in bytes
 * @param[in] num_tensors Number of tensors
 * @param[out] checkpoint_id Checkpoint identifier
 * @return 0 on success, -1 on error
 */
int gpu_checkpoint_create(
    gpu_health_monitor_t* monitor,
    int device_id,
    void** tensors,
    size_t* sizes,
    size_t num_tensors,
    uint64_t* checkpoint_id
);

/**
 * @brief Restore GPU state from checkpoint
 *
 * @param[in] monitor Health monitor
 * @param[in] checkpoint_id Checkpoint to restore
 * @param[in] tensors Array of device pointers to restore to
 * @return 0 on success, -1 on error
 */
int gpu_checkpoint_restore(
    gpu_health_monitor_t* monitor,
    uint64_t checkpoint_id,
    void** tensors
);

/**
 * @brief Delete checkpoint
 *
 * @param[in] monitor Health monitor
 * @param[in] checkpoint_id Checkpoint to delete
 * @return 0 on success, -1 on error
 */
int gpu_checkpoint_delete(
    gpu_health_monitor_t* monitor,
    uint64_t checkpoint_id
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_GPU_HEALTH_H */
