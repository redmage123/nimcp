/**
 * @file nimcp_continuous_monitor.c
 * @brief Implementation of Continuous Security Monitoring
 *
 * WHAT: Implements continuous real-time security monitoring with no temporal gaps.
 *
 * WHY:  Security monitoring must be continuous - any gap is a vulnerability.
 *       This module ensures monitoring is ALWAYS active.
 *
 * HOW:  Implements a polling-based monitoring loop that:
 *       - Runs verification checks on a configurable interval
 *       - Generates alerts for violations
 *       - Maintains statistics and audit trail
 *       - Provides callbacks for alert handling
 *
 * Part of Phase SC-1: Security Coverage Framework (Tier 0.7)
 */

#include "security/nimcp_continuous_monitor.h"
#include "security/nimcp_security.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/validation/nimcp_common.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"

#define LOG_MODULE "security_continuous_monitor"
#include "utils/bridge/nimcp_bridge_boilerplate.h"

BRIDGE_BOILERPLATE_MESH_ONLY(continuous_monitor, MESH_ADAPTER_CATEGORY_SECURITY)


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "constants/nimcp_buffer_constants.h"

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Alert callback registration
 */
typedef struct {
    nimcp_alert_callback_t callback;
    void* user_data;
    bool active;
} callback_registration_t;

/**
 * @brief Alert queue entry
 */
typedef struct {
    nimcp_alert_t alert;
    bool used;
} alert_entry_t;

/**
 * @brief Internal continuous monitor context
 */
struct nimcp_continuous_monitor {
    // Associated coverage context
    nimcp_security_coverage_t* coverage;

    // Configuration
    nimcp_monitor_config_t config;

    // State
    nimcp_monitor_state_t state;
    nimcp_mutex_t state_mutex;

    // Monitoring thread
    nimcp_thread_t monitor_thread;
    bool thread_running;
    bool stop_requested;

    // Alert queue
    alert_entry_t alert_queue[NIMCP_MONITOR_MAX_ALERTS];
    uint32_t alert_head;
    uint32_t alert_tail;
    uint32_t alert_count;
    nimcp_mutex_t alert_mutex;

    // Callbacks
    callback_registration_t callbacks[NIMCP_MONITOR_MAX_CALLBACKS];
    uint32_t num_callbacks;
    nimcp_mutex_t callback_mutex;

    // Statistics
    nimcp_monitor_stats_t stats;
    uint64_t start_time;
    uint64_t total_cycle_time;
    nimcp_mutex_t stats_mutex;

    // Initialized flag
    bool initialized;
};

//=============================================================================
// Helper Functions
//=============================================================================

/**
 * @brief Get current timestamp in milliseconds
 */
static uint64_t get_timestamp_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)(ts.tv_sec * NIMCP_MS_PER_SEC + ts.tv_nsec / NIMCP_NS_PER_MS);
}

/**
 * @brief Create and queue an alert
 */
