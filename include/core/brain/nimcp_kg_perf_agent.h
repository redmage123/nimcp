/**
 * @file nimcp_kg_perf_agent.h
 * @brief KG Performance Monitoring Agent
 * @version 1.0.0
 * @date 2025-01-16
 *
 * WHAT: Independent background agent that monitors KG I/O performance and
 *       dynamically adjusts resources (connections, threads, buffers)
 * WHY:  Ensures optimal KG persistence performance under varying load conditions,
 *       prevents latency spikes and resource exhaustion through proactive scaling
 * HOW:  Runs as background thread, samples metrics, analyzes trends, makes
 *       scaling decisions, and coordinates with brain immune system for alerting
 *
 * ARCHITECTURE:
 * ```
 * +-----------------------------------------------------------------------------+
 * |                    KG PERFORMANCE MONITORING AGENT                           |
 * +-----------------------------------------------------------------------------+
 * |                                                                              |
 * |  AGENT LIFECYCLE                                                             |
 * |  ---------------                                                             |
 * |  +-----------------------------------------------------------------------------+
 * |  | KG System Init -> Agent Thread Spawned -> Runs Independently Until Shutdown|
 * |  +-----------------------------------------------------------------------------+
 * |                                                                              |
 * |  MONITORING LOOP (runs every sample_interval_ms)                            |
 * |  ---------------------------------------------------------------------------|
 * |  +----------------+     +----------------+     +----------------+          |
 * |  | Collect Metrics|---->| Analyze Trends |---->| Make Decision  |          |
 * |  | - Latency      |     | - Moving avg   |     | - Scale up?    |          |
 * |  | - Throughput   |     | - Anomalies    |     | - Scale down?  |          |
 * |  | - Queue depth  |     | - Predictions  |     | - Hold steady? |          |
 * |  | - Error rate   |     | - Thresholds   |     | - Alert?       |          |
 * |  +----------------+     +----------------+     +-------+--------+          |
 * |                                                         |                    |
 * |                                                         v                    |
 * |  +-----------------------------------------------------------------------------+
 * |  |                     DYNAMIC RESOURCE ADJUSTMENT                          |
 * |  |  +-----------------+  +-----------------+  +-----------------+         |
 * |  |  | Connection Pool |  | Thread Pool     |  | Buffer Sizes    |         |
 * |  |  | Scale up/down   |  | Add/remove      |  | Grow/shrink     |         |
 * |  |  | connections     |  | worker threads  |  | write buffers   |         |
 * |  |  +-----------------+  +-----------------+  +-----------------+         |
 * |  +-----------------------------------------------------------------------------+
 * |                                                                              |
 * |  ALERTING (via brain's immune system)                                       |
 * |  ---------------------------------------                                     |
 * |  | WARNING: Latency > threshold -> Logged, scaling attempted                |
 * |  | CRITICAL: Latency > critical -> Alert to brain, emergency scaling        |
 * |  | RECOVERY: Performance restored -> Log recovery, scale down gradually     |
 * |                                                                              |
 * +-----------------------------------------------------------------------------+
 * ```
 *
 * SCALING DECISIONS:
 * | Condition                                        | Decision              |
 * |--------------------------------------------------|-----------------------|
 * | Latency > critical OR queue > 90%                | SCALE_UP_EMERGENCY    |
 * | Latency > warning OR queue > 70% (sustained)     | SCALE_UP_AGGRESSIVE   |
 * | Connection wait > 50ms (sustained)               | SCALE_UP_GRADUAL      |
 * | Idle connections > 50% AND low util (sustained)  | SCALE_DOWN_GRADUAL    |
 * | All metrics healthy                              | HOLD                  |
 *
 * THREAD SAFETY: Agent operations are thread-safe via internal mutex
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_KG_PERF_AGENT_H
#define NIMCP_KG_PERF_AGENT_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

/* Forward declare types from other modules to avoid circular includes */
typedef struct kg_io_dispatcher kg_io_dispatcher_t;
typedef struct kg_connection_pool kg_connection_pool_t;
typedef struct nimcp_brain_immune nimcp_brain_immune_t;

