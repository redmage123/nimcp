/**
 * @file nimcp_hypo_predictive_fep_bridge.h
 * @brief Free Energy Principle bridge for Hypothalamus Predictive Integration
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: FEP integration for hypothalamic modulation of predictive processing
 * WHY:  Hypothalamic drives prioritize predictions; prediction accuracy reduces free energy
 * HOW:  Map drive urgency to prediction priority, prediction accuracy to free energy
 *       reduction, and use active inference for adaptive behavior
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * HYPOTHALAMIC DRIVES AS PREDICTION PRIORITIZERS:
 * ------------------------------------------------
 * The hypothalamus modulates which predictions receive processing priority:
 *
 * 1. High HUNGER drive -> Prioritize food-related predictions
 * 2. High SAFETY drive -> Prioritize threat predictions
 * 3. High SOCIAL drive -> Prioritize social outcome predictions
 * 4. Prediction accuracy -> Free energy reduction (homeostasis)
 * 5. Prediction errors -> Drive-weighted surprise signals
 *
 * NEUROBIOLOGICAL CONNECTIONS:
 * - Orexin neurons: Arousal and attentional priority for predictions
 * - Dopamine system: Reward prediction and prediction error signaling
 * - HPA axis: Stress predictions and allostatic regulation
 * - Basal ganglia: Action prediction and motor preparation
 *
 * FEP INTEGRATION:
 * ```
 * Drive State (d) -> Prediction Priority Weights
 *         |
 * Prior Beliefs P(s) weighted by drives
 *         |
 * Prediction: x_hat = g(mu) - what we expect to happen
 *         |
 * Observation: x = sensory input - what actually happens
 *         |
 * Prediction Error: epsilon = x - x_hat
 *         |
 * Free Energy F = sum(drive_weight[i] * precision[i] * epsilon[i]^2)
 *         |
 * Accurate Prediction -> FE Reduction -> Homeostatic balance
 *         |
 * Active Inference: Update predictions OR act to fulfill predictions
 * ```
 *
 * FEP MAPPINGS:
 * - Drive urgency -> Prediction priority (urgent drives = prioritized predictions)
 * - Prediction accuracy -> Free energy reduction (accurate = low FE)
 * - Prediction error -> Surprise signal (weighted by drive relevance)
 * - Model update -> Belief revision (perceptual inference)
 * - Action selection -> Active inference (change world to match predictions)
 *
 * PREDICTION PRIORITY LEVELS:
 * - Background (<0.2)  -> Low-priority background monitoring
 * - Routine (0.2-0.4)  -> Standard processing
 * - Elevated (0.4-0.6) -> Enhanced attention to predictions
 * - High (0.6-0.8)     -> Priority processing, quick updates
 * - Critical (>0.8)    -> Maximum priority, immediate response
 *
 * @see nimcp_hypothalamus_drives.h
 * @see nimcp_predictive.h
 * @see nimcp_free_energy.h
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_HYPO_PREDICTIVE_FEP_BRIDGE_H
#define NIMCP_HYPO_PREDICTIVE_FEP_BRIDGE_H

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

/** Bio-async module ID for hypothalamus predictive FEP bridge */
#define BIO_MODULE_HYPO_PREDICTIVE_FEP    0x0B07

/** Maximum number of tracked prediction channels */
#define HYPO_PRED_MAX_CHANNELS            16

/** Free energy thresholds for prediction urgency */
#define HYPO_PRED_FEP_LOW_THRESHOLD       2.0f   /**< Low urgency */
#define HYPO_PRED_FEP_MEDIUM_THRESHOLD    5.0f   /**< Medium urgency */
#define HYPO_PRED_FEP_HIGH_THRESHOLD      10.0f  /**< High urgency */
#define HYPO_PRED_FEP_CRITICAL_THRESHOLD  20.0f  /**< Critical situation */

/** Precision bounds */
#define HYPO_PRED_FEP_MIN_PRECISION       0.1f   /**< Minimum precision */
#define HYPO_PRED_FEP_MAX_PRECISION       10.0f  /**< Maximum precision */
#define HYPO_PRED_FEP_DEFAULT_PRECISION   1.0f   /**< Default precision */

/** Prediction accuracy bounds */
#define HYPO_PRED_MIN_ACCURACY            0.0f   /**< Complete misprediction */
#define HYPO_PRED_MAX_ACCURACY            1.0f   /**< Perfect prediction */

/** Priority weight bounds */
#define HYPO_PRED_MIN_PRIORITY            0.01f  /**< Minimum priority */
#define HYPO_PRED_MAX_PRIORITY            1.0f   /**< Maximum priority */

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Prediction channel types linked to drives
 *
 * WHAT: Categories of predictions aligned with drive systems
 * WHY:  Different drives prioritize different prediction types
 */
