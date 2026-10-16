/**
 * @file nimcp_memory.h
 * @brief Memory tracking and debugging system
 *
 * WHAT: Wrapper around malloc/calloc/realloc/free with leak detection
 * WHY: Catch memory leaks, buffer overflows, and double-frees during development
 * HOW: Canary guards, allocation tracking, pattern analysis
 *
 * USAGE:
 *   // Enable tracking at startup
 *   nimcp_memory_init();
 *   nimcp_memory_enable_tracking(true);
 *
 *   // Use instead of malloc/calloc/free
 *   void* ptr = nimcp_malloc(100);
 *   nimcp_free(ptr);
 *
 *   // Check for leaks before shutdown
 *   nimcp_memory_check_leaks();
 *   nimcp_memory_cleanup();
 *
 * PERFORMANCE:
 *   - ~40 bytes overhead per allocation (tracking + canaries)
 *   - ~2-5% time overhead (mutex locks, linked list updates)
 *   - Recommended for DEBUG builds only
 */

#ifndef NIMCP_MEMORY_H
#define NIMCP_MEMORY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Statistics and Tracking Structures
//=============================================================================

/**
 * WHAT: Memory usage statistics
 * WHY: Monitor memory consumption and allocation patterns
 */
typedef struct {
    size_t total_allocated;    /**< Total bytes ever allocated */
    size_t current_allocated;  /**< Current bytes in use */
    size_t peak_allocated;     /**< Peak memory usage */
    size_t allocation_count;   /**< Number of allocations */
    size_t free_count;         /**< Number of frees */
    size_t failed_allocations; /**< Failed allocation attempts */
} nimcp_memory_stats_t;

//=============================================================================
// Core Memory Functions (Drop-in replacements for malloc/free)
//=============================================================================

/**
 * WHAT: Allocate memory with tracking
 * WHY: Track allocations and detect leaks
 * HOW: Wraps malloc with canary guards and tracking
 *
 * @param size Bytes to allocate
 * @return Pointer to allocated memory or NULL on failure
 */
void* nimcp_malloc(size_t size);

/**
 * WHAT: Allocate zero-initialized memory with tracking
 * WHY: Track allocations and ensure zero initialization
 * HOW: Wraps calloc with canary guards and tracking
 *
 * @param count Number of elements
 * @param size Size of each element
 * @return Pointer to allocated memory or NULL on failure
 */
void* nimcp_calloc(size_t count, size_t size);

/**
 * WHAT: Allocate aligned memory with tracking
 * WHY: Required for SIMD operations, atomic ops, and struct alignment
 * HOW: Uses aligned_alloc with canary guards and tracking
 *
 * ALIGNMENT REQUIREMENTS:
 * - alignment must be power of 2
 * - alignment must be multiple of sizeof(void*)
 * - size must be multiple of alignment
 *
 * @param alignment Alignment in bytes (must be power of 2)
 * @param size Bytes to allocate (will be rounded up to alignment)
 * @return Pointer to aligned memory or NULL on failure
 */
void* nimcp_aligned_alloc(size_t alignment, size_t size);

/**
 * WHAT: Resize memory allocation
 * WHY: Track reallocations and update tracking info
 * HOW: Untrack old pointer, realloc, track new pointer
 *
 * @param ptr Pointer to resize (or NULL for new allocation)
 * @param new_size New size in bytes
 * @return Pointer to resized memory or NULL on failure
 */
void* nimcp_realloc(void* ptr, size_t new_size);

/**
 * WHAT: Free memory with tracking
 * WHY: Detect double-frees and buffer overflows
 * HOW: Check canaries, untrack, then free
 *
 * @param ptr Pointer to free (NULL is safe)
 */
void nimcp_free(void* ptr);

/**
 * WHAT: Duplicate string with tracking
 * WHY: Track string allocations
 * HOW: Uses nimcp_malloc internally
 *
 * @param str String to duplicate
 * @return Pointer to duplicated string or NULL on failure
 */
char* nimcp_strdup(const char* str);

//=============================================================================
// Aligned Memory Functions
//=============================================================================