static void queue_alert(
    nimcp_continuous_monitor_t* monitor,
    nimcp_alert_type_t type,
    nimcp_alert_severity_t severity,
    uint32_t source_id,
    const char* message)
{
    if (!monitor)
        return;

    nimcp_mutex_lock(&monitor->alert_mutex);

    // Check if queue is full
    if (monitor->alert_count >= NIMCP_MONITOR_MAX_ALERTS) {
        nimcp_mutex_unlock(&monitor->alert_mutex);

        // Log that we're dropping alerts
        nimcp_security_log_event(
            NIMCP_SECURITY_EVENT_THREAT_DETECTED,
            NIMCP_THREAT_HIGH,
            "Alert queue full - dropping alert"
        );
        return;
    }

    // Find next available slot
    alert_entry_t* entry = &monitor->alert_queue[monitor->alert_tail];

    entry->alert.type = type;
    entry->alert.severity = severity;
    entry->alert.timestamp = get_timestamp_ms();
    entry->alert.source_id = source_id;
    entry->alert.acknowledged = false;
    entry->alert.context = NULL;
    entry->used = true;

    if (message) {
        strncpy(entry->alert.message, message, sizeof(entry->alert.message) - 1);
        entry->alert.message[sizeof(entry->alert.message) - 1] = '\0';
    } else {
        entry->alert.message[0] = '\0';
    }

    monitor->alert_tail = (monitor->alert_tail + 1) % NIMCP_MONITOR_MAX_ALERTS;
    monitor->alert_count++;

    // Update statistics
    nimcp_mutex_lock(&monitor->stats_mutex);
    monitor->stats.alerts_generated++;
    nimcp_mutex_unlock(&monitor->stats_mutex);

    nimcp_mutex_unlock(&monitor->alert_mutex);

    // Invoke callbacks
    nimcp_mutex_lock(&monitor->callback_mutex);
    for (uint32_t i = 0; i < NIMCP_MONITOR_MAX_CALLBACKS; i++) {
        if (monitor->callbacks[i].active && monitor->callbacks[i].callback) {
            bool ack = monitor->callbacks[i].callback(
                &entry->alert,
                monitor->callbacks[i].user_data
            );
            if (ack) {
                entry->alert.acknowledged = true;
                nimcp_mutex_lock(&monitor->stats_mutex);
                monitor->stats.alerts_acknowledged++;
                nimcp_mutex_unlock(&monitor->stats_mutex);
            }
        }
    }
    nimcp_mutex_unlock(&monitor->callback_mutex);

    // Log critical and above
    if (severity >= NIMCP_ALERT_CRITICAL) {
        nimcp_security_log_event(
            NIMCP_SECURITY_EVENT_THREAT_DETECTED,
            NIMCP_THREAT_CRITICAL,
            message ? message : "Critical security alert"
        );
    }
}

/**
 * @brief Run memory integrity check
 */
static bool run_memory_check(nimcp_continuous_monitor_t* monitor)
{
    if (!monitor || !monitor->coverage)
        return true;

    bool result = nimcp_coverage_verify_all_regions(monitor->coverage);

    if (!result) {
        queue_alert(
            monitor,
            NIMCP_ALERT_TYPE_MEMORY_VIOLATION,
            NIMCP_ALERT_CRITICAL,
            0,
            "Memory integrity violation detected"
        );
    }

    return result;
}

/**
 * @brief Run CFI validation check
 */
static bool run_cfi_check(nimcp_continuous_monitor_t* monitor)
{
    if (!monitor || !monitor->coverage)
        return true;

    // CFI checks are performed inline during code execution
    // This check verifies the overall CFI status
    nimcp_dimension_stats_t stats;
    nimcp_coverage_verify_dimension(
        monitor->coverage,
        NIMCP_COVERAGE_CODE_PATHS,
        &stats
    );

    if (stats.violations_detected > 0) {
        char msg[NIMCP_LABEL_BUFFER_SIZE];
        snprintf(msg, sizeof(msg),
                 "CFI violations detected: %lu total",
                 (unsigned long)stats.violations_detected);
        queue_alert(
            monitor,
            NIMCP_ALERT_TYPE_CFI_VIOLATION,
            NIMCP_ALERT_CRITICAL,
            0,
            msg
        );
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "run_cfi_check: operation failed");
        return false;
    }

    return true;
}

/**
 * @brief Run IPC authentication check
 */
static bool run_ipc_check(nimcp_continuous_monitor_t* monitor)
{
    if (!monitor || !monitor->coverage)
        return true;

    nimcp_dimension_stats_t stats;
    nimcp_coverage_verify_dimension(
        monitor->coverage,
        NIMCP_COVERAGE_IPC_CHANNELS,
        &stats
    );

    if (stats.violations_detected > 0) {
        char msg[NIMCP_LABEL_BUFFER_SIZE];
        snprintf(msg, sizeof(msg),
                 "IPC authentication failures: %lu total",
                 (unsigned long)stats.violations_detected);
        queue_alert(
            monitor,
            NIMCP_ALERT_TYPE_AUTH_FAILURE,
            NIMCP_ALERT_HIGH,
            0,
            msg
        );
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "run_ipc_check: operation failed");
        return false;
    }

    return true;
}

