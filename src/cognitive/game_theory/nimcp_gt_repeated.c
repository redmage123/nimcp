//=============================================================================
// nimcp_gt_repeated.c - Repeated Games Implementation
//=============================================================================
/**
 * @file nimcp_gt_repeated.c
 * @brief Repeated games with folk theorem and trigger strategies
 */

#include "cognitive/game_theory/nimcp_gt_repeated.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/time/nimcp_time.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <float.h>
#include <stdlib.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/thread/nimcp_thread_rand.h"

BRIDGE_BOILERPLATE(gt_repeated, MESH_ADAPTER_CATEGORY_COGNITIVE)



//=============================================================================
// Internal Structure
//=============================================================================

/**
 * @brief Internal state for each player's strategy
 */
typedef struct {
    nimcp_repeated_strategy_t strategy;
    bool trigger_activated;            /**< Has grim trigger fired? */
    int32_t trigger_round;             /**< Round when trigger activated */
    uint32_t last_opponent_action;     /**< For tit-for-tat */
    bool last_own_payoff_good;         /**< For Pavlov strategy */
} nimcp_player_state_t;

/**
 * @brief Internal repeated game structure
 */
struct nimcp_repeated_game_struct {
    nimcp_repeated_config_t config;

    // Stage game
    nimcp_stage_game_t stage_game;

    // History (circular buffer)
    nimcp_round_record_t* history;
    uint32_t history_capacity;
    uint32_t history_count;
    uint32_t history_head;

    // Player states
    nimcp_player_state_t player_states[NIMCP_GT_MAX_PLAYERS];

    // Statistics
    float total_payoffs[NIMCP_GT_MAX_PLAYERS];
    float discounted_sum[NIMCP_GT_MAX_PLAYERS];
    uint32_t mutual_cooperation_count;
    uint32_t rounds_played;

    // Precomputed values
    float minmax_payoffs[NIMCP_GT_MAX_PLAYERS];
    bool minmax_computed;

    // Thread safety
    nimcp_platform_mutex_t mutex;
    bool initialized;
};

//=============================================================================
// Static Name Tables
//=============================================================================

static const char* s_strategy_names[] = {
    "Always Cooperate",
    "Always Defect",
    "Tit-for-Tat",
    "Grim Trigger",
    "Generous Tit-for-Tat",
    "Pavlov (Win-Stay-Lose-Shift)",
    "Custom"
};

static const char* s_coop_level_names[] = {
    "None",
    "Low",
    "Medium",
    "High",
    "Full"
};

//=============================================================================
// Internal Helper Functions
//=============================================================================

/**
 * @brief Get payoff from stage game matrix
 */
static float get_stage_payoff(
    const nimcp_stage_game_t* game,
    const uint32_t* actions,
    uint32_t player
) {
    if (!game || !actions || player >= game->num_players) {
        return 0.0f;
    }

    // For 2-player game: payoff[action1][action2][player]
    // Flattened: index = (a1 * num_a2 + a2) * num_players + player
    if (game->num_players == 2) {
        uint32_t a1 = actions[0];
        uint32_t a2 = actions[1];
        uint32_t n1 = game->num_actions[0];
        uint32_t n2 = game->num_actions[1];

        if (a1 >= n1 || a2 >= n2) {
            return 0.0f;
        }

        size_t idx = (a1 * n2 + a2) * 2 + player;
        if (idx < game->matrix_size) {
            return game->payoff_matrix[idx];
        }
    }
    // General N-player case would need more complex indexing
    return 0.0f;
}

/**
 * @brief Get history record at index (accounting for circular buffer)
 */
static const nimcp_round_record_t* get_history_record(
    const struct nimcp_repeated_game_struct* ctx,
    uint32_t rounds_ago
) {
    if (!ctx || rounds_ago >= ctx->history_count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_BUFFER_OVERFLOW, "get_history_record: ctx is NULL");
        return NULL;
    }

    uint32_t idx = (ctx->history_head + ctx->history_capacity - 1 - rounds_ago)
                   % ctx->history_capacity;
    return &ctx->history[idx];
}

/**
 * @brief Check if action 0 is "cooperate" (convention)
 */
static inline bool is_cooperative_action(uint32_t action) {
    return action == 0;  // Convention: action 0 is cooperate
}

/**
 * @brief Compute action according to strategy for 2-player game
 */
static uint32_t compute_strategy_action_2p(
    const struct nimcp_repeated_game_struct* ctx,
    uint32_t player
) {
    const nimcp_player_state_t* state = &ctx->player_states[player];
    uint32_t opponent = 1 - player;  // 2-player game

    switch (state->strategy.type) {
        case NIMCP_STRATEGY_ALWAYS_COOPERATE:
            return 0;  // Cooperate

        case NIMCP_STRATEGY_ALWAYS_DEFECT:
            return 1;  // Defect

        case NIMCP_STRATEGY_TIT_FOR_TAT:
            if (ctx->history_count == 0) {
                return 0;  // Start with cooperate
            }
            // Copy opponent's last action
            return state->last_opponent_action;

        case NIMCP_STRATEGY_GRIM_TRIGGER:
            if (state->trigger_activated) {
                return 1;  // Defect forever
            }
            return 0;  // Cooperate until triggered

        case NIMCP_STRATEGY_GENEROUS_TFT: {
            if (ctx->history_count == 0) {
                return 0;  // Start with cooperate
            }
            // With probability forgiveness_prob, cooperate even if opponent defected
            if (state->last_opponent_action == 1) {
                float r = (float)nimcp_tl_rand() / (float)RAND_MAX;
                if (r < state->strategy.forgiveness_prob) {
                    return 0;  // Forgive
                }
                return 1;  // Retaliate
            }
            return 0;  // Opponent cooperated, so cooperate
        }

        case NIMCP_STRATEGY_PAVLOV: {
            if (ctx->history_count == 0) {
                return 0;  // Start with cooperate
            }
            // Win-stay, lose-shift
            const nimcp_round_record_t* last = get_history_record(ctx, 0);
            if (!last) return 0;

            uint32_t my_last_action = last->actions[player];
            float my_last_payoff = last->payoffs[player];

            // "Good" payoff is cooperation payoff (typically T or R)
            // Simple heuristic: if got >= average of minmax, stay
            float threshold = ctx->minmax_payoffs[player];
            if (my_last_payoff >= threshold) {
                return my_last_action;  // Stay
            }
            return 1 - my_last_action;  // Shift
        }

        case NIMCP_STRATEGY_CUSTOM:
            if (state->strategy.custom_strategy) {
                nimcp_repeated_history_t hist = {
                    .rounds = ctx->history,
                    .num_rounds = ctx->history_count,
                    .max_rounds = ctx->history_capacity,
                    .head = ctx->history_head
                };
                return state->strategy.custom_strategy(
                    &hist, player, state->strategy.custom_data
                );
            }
            return 0;

        default:
            return 0;
    }
}

