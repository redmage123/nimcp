/**
 * @file nimcp_kg_gc_agent.h
 * @brief KG Garbage Collection Subagent - Background Service
 * @version 1.0.0
 * @date 2026-01-16
 *
 * WHAT: Dedicated background subagent for KG garbage collection
 * WHY:  Offload GC work to a separate agent that runs independently of main operations
 * HOW:  Runs as a separate thread with configurable scheduling, priorities, and throttling
 *
 * SUBAGENT ARCHITECTURE:
 * ```
 * +===========================================================================+
 * |                    KG GC SUBAGENT ARCHITECTURE                             |
 * +===========================================================================+
 * |                                                                            |
 * |   Main Brain Thread                    GC Subagent Thread                  |
 * |   +-----------------+                  +-------------------------+         |
 * |   |  Brain KG Ops   |                  |  GC Agent Loop          |         |
 * |   |  - Read/Write   |    Messages      |  +-----------------+    |         |
 * |   |  - Query        | <--------------> |  | Check Schedule  |    |         |
 * |   |  - Traverse     |                  |  +-----------------+    |         |
 * |   +-----------------+                  |          |              |         |
 * |          |                             |          v              |         |
 * |          |                             |  +-----------------+    |         |
 * |          v                             |  | Analyze Waste   |    |         |
 * |   +-----------------+                  |  +-----------------+    |         |
 * |   | GC Agent Handle |                  |          |              |         |
 * |   | - Start/Stop    |                  |          v              |         |
 * |   | - Configure     |                  |  +-----------------+    |         |
 * |   | - Get Status    |                  |  | Run GC Cycle    |    |         |
 * |   +-----------------+                  |  +-----------------+    |         |
 * |                                        |          |              |         |
 * |   SCHEDULING MODES:                    |          v              |         |
 * |   - Periodic (every N minutes)         |  +-----------------+    |         |
 * |   - Threshold-based (when waste > X%)  |  | Report Stats    |    |         |
 * |   - Off-peak (during low activity)     |  +-----------------+    |         |
 * |   - On-demand (manual trigger only)    |          |              |         |
 * |                                        |          v              |         |
 * |   PRIORITY LANES:                      |  +-----------------+    |         |
 * |   - LOW: Background, fully throttled   |  | Sleep Interval  |    |         |
 * |   - NORMAL: Standard GC               |  +-----------------+    |         |
 * |   - HIGH: Urgent memory pressure       +-------------------------+         |
 * |   - CRITICAL: Emergency reclamation                                        |
 * |                                                                            |
 * +===========================================================================+
 * ```
 *
 * THREAD SAFETY: All operations are thread-safe
 *
 * USAGE:
 * ```c
 * // Create and configure GC agent
 * kg_gc_agent_config_t config;
 * kg_gc_agent_default_config(&config);
 * config.scheduling_mode = KG_GC_SCHEDULE_PERIODIC;
 * config.interval_minutes = 30;
 *
 * kg_gc_agent_t* agent = kg_gc_agent_create(kg, &config);
 *
 * // Start the agent
 * kg_gc_agent_start(agent);
 *
 * // Check status anytime
 * kg_gc_agent_status_t status;
 * kg_gc_agent_get_status(agent, &status);
 *
 * // Stop and cleanup
 * kg_gc_agent_stop(agent);
 * kg_gc_agent_destroy(agent);
 * ```
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_KG_GC_AGENT_H
#define NIMCP_KG_GC_AGENT_H

#include "core/brain/nimcp_brain_kg.h"
#include "core/brain/nimcp_kg_gc.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Default agent check interval in seconds */
#define KG_GC_AGENT_DEFAULT_CHECK_INTERVAL_SEC    60

/** Default GC interval in minutes */
#define KG_GC_AGENT_DEFAULT_GC_INTERVAL_MIN       30

/** Default waste threshold percentage for threshold-based scheduling */
#define KG_GC_AGENT_DEFAULT_WASTE_THRESHOLD       0.15f

/** Maximum agent name length */
#define KG_GC_AGENT_MAX_NAME_LEN                  64

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief GC agent scheduling modes
 */
