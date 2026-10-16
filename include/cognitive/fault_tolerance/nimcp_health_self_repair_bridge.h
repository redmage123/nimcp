/**
 * @file nimcp_health_self_repair_bridge.h
 * @brief Health Detection to Self-Repair Automation Bridge
 * @version 1.0.0
 * @date 2025-01-20
 *
 * WHAT: Connects health detection systems to self-repair automation
 * WHY:  Enable autonomous fault detection and repair pipeline
 * HOW:  Auto-trigger on severity, rate limiting, outcome tracking
 *
 * ARCHITECTURE:
 * ```
 * ┌──────────────────────┐     ┌────────────────────────┐
 * │   Health Monitor     │────>│                        │
 * │     anomaly_t        │     │   Health Self-Repair   │
 * └──────────────────────┘     │        Bridge          │
 *                              │                        │
 * ┌──────────────────────┐     │  - Trigger Policy      │     ┌─────────────────┐
 * │   Health Agent       │────>│  - Rate Limiting       │────>│  Self-Repair    │
 * │     message_t        │     │  - Aggregation         │     │  Coordinator    │
 * └──────────────────────┘     │  - Outcome Tracking    │     └─────────────────┘
 *                              │  - Bio-Async           │
 * ┌──────────────────────┐     │                        │
 * │   Health Diagnostic  │────>│                        │
 * │      Bridge          │     └────────────────────────┘
 * └──────────────────────┘
 * ```
 *
 * TRIGGER POLICIES:
 * - MANUAL: Explicit API call only
 * - FATAL_ONLY: Only DIAG_SEVERITY_FATAL triggers repair
 * - CRITICAL: CRITICAL and FATAL trigger repair
 * - ERROR: ERROR, CRITICAL, FATAL trigger repair
 * - AUTO: Intelligent auto-trigger based on patterns
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_HEALTH_SELF_REPAIR_BRIDGE_H
#define NIMCP_HEALTH_SELF_REPAIR_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Dependencies */
#include "cognitive/fault_tolerance/nimcp_health_diagnostic_bridge.h"
#include "cognitive/fault_tolerance/nimcp_self_repair.h"
#include "utils/fault_tolerance/nimcp_health_agent.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define HEALTH_SELF_REPAIR_BRIDGE_VERSION    "1.0.0"
#define HEALTH_SELF_REPAIR_BRIDGE_MAGIC      0x48535242  /**< 'HSRB' */
#define HEALTH_SELF_REPAIR_MAX_PENDING       128         /**< Max pending repairs */
#define HEALTH_SELF_REPAIR_MAX_HISTORY       256         /**< Max outcome history */

/* ============================================================================
 * Trigger Policy
 * ============================================================================ */

/**
 * @brief Trigger policy for auto-repair
 */
typedef enum {
    HEALTH_TRIGGER_MANUAL = 0,          /**< Manual trigger only */
    HEALTH_TRIGGER_FATAL_ONLY,          /**< Only FATAL severity */
    HEALTH_TRIGGER_CRITICAL,            /**< CRITICAL and FATAL */
    HEALTH_TRIGGER_ERROR,               /**< ERROR, CRITICAL, FATAL */
    HEALTH_TRIGGER_AUTO                 /**< Intelligent auto-trigger */
} health_trigger_policy_t;

/**
 * @brief Repair outcome status
 */
typedef enum {
    HEALTH_REPAIR_OUTCOME_PENDING = 0,  /**< Repair in progress */
    HEALTH_REPAIR_OUTCOME_SUCCESS,      /**< Repair succeeded */
    HEALTH_REPAIR_OUTCOME_FAILED,       /**< Repair failed */
    HEALTH_REPAIR_OUTCOME_SKIPPED,      /**< Skipped (rate limit, etc.) */
    HEALTH_REPAIR_OUTCOME_TIMEOUT       /**< Repair timed out */
} health_repair_outcome_t;

/* ============================================================================
 * Configuration Structures
 * ============================================================================ */

/**
 * @brief Rate limiting configuration
 */
typedef struct {
    uint32_t max_repairs_per_window;    /**< Max repairs in time window */
    uint32_t window_duration_ms;        /**< Time window duration (ms) */
    uint32_t cooldown_ms;               /**< Cooldown after hitting limit */
    bool per_error_type_limit;          /**< Apply limit per error type */
} health_rate_limit_config_t;

/**
 * @brief Aggregation configuration for batching repairs
 */
typedef struct {
    bool enabled;                       /**< Enable aggregation */
    uint32_t window_ms;                 /**< Aggregation window (ms) */
    uint32_t max_batch_size;            /**< Max repairs per batch */
    bool aggregate_same_type_only;      /**< Only aggregate same error types */
} health_aggregation_config_t;

