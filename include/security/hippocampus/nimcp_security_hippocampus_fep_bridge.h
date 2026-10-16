/**
 * @file nimcp_security_hippocampus_fep_bridge.h
 * @brief Free Energy Principle bridge for Security Hippocampus Bridge
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: FEP integration for hippocampus memory security
 * WHY:  Memory integrity violations are high-surprise events in FEP framework
 * HOW:  Map memory security metrics to free energy, use prediction errors for detection
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * MEMORY SECURITY AS SURPRISE DETECTION:
 * - Intact memory = low free energy (expected consolidation patterns)
 * - Memory tampering = high free energy (surprising/unexpected changes)
 * - Consolidation attacks = prediction error (unexpected replay patterns)
 * - Replay hijacking = high surprise (violates generative model)
 *
 * FEP INTEGRATION:
 * ```
 * Memory State Observation (o) -> Integrity Assessment
 *         |
 * Expected Consolidation Pattern mu (learned generative model)
 *         |
 * Prediction Error: epsilon = o - g(mu)
 *         |
 * Free Energy F = Complexity + Inaccuracy
 *         |
 * Surprise = -ln p(o) <= F
 *         |
 * Security Threat Level = F / F_threshold
 * ```
 *
 * SECURITY-FEP MAPPING:
 * - Memory integrity score -> Free energy (inverted: low integrity = high FE)
 * - Consolidation anomaly -> Prediction error
 * - Replay tampering -> Surprise level
 * - Protective consolidation -> Active inference response
 *
 * DETECTION MAPPING:
 * - Low FE (<2.0) -> Normal memory operations
 * - Medium FE (2-5) -> Suspicious (monitor closely)
 * - High FE (5-10) -> Consolidation attack detected
 * - Very high FE (>10) -> Critical memory tampering
 *
 * ARCHITECTURE:
 * ```
 * +============================================================================+
 * |        SECURITY HIPPOCAMPUS - FEP BRIDGE (Memory Integrity Detection)     |
 * +============================================================================+
 * |                                                                            |
 * |   +------------------+         +------------------+                        |
 * |   |  FEP System      |-------->|  Security        |                        |
 * |   |                  |         |  Hippocampus     |                        |
 * |   | * Free Energy    |         |                  |                        |
 * |   | * Surprise       |         | * Memory Guard   |                        |
 * |   | * Precision      |         | * Consolidation  |                        |
 * |   +------------------+         +------------------+                        |
 * |           |                              |                                 |
 * |   +--------------------------------------------------------------+         |
 * |   |              BIDIRECTIONAL EFFECTS                           |         |
 * |   |                                                              |         |
 * |   |  FEP -> Security:                                            |         |
 * |   |    * Free energy -> Threat level                             |         |
 * |   |    * Surprise -> Anomaly sensitivity                         |         |
 * |   |    * Precision -> Detection confidence                       |         |
 * |   |                                                              |         |
 * |   |  Security -> FEP:                                            |         |
 * |   |    * Detected attacks -> High-surprise observations          |         |
 * |   |    * Normal operations -> Update generative model            |         |
 * |   |    * False positives -> Reduce precision                     |         |
 * |   +--------------------------------------------------------------+         |
 * |                                                                            |
 * +============================================================================+
 * ```
 *
 * ACTIVE INFERENCE FOR PROTECTIVE RESPONSES:
 * - High free energy triggers protective consolidation
 * - Precision modulation adjusts detection sensitivity
 * - Belief updates adapt to evolving attack patterns
 *
 * @see nimcp_security_hippocampus_bridge.h
 * @see nimcp_free_energy.h
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_SECURITY_HIPPOCAMPUS_FEP_BRIDGE_H
#define NIMCP_SECURITY_HIPPOCAMPUS_FEP_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "utils/bridge/nimcp_bridge_base.h"
#include "security/hippocampus/nimcp_security_hippocampus_bridge.h"
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

/** Free energy thresholds for memory security threat levels */
#define SEC_HIPPO_FEP_NORMAL_THRESHOLD      2.0f   /**< Below = normal operations */
#define SEC_HIPPO_FEP_SUSPICIOUS_THRESHOLD  5.0f   /**< Above = suspicious activity */
#define SEC_HIPPO_FEP_ATTACK_THRESHOLD      10.0f  /**< Above = attack detected */
#define SEC_HIPPO_FEP_CRITICAL_THRESHOLD    20.0f  /**< Above = critical threat */

