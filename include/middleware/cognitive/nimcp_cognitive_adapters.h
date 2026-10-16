//=============================================================================
// nimcp_cognitive_adapters.h - Cognitive Layer Middleware Adapters
//=============================================================================

#ifndef NIMCP_COGNITIVE_ADAPTERS_H
#define NIMCP_COGNITIVE_ADAPTERS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Middleware components
#include "middleware/buffering/nimcp_sliding_window.h"
#include "middleware/buffering/nimcp_integration_buffer.h"
#include "middleware/buffering/nimcp_temporal_accumulator.h"
#include "middleware/routing/nimcp_attention_gate.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file nimcp_cognitive_adapters.h
 * @brief Middleware adapters for cognitive layer integration
 *
 * WHAT: Adapters connecting cognitive modules to middleware infrastructure
 * WHY:  Bridge cognitive processes with buffering, routing, and attention
 * HOW:  Specialized adapters for working memory, consolidation, and attention
 *
 * BIOLOGICAL BASIS:
 * - Working memory: Prefrontal cortex maintains active representations (7±2 items)
 * - Consolidation: Hippocampal replay transfers episodic to semantic memory
 * - Attention: Thalamic gating and cortical amplification of relevant signals
 *
 * DESIGN PRINCIPLES:
 * - Single Responsibility: Each adapter handles one cognitive function
 * - Composability: Adapters can be combined for complex processing
 * - Efficiency: Minimal overhead, direct middleware access
 * - Transparency: Clear mapping between cognitive concepts and middleware
 *
 * INTEGRATION ARCHITECTURE:
 *
 *   Cognitive Layer              Middleware Layer
 *   ================             =================
 *   Working Memory    <--->      Sliding Window + Attention Gate
 *   Consolidation     <--->      Integration Buffer + Accumulator
 *   Attention         <--->      Attention Gate + Pattern Detection
 *
 * @author NIMCP Development Team
 * @version 1.0.0
 * @date 2025-11-20
 */

//=============================================================================
// CONSTANTS
//=============================================================================

/** Working memory capacity (Miller's 7±2 law) */
#define COGNITIVE_WM_DEFAULT_CAPACITY 9

/** Consolidation window sizes (milliseconds) */
#define COGNITIVE_CONSOL_FAST_WINDOW_MS 100
#define COGNITIVE_CONSOL_MEDIUM_WINDOW_MS 1000
#define COGNITIVE_CONSOL_SLOW_WINDOW_MS 10000

/** Attention spotlight size */
#define COGNITIVE_ATTENTION_SPOTLIGHT_SIZE 8

/** Pattern detection threshold */
#define COGNITIVE_PATTERN_THRESHOLD 0.7f

/** Maximum adapter channels */
#define COGNITIVE_MAX_CHANNELS 256

//=============================================================================
// WORKING MEMORY ADAPTER
//=============================================================================

/**
 * @brief Working memory buffer modes
 *
 * WHAT: How working memory processes incoming data
 * WHY:  Different cognitive tasks require different buffering strategies
 */
typedef enum {
    WM_MODE_SLIDING,        /**< Sliding window over recent data */
    WM_MODE_ATTENTION_GATED, /**< Attention-weighted buffering */
    WM_MODE_HYBRID          /**< Combined sliding + attention */
} wm_buffer_mode_t;

/**
 * @brief Working memory item
 *
 * WHAT: Single item in working memory buffer
 * WHY:  Track salience, recency, and content
 */
typedef struct {
    uint32_t item_id;           /**< Unique item identifier */
    float* data;                /**< Item data (owned by adapter) */
    size_t data_size;           /**< Data element count */
    float salience;             /**< Item salience [0.0, 1.0] */
    uint64_t timestamp_us;      /**< When item entered WM */
    bool is_active;             /**< Whether item is active */
} wm_item_t;

/**
 * @brief Working memory adapter configuration
 */
typedef struct {
    wm_buffer_mode_t mode;      /**< Buffering mode */
    uint32_t capacity;          /**< Maximum items (default: 9) */
    size_t window_size;         /**< Sliding window size */
    float attention_threshold;  /**< Minimum attention for storage */
    bool enable_decay;          /**< Enable temporal decay */
    float decay_tau_ms;         /**< Decay time constant (ms) */
} wm_adapter_config_t;

