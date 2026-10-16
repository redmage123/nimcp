/**
 * @file nimcp_swarm_cascade.h
 * @brief Cascading Failure Prevention System for NIMCP Swarms
 *
 * Biological Inspiration:
 * - Power grid stability with circuit breakers and load shedding
 * - Neural network robustness with graceful degradation
 * - Immune system response with isolation and recovery
 * - Homeostatic regulation maintaining system stability
 *
 * Features:
 * - Health state management (optimal, degraded, failing, failed, recovering)
 * - ML-based failure prediction and anomaly detection
 * - Circuit breakers to isolate failing components
 * - Graceful degradation with priority-based capability shedding
 * - Automatic recovery protocols with health verification
 * - Dynamic redundancy and hot standby management
 * - Cascade detection with pattern recognition
 * - Bio-async integration for distributed coordination
 */

#ifndef NIMCP_SWARM_CASCADE_H
#define NIMCP_SWARM_CASCADE_H

#include "utils/validation/nimcp_common.h"
#include "async/nimcp_bio_messages.h"
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#include "async/nimcp_bio_router.h"
#include "utils/thread/nimcp_thread.h"
#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Health State Definitions
 * ========================================================================== */

/**
 * @brief Health states for swarm nodes
 * Inspired by power grid stability levels
 */
typedef enum {
    HEALTH_OPTIMAL = 0,      /**< Full capability, all systems nominal */
    HEALTH_DEGRADED = 1,     /**< Reduced capability, some systems impaired */
    HEALTH_FAILING = 2,      /**< Imminent failure, critical systems affected */
    HEALTH_FAILED = 3,       /**< Non-operational, complete failure */
    HEALTH_RECOVERING = 4,   /**< Coming back online, restoring capability */
    HEALTH_UNKNOWN = 5       /**< Cannot determine health status */
} nimcp_health_state_t;

/**
 * @brief Failure severity levels
 */
typedef enum {
    FAILURE_SEVERITY_MINOR = 0,      /**< Minimal impact, no cascade risk */
    FAILURE_SEVERITY_MODERATE = 1,   /**< Some impact, low cascade risk */
    FAILURE_SEVERITY_MAJOR = 2,      /**< Significant impact, medium cascade risk */
    FAILURE_SEVERITY_CRITICAL = 3,   /**< Severe impact, high cascade risk */
    FAILURE_SEVERITY_CATASTROPHIC = 4 /**< System-wide impact, imminent cascade */
} nimcp_failure_severity_t;

/**
 * @brief Recovery strategies
 */
typedef enum {
    RECOVERY_IMMEDIATE = 0,   /**< Attempt immediate recovery */
    RECOVERY_GRADUAL = 1,     /**< Gradual capability restoration */
    RECOVERY_SUPERVISED = 2,  /**< Manual verification required */
    RECOVERY_ISOLATED = 3     /**< Keep isolated until cleared */
} nimcp_recovery_strategy_t;

/* ============================================================================
 * Telemetry and Metrics
 * ========================================================================== */

/**
 * @brief Health telemetry data for failure prediction
 * Collected continuously for anomaly detection
 */
typedef struct {
    double cpu_usage;              /**< CPU utilization (0.0-1.0) */
    double memory_usage;           /**< Memory utilization (0.0-1.0) */
    double network_latency_ms;     /**< Network round-trip time */
    double packet_loss_rate;       /**< Packet loss (0.0-1.0) */
    double error_rate;             /**< Error frequency */
    double processing_delay_ms;    /**< Message processing delay */
    double queue_depth;            /**< Message queue backlog */
    double temperature;            /**< System temperature (if available) */
    uint64_t successful_ops;       /**< Successful operations count */
    uint64_t failed_ops;           /**< Failed operations count */
    uint64_t timestamp_us;         /**< Microsecond timestamp */
} nimcp_health_telemetry_t;

/**
 * @brief Anomaly detection result
 */
typedef struct {
    bool is_anomalous;             /**< True if anomaly detected */
    double anomaly_score;          /**< Anomaly confidence (0.0-1.0) */
    double deviation_sigma;        /**< Standard deviations from normal */
    char metric_name[64];          /**< Which metric is anomalous */
    uint64_t detection_time_us;    /**< When anomaly was detected */
} nimcp_anomaly_detection_t;

/**
 * @brief Failure prediction result
 */