/** Precision bounds for detection sensitivity */
#define SEC_HIPPO_FEP_MIN_PRECISION         0.1f   /**< Minimum precision */
#define SEC_HIPPO_FEP_MAX_PRECISION         10.0f  /**< Maximum precision */
#define SEC_HIPPO_FEP_DEFAULT_PRECISION     1.0f   /**< Default precision */

/** Surprise thresholds */
#define SEC_HIPPO_FEP_SURPRISE_NORMAL       2.0f   /**< Normal surprise level */
#define SEC_HIPPO_FEP_SURPRISE_ANOMALY      8.0f   /**< Anomaly threshold */

/** Prediction error thresholds */
#define SEC_HIPPO_FEP_PE_TOLERANCE          0.2f   /**< Normal prediction error */
#define SEC_HIPPO_FEP_PE_ATTACK             0.7f   /**< Attack threshold */

/** Bio-async module ID for security-hippocampus FEP bridge */
#define BIO_MODULE_SECURITY_HIPPOCAMPUS_FEP 0x0E21

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Memory security threat levels based on FEP metrics
 *
 * WHAT: Threat classification from free energy analysis
 * WHY:  Enable graded response to memory security events
 */
typedef enum {
    SEC_HIPPO_FEP_THREAT_NONE = 0,       /**< No threat detected */
    SEC_HIPPO_FEP_THREAT_SUSPICIOUS,     /**< Suspicious pattern */
    SEC_HIPPO_FEP_THREAT_MODERATE,       /**< Moderate threat */
    SEC_HIPPO_FEP_THREAT_HIGH,           /**< High threat level */
    SEC_HIPPO_FEP_THREAT_CRITICAL        /**< Critical - immediate response */
} sec_hippo_fep_threat_level_t;

/**
 * @brief Active inference response types
 *
 * WHAT: Types of protective responses via active inference
 * WHY:  Different threats require different countermeasures
 */
typedef enum {
    SEC_HIPPO_FEP_RESPONSE_NONE = 0,     /**< No response needed */
    SEC_HIPPO_FEP_RESPONSE_MONITOR,      /**< Increase monitoring */
    SEC_HIPPO_FEP_RESPONSE_PROTECT,      /**< Activate protection */
    SEC_HIPPO_FEP_RESPONSE_ISOLATE,      /**< Isolate affected memory */
    SEC_HIPPO_FEP_RESPONSE_RESTORE       /**< Restore from backup */
} sec_hippo_fep_response_t;

/* ============================================================================
 * Configuration Structures
 * ============================================================================ */

/**
 * @brief Security hippocampus FEP configuration
 *
 * WHAT: Configuration for FEP-security integration
 * WHY:  Control detection sensitivity and response parameters
 * HOW:  Adjustable thresholds, learning rates, and feature flags
 */
typedef struct {
    /* FEP parameters */
    float free_energy_threshold;          /**< FE threshold for threat detection */
    float surprise_threshold;             /**< Surprise threshold */
    float precision_learning_rate;        /**< Precision adaptation rate */

    /* Detection parameters */
    bool use_fep_detection;               /**< Use FEP for threat detection */
    bool enable_precision_modulation;     /**< Adapt precision dynamically */
    float normal_fe_threshold;            /**< FE threshold for normal ops */
    float critical_fe_threshold;          /**< FE threshold for critical */

    /* Memory integrity mapping */
    float integrity_to_fe_scale;          /**< Scale factor: integrity -> FE */
    float consolidation_pe_weight;        /**< Weight for consolidation errors */
    float replay_surprise_weight;         /**< Weight for replay surprises */

    /* Active inference settings */
    bool enable_active_inference;         /**< Enable protective responses */
    float response_threshold;             /**< FE threshold for response */
    float action_temperature;             /**< Softmax temperature for actions */

    /* Learning */
    bool enable_online_learning;          /**< Update FEP from detections */
    float learning_rate;                  /**< Belief update rate */
    bool learn_from_false_positives;      /**< Reduce precision on FP */
} sec_hippo_fep_config_t;

/* ============================================================================
 * Effects Structures
 * ============================================================================ */

/**
 * @brief FEP effects on security hippocampus (FEP -> Security)
 *
 * WHAT: How FEP analysis affects security operations
 * WHY:  Free energy provides threat detection signal
 */