/**
 * @brief Working memory adapter statistics
 */
typedef struct {
    uint32_t current_items;     /**< Current WM item count */
    uint32_t total_added;       /**< Total items added */
    uint32_t total_evicted;     /**< Total items evicted */
    float avg_salience;         /**< Average item salience */
    float capacity_utilization; /**< Percentage of capacity used */
} wm_adapter_stats_t;

/**
 * @brief Opaque working memory adapter handle
 */
typedef struct wm_adapter wm_adapter_t;

/**
 * @brief Create working memory adapter
 *
 * WHAT: Initialize WM adapter with middleware components
 * WHY:  Provide working memory buffering with attention gating
 * HOW:  Allocate adapter, create sliding window and attention gate
 *
 * @param config Configuration (NULL for defaults)
 * @return Adapter handle or NULL on failure
 */
wm_adapter_t* wm_adapter_create(const wm_adapter_config_t* config);

/**
 * @brief Destroy working memory adapter
 *
 * WHAT: Free all adapter resources
 * WHY:  Prevent memory leaks
 * HOW:  Free items, destroy middleware components, free adapter
 */
void wm_adapter_destroy(wm_adapter_t* adapter);

/**
 * @brief Add item to working memory
 *
 * WHAT: Store item in WM with attention gating
 * WHY:  Maintain limited-capacity buffer of important items
 * HOW:  Check capacity → Apply attention gate → Evict if needed → Store
 *
 * ALGORITHM:
 * 1. Calculate item salience (attention weight)
 * 2. If salience < threshold, reject
 * 3. If at capacity, evict lowest-salience item
 * 4. Store item in buffer
 * 5. Update attention weights
 *
 * @param adapter Adapter handle
 * @param item_id Item identifier
 * @param data Item data (copied)
 * @param data_size Data element count
 * @param salience Initial salience [0.0, 1.0]
 * @return true on success, false on error
 */
bool wm_adapter_add_item(wm_adapter_t* adapter,
                          uint32_t item_id,
                          const float* data,
                          size_t data_size,
                          float salience);

/**
 * @brief Update item attention weight
 *
 * WHAT: Modulate item salience via attention
 * WHY:  Top-down attention control
 * HOW:  Set attention gate weight, recompute salience
 */
bool wm_adapter_set_attention(wm_adapter_t* adapter,
                                uint32_t item_id,
                                float attention_weight);

/**
 * @brief Get item from working memory
 *
 * WHAT: Retrieve item by ID
 * WHY:  Access WM contents
 * HOW:  Find item, return pointer
 *
 * @return Pointer to item or NULL if not found
 * @note Pointer valid until next add/remove
 */
const wm_item_t* wm_adapter_get_item(const wm_adapter_t* adapter,
                                      uint32_t item_id);

/**
 * @brief Get all active items
 *
 * WHAT: Retrieve all items in WM
 * WHY:  Iterate over WM contents
 * HOW:  Copy item pointers to array
 *
 * @param items Output array (must hold capacity items)
 * @param max_items Size of output array
 * @return Number of items returned
 */
uint32_t wm_adapter_get_all_items(const wm_adapter_t* adapter,
                                   const wm_item_t** items,
                                   uint32_t max_items);

/**
 * @brief Remove item from working memory
 *
 * WHAT: Delete item by ID
 * WHY:  Explicitly clear resolved items
 * HOW:  Find item → Free data → Remove from buffer
 */
bool wm_adapter_remove_item(wm_adapter_t* adapter, uint32_t item_id);

/**
 * @brief Clear all items
 *
 * WHAT: Empty working memory
 * WHY:  Reset on context switch
 * HOW:  Free all items, reset buffer
 */
void wm_adapter_clear(wm_adapter_t* adapter);

/**
 * @brief Update temporal decay
 *
 * WHAT: Apply time-based salience decay
 * WHY:  Model WM forgetting
 * HOW:  For each item: salience *= exp(-dt/tau)
 *
 * @param adapter Adapter handle
 * @param dt Time step (microseconds)
 */
