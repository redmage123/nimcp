//=============================================================================
// nimcp_gt_global_workspace.h - Game-Theoretic Global Workspace Integration
//=============================================================================
/**
 * @file nimcp_gt_global_workspace.h
 * @brief Game-theoretic competition strategy for Global Workspace
 *
 * WHAT: Auction-based competition for broadcast access
 * WHY:  Incentive-compatible, efficient access allocation
 * HOW:  Second-price auction where modules bid with salience
 *
 * BIOLOGICAL INSPIRATION:
 * - Neural assemblies competing for conscious access
 * - Ignition as auction threshold
 * - Global broadcast as resource allocation
 *
 * INTEGRATION: Adds COMPETITION_AUCTION strategy
 *
 * @author NIMCP Development Team
 * @date 2024-12-27
 * @version 1.0.0
 */

#ifndef NIMCP_GT_GLOBAL_WORKSPACE_H
#define NIMCP_GT_GLOBAL_WORKSPACE_H

#include "cognitive/game_theory/nimcp_auction.h"
#include "cognitive/global_workspace/nimcp_global_workspace.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Extended Competition Strategy
//=============================================================================

/**
 * @brief Game-theoretic competition strategies
 *
 * These extend the base competition_strategy_t enum.
 * Use gt_gw_set_strategy() to enable these.
 */
typedef enum {
    GT_GW_STRATEGY_SECOND_PRICE = 100, /**< Second-price (Vickrey) auction */
    GT_GW_STRATEGY_VCG,                /**< VCG mechanism (multi-slot) */
    GT_GW_STRATEGY_ASCENDING           /**< Ascending (English) auction */
} gt_gw_strategy_t;

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Auction-based GW configuration
 */
typedef struct {
    gt_gw_strategy_t strategy;        /**< Auction type */
    float reserve_price;              /**< Minimum bid for broadcast */
    float initial_budget;             /**< Initial module budget */
    float budget_replenish_rate;      /**< Budget restoration per cycle */
    float budget_replenish_interval_ms; /**< How often to replenish */
    bool enable_budget_constraints;   /**< Limit module spending? */
    bool track_bid_history;           /**< Keep history for analysis? */
    uint32_t history_depth;           /**< Bid history size */
} gt_gw_config_t;

/**
 * @brief Per-module bidding state
 */
typedef struct {
    cognitive_module_t module;        /**< Module identifier */
    float current_budget;             /**< Remaining budget */
    float total_spent;                /**< Cumulative spending */
    uint64_t bids_submitted;          /**< Total bids */
    uint64_t wins;                    /**< Broadcast wins */
    float avg_winning_bid;            /**< Average winning bid */
    float avg_payment;                /**< Average payment (may < bid) */
    uint64_t last_win_time_ms;        /**< Last win timestamp */
} gt_gw_module_state_t;

/**
 * @brief Auction round result
 */
typedef struct {
    cognitive_module_t winner;        /**< Winner module */
    float winning_bid;                /**< Winner's bid */
    float payment;                    /**< Actual payment */
    float second_highest_bid;         /**< Runner-up bid */
    uint32_t num_bidders;             /**< Bidders this round */
    float total_bids;                 /**< Sum of all bids */
    bool reserve_met;                 /**< Reserve price met? */
} gt_gw_round_result_t;

/**
 * @brief Opaque handle to GW auction context
 */
typedef struct gt_gw_auction_ctx_struct* gt_gw_auction_ctx_t;

//=============================================================================
// Lifecycle
//=============================================================================

/**
 * @brief Get default auction configuration
 */
gt_gw_config_t gt_gw_default_config(void);

/**
 * @brief Create auction context for global workspace
 *
 * @param workspace Global workspace to augment
 * @param config Auction configuration
 * @return Context handle or NULL
 */
gt_gw_auction_ctx_t gt_gw_create(
    global_workspace_t* workspace,
    const gt_gw_config_t* config
);

/**
 * @brief Destroy auction context
 */
void gt_gw_destroy(gt_gw_auction_ctx_t ctx);

//=============================================================================
// Core Operations
//=============================================================================

/**
 * @brief Submit bid for workspace access
 *
 * WHAT: Module bids for broadcast slot
 * WHY:  Truthful bidding under second-price mechanism
 * HOW:  Bid = salience (value to module)
 *
 * @param ctx Auction context
 * @param module Bidding module
 * @param content Content to broadcast if won
 * @param content_dim Content dimensions
 * @param bid Bid amount (typically = salience)
 * @return NIMCP_SUCCESS or error
 */
nimcp_error_t gt_gw_bid(
    gt_gw_auction_ctx_t ctx,
    cognitive_module_t module,
    const float* content,
    uint32_t content_dim,
    float bid
);

/**
 * @brief Resolve current auction round
 *
 * WHAT: Determine winner and broadcast their content
 * WHY:  Complete the allocation cycle
 * HOW:  Select highest bidder, compute payment, broadcast
 *
 * @param ctx Auction context
 * @param result Output round result
 * @return NIMCP_SUCCESS or error
 */
nimcp_error_t gt_gw_resolve(
    gt_gw_auction_ctx_t ctx,
    gt_gw_round_result_t* result
);

/**
 * @brief Combined bid and resolve (single-step competition)
 *
 * @param ctx Auction context
 * @param module Bidding module
 * @param content Content to broadcast
 * @param content_dim Content size
 * @param bid Bid amount
 * @return true if this module won
 */
bool gt_gw_compete(
    gt_gw_auction_ctx_t ctx,
    cognitive_module_t module,
    const float* content,
    uint32_t content_dim,
    float bid
);

/**
 * @brief Replenish module budgets
 *
 * @param ctx Auction context
 * @param amount Amount to add (0 = use config rate)
 */
void gt_gw_replenish_budgets(gt_gw_auction_ctx_t ctx, float amount);

/**
 * @brief Reset module budgets to initial value
 */
void gt_gw_reset_budgets(gt_gw_auction_ctx_t ctx);

//=============================================================================
// Query Functions
//=============================================================================

/**
 * @brief Get module bidding state
 */
nimcp_error_t gt_gw_get_module_state(
    const gt_gw_auction_ctx_t ctx,
    cognitive_module_t module,
    gt_gw_module_state_t* state
);

/**
 * @brief Get underlying auction handle
 */
nimcp_auction_t gt_gw_get_auction(const gt_gw_auction_ctx_t ctx);

/**
 * @brief Get underlying workspace
 */
global_workspace_t* gt_gw_get_workspace(const gt_gw_auction_ctx_t ctx);

/**
 * @brief Check if auction strategy is active
 */
bool gt_gw_is_auction_active(const gt_gw_auction_ctx_t ctx);

/**
 * @brief Get auction statistics
 */
nimcp_error_t gt_gw_get_stats(
    const gt_gw_auction_ctx_t ctx,
    nimcp_game_stats_t* stats
);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_GT_GLOBAL_WORKSPACE_H
