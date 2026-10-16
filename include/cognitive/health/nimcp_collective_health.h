/**
 * @file nimcp_collective_health.h
 * @brief Collective Health Monitoring - Distributed Health Across Brain Instances
 * @version 1.0.0
 * @date 2026-01-18
 *
 * WHAT: Distributed health monitoring using collective cognition infrastructure
 * WHY:  Enable swarm intelligence for anomaly detection and consensus-based recovery
 * HOW:  Integrate health agents with collective cognition, hyperscanning, and swarm systems
 *
 * PHASE 8: Section 27 - Collective & Recursive Cognition Integration
 *
 * KEY FEATURES:
 * 1. Collective Anomaly Consensus - Distributed agreement on anomaly detection
 * 2. Aggregated Health Scores - Weighted health across all instances
 * 3. Hyperscanning Health Sync - Neural sync for health state coordination
 * 4. Swarm Immune Coordination - Coordinated immune responses across instances
 *
 * ARCHITECTURE:
 * ```
 * +-----------------------------------------------------------------------------+
 * |                        COLLECTIVE HEALTH MONITORING                          |
 * +-----------------------------------------------------------------------------+
 * |  +----------------+  +----------------+  +----------------+  +------------+ |
 * |  | Health Agent 1 |  | Health Agent 2 |  | Health Agent N |  | Master HA  | |
 * |  |   (Brain 1)    |  |   (Brain 2)    |  |   (Brain N)    |  | (Aggregator)| |
 * |  +-------+--------+  +-------+--------+  +-------+--------+  +------+-----+ |
 * |          |                   |                   |                  |       |
 * |          +-------------------+-------------------+------------------+       |
 * |                              |                                              |
 * |  +--------------------------+v+------------------------------------------+ |
 * |  |                   HYPERSCANNING LAYER                                  | |
 * |  |   • Neural sync for health state      • Gamma-binding for anomalies   | |
 * |  |   • Theta sync for emotional health   • Leader-follower coordination  | |
 * |  +---------------------------+--------------------------------------------+ |
 * |                              |                                              |
 * |  +--------------------------+v+------------------------------------------+ |
 * |  |                COLLECTIVE COGNITION CORE                               | |
 * |  |   • Collective Phi (Integrated Health)   • Shared Goals (Recovery)    | |
 * |  |   • We-Mode Coordination                 • Extended Mind (Tools)      | |
 * |  +---------------------------+--------------------------------------------+ |
 * |                              |                                              |
 * |  +--------------------------+v+------------------------------------------+ |
 * |  |                SWARM IMMUNE COORDINATION                               | |
 * |  |   • Swarm-wide quarantine    • Antibody propagation                   | |
 * |  |   • Synchronized checkpoints • Load redistribution                    | |
 * |  +-----------------------------------------------------------------------+ |
 * +-----------------------------------------------------------------------------+
 * ```
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_COLLECTIVE_HEALTH_H
#define NIMCP_COLLECTIVE_HEALTH_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Include dependencies */
#include "utils/fault_tolerance/nimcp_health_agent.h"
#include "cognitive/collective_cognition/nimcp_collective_cognition.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Maximum instances in a health collective */
#define COLLECTIVE_HEALTH_MAX_INSTANCES         16

/** Maximum pending consensus requests */
#define COLLECTIVE_HEALTH_MAX_PENDING_CONSENSUS 64

/** Default consensus timeout (ms) */
#define COLLECTIVE_HEALTH_DEFAULT_CONSENSUS_TIMEOUT_MS 500

/** Default anomaly consensus threshold */
#define COLLECTIVE_HEALTH_DEFAULT_ANOMALY_THRESHOLD 0.6f

/** Default recovery quorum threshold */
#define COLLECTIVE_HEALTH_DEFAULT_RECOVERY_QUORUM 0.75f

/* ============================================================================
 * Collective Health Configuration
 * ============================================================================ */

/**
 * @brief Configuration for collective health monitoring
 */