typedef enum {
    KG_GC_SCHEDULE_PERIODIC = 0,  /**< Run GC at fixed intervals */
    KG_GC_SCHEDULE_THRESHOLD,     /**< Run when waste exceeds threshold */
    KG_GC_SCHEDULE_OFF_PEAK,      /**< Run during detected low-activity periods */
    KG_GC_SCHEDULE_ON_DEMAND,     /**< Manual trigger only */
    KG_GC_SCHEDULE_ADAPTIVE       /**< Combine threshold + off-peak */
} kg_gc_schedule_mode_t;

/**
 * @brief GC agent priority levels
 */
typedef enum {
    KG_GC_PRIORITY_LOW = 0,       /**< Background GC, fully throttled */
    KG_GC_PRIORITY_NORMAL,        /**< Standard GC priority */
    KG_GC_PRIORITY_HIGH,          /**< Elevated priority (memory pressure) */
    KG_GC_PRIORITY_CRITICAL       /**< Emergency GC, minimal throttling */
} kg_gc_priority_t;

/**
 * @brief GC agent state
 */
typedef enum {
    KG_GC_AGENT_STOPPED = 0,      /**< Agent not running */
    KG_GC_AGENT_STARTING,         /**< Agent is starting up */
    KG_GC_AGENT_RUNNING,          /**< Agent is active and monitoring */
    KG_GC_AGENT_PAUSED,           /**< Agent is paused */
    KG_GC_AGENT_COLLECTING,       /**< Currently running GC */
    KG_GC_AGENT_STOPPING,         /**< Agent is shutting down */
    KG_GC_AGENT_ERROR             /**< Agent encountered an error */
} kg_gc_agent_state_t;

/* ============================================================================
 * Configuration Structure
 * ============================================================================ */

/**
 * @brief GC agent configuration
 */
typedef struct {
    char agent_name[KG_GC_AGENT_MAX_NAME_LEN]; /**< Agent identifier */

    /* Scheduling */
    kg_gc_schedule_mode_t scheduling_mode;     /**< How GC is triggered */
    uint32_t interval_minutes;                 /**< GC interval for periodic mode */
    uint32_t check_interval_seconds;           /**< How often agent checks state */
    float waste_threshold;                     /**< Threshold for threshold mode [0.0-1.0] */

    /* Priority and throttling */
    kg_gc_priority_t priority;                 /**< Agent priority level */
    float cpu_limit_percent;                   /**< Max CPU usage [0.0-100.0] */
    uint32_t max_duration_seconds;             /**< Max time per GC cycle */

    /* GC targets (bitmask of kg_gc_target_t) */
    uint32_t gc_targets;                       /**< Which garbage types to collect */

    /* Off-peak configuration */
    uint32_t off_peak_start_hour;              /**< Start of off-peak window (0-23) */
    uint32_t off_peak_end_hour;                /**< End of off-peak window (0-23) */
    uint32_t activity_threshold_ops_per_sec;   /**< Max ops/sec to consider "off-peak" */

    /* Advanced options */
    bool enable_compaction;                    /**< Run compaction after GC */
    bool enable_metrics;                       /**< Emit observability metrics */
    bool enable_notifications;                 /**< Send event notifications */
    bool run_on_startup;                       /**< Run GC immediately on agent start */
} kg_gc_agent_config_t;

/* ============================================================================
 * Status Structure
 * ============================================================================ */

/**
 * @brief GC agent status
 */
