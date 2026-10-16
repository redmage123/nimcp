//=============================================================================
// nimcp_semaphore.c - Counting Semaphore Implementation
//=============================================================================
// ARCHITECTURAL OVERVIEW:
// This module implements a counting semaphore synchronization primitive built
// on top of NIMCP's mutex and condition variable abstractions. A counting
// semaphore maintains a non-negative integer count representing available
// resources, enabling resource pool management, rate limiting, and
// producer-consumer patterns.
//
// KEY DESIGN: MONITOR PATTERN
// ============================
// WHY MONITOR:
// - Encapsulates shared state (count) with synchronization (mutex + condvar)
// - Provides clean interface (wait/post) hiding complexity
// - Ensures correct synchronization (no race conditions)
// - Single point of control for resource counting
//
// SEMAPHORE ARCHITECTURE:
//
//   ┌─────────────────────────────────────────────────┐
//   │           nimcp_semaphore_t                     │
//   │  ┌──────────────────────────────────────────┐   │
//   │  │ count (uint32_t)                         │   │
//   │  │ - Current available resources            │   │
//   │  └──────────────────────────────────────────┘   │
//   │  ┌──────────────────────────────────────────┐   │
//   │  │ mutex (nimcp_mutex_t)                    │   │
//   │  │ - Protects count and statistics          │   │
//   │  └──────────────────────────────────────────┘   │
//   │  ┌──────────────────────────────────────────┐   │
//   │  │ cond (nimcp_cond_t)                      │   │
//   │  │ - Signals when count becomes > 0         │   │
//   │  └──────────────────────────────────────────┘   │
//   │  ┌──────────────────────────────────────────┐   │
//   │  │ Statistics                               │   │
//   │  │ - total_waits, total_posts               │   │
//   │  │ - current_waiters, max_waiters           │   │
//   │  └──────────────────────────────────────────┘   │
//   └─────────────────────────────────────────────────┘
//
// WHY THIS DESIGN:
// - SEPARATION OF CONCERNS: Count management vs synchronization
// - TESTABILITY: Can verify count changes without threading complexity
// - PORTABILITY: Built on portable mutex/condvar (not platform sem_t)
// - MAINTAINABILITY: Clear state transitions
//
// SEMAPHORE STATE MACHINE:
//
//                 ┌──────────┐
//                 │ count > 0│
//                 └─────┬────┘
//                       │
//          wait() ──────┼────── post()
//          (decrement)  │       (increment)
//                       │
//                 ┌─────▼────┐
//                 │ count = 0│
//                 │ (waiters │
//                 │  block)  │
//                 └──────────┘
//
// STATE TRANSITIONS:
// 1. count > 0: Resources available
//    - wait(): Decrements count, returns immediately
//    - post(): Increments count
//
// 2. count = 0: No resources available
//    - wait(): Blocks on condition variable
//    - post(): Increments count, wakes one waiter
//
// WAIT OPERATION FLOW:
//
//   wait() called
//        │
//   ┌────▼────────┐
//   │ Lock mutex  │
//   └────┬────────┘
//        │
//   ┌────▼────────┐
//   │ count > 0?  │────No──┐
//   └────┬────────┘        │
//        │Yes          ┌───▼────────┐
//   ┌────▼────────┐    │ Wait on    │
//   │ count--     │    │ condvar    │◄──┐
//   └────┬────────┘    └───┬────────┘   │
//        │                 │ signaled   │
//   ┌────▼────────┐    ┌───▼────────┐   │
//   │ Unlock mutex│    │ count > 0? │───No
//   └────┬────────┘    └───┬────────┘
//        │                 │Yes
//   ┌────▼────────┐    ┌───▼────────┐
//   │  Return     │    │ count--    │
//   └─────────────┘    └───┬────────┘
//                          │
//                     ┌────▼────────┐
//                     │ Unlock mutex│
//                     └────┬────────┘
//                          │
//                     ┌────▼────────┐
//                     │  Return     │
//                     └─────────────┘
//
// WHY WHILE LOOP IN WAIT:
// - SPURIOUS WAKEUPS: Condition variable can wake without signal
// - MULTIPLE WAITERS: Another thread may consume resource first
// - CORRECTNESS: Always re-check condition after wake
//
// POST OPERATION FLOW:
//
//   post() called
//        │
//   ┌────▼────────┐
//   │ Lock mutex  │
//   └────┬────────┘
//        │
//   ┌────▼────────┐
//   │ Overflow?   │───Yes──┐
//   └────┬────────┘        │
//        │No          ┌────▼────────┐
//   ┌────▼────────┐   │ Return ERROR│
//   │ count++     │   └─────────────┘
//   └────┬────────┘
//        │
//   ┌────▼────────┐
//   │ Signal cond │
//   │ (wake one)  │
//   └────┬────────┘
//        │
//   ┌────▼────────┐
//   │ Unlock mutex│
//   └────┬────────┘
//        │
//   ┌────▼────────┐
//   │  Return     │
//   └─────────────┘
//
// ALTERNATIVE APPROACHES REJECTED:
// - Platform sem_t (POSIX): Not portable to Windows cleanly
// - Atomic operations only: Can't block efficiently (busy-wait)
// - Mutex only (no condvar): Must busy-wait or poll
// - Spinlock: Wastes CPU while waiting (not suitable for blocking)
//
// THREAD SAFETY GUARANTEES:
// =========================
// 1. MUTEX PROTECTION
//    - All count accesses protected by mutex
//    - Statistics updates atomic within mutex
//
// 2. CONDITION VARIABLE SEMANTICS
//    - Atomically releases mutex and waits
//    - Re-acquires mutex when signaled
//    - No lost wakeups
//
// 3. OVERFLOW PROTECTION
//    - post() checks for UINT32_MAX before increment
//    - Prevents wraparound to 0
//
// 4. STATISTICS CONSISTENCY
//    - All statistics updated within mutex
//    - Snapshot reads are consistent
//
// SEMAPHORE TYPES AND USE CASES:
// ===============================
// 1. BINARY SEMAPHORE (count=0 or 1)
//    - Similar to mutex
//    - Can be signaled from different thread
//    - Example: Event notification
//
// 2. COUNTING SEMAPHORE (count=N)
//    - Resource pool with N items
//    - Example: Connection pool, thread pool slots
//
// 3. PRODUCER-CONSUMER
//    - Two semaphores: empty_slots, full_slots
//    - empty_slots.count = buffer_size initially
//    - full_slots.count = 0 initially
//
// PERFORMANCE CHARACTERISTICS:
// ============================
// WAIT OPERATIONS:
// - wait: O(1) if count > 0, blocks if count = 0
// - trywait: O(1) always (never blocks)
// - timedwait: O(1) if count > 0, blocks up to timeout
//
// POST OPERATION:
// - post: O(1) (increment + signal one waiter)
//
// MEMORY OVERHEAD:
// - Per semaphore: ~120 bytes
//   * count: 4 bytes
//   * mutex: ~40 bytes
//   * condvar: ~48 bytes
//   * statistics: 24 bytes (4 × uint64_t/uint32_t)
//
// DESIGN PATTERNS:
// ================
// 1. MONITOR: Encapsulates synchronization with state
// 2. GUARD CLAUSE: Parameter validation at function entry
// 3. RAII: Init/destroy lifecycle management
// 4. STATISTICS: Built-in profiling/debugging support
//
// SOLID PRINCIPLES:
// =================
// - SINGLE RESPONSIBILITY: Each function has one clear purpose
//   * wait: Only acquires resource
//   * post: Only releases resource
//
// - OPEN/CLOSED: Can extend with new wait variants without modifying core
//   * Add nimcp_semaphore_waitfor_multiple (future)
//   * Add nimcp_semaphore_post_multiple (future)
//
// - INTERFACE SEGREGATION: Clean, focused API
//   * Init/destroy lifecycle
//   * Wait variants (wait/trywait/timedwait)
//   * Post operation
//   * Query operation (get_count)
//
// USE CASES IN NIMCP:
// ===================
// 1. THREAD POOL SLOT MANAGEMENT
//    - Semaphore with count = max_threads
//    - wait() before spawning worker
//    - post() when worker exits
//
// 2. RATE LIMITING
//    - Semaphore with count = max_concurrent_operations
//    - wait() before operation
//    - post() after operation
//
// 3. BOUNDED BUFFER
//    - empty_slots semaphore: wait before produce
//    - full_slots semaphore: wait before consume
//
// 4. RESOURCE POOL
//    - Semaphore with count = pool_size
//    - wait() to acquire resource
//    - post() to release resource
//
// LIMITATIONS AND TRADE-OFFS:
// ===========================
// 1. NO PRIORITY ORDERING
//    TRADE-OFF: Simplicity vs priority scheduling
//    MITIGATION: Use condition variable's native fairness (typically FIFO)
//
// 2. SIGNAL WAKES ONE (not all)
//    TRADE-OFF: Efficiency vs broadcast
//    MITIGATION: Correct for semaphore semantics (one resource = one waiter)
//
// 3. STATISTICS OVERHEAD
//    TRADE-OFF: Debugging value vs memory/performance cost
//    MITIGATION: Minimal cost (few bytes, updates within existing lock)
//
// 4. UINT32_MAX LIMIT
//    TRADE-OFF: Overflow protection vs unlimited count
//    MITIGATION: 4 billion resources sufficient for all practical use cases
//
//=============================================================================

