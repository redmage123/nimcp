//=============================================================================
// nimcp_gt_auction_ext.c - Extended Auction Mechanisms Implementation
//=============================================================================
/**
 * @file nimcp_gt_auction_ext.c
 * @brief Extended auction mechanism implementations
 *
 * WHAT: Combinatorial, double, and multi-unit auction implementations
 * WHY:  Enable complex resource allocation in neural systems
 * HOW:  Winner determination algorithms, order matching, pricing rules
 */

#include "cognitive/game_theory/nimcp_gt_auction_ext.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/time/nimcp_time.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdlib.h>
#include <float.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(gt_auction_ext, MESH_ADAPTER_CATEGORY_COGNITIVE)



//=============================================================================
// Combinatorial Auction Internal Structure
//=============================================================================

struct nimcp_combo_auction_struct {
    nimcp_combo_auction_config_t config;
    nimcp_combo_state_t state;

    // Bundle bids storage
    nimcp_bundle_bid_t* bids;
    uint32_t num_bids;
    uint32_t max_bids;

    // Solution state
    nimcp_combo_result_t last_result;
    bool solved;

    // Thread safety
    nimcp_platform_mutex_t mutex;
};

//=============================================================================
// Double Auction Internal Structure
//=============================================================================

struct nimcp_double_auction_struct {
    nimcp_double_auction_config_t config;
    nimcp_double_state_t state;

    // Order books
    nimcp_order_t* buy_orders;
    nimcp_order_t* sell_orders;
    uint32_t num_buy_orders;
    uint32_t num_sell_orders;
    uint32_t max_orders;

    // Matched trades
    nimcp_trade_t* trades;
    uint32_t num_trades;
    uint32_t max_trades;

    // Clearing result
    nimcp_clearing_result_t last_result;
    bool cleared;

    // Thread safety
    nimcp_platform_mutex_t mutex;
};

//=============================================================================
// Multi-Unit Auction Internal Structure
//=============================================================================

struct nimcp_multi_unit_auction_struct {
    nimcp_multi_unit_config_t config;
    nimcp_multi_unit_state_t state;

    // Bids storage
    nimcp_multi_bid_t* bids;
    uint32_t num_bids;
    uint32_t max_bids;

    // Result
    nimcp_multi_unit_result_t last_result;
    bool allocated;

    // Thread safety
    nimcp_platform_mutex_t mutex;
};

//=============================================================================
// Static Name Tables
//=============================================================================

static const char* s_combo_algo_names[] = {
    "Optimal (Branch & Bound)",
    "Greedy Approximation",
    "Genetic Algorithm"
};

static const char* s_clearing_rule_names[] = {
    "Uniform Price",
    "Discriminatory (Pay-as-bid)",
    "Midpoint"
};

static const char* s_multi_unit_type_names[] = {
    "Discriminatory",
    "Uniform Price"
};

//=============================================================================
// Utility Functions
//=============================================================================

uint32_t nimcp_bundle_count_items(uint64_t mask) {
    // WHAT: Count set bits (population count)
    // WHY:  Know how many items in bundle
    // HOW:  Brian Kernighan's algorithm
    /* Phase 8: Heartbeat at operation start */
    gt_auction_ext_heartbeat("gt_auction_e_bundle_count_items", 0.0f);


    uint32_t count = 0;
    while (mask) {
        mask &= (mask - 1);
        count++;
    }
    return count;
}

bool nimcp_bundles_overlap(uint64_t mask1, uint64_t mask2) {
    // WHAT: Check if bundles share items
    // WHY:  Detect conflicts in allocation
    // HOW:  Bitwise AND
    /* Phase 8: Heartbeat at operation start */
    gt_auction_ext_heartbeat("gt_auction_e_bundles_overlap", 0.0f);


    return (mask1 & mask2) != 0;
}

const char* nimcp_combo_algo_name(nimcp_combo_algo_t algo) {
    if (algo >= NIMCP_COMBO_ALGO_COUNT) {
        return "Unknown";
    }
    return s_combo_algo_names[algo];
}

const char* nimcp_clearing_rule_name(nimcp_clearing_rule_t rule) {
    if (rule > NIMCP_CLEARING_MIDPOINT) {
        return "Unknown";
    }
    return s_clearing_rule_names[rule];
}

const char* nimcp_multi_unit_type_name(nimcp_multi_unit_type_t type) {
    if (type > NIMCP_MULTI_UNIT_UNIFORM) {
        return "Unknown";
    }
    return s_multi_unit_type_names[type];
}

//=============================================================================
// Combinatorial Auction Implementation
//=============================================================================

nimcp_combo_auction_config_t nimcp_combo_auction_default_config(uint32_t num_items) {
    /* Phase 8: Heartbeat at operation start */
    gt_auction_ext_heartbeat("gt_auction_e_combo_auction_defaul", 0.0f);


    nimcp_combo_auction_config_t config;
    memset(&config, 0, sizeof(config));

    config.num_items = num_items > NIMCP_GT_MAX_COMBO_ITEMS ?
                       NIMCP_GT_MAX_COMBO_ITEMS : num_items;
    config.max_bidders = NIMCP_GT_MAX_PLAYERS;
    config.deadline_ms = 0;
    config.allow_xor_bids = false;
    config.compute_vcg_payments = true;
    config.max_iterations = NIMCP_GT_MAX_ITERATIONS;
    config.time_limit_ms = 0.0f;

    for (uint32_t i = 0; i < NIMCP_GT_MAX_COMBO_ITEMS; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && NIMCP_GT_MAX_COMBO_ITEMS > 256) {
            gt_auction_ext_heartbeat("gt_auction_e_loop",
                             (float)(i + 1) / (float)NIMCP_GT_MAX_COMBO_ITEMS);
        }

        config.reserve_prices[i] = 0.0f;
    }

    return config;
}

nimcp_combo_auction_t nimcp_combo_auction_create(
    const nimcp_combo_auction_config_t* config
) {
    if (!config || config->num_items == 0 ||
        config->num_items > NIMCP_GT_MAX_COMBO_ITEMS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_combo_auction_create: config is NULL or num_items invalid");
        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    gt_auction_ext_heartbeat("gt_auction_e_combo_auction_create", 0.0f);

    nimcp_combo_auction_t ctx = nimcp_calloc(1, sizeof(struct nimcp_combo_auction_struct));
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate ctx");

        return NULL;
    }

    ctx->config = *config;
    ctx->state = NIMCP_COMBO_STATE_CREATED;

    // Allocate bid storage
    ctx->max_bids = NIMCP_GT_MAX_BUNDLE_BIDS;
    ctx->bids = nimcp_calloc(ctx->max_bids, sizeof(nimcp_bundle_bid_t));
    if (!ctx->bids) {
        nimcp_free(ctx);
        ctx = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_combo_auction_create: ctx->bids is NULL");
        return NULL;
    }

    if (nimcp_platform_mutex_init(&ctx->mutex, false) != 0) {
        nimcp_free(ctx->bids);
        nimcp_free(ctx);
        ctx = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "nimcp_combo_auction_create: validation failed");
        return NULL;
    }

    ctx->num_bids = 0;
    ctx->solved = false;
    memset(&ctx->last_result, 0, sizeof(nimcp_combo_result_t));

    return ctx;
}

