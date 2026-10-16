/**
 * @file nimcp_omni_wm_hypothalamus_bridge.h
 * @brief World Model Hypothalamus Bridge - Homeostatic Control Integration
 * @version 1.0.0
 * @date 2026-01-17
 *
 * WHAT: Bidirectional bridge connecting World Model (RSSM) with Hypothalamus systems
 * WHY:  Enable homeostatic-informed world modeling and prediction-driven resource planning
 * HOW:  Drive states modulate predictions; WM predicts resource availability
 *
 * THEORETICAL FOUNDATION:
 * =======================
 *
 * HOMEOSTATIC PREDICTIVE PROCESSING (Seth & Friston, 2016):
 * ---------------------------------------------------------
 * The hypothalamus maintains homeostatic setpoints, but these setpoints are
 * themselves predictions about optimal internal states. The world model can
 * enhance homeostatic control by:
 *
 *   1. Predicting future resource availability
 *   2. Anticipating homeostatic challenges
 *   3. Adjusting predictions based on internal drive states
 *
 * BYRNES' STEERING SUBSYSTEM (2022):
 * ----------------------------------
 * Steve Byrnes' insight: the hypothalamus (steering subsystem ~10% of brain)
 * sends reward signals that steer the learning subsystem (~90%). The WM
 * bridge allows predictions to be shaped by alignment-safe drive parameters:
 *
 *   Hypothalamus -> WM: Drive states bias prediction priorities
 *   WM -> Hypothalamus: Predicted rewards guide setpoint adjustment
 *
 * CIRCADIAN INTEGRATION:
 * ----------------------
 * Time-of-day strongly affects both predictions and homeostatic control:
 *
 *   - Morning: Higher arousal predictions, increased exploration
 *   - Evening: Conservative predictions, consolidation mode
 *   - Night: Minimal predictions, consolidation and maintenance
 *
 * DATA FLOW:
 * ----------
 *   Hypothalamus -> WM: Homeostatic drive states (hunger, arousal, stress)
 *   WM -> Hypothalamus: Predicted resource availability
 *   Circadian -> WM: Time-of-day prediction adjustments
 *   WM -> Circadian: Phase-shifted anticipatory predictions
 *
 * KEY FEATURES:
 * -------------
 *   - Drive state modulation of prediction priorities
 *   - Stress-induced conservative predictions
 *   - Circadian rhythm alignment
 *   - Reward prediction based on internal states
 *
 * BIO-ASYNC:
 *   Module ID: 0x0E67
 *   Message Range: 0x6700-0x67FF
 */

#ifndef NIMCP_OMNI_WM_HYPOTHALAMUS_BRIDGE_H
#define NIMCP_OMNI_WM_HYPOTHALAMUS_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/error/nimcp_error_codes.h"
#include "async/nimcp_bio_messages.h"  /* BIO_MSG_WM_HYPOTHAL_* message types */
/* Phase 8: Forward declaration for health agent */
typedef struct nimcp_health_agent nimcp_health_agent_t;


#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

/* World Model (from nimcp_omni_world_model.h) */
typedef struct omni_world_model omni_world_model_t;

/* Hypothalamus Drive System (from nimcp_hypothalamus_drives.h) */
typedef struct hypo_drive_system hypo_drive_system_handle_t;

/* Hypothalamus Homeostasis (from nimcp_hypothalamus_homeostasis.h) */
typedef struct hypo_homeostasis hypo_homeostasis_handle_t;

/* Circadian System (from nimcp_medulla.h)
 * Note: Actual type defined in nimcp_medulla.h. Forward declare struct and typedef. */
struct circadian_rhythm_struct;
typedef struct circadian_rhythm_struct* circadian_rhythm_t;

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Bio-async module ID for World Model Hypothalamus Bridge */
#define BIO_MODULE_WM_HYPOTHALAMUS_BRIDGE       0x0E67

/** Maximum number of drives to track */
#define WM_HYPO_MAX_DRIVES                      16

/** Maximum prediction horizon in steps */
#define WM_HYPO_MAX_PREDICTION_HORIZON          32

/** Maximum resource types to track */
#define WM_HYPO_MAX_RESOURCE_TYPES              8

/** Default stress threshold for conservative predictions */
#define WM_HYPO_DEFAULT_STRESS_THRESHOLD        0.7f

/** Default drive urgency threshold for priority boost */
#define WM_HYPO_DEFAULT_URGENCY_THRESHOLD       0.6f

