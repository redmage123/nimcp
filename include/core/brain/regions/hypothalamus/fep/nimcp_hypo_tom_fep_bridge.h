/**
 * @file nimcp_hypo_tom_fep_bridge.h
 * @brief Free Energy Principle bridge for Hypothalamus Theory of Mind Integration
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: FEP integration for hypothalamic modulation of Theory of Mind processing
 * WHY:  SOCIAL drive increases investment in mental modeling of others;
 *       Model accuracy maps to prediction error in the FEP framework
 * HOW:  Map SOCIAL drive to model complexity, other-model accuracy to prediction error,
 *       and use active inference for social behavior selection
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * HYPOTHALAMIC SOCIAL DRIVE AS THEORY OF MIND MODULATOR:
 * -------------------------------------------------------
 * The hypothalamus modulates social cognition through oxytocin and vasopressin:
 *
 * 1. SOCIAL drive urgency -> Investment in mental modeling
 * 2. Low SOCIAL drive -> Minimal ToM processing, less accurate other-models
 * 3. High SOCIAL drive -> Enhanced ToM, more complex/accurate mental models
 * 4. Other-model accuracy -> Prediction error (mispredict = high FE)
 *
 * NEUROBIOLOGICAL CONNECTIONS:
 * - Oxytocin neurons: Social bonding, trust, enhanced mentalizing
 * - Vasopressin: Social recognition, pair bonding
 * - Amygdala-hypothalamus: Threat assessment of social others
 * - mPFC connection: Executive control over social processing
 *
 * FEP INTEGRATION:
 * ```
 * Social Drive (d) -> ToM Complexity Modulation
 *         |
 * Expected Other-State mu (mental model of other)
 *         |
 * Prediction Error: epsilon = observed_behavior - predicted_behavior
 *         |
 * Free Energy F = ToM_error * precision
 *         |
 * Active Inference: Update model or change social behavior
 *         |
 * Precision = f(relationship_importance, social_context, uncertainty)
 * ```
 *
 * FEP MAPPINGS:
 * - SOCIAL drive urgency -> Model complexity (more modeling resources)
 * - Other-model accuracy -> Prediction error (inaccurate = high PE)
 * - Social relationship importance -> Precision weighting
 * - Successful prediction -> Free energy reduction
 * - Failed prediction -> Model update (learning)
 *
 * ToM PROCESSING LEVELS:
 * - Minimal (<0.2)  -> Basic behavior observation
 * - Basic (0.2-0.4) -> Simple intention inference
 * - Moderate (0.4-0.6) -> Belief-desire-intention modeling
 * - Advanced (0.6-0.8) -> False belief understanding
 * - Expert (>0.8)   -> Multi-level recursive modeling (what they think I think)
 *
 * @see nimcp_hypothalamus_drives.h
 * @see nimcp_theory_of_mind.h
 * @see nimcp_free_energy.h
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_HYPO_TOM_FEP_BRIDGE_H
#define NIMCP_HYPO_TOM_FEP_BRIDGE_H

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

/** Bio-async module ID for hypothalamus ToM FEP bridge */
#define BIO_MODULE_HYPO_TOM_FEP    0x0B06

/** Maximum number of tracked agents for ToM */
#define HYPO_TOM_MAX_AGENTS        16

/** Free energy thresholds for ToM processing levels */
#define HYPO_TOM_FEP_LOW_THRESHOLD       2.0f   /**< Low ToM engagement */
#define HYPO_TOM_FEP_MEDIUM_THRESHOLD    5.0f   /**< Medium ToM engagement */
#define HYPO_TOM_FEP_HIGH_THRESHOLD      10.0f  /**< High ToM engagement */
#define HYPO_TOM_FEP_CRITICAL_THRESHOLD  20.0f  /**< Critical social situation */

/** Precision bounds */
#define HYPO_TOM_FEP_MIN_PRECISION       0.1f   /**< Minimum precision */
#define HYPO_TOM_FEP_MAX_PRECISION       10.0f  /**< Maximum precision */
#define HYPO_TOM_FEP_DEFAULT_PRECISION   1.0f   /**< Default precision */

/** Model complexity bounds */
#define HYPO_TOM_MIN_COMPLEXITY          0.1f   /**< Minimal modeling */
#define HYPO_TOM_MAX_COMPLEXITY          1.0f   /**< Maximum complexity */

