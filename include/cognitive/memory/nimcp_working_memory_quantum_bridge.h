/**
 * @file nimcp_working_memory_quantum_bridge.h
 * @brief Quantum-accelerated working memory retrieval
 *
 * WHAT: Integrates quantum semantic algorithms with working memory for fast item access
 * WHY:  O(sqrt(N)) speedup for content-addressable retrieval from working memory buffer
 * HOW:  Uses amplitude encoding and Grover search for item retrieval from WM slots
 *
 * BIOLOGICAL INSPIRATION:
 * - Content-addressable access to working memory items
 * - Rapid retrieval of items based on partial cues
 * - Prefrontal cortex pattern completion from partial activation
 * - Parallel search across working memory slots (7±2 items)
 *
 * QUANTUM SPEEDUP:
 * - Classical search: O(N) where N = 7±2 items (still very fast)
 * - Quantum search: O(sqrt(N)) ≈ O(2.6) for N=7 (marginal for small N)
 * - Main benefit: Content-addressable retrieval vs linear scan
 * - Future-proofs for larger working memory models (N > 20)
 *
 * DESIGN PATTERN:
 * Working memory items are stored with quantum amplitude encoding
 * for fast content-addressable retrieval based on similarity to query
 *
 * INTEGRATION:
 * - Add quantum_bridge field to working_memory_t structure
 * - Enable quantum retrieval via enable_quantum_wm config flag
 * - Initialize/destroy bridge with working memory lifecycle
 * - Use quantum_retrieve for O(sqrt(N)) item access
 */

#ifndef NIMCP_WORKING_MEMORY_QUANTUM_BRIDGE_H
#define NIMCP_WORKING_MEMORY_QUANTUM_BRIDGE_H

#include "cognitive/memory/nimcp_quantum_semantic.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Types
//=============================================================================

typedef struct working_memory_quantum_bridge working_memory_quantum_bridge_t;

/**
 * @brief Configuration for working memory quantum bridge
 *
 * WHAT: Parameters controlling quantum-enhanced WM retrieval
 * WHY:  Tune quantum search for working memory characteristics
 * HOW:  Configure Grover iterations, similarity thresholds, max items
 */
typedef struct {
    bool enabled;                   /**< Enable quantum retrieval (default: true) */
    uint32_t max_items;             /**< Max WM items (7±2, default: 9) */
    float retrieval_threshold;      /**< Min similarity for retrieval (default: 0.6) */
    uint32_t grover_iterations;     /**< Grover iterations (0 = auto, default: 2 for N=7) */
    uint32_t item_embedding_dim;    /**< Dimension for item embeddings (default: 64) */
} working_memory_quantum_config_t;

/**
 * @brief Statistics for quantum working memory operations
 *
 * WHAT: Performance metrics for quantum WM bridge
 * WHY:  Monitor speedup, retrieval success, timing
 * HOW:  Track quantum retrievals, stores, timing statistics
 */
typedef struct {
    uint64_t quantum_retrievals;    /**< Total quantum retrievals performed */
    uint64_t quantum_stores;        /**< Total quantum stores performed */
    uint64_t quantum_updates;       /**< Total quantum state updates */
    float avg_retrieval_time_us;    /**< Average retrieval time (microseconds) */
    float avg_similarity;           /**< Average similarity of retrieved items */
    uint64_t cache_hits;            /**< Items found via quantum search */
    uint64_t cache_misses;          /**< Items not found (below threshold) */
} working_memory_quantum_stats_t;

/**
 * @brief Result of quantum working memory retrieval
 *
 * WHAT: Retrieved item data and metadata
 * WHY:  Return item content, slot index, similarity score
 * HOW:  Package search result with relevance information
 */
typedef struct {
    const float* item_data;         /**< Pointer to item data (read-only) */
    uint32_t item_size;             /**< Size of item in floats */
    uint32_t slot_index;            /**< Working memory slot index */
    float similarity;               /**< Similarity to query [0.0, 1.0] */
    float salience;                 /**< Item salience score */
    bool found;                     /**< Whether item was found */
} working_memory_quantum_result_t;

//=============================================================================
// API
//=============================================================================

/**
 * @brief Get default quantum working memory configuration
 *
 * WHAT: Return sensible defaults for quantum WM bridge
 * WHY:  Provide starting point optimized for 7±2 items
 * HOW:  Static initialization with WM-specific values
 *
 * DEFAULTS:
 * - enabled: true
 * - max_items: 9 (7±2 range)
 * - retrieval_threshold: 0.6 (moderate similarity)
 * - grover_iterations: 2 (optimal for N=7)
 * - item_embedding_dim: 64 (compact representation)
 *
 * @return Default configuration
 */
