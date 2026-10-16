/**
 * @file nimcp_dragonfly_prediction.h
 * @brief Trajectory Prediction and Evasion Detection Module
 *
 * WHAT: Predicts target trajectory and detects evasive maneuvers
 * WHY:  Dragonflies predict prey movement for successful interception
 * HOW:  Interacting Multiple Model (IMM) filter with evasion classification
 *
 * BIOLOGICAL REFERENCE:
 * - Mischiati et al. (2015) "Internal models direct dragonfly interception steering"
 * - Lin & Leonardo (2017) "Heuristic rules underlying dragonfly prey selection"
 *
 * KEY FEATURES:
 * - Multiple motion models: Constant Velocity, Constant Accel, Singer, Jink, Weave
 * - Interacting Multiple Model (IMM) filter for model switching
 * - Evasion detection and classification
 * - Predictive facilitation (gain boost along predicted trajectory)
 * - Forward and inverse models for motor control
 *
 * @author NIMCP Team
 * @date 2024-12-27
 */

#ifndef NIMCP_DRAGONFLY_PREDICTION_H
#define NIMCP_DRAGONFLY_PREDICTION_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

//=============================================================================
// Forward Declarations
//=============================================================================

typedef struct dragonfly_predictor_s dragonfly_predictor_t;

//=============================================================================
// Constants
//=============================================================================

#define PREDICTOR_MAX_MODELS 6       /**< Maximum motion models in IMM */
#define PREDICTOR_MAX_TRAJECTORY 64  /**< Maximum trajectory points */
#define PREDICTOR_STATE_DIM 9        /**< x,y,z, vx,vy,vz, ax,ay,az */

//=============================================================================
// Type Definitions
//=============================================================================

/**
 * @brief Motion model types for trajectory prediction
 */
typedef enum {
    PRED_MODEL_CV,            /**< Constant Velocity (straight-line) */
    PRED_MODEL_CA,            /**< Constant Acceleration (smooth curves) */
    PRED_MODEL_SINGER,        /**< Singer (maneuvering target, random accel) */
    PRED_MODEL_JINK,          /**< Evasive jinking (high-freq direction change) */
    PRED_MODEL_WEAVE,         /**< Evasive weaving (S-pattern) */
    PRED_MODEL_SPIRAL         /**< Evasive spiral (curved escape) */
} prediction_motion_model_t;

/**
 * @brief Evasion type classification
 */
typedef enum {
    EVASION_NONE,             /**< Normal (non-evasive) motion */
    EVASION_JINK,             /**< Random direction changes */
    EVASION_BREAK,            /**< Sudden deceleration */
    EVASION_WEAVE,            /**< S-pattern oscillation */
    EVASION_SPIRAL,           /**< Curved/spiral escape */
    EVASION_COMBINED          /**< Multiple evasion types */
} evasion_type_t;

/**
 * @brief Predicted state at a future time point
 */
typedef struct {
    float position[3];        /**< Predicted position (x, y, z) */
    float velocity[3];        /**< Predicted velocity (vx, vy, vz) */
    float acceleration[3];    /**< Predicted acceleration (ax, ay, az) */
    float confidence;         /**< Prediction confidence [0,1] */
    float time_offset_ms;     /**< Time from now (milliseconds) */
    float covariance[9];      /**< Position covariance (diagonal: xx,yy,zz,vv,vv,vv,aa,aa,aa) */
} predicted_state_t;

/**
 * @brief Evasion state and classification
 */
typedef struct {
    evasion_type_t current_type;           /**< Current evasion classification */
    float model_probabilities[PREDICTOR_MAX_MODELS];  /**< IMM model weights */
    float maneuver_intensity;              /**< Evasion intensity [0,1] */
    float prediction_uncertainty;          /**< Grows with evasion */
    uint64_t last_maneuver_us;             /**< Last maneuver timestamp */
    uint32_t maneuver_count;               /**< Total maneuvers detected */
    float jink_frequency_hz;               /**< Detected jink frequency */
    float weave_amplitude;                 /**< Detected weave amplitude */
} evasion_state_t;

/**
 * @brief Complete trajectory prediction
 */
typedef struct {
    predicted_state_t* trajectory;         /**< Array of future states */
    uint32_t num_points;                   /**< Number of trajectory points */
    evasion_state_t evasion;               /**< Evasion state */
    float optimal_intercept_time_ms;       /**< Best interception time */
    float optimal_intercept_point[3];      /**< Best interception location */
    float prediction_horizon_ms;           /**< How far ahead prediction extends */
    uint64_t timestamp_us;                 /**< When prediction was computed */
} trajectory_prediction_t;

/**
 * @brief Predictor configuration
 */
