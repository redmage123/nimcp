//=============================================================================
// nimcp_gt_auction_ext.h - Extended Auction Mechanisms
//=============================================================================
/**
 * @file nimcp_gt_auction_ext.h
 * @brief Extended auction mechanisms: combinatorial, double, multi-unit
 *
 * WHAT: Advanced auction mechanisms beyond single-item sealed-bid
 * WHY:  Enable complex resource allocation scenarios in neural systems
 * HOW:  Combinatorial auctions, double auctions, discriminatory/uniform pricing
 *
 * BIOLOGICAL INSPIRATION:
 * - Combinatorial: Neural binding problem (combining features)
 * - Double auctions: Synaptic marketplace (neurotransmitter release/uptake)
 * - Multi-unit: Population coding (multiple neurons for same stimulus)
 *
 * INTEGRATION: Global Workspace, Working Memory allocation
 *
 * @author NIMCP Development Team
 * @date 2024-12-27
 * @version 1.0.0
 */

#ifndef NIMCP_GT_AUCTION_EXT_H
#define NIMCP_GT_AUCTION_EXT_H

#include "cognitive/game_theory/nimcp_game_theory.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Additional Error Codes (extending base 25000 range)
//=============================================================================

#define NIMCP_GT_ERROR_NO_ORDERS           (NIMCP_GT_ERROR_BASE + 60)
#define NIMCP_GT_ERROR_NO_CLEARING_PRICE   (NIMCP_GT_ERROR_BASE + 61)
#define NIMCP_GT_ERROR_BUNDLE_CONFLICT     (NIMCP_GT_ERROR_BASE + 62)
#define NIMCP_GT_ERROR_INFEASIBLE          (NIMCP_GT_ERROR_BASE + 63)

//=============================================================================
// Tier-Scaled Constants
//=============================================================================

/** Maximum items in combinatorial auction */
#if NIMCP_BUILD_TIER == PLATFORM_TIER_FULL_VALUE
    #define NIMCP_GT_MAX_COMBO_ITEMS 32
#elif NIMCP_BUILD_TIER == PLATFORM_TIER_MEDIUM
    #define NIMCP_GT_MAX_COMBO_ITEMS 16
#elif NIMCP_BUILD_TIER == PLATFORM_TIER_CONSTRAINED
    #define NIMCP_GT_MAX_COMBO_ITEMS 8
#else
    #define NIMCP_GT_MAX_COMBO_ITEMS 4
#endif

/** Maximum bundle bids */
#if NIMCP_BUILD_TIER == PLATFORM_TIER_FULL_VALUE
    #define NIMCP_GT_MAX_BUNDLE_BIDS 256
#elif NIMCP_BUILD_TIER == PLATFORM_TIER_MEDIUM
    #define NIMCP_GT_MAX_BUNDLE_BIDS 128
#elif NIMCP_BUILD_TIER == PLATFORM_TIER_CONSTRAINED
    #define NIMCP_GT_MAX_BUNDLE_BIDS 32
#else
    #define NIMCP_GT_MAX_BUNDLE_BIDS 16
#endif

/** Maximum orders in double auction */
#if NIMCP_BUILD_TIER == PLATFORM_TIER_FULL_VALUE
    #define NIMCP_GT_MAX_ORDERS 512
#elif NIMCP_BUILD_TIER == PLATFORM_TIER_MEDIUM
    #define NIMCP_GT_MAX_ORDERS 256
#elif NIMCP_BUILD_TIER == PLATFORM_TIER_CONSTRAINED
    #define NIMCP_GT_MAX_ORDERS 64
#else
    #define NIMCP_GT_MAX_ORDERS 32
#endif

/** Maximum trades in double auction */
#if NIMCP_BUILD_TIER == PLATFORM_TIER_FULL_VALUE
    #define NIMCP_GT_MAX_TRADES 256
#elif NIMCP_BUILD_TIER == PLATFORM_TIER_MEDIUM
    #define NIMCP_GT_MAX_TRADES 128