typedef struct {
    bool failure_predicted;        /**< True if failure predicted */
    double confidence;             /**< Prediction confidence (0.0-1.0) */
    uint64_t time_to_failure_us;   /**< Estimated time until failure */
    nimcp_failure_severity_t severity; /**< Predicted failure severity */
    char cause[128];               /**< Predicted failure cause */
    uint64_t prediction_time_us;   /**< When prediction was made */
} nimcp_failure_prediction_t;

/* ============================================================================
 * Circuit Breaker
 * ========================================================================== */

/**
 * @brief Circuit breaker states
 * Inspired by electrical circuit breakers
 */
typedef enum {
    BREAKER_CLOSED = 0,      /**< Normal operation, traffic flowing */
    BREAKER_OPEN = 1,        /**< Tripped, traffic blocked */
    BREAKER_HALF_OPEN = 2    /**< Testing recovery, limited traffic */
} nimcp_breaker_state_t;

/**
 * @brief Circuit breaker configuration
 */
typedef struct {
    uint32_t failure_threshold;    /**< Failures before trip */
    uint64_t timeout_us;           /**< Time before attempting reset */
    uint32_t success_threshold;    /**< Successes before closing */
    uint32_t half_open_max_calls;  /**< Max calls in half-open state */
} nimcp_breaker_config_t;

/**
 * @brief Circuit breaker state and statistics
 */
typedef struct {
    nimcp_breaker_state_t state;   /**< Current breaker state */
    uint32_t failure_count;        /**< Consecutive failures */
    uint32_t success_count;        /**< Consecutive successes */
    uint64_t last_failure_time_us; /**< Last failure timestamp */
    uint64_t trip_time_us;         /**< When breaker tripped */
    uint32_t total_trips;          /**< Lifetime trip count */
    nimcp_breaker_config_t config; /**< Breaker configuration */
} nimcp_circuit_breaker_t;

/* ============================================================================
 * Load Shedding and Graceful Degradation
 * ========================================================================== */

/**
 * @brief Capability priorities for load shedding
 */
typedef enum {
    PRIORITY_CRITICAL = 0,    /**< Core functions, never shed */
    PRIORITY_HIGH = 1,        /**< Important, shed only under duress */
    PRIORITY_MEDIUM = 2,      /**< Standard, shed when degraded */
    PRIORITY_LOW = 3,         /**< Nice-to-have, shed first */
    PRIORITY_BACKGROUND = 4   /**< Non-essential, always shed when stressed */
} nimcp_capability_priority_t;

/**
 * @brief Capability descriptor
 */
typedef struct {
    char name[64];                      /**< Capability name */
    nimcp_capability_priority_t priority; /**< Importance level */
    bool enabled;                       /**< Currently enabled */
    double resource_cost;               /**< Resource requirement (0.0-1.0) */
    uint64_t disable_time_us;           /**< When disabled (if applicable) */
} nimcp_capability_t;

/**
 * @brief Load shedding decision
 */
typedef struct {
    uint32_t capabilities_to_shed;     /**< Number of capabilities to disable */
    uint32_t capability_indices[32];   /**< Which capabilities to shed */
    double estimated_relief;           /**< Estimated resource relief (0.0-1.0) */
    nimcp_health_state_t target_state; /**< Expected state after shedding */
} nimcp_load_shedding_decision_t;

/* ============================================================================
 * Redundancy and Failover
 * ========================================================================== */

/**
 * @brief Node role in redundancy scheme
 */
typedef enum {
    ROLE_PRIMARY = 0,        /**< Active primary node */
    ROLE_HOT_STANDBY = 1,    /**< Ready to take over immediately */
    ROLE_WARM_STANDBY = 2,   /**< Can take over with brief delay */
    ROLE_COLD_STANDBY = 3,   /**< Backup, requires setup time */
    ROLE_NONE = 4            /**< No redundancy role */
} nimcp_redundancy_role_t;

/**
 * @brief Redundancy group configuration
 */
typedef struct {
    char group_name[64];               /**< Redundancy group identifier */
    uint32_t primary_node_id;          /**< Current primary node */
    uint32_t standby_node_ids[8];      /**< Standby nodes */
    uint32_t num_standbys;             /**< Number of standbys */
    nimcp_redundancy_role_t roles[8];  /**< Standby roles */
    uint64_t last_heartbeat_us;        /**< Last primary heartbeat */
    uint64_t failover_timeout_us;      /**< Time before failover */
} nimcp_redundancy_group_t;

/**
 * @brief Failover decision
 */