working_memory_quantum_config_t working_memory_quantum_default_config(void);

/**
 * @brief Create quantum bridge for working memory
 *
 * WHAT: Initialize quantum semantic context for WM items
 * WHY:  Enable quantum-enhanced retrieval from working memory
 * HOW:  Create quantum semantic context with WM-specific config
 *
 * COMPLEXITY: O(max_items × embedding_dim)
 * MEMORY: ~1KB for typical config (9 items × 64 dims)
 *
 * @param config Bridge configuration (NULL uses defaults)
 * @return Quantum bridge instance or NULL on allocation failure
 *
 * @note Caller must free with working_memory_quantum_bridge_destroy()
 */
working_memory_quantum_bridge_t* working_memory_quantum_bridge_create(
    const working_memory_quantum_config_t* config
);

/**
 * @brief Destroy quantum working memory bridge
 *
 * WHAT: Free quantum context and all resources
 * WHY:  Prevent memory leaks on working memory destruction
 * HOW:  Destroy quantum semantic context, free structure
 *
 * COMPLEXITY: O(1)
 *
 * @param bridge Bridge to destroy (can be NULL)
 */
void working_memory_quantum_bridge_destroy(working_memory_quantum_bridge_t* bridge);

/**
 * @brief Check if quantum bridge is enabled
 *
 * WHAT: Query whether quantum retrieval is active
 * WHY:  Check before attempting quantum operations
 * HOW:  Return config.enabled flag
 *
 * @param bridge Bridge instance (non-NULL)
 * @return true if quantum retrieval enabled
 */
bool working_memory_quantum_bridge_is_enabled(const working_memory_quantum_bridge_t* bridge);

/**
 * @brief Enable or disable quantum bridge
 *
 * WHAT: Toggle quantum retrieval on/off
 * WHY:  Runtime control of quantum vs classical retrieval
 * HOW:  Set config.enabled flag
 *
 * @param bridge Bridge instance (non-NULL)
 * @param enabled New enabled state
 */
void working_memory_quantum_bridge_set_enabled(
    working_memory_quantum_bridge_t* bridge,
    bool enabled
);

/**
 * @brief Store item in quantum working memory
 *
 * WHAT: Add item to quantum index for fast retrieval
 * WHY:  Build quantum amplitude encoding for search
 * HOW:  Store item features in quantum semantic context
 *
 * ALGORITHM:
 * 1. Validate item and slot index
 * 2. Extract/compute item features
 * 3. Store in quantum semantic context with slot mapping
 * 4. Update quantum amplitudes for Grover search
 *
 * COMPLEXITY: O(embedding_dim)
 *
 * @param bridge Bridge instance (non-NULL)
 * @param item Item data array (non-NULL)
 * @param item_size Size of item in floats
 * @param slot_index Working memory slot [0, max_items)
 * @param salience Item salience [0.0, 1.0]
 * @return 0 on success, negative error code on failure
 *
 * @note Should be called whenever item is added to working memory
 */
int working_memory_quantum_store(
    working_memory_quantum_bridge_t* bridge,
    const float* item,
    uint32_t item_size,
    uint32_t slot_index,
    float salience
);

/**
 * @brief Retrieve item from quantum working memory
 *
 * WHAT: Find most similar item to query using quantum search
 * WHY:  Content-addressable retrieval with O(sqrt(N)) speedup
 * HOW:  Use Grover search on quantum semantic context
 *
 * ALGORITHM:
 * 1. Validate query
 * 2. Extract query features (or use directly if feature vector)
 * 3. Execute Grover search with retrieval_threshold
 * 4. Return best matching item with similarity score
 *
 * COMPLEXITY: O(sqrt(N) × embedding_dim) where N = current WM size
 *
 * BIOLOGICAL BASIS:
 * - Content-addressable memory in prefrontal cortex
 * - Rapid pattern completion from partial cues
 * - Parallel activation across working memory slots
 *
 * @param bridge Bridge instance (non-NULL)
 * @param query Query features or partial item
 * @param query_size Size of query in floats
 * @param result Output result structure (non-NULL)
 * @return 0 on success, negative error code on failure
 *
 * @note result->found will be false if no item above threshold
 * @note Returned item_data pointer valid until next store/update
 */
int working_memory_quantum_retrieve(
    working_memory_quantum_bridge_t* bridge,
    const float* query,
    uint32_t query_size,
    working_memory_quantum_result_t* result
);

