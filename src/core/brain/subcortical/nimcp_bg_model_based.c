//=============================================================================
// nimcp_bg_model_based.c - Model-Based Planning for Basal Ganglia
//=============================================================================

#include "core/brain/subcortical/nimcp_bg_model_based.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/algorithms/nimcp_monte_carlo.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_constants.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE_MESH_ONLY(bg_model_based, MESH_ADAPTER_CATEGORY_SUBCORTICAL)


/* ============================================================================
 * INTERNAL STRUCTURES
 * ============================================================================ */

#define MB_REPLAY_BUFFER_SIZE 1000

typedef struct {
    uint32_t state;
    uint32_t action;
    float reward;
    uint32_t next_state;
    float priority;
} mb_replay_entry_t;

struct bg_model_based {
    /* Configuration */
    bg_mb_config_t config;

    /* Transition model: P(s'|s,a) */
    float*** transition_probs;  /* [state][action][next_state] */
    uint32_t** transition_counts;  /* [state*action][next_state] */

    /* Reward model: R(s,a) */
    float** reward_model;       /* [state][action] */
    float** reward_variance;    /* [state][action] */
    uint32_t** reward_counts;   /* [state][action] */

    /* Value function */
    float* state_values;        /* V(s) */
    float** action_values;      /* Q(s,a) */

    /* Arbitration */
    bg_mb_arbitration_t arbitration;
    bg_mb_arbitration_mode_t arbit_mode;

    /* Replay buffer */
    mb_replay_entry_t* replay_buffer;
    uint32_t replay_head;
    uint32_t replay_count;

    /* Planning state */
    uint32_t last_planned_action;

    /* Random state (uses nimcp_monte_carlo utilities) */
    uint32_t rand_seed;

    /* Statistics */
    bg_mb_stats_t stats;

    /* Thread safety */
    nimcp_mutex_t* mutex;
};

/* ============================================================================
 * LIFECYCLE IMPLEMENTATION
 * ============================================================================ */

void bg_mb_default_config(bg_mb_config_t* config) {
    if (!config) return;

    config->num_states = 64;
    config->num_actions = 8;
    config->planning_depth = 5;
    config->num_simulations = 10;

    config->planning_algo = BG_MB_PLAN_TRAJECTORY_SAMPLING;
    config->model_type = BG_MB_MODEL_TABULAR;
    config->arbit_mode = BG_MB_ARBIT_UNCERTAINTY;

    config->transition_lr = BG_MB_TRANSITION_LR;
    config->reward_lr = BG_MB_REWARD_LR;
    config->discount_factor = NIMCP_REWARD_DISCOUNT_DEFAULT;

    config->initial_mb_weight = 0.5f;
    config->uncertainty_threshold = 0.3f;
    config->planning_budget_ms = 50.0f;

    config->enable_prioritized_replay = true;
    config->enable_successor_features = false;
}

