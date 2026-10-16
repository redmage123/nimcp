/**
 * @file nimcp_security_training_fep_bridge.h
 * @brief Free Energy Principle bridge for Security Training Integration
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: FEP integration for training security - attack anomalies as surprise
 * WHY:  Training attacks (poisoning, gradient manipulation, backdoors) are high-surprise
 *       observations in the FEP framework; normal training behavior is expected (low free
 *       energy), while malicious interference deviates from predictions (high free energy)
 * HOW:  Map attack detection scores to free energy, anomaly patterns to prediction error,
 *       data source trust to precision weighting, and protective responses to active inference
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * NEURAL IMMUNE RESPONSE AS PREDICTIVE PROCESSING:
 * The brain's immune system (microglia, astrocytes) continuously monitors for threats,
 * operating under a predictive model where healthy tissue is expected and pathology
 * generates prediction errors. Training security operates analogously:
 *
 * - Healthy gradients = normal synaptic plasticity (low free energy)
 * - Poisoned data = neural toxins (high free energy spikes)
 * - Gradient manipulation = aberrant signaling (prediction error)
 * - Backdoor insertion = trojan pathways (model integrity violation)
 * - Model extraction = unauthorized memory access (suspicious access patterns)
 *
 * FEP-SECURITY TRAINING MAPPING:
 * ```
 * Training Security                  Free Energy Framework
 * -------------------------------------------------------------------------
 * Training sample                    -> Observation o
 * Expected distribution              -> Prediction g(mu)
 * Anomaly score                      -> Prediction error epsilon = o - g(mu)
 * Attack detection score             -> Free energy F
 * Data source trust                  -> Precision Pi (inverse variance)
 * Protective response (quarantine)   -> High-surprise response
 * Model recovery (rollback)          -> Belief update mu' = mu - lr*dF/dmu
 * ```
 *
 * PRECISION-WEIGHTED TRUST MODEL:
 * High-trust data sources have high precision (their samples matter more).
 * Low-trust sources have low precision (their samples are discounted):
 *
 *   Weighted contribution = Pi_source * sample_gradient
 *   Aggregation = Sum(Pi_i * g_i) / Sum(Pi_i)
 *
 * ACTIVE INFERENCE FOR PROTECTION:
 * The system performs active inference to minimize expected free energy:
 * - Policies = {monitor, quarantine_samples, sanitize_gradients, halt_training, rollback}
 * - Action selection minimizes G(pi) = Risk + Ambiguity
 * - Risk = divergence from secure (attack-free) state
 * - Ambiguity = uncertainty about attack presence
 *
 * ATTACK DETECTION VIA FREE ENERGY:
 * ```
 * Attack Type                | Detection Signal           | FE Response
 * ---------------------------|----------------------------|------------------------
 * Data Poisoning             | Distribution shift         | High prediction error
 * Gradient Manipulation      | Gradient statistics anomaly| Precision reduction
 * Model Extraction           | Query pattern anomaly      | Surprise spike
 * Backdoor Insertion         | Trigger pattern detection  | Critical FE threshold
 * ```
 *
 * ARCHITECTURE:
 * ```
 * +=========================================================================+
 * |        SECURITY TRAINING - FEP BRIDGE (Attack Detection)                |
 * +=========================================================================+
 * |                                                                         |
 * |   +------------------+         +------------------------+               |
 * |   |  FEP System      |-------->|  Security Training     |               |
 * |   |                  |         |  Bridge                |               |
 * |   | - Free Energy    |         |                        |               |
 * |   | - Surprise       |         | - Poisoning Detection  |               |
 * |   | - Precision      |         | - Gradient Sanitization|               |
 * |   +------------------+         | - Model Integrity      |               |
 * |           |                    +------------------------+               |
 * |           v                                                             |
 * |   +------------------------------------------------------------------+  |
 * |   |              BIDIRECTIONAL EFFECTS                               |  |
 * |   |                                                                  |  |
 * |   |  FEP -> Security:                                                |  |
 * |   |    - Free energy -> Detection threshold modulation               |  |
 * |   |    - Surprise -> Quarantine/rollback trigger                     |  |
 * |   |    - Precision -> Detection sensitivity                          |  |
 * |   |    - EFE -> Protective action selection                          |  |
 * |   |                                                                  |  |
 * |   |  Security -> FEP:                                                |  |
 * |   |    - Attack scores -> High-surprise observations                 |  |
 * |   |    - Anomaly patterns -> Prediction errors                       |  |
 * |   |    - Data source trust -> Precision weights                      |  |
 * |   |    - Protective actions -> Belief updates                        |  |
 * |   +------------------------------------------------------------------+  |
 * |                                                                         |
 * +=========================================================================+
 * ```
 *
 * DETECTION THRESHOLDS (Free Energy Based):
 * ```
 * Free Energy Range    | Threat Status           | Action
 * ---------------------|-------------------------|---------------------------
 * < 2.0                | Normal training         | Full trust, high precision
 * 2.0 - 5.0            | Suspicious              | Monitor, log anomalies
 * 5.0 - 10.0           | Likely attack           | Flag for review, reduce trust
 * 10.0 - 20.0          | Attack detected         | Quarantine, sanitize
 * > 20.0               | Critical threat         | Halt training, rollback
 * ```
 *
 * GOTCHAS:
 * ========
 * - FEP bridges return 0 for success, -1 for errors (not NIMCP_OK/NIMCP_ERROR_*)
 * - Precision values must be > 0 to avoid division by zero
 * - Free energy computation requires at least one observation
 * - Worker precisions array is shallow-copied in get_effects
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_SECURITY_TRAINING_FEP_BRIDGE_H
#define NIMCP_SECURITY_TRAINING_FEP_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "utils/bridge/nimcp_bridge_base.h"
#include "security/training/nimcp_security_training_bridge.h"
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

/** Module identification */
#define SECURITY_TRAINING_FEP_MODULE_NAME     "security_training_fep"
#define SECURITY_TRAINING_FEP_MODULE_VERSION  "1.0.0"

