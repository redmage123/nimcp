/**
 * @file nimcp_health_cognitive_bridge.h
 * @brief Unified Health Agent Cognitive Integration Bridge
 * @version 1.0.0
 * @date 2026-01-18
 *
 * WHAT: Unified bridge connecting health agent to all cognitive systems
 * WHY:  Single point of integration for collective cognition, RCOG, and meta-health
 * HOW:  Coordinates all cognitive subsystems for intelligent health management
 *
 * PHASE 8: Section 27 - Collective & Recursive Cognition Integration
 *
 * KEY FEATURES:
 * 1. Unified Configuration - Single config for all cognitive integration
 * 2. Intelligent Anomaly Handling - Automatic routing to appropriate subsystem
 * 3. Coordinated Recovery - Collective + RCOG + Ethics for recovery decisions
 * 4. Continuous Improvement - Meta-health reflection with auto-apply
 *
 * INTEGRATION ARCHITECTURE:
 * ```
 * +----------------------------------------------------------------------+
 * |                 HEALTH AGENT COGNITIVE BRIDGE                        |
 * +----------------------------------------------------------------------+
 * |                                                                      |
 * |                      +------------------+                            |
 * |                      |  HEALTH AGENT    |                            |
 * |                      |  (Core System)   |                            |
 * |                      +--------+---------+                            |
 * |                               |                                      |
 * |                      +--------v---------+                            |
 * |                      |  COGNITIVE       |                            |
 * |                      |  BRIDGE          |                            |
 * |                      +--------+---------+                            |
 * |                               |                                      |
 * |         +---------------------+---------------------+                |
 * |         |                     |                     |                |
 * |  +------v------+       +------v------+       +------v------+         |
 * |  | COLLECTIVE  |       |    RCOG     |       | META-HEALTH |         |
 * |  | HEALTH      |       |   HEALTH    |       | REFLECTION  |         |
 * |  +------+------+       +------+------+       +------+------+         |
 * |         |                     |                     |                |
 * |  +------v------+       +------v------+       +------v------+         |
 * |  |Swarm Immune |       | Diagnosis   |       | Self-Assess |         |
 * |  |Consensus    |       | Recovery    |       | Learn       |         |
 * |  |Aggregation  |       | Planning    |       | Improve     |         |
 * |  +-------------+       +-------------+       +-------------+         |
 * |                                                                      |
 * +----------------------------------------------------------------------+
 * ```
 *
 * INTELLIGENT HANDLING FLOW:
 * 1. Anomaly detected by health agent
 * 2. If collective enabled: Propose to collective for consensus
 * 3. If consensus (or not collective): Submit to RCOG for diagnosis
 * 4. RCOG decomposes, investigates, suggests recovery
 * 5. Recovery plan checked by ethics (from Phase 9)
 * 6. If swarm enabled: Request quorum for recovery action
 * 7. If quorum: Execute recovery
 * 8. Record outcome for meta-health reflection
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_HEALTH_COGNITIVE_BRIDGE_H
#define NIMCP_HEALTH_COGNITIVE_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Include all cognitive health headers */
#include "cognitive/health/nimcp_collective_health.h"
#include "cognitive/health/nimcp_rcog_health.h"
#include "cognitive/health/nimcp_meta_health.h"
#include "utils/fault_tolerance/nimcp_health_agent.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Maximum pending intelligent handling requests */
#define COGNITIVE_BRIDGE_MAX_PENDING         64

/** Default handling timeout (ms) */
#define COGNITIVE_BRIDGE_DEFAULT_TIMEOUT_MS  60000

/* ============================================================================
 * Configuration
 * ============================================================================ */

/**
 * @brief Unified cognitive integration configuration
 */
