//=============================================================================
// nimcp_barrier.h - Barrier Synchronization Primitive for NIMCP
//=============================================================================
// ARCHITECTURAL OVERVIEW:
// This module implements a cyclic barrier synchronization primitive that
// allows multiple threads to wait at a synchronization point until all threads
// have arrived. Once all threads arrive, they are all released simultaneously.
//
// KEY DESIGN: MONITOR PATTERN + CYCLIC BARRIER
// ==============================================
// WHY BARRIER:
// - Coordinate parallel phases: All threads complete phase N before any starts N+1
// - Simplify parallel algorithms: No manual thread coordination needed
// - Enable wave synchronization: Synchronous wavefront processing
// - Support iterative parallel computation: Multiple barrier cycles
//
// BARRIER PATTERN:
// ================
//   Thread 1         Thread 2         Thread 3
//      |                |                |
//   work()           work()           work()
//      |                |                |
//   barrier_wait() --→ barrier_wait() ←-- barrier_wait()
//      |                |                |
//     (all blocked until count reached)
//      |                |                |
//   ← ← ← ← (all released simultaneously) → → → →
//      |                |                |
//   work()           work()           work()
//      |                |                |
//
// WHY CYCLIC (REUSABLE):
// - Same barrier used for multiple synchronization points
// - No need to recreate barrier for each iteration
// - Efficient for iterative algorithms (simulation loops, parallel pipeline)
//
// IMPLEMENTATION STRATEGY:
// ========================
// INTERNAL STATE:
// - count: Total number of threads that must arrive
// - waiting: Current number of threads waiting at barrier
// - cycle: Current cycle number (incremented on release)
// - mutex: Protects barrier state
// - cond: Condition variable for blocking/waking threads
//
// WAIT ALGORITHM:
//
//   1. Lock mutex
//   2. Increment waiting count
//   3. If waiting < count:
//      a. Wait on condition variable (releases mutex)
//      b. When woken, re-acquire mutex and return
//   4. If waiting == count (last thread):
//      a. Reset waiting to 0
//      b. Increment cycle counter
//      c. Broadcast to wake all waiters
//      d. Return NIMCP_BARRIER_SERIAL_THREAD (distinguished return)
//   5. Unlock mutex
//
// WHY CONDITION VARIABLE (not busy-wait):
// - EFFICIENT: Threads block without consuming CPU
// - FAIR: OS scheduler ensures all threads eventually run
// - SCALABLE: Works with arbitrary thread count
// - PORTABLE: Standard POSIX mechanism
//
// WHY BROADCAST (not signal):
// - Must wake ALL waiting threads simultaneously
// - Signal only wakes one thread (insufficient)
// - Barrier semantics require collective release
//
// SERIAL THREAD PATTERN:
// ======================
// WHY ONE THREAD GETS SPECIAL RETURN:
// - Designates "master" thread for this barrier cycle
// - Master can perform single-threaded work (aggregate results, print stats)
// - Matches pthread_barrier semantics (POSIX standard)
//
// USAGE PATTERN:
//   nimcp_result_t result = nimcp_barrier_wait(&barrier);
//   if (result == NIMCP_BARRIER_SERIAL_THREAD) {
//       // Only this thread executes this
//       aggregate_results();
//   }
//   // All threads continue here
//
// CYCLIC BARRIER LIFECYCLE:
// ==========================
//
//   Cycle 0:                    Cycle 1:                    Cycle 2:
//   waiting=0 → 1 → 2 → 3      waiting=0 → 1 → 2 → 3      waiting=0 → ...
//                       ↓                          ↓
//                  broadcast()                broadcast()
//
// WHY CYCLE COUNTER:
// - Detect missed barriers (cycle mismatch indicates sync error)
// - Debug parallel algorithms (track barrier passage)
// - Statistics (total cycles completed)
//
// RESET FUNCTIONALITY:
// ====================
// WHY RESET:
// - Change thread count for new parallel phase
// - Recovery from error conditions
// - Reset statistics for new measurement period
//
// PRECONDITIONS:
// - No threads currently waiting (waiting == 0)
// - Otherwise: NIMCP_ERROR_BUSY
//
// STATISTICS TRACKING:
// ====================
// - total_waits: Total number of thread arrivals across all cycles
// - total_cycles: Number of complete barrier cycles
//
// WHY STATISTICS:
// - Performance analysis (barrier overhead measurement)
// - Debugging (verify expected call patterns)
// - Profiling (identify synchronization bottlenecks)
//
// THREAD SAFETY GUARANTEES:
// ==========================
// 1. ATOMIC STATE UPDATES
//    - Mutex protects all barrier state
//    - No race conditions on waiting/cycle counters
//
// 2. NO LOST WAKEUPS
//    - Condition variable semantics prevent lost broadcasts
//    - Atomic unlock+wait ensures no race between wait and signal
//
// 3. PROPER ORDERING
//    - Memory barrier on mutex lock/unlock ensures visibility
//    - All threads see consistent state after barrier
//
// 4. DEADLOCK-FREE
//    - No circular lock dependencies
//    - Timeout support (via platform cond_timedwait if needed)
//
// PERFORMANCE CHARACTERISTICS:
// =============================
// WAIT LATENCY:
// - Arrival: O(1) - increment counter, check condition
// - Last thread: O(n) - wake n-1 waiting threads
// - Context switch: ~1-10µs per thread wakeup
// - Total barrier latency: ~10-100µs for 2-8 threads
//
// MEMORY OVERHEAD:
// - Barrier structure: ~80 bytes
//   * mutex: ~40 bytes (nimcp_mutex_t)
//   * cond: ~48 bytes (pthread_cond_t)
//   * counters: 4×4 = 16 bytes (count, waiting, cycle, stats)
//
// SCALABILITY:
// - Linear wakeup cost: O(n) context switches
// - Lock contention: O(n) threads serialize on mutex
// - NOT suitable for >100 threads (consider tree barrier)
//
// COMPARISON WITH ALTERNATIVES:
// ==============================
// vs SEMAPHORE BARRIER:
// - Simpler implementation (one CV vs two semaphores)
// - Clearer semantics (monitor pattern)
// - Better debugging (state visible in single struct)
//
// vs ATOMIC SPIN BARRIER:
// - Better for long waits (no CPU spinning)
// - Worse for short waits (~1µs overhead vs ~50ns)
// - Better fairness (OS scheduling)
//
// vs pthread_barrier_t:
// - Portable (pthread_barrier_t optional in POSIX)
// - More features (statistics, cycle tracking, get_waiting)
// - Consistent with NIMCP threading API
//
// USE CASES IN NIMCP:
// ===================
// 1. PARALLEL CORTICAL COLUMN SIMULATION
//    - Synchronize computation waves across minicolumns
//    - Each timestep: compute → barrier → integrate → barrier
//
// 2. PARALLEL EVENT PROCESSING
//    - Worker threads process events in parallel
//    - Barrier between processing and state update phases
//
// 3. MULTI-THREADED TESTING
//    - Synchronize test threads for race condition testing
//    - Ensure all threads start simultaneously
//
// 4. PARALLEL LEARNING ALGORITHMS
//    - Barrier between gradient computation and weight update
//    - Synchronous parallel training epochs
//
// LIMITATIONS AND TRADE-OFFS:
// ============================
// 1. FIXED THREAD COUNT
//    TRADE-OFF: Simplicity vs dynamic resizing
//    MITIGATION: Use reset() to change count when no threads waiting
//
// 2. SINGLE SYNCHRONIZATION POINT
//    TRADE-OFF: Simple API vs complex patterns (tree, butterfly)
//    MITIGATION: Use multiple barriers for multi-level sync
//
// 3. O(n) WAKEUP COST
//    TRADE-OFF: Portability vs scalability
//    MITIGATION: For >100 threads, consider hierarchical barrier
//
// WHY THESE TRADE-OFFS:
// - Fixed count: Simplifies implementation, matches common usage
// - Single point: Sufficient for most parallel algorithms
// - O(n) wakeup: Acceptable for target thread counts (2-32)
//
// ERROR HANDLING:
// ===============
// - NIMCP_ERROR_INVALID_PARAM: NULL barrier, count=0
// - NIMCP_ERROR_SYSTEM: pthread mutex/cond operation failed
// - NIMCP_ERROR_BUSY: Reset called while threads waiting
// - NIMCP_SUCCESS: Normal completion (non-serial thread)
// - NIMCP_BARRIER_SERIAL_THREAD: Serial thread (master)
//
//=============================================================================

