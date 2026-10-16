/**
 * @file nimcp_continuous_monitor.h
 * @brief Continuous Security Monitoring - No gaps in time protection
 *
 * WHAT: Provides continuous real-time security monitoring with no temporal gaps.
 *       Runs as a background monitoring system that continuously verifies
 *       system integrity across all security dimensions.
 *
 * WHY:  Attackers exploit temporal windows when security systems are inactive.
 *       This module ensures monitoring is ALWAYS active with no gaps.
 *
 * HOW:  Background thread performs continuous verification cycles:
 *       - Memory integrity checks
 *       - CFI validation
 *       - IPC authentication monitoring
 *       - Anomaly detection
 *       - Heartbeat generation for temporal proof
 *
 * Part of Phase SC-1: Security Coverage Framework (Tier 0.7)
 */

#ifndef NIMCP_CONTINUOUS_MONITOR_H
#define NIMCP_CONTINUOUS_MONITOR_H

#include "utils/validation/nimcp_common.h"
#include "security/nimcp_security_coverage.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** Maximum monitoring interval in milliseconds */
#define NIMCP_MONITOR_MAX_INTERVAL_MS 1000

/** Minimum monitoring interval in milliseconds */
#define NIMCP_MONITOR_MIN_INTERVAL_MS 10

/** Default monitoring interval in milliseconds */
#define NIMCP_MONITOR_DEFAULT_INTERVAL_MS 100

/** Maximum number of monitoring callbacks */
#define NIMCP_MONITOR_MAX_CALLBACKS 32

/** Maximum alert queue size */
#define NIMCP_MONITOR_MAX_ALERTS 256

/** Alert severity threshold for immediate action */
#define NIMCP_ALERT_SEVERITY_THRESHOLD 3

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Monitor state
 */
typedef enum {
    NIMCP_MONITOR_STATE_STOPPED = 0,     /**< Not running */
    NIMCP_MONITOR_STATE_STARTING,        /**< Initializing */
    NIMCP_MONITOR_STATE_RUNNING,         /**< Active monitoring */
    NIMCP_MONITOR_STATE_PAUSED,          /**< Temporarily paused */
    NIMCP_MONITOR_STATE_STOPPING,        /**< Shutting down */
    NIMCP_MONITOR_STATE_ERROR            /**< Error state */
} nimcp_monitor_state_t;

/**
 * @brief Alert severity levels
 */
typedef enum {
    NIMCP_ALERT_INFO = 0,                /**< Informational */
    NIMCP_ALERT_WARNING,                 /**< Warning - investigate */
    NIMCP_ALERT_HIGH,                    /**< High - action needed */
    NIMCP_ALERT_CRITICAL,                /**< Critical - immediate action */
    NIMCP_ALERT_EMERGENCY                /**< Emergency - system at risk */
} nimcp_alert_severity_t;

/**
 * @brief Alert types
 */
typedef enum {
    NIMCP_ALERT_TYPE_MEMORY_VIOLATION = 0,
    NIMCP_ALERT_TYPE_CFI_VIOLATION,
    NIMCP_ALERT_TYPE_AUTH_FAILURE,
    NIMCP_ALERT_TYPE_ANOMALY_DETECTED,
    NIMCP_ALERT_TYPE_TEMPORAL_GAP,
    NIMCP_ALERT_TYPE_COVERAGE_DEGRADED,
    NIMCP_ALERT_TYPE_RATE_EXCEEDED,
    NIMCP_ALERT_TYPE_UNKNOWN
} nimcp_alert_type_t;

/**
 * @brief Monitoring check types
 */
typedef enum {
    NIMCP_CHECK_MEMORY_INTEGRITY,        /**< Verify memory hashes */
    NIMCP_CHECK_CFI_VALIDATION,          /**< Verify control flow */
    NIMCP_CHECK_IPC_AUTHENTICATION,      /**< Verify IPC auth */
    NIMCP_CHECK_COVERAGE_STATUS,         /**< Verify coverage levels */
    NIMCP_CHECK_ANOMALY_DETECTION,       /**< Statistical anomaly check */
    NIMCP_CHECK_HEARTBEAT,               /**< Temporal proof heartbeat */
    NIMCP_CHECK_ALL                      /**< Run all checks */
} nimcp_check_type_t;

