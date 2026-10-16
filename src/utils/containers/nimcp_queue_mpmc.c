/**
 * @file nimcp_queue_mpmc.c
 * @brief Lock-free Multi-Producer Multi-Consumer queue implementation
 *
 * WHAT: Lock-free queue supporting multiple producers and consumers
 * WHY:  Handle concurrent access from many threads with low latency
 * HOW:  Vyukov bounded MPMC algorithm with CAS operations
 *
 * PERFORMANCE: ~200-500ns per operation (lock-free, handles contention)
 * USE CASE: Work stealing, task queues, event distribution
 *
 * ALGORITHM: Bounded MPMC Queue (Dmitry Vyukov)
 * - Each slot has a sequence number
 * - Producers CAS slot sequence to claim position
 * - Consumers CAS slot sequence to claim item
 * - Automatic backoff on contention
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

#define LOG_MODULE "QUEUE_MPMC"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(queue_mpmc)

//=============================================================================
// Internal Structure
//=============================================================================

/**
 * @brief Cell structure for MPMC queue slots
 */
typedef struct {
    atomic_size_t sequence;  /**< Sequence number for this slot */
    uint8_t* data;           /**< Pointer to item data (within buffer) */
} mpmc_cell_t;

/**
 * @brief MPMC queue implementation data
 */
typedef struct {
    // Cache-line aligned indices
    _Alignas(NIMCP_QUEUE_CACHE_LINE_SIZE) atomic_size_t enqueue_pos;
    uint8_t _pad1[NIMCP_QUEUE_CACHE_LINE_SIZE - sizeof(atomic_size_t)];

    _Alignas(NIMCP_QUEUE_CACHE_LINE_SIZE) atomic_size_t dequeue_pos;
    uint8_t _pad2[NIMCP_QUEUE_CACHE_LINE_SIZE - sizeof(atomic_size_t)];

    // Cells array
    mpmc_cell_t* cells;
    size_t mask;  // capacity - 1 (for fast modulo via AND)

    // Spin count for busy waiting
    uint32_t spin_count;

    // Optional blocking support
    nimcp_mutex_t mutex;
    nimcp_cond_t not_empty;
    nimcp_cond_t not_full;
    bool blocking_enabled;

    // Contention statistics
    atomic_uint_fast64_t cas_retries;
    atomic_uint_fast64_t spin_cycles;
} mpmc_impl_t;

//=============================================================================
// Spin/Backoff Helpers
//=============================================================================

static inline void cpu_pause(void) {
#if defined(__x86_64__) || defined(__i386__)
    __asm__ __volatile__("pause" ::: "memory");
#elif defined(__aarch64__)
    __asm__ __volatile__("yield" ::: "memory");
#else
    atomic_thread_fence(memory_order_seq_cst);
#endif
}

static inline void spin_wait(uint32_t count) {
    for (uint32_t i = 0; i < count; i++) {
        cpu_pause();
    }
}

//=============================================================================
// Create/Destroy
//=============================================================================

nimcp_result_t nimcp_queue_mpmc_create(
    struct nimcp_queue* queue,
    const nimcp_queue_config_t* config
) {
    if (!queue || !config) {
        return NIMCP_INVALID_PARAM;
    }

    // Capacity must be power of 2 for efficient modulo
    size_t capacity = nimcp_queue_next_power_of_2(config->max_size);
    if (capacity < NIMCP_QUEUE_MIN_CAPACITY) {
        capacity = NIMCP_QUEUE_MIN_CAPACITY;
    }

    LOG_DEBUG(LOG_MODULE, "Creating MPMC queue (requested=%zu, actual=%zu, item_size=%zu)",
              config->max_size, capacity, config->item_size);

    // Allocate implementation data
    mpmc_impl_t* impl = nimcp_calloc(1, sizeof(mpmc_impl_t));
    if (!impl) {
        LOG_ERROR(LOG_MODULE, "Failed to allocate mpmc_impl_t");
        return NIMCP_NO_MEMORY;
    }

    // Allocate cells array
    impl->cells = nimcp_calloc(capacity, sizeof(mpmc_cell_t));
    if (!impl->cells) {
        LOG_ERROR(LOG_MODULE, "Failed to allocate cells array");
        nimcp_free(impl);
        return NIMCP_NO_MEMORY;
    }

    // Allocate data buffer
    queue->buffer = nimcp_malloc(capacity * config->item_size);
    if (!queue->buffer) {
        LOG_ERROR(LOG_MODULE, "Failed to allocate queue buffer");
        nimcp_free(impl->cells);
        nimcp_free(impl);
        return NIMCP_NO_MEMORY;
    }

    // Initialize cells with sequence numbers
    for (size_t i = 0; i < capacity; i++) {
        atomic_init(&impl->cells[i].sequence, i);
        impl->cells[i].data = queue->buffer + (i * config->item_size);
    }

    // Initialize indices
    atomic_init(&impl->enqueue_pos, 0);
    atomic_init(&impl->dequeue_pos, 0);
    impl->mask = capacity - 1;
    impl->spin_count = config->spin_count > 0 ? config->spin_count : NIMCP_QUEUE_DEFAULT_SPIN_COUNT;

    // Initialize statistics
    atomic_init(&impl->cas_retries, 0);
    atomic_init(&impl->spin_cycles, 0);

    // Initialize optional blocking support
    impl->blocking_enabled = config->is_blocking;
    if (impl->blocking_enabled) {
        if (nimcp_mutex_init(&impl->mutex, NULL) != NIMCP_SUCCESS ||
            nimcp_cond_init(&impl->not_empty) != NIMCP_SUCCESS ||
            nimcp_cond_init(&impl->not_full) != NIMCP_SUCCESS) {
            LOG_ERROR(LOG_MODULE, "Failed to init blocking primitives");
            nimcp_free(queue->buffer);
            nimcp_free(impl->cells);
            nimcp_free(impl);
            return NIMCP_INIT_FAILED;
        }
    }

    queue->impl_data = impl;
    queue->capacity = capacity;

    LOG_INFO(LOG_MODULE, "MPMC queue created (capacity=%zu, spin_count=%u, blocking=%s)",
             capacity, impl->spin_count, impl->blocking_enabled ? "yes" : "no");
    return NIMCP_SUCCESS;
}

