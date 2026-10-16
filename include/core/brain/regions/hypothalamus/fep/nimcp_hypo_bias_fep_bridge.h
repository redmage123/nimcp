/**
 * @file nimcp_hypo_bias_fep_bridge.h
 * @brief Free Energy Principle bridge for Hypothalamus Bias Integration
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: FEP integration for hypothalamic bias modulation through drives
 * WHY:  Hypothalamic drives create cognitive biases as survival heuristics;
 *       SAFETY drive promotes risk aversion, HUNGER drive promotes present bias
 * HOW:  Map drive states to bias strengths, bias detection to prediction error,
 *       and use active inference for bias correction decisions
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * HYPOTHALAMIC DRIVES AS COGNITIVE BIAS GENERATORS:
 * --------------------------------------------------
 * The hypothalamus generates primal drives that systematically bias cognition:
 *
 * 1. SAFETY drive -> Risk aversion bias (loss aversion, negativity bias)
 * 2. HUNGER drive -> Present bias (temporal discounting, impulsivity)
 * 3. SOCIAL drive -> Ingroup favoritism, conformity bias
 * 4. REPRODUCTIVE drive -> Attractiveness halo, mate preference bias
 * 5. CURIOSITY drive -> Novelty-seeking bias, confirmation bias (seeking validation)
 *
 * NEUROBIOLOGICAL CONNECTIONS:
 * - Amygdala-hypothalamus: Fear/threat -> risk aversion bias
 * - Orexin system: Hunger -> present-oriented decision making
 * - Oxytocin pathways: Social bonding -> ingroup bias
 * - Dopamine reward: Prediction error -> confirmation bias
 *
 * FEP INTEGRATION:
 * ```
 * Drive State (d) -> Bias Computation
 *         |
 * Expected Unbiased State mu (rational baseline)
 *         |
 * Prediction Error: epsilon = observed_bias - expected_baseline
 *         |
 * Free Energy F = bias_strength * precision
 *         |
 * Active Inference: Debias if FE > threshold
 *         |
 * Precision = f(awareness, cognitive_load, time_pressure)
 * ```
 *
 * FEP MAPPINGS:
 * - Drive urgency -> Bias strength (higher urgency = stronger bias)
 * - Bias detection -> Prediction error (bias deviates from rational baseline)
 * - Cognitive load -> Precision reduction (less capacity to detect bias)
 * - Debiasing success -> Free energy reduction
 *
 * BIAS INTENSITY LEVELS:
 * - Minimal (<0.2)  -> Near-rational processing
 * - Mild (0.2-0.4)  -> Slight systematic distortion
 * - Moderate (0.4-0.6) -> Noticeable bias effects
 * - Strong (0.6-0.8) -> Significant decision distortion
 * - Severe (>0.8)   -> Overwhelming bias, System 1 dominance
 *
 * @see nimcp_hypothalamus_drives.h
 * @see nimcp_bias_detection.h
 * @see nimcp_free_energy.h
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_HYPO_BIAS_FEP_BRIDGE_H
#define NIMCP_HYPO_BIAS_FEP_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "utils/bridge/nimcp_bridge_base.h"
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_drives.h"
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"
#include "utils/validation/nimcp_common.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/platform/nimcp_platform_time.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Bio-async module ID for hypothalamus bias FEP bridge */
#define BIO_MODULE_HYPO_BIAS_FEP    0x0B05

/** Free energy thresholds for bias severity levels */
#define HYPO_BIAS_FEP_LOW_THRESHOLD       2.0f   /**< Low bias */
#define HYPO_BIAS_FEP_MEDIUM_THRESHOLD    5.0f   /**< Medium bias */
#define HYPO_BIAS_FEP_HIGH_THRESHOLD      10.0f  /**< High bias */
#define HYPO_BIAS_FEP_SEVERE_THRESHOLD    20.0f  /**< Severe bias */

/** Precision bounds */
#define HYPO_BIAS_FEP_MIN_PRECISION       0.1f   /**< Minimum precision */
#define HYPO_BIAS_FEP_MAX_PRECISION       10.0f  /**< Maximum precision */
#define HYPO_BIAS_FEP_DEFAULT_PRECISION   1.0f   /**< Default precision */

/** Bias strength bounds */
#define HYPO_BIAS_FEP_MIN_STRENGTH        0.0f   /**< No bias */
#define HYPO_BIAS_FEP_MAX_STRENGTH        1.0f   /**< Maximum bias */

