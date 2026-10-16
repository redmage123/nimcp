#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_thread_pool.c - Fixed-Size Thread Pool for Parallel Task Execution
//=============================================================================
// ARCHITECTURAL OVERVIEW:
// This module implements a fixed-size thread pool with a bounded task queue,
// designed for efficient parallel processing of independent tasks. It follows
// the Producer-Consumer pattern where submitting threads produce tasks and
// worker threads consume them. The pool provides work distribution, load
// balancing, and lifecycle management for a set of worker threads.
//
// WHY THREAD POOL vs SPAWNING THREADS:
// ====================================
// Thread pools solve critical performance and resource management problems:
//
// 1. THREAD CREATION OVERHEAD
//    - Creating thread: ~100μs-1ms (syscall, kernel structures, stack allocation)
//    - Destroying thread: ~50μs-500μs (cleanup, stack deallocation)
//    - For 10,000 short tasks: Spawning = ~1-10 seconds, Pool = ~milliseconds
//
//    Example (1ms tasks):
//      Spawn per-task:  10,000 × (1ms creation + 1ms task + 0.5ms cleanup) = 25s
//      Thread pool:     10,000 × 1ms / 4 threads = 2.5s
//      Speedup: 10x
//
// 2. RESOURCE EXHAUSTION
//    - Each thread: 2-8MB stack + kernel resources
//    - 10,000 threads: 20-80GB just for stacks!
//    - Context switching overhead grows O(n²) with thread count
//    - Thread pool: Fixed resources (e.g., 4 threads = 8-32MB)
//
// 3. PREDICTABLE PERFORMANCE
//    - Fixed thread count matches CPU cores (no oversubscription)
//    - No context-switching thrash
//    - Cache locality: Same threads reused, TLB warm
//
// 4. SIMPLIFIED LIFECYCLE MANAGEMENT
//    - Threads created once, reused many times
//    - Graceful shutdown: Wait for queue to drain
//    - No need to track individual thread handles
//
// WHEN NOT TO USE THREAD POOL:
// - Blocking I/O tasks (use async I/O or dedicated threads)
// - Long-running tasks (defeats reuse benefit)
// - Tasks with varying priorities (need priority queue)
// - Very few tasks (overhead not worth it)
//
// KEY DESIGN: PRODUCER-CONSUMER PATTERN
// ======================================
// WHY PRODUCER-CONSUMER:
// - Decouples task submission from execution
// - Natural flow control via bounded queue
// - Synchronization via condition variables
// - Well-understood concurrency pattern
//
// ARCHITECTURE DIAGRAM:
//
//   ┌─────────────────────────────────────────────────────────────┐
//   │                    THREAD POOL                              │
//   │                                                             │
//   │  ┌──────────────┐      ┌─────────────────┐                 │
//   │  │  Submitters  │      │   Circular      │                 │
//   │  │  (Producers) │─────▶│   Task Queue    │                 │
//   │  └──────────────┘      │   (Bounded)     │                 │
//   │        │               └────────┬────────┘                 │
//   │        │                        │                          │
//   │        │                        │ task_available           │
//   │        │                        ▼                          │
//   │        │               ┌─────────────────┐                 │
//   │        │               │  Worker Threads │                 │
//   │        │               │  (Consumers)    │                 │
//   │        │               │                 │                 │
//   │        │               │  T1  T2  T3  T4 │                 │
//   │        │               └─────────────────┘                 │
//   │        │                        │                          │
//   │        │                        │ task_complete            │
//   │        ▼                        ▼                          │
//   │  ┌─────────────────────────────────────┐                  │
//   │  │    Condition Variables (Sync)       │                  │
//   │  │  - task_available: Signal workers   │                  │
//   │  │  - task_complete: Signal submitters │                  │
//   │  └─────────────────────────────────────┘                  │
//   │                                                            │
//   │  Protected by: Global Mutex                               │
//   └────────────────────────────────────────────────────────────┘
//
// CIRCULAR QUEUE DESIGN:
// ======================
// WHY CIRCULAR QUEUE:
// - Fixed-size allocation (no malloc during operation)
// - O(1) enqueue/dequeue operations
// - Natural wrapping with modulo arithmetic
// - Cache-friendly (array layout)
//
// QUEUE STRUCTURE:
//   ┌───┬───┬───┬───┬───┬───┬───┬───┐
//   │ T1│ T2│   │   │   │   │ T5│ T6│  (T = Task)
//   └───┴───┴───┴───┴───┴───┴───┴───┘
//     ▲                       ▲
//     head (next dequeue)     tail (next enqueue)
//
// STATE TRACKING:
// - head: Index of next task to dequeue
// - tail: Index of next slot to enqueue
// - count: Number of tasks in queue
//
// WHY TRACK COUNT:
// - Disambiguates empty vs full: Both have head==tail
// - Faster than computing (tail-head) % size
// - Enables wait conditions: count==0 (empty), count==MAX (full)
//
// ALGORITHM INVARIANTS:
// - 0 ≤ count ≤ MAX_QUEUE
// - head = (head + 1) % MAX_QUEUE on dequeue
// - tail = (tail + 1) % MAX_QUEUE on enqueue
// - Queue empty when count == 0
// - Queue full when count == MAX_QUEUE
//
// ALTERNATIVE APPROACHES REJECTED:
// - Linked list: Requires malloc/free per task (slow, fragmentation)
// - Unbounded queue: Memory exhaustion risk
// - Lock-free queue: Complex, not needed for debug/batch workloads
//
// WORKER THREAD LIFECYCLE:
// =========================
// STATE MACHINE:
//
//   ┌────────────┐
//   │   CREATED  │ (nimcp_pool_create)
//   └─────┬──────┘
//         │
//         ▼
//   ┌────────────┐
//   │   WAITING  │◀──────┐
//   │  (blocked) │       │
//   └─────┬──────┘       │
//         │ task_available
//         ▼              │
//   ┌────────────┐       │
//   │  EXECUTING │       │
//   │   (active) │       │
//   └─────┬──────┘       │
//         │ task done    │
//         └──────────────┘
//         │ shutdown=true
//         ▼
//   ┌────────────┐
//   │    EXIT    │ (nimcp_pool_destroy)
//   └────────────┘
//
// WORKER ALGORITHM:
// 1. Acquire lock
// 2. Wait while (queue_count==0 && !shutdown)
// 3. Check shutdown: Exit if shutdown && queue_count==0
// 4. Dequeue task, increment active_threads
// 5. Release lock
// 6. Execute task (LOCK-FREE for parallelism!)
// 7. Acquire lock, decrement active_threads, signal completion
// 8. Goto 1
//
// WHY EXECUTE OUTSIDE LOCK:
// - Enables true parallelism (multiple tasks run simultaneously)
// - Lock only protects queue/state, not task execution
// - Critical for performance: Otherwise serial execution!
//
// TASK SUBMISSION STRATEGY:
// ==========================
// BLOCKING SUBMISSION:
// - If queue full: Block until space available
// - Backpressure: Slows producers to consumer rate
// - Alternative: Drop tasks (unacceptable for batch processing)
// - Alternative: Unbounded queue (memory exhaustion risk)
//
// SUBMISSION ALGORITHM:
// 1. Acquire lock
// 2. Check shutdown: Return error if true
// 3. Wait while (queue_count >= MAX_QUEUE && !shutdown)
// 4. Enqueue task, increment queue_count
// 5. Signal one worker (task_available)
// 6. Release lock
//
// WHY SIGNAL vs BROADCAST:
// - signal: Wake one worker (enough for one task)
// - broadcast: Wake all workers (waste of wakeups)
// - Efficiency: Only wake workers as needed
//
// LOAD BALANCING:
// ===============
// CURRENT APPROACH: FIFO Work Queue
// - All workers share single queue
// - First-available worker takes next task
// - Simple, fair, good cache behavior
//
// LOAD DISTRIBUTION:
// - Tasks naturally distribute across idle workers
// - No manual assignment needed
// - Good utilization: No worker sits idle while others work
//
// LIMITATIONS:
// - No work stealing (single queue, no per-worker queues)
// - No task priority (FIFO only)
// - No CPU affinity (OS scheduler decides)
//
// ALTERNATIVE APPROACHES:
// - Work stealing: Per-worker queues + steal from others
//   * Pros: Better cache locality, less contention
//   * Cons: Much more complex, diminishing returns for small pools
// - Priority queue: Tasks have priorities
//   * Pros: Critical tasks first
//   * Cons: Starvation risk, more complex
//
// WHY SINGLE QUEUE IS SUFFICIENT:
// - NIMCP workloads: Batch processing of similar tasks
// - Pool size: 4-16 threads (not 100s)
// - Task duration: Milliseconds (contention negligible)
// - Simplicity: Easy to understand and debug
//
// SHUTDOWN SEQUENCE:
// ==================
// GRACEFUL SHUTDOWN:
// 1. Set shutdown flag
// 2. Broadcast to all workers (wake up)
// 3. Workers check: If shutdown && queue_count==0, exit
// 4. Workers drain queue first, then exit
// 5. Join all worker threads (wait for exit)
// 6. Cleanup synchronization primitives
//
// WHY DRAIN QUEUE:
// - Complete submitted tasks (no dropped work)
// - Predictable behavior
// - Alternative: Immediate shutdown (lose queued tasks)
//
// SHUTDOWN INVARIANTS:
// - Once shutdown=true, no new tasks accepted
// - All queued tasks execute before exit
// - Join waits for all workers to finish
//
// BACKPRESSURE HANDLING:
// ======================
// BOUNDED QUEUE AS NATURAL BACKPRESSURE:
// - Queue full → Submitter blocks
// - Slows submission to consumption rate
// - Prevents memory exhaustion
// - Self-regulating system
//
// EXAMPLE FLOW:
//   Producer rate: 1000 tasks/sec
//   Consumer rate: 500 tasks/sec (4 workers × 125 tasks/sec)
//   Queue size: 1024 tasks
//
//   Time 0s: Queue fills in 1.024 seconds
//   Time 1s: Producer blocks, rate limited to 500 tasks/sec
//   Steady state: Queue stays near full, producer blocked 50% of time
//
// WHY THIS IS GOOD:
// - Prevents unbounded queue growth
// - Provides feedback to submitter (slow down!)
// - Graceful degradation under load
//
// ALTERNATIVE: UNBOUNDED QUEUE
// - Pro: Never blocks submitter
// - Con: Memory exhaustion under sustained overload
// - Con: No backpressure signal
//
// THREAD SAFETY GUARANTEES:
// ==========================
// SYNCHRONIZATION PRIMITIVES:
// - lock: Protects all shared state (queue, counters, flags)
// - task_available: Condition variable for worker wakeup
// - task_complete: Condition variable for completion signaling
//
// CRITICAL SECTIONS:
// 1. Queue manipulation (enqueue/dequeue)
// 2. Counter updates (queue_count, active_threads)
// 3. Shutdown flag access
// 4. Statistics updates
//
// LOCK-FREE REGIONS:
// - Task execution (outside lock for parallelism)
// - Thread creation/join (inherently thread-safe)
//
// DEADLOCK AVOIDANCE:
// - Single global lock (no lock ordering issues)
// - Always acquire/release in same function
// - Condition variables automatically release/reacquire lock
//
// RACE CONDITION PREVENTION:
// - All shared state accessed under lock
// - Condition variables: Atomic wait + unlock
// - Shutdown checked under lock before task submission
//
// WHY GLOBAL MUTEX vs PER-QUEUE/PER-WORKER LOCKS:
// - Simplicity: One lock, no ordering issues
// - Correctness: No race conditions
// - Performance: Lock contention minimal (tasks execute outside lock)
// - Trade-off: Acceptable for small pools (4-16 threads)
//
// PERFORMANCE CHARACTERISTICS:
// =============================
// TIME COMPLEXITY:
// - pool_create: O(n) where n = num_threads (thread creation)
// - pool_destroy: O(n) where n = num_threads (thread join)
// - pool_submit: O(1) amortized (may block if queue full)
// - pool_wait: O(m) where m = tasks to complete
// - pool_pending: O(1)
// - pool_active: O(1)
//
// SPACE COMPLEXITY:
// - Fixed: num_threads × stack_size (typically 4 × 2MB = 8MB)
// - Fixed: MAX_QUEUE × sizeof(pool_task_t) (1024 × 16 bytes = 16KB)
// - Fixed: Synchronization primitives (~100 bytes)
// - Total: ~8MB + 16KB + overhead
//
// THROUGHPUT:
// - Theoretical: num_threads × task_rate
// - Example: 4 threads × 1000 tasks/sec = 4000 tasks/sec
// - Bottleneck: Task execution time, not pool overhead
//
// LATENCY:
// - Task-to-execution: ~μs (if worker idle) to ~ms (if queue full)
// - Lock acquisition: ~50-100ns (uncontended)
// - Condition variable signal: ~μs
//
// SCALABILITY:
// - Linear speedup up to num_cores (for CPU-bound tasks)
// - Diminishing returns beyond num_cores (context switching)
// - Queue contention: Minimal for task_time >> lock_time
//
// OVERHEAD:
// - Per-task: ~100-200ns (enqueue + dequeue + sync)
// - Negligible for tasks > 10μs
// - Significant for tasks < 1μs (use batch processing)
//
// DESIGN PATTERNS:
// ================
// 1. PRODUCER-CONSUMER: Core pattern (submitters produce, workers consume)
// 2. OBJECT POOL: Thread pool itself is object pool of workers
// 3. COMMAND PATTERN: Tasks encapsulate function + argument
// 4. ACTIVE OBJECT: Each worker is active object processing commands
// 5. BARRIER: pool_wait() is barrier synchronization
// 6. SINGLETON: One pool instance manages lifecycle
//
// SOLID PRINCIPLES:
// =================
// - Single Responsibility: Pool manages threads, queue, and lifecycle
// - Open/Closed: Can extend with priority queue without modifying core
// - Liskov Substitution: Pool_task_t can be any function pointer
// - Interface Segregation: Minimal API (create, destroy, submit, wait)
// - Dependency Inversion: Depends on abstract nimcp_thread_t interface
//
// USE CASES IN NIMCP:
// ===================
// 1. PARALLEL FILE PROCESSING
//    - Submit one task per file
//    - Workers process files concurrently
//    - Wait for all files before proceeding
//
// 2. BATCH VERIFICATION
//    - Submit verification tasks for multiple items
//    - Parallel verification across items
//    - Aggregate results after completion
//
// 3. PARALLEL COMPRESSION
//    - Submit compression tasks for chunks
//    - Workers compress in parallel
//    - Combine compressed chunks
//
// 4. PARALLEL TESTING
//    - Submit test cases as tasks
//    - Parallel test execution
//    - Wait for all tests to finish
//
// USAGE PATTERN:
//
//   // 1. Create pool at startup (match CPU cores)
//   size_t num_cores = 4;  // Or get from system
//   nimcp_thread_pool_t* pool = nimcp_pool_create(num_cores);
//
//   // 2. Submit tasks (can be from multiple threads)
//   for (int i = 0; i < 1000; i++) {
//       nimcp_pool_submit(pool, process_file, &files[i]);
//   }
//
//   // 3. Wait for completion (barrier)
//   nimcp_pool_wait(pool);
//
//   // 4. Check status (optional)
//   printf("Pending: %zu, Active: %zu\n",
//          nimcp_pool_pending(pool),
//          nimcp_pool_active(pool));
//
//   // 5. Cleanup (graceful shutdown)
//   nimcp_pool_destroy(pool);
//
// LIMITATIONS AND TRADE-OFFS:
// ============================
// 1. FIXED SIZE
//    - Pro: Predictable resource usage
//    - Con: Can't adapt to load spikes
//    - Alternative: Dynamic resizing (much more complex)
//
// 2. BOUNDED QUEUE
//    - Pro: Prevents memory exhaustion
//    - Con: Blocks submitter when full
//    - Alternative: Unbounded queue (memory risk)
//
// 3. FIFO ORDERING
//    - Pro: Simple, fair
//    - Con: No priorities
//    - Alternative: Priority queue (more complex)
//
// 4. NO WORK STEALING
//    - Pro: Simple implementation
//    - Con: Potential load imbalance for heterogeneous tasks
//    - Alternative: Per-worker queues + stealing (complex)
//
// 5. BLOCKING SUBMISSION
//    - Pro: Natural backpressure
//    - Con: Can deadlock if pool waits on itself
//    - Mitigation: Never submit from task function!
//
// 6. NO TASK CANCELLATION
//    - Pro: Simple lifecycle
//    - Con: Can't cancel queued tasks
//    - Alternative: Task IDs + cancellation API (complex)
//
// WHY THESE TRADE-OFFS:
// - Target workload: Batch processing of independent, similar tasks
// - Simplicity: Easy to understand, debug, and maintain
// - Reliability: No complex corner cases
// - Performance: Good enough for NIMCP use cases
// - Alternatives: More complex patterns for different workloads
//
// DEBUGGING TIPS:
// ===============
// - Deadlock: Check if tasks submit to same pool (circular dependency)
// - Poor utilization: Check task_time vs overhead (~10μs minimum)
// - Queue overflow: Increase MAX_QUEUE or slow submission rate
// - Memory leaks: Ensure pool_destroy called, check task cleanup
// - Crashes: Ensure tasks don't modify shared state without locking
//
//=============================================================================