bg_model_based_t* bg_mb_create(const bg_mb_config_t* config) {
    bg_model_based_t* mb = nimcp_calloc(1, sizeof(bg_model_based_t));
    if (!mb) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mb is NULL");

        return NULL;

    }

    /* Apply configuration */
    if (config) {
        mb->config = *config;
    } else {
        bg_mb_default_config(&mb->config);
    }

    uint32_t ns = mb->config.num_states;
    uint32_t na = mb->config.num_actions;

    /* Allocate transition model */
    mb->transition_probs = nimcp_calloc(ns, sizeof(float**));
    if (!mb->transition_probs) goto cleanup;

    for (uint32_t s = 0; s < ns; s++) {
        mb->transition_probs[s] = nimcp_calloc(na, sizeof(float*));
        if (!mb->transition_probs[s]) goto cleanup;

        for (uint32_t a = 0; a < na; a++) {
            mb->transition_probs[s][a] = nimcp_calloc(ns, sizeof(float));
            if (!mb->transition_probs[s][a]) goto cleanup;

            /* Initialize uniform transitions */
            for (uint32_t sp = 0; sp < ns; sp++) {
                mb->transition_probs[s][a][sp] = 1.0f / ns;
            }
        }
    }

    /* Allocate reward model */
    mb->reward_model = nimcp_calloc(ns, sizeof(float*));
    mb->reward_variance = nimcp_calloc(ns, sizeof(float*));
    mb->reward_counts = nimcp_calloc(ns, sizeof(uint32_t*));

    if (!mb->reward_model || !mb->reward_variance || !mb->reward_counts) goto cleanup;

    for (uint32_t s = 0; s < ns; s++) {
        mb->reward_model[s] = nimcp_calloc(na, sizeof(float));
        mb->reward_variance[s] = nimcp_calloc(na, sizeof(float));
        mb->reward_counts[s] = nimcp_calloc(na, sizeof(uint32_t));
        if (!mb->reward_model[s] || !mb->reward_variance[s] || !mb->reward_counts[s]) goto cleanup;
    }

    /* Allocate value functions */
    mb->state_values = nimcp_calloc(ns, sizeof(float));
    mb->action_values = nimcp_calloc(ns, sizeof(float*));
    if (!mb->state_values || !mb->action_values) goto cleanup;

    for (uint32_t s = 0; s < ns; s++) {
        mb->action_values[s] = nimcp_calloc(na, sizeof(float));
        if (!mb->action_values[s]) goto cleanup;
    }

    /* Allocate replay buffer */
    mb->replay_buffer = nimcp_calloc(MB_REPLAY_BUFFER_SIZE, sizeof(mb_replay_entry_t));
    if (!mb->replay_buffer) goto cleanup;

    /* Initialize arbitration */
    mb->arbitration.model_based_weight = mb->config.initial_mb_weight;
    mb->arbitration.model_free_weight = 1.0f - mb->config.initial_mb_weight;
    mb->arbitration.model_uncertainty = 1.0f;
    mb->arbitration.mb_reliability = 0.5f;
    mb->arbitration.mf_reliability = 0.5f;

    mb->arbit_mode = mb->config.arbit_mode;

    /* Create mutex */
    mb->mutex = nimcp_mutex_create(NULL);
    if (!mb->mutex) goto cleanup;

    /* Initialize thread-safe RNG seed */
    mb->rand_seed = mc_seed_from_time();

    return mb;

cleanup:
    bg_mb_destroy(mb);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bg_mb_create: allocation failed");
    return NULL;
}

void bg_mb_destroy(bg_model_based_t* mb) {
    if (!mb) return;

    uint32_t ns = mb->config.num_states;
    uint32_t na = mb->config.num_actions;

    if (mb->mutex) {
        nimcp_mutex_free(mb->mutex);
    }

    /* Free transition model */
    if (mb->transition_probs) {
        for (uint32_t s = 0; s < ns; s++) {
            if (mb->transition_probs[s]) {
                for (uint32_t a = 0; a < na; a++) {
                    nimcp_free(mb->transition_probs[s][a]);
                }
                nimcp_free(mb->transition_probs[s]);
            }
        }
        nimcp_free(mb->transition_probs);
    }

    /* Free reward model */
    if (mb->reward_model) {
        for (uint32_t s = 0; s < ns; s++) {
            nimcp_free(mb->reward_model[s]);
        }
        nimcp_free(mb->reward_model);
    }

    if (mb->reward_variance) {
        for (uint32_t s = 0; s < ns; s++) {
            nimcp_free(mb->reward_variance[s]);
        }
        nimcp_free(mb->reward_variance);
    }

    if (mb->reward_counts) {
        for (uint32_t s = 0; s < ns; s++) {
            nimcp_free(mb->reward_counts[s]);
        }
        nimcp_free(mb->reward_counts);
    }

    /* Free value functions */
    if (mb->action_values) {
        for (uint32_t s = 0; s < ns; s++) {
            nimcp_free(mb->action_values[s]);
        }
        nimcp_free(mb->action_values);
    }
    nimcp_free(mb->state_values);

    nimcp_free(mb->replay_buffer);
    nimcp_free(mb);
}

