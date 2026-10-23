/**
 * @file nimcp_gpu_health.cu
 * @brief GPU health monitoring implementation
 */

#include "gpu/common/nimcp_gpu_health.h"
#include "gpu/common/nimcp_cuda_utils.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "platform/threading/nimcp_thread.h"

#include <cuda_runtime.h>
#include <string.h>
#include <stdio.h>

#define LOG_MODULE "GPU_HEALTH"
#define MAX_GPUS 8

struct gpu_health_monitor {
    gpu_health_metrics_t metrics[MAX_GPUS];
    gpu_health_callback_t callback;
    void* callback_user_data;
    gpu_health_status_t callback_min_status;

    uint32_t check_interval_ms;
    bool monitoring_active;
    nimcp_thread_t* monitor_thread;
    nimcp_mutex_t* metrics_mutex;
};

/**
 * Perform a quick GPU health probe
 */
static bool probe_gpu_health(int device_id, gpu_health_metrics_t* metrics)
{
    cudaError_t err = cudaSetDevice(device_id);
    if (err != cudaSuccess) {
        metrics->context_valid = false;
        metrics->status = GPU_HEALTH_FAILED;
        snprintf(metrics->status_message, sizeof(metrics->status_message),
                "Failed to set device %d: %s", device_id, cudaGetErrorString(err));
        return false;
    }

    /* Check memory status */
    size_t free_mem = 0, total_mem = 0;
    err = cudaMemGetInfo(&free_mem, &total_mem);
    if (err != cudaSuccess) {
        metrics->context_valid = false;
        metrics->failed_context_switches++;
        metrics->status = GPU_HEALTH_CRITICAL;
        snprintf(metrics->status_message, sizeof(metrics->status_message),
                "Failed to query memory: %s", cudaGetErrorString(err));
        return false;
    }

    metrics->total_memory = total_mem;
    metrics->free_memory = free_mem;
    metrics->used_memory = total_mem - free_mem;
    metrics->context_valid = true;

    /* Test a small allocation to verify GPU is responsive */
    void* test_ptr = NULL;
    err = cudaMalloc(&test_ptr, 1024);
    if (err != cudaSuccess) {
        metrics->consecutive_errors++;
        metrics->total_errors++;
        metrics->status = GPU_HEALTH_CRITICAL;
        snprintf(metrics->status_message, sizeof(metrics->status_message),
                "Test allocation failed: %s", cudaGetErrorString(err));
        return false;
    }
    cudaFree(test_ptr);

    /* Reset consecutive error counter on success */
    metrics->consecutive_errors = 0;
    metrics->successful_ops++;

    /* Calculate error rate */
    uint32_t total_ops = metrics->successful_ops + metrics->total_errors;
    if (total_ops > 0) {
        metrics->error_rate = (float)metrics->total_errors * 1000.0f / (float)total_ops;
    }

    /* Determine health status based on metrics */
    if (metrics->consecutive_errors >= 10) {
        metrics->status = GPU_HEALTH_FAILED;
        snprintf(metrics->status_message, sizeof(metrics->status_message),
                "10+ consecutive errors");
    } else if (metrics->consecutive_errors >= 5) {
        metrics->status = GPU_HEALTH_CRITICAL;
        snprintf(metrics->status_message, sizeof(metrics->status_message),
                "5+ consecutive errors");
    } else if (metrics->error_rate > 50.0f) {
        metrics->status = GPU_HEALTH_CRITICAL;
        snprintf(metrics->status_message, sizeof(metrics->status_message),
                "High error rate: %.1f/1000", metrics->error_rate);
    } else if (free_mem < total_mem / 20) {  /* <5% free */
        metrics->status = GPU_HEALTH_DEGRADED;
        snprintf(metrics->status_message, sizeof(metrics->status_message),
                "Low memory: %zu MB free", free_mem / (1024*1024));
    } else if (metrics->error_rate > 10.0f) {
        metrics->status = GPU_HEALTH_DEGRADED;
        snprintf(metrics->status_message, sizeof(metrics->status_message),
                "Elevated error rate: %.1f/1000", metrics->error_rate);
    } else if (metrics->slow_operations > 10) {
        metrics->status = GPU_HEALTH_GOOD;
        snprintf(metrics->status_message, sizeof(metrics->status_message),
                "%u slow operations detected", metrics->slow_operations);
    } else {
        metrics->status = GPU_HEALTH_EXCELLENT;
        snprintf(metrics->status_message, sizeof(metrics->status_message),
                "All systems nominal");
    }

    metrics->last_check = time(NULL);
    return true;
}

