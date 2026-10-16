/**
 * @file nimcp_self_repair_health_notify.h
 * @brief Self-Repair Health Agent Notification Extension
 * @version 1.0.0
 * @date 2025-01-20
 *
 * WHAT: Extends self-repair coordinator with health agent notification
 * WHY:  Escalate repair failures to health monitoring for intervention
 * HOW:  Callback mechanism to notify health agent of failures
 *
 * ARCHITECTURE:
 * ```
 * ┌───────────────────────────────────────────────────────────────────────────┐
 * │              SELF-REPAIR HEALTH NOTIFICATION INTEGRATION                  │
 * ├───────────────────────────────────────────────────────────────────────────┤
 * │                                                                            │
 * │   ┌─────────────────┐     ┌───────────────────────┐     ┌──────────────┐  │
 * │   │  Self-Repair    │     │  Health Notification  │     │   Health     │  │
 * │   │  Coordinator    │────>│      Bridge           │────>│   Agent      │  │
 * │   └─────────────────┘     └───────────────────────┘     └──────────────┘  │
 * │                                                                            │
 * │   NOTIFICATION TRIGGERS:                                                   │
 * │   - Repair failure                                                         │
 * │   - Rollback event                                                         │
 * │   - High-risk repair                                                       │
 * │   - Repeated failures for same issue                                       │
 * │                                                                            │
 * │   NOTIFICATION CONTENT:                                                    │
 * │   - Repair ID and status                                                   │
 * │   - Original diagnostic info                                               │
 * │   - Failure details                                                        │
 * │   - Suggested intervention                                                 │
 * │                                                                            │
 * └───────────────────────────────────────────────────────────────────────────┘
 * ```
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_SELF_REPAIR_HEALTH_NOTIFY_H
#define NIMCP_SELF_REPAIR_HEALTH_NOTIFY_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Dependencies */
#include "cognitive/fault_tolerance/nimcp_self_repair.h"

/* Forward declaration to avoid circular includes */
struct nimcp_health_agent;
typedef struct nimcp_health_agent nimcp_health_agent_t;

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define SELF_REPAIR_HEALTH_NOTIFY_VERSION    "1.0.0"
#define SELF_REPAIR_HEALTH_NOTIFY_MAGIC      0x53524E48  /**< 'SRNH' */

/* ============================================================================
 * Notification Types
 * ============================================================================ */

/**
 * @brief Types of health notifications from self-repair
 */
typedef enum {
    REPAIR_NOTIFY_FAILURE = 0,          /**< Repair failed */
    REPAIR_NOTIFY_ROLLBACK,             /**< Repair was rolled back */
    REPAIR_NOTIFY_HIGH_RISK,            /**< High-risk repair attempted */
    REPAIR_NOTIFY_REPEATED_FAILURE,     /**< Multiple failures for same issue */
    REPAIR_NOTIFY_TIMEOUT,              /**< Repair timed out */
    REPAIR_NOTIFY_VALIDATION_FAILED,    /**< Fix failed validation */
    REPAIR_NOTIFY_SUCCESS               /**< Repair succeeded (optional) */
} repair_notify_type_t;

/**
 * @brief Suggested intervention for health agent
 */
typedef enum {
    REPAIR_INTERVENTION_NONE = 0,       /**< No intervention needed */
    REPAIR_INTERVENTION_LOG_ONLY,       /**< Just log the event */
    REPAIR_INTERVENTION_ALERT,          /**< Send alert to operators */
    REPAIR_INTERVENTION_QUARANTINE,     /**< Quarantine affected component */
    REPAIR_INTERVENTION_ROLLBACK,       /**< Trigger system rollback */
    REPAIR_INTERVENTION_MANUAL_REPAIR,  /**< Request manual repair */
    REPAIR_INTERVENTION_RESTART         /**< Restart affected service */
} repair_intervention_t;

/* ============================================================================
 * Configuration
 * ============================================================================ */

/**
 * @brief Health notification configuration
 */
