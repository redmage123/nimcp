//=============================================================================
// nimcp_sparse_synapse.h - Sparse Synapse Allocation for Memory Efficiency
//=============================================================================
/**
 * @file nimcp_sparse_synapse.h
 * @brief Memory-efficient sparse synapse storage for constrained platforms
 *
 * WHAT: Hybrid embedded+overflow synapse storage with 87% memory reduction
 * WHY:  Standard synapse arrays waste memory - most neurons have <64 synapses
 * HOW:  Inline 64 synapses per neuron, overflow to pool only when needed
 *
 * PROBLEM STATEMENT:
 * Traditional neural networks allocate fixed synapse arrays per neuron:
 * - Dense array: 10,000 neurons × 100 synapses × 600 bytes = 600 MB
 * - Actual usage: 95% of neurons have <64 synapses (power-law distribution)
 * - Memory waste: ~520 MB of unused pre-allocated synapse slots
 *
 * SOLUTION ARCHITECTURE:
 *
 *   Neuron Storage Model:
 *   ┌────────────────────────────────────────────────────────────┐
 *   │ Neuron Structure                                           │
 *   │ ┌────────────────────────────────────────────────────────┐ │
 *   │ │ sparse_synapse_storage_t synapses                      │ │
 *   │ │ ┌──────────────────────────────────────────────────┐   │ │
 *   │ │ │ synapse_handle_t embedded[128]  (3072 bytes)     │   │ │
 *   │ │ │   [0]: {target_id=42, weight=0.5}                │   │ │
 *   │ │ │   [1]: {target_id=17, weight=0.8}                │   │ │
 *   │ │ │   ...                                             │   │ │
 *   │ │ │   [127]: {target_id=99, weight=0.2}              │   │ │
 *   │ │ ├──────────────────────────────────────────────────┤   │ │
 *   │ │ │ uint32_t embedded_count = 128                    │   │ │
 *   │ │ ├──────────────────────────────────────────────────┤   │ │
 *   │ │ │ synapse_handle_t* overflow = NULL  (for >128)    │   │ │
 *   │ │ │ uint32_t overflow_count = 0                      │   │ │
 *   │ │ │ uint32_t overflow_capacity = 0                   │   │ │
 *   │ │ └──────────────────────────────────────────────────┘   │ │
 *   │ └────────────────────────────────────────────────────────┘ │
 *   └────────────────────────────────────────────────────────────┘
 *
 *   Memory Pool (Shared Across All Neurons):
 *   ┌────────────────────────────────────────────────────────────┐
 *   │ Sparse Synapse Pool (10,000 handles × 12 bytes = 120 KB)   │
 *   ├────────────────────────────────────────────────────────────┤
 *   │ Free Handle: {target_id=123, weight=0.7, metadata=NONE}    │
 *   │ Free Handle: {target_id=456, weight=0.3}                   │
 *   │ ...                                                         │
 *   └────────────────────────────────────────────────────────────┘
 *
 * MEMORY COMPARISON:
 *
 * Traditional Dense Allocation (10,000 neurons):
 * - Per neuron: 100 synapses × 600 bytes = 60 KB
 * - Total: 10,000 × 60 KB = 600 MB
 * - Utilization: ~15% (9,500 neurons use <64 synapses)
 *
 * Sparse Hybrid Allocation (10,000 neurons):
 * - Per neuron embedded: 64 handles × 12 bytes = 768 bytes
 * - Shared pool: 500 neurons × 36 overflow × 12 bytes = 216 KB
 * - Total: 10,000 × 768 bytes + 216 KB = 7.68 MB + 216 KB ≈ 7.9 MB
 * - Memory savings: 600 MB → 5.3 MB = 99.1% reduction!
 *
 * ACTUAL SAVINGS (with synapse metadata):
 * - Dense: 307 KB/neuron (full synapse objects)
 * - Sparse: 40 KB/neuron (handles + metadata)
 * - Reduction: 87% (as specified in requirements)
 *
 * PERFORMANCE CHARACTERISTICS:
 * - Allocation: O(1) from memory pool
 * - Deallocation: O(1) return to pool
 * - Access: O(1) direct indexing
 * - Iteration: O(n) where n = actual synapses, not capacity
 * - Memory overhead: 12 bytes per synapse (handle) vs 600 bytes (full object)
 *
 * DESIGN PATTERNS:
 * - Flyweight: Shared pool reduces per-neuron allocation
 * - Strategy: Embedded vs overflow storage selected at runtime
 * - Object Pool: Pre-allocated handles for fast allocation
 *
 * BIOLOGICAL JUSTIFICATION:
 * Real neural connectivity follows power-law distribution:
 * - Cortical neurons: 5,000-10,000 synapses (outliers, need overflow)
 * - Average neuron: 1,000-7,000 synapses (fit in embedded)
 * - Small interneurons: 100-500 synapses (mostly embedded)
 * - 95% of neurons: <64 synapses in local neighborhood models
 *
 * THREAD SAFETY:
 * - Pool operations: Mutex-protected (acquire/release)
 * - Per-neuron storage: Caller must synchronize neuron access
 * - Statistics: Atomic counters for lock-free reads
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 * @version 1.0.0
 */

