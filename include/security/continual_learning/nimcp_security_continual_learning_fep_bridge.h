/**
 * @file nimcp_security_continual_learning_fep_bridge.h
 * @brief Free Energy Principle Bridge for Security Continual Learning
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: FEP integration for continual learning security - catastrophic forgetting as surprise
 * WHY:  Forgetting attacks represent high-surprise deviations from expected knowledge retention
 * HOW:  Map knowledge retention to free energy, memory drift to prediction error, protective
 *       consolidation to active inference response
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * CATASTROPHIC FORGETTING AS FREE ENERGY VIOLATION:
 * - Normal knowledge retention = low free energy (expected model behavior)
 * - Catastrophic forgetting = high free energy (surprising knowledge loss)
 * - Memory consolidation = generative model maintenance
 * - Replay buffer = experience repository for belief updating
 *
 * FEP INTEGRATION:
 * ```
 * Knowledge State K(t) -> Expected Retention Model
 *         |
 *         v
 * Retention Prediction: R_pred = E[Accuracy(t) | K(t-1)]
 *         |
 *         v
 * Retention Error: epsilon = R_actual - R_pred
 *         |
 *         v
 * Free Energy F = f(Retention_Loss, Drift_Magnitude, Replay_Anomaly)
 *         |
 *         v
 * Surprise = -ln p(Current_State | Expected_State)
 *         |
 *         v
 * Protective Response = Active Inference (EWC boost, LR reduction, replay lockdown)
 * ```
 *
 * DETECTION MAPPING:
 * - Low FE (<2.0)   -> Normal learning dynamics
 * - Medium FE (2-5) -> Suspicious drift (monitor closely)
 * - High FE (5-10)  -> Potential forgetting attack (active defense)
 * - Very high (>10) -> Critical attack (emergency consolidation)
 *
 * ARCHITECTURE:
 * ```
 * +===========================================================================+
 * |     SECURITY CONTINUAL LEARNING - FEP BRIDGE (Forgetting as Surprise)     |
 * +===========================================================================+
 * |                                                                            |
 * |   +------------------+           +--------------------------------+        |
 * |   |   FEP System     |---------->| Security Continual Learning   |        |
 * |   |                  |           |                                |        |
 * |   | * Free Energy    |           | * Retention Monitor            |        |
 * |   | * Surprise       |           | * Drift Detection              |        |
 * |   | * Precision      |           | * Replay Verification          |        |
 * |   +------------------+           +--------------------------------+        |
 * |           |                                    |                          |
 * |           v                                    v                          |
 * |   +---------------------------------------------------------------+       |
 * |   |                BIDIRECTIONAL EFFECTS                          |       |
 * |   |                                                               |       |
 * |   |   FEP -> Security CL:                                         |       |
 * |   |     * Free energy -> Forgetting severity score                |       |
 * |   |     * Surprise -> Attack likelihood                           |       |
 * |   |     * Precision -> Protection sensitivity                     |       |
 * |   |     * Active inference -> Protective consolidation            |       |
 * |   |                                                               |       |
 * |   |   Security CL -> FEP:                                         |       |
 * |   |     * Retention loss -> Observation (high surprise)           |       |
 * |   |     * Drift detection -> Update generative model              |       |
 * |   |     * Replay anomaly -> Increase precision                    |       |
 * |   |     * False positives -> Reduce precision                     |       |
 * |   +---------------------------------------------------------------+       |
 * |                                                                            |
 * +===========================================================================+
 * ```
 *
 * KEY MAPPINGS:
 * - Knowledge retention loss -> Free energy
 * - Memory consolidation drift -> Prediction error
 * - Replay buffer anomaly -> Surprise level
 * - Protective consolidation -> Active inference response
 *
 * GOTCHAS:
 * - Retention baseline must be established before FEP integration
 * - High precision = sensitive detection but more false positives
 * - FEP updates require recent retention data (staleness check)
 * - Bio-async messages may be delayed under high load
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

#ifndef NIMCP_SECURITY_CONTINUAL_LEARNING_FEP_BRIDGE_H
#define NIMCP_SECURITY_CONTINUAL_LEARNING_FEP_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "utils/bridge/nimcp_bridge_base.h"
#include "security/continual/nimcp_security_continual_learning_bridge.h"
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

/** Free energy thresholds based on forgetting severity */
#define SECURITY_CL_FEP_NORMAL_THRESHOLD         2.0f
#define SECURITY_CL_FEP_SUSPICIOUS_THRESHOLD     5.0f
#define SECURITY_CL_FEP_ATTACK_THRESHOLD         10.0f
#define SECURITY_CL_FEP_CRITICAL_THRESHOLD       20.0f

