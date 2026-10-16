/**
 * @file nimcp_security_immune_fep_bridge.h
 * @brief Free Energy Principle bridge for Security-Immune System Integration
 * @version 1.0.0
 * @date 2025-01-10
 *
 * WHAT: FEP integration for brain immune system security operations
 * WHY:  Immune system anomalies represent high-surprise events in the predictive
 *       processing framework - the system expects healthy immune function, and
 *       deviations generate free energy that must be minimized through protective
 *       responses and adaptive immune tuning
 * HOW:  Map immune evasion attempts, autoimmune patterns, cytokine storms, and
 *       memory corruption to free energy metrics; use active inference for
 *       protective responses; precision modulation for sensitivity tuning
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * IMMUNE SYSTEM AS PREDICTIVE PROCESSING:
 * ---------------------------------------
 * The biological immune system can be viewed through an FEP lens:
 * - Expected state: Healthy tissue, normal cytokine levels, functioning memory
 * - Observations: Antigen presentations, cytokine signals, immune cell behavior
 * - Prediction errors: Unexpected pathogens, abnormal immune activity, evasion
 * - Free energy: Quantifies how "surprising" the immune state is
 *
 * THREAT TYPES MAPPED TO FEP:
 * ---------------------------
 *
 * 1. IMMUNE EVASION ATTEMPTS:
 *    - Biological: Pathogens that mask surface antigens, mimic self
 *    - Digital: Threats that avoid antigen presentation, disguise signatures
 *    - FEP: High surprise when expected detection fails to occur
 *    - Response: Increase precision, broaden pattern recognition
 *
 * 2. AUTOIMMUNE-LIKE ATTACKS:
 *    - Biological: T cells attacking self tissue, loss of tolerance
 *    - Digital: Security measures attacking legitimate components
 *    - FEP: Prediction error from "friendly fire" patterns
 *    - Response: Activate regulatory mechanisms, reduce aggression
 *
 * 3. CYTOKINE STORM INDICATORS:
 *    - Biological: Runaway inflammatory cascade, organ damage
 *    - Digital: Cascading security alerts, resource exhaustion
 *    - FEP: Extreme deviation from homeostatic cytokine levels
 *    - Response: Emergency suppression, anti-inflammatory cascade
 *
 * 4. IMMUNE MEMORY CORRUPTION:
 *    - Biological: Loss of learned immunity, memory cell dysfunction
 *    - Digital: Pattern database corruption, false memory formation
 *    - FEP: Prediction failures from trusted memory sources
 *    - Response: Memory validation, reconsolidation triggers
 *
 * FEP INTEGRATION MODEL:
 * ----------------------
 * ```
 * Immune Observation (o) --> Anomaly Extraction
 *         |
 * Expected Immune State mu (generative model of healthy immune function)
 *         |
 * Prediction Error: epsilon = o - g(mu)
 *   - Evasion detection gaps
 *   - Autoimmune signatures
 *   - Cytokine level deviations
 *   - Memory inconsistencies
 *         |
 * Free Energy F = Complexity + Inaccuracy
 *         |
 * Surprise = -ln p(o) <= F
 *         |
 * Immune Threat Score = F / F_threshold
 * ```
 *
 * THREAT MAPPING TO FEP:
 * ----------------------
 * - Low FE (<2.0)  --> Normal immune function
 * - Medium FE (2-5) --> Suspicious patterns (enhance monitoring)
 * - High FE (5-10)  --> Active threat (protective response)
 * - Very high FE (>10) --> Critical immune compromise (emergency)
 *
 * PRECISION MODULATION:
 * ---------------------
 * - High precision: Sensitive detection (may trigger autoimmune-like FP)
 * - Low precision: Tolerant detection (may miss evasion)
 * - Adaptive precision based on threat landscape and inflammation state
 *
 * ACTIVE INFERENCE RESPONSES:
 * ---------------------------
 * - Trigger inflammation cascade (mobilize immune resources)
 * - Activate regulatory T cells (suppress over-reaction)
 * - Enhance B cell memory formation (learn from threats)
 * - Initiate immune memory reconsolidation (repair corruption)
 *
 * ARCHITECTURE:
 * ```
 * +============================================================================+
 * |          SECURITY-IMMUNE FEP BRIDGE                                        |
 * +============================================================================+
 * |                                                                            |
 * |   +------------------+         +------------------+                        |
 * |   |  FEP System      |-------->|  Security        |                        |
 * |   |                  |         |  Immune          |                        |
 * |   | - Free Energy    |         |  Unified Bridge  |                        |
 * |   | - Surprise       |         |                  |                        |
 * |   | - Precision      |         | - Antigens       |                        |
 * |   | - Active Inf     |         | - B/T Cells      |                        |
 * |   +------------------+         | - Cytokines      |                        |
 * |           |                    +------------------+                        |
 * |           v                                                                |
 * |   +--------------------------------------------------------------------+  |
 * |   |              BIDIRECTIONAL EFFECTS                                 |  |
 * |   |                                                                    |  |
 * |   |  FEP --> Immune:                                                   |  |
 * |   |    - Free energy --> Threat severity assessment                    |  |
 * |   |    - Surprise --> Anomaly probability                              |  |
 * |   |    - Precision --> Detection sensitivity                           |  |
 * |   |    - Active inference --> Protective response selection            |  |
 * |   |                                                                    |  |
 * |   |  Immune --> FEP:                                                   |  |
 * |   |    - Antigen presentations --> High-surprise observations          |  |
 * |   |    - Cytokine levels --> Precision modulation                      |  |
 * |   |    - Inflammation state --> Belief update urgency                  |  |
 * |   |    - Memory failures --> Model update triggers                     |  |
 * |   +--------------------------------------------------------------------+  |
 * |                                                                            |
 * +============================================================================+
 * ```
 *
 * NIMCP STANDARDS:
 * - All functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT-WHY-HOW documentation
 * - Thread-safe via mutex
 * - nimcp_malloc/nimcp_free memory management
 * - bridge_base_t as first member
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_SECURITY_IMMUNE_FEP_BRIDGE_H
#define NIMCP_SECURITY_IMMUNE_FEP_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "utils/bridge/nimcp_bridge_base.h"
#include "security/immune/nimcp_security_immune_unified_bridge.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"
#include "utils/validation/nimcp_common.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/platform/nimcp_platform_time.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Bio-async module identifier for this bridge */
#define BIO_MODULE_SECURITY_IMMUNE_FEP  0x0641

