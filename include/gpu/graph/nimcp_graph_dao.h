/**
 * @file nimcp_graph_dao.h
 * @brief Data Access Object Pattern for GPU Graph Storage and Retrieval
 *
 * WHAT: Abstract DAO interface for GPU graph data management
 * WHY:  Decouples graph operations from storage implementation
 * HOW:  Strategy pattern with concrete GPU/hybrid implementations
 *
 * ARCHITECTURE:
 *
 *   +----------------------------------------------------------+
 *   |                    GRAPH DAO LAYER                       |
 *   |                                                          |
 *   |  +----------------+     +------------------+             |
 *   |  |  Abstract DAO  |<----|  GPU Graph DAO   |             |
 *   |  |   Interface    |     | (CUDA Backend)   |             |
 *   |  +----------------+     +------------------+             |
 *   |          ^                     |                         |
 *   |          |              +------+------+                  |
 *   |          |              |             |                  |
 *   |  +----------------+  +--------+  +----------+            |
 *   |  | Hybrid DAO     |  |  LRU   |  |   GPU    |            |
 *   |  |(CPU+GPU Cache) |  | Cache  |  | Storage  |            |
 *   |  +----------------+  +--------+  +----------+            |
 *   +----------------------------------------------------------+
 *
 * FEATURES:
 * - CRUD operations for graph entities
 * - Batch insert for efficient bulk loading
 * - GPU-CPU synchronization
 * - LRU caching for frequently accessed graphs
 * - Thread-safe operations with mutex protection
 *
 * @version 1.0
 * @author NIMCP Development Team
 * @date 2025
 */

#ifndef NIMCP_GRAPH_DAO_H
#define NIMCP_GRAPH_DAO_H

#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/graphql/nimcp_graphql_utils.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "common/nimcp_export.h"
#include "utils/error/nimcp_error_codes.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "utils/thread/nimcp_thread.h"

//=============================================================================
// Forward Declarations
//=============================================================================

struct nimcp_gpu_graph_s;       // Defined in nimcp_graph_gpu.h
struct nimcp_gpu_graph_dao_s;   // Defined below

//=============================================================================
// DAO Configuration
//=============================================================================

/** Default LRU cache size (number of graphs) */
#define NIMCP_DAO_DEFAULT_CACHE_SIZE 16

/** Default GPU storage capacity (number of graphs) */
#define NIMCP_DAO_DEFAULT_GPU_CAPACITY 64

/** Maximum graph ID */
#define NIMCP_DAO_MAX_GRAPH_ID 0x7FFFFFFF

//=============================================================================
// DAO Operation Interface (Strategy Pattern)
//=============================================================================

/**
 * @brief Abstract DAO operations interface
 *
 * WHAT: Virtual function table for graph data access
 * WHY:  Enables different storage backends (GPU, hybrid, distributed)
 * HOW:  Function pointers for CRUD and sync operations
 */
typedef struct nimcp_graph_dao_ops_s {
    /**
     * @brief Create a new graph in storage
     * @param dao DAO instance (cast to concrete type)
     * @param graph_data Graph data to store
     * @return Graph ID on success, negative error code on failure
     */
    int (*create)(void* dao, void* graph_data);

    /**
     * @brief Read a graph by ID
     * @param dao DAO instance
     * @param id Graph ID
     * @param out_data Output: graph data pointer
     * @return NIMCP_SUCCESS or error code
     */
    int (*read)(void* dao, int id, void** out_data);

    /**
     * @brief Update an existing graph
     * @param dao DAO instance
     * @param id Graph ID
     * @param graph_data New graph data
     * @return NIMCP_SUCCESS or error code
     */
    int (*update)(void* dao, int id, void* graph_data);

    /**
     * @brief Delete a graph by ID
     * @param dao DAO instance
     * @param id Graph ID
     * @return NIMCP_SUCCESS or error code
     */
    int (*delete_graph)(void* dao, int id);

    /**
     * @brief Execute a query on stored graphs
     * @param dao DAO instance
     * @param query Graph query
     * @param results Output: query results
     * @return NIMCP_SUCCESS or error code
     */
    int (*query)(void* dao, nimcp_graph_query_t* query, void** results);

    /**
     * @brief Batch insert multiple graphs
     * @param dao DAO instance
     * @param graphs Array of graph pointers
     * @param count Number of graphs
     * @return Number of successfully inserted graphs
     */
    int (*batch_insert)(void* dao, void** graphs, size_t count);

    /**
     * @brief Synchronize host cache to GPU
     * @param dao DAO instance
     * @return NIMCP_SUCCESS or error code
     */
    int (*sync_to_gpu)(void* dao);

    /**
     * @brief Synchronize GPU storage to host cache
     * @param dao DAO instance
     * @return NIMCP_SUCCESS or error code
     */
    int (*sync_from_gpu)(void* dao);

} nimcp_graph_dao_ops_t;

