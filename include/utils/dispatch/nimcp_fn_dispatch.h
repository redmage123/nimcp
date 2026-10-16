/**
 * @file nimcp_fn_dispatch.h
 * @brief Function dispatch table for hot-swappable functions
 *
 * WHAT: Thread-safe function dispatch table with hot-swap, versioning, and quarantine
 * WHY:  Enable runtime function replacement for live patching, A/B testing, and fault isolation
 * HOW:  Hash table with RW locks, atomic pointer swaps, version history, and crash tracking
 *
 * ARCHITECTURE:
 *
 *   Application Code                    Dispatch Table
 *   ┌─────────────────┐                 ┌──────────────────────────────┐
 *   │ fn_dispatch_get │───────────────> │ Entry: "nimcp_brain_create"  │
 *   │  (name lookup)  │                 │ ├── current_ptr   ─────────┐ │
 *   └─────────────────┘                 │ ├── original_ptr           │ │
 *                                       │ ├── patch_history[]        │ │
 *                                       │ ├── version: 3             │ │
 *                                       │ ├── rwlock                 │ │
 *                                       │ ├── quarantined: false     │ │
 *                                       │ ├── call_count: 1234       │ │
 *                                       │ └── crash_count: 0         │ │
 *                                       └──────────────────────────────┘
 *                                                       │
 *                                                       v
 *                                       ┌──────────────────────────────┐
 *                                       │ nimcp_brain_create_v3()      │
 *                                       │ (current implementation)     │
 *                                       └──────────────────────────────┘
 *
 * FEATURES:
 * - Auto-registration of NIMCP_EXPORT functions at library load
 * - Thread-safe function pointer swap (atomic with RW lock)
 * - Version tracking with rollback capability
 * - Quarantine for repeatedly crashing functions
 * - Call and crash statistics for monitoring
 *
 * THREAD SAFETY:
 * - Reader-writer locks for concurrent read access
 * - Exclusive write lock for swaps and modifications
 * - Atomic pointer swap for lock-free fast path (where supported)
 *
 * PERFORMANCE:
 * - O(1) average lookup (hash table)
 * - Lock-free reads with atomic pointer (if not quarantined)
 * - Write operations acquire exclusive lock
 *
 * USAGE:
 * ```c
 * // Create dispatch table
 * fn_dispatch_table_t* table = fn_dispatch_create();
 *
 * // Auto-register all NIMCP_EXPORT functions
 * fn_dispatch_auto_register(table);
 *
 * // Get function pointer for calling
 * void* fn = fn_dispatch_get(table, "nimcp_brain_create");
 *
 * // Hot-swap a function
 * void* old_fn;
 * fn_dispatch_swap(table, "nimcp_brain_create", new_impl, &old_fn);
 *
 * // Rollback to previous version
 * fn_dispatch_rollback(table, "nimcp_brain_create", 1);
 *
 * // Quarantine a crashing function
 * fn_dispatch_quarantine(table, "nimcp_buggy_fn");
 *
 * // Cleanup
 * fn_dispatch_destroy(table);
 * ```
 *
 * @author NIMCP Development Team
 * @date 2025-12-27
 * @version 1.0.0
 */

#ifndef NIMCP_FN_DISPATCH_H
#define NIMCP_FN_DISPATCH_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <pthread.h>
#include "utils/thread/nimcp_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Export Macro
//=============================================================================

#ifndef NIMCP_EXPORT
    #if defined(__GNUC__) && __GNUC__ >= 4
        #define NIMCP_EXPORT __attribute__((visibility("default")))
    #else
        #define NIMCP_EXPORT
    #endif
#endif

//=============================================================================
// Constants
//=============================================================================

/** @brief Maximum function name length */
#define FN_DISPATCH_NAME_MAX 128

/** @brief Default initial capacity for dispatch table */
#define FN_DISPATCH_DEFAULT_CAPACITY 256

/** @brief Default initial capacity for patch history */
#define FN_DISPATCH_HISTORY_INITIAL_CAPACITY 16

/** @brief Crash count threshold for auto-quarantine */
#define FN_DISPATCH_AUTO_QUARANTINE_THRESHOLD 5

/** @brief Magic value for validation */
#define FN_DISPATCH_MAGIC 0x464E4453  // 'FNDS'

//=============================================================================
// Error Codes
//=============================================================================