/** Free energy thresholds for immune security */
#define IMMUNE_FEP_NORMAL_THRESHOLD       2.0f   /**< Normal immune function */
#define IMMUNE_FEP_SUSPICIOUS_THRESHOLD   5.0f   /**< Suspicious patterns */
#define IMMUNE_FEP_THREAT_THRESHOLD       10.0f  /**< Active threat indicators */
#define IMMUNE_FEP_CRITICAL_THRESHOLD     20.0f  /**< Critical immune compromise */

/** Precision bounds for detection sensitivity */
#define IMMUNE_FEP_MIN_PRECISION          0.1f   /**< Minimum detection precision */
#define IMMUNE_FEP_MAX_PRECISION          10.0f  /**< Maximum detection precision */
#define IMMUNE_FEP_DEFAULT_PRECISION      1.0f   /**< Default precision level */

/** Component weights for free energy computation */
#define IMMUNE_FEP_EVASION_WEIGHT         3.0f   /**< Weight for evasion attempts */
#define IMMUNE_FEP_AUTOIMMUNE_WEIGHT      4.0f   /**< Weight for autoimmune patterns */
#define IMMUNE_FEP_STORM_WEIGHT           5.0f   /**< Weight for cytokine storms */
#define IMMUNE_FEP_MEMORY_WEIGHT          3.5f   /**< Weight for memory corruption */

/** Active inference thresholds */
#define IMMUNE_FEP_RESPONSE_THRESHOLD     0.5f   /**< FE ratio triggering response */
#define IMMUNE_FEP_EMERGENCY_THRESHOLD    0.8f   /**< FE ratio for emergency */
#define IMMUNE_FEP_RECOVERY_THRESHOLD     0.2f   /**< FE ratio for recovery */