/** Cognitive load impact */
#define HYPO_BIAS_FEP_LOAD_PRECISION_SCALE  0.6f /**< Max precision reduction from load */
#define HYPO_BIAS_FEP_LOAD_BIAS_AMPLIFY     0.4f /**< Max bias amplification from load */

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Types of cognitive bias linked to drives
 *
 * WHAT: Drive-induced cognitive biases
 * WHY:  Different drives generate specific bias patterns
 */
typedef enum {
    HYPO_BIAS_RISK_AVERSION = 0,      /**< SAFETY -> Loss aversion, negativity bias */
    HYPO_BIAS_PRESENT_BIAS,           /**< HUNGER -> Temporal discounting, impulsivity */
    HYPO_BIAS_INGROUP_FAVORITISM,     /**< SOCIAL -> Prefer ingroup members */
    HYPO_BIAS_MATE_PREFERENCE,        /**< REPRODUCTIVE -> Attractiveness halo */
    HYPO_BIAS_NOVELTY_SEEKING,        /**< CURIOSITY -> Prefer novel over familiar */
    HYPO_BIAS_CONFIRMATION,           /**< CURIOSITY -> Seek validating information */
    HYPO_BIAS_AVAILABILITY,           /**< SAFETY -> Overweight recent/vivid events */
    HYPO_BIAS_ANCHORING,              /**< General -> First information dominates */
    HYPO_BIAS_TYPE_COUNT
} hypo_bias_type_t;

/**
 * @brief Bias severity levels based on FEP metrics
 *
 * WHAT: Categorization of bias intensity
 * WHY:  Enable graded debiasing responses
 */
typedef enum {
    HYPO_BIAS_FEP_LEVEL_MINIMAL = 0,  /**< Near-rational */
    HYPO_BIAS_FEP_LEVEL_MILD,         /**< Slight distortion */
    HYPO_BIAS_FEP_LEVEL_MODERATE,     /**< Noticeable bias */
    HYPO_BIAS_FEP_LEVEL_STRONG,       /**< Significant distortion */
    HYPO_BIAS_FEP_LEVEL_SEVERE        /**< Overwhelming System 1 dominance */
} hypo_bias_fep_level_t;

/**
 * @brief Active inference response types for bias
 *
 * WHAT: Types of bias correction via active inference
 * WHY:  Different bias levels require different interventions
 */
typedef enum {
    HYPO_BIAS_FEP_RESPONSE_NONE = 0,      /**< No intervention needed */
    HYPO_BIAS_FEP_RESPONSE_MONITOR,       /**< Monitor for escalation */
    HYPO_BIAS_FEP_RESPONSE_ALERT,         /**< Alert to bias presence */
    HYPO_BIAS_FEP_RESPONSE_SLOW_DOWN,     /**< Engage System 2 */
    HYPO_BIAS_FEP_RESPONSE_DEBIAS         /**< Apply debiasing intervention */
} hypo_bias_fep_response_t;

/* ============================================================================
 * Configuration Structure
 * ============================================================================ */

/**
 * @brief Hypothalamus bias FEP configuration
 *
 * WHAT: Configuration for FEP-bias integration
 * WHY:  Control bias detection and debiasing behavior
 */
typedef struct {
    /* FEP parameters */
    float drive_fe_weight;                    /**< Weight of drives in free energy */
    float prediction_error_gain;              /**< PE gain from bias deviation */
    float precision_modulation;               /**< Precision based on cognitive load */
    bool enable_active_inference;             /**< Allow bias-correction actions */
    bool enable_bio_async;                    /**< Bio-async integration enabled */

    /* Bias computation */
    float drive_to_bias_scale[HYPO_DRIVE_COUNT]; /**< Drive -> bias mapping per drive */
    float bias_decay_rate;                    /**< Natural bias decay over time */
    float awareness_boost;                    /**< Precision boost from awareness */

    /* Cognitive load effects */
    float load_precision_scale;               /**< Load impact on precision */
    float load_bias_amplify;                  /**< Load amplification of bias */
    bool enable_load_effects;                 /**< Consider cognitive load */

    /* Debiasing parameters */
    float debias_threshold;                   /**< FE threshold to trigger debiasing */
    float debias_strength;                    /**< Debiasing intervention strength */
    float debias_learning_rate;               /**< Learn from debiasing outcomes */

    /* Detection parameters */
    float free_energy_threshold;              /**< FE threshold for detection */
    float surprise_threshold;                 /**< Surprise threshold */
    float precision_learning_rate;            /**< Precision adaptation rate */

    /* Learning */
    bool enable_online_learning;              /**< Update FEP from events */
    float learning_rate;                      /**< Belief update rate */
} hypo_bias_fep_config_t;

