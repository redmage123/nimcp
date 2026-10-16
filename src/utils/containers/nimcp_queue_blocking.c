/**
 * @file nimcp_queue_blocking.c
 * @brief Mutex-based blocking queue implementation
 *
 * WHAT: Thread-safe queue using mutex + condition variables
 * WHY:  General purpose, works with any number of producers/consumers
 * HOW:  Circular buffer with mutex protection and condvar signaling
 *
 * PERFORMANCE: ~1-10us per operation (includes context switch if contended)
 * USE CASE: General purpose, unknown thread patterns
 *
 * @author NIMCP Development Team
 * @date 2025-11-28
 * @version 2.0.0
 */

#include "nimcp_queue_internal.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory_guards.h"  // For nimcp_calloc/nimcp_free
#include "utils/memory/nimcp_unified_memory.h"

#define LOG_MODULE "QUEUE_BLOCKING"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(queue_blocking)

//=============================================================================
// Internal Structure
//=============================================================================

/**
 * @brief Blocking queue implementation data
 */
typedef struct {
    size_t head;                  /**< Read position */
    size_t tail;                  /**< Write position */
    nimcp_mutex_t mutex;          /**< Protects all state */
    nimcp_cond_t not_empty;       /**< Signaled when item added */
    nimcp_cond_t not_full;        /**< Signaled when item removed */
} blocking_impl_t;

//=============================================================================
// Internal Helpers
//=============================================================================

static bool is_full(struct nimcp_queue* q, blocking_impl_t* impl) {
    return ((impl->tail + 1) % q->capacity) == impl->head;
}

static bool is_empty(blocking_impl_t* impl) {
    return impl->head == impl->tail;
}

//=============================================================================
// Create/Destroy
//=============================================================================

nimcp_result_t nimcp_queue_blocking_create(
    struct nimcp_queue* queue,
    const nimcp_queue_config_t* config
) {
    if (!queue || !config) {
        return NIMCP_INVALID_PARAM;
    }

    // Validate capacity - must be non-zero and not overflow when adding sentinel slot
    if (config->max_size == 0) {
        LOG_ERROR(LOG_MODULE, "Invalid queue capacity: max_size cannot be 0");
        return NIMCP_INVALID_PARAM;
    }

    if (config->item_size == 0) {
        LOG_ERROR(LOG_MODULE, "Invalid item_size: cannot be 0");
        return NIMCP_INVALID_PARAM;
    }

    // Check for overflow when computing buffer size: (max_size + 1) * item_size
    if (config->max_size > SIZE_MAX - 1) {
        LOG_ERROR(LOG_MODULE, "Capacity overflow: max_size too large");
        return NIMCP_INVALID_PARAM;
    }
    size_t capacity_with_sentinel = config->max_size + 1;
    if (capacity_with_sentinel > SIZE_MAX / config->item_size) {
        LOG_ERROR(LOG_MODULE, "Buffer size overflow: capacity * item_size too large");
        return NIMCP_INVALID_PARAM;
    }

    LOG_DEBUG(LOG_MODULE, "Creating blocking queue (capacity=%zu, item_size=%zu)",
              config->max_size, config->item_size);

    // Allocate implementation data
    blocking_impl_t* impl = nimcp_calloc(1, sizeof(blocking_impl_t));
    if (!impl) {
        LOG_ERROR(LOG_MODULE, "Failed to allocate blocking_impl_t");
        return NIMCP_NO_MEMORY;
    }

    // Allocate buffer (extra slot to distinguish full from empty in circular buffer)
    // With capacity = max_size + 1, we can store max_size items
    queue->buffer = nimcp_malloc((config->max_size + 1) * config->item_size);
    if (!queue->buffer) {
        LOG_ERROR(LOG_MODULE, "Failed to allocate queue buffer");
        nimcp_free(impl);
        return NIMCP_NO_MEMORY;
    }

    // Initialize indices
    impl->head = 0;
    impl->tail = 0;

    // Initialize synchronization primitives
    if (nimcp_mutex_init(&impl->mutex, NULL) != NIMCP_SUCCESS) {
        LOG_ERROR(LOG_MODULE, "Failed to init mutex");
        nimcp_free(queue->buffer);
        nimcp_free(impl);
        return NIMCP_INIT_FAILED;
    }

    if (nimcp_cond_init(&impl->not_empty) != NIMCP_SUCCESS) {
        LOG_ERROR(LOG_MODULE, "Failed to init not_empty condvar");
        nimcp_mutex_destroy(&impl->mutex);
        nimcp_free(queue->buffer);
        nimcp_free(impl);
        return NIMCP_INIT_FAILED;
    }

    if (nimcp_cond_init(&impl->not_full) != NIMCP_SUCCESS) {
        LOG_ERROR(LOG_MODULE, "Failed to init not_full condvar");
        nimcp_cond_destroy(&impl->not_empty);
        nimcp_mutex_destroy(&impl->mutex);
        nimcp_free(queue->buffer);
        nimcp_free(impl);
        return NIMCP_INIT_FAILED;
    }

    // Store impl data
    queue->impl_data = impl;
    // Internal capacity includes sentinel slot, user-facing capacity is max_size
    queue->capacity = config->max_size + 1;

    LOG_INFO(LOG_MODULE, "Blocking queue created successfully (capacity=%zu)", config->max_size);
    return NIMCP_SUCCESS;
}