//=============================================================================
// LRU Cache Entry
//=============================================================================

/**
 * @brief LRU cache entry for frequently accessed graphs
 */
typedef struct nimcp_graph_cache_entry_s {
    int graph_id;                    /**< Graph identifier */
    void* graph_data;                /**< Cached graph data (host memory) */
    uint64_t access_time;            /**< Last access timestamp */
    uint64_t access_count;           /**< Access count for statistics */
    bool dirty;                      /**< Modified since last sync */
    struct nimcp_graph_cache_entry_s* prev; /**< Previous in hash chain */
    struct nimcp_graph_cache_entry_s* next; /**< Next in hash chain */
    struct nimcp_graph_cache_entry_s* lru_prev; /**< Previous in LRU list */
    struct nimcp_graph_cache_entry_s* lru_next; /**< Next in LRU list */
} nimcp_graph_cache_entry_t;

/**
 * @brief LRU cache for graph data
 */
typedef struct nimcp_graph_lru_cache_s {
    nimcp_graph_cache_entry_t** entries;  /**< Hash table of entries */
    size_t capacity;                       /**< Maximum entries */
    size_t count;                          /**< Current entry count */
    nimcp_graph_cache_entry_t* head;       /**< Most recently used */
    nimcp_graph_cache_entry_t* tail;       /**< Least recently used */
    uint64_t hits;                         /**< Cache hit count */
    uint64_t misses;                       /**< Cache miss count */
} nimcp_graph_lru_cache_t;

//=============================================================================
// GPU Graph DAO
//=============================================================================

/**
 * @brief Concrete GPU Graph DAO implementation
 *
 * WHAT: GPU-resident graph storage with LRU host cache
 * WHY:  Efficient graph storage for GPU-accelerated operations
 * HOW:  GPU memory management + host-side LRU cache
 */
typedef struct nimcp_gpu_graph_dao_s {
    nimcp_graph_dao_ops_t ops;       /**< DAO operation interface */
    nimcp_gpu_context_t* gpu_ctx;    /**< GPU context */
    nimcp_graph_lru_cache_t* host_cache; /**< LRU cache for host-side access */
    void** gpu_storage;              /**< Array of GPU-resident graphs */
    int* gpu_ids;                    /**< Graph IDs in GPU storage */
    size_t cache_size;               /**< Host cache capacity */
    size_t gpu_capacity;             /**< GPU storage capacity */
    size_t gpu_count;                /**< Current GPU-resident count */
    int next_id;                     /**< Next available graph ID */
    nimcp_mutex_t lock;            /**< Thread safety mutex */
    bool initialized;                /**< Initialization flag */
} nimcp_gpu_graph_dao_t;

//=============================================================================
// Factory Functions
//=============================================================================

/**
 * @brief Create a GPU graph DAO
 *
 * WHAT: Creates a DAO optimized for GPU-resident graphs
 * WHY:  Efficient access to GPU graph data
 * HOW:  GPU memory allocation + host cache initialization
 *
 * @param gpu_context GPU context for CUDA operations
 * @param cache_size Host cache size (0 for default)
 * @return DAO instance or NULL on failure
 */
NIMCP_EXPORT nimcp_gpu_graph_dao_t* nimcp_graph_dao_create_gpu(
    nimcp_gpu_context_t* gpu_context,
    size_t cache_size
);