typedef struct {
    float free_energy;                    /**< Current free energy */
    float surprise_level;                 /**< Current surprise */
    float prediction_error;               /**< Prediction error magnitude */

    sec_hippo_fep_threat_level_t threat_level; /**< Derived threat level */
    float threat_confidence;              /**< Threat detection confidence [0-1] */

    float detection_sensitivity;          /**< Precision-based sensitivity */
    float integrity_estimate;             /**< FEP-derived integrity estimate */

    sec_hippo_fep_response_t recommended_response; /**< Recommended action */
    float response_urgency;               /**< Response urgency [0-1] */
} fep_to_security_effects_t;

/**
 * @brief Security hippocampus effects on FEP (Security -> FEP)
 *
 * WHAT: How security detections affect FEP beliefs
 * WHY:  Security events update the generative model
 */
typedef struct {
    uint64_t attacks_detected;            /**< Total attacks detected */
    uint64_t normal_operations;           /**< Normal operations counted */
    uint64_t false_positives;             /**< Known false positives */

    float avg_integrity_score;            /**< Average memory integrity */
    float consolidation_health;           /**< Consolidation health [0-1] */
    float replay_validity;                /**< Replay validation score [0-1] */

    float current_threat_level;           /**< Current security threat [0-1] */
    bool under_attack;                    /**< Active attack in progress */
} security_to_fep_effects_t;

/* ============================================================================
 * State and Statistics Structures
 * ============================================================================ */

/**
 * @brief FEP bridge state
 *
 * WHAT: Current operational state of the bridge
 * WHY:  Track real-time status for monitoring and debugging
 */
typedef struct {
    bool active;                          /**< Whether bridge is active */
    uint64_t update_count;                /**< Number of updates */
    uint64_t detection_count;             /**< Detections processed */

    float current_precision;              /**< Current precision level */
    float avg_surprise;                   /**< Running average surprise */
    float avg_prediction_error;           /**< Running average PE */

    sec_hippo_fep_threat_level_t last_threat; /**< Last detected threat */
    uint64_t last_threat_time;            /**< Timestamp of last threat */
} sec_hippo_fep_state_t;

/**
 * @brief FEP bridge statistics
 *
 * WHAT: Cumulative statistics for the bridge
 * WHY:  Performance monitoring and tuning
 */
typedef struct {
    uint64_t total_updates;               /**< Total updates performed */
    uint64_t fep_detections;              /**< FEP-based detections */
    uint64_t threats_detected;            /**< Threats found */
    uint64_t protective_responses;        /**< Protective actions taken */
    uint64_t precision_adaptations;       /**< Precision updates */

    float avg_free_energy;                /**< Average free energy */
    float avg_surprise;                   /**< Average surprise */
    float avg_prediction_error;           /**< Average prediction error */
    float current_precision;              /**< Current precision */

    float max_free_energy;                /**< Maximum FE observed */
    float max_surprise;                   /**< Maximum surprise observed */

    uint64_t false_positive_count;        /**< False positives */
    uint64_t true_positive_count;         /**< True positives */
    float detection_accuracy;             /**< Detection accuracy rate */
} sec_hippo_fep_stats_t;

/* ============================================================================
 * Bridge Structure
 * ============================================================================ */

/**
 * @brief Security Hippocampus FEP Bridge
 *
 * WHAT: Main bridge structure connecting security hippocampus to FEP
 * WHY:  Centralized integration of memory security with free energy principle
 * HOW:  Contains configuration, connections, effects, and state
 */
typedef struct {
    bridge_base_t base;                   /**< MUST be first: base bridge infrastructure */

    sec_hippo_fep_config_t config;        /**< Configuration */

    /* System connections */
    sec_hippo_bridge_t* security_hippo;   /**< Connected security hippocampus */
    fep_system_t* fep_system;             /**< Connected FEP system */

    /* Bidirectional effects */
    fep_to_security_effects_t fep_effects;     /**< FEP -> Security effects */
    security_to_fep_effects_t security_effects; /**< Security -> FEP effects */

    /* State and statistics */
    sec_hippo_fep_state_t state;          /**< Current state */
    sec_hippo_fep_stats_t stats;          /**< Statistics */
} sec_hippo_fep_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default FEP bridge configuration
 *
 * WHAT: Provide sensible defaults for security-FEP integration
 * WHY:  Simplify initialization with biologically-plausible settings
 * HOW:  Return balanced defaults for detection and learning
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int sec_hippo_fep_default_config(sec_hippo_fep_config_t* config);

