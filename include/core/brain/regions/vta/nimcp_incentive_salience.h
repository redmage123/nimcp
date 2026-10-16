/**
 * @file nimcp_incentive_salience.h
 * @brief Incentive salience ("wanting") computation for VTA
 * @date 2026-01-11
 *
 * Models the incentive salience system that transforms reward representations
 * into motivation. Key distinction from hedonic impact ("liking"):
 * - Wanting: Motivation to pursue reward (DA-dependent)
 * - Liking: Hedonic pleasure from reward (opioid-dependent)
 *
 * Supports:
 * - Goal-directed wanting
 * - Effort-cost computation
 * - Urgency and temporal factors
 * - Cue-triggered wanting
 */

#ifndef NIMCP_INCENTIVE_SALIENCE_H
#define NIMCP_INCENTIVE_SALIENCE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/*=============================================================================
 * Constants
 *===========================================================================*/

#define SALIENCE_MAX_GOALS         16       /* Max concurrent goals */
#define SALIENCE_MAX_CUES          32       /* Max cue-reward associations */
#define SALIENCE_DEFAULT_EFFORT_K  0.5f     /* Effort cost parameter */
#define SALIENCE_DEFAULT_DELAY_K   0.1f     /* Delay discounting parameter */
#define SALIENCE_DEFAULT_DA_GAIN   1.0f     /* DA -> wanting gain */

/*=============================================================================
 * Enumerations
 *===========================================================================*/

/**
 * @brief Motivation state
 */
typedef enum {
    MOTIVATION_NONE = 0,          /**< No motivation */
    MOTIVATION_LOW,               /**< Low motivation */
    MOTIVATION_MODERATE,          /**< Moderate motivation */
    MOTIVATION_HIGH,              /**< High motivation */
    MOTIVATION_INTENSE            /**< Intense craving */
} nimcp_motivation_level_t;

/**
 * @brief Goal type
 */
typedef enum {
    GOAL_PRIMARY = 0,             /**< Primary reward (food, water) */
    GOAL_SECONDARY,               /**< Learned reward (money) */
    GOAL_SOCIAL,                  /**< Social reward */
    GOAL_INTRINSIC,               /**< Intrinsic motivation */
    GOAL_AVOIDANCE                /**< Avoid aversive outcome */
} nimcp_goal_type_t;

/**
 * @brief Effort type
 */
typedef enum {
    EFFORT_PHYSICAL = 0,          /**< Physical effort */
    EFFORT_COGNITIVE,             /**< Mental effort */
    EFFORT_SOCIAL,                /**< Social effort */
    EFFORT_TEMPORAL               /**< Waiting/patience */
} nimcp_effort_type_t;

/*=============================================================================
 * Structures
 *===========================================================================*/

/**
 * @brief Goal representation
 */
typedef struct {
    uint32_t id;
    nimcp_goal_type_t type;
    float value;                  /**< Reward value */
    float probability;            /**< P(success) */
    float distance;               /**< Progress to goal [0-1] */
    float effort_required;        /**< Total effort needed */
    float time_to_reward;         /**< Expected delay (ms) */
    float urgency;                /**< Time pressure [0-1] */
    bool active;                  /**< Currently pursued */
} nimcp_goal_t;

/**
 * @brief Cue-triggered wanting
 */
typedef struct {
    uint32_t id;
    float salience;               /**< Learned salience [0-1] */
    float da_boost;               /**< DA boost when present */
    float decay_rate;             /**< Salience decay */
    uint32_t association_count;   /**< Learning trials */
    bool present;                 /**< Currently active */
} nimcp_salience_cue_t;

/**
 * @brief Effort-cost parameters
 */
typedef struct {
    float physical_cost_k;        /**< Physical effort sensitivity */
    float cognitive_cost_k;       /**< Cognitive effort sensitivity */
    float delay_discount_k;       /**< Delay discounting rate */
    float probability_weight;     /**< Probability weighting */
    float loss_aversion;          /**< Loss vs gain asymmetry */
} nimcp_effort_params_t;

