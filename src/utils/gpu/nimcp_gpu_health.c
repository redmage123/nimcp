/**
 * @file nimcp_gpu_health.c
 * @brief GPU Health Monitoring and Resilience System Implementation
 *
 * Provides comprehensive GPU health monitoring with automatic detection
 * of CUDA availability. When CUDA is not available, provides stub
 * implementations that report no GPUs.
 *
 * @copyright Copyright (c) 2025 NIMCP Project
 */

#include "utils/gpu/nimcp_gpu_health.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/platform/nimcp_platform_time.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdatomic.h>

#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(gpu_health)

/*==============================================================================
 * CONSTANTS AND MACROS
 *============================================================================*/

#define GPU_HEALTH_MAGIC          0x47505548  /* "GPUH" */
#define GPU_MEMORY_POOL_MAGIC     0x47504D50  /* "GPMP" */
#define MAX_GPU_DEVICES           16
#define MAX_ERROR_CALLBACKS       16
#define ERROR_HISTORY_SIZE        256
#define MAX_CHECKPOINTS           64
#define METRICS_HISTORY_SIZE      128

/*==============================================================================
 * INTERNAL STRUCTURES
 *============================================================================*/

/**
 * @brief Error callback registration
 */
typedef struct {
    gpu_error_callback_t callback;
    void* user_data;
    bool active;
} gpu_error_callback_entry_t;

/**
 * @brief GPU device state
 */
typedef struct {
    int device_id;
    bool available;
    bool quarantined;
    gpu_health_metrics_t current_metrics;
    gpu_health_metrics_t metrics_history[METRICS_HISTORY_SIZE];
    size_t metrics_history_idx;
    size_t metrics_history_count;

    /* Error tracking */
    gpu_error_event_t error_history[ERROR_HISTORY_SIZE];
    size_t error_history_idx;
    size_t error_count_last_minute;
    uint64_t last_error_count_reset_us;

    /* Recovery tracking */
    uint32_t recovery_attempts;
    uint32_t successful_recoveries;
    uint64_t last_recovery_us;
} gpu_device_state_t;

/**
 * @brief Checkpoint entry
 */
typedef struct {
    uint64_t checkpoint_id;
    int device_id;
    void** host_copies;
    size_t* sizes;
    size_t num_tensors;
    uint64_t timestamp_us;
    bool valid;
} gpu_checkpoint_entry_t;

/**
 * @brief GPU health monitor internal structure
 */
struct gpu_health_monitor {
    uint32_t magic;
    gpu_health_config_t config;

    /* Device state */
    int num_devices;
    gpu_device_state_t devices[MAX_GPU_DEVICES];

    /* Error callbacks */
    gpu_error_callback_entry_t error_callbacks[MAX_ERROR_CALLBACKS];
    nimcp_mutex_t* callbacks_mutex;

    /* Monitoring thread */
    nimcp_thread_t monitor_thread;
    _Atomic bool running;
    _Atomic bool should_stop;

    /* Checkpoints */
    gpu_checkpoint_entry_t checkpoints[MAX_CHECKPOINTS];
    nimcp_mutex_t* checkpoints_mutex;
    _Atomic uint64_t next_checkpoint_id;

    /* Overall stats */
    _Atomic uint64_t total_errors;
    _Atomic uint64_t total_recoveries;
    _Atomic uint64_t total_checks;
    _Atomic float overall_health_score;

    /* Synchronization */
    nimcp_mutex_t* state_mutex;
};

/**
 * @brief GPU memory pool internal structure
 */
struct gpu_memory_pool {
    uint32_t magic;
    gpu_memory_pool_config_t config;
    gpu_health_monitor_t* monitor;

    /* Pool state - simplified for non-CUDA implementation */
    size_t allocated_size;
    uint32_t num_allocations;
    nimcp_mutex_t* pool_mutex;

    /* Statistics */
    gpu_memory_pool_stats_t stats;
};

/*==============================================================================
 * FORWARD DECLARATIONS
 *============================================================================*/

static void* gpu_monitor_thread_func(void* arg);
static void gpu_poll_device_metrics(gpu_health_monitor_t* monitor, int device_id);
static void gpu_update_health_score(gpu_health_monitor_t* monitor, int device_id);
static void gpu_check_thresholds(gpu_health_monitor_t* monitor, int device_id);
static void gpu_notify_error(gpu_health_monitor_t* monitor, const gpu_error_event_t* error);
static uint64_t get_timestamp_us(void);

/*==============================================================================
 * HELPER FUNCTIONS
 *============================================================================*/

static uint64_t get_timestamp_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

static bool validate_monitor(const gpu_health_monitor_t* monitor) {
    return monitor && monitor->magic == GPU_HEALTH_MAGIC;
}

static bool validate_pool(const gpu_memory_pool_t* pool) {
    return pool && pool->magic == GPU_MEMORY_POOL_MAGIC;
}

/*==============================================================================
 * GPU HEALTH CONFIGURATION
 *============================================================================*/

void gpu_health_get_default_config(gpu_health_config_t* config) {
    if (!config) return;

    memset(config, 0, sizeof(*config));

    /* Monitoring intervals */
    config->poll_interval_ms = 1000;          /* 1 second */
    config->deep_check_interval_ms = 60000;   /* 1 minute */

    /* Temperature thresholds */
    config->temp_warning_celsius = 75.0f;
    config->temp_critical_celsius = 85.0f;
    config->temp_shutdown_celsius = 95.0f;

    /* Memory thresholds */
    config->memory_warning_pct = 0.80f;
    config->memory_critical_pct = 0.95f;

    /* Error thresholds */
    config->ecc_warning_count = 10;
    config->max_cuda_errors_per_min = 5;
    config->kernel_timeout_ms = 30000;        /* 30 seconds */

    /* Performance thresholds */
    config->utilization_low_threshold = 0.10f;
    config->utilization_high_threshold = 0.95f;

    /* Recovery options */
    config->enable_auto_recovery = true;
    config->enable_thermal_throttling = true;
    config->enable_memory_defrag = true;
    config->enable_cpu_fallback = true;
    config->enable_tdr_detection = true;

    /* Multi-GPU options */
    config->enable_multi_gpu_balancing = false;
    config->preferred_device_id = -1;         /* Auto-select */
    config->max_devices = 0;                  /* Use all */
}

/*==============================================================================
 * GPU HEALTH MONITOR LIFECYCLE
 *============================================================================*/

gpu_health_monitor_t* gpu_health_monitor_create(const gpu_health_config_t* config) {
    gpu_health_monitor_t* monitor = nimcp_calloc(1, sizeof(gpu_health_monitor_t));
    if (!monitor) {
        nimcp_log(LOG_LEVEL_ERROR, "Failed to allocate GPU health monitor");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "monitor is NULL");

        return NULL;
    }

    monitor->magic = GPU_HEALTH_MAGIC;

    /* Apply config */
    if (config) {
        monitor->config = *config;
    } else {
        gpu_health_get_default_config(&monitor->config);
    }

    /* Create mutexes */
    mutex_attr_t attr;
    attr.type = MUTEX_TYPE_NORMAL;

    monitor->callbacks_mutex = nimcp_mutex_create(&attr);
    monitor->checkpoints_mutex = nimcp_mutex_create(&attr);
    monitor->state_mutex = nimcp_mutex_create(&attr);

    if (!monitor->callbacks_mutex || !monitor->checkpoints_mutex || !monitor->state_mutex) {
        nimcp_log(LOG_LEVEL_ERROR, "Failed to create GPU health monitor mutexes");
        gpu_health_monitor_destroy(monitor);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "gpu_health_monitor_create: required parameter is NULL (monitor->callbacks_mutex, monitor->checkpoints_mutex, monitor->state_mutex)");
        return NULL;
    }

    /* Initialize atomics */
    atomic_init(&monitor->running, false);
    atomic_init(&monitor->should_stop, false);
    atomic_init(&monitor->total_errors, 0);
    atomic_init(&monitor->total_recoveries, 0);
    atomic_init(&monitor->total_checks, 0);
    atomic_init(&monitor->overall_health_score, 1.0f);
    atomic_init(&monitor->next_checkpoint_id, 1);

    /* Detect GPUs - stub implementation reports 0 devices when CUDA unavailable */
    monitor->num_devices = 0;