#elif NIMCP_BUILD_TIER == PLATFORM_TIER_CONSTRAINED
    #define NIMCP_GT_MAX_TRADES 32
#else
    #define NIMCP_GT_MAX_TRADES 16
#endif

//=============================================================================
// Combinatorial Auction Types
//=============================================================================

/**
 * @brief Winner determination algorithm
 */
typedef enum {
    NIMCP_COMBO_ALGO_OPTIMAL,         /**< Optimal (exponential worst case) */
    NIMCP_COMBO_ALGO_GREEDY,          /**< Greedy approximation */
    NIMCP_COMBO_ALGO_GENETIC,         /**< Genetic algorithm heuristic */
    NIMCP_COMBO_ALGO_COUNT
} nimcp_combo_algo_t;

/**
 * @brief Combinatorial auction state
 */
typedef enum {
    NIMCP_COMBO_STATE_CREATED,
    NIMCP_COMBO_STATE_BIDDING,
    NIMCP_COMBO_STATE_SOLVING,
    NIMCP_COMBO_STATE_COMPLETED,
    NIMCP_COMBO_STATE_NO_WINNER,
    NIMCP_COMBO_STATE_CANCELLED
} nimcp_combo_state_t;

/**
 * @brief Bundle bid: bid on a combination of items
 *
 * Items are represented as a bitmask where bit i indicates item i.
 * For example, items_mask = 0b1011 means bidding on items 0, 1, and 3.
 */
typedef struct {
    nimcp_player_id_t bidder_id;      /**< Who submitted the bid */
    uint64_t items_mask;              /**< Bitmask of items in bundle */
    float value;                      /**< Bid value for entire bundle */
    uint64_t timestamp_ms;            /**< When bid was submitted */
    bool is_valid;                    /**< Bid validity flag */
} nimcp_bundle_bid_t;

/**
 * @brief Combinatorial auction configuration
 */
typedef struct {
    uint32_t num_items;               /**< Number of items (max 64) */
    float reserve_prices[NIMCP_GT_MAX_COMBO_ITEMS]; /**< Reserve per item */
    uint32_t max_bidders;             /**< Maximum concurrent bidders */
    uint64_t deadline_ms;             /**< Bidding deadline (0 = none) */
    bool allow_xor_bids;              /**< Allow XOR bidding language */
    bool compute_vcg_payments;        /**< Compute VCG payments after solve */
    uint32_t max_iterations;          /**< Max iterations for heuristics */
    float time_limit_ms;              /**< Time limit for solving (0 = none) */
} nimcp_combo_auction_config_t;

/**
 * @brief Winner allocation for a single bidder
 */
typedef struct {
    nimcp_player_id_t bidder_id;      /**< Winning bidder */
    uint64_t items_won;               /**< Bitmask of items won */
    float bid_value;                  /**< Original bid value */
    float vcg_payment;                /**< VCG payment (if computed) */
} nimcp_combo_winner_t;

/**
 * @brief Combinatorial auction result
 */
typedef struct {
    nimcp_combo_winner_t winners[NIMCP_GT_MAX_PLAYERS]; /**< Winning allocations */
    uint32_t num_winners;             /**< Number of winners */
    uint64_t allocated_items;         /**< Bitmask of allocated items */
    float total_value;                /**< Sum of winning bids */
    float total_vcg_payments;         /**< Sum of VCG payments */
    float efficiency;                 /**< Allocation efficiency (0-1) */
    uint32_t iterations_used;         /**< Iterations for heuristic */
    float solve_time_ms;              /**< Time to solve */
    nimcp_combo_state_t final_state;  /**< Final auction state */
} nimcp_combo_result_t;

/**
 * @brief Opaque combinatorial auction handle
 */
typedef struct nimcp_combo_auction_struct* nimcp_combo_auction_t;

//=============================================================================
// Double Auction Types
//=============================================================================

/**
 * @brief Order side (buy or sell)
 */