void nimcp_queue_mpmc_destroy(struct nimcp_queue* queue) {
    if (!queue || !queue->impl_data) {
        return;
    }

    LOG_DEBUG(LOG_MODULE, "Destroying MPMC queue");

    mpmc_impl_t* impl = (mpmc_impl_t*)queue->impl_data;

    // Log contention statistics
    LOG_INFO(LOG_MODULE, "MPMC queue stats: cas_retries=%lu, spin_cycles=%lu",
             (unsigned long)atomic_load(&impl->cas_retries),
             (unsigned long)atomic_load(&impl->spin_cycles));

    if (impl->blocking_enabled) {
        nimcp_cond_destroy(&impl->not_full);
        nimcp_cond_destroy(&impl->not_empty);
        nimcp_mutex_destroy(&impl->mutex);
    }

    nimcp_free(impl->cells);
    nimcp_free(impl);
    queue->impl_data = NULL;

    if (queue->buffer) {
        nimcp_free(queue->buffer);
        queue->buffer = NULL;
    }

    LOG_INFO(LOG_MODULE, "MPMC queue destroyed");
}

//=============================================================================
// Queue Operations
//=============================================================================

nimcp_result_t nimcp_queue_mpmc_enqueue(
    struct nimcp_queue* queue,
    const void* item,
    uint32_t timeout_ms
) {
    if (!queue || !item || !queue->impl_data) {
        return NIMCP_INVALID_PARAM;
    }

    mpmc_impl_t* impl = (mpmc_impl_t*)queue->impl_data;
    mpmc_cell_t* cell;
    size_t pos;
    size_t spin_count = 0;

    for (;;) {
        pos = atomic_load_explicit(&impl->enqueue_pos, memory_order_relaxed);
        cell = &impl->cells[pos & impl->mask];
        size_t seq = atomic_load_explicit(&cell->sequence, memory_order_acquire);
        intptr_t diff = (intptr_t)seq - (intptr_t)pos;

        if (diff == 0) {
            // Slot is available, try to claim it
            if (atomic_compare_exchange_weak_explicit(
                    &impl->enqueue_pos, &pos, pos + 1,
                    memory_order_relaxed, memory_order_relaxed)) {
                break;  // Success!
            }
            // CAS failed, someone else got it
            atomic_fetch_add_explicit(&impl->cas_retries, 1, memory_order_relaxed);
        } else if (diff < 0) {
            // Queue is full
            if (!impl->blocking_enabled) {
                __atomic_fetch_add(&queue->status.enqueue_failures, 1, __ATOMIC_RELAXED);
                return NIMCP_QUEUE_FULL;
            }

            // Spin for a bit before blocking
            if (spin_count < impl->spin_count) {
                spin_wait(1);
                spin_count++;
                atomic_fetch_add_explicit(&impl->spin_cycles, 1, memory_order_relaxed);
                continue;
            }

            // Block waiting for space
            nimcp_mutex_lock(&impl->mutex);
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
            nimcp_mutex_unlock(&impl->mutex);
            spin_count = 0;  // Reset spin count
        }
        // else: diff > 0, sequence advanced, retry
    }

    // Copy item to cell
    memcpy(cell->data, item, queue->config.item_size);

    // Publish by updating sequence (release)
    atomic_store_explicit(&cell->sequence, pos + 1, memory_order_release);

    // Update statistics atomically for thread-safety
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

nimcp_result_t nimcp_queue_mpmc_dequeue(
    struct nimcp_queue* queue,
    void* item,
    uint32_t timeout_ms
) {
    if (!queue || !item || !queue->impl_data) {
        return NIMCP_INVALID_PARAM;
    }

    mpmc_impl_t* impl = (mpmc_impl_t*)queue->impl_data;
    mpmc_cell_t* cell;
    size_t pos;
    size_t spin_count = 0;

    for (;;) {
        pos = atomic_load_explicit(&impl->dequeue_pos, memory_order_relaxed);
        cell = &impl->cells[pos & impl->mask];
        size_t seq = atomic_load_explicit(&cell->sequence, memory_order_acquire);
        intptr_t diff = (intptr_t)seq - (intptr_t)(pos + 1);

        if (diff == 0) {
            // Item is ready, try to claim it
            if (atomic_compare_exchange_weak_explicit(
                    &impl->dequeue_pos, &pos, pos + 1,
                    memory_order_relaxed, memory_order_relaxed)) {
                break;  // Success!
            }
            // CAS failed
            atomic_fetch_add_explicit(&impl->cas_retries, 1, memory_order_relaxed);
        } else if (diff < 0) {
            // Queue is empty
            if (!impl->blocking_enabled) {
                __atomic_fetch_add(&queue->status.dequeue_failures, 1, __ATOMIC_RELAXED);
                return NIMCP_QUEUE_EMPTY;
            }

            // Spin before blocking
            if (spin_count < impl->spin_count) {
                spin_wait(1);
                spin_count++;
                atomic_fetch_add_explicit(&impl->spin_cycles, 1, memory_order_relaxed);
                continue;
            }

            // Block waiting for item
            nimcp_mutex_lock(&impl->mutex);
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
            nimcp_mutex_unlock(&impl->mutex);
            spin_count = 0;
        }
        // else: diff > 0, retry
    }

    // Copy item from cell
    memcpy(item, cell->data, queue->config.item_size);

    // Mark cell as available for reuse
    atomic_store_explicit(&cell->sequence, pos + impl->mask + 1, memory_order_release);

    // Update statistics atomically for thread-safety
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

nimcp_result_t nimcp_queue_mpmc_peek(struct nimcp_queue* queue, void* item) {
    if (!queue || !item || !queue->impl_data) {
        return NIMCP_INVALID_PARAM;
    }

    mpmc_impl_t* impl = (mpmc_impl_t*)queue->impl_data;

    size_t pos = atomic_load_explicit(&impl->dequeue_pos, memory_order_relaxed);
    mpmc_cell_t* cell = &impl->cells[pos & impl->mask];
    size_t seq = atomic_load_explicit(&cell->sequence, memory_order_acquire);
    intptr_t diff = (intptr_t)seq - (intptr_t)(pos + 1);

    if (diff < 0) {
        return NIMCP_QUEUE_EMPTY;
    }

    memcpy(item, cell->data, queue->config.item_size);
    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_queue_mpmc_clear(struct nimcp_queue* queue) {
    if (!queue || !queue->impl_data) {
        return NIMCP_INVALID_PARAM;
    }

    mpmc_impl_t* impl = (mpmc_impl_t*)queue->impl_data;

    // Reset all sequences
    for (size_t i = 0; i <= impl->mask; i++) {
        atomic_store_explicit(&impl->cells[i].sequence, i, memory_order_release);
    }

    atomic_store_explicit(&impl->enqueue_pos, 0, memory_order_release);
    atomic_store_explicit(&impl->dequeue_pos, 0, memory_order_release);
    __atomic_store_n(&queue->status.current_size, 0, __ATOMIC_RELAXED);

    if (impl->blocking_enabled) {
        nimcp_mutex_lock(&impl->mutex);
        nimcp_cond_broadcast(&impl->not_full);
        nimcp_mutex_unlock(&impl->mutex);
    }

    return NIMCP_SUCCESS;
}

bool nimcp_queue_mpmc_is_empty(struct nimcp_queue* queue) {
    if (!queue || !queue->impl_data) {
        return true;
    }

    mpmc_impl_t* impl = (mpmc_impl_t*)queue->impl_data;

    size_t pos = atomic_load_explicit(&impl->dequeue_pos, memory_order_relaxed);
    mpmc_cell_t* cell = &impl->cells[pos & impl->mask];
    size_t seq = atomic_load_explicit(&cell->sequence, memory_order_acquire);

    return ((intptr_t)seq - (intptr_t)(pos + 1)) < 0;
}

bool nimcp_queue_mpmc_is_full(struct nimcp_queue* queue) {
    if (!queue || !queue->impl_data) {
        return true;
    }

    mpmc_impl_t* impl = (mpmc_impl_t*)queue->impl_data;

    size_t pos = atomic_load_explicit(&impl->enqueue_pos, memory_order_relaxed);
    mpmc_cell_t* cell = &impl->cells[pos & impl->mask];
    size_t seq = atomic_load_explicit(&cell->sequence, memory_order_acquire);

    return ((intptr_t)seq - (intptr_t)pos) < 0;
}

size_t nimcp_queue_mpmc_get_size(struct nimcp_queue* queue) {
    if (!queue || !queue->impl_data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,

                "nimcp_queue_mpmc_get_size: invalid parameters");

            return 0;
    }

    mpmc_impl_t* impl = (mpmc_impl_t*)queue->impl_data;

    size_t enq = atomic_load_explicit(&impl->enqueue_pos, memory_order_acquire);
    size_t deq = atomic_load_explicit(&impl->dequeue_pos, memory_order_acquire);

    return (enq >= deq) ? (enq - deq) : 0;
}