/** Free energy thresholds for attack detection */
#define SECURITY_TRAIN_FEP_NORMAL_THRESHOLD       2.0f   /**< Below = normal training */
#define SECURITY_TRAIN_FEP_SUSPICIOUS_THRESHOLD   5.0f   /**< Monitor mode */
#define SECURITY_TRAIN_FEP_ATTACK_THRESHOLD       10.0f  /**< Attack detected */
#define SECURITY_TRAIN_FEP_CRITICAL_THRESHOLD     20.0f  /**< Critical threat */

/** Precision bounds for detection sensitivity */
#define SECURITY_TRAIN_FEP_MIN_PRECISION          0.05f  /**< Minimum precision */
#define SECURITY_TRAIN_FEP_MAX_PRECISION          20.0f  /**< Maximum precision */
#define SECURITY_TRAIN_FEP_DEFAULT_PRECISION      1.0f   /**< Default precision */

/** Trust-precision mapping */
#define SECURITY_TRAIN_FEP_UNTRUSTED_PRECISION    0.1f   /**< Untrusted source */
#define SECURITY_TRAIN_FEP_VERIFIED_PRECISION     0.5f   /**< Verified source */
#define SECURITY_TRAIN_FEP_CERTIFIED_PRECISION    0.8f   /**< Certified source */
#define SECURITY_TRAIN_FEP_INTERNAL_PRECISION     1.0f   /**< Internal source */

/** Learning rates */
#define SECURITY_TRAIN_FEP_DEFAULT_BELIEF_LR      0.1f   /**< Belief update rate */
#define SECURITY_TRAIN_FEP_DEFAULT_PRECISION_LR   0.05f  /**< Precision adaptation rate */

/** Bio-async */
#define SECURITY_TRAIN_FEP_BIO_INBOX_CAPACITY     64

/** Maximum tracked data sources */
#define SECURITY_TRAIN_FEP_MAX_DATA_SOURCES       256

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Protective action policies
 *
 * WHAT: Available protective responses
 * WHY:  Active inference selects optimal policy
 * HOW:  Each policy has associated EFE cost
 */
typedef enum {
    SECURITY_TRAIN_POLICY_MONITOR = 0,      /**< Passive monitoring */
    SECURITY_TRAIN_POLICY_QUARANTINE,       /**< Quarantine samples */
    SECURITY_TRAIN_POLICY_SANITIZE,         /**< Sanitize gradients */
    SECURITY_TRAIN_POLICY_HALT,             /**< Halt training */
    SECURITY_TRAIN_POLICY_ROLLBACK,         /**< Rollback to checkpoint */
    SECURITY_TRAIN_POLICY_COUNT
} security_train_policy_t;