/**
 * @brief Health self-repair bridge configuration
 */
typedef struct {
    /* Trigger policy */
    health_trigger_policy_t trigger_policy; /**< When to auto-trigger repairs */

    /* Rate limiting */
    health_rate_limit_config_t rate_limit;

    /* Aggregation */
    health_aggregation_config_t aggregation;

    /* Repair options */
    bool async_repairs;                 /**< Process repairs asynchronously */
    bool notify_health_agent;           /**< Notify health agent of outcomes */
    bool enable_bio_async;              /**< Enable bio-async messaging */

    /* Confidence threshold */
    float min_confidence;               /**< Minimum confidence to trigger */

    /* Timeouts */
    uint32_t repair_timeout_ms;         /**< Per-repair timeout */

    /* Learning */
    bool learn_from_outcomes;           /**< Feed outcomes to code immune */

    /* Logging */
    bool verbose_logging;               /**< Enable verbose output */
} health_self_repair_bridge_config_t;

/* ============================================================================
 * Tracking Structures
 * ============================================================================ */

/**
 * @brief Repair request tracking record
 */
typedef struct {
    uint64_t request_id;                /**< Unique request ID */
    uint64_t diagnostic_id;             /**< Source diagnostic ID */
    uint64_t repair_id;                 /**< Self-repair repair ID */

    /* Source info */
    error_type_t error_type;            /**< Error type */
    diag_severity_t severity;           /**< Diagnostic severity */
    float confidence;                   /**< Confidence level */

    /* Status */
    health_repair_outcome_t outcome;    /**< Repair outcome */
    char error_message[256];            /**< Error message if failed */

    /* Timing */
    uint64_t submitted_at;              /**< When submitted (ms since epoch) */
    uint64_t completed_at;              /**< When completed */
    uint64_t duration_ms;               /**< Total duration */

    /* Flags */
    bool async;                         /**< Was async request */
    bool aggregated;                    /**< Part of aggregated batch */
} health_repair_tracking_t;

/* ============================================================================
 * Statistics
 * ============================================================================ */

/**
 * @brief Health self-repair bridge statistics
 */
typedef struct {
    /* Request counts */
    uint64_t repairs_triggered;         /**< Total repairs triggered */
    uint64_t repairs_succeeded;         /**< Successful repairs */
    uint64_t repairs_failed;            /**< Failed repairs */
    uint64_t repairs_skipped;           /**< Skipped (rate limit, etc.) */
    uint64_t repairs_timed_out;         /**< Timed out repairs */

    /* Rate limiting */
    uint64_t rate_limited_count;        /**< Requests rate limited */
    uint32_t current_window_count;      /**< Repairs in current window */

    /* Aggregation */
    uint64_t batches_created;           /**< Aggregation batches created */
    float avg_batch_size;               /**< Average batch size */

    /* By severity */
    uint64_t by_severity[DIAG_SEVERITY_FATAL + 1]; /**< Count by severity */

    /* By error type (top 32) */
    uint64_t by_error_type[32];         /**< Count by error type category */

    /* Timing */
    float avg_repair_time_ms;           /**< Average repair time */
    float max_repair_time_ms;           /**< Max repair time */

    /* Success rate */
    float success_rate;                 /**< Overall success rate */
} health_self_repair_bridge_stats_t;

/* ============================================================================
 * Callback Types
 * ============================================================================ */

/**
 * @brief Callback for repair trigger notification
 */
typedef void (*health_repair_trigger_cb_t)(
    uint64_t request_id,
    const diagnostic_result_t* diagnostic,
    void* user_data
);

/**
 * @brief Callback for repair outcome notification
 */
typedef void (*health_repair_outcome_cb_t)(
    uint64_t request_id,
    health_repair_outcome_t outcome,
    const self_repair_result_t* result,
    void* user_data
);

/* ============================================================================
 * Bridge Handle
 * ============================================================================ */

/**
 * @brief Opaque health self-repair bridge handle
 */
typedef struct health_self_repair_bridge health_self_repair_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default bridge configuration
 *
 * WHAT: Initialize configuration with sensible defaults
 * WHY:  Easy setup with good starting values
 * HOW:  Return struct with balanced parameters
 *
 * DEFAULTS:
 * - trigger_policy: HEALTH_TRIGGER_CRITICAL
 * - rate_limit.max_repairs_per_window: 10
 * - rate_limit.window_duration_ms: 60000 (1 minute)
 * - aggregation.window_ms: 5000 (5 seconds)
 * - async_repairs: true
 * - min_confidence: 0.6
 *
 * @param config Output configuration (non-NULL)
 * @return 0 on success, -1 on error
 */
int health_self_repair_bridge_default_config(
    health_self_repair_bridge_config_t* config
);