/** Precision bounds for detection sensitivity */
#define SECURITY_CL_FEP_MIN_PRECISION            0.1f
#define SECURITY_CL_FEP_MAX_PRECISION            10.0f
#define SECURITY_CL_FEP_DEFAULT_PRECISION        1.0f

/** Retention-to-free-energy conversion */
#define SECURITY_CL_FEP_RETENTION_SCALE          10.0f
#define SECURITY_CL_FEP_DRIFT_SCALE              5.0f
#define SECURITY_CL_FEP_REPLAY_SCALE             8.0f

/** Learning rates */
#define SECURITY_CL_FEP_BELIEF_LR                0.1f
#define SECURITY_CL_FEP_PRECISION_LR             0.05f

/** Bio-async configuration */
#define SECURITY_CL_FEP_BIO_INBOX_CAPACITY       64

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Forgetting attack severity based on free energy
 *
 * WHAT: Classify forgetting attack severity from FEP metrics
 * WHY:  Different severities require different protective responses
 * HOW:  Map free energy magnitude to severity levels
 */
typedef enum {
    SECURITY_CL_FEP_SEVERITY_NONE = 0,       /**< No attack detected (FE < 2) */
    SECURITY_CL_FEP_SEVERITY_LOW,            /**< Suspicious drift (FE 2-5) */
    SECURITY_CL_FEP_SEVERITY_MEDIUM,         /**< Likely attack (FE 5-10) */
    SECURITY_CL_FEP_SEVERITY_HIGH,           /**< Confirmed attack (FE 10-20) */
    SECURITY_CL_FEP_SEVERITY_CRITICAL        /**< Emergency (FE > 20) */
} security_cl_fep_severity_t;

/**
 * @brief Active inference response type
 *
 * WHAT: Protective actions derived from active inference
 * WHY:  Map FEP action selection to security responses
 * HOW:  Active inference selects minimal EFE protective action
 */
typedef enum {
    SECURITY_CL_FEP_RESPONSE_NONE = 0,       /**< No action needed */
    SECURITY_CL_FEP_RESPONSE_MONITOR,        /**< Increase monitoring */
    SECURITY_CL_FEP_RESPONSE_EWC_BOOST,      /**< Boost EWC regularization */
    SECURITY_CL_FEP_RESPONSE_LR_REDUCE,      /**< Reduce learning rate */
    SECURITY_CL_FEP_RESPONSE_REPLAY_LOCK,    /**< Lock replay buffer */
    SECURITY_CL_FEP_RESPONSE_CONSOLIDATE,    /**< Emergency consolidation */
    SECURITY_CL_FEP_RESPONSE_ROLLBACK        /**< Rollback to checkpoint */
} security_cl_fep_response_t;

/* ============================================================================
 * Configuration Structures
 * ============================================================================ */

/**
 * @brief FEP bridge configuration
 */
typedef struct {
    /** FEP parameters */
    float free_energy_threshold;             /**< FE threshold for attack detection */
    float surprise_threshold;                /**< Surprise threshold */
    float precision_learning_rate;           /**< Precision adaptation rate */

    /** Retention-to-FE mapping */
    float retention_loss_weight;             /**< Weight for retention loss -> FE */
    float drift_weight;                      /**< Weight for drift -> prediction error */
    float replay_anomaly_weight;             /**< Weight for replay anomaly -> surprise */

    /** Detection parameters */
    bool enable_fep_detection;               /**< Use FEP for attack detection */
    bool enable_precision_modulation;        /**< Adapt precision based on detections */
    bool enable_active_inference;            /**< Use active inference for responses */

    /** Learning */
    bool enable_online_learning;             /**< Update FEP from detections */
    float belief_learning_rate;              /**< Belief update rate */
    bool learn_from_false_positives;         /**< Update on FP feedback */

    /** Bio-async */
    bool enable_bio_async;                   /**< Enable bio-async messaging */
    uint32_t bio_inbox_capacity;             /**< Message queue size */

    /** Logging */
    bool enable_logging;                     /**< Enable detailed logging */
} security_cl_fep_config_t;