typedef struct {
    bool notify_on_failure;             /**< Notify on repair failure */
    bool notify_on_rollback;            /**< Notify on rollback events */
    bool notify_on_high_risk;           /**< Notify on high-risk repairs */
    bool notify_on_timeout;             /**< Notify on repair timeouts */
    bool notify_on_success;             /**< Notify on success (for tracking) */

    /* Repeated failure tracking */
    bool track_repeated_failures;       /**< Track repeated failures */
    uint32_t repeated_failure_threshold; /**< Failures before escalating */
    uint32_t repeated_failure_window_ms; /**< Time window for counting */

    /* Risk thresholds */
    float high_risk_threshold;          /**< Risk score to trigger notification */

    /* Bio-async integration */
    bool enable_bio_async;              /**< Enable bio-async messaging */

    /* Logging */
    bool verbose_logging;               /**< Enable verbose output */
} self_repair_health_notify_config_t;

/* ============================================================================
 * Notification Message
 * ============================================================================ */

/**
 * @brief Health notification message from self-repair
 */
typedef struct {
    repair_notify_type_t type;          /**< Notification type */
    repair_intervention_t intervention; /**< Suggested intervention */

    /* Repair info */
    uint64_t repair_id;                 /**< Repair ID */
    repair_status_t status;             /**< Repair status */
    repair_stage_t stage;               /**< Stage where failure occurred */

    /* Original diagnostic info */
    uint64_t diagnostic_id;             /**< Original diagnostic ID */
    error_type_t error_type;            /**< Original error type */
    diag_severity_t severity;           /**< Original severity */

    /* Failure details */
    char error_message[256];            /**< Error message */
    char source_file[512];              /**< Affected source file */
    char function_name[128];            /**< Affected function */

    /* Context */
    uint32_t failure_count;             /**< Consecutive failures for issue */
    float risk_score;                   /**< Repair risk score */
    float confidence;                   /**< Fix confidence */

    /* Timing */
    uint64_t repair_start_time;         /**< When repair started (ms) */
    uint64_t notification_time;         /**< When notification created */
    uint64_t repair_duration_ms;        /**< Total repair duration */
} self_repair_health_notification_t;

/* ============================================================================
 * Statistics
 * ============================================================================ */

/**
 * @brief Health notification statistics
 */
typedef struct {
    uint64_t notifications_sent;        /**< Total notifications sent */
    uint64_t failures_notified;         /**< Failure notifications */
    uint64_t rollbacks_notified;        /**< Rollback notifications */
    uint64_t high_risk_notified;        /**< High-risk notifications */
    uint64_t repeated_failures_notified; /**< Repeated failure escalations */
    uint64_t by_type[7];                /**< Count by notification type */
    uint64_t by_intervention[7];        /**< Count by intervention type */
} self_repair_health_notify_stats_t;

/* ============================================================================
 * Callback Types
 * ============================================================================ */

/**
 * @brief Callback for failure notification (custom handler)
 */
typedef void (*self_repair_failure_cb_t)(
    const self_repair_health_notification_t* notification,
    void* user_data
);

/* ============================================================================
 * Bridge Handle
 * ============================================================================ */

/**
 * @brief Opaque health notification bridge handle
 */
typedef struct self_repair_health_notify_bridge self_repair_health_notify_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default notification configuration
 *
 * WHAT: Initialize configuration with sensible defaults
 * WHY:  Easy setup with good starting values
 * HOW:  Return struct with balanced parameters
 *
 * DEFAULTS:
 * - notify_on_failure: true
 * - notify_on_rollback: true
 * - notify_on_high_risk: true
 * - repeated_failure_threshold: 3
 * - high_risk_threshold: 0.5
 *
 * @param config Output configuration (non-NULL)
 * @return 0 on success, -1 on error
 */
int self_repair_health_notify_default_config(
    self_repair_health_notify_config_t* config
);

/**
 * @brief Create health notification bridge
 *
 * WHAT: Initialize bridge for self-repair to health agent notifications
 * WHY:  Enable escalation of repair failures
 * HOW:  Allocate bridge, initialize tracking
 *
 * @param config Configuration (NULL for defaults)
 * @param self_repair Self-repair coordinator (required)
 * @param health_agent Health agent to notify (can be NULL, set later)
 * @return Bridge handle or NULL on failure
 */
self_repair_health_notify_bridge_t* self_repair_health_notify_create(
    const self_repair_health_notify_config_t* config,
    self_repair_coordinator_t* self_repair,
    nimcp_health_agent_t* health_agent
);