int bg_mb_reset(bg_model_based_t* mb) {
    if (!mb) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bg_mb_reset: mb is NULL");
        return -1;
    }

    nimcp_mutex_lock(mb->mutex);

    uint32_t ns = mb->config.num_states;
    uint32_t na = mb->config.num_actions;

    /* Reset transition model to uniform */
    for (uint32_t s = 0; s < ns; s++) {
        for (uint32_t a = 0; a < na; a++) {
            for (uint32_t sp = 0; sp < ns; sp++) {
                mb->transition_probs[s][a][sp] = 1.0f / ns;
            }
        }
    }

    /* Reset reward model */
    for (uint32_t s = 0; s < ns; s++) {
        memset(mb->reward_model[s], 0, na * sizeof(float));
        memset(mb->reward_variance[s], 0, na * sizeof(float));
        memset(mb->reward_counts[s], 0, na * sizeof(uint32_t));
    }

    /* Reset values */
    memset(mb->state_values, 0, ns * sizeof(float));
    for (uint32_t s = 0; s < ns; s++) {
        memset(mb->action_values[s], 0, na * sizeof(float));
    }

    /* Reset replay buffer */
    mb->replay_head = 0;
    mb->replay_count = 0;

    /* Reset arbitration */
    mb->arbitration.model_based_weight = mb->config.initial_mb_weight;
    mb->arbitration.model_free_weight = 1.0f - mb->config.initial_mb_weight;
    mb->arbitration.model_uncertainty = 1.0f;

    /* Reset statistics */
    memset(&mb->stats, 0, sizeof(bg_mb_stats_t));

    nimcp_mutex_unlock(mb->mutex);
    return 0;
}

/* ============================================================================
 * MODEL LEARNING IMPLEMENTATION
 * ============================================================================ */

int bg_mb_update_transition(bg_model_based_t* mb,
                             uint32_t state,
                             uint32_t action,
                             uint32_t next_state) {
    if (!mb) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bg_mb_update_transition: mb is NULL");
        return -1;
    }
    if (state >= mb->config.num_states ||
        action >= mb->config.num_actions ||
        next_state >= mb->config.num_states) return -1;

    nimcp_mutex_lock(mb->mutex);

    float* probs = mb->transition_probs[state][action];
    float lr = mb->config.transition_lr;

    /* Incremental update towards observed transition */
    for (uint32_t sp = 0; sp < mb->config.num_states; sp++) {
        float target = (sp == next_state) ? 1.0f : 0.0f;
        probs[sp] += lr * (target - probs[sp]);
    }

    /* Normalize probabilities */
    float sum = 0.0f;
    for (uint32_t sp = 0; sp < mb->config.num_states; sp++) {
        sum += probs[sp];
    }
    if (sum > 0.0f) {
        for (uint32_t sp = 0; sp < mb->config.num_states; sp++) {
            probs[sp] /= sum;
        }
    }

    mb->stats.model_updates++;

    nimcp_mutex_unlock(mb->mutex);
    return 0;
}

int bg_mb_update_reward(bg_model_based_t* mb,
                         uint32_t state,
                         uint32_t action,
                         float reward) {
    if (!mb) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bg_mb_update_reward: mb is NULL");
        return -1;
    }
    if (state >= mb->config.num_states ||
        action >= mb->config.num_actions) return -1;

    nimcp_mutex_lock(mb->mutex);

    float lr = mb->config.reward_lr;
    float old_mean = mb->reward_model[state][action];
    float new_mean = old_mean + lr * (reward - old_mean);
    mb->reward_model[state][action] = new_mean;

    /* Update variance estimate */
    float delta = reward - new_mean;
    mb->reward_variance[state][action] =
        mb->reward_variance[state][action] * 0.9f + delta * delta * 0.1f;

    mb->reward_counts[state][action]++;

    nimcp_mutex_unlock(mb->mutex);
    return 0;
}

float bg_mb_get_transition_prob(const bg_model_based_t* mb,
                                 uint32_t state,
                                 uint32_t action,
                                 uint32_t next_state) {
    if (!mb) return 0.0f;
    if (state >= mb->config.num_states ||
        action >= mb->config.num_actions ||
        next_state >= mb->config.num_states) return 0.0f;

    return mb->transition_probs[state][action][next_state];
}

