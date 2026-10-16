/**
 * @file nimcp_reward_prediction_error.h
 * @brief Reward Prediction Error (RPE) computation for VTA
 * @date 2026-01-11
 *
 * Implements temporal difference (TD) learning for reward prediction:
 * - RPE = actual_reward - expected_reward
 * - Positive RPE -> phasic DA burst (better than expected)
 * - Negative RPE -> phasic DA pause (worse than expected)
 * - Zero RPE -> no change (as expected)
 *
 * Supports:
 * - TD(0) learning
 * - TD(lambda) with eligibility traces
 * - Multiple value functions (state, action, cue)
 */

#ifndef NIMCP_REWARD_PREDICTION_ERROR_H
#define NIMCP_REWARD_PREDICTION_ERROR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/*=============================================================================
 * Constants
 *===========================================================================*/

#define RPE_MAX_STATES             256      /* Max states for value function */
#define RPE_MAX_CUES               32       /* Max reward-predictive cues */
#define RPE_DEFAULT_ALPHA          0.1f     /* Learning rate */
#define RPE_DEFAULT_GAMMA          0.95f    /* Discount factor */
#define RPE_DEFAULT_LAMBDA         0.8f     /* Eligibility trace decay */
#define RPE_BURST_THRESHOLD        0.3f     /* RPE threshold for burst */
#define RPE_PAUSE_THRESHOLD       -0.3f     /* RPE threshold for pause */

/*=============================================================================
 * Enumerations
 *===========================================================================*/

/**
 * @brief RPE type classification
 */
typedef enum {
    RPE_TYPE_NONE = 0,            /**< No significant RPE */
    RPE_TYPE_POSITIVE,            /**< Better than expected */
    RPE_TYPE_NEGATIVE,            /**< Worse than expected */
    RPE_TYPE_SURPRISE             /**< Unexpected event */
} nimcp_rpe_type_t;

/**
 * @brief Value function type
 */
typedef enum {
    VALUE_TYPE_STATE = 0,         /**< State value V(s) */
    VALUE_TYPE_ACTION,            /**< Action value Q(s,a) */
    VALUE_TYPE_CUE,               /**< Cue-triggered prediction */
    VALUE_TYPE_COUNT
} nimcp_value_type_t;

/**
 * @brief Learning algorithm
 */
typedef enum {
    RPE_ALGO_TD0 = 0,             /**< TD(0) */
    RPE_ALGO_TD_LAMBDA,           /**< TD(lambda) with traces */
    RPE_ALGO_SARSA,               /**< On-policy SARSA */
    RPE_ALGO_Q_LEARNING           /**< Off-policy Q-learning */
} nimcp_rpe_algorithm_t;

/*=============================================================================
 * Structures
 *===========================================================================*/

/**
 * @brief Reward-predictive cue
 */
typedef struct {
    uint32_t id;
    float predictive_value;       /**< Learned reward prediction */
    float eligibility;            /**< Eligibility trace */
    float onset_time;             /**< When cue appeared */
    float delay;                  /**< Expected delay to reward */
    uint32_t exposure_count;      /**< Times encountered */
    bool active;                  /**< Currently present */
} nimcp_reward_cue_t;

/**
 * @brief Eligibility trace entry
 */
typedef struct {
    uint32_t state_id;
    float trace_value;
    float decay_rate;
} nimcp_eligibility_entry_t;

/**
 * @brief RPE computation result
 */
typedef struct {
    float rpe;                    /**< Raw RPE value */
    nimcp_rpe_type_t type;        /**< Classification */
    float magnitude;              /**< |RPE| */
    float expected;               /**< What was expected */
    float actual;                 /**< What was received */
    float da_response;            /**< Suggested DA response */
    bool triggers_burst;          /**< Would trigger burst */
    bool triggers_pause;          /**< Would trigger pause */
} nimcp_rpe_result_t;

/**
 * @brief RPE system configuration
 */
typedef struct {
    float alpha;                  /**< Learning rate */
    float gamma;                  /**< Discount factor */
    float lambda;                 /**< Eligibility decay */
    float burst_threshold;        /**< RPE for burst */
    float pause_threshold;        /**< RPE for pause */
    nimcp_rpe_algorithm_t algorithm;
    bool use_eligibility_traces;
    bool normalize_rpe;           /**< Normalize by variance */
    float rpe_scale;              /**< Scale factor for DA response */
} nimcp_rpe_config_t;

/**
 * @brief RPE system state
 */