/**
 * @file nimcp_thread_pool.c
 * @brief Thread pool implementation with bounded queue
 *
 * WHAT: Fixed-size thread pool for parallel task execution
 * WHY: Amortize thread creation cost, limit resources, enable parallelism
 * HOW: Producer-consumer pattern with circular queue and condition variables
 */

#include "utils/thread/nimcp_thread_pool.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "utils/validation/nimcp_common.h"
#include "api/nimcp_api_exception.h"
#include <stdlib.h>
#include <string.h>
#include "utils/memory/nimcp_memory.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(thread_pool)

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * WHAT: Individual task in the queue
 * WHY: Encapsulates work unit for execution
 * HOW: Command pattern (function pointer + argument)
 *
 * STRUCTURE:
 * - func: Task function to execute
 * - arg: User-provided argument passed to func
 *
 * WHY SEPARATE FUNC AND ARG:
 * - Generic: Works with any function signature
 * - Type-safe: Caller casts void* to appropriate type
 * - Flexible: Same mechanism for all task types
 *
 * SIZE: 16 bytes (2 pointers on 64-bit)
 */
typedef struct {
    nimcp_task_fn func;
    void* arg;
} pool_task_t;

/**
 * WHAT: Thread pool state and configuration
 * WHY: Encapsulates all pool data for thread safety
 * HOW: Opaque struct with internal linkage
 *
 * STRUCTURE:
 * - threads: Array of worker thread handles
 * - num_threads: Number of workers (fixed at creation)
 * - queue: Circular buffer of tasks
 * - queue_head: Index of next task to dequeue
 * - queue_tail: Index of next slot to enqueue
 * - queue_count: Number of tasks currently in queue
 * - lock: Mutex protecting all shared state
 * - task_available: Condition variable for worker wakeup
 * - task_complete: Condition variable for completion signaling
 * - active_threads: Number of workers currently executing tasks
 * - shutdown: Flag indicating pool is shutting down
 *
 * WHY FIXED-SIZE ARRAYS:
 * - No dynamic allocation during operation (faster, safer)
 * - Bounded resource usage (predictable)
 * - Cache-friendly (contiguous layout)
 *
 * WHY TWO CONDITION VARIABLES:
 * - task_available: Workers wait for work
 * - task_complete: Submitters wait for completion/space
 * - Separation avoids spurious wakeups
 *
 * INVARIANTS:
 * - 0 ≤ queue_count ≤ MAX_QUEUE
 * - 0 ≤ active_threads ≤ num_threads
 * - 0 ≤ queue_head < MAX_QUEUE
 * - 0 ≤ queue_tail < MAX_QUEUE
 * - shutdown=true → no new submissions accepted
 *
 * SIZE: ~1KB (threads array) + 16KB (queue array) + overhead
 */
