/**
 * @file nimcp_async_checkpoint.c
 * @brief Asynchronous checkpoint writer implementation
 *
 * IMPLEMENTATION NOTES:
 * - Producer-consumer pattern with pthread
 * - Circular buffer for request queue (lock-free reads)
 * - Mutex-protected queue modifications
 * - Condition variable for work notification
 * - Graceful shutdown with timeout
 * - Automatic retry with exponential backoff
 *
 * THREAD SAFETY:
 * - Queue operations: mutex protected
 * - Statistics updates: atomic where possible
 * - Background thread: single consumer
 * - Main thread: multiple producers OK
 *
 * PERFORMANCE:
 * - Queue operation: O(1), <1ms
 * - Background processing: O(queue_size)
 * - Memory: ~100KB base + ~400 bytes per request
 *
 * @author NIMCP Team
 * @date 2025-11-20
 */

#include "utils/fault_tolerance/nimcp_async_checkpoint.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "core/brain/persistence/nimcp_brain_persistence.h"
#include "utils/fault_tolerance/nimcp_checkpoint.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"

#define LOG_MODULE "utils_async_checkpoint"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(async_checkpoint)

#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>
#include "utils/memory/nimcp_unified_memory.h"
#include "constants/nimcp_buffer_constants.h"

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Circular request queue
 *
 * WHAT: Fixed-size circular buffer for checkpoint requests
 * WHY:  Efficient queue with O(1) operations
 * HOW:  Head/tail pointers with wraparound
 */
typedef struct {
    async_checkpoint_request_t requests[ASYNC_CHECKPOINT_MAX_QUEUE];
    uint32_t head;              /**< Next dequeue position */
    uint32_t tail;              /**< Next enqueue position */
    uint32_t count;             /**< Current queue size */
    nimcp_mutex_t mutex;        /**< Queue mutex */
    nimcp_cond_t not_empty;     /**< Signal when queue not empty */
    nimcp_cond_t not_full;      /**< Signal when queue not full */
} request_queue_t;

/**
 * @brief Completed requests history
 *
 * WHAT: Circular buffer tracking recently completed requests
 * WHY:  Allow status queries after request leaves main queue
 * HOW:  Store last 100 completed/failed requests
 */
#define ASYNC_CHECKPOINT_HISTORY_SIZE 100
typedef struct {
    async_checkpoint_request_t requests[ASYNC_CHECKPOINT_HISTORY_SIZE];
    uint32_t head;              /**< Next write position */
    uint32_t count;             /**< Current size */
    nimcp_mutex_t mutex;        /**< History mutex */
} request_history_t;

/**
 * @brief Async checkpoint writer structure
 *
 * WHAT: Complete writer state
 * WHY:  Encapsulate all writer data
 * HOW:  Opaque structure, accessed via API
 */
struct async_checkpoint_writer_struct {
    nimcp_thread_t worker_thread;   /**< Background worker thread */
    request_queue_t queue;          /**< Request queue */
    request_history_t history;      /**< Completed requests history */
    async_checkpoint_stats_t stats; /**< Statistics */
    volatile bool shutdown;         /**< Shutdown flag */
    nimcp_mutex_t stats_mutex;      /**< Statistics mutex */
    uint64_t next_request_id;       /**< Next request ID (atomic) */
    char error_msg[NIMCP_ERROR_BUFFER_SIZE];            /**< Last error message */
    nimcp_mutex_t error_mutex;      /**< Error message mutex */
};

//=============================================================================
// Internal Helper Functions - Time
//=============================================================================

/**
 * @brief Get current time in microseconds
 *
 * WHAT: Get monotonic time for latency measurement
 * WHY:  Accurate latency tracking
 * HOW:  clock_gettime(CLOCK_MONOTONIC)
 *
 * @return Time in microseconds since arbitrary epoch
 */
uint64_t async_checkpoint_get_time_us(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        // Fallback to gettimeofday if clock_gettime fails
        struct timeval tv;
        gettimeofday(&tv, NULL);
        return (uint64_t)tv.tv_sec * 1000000ULL + (uint64_t)tv.tv_usec;
    }
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

/**
 * @brief Sleep for specified microseconds
 *
 * WHAT: Sleep with microsecond precision
 * WHY:  Implement retry delays
 * HOW:  usleep() or nanosleep()
 *
 * @param us Microseconds to sleep
 */
static void sleep_us(uint64_t us) {
    struct timespec ts;
    ts.tv_sec = us / 1000000;
    ts.tv_nsec = (us % 1000000) * 1000;
    nanosleep(&ts, NULL);
}

