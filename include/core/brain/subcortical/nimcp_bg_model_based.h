//=============================================================================
// nimcp_bg_model_based.h - Model-Based Planning for Basal Ganglia
//=============================================================================
/**
 * @file nimcp_bg_model_based.h
 * @brief Model-based planning and forward models for basal ganglia
 *
 * BIOLOGICAL BASIS:
 * The basal ganglia supports both:
 * - Model-free: Cached action values (habit-like)
 * - Model-based: Forward planning using internal models
 *
 * KEY COMPONENTS:
 * - Transition model: P(s'|s,a) - predicts next states
 * - Reward model: R(s,a) - predicts rewards
 * - Planning: Mental simulation via forward model
 * - Arbitration: Balance between model-free and model-based
 *
 * INTEGRATION:
 * - Dorsolateral striatum: Model-free
 * - Dorsomedial striatum: Model-based
 * - PFC: Model maintenance and planning
 * - Hippocampus: State representations
 *
 * @version 1.0.0
 * @date 2025-12-30
 */

#ifndef NIMCP_BG_MODEL_BASED_H
#define NIMCP_BG_MODEL_BASED_H

#include "utils/validation/nimcp_common.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * CONSTANTS
 * ============================================================================ */

#define BG_MB_MAX_STATES            256     /**< Maximum discrete states */
#define BG_MB_MAX_ACTIONS           32      /**< Maximum actions */
#define BG_MB_MAX_PLANNING_DEPTH    10      /**< Maximum planning horizon */
#define BG_MB_MAX_TRAJECTORIES      100     /**< Max simulated trajectories */

/** Learning rates */
#define BG_MB_TRANSITION_LR         0.1f
#define BG_MB_REWARD_LR             0.2f
#define BG_MB_ARBITRATION_LR        0.05f

/* ============================================================================
 * ENUMERATIONS
 * ============================================================================ */

/**
 * @brief Planning algorithm
 */
typedef enum {
    BG_MB_PLAN_VALUE_ITERATION,     /**< Full value iteration */
    BG_MB_PLAN_MCTS,                /**< Monte Carlo tree search */
    BG_MB_PLAN_PRIORITIZED_SWEEP,   /**< Prioritized sweeping */
    BG_MB_PLAN_TRAJECTORY_SAMPLING, /**< Sample trajectories */
    BG_MB_PLAN_COUNT
} bg_mb_planning_algo_t;

/**
 * @brief Model type
 */
typedef enum {
    BG_MB_MODEL_TABULAR,            /**< Table-based model */
    BG_MB_MODEL_NEURAL,             /**< Neural network model */
    BG_MB_MODEL_GAUSSIAN,           /**< Gaussian process model */
    BG_MB_MODEL_COUNT
} bg_mb_model_type_t;

/**
 * @brief Arbitration mode
 */
typedef enum {
    BG_MB_ARBIT_FIXED,              /**< Fixed weighting */
    BG_MB_ARBIT_UNCERTAINTY,        /**< Uncertainty-based */
    BG_MB_ARBIT_RELIABILITY,        /**< Reliability-based */
    BG_MB_ARBIT_SPEED_ACCURACY,     /**< Speed-accuracy tradeoff */
    BG_MB_ARBIT_COUNT
} bg_mb_arbitration_mode_t;

/* ============================================================================
 * DATA STRUCTURES
 * ============================================================================ */

/**
 * @brief Transition model entry
 */
typedef struct {
    uint32_t from_state;
    uint32_t action;
    uint32_t to_state;
    float probability;
    uint32_t count;                 /**< Observation count */
} bg_mb_transition_t;

/**
 * @brief Reward model entry
 */
typedef struct {
    uint32_t state;
    uint32_t action;
    float expected_reward;
    float reward_variance;
    uint32_t count;
} bg_mb_reward_entry_t;

/**
 * @brief Simulated trajectory
 */
typedef struct {
    uint32_t* states;
    uint32_t* actions;
    float* rewards;
    uint32_t length;
    float total_return;
} bg_mb_trajectory_t;

/**
 * @brief Planning result
 */
typedef struct {
    uint32_t best_action;
    float expected_return;
    float* action_values;           /**< Q-values for each action */
    uint32_t num_actions;
    uint32_t simulations_run;
    float planning_time_ms;
} bg_mb_plan_result_t;

/**
 * @brief Arbitration state
 */
typedef struct {
    float model_based_weight;       /**< Weight for MB system */
    float model_free_weight;        /**< Weight for MF system */
    float model_uncertainty;        /**< Current model uncertainty */
    float mb_reliability;           /**< MB system reliability estimate */
    float mf_reliability;           /**< MF system reliability estimate */
    float decision_conflict;        /**< MB vs MF disagreement */
} bg_mb_arbitration_t;

/**
 * @brief Configuration
 */