/**
 * @file nimcp_semaphore.c
 * @brief Counting semaphore implementation
 *
 * WHAT: Thread-safe counting semaphore for resource management
 * WHY: Enable resource counting, rate limiting, producer-consumer patterns
 * HOW: Monitor pattern using mutex + condition variable
 */

#include "utils/thread/nimcp_semaphore.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include <errno.h>
#include <stddef.h>
#include <string.h>
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(semaphore)

//=============================================================================
// Helper Functions
//=============================================================================

/**
 * @brief Set thread-local error message
 *
 * WHY NEEDED:
 * - Provide detailed error context for debugging
 * - Store error in thread-local storage (no cross-contamination)
 *
 * NOTE: Uses nimcp_thread error handling mechanism
 */
static void set_semaphore_error(const char* message)
{
    // For now, we rely on nimcp_thread.c's error handling
    // nimcp_thread functions set their own errors via set_thread_error()
    (void)message;  // Suppress unused warning
}

//=============================================================================
// Semaphore Lifecycle
//=============================================================================

/**
 * @brief Initialize semaphore with initial count
 *
 * ALGORITHM:
 * 1. Validate parameters (sem not NULL)
 * 2. Initialize mutex (normal type, non-recursive)
 * 3. If mutex init fails: Return error
 * 4. Initialize condition variable
 * 5. If condvar init fails: Destroy mutex, return error
 * 6. Set initial count
 * 7. Zero all statistics
 *
 * ERROR HANDLING:
 * - Cleanup on partial failure (destroy mutex if condvar init fails)
 * - Prevents resource leaks
 *
 * COMPLEXITY: O(1)
 * THREAD SAFETY: Creates new independent semaphore
 */
