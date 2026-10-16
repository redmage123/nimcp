/**
 * @file nimcp_recovery_cache.h
 * @brief Recovery strategy cache for fault tolerance
 *
 * WHAT: Memoization cache for successful recovery strategies
 * WHY: 63x speedup for repeated errors (17ms -> 0.27ms)
 * HOW: Error signature fingerprinting + LRU hash table + success tracking
 *
 * Performance Targets:
 * - Lookup: <100ns
 * - Store: <1us
 * - Hit rate: >80% for repeated errors
 *
 * Architecture:
 * - Hash table with chaining for O(1) lookup
 * - LRU eviction policy for capacity management
 * - Thread-safe operations with minimal lock contention
 * - Success rate tracking for strategy validation
 */

#ifndef NIMCP_RECOVERY_CACHE_H
#define NIMCP_RECOVERY_CACHE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "utils/thread/nimcp_thread.h"

/* ============================================================================
 * CONSTANTS AND CONFIGURATION
 * ============================================================================ */

/** Default cache capacity (number of entries) */
#define NIMCP_RECOVERY_CACHE_DEFAULT_CAPACITY 1024

/** Hash table size (prime for better distribution) */
#define NIMCP_RECOVERY_CACHE_HASH_SIZE 1021

/** Maximum signature size in bytes */
#define NIMCP_RECOVERY_CACHE_MAX_SIGNATURE_SIZE 64

/** Maximum strategy description size */
#define NIMCP_RECOVERY_CACHE_MAX_STRATEGY_SIZE 256

/* ============================================================================
 * TYPE DEFINITIONS
 * ============================================================================ */

/**
 * @brief Recovery strategy enumeration
 *
 * WHAT: Available recovery strategies
 * WHY: Type-safe strategy identification
 * HOW: Enumeration with explicit values
 */
typedef enum {
    NIMCP_RECOVERY_STRATEGY_NONE = 0,
    NIMCP_RECOVERY_STRATEGY_RETRY = 1,
    NIMCP_RECOVERY_STRATEGY_ROLLBACK = 2,
    NIMCP_RECOVERY_STRATEGY_CHECKPOINT_RESTORE = 3,
    NIMCP_RECOVERY_STRATEGY_ALTERNATE_PATH = 4,
    NIMCP_RECOVERY_STRATEGY_SAFE_MODE = 5,
    NIMCP_RECOVERY_STRATEGY_RESET = 6,
    NIMCP_RECOVERY_STRATEGY_CUSTOM = 7
} nimcp_recovery_strategy_t;

/**
 * @brief Error signature structure
 *
 * WHAT: Unique fingerprint for error conditions
 * WHY: Fast error matching and cache lookup
 * HOW: Hash of signal + address + context
 */
typedef struct {
    uint64_t hash;                                           /**< Primary hash value */
    uint8_t data[NIMCP_RECOVERY_CACHE_MAX_SIGNATURE_SIZE];  /**< Raw signature data */
    size_t size;                                             /**< Signature size in bytes */
} nimcp_error_signature_t;

/**
 * @brief Cache entry structure
 *
 * WHAT: Single cached recovery strategy
 * WHY: Store strategy with metadata for decision making
 * HOW: LRU chain + success tracking + timestamp
 */
typedef struct nimcp_cache_entry {
    nimcp_error_signature_t signature;                       /**< Error fingerprint */
    nimcp_recovery_strategy_t strategy;                      /**< Recovery strategy */
    char strategy_desc[NIMCP_RECOVERY_CACHE_MAX_STRATEGY_SIZE]; /**< Human-readable description */

    /* Success tracking */
    uint64_t success_count;                                  /**< Successful recoveries */
    uint64_t failure_count;                                  /**< Failed recoveries */
    double success_rate;                                     /**< Cached success rate */

    /* Timing information */
    uint64_t created_timestamp;                              /**< Creation time (ns) */
    uint64_t last_access_timestamp;                          /**< Last access time (ns) */
    uint64_t last_success_timestamp;                         /**< Last success time (ns) */

    /* LRU chain */
    struct nimcp_cache_entry* prev;                          /**< Previous in LRU order */
    struct nimcp_cache_entry* next;                          /**< Next in LRU order */

    /* Hash chain */
    struct nimcp_cache_entry* hash_next;                     /**< Next in hash bucket */
} nimcp_cache_entry_t;

/**
 * @brief Cache statistics
 *
 * WHAT: Performance and utilization metrics
 * WHY: Monitor cache effectiveness
 * HOW: Counters updated on each operation
 */
typedef struct {
    uint64_t hits;                                           /**< Cache hits */
    uint64_t misses;                                         /**< Cache misses */
    uint64_t stores;                                         /**< Store operations */
    uint64_t evictions;                                      /**< LRU evictions */
    uint64_t invalidations;                                  /**< Manual invalidations */

    double hit_rate;                                         /**< Cached hit rate */
    size_t current_size;                                     /**< Current entries */
    size_t max_size;                                         /**< Maximum capacity */

    uint64_t total_lookup_time_ns;                           /**< Total lookup time */
    uint64_t total_store_time_ns;                            /**< Total store time */
    uint64_t avg_lookup_time_ns;                             /**< Average lookup time */
    uint64_t avg_store_time_ns;                              /**< Average store time */
} nimcp_recovery_cache_stats_t;