typedef struct {
    /** Enable hyperscanning for health state sync */
    bool enable_hyperscanning;

    /** Enable collective phi for integrated health information */
    bool enable_collective_phi;

    /** Enable shared goals for recovery coordination */
    bool enable_shared_goals;

    /** Enable we-mode for coordinated recovery */
    bool enable_we_mode_recovery;

    /* Consensus settings */
    /** Agreement threshold for anomaly detection (0.5-1.0) */
    float anomaly_consensus_threshold;

    /** Quorum threshold for recovery approval (0.5-1.0) */
    float recovery_quorum_threshold;

    /** Maximum time to reach consensus (ms) */
    uint32_t max_consensus_time_ms;

    /* Health integration */
    /** Aggregate health scores from all instances */
    bool aggregate_health_scores;

    /** Share failure predictions across collective */
    bool share_failure_predictions;

    /** Sync immune memory across collective */
    bool propagate_immune_memory;

    /* Weighting */
    /** Weight for local health score vs collective */
    float local_weight;

    /** Weight for instance reliability in aggregation */
    bool use_reliability_weighting;

} collective_health_config_t;

/* ============================================================================
 * Anomaly Consensus Types
 * ============================================================================ */

/**
 * @brief Anomaly proposal for collective consensus
 */
typedef struct {
    /** The anomaly detected by local agent */
    health_agent_msg_type_t anomaly_type;

    /** Source of the anomaly */
    health_agent_source_t source;

    /** Severity assessed by local agent */
    health_agent_severity_t severity;

    /** Instance ID that detected the anomaly */
    uint32_t instance_id;

    /** Local confidence in detection (0.0-1.0) */
    float local_confidence;

    /** Timestamp of detection */
    uint64_t detection_time_us;

    /** Additional context data */
    uint8_t context_data[256];
    size_t context_size;

    /** Suggested recovery action */
    health_agent_recovery_t suggested_recovery;

} collective_anomaly_proposal_t;

/**
 * @brief Result of collective anomaly consensus
 */
typedef struct {
    /** Whether consensus was reached */
    bool consensus_reached;

    /** Confidence level of consensus (0.0-1.0) */
    float consensus_confidence;

    /** Number of instances that agreed */
    uint32_t agreeing_instances;

    /** Total instances in collective */
    uint32_t total_instances;

    /** Agreed severity (may differ from proposal) */
    health_agent_severity_t agreed_severity;

    /** Agreed recovery action */
    health_agent_recovery_t agreed_recovery;

    /** Collective phi at time of consensus */
    float collective_phi;

    /** Time taken to reach consensus (ms) */
    uint32_t consensus_time_ms;

    /** Whether consensus timed out */
    bool timed_out;

    /** Reason for disagreement (if no consensus) */
    char disagreement_reason[128];

} collective_anomaly_consensus_t;

/* ============================================================================
 * Health Summary Types
 * ============================================================================ */

/**
 * @brief Aggregated health summary across collective
 */
typedef struct {
    /** Weighted average health score (0.0-1.0) */
    float collective_health_score;

    /** Integrated information about health (IIT phi) */
    float collective_phi;

    /** Number of healthy instances */
    uint32_t healthy_instances;

    /** Number of degraded instances */
    uint32_t degraded_instances;

    /** Number of failed instances */
    uint32_t failed_instances;

    /** Severity of most severe active issue */
    health_agent_severity_t most_severe_issue;

    /** Source of most common issue */
    health_agent_source_t most_common_issue_source;

    /** Whether network fragmentation detected */
    bool is_fragmented;

    /** Whether collective is overloaded */
    bool is_overloaded;

    /** Current leader instance (if any) */
    uint32_t leader_instance_id;

    /** Leader's influence score */
    float leader_influence;

    /** Total active anomalies across collective */
    uint32_t total_active_anomalies;

    /** Average failure prediction across instances */
    float avg_failure_probability;

    /** Timestamp of summary */
    uint64_t timestamp_us;

} collective_health_summary_t;

/* ============================================================================
 * Swarm Immune Action Types
 * ============================================================================ */

/**
 * @brief Swarm-level immune actions
 */
typedef enum {
    /** No swarm action */
    SWARM_IMMUNE_NONE = 0,

    /** Isolate a misbehaving instance */
    SWARM_IMMUNE_QUARANTINE_INSTANCE,

    /** Share antibody pattern across swarm */
    SWARM_IMMUNE_PROPAGATE_ANTIBODY,

    /** Coordinated garbage collection */
    SWARM_IMMUNE_COLLECTIVE_GC,

    /** All instances checkpoint together */
    SWARM_IMMUNE_SYNCHRONIZED_CHECKPOINT,

    /** Move load from sick to healthy instances */
    SWARM_IMMUNE_LOAD_REDISTRIBUTE,

    /** Sync immune memory cells across instances */
    SWARM_IMMUNE_MEMORY_SYNC,

    /** Coordinated rollback */
    SWARM_IMMUNE_COORDINATED_ROLLBACK,

    /** Evict compromised instance from collective */
    SWARM_IMMUNE_EVICT_INSTANCE,

    SWARM_IMMUNE_ACTION_COUNT
} swarm_immune_action_t;

