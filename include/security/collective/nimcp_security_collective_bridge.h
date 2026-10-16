/**
 * @file nimcp_security_collective_bridge.h
 * @brief Security-Collective Cognition Integration Bridge
 * @version 1.0.0
 * @date 2025-01-10
 *
 * WHAT: Bidirectional integration between security subsystem and collective cognition
 * WHY:  Swarm/collective systems are vulnerable to Byzantine agents, consensus
 *       manipulation, and emergent behavior exploitation. Security must monitor
 *       and protect distributed consciousness while collective cognition benefits
 *       from threat detection and trust scoring.
 * HOW:  Security detects Byzantine agents, verifies consensus integrity, monitors
 *       swarm behavior, and validates emergent patterns. Collective cognition
 *       provides group activity data for anomaly detection.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * IMMUNE SYSTEM AND COLLECTIVE BEHAVIOR:
 * ---------------------------------------
 * - Social insect colonies use distributed immune responses
 * - Quorum sensing in bacteria detects group consensus
 * - Neural synchronization requires pattern validation
 * - Security = distributed immune system for collective consciousness
 *
 * SECURITY -> COLLECTIVE COGNITION PATHWAYS:
 * ------------------------------------------
 * 1. Byzantine Agent Detection:
 *    - Detect agents sending conflicting information
 *    - Identify compromised or malicious swarm members
 *    - Quarantine agents exhibiting anomalous behavior
 *
 * 2. Consensus Integrity Verification:
 *    - Validate consensus formation processes
 *    - Detect Sybil attacks (fake agent identities)
 *    - Verify voting/agreement protocols
 *
 * 3. Emergent Pattern Validation:
 *    - Validate emergent collective behaviors
 *    - Detect manipulation of emergence dynamics
 *    - Verify collective decisions are authentic
 *
 * COLLECTIVE COGNITION -> SECURITY PATHWAYS:
 * ------------------------------------------
 * 1. Swarm Behavior Monitoring:
 *    - Report collective activity patterns
 *    - Signal unusual synchronization events
 *    - Provide baseline behavior metrics
 *
 * 2. Agent Trust Scoring:
 *    - Track agent contribution history
 *    - Build trust based on collective feedback
 *    - Enable reputation-based security decisions
 *
 * ARCHITECTURE:
 * ```
 * +------------------------------------------------------------------+
 * |              SECURITY-COLLECTIVE COGNITION BRIDGE                |
 * +==================================================================+
 * |                                                                  |
 * |  +------------------------------------------------------------+  |
 * |  |          SECURITY -> COLLECTIVE PATHWAYS                   |  |
 * |  |                                                             |  |
 * |  |  +------------------+                                       |  |
 * |  |  | BYZANTINE        | --> Agent Isolation                  |  |
 * |  |  | DETECTOR         |     - Quarantine malicious agents    |  |
 * |  |  | - Conflict check |     - Block conflicting messages     |  |
 * |  |  +------------------+                                       |  |
 * |  |                                                             |  |
 * |  |  +------------------+                                       |  |
 * |  |  | CONSENSUS        | --> Integrity Enforcement            |  |
 * |  |  | VERIFIER         |     - Validate agreements            |  |
 * |  |  | - Sybil detect   |     - Prevent manipulation           |  |
 * |  |  +------------------+                                       |  |
 * |  |                                                             |  |
 * |  |  +------------------+                                       |  |
 * |  |  | EMERGENCE        | --> Behavior Validation              |  |
 * |  |  | VALIDATOR        |     - Pattern authenticity           |  |
 * |  |  | - Pattern check  |     - Collective decision verify     |  |
 * |  |  +------------------+                                       |  |
 * |  +------------------------------------------------------------+  |
 * |                                                                  |
 * |  +------------------------------------------------------------+  |
 * |  |          COLLECTIVE -> SECURITY PATHWAYS                   |  |
 * |  |                                                             |  |
 * |  |  +------------------+                                       |  |
 * |  |  | SWARM MONITOR    | --> Activity Reporting               |  |
 * |  |  | - Behavior track |     - Anomaly detection data         |  |
 * |  |  | - Sync events    |     - Baseline metrics               |  |
 * |  |  +------------------+                                       |  |
 * |  |                                                             |  |
 * |  |  +------------------+                                       |  |
 * |  |  | TRUST SCORER     | --> Reputation Data                  |  |
 * |  |  | - Agent history  |     - Trust-based decisions          |  |
 * |  |  | - Feedback       |     - Permission scaling             |  |
 * |  |  +------------------+                                       |  |
 * |  +------------------------------------------------------------+  |
 * |                                                                  |
 * +------------------------------------------------------------------+
 * ```
 *
 * NIMCP STANDARDS:
 * - All functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT-WHY-HOW documentation
 * - Thread-safe via mutex
 * - nimcp_malloc/nimcp_free memory management
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_SECURITY_COLLECTIVE_BRIDGE_H
#define NIMCP_SECURITY_COLLECTIVE_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/error/nimcp_error_codes.h"

/* Module integrations */
#include "cognitive/collective_cognition/nimcp_collective_cognition.h"
#include "security/nimcp_policy_engine.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Maximum agents tracked for Byzantine detection */
#define SECURITY_COLLECTIVE_MAX_AGENTS           128