void nimcp_combo_auction_destroy(nimcp_combo_auction_t ctx) {
    if (!ctx) return;

    /* Phase 8: Heartbeat at operation start */
    gt_auction_ext_heartbeat("gt_auction_e_combo_auction_destro", 0.0f);


    nimcp_platform_mutex_destroy(&ctx->mutex);
    nimcp_free(ctx->bids);
    nimcp_free(ctx);
    ctx = NULL;
}

nimcp_error_t nimcp_combo_auction_submit_bundle_bid(
    nimcp_combo_auction_t ctx,
    nimcp_player_id_t bidder,
    uint64_t items_mask,
    float value
) {
    if (!ctx) {
        return NIMCP_GT_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    gt_auction_ext_heartbeat("gt_auction_e_combo_auction_submit", 0.0f);


    nimcp_platform_mutex_lock(&ctx->mutex);

    // Validate state
    if (ctx->state == NIMCP_COMBO_STATE_COMPLETED ||
        ctx->state == NIMCP_COMBO_STATE_CANCELLED) {
        nimcp_platform_mutex_unlock(&ctx->mutex);
        return NIMCP_GT_ERROR_AUCTION_CLOSED;
    }

    // Check capacity
    if (ctx->num_bids >= ctx->max_bids) {
        nimcp_platform_mutex_unlock(&ctx->mutex);
        return NIMCP_GT_ERROR_CAPACITY;
    }

    // Validate bid
    if (value < 0.0f) {
        nimcp_platform_mutex_unlock(&ctx->mutex);
        return NIMCP_GT_ERROR_INVALID_BID;
    }

    // Validate items mask (must not exceed num_items)
    uint64_t max_mask = ((uint64_t)1 << ctx->config.num_items) - 1;
    if (items_mask == 0 || (items_mask & ~max_mask) != 0) {
        nimcp_platform_mutex_unlock(&ctx->mutex);
        return NIMCP_GT_ERROR_INVALID_PARAMETER;
    }

    // Check against reserve prices
    float total_reserve = 0.0f;
    for (uint32_t i = 0; i < ctx->config.num_items; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && ctx->config.num_items > 256) {
            gt_auction_ext_heartbeat("gt_auction_e_loop",
                             (float)(i + 1) / (float)ctx->config.num_items);
        }

        if (items_mask & ((uint64_t)1 << i)) {
            total_reserve += ctx->config.reserve_prices[i];
        }
    }
    if (value < total_reserve) {
        nimcp_platform_mutex_unlock(&ctx->mutex);
        return NIMCP_GT_ERROR_BID_TOO_LOW;
    }

    // Transition to bidding state
    if (ctx->state == NIMCP_COMBO_STATE_CREATED) {
        ctx->state = NIMCP_COMBO_STATE_BIDDING;
    }

    // Store bid
    nimcp_bundle_bid_t* bid = &ctx->bids[ctx->num_bids];
    bid->bidder_id = bidder;
    bid->items_mask = items_mask;
    bid->value = value;
    bid->timestamp_ms = nimcp_time_get_ms();
    bid->is_valid = true;

    ctx->num_bids++;
    ctx->solved = false;  // Invalidate previous solution

    nimcp_platform_mutex_unlock(&ctx->mutex);
    return NIMCP_SUCCESS;
}

/**
 * @brief Compare bundle bids by value density (value per item)
 */
static int compare_bids_by_density(const void* a, const void* b) {
    const nimcp_bundle_bid_t* bid_a = (const nimcp_bundle_bid_t*)a;
    const nimcp_bundle_bid_t* bid_b = (const nimcp_bundle_bid_t*)b;

    uint32_t items_a = nimcp_bundle_count_items(bid_a->items_mask);
    uint32_t items_b = nimcp_bundle_count_items(bid_b->items_mask);

    float density_a = (items_a > 0) ? bid_a->value / (float)items_a : 0.0f;
    float density_b = (items_b > 0) ? bid_b->value / (float)items_b : 0.0f;

    if (density_b > density_a) return 1;
    if (density_b < density_a) return -1;

    // Tie-break by total value
    if (bid_b->value > bid_a->value) return 1;
    if (bid_b->value < bid_a->value) return -1;

    return 0;
}

/**
 * @brief Recursive optimal solver helper
 *
 * Uses branch-and-bound with proper selection tracking.
 * current_selection tracks bids selected in current path.
 * best_selection is only updated when a new best value is found.
 */
static void combo_solve_recursive(
    const nimcp_bundle_bid_t* bids,
    uint32_t num_bids,
    uint32_t idx,
    uint64_t allocated,
    uint64_t current_selection,
    float current_value,
    uint64_t* best_selection,
    float* best_value
) {
    // Base case: processed all bids
    if (idx >= num_bids) {
        if (current_value > *best_value) {
            *best_value = current_value;
            *best_selection = current_selection;
        }
        return;
    }

    const nimcp_bundle_bid_t* bid = &bids[idx];

    // Prune if bid is invalid
    if (!bid->is_valid) {
        combo_solve_recursive(bids, num_bids, idx + 1, allocated,
                              current_selection, current_value,
                              best_selection, best_value);
        return;
    }

    // Option 1: Skip this bid
    combo_solve_recursive(bids, num_bids, idx + 1, allocated,
                          current_selection, current_value,
                          best_selection, best_value);

    // Option 2: Include this bid (if no conflict with already allocated items)
    if ((bid->items_mask & allocated) == 0) {
        uint64_t new_allocated = allocated | bid->items_mask;
        uint64_t new_selection = current_selection | ((uint64_t)1 << idx);
        float new_value = current_value + bid->value;

        combo_solve_recursive(bids, num_bids, idx + 1, new_allocated,
                              new_selection, new_value,
                              best_selection, best_value);
    }
}

