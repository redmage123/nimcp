/**
 * @file nimcp_security_epistemic_fep_bridge.h
 * @brief Free Energy Principle Bridge for Security-Epistemic Integration
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: FEP integration for epistemic security - maps belief integrity to free energy
 * WHY:  Belief corruption, evidence tampering, and confidence manipulation represent
 *       high-surprise deviations from expected epistemic states in FEP framework
 * HOW:  Map corruption scores to free energy, evidence tampering to prediction errors,
 *       and confidence manipulation to surprise levels
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * EPISTEMIC INTEGRITY AS FREE ENERGY MINIMIZATION:
 * - Healthy belief systems = low free energy (predictions match reality)
 * - Corrupted beliefs = high free energy (model diverges from evidence)
 * - Tampered evidence = prediction error (observations violate expectations)
 * - Confidence manipulation = surprise (unexpected certainty changes)
 *
 * FEP INTEGRATION:
 * ```
 * Belief State (b) --> Integrity Verification
 *         |
 *         v
 * Expected State mu (learned epistemic model)
 *         |
 *         v
 * Prediction Error: epsilon = b - g(mu)
 *         |
 *         v
 * Free Energy F = Complexity + Inaccuracy
 *         |
 *         v
 * Surprise = -ln p(b) <= F
 *         |
 *         v
 * Corruption Score = F / F_threshold
 * ```
 *
 * DETECTION MAPPING:
 * - Low FE (<1.5)  --> Healthy epistemic state
 * - Medium FE (1.5-4.0) --> Suspicious (elevated monitoring)
 * - High FE (4.0-8.0) --> Probable tampering (quarantine beliefs)
 * - Very high FE (>8.0) --> Active attack (reject and restore)
 *
 * ARCHITECTURE:
 * ```
 * +=========================================================================+
 * |          SECURITY EPISTEMIC - FEP BRIDGE (Belief Integrity)            |
 * +=========================================================================+
 * |                                                                         |
 * |   +------------------+         +------------------+                     |
 * |   |  FEP System      |<------->|  Epistemic       |                     |
 * |   |                  |         |  Security        |                     |
 * |   | - Free Energy    |         |                  |                     |
 * |   | - Surprise       |         | - Belief Check   |                     |
 * |   | - Precision      |         | - Evidence Verify|                     |
 * |   | - Active Inf.    |         | - Conf Validate  |                     |
 * |   +------------------+         +------------------+                     |
 * |           |                              |                              |
 * |           v                              v                              |
 * |   +-------------------------------------------------------------+      |
 * |   |              BIDIRECTIONAL EFFECTS                          |      |
 * |   |                                                             |      |
 * |   |  FEP --> Security:                                          |      |
 * |   |    - Free energy --> Corruption score                       |      |
 * |   |    - Surprise --> Detection threshold                       |      |
 * |   |    - Precision --> Detection sensitivity                    |      |
 * |   |    - Active inference --> Belief restoration action         |      |
 * |   |                                                             |      |
 * |   |  Security --> FEP:                                          |      |
 * |   |    - Corrupted beliefs --> High-surprise observations       |      |
 * |   |    - Evidence tampering --> Prediction error spikes         |      |
 * |   |    - Confidence manipulation --> Precision updates          |      |
 * |   |    - Verified beliefs --> Update generative model           |      |
 * |   +-------------------------------------------------------------+      |
 * |                                                                         |
 * +=========================================================================+
 * ```
 *
 * RESTORATION VIA ACTIVE INFERENCE:
 * When high free energy is detected, the system uses active inference to select
 * restoration actions that minimize expected free energy:
 * - Rollback to prior belief state
 * - Re-verify evidence chains
 * - Reset confidence to prior levels
 * - Quarantine suspicious sources
 *
 * @see nimcp_security_epistemic_bridge.h
 * @see nimcp_free_energy.h
 * @see nimcp_bio_async.h
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_SECURITY_EPISTEMIC_FEP_BRIDGE_H
#define NIMCP_SECURITY_EPISTEMIC_FEP_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "utils/bridge/nimcp_bridge_base.h"
#include "security/epistemic/nimcp_security_epistemic_bridge.h"
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"
#include "utils/validation/nimcp_common.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** @brief Free energy thresholds for epistemic integrity levels */
#define SEC_EPIST_FEP_HEALTHY_THRESHOLD     1.5f   /**< Healthy beliefs (low FE) */
#define SEC_EPIST_FEP_SUSPICIOUS_THRESHOLD  4.0f   /**< Suspicious (monitoring) */
#define SEC_EPIST_FEP_TAMPERED_THRESHOLD    8.0f   /**< Probable tampering */
#define SEC_EPIST_FEP_ATTACK_THRESHOLD      15.0f  /**< Active attack */

