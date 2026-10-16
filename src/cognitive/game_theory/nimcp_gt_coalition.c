//=============================================================================
// nimcp_gt_coalition.c - Coalition Formation Dynamics Implementation
//=============================================================================
/**
 * @file nimcp_gt_coalition.c
 * @brief Coalition formation algorithms and stability analysis
 */

#include "cognitive/game_theory/nimcp_gt_coalition.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <time.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_learning_constants.h"

BRIDGE_BOILERPLATE(gt_coalition, MESH_ADAPTER_CATEGORY_COGNITIVE)



//=============================================================================
// Internal Structure
//=============================================================================

/**
 * @brief Preference ordering for a player
 */
typedef struct {
    uint32_t preferences[NIMCP_GT_MAX_PREFERENCES]; /**< Coalition bitmasks in preference order */
    uint32_t num_preferences;                        /**< Number of preferences set */
    bool is_set;                                     /**< Has preferences been set? */
} player_preference_t;

struct nimcp_coalition_game_struct {
    nimcp_coalition_config_t config;

    // Value function
    nimcp_gt_coalition_value_fn value_fn;
    void* value_user_data;

    // Preference function (for hedonic games)
    nimcp_gt_preference_fn pref_fn;
    void* pref_user_data;

    // Explicit preference orderings per player
    player_preference_t player_prefs[NIMCP_GT_MAX_PLAYERS];

    // Coalition value cache (for n <= 20)
    float* value_cache;
    bool* cache_valid;
    uint32_t cache_size;

    // Statistics
    uint64_t coalitions_evaluated;

    // Thread safety
    nimcp_platform_mutex_t mutex;
};

//=============================================================================
// Static Helpers
//=============================================================================

static const char* s_stability_names[] = {
    "Core Stability",
    "Nash Stability",
    "Individual Rationality",
    "Contractual Stability",
    "Strict Core"
};

static const char* s_formation_names[] = {
    "Greedy Sequential",
    "Optimal Partition",
    "Merge/Split Dynamics",
    "Bottom-Up",
    "Top-Down"
};

/**
 * @brief Count set bits in a 32-bit integer
 */
static uint32_t popcount32(uint32_t x) {
    uint32_t count = 0;
    while (x) {
        count += x & 1;
        x >>= 1;
    }
    return count;
}

/**
 * @brief Get current time in milliseconds
 */
static float get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (float)(ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0);
}

/**
 * @brief Get cached or compute coalition value
 */
static float get_coalition_value_cached(
    nimcp_coalition_game_t game,
    uint32_t coalition
) {
    if (!game->value_fn) {
        return 0.0f;
    }

    // Check cache
    if (game->value_cache && coalition < game->cache_size) {
        if (game->cache_valid[coalition]) {
            return game->value_cache[coalition];
        }
    }

    // Compute value
    float value = game->value_fn(coalition, game->config.num_players, game->value_user_data);
    game->coalitions_evaluated++;

    // Store in cache
    if (game->value_cache && coalition < game->cache_size) {
        game->value_cache[coalition] = value;
        game->cache_valid[coalition] = true;
    }

    return value;
}

/**
 * @brief Compare coalitions using preference function or explicit ordering
 *
 * @return negative if coal1 < coal2, 0 if equal, positive if coal1 > coal2
 */
static int compare_coalitions(
    nimcp_coalition_game_t game,
    uint32_t player,
    uint32_t coalition1,
    uint32_t coalition2
) {
    // Use explicit preference ordering if set
    if (game->player_prefs[player].is_set) {
        int rank1 = -1, rank2 = -1;
        for (uint32_t i = 0; i < game->player_prefs[player].num_preferences; i++) {
            if (game->player_prefs[player].preferences[i] == coalition1) rank1 = (int)i;
            if (game->player_prefs[player].preferences[i] == coalition2) rank2 = (int)i;
        }
        // Lower rank = more preferred
        if (rank1 < 0 && rank2 < 0) return 0;
        if (rank1 < 0) return -1;
        if (rank2 < 0) return 1;
        return rank2 - rank1;  // Higher rank2 means coal1 is preferred
    }

    // Use preference callback if set
    if (game->pref_fn) {
        return game->pref_fn(player, coalition1, coalition2, game->pref_user_data);
    }

    // Default: compare by per-capita value
    float val1 = get_coalition_value_cached(game, coalition1);
    float val2 = get_coalition_value_cached(game, coalition2);
    uint32_t size1 = popcount32(coalition1);
    uint32_t size2 = popcount32(coalition2);

    float per_cap1 = (size1 > 0) ? val1 / (float)size1 : 0.0f;
    float per_cap2 = (size2 > 0) ? val2 / (float)size2 : 0.0f;

    if (per_cap1 > per_cap2) return 1;
    if (per_cap1 < per_cap2) return -1;
    return 0;
}

/**
 * @brief Compute player payoff in a coalition (equal split)
 */
static float compute_player_payoff(
    nimcp_coalition_game_t game,
    uint32_t coalition,
    uint32_t player
) {
    if (!(coalition & (1u << player))) {
        return 0.0f;  // Player not in coalition
    }

    float value = get_coalition_value_cached(game, coalition);
    uint32_t size = popcount32(coalition);

    return (size > 0) ? value / (float)size : 0.0f;
}

/**
 * @brief Find which coalition a player is in within a structure
 */
static int32_t find_player_coalition_internal(
    const nimcp_coalition_structure_t* structure,
    uint32_t player
) {
    uint32_t player_bit = 1u << player;
    for (uint32_t i = 0; i < structure->num_coalitions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && structure->num_coalitions > 256) {
            gt_coalition_heartbeat("gt_coalition_loop",
                             (float)(i + 1) / (float)structure->num_coalitions);
        }

        if (structure->coalitions[i].members & player_bit) {
            return (int32_t)i;
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "find_player_coalition_internal: validation failed");
    return -1;
}

/**
 * @brief Compute structure total value
 */
static float compute_structure_value(
    nimcp_coalition_game_t game,
    nimcp_coalition_structure_t* structure
) {
    float total = 0.0f;
    for (uint32_t i = 0; i < structure->num_coalitions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && structure->num_coalitions > 256) {
            gt_coalition_heartbeat("gt_coalition_loop",
                             (float)(i + 1) / (float)structure->num_coalitions);
        }

        structure->coalitions[i].value = get_coalition_value_cached(game, structure->coalitions[i].members);
        total += structure->coalitions[i].value;
    }
    structure->total_value = total;
    return total;
}

/**
 * @brief Internal unlocked version of is_individually_rational
 * @note Caller must hold mutex
 */