struct nimcp_thread_pool {
    /* Worker threads */
    nimcp_thread_t threads[NIMCP_POOL_MAX_THREADS];
    size_t num_threads;

    /* Circular task queue */
    pool_task_t queue[NIMCP_POOL_MAX_QUEUE];
    size_t queue_head;  /* Next task to dequeue */
    size_t queue_tail;  /* Next slot to enqueue */
    size_t queue_count; /* Number of tasks in queue */

    /* Synchronization primitives */
    nimcp_mutex_t lock;
    nimcp_cond_t task_available; /* Signal when task added */
    nimcp_cond_t task_complete;  /* Signal when task done */

    /* State tracking */
    size_t active_threads; /* Threads currently executing tasks */
    bool shutdown;         /* Pool is shutting down */
};

//=============================================================================
// Worker Thread
//=============================================================================

/**
 * @brief Worker thread main loop
 *
 * WHY WORKER LOOP:
 * - Reuses thread for multiple tasks (amortizes creation cost)
 * - Waits for work instead of polling (efficient)
 * - Checks shutdown to exit gracefully
 *
 * ALGORITHM:
 * 1. Acquire lock
 * 2. Wait while (no tasks && not shutdown)
 * 3. If shutdown && queue empty: Exit loop
 * 4. Dequeue task, mark thread active
 * 5. Release lock
 * 6. Execute task (CRITICAL: outside lock for parallelism!)
 * 7. Acquire lock, mark thread inactive, signal completion
 * 8. Release lock, goto 1
 *
 * WHY EXECUTE OUTSIDE LOCK:
 * - Enables true parallelism (multiple tasks run simultaneously)
 * - Lock only protects queue/state, not execution
 * - Critical for performance: Otherwise serial execution!
 *
 * WHY WAIT ON CONDITION VARIABLE:
 * - Efficient: Thread sleeps, no CPU usage
 * - Atomic: Releases lock while waiting, reacquires on wakeup
 * - Reliable: No race between check and wait
 *
 * SHUTDOWN PROTOCOL:
 * - Check (shutdown && queue_count==0) after wakeup
 * - Drains queue before exiting (completes submitted tasks)
 * - Ensures no tasks dropped during shutdown
 *
 * STATE TRANSITIONS:
 * WAITING → EXECUTING → WAITING → ... → EXIT
 *
 * WHY INCREMENT/DECREMENT active_threads:
 * - Enables pool_wait() to detect completion
 * - Enables pool_active() to report status
 * - Must be under lock (shared state)
 *
 * WHY SIGNAL task_complete:
 * - Wakes pool_wait() when task finishes
 * - Wakes pool_submit() when queue has space
 * - Enables coordination with submitters
 *
 * COMPLEXITY: O(1) per task (dequeue + execute + signal)
 * THREAD SAFETY: Lock-protected except task execution
 *
 * @param arg Thread pool pointer (cast from void*)
 * @return NULL (unused)
 */