/** @brief Precision bounds for detection sensitivity */
#define SEC_EPIST_FEP_MIN_PRECISION         0.05f  /**< Minimum precision */
#define SEC_EPIST_FEP_MAX_PRECISION         15.0f  /**< Maximum precision */
#define SEC_EPIST_FEP_DEFAULT_PRECISION     1.0f   /**< Default precision */

/** @brief Surprise thresholds */
#define SEC_EPIST_FEP_SURPRISE_LOW          2.0f   /**< Low surprise */
#define SEC_EPIST_FEP_SURPRISE_MEDIUM       5.0f   /**< Medium surprise */
#define SEC_EPIST_FEP_SURPRISE_HIGH         10.0f  /**< High surprise */

/** @brief Bio-async module ID */
#define BIO_MODULE_SECURITY_EPISTEMIC_FEP   0x0E21 /**< Security epistemic FEP bridge */

/** @brief Maximum restoration actions to evaluate */
#define SEC_EPIST_FEP_MAX_ACTIONS           16

/** @brief History window for running averages */
#define SEC_EPIST_FEP_HISTORY_SIZE          64

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Epistemic integrity levels based on free energy
 *
 * WHAT: Categorization of epistemic state health
 * WHY:  Drive appropriate security responses based on FE level
 */
typedef enum {
    SEC_EPIST_FEP_INTEGRITY_HEALTHY = 0,    /**< Low FE, beliefs are consistent */
    SEC_EPIST_FEP_INTEGRITY_SUSPICIOUS,      /**< Elevated FE, needs monitoring */
    SEC_EPIST_FEP_INTEGRITY_TAMPERED,        /**< High FE, probable tampering */
    SEC_EPIST_FEP_INTEGRITY_COMPROMISED,     /**< Very high FE, active attack */
    SEC_EPIST_FEP_INTEGRITY_COUNT
} sec_epist_fep_integrity_t;

/**
 * @brief Restoration action types via active inference
 *
 * WHAT: Actions that can minimize expected free energy
 * WHY:  Active inference selects actions to restore epistemic integrity
 */
typedef enum {
    SEC_EPIST_FEP_ACTION_NONE = 0,           /**< No action needed */
    SEC_EPIST_FEP_ACTION_MONITOR,            /**< Increase monitoring */
    SEC_EPIST_FEP_ACTION_QUARANTINE_BELIEF,  /**< Quarantine suspicious belief */
    SEC_EPIST_FEP_ACTION_QUARANTINE_SOURCE,  /**< Quarantine untrusted source */
    SEC_EPIST_FEP_ACTION_ROLLBACK,           /**< Rollback to prior state */
    SEC_EPIST_FEP_ACTION_REVERIFY,           /**< Re-verify evidence chains */
    SEC_EPIST_FEP_ACTION_RESET_CONFIDENCE,   /**< Reset confidence levels */
    SEC_EPIST_FEP_ACTION_REJECT_INPUT,       /**< Reject current input */
    SEC_EPIST_FEP_ACTION_COUNT
} sec_epist_fep_action_t;

/**
 * @brief Detection event types
 *
 * WHAT: Types of epistemic integrity violations detected
 * WHY:  Categorize detections for appropriate response
 */
