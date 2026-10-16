/**
 * @file nimcp_security_epistemic_bridge.h
 * @brief Security - Epistemic Filter Bidirectional Integration Bridge
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: Bidirectional bridge integrating security controls with epistemic reasoning
 * WHY:  Protect epistemic reasoning from confidence manipulation, belief injection,
 *       uncertainty exploitation, and evidence chain tampering
 * HOW:  Confidence validation, belief verification, uncertainty enforcement,
 *       evidence chain validation, and epistemic attack detection
 *
 * BIOLOGICAL BASIS:
 * - Prefrontal cortex applies critical thinking filters to beliefs
 * - Anterior cingulate cortex monitors confidence calibration
 * - Hippocampus verifies evidence source integrity
 * - Orbitofrontal cortex evaluates epistemic trustworthiness
 *
 * SECURITY MODEL:
 * ```
 * +-------------------------------------------------------------------------+
 * |                SECURITY-EPISTEMIC BRIDGE ARCHITECTURE                    |
 * +-------------------------------------------------------------------------+
 * |                                                                         |
 * |   SECURITY LAYER                   EPISTEMIC FILTER                     |
 * |   +-----------------+              +-----------------+                  |
 * |   | Confidence Val  |<----------->| Confidence Mgmt |                  |
 * |   | Belief Verify   |<----------->| Belief Formation|                  |
 * |   | Uncertainty Enf |<----------->| Uncertainty Est |                  |
 * |   | Evidence Valid  |<----------->| Evidence Chains |                  |
 * |   | Attack Detect   |<----------->| Bias Detection  |                  |
 * |   +-----------------+              +-----------------+                  |
 * |          |                                  |                           |
 * |          v                                  v                           |
 * |   +---------------------------------------------+                       |
 * |   |           BIDIRECTIONAL EFFECTS             |                       |
 * |   | Security->Epistemic: Validation constraints |                       |
 * |   | Epistemic->Security: Trust signals, alerts  |                       |
 * |   +---------------------------------------------+                       |
 * +-------------------------------------------------------------------------+
 * ```
 *
 * ATTACK TYPES:
 * - Confidence Manipulation: Artificially inflating/deflating confidence
 * - Belief Injection: Inserting false beliefs into reasoning chain
 * - Uncertainty Exploitation: Exploiting edge cases in uncertainty bounds
 * - Evidence Tampering: Modifying or fabricating evidence chains
 * - Bias Amplification: Triggering or amplifying cognitive biases
 *
 * @see nimcp_epistemic_filter.h
 * @see nimcp_blood_brain_barrier.h
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_SECURITY_EPISTEMIC_BRIDGE_H
#define NIMCP_SECURITY_EPISTEMIC_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "utils/bridge/nimcp_bridge_base.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** @brief Maximum tracked beliefs */
#define SEC_EPIST_MAX_BELIEFS              1024

/** @brief Maximum evidence chain length */
#define SEC_EPIST_MAX_EVIDENCE_CHAIN       64

/** @brief Maximum concurrent validation sessions */
#define SEC_EPIST_MAX_SESSIONS             128

/** @brief Maximum attack signatures tracked */
#define SEC_EPIST_MAX_ATTACK_SIGNATURES    256

/** @brief Maximum audit log entries */
#define SEC_EPIST_MAX_AUDIT_ENTRIES        4096

/** @brief Confidence history window size */
#define SEC_EPIST_CONFIDENCE_HISTORY       100

/** @brief Bio-async module ID for security-epistemic bridge */
#define BIO_MODULE_SECURITY_EPISTEMIC      0x0E20

/** @brief Default minimum confidence for acceptance */
#define SEC_EPIST_DEFAULT_MIN_CONFIDENCE   0.1f

/** @brief Default maximum confidence allowed */
#define SEC_EPIST_DEFAULT_MAX_CONFIDENCE   0.99f

/** @brief Default uncertainty floor */
#define SEC_EPIST_DEFAULT_UNCERTAINTY_MIN  0.01f

/** @brief Attack detection window in milliseconds */
#define SEC_EPIST_ATTACK_WINDOW_MS         30000

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Epistemic attack types
 *
 * WHAT: Types of attacks on epistemic reasoning
 * WHY:  Identify and respond to specific manipulation attempts
 */