/**
 * @brief Run anomaly detection check
 */
static bool run_anomaly_check(nimcp_continuous_monitor_t* monitor)
{
    if (!monitor || !monitor->coverage)
        return true;

    // Basic anomaly detection: check if coverage has degraded
    float coverage = nimcp_coverage_get_percentage(monitor->coverage);

    if (coverage < 100.0F) {
        char msg[NIMCP_LABEL_BUFFER_SIZE];
        snprintf(msg, sizeof(msg),
                 "Coverage degraded to %.1f%% - security gaps detected",
                 coverage);
        queue_alert(
            monitor,
            NIMCP_ALERT_TYPE_COVERAGE_DEGRADED,
            NIMCP_ALERT_WARNING,
            0,
            msg
        );
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "run_anomaly_check: operation failed");
        return false;
    }

    return true;
}

/**
 * @brief Run heartbeat check
 */
static bool run_heartbeat_check(nimcp_continuous_monitor_t* monitor)
{
    if (!monitor || !monitor->coverage)
        return true;

    // Record heartbeat for temporal coverage proof
    nimcp_coverage_heartbeat(monitor->coverage);

    // Check for temporal gaps
    bool no_gaps = nimcp_coverage_check_temporal(
        monitor->coverage,
        monitor->config.interval_ms * 2  // Allow 2x interval as max gap
    );

    if (!no_gaps) {
        queue_alert(
            monitor,
            NIMCP_ALERT_TYPE_TEMPORAL_GAP,
            NIMCP_ALERT_WARNING,
            0,
            "Temporal coverage gap detected - monitoring interruption"
        );
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "run_heartbeat_check: no_gaps is NULL");
        return false;
    }

    return true;
}

/**
 * @brief Monitoring thread function
 */
static void* monitor_thread_func(void* arg)
{
    nimcp_continuous_monitor_t* monitor = (nimcp_continuous_monitor_t*)arg;

    if (!monitor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "monitor_thread_func: monitor is NULL");
        return NULL;
    }

    while (!monitor->stop_requested) {
        uint64_t cycle_start = get_timestamp_ms();

        nimcp_mutex_lock(&monitor->state_mutex);
        nimcp_monitor_state_t state = monitor->state;
        nimcp_mutex_unlock(&monitor->state_mutex);

        if (state == NIMCP_MONITOR_STATE_RUNNING) {
            // Run monitoring checks
            if (monitor->config.memory_check_enabled) {
                run_memory_check(monitor);
            }

            if (monitor->config.cfi_check_enabled) {
                run_cfi_check(monitor);
            }

            if (monitor->config.ipc_check_enabled) {
                run_ipc_check(monitor);
            }

            if (monitor->config.anomaly_check_enabled) {
                run_anomaly_check(monitor);
            }

            // Always run heartbeat
            run_heartbeat_check(monitor);

            // Update statistics
            uint64_t cycle_end = get_timestamp_ms();
            uint64_t cycle_duration = cycle_end - cycle_start;

            nimcp_mutex_lock(&monitor->stats_mutex);
            monitor->stats.cycles_completed++;
            monitor->stats.checks_performed += 5;  // 5 check types
            monitor->stats.last_cycle_time = cycle_end;
            monitor->total_cycle_time += cycle_duration;

            if (cycle_duration > monitor->stats.max_cycle_duration_ms) {
                monitor->stats.max_cycle_duration_ms = cycle_duration;
            }

            if (monitor->stats.cycles_completed > 0) {
                monitor->stats.avg_cycle_duration_ms =
                    monitor->total_cycle_time / monitor->stats.cycles_completed;
            }

            monitor->stats.uptime_ms = cycle_end - monitor->start_time;
            nimcp_mutex_unlock(&monitor->stats_mutex);
        }

        // Sleep for configured interval
        struct timespec ts;
        ts.tv_sec = monitor->config.interval_ms / NIMCP_MS_PER_SEC;
        ts.tv_nsec = (monitor->config.interval_ms % NIMCP_MS_PER_SEC) * NIMCP_NS_PER_MS;
        nanosleep(&ts, NULL);
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "monitor_thread_func: operation failed");
    return NULL;
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