/** Default circadian modulation strength */
#define WM_HYPO_DEFAULT_CIRCADIAN_STRENGTH      0.5f

/** Maximum state dimension for internal buffers */
#define WM_HYPO_MAX_STATE_DIM                   256

/* ============================================================================
 * Bio-Async Message Types (0x6700-0x67FF)
 * ============================================================================
 * Message types are defined in nimcp_bio_messages.h to avoid duplication.
 * Key message types used by this bridge:
 *   - BIO_MSG_WM_HYPOTHAL_DRIVE_STATE (0x6700): Homeostatic drive state
 *   - BIO_MSG_WM_HYPOTHAL_CIRCADIAN: Circadian rhythm signal
 *   - BIO_MSG_WM_HYPOTHAL_RESOURCE_PRED: Resource availability prediction
 *   - BIO_MSG_WM_HYPOTHAL_REWARD_PRED: Reward prediction from drives
 * ============================================================================ */

/** @brief Message type alias for Hypothalamus bridge (uses bio_message_type_t from nimcp_bio_messages.h) */
typedef bio_message_type_t omni_wm_hypothalamus_msg_type_t;

/* ============================================================================
 * Drive State Structures
 * ============================================================================ */

/**
 * @brief Drive state for world model integration
 *
 * WHAT: Simplified drive state for WM prediction modulation
 * WHY:  WM needs to know which drives are active to prioritize predictions
 * HOW:  Extract key fields from full drive system
 */
typedef struct {
    uint32_t drive_type;            /**< Drive type identifier (hypo_drive_type_t) */
    float level;                    /**< Current drive level [0, 1] */
    float urgency;                  /**< Urgency/priority weight [0, 1] */
    float deviation_from_setpoint;  /**< How far from optimal */
    float time_since_satisfied;     /**< Time since last satisfaction (seconds) */
    bool is_active;                 /**< Drive currently motivating behavior */
} wm_drive_state_t;

/**
 * @brief Complete drive system state for WM
 *
 * WHAT: Vector of all drives for comprehensive state representation
 * WHY:  WM predictions should consider all homeostatic needs
 * HOW:  Array of individual drive states plus global modulation
 */
typedef struct {
    wm_drive_state_t drives[WM_HYPO_MAX_DRIVES];
    uint32_t drive_count;           /**< Number of active drives */
    uint32_t priority_drive;        /**< Currently dominant drive type */
    float global_arousal;           /**< Global arousal level [0, 1] */
    float global_stress;            /**< Global stress level [0, 1] */
    float reward_signal;            /**< Current reward signal [-1, 1] */
    uint64_t timestamp_us;          /**< When state was captured */
} wm_hypothal_drive_vector_t;

/* ============================================================================
 * Circadian State Structures
 * ============================================================================ */

/**
 * @brief Circadian state for world model integration
 *
 * WHAT: Circadian phase and modulation factors for WM
 * WHY:  Time-of-day affects prediction accuracy and priority
 * HOW:  Extract modulation factors from circadian system
 */
typedef struct {
    uint32_t phase;                 /**< Current circadian phase (circadian_phase_t) */
    float cycle_position;           /**< Position in cycle [0, 1] (0=midnight) */
    float arousal_modulation;       /**< Arousal factor [0, 1] */
    float learning_rate_modulation; /**< Learning rate factor [0, 1] */
    float consolidation_modulation; /**< Consolidation factor [0, 1] */
    float metabolism_modulation;    /**< Metabolism factor [0, 1] */
    float sleep_pressure;           /**< Homeostatic sleep drive [0, 1] */
    bool is_sleep_period;           /**< Currently in sleep period */
    uint64_t timestamp_us;          /**< When state was captured */
} wm_circadian_state_t;

/* ============================================================================
 * Resource Prediction Structures
 * ============================================================================ */

/**
 * @brief Resource type enumeration
 */
typedef enum {
    WM_RESOURCE_ENERGY = 0,         /**< Metabolic energy (glucose, ATP) */
    WM_RESOURCE_WATER,              /**< Hydration */
    WM_RESOURCE_SAFETY,             /**< Safety/threat level */
    WM_RESOURCE_SOCIAL,             /**< Social connection availability */
    WM_RESOURCE_INFORMATION,        /**< Information gain opportunity */
    WM_RESOURCE_REST,               /**< Rest/recovery opportunity */
    WM_RESOURCE_COMPUTATION,        /**< Computational resource availability */
    WM_RESOURCE_MEMORY,             /**< Memory resource availability */
    WM_RESOURCE_COUNT
} wm_resource_type_t;