typedef enum {
    HYPO_PRED_CHANNEL_FOOD = 0,           /**< HUNGER -> Food availability predictions */
    HYPO_PRED_CHANNEL_WATER,              /**< THIRST -> Water availability predictions */
    HYPO_PRED_CHANNEL_THREAT,             /**< SAFETY -> Threat predictions */
    HYPO_PRED_CHANNEL_TEMPERATURE,        /**< TEMPERATURE -> Thermal predictions */
    HYPO_PRED_CHANNEL_SOCIAL,             /**< SOCIAL -> Social outcome predictions */
    HYPO_PRED_CHANNEL_MATE,               /**< REPRODUCTIVE -> Mating predictions */
    HYPO_PRED_CHANNEL_NOVELTY,            /**< CURIOSITY -> Novel stimulus predictions */
    HYPO_PRED_CHANNEL_REST,               /**< SLEEP -> Rest opportunity predictions */
    HYPO_PRED_CHANNEL_GENERAL,            /**< General environmental predictions */
    HYPO_PRED_CHANNEL_COUNT
} hypo_pred_channel_t;

/**
 * @brief Prediction priority levels based on drive urgency
 *
 * WHAT: How much priority a prediction channel receives
 * WHY:  Enable drive-modulated resource allocation
 */
typedef enum {
    HYPO_PRED_FEP_LEVEL_BACKGROUND = 0,   /**< Background monitoring */
    HYPO_PRED_FEP_LEVEL_ROUTINE,          /**< Standard processing */
    HYPO_PRED_FEP_LEVEL_ELEVATED,         /**< Enhanced attention */
    HYPO_PRED_FEP_LEVEL_HIGH,             /**< Priority processing */
    HYPO_PRED_FEP_LEVEL_CRITICAL          /**< Maximum priority */
} hypo_pred_fep_level_t;

/**
 * @brief Active inference response types for prediction
 *
 * WHAT: Types of responses via active inference
 * WHY:  Prediction errors require different responses
 */
typedef enum {
    HYPO_PRED_FEP_RESPONSE_CONTINUE = 0,  /**< Continue with current model */
    HYPO_PRED_FEP_RESPONSE_UPDATE,        /**< Update prediction model */
    HYPO_PRED_FEP_RESPONSE_ATTEND,        /**< Increase attention */
    HYPO_PRED_FEP_RESPONSE_ACT,           /**< Take action to fulfill prediction */
    HYPO_PRED_FEP_RESPONSE_EXPLORE,       /**< Explore to reduce uncertainty */
    HYPO_PRED_FEP_RESPONSE_EMERGENCY      /**< Emergency response */
} hypo_pred_fep_response_t;

/* ============================================================================
 * Configuration Structure
 * ============================================================================ */

/**
 * @brief Hypothalamus Predictive FEP configuration
 *
 * WHAT: Configuration for FEP-predictive integration
 * WHY:  Control prediction priority and accuracy tracking
 */
typedef struct {
    /* FEP parameters */
    float drive_fe_weight;                    /**< Weight of drives in free energy */
    float prediction_error_gain;              /**< PE gain from prediction errors */
    float precision_modulation;               /**< Precision based on context */
    bool enable_active_inference;             /**< Allow prediction-based actions */
    bool enable_bio_async;                    /**< Bio-async integration enabled */

    /* Priority computation */
    float drive_to_priority_scale[HYPO_DRIVE_COUNT]; /**< Drive -> priority mapping */
    float base_priority;                      /**< Baseline priority without drive */
    float priority_decay_rate;                /**< Priority decay over time */

    /* Accuracy tracking */
    float accuracy_learning_rate;             /**< Learn from prediction outcomes */
    float accuracy_threshold;                 /**< Threshold for "accurate" */
    bool enable_accuracy_learning;            /**< Learn from accuracy feedback */

    /* FE reduction parameters */
    float fe_reduction_per_accuracy;          /**< FE reduction per accuracy unit */
    float surprise_amplification;             /**< Amplify surprise for urgent drives */

    /* Detection parameters */
    float free_energy_threshold;              /**< FE threshold for detection */
    float surprise_threshold;                 /**< Surprise threshold */
    float precision_learning_rate;            /**< Precision adaptation rate */

    /* Active inference */
    float action_threshold;                   /**< FE threshold to trigger action */
    float exploration_threshold;              /**< FE threshold for exploration */

    /* Learning */
    bool enable_online_learning;              /**< Update FEP from events */
    float learning_rate;                      /**< Belief update rate */
} hypo_pred_fep_config_t;