/* ============================================================================
 * Effects Structures
 * ============================================================================ */

/**
 * @brief FEP effects output for bias
 *
 * WHAT: How FEP analysis affects bias modulation
 * WHY:  Free energy provides bias detection and correction signal
 */
typedef struct {
    float free_energy;                        /**< Current FE from biases */
    float prediction_error;                   /**< PE from bias deviation */
    float precision;                          /**< Current precision */
    float active_inference_strength;          /**< Action strength */

    hypo_bias_fep_level_t bias_level;         /**< Bias severity classification */
    float bias_confidence;                    /**< Detection confidence [0-1] */

    hypo_bias_fep_response_t recommended_response; /**< Recommended action */
    float response_urgency;                   /**< Response urgency [0-1] */

    /* Bias outputs per type */
    float bias_strengths[HYPO_BIAS_TYPE_COUNT]; /**< Strength per bias type */
    float total_bias;                         /**< Aggregate bias strength */
    hypo_bias_type_t dominant_bias;           /**< Strongest bias type */
    float dominant_strength;                  /**< Strength of dominant bias */

    /* Debiasing metrics */
    float debiasing_potential;                /**< Estimated debiasing success [0-1] */
    bool debiasing_recommended;               /**< Should debias now? */
} hypo_bias_fep_effects_t;

/**
 * @brief Bias effects on FEP
 *
 * WHAT: How bias state affects FEP beliefs
 * WHY:  Bias patterns update the generative model
 */
typedef struct {
    /* Drive state */
    float drive_urgencies[HYPO_DRIVE_COUNT];  /**< Current drive urgencies */
    hypo_drive_type_t dominant_drive;         /**< Most urgent drive */

    /* Cognitive state */
    float cognitive_load;                     /**< Current load [0-1] */
    float time_pressure;                      /**< Time pressure level [0-1] */
    float awareness;                          /**< Metacognitive awareness [0-1] */

    /* Bias history */
    uint64_t biases_detected;                 /**< Total biases detected */
    uint64_t debias_attempts;                 /**< Debiasing attempts */
    uint64_t debias_successes;                /**< Successful debias */
    float avg_bias_reduction;                 /**< Average reduction achieved */
} bias_to_fep_effects_t;

/* ============================================================================
 * State and Statistics Structures
 * ============================================================================ */

/**
 * @brief Bias tracking state
 *
 * WHAT: Tracks bias evolution over time
 * WHY:  Enables FEP-based bias prediction
 */
typedef struct {
    float bias_history[HYPO_BIAS_TYPE_COUNT][16]; /**< Recent bias history */
    uint32_t history_idx;                     /**< Current history index */

    float predicted_bias[HYPO_BIAS_TYPE_COUNT]; /**< Predicted bias levels */
    float bias_velocity[HYPO_BIAS_TYPE_COUNT];  /**< Rate of bias change */

    uint64_t last_debias_time_ms;             /**< Last debiasing attempt */
    hypo_bias_type_t last_debiased;           /**< Last bias type addressed */
} hypo_bias_tracking_t;

/**
 * @brief FEP bridge state
 *
 * WHAT: Current operational state of the bridge
 * WHY:  Track real-time status for monitoring
 */
typedef struct {
    bool active;                              /**< Whether bridge is active */
    uint64_t update_count;                    /**< Number of updates */
    uint64_t detection_count;                 /**< Detections processed */

    float current_precision;                  /**< Current precision level */
    float avg_surprise;                       /**< Running average surprise */
    float avg_prediction_error;               /**< Running average PE */

    hypo_bias_fep_level_t last_level;         /**< Last bias level */
    uint64_t last_detection_time_ms;          /**< Timestamp of last detection */

    hypo_bias_tracking_t bias_tracking;       /**< Bias tracking state */
} hypo_bias_fep_state_t;