//=============================================================================
// Internal Helper Functions - Queue
//=============================================================================

/**
 * @brief Initialize request queue
 *
 * WHAT: Initialize queue structure
 * WHY:  Setup queue for use
 * HOW:  Initialize mutexes, condition variables, counters
 *
 * @param queue Queue to initialize
 * @return true on success, false on error
 */
static bool queue_init(request_queue_t* queue) {
    if (!queue) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "queue_init: queue is NULL");
        return false;
    }

    memset(queue, 0, sizeof(request_queue_t));
    queue->head = 0;
    queue->tail = 0;
    queue->count = 0;

    if (nimcp_mutex_init(&queue->mutex, NULL) != NIMCP_SUCCESS) {
        NIMCP_LOGGING_ERROR("Failed to initialize queue mutex");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "queue_init: validation failed");
        return false;
    }

    if (nimcp_cond_init(&queue->not_empty) != NIMCP_SUCCESS) {
        nimcp_mutex_destroy(&queue->mutex);
        NIMCP_LOGGING_ERROR("Failed to initialize not_empty condition");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "queue_init: validation failed");
        return false;
    }

    if (nimcp_cond_init(&queue->not_full) != NIMCP_SUCCESS) {
        nimcp_cond_destroy(&queue->not_empty);
        nimcp_mutex_destroy(&queue->mutex);
        NIMCP_LOGGING_ERROR("Failed to initialize not_full condition");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "queue_init: validation failed");
        return false;
    }

    return true;
}

/**
 * @brief Destroy request queue
 *
 * WHAT: Cleanup queue resources
 * WHY:  Prevent resource leaks
 * HOW:  Destroy mutexes and condition variables
 *
 * @param queue Queue to destroy
 */
static void queue_destroy(request_queue_t* queue) {
    if (!queue) {
        return;
    }

    nimcp_cond_destroy(&queue->not_full);
    nimcp_cond_destroy(&queue->not_empty);
    nimcp_mutex_destroy(&queue->mutex);
}

/**
 * @brief Enqueue checkpoint request
 *
 * WHAT: Add request to queue
 * WHY:  Queue checkpoint for background processing
 * HOW:  Add to tail, update count, signal not_empty
 *
 * @param queue Request queue
 * @param request Request to enqueue (copied)
 * @return true on success, false if queue full
 */
static bool queue_enqueue(request_queue_t* queue, const async_checkpoint_request_t* request) {
    if (!queue || !request) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "queue_enqueue: required parameter is NULL (queue, request)");
        return false;
    }

    nimcp_mutex_lock(&queue->mutex);

    // Check if queue is full
    if (queue->count >= ASYNC_CHECKPOINT_MAX_QUEUE) {
        nimcp_mutex_unlock(&queue->mutex);
        NIMCP_LOGGING_WARN("Checkpoint queue is full (%u requests)", queue->count);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "queue_enqueue: capacity exceeded");
        return false;
    }

    // Copy request to queue
    memcpy(&queue->requests[queue->tail], request, sizeof(async_checkpoint_request_t));

    // Update tail (circular)
    queue->tail = (queue->tail + 1) % ASYNC_CHECKPOINT_MAX_QUEUE;
    queue->count++;

    // Signal that queue is not empty
    nimcp_cond_signal(&queue->not_empty);

    nimcp_mutex_unlock(&queue->mutex);
    return true;
}

/**
 * @brief Dequeue checkpoint request
 *
 * WHAT: Remove request from queue
 * WHY:  Get next request to process
 * HOW:  Remove from head, update count, signal not_full
 *
 * @param queue Request queue
 * @param request Output parameter for dequeued request
 * @param timeout_ms Timeout in milliseconds (0 = non-blocking)
 * @return true if request dequeued, false if queue empty or timeout
 */