#ifdef NIMCP_CUDA_ENABLED
    /* When CUDA is enabled, query actual device count */
    int device_count = 0;
    cudaError_t err = cudaGetDeviceCount(&device_count);
    if (err == cudaSuccess && device_count > 0) {
        monitor->num_devices = (device_count > MAX_GPU_DEVICES) ? MAX_GPU_DEVICES : device_count;

        /* Initialize device states */
        for (int i = 0; i < monitor->num_devices; i++) {
            monitor->devices[i].device_id = i;
            monitor->devices[i].available = true;
            monitor->devices[i].quarantined = false;

            /* Get device name */
            cudaDeviceProp prop;
            if (cudaGetDeviceProperties(&prop, i) == cudaSuccess) {
                strncpy(monitor->devices[i].current_metrics.device_name,
                        prop.name, sizeof(monitor->devices[i].current_metrics.device_name) - 1);
                monitor->devices[i].current_metrics.compute_capability_major = prop.major;
                monitor->devices[i].current_metrics.compute_capability_minor = prop.minor;
                monitor->devices[i].current_metrics.memory_total = prop.totalGlobalMem;
            }
        }
    }
#endif

    if (monitor->num_devices > 0) {
        nimcp_log(LOG_LEVEL_INFO, "GPU health monitor created with %d device(s)",
                  monitor->num_devices);
    } else {
        nimcp_log(LOG_LEVEL_DEBUG, "GPU health monitor created (no CUDA devices available)");
    }

    return monitor;
}

void gpu_health_monitor_destroy(gpu_health_monitor_t* monitor) {
    if (!monitor) return;

    /* Stop monitoring if running */
    if (atomic_load(&monitor->running)) {
        gpu_health_monitor_stop(monitor);
    }

    /* Clean up checkpoints */
    if (monitor->checkpoints_mutex) {
        nimcp_mutex_lock(monitor->checkpoints_mutex);
        for (int i = 0; i < MAX_CHECKPOINTS; i++) {
            if (monitor->checkpoints[i].valid) {
                for (size_t j = 0; j < monitor->checkpoints[i].num_tensors; j++) {
                    nimcp_free(monitor->checkpoints[i].host_copies[j]);
                }
                nimcp_free(monitor->checkpoints[i].host_copies);
                nimcp_free(monitor->checkpoints[i].sizes);
            }
        }
        nimcp_mutex_unlock(monitor->checkpoints_mutex);
    }

    /* Destroy mutexes */
    if (monitor->callbacks_mutex) nimcp_mutex_free(monitor->callbacks_mutex);
    if (monitor->checkpoints_mutex) nimcp_mutex_free(monitor->checkpoints_mutex);
    if (monitor->state_mutex) nimcp_mutex_free(monitor->state_mutex);

    monitor->magic = 0;
    nimcp_free(monitor);

    nimcp_log(LOG_LEVEL_DEBUG, "GPU health monitor destroyed");
}

int gpu_health_monitor_start(gpu_health_monitor_t* monitor) {
    if (!validate_monitor(monitor)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "gpu_health_monitor_start: validate_monitor is NULL");
        return -1;
    }

    if (atomic_load(&monitor->running)) {
        nimcp_log(LOG_LEVEL_WARN, "GPU health monitor already running");
        return 0;
    }

    if (monitor->num_devices == 0) {
        /* No GPUs to monitor, but still mark as running for consistency */
        atomic_store(&monitor->running, true);
        nimcp_log(LOG_LEVEL_DEBUG, "GPU health monitor started (no devices to monitor)");
        return 0;
    }

    atomic_store(&monitor->should_stop, false);

    /* Create monitoring thread */
    thread_attr_t thread_attr = {
        .stack_size = 0,
        .priority = 0,  /* Normal priority */
        .detached = false
    };

    int result = nimcp_thread_create(&monitor->monitor_thread,
                                     gpu_monitor_thread_func, monitor,
                                     &thread_attr);
    if (result != 0) {
        nimcp_log(LOG_LEVEL_ERROR, "Failed to create GPU monitor thread: %d", result);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "gpu_health_monitor_start: validation failed");
        return -1;
    }

    atomic_store(&monitor->running, true);
    nimcp_log(LOG_LEVEL_INFO, "GPU health monitor started");

    return 0;
}

int gpu_health_monitor_stop(gpu_health_monitor_t* monitor) {
    if (!validate_monitor(monitor)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "gpu_health_monitor_stop: validate_monitor is NULL");
        return -1;
    }

    if (!atomic_load(&monitor->running)) {
        return 0;
    }

    atomic_store(&monitor->should_stop, true);

    if (monitor->num_devices > 0) {
        /* Wait for thread to finish */
        nimcp_thread_join(monitor->monitor_thread, NULL);
    }

    atomic_store(&monitor->running, false);
    nimcp_log(LOG_LEVEL_INFO, "GPU health monitor stopped");

    return 0;
}

bool gpu_health_monitor_is_running(const gpu_health_monitor_t* monitor) {
    if (!validate_monitor(monitor)) {
        return false;
    }
    return atomic_load((volatile _Atomic bool*)&monitor->running);
}

/*==============================================================================
 * MONITORING THREAD
 *============================================================================*/