typedef enum {
    SEC_EPIST_ATTACK_NONE = 0,           /**< No attack detected */
    SEC_EPIST_ATTACK_CONFIDENCE_INFLATE, /**< Artificial confidence inflation */
    SEC_EPIST_ATTACK_CONFIDENCE_DEFLATE, /**< Artificial confidence deflation */
    SEC_EPIST_ATTACK_BELIEF_INJECT,      /**< False belief injection */
    SEC_EPIST_ATTACK_BELIEF_CORRUPT,     /**< Existing belief corruption */
    SEC_EPIST_ATTACK_UNCERTAINTY_EXPLOIT,/**< Uncertainty bound exploitation */
    SEC_EPIST_ATTACK_EVIDENCE_TAMPER,    /**< Evidence chain tampering */
    SEC_EPIST_ATTACK_EVIDENCE_FORGE,     /**< Evidence fabrication */
    SEC_EPIST_ATTACK_BIAS_AMPLIFY,       /**< Cognitive bias amplification */
    SEC_EPIST_ATTACK_SOURCE_POISON,      /**< Source reliability poisoning */
    SEC_EPIST_ATTACK_CIRCULAR_EVIDENCE,  /**< Circular evidence reference */
    SEC_EPIST_ATTACK_SYBIL,              /**< Multiple fake sources */
    SEC_EPIST_ATTACK_COUNT
} security_epist_attack_t;

/**
 * @brief Confidence validation status
 */
typedef enum {
    SEC_EPIST_CONF_VALID = 0,            /**< Confidence is valid */
    SEC_EPIST_CONF_TOO_LOW,              /**< Below minimum threshold */
    SEC_EPIST_CONF_TOO_HIGH,             /**< Exceeds maximum threshold */
    SEC_EPIST_CONF_RATE_ANOMALY,         /**< Abnormal change rate */
    SEC_EPIST_CONF_SOURCE_MISMATCH,      /**< Doesn't match evidence quality */
    SEC_EPIST_CONF_CALIBRATION_FAIL      /**< Calibration check failed */
} security_epist_conf_status_t;

/**
 * @brief Belief integrity status
 */
typedef enum {
    SEC_EPIST_BELIEF_VALID = 0,          /**< Belief integrity verified */
    SEC_EPIST_BELIEF_CORRUPTED,          /**< Belief data corrupted */
    SEC_EPIST_BELIEF_UNAUTHORIZED,       /**< Unauthorized modification */
    SEC_EPIST_BELIEF_CIRCULAR,           /**< Circular dependency detected */
    SEC_EPIST_BELIEF_INCONSISTENT,       /**< Logical inconsistency */
    SEC_EPIST_BELIEF_EXPIRED             /**< Belief validation expired */
} security_epist_belief_status_t;

/**
 * @brief Evidence chain validation status
 */
typedef enum {
    SEC_EPIST_EVIDENCE_VALID = 0,        /**< Evidence chain valid */
    SEC_EPIST_EVIDENCE_BROKEN,           /**< Chain has gaps */
    SEC_EPIST_EVIDENCE_CIRCULAR,         /**< Circular reference */
    SEC_EPIST_EVIDENCE_FORGED,           /**< Forged evidence detected */
    SEC_EPIST_EVIDENCE_TAMPERED,         /**< Evidence modified */
    SEC_EPIST_EVIDENCE_EXPIRED,          /**< Evidence too old */
    SEC_EPIST_EVIDENCE_SOURCE_UNTRUSTED  /**< Source not trusted */
} security_epist_evidence_status_t;

/**
 * @brief Bridge operational state
 */
typedef enum {
    SEC_EPIST_STATE_IDLE = 0,            /**< No active operations */
    SEC_EPIST_STATE_VALIDATING,          /**< Validating confidence/belief */
    SEC_EPIST_STATE_VERIFYING,           /**< Verifying evidence chain */
    SEC_EPIST_STATE_DETECTING,           /**< Running attack detection */
    SEC_EPIST_STATE_ENFORCING,           /**< Enforcing uncertainty bounds */
    SEC_EPIST_STATE_AUDITING,            /**< Writing audit log */
    SEC_EPIST_STATE_ERROR                /**< Error state */
} security_epist_state_t;

/**
 * @brief Audit event types
 */
typedef enum {
    SEC_EPIST_AUDIT_CONFIDENCE = 0,      /**< Confidence validation event */
    SEC_EPIST_AUDIT_BELIEF,              /**< Belief verification event */
    SEC_EPIST_AUDIT_UNCERTAINTY,         /**< Uncertainty enforcement event */
    SEC_EPIST_AUDIT_EVIDENCE,            /**< Evidence validation event */
    SEC_EPIST_AUDIT_ATTACK,              /**< Attack detection event */
    SEC_EPIST_AUDIT_CORRECTION,          /**< Confidence/belief correction */
    SEC_EPIST_AUDIT_REJECTION            /**< Input rejection event */
} security_epist_audit_type_t;

/* ============================================================================
 * Configuration Structure
 * ============================================================================ */

/**
 * @brief Security-Epistemic bridge configuration
 *
 * WHAT: Configuration parameters for the bridge
 * WHY:  Control epistemic security features and thresholds
 * HOW:  Enable/disable features, set validation bounds and policies
 */