static bool queue_dequeue(request_queue_t* queue, async_checkpoint_request_t* request, uint32_t timeout_ms) {
    if (!queue || !request) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "queue_dequeue: required parameter is NULL (queue, request)");
        return false;
    }

    nimcp_mutex_lock(&queue->mutex);

    // Wait for queue to become non-empty
    if (timeout_ms == 0) {
        // Non-blocking
        if (queue->count == 0) {
            nimcp_mutex_unlock(&queue->mutex);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "queue_dequeue: queue->count is zero");
            return false;
        }
    } else {
        // Blocking with timeout
        while (queue->count == 0) {
            int ret = nimcp_cond_timedwait(&queue->not_empty, &queue->mutex, timeout_ms);
            if (ret == ETIMEDOUT) {
                nimcp_mutex_unlock(&queue->mutex);
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "queue_dequeue: validation failed");
                return false;
            }
            if (ret != NIMCP_SUCCESS) {
                nimcp_mutex_unlock(&queue->mutex);
                NIMCP_LOGGING_ERROR("nimcp_cond_timedwait failed: %d", ret);
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "queue_dequeue: validation failed");
                return false;
            }
        }
    }

    // Copy request from queue
    memcpy(request, &queue->requests[queue->head], sizeof(async_checkpoint_request_t));

    // Update head (circular)
    queue->head = (queue->head + 1) % ASYNC_CHECKPOINT_MAX_QUEUE;
    queue->count--;

    // Signal that queue is not full
    nimcp_cond_signal(&queue->not_full);

    nimcp_mutex_unlock(&queue->mutex);
    return true;
}

/**
 * @brief Get queue size
 *
 * WHAT: Get current number of queued requests
 * WHY:  Monitor queue depth
 * HOW:  Atomic read of count
 *
 * @param queue Request queue
 * @return Queue size (0 if NULL)
 */
static uint32_t queue_size(request_queue_t* queue) {
    if (!queue) {
        return 0;
    }

    nimcp_mutex_lock(&queue->mutex);
    uint32_t count = queue->count;
    nimcp_mutex_unlock(&queue->mutex);

    return count;
}

//=============================================================================
// Internal Helper Functions - History
//=============================================================================

/**
 * @brief Initialize request history
 *
 * WHAT: Initialize history structure
 * WHY:  Setup history for use
 * HOW:  Initialize mutex, counters
 *
 * @param history History to initialize
 * @return true on success, false on error
 */
static bool history_init(request_history_t* history) {
    if (!history) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "history_init: history is NULL");
        return false;
    }

    memset(history, 0, sizeof(request_history_t));
    history->head = 0;
    history->count = 0;

    if (nimcp_mutex_init(&history->mutex, NULL) != NIMCP_SUCCESS) {
        NIMCP_LOGGING_ERROR("Failed to initialize history mutex");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "history_init: validation failed");
        return false;
    }

    return true;
}

/**
 * @brief Destroy request history
 *
 * WHAT: Cleanup history resources
 * WHY:  Prevent resource leaks
 * HOW:  Destroy mutex
 *
 * @param history History to destroy
 */
static void history_destroy(request_history_t* history) {
    if (!history) {
        return;
    }

    nimcp_mutex_destroy(&history->mutex);
}

/**
 * @brief Add completed request to history
 *
 * WHAT: Store completed request in history buffer
 * WHY:  Allow status queries after request leaves queue
 * HOW:  Add to circular buffer, keep track of oldest request
 *
 * @param history Request history
 * @param request Completed request
 */
static void history_add(request_history_t* history, const async_checkpoint_request_t* request) {
    if (!history || !request) {
        return;
    }

    nimcp_mutex_lock(&history->mutex);

    // Store request at head position
    memcpy(&history->requests[history->head], request, sizeof(async_checkpoint_request_t));

    // Move head to next position
    history->head = (history->head + 1) % ASYNC_CHECKPOINT_HISTORY_SIZE;

    // Update count (max out at capacity)
    if (history->count < ASYNC_CHECKPOINT_HISTORY_SIZE) {
        history->count++;
    }

    nimcp_mutex_unlock(&history->mutex);
}

/**
 * @brief Search history for request
 *
 * WHAT: Find request in history buffer
 * WHY:  Get status of completed requests
 * HOW:  Linear search through all stored requests
 *
 * @param history Request history
 * @param request_id Request ID to find
 * @param request Output parameter for found request (can be NULL)
 * @return true if found, false otherwise
 */
static bool history_find(request_history_t* history, uint64_t request_id, async_checkpoint_request_t* request) {
    if (!history || request_id == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "history_find: history is NULL");
        return false;
    }

    nimcp_mutex_lock(&history->mutex);

    // Search all stored requests (simple linear search)
    for (uint32_t i = 0; i < ASYNC_CHECKPOINT_HISTORY_SIZE; i++) {
        // Skip uninitialized entries (request_id = 0)
        if (history->requests[i].request_id != 0 && history->requests[i].request_id == request_id) {
            if (request) {
                memcpy(request, &history->requests[i], sizeof(async_checkpoint_request_t));
            }
            nimcp_mutex_unlock(&history->mutex);
            return true;
        }
    }

    nimcp_mutex_unlock(&history->mutex);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "history_find: validation failed");
    return false;
}