/** Maximum consensus participants */
#define SECURITY_COLLECTIVE_MAX_CONSENSUS        64

/** Maximum emergent patterns tracked */
#define SECURITY_COLLECTIVE_MAX_PATTERNS         32

/** Maximum quarantined agents */
#define SECURITY_COLLECTIVE_MAX_QUARANTINE       32

/** Maximum trust history entries per agent */
#define SECURITY_COLLECTIVE_MAX_TRUST_HISTORY    64

/** Default Byzantine detection threshold */
#define SECURITY_COLLECTIVE_DEFAULT_BYZANTINE_THRESHOLD  0.3f

/** Default consensus verification interval (ms) */
#define SECURITY_COLLECTIVE_DEFAULT_VERIFY_INTERVAL      100

/** Default trust decay rate per interval */
#define SECURITY_COLLECTIVE_DEFAULT_TRUST_DECAY          0.01f

/** Magic number for validation */
#define SECURITY_COLLECTIVE_BRIDGE_MAGIC         0x53434252  /* 'SCBR' */

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct security_collective_bridge security_collective_bridge_t;

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Byzantine agent status
 */
typedef enum {
    BYZANTINE_STATUS_NORMAL = 0,         /**< Agent behaving normally */
    BYZANTINE_STATUS_SUSPECTED,          /**< Suspicious behavior detected */
    BYZANTINE_STATUS_CONFIRMED,          /**< Byzantine behavior confirmed */
    BYZANTINE_STATUS_QUARANTINED         /**< Agent is quarantined */
} byzantine_status_t;

/**
 * @brief Consensus verification result
 */
typedef enum {
    CONSENSUS_VALID = 0,                 /**< Consensus is valid */
    CONSENSUS_INVALID_QUORUM,            /**< Insufficient quorum */
    CONSENSUS_INVALID_SYBIL,             /**< Sybil attack detected */
    CONSENSUS_INVALID_MANIPULATION,      /**< Consensus manipulation detected */
    CONSENSUS_INVALID_TIMEOUT,           /**< Consensus timed out */
    CONSENSUS_INVALID_CONFLICTING        /**< Conflicting votes detected */
} consensus_validity_t;

/**
 * @brief Emergent pattern status
 */
typedef enum {
    EMERGENT_PATTERN_VALID = 0,          /**< Pattern is authentic */
    EMERGENT_PATTERN_SUSPICIOUS,         /**< Pattern requires monitoring */
    EMERGENT_PATTERN_MANIPULATED,        /**< Pattern appears manipulated */
    EMERGENT_PATTERN_UNKNOWN             /**< Pattern cannot be classified */
} emergent_pattern_status_t;

/**
 * @brief Agent trust level
 */
