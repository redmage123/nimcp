//=============================================================================
// nimcp_distributed_cow.h - Distributed Copy-on-Write Brain Cloning
//=============================================================================
/**
 * @file nimcp_distributed_cow.h
 * @brief Distributed Copy-on-Write (COW) brain cloning across network nodes
 *
 * WHAT: Extends local COW to allow brain clones on different machines
 *       while sharing network weights over the network.
 *
 * WHY:  Enable efficient distributed brain deployment:
 *       - Deploy same model to multiple nodes with minimal bandwidth
 *       - Share 86% of brain memory across network
 *       - Lazy network fetching (only load neurons needed for inference)
 *       - Write operations trigger local COW with full network fetch
 *
 * HOW:  Network protocol for COW sharing:
 *       1. Master node holds original network
 *       2. Remote nodes create COW clones that fetch data on demand
 *       3. Cache fetched network segments locally
 *       4. Reference counting across nodes
 *       5. Compression for network transfer
 *
 * ARCHITECTURE:
 *
 *   ┌──────────────────────────────────────────────────────────┐
 *   │                    MASTER NODE                            │
 *   │  ┌──────────────────────────────────────────────┐        │
 *   │  │  Original Brain (Full Network)               │        │
 *   │  │  - Complete network weights                  │        │
 *   │  │  - Reference count tracking                  │        │
 *   │  │  - Serves network segments on demand         │        │
 *   │  └──────────────────────────────────────────────┘        │
 *   │           │                                               │
 *   │           │ P2P Network (NIMCP Protocol)                 │
 *   └───────────┼───────────────────────────────────────────────┘
 *               │
 *     ┌─────────┴─────────┬──────────────┬──────────────┐
 *     │                   │              │              │
 *     ▼                   ▼              ▼              ▼
 * ┌───────┐           ┌───────┐      ┌───────┐      ┌───────┐
 * │REMOTE │           │REMOTE │      │REMOTE │      │REMOTE │
 * │NODE 1 │           │NODE 2 │      │NODE 3 │      │NODE N │
 * │       │           │       │      │       │      │       │
 * │COW    │           │COW    │      │COW    │      │COW    │
 * │Clone  │           │Clone  │      │Clone  │      │Clone  │
 * │       │           │       │      │       │      │       │
 * │Cache  │           │Cache  │      │Cache  │      │Cache  │
 * └───────┘           └───────┘      └───────┘      └───────┘
 *
 * PROTOCOL MESSAGES:
 * - CTRL_MSG_COW_FETCH_SEGMENT: Fetch network segment (neurons, synapses)
 * - CTRL_MSG_COW_REFCOUNT_INC: Increment remote reference count
 * - CTRL_MSG_COW_REFCOUNT_DEC: Decrement remote reference count
 * - CTRL_MSG_COW_FULL_FETCH: Trigger full network fetch (for writes)
 *
 * PERFORMANCE:
 * - Network overhead: O(S) where S = segment size
 * - Lazy loading: Only fetch neurons needed for inference
 * - Compression: ~70% reduction in network transfer
 * - Caching: O(1) access after first fetch
 *
 * MEMORY SAVINGS:
 * - Without distributed COW: N nodes × 50MB = 500MB (for 10 nodes)
 * - With distributed COW: 50MB (master) + 10 × 7MB (caches) = 120MB
 * - Savings: 76% reduction in total cluster memory
 *
 * THREAD SAFETY:
 * - All operations protected by rwlock
 * - Network fetches are serialized
 * - Cache updates are atomic
 *
 * @author NIMCP Development Team
 * @date 2025-11-09
 * @version 2.8.0
 */

#ifndef NIMCP_DISTRIBUTED_COW_H
#define NIMCP_DISTRIBUTED_COW_H

#include <stdbool.h>
#include <stdint.h>
#include "common/nimcp_export.h"
#include "core/brain/nimcp_brain.h"
#include "networking/p2p/nimcp_p2pnode.h"
#include "plasticity/adaptive/nimcp_adaptive.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/platform/nimcp_platform_rwlock.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Configuration and Types
//=============================================================================

/**
 * @brief Distributed COW configuration
 */