/**
 * @brief Create security hippocampus FEP bridge
 *
 * WHAT: Initialize FEP integration for hippocampus memory security
 * WHY:  Enable surprise-based threat detection
 * HOW:  Connect FEP system to security hippocampus, allocate structures
 *
 * @param config Configuration (NULL for defaults)
 * @param security_hippo Security hippocampus bridge handle
 * @param fep_system FEP system handle
 * @return Bridge handle or NULL on failure
 */
sec_hippo_fep_bridge_t* sec_hippo_fep_create(
    const sec_hippo_fep_config_t* config,
    sec_hippo_bridge_t* security_hippo,
    fep_system_t* fep_system
);

/**
 * @brief Destroy security hippocampus FEP bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Prevent memory leaks
 * HOW:  Free allocations, disconnect bio-async
 *
 * @param bridge Bridge handle (NULL safe)
 */
void sec_hippo_fep_destroy(sec_hippo_fep_bridge_t* bridge);

/**
 * @brief Reset bridge to initial state
 *
 * WHAT: Clear state while preserving connections
 * WHY:  Fresh start without reconnection
 * HOW:  Reset effects, state, and statistics
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int sec_hippo_fep_reset(sec_hippo_fep_bridge_t* bridge);

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
int sec_hippo_fep_get_config(
    const sec_hippo_fep_bridge_t* bridge,
    sec_hippo_fep_config_t* config
);

/**
 * @brief Set configuration
 *
 * WHAT: Update bridge configuration
 * WHY:  Allow runtime tuning of parameters
 * HOW:  Copy new config, validate bounds
 *
 * @param bridge Bridge handle
 * @param config New configuration
 * @return 0 on success, -1 on error
 */
int sec_hippo_fep_set_config(
    sec_hippo_fep_bridge_t* bridge,
    const sec_hippo_fep_config_t* config
);

/* ============================================================================
 * Update API
 * ============================================================================ */

/**
 * @brief Compute FEP effects on security
 *
 * WHAT: Compute FEP-derived security metrics
 * WHY:  Use free energy for threat detection
 * HOW:  Process current FEP state, compute effects
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int sec_hippo_fep_compute_effects(sec_hippo_fep_bridge_t* bridge);

/**
 * @brief Update from security detection
 *
 * WHAT: Feed security detection back to FEP
 * WHY:  Update generative model from detections
 * HOW:  Convert detection to FEP observation, update beliefs
 *
 * @param bridge Bridge handle
 * @param threat_detected Whether threat was detected
 * @param integrity_score Current memory integrity score [0-1]
 * @param consolidation_status Consolidation status
 * @return 0 on success, -1 on error
 */
int sec_hippo_fep_update_from_detection(
    sec_hippo_fep_bridge_t* bridge,
    bool threat_detected,
    float integrity_score,
    sec_hippo_consolidation_status_t consolidation_status
);

/**
 * @brief Update full bridge state
 *
 * WHAT: Main update loop for bridge synchronization
 * WHY:  Maintain bidirectional synchronization
 * HOW:  Compute effects in both directions, update state
 *
 * @param bridge Bridge handle
 * @param delta_ms Time since last update in milliseconds
 * @return 0 on success, -1 on error
 */
int sec_hippo_fep_update(sec_hippo_fep_bridge_t* bridge, uint64_t delta_ms);

/* ============================================================================
 * Detection API
 * ============================================================================ */

/**
 * @brief Detect threat using FEP analysis
 *
 * WHAT: Analyze memory state using FEP for threat detection
 * WHY:  Combine security metrics with FEP for enhanced detection
 * HOW:  Compute free energy, surprise, determine threat level
 *
 * @param bridge Bridge handle
 * @param integrity_score Memory integrity score [0-1]
 * @param consolidation_rate Current consolidation rate
 * @param replay_fidelity Replay fidelity score [0-1]
 * @param threat_level_out Output threat level
 * @param confidence_out Output detection confidence [0-1]
 * @return 0 on success, -1 on error
 */
int sec_hippo_fep_detect_threat(
    sec_hippo_fep_bridge_t* bridge,
    float integrity_score,
    float consolidation_rate,
    float replay_fidelity,
    sec_hippo_fep_threat_level_t* threat_level_out,
    float* confidence_out
);

