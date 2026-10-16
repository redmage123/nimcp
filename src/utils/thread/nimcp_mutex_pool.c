#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_mutex_pool.c - Shared Mutex Pool Implementation
//=============================================================================
/**
 * @file nimcp_mutex_pool.c
 * @brief Implementation of shared mutex pool for memory optimization
 *
 * WHAT: Pool of shared mutexes for bridge synchronization
 * WHY:  Reduce memory footprint from 5.6KB to ~640 bytes (70+ bridges)
 * HOW:  Hash-based assignment of bridges to fixed pool of mutexes
 *
 * @author NIMCP Development Team
 * @date 2024-12-22
 */

#include "utils/thread/nimcp_mutex_pool.h"
#include "utils/memory/nimcp_memory_guards.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/platform/nimcp_platform_once.h"
#include "utils/thread/nimcp_atomic.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(mutex_pool)

//=============================================================================
// Internal State
//=============================================================================

/**
 * @brief Internal mutex pool structure
 */
typedef struct {
    nimcp_mutex_t mutexes[NIMCP_MUTEX_POOL_SIZE];   /**< The actual mutexes */
    uint32_t ref_counts[NIMCP_MUTEX_POOL_SIZE];     /**< Reference count per slot */
    nimcp_atomic_uint32_t total_acquires;            /**< Total acquire calls (atomic) */
    nimcp_atomic_uint32_t total_releases;            /**< Total release calls (atomic) */
    nimcp_atomic_uint32_t total_locks;               /**< Total lock operations (atomic) */
    nimcp_atomic_uint32_t total_unlocks;             /**< Total unlock operations (atomic) */
    nimcp_atomic_uint32_t contention_events;         /**< Times a lock had to wait (atomic) */
    nimcp_atomic_uint32_t slot_usage[NIMCP_MUTEX_POOL_SIZE];  /**< Usage count per slot (atomic) */
    bool initialized;                                /**< Init flag */
} mutex_pool_t;

/**
 * @brief Global mutex pool instance
 */
static mutex_pool_t g_pool = {0};

/**
 * WHAT: Platform-once control for thread-safe auto-initialization
 * WHY:  Prevent race condition when multiple threads call acquire() simultaneously
 * HOW:  nimcp_platform_once guarantees exactly-once execution
 */
static nimcp_platform_once_t g_pool_init_once = NIMCP_PLATFORM_ONCE_INIT;

/**
 * WHAT: Initialization result for thread-safe error propagation
 * WHY:  nimcp_platform_once doesn't return error, so store result for later check
 */
static int g_pool_init_result = 0;

//=============================================================================
// Hash Function
//=============================================================================

/**
 * @brief FNV-1a hash for strings
 *
 * WHAT: Fast non-cryptographic hash
 * WHY:  Deterministic slot assignment
 * HOW:  FNV-1a algorithm
 */
static uint32_t fnv1a_hash(const char* str) {
    if (!str) return 0;

    uint32_t hash = 2166136261u;  // FNV offset basis
    while (*str) {
        hash ^= (uint8_t)*str++;
        hash *= 16777619u;  // FNV prime
    }
    return hash;
}

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Internal initialization routine called by nimcp_platform_once
 *
 * WHAT: Thread-safe one-time initialization of mutex pool
 * WHY:  Ensures pool is initialized exactly once even with concurrent callers
 */
static void mutex_pool_init_internal(void) {
    /* Initialize all pool mutexes */
    for (int i = 0; i < NIMCP_MUTEX_POOL_SIZE; i++) {
        int rc = nimcp_platform_mutex_init(&g_pool.mutexes[i], false);
        if (rc != 0) {
            /* Rollback on failure */
            for (int j = 0; j < i; j++) {
                nimcp_platform_mutex_destroy(&g_pool.mutexes[j]);
            }
            /* P0 fix: Explicitly set initialized to false on failure to ensure
             * consistent state. Although initialized starts as false, being explicit
             * here makes the failure state clear and guards against future changes. */
            g_pool.initialized = false;
            g_pool_init_result = -1;
            return;
        }
        g_pool.ref_counts[i] = 0;
        nimcp_atomic_init_u32(&g_pool.slot_usage[i], 0);
    }

    /* Initialize atomic statistics */
    nimcp_atomic_init_u32(&g_pool.total_acquires, 0);
    nimcp_atomic_init_u32(&g_pool.total_releases, 0);
    nimcp_atomic_init_u32(&g_pool.total_locks, 0);
    nimcp_atomic_init_u32(&g_pool.total_unlocks, 0);
    nimcp_atomic_init_u32(&g_pool.contention_events, 0);

    g_pool.initialized = true;
    g_pool_init_result = 0;
}