typedef struct distributed_cow_config {
    // Network settings
    uint32_t segment_size;           /**< Size of network segments to fetch (neurons) */
    uint32_t cache_capacity_mb;      /**< Maximum cache size in MB */
    uint32_t fetch_timeout_ms;       /**< Timeout for network fetch operations */
    uint32_t refcount_sync_interval_ms; /**< Reference count sync interval */

    // Compression settings
    bool enable_compression;         /**< Enable network compression */
    float compression_threshold;     /**< Min weight magnitude for transmission */

    // Caching strategy
    bool enable_prefetch;            /**< Enable predictive prefetching */
    uint32_t prefetch_lookahead;     /**< Neurons to prefetch ahead */

    // Performance tuning
    uint32_t max_concurrent_fetches; /**< Max concurrent network fetches */
    bool aggressive_caching;         /**< Cache entire network on first access */
} distributed_cow_config_t;

/**
 * @brief Network segment descriptor
 */
typedef struct {
    uint32_t start_neuron_id;   /**< First neuron in segment */
    uint32_t num_neurons;       /**< Number of neurons in segment */
    uint32_t num_synapses;      /**< Total synapses in segment */
    uint64_t segment_id;        /**< Unique segment identifier */
    uint64_t timestamp;         /**< Fetch timestamp */
    bool is_compressed;         /**< Is segment compressed? */
    size_t compressed_size;     /**< Compressed data size */
    size_t uncompressed_size;   /**< Uncompressed data size */
} network_segment_t;

/**
 * @brief Distributed COW state
 */
typedef struct {
    // Network information
    bool is_distributed;        /**< Is this a distributed clone? */
    bool is_master;             /**< Is this the master node? */
    char master_host[256];      /**< Master node hostname/IP */
    uint16_t master_port;       /**< Master node port */

    // P2P connection
    p2p_node_t p2p_node;        /**< P2P network node */

    // Reference counting
    uint32_t local_refcount;    /**< Local reference count */
    uint32_t remote_refcount;   /**< Remote reference count (master only) */
    uint64_t clone_id;          /**< Unique clone identifier */

    // Segment cache
    network_segment_t** cached_segments; /**< Array of cached segments */
    uint32_t num_cached_segments;        /**< Number of cached segments */
    uint32_t cache_capacity;             /**< Max cached segments */
    size_t cache_size_bytes;             /**< Current cache size */

    // Fetch statistics
    uint64_t total_fetches;          /**< Total network fetches */
    uint64_t total_bytes_fetched;    /**< Total bytes transferred */
    uint64_t cache_hits;             /**< Cache hits */
    uint64_t cache_misses;           /**< Cache misses */
    float avg_fetch_latency_ms;      /**< Average fetch latency */

    // Configuration
    distributed_cow_config_t config; /**< Configuration */

    // Synchronization
    nimcp_platform_rwlock_t cache_lock;     /**< Cache access lock */
    nimcp_platform_mutex_t fetch_mutex;     /**< Fetch operation mutex */
} distributed_cow_state_t;

/**
 * @brief Distributed COW statistics
 */
typedef struct distributed_cow_stats {
    bool is_distributed;             /**< Is distributed clone? */
    bool is_master;                  /**< Is master node? */
    uint32_t local_refcount;         /**< Local reference count */
    uint32_t remote_refcount;        /**< Remote reference count */
    uint32_t num_cached_segments;    /**< Cached segments */
    size_t cache_size_bytes;         /**< Cache memory usage */
    uint64_t total_fetches;          /**< Total fetches */
    uint64_t total_bytes_fetched;    /**< Total bytes transferred */
    uint64_t cache_hits;             /**< Cache hits */
    uint64_t cache_misses;           /**< Cache misses */
    float cache_hit_rate;            /**< Cache hit rate (0-1) */
    float avg_fetch_latency_ms;      /**< Average fetch latency */
    float network_bandwidth_mbps;    /**< Network bandwidth usage */
} distributed_cow_stats_t;

//=============================================================================
// Distributed COW API
//=============================================================================

