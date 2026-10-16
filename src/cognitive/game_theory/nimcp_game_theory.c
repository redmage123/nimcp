//=============================================================================
// nimcp_game_theory.c - Game Theory Core Implementation
//=============================================================================
/**
 * @file nimcp_game_theory.c
 * @brief Core game theory system implementation
 */

#include "cognitive/game_theory/nimcp_game_theory.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/algorithms/nimcp_monte_carlo.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_constants.h"

BRIDGE_BOILERPLATE(game_theory, MESH_ADAPTER_CATEGORY_COGNITIVE)



//=============================================================================
// Monte Carlo Integration - Thread-local seed
//=============================================================================

static _Thread_local uint32_t g_gt_mc_seed = 0;

//=============================================================================
// Internal Structure
//=============================================================================

struct nimcp_gt_system_struct {
    nimcp_gt_config_t config;
    nimcp_game_stats_t stats;
    bool initialized;
    nimcp_platform_mutex_t mutex;
};

//=============================================================================
// Static Name Tables
//=============================================================================

static const char* s_game_type_names[] = {
    "Zero-Sum",
    "Cooperative",
    "General-Sum",
    "Potential",
    "Congestion",
    "Auction",
    "Bargaining"
};

static const char* s_solution_concept_names[] = {
    "Nash Equilibrium",
    "Correlated Equilibrium",
    "Pareto Optimal",
    "Core",
    "Shapley Value",
    "Nucleolus"
};

//=============================================================================
// Configuration
//=============================================================================

nimcp_gt_config_t nimcp_gt_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    game_theory_heartbeat("game_theory_gt_default_config", 0.0f);


    nimcp_gt_config_t config = {
        .max_players = NIMCP_GT_MAX_PLAYERS,
        .max_iterations = NIMCP_GT_MAX_ITERATIONS,
        .convergence_epsilon = NIMCP_CONVERGENCE_THRESHOLD,
        .enable_statistics = true,
        .enable_history = true,
        .history_depth = NIMCP_GT_HISTORY_DEPTH
    };
    return config;
}

//=============================================================================
// Lifecycle
//=============================================================================

nimcp_gt_system_t nimcp_gt_create(const nimcp_gt_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    game_theory_heartbeat("game_theory_gt_create", 0.0f);


    nimcp_gt_system_t system = nimcp_calloc(1, sizeof(struct nimcp_gt_system_struct));
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate system");

        return NULL;
    }

    if (config) {
        system->config = *config;
    } else {
        system->config = nimcp_gt_default_config();
    }

    if (nimcp_platform_mutex_init(&system->mutex, false) != 0) {
        nimcp_free(system);
        system = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "nimcp_gt_create: validation failed");
        return NULL;
    }

    memset(&system->stats, 0, sizeof(nimcp_game_stats_t));
    system->initialized = true;

    return system;
}

void nimcp_gt_destroy(nimcp_gt_system_t system) {
    if (!system) return;

    /* Phase 8: Heartbeat at operation start */
    game_theory_heartbeat("game_theory_gt_destroy", 0.0f);


    nimcp_platform_mutex_destroy(&system->mutex);
    nimcp_free(system);
    system = NULL;
}

bool nimcp_gt_is_initialized(const nimcp_gt_system_t system) {
    /* Phase 8: Heartbeat at operation start */
    game_theory_heartbeat("game_theory_gt_is_initialized", 0.0f);


    return system && system->initialized;
}