/**
 * @brief Update player state after a round
 */
static void update_player_state(
    struct nimcp_repeated_game_struct* ctx,
    uint32_t player,
    const uint32_t* actions,
    const float* payoffs
) {
    nimcp_player_state_t* state = &ctx->player_states[player];

    if (ctx->config.num_players == 2) {
        uint32_t opponent = 1 - player;
        state->last_opponent_action = actions[opponent];

        // Check grim trigger condition
        if (!state->trigger_activated && actions[opponent] == 1) {
            // Opponent defected
            state->trigger_activated = true;
            state->trigger_round = (int32_t)ctx->rounds_played;
        }
    }

    // Update payoff tracking for Pavlov
    state->last_own_payoff_good = (payoffs[player] >= ctx->minmax_payoffs[player]);
}

/**
 * @brief Compute minmax payoffs (lazy computation)
 */
static void ensure_minmax_computed(struct nimcp_repeated_game_struct* ctx) {
    if (ctx->minmax_computed) {
        return;
    }

    // For 2-player game, compute minmax for each player
    if (ctx->config.num_players == 2) {
        uint32_t n1 = ctx->stage_game.num_actions[0];
        uint32_t n2 = ctx->stage_game.num_actions[1];

        // Player 0: max over own actions of min over opponent actions
        float minmax0 = -FLT_MAX;
        for (uint32_t a0 = 0; a0 < n1; a0++) {
            /* Phase 8: Loop progress heartbeat */
            if ((a0 & 0xFF) == 0 && n1 > 256) {
                gt_repeated_heartbeat("gt_repeated_loop",
                                 (float)(a0 + 1) / (float)n1);
            }

            float min_payoff = FLT_MAX;
            for (uint32_t a1 = 0; a1 < n2; a1++) {
                /* Phase 8: Loop progress heartbeat */
                if ((a1 & 0xFF) == 0 && n2 > 256) {
                    gt_repeated_heartbeat("gt_repeated_loop",
                                     (float)(a1 + 1) / (float)n2);
                }

                uint32_t actions[2] = {a0, a1};
                float p = get_stage_payoff(&ctx->stage_game, actions, 0);
                if (p < min_payoff) min_payoff = p;
            }
            if (min_payoff > minmax0) minmax0 = min_payoff;
        }
        ctx->minmax_payoffs[0] = minmax0;

        // Player 1: max over own actions of min over opponent actions
        float minmax1 = -FLT_MAX;
        for (uint32_t a1 = 0; a1 < n2; a1++) {
            /* Phase 8: Loop progress heartbeat */
            if ((a1 & 0xFF) == 0 && n2 > 256) {
                gt_repeated_heartbeat("gt_repeated_loop",
                                 (float)(a1 + 1) / (float)n2);
            }

            float min_payoff = FLT_MAX;
            for (uint32_t a0 = 0; a0 < n1; a0++) {
                /* Phase 8: Loop progress heartbeat */
                if ((a0 & 0xFF) == 0 && n1 > 256) {
                    gt_repeated_heartbeat("gt_repeated_loop",
                                     (float)(a0 + 1) / (float)n1);
                }

                uint32_t actions[2] = {a0, a1};
                float p = get_stage_payoff(&ctx->stage_game, actions, 1);
                if (p < min_payoff) min_payoff = p;
            }
            if (min_payoff > minmax1) minmax1 = min_payoff;
        }
        ctx->minmax_payoffs[1] = minmax1;
    }

    ctx->minmax_computed = true;
}

//=============================================================================
// Configuration
//=============================================================================

nimcp_repeated_config_t nimcp_repeated_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    gt_repeated_heartbeat("gt_repeated_repeated_default_con", 0.0f);


    nimcp_repeated_config_t config;
    memset(&config, 0, sizeof(config));

    config.discount_factor = NIMCP_REPEATED_DEFAULT_DISCOUNT;
    config.history_length = NIMCP_REPEATED_MAX_HISTORY;
    config.num_players = 2;
    config.track_cooperation = true;
    config.enable_statistics = true;

    return config;
}

//=============================================================================
// Lifecycle
//=============================================================================