/**
 * @brief Create distributed COW clone on remote node
 *
 * WHAT: Creates a COW clone that fetches network data from remote master
 * WHY:  Enable efficient distributed deployment without full network copy
 * HOW:  Establishes P2P connection, initializes cache, registers with master
 *
 * @param original Brain to clone (must be on master node)
 * @param remote_host Master node hostname/IP
 * @param remote_port Master node port
 * @param config Distributed COW configuration (NULL for defaults)
 * @return Distributed COW clone or NULL on error
 *
 * NETWORK PROTOCOL:
 * 1. Connect to master via P2P
 * 2. Send CTRL_MSG_COW_CREATE_CLONE with brain ID
 * 3. Master responds with clone_id and metadata
 * 4. Increment remote reference count
 * 5. Initialize local cache
 *
 * MEMORY: ~1-10MB overhead (cache + metadata)
 * LATENCY: ~10-100ms (connection + handshake)
 *
 * THREAD SAFETY: Thread-safe
 */
NIMCP_EXPORT brain_t brain_clone_cow_distributed(
    brain_t original,
    const char* remote_host,
    uint16_t remote_port,
    const distributed_cow_config_t* config
);

/**
 * @brief Enable distributed COW serving on master node
 *
 * WHAT: Configures brain to serve network segments to remote clones
 * WHY:  Allow brain to act as master for distributed COW
 * HOW:  Registers network segment handlers, starts P2P listener
 *
 * @param brain Brain to enable as master
 * @param p2p_node P2P node for serving (must be started)
 * @return true on success, false on failure
 *
 * THREAD SAFETY: Thread-safe
 */
NIMCP_EXPORT bool brain_enable_distributed_cow_master(
    brain_t brain,
    p2p_node_t p2p_node
);

/**
 * @brief Fetch network segment from master
 *
 * WHAT: Fetches neurons and synapses for specific segment from master
 * WHY:  Lazy loading - only fetch what's needed
 * HOW:  Send CTRL_MSG_COW_FETCH_SEGMENT, receive compressed data
 *
 * @param brain Distributed COW clone
 * @param start_neuron_id First neuron to fetch
 * @param num_neurons Number of neurons to fetch
 * @return true on success, false on failure
 *
 * NETWORK PROTOCOL:
 * 1. Send fetch request with segment range
 * 2. Master serializes network segment
 * 3. Compress segment data (if enabled)
 * 4. Transfer over P2P
 * 5. Decompress and cache locally
 *
 * COMPLEXITY: O(S) where S = segment size
 * LATENCY: ~1-50ms depending on segment size and network
 *
 * THREAD SAFETY: Thread-safe (fetch mutex)
 */
NIMCP_EXPORT bool distributed_cow_fetch_segment(
    brain_t brain,
    uint32_t start_neuron_id,
    uint32_t num_neurons
);

/**
 * @brief Prefetch network segments predictively
 *
 * WHAT: Prefetch segments likely to be needed soon
 * WHY:  Reduce inference latency by fetching ahead
 * HOW:  Analyzes access patterns, fetches adjacent segments
 *
 * @param brain Distributed COW clone
 * @param current_neuron_id Current neuron being accessed
 * @return Number of segments prefetched
 *
 * ALGORITHM:
 * - Identify active neurons from recent inferences
 * - Fetch adjacent segments within prefetch_lookahead
 * - Respect cache capacity limits
 *
 * THREAD SAFETY: Thread-safe (spawns background fetch)
 */
NIMCP_EXPORT uint32_t distributed_cow_prefetch_segments(
    brain_t brain,
    uint32_t current_neuron_id
);

/**
 * @brief Trigger full network fetch (for write operations)
 *
 * WHAT: Fetches entire network from master to local node
 * WHY:  Prepare for write operation (learning, fine-tuning)
 * HOW:  Fetches all segments, creates local writable copy
 *
 * @param brain Distributed COW clone
 * @return true on success, false on failure
 *
 * BEHAVIOR:
 * - Fetches all missing segments
 * - Creates local writable network copy
 * - Transitions from distributed to local COW
 * - Decrements remote reference count
 *
 * LATENCY: ~100-1000ms depending on network size
 * MEMORY: Allocates full network memory (~50MB typical)
 *
 * THREAD SAFETY: Thread-safe
 */
NIMCP_EXPORT bool distributed_cow_fetch_full_network(brain_t brain);

/**
 * @brief Get distributed COW statistics
 *
 * @param brain Brain handle
 * @param stats Output statistics structure
 * @return true on success, false if not distributed COW
 *
 * THREAD SAFETY: Thread-safe (read lock)
 */
NIMCP_EXPORT bool brain_get_distributed_cow_stats(
    brain_t brain,
    distributed_cow_stats_t* stats
);