/**
 * @brief FEP bridge statistics
 *
 * WHAT: Cumulative statistics for the bridge
 * WHY:  Performance monitoring and tuning
 */
typedef struct {
    uint64_t total_updates;                   /**< Total updates performed */
    uint64_t fep_detections;                  /**< FEP-based detections */
    uint64_t biases_detected;                 /**< Total biases detected */
    uint64_t debias_triggered;                /**< Debiasing interventions */
    uint64_t debias_successful;               /**< Successful interventions */
    uint64_t precision_adaptations;           /**< Precision updates */

    /* By bias type */
    uint64_t detection_counts[HYPO_BIAS_TYPE_COUNT]; /**< Detections by type */

    float avg_free_energy;                    /**< Average free energy */
    float avg_surprise;                       /**< Average surprise */
    float avg_prediction_error;               /**< Average prediction error */
    float current_precision;                  /**< Current precision */

    float max_free_energy;                    /**< Maximum FE observed */
    float max_bias_strength;                  /**< Maximum bias observed */
    float avg_debias_reduction;               /**< Average debiasing effect */

    float system2_engagement_rate;            /**< Rate of System 2 activation */
} hypo_bias_fep_stats_t;

/* ============================================================================
 * Bridge Structure
 * ============================================================================ */

/**
 * @brief Hypothalamus Bias FEP Bridge
 *
 * WHAT: Main bridge connecting hypothalamus drives to cognitive bias via FEP
 * WHY:  Centralized integration of drive-induced bias with free energy principle
 * HOW:  Contains configuration, connections, effects, and state
 */
typedef struct {
    bridge_base_t base;                       /**< MUST be first: base infrastructure */

    hypo_bias_fep_config_t config;            /**< Configuration */

    /* System connections */
    hypo_drive_system_handle_t* drive_system; /**< Connected drive system */
    fep_system_t* fep_system;                 /**< Connected FEP system */

    /* Bidirectional effects */
    hypo_bias_fep_effects_t fep_effects;      /**< FEP -> Bias effects */
    bias_to_fep_effects_t bias_effects;       /**< Bias -> FEP effects */

    /* State and statistics */
    hypo_bias_fep_state_t state;              /**< Current state */
    hypo_bias_fep_stats_t stats;              /**< Statistics */
} hypo_bias_fep_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default FEP bridge configuration
 *
 * WHAT: Provide sensible defaults for bias FEP integration
 * WHY:  Simplify initialization with biologically-plausible settings
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int hypo_bias_fep_default_config(hypo_bias_fep_config_t* config);

/**
 * @brief Create hypothalamus bias FEP bridge
 *
 * WHAT: Initialize FEP integration for bias modulation
 * WHY:  Enable drive-based bias detection and correction
 *
 * @param config Configuration (NULL for defaults)
 * @param drive_system Drive system handle
 * @param fep_system FEP system handle
 * @return Bridge handle or NULL on failure
 */
hypo_bias_fep_bridge_t* hypo_bias_fep_create(
    const hypo_bias_fep_config_t* config,
    hypo_drive_system_handle_t* drive_system,
    fep_system_t* fep_system
);

/**
 * @brief Destroy hypothalamus bias FEP bridge
 *
 * @param bridge Bridge handle (NULL safe)
 */
void hypo_bias_fep_destroy(hypo_bias_fep_bridge_t* bridge);

/**
 * @brief Reset bridge to initial state
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int hypo_bias_fep_reset(hypo_bias_fep_bridge_t* bridge);

/**
 * @brief Update bridge state
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int hypo_bias_fep_update(hypo_bias_fep_bridge_t* bridge);

/* ============================================================================
 * Core Operations API
 * ============================================================================ */

/**
 * @brief Compute free energy from drive-induced bias
 *
 * WHAT: Calculate FE from current drive states and resulting biases
 * WHY:  Core FEP computation for bias detection
 *
 * @param bridge Bridge handle
 * @param drives Drive state input
 * @return 0 on success, -1 on error
 */
int hypo_bias_fep_compute_fe(
    hypo_bias_fep_bridge_t* bridge,
    const hypo_drive_system_t* drives
);

/**
 * @brief Modulate precision based on cognitive load
 *
 * WHAT: Adjust bias detection precision based on cognitive load
 * WHY:  High load reduces ability to detect and correct bias
 *
 * @param bridge Bridge handle
 * @param cognitive_load Cognitive load level [0-1]
 * @return 0 on success, -1 on error
 */