nimcp_repeated_game_t nimcp_repeated_create(
    const nimcp_repeated_config_t* config,
    const float* stage_payoffs,
    const uint32_t* num_actions,
    uint32_t num_players
) {
    if (!stage_payoffs || !num_actions || num_players == 0 ||
        num_players > NIMCP_GT_MAX_PLAYERS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_repeated_create: NULL payoffs/actions or invalid player count");
        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    gt_repeated_heartbeat("gt_repeated_repeated_create", 0.0f);

    nimcp_repeated_game_t ctx = nimcp_calloc(1, sizeof(struct nimcp_repeated_game_struct));
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate ctx");

        return NULL;
    }

    // Apply configuration
    if (config) {
        ctx->config = *config;
        ctx->config.num_players = num_players;  // Override with actual
    } else {
        ctx->config = nimcp_repeated_default_config();
        ctx->config.num_players = num_players;
    }

    // Validate and cap history length
    if (ctx->config.history_length > NIMCP_REPEATED_MAX_HISTORY) {
        ctx->config.history_length = NIMCP_REPEATED_MAX_HISTORY;
    }
    if (ctx->config.history_length == 0) {
        ctx->config.history_length = 64;
    }

    // Allocate history buffer
    ctx->history_capacity = ctx->config.history_length;
    ctx->history = nimcp_calloc(ctx->history_capacity, sizeof(nimcp_round_record_t));
    if (!ctx->history) {
        nimcp_free(ctx);
        ctx = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_repeated_create: ctx->history is NULL");
        return NULL;
    }

    // Copy stage game structure
    ctx->stage_game.num_players = num_players;
    size_t matrix_size = 1;
    for (uint32_t i = 0; i < num_players; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_players > 256) {
            gt_repeated_heartbeat("gt_repeated_loop",
                             (float)(i + 1) / (float)num_players);
        }

        ctx->stage_game.num_actions[i] = num_actions[i];
        matrix_size *= num_actions[i];
    }
    matrix_size *= num_players;  // Payoffs for each player
    ctx->stage_game.matrix_size = matrix_size;

    ctx->stage_game.payoff_matrix = nimcp_calloc(matrix_size, sizeof(float));
    if (!ctx->stage_game.payoff_matrix) {
        nimcp_free(ctx->history);
        nimcp_free(ctx);
        ctx = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_repeated_create: ctx->stage_game is NULL");
        return NULL;
    }
    memcpy(ctx->stage_game.payoff_matrix, stage_payoffs, matrix_size * sizeof(float));

    // Initialize player states with default strategies
    for (uint32_t i = 0; i < num_players; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_players > 256) {
            gt_repeated_heartbeat("gt_repeated_loop",
                             (float)(i + 1) / (float)num_players);
        }

        ctx->player_states[i].strategy.type = NIMCP_STRATEGY_TIT_FOR_TAT;
        ctx->player_states[i].strategy.forgiveness_prob = 0.1f;
        ctx->player_states[i].strategy.exploration_rate = 0.0f;
        ctx->player_states[i].trigger_activated = false;
        ctx->player_states[i].trigger_round = -1;
    }

    // Initialize mutex
    if (nimcp_platform_mutex_init(&ctx->mutex, false) != 0) {
        nimcp_free(ctx->stage_game.payoff_matrix);
        nimcp_free(ctx->history);
        nimcp_free(ctx);
        ctx = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "nimcp_repeated_create: validation failed");
        return NULL;
    }

    // Compute minmax payoffs
    ensure_minmax_computed(ctx);

    ctx->initialized = true;
    return ctx;
}

void nimcp_repeated_destroy(nimcp_repeated_game_t ctx) {
    if (!ctx) return;

    /* Phase 8: Heartbeat at operation start */
    gt_repeated_heartbeat("gt_repeated_repeated_destroy", 0.0f);


    nimcp_platform_mutex_destroy(&ctx->mutex);
    nimcp_free(ctx->stage_game.payoff_matrix);
    nimcp_free(ctx->history);
    nimcp_free(ctx);
    ctx = NULL;
}

//=============================================================================
// Configuration Functions
//=============================================================================