typedef struct {
    kg_gc_agent_state_t state;                 /**< Current agent state */
    kg_gc_priority_t current_priority;         /**< Active priority level */

    /* Timing */
    uint64_t agent_started_at;                 /**< When agent was started (ms) */
    uint64_t last_gc_run_at;                   /**< Last GC completion time (ms) */
    uint64_t next_gc_scheduled_at;             /**< Next scheduled GC time (ms) */
    uint64_t uptime_seconds;                   /**< Agent uptime */

    /* Statistics */
    uint32_t total_gc_runs;                    /**< Total GC cycles completed */
    uint32_t successful_gc_runs;               /**< Successful GC cycles */
    uint32_t failed_gc_runs;                   /**< Failed GC cycles */
    uint64_t total_items_collected;            /**< Total items collected */
    uint64_t total_bytes_reclaimed;            /**< Total bytes reclaimed */
    uint64_t total_gc_duration_ms;             /**< Cumulative GC time */

    /* Current GC progress (if collecting) */
    float current_gc_progress;                 /**< Progress of current GC [0.0-1.0] */
    uint32_t current_gc_items_processed;       /**< Items processed in current GC */

    /* Health */
    float current_waste_level;                 /**< Current estimated waste level */
    float avg_gc_duration_ms;                  /**< Average GC cycle duration */
    char last_error[256];                      /**< Last error message */
} kg_gc_agent_status_t;

/* ============================================================================
 * Callback Types
 * ============================================================================ */

/* Forward declaration for callback signatures */
struct kg_gc_agent;

/**
 * @brief GC completion callback
 *
 * Called after each GC cycle completes.
 *
 * @param agent Agent that completed GC
 * @param stats Statistics from the GC run
 * @param user_data User-provided context
 */
typedef void (*kg_gc_agent_complete_fn)(
    struct kg_gc_agent* agent,
    const kg_gc_stats_t* stats,
    void* user_data
);

/**
 * @brief Priority escalation callback
 *
 * Called when agent detects conditions requiring priority escalation.
 *
 * @param agent Agent requesting escalation
 * @param current Current priority
 * @param requested Requested priority
 * @param reason Reason for escalation
 * @param user_data User-provided context
 * @return true to approve escalation, false to deny
 */
typedef bool (*kg_gc_agent_escalate_fn)(
    struct kg_gc_agent* agent,
    kg_gc_priority_t current,
    kg_gc_priority_t requested,
    const char* reason,
    void* user_data
);

/* ============================================================================
 * Opaque Type
 * ============================================================================ */

/**
 * @brief GC agent handle (opaque)
 */
typedef struct kg_gc_agent kg_gc_agent_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default agent configuration
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int kg_gc_agent_default_config(kg_gc_agent_config_t* config);

/**
 * @brief Create GC agent for a knowledge graph
 *
 * Creates but does not start the agent. Call kg_gc_agent_start() to begin.
 *
 * @param kg Brain knowledge graph to manage
 * @param config Agent configuration (NULL for defaults)
 * @return Agent handle or NULL on error
 */
kg_gc_agent_t* kg_gc_agent_create(brain_kg_t* kg, const kg_gc_agent_config_t* config);

/**
 * @brief Destroy GC agent
 *
 * Stops the agent if running and releases all resources.
 *
 * @param agent Agent to destroy (NULL safe)
 */
void kg_gc_agent_destroy(kg_gc_agent_t* agent);

/**
 * @brief Start the GC agent
 *
 * Spawns background thread and begins monitoring/scheduling.
 *
 * @param agent Agent to start
 * @return 0 on success, -1 on error
 */
int kg_gc_agent_start(kg_gc_agent_t* agent);

/**
 * @brief Stop the GC agent
 *
 * Gracefully stops the agent, waiting for current GC to complete.
 *
 * @param agent Agent to stop
 * @return 0 on success, -1 on error
 */
int kg_gc_agent_stop(kg_gc_agent_t* agent);

/**
 * @brief Pause the GC agent
 *
 * Temporarily suspends GC scheduling without stopping the thread.
 *
 * @param agent Agent to pause
 * @return 0 on success, -1 on error
 */
int kg_gc_agent_pause(kg_gc_agent_t* agent);

/**
 * @brief Resume the GC agent
 *
 * Resumes a paused agent.
 *
 * @param agent Agent to resume
 * @return 0 on success, -1 on error
 */
int kg_gc_agent_resume(kg_gc_agent_t* agent);

/* ============================================================================
 * Configuration API
 * ============================================================================ */

/**
 * @brief Update agent configuration
 *
 * Can be called while agent is running.
 *
 * @param agent Agent to configure
 * @param config New configuration
 * @return 0 on success, -1 on error
 */
int kg_gc_agent_set_config(kg_gc_agent_t* agent, const kg_gc_agent_config_t* config);