/* ============================================================================
 * Prediction Channel Structure
 * ============================================================================ */

/**
 * @brief State of a prediction channel
 *
 * WHAT: Tracks predictions and errors for a specific channel
 * WHY:  Enable per-channel priority and accuracy tracking
 */
typedef struct {
    hypo_pred_channel_t channel;              /**< Channel type */
    bool active;                              /**< Is channel being tracked? */

    /* Priority */
    float priority;                           /**< Current priority [0-1] */
    float precision;                          /**< Channel precision */

    /* Predictions */
    float current_prediction;                 /**< Current prediction value */
    float prediction_variance;                /**< Uncertainty in prediction */

    /* Accuracy tracking */
    uint32_t predictions_made;                /**< Total predictions */
    uint32_t predictions_accurate;            /**< Accurate predictions */
    float accuracy_rate;                      /**< Running accuracy rate */
    float prediction_error_ema;               /**< EMA of prediction error */

    /* Free energy */
    float channel_free_energy;                /**< Channel contribution to FE */
    float fe_reduction_cumulative;            /**< Cumulative FE reduction */

    /* Timestamps */
    uint64_t last_prediction_ms;              /**< Last prediction timestamp */
    uint64_t last_update_ms;                  /**< Last update timestamp */
} hypo_pred_channel_state_t;

/* ============================================================================
 * Effects Structures
 * ============================================================================ */

/**
 * @brief FEP effects output for predictive processing
 *
 * WHAT: How FEP analysis affects prediction processing
 * WHY:  Free energy provides prediction prioritization signal
 */
typedef struct {
    float free_energy;                        /**< Current total FE */
    float prediction_error;                   /**< Aggregate PE */
    float precision;                          /**< Current precision */
    float active_inference_strength;          /**< Action strength */

    hypo_pred_fep_level_t priority_level;     /**< Overall priority level */
    float priority_confidence;                /**< Confidence in prioritization */

    hypo_pred_fep_response_t recommended_response; /**< Recommended action */
    float response_urgency;                   /**< Response urgency [0-1] */

    /* Priority outputs per channel */
    float channel_priorities[HYPO_PRED_CHANNEL_COUNT]; /**< Priority per channel */
    float total_priority;                     /**< Sum of priorities */
    hypo_pred_channel_t dominant_channel;     /**< Highest priority channel */
    float dominant_priority;                  /**< Priority of dominant channel */

    /* Accuracy metrics */
    float avg_accuracy;                       /**< Average prediction accuracy */
    float fe_reduction;                       /**< FE reduction from accuracy */
    bool model_update_needed;                 /**< Should update model? */

    /* Surprise metrics */
    float surprise_level;                     /**< Current surprise */
    float weighted_surprise;                  /**< Drive-weighted surprise */
} hypo_pred_fep_effects_t;

/**
 * @brief Predictive effects on FEP
 *
 * WHAT: How predictive state affects FEP beliefs
 * WHY:  Prediction accuracy updates the generative model
 */
typedef struct {
    /* Drive state */
    float drive_urgencies[HYPO_DRIVE_COUNT];  /**< Current drive urgencies */
    hypo_drive_type_t dominant_drive;         /**< Most urgent drive */
    float dominant_urgency;                   /**< Urgency of dominant drive */

    /* Prediction metrics */
    uint64_t total_predictions;               /**< Total predictions made */
    uint64_t accurate_predictions;            /**< Accurate predictions */
    float overall_accuracy;                   /**< Overall accuracy rate */

    /* FE reduction tracking */
    float total_fe_reduction;                 /**< Cumulative FE reduction */
    float avg_fe_reduction_per_prediction;    /**< Average reduction */

    /* Active channels */
    uint32_t active_channel_count;            /**< Number of active channels */
} pred_to_fep_effects_t;

/* ============================================================================
 * State and Statistics Structures
 * ============================================================================ */

/**
 * @brief Prediction tracking state
 *
 * WHAT: Tracks prediction evolution over time
 * WHY:  Enables FEP-based prediction priority adaptation
 */