typedef enum {
    SEC_EPIST_FEP_DETECT_NONE = 0,           /**< No detection */
    SEC_EPIST_FEP_DETECT_BELIEF_CORRUPT,     /**< Belief corruption */
    SEC_EPIST_FEP_DETECT_EVIDENCE_TAMPER,    /**< Evidence tampering */
    SEC_EPIST_FEP_DETECT_CONFIDENCE_MANIP,   /**< Confidence manipulation */
    SEC_EPIST_FEP_DETECT_SOURCE_POISON,      /**< Source poisoning */
    SEC_EPIST_FEP_DETECT_CIRCULAR_EVIDENCE,  /**< Circular evidence chain */
    SEC_EPIST_FEP_DETECT_COUNT
} sec_epist_fep_detection_t;

/* ============================================================================
 * Configuration Structure
 * ============================================================================ */

/**
 * @brief Security Epistemic FEP bridge configuration
 *
 * WHAT: Configuration parameters for FEP-epistemic integration
 * WHY:  Control sensitivity, thresholds, and learning behavior
 * HOW:  Set thresholds, enable features, configure modulation
 */
typedef struct {
    /* FEP parameters */
    float corruption_fe_threshold;           /**< FE threshold for corruption detection */
    float surprise_threshold;                /**< Surprise threshold for alerts */
    float precision_learning_rate;           /**< Precision adaptation rate */

    /* Detection parameters */
    bool use_fep_scoring;                    /**< Use FEP for corruption scoring */
    bool enable_precision_modulation;        /**< Adapt precision based on detections */
    float healthy_fe_threshold;              /**< FE threshold for healthy state */
    float attack_fe_threshold;               /**< FE threshold for attack detection */

    /* Active inference restoration */
    bool enable_active_restoration;          /**< Enable active inference for restoration */
    float action_temperature;                /**< Softmax temperature for action selection */
    uint32_t max_restoration_actions;        /**< Max actions per cycle */

    /* Belief integrity parameters */
    float belief_corruption_weight;          /**< Weight for belief corruption in FE */
    float evidence_tamper_weight;            /**< Weight for evidence tampering */
    float confidence_manip_weight;           /**< Weight for confidence manipulation */

    /* Learning parameters */
    bool enable_online_learning;             /**< Update FEP from detections */
    float learning_rate;                     /**< Belief update rate */
    bool learn_from_false_positives;         /**< Update on FP feedback */

    /* Bio-async integration */
    bool enable_bio_async;                   /**< Enable bio-async callbacks */
} sec_epist_fep_config_t;

/* ============================================================================
 * Effects Structures
 * ============================================================================ */

/**
 * @brief FEP effects on epistemic security (FEP --> Security)
 *
 * WHAT: How FEP state affects security detection/response
 * WHY:  Free energy provides corruption and tampering indicators
 */
typedef struct {
    /* Corruption indicators */
    float fep_corruption_score;              /**< Corruption score from FE [0-1] */
    float belief_integrity_score;            /**< Belief integrity [0-1, 1=healthy] */
    float evidence_chain_score;              /**< Evidence chain validity [0-1] */

    /* Detection thresholds (precision-adjusted) */
    float adjusted_corruption_threshold;     /**< Precision-adjusted threshold */
    float detection_sensitivity;             /**< Current sensitivity from precision */

    /* Surprise indicators */
    float surprise_score;                    /**< Current surprise level */
    float confidence_surprise;               /**< Surprise from confidence changes */

    /* Integrity classification */
    sec_epist_fep_integrity_t integrity_level; /**< Current integrity level */

    /* Active inference outputs */
    sec_epist_fep_action_t recommended_action; /**< Recommended restoration action */
    float action_confidence;                 /**< Confidence in recommended action */
} sec_epist_fep_effects_t;

/**
 * @brief Security effects on FEP (Security --> FEP)
 *
 * WHAT: How security detections affect FEP state
 * WHY:  Security events inform belief updates and precision
 */