typedef struct {
    /* Feature enable flags */
    bool enable_confidence_validation;    /**< Enable confidence score validation */
    bool enable_belief_verification;      /**< Enable belief integrity verification */
    bool enable_uncertainty_enforcement;  /**< Enable uncertainty bound enforcement */
    bool enable_evidence_validation;      /**< Enable evidence chain validation */
    bool enable_attack_detection;         /**< Enable epistemic attack detection */
    bool enable_audit;                    /**< Enable audit logging */
    bool enable_auto_correction;          /**< Enable automatic correction */

    /* Confidence validation settings */
    float min_confidence;                 /**< Minimum allowed confidence [0-1] */
    float max_confidence;                 /**< Maximum allowed confidence [0-1] */
    float max_confidence_rate;            /**< Maximum confidence change per second */
    bool require_calibrated_confidence;   /**< Require confidence calibration */

    /* Uncertainty enforcement settings */
    float min_uncertainty;                /**< Minimum uncertainty floor [0-1] */
    float max_uncertainty;                /**< Maximum uncertainty ceiling [0-1] */
    bool enforce_irreducible_uncertainty; /**< Enforce irreducible uncertainty */

    /* Evidence validation settings */
    uint32_t max_evidence_age_s;          /**< Maximum evidence age in seconds */
    uint32_t min_independent_sources;     /**< Minimum independent sources required */
    float min_source_reliability;         /**< Minimum source reliability [0-1] */
    bool detect_circular_evidence;        /**< Detect circular evidence chains */

    /* Attack detection settings */
    float attack_threshold;               /**< Threshold for attack alerts [0-1] */
    uint32_t attack_window_ms;            /**< Time window for pattern detection */
    bool block_on_attack;                 /**< Block operations during attack */

    /* Sensitivity parameters */
    float security_sensitivity;           /**< Overall security sensitivity [0.5-2.0] */
    float epistemic_sensitivity;          /**< Epistemic protection sensitivity [0.5-2.0] */

    /* Bio-async integration */
    bool enable_bio_async;                /**< Enable bio-async callbacks */
} security_epist_config_t;

/* ============================================================================
 * Belief Structure
 * ============================================================================ */

/**
 * @brief Belief record for verification
 *
 * WHAT: Represents a belief with integrity metadata
 * WHY:  Track belief provenance and modification history
 */
typedef struct {
    uint64_t belief_id;                   /**< Unique belief identifier */
    uint64_t creation_time;               /**< When belief was formed */
    uint64_t last_verified;               /**< Last verification timestamp */
    uint32_t verification_count;          /**< Number of verifications */
    float initial_confidence;             /**< Original confidence score */
    float current_confidence;             /**< Current confidence score */
    uint64_t hash;                        /**< Belief content hash */
    uint32_t evidence_count;              /**< Supporting evidence count */
    bool is_locked;                       /**< Belief is locked from modification */
} security_epist_belief_t;

/* ============================================================================
 * Evidence Structure
 * ============================================================================ */

/**
 * @brief Evidence link in chain
 */
typedef struct {
    uint64_t evidence_id;                 /**< Evidence identifier */
    uint64_t source_id;                   /**< Source identifier */
    float source_reliability;             /**< Source reliability score */
    uint64_t timestamp;                   /**< Evidence timestamp */
    uint64_t hash;                        /**< Evidence content hash */
    uint32_t dependent_count;             /**< Dependent evidence count */
    bool is_primary;                      /**< Is primary source */
} security_epist_evidence_link_t;

/**
 * @brief Evidence chain for validation
 */
typedef struct {
    security_epist_evidence_link_t links[SEC_EPIST_MAX_EVIDENCE_CHAIN];
    uint32_t link_count;                  /**< Number of links in chain */
    uint64_t chain_id;                    /**< Chain identifier */
    float overall_reliability;            /**< Calculated chain reliability */
    bool has_primary_source;              /**< Contains primary source */
    uint32_t independent_paths;           /**< Independent verification paths */
} security_epist_evidence_chain_t;

/* ============================================================================
 * Effects Structures
 * ============================================================================ */

/**
 * @brief Security effects on epistemic filter
 *
 * WHAT: How security controls affect epistemic reasoning
 * WHY:  Track security impositions on epistemic behavior
 */
