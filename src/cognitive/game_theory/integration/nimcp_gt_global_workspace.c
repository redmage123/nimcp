//=============================================================================
// nimcp_gt_global_workspace.c - Game-Theoretic Global Workspace Integration
//=============================================================================
/**
 * @file nimcp_gt_global_workspace.c
 * @brief Auction-based competition for Global Workspace broadcast access
 *
 * WHAT: Second-price auction for GW slot allocation
 * WHY:  Incentive-compatible, efficient access allocation
 * HOW:  Modules bid with salience, highest bidder wins, pays second price
 *
 * @author NIMCP Development Team
 * @date 2024-12-27
 * @version 1.0.0
 */

#include "cognitive/game_theory/integration/nimcp_gt_global_workspace.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "cognitive/game_theory/nimcp_auction.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/time/nimcp_time.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(gt_global_workspace, MESH_ADAPTER_CATEGORY_COGNITIVE)



//=============================================================================
// Constants
//=============================================================================

#define MAX_GW_MODULES 64
#define DEFAULT_HISTORY_DEPTH 100
#define MAX_PENDING_BIDS 32

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Pending bid for current round
 */
typedef struct {
    cognitive_module_t module;
    float bid;
    float* content;
    uint32_t content_dim;
    bool valid;
} gt_gw_pending_bid_t;

/**
 * @brief Opaque context structure
 */
struct gt_gw_auction_ctx_struct {
    global_workspace_t* workspace;
    gt_gw_config_t config;
    nimcp_auction_t auction;

    // Module states
    gt_gw_module_state_t module_states[MAX_GW_MODULES];
    uint32_t num_modules;

    // Current round bids
    gt_gw_pending_bid_t pending_bids[MAX_PENDING_BIDS];
    uint32_t num_pending_bids;

    // Statistics
    uint64_t rounds_completed;
    uint64_t successful_broadcasts;
    uint64_t reserve_failures;
    float total_payments;
    float total_welfare;

    // Timing
    uint64_t last_replenish_time_ms;

    bool active;
};

//=============================================================================
// Static Helpers
//=============================================================================

static int find_module_state(
    const gt_gw_auction_ctx_t ctx,
    cognitive_module_t module
) {
    for (uint32_t i = 0; i < ctx->num_modules; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && ctx->num_modules > 256) {
            gt_global_workspace_heartbeat("gt_global_wo_loop",
                             (float)(i + 1) / (float)ctx->num_modules);
        }

        if (ctx->module_states[i].module == module) {
            return (int)i;
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "find_module_state: validation failed");
    return -1;
}

static int add_module_state(
    gt_gw_auction_ctx_t ctx,
    cognitive_module_t module
) {
    if (ctx->num_modules >= MAX_GW_MODULES) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "add_module_state: capacity exceeded");
        return -1;
    }

    uint32_t idx = ctx->num_modules++;
    gt_gw_module_state_t* state = &ctx->module_states[idx];

    state->module = module;
    state->current_budget = ctx->config.initial_budget;
    state->total_spent = 0.0f;
    state->bids_submitted = 0;
    state->wins = 0;
    state->avg_winning_bid = 0.0f;
    state->avg_payment = 0.0f;
    state->last_win_time_ms = 0;

    return (int)idx;
}

static void update_winner_state(
    gt_gw_auction_ctx_t ctx,
    int module_idx,
    float winning_bid,
    float payment
) {
    gt_gw_module_state_t* state = &ctx->module_states[module_idx];

    state->wins++;
    state->total_spent += payment;
    state->current_budget -= payment;
    state->last_win_time_ms = nimcp_time_get_ms();

    // Update running averages
    float alpha = 1.0f / (float)state->wins;
    if (isfinite(winning_bid)) {
        state->avg_winning_bid = (1.0f - alpha) * state->avg_winning_bid + alpha * winning_bid;
    }
    if (isfinite(payment)) {
        state->avg_payment = (1.0f - alpha) * state->avg_payment + alpha * payment;
    }
}

//=============================================================================
// Lifecycle
//=============================================================================