/* ============================================================================
 * Effect Structures
 * ============================================================================ */

/**
 * @brief FEP effects on Security CL (FEP -> Security)
 *
 * WHAT: FEP-derived protection signals
 * WHY:  Drive security responses from free energy analysis
 * HOW:  Updated each FEP cycle, consumed by security CL
 */
typedef struct {
    /** Free energy metrics */
    float current_free_energy;               /**< Current total free energy */
    float surprise_level;                    /**< Current surprise estimate */
    float prediction_error;                  /**< Current prediction error magnitude */

    /** Detection outputs */
    float forgetting_severity_score;         /**< FEP-based severity [0-1] */
    float attack_likelihood;                 /**< Probability of attack [0-1] */
    security_cl_fep_severity_t severity;     /**< Classified severity level */

    /** Precision effects */
    float detection_precision;               /**< Current detection precision */
    float sensitivity_multiplier;            /**< Threshold sensitivity multiplier */

    /** Active inference response */
    security_cl_fep_response_t response;     /**< Recommended protective action */
    float response_urgency;                  /**< How urgent is response [0-1] */
    float ewc_boost_factor;                  /**< Recommended EWC boost */
    float lr_reduction_factor;               /**< Recommended LR reduction */

    /** Validity */
    uint64_t last_update_ms;                 /**< When effects were updated */
    bool valid;                              /**< Whether effects are current */
} security_cl_fep_effects_t;

/**
 * @brief Security CL effects on FEP (Security -> FEP)
 *
 * WHAT: Security-derived observations for FEP
 * WHY:  Update FEP model from security events
 * HOW:  Security events become high-surprise observations
 */
typedef struct {
    /** Retention signals */
    float current_retention;                 /**< Current retention [0-1] */
    float retention_delta;                   /**< Change since last update */
    bool retention_anomaly;                  /**< Anomalous retention pattern */

    /** Drift signals */
    float drift_magnitude;                   /**< Current drift score */
    security_cl_drift_type_t drift_type;     /**< Classified drift type */
    bool adversarial_drift;                  /**< Is drift adversarial? */

    /** Replay signals */
    uint32_t replay_integrity_failures;      /**< Recent integrity failures */
    bool replay_poisoned;                    /**< Replay buffer compromised? */

    /** Attack signals */
    uint64_t attacks_detected;               /**< Total attacks detected */
    security_cl_forgetting_type_t attack_type; /**< Last attack type */
    float attack_severity;                   /**< Attack severity [0-1] */

    /** Learning signals */
    float current_lr;                        /**< Current learning rate */
    bool lr_manipulation;                    /**< LR manipulation detected? */

    /** Validity */
    uint64_t timestamp_ms;                   /**< When captured */
    bool valid;                              /**< Whether valid */
} fep_security_cl_effects_t;

/* ============================================================================
 * State and Statistics Structures
 * ============================================================================ */

/**
 * @brief FEP bridge internal state
 */
typedef struct {
    bool active;                             /**< Bridge is active */
    uint64_t update_count;                   /**< Number of updates */
    uint64_t detection_count;                /**< Detections processed */

    /** Precision state */
    float current_precision;                 /**< Current precision level */
    float precision_ema;                     /**< Exponential moving average */

    /** Free energy state */
    float avg_free_energy;                   /**< Running average FE */
    float max_free_energy;                   /**< Maximum FE observed */
    float avg_surprise;                      /**< Running average surprise */

    /** Belief state */
    float* retention_beliefs;                /**< Expected retention per task */
    uint32_t num_task_beliefs;               /**< Number of task beliefs */

    /** Response state */
    security_cl_fep_response_t last_response; /**< Last issued response */
    uint64_t last_response_time_ms;          /**< When response was issued */
} security_cl_fep_state_t;