typedef struct {
    /* Detection counts */
    uint64_t beliefs_verified;               /**< Successfully verified beliefs */
    uint64_t beliefs_corrupted;              /**< Corrupted beliefs detected */
    uint64_t evidence_chains_valid;          /**< Valid evidence chains */
    uint64_t evidence_chains_tampered;       /**< Tampered evidence chains */
    uint64_t confidence_manipulations;       /**< Confidence manipulation attempts */

    /* Running averages */
    float avg_belief_corruption;             /**< Average corruption score */
    float avg_evidence_integrity;            /**< Average evidence integrity */
    float avg_confidence_drift;              /**< Average confidence change rate */

    /* Attack state */
    bool attack_in_progress;                 /**< Currently under attack */
    security_epist_attack_t current_attack;  /**< Type of current attack */
    float attack_severity;                   /**< Current attack severity */
} fep_security_epist_effects_t;

/* ============================================================================
 * State and Statistics Structures
 * ============================================================================ */

/**
 * @brief Bridge operational state
 *
 * WHAT: Current state of the FEP-epistemic bridge
 * WHY:  Track active status, precision, and running metrics
 */
typedef struct {
    bool active;                             /**< Whether bridge is active */
    uint64_t update_count;                   /**< Number of updates */
    uint64_t detection_count;                /**< Detections processed */
    uint64_t restoration_count;              /**< Restorations performed */

    /* Precision state */
    float current_precision;                 /**< Current precision level */
    float precision_velocity;                /**< Rate of precision change */

    /* Running averages */
    float avg_free_energy;                   /**< Running average FE */
    float avg_surprise;                      /**< Running average surprise */
    float avg_prediction_error;              /**< Running average pred error */

    /* Integrity tracking */
    sec_epist_fep_integrity_t last_integrity; /**< Last integrity level */
    uint64_t integrity_transitions;          /**< State transitions */
} sec_epist_fep_state_t;

/**
 * @brief Bridge statistics
 *
 * WHAT: Accumulated statistics for monitoring and debugging
 * WHY:  Track performance and detection accuracy
 */
typedef struct {
    /* Detection statistics */
    uint64_t total_detections;               /**< Total detection runs */
    uint64_t fep_based_detections;           /**< Detections using FEP */
    uint64_t corruptions_found;              /**< Corruptions detected */
    uint64_t tamperings_found;               /**< Tamperings detected */
    uint64_t false_positives;                /**< Known false positives */

    /* FEP metrics */
    float avg_free_energy;                   /**< Average free energy */
    float max_free_energy;                   /**< Maximum FE observed */
    float avg_surprise;                      /**< Average surprise */
    float max_surprise;                      /**< Maximum surprise observed */

    /* Precision tracking */
    float current_precision;                 /**< Current precision */
    uint64_t precision_adaptations;          /**< Precision updates */

    /* Restoration statistics */
    uint64_t restorations_attempted;         /**< Restoration attempts */
    uint64_t restorations_successful;        /**< Successful restorations */
    float avg_restoration_fe_reduction;      /**< Avg FE reduction per restoration */

    /* Integrity statistics */
    uint64_t healthy_states;                 /**< Time in healthy state */
    uint64_t suspicious_states;              /**< Time in suspicious state */
    uint64_t tampered_states;                /**< Time in tampered state */
    uint64_t compromised_states;             /**< Time in compromised state */
} sec_epist_fep_stats_t;

/* ============================================================================
 * Detection Result Structure
 * ============================================================================ */

/**
 * @brief Detection result from FEP-enhanced analysis
 *
 * WHAT: Result of epistemic integrity verification using FEP
 * WHY:  Provide detailed detection information for response
 */
typedef struct {
    /* Primary detection */
    sec_epist_fep_detection_t detection_type; /**< Type of detection */
    float corruption_score;                   /**< Overall corruption score [0-1] */
    float confidence;                         /**< Detection confidence [0-1] */

    /* FEP metrics */
    float free_energy;                        /**< Current free energy */
    float surprise;                           /**< Surprise level */
    float prediction_error;                   /**< Prediction error magnitude */

    /* Integrity assessment */
    sec_epist_fep_integrity_t integrity;      /**< Integrity classification */
    bool requires_action;                     /**< Whether action is needed */
    sec_epist_fep_action_t recommended_action; /**< Recommended action */

    /* Details */
    uint64_t affected_belief_id;              /**< Affected belief (if any) */
    char explanation[256];                    /**< Human-readable explanation */
} sec_epist_fep_result_t;

