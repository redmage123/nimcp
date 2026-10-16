//=============================================================================
// nimcp_page_cow_opt.h - Optimized Page-Level COW Operations
//=============================================================================
/**
 * @file nimcp_page_cow_opt.h
 * @brief Memory-optimized lookup and batch operations for page-level COW
 *
 * WHAT: Hash-based view lookup, prefetch hints, and batch COW operations
 * WHY:  O(n) linear search in signal handler is expensive with many views;
 *       batch operations reduce signal handler overhead for known write patterns
 * HOW:  Hash table for O(1) address lookup, __builtin_prefetch for cache,
 *       batch privatization to avoid repeated signal handler invocations
 *
 * ARCHITECTURE:
 *
 *   Hash-Based View Lookup:
 *   ┌─────────────────────────────────────────────────────────────────────────┐
 *   │  Before (O(n) linear scan):                                             │
 *   │  ┌─────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┐                      │
 *   │  │ V0  │ V1  │ V2  │ V3  │ ... │ V97 │ V98 │ V99 │  Scan 100 views!    │
 *   │  └─────┴─────┴─────┴─────┴─────┴─────┴─────┴─────┘                      │
 *   │                                                                         │
 *   │  After (O(1) hash lookup):                                              │
 *   │  addr=0x7f4000   hash(addr>>12) = 42                                    │
 *   │                      │                                                  │
 *   │  ┌─────┬─────┬─────┬─▼───┬─────┬─────┬─────┐                            │
 *   │  │     │     │     │ V42 │     │     │     │  Direct bucket access!    │
 *   │  └─────┴─────┴─────┴─────┴─────┴─────┴─────┘                            │
 *   └─────────────────────────────────────────────────────────────────────────┘
 *
 *   Batch Privatization:
 *   ┌─────────────────────────────────────────────────────────────────────────┐
 *   │  Before (per-page signal overhead):                                     │
 *   │  Write P0 → SIGSEGV → COW → Write P1 → SIGSEGV → COW → ...             │
 *   │                                                                         │
 *   │  After (batch pre-privatization):                                       │
 *   │  Batch privatize [P0..P7] → Write P0..P7 (no signals!)                  │
 *   └─────────────────────────────────────────────────────────────────────────┘
 *
 * PERFORMANCE:
 * - View Lookup: O(1) average vs O(n) linear search
 * - Batch COW: Single lock acquisition for N pages
 * - Prefetch: ~50-100 cycles saved per cache miss avoided
 *
 * USAGE:
 * 1. Initialize hash table with page_cow_hash_init()
 * 2. Views auto-register on creation (if using optimized path)
 * 3. Use batch privatization for known write patterns
 * 4. Use prefetch hints before sequential access
 *
 * @author NIMCP Development Team
 * @date 2025-11-24
 * @version 1.0.0
 */

#ifndef NIMCP_PAGE_COW_OPT_H
#define NIMCP_PAGE_COW_OPT_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>
#include "utils/memory/nimcp_page_cow.h"