/**
 * @brief Create health self-repair bridge
 *
 * WHAT: Initialize bridge connecting health to self-repair
 * WHY:  Enable autonomous repair pipeline
 * HOW:  Allocate bridge, initialize tracking
 *
 * @param config Configuration (NULL for defaults)
 * @param diagnostic_bridge Health diagnostic bridge (required)
 * @param self_repair Self-repair coordinator (required)
 * @return Bridge handle or NULL on failure
 */
health_self_repair_bridge_t* health_self_repair_bridge_create(
    const health_self_repair_bridge_config_t* config,
    health_diag_bridge_t* diagnostic_bridge,
    self_repair_coordinator_t* self_repair
);

/**
 * @brief Destroy health self-repair bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Cancel pending repairs, free structures
 *
 * @param bridge Bridge handle (NULL safe)
 */
void health_self_repair_bridge_destroy(health_self_repair_bridge_t* bridge);

/**
 * @brief Connect to health agent for notifications
 *
 * WHAT: Establish connection to health agent
 * WHY:  Receive health events, send outcomes
 * HOW:  Register callbacks
 *
 * @param bridge Bridge handle
 * @param health_agent Health agent to connect
 * @return 0 on success, -1 on error
 */
int health_self_repair_bridge_connect_health_agent(
    health_self_repair_bridge_t* bridge,
    nimcp_health_agent_t* health_agent
);

/* ============================================================================
 * Trigger API
 * ============================================================================ */

/**
 * @brief Process health anomaly for potential repair
 *
 * WHAT: Evaluate anomaly and trigger repair if policy allows
 * WHY:  Main entry point for anomaly-driven repair
 * HOW:  Convert to diagnostic, check policy, trigger if appropriate
 *
 * @param bridge Bridge handle
 * @param anomaly Health monitor anomaly
 * @param request_id Output: assigned request ID (can be NULL)
 * @return 0 if repair triggered, 1 if skipped, -1 on error
 */
int health_self_repair_bridge_process_anomaly(
    health_self_repair_bridge_t* bridge,
    const anomaly_t* anomaly,
    uint64_t* request_id
);

/**
 * @brief Process health agent message for potential repair
 *
 * WHAT: Evaluate agent message and trigger repair if policy allows
 * WHY:  Agent-detected issues feed into repair pipeline
 * HOW:  Convert to diagnostic, check policy, trigger if appropriate
 *
 * @param bridge Bridge handle
 * @param message Health agent message
 * @param request_id Output: assigned request ID (can be NULL)
 * @return 0 if repair triggered, 1 if skipped, -1 on error
 */
int health_self_repair_bridge_process_agent_message(
    health_self_repair_bridge_t* bridge,
    const health_agent_message_t* message,
    uint64_t* request_id
);

/**
 * @brief Trigger repair from diagnostic result
 *
 * WHAT: Directly trigger repair from already-converted diagnostic
 * WHY:  Skip conversion when diagnostic already available
 * HOW:  Check policy, submit to self-repair
 *
 * @param bridge Bridge handle
 * @param diagnostic Diagnostic result
 * @param request_id Output: assigned request ID (can be NULL)
 * @return 0 if repair triggered, 1 if skipped, -1 on error
 */
int health_self_repair_bridge_trigger_from_diagnostic(
    health_self_repair_bridge_t* bridge,
    diagnostic_result_t* diagnostic,
    uint64_t* request_id
);

/**
 * @brief Force trigger repair (bypass policy)
 *
 * WHAT: Force repair regardless of policy
 * WHY:  Manual override for urgent situations
 * HOW:  Submit directly to self-repair
 *
 * @param bridge Bridge handle
 * @param diagnostic Diagnostic result
 * @param request_id Output: assigned request ID
 * @return 0 on success, -1 on error
 */
int health_self_repair_bridge_force_trigger(
    health_self_repair_bridge_t* bridge,
    diagnostic_result_t* diagnostic,
    uint64_t* request_id
);

/* ============================================================================
 * Rate Limiting API
 * ============================================================================ */

/**
 * @brief Check if repair would be rate limited
 *
 * WHAT: Check current rate limit status
 * WHY:  Pre-check before attempting repair
 * HOW:  Check window counts against limits
 *
 * @param bridge Bridge handle
 * @param error_type Error type (for per-type limiting)
 * @return true if would be rate limited, false if OK
 */
bool health_self_repair_bridge_is_rate_limited(
    const health_self_repair_bridge_t* bridge,
    error_type_t error_type
);

/**
 * @brief Reset rate limit counters
 *
 * WHAT: Clear rate limit tracking
 * WHY:  Reset after maintenance or configuration change
 * HOW:  Zero counters, reset window start
 *
 * @param bridge Bridge handle
 */