void wm_adapter_update_decay(wm_adapter_t* adapter, uint64_t dt);

/**
 * @brief Get adapter statistics
 */
bool wm_adapter_get_stats(const wm_adapter_t* adapter,
                           wm_adapter_stats_t* stats);

/**
 * @brief Get default configuration
 */
wm_adapter_config_t wm_adapter_default_config(void);

//=============================================================================
// CONSOLIDATION ADAPTER
//=============================================================================

/**
 * @brief Consolidation strategies
 *
 * WHAT: How data is integrated across timescales
 * WHY:  Different cognitive tasks need different consolidation
 */
typedef enum {
    CONSOL_STRATEGY_AVERAGE,    /**< Simple averaging */
    CONSOL_STRATEGY_WEIGHTED,   /**< Weighted by salience */
    CONSOL_STRATEGY_THRESHOLD,  /**< Threshold-based */
    CONSOL_STRATEGY_ADAPTIVE    /**< Adaptive based on variance */
} consolidation_strategy_t;

/**
 * @brief Consolidation adapter configuration
 */
typedef struct {
    consolidation_strategy_t strategy;  /**< Consolidation strategy */
    size_t fast_size;           /**< Fast buffer size */
    size_t medium_size;         /**< Medium buffer size */
    size_t slow_size;           /**< Slow buffer size */
    size_t num_channels;        /**< Number of channels */
    float alpha;                /**< Accumulator smoothing factor */
    bool enable_normalization;  /**< Normalize consolidated output */
} consol_adapter_config_t;

/**
 * @brief Consolidation adapter statistics
 */
typedef struct {
    uint64_t total_updates;     /**< Total consolidation updates */
    float fast_activity;        /**< Fast timescale activity */
    float medium_activity;      /**< Medium timescale activity */
    float slow_activity;        /**< Slow timescale activity */
    float integration_quality;  /**< Quality metric [0.0, 1.0] */
} consol_adapter_stats_t;

/**
 * @brief Opaque consolidation adapter handle
 */
typedef struct consol_adapter consol_adapter_t;

/**
 * @brief Create consolidation adapter
 *
 * WHAT: Initialize multi-timescale consolidation
 * WHY:  Integrate data across temporal scales
 * HOW:  Allocate adapter, create integration buffer and accumulator
 *
 * @param config Configuration (NULL for defaults)
 * @return Adapter handle or NULL on failure
 */
consol_adapter_t* consol_adapter_create(const consol_adapter_config_t* config);

/**
 * @brief Destroy consolidation adapter
 */
void consol_adapter_destroy(consol_adapter_t* adapter);

/**
 * @brief Update consolidation with new data
 *
 * WHAT: Add data to multi-timescale buffers
 * WHY:  Maintain temporal integration
 * HOW:  Push to integration buffer → Update accumulators → Consolidate
 *
 * ALGORITHM:
 * 1. Add data to fast buffer
 * 2. Propagate to medium/slow buffers (downsampling)
 * 3. Update temporal accumulators
 * 4. Apply consolidation strategy
 * 5. Normalize if enabled
 *
 * @param adapter Adapter handle
 * @param channel Channel index
 * @param value Sample value
 * @param timestamp Sample timestamp
 * @return true on success
 */
bool consol_adapter_update(consol_adapter_t* adapter,
                            size_t channel,
                            float value,
                            uint64_t timestamp);

/**
 * @brief Get consolidated value
 *
 * WHAT: Retrieve integrated value at timescale
 * WHY:  Access multi-scale representation
 * HOW:  Read from integration buffer or accumulator
 *
 * @param adapter Adapter handle
 * @param level Timescale level (FAST/MEDIUM/SLOW)
 * @param channel Channel index
 * @return Consolidated value
 */
float consol_adapter_get_value(const consol_adapter_t* adapter,
                                 timescale_level_t level,
                                 size_t channel);

/**
 * @brief Get consolidation across all timescales
 *
 * WHAT: Compute weighted integration of all levels
 * WHY:  Single consolidated representation
 * HOW:  Weighted average of fast/medium/slow values
 *
 * @param adapter Adapter handle
 * @param channel Channel index
 * @return Consolidated value across timescales
 */
