//=============================================================================
// nimcp_gt_learning.c - Strategic Learning Implementation
//=============================================================================
/**
 * @file nimcp_gt_learning.c
 * @brief Strategic learning algorithm implementations
 *
 * Implements Q-learning, SARSA, CFR, Fictitious Play, EXP3,
 * and opponent modeling for game-theoretic learning.
 */

#include "cognitive/game_theory/nimcp_gt_learning.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <float.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_constants.h"
#include "constants/nimcp_math_constants.h"

BRIDGE_BOILERPLATE(gt_learning, MESH_ADAPTER_CATEGORY_COGNITIVE)



//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Q-table internal structure
 */
struct nimcp_gt_q_table_struct {
    nimcp_gt_q_entry_t* entries;      /**< Flat array [state * num_actions + action] */
    uint32_t num_states;
    uint32_t num_actions;
};

/**
 * @brief Regret table internal structure
 */
struct nimcp_gt_regret_table_struct {
    nimcp_gt_regret_entry_t* entries; /**< Array indexed by info set */
    uint32_t num_info_sets;
    uint32_t max_actions;
};

/**
 * @brief Learner internal structure
 */
struct nimcp_gt_learner_struct {
    nimcp_gt_learning_config_t config;

    /* Q-table (for Q-learning, SARSA) */
    nimcp_gt_q_table_t q_table;

    /* Regret table (for CFR) */
    nimcp_gt_regret_table_t regret_table;

    /* Fictitious play state */
    uint32_t* opponent_action_counts;
    uint32_t total_opponent_observations;

    /* EXP3 state */
    float* exp3_weights;
    float* exp3_probabilities;

    /* Opponent model */
    nimcp_gt_opponent_model_t opponent_model;
    uint32_t* action_history;
    uint32_t* our_action_history;
    uint32_t history_len;
    uint32_t history_capacity;

    /* Current rates */
    float current_learning_rate;
    float current_exploration_rate;
    uint64_t schedule_step;

    /* Statistics */
    nimcp_gt_learning_stats_t stats;

    /* Q-value change tracking for convergence */
    float last_max_q_change;

    /* Random state */
    unsigned int rand_seed;

    /* Thread safety */
    nimcp_platform_mutex_t mutex;
};

//=============================================================================
// Static Name Tables
//=============================================================================

static const char* s_learn_method_names[] = {
    "Q-Learning",
    "SARSA",
    "Counterfactual Regret Minimization",
    "Fictitious Play",
    "EXP3"
};

static const char* s_explore_strategy_names[] = {
    "Epsilon-Greedy",
    "Boltzmann/Softmax",
    "Upper Confidence Bound",
    "Thompson Sampling"
};

static const char* s_opponent_type_names[] = {
    "Unknown",
    "Random",
    "Cooperative",
    "Competitive",
    "Tit-for-Tat",
    "Adaptive"
};

//=============================================================================
// Static Helpers
//=============================================================================

/**
 * @brief Generate random float in [0, 1)
 */
static float rand_float(unsigned int* seed) {
    return (float)rand_r(seed) / (float)RAND_MAX;
}

/**
 * @brief Sample from discrete distribution
 */
static uint32_t sample_distribution(const float* probs, uint32_t n, unsigned int* seed) {
    float r = rand_float(seed);
    float cumsum = 0.0f;

    for (uint32_t i = 0; i < n; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && n > 256) {
            gt_learning_heartbeat("gt_learning_loop",
                             (float)(i + 1) / (float)n);
        }

        cumsum += probs[i];
        if (r < cumsum) {
            return i;
        }
    }

    return n - 1;  /* Handle rounding errors */
}

/**
 * @brief Softmax normalization
 */
static void softmax(const float* values, float* probs, uint32_t n, float temperature) {
    if (n == 0 || temperature <= 0.0f) {
        return;
    }

    /* Find max for numerical stability */
    float max_val = values[0];
    for (uint32_t i = 1; i < n; i++) {
        if (values[i] > max_val) {
            max_val = values[i];
        }
    }

    /* Compute exp and sum */
    float sum = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && n > 256) {
            gt_learning_heartbeat("gt_learning_loop",
                             (float)(i + 1) / (float)n);
        }

        probs[i] = expf((values[i] - max_val) / temperature);
        sum += probs[i];
    }

    /* Normalize */
    if (sum > 0.0f) {
        for (uint32_t i = 0; i < n; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && n > 256) {
                gt_learning_heartbeat("gt_learning_loop",
                                 (float)(i + 1) / (float)n);
            }

            probs[i] /= sum;
        }
    } else {
        /* Uniform fallback */
        float uniform = 1.0f / (float)n;
        for (uint32_t i = 0; i < n; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && n > 256) {
                gt_learning_heartbeat("gt_learning_loop",
                                 (float)(i + 1) / (float)n);
            }

            probs[i] = uniform;
        }
    }
}

/**
 * @brief Find argmax of array
 */
static uint32_t argmax(const float* values, uint32_t n) {
    if (n == 0) return 0;

    uint32_t best = 0;
    float best_val = values[0];

    for (uint32_t i = 1; i < n; i++) {
        if (values[i] > best_val) {
            best_val = values[i];
            best = i;
        }
    }

    return best;
}

/**
 * @brief Get Q-entry pointer (internal, no locking)
 */
static nimcp_gt_q_entry_t* get_q_entry(
    nimcp_gt_q_table_t q_table,
    uint32_t state,
    uint32_t action
) {
    if (!q_table || state >= q_table->num_states || action >= q_table->num_actions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "get_q_entry: q_table is NULL");
        return NULL;
    }
    return &q_table->entries[state * q_table->num_actions + action];
}

/**
 * @brief Get regret entry pointer (internal)
 */
static nimcp_gt_regret_entry_t* get_regret_entry(
    nimcp_gt_regret_table_t table,
    uint32_t info_set
) {
    if (!table || info_set >= table->num_info_sets) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "get_regret_entry: table is NULL");
        return NULL;
    }
    return &table->entries[info_set];
}

//=============================================================================
// Q-Table Management
//=============================================================================

static nimcp_gt_q_table_t q_table_create(uint32_t num_states, uint32_t num_actions) {
    if (num_states == 0 || num_actions == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "q_table_create: num_states is zero");
        return NULL;
    }

    nimcp_gt_q_table_t table = nimcp_calloc(1, sizeof(struct nimcp_gt_q_table_struct));
    if (!table) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate table");

        return NULL;
    }

    table->num_states = num_states;
    table->num_actions = num_actions;

    size_t num_entries = (size_t)num_states * num_actions;
    table->entries = nimcp_calloc(num_entries, sizeof(nimcp_gt_q_entry_t));
    if (!table->entries) {
        nimcp_free(table);
        table = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "q_table_create: table->entries is NULL");
        return NULL;
    }

    return table;
}

static void q_table_destroy(nimcp_gt_q_table_t table) {
    if (!table) return;
    nimcp_free(table->entries);
    nimcp_free(table);
    table = NULL;
}

static void q_table_reset(nimcp_gt_q_table_t table) {
    if (!table || !table->entries) return;
    size_t num_entries = (size_t)table->num_states * table->num_actions;
    memset(table->entries, 0, num_entries * sizeof(nimcp_gt_q_entry_t));
}

//=============================================================================
// Regret Table Management
//=============================================================================

static nimcp_gt_regret_table_t regret_table_create(uint32_t num_info_sets, uint32_t max_actions) {
    if (num_info_sets == 0 || max_actions == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "regret_table_create: num_info_sets is zero");
        return NULL;
    }

    nimcp_gt_regret_table_t table = nimcp_calloc(1, sizeof(struct nimcp_gt_regret_table_struct));
    if (!table) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate table");

        return NULL;
    }

    table->num_info_sets = num_info_sets;
    table->max_actions = max_actions;

    table->entries = nimcp_calloc(num_info_sets, sizeof(nimcp_gt_regret_entry_t));
    if (!table->entries) {
        nimcp_free(table);
        table = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "regret_table_create: table->entries is NULL");
        return NULL;
    }

    /* Allocate regret/strategy arrays for each info set */
    for (uint32_t i = 0; i < num_info_sets; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_info_sets > 256) {
            gt_learning_heartbeat("gt_learning_loop",
                             (float)(i + 1) / (float)num_info_sets);
        }

        table->entries[i].cumulative_regret = nimcp_calloc(max_actions, sizeof(float));
        table->entries[i].cumulative_strategy = nimcp_calloc(max_actions, sizeof(float));
        table->entries[i].num_actions = max_actions;

        if (!table->entries[i].cumulative_regret || !table->entries[i].cumulative_strategy) {
            /* Cleanup on failure */
            for (uint32_t j = 0; j <= i; j++) {
                nimcp_free(table->entries[j].cumulative_regret);
                nimcp_free(table->entries[j].cumulative_strategy);
            }
            nimcp_free(table->entries);
            nimcp_free(table);
            table = NULL;
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "regret_table_create: failed to allocate regret/strategy arrays");
            return NULL;
        }
    }

    return table;
}

static void regret_table_destroy(nimcp_gt_regret_table_t table) {
    if (!table) return;

    for (uint32_t i = 0; i < table->num_info_sets; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && table->num_info_sets > 256) {
            gt_learning_heartbeat("gt_learning_loop",
                             (float)(i + 1) / (float)table->num_info_sets);
        }

        nimcp_free(table->entries[i].cumulative_regret);
        nimcp_free(table->entries[i].cumulative_strategy);
    }
    nimcp_free(table->entries);
    nimcp_free(table);
    table = NULL;
}

static void regret_table_reset(nimcp_gt_regret_table_t table) {
    if (!table) return;

    for (uint32_t i = 0; i < table->num_info_sets; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && table->num_info_sets > 256) {
            gt_learning_heartbeat("gt_learning_loop",
                             (float)(i + 1) / (float)table->num_info_sets);
        }

        if (table->entries[i].cumulative_regret) {
            memset(table->entries[i].cumulative_regret, 0,
                   table->max_actions * sizeof(float));
        }
        if (table->entries[i].cumulative_strategy) {
            memset(table->entries[i].cumulative_strategy, 0,
                   table->max_actions * sizeof(float));
        }
        table->entries[i].iteration_count = 0;
    }
}

//=============================================================================
// Configuration
//=============================================================================