typedef struct {
    /* Collective integration */
    /** Enable collective health monitoring */
    bool enable_collective_monitoring;

    /** Enable consensus for anomaly detection */
    bool enable_consensus_decisions;

    /** Enable swarm immune coordination */
    bool enable_swarm_immune;

    /** Collective health configuration */
    collective_health_config_t collective_config;

    /* RCOG integration */
    /** Enable RCOG for diagnosis */
    bool enable_rcog_diagnosis;

    /** Enable RCOG for recovery planning */
    bool enable_rcog_recovery_planning;

    /** RCOG timeout (ms) */
    uint32_t rcog_timeout_ms;

    /** RCOG confidence threshold */
    float rcog_confidence_threshold;

    /** RCOG health configuration */
    rcog_health_config_t rcog_config;

    /* Meta-health integration */
    /** Enable meta-health reflection */
    bool enable_meta_reflection;

    /** Enable automatic application of learnings */
    bool enable_auto_learn;

    /** Meta-health configuration */
    meta_health_config_t meta_config;

    /* Imagination integration */
    /** Enable imagination for what-if planning */
    bool enable_imagination_for_planning;

    /** Maximum imagination steps */
    uint32_t max_imagination_steps;

    /* Global settings */
    /** Default handling timeout (ms) */
    uint32_t default_timeout_ms;

    /** Whether to require consensus before recovery */
    bool require_consensus_for_recovery;

    /** Whether to require quorum for swarm actions */
    bool require_quorum_for_swarm;

    /** Ethics validation (integrates with Phase 9) */
    bool enable_ethics_validation;

    /** Emotion-aware thresholds (integrates with Phase 9) */
    bool enable_emotion_awareness;

} cognitive_bridge_config_t;

/**
 * @brief Get default cognitive bridge configuration
 * @return Default configuration
 */
cognitive_bridge_config_t health_cognitive_bridge_default_config(void);

/* ============================================================================
 * Cognitive Status Types
 * ============================================================================ */

/**
 * @brief Cognitive processing status
 */
typedef struct {
    /** Whether collective is connected */
    bool collective_connected;

    /** Whether RCOG is connected */
    bool rcog_connected;

    /** Whether meta-health is active */
    bool meta_health_active;

    /** Pending consensus requests */
    uint32_t pending_consensus_requests;

    /** Pending RCOG goals */
    uint32_t pending_rcog_goals;

    /** Completed reflections */
    uint32_t completed_reflections;

    /** Average cognitive processing time (ms) */
    float avg_cognitive_processing_time_ms;

    /** Collective health score (if connected) */
    float collective_health_score;

    /** Current collective phi */
    float collective_phi;

    /** Meta-health accuracy rate */
    float meta_health_accuracy;

    /** Active cognitive mode */
    char active_mode[32];

} cognitive_bridge_status_t;

/* ============================================================================
 * Intelligent Handling Types
 * ============================================================================ */

/**
 * @brief Result of intelligent anomaly handling
 */
typedef struct {
    /** Whether handling succeeded */
    bool success;

    /** Error message if failed */
    char error_message[256];

    /* Consensus stage */
    /** Whether consensus was required */
    bool consensus_required;

    /** Whether consensus was reached */
    bool consensus_reached;

    /** Consensus confidence */
    float consensus_confidence;

    /* Diagnosis stage */
    /** Whether diagnosis was performed */
    bool diagnosis_performed;

    /** Diagnosis result (if performed) */
    rcog_health_diagnosis_t diagnosis;

    /* Recovery stage */
    /** Whether recovery was planned */
    bool recovery_planned;

    /** Recovery plan (if planned) */
    rcog_health_recovery_plan_t recovery_plan;

    /** Whether ethics approved the action */
    bool ethics_approved;

    /** Ethics rejection reason (if not approved) */
    char ethics_rejection[128];

    /* Execution stage */
    /** Whether recovery was executed */
    bool recovery_executed;

    /** Whether swarm quorum was obtained */
    bool swarm_quorum_obtained;

    /** Recovery outcome */
    meta_health_outcome_t outcome;

    /* Statistics */
    /** Total processing time (ms) */
    uint32_t total_processing_time_ms;

    /** Consensus time (ms) */
    uint32_t consensus_time_ms;

    /** Diagnosis time (ms) */
    uint32_t diagnosis_time_ms;

    /** Recovery time (ms) */
    uint32_t recovery_time_ms;

} intelligent_handling_result_t;

/* ============================================================================
 * Cognitive Bridge Handle
 * ============================================================================ */

/** Health cognitive bridge handle */
typedef struct health_cognitive_bridge health_cognitive_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Connect health agent to cognitive systems
 *
 * Creates and initializes the cognitive bridge with all subsystems.
 *
 * @param agent Health agent to connect
 * @param collective Collective cognition system (optional)
 * @param rcog RCOG engine (optional)
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
health_cognitive_bridge_t* health_cognitive_bridge_create(
    nimcp_health_agent_t* agent,
    collective_cognition_t* collective,
    rcog_engine_t* rcog,
    const cognitive_bridge_config_t* config
);