/**
 * @brief Attack severity levels
 *
 * WHAT: Severity classification of detected attacks
 * WHY:  Informs response urgency
 * HOW:  Derived from free energy magnitude
 */
typedef enum {
    SECURITY_TRAIN_SEVERITY_NONE = 0,       /**< No attack detected */
    SECURITY_TRAIN_SEVERITY_LOW,            /**< Minor anomaly */
    SECURITY_TRAIN_SEVERITY_MEDIUM,         /**< Possible attack */
    SECURITY_TRAIN_SEVERITY_HIGH,           /**< Confirmed attack */
    SECURITY_TRAIN_SEVERITY_CRITICAL        /**< Critical threat */
} security_train_severity_t;

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Security training FEP configuration
 *
 * WHAT: Configuration for FEP-security training integration
 * WHY:  Customize attack detection via free energy framework
 * HOW:  Thresholds, learning rates, and feature enables
 */
typedef struct {
    /* FEP parameters */
    float attack_fe_threshold;              /**< Free energy threshold for attack */
    float surprise_threshold;               /**< Surprise threshold for action */
    float precision_learning_rate;          /**< Precision adaptation rate */

    /* Detection parameters */
    bool use_fep_scoring;                   /**< Use FEP for attack scoring */
    bool enable_precision_modulation;       /**< Adapt precision from trust levels */
    float normal_fe_threshold;              /**< FE threshold for normal training */
    float critical_fe_threshold;            /**< FE threshold for critical threat */

    /* Trust-precision coupling */
    bool enable_trust_precision_coupling;   /**< Map trust levels to precision */
    float trust_precision_scale;            /**< Scaling factor for trust->precision */

    /* Learning */
    bool enable_online_learning;            /**< Update FEP from detections */
    float belief_learning_rate;             /**< Belief update rate */
    bool learn_from_responses;              /**< Update beliefs on protective actions */

    /* Active inference for protection */
    bool enable_active_defense;             /**< Use EFE for policy selection */
    float action_temperature;               /**< Softmax temperature for actions */

    /* Attack-specific thresholds */
    float poisoning_fe_weight;              /**< Weight for poisoning in FE */
    float gradient_fe_weight;               /**< Weight for gradient attacks in FE */
    float extraction_fe_weight;             /**< Weight for extraction attacks in FE */
    float backdoor_fe_weight;               /**< Weight for backdoor attacks in FE */

    /* Bio-async */
    bool enable_bio_async;                  /**< Enable bio-async messaging */
    uint32_t bio_inbox_capacity;            /**< Message queue size */
} security_train_fep_config_t;

/**
 * @brief FEP effects on security training (FEP -> Security)
 *
 * WHAT: FEP-derived modulation of security parameters
 * WHY:  Free energy informs detection thresholds and sensitivity
 * HOW:  Computed from current FEP state each update
 */
typedef struct {
    /* Detection modulation */
    float detection_threshold_scale;        /**< Scale factor for detection thresholds */
    float detection_sensitivity;            /**< Precision-based sensitivity [0-1] */
    float action_urgency;                   /**< Urgency for protective action [0-1] */

    /* Per-source precision */
    float* source_precisions;               /**< Precision weight per data source */
    uint32_t num_source_precisions;         /**< Number of sources tracked */

    /* Free energy metrics */
    float current_free_energy;              /**< Current system free energy */
    float surprise_level;                   /**< Current surprise level */
    float prediction_error_magnitude;       /**< Aggregate prediction error */

    /* Per-attack-type free energy */
    float poisoning_free_energy;            /**< FE from poisoning detection */
    float gradient_free_energy;             /**< FE from gradient anomalies */
    float extraction_free_energy;           /**< FE from extraction patterns */
    float backdoor_free_energy;             /**< FE from backdoor detection */

    /* Active defense */
    float policy_scores[SECURITY_TRAIN_POLICY_COUNT]; /**< EFE for each policy */
    security_train_policy_t recommended_policy;       /**< Recommended policy */

    /* Attack assessment */
    float attack_probability;               /**< Overall attack probability [0-1] */
    security_train_severity_t severity;     /**< Current severity level */
    float detection_confidence;             /**< Confidence in detection */

    /* Metadata */
    uint64_t last_update_ms;                /**< When effects were computed */
    bool valid;                             /**< Whether effects are current */
} security_train_fep_effects_t;