/**
 * @brief Resource availability prediction
 *
 * WHAT: WM prediction of resource availability over horizon
 * WHY:  Hypothalamus can use predictions to adjust setpoints
 * HOW:  Predict resource levels at future timesteps
 */
typedef struct {
    wm_resource_type_t type;        /**< Resource type */
    float current_availability;     /**< Current availability [0, 1] */
    float* predicted_availability;  /**< Predicted availability over horizon */
    uint32_t horizon_steps;         /**< Number of prediction steps */
    float prediction_confidence;    /**< Confidence in prediction */
    float mean_predicted;           /**< Mean predicted availability */
    float min_predicted;            /**< Minimum predicted (worst case) */
    float time_to_depletion;        /**< Estimated time to depletion (if < 0.2) */
} wm_resource_prediction_t;

/**
 * @brief Complete resource forecast
 */
typedef struct {
    wm_resource_prediction_t resources[WM_HYPO_MAX_RESOURCE_TYPES];
    uint32_t resource_count;        /**< Number of resources tracked */
    float overall_resource_score;   /**< Aggregate resource availability */
    float forecast_confidence;      /**< Overall forecast confidence */
    uint64_t forecast_timestamp_us; /**< When forecast was generated */
} wm_resource_forecast_t;

/* ============================================================================
 * Configuration Structure
 * ============================================================================ */

/**
 * @brief World Model Hypothalamus Bridge configuration
 *
 * WHAT: Parameters controlling WM-Hypothalamus integration
 * WHY:  Tune drive modulation, circadian alignment, and prediction behavior
 * HOW:  Configurable thresholds, strengths, and feature flags
 */
typedef struct {
    /* General Settings */
    bool enable_modulation;                 /**< Enable bidirectional modulation */
    float sensitivity;                      /**< General sensitivity [0.5-2.0] */

    /* Drive Modulation Settings */
    bool enable_drive_modulation;           /**< Let drives modulate predictions */
    float drive_urgency_threshold;          /**< Threshold for priority boost */
    float drive_modulation_strength;        /**< How much drives affect predictions */
    bool enable_reward_prediction;          /**< Predict rewards based on drives */

    /* Stress and Conservative Mode Settings */
    bool enable_stress_modulation;          /**< Let stress affect predictions */
    float stress_threshold;                 /**< Threshold for conservative mode */
    float conservative_confidence_scale;    /**< Scale confidence when stressed */
    float conservative_horizon_scale;       /**< Scale horizon when stressed */

    /* Circadian Integration Settings */
    bool enable_circadian_modulation;       /**< Let circadian affect predictions */
    float circadian_modulation_strength;    /**< How much circadian affects WM */
    bool enable_phase_prediction;           /**< Predict circadian phase effects */
    bool enable_sleep_pressure_tracking;    /**< Track sleep pressure in WM */

    /* Resource Prediction Settings */
    bool enable_resource_prediction;        /**< Predict resource availability */
    uint32_t resource_prediction_horizon;   /**< Steps ahead to predict */
    float resource_confidence_threshold;    /**< Min confidence for forecasts */

    /* Homeostasis Integration Settings */
    bool enable_homeostasis_feedback;       /**< Feed WM state to homeostasis */
    float homeostasis_learning_rate;        /**< Learning rate for setpoint updates */
    bool enable_setpoint_prediction;        /**< Predict optimal setpoints */

    /* Alignment Settings (Byrnes' insight) */
    bool enable_alignment_checks;           /**< Check predictions against alignment */
    float alignment_weight;                 /**< Weight for alignment in reward */

    /* Bio-async Settings */
    bool enable_bio_async;                  /**< Enable bio-async messaging */
} omni_wm_hypothalamus_bridge_config_t;

/* ============================================================================
 * Effect Structures
 * ============================================================================ */

/**
 * @brief Effects from Hypothalamus to World Model
 *
 * WHAT: Homeostatic information flowing to world model
 * WHY:  WM predictions should be informed by internal states
 * HOW:  Drive states, circadian, and stress modulate predictions
 */