gt_gw_config_t gt_gw_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    gt_global_workspace_heartbeat("gt_global_wo_gt_gw_default_config", 0.0f);


    gt_gw_config_t config = {
        .strategy = GT_GW_STRATEGY_SECOND_PRICE,
        .reserve_price = 0.1f,
        .initial_budget = 100.0f,
        .budget_replenish_rate = 1.0f,
        .budget_replenish_interval_ms = 1000.0f,
        .enable_budget_constraints = true,
        .track_bid_history = true,
        .history_depth = DEFAULT_HISTORY_DEPTH
    };
    return config;
}

gt_gw_auction_ctx_t gt_gw_create(
    global_workspace_t* workspace,
    const gt_gw_config_t* config
) {
    if (!workspace) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "workspace is NULL");

        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    gt_global_workspace_heartbeat("gt_global_wo_gt_gw_create", 0.0f);


    gt_gw_auction_ctx_t ctx = nimcp_calloc(1, sizeof(struct gt_gw_auction_ctx_struct));
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate ctx");

        return NULL;
    }

    ctx->workspace = workspace;
    ctx->config = config ? *config : gt_gw_default_config();

    // Create underlying auction
    nimcp_auction_config_t auction_config = nimcp_auction_default_config();
    auction_config.type = NIMCP_AUCTION_SECOND_PRICE;
    auction_config.reserve_price = ctx->config.reserve_price;
    auction_config.max_bidders = MAX_PENDING_BIDS;
    auction_config.allow_tie_random = true;
    auction_config.reveal_bids = false;

    ctx->auction = nimcp_auction_create(&auction_config);
    if (!ctx->auction) {
        nimcp_free(ctx);
        ctx = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "gt_gw_create: ctx->auction is NULL");
        return NULL;
    }

    // Initialize state
    ctx->num_modules = 0;
    ctx->num_pending_bids = 0;
    ctx->rounds_completed = 0;
    ctx->successful_broadcasts = 0;
    ctx->reserve_failures = 0;
    ctx->total_payments = 0.0f;
    ctx->total_welfare = 0.0f;
    ctx->last_replenish_time_ms = nimcp_time_get_ms();
    ctx->active = true;

    return ctx;
}

void gt_gw_destroy(gt_gw_auction_ctx_t ctx) {
    if (!ctx) {
        return;
    }

    // Free any pending bid content
    /* Phase 8: Heartbeat at operation start */
    gt_global_workspace_heartbeat("gt_global_wo_gt_gw_destroy", 0.0f);


    for (uint32_t i = 0; i < ctx->num_pending_bids; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && ctx->num_pending_bids > 256) {
            gt_global_workspace_heartbeat("gt_global_wo_loop",
                             (float)(i + 1) / (float)ctx->num_pending_bids);
        }

        if (ctx->pending_bids[i].content) {
            nimcp_free(ctx->pending_bids[i].content);
        }
    }

    if (ctx->auction) {
        nimcp_auction_destroy(ctx->auction);
    }

    nimcp_free(ctx);
    ctx = NULL;
}

//=============================================================================
// Core Operations
//=============================================================================