/* ============================================================================
 * Bridge Structure
 * ============================================================================ */

/**
 * @brief Security Epistemic FEP bridge
 *
 * WHAT: Main bridge structure for FEP-epistemic integration
 * WHY:  Centralized control for free energy-based epistemic security
 * HOW:  Contains FEP system, epistemic bridge, effects, and state
 */
typedef struct {
    bridge_base_t base;                      /**< MUST be first: base infrastructure */

    /* Configuration */
    sec_epist_fep_config_t config;           /**< Bridge configuration */

    /* Connected systems */
    fep_system_t* fep_system;                /**< FEP system handle */
    security_epist_bridge_t* epist_bridge;   /**< Epistemic security bridge */

    /* Bidirectional effects */
    sec_epist_fep_effects_t fep_effects;     /**< FEP --> Security effects */
    fep_security_epist_effects_t sec_effects; /**< Security --> FEP effects */

    /* State and statistics */
    sec_epist_fep_state_t state;             /**< Current operational state */
    sec_epist_fep_stats_t stats;             /**< Accumulated statistics */

    /* History buffers for running averages */
    float* fe_history;                       /**< Free energy history */
    float* surprise_history;                 /**< Surprise history */
    uint32_t history_head;                   /**< Circular buffer head */
    uint32_t history_count;                  /**< Number of history entries */

    /* Restoration state */
    sec_epist_fep_action_t pending_action;   /**< Pending restoration action */
    uint64_t last_restoration_time;          /**< Last restoration timestamp */
} sec_epist_fep_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * WHAT: Provide sensible defaults for FEP-epistemic integration
 * WHY:  Simplify initialization with biologically-plausible defaults
 * HOW:  Return standard thresholds and rates
 *
 * @param config Output configuration structure
 * @return 0 on success, -1 on error
 *
 * Defaults:
 * - corruption_fe_threshold: 4.0
 * - surprise_threshold: 5.0
 * - precision_learning_rate: 0.05
 * - enable_active_restoration: true
 */
int sec_epist_fep_default_config(sec_epist_fep_config_t* config);

/**
 * @brief Create security epistemic FEP bridge
 *
 * WHAT: Initialize FEP integration for epistemic security
 * WHY:  Enable free energy-based corruption and tampering detection
 * HOW:  Connect FEP system to epistemic bridge, allocate structures
 *
 * @param config Configuration (NULL for defaults)
 * @param epist_bridge Epistemic security bridge handle
 * @param fep_system FEP system handle
 * @return Bridge handle or NULL on failure
 *
 * Memory: ~32KB for default configuration
 * Thread safety: Returned handle is thread-safe
 */
sec_epist_fep_bridge_t* sec_epist_fep_create(
    const sec_epist_fep_config_t* config,
    security_epist_bridge_t* epist_bridge,
    fep_system_t* fep_system
);

/**
 * @brief Destroy security epistemic FEP bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Prevent memory leaks
 * HOW:  Free allocations, disconnect bio-async, cleanup base
 *
 * @param bridge Bridge handle (NULL safe)
 */
void sec_epist_fep_destroy(sec_epist_fep_bridge_t* bridge);

/**
 * @brief Reset bridge to initial state
 *
 * WHAT: Clear state while preserving connections
 * WHY:  Fresh start without reconnection overhead
 * HOW:  Zero effects, reset statistics, clear history
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int sec_epist_fep_reset(sec_epist_fep_bridge_t* bridge);

/* ============================================================================
 * Configuration API
 * ============================================================================ */

/**
 * @brief Get current configuration
 *
 * @param bridge Bridge handle
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int sec_epist_fep_get_config(
    const sec_epist_fep_bridge_t* bridge,
    sec_epist_fep_config_t* config
);

/**
 * @brief Update configuration
 *
 * WHAT: Update bridge configuration at runtime
 * WHY:  Allow dynamic adjustment of sensitivity and thresholds
 * HOW:  Validate and apply new configuration
 *
 * @param bridge Bridge handle
 * @param config New configuration
 * @return 0 on success, -1 on error
 */
