/**
 * @file nimcp_cache.h
 * @brief Copy-on-Write (COW) caching system for efficient memory sharing
 *
 * WHAT: Reference-counted memory sharing with copy-on-write semantics
 * WHY: Reduce memory usage when cloning/replicating brain structures
 * HOW: Atomic reference counting + lazy copying on first write
 *
 * USAGE:
 *   // Allocate cached memory
 *   void* data = nimcp_cache_alloc(1024 * 1024);  // 1MB
 *   memcpy(data, source_data, size);
 *
 *   // Create multiple references (no copy)
 *   void* ref1 = nimcp_cache_reference(data);  // Fast: just increment ref count
 *   void* ref2 = nimcp_cache_reference(data);
 *
 *   // Make writable (triggers copy if shared)
 *   void* writable = nimcp_cache_make_writable(ref1);
 *   // Now ref1 has private copy, ref2 still shares original
 *
 *   // Release references
 *   nimcp_cache_release(ref1);
 *   nimcp_cache_release(ref2);
 *   nimcp_cache_release(data);  // Frees when ref_count reaches 0
 *
 * PERFORMANCE:
 *   - Reference: O(1) atomic increment (~5 CPU cycles)
 *   - Make writable: O(n) copy if shared, O(1) if already private
 *   - Memory overhead: 48 bytes per allocation
 *   - Thread-safe with atomic operations
 */

#ifndef NIMCP_CACHE_H
#define NIMCP_CACHE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Note: For Phase 1, using regular uint32_t with mutex protection
// instead of atomic_uint_fast32_t to avoid C++/C11 compatibility issues
// Full atomic implementation will be added in Phase 2

//=============================================================================
// Cache Statistics and Configuration
//=============================================================================

/**
 * WHAT: Cache system statistics
 * WHY: Monitor memory savings and cache efficiency
 */
typedef struct {
    uint64_t allocations_created;   /**< Total cached allocations created */
    uint64_t references_created;    /**< Total references created */
    uint64_t copies_triggered;      /**< Times make_writable copied data */
    size_t memory_allocated;        /**< Total memory allocated (bytes) */
    size_t memory_shared;           /**< Memory currently shared via COW */
    size_t memory_saved;            /**< Memory saved by COW (estimated) */
    uint32_t active_allocations;    /**< Currently active allocations */
    uint32_t peak_allocations;      /**< Peak concurrent allocations */
} nimcp_cache_stats_t;

/**
 * WHAT: Cache configuration options
 * WHY: Control cache behavior and limits
 */
typedef struct {
    size_t max_total_memory;        /**< Max total cached memory (0 = unlimited) */
    size_t max_single_allocation;   /**< Max size for single cached allocation */
    bool enable_tracking;           /**< Track allocations for debugging */
    bool enable_debug_output;       /**< Print debug messages */
} nimcp_cache_config_t;

//=============================================================================
// Core Cache Functions
//=============================================================================

/**
 * WHAT: Allocate cached memory
 * WHY: Create shareable memory with reference counting
 * HOW: Wraps allocation with COW metadata
 *
 * @param size Bytes to allocate
 * @return Pointer to allocated memory or NULL on failure
 *
 * NOTE: Use nimcp_cache_release() to free, not nimcp_free()
 */
void* nimcp_cache_alloc(size_t size);

/**
 * WHAT: Allocate zero-initialized cached memory
 * WHY: Convenience function for common pattern
 * HOW: Allocates and zeros memory
 *
 * @param count Number of elements
 * @param size Size of each element
 * @return Pointer to allocated memory or NULL on failure
 */
void* nimcp_cache_calloc(size_t count, size_t size);

/**
 * WHAT: Create new reference to cached memory
 * WHY: Share memory without copying
 * HOW: Atomic increment of reference count
 *
 * @param ptr Pointer to cached memory
 * @return Same pointer (with incremented ref count) or NULL if invalid
 *
 * PERFORMANCE: ~5 CPU cycles (atomic increment)
 * THREAD-SAFE: Yes
 */
void* nimcp_cache_reference(void* ptr);

/**
 * WHAT: Make cached memory writable (copy if shared)
 * WHY: Enable writing to shared memory
 * HOW: Copies data if ref_count > 1, returns same pointer if ref_count == 1
 *
 * @param ptr Pointer to cached memory
 * @return Writable pointer (may be same or new copy)
 *
 * PERFORMANCE: O(1) if already private, O(n) if shared
 * THREAD-SAFE: Yes
 *
 * IMPORTANT: Old pointer may become invalid. Always use returned pointer:
 *   ptr = nimcp_cache_make_writable(ptr);
 */
void* nimcp_cache_make_writable(void* ptr);

/**
 * WHAT: Release reference to cached memory
 * WHY: Decrement ref count and free if last reference
 * HOW: Atomic decrement, free if count reaches 0
 *
 * @param ptr Pointer to cached memory (NULL is safe)
 *
 * THREAD-SAFE: Yes
 */
void nimcp_cache_release(void* ptr);

//=============================================================================
// Query Functions
//=============================================================================

/**
 * WHAT: Check if memory is cache-managed
 * WHY: Verify pointer is cache-managed
 * HOW: Looks up in tracking table
 *
 * @param ptr Pointer to check
 * @return true if cache-managed, false otherwise
 */
bool nimcp_cache_is_cached(const void* ptr);