/** Cytokine influence on precision */
#define IMMUNE_FEP_IL1_PRECISION_BOOST    0.15f  /**< IL-1 increases precision */
#define IMMUNE_FEP_IL6_PRECISION_BOOST    0.20f  /**< IL-6 increases precision */
#define IMMUNE_FEP_TNF_PRECISION_BOOST    0.30f  /**< TNF-a max precision boost */
#define IMMUNE_FEP_IL10_PRECISION_REDUCE  0.15f  /**< IL-10 reduces precision */

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief FEP-based immune threat classification
 *
 * WHAT: Categorizes immune anomalies based on free energy levels
 * WHY:  Enables graduated response to different threat severities
 * HOW:  Map free energy thresholds to categorical threat levels
 */
typedef enum {
    IMMUNE_FEP_THREAT_NONE = 0,       /**< No threat (low FE) */
    IMMUNE_FEP_THREAT_MONITOR,        /**< Requires monitoring (medium FE) */
    IMMUNE_FEP_THREAT_EVASION,        /**< Evasion attempt detected */
    IMMUNE_FEP_THREAT_AUTOIMMUNE,     /**< Autoimmune-like pattern */
    IMMUNE_FEP_THREAT_STORM,          /**< Cytokine storm indicators */
    IMMUNE_FEP_THREAT_MEMORY,         /**< Memory corruption detected */
    IMMUNE_FEP_THREAT_CRITICAL        /**< Critical immune compromise */
} immune_fep_threat_t;

/**
 * @brief Active inference response type for immune protection
 *
 * WHAT: Actions that can be taken to minimize expected free energy
 * WHY:  Active inference guides protective responses
 * HOW:  Each response type targets different aspects of immune function
 */
typedef enum {
    IMMUNE_FEP_RESPONSE_NONE = 0,     /**< No action needed */
    IMMUNE_FEP_RESPONSE_OBSERVE,      /**< Increase observation precision */
    IMMUNE_FEP_RESPONSE_ALERT,        /**< Issue immune system alert */
    IMMUNE_FEP_RESPONSE_INFLAME,      /**< Trigger inflammation cascade */
    IMMUNE_FEP_RESPONSE_REGULATE,     /**< Activate regulatory mechanisms */
    IMMUNE_FEP_RESPONSE_REPAIR,       /**< Initiate memory repair */
    IMMUNE_FEP_RESPONSE_SUPPRESS,     /**< Emergency suppression (storm) */
    IMMUNE_FEP_RESPONSE_EMERGENCY     /**< Full emergency response */
} immune_fep_response_t;

/* ============================================================================
 * Configuration Structure
 * ============================================================================ */

/**
 * @brief FEP bridge configuration for immune security
 *
 * WHAT: Configuration parameters for FEP-immune integration
 * WHY:  Allow tuning of detection sensitivity and response behavior
 * HOW:  Set thresholds, weights, and feature enables
 */
typedef struct {
    /* FEP thresholds */
    float evasion_fe_threshold;       /**< FE threshold for evasion detection */
    float autoimmune_fe_threshold;    /**< FE threshold for autoimmune detection */
    float storm_fe_threshold;         /**< FE threshold for storm detection */
    float memory_fe_threshold;        /**< FE threshold for memory corruption */
    float surprise_threshold;         /**< General surprise threshold */

    /* Detection weights */
    bool use_fep_scoring;             /**< Use FEP for threat scoring */
    bool enable_precision_modulation; /**< Adapt precision dynamically */
    float evasion_weight;             /**< Weight for evasion in FE */
    float autoimmune_weight;          /**< Weight for autoimmune in FE */
    float storm_weight;               /**< Weight for storm in FE */
    float memory_weight;              /**< Weight for memory in FE */

    /* Active inference */
    bool enable_active_inference;     /**< Enable active inference responses */
    float response_threshold;         /**< FE ratio for response trigger */
    float emergency_threshold;        /**< FE ratio for emergency */
    float recovery_threshold;         /**< FE ratio for recovery */
    float inference_learning_rate;    /**< Learning rate for inference */

    /* Precision learning */
    float precision_learning_rate;    /**< Rate for precision adaptation */
    bool learn_from_cytokines;        /**< Modulate precision from cytokines */
    bool learn_from_inflammation;     /**< Modulate precision from inflammation */

    /* Cytokine influence */
    float il1_precision_boost;        /**< IL-1 effect on precision */
    float il6_precision_boost;        /**< IL-6 effect on precision */
    float tnf_precision_boost;        /**< TNF-a effect on precision */
    float il10_precision_reduce;      /**< IL-10 effect on precision */

    /* Sensitivity factors */
    float fep_sensitivity;            /**< FEP effect scaling [0.5-2.0] */
    float immune_sensitivity;         /**< Immune effect scaling [0.5-2.0] */
} security_immune_fep_config_t;