typedef struct {
    float priority_history[HYPO_PRED_CHANNEL_COUNT][16]; /**< Priority history */
    float fe_history[16];                     /**< Free energy history */
    uint32_t history_idx;                     /**< Current history index */

    float predicted_fe;                       /**< Predicted free energy */
    float fe_velocity;                        /**< Rate of FE change */

    uint64_t last_accurate_time_ms;           /**< Last accurate prediction */
    hypo_pred_channel_t last_accurate_channel; /**< Last accurate channel */
} hypo_pred_tracking_t;

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

    hypo_pred_fep_level_t last_level;         /**< Last priority level */
    uint64_t last_detection_time_ms;          /**< Timestamp of last detection */

    hypo_pred_tracking_t pred_tracking;       /**< Prediction tracking state */

    /* Channel states */
    hypo_pred_channel_state_t channels[HYPO_PRED_CHANNEL_COUNT];
} hypo_pred_fep_state_t;

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
    uint64_t actions_triggered;               /**< Actions from active inference */
    uint64_t precision_adaptations;           /**< Precision updates */

    /* By channel */
    uint64_t predictions_by_channel[HYPO_PRED_CHANNEL_COUNT];
    uint64_t accurate_by_channel[HYPO_PRED_CHANNEL_COUNT];

    float avg_free_energy;                    /**< Average free energy */
    float avg_surprise;                       /**< Average surprise */
    float avg_prediction_error;               /**< Average prediction error */
    float current_precision;                  /**< Current precision */

    float max_free_energy;                    /**< Maximum FE observed */
    float min_free_energy;                    /**< Minimum FE observed */
    float max_fe_reduction;                   /**< Maximum FE reduction */

    float overall_accuracy_rate;              /**< Overall accuracy [0-1] */
    float total_fe_reduction;                 /**< Total FE reduction */
} hypo_pred_fep_stats_t;

/* ============================================================================
 * Bridge Structure
 * ============================================================================ */

/**
 * @brief Hypothalamus Predictive FEP Bridge
 *
 * WHAT: Main bridge connecting hypothalamus drives to predictive processing via FEP
 * WHY:  Centralized integration of drive-modulated prediction with free energy
 * HOW:  Contains configuration, connections, effects, and state
 */
typedef struct {
    bridge_base_t base;                       /**< MUST be first: base infrastructure */

    hypo_pred_fep_config_t config;            /**< Configuration */

    /* System connections */
    hypo_drive_system_handle_t* drive_system; /**< Connected drive system */
    fep_system_t* fep_system;                 /**< Connected FEP system */

    /* Bidirectional effects */
    hypo_pred_fep_effects_t fep_effects;      /**< FEP -> Pred effects */
    pred_to_fep_effects_t pred_effects;       /**< Pred -> FEP effects */

    /* State and statistics */
    hypo_pred_fep_state_t state;              /**< Current state */
    hypo_pred_fep_stats_t stats;              /**< Statistics */
} hypo_pred_fep_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default FEP bridge configuration
 *
 * WHAT: Provide sensible defaults for predictive FEP integration
 * WHY:  Simplify initialization with biologically-plausible settings
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int hypo_pred_fep_default_config(hypo_pred_fep_config_t* config);

/**
 * @brief Create hypothalamus predictive FEP bridge
 *
 * WHAT: Initialize FEP integration for predictive processing
 * WHY:  Enable drive-based prediction prioritization
 *
 * @param config Configuration (NULL for defaults)
 * @param drive_system Drive system handle
 * @param fep_system FEP system handle
 * @return Bridge handle or NULL on failure
 */
hypo_pred_fep_bridge_t* hypo_pred_fep_create(
    const hypo_pred_fep_config_t* config,
    hypo_drive_system_handle_t* drive_system,
    fep_system_t* fep_system
);

/**
 * @brief Destroy hypothalamus predictive FEP bridge
 *
 * @param bridge Bridge handle (NULL safe)
 */
void hypo_pred_fep_destroy(hypo_pred_fep_bridge_t* bridge);

/**
 * @brief Reset bridge to initial state
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int hypo_pred_fep_reset(hypo_pred_fep_bridge_t* bridge);

/**
 * @brief Update bridge state
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int hypo_pred_fep_update(hypo_pred_fep_bridge_t* bridge);

/* ============================================================================
 * Core Operations API
 * ============================================================================ */

/**
 * @brief Compute free energy from prediction state
 *
 * WHAT: Calculate FE from current predictions and errors
 * WHY:  Core FEP computation for predictive processing
 *
 * @param bridge Bridge handle
 * @param drives Drive state input
 * @return 0 on success, -1 on error
 */
int hypo_pred_fep_compute_fe(
    hypo_pred_fep_bridge_t* bridge,
    const hypo_drive_system_t* drives
);

/**
 * @brief Modulate precision based on context
 *
 * WHAT: Adjust prediction precision based on drive urgency
 * WHY:  Urgent drives increase precision for relevant channels
 *
 * @param bridge Bridge handle
 * @param channel Prediction channel
 * @param precision_factor Precision multiplier
 * @return 0 on success, -1 on error
 */