/**
 * @brief Create a hybrid graph DAO
 *
 * WHAT: Creates a DAO with both GPU and CPU storage
 * WHY:  Supports graphs larger than GPU memory
 * HOW:  GPU for active graphs, CPU for overflow
 *
 * @param gpu_context GPU context for CUDA operations
 * @param gpu_capacity Maximum graphs in GPU memory
 * @return DAO instance or NULL on failure
 */
NIMCP_EXPORT nimcp_gpu_graph_dao_t* nimcp_graph_dao_create_hybrid(
    nimcp_gpu_context_t* gpu_context,
    size_t gpu_capacity
);

/**
 * @brief Destroy a graph DAO
 *
 * @param dao DAO to destroy (can be NULL)
 */
NIMCP_EXPORT void nimcp_graph_dao_destroy(nimcp_gpu_graph_dao_t* dao);

//=============================================================================
// CRUD Operations
//=============================================================================

/**
 * @brief Create a new graph in the DAO
 *
 * @param dao DAO instance
 * @param graph Graph to store (takes ownership)
 * @return Graph ID on success, negative on failure
 */
NIMCP_EXPORT int nimcp_graph_dao_create(
    nimcp_gpu_graph_dao_t* dao,
    struct nimcp_gpu_graph_s* graph
);

/**
 * @brief Read a graph by ID
 *
 * @param dao DAO instance
 * @param id Graph ID
 * @param out_graph Output: graph pointer (do not free)
 * @return NIMCP_SUCCESS or error code
 */
NIMCP_EXPORT nimcp_error_t nimcp_graph_dao_read(
    nimcp_gpu_graph_dao_t* dao,
    int id,
    struct nimcp_gpu_graph_s** out_graph
);

/**
 * @brief Update an existing graph
 *
 * @param dao DAO instance
 * @param id Graph ID
 * @param graph New graph data (takes ownership)
 * @return NIMCP_SUCCESS or error code
 */
NIMCP_EXPORT nimcp_error_t nimcp_graph_dao_update(
    nimcp_gpu_graph_dao_t* dao,
    int id,
    struct nimcp_gpu_graph_s* graph
);

/**
 * @brief Delete a graph by ID
 *
 * @param dao DAO instance
 * @param id Graph ID
 * @return NIMCP_SUCCESS or error code
 */
NIMCP_EXPORT nimcp_error_t nimcp_graph_dao_delete(
    nimcp_gpu_graph_dao_t* dao,
    int id
);

//=============================================================================
// Batch Operations
//=============================================================================

/**
 * @brief Batch insert multiple graphs
 *
 * @param dao DAO instance
 * @param graphs Array of graph pointers (takes ownership)
 * @param count Number of graphs
 * @param out_ids Output: array of assigned IDs (caller allocated)
 * @return Number of successfully inserted graphs
 */
NIMCP_EXPORT size_t nimcp_graph_dao_batch_insert(
    nimcp_gpu_graph_dao_t* dao,
    struct nimcp_gpu_graph_s** graphs,
    size_t count,
    int* out_ids
);

/**
 * @brief Batch read multiple graphs by ID
 *
 * @param dao DAO instance
 * @param ids Array of graph IDs
 * @param count Number of IDs
 * @param out_graphs Output: array of graph pointers (caller allocated)
 * @return Number of successfully read graphs
 */
NIMCP_EXPORT size_t nimcp_graph_dao_batch_read(
    nimcp_gpu_graph_dao_t* dao,
    const int* ids,
    size_t count,
    struct nimcp_gpu_graph_s** out_graphs
);

/**
 * @brief Batch delete multiple graphs
 *
 * @param dao DAO instance
 * @param ids Array of graph IDs
 * @param count Number of IDs
 * @return Number of successfully deleted graphs
 */
NIMCP_EXPORT size_t nimcp_graph_dao_batch_delete(
    nimcp_gpu_graph_dao_t* dao,
    const int* ids,
    size_t count
);

//=============================================================================
// Query Operations
//=============================================================================

/**
 * @brief Execute a query on stored graphs
 *
 * @param dao DAO instance
 * @param query GraphQL-style query
 * @param results Output: query results
 * @return NIMCP_SUCCESS or error code
 */
NIMCP_EXPORT nimcp_error_t nimcp_graph_dao_query(
    nimcp_gpu_graph_dao_t* dao,
    nimcp_graph_query_t* query,
    nimcp_graphql_result_t** results
);