float consol_adapter_get_consolidated(const consol_adapter_t* adapter,
                                        size_t channel);

/**
 * @brief Get temporal trend
 *
 * WHAT: Calculate trend across timescales
 * WHY:  Detect gradual changes
 * HOW:  Return (slow - fast) normalized
 */
float consol_adapter_get_trend(const consol_adapter_t* adapter,
                                 size_t channel);

/**
 * @brief Normalize channel data
 *
 * WHAT: Apply normalization to consolidated values
 * WHY:  Stabilize learning and integration
 * HOW:  Z-score normalization using window statistics
 *
 * @param adapter Adapter handle
 * @param channel Channel index
 * @return Normalized value
 */
float consol_adapter_normalize(const consol_adapter_t* adapter,
                                 size_t channel);

/**
 * @brief Clear consolidation buffers
 */
void consol_adapter_clear(consol_adapter_t* adapter);

/**
 * @brief Get adapter statistics
 */
bool consol_adapter_get_stats(const consol_adapter_t* adapter,
                               consol_adapter_stats_t* stats);

/**
 * @brief Get default configuration
 */
consol_adapter_config_t consol_adapter_default_config(void);

//=============================================================================
// ATTENTION ADAPTER
//=============================================================================

/**
 * @brief Attention control modes
 *
 * WHAT: Source of attention control signals
 * WHY:  Different tasks use different attention mechanisms
 */
typedef enum {
    ATTENTION_CONTROL_TOPDOWN,      /**< Executive control */
    ATTENTION_CONTROL_BOTTOMUP,     /**< Salience-driven */
    ATTENTION_CONTROL_MIXED,        /**< Combined */
    ATTENTION_CONTROL_LEARNED       /**< Learned attention policy */
} attention_control_mode_t;

/**
 * @brief Attention pattern
 *
 * WHAT: Detected attention pattern
 * WHY:  Track attention shifts and focus patterns
 */
typedef struct {
    uint32_t pattern_id;        /**< Pattern identifier */
    uint32_t* target_ids;       /**< Attended targets (owned) */
    uint32_t num_targets;       /**< Number of targets */
    float coherence;            /**< Pattern coherence [0.0, 1.0] */
    uint64_t timestamp_us;      /**< When pattern detected */
} attention_pattern_t;

/**
 * @brief Attention adapter configuration
 */
typedef struct {
    attention_control_mode_t mode;  /**< Control mode */
    uint32_t max_targets;       /**< Maximum targets */
    uint32_t spotlight_size;    /**< Attention spotlight size */
    bool enable_wta;            /**< Enable winner-take-all */
    bool enable_pattern_detection; /**< Detect attention patterns */
    float pattern_threshold;    /**< Pattern detection threshold */
} attention_adapter_config_t;

/**
 * @brief Attention adapter statistics
 */
typedef struct {
    uint32_t active_targets;    /**< Current active targets */
    uint32_t total_shifts;      /**< Total attention shifts */
    uint32_t patterns_detected; /**< Patterns detected */
    float avg_spotlight_size;   /**< Average spotlight size */
    float attention_stability;  /**< Stability metric [0.0, 1.0] */
} attention_adapter_stats_t;

/**
 * @brief Opaque attention adapter handle
 */
typedef struct attention_adapter attention_adapter_t;

/**
 * @brief Create attention adapter
 *
 * WHAT: Initialize attention routing system
 * WHY:  Provide attention-based signal modulation
 * HOW:  Allocate adapter, create attention gate
 *
 * @param config Configuration (NULL for defaults)
 * @return Adapter handle or NULL on failure
 */
attention_adapter_t* attention_adapter_create(
    const attention_adapter_config_t* config
);

/**
 * @brief Destroy attention adapter
 */
void attention_adapter_destroy(attention_adapter_t* adapter);

/**
 * @brief Set attention weight
 *
 * WHAT: Control attention to target
 * WHY:  Top-down attention modulation
 * HOW:  Set gate weight, update spotlight
 *
 * @param adapter Adapter handle
 * @param source_id Source identifier (for routing)
 * @param target_id Target identifier
 * @param weight Attention weight [0.0, 1.0]
 * @return true on success
 */