/**
 * @brief Request for swarm immune action
 */
typedef struct {
    /** Action type */
    swarm_immune_action_t action;

    /** Target instance ID (0 = all instances) */
    uint32_t target_instance_id;

    /** Urgency (0.0-1.0), affects quorum timeout */
    float urgency;

    /** Reason for action */
    char reason[128];

    /** Action-specific parameters */
    void* action_params;
    size_t action_params_size;

    /** Requesting instance ID */
    uint32_t requesting_instance_id;

    /** Associated anomaly (if any) */
    health_agent_msg_type_t related_anomaly;

} swarm_immune_request_t;

/**
 * @brief Response to swarm immune action request
 */
typedef struct {
    /** Whether action was approved by quorum */
    bool approved;

    /** Number of approving instances */
    uint32_t approving_instances;

    /** Total instances that voted */
    uint32_t total_instances;

    /** Number of instances executing the action */
    uint32_t executing_instances;

    /** Collective confidence in action */
    float collective_confidence;

    /** Time taken for approval (ms) */
    uint32_t approval_time_ms;

    /** Reason for rejection (if not approved) */
    char rejection_reason[128];

} swarm_immune_response_t;

/* ============================================================================
 * Instance Health Report
 * ============================================================================ */

/**
 * @brief Health report from a single instance
 */
typedef struct {
    /** Instance ID */
    uint32_t instance_id;

    /** Overall health score (0.0-1.0) */
    float health_score;

    /** Failure probability (0.0-1.0) */
    float failure_probability;

    /** Time to failure estimate (ms, 0 = unknown) */
    uint32_t time_to_failure_ms;

    /** Number of active anomalies */
    uint32_t active_anomalies;

    /** Most severe anomaly severity */
    health_agent_severity_t max_severity;

    /** Current recovery action in progress */
    health_agent_recovery_t current_recovery;

    /** Immune system inflammation level */
    uint8_t inflammation_level;

    /** Resource utilization (0.0-1.0) */
    float resource_utilization;

    /** Reliability score (0.0-1.0) */
    float reliability_score;

    /** Last heartbeat timestamp */
    uint64_t last_heartbeat_us;

    /** Whether instance is responsive */
    bool is_responsive;

} instance_health_report_t;

/* ============================================================================
 * Opaque Handle
 * ============================================================================ */

/** Collective health monitor handle */
typedef struct collective_health_monitor collective_health_monitor_t;

/* ============================================================================
 * Configuration API
 * ============================================================================ */

/**
 * @brief Get default collective health configuration
 * @return Default configuration
 */
collective_health_config_t collective_health_default_config(void);

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Create collective health monitor
 *
 * @param local_agent Local health agent for this instance
 * @param collective Collective cognition system
 * @param config Configuration (NULL for defaults)
 * @return Monitor handle or NULL on failure
 */
collective_health_monitor_t* collective_health_monitor_create(
    nimcp_health_agent_t* local_agent,
    collective_cognition_t* collective,
    const collective_health_config_t* config
);

/**
 * @brief Destroy collective health monitor
 * @param monitor Monitor to destroy
 */
void collective_health_monitor_destroy(collective_health_monitor_t* monitor);

/**
 * @brief Start collective health monitoring
 * @param monitor Monitor handle
 * @return 0 on success, -1 on error
 */
int collective_health_monitor_start(collective_health_monitor_t* monitor);

/**
 * @brief Stop collective health monitoring
 * @param monitor Monitor handle
 * @return 0 on success, -1 on error
 */
int collective_health_monitor_stop(collective_health_monitor_t* monitor);

/**
 * @brief Check if monitor is running
 * @param monitor Monitor handle
 * @return true if running
 */
bool collective_health_monitor_is_running(const collective_health_monitor_t* monitor);

/* ============================================================================
 * Consensus API
 * ============================================================================ */