typedef struct {
    bool should_failover;              /**< True if failover needed */
    uint32_t failed_node_id;           /**< Node that failed */
    uint32_t new_primary_id;           /**< New primary node */
    char reason[128];                  /**< Failover reason */
    uint64_t decision_time_us;         /**< When decision was made */
} nimcp_failover_decision_t;

/* ============================================================================
 * Cascade Detection
 * ========================================================================== */

/**
 * @brief Failure event record
 */
typedef struct {
    uint32_t node_id;                  /**< Failed node */
    nimcp_health_state_t prev_state;   /**< State before failure */
    nimcp_health_state_t new_state;    /**< State after failure */
    nimcp_failure_severity_t severity; /**< Failure severity */
    uint64_t timestamp_us;             /**< When failure occurred */
    char description[128];             /**< Failure description */
} nimcp_failure_event_t;

/**
 * @brief Cascade detection result
 */
typedef struct {
    bool cascade_detected;             /**< True if cascade in progress */
    uint32_t affected_nodes;           /**< Number of affected nodes */
    double cascade_rate;               /**< Failures per second */
    double correlation_score;          /**< Failure correlation (0.0-1.0) */
    uint64_t cascade_start_us;         /**< When cascade began */
    nimcp_failure_event_t events[32];  /**< Recent failure events */
    uint32_t num_events;               /**< Number of events */
} nimcp_cascade_detection_t;

/* ============================================================================
 * Recovery Protocol
 * ========================================================================== */

/**
 * @brief Recovery phase
 */
typedef enum {
    RECOVERY_PHASE_ISOLATE = 0,      /**< Isolate failed component */
    RECOVERY_PHASE_DIAGNOSE = 1,     /**< Diagnose failure cause */
    RECOVERY_PHASE_REPAIR = 2,       /**< Attempt repair/restart */
    RECOVERY_PHASE_VERIFY = 3,       /**< Verify health before rejoining */
    RECOVERY_PHASE_REINTEGRATE = 4,  /**< Gradually restore capability */
    RECOVERY_PHASE_COMPLETE = 5      /**< Fully recovered */
} nimcp_recovery_phase_t;

/**
 * @brief Recovery state
 */
typedef struct {
    uint32_t node_id;                  /**< Recovering node */
    nimcp_recovery_phase_t phase;      /**< Current recovery phase */
    nimcp_recovery_strategy_t strategy; /**< Recovery approach */
    double progress;                   /**< Progress (0.0-1.0) */
    uint64_t recovery_start_us;        /**< When recovery began */
    uint64_t estimated_completion_us;  /**< Expected completion time */
    bool verification_passed;          /**< Health checks passed */
    char status_message[128];          /**< Recovery status */
} nimcp_recovery_state_t;

/* ============================================================================
 * Main Cascade Prevention System
 * ========================================================================== */

/**
 * @brief Cascade prevention system configuration
 */
typedef struct {
    /* Failure prediction settings */
    bool enable_ml_prediction;         /**< Enable ML-based prediction */
    double anomaly_threshold;          /**< Anomaly detection threshold */
    uint32_t telemetry_window_size;    /**< History window for predictions */

    /* Circuit breaker settings */
    nimcp_breaker_config_t default_breaker_config;

    /* Load shedding settings */
    bool enable_auto_shedding;         /**< Enable automatic load shedding */
    double shedding_threshold;         /**< Health threshold for shedding */

    /* Cascade detection settings */
    uint32_t cascade_window_ms;        /**< Time window for cascade detection */
    double cascade_rate_threshold;     /**< Failures/sec to trigger cascade alert */

    /* Recovery settings */
    nimcp_recovery_strategy_t default_recovery_strategy;
    uint64_t recovery_timeout_us;      /**< Max time for recovery */

    /* Redundancy settings */
    bool enable_auto_failover;         /**< Enable automatic failover */
    uint64_t heartbeat_interval_us;    /**< Heartbeat frequency */
} nimcp_cascade_config_t;

/**
 * @brief Historical telemetry for ML predictions
 */
typedef struct {
    nimcp_health_telemetry_t *samples; /**< Circular buffer of samples */
    uint32_t capacity;                 /**< Buffer capacity */
    uint32_t count;                    /**< Number of samples */
    uint32_t head;                     /**< Write position */

    /* Statistical baseline for anomaly detection */
    double mean_cpu;
    double mean_memory;
    double mean_latency;
    double std_cpu;
    double std_memory;
    double std_latency;
} nimcp_telemetry_history_t;

/**
 * @brief Cascade prevention system state
 */