/** Prediction accuracy bounds */
#define HYPO_TOM_MIN_ACCURACY            0.0f   /**< Complete misprediction */
#define HYPO_TOM_MAX_ACCURACY            1.0f   /**< Perfect prediction */

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Theory of Mind processing depth levels
 *
 * WHAT: Depth of mental modeling based on drive urgency
 * WHY:  Different social contexts require different modeling depths
 */
typedef enum {
    HYPO_TOM_LEVEL_MINIMAL = 0,       /**< Basic behavior observation */
    HYPO_TOM_LEVEL_BASIC,             /**< Simple intention inference */
    HYPO_TOM_LEVEL_MODERATE,          /**< BDI modeling */
    HYPO_TOM_LEVEL_ADVANCED,          /**< False belief understanding */
    HYPO_TOM_LEVEL_EXPERT             /**< Recursive multi-level modeling */
} hypo_tom_fep_level_t;

/**
 * @brief Social relationship importance levels
 *
 * WHAT: How important the relationship is for precision weighting
 * WHY:  More important relationships warrant more accurate modeling
 */
typedef enum {
    HYPO_TOM_RELATION_STRANGER = 0,   /**< Unknown person */
    HYPO_TOM_RELATION_ACQUAINTANCE,   /**< Casual contact */
    HYPO_TOM_RELATION_COLLEAGUE,      /**< Work/task relationship */
    HYPO_TOM_RELATION_FRIEND,         /**< Personal friendship */
    HYPO_TOM_RELATION_FAMILY,         /**< Family member */
    HYPO_TOM_RELATION_INTIMATE        /**< Intimate partner */
} hypo_tom_relation_t;

/**
 * @brief Active inference response types for ToM
 *
 * WHAT: Types of social actions via active inference
 * WHY:  Different prediction errors require different responses
 */
typedef enum {
    HYPO_TOM_FEP_RESPONSE_OBSERVE = 0,    /**< Continue observing */
    HYPO_TOM_FEP_RESPONSE_UPDATE_MODEL,   /**< Update mental model */
    HYPO_TOM_FEP_RESPONSE_SEEK_INFO,      /**< Seek more information */
    HYPO_TOM_FEP_RESPONSE_COMMUNICATE,    /**< Engage communication */
    HYPO_TOM_FEP_RESPONSE_COORDINATE,     /**< Coordinate actions */
    HYPO_TOM_FEP_RESPONSE_REPAIR          /**< Repair relationship */
} hypo_tom_fep_response_t;

/* ============================================================================
 * Configuration Structure
 * ============================================================================ */

/**
 * @brief Hypothalamus ToM FEP configuration
 *
 * WHAT: Configuration for FEP-ToM integration
 * WHY:  Control mental modeling and social prediction behavior
 */
typedef struct {
    /* FEP parameters */
    float drive_fe_weight;                    /**< Weight of drives in free energy */
    float prediction_error_gain;              /**< PE gain from model inaccuracy */
    float precision_modulation;               /**< Precision based on relationship */
    bool enable_active_inference;             /**< Allow social action selection */
    bool enable_bio_async;                    /**< Bio-async integration enabled */

    /* ToM computation */
    float social_drive_to_complexity_scale;   /**< Drive -> complexity mapping */
    float base_model_complexity;              /**< Baseline complexity without drive */
    float complexity_decay_rate;              /**< Complexity decay over time */

    /* Relationship effects */
    float relation_precision_scale[6];        /**< Precision boost per relation type */
    bool enable_relation_modulation;          /**< Adjust precision by relationship */

    /* Prediction learning */
    float model_update_rate;                  /**< Rate of model updates from errors */
    float prediction_learning_rate;           /**< Learn from prediction outcomes */
    bool enable_prediction_learning;          /**< Learn from social predictions */

    /* Detection parameters */
    float free_energy_threshold;              /**< FE threshold for detection */
    float surprise_threshold;                 /**< Surprise threshold */
    float precision_learning_rate;            /**< Precision adaptation rate */

    /* Recursive depth */
    uint32_t max_recursion_depth;             /**< Max "I think you think I think" depth */
    float recursion_cost_per_level;           /**< FE cost per recursion level */

    /* Learning */
    bool enable_online_learning;              /**< Update FEP from events */
    float learning_rate;                      /**< Belief update rate */
} hypo_tom_fep_config_t;

/* ============================================================================
 * Agent Model Structure
 * ============================================================================ */

/**
 * @brief Mental model of another agent
 *
 * WHAT: ToM representation of another agent's mental state
 * WHY:  Track beliefs about others for social prediction
 */