int hypo_bias_fep_modulate_precision(
    hypo_bias_fep_bridge_t* bridge,
    float cognitive_load
);

/**
 * @brief Get current FEP effects
 *
 * @param bridge Bridge handle
 * @param effects Output effects
 * @return 0 on success, -1 on error
 */
int hypo_bias_fep_get_effects(
    const hypo_bias_fep_bridge_t* bridge,
    hypo_bias_fep_effects_t* effects
);

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge handle
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int hypo_bias_fep_get_stats(
    const hypo_bias_fep_bridge_t* bridge,
    hypo_bias_fep_stats_t* stats
);

/* ============================================================================
 * Bias API
 * ============================================================================ */

/**
 * @brief Get bias strength for a specific type
 *
 * WHAT: Get the current strength for a specific bias type
 * WHY:  Enable bias-specific queries
 *
 * @param bridge Bridge handle
 * @param bias_type Bias type
 * @return Bias strength [0-1] or -1.0f on error
 */
float hypo_bias_fep_get_strength(
    const hypo_bias_fep_bridge_t* bridge,
    hypo_bias_type_t bias_type
);

/**
 * @brief Get all bias strengths
 *
 * WHAT: Get strengths for all bias types
 * WHY:  Enable comprehensive bias assessment
 *
 * @param bridge Bridge handle
 * @param strengths Output array (size HYPO_BIAS_TYPE_COUNT)
 * @return 0 on success, -1 on error
 */
int hypo_bias_fep_get_strengths(
    const hypo_bias_fep_bridge_t* bridge,
    float* strengths
);

/**
 * @brief Trigger debiasing for a specific bias type
 *
 * WHAT: Initiate debiasing intervention
 * WHY:  Active inference response to high bias
 *
 * @param bridge Bridge handle
 * @param bias_type Bias type to address
 * @param intensity Intervention intensity [0-1]
 * @return 0 on success, -1 on error
 */
int hypo_bias_fep_trigger_debias(
    hypo_bias_fep_bridge_t* bridge,
    hypo_bias_type_t bias_type,
    float intensity
);

/**
 * @brief Report debiasing outcome
 *
 * WHAT: Report success/failure of debiasing attempt
 * WHY:  Learn from intervention outcomes
 *
 * @param bridge Bridge handle
 * @param bias_type Bias type addressed
 * @param reduction Achieved reduction [0-1]
 * @return 0 on success, -1 on error
 */
int hypo_bias_fep_report_debias_outcome(
    hypo_bias_fep_bridge_t* bridge,
    hypo_bias_type_t bias_type,
    float reduction
);

/* ============================================================================
 * Bio-Async API
 * ============================================================================ */

/**
 * @brief Connect to bio-async router
 *
 * @param bridge Bridge handle
 * @param router Bio-router handle
 * @return 0 on success, -1 on error
 */
int hypo_bias_fep_connect_bio_async(
    hypo_bias_fep_bridge_t* bridge,
    bio_router_t* router
);

/**
 * @brief Disconnect from bio-async router
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int hypo_bias_fep_disconnect_bio_async(hypo_bias_fep_bridge_t* bridge);

/**
 * @brief Process pending bio-async messages
 *
 * @param bridge Bridge handle
 * @param max_messages Maximum messages to process (0 = all)
 * @return Number of messages processed
 */
int hypo_bias_fep_process_messages(
    hypo_bias_fep_bridge_t* bridge,
    uint32_t max_messages
);

/* ============================================================================
 * Utility API
 * ============================================================================ */

/**
 * @brief Get bias type name
 *
 * @param bias_type Bias type
 * @return Human-readable name
 */
const char* hypo_bias_fep_type_name(hypo_bias_type_t bias_type);

/**
 * @brief Get bias level name
 *
 * @param level Bias level
 * @return Human-readable name
 */
const char* hypo_bias_fep_level_name(hypo_bias_fep_level_t level);

/**
 * @brief Get response type name
 *
 * @param response Response type
 * @return Human-readable name
 */
const char* hypo_bias_fep_response_name(hypo_bias_fep_response_t response);

/**
 * @brief Print bridge summary
 *
 * @param bridge Bridge handle
 */
void hypo_bias_fep_print_summary(const hypo_bias_fep_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_HYPO_BIAS_FEP_BRIDGE_H */
