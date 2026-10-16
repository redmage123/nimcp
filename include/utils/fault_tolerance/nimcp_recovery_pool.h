/**
 * @file nimcp_recovery_pool.h
 * @brief Pre-Allocated Memory Pool for OOM Recovery
 *
 * WHAT: Emergency memory pool for fault-tolerant recovery operations
 * WHY:  Ensure recovery can succeed even when out of memory
 * HOW:  Pre-allocate 1MB pool at startup, use during recovery only
 *
 * DESIGN PRINCIPLES:
 * - Pre-allocation: Pool allocated at startup when memory available
 * - Emergency-only: Pool used ONLY during OOM recovery
 * - Simple allocator: Bump allocator for minimal overhead
 * - Thread-safe: Mutex-protected for concurrent recovery attempts
 * - Tracked allocations: Track all allocations for proper reset
 * - Statistics: Monitor pool usage and emergency activations
 *
 * ARCHITECTURE:
 * ```
 * +------------------+
 * | Recovery Pool    |
 * +------------------+
 * | Pre-allocated    | <- 1MB buffer allocated at startup
 * | Emergency Buffer |    (never freed until shutdown)
 * +------------------+
 * | Current Offset   | <- Bump allocator position
 * | Allocation List  | <- Track all allocations
 * | Usage Stats      | <- Pool utilization metrics
 * | Thread Safety    | <- Mutex for concurrent access
 * +------------------+
 * ```
 *
 * USAGE PATTERN:
 * ```c
 * // 1. Startup: Create pool (when memory available)
 * recovery_pool_t* pool = recovery_pool_create(1024 * 1024);  // 1MB
 *
 * // 2. Normal operation: Pool sits idle
 * // ... application runs ...
 *
 * // 3. OOM Detected: Switch to emergency mode
 * if (nimcp_malloc(size) == NULL) {
 *     recovery_pool_enter_emergency_mode(pool);
 *
 *     // 4. Recovery: Use pool allocations
 *     void* data = recovery_pool_alloc(pool, size);
 *     // ... perform recovery ...
 *     recovery_pool_free(pool, data);
 *
 *     // 5. Recovery Complete: Reset pool
 *     recovery_pool_reset(pool);
 *     recovery_pool_exit_emergency_mode(pool);
 * }
 *
 * // 6. Shutdown: Destroy pool
 * recovery_pool_destroy(pool);
 * ```
 *
 * INTEGRATION POINTS:
 * - checkpoint_load(): Use pool during OOM recovery
 * - diagnostics_analyze_*(): Use pool for diagnostic allocations
 * - recovery_*(): Use pool for all recovery operations
 *
 * THREAD SAFETY:
 * All functions are thread-safe via internal mutex
 *
 * PERFORMANCE:
 * - Allocation: O(1) bump allocator
 * - Free: O(1) mark-and-sweep during reset
 * - Reset: O(n) where n = allocation count
 *
 * @author NIMCP Team
 * @date 2025-11-20
 */

#ifndef NIMCP_RECOVERY_POOL_H
#define NIMCP_RECOVERY_POOL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "utils/memory/nimcp_memory.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Type Definitions
//=============================================================================

/**
 * @brief Recovery pool handle (opaque)
 *
 * WHAT: Handle to pre-allocated emergency memory pool
 * WHY:  Encapsulate internal state
 * HOW:  Opaque pointer to internal structure
 */
typedef struct recovery_pool recovery_pool_t;

//=============================================================================
// Pool Statistics
//=============================================================================

/**
 * @brief Recovery pool usage statistics
 *
 * WHAT: Metrics for pool utilization and emergency mode activations
 * WHY:  Monitor pool effectiveness and size requirements
 * HOW:  Tracked during allocation/reset/emergency mode
 */
