/**
 * @file nimcp_working_memory.h
 * @brief Working memory system with capacity limits (Miller's 7±2)
 *
 * WHAT: Implements limited-capacity buffer for active cognitive representations
 * WHY:  Enable reasoning, planning, and sequential processing over maintained items
 * HOW:  Fixed-size buffer with salience-based eviction and temporal decay
 *
 * BIOLOGICAL BASIS:
 * - Prefrontal cortex (dlPFC) maintains ~7 items via persistent neural firing
 * - Attention refresh prevents decay (rehearsal)
 * - Salience determines retention priority under capacity pressure
 * - Items decay exponentially without attention (forgetting curve)
 *
 * THEORETICAL FOUNDATION:
 * - Miller (1956): "The Magical Number Seven, Plus or Minus Two"
 * - Baddeley & Hitch (1974): Working memory model
 * - Cowan (2001): ~4 chunks in focus of attention
 *
 * INTEGRATION POINTS:
 *
 * 1. Brain Structure (src/core/brain/nimcp_brain.c)
 *    Add to struct brain_struct:
 *      working_memory_t* working_memory;  // Phase 10.1
 *
 * 2. Configuration (src/core/brain/nimcp_brain.h)
 *    Add to brain_config_t:
 *      bool enable_working_memory;        // Phase 10.1
 *      uint32_t working_memory_capacity;  // Default: 7
 *
 * 3. Initialization (brain_create_custom())
 *    if (config->enable_working_memory) {
 *        brain->working_memory = working_memory_create(
 *            config->working_memory_capacity
 *        );
 *    }
 *
 * 4. Cleanup (brain_destroy())
 *    if (brain->working_memory) {
 *        working_memory_destroy(brain->working_memory);
 *    }
 *
 * 5. Processing Pipeline
 *    Stage: After sensory processing, before decision-making
 *    Purpose: Maintain active representations for multi-step reasoning
 *
 * DEPENDENCIES:
 * - None (standalone module)
 *
 * DEPENDENT MODULES (Phase 10):
 * - Theory of Mind: Uses working memory for belief tracking
 * - Executive Functions: Uses working memory for planning
 * - Meta-Learning: Uses working memory for task context
 *
 * PERFORMANCE:
 * - Add: O(n) where n = capacity (typically n=7, very fast)
 * - Get: O(1) direct array access
 * - Decay: O(n) where n = current_size
 * - Refresh: O(1) direct array access
 *
 * MEMORY OVERHEAD:
 * - Base: sizeof(working_memory_t) = ~200 bytes
 * - Per item: item_size × sizeof(float) + metadata = ~50-100 bytes
 * - Total (7 items): ~1KB typical
 *
 * @author NIMCP Development Team - Phase 10.1
 * @date 2025-01-09
 * @version 2.7.0 Phase 10.1
 */

#ifndef NIMCP_WORKING_MEMORY_H
#define NIMCP_WORKING_MEMORY_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "cognitive/nimcp_emotional_tagging.h"  // Phase 10.3: Emotional tagging
#include "utils/encoding/nimcp_positional_encoding.h"  // Positional encoding for serial position
#include "cognitive/nimcp_sleep_wake.h"  // Sleep state integration

/* Forward declaration for brain immune integration */
struct brain_immune_system;

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Configuration Constants
//=============================================================================

/**
 * @brief Default working memory capacity (Miller's magic number)
 */
#define WORKING_MEMORY_DEFAULT_CAPACITY 7

/**
 * @brief Maximum item capacity (safety limit)
 */
#define WORKING_MEMORY_MAX_CAPACITY 20

/**
 * @brief Maximum size of single item in floats
 */
#define WORKING_MEMORY_MAX_ITEM_SIZE 1024

/**
 * @brief Decay time constant in milliseconds
 *
 * MEANING: Time for item strength to decay to ~37% (1/e) without refresh
 * BIOLOGICAL: Matches short-term memory decay (seconds to minutes)
 */