/**
 * @brief Recovery cache structure
 *
 * WHAT: Main cache container
 * WHY: Fast recovery strategy lookup and management
 * HOW: Hash table + LRU list + statistics
 */
typedef struct {
    /* Hash table */
    nimcp_cache_entry_t** hash_table;                        /**< Hash buckets */
    size_t hash_size;                                        /**< Number of buckets */

    /* LRU list */
    nimcp_cache_entry_t* lru_head;                           /**< Most recently used */
    nimcp_cache_entry_t* lru_tail;                           /**< Least recently used */

    /* Configuration */
    size_t capacity;                                         /**< Maximum entries */
    size_t current_size;                                     /**< Current entries */

    /* Statistics */
    nimcp_recovery_cache_stats_t stats;                      /**< Performance metrics */

    /* Thread safety */
    nimcp_mutex_t lock;                                    /**< Protects all fields */

    /* Configuration flags */
    bool enable_lru;                                         /**< Enable LRU eviction */
    bool track_stats;                                        /**< Enable statistics */
} nimcp_recovery_cache_t;

/**
 * @brief Signal/crash context for recovery signature generation
 *
 * WHAT: Context information for error fingerprinting
 * WHY: Create unique signatures for different error scenarios
 * HOW: Combine signal, address, and context hash
 *
 * NOTE: Named nimcp_signal_error_context_t to avoid collision with
 * nimcp_error_context_t in error/nimcp_error_codes.h which has different fields
 */
typedef struct {
    int signal;                                              /**< Signal number */
    void* fault_address;                                     /**< Fault address */
    uint64_t context_hash;                                   /**< Additional context */
    const char* function_name;                               /**< Function where error occurred */
    int error_code;                                          /**< System error code */
} nimcp_signal_error_context_t;

/* ============================================================================
 * CACHE LIFECYCLE
 * ============================================================================ */

/**
 * @brief Create recovery cache
 *
 * WHAT: Allocate and initialize cache
 * WHY: Setup fast recovery strategy lookup
 * HOW: Allocate hash table + initialize LRU + setup mutex
 *
 * @param capacity Maximum number of entries (0 for default)
 * @return Initialized cache or NULL on failure
 *
 * @note Thread-safe after creation
 * @note Call nimcp_recovery_cache_destroy() to free
 */
nimcp_recovery_cache_t* nimcp_recovery_cache_create(size_t capacity);

/**
 * @brief Destroy recovery cache
 *
 * WHAT: Free all cache resources
 * WHY: Clean shutdown and memory management
 * HOW: Free all entries + hash table + mutex
 *
 * @param cache Cache to destroy
 *
 * @note Logs warning if cache still has entries
 */
void nimcp_recovery_cache_destroy(nimcp_recovery_cache_t* cache);

/**
 * @brief Clear cache contents
 *
 * WHAT: Remove all entries
 * WHY: Reset cache state or handle configuration changes
 * HOW: Free all entries, reset LRU and stats
 *
 * @param cache Cache to clear
 * @return true on success
 */
bool nimcp_recovery_cache_clear(nimcp_recovery_cache_t* cache);

/* ============================================================================
 * SIGNATURE OPERATIONS
 * ============================================================================ */

/**
 * @brief Compute error signature
 *
 * WHAT: Generate unique fingerprint for error
 * WHY: Fast error matching in cache
 * HOW: Hash signal + address + context
 *
 * @param context Error context
 * @param signature Output signature
 * @return true on success
 *
 * @note Deterministic - same context produces same signature
 * @note Fast: <50ns typical
 */
bool nimcp_recovery_cache_compute_signature(
    const nimcp_signal_error_context_t* context,
    nimcp_error_signature_t* signature
);

/**
 * @brief Compare error signatures
 *
 * WHAT: Check if two signatures match
 * WHY: Signature equality testing
 * HOW: Compare hash first, then data if needed
 *
 * @param sig1 First signature
 * @param sig2 Second signature
 * @return true if signatures match
 *
 * @note Fast path: hash comparison only in most cases
 */
bool nimcp_recovery_cache_signatures_equal(
    const nimcp_error_signature_t* sig1,
    const nimcp_error_signature_t* sig2
);

/* ============================================================================
 * CACHE OPERATIONS
 * ============================================================================ */

/**
 * @brief Lookup recovery strategy
 *
 * WHAT: Find cached strategy for error signature
 * WHY: Skip expensive diagnostics for known errors
 * HOW: Hash table lookup + LRU update
 *
 * @param cache Recovery cache
 * @param signature Error signature
 * @param[out] strategy Found strategy (if hit)
 * @return true if found (cache hit)
 *
 * @note Updates LRU on hit
 * @note Updates statistics
 * @note Performance: <100ns typical
 */