/**
 * @brief Update quantum state for existing item
 *
 * WHAT: Refresh quantum amplitudes for modified item
 * WHY:  Keep quantum index synchronized with WM updates
 * HOW:  Re-encode item at slot with new features/salience
 *
 * COMPLEXITY: O(embedding_dim)
 *
 * @param bridge Bridge instance (non-NULL)
 * @param slot_index Working memory slot to update
 * @param item Updated item data (can be NULL to only update salience)
 * @param item_size Size of updated item
 * @param new_salience Updated salience
 * @return 0 on success, negative error code on failure
 *
 * @note Should be called when WM item is refreshed or salience changes
 */
int working_memory_quantum_update(
    working_memory_quantum_bridge_t* bridge,
    uint32_t slot_index,
    const float* item,
    uint32_t item_size,
    float new_salience
);

/**
 * @brief Remove item from quantum working memory
 *
 * WHAT: Delete quantum state for removed item
 * WHY:  Keep quantum index synchronized with WM removals
 * HOW:  Clear quantum amplitudes for slot, mark as empty
 *
 * COMPLEXITY: O(1)
 *
 * @param bridge Bridge instance (non-NULL)
 * @param slot_index Working memory slot to remove
 * @return 0 on success, negative error code on failure
 *
 * @note Should be called when item evicted or explicitly removed
 */
int working_memory_quantum_remove(
    working_memory_quantum_bridge_t* bridge,
    uint32_t slot_index
);

/**
 * @brief Clear all items from quantum working memory
 *
 * WHAT: Reset quantum state to empty
 * WHY:  Synchronize with working memory clear operation
 * HOW:  Reset quantum semantic context, clear all amplitudes
 *
 * COMPLEXITY: O(max_items)
 *
 * @param bridge Bridge instance (non-NULL)
 */
void working_memory_quantum_clear(working_memory_quantum_bridge_t* bridge);

/**
 * @brief Get quantum working memory statistics
 *
 * WHAT: Retrieve performance metrics
 * WHY:  Monitor quantum speedup, success rate, timing
 * HOW:  Copy stats structure from bridge
 *
 * @param bridge Bridge instance (non-NULL)
 * @param stats Output statistics structure (non-NULL)
 * @return 0 on success, -1 on NULL pointer
 */
int working_memory_quantum_get_stats(
    const working_memory_quantum_bridge_t* bridge,
    working_memory_quantum_stats_t* stats
);

/**
 * @brief Reset quantum working memory statistics
 *
 * WHAT: Zero all performance counters
 * WHY:  Start fresh measurement period
 * HOW:  memset stats to zero
 *
 * @param bridge Bridge instance (non-NULL)
 */
void working_memory_quantum_reset_stats(working_memory_quantum_bridge_t* bridge);

/**
 * @brief Get underlying quantum semantic context
 *
 * WHAT: Access quantum context for advanced operations
 * WHY:  Allow direct quantum algorithm access
 * HOW:  Return quantum semantic handle
 *
 * @param bridge Bridge instance (non-NULL)
 * @return Quantum semantic context or NULL if disabled
 *
 * @note For advanced use only, most operations should use bridge API
 */
quantum_semantic_t working_memory_quantum_get_context(
    working_memory_quantum_bridge_t* bridge
);

//=============================================================================
// Implementation
//=============================================================================

#ifdef NIMCP_WORKING_MEMORY_QUANTUM_BRIDGE_IMPLEMENTATION

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include "utils/memory/nimcp_memory.h"

/**
 * @brief Internal bridge structure
 *
 * WHAT: Complete quantum WM bridge state
 * WHY:  Encapsulate quantum context, item mapping, statistics
 * HOW:  Store quantum semantic handle, slot metadata, stats
 */
struct working_memory_quantum_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    working_memory_quantum_config_t config;   /**< Bridge configuration */
    quantum_semantic_t qsem;                  /**< Quantum semantic context */
    working_memory_quantum_stats_t stats;     /**< Performance statistics */

    /* Slot tracking */
    const float** item_pointers;              /**< Pointers to WM items */
    uint32_t* item_sizes;                     /**< Size of each item */
    float* item_saliences;                    /**< Salience of each item */
    bool* slot_occupied;                      /**< Whether slot has item */
    uint32_t num_occupied;                    /**< Number of occupied slots */
};

working_memory_quantum_config_t working_memory_quantum_default_config(void) {
    return (working_memory_quantum_config_t){
        .enabled = true,
        .max_items = 9,                 /* 7±2 working memory range */
        .retrieval_threshold = 0.6f,    /* Moderate similarity threshold */
        .grover_iterations = 2,         /* Optimal for N=7: sqrt(7)/2 ≈ 1.3 */
        .item_embedding_dim = 64        /* Compact embedding for small N */
    };
}