typedef enum {
    TRUST_LEVEL_UNTRUSTED = 0,           /**< No trust established */
    TRUST_LEVEL_MINIMAL,                 /**< Minimal trust (new agent) */
    TRUST_LEVEL_LOW,                     /**< Low trust score */
    TRUST_LEVEL_MODERATE,                /**< Moderate trust score */
    TRUST_LEVEL_HIGH,                    /**< High trust score */
    TRUST_LEVEL_VERIFIED                 /**< Verified trusted agent */
} trust_level_t;

/**
 * @brief Swarm behavior type
 */
typedef enum {
    SWARM_BEHAVIOR_IDLE = 0,             /**< Swarm is idle */
    SWARM_BEHAVIOR_SYNCHRONIZING,        /**< Agents synchronizing */
    SWARM_BEHAVIOR_CONSENSUS_FORMING,    /**< Forming consensus */
    SWARM_BEHAVIOR_TASK_EXECUTING,       /**< Executing collective task */
    SWARM_BEHAVIOR_EMERGENT,             /**< Emergent behavior detected */
    SWARM_BEHAVIOR_FRAGMENTED            /**< Swarm is fragmented */
} swarm_behavior_t;

/* ============================================================================
 * Configuration Structure
 * ============================================================================ */

/**
 * @brief Configuration for Security-Collective Cognition bridge
 */
typedef struct {
    /* Byzantine Detection */
    bool enable_byzantine_detection;      /**< Enable Byzantine agent detection */
    float byzantine_threshold;            /**< Threshold for Byzantine detection [0-1] */
    uint32_t min_conflicts_for_byzantine; /**< Min conflicts before flagging */
    bool enable_automatic_quarantine;     /**< Auto-quarantine Byzantine agents */

    /* Consensus Verification */
    bool enable_consensus_verification;   /**< Enable consensus integrity checks */
    float min_quorum_ratio;               /**< Minimum quorum for valid consensus [0-1] */
    bool enable_sybil_detection;          /**< Enable Sybil attack detection */
    uint32_t consensus_timeout_ms;        /**< Consensus timeout */

    /* Swarm Monitoring */
    bool enable_swarm_monitoring;         /**< Enable swarm behavior monitoring */
    uint32_t monitoring_interval_ms;      /**< Monitoring interval */
    float anomaly_threshold;              /**< Threshold for anomaly detection [0-1] */

    /* Emergent Pattern Validation */
    bool enable_pattern_validation;       /**< Enable emergent pattern validation */
    float pattern_confidence_threshold;   /**< Min confidence for valid pattern [0-1] */
    uint32_t pattern_history_size;        /**< Size of pattern history buffer */

    /* Agent Trust Scoring */
    bool enable_trust_scoring;            /**< Enable agent trust scoring */
    float initial_trust_score;            /**< Initial trust for new agents [0-1] */
    float trust_decay_rate;               /**< Trust decay rate per interval */
    float trust_boost_rate;               /**< Trust boost rate per good action */

    /* Sensitivity Factors */
    float security_sensitivity;           /**< Security effect scaling [0.5-2.0] */
    float collective_sensitivity;         /**< Collective effect scaling [0.5-2.0] */
} security_collective_config_t;

/* ============================================================================
 * Result Structures
 * ============================================================================ */

/**
 * @brief Byzantine detection result for an agent
 */
typedef struct {
    uint32_t agent_id;                    /**< Agent identifier */
    byzantine_status_t status;            /**< Byzantine status */
    float confidence;                     /**< Detection confidence [0-1] */
    uint32_t conflict_count;              /**< Number of conflicts detected */
    uint32_t message_count;               /**< Total messages from agent */
    uint64_t first_conflict_time;         /**< Time of first conflict */
    uint64_t last_activity_time;          /**< Time of last activity */
    bool is_quarantined;                  /**< Currently quarantined */
} byzantine_detection_result_t;

/**
 * @brief Consensus verification result
 */
typedef struct {
    uint32_t consensus_id;                /**< Consensus identifier */
    consensus_validity_t validity;        /**< Validity status */
    float quorum_ratio;                   /**< Achieved quorum ratio [0-1] */
    uint32_t participant_count;           /**< Number of participants */
    uint32_t valid_votes;                 /**< Number of valid votes */
    uint32_t invalid_votes;               /**< Number of invalid votes */
    uint32_t suspected_sybil_count;       /**< Suspected Sybil agents */
    uint64_t formation_time_ms;           /**< Time to form consensus */
    bool manipulation_detected;           /**< Manipulation indicators */
} consensus_verification_result_t;