typedef struct {
    /* Drive State Effects */
    wm_hypothal_drive_vector_t drive_vector;  /**< Current drive states */
    float drive_priority_boost[WM_HYPO_MAX_DRIVES]; /**< Prediction priority boosts */
    uint32_t prediction_focus_drive;          /**< Which drive to prioritize */

    /* Arousal and Stress Effects */
    float arousal_level;                      /**< Current arousal [0, 1] */
    float stress_level;                       /**< Current stress [0, 1] */
    bool conservative_mode_active;            /**< Use conservative predictions */
    float prediction_confidence_modifier;     /**< Modifier for WM confidence */
    float prediction_horizon_modifier;        /**< Modifier for prediction horizon */

    /* Circadian Effects */
    wm_circadian_state_t circadian_state;     /**< Current circadian state */
    float learning_rate_modifier;             /**< Circadian learning rate mod */
    float exploration_modifier;               /**< Circadian exploration mod */

    /* Reward Signal */
    float current_reward;                     /**< Current reward signal [-1, 1] */
    float reward_prediction_target;           /**< Target for reward prediction */
    float alignment_score;                    /**< Current alignment score [0, 1] */

    /* Homeostatic Errors */
    float homeostatic_error_total;            /**< Sum of setpoint deviations */
    float* controller_outputs;                /**< PID controller outputs */
    uint32_t controller_count;                /**< Number of controllers */
} hypothalamus_to_omni_wm_effects_t;

/**
 * @brief Effects from World Model to Hypothalamus
 *
 * WHAT: WM predictions flowing to hypothalamus
 * WHY:  Hypothalamus can use predictions for anticipatory control
 * HOW:  Resource forecasts, predicted rewards, state predictions
 */
typedef struct {
    /* Resource Predictions */
    wm_resource_forecast_t resource_forecast; /**< Predicted resource availability */
    bool resource_scarcity_predicted;         /**< Scarcity predicted ahead */
    float time_to_scarcity;                   /**< When scarcity expected */

    /* Reward Predictions */
    float predicted_reward;                   /**< WM predicted reward */
    float reward_prediction_confidence;       /**< Confidence in prediction */
    float predicted_drive_satisfaction[WM_HYPO_MAX_DRIVES]; /**< Per-drive satisfaction */

    /* State Predictions */
    float* predicted_world_state;             /**< Predicted future world state */
    uint32_t predicted_state_dim;             /**< Dimension of prediction */
    uint32_t prediction_horizon;              /**< How far ahead */
    float state_prediction_confidence;        /**< Confidence in state prediction */

    /* Optimal Setpoint Suggestions */
    float* suggested_setpoints;               /**< WM suggested setpoints */
    uint32_t setpoint_count;                  /**< Number of suggestions */
    float setpoint_confidence;                /**< Confidence in suggestions */

    /* Counterfactual Results */
    bool has_counterfactual;                  /**< Counterfactual available */
    float counterfactual_reward_diff;         /**< Reward difference if action changed */
    float counterfactual_confidence;          /**< Confidence in counterfactual */

    /* Anomaly Detection */
    bool anomaly_detected;                    /**< Anomaly in predictions */
    float anomaly_magnitude;                  /**< How anomalous */
    uint32_t anomaly_type;                    /**< Type of anomaly */
} omni_wm_to_hypothalamus_effects_t;

/* ============================================================================
 * Statistics Structure
 * ============================================================================ */

/**
 * @brief World Model Hypothalamus Bridge statistics
 *
 * WHAT: Metrics for monitoring bridge operation
 * WHY:  Performance tracking, debugging, and optimization
 * HOW:  Counters, averages, and error metrics
 */
typedef struct {
    /* Drive Modulation Statistics */
    uint64_t drive_updates_received;          /**< Drive state updates received */
    uint64_t priority_changes;                /**< Priority drive changes */
    uint64_t conservative_mode_entries;       /**< Times entered conservative mode */
    float mean_stress_level;                  /**< Average stress level */
    float mean_arousal_level;                 /**< Average arousal level */

    /* Circadian Statistics */
    uint64_t circadian_updates_received;      /**< Circadian updates received */
    uint64_t phase_transitions;               /**< Circadian phase transitions */
    float mean_learning_modifier;             /**< Average learning rate modifier */

    /* Resource Prediction Statistics */
    uint64_t resource_predictions_generated;  /**< Resource forecasts made */
    float mean_resource_confidence;           /**< Average forecast confidence */
    uint64_t scarcity_predictions;            /**< Scarcity warnings issued */

    /* Reward Prediction Statistics */
    uint64_t reward_predictions_made;         /**< Reward predictions made */
    float mean_reward_prediction_error;       /**< Average prediction error */
    float mean_alignment_score;               /**< Average alignment score */

    /* Timing Statistics */
    uint64_t total_updates;                   /**< Total update cycles */
    double total_processing_time_ms;          /**< Total processing time */
    double mean_update_time_ms;               /**< Average update duration */
    uint64_t last_update_time_us;             /**< Last update timestamp */

    /* Error Statistics */
    uint64_t errors_total;                    /**< Total errors encountered */
    uint64_t errors_drive;                    /**< Drive-related errors */
    uint64_t errors_circadian;                /**< Circadian-related errors */
    uint64_t errors_prediction;               /**< Prediction-related errors */
} omni_wm_hypothalamus_bridge_stats_t;

