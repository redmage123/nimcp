/**
 * @file nimcp_mesh_resilience_integration.h
 * @brief Health Agent and Mesh Resilience Integration
 *
 * WHAT: Wires health agents to mesh network for distributed resilience
 * WHY:  Aggregate health monitoring through mesh for coordinated recovery
 * HOW:  Health agent registration, heartbeat routing, failure detection
 *
 * ARCHITECTURE:
 * ```
 * ┌─────────────────────────────────────────────────────────────────────────────┐
 * │                    MESH RESILIENCE INTEGRATION                              │
 * ├─────────────────────────────────────────────────────────────────────────────┤
 * │                                                                              │
 * │  ┌──────────────────────────────────────────────────────────────────────┐   │
 * │  │                      HEALTH AGENTS                                    │   │
 * │  │  ┌────────────┐ ┌────────────┐ ┌────────────┐ ┌────────────┐         │   │
 * │  │  │ Cognitive  │ │  Sensory   │ │   Motor    │ │    GPU     │  ...    │   │
 * │  │  │   Agent    │ │   Agent    │ │   Agent    │ │   Agent    │         │   │
 * │  │  └──────┬─────┘ └──────┬─────┘ └──────┬─────┘ └──────┬─────┘         │   │
 * │  │         │              │              │              │                │   │
 * │  └─────────┼──────────────┼──────────────┼──────────────┼────────────────┘   │
 * │            │              │              │              │                    │
 * │            ▼              ▼              ▼              ▼                    │
 * │  ┌──────────────────────────────────────────────────────────────────────┐   │
 * │  │                   MESH HEALTH BRIDGE                                  │   │
 * │  │  • Heartbeat aggregation                                              │   │
 * │  │  • Health score computation                                           │   │
 * │  │  • Status change detection                                            │   │
 * │  │  • Mesh routing of health updates                                     │   │
 * │  └──────────────────────────────────────────────────────────────────────┘   │
 * │                                    │                                        │
 * │                                    ▼                                        │
 * │  ┌──────────────────────────────────────────────────────────────────────┐   │
 * │  │                   COORDINATOR POOLS                                   │   │
 * │  │  • Receive health metrics                                             │   │
 * │  │  • Trigger leader election on failure                                 │   │
 * │  │  • Coordinate recovery actions                                        │   │
 * │  │  • Report to system health                                            │   │
 * │  └──────────────────────────────────────────────────────────────────────┘   │
 * │                                    │                                        │
 * │                                    ▼                                        │
 * │  ┌──────────────────────────────────────────────────────────────────────┐   │
 * │  │                   RECOVERY INTEGRATION                                │   │
 * │  │  • GPU recovery context                                               │   │
 * │  │  • Immune system integration                                          │   │
 * │  │  • Checkpoint/rollback                                                │   │
 * │  │  • Module restart                                                     │   │
 * │  └──────────────────────────────────────────────────────────────────────┘   │
 * │                                                                              │
 * └─────────────────────────────────────────────────────────────────────────────┘
 * ```
 *
 * BIOLOGICAL MOTIVATION:
 * - Distributed immune response across brain regions
 * - Glial cells monitor local health, coordinate globally
 * - Self-healing through coordinated recovery actions
 *
 * @author NIMCP Development Team
 * @date 2025-02-01
 * @version 1.0.0
 */

#ifndef NIMCP_MESH_RESILIENCE_INTEGRATION_H
#define NIMCP_MESH_RESILIENCE_INTEGRATION_H

#include "mesh/nimcp_mesh_types.h"
#include "mesh/nimcp_mesh_health_bridge.h"
#include "utils/error/nimcp_error_codes.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct mesh_resilience_integration mesh_resilience_integration_t;
typedef struct mesh_bootstrap mesh_bootstrap_t;
typedef struct mesh_coordinator_pool mesh_coordinator_pool_t;
typedef struct nimcp_health_agent nimcp_health_agent_t;
typedef struct gpu_recovery_context gpu_recovery_context_t;
typedef struct brain_immune_system brain_immune_system_t;

