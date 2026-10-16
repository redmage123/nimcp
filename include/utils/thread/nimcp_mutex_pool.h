//=============================================================================
// nimcp_mutex_pool.h - Shared Mutex Pool for Memory Optimization
//=============================================================================
/**
 * @file nimcp_mutex_pool.h
 * @brief Shared mutex pool to reduce per-bridge mutex overhead
 *
 * WHAT: Pool of shared mutexes that bridges can use instead of individual mutexes
 * WHY:  70+ bridges * 80 bytes per mutex = 5.6KB waste; sharing reduces to ~640 bytes
 * HOW:  Hash-based assignment of bridges to pool slots
 *
 * BIOLOGICAL BASIS:
 * Like neural resource sharing in the brain, where multiple synapses share
 * access to limited vesicle pools and ion channels rather than each having
 * dedicated resources.
 *
 * USAGE:
 * @code
 * // Instead of allocating individual mutex:
 * // bridge->mutex = nimcp_malloc(sizeof(nimcp_mutex_t));
 * // nimcp_mutex_init(bridge->mutex, NULL);
 *
 * // Use the pool:
 * bridge->mutex_slot = nimcp_mutex_pool_acquire("my_bridge_name");
 * nimcp_mutex_pool_lock(bridge->mutex_slot);
 * // critical section
 * nimcp_mutex_pool_unlock(bridge->mutex_slot);
 * @endcode
 *
 * THREAD SAFETY: All operations are thread-safe
 *
 * @author NIMCP Development Team
 * @date 2024-12-22
 * @version 1.0.0
 */

#ifndef NIMCP_MUTEX_POOL_H
#define NIMCP_MUTEX_POOL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "utils/thread/nimcp_thread.h"
#include "utils/platform/nimcp_tier_optimization.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/**
 * @brief Invalid mutex slot constant
 */
#define NIMCP_MUTEX_SLOT_INVALID ((uint32_t)-1)

//=============================================================================
// Types
//=============================================================================

/**
 * @brief Mutex pool slot handle
 */
typedef uint32_t nimcp_mutex_slot_t;

/**
 * @brief Mutex pool statistics
 */
typedef struct {
    uint32_t total_acquires;        /**< Total acquire calls */
    uint32_t total_releases;        /**< Total release calls */
    uint32_t total_locks;           /**< Total lock operations */
    uint32_t total_unlocks;         /**< Total unlock operations */
    uint32_t contention_events;     /**< Times a lock had to wait */
    uint32_t slot_usage[NIMCP_MUTEX_POOL_SIZE];  /**< Usage count per slot */
} nimcp_mutex_pool_stats_t;

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Initialize the global mutex pool
 *
 * WHAT: Create and initialize the shared mutex pool
 * WHY:  Must be called before any pool operations
 * HOW:  Allocates pool mutexes and tracking structures
 *
 * THREAD SAFETY: Call once at startup before any concurrent access
 *
 * @return 0 on success, negative error code on failure
 */
int nimcp_mutex_pool_init(void);

/**
 * @brief Destroy the global mutex pool
 *
 * WHAT: Clean up and free pool resources
 * WHY:  Release resources at shutdown
 * HOW:  Destroys all pool mutexes
 *
 * THREAD SAFETY: Call once at shutdown, no concurrent operations
 *
 * @return 0 on success, negative error code on failure
 */
int nimcp_mutex_pool_destroy(void);

/**
 * @brief Check if mutex pool is initialized
 *
 * @return true if pool is ready, false otherwise
 */
bool nimcp_mutex_pool_is_initialized(void);

//=============================================================================
// Slot Management API
//=============================================================================

/**
 * @brief Acquire a mutex slot for a bridge
 *
 * WHAT: Get a slot index for a named bridge to use
 * WHY:  Deterministic assignment based on name hash
 * HOW:  Hash the name and modulo by pool size
 *
 * THREAD SAFETY: Thread-safe
 *
 * @param bridge_name Unique name of the bridge (used for hashing)
 * @return Slot index (0 to NIMCP_MUTEX_POOL_SIZE-1), or NIMCP_MUTEX_SLOT_INVALID on error
 *
 * @code
 * nimcp_mutex_slot_t slot = nimcp_mutex_pool_acquire("stdp_immune_bridge");
 * @endcode
 */
nimcp_mutex_slot_t nimcp_mutex_pool_acquire(const char* bridge_name);

/**
 * @brief Acquire a mutex slot by numeric ID
 *
 * WHAT: Get a slot index for a numeric bridge ID
 * WHY:  Faster than string hashing when ID is available
 * HOW:  Simple modulo by pool size
 *
 * @param bridge_id Unique numeric ID of the bridge
 * @return Slot index (0 to NIMCP_MUTEX_POOL_SIZE-1)
 */
nimcp_mutex_slot_t nimcp_mutex_pool_acquire_by_id(uint32_t bridge_id);

/**
 * @brief Release a mutex slot
 *
 * WHAT: Indicate a bridge is done with its slot
 * WHY:  Tracking for statistics (slot is not actually freed)
 * HOW:  Decrements reference count for the slot
 *
 * @param slot Slot to release
 */
void nimcp_mutex_pool_release(nimcp_mutex_slot_t slot);

//=============================================================================
// Lock/Unlock API
//=============================================================================