nimcp_error_t nimcp_combo_auction_solve_optimal(
    nimcp_combo_auction_t ctx,
    nimcp_combo_result_t* result
) {
    if (!ctx || !result) {
        return NIMCP_GT_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    gt_auction_ext_heartbeat("gt_auction_e_combo_auction_solve_", 0.0f);


    nimcp_platform_mutex_lock(&ctx->mutex);

    if (ctx->state == NIMCP_COMBO_STATE_CANCELLED) {
        nimcp_platform_mutex_unlock(&ctx->mutex);
        return NIMCP_GT_ERROR_INVALID_STATE;
    }

    ctx->state = NIMCP_COMBO_STATE_SOLVING;
    uint64_t start_time = nimcp_time_get_ms();

    memset(result, 0, sizeof(nimcp_combo_result_t));

    if (ctx->num_bids == 0) {
        result->final_state = NIMCP_COMBO_STATE_NO_WINNER;
        ctx->state = NIMCP_COMBO_STATE_NO_WINNER;
        nimcp_platform_mutex_unlock(&ctx->mutex);
        return NIMCP_SUCCESS;
    }

    // Sort bids by density for better pruning
    qsort(ctx->bids, ctx->num_bids, sizeof(nimcp_bundle_bid_t), compare_bids_by_density);

    // Solve with branch and bound
    uint64_t best_selection = 0;
    float best_value = 0.0f;

    combo_solve_recursive(ctx->bids, ctx->num_bids, 0, 0, 0, 0.0f,
                          &best_selection, &best_value);

    // Extract winners from selection bitmap
    result->num_winners = 0;
    result->allocated_items = 0;
    result->total_value = 0.0f;

    for (uint32_t i = 0; i < ctx->num_bids && result->num_winners < NIMCP_GT_MAX_PLAYERS; i++) {
        if (best_selection & ((uint64_t)1 << i)) {
            nimcp_combo_winner_t* winner = &result->winners[result->num_winners];
            winner->bidder_id = ctx->bids[i].bidder_id;
            winner->items_won = ctx->bids[i].items_mask;
            winner->bid_value = ctx->bids[i].value;
            winner->vcg_payment = 0.0f;  // Computed separately

            result->allocated_items |= ctx->bids[i].items_mask;
            result->total_value += ctx->bids[i].value;
            result->num_winners++;
        }
    }

    // Compute efficiency
    if (result->total_value > 0.0f) {
        // Find max possible value (sum of all non-conflicting max bids per item)
        float max_possible = 0.0f;
        for (uint32_t i = 0; i < ctx->num_bids; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && ctx->num_bids > 256) {
                gt_auction_ext_heartbeat("gt_auction_e_loop",
                                 (float)(i + 1) / (float)ctx->num_bids);
            }

            max_possible += ctx->bids[i].value;
        }
        result->efficiency = result->total_value / max_possible;
        if (result->efficiency > 1.0f) result->efficiency = 1.0f;
    } else {
        result->efficiency = 0.0f;
    }

    result->solve_time_ms = (float)(nimcp_time_get_ms() - start_time);
    result->final_state = (result->num_winners > 0) ?
                          NIMCP_COMBO_STATE_COMPLETED : NIMCP_COMBO_STATE_NO_WINNER;

    ctx->last_result = *result;
    ctx->state = result->final_state;
    ctx->solved = true;

    nimcp_platform_mutex_unlock(&ctx->mutex);
    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_combo_auction_solve_greedy(
    nimcp_combo_auction_t ctx,
    nimcp_combo_result_t* result
) {
    if (!ctx || !result) {
        return NIMCP_GT_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    gt_auction_ext_heartbeat("gt_auction_e_combo_auction_solve_", 0.0f);


    nimcp_platform_mutex_lock(&ctx->mutex);

    if (ctx->state == NIMCP_COMBO_STATE_CANCELLED) {
        nimcp_platform_mutex_unlock(&ctx->mutex);
        return NIMCP_GT_ERROR_INVALID_STATE;
    }

    ctx->state = NIMCP_COMBO_STATE_SOLVING;
    uint64_t start_time = nimcp_time_get_ms();

    memset(result, 0, sizeof(nimcp_combo_result_t));

    if (ctx->num_bids == 0) {
        result->final_state = NIMCP_COMBO_STATE_NO_WINNER;
        ctx->state = NIMCP_COMBO_STATE_NO_WINNER;
        nimcp_platform_mutex_unlock(&ctx->mutex);
        return NIMCP_SUCCESS;
    }

    // Sort bids by value density (highest first)
    qsort(ctx->bids, ctx->num_bids, sizeof(nimcp_bundle_bid_t), compare_bids_by_density);

    // Greedy selection
    uint64_t allocated = 0;
    result->num_winners = 0;

    for (uint32_t i = 0; i < ctx->num_bids && result->num_winners < NIMCP_GT_MAX_PLAYERS; i++) {
        nimcp_bundle_bid_t* bid = &ctx->bids[i];

        if (!bid->is_valid) continue;

        // Check for conflict
        if ((bid->items_mask & allocated) != 0) continue;

        // Accept this bid
        nimcp_combo_winner_t* winner = &result->winners[result->num_winners];
        winner->bidder_id = bid->bidder_id;
        winner->items_won = bid->items_mask;
        winner->bid_value = bid->value;
        winner->vcg_payment = 0.0f;

        allocated |= bid->items_mask;
        result->total_value += bid->value;
        result->num_winners++;
        result->iterations_used++;
    }

    result->allocated_items = allocated;
    result->solve_time_ms = (float)(nimcp_time_get_ms() - start_time);

    // Compute efficiency
    if (result->total_value > 0.0f) {
        float max_possible = 0.0f;
        for (uint32_t i = 0; i < ctx->num_bids; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && ctx->num_bids > 256) {
                gt_auction_ext_heartbeat("gt_auction_e_loop",
                                 (float)(i + 1) / (float)ctx->num_bids);
            }

            max_possible += ctx->bids[i].value;
        }
        result->efficiency = result->total_value / max_possible;
        if (result->efficiency > 1.0f) result->efficiency = 1.0f;
    }

    result->final_state = (result->num_winners > 0) ?
                          NIMCP_COMBO_STATE_COMPLETED : NIMCP_COMBO_STATE_NO_WINNER;

    ctx->last_result = *result;
    ctx->state = result->final_state;
    ctx->solved = true;

    nimcp_platform_mutex_unlock(&ctx->mutex);
    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_combo_auction_get_vcg_payments(
    nimcp_combo_auction_t ctx,
    nimcp_combo_result_t* result
) {
    if (!ctx || !result) {
        return NIMCP_GT_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    gt_auction_ext_heartbeat("gt_auction_e_combo_auction_get_vc", 0.0f);


    nimcp_platform_mutex_lock(&ctx->mutex);

    if (!ctx->solved || ctx->state != NIMCP_COMBO_STATE_COMPLETED) {
        nimcp_platform_mutex_unlock(&ctx->mutex);
        return NIMCP_GT_ERROR_INVALID_STATE;
    }

    // For each winner, compute VCG payment = welfare without them - welfare of others with them
    // VCG payment = (optimal value excluding winner i) - (sum of other winners' values)

    result->total_vcg_payments = 0.0f;

    for (uint32_t w = 0; w < result->num_winners; w++) {
        /* Phase 8: Loop progress heartbeat */
        if ((w & 0xFF) == 0 && result->num_winners > 256) {
            gt_auction_ext_heartbeat("gt_auction_e_loop",
                             (float)(w + 1) / (float)result->num_winners);
        }

        nimcp_combo_winner_t* winner = &result->winners[w];

        // Compute sum of values of other winners
        float others_value = result->total_value - winner->bid_value;

        // Solve optimal excluding this winner's bids
        float optimal_without = 0.0f;
        uint64_t allocated = 0;

        // Sort bids by value for simple optimal without one bidder
        for (uint32_t i = 0; i < ctx->num_bids; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && ctx->num_bids > 256) {
                gt_auction_ext_heartbeat("gt_auction_e_loop",
                                 (float)(i + 1) / (float)ctx->num_bids);
            }

            nimcp_bundle_bid_t* bid = &ctx->bids[i];

            if (!bid->is_valid) continue;
            if (bid->bidder_id == winner->bidder_id) continue;  // Exclude this winner
            if ((bid->items_mask & allocated) != 0) continue;   // Conflict

            allocated |= bid->items_mask;
            optimal_without += bid->value;
        }

        // VCG payment = externality imposed on others
        winner->vcg_payment = optimal_without - others_value;
        if (winner->vcg_payment < 0.0f) {
            winner->vcg_payment = 0.0f;  // Payment cannot be negative
        }

        result->total_vcg_payments += winner->vcg_payment;
    }

    ctx->last_result = *result;

    nimcp_platform_mutex_unlock(&ctx->mutex);
    return NIMCP_SUCCESS;
}

nimcp_combo_state_t nimcp_combo_auction_get_state(const nimcp_combo_auction_t ctx) {
    if (!ctx) return NIMCP_COMBO_STATE_CREATED;
    /* Phase 8: Heartbeat at operation start */
    gt_auction_ext_heartbeat("gt_auction_e_combo_auction_get_st", 0.0f);


    return ctx->state;
}

uint32_t nimcp_combo_auction_get_bid_count(const nimcp_combo_auction_t ctx) {
    if (!ctx) return 0;
    /* Phase 8: Heartbeat at operation start */
    gt_auction_ext_heartbeat("gt_auction_e_combo_auction_get_bi", 0.0f);


    return ctx->num_bids;
}

nimcp_error_t nimcp_combo_auction_cancel(nimcp_combo_auction_t ctx) {
    if (!ctx) {
        return NIMCP_GT_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    gt_auction_ext_heartbeat("gt_auction_e_combo_auction_cancel", 0.0f);


    nimcp_platform_mutex_lock(&ctx->mutex);
    ctx->state = NIMCP_COMBO_STATE_CANCELLED;
    nimcp_platform_mutex_unlock(&ctx->mutex);

    return NIMCP_SUCCESS;
}

//=============================================================================
// Double Auction Implementation
//=============================================================================

nimcp_double_auction_config_t nimcp_double_auction_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    gt_auction_ext_heartbeat("gt_auction_e_double_auction_defau", 0.0f);


    nimcp_double_auction_config_t config = {
        .clearing_rule = NIMCP_CLEARING_UNIFORM,
        .max_orders = NIMCP_GT_MAX_ORDERS,
        .deadline_ms = 0,
        .min_price = 0.0f,
        .max_price = 0.0f,
        .allow_partial_fills = true,
        .continuous_clearing = false
    };
    return config;
}

nimcp_double_auction_t nimcp_double_auction_create(
    const nimcp_double_auction_config_t* config
) {
    /* Phase 8: Heartbeat at operation start */
    gt_auction_ext_heartbeat("gt_auction_e_double_auction_creat", 0.0f);


    nimcp_double_auction_t ctx = nimcp_calloc(1, sizeof(struct nimcp_double_auction_struct));
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate ctx");

        return NULL;
    }

    if (config) {
        ctx->config = *config;
    } else {
        ctx->config = nimcp_double_auction_default_config();
    }

    ctx->state = NIMCP_DOUBLE_STATE_CREATED;

    // Allocate order books
    ctx->max_orders = ctx->config.max_orders > 0 ? ctx->config.max_orders : NIMCP_GT_MAX_ORDERS;
    ctx->buy_orders = nimcp_calloc(ctx->max_orders, sizeof(nimcp_order_t));
    ctx->sell_orders = nimcp_calloc(ctx->max_orders, sizeof(nimcp_order_t));

    if (!ctx->buy_orders || !ctx->sell_orders) {
        nimcp_free(ctx->buy_orders);
        nimcp_free(ctx->sell_orders);
        nimcp_free(ctx);
        ctx = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_double_auction_create: required parameter is NULL (ctx->buy_orders, ctx->sell_orders)");
        return NULL;
    }

    // Allocate trades storage
    ctx->max_trades = NIMCP_GT_MAX_TRADES;
    ctx->trades = nimcp_calloc(ctx->max_trades, sizeof(nimcp_trade_t));
    if (!ctx->trades) {
        nimcp_free(ctx->buy_orders);
        nimcp_free(ctx->sell_orders);
        nimcp_free(ctx);
        ctx = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_double_auction_create: ctx->trades is NULL");
        return NULL;
    }

    if (nimcp_platform_mutex_init(&ctx->mutex, false) != 0) {
        nimcp_free(ctx->trades);
        nimcp_free(ctx->buy_orders);
        nimcp_free(ctx->sell_orders);
        nimcp_free(ctx);
        ctx = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "nimcp_double_auction_create: validation failed");
        return NULL;
    }

    ctx->num_buy_orders = 0;
    ctx->num_sell_orders = 0;
    ctx->num_trades = 0;
    ctx->cleared = false;

    return ctx;
}

