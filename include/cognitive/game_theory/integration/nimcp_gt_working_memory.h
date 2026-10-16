//=============================================================================
// nimcp_gt_working_memory.h - Priority Auction for Working Memory Slots
//=============================================================================
/**
 * @file nimcp_gt_working_memory.h
 * @brief Priority auction for working memory slot allocation
 *
 * WHAT: Auction-based eviction policy for working memory
 * WHY:  Market mechanism for efficient slot allocation
 * HOW:  Items bid with salience, lowest bidders evicted
 *
 * BIOLOGICAL INSPIRATION:
 * - Attentional competition among memory traces
 * - Rehearsal as bidding for retention
 * - Decay as declining bid strength
 *
 * INTEGRATION: Extends WM eviction policy
 *
 * @author NIMCP Development Team
 * @date 2024-12-27
 * @version 1.0.0
 */

#ifndef NIMCP_GT_WORKING_MEMORY_H
#define NIMCP_GT_WORKING_MEMORY_H

#include "cognitive/game_theory/nimcp_auction.h"
#include "cognitive/nimcp_working_memory.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Extended Eviction Policy
//=============================================================================

/**
 * @brief Game-theoretic eviction policies
 *
 * Extends standard eviction with auction mechanisms.
 */
typedef enum {
    GT_WM_EVICTION_AUCTION = 100,     /**< Auction for slot retention */
    GT_WM_EVICTION_CONGESTION,        /**< Congestion game dynamics */
    GT_WM_EVICTION_PRIORITY_AUCTION   /**< Priority-weighted auction */
} gt_wm_eviction_policy_t;

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Auction-based WM configuration
 */
typedef struct {
    gt_wm_eviction_policy_t policy;   /**< Eviction policy */
    float slot_reserve_price;         /**< Minimum bid to retain slot */
    float congestion_cost_factor;     /**< Cost increase per occupied slot */
    float decay_rate;                 /**< Bid decay per cycle */
    bool enable_preemption;           /**< Can new items preempt existing? */
    float preemption_premium;         /**< Extra bid needed to preempt */
    uint32_t eviction_interval_ms;    /**< How often to run auction */
} gt_wm_config_t;

/**
 * @brief Item bidding state
 */
typedef struct {
    uint32_t item_index;              /**< Slot index */
    float current_bid;                /**< Current retention bid */
    float initial_bid;                /**< Original bid when added */
    float congestion_cost;            /**< Current congestion cost */
    float effective_value;            /**< bid - cost */
    uint32_t age_cycles;              /**< Cycles since added */
    uint32_t rehearsal_count;         /**< Times refreshed */
    uint64_t last_access_time_ms;     /**< Last access timestamp */
} gt_wm_slot_state_t;

/**
 * @brief Eviction round result
 */
typedef struct {
    uint32_t evicted_indices[16];     /**< Indices of evicted items */
    uint32_t num_evicted;             /**< Count of evictions */
    float lowest_surviving_bid;       /**< Threshold bid */
    float total_congestion_cost;      /**< Sum of congestion costs */
    uint32_t occupancy_before;        /**< Slots used before eviction */
    uint32_t occupancy_after;         /**< Slots used after eviction */
} gt_wm_eviction_result_t;

/**
 * @brief Opaque handle to WM auction context
 */
typedef struct gt_wm_auction_ctx_struct* gt_wm_auction_ctx_t;

//=============================================================================
// Lifecycle
//=============================================================================

/**
 * @brief Get default WM auction configuration
 */
gt_wm_config_t gt_wm_default_config(void);

/**
 * @brief Create WM auction context
 *
 * @param wm Working memory to augment
 * @param config Auction configuration
 * @return Context handle or NULL
 */
gt_wm_auction_ctx_t gt_wm_create(
    working_memory_t* wm,
    const gt_wm_config_t* config
);

/**
 * @brief Destroy WM auction context
 */
void gt_wm_destroy(gt_wm_auction_ctx_t ctx);

//=============================================================================
// Core Operations
//=============================================================================

/**
 * @brief Add item to WM via auction
 *
 * WHAT: Bid for working memory slot
 * WHY:  Market-based slot allocation
 * HOW:  Compare bid to reserve and existing bids
 *
 * @param ctx Auction context
 * @param item Item content
 * @param item_size Item size
 * @param bid Retention bid (typically = salience)
 * @param slot_index Output: assigned slot (-1 if failed)
 * @return NIMCP_SUCCESS or error
 */
nimcp_error_t gt_wm_add(
    gt_wm_auction_ctx_t ctx,
    const float* item,
    uint32_t item_size,
    float bid,
    int32_t* slot_index
);

/**
 * @brief Refresh item bid (rehearsal)
 *
 * @param ctx Auction context
 * @param slot_index Slot to refresh
 * @param new_bid New bid value (0 = restore initial)
 * @return NIMCP_SUCCESS or error
 */
nimcp_error_t gt_wm_refresh(
    gt_wm_auction_ctx_t ctx,
    uint32_t slot_index,
    float new_bid
);

/**
 * @brief Run eviction auction
 *
 * WHAT: Determine which items retain slots
 * WHY:  Efficient allocation under capacity
 * HOW:  Auction among items, lowest bidders evicted
 *
 * @param ctx Auction context
 * @param result Output eviction result
 * @return NIMCP_SUCCESS or error
 */
nimcp_error_t gt_wm_run_eviction(
    gt_wm_auction_ctx_t ctx,
    gt_wm_eviction_result_t* result
);

/**
 * @brief Apply decay to all bids
 *
 * @param ctx Auction context
 * @param decay_amount Amount to decay (0 = use config rate)
 * @return NIMCP_SUCCESS or error
 */
nimcp_error_t gt_wm_apply_decay(
    gt_wm_auction_ctx_t ctx,
    float decay_amount
);

/**
 * @brief Compute current congestion cost
 *
 * @param ctx Auction context
 * @return Current congestion cost
 */
float gt_wm_get_congestion_cost(const gt_wm_auction_ctx_t ctx);

//=============================================================================
// Query Functions
//=============================================================================

/**
 * @brief Get slot bidding state
 */
nimcp_error_t gt_wm_get_slot_state(
    const gt_wm_auction_ctx_t ctx,
    uint32_t slot_index,
    gt_wm_slot_state_t* state
);

/**
 * @brief Get underlying working memory
 */
working_memory_t* gt_wm_get_wm(const gt_wm_auction_ctx_t ctx);

/**
 * @brief Get current occupancy
 */
uint32_t gt_wm_get_occupancy(const gt_wm_auction_ctx_t ctx);

/**
 * @brief Get highest current bid
 */
float gt_wm_get_highest_bid(const gt_wm_auction_ctx_t ctx);

/**
 * @brief Get lowest current bid
 */
float gt_wm_get_lowest_bid(const gt_wm_auction_ctx_t ctx);

/**
 * @brief Check if auction policy is active
 */
bool gt_wm_is_active(const gt_wm_auction_ctx_t ctx);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_GT_WORKING_MEMORY_H