/**
 * @brief FEP bridge statistics
 */
typedef struct {
    /** Update counts */
    uint64_t total_updates;                  /**< Total FEP updates */
    uint64_t fep_detections;                 /**< Detections using FEP */
    uint64_t attacks_detected;               /**< Attacks found via FEP */
    uint64_t false_positives;                /**< Known false positives */

    /** Free energy metrics */
    float avg_free_energy;                   /**< Average free energy */
    float max_free_energy;                   /**< Maximum free energy */
    float avg_surprise;                      /**< Average surprise */

    /** Precision metrics */
    float current_precision;                 /**< Current precision */
    uint64_t precision_adaptations;          /**< Precision updates */

    /** Response metrics */
    uint64_t responses_issued;               /**< Total responses issued */
    uint64_t ewc_boosts;                     /**< EWC boost responses */
    uint64_t lr_reductions;                  /**< LR reduction responses */
    uint64_t replay_locks;                   /**< Replay lock responses */
    uint64_t emergency_consolidations;       /**< Emergency consolidations */

    /** Timing */
    float avg_update_time_us;                /**< Average update time */
    uint64_t uptime_ms;                      /**< Bridge uptime */
} security_cl_fep_stats_t;

/* ============================================================================
 * Main Bridge Structure
 * ============================================================================ */

/**
 * @brief Security Continual Learning FEP Bridge
 *
 * WHAT: Integrates FEP with security continual learning
 * WHY:  Enable predictive protection against forgetting attacks
 * HOW:  Map retention/drift to FE, use active inference for responses
 */
typedef struct {
    bridge_base_t base;                      /**< MUST be first: base bridge */

    /** Configuration */
    security_cl_fep_config_t config;

    /** Connected systems */
    fep_system_t* fep_system;                /**< FEP system */
    security_cl_bridge_t* security_cl;       /**< Security CL bridge */

    /** Bidirectional effects */
    security_cl_fep_effects_t fep_effects;   /**< FEP -> Security effects */
    fep_security_cl_effects_t cl_effects;    /**< Security -> FEP effects */

    /** Internal state */
    security_cl_fep_state_t state;

    /** Statistics */
    security_cl_fep_stats_t stats;
} security_cl_fep_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default FEP bridge configuration
 *
 * WHAT: Provide sensible defaults for FEP-security CL integration
 * WHY:  Easy initialization with biologically-plausible parameters
 * HOW:  Return struct with balanced sensitivity/specificity
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int security_cl_fep_default_config(security_cl_fep_config_t* config);

/**
 * @brief Create security CL FEP bridge
 *
 * WHAT: Initialize FEP integration for continual learning security
 * WHY:  Enable surprise-based forgetting attack detection
 * HOW:  Connect FEP system to security CL, allocate state
 *
 * @param config Configuration (NULL for defaults)
 * @param fep_system FEP system handle
 * @param security_cl Security CL bridge handle
 * @return Bridge handle or NULL on failure
 */
security_cl_fep_bridge_t* security_cl_fep_create(
    const security_cl_fep_config_t* config,
    fep_system_t* fep_system,
    security_cl_bridge_t* security_cl
);

/**
 * @brief Destroy security CL FEP bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Prevent memory leaks
 * HOW:  Free allocations, disconnect bio-async
 *
 * @param bridge Bridge handle (NULL safe)
 */
void security_cl_fep_destroy(security_cl_fep_bridge_t* bridge);

/**
 * @brief Reset bridge state
 *
 * WHAT: Reset bridge to initial state
 * WHY:  Allow reuse without recreation
 * HOW:  Clear effects, statistics; keep configuration
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int security_cl_fep_reset(security_cl_fep_bridge_t* bridge);

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
int security_cl_fep_get_config(
    const security_cl_fep_bridge_t* bridge,
    security_cl_fep_config_t* config
);

/**
 * @brief Set configuration
 *
 * WHAT: Update bridge configuration
 * WHY:  Runtime tuning of FEP parameters
 * HOW:  Validate and apply new configuration
 *
 * @param bridge Bridge handle
 * @param config New configuration
 * @return 0 on success, -1 on error
 */