/* ============================================================================
 * Effect Structures
 * ============================================================================ */

/**
 * @brief FEP effects on immune security (FEP --> Immune)
 *
 * WHAT: Effects computed from FEP state that influence immune decisions
 * WHY:  Free energy quantifies how "surprising" immune state is
 * HOW:  Map FE components to immune security metrics
 */
typedef struct {
    /* Free energy derived metrics */
    float total_free_energy;          /**< Current total FE */
    float evasion_surprise;           /**< Surprise from evasion attempts */
    float autoimmune_surprise;        /**< Surprise from autoimmune patterns */
    float storm_surprise;             /**< Surprise from cytokine storm */
    float memory_surprise;            /**< Surprise from memory corruption */

    /* Derived security scores */
    float threat_severity;            /**< Normalized threat severity [0-1] */
    float evasion_probability;        /**< Probability of evasion attempt */
    float autoimmune_probability;     /**< Probability of autoimmune attack */
    float storm_probability;          /**< Probability of cytokine storm */
    float memory_corruption_prob;     /**< Probability of memory corruption */

    /* Precision effects */
    float detection_sensitivity;      /**< Precision-derived sensitivity */
    float current_precision;          /**< Current precision level */
    float cytokine_precision_mod;     /**< Cytokine-induced precision change */

    /* Threat classification */
    immune_fep_threat_t threat_level;          /**< Classified threat level */
    immune_fep_response_t recommended_action;  /**< Active inference recommendation */
} fep_to_immune_effects_t;

/**
 * @brief Immune effects on FEP (Immune --> FEP)
 *
 * WHAT: Immune observations that update FEP generative model
 * WHY:  Immune anomalies are high-surprise observations
 * HOW:  Feed immune metrics back to update beliefs
 */
typedef struct {
    /* Detection events */
    uint64_t evasion_detections;      /**< Total evasion detections */
    uint64_t autoimmune_detections;   /**< Total autoimmune detections */
    uint64_t storm_indicators;        /**< Total storm indicators */
    uint64_t memory_corruptions;      /**< Total memory corruptions */
    uint64_t false_positives;         /**< Known false positives */

    /* Cytokine state */
    float cytokine_il1;               /**< Current IL-1 level [0-1] */
    float cytokine_il6;               /**< Current IL-6 level [0-1] */
    float cytokine_tnf;               /**< Current TNF-a level [0-1] */
    float cytokine_il10;              /**< Current IL-10 level [0-1] */
    float cytokine_ifn;               /**< Current IFN-g level [0-1] */

    /* Inflammation state */
    brain_inflammation_level_t inflammation_level; /**< Current inflammation */
    float inflammation_duration_sec;  /**< How long inflamed */
    bool cytokine_storm_active;       /**< Storm condition active */

    /* Immune cell metrics */
    uint32_t active_b_cells;          /**< Active B cell count */
    uint32_t active_t_cells;          /**< Active T cell count */
    uint32_t memory_cells;            /**< Memory cell count */
    float avg_antibody_effectiveness; /**< Average antibody effectiveness */

    /* Response state */
    uint32_t active_responses;        /**< Currently active responses */
    uint32_t pending_antigens;        /**< Antigens awaiting processing */
} immune_to_fep_effects_t;

/* ============================================================================
 * State and Statistics Structures
 * ============================================================================ */

/**
 * @brief Current FEP bridge state
 */