typedef struct {
    /* IMM configuration */
    bool enable_imm;                       /**< Use Interacting Multiple Model filter */
    uint32_t num_models;                   /**< Number of models (1-6) */
    prediction_motion_model_t models[PREDICTOR_MAX_MODELS];  /**< Active models */
    float model_transition_prob;           /**< Model switching probability */

    /* Prediction parameters */
    float max_prediction_ms;               /**< Maximum lookahead time */
    uint32_t prediction_steps;             /**< Number of future states */
    float process_noise;                   /**< Motion model uncertainty (Q) */
    float measurement_noise;               /**< Observation uncertainty (R) */

    /* Evasion detection thresholds */
    float jink_accel_threshold;            /**< Accel spike to detect jink (m/s^2) */
    float break_decel_threshold;           /**< Decel to detect break (m/s^2) */
    float weave_frequency_min;             /**< Min weave frequency (Hz) */
    float weave_frequency_max;             /**< Max weave frequency (Hz) */
    float model_switch_threshold;          /**< Probability to switch models */

    /* Facilitation */
    float facilitation_width;              /**< Angular width of facilitation cone */
    float facilitation_gain;               /**< Gain boost in prediction direction */
} prediction_config_t;

/**
 * @brief Predictor statistics
 */
typedef struct {
    uint64_t predictions_made;             /**< Total predictions computed */
    uint64_t evasions_detected;            /**< Total evasion events */
    uint64_t model_switches;               /**< IMM model switches */
    float avg_prediction_error;            /**< Average prediction error (meters) */
    float max_prediction_error;            /**< Maximum prediction error */
    float avg_prediction_time_us;          /**< Average compute time */
    float model_accuracy[PREDICTOR_MAX_MODELS];  /**< Per-model accuracy */
} prediction_stats_t;

//=============================================================================
// Configuration Functions
//=============================================================================

/**
 * @brief Get default predictor configuration
 *
 * @return Default configuration (single CV model)
 */
prediction_config_t prediction_default_config(void);

/**
 * @brief Get configuration with full IMM filter
 *
 * @return Configuration with all 6 motion models
 */
prediction_config_t prediction_imm_config(void);

/**
 * @brief Validate predictor configuration
 *
 * @param config Configuration to validate
 * @return true if valid, false otherwise
 */
bool prediction_validate_config(const prediction_config_t* config);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create a trajectory predictor
 *
 * @param config Configuration (NULL for defaults)
 * @return Predictor handle or NULL on failure
 */
dragonfly_predictor_t* dragonfly_predictor_create(const prediction_config_t* config);

/**
 * @brief Destroy a predictor and free resources
 *
 * @param pred Predictor handle (NULL-safe)
 */
void dragonfly_predictor_destroy(dragonfly_predictor_t* pred);

/**
 * @brief Reset predictor state
 *
 * @param pred Predictor handle
 * @return 0 on success, -1 on error
 */
int dragonfly_predictor_reset(dragonfly_predictor_t* pred);

//=============================================================================
// Core Prediction Functions
//=============================================================================

/**
 * @brief Update predictor with new observation
 *
 * WHAT: Process new position/velocity observation
 * WHY:  Updates internal state for prediction
 *
 * @param pred Predictor handle
 * @param position Observed position [3]
 * @param velocity Observed velocity [3] (can be NULL)
 * @param dt Time delta since last update (seconds)
 * @return 0 on success, -1 on error
 */
int dragonfly_predictor_update(
    dragonfly_predictor_t* pred,
    const float position[3],
    const float velocity[3],
    float dt
);

/**
 * @brief Compute trajectory prediction
 *
 * WHAT: Generate predicted trajectory with evasion analysis
 * WHY:  Main output for interception planning
 *
 * @param pred Predictor handle
 * @param lookahead_ms Maximum lookahead time (ms)
 * @param prediction Output: trajectory prediction (caller must allocate trajectory array)
 * @return 0 on success, -1 on error
 */
int dragonfly_predictor_predict(
    dragonfly_predictor_t* pred,
    float lookahead_ms,
    trajectory_prediction_t* prediction
);

/**
 * @brief Get predicted state at specific time
 *
 * @param pred Predictor handle
 * @param time_offset_ms Time ahead (ms)
 * @param state Output: predicted state
 * @return 0 on success, -1 on error
 */
int dragonfly_predictor_get_state_at(
    dragonfly_predictor_t* pred,
    float time_offset_ms,
    predicted_state_t* state
);

//=============================================================================
// Evasion Detection Functions
//=============================================================================

/**
 * @brief Get current evasion state
 *
 * @param pred Predictor handle
 * @param evasion Output: evasion state
 * @return 0 on success, -1 on error
 */
int dragonfly_predictor_get_evasion(
    const dragonfly_predictor_t* pred,
    evasion_state_t* evasion
);