typedef struct {
    /* Confidence constraints */
    float enforced_min_confidence;        /**< Currently enforced minimum */
    float enforced_max_confidence;        /**< Currently enforced maximum */
    uint32_t confidence_rejections;       /**< Confidence values rejected */
    uint32_t confidence_corrections;      /**< Confidence values corrected */

    /* Belief constraints */
    uint32_t beliefs_verified;            /**< Beliefs successfully verified */
    uint32_t beliefs_rejected;            /**< Beliefs rejected */
    uint32_t beliefs_locked;              /**< Beliefs currently locked */
    bool accepting_new_beliefs;           /**< Whether new beliefs accepted */

    /* Uncertainty enforcement */
    float enforced_uncertainty_floor;     /**< Current uncertainty floor */
    uint32_t uncertainty_violations;      /**< Uncertainty violations detected */

    /* Evidence constraints */
    uint32_t evidence_chains_validated;   /**< Chains validated */
    uint32_t evidence_chains_rejected;    /**< Chains rejected */
    float min_chain_reliability;          /**< Minimum chain reliability required */

    /* Attack response */
    bool attack_mode_active;              /**< Attack response mode active */
    security_epist_attack_t active_attack;/**< Type of active attack */
    float attack_severity;                /**< Severity of current attack */

    /* Performance impact */
    float validation_latency_ms;          /**< Validation processing latency */
    float throughput_reduction;           /**< Throughput reduction factor [0-1] */
} security_to_epist_effects_t;

/**
 * @brief Epistemic effects on security system
 *
 * WHAT: How epistemic behavior affects security
 * WHY:  Epistemic patterns inform security decisions
 */
typedef struct {
    /* Reasoning patterns */
    float average_confidence;             /**< Average confidence of beliefs */
    float confidence_variance;            /**< Variance in confidence scores */
    float belief_update_rate;             /**< Rate of belief updates */
    uint32_t active_beliefs;              /**< Number of active beliefs */

    /* Trust indicators */
    float source_trust_average;           /**< Average source trust level */
    uint32_t trusted_sources;             /**< Number of trusted sources */
    uint32_t untrusted_sources;           /**< Number of untrusted sources */
    float evidence_quality_average;       /**< Average evidence quality */

    /* Anomaly indicators */
    bool anomaly_detected;                /**< Whether anomaly was detected */
    float anomaly_score;                  /**< Anomaly severity score [0-1] */
    uint32_t anomaly_type;                /**< Type of anomaly detected */
    char anomaly_description[128];        /**< Human-readable description */

    /* Bias indicators */
    uint32_t biases_detected;             /**< Number of biases detected */
    float bias_severity;                  /**< Overall bias severity */
    uint32_t confirmation_bias_count;     /**< Confirmation bias instances */

    /* Health metrics */
    float epistemic_health;               /**< Overall epistemic health [0-1] */
    float calibration_score;              /**< Confidence calibration [0-1] */
    float skepticism_level;               /**< Current skepticism level [0-1] */
} epist_to_security_effects_t;

/* ============================================================================
 * State and Statistics Structures
 * ============================================================================ */

/**
 * @brief Bridge operational state
 */
typedef struct {
    security_epist_state_t state;         /**< Current operational state */
    uint64_t last_validation;             /**< Timestamp of last validation */
    uint64_t last_verification;           /**< Timestamp of last verification */
    uint64_t last_attack_check;           /**< Timestamp of last attack check */
    uint32_t active_sessions;             /**< Number of active sessions */
    uint32_t pending_validations;         /**< Validations pending */
    bool epistemic_connected;             /**< Epistemic filter connected */
} security_epist_state_info_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    /* Confidence validation stats */
    uint64_t total_confidence_checks;     /**< Total confidence checks */
    uint64_t confidence_valid;            /**< Valid confidence scores */
    uint64_t confidence_rejected;         /**< Rejected confidence scores */
    uint64_t confidence_corrected;        /**< Corrected confidence scores */
    float mean_confidence;                /**< Mean confidence value */
    float mean_validation_latency_us;     /**< Mean validation latency */

    /* Belief verification stats */
    uint64_t total_belief_checks;         /**< Total belief verifications */
    uint64_t beliefs_verified;            /**< Successfully verified */
    uint64_t beliefs_rejected;            /**< Rejected beliefs */
    uint64_t beliefs_corrupted;           /**< Corrupted beliefs detected */

    /* Uncertainty enforcement stats */
    uint64_t uncertainty_checks;          /**< Uncertainty checks performed */
    uint64_t uncertainty_violations;      /**< Violations detected */
    uint64_t uncertainty_corrections;     /**< Corrections applied */

    /* Evidence validation stats */
    uint64_t evidence_chains_checked;     /**< Evidence chains checked */
    uint64_t evidence_chains_valid;       /**< Valid evidence chains */
    uint64_t evidence_chains_rejected;    /**< Rejected evidence chains */
    uint64_t circular_evidence_detected;  /**< Circular references found */
    uint64_t forged_evidence_detected;    /**< Forged evidence detected */

    /* Attack detection stats */
    uint64_t attack_checks;               /**< Attack detection runs */
    uint64_t attacks_detected;            /**< Attacks detected */
    uint64_t attacks_blocked;             /**< Attacks blocked */
    uint64_t false_positives;             /**< Known false positives */
    float mean_attack_severity;           /**< Mean attack severity */

    /* Audit stats */
    uint64_t audit_entries;               /**< Total audit log entries */
    uint64_t audit_alerts;                /**< Audit alerts generated */
} security_epist_stats_t;