/** @brief Dispatch-specific error codes */
typedef enum {
    FN_DISPATCH_OK = 0,                    /**< Success */
    FN_DISPATCH_ERR_NULL = -1,             /**< NULL pointer argument */
    FN_DISPATCH_ERR_NOT_FOUND = -2,        /**< Function not found */
    FN_DISPATCH_ERR_ALREADY_EXISTS = -3,   /**< Function already registered */
    FN_DISPATCH_ERR_NO_MEMORY = -4,        /**< Memory allocation failed */
    FN_DISPATCH_ERR_QUARANTINED = -5,      /**< Function is quarantined */
    FN_DISPATCH_ERR_NO_HISTORY = -6,       /**< No history for rollback */
    FN_DISPATCH_ERR_INVALID_STATE = -7,    /**< Invalid table state */
    FN_DISPATCH_ERR_LOCK_FAILED = -8,      /**< Lock acquisition failed */
    FN_DISPATCH_ERR_DLOPEN = -9,           /**< dlopen/dlsym failed */
    FN_DISPATCH_ERR_PARSE = -10            /**< ELF/symbol parsing failed */
} fn_dispatch_error_t;

//=============================================================================
// Type Definitions
//=============================================================================

/**
 * @brief Dispatch table entry for a single function
 *
 * WHAT: Tracks a function's current pointer, history, and statistics
 * WHY:  Enable hot-swap with rollback and crash detection
 * HOW:  Store versions in history array, protect with RW lock
 */
typedef struct {
    char name[FN_DISPATCH_NAME_MAX];   /**< Function name */
    void* current_ptr;                  /**< Current function pointer */
    void* original_ptr;                 /**< Original function pointer */
    void** patch_history;               /**< History of previous pointers */
    uint32_t patch_count;               /**< Number of patches applied */
    uint32_t patch_capacity;            /**< Capacity of patch_history */
    uint32_t version;                   /**< Current version number */
    pthread_rwlock_t lock;              /**< Reader-writer lock for entry */
    bool quarantined;                   /**< Whether function is quarantined */
    uint64_t call_count;                /**< Number of calls to this function */
    uint64_t crash_count;               /**< Number of crashes in this function */
} fn_dispatch_entry_t;

/**
 * @brief Function dispatch table
 *
 * WHAT: Container for all dispatch entries with global table lock
 * WHY:  Centralize function dispatch for hot-swapping
 * HOW:  Dynamic array of entries protected by mutex
 */
typedef struct fn_dispatch_table {
    fn_dispatch_entry_t* entries;      /**< Array of dispatch entries */
    uint32_t count;                     /**< Number of registered functions */
    uint32_t capacity;                  /**< Capacity of entries array */
    nimcp_mutex_t table_lock;         /**< Global table lock for modifications */
    bool auto_registered;               /**< Whether auto-registration completed */
    uint32_t magic;                     /**< Magic number for validation */
} fn_dispatch_table_t;

/**
 * @brief Statistics for dispatch table
 */
typedef struct {
    uint32_t total_entries;            /**< Total registered functions */
    uint32_t quarantined_count;        /**< Number of quarantined functions */
    uint64_t total_calls;              /**< Total calls across all functions */
    uint64_t total_crashes;            /**< Total crashes across all functions */
    uint32_t total_swaps;              /**< Total swap operations performed */
    uint32_t total_rollbacks;          /**< Total rollback operations performed */
} fn_dispatch_stats_t;

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create a new function dispatch table
 *
 * WHAT: Allocates and initializes a dispatch table
 * WHY:  Entry point for using function dispatch
 * HOW:  Allocate memory, initialize locks, set up empty table
 *
 * @return Newly created dispatch table or NULL on failure
 *
 * THREAD SAFETY: Thread-safe
 * MEMORY: Caller owns returned table
 *
 * EXAMPLE:
 * ```c
 * fn_dispatch_table_t* table = fn_dispatch_create();
 * if (!table) {
 *     LOG_ERROR("Failed to create dispatch table");
 *     return;
 * }
 * ```
 */
NIMCP_EXPORT fn_dispatch_table_t* fn_dispatch_create(void);

/**
 * @brief Destroy a function dispatch table and free all resources
 *
 * WHAT: Frees all memory and destroys locks
 * WHY:  Clean shutdown of dispatch system
 * HOW:  Destroy entry locks, free history arrays, free table
 *
 * @param table Dispatch table to destroy (NULL-safe)
 *
 * THREAD SAFETY: NOT thread-safe - ensure no concurrent access
 * MEMORY: Frees all table resources
 */
NIMCP_EXPORT void fn_dispatch_destroy(fn_dispatch_table_t* table);