int nimcp_mutex_pool_init(void) {
    if (g_pool.initialized) {
        return 0;  /* Already initialized */
    }

    /* Thread-safe one-time initialization using platform_once */
    nimcp_platform_once(&g_pool_init_once, mutex_pool_init_internal);

    return g_pool_init_result;
}

int nimcp_mutex_pool_destroy(void) {
    if (!g_pool.initialized) {
        return 0;
    }

    // Destroy all pool mutexes
    for (int i = 0; i < NIMCP_MUTEX_POOL_SIZE; i++) {
        nimcp_platform_mutex_destroy(&g_pool.mutexes[i]);
    }

    // No stats_lock to destroy (now using atomics)

    g_pool.initialized = false;

    /* P2: Reset platform_once so pool can be re-initialized after destroy */
    g_pool_init_once = (nimcp_platform_once_t)NIMCP_PLATFORM_ONCE_INIT;

    return 0;
}

bool nimcp_mutex_pool_is_initialized(void) {
    return g_pool.initialized;
}

//=============================================================================
// Slot Management API
//=============================================================================

nimcp_mutex_slot_t nimcp_mutex_pool_acquire(const char* bridge_name) {
    if (!g_pool.initialized) {
        /* Thread-safe auto-initialize on first use via platform_once */
        nimcp_platform_once(&g_pool_init_once, mutex_pool_init_internal);
        if (g_pool_init_result != 0 || !g_pool.initialized) {
            return NIMCP_MUTEX_SLOT_INVALID;
        }
    }

    if (!bridge_name) {
        return NIMCP_MUTEX_SLOT_INVALID;
    }

    // Hash name to get slot
    uint32_t hash = fnv1a_hash(bridge_name);
    uint32_t slot = hash % NIMCP_MUTEX_POOL_SIZE;

    /* Update ref count and stats atomically (lock-free)
     * WHY: Atomic operations avoid mutex overhead for statistics
     * HOW: GCC __atomic_add_fetch provides lock-free increment
     * NOTE: This is correct without additional locking because:
     *   - ref_counts only incremented here, decremented in release
     *   - Atomicity guarantees no lost updates
     *   - Slot assignment is deterministic (hash-based)
     */
    __atomic_add_fetch(&g_pool.ref_counts[slot], 1, __ATOMIC_RELAXED);
    nimcp_atomic_fetch_add_u32(&g_pool.total_acquires, 1, NIMCP_MEMORY_ORDER_RELAXED);
    nimcp_atomic_fetch_add_u32(&g_pool.slot_usage[slot], 1, NIMCP_MEMORY_ORDER_RELAXED);

    return slot;
}

nimcp_mutex_slot_t nimcp_mutex_pool_acquire_by_id(uint32_t bridge_id) {
    if (!g_pool.initialized) {
        /* Thread-safe auto-initialize on first use via platform_once */
        nimcp_platform_once(&g_pool_init_once, mutex_pool_init_internal);
        if (g_pool_init_result != 0 || !g_pool.initialized) {
            return NIMCP_MUTEX_SLOT_INVALID;
        }
    }

    uint32_t slot = bridge_id % NIMCP_MUTEX_POOL_SIZE;

    // Update ref count and stats atomically (lock-free)
    __atomic_add_fetch(&g_pool.ref_counts[slot], 1, __ATOMIC_RELAXED);
    nimcp_atomic_fetch_add_u32(&g_pool.total_acquires, 1, NIMCP_MEMORY_ORDER_RELAXED);
    nimcp_atomic_fetch_add_u32(&g_pool.slot_usage[slot], 1, NIMCP_MEMORY_ORDER_RELAXED);

    return slot;
}

void nimcp_mutex_pool_release(nimcp_mutex_slot_t slot) {
    if (!g_pool.initialized || slot >= NIMCP_MUTEX_POOL_SIZE) {
        return;
    }

    /* P2-U22: Use CAS loop to prevent underflow. The old load-then-sub pattern
     * was racy: between the load and sub_fetch, another thread could decrement
     * to 0, causing this thread to underflow past 0 with the sub_fetch. */
    uint32_t old_count = __atomic_load_n(&g_pool.ref_counts[slot], __ATOMIC_RELAXED);
    while (old_count > 0) {
        if (__atomic_compare_exchange_n(&g_pool.ref_counts[slot], &old_count,
                                         old_count - 1, false,
                                         __ATOMIC_RELAXED, __ATOMIC_RELAXED)) {
            break;
        }
        /* CAS failed, old_count was reloaded - retry */
    }
    nimcp_atomic_fetch_add_u32(&g_pool.total_releases, 1, NIMCP_MEMORY_ORDER_RELAXED);
}