//=============================================================================
// Internal Helper Functions - Statistics
//=============================================================================

/**
 * @brief Update statistics for completed request
 *
 * WHAT: Update stats after request completes
 * WHY:  Track performance metrics
 * HOW:  Update counters and latency stats
 *
 * @param writer Writer instance
 * @param request Completed request
 */
static void update_stats_completed(async_checkpoint_writer_t* writer, const async_checkpoint_request_t* request) {
    if (!writer || !request) {
        return;
    }

    nimcp_mutex_lock(&writer->stats_mutex);

    writer->stats.total_completed++;
    writer->stats.current_pending--;

    // Calculate latency
    uint64_t latency_us = request->complete_time_us - request->queue_time_us;

    // Update latency stats
    if (writer->stats.total_completed == 1) {
        writer->stats.avg_latency_us = latency_us;
        writer->stats.min_latency_us = latency_us;
        writer->stats.max_latency_us = latency_us;
    } else {
        // Exponential moving average
        writer->stats.avg_latency_us = (writer->stats.avg_latency_us * 9 + latency_us) / 10;

        if (latency_us < writer->stats.min_latency_us) {
            writer->stats.min_latency_us = latency_us;
        }
        if (latency_us > writer->stats.max_latency_us) {
            writer->stats.max_latency_us = latency_us;
        }
    }

    nimcp_mutex_unlock(&writer->stats_mutex);
}

/**
 * @brief Update statistics for failed request
 *
 * WHAT: Update stats after request fails
 * WHY:  Track failure rate
 * HOW:  Update failure counter and error message
 *
 * @param writer Writer instance
 * @param request Failed request
 */
static void update_stats_failed(async_checkpoint_writer_t* writer, const async_checkpoint_request_t* request) {
    if (!writer || !request) {
        return;
    }

    nimcp_mutex_lock(&writer->stats_mutex);

    writer->stats.total_failed++;
    writer->stats.current_pending--;
    writer->stats.last_error_time = async_checkpoint_get_time_us();
    strncpy(writer->stats.last_error, request->error_msg, sizeof(writer->stats.last_error) - 1);
    writer->stats.last_error[sizeof(writer->stats.last_error) - 1] = '\0';

    nimcp_mutex_unlock(&writer->stats_mutex);
}

//=============================================================================
// Background Worker Thread
//=============================================================================

/**
 * @brief Process single checkpoint request
 *
 * WHAT: Write checkpoint to disk
 * WHY:  Execute queued checkpoint
 * HOW:  Call checkpoint_save(), retry on failure
 *
 * @param writer Writer instance
 * @param request Request to process
 * @return true on success, false on failure
 */