/**
 * @brief Get recommended protective response
 *
 * WHAT: Determine appropriate response via active inference
 * WHY:  Actions minimize expected free energy
 * HOW:  Evaluate response policies, select optimal action
 *
 * @param bridge Bridge handle
 * @param response_out Output recommended response
 * @param urgency_out Output urgency level [0-1]
 * @return 0 on success, -1 on error
 */
int sec_hippo_fep_get_response(
    sec_hippo_fep_bridge_t* bridge,
    sec_hippo_fep_response_t* response_out,
    float* urgency_out
);

/**
 * @brief Report false positive detection
 *
 * WHAT: Update FEP on known false positive
 * WHY:  Reduce precision to prevent similar FPs
 * HOW:  Lower precision for observation type
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int sec_hippo_fep_report_false_positive(sec_hippo_fep_bridge_t* bridge);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get FEP effects on security
 *
 * @param bridge Bridge handle
 * @param effects Output effects
 * @return 0 on success, -1 on error
 */
int sec_hippo_fep_get_fep_effects(
    const sec_hippo_fep_bridge_t* bridge,
    fep_to_security_effects_t* effects
);

/**
 * @brief Get security effects on FEP
 *
 * @param bridge Bridge handle
 * @param effects Output effects
 * @return 0 on success, -1 on error
 */
int sec_hippo_fep_get_security_effects(
    const sec_hippo_fep_bridge_t* bridge,
    security_to_fep_effects_t* effects
);

/**
 * @brief Get bridge state
 *
 * @param bridge Bridge handle
 * @param state Output state
 * @return 0 on success, -1 on error
 */
int sec_hippo_fep_get_state(
    const sec_hippo_fep_bridge_t* bridge,
    sec_hippo_fep_state_t* state
);

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge handle
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int sec_hippo_fep_get_stats(
    const sec_hippo_fep_bridge_t* bridge,
    sec_hippo_fep_stats_t* stats
);

/**
 * @brief Get current free energy
 *
 * @param bridge Bridge handle
 * @return Current free energy or -1.0f on error
 */
float sec_hippo_fep_get_free_energy(const sec_hippo_fep_bridge_t* bridge);

/**
 * @brief Get current surprise level
 *
 * @param bridge Bridge handle
 * @return Current surprise or -1.0f on error
 */
float sec_hippo_fep_get_surprise(const sec_hippo_fep_bridge_t* bridge);

/**
 * @brief Get current threat level
 *
 * @param bridge Bridge handle
 * @return Current threat level
 */
sec_hippo_fep_threat_level_t sec_hippo_fep_get_threat_level(
    const sec_hippo_fep_bridge_t* bridge
);

/* ============================================================================
 * Bio-Async API
 * ============================================================================ */

/**
 * @brief Connect to bio-async router
 *
 * WHAT: Register bridge with bio-async system
 * WHY:  Enable inter-module security notifications
 * HOW:  Register module, setup inbox
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int sec_hippo_fep_connect_bio_async(sec_hippo_fep_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int sec_hippo_fep_disconnect_bio_async(sec_hippo_fep_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Bridge handle
 * @return true if connected
 */
bool sec_hippo_fep_is_bio_async_connected(const sec_hippo_fep_bridge_t* bridge);

/* ============================================================================
 * Utility API
 * ============================================================================ */

/**
 * @brief Get threat level name
 *
 * @param level Threat level
 * @return Human-readable name
 */
const char* sec_hippo_fep_threat_level_name(sec_hippo_fep_threat_level_t level);

/**
 * @brief Get response type name
 *
 * @param response Response type
 * @return Human-readable name
 */
const char* sec_hippo_fep_response_name(sec_hippo_fep_response_t response);

/**
 * @brief Print bridge summary
 *
 * WHAT: Print current bridge state (debug)
 * WHY:  Facilitate debugging and monitoring
 * HOW:  Format and print key metrics
 *
 * @param bridge Bridge handle
 */
void sec_hippo_fep_print_summary(const sec_hippo_fep_bridge_t* bridge);

/**
 * @brief Print statistics
 *
 * @param stats Statistics to print
 */
void sec_hippo_fep_print_stats(const sec_hippo_fep_stats_t* stats);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SECURITY_HIPPOCAMPUS_FEP_BRIDGE_H */