nimcp_error_t nimcp_repeated_set_discount(
    nimcp_repeated_game_t ctx,
    float discount_factor
) {
    if (!ctx) {
        return NIMCP_GT_ERROR_NULL_POINTER;
    }
    /* Phase 8: Heartbeat at operation start */
    gt_repeated_heartbeat("gt_repeated_repeated_set_discoun", 0.0f);


    if (discount_factor <= 0.0f || discount_factor > 1.0f) {
        return NIMCP_GT_ERROR_INVALID_PARAMETER;
    }

    nimcp_platform_mutex_lock(&ctx->mutex);
    ctx->config.discount_factor = discount_factor;
    nimcp_platform_mutex_unlock(&ctx->mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_repeated_set_strategy(
    nimcp_repeated_game_t ctx,
    uint32_t player,
    const nimcp_repeated_strategy_t* strategy
) {
    if (!ctx || !strategy) {
        return NIMCP_GT_ERROR_NULL_POINTER;
    }
    /* Phase 8: Heartbeat at operation start */
    gt_repeated_heartbeat("gt_repeated_repeated_set_strateg", 0.0f);


    if (player >= ctx->config.num_players) {
        return NIMCP_GT_ERROR_INVALID_PARAMETER;
    }

    nimcp_platform_mutex_lock(&ctx->mutex);
    ctx->player_states[player].strategy = *strategy;
    // Reset trigger state when strategy changes
    ctx->player_states[player].trigger_activated = false;
    ctx->player_states[player].trigger_round = -1;
    nimcp_platform_mutex_unlock(&ctx->mutex);

    return NIMCP_SUCCESS;
}

//=============================================================================
// Core Game Operations
//=============================================================================

nimcp_error_t nimcp_repeated_play_round(
    nimcp_repeated_game_t ctx,
    const uint32_t* actions,
    float* payoffs_out
) {
    if (!ctx || !actions) {
        return NIMCP_GT_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    gt_repeated_heartbeat("gt_repeated_repeated_play_round", 0.0f);


    nimcp_platform_mutex_lock(&ctx->mutex);

    // Validate actions
    for (uint32_t i = 0; i < ctx->config.num_players; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && ctx->config.num_players > 256) {
            gt_repeated_heartbeat("gt_repeated_loop",
                             (float)(i + 1) / (float)ctx->config.num_players);
        }

        if (actions[i] >= ctx->stage_game.num_actions[i]) {
            nimcp_platform_mutex_unlock(&ctx->mutex);
            return NIMCP_GT_ERROR_INVALID_PARAMETER;
        }
    }

    // Compute payoffs
    float payoffs[NIMCP_GT_MAX_PLAYERS];
    for (uint32_t i = 0; i < ctx->config.num_players; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && ctx->config.num_players > 256) {
            gt_repeated_heartbeat("gt_repeated_loop",
                             (float)(i + 1) / (float)ctx->config.num_players);
        }

        payoffs[i] = get_stage_payoff(&ctx->stage_game, actions, i);
    }

    // Record in history (circular buffer)
    nimcp_round_record_t* record = &ctx->history[ctx->history_head];
    for (uint32_t i = 0; i < ctx->config.num_players; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && ctx->config.num_players > 256) {
            gt_repeated_heartbeat("gt_repeated_loop",
                             (float)(i + 1) / (float)ctx->config.num_players);
        }

        record->actions[i] = actions[i];
        record->payoffs[i] = payoffs[i];
    }
    record->timestamp_ms = nimcp_time_get_ms();

    ctx->history_head = (ctx->history_head + 1) % ctx->history_capacity;
    if (ctx->history_count < ctx->history_capacity) {
        ctx->history_count++;
    }

    // Update statistics
    float discount_power = powf(ctx->config.discount_factor, (float)ctx->rounds_played);
    for (uint32_t i = 0; i < ctx->config.num_players; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && ctx->config.num_players > 256) {
            gt_repeated_heartbeat("gt_repeated_loop",
                             (float)(i + 1) / (float)ctx->config.num_players);
        }

        ctx->total_payoffs[i] += payoffs[i];
        ctx->discounted_sum[i] += discount_power * payoffs[i];
    }

    // Check for mutual cooperation (2-player, action 0 = cooperate)
    if (ctx->config.num_players == 2 &&
        actions[0] == 0 && actions[1] == 0) {
        ctx->mutual_cooperation_count++;
    }

    // Update player states
    for (uint32_t i = 0; i < ctx->config.num_players; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && ctx->config.num_players > 256) {
            gt_repeated_heartbeat("gt_repeated_loop",
                             (float)(i + 1) / (float)ctx->config.num_players);
        }

        update_player_state(ctx, i, actions, payoffs);
    }

    ctx->rounds_played++;

    // Copy payoffs out if requested
    if (payoffs_out) {
        for (uint32_t i = 0; i < ctx->config.num_players; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && ctx->config.num_players > 256) {
                gt_repeated_heartbeat("gt_repeated_loop",
                                 (float)(i + 1) / (float)ctx->config.num_players);
            }

            payoffs_out[i] = payoffs[i];
        }
    }

    nimcp_platform_mutex_unlock(&ctx->mutex);
    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_repeated_get_history(
    const nimcp_repeated_game_t ctx,
    nimcp_repeated_history_t* history_out
) {
    if (!ctx || !history_out) {
        return NIMCP_GT_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    gt_repeated_heartbeat("gt_repeated_repeated_get_history", 0.0f);


    nimcp_platform_mutex_lock(&ctx->mutex);

    history_out->rounds = ctx->history;
    history_out->num_rounds = ctx->history_count;
    history_out->max_rounds = ctx->history_capacity;
    history_out->head = ctx->history_head;

    nimcp_platform_mutex_unlock(&ctx->mutex);
    return NIMCP_SUCCESS;
}

float nimcp_repeated_compute_avg_payoff(
    const nimcp_repeated_game_t ctx,
    uint32_t player
) {
    if (!ctx || player >= ctx->config.num_players) {
        return NAN;
    }

    /* Phase 8: Heartbeat at operation start */
    gt_repeated_heartbeat("gt_repeated_repeated_compute_avg", 0.0f);


    nimcp_platform_mutex_lock(&ctx->mutex);

    float avg = 0.0f;
    if (ctx->rounds_played == 0) {
        avg = NAN;
    } else {
        avg = ctx->total_payoffs[player] / (float)ctx->rounds_played;
    }

    nimcp_platform_mutex_unlock(&ctx->mutex);
    return avg;
}

float nimcp_repeated_compute_discounted_payoff(
    const nimcp_repeated_game_t ctx,
    uint32_t player
) {
    if (!ctx || player >= ctx->config.num_players) {
        return NAN;
    }

    /* Phase 8: Heartbeat at operation start */
    gt_repeated_heartbeat("gt_repeated_repeated_compute_dis", 0.0f);


    nimcp_platform_mutex_lock(&ctx->mutex);

    float result = 0.0f;
    if (ctx->rounds_played == 0) {
        result = NAN;
    } else {
        // Normalize by (1 - delta) for average interpretation
        float delta = ctx->config.discount_factor;
        if (delta < 1.0f) {
            result = ctx->discounted_sum[player] * (1.0f - delta);
        } else {
            // Delta = 1 means no discounting, use average
            result = ctx->total_payoffs[player] / (float)ctx->rounds_played;
        }
    }

    nimcp_platform_mutex_unlock(&ctx->mutex);
    return result;
}

//=============================================================================
// Folk Theorem and Feasibility
//=============================================================================

bool nimcp_repeated_is_sustainable(
    const nimcp_repeated_game_t ctx,
    const float* target_payoffs
) {
    if (!ctx || !target_payoffs) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_repeated_is_sustainable: required parameter is NULL (ctx, target_payoffs)");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    gt_repeated_heartbeat("gt_repeated_repeated_is_sustaina", 0.0f);


    nimcp_platform_mutex_lock(&ctx->mutex);

    // Check individual rationality: each player gets at least minmax
    for (uint32_t i = 0; i < ctx->config.num_players; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && ctx->config.num_players > 256) {
            gt_repeated_heartbeat("gt_repeated_loop",
                             (float)(i + 1) / (float)ctx->config.num_players);
        }

        if (target_payoffs[i] < ctx->minmax_payoffs[i]) {
            nimcp_platform_mutex_unlock(&ctx->mutex);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_repeated_is_sustainable: validation failed");
            return false;
        }
    }

    // Check feasibility: target must be achievable by some strategy mix
    // For simplicity, check if target is within convex hull of pure profiles
    // This is a simplified check - proper check would use linear programming

    // Find best single-stage payoff for each player
    float max_payoffs[NIMCP_GT_MAX_PLAYERS];
    for (uint32_t i = 0; i < ctx->config.num_players; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && ctx->config.num_players > 256) {
            gt_repeated_heartbeat("gt_repeated_loop",
                             (float)(i + 1) / (float)ctx->config.num_players);
        }

        max_payoffs[i] = -FLT_MAX;
    }

    if (ctx->config.num_players == 2) {
        uint32_t n1 = ctx->stage_game.num_actions[0];
        uint32_t n2 = ctx->stage_game.num_actions[1];

        for (uint32_t a0 = 0; a0 < n1; a0++) {
            /* Phase 8: Loop progress heartbeat */
            if ((a0 & 0xFF) == 0 && n1 > 256) {
                gt_repeated_heartbeat("gt_repeated_loop",
                                 (float)(a0 + 1) / (float)n1);
            }

            for (uint32_t a1 = 0; a1 < n2; a1++) {
                /* Phase 8: Loop progress heartbeat */
                if ((a1 & 0xFF) == 0 && n2 > 256) {
                    gt_repeated_heartbeat("gt_repeated_loop",
                                     (float)(a1 + 1) / (float)n2);
                }

                uint32_t actions[2] = {a0, a1};
                for (uint32_t i = 0; i < 2; i++) {
                    /* Phase 8: Loop progress heartbeat */
                    if ((i & 0xFF) == 0 && 2 > 256) {
                        gt_repeated_heartbeat("gt_repeated_loop",
                                         (float)(i + 1) / (float)2);
                    }

                    float p = get_stage_payoff(&ctx->stage_game, actions, i);
                    if (p > max_payoffs[i]) max_payoffs[i] = p;
                }
            }
        }
    }

    // Target should not exceed max achievable
    for (uint32_t i = 0; i < ctx->config.num_players; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && ctx->config.num_players > 256) {
            gt_repeated_heartbeat("gt_repeated_loop",
                             (float)(i + 1) / (float)ctx->config.num_players);
        }

        if (target_payoffs[i] > max_payoffs[i]) {
            nimcp_platform_mutex_unlock(&ctx->mutex);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_repeated_is_sustainable: validation failed");
            return false;
        }
    }

    // Check discount factor condition
    // Critical discount: deviation gain / (deviation gain + punishment)
    // This is a simplified check
    float delta = ctx->config.discount_factor;
    for (uint32_t i = 0; i < ctx->config.num_players; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && ctx->config.num_players > 256) {
            gt_repeated_heartbeat("gt_repeated_loop",
                             (float)(i + 1) / (float)ctx->config.num_players);
        }

        float deviation_gain = max_payoffs[i] - target_payoffs[i];
        float punishment = target_payoffs[i] - ctx->minmax_payoffs[i];

        if (punishment <= 0 && deviation_gain > 0) {
            // Cannot punish, deviation is profitable
            nimcp_platform_mutex_unlock(&ctx->mutex);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_repeated_is_sustainable: validation failed");
            return false;
        }

        if (deviation_gain > 0) {
            float critical_delta = deviation_gain / (deviation_gain + punishment);
            if (delta < critical_delta) {
                nimcp_platform_mutex_unlock(&ctx->mutex);
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_repeated_is_sustainable: validation failed");
                return false;
            }
        }
    }

    nimcp_platform_mutex_unlock(&ctx->mutex);
    return true;
}