/**
 * @brief Propose anomaly to collective for consensus
 *
 * Distributes anomaly observation to collective and awaits consensus.
 * Returns when consensus reached or timeout.
 *
 * @param monitor Collective health monitor
 * @param proposal Anomaly proposal
 * @param consensus Output consensus result
 * @return 0 on success, -1 on error
 */
int collective_health_propose_anomaly(
    collective_health_monitor_t* monitor,
    const collective_anomaly_proposal_t* proposal,
    collective_anomaly_consensus_t* consensus
);

/**
 * @brief Propose anomaly asynchronously
 *
 * @param monitor Collective health monitor
 * @param proposal Anomaly proposal
 * @param request_id Output request ID for tracking
 * @return 0 on success, -1 on error
 */
int collective_health_propose_anomaly_async(
    collective_health_monitor_t* monitor,
    const collective_anomaly_proposal_t* proposal,
    uint64_t* request_id
);

/**
 * @brief Check status of async consensus request
 *
 * @param monitor Collective health monitor
 * @param request_id Request ID
 * @param consensus Output consensus result (if complete)
 * @return 1 if complete, 0 if pending, -1 on error
 */
int collective_health_check_consensus(
    collective_health_monitor_t* monitor,
    uint64_t request_id,
    collective_anomaly_consensus_t* consensus
);

/**
 * @brief Vote on a proposed anomaly from another instance
 *
 * @param monitor Collective health monitor
 * @param proposal Proposal to vote on
 * @param agree Whether this instance agrees
 * @param confidence Confidence in vote (0.0-1.0)
 * @return 0 on success, -1 on error
 */
int collective_health_vote_anomaly(
    collective_health_monitor_t* monitor,
    const collective_anomaly_proposal_t* proposal,
    bool agree,
    float confidence
);

/* ============================================================================
 * Health Summary API
 * ============================================================================ */

/**
 * @brief Get aggregated health summary across collective
 *
 * @param monitor Collective health monitor
 * @param summary Output health summary
 * @return 0 on success, -1 on error
 */
int collective_health_get_summary(
    const collective_health_monitor_t* monitor,
    collective_health_summary_t* summary
);

/**
 * @brief Get health report for specific instance
 *
 * @param monitor Collective health monitor
 * @param instance_id Instance to query
 * @param report Output health report
 * @return 0 on success, -1 on error
 */
int collective_health_get_instance_report(
    const collective_health_monitor_t* monitor,
    uint32_t instance_id,
    instance_health_report_t* report
);

/**
 * @brief Get health reports for all instances
 *
 * @param monitor Collective health monitor
 * @param reports Output array of reports
 * @param max_reports Maximum reports
 * @param num_reports Output number of reports
 * @return 0 on success, -1 on error
 */
int collective_health_get_all_reports(
    const collective_health_monitor_t* monitor,
    instance_health_report_t* reports,
    uint32_t max_reports,
    uint32_t* num_reports
);

/* ============================================================================
 * Swarm Immune API
 * ============================================================================ */

/**
 * @brief Request swarm immune action with quorum
 *
 * @param monitor Collective health monitor
 * @param request Action request
 * @param response Output response
 * @return 0 on success, -1 on error
 */
int collective_health_request_swarm_action(
    collective_health_monitor_t* monitor,
    const swarm_immune_request_t* request,
    swarm_immune_response_t* response
);

/**
 * @brief Request swarm immune action asynchronously
 *
 * @param monitor Collective health monitor
 * @param request Action request
 * @param request_id Output request ID
 * @return 0 on success, -1 on error
 */
int collective_health_request_swarm_action_async(
    collective_health_monitor_t* monitor,
    const swarm_immune_request_t* request,
    uint64_t* request_id
);

/**
 * @brief Check status of async swarm action request
 *
 * @param monitor Collective health monitor
 * @param request_id Request ID
 * @param response Output response (if complete)
 * @return 1 if complete, 0 if pending, -1 on error
 */
int collective_health_check_swarm_action(
    collective_health_monitor_t* monitor,
    uint64_t request_id,
    swarm_immune_response_t* response
);

/* ============================================================================
 * Hyperscanning Health API
 * ============================================================================ */

/**
 * @brief Get health synchronization state
 *
 * @param monitor Collective health monitor
 * @param sync_state Output hyperscan state
 * @return 0 on success, -1 on error
 */
int collective_health_get_sync_state(
    const collective_health_monitor_t* monitor,
    hyperscan_state_t* sync_state
);