/* ============================================================================
 * Audit Entry Structure
 * ============================================================================ */

/**
 * @brief Audit log entry
 */
typedef struct {
    uint64_t timestamp;                   /**< Event timestamp */
    security_epist_audit_type_t type;     /**< Event type */
    uint64_t belief_id;                   /**< Related belief ID (if any) */
    float original_confidence;            /**< Original confidence value */
    float corrected_confidence;           /**< Corrected value (if any) */
    security_epist_attack_t attack_type;  /**< Attack type (if attack event) */
    bool success;                         /**< Whether operation succeeded */
    char details[256];                    /**< Additional details */
} security_epist_audit_entry_t;

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

/* Epistemic filter type - forward declaration */
typedef struct epistemic_filter_struct* epistemic_filter_t;

/* BBB forward declaration */
typedef struct bbb_system_struct* bbb_system_t;

/* ============================================================================
 * Bridge Structure
 * ============================================================================ */

/**
 * @brief Security-Epistemic bridge
 *
 * WHAT: Main bridge structure connecting security to epistemic filter
 * WHY:  Centralized security control for epistemic reasoning
 * HOW:  Contains connections, effects, state, and configuration
 */
typedef struct security_epist_bridge {
    bridge_base_t base;                   /**< MUST be first: base infrastructure */

    /* Configuration */
    security_epist_config_t config;       /**< Bridge configuration */

    /* Epistemic filter connection */
    epistemic_filter_t epistemic_filter;  /**< Connected epistemic filter */
    bool epistemic_connected;             /**< Connection flag */

    /* BBB connection */
    bbb_system_t bbb;                     /**< Connected Blood-Brain Barrier */
    bool bbb_connected;                   /**< BBB connection flag */

    /* Bidirectional effects */
    security_to_epist_effects_t security_effects;  /**< Security->Epistemic */
    epist_to_security_effects_t epist_effects;     /**< Epistemic->Security */

    /* State and statistics */
    security_epist_state_info_t state;    /**< Current operational state */
    security_epist_stats_t stats;         /**< Operational statistics */

    /* Belief tracking */
    security_epist_belief_t* beliefs;     /**< Tracked beliefs */
    uint32_t num_beliefs;                 /**< Number of tracked beliefs */

    /* Confidence history for anomaly detection */
    float* confidence_history;            /**< Confidence change history */
    uint32_t history_head;                /**< Circular buffer head */
    uint32_t history_count;               /**< Number of entries */

    /* Audit log */
    security_epist_audit_entry_t* audit_log;  /**< Audit log buffer */
    uint32_t audit_log_head;              /**< Circular buffer head */
    uint32_t audit_log_count;             /**< Number of entries */
} security_epist_bridge_t;

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * WHAT: Returns sensible default configuration
 * WHY:  Starting point for most deployments
 *
 * @param config Output configuration structure
 * @return 0 on success, -1 on error
 *
 * Defaults:
 * - enable_confidence_validation: true
 * - enable_belief_verification: true
 * - enable_uncertainty_enforcement: true
 * - min_confidence: 0.1
 * - max_confidence: 0.99
 * - min_uncertainty: 0.01
 * - security_sensitivity: 1.0
 */
int security_epist_default_config(security_epist_config_t* config);

/**
 * @brief Create security-epistemic bridge
 *
 * WHAT: Allocates and initializes bridge
 * WHY:  Entry point for security-epistemic integration
 * HOW:  Allocates structures, initializes state, applies config
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 *
 * Memory: ~128KB for default configuration
 * Thread safety: Returned handle is thread-safe
 */
security_epist_bridge_t* security_epist_bridge_create(
    const security_epist_config_t* config
);

/**
 * @brief Destroy security-epistemic bridge
 *
 * WHAT: Releases all resources
 * WHY:  Clean shutdown
 * HOW:  Disconnects systems, frees memory
 *
 * @param bridge Bridge to destroy (NULL safe)
 */
void security_epist_bridge_destroy(security_epist_bridge_t* bridge);

/**
 * @brief Reset bridge to initial state
 *
 * WHAT: Clears state while preserving connections
 * WHY:  Fresh start without reconnection
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int security_epist_bridge_reset(security_epist_bridge_t* bridge);

/* ============================================================================
 * Connection Functions
 * ============================================================================ */

/**
 * @brief Connect epistemic filter
 *
 * @param bridge Bridge handle
 * @param epistemic_filter Epistemic filter system
 * @return 0 on success, -1 on error
 */
int security_epist_connect_filter(
    security_epist_bridge_t* bridge,
    epistemic_filter_t epistemic_filter
);

/**
 * @brief Connect Blood-Brain Barrier
 *
 * @param bridge Bridge handle
 * @param bbb BBB system
 * @return 0 on success, -1 on error
 */
int security_epist_connect_bbb(
    security_epist_bridge_t* bridge,
    bbb_system_t bbb
);