nimcp_result_t nimcp_semaphore_init(nimcp_semaphore_t* sem, uint32_t initial_count)
{
    // GUARD CLAUSE: Validate parameters
    // WHY: Prevent NULL dereference, catch programming errors early
    NIMCP_CHECK_THROW(sem, NIMCP_ERROR_INVALID_PARAM, "semaphore is NULL");

    // Initialize mutex (normal type)
    // WHY: Protect count and statistics from concurrent access
    nimcp_result_t result = nimcp_mutex_init(&sem->mutex, NULL);
    if (result != NIMCP_SUCCESS) {
        return result;  // Mutex init failed (error already set by nimcp_mutex_init)
    }

    // Initialize condition variable
    // WHY: Enable efficient blocking when count = 0
    result = nimcp_cond_init(&sem->cond);
    if (result != NIMCP_SUCCESS) {
        // Cleanup: Destroy mutex on condvar init failure
        // WHY: Prevent resource leak
        nimcp_mutex_destroy(&sem->mutex);
        return result;  // Condvar init failed (error already set by nimcp_cond_init)
    }

    // Set initial state
    // WHY: Semaphore starts with specified resource count
    sem->count = initial_count;

    // Zero statistics
    // WHY: Clean initial state for profiling/debugging
    sem->total_waits = 0;
    sem->total_posts = 0;
    sem->current_waiters = 0;
    sem->max_waiters = 0;

    return NIMCP_SUCCESS;
}

/**
 * @brief Destroy semaphore
 *
 * ALGORITHM:
 * 1. Validate parameters (sem not NULL)
 * 2. Destroy condition variable
 * 3. Destroy mutex
 * 4. Return success/error
 *
 * PRECONDITIONS:
 * - No threads should be waiting (undefined behavior if violated)
 * - Semaphore not in use
 *
 * ERROR HANDLING:
 * - If condvar destroy fails: Still attempt mutex destroy
 * - Return first error encountered
 *
 * COMPLEXITY: O(1)
 * THREAD SAFETY: Caller must ensure no concurrent use
 */
