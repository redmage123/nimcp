/**
 * @file nimcp_queue.h
 * @brief Unified queue API with multiple implementation types
 *
 * WHAT: Thread-safe queue abstraction supporting blocking, SPSC, and MPMC modes
 * WHY:  Provide flexible queue implementations optimized for different use cases
 * HOW:  Factory pattern dispatches to specialized implementations per queue type
 *
 * QUEUE TYPES:
 * - BLOCKING:  Mutex + condvar, any number of producers/consumers
 * - SPSC:      Lock-free, exactly 1 producer and 1 consumer (<100ns latency)
 * - MPMC:      Lock-free Vyukov algorithm, multiple producers/consumers (<500ns)
 *
 * ARCHITECTURE:
 *
 *   Application Code
 *         │
 *         ▼
 *   ┌─────────────────────────────────────────────────────────┐
 *   │              Unified Queue API (nimcp_queue.h)          │
 *   │  nimcp_queue_create, nimcp_queue_enqueue, etc.          │
 *   └─────────────────────────────────────────────────────────┘
 *         │
 *         │ (dispatch based on queue_type)
 *         │
 *   ┌─────┴─────────────────────┬──────────────────────────────┐
 *   │                           │                              │
 *   ▼                           ▼                              ▼
 * ┌──────────────┐    ┌──────────────────┐    ┌────────────────────┐
 * │   BLOCKING   │    │      SPSC        │    │       MPMC         │
 * │ (mutex+cond) │    │  (lock-free)     │    │ (Vyukov algorithm) │
 * │ Any threads  │    │ 1 prod, 1 cons   │    │ N prod, M cons     │
 * └──────────────┘    └──────────────────┘    └────────────────────┘
 *
 * PERFORMANCE CHARACTERISTICS:
 * - BLOCKING:  ~1-10μs per operation (includes context switch if contended)
 * - SPSC:      ~50-100ns per operation (wait-free, cache-optimized)
 * - MPMC:      ~200-500ns per operation (lock-free, handles contention)
 *
 * USE CASE SELECTION:
 * - BLOCKING:  General purpose, unknown thread patterns
 * - SPSC:      Pipeline stages, producer-consumer pairs, spike propagation
 * - MPMC:      Work stealing, task queues, event distribution
 *
 * @author NIMCP Development Team
 * @date 2025-11-28
 * @version 2.0.0
 */

#ifndef NIMCP_QUEUE_H
#define NIMCP_QUEUE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "utils/validation/nimcp_common.h"
#include "common/nimcp_export.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** @brief CPU cache line size for alignment (prevents false sharing) */
#define NIMCP_QUEUE_CACHE_LINE_SIZE 64

/** @brief Default spin count before blocking (for MPMC) */
#define NIMCP_QUEUE_DEFAULT_SPIN_COUNT 1000

/** @brief Maximum queue capacity */
#define NIMCP_QUEUE_MAX_CAPACITY (1ULL << 30)

/** @brief Minimum queue capacity */
#define NIMCP_QUEUE_MIN_CAPACITY 2

//=============================================================================
// Queue Type Enumeration
//=============================================================================

/**
 * @brief Queue implementation type
 *
 * WHAT: Selects underlying queue algorithm
 * WHY:  Different use cases need different performance characteristics
 * HOW:  Factory dispatches to specialized implementation
 */
typedef enum {
    NIMCP_QUEUE_TYPE_BLOCKING = 0,  /**< Mutex + condvar (general purpose) */
    NIMCP_QUEUE_TYPE_SPSC = 1,      /**< Lock-free SPSC (single producer/consumer) */
    NIMCP_QUEUE_TYPE_MPMC = 2       /**< Lock-free MPMC (multiple producers/consumers) */
} nimcp_queue_type_t;

//=============================================================================
// Configuration Structure
//=============================================================================

/**
 * @brief Queue configuration
 *
 * WHAT: Parameters for queue creation
 * WHY:  Allow customization of queue behavior
 * HOW:  Passed to nimcp_queue_create()
 */
typedef struct {
    // Required parameters
    size_t max_size;          /**< Maximum number of items (rounded to power of 2 for SPSC/MPMC) */
    size_t item_size;         /**< Size of each item in bytes */
    nimcp_queue_type_t type;  /**< Queue implementation type */

    // Blocking behavior
    bool is_blocking;         /**< Whether operations block when full/empty */
    uint32_t timeout_ms;      /**< Default timeout for blocking operations (0 = infinite) */

    // Performance tuning (MPMC only)
    uint32_t spin_count;      /**< Spin iterations before blocking (0 = use default) */

    // Memory management
    void* mem_manager;        /**< Optional: unified memory manager (NULL = use malloc) */
} nimcp_queue_config_t;