typedef enum {
    NIMCP_ORDER_BUY,                  /**< Buy order (bid) */
    NIMCP_ORDER_SELL                  /**< Sell order (ask) */
} nimcp_order_side_t;

/**
 * @brief Double auction clearing rule
 */
typedef enum {
    NIMCP_CLEARING_UNIFORM,           /**< Uniform price (same for all) */
    NIMCP_CLEARING_DISCRIMINATORY,    /**< Pay-as-bid */
    NIMCP_CLEARING_MIDPOINT           /**< Trade at midpoint of matched orders */
} nimcp_clearing_rule_t;

/**
 * @brief Double auction state
 */
typedef enum {
    NIMCP_DOUBLE_STATE_CREATED,
    NIMCP_DOUBLE_STATE_COLLECTING,
    NIMCP_DOUBLE_STATE_CLEARING,
    NIMCP_DOUBLE_STATE_COMPLETED,
    NIMCP_DOUBLE_STATE_NO_TRADES,
    NIMCP_DOUBLE_STATE_CANCELLED
} nimcp_double_state_t;

/**
 * @brief Single order (buy or sell)
 */
typedef struct {
    nimcp_player_id_t trader_id;      /**< Who submitted the order */
    nimcp_order_side_t side;          /**< Buy or sell */
    float price;                      /**< Limit price */
    uint32_t quantity;                /**< Quantity desired */
    uint32_t filled_quantity;         /**< Quantity filled so far */
    uint64_t timestamp_ms;            /**< When submitted */
    bool is_valid;                    /**< Order validity flag */
} nimcp_order_t;

/**
 * @brief Matched trade between buyer and seller
 */
typedef struct {
    nimcp_player_id_t buyer_id;       /**< Buyer ID */
    nimcp_player_id_t seller_id;      /**< Seller ID */
    float trade_price;                /**< Execution price */
    uint32_t quantity;                /**< Traded quantity */
    float buyer_surplus;              /**< Buyer surplus (value - price) */
    float seller_surplus;             /**< Seller surplus (price - cost) */
    uint64_t timestamp_ms;            /**< When trade occurred */
} nimcp_trade_t;

/**
 * @brief Double auction configuration
 */
typedef struct {
    nimcp_clearing_rule_t clearing_rule; /**< How to determine prices */
    uint32_t max_orders;              /**< Maximum orders to accept */
    uint64_t deadline_ms;             /**< Order collection deadline (0 = none) */
    float min_price;                  /**< Minimum allowed price */
    float max_price;                  /**< Maximum allowed price (0 = unlimited) */
    bool allow_partial_fills;         /**< Allow partial order fills */
    bool continuous_clearing;         /**< Clear continuously or batch */
} nimcp_double_auction_config_t;

/**
 * @brief Market clearing result
 */
typedef struct {
    float clearing_price;             /**< Market clearing price (uniform) */
    uint32_t total_quantity;          /**< Total quantity traded */
    uint32_t num_trades;              /**< Number of trades */
    float total_buyer_surplus;        /**< Total buyer surplus */
    float total_seller_surplus;       /**< Total seller surplus */
    float total_welfare;              /**< Total social welfare */
    float efficiency;                 /**< Market efficiency (0-1) */
    nimcp_double_state_t final_state; /**< Final auction state */
} nimcp_clearing_result_t;

/**
 * @brief Opaque double auction handle
 */
typedef struct nimcp_double_auction_struct* nimcp_double_auction_t;

//=============================================================================
// Multi-Unit Auction Types
//=============================================================================

/**
 * @brief Multi-unit auction type
 */
typedef enum {
    NIMCP_MULTI_UNIT_DISCRIMINATORY,  /**< Pay-as-bid (discriminatory) */
    NIMCP_MULTI_UNIT_UNIFORM          /**< Uniform price */
} nimcp_multi_unit_type_t;

/**
 * @brief Multi-unit auction state
 */