/**
 * @brief Disconnect all systems
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int security_epist_disconnect_all(security_epist_bridge_t* bridge);

/**
 * @brief Check if epistemic filter is connected
 *
 * @param bridge Bridge handle
 * @return true if connected, false otherwise
 */
bool security_epist_is_connected(const security_epist_bridge_t* bridge);

/* ============================================================================
 * Confidence Validation Functions
 * ============================================================================ */

/**
 * @brief Validate confidence score
 *
 * WHAT: Validates a confidence score for security
 * WHY:  Prevent confidence manipulation attacks
 * HOW:  Check bounds, rate of change, calibration
 *
 * @param bridge Bridge handle
 * @param confidence Confidence score to validate [0-1]
 * @param belief_id Associated belief ID (0 for no belief)
 * @param status_out Output validation status
 * @return true if valid, false if invalid
 *
 * Checks performed:
 * - Minimum/maximum bounds
 * - Rate of change within limits
 * - Calibration alignment
 * - Consistency with evidence quality
 */
bool security_epist_validate_confidence(
    security_epist_bridge_t* bridge,
    float confidence,
    uint64_t belief_id,
    security_epist_conf_status_t* status_out
);

/**
 * @brief Correct invalid confidence score
 *
 * WHAT: Adjusts confidence to valid range
 * WHY:  Recover from minor validation failures
 *
 * @param bridge Bridge handle
 * @param confidence Original confidence value
 * @param corrected_out Output corrected value
 * @return 0 on success, -1 on error
 */
int security_epist_correct_confidence(
    security_epist_bridge_t* bridge,
    float confidence,
    float* corrected_out
);

/**
 * @brief Set confidence bounds
 *
 * @param bridge Bridge handle
 * @param min_confidence Minimum allowed [0-1]
 * @param max_confidence Maximum allowed [0-1]
 * @return 0 on success, -1 on error
 */
int security_epist_set_confidence_bounds(
    security_epist_bridge_t* bridge,
    float min_confidence,
    float max_confidence
);

/* ============================================================================
 * Belief Verification Functions
 * ============================================================================ */

/**
 * @brief Verify belief integrity
 *
 * WHAT: Verifies a belief has not been tampered with
 * WHY:  Detect belief injection and corruption attacks
 * HOW:  Check hash, provenance, consistency
 *
 * @param bridge Bridge handle
 * @param belief_id Belief identifier
 * @param content_hash Hash of current belief content
 * @param status_out Output verification status
 * @return true if verified, false if failed
 */
bool security_epist_verify_belief(
    security_epist_bridge_t* bridge,
    uint64_t belief_id,
    uint64_t content_hash,
    security_epist_belief_status_t* status_out
);

/**
 * @brief Register new belief for tracking
 *
 * @param bridge Bridge handle
 * @param belief_id Belief identifier
 * @param content_hash Hash of belief content
 * @param initial_confidence Initial confidence score
 * @return 0 on success, -1 on error
 */
int security_epist_register_belief(
    security_epist_bridge_t* bridge,
    uint64_t belief_id,
    uint64_t content_hash,
    float initial_confidence
);

/**
 * @brief Update belief confidence with verification
 *
 * @param bridge Bridge handle
 * @param belief_id Belief identifier
 * @param new_confidence New confidence score
 * @param new_hash Updated content hash (0 to skip hash check)
 * @return 0 on success, -1 on error
 */
int security_epist_update_belief(
    security_epist_bridge_t* bridge,
    uint64_t belief_id,
    float new_confidence,
    uint64_t new_hash
);

/**
 * @brief Lock belief from modification
 *
 * @param bridge Bridge handle
 * @param belief_id Belief to lock
 * @return 0 on success, -1 on error
 */
int security_epist_lock_belief(
    security_epist_bridge_t* bridge,
    uint64_t belief_id
);

/**
 * @brief Revoke/remove belief
 *
 * @param bridge Bridge handle
 * @param belief_id Belief to revoke
 * @return 0 on success, -1 on error
 */
int security_epist_revoke_belief(
    security_epist_bridge_t* bridge,
    uint64_t belief_id
);

/* ============================================================================
 * Uncertainty Enforcement Functions
 * ============================================================================ */

/**
 * @brief Enforce uncertainty bounds
 *
 * WHAT: Validates uncertainty is within allowed bounds
 * WHY:  Prevent uncertainty exploitation attacks
 * HOW:  Check floor, ceiling, and irreducibility
 *
 * @param bridge Bridge handle
 * @param uncertainty Uncertainty value to check [0-1]
 * @param is_valid_out Output: whether value is valid
 * @return 0 on success, -1 on error
 */
int security_epist_enforce_uncertainty(
    security_epist_bridge_t* bridge,
    float uncertainty,
    bool* is_valid_out
);