int security_cl_fep_set_config(
    security_cl_fep_bridge_t* bridge,
    const security_cl_fep_config_t* config
);

/* ============================================================================
 * Core Update API
 * ============================================================================ */

/**
 * @brief Compute FEP effects from security state
 *
 * WHAT: Main FEP update - convert security signals to free energy
 * WHY:  Detect forgetting attacks as high-surprise events
 * HOW:  Map retention loss, drift, replay anomaly to FE components
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int security_cl_fep_compute_effects(security_cl_fep_bridge_t* bridge);

/**
 * @brief Update FEP model from detection result
 *
 * WHAT: Feed detection back to FEP for learning
 * WHY:  Improve predictive model from attack observations
 * HOW:  Convert detection to high/low surprise observation
 *
 * @param bridge Bridge handle
 * @param is_attack Whether attack was detected
 * @param attack_type Type of attack (if detected)
 * @param severity Attack severity [0-1]
 * @return 0 on success, -1 on error
 */
int security_cl_fep_update_from_detection(
    security_cl_fep_bridge_t* bridge,
    bool is_attack,
    security_cl_forgetting_type_t attack_type,
    float severity
);

/**
 * @brief Apply precision modulation
 *
 * WHAT: Adapt detection precision based on attack history
 * WHY:  Balance sensitivity vs false positive rate
 * HOW:  Increase precision after attacks, decrease after FPs
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int security_cl_fep_apply_precision_modulation(security_cl_fep_bridge_t* bridge);

/**
 * @brief Select protective response via active inference
 *
 * WHAT: Choose optimal protective action
 * WHY:  Minimize expected free energy of response
 * HOW:  Evaluate EFE for each possible response, select minimal
 *
 * @param bridge Bridge handle
 * @param response Output: selected response
 * @param urgency Output: response urgency [0-1]
 * @return 0 on success, -1 on error
 */
int security_cl_fep_select_response(
    security_cl_fep_bridge_t* bridge,
    security_cl_fep_response_t* response,
    float* urgency
);

/* ============================================================================
 * Retention-FEP Mapping API
 * ============================================================================ */

/**
 * @brief Convert retention loss to free energy
 *
 * WHAT: Map retention degradation to free energy
 * WHY:  Retention loss = deviation from expected state = free energy
 * HOW:  FE = scale * (1 - retention/expected) * precision
 *
 * @param bridge Bridge handle
 * @param task_id Task to evaluate
 * @param current_retention Current retention [0-1]
 * @param expected_retention Expected retention [0-1]
 * @return Free energy contribution
 */
float security_cl_fep_retention_to_fe(
    const security_cl_fep_bridge_t* bridge,
    uint32_t task_id,
    float current_retention,
    float expected_retention
);

/**
 * @brief Convert drift magnitude to prediction error
 *
 * WHAT: Map concept drift to prediction error
 * WHY:  Drift = deviation between predicted and observed distribution
 * HOW:  Error = drift_magnitude * weight * precision
 *
 * @param bridge Bridge handle
 * @param drift_score Drift magnitude
 * @param drift_type Type of drift
 * @return Prediction error contribution
 */
float security_cl_fep_drift_to_error(
    const security_cl_fep_bridge_t* bridge,
    float drift_score,
    security_cl_drift_type_t drift_type
);

/**
 * @brief Convert replay anomaly to surprise
 *
 * WHAT: Map replay buffer anomaly to surprise
 * WHY:  Replay tampering is unexpected (high surprise)
 * HOW:  Surprise = -ln P(anomaly) approximated by severity
 *
 * @param bridge Bridge handle
 * @param integrity_failures Number of recent failures
 * @param is_poisoned Buffer confirmed poisoned
 * @return Surprise level contribution
 */
float security_cl_fep_replay_to_surprise(
    const security_cl_fep_bridge_t* bridge,
    uint32_t integrity_failures,
    bool is_poisoned
);