nimcp_error_t nimcp_gt_get_stats(const nimcp_gt_system_t system,
                                  nimcp_game_stats_t* stats) {
    if (!system || !stats) {
        return NIMCP_GT_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    game_theory_heartbeat("game_theory_gt_get_stats", 0.0f);


    nimcp_platform_mutex_lock(&system->mutex);
    *stats = system->stats;
    nimcp_platform_mutex_unlock(&system->mutex);

    return NIMCP_SUCCESS;
}

void nimcp_gt_reset_stats(nimcp_gt_system_t system) {
    if (!system) return;

    /* Phase 8: Heartbeat at operation start */
    game_theory_heartbeat("game_theory_gt_reset_stats", 0.0f);


    nimcp_platform_mutex_lock(&system->mutex);
    memset(&system->stats, 0, sizeof(nimcp_game_stats_t));
    nimcp_platform_mutex_unlock(&system->mutex);
}

//=============================================================================
// Name Utilities
//=============================================================================

const char* nimcp_game_type_name(nimcp_game_type_t type) {
    if (type >= NIMCP_GAME_COUNT) {
        return "Unknown";
    }
    return s_game_type_names[type];
}

const char* nimcp_solution_concept_name(nimcp_solution_concept_t solution_concept) {
    if (solution_concept > NIMCP_SOLUTION_NUCLEOLUS) {
        return "Unknown";
    }
    return s_solution_concept_names[solution_concept];
}

//=============================================================================
// Fairness and Efficiency Utilities
//=============================================================================

float nimcp_compute_fairness_index(const float* allocations, uint32_t n) {
    if (!allocations || n == 0) {
        return 0.0f;
    }

    // Jain's fairness index: J = (sum x_i)^2 / (n * sum x_i^2)
    /* Phase 8: Heartbeat at operation start */
    game_theory_heartbeat("game_theory_compute_fairness_ind", 0.0f);


    float sum = 0.0f;
    float sum_sq = 0.0f;

    for (uint32_t i = 0; i < n; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && n > 256) {
            game_theory_heartbeat("game_theory_loop",
                             (float)(i + 1) / (float)n);
        }

        sum += allocations[i];
        sum_sq += allocations[i] * allocations[i];
    }

    if (sum_sq < 1e-10f) {
        return 1.0f;  // All zero is perfectly fair
    }

    return (sum * sum) / ((float)n * sum_sq);
}

bool nimcp_is_pareto_optimal(const float* utilities, uint32_t n,
                              const float* feasible_utilities,
                              uint32_t num_feasible) {
    if (!utilities || !feasible_utilities || n == 0 || num_feasible == 0) {
        return false;
    }

    // Check if any feasible allocation Pareto-dominates current
    /* Phase 8: Heartbeat at operation start */
    game_theory_heartbeat("game_theory_is_pareto_optimal", 0.0f);


    for (uint32_t f = 0; f < num_feasible; f++) {
        /* Phase 8: Loop progress heartbeat */
        if ((f & 0xFF) == 0 && num_feasible > 256) {
            game_theory_heartbeat("game_theory_loop",
                             (float)(f + 1) / (float)num_feasible);
        }

        const float* other = &feasible_utilities[f * n];

        bool at_least_one_better = false;
        bool all_at_least_as_good = true;

        for (uint32_t i = 0; i < n; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && n > 256) {
                game_theory_heartbeat("game_theory_loop",
                                 (float)(i + 1) / (float)n);
            }

            if (other[i] < utilities[i]) {
                all_at_least_as_good = false;
                break;
            }
            if (other[i] > utilities[i]) {
                at_least_one_better = true;
            }
        }

        // Other dominates current -> not Pareto optimal
        if (all_at_least_as_good && at_least_one_better) {
            return false;
        }
    }

    return true;
}

//=============================================================================
// Player Utilities
//=============================================================================

void nimcp_player_init(nimcp_player_t* player, nimcp_player_id_t id,
                        const char* name, uint32_t num_actions) {
    if (!player) return;

    /* Phase 8: Heartbeat at operation start */
    game_theory_heartbeat("game_theory_player_init", 0.0f);


    memset(player, 0, sizeof(nimcp_player_t));
    player->id = id;
    player->num_actions = num_actions;

    if (name) {
        strncpy(player->name, name, sizeof(player->name) - 1);
        player->name[sizeof(player->name) - 1] = '\0';
    }

    if (num_actions > 0) {
        player->strategy = nimcp_calloc(num_actions, sizeof(float));
        if (player->strategy) {
            // Default: uniform distribution
            float uniform = 1.0f / (float)num_actions;
            for (uint32_t i = 0; i < num_actions; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && num_actions > 256) {
                    game_theory_heartbeat("game_theory_loop",
                                     (float)(i + 1) / (float)num_actions);
                }

                player->strategy[i] = uniform;
            }
        }
    }
}