typedef enum {
    NIMCP_MULTI_STATE_CREATED,
    NIMCP_MULTI_STATE_BIDDING,
    NIMCP_MULTI_STATE_ALLOCATING,
    NIMCP_MULTI_STATE_COMPLETED,
    NIMCP_MULTI_STATE_CANCELLED
} nimcp_multi_unit_state_t;

/**
 * @brief Multi-unit bid: quantity at price
 */
typedef struct {
    nimcp_player_id_t bidder_id;      /**< Who submitted the bid */
    float price;                      /**< Bid price per unit */
    uint32_t quantity;                /**< Quantity desired */
    uint64_t timestamp_ms;            /**< When submitted */
    bool is_valid;                    /**< Validity flag */
} nimcp_multi_bid_t;

/**
 * @brief Multi-unit allocation for a bidder
 */
typedef struct {
    nimcp_player_id_t bidder_id;      /**< Bidder ID */
    uint32_t units_won;               /**< Number of units allocated */
    float payment;                    /**< Total payment */
    float avg_price;                  /**< Average price per unit */
} nimcp_multi_alloc_t;

/**
 * @brief Multi-unit auction configuration
 */
typedef struct {
    nimcp_multi_unit_type_t type;     /**< Discriminatory or uniform */
    uint32_t total_units;             /**< Total units available */
    float reserve_price;              /**< Minimum acceptable price */
    uint32_t max_bidders;             /**< Maximum bidders */
    uint64_t deadline_ms;             /**< Deadline (0 = none) */
} nimcp_multi_unit_config_t;

/**
 * @brief Multi-unit auction result
 */
typedef struct {
    nimcp_multi_alloc_t allocations[NIMCP_GT_MAX_PLAYERS]; /**< Allocations */
    uint32_t num_winners;             /**< Number of winning bidders */
    uint32_t units_allocated;         /**< Total units allocated */
    float total_revenue;              /**< Total revenue */
    float clearing_price;             /**< Clearing price (uniform) */
    float efficiency;                 /**< Allocation efficiency */
    nimcp_multi_unit_state_t final_state; /**< Final state */
} nimcp_multi_unit_result_t;

/**
 * @brief Opaque multi-unit auction handle
 */
typedef struct nimcp_multi_unit_auction_struct* nimcp_multi_unit_auction_t;

//=============================================================================
// Combinatorial Auction API
//=============================================================================

/**
 * @brief Get default combinatorial auction configuration
 */
nimcp_combo_auction_config_t nimcp_combo_auction_default_config(uint32_t num_items);

/**
 * @brief Create a combinatorial auction
 *
 * WHAT: Initialize combinatorial auction context
 * WHY:  Enable bundle bidding for complex resource allocation
 * HOW:  Allocate context, initialize data structures
 *
 * @param config Auction configuration
 * @return Auction handle or NULL on failure
 */
nimcp_combo_auction_t nimcp_combo_auction_create(
    const nimcp_combo_auction_config_t* config
);

/**
 * @brief Destroy a combinatorial auction
 */
void nimcp_combo_auction_destroy(nimcp_combo_auction_t ctx);