//=============================================================================
// Statistics Structure
//=============================================================================

/**
 * @brief Queue runtime statistics
 *
 * WHAT: Performance and behavior metrics
 * WHY:  Enable monitoring, profiling, and debugging
 * HOW:  Atomic counters updated during operations
 */
typedef struct {
    // Operation counts
    uint64_t total_enqueued;      /**< Total items successfully enqueued */
    uint64_t total_dequeued;      /**< Total items successfully dequeued */
    uint64_t enqueue_failures;    /**< Enqueue attempts that failed (full/timeout) */
    uint64_t dequeue_failures;    /**< Dequeue attempts that failed (empty/timeout) */

    // Capacity metrics
    size_t current_size;          /**< Current number of items in queue */
    size_t peak_size;             /**< Maximum size ever reached */
    size_t capacity;              /**< Maximum capacity */

    // Blocking statistics (BLOCKING and MPMC with blocking)
    uint64_t blocking_waits;      /**< Times operations blocked waiting */
    uint64_t blocking_timeouts;   /**< Blocking operations that timed out */

    // Contention metrics (MPMC only)
    uint64_t cas_retries;         /**< CAS operation retries due to contention */
    uint64_t spin_cycles;         /**< Total spin iterations */

    // Batch operation statistics
    uint64_t batch_enqueue_ops;   /**< Number of batch enqueue operations */
    uint64_t batch_dequeue_ops;   /**< Number of batch dequeue operations */
    uint64_t batch_items_enqueued;/**< Total items enqueued via batch */
    uint64_t batch_items_dequeued;/**< Total items dequeued via batch */

    // Type information
    nimcp_queue_type_t type;      /**< Queue implementation type */
} nimcp_queue_status_t;

//=============================================================================
// Queue Handle
//=============================================================================

/**
 * @brief Opaque queue handle
 *
 * WHAT: Handle to queue instance (type-erased)
 * WHY:  Encapsulation - hide internal structures
 * HOW:  Actual struct defined in implementation
 */
typedef struct nimcp_queue* nimcp_queue_handle_t;

//=============================================================================
// Queue Creation and Destruction
//=============================================================================

/**
 * @brief Create queue with specified configuration
 *
 * WHAT: Allocates and initializes queue of specified type
 * WHY:  Factory method for creating queues
 * HOW:  Dispatches to type-specific implementation
 *
 * @param config Queue configuration
 * @param queue Output: queue handle
 * @return NIMCP_SUCCESS or error code
 *
 * COMPLEXITY: O(n) where n = max_size
 * THREAD SAFETY: Thread-safe
 *
 * NOTES:
 * - For SPSC/MPMC, max_size is rounded up to next power of 2
 * - Memory is allocated from unified memory if mem_manager provided
 *
 * EXAMPLE:
 * ```c
 * nimcp_queue_config_t config = {
 *     .max_size = 1024,
 *     .item_size = sizeof(event_t),
 *     .type = NIMCP_QUEUE_TYPE_SPSC,
 *     .is_blocking = true,
 *     .timeout_ms = 100
 * };
 * nimcp_queue_handle_t queue;
 * nimcp_result_t result = nimcp_queue_create(&config, &queue);
 * ```
 */
NIMCP_EXPORT nimcp_result_t nimcp_queue_create(
    const nimcp_queue_config_t* config,
    nimcp_queue_handle_t* queue
);

/**
 * @brief Destroy queue and free resources
 *
 * WHAT: Frees all resources associated with queue
 * WHY:  Clean shutdown
 * HOW:  Type-specific cleanup
 *
 * @param queue Queue handle (NULL safe)
 * @return NIMCP_SUCCESS or error code
 *
 * WARNING: Queue must not be in use by any threads when destroyed
 */
NIMCP_EXPORT nimcp_result_t nimcp_queue_destroy(nimcp_queue_handle_t queue);

/**
 * @brief Get default configuration for queue type
 *
 * @param type Queue type
 * @return Default configuration
 */
NIMCP_EXPORT nimcp_queue_config_t nimcp_queue_default_config(nimcp_queue_type_t type);

//=============================================================================
// Single Item Operations
//=============================================================================

/**
 * @brief Enqueue single item
 *
 * WHAT: Add item to queue tail
 * WHY:  Producer operation
 * HOW:  Type-specific implementation (lock-free or mutex)
 *
 * @param queue Queue handle
 * @param item Pointer to item data to copy
 * @param timeout_ms Timeout in milliseconds (0 = try once, UINT32_MAX = infinite)
 * @return NIMCP_SUCCESS, NIMCP_ERROR_QUEUE_FULL, or NIMCP_ERROR_TIMEOUT
 *
 * COMPLEXITY: O(1)
 * THREAD SAFETY:
 * - BLOCKING: Safe for multiple producers
 * - SPSC: Safe for single producer only
 * - MPMC: Safe for multiple producers
 */