typedef struct {
    size_t pool_size_bytes;          /**< Total pool size (bytes) */
    size_t current_used_bytes;       /**< Currently allocated (bytes) */
    size_t peak_used_bytes;          /**< Peak usage (bytes) */
    size_t total_allocated_bytes;    /**< Lifetime allocated (bytes) */

    uint32_t allocation_count;       /**< Current allocation count */
    uint32_t peak_allocation_count;  /**< Peak allocation count */
    uint32_t total_allocations;      /**< Lifetime allocation count */
    uint32_t failed_allocations;     /**< Failed allocation attempts */

    uint32_t emergency_activations;  /**< Emergency mode activations */
    uint32_t reset_count;            /**< Number of resets */

    bool is_emergency_mode;          /**< Currently in emergency mode */
    bool pool_exhausted;             /**< Pool ran out of space */

    // Fragmentation analysis
    size_t largest_free_block;       /**< Largest contiguous free space */
    float fragmentation_percent;     /**< Internal fragmentation (0-100) */
} recovery_pool_stats_t;

//=============================================================================
// Pool Creation and Destruction
//=============================================================================

/**
 * @brief Create pre-allocated recovery pool
 *
 * WHAT: Allocate emergency memory pool for OOM recovery
 * WHY:  Ensure recovery has guaranteed memory available
 * HOW:  Pre-allocate buffer at startup, initialize metadata
 *
 * ALLOCATION:
 * - Buffer: size_bytes (pre-allocated, never freed until destroy)
 * - Metadata: ~128 bytes for tracking structures
 * - Total: ~(size_bytes + 128) bytes
 *
 * THREAD-SAFE: Yes (new pool is independent)
 *
 * ERROR HANDLING:
 * - Returns NULL if allocation fails
 * - Returns NULL if size_bytes == 0
 * - Returns NULL if size_bytes > 100MB (sanity check)
 *
 * RECOMMENDED SIZE:
 * - Minimum: 256KB (for basic recovery)
 * - Standard: 1MB (for checkpoint + diagnostics)
 * - Large: 4MB (for complex recovery scenarios)
 *
 * @param size_bytes Pool size in bytes (typically 1MB = 1024*1024)
 * @return Pool handle or NULL on failure
 */
recovery_pool_t* recovery_pool_create(size_t size_bytes);

/**
 * @brief Destroy recovery pool
 *
 * WHAT: Free pool and all associated resources
 * WHY:  Clean shutdown and resource cleanup
 * HOW:  Free buffer, free metadata, zero handle
 *
 * WARNING:
 * - All pool allocations become invalid after destroy
 * - Caller must ensure no references remain
 * - Safe to call with NULL (no-op)
 *
 * THREAD-SAFE: No (caller must ensure no concurrent access)
 *
 * @param pool Pool to destroy (NULL is safe)
 */
void recovery_pool_destroy(recovery_pool_t* pool);

//=============================================================================
// Emergency Mode Control
//=============================================================================

/**
 * @brief Enter emergency recovery mode
 *
 * WHAT: Activate pool for emergency allocations
 * WHY:  Signal that system is in OOM recovery state
 * HOW:  Set emergency flag, increment activation counter
 *
 * USAGE:
 * - Call when nimcp_malloc() fails and recovery is needed
 * - All subsequent pool_alloc() calls will succeed (if space available)
 * - Exit emergency mode after recovery complete
 *
 * THREAD-SAFE: Yes (mutex-protected)
 *
 * @param pool Pool to activate
 * @return true on success, false if pool is NULL
 */
bool recovery_pool_enter_emergency_mode(recovery_pool_t* pool);

/**
 * @brief Exit emergency recovery mode
 *
 * WHAT: Deactivate emergency mode
 * WHY:  Signal that recovery is complete
 * HOW:  Clear emergency flag
 *
 * NOTE: Does NOT reset pool - call recovery_pool_reset() explicitly
 *
 * THREAD-SAFE: Yes (mutex-protected)
 *
 * @param pool Pool to deactivate
 * @return true on success, false if pool is NULL
 */
bool recovery_pool_exit_emergency_mode(recovery_pool_t* pool);

/**
 * @brief Check if pool is in emergency mode
 *
 * WHAT: Query emergency mode status
 * WHY:  Conditional logic based on pool state
 * HOW:  Thread-safe read of emergency flag
 *
 * THREAD-SAFE: Yes (mutex-protected)
 *
 * @param pool Pool to query
 * @return true if in emergency mode, false otherwise
 */