/**
 * @brief Submit a bundle bid
 *
 * WHAT: Bid on a bundle of items
 * WHY:  Express complementarity/substitutability of items
 * HOW:  Store bid with items_mask and value
 *
 * @param ctx Auction handle
 * @param bidder Bidder ID
 * @param items_mask Bitmask of items in bundle
 * @param value Bid value for entire bundle
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t nimcp_combo_auction_submit_bundle_bid(
    nimcp_combo_auction_t ctx,
    nimcp_player_id_t bidder,
    uint64_t items_mask,
    float value
);

/**
 * @brief Solve winner determination problem optimally
 *
 * WHAT: Find optimal allocation maximizing social welfare
 * WHY:  Achieve efficient resource allocation
 * HOW:  Branch-and-bound or integer programming
 *
 * WARNING: Exponential worst-case complexity in number of items
 *
 * @param ctx Auction handle
 * @param result Output result structure
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t nimcp_combo_auction_solve_optimal(
    nimcp_combo_auction_t ctx,
    nimcp_combo_result_t* result
);

/**
 * @brief Solve winner determination with greedy approximation
 *
 * WHAT: Fast approximation algorithm
 * WHY:  Handle large auctions efficiently
 * HOW:  Greedily select highest value-density bundles
 *
 * @param ctx Auction handle
 * @param result Output result structure
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t nimcp_combo_auction_solve_greedy(
    nimcp_combo_auction_t ctx,
    nimcp_combo_result_t* result
);

/**
 * @brief Compute VCG payments for winners
 *
 * WHAT: Calculate incentive-compatible payments
 * WHY:  Ensure truthful bidding is dominant strategy
 * HOW:  VCG payment = externality imposed on others
 *
 * Must be called after solve_optimal or solve_greedy
 *
 * @param ctx Auction handle
 * @param result Result structure to update with payments
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t nimcp_combo_auction_get_vcg_payments(
    nimcp_combo_auction_t ctx,
    nimcp_combo_result_t* result
);

/**
 * @brief Get current auction state
 */
nimcp_combo_state_t nimcp_combo_auction_get_state(const nimcp_combo_auction_t ctx);

/**
 * @brief Get number of bundle bids received
 */
uint32_t nimcp_combo_auction_get_bid_count(const nimcp_combo_auction_t ctx);

/**
 * @brief Cancel the auction
 */
nimcp_error_t nimcp_combo_auction_cancel(nimcp_combo_auction_t ctx);

//=============================================================================
// Double Auction API
//=============================================================================

/**
 * @brief Get default double auction configuration
 */
nimcp_double_auction_config_t nimcp_double_auction_default_config(void);

/**
 * @brief Create a double auction
 *
 * WHAT: Initialize double auction context
 * WHY:  Enable two-sided market with buyers and sellers
 * HOW:  Allocate context, initialize order books
 *
 * @param config Auction configuration
 * @return Auction handle or NULL on failure
 */
nimcp_double_auction_t nimcp_double_auction_create(
    const nimcp_double_auction_config_t* config
);

/**
 * @brief Destroy a double auction
 */
void nimcp_double_auction_destroy(nimcp_double_auction_t ctx);

/**
 * @brief Submit a buy order
 *
 * WHAT: Buyer submits bid with max price and quantity
 * WHY:  Express demand for goods
 * HOW:  Add to buy order book
 *
 * @param ctx Auction handle
 * @param buyer Buyer ID
 * @param max_price Maximum price willing to pay
 * @param quantity Desired quantity
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t nimcp_double_auction_submit_buy(
    nimcp_double_auction_t ctx,
    nimcp_player_id_t buyer,
    float max_price,
    uint32_t quantity
);

/**
 * @brief Submit a sell order
 *
 * WHAT: Seller submits ask with min price and quantity
 * WHY:  Express supply of goods
 * HOW:  Add to sell order book
 *
 * @param ctx Auction handle
 * @param seller Seller ID
 * @param min_price Minimum price willing to accept
 * @param quantity Available quantity
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t nimcp_double_auction_submit_sell(
    nimcp_double_auction_t ctx,
    nimcp_player_id_t seller,
    float min_price,
    uint32_t quantity
);

/**
 * @brief Clear the market
 *
 * WHAT: Match buyers and sellers, determine trades
 * WHY:  Execute trades at market-clearing price
 * HOW:  Sort orders, find intersection of supply/demand
 *
 * @param ctx Auction handle
 * @param result Output clearing result
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t nimcp_double_auction_clear(
    nimcp_double_auction_t ctx,
    nimcp_clearing_result_t* result
);

/**
 * @brief Get matched trades
 *
 * @param ctx Auction handle
 * @param trades Output array for trades
 * @param max_trades Maximum trades to return
 * @param num_trades Output: actual number of trades
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t nimcp_double_auction_get_trades(
    nimcp_double_auction_t ctx,
    nimcp_trade_t* trades,
    uint32_t max_trades,
    uint32_t* num_trades
);

/**
 * @brief Get surplus measures
 *
 * @param ctx Auction handle
 * @param buyer_surplus Output: total buyer surplus
 * @param seller_surplus Output: total seller surplus
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t nimcp_double_auction_get_surplus(
    nimcp_double_auction_t ctx,
    float* buyer_surplus,
    float* seller_surplus
);

/**
 * @brief Get current auction state
 */
