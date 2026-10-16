/**
 * @file nimcp_attention_wm_bridge.h
 * @brief Attention-WorkingMemory Integration Bridge
 * @version 1.0.0
 * @date 2025-12-31
 *
 * WHAT: Bidirectional integration between attention and working memory systems
 * WHY:  Attention gates entry into working memory; WM capacity is limited by
 *       attention. Focus shifts update WM item priorities.
 * HOW:  Attention strength determines WM gating; priority updates track focus;
 *       items below threshold are evicted.
 *
 * BIOLOGICAL BASIS:
 * - Prefrontal cortex maintains WM representations via sustained attention
 * - Cowan's embedded-processes model: attention selects WM contents
 * - Basal ganglia gates WM updates based on attentional signals
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_ATTENTION_WM_BRIDGE_H
#define NIMCP_ATTENTION_WM_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define ATTENTION_WM_MAX_ITEMS            64
#define ATTENTION_WM_DEFAULT_CAPACITY     7
#define ATTENTION_WM_MIN_PRIORITY         0.0f
#define ATTENTION_WM_MAX_PRIORITY         1.0f

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct attention_wm_bridge attention_wm_bridge_t;

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Working memory item with attention metadata
 */
typedef struct {
    uint64_t item_id;                    /**< Unique item identifier */
    float priority;                      /**< Current priority [0, 1] */
    float attention_strength;            /**< Attention when gated in */
    uint64_t entry_time;                 /**< When item entered WM */
    uint64_t last_access_time;           /**< Last time item was accessed */
} attention_wm_item_t;

/**
 * @brief Configuration for Attention-WM bridge
 */
typedef struct {
    size_t capacity_limit;               /**< Maximum WM capacity */
    float attention_threshold;           /**< Min attention for WM entry */
    float decay_rate;                    /**< Priority decay rate per second */
} attention_wm_config_t;

/**
 * @brief Statistics for Attention-WM bridge
 */
typedef struct {
    uint64_t items_gated_in;             /**< Items admitted to WM */
    uint64_t items_evicted;              /**< Items evicted from WM */
    uint64_t priority_updates;           /**< Priority update operations */
    uint64_t focus_shifts;               /**< Focus shift events */
    size_t current_item_count;           /**< Current items in WM */
    float avg_priority;                  /**< Average item priority */
} attention_wm_stats_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default Attention-WM configuration
 *
 * WHAT: Provide sensible default configuration
 * WHY:  Easy initialization with biologically-plausible defaults
 * HOW:  Set capacity ~7 (Miller's number), standard thresholds
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int attention_wm_bridge_default_config(attention_wm_config_t* config);

/**
 * @brief Create Attention-WM bridge
 *
 * WHAT: Initialize Attention-Working Memory integration bridge
 * WHY:  Enable attention-gated WM operations
 * HOW:  Allocate bridge, initialize item storage
 *
 * @param config Configuration (NULL for defaults)
 * @return New bridge or NULL on failure
 */
attention_wm_bridge_t* attention_wm_bridge_create(
    const attention_wm_config_t* config
);

/**
 * @brief Destroy Attention-WM bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Free item storage, clear state
 *
 * @param bridge Bridge to destroy (NULL safe)
 */
void attention_wm_bridge_destroy(attention_wm_bridge_t* bridge);

/* ============================================================================
 * Attention -> WM Direction
 * ============================================================================ */

/**
 * @brief Gate item entry into working memory
 *
 * WHAT: Use attention strength to gate WM entry
 * WHY:  Only sufficiently attended items enter WM
 * HOW:  Check attention threshold, manage capacity
 *
 * @param bridge Attention-WM bridge
 * @param item_id ID of item to gate
 * @param attention_strength Current attention on item [0, 1]
 * @return 0 on success (item gated in), -1 on error or rejection
 */
int attention_wm_gate_entry(
    attention_wm_bridge_t* bridge,
    uint64_t item_id,
    float attention_strength
);

/**
 * @brief Handle attention focus shift
 *
 * WHAT: Update WM state when attention shifts
 * WHY:  Focus shifts affect WM item priorities
 * HOW:  Boost new focus item, decay old focus item
 *
 * @param bridge Attention-WM bridge
 * @param old_focus Previous focus item ID (0 if none)
 * @param new_focus New focus item ID
 * @return 0 on success, -1 on error
 */
int attention_wm_on_focus_shift(
    attention_wm_bridge_t* bridge,
    uint64_t old_focus,
    uint64_t new_focus
);

/* ============================================================================
 * WM Management
 * ============================================================================ */

/**
 * @brief Update priority of a WM item
 *
 * WHAT: Change the priority of an item in WM
 * WHY:  Priority determines retention vs eviction
 * HOW:  Update priority, potentially trigger eviction
 *
 * @param bridge Attention-WM bridge
 * @param item_id ID of item to update
 * @param new_priority New priority value [0, 1]
 * @return 0 on success, -1 on error
 */
int attention_wm_update_priority(
    attention_wm_bridge_t* bridge,
    uint64_t item_id,
    float new_priority
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get items above attention threshold
 *
 * WHAT: Query WM items currently being attended
 * WHY:  Identify active WM contents
 * HOW:  Filter items by priority/attention threshold
 *
 * @param bridge Attention-WM bridge
 * @param items Output array of attended items
 * @param max_count Maximum number of results
 * @return Number of items found, -1 on error
 */
int attention_wm_get_attended_items(
    attention_wm_bridge_t* bridge,
    attention_wm_item_t items[],
    size_t max_count
);

/* ============================================================================
 * Stats API
 * ============================================================================ */

/**
 * @brief Get bridge statistics
 *
 * @param bridge Attention-WM bridge
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int attention_wm_bridge_get_stats(
    const attention_wm_bridge_t* bridge,
    attention_wm_stats_t* stats
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_ATTENTION_WM_BRIDGE_H */