/* Alias for consistency */
typedef nimcp_health_agent_t health_agent_t;

/* ============================================================================
 * Constants
 * ============================================================================ */

/** @brief Magic number for structure validation */
#define MESH_RESILIENCE_MAGIC           0x52534C4E  /* "RSLN" */

/** @brief Maximum health agents tracked */
#define MESH_RESILIENCE_MAX_AGENTS      512

/** @brief Maximum recovery actions in queue */
#define MESH_RESILIENCE_MAX_ACTIONS     128

/** @brief Maximum failure events tracked */
#define MESH_RESILIENCE_MAX_FAILURES    256

/** @brief Default heartbeat aggregation interval (ms) */
#define MESH_RESILIENCE_HEARTBEAT_INTERVAL_MS   100

/** @brief Default failure detection timeout (ms) */
#define MESH_RESILIENCE_FAILURE_TIMEOUT_MS      5000

/* ============================================================================
 * Recovery Action Types
 * ============================================================================ */

/**
 * @brief Recovery action type
 */
typedef enum mesh_recovery_action_type {
    MESH_RECOVERY_NONE = 0,             /**< No action */
    MESH_RECOVERY_RESTART_MODULE,       /**< Restart specific module */
    MESH_RECOVERY_RESTART_CHANNEL,      /**< Restart entire channel */
    MESH_RECOVERY_TRIGGER_ELECTION,     /**< Force leader election */
    MESH_RECOVERY_GPU_RESET,            /**< GPU recovery reset */
    MESH_RECOVERY_CHECKPOINT,           /**< Create checkpoint */
    MESH_RECOVERY_ROLLBACK,             /**< Rollback to checkpoint */
    MESH_RECOVERY_QUARANTINE,           /**< Quarantine module */
    MESH_RECOVERY_IMMUNE_RESPONSE,      /**< Trigger immune response */
    MESH_RECOVERY_LOAD_SHED,            /**< Reduce system load */
    MESH_RECOVERY_GRACEFUL_DEGRADATION, /**< Enter degraded mode */
} mesh_recovery_action_type_t;

/**
 * @brief Failure severity level
 */
typedef enum mesh_failure_severity {
    MESH_FAILURE_NONE = 0,              /**< No failure */
    MESH_FAILURE_WARNING,               /**< Warning condition */
    MESH_FAILURE_DEGRADED,              /**< Degraded operation */
    MESH_FAILURE_CRITICAL,              /**< Critical failure */
    MESH_FAILURE_FATAL,                 /**< Fatal/unrecoverable */
} mesh_failure_severity_t;

/**
 * @brief Failure source type
 */
typedef enum mesh_failure_source {
    MESH_FAILURE_SRC_UNKNOWN = 0,       /**< Unknown source */
    MESH_FAILURE_SRC_HEARTBEAT,         /**< Missed heartbeats */
    MESH_FAILURE_SRC_MEMORY,            /**< Memory corruption */
    MESH_FAILURE_SRC_COMPUTATION,       /**< Computation error */
    MESH_FAILURE_SRC_GPU,               /**< GPU failure */
    MESH_FAILURE_SRC_NETWORK,           /**< Network/mesh issue */
    MESH_FAILURE_SRC_CONSENSUS,         /**< Consensus failure */
    MESH_FAILURE_SRC_COORDINATOR,       /**< Coordinator failure */
    MESH_FAILURE_SRC_RESOURCE,          /**< Resource exhaustion */
} mesh_failure_source_t;

/* ============================================================================
 * Health Agent Registration
 * ============================================================================ */

/**
 * @brief Health agent registration entry
 */