static void* worker_thread(void* arg)
{
    nimcp_thread_pool_t* pool = (nimcp_thread_pool_t*) arg;
    if (!pool) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "worker_thread: pool is NULL");
        return NULL;  // Defensive null check
    }

    while (1) {
        if (nimcp_mutex_lock(&pool->lock) != NIMCP_SUCCESS) {
            // Mutex lock failed - exit worker thread
            LOG_ERROR("THREAD_POOL", "Worker thread failed to acquire mutex");
            break;
        }

        // WHY LOOP: Condition variables can have spurious wakeups
        // Must re-check condition after each wakeup
        while (pool->queue_count == 0 && !pool->shutdown) {
            nimcp_cond_wait(&pool->task_available, &pool->lock);
        }

        // WHY CHECK BOTH FLAGS:
        // shutdown=true but queue_count>0 → Drain queue first
        // shutdown=true and queue_count==0 → Exit now
        if (pool->shutdown && pool->queue_count == 0) {
            nimcp_mutex_unlock(&pool->lock);
            break;
        }

        // Dequeue task from circular buffer
        // WHY CIRCULAR: O(1) dequeue with wrap-around
        pool_task_t task = pool->queue[pool->queue_head];
        pool->queue_head = (pool->queue_head + 1) % NIMCP_POOL_MAX_QUEUE;
        pool->queue_count--;
        pool->active_threads++;

        nimcp_mutex_unlock(&pool->lock);

        // CRITICAL: Execute task outside lock for parallelism!
        // WHY: Allows other workers to dequeue and execute simultaneously
        // Multiple workers can be in this section at once
        if (task.func) {
            task.func(task.arg);
        }

        // Mark task completion
        // WHY LOCK: active_threads is shared state
        // WHY SIGNAL: Wake pool_wait() and pool_submit() waiters
        nimcp_mutex_lock(&pool->lock);
        pool->active_threads--;
        nimcp_cond_signal(&pool->task_complete);
        nimcp_mutex_unlock(&pool->lock);
    }

    return NULL;
}