bool recovery_pool_is_emergency_mode(const recovery_pool_t* pool);

//=============================================================================
// Memory Allocation
//=============================================================================

/**
 * @brief Allocate memory from recovery pool
 *
 * WHAT: Emergency allocation using bump allocator
 * WHY:  Guaranteed allocation during OOM recovery
 * HOW:  Bump offset, track allocation, return pointer
 *
 * ALLOCATOR:
 * - Type: Bump allocator (fast, simple, no reuse)
 * - Complexity: O(1) allocation
 * - Alignment: 8-byte aligned for all platforms
 * - Tracking: All allocations tracked for reset
 *
 * USAGE RESTRICTIONS:
 * - Allocations are NOT reusable until reset
 * - Free is a no-op (space reclaimed on reset only)
 * - No realloc support (by design)
 *
 * THREAD-SAFE: Yes (mutex-protected)
 *
 * ERROR HANDLING:
 * - Returns NULL if pool is NULL
 * - Returns NULL if size == 0
 * - Returns NULL if pool exhausted
 * - Increments failed_allocations on failure
 *
 * ALIGNMENT:
 * - All allocations 8-byte aligned
 * - Compatible with any data type
 *
 * @param pool Pool to allocate from
 * @param size Bytes to allocate
 * @return Pointer to allocated memory or NULL on failure
 */
void* recovery_pool_alloc(recovery_pool_t* pool, size_t size);

/**
 * @brief Allocate zero-initialized memory from pool
 *
 * WHAT: Emergency calloc equivalent
 * WHY:  Many recovery operations need zero-initialized memory
 * HOW:  Allocate from pool, zero memory, return pointer
 *
 * THREAD-SAFE: Yes (mutex-protected)
 *
 * @param pool Pool to allocate from
 * @param count Number of elements
 * @param size Size of each element
 * @return Pointer to zero-initialized memory or NULL on failure
 */
void* recovery_pool_calloc(recovery_pool_t* pool, size_t count, size_t size);

/**
 * @brief Free memory from recovery pool (no-op)
 *
 * WHAT: Mark memory as freed (no-op in bump allocator)
 * WHY:  API consistency with malloc/free
 * HOW:  Track freed pointer (space reclaimed on reset only)
 *
 * IMPORTANT:
 * - This is a NO-OP for memory reclamation
 * - Space is reclaimed only on recovery_pool_reset()
 * - Provided for API consistency only
 *
 * THREAD-SAFE: Yes (mutex-protected)
 *
 * @param pool Pool that owns the memory
 * @param ptr Pointer to free (NULL is safe)
 */
void recovery_pool_free(recovery_pool_t* pool, void* ptr);

//=============================================================================
// Pool Management
//=============================================================================

/**
 * @brief Reset pool for reuse
 *
 * WHAT: Reclaim all allocated space for reuse
 * WHY:  Prepare pool for next emergency
 * HOW:  Reset bump offset, clear allocation tracking
 *
 * USAGE:
 * - Call after recovery complete
 * - All previous allocations become invalid
 * - Pool ready for next emergency
 *
 * THREAD-SAFE: Yes (mutex-protected)
 *
 * WARNING:
 * - All pointers from pool_alloc() become invalid
 * - Caller must ensure no references remain
 *
 * @param pool Pool to reset
 * @return true on success, false if pool is NULL
 */
bool recovery_pool_reset(recovery_pool_t* pool);

/**
 * @brief Get pool statistics
 *
 * WHAT: Query pool usage and emergency mode activations
 * WHY:  Monitor pool effectiveness, tune pool size
 * HOW:  Thread-safe copy of statistics structure
 *
 * THREAD-SAFE: Yes (mutex-protected)
 *
 * USES:
 * - Monitoring: Track pool utilization over time
 * - Tuning: Determine if pool size is adequate
 * - Debugging: Identify pool exhaustion scenarios
 *
 * @param pool Pool to query
 * @param stats Output parameter for statistics
 * @return true on success, false if pool or stats is NULL
 */
bool recovery_pool_get_stats(const recovery_pool_t* pool, recovery_pool_stats_t* stats);