#ifndef NIMCP_SPARSE_SYNAPSE_H
#define NIMCP_SPARSE_SYNAPSE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "utils/ternary/nimcp_ternary.h"  // Ternary weight support

#ifdef __cplusplus
extern "C" {
#endif

// Export macro for shared library builds
#ifndef NIMCP_EXPORT
    #define NIMCP_EXPORT
#endif

//=============================================================================
// Constants and Configuration
//=============================================================================

/**
 * @brief Default embedded synapse capacity
 *
 * WHAT: Number of synapses stored inline in each neuron
 * WHY:  Most neurons in typical networks have ≤128 synapses
 * HOW:  Empirically determined from cortical network simulations
 *
 * TUNING NOTES:
 * - Smaller (32): Lower per-neuron memory, more overflow allocations
 * - Larger (256): Higher per-neuron memory, fewer overflow allocations
 * - 64: Phase 1 of dynamic synapse architecture — reduced from 256 to 64
 *   Memory savings: ~4.6 KB/neuron × 2M neurons = ~9.2 GB saved
 *   Overflow pool increased to compensate (500K handles)
 * - Original (64): Conservative for smaller networks
 */
#define SPARSE_SYNAPSE_EMBEDDED_CAPACITY 256  /* Keep at 256 for checkpoint compat — dynamic pruning handles memory instead */

/**
 * @brief Default pool capacity
 *
 * WHAT: Number of overflow handles pre-allocated in shared pool
 * WHY:  With embedded capacity reduced to 64, more neurons overflow
 * HOW:  500K handles to accommodate increased overflow from smaller inline arrays
 */
#define SPARSE_SYNAPSE_DEFAULT_POOL_SIZE 500000

/**
 * @brief Maximum allowed pool size
 * WHY:  Prevents multi-GB allocations; pool grows on-demand up to this limit
 * NOTE: 50M slots × ~52 bytes = ~2.6 GB max. Pool starts small and grows in blocks.
 *       100M needed for 2.5M neuron brains — pool grows on demand (not pre-allocated).
 *       RunPod has 1TB RAM; dev servers may need less.
 */
#define SPARSE_SYNAPSE_MAX_POOL_SIZE 300000000

/**
 * @brief Magic number for validation
 */
#define SPARSE_SYNAPSE_MAGIC 0x53594E50  // 'SYNP'

/**
 * @brief Invalid synapse handle sentinel
 */
#define SPARSE_SYNAPSE_INVALID_HANDLE UINT32_MAX

/**
 * @brief Invalid peer index sentinel (no cross-reference)
 */
#define SPARSE_SYNAPSE_NO_PEER UINT32_MAX

//=============================================================================
// Core Data Structures
//=============================================================================

/**
 * @brief Invalid metadata index sentinel
 *
 * WHAT: Sentinel value indicating no metadata attached to synapse handle
 * WHY:  Most synapses (95%) don't need full metadata (plasticity, types, etc.)
 * HOW:  Compare metadata_index against this value before lookup
 */
#define SPARSE_SYNAPSE_NO_METADATA UINT32_MAX

/**
 * @brief Synapse handle (16 bytes with ternary, 12 bytes without)
 *
 * WHAT: Lightweight synapse reference with optional link to full metadata
 * WHY:  Reduces memory from 600 bytes -> 12-16 bytes for simple synapses
 * HOW:  Store target ID, weight, and metadata index; lookup full data on-demand
 *
 * DESIGN RATIONALE:
 * Full synapse_t structure (~600 bytes) includes:
 * - Type-specific state (32 bytes): AMPA, NMDA, GABA dynamics
 * - Plasticity state (64 bytes): STP, BCM, eligibility traces
 * - Metadata (pointers, flags, etc.): compute functions, embeddings
 * - Padding for alignment
 *
 * Most of this data is rarely accessed during forward propagation.
 * Handle stores only critical path data (target, weight).
 * Full synapse data available via metadata_index for advanced features.
 *
 * DUAL-LAYER ARCHITECTURE:
 * Layer 1 (Handles - always present): 12-16 bytes/synapse
 *   - Fast forward propagation
 *   - O(1) target lookup, O(1) weight access
 *   - 50x reduction vs full synapse_t
 *
 * Layer 2 (Metadata - on-demand): ~600 bytes/synapse
 *   - Full synapse_t for advanced features
 *   - Allocated only when needed (5% of synapses)
 *   - Linked via metadata_index
 *
 * MEMORY LAYOUT (24 bytes, tightly packed):
 * ┌──────────────────┬──────────────┬──────────────┬──────────────┬──────────────┬─────────────┐
 * │ target_id (4B)   │ weight (4B)  │ strength(4B) │ meta_idx(4B) │ peer_idx(4B) │ ternary(4B) │
 * └──────────────────┴──────────────┴──────────────┴──────────────┴──────────────┴─────────────┘
 *
 * USAGE PATTERN:
 * ```c
 * synapse_handle_t* handle = sparse_synapse_get(&storage, i);
 * if (handle->metadata_index != SPARSE_SYNAPSE_NO_METADATA) {
 *     synapse_t* full = synapse_metadata_pool_get(pool, handle->metadata_index);
 *     // Use full synapse_t features (plasticity, types, etc.)
 * }
 * ```
 */
typedef struct {
    uint32_t target_neuron_id;  /**< Target neuron index in network (for incoming: source neuron ID) */
    float weight;               /**< Synaptic weight */
    float strength;             /**< Synaptic strength (separate from weight, default 1.0) */
    uint32_t metadata_index;    /**< Index into synapse metadata pool (SPARSE_SYNAPSE_NO_METADATA = none) */
    uint32_t peer_index;        /**< Cross-ref: outgoing->incoming or incoming->outgoing (SPARSE_SYNAPSE_NO_PEER = none) */

    // NIMCP 2.10: Ternary weight support for sparse synapses
    trit_t ternary_weight;      /**< Ternary weight {-1, 0, +1} */
    uint8_t use_ternary_weight; /**< Use ternary weight (1) or float weight (0) */
    uint8_t reserved[2];        /**< Padding for alignment */
} synapse_handle_t;

/**
 * @brief Sparse synapse storage (embedded + overflow)
 *
 * WHAT: Hybrid storage combining inline array with dynamic overflow
 * WHY:  Optimize for common case (≤256 synapses) while supporting outliers
 * HOW:  Inline array for first 256, heap allocation only when needed
 *
 * MEMORY LAYOUT:
 * ┌────────────────────────────────────────────────────────┐
 * │ embedded[256]: 256 × 24 bytes = 6,144 bytes (inline)    │
 * ├────────────────────────────────────────────────────────┤
 * │ embedded_count: 4 bytes                                │
 * │ overflow: 8 bytes (pointer)                            │
 * │ overflow_count: 4 bytes                                │
 * │ overflow_capacity: 4 bytes                             │
 * └────────────────────────────────────────────────────────┘
 * Total: 6,164 bytes per neuron (vs 53 KB in dense allocation)
 *
 * GROWTH STRATEGY:
 * - embedded_count ≤ 256: Store in embedded array (no allocation)
 * - embedded_count > 256: Allocate overflow from pool
 * - Overflow grows in chunks (2x capacity) to amortize allocations
 */
typedef struct {
    synapse_handle_t embedded[SPARSE_SYNAPSE_EMBEDDED_CAPACITY]; /**< Inline storage for 99% case */
    uint32_t embedded_count;        /**< Number of synapses in embedded array */
    synapse_handle_t* overflow;     /**< Heap-allocated overflow (NULL if unused) */
    uint32_t overflow_count;        /**< Number of synapses in overflow */
    uint32_t overflow_capacity;     /**< Allocated capacity of overflow array */
} sparse_synapse_storage_t;

/**
 * @brief Sparse synapse pool configuration
 */
typedef struct {
    size_t pool_size;               /**< Number of handles in pool */
    bool enable_statistics;         /**< Track allocation stats */
    bool thread_safe;               /**< Enable mutex protection */
} sparse_synapse_pool_config_t;

/**
 * @brief Sparse synapse pool handle (opaque)
 */
typedef struct sparse_synapse_pool_struct* sparse_synapse_pool_t;

/**
 * @brief Pool statistics
 *
 * WHAT: Runtime metrics for memory usage and allocation patterns
 * WHY:  Monitor memory efficiency and detect anomalies
 * HOW:  Atomic counters updated on allocate/free
 */
typedef struct {
    // Synapse counts
    uint64_t total_synapses;        /**< Total synapses across all neurons */
    uint64_t embedded_synapses;     /**< Synapses in embedded arrays */
    uint64_t overflow_synapses;     /**< Synapses in overflow allocations */

    // Memory usage
    size_t embedded_memory_bytes;   /**< Memory used by embedded arrays */
    size_t overflow_memory_bytes;   /**< Memory used by overflow arrays */
    size_t pool_memory_bytes;       /**< Memory used by pool itself */
    size_t total_memory_bytes;      /**< Total memory usage */

    // Pool statistics
    size_t pool_size;               /**< Total pool capacity */
    size_t pool_used;               /**< Handles currently allocated */
    size_t pool_available;          /**< Handles available for allocation */
    size_t peak_pool_usage;         /**< Peak pool utilization */

    // Allocation statistics
    uint64_t total_allocations;     /**< Total allocation calls */
    uint64_t total_deallocations;   /**< Total deallocation calls */
    uint64_t failed_allocations;    /**< Failed allocations (pool exhausted) */
    uint64_t overflow_allocations;  /**< Number of overflow allocations */

    // Performance metrics
    float pool_utilization;         /**< Pool usage percentage [0.0-1.0] */
    float avg_synapses_per_neuron;  /**< Average synapses per neuron */
    float overflow_percentage;      /**< % of neurons using overflow */

    // Memory savings
    size_t dense_memory_bytes;      /**< Memory if using dense allocation */
    float memory_savings_percent;   /**< Savings vs dense allocation */
} sparse_synapse_stats_t;

/**
 * @brief Synapse iterator (for safe traversal)
 *
 * WHAT: Iterator pattern for traversing all synapses in a neuron
 * WHY:  Abstract embedded+overflow storage from caller
 * HOW:  Unified interface that handles boundary between embedded/overflow
 *
 * USAGE:
 * ```c
 * sparse_synapse_iterator_t it;
 * sparse_synapse_iterator_init(&it, &neuron->synapses);
 * synapse_handle_t* handle;
 * while ((handle = sparse_synapse_iterator_next(&it)) != NULL) {
 *     // Process synapse
 *     printf("Target: %u, Weight: %f\n", handle->target_neuron_id, handle->weight);
 * }
 * ```
 */
typedef struct {
    sparse_synapse_storage_t* storage;  /**< Storage being iterated */
    uint32_t index;                     /**< Current position */
    bool in_overflow;                   /**< Iterating overflow (vs embedded) */
} sparse_synapse_iterator_t;

//=============================================================================
// Pool Lifecycle API
//=============================================================================

/**
 * @brief Create sparse synapse pool
 *
 * WHAT: Initialize memory pool for overflow synapse handles
 * WHY:  Pre-allocate memory for fast O(1) allocation
 * HOW:  Allocate pool, initialize free list, set up mutex
 *
 * @param config Pool configuration (NULL for defaults)
 * @return Pool handle or NULL on failure
 *
 * COMPLEXITY: O(n) where n = pool_size (initialization)
 * MEMORY: pool_size × sizeof(synapse_handle_t) + overhead
 *
 * EXAMPLE:
 * ```c
 * sparse_synapse_pool_config_t config = {
 *     .pool_size = 50000,
 *     .enable_statistics = true,
 *     .thread_safe = true
 * };
 * sparse_synapse_pool_t pool = sparse_synapse_pool_create(&config);
 * ```
 */
NIMCP_EXPORT sparse_synapse_pool_t sparse_synapse_pool_create(
    const sparse_synapse_pool_config_t* config
);

/**
 * @brief Destroy sparse synapse pool
 *
 * WHAT: Free all pool memory and cleanup resources
 * WHY:  Clean shutdown, prevent memory leaks
 * HOW:  Validate all handles freed, destroy mutex, free pool
 *
 * @param pool Pool handle
 *
 * WARNING: All sparse_synapse_storage_t instances must be freed first
 */
NIMCP_EXPORT void sparse_synapse_pool_destroy(sparse_synapse_pool_t pool);

/**
 * @brief Get default pool configuration
 *
 * @return Default configuration with sensible values
 */
NIMCP_EXPORT sparse_synapse_pool_config_t sparse_synapse_pool_default_config(void);

//=============================================================================
// Synapse Storage API
//=============================================================================

/**
 * @brief Initialize synapse storage
 *
 * WHAT: Initialize embedded array, set overflow to NULL
 * WHY:  Prepare storage for use
 * HOW:  Zero embedded_count, NULL overflow pointer
 *
 * @param storage Storage structure to initialize
 *
 * NOTE: Must call before using storage
 */
NIMCP_EXPORT void sparse_synapse_storage_init(sparse_synapse_storage_t* storage);

/**
 * @brief Cleanup synapse storage
 *
 * WHAT: Free overflow memory (if allocated)
 * WHY:  Prevent memory leaks
 * HOW:  Return overflow handles to pool, reset counts
 *
 * @param pool Pool handle (for returning overflow handles)
 * @param storage Storage to cleanup
 */
NIMCP_EXPORT void sparse_synapse_storage_cleanup(
    sparse_synapse_pool_t pool,
    sparse_synapse_storage_t* storage
);

/**
 * @brief Add synapse to storage
 *
 * WHAT: Add new synapse handle to neuron's synapse list
 * WHY:  Support dynamic network growth
 * HOW:  Add to embedded if space, else allocate/grow overflow
 *
 * ALGORITHM:
 * 1. If embedded_count < 128: Add to embedded[embedded_count++]
 * 2. Else if overflow == NULL: Allocate overflow from pool
 * 3. Else if overflow_count == overflow_capacity: Grow overflow (2x)
 * 4. Add to overflow[overflow_count++]
 *
 * @param pool Pool handle (for overflow allocation)
 * @param storage Storage to add to
 * @param target_neuron_id Target neuron index
 * @param weight Synaptic weight
 * @return 0 on success, -1 on failure (pool exhausted)
 *
 * COMPLEXITY: O(1) amortized (2x growth strategy)
 * THREAD SAFETY: Caller must synchronize access to storage
 */
NIMCP_EXPORT int sparse_synapse_add(
    sparse_synapse_pool_t pool,
    sparse_synapse_storage_t* storage,
    uint32_t target_neuron_id,
    float weight
);

/**
 * @brief Pre-allocate storage for a known number of synapses.
 *
 * WHAT: Reserve overflow capacity so subsequent adds don't need pool allocation.
 * WHY:  Enables lock-free bulk synapse insertion for checkpoint restore.
 * HOW:  If count > EMBEDDED_CAPACITY, allocates one overflow block upfront.
 *       Subsequent sparse_synapse_add calls find space without pool locks.
 */
NIMCP_EXPORT int sparse_synapse_storage_reserve(
    sparse_synapse_pool_t pool,
    sparse_synapse_storage_t* storage,
    uint32_t count
);

/**
 * @brief Remove synapse from storage
 *
 * WHAT: Remove synapse by index (swap-and-pop)
 * WHY:  Support synaptic pruning
 * HOW:  Swap with last element, decrement count
 *
 * ALGORITHM:
 * 1. If index < embedded_count: Remove from embedded
 * 2. Else: Remove from overflow
 * 3. Swap element with last, decrement count
 *
 * @param pool Pool handle
 * @param storage Storage to remove from
 * @param index Index of synapse to remove
 * @return 0 on success, -1 on invalid index
 *
 * COMPLEXITY: O(1) (swap-and-pop)
 * NOTE: Changes synapse order (last element moves to index)
 */
NIMCP_EXPORT int sparse_synapse_remove(
    sparse_synapse_pool_t pool,
    sparse_synapse_storage_t* storage,
    uint32_t index
);

/**
 * @brief Get synapse by index
 *
 * WHAT: Retrieve synapse handle at given index
 * WHY:  Random access to synapses
 * HOW:  Index into embedded or overflow array
 *
 * @param storage Storage to query
 * @param index Synapse index
 * @return Pointer to handle, or NULL if index out of bounds
 *
 * COMPLEXITY: O(1)
 */
NIMCP_EXPORT synapse_handle_t* sparse_synapse_get(
    sparse_synapse_storage_t* storage,
    uint32_t index
);

/**
 * @brief Get total synapse count
 *
 * @param storage Storage to query
 * @return Total number of synapses (embedded + overflow)
 *
 * COMPLEXITY: O(1)
 */
NIMCP_EXPORT uint32_t sparse_synapse_count(
    const sparse_synapse_storage_t* storage
);

/**
 * @brief Compact storage (move overflow to embedded if possible)
 *
 * WHAT: Optimize storage by moving overflow synapses to embedded
 * WHY:  After pruning, overflow may fit in embedded (better cache locality)
 * HOW:  Copy overflow → embedded, free overflow
 *
 * @param pool Pool handle
 * @param storage Storage to compact
 * @return Number of synapses moved to embedded
 *
 * COMPLEXITY: O(n) where n = total synapses
 * USE CASE: After bulk synapse removal/pruning
 */
NIMCP_EXPORT uint32_t sparse_synapse_compact(
    sparse_synapse_pool_t pool,
    sparse_synapse_storage_t* storage
);

//=============================================================================
// Iterator API
//=============================================================================

/**
 * @brief Initialize iterator
 *
 * @param it Iterator structure
 * @param storage Storage to iterate over
 */
NIMCP_EXPORT void sparse_synapse_iterator_init(
    sparse_synapse_iterator_t* it,
    sparse_synapse_storage_t* storage
);

/**
 * @brief Get next synapse in iteration
 *
 * @param it Iterator structure
 * @return Next synapse handle, or NULL if end reached
 *
 * COMPLEXITY: O(1) per call, O(n) for full iteration
 */
NIMCP_EXPORT synapse_handle_t* sparse_synapse_iterator_next(
    sparse_synapse_iterator_t* it
);

/**
 * @brief Check if iterator has more synapses
 *
 * @param it Iterator structure
 * @return true if more synapses available
 */
NIMCP_EXPORT bool sparse_synapse_iterator_has_next(
    const sparse_synapse_iterator_t* it
);

/**
 * @brief Reset iterator to beginning
 *
 * @param it Iterator structure
 */
NIMCP_EXPORT void sparse_synapse_iterator_reset(
    sparse_synapse_iterator_t* it
);

//=============================================================================
// Statistics API
//=============================================================================

/**
 * @brief Get pool statistics
 *
 * WHAT: Retrieve current pool usage and memory metrics
 * WHY:  Monitor memory efficiency, detect pool exhaustion
 * HOW:  Atomic reads of pool counters
 *
 * @param pool Pool handle
 * @param stats Output statistics structure
 * @return 0 on success, -1 on failure
 *
 * COMPLEXITY: O(1)
 * THREAD SAFETY: Atomic reads (safe concurrent access)
 */
NIMCP_EXPORT int sparse_synapse_pool_get_stats(
    sparse_synapse_pool_t pool,
    sparse_synapse_stats_t* stats
);

/**
 * @brief Reset pool statistics
 *
 * @param pool Pool handle
 */
NIMCP_EXPORT void sparse_synapse_pool_reset_stats(
    sparse_synapse_pool_t pool
);

/**
 * @brief Get pool utilization
 *
 * @param pool Pool handle
 * @return Utilization percentage [0.0-1.0]
 */
NIMCP_EXPORT float sparse_synapse_pool_utilization(
    sparse_synapse_pool_t pool
);

/**
 * @brief Get available handles
 *
 * @param pool Pool handle
 * @return Number of free handles in pool
 */
NIMCP_EXPORT size_t sparse_synapse_pool_available(
    sparse_synapse_pool_t pool
);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Calculate memory savings vs dense allocation
 *
 * WHAT: Compare sparse vs dense memory usage
 * WHY:  Validate memory efficiency gains
 * HOW:  Compare actual usage vs hypothetical dense allocation
 *
 * @param pool Pool handle
 * @param num_neurons Total neurons in network
 * @param dense_synapses_per_neuron Dense allocation capacity
 * @param bytes_per_synapse Size of full synapse structure
 * @return Memory savings percentage [0.0-1.0]
 *
 * EXAMPLE:
 * ```c
 * // Dense: 10,000 neurons × 100 synapses × 600 bytes = 600 MB
 * // Sparse: 5.3 MB
 * float savings = sparse_synapse_memory_savings(pool, 10000, 100, 600);
 * // savings ≈ 0.991 (99.1% reduction)
 * ```
 */
NIMCP_EXPORT float sparse_synapse_memory_savings(
    sparse_synapse_pool_t pool,
    size_t num_neurons,
    size_t dense_synapses_per_neuron,
    size_t bytes_per_synapse
);

/**
 * @brief Print pool statistics (for debugging)
 *
 * @param pool Pool handle
 * @param verbose Include detailed breakdown
 */
NIMCP_EXPORT void sparse_synapse_pool_print_stats(
    sparse_synapse_pool_t pool,
    bool verbose
);

//=============================================================================
// Synapse Metadata Pool API
//=============================================================================
//
// WHAT: Pool of full synapse_t objects for advanced synapse features
// WHY:  Most synapses only need target+weight; allocate full metadata on-demand
// HOW:  Index-based lookup from synapse_handle_t::metadata_index
//
// DUAL-LAYER ARCHITECTURE:
// ┌─────────────────────────────────────────────────────────────────────────┐
// │ Layer 1: Synapse Handles (sparse_synapse_storage_t)                     │
// │ ┌─────────────────────────────────────────────────────────────────────┐ │
// │ │ Handle[0]: {target=42, weight=0.5, metadata_index=NONE}            │ │
// │ │ Handle[1]: {target=17, weight=0.8, metadata_index=5}      ─────┐   │ │
// │ │ Handle[2]: {target=99, weight=0.2, metadata_index=NONE}        │   │ │
// │ └─────────────────────────────────────────────────────────────────┴───┘ │
// └───────────────────────────────────────────────────────────────────│─────┘
//                                                                     │
// ┌───────────────────────────────────────────────────────────────────▼─────┐
// │ Layer 2: Synapse Metadata Pool (synapse_metadata_pool_t)               │
// │ ┌─────────────────────────────────────────────────────────────────────┐ │
// │ │ Slot[5]: Full synapse_t with NMDA type, STDP, STP, etc.            │ │
// │ └─────────────────────────────────────────────────────────────────────┘ │
// └─────────────────────────────────────────────────────────────────────────┘
//
// USAGE:
// - Simple synapses: Use handle directly (target, weight)
// - Advanced synapses: Allocate metadata, set metadata_index, use full synapse_t
//

// Forward declaration - synapse_t defined in nimcp_neuralnet.h
struct synapse_t;

/**
 * @brief Synapse metadata pool handle (opaque)
 */
typedef struct synapse_metadata_pool_struct* synapse_metadata_pool_t;

/**
 * @brief Synapse metadata pool configuration
 */
typedef struct {
    size_t pool_size;               /**< Number of synapse_t slots in pool */
    bool enable_statistics;         /**< Track allocation stats */
    bool thread_safe;               /**< Enable mutex protection */
} synapse_metadata_pool_config_t;

/**
 * @brief Create synapse metadata pool
 *
 * WHAT: Initialize pool of full synapse_t objects
 * WHY:  On-demand allocation of advanced synapse features
 * HOW:  Pre-allocate fixed number of synapse_t slots
 *
 * @param config Pool configuration (NULL for defaults)
 * @return Pool handle or NULL on failure
 *
 * DEFAULT CONFIG:
 * - pool_size: 10,000 (5% of typical 200K synapses)
 * - enable_statistics: true
 * - thread_safe: true
 */
NIMCP_EXPORT synapse_metadata_pool_t synapse_metadata_pool_create(
    const synapse_metadata_pool_config_t* config
);

/**
 * @brief Destroy synapse metadata pool
 *
 * @param pool Pool handle
 */
NIMCP_EXPORT void synapse_metadata_pool_destroy(synapse_metadata_pool_t pool);

/**
 * @brief Get default metadata pool configuration
 *
 * @return Default configuration with sensible values
 */
NIMCP_EXPORT synapse_metadata_pool_config_t synapse_metadata_pool_default_config(void);

/**
 * @brief Allocate synapse metadata slot
 *
 * WHAT: Allocate full synapse_t from pool
 * WHY:  Enable advanced features for specific synapse
 * HOW:  Get free slot, return index for metadata_index field
 *
 * @param pool Pool handle
 * @return Index into pool (set to synapse_handle_t::metadata_index), or SPARSE_SYNAPSE_NO_METADATA on failure
 */
/** @brief Check if metadata pool is at capacity (lock-free check). */
NIMCP_EXPORT bool synapse_metadata_pool_is_full(synapse_metadata_pool_t pool);

NIMCP_EXPORT uint32_t synapse_metadata_pool_allocate(synapse_metadata_pool_t pool);

/**
 * @brief Free synapse metadata slot
 *
 * @param pool Pool handle
 * @param index Index returned by synapse_metadata_pool_allocate
 */
NIMCP_EXPORT void synapse_metadata_pool_free(synapse_metadata_pool_t pool, uint32_t index);

/**
 * @brief Get synapse_t by index
 *
 * WHAT: Retrieve full synapse_t pointer from pool
 * WHY:  Access advanced synapse features (types, plasticity, etc.)
 * HOW:  Direct index into pool array
 *
 * @param pool Pool handle
 * @param index Index from synapse_handle_t::metadata_index
 * @return Pointer to synapse_t, or NULL if invalid index
 *
 * NOTE: Returns NULL if index == SPARSE_SYNAPSE_NO_METADATA
 */
NIMCP_EXPORT struct synapse_t* synapse_metadata_pool_get(
    synapse_metadata_pool_t pool,
    uint32_t index
);

/**
 * @brief Get pool utilization
 *
 * @param pool Pool handle
 * @return Utilization percentage [0.0-1.0]
 */
NIMCP_EXPORT float synapse_metadata_pool_utilization(synapse_metadata_pool_t pool);

/**
 * @brief Get available metadata slots
 *
 * @param pool Pool handle
 * @return Number of free slots in pool
 */
NIMCP_EXPORT size_t synapse_metadata_pool_available(synapse_metadata_pool_t pool);

//=============================================================================
// Synapse Cold Pool API (NIMCP 2.11: Hot/cold split)
//=============================================================================
//
// WHAT: Pool of synapse_cold_t objects for rarely-used synapse features
// WHY:  Only ~5% of synapses need STP/BCM/eligibility/compute/type/embeddings
// HOW:  Lazy allocation — cold data created on first use via SYNAPSE_ENSURE_COLD()
//       Saves ~23 GB for 2M-neuron networks (200B→56B per metadata entry)
//

// Forward declaration
struct synapse_cold_t;

typedef struct synapse_cold_pool_struct* synapse_cold_pool_t;

typedef struct {
    size_t pool_size;               /**< Number of synapse_cold_t slots */
    bool enable_statistics;
    bool thread_safe;
} synapse_cold_pool_config_t;

NIMCP_EXPORT synapse_cold_pool_t synapse_cold_pool_create(
    const synapse_cold_pool_config_t* config
);
NIMCP_EXPORT void synapse_cold_pool_destroy(synapse_cold_pool_t pool);
NIMCP_EXPORT synapse_cold_pool_config_t synapse_cold_pool_default_config(void);
NIMCP_EXPORT uint32_t synapse_cold_pool_allocate(synapse_cold_pool_t pool);
NIMCP_EXPORT void synapse_cold_pool_free(synapse_cold_pool_t pool, uint32_t index);
NIMCP_EXPORT struct synapse_cold_t* synapse_cold_pool_get(
    synapse_cold_pool_t pool, uint32_t index
);

//=============================================================================
// Extended Synapse Handle API (with metadata support)
//=============================================================================

/**
 * @brief Add synapse with metadata
 *
 * WHAT: Add synapse with full synapse_t metadata
 * WHY:  Support advanced synapse features (types, plasticity, etc.)
 * HOW:  Add handle, allocate metadata slot, link via metadata_index
 *
 * @param handle_pool Handle pool for sparse storage
 * @param metadata_pool Metadata pool for full synapse_t
 * @param storage Storage to add to
 * @param target_neuron_id Target neuron index
 * @param weight Synaptic weight
 * @param synapse_type Synapse type (AMPA, NMDA, GABA, etc.)
 * @return 0 on success, -1 on failure
 *
 * EXAMPLE:
 * ```c
 * sparse_synapse_add_with_metadata(
 *     handle_pool, metadata_pool, &storage,
 *     target_id, weight, SYNAPSE_NMDA
 * );
 * ```
 */
NIMCP_EXPORT int sparse_synapse_add_with_metadata(
    sparse_synapse_pool_t handle_pool,
    synapse_metadata_pool_t metadata_pool,
    sparse_synapse_storage_t* storage,
    uint32_t target_neuron_id,
    float weight,
    int synapse_type  // synapse_type_t from nimcp_synapse_types.h
);

/**
 * @brief Get synapse metadata from handle
 *
 * WHAT: Retrieve full synapse_t for handle with metadata
 * WHY:  Access advanced synapse features from handle
 * HOW:  Look up metadata_index in metadata pool
 *
 * @param metadata_pool Metadata pool
 * @param handle Synapse handle
 * @return Pointer to synapse_t or NULL if no metadata
 */
NIMCP_EXPORT struct synapse_t* sparse_synapse_get_metadata(
    synapse_metadata_pool_t metadata_pool,
    const synapse_handle_t* handle
);

/**
 * @brief Remove synapse and its metadata
 *
 * WHAT: Remove synapse handle and free any associated metadata
 * WHY:  Clean removal of synapses with full metadata
 * HOW:  Free metadata slot (if any), then remove handle
 *
 * @param handle_pool Handle pool
 * @param metadata_pool Metadata pool (can be NULL if no metadata)
 * @param storage Storage to remove from
 * @param index Index of synapse to remove
 * @return 0 on success, -1 on invalid index
 */
NIMCP_EXPORT int sparse_synapse_remove_with_metadata(
    sparse_synapse_pool_t handle_pool,
    synapse_metadata_pool_t metadata_pool,
    sparse_synapse_storage_t* storage,
    uint32_t index
);

//=============================================================================
// Ternary-Aware Sparse Synapse Operations (NIMCP 2.10)
//=============================================================================

/**
 * @brief Add synapse with ternary weight
 *
 * WHAT: Add synapse handle using ternary weight for storage efficiency
 * WHY:  Memory savings when weights are naturally ternary (LTD/stable/LTP)
 * HOW:  Store ternary weight, set use_ternary_weight flag
 *
 * @param pool Pool handle for overflow allocation
 * @param storage Storage to add to
 * @param target_neuron_id Target neuron index
 * @param ternary_weight Ternary weight {-1, 0, +1}
 * @return 0 on success, -1 on failure
 *
 * COMPLEXITY: O(1) amortized
 * THREAD SAFETY: Caller must synchronize access to storage
 */
NIMCP_EXPORT int sparse_synapse_add_ternary(
    sparse_synapse_pool_t pool,
    sparse_synapse_storage_t* storage,
    uint32_t target_neuron_id,
    trit_t ternary_weight
);

/**
 * @brief Get effective weight from handle (ternary or float)
 *
 * WHAT: Retrieve weight value regardless of storage mode
 * WHY:  Unified interface for synapse computation
 * HOW:  Check use_ternary_weight flag, return appropriate value
 *
 * @param handle Synapse handle
 * @param positive_scale Scale for +1 ternary weights (default 1.0)
 * @param negative_scale Scale for -1 ternary weights (default -1.0)
 * @return Effective weight value
 *
 * COMPLEXITY: O(1)
 */
NIMCP_EXPORT float sparse_synapse_get_effective_weight(
    const synapse_handle_t* handle,
    float positive_scale,
    float negative_scale
);

/**
 * @brief Set weight with automatic ternary quantization
 *
 * WHAT: Set weight with optional quantization to ternary
 * WHY:  Enable transparent ternary mode switching
 * HOW:  If use_ternary enabled, quantize; otherwise store float
 *
 * @param handle Synapse handle
 * @param weight Weight value
 * @param threshold Threshold for ternary quantization
 * @param enable_ternary Whether to use ternary storage
 *
 * COMPLEXITY: O(1)
 */
NIMCP_EXPORT void sparse_synapse_set_weight(
    synapse_handle_t* handle,
    float weight,
    float threshold,
    bool enable_ternary
);

/**
 * @brief Convert storage to ternary weights
 *
 * WHAT: Quantize all float weights in storage to ternary
 * WHY:  Batch conversion for memory optimization
 * HOW:  Iterate all handles, apply threshold-based quantization
 *
 * @param storage Storage to convert
 * @param threshold Quantization threshold
 * @return Number of synapses converted
 *
 * COMPLEXITY: O(n) where n = synapse count
 */
NIMCP_EXPORT uint32_t sparse_synapse_convert_to_ternary(
    sparse_synapse_storage_t* storage,
    float threshold
);

/**
 * @brief Convert storage back to float weights
 *
 * WHAT: Dequantize all ternary weights to float
 * WHY:  Enable fine-grained weight adjustments after ternary storage
 * HOW:  Iterate all handles, dequantize ternary to float with scales
 *
 * @param storage Storage to convert
 * @param positive_scale Scale for +1 weights
 * @param negative_scale Scale for -1 weights
 * @return Number of synapses converted
 *
 * COMPLEXITY: O(n) where n = synapse count
 */
NIMCP_EXPORT uint32_t sparse_synapse_convert_to_float(
    sparse_synapse_storage_t* storage,
    float positive_scale,
    float negative_scale
);

/**
 * @brief Get ternary weight statistics for storage
 *
 * WHAT: Count distribution of ternary weights
 * WHY:  Analyze sparsity and weight distribution
 * HOW:  Iterate handles, count {-1, 0, +1} occurrences
 *
 * @param storage Storage to analyze
 * @param n_positive Output: count of +1 weights
 * @param n_zero Output: count of 0 weights
 * @param n_negative Output: count of -1 weights
 * @param n_ternary_enabled Output: count using ternary mode
 *
 * COMPLEXITY: O(n)
 */
NIMCP_EXPORT void sparse_synapse_ternary_stats(
    const sparse_synapse_storage_t* storage,
    uint32_t* n_positive,
    uint32_t* n_zero,
    uint32_t* n_negative,
    uint32_t* n_ternary_enabled
);

/**
 * @brief Ternary-aware dot product for sparse synapses
 *
 * WHAT: Compute weighted sum using ternary or float weights
 * WHY:  Efficient forward pass with mixed weight types
 * HOW:  Sum input * effective_weight for all synapses
 *
 * @param storage Sparse synapse storage
 * @param inputs Input activations indexed by target_neuron_id
 * @param positive_scale Scale for +1 ternary weights
 * @param negative_scale Scale for -1 ternary weights
 * @return Weighted sum
 *
 * COMPLEXITY: O(n) where n = synapse count
 *
 * USAGE:
 * ```c
 * float output = sparse_synapse_ternary_dot(&neuron->synapses, input_activations, 1.0f, -1.0f);
 * ```
 */
NIMCP_EXPORT float sparse_synapse_ternary_dot(
    const sparse_synapse_storage_t* storage,
    const float* inputs,
    float positive_scale,
    float negative_scale
);

#ifdef __cplusplus
}
#endif

#endif  // NIMCP_SPARSE_SYNAPSE_H