//=============================================================================
// Public API
//=============================================================================

/**
 * @brief Create a thread pool
 *
 * WHY CREATE UPFRONT:
 * - Amortize thread creation cost across many tasks
 * - Fixed resource allocation (predictable)
 * - Workers ready immediately when tasks arrive
 *
 * ALGORITHM:
 * 1. Validate parameters (num_threads in valid range)
 * 2. Allocate pool structure
 * 3. Initialize fields (counters, indices, flags)
 * 4. Initialize synchronization primitives (mutex, condvars)
 * 5. Create worker threads
 * 6. Handle failures: Cleanup and return NULL
 *
 * WHY VALIDATE num_threads:
 * - 0 threads: Useless, tasks never execute
 * - >MAX_THREADS: Array overflow, excessive resources
 * - Typical: Match CPU cores (4-16)
 *
 * INITIALIZATION ORDER:
 * 1. Allocate memory (calloc zeros everything)
 * 2. Set counters and flags
 * 3. Initialize mutex (needed for condvars)
 * 4. Initialize condition variables
 * 5. Create threads (start worker loops)
 *
 * WHY THIS ORDER:
 * - Primitives must exist before threads start
 * - Threads may immediately acquire lock
 * - Ensures no race conditions during initialization
 *
 * ERROR HANDLING:
 * - Any failure → Cleanup partial state
 * - Set shutdown=true to exit already-created threads
 * - Broadcast to wake workers for shutdown check
 * - Join created threads (wait for exit)
 * - Destroy primitives in reverse order
 * - Free pool structure
 *
 * WHY GRACEFUL CLEANUP ON ERROR:
 * - Prevents resource leaks (threads, mutexes)
 * - Leaves system in consistent state
 * - Caller can retry or fallback to serial execution
 *
 * COMPLEXITY: O(n) where n = num_threads (thread creation)
 * THREAD SAFETY: Not thread-safe (called once at startup)
 *
 * @param num_threads Number of worker threads (1 to MAX_THREADS)
 * @return Pool handle or NULL on error
 */
