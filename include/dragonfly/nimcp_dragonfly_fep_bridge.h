/**
 * @file nimcp_dragonfly_fep_bridge.h
 * @brief Dragonfly-to-Free Energy Principle Integration Bridge
 *
 * WHAT: Bridges dragonfly tracking to Free Energy Principle framework
 * WHY:  Enable prediction error minimization for optimal interception
 * HOW:  Generative model of prey motion, active inference for pursuit
 *
 * BIOLOGICAL BASIS:
 * - Dragonflies minimize prediction error during pursuit
 * - Internal model predicts prey trajectory
 * - Active inference drives motor commands to reduce surprise
 * - Precision weighting adapts based on reliability of observations
 *
 * @author NIMCP Development Team
 * @date 2025-12-28
 */

#ifndef NIMCP_DRAGONFLY_FEP_BRIDGE_H
#define NIMCP_DRAGONFLY_FEP_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
struct dragonfly_system_s;
typedef struct dragonfly_system_s dragonfly_system_t;

//=============================================================================
// Constants
//=============================================================================

#define DRAGONFLY_FEP_MAX_STATE_DIM 32      /**< Max state dimension */
#define DRAGONFLY_FEP_MAX_OBS_DIM 64        /**< Max observation dimension */
#define DRAGONFLY_FEP_DEFAULT_PRECISION 1.0f /**< Default precision */

//=============================================================================
// Enums
//=============================================================================

/**
 * @brief Generative model types for prey motion
 */
typedef enum {
    FEP_MODEL_LINEAR = 0,           /**< Linear motion model */
    FEP_MODEL_CONSTANT_VELOCITY,    /**< Constant velocity model */
    FEP_MODEL_CONSTANT_ACCELERATION,/**< Constant acceleration model */
    FEP_MODEL_MANEUVERING,          /**< Maneuvering target model */
    FEP_MODEL_EVASIVE               /**< Evasive maneuver model */
} dragonfly_fep_model_t;

/**
 * @brief Active inference action types
 */
typedef enum {
    FEP_ACTION_OBSERVE = 0,         /**< Pure observation */
    FEP_ACTION_PURSUIT,             /**< Active pursuit */
    FEP_ACTION_INTERCEPT,           /**< Active interception */
    FEP_ACTION_PREDICT              /**< Predictive saccade */
} dragonfly_fep_action_t;

/**
 * @brief Precision weighting mode
 */
typedef enum {
    FEP_PRECISION_FIXED = 0,        /**< Fixed precision weights */
    FEP_PRECISION_ADAPTIVE,         /**< Adaptive based on reliability */
    FEP_PRECISION_HIERARCHICAL      /**< Hierarchical precision */
} dragonfly_fep_precision_mode_t;

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Prediction error components
 */
typedef struct {
    float sensory_error;            /**< Sensory prediction error */
    float proprioceptive_error;     /**< Proprioceptive prediction error */
    float model_error;              /**< Internal model prediction error */
    float total_free_energy;        /**< Total variational free energy */
    float precision_weighted_error; /**< Precision-weighted error */
} dragonfly_fep_errors_t;

/**
 * @brief Active inference state
 */
typedef struct {
    float beliefs[DRAGONFLY_FEP_MAX_STATE_DIM];     /**< Current beliefs about state */
    float precision[DRAGONFLY_FEP_MAX_STATE_DIM];   /**< Precision of beliefs */
    uint32_t state_dim;                              /**< State dimension */
    float expected_free_energy;                      /**< Expected FE for action */
    dragonfly_fep_action_t current_action;           /**< Current action */
} dragonfly_fep_inference_t;

/**
 * @brief Configuration
 */
typedef struct {
    /* Model settings */
    dragonfly_fep_model_t default_model;        /**< Default generative model */
    bool auto_model_selection;                  /**< Auto-select best model */
    float model_evidence_threshold;             /**< Threshold for model switching */

    /* Precision settings */
    dragonfly_fep_precision_mode_t precision_mode; /**< Precision weighting mode */
    float sensory_precision;                    /**< Sensory precision weight */
    float proprioceptive_precision;             /**< Proprioceptive precision */
    float prior_precision;                      /**< Prior precision weight */

    /* Inference settings */
    float learning_rate;                        /**< Belief update rate */
    uint32_t inference_steps;                   /**< Steps per update cycle */
    float action_precision;                     /**< Precision of action selection */

    /* Integration */
    bool use_tsdn_predictions;                  /**< Use TSDN for predictions */
    bool use_tracking_observations;             /**< Use tracker observations */
    float prediction_horizon_ms;                /**< How far ahead to predict */
} dragonfly_fep_config_t;

