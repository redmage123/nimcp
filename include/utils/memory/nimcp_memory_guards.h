/**
 * @file nimcp_memory_guards.h
 * @brief Memory corruption detection with canaries and guards
 *
 * WHAT: Detect memory corruption, overflows, and double-frees
 * WHY:  Catch bugs early before they cause crashes or security issues
 * HOW:  Add canary values around allocations, track allocation state
 *
 * FEATURES:
 * - Canary-based overflow detection (0xDEADBEEF / 0xCAFEBABE)
 * - Double-free detection
 * - Use-after-free detection
 * - Allocation tracking with file/line info
 * - Memory leak detection
 * - Buffer overflow detection
 *
 * USAGE:
 * 1. Enable at runtime: memory_guards_init()
 * 2. Use guarded allocation: nimcp_malloc_guarded(size, __FILE__, __LINE__)
 * 3. Use guarded free: nimcp_free_guarded(ptr, __FILE__, __LINE__)
 * 4. Or use macros: nimcp_malloc(size), nimcp_free(ptr) - auto file/line
 * 5. Periodic checks: memory_guards_check_all()
 * 6. Report leaks: memory_guards_report_leaks()
 *
 * PERFORMANCE IMPACT:
 * - Adds 32 bytes overhead per allocation (2 canaries + metadata)
 * - Minimal runtime cost for alloc/free (~5% overhead)
 * - Periodic checks are O(N) where N = active allocations
 *
 * @author NIMCP Team
 * @date 2025-11-09
 */

#ifndef NIMCP_MEMORY_GUARDS_H
#define NIMCP_MEMORY_GUARDS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Configuration
//=============================================================================

#define CANARY_START  0xDEADBEEF  /**< Start canary magic value */
#define CANARY_END    0xCAFEBABE  /**< End canary magic value */
#define FREED_MARKER  0xFEEDFACE  /**< Marker for freed memory */

/**
 * @brief Memory guard statistics
 */
typedef struct {
    uint64_t total_allocations;       /**< Total allocations made */
    uint64_t total_frees;              /**< Total frees made */
    uint64_t active_allocations;       /**< Current active allocations */
    uint64_t total_bytes_allocated;    /**< Total bytes allocated */
    uint64_t active_bytes;             /**< Current active bytes */
    uint64_t peak_bytes;               /**< Peak memory usage */
    uint64_t peak_allocations;         /**< Peak allocation count */

    // Error counts
    uint64_t buffer_overflows_detected;  /**< Overflows caught */
    uint64_t double_frees_detected;      /**< Double-frees caught */
    uint64_t use_after_frees_detected;   /**< Use-after-free caught */
    uint64_t corruption_detected;        /**< Other corruption */
    uint64_t leaks_detected;             /**< Memory leaks found */
} memory_guard_stats_t;

/**
 * @brief Memory guard configuration
 */
typedef struct {
    bool enable_guards;              /**< Enable guard system */
    bool enable_leak_detection;      /**< Track allocations for leak detection */
    bool enable_overflow_detection;  /**< Check canaries on free */
    bool enable_double_free_detection; /**< Detect double-frees */
    bool enable_use_after_free_detection; /**< Poison freed memory */
    bool abort_on_error;             /**< Abort on corruption (vs log only) */
    uint32_t check_frequency;        /**< How often to check all guards (0=never) */
} memory_guard_config_t;

//=============================================================================
// Public API
//=============================================================================

/**
 * @brief Initialize memory guard system
 *
 * WHAT: Setup guard system with given configuration
 * WHY:  Enable memory safety checks
 * HOW:  Initialize tracking data structures
 *
 * @param config Configuration (NULL for defaults)
 * @return true on success
 */
bool memory_guards_init(const memory_guard_config_t* config);

/**
 * @brief Shutdown memory guard system
 *
 * WHAT: Clean up and report final statistics
 * WHY:  Detect leaks at program exit
 * HOW:  Check all tracked allocations, report leaks
 */
void memory_guards_shutdown(void);

/**
 * @brief Get default configuration
 *
 * @return Default config with guards enabled
 */
memory_guard_config_t memory_guards_default_config(void);

