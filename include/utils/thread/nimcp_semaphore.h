//=============================================================================
// nimcp_semaphore.h - Counting Semaphore for NIMCP Threading Utilities
//=============================================================================

#ifndef NIMCP_SEMAPHORE_H
#define NIMCP_SEMAPHORE_H

#include <stdint.h>
#include "utils/thread/nimcp_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file nimcp_semaphore.h
 * @brief Counting semaphore implementation using mutex and condition variable
 *
 * WHAT: Counting semaphore synchronization primitive
 * WHY: Enable resource counting, rate limiting, and producer-consumer patterns
 * HOW: Built on nimcp_mutex_t and nimcp_cond_t primitives
 *
 * Features:
 * - Counting semaphore (not just binary)
 * - Wait (blocking), try-wait (non-blocking), timed-wait (with timeout)
 * - Post (increment count, wake waiters)
 * - Thread-safe statistics tracking
 * - Overflow protection
 * - SRP - semaphore operations only
 *
 * TYPICAL USE CASES:
 * 1. Resource pool management (N available resources)
 * 2. Rate limiting (max N concurrent operations)
 * 3. Producer-consumer with bounded buffer
 * 4. Thread throttling (max N active threads)
 *
 * EXAMPLE (resource pool with 5 slots):
 *   nimcp_semaphore_t sem;
 *   nimcp_semaphore_init(&sem, 5);  // 5 available slots
 *
 *   // Consumer thread
 *   nimcp_semaphore_wait(&sem);     // Acquire slot (blocks if count=0)
 *   use_resource();
 *   nimcp_semaphore_post(&sem);     // Release slot
 *
 * EXAMPLE (producer-consumer):
 *   nimcp_semaphore_t empty_slots, full_slots;
 *   nimcp_semaphore_init(&empty_slots, BUFFER_SIZE);  // All empty
 *   nimcp_semaphore_init(&full_slots, 0);             // None full
 *
 *   // Producer
 *   nimcp_semaphore_wait(&empty_slots);
 *   produce_item();
 *   nimcp_semaphore_post(&full_slots);
 *
 *   // Consumer
 *   nimcp_semaphore_wait(&full_slots);
 *   consume_item();
 *   nimcp_semaphore_post(&empty_slots);
 *
 * DESIGN DECISIONS:
 * - Use mutex + condvar (not platform-specific sem_t)
 *   WHY: Maximum portability, consistent with NIMCP threading model
 * - Count type is uint32_t (not int)
 *   WHY: Negative counts are meaningless, prevent wraparound bugs
 * - Statistics tracking included
 *   WHY: Debugging, profiling, system health monitoring
 * - Overflow protection on post
 *   WHY: Prevent count overflow, detect programming errors
 *
 * THREAD SAFETY:
 * - All operations are thread-safe
 * - Uses internal mutex for protection
 * - Condition variable for efficient waiting
 * - No busy-waiting (blocks efficiently)
 */

//=============================================================================
// Semaphore Structure
//=============================================================================

/**
 * WHAT: Counting semaphore state and statistics
 * WHY: Track count, waiters, and usage statistics for debugging/profiling
 * HOW: Protected by internal mutex, signaled via condition variable
 *
 * IMPLEMENTATION INVARIANTS:
 * - count >= 0 (always non-negative)
 * - total_waits >= 0
 * - total_posts >= 0
 * - max_waiters >= current_waiters
 *
 * STATISTICS:
 * - total_waits: Cumulative number of wait calls
 * - total_posts: Cumulative number of post calls
 * - current_waiters: Threads currently blocked in wait
 * - max_waiters: Peak number of concurrent waiters (high-water mark)
 *
 * WHY STATISTICS:
 * - Debugging: Identify contention hotspots
 * - Profiling: Measure semaphore usage patterns
 * - Monitoring: Track system health (high max_waiters = bottleneck)
 */
typedef struct nimcp_semaphore {
    uint32_t count;            // Current semaphore count
    nimcp_mutex_t mutex;       // Protects count and statistics
    nimcp_cond_t cond;         // Signals when count > 0

    // Statistics (for debugging/profiling)
    uint64_t total_waits;      // Total wait calls (including try/timed)
    uint64_t total_posts;      // Total post calls
    uint32_t current_waiters;  // Threads currently waiting
    uint32_t max_waiters;      // Maximum concurrent waiters seen
} nimcp_semaphore_t;

//=============================================================================
// Semaphore Operations
//=============================================================================

/**
 * @brief Initialize semaphore with initial count
 *
 * WHAT: Create semaphore with specified initial count
 * WHY: Prepare semaphore for use, set resource availability
 * HOW: Initialize mutex, condvar, count, and statistics
 *
 * ALGORITHM:
 * 1. Validate parameters (sem not NULL)
 * 2. Initialize internal mutex
 * 3. Initialize internal condition variable
 * 4. Set count to initial_count
 * 5. Zero all statistics
 *
 * INITIAL COUNT SEMANTICS:
 * - initial_count=0: All waiters block (typical for "full slots")
 * - initial_count=N: N resources available (typical for resource pool)
 * - initial_count=UINT32_MAX: Effectively unlimited (rare)
 *
 * ERROR CASES:
 * - sem is NULL: Returns NIMCP_ERROR_INVALID_PARAM
 * - Mutex init fails: Returns NIMCP_ERROR_SYSTEM
 * - Condvar init fails: Returns NIMCP_ERROR_SYSTEM (mutex destroyed)
 *
 * COMPLEXITY: O(1)
 * THREAD SAFETY: Creates new independent semaphore (thread-safe)
 *
 * @param sem Semaphore to initialize
 * @param initial_count Starting count (number of available resources)
 * @return NIMCP_SUCCESS or NIMCP_ERROR_*
 */