int sec_epist_fep_set_config(
    sec_epist_fep_bridge_t* bridge,
    const sec_epist_fep_config_t* config
);

/* ============================================================================
 * Compute and Update API
 * ============================================================================ */

/**
 * @brief Compute FEP effects on epistemic security
 *
 * WHAT: Calculate FEP-derived corruption and integrity scores
 * WHY:  Use free energy for epistemic integrity assessment
 * HOW:  Process current FEP state, compute integrity metrics
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int sec_epist_fep_compute_effects(sec_epist_fep_bridge_t* bridge);

/**
 * @brief Update bridge from security detection
 *
 * WHAT: Feed security detection back to FEP for learning
 * WHY:  Update generative model from epistemic events
 * HOW:  Convert detection to FEP observation, update beliefs
 *
 * @param bridge Bridge handle
 * @param detection Detection type
 * @param severity Detection severity [0-1]
 * @param belief_id Affected belief ID (0 if none)
 * @return 0 on success, -1 on error
 */
int sec_epist_fep_update_from_detection(
    sec_epist_fep_bridge_t* bridge,
    sec_epist_fep_detection_t detection,
    float severity,
    uint64_t belief_id
);

/**
 * @brief Apply precision modulation
 *
 * WHAT: Adjust detection precision based on FEP state
 * WHY:  Adapt sensitivity to current integrity level
 * HOW:  Modulate precision based on detection performance
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int sec_epist_fep_apply_precision_modulation(sec_epist_fep_bridge_t* bridge);

/**
 * @brief Verify belief using FEP-enhanced detection
 *
 * WHAT: Analyze belief for corruption using FEP + epistemic methods
 * WHY:  Combine complementary approaches for better detection
 * HOW:  Run both detectors, fuse scores
 *
 * @param bridge Bridge handle
 * @param belief_id Belief to verify
 * @param content_hash Current content hash
 * @param result Output detection result
 * @return 0 on success, -1 on error
 */
int sec_epist_fep_verify_belief(
    sec_epist_fep_bridge_t* bridge,
    uint64_t belief_id,
    uint64_t content_hash,
    sec_epist_fep_result_t* result
);

/**
 * @brief Validate evidence chain using FEP
 *
 * WHAT: Check evidence chain for tampering via FEP analysis
 * WHY:  Prediction error reveals unexpected chain modifications
 * HOW:  Model expected chain structure, detect deviations
 *
 * @param bridge Bridge handle
 * @param chain Evidence chain to validate
 * @param result Output detection result
 * @return 0 on success, -1 on error
 */
int sec_epist_fep_validate_evidence(
    sec_epist_fep_bridge_t* bridge,
    const security_epist_evidence_chain_t* chain,
    sec_epist_fep_result_t* result
);

/* ============================================================================
 * Active Inference Restoration API
 * ============================================================================ */

/**
 * @brief Select restoration action via active inference
 *
 * WHAT: Choose action that minimizes expected free energy
 * WHY:  Active inference guides optimal restoration
 * HOW:  Evaluate actions, select via softmax over -EFE
 *
 * @param bridge Bridge handle
 * @param action_out Output selected action
 * @param confidence_out Output action confidence
 * @return 0 on success, -1 on error
 */
int sec_epist_fep_select_restoration(
    sec_epist_fep_bridge_t* bridge,
    sec_epist_fep_action_t* action_out,
    float* confidence_out
);

/**
 * @brief Execute restoration action
 *
 * WHAT: Perform the selected restoration action
 * WHY:  Reduce free energy by restoring epistemic integrity
 * HOW:  Execute action type on epistemic bridge
 *
 * @param bridge Bridge handle
 * @param action Action to execute
 * @param target_belief_id Target belief (0 if not applicable)
 * @return 0 on success, -1 on error
 */
