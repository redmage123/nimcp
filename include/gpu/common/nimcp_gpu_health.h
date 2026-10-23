/**
 * @file nimcp_gpu_health.h
 * @brief GPU health monitoring and recovery system
 *
 * Monitors GPU health metrics and detects degradation before catastrophic
 * failure. Provides early warning and recovery mechanisms.
 */

#ifndef NIMCP_GPU_HEALTH_H
#define NIMCP_GPU_HEALTH_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * GPU health status levels
 */
typedef enum {
    GPU_HEALTH_EXCELLENT = 0,   /* All metrics nominal */
    GPU_HEALTH_GOOD = 1,        /* Minor degradation, no action needed */
    GPU_HEALTH_DEGRADED = 2,    /* Significant degradation, warning */
    GPU_HEALTH_CRITICAL = 3,    /* Severe degradation, recovery recommended */
    GPU_HEALTH_FAILED = 4       /* GPU unusable, fallback to CPU */
} gpu_health_status_t;

/**
 * GPU health metrics
 */
typedef struct {
    /* Memory health */
    size_t total_memory;
    size_t free_memory;
    size_t used_memory;
    float memory_fragmentation;   /* 0.0-1.0 */

    /* Operation health */
    uint32_t consecutive_errors;
    uint32_t total_errors;
    uint32_t successful_ops;
    float error_rate;             /* Errors per 1000 ops */

    /* Performance health */
    double avg_memcpy_time_ms;
    double avg_kernel_time_ms;
    uint32_t slow_operations;     /* Ops taking >2x expected time */

    /* Context health */
    bool context_valid;
    uint32_t context_switches;
    uint32_t failed_context_switches;

    /* Timing */
    time_t last_check;
    time_t last_error;
    time_t last_recovery_attempt;

    /* Overall status */
    gpu_health_status_t status;
    char status_message[256];
} gpu_health_metrics_t;

/**
 * GPU health monitor handle
 */
typedef struct gpu_health_monitor gpu_health_monitor_t;

/**
 * Health check callback - called when health degrades
 *
 * @param device_id GPU device ID
 * @param metrics Current health metrics
 * @param user_data User-provided callback data
 * @return true to attempt recovery, false to continue monitoring
 */
typedef bool (*gpu_health_callback_t)(int device_id,
                                       const gpu_health_metrics_t* metrics,
                                       void* user_data);

/**
 * Initialize GPU health monitoring system
 *
 * @param check_interval_ms How often to check GPU health (milliseconds)
 * @return Health monitor handle, or NULL on failure
 */
gpu_health_monitor_t* nimcp_gpu_health_init(uint32_t check_interval_ms);

/**
 * Destroy GPU health monitor
 *
 * @param monitor Health monitor handle
 */
void nimcp_gpu_health_destroy(gpu_health_monitor_t* monitor);

/**
 * Register health degradation callback
 *
 * @param monitor Health monitor handle
 * @param callback Function to call when health degrades
 * @param user_data User data to pass to callback
 * @param min_status Minimum status level to trigger callback
 */
void nimcp_gpu_health_set_callback(gpu_health_monitor_t* monitor,
                                    gpu_health_callback_t callback,
                                    void* user_data,
                                    gpu_health_status_t min_status);

/**
 * Perform immediate health check on a GPU
 *
 * @param monitor Health monitor handle
 * @param device_id GPU device ID to check
 * @param metrics Output buffer for health metrics
 * @return true if check succeeded, false on failure
 */
bool nimcp_gpu_health_check(gpu_health_monitor_t* monitor,
                            int device_id,
                            gpu_health_metrics_t* metrics);

/**
 * Record a successful GPU operation
 *
 * @param monitor Health monitor handle
 * @param device_id GPU device ID
 * @param operation_time_ms Time the operation took
 */
void nimcp_gpu_health_record_success(gpu_health_monitor_t* monitor,
                                     int device_id,
                                     double operation_time_ms);

/**
 * Record a failed GPU operation
 *
 * @param monitor Health monitor handle
 * @param device_id GPU device ID
 * @param error_code CUDA error code
 */
void nimcp_gpu_health_record_error(gpu_health_monitor_t* monitor,
                                   int device_id,
                                   int error_code);

/**
 * Attempt GPU recovery
 *
 * @param monitor Health monitor handle
 * @param device_id GPU device ID
 * @return true if recovery succeeded, false otherwise
 */
bool nimcp_gpu_health_recover(gpu_health_monitor_t* monitor, int device_id);

/**
 * Get current health status for a GPU
 *
 * @param monitor Health monitor handle
 * @param device_id GPU device ID
 * @return Current health status
 */
gpu_health_status_t nimcp_gpu_health_get_status(gpu_health_monitor_t* monitor,
                                                int device_id);

/**
 * Start background health monitoring thread
 *
 * @param monitor Health monitor handle
 * @return true if thread started successfully
 */
bool nimcp_gpu_health_start_monitoring(gpu_health_monitor_t* monitor);

/**
 * Stop background health monitoring thread
 *
 * @param monitor Health monitor handle
 */
void nimcp_gpu_health_stop_monitoring(gpu_health_monitor_t* monitor);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_GPU_HEALTH_H */