/**
 * @brief Allocate memory with guards
 *
 * WHAT: Allocate memory with canaries and tracking
 * WHY:  Detect overflows and track allocation
 * HOW:  Allocate extra space for canaries + header
 *
 * @param size Size to allocate (user bytes)
 * @param file Source file (__FILE__)
 * @param line Source line (__LINE__)
 * @return Pointer to user data, or NULL on failure
 */
void* nimcp_malloc_guarded(size_t size, const char* file, int line);

/**
 * @brief Allocate zeroed memory with guards
 *
 * @param nmemb Number of elements
 * @param size Size of each element
 * @param file Source file (__FILE__)
 * @param line Source line (__LINE__)
 * @return Pointer to zeroed memory, or NULL on failure
 */
void* nimcp_calloc_guarded(size_t nmemb, size_t size, const char* file, int line);

/**
 * @brief Reallocate memory with guards
 *
 * @param ptr Existing pointer (can be NULL)
 * @param size New size
 * @param file Source file (__FILE__)
 * @param line Source line (__LINE__)
 * @return Pointer to reallocated memory, or NULL on failure
 */
void* nimcp_realloc_guarded(void* ptr, size_t size, const char* file, int line);

/**
 * @brief Free guarded memory
 *
 * WHAT: Free memory and check guards
 * WHY:  Detect corruption and double-frees
 * HOW:  Verify canaries, mark as freed, call nimcp_free()
 *
 * @param ptr Pointer to free
 * @param file Source file (__FILE__)
 * @param line Source line (__LINE__)
 */
void nimcp_free_guarded(void* ptr, const char* file, int line);

/**
 * @brief Check guards on specific allocation
 *
 * WHAT: Verify canaries are intact
 * WHY:  Detect buffer overflows
 * HOW:  Read and compare canary values
 *
 * @param ptr Pointer to check
 * @return true if guards intact, false if corrupted
 */
bool memory_guards_check_ptr(void* ptr);

/**
 * @brief Check all tracked allocations
 *
 * WHAT: Verify all active allocations have intact guards
 * WHY:  Periodic corruption detection
 * HOW:  Iterate tracking table, check each canary
 *
 * @return Number of corruptions found
 */
uint32_t memory_guards_check_all(void);

/**
 * @brief Get memory guard statistics
 *
 * @return Current statistics
 */
memory_guard_stats_t memory_guards_get_stats(void);

/**
 * @brief Print memory guard statistics
 */
void memory_guards_print_stats(void);

/**
 * @brief Report memory leaks
 *
 * WHAT: Print all active allocations (leaks)
 * WHY:  Find memory leaks at shutdown
 * HOW:  Iterate tracking table, print file/line info
 *
 * @return Number of leaks found
 */
uint32_t memory_guards_report_leaks(void);

/**
 * @brief Enable/disable guards at runtime
 *
 * @param enable True to enable, false to disable
 */
void memory_guards_set_enabled(bool enable);

/**
 * @brief Check if guards are enabled
 *
 * @return true if enabled
 */
bool memory_guards_is_enabled(void);

//=============================================================================
// Convenience Macros (for automatic file/line tracking)
//=============================================================================

#ifdef NIMCP_ENABLE_MEMORY_GUARDS

// Guarded allocation macros (auto file/line)
#define nimcp_malloc(size)           nimcp_malloc_guarded((size), __FILE__, __LINE__)
#define nimcp_calloc(nmemb, size)    nimcp_calloc_guarded((nmemb), (size), __FILE__, __LINE__)
#define nimcp_realloc(ptr, size)     nimcp_realloc_guarded((ptr), (size), __FILE__, __LINE__)
#define nimcp_free(ptr)              nimcp_free_guarded((ptr), __FILE__, __LINE__)

#else

// Fallback to nimcp_memory.h functions (memory tracking without guards)
// Include nimcp_memory.h to get function declarations
#include "utils/memory/nimcp_memory.h"

// NOTE: nimcp_malloc, nimcp_calloc, etc. are now function declarations
// from nimcp_memory.h, NOT macros. This ensures proper memory tracking.

#endif // NIMCP_ENABLE_MEMORY_GUARDS

#ifdef __cplusplus
}
#endif

#endif // NIMCP_MEMORY_GUARDS_H