/**
 * @file nimcp_barrier.h
 * @brief Barrier synchronization primitive for coordinating parallel threads
 *
 * WHAT: Cyclic barrier allowing N threads to synchronize at a common point
 * WHY: Enable parallel algorithms with phase synchronization
 * HOW: Monitor pattern (mutex + condition variable) with cycle counter
 */

#ifndef NIMCP_BARRIER_H
#define NIMCP_BARRIER_H

#include <stdint.h>
#include <stdbool.h>
#include "utils/thread/nimcp_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/**
 * WHAT: Special return code for serial thread (one thread per cycle)
 * WHY: Designates "master" thread for single-threaded work
 * HOW: Returned to last thread that completes barrier
 *
 * USAGE:
 *   if (nimcp_barrier_wait(&barrier) == NIMCP_BARRIER_SERIAL_THREAD) {
 *       // Only serial thread executes this
 *       aggregate_results();
 *   }
 */
#define NIMCP_BARRIER_SERIAL_THREAD 1

//=============================================================================
// Type Definitions
//=============================================================================

/**
 * @brief Barrier synchronization structure (opaque)
 *
 * WHAT: Encapsulates barrier state and synchronization primitives
 * WHY: Hide implementation details, prevent external modification
 *
 * INTERNAL FIELDS (in .c file):
 * - count: Number of threads required at barrier
 * - waiting: Current number of waiting threads
 * - cycle: Current cycle number (incremented each release)
 * - mutex: Protects barrier state
 * - cond: Condition variable for blocking/waking
 * - total_waits: Cumulative wait count (statistics)
 * - total_cycles: Cumulative cycle count (statistics)
 */