typedef struct {
    bool active;                      /**< Whether bridge is active */
    uint64_t update_count;            /**< Number of updates */
    uint64_t inference_count;         /**< Active inference actions */
    uint64_t last_update_time;        /**< Last update timestamp (ms) */

    /* Running FEP state */
    float current_free_energy;        /**< Current total FE */
    float avg_free_energy;            /**< Running average FE */
    float max_free_energy;            /**< Maximum FE observed */
    float current_surprise;           /**< Current surprise level */
    float avg_surprise;               /**< Running average surprise */

    /* Precision state */
    float evasion_precision;          /**< Precision for evasion detection */
    float autoimmune_precision;       /**< Precision for autoimmune detection */
    float storm_precision;            /**< Precision for storm detection */
    float memory_precision;           /**< Precision for memory corruption */

    /* Active inference state */
    immune_fep_response_t last_response;  /**< Last active inference response */
    uint64_t last_response_time;      /**< Timestamp of last response */
    bool emergency_mode_active;       /**< Emergency response active */
} security_immune_fep_state_t;

/**
 * @brief FEP bridge statistics
 */
typedef struct {
    /* Update statistics */
    uint64_t total_updates;           /**< Total bridge updates */
    uint64_t fep_based_decisions;     /**< Decisions guided by FEP */

    /* Detection statistics */
    uint64_t threats_detected;        /**< Total threats detected */
    uint64_t evasion_detections;      /**< Evasion detections */
    uint64_t autoimmune_detections;   /**< Autoimmune detections */
    uint64_t storm_detections;        /**< Storm detections */
    uint64_t memory_corruptions;      /**< Memory corruption detections */
    uint64_t false_positives;         /**< Known false positives */

    /* Active inference statistics */
    uint64_t inflammation_triggers;   /**< Times inflammation was triggered */
    uint64_t regulatory_activations;  /**< Regulatory activations */
    uint64_t memory_repairs;          /**< Memory repair actions */
    uint64_t emergency_responses;     /**< Emergency responses triggered */
    uint64_t precision_adaptations;   /**< Precision adaptations */

    /* Free energy statistics */
    float avg_free_energy;            /**< Average free energy */
    float max_free_energy;            /**< Maximum free energy */
    float avg_surprise;               /**< Average surprise */
    float current_precision;          /**< Current overall precision */

    /* Performance */
    float avg_update_time_us;         /**< Average update time (microseconds) */
    float avg_inference_time_us;      /**< Average inference time */
} security_immune_fep_stats_t;

/* ============================================================================
 * Bridge Structure
 * ============================================================================ */

/**
 * @brief Security-Immune FEP Bridge
 *
 * WHAT: Bridges security-immune integration with FEP system
 * WHY:  Enables predictive processing for immune threat detection
 * HOW:  Maps immune observations to free energy, uses active inference
 */
typedef struct security_immune_fep_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge */

    /* Configuration */
    security_immune_fep_config_t config;

    /* Connected systems */
    sec_immune_unified_bridge_t* unified_bridge; /**< Unified security-immune bridge */
    brain_immune_system_t* immune_system;        /**< Brain immune system */
    fep_system_t* fep_system;                    /**< FEP system */

    /* Bidirectional effects */
    fep_to_immune_effects_t fep_effects;         /**< FEP --> Immune effects */
    immune_to_fep_effects_t immune_effects;      /**< Immune --> FEP effects */

    /* State */
    security_immune_fep_state_t state;

    /* Statistics */
    security_immune_fep_stats_t stats;
} security_immune_fep_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default FEP bridge configuration
 *
 * WHAT: Provides sensible defaults for immune security FEP integration
 * WHY:  Simplifies initialization with biologically-plausible values
 * HOW:  Sets thresholds based on expected immune behavior variance
 *
 * @param config Output configuration structure
 * @return 0 on success, -1 on error
 */
int security_immune_fep_default_config(security_immune_fep_config_t* config);

/**
 * @brief Create security-immune FEP bridge
 *
 * WHAT: Initialize FEP integration for immune security
 * WHY:  Enable surprise-based threat detection in immune systems
 * HOW:  Connect FEP system to immune bridge, initialize state
 *
 * @param config Configuration (NULL for defaults)
 * @param unified_bridge Security-immune unified bridge handle
 * @param immune_system Brain immune system handle
 * @param fep_system FEP system handle
 * @return Bridge handle or NULL on failure
 */