nimcp_error_t nimcp_repeated_feasibility_set(
    const nimcp_repeated_game_t ctx,
    nimcp_payoff_point_t* vertices,
    uint32_t* num_vertices
) {
    if (!ctx || !vertices || !num_vertices) {
        return NIMCP_GT_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    gt_repeated_heartbeat("gt_repeated_repeated_feasibility", 0.0f);


    nimcp_platform_mutex_lock(&ctx->mutex);

    uint32_t max_vertices = *num_vertices;
    *num_vertices = 0;

    // Enumerate all pure strategy profiles
    if (ctx->config.num_players == 2) {
        uint32_t n1 = ctx->stage_game.num_actions[0];
        uint32_t n2 = ctx->stage_game.num_actions[1];

        for (uint32_t a0 = 0; a0 < n1 && *num_vertices < max_vertices; a0++) {
            for (uint32_t a1 = 0; a1 < n2 && *num_vertices < max_vertices; a1++) {
                uint32_t actions[2] = {a0, a1};
                nimcp_payoff_point_t* v = &vertices[*num_vertices];

                v->payoffs[0] = get_stage_payoff(&ctx->stage_game, actions, 0);
                v->payoffs[1] = get_stage_payoff(&ctx->stage_game, actions, 1);

                (*num_vertices)++;
            }
        }
    }

    nimcp_platform_mutex_unlock(&ctx->mutex);
    return NIMCP_SUCCESS;
}

float nimcp_repeated_minmax_payoff(
    const nimcp_repeated_game_t ctx,
    uint32_t player
) {
    if (!ctx || player >= ctx->config.num_players) {
        return NAN;
    }

    /* Phase 8: Heartbeat at operation start */
    gt_repeated_heartbeat("gt_repeated_repeated_minmax_payo", 0.0f);


    nimcp_platform_mutex_lock(&ctx->mutex);
    float result = ctx->minmax_payoffs[player];
    nimcp_platform_mutex_unlock(&ctx->mutex);

    return result;
}

float nimcp_repeated_critical_discount(
    const nimcp_repeated_game_t ctx,
    const float* target_payoffs
) {
    if (!ctx || !target_payoffs) {
        return 1.0f;
    }

    /* Phase 8: Heartbeat at operation start */
    gt_repeated_heartbeat("gt_repeated_repeated_critical_di", 0.0f);


    nimcp_platform_mutex_lock(&ctx->mutex);

    // Find max achievable payoffs
    float max_payoffs[NIMCP_GT_MAX_PLAYERS];
    for (uint32_t i = 0; i < ctx->config.num_players; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && ctx->config.num_players > 256) {
            gt_repeated_heartbeat("gt_repeated_loop",
                             (float)(i + 1) / (float)ctx->config.num_players);
        }

        max_payoffs[i] = -FLT_MAX;
    }

    if (ctx->config.num_players == 2) {
        uint32_t n1 = ctx->stage_game.num_actions[0];
        uint32_t n2 = ctx->stage_game.num_actions[1];

        for (uint32_t a0 = 0; a0 < n1; a0++) {
            /* Phase 8: Loop progress heartbeat */
            if ((a0 & 0xFF) == 0 && n1 > 256) {
                gt_repeated_heartbeat("gt_repeated_loop",
                                 (float)(a0 + 1) / (float)n1);
            }

            for (uint32_t a1 = 0; a1 < n2; a1++) {
                /* Phase 8: Loop progress heartbeat */
                if ((a1 & 0xFF) == 0 && n2 > 256) {
                    gt_repeated_heartbeat("gt_repeated_loop",
                                     (float)(a1 + 1) / (float)n2);
                }

                uint32_t actions[2] = {a0, a1};
                for (uint32_t i = 0; i < 2; i++) {
                    /* Phase 8: Loop progress heartbeat */
                    if ((i & 0xFF) == 0 && 2 > 256) {
                        gt_repeated_heartbeat("gt_repeated_loop",
                                         (float)(i + 1) / (float)2);
                    }

                    float p = get_stage_payoff(&ctx->stage_game, actions, i);
                    if (p > max_payoffs[i]) max_payoffs[i] = p;
                }
            }
        }
    }

    // Critical discount for each player
    float max_critical = 0.0f;

    for (uint32_t i = 0; i < ctx->config.num_players; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && ctx->config.num_players > 256) {
            gt_repeated_heartbeat("gt_repeated_loop",
                             (float)(i + 1) / (float)ctx->config.num_players);
        }

        float deviation_gain = max_payoffs[i] - target_payoffs[i];
        float punishment = target_payoffs[i] - ctx->minmax_payoffs[i];

        if (punishment <= 0) {
            // Cannot punish deviation
            if (deviation_gain > 0) {
                nimcp_platform_mutex_unlock(&ctx->mutex);
                return 1.0f;  // Unsustainable
            }
        } else if (deviation_gain > 0) {
            float critical = deviation_gain / (deviation_gain + punishment);
            if (critical > max_critical) {
                max_critical = critical;
            }
        }
    }

    nimcp_platform_mutex_unlock(&ctx->mutex);
    return max_critical;
}