nimcp_result_t nimcp_semaphore_init(nimcp_semaphore_t* sem, uint32_t initial_count);

/**
 * @brief Destroy semaphore
 *
 * WHAT: Free semaphore resources
 * WHY: Clean shutdown, prevent resource leaks
 * HOW: Destroy mutex and condition variable
 *
 * PRECONDITIONS:
 * - No threads should be waiting on semaphore
 * - Semaphore should not be in use
 *
 * ALGORITHM:
 * 1. Validate parameters (sem not NULL)
 * 2. Destroy condition variable
 * 3. Destroy mutex
 *
 * ERROR CASES:
 * - sem is NULL: Returns NIMCP_ERROR_INVALID_PARAM
 * - Threads waiting: Undefined behavior (destroy may fail)
 * - Condvar destroy fails: Returns NIMCP_ERROR_SYSTEM
 * - Mutex destroy fails: Returns NIMCP_ERROR_SYSTEM
 *
 * COMPLEXITY: O(1)
 * THREAD SAFETY: Caller must ensure no concurrent use
 *
 * @param sem Semaphore to destroy
 * @return NIMCP_SUCCESS or NIMCP_ERROR_*
 */
nimcp_result_t nimcp_semaphore_destroy(nimcp_semaphore_t* sem);

/**
 * @brief Wait on semaphore (decrement, blocking)
 *
 * WHAT: Acquire resource from semaphore, block if unavailable
 * WHY: Wait for resource to become available
 * HOW: Decrement count if >0, else wait on condition variable
 *
 * ALGORITHM:
 * 1. Validate parameters (sem not NULL)
 * 2. Lock mutex
 * 3. Increment total_waits statistic
 * 4. While count == 0:
 *    a. Increment current_waiters
 *    b. Update max_waiters if needed
 *    c. Wait on condition variable (releases mutex)
 *    d. Mutex re-acquired when signaled
 *    e. Decrement current_waiters
 * 5. Decrement count (resource acquired)
 * 6. Unlock mutex
 *
 * WHY WHILE LOOP (not if):
 * - Spurious wakeups: Condition variable may wake without signal
 * - Multiple waiters: Another thread may consume resource first
 * - Correctness: Always re-check count after wake
 *
 * BLOCKING BEHAVIOR:
 * - If count > 0: Returns immediately (non-blocking path)
 * - If count == 0: Blocks until post() increments count
 *
 * ERROR CASES:
 * - sem is NULL: Returns NIMCP_ERROR_INVALID_PARAM
 * - Mutex operations fail: Returns NIMCP_ERROR_SYSTEM
 *
 * COMPLEXITY: O(1) if count > 0, blocks indefinitely if count == 0
 * THREAD SAFETY: Fully safe (mutex protected)
 *
 * @param sem Semaphore to wait on
 * @return NIMCP_SUCCESS or NIMCP_ERROR_*
 */
nimcp_result_t nimcp_semaphore_wait(nimcp_semaphore_t* sem);

/**
 * @brief Try to wait on semaphore (non-blocking)
 *
 * WHAT: Attempt to acquire resource without blocking
 * WHY: Polling pattern, avoid blocking in critical sections
 * HOW: Decrement count if >0, else return NIMCP_BUSY immediately
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
 * - Always returns immediately (never blocks)
 * - No waiting on condition variable
 *
 * RETURN VALUE SEMANTICS:
 * - NIMCP_SUCCESS: Resource acquired, count decremented
 * - NIMCP_BUSY: No resources available (expected outcome, not error)
 * - NIMCP_ERROR_*: Actual error occurred
 *
 * TYPICAL USAGE (polling):
 *   while (nimcp_semaphore_trywait(&sem) == NIMCP_BUSY) {
 *       do_other_work();
 *       usleep(1000);  // Back off
 *   }
 *   // Got resource
 *
 * ERROR CASES:
 * - sem is NULL: Returns NIMCP_ERROR_INVALID_PARAM
 *
 * COMPLEXITY: O(1) (always non-blocking)
 * THREAD SAFETY: Fully safe (mutex protected)
 *
 * @param sem Semaphore to try waiting on
 * @return NIMCP_SUCCESS if acquired, NIMCP_BUSY if unavailable, NIMCP_ERROR_* on error
 */
nimcp_result_t nimcp_semaphore_trywait(nimcp_semaphore_t* sem);