typedef struct {
    /* Configuration */
    nimcp_cascade_config_t config;

    /* Node health tracking */
    uint32_t node_id;                  /**< This node's ID */
    nimcp_health_state_t health_state; /**< Current health state */
    nimcp_health_telemetry_t current_telemetry;
    nimcp_telemetry_history_t telemetry_history;

    /* Failure prediction */
    nimcp_failure_prediction_t last_prediction;
    nimcp_anomaly_detection_t last_anomaly;

    /* Circuit breakers (per dependent service) */
    nimcp_circuit_breaker_t *breakers; /**< Circuit breakers */
    uint32_t num_breakers;             /**< Number of breakers */
    uint32_t breaker_capacity;         /**< Breaker array capacity */

    /* Capabilities for load shedding */
    nimcp_capability_t *capabilities;  /**< Registered capabilities */
    uint32_t num_capabilities;         /**< Number of capabilities */
    uint32_t capability_capacity;      /**< Capability array capacity */

    /* Redundancy groups */
    nimcp_redundancy_group_t *groups;  /**< Redundancy groups */
    uint32_t num_groups;               /**< Number of groups */
    uint32_t group_capacity;           /**< Group array capacity */

    /* Cascade detection */
    nimcp_cascade_detection_t cascade_state;

    /* Recovery state */
    nimcp_recovery_state_t recovery_state;

    /* Bio-async integration */
    void *bio_ctx;                     /**< Bio-async context */
    bool bio_enabled;                  /**< Bio-async enabled */

    /* Statistics */
    uint64_t total_failures;           /**< Lifetime failure count */
    uint64_t cascades_prevented;       /**< Cascades stopped */
    uint64_t successful_recoveries;    /**< Successful recovery count */

    /* Synchronization */
    void *lock;                        /**< Thread safety lock */
} nimcp_cascade_system_t;

/* ============================================================================
 * API Functions
 * ========================================================================== */

/**
 * @brief Create cascade prevention system
 * @param config Configuration parameters
 * @param bio_ctx Bio-async context (can be NULL)
 * @return Cascade system instance or NULL on failure
 */
nimcp_cascade_system_t* nimcp_cascade_create(
    const nimcp_cascade_config_t *config,
    void *bio_ctx
);

/**
 * @brief Destroy cascade prevention system
 * @param system System to destroy
 */
void nimcp_cascade_destroy(nimcp_cascade_system_t *system);

/**
 * @brief Get default configuration
 * @param config Output configuration
 * @return NIMCP_OK on success
 */
nimcp_result_t nimcp_cascade_get_default_config(nimcp_cascade_config_t *config);

/* ============================================================================
 * Health Monitoring
 * ========================================================================== */

/**
 * @brief Update health telemetry
 * @param system Cascade system
 * @param telemetry Current telemetry data
 * @return NIMCP_OK on success
 */
nimcp_result_t nimcp_cascade_update_telemetry(
    nimcp_cascade_system_t *system,
    const nimcp_health_telemetry_t *telemetry
);

/**
 * @brief Get current health state
 * @param system Cascade system
 * @return Current health state
 */
nimcp_health_state_t nimcp_cascade_get_health_state(
    const nimcp_cascade_system_t *system
);

/**
 * @brief Convert health state to string
 * @param state Health state
 * @return String representation
 */
const char* nimcp_cascade_health_state_string(nimcp_health_state_t state);

/* ============================================================================
 * Failure Prediction
 * ========================================================================== */

/**
 * @brief Detect anomalies in telemetry
 * @param system Cascade system
 * @param telemetry Telemetry to analyze
 * @param result Output anomaly detection result
 * @return NIMCP_OK on success
 */
nimcp_result_t nimcp_cascade_detect_anomaly(
    nimcp_cascade_system_t *system,
    const nimcp_health_telemetry_t *telemetry,
    nimcp_anomaly_detection_t *result
);

/**
 * @brief Predict future failures
 * @param system Cascade system
 * @param prediction Output prediction result
 * @return NIMCP_OK on success
 */
nimcp_result_t nimcp_cascade_predict_failure(
    nimcp_cascade_system_t *system,
    nimcp_failure_prediction_t *prediction
);

/* ============================================================================
 * Circuit Breakers
 * ========================================================================== */

/**
 * @brief Register circuit breaker for a service
 * @param system Cascade system
 * @param service_name Service identifier
 * @param config Breaker configuration
 * @param breaker_id Output breaker ID
 * @return NIMCP_OK on success
 */