/**
 * WHAT: Check if cached memory is shared
 * WHY: Determine if write will trigger copy
 * HOW: Returns true if ref_count > 1
 *
 * @param ptr Pointer to cached memory
 * @return true if shared (ref_count > 1), false if private or invalid
 */
bool nimcp_cache_is_shared(const void* ptr);

/**
 * WHAT: Get reference count for cached memory
 * WHY: Debug and monitoring
 * HOW: Atomic load of ref_count
 *
 * @param ptr Pointer to cached memory
 * @return Reference count or 0 if invalid
 */
uint32_t nimcp_cache_get_refcount(const void* ptr);

/**
 * WHAT: Get size of cached allocation
 * WHY: Query allocation size
 * HOW: Returns size from metadata
 *
 * @param ptr Pointer to cached memory
 * @return Size in bytes or 0 if invalid
 */
size_t nimcp_cache_get_size(const void* ptr);

//=============================================================================
// Initialization and Configuration
//=============================================================================

/**
 * WHAT: Initialize cache system
 * WHY: Set up tracking structures
 * HOW: Allocates hash table for tracking
 *
 * Call at application startup (idempotent - safe to call multiple times)
 */
void nimcp_cache_init(void);

/**
 * WHAT: Shutdown cache system
 * WHY: Free tracking structures and report leaks
 * HOW: Walks tracking table, reports leaks, cleans up
 *
 * Call at application shutdown
 */
void nimcp_cache_cleanup(void);

/**
 * WHAT: Configure cache system
 * WHY: Set limits and behavior
 * HOW: Copies config to global state
 *
 * @param config Configuration structure
 */
void nimcp_cache_configure(const nimcp_cache_config_t* config);

/**
 * WHAT: Get default configuration
 * WHY: Convenience function for initialization
 * HOW: Returns sensible defaults
 *
 * @return Default configuration
 */
nimcp_cache_config_t nimcp_cache_get_default_config(void);

//=============================================================================
// Statistics and Debugging
//=============================================================================

/**
 * WHAT: Get cache statistics
 * WHY: Monitor memory savings and efficiency
 * HOW: Thread-safe copy of stats
 *
 * @param stats Output parameter for statistics
 * @return true on success, false if stats is NULL
 */
bool nimcp_cache_get_stats(nimcp_cache_stats_t* stats);

/**
 * WHAT: Clear cache statistics
 * WHY: Reset counters for profiling
 * HOW: Zeros stats (preserves memory_allocated)
 */
void nimcp_cache_clear_stats(void);

/**
 * WHAT: Record external COW reference creation
 * WHY: Track COW statistics for non-cache COW implementations
 * HOW: Updates references_created and memory_saved counters
 *
 * @param size Size of shared memory being referenced
 *
 * NOTE: Use this when implementing COW with manual reference counting
 * instead of nimcp_cache functions, to maintain accurate statistics.
 */
void nimcp_cache_record_reference(size_t size);

/**
 * WHAT: Dump all cached allocations
 * WHY: Debug memory usage
 * HOW: Walks tracking table and prints
 */
void nimcp_cache_dump_allocations(void);

/**
 * WHAT: Check for cache leaks
 * WHY: Find unreleased cached allocations
 * HOW: Reports all allocations with ref_count > 0
 */
void nimcp_cache_check_leaks(void);

/**
 * WHAT: Analyze cache efficiency
 * WHY: Understand cache benefits
 * HOW: Calculates memory saved, copy rate, etc.
 */
void nimcp_cache_analyze_efficiency(void);

//=============================================================================
// Advanced Functions
//=============================================================================

/**
 * WHAT: Force copy of cached memory (make independent)
 * WHY: Explicitly break sharing
 * HOW: Always copies, decrements source ref_count
 *
 * @param ptr Pointer to cached memory
 * @return New independent copy or NULL on failure
 */
void* nimcp_cache_force_copy(void* ptr);

/**
 * WHAT: Check if two pointers share cached memory
 * WHY: Detect sharing relationships
 * HOW: Compares underlying cache headers
 *
 * @param ptr1 First pointer
 * @param ptr2 Second pointer
 * @return true if both point to same cached allocation
 */
bool nimcp_cache_are_shared(const void* ptr1, const void* ptr2);

/**
 * WHAT: Get cached allocation metadata as string
 * WHY: Debugging and logging
 * HOW: Formats info into provided buffer
 *
 * @param ptr Pointer to cached memory
 * @param buffer Output buffer
 * @param buffer_size Size of buffer
 * @return true on success
 */
bool nimcp_cache_get_info(const void* ptr, char* buffer, size_t buffer_size);

//=============================================================================
// Build Configuration
//=============================================================================

/**
 * WHAT: Enable detailed cache tracking
 * WHY: Debug mode with extra validation
 * HOW: Define at compile time
 *
 * When defined, cache system tracks:
 * - Allocation source location (file, line)
 * - Allocation timestamps
 * - Operation history
 */
#ifdef NIMCP_CACHE_DEBUG
    #define nimcp_cache_alloc_debug(size, file, line) \
        nimcp_cache_alloc_internal(size, file, line)
    #define nimcp_cache_alloc(size) \
        nimcp_cache_alloc_internal(size, __FILE__, __LINE__)
#endif

#ifdef __cplusplus
}
#endif

#endif  // NIMCP_CACHE_H