/* ============================================================================
 * Performance Metrics Structure
 * ============================================================================ */

/**
 * @brief Performance metrics collected by the agent
 *
 * WHAT: Snapshot of KG I/O performance metrics at a point in time
 * WHY:  Provides data for trend analysis and scaling decisions
 * HOW:  Collected from I/O dispatcher and connection pool at sample intervals
 */
typedef struct {
    /* Latency metrics (nanoseconds) */
    uint64_t write_latency_avg_ns;   /**< Average write latency */
    uint64_t write_latency_p50_ns;   /**< 50th percentile write latency */
    uint64_t write_latency_p95_ns;   /**< 95th percentile write latency */
    uint64_t write_latency_p99_ns;   /**< 99th percentile write latency */
    uint64_t write_latency_max_ns;   /**< Maximum observed write latency */

    uint64_t read_latency_avg_ns;    /**< Average read latency */
    uint64_t read_latency_p50_ns;    /**< 50th percentile read latency */
    uint64_t read_latency_p95_ns;    /**< 95th percentile read latency */
    uint64_t read_latency_p99_ns;    /**< 99th percentile read latency */
    uint64_t read_latency_max_ns;    /**< Maximum observed read latency */

    /* Throughput metrics */
    uint64_t writes_per_sec;         /**< Write operations per second */
    uint64_t reads_per_sec;          /**< Read operations per second */
    uint64_t bytes_written_per_sec;  /**< Bytes written per second */
    uint64_t bytes_read_per_sec;     /**< Bytes read per second */

    /* Queue metrics */
    uint32_t write_queue_depth;      /**< Current write queue depth */
    uint32_t read_queue_depth;       /**< Current read queue depth */
    uint32_t pending_operations;     /**< Total pending operations */
    float queue_saturation;          /**< Queue saturation ratio [0.0-1.0] */

    /* Connection pool metrics */
    uint32_t active_connections;     /**< Currently active connections */
    uint32_t idle_connections;       /**< Currently idle connections */
    uint32_t total_connections;      /**< Total connections in pool */
    uint32_t connection_wait_count;  /**< Requests waiting for connection */
    uint64_t connection_wait_time_ns; /**< Time spent waiting for connection */

    /* Error metrics */
    uint32_t errors_per_sec;         /**< Errors per second */
    uint32_t timeouts_per_sec;       /**< Timeouts per second */
    uint32_t connection_failures;    /**< Connection failure count */
    float error_rate;                /**< Error ratio (errors/total ops) */

    /* Resource utilization */
    float cpu_utilization;           /**< CPU utilization [0.0-1.0] */
    uint64_t memory_usage_bytes;     /**< Memory usage in bytes */
    uint32_t open_file_descriptors;  /**< Open file descriptor count */

    /* Timestamp */
    uint64_t sample_timestamp_ns;    /**< Sample timestamp (nanoseconds) */
} kg_perf_metrics_t;

/* ============================================================================
 * Scaling Decision Enumeration
 * ============================================================================ */

/**
 * @brief Scaling decision made by the agent
 *
 * WHAT: Categorizes the scaling action to take
 * WHY:  Enables appropriate response to different load conditions
 * HOW:  Based on threshold analysis and trend prediction
 */
typedef enum {
    KG_SCALE_HOLD = 0,               /**< No change needed */
    KG_SCALE_UP_GRADUAL,             /**< Add resources slowly */
    KG_SCALE_UP_AGGRESSIVE,          /**< Add resources quickly (high load) */
    KG_SCALE_UP_EMERGENCY,           /**< Maximum scaling (critical situation) */
    KG_SCALE_DOWN_GRADUAL,           /**< Remove resources slowly */
    KG_SCALE_DOWN_AGGRESSIVE         /**< Remove resources quickly (low load) */
} kg_scale_decision_t;