typedef struct mesh_agent_registration {
    mesh_participant_id_t participant_id;   /**< Associated participant */
    health_agent_t* agent;                  /**< Health agent handle */
    mesh_channel_id_t channel;              /**< Channel assignment */
    char module_name[MESH_MAX_NAME_LEN];    /**< Module name */

    /* Current status */
    mesh_health_status_t status;            /**< Current health status */
    float health_score;                     /**< Health score [0, 1] */
    uint64_t last_heartbeat_ns;             /**< Last heartbeat */
    uint32_t missed_heartbeats;             /**< Consecutive missed */

    /* Statistics */
    uint64_t heartbeats_received;           /**< Total heartbeats */
    uint64_t failures_detected;             /**< Failures detected */
    uint64_t recoveries_triggered;          /**< Recoveries triggered */

    /* State */
    bool active;                            /**< Registration active */
    uint64_t registered_at_ns;              /**< Registration time */
} mesh_agent_registration_t;

/**
 * @brief Failure event record
 */
typedef struct mesh_failure_event {
    uint64_t event_id;                      /**< Unique event ID */
    uint64_t timestamp_ns;                  /**< When detected */
    mesh_participant_id_t participant_id;   /**< Affected participant */
    mesh_channel_id_t channel;              /**< Affected channel */

    mesh_failure_severity_t severity;       /**< Failure severity */
    mesh_failure_source_t source;           /**< Failure source */

    char description[128];                  /**< Human-readable description */

    /* Recovery */
    mesh_recovery_action_type_t action;     /**< Recovery action taken */
    bool recovery_successful;               /**< Recovery succeeded */
    uint64_t recovery_duration_ns;          /**< Time to recover */
} mesh_failure_event_t;

/**
 * @brief Recovery action request
 */
typedef struct mesh_recovery_action {
    mesh_recovery_action_type_t type;       /**< Action type */
    mesh_participant_id_t target;           /**< Target participant */
    mesh_channel_id_t channel;              /**< Target channel */
    mesh_failure_severity_t severity;       /**< Trigger severity */
    uint64_t requested_at_ns;               /**< When requested */
    char reason[128];                       /**< Reason for action */
} mesh_recovery_action_t;

/* ============================================================================
 * Aggregated Health Metrics
 * ============================================================================ */

/**
 * @brief Aggregated channel health metrics
 */
typedef struct mesh_channel_health_metrics {
    mesh_channel_id_t channel;              /**< Channel ID */
    size_t total_agents;                    /**< Total agents in channel */
    size_t healthy_agents;                  /**< Healthy agents */
    size_t degraded_agents;                 /**< Degraded agents */
    size_t failed_agents;                   /**< Failed agents */

    float avg_health_score;                 /**< Average health score */
    float min_health_score;                 /**< Minimum health score */
    float max_health_score;                 /**< Maximum health score */

    uint64_t total_heartbeats;              /**< Total heartbeats in period */
    uint64_t missed_heartbeats;             /**< Missed heartbeats in period */
    float heartbeat_rate;                   /**< Heartbeats per second */

    mesh_health_status_t status;            /**< Overall channel status */
    uint64_t computed_at_ns;                /**< When computed */
} mesh_channel_health_metrics_t;

/**
 * @brief System-wide resilience metrics
 */
typedef struct mesh_system_resilience_metrics {
    mesh_health_status_t status;            /**< Overall system status */
    float system_health_score;              /**< System health [0, 1] */
    float resilience_score;                 /**< Resilience score [0, 1] */

    /* Agent counts */
    size_t total_agents;                    /**< Total registered agents */
    size_t healthy_agents;                  /**< Healthy agents */
    size_t degraded_agents;                 /**< Degraded agents */
    size_t critical_agents;                 /**< Critical agents */
    size_t failed_agents;                   /**< Failed agents */

    /* Channel metrics */
    mesh_channel_health_metrics_t channels[8];
    size_t channel_count;                   /**< Active channels */

    /* Failure statistics */
    uint64_t total_failures;                /**< Total failures detected */
    uint64_t total_recoveries;              /**< Total recovery attempts */
    uint64_t successful_recoveries;         /**< Successful recoveries */
    float recovery_success_rate;            /**< Recovery success rate */

    /* Timing */
    float avg_detection_latency_ms;         /**< Avg failure detection time */
    float avg_recovery_time_ms;             /**< Avg recovery time */

    uint64_t computed_at_ns;                /**< When computed */
} mesh_system_resilience_metrics_t;