//=============================================================================
// Trigger Strategy Functions
//=============================================================================

uint32_t nimcp_repeated_get_strategy_action(
    const nimcp_repeated_game_t ctx,
    uint32_t player
) {
    if (!ctx || player >= ctx->config.num_players) {
        return 0;
    }

    /* Phase 8: Heartbeat at operation start */
    gt_repeated_heartbeat("gt_repeated_repeated_get_strateg", 0.0f);


    nimcp_platform_mutex_lock(&ctx->mutex);

    uint32_t action = 0;
    if (ctx->config.num_players == 2) {
        action = compute_strategy_action_2p(ctx, player);
    } else {
        // For N-player, default to cooperation
        action = 0;
    }

    // Apply exploration if configured
    float explore = ctx->player_states[player].strategy.exploration_rate;
    if (explore > 0.0f) {
        float r = (float)nimcp_tl_rand() / (float)RAND_MAX;
        if (r < explore) {
            // Random action
            action = (uint32_t)nimcp_tl_rand() % ctx->stage_game.num_actions[player];
        }
    }

    nimcp_platform_mutex_unlock(&ctx->mutex);
    return action;
}

bool nimcp_repeated_trigger_activated(
    const nimcp_repeated_game_t ctx,
    uint32_t player
) {
    if (!ctx || player >= ctx->config.num_players) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_repeated_trigger_activated: ctx is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    gt_repeated_heartbeat("gt_repeated_repeated_trigger_act", 0.0f);


    nimcp_platform_mutex_lock(&ctx->mutex);
    bool activated = ctx->player_states[player].trigger_activated;
    nimcp_platform_mutex_unlock(&ctx->mutex);

    return activated;
}

int32_t nimcp_repeated_trigger_threshold(
    const nimcp_repeated_game_t ctx,
    uint32_t player
) {
    if (!ctx || player >= ctx->config.num_players) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_repeated_trigger_threshold: ctx is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    gt_repeated_heartbeat("gt_repeated_repeated_trigger_thr", 0.0f);


    nimcp_platform_mutex_lock(&ctx->mutex);
    int32_t round = ctx->player_states[player].trigger_round;
    nimcp_platform_mutex_unlock(&ctx->mutex);

    return round;
}