/* ============================================================================
 * Scaling Action Structure
 * ============================================================================ */

/**
 * @brief Scaling action taken by the agent
 *
 * WHAT: Records a scaling action with all relevant details
 * WHY:  Enables audit trail and analysis of scaling decisions
 * HOW:  Created each time agent makes a scaling decision
 */
typedef struct {
    kg_scale_decision_t decision;    /**< The scaling decision made */
    uint64_t timestamp_ns;           /**< When the decision was made */

    /* Connection pool changes */
    int32_t connection_delta;        /**< Connection change (+N or -N) */
    uint32_t new_pool_size;          /**< New pool size after change */

    /* Thread pool changes */
    int32_t writer_thread_delta;     /**< Writer thread change (+N or -N) */
    int32_t reader_thread_delta;     /**< Reader thread change (+N or -N) */

    /* Buffer changes */
    int32_t write_buffer_delta_kb;   /**< Write buffer change in KB */
    int32_t read_buffer_delta_kb;    /**< Read buffer change in KB */

    /* Reason for decision */
    char reason[256];                /**< Human-readable reason */

    /* Triggering metrics */
    kg_perf_metrics_t triggering_metrics; /**< Metrics that triggered decision */
} kg_scale_action_t;

/* ============================================================================
 * Performance Thresholds Structure
 * ============================================================================ */

/**
 * @brief Performance thresholds for scaling decisions
 *
 * WHAT: Configurable thresholds that trigger scaling actions
 * WHY:  Allows tuning agent behavior for different workloads
 * HOW:  Compared against collected metrics during analysis
 */
typedef struct {
    /* Latency thresholds (nanoseconds) */
    uint64_t latency_warning_ns;     /**< Warning threshold (default: 10ms) */
    uint64_t latency_critical_ns;    /**< Critical threshold (default: 100ms) */
    uint64_t latency_target_ns;      /**< Target latency (default: 1ms) */

    /* Throughput thresholds */
    uint64_t min_writes_per_sec;     /**< Minimum acceptable writes/sec */
    uint64_t min_reads_per_sec;      /**< Minimum acceptable reads/sec */

    /* Queue thresholds */
    float queue_saturation_warning;  /**< Warning threshold (default: 0.7) */
    float queue_saturation_critical; /**< Critical threshold (default: 0.9) */

    /* Connection wait thresholds */
    uint64_t connection_wait_warning_ns; /**< Wait time warning (default: 50ms) */
    uint32_t connection_wait_count_warning; /**< Waiting count warning (default: 10) */

    /* Error thresholds */
    float error_rate_warning;        /**< Error rate warning (default: 0.01) */
    float error_rate_critical;       /**< Error rate critical (default: 0.05) */

    /* Scale-down thresholds (must be sustained for cooldown period) */
    float idle_connection_threshold; /**< Idle ratio for scale-down (default: 0.5) */
    float low_utilization_threshold; /**< Low util threshold (default: 0.3) */
} kg_perf_thresholds_t;

/* ============================================================================
 * Scaling Policy Structure
 * ============================================================================ */

/**
 * @brief Scaling policy configuration
 *
 * WHAT: Configures how the agent scales resources
 * WHY:  Allows fine-tuning of scaling behavior and limits
 * HOW:  Controls min/max bounds, increments, and timing
 */