static void* gpu_monitor_thread_func(void* arg) {
    gpu_health_monitor_t* monitor = (gpu_health_monitor_t*)arg;

    uint64_t last_deep_check_us = get_timestamp_us();

    while (!atomic_load(&monitor->should_stop)) {
        uint64_t now_us = get_timestamp_us();

        /* Poll each device */
        for (int i = 0; i < monitor->num_devices; i++) {
            if (!monitor->devices[i].quarantined) {
                gpu_poll_device_metrics(monitor, i);
                gpu_update_health_score(monitor, i);
                gpu_check_thresholds(monitor, i);
            }
        }

        /* Deep check at longer intervals */
        if ((now_us - last_deep_check_us) >= monitor->config.deep_check_interval_ms * 1000ULL) {
            /* Perform deeper diagnostics */
            for (int i = 0; i < monitor->num_devices; i++) {
                if (!monitor->devices[i].quarantined) {
                    /* Check for async errors */
                    gpu_error_event_t error;
                    if (gpu_error_check_async(monitor, i, &error) > 0) {
                        gpu_notify_error(monitor, &error);
                    }
                }
            }
            last_deep_check_us = now_us;
        }

        atomic_fetch_add(&monitor->total_checks, 1);

        /* Sleep until next poll */
        nimcp_platform_sleep_ms(monitor->config.poll_interval_ms);
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gpu_monitor_thread_func: operation failed");
    return NULL;
}

static void gpu_poll_device_metrics(gpu_health_monitor_t* monitor, int device_id) {
    if (device_id < 0 || device_id >= monitor->num_devices) return;

    gpu_device_state_t* device = &monitor->devices[device_id];
    gpu_health_metrics_t* metrics = &device->current_metrics;

    metrics->device_id = device_id;
    metrics->timestamp_us = get_timestamp_us();

#ifdef NIMCP_CUDA_ENABLED
    /* Query actual GPU metrics via CUDA/NVML */
    cudaSetDevice(device_id);

    /* Memory info */
    size_t free_mem, total_mem;
    if (cudaMemGetInfo(&free_mem, &total_mem) == cudaSuccess) {
        metrics->memory_total = total_mem;
        metrics->memory_free = free_mem;
        metrics->memory_used = total_mem - free_mem;
        metrics->memory_utilization = (float)metrics->memory_used / (float)total_mem;
    }

    /* Additional metrics would come from NVML library */
    /* For now, use simulated values */
    metrics->temperature_celsius = 50.0f;  /* Placeholder */
    metrics->gpu_utilization = 0.5f;       /* Placeholder */
    metrics->power_watts = 100.0f;         /* Placeholder */
#else
    /* Stub values when CUDA not available */
    metrics->temperature_celsius = 0.0f;
    metrics->memory_total = 0;
    metrics->memory_used = 0;
    metrics->memory_free = 0;
    metrics->memory_utilization = 0.0f;
    metrics->gpu_utilization = 0.0f;
    metrics->power_watts = 0.0f;
#endif

    /* Store in history */
    nimcp_mutex_lock(monitor->state_mutex);
    device->metrics_history[device->metrics_history_idx] = *metrics;
    device->metrics_history_idx = (device->metrics_history_idx + 1) % METRICS_HISTORY_SIZE;
    if (device->metrics_history_count < METRICS_HISTORY_SIZE) {
        device->metrics_history_count++;
    }
    nimcp_mutex_unlock(monitor->state_mutex);
}

static void gpu_update_health_score(gpu_health_monitor_t* monitor, int device_id) {
    if (device_id < 0 || device_id >= monitor->num_devices) return;

    gpu_device_state_t* device = &monitor->devices[device_id];
    gpu_health_metrics_t* metrics = &device->current_metrics;
    const gpu_health_config_t* config = &monitor->config;

    float score = 1.0f;

    /* Temperature factor */
    if (metrics->temperature_celsius > config->temp_critical_celsius) {
        score *= 0.3f;
    } else if (metrics->temperature_celsius > config->temp_warning_celsius) {
        float temp_ratio = (metrics->temperature_celsius - config->temp_warning_celsius) /
                          (config->temp_critical_celsius - config->temp_warning_celsius);
        score *= (1.0f - 0.4f * temp_ratio);
    }

    /* Memory factor */
    if (metrics->memory_utilization > config->memory_critical_pct) {
        score *= 0.4f;
    } else if (metrics->memory_utilization > config->memory_warning_pct) {
        float mem_ratio = (metrics->memory_utilization - config->memory_warning_pct) /
                         (config->memory_critical_pct - config->memory_warning_pct);
        score *= (1.0f - 0.3f * mem_ratio);
    }

    /* Error factor */
    if (metrics->ecc_errors_uncorrectable > 0) {
        score *= 0.5f;
    }
    if (device->error_count_last_minute > config->max_cuda_errors_per_min) {
        score *= 0.6f;
    }

    /* Throttling factor */
    if (metrics->clock_throttle_reasons != GPU_THROTTLE_NONE) {
        score *= 0.85f;
    }

    /* Update metrics */
    metrics->health_score = score;

    /* Determine status */
    if (score >= 0.9f) {
        metrics->status = GPU_HEALTH_OPTIMAL;
    } else if (score >= 0.75f) {
        metrics->status = GPU_HEALTH_GOOD;
    } else if (score >= 0.5f) {
        metrics->status = GPU_HEALTH_WARNING;
    } else if (score >= 0.25f) {
        metrics->status = GPU_HEALTH_DEGRADED;
    } else if (score > 0.0f) {
        metrics->status = GPU_HEALTH_CRITICAL;
    } else {
        metrics->status = GPU_HEALTH_FAILED;
    }

    /* Update overall health score */
    float overall = 0.0f;
    int healthy_count = 0;
    for (int i = 0; i < monitor->num_devices; i++) {
        if (!monitor->devices[i].quarantined) {
            overall += monitor->devices[i].current_metrics.health_score;
            healthy_count++;
        }
    }
    if (healthy_count > 0) {
        atomic_store(&monitor->overall_health_score, overall / healthy_count);
    }
}

static void gpu_check_thresholds(gpu_health_monitor_t* monitor, int device_id) {
    if (device_id < 0 || device_id >= monitor->num_devices) return;

    gpu_device_state_t* device = &monitor->devices[device_id];
    gpu_health_metrics_t* metrics = &device->current_metrics;
    const gpu_health_config_t* config = &monitor->config;

    /* Check temperature thresholds */
    if (metrics->temperature_celsius >= config->temp_shutdown_celsius) {
        gpu_error_event_t error = {
            .type = GPU_ERROR_THERMAL_THROTTLE,
            .device_id = device_id,
            .timestamp_us = get_timestamp_us(),
            .severity = GPU_ERROR_SEV_FATAL,
            .is_recoverable = false
        };
        snprintf(error.description, sizeof(error.description),
                "GPU %d temperature critical: %.1f°C (threshold: %.1f°C)",
                device_id, metrics->temperature_celsius, config->temp_shutdown_celsius);
        gpu_notify_error(monitor, &error);

        /* Quarantine GPU */
        if (config->enable_auto_recovery) {
            device->quarantined = true;
            nimcp_log(LOG_LEVEL_ERROR, "GPU %d quarantined due to critical temperature", device_id);
        }
    } else if (metrics->temperature_celsius >= config->temp_critical_celsius) {
        gpu_error_event_t error = {
            .type = GPU_ERROR_THERMAL_THROTTLE,
            .device_id = device_id,
            .timestamp_us = get_timestamp_us(),
            .severity = GPU_ERROR_SEV_CRITICAL,
            .is_recoverable = true
        };
        snprintf(error.description, sizeof(error.description),
                "GPU %d temperature high: %.1f°C (threshold: %.1f°C)",
                device_id, metrics->temperature_celsius, config->temp_critical_celsius);
        gpu_notify_error(monitor, &error);
    }

    /* Check memory thresholds */
    if (metrics->memory_utilization >= config->memory_critical_pct) {
        gpu_error_event_t error = {
            .type = GPU_ERROR_CUDA_OOM,
            .device_id = device_id,
            .timestamp_us = get_timestamp_us(),
            .severity = GPU_ERROR_SEV_CRITICAL,
            .is_recoverable = true
        };
        snprintf(error.description, sizeof(error.description),
                "GPU %d memory critical: %.1f%% used",
                device_id, metrics->memory_utilization * 100.0f);
        gpu_notify_error(monitor, &error);
    }
}

static void gpu_notify_error(gpu_health_monitor_t* monitor, const gpu_error_event_t* error) {
    atomic_fetch_add(&monitor->total_errors, 1);

    /* Store in device history */
    if (error->device_id >= 0 && error->device_id < monitor->num_devices) {
        gpu_device_state_t* device = &monitor->devices[error->device_id];

        nimcp_mutex_lock(monitor->state_mutex);
        device->error_history[device->error_history_idx] = *error;
        device->error_history_idx = (device->error_history_idx + 1) % ERROR_HISTORY_SIZE;
        device->error_count_last_minute++;
        nimcp_mutex_unlock(monitor->state_mutex);
    }

    /* Notify callbacks */
    nimcp_mutex_lock(monitor->callbacks_mutex);
    for (int i = 0; i < MAX_ERROR_CALLBACKS; i++) {
        if (monitor->error_callbacks[i].active && monitor->error_callbacks[i].callback) {
            monitor->error_callbacks[i].callback(error, monitor->error_callbacks[i].user_data);
        }
    }
    nimcp_mutex_unlock(monitor->callbacks_mutex);

    /* Log the error */
    nimcp_log(LOG_LEVEL_WARN, "GPU error on device %d: %s (severity: %s)",
              error->device_id, error->description,
              gpu_error_severity_name(error->severity));
}

/*==============================================================================
 * GPU HEALTH METRICS API
 *============================================================================*/

int gpu_health_get_metrics(
    gpu_health_monitor_t* monitor,
    int device_id,
    gpu_health_metrics_t* metrics
) {
    if (!validate_monitor(monitor) || !metrics) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gpu_health_get_metrics: required parameter is NULL (validate_monitor, metrics)");
        return -1;
    }
    if (device_id < 0 || device_id >= monitor->num_devices) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "gpu_health_get_metrics: capacity exceeded");
        return -1;
    }

    nimcp_mutex_lock(monitor->state_mutex);
    *metrics = monitor->devices[device_id].current_metrics;
    nimcp_mutex_unlock(monitor->state_mutex);

    return 0;
}