nimcp_result_t nimcp_semaphore_destroy(nimcp_semaphore_t* sem)
{
    // GUARD CLAUSE: Validate parameters
    NIMCP_CHECK_THROW(sem, NIMCP_ERROR_INVALID_PARAM, "semaphore is NULL");

    // Destroy condition variable
    // WHY: Free condvar resources first (no dependencies on mutex)
    nimcp_result_t cond_result = nimcp_cond_destroy(&sem->cond);

    // Destroy mutex
    // WHY: Free mutex resources
    nimcp_result_t mutex_result = nimcp_mutex_destroy(&sem->mutex);

    // Return first error encountered
    // WHY: Report failure even if one destroy succeeded
    if (cond_result != NIMCP_SUCCESS) {
        return cond_result;
    }
    return mutex_result;
}

//=============================================================================
// Wait Operations
//=============================================================================

/**
 * @brief Wait on semaphore (blocking)
 *
 * ALGORITHM:
 * 1. Validate parameters (sem not NULL)
 * 2. Lock mutex
 * 3. Increment total_waits statistic
 * 4. While count == 0:
 *    a. Increment current_waiters
 *    b. Update max_waiters (high-water mark)
 *    c. Wait on condition variable (atomically releases mutex)
 *    d. Mutex re-acquired when signaled
 *    e. Decrement current_waiters
 * 5. Decrement count (resource acquired)
 * 6. Unlock mutex
 * 7. Return success
 *
 * WHY WHILE LOOP:
 * - Spurious wakeups: Condition variable can wake without post()
 * - Multiple waiters: Another thread may acquire resource first
 * - Correctness: Always verify condition after wake
 *
 * BLOCKING SEMANTICS:
 * - count > 0: Returns immediately (fast path)
 * - count = 0: Blocks until post() increments count
 *
 * COMPLEXITY: O(1) if count > 0, blocks indefinitely if count = 0
 * THREAD SAFETY: Fully safe (mutex protected, atomic wait)
 */
nimcp_result_t nimcp_semaphore_wait(nimcp_semaphore_t* sem)
{
    // GUARD CLAUSE: Validate parameters
    NIMCP_CHECK_THROW(sem, NIMCP_ERROR_INVALID_PARAM, "semaphore is NULL");

    // CRITICAL SECTION START: Lock mutex
    // WHY: Protect count and statistics from concurrent modification
    nimcp_result_t result = nimcp_mutex_lock(&sem->mutex);
    if (result != NIMCP_SUCCESS) {
        return result;
    }

    // Update statistics
    // WHY: Track semaphore usage for profiling/debugging
    sem->total_waits++;

    // WAIT LOOP: Block while no resources available
    // WHY WHILE: Spurious wakeups, multiple waiters (re-check condition)
    while (sem->count == 0) {
        // Track waiters for statistics
        sem->current_waiters++;
        if (sem->current_waiters > sem->max_waiters) {
            sem->max_waiters = sem->current_waiters;  // Update high-water mark
        }

        // BLOCK: Wait on condition variable
        // WHY: Efficient blocking (no busy-wait)
        // HOW: Atomically releases mutex, waits for signal, re-acquires mutex
        result = nimcp_cond_wait(&sem->cond, &sem->mutex);

        // Update waiter count (woke up)
        sem->current_waiters--;

        // Check for error during wait
        if (result != NIMCP_SUCCESS) {
            nimcp_mutex_unlock(&sem->mutex);
            return result;
        }

        // Loop back to re-check count (spurious wakeup or consumed by another thread)
    }

    // ACQUIRE RESOURCE: Decrement count
    // WHY: count > 0 (loop exited), safe to decrement
    sem->count--;

    // CRITICAL SECTION END: Unlock mutex
    nimcp_mutex_unlock(&sem->mutex);

    return NIMCP_SUCCESS;
}

/**
 * @brief Try to wait on semaphore (non-blocking)
 *
 * ALGORITHM:
 * 1. Validate parameters (sem not NULL)
 * 2. Lock mutex
 * 3. Increment total_waits statistic
 * 4. If count > 0:
 *    a. Decrement count
 *    b. Unlock mutex
 *    c. Return NIMCP_SUCCESS
 * 5. Else (count == 0):
 *    a. Unlock mutex
 *    b. Return NIMCP_BUSY (not an error!)
 *
 * NON-BLOCKING GUARANTEE:
 * - Never waits on condition variable
 * - Always returns immediately
 *
 * RETURN VALUE SEMANTICS:
 * - NIMCP_SUCCESS: Resource acquired
 * - NIMCP_BUSY: No resources available (expected, not error)
 * - NIMCP_ERROR_*: Actual error
 *
 * COMPLEXITY: O(1) (always non-blocking)
 * THREAD SAFETY: Fully safe (mutex protected)
 */