/**
 * @brief Lock a pool mutex slot (blocking)
 *
 * WHAT: Acquire exclusive access to a slot's mutex
 * WHY:  Thread synchronization
 * HOW:  Wraps nimcp_mutex_lock for the slot's mutex
 *
 * THREAD SAFETY: Thread-safe, will block until lock acquired
 *
 * @param slot Slot to lock
 * @return 0 on success, negative error code on failure
 */
int nimcp_mutex_pool_lock(nimcp_mutex_slot_t slot);

/**
 * @brief Try to lock a pool mutex slot (non-blocking)
 *
 * WHAT: Attempt to acquire lock without blocking
 * WHY:  Avoid deadlock in some algorithms
 * HOW:  Wraps nimcp_mutex_trylock
 *
 * @param slot Slot to try locking
 * @return 0 on success, NIMCP_BUSY if already locked, negative on error
 */
int nimcp_mutex_pool_trylock(nimcp_mutex_slot_t slot);

/**
 * @brief Unlock a pool mutex slot
 *
 * WHAT: Release exclusive access to a slot's mutex
 * WHY:  Allow other threads to proceed
 * HOW:  Wraps nimcp_mutex_unlock
 *
 * @param slot Slot to unlock
 * @return 0 on success, negative error code on failure
 */
int nimcp_mutex_pool_unlock(nimcp_mutex_slot_t slot);

//=============================================================================
// Statistics API
//=============================================================================

/**
 * @brief Get mutex pool statistics
 *
 * @param stats Output statistics structure
 * @return 0 on success, negative error code on failure
 */
int nimcp_mutex_pool_get_stats(nimcp_mutex_pool_stats_t* stats);

/**
 * @brief Reset mutex pool statistics
 */
void nimcp_mutex_pool_reset_stats(void);

//=============================================================================
// Compatibility Macros
//=============================================================================

/**
 * @brief Macros for gradual migration from per-bridge to pooled mutexes
 *
 * When NIMCP_USE_MUTEX_POOL is enabled (MINIMAL tier), these macros use the pool.
 * Otherwise, they use traditional per-bridge mutex allocation.
 */

#if NIMCP_USE_MUTEX_POOL

    /**
     * @brief Declare mutex slot field in struct
     */
    #define NIMCP_BRIDGE_MUTEX_FIELD nimcp_mutex_slot_t mutex_slot

    /**
     * @brief Initialize bridge mutex (use pool)
     */
    #define NIMCP_BRIDGE_MUTEX_INIT(bridge, name) \
        do { (bridge)->mutex_slot = nimcp_mutex_pool_acquire(name); } while(0)

    /**
     * @brief Destroy bridge mutex (release slot)
     */
    #define NIMCP_BRIDGE_MUTEX_DESTROY(bridge) \
        do { nimcp_mutex_pool_release((bridge)->mutex_slot); } while(0)

    /**
     * @brief Lock bridge mutex
     */
    #define NIMCP_BRIDGE_MUTEX_LOCK(bridge) \
        nimcp_mutex_pool_lock((bridge)->mutex_slot)

    /**
     * @brief Unlock bridge mutex
     */
    #define NIMCP_BRIDGE_MUTEX_UNLOCK(bridge) \
        nimcp_mutex_pool_unlock((bridge)->mutex_slot)

    /**
     * @brief Check if bridge has valid mutex
     */
    #define NIMCP_BRIDGE_MUTEX_VALID(bridge) \
        ((bridge)->mutex_slot != NIMCP_MUTEX_SLOT_INVALID)

#else /* !NIMCP_USE_MUTEX_POOL */

    /**
     * @brief Declare mutex pointer field in struct
     */
    #define NIMCP_BRIDGE_MUTEX_FIELD nimcp_mutex_t* mutex

    /**
     * @brief Initialize bridge mutex (allocate)
     */
    #define NIMCP_BRIDGE_MUTEX_INIT(bridge, name) \
        do { \
            (bridge)->mutex = (nimcp_mutex_t*)nimcp_malloc(sizeof(nimcp_mutex_t)); \
            if ((bridge)->mutex) nimcp_mutex_init((bridge)->mutex, NULL); \
        } while(0)

    /**
     * @brief Destroy bridge mutex (free)
     */
    #define NIMCP_BRIDGE_MUTEX_DESTROY(bridge) \
        do { \
            if ((bridge)->mutex) { \
                nimcp_mutex_destroy((bridge)->mutex); \
                nimcp_free((bridge)->mutex); \
                (bridge)->mutex = NULL; \
            } \
        } while(0)

    /**
     * @brief Lock bridge mutex
     */
    #define NIMCP_BRIDGE_MUTEX_LOCK(bridge) \
        (((bridge)->mutex) ? nimcp_mutex_lock((bridge)->mutex) : 0)

    /**
     * @brief Unlock bridge mutex
     */
    #define NIMCP_BRIDGE_MUTEX_UNLOCK(bridge) \
        (((bridge)->mutex) ? nimcp_mutex_unlock((bridge)->mutex) : 0)

    /**
     * @brief Check if bridge has valid mutex
     */
    #define NIMCP_BRIDGE_MUTEX_VALID(bridge) \
        ((bridge)->mutex != NULL)

#endif /* NIMCP_USE_MUTEX_POOL */

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MUTEX_POOL_H */