#ifdef __cplusplus
extern "C" {
#endif

// Export macro (for shared library builds)
#ifndef NIMCP_EXPORT
    #define NIMCP_EXPORT
#endif

//=============================================================================
// Configuration Constants
//=============================================================================

/**
 * @brief Hash table bucket count (must be power of 2)
 *
 * WHAT: Number of buckets in view lookup hash table
 * WHY:  Balance between memory usage and collision rate
 * HOW:  Power of 2 enables fast modulo via bitmask
 *
 * With 1024 buckets and typical 16-entry chains, supports 16K views
 * at ~1.5 average chain length (expected with good hash distribution)
 */
#define PAGE_COW_HASH_BUCKETS 1024

/**
 * @brief Maximum chain length before warning
 *
 * WHAT: Threshold for hash collision chain length
 * WHY:  Detect degenerate hash distribution
 * HOW:  Log warning if exceeded during insert
 */
#define PAGE_COW_HASH_MAX_CHAIN 16

/**
 * @brief Maximum pages to batch privatize in single operation
 *
 * WHAT: Upper limit on batch size
 * WHY:  Prevent excessive lock holding time
 * HOW:  Caller can request more; we process in chunks
 */
#define PAGE_COW_BATCH_MAX 256

//=============================================================================
// Prefetch Macros
//=============================================================================

/**
 * @brief Prefetch for read access (temporal - keep in cache)
 *
 * WHAT: Hint to CPU to load cache line for reading
 * WHY:  Hide memory latency when access pattern is predictable
 * HOW:  __builtin_prefetch with locality hint
 *
 * @param addr Address to prefetch (will load containing cache line)
 *
 * USAGE:
 * ```c
 * const float* weights = page_cow_view_read(view);
 * for (size_t i = 0; i < n; i += 16) {
 *     PAGE_COW_PREFETCH_READ(&weights[i + 16]);  // Prefetch next chunk
 *     process(&weights[i]);
 * }
 * ```
 */
#define PAGE_COW_PREFETCH_READ(addr) \
    __builtin_prefetch((const void*)(addr), 0, 3)

/**
 * @brief Prefetch for write access (temporal - keep in cache)
 *
 * WHAT: Hint to CPU to load cache line for writing
 * WHY:  Avoid RFO (Read-For-Ownership) stall on write
 * HOW:  __builtin_prefetch with write intent
 *
 * @param addr Address to prefetch for writing
 *
 * NOTE: Use before page_cow_view_write() calls for better performance
 */
#define PAGE_COW_PREFETCH_WRITE(addr) \
    __builtin_prefetch((void*)(addr), 1, 3)

/**
 * @brief Prefetch for streaming read (non-temporal - don't cache)
 *
 * WHAT: Hint for data that won't be reused soon
 * WHY:  Avoid polluting cache with one-time-use data
 * HOW:  __builtin_prefetch with low locality hint
 *
 * @param addr Address to prefetch
 *
 * USE CASE: Sequential scan of large dataset
 */
#define PAGE_COW_PREFETCH_STREAM(addr) \
    __builtin_prefetch((const void*)(addr), 0, 0)

/**
 * @brief Prefetch a page for COW operation
 *
 * WHAT: Prefetch both source and destination for page copy
 * WHY:  Page copy is memory-bound; prefetch hides latency
 * HOW:  Prefetch multiple cache lines spanning the page
 *
 * @param src_page Source page address
 * @param dst_page Destination page address (can be NULL)
 *
 * NOTE: Prefetches first 4 cache lines (256 bytes) of page
 */
#define PAGE_COW_PREFETCH_PAGE(src_page, dst_page) \
    do { \
        const char* _s = (const char*)(src_page); \
        char* _d = (char*)(dst_page); \
        PAGE_COW_PREFETCH_READ(_s); \
        PAGE_COW_PREFETCH_READ(_s + 64); \
        PAGE_COW_PREFETCH_READ(_s + 128); \
        PAGE_COW_PREFETCH_READ(_s + 192); \
        if (_d) { \
            PAGE_COW_PREFETCH_WRITE(_d); \
            PAGE_COW_PREFETCH_WRITE(_d + 64); \
            PAGE_COW_PREFETCH_WRITE(_d + 128); \
            PAGE_COW_PREFETCH_WRITE(_d + 192); \
        } \
    } while (0)

//=============================================================================
// Hash Table Types
//=============================================================================

/**
 * @brief Hash table entry for view address lookup
 *
 * WHAT: Single entry mapping address range to view handle
 * WHY:  Enable O(1) lookup by faulting address
 * HOW:  Store view's base address and extent for range check
 */
typedef struct page_cow_hash_entry {
    page_cow_view_t view;           /**< View handle (NULL = empty slot) */
    uintptr_t base_addr;            /**< View base address */
    size_t size;                    /**< View size in bytes */
    struct page_cow_hash_entry* next; /**< Next entry in chain (collision) */
} page_cow_hash_entry_t;

/**
 * @brief Hash table bucket
 *
 * WHAT: Head of collision chain for one hash bucket
 * WHY:  Open chaining for collision resolution
 * HOW:  Linked list of entries with same hash
 */
typedef struct {
    page_cow_hash_entry_t* head;    /**< First entry in chain (NULL = empty) */
    atomic_size_t count;            /**< Number of entries in chain */
} page_cow_hash_bucket_t;

/**
 * @brief Hash table for view address lookup
 *
 * WHAT: Complete hash table structure
 * WHY:  Replace O(n) linear search with O(1) hash lookup
 * HOW:  Array of buckets with chaining for collisions
 *
 * MEMORY: ~8KB for bucket array + ~24 bytes per registered view
 */
typedef struct {
    page_cow_hash_bucket_t buckets[PAGE_COW_HASH_BUCKETS]; /**< Hash buckets */
    atomic_size_t total_entries;    /**< Total entries across all buckets */
    atomic_size_t max_chain;        /**< Maximum chain length observed */
    atomic_flag lock;               /**< Spinlock for modifications */
    bool initialized;               /**< Initialization flag */
} page_cow_hash_table_t;

//=============================================================================
// Batch Operation Types
//=============================================================================

/**
 * @brief Batch privatization request
 *
 * WHAT: Specifies a contiguous range of pages to privatize
 * WHY:  Enable efficient batch COW for known write patterns
 * HOW:  Single lock acquisition for entire range
 */
typedef struct {
    page_cow_view_t view;           /**< View to privatize pages in */
    size_t start_page;              /**< Starting page index */
    size_t num_pages;               /**< Number of pages to privatize */
} page_cow_batch_request_t;

/**
 * @brief Batch privatization result
 *
 * WHAT: Result of batch privatization operation
 * WHY:  Report success/failure per range
 * HOW:  Count of pages actually privatized
 */
typedef struct {
    size_t pages_privatized;        /**< Number of pages made private */
    size_t pages_already_private;   /**< Pages that were already private */
    size_t pages_failed;            /**< Pages that failed to privatize */
    uint64_t copy_time_ns;          /**< Time spent copying (nanoseconds) */
} page_cow_batch_result_t;

/**
 * @brief Batch operation statistics
 *
 * WHAT: Aggregate statistics for batch operations
 * WHY:  Monitor efficiency of batch strategy
 * HOW:  Track counts and timing
 */
typedef struct {
    uint64_t batch_ops;             /**< Total batch operations */
    uint64_t pages_batched;         /**< Total pages processed in batches */
    uint64_t signals_avoided;       /**< Signal handler invocations avoided */
    uint64_t total_batch_time_ns;   /**< Total time in batch operations */
} page_cow_batch_stats_t;

//=============================================================================
// Hash Table API
//=============================================================================

/**
 * @brief Initialize the view hash table
 *
 * WHAT: Set up hash table for O(1) view lookup
 * WHY:  Must be called before registering views
 * HOW:  Zero buckets, initialize lock
 *
 * @param table Hash table to initialize
 * @return true on success, false on failure
 *
 * THREAD SAFETY: Not thread-safe; call once at startup
 */
NIMCP_EXPORT bool page_cow_hash_init(page_cow_hash_table_t* table);

/**
 * @brief Destroy the view hash table
 *
 * WHAT: Free all hash table resources
 * WHY:  Clean shutdown
 * HOW:  Free all entry chains, clear buckets
 *
 * @param table Hash table to destroy
 *
 * WARNING: All views must be unregistered first
 */
NIMCP_EXPORT void page_cow_hash_destroy(page_cow_hash_table_t* table);

/**
 * @brief Register a view in the hash table
 *
 * WHAT: Add view to hash table for fast lookup
 * WHY:  Enable O(1) signal handler lookup
 * HOW:  Hash base address, insert into bucket
 *
 * @param table Hash table
 * @param view View to register
 * @return true on success, false on failure
 *
 * COMPLEXITY: O(1) average, O(n) worst case with collisions
 * THREAD SAFETY: Thread-safe (uses spinlock)
 */
NIMCP_EXPORT bool page_cow_hash_register(
    page_cow_hash_table_t* table,
    page_cow_view_t view
);

/**
 * @brief Unregister a view from the hash table
 *
 * WHAT: Remove view from hash table
 * WHY:  Clean up on view destruction
 * HOW:  Find and remove entry from bucket chain
 *
 * @param table Hash table
 * @param view View to unregister
 * @return true if found and removed, false otherwise
 *
 * THREAD SAFETY: Thread-safe (uses spinlock)
 */
NIMCP_EXPORT bool page_cow_hash_unregister(
    page_cow_hash_table_t* table,
    page_cow_view_t view
);

/**
 * @brief Look up view by faulting address
 *
 * WHAT: Find view containing the given address
 * WHY:  Signal handler needs to identify which view faulted
 * HOW:  Hash address, search bucket for containing view
 *
 * @param table Hash table
 * @param addr Faulting address
 * @return View containing address, or NULL if not found
 *
 * COMPLEXITY: O(1) average, O(chain_length) worst case
 * THREAD SAFETY: Thread-safe (uses spinlock)
 * ASYNC-SIGNAL-SAFE: Yes (uses spinlock, not mutex)
 */
NIMCP_EXPORT page_cow_view_t page_cow_hash_lookup(
    page_cow_hash_table_t* table,
    void* addr
);

//=============================================================================
// Batch Privatization API
//=============================================================================

/**
 * @brief Batch privatize pages in a single operation
 *
 * WHAT: Make multiple pages private in one call
 * WHY:  Avoid signal handler overhead for known write patterns
 * HOW:  Lock once, privatize all pages, unlock
 *
 * @param request Batch request specifying view and page range
 * @param result Output result (can be NULL if not needed)
 * @return true on success (at least one page privatized)
 *
 * COMPLEXITY: O(num_pages * page_size) for copying
 * THREAD SAFETY: Thread-safe
 *
 * EXAMPLE:
 * ```c
 * // Pre-privatize pages we know we'll modify
 * page_cow_batch_request_t req = {
 *     .view = my_view,
 *     .start_page = 0,
 *     .num_pages = 64  // First 256KB
 * };
 * page_cow_batch_result_t result;
 * page_cow_batch_privatize(&req, &result);
 * printf("Privatized %zu pages\n", result.pages_privatized);
 * ```
 */
NIMCP_EXPORT bool page_cow_batch_privatize(
    const page_cow_batch_request_t* request,
    page_cow_batch_result_t* result
);

/**
 * @brief Batch privatize multiple ranges in one call
 *
 * WHAT: Process multiple privatization requests atomically
 * WHY:  Even more efficient for scattered write patterns
 * HOW:  Single lock for all operations
 *
 * @param requests Array of batch requests
 * @param num_requests Number of requests
 * @param results Output array (can be NULL, or same size as requests)
 * @return Number of successful requests
 *
 * THREAD SAFETY: Thread-safe
 */
NIMCP_EXPORT size_t page_cow_batch_privatize_multi(
    const page_cow_batch_request_t* requests,
    size_t num_requests,
    page_cow_batch_result_t* results
);

/**
 * @brief Get batch operation statistics
 *
 * @param stats Output statistics
 * @return true on success
 */
NIMCP_EXPORT bool page_cow_batch_get_stats(page_cow_batch_stats_t* stats);

/**
 * @brief Reset batch operation statistics
 */
NIMCP_EXPORT void page_cow_batch_reset_stats(void);

//=============================================================================
// Inline Helper Functions
//=============================================================================

/**
 * @brief Compute hash for address (page-aligned)
 *
 * WHAT: Hash function for view base addresses
 * WHY:  Distribute views across buckets
 * HOW:  Shift by page bits, multiply-shift hash
 *
 * @param addr Address to hash
 * @return Hash bucket index [0, PAGE_COW_HASH_BUCKETS)
 *
 * NOTE: Uses multiply-shift method for good distribution
 */
static inline size_t page_cow_hash_addr(uintptr_t addr) {
    // Remove page offset bits (address is page-aligned for view base)
    uintptr_t page_num = addr >> 12;  // Assuming 4KB pages

    // Multiply-shift hash (Knuth's golden ratio method)
    // 0x9E3779B97F4A7C15 is 2^64 / phi (golden ratio)
    const uint64_t mult = 0x9E3779B97F4A7C15ULL;
    uint64_t hash = (uint64_t)page_num * mult;

    // Take high bits for better distribution
    size_t bucket = (hash >> (64 - 10)) & (PAGE_COW_HASH_BUCKETS - 1);
    return bucket;
}

/**
 * @brief Check if address falls within view range
 *
 * WHAT: Range check for view membership
 * WHY:  Multiple views may hash to same bucket
 * HOW:  Compare against base and extent
 *
 * @param entry Hash table entry
 * @param addr Address to check
 * @return true if address is within entry's view range
 */
static inline bool page_cow_addr_in_entry(
    const page_cow_hash_entry_t* entry,
    uintptr_t addr
) {
    return (addr >= entry->base_addr) &&
           (addr < entry->base_addr + entry->size);
}

/**
 * @brief Calculate page index from faulting address
 *
 * WHAT: Get page index within view from faulting address
 * WHY:  Need page index for COW operation
 * HOW:  Offset from base divided by page size
 *
 * @param view_base View base address
 * @param fault_addr Faulting address
 * @return Page index
 */
static inline size_t page_cow_fault_to_page_idx(
    uintptr_t view_base,
    uintptr_t fault_addr
) {
    return (fault_addr - view_base) / PAGE_COW_PAGE_SIZE;
}

/**
 * @brief Prefetch N pages ahead for sequential access
 *
 * WHAT: Prefetch multiple pages in sequence
 * WHY:  Improve performance for sequential iteration
 * HOW:  Issue prefetch for next N cache lines
 *
 * @param base Base address of page
 * @param page_idx Current page index
 * @param total_pages Total pages in view
 * @param lookahead Number of pages to prefetch ahead
 */
static inline void page_cow_prefetch_ahead(
    const void* base,
    size_t page_idx,
    size_t total_pages,
    size_t lookahead
) {
    for (size_t i = 1; i <= lookahead && page_idx + i < total_pages; i++) {
        const char* page = (const char*)base + (page_idx + i) * PAGE_COW_PAGE_SIZE;
        PAGE_COW_PREFETCH_READ(page);
    }
}

/**
 * @brief Prefetch pages for batch privatization
 *
 * WHAT: Prefetch source pages before batch copy
 * WHY:  Hide memory latency during batch operations
 * HOW:  Issue prefetch for each page in range
 *
 * @param region_base Region base address
 * @param start_page Starting page index
 * @param num_pages Number of pages to prefetch
 */
static inline void page_cow_prefetch_batch(
    const void* region_base,
    size_t start_page,
    size_t num_pages
) {
    for (size_t i = 0; i < num_pages && i < 8; i++) {  // Limit to 8 to avoid prefetch queue overflow
        const char* page = (const char*)region_base + (start_page + i) * PAGE_COW_PAGE_SIZE;
        PAGE_COW_PREFETCH_READ(page);
    }
}

#ifdef __cplusplus
}
#endif

#endif  // NIMCP_PAGE_COW_OPT_H