/* ============================================================================
 * Configuration
 * ============================================================================ */

/**
 * @brief Resilience integration configuration
 */
typedef struct mesh_resilience_config {
    /* Heartbeat aggregation */
    uint64_t heartbeat_interval_ms;         /**< Aggregation interval */
    uint32_t missed_threshold;              /**< Missed before warning */
    uint32_t failure_threshold;             /**< Missed before failure */

    /* Failure detection */
    uint64_t failure_timeout_ms;            /**< Failure detection timeout */
    bool enable_auto_recovery;              /**< Enable automatic recovery */
    mesh_failure_severity_t auto_recovery_min; /**< Min severity for auto */

    /* Recovery integration */
    bool integrate_gpu_recovery;            /**< Integrate GPU recovery */
    bool integrate_immune_system;           /**< Integrate immune system */
    bool enable_checkpointing;              /**< Enable checkpoints */

    /* Routing */
    bool route_health_through_mesh;         /**< Route via mesh channels */
    bool aggregate_per_channel;             /**< Per-channel aggregation */

    /* Coordinator notification */
    bool notify_coordinator_pools;          /**< Notify pools on failure */
    bool trigger_elections_on_failure;      /**< Trigger elections on fail */

    /* Logging */
    bool verbose_logging;
} mesh_resilience_config_t;

/**
 * @brief Resilience integration statistics
 */
typedef struct mesh_resilience_stats {
    uint64_t agents_registered;             /**< Total agents registered */
    uint64_t heartbeats_aggregated;         /**< Heartbeats aggregated */
    uint64_t health_checks_performed;       /**< Health checks */
    uint64_t failures_detected;             /**< Failures detected */
    uint64_t recoveries_triggered;          /**< Recovery actions triggered */
    uint64_t recoveries_succeeded;          /**< Successful recoveries */
    uint64_t elections_triggered;           /**< Elections triggered */
    uint64_t gpu_recoveries;                /**< GPU recoveries */
    uint64_t immune_responses;              /**< Immune responses */

    float avg_health_score;                 /**< Average health score */
    float uptime_ratio;                     /**< System uptime ratio */

    uint64_t last_check_ns;                 /**< Last health check */
} mesh_resilience_stats_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default resilience configuration
 *
 * @param config Output configuration
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_resilience_default_config(
    mesh_resilience_config_t* config
);

/**
 * @brief Create resilience integration context
 *
 * WHAT: Initialize health agent mesh integration
 * WHY:  Coordinate distributed health monitoring
 * HOW:  Connect to health bridge, set up recovery paths
 *
 * @param bootstrap Mesh bootstrap handle
 * @param config Configuration (NULL for defaults)
 * @return Integration handle or NULL on failure
 */
mesh_resilience_integration_t* mesh_resilience_integration_create(
    mesh_bootstrap_t* bootstrap,
    const mesh_resilience_config_t* config
);

/**
 * @brief Destroy resilience integration context
 *
 * @param resilience Integration to destroy (NULL-safe)
 */
void mesh_resilience_integration_destroy(
    mesh_resilience_integration_t* resilience
);

/* ============================================================================
 * Health Agent Registration API
 * ============================================================================ */

/**
 * @brief Register health agent with mesh
 *
 * WHAT: Connect health agent to mesh health bridge
 * WHY:  Enable distributed health monitoring for module
 * HOW:  Create registration, wire to health bridge
 *
 * @param resilience Resilience integration handle
 * @param participant_id Associated mesh participant
 * @param agent Health agent handle
 * @param module_name Module name
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_resilience_register_agent(
    mesh_resilience_integration_t* resilience,
    mesh_participant_id_t participant_id,
    health_agent_t* agent,
    const char* module_name
);

/**
 * @brief Unregister health agent
 *
 * @param resilience Resilience integration handle
 * @param participant_id Participant to unregister
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_resilience_unregister_agent(
    mesh_resilience_integration_t* resilience,
    mesh_participant_id_t participant_id
);

/**
 * @brief Get agent registration
 *
 * @param resilience Resilience integration handle
 * @param participant_id Participant ID
 * @param registration Output registration
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_resilience_get_agent(
    const mesh_resilience_integration_t* resilience,
    mesh_participant_id_t participant_id,
    mesh_agent_registration_t* registration
);

/* ============================================================================
 * Heartbeat Aggregation API
 * ============================================================================ */