/**
 * Background monitoring thread
 */
static void* monitoring_thread_func(void* arg)
{
    gpu_health_monitor_t* monitor = (gpu_health_monitor_t*)arg;

    while (monitor->monitoring_active) {
        /* Check all GPUs */
        int device_count = 0;
        cudaGetDeviceCount(&device_count);

        for (int i = 0; i < device_count && i < MAX_GPUS; i++) {
            nimcp_mutex_lock(monitor->metrics_mutex);
            gpu_health_metrics_t old_status = monitor->metrics[i];
            bool check_ok = probe_gpu_health(i, &monitor->metrics[i]);
            nimcp_mutex_unlock(monitor->metrics_mutex);

            /* Trigger callback if health degraded */
            if (check_ok && monitor->callback) {
                gpu_health_status_t new_status = monitor->metrics[i].status;
                gpu_health_status_t old_status_level = old_status.status;

                if (new_status >= monitor->callback_min_status &&
                    new_status > old_status_level) {
                    bool should_recover = monitor->callback(i, &monitor->metrics[i],
                                                           monitor->callback_user_data);
                    if (should_recover) {
                        NIMCP_LOG_WARN("Health callback requested recovery for GPU %d", i);
                        nimcp_gpu_health_recover(monitor, i);
                    }
                }
            }
        }

        /* Sleep until next check */
        nimcp_thread_sleep(monitor->check_interval_ms);
    }

    return NULL;
}

gpu_health_monitor_t* nimcp_gpu_health_init(uint32_t check_interval_ms)
{
    gpu_health_monitor_t* monitor = (gpu_health_monitor_t*)
        nimcp_calloc(1, sizeof(gpu_health_monitor_t));
    if (!monitor) return NULL;

    monitor->check_interval_ms = check_interval_ms;
    monitor->callback_min_status = GPU_HEALTH_DEGRADED;
    monitor->metrics_mutex = nimcp_mutex_create(NULL);

    if (!monitor->metrics_mutex) {
        nimcp_free(monitor);
        return NULL;
    }

    /* Initialize metrics for all GPUs */
    int device_count = 0;
    cudaGetDeviceCount(&device_count);
    for (int i = 0; i < device_count && i < MAX_GPUS; i++) {
        monitor->metrics[i].status = GPU_HEALTH_EXCELLENT;
        monitor->metrics[i].context_valid = true;
    }

    NIMCP_LOG_INFO("GPU health monitor initialized (check interval: %u ms)",
                   check_interval_ms);
    return monitor;
}

void nimcp_gpu_health_destroy(gpu_health_monitor_t* monitor)
{
    if (!monitor) return;

    if (monitor->monitoring_active) {
        nimcp_gpu_health_stop_monitoring(monitor);
    }

    if (monitor->metrics_mutex) {
        nimcp_mutex_free(monitor->metrics_mutex);
    }

    nimcp_free(monitor);
}

void nimcp_gpu_health_set_callback(gpu_health_monitor_t* monitor,
                                   gpu_health_callback_t callback,
                                   void* user_data,
                                   gpu_health_status_t min_status)
{
    if (!monitor) return;
    monitor->callback = callback;
    monitor->callback_user_data = user_data;
    monitor->callback_min_status = min_status;
}

bool nimcp_gpu_health_check(gpu_health_monitor_t* monitor,
                           int device_id,
                           gpu_health_metrics_t* metrics)
{
    if (!monitor || device_id < 0 || device_id >= MAX_GPUS) return false;

    nimcp_mutex_lock(monitor->metrics_mutex);
    bool result = probe_gpu_health(device_id, &monitor->metrics[device_id]);
    if (metrics) {
        *metrics = monitor->metrics[device_id];
    }
    nimcp_mutex_unlock(monitor->metrics_mutex);

    return result;
}