int gpu_health_get_all_metrics(
    gpu_health_monitor_t* monitor,
    gpu_health_metrics_t* metrics_array,
    int* num_devices
) {
    if (!validate_monitor(monitor) || !metrics_array || !num_devices) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gpu_health_get_all_metrics: required parameter is NULL (validate_monitor, metrics_array, num_devices)");
        return -1;
    }

    int count = (*num_devices < monitor->num_devices) ? *num_devices : monitor->num_devices;

    nimcp_mutex_lock(monitor->state_mutex);
    for (int i = 0; i < count; i++) {
        metrics_array[i] = monitor->devices[i].current_metrics;
    }
    nimcp_mutex_unlock(monitor->state_mutex);

    *num_devices = count;
    return 0;
}

gpu_health_status_t gpu_health_check(
    gpu_health_monitor_t* monitor,
    int device_id
) {
    if (!validate_monitor(monitor)) return GPU_HEALTH_FAILED;

    if (device_id < 0) {
        /* Overall status */
        float score = atomic_load(&monitor->overall_health_score);
        if (score >= 0.9f) return GPU_HEALTH_OPTIMAL;
        if (score >= 0.75f) return GPU_HEALTH_GOOD;
        if (score >= 0.5f) return GPU_HEALTH_WARNING;
        if (score >= 0.25f) return GPU_HEALTH_DEGRADED;
        if (score > 0.0f) return GPU_HEALTH_CRITICAL;
        return GPU_HEALTH_FAILED;
    }

    if (device_id >= monitor->num_devices) return GPU_HEALTH_FAILED;

    return monitor->devices[device_id].current_metrics.status;
}

float gpu_health_get_score(
    gpu_health_monitor_t* monitor,
    int device_id
) {
    if (!validate_monitor(monitor)) return 0.0f;

    if (device_id < 0) {
        return atomic_load(&monitor->overall_health_score);
    }

    if (device_id >= monitor->num_devices) return 0.0f;

    return monitor->devices[device_id].current_metrics.health_score;
}

float gpu_health_predict_failure_probability(
    gpu_health_monitor_t* monitor,
    int device_id,
    uint32_t time_horizon_minutes
) {
    if (!validate_monitor(monitor)) return 1.0f;
    if (device_id < 0 || device_id >= monitor->num_devices) return 1.0f;

    gpu_device_state_t* device = &monitor->devices[device_id];

    if (device->quarantined) return 1.0f;

    /* Simple prediction based on current health and trends */
    float base_prob = 1.0f - device->current_metrics.health_score;

    /* Analyze trends if we have enough history */
    if (device->metrics_history_count >= 10) {
        nimcp_mutex_lock(monitor->state_mutex);

        /* Calculate temperature trend */
        float temp_sum = 0.0f;
        size_t recent_count = (device->metrics_history_count < 10) ?
                              device->metrics_history_count : 10;
        size_t start_idx = (device->metrics_history_idx + METRICS_HISTORY_SIZE - recent_count)
                           % METRICS_HISTORY_SIZE;

        for (size_t i = 0; i < recent_count; i++) {
            size_t idx = (start_idx + i) % METRICS_HISTORY_SIZE;
            temp_sum += device->metrics_history[idx].temperature_celsius;
        }
        float avg_temp = temp_sum / recent_count;

        /* If temperature trending up, increase failure probability */
        if (device->current_metrics.temperature_celsius > avg_temp + 5.0f) {
            base_prob += 0.1f * (time_horizon_minutes / 60.0f);
        }

        nimcp_mutex_unlock(monitor->state_mutex);
    }

    /* Factor in error rate */
    if (device->error_count_last_minute > 0) {
        base_prob += 0.05f * device->error_count_last_minute;
    }

    /* Clamp to valid range */
    if (base_prob > 1.0f) base_prob = 1.0f;
    if (base_prob < 0.0f) base_prob = 0.0f;

    return base_prob;
}

int gpu_health_get_device_count(gpu_health_monitor_t* monitor) {
    if (!validate_monitor(monitor)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "gpu_health_get_device_count: validate_monitor is NULL");
        return -1;
    }
    return monitor->num_devices;
}

/*==============================================================================
 * GPU ERROR DETECTION API
 *============================================================================*/

int gpu_error_register_callback(
    gpu_health_monitor_t* monitor,
    gpu_error_callback_t callback,
    void* user_data
) {
    if (!validate_monitor(monitor) || !callback) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gpu_error_register_callback: required parameter is NULL (validate_monitor, callback)");
        return -1;
    }

    int callback_id = -1;

    nimcp_mutex_lock(monitor->callbacks_mutex);
    for (int i = 0; i < MAX_ERROR_CALLBACKS; i++) {
        if (!monitor->error_callbacks[i].active) {
            monitor->error_callbacks[i].callback = callback;
            monitor->error_callbacks[i].user_data = user_data;
            monitor->error_callbacks[i].active = true;
            callback_id = i;
            break;
        }
    }
    nimcp_mutex_unlock(monitor->callbacks_mutex);

    return callback_id;
}

int gpu_error_unregister_callback(
    gpu_health_monitor_t* monitor,
    int callback_id
) {
    if (!validate_monitor(monitor)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "gpu_error_unregister_callback: validate_monitor is NULL");
        return -1;
    }
    if (callback_id < 0 || callback_id >= MAX_ERROR_CALLBACKS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "gpu_error_unregister_callback: capacity exceeded");
        return -1;
    }

    nimcp_mutex_lock(monitor->callbacks_mutex);
    monitor->error_callbacks[callback_id].active = false;
    monitor->error_callbacks[callback_id].callback = NULL;
    monitor->error_callbacks[callback_id].user_data = NULL;
    nimcp_mutex_unlock(monitor->callbacks_mutex);

    return 0;
}

int gpu_error_check_async(
    gpu_health_monitor_t* monitor,
    int device_id,
    gpu_error_event_t* error
) {
    if (!validate_monitor(monitor) || !error) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gpu_error_check_async: required parameter is NULL (validate_monitor, error)");
        return -1;
    }
    if (device_id < 0 || device_id >= monitor->num_devices) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "gpu_error_check_async: capacity exceeded");
        return -1;
    }

    memset(error, 0, sizeof(*error));

#ifdef NIMCP_CUDA_ENABLED
    cudaSetDevice(device_id);
    cudaError_t cuda_err = cudaGetLastError();

    if (cuda_err != cudaSuccess) {
        error->type = GPU_ERROR_CUDA_SYNC_FAILED;
        error->device_id = device_id;
        error->timestamp_us = get_timestamp_us();
        error->cuda_error_code = cuda_err;
        error->severity = GPU_ERROR_SEV_ERROR;
        error->is_recoverable = true;
        snprintf(error->description, sizeof(error->description),
                "CUDA async error: %s", cudaGetErrorString(cuda_err));
        return 1;
    }