/**
 * @brief Process heartbeat from agent
 *
 * WHAT: Receive and aggregate heartbeat from health agent
 * WHY:  Track health status across mesh
 * HOW:  Update registration, route through mesh if enabled
 *
 * @param resilience Resilience integration handle
 * @param participant_id Source participant
 * @param op Heartbeat operation type
 * @param progress Progress value [0-100]
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_resilience_heartbeat(
    mesh_resilience_integration_t* resilience,
    mesh_participant_id_t participant_id,
    mesh_heartbeat_op_t op,
    uint8_t progress
);

/**
 * @brief Update health metrics for agent
 *
 * WHAT: Update detailed health metrics
 * WHY:  Rich health data for scoring
 * HOW:  Update registration metrics, recompute health score
 *
 * @param resilience Resilience integration handle
 * @param participant_id Participant ID
 * @param metrics Health metrics
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_resilience_update_metrics(
    mesh_resilience_integration_t* resilience,
    mesh_participant_id_t participant_id,
    const health_metrics_t* metrics
);

/**
 * @brief Check all registered agents for missed heartbeats
 *
 * WHAT: Periodic check for unresponsive agents
 * WHY:  Detect failures via heartbeat timeout
 * HOW:  Compare timestamps, mark failures, trigger recovery
 *
 * @param resilience Resilience integration handle
 * @return Number of new failures detected
 */
size_t mesh_resilience_check_heartbeats(
    mesh_resilience_integration_t* resilience
);

/**
 * @brief Aggregate health metrics for channel
 *
 * @param resilience Resilience integration handle
 * @param channel Channel ID
 * @param metrics Output metrics
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_resilience_aggregate_channel(
    mesh_resilience_integration_t* resilience,
    mesh_channel_id_t channel,
    mesh_channel_health_metrics_t* metrics
);

/**
 * @brief Get system-wide resilience metrics
 *
 * @param resilience Resilience integration handle
 * @param metrics Output metrics
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_resilience_get_system_metrics(
    mesh_resilience_integration_t* resilience,
    mesh_system_resilience_metrics_t* metrics
);

/* ============================================================================
 * Failure Detection and Recovery API
 * ============================================================================ */

/**
 * @brief Report failure for participant
 *
 * WHAT: Record failure event for participant
 * WHY:  Enable coordinated recovery response
 * HOW:  Create failure event, notify coordinator, trigger recovery
 *
 * @param resilience Resilience integration handle
 * @param participant_id Failed participant
 * @param severity Failure severity
 * @param source Failure source
 * @param description Human-readable description
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_resilience_report_failure(
    mesh_resilience_integration_t* resilience,
    mesh_participant_id_t participant_id,
    mesh_failure_severity_t severity,
    mesh_failure_source_t source,
    const char* description
);

/**
 * @brief Request recovery action
 *
 * WHAT: Queue recovery action for execution
 * WHY:  Coordinate recovery through mesh
 * HOW:  Add to recovery queue, execute via appropriate system
 *
 * @param resilience Resilience integration handle
 * @param action Recovery action to request
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_resilience_request_recovery(
    mesh_resilience_integration_t* resilience,
    const mesh_recovery_action_t* action
);

/**
 * @brief Execute pending recovery actions
 *
 * @param resilience Resilience integration handle
 * @return Number of actions executed
 */
size_t mesh_resilience_execute_recoveries(
    mesh_resilience_integration_t* resilience
);

/**
 * @brief Get recent failure events
 *
 * @param resilience Resilience integration handle
 * @param events Output events array (caller allocates)
 * @param max_events Maximum events to return
 * @param event_count Output: actual event count
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_resilience_get_failures(
    const mesh_resilience_integration_t* resilience,
    mesh_failure_event_t* events,
    size_t max_events,
    size_t* event_count
);

/* ============================================================================
 * Coordinator Pool Integration API
 * ============================================================================ */