security_immune_fep_bridge_t* security_immune_fep_create(
    const security_immune_fep_config_t* config,
    sec_immune_unified_bridge_t* unified_bridge,
    brain_immune_system_t* immune_system,
    fep_system_t* fep_system
);

/**
 * @brief Destroy security-immune FEP bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Prevent memory leaks and dangling connections
 * HOW:  Disconnect bio-async, cleanup base, free bridge
 *
 * @param bridge Bridge handle (NULL safe)
 */
void security_immune_fep_destroy(security_immune_fep_bridge_t* bridge);

/**
 * @brief Reset bridge to initial state
 *
 * WHAT: Reset state and statistics while preserving connections
 * WHY:  Allow bridge reuse without reconnection overhead
 * HOW:  Zero state/stats, reset precision to defaults
 *
 * @param bridge Bridge handle
 * @return 0 on success, error code on failure
 */
int security_immune_fep_reset(security_immune_fep_bridge_t* bridge);

/* ============================================================================
 * Configuration API
 * ============================================================================ */

/**
 * @brief Get current configuration
 *
 * @param bridge Bridge handle
 * @param config Output configuration structure
 * @return 0 on success, error code on failure
 */
int security_immune_fep_get_config(
    const security_immune_fep_bridge_t* bridge,
    security_immune_fep_config_t* config
);

/**
 * @brief Set bridge configuration
 *
 * WHAT: Update bridge configuration at runtime
 * WHY:  Allow dynamic tuning of detection parameters
 * HOW:  Validate and apply new configuration
 *
 * @param bridge Bridge handle
 * @param config New configuration
 * @return 0 on success, error code on failure
 */
int security_immune_fep_set_config(
    security_immune_fep_bridge_t* bridge,
    const security_immune_fep_config_t* config
);

/* ============================================================================
 * Core Update API
 * ============================================================================ */

/**
 * @brief Compute FEP effects on immune security (main update)
 *
 * WHAT: Calculate free energy from current immune state
 * WHY:  Core FEP computation - quantify surprise from immune behavior
 * HOW:  Process immune observations through generative model
 *
 * @param bridge Bridge handle
 * @return 0 on success, error code on failure
 */
int security_immune_fep_compute_effects(security_immune_fep_bridge_t* bridge);

/**
 * @brief Update FEP from immune detection
 *
 * WHAT: Feed immune detection result to FEP system
 * WHY:  Detections are high-surprise observations that update beliefs
 * HOW:  Convert detection to observation, process through FEP
 *
 * @param bridge Bridge handle
 * @param detection_type Type of detection
 * @param severity Detection severity [0-1]
 * @param antigen_id Associated antigen ID (if applicable)
 * @return 0 on success, error code on failure
 */
int security_immune_fep_update_from_detection(
    security_immune_fep_bridge_t* bridge,
    immune_fep_threat_t detection_type,
    float severity,
    uint32_t antigen_id
);

/**
 * @brief Apply active inference for protective response
 *
 * WHAT: Use active inference to determine protective action
 * WHY:  Actions minimize expected free energy
 * HOW:  Evaluate response policies, select action that minimizes EFE
 *
 * @param bridge Bridge handle
 * @param response Output recommended response
 * @return 0 on success, error code on failure
 */
int security_immune_fep_active_inference(
    security_immune_fep_bridge_t* bridge,
    immune_fep_response_t* response
);

/**
 * @brief Apply precision modulation based on immune state
 *
 * WHAT: Adapt detection precision based on cytokines and inflammation
 * WHY:  Precision is attention - focus on likely threats
 * HOW:  Update precision based on cytokine levels and threat history
 *
 * @param bridge Bridge handle
 * @return 0 on success, error code on failure
 */
int security_immune_fep_modulate_precision(
    security_immune_fep_bridge_t* bridge
);

/* ============================================================================
 * Detection Feedback API
 * ============================================================================ */