int sec_epist_fep_execute_restoration(
    sec_epist_fep_bridge_t* bridge,
    sec_epist_fep_action_t action,
    uint64_t target_belief_id
);

/**
 * @brief Report restoration outcome
 *
 * WHAT: Report whether restoration was successful
 * WHY:  Update FEP from action outcomes for learning
 * HOW:  Measure FE reduction, update precision
 *
 * @param bridge Bridge handle
 * @param action Action that was executed
 * @param success Whether action succeeded
 * @param fe_reduction Free energy reduction achieved
 * @return 0 on success, -1 on error
 */
int sec_epist_fep_report_restoration(
    sec_epist_fep_bridge_t* bridge,
    sec_epist_fep_action_t action,
    bool success,
    float fe_reduction
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get FEP effects on security
 *
 * @param bridge Bridge handle
 * @param effects Output effects structure
 * @return 0 on success, -1 on error
 */
int sec_epist_fep_get_effects(
    const sec_epist_fep_bridge_t* bridge,
    sec_epist_fep_effects_t* effects
);

/**
 * @brief Get security effects on FEP
 *
 * @param bridge Bridge handle
 * @param effects Output effects structure
 * @return 0 on success, -1 on error
 */
int sec_epist_fep_get_security_effects(
    const sec_epist_fep_bridge_t* bridge,
    fep_security_epist_effects_t* effects
);

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge handle
 * @param stats Output statistics structure
 * @return 0 on success, -1 on error
 */
int sec_epist_fep_get_stats(
    const sec_epist_fep_bridge_t* bridge,
    sec_epist_fep_stats_t* stats
);

/**
 * @brief Get current corruption score
 *
 * @param bridge Bridge handle
 * @return Current corruption score [0-1] or -1.0 on error
 */
float sec_epist_fep_get_corruption_score(const sec_epist_fep_bridge_t* bridge);

/**
 * @brief Get current integrity level
 *
 * @param bridge Bridge handle
 * @return Current integrity level or -1 on error
 */
sec_epist_fep_integrity_t sec_epist_fep_get_integrity(
    const sec_epist_fep_bridge_t* bridge
);

/**
 * @brief Get current free energy
 *
 * @param bridge Bridge handle
 * @return Current free energy or -1.0 on error
 */
float sec_epist_fep_get_free_energy(const sec_epist_fep_bridge_t* bridge);

/**
 * @brief Reset statistics
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int sec_epist_fep_reset_stats(sec_epist_fep_bridge_t* bridge);

/* ============================================================================
 * Bio-Async API
 * ============================================================================ */

/**
 * @brief Connect to bio-async router
 *
 * WHAT: Register bridge with bio-async system
 * WHY:  Enable inter-module epistemic security notifications
 * HOW:  Register module, setup inbox
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int sec_epist_fep_connect_bio_async(sec_epist_fep_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int sec_epist_fep_disconnect_bio_async(sec_epist_fep_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Bridge handle
 * @return true if connected, false otherwise
 */
bool sec_epist_fep_is_bio_async_connected(const sec_epist_fep_bridge_t* bridge);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Get integrity level name
 *
 * @param level Integrity level
 * @return Human-readable name
 */
const char* sec_epist_fep_integrity_name(sec_epist_fep_integrity_t level);

/**
 * @brief Get action name
 *
 * @param action Action type
 * @return Human-readable name
 */
const char* sec_epist_fep_action_name(sec_epist_fep_action_t action);

/**
 * @brief Get detection type name
 *
 * @param detection Detection type
 * @return Human-readable name
 */
const char* sec_epist_fep_detection_name(sec_epist_fep_detection_t detection);

/**
 * @brief Print bridge state summary (debug)
 *
 * @param bridge Bridge handle
 */
void sec_epist_fep_print_summary(const sec_epist_fep_bridge_t* bridge);

/**
 * @brief Print statistics (debug)
 *
 * @param stats Statistics to print
 */
void sec_epist_fep_print_stats(const sec_epist_fep_stats_t* stats);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SECURITY_EPISTEMIC_FEP_BRIDGE_H */