//=============================================================================
// Registration Functions
//=============================================================================

/**
 * @brief Auto-register all NIMCP_EXPORT functions from library
 *
 * WHAT: Parse ELF symbols and register all exported functions
 * WHY:  Automatic discovery of hot-swappable functions
 * HOW:  Use dlopen(NULL) for self-handle, iterate symbol table
 *
 * @param table Dispatch table to populate
 * @return FN_DISPATCH_OK on success, error code on failure
 *
 * THREAD SAFETY: Thread-safe (acquires table lock)
 * MEMORY: Allocates entries for each discovered function
 *
 * NOTE: Only works on Linux with ELF binaries. On other platforms,
 *       use fn_dispatch_register() for manual registration.
 *
 * EXAMPLE:
 * ```c
 * fn_dispatch_table_t* table = fn_dispatch_create();
 * int result = fn_dispatch_auto_register(table);
 * if (result != FN_DISPATCH_OK) {
 *     LOG_WARN("Auto-registration failed, using manual registration");
 * }
 * ```
 */
NIMCP_EXPORT int fn_dispatch_auto_register(fn_dispatch_table_t* table);

/**
 * @brief Manually register a function in the dispatch table
 *
 * WHAT: Add a function to the dispatch table by name
 * WHY:  Manual registration for non-exported or platform-specific functions
 * HOW:  Create entry, store pointer, initialize locks
 *
 * @param table Dispatch table
 * @param name Function name (must be unique)
 * @param fn_ptr Pointer to function
 * @return FN_DISPATCH_OK on success, error code on failure
 *
 * THREAD SAFETY: Thread-safe (acquires table lock)
 * MEMORY: Allocates new entry
 *
 * EXAMPLE:
 * ```c
 * fn_dispatch_register(table, "my_custom_function", my_custom_function);
 * ```
 */
NIMCP_EXPORT int fn_dispatch_register(fn_dispatch_table_t* table,
                                       const char* name,
                                       void* fn_ptr);

//=============================================================================
// Lookup Functions
//=============================================================================

/**
 * @brief Get current function pointer by name
 *
 * WHAT: Look up function in dispatch table
 * WHY:  Get current implementation (may be patched)
 * HOW:  Hash lookup with read lock
 *
 * @param table Dispatch table
 * @param name Function name
 * @return Function pointer or NULL if not found/quarantined
 *
 * THREAD SAFETY: Thread-safe (read lock)
 * MEMORY: No allocation
 *
 * NOTE: Returns NULL if function is quarantined. Use fn_dispatch_get_entry()
 *       to check quarantine status.
 *
 * EXAMPLE:
 * ```c
 * typedef brain_t (*brain_create_fn)(const char*, ...);
 * brain_create_fn create = (brain_create_fn)fn_dispatch_get(table, "nimcp_brain_create");
 * if (create) {
 *     brain_t brain = create("my_brain", ...);
 * }
 * ```
 */
NIMCP_EXPORT void* fn_dispatch_get(fn_dispatch_table_t* table, const char* name);

/**
 * @brief Get full dispatch entry for a function
 *
 * WHAT: Get complete entry with statistics and status
 * WHY:  Access quarantine status, crash count, version info
 * HOW:  Linear search with read lock
 *
 * @param table Dispatch table
 * @param name Function name
 * @return Pointer to entry or NULL if not found
 *
 * THREAD SAFETY: Thread-safe (read lock)
 * MEMORY: Returns pointer to internal entry (do not free)
 *
 * NOTE: Returned pointer is valid while holding table lock.
 *       Copy data if needed beyond lock scope.
 *
 * EXAMPLE:
 * ```c
 * const fn_dispatch_entry_t* entry = fn_dispatch_get_entry(table, "nimcp_brain_create");
 * if (entry) {
 *     printf("Version: %u, Calls: %lu, Crashes: %lu, Quarantined: %s\n",
 *            entry->version, entry->call_count, entry->crash_count,
 *            entry->quarantined ? "yes" : "no");
 * }
 * ```
 */
NIMCP_EXPORT const fn_dispatch_entry_t* fn_dispatch_get_entry(fn_dispatch_table_t* table,
                                                                const char* name);

//=============================================================================
// Hot-Swap Functions
//=============================================================================

