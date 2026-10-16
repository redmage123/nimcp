/**
 * @file nimcp_queue_spsc.c
 * @brief Lock-free Single-Producer Single-Consumer queue implementation
 *
 * WHAT: Wait-free queue for exactly one producer and one consumer thread
 * WHY:  Ultra-low latency (<100ns), no locks, no context switches
 * HOW:  Separate head/tail with cache-line padding, memory barriers
 *
 * PERFORMANCE: ~50-100ns per operation (wait-free, cache-optimized)
 * USE CASE: Pipeline stages, producer-consumer pairs, spike propagation
 *
 * IMPORTANT: Using this queue with multiple producers or multiple consumers
 *           is UNDEFINED BEHAVIOR and will cause data corruption.
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
#include <stdatomic.h>
#include <string.h>
#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory_guards.h"  // For nimcp_calloc/nimcp_free
#include "utils/memory/nimcp_unified_memory.h"

#define LOG_MODULE "QUEUE_SPSC"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(queue_spsc)

//=============================================================================
// Internal Structure
//=============================================================================

/**
 * @brief SPSC queue implementation data with cache-line padding
 *
 * Cache-line padding prevents false sharing between producer (tail)
 * and consumer (head) threads.
 */
typedef struct {
    // Producer side (owned by single producer thread)
    _Alignas(NIMCP_QUEUE_CACHE_LINE_SIZE) atomic_size_t tail;
    size_t cached_head;  // Local cache of head to reduce cache misses
    uint8_t _pad_producer[NIMCP_QUEUE_CACHE_LINE_SIZE - sizeof(atomic_size_t) - sizeof(size_t)];

    // Consumer side (owned by single consumer thread)
    _Alignas(NIMCP_QUEUE_CACHE_LINE_SIZE) atomic_size_t head;
    size_t cached_tail;  // Local cache of tail to reduce cache misses
    uint8_t _pad_consumer[NIMCP_QUEUE_CACHE_LINE_SIZE - sizeof(atomic_size_t) - sizeof(size_t)];

    // Buffer sizing
    size_t buffer_size;  // Actual buffer slots (capacity + 1 for sentinel)

    // Mutex for blocking operations (optional)
    nimcp_mutex_t mutex;
    nimcp_cond_t not_empty;
    nimcp_cond_t not_full;
    bool blocking_enabled;
} spsc_impl_t;

//=============================================================================
// Create/Destroy
//=============================================================================

nimcp_result_t nimcp_queue_spsc_create(
    struct nimcp_queue* queue,
    const nimcp_queue_config_t* config
) {
    if (!queue || !config) {
        return NIMCP_INVALID_PARAM;
    }

    // Round up capacity to power of 2 for efficient modulo (bitwise AND)
    size_t capacity = nimcp_queue_next_power_of_2(config->max_size);
    if (capacity < NIMCP_QUEUE_MIN_CAPACITY) {
        capacity = NIMCP_QUEUE_MIN_CAPACITY;
    }

    LOG_DEBUG(LOG_MODULE, "Creating SPSC queue (requested=%zu, actual=%zu, item_size=%zu)",
              config->max_size, capacity, config->item_size);

    // Allocate implementation data (aligned for cache efficiency)
    spsc_impl_t* impl = nimcp_calloc(1, sizeof(spsc_impl_t));
    if (!impl) {
        LOG_ERROR(LOG_MODULE, "Failed to allocate spsc_impl_t");
        return NIMCP_NO_MEMORY;
    }

    // Allocate buffer (extra slot to distinguish full from empty)
    queue->buffer = nimcp_malloc((capacity + 1) * config->item_size);
    if (!queue->buffer) {
        LOG_ERROR(LOG_MODULE, "Failed to allocate queue buffer");
        nimcp_free(impl);
        return NIMCP_NO_MEMORY;
    }

    // Initialize atomic indices
    atomic_init(&impl->head, 0);
    atomic_init(&impl->tail, 0);
    impl->cached_head = 0;
    impl->cached_tail = 0;

    // Initialize optional blocking support
    impl->blocking_enabled = config->is_blocking;
    if (impl->blocking_enabled) {
        if (nimcp_mutex_init(&impl->mutex, NULL) != NIMCP_SUCCESS ||
            nimcp_cond_init(&impl->not_empty) != NIMCP_SUCCESS ||
            nimcp_cond_init(&impl->not_full) != NIMCP_SUCCESS) {
            LOG_ERROR(LOG_MODULE, "Failed to init blocking primitives");
            nimcp_free(queue->buffer);
            nimcp_free(impl);
            return NIMCP_INIT_FAILED;
        }
    }

    queue->impl_data = impl;
    queue->capacity = capacity;  // Logical capacity (power of 2) for API
    impl->buffer_size = capacity + 1;  // Actual buffer slots (+1 for sentinel)

    LOG_INFO(LOG_MODULE, "SPSC queue created (capacity=%zu, buffer_size=%zu, blocking=%s)",
             capacity, impl->buffer_size, impl->blocking_enabled ? "yes" : "no");
    return NIMCP_SUCCESS;
}