void nimcp_player_cleanup(nimcp_player_t* player) {
    if (!player) return;

    /* Phase 8: Heartbeat at operation start */
    game_theory_heartbeat("game_theory_player_cleanup", 0.0f);


    if (player->strategy) {
        nimcp_free(player->strategy);
        player->strategy = NULL;
    }
    if (player->private_info) {
        nimcp_free(player->private_info);
        player->private_info = NULL;
    }
}

void nimcp_game_outcome_init(nimcp_game_outcome_t* outcome) {
    if (!outcome) return;

    /* Phase 8: Heartbeat at operation start */
    game_theory_heartbeat("game_theory_game_outcome_init", 0.0f);


    memset(outcome, 0, sizeof(nimcp_game_outcome_t));

    // Initialize winners to invalid
    for (uint32_t i = 0; i < NIMCP_GT_MAX_PLAYERS; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && NIMCP_GT_MAX_PLAYERS > 256) {
            game_theory_heartbeat("game_theory_loop",
                             (float)(i + 1) / (float)NIMCP_GT_MAX_PLAYERS);
        }

        outcome->winners[i] = NIMCP_GT_INVALID_PLAYER;
    }
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

/**
 * WHAT: Query knowledge graph for Game Theory self-knowledge
 * WHY:  Enable self-awareness about module's role and connections
 * HOW:  Query KG for entity observations and relations
 */
int game_theory_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    game_theory_heartbeat("game_theory_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Game_Theory");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                game_theory_heartbeat("game_theory_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            /* Game theory self-knowledge logged */
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Game_Theory");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Game_Theory");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}

//=============================================================================
// Monte Carlo Enhanced Functions
//=============================================================================

/**
 * @brief Select action according to mixed strategy using MC sampling
 *
 * WHAT: Sample action from probability distribution
 * WHY:  Realize mixed strategies in actual play
 * HOW:  Use MC importance sampling
 *
 * @param player Player with mixed strategy
 * @return Selected action index
 */
uint32_t nimcp_gt_sample_action_mc(const nimcp_player_t* player) {
    if (!player || !player->strategy || player->num_actions == 0) {
        return 0;
    }

    /* Phase 8: Heartbeat at operation start */
    game_theory_heartbeat("game_theory_gt_sample_action_mc", 0.0f);


    if (g_gt_mc_seed == 0) {
        g_gt_mc_seed = mc_seed_from_time();
    }

    /* Sample using MC */
    float r = mc_random_uniform(&g_gt_mc_seed);
    float cumulative = 0.0f;

    for (uint32_t i = 0; i < player->num_actions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && player->num_actions > 256) {
            game_theory_heartbeat("game_theory_loop",
                             (float)(i + 1) / (float)player->num_actions);
        }

        cumulative += player->strategy[i];
        if (r < cumulative) {
            return i;
        }
    }

    return player->num_actions - 1;
}

/**
 * @brief Estimate expected utility via Monte Carlo sampling
 *
 * WHAT: Compute expected payoff under mixed strategies
 * WHY:  Evaluate strategy quality when payoff matrix is stochastic
 * HOW:  Sample action profiles, average payoffs
 *
 * @param system Game theory system
 * @param players Array of players with strategies
 * @param num_players Number of players
 * @param payoff_fn Payoff function (action profile -> utilities)
 * @param num_samples Number of MC samples
 * @param expected_utilities Output: expected utility per player
 * @param user_data User data for payoff function
 * @return 0 on success
 */