//=============================================================================
// Data Structures
//=============================================================================

/**
 * @brief Security alert structure
 */
typedef struct {
    nimcp_alert_type_t type;             /**< Alert type */
    nimcp_alert_severity_t severity;     /**< Severity level */
    uint64_t timestamp;                  /**< When alert occurred */
    uint32_t source_id;                  /**< Source identifier */
    char message[256];                   /**< Human-readable message */
    void* context;                       /**< Optional context data */
    bool acknowledged;                   /**< Has been acknowledged */
} nimcp_alert_t;

/**
 * @brief Monitoring statistics
 */
typedef struct {
    uint64_t cycles_completed;           /**< Number of monitoring cycles */
    uint64_t checks_performed;           /**< Total checks performed */
    uint64_t violations_detected;        /**< Total violations found */
    uint64_t alerts_generated;           /**< Total alerts created */
    uint64_t alerts_acknowledged;        /**< Alerts acknowledged */
    uint64_t uptime_ms;                  /**< Monitoring uptime */
    uint64_t last_cycle_time;            /**< Last cycle timestamp */
    uint64_t max_cycle_duration_ms;      /**< Longest cycle duration */
    uint64_t avg_cycle_duration_ms;      /**< Average cycle duration */
    float cpu_usage_percent;             /**< Monitor CPU usage */
} nimcp_monitor_stats_t;

/**
 * @brief Monitoring configuration
 */
typedef struct {
    uint32_t interval_ms;                /**< Check interval */
    bool memory_check_enabled;           /**< Enable memory checks */
    bool cfi_check_enabled;              /**< Enable CFI checks */
    bool ipc_check_enabled;              /**< Enable IPC checks */
    bool anomaly_check_enabled;          /**< Enable anomaly detection */
    bool auto_respond;                   /**< Auto-respond to alerts */
    nimcp_alert_severity_t alert_threshold; /**< Min severity to report */
} nimcp_monitor_config_t;

/**
 * @brief Alert callback function type
 *
 * @param alert The alert that was generated
 * @param user_data User-provided context
 * @return true to acknowledge alert, false to keep active
 */
typedef bool (*nimcp_alert_callback_t)(const nimcp_alert_t* alert, void* user_data);

/**
 * @brief Continuous monitor context (opaque handle)
 */
typedef struct nimcp_continuous_monitor nimcp_continuous_monitor_t;

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create continuous monitor
 *
 * @param coverage Security coverage context to monitor
 * @return Monitor context or NULL on failure
 */
nimcp_continuous_monitor_t* nimcp_monitor_create(
    nimcp_security_coverage_t* coverage
);

/**
 * @brief Initialize monitor with configuration
 *
 * @param monitor Monitor context
 * @param config Monitoring configuration
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_monitor_init(
    nimcp_continuous_monitor_t* monitor,
    const nimcp_monitor_config_t* config
);

/**
 * @brief Destroy monitor and free resources
 *
 * @param monitor Monitor context
 */
void nimcp_monitor_destroy(nimcp_continuous_monitor_t* monitor);

//=============================================================================
// Monitor Control
//=============================================================================

/**
 * @brief Start continuous monitoring
 *
 * @param monitor Monitor context
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_monitor_start(nimcp_continuous_monitor_t* monitor);

/**
 * @brief Stop continuous monitoring
 *
 * @param monitor Monitor context
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_monitor_stop(nimcp_continuous_monitor_t* monitor);

/**
 * @brief Pause monitoring temporarily
 *
 * @param monitor Monitor context
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_monitor_pause(nimcp_continuous_monitor_t* monitor);

/**
 * @brief Resume paused monitoring
 *
 * @param monitor Monitor context
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_monitor_resume(nimcp_continuous_monitor_t* monitor);

/**
 * @brief Get current monitor state
 *
 * @param monitor Monitor context
 * @return Current state
 */
nimcp_monitor_state_t nimcp_monitor_get_state(
    nimcp_continuous_monitor_t* monitor
);

//=============================================================================
// Manual Checks
//=============================================================================

