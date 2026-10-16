//=============================================================================
// nimcp_auction.h - Auction Mechanisms for Resource Allocation
//=============================================================================
/**
 * @file nimcp_auction.h
 * @brief Auction mechanisms for resource allocation
 *
 * WHAT: Implements auction mechanisms for competitive resource allocation
 * WHY:  Enable efficient, incentive-compatible resource allocation
 * HOW:  Second-price auctions, VCG mechanism, ascending/descending auctions
 *
 * BIOLOGICAL INSPIRATION:
 * - Competitive foraging in animal groups
 * - Neural competition for representation in cortex
 * - Synaptic competition for limited resources
 *
 * INTEGRATION: Global Workspace (COMPETITION_AUCTION strategy)
 *
 * @author NIMCP Development Team
 * @date 2024-12-27
 * @version 1.0.0
 */

#ifndef NIMCP_AUCTION_H
#define NIMCP_AUCTION_H

#include "cognitive/game_theory/nimcp_game_theory.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Auction Types
//=============================================================================

typedef enum {
    NIMCP_AUCTION_FIRST_PRICE,        /**< First-price sealed bid */
    NIMCP_AUCTION_SECOND_PRICE,       /**< Vickrey (second-price) auction */
    NIMCP_AUCTION_VCG,                /**< Vickrey-Clarke-Groves mechanism */
    NIMCP_AUCTION_ASCENDING,          /**< English auction (ascending) */
    NIMCP_AUCTION_DESCENDING,         /**< Dutch auction (descending) */
    NIMCP_AUCTION_COUNT
} nimcp_auction_type_t;

typedef enum {
    NIMCP_AUCTION_STATE_CREATED,
    NIMCP_AUCTION_STATE_BIDDING,
    NIMCP_AUCTION_STATE_RESOLVING,
    NIMCP_AUCTION_STATE_COMPLETED,
    NIMCP_AUCTION_STATE_CANCELLED,
    NIMCP_AUCTION_STATE_NO_WINNER     /**< No bids met reserve */
} nimcp_auction_state_t;

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Single bid submission
 */
typedef struct {
    nimcp_player_id_t bidder_id;      /**< Who submitted the bid */
    float bid_amount;                 /**< Bid value */
    uint32_t item_id;                 /**< Item being bid on (0 for single-item) */
    float valuation;                  /**< Bidder's private valuation (for VCG) */
    uint64_t timestamp_ms;            /**< When bid was submitted */
    bool is_valid;                    /**< Bid validity flag */
} nimcp_bid_t;

/**
 * @brief Auction configuration
 */
typedef struct {
    nimcp_auction_type_t type;        /**< Auction mechanism type */
    uint32_t num_items;               /**< Number of items (1 for single-item) */
    float reserve_price;              /**< Minimum acceptable bid */
    uint64_t deadline_ms;             /**< Bidding deadline (0 = no deadline) */
    uint32_t max_bidders;             /**< Maximum concurrent bidders */
    bool allow_tie_random;            /**< Random tie-breaking if true */
    bool reveal_bids;                 /**< Reveal bids after completion? */
    float ascending_increment;        /**< Minimum increment for ascending */
    float descending_decrement;       /**< Decrement for Dutch auction */
} nimcp_auction_config_t;

/**
 * @brief Auction result (single item)
 */
typedef struct {
    nimcp_player_id_t winner_id;      /**< Winner ID (INVALID if none) */
    float winning_bid;                /**< Winning bid amount */
    float payment;                    /**< Actual payment (may differ for VCG) */
    float second_highest_bid;         /**< Runner-up bid */
    uint32_t num_bids;                /**< Total bids received */
    nimcp_auction_state_t final_state; /**< Final auction state */
    float efficiency;                 /**< Allocation efficiency (0-1) */
    uint64_t resolution_time_ms;      /**< When resolved */
} nimcp_auction_result_t;

/**
 * @brief VCG-specific result (multi-item)
 */
typedef struct {
    nimcp_player_id_t winners[NIMCP_GT_MAX_AUCTION_ITEMS]; /**< Winner per item */
    float payments[NIMCP_GT_MAX_AUCTION_ITEMS];   /**< VCG payment per winner */
    float allocations[NIMCP_GT_MAX_AUCTION_ITEMS]; /**< What each got */
    uint32_t num_items;               /**< Number of items allocated */
    float total_welfare;              /**< Total social welfare achieved */
    float revenue;                    /**< Total auctioneer revenue */
    bool is_efficient;                /**< Achieved efficient allocation? */
} nimcp_vcg_result_t;