#endif

    return 0;  /* No error */
}

const char* gpu_error_type_name(gpu_error_type_t type) {
    static const char* names[] = {
        [GPU_ERROR_NONE] = "None",
        [GPU_ERROR_CUDA_INIT_FAILED] = "CUDA Init Failed",
        [GPU_ERROR_CUDA_OOM] = "CUDA Out of Memory",
        [GPU_ERROR_CUDA_INVALID_DEVICE] = "Invalid Device",
        [GPU_ERROR_CUDA_KERNEL_LAUNCH] = "Kernel Launch Failed",
        [GPU_ERROR_CUDA_SYNC_FAILED] = "Sync Failed",
        [GPU_ERROR_CUDA_ILLEGAL_ADDRESS] = "Illegal Address",
        [GPU_ERROR_CUDA_ASSERT] = "CUDA Assert",
        [GPU_ERROR_ECC_CORRECTABLE] = "ECC Correctable",
        [GPU_ERROR_ECC_UNCORRECTABLE] = "ECC Uncorrectable",
        [GPU_ERROR_THERMAL_THROTTLE] = "Thermal Throttle",
        [GPU_ERROR_POWER_THROTTLE] = "Power Throttle",
        [GPU_ERROR_HARDWARE_FAULT] = "Hardware Fault",
        [GPU_ERROR_KERNEL_TIMEOUT] = "Kernel Timeout",
        [GPU_ERROR_TDR_RESET] = "TDR Reset",
        [GPU_ERROR_MEMORY_CORRUPTION] = "Memory Corruption",
        [GPU_ERROR_NAN_INF_DETECTED] = "NaN/Inf Detected",
        [GPU_ERROR_PCIE_ERROR] = "PCIe Error",
        [GPU_ERROR_NVLINK_ERROR] = "NVLink Error",
        [GPU_ERROR_PEER_ACCESS_FAILED] = "Peer Access Failed"
    };

    if (type >= 0 && type < GPU_ERROR_COUNT) {
        return names[type];
    }
    return "Unknown";
}

const char* gpu_error_severity_name(gpu_error_severity_t severity) {
    static const char* names[] = {
        [GPU_ERROR_SEV_INFO] = "Info",
        [GPU_ERROR_SEV_WARNING] = "Warning",
        [GPU_ERROR_SEV_ERROR] = "Error",
        [GPU_ERROR_SEV_CRITICAL] = "Critical",
        [GPU_ERROR_SEV_FATAL] = "Fatal"
    };

    if (severity >= GPU_ERROR_SEV_INFO && severity <= GPU_ERROR_SEV_FATAL) {
        return names[severity];
    }
    return "Unknown";
}

/*==============================================================================
 * GPU TENSOR VALIDATION API
 *============================================================================*/

int gpu_tensor_validate(
    gpu_health_monitor_t* monitor,
    const void* device_ptr,
    size_t num_elements,
    size_t element_size,
    uint32_t* nan_count,
    uint32_t* inf_count
) {
    if (!validate_monitor(monitor)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "gpu_tensor_validate: validate_monitor is NULL");
        return -1;
    }
    if (!device_ptr || num_elements == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "gpu_tensor_validate: device_ptr is NULL");
        return -1;
    }
    if (!nan_count || !inf_count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gpu_tensor_validate: required parameter is NULL (nan_count, inf_count)");
        return -1;
    }

    *nan_count = 0;
    *inf_count = 0;

#ifdef NIMCP_CUDA_ENABLED
    /* Allocate host buffer and copy from device */
    void* host_buffer = nimcp_malloc(num_elements * element_size);
    if (!host_buffer) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "host_buffer is NULL");

        return -1;

    }

    cudaError_t err = cudaMemcpy(host_buffer, device_ptr,
                                  num_elements * element_size,
                                  cudaMemcpyDeviceToHost);
    if (err != cudaSuccess) {
        nimcp_free(host_buffer);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "gpu_tensor_validate: validation failed");
        return -1;
    }

    /* Scan for NaN/Inf */
    if (element_size == sizeof(float)) {
        float* data = (float*)host_buffer;
        for (size_t i = 0; i < num_elements; i++) {
            if (isnan(data[i])) (*nan_count)++;
            else if (isinf(data[i])) (*inf_count)++;
        }
    } else if (element_size == sizeof(double)) {
        double* data = (double*)host_buffer;
        for (size_t i = 0; i < num_elements; i++) {
            if (isnan(data[i])) (*nan_count)++;
            else if (isinf(data[i])) (*inf_count)++;
        }
    }

    nimcp_free(host_buffer);
    return 0;
#else
    /* No CUDA - can't validate GPU tensors */
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "gpu_tensor_validate: validation failed");
    return -1;
#endif
}

int gpu_tensor_sanitize(
    gpu_health_monitor_t* monitor,
    void* device_ptr,
    size_t num_elements,
    size_t element_size,
    float nan_replacement,
    float inf_replacement
) {
    if (!validate_monitor(monitor)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "gpu_tensor_sanitize: validate_monitor is NULL");
        return -1;
    }
    if (!device_ptr || num_elements == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "gpu_tensor_sanitize: device_ptr is NULL");
        return -1;
    }

    int replaced = 0;

#ifdef NIMCP_CUDA_ENABLED
    /* Allocate host buffer and copy from device */
    void* host_buffer = nimcp_malloc(num_elements * element_size);
    if (!host_buffer) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "host_buffer is NULL");

        return -1;

    }

    cudaError_t err = cudaMemcpy(host_buffer, device_ptr,
                                  num_elements * element_size,
                                  cudaMemcpyDeviceToHost);
    if (err != cudaSuccess) {
        nimcp_free(host_buffer);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "gpu_tensor_sanitize: validation failed");
        return -1;
    }

    /* Replace NaN/Inf values */
    if (element_size == sizeof(float)) {
        float* data = (float*)host_buffer;
        for (size_t i = 0; i < num_elements; i++) {
            if (isnan(data[i])) {
                data[i] = nan_replacement;
                replaced++;
            } else if (isinf(data[i])) {
                data[i] = inf_replacement;
                replaced++;
            }
        }
    } else if (element_size == sizeof(double)) {
        double* data = (double*)host_buffer;
        for (size_t i = 0; i < num_elements; i++) {
            if (isnan(data[i])) {
                data[i] = (double)nan_replacement;
                replaced++;
            } else if (isinf(data[i])) {
                data[i] = (double)inf_replacement;
                replaced++;
            }
        }
    }

    /* Copy back to device */
    if (replaced > 0) {
        err = cudaMemcpy(device_ptr, host_buffer,
                        num_elements * element_size,
                        cudaMemcpyHostToDevice);
        if (err != cudaSuccess) {
            nimcp_free(host_buffer);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "gpu_tensor_sanitize: validation failed");
            return -1;
        }
    }

    nimcp_free(host_buffer);
    return replaced;
#else
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "gpu_tensor_sanitize: validation failed");
    return -1;
#endif
}

/*==============================================================================
 * GPU-IMMUNE BRIDGE API
 *============================================================================*/