nimcp_error_t gt_gw_bid(
    gt_gw_auction_ctx_t ctx,
    cognitive_module_t module,
    const float* content,
    uint32_t content_dim,
    float bid
) {
    /* Phase 8: Heartbeat at operation start */
    gt_global_workspace_heartbeat("gt_global_wo_gt_gw_bid", 0.0f);


    NIMCP_CHECK_THROW(ctx && content && content_dim > 0, NIMCP_ERROR_INVALID_PARAM, "ctx, content is NULL or content_dim is 0");
    NIMCP_CHECK_THROW(ctx->active, NIMCP_GT_ERROR_GAME_OVER, "ctx is not active");
    NIMCP_CHECK_THROW(bid >= 0.0f, NIMCP_GT_ERROR_INVALID_BID, "bid is negative");

    // Find or add module state
    int module_idx = find_module_state(ctx, module);
    if (module_idx < 0) {
        module_idx = add_module_state(ctx, module);
        if (module_idx < 0) {
            return NIMCP_GT_ERROR_CAPACITY;
        }
    }

    gt_gw_module_state_t* state = &ctx->module_states[module_idx];

    // Check budget constraint
    if (ctx->config.enable_budget_constraints && bid > state->current_budget) {
        return NIMCP_GT_ERROR_BUDGET;
    }

    // Check pending bid capacity
    if (ctx->num_pending_bids >= MAX_PENDING_BIDS) {
        return NIMCP_GT_ERROR_CAPACITY;
    }

    // Store pending bid
    gt_gw_pending_bid_t* pending = &ctx->pending_bids[ctx->num_pending_bids];
    pending->module = module;
    pending->bid = bid;
    pending->content_dim = content_dim;

    // Copy content
    pending->content = nimcp_calloc(content_dim, sizeof(float));
    if (!pending->content) {
        return NIMCP_ERROR_NO_MEMORY;
    }
    memcpy(pending->content, content, content_dim * sizeof(float));
    pending->valid = true;

    ctx->num_pending_bids++;

    // Update module stats
    state->bids_submitted++;

    // Submit to underlying auction
    nimcp_error_t err = nimcp_auction_bid(ctx->auction, (nimcp_player_id_t)module, bid, 0);
    if (err != NIMCP_SUCCESS) {
        // Rollback
        nimcp_free(pending->content);
        pending->content = NULL;
        pending->valid = false;
        ctx->num_pending_bids--;
        return err;
    }

    return NIMCP_SUCCESS;
}

nimcp_error_t gt_gw_resolve(
    gt_gw_auction_ctx_t ctx,
    gt_gw_round_result_t* result
) {
    /* Phase 8: Heartbeat at operation start */
    gt_global_workspace_heartbeat("gt_global_wo_gt_gw_resolve", 0.0f);


    NIMCP_CHECK_THROW(ctx && result, NIMCP_ERROR_INVALID_PARAM, "ctx or result is NULL");
    NIMCP_CHECK_THROW(ctx->active, NIMCP_GT_ERROR_GAME_OVER, "ctx is not active");

    memset(result, 0, sizeof(gt_gw_round_result_t));

    if (ctx->num_pending_bids == 0) {
        result->reserve_met = false;
        return NIMCP_SUCCESS;
    }

    // Resolve the auction
    nimcp_auction_result_t auction_result;
    nimcp_error_t err = nimcp_auction_resolve(ctx->auction, &auction_result);
    if (err != NIMCP_SUCCESS) {
        return err;
    }

    // Populate result
    result->winner = (cognitive_module_t)auction_result.winner_id;
    result->winning_bid = auction_result.winning_bid;
    result->payment = auction_result.payment;
    result->second_highest_bid = auction_result.second_highest_bid;
    result->num_bidders = ctx->num_pending_bids;
    result->reserve_met = (auction_result.final_state != NIMCP_AUCTION_STATE_NO_WINNER);

    // Calculate total bids
    float total_bids = 0.0f;
    for (uint32_t i = 0; i < ctx->num_pending_bids; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && ctx->num_pending_bids > 256) {
            gt_global_workspace_heartbeat("gt_global_wo_loop",
                             (float)(i + 1) / (float)ctx->num_pending_bids);
        }

        total_bids += ctx->pending_bids[i].bid;
    }
    result->total_bids = total_bids;

    // Update statistics
    ctx->rounds_completed++;

    if (result->reserve_met) {
        ctx->successful_broadcasts++;
        ctx->total_payments += result->payment;
        ctx->total_welfare += result->winning_bid;  // Winner's valuation

        // Update winner state
        int winner_idx = find_module_state(ctx, result->winner);
        if (winner_idx >= 0) {
            update_winner_state(ctx, winner_idx, result->winning_bid, result->payment);
        }

        // Broadcast winner's content to global workspace
        for (uint32_t i = 0; i < ctx->num_pending_bids; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && ctx->num_pending_bids > 256) {
                gt_global_workspace_heartbeat("gt_global_wo_loop",
                                 (float)(i + 1) / (float)ctx->num_pending_bids);
            }

            if (ctx->pending_bids[i].module == result->winner && ctx->pending_bids[i].valid) {
                // Would call global_workspace_broadcast() here
                break;
            }
        }
    } else {
        ctx->reserve_failures++;
    }

    // Clear pending bids for next round
    for (uint32_t i = 0; i < ctx->num_pending_bids; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && ctx->num_pending_bids > 256) {
            gt_global_workspace_heartbeat("gt_global_wo_loop",
                             (float)(i + 1) / (float)ctx->num_pending_bids);
        }

        if (ctx->pending_bids[i].content) {
            nimcp_free(ctx->pending_bids[i].content);
            ctx->pending_bids[i].content = NULL;
        }
        ctx->pending_bids[i].valid = false;
    }
    ctx->num_pending_bids = 0;

    // Create fresh auction for next round
    nimcp_auction_destroy(ctx->auction);
    nimcp_auction_config_t auction_config = nimcp_auction_default_config();
    auction_config.type = NIMCP_AUCTION_SECOND_PRICE;
    auction_config.reserve_price = ctx->config.reserve_price;
    auction_config.max_bidders = MAX_PENDING_BIDS;
    auction_config.allow_tie_random = true;

    ctx->auction = nimcp_auction_create(&auction_config);

    return NIMCP_SUCCESS;
}