float bg_mb_get_expected_reward(const bg_model_based_t* mb,
                                 uint32_t state,
                                 uint32_t action) {
    if (!mb) return 0.0f;
    if (state >= mb->config.num_states ||
        action >= mb->config.num_actions) return 0.0f;

    return mb->reward_model[state][action];
}

float bg_mb_get_uncertainty(const bg_model_based_t* mb,
                             uint32_t state,
                             uint32_t action) {
    if (!mb) return 1.0f;
    if (state >= mb->config.num_states ||
        action >= mb->config.num_actions) return 1.0f;

    uint32_t count = mb->reward_counts[state][action];
    if (count == 0) return 1.0f;

    /* Uncertainty decreases with observations */
    return 1.0f / sqrtf((float)count + 1.0f);
}

/* ============================================================================
 * PLANNING IMPLEMENTATION
 * ============================================================================ */

int bg_mb_plan(bg_model_based_t* mb,
                uint32_t current_state,
                bg_mb_plan_result_t* result) {
    if (!mb || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bg_mb_plan: required parameter is NULL (mb, result)");
        return -1;
    }
    if (current_state >= mb->config.num_states) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "bg_mb_plan: current_state out of range");
        return -1;
    }

    nimcp_mutex_lock(mb->mutex);

    uint32_t na = mb->config.num_actions;
    uint32_t ns = mb->config.num_states;
    float gamma = mb->config.discount_factor;

    /* Allocate result action_values if needed */
    result->action_values = mb->action_values[current_state];
    result->num_actions = na;

    /* Simple one-step lookahead planning */
    float best_value = -1e9f;
    uint32_t best_action = 0;

    for (uint32_t a = 0; a < na; a++) {
        float expected_reward = mb->reward_model[current_state][a];

        /* Expected future value */
        float expected_future = 0.0f;
        for (uint32_t sp = 0; sp < ns; sp++) {
            expected_future += mb->transition_probs[current_state][a][sp] *
                               mb->state_values[sp];
        }

        float q_value = expected_reward + gamma * expected_future;
        mb->action_values[current_state][a] = q_value;

        if (q_value > best_value) {
            best_value = q_value;
            best_action = a;
        }
    }

    result->best_action = best_action;
    result->expected_return = best_value;
    result->simulations_run = 1;
    result->planning_time_ms = 0.1f;

    mb->last_planned_action = best_action;
    mb->stats.planning_episodes++;

    nimcp_mutex_unlock(mb->mutex);
    return 0;
}

int bg_mb_simulate_trajectory(bg_model_based_t* mb,
                               uint32_t start_state,
                               uint32_t horizon,
                               bg_mb_trajectory_t* trajectory) {
    if (!mb || !trajectory) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bg_mb_simulate_trajectory: required parameter is NULL (mb, trajectory)");
        return -1;
    }
    if (start_state >= mb->config.num_states) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "bg_mb_simulate_trajectory: start_state out of range");
        return -1;
    }

    nimcp_mutex_lock(mb->mutex);

    trajectory->length = 0;
    trajectory->total_return = 0.0f;

    uint32_t state = start_state;
    float gamma = mb->config.discount_factor;
    float discount = 1.0f;

    for (uint32_t t = 0; t < horizon && t < BG_MB_MAX_PLANNING_DEPTH; t++) {
        /* Choose best action */
        float best_q = -1e9f;
        uint32_t best_a = 0;
        for (uint32_t a = 0; a < mb->config.num_actions; a++) {
            if (mb->action_values[state][a] > best_q) {
                best_q = mb->action_values[state][a];
                best_a = a;
            }
        }

        /* Get reward */
        float reward = mb->reward_model[state][best_a];
        trajectory->total_return += discount * reward;
        discount *= gamma;

        /* Sample next state using thread-safe RNG */
        float r = mc_random_uniform(&mb->rand_seed);
        float cumsum = 0.0f;
        uint32_t next_state = 0;
        for (uint32_t sp = 0; sp < mb->config.num_states; sp++) {
            cumsum += mb->transition_probs[state][best_a][sp];
            if (r < cumsum) {
                next_state = sp;
                break;
            }
        }

        if (trajectory->states) trajectory->states[t] = state;
        if (trajectory->actions) trajectory->actions[t] = best_a;
        if (trajectory->rewards) trajectory->rewards[t] = reward;

        trajectory->length++;
        state = next_state;
    }

    nimcp_mutex_unlock(mb->mutex);
    return 0;
}