typedef struct {
    /* Pool size limits */
    uint32_t min_connections;        /**< Minimum connections (default: 4) */
    uint32_t max_connections;        /**< Maximum connections (default: 128) */
    uint32_t min_writer_threads;     /**< Minimum writers (default: 2) */
    uint32_t max_writer_threads;     /**< Maximum writers (default: 32) */
    uint32_t min_reader_threads;     /**< Minimum readers (default: 2) */
    uint32_t max_reader_threads;     /**< Maximum readers (default: 64) */

    /* Scaling increments */
    uint32_t scale_up_increment;     /**< Connections per scale-up (default: 4) */
    uint32_t scale_down_increment;   /**< Connections per scale-down (default: 2) */
    uint32_t emergency_scale_increment; /**< Emergency increment (default: 16) */

    /* Timing */
    uint32_t scale_up_cooldown_ms;   /**< Cooldown between scale-ups (default: 5000) */
    uint32_t scale_down_cooldown_ms; /**< Cooldown between scale-downs (default: 60000) */
    uint32_t metric_window_ms;       /**< Averaging window (default: 10000) */
    uint32_t trend_window_samples;   /**< Samples for trend analysis (default: 30) */

    /* Safety */
    bool enable_emergency_scaling;   /**< Allow emergency scale-up (default: true) */
    bool enable_scale_down;          /**< Allow automatic scale-down (default: true) */
    bool require_sustained_load;     /**< Require sustained high load (default: true) */
    uint32_t sustained_load_samples; /**< Required sustained samples (default: 5) */
} kg_scaling_policy_t;

/* ============================================================================
 * Agent Configuration Structure
 * ============================================================================ */

/**
 * @brief Performance agent configuration
 *
 * WHAT: Complete configuration for the performance monitoring agent
 * WHY:  Allows customization of monitoring and scaling behavior
 * HOW:  Passed to kg_perf_agent_create() during initialization
 */
typedef struct {
    /* Monitoring settings */
    uint32_t sample_interval_ms;     /**< Metric collection interval (default: 1000) */
    uint32_t analysis_interval_ms;   /**< Trend analysis interval (default: 5000) */
    bool enable_predictive_scaling;  /**< Use ML for prediction (default: true) */

    /* Thresholds */
    kg_perf_thresholds_t thresholds; /**< Performance thresholds */

    /* Scaling policy */
    kg_scaling_policy_t policy;      /**< Scaling policy configuration */

    /* Alerting */
    bool enable_alerts;              /**< Enable alerting (default: true) */
    bool alert_to_immune_system;     /**< Report to brain immune (default: true) */
    bool alert_to_log;               /**< Log alerts (default: true) */

    /* History */
    uint32_t metrics_history_size;   /**< Samples to keep (default: 3600 = 1hr at 1s) */
    bool persist_metrics_to_kg;      /**< Store metrics in KG (default: true) */

    /* Thread settings */
    int cpu_affinity;                /**< CPU core for agent (-1 = any) */
    int priority;                    /**< Thread priority (default: normal) */
} kg_perf_agent_config_t;

/* ============================================================================
 * Alert Level Enumeration
 * ============================================================================ */

/**
 * @brief Performance alert severity levels
 *
 * WHAT: Categorizes alert severity
 * WHY:  Enables appropriate response escalation
 * HOW:  Used in alert callbacks and immune system integration
 */
typedef enum {
    KG_ALERT_INFO = 0,               /**< Informational message */
    KG_ALERT_WARNING,                /**< Warning condition */
    KG_ALERT_CRITICAL,               /**< Critical issue requiring action */
    KG_ALERT_RECOVERY                /**< Recovery from previous issue */
} kg_perf_alert_level_t;

/* ============================================================================
 * Opaque Handle
 * ============================================================================ */

/**
 * @brief Performance agent context (opaque)
 *
 * WHAT: Opaque handle to the performance agent
 * WHY:  Encapsulates internal state, enables forward compatibility
 * HOW:  Created by kg_perf_agent_create(), destroyed by kg_perf_agent_destroy()
 */
typedef struct kg_perf_agent kg_perf_agent_t;

/* ============================================================================
 * Callback Types
 * ============================================================================ */

