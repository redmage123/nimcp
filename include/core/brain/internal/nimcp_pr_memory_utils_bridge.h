/**
 * @file nimcp_pr_memory_utils_bridge.h
 * @brief PR Memory Utils Integration - Hash Table, Memory Pools, Metrics, Cache
 * @version 1.0.0
 * @date 2026-01-12
 *
 * Integrates NIMCP utilities into PR Memory for enhanced Z-Ladder operations:
 * - Hash table for O(1) memory node lookup
 * - Memory pools for node allocation
 * - LRU cache for frequently accessed memories
 * - Metrics for consolidation analytics
 * - Graph utilities for entanglement analysis
 */

#ifndef NIMCP_PR_MEMORY_UTILS_BRIDGE_H
#define NIMCP_PR_MEMORY_UTILS_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include "core/brain/internal/nimcp_brain_pr_memory.h"

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * CONSTANTS
 *===========================================================================*/

#define PR_MEMORY_HASH_BUCKETS      4096    /* Hash table size */
#define PR_NODE_POOL_SIZE           100000  /* Memory node pool */
#define PR_CACHE_SIZE               512     /* LRU cache entries */
#define PR_METRICS_BUFFER_SIZE      10000   /* Metrics buffer */

/*=============================================================================
 * FORWARD DECLARATIONS
 *===========================================================================*/

typedef struct pr_memory_utils_ctx_internal* pr_memory_utils_ctx_t;

/*=============================================================================
 * METRICS STRUCTURES
 *===========================================================================*/

/**
 * @brief PR Memory metrics for Z-Ladder analytics
 */
typedef struct {
    /* Z-Ladder counts */
    uint32_t z0_count;                  /**< Working memory (Z0) */
    uint32_t z1_count;                  /**< Short-term (Z1) */
    uint32_t z2_count;                  /**< Long-term (Z2) */
    uint32_t z3_count;                  /**< Permanent (Z3) */

    /* Counters */
    uint64_t total_stores;              /**< Total memory stores */
    uint64_t total_retrievals;          /**< Total retrievals */
    uint64_t total_promotions;          /**< Z-level promotions */
    uint64_t total_demotions;           /**< Z-level demotions */
    uint64_t total_evictions;           /**< Evicted memories */
    uint64_t cache_hits;                /**< LRU cache hits */
    uint64_t cache_misses;              /**< LRU cache misses */

    /* Gauges */
    float promotion_rate;               /**< Promotions per second */
    float retrieval_efficiency;         /**< Cache hit rate */
    float memory_utilization;           /**< Used/total capacity */
    float theta_phase;                  /**< Current theta phase */
    float gamma_amplitude;              /**< Current gamma amplitude */

    /* Entanglement */
    uint32_t entangle_nodes;            /**< Entanglement graph nodes */
    uint32_t entangle_edges;            /**< Entanglement graph edges */
    float avg_node_degree;              /**< Average connections */
    float clustering_coefficient;       /**< Graph clustering */

    /* Histograms */
    uint32_t resonance_strength_hist[20];   /**< Resonance distribution */
    uint32_t consolidation_latency_hist[20];/**< Promotion latency */

    /* Timers */
    double avg_store_latency_us;        /**< Store operation latency */
    double avg_retrieval_latency_us;    /**< Retrieval latency */
    double avg_promotion_time_ms;       /**< Promotion processing time */

    /* Timestamp */
    uint64_t last_update_ms;
} pr_memory_metrics_t;

/*=============================================================================
 * CONFIGURATION
 *===========================================================================*/

/**
 * @brief PR Memory utils configuration
 */
typedef struct {
    /* Hash table */
    bool enable_hash_index;
    uint32_t hash_buckets;

    /* Memory pools */
    bool enable_node_pool;
    uint32_t node_pool_size;

    /* LRU cache */
    bool enable_cache;
    uint32_t cache_size;

    /* Metrics */
    bool enable_metrics;
    uint32_t metrics_flush_interval_ms;
    char metrics_output_dir[256];

    /* Graph analysis */
    bool enable_graph_analysis;
    uint32_t graph_analysis_interval;   /**< Analyze every N operations */
} pr_memory_utils_config_t;

/*=============================================================================
 * LIFECYCLE API
 *===========================================================================*/

/**
 * @brief Get default configuration
 */
pr_memory_utils_config_t pr_memory_utils_default_config(void);

/**
 * @brief Create PR memory utils context
 */
pr_memory_utils_ctx_t pr_memory_utils_create(const pr_memory_utils_config_t* config);

/**
 * @brief Destroy PR memory utils context
 */
void pr_memory_utils_destroy(pr_memory_utils_ctx_t ctx);

/**
 * @brief Reset utils context
 */
void pr_memory_utils_reset(pr_memory_utils_ctx_t ctx);

/**
 * @brief Attach utils to brain PR memory
 */
bool pr_memory_utils_attach(pr_memory_utils_ctx_t ctx, struct brain_struct* brain);

/*=============================================================================
 * HASH INDEX API
 *===========================================================================*/

/**
 * @brief Index memory node by resonance signature
 */
bool pr_memory_utils_index_node(
    pr_memory_utils_ctx_t ctx,
    uint64_t resonance_signature,
    void* node_ptr
);

/**
 * @brief Lookup node by resonance signature (O(1))
 */
void* pr_memory_utils_lookup_node(
    pr_memory_utils_ctx_t ctx,
    uint64_t resonance_signature
);

/**
 * @brief Remove node from index
 */
bool pr_memory_utils_remove_node(
    pr_memory_utils_ctx_t ctx,
    uint64_t resonance_signature
);

/**
 * @brief Get hash table statistics
 */