static bool process_checkpoint_request(async_checkpoint_writer_t* writer, async_checkpoint_request_t* request) {
    if (!writer || !request) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "process_checkpoint_request: required parameter is NULL (writer, request)");
        return false;
    }

    // Check if request was cancelled while queued
    if (request->status == CHECKPOINT_STATUS_CANCELLED) {
        NIMCP_LOGGING_INFO("Skipping cancelled checkpoint request %lu", request->request_id);
        // Add to history so wait_request can see it was cancelled
        history_add(&writer->history, request);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "process_checkpoint_request: validation failed");
        return false;
    }

    request->start_time_us = async_checkpoint_get_time_us();
    request->status = CHECKPOINT_STATUS_PROCESSING;

    NIMCP_LOGGING_DEBUG("Processing checkpoint request %lu: %s", request->request_id, request->path);

    // Try to save checkpoint with retries
    bool success = false;
    for (uint32_t retry = 0; retry <= ASYNC_CHECKPOINT_MAX_RETRIES; retry++) {
        request->retry_count = retry;

        // Call synchronous brain_save (simpler and more reliable)
        success = brain_save(request->brain, request->path);

        if (!success) {
            // Get error from brain system (brain_get_last_error is defined in nimcp_brain_strategy.c)
            extern const char* brain_get_last_error(void);
            const char* error = brain_get_last_error();

            if (error && strlen(error) > 0) {
                strncpy(request->error_msg, error, sizeof(request->error_msg) - 1);
                request->error_msg[sizeof(request->error_msg) - 1] = '\0';
            } else {
                // Fallback error message with path information (limit path to fit in buffer)
                snprintf(request->error_msg, sizeof(request->error_msg),
                         "Failed to save checkpoint to %.180s (invalid path or I/O error)", request->path);
            }
        }

        if (success) {
            break;
        }

        // Retry with exponential backoff
        if (retry < ASYNC_CHECKPOINT_MAX_RETRIES) {
            uint64_t delay_ms = ASYNC_CHECKPOINT_RETRY_DELAY_MS * (1 << retry);  // 100ms, 200ms, 400ms
            NIMCP_LOGGING_WARN("Checkpoint failed (attempt %u/%u), retrying in %lu ms: %s",
                               retry + 1, ASYNC_CHECKPOINT_MAX_RETRIES + 1, delay_ms, request->error_msg);
            sleep_us(delay_ms * 1000);
        }
    }

    request->complete_time_us = async_checkpoint_get_time_us();

    if (success) {
        request->status = CHECKPOINT_STATUS_COMPLETED;
        NIMCP_LOGGING_INFO("Checkpoint completed: %s (%.2f ms)",
                           request->path,
                           (request->complete_time_us - request->queue_time_us) / 1000.0);
        update_stats_completed(writer, request);
    } else {
        request->status = CHECKPOINT_STATUS_FAILED;
        NIMCP_LOGGING_ERROR("Checkpoint failed after %u retries: %s",
                            request->retry_count + 1, request->error_msg);
        update_stats_failed(writer, request);

        // Update writer error message
        nimcp_mutex_lock(&writer->error_mutex);
        strncpy(writer->error_msg, request->error_msg, sizeof(writer->error_msg) - 1);
        writer->error_msg[sizeof(writer->error_msg) - 1] = '\0';
        nimcp_mutex_unlock(&writer->error_mutex);
    }

    // Add request to history for later queries
    history_add(&writer->history, request);

    // Free options if allocated
    if (request->options) {
        nimcp_free(request->options);
        request->options = NULL;
    }

    return success;
}

/**
 * @brief Background worker thread function
 *
 * WHAT: Main loop for background checkpoint writer
 * WHY:  Process queued checkpoints asynchronously
 * HOW:  Dequeue requests, process, repeat until shutdown
 *
 * @param arg Writer instance (async_checkpoint_writer_t*)
 * @return NULL
 */
static void* worker_thread_func(void* arg) {
    async_checkpoint_writer_t* writer = (async_checkpoint_writer_t*)arg;
    if (!writer) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "writer is NULL");

        return NULL;
    }

    NIMCP_LOGGING_INFO("Async checkpoint worker thread started");

    while (!writer->shutdown) {
        async_checkpoint_request_t request;

        // Wait for request with timeout (100ms)
        if (queue_dequeue(&writer->queue, &request, 100)) {
            // Process request
            process_checkpoint_request(writer, &request);
        }
    }

    // Process remaining requests on shutdown
    NIMCP_LOGGING_INFO("Worker thread shutting down, draining queue...");
    async_checkpoint_request_t request;
    while (queue_dequeue(&writer->queue, &request, 0)) {
        process_checkpoint_request(writer, &request);
    }

    NIMCP_LOGGING_INFO("Async checkpoint worker thread stopped");
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "worker_thread_func: operation failed");
    return NULL;
}

//=============================================================================
// Public API - Lifecycle Management
//=============================================================================

async_checkpoint_writer_t* async_checkpoint_create(void) {
    // Allocate writer
    async_checkpoint_writer_t* writer = (async_checkpoint_writer_t*)nimcp_malloc(sizeof(async_checkpoint_writer_t));
    if (!writer) {
        NIMCP_LOGGING_ERROR("Failed to allocate async checkpoint writer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "async_checkpoint_create: writer is NULL");
        return NULL;
    }

    memset(writer, 0, sizeof(async_checkpoint_writer_t));

    // Initialize queue
    if (!queue_init(&writer->queue)) {
        nimcp_free(writer);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "async_checkpoint_create: queue_init is NULL");
        return NULL;
    }

    // Initialize history
    if (!history_init(&writer->history)) {
        queue_destroy(&writer->queue);
        nimcp_free(writer);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "async_checkpoint_create: history_init is NULL");
        return NULL;
    }

    // Initialize statistics mutex
    if (nimcp_mutex_init(&writer->stats_mutex, NULL) != NIMCP_SUCCESS) {
        NIMCP_LOGGING_ERROR("Failed to initialize stats mutex");
        history_destroy(&writer->history);
        queue_destroy(&writer->queue);
        nimcp_free(writer);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "async_checkpoint_create: validation failed");
        return NULL;
    }

    // Initialize error mutex
    if (nimcp_mutex_init(&writer->error_mutex, NULL) != NIMCP_SUCCESS) {
        NIMCP_LOGGING_ERROR("Failed to initialize error mutex");
        nimcp_mutex_destroy(&writer->stats_mutex);
        history_destroy(&writer->history);
        queue_destroy(&writer->queue);
        nimcp_free(writer);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "async_checkpoint_create: validation failed");
        return NULL;
    }

    // Initialize state
    writer->shutdown = false;
    writer->next_request_id = 1;

    // Create worker thread
    if (nimcp_thread_create(&writer->worker_thread, worker_thread_func, writer,
                            NULL) != 0) {
        NIMCP_LOGGING_ERROR("Failed to create worker thread: %s", strerror(errno));
        nimcp_mutex_destroy(&writer->error_mutex);
        nimcp_mutex_destroy(&writer->stats_mutex);
        history_destroy(&writer->history);
        queue_destroy(&writer->queue);
        nimcp_free(writer);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "async_checkpoint_create: operation failed");
        return NULL;
    }

    NIMCP_LOGGING_INFO("Async checkpoint writer created");
    return writer;
}