nimcp_thread_pool_t* nimcp_pool_create(size_t num_threads)
{
    // Validate parameters
    // WHY CHECK 0: Pool with no threads can't do work
    // WHY CHECK MAX: Prevent array overflow
    if (num_threads == 0) {
        LOG_ERROR("THREAD_POOL", "Invalid num_threads: cannot be 0");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "Thread pool num_threads cannot be 0");
        return NULL;
    }
    if (num_threads > NIMCP_POOL_MAX_THREADS) {
        LOG_ERROR("THREAD_POOL", "Invalid num_threads %zu: exceeds max %d",
                  num_threads, NIMCP_POOL_MAX_THREADS);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "Thread pool num_threads %zu exceeds max %d",
                             num_threads, NIMCP_POOL_MAX_THREADS);
        return NULL;
    }

    // Allocate pool structure
    // WHY CALLOC: Zeros all fields (safe initial state)
    nimcp_thread_pool_t* pool = (nimcp_thread_pool_t*) nimcp_calloc(1, sizeof(nimcp_thread_pool_t));
    NIMCP_API_CHECK_ALLOC(pool, "Failed to allocate thread pool structure");

    // Initialize fields
    pool->num_threads = num_threads;
    pool->queue_head = 0;
    pool->queue_tail = 0;
    pool->queue_count = 0;
    pool->active_threads = 0;
    pool->shutdown = false;

    // Initialize mutex
    // WHY FIRST: Needed for condition variables
    if (nimcp_mutex_init(&pool->lock, NULL) != NIMCP_SUCCESS) {
        LOG_ERROR("THREAD_POOL", "Failed to initialize pool mutex");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_SYSTEM, "Failed to initialize thread pool mutex");
        nimcp_free(pool);
        return NULL;
    }

    // Initialize condition variable for worker wakeup
    // WHY: Workers wait for tasks on this condvar
    if (nimcp_cond_init(&pool->task_available) != NIMCP_SUCCESS) {
        LOG_ERROR("THREAD_POOL", "Failed to initialize task_available condition variable");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_SYSTEM, "Failed to initialize task_available condition variable");
        nimcp_mutex_destroy(&pool->lock);
        nimcp_free(pool);
        return NULL;
    }

    // Initialize condition variable for completion signaling
    // WHY: Submitters wait for completion/space on this condvar
    if (nimcp_cond_init(&pool->task_complete) != NIMCP_SUCCESS) {
        LOG_ERROR("THREAD_POOL", "Failed to initialize task_complete condition variable");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_SYSTEM, "Failed to initialize task_complete condition variable");
        nimcp_cond_destroy(&pool->task_available);
        nimcp_mutex_destroy(&pool->lock);
        nimcp_free(pool);
        return NULL;
    }

    // Create worker threads
    // WHY LOOP: Create num_threads workers
    // WHY PASS pool: Each worker needs pool state
    for (size_t i = 0; i < num_threads; i++) {
        if (nimcp_thread_create(&pool->threads[i], worker_thread, pool, NULL) != NIMCP_SUCCESS) {
            // Cleanup on failure
            // WHY SET shutdown: Signal already-created workers to exit
            pool->shutdown = true;
            nimcp_cond_broadcast(&pool->task_available);

            // Join already created threads
            // WHY LOOP TO i: Only join successfully created threads
            for (size_t j = 0; j < i; j++) {
                nimcp_thread_join(pool->threads[j], NULL);
            }

            // Destroy primitives and free pool
            nimcp_cond_destroy(&pool->task_complete);
            nimcp_cond_destroy(&pool->task_available);
            nimcp_mutex_destroy(&pool->lock);
            nimcp_free(pool);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "nimcp_pool_create: thread creation failed");
            return NULL;
        }
    }

    return pool;
}

/**
 * @brief Destroy thread pool (graceful shutdown)
 *
 * WHY GRACEFUL SHUTDOWN:
 * - Completes queued tasks (no dropped work)
 * - Allows workers to exit cleanly
 * - Prevents resource leaks
 *
 * ALGORITHM:
 * 1. Acquire lock
 * 2. Set shutdown flag
 * 3. Broadcast to all workers (wake them up)
 * 4. Release lock
 * 5. Join all worker threads (wait for exit)
 * 6. Destroy synchronization primitives
 * 7. Free pool structure
 *
 * WHY BROADCAST vs SIGNAL:
 * - broadcast: Wake ALL workers (all need to exit)
 * - signal: Wake ONE worker (not enough for shutdown)
 *
 * WHY JOIN:
 * - Waits for worker threads to finish
 * - Ensures no access to pool after destroy
 * - Prevents use-after-free bugs
 *
 * SHUTDOWN SEQUENCE:
 * 1. shutdown=true prevents new submissions
 * 2. Workers drain existing queue
 * 3. Workers exit when queue empty
 * 4. Join waits for all exits
 * 5. Primitives destroyed (safe, no users)
 *
 * COMPLEXITY: O(n + m) where n=num_threads (join), m=queued_tasks (drain)
 * THREAD SAFETY: Assumes no concurrent pool_submit calls
 * - Caller must ensure no more submissions before destroy
 *
 * @param pool Pool to destroy (NULL is safe)
 */