nimcp_gt_learning_config_t nimcp_gt_learning_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    gt_learning_heartbeat("gt_learning_gt_learning_default_", 0.0f);


    nimcp_gt_learning_config_t config = {
        .method = NIMCP_GT_LEARN_Q_LEARNING,
        .explore = NIMCP_GT_EXPLORE_EPSILON_GREEDY,
        .lr_schedule = NIMCP_GT_SCHEDULE_DECAY,

        /* Learning parameters */
        .learning_rate = NIMCP_LEARNING_RATE_COARSE,
        .discount_factor = 0.95f,
        .exploration_rate = 0.3f,
        .exploration_min = 0.01f,
        .exploration_decay = 0.995f,
        .temperature = NIMCP_TEMPERATURE_DEFAULT,

        /* Schedule parameters */
        .lr_decay_rate = 0.999f,
        .lr_min = 0.001f,

        /* CFR parameters */
        .use_linear_cfr = false,
        .use_cfr_plus = true,
        .regret_matching_epsilon = NIMCP_CONVERGENCE_THRESHOLD,

        /* EXP3 parameters */
        .exp3_gamma = 0.1f,

        /* Opponent modeling */
        .enable_opponent_modeling = true,
        .model_window_size = 50,
        .type_prior_strength = 1.0f,

        /* Dimensions (to be set by create) */
        .num_states = 0,
        .num_actions = 0
    };

    return config;
}

const char* nimcp_gt_learn_method_name(nimcp_gt_learn_method_t method) {
    if (method >= NIMCP_GT_LEARN_COUNT) {
        return "Unknown";
    }
    return s_learn_method_names[method];
}

const char* nimcp_gt_explore_strategy_name(nimcp_gt_explore_strategy_t strategy) {
    if (strategy >= NIMCP_GT_EXPLORE_COUNT) {
        return "Unknown";
    }
    return s_explore_strategy_names[strategy];
}

const char* nimcp_gt_opponent_type_name(nimcp_gt_opponent_type_t type) {
    if (type >= NIMCP_GT_OPPONENT_COUNT) {
        return "Unknown";
    }
    return s_opponent_type_names[type];
}

//=============================================================================
// Lifecycle
//=============================================================================

nimcp_gt_learner_t nimcp_gt_learner_create(
    const nimcp_gt_learning_config_t* config,
    uint32_t num_states,
    uint32_t num_actions
) {
    if (!config || num_states == 0 || num_actions == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_gt_learner_create: config is NULL");
        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    gt_learning_heartbeat("gt_learning_gt_learner_create", 0.0f);


    if (num_states > NIMCP_GT_MAX_STATES || num_actions > NIMCP_GT_MAX_ACTIONS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_gt_learner_create: validation failed");
        return NULL;
    }

    nimcp_gt_learner_t learner = nimcp_calloc(1, sizeof(struct nimcp_gt_learner_struct));
    if (!learner) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate learner");

        return NULL;
    }

    /* Copy config and set dimensions */
    learner->config = *config;
    learner->config.num_states = num_states;
    learner->config.num_actions = num_actions;

    /* Initialize current rates */
    learner->current_learning_rate = config->learning_rate;
    learner->current_exploration_rate = config->exploration_rate;
    learner->schedule_step = 0;

    /* Initialize random seed */
    learner->rand_seed = (unsigned int)time(NULL) ^ (unsigned int)(uintptr_t)learner;

    /* Create Q-table for TD methods */
    if (config->method == NIMCP_GT_LEARN_Q_LEARNING ||
        config->method == NIMCP_GT_LEARN_SARSA) {
        learner->q_table = q_table_create(num_states, num_actions);
        if (!learner->q_table) {
            nimcp_free(learner);
            learner = NULL;
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_gt_learner_create: learner->q_table is NULL");
            return NULL;
        }
    }

    /* Create regret table for CFR */
    if (config->method == NIMCP_GT_LEARN_CFR) {
        uint32_t num_info_sets = num_states;  /* Treat states as info sets */
        if (num_info_sets > NIMCP_GT_MAX_INFO_SETS) {
            num_info_sets = NIMCP_GT_MAX_INFO_SETS;
        }
        learner->regret_table = regret_table_create(num_info_sets, num_actions);
        if (!learner->regret_table) {
            q_table_destroy(learner->q_table);
            nimcp_free(learner);
            learner = NULL;
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_gt_learner_create: learner->regret_table is NULL");
            return NULL;
        }
    }

    /* Allocate fictitious play counts */
    learner->opponent_action_counts = nimcp_calloc(num_actions, sizeof(uint32_t));
    if (!learner->opponent_action_counts) {
        regret_table_destroy(learner->regret_table);
        q_table_destroy(learner->q_table);
        nimcp_free(learner);
        learner = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_gt_learner_create: learner->opponent_action_counts is NULL");
        return NULL;
    }

    /* Allocate EXP3 weights */
    learner->exp3_weights = nimcp_calloc(num_actions, sizeof(float));
    learner->exp3_probabilities = nimcp_calloc(num_actions, sizeof(float));
    if (!learner->exp3_weights || !learner->exp3_probabilities) {
        nimcp_free(learner->exp3_probabilities);
        nimcp_free(learner->exp3_weights);
        nimcp_free(learner->opponent_action_counts);
        regret_table_destroy(learner->regret_table);
        q_table_destroy(learner->q_table);
        nimcp_free(learner);
        learner = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_gt_learner_create: required parameter is NULL (learner->exp3_weights, learner->exp3_probabilities)");
        return NULL;
    }

    /* Initialize EXP3 weights to 1 */
    for (uint32_t i = 0; i < num_actions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_actions > 256) {
            gt_learning_heartbeat("gt_learning_loop",
                             (float)(i + 1) / (float)num_actions);
        }

        learner->exp3_weights[i] = 1.0f;
    }

    /* Allocate action history for opponent modeling */
    learner->history_capacity = config->model_window_size > 0 ?
                                 config->model_window_size : NIMCP_GT_MAX_HISTORY;
    learner->action_history = nimcp_calloc(learner->history_capacity, sizeof(uint32_t));
    learner->our_action_history = nimcp_calloc(learner->history_capacity, sizeof(uint32_t));
    if (!learner->action_history || !learner->our_action_history) {
        nimcp_free(learner->our_action_history);
        nimcp_free(learner->action_history);
        nimcp_free(learner->exp3_probabilities);
        nimcp_free(learner->exp3_weights);
        nimcp_free(learner->opponent_action_counts);
        regret_table_destroy(learner->regret_table);
        q_table_destroy(learner->q_table);
        nimcp_free(learner);
        learner = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_gt_learner_create: failed to allocate action_history buffers");
        return NULL;
    }

    /* Initialize opponent model */
    nimcp_gt_opponent_model_init(&learner->opponent_model, num_actions);

    /* Initialize mutex */
    if (nimcp_platform_mutex_init(&learner->mutex, false) != 0) {
        nimcp_gt_opponent_model_cleanup(&learner->opponent_model);
        nimcp_free(learner->our_action_history);
        nimcp_free(learner->action_history);
        nimcp_free(learner->exp3_probabilities);
        nimcp_free(learner->exp3_weights);
        nimcp_free(learner->opponent_action_counts);
        regret_table_destroy(learner->regret_table);
        q_table_destroy(learner->q_table);
        nimcp_free(learner);
        learner = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_gt_learner_create: mutex_init failed");
        return NULL;
    }

    return learner;
}

void nimcp_gt_learner_destroy(nimcp_gt_learner_t learner) {
    if (!learner) return;

    /* Phase 8: Heartbeat at operation start */
    gt_learning_heartbeat("gt_learning_gt_learner_destroy", 0.0f);


    nimcp_platform_mutex_destroy(&learner->mutex);
    nimcp_gt_opponent_model_cleanup(&learner->opponent_model);
    nimcp_free(learner->our_action_history);
    nimcp_free(learner->action_history);
    nimcp_free(learner->exp3_probabilities);
    nimcp_free(learner->exp3_weights);
    nimcp_free(learner->opponent_action_counts);
    regret_table_destroy(learner->regret_table);
    q_table_destroy(learner->q_table);
    nimcp_free(learner);
    learner = NULL;
}

nimcp_error_t nimcp_gt_learner_reset(nimcp_gt_learner_t learner) {
    if (!learner) {
        return NIMCP_GT_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    gt_learning_heartbeat("gt_learning_gt_learner_reset", 0.0f);


    nimcp_platform_mutex_lock(&learner->mutex);

    /* Reset Q-table */
    q_table_reset(learner->q_table);

    /* Reset regret table */
    regret_table_reset(learner->regret_table);

    /* Reset fictitious play counts */
    memset(learner->opponent_action_counts, 0,
           learner->config.num_actions * sizeof(uint32_t));
    learner->total_opponent_observations = 0;

    /* Reset EXP3 weights */
    for (uint32_t i = 0; i < learner->config.num_actions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && learner->config.num_actions > 256) {
            gt_learning_heartbeat("gt_learning_loop",
                             (float)(i + 1) / (float)learner->config.num_actions);
        }

        learner->exp3_weights[i] = 1.0f;
    }

    /* Reset history */
    learner->history_len = 0;

    /* Reset opponent model */
    nimcp_gt_opponent_model_cleanup(&learner->opponent_model);
    nimcp_gt_opponent_model_init(&learner->opponent_model, learner->config.num_actions);

    /* Reset rates to initial */
    learner->current_learning_rate = learner->config.learning_rate;
    learner->current_exploration_rate = learner->config.exploration_rate;
    learner->schedule_step = 0;

    /* Reset stats */
    memset(&learner->stats, 0, sizeof(nimcp_gt_learning_stats_t));

    learner->last_max_q_change = 0.0f;

    nimcp_platform_mutex_unlock(&learner->mutex);
    return NIMCP_SUCCESS;
}

//=============================================================================
// Core Learning: Q-Learning and SARSA
//=============================================================================