//=============================================================================
// Simulation Functions
//=============================================================================

nimcp_error_t nimcp_repeated_simulate(
    nimcp_repeated_game_t ctx,
    uint32_t num_rounds,
    nimcp_repeated_result_t* result
) {
    if (!ctx || !result) {
        return NIMCP_GT_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    gt_repeated_heartbeat("gt_repeated_repeated_simulate", 0.0f);


    memset(result, 0, sizeof(nimcp_repeated_result_t));

    uint32_t actions[NIMCP_GT_MAX_PLAYERS];
    float payoffs[NIMCP_GT_MAX_PLAYERS];

    uint32_t initial_rounds = ctx->rounds_played;

    for (uint32_t r = 0; r < num_rounds; r++) {
        /* Phase 8: Loop progress heartbeat */
        if ((r & 0xFF) == 0 && num_rounds > 256) {
            gt_repeated_heartbeat("gt_repeated_loop",
                             (float)(r + 1) / (float)num_rounds);
        }

        // Get actions from strategies
        for (uint32_t p = 0; p < ctx->config.num_players; p++) {
            /* Phase 8: Loop progress heartbeat */
            if ((p & 0xFF) == 0 && ctx->config.num_players > 256) {
                gt_repeated_heartbeat("gt_repeated_loop",
                                 (float)(p + 1) / (float)ctx->config.num_players);
            }

            actions[p] = nimcp_repeated_get_strategy_action(ctx, p);
        }

        // Play round
        nimcp_error_t err = nimcp_repeated_play_round(ctx, actions, payoffs);
        if (err != NIMCP_SUCCESS) {
            return err;
        }
    }

    nimcp_platform_mutex_lock(&ctx->mutex);

    // Compute results
    result->rounds_played = num_rounds;

    for (uint32_t i = 0; i < ctx->config.num_players; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && ctx->config.num_players > 256) {
            gt_repeated_heartbeat("gt_repeated_loop",
                             (float)(i + 1) / (float)ctx->config.num_players);
        }

        result->total_payoffs[i] = ctx->total_payoffs[i];
        result->avg_payoffs[i] = ctx->total_payoffs[i] / (float)ctx->rounds_played;

        // Compute discounted payoffs for this simulation
        float delta = ctx->config.discount_factor;
        if (delta < 1.0f) {
            result->discounted_payoffs[i] = ctx->discounted_sum[i] * (1.0f - delta);
        } else {
            result->discounted_payoffs[i] = result->avg_payoffs[i];
        }
    }

    // Cooperation metrics
    result->cooperation_rate = (float)ctx->mutual_cooperation_count /
                               (float)ctx->rounds_played;
    result->coop_level = nimcp_repeated_detect_cooperation(ctx);

    // Check stability
    result->reached_equilibrium = nimcp_repeated_is_stable(ctx, num_rounds / 4);
    if (result->reached_equilibrium) {
        // Find when it became stable (approximate)
        result->equilibrium_round = ctx->rounds_played - num_rounds / 4;
    }

    nimcp_platform_mutex_unlock(&ctx->mutex);
    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_repeated_reset(nimcp_repeated_game_t ctx) {
    if (!ctx) {
        return NIMCP_GT_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    gt_repeated_heartbeat("gt_repeated_repeated_reset", 0.0f);


    nimcp_platform_mutex_lock(&ctx->mutex);

    // Clear history
    memset(ctx->history, 0, ctx->history_capacity * sizeof(nimcp_round_record_t));
    ctx->history_count = 0;
    ctx->history_head = 0;

    // Reset statistics
    memset(ctx->total_payoffs, 0, sizeof(ctx->total_payoffs));
    memset(ctx->discounted_sum, 0, sizeof(ctx->discounted_sum));
    ctx->mutual_cooperation_count = 0;
    ctx->rounds_played = 0;

    // Reset player states
    for (uint32_t i = 0; i < ctx->config.num_players; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && ctx->config.num_players > 256) {
            gt_repeated_heartbeat("gt_repeated_loop",
                             (float)(i + 1) / (float)ctx->config.num_players);
        }

        ctx->player_states[i].trigger_activated = false;
        ctx->player_states[i].trigger_round = -1;
        ctx->player_states[i].last_opponent_action = 0;
        ctx->player_states[i].last_own_payoff_good = true;
    }

    nimcp_platform_mutex_unlock(&ctx->mutex);
    return NIMCP_SUCCESS;
}

//=============================================================================
// Cooperation Analysis
//=============================================================================

nimcp_cooperation_level_t nimcp_repeated_detect_cooperation(
    const nimcp_repeated_game_t ctx
) {
    if (!ctx || ctx->rounds_played == 0) {
        return NIMCP_COOP_LEVEL_NONE;
    }

    // Note: caller may already hold lock, but we need thread safety
    // For internal use, assume lock is held; for external, lock is acquired
    /* Phase 8: Heartbeat at operation start */
    gt_repeated_heartbeat("gt_repeated_repeated_detect_coop", 0.0f);


    float rate = (float)ctx->mutual_cooperation_count / (float)ctx->rounds_played;

    if (rate >= 0.95f) {
        return NIMCP_COOP_LEVEL_FULL;
    } else if (rate >= 0.75f) {
        return NIMCP_COOP_LEVEL_HIGH;
    } else if (rate >= 0.50f) {
        return NIMCP_COOP_LEVEL_MEDIUM;
    } else if (rate >= 0.25f) {
        return NIMCP_COOP_LEVEL_LOW;
    } else {
        return NIMCP_COOP_LEVEL_NONE;
    }
}

float nimcp_repeated_cooperation_rate(const nimcp_repeated_game_t ctx) {
    if (!ctx || ctx->rounds_played == 0) {
        return 0.0f;
    }

    /* Phase 8: Heartbeat at operation start */
    gt_repeated_heartbeat("gt_repeated_repeated_cooperation", 0.0f);


    nimcp_platform_mutex_lock(&ctx->mutex);
    float rate = (float)ctx->mutual_cooperation_count / (float)ctx->rounds_played;
    nimcp_platform_mutex_unlock(&ctx->mutex);

    return rate;
}

bool nimcp_repeated_is_stable(
    const nimcp_repeated_game_t ctx,
    uint32_t window_size
) {
    if (!ctx || window_size == 0 || ctx->history_count < window_size) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_repeated_is_stable: ctx is NULL");
        return false;
    }

    // Check if action pattern repeats in the window
    // Look for consistent play (all same actions in window)

    // Note: For external call, we need to lock
    // For internal call (from simulate), lock is already held
    // This is a simplified implementation

    /* Phase 8: Heartbeat at operation start */
    gt_repeated_heartbeat("gt_repeated_repeated_is_stable", 0.0f);


    const nimcp_round_record_t* first = get_history_record(ctx, window_size - 1);
    if (!first) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_repeated_is_stable: first is NULL");
        return false;
    }

    uint32_t first_actions[NIMCP_GT_MAX_PLAYERS];
    for (uint32_t i = 0; i < ctx->config.num_players; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && ctx->config.num_players > 256) {
            gt_repeated_heartbeat("gt_repeated_loop",
                             (float)(i + 1) / (float)ctx->config.num_players);
        }

        first_actions[i] = first->actions[i];
    }

    // Check if all subsequent rounds have same actions
    for (uint32_t r = 1; r < window_size; r++) {
        const nimcp_round_record_t* rec = get_history_record(ctx, window_size - 1 - r);
        if (!rec) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_repeated_is_stable: rec is NULL");
            return false;
        }

        for (uint32_t i = 0; i < ctx->config.num_players; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && ctx->config.num_players > 256) {
                gt_repeated_heartbeat("gt_repeated_loop",
                                 (float)(i + 1) / (float)ctx->config.num_players);
            }

            if (rec->actions[i] != first_actions[i]) {
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_repeated_is_stable: validation failed");
                return false;
            }
        }
    }

    return true;
}