typedef struct {
    uint32_t agent_id;                        /**< Unique agent identifier */
    bool active;                              /**< Is this model in use? */

    /* Relationship */
    hypo_tom_relation_t relationship;         /**< Relationship type */
    float familiarity;                        /**< How well known [0-1] */
    float trust;                              /**< Trust level [0-1] */

    /* Model state */
    float model_complexity;                   /**< Current model depth [0-1] */
    float model_accuracy;                     /**< Recent accuracy [0-1] */
    float model_confidence;                   /**< Confidence in model [0-1] */

    /* Prediction tracking */
    uint32_t predictions_made;                /**< Total predictions */
    uint32_t predictions_correct;             /**< Correct predictions */
    float prediction_error_ema;               /**< EMA of prediction error */

    /* Timestamps */
    uint64_t last_interaction_ms;             /**< Last interaction time */
    uint64_t last_update_ms;                  /**< Last model update time */
} hypo_tom_agent_model_t;

/* ============================================================================
 * Effects Structures
 * ============================================================================ */

/**
 * @brief FEP effects output for ToM
 *
 * WHAT: How FEP analysis affects ToM processing
 * WHY:  Free energy provides social prediction and modeling signal
 */
typedef struct {
    float free_energy;                        /**< Current FE from ToM */
    float prediction_error;                   /**< PE from model inaccuracy */
    float precision;                          /**< Current precision */
    float active_inference_strength;          /**< Action strength */

    hypo_tom_fep_level_t tom_level;           /**< ToM processing depth */
    float processing_confidence;              /**< Confidence in modeling [0-1] */

    hypo_tom_fep_response_t recommended_response; /**< Recommended action */
    float response_urgency;                   /**< Response urgency [0-1] */

    /* Model state summary */
    float avg_model_complexity;               /**< Average model complexity */
    float avg_model_accuracy;                 /**< Average model accuracy */
    uint32_t active_model_count;              /**< Number of active models */

    /* Current focus */
    uint32_t focus_agent_id;                  /**< Currently focused agent */
    float focus_prediction_error;             /**< PE for focus agent */
    bool focus_model_needs_update;            /**< Should update focus model */
} hypo_tom_fep_effects_t;

/**
 * @brief ToM effects on FEP
 *
 * WHAT: How ToM state affects FEP beliefs
 * WHY:  Social cognition updates the generative model
 */
typedef struct {
    /* Drive state */
    float social_drive_urgency;               /**< Current SOCIAL drive */
    float drive_urgencies[HYPO_DRIVE_COUNT];  /**< All drive urgencies */

    /* Social context */
    uint32_t current_social_context;          /**< Current context ID */
    float social_importance;                  /**< Context importance [0-1] */
    float social_uncertainty;                 /**< Uncertainty about others [0-1] */

    /* ToM metrics */
    uint64_t total_predictions;               /**< Total predictions made */
    uint64_t accurate_predictions;            /**< Accurate predictions */
    float overall_accuracy;                   /**< Overall accuracy rate */
    uint64_t model_updates;                   /**< Total model updates */
} tom_to_fep_effects_t;

/* ============================================================================
 * State and Statistics Structures
 * ============================================================================ */

/**
 * @brief ToM tracking state
 *
 * WHAT: Tracks ToM evolution over time
 * WHY:  Enables FEP-based social prediction
 */
typedef struct {
    float complexity_history[16];             /**< Recent complexity history */
    uint32_t history_idx;                     /**< Current history index */

    float predicted_complexity;               /**< Predicted complexity */
    float complexity_velocity;                /**< Rate of complexity change */

    uint64_t last_prediction_time_ms;         /**< Last prediction timestamp */
    uint32_t last_predicted_agent;            /**< Last agent predicted */
} hypo_tom_tracking_t;

/**
 * @brief FEP bridge state
 *
 * WHAT: Current operational state of the bridge
 * WHY:  Track real-time status for monitoring
 */
typedef struct {
    bool active;                              /**< Whether bridge is active */
    uint64_t update_count;                    /**< Number of updates */
    uint64_t prediction_count;                /**< Predictions processed */

    float current_precision;                  /**< Current precision level */
    float avg_surprise;                       /**< Running average surprise */
    float avg_prediction_error;               /**< Running average PE */

    hypo_tom_fep_level_t last_level;          /**< Last ToM level */
    uint64_t last_detection_time_ms;          /**< Timestamp of last detection */

    hypo_tom_tracking_t tom_tracking;         /**< ToM tracking state */

    /* Agent models */
    hypo_tom_agent_model_t agent_models[HYPO_TOM_MAX_AGENTS];
    uint32_t agent_count;                     /**< Number of tracked agents */
} hypo_tom_fep_state_t;