/**
 * @brief Get current agent configuration
 *
 * @param agent Agent to query
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int kg_gc_agent_get_config(const kg_gc_agent_t* agent, kg_gc_agent_config_t* config);

/**
 * @brief Set agent priority
 *
 * @param agent Agent to configure
 * @param priority New priority level
 * @return 0 on success, -1 on error
 */
int kg_gc_agent_set_priority(kg_gc_agent_t* agent, kg_gc_priority_t priority);

/**
 * @brief Set GC targets
 *
 * @param agent Agent to configure
 * @param targets Bitmask of kg_gc_target_t
 * @return 0 on success, -1 on error
 */
int kg_gc_agent_set_targets(kg_gc_agent_t* agent, uint32_t targets);

/* ============================================================================
 * Status and Monitoring API
 * ============================================================================ */

/**
 * @brief Get agent status
 *
 * @param agent Agent to query
 * @param status Output status
 * @return 0 on success, -1 on error
 */
int kg_gc_agent_get_status(const kg_gc_agent_t* agent, kg_gc_agent_status_t* status);

/**
 * @brief Get agent state
 *
 * @param agent Agent to query
 * @return Current state
 */
kg_gc_agent_state_t kg_gc_agent_get_state(const kg_gc_agent_t* agent);

/**
 * @brief Check if agent is running
 *
 * @param agent Agent to check
 * @return true if running (including collecting/paused)
 */
bool kg_gc_agent_is_running(const kg_gc_agent_t* agent);

/**
 * @brief Check if GC is currently in progress
 *
 * @param agent Agent to check
 * @return true if actively collecting
 */
bool kg_gc_agent_is_collecting(const kg_gc_agent_t* agent);

/* ============================================================================
 * Manual Control API
 * ============================================================================ */

/**
 * @brief Trigger immediate GC run
 *
 * Bypasses scheduling and runs GC immediately.
 *
 * @param agent Agent to trigger
 * @return 0 on success, -1 on error (e.g., already collecting)
 */
int kg_gc_agent_trigger_now(kg_gc_agent_t* agent);

/**
 * @brief Request priority escalation
 *
 * Manually request higher priority for urgent situations.
 *
 * @param agent Agent to escalate
 * @param priority Requested priority
 * @param reason Reason for escalation
 * @return 0 on success, -1 on error
 */
int kg_gc_agent_request_escalation(
    kg_gc_agent_t* agent,
    kg_gc_priority_t priority,
    const char* reason
);

/**
 * @brief Cancel current GC operation
 *
 * @param agent Agent to interrupt
 * @return 0 on success, -1 if not collecting
 */
int kg_gc_agent_cancel_current(kg_gc_agent_t* agent);

/* ============================================================================
 * Callback Registration
 * ============================================================================ */

/**
 * @brief Set GC completion callback
 *
 * @param agent Agent to configure
 * @param callback Callback function
 * @param user_data Context passed to callback
 * @return 0 on success, -1 on error
 */
int kg_gc_agent_set_complete_callback(
    kg_gc_agent_t* agent,
    kg_gc_agent_complete_fn callback,
    void* user_data
);

/**
 * @brief Set priority escalation callback
 *
 * @param agent Agent to configure
 * @param callback Callback function
 * @param user_data Context passed to callback
 * @return 0 on success, -1 on error
 */
int kg_gc_agent_set_escalation_callback(
    kg_gc_agent_t* agent,
    kg_gc_agent_escalate_fn callback,
    void* user_data
);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Convert schedule mode to string
 */
const char* kg_gc_schedule_mode_to_string(kg_gc_schedule_mode_t mode);

/**
 * @brief Convert priority to string
 */
const char* kg_gc_priority_to_string(kg_gc_priority_t priority);

/**
 * @brief Convert agent state to string
 */
const char* kg_gc_agent_state_to_string(kg_gc_agent_state_t state);

/**
 * @brief Reset agent statistics
 *
 * @param agent Agent to reset
 */
void kg_gc_agent_reset_stats(kg_gc_agent_t* agent);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_KG_GC_AGENT_H */