#define WORKING_MEMORY_DECAY_TAU_MS 1000.0f

/**
 * @brief Minimum salience for item retention
 *
 * Items below this threshold are evicted during capacity enforcement
 */
#define WORKING_MEMORY_MIN_SALIENCE 0.01f

//=============================================================================
// Core Types
//=============================================================================

/**
 * @brief Working memory instance (opaque pointer)
 *
 * DESIGN: Opaque to enforce encapsulation
 * Internal structure defined in .c file
 */
typedef struct working_memory working_memory_t;
#define NIMCP_WORKING_MEMORY_T_DEFINED

/**
 * @brief Working memory configuration
 *
 * All parameters have sensible defaults via working_memory_default_config()
 */
typedef struct {
    uint32_t capacity;              /**< Max items (default: 7) */
    float decay_tau_ms;             /**< Decay time constant (default: 1000ms) */
    float min_salience;             /**< Eviction threshold (default: 0.01) */
    bool enable_attention_refresh;  /**< Enable rehearsal (default: true) */
    bool enable_temporal_decay;     /**< Enable forgetting (default: true) */
    bool enable_positional_encoding; /**< Apply position encodings (default: true) */
    nimcp_pos_encoding_type_t pe_type; /**< Type of positional encoding (default: SINUSOIDAL) */
    uint32_t pe_embedding_dim;      /**< Dimension for position embeddings (default: 64) */
    bool enable_quantum_wm;         /**< Enable quantum retrieval (default: true) */
} working_memory_config_t;

/**
 * @brief Working memory statistics
 *
 * For monitoring and debugging
 */
typedef struct {
    uint32_t capacity;              /**< Maximum capacity */
    uint32_t current_size;          /**< Current number of items */
    uint32_t total_additions;       /**< Lifetime additions */
    uint32_t total_evictions;       /**< Lifetime evictions */
    uint32_t total_refreshes;       /**< Lifetime attention refreshes */
    float avg_salience;             /**< Average salience of current items */
    float oldest_item_age_ms;       /**< Age of oldest item */
} working_memory_stats_t;

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create working memory with default configuration
 *
 * WHAT: Allocate and initialize working memory buffer
 * WHY:  Provide simple API for common case (capacity=7)
 * HOW:  Use default config, call working_memory_create_custom()
 *
 * COMPLEXITY: O(capacity) for allocation
 * MEMORY: Allocates ~1KB for default capacity
 *
 * @return Working memory instance or NULL on allocation failure
 *
 * @note Caller must free with working_memory_destroy()
 */
working_memory_t* working_memory_create(void);

/**
 * @brief Create working memory with custom configuration
 *
 * WHAT: Allocate and initialize working memory with specified parameters
 * WHY:  Allow customization of capacity, decay, etc.
 * HOW:  Validate config → Allocate structures → Initialize metadata
 *
 * COMPLEXITY: O(capacity) for allocation
 * MEMORY: capacity × (MAX_ITEM_SIZE + metadata) bytes worst case
 *
 * @param config Configuration parameters (NULL uses defaults)
 * @return Working memory instance or NULL on invalid config/allocation failure
 *
 * @note Capacity clamped to [1, WORKING_MEMORY_MAX_CAPACITY]
 */
working_memory_t* working_memory_create_custom(const working_memory_config_t* config);

/**
 * @brief Destroy working memory and free all resources
 *
 * WHAT: Free all items and metadata, then working memory structure
 * WHY:  Prevent memory leaks
 * HOW:  Free each item → Free arrays → Free structure
 *
 * COMPLEXITY: O(n) where n = current_size
 *
 * @param wm Working memory to destroy (can be NULL)
 */
void working_memory_destroy(working_memory_t* wm);

/**
 * @brief Get default working memory configuration
 *
 * WHAT: Return configuration with sensible defaults
 * WHY:  Provide starting point for customization
 * HOW:  Static initialization with constant values
 *
 * DEFAULTS:
 * - capacity: 7 (Miller's number)
 * - decay_tau_ms: 1000ms (1 second half-life)
 * - min_salience: 0.01 (1% threshold)
 * - enable_attention_refresh: true
 * - enable_temporal_decay: true
 *
 * @return Default configuration
 */