/**
 * @brief Security effects on FEP (Security -> FEP)
 *
 * WHAT: Security-derived updates to FEP system
 * WHY:  Detection outcomes inform belief updates and precision
 * HOW:  Converted from security events each cycle
 */
typedef struct {
    /* Detection statistics */
    uint64_t poisoning_detections;          /**< Data poisoning detections */
    uint64_t gradient_manipulations;        /**< Gradient manipulation detections */
    uint64_t extraction_attempts;           /**< Model extraction attempts */
    uint64_t backdoor_detections;           /**< Backdoor insertion detections */
    uint64_t normal_observations;           /**< Normal training observations */

    /* Aggregate metrics */
    float avg_poisoning_score;              /**< Average poisoning score */
    float avg_gradient_anomaly;             /**< Average gradient anomaly */
    float avg_extraction_score;             /**< Average extraction score */
    float avg_backdoor_score;               /**< Average backdoor score */
    float current_threat_level;             /**< Overall threat level [0-1] */

    /* Training health */
    float loss_stability;                   /**< Loss stability metric */
    float gradient_stability;               /**< Gradient stability metric */
    bool training_halted;                   /**< Whether training was halted */
    uint64_t rollbacks_performed;           /**< Number of rollbacks */

    /* Data source statistics */
    uint32_t trusted_sources;               /**< Number of trusted sources */
    uint32_t blocked_sources;               /**< Number of blocked sources */
    float avg_source_trust;                 /**< Average source trust */

    /* Protective actions taken */
    uint64_t samples_quarantined;           /**< Samples quarantined */
    uint64_t gradients_sanitized;           /**< Gradient sanitizations */

    /* Metadata */
    uint64_t timestamp_ms;                  /**< When captured */
    bool valid;                             /**< Whether valid */
} fep_security_train_effects_t;

/**
 * @brief FEP bridge internal state
 */
typedef struct {
    bool active;                            /**< Whether bridge is active */
    uint64_t update_count;                  /**< Number of updates */
    uint64_t detection_cycles;              /**< Detection cycles processed */

    /* Precision tracking */
    float system_precision;                 /**< Current system precision */
    float avg_source_precision;             /**< Average per-source precision */

    /* Surprise tracking */
    float avg_surprise;                     /**< Running average surprise */
    float max_surprise_seen;                /**< Maximum surprise observed */

    /* Per-attack-type state */
    float poisoning_baseline;               /**< Baseline for poisoning detection */
    float gradient_baseline;                /**< Baseline for gradient detection */
    float extraction_baseline;              /**< Baseline for extraction detection */
    float backdoor_baseline;                /**< Baseline for backdoor detection */

    /* Expected patterns */
    float* expected_gradient_stats;         /**< Expected gradient statistics */
    uint32_t gradient_stats_dim;            /**< Dimensionality */
} security_train_fep_state_t;

/**
 * @brief FEP bridge statistics
 */
typedef struct {
    /* Update counts */
    uint64_t total_updates;                 /**< Total update calls */
    uint64_t fep_updates;                   /**< FEP state updates */
    uint64_t security_updates;              /**< Security state updates */

    /* Detection statistics */
    uint64_t detections_processed;          /**< Total detections analyzed */
    uint64_t fep_triggered_actions;         /**< Actions triggered by FEP */
    uint64_t precision_adaptations;         /**< Precision update count */

    /* Free energy metrics */
    float avg_free_energy;                  /**< Average free energy */
    float max_free_energy;                  /**< Maximum free energy seen */
    float avg_surprise;                     /**< Average surprise */
    float current_precision;                /**< Current system precision */

    /* Attack metrics */
    float avg_attack_score;                 /**< Average attack score */
    uint64_t attack_events;                 /**< Attack events detected */

    /* Per-attack-type counts */
    uint64_t poisoning_events;              /**< Poisoning events */
    uint64_t gradient_events;               /**< Gradient manipulation events */
    uint64_t extraction_events;             /**< Extraction attempt events */
    uint64_t backdoor_events;               /**< Backdoor insertion events */

    /* Performance */
    float avg_update_time_us;               /**< Average update time */
    uint64_t bio_async_messages_sent;       /**< Bio-async messages sent */
    uint64_t bio_async_messages_received;   /**< Bio-async messages received */

    /* Connection status */
    bool fep_connected;                     /**< FEP system connected */
    bool security_connected;                /**< Security bridge connected */
    bool bio_async_connected;               /**< Bio-async connected */
} security_train_fep_stats_t;

