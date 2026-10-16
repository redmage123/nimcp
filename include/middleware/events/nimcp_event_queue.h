//=============================================================================
// nimcp_event_queue.h - Priority Event Queue
//=============================================================================

#ifndef NIMCP_EVENT_QUEUE_H
#define NIMCP_EVENT_QUEUE_H

#include "middleware/events/nimcp_event_types.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file nimcp_event_queue.h
 * @brief Priority event queue with min-heap implementation
 *
 * WHAT: Thread-safe priority queue for events
 * WHY:  Process critical events before routine ones
 * HOW:  Binary min-heap ordered by priority, FIFO within same priority
 *
 * DESIGN PATTERNS:
 * - Priority Queue: Min-heap data structure
 * - Bounded Buffer: Fixed capacity with overflow policy
 * - Observer Pattern: Callbacks on overflow/underflow
 *
 * PERFORMANCE:
 * - Enqueue: O(log n)
 * - Dequeue: O(log n)
 * - Peek: O(1)
 * - Space: O(capacity)
 *
 * THREAD-SAFETY: All operations are thread-safe (mutex protected)
 */

//=============================================================================
// Configuration and Types
//=============================================================================

/**
 * @brief Overflow policy when queue is full
 */
typedef enum {
    OVERFLOW_POLICY_DROP_OLDEST,    /**< Drop oldest (least recent) event */
    OVERFLOW_POLICY_DROP_LOWEST,    /**< Drop lowest priority event */
    OVERFLOW_POLICY_DROP_NEWEST,    /**< Drop incoming event */
    OVERFLOW_POLICY_BLOCK           /**< Block until space available */
} overflow_policy_t;

/**
 * @brief Event queue configuration
 */
typedef struct {
    uint32_t capacity;              /**< Maximum events (default: 1024) */
    overflow_policy_t overflow_policy; /**< What to do when full */
    bool enable_coalescing;         /**< Merge similar events? */
    uint64_t block_timeout_us;      /**< Timeout for BLOCK policy (0=forever) */
    uint32_t max_payload_size;      /**< Max payload size for pooling (default: 256 bytes) */
} event_queue_config_t;

/**
 * @brief Event queue statistics
 */
typedef struct {
    uint64_t total_enqueued;        /**< Total events enqueued */
    uint64_t total_dequeued;        /**< Total events dequeued */
    uint64_t total_dropped;         /**< Total events dropped */
    uint64_t total_coalesced;       /**< Total events coalesced */
    uint32_t current_size;          /**< Current queue size */
    uint32_t peak_size;             /**< Peak queue size */
    float avg_wait_time_us;         /**< Average wait time */
} event_queue_stats_t;

/**
 * @brief Opaque event queue handle
 */
typedef struct event_queue_struct* event_queue_t;

//=============================================================================
// Event Queue Lifecycle
//=============================================================================

/**
 * @brief Create event queue
 *
 * WHAT: Allocates and initializes priority queue
 * WHY:  Factory function with validation
 * HOW:  Allocates heap array, initializes mutex
 *
 * COMPLEXITY: O(capacity)
 * THREAD-SAFE: Yes
 *
 * @param config Queue configuration (NULL for defaults)
 * @return Queue handle or NULL on error
 */
event_queue_t event_queue_create(const event_queue_config_t* config);

/**
 * @brief Destroy event queue
 *
 * WHAT: Frees queue and all contained events
 * WHY:  Clean resource cleanup
 * HOW:  Frees all events, destroys mutex, frees structure
 *
 * COMPLEXITY: O(n) where n = current size
 * THREAD-SAFE: Yes (but queue unusable after)
 *
 * @param queue Queue to destroy (NULL safe)
 */
void event_queue_destroy(event_queue_t queue);

//=============================================================================
// Queue Operations
//=============================================================================

/**
 * @brief Enqueue event (add to queue)
 *
 * WHAT: Insert event maintaining priority order
 * WHY:  Add events for processing
 * HOW:  Min-heap insertion, handle overflow per policy
 *
 * COMPLEXITY: O(log n) worst case
 * THREAD-SAFE: Yes
 * BLOCKS: Only if policy=OVERFLOW_POLICY_BLOCK and queue full
 *
 * @param queue Event queue
 * @param event Event to enqueue (will be copied)
 * @return true on success, false if dropped
 */