/**
 * WHAT: Allocate aligned memory
 * WHY: Some data structures need specific alignment (SIMD, cache lines)
 * HOW: Uses posix_memalign with tracking
 *
 * @param size Bytes to allocate
 * @param alignment Alignment boundary (must be power of 2)
 * @return Pointer to aligned memory or NULL on failure
 */
void* nimcp_aligned_malloc(size_t size, size_t alignment);

/**
 * WHAT: Free aligned memory
 * WHY: Cleanup aligned allocations
 *
 * @param ptr Pointer to free
 */
void nimcp_aligned_free(void* ptr);

//=============================================================================
// Initialization and Cleanup
//=============================================================================

/**
 * WHAT: Initialize memory tracking system
 * WHY: Set up tracking structures before use
 * HOW: Initializes mutex, stats, and tracking lists
 *
 * Call this at application startup (idempotent - safe to call multiple times)
 */
void nimcp_memory_init(void);

/**
 * WHAT: Clean up memory tracking system
 * WHY: Free tracking structures and report leaks
 * HOW: Walks allocation list, reports leaks, cleans up
 *
 * Call this at application shutdown
 */
void nimcp_memory_cleanup(void);

//=============================================================================
// Configuration
//=============================================================================

/**
 * WHAT: Enable or disable allocation tracking
 * WHY: Control overhead (disable in production, enable in debug)
 * HOW: Sets tracking_enabled flag
 *
 * @param enable true to enable tracking, false to disable
 */
void nimcp_memory_enable_tracking(bool enable);

/**
 * WHAT: Enable or disable debug output
 * WHY: Control verbosity of memory operations
 * HOW: Sets debug_output flag
 *
 * @param enable true to enable debug output, false to disable
 */
void nimcp_memory_enable_debug_output(bool enable);

//=============================================================================
// Statistics and Reporting
//=============================================================================

/**
 * WHAT: Get current memory statistics
 * WHY: Monitor memory usage programmatically
 * HOW: Thread-safe copy of stats structure
 *
 * @param stats Output parameter for statistics
 * @return true on success, false if stats is NULL
 */
bool nimcp_memory_get_stats(nimcp_memory_stats_t* stats);

/**
 * WHAT: Clear memory statistics
 * WHY: Reset counters for profiling specific sections
 * HOW: Zeros out stats structure (thread-safe)
 */
void nimcp_memory_clear_stats(void);

/**
 * WHAT: Reset entire memory tracker state
 * WHY: Restore memory tracker to fresh state for test isolation
 * HOW: Free all tracking structures, reset stats, clear lists
 *
 * USAGE: Call between tests to prevent state contamination
 * WARNING: Does not free user allocations - caller must free those first
 * THREAD-SAFE: Yes (acquires lock)
 */
void nimcp_memory_reset_state(void);

/**
 * WHAT: Dump all current allocations to console
 * WHY: Debug memory usage and find allocation hotspots
 * HOW: Walks allocation list and prints each block
 */
void nimcp_memory_dump_allocations(void);

/**
 * WHAT: Check for memory leaks
 * WHY: Find leaked memory before shutdown
 * HOW: Walks allocation list and reports all unfreed blocks
 */
void nimcp_memory_check_leaks(void);

/**
 * WHAT: Analyze allocation patterns
 * WHY: Understand allocation behavior and optimize
 * HOW: Reports allocation sizes, counts, lifetimes
 */
void nimcp_memory_analyze_patterns(void);

//=============================================================================
// Build Configuration Macros
//=============================================================================

/**
 * WHAT: Optionally replace standard allocators with tracking versions
 * WHY: Automatic tracking without code changes
 * HOW: Macro replacement (use with caution)
 *
 * Define NIMCP_OVERRIDE_MALLOC to enable this behavior
 */
#ifdef NIMCP_OVERRIDE_MALLOC
    #define malloc(s) nimcp_malloc(s)
    #define calloc(c, s) nimcp_calloc(c, s)
    #define realloc(p, s) nimcp_realloc(p, s)
    #define free(p) nimcp_free(p)
    #define strdup(s) nimcp_strdup(s)
#endif

#ifdef __cplusplus
}
#endif

#endif  // NIMCP_MEMORY_H