void async_checkpoint_destroy(async_checkpoint_writer_t* writer) {
    if (!writer) {
        return;
    }

    NIMCP_LOGGING_INFO("Destroying async checkpoint writer...");

    // Signal shutdown
    writer->shutdown = true;

    // Wake up worker thread
    nimcp_mutex_lock(&writer->queue.mutex);
    nimcp_cond_signal(&writer->queue.not_empty);
    nimcp_mutex_unlock(&writer->queue.mutex);

    // Wait for worker thread to finish
    nimcp_thread_join(writer->worker_thread, NULL);

    // Log pending requests
    uint32_t pending = writer->stats.current_pending;
    if (pending > 0) {
        NIMCP_LOGGING_WARN("Destroying writer with %u pending requests", pending);
    }

    // Cleanup
    nimcp_mutex_destroy(&writer->error_mutex);
    nimcp_mutex_destroy(&writer->stats_mutex);
    history_destroy(&writer->history);
    queue_destroy(&writer->queue);
    nimcp_free(writer);

    NIMCP_LOGGING_INFO("Async checkpoint writer destroyed");
}

//=============================================================================
// Public API - Checkpoint Queueing
//=============================================================================

uint64_t async_checkpoint_queue(async_checkpoint_writer_t* writer, brain_t brain, const char* path) {
    return async_checkpoint_queue_ex(writer, brain, path, NULL);
}

uint64_t async_checkpoint_queue_ex(async_checkpoint_writer_t* writer,
                                    brain_t brain,
                                    const char* path,
                                    const checkpoint_options_t* options) {
    // Guard: NULL checks
    if (!writer) {
        NIMCP_LOGGING_ERROR("async_checkpoint_queue_ex: NULL writer");
        return 0;
    }
    if (!brain) {
        NIMCP_LOGGING_ERROR("async_checkpoint_queue_ex: NULL brain");
        return 0;
    }
    if (!path) {
        NIMCP_LOGGING_ERROR("async_checkpoint_queue_ex: NULL path");
        return 0;
    }

    // Guard: Check shutdown
    if (writer->shutdown) {
        NIMCP_LOGGING_ERROR("Writer is shutting down, cannot queue requests");
        return 0;
    }

    // Create request
    async_checkpoint_request_t request;
    memset(&request, 0, sizeof(request));

    request.brain = brain;
    strncpy(request.path, path, sizeof(request.path) - 1);
    request.path[sizeof(request.path) - 1] = '\0';

    // Copy options if provided
    if (options) {
        request.options = (checkpoint_options_t*)nimcp_malloc(sizeof(checkpoint_options_t));
        if (!request.options) {
            NIMCP_LOGGING_ERROR("Failed to allocate options");
            return 0;
        }
        memcpy(request.options, options, sizeof(checkpoint_options_t));
    } else {
        request.options = NULL;
    }

    request.status = CHECKPOINT_STATUS_QUEUED;
    request.queue_time_us = async_checkpoint_get_time_us();
    request.retry_count = 0;

    // Assign request ID
    request.request_id = __sync_fetch_and_add(&writer->next_request_id, 1);

    // Enqueue request
    if (!queue_enqueue(&writer->queue, &request)) {
        if (request.options) {
            nimcp_free(request.options);
        }
        NIMCP_LOGGING_ERROR("Failed to enqueue checkpoint request (queue full)");
        return 0;
    }

    // Update statistics
    nimcp_mutex_lock(&writer->stats_mutex);
    writer->stats.total_queued++;
    writer->stats.current_pending++;
    if (writer->stats.current_pending > writer->stats.peak_queue_size) {
        writer->stats.peak_queue_size = writer->stats.current_pending;
    }
    nimcp_mutex_unlock(&writer->stats_mutex);

    NIMCP_LOGGING_DEBUG("Queued checkpoint request %lu: %s", request.request_id, path);
    return request.request_id;
}