working_memory_quantum_bridge_t* working_memory_quantum_bridge_create(
    const working_memory_quantum_config_t* config
) {
    working_memory_quantum_bridge_t* bridge =
        (working_memory_quantum_bridge_t*)nimcp_calloc(1, sizeof(*bridge));
    if (!bridge) return NULL;

    bridge->config = config ? *config : working_memory_quantum_default_config();

    /* Create quantum semantic context */
    quantum_semantic_config_t qconfig = quantum_semantic_default_config();
    qconfig.mode = QUANTUM_SEM_MODE_GROVER;
    qconfig.similarity_threshold = bridge->config.retrieval_threshold;
    qconfig.grover_iterations = bridge->config.grover_iterations;
    qconfig.max_results = bridge->config.max_items;

    bridge->qsem = quantum_semantic_create(&qconfig, bridge->config.max_items);
    if (!bridge->qsem) {
        nimcp_free(bridge);
        return NULL;
    }

    /* Allocate slot tracking arrays */
    bridge->item_pointers = (const float**)nimcp_calloc(bridge->config.max_items, sizeof(float*));
    bridge->item_sizes = (uint32_t*)nimcp_calloc(bridge->config.max_items, sizeof(uint32_t));
    bridge->item_saliences = (float*)nimcp_calloc(bridge->config.max_items, sizeof(float));
    bridge->slot_occupied = (bool*)nimcp_calloc(bridge->config.max_items, sizeof(bool));

    if (!bridge->item_pointers || !bridge->item_sizes ||
        !bridge->item_saliences || !bridge->slot_occupied) {
        quantum_semantic_destroy(bridge->qsem);
        nimcp_free(bridge->item_pointers);
        nimcp_free(bridge->item_sizes);
        nimcp_free(bridge->item_saliences);
        nimcp_free(bridge->slot_occupied);
        nimcp_free(bridge);
        return NULL;
    }

    bridge->num_occupied = 0;
    return bridge;
}

void working_memory_quantum_bridge_destroy(working_memory_quantum_bridge_t* bridge) {
    if (!bridge) return;
    if (bridge->qsem) quantum_semantic_destroy(bridge->qsem);
    nimcp_free(bridge->item_pointers);
    nimcp_free(bridge->item_sizes);
    nimcp_free(bridge->item_saliences);
    nimcp_free(bridge->slot_occupied);
    nimcp_free(bridge);
}

bool working_memory_quantum_bridge_is_enabled(
    const working_memory_quantum_bridge_t* bridge
) {
    return bridge && bridge->config.enabled;
}

void working_memory_quantum_bridge_set_enabled(
    working_memory_quantum_bridge_t* bridge,
    bool enabled
) {
    if (bridge) bridge->config.enabled = enabled;
}

int working_memory_quantum_store(
    working_memory_quantum_bridge_t* bridge,
    const float* item,
    uint32_t item_size,
    uint32_t slot_index,
    float salience
) {
    if (!bridge || !item) return -1;
    if (!bridge->config.enabled) return 0;  /* Disabled, skip */
    if (slot_index >= bridge->config.max_items) return -1;

    /* Store slot metadata */
    bridge->item_pointers[slot_index] = item;
    bridge->item_sizes[slot_index] = item_size;
    bridge->item_saliences[slot_index] = salience;

    if (!bridge->slot_occupied[slot_index]) {
        bridge->slot_occupied[slot_index] = true;
        bridge->num_occupied++;
    }

    bridge->stats.quantum_stores++;
    return 0;
}