/**
 * @brief Alert callback type
 *
 * WHAT: Callback invoked when performance alerts are generated
 * WHY:  Allows external systems to react to performance issues
 * HOW:  Registered via kg_perf_agent_register_alert_callback()
 *
 * @param level Alert severity level
 * @param message Human-readable alert message
 * @param metrics Current metrics that triggered the alert
 * @param user_data User-provided context
 */
typedef void (*kg_perf_alert_fn)(kg_perf_alert_level_t level,
                                  const char* message,
                                  const kg_perf_metrics_t* metrics,
                                  void* user_data);

/**
 * @brief Scaling action callback type
 *
 * WHAT: Callback invoked before/after scaling actions
 * WHY:  Allows external observation and coordination of scaling
 * HOW:  Registered via kg_perf_agent_register_scale_callback()
 *
 * @param action The scaling action being taken
 * @param user_data User-provided context
 */
typedef void (*kg_perf_scale_fn)(const kg_scale_action_t* action, void* user_data);

/* ============================================================================
 * Agent Lifecycle Functions
 * ============================================================================ */

/**
 * @brief Get default agent configuration
 *
 * WHAT: Initializes configuration with sensible defaults
 * WHY:  Simplifies agent creation with reasonable starting values
 * HOW:  Fills in all fields with documented default values
 *
 * @param config Output configuration structure
 * @return 0 on success, -1 on error (NULL config)
 */
int kg_perf_agent_default_config(kg_perf_agent_config_t* config);

/**
 * @brief Create and start the performance agent
 *
 * WHAT: Creates agent instance and spawns monitoring thread
 * WHY:  Enables automatic performance monitoring and scaling
 * HOW:  Allocates resources, starts background thread, begins monitoring
 *
 * Called automatically during KG system initialization if enabled.
 *
 * @param dispatcher I/O dispatcher to monitor (required)
 * @param pool Connection pool to scale (required)
 * @param config Agent configuration (NULL for defaults)
 * @return Agent handle on success, NULL on error
 */
kg_perf_agent_t* kg_perf_agent_create(
    kg_io_dispatcher_t* dispatcher,
    kg_connection_pool_t* pool,
    const kg_perf_agent_config_t* config);

/**
 * @brief Stop and destroy the agent
 *
 * WHAT: Stops monitoring thread and frees all resources
 * WHY:  Clean shutdown during KG system teardown
 * HOW:  Signals thread to stop, waits for exit, frees memory
 *
 * @param agent Agent to destroy (NULL safe)
 */
void kg_perf_agent_destroy(kg_perf_agent_t* agent);

/**
 * @brief Start monitoring
 *
 * WHAT: Starts the monitoring thread if not already running
 * WHY:  Allows delayed start or restart after pause
 * HOW:  Spawns or resumes the monitoring thread
 *
 * @param agent Agent handle
 * @return 0 on success, -1 on error
 */
int kg_perf_agent_start(kg_perf_agent_t* agent);

/**
 * @brief Stop monitoring
 *
 * WHAT: Pauses monitoring thread (can be restarted)
 * WHY:  Allows temporary pause without destroying agent
 * HOW:  Signals thread to pause, waits for acknowledgment
 *
 * @param agent Agent handle
 * @return 0 on success, -1 on error
 */
int kg_perf_agent_stop(kg_perf_agent_t* agent);

/**
 * @brief Check if agent is running
 *
 * WHAT: Returns current running state of the agent
 * WHY:  Allows callers to check agent status
 * HOW:  Returns internal running flag
 *
 * @param agent Agent handle
 * @return true if running, false if stopped or NULL
 */
bool kg_perf_agent_is_running(const kg_perf_agent_t* agent);

/* ============================================================================
 * Manual Intervention Functions
 * ============================================================================ */

/**
 * @brief Force immediate scaling
 *
 * WHAT: Forces a scaling action bypassing threshold checks
 * WHY:  Allows manual intervention for known situations
 * HOW:  Applies specified scaling decision immediately
 *
 * @param agent Agent handle
 * @param decision Scaling decision to apply
 * @return 0 on success, -1 on error
 */