typedef struct {
    uint32_t num_states;
    uint32_t num_actions;
    uint32_t planning_depth;
    uint32_t num_simulations;

    bg_mb_planning_algo_t planning_algo;
    bg_mb_model_type_t model_type;
    bg_mb_arbitration_mode_t arbit_mode;

    float transition_lr;
    float reward_lr;
    float discount_factor;

    float initial_mb_weight;
    float uncertainty_threshold;
    float planning_budget_ms;

    bool enable_prioritized_replay;
    bool enable_successor_features;
} bg_mb_config_t;

/**
 * @brief Statistics
 */
typedef struct {
    uint32_t model_updates;
    uint32_t planning_episodes;
    float avg_planning_depth;
    float model_accuracy;
    float avg_mb_weight;
    float replay_utilization;
} bg_mb_stats_t;

/**
 * @brief Main handle
 */
typedef struct bg_model_based bg_model_based_t;

/* ============================================================================
 * LIFECYCLE API
 * ============================================================================ */

void bg_mb_default_config(bg_mb_config_t* config);
bg_model_based_t* bg_mb_create(const bg_mb_config_t* config);
void bg_mb_destroy(bg_model_based_t* mb);
int bg_mb_reset(bg_model_based_t* mb);

/* ============================================================================
 * MODEL LEARNING API
 * ============================================================================ */

/**
 * @brief Update transition model with observation
 */
int bg_mb_update_transition(bg_model_based_t* mb,
                             uint32_t state,
                             uint32_t action,
                             uint32_t next_state);

/**
 * @brief Update reward model with observation
 */
int bg_mb_update_reward(bg_model_based_t* mb,
                         uint32_t state,
                         uint32_t action,
                         float reward);

/**
 * @brief Get transition probability
 */
float bg_mb_get_transition_prob(const bg_model_based_t* mb,
                                 uint32_t state,
                                 uint32_t action,
                                 uint32_t next_state);

/**
 * @brief Get expected reward
 */
float bg_mb_get_expected_reward(const bg_model_based_t* mb,
                                 uint32_t state,
                                 uint32_t action);

/**
 * @brief Get model uncertainty for state-action
 */
float bg_mb_get_uncertainty(const bg_model_based_t* mb,
                             uint32_t state,
                             uint32_t action);

/* ============================================================================
 * PLANNING API
 * ============================================================================ */

/**
 * @brief Plan from current state
 */
int bg_mb_plan(bg_model_based_t* mb,
                uint32_t current_state,
                bg_mb_plan_result_t* result);

/**
 * @brief Simulate single trajectory
 */
int bg_mb_simulate_trajectory(bg_model_based_t* mb,
                               uint32_t start_state,
                               uint32_t horizon,
                               bg_mb_trajectory_t* trajectory);

/**
 * @brief Run prioritized sweeping update
 */
int bg_mb_prioritized_sweep(bg_model_based_t* mb,
                             uint32_t num_updates);

/**
 * @brief Get planned action
 */
uint32_t bg_mb_get_planned_action(const bg_model_based_t* mb,
                                   uint32_t state);

/**
 * @brief Get state value from planning
 */
float bg_mb_get_state_value(const bg_model_based_t* mb,
                             uint32_t state);

/* ============================================================================
 * ARBITRATION API
 * ============================================================================ */

/**
 * @brief Arbitrate between model-based and model-free
 */
int bg_mb_arbitrate(bg_model_based_t* mb,
                     float* mb_q_values,
                     float* mf_q_values,
                     uint32_t num_actions,
                     float* combined_q_values,
                     bg_mb_arbitration_t* arbit_state);

/**
 * @brief Update arbitration based on outcome
 */
int bg_mb_update_arbitration(bg_model_based_t* mb,
                              float prediction_error,
                              bool was_mb_action);

/**
 * @brief Get current arbitration weights
 */
int bg_mb_get_arbitration(const bg_model_based_t* mb,
                           bg_mb_arbitration_t* arbit);

/**
 * @brief Set arbitration mode
 */
int bg_mb_set_arbitration_mode(bg_model_based_t* mb,
                                bg_mb_arbitration_mode_t mode);

/* ============================================================================
 * REPLAY API
 * ============================================================================ */

/**
 * @brief Store experience for replay
 */
int bg_mb_store_experience(bg_model_based_t* mb,
                            uint32_t state,
                            uint32_t action,
                            float reward,
                            uint32_t next_state);

/**
 * @brief Perform replay-based planning
 */
int bg_mb_replay(bg_model_based_t* mb, uint32_t num_replays);

/**
 * @brief Clear replay buffer
 */
int bg_mb_clear_replay(bg_model_based_t* mb);

/* ============================================================================
 * QUERY API
 * ============================================================================ */

int bg_mb_get_stats(const bg_model_based_t* mb, bg_mb_stats_t* stats);

/**
 * @brief Get model accuracy estimate
 */
float bg_mb_get_model_accuracy(const bg_model_based_t* mb);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BG_MODEL_BASED_H */