/**
 * @brief Report evasion attempt to FEP
 *
 * WHAT: Feed evasion detection to FEP
 * WHY:  Evasion attempts are high-surprise events (expected detection failed)
 * HOW:  Map to prediction error, increase surveillance precision
 *
 * @param bridge Bridge handle
 * @param antigen_id Evading antigen (if known)
 * @param evasion_method Method of evasion (bitfield)
 * @param confidence Detection confidence [0-1]
 * @return 0 on success, error code on failure
 */
int security_immune_fep_report_evasion(
    security_immune_fep_bridge_t* bridge,
    uint32_t antigen_id,
    uint32_t evasion_method,
    float confidence
);

/**
 * @brief Report autoimmune-like pattern to FEP
 *
 * WHAT: Feed autoimmune detection to FEP
 * WHY:  Self-attack patterns indicate tolerance failure
 * HOW:  Map to prediction error, activate regulatory response
 *
 * @param bridge Bridge handle
 * @param target_component Component being attacked
 * @param attacker_id Attacking immune component
 * @param severity Attack severity [0-1]
 * @return 0 on success, error code on failure
 */
int security_immune_fep_report_autoimmune(
    security_immune_fep_bridge_t* bridge,
    uint32_t target_component,
    uint32_t attacker_id,
    float severity
);

/**
 * @brief Report cytokine storm indicators to FEP
 *
 * WHAT: Feed storm indicators to FEP
 * WHY:  Runaway inflammation is extremely high surprise
 * HOW:  Map cytokine levels to prediction error with highest weight
 *
 * @param bridge Bridge handle
 * @param cytokine_levels Array of 5 cytokine levels [IL1, IL6, TNF, IL10, IFN]
 * @param inflammation_level Current inflammation level
 * @return 0 on success, error code on failure
 */
int security_immune_fep_report_storm(
    security_immune_fep_bridge_t* bridge,
    const float* cytokine_levels,
    brain_inflammation_level_t inflammation_level
);

/**
 * @brief Report memory corruption to FEP
 *
 * WHAT: Feed memory corruption detection to FEP
 * WHY:  Memory failures undermine predictive model
 * HOW:  Map to prediction error, trigger reconsolidation
 *
 * @param bridge Bridge handle
 * @param memory_cell_id Corrupted memory cell
 * @param corruption_type Type of corruption
 * @param severity Corruption severity [0-1]
 * @return 0 on success, error code on failure
 */
int security_immune_fep_report_memory_corruption(
    security_immune_fep_bridge_t* bridge,
    uint32_t memory_cell_id,
    uint32_t corruption_type,
    float severity
);

/**
 * @brief Report false positive for precision learning
 *
 * WHAT: Indicate that a detection was a false positive
 * WHY:  Reduces precision to prevent similar false alarms
 * HOW:  Lower precision for the detection type that triggered FP
 *
 * @param bridge Bridge handle
 * @param detection_type Type of false positive detection
 * @return 0 on success, error code on failure
 */
int security_immune_fep_report_false_positive(
    security_immune_fep_bridge_t* bridge,
    immune_fep_threat_t detection_type
);

/* ============================================================================
 * Cytokine Integration API
 * ============================================================================ */

/**
 * @brief Update FEP from cytokine state
 *
 * WHAT: Sync cytokine levels to FEP precision modulation
 * WHY:  Cytokines represent immune system "attention"
 * HOW:  Map cytokine concentrations to precision adjustments
 *
 * @param bridge Bridge handle
 * @return 0 on success, error code on failure
 */
int security_immune_fep_sync_cytokines(
    security_immune_fep_bridge_t* bridge
);

/**
 * @brief Get cytokine-induced precision modifier
 *
 * WHAT: Compute precision change from cytokine state
 * WHY:  Pro-inflammatory cytokines increase alertness/precision
 * HOW:  Weight each cytokine's effect on precision
 *
 * @param bridge Bridge handle
 * @return Precision modifier (>1 = higher precision, <1 = lower)
 */