/**
 * @brief Security training FEP bridge
 *
 * WHAT: Complete FEP-security training integration bridge
 * WHY:  Enable free energy-based attack detection
 * HOW:  Bidirectional effects, precision modulation, active defense
 */
typedef struct {
    bridge_base_t base;                     /**< MUST be first: base bridge */

    /* Connected systems */
    fep_system_t* fep_system;               /**< FEP system */
    security_training_bridge_t* security_bridge; /**< Security training bridge */

    /* Configuration */
    security_train_fep_config_t config;

    /* Bidirectional effects */
    security_train_fep_effects_t fep_effects;     /**< FEP -> Security */
    fep_security_train_effects_t security_effects; /**< Security -> FEP */

    /* Internal state */
    security_train_fep_state_t state;

    /* Statistics */
    security_train_fep_stats_t stats;
} security_train_fep_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default FEP bridge configuration
 *
 * WHAT: Provide sensible defaults for FEP-security training integration
 * WHY:  Simplify initialization with secure, biologically-plausible defaults
 * HOW:  Return struct with conservative thresholds
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int security_train_fep_default_config(security_train_fep_config_t* config);

/**
 * @brief Create security training FEP bridge
 *
 * WHAT: Initialize FEP integration for training security
 * WHY:  Enable free energy-based attack detection
 * HOW:  Allocate bridge, connect systems, initialize state
 *
 * @param config Configuration (NULL for defaults)
 * @param fep_system FEP system handle
 * @param security_bridge Security training bridge handle
 * @return Bridge handle or NULL on failure
 */
security_train_fep_bridge_t* security_train_fep_create(
    const security_train_fep_config_t* config,
    fep_system_t* fep_system,
    security_training_bridge_t* security_bridge
);

/**
 * @brief Destroy security training FEP bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Prevent memory leaks
 * HOW:  Disconnect bio-async, free allocations, destroy mutex
 *
 * @param bridge Bridge handle (NULL safe)
 */
void security_train_fep_destroy(security_train_fep_bridge_t* bridge);

/**
 * @brief Reset bridge to initial state
 *
 * WHAT: Clear state while preserving connections
 * WHY:  Allow reuse after training session
 * HOW:  Zero state/stats, keep config and connections
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int security_train_fep_reset(security_train_fep_bridge_t* bridge);

/* ============================================================================
 * Configuration API
 * ============================================================================ */

/**
 * @brief Get current configuration
 *
 * @param bridge Bridge handle
 * @param config Output configuration
 * @return 0 on success, -1 on failure
 */
int security_train_fep_get_config(
    const security_train_fep_bridge_t* bridge,
    security_train_fep_config_t* config
);

/**
 * @brief Set configuration
 *
 * WHAT: Update bridge configuration
 * WHY:  Adjust behavior at runtime
 * HOW:  Copy config, validate, apply
 *
 * @param bridge Bridge handle
 * @param config New configuration
 * @return 0 on success, -1 on failure
 */
int security_train_fep_set_config(
    security_train_fep_bridge_t* bridge,
    const security_train_fep_config_t* config
);

/* ============================================================================
 * Update API
 * ============================================================================ */

/**
 * @brief Compute FEP effects on security training
 *
 * WHAT: Compute FEP-derived modulation of security parameters
 * WHY:  Free energy informs detection thresholds and precision
 * HOW:  Process current FEP state, update effects structure
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int security_train_fep_compute_effects(security_train_fep_bridge_t* bridge);

/**
 * @brief Update FEP from poisoning detection
 *
 * WHAT: Feed poisoning detection results to FEP
 * WHY:  Poisoning is a high-surprise observation
 * HOW:  Convert detection to FEP observation, compute free energy
 *
 * @param bridge Bridge handle
 * @param poisoning_result Poisoning detection result
 * @return 0 on success, -1 on failure
 */