/**
 * @brief Force health state synchronization
 *
 * Triggers immediate hyperscanning sync of health states.
 *
 * @param monitor Collective health monitor
 * @return 0 on success, -1 on error
 */
int collective_health_force_sync(collective_health_monitor_t* monitor);

/* ============================================================================
 * Collective Threat Detection
 * ============================================================================ */

/**
 * @brief Collective threat information
 */
typedef struct {
    /** Threat type (from swarm immune) */
    uint32_t threat_type;

    /** Severity */
    health_agent_severity_t severity;

    /** Number of instances detecting this threat */
    uint32_t detecting_instances;

    /** Confidence in detection */
    float confidence;

    /** Whether threat is confirmed by collective */
    bool confirmed;

    /** Recommended response */
    swarm_immune_action_t recommended_action;

    /** Detection timestamp */
    uint64_t detection_time_us;

    /** Source description */
    char source_description[128];

} collective_threat_t;

/**
 * @brief Callback for collective threat detection
 */
typedef void (*collective_threat_callback_t)(
    const collective_threat_t* threat,
    void* user_data
);

/**
 * @brief Register callback for collective threat detection
 *
 * @param monitor Collective health monitor
 * @param callback Callback function
 * @param user_data User data for callback
 * @return 0 on success, -1 on error
 */
int collective_health_register_threat_callback(
    collective_health_monitor_t* monitor,
    collective_threat_callback_t callback,
    void* user_data
);

/* ============================================================================
 * Failure Prediction Sharing
 * ============================================================================ */

/**
 * @brief Share failure prediction with collective
 *
 * @param monitor Collective health monitor
 * @param failure_probability Predicted failure probability (0.0-1.0)
 * @param time_to_failure_ms Estimated time to failure (ms)
 * @param source Source of predicted failure
 * @return 0 on success, -1 on error
 */
int collective_health_share_prediction(
    collective_health_monitor_t* monitor,
    float failure_probability,
    uint32_t time_to_failure_ms,
    health_agent_source_t source
);

/**
 * @brief Get worst failure prediction across collective
 *
 * @param monitor Collective health monitor
 * @param instance_id Output instance with worst prediction
 * @param failure_probability Output failure probability
 * @param time_to_failure_ms Output time to failure
 * @return 0 on success, -1 on error
 */
int collective_health_get_worst_prediction(
    const collective_health_monitor_t* monitor,
    uint32_t* instance_id,
    float* failure_probability,
    uint32_t* time_to_failure_ms
);

/* ============================================================================
 * Statistics
 * ============================================================================ */

/**
 * @brief Collective health statistics
 */
typedef struct {
    /** Total anomaly proposals */
    uint64_t anomalies_proposed;

    /** Successful consensus reached */
    uint64_t consensus_reached;

    /** Failed consensus (timeout/disagreement) */
    uint64_t consensus_failed;

    /** Swarm actions requested */
    uint64_t swarm_actions_requested;

    /** Swarm actions approved */
    uint64_t swarm_actions_approved;

    /** Swarm actions rejected */
    uint64_t swarm_actions_rejected;

    /** Total health syncs */
    uint64_t health_syncs;

    /** Average consensus time (ms) */
    float avg_consensus_time_ms;

    /** Average collective health score */
    float avg_collective_health;

    /** Peak collective phi */
    float peak_collective_phi;

    /** Number of quarantine events */
    uint64_t quarantine_events;

    /** Number of load redistributions */
    uint64_t load_redistributions;

} collective_health_stats_t;

/**
 * @brief Get collective health statistics
 *
 * @param monitor Collective health monitor
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int collective_health_get_stats(
    const collective_health_monitor_t* monitor,
    collective_health_stats_t* stats
);

/**
 * @brief Reset collective health statistics
 * @param monitor Collective health monitor
 */
void collective_health_reset_stats(collective_health_monitor_t* monitor);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Get swarm immune action name
 * @param action Action type
 * @return Human-readable action name
 */
const char* swarm_immune_action_name(swarm_immune_action_t action);

/**
 * @brief Initialize anomaly proposal with defaults
 * @param proposal Proposal to initialize
 */
void collective_health_init_proposal(collective_anomaly_proposal_t* proposal);

/**
 * @brief Initialize swarm immune request with defaults
 * @param request Request to initialize
 */
void collective_health_init_swarm_request(swarm_immune_request_t* request);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_COLLECTIVE_HEALTH_H */