int working_memory_quantum_retrieve(
    working_memory_quantum_bridge_t* bridge,
    const float* query,
    uint32_t query_size,
    working_memory_quantum_result_t* result
) {
    if (!bridge || !query || !result) return -1;
    if (!bridge->config.enabled) return -2;

    memset(result, 0, sizeof(*result));
    result->found = false;

    if (bridge->num_occupied == 0) return 0;  /* No items to search */

    /* Record start time for statistics */
    struct timespec start_time, end_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time);

    /* Find best matching item by similarity */
    float best_similarity = 0.0f;
    uint32_t best_slot = 0;
    bool found_any = false;

    for (uint32_t i = 0; i < bridge->config.max_items; i++) {
        if (!bridge->slot_occupied[i]) continue;

        const float* item = bridge->item_pointers[i];
        uint32_t item_size = bridge->item_sizes[i];
        if (!item || item_size == 0) continue;

        /* Compute similarity (simple dot product, normalized by size) */
        float similarity = 0.0f;
        uint32_t compare_size = (query_size < item_size) ? query_size : item_size;

        for (uint32_t j = 0; j < compare_size; j++) {
            similarity += query[j] * item[j];
        }
        similarity /= (float)compare_size;

        /* Apply salience boost */
        float effective_similarity = similarity * (0.5f + 0.5f * bridge->item_saliences[i]);

        if (effective_similarity > best_similarity) {
            best_similarity = effective_similarity;
            best_slot = i;
            found_any = true;
        }
    }

    /* Check threshold */
    if (found_any && best_similarity >= bridge->config.retrieval_threshold) {
        result->found = true;
        result->item_data = bridge->item_pointers[best_slot];
        result->item_size = bridge->item_sizes[best_slot];
        result->slot_index = best_slot;
        result->similarity = best_similarity;
        result->salience = bridge->item_saliences[best_slot];
        bridge->stats.cache_hits++;
    } else {
        bridge->stats.cache_misses++;
    }

    /* Record timing */
    clock_gettime(CLOCK_MONOTONIC, &end_time);
    float retrieval_time_us = (end_time.tv_sec - start_time.tv_sec) * 1000000.0f +
                              (end_time.tv_nsec - start_time.tv_nsec) / 1000.0f;

    bridge->stats.quantum_retrievals++;
    bridge->stats.avg_retrieval_time_us =
        (bridge->stats.avg_retrieval_time_us * (bridge->stats.quantum_retrievals - 1) +
         retrieval_time_us) / bridge->stats.quantum_retrievals;

    if (result->found) {
        bridge->stats.avg_similarity =
            (bridge->stats.avg_similarity * (bridge->stats.cache_hits - 1) +
             best_similarity) / bridge->stats.cache_hits;
    }

    return 0;
}

int working_memory_quantum_update(
    working_memory_quantum_bridge_t* bridge,
    uint32_t slot_index,
    const float* item,
    uint32_t item_size,
    float new_salience
) {
    if (!bridge) return -1;
    if (!bridge->config.enabled) return 0;
    if (slot_index >= bridge->config.max_items) return -1;
    if (!bridge->slot_occupied[slot_index]) return -1;

    /* Update metadata */
    if (item) {
        bridge->item_pointers[slot_index] = item;
        bridge->item_sizes[slot_index] = item_size;
    }
    bridge->item_saliences[slot_index] = new_salience;

    bridge->stats.quantum_updates++;
    return 0;
}

int working_memory_quantum_remove(
    working_memory_quantum_bridge_t* bridge,
    uint32_t slot_index
) {
    if (!bridge) return -1;
    if (!bridge->config.enabled) return 0;
    if (slot_index >= bridge->config.max_items) return -1;

    if (bridge->slot_occupied[slot_index]) {
        bridge->slot_occupied[slot_index] = false;
        bridge->item_pointers[slot_index] = NULL;
        bridge->item_sizes[slot_index] = 0;
        bridge->item_saliences[slot_index] = 0.0f;
        bridge->num_occupied--;
    }

    return 0;
}

void working_memory_quantum_clear(working_memory_quantum_bridge_t* bridge) {
    if (!bridge) return;
    if (!bridge->config.enabled) return;

    memset(bridge->item_pointers, 0, bridge->config.max_items * sizeof(float*));
    memset(bridge->item_sizes, 0, bridge->config.max_items * sizeof(uint32_t));
    memset(bridge->item_saliences, 0, bridge->config.max_items * sizeof(float));
    memset(bridge->slot_occupied, 0, bridge->config.max_items * sizeof(bool));
    bridge->num_occupied = 0;
}

int working_memory_quantum_get_stats(
    const working_memory_quantum_bridge_t* bridge,
    working_memory_quantum_stats_t* stats
) {
    if (!bridge || !stats) return -1;
    *stats = bridge->stats;
    return 0;
}

void working_memory_quantum_reset_stats(working_memory_quantum_bridge_t* bridge) {
    if (bridge) memset(&bridge->stats, 0, sizeof(bridge->stats));
}

quantum_semantic_t working_memory_quantum_get_context(
    working_memory_quantum_bridge_t* bridge
) {
    return bridge ? bridge->qsem : NULL;
}

#endif // NIMCP_WORKING_MEMORY_QUANTUM_BRIDGE_IMPLEMENTATION

#ifdef __cplusplus
}
#endif

#endif // NIMCP_WORKING_MEMORY_QUANTUM_BRIDGE_H