void nimcp_queue_blocking_destroy(struct nimcp_queue* queue) {
    if (!queue || !queue->impl_data) {
        return;
    }

    LOG_DEBUG(LOG_MODULE, "Destroying blocking queue");

    blocking_impl_t* impl = (blocking_impl_t*)queue->impl_data;

    nimcp_cond_destroy(&impl->not_full);
    nimcp_cond_destroy(&impl->not_empty);
    nimcp_mutex_destroy(&impl->mutex);

    nimcp_free(impl);
    queue->impl_data = NULL;

    if (queue->buffer) {
        nimcp_free(queue->buffer);
        queue->buffer = NULL;
    }

    LOG_INFO(LOG_MODULE, "Blocking queue destroyed");
}

//=============================================================================
// Queue Operations
//=============================================================================

nimcp_result_t nimcp_queue_blocking_enqueue(
    struct nimcp_queue* queue,
    const void* item,
    uint32_t timeout_ms
) {
    if (!queue || !item || !queue->impl_data) {
        return NIMCP_INVALID_PARAM;
    }

    blocking_impl_t* impl = (blocking_impl_t*)queue->impl_data;

    nimcp_mutex_lock(&impl->mutex);

    // Check if full
    if (is_full(queue, impl)) {
        if (!queue->config.is_blocking) {
            queue->status.enqueue_failures++;
            nimcp_mutex_unlock(&impl->mutex);
            return NIMCP_QUEUE_FULL;
        }

        // Blocking wait
        // timeout_ms == 0 means "try" (non-blocking), return immediately
        if (timeout_ms == 0) {
            queue->status.enqueue_failures++;
            nimcp_mutex_unlock(&impl->mutex);
            return NIMCP_QUEUE_FULL;
        } else if (timeout_ms == UINT32_MAX) {
            // Block indefinitely
            while (is_full(queue, impl)) {
                queue->status.blocking_waits++;
                nimcp_cond_wait(&impl->not_full, &impl->mutex);
            }
        } else {
            // Timed wait with spurious wakeup protection
            // Loop until queue has space or timeout occurs
            while (is_full(queue, impl)) {
                queue->status.blocking_waits++;
                nimcp_result_t result = nimcp_cond_timedwait(&impl->not_full, &impl->mutex, timeout_ms);
                if (result != NIMCP_SUCCESS) {
                    // Re-check condition after timeout - may have been signaled just before timeout
                    if (is_full(queue, impl)) {
                        queue->status.blocking_timeouts++;
                        nimcp_mutex_unlock(&impl->mutex);
                        return NIMCP_TIMEOUT;
                    }
                    // Queue has space now, exit loop and proceed
                    break;
                }
                // On success, loop will re-check condition to handle spurious wakeup
            }
        }
    }

    // Copy item to buffer
    memcpy(queue->buffer + (impl->tail * queue->config.item_size),
           item, queue->config.item_size);

    // Advance tail
    impl->tail = (impl->tail + 1) % queue->capacity;

    // Update statistics
    queue->status.current_size++;
    queue->status.total_enqueued++;
    if (queue->status.current_size > queue->status.peak_size) {
        queue->status.peak_size = queue->status.current_size;
    }

    // Signal consumer
    nimcp_cond_signal(&impl->not_empty);

    nimcp_mutex_unlock(&impl->mutex);
    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_queue_blocking_dequeue(
    struct nimcp_queue* queue,
    void* item,
    uint32_t timeout_ms
) {
    if (!queue || !item || !queue->impl_data) {
        return NIMCP_INVALID_PARAM;
    }

    blocking_impl_t* impl = (blocking_impl_t*)queue->impl_data;

    nimcp_mutex_lock(&impl->mutex);

    // Check if empty
    if (is_empty(impl)) {
        if (!queue->config.is_blocking) {
            queue->status.dequeue_failures++;
            nimcp_mutex_unlock(&impl->mutex);
            return NIMCP_QUEUE_EMPTY;
        }

        // Blocking wait
        // timeout_ms == 0 means "try" (non-blocking), return immediately
        if (timeout_ms == 0) {
            queue->status.dequeue_failures++;
            nimcp_mutex_unlock(&impl->mutex);
            return NIMCP_QUEUE_EMPTY;
        } else if (timeout_ms == UINT32_MAX) {
            // Block indefinitely
            while (is_empty(impl)) {
                queue->status.blocking_waits++;
                nimcp_cond_wait(&impl->not_empty, &impl->mutex);
            }
        } else {
            // Timed wait with spurious wakeup protection
            // Loop until queue has items or timeout occurs
            while (is_empty(impl)) {
                queue->status.blocking_waits++;
                nimcp_result_t result = nimcp_cond_timedwait(&impl->not_empty, &impl->mutex, timeout_ms);
                if (result != NIMCP_SUCCESS) {
                    // Re-check condition after timeout - may have been signaled just before timeout
                    if (is_empty(impl)) {
                        queue->status.blocking_timeouts++;
                        nimcp_mutex_unlock(&impl->mutex);
                        return NIMCP_TIMEOUT;
                    }
                    // Queue has items now, exit loop and proceed
                    break;
                }
                // On success, loop will re-check condition to handle spurious wakeup
            }
        }
    }

    // Copy item from buffer
    memcpy(item, queue->buffer + (impl->head * queue->config.item_size),
           queue->config.item_size);

    // Advance head
    impl->head = (impl->head + 1) % queue->capacity;

    // Update statistics
    queue->status.current_size--;
    queue->status.total_dequeued++;

    // Signal producer
    nimcp_cond_signal(&impl->not_full);

    nimcp_mutex_unlock(&impl->mutex);
    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_queue_blocking_peek(struct nimcp_queue* queue, void* item) {
    if (!queue || !item || !queue->impl_data) {
        return NIMCP_INVALID_PARAM;
    }

    blocking_impl_t* impl = (blocking_impl_t*)queue->impl_data;

    nimcp_mutex_lock(&impl->mutex);

    if (is_empty(impl)) {
        nimcp_mutex_unlock(&impl->mutex);
        return NIMCP_QUEUE_EMPTY;
    }

    memcpy(item, queue->buffer + (impl->head * queue->config.item_size),
           queue->config.item_size);

    nimcp_mutex_unlock(&impl->mutex);
    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_queue_blocking_clear(struct nimcp_queue* queue) {
    if (!queue || !queue->impl_data) {
        return NIMCP_INVALID_PARAM;
    }

    blocking_impl_t* impl = (blocking_impl_t*)queue->impl_data;

    nimcp_mutex_lock(&impl->mutex);

    impl->head = 0;
    impl->tail = 0;
    queue->status.current_size = 0;

    nimcp_cond_broadcast(&impl->not_full);

    nimcp_mutex_unlock(&impl->mutex);
    return NIMCP_SUCCESS;
}

bool nimcp_queue_blocking_is_empty(struct nimcp_queue* queue) {
    if (!queue || !queue->impl_data) {
        return true;
    }

    blocking_impl_t* impl = (blocking_impl_t*)queue->impl_data;

    nimcp_mutex_lock(&impl->mutex);
    bool empty = is_empty(impl);
    nimcp_mutex_unlock(&impl->mutex);

    return empty;
}

bool nimcp_queue_blocking_is_full(struct nimcp_queue* queue) {
    if (!queue || !queue->impl_data) {
        return true;
    }

    blocking_impl_t* impl = (blocking_impl_t*)queue->impl_data;

    nimcp_mutex_lock(&impl->mutex);
    bool full = is_full(queue, impl);
    nimcp_mutex_unlock(&impl->mutex);

    return full;
}

size_t nimcp_queue_blocking_get_size(struct nimcp_queue* queue) {
    if (!queue || !queue->impl_data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,

                "nimcp_queue_blocking_get_size: invalid parameters");

            return 0;
    }

    blocking_impl_t* impl = (blocking_impl_t*)queue->impl_data;

    nimcp_mutex_lock(&impl->mutex);
    size_t size = queue->status.current_size;
    nimcp_mutex_unlock(&impl->mutex);

    return size;
}