void nimcp_gpu_health_record_success(gpu_health_monitor_t* monitor,
                                    int device_id,
                                    double operation_time_ms)
{
    if (!monitor || device_id < 0 || device_id >= MAX_GPUS) return;

    nimcp_mutex_lock(monitor->metrics_mutex);
    monitor->metrics[device_id].consecutive_errors = 0;
    monitor->metrics[device_id].successful_ops++;

    /* Track slow operations (>100ms for memcpy, >200ms for kernels) */
    if (operation_time_ms > 100.0) {
        monitor->metrics[device_id].slow_operations++;
    }
    nimcp_mutex_unlock(monitor->metrics_mutex);
}

void nimcp_gpu_health_record_error(gpu_health_monitor_t* monitor,
                                  int device_id,
                                  int error_code)
{
    if (!monitor || device_id < 0 || device_id >= MAX_GPUS) return;

    nimcp_mutex_lock(monitor->metrics_mutex);
    monitor->metrics[device_id].consecutive_errors++;
    monitor->metrics[device_id].total_errors++;
    monitor->metrics[device_id].last_error = time(NULL);

    /* Immediate status downgrade on errors */
    if (monitor->metrics[device_id].consecutive_errors >= 3) {
        monitor->metrics[device_id].status = GPU_HEALTH_CRITICAL;
    }
    nimcp_mutex_unlock(monitor->metrics_mutex);

    NIMCP_LOG_WARN("GPU %d error recorded: code=%d, consecutive=%u",
                   device_id, error_code,
                   monitor->metrics[device_id].consecutive_errors);
}

bool nimcp_gpu_health_recover(gpu_health_monitor_t* monitor, int device_id)
{
    if (!monitor || device_id < 0 || device_id >= MAX_GPUS) return false;

    NIMCP_LOG_WARN("Attempting GPU %d recovery", device_id);

    monitor->metrics[device_id].last_recovery_attempt = time(NULL);

    /* Try device reset */
    cudaError_t err = cudaSetDevice(device_id);
    if (err != cudaSuccess) {
        NIMCP_LOG_ERROR("Recovery failed: cannot set device %d", device_id);
        return false;
    }

    err = cudaDeviceReset();
    if (err != cudaSuccess) {
        NIMCP_LOG_ERROR("Recovery failed: cudaDeviceReset failed: %s",
                       cudaGetErrorString(err));
        return false;
    }

    /* Re-initialize device */
    cudaFree(0);  /* Force context creation */

    /* Verify recovery */
    gpu_health_metrics_t test_metrics;
    bool recovered = probe_gpu_health(device_id, &test_metrics);

    if (recovered && test_metrics.status < GPU_HEALTH_CRITICAL) {
        nimcp_mutex_lock(monitor->metrics_mutex);
        monitor->metrics[device_id] = test_metrics;
        monitor->metrics[device_id].consecutive_errors = 0;
        nimcp_mutex_unlock(monitor->metrics_mutex);

        NIMCP_LOG_INFO("GPU %d recovery successful", device_id);
        return true;
    }

    NIMCP_LOG_ERROR("GPU %d recovery failed", device_id);
    return false;
}

gpu_health_status_t nimcp_gpu_health_get_status(gpu_health_monitor_t* monitor,
                                                int device_id)
{
    if (!monitor || device_id < 0 || device_id >= MAX_GPUS) {
        return GPU_HEALTH_FAILED;
    }

    nimcp_mutex_lock(monitor->metrics_mutex);
    gpu_health_status_t status = monitor->metrics[device_id].status;
    nimcp_mutex_unlock(monitor->metrics_mutex);

    return status;
}

bool nimcp_gpu_health_start_monitoring(gpu_health_monitor_t* monitor)
{
    if (!monitor || monitor->monitoring_active) return false;

    monitor->monitoring_active = true;
    monitor->monitor_thread = nimcp_thread_create(monitoring_thread_func,
                                                   monitor,
                                                   "gpu_health");

    if (!monitor->monitor_thread) {
        monitor->monitoring_active = false;
        return false;
    }

    NIMCP_LOG_INFO("GPU health monitoring thread started");
    return true;
}

void nimcp_gpu_health_stop_monitoring(gpu_health_monitor_t* monitor)
{
    if (!monitor || !monitor->monitoring_active) return;

    monitor->monitoring_active = false;

    if (monitor->monitor_thread) {
        nimcp_thread_join(monitor->monitor_thread);
        monitor->monitor_thread = NULL;
    }

    NIMCP_LOG_INFO("GPU health monitoring thread stopped");
}