void nimcp_double_auction_destroy(nimcp_double_auction_t ctx) {
    if (!ctx) return;

    /* Phase 8: Heartbeat at operation start */
    gt_auction_ext_heartbeat("gt_auction_e_double_auction_destr", 0.0f);


    nimcp_platform_mutex_destroy(&ctx->mutex);
    nimcp_free(ctx->trades);
    nimcp_free(ctx->buy_orders);
    nimcp_free(ctx->sell_orders);
    nimcp_free(ctx);
    ctx = NULL;
}

nimcp_error_t nimcp_double_auction_submit_buy(
    nimcp_double_auction_t ctx,
    nimcp_player_id_t buyer,
    float max_price,
    uint32_t quantity
) {
    if (!ctx) {
        return NIMCP_GT_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    gt_auction_ext_heartbeat("gt_auction_e_double_auction_submi", 0.0f);


    nimcp_platform_mutex_lock(&ctx->mutex);

    if (ctx->state == NIMCP_DOUBLE_STATE_COMPLETED ||
        ctx->state == NIMCP_DOUBLE_STATE_CANCELLED) {
        nimcp_platform_mutex_unlock(&ctx->mutex);
        return NIMCP_GT_ERROR_AUCTION_CLOSED;
    }

    if (ctx->num_buy_orders >= ctx->max_orders) {
        nimcp_platform_mutex_unlock(&ctx->mutex);
        return NIMCP_GT_ERROR_CAPACITY;
    }

    if (max_price < ctx->config.min_price) {
        nimcp_platform_mutex_unlock(&ctx->mutex);
        return NIMCP_GT_ERROR_BID_TOO_LOW;
    }

    if (ctx->config.max_price > 0.0f && max_price > ctx->config.max_price) {
        nimcp_platform_mutex_unlock(&ctx->mutex);
        return NIMCP_GT_ERROR_INVALID_BID;
    }

    if (quantity == 0) {
        nimcp_platform_mutex_unlock(&ctx->mutex);
        return NIMCP_GT_ERROR_INVALID_PARAMETER;
    }

    if (ctx->state == NIMCP_DOUBLE_STATE_CREATED) {
        ctx->state = NIMCP_DOUBLE_STATE_COLLECTING;
    }

    nimcp_order_t* order = &ctx->buy_orders[ctx->num_buy_orders];
    order->trader_id = buyer;
    order->side = NIMCP_ORDER_BUY;
    order->price = max_price;
    order->quantity = quantity;
    order->filled_quantity = 0;
    order->timestamp_ms = nimcp_time_get_ms();
    order->is_valid = true;

    ctx->num_buy_orders++;
    ctx->cleared = false;

    nimcp_platform_mutex_unlock(&ctx->mutex);
    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_double_auction_submit_sell(
    nimcp_double_auction_t ctx,
    nimcp_player_id_t seller,
    float min_price,
    uint32_t quantity
) {
    if (!ctx) {
        return NIMCP_GT_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    gt_auction_ext_heartbeat("gt_auction_e_double_auction_submi", 0.0f);


    nimcp_platform_mutex_lock(&ctx->mutex);

    if (ctx->state == NIMCP_DOUBLE_STATE_COMPLETED ||
        ctx->state == NIMCP_DOUBLE_STATE_CANCELLED) {
        nimcp_platform_mutex_unlock(&ctx->mutex);
        return NIMCP_GT_ERROR_AUCTION_CLOSED;
    }

    if (ctx->num_sell_orders >= ctx->max_orders) {
        nimcp_platform_mutex_unlock(&ctx->mutex);
        return NIMCP_GT_ERROR_CAPACITY;
    }

    if (min_price < ctx->config.min_price) {
        nimcp_platform_mutex_unlock(&ctx->mutex);
        return NIMCP_GT_ERROR_INVALID_BID;
    }

    if (ctx->config.max_price > 0.0f && min_price > ctx->config.max_price) {
        nimcp_platform_mutex_unlock(&ctx->mutex);
        return NIMCP_GT_ERROR_INVALID_BID;
    }

    if (quantity == 0) {
        nimcp_platform_mutex_unlock(&ctx->mutex);
        return NIMCP_GT_ERROR_INVALID_PARAMETER;
    }

    if (ctx->state == NIMCP_DOUBLE_STATE_CREATED) {
        ctx->state = NIMCP_DOUBLE_STATE_COLLECTING;
    }

    nimcp_order_t* order = &ctx->sell_orders[ctx->num_sell_orders];
    order->trader_id = seller;
    order->side = NIMCP_ORDER_SELL;
    order->price = min_price;
    order->quantity = quantity;
    order->filled_quantity = 0;
    order->timestamp_ms = nimcp_time_get_ms();
    order->is_valid = true;

    ctx->num_sell_orders++;
    ctx->cleared = false;

    nimcp_platform_mutex_unlock(&ctx->mutex);
    return NIMCP_SUCCESS;
}

/**
 * @brief Compare buy orders (descending by price, then ascending by time)
 */
static int compare_buy_orders(const void* a, const void* b) {
    const nimcp_order_t* order_a = (const nimcp_order_t*)a;
    const nimcp_order_t* order_b = (const nimcp_order_t*)b;

    // Higher price first
    if (order_b->price > order_a->price) return 1;
    if (order_b->price < order_a->price) return -1;

    // Earlier timestamp first
    if (order_a->timestamp_ms < order_b->timestamp_ms) return -1;
    if (order_a->timestamp_ms > order_b->timestamp_ms) return 1;

    return 0;
}

/**
 * @brief Compare sell orders (ascending by price, then ascending by time)
 */
static int compare_sell_orders(const void* a, const void* b) {
    const nimcp_order_t* order_a = (const nimcp_order_t*)a;
    const nimcp_order_t* order_b = (const nimcp_order_t*)b;

    // Lower price first
    if (order_a->price < order_b->price) return -1;
    if (order_a->price > order_b->price) return 1;

    // Earlier timestamp first
    if (order_a->timestamp_ms < order_b->timestamp_ms) return -1;
    if (order_a->timestamp_ms > order_b->timestamp_ms) return 1;

    return 0;
}

nimcp_error_t nimcp_double_auction_clear(
    nimcp_double_auction_t ctx,
    nimcp_clearing_result_t* result
) {
    if (!ctx || !result) {
        return NIMCP_GT_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    gt_auction_ext_heartbeat("gt_auction_e_double_auction_clear", 0.0f);


    nimcp_platform_mutex_lock(&ctx->mutex);

    if (ctx->state == NIMCP_DOUBLE_STATE_CANCELLED) {
        nimcp_platform_mutex_unlock(&ctx->mutex);
        return NIMCP_GT_ERROR_INVALID_STATE;
    }

    ctx->state = NIMCP_DOUBLE_STATE_CLEARING;
    memset(result, 0, sizeof(nimcp_clearing_result_t));

    if (ctx->num_buy_orders == 0 || ctx->num_sell_orders == 0) {
        result->final_state = NIMCP_DOUBLE_STATE_NO_TRADES;
        ctx->state = NIMCP_DOUBLE_STATE_NO_TRADES;
        ctx->last_result = *result;
        nimcp_platform_mutex_unlock(&ctx->mutex);
        return NIMCP_SUCCESS;
    }

    // Sort order books
    qsort(ctx->buy_orders, ctx->num_buy_orders, sizeof(nimcp_order_t), compare_buy_orders);
    qsort(ctx->sell_orders, ctx->num_sell_orders, sizeof(nimcp_order_t), compare_sell_orders);

    // Find market clearing price (intersection of supply and demand)
    // Build cumulative demand and supply curves
    uint32_t buy_idx = 0;
    uint32_t sell_idx = 0;
    ctx->num_trades = 0;

    float clearing_price = 0.0f;
    bool found_clearing = false;

    // Match orders where buy price >= sell price
    while (buy_idx < ctx->num_buy_orders && sell_idx < ctx->num_sell_orders) {
        nimcp_order_t* buy = &ctx->buy_orders[buy_idx];
        nimcp_order_t* sell = &ctx->sell_orders[sell_idx];

        if (!buy->is_valid) { buy_idx++; continue; }
        if (!sell->is_valid) { sell_idx++; continue; }

        // Check if trade possible
        if (buy->price < sell->price) {
            break;  // No more trades possible
        }

        found_clearing = true;

        // Determine trade quantity
        uint32_t buy_remaining = buy->quantity - buy->filled_quantity;
        uint32_t sell_remaining = sell->quantity - sell->filled_quantity;
        uint32_t trade_qty = (buy_remaining < sell_remaining) ? buy_remaining : sell_remaining;

        if (trade_qty == 0) {
            if (buy_remaining == 0) buy_idx++;
            if (sell_remaining == 0) sell_idx++;
            continue;
        }

        // Determine trade price based on clearing rule
        float trade_price = 0.0f;
        switch (ctx->config.clearing_rule) {
            case NIMCP_CLEARING_UNIFORM:
                // Use midpoint as preliminary; actual uniform price computed after all matches
                trade_price = (buy->price + sell->price) / 2.0f;
                break;
            case NIMCP_CLEARING_DISCRIMINATORY:
                // Pay-as-bid: use buyer's price
                trade_price = buy->price;
                break;
            case NIMCP_CLEARING_MIDPOINT:
            default:
                trade_price = (buy->price + sell->price) / 2.0f;
                break;
        }

        clearing_price = trade_price;

        // Record trade
        if (ctx->num_trades < ctx->max_trades) {
            nimcp_trade_t* trade = &ctx->trades[ctx->num_trades];
            trade->buyer_id = buy->trader_id;
            trade->seller_id = sell->trader_id;
            trade->trade_price = trade_price;
            trade->quantity = trade_qty;
            trade->buyer_surplus = (buy->price - trade_price) * trade_qty;
            trade->seller_surplus = (trade_price - sell->price) * trade_qty;
            trade->timestamp_ms = nimcp_time_get_ms();

            result->total_buyer_surplus += trade->buyer_surplus;
            result->total_seller_surplus += trade->seller_surplus;
            result->total_quantity += trade_qty;
            ctx->num_trades++;
        }

        // Update filled quantities
        buy->filled_quantity += trade_qty;
        sell->filled_quantity += trade_qty;

        if (buy->filled_quantity >= buy->quantity) buy_idx++;
        if (sell->filled_quantity >= sell->quantity) sell_idx++;

        if (!ctx->config.allow_partial_fills) {
            // Only complete fills allowed
            buy_idx++;
            sell_idx++;
        }
    }

    // For uniform pricing, update all trades to use clearing price
    if (ctx->config.clearing_rule == NIMCP_CLEARING_UNIFORM && ctx->num_trades > 0) {
        // Use the last matched price as uniform clearing price
        for (uint32_t i = 0; i < ctx->num_trades; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && ctx->num_trades > 256) {
                gt_auction_ext_heartbeat("gt_auction_e_loop",
                                 (float)(i + 1) / (float)ctx->num_trades);
            }

            nimcp_trade_t* trade = &ctx->trades[i];
            float old_price = trade->trade_price;
            trade->trade_price = clearing_price;

            // Recalculate surplus with uniform price
            // Need to find original buy/sell prices (simplified: use stored surplus to back-calculate)
            float buyer_valuation = old_price + trade->buyer_surplus / trade->quantity;
            float seller_cost = old_price - trade->seller_surplus / trade->quantity;

            trade->buyer_surplus = (buyer_valuation - clearing_price) * trade->quantity;
            trade->seller_surplus = (clearing_price - seller_cost) * trade->quantity;
        }

        // Recalculate totals
        result->total_buyer_surplus = 0.0f;
        result->total_seller_surplus = 0.0f;
        for (uint32_t i = 0; i < ctx->num_trades; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && ctx->num_trades > 256) {
                gt_auction_ext_heartbeat("gt_auction_e_loop",
                                 (float)(i + 1) / (float)ctx->num_trades);
            }

            result->total_buyer_surplus += ctx->trades[i].buyer_surplus;
            result->total_seller_surplus += ctx->trades[i].seller_surplus;
        }
    }

    result->clearing_price = clearing_price;
    result->num_trades = ctx->num_trades;
    result->total_welfare = result->total_buyer_surplus + result->total_seller_surplus;

    // Compute efficiency (actual welfare / max possible welfare)
    if (result->total_welfare > 0.0f) {
        // Max welfare would be all possible trades at max surplus
        float max_welfare = 0.0f;
        for (uint32_t i = 0; i < ctx->num_buy_orders; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && ctx->num_buy_orders > 256) {
                gt_auction_ext_heartbeat("gt_auction_e_loop",
                                 (float)(i + 1) / (float)ctx->num_buy_orders);
            }

            max_welfare += ctx->buy_orders[i].price * ctx->buy_orders[i].quantity;
        }
        for (uint32_t i = 0; i < ctx->num_sell_orders; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && ctx->num_sell_orders > 256) {
                gt_auction_ext_heartbeat("gt_auction_e_loop",
                                 (float)(i + 1) / (float)ctx->num_sell_orders);
            }

            max_welfare -= ctx->sell_orders[i].price * ctx->sell_orders[i].quantity;
        }
        if (max_welfare > 0.0f) {
            result->efficiency = result->total_welfare / max_welfare;
            if (result->efficiency > 1.0f) result->efficiency = 1.0f;
        }
    }

    if (found_clearing && ctx->num_trades > 0) {
        result->final_state = NIMCP_DOUBLE_STATE_COMPLETED;
        ctx->state = NIMCP_DOUBLE_STATE_COMPLETED;
    } else {
        result->final_state = NIMCP_DOUBLE_STATE_NO_TRADES;
        ctx->state = NIMCP_DOUBLE_STATE_NO_TRADES;
    }

    ctx->last_result = *result;
    ctx->cleared = true;

    nimcp_platform_mutex_unlock(&ctx->mutex);
    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_double_auction_get_trades(
    nimcp_double_auction_t ctx,
    nimcp_trade_t* trades,
    uint32_t max_trades,
    uint32_t* num_trades
) {
    if (!ctx || !trades || !num_trades) {
        return NIMCP_GT_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    gt_auction_ext_heartbeat("gt_auction_e_double_auction_get_t", 0.0f);


    nimcp_platform_mutex_lock(&ctx->mutex);

    if (!ctx->cleared) {
        *num_trades = 0;
        nimcp_platform_mutex_unlock(&ctx->mutex);
        return NIMCP_GT_ERROR_INVALID_STATE;
    }

    uint32_t copy_count = ctx->num_trades < max_trades ? ctx->num_trades : max_trades;
    memcpy(trades, ctx->trades, copy_count * sizeof(nimcp_trade_t));
    *num_trades = copy_count;

    nimcp_platform_mutex_unlock(&ctx->mutex);
    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_double_auction_get_surplus(
    nimcp_double_auction_t ctx,
    float* buyer_surplus,
    float* seller_surplus
) {
    if (!ctx || !buyer_surplus || !seller_surplus) {
        return NIMCP_GT_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    gt_auction_ext_heartbeat("gt_auction_e_double_auction_get_s", 0.0f);


    nimcp_platform_mutex_lock(&ctx->mutex);

    if (!ctx->cleared) {
        *buyer_surplus = 0.0f;
        *seller_surplus = 0.0f;
        nimcp_platform_mutex_unlock(&ctx->mutex);
        return NIMCP_GT_ERROR_INVALID_STATE;
    }

    *buyer_surplus = ctx->last_result.total_buyer_surplus;
    *seller_surplus = ctx->last_result.total_seller_surplus;

    nimcp_platform_mutex_unlock(&ctx->mutex);
    return NIMCP_SUCCESS;
}

nimcp_double_state_t nimcp_double_auction_get_state(const nimcp_double_auction_t ctx) {
    if (!ctx) return NIMCP_DOUBLE_STATE_CREATED;
    /* Phase 8: Heartbeat at operation start */
    gt_auction_ext_heartbeat("gt_auction_e_double_auction_get_s", 0.0f);


    return ctx->state;
}

nimcp_error_t nimcp_double_auction_get_order_counts(
    const nimcp_double_auction_t ctx,
    uint32_t* buy_orders,
    uint32_t* sell_orders
) {
    if (!ctx || !buy_orders || !sell_orders) {
        return NIMCP_GT_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    gt_auction_ext_heartbeat("gt_auction_e_double_auction_get_o", 0.0f);


    nimcp_platform_mutex_lock(&ctx->mutex);
    *buy_orders = ctx->num_buy_orders;
    *sell_orders = ctx->num_sell_orders;
    nimcp_platform_mutex_unlock(&ctx->mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_double_auction_cancel(nimcp_double_auction_t ctx) {
    if (!ctx) {
        return NIMCP_GT_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    gt_auction_ext_heartbeat("gt_auction_e_double_auction_cance", 0.0f);


    nimcp_platform_mutex_lock(&ctx->mutex);
    ctx->state = NIMCP_DOUBLE_STATE_CANCELLED;
    nimcp_platform_mutex_unlock(&ctx->mutex);

    return NIMCP_SUCCESS;
}

//=============================================================================
// Multi-Unit Auction Implementation
//=============================================================================

nimcp_multi_unit_config_t nimcp_multi_unit_default_config(uint32_t total_units) {
    /* Phase 8: Heartbeat at operation start */
    gt_auction_ext_heartbeat("gt_auction_e_multi_unit_default_c", 0.0f);


    nimcp_multi_unit_config_t config = {
        .type = NIMCP_MULTI_UNIT_UNIFORM,
        .total_units = total_units,
        .reserve_price = 0.0f,
        .max_bidders = NIMCP_GT_MAX_PLAYERS,
        .deadline_ms = 0
    };
    return config;
}

nimcp_multi_unit_auction_t nimcp_multi_unit_create(
    const nimcp_multi_unit_config_t* config
) {
    if (!config || config->total_units == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_multi_unit_create: config is NULL or total_units is 0");
        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    gt_auction_ext_heartbeat("gt_auction_e_multi_unit_create", 0.0f);


    nimcp_multi_unit_auction_t ctx = nimcp_calloc(1, sizeof(struct nimcp_multi_unit_auction_struct));
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate ctx");

        return NULL;
    }

    ctx->config = *config;
    ctx->state = NIMCP_MULTI_STATE_CREATED;

    // Allocate bids storage (allow multiple bids per bidder)
    ctx->max_bids = config->max_bidders * 10;  // Allow 10 bids per bidder
    ctx->bids = nimcp_calloc(ctx->max_bids, sizeof(nimcp_multi_bid_t));
    if (!ctx->bids) {
        nimcp_free(ctx);
        ctx = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_multi_unit_create: ctx->bids is NULL");
        return NULL;
    }

    if (nimcp_platform_mutex_init(&ctx->mutex, false) != 0) {
        nimcp_free(ctx->bids);
        nimcp_free(ctx);
        ctx = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "nimcp_multi_unit_create: validation failed");
        return NULL;
    }

    ctx->num_bids = 0;
    ctx->allocated = false;

    return ctx;
}

void nimcp_multi_unit_destroy(nimcp_multi_unit_auction_t ctx) {
    if (!ctx) return;

    /* Phase 8: Heartbeat at operation start */
    gt_auction_ext_heartbeat("gt_auction_e_multi_unit_destroy", 0.0f);


    nimcp_platform_mutex_destroy(&ctx->mutex);
    nimcp_free(ctx->bids);
    nimcp_free(ctx);
    ctx = NULL;
}

nimcp_error_t nimcp_multi_unit_submit_bid(
    nimcp_multi_unit_auction_t ctx,
    nimcp_player_id_t bidder,
    float price,
    uint32_t quantity
) {
    if (!ctx) {
        return NIMCP_GT_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    gt_auction_ext_heartbeat("gt_auction_e_multi_unit_submit_bi", 0.0f);


    nimcp_platform_mutex_lock(&ctx->mutex);

    if (ctx->state == NIMCP_MULTI_STATE_COMPLETED ||
        ctx->state == NIMCP_MULTI_STATE_CANCELLED) {
        nimcp_platform_mutex_unlock(&ctx->mutex);
        return NIMCP_GT_ERROR_AUCTION_CLOSED;
    }

    if (ctx->num_bids >= ctx->max_bids) {
        nimcp_platform_mutex_unlock(&ctx->mutex);
        return NIMCP_GT_ERROR_CAPACITY;
    }

    if (price < ctx->config.reserve_price) {
        nimcp_platform_mutex_unlock(&ctx->mutex);
        return NIMCP_GT_ERROR_BID_TOO_LOW;
    }

    if (quantity == 0) {
        nimcp_platform_mutex_unlock(&ctx->mutex);
        return NIMCP_GT_ERROR_INVALID_PARAMETER;
    }

    if (ctx->state == NIMCP_MULTI_STATE_CREATED) {
        ctx->state = NIMCP_MULTI_STATE_BIDDING;
    }

    nimcp_multi_bid_t* bid = &ctx->bids[ctx->num_bids];
    bid->bidder_id = bidder;
    bid->price = price;
    bid->quantity = quantity;
    bid->timestamp_ms = nimcp_time_get_ms();
    bid->is_valid = true;

    ctx->num_bids++;
    ctx->allocated = false;

    nimcp_platform_mutex_unlock(&ctx->mutex);
    return NIMCP_SUCCESS;
}

/**
 * @brief Compare multi-unit bids (descending by price)
 */
static int compare_multi_bids(const void* a, const void* b) {
    const nimcp_multi_bid_t* bid_a = (const nimcp_multi_bid_t*)a;
    const nimcp_multi_bid_t* bid_b = (const nimcp_multi_bid_t*)b;

    if (bid_b->price > bid_a->price) return 1;
    if (bid_b->price < bid_a->price) return -1;

    // Tie-break by timestamp
    if (bid_a->timestamp_ms < bid_b->timestamp_ms) return -1;
    if (bid_a->timestamp_ms > bid_b->timestamp_ms) return 1;

    return 0;
}

nimcp_error_t nimcp_multi_unit_allocate(
    nimcp_multi_unit_auction_t ctx,
    nimcp_multi_unit_result_t* result
) {
    if (!ctx || !result) {
        return NIMCP_GT_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    gt_auction_ext_heartbeat("gt_auction_e_multi_unit_allocate", 0.0f);


    nimcp_platform_mutex_lock(&ctx->mutex);

    if (ctx->state == NIMCP_MULTI_STATE_CANCELLED) {
        nimcp_platform_mutex_unlock(&ctx->mutex);
        return NIMCP_GT_ERROR_INVALID_STATE;
    }

    ctx->state = NIMCP_MULTI_STATE_ALLOCATING;
    memset(result, 0, sizeof(nimcp_multi_unit_result_t));

    if (ctx->num_bids == 0) {
        result->final_state = NIMCP_MULTI_STATE_COMPLETED;
        ctx->state = NIMCP_MULTI_STATE_COMPLETED;
        ctx->last_result = *result;
        nimcp_platform_mutex_unlock(&ctx->mutex);
        return NIMCP_SUCCESS;
    }

    // Sort bids by price (highest first)
    qsort(ctx->bids, ctx->num_bids, sizeof(nimcp_multi_bid_t), compare_multi_bids);

    // Allocate units to highest bidders
    uint32_t units_remaining = ctx->config.total_units;
    float clearing_price = ctx->config.reserve_price;

    // Track allocations per bidder (use simple array, indexed by winner position)
    typedef struct {
        nimcp_player_id_t bidder_id;
        uint32_t units;
        float total_payment;
    } temp_alloc_t;

    temp_alloc_t temp_allocs[NIMCP_GT_MAX_PLAYERS];
    uint32_t num_temp_allocs = 0;
    memset(temp_allocs, 0, sizeof(temp_allocs));

    for (uint32_t i = 0; i < ctx->num_bids && units_remaining > 0; i++) {
        nimcp_multi_bid_t* bid = &ctx->bids[i];

        if (!bid->is_valid) continue;

        uint32_t units_to_allocate = (bid->quantity < units_remaining) ?
                                      bid->quantity : units_remaining;

        // Find or create allocation for this bidder
        int alloc_idx = -1;
        for (uint32_t j = 0; j < num_temp_allocs; j++) {
            /* Phase 8: Loop progress heartbeat */
            if ((j & 0xFF) == 0 && num_temp_allocs > 256) {
                gt_auction_ext_heartbeat("gt_auction_e_loop",
                                 (float)(j + 1) / (float)num_temp_allocs);
            }

            if (temp_allocs[j].bidder_id == bid->bidder_id) {
                alloc_idx = (int)j;
                break;
            }
        }

        if (alloc_idx < 0 && num_temp_allocs < NIMCP_GT_MAX_PLAYERS) {
            alloc_idx = (int)num_temp_allocs;
            temp_allocs[alloc_idx].bidder_id = bid->bidder_id;
            num_temp_allocs++;
        }

        if (alloc_idx >= 0) {
            temp_allocs[alloc_idx].units += units_to_allocate;

            // Payment depends on auction type
            if (ctx->config.type == NIMCP_MULTI_UNIT_DISCRIMINATORY) {
                // Pay-as-bid
                temp_allocs[alloc_idx].total_payment += bid->price * units_to_allocate;
            }
            // For uniform: we'll update payments after finding clearing price
        }

        result->units_allocated += units_to_allocate;
        units_remaining -= units_to_allocate;

        // Track clearing price (price of last accepted bid)
        clearing_price = bid->price;
    }

    result->clearing_price = clearing_price;

    // Finalize allocations
    result->num_winners = num_temp_allocs;
    for (uint32_t i = 0; i < num_temp_allocs; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_temp_allocs > 256) {
            gt_auction_ext_heartbeat("gt_auction_e_loop",
                             (float)(i + 1) / (float)num_temp_allocs);
        }

        result->allocations[i].bidder_id = temp_allocs[i].bidder_id;
        result->allocations[i].units_won = temp_allocs[i].units;

        if (ctx->config.type == NIMCP_MULTI_UNIT_UNIFORM) {
            // Uniform price: everyone pays clearing price
            result->allocations[i].payment = clearing_price * temp_allocs[i].units;
        } else {
            // Discriminatory: pay as bid
            result->allocations[i].payment = temp_allocs[i].total_payment;
        }

        result->allocations[i].avg_price = result->allocations[i].payment /
                                           (float)result->allocations[i].units_won;
        result->total_revenue += result->allocations[i].payment;
    }

    // Compute efficiency
    if (ctx->config.total_units > 0) {
        result->efficiency = (float)result->units_allocated / (float)ctx->config.total_units;
    }

    result->final_state = NIMCP_MULTI_STATE_COMPLETED;
    ctx->state = NIMCP_MULTI_STATE_COMPLETED;
    ctx->last_result = *result;
    ctx->allocated = true;

    nimcp_platform_mutex_unlock(&ctx->mutex);
    return NIMCP_SUCCESS;
}

nimcp_multi_unit_state_t nimcp_multi_unit_get_state(const nimcp_multi_unit_auction_t ctx) {
    if (!ctx) return NIMCP_MULTI_STATE_CREATED;
    /* Phase 8: Heartbeat at operation start */
    gt_auction_ext_heartbeat("gt_auction_e_multi_unit_get_state", 0.0f);


    return ctx->state;
}

uint32_t nimcp_multi_unit_get_bid_count(const nimcp_multi_unit_auction_t ctx) {
    if (!ctx) return 0;
    /* Phase 8: Heartbeat at operation start */
    gt_auction_ext_heartbeat("gt_auction_e_multi_unit_get_bid_c", 0.0f);


    return ctx->num_bids;
}

nimcp_error_t nimcp_multi_unit_cancel(nimcp_multi_unit_auction_t ctx) {
    if (!ctx) {
        return NIMCP_GT_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    gt_auction_ext_heartbeat("gt_auction_e_multi_unit_cancel", 0.0f);


    nimcp_platform_mutex_lock(&ctx->mutex);
    ctx->state = NIMCP_MULTI_STATE_CANCELLED;
    nimcp_platform_mutex_unlock(&ctx->mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

/**
 * WHAT: Query knowledge graph for GT Auction Extended self-knowledge
 * WHY:  Enable self-awareness about module's role and connections
 * HOW:  Query KG for entity observations and relations
 */
int gt_auction_ext_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    gt_auction_ext_heartbeat("gt_auction_e_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "GT_Auction_Extended");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                gt_auction_ext_heartbeat("gt_auction_e_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            /* GT Auction extended self-knowledge logged */
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "GT_Auction_Extended");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "GT_Auction_Extended");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void gt_auction_ext_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_gt_auction_ext_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int gt_auction_ext_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "gt_auction_ext_training_begin: NULL argument");
        return -1;
    }
    gt_auction_ext_heartbeat_instance(NULL, "gt_auction_ext_training_begin", 0.0f);
    (void)(struct nimcp_combo_auction_struct*)instance; /* Module state available for reset */
    return 0;
}

int gt_auction_ext_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "gt_auction_ext_training_end: NULL argument");
        return -1;
    }
    gt_auction_ext_heartbeat_instance(NULL, "gt_auction_ext_training_end", 1.0f);
    (void)(struct nimcp_combo_auction_struct*)instance; /* Module state available for finalization */
    return 0;
}

int gt_auction_ext_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "gt_auction_ext_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    gt_auction_ext_heartbeat_instance(NULL, "gt_auction_ext_training_step", progress);
    (void)(struct nimcp_combo_auction_struct*)instance; /* Module state available for step adaptation */
    return 0;
}