nimcp_continuous_monitor_t* nimcp_monitor_create(
    nimcp_security_coverage_t* coverage)
{
    nimcp_continuous_monitor_t* monitor =
        (nimcp_continuous_monitor_t*)nimcp_calloc(1, sizeof(nimcp_continuous_monitor_t));

    if (!monitor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_monitor_create: failed to allocate monitor");
        return NULL;
    }

    monitor->coverage = coverage;
    monitor->state = NIMCP_MONITOR_STATE_STOPPED;
    monitor->initialized = false;

    // Initialize mutexes
    nimcp_mutex_init(&monitor->state_mutex, NULL);
    nimcp_mutex_init(&monitor->alert_mutex, NULL);
    nimcp_mutex_init(&monitor->callback_mutex, NULL);
    nimcp_mutex_init(&monitor->stats_mutex, NULL);

    return monitor;
}

nimcp_result_t nimcp_monitor_init(
    nimcp_continuous_monitor_t* monitor,
    const nimcp_monitor_config_t* config)
{
    NIMCP_API_CHECK_NULL(monitor, NIMCP_INVALID_PARAM, "NULL monitor in init");

    if (monitor->initialized)
        return NIMCP_INVALID_STATE;

    // Copy configuration
    if (config) {
        memcpy(&monitor->config, config, sizeof(nimcp_monitor_config_t));
    } else {
        nimcp_monitor_get_default_config(&monitor->config);
    }

    // Validate interval
    if (monitor->config.interval_ms < NIMCP_MONITOR_MIN_INTERVAL_MS) {
        monitor->config.interval_ms = NIMCP_MONITOR_MIN_INTERVAL_MS;
    }
    if (monitor->config.interval_ms > NIMCP_MONITOR_MAX_INTERVAL_MS) {
        monitor->config.interval_ms = NIMCP_MONITOR_MAX_INTERVAL_MS;
    }

    monitor->start_time = get_timestamp_ms();
    monitor->initialized = true;

    nimcp_security_log_event(
        NIMCP_SECURITY_EVENT_DIRECTIVE_VERIFIED,
        NIMCP_THREAT_NONE,
        "Continuous security monitor initialized"
    );

    return NIMCP_SUCCESS;
}

void nimcp_monitor_destroy(nimcp_continuous_monitor_t* monitor)
{
    if (!monitor)
        return;

    // Stop monitoring if running
    if (monitor->state == NIMCP_MONITOR_STATE_RUNNING ||
        monitor->state == NIMCP_MONITOR_STATE_PAUSED) {
        nimcp_monitor_stop(monitor);
    }

    // Destroy mutexes
    nimcp_mutex_destroy(&monitor->state_mutex);
    nimcp_mutex_destroy(&monitor->alert_mutex);
    nimcp_mutex_destroy(&monitor->callback_mutex);
    nimcp_mutex_destroy(&monitor->stats_mutex);

    memset(monitor, 0, sizeof(nimcp_continuous_monitor_t));
    nimcp_free(monitor);
}

//=============================================================================
// Monitor Control
//=============================================================================