/* ============================================================================
 * Main Bridge Structure
 * ============================================================================ */

/**
 * @brief World Model Hypothalamus Bridge
 *
 * WHAT: Main bridge structure connecting WM with hypothalamus systems
 * WHY:  Orchestrates bidirectional information flow
 * HOW:  Maintains connections, effects, and state
 *
 * Memory Layout:
 *   bridge_base_t base MUST be first for pointer casting compatibility
 */
typedef struct omni_wm_hypothalamus_bridge {
    bridge_base_t base;                       /**< MUST be first: base bridge infrastructure */

    /* Configuration */
    omni_wm_hypothalamus_bridge_config_t config; /**< Bridge configuration */

    /* Connected Systems */
    omni_world_model_t* world_model;          /**< World model (RSSM) */
    hypo_drive_system_handle_t* drive_system; /**< Hypothalamus drive system */
    hypo_homeostasis_handle_t* homeostasis;   /**< Homeostasis system */
    circadian_rhythm_t* circadian;            /**< Circadian rhythm system */

    /* Bidirectional Effects */
    hypothalamus_to_omni_wm_effects_t hypo_to_wm;   /**< Effects: Hypothalamus -> WM */
    omni_wm_to_hypothalamus_effects_t wm_to_hypo;   /**< Effects: WM -> Hypothalamus */

    /* Internal State */
    bool conservative_mode;                   /**< Currently in conservative mode */
    float current_stress_smoothed;            /**< Smoothed stress level */
    float current_arousal_smoothed;           /**< Smoothed arousal level */
    uint32_t current_circadian_phase;         /**< Current circadian phase */

    /* Resource Tracking */
    float resource_levels[WM_HYPO_MAX_RESOURCE_TYPES];     /**< Current resource levels */
    float resource_predictions[WM_HYPO_MAX_RESOURCE_TYPES][WM_HYPO_MAX_PREDICTION_HORIZON];

    /* Prediction State */
    float* last_predicted_state;              /**< Last WM state prediction */
    uint32_t last_prediction_dim;             /**< Dimension of last prediction */
    float last_prediction_confidence;         /**< Confidence of last prediction */

    /* Reward Tracking */
    float reward_prediction_running_error;    /**< Running reward prediction error */
    uint64_t reward_prediction_count;         /**< Number of reward predictions */

    /* Statistics */
    omni_wm_hypothalamus_bridge_stats_t stats; /**< Bridge statistics */

    /* Phase 8: Instance-level health agent */
    nimcp_health_agent_t* health_agent;         /**< Per-instance health agent */
} omni_wm_hypothalamus_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * WHAT: Returns sensible defaults for bridge configuration
 * WHY:  Convenient initialization with biologically-plausible values
 * HOW:  Sets all config fields to defaults
 *
 * @param config Configuration structure to initialize
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_hypothalamus_bridge_default_config(
    omni_wm_hypothalamus_bridge_config_t* config);

/**
 * @brief Create World Model Hypothalamus Bridge
 *
 * WHAT: Allocate and initialize bridge
 * WHY:  Required before connecting systems
 * HOW:  Allocate structure, initialize base, set config
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge instance or NULL on failure
 */
omni_wm_hypothalamus_bridge_t* omni_wm_hypothalamus_bridge_create(
    const omni_wm_hypothalamus_bridge_config_t* config);

/**
 * @brief Destroy World Model Hypothalamus Bridge
 *
 * WHAT: Clean up and free bridge resources
 * WHY:  Proper resource management
 * HOW:  Disconnect systems, free buffers, cleanup base
 *
 * @param bridge Bridge to destroy (NULL-safe)
 */
void omni_wm_hypothalamus_bridge_destroy(omni_wm_hypothalamus_bridge_t* bridge);