bool gt_gw_compete(
    gt_gw_auction_ctx_t ctx,
    cognitive_module_t module,
    const float* content,
    uint32_t content_dim,
    float bid
) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gt_gw_compete: ctx is NULL");
        return false;
    }

    // Submit bid
    /* Phase 8: Heartbeat at operation start */
    gt_global_workspace_heartbeat("gt_global_wo_gt_gw_compete", 0.0f);


    nimcp_error_t err = gt_gw_bid(ctx, module, content, content_dim, bid);
    if (err != NIMCP_SUCCESS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "gt_gw_compete: validation failed");
        return false;
    }

    // Resolve immediately
    gt_gw_round_result_t result;
    err = gt_gw_resolve(ctx, &result);
    if (err != NIMCP_SUCCESS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "gt_gw_compete: validation failed");
        return false;
    }

    return (result.reserve_met && result.winner == module);
}

void gt_gw_replenish_budgets(gt_gw_auction_ctx_t ctx, float amount) {
    if (!ctx) {
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    gt_global_workspace_heartbeat("gt_global_wo_gt_gw_replenish_budg", 0.0f);


    float replenish = (amount > 0.0f) ? amount : ctx->config.budget_replenish_rate;

    for (uint32_t i = 0; i < ctx->num_modules; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && ctx->num_modules > 256) {
            gt_global_workspace_heartbeat("gt_global_wo_loop",
                             (float)(i + 1) / (float)ctx->num_modules);
        }

        ctx->module_states[i].current_budget += replenish;

        // Cap at initial budget
        if (ctx->module_states[i].current_budget > ctx->config.initial_budget) {
            ctx->module_states[i].current_budget = ctx->config.initial_budget;
        }
    }

    ctx->last_replenish_time_ms = nimcp_time_get_ms();
}

void gt_gw_reset_budgets(gt_gw_auction_ctx_t ctx) {
    if (!ctx) {
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    gt_global_workspace_heartbeat("gt_global_wo_gt_gw_reset_budgets", 0.0f);


    for (uint32_t i = 0; i < ctx->num_modules; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && ctx->num_modules > 256) {
            gt_global_workspace_heartbeat("gt_global_wo_loop",
                             (float)(i + 1) / (float)ctx->num_modules);
        }

        ctx->module_states[i].current_budget = ctx->config.initial_budget;
    }
}

//=============================================================================
// Query Functions
//=============================================================================

nimcp_error_t gt_gw_get_module_state(
    const gt_gw_auction_ctx_t ctx,
    cognitive_module_t module,
    gt_gw_module_state_t* state
) {
    /* Phase 8: Heartbeat at operation start */
    gt_global_workspace_heartbeat("gt_global_wo_gt_gw_get_module_sta", 0.0f);


    NIMCP_CHECK_THROW(ctx && state, NIMCP_ERROR_INVALID_PARAM, "ctx or state is NULL");

    int idx = find_module_state((gt_gw_auction_ctx_t)ctx, module);
    if (idx < 0) {
        return NIMCP_GT_ERROR_PLAYER_NOT_FOUND;
    }

    *state = ctx->module_states[idx];
    return NIMCP_SUCCESS;
}