void pr_memory_utils_hash_stats(
    pr_memory_utils_ctx_t ctx,
    uint32_t* num_entries,
    uint32_t* num_buckets,
    float* load_factor
);

/*=============================================================================
 * LRU CACHE API
 *===========================================================================*/

/**
 * @brief Get from cache (updates LRU order)
 */
void* pr_memory_utils_cache_get(
    pr_memory_utils_ctx_t ctx,
    uint64_t resonance_signature
);

/**
 * @brief Put into cache (may evict LRU entry)
 */
bool pr_memory_utils_cache_put(
    pr_memory_utils_ctx_t ctx,
    uint64_t resonance_signature,
    void* node_ptr
);

/**
 * @brief Invalidate cache entry
 */
void pr_memory_utils_cache_invalidate(
    pr_memory_utils_ctx_t ctx,
    uint64_t resonance_signature
);

/**
 * @brief Get cache statistics
 */
void pr_memory_utils_cache_stats(
    pr_memory_utils_ctx_t ctx,
    uint32_t* size,
    uint32_t* capacity,
    float* hit_rate
);

/*=============================================================================
 * MEMORY POOL API
 *===========================================================================*/

/**
 * @brief Allocate memory node from pool
 */
void* pr_memory_utils_alloc_node(pr_memory_utils_ctx_t ctx, size_t size);

/**
 * @brief Free memory node to pool
 */
void pr_memory_utils_free_node(pr_memory_utils_ctx_t ctx, void* node);

/**
 * @brief Get pool statistics
 */
void pr_memory_utils_pool_stats(
    pr_memory_utils_ctx_t ctx,
    uint32_t* total,
    uint32_t* used,
    uint32_t* free
);

/*=============================================================================
 * METRICS API
 *===========================================================================*/

/**
 * @brief Record store operation
 */
void pr_memory_utils_record_store(
    pr_memory_utils_ctx_t ctx,
    uint64_t resonance_signature,
    int z_level,
    double latency_us
);

/**
 * @brief Record retrieval operation
 */
void pr_memory_utils_record_retrieval(
    pr_memory_utils_ctx_t ctx,
    uint64_t resonance_signature,
    bool cache_hit,
    double latency_us
);

/**
 * @brief Record promotion event
 */
void pr_memory_utils_record_promotion(
    pr_memory_utils_ctx_t ctx,
    int from_level,
    int to_level,
    double latency_ms
);

/**
 * @brief Record eviction event
 */
void pr_memory_utils_record_eviction(
    pr_memory_utils_ctx_t ctx,
    int z_level
);

/**
 * @brief Update Z-ladder counts
 */
void pr_memory_utils_update_z_counts(
    pr_memory_utils_ctx_t ctx,
    uint32_t z0, uint32_t z1, uint32_t z2, uint32_t z3
);

/**
 * @brief Update entanglement stats
 */
void pr_memory_utils_update_entangle_stats(
    pr_memory_utils_ctx_t ctx,
    uint32_t nodes,
    uint32_t edges,
    float clustering
);

/**
 * @brief Get current metrics snapshot
 */
bool pr_memory_utils_get_metrics(pr_memory_utils_ctx_t ctx, pr_memory_metrics_t* metrics);

/**
 * @brief Flush metrics to disk
 */
int32_t pr_memory_utils_flush_metrics(pr_memory_utils_ctx_t ctx);

/**
 * @brief Export metrics to CSV
 */
bool pr_memory_utils_export_csv(pr_memory_utils_ctx_t ctx, const char* filename);

/**
 * @brief Export metrics to JSON
 */
bool pr_memory_utils_export_json(pr_memory_utils_ctx_t ctx, const char* filename);

/*=============================================================================
 * GRAPH ANALYSIS API (Entanglement)
 *===========================================================================*/

/**
 * @brief Compute clustering coefficient of entanglement graph
 */
float pr_memory_utils_compute_clustering(pr_memory_utils_ctx_t ctx);

/**
 * @brief Find hub nodes (high degree centrality)
 */
bool pr_memory_utils_find_hubs(
    pr_memory_utils_ctx_t ctx,
    uint32_t* hub_ids,
    uint32_t max_hubs,
    uint32_t* num_found
);

/**
 * @brief Compute modularity of memory clusters
 */
float pr_memory_utils_compute_modularity(pr_memory_utils_ctx_t ctx);

/**
 * @brief Detect memory communities (Louvain algorithm)
 */
bool pr_memory_utils_detect_communities(
    pr_memory_utils_ctx_t ctx,
    uint32_t* community_assignments,
    uint32_t* num_communities
);

/*=============================================================================
 * ENHANCED PR MEMORY OPERATIONS
 *===========================================================================*/

/**
 * @brief Enhanced store with hash index and cache
 */
bool pr_memory_utils_store_enhanced(
    pr_memory_utils_ctx_t ctx,
    struct brain_struct* brain,
    const void* content,
    size_t content_size,
    float resonance_strength,
    uint64_t* signature_out
);

/**
 * @brief Enhanced retrieve with cache and metrics
 */
void* pr_memory_utils_retrieve_enhanced(
    pr_memory_utils_ctx_t ctx,
    struct brain_struct* brain,
    uint64_t resonance_signature,
    float* strength_out
);

/**
 * @brief Enhanced consolidation tick with analytics
 */
bool pr_memory_utils_tick_enhanced(
    pr_memory_utils_ctx_t ctx,
    struct brain_struct* brain,
    uint64_t current_time_us
);

/**
 * @brief Batch promote memories with optimization
 */
uint32_t pr_memory_utils_batch_promote(
    pr_memory_utils_ctx_t ctx,
    struct brain_struct* brain,
    const uint64_t* signatures,
    uint32_t count,
    int target_level
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PR_MEMORY_UTILS_BRIDGE_H */