void nimcp_pool_destroy(nimcp_thread_pool_t* pool)
{
    if (!pool) {
        return;
    }

    // Signal shutdown
    // WHY LOCK: shutdown is shared state
    nimcp_mutex_lock(&pool->lock);
    pool->shutdown = true;
    // WHY BROADCAST: Wake all workers so they can check shutdown flag
    nimcp_cond_broadcast(&pool->task_available);
    // Also broadcast to task_complete to wake any threads waiting in pool_wait() or pool_submit()
    nimcp_cond_broadcast(&pool->task_complete);
    nimcp_mutex_unlock(&pool->lock);

    // Wait for all threads to finish
    // WHY JOIN: Ensures threads have exited before cleanup
    // WHY LOOP: Join each worker individually
    for (size_t i = 0; i < pool->num_threads; i++) {
        nimcp_thread_join(pool->threads[i], NULL);
    }

    // Cleanup synchronization primitives
    // WHY AFTER JOIN: Threads may still be using them before join completes
    // WHY THIS ORDER: Reverse of creation order
    nimcp_cond_destroy(&pool->task_complete);
    nimcp_cond_destroy(&pool->task_available);
    nimcp_mutex_destroy(&pool->lock);

    // Free pool structure
    nimcp_free(pool);
}

/**
 * @brief Submit a task to the pool
 *
 * WHY SUBMIT API:
 * - Decouples task creation from execution
 * - Enables parallel processing
 * - Provides backpressure (blocks if queue full)
 *
 * ALGORITHM:
 * 1. Validate parameters
 * 2. Acquire lock
 * 3. Check shutdown: Return error if true
 * 4. Wait while (queue full && not shutdown)
 * 5. If shutdown after wait: Return error
 * 6. Enqueue task at tail
 * 7. Update tail and count
 * 8. Signal one worker
 * 9. Release lock
 *
 * WHY BLOCK WHEN FULL:
 * - Natural backpressure (slows producer to consumer rate)
 * - Prevents memory exhaustion (bounded queue)
 * - Self-regulating system
 * - Alternative: Drop task (unacceptable for batch processing)
 * - Alternative: Unbounded queue (memory risk)
 *
 * WHY WAIT LOOP:
 * - Condition variables can have spurious wakeups
 * - Must re-check condition after each wakeup
 * - Handles race: Multiple submitters racing for space
 *
 * WHY CHECK SHUTDOWN TWICE:
 * - First check (line 3): Fail fast if already shutdown
 * - Second check (line 5): Catch shutdown during wait
 * - Ensures no tasks submitted after shutdown initiated
 *
 * WHY SIGNAL ONE WORKER:
 * - One task added → One worker needed
 * - Efficient: Don't wake all workers (thundering herd)
 * - Fair: OS scheduler picks which worker wakes
 *
 * ENQUEUE ALGORITHM:
 * - Place task at queue[tail]
 * - Advance tail: (tail + 1) % MAX_QUEUE (circular wrap)
 * - Increment count
 *
 * COMPLEXITY: O(1) amortized (may block if queue full)
 * THREAD SAFETY: Fully thread-safe (lock-protected)
 *
 * @param pool Pool handle
 * @param task Task function to execute
 * @param arg Argument passed to task function
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_pool_submit(nimcp_thread_pool_t* pool, nimcp_task_fn task, void* arg)
{
    NIMCP_API_CHECK_NULL(pool, NIMCP_ERROR_INVALID_PARAM, "NULL pool in nimcp_pool_submit");
    NIMCP_API_CHECK_NULL(task, NIMCP_ERROR_INVALID_PARAM, "NULL task function in nimcp_pool_submit");

    nimcp_mutex_lock(&pool->lock);

    // Check for shutdown
    // WHY: Don't accept new tasks if shutting down
    if (pool->shutdown) {
        nimcp_mutex_unlock(&pool->lock);
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_SYSTEM, "thread pool is shutting down");
    }

    // Wait if queue is full
    // WHY LOOP: Re-check after each wakeup (spurious wakeups possible)
    // WHY WAIT ON task_complete: Signaled when task finishes (frees space)
    while (pool->queue_count >= NIMCP_POOL_MAX_QUEUE && !pool->shutdown) {
        nimcp_cond_wait(&pool->task_complete, &pool->lock);
    }

    // Check shutdown again (may have changed during wait)
    if (pool->shutdown) {
        nimcp_mutex_unlock(&pool->lock);
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_SYSTEM, "thread pool shut down during wait");
    }

    // Enqueue task
    // WHY ASSIGN BOTH FIELDS: Store function and argument
    pool->queue[pool->queue_tail].func = task;
    pool->queue[pool->queue_tail].arg = arg;
    // WHY MODULO: Circular buffer wrap-around
    pool->queue_tail = (pool->queue_tail + 1) % NIMCP_POOL_MAX_QUEUE;
    pool->queue_count++;

    // Signal worker threads
    // WHY SIGNAL vs BROADCAST: One task, one worker needed
    nimcp_cond_signal(&pool->task_available);
    nimcp_mutex_unlock(&pool->lock);

    return NIMCP_SUCCESS;
}

/**
 * @brief Wait for all submitted tasks to complete
 *
 * WHY WAIT API:
 * - Barrier synchronization (wait for all tasks)
 * - Enables sequential phases (submit → wait → submit)
 * - Ensures completion before proceeding
 *
 * ALGORITHM:
 * 1. Validate parameters
 * 2. Acquire lock
 * 3. Wait while (queue not empty OR workers active) AND not shutdown
 * 4. Release lock
 * 5. Return error if shutdown, success otherwise
 *
 * COMPLETION CONDITION:
 * - queue_count == 0: No pending tasks
 * - active_threads == 0: No executing tasks
 * - Both conditions needed: Tasks may be executing but not in queue
 *
 * WHY BOTH CONDITIONS:
 * - queue_count == 0 alone: Miss currently executing tasks
 * - active_threads == 0 alone: Miss queued tasks
 * - Together: All tasks submitted before wait() have completed
 *
 * WHY WAIT LOOP:
 * - Condition variables can have spurious wakeups
 * - Must re-check condition after each wakeup
 * - Handles race: Task completes, another submitted immediately
 *
 * WHY WAIT ON task_complete:
 * - Signaled when worker finishes task (active_threads decrements)
 * - Signaled when task dequeued (queue_count may decrease elsewhere)
 * - Ensures wakeup when progress made
 *
 * USAGE PATTERN:
 *   for (i = 0; i < 100; i++)
 *       pool_submit(pool, task, &data[i]);
 *   pool_wait(pool);  // Barrier: Wait for all 100 tasks
 *   // Safe to use results now
 *
 * COMPLEXITY: O(m) where m = remaining tasks
 * THREAD SAFETY: Fully thread-safe (lock-protected)
 *
 * @param pool Pool handle
 * @return NIMCP_SUCCESS or NIMCP_ERROR_SYSTEM if shutdown
 */