/**
 * @brief Detect evasion from acceleration
 *
 * @param pred Predictor handle
 * @param observed_accel Observed acceleration [3]
 * @return Detected evasion type
 */
evasion_type_t dragonfly_predictor_detect_evasion(
    dragonfly_predictor_t* pred,
    const float observed_accel[3]
);

/**
 * @brief Get evasion intensity
 *
 * @param pred Predictor handle
 * @return Intensity [0,1], 0 if no evasion
 */
float dragonfly_predictor_get_evasion_intensity(const dragonfly_predictor_t* pred);

//=============================================================================
// Forward and Inverse Models
//=============================================================================

/**
 * @brief Forward model: predict state from current state + action
 *
 * @param pred Predictor handle
 * @param current_state Current state [9]: pos, vel, accel
 * @param action Intended action/movement [3]
 * @param dt Time delta
 * @param predicted_state Output: predicted state [9]
 * @return 0 on success, -1 on error
 */
int dragonfly_forward_model(
    const dragonfly_predictor_t* pred,
    const float current_state[9],
    const float action[3],
    float dt,
    float predicted_state[9]
);

/**
 * @brief Inverse model: compute action for desired state
 *
 * @param pred Predictor handle
 * @param current_state Current state [9]
 * @param desired_state Desired state [9]
 * @param dt Time delta
 * @param required_action Output: required action [3]
 * @return 0 on success, -1 on error
 */
int dragonfly_inverse_model(
    const dragonfly_predictor_t* pred,
    const float current_state[9],
    const float desired_state[9],
    float dt,
    float required_action[3]
);

//=============================================================================
// IMM Filter Functions
//=============================================================================

/**
 * @brief Get model probabilities (IMM filter weights)
 *
 * @param pred Predictor handle
 * @param probabilities Output: array of probabilities [num_models]
 * @param num_models Number of models
 * @return 0 on success, -1 on error
 */
int dragonfly_predictor_get_model_probabilities(
    const dragonfly_predictor_t* pred,
    float* probabilities,
    uint32_t num_models
);

/**
 * @brief Get most likely motion model
 *
 * @param pred Predictor handle
 * @return Most probable model, or PRED_MODEL_CV on error
 */
prediction_motion_model_t dragonfly_predictor_get_dominant_model(
    const dragonfly_predictor_t* pred
);

//=============================================================================
// Facilitation Functions
//=============================================================================

/**
 * @brief Get facilitation gain for a direction
 *
 * @param pred Predictor handle
 * @param direction Query direction (radians)
 * @return Gain multiplier [1.0, 1.0 + facilitation_gain]
 */
float dragonfly_predictor_get_facilitation_gain(
    const dragonfly_predictor_t* pred,
    float direction
);

/**
 * @brief Get predicted direction for facilitation
 *
 * @param pred Predictor handle
 * @return Predicted direction (radians)
 */
float dragonfly_predictor_get_predicted_direction(const dragonfly_predictor_t* pred);

//=============================================================================
// Statistics and Configuration
//=============================================================================

/**
 * @brief Get predictor statistics
 *
 * @param pred Predictor handle
 * @param stats Output: statistics structure
 * @return 0 on success, -1 on error
 */
int dragonfly_predictor_get_stats(
    const dragonfly_predictor_t* pred,
    prediction_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param pred Predictor handle
 * @return 0 on success, -1 on error
 */
int dragonfly_predictor_reset_stats(dragonfly_predictor_t* pred);

/**
 * @brief Update configuration
 *
 * @param pred Predictor handle
 * @param config New configuration
 * @return 0 on success, -1 on error
 */
int dragonfly_predictor_set_config(
    dragonfly_predictor_t* pred,
    const prediction_config_t* config
);

/**
 * @brief Get current configuration
 *
 * @param pred Predictor handle
 * @param config Output: configuration
 * @return 0 on success, -1 on error
 */
int dragonfly_predictor_get_config(
    const dragonfly_predictor_t* pred,
    prediction_config_t* config
);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get evasion type name
 *
 * @param type Evasion type
 * @return Human-readable name
 */
const char* dragonfly_evasion_name(evasion_type_t type);

/**
 * @brief Get motion model name
 *
 * @param model Motion model
 * @return Human-readable name
 */
const char* dragonfly_model_name(prediction_motion_model_t model);

/**
 * @brief Allocate trajectory array
 *
 * @param num_points Number of trajectory points
 * @return Allocated array or NULL on failure
 */
predicted_state_t* dragonfly_trajectory_alloc(uint32_t num_points);

/**
 * @brief Free trajectory array
 *
 * @param trajectory Array to free (NULL-safe)
 */
void dragonfly_trajectory_free(predicted_state_t* trajectory);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_DRAGONFLY_PREDICTION_H */