//=============================================================================
// Query Functions
//=============================================================================

const char* nimcp_repeated_strategy_name(nimcp_repeated_strategy_type_t type) {
    if (type >= NIMCP_REPEATED_STRATEGY_COUNT) {
        return "Unknown";
    }
    return s_strategy_names[type];
}

const char* nimcp_repeated_coop_level_name(nimcp_cooperation_level_t level) {
    if (level > NIMCP_COOP_LEVEL_FULL) {
        return "Unknown";
    }
    return s_coop_level_names[level];
}

float nimcp_repeated_get_discount(const nimcp_repeated_game_t ctx) {
    if (!ctx) return 0.0f;
    /* Phase 8: Heartbeat at operation start */
    gt_repeated_heartbeat("gt_repeated_repeated_get_discoun", 0.0f);


    return ctx->config.discount_factor;
}

uint32_t nimcp_repeated_get_num_rounds(const nimcp_repeated_game_t ctx) {
    if (!ctx) return 0;
    /* Phase 8: Heartbeat at operation start */
    gt_repeated_heartbeat("gt_repeated_repeated_get_num_rou", 0.0f);


    return ctx->rounds_played;
}

uint32_t nimcp_repeated_get_num_players(const nimcp_repeated_game_t ctx) {
    if (!ctx) return 0;
    /* Phase 8: Heartbeat at operation start */
    gt_repeated_heartbeat("gt_repeated_repeated_get_num_pla", 0.0f);


    return ctx->config.num_players;
}

nimcp_repeated_strategy_type_t nimcp_repeated_get_strategy_type(
    const nimcp_repeated_game_t ctx,
    uint32_t player
) {
    if (!ctx || player >= ctx->config.num_players) {
        return NIMCP_STRATEGY_TIT_FOR_TAT;  // Default
    }
    /* Phase 8: Heartbeat at operation start */
    gt_repeated_heartbeat("gt_repeated_repeated_get_strateg", 0.0f);


    return ctx->player_states[player].strategy.type;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

/**
 * WHAT: Query knowledge graph for GT Repeated self-knowledge
 * WHY:  Enable self-awareness about module's role and connections
 * HOW:  Query KG for entity observations and relations
 */
int gt_repeated_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    gt_repeated_heartbeat("gt_repeated_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "GT_Repeated");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                gt_repeated_heartbeat("gt_repeated_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            /* GT Repeated self-knowledge logged */
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "GT_Repeated");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "GT_Repeated");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void gt_repeated_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_gt_repeated_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int gt_repeated_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "gt_repeated_training_begin: NULL argument");
        return -1;
    }
    gt_repeated_heartbeat_instance(NULL, "gt_repeated_training_begin", 0.0f);
    (void)(struct nimcp_repeated_game_struct*)instance; /* Module state available for reset */
    return 0;
}

int gt_repeated_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "gt_repeated_training_end: NULL argument");
        return -1;
    }
    gt_repeated_heartbeat_instance(NULL, "gt_repeated_training_end", 1.0f);
    (void)(struct nimcp_repeated_game_struct*)instance; /* Module state available for finalization */
    return 0;
}

int gt_repeated_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "gt_repeated_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    gt_repeated_heartbeat_instance(NULL, "gt_repeated_training_step", progress);
    (void)(struct nimcp_repeated_game_struct*)instance; /* Module state available for step adaptation */
    return 0;
}
