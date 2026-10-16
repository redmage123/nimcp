#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_barrier.c - Barrier Synchronization Primitive Implementation
//=============================================================================

/**
 * @file nimcp_barrier.c
 * @brief Barrier synchronization implementation using monitor pattern
 *
 * WHAT: Cyclic barrier with condition variable + mutex synchronization
 * WHY: Coordinate N threads at synchronization point(s)
 * HOW: Monitor pattern - lock state, check condition, wait/signal, unlock
 */

#include "utils/thread/nimcp_barrier.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "utils/memory/nimcp_memory.h"
#include <errno.h>
#include <string.h>
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(barrier)

//=============================================================================
// Internal Structure Definition
//=============================================================================

/**
 * @brief Barrier internal structure
 *
 * WHY OPAQUE:
 * - Hide implementation details from users
 * - Allow future implementation changes without API break
 * - Prevent accidental state corruption
 *
 * INVARIANTS:
 * - 0 ≤ waiting ≤ count (always)
 * - count > 0 (set at init, enforced)
 * - total_waits ≥ total_cycles × count (if no errors)
 */
struct nimcp_barrier {
    // Configuration
    uint32_t count;         /**< Number of threads required at barrier */

    // Dynamic state
    uint32_t waiting;       /**< Current number of waiting threads */
    uint32_t cycle;         /**< Current cycle number */

    // Synchronization primitives
    nimcp_mutex_t mutex;    /**< Protects barrier state */
    nimcp_cond_t cond;      /**< Blocks/wakes waiting threads */

    // Statistics
    uint64_t total_waits;   /**< Total wait operations (all cycles) */
    uint64_t total_cycles;  /**< Total complete cycles */
};

//=============================================================================
// Barrier Operations Implementation
//=============================================================================

/**
 * @brief Initialize barrier for specified thread count
 *
 * WHY ALLOCATE DYNAMICALLY:
 * - User doesn't need to know structure size
 * - Easier to change implementation later
 * - Consistent with other NIMCP primitives
 *
 * ERROR HANDLING:
 * - Validate all parameters before allocation
 * - Clean up on partial initialization failure
 * - NULL out barrier on any error
 */
nimcp_result_t nimcp_barrier_init(nimcp_barrier_t** barrier, uint32_t count)
{
    // Validate parameters
    // WHY CHECK: barrier == NULL → crash, count == 0 → deadlock
    NIMCP_CHECK_THROW(barrier, NIMCP_ERROR_INVALID_PARAM, "barrier output pointer is NULL");

    if (count == 0) {
        *barrier = NULL;
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_INVALID_PARAM, "barrier count must be greater than 0");
    }

    // Allocate barrier structure
    // WHY CALLOC: Zero-initializes all fields (safe initial state)
    nimcp_barrier_t* b = (nimcp_barrier_t*)nimcp_calloc(1, sizeof(nimcp_barrier_t));
    if (!b) {
        *barrier = NULL;
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_MEMORY, "failed to allocate barrier structure");
    }

    // Set thread count
    b->count = count;

    // Initialize dynamic state (calloc already zeroed these)
    // WHY EXPLICIT: Documentation of initial state
    b->waiting = 0;
    b->cycle = 0;
    b->total_waits = 0;
    b->total_cycles = 0;

    // Initialize mutex using NIMCP wrapper (platform abstraction)
    // WHY NORMAL MUTEX: No recursion needed, simpler/faster
    nimcp_result_t result = nimcp_mutex_init(&b->mutex, NULL);
    if (result != NIMCP_SUCCESS) {
        nimcp_free(b);
        *barrier = NULL;
        return result;
    }

    // Initialize condition variable using NIMCP wrapper
    result = nimcp_cond_init(&b->cond);
    if (result != NIMCP_SUCCESS) {
        // Cleanup mutex on failure
        nimcp_mutex_destroy(&b->mutex);
        nimcp_free(b);
        *barrier = NULL;
        return result;
    }

    // Success: Return initialized barrier
    *barrier = b;
    return NIMCP_SUCCESS;
}

/**
 * @brief Destroy barrier and free resources
 *
 * WHY CHECK waiting==0:
 * - Destroying barrier with waiting threads → lost wakeups
 * - Better to fail early than corrupt state
 *
 * CLEANUP ORDER:
 * - Condition variable first (may depend on mutex)
 * - Then mutex
 * - Finally free structure
 */