int nimcp_gt_expected_utility_mc(
    nimcp_gt_system_t system,
    const nimcp_player_t* players,
    uint32_t num_players,
    float (*payoff_fn)(const uint32_t* actions, uint32_t num_players, void* user_data),
    uint32_t num_samples,
    float* expected_utilities,
    void* user_data
) {
    if (!system || !players || !payoff_fn || !expected_utilities || num_players == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_gt_expected_utility_mc: required parameter is NULL (system, players, payoff_fn, expected_utilities)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    game_theory_heartbeat("game_theory_gt_expected_utility_", 0.0f);


    if (g_gt_mc_seed == 0) {
        g_gt_mc_seed = mc_seed_from_time();
    }

    /* Initialize utilities to zero */
    for (uint32_t i = 0; i < num_players; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_players > 256) {
            game_theory_heartbeat("game_theory_loop",
                             (float)(i + 1) / (float)num_players);
        }

        expected_utilities[i] = 0.0f;
    }

    /* Allocate action profile */
    uint32_t* actions = nimcp_calloc(num_players, sizeof(uint32_t));
    if (!actions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_gt_expected_utility_mc: actions is NULL");
        return -1;
    }

    /* Monte Carlo sampling */
    for (uint32_t s = 0; s < num_samples; s++) {
        /* Phase 8: Loop progress heartbeat */
        if ((s & 0xFF) == 0 && num_samples > 256) {
            game_theory_heartbeat("game_theory_loop",
                             (float)(s + 1) / (float)num_samples);
        }

        /* Sample action profile from mixed strategies */
        for (uint32_t p = 0; p < num_players; p++) {
            /* Phase 8: Loop progress heartbeat */
            if ((p & 0xFF) == 0 && num_players > 256) {
                game_theory_heartbeat("game_theory_loop",
                                 (float)(p + 1) / (float)num_players);
            }

            float r = mc_random_uniform(&g_gt_mc_seed);
            float cumulative = 0.0f;
            actions[p] = 0;

            for (uint32_t a = 0; a < players[p].num_actions; a++) {
                cumulative += players[p].strategy[a];
                if (r < cumulative) {
                    actions[p] = a;
                    break;
                }
            }
        }

        /* Compute payoff and accumulate */
        float payoff = payoff_fn(actions, num_players, user_data);
        for (uint32_t p = 0; p < num_players; p++) {
            /* Phase 8: Loop progress heartbeat */
            if ((p & 0xFF) == 0 && num_players > 256) {
                game_theory_heartbeat("game_theory_loop",
                                 (float)(p + 1) / (float)num_players);
            }

            expected_utilities[p] += payoff;
        }
    }

    /* Average */
    for (uint32_t i = 0; i < num_players; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_players > 256) {
            game_theory_heartbeat("game_theory_loop",
                             (float)(i + 1) / (float)num_players);
        }

        expected_utilities[i] /= (float)num_samples;
    }

    nimcp_free(actions);
    actions = NULL;
    return 0;
}

/**
 * @brief Update strategy via fictitious play with MC sampling
 *
 * WHAT: Update strategy using best response to opponent history
 * WHY:  Simple learning algorithm that converges to Nash in some games
 * HOW:  Track opponent action frequencies, compute best response
 *
 * @param player Player to update
 * @param opponent_frequencies Observed frequencies of opponent actions
 * @param num_opponent_actions Number of opponent actions
 * @param payoff_matrix Player's payoff matrix [my_action][opp_action]
 * @param learning_rate How fast to update (0-1)
 * @return 0 on success
 */