/**
 * @brief FEP bridge statistics
 *
 * WHAT: Cumulative statistics for the bridge
 * WHY:  Performance monitoring and tuning
 */
typedef struct {
    uint64_t total_updates;                   /**< Total updates performed */
    uint64_t fep_predictions;                 /**< FEP-based predictions */
    uint64_t accurate_predictions;            /**< Accurate predictions */
    uint64_t model_updates;                   /**< Model updates triggered */
    uint64_t precision_adaptations;           /**< Precision updates */

    /* By relation type */
    uint64_t predictions_by_relation[6];      /**< Predictions by relation */
    uint64_t accurate_by_relation[6];         /**< Accurate by relation */

    float avg_free_energy;                    /**< Average free energy */
    float avg_surprise;                       /**< Average surprise */
    float avg_prediction_error;               /**< Average prediction error */
    float current_precision;                  /**< Current precision */

    float max_free_energy;                    /**< Maximum FE observed */
    float max_prediction_error;               /**< Maximum PE observed */
    float overall_accuracy_rate;              /**< Overall accuracy [0-1] */

    float avg_model_complexity;               /**< Average model complexity */
    float avg_recursion_depth;                /**< Average recursion depth */
} hypo_tom_fep_stats_t;

/* ============================================================================
 * Bridge Structure
 * ============================================================================ */

/**
 * @brief Hypothalamus Theory of Mind FEP Bridge
 *
 * WHAT: Main bridge connecting hypothalamus drives to ToM via FEP
 * WHY:  Centralized integration of social drive with mental modeling
 * HOW:  Contains configuration, connections, effects, and state
 */
typedef struct {
    bridge_base_t base;                       /**< MUST be first: base infrastructure */

    hypo_tom_fep_config_t config;             /**< Configuration */

    /* System connections */
    hypo_drive_system_handle_t* drive_system; /**< Connected drive system */
    fep_system_t* fep_system;                 /**< Connected FEP system */

    /* Bidirectional effects */
    hypo_tom_fep_effects_t fep_effects;       /**< FEP -> ToM effects */
    tom_to_fep_effects_t tom_effects;         /**< ToM -> FEP effects */

    /* State and statistics */
    hypo_tom_fep_state_t state;               /**< Current state */
    hypo_tom_fep_stats_t stats;               /**< Statistics */
} hypo_tom_fep_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default FEP bridge configuration
 *
 * WHAT: Provide sensible defaults for ToM FEP integration
 * WHY:  Simplify initialization with biologically-plausible settings
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int hypo_tom_fep_default_config(hypo_tom_fep_config_t* config);

/**
 * @brief Create hypothalamus ToM FEP bridge
 *
 * WHAT: Initialize FEP integration for Theory of Mind
 * WHY:  Enable drive-based mental modeling
 *
 * @param config Configuration (NULL for defaults)
 * @param drive_system Drive system handle
 * @param fep_system FEP system handle
 * @return Bridge handle or NULL on failure
 */
hypo_tom_fep_bridge_t* hypo_tom_fep_create(
    const hypo_tom_fep_config_t* config,
    hypo_drive_system_handle_t* drive_system,
    fep_system_t* fep_system
);

/**
 * @brief Destroy hypothalamus ToM FEP bridge
 *
 * @param bridge Bridge handle (NULL safe)
 */
void hypo_tom_fep_destroy(hypo_tom_fep_bridge_t* bridge);

/**
 * @brief Reset bridge to initial state
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int hypo_tom_fep_reset(hypo_tom_fep_bridge_t* bridge);

/**
 * @brief Update bridge state
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int hypo_tom_fep_update(hypo_tom_fep_bridge_t* bridge);

/* ============================================================================
 * Core Operations API
 * ============================================================================ */

/**
 * @brief Compute free energy from ToM state
 *
 * WHAT: Calculate FE from current mental models and prediction errors
 * WHY:  Core FEP computation for social cognition
 *
 * @param bridge Bridge handle
 * @param drives Drive state input
 * @return 0 on success, -1 on error
 */
int hypo_tom_fep_compute_fe(
    hypo_tom_fep_bridge_t* bridge,
    const hypo_drive_system_t* drives
);