/**
 * @brief Find graphs matching filter criteria
 *
 * @param dao DAO instance
 * @param filter Filter expression
 * @param out_ids Output: matching graph IDs (caller allocated)
 * @param max_results Maximum results to return
 * @return Number of matching graphs
 */
NIMCP_EXPORT size_t nimcp_graph_dao_find(
    nimcp_gpu_graph_dao_t* dao,
    const char* filter,
    int* out_ids,
    size_t max_results
);

/**
 * @brief Count graphs matching filter criteria
 *
 * @param dao DAO instance
 * @param filter Filter expression (NULL for all)
 * @return Number of matching graphs
 */
NIMCP_EXPORT size_t nimcp_graph_dao_count(
    nimcp_gpu_graph_dao_t* dao,
    const char* filter
);

//=============================================================================
// Synchronization
//=============================================================================

/**
 * @brief Sync dirty cache entries to GPU
 *
 * @param dao DAO instance
 * @return NIMCP_SUCCESS or error code
 */
NIMCP_EXPORT nimcp_error_t nimcp_graph_dao_sync_to_gpu(
    nimcp_gpu_graph_dao_t* dao
);

/**
 * @brief Sync GPU graphs to host cache
 *
 * @param dao DAO instance
 * @return NIMCP_SUCCESS or error code
 */
NIMCP_EXPORT nimcp_error_t nimcp_graph_dao_sync_from_gpu(
    nimcp_gpu_graph_dao_t* dao
);

/**
 * @brief Flush all cached data
 *
 * @param dao DAO instance
 * @return NIMCP_SUCCESS or error code
 */
NIMCP_EXPORT nimcp_error_t nimcp_graph_dao_flush(
    nimcp_gpu_graph_dao_t* dao
);

//=============================================================================
// Cache Management
//=============================================================================

/**
 * @brief Get cache statistics
 *
 * @param dao DAO instance
 * @param out_hits Output: cache hits
 * @param out_misses Output: cache misses
 * @param out_count Output: current cache entries
 */
NIMCP_EXPORT void nimcp_graph_dao_cache_stats(
    const nimcp_gpu_graph_dao_t* dao,
    uint64_t* out_hits,
    uint64_t* out_misses,
    size_t* out_count
);

/**
 * @brief Clear the cache (evict all entries)
 *
 * @param dao DAO instance
 * @param sync_first If true, sync dirty entries first
 * @return NIMCP_SUCCESS or error code
 */
NIMCP_EXPORT nimcp_error_t nimcp_graph_dao_cache_clear(
    nimcp_gpu_graph_dao_t* dao,
    bool sync_first
);

/**
 * @brief Prefetch graphs into cache
 *
 * @param dao DAO instance
 * @param ids Graph IDs to prefetch
 * @param count Number of IDs
 * @return Number of successfully prefetched graphs
 */
NIMCP_EXPORT size_t nimcp_graph_dao_prefetch(
    nimcp_gpu_graph_dao_t* dao,
    const int* ids,
    size_t count
);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Check if graph exists in DAO
 *
 * @param dao DAO instance
 * @param id Graph ID
 * @return true if graph exists
 */
NIMCP_EXPORT bool nimcp_graph_dao_exists(
    const nimcp_gpu_graph_dao_t* dao,
    int id
);

/**
 * @brief Get DAO storage statistics
 *
 * @param dao DAO instance
 * @param out_total Total graphs stored
 * @param out_gpu Graphs in GPU memory
 * @param out_cached Graphs in host cache
 */
NIMCP_EXPORT void nimcp_graph_dao_stats(
    const nimcp_gpu_graph_dao_t* dao,
    size_t* out_total,
    size_t* out_gpu,
    size_t* out_cached
);

/**
 * @brief List all graph IDs
 *
 * @param dao DAO instance
 * @param out_ids Output: array of IDs (caller allocated)
 * @param max_count Maximum IDs to return
 * @return Number of IDs written
 */
NIMCP_EXPORT size_t nimcp_graph_dao_list_ids(
    const nimcp_gpu_graph_dao_t* dao,
    int* out_ids,
    size_t max_count
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_GRAPH_DAO_H */