/**
 * @brief Destroy cognitive bridge
 * @param bridge Bridge to destroy
 */
void health_cognitive_bridge_destroy(health_cognitive_bridge_t* bridge);

/**
 * @brief Start cognitive integration
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int health_cognitive_bridge_start(health_cognitive_bridge_t* bridge);

/**
 * @brief Stop cognitive integration
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int health_cognitive_bridge_stop(health_cognitive_bridge_t* bridge);

/**
 * @brief Check if bridge is running
 * @param bridge Bridge handle
 * @return true if running
 */
bool health_cognitive_bridge_is_running(const health_cognitive_bridge_t* bridge);

/* ============================================================================
 * Intelligent Handling API
 * ============================================================================ */

/**
 * @brief Intelligent anomaly handling using cognitive systems
 *
 * Full cognitive pipeline:
 * 1. Propose anomaly to collective for consensus
 * 2. If consensus: Submit to RCOG for diagnosis
 * 3. RCOG decomposes, investigates, suggests recovery
 * 4. Recovery plan validated by ethics (if enabled)
 * 5. Recovery plan proposed to swarm for quorum
 * 6. If quorum: Execute recovery
 * 7. Record outcome for meta-health reflection
 *
 * @param bridge Cognitive bridge
 * @param anomaly_type Type of anomaly detected
 * @param source Source of anomaly
 * @param severity Severity level
 * @param result Output handling result
 * @return 0 on success, -1 on error
 */
int health_cognitive_intelligent_handle(
    health_cognitive_bridge_t* bridge,
    health_agent_msg_type_t anomaly_type,
    health_agent_source_t source,
    health_agent_severity_t severity,
    intelligent_handling_result_t* result
);

/**
 * @brief Intelligent handling asynchronously
 *
 * @param bridge Cognitive bridge
 * @param anomaly_type Type of anomaly
 * @param source Source of anomaly
 * @param severity Severity level
 * @param request_id Output request ID
 * @return 0 on success, -1 on error
 */
int health_cognitive_intelligent_handle_async(
    health_cognitive_bridge_t* bridge,
    health_agent_msg_type_t anomaly_type,
    health_agent_source_t source,
    health_agent_severity_t severity,
    uint64_t* request_id
);

/**
 * @brief Check status of async handling
 *
 * @param bridge Cognitive bridge
 * @param request_id Request ID
 * @param result Output result (if complete)
 * @return 1 if complete, 0 if pending, -1 on error
 */
int health_cognitive_check_handling(
    health_cognitive_bridge_t* bridge,
    uint64_t request_id,
    intelligent_handling_result_t* result
);

/* ============================================================================
 * Component Access API
 * ============================================================================ */

/**
 * @brief Get collective health monitor
 * @param bridge Cognitive bridge
 * @return Collective health monitor or NULL
 */
collective_health_monitor_t* health_cognitive_get_collective(
    health_cognitive_bridge_t* bridge
);

/**
 * @brief Get RCOG health integration
 * @param bridge Cognitive bridge
 * @return RCOG health integration or NULL
 */
rcog_health_integration_t* health_cognitive_get_rcog(
    health_cognitive_bridge_t* bridge
);

/**
 * @brief Get meta-health reflector
 * @param bridge Cognitive bridge
 * @return Meta-health reflector or NULL
 */
meta_health_reflector_t* health_cognitive_get_meta(
    health_cognitive_bridge_t* bridge
);

/* ============================================================================
 * Status API
 * ============================================================================ */

/**
 * @brief Get cognitive processing status
 *
 * @param bridge Cognitive bridge
 * @param status Output status
 * @return 0 on success, -1 on error
 */
int health_cognitive_get_status(
    const health_cognitive_bridge_t* bridge,
    cognitive_bridge_status_t* status
);

/* ============================================================================
 * Manual Control API
 * ============================================================================ */

/**
 * @brief Force immediate meta-health reflection
 *
 * @param bridge Cognitive bridge
 * @param result Output reflection result
 * @return 0 on success, -1 on error
 */
int health_cognitive_force_reflection(
    health_cognitive_bridge_t* bridge,
    meta_health_reflection_result_t* result
);

/**
 * @brief Force health state synchronization
 *
 * @param bridge Cognitive bridge
 * @return 0 on success, -1 on error
 */