int bg_mb_prioritized_sweep(bg_model_based_t* mb, uint32_t num_updates) {
    if (!mb) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bg_mb_prioritized_sweep: mb is NULL");
        return -1;
    }

    nimcp_mutex_lock(mb->mutex);

    /* Simple backup for random states */
    float gamma = mb->config.discount_factor;

    for (uint32_t u = 0; u < num_updates; u++) {
        uint32_t s = mc_random_int(&mb->rand_seed, mb->config.num_states);

        /* Compute V(s) = max_a Q(s,a) */
        float max_q = -1e9f;
        for (uint32_t a = 0; a < mb->config.num_actions; a++) {
            float expected_reward = mb->reward_model[s][a];
            float expected_future = 0.0f;
            for (uint32_t sp = 0; sp < mb->config.num_states; sp++) {
                expected_future += mb->transition_probs[s][a][sp] * mb->state_values[sp];
            }
            float q = expected_reward + gamma * expected_future;
            if (q > max_q) max_q = q;
        }
        mb->state_values[s] = max_q;
    }

    nimcp_mutex_unlock(mb->mutex);
    return 0;
}

uint32_t bg_mb_get_planned_action(const bg_model_based_t* mb, uint32_t state) {
    if (!mb || state >= mb->config.num_states) return 0;
    return mb->last_planned_action;
}

float bg_mb_get_state_value(const bg_model_based_t* mb, uint32_t state) {
    if (!mb || state >= mb->config.num_states) return 0.0f;
    return mb->state_values[state];
}

/* ============================================================================
 * ARBITRATION IMPLEMENTATION
 * ============================================================================ */

int bg_mb_arbitrate(bg_model_based_t* mb,
                     float* mb_q_values,
                     float* mf_q_values,
                     uint32_t num_actions,
                     float* combined_q_values,
                     bg_mb_arbitration_t* arbit_state) {
    if (!mb || !mb_q_values || !mf_q_values || !combined_q_values) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bg_mb_arbitrate: required parameter is NULL (mb, mb_q_values, mf_q_values, combined_q_values)");
        return -1;
    }

    nimcp_mutex_lock(mb->mutex);

    float mb_w = mb->arbitration.model_based_weight;
    float mf_w = mb->arbitration.model_free_weight;

    for (uint32_t a = 0; a < num_actions; a++) {
        combined_q_values[a] = mb_w * mb_q_values[a] + mf_w * mf_q_values[a];
    }

    if (arbit_state) {
        *arbit_state = mb->arbitration;
    }

    nimcp_mutex_unlock(mb->mutex);
    return 0;
}

int bg_mb_update_arbitration(bg_model_based_t* mb,
                              float prediction_error,
                              bool was_mb_action) {
    if (!mb) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bg_mb_update_arbitration: mb is NULL");
        return -1;
    }

    nimcp_mutex_lock(mb->mutex);

    float lr = BG_MB_ARBITRATION_LR;
    float abs_pe = fabsf(prediction_error);

    if (was_mb_action) {
        /* Update MB reliability based on prediction error */
        mb->arbitration.mb_reliability *= (1.0f - lr);
        mb->arbitration.mb_reliability += lr * (1.0f - abs_pe);
    } else {
        /* Update MF reliability */
        mb->arbitration.mf_reliability *= (1.0f - lr);
        mb->arbitration.mf_reliability += lr * (1.0f - abs_pe);
    }

    /* Update weights based on reliability */
    float total = mb->arbitration.mb_reliability + mb->arbitration.mf_reliability;
    if (total > 0.0f) {
        mb->arbitration.model_based_weight = mb->arbitration.mb_reliability / total;
        mb->arbitration.model_free_weight = mb->arbitration.mf_reliability / total;
    }

    mb->arbitration.model_based_weight = nimcp_clampf(mb->arbitration.model_based_weight, 0.1f, 0.9f);
    mb->arbitration.model_free_weight = 1.0f - mb->arbitration.model_based_weight;

    mb->stats.avg_mb_weight = mb->arbitration.model_based_weight;

    nimcp_mutex_unlock(mb->mutex);
    return 0;
}