nimcp_result_t nimcp_semaphore_trywait(nimcp_semaphore_t* sem)
{
    // GUARD CLAUSE: Validate parameters
    NIMCP_CHECK_THROW(sem, NIMCP_ERROR_INVALID_PARAM, "semaphore is NULL");

    // CRITICAL SECTION START: Lock mutex
    nimcp_result_t result = nimcp_mutex_lock(&sem->mutex);
    if (result != NIMCP_SUCCESS) {
        return result;
    }

    // Update statistics
    sem->total_waits++;

    // CHECK AVAILABILITY: Non-blocking check
    if (sem->count > 0) {
        // FAST PATH: Resource available, acquire immediately
        sem->count--;
        nimcp_mutex_unlock(&sem->mutex);
        return NIMCP_SUCCESS;
    } else {
        // SLOW PATH: No resources available, return immediately
        // WHY NIMCP_BUSY: Not an error, just status (consistent with mutex_trylock)
        nimcp_mutex_unlock(&sem->mutex);
        return NIMCP_BUSY;
    }
}

/**
 * @brief Wait on semaphore with timeout
 *
 * ALGORITHM:
 * 1. Validate parameters (sem not NULL)
 * 2. Lock mutex
 * 3. Increment total_waits statistic
 * 4. While count == 0:
 *    a. Increment current_waiters
 *    b. Update max_waiters
 *    c. Timed wait on condition variable
 *    d. Decrement current_waiters
 *    e. If timeout: Unlock, return NIMCP_BUSY
 *    f. If signaled: Continue loop (re-check count)
 * 5. Decrement count (resource acquired)
 * 6. Unlock mutex
 * 7. Return success
 *
 * TIMEOUT SEMANTICS:
 * - timeout_ms = 0: Equivalent to trywait
 * - timeout_ms > 0: Wait up to timeout_ms milliseconds
 * - If timeout expires: Return NIMCP_BUSY (not error)
 *
 * WHY TIMEOUT IN LOOP:
 * - First iteration: Wait up to timeout_ms
 * - Subsequent iterations: Timeout already expired (return NIMCP_BUSY)
 * - Handles spurious wakeups correctly
 *
 * COMPLEXITY: O(1) if count > 0, blocks up to timeout_ms if count = 0
 * THREAD SAFETY: Fully safe (mutex protected, atomic timed wait)
 */
nimcp_result_t nimcp_semaphore_timedwait(nimcp_semaphore_t* sem, uint32_t timeout_ms)
{
    // GUARD CLAUSE: Validate parameters
    NIMCP_CHECK_THROW(sem, NIMCP_ERROR_INVALID_PARAM, "semaphore is NULL");

    // CRITICAL SECTION START: Lock mutex
    nimcp_result_t result = nimcp_mutex_lock(&sem->mutex);
    if (result != NIMCP_SUCCESS) {
        return result;
    }

    // Update statistics
    sem->total_waits++;

    // TIMED WAIT LOOP: Block while no resources available, up to timeout
    while (sem->count == 0) {
        // Track waiters for statistics
        sem->current_waiters++;
        if (sem->current_waiters > sem->max_waiters) {
            sem->max_waiters = sem->current_waiters;
        }

        // TIMED BLOCK: Wait on condition variable with timeout
        // WHY: Prevent indefinite blocking
        // HOW: Atomically releases mutex, waits for signal or timeout, re-acquires mutex
        result = nimcp_cond_timedwait(&sem->cond, &sem->mutex, timeout_ms);

        // Update waiter count (woke up or timed out)
        sem->current_waiters--;

        // Check result
        if (result == NIMCP_BUSY) {
            // TIMEOUT: No resources available within timeout
            // WHY NIMCP_BUSY: Expected outcome, not error
            nimcp_mutex_unlock(&sem->mutex);
            return NIMCP_BUSY;
        } else if (result != NIMCP_SUCCESS) {
            // ERROR: Wait failed for other reason
            nimcp_mutex_unlock(&sem->mutex);
            return result;
        }

        // SIGNALED: Loop back to re-check count
        // WHY: Spurious wakeup or resource consumed by another thread
    }

    // ACQUIRE RESOURCE: Decrement count
    // WHY: count > 0 (loop exited), safe to decrement
    sem->count--;

    // CRITICAL SECTION END: Unlock mutex
    nimcp_mutex_unlock(&sem->mutex);

    return NIMCP_SUCCESS;
}

