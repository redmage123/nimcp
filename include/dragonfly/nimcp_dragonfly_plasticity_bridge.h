/**
 * @file nimcp_dragonfly_plasticity_bridge.h
 * @brief Plasticity-Dragonfly Integration Bridge
 *
 * BIOLOGICAL REFERENCE:
 * Dragonfly hunting circuits exhibit plasticity through experience:
 * - TSDN tuning curves adapt to frequently encountered target directions
 * - Prediction models improve with hunting experience
 * - Navigation gains optimize based on success/failure feedback
 *
 * WHAT: Enables learning-based adaptation of hunting circuits
 * WHY:  Improves hunting success through experience
 * HOW:  Reward-modulated plasticity on TSDN and prediction parameters
 *
 * @author NIMCP Team
 * @date 2024-12-29
 */

#ifndef NIMCP_DRAGONFLY_PLASTICITY_BRIDGE_H
#define NIMCP_DRAGONFLY_PLASTICITY_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "dragonfly/nimcp_dragonfly.h"
#include "dragonfly/nimcp_dragonfly_tsdn.h"

//=============================================================================
// Forward Declarations
//=============================================================================

typedef struct dragonfly_plasticity_bridge_s* dragonfly_plasticity_bridge_t;
typedef struct plasticity_coordinator_s* plasticity_coordinator_t;

//=============================================================================
// Constants
//=============================================================================

#define PLASTICITY_TSDN_NEURONS 16    /**< Number of TSDN neurons */
#define PLASTICITY_IMM_MODELS 6       /**< Number of IMM motion models */

//=============================================================================
// Type Definitions
//=============================================================================

/**
 * @brief Learning signal type
 */
typedef enum {
    LEARN_REWARD,           /**< Positive reward (success) */
    LEARN_PUNISHMENT,       /**< Negative reward (failure) */
    LEARN_PREDICTION_ERROR, /**< Prediction error signal */
    LEARN_NOVELTY,          /**< Novel stimulus */
    LEARN_SURPRISE          /**< Unexpected outcome */
} learning_signal_t;

/**
 * @brief Plasticity target
 */
typedef enum {
    PLAST_TARGET_TSDN_TUNING,     /**< TSDN direction tuning curves */
    PLAST_TARGET_TSDN_GAIN,       /**< TSDN response gain */
    PLAST_TARGET_IMM_PRIORS,      /**< IMM model prior probabilities */
    PLAST_TARGET_NAV_GAIN,        /**< Navigation gain (PN) */
    PLAST_TARGET_LEAD_FACTOR,     /**< Lead time factor */
    PLAST_TARGET_ATTENTION        /**< Attention thresholds */
} plasticity_target_t;

/**
 * @brief TSDN tuning adaptation
 */
typedef struct {
    float preferred_direction[PLASTICITY_TSDN_NEURONS]; /**< Preferred directions */
    float tuning_width[PLASTICITY_TSDN_NEURONS];        /**< Tuning curve widths */
    float gain[PLASTICITY_TSDN_NEURONS];                /**< Response gains */
    float baseline[PLASTICITY_TSDN_NEURONS];            /**< Baseline firing */
} tsdn_tuning_state_t;

/**
 * @brief IMM model adaptation
 */
typedef struct {
    float model_priors[PLASTICITY_IMM_MODELS];          /**< Prior probabilities */
    float transition_matrix[PLASTICITY_IMM_MODELS][PLASTICITY_IMM_MODELS]; /**< Transitions */
    float model_success_rate[PLASTICITY_IMM_MODELS];    /**< Success per model */
} imm_adaptation_state_t;

/**
 * @brief Interception parameter adaptation
 */
typedef struct {
    float nav_gain;               /**< Proportional navigation gain */
    float lead_time_factor;       /**< Lead pursuit factor */
    float pursuit_aggressiveness; /**< Pursuit aggressiveness [0,1] */
    float abort_threshold;        /**< Confidence to abort */
} intercept_adaptation_state_t;

/**
 * @brief Learning event for plasticity
 */
typedef struct {
    learning_signal_t signal;     /**< Type of learning signal */
    float magnitude;              /**< Signal magnitude [0,1] */

    /* Context */
    float target_direction_rad;   /**< Target direction at event */
    float target_speed;           /**< Target speed at event */
    uint32_t motion_model;        /**< Active motion model */

    /* Outcome */
    float prediction_error;       /**< Prediction error magnitude */
    float miss_distance;          /**< Miss distance (if applicable) */
    bool hunt_success;            /**< Hunt success flag */

    /* Timing */
    uint64_t timestamp_us;        /**< Event timestamp */
} plasticity_event_t;

/**
 * @brief Plasticity bridge configuration
 */
typedef struct {
    /* Learning rates */
    float tsdn_learning_rate;     /**< TSDN adaptation rate */
    float imm_learning_rate;      /**< IMM adaptation rate */
    float intercept_learning_rate;/**< Interception param rate */

    /* Eligibility traces */
    float eligibility_decay;      /**< Trace decay time constant */
    bool enable_eligibility;      /**< Enable eligibility traces */

    /* Reward modulation */
    float success_reward;         /**< Reward for successful hunt */
    float failure_punishment;     /**< Punishment for failed hunt */
    float prediction_error_weight;/**< Weight of prediction errors */

    /* Constraints */
    float min_nav_gain;           /**< Minimum navigation gain */
    float max_nav_gain;           /**< Maximum navigation gain */
    float min_tuning_width;       /**< Minimum tuning width */
    float max_tuning_width;       /**< Maximum tuning width */

    /* Homeostasis */
    bool enable_homeostasis;      /**< Enable homeostatic regulation */
    float homeostasis_rate;       /**< Homeostasis rate */
    float target_activity;        /**< Target activity level */

    /* Metaplasticity */
    bool enable_metaplasticity;   /**< Enable metaplasticity */
    float metaplasticity_rate;    /**< Metaplasticity rate */
} dragonfly_plasticity_config_t;