/**
 * @brief Incentive salience state
 */
typedef struct {
    float wanting;                /**< Current wanting [0-1] */
    float liking;                 /**< Current liking [0-1] */
    float motivation;             /**< Overall motivation [0-1] */
    float vigor;                  /**< Response vigor [0-1] */
    nimcp_motivation_level_t level;
} nimcp_salience_state_t;

/**
 * @brief Utility computation result
 */
typedef struct {
    float reward_value;           /**< Subjective reward value */
    float effort_cost;            /**< Total effort cost */
    float delay_cost;             /**< Delay discounting */
    float probability_adjusted;   /**< Probability-weighted value */
    float net_utility;            /**< Final utility */
    bool worth_pursuing;          /**< Above threshold? */
} nimcp_utility_result_t;

/**
 * @brief Incentive salience configuration
 */
typedef struct {
    float da_wanting_gain;        /**< DA -> wanting mapping gain */
    float wanting_baseline;       /**< Baseline wanting level */
    float wanting_decay;          /**< Wanting decay rate */
    float effort_sensitivity;     /**< Overall effort sensitivity */
    float delay_sensitivity;      /**< Overall delay sensitivity */
    float urgency_boost;          /**< Urgency enhancement */
    float cue_salience_lr;        /**< Cue learning rate */
    nimcp_effort_params_t effort; /**< Effort parameters */
} nimcp_salience_config_t;

/**
 * @brief Incentive salience system
 */
typedef struct {
    bool initialized;

    /* Configuration */
    nimcp_salience_config_t config;

    /* State */
    nimcp_salience_state_t state;

    /* Goals */
    nimcp_goal_t goals[SALIENCE_MAX_GOALS];
    uint32_t num_goals;
    uint32_t active_goal_id;

    /* Cues */
    nimcp_salience_cue_t cues[SALIENCE_MAX_CUES];
    uint32_t num_cues;

    /* DA state */
    float current_da;             /**< Current DA level */
    float da_baseline;            /**< Baseline for comparison */

    /* Metrics */
    uint32_t update_count;
    float total_wanting;
    float total_effort_expended;
    uint32_t goals_achieved;
} nimcp_salience_system_t;

/*=============================================================================
 * Lifecycle API
 *===========================================================================*/

/**
 * @brief Initialize salience system
 */
int nimcp_salience_init(
    nimcp_salience_system_t* system,
    const nimcp_salience_config_t* config
);

/**
 * @brief Shutdown salience system
 */
int nimcp_salience_shutdown(nimcp_salience_system_t* system);

/**
 * @brief Reset salience system
 */
int nimcp_salience_reset(nimcp_salience_system_t* system);

/**
 * @brief Get default configuration
 */
nimcp_salience_config_t nimcp_salience_default_config(void);

/*=============================================================================
 * Core Salience API
 *===========================================================================*/

/**
 * @brief Update salience system
 * @param system Salience system
 * @param da_level Current DA level
 * @param dt Time step (ms)
 */
int nimcp_salience_update(
    nimcp_salience_system_t* system,
    float da_level,
    float dt
);

/**
 * @brief Compute wanting from DA level
 */
int nimcp_salience_compute_wanting(
    nimcp_salience_system_t* system,
    float da_level,
    float* wanting
);

/**
 * @brief Get current wanting level
 */
int nimcp_salience_get_wanting(
    nimcp_salience_system_t* system,
    float* wanting
);

/**
 * @brief Get current motivation level
 */
int nimcp_salience_get_motivation(
    nimcp_salience_system_t* system,
    nimcp_motivation_level_t* level
);

/**
 * @brief Get vigor (response intensity)
 */
int nimcp_salience_get_vigor(
    nimcp_salience_system_t* system,
    float* vigor
);