/**
 * @brief Atomically swap function pointer
 *
 * WHAT: Replace function implementation with new version
 * WHY:  Hot-patch functions at runtime
 * HOW:  Acquire write lock, swap pointer, update history
 *
 * @param table Dispatch table
 * @param name Function name
 * @param new_ptr New function pointer
 * @param old_ptr_out Output: previous pointer (can be NULL)
 * @return FN_DISPATCH_OK on success, error code on failure
 *
 * THREAD SAFETY: Thread-safe (write lock)
 * MEMORY: May reallocate history array
 *
 * NOTE: Old pointer is stored in history for rollback.
 *       Version number is incremented.
 *
 * EXAMPLE:
 * ```c
 * void* old_fn;
 * int result = fn_dispatch_swap(table, "nimcp_brain_create", new_brain_create_v2, &old_fn);
 * if (result == FN_DISPATCH_OK) {
 *     LOG_INFO("Swapped nimcp_brain_create to v2");
 * }
 * ```
 */
NIMCP_EXPORT int fn_dispatch_swap(fn_dispatch_table_t* table,
                                   const char* name,
                                   void* new_ptr,
                                   void** old_ptr_out);

/**
 * @brief Rollback function to previous version
 *
 * WHAT: Restore function to N versions back
 * WHY:  Undo bad patches quickly
 * HOW:  Pop from history, update current pointer
 *
 * @param table Dispatch table
 * @param name Function name
 * @param versions Number of versions to rollback (1 = previous)
 * @return FN_DISPATCH_OK on success, error code on failure
 *
 * THREAD SAFETY: Thread-safe (write lock)
 * MEMORY: No allocation (reuses history)
 *
 * EXAMPLE:
 * ```c
 * // Rollback to previous version
 * fn_dispatch_rollback(table, "nimcp_brain_create", 1);
 *
 * // Rollback to original
 * fn_dispatch_rollback(table, "nimcp_brain_create", 0);  // 0 = original
 * ```
 */
NIMCP_EXPORT int fn_dispatch_rollback(fn_dispatch_table_t* table,
                                       const char* name,
                                       uint32_t versions);

//=============================================================================
// Quarantine Functions
//=============================================================================

/**
 * @brief Quarantine a function (disable it)
 *
 * WHAT: Mark function as quarantined so calls return NULL
 * WHY:  Isolate crashing or misbehaving functions
 * HOW:  Set quarantined flag on entry
 *
 * @param table Dispatch table
 * @param name Function name
 * @return FN_DISPATCH_OK on success, error code on failure
 *
 * THREAD SAFETY: Thread-safe (write lock)
 *
 * EXAMPLE:
 * ```c
 * fn_dispatch_quarantine(table, "nimcp_buggy_function");
 * // Now fn_dispatch_get("nimcp_buggy_function") returns NULL
 * ```
 */
NIMCP_EXPORT int fn_dispatch_quarantine(fn_dispatch_table_t* table, const char* name);

/**
 * @brief Remove quarantine from a function
 *
 * WHAT: Re-enable a quarantined function
 * WHY:  Restore function after fix or reset
 * HOW:  Clear quarantined flag, optionally reset crash count
 *
 * @param table Dispatch table
 * @param name Function name
 * @return FN_DISPATCH_OK on success, error code on failure
 *
 * THREAD SAFETY: Thread-safe (write lock)
 *
 * EXAMPLE:
 * ```c
 * fn_dispatch_unquarantine(table, "nimcp_fixed_function");
 * ```
 */
NIMCP_EXPORT int fn_dispatch_unquarantine(fn_dispatch_table_t* table, const char* name);

//=============================================================================
// Statistics Functions
//=============================================================================

/**
 * @brief Record a crash for a function
 *
 * WHAT: Increment crash counter for function
 * WHY:  Track function stability
 * HOW:  Atomic increment of crash_count
 *
 * @param table Dispatch table
 * @param name Function name
 * @return FN_DISPATCH_OK on success, error code on failure
 *
 * THREAD SAFETY: Thread-safe (uses atomic or write lock)
 *
 * NOTE: If crash_count exceeds FN_DISPATCH_AUTO_QUARANTINE_THRESHOLD,
 *       function is automatically quarantined.
 *
 * EXAMPLE:
 * ```c
 * // In signal handler or catch block
 * fn_dispatch_record_crash(table, current_function_name);
 * ```
 */
NIMCP_EXPORT int fn_dispatch_record_crash(fn_dispatch_table_t* table, const char* name);

/**
 * @brief Record a successful call to a function
 *
 * WHAT: Increment call counter for function
 * WHY:  Track function usage
 * HOW:  Atomic increment of call_count
 *
 * @param table Dispatch table
 * @param name Function name
 * @return FN_DISPATCH_OK on success, error code on failure
 *
 * THREAD SAFETY: Thread-safe (uses atomic)
 */