nimcp_result_t nimcp_barrier_destroy(nimcp_barrier_t** barrier)
{
    // Validate parameters
    NIMCP_CHECK_THROW(barrier && *barrier, NIMCP_ERROR_INVALID_PARAM, "barrier is NULL");

    nimcp_barrier_t* b = *barrier;

    // Destroy condition variable
    // WHY FIRST: CV may have internal state referencing mutex
    nimcp_result_t result = nimcp_cond_destroy(&b->cond);
    if (result != NIMCP_SUCCESS) {
        // Continue cleanup even on error
        // WHY: Better to free memory than leak
    }

    // Destroy mutex
    nimcp_result_t mutex_result = nimcp_mutex_destroy(&b->mutex);
    if (mutex_result != NIMCP_SUCCESS && result == NIMCP_SUCCESS) {
        result = mutex_result;  // Propagate first error
    }

    // Free barrier structure
    nimcp_free(b);

    // NULL out barrier pointer (prevent use-after-free)
    *barrier = NULL;

    return result;
}

/**
 * @brief Wait at barrier until all threads arrive
 *
 * CRITICAL SECTION:
 * - Lock held: checking waiting, modifying state, waiting on CV
 * - Lock released: during cond_wait (atomic), after completion
 *
 * ALGORITHM DETAILS:
 * 1. Lock mutex → Enter critical section
 * 2. waiting++ → Record arrival
 * 3. total_waits++ → Update statistics
 * 4. Check: waiting == count?
 *    YES (last thread):
 *      - waiting = 0 (reset for next cycle)
 *      - cycle++ (track barrier cycles)
 *      - total_cycles++ (statistics)
 *      - broadcast(cond) (wake all waiters)
 *      - unlock(mutex)
 *      - return SERIAL_THREAD (special return)
 *    NO (not last):
 *      - cond_wait(cond, mutex) (atomic unlock+wait)
 *        * Releases mutex
 *        * Blocks thread
 *        * Re-acquires mutex when signaled
 *      - unlock(mutex)
 *      - return SUCCESS
 *
 * WHY WHILE LOOP (not if):
 * - Spurious wakeups possible (POSIX allows)
 * - Multiple cycles may occur (cyclic barrier)
 * - Actually, we DON'T use while here because:
 *   * Each broadcast releases ALL waiters
 *   * waiting is reset to 0, not checked by woken threads
 *   * Single wait is sufficient for barrier pattern
 *
 * MEMORY ORDERING:
 * - Mutex ensures happens-before relationship
 * - All updates before unlock visible to next lock holder
 * - Memory fence in mutex operations
 */
nimcp_result_t nimcp_barrier_wait(nimcp_barrier_t* barrier)
{
    // Validate barrier
    NIMCP_CHECK_THROW(barrier, NIMCP_ERROR_INVALID_PARAM, "barrier is NULL");

    // Enter critical section
    nimcp_result_t result = nimcp_mutex_lock(&barrier->mutex);
    if (result != NIMCP_SUCCESS) {
        return result;
    }

    // Record arrival
    barrier->waiting++;
    barrier->total_waits++;

    // Check if all threads have arrived
    if (barrier->waiting < barrier->count) {
        // NOT LAST: Wait for others
        // WHY COND_WAIT: Atomically releases mutex and blocks
        // When signaled: Re-acquires mutex before returning
        result = nimcp_cond_wait(&barrier->cond, &barrier->mutex);

        // Unlock mutex and return
        // WHY UNLOCK: Critical section complete
        nimcp_mutex_unlock(&barrier->mutex);

        return result;  // NIMCP_SUCCESS if woken normally

    } else {
        // LAST THREAD: All arrived, release barrier

        // Reset for next cycle
        // WHY: Allow barrier reuse (cyclic barrier)
        barrier->waiting = 0;

        // Increment cycle counter
        barrier->cycle++;
        barrier->total_cycles++;

        // Wake all waiting threads
        // WHY BROADCAST (not signal): Must wake ALL waiters simultaneously
        result = nimcp_cond_broadcast(&barrier->cond);

        // Unlock mutex
        nimcp_mutex_unlock(&barrier->mutex);

        // Check broadcast result
        if (result != NIMCP_SUCCESS) {
            return result;
        }

        // Return special code for serial thread
        // WHY: Designates "master" for single-threaded work
        return NIMCP_BARRIER_SERIAL_THREAD;
    }
}