/**
 * @brief Swarm monitoring result
 */
typedef struct {
    swarm_behavior_t current_behavior;    /**< Current swarm behavior */
    uint32_t active_agents;               /**< Number of active agents */
    float synchronization_level;          /**< Synchronization level [0-1] */
    float coherence_level;                /**< Coherence level [0-1] */
    float fragmentation_index;            /**< Fragmentation index [0-1] */
    bool anomaly_detected;                /**< Anomaly detected flag */
    float anomaly_score;                  /**< Anomaly severity [0-1] */
    uint64_t last_update_time;            /**< Last update timestamp */
} swarm_monitoring_result_t;

/**
 * @brief Emergent pattern validation result
 */
typedef struct {
    uint32_t pattern_id;                  /**< Pattern identifier */
    emergent_pattern_status_t status;     /**< Pattern status */
    float confidence;                     /**< Validation confidence [0-1] */
    float authenticity_score;             /**< Authenticity score [0-1] */
    uint32_t contributing_agents;         /**< Agents contributing to pattern */
    uint64_t emergence_time;              /**< When pattern emerged */
    uint64_t duration_ms;                 /**< Pattern duration */
    bool is_stable;                       /**< Pattern stability flag */
} emergent_pattern_result_t;

/**
 * @brief Agent trust score result
 */
typedef struct {
    uint32_t agent_id;                    /**< Agent identifier */
    trust_level_t level;                  /**< Trust level */
    float trust_score;                    /**< Trust score [0-1] */
    uint32_t positive_actions;            /**< Positive action count */
    uint32_t negative_actions;            /**< Negative action count */
    uint32_t neutral_actions;             /**< Neutral action count */
    uint64_t first_seen_time;             /**< First seen timestamp */
    uint64_t last_action_time;            /**< Last action timestamp */
    float trend;                          /**< Trust trend [-1, 1] */
} agent_trust_result_t;

/* ============================================================================
 * Effects Structures
 * ============================================================================ */

/**
 * @brief Security effects on collective cognition
 */
typedef struct {
    /* Agent isolation */
    uint32_t quarantined_agent_count;     /**< Number of quarantined agents */
    uint32_t quarantined_agents[SECURITY_COLLECTIVE_MAX_QUARANTINE]; /**< Quarantined IDs */

    /* Consensus enforcement */
    bool consensus_blocked;               /**< Consensus was blocked */
    uint32_t blocked_consensus_count;     /**< Count of blocked consensuses */
    consensus_validity_t last_block_reason; /**< Last block reason */

    /* Behavior restrictions */
    bool swarm_restricted;                /**< Swarm restrictions active */
    float allowed_synchronization;        /**< Allowed sync level [0-1] */
    uint32_t restricted_patterns;         /**< Restricted pattern count */

    /* Trust-based effects */
    float avg_swarm_trust;                /**< Average swarm trust [0-1] */
    uint32_t untrusted_agent_count;       /**< Untrusted agent count */
} security_to_collective_effects_t;

/**
 * @brief Collective cognition effects on security
 */
typedef struct {
    /* Swarm activity */
    swarm_behavior_t current_behavior;    /**< Current swarm behavior */
    uint32_t active_agent_count;          /**< Active agents */
    float synchronization_level;          /**< Sync level [0-1] */

    /* Consensus events */
    uint32_t consensus_attempts;          /**< Consensus attempts this cycle */
    uint32_t consensus_successes;         /**< Successful consensuses */
    uint32_t consensus_failures;          /**< Failed consensuses */

    /* Anomaly indicators */
    bool unusual_behavior;                /**< Unusual behavior detected */
    float behavior_anomaly_score;         /**< Anomaly score [0-1] */
    float trust_variation;                /**< Trust score variation [0-1] */

    /* Emergent events */
    uint32_t emergent_patterns_detected;  /**< Patterns detected this cycle */
    float emergence_rate;                 /**< Rate of emergence */
} collective_to_security_effects_t;