/*=============================================================================
 * Goal API
 *===========================================================================*/

/**
 * @brief Add goal
 */
int nimcp_salience_add_goal(
    nimcp_salience_system_t* system,
    nimcp_goal_type_t type,
    float value,
    float effort,
    float delay,
    uint32_t* goal_id
);

/**
 * @brief Remove goal
 */
int nimcp_salience_remove_goal(
    nimcp_salience_system_t* system,
    uint32_t goal_id
);

/**
 * @brief Set active goal
 */
int nimcp_salience_set_active_goal(
    nimcp_salience_system_t* system,
    uint32_t goal_id
);

/**
 * @brief Update goal progress
 */
int nimcp_salience_update_goal_progress(
    nimcp_salience_system_t* system,
    uint32_t goal_id,
    float progress
);

/**
 * @brief Signal goal achievement
 */
int nimcp_salience_goal_achieved(
    nimcp_salience_system_t* system,
    uint32_t goal_id,
    float actual_reward
);

/**
 * @brief Get goal wanting
 */
int nimcp_salience_get_goal_wanting(
    nimcp_salience_system_t* system,
    uint32_t goal_id,
    float* wanting
);

/*=============================================================================
 * Effort-Utility API
 *===========================================================================*/

/**
 * @brief Compute net utility of action
 */
int nimcp_salience_compute_utility(
    nimcp_salience_system_t* system,
    float reward_value,
    float effort_required,
    float delay,
    float probability,
    nimcp_utility_result_t* result
);

/**
 * @brief Compute effort cost
 */
int nimcp_salience_compute_effort_cost(
    nimcp_salience_system_t* system,
    float effort,
    nimcp_effort_type_t type,
    float* cost
);

/**
 * @brief Apply delay discounting
 */
int nimcp_salience_apply_delay_discount(
    nimcp_salience_system_t* system,
    float value,
    float delay,
    float* discounted
);

/**
 * @brief Check if action is worth pursuing
 */
int nimcp_salience_is_worth_pursuing(
    nimcp_salience_system_t* system,
    float reward,
    float effort,
    float delay,
    bool* worth_it
);

/*=============================================================================
 * Cue API
 *===========================================================================*/

/**
 * @brief Add cue-reward association
 */
int nimcp_salience_add_cue(
    nimcp_salience_system_t* system,
    float initial_salience,
    uint32_t* cue_id
);

/**
 * @brief Signal cue presence
 */
int nimcp_salience_cue_present(
    nimcp_salience_system_t* system,
    uint32_t cue_id
);

/**
 * @brief Signal cue absence
 */
int nimcp_salience_cue_absent(
    nimcp_salience_system_t* system,
    uint32_t cue_id
);

/**
 * @brief Update cue salience based on outcome
 */
int nimcp_salience_update_cue(
    nimcp_salience_system_t* system,
    uint32_t cue_id,
    float reward_received
);

/**
 * @brief Get total cue-triggered wanting
 */
int nimcp_salience_get_cue_wanting(
    nimcp_salience_system_t* system,
    float* cue_wanting
);

/*=============================================================================
 * Liking API
 *===========================================================================*/

/**
 * @brief Signal hedonic impact (liking)
 */
int nimcp_salience_signal_liking(
    nimcp_salience_system_t* system,
    float liking
);

/**
 * @brief Get current liking
 */
int nimcp_salience_get_liking(
    nimcp_salience_system_t* system,
    float* liking
);

/**
 * @brief Get wanting/liking dissociation
 */
int nimcp_salience_get_wanting_liking_ratio(
    nimcp_salience_system_t* system,
    float* ratio
);

/*=============================================================================
 * State API
 *===========================================================================*/

/**
 * @brief Get full salience state
 */
int nimcp_salience_get_state(
    nimcp_salience_system_t* system,
    nimcp_salience_state_t* state
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_INCENTIVE_SALIENCE_H */