/**
 * @brief Route health metrics to coordinator pool
 *
 * WHAT: Send aggregated health to coordinator pool
 * WHY:  Coordinators need health data for decisions
 * HOW:  Send via mesh channel transaction
 *
 * @param resilience Resilience integration handle
 * @param pool Target coordinator pool
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_resilience_route_to_coordinator(
    mesh_resilience_integration_t* resilience,
    mesh_coordinator_pool_t* pool
);

/**
 * @brief Trigger election on pool due to failure
 *
 * @param resilience Resilience integration handle
 * @param pool_id Pool to trigger election
 * @param reason Reason for election
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_resilience_trigger_election(
    mesh_resilience_integration_t* resilience,
    mesh_pool_id_t pool_id,
    const char* reason
);

/* ============================================================================
 * GPU Recovery Integration API
 * ============================================================================ */

/**
 * @brief Connect GPU recovery context
 *
 * WHAT: Wire GPU recovery to resilience system
 * WHY:  Coordinate GPU failures with mesh recovery
 * HOW:  Set callback, register recovery paths
 *
 * @param resilience Resilience integration handle
 * @param gpu_recovery GPU recovery context
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_resilience_connect_gpu_recovery(
    mesh_resilience_integration_t* resilience,
    gpu_recovery_context_t* gpu_recovery
);

/**
 * @brief Trigger GPU recovery
 *
 * @param resilience Resilience integration handle
 * @param reason Recovery reason
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_resilience_trigger_gpu_recovery(
    mesh_resilience_integration_t* resilience,
    const char* reason
);

/* ============================================================================
 * Immune System Integration API
 * ============================================================================ */

/**
 * @brief Connect immune system
 *
 * WHAT: Wire brain immune system to resilience
 * WHY:  Coordinate immune responses with mesh recovery
 * HOW:  Set callbacks, share health data
 *
 * @param resilience Resilience integration handle
 * @param immune Brain immune system
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_resilience_connect_immune(
    mesh_resilience_integration_t* resilience,
    brain_immune_system_t* immune
);

/**
 * @brief Trigger immune response
 *
 * @param resilience Resilience integration handle
 * @param participant_id Target participant
 * @param severity Response severity
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_resilience_trigger_immune_response(
    mesh_resilience_integration_t* resilience,
    mesh_participant_id_t participant_id,
    mesh_failure_severity_t severity
);

/* ============================================================================
 * Statistics API
 * ============================================================================ */

/**
 * @brief Get resilience statistics
 *
 * @param resilience Resilience integration handle
 * @param stats Output statistics
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_resilience_get_stats(
    const mesh_resilience_integration_t* resilience,
    mesh_resilience_stats_t* stats
);

/**
 * @brief Reset resilience statistics
 *
 * @param resilience Resilience integration handle
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_resilience_reset_stats(
    mesh_resilience_integration_t* resilience
);

/* ============================================================================
 * Bootstrap Integration
 * ============================================================================ */

/**
 * @brief Get resilience integration from bootstrap
 *
 * @param bootstrap Bootstrap handle
 * @return Resilience integration or NULL
 */
mesh_resilience_integration_t* mesh_bootstrap_get_resilience(
    mesh_bootstrap_t* bootstrap
);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Convert recovery action type to string
 *
 * @param type Recovery action type
 * @return Type string
 */
const char* mesh_recovery_action_to_string(mesh_recovery_action_type_t type);

/**
 * @brief Convert failure severity to string
 *
 * @param severity Failure severity
 * @return Severity string
 */
const char* mesh_failure_severity_to_string(mesh_failure_severity_t severity);

/**
 * @brief Convert failure source to string
 *
 * @param source Failure source
 * @return Source string
 */
const char* mesh_failure_source_to_string(mesh_failure_source_t source);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MESH_RESILIENCE_INTEGRATION_H */