working_memory_config_t working_memory_default_config(void);

//=============================================================================
// Item Management
//=============================================================================

/**
 * @brief Add item to working memory
 *
 * WHAT: Insert new item into working memory buffer with salience
 * WHY:  Maintain active representation for reasoning/planning
 * HOW:  Validate → Evict if full → Deep copy item → Store metadata
 *
 * ALGORITHM:
 * 1. Validate all parameters (NULL checks, range checks)
 * 2. If at capacity, evict lowest-salience item
 * 3. Allocate and copy item data (deep copy)
 * 4. Store salience, timestamp, mark as not yet refreshed
 * 5. Increment current size
 *
 * COMPLEXITY: O(n) where n = capacity (eviction search)
 * MEMORY: Allocates item_size × sizeof(float) bytes
 *
 * BIOLOGICAL: Mimics prefrontal encoding of new representation
 *
 * @param wm Working memory instance (non-NULL)
 * @param item Item data array (non-NULL)
 * @param item_size Size of item in floats (> 0, <= MAX_ITEM_SIZE)
 * @param salience Importance [0.0, 1.0] for eviction priority
 * @return true on success, false on invalid parameters or allocation failure
 *
 * @note Higher salience items retained longer under capacity pressure
 * @note Caller retains ownership of item (deep copy made)
 */
bool working_memory_add(
    working_memory_t* wm,
    const float* item,
    uint32_t item_size,
    float salience
);

/**
 * @brief Add item to working memory with emotional tag (Phase 10.3)
 *
 * WHAT: Insert new item with emotional context for enhanced salience
 * WHY:  Emotional events receive memory priority (biological)
 * HOW:  Store emotional tag → Compute boosted salience → Add item
 *
 * ALGORITHM:
 * 1. Validate all parameters (including emotion)
 * 2. Compute emotional salience boost
 * 3. Store item with both base and emotional salience
 * 4. Attach emotional tag to item metadata
 *
 * COMPLEXITY: O(n) where n = capacity (eviction search)
 * MEMORY: Allocates item_size × sizeof(float) + sizeof(emotional_tag_t)
 *
 * BIOLOGICAL BASIS:
 * - Amygdala tags emotional events for enhanced hippocampal encoding
 * - High arousal emotions receive attentional priority
 * - Emotional context aids retrieval and consolidation
 *
 * SALIENCE COMPUTATION:
 * total_salience = base_salience × emotional_boost
 * where emotional_boost = 1.0 + (arousal × 0.5) + (|valence| × 0.3)
 *
 * @param wm Working memory instance (non-NULL)
 * @param item Item data array (non-NULL)
 * @param item_size Size of item in floats (> 0, <= MAX_ITEM_SIZE)
 * @param base_salience Base importance [0.0, 1.0]
 * @param emotion Emotional tag (non-NULL)
 * @return true on success, false on invalid parameters or allocation failure
 *
 * @note Emotional boost can increase effective salience up to ~1.8×
 * @note Caller retains ownership of emotion (deep copy made)
 */
bool working_memory_add_with_emotion(
    working_memory_t* wm,
    const float* item,
    uint32_t item_size,
    float base_salience,
    const emotional_tag_t* emotion
);

/**
 * @brief Get item from working memory by index
 *
 * WHAT: Retrieve pointer to item at specified index
 * WHY:  Access active representations for processing
 * HOW:  Validate index → Return pointer
 *
 * COMPLEXITY: O(1) direct array access
 *
 * @param wm Working memory instance (non-NULL)
 * @param index Item index [0, current_size)
 * @param size Output: size of item in floats (can be NULL)
 * @return Pointer to item data or NULL if invalid index
 *
 * @note Returned pointer is READ-ONLY, do NOT modify
 * @note Pointer valid until next add/decay/destroy operation
 */