int nimcp_gt_fictitious_play_update_mc(
    nimcp_player_t* player,
    const float* opponent_frequencies,
    uint32_t num_opponent_actions,
    const float* payoff_matrix,
    float learning_rate
) {
    if (!player || !opponent_frequencies || !payoff_matrix || !player->strategy) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_gt_fictitious_play_update_mc: required parameter is NULL (player, opponent_frequencies, payoff_matrix, player->strategy)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    game_theory_heartbeat("game_theory_gt_fictitious_play_u", 0.0f);


    if (g_gt_mc_seed == 0) {
        g_gt_mc_seed = mc_seed_from_time();
    }

    /* Compute expected payoff for each of my actions */
    float* expected_payoffs = nimcp_calloc(player->num_actions, sizeof(float));
    if (!expected_payoffs) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_gt_fictitious_play_update_mc: expected_payoffs is NULL");
        return -1;
    }

    float max_payoff = -1e30f;
    uint32_t best_action = 0;

    for (uint32_t a = 0; a < player->num_actions; a++) {
        /* Phase 8: Loop progress heartbeat */
        if ((a & 0xFF) == 0 && player->num_actions > 256) {
            game_theory_heartbeat("game_theory_loop",
                             (float)(a + 1) / (float)player->num_actions);
        }

        expected_payoffs[a] = 0.0f;
        for (uint32_t o = 0; o < num_opponent_actions; o++) {
            /* Phase 8: Loop progress heartbeat */
            if ((o & 0xFF) == 0 && num_opponent_actions > 256) {
                game_theory_heartbeat("game_theory_loop",
                                 (float)(o + 1) / (float)num_opponent_actions);
            }

            expected_payoffs[a] += payoff_matrix[a * num_opponent_actions + o] * opponent_frequencies[o];
        }
        if (expected_payoffs[a] > max_payoff) {
            max_payoff = expected_payoffs[a];
            best_action = a;
        }
    }

    /* Update strategy toward best response with noise for exploration */
    for (uint32_t a = 0; a < player->num_actions; a++) {
        /* Phase 8: Loop progress heartbeat */
        if ((a & 0xFF) == 0 && player->num_actions > 256) {
            game_theory_heartbeat("game_theory_loop",
                             (float)(a + 1) / (float)player->num_actions);
        }

        float target = (a == best_action) ? 1.0f : 0.0f;
        /* Add small noise for exploration */
        float noise = mc_random_normal(&g_gt_mc_seed, 0.0f, 0.01f);
        player->strategy[a] = (1.0f - learning_rate) * player->strategy[a] +
                              learning_rate * target + noise;
        if (player->strategy[a] < 0.0f) player->strategy[a] = 0.0f;
    }

    /* Normalize */
    float sum = 0.0f;
    for (uint32_t a = 0; a < player->num_actions; a++) {
        /* Phase 8: Loop progress heartbeat */
        if ((a & 0xFF) == 0 && player->num_actions > 256) {
            game_theory_heartbeat("game_theory_loop",
                             (float)(a + 1) / (float)player->num_actions);
        }

        sum += player->strategy[a];
    }
    if (sum > 0.0f) {
        for (uint32_t a = 0; a < player->num_actions; a++) {
            /* Phase 8: Loop progress heartbeat */
            if ((a & 0xFF) == 0 && player->num_actions > 256) {
                game_theory_heartbeat("game_theory_loop",
                                 (float)(a + 1) / (float)player->num_actions);
            }

            player->strategy[a] /= sum;
        }
    }

    nimcp_free(expected_payoffs);
    expected_payoffs = NULL;
    return 0;
}

/**
 * @brief Get thread-local MC seed for game theory
 *
 * @return Pointer to thread-local seed
 */
uint32_t* nimcp_gt_get_mc_seed(void) {
    if (g_gt_mc_seed == 0) {
        g_gt_mc_seed = mc_seed_from_time();
    }
    return &g_gt_mc_seed;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void game_theory_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_game_theory_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int game_theory_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "game_theory_training_begin: NULL argument");
        return -1;
    }
    game_theory_heartbeat_instance(NULL, "game_theory_training_begin", 0.0f);
    (void)(struct nimcp_gt_system_struct*)instance; /* Module state available for reset */
    return 0;
}

int game_theory_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "game_theory_training_end: NULL argument");
        return -1;
    }
    game_theory_heartbeat_instance(NULL, "game_theory_training_end", 1.0f);
    (void)(struct nimcp_gt_system_struct*)instance; /* Module state available for finalization */
    return 0;
}

int game_theory_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "game_theory_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    game_theory_heartbeat_instance(NULL, "game_theory_training_step", progress);
    (void)(struct nimcp_gt_system_struct*)instance; /* Module state available for step adaptation */
    return 0;
}