nimcp_result_t nimcp_cascade_register_breaker(
    nimcp_cascade_system_t *system,
    const char *service_name,
    const nimcp_breaker_config_t *config,
    uint32_t *breaker_id
);

/**
 * @brief Record operation result for circuit breaker
 * @param system Cascade system
 * @param breaker_id Breaker ID
 * @param success True if operation succeeded
 * @return NIMCP_OK on success
 */
nimcp_result_t nimcp_cascade_record_operation(
    nimcp_cascade_system_t *system,
    uint32_t breaker_id,
    bool success
);

/**
 * @brief Check if circuit breaker allows operation
 * @param system Cascade system
 * @param breaker_id Breaker ID
 * @param allowed Output: true if operation allowed
 * @return NIMCP_OK on success
 */
nimcp_result_t nimcp_cascade_check_breaker(
    nimcp_cascade_system_t *system,
    uint32_t breaker_id,
    bool *allowed
);

/**
 * @brief Get circuit breaker state
 * @param system Cascade system
 * @param breaker_id Breaker ID
 * @param breaker Output breaker state
 * @return NIMCP_OK on success
 */
nimcp_result_t nimcp_cascade_get_breaker_state(
    const nimcp_cascade_system_t *system,
    uint32_t breaker_id,
    nimcp_circuit_breaker_t *breaker
);

/* ============================================================================
 * Load Shedding
 * ========================================================================== */

/**
 * @brief Register capability for load shedding
 * @param system Cascade system
 * @param capability Capability descriptor
 * @param capability_id Output capability ID
 * @return NIMCP_OK on success
 */
nimcp_result_t nimcp_cascade_register_capability(
    nimcp_cascade_system_t *system,
    const nimcp_capability_t *capability,
    uint32_t *capability_id
);

/**
 * @brief Determine load shedding decision
 * @param system Cascade system
 * @param target_health Target health state to achieve
 * @param decision Output shedding decision
 * @return NIMCP_OK on success
 */
nimcp_result_t nimcp_cascade_decide_load_shedding(
    nimcp_cascade_system_t *system,
    nimcp_health_state_t target_health,
    nimcp_load_shedding_decision_t *decision
);

/**
 * @brief Apply load shedding decision
 * @param system Cascade system
 * @param decision Shedding decision to apply
 * @return NIMCP_OK on success
 */
nimcp_result_t nimcp_cascade_apply_load_shedding(
    nimcp_cascade_system_t *system,
    const nimcp_load_shedding_decision_t *decision
);

/**
 * @brief Restore shed capabilities
 * @param system Cascade system
 * @param num_to_restore Number of capabilities to restore
 * @return NIMCP_OK on success
 */
nimcp_result_t nimcp_cascade_restore_capabilities(
    nimcp_cascade_system_t *system,
    uint32_t num_to_restore
);

/* ============================================================================
 * Redundancy and Failover
 * ========================================================================== */

/**
 * @brief Register redundancy group
 * @param system Cascade system
 * @param group Redundancy group configuration
 * @param group_id Output group ID
 * @return NIMCP_OK on success
 */
nimcp_result_t nimcp_cascade_register_redundancy_group(
    nimcp_cascade_system_t *system,
    const nimcp_redundancy_group_t *group,
    uint32_t *group_id
);

/**
 * @brief Update heartbeat for redundancy group
 * @param system Cascade system
 * @param group_id Group ID
 * @param node_id Node sending heartbeat
 * @return NIMCP_OK on success
 */
nimcp_result_t nimcp_cascade_update_heartbeat(
    nimcp_cascade_system_t *system,
    uint32_t group_id,
    uint32_t node_id
);

/**
 * @brief Check if failover is needed
 * @param system Cascade system
 * @param group_id Group ID
 * @param decision Output failover decision
 * @return NIMCP_OK on success
 */
nimcp_result_t nimcp_cascade_check_failover(
    nimcp_cascade_system_t *system,
    uint32_t group_id,
    nimcp_failover_decision_t *decision
);

/**
 * @brief Execute failover
 * @param system Cascade system
 * @param decision Failover decision to execute
 * @return NIMCP_OK on success
 */
nimcp_result_t nimcp_cascade_execute_failover(
    nimcp_cascade_system_t *system,
    const nimcp_failover_decision_t *decision
);

/* ============================================================================
 * Cascade Detection
 * ========================================================================== */

/**
 * @brief Record failure event
 * @param system Cascade system
 * @param event Failure event to record
 * @return NIMCP_OK on success
 */