int kg_perf_agent_force_scale(kg_perf_agent_t* agent, kg_scale_decision_t decision);

/**
 * @brief Set new thresholds at runtime
 *
 * WHAT: Updates performance thresholds while agent is running
 * WHY:  Allows dynamic tuning based on observed behavior
 * HOW:  Atomically replaces current thresholds
 *
 * @param agent Agent handle
 * @param thresholds New thresholds to apply
 * @return 0 on success, -1 on error
 */
int kg_perf_agent_set_thresholds(kg_perf_agent_t* agent,
                                  const kg_perf_thresholds_t* thresholds);

/**
 * @brief Set new scaling policy at runtime
 *
 * WHAT: Updates scaling policy while agent is running
 * WHY:  Allows dynamic adjustment of scaling behavior
 * HOW:  Atomically replaces current policy
 *
 * @param agent Agent handle
 * @param policy New policy to apply
 * @return 0 on success, -1 on error
 */
int kg_perf_agent_set_policy(kg_perf_agent_t* agent,
                              const kg_scaling_policy_t* policy);

/**
 * @brief Pause automatic scaling
 *
 * WHAT: Pauses automatic scaling while continuing monitoring
 * WHY:  Allows observation without intervention during analysis
 * HOW:  Sets scaling pause flag, continues collecting metrics
 *
 * @param agent Agent handle
 * @return 0 on success, -1 on error
 */
int kg_perf_agent_pause_scaling(kg_perf_agent_t* agent);

/**
 * @brief Resume automatic scaling
 *
 * WHAT: Resumes automatic scaling after pause
 * WHY:  Re-enables agent intervention after analysis period
 * HOW:  Clears scaling pause flag
 *
 * @param agent Agent handle
 * @return 0 on success, -1 on error
 */
int kg_perf_agent_resume_scaling(kg_perf_agent_t* agent);

/* ============================================================================
 * Metrics and History Functions
 * ============================================================================ */

/**
 * @brief Get current metrics snapshot
 *
 * WHAT: Returns most recent collected metrics
 * WHY:  Allows external monitoring of current performance
 * HOW:  Copies current metrics to caller's buffer
 *
 * @param agent Agent handle
 * @param metrics Output metrics structure
 * @return 0 on success, -1 on error
 */
int kg_perf_agent_get_current_metrics(const kg_perf_agent_t* agent,
                                       kg_perf_metrics_t* metrics);

/**
 * @brief Get metrics history
 *
 * WHAT: Returns historical metrics samples (most recent first)
 * WHY:  Enables external trend analysis and visualization
 * HOW:  Copies from internal circular buffer
 *
 * @param agent Agent handle
 * @param history Output array for metrics
 * @param max_samples Maximum samples to return
 * @param out_count Actual number of samples returned
 * @return 0 on success, -1 on error
 */
int kg_perf_agent_get_metrics_history(const kg_perf_agent_t* agent,
                                       kg_perf_metrics_t* history,
                                       uint32_t max_samples,
                                       uint32_t* out_count);

/**
 * @brief Get scaling action history
 *
 * WHAT: Returns history of scaling actions taken
 * WHY:  Enables audit trail and effectiveness analysis
 * HOW:  Copies from internal action log
 *
 * @param agent Agent handle
 * @param history Output array for actions
 * @param max_actions Maximum actions to return
 * @param out_count Actual number of actions returned
 * @return 0 on success, -1 on error
 */
int kg_perf_agent_get_action_history(const kg_perf_agent_t* agent,
                                      kg_scale_action_t* history,
                                      uint32_t max_actions,
                                      uint32_t* out_count);

/**
 * @brief Get predicted metrics
 *
 * WHAT: Returns predicted metrics for future time point
 * WHY:  Enables proactive scaling before issues occur
 * HOW:  Uses ML model if predictive scaling enabled
 *
 * @param agent Agent handle
 * @param lookahead_ms Time ahead to predict (milliseconds)
 * @param predicted Output predicted metrics
 * @return 0 on success, -1 on error or if prediction disabled
 */