/**
 * @brief Modulate precision based on relationship
 *
 * WHAT: Adjust prediction precision based on relationship importance
 * WHY:  More important relationships warrant higher precision
 *
 * @param bridge Bridge handle
 * @param agent_id Agent to set precision for
 * @param relationship Relationship type
 * @return 0 on success, -1 on error
 */
int hypo_tom_fep_modulate_precision(
    hypo_tom_fep_bridge_t* bridge,
    uint32_t agent_id,
    hypo_tom_relation_t relationship
);

/**
 * @brief Get current FEP effects
 *
 * @param bridge Bridge handle
 * @param effects Output effects
 * @return 0 on success, -1 on error
 */
int hypo_tom_fep_get_effects(
    const hypo_tom_fep_bridge_t* bridge,
    hypo_tom_fep_effects_t* effects
);

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge handle
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int hypo_tom_fep_get_stats(
    const hypo_tom_fep_bridge_t* bridge,
    hypo_tom_fep_stats_t* stats
);

/* ============================================================================
 * Agent Model API
 * ============================================================================ */

/**
 * @brief Register or update an agent model
 *
 * WHAT: Add or update mental model for an agent
 * WHY:  Enable ToM tracking for social prediction
 *
 * @param bridge Bridge handle
 * @param agent_id Unique agent identifier
 * @param relationship Relationship type
 * @return 0 on success, -1 on error
 */
int hypo_tom_fep_register_agent(
    hypo_tom_fep_bridge_t* bridge,
    uint32_t agent_id,
    hypo_tom_relation_t relationship
);

/**
 * @brief Get model for an agent
 *
 * WHAT: Retrieve mental model for an agent
 * WHY:  Access current state of other-model
 *
 * @param bridge Bridge handle
 * @param agent_id Agent identifier
 * @param model Output model
 * @return 0 on success, -1 on error
 */
int hypo_tom_fep_get_agent_model(
    const hypo_tom_fep_bridge_t* bridge,
    uint32_t agent_id,
    hypo_tom_agent_model_t* model
);

/**
 * @brief Report prediction outcome for an agent
 *
 * WHAT: Report whether a prediction about an agent was accurate
 * WHY:  Learn from prediction outcomes, update models
 *
 * @param bridge Bridge handle
 * @param agent_id Agent identifier
 * @param prediction_error How wrong the prediction was [0-1]
 * @return 0 on success, -1 on error
 */
int hypo_tom_fep_report_prediction(
    hypo_tom_fep_bridge_t* bridge,
    uint32_t agent_id,
    float prediction_error
);

/**
 * @brief Focus ToM processing on an agent
 *
 * WHAT: Direct ToM resources to a specific agent
 * WHY:  Allocate modeling capacity where needed
 *
 * @param bridge Bridge handle
 * @param agent_id Agent to focus on
 * @return 0 on success, -1 on error
 */
int hypo_tom_fep_focus_agent(
    hypo_tom_fep_bridge_t* bridge,
    uint32_t agent_id
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
int hypo_tom_fep_connect_bio_async(
    hypo_tom_fep_bridge_t* bridge,
    bio_router_t* router
);

/**
 * @brief Disconnect from bio-async router
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int hypo_tom_fep_disconnect_bio_async(hypo_tom_fep_bridge_t* bridge);

/**
 * @brief Process pending bio-async messages
 *
 * @param bridge Bridge handle
 * @param max_messages Maximum messages to process (0 = all)
 * @return Number of messages processed
 */
int hypo_tom_fep_process_messages(
    hypo_tom_fep_bridge_t* bridge,
    uint32_t max_messages
);

/* ============================================================================
 * Utility API
 * ============================================================================ */

/**
 * @brief Get ToM level name
 *
 * @param level ToM processing level
 * @return Human-readable name
 */
const char* hypo_tom_fep_level_name(hypo_tom_fep_level_t level);

/**
 * @brief Get relationship type name
 *
 * @param relation Relationship type
 * @return Human-readable name
 */
const char* hypo_tom_fep_relation_name(hypo_tom_relation_t relation);

/**
 * @brief Get response type name
 *
 * @param response Response type
 * @return Human-readable name
 */
const char* hypo_tom_fep_response_name(hypo_tom_fep_response_t response);

/**
 * @brief Print bridge summary
 *
 * @param bridge Bridge handle
 */
void hypo_tom_fep_print_summary(const hypo_tom_fep_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_HYPO_TOM_FEP_BRIDGE_H */