nimcp_result_t nimcp_cascade_record_failure(
    nimcp_cascade_system_t *system,
    const nimcp_failure_event_t *event
);

/**
 * @brief Detect cascade in progress
 * @param system Cascade system
 * @param detection Output cascade detection result
 * @return NIMCP_OK on success
 */
nimcp_result_t nimcp_cascade_detect_cascade(
    nimcp_cascade_system_t *system,
    nimcp_cascade_detection_t *detection
);

/* ============================================================================
 * Recovery
 * ========================================================================== */

/**
 * @brief Start recovery process
 * @param system Cascade system
 * @param node_id Node to recover
 * @param strategy Recovery strategy
 * @return NIMCP_OK on success
 */
nimcp_result_t nimcp_cascade_start_recovery(
    nimcp_cascade_system_t *system,
    uint32_t node_id,
    nimcp_recovery_strategy_t strategy
);

/**
 * @brief Update recovery progress
 * @param system Cascade system
 * @param phase Current recovery phase
 * @param progress Progress (0.0-1.0)
 * @return NIMCP_OK on success
 */
nimcp_result_t nimcp_cascade_update_recovery(
    nimcp_cascade_system_t *system,
    nimcp_recovery_phase_t phase,
    double progress
);

/**
 * @brief Verify node health before reintegration
 * @param system Cascade system
 * @param node_id Node to verify
 * @param passed Output: true if verification passed
 * @return NIMCP_OK on success
 */
nimcp_result_t nimcp_cascade_verify_health(
    nimcp_cascade_system_t *system,
    uint32_t node_id,
    bool *passed
);

/**
 * @brief Complete recovery and reintegrate node
 * @param system Cascade system
 * @param node_id Node to reintegrate
 * @return NIMCP_OK on success
 */
nimcp_result_t nimcp_cascade_complete_recovery(
    nimcp_cascade_system_t *system,
    uint32_t node_id
);

/**
 * @brief Get recovery state
 * @param system Cascade system
 * @param state Output recovery state
 * @return NIMCP_OK on success
 */
nimcp_result_t nimcp_cascade_get_recovery_state(
    const nimcp_cascade_system_t *system,
    nimcp_recovery_state_t *state
);

/* ============================================================================
 * Bio-Async Integration
 * ========================================================================== */

/**
 * @brief Enable bio-async integration
 * @param system Cascade system
 * @param bio_ctx Bio-async context
 * @return NIMCP_OK on success
 */
nimcp_result_t nimcp_cascade_enable_bio_async(
    nimcp_cascade_system_t *system,
    void *bio_ctx
);

/**
 * @brief Broadcast health status
 * @param system Cascade system
 * @return NIMCP_OK on success
 */
nimcp_result_t nimcp_cascade_broadcast_health(
    nimcp_cascade_system_t *system
);

/**
 * @brief Send failure alert
 * @param system Cascade system
 * @param event Failure event to broadcast
 * @return NIMCP_OK on success
 */
nimcp_result_t nimcp_cascade_send_failure_alert(
    nimcp_cascade_system_t *system,
    const nimcp_failure_event_t *event
);

/**
 * @brief Send recovery coordination message
 * @param system Cascade system
 * @param state Recovery state to broadcast
 * @return NIMCP_OK on success
 */
nimcp_result_t nimcp_cascade_send_recovery_message(
    nimcp_cascade_system_t *system,
    const nimcp_recovery_state_t *state
);

/* ============================================================================
 * Statistics and Monitoring
 * ========================================================================== */

/**
 * @brief Get system statistics
 * @param system Cascade system
 * @param total_failures Output: total failures
 * @param cascades_prevented Output: cascades prevented
 * @param successful_recoveries Output: successful recoveries
 * @return NIMCP_OK on success
 */
nimcp_result_t nimcp_cascade_get_statistics(
    const nimcp_cascade_system_t *system,
    uint64_t *total_failures,
    uint64_t *cascades_prevented,
    uint64_t *successful_recoveries
);

/**
 * @brief Get system health summary
 * @param system Cascade system
 * @param buffer Output buffer for summary
 * @param buffer_size Buffer size
 * @return NIMCP_OK on success
 */
nimcp_result_t nimcp_cascade_get_health_summary(
    const nimcp_cascade_system_t *system,
    char *buffer,
    size_t buffer_size
);

#include "async/nimcp_bio_router.h"
#include "utils/thread/nimcp_thread.h"
#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SWARM_CASCADE_H */