void health_self_repair_bridge_reset_rate_limit(
    health_self_repair_bridge_t* bridge
);

/* ============================================================================
 * Tracking and Query API
 * ============================================================================ */

/**
 * @brief Get repair tracking record by request ID
 *
 * @param bridge Bridge handle
 * @param request_id Request ID to look up
 * @return Tracking record or NULL if not found
 */
const health_repair_tracking_t* health_self_repair_bridge_get_tracking(
    const health_self_repair_bridge_t* bridge,
    uint64_t request_id
);

/**
 * @brief Get recent repair tracking records
 *
 * @param bridge Bridge handle
 * @param records Output array
 * @param max_records Max records to return
 * @return Number of records copied
 */
uint32_t health_self_repair_bridge_get_recent_tracking(
    const health_self_repair_bridge_t* bridge,
    health_repair_tracking_t* records,
    uint32_t max_records
);

/**
 * @brief Get pending repair count
 *
 * @param bridge Bridge handle
 * @return Number of pending repairs
 */
uint32_t health_self_repair_bridge_get_pending_count(
    const health_self_repair_bridge_t* bridge
);

/* ============================================================================
 * Callback Registration
 * ============================================================================ */

/**
 * @brief Set repair trigger callback
 */
int health_self_repair_bridge_set_trigger_callback(
    health_self_repair_bridge_t* bridge,
    health_repair_trigger_cb_t callback,
    void* user_data
);

/**
 * @brief Set repair outcome callback
 */
int health_self_repair_bridge_set_outcome_callback(
    health_self_repair_bridge_t* bridge,
    health_repair_outcome_cb_t callback,
    void* user_data
);

/* ============================================================================
 * Statistics API
 * ============================================================================ */

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge handle
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int health_self_repair_bridge_get_stats(
    const health_self_repair_bridge_t* bridge,
    health_self_repair_bridge_stats_t* stats
);

/**
 * @brief Reset bridge statistics
 *
 * @param bridge Bridge handle
 */
void health_self_repair_bridge_reset_stats(
    health_self_repair_bridge_t* bridge
);

/* ============================================================================
 * Processing API
 * ============================================================================ */

/**
 * @brief Process pending outcomes
 *
 * WHAT: Check and process completed async repairs
 * WHY:  Update tracking, notify callbacks
 * HOW:  Poll self-repair for completed items
 *
 * @param bridge Bridge handle
 * @param max_process Maximum outcomes to process
 * @return Number of outcomes processed
 */
uint32_t health_self_repair_bridge_process_outcomes(
    health_self_repair_bridge_t* bridge,
    uint32_t max_process
);

/**
 * @brief Process aggregation window
 *
 * WHAT: Submit aggregated repairs when window expires
 * WHY:  Batch similar repairs for efficiency
 * HOW:  Check window expiry, submit batch
 *
 * @param bridge Bridge handle
 * @return Number of batched repairs submitted
 */
uint32_t health_self_repair_bridge_process_aggregation(
    health_self_repair_bridge_t* bridge
);

/* ============================================================================
 * Bio-Async Integration
 * ============================================================================ */

/**
 * @brief Broadcast repair trigger via bio-async
 *
 * @param bridge Bridge handle
 * @param request_id Request ID
 * @param diagnostic Diagnostic being repaired
 * @return 0 on success, -1 on error
 */
int health_self_repair_bridge_broadcast_trigger(
    health_self_repair_bridge_t* bridge,
    uint64_t request_id,
    const diagnostic_result_t* diagnostic
);

/**
 * @brief Broadcast repair outcome via bio-async
 *
 * @param bridge Bridge handle
 * @param request_id Request ID
 * @param outcome Repair outcome
 * @return 0 on success, -1 on error
 */
int health_self_repair_bridge_broadcast_outcome(
    health_self_repair_bridge_t* bridge,
    uint64_t request_id,
    health_repair_outcome_t outcome
);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Get trigger policy name
 *
 * @param policy Trigger policy
 * @return String name (static)
 */
const char* health_self_repair_bridge_policy_name(health_trigger_policy_t policy);

/**
 * @brief Get repair outcome name
 *
 * @param outcome Repair outcome
 * @return String name (static)
 */
const char* health_self_repair_bridge_outcome_name(health_repair_outcome_t outcome);

/**
 * @brief Get bridge version string
 *
 * @return Version string
 */
const char* health_self_repair_bridge_version(void);

/**
 * @brief Check if bridge is ready
 *
 * @param bridge Bridge handle
 * @return true if ready, false otherwise
 */
bool health_self_repair_bridge_is_ready(const health_self_repair_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_HEALTH_SELF_REPAIR_BRIDGE_H */