/**
 * @brief Run a single monitoring cycle manually
 *
 * @param monitor Monitor context
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_monitor_run_cycle(nimcp_continuous_monitor_t* monitor);

/**
 * @brief Run a specific check type
 *
 * @param monitor Monitor context
 * @param check_type Type of check to run
 * @return true if check passed, false if violation found
 */
bool nimcp_monitor_run_check(
    nimcp_continuous_monitor_t* monitor,
    nimcp_check_type_t check_type
);

//=============================================================================
// Alert Management
//=============================================================================

/**
 * @brief Register alert callback
 *
 * @param monitor Monitor context
 * @param callback Callback function
 * @param user_data User context passed to callback
 * @return Callback ID or -1 on failure
 */
int32_t nimcp_monitor_register_callback(
    nimcp_continuous_monitor_t* monitor,
    nimcp_alert_callback_t callback,
    void* user_data
);

/**
 * @brief Unregister alert callback
 *
 * @param monitor Monitor context
 * @param callback_id Callback ID from registration
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_monitor_unregister_callback(
    nimcp_continuous_monitor_t* monitor,
    int32_t callback_id
);

/**
 * @brief Get next pending alert
 *
 * @param monitor Monitor context
 * @param alert Output: alert data
 * @return true if alert available, false if queue empty
 */
bool nimcp_monitor_get_alert(
    nimcp_continuous_monitor_t* monitor,
    nimcp_alert_t* alert
);

/**
 * @brief Acknowledge an alert
 *
 * @param monitor Monitor context
 * @param alert Alert to acknowledge
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_monitor_acknowledge_alert(
    nimcp_continuous_monitor_t* monitor,
    const nimcp_alert_t* alert
);

/**
 * @brief Get count of pending alerts
 *
 * @param monitor Monitor context
 * @return Number of pending alerts
 */
uint32_t nimcp_monitor_get_alert_count(nimcp_continuous_monitor_t* monitor);

/**
 * @brief Clear all alerts
 *
 * @param monitor Monitor context
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_monitor_clear_alerts(nimcp_continuous_monitor_t* monitor);

//=============================================================================
// Statistics and Reporting
//=============================================================================

/**
 * @brief Get monitoring statistics
 *
 * @param monitor Monitor context
 * @param stats Output: statistics structure
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_monitor_get_stats(
    nimcp_continuous_monitor_t* monitor,
    nimcp_monitor_stats_t* stats
);

/**
 * @brief Reset monitoring statistics
 *
 * @param monitor Monitor context
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_monitor_reset_stats(nimcp_continuous_monitor_t* monitor);

/**
 * @brief Generate monitoring report
 *
 * @param monitor Monitor context
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @return Bytes written or -1 on error
 */
int32_t nimcp_monitor_generate_report(
    nimcp_continuous_monitor_t* monitor,
    char* buffer,
    size_t buffer_size
);

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Update monitoring interval
 *
 * @param monitor Monitor context
 * @param interval_ms New interval in milliseconds
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_monitor_set_interval(
    nimcp_continuous_monitor_t* monitor,
    uint32_t interval_ms
);

/**
 * @brief Enable/disable specific check type
 *
 * @param monitor Monitor context
 * @param check_type Check to configure
 * @param enabled Whether to enable
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_monitor_set_check_enabled(
    nimcp_continuous_monitor_t* monitor,
    nimcp_check_type_t check_type,
    bool enabled
);

/**
 * @brief Get default configuration
 *
 * @param config Output: default configuration
 */
void nimcp_monitor_get_default_config(nimcp_monitor_config_t* config);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get alert type name as string
 *
 * @param type Alert type
 * @return Type name string
 */
const char* nimcp_alert_type_name(nimcp_alert_type_t type);

/**
 * @brief Get alert severity name as string
 *
 * @param severity Severity level
 * @return Severity name string
 */
const char* nimcp_alert_severity_name(nimcp_alert_severity_t severity);

/**
 * @brief Get monitor state name as string
 *
 * @param state Monitor state
 * @return State name string
 */
const char* nimcp_monitor_state_name(nimcp_monitor_state_t state);

#ifdef __cplusplus
}
#endif

#endif  // NIMCP_CONTINUOUS_MONITOR_H