/**
 * @brief Plasticity bridge state
 */
typedef struct {
    /* Adaptation states */
    tsdn_tuning_state_t tsdn_state;
    imm_adaptation_state_t imm_state;
    intercept_adaptation_state_t intercept_state;

    /* Eligibility traces */
    float tsdn_eligibility[PLASTICITY_TSDN_NEURONS];
    float imm_eligibility[PLASTICITY_IMM_MODELS];

    /* Learning progress */
    uint64_t learning_events;     /**< Total learning events */
    float cumulative_reward;      /**< Cumulative reward signal */
    float avg_prediction_error;   /**< Average prediction error */
} dragonfly_plasticity_state_t;

/**
 * @brief Plasticity bridge statistics
 */
typedef struct {
    uint64_t reward_events;       /**< Total reward events */
    uint64_t punishment_events;   /**< Total punishment events */
    uint64_t tsdn_updates;        /**< TSDN adaptation updates */
    uint64_t imm_updates;         /**< IMM adaptation updates */
    uint64_t intercept_updates;   /**< Interception param updates */
    float nav_gain_change;        /**< Total nav gain change */
    float avg_tuning_shift;       /**< Average tuning curve shift */
    float learning_progress;      /**< Overall learning progress [0,1] */
} dragonfly_plasticity_stats_t;

//=============================================================================
// Configuration Functions
//=============================================================================

/**
 * @brief Get default plasticity bridge configuration
 */
dragonfly_plasticity_config_t dragonfly_plasticity_default_config(void);

/**
 * @brief Validate plasticity bridge configuration
 */
bool dragonfly_plasticity_validate_config(const dragonfly_plasticity_config_t* config);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create plasticity bridge
 */
dragonfly_plasticity_bridge_t dragonfly_plasticity_bridge_create(
    const dragonfly_plasticity_config_t* config
);

/**
 * @brief Destroy plasticity bridge
 */
void dragonfly_plasticity_bridge_destroy(dragonfly_plasticity_bridge_t bridge);

/**
 * @brief Connect to systems
 */
int dragonfly_plasticity_bridge_connect(
    dragonfly_plasticity_bridge_t bridge,
    dragonfly_system_t* dragonfly,
    plasticity_coordinator_t plasticity
);

/**
 * @brief Disconnect from systems
 */
int dragonfly_plasticity_bridge_disconnect(dragonfly_plasticity_bridge_t bridge);

//=============================================================================
// Learning Functions
//=============================================================================

/**
 * @brief Process learning event
 */
int dragonfly_plasticity_learn(
    dragonfly_plasticity_bridge_t bridge,
    const plasticity_event_t* event
);

/**
 * @brief Signal hunt success (reward)
 */
int dragonfly_plasticity_reward(
    dragonfly_plasticity_bridge_t bridge,
    float target_direction,
    float target_speed,
    uint32_t motion_model,
    float miss_distance
);

/**
 * @brief Signal hunt failure (punishment)
 */
int dragonfly_plasticity_punish(
    dragonfly_plasticity_bridge_t bridge,
    float target_direction,
    float target_speed,
    uint32_t motion_model,
    float miss_distance,
    const char* reason
);

/**
 * @brief Update eligibility traces
 */
int dragonfly_plasticity_update_eligibility(
    dragonfly_plasticity_bridge_t bridge,
    float target_direction,
    uint32_t motion_model,
    float dt_s
);

//=============================================================================
// Adaptation Functions
//=============================================================================

/**
 * @brief Apply current adaptations to dragonfly system
 */
int dragonfly_plasticity_apply_adaptations(dragonfly_plasticity_bridge_t bridge);

/**
 * @brief Reset adaptations to defaults
 */
int dragonfly_plasticity_reset_adaptations(dragonfly_plasticity_bridge_t bridge);

/**
 * @brief Get adapted TSDN parameters
 */
int dragonfly_plasticity_get_tsdn_params(
    const dragonfly_plasticity_bridge_t bridge,
    tsdn_tuning_state_t* params
);

/**
 * @brief Get adapted IMM parameters
 */
int dragonfly_plasticity_get_imm_params(
    const dragonfly_plasticity_bridge_t bridge,
    imm_adaptation_state_t* params
);

/**
 * @brief Get adapted interception parameters
 */
int dragonfly_plasticity_get_intercept_params(
    const dragonfly_plasticity_bridge_t bridge,
    intercept_adaptation_state_t* params
);

//=============================================================================
// Query Functions
//=============================================================================

/**
 * @brief Get plasticity bridge state
 */
int dragonfly_plasticity_get_state(
    const dragonfly_plasticity_bridge_t bridge,
    dragonfly_plasticity_state_t* state
);

/**
 * @brief Get plasticity bridge statistics
 */
int dragonfly_plasticity_get_stats(
    const dragonfly_plasticity_bridge_t bridge,
    dragonfly_plasticity_stats_t* stats
);

/**
 * @brief Get learning signal name
 */
const char* dragonfly_learning_signal_name(learning_signal_t signal);

/**
 * @brief Get plasticity target name
 */
const char* dragonfly_plasticity_target_name(plasticity_target_t target);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_DRAGONFLY_PLASTICITY_BRIDGE_H */