int bg_mb_get_arbitration(const bg_model_based_t* mb,
                           bg_mb_arbitration_t* arbit) {
    if (!mb || !arbit) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bg_mb_get_arbitration: required parameter is NULL (mb, arbit)");
        return -1;
    }
    *arbit = mb->arbitration;
    return 0;
}

int bg_mb_set_arbitration_mode(bg_model_based_t* mb,
                                bg_mb_arbitration_mode_t mode) {
    if (!mb || mode >= BG_MB_ARBIT_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "bg_mb_set_arbitration_mode: invalid mode");
        return -1;
    }

    nimcp_mutex_lock(mb->mutex);
    mb->arbit_mode = mode;
    nimcp_mutex_unlock(mb->mutex);
    return 0;
}

/* ============================================================================
 * REPLAY IMPLEMENTATION
 * ============================================================================ */

int bg_mb_store_experience(bg_model_based_t* mb,
                            uint32_t state,
                            uint32_t action,
                            float reward,
                            uint32_t next_state) {
    if (!mb) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bg_mb_get_state_value: mb is NULL");
        return -1;
    }

    nimcp_mutex_lock(mb->mutex);

    mb_replay_entry_t* entry = &mb->replay_buffer[mb->replay_head];
    entry->state = state;
    entry->action = action;
    entry->reward = reward;
    entry->next_state = next_state;
    entry->priority = fabsf(reward) + 0.01f;

    mb->replay_head = (mb->replay_head + 1) % MB_REPLAY_BUFFER_SIZE;
    if (mb->replay_count < MB_REPLAY_BUFFER_SIZE) {
        mb->replay_count++;
    }

    nimcp_mutex_unlock(mb->mutex);
    return 0;
}

int bg_mb_replay(bg_model_based_t* mb, uint32_t num_replays) {
    if (!mb || mb->replay_count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "bg_mb_replay: mb is NULL");
        return -1;
    }

    nimcp_mutex_lock(mb->mutex);

    for (uint32_t r = 0; r < num_replays; r++) {
        /* Sample random experience using thread-safe RNG */
        uint32_t idx = mc_random_int(&mb->rand_seed, mb->replay_count);
        mb_replay_entry_t* entry = &mb->replay_buffer[idx];

        /* Update models */
        bg_mb_update_transition(mb, entry->state, entry->action, entry->next_state);
        bg_mb_update_reward(mb, entry->state, entry->action, entry->reward);
    }

    mb->stats.replay_utilization =
        (float)mb->replay_count / MB_REPLAY_BUFFER_SIZE;

    nimcp_mutex_unlock(mb->mutex);
    return 0;
}

int bg_mb_clear_replay(bg_model_based_t* mb) {
    if (!mb) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bg_mb_clear_replay: mb is NULL");
        return -1;
    }

    nimcp_mutex_lock(mb->mutex);
    mb->replay_head = 0;
    mb->replay_count = 0;
    nimcp_mutex_unlock(mb->mutex);
    return 0;
}

/* ============================================================================
 * QUERY IMPLEMENTATION
 * ============================================================================ */

int bg_mb_get_stats(const bg_model_based_t* mb, bg_mb_stats_t* stats) {
    if (!mb || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bg_mb_get_stats: required parameter is NULL (mb, stats)");
        return -1;
    }
    *stats = mb->stats;
    return 0;
}

float bg_mb_get_model_accuracy(const bg_model_based_t* mb) {
    if (!mb) return 0.0f;

    /* Estimate accuracy from reliability */
    return nimcp_clampf(mb->arbitration.mb_reliability, 0.0f, 1.0f);
}