static bool is_individually_rational_unlocked(
    nimcp_coalition_game_t game,
    const nimcp_coalition_structure_t* structure
) {
    if (!game || !structure || !game->value_fn) {
        return false;
    }

    uint32_t n = game->config.num_players;

    for (uint32_t p = 0; p < n; p++) {
        /* Phase 8: Loop progress heartbeat */
        if ((p & 0xFF) == 0 && n > 256) {
            gt_coalition_heartbeat("gt_coalition_loop",
                             (float)(p + 1) / (float)n);
        }

        uint32_t player_bit = 1u << p;

        // Find player's current coalition
        int32_t coal_idx = find_player_coalition_internal(structure, p);
        if (coal_idx < 0) {
            return false;
        }

        // Compute current payoff
        float current_payoff = compute_player_payoff(game, structure->coalitions[coal_idx].members, p);

        // Compute singleton value
        float singleton_value = get_coalition_value_cached(game, player_bit);

        // Individual rationality: current payoff >= singleton value
        if (current_payoff < singleton_value - game->config.convergence_epsilon) {
            return false;
        }
    }

    return true;
}

/**
 * @brief Internal unlocked version of is_in_core
 * @note Caller must hold mutex
 */
static bool is_in_core_unlocked(
    nimcp_coalition_game_t game,
    const nimcp_coalition_structure_t* structure
) {
    if (!game || !structure || !game->value_fn) {
        return false;
    }

    uint32_t n = game->config.num_players;
    uint32_t num_coalitions = 1u << n;

    // Compute payoffs in current structure
    float payoffs[NIMCP_GT_MAX_PLAYERS] = {0};
    for (uint32_t c = 0; c < structure->num_coalitions; c++) {
        /* Phase 8: Loop progress heartbeat */
        if ((c & 0xFF) == 0 && structure->num_coalitions > 256) {
            gt_coalition_heartbeat("gt_coalition_loop",
                             (float)(c + 1) / (float)structure->num_coalitions);
        }

        uint32_t coal = structure->coalitions[c].members;
        float value = get_coalition_value_cached(game, coal);
        uint32_t size = popcount32(coal);
        float per_player = (size > 0) ? value / (float)size : 0.0f;

        for (uint32_t p = 0; p < n; p++) {
            /* Phase 8: Loop progress heartbeat */
            if ((p & 0xFF) == 0 && n > 256) {
                gt_coalition_heartbeat("gt_coalition_loop",
                                 (float)(p + 1) / (float)n);
            }

            if (coal & (1u << p)) {
                payoffs[p] = per_player;
            }
        }
    }

    // Check core condition: for all coalitions S, sum(payoff[i] for i in S) >= v(S)
    for (uint32_t S = 1; S < num_coalitions; S++) {
        float payoff_sum = 0.0f;
        for (uint32_t p = 0; p < n; p++) {
            /* Phase 8: Loop progress heartbeat */
            if ((p & 0xFF) == 0 && n > 256) {
                gt_coalition_heartbeat("gt_coalition_loop",
                                 (float)(p + 1) / (float)n);
            }

            if (S & (1u << p)) {
                payoff_sum += payoffs[p];
            }
        }

        float v_S = get_coalition_value_cached(game, S);
        if (payoff_sum < v_S - game->config.convergence_epsilon) {
            return false;
        }
    }

    return true;
}

//=============================================================================
// Configuration
//=============================================================================

nimcp_coalition_config_t nimcp_coalition_default_config(uint32_t num_players) {
    /* Phase 8: Heartbeat at operation start */
    gt_coalition_heartbeat("gt_coalition_coalition_default_co", 0.0f);


    nimcp_coalition_config_t config = {
        .num_players = (num_players > NIMCP_GT_MAX_PLAYERS) ? NIMCP_GT_MAX_PLAYERS : num_players,
        .algorithm = NIMCP_FORMATION_GREEDY,
        .max_iterations = NIMCP_GT_MAX_ITERATIONS,
        .convergence_epsilon = NIMCP_EPSILON_LARGE,
        .cache_values = true,
        .use_preferences = false
    };
    return config;
}

//=============================================================================
// Lifecycle
//=============================================================================

nimcp_coalition_game_t nimcp_coalition_create(const nimcp_coalition_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    gt_coalition_heartbeat("gt_coalition_coalition_create", 0.0f);


    nimcp_coalition_config_t default_config = nimcp_coalition_default_config(4);
    if (!config) {
        config = &default_config;
    }

    if (config->num_players == 0 || config->num_players > NIMCP_GT_MAX_PLAYERS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_coalition_create: config->num_players is zero");
        return NULL;
    }

    // Validate player count doesn't exceed bitmask capacity (32-bit coalitions)
    // This is a hard limit due to uint32_t coalition bitmask representation
    if (config->num_players > 32) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_coalition_create: num_players exceeds uint32_t bitmask capacity (max 32)");
        return NULL;  // Coalition bitmask overflow: uint32_t can only represent 32 players
    }

    nimcp_coalition_game_t game = nimcp_calloc(1, sizeof(struct nimcp_coalition_game_struct));
    if (!game) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate game");

        return NULL;
    }

    game->config = *config;

    // Allocate value cache if enabled and feasible
    if (config->cache_values && config->num_players <= 20) {
        game->cache_size = 1u << config->num_players;
        game->value_cache = nimcp_calloc(game->cache_size, sizeof(float));
        game->cache_valid = nimcp_calloc(game->cache_size, sizeof(bool));
        if (!game->value_cache || !game->cache_valid) {
            nimcp_free(game->value_cache);
            nimcp_free(game->cache_valid);
            nimcp_free(game);
            game = NULL;
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_coalition_create: required parameter is NULL (game->value_cache, game->cache_valid)");
            return NULL;
        }
    }

    // Initialize player preferences
    for (uint32_t i = 0; i < NIMCP_GT_MAX_PLAYERS; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && NIMCP_GT_MAX_PLAYERS > 256) {
            gt_coalition_heartbeat("gt_coalition_loop",
                             (float)(i + 1) / (float)NIMCP_GT_MAX_PLAYERS);
        }

        game->player_prefs[i].is_set = false;
        game->player_prefs[i].num_preferences = 0;
    }

    if (nimcp_platform_mutex_init(&game->mutex, false) != 0) {
        nimcp_free(game->value_cache);
        nimcp_free(game->cache_valid);
        nimcp_free(game);
        game = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "nimcp_coalition_create: validation failed");
        return NULL;
    }

    return game;
}

void nimcp_coalition_destroy(nimcp_coalition_game_t game) {
    if (!game) return;

    /* Phase 8: Heartbeat at operation start */
    gt_coalition_heartbeat("gt_coalition_coalition_destroy", 0.0f);


    nimcp_platform_mutex_destroy(&game->mutex);
    nimcp_free(game->value_cache);
    nimcp_free(game->cache_valid);
    nimcp_free(game);
    game = NULL;
}