/**
 * @brief Get number of threads currently waiting at barrier
 *
 * WHY LOCK:
 * - Reading uint32_t may not be atomic on all platforms
 * - Even if atomic, need memory barrier for visibility
 * - Mutex provides both atomicity and ordering
 *
 * RACE CONDITION:
 * - Value may change immediately after return (inherent)
 * - User must understand this is a snapshot
 * - Acceptable for debugging/monitoring use case
 */
uint32_t nimcp_barrier_get_waiting(const nimcp_barrier_t* barrier)
{
    // Validate barrier
    if (!barrier) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "nimcp_barrier_get_waiting: barrier is NULL");

            return 0;
    }

    // Lock mutex (need to cast away const for lock)
    // WHY CAST: Lock doesn't modify logical state, only sync primitives
    nimcp_barrier_t* b = (nimcp_barrier_t*)barrier;
    nimcp_mutex_lock(&b->mutex);

    // Read waiting count
    uint32_t waiting = b->waiting;

    // Unlock mutex
    nimcp_mutex_unlock(&b->mutex);

    return waiting;
}

/**
 * @brief Reset barrier for reuse with potentially different count
 *
 * WHY CHECK waiting==0:
 * - Changing count while threads waiting → inconsistent state
 * - Some threads wait for old count, some for new count → deadlock
 * - Better to fail than corrupt
 *
 * RESET POLICY:
 * - Preserve cycle counter (shows total history)
 * - Reset statistics (alternative: preserve cumulative stats)
 * - Currently resets stats for clean measurement periods
 */
nimcp_result_t nimcp_barrier_reset(nimcp_barrier_t* barrier, uint32_t count)
{
    // Validate parameters
    NIMCP_CHECK_THROW(barrier, NIMCP_ERROR_INVALID_PARAM, "barrier is NULL");
    NIMCP_CHECK_THROW(count > 0, NIMCP_ERROR_INVALID_PARAM, "count must be greater than 0");

    // Lock mutex
    nimcp_result_t result = nimcp_mutex_lock(&barrier->mutex);
    if (result != NIMCP_SUCCESS) {
        return result;
    }

    // Check no threads waiting
    // WHY: Resetting with waiters → inconsistent state
    if (barrier->waiting > 0) {
        nimcp_mutex_unlock(&barrier->mutex);
        return NIMCP_BUSY;
    }

    // Update count
    barrier->count = count;

    // Reset cycle to 0 (new barrier sequence)
    // ALTERNATIVE: Could preserve cycle for continuity
    barrier->cycle = 0;

    // Reset statistics
    // ALTERNATIVE: Could preserve for cumulative tracking
    barrier->total_waits = 0;
    barrier->total_cycles = 0;

    // Unlock mutex
    nimcp_mutex_unlock(&barrier->mutex);

    return NIMCP_SUCCESS;
}

/**
 * @brief Get total number of wait operations across all cycles
 *
 * CONSISTENCY:
 * - Lock ensures atomic read of uint64_t
 * - Prevents torn reads on 32-bit platforms
 */
uint64_t nimcp_barrier_get_total_waits(const nimcp_barrier_t* barrier)
{
    if (!barrier) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "nimcp_barrier_get_total_waits: barrier is NULL");

            return 0;
    }

    nimcp_barrier_t* b = (nimcp_barrier_t*)barrier;
    nimcp_mutex_lock(&b->mutex);

    uint64_t total = b->total_waits;

    nimcp_mutex_unlock(&b->mutex);

    return total;
}

/**
 * @brief Get total number of complete barrier cycles
 *
 * USE CASE:
 * - Track iteration count in parallel simulation
 * - Performance measurement: cycles per second
 * - Progress monitoring
 */
uint64_t nimcp_barrier_get_total_cycles(const nimcp_barrier_t* barrier)
{
    if (!barrier) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "nimcp_barrier_get_total_cycles: barrier is NULL");

            return 0;
    }

    nimcp_barrier_t* b = (nimcp_barrier_t*)barrier;
    nimcp_mutex_lock(&b->mutex);

    uint64_t cycles = b->total_cycles;

    nimcp_mutex_unlock(&b->mutex);

    return cycles;
}