//=============================================================================
// Lock/Unlock API
//=============================================================================

int nimcp_mutex_pool_lock(nimcp_mutex_slot_t slot) {
    if (!g_pool.initialized || slot >= NIMCP_MUTEX_POOL_SIZE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "nimcp_mutex_pool_lock: g_pool is NULL");
        return -1;
    }

    int rc = nimcp_platform_mutex_lock(&g_pool.mutexes[slot]);

    // Update stats atomically (thread-safe)
    if (rc == 0) {
        nimcp_atomic_fetch_add_u32(&g_pool.total_locks, 1, NIMCP_MEMORY_ORDER_RELAXED);
    }

    return rc;
}

int nimcp_mutex_pool_trylock(nimcp_mutex_slot_t slot) {
    if (!g_pool.initialized || slot >= NIMCP_MUTEX_POOL_SIZE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "nimcp_mutex_pool_trylock: g_pool is NULL");
        return -1;
    }

    int rc = nimcp_platform_mutex_trylock(&g_pool.mutexes[slot]);

    // Update stats atomically (thread-safe)
    if (rc == 0) {
        nimcp_atomic_fetch_add_u32(&g_pool.total_locks, 1, NIMCP_MEMORY_ORDER_RELAXED);
    } else if (rc == NIMCP_BUSY || rc > 0) {
        nimcp_atomic_fetch_add_u32(&g_pool.contention_events, 1, NIMCP_MEMORY_ORDER_RELAXED);
    }

    return rc;
}

int nimcp_mutex_pool_unlock(nimcp_mutex_slot_t slot) {
    if (!g_pool.initialized || slot >= NIMCP_MUTEX_POOL_SIZE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "nimcp_mutex_pool_unlock: g_pool is NULL");
        return -1;
    }

    int rc = nimcp_platform_mutex_unlock(&g_pool.mutexes[slot]);

    // Update stats atomically (thread-safe)
    if (rc == 0) {
        nimcp_atomic_fetch_add_u32(&g_pool.total_unlocks, 1, NIMCP_MEMORY_ORDER_RELAXED);
    }

    return rc;
}

//=============================================================================
// Statistics API
//=============================================================================

int nimcp_mutex_pool_get_stats(nimcp_mutex_pool_stats_t* stats) {
    if (!stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stats is NULL");

        return -1;
    }

    if (!g_pool.initialized) {
        memset(stats, 0, sizeof(*stats));
        return 0;
    }

    // Read atomic statistics (lock-free, thread-safe)
    stats->total_acquires = nimcp_atomic_load_u32(&g_pool.total_acquires, NIMCP_MEMORY_ORDER_RELAXED);
    stats->total_releases = nimcp_atomic_load_u32(&g_pool.total_releases, NIMCP_MEMORY_ORDER_RELAXED);
    stats->total_locks = nimcp_atomic_load_u32(&g_pool.total_locks, NIMCP_MEMORY_ORDER_RELAXED);
    stats->total_unlocks = nimcp_atomic_load_u32(&g_pool.total_unlocks, NIMCP_MEMORY_ORDER_RELAXED);
    stats->contention_events = nimcp_atomic_load_u32(&g_pool.contention_events, NIMCP_MEMORY_ORDER_RELAXED);

    for (int i = 0; i < NIMCP_MUTEX_POOL_SIZE; i++) {
        stats->slot_usage[i] = nimcp_atomic_load_u32(&g_pool.slot_usage[i], NIMCP_MEMORY_ORDER_RELAXED);
    }

    return 0;
}

void nimcp_mutex_pool_reset_stats(void) {
    if (!g_pool.initialized) {
        return;
    }

    // Reset atomic statistics (lock-free)
    nimcp_atomic_store_u32(&g_pool.total_acquires, 0, NIMCP_MEMORY_ORDER_RELAXED);
    nimcp_atomic_store_u32(&g_pool.total_releases, 0, NIMCP_MEMORY_ORDER_RELAXED);
    nimcp_atomic_store_u32(&g_pool.total_locks, 0, NIMCP_MEMORY_ORDER_RELAXED);
    nimcp_atomic_store_u32(&g_pool.total_unlocks, 0, NIMCP_MEMORY_ORDER_RELAXED);
    nimcp_atomic_store_u32(&g_pool.contention_events, 0, NIMCP_MEMORY_ORDER_RELAXED);

    for (int i = 0; i < NIMCP_MUTEX_POOL_SIZE; i++) {
        nimcp_atomic_store_u32(&g_pool.slot_usage[i], 0, NIMCP_MEMORY_ORDER_RELAXED);
    }
}