const float* working_memory_get(
    working_memory_t* wm,
    uint32_t index,
    uint32_t* size
);

/**
 * @brief Remove item from working memory by index
 *
 * WHAT: Delete item at specified index
 * WHY:  Explicit removal when item no longer needed
 * HOW:  Validate → Free item data → Shift remaining items
 *
 * COMPLEXITY: O(n) where n = current_size (shift operation)
 *
 * @param wm Working memory instance (non-NULL)
 * @param index Item index [0, current_size)
 * @return true on success, false on invalid index
 */
bool working_memory_remove(working_memory_t* wm, uint32_t index);

/**
 * @brief Clear all items from working memory
 *
 * WHAT: Remove all items, reset to empty state
 * WHY:  Task switching, context reset
 * HOW:  Free all items → Reset counters
 *
 * COMPLEXITY: O(n) where n = current_size
 *
 * @param wm Working memory instance (non-NULL)
 */
void working_memory_clear(working_memory_t* wm);

/**
 * @brief Get current number of items in working memory
 *
 * WHAT: Return count of currently stored items
 * WHY:  Check occupancy, iteration
 * HOW:  Return current_size field
 *
 * COMPLEXITY: O(1)
 *
 * @param wm Working memory instance (non-NULL)
 * @return Number of items [0, capacity]
 */
uint32_t working_memory_get_size(const working_memory_t* wm);

/**
 * @brief Get current number of items (alias for get_size)
 *
 * WHAT: Return count of currently stored items
 * WHY:  Provide alternative naming for consistency with other APIs
 * HOW:  Return current_size field
 *
 * COMPLEXITY: O(1)
 *
 * @param wm Working memory instance (non-NULL)
 * @return Number of items [0, capacity]
 */
uint32_t working_memory_get_count(const working_memory_t* wm);

/**
 * @brief Get working memory utilization percentage
 *
 * WHAT: Return percentage of capacity currently in use
 * WHY:  Monitor memory pressure and capacity usage
 * HOW:  Return (current_size / capacity) as float [0.0, 1.0]
 *
 * COMPLEXITY: O(1)
 *
 * @param wm Working memory instance (non-NULL)
 * @return Utilization [0.0, 1.0] where 1.0 = full
 */
float working_memory_get_utilization(working_memory_t* wm);

/**
 * @brief Get maximum capacity of working memory
 *
 * WHAT: Return maximum number of items that can be stored
 * WHY:  Check limits before adding
 * HOW:  Return capacity field
 *
 * COMPLEXITY: O(1)
 *
 * @param wm Working memory instance (non-NULL)
 * @return Capacity (typically 7)
 */
uint32_t working_memory_get_capacity(working_memory_t* wm);

//=============================================================================
// Salience & Attention
//=============================================================================

/**
 * @brief Refresh item with attention to prevent decay
 *
 * WHAT: Mark item as attended, reset decay
 * WHY:  Rehearsal maintains items in working memory (biological)
 * HOW:  Update timestamp, set refreshed flag
 *
 * COMPLEXITY: O(1)
 *
 * BIOLOGICAL BASIS:
 * - Attention refresh = neural rehearsal
 * - Prevents decay (like repeating phone number to remember it)
 * - Prefrontal persistent firing maintained by attention
 *
 * @param wm Working memory instance (non-NULL)
 * @param index Item index [0, current_size)
 * @return true on success, false on invalid index
 */
bool working_memory_refresh(working_memory_t* wm, uint32_t index);

/**
 * @brief Update salience of existing item
 *
 * WHAT: Change importance score of item
 * WHY:  Dynamic prioritization (item became more/less important)
 * HOW:  Validate → Update salience field
 *
 * COMPLEXITY: O(1)
 *
 * @param wm Working memory instance (non-NULL)
 * @param index Item index [0, current_size)
 * @param new_salience New importance [0.0, 1.0]
 * @return true on success, false on invalid parameters
 */