/**
 * @brief Destroy health notification bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Disconnect callbacks, free structures
 *
 * @param bridge Bridge handle (NULL safe)
 */
void self_repair_health_notify_destroy(
    self_repair_health_notify_bridge_t* bridge
);

/**
 * @brief Connect to health agent
 *
 * WHAT: Set or change the health agent to notify
 * WHY:  Allow late binding or agent changes
 * HOW:  Store reference for notification delivery
 *
 * @param bridge Bridge handle
 * @param health_agent Health agent (NULL to disconnect)
 * @return 0 on success, -1 on error
 */
int self_repair_health_notify_connect(
    self_repair_health_notify_bridge_t* bridge,
    nimcp_health_agent_t* health_agent
);

/* ============================================================================
 * Callback Registration
 * ============================================================================ */

/**
 * @brief Set custom failure callback
 *
 * WHAT: Register custom handler for repair failures
 * WHY:  Allow custom processing beyond health agent
 * HOW:  Callback invoked on each notification
 *
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data for callback
 * @return 0 on success, -1 on error
 */
int self_repair_health_notify_set_callback(
    self_repair_health_notify_bridge_t* bridge,
    self_repair_failure_cb_t callback,
    void* user_data
);

/* ============================================================================
 * Notification API
 * ============================================================================ */

/**
 * @brief Notify of repair result
 *
 * WHAT: Process repair result and notify if needed
 * WHY:  Main entry point for repair outcome handling
 * HOW:  Evaluate result, create notification if criteria met
 *
 * @param bridge Bridge handle
 * @param repair_id Repair ID
 * @param result Repair result
 * @return 0 on success, -1 on error
 */
int self_repair_health_notify_result(
    self_repair_health_notify_bridge_t* bridge,
    uint64_t repair_id,
    const self_repair_result_t* result
);

/**
 * @brief Send explicit notification
 *
 * WHAT: Send custom notification to health agent
 * WHY:  Allow manual escalation
 * HOW:  Construct and deliver notification
 *
 * @param bridge Bridge handle
 * @param notification Notification to send
 * @return 0 on success, -1 on error
 */
int self_repair_health_notify_send(
    self_repair_health_notify_bridge_t* bridge,
    const self_repair_health_notification_t* notification
);

/**
 * @brief Determine intervention for repair failure
 *
 * WHAT: Suggest appropriate intervention for failure
 * WHY:  Guide health agent response
 * HOW:  Analyze failure context and history
 *
 * @param bridge Bridge handle
 * @param result Repair result
 * @return Suggested intervention
 */
repair_intervention_t self_repair_suggest_intervention(
    const self_repair_health_notify_bridge_t* bridge,
    const self_repair_result_t* result
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get notification statistics
 *
 * @param bridge Bridge handle
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int self_repair_health_notify_get_stats(
    const self_repair_health_notify_bridge_t* bridge,
    self_repair_health_notify_stats_t* stats
);

/**
 * @brief Reset notification statistics
 *
 * @param bridge Bridge handle
 */
void self_repair_health_notify_reset_stats(
    self_repair_health_notify_bridge_t* bridge
);

/**
 * @brief Check if bridge is ready
 *
 * @param bridge Bridge handle
 * @return true if ready, false otherwise
 */
bool self_repair_health_notify_is_ready(
    const self_repair_health_notify_bridge_t* bridge
);

/* ============================================================================
 * Bio-Async Integration
 * ============================================================================ */

/**
 * @brief Broadcast failure notification via bio-async
 *
 * @param bridge Bridge handle
 * @param notification Notification to broadcast
 * @return 0 on success, -1 on error
 */
int self_repair_health_notify_broadcast(
    self_repair_health_notify_bridge_t* bridge,
    const self_repair_health_notification_t* notification
);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Get notification type name
 *
 * @param type Notification type
 * @return String name (static)
 */
const char* self_repair_notify_type_name(repair_notify_type_t type);

/**
 * @brief Get intervention name
 *
 * @param intervention Intervention type
 * @return String name (static)
 */
const char* self_repair_intervention_name(repair_intervention_t intervention);

/**
 * @brief Get bridge version string
 *
 * @return Version string
 */
const char* self_repair_health_notify_version(void);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SELF_REPAIR_HEALTH_NOTIFY_H */