bool attention_adapter_set_weight(attention_adapter_t* adapter,
                                   uint32_t source_id,
                                   uint32_t target_id,
                                   float weight);

/**
 * @brief Update bottom-up salience
 *
 * WHAT: Set salience-driven attention component
 * WHY:  Capture stimulus-driven attention
 * HOW:  Update attention gate salience
 */
bool attention_adapter_update_salience(attention_adapter_t* adapter,
                                        uint32_t target_id,
                                        float salience);

/**
 * @brief Apply winner-take-all
 *
 * WHAT: Select single highest-weighted target
 * WHY:  Model limited attention capacity
 * HOW:  Find max weight → Set to 1.0 → Suppress others
 *
 * @param winner_id Output: winning target ID (optional)
 * @return true on success
 */
bool attention_adapter_apply_wta(attention_adapter_t* adapter,
                                  uint32_t* winner_id);

/**
 * @brief Update attention spotlight
 *
 * WHAT: Select top-N targets for focus
 * WHY:  Model flexible attention capacity
 * HOW:  Sort by weight → Mark top-N
 *
 * @param spotlight_ids Output: IDs in spotlight (optional)
 * @param num_in_spotlight Output: spotlight size (optional)
 * @return true on success
 */
bool attention_adapter_update_spotlight(attention_adapter_t* adapter,
                                         uint32_t* spotlight_ids,
                                         uint32_t* num_in_spotlight);

/**
 * @brief Route signal with attention
 *
 * WHAT: Modulate signal by attention weight
 * WHY:  Amplify attended signals, suppress unattended
 * HOW:  signal_out = signal_in * attention_weight
 *
 * @param adapter Adapter handle
 * @param target_id Target identifier
 * @param signal_in Input signal
 * @param signal_out Output signal (modulated)
 * @param signal_size Signal element count
 * @return true on success
 */
bool attention_adapter_route_signal(attention_adapter_t* adapter,
                                     uint32_t target_id,
                                     const float* signal_in,
                                     float* signal_out,
                                     size_t signal_size);

/**
 * @brief Detect attention pattern
 *
 * WHAT: Identify recurring attention configurations
 * WHY:  Learn attention strategies
 * HOW:  Analyze spotlight history → Find patterns
 *
 * @param adapter Adapter handle
 * @param pattern Output: detected pattern (optional)
 * @return true if pattern detected
 */
bool attention_adapter_detect_pattern(attention_adapter_t* adapter,
                                       attention_pattern_t* pattern);

/**
 * @brief Get recent attention shifts
 *
 * @param adapter Adapter handle
 * @param shifts Output array for shifts
 * @param max_shifts Maximum shifts to return
 * @param num_shifts Output: number of shifts
 * @return true on success
 */
bool attention_adapter_get_shifts(const attention_adapter_t* adapter,
                                   attention_shift_t* shifts,
                                   uint32_t max_shifts,
                                   uint32_t* num_shifts);

/**
 * @brief Reset attention state
 */
void attention_adapter_reset(attention_adapter_t* adapter);

/**
 * @brief Get adapter statistics
 */
bool attention_adapter_get_stats(const attention_adapter_t* adapter,
                                  attention_adapter_stats_t* stats);

/**
 * @brief Get default configuration
 */
attention_adapter_config_t attention_adapter_default_config(void);

//=============================================================================
// UTILITY FUNCTIONS
//=============================================================================

/**
 * @brief Get current timestamp in microseconds
 *
 * WHAT: Consistent time source for all adapters
 * WHY:  Synchronized timing across cognitive layer
 * HOW:  Use clock_gettime(CLOCK_MONOTONIC)
 */
uint64_t cognitive_adapter_get_timestamp_us(void);

/**
 * @brief Calculate salience from signal properties
 *
 * WHAT: Compute salience from signal statistics
 * WHY:  Automatic salience estimation
 * HOW:  Combine novelty, intensity, and variance
 *
 * @param signal Input signal
 * @param signal_size Signal element count
 * @param baseline Baseline value for novelty
 * @return Computed salience [0.0, 1.0]
 */
float cognitive_adapter_calculate_salience(const float* signal,
                                             size_t signal_size,
                                             float baseline);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_COGNITIVE_ADAPTERS_H