/**
 * @brief Reset bridge state
 *
 * WHAT: Reset effects and statistics, keep connections
 * WHY:  Allow fresh start without reconnection
 * HOW:  Zero effects, reset stats, preserve config
 *
 * @param bridge Bridge to reset
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_hypothalamus_bridge_reset(omni_wm_hypothalamus_bridge_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

/**
 * @brief Connect all hypothalamus systems to bridge
 *
 * WHAT: Establish connections to WM, drives, homeostasis, and circadian
 * WHY:  Single call to wire up all systems
 * HOW:  Store pointers, validate connections, activate bridge
 *
 * @param bridge Bridge instance
 * @param world_model World model (RSSM) - required
 * @param drive_system Drive system - optional
 * @param homeostasis Homeostasis system - optional
 * @param circadian Circadian system - optional
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_hypothalamus_bridge_connect(
    omni_wm_hypothalamus_bridge_t* bridge,
    omni_world_model_t* world_model,
    hypo_drive_system_handle_t* drive_system,
    hypo_homeostasis_handle_t* homeostasis,
    circadian_rhythm_t* circadian);

/**
 * @brief Connect world model
 *
 * @param bridge Bridge instance
 * @param world_model World model to connect
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_hypothalamus_bridge_connect_world_model(
    omni_wm_hypothalamus_bridge_t* bridge,
    omni_world_model_t* world_model);

/**
 * @brief Connect drive system
 *
 * @param bridge Bridge instance
 * @param drive_system Drive system to connect
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_hypothalamus_bridge_connect_drives(
    omni_wm_hypothalamus_bridge_t* bridge,
    hypo_drive_system_handle_t* drive_system);

/**
 * @brief Connect homeostasis system
 *
 * @param bridge Bridge instance
 * @param homeostasis Homeostasis system to connect
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_hypothalamus_bridge_connect_homeostasis(
    omni_wm_hypothalamus_bridge_t* bridge,
    hypo_homeostasis_handle_t* homeostasis);

/**
 * @brief Connect circadian system
 *
 * @param bridge Bridge instance
 * @param circadian Circadian system to connect
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_hypothalamus_bridge_connect_circadian(
    omni_wm_hypothalamus_bridge_t* bridge,
    circadian_rhythm_t* circadian);

/**
 * @brief Check if bridge is fully connected
 *
 * @param bridge Bridge to check
 * @return true if world model connected (minimum requirement)
 */
bool omni_wm_hypothalamus_bridge_is_connected(const omni_wm_hypothalamus_bridge_t* bridge);

/* ============================================================================
 * Update API
 * ============================================================================ */

/**
 * @brief Main update cycle
 *
 * WHAT: Process bidirectional information flow
 * WHY:  Called each timestep to sync WM and hypothalamus systems
 * HOW:  Gather hypothalamus effects, compute WM effects, apply both
 *
 * @param bridge Bridge instance
 * @param dt Time delta in seconds
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_hypothalamus_bridge_update(
    omni_wm_hypothalamus_bridge_t* bridge,
    float dt);

/**
 * @brief Notify of drive state change
 *
 * WHAT: Update bridge when drive state changes
 * WHY:  Immediate response to significant homeostatic changes
 * HOW:  Update drive vector, check for mode transitions
 *
 * @param bridge Bridge instance
 * @param drive_type Which drive changed
 * @param new_level New drive level
 * @param new_urgency New urgency
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_hypothalamus_bridge_on_drive_change(
    omni_wm_hypothalamus_bridge_t* bridge,
    uint32_t drive_type,
    float new_level,
    float new_urgency);

/**
 * @brief Notify of circadian phase transition
 *
 * WHAT: Update bridge when circadian phase changes
 * WHY:  Adjust prediction parameters for new phase
 * HOW:  Update circadian state, recalculate modulation factors
 *
 * @param bridge Bridge instance
 * @param new_phase New circadian phase
 * @param cycle_position Position in cycle [0, 1]
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_hypothalamus_bridge_on_phase_change(
    omni_wm_hypothalamus_bridge_t* bridge,
    uint32_t new_phase,
    float cycle_position);

/* ============================================================================
 * Drive Modulation API
 * ============================================================================ */

/**
 * @brief Get prediction priority boost for drive
 *
 * WHAT: Calculate how much to boost predictions related to drive
 * WHY:  Urgent drives should prioritize relevant predictions
 * HOW:  Map urgency to priority boost factor
 *
 * @param bridge Bridge instance
 * @param drive_type Which drive to query
 * @return Priority boost factor [0, 2]
 */
float omni_wm_hypothalamus_bridge_get_priority_boost(
    const omni_wm_hypothalamus_bridge_t* bridge,
    uint32_t drive_type);