nimcp_result_t nimcp_pool_wait(nimcp_thread_pool_t* pool)
{
    NIMCP_API_CHECK_NULL(pool, NIMCP_ERROR_INVALID_PARAM, "NULL pool in nimcp_pool_wait");

    nimcp_mutex_lock(&pool->lock);

    // Wait until queue is empty and no active threads
    // WHY BOTH CONDITIONS: See function comment above
    // WHY CHECK shutdown: Exit wait if pool shutting down
    while ((pool->queue_count > 0 || pool->active_threads > 0) && !pool->shutdown) {
        nimcp_cond_wait(&pool->task_complete, &pool->lock);
    }

    // Cache shutdown value BEFORE releasing lock to avoid race condition
    // where pool could be destroyed after unlock but before we read shutdown
    bool was_shutdown = pool->shutdown;

    nimcp_mutex_unlock(&pool->lock);

    // WHY RETURN ERROR ON SHUTDOWN:
    // - Caller should know pool was destroyed during wait
    // - Prevents further use of pool
    return was_shutdown ? NIMCP_ERROR_SYSTEM : NIMCP_SUCCESS;
}

/**
 * @brief Get number of pending tasks
 *
 * WHY PENDING COUNT:
 * - Monitor queue depth
 * - Detect backpressure (queue near full)
 * - Debug: Understand system behavior
 *
 * DEFINITION: Number of tasks in queue (not yet dequeued)
 * - Does NOT include currently executing tasks (see pool_active)
 *
 * WHY LOCK:
 * - queue_count is shared state
 * - Ensures consistent snapshot
 *
 * COMPLEXITY: O(1)
 * THREAD SAFETY: Fully thread-safe (lock-protected)
 *
 * @param pool Pool handle
 * @return Number of queued tasks (0 if pool NULL)
 */
size_t nimcp_pool_pending(nimcp_thread_pool_t* pool)
{
    if (!pool) {
        return 0;
    }

    nimcp_mutex_lock(&pool->lock);
    size_t pending = pool->queue_count;
    nimcp_mutex_unlock(&pool->lock);

    return pending;
}

/**
 * @brief Get number of active worker threads
 *
 * WHY ACTIVE COUNT:
 * - Monitor utilization (active/total ratio)
 * - Detect idle workers (underutilization)
 * - Debug: Understand parallelism
 *
 * DEFINITION: Number of workers currently executing tasks
 * - Does NOT include idle workers waiting for tasks
 *
 * UTILIZATION METRICS:
 * - active == num_threads: Full utilization
 * - active < num_threads: Some workers idle (may be normal)
 * - active == 0 && pending > 0: Workers waking up (transient)
 *
 * WHY LOCK:
 * - active_threads is shared state
 * - Ensures consistent snapshot
 *
 * COMPLEXITY: O(1)
 * THREAD SAFETY: Fully thread-safe (lock-protected)
 *
 * @param pool Pool handle
 * @return Number of active threads (0 if pool NULL)
 */
size_t nimcp_pool_active(nimcp_thread_pool_t* pool)
{
    if (!pool) {
        return 0;
    }

    nimcp_mutex_lock(&pool->lock);
    size_t active = pool->active_threads;
    nimcp_mutex_unlock(&pool->lock);

    return active;
}