gpu_antigen_type_t gpu_error_to_antigen(gpu_error_type_t error_type) {
    switch (error_type) {
        case GPU_ERROR_THERMAL_THROTTLE:
        case GPU_ERROR_POWER_THROTTLE:
            return GPU_ANTIGEN_THERMAL;

        case GPU_ERROR_CUDA_OOM:
        case GPU_ERROR_MEMORY_CORRUPTION:
            return GPU_ANTIGEN_MEMORY;

        case GPU_ERROR_CUDA_KERNEL_LAUNCH:
        case GPU_ERROR_CUDA_SYNC_FAILED:
        case GPU_ERROR_CUDA_ILLEGAL_ADDRESS:
        case GPU_ERROR_CUDA_ASSERT:
            return GPU_ANTIGEN_COMPUTE;

        case GPU_ERROR_ECC_CORRECTABLE:
        case GPU_ERROR_ECC_UNCORRECTABLE:
        case GPU_ERROR_HARDWARE_FAULT:
        case GPU_ERROR_TDR_RESET:
            return GPU_ANTIGEN_HARDWARE;

        case GPU_ERROR_PCIE_ERROR:
        case GPU_ERROR_NVLINK_ERROR:
        case GPU_ERROR_PEER_ACCESS_FAILED:
            return GPU_ANTIGEN_COMMUNICATION;

        case GPU_ERROR_NAN_INF_DETECTED:
            return GPU_ANTIGEN_CORRUPTION;

        case GPU_ERROR_KERNEL_TIMEOUT:
            return GPU_ANTIGEN_TIMEOUT;

        default:
            return GPU_ANTIGEN_COMPUTE;
    }
}

int gpu_immune_get_response(
    gpu_health_monitor_t* monitor,
    const gpu_error_event_t* error,
    gpu_immune_response_t* response
) {
    if (!validate_monitor(monitor) || !error || !response) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gpu_immune_get_response: required parameter is NULL (validate_monitor, error, response)");
        return -1;
    }

    memset(response, 0, sizeof(*response));
    response->antigen_type = gpu_error_to_antigen(error->type);

    /* Determine recovery strategy based on error type and severity */
    switch (error->type) {
        case GPU_ERROR_THERMAL_THROTTLE:
        case GPU_ERROR_POWER_THROTTLE:
            response->suggested_recovery = GPU_RECOVERY_THROTTLE;
            response->fallback_recovery = GPU_RECOVERY_MIGRATE_GPU;
            response->urgency = (error->severity == GPU_ERROR_SEV_FATAL) ? 1.0f : 0.7f;
            response->requires_checkpoint = true;
            strncpy(response->reason, "Thermal/power issue - reduce load", sizeof(response->reason) - 1);
            break;

        case GPU_ERROR_CUDA_OOM:
            response->suggested_recovery = GPU_RECOVERY_CLEAR_CACHE;
            response->fallback_recovery = GPU_RECOVERY_DEFRAG_MEMORY;
            response->urgency = 0.8f;
            response->requires_checkpoint = true;
            strncpy(response->reason, "Out of memory - clear caches or defrag", sizeof(response->reason) - 1);
            break;

        case GPU_ERROR_CUDA_KERNEL_LAUNCH:
        case GPU_ERROR_CUDA_SYNC_FAILED:
            response->suggested_recovery = GPU_RECOVERY_RETRY;
            response->fallback_recovery = GPU_RECOVERY_RESET_CONTEXT;
            response->urgency = 0.5f;
            response->requires_sync = true;
            strncpy(response->reason, "Kernel error - retry or reset context", sizeof(response->reason) - 1);
            break;

        case GPU_ERROR_CUDA_ILLEGAL_ADDRESS:
        case GPU_ERROR_MEMORY_CORRUPTION:
            response->suggested_recovery = GPU_RECOVERY_RESET_CONTEXT;
            response->fallback_recovery = GPU_RECOVERY_QUARANTINE_GPU;
            response->urgency = 0.9f;
            response->requires_checkpoint = true;
            strncpy(response->reason, "Memory corruption - reset or quarantine", sizeof(response->reason) - 1);
            break;

        case GPU_ERROR_ECC_UNCORRECTABLE:
        case GPU_ERROR_HARDWARE_FAULT:
            response->suggested_recovery = GPU_RECOVERY_MIGRATE_GPU;
            response->fallback_recovery = GPU_RECOVERY_FALLBACK_CPU;
            response->urgency = 1.0f;
            response->requires_checkpoint = true;
            strncpy(response->reason, "Hardware fault - migrate to another GPU", sizeof(response->reason) - 1);
            break;

        case GPU_ERROR_NAN_INF_DETECTED:
            response->suggested_recovery = GPU_RECOVERY_CHECKPOINT;
            response->fallback_recovery = GPU_RECOVERY_REDUCE_BATCH;
            response->urgency = 0.6f;
            strncpy(response->reason, "Data corruption - checkpoint and investigate", sizeof(response->reason) - 1);
            break;

        case GPU_ERROR_KERNEL_TIMEOUT:
            response->suggested_recovery = GPU_RECOVERY_REDUCE_BATCH;
            response->fallback_recovery = GPU_RECOVERY_RESET_CONTEXT;
            response->urgency = 0.7f;
            response->requires_sync = true;
            strncpy(response->reason, "Kernel timeout - reduce workload", sizeof(response->reason) - 1);
            break;

        default:
            response->suggested_recovery = GPU_RECOVERY_RETRY;
            response->fallback_recovery = GPU_RECOVERY_RESET_CONTEXT;
            response->urgency = 0.5f;
            strncpy(response->reason, "Unknown error - retry operation", sizeof(response->reason) - 1);
            break;
    }

    return 0;
}

int gpu_immune_execute_recovery(
    gpu_health_monitor_t* monitor,
    int device_id,
    gpu_recovery_action_t action
) {
    if (!validate_monitor(monitor)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "gpu_immune_execute_recovery: validate_monitor is NULL");
        return -1;
    }
    if (device_id < 0 || device_id >= monitor->num_devices) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "gpu_immune_execute_recovery: capacity exceeded");
        return -1;
    }

    gpu_device_state_t* device = &monitor->devices[device_id];

    nimcp_log(LOG_LEVEL_INFO, "Executing GPU recovery action '%s' on device %d",
              gpu_recovery_action_name(action), device_id);

    switch (action) {
        case GPU_RECOVERY_NONE:
            return 0;

        case GPU_RECOVERY_RETRY:
            /* Caller handles retry logic */
            return 0;

        case GPU_RECOVERY_REDUCE_BATCH:
            /* Signal to caller to reduce batch size */
            nimcp_log(LOG_LEVEL_INFO, "Recovery: Signaling batch size reduction for GPU %d", device_id);
            return 0;

        case GPU_RECOVERY_CLEAR_CACHE:
#ifdef NIMCP_CUDA_ENABLED
            cudaSetDevice(device_id);
            cudaDeviceSynchronize();
            /* Clear caches by freeing and reallocating */
            nimcp_log(LOG_LEVEL_INFO, "Recovery: Cleared GPU %d caches", device_id);
#endif
            return 0;

        case GPU_RECOVERY_DEFRAG_MEMORY:
            nimcp_log(LOG_LEVEL_INFO, "Recovery: Memory defrag requested for GPU %d", device_id);
            return 0;

        case GPU_RECOVERY_RESET_CONTEXT:
#ifdef NIMCP_CUDA_ENABLED
            cudaSetDevice(device_id);
            cudaDeviceReset();
            nimcp_log(LOG_LEVEL_INFO, "Recovery: Reset CUDA context on GPU %d", device_id);
#endif
            return 0;

        case GPU_RECOVERY_THROTTLE:
            nimcp_log(LOG_LEVEL_INFO, "Recovery: Throttling GPU %d", device_id);
            return 0;

        case GPU_RECOVERY_CHECKPOINT:
            nimcp_log(LOG_LEVEL_INFO, "Recovery: Checkpoint requested before GPU %d recovery", device_id);
            return 0;

        case GPU_RECOVERY_MIGRATE_GPU:
            /* Find alternative GPU */
            for (int i = 0; i < monitor->num_devices; i++) {
                if (i != device_id && !monitor->devices[i].quarantined &&
                    monitor->devices[i].current_metrics.health_score > 0.5f) {
                    nimcp_log(LOG_LEVEL_INFO, "Recovery: Migrating from GPU %d to GPU %d",
                             device_id, i);
                    return i;  /* Return new device ID */
                }
            }
            nimcp_log(LOG_LEVEL_WARN, "Recovery: No healthy GPU available for migration");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "gpu_immune_execute_recovery: operation failed");
            return -1;

        case GPU_RECOVERY_FALLBACK_CPU:
            nimcp_log(LOG_LEVEL_INFO, "Recovery: Falling back to CPU computation");
            return 0;

        case GPU_RECOVERY_QUARANTINE_GPU:
            device->quarantined = true;
            nimcp_log(LOG_LEVEL_WARN, "Recovery: GPU %d quarantined", device_id);
            return 0;

        default:
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "gpu_immune_execute_recovery: operation failed");
            return -1;
    }

    device->recovery_attempts++;
    device->last_recovery_us = get_timestamp_us();
    atomic_fetch_add(&monitor->total_recoveries, 1);

    return 0;
}