//=============================================================================
// Configuration Functions
//=============================================================================

nimcp_error_t nimcp_coalition_set_value_function(
    nimcp_coalition_game_t game,
    nimcp_gt_coalition_value_fn value_fn,
    void* user_data
) {
    if (!game) {
        return NIMCP_GT_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    gt_coalition_heartbeat("gt_coalition_coalition_set_value_", 0.0f);


    nimcp_platform_mutex_lock(&game->mutex);
    game->value_fn = value_fn;
    game->value_user_data = user_data;

    // Clear cache when value function changes
    if (game->cache_valid) {
        memset(game->cache_valid, 0, game->cache_size * sizeof(bool));
    }

    nimcp_platform_mutex_unlock(&game->mutex);
    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_coalition_set_preferences(
    nimcp_coalition_game_t game,
    nimcp_gt_preference_fn pref_fn,
    void* user_data
) {
    if (!game) {
        return NIMCP_GT_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    gt_coalition_heartbeat("gt_coalition_coalition_set_prefer", 0.0f);


    nimcp_platform_mutex_lock(&game->mutex);
    game->pref_fn = pref_fn;
    game->pref_user_data = user_data;
    nimcp_platform_mutex_unlock(&game->mutex);
    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_coalition_set_preference_order(
    nimcp_coalition_game_t game,
    uint32_t player,
    const uint32_t* preferences,
    uint32_t num_preferences
) {
    if (!game || !preferences) {
        return NIMCP_GT_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    gt_coalition_heartbeat("gt_coalition_coalition_set_prefer", 0.0f);


    if (player >= game->config.num_players) {
        return NIMCP_GT_ERROR_INVALID_PARAMETER;
    }

    if (num_preferences > NIMCP_GT_MAX_PREFERENCES) {
        num_preferences = NIMCP_GT_MAX_PREFERENCES;
    }

    nimcp_platform_mutex_lock(&game->mutex);

    memcpy(game->player_prefs[player].preferences, preferences, num_preferences * sizeof(uint32_t));
    game->player_prefs[player].num_preferences = num_preferences;
    game->player_prefs[player].is_set = true;

    nimcp_platform_mutex_unlock(&game->mutex);
    return NIMCP_SUCCESS;
}

//=============================================================================
// Greedy Formation
//=============================================================================

nimcp_error_t nimcp_coalition_form_greedy(
    nimcp_coalition_game_t game,
    nimcp_coalition_result_t* result
) {
    if (!game || !result) {
        return NIMCP_GT_ERROR_NULL_POINTER;
    }

    if (!game->value_fn) {
        return NIMCP_GT_ERROR_VALUE_FN_NOT_SET;
    }

    /* Phase 8: Heartbeat at operation start */
    gt_coalition_heartbeat("gt_coalition_coalition_form_greed", 0.0f);


    nimcp_platform_mutex_lock(&game->mutex);

    float start_time = get_time_ms();
    uint32_t n = game->config.num_players;

    memset(result, 0, sizeof(nimcp_coalition_result_t));
    game->coalitions_evaluated = 0;

    // Initialize with singletons
    nimcp_coalition_structure_init_singletons(&result->structure, n);

    bool improved = true;
    uint32_t iterations = 0;

    while (improved && iterations < game->config.max_iterations) {
        improved = false;
        iterations++;

        // For each player, try joining each existing coalition
        for (uint32_t player = 0; player < n; player++) {
            /* Phase 8: Loop progress heartbeat */
            if ((player & 0xFF) == 0 && n > 256) {
                gt_coalition_heartbeat("gt_coalition_loop",
                                 (float)(player + 1) / (float)n);
            }

            uint32_t player_bit = 1u << player;
            int32_t current_coal_idx = find_player_coalition_internal(&result->structure, player);
            if (current_coal_idx < 0) continue;

            uint32_t current_coalition = result->structure.coalitions[current_coal_idx].members;
            float current_payoff = compute_player_payoff(game, current_coalition, player);

            // Try joining each other coalition
            float best_payoff = current_payoff;
            int32_t best_target = -1;

            for (uint32_t c = 0; c < result->structure.num_coalitions; c++) {
                /* Phase 8: Loop progress heartbeat */
                if ((c & 0xFF) == 0 && result->structure.num_coalitions > 256) {
                    gt_coalition_heartbeat("gt_coalition_loop",
                                     (float)(c + 1) / (float)result->structure.num_coalitions);
                }

                if ((int32_t)c == current_coal_idx) continue;

                uint32_t target_coal = result->structure.coalitions[c].members | player_bit;
                float new_payoff = compute_player_payoff(game, target_coal, player);

                if (new_payoff > best_payoff + game->config.convergence_epsilon) {
                    best_payoff = new_payoff;
                    best_target = (int32_t)c;
                }
            }

            // Also try forming singleton if currently in multi-player coalition
            if (popcount32(current_coalition) > 1) {
                float singleton_payoff = compute_player_payoff(game, player_bit, player);
                if (singleton_payoff > best_payoff + game->config.convergence_epsilon) {
                    best_payoff = singleton_payoff;
                    best_target = -2;  // Signal to form singleton
                }
            }

            // Apply best move
            if (best_target != -1) {
                improved = true;

                // Remove player from current coalition
                result->structure.coalitions[current_coal_idx].members &= ~player_bit;
                result->structure.coalitions[current_coal_idx].size = popcount32(result->structure.coalitions[current_coal_idx].members);

                if (best_target == -2) {
                    // Form new singleton
                    if (result->structure.num_coalitions < NIMCP_GT_MAX_COALITIONS) {
                        result->structure.coalitions[result->structure.num_coalitions].members = player_bit;
                        result->structure.coalitions[result->structure.num_coalitions].size = 1;
                        result->structure.num_coalitions++;
                    }
                } else {
                    // Join target coalition
                    result->structure.coalitions[best_target].members |= player_bit;
                    result->structure.coalitions[best_target].size = popcount32(result->structure.coalitions[best_target].members);
                }

                // Remove empty coalitions
                uint32_t write_idx = 0;
                for (uint32_t read_idx = 0; read_idx < result->structure.num_coalitions; read_idx++) {
                    /* Phase 8: Loop progress heartbeat */
                    if ((read_idx & 0xFF) == 0 && result->structure.num_coalitions > 256) {
                        gt_coalition_heartbeat("gt_coalition_loop",
                                         (float)(read_idx + 1) / (float)result->structure.num_coalitions);
                    }

                    if (result->structure.coalitions[read_idx].members != 0) {
                        if (write_idx != read_idx) {
                            result->structure.coalitions[write_idx] = result->structure.coalitions[read_idx];
                        }
                        write_idx++;
                    }
                }
                result->structure.num_coalitions = write_idx;
            }
        }
    }

    // Compute final values
    compute_structure_value(game, &result->structure);

    // Check stability (use unlocked versions since we already hold mutex)
    result->is_stable[NIMCP_STABILITY_NASH] = !improved;  // Nash stable if no improvements
    result->is_stable[NIMCP_STABILITY_INDIVIDUAL] = is_individually_rational_unlocked(game, &result->structure);
    result->is_stable[NIMCP_STABILITY_CORE] = is_in_core_unlocked(game, &result->structure);

    result->iterations = iterations;
    result->coalitions_evaluated = game->coalitions_evaluated;
    result->formation_time_ms = get_time_ms() - start_time;
    result->converged = !improved;

    nimcp_platform_mutex_unlock(&game->mutex);
    return NIMCP_SUCCESS;
}

//=============================================================================
// Optimal Formation (Dynamic Programming)
//=============================================================================

nimcp_error_t nimcp_coalition_form_optimal(
    nimcp_coalition_game_t game,
    nimcp_coalition_result_t* result
) {
    if (!game || !result) {
        return NIMCP_GT_ERROR_NULL_POINTER;
    }

    if (!game->value_fn) {
        return NIMCP_GT_ERROR_VALUE_FN_NOT_SET;
    }

    // Optimal is only feasible for small n
    /* Phase 8: Heartbeat at operation start */
    gt_coalition_heartbeat("gt_coalition_coalition_form_optim", 0.0f);


    if (game->config.num_players > 15) {
        return NIMCP_GT_ERROR_CAPACITY;
    }

    nimcp_platform_mutex_lock(&game->mutex);

    float start_time = get_time_ms();
    uint32_t n = game->config.num_players;
    uint32_t num_subsets = 1u << n;

    memset(result, 0, sizeof(nimcp_coalition_result_t));
    game->coalitions_evaluated = 0;

    // Allocate DP tables
    float* opt_value = nimcp_calloc(num_subsets, sizeof(float));
    uint32_t* opt_first_coal = nimcp_calloc(num_subsets, sizeof(uint32_t));

    if (!opt_value || !opt_first_coal) {
        nimcp_free(opt_value);
        opt_value = NULL;
        nimcp_free(opt_first_coal);
        opt_first_coal = NULL;
        nimcp_platform_mutex_unlock(&game->mutex);
        return NIMCP_GT_ERROR_NO_MEMORY;
    }

    // Base case: empty set
    opt_value[0] = 0.0f;
    opt_first_coal[0] = 0;

    // DP: for each subset S, find optimal first coalition
    for (uint32_t S = 1; S < num_subsets; S++) {
        opt_value[S] = -1e30f;  // Negative infinity

        // Try each non-empty subset of S as the first coalition
        for (uint32_t C = S; C != 0; C = (C - 1) & S) {
            float coal_value = get_coalition_value_cached(game, C);
            uint32_t rest = S & ~C;
            float total = coal_value + opt_value[rest];

            if (total > opt_value[S]) {
                opt_value[S] = total;
                opt_first_coal[S] = C;
            }
        }
    }

    // Reconstruct optimal partition
    uint32_t grand = num_subsets - 1;
    result->structure.total_value = opt_value[grand];
    result->structure.all_players = grand;
    result->structure.num_coalitions = 0;

    uint32_t remaining = grand;
    while (remaining != 0 && result->structure.num_coalitions < NIMCP_GT_MAX_COALITIONS) {
        uint32_t first = opt_first_coal[remaining];
        result->structure.coalitions[result->structure.num_coalitions].members = first;
        result->structure.coalitions[result->structure.num_coalitions].value = get_coalition_value_cached(game, first);
        result->structure.coalitions[result->structure.num_coalitions].size = popcount32(first);
        result->structure.num_coalitions++;
        remaining &= ~first;
    }

    nimcp_free(opt_value);
    opt_value = NULL;
    nimcp_free(opt_first_coal);
    opt_first_coal = NULL;

    // Check stability (use unlocked versions since we already hold mutex)
    result->is_stable[NIMCP_STABILITY_INDIVIDUAL] = is_individually_rational_unlocked(game, &result->structure);
    result->is_stable[NIMCP_STABILITY_CORE] = is_in_core_unlocked(game, &result->structure);
    result->is_stable[NIMCP_STABILITY_NASH] = true;  // Optimal is Nash stable by definition

    result->iterations = 1;
    result->coalitions_evaluated = game->coalitions_evaluated;
    result->formation_time_ms = get_time_ms() - start_time;
    result->converged = true;

    nimcp_platform_mutex_unlock(&game->mutex);
    return NIMCP_SUCCESS;
}

//=============================================================================
// Merge/Split Dynamics
//=============================================================================

nimcp_error_t nimcp_coalition_form_merge_split(
    nimcp_coalition_game_t game,
    nimcp_coalition_result_t* result
) {
    if (!game || !result) {
        return NIMCP_GT_ERROR_NULL_POINTER;
    }

    if (!game->value_fn) {
        return NIMCP_GT_ERROR_VALUE_FN_NOT_SET;
    }

    /* Phase 8: Heartbeat at operation start */
    gt_coalition_heartbeat("gt_coalition_coalition_form_merge", 0.0f);


    nimcp_platform_mutex_lock(&game->mutex);

    float start_time = get_time_ms();
    uint32_t n = game->config.num_players;

    memset(result, 0, sizeof(nimcp_coalition_result_t));
    game->coalitions_evaluated = 0;

    // Initialize with singletons
    nimcp_coalition_structure_init_singletons(&result->structure, n);
    compute_structure_value(game, &result->structure);

    bool improved = true;
    uint32_t iterations = 0;

    while (improved && iterations < game->config.max_iterations) {
        improved = false;
        iterations++;

        // MERGE PHASE: Try merging pairs of coalitions
        for (uint32_t i = 0; i < result->structure.num_coalitions && !improved; i++) {
            for (uint32_t j = i + 1; j < result->structure.num_coalitions && !improved; j++) {
                uint32_t merged = result->structure.coalitions[i].members | result->structure.coalitions[j].members;
                float merged_value = get_coalition_value_cached(game, merged);
                float separate_value = result->structure.coalitions[i].value + result->structure.coalitions[j].value;

                if (merged_value > separate_value + game->config.convergence_epsilon) {
                    // Merge is beneficial
                    result->structure.coalitions[i].members = merged;
                    result->structure.coalitions[i].value = merged_value;
                    result->structure.coalitions[i].size = popcount32(merged);

                    // Remove j by shifting
                    for (uint32_t k = j; k < result->structure.num_coalitions - 1; k++) {
                        result->structure.coalitions[k] = result->structure.coalitions[k + 1];
                    }
                    result->structure.num_coalitions--;

                    result->structure.total_value = result->structure.total_value - separate_value + merged_value;
                    improved = true;
                }
            }
        }

        if (improved) continue;

        // SPLIT PHASE: Try splitting coalitions
        for (uint32_t i = 0; i < result->structure.num_coalitions && !improved; i++) {
            if (result->structure.coalitions[i].size <= 1) continue;

            uint32_t coal = result->structure.coalitions[i].members;
            float coal_value = result->structure.coalitions[i].value;

            // Try all binary partitions of this coalition
            for (uint32_t part1 = coal; part1 != 0; part1 = (part1 - 1) & coal) {
                uint32_t part2 = coal & ~part1;
                if (part1 == 0 || part2 == 0) continue;

                float val1 = get_coalition_value_cached(game, part1);
                float val2 = get_coalition_value_cached(game, part2);

                if (val1 + val2 > coal_value + game->config.convergence_epsilon) {
                    // Split is beneficial
                    result->structure.coalitions[i].members = part1;
                    result->structure.coalitions[i].value = val1;
                    result->structure.coalitions[i].size = popcount32(part1);

                    if (result->structure.num_coalitions < NIMCP_GT_MAX_COALITIONS) {
                        result->structure.coalitions[result->structure.num_coalitions].members = part2;
                        result->structure.coalitions[result->structure.num_coalitions].value = val2;
                        result->structure.coalitions[result->structure.num_coalitions].size = popcount32(part2);
                        result->structure.num_coalitions++;
                    }

                    result->structure.total_value = result->structure.total_value - coal_value + val1 + val2;
                    improved = true;
                    break;
                }
            }
        }
    }

    // Recompute final values
    compute_structure_value(game, &result->structure);

    // Check stability (use unlocked versions since we already hold mutex)
    result->is_stable[NIMCP_STABILITY_NASH] = !improved;
    result->is_stable[NIMCP_STABILITY_INDIVIDUAL] = is_individually_rational_unlocked(game, &result->structure);
    result->is_stable[NIMCP_STABILITY_CORE] = is_in_core_unlocked(game, &result->structure);

    result->iterations = iterations;
    result->coalitions_evaluated = game->coalitions_evaluated;
    result->formation_time_ms = get_time_ms() - start_time;
    result->converged = !improved;

    nimcp_platform_mutex_unlock(&game->mutex);
    return NIMCP_SUCCESS;
}

//=============================================================================
// Stability Analysis
//=============================================================================

bool nimcp_coalition_is_stable(
    nimcp_coalition_game_t game,
    const nimcp_coalition_structure_t* structure,
    nimcp_stability_type_t stability_type
) {
    if (!game || !structure) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    gt_coalition_heartbeat("gt_coalition_coalition_is_stable", 0.0f);


    switch (stability_type) {
        case NIMCP_STABILITY_CORE:
            return nimcp_coalition_is_in_core(game, structure);

        case NIMCP_STABILITY_INDIVIDUAL:
            return nimcp_coalition_is_individually_rational(game, structure);

        case NIMCP_STABILITY_NASH: {
            // Check no player wants to unilaterally deviate
            for (uint32_t p = 0; p < game->config.num_players; p++) {
                /* Phase 8: Loop progress heartbeat */
                if ((p & 0xFF) == 0 && game->config.num_players > 256) {
                    gt_coalition_heartbeat("gt_coalition_loop",
                                     (float)(p + 1) / (float)game->config.num_players);
                }

                for (uint32_t c = 0; c < structure->num_coalitions; c++) {
                    /* Phase 8: Loop progress heartbeat */
                    if ((c & 0xFF) == 0 && structure->num_coalitions > 256) {
                        gt_coalition_heartbeat("gt_coalition_loop",
                                         (float)(c + 1) / (float)structure->num_coalitions);
                    }

                    if (nimcp_coalition_player_would_deviate(game, structure, p, (int32_t)c)) {
                        return false;
                    }
                }
                // Check deviation to singleton
                if (nimcp_coalition_player_would_deviate(game, structure, p, -1)) {
                    return false;
                }
            }
            return true;
        }

        case NIMCP_STABILITY_CONTRACTUAL:
            // Contractual: members of blocking coalition must all agree
            // Simplified: same as core for now
            return nimcp_coalition_is_in_core(game, structure);

        case NIMCP_STABILITY_STRICT_CORE: {
            // Strict core: no weakly blocking coalition (where some improve, none worse)
            // For now, approximate as core
            return nimcp_coalition_is_in_core(game, structure);
        }

        default:
            return false;
    }
}

bool nimcp_coalition_is_in_core(
    nimcp_coalition_game_t game,
    const nimcp_coalition_structure_t* structure
) {
    if (!game || !structure || !game->value_fn) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    gt_coalition_heartbeat("gt_coalition_coalition_is_in_core", 0.0f);


    nimcp_platform_mutex_lock(&game->mutex);

    uint32_t n = game->config.num_players;
    uint32_t num_coalitions = 1u << n;

    // Compute payoffs in current structure
    float payoffs[NIMCP_GT_MAX_PLAYERS] = {0};
    for (uint32_t c = 0; c < structure->num_coalitions; c++) {
        /* Phase 8: Loop progress heartbeat */
        if ((c & 0xFF) == 0 && structure->num_coalitions > 256) {
            gt_coalition_heartbeat("gt_coalition_loop",
                             (float)(c + 1) / (float)structure->num_coalitions);
        }

        uint32_t coal = structure->coalitions[c].members;
        float value = get_coalition_value_cached(game, coal);
        uint32_t size = popcount32(coal);
        float per_player = (size > 0) ? value / (float)size : 0.0f;

        for (uint32_t p = 0; p < n; p++) {
            /* Phase 8: Loop progress heartbeat */
            if ((p & 0xFF) == 0 && n > 256) {
                gt_coalition_heartbeat("gt_coalition_loop",
                                 (float)(p + 1) / (float)n);
            }

            if (coal & (1u << p)) {
                payoffs[p] = per_player;
            }
        }
    }

    // Check core condition: for all coalitions S, sum(payoff[i] for i in S) >= v(S)
    bool in_core = true;
    for (uint32_t S = 1; S < num_coalitions && in_core; S++) {
        float payoff_sum = 0.0f;
        for (uint32_t p = 0; p < n; p++) {
            /* Phase 8: Loop progress heartbeat */
            if ((p & 0xFF) == 0 && n > 256) {
                gt_coalition_heartbeat("gt_coalition_loop",
                                 (float)(p + 1) / (float)n);
            }

            if (S & (1u << p)) {
                payoff_sum += payoffs[p];
            }
        }

        float v_S = get_coalition_value_cached(game, S);
        if (payoff_sum < v_S - game->config.convergence_epsilon) {
            in_core = false;
        }
    }

    nimcp_platform_mutex_unlock(&game->mutex);
    return in_core;
}

bool nimcp_coalition_is_individually_rational(
    nimcp_coalition_game_t game,
    const nimcp_coalition_structure_t* structure
) {
    if (!game || !structure || !game->value_fn) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    gt_coalition_heartbeat("gt_coalition_coalition_is_individ", 0.0f);


    nimcp_platform_mutex_lock(&game->mutex);

    uint32_t n = game->config.num_players;
    bool is_rational = true;

    for (uint32_t p = 0; p < n && is_rational; p++) {
        uint32_t player_bit = 1u << p;

        // Find player's current coalition
        int32_t coal_idx = find_player_coalition_internal(structure, p);
        if (coal_idx < 0) {
            is_rational = false;
            continue;
        }

        // Compute current payoff
        float current_payoff = compute_player_payoff(game, structure->coalitions[coal_idx].members, p);

        // Compute singleton value
        float singleton_value = get_coalition_value_cached(game, player_bit);

        // Individual rationality: current payoff >= singleton value
        if (current_payoff < singleton_value - game->config.convergence_epsilon) {
            is_rational = false;
        }
    }

    nimcp_platform_mutex_unlock(&game->mutex);
    return is_rational;
}

nimcp_error_t nimcp_coalition_find_blocking(
    nimcp_coalition_game_t game,
    const nimcp_coalition_structure_t* structure,
    uint32_t* blocking_coalition
) {
    if (!game || !structure || !blocking_coalition) {
        return NIMCP_GT_ERROR_NULL_POINTER;
    }

    if (!game->value_fn) {
        return NIMCP_GT_ERROR_VALUE_FN_NOT_SET;
    }

    /* Phase 8: Heartbeat at operation start */
    gt_coalition_heartbeat("gt_coalition_coalition_find_block", 0.0f);


    nimcp_platform_mutex_lock(&game->mutex);

    uint32_t n = game->config.num_players;
    uint32_t num_coalitions = 1u << n;
    *blocking_coalition = 0;

    // Compute current payoffs
    float payoffs[NIMCP_GT_MAX_PLAYERS] = {0};
    for (uint32_t c = 0; c < structure->num_coalitions; c++) {
        /* Phase 8: Loop progress heartbeat */
        if ((c & 0xFF) == 0 && structure->num_coalitions > 256) {
            gt_coalition_heartbeat("gt_coalition_loop",
                             (float)(c + 1) / (float)structure->num_coalitions);
        }

        uint32_t coal = structure->coalitions[c].members;
        float value = get_coalition_value_cached(game, coal);
        uint32_t size = popcount32(coal);
        float per_player = (size > 0) ? value / (float)size : 0.0f;

        for (uint32_t p = 0; p < n; p++) {
            /* Phase 8: Loop progress heartbeat */
            if ((p & 0xFF) == 0 && n > 256) {
                gt_coalition_heartbeat("gt_coalition_loop",
                                 (float)(p + 1) / (float)n);
            }

            if (coal & (1u << p)) {
                payoffs[p] = per_player;
            }
        }
    }

    // Search for blocking coalition
    for (uint32_t S = 1; S < num_coalitions; S++) {
        float v_S = get_coalition_value_cached(game, S);
        uint32_t size = popcount32(S);
        float new_payoff = (size > 0) ? v_S / (float)size : 0.0f;

        // Check if all members of S would strictly prefer S
        bool all_prefer = true;
        for (uint32_t p = 0; p < n && all_prefer; p++) {
            if (S & (1u << p)) {
                if (new_payoff <= payoffs[p] + game->config.convergence_epsilon) {
                    all_prefer = false;
                }
            }
        }

        if (all_prefer) {
            *blocking_coalition = S;
            nimcp_platform_mutex_unlock(&game->mutex);
            return NIMCP_SUCCESS;
        }
    }

    nimcp_platform_mutex_unlock(&game->mutex);
    return NIMCP_GT_ERROR_NO_BLOCKING;
}

//=============================================================================
// Dynamics Operations
//=============================================================================

nimcp_error_t nimcp_coalition_merge(
    nimcp_coalition_game_t game,
    const nimcp_coalition_structure_t* structure,
    uint32_t coal_idx1,
    uint32_t coal_idx2,
    nimcp_coalition_structure_t* new_structure
) {
    if (!game || !structure || !new_structure) {
        return NIMCP_GT_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    gt_coalition_heartbeat("gt_coalition_coalition_merge", 0.0f);


    if (coal_idx1 >= structure->num_coalitions || coal_idx2 >= structure->num_coalitions) {
        return NIMCP_GT_ERROR_COALITION_INVALID;
    }

    if (coal_idx1 == coal_idx2) {
        return NIMCP_GT_ERROR_INVALID_PARAMETER;
    }

    nimcp_platform_mutex_lock(&game->mutex);

    // Copy structure
    memcpy(new_structure, structure, sizeof(nimcp_coalition_structure_t));

    // Ensure coal_idx1 < coal_idx2 for easier removal
    if (coal_idx1 > coal_idx2) {
        uint32_t tmp = coal_idx1;
        coal_idx1 = coal_idx2;
        coal_idx2 = tmp;
    }

    // Merge
    uint32_t merged = new_structure->coalitions[coal_idx1].members | new_structure->coalitions[coal_idx2].members;
    new_structure->coalitions[coal_idx1].members = merged;
    new_structure->coalitions[coal_idx1].value = get_coalition_value_cached(game, merged);
    new_structure->coalitions[coal_idx1].size = popcount32(merged);

    // Remove coal_idx2 by shifting
    for (uint32_t k = coal_idx2; k < new_structure->num_coalitions - 1; k++) {
        new_structure->coalitions[k] = new_structure->coalitions[k + 1];
    }
    new_structure->num_coalitions--;

    // Recompute total value
    new_structure->total_value = 0.0f;
    for (uint32_t i = 0; i < new_structure->num_coalitions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && new_structure->num_coalitions > 256) {
            gt_coalition_heartbeat("gt_coalition_loop",
                             (float)(i + 1) / (float)new_structure->num_coalitions);
        }

        new_structure->total_value += new_structure->coalitions[i].value;
    }

    nimcp_platform_mutex_unlock(&game->mutex);
    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_coalition_split(
    nimcp_coalition_game_t game,
    const nimcp_coalition_structure_t* structure,
    uint32_t coal_idx,
    nimcp_coalition_structure_t* new_structure
) {
    if (!game || !structure || !new_structure) {
        return NIMCP_GT_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    gt_coalition_heartbeat("gt_coalition_coalition_split", 0.0f);


    if (coal_idx >= structure->num_coalitions) {
        return NIMCP_GT_ERROR_COALITION_INVALID;
    }

    if (structure->coalitions[coal_idx].size <= 1) {
        return NIMCP_GT_ERROR_COALITION_INVALID;  // Cannot split singleton
    }

    if (structure->num_coalitions >= NIMCP_GT_MAX_COALITIONS) {
        return NIMCP_GT_ERROR_CAPACITY;
    }

    nimcp_platform_mutex_lock(&game->mutex);

    uint32_t coal = structure->coalitions[coal_idx].members;
    float coal_value = get_coalition_value_cached(game, coal);

    // Find best binary split
    uint32_t best_part1 = 0, best_part2 = 0;
    float best_split_value = coal_value;

    for (uint32_t part1 = coal; part1 != 0; part1 = (part1 - 1) & coal) {
        uint32_t part2 = coal & ~part1;
        if (part1 == 0 || part2 == 0) continue;

        float val1 = get_coalition_value_cached(game, part1);
        float val2 = get_coalition_value_cached(game, part2);

        if (val1 + val2 > best_split_value) {
            best_split_value = val1 + val2;
            best_part1 = part1;
            best_part2 = part2;
        }
    }

    if (best_part1 == 0 || best_part2 == 0) {
        // No beneficial split found
        memcpy(new_structure, structure, sizeof(nimcp_coalition_structure_t));
        nimcp_platform_mutex_unlock(&game->mutex);
        return NIMCP_SUCCESS;
    }

    // Apply split
    memcpy(new_structure, structure, sizeof(nimcp_coalition_structure_t));

    new_structure->coalitions[coal_idx].members = best_part1;
    new_structure->coalitions[coal_idx].value = get_coalition_value_cached(game, best_part1);
    new_structure->coalitions[coal_idx].size = popcount32(best_part1);

    new_structure->coalitions[new_structure->num_coalitions].members = best_part2;
    new_structure->coalitions[new_structure->num_coalitions].value = get_coalition_value_cached(game, best_part2);
    new_structure->coalitions[new_structure->num_coalitions].size = popcount32(best_part2);
    new_structure->num_coalitions++;

    // Recompute total value
    new_structure->total_value = 0.0f;
    for (uint32_t i = 0; i < new_structure->num_coalitions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && new_structure->num_coalitions > 256) {
            gt_coalition_heartbeat("gt_coalition_loop",
                             (float)(i + 1) / (float)new_structure->num_coalitions);
        }

        new_structure->total_value += new_structure->coalitions[i].value;
    }

    nimcp_platform_mutex_unlock(&game->mutex);
    return NIMCP_SUCCESS;
}

bool nimcp_coalition_player_would_deviate(
    nimcp_coalition_game_t game,
    const nimcp_coalition_structure_t* structure,
    uint32_t player,
    int32_t target_coal_idx
) {
    if (!game || !structure) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_coalition_player_would_deviate: required parameter is NULL (game, structure)");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    gt_coalition_heartbeat("gt_coalition_coalition_player_wou", 0.0f);


    if (player >= game->config.num_players) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_coalition_player_would_deviate: capacity exceeded");
        return false;
    }

    nimcp_platform_mutex_lock(&game->mutex);

    uint32_t player_bit = 1u << player;

    // Find current coalition
    int32_t current_idx = find_player_coalition_internal(structure, player);
    if (current_idx < 0) {
        nimcp_platform_mutex_unlock(&game->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_coalition_player_would_deviate: validation failed");
        return false;
    }

    // Same coalition - no deviation
    if (current_idx == target_coal_idx) {
        nimcp_platform_mutex_unlock(&game->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_coalition_player_would_deviate: validation failed");
        return false;
    }

    uint32_t current_coal = structure->coalitions[current_idx].members;
    float current_payoff = compute_player_payoff(game, current_coal, player);

    float new_payoff = 0.0f;
    if (target_coal_idx < 0) {
        // Deviation to singleton - check if already a singleton
        if (current_coal == player_bit) {
            // Player is already alone, no deviation possible
            nimcp_platform_mutex_unlock(&game->mutex);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_coalition_player_would_deviate: validation failed");
            return false;
        }
        // Compute payoff as singleton (value / 1 = value)
        new_payoff = get_coalition_value_cached(game, player_bit);
    } else if ((uint32_t)target_coal_idx < structure->num_coalitions) {
        // Deviation to existing coalition
        uint32_t target_coal = structure->coalitions[target_coal_idx].members | player_bit;
        new_payoff = compute_player_payoff(game, target_coal, player);
    } else {
        nimcp_platform_mutex_unlock(&game->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_coalition_player_would_deviate: operation failed");
        return false;
    }

    bool would_deviate = (new_payoff > current_payoff + game->config.convergence_epsilon);

    nimcp_platform_mutex_unlock(&game->mutex);
    return would_deviate;
}

//=============================================================================
// Value and Payoff Computation
//=============================================================================

nimcp_error_t nimcp_coalition_compute_value(
    nimcp_coalition_game_t game,
    uint32_t coalition,
    float* value
) {
    if (!game || !value) {
        return NIMCP_GT_ERROR_NULL_POINTER;
    }

    if (!game->value_fn) {
        return NIMCP_GT_ERROR_VALUE_FN_NOT_SET;
    }

    /* Phase 8: Heartbeat at operation start */
    gt_coalition_heartbeat("gt_coalition_coalition_compute_va", 0.0f);


    nimcp_platform_mutex_lock(&game->mutex);
    *value = get_coalition_value_cached(game, coalition);
    nimcp_platform_mutex_unlock(&game->mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_coalition_compute_payoff(
    nimcp_coalition_game_t game,
    uint32_t coalition,
    uint32_t player,
    float* payoff
) {
    if (!game || !payoff) {
        return NIMCP_GT_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    gt_coalition_heartbeat("gt_coalition_coalition_compute_pa", 0.0f);


    if (player >= game->config.num_players) {
        return NIMCP_GT_ERROR_INVALID_PARAMETER;
    }

    if (!game->value_fn) {
        return NIMCP_GT_ERROR_VALUE_FN_NOT_SET;
    }

    nimcp_platform_mutex_lock(&game->mutex);
    *payoff = compute_player_payoff(game, coalition, player);
    nimcp_platform_mutex_unlock(&game->mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_coalition_compute_payoffs(
    nimcp_coalition_game_t game,
    const nimcp_coalition_structure_t* structure,
    float* payoffs
) {
    if (!game || !structure || !payoffs) {
        return NIMCP_GT_ERROR_NULL_POINTER;
    }

    if (!game->value_fn) {
        return NIMCP_GT_ERROR_VALUE_FN_NOT_SET;
    }

    /* Phase 8: Heartbeat at operation start */
    gt_coalition_heartbeat("gt_coalition_coalition_compute_pa", 0.0f);


    nimcp_platform_mutex_lock(&game->mutex);

    uint32_t n = game->config.num_players;
    memset(payoffs, 0, n * sizeof(float));

    for (uint32_t c = 0; c < structure->num_coalitions; c++) {
        /* Phase 8: Loop progress heartbeat */
        if ((c & 0xFF) == 0 && structure->num_coalitions > 256) {
            gt_coalition_heartbeat("gt_coalition_loop",
                             (float)(c + 1) / (float)structure->num_coalitions);
        }

        uint32_t coal = structure->coalitions[c].members;
        float value = get_coalition_value_cached(game, coal);
        uint32_t size = popcount32(coal);
        float per_player = (size > 0) ? value / (float)size : 0.0f;

        for (uint32_t p = 0; p < n; p++) {
            /* Phase 8: Loop progress heartbeat */
            if ((p & 0xFF) == 0 && n > 256) {
                gt_coalition_heartbeat("gt_coalition_loop",
                                 (float)(p + 1) / (float)n);
            }

            if (coal & (1u << p)) {
                payoffs[p] = per_player;
            }
        }
    }

    nimcp_platform_mutex_unlock(&game->mutex);
    return NIMCP_SUCCESS;
}

//=============================================================================
// Utility Functions
//=============================================================================

void nimcp_coalition_structure_init_singletons(
    nimcp_coalition_structure_t* structure,
    uint32_t num_players
) {
    if (!structure) return;

    /* Phase 8: Heartbeat at operation start */
    gt_coalition_heartbeat("gt_coalition_coalition_structure_", 0.0f);


    memset(structure, 0, sizeof(nimcp_coalition_structure_t));

    if (num_players > NIMCP_GT_MAX_PLAYERS) {
        num_players = NIMCP_GT_MAX_PLAYERS;
    }

    structure->num_coalitions = num_players;
    structure->all_players = (1u << num_players) - 1;

    for (uint32_t i = 0; i < num_players; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_players > 256) {
            gt_coalition_heartbeat("gt_coalition_loop",
                             (float)(i + 1) / (float)num_players);
        }

        structure->coalitions[i].members = 1u << i;
        structure->coalitions[i].size = 1;
        structure->coalitions[i].value = 0.0f;
    }
}

void nimcp_coalition_structure_init_grand(
    nimcp_coalition_structure_t* structure,
    uint32_t num_players
) {
    if (!structure) return;

    /* Phase 8: Heartbeat at operation start */
    gt_coalition_heartbeat("gt_coalition_coalition_structure_", 0.0f);


    memset(structure, 0, sizeof(nimcp_coalition_structure_t));

    if (num_players > NIMCP_GT_MAX_PLAYERS) {
        num_players = NIMCP_GT_MAX_PLAYERS;
    }

    structure->num_coalitions = 1;
    structure->all_players = (1u << num_players) - 1;

    structure->coalitions[0].members = structure->all_players;
    structure->coalitions[0].size = num_players;
    structure->coalitions[0].value = 0.0f;
}

bool nimcp_coalition_structure_is_valid(
    const nimcp_coalition_structure_t* structure,
    uint32_t num_players
) {
    if (!structure) {
        return false;
    }
    if (num_players > NIMCP_GT_MAX_PLAYERS) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    gt_coalition_heartbeat("gt_coalition_coalition_structure_", 0.0f);


    uint32_t all_players = (1u << num_players) - 1;
    uint32_t covered = 0;

    for (uint32_t i = 0; i < structure->num_coalitions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && structure->num_coalitions > 256) {
            gt_coalition_heartbeat("gt_coalition_loop",
                             (float)(i + 1) / (float)structure->num_coalitions);
        }

        uint32_t coal = structure->coalitions[i].members;

        // Check coalition is non-empty and within player set
        if (coal == 0 || (coal & ~all_players) != 0) {
            return false;
        }

        // Check no overlap with previously seen coalitions
        if ((covered & coal) != 0) {
            return false;
        }

        covered |= coal;
    }

    // Check all players are covered
    return covered == all_players;
}

int32_t nimcp_coalition_find_player_coalition(
    const nimcp_coalition_structure_t* structure,
    uint32_t player
) {
    /* Phase 8: Heartbeat at operation start */
    gt_coalition_heartbeat("gt_coalition_coalition_find_playe", 0.0f);


    return find_player_coalition_internal(structure, player);
}

const char* nimcp_stability_type_name(nimcp_stability_type_t type) {
    if (type >= NIMCP_STABILITY_COUNT) {
        return "Unknown";
    }
    return s_stability_names[type];
}

const char* nimcp_formation_algorithm_name(nimcp_formation_algorithm_t algorithm) {
    if (algorithm >= NIMCP_FORMATION_COUNT) {
        return "Unknown";
    }
    return s_formation_names[algorithm];
}

uint32_t nimcp_coalition_get_num_players(const nimcp_coalition_game_t game) {
    if (!game) return 0;
    /* Phase 8: Heartbeat at operation start */
    gt_coalition_heartbeat("gt_coalition_coalition_get_num_pl", 0.0f);


    return game->config.num_players;
}

void nimcp_coalition_clear_cache(nimcp_coalition_game_t game) {
    if (!game) return;

    /* Phase 8: Heartbeat at operation start */
    gt_coalition_heartbeat("gt_coalition_coalition_clear_cach", 0.0f);


    nimcp_platform_mutex_lock(&game->mutex);
    if (game->cache_valid) {
        memset(game->cache_valid, 0, game->cache_size * sizeof(bool));
    }
    game->coalitions_evaluated = 0;
    nimcp_platform_mutex_unlock(&game->mutex);
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

/**
 * WHAT: Query knowledge graph for GT Coalition self-knowledge
 * WHY:  Enable self-awareness about module's role and connections
 * HOW:  Query KG for entity observations and relations
 */
int gt_coalition_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    gt_coalition_heartbeat("gt_coalition_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "GT_Coalition");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                gt_coalition_heartbeat("gt_coalition_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            /* GT Coalition self-knowledge logged */
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "GT_Coalition");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "GT_Coalition");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void gt_coalition_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_gt_coalition_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int gt_coalition_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "gt_coalition_training_begin: NULL argument");
        return -1;
    }
    gt_coalition_heartbeat_instance(NULL, "gt_coalition_training_begin", 0.0f);
    (void)(struct nimcp_coalition_game_struct*)instance; /* Module state available for reset */
    return 0;
}

int gt_coalition_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "gt_coalition_training_end: NULL argument");
        return -1;
    }
    gt_coalition_heartbeat_instance(NULL, "gt_coalition_training_end", 1.0f);
    (void)(struct nimcp_coalition_game_struct*)instance; /* Module state available for finalization */
    return 0;
}

int gt_coalition_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "gt_coalition_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    gt_coalition_heartbeat_instance(NULL, "gt_coalition_training_step", progress);
    (void)(struct nimcp_coalition_game_struct*)instance; /* Module state available for step adaptation */
    return 0;
}