NIMCP_EXPORT nimcp_result_t nimcp_queue_enqueue(
    nimcp_queue_handle_t queue,
    const void* item,
    uint32_t timeout_ms
);

/**
 * @brief Dequeue single item
 *
 * WHAT: Remove item from queue head
 * WHY:  Consumer operation
 * HOW:  Type-specific implementation (lock-free or mutex)
 *
 * @param queue Queue handle
 * @param item Pointer to buffer for item data
 * @param timeout_ms Timeout in milliseconds (0 = try once, UINT32_MAX = infinite)
 * @return NIMCP_SUCCESS, NIMCP_ERROR_QUEUE_EMPTY, or NIMCP_ERROR_TIMEOUT
 *
 * COMPLEXITY: O(1)
 * THREAD SAFETY:
 * - BLOCKING: Safe for multiple consumers
 * - SPSC: Safe for single consumer only
 * - MPMC: Safe for multiple consumers
 */
NIMCP_EXPORT nimcp_result_t nimcp_queue_dequeue(
    nimcp_queue_handle_t queue,
    void* item,
    uint32_t timeout_ms
);

/**
 * @brief Try to enqueue without blocking
 *
 * WHAT: Non-blocking enqueue attempt
 * WHY:  Fast path when blocking not desired
 * HOW:  Equivalent to enqueue with timeout_ms=0
 *
 * @param queue Queue handle
 * @param item Pointer to item data
 * @return true if enqueued, false if full
 */
NIMCP_EXPORT bool nimcp_queue_try_enqueue(
    nimcp_queue_handle_t queue,
    const void* item
);

/**
 * @brief Try to dequeue without blocking
 *
 * WHAT: Non-blocking dequeue attempt
 * WHY:  Fast path when blocking not desired
 * HOW:  Equivalent to dequeue with timeout_ms=0
 *
 * @param queue Queue handle
 * @param item Pointer to buffer for item
 * @return true if dequeued, false if empty
 */
NIMCP_EXPORT bool nimcp_queue_try_dequeue(
    nimcp_queue_handle_t queue,
    void* item
);

//=============================================================================
// Batch Operations
//=============================================================================

/**
 * @brief Enqueue multiple items
 *
 * WHAT: Bulk enqueue for efficiency
 * WHY:  Amortize overhead for high-throughput scenarios
 * HOW:  Single lock acquisition or optimized batch copy
 *
 * @param queue Queue handle
 * @param items Array of items to enqueue
 * @param count Number of items
 * @param enqueued_count Output: actual number enqueued
 * @param timeout_ms Timeout for entire batch
 * @return NIMCP_SUCCESS if all enqueued, NIMCP_ERROR_PARTIAL if some enqueued
 *
 * COMPLEXITY: O(count)
 */
NIMCP_EXPORT nimcp_result_t nimcp_queue_enqueue_batch(
    nimcp_queue_handle_t queue,
    const void* items,
    size_t count,
    size_t* enqueued_count,
    uint32_t timeout_ms
);

/**
 * @brief Dequeue multiple items
 *
 * WHAT: Bulk dequeue for efficiency
 * WHY:  Amortize overhead for high-throughput scenarios
 * HOW:  Single lock acquisition or optimized batch copy
 *
 * @param queue Queue handle
 * @param items Buffer for dequeued items
 * @param max_count Maximum items to dequeue
 * @param dequeued_count Output: actual number dequeued
 * @param timeout_ms Timeout for first item (subsequent items non-blocking)
 * @return NIMCP_SUCCESS if any dequeued, NIMCP_ERROR_QUEUE_EMPTY if none
 *
 * COMPLEXITY: O(max_count)
 */
NIMCP_EXPORT nimcp_result_t nimcp_queue_dequeue_batch(
    nimcp_queue_handle_t queue,
    void* items,
    size_t max_count,
    size_t* dequeued_count,
    uint32_t timeout_ms
);

//=============================================================================
// Queue State Queries
//=============================================================================

/**
 * @brief Peek at front item without removing
 *
 * @param queue Queue handle
 * @param item Output buffer for item copy
 * @return NIMCP_SUCCESS or NIMCP_ERROR_QUEUE_EMPTY
 */
NIMCP_EXPORT nimcp_result_t nimcp_queue_peek(
    nimcp_queue_handle_t queue,
    void* item
);