/**
 * @brief Get overall drive-based prediction modifier
 *
 * WHAT: Get combined modifier for all active drives
 * WHY:  Single value to modulate WM predictions
 * HOW:  Weighted combination of drive urgencies
 *
 * @param bridge Bridge instance
 * @return Prediction modifier [0.5, 2.0]
 */
float omni_wm_hypothalamus_bridge_get_drive_modifier(
    const omni_wm_hypothalamus_bridge_t* bridge);

/**
 * @brief Check if conservative prediction mode is active
 *
 * @param bridge Bridge instance
 * @return true if in conservative mode
 */
bool omni_wm_hypothalamus_bridge_is_conservative(
    const omni_wm_hypothalamus_bridge_t* bridge);

/* ============================================================================
 * Stress and Arousal API
 * ============================================================================ */

/**
 * @brief Set stress level directly
 *
 * WHAT: Update stress level in bridge
 * WHY:  External stress signals may need to override drive-based
 * HOW:  Update smoothed stress, check conservative threshold
 *
 * @param bridge Bridge instance
 * @param stress_level New stress level [0, 1]
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_hypothalamus_bridge_set_stress(
    omni_wm_hypothalamus_bridge_t* bridge,
    float stress_level);

/**
 * @brief Set arousal level directly
 *
 * WHAT: Update arousal level in bridge
 * WHY:  External arousal signals may need to override circadian
 * HOW:  Update smoothed arousal, adjust prediction parameters
 *
 * @param bridge Bridge instance
 * @param arousal_level New arousal level [0, 1]
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_hypothalamus_bridge_set_arousal(
    omni_wm_hypothalamus_bridge_t* bridge,
    float arousal_level);

/**
 * @brief Get current stress level
 *
 * @param bridge Bridge instance
 * @return Smoothed stress level [0, 1]
 */
float omni_wm_hypothalamus_bridge_get_stress(
    const omni_wm_hypothalamus_bridge_t* bridge);

/**
 * @brief Get current arousal level
 *
 * @param bridge Bridge instance
 * @return Smoothed arousal level [0, 1]
 */
float omni_wm_hypothalamus_bridge_get_arousal(
    const omni_wm_hypothalamus_bridge_t* bridge);

/* ============================================================================
 * Circadian API
 * ============================================================================ */

/**
 * @brief Get circadian modulation factor
 *
 * WHAT: Get time-of-day modulation for specific factor
 * WHY:  Different aspects of WM have different circadian profiles
 * HOW:  Query circadian state for modulation type
 *
 * @param bridge Bridge instance
 * @param modulation_type Type of modulation (0=arousal, 1=learning, 2=consolidation, 3=metabolism)
 * @return Modulation factor [0, 1]
 */
float omni_wm_hypothalamus_bridge_get_circadian_modulation(
    const omni_wm_hypothalamus_bridge_t* bridge,
    uint32_t modulation_type);

/**
 * @brief Get current circadian phase
 *
 * @param bridge Bridge instance
 * @return Current circadian phase
 */
uint32_t omni_wm_hypothalamus_bridge_get_circadian_phase(
    const omni_wm_hypothalamus_bridge_t* bridge);

/**
 * @brief Get sleep pressure
 *
 * @param bridge Bridge instance
 * @return Sleep pressure [0, 1]
 */
float omni_wm_hypothalamus_bridge_get_sleep_pressure(
    const omni_wm_hypothalamus_bridge_t* bridge);

/* ============================================================================
 * Resource Prediction API
 * ============================================================================ */

/**
 * @brief Predict resource availability
 *
 * WHAT: Predict availability of specific resource over horizon
 * WHY:  Hypothalamus can anticipate scarcity
 * HOW:  Use WM to forecast resource trajectory
 *
 * @param bridge Bridge instance
 * @param resource_type Type of resource to predict
 * @param horizon_steps Steps ahead to predict
 * @param prediction_out Output: resource prediction (pre-allocated)
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_hypothalamus_bridge_predict_resource(
    omni_wm_hypothalamus_bridge_t* bridge,
    wm_resource_type_t resource_type,
    uint32_t horizon_steps,
    wm_resource_prediction_t* prediction_out);

/**
 * @brief Generate complete resource forecast
 *
 * WHAT: Predict all tracked resources
 * WHY:  Comprehensive view of future resource state
 * HOW:  Call predict_resource for each type
 *
 * @param bridge Bridge instance
 * @param forecast_out Output: complete forecast (pre-allocated)
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_hypothalamus_bridge_forecast_resources(
    omni_wm_hypothalamus_bridge_t* bridge,
    wm_resource_forecast_t* forecast_out);

/**
 * @brief Update current resource level
 *
 * WHAT: Inform bridge of actual resource state
 * WHY:  Ground predictions in reality
 * HOW:  Update resource tracking, recalibrate predictions
 *
 * @param bridge Bridge instance
 * @param resource_type Resource type
 * @param level Current level [0, 1]
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_hypothalamus_bridge_update_resource(
    omni_wm_hypothalamus_bridge_t* bridge,
    wm_resource_type_t resource_type,
    float level);

/* ============================================================================
 * Reward Prediction API
 * ============================================================================ */