typedef struct {
    bool initialized;

    /* Configuration */
    nimcp_rpe_config_t config;

    /* Value functions */
    float state_values[RPE_MAX_STATES];
    float eligibility_traces[RPE_MAX_STATES];
    uint32_t current_state;
    uint32_t previous_state;

    /* Cues */
    nimcp_reward_cue_t cues[RPE_MAX_CUES];
    uint32_t num_cues;

    /* RPE state */
    float current_rpe;
    float rpe_variance;           /**< For normalization */
    float rpe_running_mean;
    nimcp_rpe_result_t last_result;

    /* Reward tracking */
    float last_reward;
    float cumulative_reward;
    float average_reward;
    uint32_t reward_count;

    /* Timing */
    float time_since_last_reward;
    float expected_reward_time;

    /* Metrics */
    uint32_t update_count;
    uint32_t positive_rpe_count;
    uint32_t negative_rpe_count;
    float total_learning;
} nimcp_rpe_system_t;

/*=============================================================================
 * Lifecycle API
 *===========================================================================*/

/**
 * @brief Initialize RPE system
 */
int nimcp_rpe_init(nimcp_rpe_system_t* system, const nimcp_rpe_config_t* config);

/**
 * @brief Shutdown RPE system
 */
int nimcp_rpe_shutdown(nimcp_rpe_system_t* system);

/**
 * @brief Reset RPE system
 */
int nimcp_rpe_reset(nimcp_rpe_system_t* system);

/**
 * @brief Get default configuration
 */
nimcp_rpe_config_t nimcp_rpe_default_config(void);

/*=============================================================================
 * Core RPE API
 *===========================================================================*/

/**
 * @brief Compute RPE for reward event
 * @param system RPE system
 * @param reward Actual reward received
 * @param result Output result structure
 */
int nimcp_rpe_compute(
    nimcp_rpe_system_t* system,
    float reward,
    nimcp_rpe_result_t* result
);

/**
 * @brief Update value function based on RPE
 */
int nimcp_rpe_learn(nimcp_rpe_system_t* system, float rpe);

/**
 * @brief Get expected reward for current state
 */
int nimcp_rpe_get_expectation(
    nimcp_rpe_system_t* system,
    float* expected
);

/**
 * @brief Set expected reward manually
 */
int nimcp_rpe_set_expectation(
    nimcp_rpe_system_t* system,
    float expected
);

/*=============================================================================
 * State Transition API
 *===========================================================================*/

/**
 * @brief Signal state transition
 */
int nimcp_rpe_transition_state(
    nimcp_rpe_system_t* system,
    uint32_t new_state
);

/**
 * @brief Get value of state
 */
int nimcp_rpe_get_state_value(
    nimcp_rpe_system_t* system,
    uint32_t state,
    float* value
);

/**
 * @brief Set value of state (for initialization)
 */
int nimcp_rpe_set_state_value(
    nimcp_rpe_system_t* system,
    uint32_t state,
    float value
);

/*=============================================================================
 * Cue API
 *===========================================================================*/

/**
 * @brief Add reward-predictive cue
 */
int nimcp_rpe_add_cue(
    nimcp_rpe_system_t* system,
    float initial_value,
    uint32_t* cue_id
);

/**
 * @brief Signal cue onset
 */
int nimcp_rpe_cue_onset(
    nimcp_rpe_system_t* system,
    uint32_t cue_id,
    float delay_hint
);

/**
 * @brief Signal cue offset
 */
int nimcp_rpe_cue_offset(
    nimcp_rpe_system_t* system,
    uint32_t cue_id
);

/**
 * @brief Get cue's predictive value
 */
int nimcp_rpe_get_cue_value(
    nimcp_rpe_system_t* system,
    uint32_t cue_id,
    float* value
);

/**
 * @brief Update cue values based on outcome
 */
int nimcp_rpe_update_cue_learning(
    nimcp_rpe_system_t* system,
    float actual_reward
);

/*=============================================================================
 * Eligibility Trace API
 *===========================================================================*/

/**
 * @brief Update eligibility traces
 */
int nimcp_rpe_update_traces(nimcp_rpe_system_t* system, float dt);

/**
 * @brief Reset all eligibility traces
 */
int nimcp_rpe_reset_traces(nimcp_rpe_system_t* system);

/**
 * @brief Get eligibility of state
 */
int nimcp_rpe_get_eligibility(
    nimcp_rpe_system_t* system,
    uint32_t state,
    float* eligibility
);

/*=============================================================================
 * Update API
 *===========================================================================*/

/**
 * @brief Time update (decay traces, update timing)
 */
int nimcp_rpe_update(nimcp_rpe_system_t* system, float dt);

/*=============================================================================
 * Query API
 *===========================================================================*/

/**
 * @brief Get current RPE
 */
int nimcp_rpe_get_current(nimcp_rpe_system_t* system, float* rpe);

/**
 * @brief Get last RPE result
 */
int nimcp_rpe_get_last_result(
    nimcp_rpe_system_t* system,
    nimcp_rpe_result_t* result
);

/**
 * @brief Classify RPE type
 */
nimcp_rpe_type_t nimcp_rpe_classify(float rpe, float threshold);

/**
 * @brief Map RPE to DA response
 */
int nimcp_rpe_to_da_response(
    nimcp_rpe_system_t* system,
    float rpe,
    float* da_response
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_REWARD_PREDICTION_ERROR_H */