bool event_queue_enqueue(event_queue_t queue, const event_t* event);

/**
 * @brief Dequeue event (remove highest priority)
 *
 * WHAT: Remove and return highest priority event
 * WHY:  Get next event to process
 * HOW:  Min-heap extraction (removes root, re-heapifies)
 *
 * COMPLEXITY: O(log n)
 * THREAD-SAFE: Yes
 *
 * @param queue Event queue
 * @param event Output event (caller must free with event_free)
 * @return true if event retrieved, false if queue empty
 */
bool event_queue_dequeue(event_queue_t queue, event_t* event);

/**
 * @brief Peek at highest priority event without removing
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 *
 * @param queue Event queue
 * @param event Output event (shallow copy, don't free)
 * @return true if event available
 */
bool event_queue_peek(event_queue_t queue, event_t* event);

/**
 * @brief Dequeue multiple events (batch)
 *
 * WHAT: Remove up to max_events highest priority events
 * WHY:  Efficient batch processing
 * HOW:  Repeated dequeue operations
 *
 * COMPLEXITY: O(k log n) where k = max_events
 * THREAD-SAFE: Yes
 *
 * @param queue Event queue
 * @param events Output array (pre-allocated)
 * @param max_events Maximum events to dequeue
 * @return Number of events actually dequeued
 */
uint32_t event_queue_dequeue_batch(event_queue_t queue, event_t* events,
                                    uint32_t max_events);

/**
 * @brief Get queue size
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 *
 * @param queue Event queue
 * @return Current number of events
 */
uint32_t event_queue_size(event_queue_t queue);

/**
 * @brief Check if queue is empty
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
bool event_queue_is_empty(event_queue_t queue);

/**
 * @brief Check if queue is full
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
bool event_queue_is_full(event_queue_t queue);

/**
 * @brief Clear queue (remove all events)
 *
 * WHAT: Remove and free all events
 * WHY:  Reset queue state
 * HOW:  Free all events, reset heap
 *
 * COMPLEXITY: O(n)
 * THREAD-SAFE: Yes
 *
 * @param queue Event queue
 */
void event_queue_clear(event_queue_t queue);

//=============================================================================
// Advanced Operations
//=============================================================================

/**
 * @brief Remove events matching filter
 *
 * WHAT: Remove events that match predicate
 * WHY:  Cancel pending events of specific type/source
 * HOW:  Scan heap, remove matching, re-heapify
 *
 * COMPLEXITY: O(n log n)
 * THREAD-SAFE: Yes
 *
 * @param queue Event queue
 * @param filter Predicate function (true = remove)
 * @param context User context for filter
 * @return Number of events removed
 */
typedef bool (*event_filter_fn)(const event_t* event, void* context);
uint32_t event_queue_remove_if(event_queue_t queue, event_filter_fn filter, void* context);

/**
 * @brief Count events matching filter
 *
 * COMPLEXITY: O(n)
 * THREAD-SAFE: Yes
 *
 * @param queue Event queue
 * @param filter Predicate function (true = count)
 * @param context User context
 * @return Number of matching events
 */
uint32_t event_queue_count_if(event_queue_t queue, event_filter_fn filter, void* context);

//=============================================================================
// Statistics and Monitoring
//=============================================================================

/**
 * @brief Get queue statistics
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 *
 * @param queue Event queue
 * @param stats Output statistics
 * @return true on success
 */
bool event_queue_get_stats(event_queue_t queue, event_queue_stats_t* stats);

/**
 * @brief Reset statistics
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 *
 * @param queue Event queue
 */
void event_queue_reset_stats(event_queue_t queue);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get default configuration
 *
 * @return Default config with sensible values
 */
event_queue_config_t event_queue_default_config(void);

/**
 * @brief Get last error message
 *
 * @return Error string (thread-local)
 */
const char* event_queue_get_last_error(void);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_EVENT_QUEUE_H