/**
 * @brief Predict reward based on current state and drives
 *
 * WHAT: Use WM to predict reward from action
 * WHY:  Hypothalamus uses reward predictions for planning
 * HOW:  WM forward prediction with drive-weighted reward
 *
 * @param bridge Bridge instance
 * @param action Action to evaluate
 * @param action_dim Action dimensionality
 * @param reward_out Output: predicted reward
 * @param confidence_out Output: prediction confidence
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_hypothalamus_bridge_predict_reward(
    omni_wm_hypothalamus_bridge_t* bridge,
    const float* action,
    uint32_t action_dim,
    float* reward_out,
    float* confidence_out);

/**
 * @brief Update reward prediction with actual outcome
 *
 * WHAT: Train reward prediction from actual reward
 * WHY:  Improve prediction accuracy over time
 * HOW:  Compute error, update running statistics
 *
 * @param bridge Bridge instance
 * @param predicted_reward What was predicted
 * @param actual_reward What actually occurred
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_hypothalamus_bridge_update_reward_prediction(
    omni_wm_hypothalamus_bridge_t* bridge,
    float predicted_reward,
    float actual_reward);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get current effects from Hypothalamus to WM
 *
 * @param bridge Bridge instance
 * @return Pointer to effects structure (do not free)
 */
const hypothalamus_to_omni_wm_effects_t* omni_wm_hypothalamus_bridge_get_hypo_effects(
    const omni_wm_hypothalamus_bridge_t* bridge);

/**
 * @brief Get current effects from WM to Hypothalamus
 *
 * @param bridge Bridge instance
 * @return Pointer to effects structure (do not free)
 */
const omni_wm_to_hypothalamus_effects_t* omni_wm_hypothalamus_bridge_get_wm_effects(
    const omni_wm_hypothalamus_bridge_t* bridge);

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge instance
 * @param stats Output: statistics structure
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_hypothalamus_bridge_get_stats(
    const omni_wm_hypothalamus_bridge_t* bridge,
    omni_wm_hypothalamus_bridge_stats_t* stats);

/**
 * @brief Reset bridge statistics
 *
 * @param bridge Bridge instance
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_hypothalamus_bridge_reset_stats(
    omni_wm_hypothalamus_bridge_t* bridge);

/* ============================================================================
 * Bio-Async API
 * ============================================================================ */

/**
 * @brief Connect bridge to bio-async router
 *
 * @param bridge Bridge instance
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_hypothalamus_bridge_connect_bio_async(
    omni_wm_hypothalamus_bridge_t* bridge);

/**
 * @brief Disconnect bridge from bio-async router
 *
 * @param bridge Bridge instance
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_hypothalamus_bridge_disconnect_bio_async(
    omni_wm_hypothalamus_bridge_t* bridge);

/**
 * @brief Check bio-async connection status
 *
 * @param bridge Bridge instance
 * @return true if connected to bio-async router
 */
bool omni_wm_hypothalamus_bridge_is_bio_async_connected(
    const omni_wm_hypothalamus_bridge_t* bridge);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Get message type name string
 *
 * @param msg_type Message type
 * @return Human-readable message name
 */
const char* omni_wm_hypothalamus_msg_type_to_string(omni_wm_hypothalamus_msg_type_t msg_type);

/**
 * @brief Get resource type name string
 *
 * @param resource_type Resource type
 * @return Human-readable resource name
 */
const char* wm_resource_type_to_string(wm_resource_type_t resource_type);

/**
 * @brief Validate bridge configuration
 *
 * @param config Configuration to validate
 * @return NIMCP_SUCCESS or error code describing issue
 */
nimcp_error_t omni_wm_hypothalamus_bridge_validate_config(
    const omni_wm_hypothalamus_bridge_config_t* config);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_OMNI_WM_HYPOTHALAMUS_BRIDGE_H */