/**
 * @brief Get adjusted uncertainty
 *
 * WHAT: Returns uncertainty clamped to valid range
 *
 * @param bridge Bridge handle
 * @param uncertainty Original uncertainty
 * @param adjusted_out Output adjusted value
 * @return 0 on success, -1 on error
 */
int security_epist_adjust_uncertainty(
    security_epist_bridge_t* bridge,
    float uncertainty,
    float* adjusted_out
);

/**
 * @brief Set uncertainty bounds
 *
 * @param bridge Bridge handle
 * @param min_uncertainty Minimum uncertainty floor
 * @param max_uncertainty Maximum uncertainty ceiling
 * @return 0 on success, -1 on error
 */
int security_epist_set_uncertainty_bounds(
    security_epist_bridge_t* bridge,
    float min_uncertainty,
    float max_uncertainty
);

/* ============================================================================
 * Evidence Validation Functions
 * ============================================================================ */

/**
 * @brief Validate evidence chain
 *
 * WHAT: Validates an evidence chain for integrity
 * WHY:  Detect evidence tampering and forgery
 * HOW:  Check links, sources, circularity, timestamps
 *
 * @param bridge Bridge handle
 * @param chain Evidence chain to validate
 * @param status_out Output validation status
 * @return true if valid, false if invalid
 */
bool security_epist_validate_evidence(
    security_epist_bridge_t* bridge,
    const security_epist_evidence_chain_t* chain,
    security_epist_evidence_status_t* status_out
);

/**
 * @brief Check for circular evidence
 *
 * @param bridge Bridge handle
 * @param chain Evidence chain to check
 * @return true if circular reference found, false otherwise
 */
bool security_epist_check_circular_evidence(
    security_epist_bridge_t* bridge,
    const security_epist_evidence_chain_t* chain
);

/**
 * @brief Calculate evidence chain reliability
 *
 * @param bridge Bridge handle
 * @param chain Evidence chain
 * @param reliability_out Output reliability score [0-1]
 * @return 0 on success, -1 on error
 */
int security_epist_calculate_chain_reliability(
    security_epist_bridge_t* bridge,
    const security_epist_evidence_chain_t* chain,
    float* reliability_out
);

/**
 * @brief Register trusted source
 *
 * @param bridge Bridge handle
 * @param source_id Source identifier
 * @param reliability Initial reliability score [0-1]
 * @return 0 on success, -1 on error
 */
int security_epist_register_source(
    security_epist_bridge_t* bridge,
    uint64_t source_id,
    float reliability
);

/**
 * @brief Update source reliability
 *
 * @param bridge Bridge handle
 * @param source_id Source identifier
 * @param correct Whether source was correct
 * @return 0 on success, -1 on error
 */
int security_epist_update_source(
    security_epist_bridge_t* bridge,
    uint64_t source_id,
    bool correct
);

/* ============================================================================
 * Attack Detection Functions
 * ============================================================================ */

/**
 * @brief Detect epistemic attacks
 *
 * WHAT: Scans for signs of epistemic manipulation
 * WHY:  Early warning of active attacks
 * HOW:  Pattern analysis, anomaly detection
 *
 * @param bridge Bridge handle
 * @param attack_out Output detected attack type
 * @param severity_out Output attack severity [0-1]
 * @param details_out Output buffer for details (can be NULL)
 * @param details_size Size of details buffer
 * @return true if attack detected, false otherwise
 */
bool security_epist_detect_attack(
    security_epist_bridge_t* bridge,
    security_epist_attack_t* attack_out,
    float* severity_out,
    char* details_out,
    size_t details_size
);

/**
 * @brief Get attack signature
 *
 * @param attack_type Attack type
 * @return Human-readable attack signature description
 */
const char* security_epist_get_attack_signature(security_epist_attack_t attack_type);

/**
 * @brief Report false positive attack detection
 *
 * @param bridge Bridge handle
 * @param attack_type Attack type that was false positive
 * @return 0 on success, -1 on error
 */
int security_epist_report_false_positive(
    security_epist_bridge_t* bridge,
    security_epist_attack_t attack_type
);

/* ============================================================================
 * Bidirectional Update Functions
 * ============================================================================ */

/**
 * @brief Update bridge state (main update loop)
 *
 * WHAT: Process pending operations and update effects
 * WHY:  Maintain bridge synchronization
 * HOW:  Process queued validations, update effects, run detection
 *
 * @param bridge Bridge handle
 * @param delta_ms Time since last update in milliseconds
 * @return 0 on success, -1 on error
 *
 * Call frequency: Recommended 10-100ms intervals
 */
int security_epist_bridge_update(
    security_epist_bridge_t* bridge,
    uint64_t delta_ms
);