typedef struct nimcp_barrier nimcp_barrier_t;

//=============================================================================
// Barrier Operations
//=============================================================================

/**
 * @brief Initialize barrier for specified thread count
 *
 * WHY NEEDED:
 * - Allocate barrier structure
 * - Initialize synchronization primitives (mutex, cond)
 * - Set thread count for barrier
 *
 * ALGORITHM:
 * 1. Validate parameters (barrier != NULL, count > 0)
 * 2. Allocate barrier structure
 * 3. Initialize mutex using platform abstraction
 * 4. Initialize condition variable using platform abstraction
 * 5. Set count, zero waiting/cycle/stats
 * 6. Return success or error
 *
 * PRECONDITIONS:
 * - barrier != NULL
 * - count > 0 (at least one thread)
 *
 * POSTCONDITIONS:
 * - Barrier ready for use
 * - waiting = 0, cycle = 0
 * - Statistics initialized
 *
 * ERROR CASES:
 * - NIMCP_ERROR_INVALID_PARAM: NULL barrier or count=0
 * - NIMCP_ERROR_MEMORY: Allocation failed
 * - NIMCP_ERROR_SYSTEM: Mutex/cond initialization failed
 *
 * TYPICAL USAGE:
 *   nimcp_barrier_t* barrier;
 *   nimcp_barrier_init(&barrier, 4);  // 4 threads
 *
 * COMPLEXITY: O(1)
 * THREAD SAFETY: Fully safe (creates independent barrier)
 *
 * @param barrier Output parameter for barrier handle
 * @param count Number of threads that must arrive at barrier
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_barrier_init(nimcp_barrier_t** barrier, uint32_t count);

/**
 * @brief Destroy barrier and free resources
 *
 * WHY NEEDED:
 * - Free barrier structure memory
 * - Destroy synchronization primitives
 * - Prevent memory leaks
 *
 * ALGORITHM:
 * 1. Validate parameters (barrier != NULL, *barrier != NULL)
 * 2. Destroy condition variable
 * 3. Destroy mutex
 * 4. Free barrier structure
 * 5. NULL out barrier pointer
 *
 * PRECONDITIONS:
 * - No threads currently waiting (waiting == 0)
 * - Barrier previously initialized
 *
 * POSTCONDITIONS:
 * - All resources freed
 * - *barrier = NULL
 *
 * ERROR CASES:
 * - NIMCP_ERROR_INVALID_PARAM: NULL barrier or *barrier
 * - NIMCP_ERROR_SYSTEM: Mutex/cond destruction failed
 *
 * TYPICAL USAGE:
 *   nimcp_barrier_destroy(&barrier);
 *   // barrier is now NULL
 *
 * COMPLEXITY: O(1)
 * THREAD SAFETY: NOT safe if threads still using barrier
 *
 * @param barrier Pointer to barrier handle
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_barrier_destroy(nimcp_barrier_t** barrier);

/**
 * @brief Wait at barrier until all threads arrive
 *
 * WHY NEEDED:
 * - Synchronize threads at common point
 * - Block until all threads ready to proceed
 * - One thread designated as serial thread
 *
 * ALGORITHM:
 * 1. Validate barrier != NULL
 * 2. Lock barrier mutex
 * 3. Increment waiting counter
 * 4. Increment total_waits statistic
 * 5. If waiting < count:
 *    a. Wait on condition variable (blocks, releases mutex)
 *    b. When signaled, re-acquire mutex
 *    c. Unlock mutex and return NIMCP_SUCCESS
 * 6. If waiting == count (last thread):
 *    a. Reset waiting to 0 (ready for next cycle)
 *    b. Increment cycle counter
 *    c. Increment total_cycles statistic
 *    d. Broadcast to wake all waiting threads
 *    e. Unlock mutex and return NIMCP_BARRIER_SERIAL_THREAD
 * 7. Handle errors appropriately
 *
 * RETURN VALUES:
 * - NIMCP_SUCCESS: Normal thread (not serial)
 * - NIMCP_BARRIER_SERIAL_THREAD: Serial thread (one per cycle)
 * - NIMCP_ERROR_*: Error occurred
 *
 * WHY SERIAL THREAD:
 * - Matches pthread_barrier semantics
 * - Enables single-threaded aggregation/reporting
 * - Deterministic (always last arriving thread)
 *
 * TYPICAL USAGE (simple):
 *   nimcp_barrier_wait(&barrier);
 *   // All threads synchronized here
 *
 * TYPICAL USAGE (with serial thread):
 *   if (nimcp_barrier_wait(&barrier) == NIMCP_BARRIER_SERIAL_THREAD) {
 *       printf("Iteration %d complete\n", iteration);
 *   }
 *
 * COMPLEXITY:
 * - Non-serial: O(1) + block time
 * - Serial: O(n) to wake n-1 threads
 *
 * THREAD SAFETY: Fully safe (intended for concurrent use)
 *
 * @param barrier Barrier to wait on
 * @return NIMCP_SUCCESS, NIMCP_BARRIER_SERIAL_THREAD, or error code
 */