int kg_perf_agent_get_prediction(const kg_perf_agent_t* agent,
                                  uint32_t lookahead_ms,
                                  kg_perf_metrics_t* predicted);

/* ============================================================================
 * Alerting and Callback Functions
 * ============================================================================ */

/**
 * @brief Register alert callback
 *
 * WHAT: Registers callback for performance alerts
 * WHY:  Allows external systems to receive alert notifications
 * HOW:  Adds callback to internal list, invoked on alerts
 *
 * @param agent Agent handle
 * @param callback Alert callback function
 * @param user_data Context passed to callback
 * @return 0 on success, -1 on error
 */
int kg_perf_agent_register_alert_callback(kg_perf_agent_t* agent,
                                           kg_perf_alert_fn callback,
                                           void* user_data);

/**
 * @brief Unregister alert callback
 *
 * WHAT: Removes previously registered alert callback
 * WHY:  Allows cleanup when callback owner is destroyed
 * HOW:  Removes callback from internal list
 *
 * @param agent Agent handle
 * @param callback Callback to remove
 * @return 0 on success, -1 if not found
 */
int kg_perf_agent_unregister_alert_callback(kg_perf_agent_t* agent,
                                             kg_perf_alert_fn callback);

/**
 * @brief Register scaling callbacks
 *
 * WHAT: Registers callbacks for scaling events
 * WHY:  Allows external coordination and logging of scaling
 * HOW:  Invokes before_scale before action, after_scale after
 *
 * @param agent Agent handle
 * @param before_scale Callback before scaling (can be NULL)
 * @param after_scale Callback after scaling (can be NULL)
 * @param user_data Context passed to callbacks
 * @return 0 on success, -1 on error
 */
int kg_perf_agent_register_scale_callback(kg_perf_agent_t* agent,
                                           kg_perf_scale_fn before_scale,
                                           kg_perf_scale_fn after_scale,
                                           void* user_data);

/* ============================================================================
 * Immune System Integration Functions
 * ============================================================================ */

/**
 * @brief Connect agent to brain's immune system
 *
 * WHAT: Establishes connection to brain immune system for coordinated response
 * WHY:  Enables system-wide threat response coordination
 * HOW:  Registers as immune system client, enables alert routing
 *
 * @param agent Agent handle
 * @param immune Brain immune system handle
 * @return 0 on success, -1 on error
 */
int kg_perf_agent_connect_immune_system(kg_perf_agent_t* agent,
                                         nimcp_brain_immune_t* immune);

/**
 * @brief Report performance anomaly to immune system
 *
 * WHAT: Reports a performance anomaly to brain immune system
 * WHY:  Enables coordinated response to performance threats
 * HOW:  Creates immune system threat report with anomaly details
 *
 * @param agent Agent handle
 * @param description Anomaly description
 * @param severity Alert severity level
 * @return 0 on success, -1 on error
 */
int kg_perf_agent_report_anomaly(kg_perf_agent_t* agent,
                                  const char* description,
                                  kg_perf_alert_level_t severity);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Convert scale decision to string
 *
 * WHAT: Returns human-readable string for scale decision
 * WHY:  Enables logging and debugging
 * HOW:  Switch on enum value
 *
 * @param decision Scale decision value
 * @return String representation (static, do not free)
 */
const char* kg_scale_decision_to_string(kg_scale_decision_t decision);

/**
 * @brief Convert alert level to string
 *
 * WHAT: Returns human-readable string for alert level
 * WHY:  Enables logging and debugging
 * HOW:  Switch on enum value
 *
 * @param level Alert level value
 * @return String representation (static, do not free)
 */
const char* kg_perf_alert_level_to_string(kg_perf_alert_level_t level);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_KG_PERF_AGENT_H */