nimcp_double_state_t nimcp_double_auction_get_state(const nimcp_double_auction_t ctx);

/**
 * @brief Get order counts
 */
nimcp_error_t nimcp_double_auction_get_order_counts(
    const nimcp_double_auction_t ctx,
    uint32_t* buy_orders,
    uint32_t* sell_orders
);

/**
 * @brief Cancel the auction
 */
nimcp_error_t nimcp_double_auction_cancel(nimcp_double_auction_t ctx);

//=============================================================================
// Multi-Unit Auction API
//=============================================================================

/**
 * @brief Get default multi-unit auction configuration
 */
nimcp_multi_unit_config_t nimcp_multi_unit_default_config(uint32_t total_units);

/**
 * @brief Create a multi-unit auction
 *
 * WHAT: Initialize multi-unit auction context
 * WHY:  Auction multiple identical units
 * HOW:  Allocate context, configure pricing rule
 *
 * @param config Auction configuration
 * @return Auction handle or NULL on failure
 */
nimcp_multi_unit_auction_t nimcp_multi_unit_create(
    const nimcp_multi_unit_config_t* config
);

/**
 * @brief Destroy a multi-unit auction
 */
void nimcp_multi_unit_destroy(nimcp_multi_unit_auction_t ctx);

/**
 * @brief Submit a bid for units
 *
 * @param ctx Auction handle
 * @param bidder Bidder ID
 * @param price Price per unit
 * @param quantity Number of units desired
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t nimcp_multi_unit_submit_bid(
    nimcp_multi_unit_auction_t ctx,
    nimcp_player_id_t bidder,
    float price,
    uint32_t quantity
);

/**
 * @brief Allocate units to winners
 *
 * WHAT: Determine allocation and payments
 * WHY:  Conclude the auction
 * HOW:  Sort by price, allocate, apply pricing rule
 *
 * @param ctx Auction handle
 * @param result Output result structure
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t nimcp_multi_unit_allocate(
    nimcp_multi_unit_auction_t ctx,
    nimcp_multi_unit_result_t* result
);

/**
 * @brief Get current auction state
 */
nimcp_multi_unit_state_t nimcp_multi_unit_get_state(const nimcp_multi_unit_auction_t ctx);

/**
 * @brief Get number of bids received
 */
uint32_t nimcp_multi_unit_get_bid_count(const nimcp_multi_unit_auction_t ctx);

/**
 * @brief Cancel the auction
 */
nimcp_error_t nimcp_multi_unit_cancel(nimcp_multi_unit_auction_t ctx);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Count bits set in items mask
 *
 * @param mask Bitmask
 * @return Number of items in bundle
 */
uint32_t nimcp_bundle_count_items(uint64_t mask);

/**
 * @brief Check if two bundles overlap
 *
 * @param mask1 First bundle mask
 * @param mask2 Second bundle mask
 * @return true if bundles share any items
 */
bool nimcp_bundles_overlap(uint64_t mask1, uint64_t mask2);

/**
 * @brief Get combo algorithm name
 */
const char* nimcp_combo_algo_name(nimcp_combo_algo_t algo);

/**
 * @brief Get clearing rule name
 */
const char* nimcp_clearing_rule_name(nimcp_clearing_rule_t rule);

/**
 * @brief Get multi-unit type name
 */
const char* nimcp_multi_unit_type_name(nimcp_multi_unit_type_t type);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_GT_AUCTION_EXT_H