nimcp_result_t nimcp_barrier_wait(nimcp_barrier_t* barrier);

/**
 * @brief Get number of threads currently waiting at barrier
 *
 * WHY NEEDED:
 * - Debugging: Check if all threads arrived
 * - Monitoring: Track barrier utilization
 * - Testing: Verify expected wait patterns
 *
 * ALGORITHM:
 * 1. Validate barrier != NULL
 * 2. Lock mutex
 * 3. Read waiting counter
 * 4. Unlock mutex
 * 5. Return counter value
 *
 * SEMANTICS:
 * - Returns snapshot of waiting count
 * - Value may change immediately after return (inherent race)
 * - Safe to call anytime (read-only, no state change)
 *
 * TYPICAL USAGE:
 *   uint32_t waiting = nimcp_barrier_get_waiting(&barrier);
 *   printf("%u threads at barrier\n", waiting);
 *
 * COMPLEXITY: O(1)
 * THREAD SAFETY: Fully safe (read-only with lock)
 *
 * @param barrier Barrier to query
 * @return Number of currently waiting threads, or 0 on error
 */
uint32_t nimcp_barrier_get_waiting(const nimcp_barrier_t* barrier);

/**
 * @brief Reset barrier for reuse with potentially different count
 *
 * WHY NEEDED:
 * - Change thread count for new parallel phase
 * - Reset statistics for new measurement period
 * - Recovery from error conditions
 *
 * ALGORITHM:
 * 1. Validate parameters (barrier != NULL, count > 0)
 * 2. Lock mutex
 * 3. Check waiting == 0 (no threads at barrier)
 * 4. If waiting > 0: Error NIMCP_ERROR_BUSY
 * 5. Set new count
 * 6. Reset cycle to 0 (optional: preserve cycle for continuity)
 * 7. Reset statistics to 0 (optional: preserve for cumulative stats)
 * 8. Unlock mutex
 *
 * PRECONDITIONS:
 * - No threads currently waiting (waiting == 0)
 * - Barrier previously initialized
 *
 * ERROR CASES:
 * - NIMCP_ERROR_INVALID_PARAM: NULL barrier, count=0
 * - NIMCP_ERROR_BUSY: Threads currently waiting
 *
 * TYPICAL USAGE:
 *   // After all threads finished previous phase
 *   nimcp_barrier_reset(&barrier, 8);  // Change to 8 threads
 *
 * COMPLEXITY: O(1)
 * THREAD SAFETY: Safe if no threads waiting (enforced)
 *
 * @param barrier Barrier to reset
 * @param count New thread count
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_barrier_reset(nimcp_barrier_t* barrier, uint32_t count);

/**
 * @brief Get total number of wait operations across all cycles
 *
 * WHY NEEDED:
 * - Performance analysis: Measure barrier overhead
 * - Debugging: Verify expected call patterns
 * - Statistics: Track barrier utilization
 *
 * SEMANTICS:
 * - Returns cumulative wait count since init/reset
 * - total_waits = total_cycles × count (if all cycles complete)
 *
 * COMPLEXITY: O(1)
 * THREAD SAFETY: Fully safe (read-only with lock)
 *
 * @param barrier Barrier to query
 * @return Total wait count, or 0 on error
 */
uint64_t nimcp_barrier_get_total_waits(const nimcp_barrier_t* barrier);

/**
 * @brief Get total number of complete barrier cycles
 *
 * WHY NEEDED:
 * - Track iteration count in parallel loops
 * - Performance measurement: Cycles per second
 * - Debugging: Verify progress
 *
 * SEMANTICS:
 * - Incremented each time all threads released
 * - Resets to 0 on barrier reset (configurable)
 *
 * COMPLEXITY: O(1)
 * THREAD SAFETY: Fully safe (read-only with lock)
 *
 * @param barrier Barrier to query
 * @return Total cycle count, or 0 on error
 */
uint64_t nimcp_barrier_get_total_cycles(const nimcp_barrier_t* barrier);

#ifdef __cplusplus
}
#endif

#endif  // NIMCP_BARRIER_H