void nimcp_queue_spsc_destroy(struct nimcp_queue* queue) {
    if (!queue || !queue->impl_data) {
        return;
    }

    LOG_DEBUG(LOG_MODULE, "Destroying SPSC queue");

    spsc_impl_t* impl = (spsc_impl_t*)queue->impl_data;

    if (impl->blocking_enabled) {
        nimcp_cond_destroy(&impl->not_full);
        nimcp_cond_destroy(&impl->not_empty);
        nimcp_mutex_destroy(&impl->mutex);
    }

    nimcp_free(impl);
    queue->impl_data = NULL;

    if (queue->buffer) {
        nimcp_free(queue->buffer);
        queue->buffer = NULL;
    }

    LOG_INFO(LOG_MODULE, "SPSC queue destroyed");
}

//=============================================================================
// Queue Operations
//=============================================================================

nimcp_result_t nimcp_queue_spsc_enqueue(
    struct nimcp_queue* queue,
    const void* item,
    uint32_t timeout_ms
) {
    if (!queue || !item || !queue->impl_data) {
        return NIMCP_INVALID_PARAM;
    }

    spsc_impl_t* impl = (spsc_impl_t*)queue->impl_data;

    // Load current tail (owned by producer - relaxed is fine)
    size_t tail = atomic_load_explicit(&impl->tail, memory_order_relaxed);
    size_t next_tail = (tail + 1) % impl->buffer_size;

    // Check if full using cached head first (avoid cache miss)
    if (next_tail == impl->cached_head) {
        // Update cache and recheck
        impl->cached_head = atomic_load_explicit(&impl->head, memory_order_acquire);
        if (next_tail == impl->cached_head) {
            // Queue is full
            if (!impl->blocking_enabled) {
                __atomic_fetch_add(&queue->status.enqueue_failures, 1, __ATOMIC_RELAXED);
                return NIMCP_QUEUE_FULL;
            }

            // Blocking wait
            nimcp_mutex_lock(&impl->mutex);
            while (next_tail == atomic_load_explicit(&impl->head, memory_order_acquire)) {
                __atomic_fetch_add(&queue->status.blocking_waits, 1, __ATOMIC_RELAXED);
                if (timeout_ms > 0 && timeout_ms != UINT32_MAX) {
                    nimcp_result_t r = nimcp_cond_timedwait(&impl->not_full, &impl->mutex, timeout_ms);
                    if (r != NIMCP_SUCCESS) {
                        __atomic_fetch_add(&queue->status.blocking_timeouts, 1, __ATOMIC_RELAXED);
                        nimcp_mutex_unlock(&impl->mutex);
                        return NIMCP_TIMEOUT;
                    }
                } else {
                    nimcp_cond_wait(&impl->not_full, &impl->mutex);
                }
            }
            nimcp_mutex_unlock(&impl->mutex);
            impl->cached_head = atomic_load_explicit(&impl->head, memory_order_acquire);
        }
    }

    // Copy item to buffer
    memcpy(queue->buffer + (tail * queue->config.item_size),
           item, queue->config.item_size);

    // Publish new tail with release semantics (ensures write is visible)
    atomic_store_explicit(&impl->tail, next_tail, memory_order_release);

    // Update statistics atomically (current_size is accessed by both producer and consumer)
    __atomic_fetch_add(&queue->status.total_enqueued, 1, __ATOMIC_RELAXED);
    size_t new_size = __atomic_add_fetch(&queue->status.current_size, 1, __ATOMIC_RELAXED);
    size_t peak = __atomic_load_n(&queue->status.peak_size, __ATOMIC_RELAXED);
    while (new_size > peak) {
        if (__atomic_compare_exchange_n(&queue->status.peak_size, &peak, new_size,
                                        false, __ATOMIC_RELAXED, __ATOMIC_RELAXED)) {
            break;
        }
    }

    // Signal consumer if blocking
    if (impl->blocking_enabled) {
        nimcp_mutex_lock(&impl->mutex);
        nimcp_cond_signal(&impl->not_empty);
        nimcp_mutex_unlock(&impl->mutex);
    }

    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_queue_spsc_dequeue(
    struct nimcp_queue* queue,
    void* item,
    uint32_t timeout_ms
) {
    if (!queue || !item || !queue->impl_data) {
        return NIMCP_INVALID_PARAM;
    }

    spsc_impl_t* impl = (spsc_impl_t*)queue->impl_data;

    // Load current head (owned by consumer - relaxed is fine)
    size_t head = atomic_load_explicit(&impl->head, memory_order_relaxed);

    // Check if empty using cached tail first
    if (head == impl->cached_tail) {
        impl->cached_tail = atomic_load_explicit(&impl->tail, memory_order_acquire);
        if (head == impl->cached_tail) {
            // Queue is empty
            if (!impl->blocking_enabled) {
                __atomic_fetch_add(&queue->status.dequeue_failures, 1, __ATOMIC_RELAXED);
                return NIMCP_QUEUE_EMPTY;
            }

            // Blocking wait
            nimcp_mutex_lock(&impl->mutex);
            while (head == atomic_load_explicit(&impl->tail, memory_order_acquire)) {
                __atomic_fetch_add(&queue->status.blocking_waits, 1, __ATOMIC_RELAXED);
                if (timeout_ms > 0 && timeout_ms != UINT32_MAX) {
                    nimcp_result_t r = nimcp_cond_timedwait(&impl->not_empty, &impl->mutex, timeout_ms);
                    if (r != NIMCP_SUCCESS) {
                        __atomic_fetch_add(&queue->status.blocking_timeouts, 1, __ATOMIC_RELAXED);
                        nimcp_mutex_unlock(&impl->mutex);
                        return NIMCP_TIMEOUT;
                    }
                } else {
                    nimcp_cond_wait(&impl->not_empty, &impl->mutex);
                }
            }
            nimcp_mutex_unlock(&impl->mutex);
            impl->cached_tail = atomic_load_explicit(&impl->tail, memory_order_acquire);
        }
    }

    // Copy item from buffer (acquire already done above)
    memcpy(item, queue->buffer + (head * queue->config.item_size),
           queue->config.item_size);

    // Advance head with release semantics
    size_t next_head = (head + 1) % impl->buffer_size;
    atomic_store_explicit(&impl->head, next_head, memory_order_release);

    // Update statistics atomically
    __atomic_fetch_add(&queue->status.total_dequeued, 1, __ATOMIC_RELAXED);
    __atomic_fetch_sub(&queue->status.current_size, 1, __ATOMIC_RELAXED);

    // Signal producer if blocking
    if (impl->blocking_enabled) {
        nimcp_mutex_lock(&impl->mutex);
        nimcp_cond_signal(&impl->not_full);
        nimcp_mutex_unlock(&impl->mutex);
    }

    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_queue_spsc_peek(struct nimcp_queue* queue, void* item) {
    if (!queue || !item || !queue->impl_data) {
        return NIMCP_INVALID_PARAM;
    }

    spsc_impl_t* impl = (spsc_impl_t*)queue->impl_data;

    size_t head = atomic_load_explicit(&impl->head, memory_order_relaxed);
    size_t tail = atomic_load_explicit(&impl->tail, memory_order_acquire);

    if (head == tail) {
        return NIMCP_QUEUE_EMPTY;
    }

    memcpy(item, queue->buffer + (head * queue->config.item_size),
           queue->config.item_size);

    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_queue_spsc_clear(struct nimcp_queue* queue) {
    if (!queue || !queue->impl_data) {
        return NIMCP_INVALID_PARAM;
    }

    spsc_impl_t* impl = (spsc_impl_t*)queue->impl_data;

    // Reset both indices to 0
    atomic_store_explicit(&impl->head, 0, memory_order_release);
    atomic_store_explicit(&impl->tail, 0, memory_order_release);
    impl->cached_head = 0;
    impl->cached_tail = 0;
    __atomic_store_n(&queue->status.current_size, 0, __ATOMIC_RELAXED);

    if (impl->blocking_enabled) {
        nimcp_mutex_lock(&impl->mutex);
        nimcp_cond_broadcast(&impl->not_full);
        nimcp_mutex_unlock(&impl->mutex);
    }

    return NIMCP_SUCCESS;
}

bool nimcp_queue_spsc_is_empty(struct nimcp_queue* queue) {
    if (!queue || !queue->impl_data) {
        return true;
    }

    spsc_impl_t* impl = (spsc_impl_t*)queue->impl_data;

    size_t head = atomic_load_explicit(&impl->head, memory_order_acquire);
    size_t tail = atomic_load_explicit(&impl->tail, memory_order_acquire);

    return head == tail;
}

bool nimcp_queue_spsc_is_full(struct nimcp_queue* queue) {
    if (!queue || !queue->impl_data) {
        return true;
    }

    spsc_impl_t* impl = (spsc_impl_t*)queue->impl_data;

    size_t head = atomic_load_explicit(&impl->head, memory_order_acquire);
    size_t tail = atomic_load_explicit(&impl->tail, memory_order_relaxed);
    size_t next_tail = (tail + 1) % impl->buffer_size;

    return next_tail == head;
}

size_t nimcp_queue_spsc_get_size(struct nimcp_queue* queue) {
    if (!queue || !queue->impl_data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,

                "nimcp_queue_spsc_get_size: invalid parameters");

            return 0;
    }

    spsc_impl_t* impl = (spsc_impl_t*)queue->impl_data;

    size_t head = atomic_load_explicit(&impl->head, memory_order_acquire);
    size_t tail = atomic_load_explicit(&impl->tail, memory_order_acquire);

    if (tail >= head) {
        return tail - head;
    } else {
        return impl->buffer_size - head + tail;
    }
}