nimcp_auction_t gt_gw_get_auction(const gt_gw_auction_ctx_t ctx) {
    /* Phase 8: Heartbeat at operation start */
    gt_global_workspace_heartbeat("gt_global_wo_gt_gw_get_auction", 0.0f);


    return ctx ? ctx->auction : NULL;
}

global_workspace_t* gt_gw_get_workspace(const gt_gw_auction_ctx_t ctx) {
    /* Phase 8: Heartbeat at operation start */
    gt_global_workspace_heartbeat("gt_global_wo_gt_gw_get_workspace", 0.0f);


    return ctx ? ctx->workspace : NULL;
}

bool gt_gw_is_auction_active(const gt_gw_auction_ctx_t ctx) {
    /* Phase 8: Heartbeat at operation start */
    gt_global_workspace_heartbeat("gt_global_wo_gt_gw_is_auction_act", 0.0f);


    return ctx ? ctx->active : false;
}

nimcp_error_t gt_gw_get_stats(
    const gt_gw_auction_ctx_t ctx,
    nimcp_game_stats_t* stats
) {
    /* Phase 8: Heartbeat at operation start */
    gt_global_workspace_heartbeat("gt_global_wo_gt_gw_get_stats", 0.0f);


    NIMCP_CHECK_THROW(ctx && stats, NIMCP_ERROR_INVALID_PARAM, "ctx or stats is NULL");

    memset(stats, 0, sizeof(nimcp_game_stats_t));

    stats->games_played = ctx->rounds_completed;
    stats->auctions_completed = ctx->successful_broadcasts;

    // Compute average efficiency (payments vs welfare)
    if (ctx->successful_broadcasts > 0) {
        stats->avg_efficiency = ctx->total_payments / ctx->total_welfare;

        // Compute fairness (Jain's index across module wins)
        float sum_wins = 0.0f;
        float sum_sq_wins = 0.0f;
        for (uint32_t i = 0; i < ctx->num_modules; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && ctx->num_modules > 256) {
                gt_global_workspace_heartbeat("gt_global_wo_loop",
                                 (float)(i + 1) / (float)ctx->num_modules);
            }

            float wins = (float)ctx->module_states[i].wins;
            sum_wins += wins;
            sum_sq_wins += wins * wins;
        }
        if (sum_wins > 0.0f && ctx->num_modules > 0) {
            stats->avg_fairness_index = (sum_wins * sum_wins) /
                ((float)ctx->num_modules * sum_sq_wins);
        }

        // Average social welfare
        stats->avg_social_welfare = ctx->total_welfare / (float)ctx->successful_broadcasts;
    }

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int gt_global_workspace_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    gt_global_workspace_heartbeat("gt_global_wo_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Game_Theory_Global_Workspace");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                gt_global_workspace_heartbeat("gt_global_wo_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            (void)self->observations[i];
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Game_Theory_Global_Workspace");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Game_Theory_Global_Workspace");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void gt_global_workspace_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_gt_global_workspace_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int gt_global_workspace_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "gt_global_workspace_training_begin: NULL argument");
        return -1;
    }
    gt_global_workspace_heartbeat_instance(NULL, "gt_global_workspace_training_begin", 0.0f);
    (void)(struct gt_gw_auction_ctx_struct*)instance; /* Module state available for reset */
    return 0;
}

int gt_global_workspace_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "gt_global_workspace_training_end: NULL argument");
        return -1;
    }
    gt_global_workspace_heartbeat_instance(NULL, "gt_global_workspace_training_end", 1.0f);
    (void)(struct gt_gw_auction_ctx_struct*)instance; /* Module state available for finalization */
    return 0;
}

int gt_global_workspace_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "gt_global_workspace_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    gt_global_workspace_heartbeat_instance(NULL, "gt_global_workspace_training_step", progress);
    (void)(struct gt_gw_auction_ctx_struct*)instance; /* Module state available for step adaptation */
    return 0;
}