/**
 * @brief Check if pool has sufficient space
 *
 * WHAT: Query if pool can satisfy allocation request
 * WHY:  Proactive checking before allocation
 * HOW:  Compare required_size to available space
 *
 * THREAD-SAFE: Yes (mutex-protected)
 *
 * NOTE: Result may be stale immediately after return
 *       (concurrent thread may allocate)
 *
 * @param pool Pool to check
 * @param required_size Bytes needed
 * @return true if pool has space, false otherwise
 */
bool recovery_pool_has_space(const recovery_pool_t* pool, size_t required_size);

/**
 * @brief Get available pool space
 *
 * WHAT: Query how much space remains
 * WHY:  Determine allocation headroom
 * HOW:  Calculate pool_size - current_offset
 *
 * THREAD-SAFE: Yes (mutex-protected)
 *
 * @param pool Pool to query
 * @return Available bytes, or 0 if pool is NULL
 */
size_t recovery_pool_get_available(const recovery_pool_t* pool);

//=============================================================================
// Debugging and Diagnostics
//=============================================================================

/**
 * @brief Validate pool integrity
 *
 * WHAT: Check pool internal consistency
 * WHY:  Detect corruption or invalid state
 * HOW:  Verify invariants, check canaries, validate pointers
 *
 * CHECKS:
 * - Pool structure not NULL
 * - Buffer pointer valid
 * - Offset within bounds
 * - Allocation count matches tracking
 * - Statistics consistent
 * - No memory corruption
 *
 * THREAD-SAFE: Yes (mutex-protected)
 *
 * @param pool Pool to validate
 * @return true if pool is valid, false if corrupted
 */
bool recovery_pool_validate(const recovery_pool_t* pool);

/**
 * @brief Dump pool state to console
 *
 * WHAT: Print pool statistics and allocation list
 * WHY:  Debug pool usage and identify issues
 * HOW:  Format and print pool state to stdout
 *
 * OUTPUT:
 * - Pool size and usage
 * - Current allocations
 * - Emergency mode status
 * - Statistics
 *
 * THREAD-SAFE: Yes (mutex-protected)
 *
 * @param pool Pool to dump
 */
void recovery_pool_dump(const recovery_pool_t* pool);

/**
 * @brief Get pool error message
 *
 * WHAT: Retrieve last error message for pool
 * WHY:  Debug allocation failures
 * HOW:  Thread-local error message storage
 *
 * THREAD-SAFE: Yes (thread-local storage)
 *
 * @return Error message string (valid until next call)
 */
const char* recovery_pool_get_error(void);

/**
 * @brief Clear pool error state
 *
 * WHAT: Reset error message
 * WHY:  Clear stale errors
 * HOW:  Zero thread-local buffer
 *
 * THREAD-SAFE: Yes (thread-local storage)
 */
void recovery_pool_clear_error(void);

//=============================================================================
// Integration Helpers
//=============================================================================

/**
 * @brief Set global recovery pool for NIMCP
 *
 * WHAT: Register pool for use by checkpoint/diagnostics/recovery
 * WHY:  Centralized pool for all fault tolerance subsystems
 * HOW:  Store pool pointer in global variable
 *
 * USAGE:
 * ```c
 * recovery_pool_t* pool = recovery_pool_create(1024*1024);
 * recovery_pool_set_global(pool);
 * // ... checkpoint/diagnostics will use this pool during OOM ...
 * ```
 *
 * THREAD-SAFE: No (call at startup only)
 *
 * @param pool Pool to use globally (NULL to clear)
 */
void recovery_pool_set_global(recovery_pool_t* pool);

/**
 * @brief Get global recovery pool
 *
 * WHAT: Retrieve global pool for emergency allocations
 * WHY:  Centralized access for all subsystems
 * HOW:  Return global pool pointer
 *
 * THREAD-SAFE: Yes (read-only access)
 *
 * @return Global pool or NULL if not set
 */
recovery_pool_t* recovery_pool_get_global(void);

#ifdef __cplusplus
}
#endif

#endif  // NIMCP_RECOVERY_POOL_H