int security_train_fep_update_from_poisoning(
    security_train_fep_bridge_t* bridge,
    const security_poisoning_result_t* poisoning_result
);

/**
 * @brief Update FEP from gradient anomaly
 *
 * WHAT: Feed gradient anomaly detection to FEP
 * WHY:  Gradient manipulation is prediction error
 * HOW:  Map gradient statistics to FEP observation
 *
 * @param bridge Bridge handle
 * @param anomaly_score Gradient anomaly score [0-1]
 * @param gradient_norm Current gradient norm
 * @param expected_norm Expected gradient norm
 * @return 0 on success, -1 on failure
 */
int security_train_fep_update_from_gradient_anomaly(
    security_train_fep_bridge_t* bridge,
    float anomaly_score,
    float gradient_norm,
    float expected_norm
);

/**
 * @brief Update FEP from model extraction attempt
 *
 * WHAT: Feed extraction detection to FEP
 * WHY:  Unusual query patterns generate surprise
 * HOW:  Map extraction metrics to FEP observation
 *
 * @param bridge Bridge handle
 * @param extraction_score Extraction score [0-1]
 * @param query_rate Query rate (queries per second)
 * @return 0 on success, -1 on failure
 */
int security_train_fep_update_from_extraction_attempt(
    security_train_fep_bridge_t* bridge,
    float extraction_score,
    float query_rate
);

/**
 * @brief Update FEP from backdoor detection
 *
 * WHAT: Feed backdoor detection to FEP
 * WHY:  Backdoor triggers are critical surprise events
 * HOW:  Map backdoor metrics to high-surprise observation
 *
 * @param bridge Bridge handle
 * @param backdoor_score Backdoor detection score [0-1]
 * @param trigger_confidence Confidence in trigger detection [0-1]
 * @return 0 on success, -1 on failure
 */
int security_train_fep_update_from_backdoor_detection(
    security_train_fep_bridge_t* bridge,
    float backdoor_score,
    float trigger_confidence
);

/**
 * @brief Update data source precision from trust level
 *
 * WHAT: Map data source trust to FEP precision weight
 * WHY:  High-trust sources should have more influence
 * HOW:  Convert trust level to precision, update source weights
 *
 * @param bridge Bridge handle
 * @param source_name Data source identifier
 * @param trust_level Source's current trust level
 * @return 0 on success, -1 on failure
 */
int security_train_fep_update_source_precision(
    security_train_fep_bridge_t* bridge,
    const char* source_name,
    security_data_trust_t trust_level
);

/**
 * @brief Report protective action to FEP
 *
 * WHAT: Inform FEP that a protective action was taken
 * WHY:  Actions are responses to high surprise that update beliefs
 * HOW:  Record action, update generative model
 *
 * @param bridge Bridge handle
 * @param action Type of action taken
 * @param success Whether action was successful
 * @return 0 on success, -1 on failure
 */
int security_train_fep_report_action(
    security_train_fep_bridge_t* bridge,
    security_train_policy_t action,
    bool success
);

/**
 * @brief Report false positive to FEP
 *
 * WHAT: Update FEP on corrected false positive
 * WHY:  Reduce precision to prevent similar false detections
 * HOW:  Lower precision for observation type, update beliefs
 *
 * @param bridge Bridge handle
 * @param attack_type Type of attack that was false positive
 * @return 0 on success, -1 on failure
 */