/**
 * @brief Opaque auction handle
 */
typedef struct nimcp_auction_struct* nimcp_auction_t;

//=============================================================================
// Lifecycle
//=============================================================================

/**
 * @brief Get default auction configuration
 */
nimcp_auction_config_t nimcp_auction_default_config(void);

/**
 * @brief Create an auction
 *
 * @param config Auction configuration
 * @return Auction handle or NULL on failure
 */
nimcp_auction_t nimcp_auction_create(const nimcp_auction_config_t* config);

/**
 * @brief Destroy an auction
 */
void nimcp_auction_destroy(nimcp_auction_t auction);

//=============================================================================
// Core Operations
//=============================================================================

/**
 * @brief Submit bid to auction
 *
 * WHAT: Bidder submits bid for item
 * WHY:  Competitive price discovery
 * HOW:  Store bid, validate against rules
 *
 * @param auction Auction handle
 * @param bidder_id Bidder identifier
 * @param bid_amount Bid value
 * @param item_id Item ID (0 for single-item auctions)
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t nimcp_auction_bid(
    nimcp_auction_t auction,
    nimcp_player_id_t bidder_id,
    float bid_amount,
    uint32_t item_id
);

/**
 * @brief Submit bid with private valuation (for VCG)
 *
 * @param auction Auction handle
 * @param bidder_id Bidder identifier
 * @param bid_amount Bid amount
 * @param private_valuation True valuation (for VCG payment)
 * @param item_id Item ID
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t nimcp_auction_bid_vcg(
    nimcp_auction_t auction,
    nimcp_player_id_t bidder_id,
    float bid_amount,
    float private_valuation,
    uint32_t item_id
);

/**
 * @brief Resolve auction and determine winners
 *
 * WHAT: Determine winner and payment
 * WHY:  Complete the auction mechanism
 * HOW:  Apply auction rules (first-price, second-price, etc.)
 *
 * @param auction Auction handle
 * @param result Output result structure
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t nimcp_auction_resolve(
    nimcp_auction_t auction,
    nimcp_auction_result_t* result
);

/**
 * @brief Resolve VCG auction (multi-item)
 *
 * WHAT: Compute VCG allocation and payments
 * WHY:  Incentive-compatible multi-item allocation
 * HOW:  Winner pays externality imposed on others
 *
 * @param auction Auction handle
 * @param result Output VCG result
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t nimcp_auction_resolve_vcg(
    nimcp_auction_t auction,
    nimcp_vcg_result_t* result
);

/**
 * @brief Cancel auction
 */
nimcp_error_t nimcp_auction_cancel(nimcp_auction_t auction);

/**
 * @brief Advance ascending auction by one round
 *
 * @param auction Auction handle
 * @param current_price Current highest price
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t nimcp_auction_ascending_round(
    nimcp_auction_t auction,
    float* current_price
);

/**
 * @brief Advance descending auction by one tick
 *
 * @param auction Auction handle
 * @param current_price Current price
 * @param claimed_by Output: who claimed (if any)
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t nimcp_auction_descending_tick(
    nimcp_auction_t auction,
    float* current_price,
    nimcp_player_id_t* claimed_by
);

//=============================================================================
// Query Functions
//=============================================================================

/**
 * @brief Get current auction state
 */
nimcp_auction_state_t nimcp_auction_get_state(const nimcp_auction_t auction);

/**
 * @brief Get number of bids received
 */
uint32_t nimcp_auction_get_bid_count(const nimcp_auction_t auction);

/**
 * @brief Get current highest bid
 */
float nimcp_auction_get_current_highest(const nimcp_auction_t auction);

/**
 * @brief Get auction type
 */
nimcp_auction_type_t nimcp_auction_get_type(const nimcp_auction_t auction);

/**
 * @brief Check if auction type is strategyproof
 *
 * Strategyproof means truthful bidding is a dominant strategy.
 * Second-price and VCG are strategyproof.
 */
bool nimcp_auction_is_strategyproof(nimcp_auction_type_t type);

/**
 * @brief Get auction type name
 */
const char* nimcp_auction_type_name(nimcp_auction_type_t type);

/**
 * @brief Get all bids (if revealed)
 *
 * @param auction Auction handle
 * @param bids Output array
 * @param max_bids Maximum bids to retrieve
 * @param num_bids Output: actual number
 * @return NIMCP_SUCCESS or error
 */
nimcp_error_t nimcp_auction_get_bids(
    const nimcp_auction_t auction,
    nimcp_bid_t* bids,
    uint32_t max_bids,
    uint32_t* num_bids
);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_AUCTION_H