bool working_memory_set_salience(
    working_memory_t* wm,
    uint32_t index,
    float new_salience
);

/**
 * @brief Get salience of item
 *
 * WHAT: Retrieve importance score
 * WHY:  Check priority for processing decisions
 * HOW:  Return salience field
 *
 * COMPLEXITY: O(1)
 *
 * @param wm Working memory instance (non-NULL)
 * @param index Item index [0, current_size)
 * @param salience Output: salience value (non-NULL)
 * @return true on success, false on invalid index
 */
bool working_memory_get_salience(
    const working_memory_t* wm,
    uint32_t index,
    float* salience
);

/**
 * @brief Get emotional tag of item (Phase 10.3)
 *
 * WHAT: Retrieve emotional context attached to working memory item
 * WHY:  Access emotional state for decision-making and memory retrieval
 * HOW:  Validate → Copy emotional tag to output
 *
 * COMPLEXITY: O(1)
 *
 * @param wm Working memory instance (non-NULL)
 * @param index Item index [0, current_size)
 * @param emotion Output: emotional tag (non-NULL)
 * @return true on success, false on invalid index or no emotion attached
 *
 * @note Returns neutral emotion if item has no emotional tag
 */
bool working_memory_get_emotion(
    const working_memory_t* wm,
    uint32_t index,
    emotional_tag_t* emotion
);

/**
 * @brief Get total salience including emotional boost (Phase 10.3)
 *
 * WHAT: Compute effective salience with emotional enhancement
 * WHY:  Priority decisions should consider emotional salience
 * HOW:  Retrieve base salience → Apply emotional boost → Return total
 *
 * FORMULA:
 * total_salience = base_salience × emotional_boost
 * where emotional_boost = emotional_compute_salience_boost(emotion)
 *
 * COMPLEXITY: O(1)
 *
 * @param wm Working memory instance (non-NULL)
 * @param index Item index [0, current_size)
 * @param total_salience Output: total salience value (non-NULL)
 * @return true on success, false on invalid index
 *
 * @note If item has no emotion, returns base salience (boost = 1.0)
 */
bool working_memory_get_total_salience(
    const working_memory_t* wm,
    uint32_t index,
    float* total_salience
);

//=============================================================================
// Temporal Dynamics
//=============================================================================

/**
 * @brief Apply temporal decay to all items
 *
 * WHAT: Reduce salience of items based on time since last refresh
 * WHY:  Model forgetting (biological decay of activation)
 * HOW:  For each item: salience *= exp(-age / tau)
 *
 * ALGORITHM:
 * ```
 * for each item:
 *     age = current_time - timestamp
 *     if not attention_refreshed:
 *         decay_factor = exp(-age / decay_tau_ms)
 *         salience *= decay_factor
 *     if salience < min_salience:
 *         evict item
 * ```
 *
 * COMPLEXITY: O(n) where n = current_size
 *
 * BIOLOGICAL BASIS:
 * - Exponential decay matches neural activation decay
 * - Tau = 1000ms gives half-life of ~700ms
 * - Attention refresh prevents decay (rehearsal)
 *
 * @param wm Working memory instance (non-NULL)
 * @param current_time_ms Current timestamp in milliseconds
 * @return Number of items evicted due to decay
 *
 * @note Should be called periodically (e.g., every 100-500ms)
 * @note Items with attention_refreshed=true are not decayed
 */
uint32_t working_memory_decay(
    working_memory_t* wm,
    uint64_t current_time_ms
);

/**
 * @brief Get age of item in milliseconds
 *
 * WHAT: Calculate time since item was added/refreshed
 * WHY:  Monitor staleness of representations
 * HOW:  current_time - timestamp
 *
 * COMPLEXITY: O(1)
 *
 * @param wm Working memory instance (non-NULL)
 * @param index Item index [0, current_size)
 * @param current_time_ms Current timestamp
 * @param age_ms Output: age in milliseconds (non-NULL)
 * @return true on success, false on invalid index
 */