/**
 * @brief Check if brain is distributed COW clone
 *
 * @param brain Brain handle
 * @return true if distributed COW clone, false otherwise
 */
NIMCP_EXPORT bool brain_is_distributed_cow(brain_t brain);

/**
 * @brief Clear distributed COW cache
 *
 * WHAT: Evicts cached network segments to free memory
 * WHY:  Manage memory footprint in memory-constrained environments
 * HOW:  Evicts least-recently-used segments
 *
 * @param brain Distributed COW clone
 * @param target_size_mb Target cache size in MB (0 = clear all)
 * @return Bytes freed
 *
 * EVICTION POLICY: LRU (Least Recently Used)
 *
 * THREAD SAFETY: Thread-safe (write lock)
 */
NIMCP_EXPORT size_t distributed_cow_clear_cache(
    brain_t brain,
    uint32_t target_size_mb
);

//=============================================================================
// Protocol Message Types (Extends nimcp_protocol.h)
//=============================================================================

/**
 * @brief Distributed COW protocol message types
 *
 * NOTE: These extend the CTRL_MSG_* enum from nimcp_protocol.h
 */
#define CTRL_MSG_COW_CREATE_CLONE   0x20  /**< Create distributed clone */
#define CTRL_MSG_COW_FETCH_SEGMENT  0x21  /**< Fetch network segment */
#define CTRL_MSG_COW_REFCOUNT_INC   0x22  /**< Increment reference count */
#define CTRL_MSG_COW_REFCOUNT_DEC   0x23  /**< Decrement reference count */
#define CTRL_MSG_COW_FULL_FETCH     0x24  /**< Fetch full network */
#define CTRL_MSG_COW_SEGMENT_DATA   0x25  /**< Network segment data response */
#define CTRL_MSG_COW_ERROR          0x26  /**< Error response */

/**
 * @brief COW fetch segment request payload
 */
typedef struct __attribute__((packed)) {
    uint64_t brain_id;          /**< Brain identifier */
    uint64_t clone_id;          /**< Clone identifier */
    uint32_t start_neuron_id;   /**< First neuron to fetch */
    uint32_t num_neurons;       /**< Number of neurons */
    bool enable_compression;    /**< Compress response? */
    uint8_t reserved[3];        /**< Alignment */
} cow_fetch_segment_request_t;

/**
 * @brief COW segment data response payload
 */
typedef struct __attribute__((packed)) {
    uint64_t segment_id;        /**< Segment identifier */
    uint32_t start_neuron_id;   /**< First neuron in segment */
    uint32_t num_neurons;       /**< Number of neurons */
    uint32_t num_synapses;      /**< Number of synapses */
    bool is_compressed;         /**< Is data compressed? */
    uint8_t reserved[3];        /**< Alignment */
    uint32_t data_length;       /**< Length of following data */
    // Followed by serialized network data (neurons + synapses)
} cow_segment_data_response_t;

/**
 * @brief COW reference count message payload
 */
typedef struct __attribute__((packed)) {
    uint64_t brain_id;          /**< Brain identifier */
    uint64_t clone_id;          /**< Clone identifier */
    int32_t delta;              /**< Refcount change (+1 or -1) */
} cow_refcount_msg_t;

//=============================================================================
// Default Configuration
//=============================================================================

/**
 * @brief Get default distributed COW configuration
 *
 * @return Default configuration
 */
static inline distributed_cow_config_t distributed_cow_default_config(void)
{
    distributed_cow_config_t config = {
        .segment_size = 1024,              // 1K neurons per segment
        .cache_capacity_mb = 10,           // 10MB cache
        .fetch_timeout_ms = 5000,          // 5 second timeout
        .refcount_sync_interval_ms = 10000, // 10 second sync
        .enable_compression = true,        // Enable compression
        .compression_threshold = 0.01f,    // Drop weights < 0.01
        .enable_prefetch = true,           // Enable prefetching
        .prefetch_lookahead = 2048,        // Prefetch 2K neurons ahead
        .max_concurrent_fetches = 4,       // 4 concurrent fetches
        .aggressive_caching = false        // Don't cache entire network
    };
    return config;
}

#ifdef __cplusplus
}
#endif

#endif  // NIMCP_DISTRIBUTED_COW_H