//=============================================================================
// Post Operation
//=============================================================================

/**
 * @brief Post to semaphore (increment, wake waiter)
 *
 * ALGORITHM:
 * 1. Validate parameters (sem not NULL)
 * 2. Lock mutex
 * 3. Check for overflow (count == UINT32_MAX)
 * 4. If overflow: Unlock, return error
 * 5. Increment count
 * 6. Increment total_posts statistic
 * 7. Signal condition variable (wake one waiter)
 * 8. Unlock mutex
 * 9. Return success
 *
 * OVERFLOW PROTECTION:
 * - If count == UINT32_MAX: Cannot increment (prevent wraparound)
 * - Returns NIMCP_ERROR_SYSTEM
 * - Highly unlikely in practice (4,294,967,295 resources)
 *
 * SIGNAL SEMANTICS:
 * - Wakes ONE waiting thread (FIFO typically)
 * - If no waiters: Signal is lost (count remains incremented)
 * - Fair scheduling: First waiter gets resource
 *
 * WHY SIGNAL (not broadcast):
 * - Only one resource available (one waiter can proceed)
 * - More efficient (no thundering herd)
 *
 * COMPLEXITY: O(1)
 * THREAD SAFETY: Fully safe (mutex protected, signal)
 */
nimcp_result_t nimcp_semaphore_post(nimcp_semaphore_t* sem)
{
    // GUARD CLAUSE: Validate parameters
    NIMCP_CHECK_THROW(sem, NIMCP_ERROR_INVALID_PARAM, "semaphore is NULL");

    // CRITICAL SECTION START: Lock mutex
    nimcp_result_t result = nimcp_mutex_lock(&sem->mutex);
    if (result != NIMCP_SUCCESS) {
        return result;
    }

    // OVERFLOW CHECK: Prevent wraparound to 0
    // WHY: count == UINT32_MAX cannot increment (catastrophic if wraps to 0)
    if (sem->count == UINT32_MAX) {
        nimcp_mutex_unlock(&sem->mutex);
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_SYSTEM, "Semaphore overflow: count already at UINT32_MAX");
    }

    // RELEASE RESOURCE: Increment count
    // WHY: Make resource available
    sem->count++;

    // Update statistics
    sem->total_posts++;

    // WAKE WAITER: Signal one waiting thread
    // WHY: If waiters exist, wake one to consume resource
    // HOW: cond_signal wakes one waiter (FIFO typically)
    result = nimcp_cond_signal(&sem->cond);

    // CRITICAL SECTION END: Unlock mutex
    nimcp_mutex_unlock(&sem->mutex);

    // Return signal result
    // WHY: Report if signal failed (rare, but possible)
    return result;
}

//=============================================================================
// Query Operation
//=============================================================================

/**
 * @brief Get current semaphore count
 *
 * ALGORITHM:
 * 1. Validate parameters (sem not NULL, return 0 if NULL)
 * 2. Lock mutex
 * 3. Read count
 * 4. Unlock mutex
 * 5. Return count
 *
 * RACE CONDITION CAVEAT:
 * - Count may change immediately after read
 * - Use only for monitoring/debugging
 * - Don't make synchronization decisions based on count
 * - Use wait/trywait for actual resource acquisition
 *
 * ERROR HANDLING:
 * - sem is NULL: Returns 0 (no error channel, function returns uint32_t)
 *
 * COMPLEXITY: O(1)
 * THREAD SAFETY: Fully safe (mutex protected read)
 */
uint32_t nimcp_semaphore_get_count(const nimcp_semaphore_t* sem)
{
    // GUARD CLAUSE: Validate parameters
    // WHY NULL CHECK: Prevent crash, return 0 as sentinel
    if (!sem) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "nimcp_semaphore_get_count: sem is NULL");

            return 0;
    }

    // CRITICAL SECTION START: Lock mutex (cast away const for lock)
    // WHY CONST CAST: Reading count doesn't logically modify semaphore,
    // but mutex operations require non-const pointer
    nimcp_semaphore_t* sem_mut = (nimcp_semaphore_t*)sem;

    nimcp_result_t result = nimcp_mutex_lock(&sem_mut->mutex);
    if (result != NIMCP_SUCCESS) {
        return 0;  // Error locking, return 0 as sentinel
    }

    // READ COUNT: Atomic read within mutex
    uint32_t count = sem->count;

    // CRITICAL SECTION END: Unlock mutex
    nimcp_mutex_unlock(&sem_mut->mutex);

    return count;
}