bool working_memory_get_age(
    const working_memory_t* wm,
    uint32_t index,
    uint64_t current_time_ms,
    uint64_t* age_ms
);

//=============================================================================
// Statistics & Monitoring
//=============================================================================

/**
 * @brief Get working memory statistics
 *
 * WHAT: Retrieve operational metrics
 * WHY:  Monitoring, debugging, performance analysis
 * HOW:  Populate stats structure from internal state
 *
 * COMPLEXITY: O(n) to calculate avg_salience and oldest_item_age
 *
 * @param wm Working memory instance (non-NULL)
 * @param stats Output: statistics structure (non-NULL)
 */
void working_memory_get_stats(
    const working_memory_t* wm,
    working_memory_stats_t* stats
);

/**
 * @brief Reset working memory statistics counters
 *
 * WHAT: Zero lifetime counters (additions, evictions, refreshes)
 * WHY:  Start fresh measurement period
 * HOW:  Set counters to zero, preserve current items
 *
 * COMPLEXITY: O(1)
 *
 * @param wm Working memory instance (non-NULL)
 *
 * @note Does NOT clear items, only resets counters
 */
void working_memory_reset_stats(working_memory_t* wm);

//=============================================================================
// Brain Immune Integration
//=============================================================================

/**
 * @brief Connect working memory to brain immune system
 *
 * WHAT: Establish bidirectional integration with brain immune system
 * WHY:  Inflammation impairs working memory; WM stress triggers immune response
 * HOW:  Register callback for inflammation events, enable cytokine release
 *
 * BIOLOGICAL BASIS:
 * - Pro-inflammatory cytokines (IL-6, TNF-alpha) impair prefrontal cortex function
 * - Inflammation reduces working memory capacity during illness/stress
 * - Working memory overload triggers stress cytokine release
 * - Recovery allows capacity restoration
 *
 * INTEGRATION MECHANISMS:
 * 1. IMMUNE → WM: Inflammation reduces effective capacity
 *    - LOCAL: -1 item capacity (6 items)
 *    - REGIONAL: -2 items capacity (5 items)
 *    - SYSTEMIC: -3 items capacity (4 items)
 *    - STORM: -4 items capacity (3 items minimum)
 *
 * 2. WM → IMMUNE: Working memory stress triggers cytokine release
 *    - Capacity >90% utilization: IL-6 release (cognitive load)
 *    - Eviction events: TNF-alpha release (failure signal)
 *    - Decay removal: IL-1 release (resource scarcity)
 *
 * COMPLEXITY: O(1) for connection setup
 *
 * @param wm Working memory instance (non-NULL)
 * @param immune Brain immune system (non-NULL)
 * @return true on success, false on invalid parameters
 *
 * @note Immune system must be running (brain_immune_start called)
 * @note Callbacks registered for inflammation state changes
 */
bool working_memory_connect_immune(
    working_memory_t* wm,
    struct brain_immune_system* immune
);

/**
 * @brief Disconnect working memory from brain immune system
 *
 * WHAT: Remove immune integration
 * WHY:  Clean shutdown or disable immune modulation
 * HOW:  Clear immune pointer, restore full capacity
 *
 * COMPLEXITY: O(1)
 *
 * @param wm Working memory instance (non-NULL)
 */
void working_memory_disconnect_immune(working_memory_t* wm);

/**
 * @brief Get effective capacity accounting for immune state
 *
 * WHAT: Return current capacity after inflammation penalties
 * WHY:  Check available slots considering immune impairment
 * HOW:  base_capacity - inflammation_penalty
 *
 * COMPLEXITY: O(1)
 *
 * @param wm Working memory instance (non-NULL)
 * @return Effective capacity (minimum 3 items, even under cytokine storm)
 *
 * @note Returns base capacity if no immune connection
 */
uint32_t working_memory_get_effective_capacity(const working_memory_t* wm);