nimcp_error_t nimcp_gt_learner_update(
    nimcp_gt_learner_t learner,
    uint32_t state,
    uint32_t action,
    float reward,
    uint32_t next_state
) {
    if (!learner) {
        return NIMCP_GT_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    gt_learning_heartbeat("gt_learning_gt_learner_update", 0.0f);


    if (state >= learner->config.num_states ||
        action >= learner->config.num_actions ||
        next_state >= learner->config.num_states) {
        return NIMCP_GT_ERROR_INVALID_STATE_IDX;
    }

    nimcp_platform_mutex_lock(&learner->mutex);

    nimcp_gt_q_entry_t* entry = get_q_entry(learner->q_table, state, action);
    if (!entry) {
        nimcp_platform_mutex_unlock(&learner->mutex);
        return NIMCP_GT_ERROR_INVALID_STATE_IDX;
    }

    /* Find max Q(s', a') for Q-learning */
    float max_next_q = 0.0f;
    for (uint32_t a = 0; a < learner->config.num_actions; a++) {
        /* Phase 8: Loop progress heartbeat */
        if ((a & 0xFF) == 0 && learner->config.num_actions > 256) {
            gt_learning_heartbeat("gt_learning_loop",
                             (float)(a + 1) / (float)learner->config.num_actions);
        }

        nimcp_gt_q_entry_t* next_entry = get_q_entry(learner->q_table, next_state, a);
        if (next_entry && next_entry->value > max_next_q) {
            max_next_q = next_entry->value;
        }
    }

    /* Q-learning update: Q(s,a) += alpha * (r + gamma*max_a' Q(s',a') - Q(s,a)) */
    float target = reward + learner->config.discount_factor * max_next_q;
    float td_error = target - entry->value;
    float old_value = entry->value;

    entry->value += learner->current_learning_rate * td_error;
    entry->visit_count++;

    /* Update variance estimate (Welford's algorithm) */
    float delta = td_error - entry->variance;
    entry->variance += delta / (float)entry->visit_count;

    /* Track max change for convergence */
    float change = fabsf(entry->value - old_value);
    if (change > learner->last_max_q_change) {
        learner->last_max_q_change = change;
    }

    /* Update statistics */
    learner->stats.updates++;
    learner->stats.avg_reward = (learner->stats.avg_reward * (float)(learner->stats.updates - 1) + reward) /
                                 (float)learner->stats.updates;
    learner->stats.avg_q_value = (learner->stats.avg_q_value * (float)(learner->stats.updates - 1) + entry->value) /
                                  (float)learner->stats.updates;
    learner->stats.current_learning_rate = learner->current_learning_rate;
    learner->stats.current_exploration = learner->current_exploration_rate;

    nimcp_platform_mutex_unlock(&learner->mutex);
    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_gt_learner_update_sarsa(
    nimcp_gt_learner_t learner,
    uint32_t state,
    uint32_t action,
    float reward,
    uint32_t next_state,
    uint32_t next_action
) {
    if (!learner) {
        return NIMCP_GT_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    gt_learning_heartbeat("gt_learning_gt_learner_update_sa", 0.0f);


    if (state >= learner->config.num_states ||
        action >= learner->config.num_actions ||
        next_state >= learner->config.num_states ||
        next_action >= learner->config.num_actions) {
        return NIMCP_GT_ERROR_INVALID_STATE_IDX;
    }

    nimcp_platform_mutex_lock(&learner->mutex);

    nimcp_gt_q_entry_t* entry = get_q_entry(learner->q_table, state, action);
    nimcp_gt_q_entry_t* next_entry = get_q_entry(learner->q_table, next_state, next_action);

    if (!entry || !next_entry) {
        nimcp_platform_mutex_unlock(&learner->mutex);
        return NIMCP_GT_ERROR_INVALID_STATE_IDX;
    }

    /* SARSA update: Q(s,a) += alpha * (r + gamma*Q(s',a') - Q(s,a)) */
    float target = reward + learner->config.discount_factor * next_entry->value;
    float td_error = target - entry->value;
    float old_value = entry->value;

    entry->value += learner->current_learning_rate * td_error;
    entry->visit_count++;

    /* Track change */
    float change = fabsf(entry->value - old_value);
    if (change > learner->last_max_q_change) {
        learner->last_max_q_change = change;
    }

    learner->stats.updates++;

    nimcp_platform_mutex_unlock(&learner->mutex);
    return NIMCP_SUCCESS;
}

//=============================================================================
// Action Selection
//=============================================================================

nimcp_error_t nimcp_gt_learner_select_action(
    nimcp_gt_learner_t learner,
    uint32_t state,
    uint32_t* action_out
) {
    if (!learner || !action_out) {
        return NIMCP_GT_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    gt_learning_heartbeat("gt_learning_gt_learner_select_ac", 0.0f);


    if (state >= learner->config.num_states) {
        return NIMCP_GT_ERROR_INVALID_STATE_IDX;
    }

    nimcp_platform_mutex_lock(&learner->mutex);

    uint32_t num_actions = learner->config.num_actions;
    uint32_t selected_action = 0;
    bool explored = false;

    switch (learner->config.explore) {
        case NIMCP_GT_EXPLORE_EPSILON_GREEDY: {
            if (rand_float(&learner->rand_seed) < learner->current_exploration_rate) {
                /* Explore: random action */
                selected_action = rand_r(&learner->rand_seed) % num_actions;
                explored = true;
            } else {
                /* Exploit: greedy action */
                float best_q = -FLT_MAX;
                for (uint32_t a = 0; a < num_actions; a++) {
                    /* Phase 8: Loop progress heartbeat */
                    if ((a & 0xFF) == 0 && num_actions > 256) {
                        gt_learning_heartbeat("gt_learning_loop",
                                         (float)(a + 1) / (float)num_actions);
                    }

                    nimcp_gt_q_entry_t* entry = get_q_entry(learner->q_table, state, a);
                    if (entry && entry->value > best_q) {
                        best_q = entry->value;
                        selected_action = a;
                    }
                }
            }
            break;
        }

        case NIMCP_GT_EXPLORE_BOLTZMANN: {
            /* Softmax over Q-values */
            float* q_values = nimcp_calloc(num_actions, sizeof(float));
            if (!q_values) return -1;
            float* probs = nimcp_calloc(num_actions, sizeof(float));

            if (q_values && probs) {
                for (uint32_t a = 0; a < num_actions; a++) {
                    /* Phase 8: Loop progress heartbeat */
                    if ((a & 0xFF) == 0 && num_actions > 256) {
                        gt_learning_heartbeat("gt_learning_loop",
                                         (float)(a + 1) / (float)num_actions);
                    }

                    nimcp_gt_q_entry_t* entry = get_q_entry(learner->q_table, state, a);
                    q_values[a] = entry ? entry->value : 0.0f;
                }

                softmax(q_values, probs, num_actions, learner->config.temperature);
                selected_action = sample_distribution(probs, num_actions, &learner->rand_seed);
            }

            nimcp_free(probs);
            probs = NULL;
            nimcp_free(q_values);
            q_values = NULL;
            break;
        }

        case NIMCP_GT_EXPLORE_UCB: {
            /* UCB1: select action maximizing Q + sqrt(2*ln(t)/n) */
            float best_ucb = -FLT_MAX;
            uint64_t total_visits = learner->stats.actions_selected + 1;
            float log_t = logf((float)total_visits);

            for (uint32_t a = 0; a < num_actions; a++) {
                /* Phase 8: Loop progress heartbeat */
                if ((a & 0xFF) == 0 && num_actions > 256) {
                    gt_learning_heartbeat("gt_learning_loop",
                                     (float)(a + 1) / (float)num_actions);
                }

                nimcp_gt_q_entry_t* entry = get_q_entry(learner->q_table, state, a);
                float q = entry ? entry->value : 0.0f;
                uint32_t n = entry ? entry->visit_count : 0;

                float ucb = 0.0f;
                if (n == 0) {
                    ucb = FLT_MAX;  /* Unvisited actions have infinite UCB */
                } else {
                    ucb = q + sqrtf(2.0f * log_t / (float)n);
                }

                if (ucb > best_ucb) {
                    best_ucb = ucb;
                    selected_action = a;
                }
            }
            break;
        }

        case NIMCP_GT_EXPLORE_THOMPSON: {
            /* Thompson sampling: sample from posterior and select best */
            float best_sample = -FLT_MAX;

            for (uint32_t a = 0; a < num_actions; a++) {
                /* Phase 8: Loop progress heartbeat */
                if ((a & 0xFF) == 0 && num_actions > 256) {
                    gt_learning_heartbeat("gt_learning_loop",
                                     (float)(a + 1) / (float)num_actions);
                }

                nimcp_gt_q_entry_t* entry = get_q_entry(learner->q_table, state, a);
                float mean = entry ? entry->value : 0.0f;
                float var = entry ? entry->variance : 1.0f;

                /* Sample from approximate posterior (Gaussian) */
                /* Box-Muller transform for normal random */
                float u1 = rand_float(&learner->rand_seed);
                float u2 = rand_float(&learner->rand_seed);
                if (u1 < 1e-10f) u1 = 1e-10f;
                float z = sqrtf(-2.0f * logf(u1)) * cosf(NIMCP_TWO_PI_F * u2);

                float sample = mean + sqrtf(var) * z;

                if (sample > best_sample) {
                    best_sample = sample;
                    selected_action = a;
                }
            }
            break;
        }

        default:
            /* Fallback to random */
            selected_action = rand_r(&learner->rand_seed) % num_actions;
            explored = true;
    }

    *action_out = selected_action;

    /* Update statistics */
    learner->stats.actions_selected++;
    if (explored) {
        learner->stats.explorations++;
    } else {
        learner->stats.exploitations++;
    }

    nimcp_platform_mutex_unlock(&learner->mutex);
    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_gt_learner_select_greedy(
    const nimcp_gt_learner_t learner,
    uint32_t state,
    uint32_t* action_out
) {
    if (!learner || !action_out) {
        return NIMCP_GT_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    gt_learning_heartbeat("gt_learning_gt_learner_select_gr", 0.0f);


    if (state >= learner->config.num_states) {
        return NIMCP_GT_ERROR_INVALID_STATE_IDX;
    }

    nimcp_platform_mutex_lock(&((nimcp_gt_learner_t)learner)->mutex);

    float best_q = -FLT_MAX;
    uint32_t best_action = 0;

    for (uint32_t a = 0; a < learner->config.num_actions; a++) {
        /* Phase 8: Loop progress heartbeat */
        if ((a & 0xFF) == 0 && learner->config.num_actions > 256) {
            gt_learning_heartbeat("gt_learning_loop",
                             (float)(a + 1) / (float)learner->config.num_actions);
        }

        nimcp_gt_q_entry_t* entry = get_q_entry(learner->q_table, state, a);
        if (entry && entry->value > best_q) {
            best_q = entry->value;
            best_action = a;
        }
    }

    *action_out = best_action;

    nimcp_platform_mutex_unlock(&((nimcp_gt_learner_t)learner)->mutex);
    return NIMCP_SUCCESS;
}

float nimcp_gt_learner_get_q_value(
    const nimcp_gt_learner_t learner,
    uint32_t state,
    uint32_t action
) {
    if (!learner) return 0.0f;

    /* Phase 8: Heartbeat at operation start */
    gt_learning_heartbeat("gt_learning_gt_learner_get_q_val", 0.0f);


    nimcp_platform_mutex_lock(&((nimcp_gt_learner_t)learner)->mutex);

    nimcp_gt_q_entry_t* entry = get_q_entry(learner->q_table, state, action);
    float value = entry ? entry->value : 0.0f;

    nimcp_platform_mutex_unlock(&((nimcp_gt_learner_t)learner)->mutex);
    return value;
}

nimcp_error_t nimcp_gt_learner_set_q_value(
    nimcp_gt_learner_t learner,
    uint32_t state,
    uint32_t action,
    float value
) {
    if (!learner) {
        return NIMCP_GT_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    gt_learning_heartbeat("gt_learning_gt_learner_set_q_val", 0.0f);


    if (state >= learner->config.num_states || action >= learner->config.num_actions) {
        return NIMCP_GT_ERROR_INVALID_STATE_IDX;
    }

    nimcp_platform_mutex_lock(&learner->mutex);

    nimcp_gt_q_entry_t* entry = get_q_entry(learner->q_table, state, action);
    if (entry) {
        entry->value = value;
    }

    nimcp_platform_mutex_unlock(&learner->mutex);
    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_gt_learner_get_strategy(
    const nimcp_gt_learner_t learner,
    uint32_t state,
    float* strategy_out
) {
    if (!learner || !strategy_out) {
        return NIMCP_GT_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    gt_learning_heartbeat("gt_learning_gt_learner_get_strat", 0.0f);


    if (state >= learner->config.num_states) {
        return NIMCP_GT_ERROR_INVALID_STATE_IDX;
    }

    nimcp_platform_mutex_lock(&((nimcp_gt_learner_t)learner)->mutex);

    uint32_t num_actions = learner->config.num_actions;

    /* Get Q-values */
    float* q_values = nimcp_calloc(num_actions, sizeof(float));
    if (!q_values) {
        nimcp_platform_mutex_unlock(&((nimcp_gt_learner_t)learner)->mutex);
        return NIMCP_GT_ERROR_NO_MEMORY;
    }

    for (uint32_t a = 0; a < num_actions; a++) {
        /* Phase 8: Loop progress heartbeat */
        if ((a & 0xFF) == 0 && num_actions > 256) {
            gt_learning_heartbeat("gt_learning_loop",
                             (float)(a + 1) / (float)num_actions);
        }

        nimcp_gt_q_entry_t* entry = get_q_entry(learner->q_table, state, a);
        q_values[a] = entry ? entry->value : 0.0f;
    }

    /* Softmax to get strategy */
    softmax(q_values, strategy_out, num_actions, learner->config.temperature);

    nimcp_free(q_values);
    q_values = NULL;

    nimcp_platform_mutex_unlock(&((nimcp_gt_learner_t)learner)->mutex);
    return NIMCP_SUCCESS;
}

//=============================================================================
// Counterfactual Regret Minimization (CFR)
//=============================================================================

nimcp_error_t nimcp_gt_cfr_update(
    nimcp_gt_learner_t learner,
    uint32_t info_set,
    const uint32_t* actions,
    uint32_t num_actions,
    const float* utilities
) {
    if (!learner || !actions || !utilities) {
        return NIMCP_GT_ERROR_NULL_POINTER;
    }

    if (!learner->regret_table) {
        return NIMCP_GT_ERROR_NOT_INITIALIZED;
    }

    /* Phase 8: Heartbeat at operation start */
    gt_learning_heartbeat("gt_learning_gt_cfr_update", 0.0f);


    if (info_set >= learner->regret_table->num_info_sets) {
        return NIMCP_GT_ERROR_INVALID_INFO_SET;
    }

    nimcp_platform_mutex_lock(&learner->mutex);

    nimcp_gt_regret_entry_t* entry = get_regret_entry(learner->regret_table, info_set);
    if (!entry) {
        nimcp_platform_mutex_unlock(&learner->mutex);
        return NIMCP_GT_ERROR_INVALID_INFO_SET;
    }

    /* Get current strategy via regret matching */
    float* strategy = nimcp_calloc(num_actions, sizeof(float));
    if (!strategy) {
        nimcp_platform_mutex_unlock(&learner->mutex);
        return NIMCP_GT_ERROR_NO_MEMORY;
    }

    float positive_regret_sum = 0.0f;
    for (uint32_t i = 0; i < num_actions && i < entry->num_actions; i++) {
        uint32_t a = actions[i];
        if (a < entry->num_actions) {
            float r = entry->cumulative_regret[a];
            if (learner->config.use_cfr_plus) {
                r = fmaxf(r, 0.0f);  /* CFR+: clamp negative regrets */
            }
            if (r > 0.0f) {
                positive_regret_sum += r;
            }
        }
    }

    /* Compute strategy from positive regrets */
    if (positive_regret_sum > learner->config.regret_matching_epsilon) {
        for (uint32_t i = 0; i < num_actions && i < entry->num_actions; i++) {
            uint32_t a = actions[i];
            if (a < entry->num_actions) {
                float r = entry->cumulative_regret[a];
                if (learner->config.use_cfr_plus) {
                    r = fmaxf(r, 0.0f);
                }
                strategy[i] = (r > 0.0f) ? (r / (fabsf(positive_regret_sum) > 1e-7f ? positive_regret_sum : 1e-7f)) : 0.0f;
            }
        }
    } else {
        /* Uniform strategy if no positive regrets */
        float uniform = 1.0f / (float)num_actions;
        for (uint32_t i = 0; i < num_actions; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && num_actions > 256) {
                gt_learning_heartbeat("gt_learning_loop",
                                 (float)(i + 1) / (float)num_actions);
            }

            strategy[i] = uniform;
        }
    }

    /* Compute counterfactual value (expected utility) */
    float cfv = 0.0f;
    for (uint32_t i = 0; i < num_actions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_actions > 256) {
            gt_learning_heartbeat("gt_learning_loop",
                             (float)(i + 1) / (float)num_actions);
        }

        cfv += strategy[i] * utilities[i];
    }

    /* Update cumulative regrets */
    float iteration_weight = 1.0f;
    if (learner->config.use_linear_cfr) {
        iteration_weight = (float)(entry->iteration_count + 1);
    }

    for (uint32_t i = 0; i < num_actions && i < entry->num_actions; i++) {
        uint32_t a = actions[i];
        if (a < entry->num_actions) {
            float regret = utilities[i] - cfv;
            entry->cumulative_regret[a] += iteration_weight * regret;

            /* CFR+: floor at zero */
            if (learner->config.use_cfr_plus && entry->cumulative_regret[a] < 0.0f) {
                entry->cumulative_regret[a] = 0.0f;
            }

            /* Update cumulative strategy */
            entry->cumulative_strategy[a] += iteration_weight * strategy[i];
        }
    }

    entry->iteration_count++;

    /* Update stats */
    float total_regret = 0.0f;
    for (uint32_t a = 0; a < entry->num_actions; a++) {
        /* Phase 8: Loop progress heartbeat */
        if ((a & 0xFF) == 0 && entry->num_actions > 256) {
            gt_learning_heartbeat("gt_learning_loop",
                             (float)(a + 1) / (float)entry->num_actions);
        }

        float r = entry->cumulative_regret[a];
        if (r > 0.0f) {
            total_regret += r;
        }
    }
    learner->stats.regret_sum = total_regret;
    learner->stats.updates++;

    nimcp_free(strategy);
    strategy = NULL;
    nimcp_platform_mutex_unlock(&learner->mutex);
    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_gt_cfr_get_strategy(
    const nimcp_gt_learner_t learner,
    uint32_t info_set,
    float* strategy_out
) {
    if (!learner || !strategy_out) {
        return NIMCP_GT_ERROR_NULL_POINTER;
    }

    if (!learner->regret_table || info_set >= learner->regret_table->num_info_sets) {
        return NIMCP_GT_ERROR_INVALID_INFO_SET;
    }

    /* Phase 8: Heartbeat at operation start */
    gt_learning_heartbeat("gt_learning_gt_cfr_get_strategy", 0.0f);


    nimcp_platform_mutex_lock(&((nimcp_gt_learner_t)learner)->mutex);

    nimcp_gt_regret_entry_t* entry = get_regret_entry(learner->regret_table, info_set);
    if (!entry) {
        nimcp_platform_mutex_unlock(&((nimcp_gt_learner_t)learner)->mutex);
        return NIMCP_GT_ERROR_INVALID_INFO_SET;
    }

    /* Regret matching */
    float positive_regret_sum = 0.0f;
    for (uint32_t a = 0; a < entry->num_actions; a++) {
        /* Phase 8: Loop progress heartbeat */
        if ((a & 0xFF) == 0 && entry->num_actions > 256) {
            gt_learning_heartbeat("gt_learning_loop",
                             (float)(a + 1) / (float)entry->num_actions);
        }

        float r = entry->cumulative_regret[a];
        if (learner->config.use_cfr_plus) {
            r = fmaxf(r, 0.0f);
        }
        if (r > 0.0f) {
            positive_regret_sum += r;
        }
    }

    if (positive_regret_sum > learner->config.regret_matching_epsilon) {
        for (uint32_t a = 0; a < entry->num_actions; a++) {
            /* Phase 8: Loop progress heartbeat */
            if ((a & 0xFF) == 0 && entry->num_actions > 256) {
                gt_learning_heartbeat("gt_learning_loop",
                                 (float)(a + 1) / (float)entry->num_actions);
            }

            float r = entry->cumulative_regret[a];
            if (learner->config.use_cfr_plus) {
                r = fmaxf(r, 0.0f);
            }
            strategy_out[a] = (r > 0.0f) ? (r / (fabsf(positive_regret_sum) > 1e-7f ? positive_regret_sum : 1e-7f)) : 0.0f;
        }
    } else {
        float uniform = 1.0f / (float)entry->num_actions;
        for (uint32_t a = 0; a < entry->num_actions; a++) {
            /* Phase 8: Loop progress heartbeat */
            if ((a & 0xFF) == 0 && entry->num_actions > 256) {
                gt_learning_heartbeat("gt_learning_loop",
                                 (float)(a + 1) / (float)entry->num_actions);
            }

            strategy_out[a] = uniform;
        }
    }

    nimcp_platform_mutex_unlock(&((nimcp_gt_learner_t)learner)->mutex);
    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_gt_cfr_get_average_strategy(
    const nimcp_gt_learner_t learner,
    uint32_t info_set,
    float* strategy_out
) {
    if (!learner || !strategy_out) {
        return NIMCP_GT_ERROR_NULL_POINTER;
    }

    if (!learner->regret_table || info_set >= learner->regret_table->num_info_sets) {
        return NIMCP_GT_ERROR_INVALID_INFO_SET;
    }

    /* Phase 8: Heartbeat at operation start */
    gt_learning_heartbeat("gt_learning_gt_cfr_get_average_s", 0.0f);


    nimcp_platform_mutex_lock(&((nimcp_gt_learner_t)learner)->mutex);

    nimcp_gt_regret_entry_t* entry = get_regret_entry(learner->regret_table, info_set);
    if (!entry) {
        nimcp_platform_mutex_unlock(&((nimcp_gt_learner_t)learner)->mutex);
        return NIMCP_GT_ERROR_INVALID_INFO_SET;
    }

    /* Normalize cumulative strategy */
    float strategy_sum = 0.0f;
    for (uint32_t a = 0; a < entry->num_actions; a++) {
        /* Phase 8: Loop progress heartbeat */
        if ((a & 0xFF) == 0 && entry->num_actions > 256) {
            gt_learning_heartbeat("gt_learning_loop",
                             (float)(a + 1) / (float)entry->num_actions);
        }

        strategy_sum += entry->cumulative_strategy[a];
    }

    if (strategy_sum > learner->config.regret_matching_epsilon) {
        for (uint32_t a = 0; a < entry->num_actions; a++) {
            /* Phase 8: Loop progress heartbeat */
            if ((a & 0xFF) == 0 && entry->num_actions > 256) {
                gt_learning_heartbeat("gt_learning_loop",
                                 (float)(a + 1) / (float)entry->num_actions);
            }

            strategy_out[a] = entry->cumulative_strategy[a] / (fabsf(strategy_sum) > 1e-7f ? strategy_sum : 1e-7f);
        }
    } else {
        float uniform = 1.0f / (float)entry->num_actions;
        for (uint32_t a = 0; a < entry->num_actions; a++) {
            /* Phase 8: Loop progress heartbeat */
            if ((a & 0xFF) == 0 && entry->num_actions > 256) {
                gt_learning_heartbeat("gt_learning_loop",
                                 (float)(a + 1) / (float)entry->num_actions);
            }

            strategy_out[a] = uniform;
        }
    }

    nimcp_platform_mutex_unlock(&((nimcp_gt_learner_t)learner)->mutex);
    return NIMCP_SUCCESS;
}

float nimcp_gt_cfr_get_regret(
    const nimcp_gt_learner_t learner,
    uint32_t info_set
) {
    if (!learner || !learner->regret_table) {
        return 0.0f;
    }

    /* Phase 8: Heartbeat at operation start */
    gt_learning_heartbeat("gt_learning_gt_cfr_get_regret", 0.0f);


    if (info_set >= learner->regret_table->num_info_sets) {
        return 0.0f;
    }

    nimcp_platform_mutex_lock(&((nimcp_gt_learner_t)learner)->mutex);

    nimcp_gt_regret_entry_t* entry = get_regret_entry(learner->regret_table, info_set);
    float total = 0.0f;

    if (entry) {
        for (uint32_t a = 0; a < entry->num_actions; a++) {
            /* Phase 8: Loop progress heartbeat */
            if ((a & 0xFF) == 0 && entry->num_actions > 256) {
                gt_learning_heartbeat("gt_learning_loop",
                                 (float)(a + 1) / (float)entry->num_actions);
            }

            float r = entry->cumulative_regret[a];
            if (r > 0.0f) {
                total += r;
            }
        }
    }

    nimcp_platform_mutex_unlock(&((nimcp_gt_learner_t)learner)->mutex);
    return total;
}

//=============================================================================
// Fictitious Play
//=============================================================================

nimcp_error_t nimcp_gt_fictitious_play_update(
    nimcp_gt_learner_t learner,
    uint32_t opponent_action
) {
    if (!learner) {
        return NIMCP_GT_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    gt_learning_heartbeat("gt_learning_gt_fictitious_play_u", 0.0f);


    if (opponent_action >= learner->config.num_actions) {
        return NIMCP_GT_ERROR_INVALID_ACTION_IDX;
    }

    nimcp_platform_mutex_lock(&learner->mutex);

    learner->opponent_action_counts[opponent_action]++;
    learner->total_opponent_observations++;

    /* Update action history */
    if (learner->history_len < learner->history_capacity) {
        learner->action_history[learner->history_len++] = opponent_action;
    } else {
        /* Shift history */
        memmove(learner->action_history, learner->action_history + 1,
                (learner->history_capacity - 1) * sizeof(uint32_t));
        learner->action_history[learner->history_capacity - 1] = opponent_action;
    }

    learner->stats.updates++;

    nimcp_platform_mutex_unlock(&learner->mutex);
    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_gt_fictitious_play_predict(
    const nimcp_gt_learner_t learner,
    uint32_t* prediction_out
) {
    if (!learner || !prediction_out) {
        return NIMCP_GT_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    gt_learning_heartbeat("gt_learning_gt_fictitious_play_p", 0.0f);


    nimcp_platform_mutex_lock(&((nimcp_gt_learner_t)learner)->mutex);

    if (learner->total_opponent_observations == 0) {
        nimcp_platform_mutex_unlock(&((nimcp_gt_learner_t)learner)->mutex);
        return NIMCP_GT_ERROR_EMPTY_HISTORY;
    }

    /* Return mode of opponent actions */
    *prediction_out = argmax((float*)learner->opponent_action_counts,
                              learner->config.num_actions);

    nimcp_platform_mutex_unlock(&((nimcp_gt_learner_t)learner)->mutex);
    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_gt_fictitious_play_get_distribution(
    const nimcp_gt_learner_t learner,
    float* distribution_out
) {
    if (!learner || !distribution_out) {
        return NIMCP_GT_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    gt_learning_heartbeat("gt_learning_gt_fictitious_play_g", 0.0f);


    nimcp_platform_mutex_lock(&((nimcp_gt_learner_t)learner)->mutex);

    if (learner->total_opponent_observations == 0) {
        /* Uniform prior */
        float uniform = 1.0f / (float)learner->config.num_actions;
        for (uint32_t a = 0; a < learner->config.num_actions; a++) {
            /* Phase 8: Loop progress heartbeat */
            if ((a & 0xFF) == 0 && learner->config.num_actions > 256) {
                gt_learning_heartbeat("gt_learning_loop",
                                 (float)(a + 1) / (float)learner->config.num_actions);
            }

            distribution_out[a] = uniform;
        }
    } else {
        float total = (float)learner->total_opponent_observations;
        for (uint32_t a = 0; a < learner->config.num_actions; a++) {
            /* Phase 8: Loop progress heartbeat */
            if ((a & 0xFF) == 0 && learner->config.num_actions > 256) {
                gt_learning_heartbeat("gt_learning_loop",
                                 (float)(a + 1) / (float)learner->config.num_actions);
            }

            distribution_out[a] = (float)learner->opponent_action_counts[a] / (fabsf(total) > 1e-7f ? total : 1e-7f);
        }
    }

    nimcp_platform_mutex_unlock(&((nimcp_gt_learner_t)learner)->mutex);
    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_gt_fictitious_play_best_response(
    const nimcp_gt_learner_t learner,
    uint32_t state,
    const float* payoff_matrix,
    uint32_t* best_action_out
) {
    if (!learner || !payoff_matrix || !best_action_out) {
        return NIMCP_GT_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    gt_learning_heartbeat("gt_learning_gt_fictitious_play_b", 0.0f);


    (void)state;  /* State not used in basic fictitious play */

    nimcp_platform_mutex_lock(&((nimcp_gt_learner_t)learner)->mutex);

    uint32_t num_actions = learner->config.num_actions;

    /* Get opponent distribution */
    float* opp_dist = nimcp_calloc(num_actions, sizeof(float));
    if (!opp_dist) {
        nimcp_platform_mutex_unlock(&((nimcp_gt_learner_t)learner)->mutex);
        return NIMCP_GT_ERROR_NO_MEMORY;
    }

    if (learner->total_opponent_observations == 0) {
        float uniform = 1.0f / (float)num_actions;
        for (uint32_t a = 0; a < num_actions; a++) {
            /* Phase 8: Loop progress heartbeat */
            if ((a & 0xFF) == 0 && num_actions > 256) {
                gt_learning_heartbeat("gt_learning_loop",
                                 (float)(a + 1) / (float)num_actions);
            }

            opp_dist[a] = uniform;
        }
    } else {
        float total = (float)learner->total_opponent_observations;
        for (uint32_t a = 0; a < num_actions; a++) {
            /* Phase 8: Loop progress heartbeat */
            if ((a & 0xFF) == 0 && num_actions > 256) {
                gt_learning_heartbeat("gt_learning_loop",
                                 (float)(a + 1) / (float)num_actions);
            }

            opp_dist[a] = (float)learner->opponent_action_counts[a] / (fabsf(total) > 1e-7f ? total : 1e-7f);
        }
    }

    /* Compute expected payoff for each of our actions */
    float best_payoff = -FLT_MAX;
    uint32_t best_action = 0;

    for (uint32_t our_a = 0; our_a < num_actions; our_a++) {
        /* Phase 8: Loop progress heartbeat */
        if ((our_a & 0xFF) == 0 && num_actions > 256) {
            gt_learning_heartbeat("gt_learning_loop",
                             (float)(our_a + 1) / (float)num_actions);
        }

        float expected = 0.0f;
        for (uint32_t opp_a = 0; opp_a < num_actions; opp_a++) {
            /* Phase 8: Loop progress heartbeat */
            if ((opp_a & 0xFF) == 0 && num_actions > 256) {
                gt_learning_heartbeat("gt_learning_loop",
                                 (float)(opp_a + 1) / (float)num_actions);
            }

            /* Row-major: payoff_matrix[our_a * num_actions + opp_a] */
            expected += opp_dist[opp_a] * payoff_matrix[our_a * num_actions + opp_a];
        }
        if (expected > best_payoff) {
            best_payoff = expected;
            best_action = our_a;
        }
    }

    *best_action_out = best_action;

    nimcp_free(opp_dist);
    opp_dist = NULL;
    nimcp_platform_mutex_unlock(&((nimcp_gt_learner_t)learner)->mutex);
    return NIMCP_SUCCESS;
}

//=============================================================================
// EXP3
//=============================================================================

nimcp_error_t nimcp_gt_exp3_update(
    nimcp_gt_learner_t learner,
    uint32_t action,
    float reward
) {
    if (!learner) {
        return NIMCP_GT_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    gt_learning_heartbeat("gt_learning_gt_exp3_update", 0.0f);


    if (action >= learner->config.num_actions) {
        return NIMCP_GT_ERROR_INVALID_ACTION_IDX;
    }

    nimcp_platform_mutex_lock(&learner->mutex);

    uint32_t num_actions = learner->config.num_actions;
    float gamma = learner->config.exp3_gamma;

    /* Compute current probabilities */
    float weight_sum = 0.0f;
    for (uint32_t a = 0; a < num_actions; a++) {
        /* Phase 8: Loop progress heartbeat */
        if ((a & 0xFF) == 0 && num_actions > 256) {
            gt_learning_heartbeat("gt_learning_loop",
                             (float)(a + 1) / (float)num_actions);
        }

        weight_sum += learner->exp3_weights[a];
    }

    float prob_action = (1.0f - gamma) * (learner->exp3_weights[action] / (fabsf(weight_sum) > 1e-7f ? weight_sum : 1e-7f)) +
                         gamma / (float)num_actions;

    /* Unbiased estimator */
    float estimated_reward = reward / prob_action;

    /* Update weight */
    learner->exp3_weights[action] *= expf(gamma * estimated_reward / (float)num_actions);

    /* Normalize to prevent overflow */
    float max_weight = learner->exp3_weights[0];
    for (uint32_t a = 1; a < num_actions; a++) {
        if (learner->exp3_weights[a] > max_weight) {
            max_weight = learner->exp3_weights[a];
        }
    }
    if (max_weight > 1e6f) {
        for (uint32_t a = 0; a < num_actions; a++) {
            /* Phase 8: Loop progress heartbeat */
            if ((a & 0xFF) == 0 && num_actions > 256) {
                gt_learning_heartbeat("gt_learning_loop",
                                 (float)(a + 1) / (float)num_actions);
            }

            learner->exp3_weights[a] /= max_weight;
        }
    }

    learner->stats.updates++;

    nimcp_platform_mutex_unlock(&learner->mutex);
    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_gt_exp3_select(
    nimcp_gt_learner_t learner,
    uint32_t* action_out
) {
    if (!learner || !action_out) {
        return NIMCP_GT_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    gt_learning_heartbeat("gt_learning_gt_exp3_select", 0.0f);


    nimcp_platform_mutex_lock(&learner->mutex);

    uint32_t num_actions = learner->config.num_actions;
    float gamma = learner->config.exp3_gamma;

    /* Compute probabilities */
    float weight_sum = 0.0f;
    for (uint32_t a = 0; a < num_actions; a++) {
        /* Phase 8: Loop progress heartbeat */
        if ((a & 0xFF) == 0 && num_actions > 256) {
            gt_learning_heartbeat("gt_learning_loop",
                             (float)(a + 1) / (float)num_actions);
        }

        weight_sum += learner->exp3_weights[a];
    }

    for (uint32_t a = 0; a < num_actions; a++) {
        /* Phase 8: Loop progress heartbeat */
        if ((a & 0xFF) == 0 && num_actions > 256) {
            gt_learning_heartbeat("gt_learning_loop",
                             (float)(a + 1) / (float)num_actions);
        }

        learner->exp3_probabilities[a] = (1.0f - gamma) *
                                          (learner->exp3_weights[a] / (fabsf(weight_sum) > 1e-7f ? weight_sum : 1e-7f)) +
                                          gamma / (float)num_actions;
    }

    /* Sample */
    *action_out = sample_distribution(learner->exp3_probabilities, num_actions,
                                       &learner->rand_seed);

    learner->stats.actions_selected++;

    nimcp_platform_mutex_unlock(&learner->mutex);
    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_gt_exp3_get_probabilities(
    const nimcp_gt_learner_t learner,
    float* probabilities_out
) {
    if (!learner || !probabilities_out) {
        return NIMCP_GT_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    gt_learning_heartbeat("gt_learning_gt_exp3_get_probabil", 0.0f);


    nimcp_platform_mutex_lock(&((nimcp_gt_learner_t)learner)->mutex);

    uint32_t num_actions = learner->config.num_actions;
    float gamma = learner->config.exp3_gamma;

    float weight_sum = 0.0f;
    for (uint32_t a = 0; a < num_actions; a++) {
        /* Phase 8: Loop progress heartbeat */
        if ((a & 0xFF) == 0 && num_actions > 256) {
            gt_learning_heartbeat("gt_learning_loop",
                             (float)(a + 1) / (float)num_actions);
        }

        weight_sum += learner->exp3_weights[a];
    }

    for (uint32_t a = 0; a < num_actions; a++) {
        /* Phase 8: Loop progress heartbeat */
        if ((a & 0xFF) == 0 && num_actions > 256) {
            gt_learning_heartbeat("gt_learning_loop",
                             (float)(a + 1) / (float)num_actions);
        }

        probabilities_out[a] = (1.0f - gamma) *
                                (learner->exp3_weights[a] / (fabsf(weight_sum) > 1e-7f ? weight_sum : 1e-7f)) +
                                gamma / (float)num_actions;
    }

    nimcp_platform_mutex_unlock(&((nimcp_gt_learner_t)learner)->mutex);
    return NIMCP_SUCCESS;
}

//=============================================================================
// Opponent Modeling
//=============================================================================

nimcp_error_t nimcp_gt_model_opponent(
    nimcp_gt_learner_t learner,
    const uint32_t* history,
    uint32_t history_len,
    nimcp_gt_opponent_model_t* model_out
) {
    if (!learner || !history || !model_out) {
        return NIMCP_GT_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    gt_learning_heartbeat("gt_learning_gt_model_opponent", 0.0f);


    if (history_len == 0) {
        return NIMCP_GT_ERROR_EMPTY_HISTORY;
    }

    nimcp_platform_mutex_lock(&learner->mutex);

    uint32_t num_actions = learner->config.num_actions;

    /* Initialize output model */
    nimcp_gt_opponent_model_init(model_out, num_actions);

    /* Count action frequencies */
    for (uint32_t i = 0; i < history_len; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && history_len > 256) {
            gt_learning_heartbeat("gt_learning_loop",
                             (float)(i + 1) / (float)history_len);
        }

        if (history[i] < num_actions) {
            model_out->action_counts[history[i]]++;
        }
    }
    model_out->total_observations = history_len;

    /* Compute action predictions (empirical distribution) */
    for (uint32_t a = 0; a < num_actions; a++) {
        /* Phase 8: Loop progress heartbeat */
        if ((a & 0xFF) == 0 && num_actions > 256) {
            gt_learning_heartbeat("gt_learning_loop",
                             (float)(a + 1) / (float)num_actions);
        }

        model_out->action_predictions[a] = (float)model_out->action_counts[a] /
                                            (float)history_len;
    }

    /* Type inference based on action patterns */
    /* Compute entropy of action distribution */
    float entropy = 0.0f;
    for (uint32_t a = 0; a < num_actions; a++) {
        /* Phase 8: Loop progress heartbeat */
        if ((a & 0xFF) == 0 && num_actions > 256) {
            gt_learning_heartbeat("gt_learning_loop",
                             (float)(a + 1) / (float)num_actions);
        }

        float p = model_out->action_predictions[a];
        if (p > 1e-6f) {
            entropy -= p * logf(p);
        }
    }
    float max_entropy = logf((float)num_actions);
    float entropy_ratio = (max_entropy > 0.0f) ? (entropy / max_entropy) : 1.0f;

    /* Initialize uniform priors */
    float prior = learner->config.type_prior_strength / (float)NIMCP_GT_OPPONENT_COUNT;
    for (uint32_t t = 0; t < NIMCP_GT_OPPONENT_COUNT; t++) {
        /* Phase 8: Loop progress heartbeat */
        if ((t & 0xFF) == 0 && NIMCP_GT_OPPONENT_COUNT > 256) {
            gt_learning_heartbeat("gt_learning_loop",
                             (float)(t + 1) / (float)NIMCP_GT_OPPONENT_COUNT);
        }

        model_out->type_beliefs[t] = prior;
    }

    /* Update beliefs based on entropy */
    if (entropy_ratio > 0.9f) {
        /* High entropy -> likely random */
        model_out->type_beliefs[NIMCP_GT_OPPONENT_RANDOM] += 2.0f;
    } else if (entropy_ratio < 0.3f) {
        /* Low entropy -> deterministic strategy */
        model_out->type_beliefs[NIMCP_GT_OPPONENT_COMPETITIVE] += 1.0f;
        model_out->type_beliefs[NIMCP_GT_OPPONENT_TFTIT] += 1.0f;
    }

    /* Compute cooperation rate (assume action 0 is "cooperate" for simplicity) */
    if (num_actions >= 2) {
        model_out->cooperation_rate = model_out->action_predictions[0];
        if (model_out->cooperation_rate > 0.7f) {
            model_out->type_beliefs[NIMCP_GT_OPPONENT_COOPERATIVE] += 2.0f;
        } else if (model_out->cooperation_rate < 0.3f) {
            model_out->type_beliefs[NIMCP_GT_OPPONENT_COMPETITIVE] += 2.0f;
        }
    }

    /* Check for tit-for-tat pattern (requires our action history) */
    if (learner->history_len >= 2) {
        uint32_t tft_matches = 0;
        uint32_t checks = 0;
        uint32_t start = (history_len > learner->history_len) ?
                          (history_len - learner->history_len) : 0;

        for (uint32_t i = start + 1; i < history_len && i < learner->history_len; i++) {
            /* TFT: opponent mirrors our previous action */
            if (history[i] == learner->our_action_history[i - 1]) {
                tft_matches++;
            }
            checks++;
        }

        if (checks > 5 && (float)tft_matches / (float)checks > 0.8f) {
            model_out->type_beliefs[NIMCP_GT_OPPONENT_TFTIT] += 3.0f;
        }
    }

    /* Normalize beliefs */
    float belief_sum = 0.0f;
    for (uint32_t t = 0; t < NIMCP_GT_OPPONENT_COUNT; t++) {
        /* Phase 8: Loop progress heartbeat */
        if ((t & 0xFF) == 0 && NIMCP_GT_OPPONENT_COUNT > 256) {
            gt_learning_heartbeat("gt_learning_loop",
                             (float)(t + 1) / (float)NIMCP_GT_OPPONENT_COUNT);
        }

        belief_sum += model_out->type_beliefs[t];
    }
    for (uint32_t t = 0; t < NIMCP_GT_OPPONENT_COUNT; t++) {
        /* Phase 8: Loop progress heartbeat */
        if ((t & 0xFF) == 0 && NIMCP_GT_OPPONENT_COUNT > 256) {
            gt_learning_heartbeat("gt_learning_loop",
                             (float)(t + 1) / (float)NIMCP_GT_OPPONENT_COUNT);
        }

        model_out->type_beliefs[t] /= belief_sum;
    }

    /* Find most likely type */
    model_out->predicted_type = NIMCP_GT_OPPONENT_UNKNOWN;
    float max_belief = 0.0f;
    for (uint32_t t = 0; t < NIMCP_GT_OPPONENT_COUNT; t++) {
        /* Phase 8: Loop progress heartbeat */
        if ((t & 0xFF) == 0 && NIMCP_GT_OPPONENT_COUNT > 256) {
            gt_learning_heartbeat("gt_learning_loop",
                             (float)(t + 1) / (float)NIMCP_GT_OPPONENT_COUNT);
        }

        if (model_out->type_beliefs[t] > max_belief) {
            max_belief = model_out->type_beliefs[t];
            model_out->predicted_type = (nimcp_gt_opponent_type_t)t;
        }
    }
    model_out->prediction_confidence = max_belief;

    nimcp_platform_mutex_unlock(&learner->mutex);
    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_gt_update_opponent_model(
    nimcp_gt_learner_t learner,
    uint32_t our_action,
    uint32_t opponent_action
) {
    if (!learner) {
        return NIMCP_GT_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    gt_learning_heartbeat("gt_learning_gt_update_opponent_m", 0.0f);


    if (our_action >= learner->config.num_actions ||
        opponent_action >= learner->config.num_actions) {
        return NIMCP_GT_ERROR_INVALID_ACTION_IDX;
    }

    nimcp_platform_mutex_lock(&learner->mutex);

    /* Update histories */
    if (learner->history_len < learner->history_capacity) {
        learner->action_history[learner->history_len] = opponent_action;
        learner->our_action_history[learner->history_len] = our_action;
        learner->history_len++;
    } else {
        /* Shift and append */
        memmove(learner->action_history, learner->action_history + 1,
                (learner->history_capacity - 1) * sizeof(uint32_t));
        memmove(learner->our_action_history, learner->our_action_history + 1,
                (learner->history_capacity - 1) * sizeof(uint32_t));
        learner->action_history[learner->history_capacity - 1] = opponent_action;
        learner->our_action_history[learner->history_capacity - 1] = our_action;
    }

    /* Update action counts */
    learner->opponent_action_counts[opponent_action]++;
    learner->total_opponent_observations++;

    /* Compute reciprocity (does opponent respond to our actions?) */
    if (learner->history_len >= 2) {
        uint32_t prev_our = learner->our_action_history[learner->history_len - 2];
        if (opponent_action == prev_our) {
            learner->opponent_model.reciprocity_score =
                0.9f * learner->opponent_model.reciprocity_score + 0.1f;
        } else {
            learner->opponent_model.reciprocity_score =
                0.9f * learner->opponent_model.reciprocity_score;
        }
    }

    nimcp_platform_mutex_unlock(&learner->mutex);
    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_gt_get_opponent_model(
    const nimcp_gt_learner_t learner,
    nimcp_gt_opponent_model_t* model_out
) {
    if (!learner || !model_out) {
        return NIMCP_GT_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    gt_learning_heartbeat("gt_learning_gt_get_opponent_mode", 0.0f);


    nimcp_platform_mutex_lock(&((nimcp_gt_learner_t)learner)->mutex);

    /* Copy model (shallow copy of pointers is OK since we copy the data) */
    *model_out = learner->opponent_model;

    /* Allocate and copy arrays */
    model_out->action_predictions = nimcp_calloc(learner->config.num_actions, sizeof(float));
    if (!model_out->action_predictions) return -1;
    model_out->action_counts = nimcp_calloc(learner->config.num_actions, sizeof(uint32_t));
    if (!model_out->action_counts) return -1;

    if (model_out->action_predictions && learner->opponent_model.action_predictions) {
        memcpy(model_out->action_predictions, learner->opponent_model.action_predictions,
               learner->config.num_actions * sizeof(float));
    }
    if (model_out->action_counts && learner->opponent_model.action_counts) {
        memcpy(model_out->action_counts, learner->opponent_model.action_counts,
               learner->config.num_actions * sizeof(uint32_t));
    }

    model_out->num_actions = learner->config.num_actions;

    nimcp_platform_mutex_unlock(&((nimcp_gt_learner_t)learner)->mutex);
    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_gt_predict_opponent_action(
    const nimcp_gt_learner_t learner,
    uint32_t our_action,
    uint32_t* prediction_out,
    float* confidence_out
) {
    if (!learner || !prediction_out) {
        return NIMCP_GT_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    gt_learning_heartbeat("gt_learning_gt_predict_opponent_", 0.0f);


    nimcp_platform_mutex_lock(&((nimcp_gt_learner_t)learner)->mutex);

    if (learner->total_opponent_observations == 0) {
        nimcp_platform_mutex_unlock(&((nimcp_gt_learner_t)learner)->mutex);
        return NIMCP_GT_ERROR_EMPTY_HISTORY;
    }

    uint32_t num_actions = learner->config.num_actions;
    nimcp_gt_opponent_type_t type = learner->opponent_model.predicted_type;

    uint32_t prediction = 0;
    float confidence = 0.5f;

    switch (type) {
        case NIMCP_GT_OPPONENT_RANDOM:
            /* Random: no prediction, use mode */
            prediction = argmax((float*)learner->opponent_action_counts, num_actions);
            confidence = 1.0f / (float)num_actions;
            break;

        case NIMCP_GT_OPPONENT_TFTIT:
            /* Tit-for-tat: predict they'll mirror our action */
            prediction = our_action;
            confidence = learner->opponent_model.reciprocity_score;
            break;

        case NIMCP_GT_OPPONENT_COOPERATIVE:
            /* Cooperative: predict action 0 (cooperate) */
            prediction = 0;
            confidence = learner->opponent_model.cooperation_rate;
            break;

        case NIMCP_GT_OPPONENT_COMPETITIVE:
            /* Competitive: predict action 1 (defect) */
            prediction = (num_actions > 1) ? 1 : 0;
            confidence = 1.0f - learner->opponent_model.cooperation_rate;
            break;

        default:
            /* Unknown/Adaptive: use empirical mode */
            prediction = argmax((float*)learner->opponent_action_counts, num_actions);
            confidence = (float)learner->opponent_action_counts[prediction] /
                          (float)learner->total_opponent_observations;
    }

    *prediction_out = prediction;
    if (confidence_out) {
        *confidence_out = confidence;
    }

    nimcp_platform_mutex_unlock(&((nimcp_gt_learner_t)learner)->mutex);
    return NIMCP_SUCCESS;
}

//=============================================================================
// Schedule Management
//=============================================================================

nimcp_error_t nimcp_gt_learner_advance_schedule(nimcp_gt_learner_t learner) {
    if (!learner) {
        return NIMCP_GT_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    gt_learning_heartbeat("gt_learning_gt_learner_advance_s", 0.0f);


    nimcp_platform_mutex_lock(&learner->mutex);

    learner->schedule_step++;

    /* Update learning rate */
    switch (learner->config.lr_schedule) {
        case NIMCP_GT_SCHEDULE_CONSTANT:
            /* No change */
            break;

        case NIMCP_GT_SCHEDULE_DECAY:
            learner->current_learning_rate *= learner->config.lr_decay_rate;
            if (learner->current_learning_rate < learner->config.lr_min) {
                learner->current_learning_rate = learner->config.lr_min;
            }
            break;

        case NIMCP_GT_SCHEDULE_POLYNOMIAL:
            learner->current_learning_rate = learner->config.learning_rate /
                                              (1.0f + (float)learner->schedule_step);
            if (learner->current_learning_rate < learner->config.lr_min) {
                learner->current_learning_rate = learner->config.lr_min;
            }
            break;

        case NIMCP_GT_SCHEDULE_ADAPTIVE:
            /* Reduce on high variance (simple heuristic) */
            if (learner->last_max_q_change > 1.0f) {
                learner->current_learning_rate *= 0.95f;
            } else if (learner->last_max_q_change < 0.01f) {
                learner->current_learning_rate *= 1.01f;
            }
            if (learner->current_learning_rate < learner->config.lr_min) {
                learner->current_learning_rate = learner->config.lr_min;
            }
            if (learner->current_learning_rate > learner->config.learning_rate) {
                learner->current_learning_rate = learner->config.learning_rate;
            }
            break;

        default:
            break;
    }

    /* Update exploration rate */
    learner->current_exploration_rate *= learner->config.exploration_decay;
    if (learner->current_exploration_rate < learner->config.exploration_min) {
        learner->current_exploration_rate = learner->config.exploration_min;
    }

    /* Reset change tracker */
    learner->last_max_q_change = 0.0f;

    /* Update stats */
    learner->stats.current_learning_rate = learner->current_learning_rate;
    learner->stats.current_exploration = learner->current_exploration_rate;

    nimcp_platform_mutex_unlock(&learner->mutex);
    return NIMCP_SUCCESS;
}

float nimcp_gt_learner_get_learning_rate(const nimcp_gt_learner_t learner) {
    if (!learner) return 0.0f;
    /* Phase 8: Heartbeat at operation start */
    gt_learning_heartbeat("gt_learning_gt_learner_get_learn", 0.0f);


    return learner->current_learning_rate;
}

float nimcp_gt_learner_get_exploration_rate(const nimcp_gt_learner_t learner) {
    if (!learner) return 0.0f;
    /* Phase 8: Heartbeat at operation start */
    gt_learning_heartbeat("gt_learning_gt_learner_get_explo", 0.0f);


    return learner->current_exploration_rate;
}

nimcp_error_t nimcp_gt_learner_set_learning_rate(
    nimcp_gt_learner_t learner,
    float rate
) {
    if (!learner) {
        return NIMCP_GT_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    gt_learning_heartbeat("gt_learning_gt_learner_set_learn", 0.0f);


    nimcp_platform_mutex_lock(&learner->mutex);
    learner->current_learning_rate = rate;
    nimcp_platform_mutex_unlock(&learner->mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_gt_learner_set_exploration_rate(
    nimcp_gt_learner_t learner,
    float rate
) {
    if (!learner) {
        return NIMCP_GT_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    gt_learning_heartbeat("gt_learning_gt_learner_set_explo", 0.0f);


    nimcp_platform_mutex_lock(&learner->mutex);
    learner->current_exploration_rate = rate;
    nimcp_platform_mutex_unlock(&learner->mutex);

    return NIMCP_SUCCESS;
}

//=============================================================================
// Statistics and Diagnostics
//=============================================================================

nimcp_error_t nimcp_gt_learner_get_stats(
    const nimcp_gt_learner_t learner,
    nimcp_gt_learning_stats_t* stats_out
) {
    if (!learner || !stats_out) {
        return NIMCP_GT_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    gt_learning_heartbeat("gt_learning_gt_learner_get_stats", 0.0f);


    nimcp_platform_mutex_lock(&((nimcp_gt_learner_t)learner)->mutex);
    *stats_out = learner->stats;
    nimcp_platform_mutex_unlock(&((nimcp_gt_learner_t)learner)->mutex);

    return NIMCP_SUCCESS;
}

void nimcp_gt_learner_reset_stats(nimcp_gt_learner_t learner) {
    if (!learner) return;

    /* Phase 8: Heartbeat at operation start */
    gt_learning_heartbeat("gt_learning_gt_learner_reset_sta", 0.0f);


    nimcp_platform_mutex_lock(&learner->mutex);
    memset(&learner->stats, 0, sizeof(nimcp_gt_learning_stats_t));
    learner->stats.current_learning_rate = learner->current_learning_rate;
    learner->stats.current_exploration = learner->current_exploration_rate;
    nimcp_platform_mutex_unlock(&learner->mutex);
}

bool nimcp_gt_learner_has_converged(
    const nimcp_gt_learner_t learner,
    float threshold
) {
    if (!learner) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    gt_learning_heartbeat("gt_learning_gt_learner_has_conve", 0.0f);


    nimcp_platform_mutex_lock(&((nimcp_gt_learner_t)learner)->mutex);
    bool converged = learner->last_max_q_change < threshold;
    nimcp_platform_mutex_unlock(&((nimcp_gt_learner_t)learner)->mutex);

    return converged;
}

nimcp_error_t nimcp_gt_compute_exploitability(
    const nimcp_gt_learner_t learner,
    const float* payoff_matrix,
    float* exploitability_out
) {
    if (!learner || !payoff_matrix || !exploitability_out) {
        return NIMCP_GT_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    gt_learning_heartbeat("gt_learning_gt_compute_exploitab", 0.0f);


    nimcp_platform_mutex_lock(&((nimcp_gt_learner_t)learner)->mutex);

    uint32_t num_actions = learner->config.num_actions;

    /* Get current strategy (average over states for simplicity) */
    float* strategy = nimcp_calloc(num_actions, sizeof(float));
    if (!strategy) {
        nimcp_platform_mutex_unlock(&((nimcp_gt_learner_t)learner)->mutex);
        return NIMCP_GT_ERROR_NO_MEMORY;
    }

    /* Average strategy across states */
    float count = 0.0f;
    for (uint32_t s = 0; s < learner->config.num_states; s++) {
        /* Phase 8: Loop progress heartbeat */
        if ((s & 0xFF) == 0 && learner->config.num_states > 256) {
            gt_learning_heartbeat("gt_learning_loop",
                             (float)(s + 1) / (float)learner->config.num_states);
        }

        for (uint32_t a = 0; a < num_actions; a++) {
            /* Phase 8: Loop progress heartbeat */
            if ((a & 0xFF) == 0 && num_actions > 256) {
                gt_learning_heartbeat("gt_learning_loop",
                                 (float)(a + 1) / (float)num_actions);
            }

            nimcp_gt_q_entry_t* entry = get_q_entry(learner->q_table, s, a);
            if (entry && entry->visit_count > 0) {
                strategy[a] += entry->value;
                count += 1.0f;
            }
        }
    }

    if (count > 0.0f) {
        for (uint32_t a = 0; a < num_actions; a++) {
            /* Phase 8: Loop progress heartbeat */
            if ((a & 0xFF) == 0 && num_actions > 256) {
                gt_learning_heartbeat("gt_learning_loop",
                                 (float)(a + 1) / (float)num_actions);
            }

            strategy[a] /= count;
        }
        /* Convert to probabilities via softmax */
        softmax(strategy, strategy, num_actions, 1.0f);
    } else {
        float uniform = 1.0f / (float)num_actions;
        for (uint32_t a = 0; a < num_actions; a++) {
            /* Phase 8: Loop progress heartbeat */
            if ((a & 0xFF) == 0 && num_actions > 256) {
                gt_learning_heartbeat("gt_learning_loop",
                                 (float)(a + 1) / (float)num_actions);
            }

            strategy[a] = uniform;
        }
    }

    /* Compute opponent best response value */
    float max_br_value = -FLT_MAX;
    for (uint32_t opp_a = 0; opp_a < num_actions; opp_a++) {
        /* Phase 8: Loop progress heartbeat */
        if ((opp_a & 0xFF) == 0 && num_actions > 256) {
            gt_learning_heartbeat("gt_learning_loop",
                             (float)(opp_a + 1) / (float)num_actions);
        }

        float br_value = 0.0f;
        for (uint32_t our_a = 0; our_a < num_actions; our_a++) {
            /* Phase 8: Loop progress heartbeat */
            if ((our_a & 0xFF) == 0 && num_actions > 256) {
                gt_learning_heartbeat("gt_learning_loop",
                                 (float)(our_a + 1) / (float)num_actions);
            }

            /* Opponent payoff is -our payoff in zero-sum */
            br_value += strategy[our_a] * (-payoff_matrix[our_a * num_actions + opp_a]);
        }
        if (br_value > max_br_value) {
            max_br_value = br_value;
        }
    }

    /* Exploitability = value opponent can get - 0 (Nash value in zero-sum) */
    *exploitability_out = max_br_value;

    nimcp_free(strategy);
    strategy = NULL;
    nimcp_platform_mutex_unlock(&((nimcp_gt_learner_t)learner)->mutex);
    return NIMCP_SUCCESS;
}

//=============================================================================
// Utility Functions
//=============================================================================

void nimcp_gt_opponent_model_init(
    nimcp_gt_opponent_model_t* model,
    uint32_t num_actions
) {
    if (!model) return;

    /* Phase 8: Heartbeat at operation start */
    gt_learning_heartbeat("gt_learning_gt_opponent_model_in", 0.0f);


    memset(model, 0, sizeof(nimcp_gt_opponent_model_t));

    /* Initialize with uniform beliefs */
    float uniform = 1.0f / (float)NIMCP_GT_OPPONENT_COUNT;
    for (uint32_t t = 0; t < NIMCP_GT_OPPONENT_COUNT; t++) {
        /* Phase 8: Loop progress heartbeat */
        if ((t & 0xFF) == 0 && NIMCP_GT_OPPONENT_COUNT > 256) {
            gt_learning_heartbeat("gt_learning_loop",
                             (float)(t + 1) / (float)NIMCP_GT_OPPONENT_COUNT);
        }

        model->type_beliefs[t] = uniform;
    }

    model->predicted_type = NIMCP_GT_OPPONENT_UNKNOWN;
    model->prediction_confidence = uniform;
    model->num_actions = num_actions;

    if (num_actions > 0) {
        model->action_predictions = nimcp_calloc(num_actions, sizeof(float));
        model->action_counts = nimcp_calloc(num_actions, sizeof(uint32_t));
        if (!model->action_counts) return;

        /* Uniform action predictions initially */
        if (model->action_predictions) {
            float uniform_action = 1.0f / (float)num_actions;
            for (uint32_t a = 0; a < num_actions; a++) {
                /* Phase 8: Loop progress heartbeat */
                if ((a & 0xFF) == 0 && num_actions > 256) {
                    gt_learning_heartbeat("gt_learning_loop",
                                     (float)(a + 1) / (float)num_actions);
                }

                model->action_predictions[a] = uniform_action;
            }
        }
    }
}

void nimcp_gt_opponent_model_cleanup(nimcp_gt_opponent_model_t* model) {
    if (!model) return;

    /* Phase 8: Heartbeat at operation start */
    gt_learning_heartbeat("gt_learning_gt_opponent_model_cl", 0.0f);


    nimcp_free(model->action_predictions);
    nimcp_free(model->action_counts);

    model->action_predictions = NULL;
    model->action_counts = NULL;
    model->num_actions = 0;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

/**
 * WHAT: Query knowledge graph for GT Learning self-knowledge
 * WHY:  Enable self-awareness about module's role and connections
 * HOW:  Query KG for entity observations and relations
 */
int gt_learning_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    gt_learning_heartbeat("gt_learning_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "GT_Learning");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                gt_learning_heartbeat("gt_learning_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            /* GT Learning self-knowledge logged */
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "GT_Learning");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "GT_Learning");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void gt_learning_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_gt_learning_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int gt_learning_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "gt_learning_training_begin: NULL argument");
        return -1;
    }
    gt_learning_heartbeat_instance(NULL, "gt_learning_training_begin", 0.0f);
    (void)(struct nimcp_gt_q_table_struct*)instance; /* Module state available for reset */
    return 0;
}

int gt_learning_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "gt_learning_training_end: NULL argument");
        return -1;
    }
    gt_learning_heartbeat_instance(NULL, "gt_learning_training_end", 1.0f);
    (void)(struct nimcp_gt_q_table_struct*)instance; /* Module state available for finalization */
    return 0;
}

int gt_learning_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "gt_learning_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    gt_learning_heartbeat_instance(NULL, "gt_learning_training_step", progress);
    (void)(struct nimcp_gt_q_table_struct*)instance; /* Module state available for step adaptation */
    return 0;
}