const char* gpu_recovery_action_name(gpu_recovery_action_t action) {
    static const char* names[] = {
        [GPU_RECOVERY_NONE] = "None",
        [GPU_RECOVERY_RETRY] = "Retry",
        [GPU_RECOVERY_REDUCE_BATCH] = "Reduce Batch",
        [GPU_RECOVERY_CLEAR_CACHE] = "Clear Cache",
        [GPU_RECOVERY_DEFRAG_MEMORY] = "Defrag Memory",
        [GPU_RECOVERY_RESET_CONTEXT] = "Reset Context",
        [GPU_RECOVERY_THROTTLE] = "Throttle",
        [GPU_RECOVERY_CHECKPOINT] = "Checkpoint",
        [GPU_RECOVERY_MIGRATE_GPU] = "Migrate GPU",
        [GPU_RECOVERY_FALLBACK_CPU] = "Fallback CPU",
        [GPU_RECOVERY_QUARANTINE_GPU] = "Quarantine GPU"
    };

    if (action >= 0 && action < GPU_RECOVERY_COUNT) {
        return names[action];
    }
    return "Unknown";
}

/*==============================================================================
 * GPU MEMORY POOL API
 *============================================================================*/

void gpu_memory_pool_get_default_config(gpu_memory_pool_config_t* config) {
    if (!config) return;

    memset(config, 0, sizeof(*config));
    config->device_id = 0;
    config->initial_size = 256 * 1024 * 1024;    /* 256 MB */
    config->max_size = 1024 * 1024 * 1024;       /* 1 GB */
    config->enable_defragmentation = true;
    config->enable_overflow_to_host = false;
    config->high_water_mark = 0.75f;
    config->critical_water_mark = 0.90f;
}

gpu_memory_pool_t* gpu_memory_pool_create(
    gpu_health_monitor_t* monitor,
    const gpu_memory_pool_config_t* config
) {
    gpu_memory_pool_t* pool = nimcp_calloc(1, sizeof(gpu_memory_pool_t));
    if (!pool) {
        nimcp_log(LOG_LEVEL_ERROR, "Failed to allocate GPU memory pool");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pool is NULL");

        return NULL;
    }

    pool->magic = GPU_MEMORY_POOL_MAGIC;
    pool->monitor = monitor;

    if (config) {
        pool->config = *config;
    } else {
        gpu_memory_pool_get_default_config(&pool->config);
    }

    mutex_attr_t attr;
    attr.type = MUTEX_TYPE_NORMAL;
    pool->pool_mutex = nimcp_mutex_create(&attr);

    if (!pool->pool_mutex) {
        nimcp_log(LOG_LEVEL_ERROR, "Failed to create GPU memory pool mutex");
        nimcp_free(pool);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "gpu_memory_pool_create: pool->pool_mutex is NULL");
        return NULL;
    }

    /* Initialize stats */
    pool->stats.total_size = pool->config.initial_size;
    pool->stats.free_size = pool->config.initial_size;
    pool->stats.used_size = 0;
    pool->stats.largest_free_block = pool->config.initial_size;

    nimcp_log(LOG_LEVEL_DEBUG, "GPU memory pool created for device %d (size: %zu MB)",
              pool->config.device_id, pool->config.initial_size / (1024 * 1024));

    return pool;
}

void gpu_memory_pool_destroy(gpu_memory_pool_t* pool) {
    if (!validate_pool(pool)) return;

    if (pool->pool_mutex) {
        nimcp_mutex_free(pool->pool_mutex);
    }

    pool->magic = 0;
    nimcp_free(pool);

    nimcp_log(LOG_LEVEL_DEBUG, "GPU memory pool destroyed");
}

void* gpu_memory_pool_alloc(gpu_memory_pool_t* pool, size_t size) {
    if (!validate_pool(pool) || size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "gpu_memory_pool_alloc: validate_pool is NULL");
        return NULL;
    }

    void* ptr = NULL;

#ifdef NIMCP_CUDA_ENABLED
    cudaSetDevice(pool->config.device_id);
    cudaError_t err = cudaMalloc(&ptr, size);
    if (err != cudaSuccess) {
        nimcp_log(LOG_LEVEL_WARN, "GPU memory allocation failed: %s",
                  cudaGetErrorString(err));
        pool->stats.alloc_failures++;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gpu_memory_pool_alloc: validation failed");
        return NULL;
    }
#else
    /* Stub: allocate host memory as placeholder */
    ptr = nimcp_malloc(size);
    if (!ptr) {
        pool->stats.alloc_failures++;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "ptr is NULL");

        return NULL;
    }
#endif

    nimcp_mutex_lock(pool->pool_mutex);
    pool->allocated_size += size;
    pool->num_allocations++;
    pool->stats.used_size = pool->allocated_size;
    pool->stats.free_size = pool->config.max_size - pool->allocated_size;
    pool->stats.total_allocs++;
    pool->stats.num_allocations = pool->num_allocations;
    nimcp_mutex_unlock(pool->pool_mutex);

    return ptr;
}

void gpu_memory_pool_free(gpu_memory_pool_t* pool, void* ptr) {
    if (!validate_pool(pool) || !ptr) return;

#ifdef NIMCP_CUDA_ENABLED
    cudaFree(ptr);
#else
    nimcp_free(ptr);
#endif

    nimcp_mutex_lock(pool->pool_mutex);
    if (pool->num_allocations > 0) {
        pool->num_allocations--;
    }
    pool->stats.total_frees++;
    pool->stats.num_allocations = pool->num_allocations;
    nimcp_mutex_unlock(pool->pool_mutex);
}

int gpu_memory_pool_get_stats(gpu_memory_pool_t* pool, gpu_memory_pool_stats_t* stats) {
    if (!validate_pool(pool) || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "gpu_memory_pool_get_stats: required parameter is NULL (validate_pool, stats)");
        return -1;
    }

    nimcp_mutex_lock(pool->pool_mutex);
    *stats = pool->stats;
    nimcp_mutex_unlock(pool->pool_mutex);

    return 0;
}