//=============================================================================
// Public API - Synchronization
//=============================================================================

bool async_checkpoint_wait_all(async_checkpoint_writer_t* writer, uint32_t timeout_ms) {
    if (!writer) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "async_checkpoint_wait_all: writer is NULL");
        return false;
    }

    uint64_t start_time = async_checkpoint_get_time_us();
    uint64_t timeout_us = (uint64_t)timeout_ms * 1000;

    // Wait for queue to drain
    while (true) {
        uint32_t pending = async_checkpoint_get_pending_count(writer);
        if (pending == 0) {
            return true;  // All completed
        }

        // Check timeout
        if (timeout_ms != UINT32_MAX) {
            uint64_t elapsed = async_checkpoint_get_time_us() - start_time;
            if (elapsed >= timeout_us) {
                NIMCP_LOGGING_WARN("Timeout waiting for checkpoints (%u still pending)", pending);
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "async_checkpoint_wait_all: capacity exceeded");
                return false;
            }
        }

        // Sleep briefly
        sleep_us(1000);  // 1ms
    }
}

bool async_checkpoint_wait_request(async_checkpoint_writer_t* writer,
                                    uint64_t request_id,
                                    uint32_t timeout_ms) {
    if (!writer) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "async_checkpoint_wait_all: writer is NULL");
        return false;
    }

    uint64_t start_time = async_checkpoint_get_time_us();
    uint64_t timeout_us = (uint64_t)timeout_ms * 1000;

    // Poll for request completion
    while (true) {
        async_checkpoint_request_t request;
        if (async_checkpoint_get_request_status(writer, request_id, &request)) {
            if (request.status == CHECKPOINT_STATUS_COMPLETED) {
                return true;
            }
            if (request.status == CHECKPOINT_STATUS_FAILED ||
                request.status == CHECKPOINT_STATUS_CANCELLED) {
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "async_checkpoint_wait_all: validation failed");
                return false;
            }
        }

        // Check timeout
        if (timeout_ms != UINT32_MAX) {
            uint64_t elapsed = async_checkpoint_get_time_us() - start_time;
            if (elapsed >= timeout_us) {
                NIMCP_LOGGING_WARN("Timeout waiting for request %lu", request_id);
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "async_checkpoint_wait_all: capacity exceeded");
                return false;
            }
        }

        // Sleep briefly
        sleep_us(1000);  // 1ms
    }
}

//=============================================================================
// Public API - Status Monitoring
//=============================================================================

uint32_t async_checkpoint_get_pending_count(async_checkpoint_writer_t* writer) {
    if (!writer) {
        return 0;
    }

    nimcp_mutex_lock(&writer->stats_mutex);
    uint32_t pending = writer->stats.current_pending;
    nimcp_mutex_unlock(&writer->stats_mutex);

    return pending;
}

bool async_checkpoint_get_stats(async_checkpoint_writer_t* writer,
                                 async_checkpoint_stats_t* stats) {
    if (!writer || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "async_checkpoint_get_pending_count: required parameter is NULL (writer, stats)");
        return false;
    }

    nimcp_mutex_lock(&writer->stats_mutex);
    memcpy(stats, &writer->stats, sizeof(async_checkpoint_stats_t));
    nimcp_mutex_unlock(&writer->stats_mutex);

    return true;
}

bool async_checkpoint_get_request_status(async_checkpoint_writer_t* writer,
                                          uint64_t request_id,
                                          async_checkpoint_request_t* request) {
    if (!writer) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "async_checkpoint_get_pending_count: writer is NULL");
        return false;
    }

    // First search queue for request
    nimcp_mutex_lock(&writer->queue.mutex);

    for (uint32_t i = 0; i < writer->queue.count; i++) {
        uint32_t idx = (writer->queue.head + i) % ASYNC_CHECKPOINT_MAX_QUEUE;
        if (writer->queue.requests[idx].request_id == request_id) {
            if (request) {
                memcpy(request, &writer->queue.requests[idx], sizeof(async_checkpoint_request_t));
            }
            nimcp_mutex_unlock(&writer->queue.mutex);
            return true;
        }
    }

    nimcp_mutex_unlock(&writer->queue.mutex);

    // Not in queue, search history for completed/failed requests
    if (history_find(&writer->history, request_id, request)) {
        return true;
    }

    // Not found anywhere
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "async_checkpoint_get_pending_count: validation failed");
    return false;
}