/**
 * @brief Statistics
 */
typedef struct {
    uint64_t inference_steps_total;
    uint64_t predictions_made;
    uint64_t model_switches;
    float avg_prediction_error;
    float avg_free_energy;
    float avg_precision;
    float model_evidence[5];                    /**< Evidence for each model */
} dragonfly_fep_stats_t;

/**
 * @brief FEP bridge handle
 */
typedef struct dragonfly_fep_bridge_s dragonfly_fep_bridge_t;

//=============================================================================
// Configuration
//=============================================================================

int dragonfly_fep_bridge_default_config(dragonfly_fep_config_t* config);
int dragonfly_fep_bridge_validate_config(const dragonfly_fep_config_t* config);

//=============================================================================
// Lifecycle
//=============================================================================

dragonfly_fep_bridge_t* dragonfly_fep_bridge_create(
    dragonfly_system_t* dragonfly,
    void* fep_system,
    const dragonfly_fep_config_t* config
);

void dragonfly_fep_bridge_destroy(dragonfly_fep_bridge_t* bridge);
int dragonfly_fep_bridge_reset(dragonfly_fep_bridge_t* bridge);

//=============================================================================
// Prediction Error
//=============================================================================

int dragonfly_fep_compute_errors(
    dragonfly_fep_bridge_t* bridge,
    const float* observations,
    uint32_t obs_dim,
    dragonfly_fep_errors_t* errors
);

float dragonfly_fep_get_free_energy(const dragonfly_fep_bridge_t* bridge);
float dragonfly_fep_get_surprise(const dragonfly_fep_bridge_t* bridge);

int dragonfly_fep_update_precision(
    dragonfly_fep_bridge_t* bridge,
    float sensory_reliability,
    float proprioceptive_reliability
);

//=============================================================================
// Active Inference
//=============================================================================

int dragonfly_fep_infer_state(
    dragonfly_fep_bridge_t* bridge,
    const float* observations,
    uint32_t obs_dim,
    dragonfly_fep_inference_t* inference
);

int dragonfly_fep_select_action(
    dragonfly_fep_bridge_t* bridge,
    dragonfly_fep_action_t* action,
    float* action_value
);

int dragonfly_fep_apply_action(
    dragonfly_fep_bridge_t* bridge,
    dragonfly_fep_action_t action
);

float dragonfly_fep_expected_free_energy(
    const dragonfly_fep_bridge_t* bridge,
    dragonfly_fep_action_t action
);

//=============================================================================
// Generative Model
//=============================================================================

int dragonfly_fep_set_model(
    dragonfly_fep_bridge_t* bridge,
    dragonfly_fep_model_t model
);

dragonfly_fep_model_t dragonfly_fep_get_best_model(
    const dragonfly_fep_bridge_t* bridge
);

float dragonfly_fep_get_model_evidence(
    const dragonfly_fep_bridge_t* bridge,
    dragonfly_fep_model_t model
);

int dragonfly_fep_predict(
    dragonfly_fep_bridge_t* bridge,
    float horizon_ms,
    float* predicted_state,
    uint32_t state_dim
);

//=============================================================================
// Integration
//=============================================================================

int dragonfly_fep_connect_dragonfly(
    dragonfly_fep_bridge_t* bridge,
    dragonfly_system_t* dragonfly
);

int dragonfly_fep_connect_system(
    dragonfly_fep_bridge_t* bridge,
    void* fep_system
);

int dragonfly_fep_update(dragonfly_fep_bridge_t* bridge, float dt_ms);

int dragonfly_fep_sync_with_tracker(dragonfly_fep_bridge_t* bridge);
int dragonfly_fep_sync_with_tsdn(dragonfly_fep_bridge_t* bridge);

//=============================================================================
// Statistics
//=============================================================================

int dragonfly_fep_bridge_get_stats(
    const dragonfly_fep_bridge_t* bridge,
    dragonfly_fep_stats_t* stats
);

int dragonfly_fep_bridge_reset_stats(dragonfly_fep_bridge_t* bridge);

//=============================================================================
// Utility
//=============================================================================

const char* dragonfly_fep_model_name(dragonfly_fep_model_t model);
const char* dragonfly_fep_action_name(dragonfly_fep_action_t action);
const char* dragonfly_fep_precision_mode_name(dragonfly_fep_precision_mode_t mode);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_DRAGONFLY_FEP_BRIDGE_H */