int health_cognitive_force_sync(health_cognitive_bridge_t* bridge);

/**
 * @brief Trigger diagnosis without recovery
 *
 * @param bridge Cognitive bridge
 * @param anomaly_type Type of anomaly
 * @param source Source of anomaly
 * @param severity Severity level
 * @param diagnosis Output diagnosis
 * @return 0 on success, -1 on error
 */
int health_cognitive_diagnose_only(
    health_cognitive_bridge_t* bridge,
    health_agent_msg_type_t anomaly_type,
    health_agent_source_t source,
    health_agent_severity_t severity,
    rcog_health_diagnosis_t* diagnosis
);

/**
 * @brief Get recovery plan without execution
 *
 * @param bridge Cognitive bridge
 * @param anomaly_type Type of anomaly
 * @param source Source of anomaly
 * @param severity Severity level
 * @param plan Output recovery plan
 * @return 0 on success, -1 on error
 */
int health_cognitive_plan_only(
    health_cognitive_bridge_t* bridge,
    health_agent_msg_type_t anomaly_type,
    health_agent_source_t source,
    health_agent_severity_t severity,
    rcog_health_recovery_plan_t* plan
);

/* ============================================================================
 * Statistics API
 * ============================================================================ */

/**
 * @brief Cognitive bridge statistics
 */
typedef struct {
    /** Total anomalies handled */
    uint64_t anomalies_handled;

    /** Successful intelligent handling */
    uint64_t handling_success;

    /** Failed handling */
    uint64_t handling_failed;

    /** Consensus obtained */
    uint64_t consensus_obtained;

    /** Consensus failed/timeout */
    uint64_t consensus_failed;

    /** Diagnoses performed */
    uint64_t diagnoses_performed;

    /** Recovery plans executed */
    uint64_t recoveries_executed;

    /** Ethics approvals */
    uint64_t ethics_approved;

    /** Ethics rejections */
    uint64_t ethics_rejected;

    /** Swarm quorums obtained */
    uint64_t swarm_quorums;

    /** Reflections performed */
    uint64_t reflections_performed;

    /** Average total handling time (ms) */
    float avg_handling_time_ms;

    /** Average consensus time (ms) */
    float avg_consensus_time_ms;

    /** Average diagnosis time (ms) */
    float avg_diagnosis_time_ms;

    /** Recovery success rate */
    float recovery_success_rate;

    /** Current pending requests */
    uint32_t pending_requests;

} health_cognitive_stats_t;

/**
 * @brief Get cognitive bridge statistics
 *
 * @param bridge Cognitive bridge
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int health_cognitive_get_stats(
    const health_cognitive_bridge_t* bridge,
    health_cognitive_stats_t* stats
);

/**
 * @brief Reset cognitive bridge statistics
 * @param bridge Cognitive bridge
 */
void health_cognitive_reset_stats(health_cognitive_bridge_t* bridge);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Initialize handling result
 * @param result Result to initialize
 */
void health_cognitive_init_handling_result(intelligent_handling_result_t* result);

/**
 * @brief Dump handling result for debugging
 * @param result Result to dump
 */
void health_cognitive_dump_handling_result(const intelligent_handling_result_t* result);

/**
 * @brief Dump cognitive status for debugging
 * @param status Status to dump
 */
void health_cognitive_dump_status(const cognitive_bridge_status_t* status);

/* ============================================================================
 * Convenience Wrapper for Health Agent
 * ============================================================================ */

/**
 * @brief Connect health agent to cognitive systems (simplified)
 *
 * Wrapper that creates bridge and attaches it to health agent.
 *
 * @param agent Health agent
 * @param collective Collective cognition (optional)
 * @param rcog RCOG engine (optional)
 * @param config Configuration (NULL for defaults)
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_connect_cognitive(
    nimcp_health_agent_t* agent,
    collective_cognition_t* collective,
    rcog_engine_t* rcog,
    const cognitive_bridge_config_t* config
);

/**
 * @brief Disconnect health agent from cognitive systems
 *
 * @param agent Health agent
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_disconnect_cognitive(nimcp_health_agent_t* agent);

/**
 * @brief Get cognitive bridge status from health agent
 *
 * @param agent Health agent
 * @param status Output bridge status
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_get_bridge_status(
    const nimcp_health_agent_t* agent,
    cognitive_bridge_status_t* status
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_HEALTH_COGNITIVE_BRIDGE_H */