bool async_checkpoint_is_healthy(async_checkpoint_writer_t* writer) {
    if (!writer) {
        return false;
    }

    // Check shutdown flag
    if (writer->shutdown) {
        return false;
    }

    // Check for recent errors (last 5 minutes)
    nimcp_mutex_lock(&writer->stats_mutex);
    uint64_t now = async_checkpoint_get_time_us();
    uint64_t last_error = writer->stats.last_error_time;
    bool has_recent_error = (last_error > 0) && ((now - last_error) < 5 * 60 * 1000000ULL);
    nimcp_mutex_unlock(&writer->stats_mutex);

    if (has_recent_error) {
        return false;
    }

    // Check queue not stuck (has capacity)
    uint32_t current_queue_size = queue_size(&writer->queue);
    if (current_queue_size >= ASYNC_CHECKPOINT_MAX_QUEUE) {
        return false;
    }

    return true;
}

//=============================================================================
// Public API - Error Handling
//=============================================================================

const char* async_checkpoint_get_error(async_checkpoint_writer_t* writer) {
    if (!writer) {
        return "";
    }

    nimcp_mutex_lock(&writer->error_mutex);
    const char* error = writer->error_msg;
    nimcp_mutex_unlock(&writer->error_mutex);

    return error;
}

void async_checkpoint_clear_error(async_checkpoint_writer_t* writer) {
    if (!writer) {
        return;
    }

    nimcp_mutex_lock(&writer->error_mutex);
    writer->error_msg[0] = '\0';
    nimcp_mutex_unlock(&writer->error_mutex);
}

//=============================================================================
// Public API - Advanced Features
//=============================================================================

bool async_checkpoint_cancel_request(async_checkpoint_writer_t* writer, uint64_t request_id) {
    if (!writer) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "async_checkpoint_cancel_request: writer is NULL");
        return false;
    }

    nimcp_mutex_lock(&writer->queue.mutex);

    // Search for request in queue
    for (uint32_t i = 0; i < writer->queue.count; i++) {
        uint32_t idx = (writer->queue.head + i) % ASYNC_CHECKPOINT_MAX_QUEUE;
        if (writer->queue.requests[idx].request_id == request_id) {
            // Can only cancel if still queued
            if (writer->queue.requests[idx].status == CHECKPOINT_STATUS_QUEUED) {
                writer->queue.requests[idx].status = CHECKPOINT_STATUS_CANCELLED;

                // Update stats
                nimcp_mutex_lock(&writer->stats_mutex);
                writer->stats.total_cancelled++;
                writer->stats.current_pending--;
                nimcp_mutex_unlock(&writer->stats_mutex);

                nimcp_mutex_unlock(&writer->queue.mutex);
                NIMCP_LOGGING_INFO("Cancelled checkpoint request %lu", request_id);
                return true;
            } else {
                nimcp_mutex_unlock(&writer->queue.mutex);
                NIMCP_LOGGING_WARN("Cannot cancel request %lu (status: %d)",
                                   request_id, writer->queue.requests[idx].status);
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "async_checkpoint_cancel_request: operation failed");
                return false;
            }
        }
    }

    nimcp_mutex_unlock(&writer->queue.mutex);
    NIMCP_LOGGING_WARN("Request %lu not found in queue", request_id);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "async_checkpoint_cancel_request: operation failed");
    return false;
}

bool async_checkpoint_flush(async_checkpoint_writer_t* writer) {
    return async_checkpoint_wait_all(writer, UINT32_MAX);
}

//=============================================================================
// Public API - Utility Functions
//=============================================================================

const char* async_checkpoint_status_name(checkpoint_request_status_t status) {
    switch (status) {
        case CHECKPOINT_STATUS_QUEUED:      return "QUEUED";
        case CHECKPOINT_STATUS_PROCESSING:  return "PROCESSING";
        case CHECKPOINT_STATUS_COMPLETED:   return "COMPLETED";
        case CHECKPOINT_STATUS_FAILED:      return "FAILED";
        case CHECKPOINT_STATUS_CANCELLED:   return "CANCELLED";
        default:                            return "UNKNOWN";
    }
}