/**
 * @brief Apply security effects to epistemic filter
 *
 * WHAT: Propagate security state to epistemic filter
 * WHY:  Enforce security controls on epistemic behavior
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int security_epist_apply_security_effects(security_epist_bridge_t* bridge);

/**
 * @brief Gather epistemic effects for security analysis
 *
 * WHAT: Collect epistemic behavior data for security
 * WHY:  Inform security decisions based on epistemic patterns
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int security_epist_gather_epist_effects(security_epist_bridge_t* bridge);

/* ============================================================================
 * Query Functions
 * ============================================================================ */

/**
 * @brief Get security effects on epistemic filter
 *
 * @param bridge Bridge handle
 * @param effects_out Output effects structure
 * @return 0 on success, -1 on error
 */
int security_epist_get_security_effects(
    const security_epist_bridge_t* bridge,
    security_to_epist_effects_t* effects_out
);

/**
 * @brief Get epistemic effects on security
 *
 * @param bridge Bridge handle
 * @param effects_out Output effects structure
 * @return 0 on success, -1 on error
 */
int security_epist_get_epist_effects(
    const security_epist_bridge_t* bridge,
    epist_to_security_effects_t* effects_out
);

/**
 * @brief Get bridge state information
 *
 * @param bridge Bridge handle
 * @param state_out Output state structure
 * @return 0 on success, -1 on error
 */
int security_epist_get_state(
    const security_epist_bridge_t* bridge,
    security_epist_state_info_t* state_out
);

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge handle
 * @param stats_out Output statistics structure
 * @return 0 on success, -1 on error
 */
int security_epist_get_stats(
    const security_epist_bridge_t* bridge,
    security_epist_stats_t* stats_out
);

/**
 * @brief Reset statistics counters
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int security_epist_reset_stats(security_epist_bridge_t* bridge);

/* ============================================================================
 * Audit Functions
 * ============================================================================ */

/**
 * @brief Log security event for audit
 *
 * @param bridge Bridge handle
 * @param type Audit event type
 * @param belief_id Related belief ID (0 if not applicable)
 * @param original_confidence Original confidence value
 * @param corrected_confidence Corrected value (or original if not corrected)
 * @param success Whether operation succeeded
 * @param details Additional details (can be NULL)
 * @return 0 on success, -1 on error
 */
int security_epist_audit_event(
    security_epist_bridge_t* bridge,
    security_epist_audit_type_t type,
    uint64_t belief_id,
    float original_confidence,
    float corrected_confidence,
    bool success,
    const char* details
);

/**
 * @brief Get recent audit entries
 *
 * @param bridge Bridge handle
 * @param entries_out Output buffer for entries
 * @param max_entries Maximum entries to return
 * @param count_out Actual entries returned
 * @return 0 on success, -1 on error
 */
int security_epist_get_audit_log(
    const security_epist_bridge_t* bridge,
    security_epist_audit_entry_t* entries_out,
    size_t max_entries,
    size_t* count_out
);

/**
 * @brief Clear audit log
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int security_epist_clear_audit_log(security_epist_bridge_t* bridge);

/* ============================================================================
 * Bio-Async Integration
 * ============================================================================ */

/**
 * @brief Connect to bio-async router
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int security_epist_connect_bio_async(security_epist_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int security_epist_disconnect_bio_async(security_epist_bridge_t* bridge);

/**
 * @brief Check bio-async connection status
 *
 * @param bridge Bridge handle
 * @return true if connected, false otherwise
 */
bool security_epist_is_bio_async_connected(const security_epist_bridge_t* bridge);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Get attack type name
 *
 * @param attack Attack type
 * @return Human-readable name
 */
const char* security_epist_attack_name(security_epist_attack_t attack);

/**
 * @brief Get confidence status name
 *
 * @param status Confidence status
 * @return Human-readable name
 */
const char* security_epist_conf_status_name(security_epist_conf_status_t status);

/**
 * @brief Get belief status name
 *
 * @param status Belief status
 * @return Human-readable name
 */
const char* security_epist_belief_status_name(security_epist_belief_status_t status);

/**
 * @brief Get evidence status name
 *
 * @param status Evidence status
 * @return Human-readable name
 */
const char* security_epist_evidence_status_name(security_epist_evidence_status_t status);

/**
 * @brief Get bridge state name
 *
 * @param state Bridge state
 * @return Human-readable name
 */
const char* security_epist_state_name(security_epist_state_t state);

/**
 * @brief Get audit event type name
 *
 * @param audit_type Audit event type
 * @return Human-readable name
 */
const char* security_epist_audit_type_name(security_epist_audit_type_t audit_type);

/**
 * @brief Print bridge state summary (debug)
 *
 * @param bridge Bridge handle
 */
void security_epist_print_summary(const security_epist_bridge_t* bridge);

/**
 * @brief Print statistics (debug)
 *
 * @param stats Statistics to print
 */
void security_epist_print_stats(const security_epist_stats_t* stats);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SECURITY_EPISTEMIC_BRIDGE_H */