/* ============================================================================
 * State and Statistics Structures
 * ============================================================================ */

/**
 * @brief Current state of Security-Collective interaction
 */
typedef struct {
    /* Byzantine state */
    uint32_t suspected_byzantine_count;   /**< Suspected Byzantine agents */
    uint32_t confirmed_byzantine_count;   /**< Confirmed Byzantine agents */
    uint32_t quarantined_count;           /**< Currently quarantined agents */

    /* Consensus state */
    bool consensus_in_progress;           /**< Consensus currently forming */
    uint32_t active_consensus_id;         /**< Current consensus ID */
    float current_quorum_ratio;           /**< Current quorum ratio */

    /* Swarm state */
    swarm_behavior_t swarm_behavior;      /**< Current swarm behavior */
    float swarm_health;                   /**< Overall swarm health [0-1] */
    uint32_t total_agents;                /**< Total registered agents */

    /* Trust state */
    float avg_trust_score;                /**< Average trust across agents */
    float trust_distribution[5];          /**< Trust level distribution */

    /* Timestamps */
    uint64_t last_byzantine_check;        /**< Last Byzantine check */
    uint64_t last_consensus_verify;       /**< Last consensus verify */
    uint64_t last_update_time;            /**< Last bridge update */
} security_collective_state_t;

/**
 * @brief Statistics for Security-Collective bridge
 */
typedef struct {
    /* Byzantine statistics */
    uint64_t total_byzantine_checks;      /**< Total Byzantine checks */
    uint64_t byzantine_detections;        /**< Byzantine agents detected */
    uint64_t agents_quarantined;          /**< Total agents quarantined */
    uint64_t agents_cleared;              /**< Agents cleared from quarantine */
    float avg_detection_time_ns;          /**< Average detection time */

    /* Consensus statistics */
    uint64_t consensus_verifications;     /**< Total consensus verifications */
    uint64_t valid_consensuses;           /**< Valid consensuses */
    uint64_t invalid_consensuses;         /**< Invalid consensuses */
    uint64_t sybil_attacks_detected;      /**< Sybil attacks detected */
    float avg_quorum_ratio;               /**< Average quorum ratio */

    /* Swarm statistics */
    uint64_t swarm_monitoring_updates;    /**< Swarm monitoring updates */
    uint64_t anomalies_detected;          /**< Anomalies detected */
    float avg_synchronization;            /**< Average synchronization */
    float max_fragmentation;              /**< Maximum fragmentation seen */

    /* Pattern statistics */
    uint64_t patterns_validated;          /**< Patterns validated */
    uint64_t patterns_rejected;           /**< Patterns rejected */
    float avg_pattern_confidence;         /**< Average pattern confidence */

    /* Trust statistics */
    uint64_t trust_updates;               /**< Trust score updates */
    float avg_trust_change;               /**< Average trust change */
    uint64_t trust_promotions;            /**< Trust level promotions */
    uint64_t trust_demotions;             /**< Trust level demotions */

    /* Performance */
    uint64_t bridge_updates;              /**< Total bridge updates */
    float avg_update_time_ns;             /**< Average update time */
} security_collective_stats_t;

/* ============================================================================
 * Bridge Structure
 * ============================================================================ */

/**
 * @brief Security-Collective Cognition bridge state
 */
struct security_collective_bridge {
    bridge_base_t base;                   /**< MUST be first: base bridge */

    /* Configuration */
    security_collective_config_t config;

    /* Connected systems */
    collective_cognition_t* collective;   /**< Collective cognition system */
    nimcp_policy_engine_t policy_engine;  /**< Policy engine */

    /* Current effects */
    security_to_collective_effects_t security_effects;    /**< Security -> Collective */
    collective_to_security_effects_t collective_effects;  /**< Collective -> Security */

    /* State */
    security_collective_state_t state;