int hypo_pred_fep_modulate_precision(
    hypo_pred_fep_bridge_t* bridge,
    hypo_pred_channel_t channel,
    float precision_factor
);

/**
 * @brief Get current FEP effects
 *
 * @param bridge Bridge handle
 * @param effects Output effects
 * @return 0 on success, -1 on error
 */
int hypo_pred_fep_get_effects(
    const hypo_pred_fep_bridge_t* bridge,
    hypo_pred_fep_effects_t* effects
);

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge handle
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int hypo_pred_fep_get_stats(
    const hypo_pred_fep_bridge_t* bridge,
    hypo_pred_fep_stats_t* stats
);

/* ============================================================================
 * Prediction Channel API
 * ============================================================================ */

/**
 * @brief Register a prediction for a channel
 *
 * WHAT: Record a prediction for tracking
 * WHY:  Enable accuracy tracking and FE computation
 *
 * @param bridge Bridge handle
 * @param channel Prediction channel
 * @param prediction Predicted value
 * @param variance Prediction uncertainty
 * @return 0 on success, -1 on error
 */
int hypo_pred_fep_register_prediction(
    hypo_pred_fep_bridge_t* bridge,
    hypo_pred_channel_t channel,
    float prediction,
    float variance
);

/**
 * @brief Report prediction outcome
 *
 * WHAT: Report whether a prediction was accurate
 * WHY:  Learn from prediction outcomes, reduce FE
 *
 * @param bridge Bridge handle
 * @param channel Prediction channel
 * @param actual_value What actually happened
 * @return 0 on success, -1 on error
 */
int hypo_pred_fep_report_outcome(
    hypo_pred_fep_bridge_t* bridge,
    hypo_pred_channel_t channel,
    float actual_value
);

/**
 * @brief Get channel priority
 *
 * WHAT: Get current priority for a channel
 * WHY:  Query drive-modulated priority
 *
 * @param bridge Bridge handle
 * @param channel Prediction channel
 * @return Priority [0-1] or -1.0f on error
 */
float hypo_pred_fep_get_channel_priority(
    const hypo_pred_fep_bridge_t* bridge,
    hypo_pred_channel_t channel
);

/**
 * @brief Get all channel priorities
 *
 * WHAT: Get priorities for all channels
 * WHY:  Batch priority query
 *
 * @param bridge Bridge handle
 * @param priorities Output array (size HYPO_PRED_CHANNEL_COUNT)
 * @return 0 on success, -1 on error
 */
int hypo_pred_fep_get_all_priorities(
    const hypo_pred_fep_bridge_t* bridge,
    float* priorities
);

/**
 * @brief Get channel accuracy
 *
 * WHAT: Get accuracy rate for a channel
 * WHY:  Monitor prediction performance
 *
 * @param bridge Bridge handle
 * @param channel Prediction channel
 * @return Accuracy [0-1] or -1.0f on error
 */
float hypo_pred_fep_get_channel_accuracy(
    const hypo_pred_fep_bridge_t* bridge,
    hypo_pred_channel_t channel
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
int hypo_pred_fep_connect_bio_async(
    hypo_pred_fep_bridge_t* bridge,
    bio_router_t* router
);

/**
 * @brief Disconnect from bio-async router
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int hypo_pred_fep_disconnect_bio_async(hypo_pred_fep_bridge_t* bridge);

/**
 * @brief Process pending bio-async messages
 *
 * @param bridge Bridge handle
 * @param max_messages Maximum messages to process (0 = all)
 * @return Number of messages processed
 */
int hypo_pred_fep_process_messages(
    hypo_pred_fep_bridge_t* bridge,
    uint32_t max_messages
);

/* ============================================================================
 * Utility API
 * ============================================================================ */

/**
 * @brief Get channel name
 *
 * @param channel Prediction channel
 * @return Human-readable name
 */
const char* hypo_pred_fep_channel_name(hypo_pred_channel_t channel);

/**
 * @brief Get priority level name
 *
 * @param level Priority level
 * @return Human-readable name
 */
const char* hypo_pred_fep_level_name(hypo_pred_fep_level_t level);

/**
 * @brief Get response type name
 *
 * @param response Response type
 * @return Human-readable name
 */
const char* hypo_pred_fep_response_name(hypo_pred_fep_response_t response);

/**
 * @brief Print bridge summary
 *
 * @param bridge Bridge handle
 */
void hypo_pred_fep_print_summary(const hypo_pred_fep_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_HYPO_PREDICTIVE_FEP_BRIDGE_H */