/* ============================================================================
 * Detection API
 * ============================================================================ */

/**
 * @brief Detect forgetting attack using FEP
 *
 * WHAT: Check if current state indicates forgetting attack
 * WHY:  High free energy = high surprise = potential attack
 * HOW:  Aggregate FE components, compare against threshold
 *
 * @param bridge Bridge handle
 * @param severity Output: attack severity level
 * @param confidence Output: detection confidence [0-1]
 * @return true if attack detected
 */
bool security_cl_fep_detect_attack(
    security_cl_fep_bridge_t* bridge,
    security_cl_fep_severity_t* severity,
    float* confidence
);

/**
 * @brief Get attack likelihood score
 *
 * WHAT: Probabilistic attack assessment
 * WHY:  Soft detection better than binary
 * HOW:  Sigmoid of normalized free energy
 *
 * @param bridge Bridge handle
 * @return Attack likelihood [0-1]
 */
float security_cl_fep_get_attack_likelihood(
    const security_cl_fep_bridge_t* bridge
);

/**
 * @brief Report false positive to FEP
 *
 * WHAT: Update FEP on known false positive
 * WHY:  Reduce precision to prevent similar FPs
 * HOW:  Lower precision for this observation type
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int security_cl_fep_report_false_positive(security_cl_fep_bridge_t* bridge);

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
int security_cl_fep_get_effects(
    const security_cl_fep_bridge_t* bridge,
    security_cl_fep_effects_t* effects
);

/**
 * @brief Get security effects on FEP
 *
 * @param bridge Bridge handle
 * @param effects Output effects
 * @return 0 on success, -1 on error
 */
int security_cl_fep_get_cl_effects(
    const security_cl_fep_bridge_t* bridge,
    fep_security_cl_effects_t* effects
);

/**
 * @brief Get current free energy
 *
 * @param bridge Bridge handle
 * @return Current free energy, or -1 on error
 */
float security_cl_fep_get_free_energy(const security_cl_fep_bridge_t* bridge);

/**
 * @brief Get current surprise level
 *
 * @param bridge Bridge handle
 * @return Current surprise, or -1 on error
 */
float security_cl_fep_get_surprise(const security_cl_fep_bridge_t* bridge);

/**
 * @brief Get current precision
 *
 * @param bridge Bridge handle
 * @return Current precision, or -1 on error
 */
float security_cl_fep_get_precision(const security_cl_fep_bridge_t* bridge);

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge handle
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int security_cl_fep_get_stats(
    const security_cl_fep_bridge_t* bridge,
    security_cl_fep_stats_t* stats
);

/**
 * @brief Print summary to stdout
 *
 * WHAT: Display current bridge state
 * WHY:  Debugging and monitoring
 * HOW:  Print FE, effects, statistics
 *
 * @param bridge Bridge handle
 */
void security_cl_fep_print_summary(const security_cl_fep_bridge_t* bridge);

/* ============================================================================
 * Bio-Async API
 * ============================================================================ */

/**
 * @brief Connect to bio-async router
 *
 * WHAT: Register bridge with bio-async system
 * WHY:  Enable inter-module attack notifications
 * HOW:  Register module, setup inbox
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int security_cl_fep_connect_bio_async(security_cl_fep_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * @param bridge Bridge handle
 * @return 0 on success
 */
int security_cl_fep_disconnect_bio_async(security_cl_fep_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Bridge handle
 * @return true if connected
 */
bool security_cl_fep_is_bio_async_connected(const security_cl_fep_bridge_t* bridge);

/* ============================================================================
 * String Conversion Utilities
 * ============================================================================ */

/**
 * @brief Convert severity level to string
 *
 * @param severity Severity level
 * @return Human-readable string
 */
const char* security_cl_fep_severity_to_string(security_cl_fep_severity_t severity);

/**
 * @brief Convert response type to string
 *
 * @param response Response type
 * @return Human-readable string
 */
const char* security_cl_fep_response_to_string(security_cl_fep_response_t response);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SECURITY_CONTINUAL_LEARNING_FEP_BRIDGE_H */