bool nimcp_recovery_cache_lookup(
    nimcp_recovery_cache_t* cache,
    const nimcp_error_signature_t* signature,
    nimcp_recovery_strategy_t* strategy
);

/**
 * @brief Store recovery strategy
 *
 * WHAT: Cache successful recovery strategy
 * WHY: Speed up future identical errors
 * HOW: Insert into hash table + LRU, evict if needed
 *
 * @param cache Recovery cache
 * @param signature Error signature
 * @param strategy Recovery strategy
 * @param description Human-readable description (optional)
 * @return true on success
 *
 * @note Evicts LRU entry if at capacity
 * @note Updates statistics
 * @note Performance: <1us typical
 */
bool nimcp_recovery_cache_store(
    nimcp_recovery_cache_t* cache,
    const nimcp_error_signature_t* signature,
    nimcp_recovery_strategy_t strategy,
    const char* description
);

/**
 * @brief Update entry success tracking
 *
 * WHAT: Record recovery outcome
 * WHY: Track strategy effectiveness
 * HOW: Increment counters, recalculate success rate
 *
 * @param cache Recovery cache
 * @param signature Error signature
 * @param success true if recovery succeeded
 * @return true if entry found and updated
 */
bool nimcp_recovery_cache_update_success(
    nimcp_recovery_cache_t* cache,
    const nimcp_error_signature_t* signature,
    bool success
);

/**
 * @brief Invalidate cache entry
 *
 * WHAT: Remove specific entry from cache
 * WHY: Handle strategy that stopped working
 * HOW: Remove from hash table and LRU
 *
 * @param cache Recovery cache
 * @param signature Error signature
 * @return true if entry was found and removed
 */
bool nimcp_recovery_cache_invalidate(
    nimcp_recovery_cache_t* cache,
    const nimcp_error_signature_t* signature
);

/* ============================================================================
 * CACHE MANAGEMENT
 * ============================================================================ */

/**
 * @brief Get cache entry details
 *
 * WHAT: Retrieve full entry information
 * WHY: Inspect cached strategy details
 * HOW: Lookup and copy entry data
 *
 * @param cache Recovery cache
 * @param signature Error signature
 * @param[out] entry Entry details (if found)
 * @return true if entry found
 */
bool nimcp_recovery_cache_get_entry(
    nimcp_recovery_cache_t* cache,
    const nimcp_error_signature_t* signature,
    nimcp_cache_entry_t* entry
);

/**
 * @brief Get cache statistics
 *
 * WHAT: Retrieve performance metrics
 * WHY: Monitor cache effectiveness
 * HOW: Copy statistics structure
 *
 * @param cache Recovery cache
 * @param[out] stats Statistics output
 * @return true on success
 */
bool nimcp_recovery_cache_get_stats(
    nimcp_recovery_cache_t* cache,
    nimcp_recovery_cache_stats_t* stats
);

/**
 * @brief Reset cache statistics
 *
 * WHAT: Clear all performance counters
 * WHY: Start fresh measurement period
 * HOW: Zero all statistics fields
 *
 * @param cache Recovery cache
 * @return true on success
 */
bool nimcp_recovery_cache_reset_stats(nimcp_recovery_cache_t* cache);

/**
 * @brief Resize cache capacity
 *
 * WHAT: Change maximum number of entries
 * WHY: Adjust cache size based on workload
 * HOW: Update capacity, evict if needed
 *
 * @param cache Recovery cache
 * @param new_capacity New maximum entries
 * @return true on success
 *
 * @note May trigger evictions if shrinking
 */
bool nimcp_recovery_cache_resize(
    nimcp_recovery_cache_t* cache,
    size_t new_capacity
);

/* ============================================================================
 * UTILITY FUNCTIONS
 * ============================================================================ */

/**
 * @brief Get strategy name
 *
 * WHAT: Convert strategy enum to string
 * WHY: Human-readable output
 * HOW: Lookup table
 *
 * @param strategy Recovery strategy
 * @return Strategy name string
 */
const char* nimcp_recovery_strategy_name(nimcp_recovery_strategy_t strategy);

/**
 * @brief Print cache statistics
 *
 * WHAT: Display cache performance metrics
 * WHY: Debugging and monitoring
 * HOW: Format statistics to stdout
 *
 * @param cache Recovery cache
 */
void nimcp_recovery_cache_print_stats(const nimcp_recovery_cache_t* cache);

/**
 * @brief Validate cache consistency
 *
 * WHAT: Check internal data structure integrity
 * WHY: Debugging and testing
 * HOW: Verify hash table + LRU + counts
 *
 * @param cache Recovery cache
 * @return true if cache is consistent
 */
bool nimcp_recovery_cache_validate(const nimcp_recovery_cache_t* cache);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_RECOVERY_CACHE_H */