/**
 * @brief Check if working memory is under immune impairment
 *
 * WHAT: Determine if inflammation is reducing capacity
 * WHY:  Detect cognitive impairment due to immune response
 * HOW:  Check if effective_capacity < base_capacity
 *
 * COMPLEXITY: O(1)
 *
 * @param wm Working memory instance (non-NULL)
 * @return true if capacity reduced by inflammation
 */
bool working_memory_is_immune_impaired(const working_memory_t* wm);

/**
 * @brief Trigger immune stress response from working memory overload
 *
 * WHAT: Manually signal immune system about cognitive stress
 * WHY:  Allow explicit stress signaling for testing or special conditions
 * HOW:  Release IL-6 cytokine at specified concentration
 *
 * BIOLOGICAL BASIS:
 * Working memory overload activates HPA axis → cortisol release →
 * immune activation with pro-inflammatory cytokines
 *
 * COMPLEXITY: O(1)
 *
 * @param wm Working memory instance (non-NULL)
 * @param stress_level Stress intensity [0.0, 1.0]
 * @return true on success, false if no immune connection
 *
 * @note Automatically called internally on high utilization/evictions
 */
bool working_memory_signal_stress(
    working_memory_t* wm,
    float stress_level
);

//=============================================================================
// Advanced Operations
//=============================================================================

/**
 * @brief Find item with highest salience
 *
 * WHAT: Return index of most important item
 * WHY:  Attention focus, priority processing
 * HOW:  Linear search for max salience
 *
 * COMPLEXITY: O(n) where n = current_size
 *
 * @param wm Working memory instance (non-NULL)
 * @param salience Output: salience of top item (can be NULL)
 * @return Index of highest-salience item or -1 if empty
 */
int working_memory_find_highest_salience(
    working_memory_t* wm,
    float* salience
);

/**
 * @brief Find item with lowest salience
 *
 * WHAT: Return index of least important item
 * WHY:  Eviction candidate identification
 * HOW:  Linear search for min salience
 *
 * COMPLEXITY: O(n) where n = current_size
 *
 * @param wm Working memory instance (non-NULL)
 * @param salience Output: salience of bottom item (can be NULL)
 * @return Index of lowest-salience item or -1 if empty
 */
int working_memory_find_lowest_salience(
    working_memory_t* wm,
    float* salience
);

/**
 * @brief Check if working memory is full
 *
 * WHAT: Test if at capacity
 * WHY:  Avoid allocation before checking if eviction needed
 * HOW:  Compare current_size to capacity
 *
 * COMPLEXITY: O(1)
 *
 * @param wm Working memory instance (non-NULL)
 * @return true if current_size == capacity
 */
bool working_memory_is_full(working_memory_t* wm);

/**
 * @brief Check if working memory is empty
 *
 * WHAT: Test if no items stored
 * WHY:  Guard against operations on empty buffer
 * HOW:  Check current_size == 0
 *
 * COMPLEXITY: O(1)
 *
 * @param wm Working memory instance (non-NULL)
 * @return true if current_size == 0
 */
bool working_memory_is_empty(const working_memory_t* wm);

//=============================================================================
// Sleep State Integration
//=============================================================================

/**
 * @brief Set sleep state for working memory modulation
 *
 * WHAT: Update current sleep state to modulate WM capacity and decay
 * WHY:  Sleep state affects working memory performance (biological)
 * HOW:  Store state, apply modulation from sleep bridge
 *
 * BIOLOGICAL BASIS:
 * - AWAKE: Full WM capacity (~7±2 items), normal decay
 * - DROWSY: Reduced capacity (~5 items), faster decay (1.5x)
 * - LIGHT_NREM: Minimal capacity (~2 items), faster decay (2x)
 * - DEEP_NREM: Offline (0 items), no decay (consolidation)
 * - REM: Limited capacity (~3 items), moderate decay (1.2x)
 *
 * COMPLEXITY: O(1)
 *
 * @param wm Working memory instance (non-NULL)
 * @param state New sleep state
 * @return true on success, false on NULL pointer
 */