nimcp_result_t nimcp_monitor_start(nimcp_continuous_monitor_t* monitor)
{
    if (!monitor || !monitor->initialized)
        return NIMCP_INVALID_PARAM;

    nimcp_mutex_lock(&monitor->state_mutex);

    if (monitor->state != NIMCP_MONITOR_STATE_STOPPED) {
        nimcp_mutex_unlock(&monitor->state_mutex);
        return NIMCP_INVALID_STATE;
    }

    monitor->state = NIMCP_MONITOR_STATE_STARTING;
    monitor->stop_requested = false;
    monitor->thread_running = true;

    nimcp_mutex_unlock(&monitor->state_mutex);

    // Create monitoring thread
    int result = nimcp_thread_create(
        &monitor->monitor_thread,
        monitor_thread_func,
        monitor,
        NULL
    );

    if (result != 0) {
        nimcp_mutex_lock(&monitor->state_mutex);
        monitor->state = NIMCP_MONITOR_STATE_ERROR;
        monitor->thread_running = false;
        nimcp_mutex_unlock(&monitor->state_mutex);
        return NIMCP_ERROR;
    }

    nimcp_mutex_lock(&monitor->state_mutex);
    monitor->state = NIMCP_MONITOR_STATE_RUNNING;
    nimcp_mutex_unlock(&monitor->state_mutex);

    nimcp_security_log_event(
        NIMCP_SECURITY_EVENT_DIRECTIVE_VERIFIED,
        NIMCP_THREAT_NONE,
        "Continuous security monitoring started"
    );

    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_monitor_stop(nimcp_continuous_monitor_t* monitor)
{
    if (!monitor)
        return NIMCP_INVALID_PARAM;

    nimcp_mutex_lock(&monitor->state_mutex);

    if (monitor->state != NIMCP_MONITOR_STATE_RUNNING &&
        monitor->state != NIMCP_MONITOR_STATE_PAUSED) {
        nimcp_mutex_unlock(&monitor->state_mutex);
        return NIMCP_INVALID_STATE;
    }

    monitor->state = NIMCP_MONITOR_STATE_STOPPING;
    monitor->stop_requested = true;

    nimcp_mutex_unlock(&monitor->state_mutex);

    // Wait for thread to finish
    if (monitor->thread_running) {
        nimcp_thread_join(monitor->monitor_thread, NULL);
        monitor->thread_running = false;
    }

    nimcp_mutex_lock(&monitor->state_mutex);
    monitor->state = NIMCP_MONITOR_STATE_STOPPED;
    nimcp_mutex_unlock(&monitor->state_mutex);

    nimcp_security_log_event(
        NIMCP_SECURITY_EVENT_DIRECTIVE_VERIFIED,
        NIMCP_THREAT_NONE,
        "Continuous security monitoring stopped"
    );

    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_monitor_pause(nimcp_continuous_monitor_t* monitor)
{
    if (!monitor)
        return NIMCP_INVALID_PARAM;

    nimcp_mutex_lock(&monitor->state_mutex);

    if (monitor->state != NIMCP_MONITOR_STATE_RUNNING) {
        nimcp_mutex_unlock(&monitor->state_mutex);
        return NIMCP_INVALID_STATE;
    }

    monitor->state = NIMCP_MONITOR_STATE_PAUSED;
    nimcp_mutex_unlock(&monitor->state_mutex);

    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_monitor_resume(nimcp_continuous_monitor_t* monitor)
{
    if (!monitor)
        return NIMCP_INVALID_PARAM;

    nimcp_mutex_lock(&monitor->state_mutex);

    if (monitor->state != NIMCP_MONITOR_STATE_PAUSED) {
        nimcp_mutex_unlock(&monitor->state_mutex);
        return NIMCP_INVALID_STATE;
    }

    monitor->state = NIMCP_MONITOR_STATE_RUNNING;
    nimcp_mutex_unlock(&monitor->state_mutex);

    return NIMCP_SUCCESS;
}

nimcp_monitor_state_t nimcp_monitor_get_state(nimcp_continuous_monitor_t* monitor)
{
    if (!monitor)
        return NIMCP_MONITOR_STATE_ERROR;

    nimcp_mutex_lock(&monitor->state_mutex);
    nimcp_monitor_state_t state = monitor->state;
    nimcp_mutex_unlock(&monitor->state_mutex);

    return state;
}

//=============================================================================
// Manual Checks
//=============================================================================

nimcp_result_t nimcp_monitor_run_cycle(nimcp_continuous_monitor_t* monitor)
{
    if (!monitor || !monitor->initialized)
        return NIMCP_INVALID_PARAM;

    run_memory_check(monitor);
    run_cfi_check(monitor);
    run_ipc_check(monitor);
    run_anomaly_check(monitor);
    run_heartbeat_check(monitor);

    nimcp_mutex_lock(&monitor->stats_mutex);
    monitor->stats.cycles_completed++;
    monitor->stats.checks_performed += 5;
    nimcp_mutex_unlock(&monitor->stats_mutex);

    return NIMCP_SUCCESS;
}

bool nimcp_monitor_run_check(
    nimcp_continuous_monitor_t* monitor,
    nimcp_check_type_t check_type)
{
    if (!monitor || !monitor->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_monitor_run_check: required parameter is NULL (monitor, monitor->initialized)");
        return false;
    }

    bool result = true;

    switch (check_type) {
        case NIMCP_CHECK_MEMORY_INTEGRITY:
            result = run_memory_check(monitor);
            break;
        case NIMCP_CHECK_CFI_VALIDATION:
            result = run_cfi_check(monitor);
            break;
        case NIMCP_CHECK_IPC_AUTHENTICATION:
            result = run_ipc_check(monitor);
            break;
        case NIMCP_CHECK_COVERAGE_STATUS:
        case NIMCP_CHECK_ANOMALY_DETECTION:
            result = run_anomaly_check(monitor);
            break;
        case NIMCP_CHECK_HEARTBEAT:
            result = run_heartbeat_check(monitor);
            break;
        case NIMCP_CHECK_ALL:
            result = run_memory_check(monitor) &&
                     run_cfi_check(monitor) &&
                     run_ipc_check(monitor) &&
                     run_anomaly_check(monitor) &&
                     run_heartbeat_check(monitor);
            break;
    }

    nimcp_mutex_lock(&monitor->stats_mutex);
    monitor->stats.checks_performed++;
    nimcp_mutex_unlock(&monitor->stats_mutex);

    return result;
}

//=============================================================================
// Alert Management
//=============================================================================

int32_t nimcp_monitor_register_callback(
    nimcp_continuous_monitor_t* monitor,
    nimcp_alert_callback_t callback,
    void* user_data)
{
    if (!monitor || !callback) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_monitor_register_callback: required parameter is NULL (monitor, callback)");
        return -1;
    }

    nimcp_mutex_lock(&monitor->callback_mutex);

    // Find available slot
    int32_t slot = -1;
    for (uint32_t i = 0; i < NIMCP_MONITOR_MAX_CALLBACKS; i++) {
        if (!monitor->callbacks[i].active) {
            slot = (int32_t)i;
            break;
        }
    }

    if (slot >= 0) {
        monitor->callbacks[slot].callback = callback;
        monitor->callbacks[slot].user_data = user_data;
        monitor->callbacks[slot].active = true;
        monitor->num_callbacks++;
    }

    nimcp_mutex_unlock(&monitor->callback_mutex);

    return slot;
}

nimcp_result_t nimcp_monitor_unregister_callback(
    nimcp_continuous_monitor_t* monitor,
    int32_t callback_id)
{
    if (!monitor || callback_id < 0 ||
        (uint32_t)callback_id >= NIMCP_MONITOR_MAX_CALLBACKS)
        return NIMCP_INVALID_PARAM;

    nimcp_mutex_lock(&monitor->callback_mutex);

    if (!monitor->callbacks[callback_id].active) {
        nimcp_mutex_unlock(&monitor->callback_mutex);
        return NIMCP_INVALID_STATE;
    }

    monitor->callbacks[callback_id].active = false;
    monitor->callbacks[callback_id].callback = NULL;
    monitor->callbacks[callback_id].user_data = NULL;
    monitor->num_callbacks--;

    nimcp_mutex_unlock(&monitor->callback_mutex);

    return NIMCP_SUCCESS;
}

bool nimcp_monitor_get_alert(
    nimcp_continuous_monitor_t* monitor,
    nimcp_alert_t* alert)
{
    if (!monitor || !alert) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_monitor_get_alert: required parameter is NULL (monitor, alert)");
        return false;
    }

    nimcp_mutex_lock(&monitor->alert_mutex);

    if (monitor->alert_count == 0) {
        nimcp_mutex_unlock(&monitor->alert_mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_monitor_get_alert: monitor->alert_count is zero");
        return false;
    }

    // Get alert from head
    memcpy(alert, &monitor->alert_queue[monitor->alert_head].alert, sizeof(nimcp_alert_t));
    monitor->alert_queue[monitor->alert_head].used = false;
    monitor->alert_head = (monitor->alert_head + 1) % NIMCP_MONITOR_MAX_ALERTS;
    monitor->alert_count--;

    nimcp_mutex_unlock(&monitor->alert_mutex);

    return true;
}

nimcp_result_t nimcp_monitor_acknowledge_alert(
    nimcp_continuous_monitor_t* monitor,
    const nimcp_alert_t* alert)
{
    if (!monitor || !alert)
        return NIMCP_INVALID_PARAM;

    nimcp_mutex_lock(&monitor->stats_mutex);
    monitor->stats.alerts_acknowledged++;
    nimcp_mutex_unlock(&monitor->stats_mutex);

    return NIMCP_SUCCESS;
}

uint32_t nimcp_monitor_get_alert_count(nimcp_continuous_monitor_t* monitor)
{
    if (!monitor)
        return 0;

    nimcp_mutex_lock(&monitor->alert_mutex);
    uint32_t count = monitor->alert_count;
    nimcp_mutex_unlock(&monitor->alert_mutex);

    return count;
}

nimcp_result_t nimcp_monitor_clear_alerts(nimcp_continuous_monitor_t* monitor)
{
    if (!monitor)
        return NIMCP_INVALID_PARAM;

    nimcp_mutex_lock(&monitor->alert_mutex);

    memset(monitor->alert_queue, 0, sizeof(monitor->alert_queue));
    monitor->alert_head = 0;
    monitor->alert_tail = 0;
    monitor->alert_count = 0;

    nimcp_mutex_unlock(&monitor->alert_mutex);

    return NIMCP_SUCCESS;
}

//=============================================================================
// Statistics and Reporting
//=============================================================================

nimcp_result_t nimcp_monitor_get_stats(
    nimcp_continuous_monitor_t* monitor,
    nimcp_monitor_stats_t* stats)
{
    if (!monitor || !stats)
        return NIMCP_INVALID_PARAM;

    nimcp_mutex_lock(&monitor->stats_mutex);
    memcpy(stats, &monitor->stats, sizeof(nimcp_monitor_stats_t));
    stats->uptime_ms = get_timestamp_ms() - monitor->start_time;
    nimcp_mutex_unlock(&monitor->stats_mutex);

    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_monitor_reset_stats(nimcp_continuous_monitor_t* monitor)
{
    if (!monitor)
        return NIMCP_INVALID_PARAM;

    nimcp_mutex_lock(&monitor->stats_mutex);
    memset(&monitor->stats, 0, sizeof(nimcp_monitor_stats_t));
    monitor->total_cycle_time = 0;
    nimcp_mutex_unlock(&monitor->stats_mutex);

    return NIMCP_SUCCESS;
}

int32_t nimcp_monitor_generate_report(
    nimcp_continuous_monitor_t* monitor,
    char* buffer,
    size_t buffer_size)
{
    if (!monitor || !buffer || buffer_size < 256) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_monitor_generate_report: required parameter is NULL (monitor, buffer)");
        return -1;
    }

    nimcp_monitor_stats_t stats;
    nimcp_monitor_get_stats(monitor, &stats);

    int written = snprintf(buffer, buffer_size,
        "=== Continuous Security Monitor Report ===\n"
        "State: %s\n"
        "Uptime: %lu ms\n"
        "Cycles Completed: %lu\n"
        "Checks Performed: %lu\n"
        "Violations Detected: %lu\n"
        "Alerts Generated: %lu\n"
        "Alerts Acknowledged: %lu\n"
        "Pending Alerts: %u\n"
        "Max Cycle Duration: %lu ms\n"
        "Avg Cycle Duration: %lu ms\n",
        nimcp_monitor_state_name(nimcp_monitor_get_state(monitor)),
        (unsigned long)stats.uptime_ms,
        (unsigned long)stats.cycles_completed,
        (unsigned long)stats.checks_performed,
        (unsigned long)stats.violations_detected,
        (unsigned long)stats.alerts_generated,
        (unsigned long)stats.alerts_acknowledged,
        nimcp_monitor_get_alert_count(monitor),
        (unsigned long)stats.max_cycle_duration_ms,
        (unsigned long)stats.avg_cycle_duration_ms
    );

    return written;
}

//=============================================================================
// Configuration
//=============================================================================

nimcp_result_t nimcp_monitor_set_interval(
    nimcp_continuous_monitor_t* monitor,
    uint32_t interval_ms)
{
    if (!monitor)
        return NIMCP_INVALID_PARAM;

    if (interval_ms < NIMCP_MONITOR_MIN_INTERVAL_MS ||
        interval_ms > NIMCP_MONITOR_MAX_INTERVAL_MS)
        return NIMCP_INVALID_PARAM;

    monitor->config.interval_ms = interval_ms;

    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_monitor_set_check_enabled(
    nimcp_continuous_monitor_t* monitor,
    nimcp_check_type_t check_type,
    bool enabled)
{
    if (!monitor)
        return NIMCP_INVALID_PARAM;

    switch (check_type) {
        case NIMCP_CHECK_MEMORY_INTEGRITY:
            monitor->config.memory_check_enabled = enabled;
            break;
        case NIMCP_CHECK_CFI_VALIDATION:
            monitor->config.cfi_check_enabled = enabled;
            break;
        case NIMCP_CHECK_IPC_AUTHENTICATION:
            monitor->config.ipc_check_enabled = enabled;
            break;
        case NIMCP_CHECK_ANOMALY_DETECTION:
        case NIMCP_CHECK_COVERAGE_STATUS:
            monitor->config.anomaly_check_enabled = enabled;
            break;
        case NIMCP_CHECK_HEARTBEAT:
        case NIMCP_CHECK_ALL:
            // Heartbeat is always enabled
            break;
    }

    return NIMCP_SUCCESS;
}

void nimcp_monitor_get_default_config(nimcp_monitor_config_t* config)
{
    if (!config)
        return;

    config->interval_ms = NIMCP_MONITOR_DEFAULT_INTERVAL_MS;
    config->memory_check_enabled = true;
    config->cfi_check_enabled = true;
    config->ipc_check_enabled = true;
    config->anomaly_check_enabled = true;
    config->auto_respond = false;
    config->alert_threshold = NIMCP_ALERT_WARNING;
}

//=============================================================================
// Utility Functions
//=============================================================================

const char* nimcp_alert_type_name(nimcp_alert_type_t type)
{
    static const char* names[] = {
        "Memory Violation",
        "CFI Violation",
        "Auth Failure",
        "Anomaly Detected",
        "Temporal Gap",
        "Coverage Degraded",
        "Rate Exceeded",
        "Unknown"
    };

    if (type > NIMCP_ALERT_TYPE_UNKNOWN)
        return "Invalid";

    return names[type];
}

const char* nimcp_alert_severity_name(nimcp_alert_severity_t severity)
{
    static const char* names[] = {
        "Info",
        "Warning",
        "High",
        "Critical",
        "Emergency"
    };

    if (severity > NIMCP_ALERT_EMERGENCY)
        return "Invalid";

    return names[severity];
}

const char* nimcp_monitor_state_name(nimcp_monitor_state_t state)
{
    static const char* names[] = {
        "Stopped",
        "Starting",
        "Running",
        "Paused",
        "Stopping",
        "Error"
    };

    if (state > NIMCP_MONITOR_STATE_ERROR)
        return "Invalid";

    return names[state];
}