int security_train_fep_report_false_positive(
    security_train_fep_bridge_t* bridge,
    security_poisoning_type_t attack_type
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get FEP effects on security training
 *
 * @param bridge Bridge handle
 * @param effects Output effects
 * @return 0 on success, -1 on failure
 */
int security_train_fep_get_fep_effects(
    const security_train_fep_bridge_t* bridge,
    security_train_fep_effects_t* effects
);

/**
 * @brief Get security effects on FEP
 *
 * @param bridge Bridge handle
 * @param effects Output effects
 * @return 0 on success, -1 on failure
 */
int security_train_fep_get_security_effects(
    const security_train_fep_bridge_t* bridge,
    fep_security_train_effects_t* effects
);

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge handle
 * @param stats Output statistics
 * @return 0 on success, -1 on failure
 */
int security_train_fep_get_stats(
    const security_train_fep_bridge_t* bridge,
    security_train_fep_stats_t* stats
);

/**
 * @brief Get current free energy
 *
 * @param bridge Bridge handle
 * @return Current free energy or -1.0f on error
 */
float security_train_fep_get_free_energy(const security_train_fep_bridge_t* bridge);

/**
 * @brief Get current surprise level
 *
 * @param bridge Bridge handle
 * @return Current surprise or -1.0f on error
 */
float security_train_fep_get_surprise(const security_train_fep_bridge_t* bridge);

/**
 * @brief Get data source precision
 *
 * @param bridge Bridge handle
 * @param source_name Data source identifier
 * @return Source's precision weight or -1.0f on error
 */
float security_train_fep_get_source_precision(
    const security_train_fep_bridge_t* bridge,
    const char* source_name
);

/**
 * @brief Get attack score from FEP
 *
 * WHAT: Compute FEP-based attack score for current state
 * WHY:  Provides unified anomaly metric based on free energy
 * HOW:  Normalize free energy to [0-1] attack score
 *
 * @param bridge Bridge handle
 * @return Attack score [0-1] or -1.0f on error
 */
float security_train_fep_get_attack_score(const security_train_fep_bridge_t* bridge);

/**
 * @brief Get current severity level
 *
 * WHAT: Determine severity based on free energy
 * WHY:  Informs response urgency
 * HOW:  Map free energy to severity enum
 *
 * @param bridge Bridge handle
 * @return Current severity level
 */
security_train_severity_t security_train_fep_get_severity(
    const security_train_fep_bridge_t* bridge
);

/**
 * @brief Get recommended protective policy
 *
 * WHAT: Get policy with lowest expected free energy
 * WHY:  Active inference recommends optimal action
 * HOW:  Return policy with minimum EFE
 *
 * @param bridge Bridge handle
 * @return Recommended policy
 */
security_train_policy_t security_train_fep_get_recommended_policy(
    const security_train_fep_bridge_t* bridge
);

/**
 * @brief Check if protective action is recommended
 *
 * WHAT: Determine if current free energy warrants action
 * WHY:  High surprise should trigger protection
 * HOW:  Compare free energy to threshold
 *
 * @param bridge Bridge handle
 * @return true if action recommended
 */
bool security_train_fep_should_act(const security_train_fep_bridge_t* bridge);

/* ============================================================================
 * Bio-Async API
 * ============================================================================ */

/**
 * @brief Connect to bio-async router
 *
 * WHAT: Register bridge with bio-async system
 * WHY:  Enable inter-module security alert notifications
 * HOW:  Register module, setup inbox
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int security_train_fep_connect_bio_async(security_train_fep_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int security_train_fep_disconnect_bio_async(security_train_fep_bridge_t* bridge);

/**
 * @brief Process incoming bio-async messages
 *
 * WHAT: Handle pending messages from bio-async inbox
 * WHY:  Respond to alerts from other modules
 * HOW:  Process inbox using bio_router_process_inbox
 *
 * @param bridge Bridge handle
 * @return Number of messages processed, or -1 on error
 */
int security_train_fep_process_messages(security_train_fep_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Bridge handle
 * @return true if connected
 */
bool security_train_fep_is_bio_async_connected(
    const security_train_fep_bridge_t* bridge
);

/* ============================================================================
 * String Conversion API
 * ============================================================================ */

/**
 * @brief Convert policy to string
 *
 * @param policy Policy value
 * @return Human-readable string
 */
const char* security_train_policy_to_string(security_train_policy_t policy);

/**
 * @brief Convert severity to string
 *
 * @param severity Severity value
 * @return Human-readable string
 */
const char* security_train_severity_to_string(security_train_severity_t severity);

/* ============================================================================
 * Debug API
 * ============================================================================ */

/**
 * @brief Print bridge summary to stdout
 *
 * WHAT: Display human-readable bridge state
 * WHY:  Debugging and monitoring
 * HOW:  Format and print key metrics
 *
 * @param bridge Bridge handle
 */
void security_train_fep_print_summary(const security_train_fep_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SECURITY_TRAINING_FEP_BRIDGE_H */