/**
 * @brief Wait on semaphore with timeout
 *
 * WHAT: Acquire resource with timeout limit
 * WHY: Prevent indefinite blocking, implement watchdog patterns
 * HOW: Wait on condition variable with timeout
 *
 * ALGORITHM:
 * 1. Validate parameters (sem not NULL)
 * 2. Lock mutex
 * 3. Increment total_waits statistic
 * 4. While count == 0:
 *    a. Increment current_waiters
 *    b. Update max_waiters if needed
 *    c. Timed wait on condition variable with timeout_ms
 *    d. Decrement current_waiters
 *    e. If timeout occurred: Return NIMCP_BUSY
 *    f. If signaled: Continue loop (re-check count)
 * 5. Decrement count (resource acquired)
 * 6. Unlock mutex
 *
 * TIMEOUT SEMANTICS:
 * - timeout_ms=0: Equivalent to trywait (immediate return)
 * - timeout_ms>0: Wait up to timeout_ms milliseconds
 * - If timeout expires: Return NIMCP_BUSY (not error)
 *
 * RETURN VALUE SEMANTICS:
 * - NIMCP_SUCCESS: Resource acquired before timeout
 * - NIMCP_BUSY: Timeout expired (expected outcome, not error)
 * - NIMCP_ERROR_*: Actual error occurred
 *
 * TYPICAL USAGE (timeout pattern):
 *   nimcp_result_t result = nimcp_semaphore_timedwait(&sem, 5000);  // 5s
 *   if (result == NIMCP_SUCCESS) {
 *       // Got resource
 *   } else if (result == NIMCP_BUSY) {
 *       // Timeout - resource not available
 *   } else {
 *       // Error occurred
 *   }
 *
 * ERROR CASES:
 * - sem is NULL: Returns NIMCP_ERROR_INVALID_PARAM
 * - Mutex operations fail: Returns NIMCP_ERROR_SYSTEM
 *
 * COMPLEXITY: O(1) if count > 0, blocks up to timeout_ms if count == 0
 * THREAD SAFETY: Fully safe (mutex protected)
 *
 * @param sem Semaphore to wait on
 * @param timeout_ms Timeout in milliseconds
 * @return NIMCP_SUCCESS if acquired, NIMCP_BUSY if timeout, NIMCP_ERROR_* on error
 */
nimcp_result_t nimcp_semaphore_timedwait(nimcp_semaphore_t* sem, uint32_t timeout_ms);

/**
 * @brief Post to semaphore (increment, wake waiter)
 *
 * WHAT: Release resource back to semaphore
 * WHY: Make resource available, wake waiting threads
 * HOW: Increment count, signal condition variable
 *
 * ALGORITHM:
 * 1. Validate parameters (sem not NULL)
 * 2. Lock mutex
 * 3. Check for overflow (count == UINT32_MAX)
 * 4. Increment count
 * 5. Increment total_posts statistic
 * 6. Signal condition variable (wake one waiter)
 * 7. Unlock mutex
 *
 * OVERFLOW PROTECTION:
 * - If count == UINT32_MAX: Cannot increment (return error)
 * - WHY: Prevent wraparound to 0 (catastrophic bug)
 * - Highly unlikely in practice (4 billion resources)
 *
 * SIGNAL SEMANTICS:
 * - Wakes ONE waiting thread (not all)
 * - If no waiters: Signal is lost (count remains incremented)
 * - Fair scheduling: Waiters woken in FIFO order (typically)
 *
 * WHY SIGNAL (not broadcast):
 * - Only one waiter can consume the resource
 * - More efficient than waking all waiters
 * - Prevents thundering herd problem
 *
 * ERROR CASES:
 * - sem is NULL: Returns NIMCP_ERROR_INVALID_PARAM
 * - count == UINT32_MAX: Returns NIMCP_ERROR_SYSTEM (overflow)
 * - Mutex operations fail: Returns NIMCP_ERROR_SYSTEM
 *
 * COMPLEXITY: O(1)
 * THREAD SAFETY: Fully safe (mutex protected)
 *
 * @param sem Semaphore to post to
 * @return NIMCP_SUCCESS or NIMCP_ERROR_*
 */
nimcp_result_t nimcp_semaphore_post(nimcp_semaphore_t* sem);

/**
 * @brief Get current semaphore count (non-blocking read)
 *
 * WHAT: Read current resource count
 * WHY: Debugging, monitoring, health checks
 * HOW: Atomic read of count field
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
 * - Use only for monitoring/debugging, not synchronization
 * - Don't make decisions based on count (use wait/trywait instead)
 *
 * EXAMPLE (debugging):
 *   uint32_t count = nimcp_semaphore_get_count(&sem);
 *   printf("Resources available: %u\n", count);
 *
 * ERROR HANDLING:
 * - sem is NULL: Returns 0 (no error reporting)
 * - WHY: Function returns uint32_t (no error channel)
 *
 * COMPLEXITY: O(1)
 * THREAD SAFETY: Fully safe (mutex protected read)
 *
 * @param sem Semaphore to query
 * @return Current count (0 if sem is NULL)
 */
uint32_t nimcp_semaphore_get_count(const nimcp_semaphore_t* sem);

#ifdef __cplusplus
}
#endif

#endif  // NIMCP_SEMAPHORE_H