NIMCP_EXPORT int fn_dispatch_record_call(fn_dispatch_table_t* table, const char* name);

/**
 * @brief Clear patch history for a function
 *
 * WHAT: Remove old versions from history
 * WHY:  Free memory, prevent unlimited history growth
 * HOW:  Keep last N versions, free rest
 *
 * @param table Dispatch table
 * @param name Function name
 * @param keep_versions Number of versions to keep (0 = keep none)
 * @return FN_DISPATCH_OK on success, error code on failure
 *
 * THREAD SAFETY: Thread-safe (write lock)
 */
NIMCP_EXPORT int fn_dispatch_clear_history(fn_dispatch_table_t* table,
                                            const char* name,
                                            uint32_t keep_versions);

/**
 * @brief Get dispatch table statistics
 *
 * WHAT: Aggregate statistics across all entries
 * WHY:  Monitor dispatch system health
 * HOW:  Iterate entries, sum counters
 *
 * @param table Dispatch table
 * @param stats Output statistics structure
 * @return FN_DISPATCH_OK on success, error code on failure
 *
 * THREAD SAFETY: Thread-safe (read lock)
 */
NIMCP_EXPORT int fn_dispatch_get_stats(fn_dispatch_table_t* table,
                                        fn_dispatch_stats_t* stats);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Check if dispatch table is valid
 *
 * WHAT: Validate table magic number and state
 * WHY:  Detect corruption or invalid pointers
 * HOW:  Check magic and basic invariants
 *
 * @param table Dispatch table
 * @return true if valid, false otherwise
 *
 * THREAD SAFETY: Thread-safe
 */
NIMCP_EXPORT bool fn_dispatch_is_valid(const fn_dispatch_table_t* table);

/**
 * @brief Get error message for error code
 *
 * WHAT: Convert error code to human-readable string
 * WHY:  Better error reporting
 * HOW:  Static lookup table
 *
 * @param error Error code
 * @return Error message string (static, do not free)
 */
NIMCP_EXPORT const char* fn_dispatch_strerror(fn_dispatch_error_t error);

/**
 * @brief Iterate over all registered functions
 *
 * WHAT: Call callback for each registered function
 * WHY:  Enumeration, debugging, bulk operations
 * HOW:  Iterate entries with read lock, call user function
 *
 * @param table Dispatch table
 * @param callback Function called for each entry
 * @param user_data User context passed to callback
 * @return Number of entries iterated
 *
 * THREAD SAFETY: Thread-safe (read lock held during iteration)
 *
 * NOTE: Callback should not modify table (read-only access).
 *
 * EXAMPLE:
 * ```c
 * void print_entry(const fn_dispatch_entry_t* entry, void* ctx) {
 *     printf("%s: v%u, %lu calls\n", entry->name, entry->version, entry->call_count);
 * }
 * fn_dispatch_foreach(table, print_entry, NULL);
 * ```
 */
typedef void (*fn_dispatch_callback_t)(const fn_dispatch_entry_t* entry, void* user_data);

NIMCP_EXPORT uint32_t fn_dispatch_foreach(fn_dispatch_table_t* table,
                                           fn_dispatch_callback_t callback,
                                           void* user_data);

//=============================================================================
// Global Dispatch Table
//=============================================================================

/**
 * @brief Get or create the global dispatch table
 *
 * WHAT: Access singleton global dispatch table
 * WHY:  Convenience for single-table usage
 * HOW:  Thread-safe lazy initialization
 *
 * @return Global dispatch table or NULL on failure
 *
 * THREAD SAFETY: Thread-safe (once initialization)
 *
 * EXAMPLE:
 * ```c
 * fn_dispatch_table_t* table = fn_dispatch_global();
 * fn_dispatch_auto_register(table);
 * void* fn = fn_dispatch_get(table, "nimcp_brain_create");
 * ```
 */
NIMCP_EXPORT fn_dispatch_table_t* fn_dispatch_global(void);

/**
 * @brief Destroy the global dispatch table
 *
 * WHAT: Clean up global singleton
 * WHY:  Shutdown sequence
 * HOW:  Destroy table, reset singleton
 *
 * THREAD SAFETY: NOT thread-safe - call during shutdown only
 */
NIMCP_EXPORT void fn_dispatch_global_destroy(void);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_FN_DISPATCH_H */