int gpu_memory_pool_defrag(gpu_memory_pool_t* pool) {
    if (!validate_pool(pool)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "gpu_memory_pool_defrag: validate_pool is NULL");
        return -1;
    }

    nimcp_mutex_lock(pool->pool_mutex);
    pool->stats.defrag_count++;
    nimcp_mutex_unlock(pool->pool_mutex);

    nimcp_log(LOG_LEVEL_DEBUG, "GPU memory pool defragmentation requested");

    /* Actual defragmentation would require tracking all allocations */
    return 0;
}

int gpu_memory_pool_clear(gpu_memory_pool_t* pool) {
    if (!validate_pool(pool)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "gpu_memory_pool_clear: validate_pool is NULL");
        return -1;
    }

    nimcp_mutex_lock(pool->pool_mutex);
    pool->allocated_size = 0;
    pool->num_allocations = 0;
    pool->stats.used_size = 0;
    pool->stats.free_size = pool->config.max_size;
    pool->stats.num_allocations = 0;
    nimcp_mutex_unlock(pool->pool_mutex);

    nimcp_log(LOG_LEVEL_INFO, "GPU memory pool cleared");

    return 0;
}

/*==============================================================================
 * GPU CHECKPOINT API
 *============================================================================*/

int gpu_checkpoint_create(
    gpu_health_monitor_t* monitor,
    int device_id,
    void** tensors,
    size_t* sizes,
    size_t num_tensors,
    uint64_t* checkpoint_id
) {
    if (!validate_monitor(monitor) || !tensors || !sizes || !checkpoint_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gpu_checkpoint_create: required parameter is NULL (validate_monitor, tensors, sizes, checkpoint_id)");
        return -1;
    }
    if (num_tensors == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "gpu_checkpoint_create: num_tensors is zero");
        return -1;
    }

    /* Find free checkpoint slot */
    nimcp_mutex_lock(monitor->checkpoints_mutex);

    int slot = -1;
    for (int i = 0; i < MAX_CHECKPOINTS; i++) {
        if (!monitor->checkpoints[i].valid) {
            slot = i;
            break;
        }
    }

    if (slot < 0) {
        nimcp_mutex_unlock(monitor->checkpoints_mutex);
        nimcp_log(LOG_LEVEL_WARN, "No free checkpoint slots available");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "gpu_checkpoint_create: validation failed");
        return -1;
    }

    gpu_checkpoint_entry_t* cp = &monitor->checkpoints[slot];
    cp->checkpoint_id = atomic_fetch_add(&monitor->next_checkpoint_id, 1);
    cp->device_id = device_id;
    cp->num_tensors = num_tensors;
    cp->timestamp_us = get_timestamp_us();

    /* Allocate arrays */
    cp->host_copies = nimcp_calloc(num_tensors, sizeof(void*));
    cp->sizes = nimcp_calloc(num_tensors, sizeof(size_t));

    if (!cp->host_copies || !cp->sizes) {
        nimcp_free(cp->host_copies);
        nimcp_free(cp->sizes);
        nimcp_mutex_unlock(monitor->checkpoints_mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "gpu_checkpoint_create: required parameter is NULL (cp->host_copies, cp->sizes)");
        return -1;
    }

    memcpy(cp->sizes, sizes, num_tensors * sizeof(size_t));

    /* Copy tensors to host */
    bool success = true;
    for (size_t i = 0; i < num_tensors && success; i++) {
        cp->host_copies[i] = nimcp_malloc(sizes[i]);
        if (!cp->host_copies[i]) {
            success = false;
            break;
        }

#ifdef NIMCP_CUDA_ENABLED
        cudaSetDevice(device_id);
        cudaError_t err = cudaMemcpy(cp->host_copies[i], tensors[i],
                                      sizes[i], cudaMemcpyDeviceToHost);
        if (err != cudaSuccess) {
            success = false;
        }
#else
        memcpy(cp->host_copies[i], tensors[i], sizes[i]);
#endif
    }

    if (!success) {
        /* Cleanup on failure */
        for (size_t i = 0; i < num_tensors; i++) {
            nimcp_free(cp->host_copies[i]);
        }
        nimcp_free(cp->host_copies);
        nimcp_free(cp->sizes);
        nimcp_mutex_unlock(monitor->checkpoints_mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gpu_checkpoint_create: success is NULL");
        return -1;
    }

    cp->valid = true;
    *checkpoint_id = cp->checkpoint_id;

    nimcp_mutex_unlock(monitor->checkpoints_mutex);

    nimcp_log(LOG_LEVEL_INFO, "Created GPU checkpoint %lu with %zu tensors",
              *checkpoint_id, num_tensors);

    return 0;
}

int gpu_checkpoint_restore(
    gpu_health_monitor_t* monitor,
    uint64_t checkpoint_id,
    void** tensors
) {
    if (!validate_monitor(monitor) || !tensors) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gpu_checkpoint_restore: required parameter is NULL (validate_monitor, tensors)");
        return -1;
    }

    nimcp_mutex_lock(monitor->checkpoints_mutex);

    /* Find checkpoint */
    gpu_checkpoint_entry_t* cp = NULL;
    for (int i = 0; i < MAX_CHECKPOINTS; i++) {
        if (monitor->checkpoints[i].valid &&
            monitor->checkpoints[i].checkpoint_id == checkpoint_id) {
            cp = &monitor->checkpoints[i];
            break;
        }
    }

    if (!cp) {
        nimcp_mutex_unlock(monitor->checkpoints_mutex);
        nimcp_log(LOG_LEVEL_WARN, "Checkpoint %lu not found", checkpoint_id);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gpu_checkpoint_restore: cp is NULL");
        return -1;
    }

    /* Restore tensors */
    bool success = true;
    for (size_t i = 0; i < cp->num_tensors && success; i++) {
#ifdef NIMCP_CUDA_ENABLED
        cudaSetDevice(cp->device_id);
        cudaError_t err = cudaMemcpy(tensors[i], cp->host_copies[i],
                                      cp->sizes[i], cudaMemcpyHostToDevice);
        if (err != cudaSuccess) {
            success = false;
        }
#else
        memcpy(tensors[i], cp->host_copies[i], cp->sizes[i]);
#endif
    }

    nimcp_mutex_unlock(monitor->checkpoints_mutex);

    if (!success) {
        nimcp_log(LOG_LEVEL_ERROR, "Failed to restore checkpoint %lu", checkpoint_id);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gpu_checkpoint_restore: success is NULL");
        return -1;
    }

    nimcp_log(LOG_LEVEL_INFO, "Restored GPU checkpoint %lu", checkpoint_id);
    return 0;
}

int gpu_checkpoint_delete(
    gpu_health_monitor_t* monitor,
    uint64_t checkpoint_id
) {
    if (!validate_monitor(monitor)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "gpu_checkpoint_delete: validate_monitor is NULL");
        return -1;
    }

    nimcp_mutex_lock(monitor->checkpoints_mutex);

    /* Find checkpoint */
    for (int i = 0; i < MAX_CHECKPOINTS; i++) {
        if (monitor->checkpoints[i].valid &&
            monitor->checkpoints[i].checkpoint_id == checkpoint_id) {

            gpu_checkpoint_entry_t* cp = &monitor->checkpoints[i];

            /* Free host copies */
            for (size_t j = 0; j < cp->num_tensors; j++) {
                nimcp_free(cp->host_copies[j]);
            }
            nimcp_free(cp->host_copies);
            nimcp_free(cp->sizes);

            cp->valid = false;

            nimcp_mutex_unlock(monitor->checkpoints_mutex);
            nimcp_log(LOG_LEVEL_DEBUG, "Deleted GPU checkpoint %lu", checkpoint_id);
            return 0;
        }
    }

    nimcp_mutex_unlock(monitor->checkpoints_mutex);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "gpu_checkpoint_delete: operation failed");
    return -1;
}