    /* Agent tracking */
    byzantine_detection_result_t* agent_byzantine_status; /**< Byzantine status per agent */
    agent_trust_result_t* agent_trust_scores;             /**< Trust scores per agent */
    uint32_t tracked_agent_count;                         /**< Number of tracked agents */

    /* Statistics */
    security_collective_stats_t stats;
};

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * WHAT: Returns default configuration with sensible values
 * WHY:  Provides starting point for customization
 * HOW:  Populates config struct with defaults
 *
 * @param config Output configuration structure
 * @return 0 on success, error code on failure
 */
int security_collective_default_config(security_collective_config_t* config);

/**
 * @brief Create Security-Collective bridge
 *
 * WHAT: Creates and initializes the bridge
 * WHY:  Enables security-collective cognition integration
 * HOW:  Allocates memory, initializes base, sets config
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge instance or NULL on failure
 */
security_collective_bridge_t* security_collective_bridge_create(
    const security_collective_config_t* config
);

/**
 * @brief Destroy Security-Collective bridge
 *
 * WHAT: Cleans up bridge resources
 * WHY:  Prevents memory leaks
 * HOW:  Frees tracking arrays, base cleanup, frees bridge
 *
 * @param bridge Bridge instance (NULL safe)
 */
void security_collective_bridge_destroy(security_collective_bridge_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

/**
 * @brief Connect collective cognition system
 *
 * @param bridge Bridge instance
 * @param collective Collective cognition system
 * @return 0 on success, error code on failure
 */
int security_collective_bridge_connect_collective(
    security_collective_bridge_t* bridge,
    collective_cognition_t* collective
);

/**
 * @brief Connect policy engine
 *
 * @param bridge Bridge instance
 * @param policy_engine Policy engine
 * @return 0 on success, error code on failure
 */
int security_collective_bridge_connect_policy_engine(
    security_collective_bridge_t* bridge,
    nimcp_policy_engine_t policy_engine
);

/**
 * @brief Disconnect all systems
 *
 * @param bridge Bridge instance
 * @return 0 on success, error code on failure
 */
int security_collective_bridge_disconnect(security_collective_bridge_t* bridge);

/**
 * @brief Check if bridge is fully connected
 *
 * @param bridge Bridge instance
 * @return true if all required systems connected
 */
bool security_collective_bridge_is_connected(
    const security_collective_bridge_t* bridge
);

/* ============================================================================
 * Security -> Collective Direction
 * ============================================================================ */

/**
 * @brief Detect Byzantine agents in the collective
 *
 * WHAT: Detects agents sending conflicting or malicious data
 * WHY:  Byzantine agents can corrupt collective decisions
 * HOW:  Analyzes message patterns, voting consistency, behavior history
 *
 * @param bridge Bridge instance
 * @param agent_id Agent to check (0 for all agents)
 * @param result Output detection result
 * @return 0 on success, error code on failure
 */
int security_collective_detect_byzantine(
    security_collective_bridge_t* bridge,
    uint32_t agent_id,
    byzantine_detection_result_t* result
);

/**
 * @brief Verify consensus integrity
 *
 * WHAT: Verifies that a consensus was formed legitimately
 * WHY:  Prevents consensus manipulation and Sybil attacks
 * HOW:  Validates quorum, checks for duplicate identities, analyzes voting
 *
 * @param bridge Bridge instance
 * @param consensus_id Consensus to verify
 * @param participants Array of participant agent IDs
 * @param num_participants Number of participants
 * @param result Output verification result
 * @return 0 on success, error code on failure
 */
int security_collective_verify_consensus(
    security_collective_bridge_t* bridge,
    uint32_t consensus_id,
    const uint32_t* participants,
    uint32_t num_participants,
    consensus_verification_result_t* result
);

/**
 * @brief Quarantine a Byzantine agent
 *
 * WHAT: Isolates an agent from collective participation
 * WHY:  Prevents compromised agents from affecting decisions
 * HOW:  Adds to quarantine list, blocks messages, updates trust
 *
 * @param bridge Bridge instance
 * @param agent_id Agent to quarantine
 * @param reason Quarantine reason
 * @return 0 on success, error code on failure
 */
int security_collective_quarantine_agent(
    security_collective_bridge_t* bridge,
    uint32_t agent_id,
    const char* reason
);

/**
 * @brief Release agent from quarantine
 *
 * WHAT: Removes agent from quarantine
 * WHY:  Allows rehabilitated agents to rejoin collective
 * HOW:  Removes from quarantine list, resets status, applies trust penalty
 *
 * @param bridge Bridge instance
 * @param agent_id Agent to release
 * @return 0 on success, error code on failure
 */
int security_collective_release_agent(
    security_collective_bridge_t* bridge,
    uint32_t agent_id
);

/**
 * @brief Validate emergent pattern
 *
 * WHAT: Validates that an emergent pattern is authentic
 * WHY:  Prevents manipulation of collective emergence
 * HOW:  Analyzes pattern formation, checks contributing agents, validates stability
 *
 * @param bridge Bridge instance
 * @param pattern_id Pattern to validate
 * @param result Output validation result
 * @return 0 on success, error code on failure
 */
int security_collective_validate_emergent(
    security_collective_bridge_t* bridge,
    uint32_t pattern_id,
    emergent_pattern_result_t* result
);

/* ============================================================================
 * Collective -> Security Direction
 * ============================================================================ */

/**
 * @brief Monitor swarm behavior
 *
 * WHAT: Monitors current swarm activity and health
 * WHY:  Provides baseline for anomaly detection
 * HOW:  Tracks synchronization, coherence, fragmentation metrics
 *
 * @param bridge Bridge instance
 * @param result Output monitoring result
 * @return 0 on success, error code on failure
 */
int security_collective_monitor_swarm(
    security_collective_bridge_t* bridge,
    swarm_monitoring_result_t* result
);

/**
 * @brief Score agent trustworthiness
 *
 * WHAT: Calculates trust score for an agent
 * WHY:  Enables trust-based security decisions
 * HOW:  Analyzes action history, collective feedback, time in swarm
 *
 * @param bridge Bridge instance
 * @param agent_id Agent to score
 * @param result Output trust result
 * @return 0 on success, error code on failure
 */
int security_collective_score_agent(
    security_collective_bridge_t* bridge,
    uint32_t agent_id,
    agent_trust_result_t* result
);

/**
 * @brief Report agent action for trust tracking
 *
 * WHAT: Reports an agent's action for trust calculation
 * WHY:  Builds trust history over time
 * HOW:  Records action, updates trust score, adjusts level
 *
 * @param bridge Bridge instance
 * @param agent_id Agent that performed action
 * @param positive Whether action was positive (true) or negative (false)
 * @param weight Action weight [0-1]
 * @return 0 on success, error code on failure
 */
int security_collective_report_action(
    security_collective_bridge_t* bridge,
    uint32_t agent_id,
    bool positive,
    float weight
);

/**
 * @brief Register agent with the bridge
 *
 * WHAT: Registers a new agent for tracking
 * WHY:  Initializes trust and Byzantine status tracking
 * HOW:  Allocates tracking entry, sets initial values
 *
 * @param bridge Bridge instance
 * @param agent_id Agent to register
 * @return 0 on success, error code on failure
 */
int security_collective_register_agent(
    security_collective_bridge_t* bridge,
    uint32_t agent_id
);

/**
 * @brief Unregister agent from the bridge
 *
 * WHAT: Removes agent from tracking
 * WHY:  Cleans up when agent leaves collective
 * HOW:  Removes tracking entry, frees resources
 *
 * @param bridge Bridge instance
 * @param agent_id Agent to unregister
 * @return 0 on success, error code on failure
 */
int security_collective_unregister_agent(
    security_collective_bridge_t* bridge,
    uint32_t agent_id
);

/* ============================================================================
 * Bidirectional Update API
 * ============================================================================ */

/**
 * @brief Update bridge (bidirectional)
 *
 * WHAT: Performs periodic bridge update
 * WHY:  Synchronizes state, runs detection, updates effects
 * HOW:  Runs Byzantine check, monitors swarm, updates trust decay
 *
 * @param bridge Bridge instance
 * @param delta_ms Time since last update (ms)
 * @return 0 on success, error code on failure
 */
int security_collective_bridge_update(
    security_collective_bridge_t* bridge,
    uint64_t delta_ms
);

/**
 * @brief Apply security effects to collective
 *
 * WHAT: Applies current security constraints to collective
 * WHY:  Propagates security decisions to collective behavior
 * HOW:  Updates quarantine list, sync restrictions, pattern blocks
 *
 * @param bridge Bridge instance
 * @return 0 on success, error code on failure
 */
int security_collective_apply_security_effects(
    security_collective_bridge_t* bridge
);

/**
 * @brief Apply collective effects to security
 *
 * WHAT: Applies collective activity to security monitoring
 * WHY:  Enables security visibility into collective behavior
 * HOW:  Updates anomaly tracking, trust metrics, behavior patterns
 *
 * @param bridge Bridge instance
 * @return 0 on success, error code on failure
 */
int security_collective_apply_collective_effects(
    security_collective_bridge_t* bridge
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get current bridge state
 *
 * @param bridge Bridge instance
 * @param state Output state structure
 * @return 0 on success, error code on failure
 */
int security_collective_bridge_get_state(
    const security_collective_bridge_t* bridge,
    security_collective_state_t* state
);

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge instance
 * @param stats Output statistics structure
 * @return 0 on success, error code on failure
 */
int security_collective_bridge_get_stats(
    const security_collective_bridge_t* bridge,
    security_collective_stats_t* stats
);

/**
 * @brief Get security effects on collective
 *
 * @param bridge Bridge instance
 * @param effects Output effects structure
 * @return 0 on success, error code on failure
 */
int security_collective_get_security_effects(
    const security_collective_bridge_t* bridge,
    security_to_collective_effects_t* effects
);

/**
 * @brief Get collective effects on security
 *
 * @param bridge Bridge instance
 * @param effects Output effects structure
 * @return 0 on success, error code on failure
 */
int security_collective_get_collective_effects(
    const security_collective_bridge_t* bridge,
    collective_to_security_effects_t* effects
);

/**
 * @brief Get all quarantined agents
 *
 * @param bridge Bridge instance
 * @param agent_ids Output array for agent IDs
 * @param max_agents Maximum agents to retrieve
 * @param num_agents Output number of agents retrieved
 * @return 0 on success, error code on failure
 */
int security_collective_get_quarantined_agents(
    const security_collective_bridge_t* bridge,
    uint32_t* agent_ids,
    uint32_t max_agents,
    uint32_t* num_agents
);

/**
 * @brief Get agents by trust level
 *
 * @param bridge Bridge instance
 * @param level Trust level to filter
 * @param agent_ids Output array for agent IDs
 * @param max_agents Maximum agents to retrieve
 * @param num_agents Output number of agents retrieved
 * @return 0 on success, error code on failure
 */
int security_collective_get_agents_by_trust(
    const security_collective_bridge_t* bridge,
    trust_level_t level,
    uint32_t* agent_ids,
    uint32_t max_agents,
    uint32_t* num_agents
);

/**
 * @brief Reset bridge statistics
 *
 * @param bridge Bridge instance
 * @return 0 on success, error code on failure
 */
int security_collective_bridge_reset_stats(security_collective_bridge_t* bridge);

/* ============================================================================
 * Bio-Async Integration
 * ============================================================================ */

/**
 * @brief Connect bridge to bio-async router
 *
 * @param bridge Bridge instance
 * @return 0 on success, error code on failure
 */
int security_collective_bridge_connect_bio_async(
    security_collective_bridge_t* bridge
);

/**
 * @brief Disconnect from bio-async router
 *
 * @param bridge Bridge instance
 * @return 0 on success, error code on failure
 */
int security_collective_bridge_disconnect_bio_async(
    security_collective_bridge_t* bridge
);

/**
 * @brief Check bio-async connection status
 *
 * @param bridge Bridge instance
 * @return true if connected to bio-async router
 */
bool security_collective_bridge_is_bio_async_connected(
    const security_collective_bridge_t* bridge
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SECURITY_COLLECTIVE_BRIDGE_H */