bool working_memory_set_sleep_state(working_memory_t* wm, sleep_state_t state);

/**
 * @brief Get current sleep state
 *
 * WHAT: Query current sleep/wake state
 * WHY:  Check what modulation is being applied
 * HOW:  Return current_sleep_state field
 *
 * COMPLEXITY: O(1)
 *
 * @param wm Working memory instance (non-NULL)
 * @return Current sleep state, or SLEEP_STATE_AWAKE if NULL
 */
sleep_state_t working_memory_get_sleep_state(working_memory_t* wm);

//=============================================================================
// Positional Encoding Functions
//=============================================================================

/**
 * @brief Apply positional encodings to all items in working memory
 *
 * WHAT: Add position embeddings to each item based on its slot position
 * WHY:  Capture serial position effects (primacy, recency) in working memory
 * HOW:  For each item at position i, apply PE(i) using configured encoder
 *
 * BIOLOGICAL BASIS:
 * - Serial position effects in working memory (Ebbinghaus, 1885)
 * - Primacy effect: better recall of early items (position 0-2)
 * - Recency effect: better recall of recent items (position 5-6)
 * - Prefrontal cortex encodes temporal order of active representations
 * - Position information aids retrieval and manipulation of memory items
 *
 * COMPLEXITY: O(n × d) where n = current_size, d = embedding_dim
 *
 * @param wm Working memory instance (non-NULL)
 * @return true on success, false if positional encoding disabled or error
 *
 * @note Only applies to items currently in buffer
 * @note Encodings are additive: item_data[j] += PE(position)[j]
 */
bool working_memory_encode_positions(working_memory_t* wm);

/**
 * @brief Get positional embedding for specific slot
 *
 * WHAT: Retrieve position encoding vector for a working memory slot
 * WHY:  Inspect position information, external position-aware processing
 * HOW:  Query internal positional encoder for slot's position encoding
 *
 * COMPLEXITY: O(1) if cached, O(d) if computed where d = embedding_dim
 *
 * BIOLOGICAL BASIS:
 * - Position codes in prefrontal working memory representations
 * - Enables comparison of relative positions between items
 * - Supports temporal reasoning over working memory contents
 *
 * @param wm Working memory instance (non-NULL)
 * @param slot_index Slot position [0, capacity)
 * @param output Output buffer for position embedding (pe_embedding_dim floats)
 * @return true on success, false on invalid slot or PE disabled
 *
 * @note Returns encoding for slot position, not item age
 * @note Output must be pre-allocated with pe_embedding_dim floats
 */
bool working_memory_get_position_embedding(
    const working_memory_t* wm,
    uint32_t slot_index,
    float* output
);

/**
 * @brief Configure positional encoding type
 *
 * WHAT: Change the type of positional encoding used
 * WHY:  Allow runtime switching between encoding strategies
 * HOW:  Destroy old encoder, create new encoder with specified type
 *
 * ENCODING TYPES:
 * - NIMCP_POS_SINUSOIDAL: Fixed sin/cos patterns (no training, extrapolates)
 * - NIMCP_POS_RELATIVE: Relative position embeddings (for distance-based reasoning)
 * - NIMCP_POS_LEARNED: Trainable embeddings (task-specific)
 * - NIMCP_POS_ROTARY: Rotation-based (RoPE, best for long sequences)
 * - NIMCP_POS_ALIBI: Linear attention bias (simplest)
 *
 * COMPLEXITY: O(capacity × embedding_dim) - must rebuild encoder cache
 *
 * @param wm Working memory instance (non-NULL)
 * @param pe_type New positional encoding type
 * @return true on success, false on invalid type or allocation failure
 *
 * @note Reapplies encodings to existing items after switch
 * @note Thread-safe: locks mutex during encoder swap
 */
bool working_memory_set_pe_type(
    working_memory_t* wm,
    nimcp_pos_encoding_type_t pe_type
);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_WORKING_MEMORY_H