/**
 * @brief Clear all items from queue
 *
 * @param queue Queue handle
 * @return NIMCP_SUCCESS or error code
 *
 * WARNING: Not safe if other threads are actively using the queue
 */
NIMCP_EXPORT nimcp_result_t nimcp_queue_clear(nimcp_queue_handle_t queue);

/**
 * @brief Check if queue is empty
 *
 * @param queue Queue handle
 * @return true if empty (snapshot - may change immediately)
 */
NIMCP_EXPORT bool nimcp_queue_is_empty(nimcp_queue_handle_t queue);

/**
 * @brief Check if queue is full
 *
 * @param queue Queue handle
 * @return true if full (snapshot - may change immediately)
 */
NIMCP_EXPORT bool nimcp_queue_is_full(nimcp_queue_handle_t queue);

/**
 * @brief Get current queue size
 *
 * @param queue Queue handle
 * @return Number of items (snapshot)
 */
NIMCP_EXPORT size_t nimcp_queue_get_size(nimcp_queue_handle_t queue);

/**
 * @brief Get queue capacity
 *
 * @param queue Queue handle
 * @return Maximum capacity
 */
NIMCP_EXPORT size_t nimcp_queue_get_capacity(nimcp_queue_handle_t queue);

/**
 * @brief Get available space
 *
 * @param queue Queue handle
 * @return Number of items that can be enqueued (snapshot)
 */
NIMCP_EXPORT size_t nimcp_queue_get_available(nimcp_queue_handle_t queue);

/**
 * @brief Get queue type
 *
 * @param queue Queue handle
 * @return Queue implementation type
 */
NIMCP_EXPORT nimcp_queue_type_t nimcp_queue_get_type(nimcp_queue_handle_t queue);

//=============================================================================
// Statistics and Monitoring
//=============================================================================

/**
 * @brief Get queue statistics
 *
 * @param queue Queue handle
 * @param status Output statistics structure
 * @return NIMCP_SUCCESS or error code
 */
NIMCP_EXPORT nimcp_result_t nimcp_queue_get_status(
    nimcp_queue_handle_t queue,
    nimcp_queue_status_t* status
);

/**
 * @brief Reset queue statistics
 *
 * @param queue Queue handle
 * @return NIMCP_SUCCESS or error code
 */
NIMCP_EXPORT nimcp_result_t nimcp_queue_reset_stats(nimcp_queue_handle_t queue);

/**
 * @brief Get queue utilization percentage
 *
 * @param queue Queue handle
 * @return Utilization as percentage (0.0 - 100.0)
 */
NIMCP_EXPORT float nimcp_queue_get_utilization(nimcp_queue_handle_t queue);

//=============================================================================
// Configuration (Runtime)
//=============================================================================

/**
 * @brief Set spin count for MPMC blocking operations
 *
 * @param queue Queue handle
 * @param spin_count New spin count (0 = disable spinning)
 * @return NIMCP_SUCCESS or error code
 *
 * NOTE: Only affects MPMC queues
 */
NIMCP_EXPORT nimcp_result_t nimcp_queue_set_spin_count(
    nimcp_queue_handle_t queue,
    uint32_t spin_count
);

/**
 * @brief Get current spin count
 *
 * @param queue Queue handle
 * @return Current spin count
 */
NIMCP_EXPORT uint32_t nimcp_queue_get_spin_count(nimcp_queue_handle_t queue);

//=============================================================================
// Helper Functions
//=============================================================================

/**
 * @brief Get queue type name as string
 *
 * @param type Queue type enum
 * @return Human-readable string
 */
static inline const char* nimcp_queue_type_name(nimcp_queue_type_t type)
{
    switch (type) {
        case NIMCP_QUEUE_TYPE_BLOCKING: return "blocking";
        case NIMCP_QUEUE_TYPE_SPSC: return "spsc";
        case NIMCP_QUEUE_TYPE_MPMC: return "mpmc";
        default: return "unknown";
    }
}

/**
 * @brief Round up to next power of 2
 *
 * @param value Input value
 * @return Next power of 2 >= value
 */
static inline size_t nimcp_queue_next_power_of_2(size_t value)
{
    if (value == 0) return 1;
    if ((value & (value - 1)) == 0) return value;
    value--;
    value |= value >> 1;
    value |= value >> 2;
    value |= value >> 4;
    value |= value >> 8;
    value |= value >> 16;
#if SIZE_MAX > UINT32_MAX
    value |= value >> 32;
#endif
    return value + 1;
}

#ifdef __cplusplus
}
#endif

#endif  // NIMCP_QUEUE_H