float security_immune_fep_get_cytokine_precision_mod(
    const security_immune_fep_bridge_t* bridge
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get FEP effects on immune security
 *
 * @param bridge Bridge handle
 * @param effects Output effects structure
 * @return 0 on success, error code on failure
 */
int security_immune_fep_get_fep_effects(
    const security_immune_fep_bridge_t* bridge,
    fep_to_immune_effects_t* effects
);

/**
 * @brief Get immune effects on FEP
 *
 * @param bridge Bridge handle
 * @param effects Output effects structure
 * @return 0 on success, error code on failure
 */
int security_immune_fep_get_immune_effects(
    const security_immune_fep_bridge_t* bridge,
    immune_to_fep_effects_t* effects
);

/**
 * @brief Get current bridge state
 *
 * @param bridge Bridge handle
 * @param state Output state structure
 * @return 0 on success, error code on failure
 */
int security_immune_fep_get_state(
    const security_immune_fep_bridge_t* bridge,
    security_immune_fep_state_t* state
);

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge handle
 * @param stats Output statistics structure
 * @return 0 on success, error code on failure
 */
int security_immune_fep_get_stats(
    const security_immune_fep_bridge_t* bridge,
    security_immune_fep_stats_t* stats
);

/**
 * @brief Get current free energy level
 *
 * @param bridge Bridge handle
 * @return Current free energy or -1.0f on error
 */
float security_immune_fep_get_free_energy(
    const security_immune_fep_bridge_t* bridge
);

/**
 * @brief Get current threat level
 *
 * @param bridge Bridge handle
 * @return Current threat classification
 */
immune_fep_threat_t security_immune_fep_get_threat_level(
    const security_immune_fep_bridge_t* bridge
);

/**
 * @brief Check if emergency mode is active
 *
 * @param bridge Bridge handle
 * @return true if emergency mode active
 */
bool security_immune_fep_is_emergency_mode(
    const security_immune_fep_bridge_t* bridge
);

/* ============================================================================
 * Debug/Diagnostic API
 * ============================================================================ */

/**
 * @brief Print bridge summary to stdout
 *
 * WHAT: Display human-readable bridge state summary
 * WHY:  Debugging and monitoring
 * HOW:  Format and print key metrics
 *
 * @param bridge Bridge handle
 */
void security_immune_fep_print_summary(
    const security_immune_fep_bridge_t* bridge
);

/**
 * @brief Reset statistics only (preserve state)
 *
 * @param bridge Bridge handle
 * @return 0 on success, error code on failure
 */
int security_immune_fep_reset_stats(
    security_immune_fep_bridge_t* bridge
);

/* ============================================================================
 * Bio-Async Integration API
 * ============================================================================ */

/**
 * @brief Connect to bio-async router
 *
 * WHAT: Register bridge with bio-async system
 * WHY:  Enable inter-module immune threat notifications
 * HOW:  Register module, setup message inbox
 *
 * @param bridge Bridge handle
 * @return 0 on success, error code on failure
 */
int security_immune_fep_connect_bio_async(
    security_immune_fep_bridge_t* bridge
);

/**
 * @brief Disconnect from bio-async router
 *
 * @param bridge Bridge handle
 * @return 0 on success, error code on failure
 */
int security_immune_fep_disconnect_bio_async(
    security_immune_fep_bridge_t* bridge
);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Bridge handle
 * @return true if connected to bio-async router
 */
bool security_immune_fep_is_bio_async_connected(
    const security_immune_fep_bridge_t* bridge
);

/**
 * @brief Process pending bio-async messages
 *
 * WHAT: Process messages from bio-async inbox
 * WHY:  Handle immune notifications from other modules
 * HOW:  Use bio_router_process_inbox to dequeue and process
 *
 * @param bridge Bridge handle
 * @return Number of messages processed, -1 on error
 */
int security_immune_fep_process_messages(
    security_immune_fep_bridge_t* bridge
);

/* ============================================================================
 * String Conversion API
 * ============================================================================ */

/**
 * @brief Convert threat level to string
 *
 * @param threat Threat level
 * @return Human-readable string
 */
const char* immune_fep_threat_to_string(immune_fep_threat_t threat);

/**
 * @brief Convert response type to string
 *
 * @param response Response type
 * @return Human-readable string
 */
const char* immune_fep_response_to_string(immune_fep_response_t response);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SECURITY_IMMUNE_FEP_BRIDGE_H */
