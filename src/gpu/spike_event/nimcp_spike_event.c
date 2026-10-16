/**
 * @file nimcp_spike_event.c
 * @brief Implementation of spike event message-passing system
 *
 * WHAT: Implements biological spike communication between neurons
 * WHY:  True P2P neuron architecture requires message-passing, not shared memory
 * HOW:  Lock-free queues, circular buffers, atomic operations
 *
 * DESIGN PATTERNS:
 * - Producer-Consumer: Spike queues
 * - Circular Buffer: Spike trains
 * - Lock-Free: Atomic operations for thread safety
 *
 * BIO-ASYNC INTEGRATION:
 * - Registers with bio_router for spike event propagation
 * - Sends spike events to middleware for processing
 * - Receives commands from cognitive layer
 *
 * @author NIMCP Development Team
 * @date 2025
 * @version 2.7.0
 */

#include "gpu/nimcp_spike_event.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"
#include "utils/validation/nimcp_validate.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/logging/nimcp_logging.h"
#include "security/nimcp_security.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>

#define LOG_MODULE "SPIKE_EVENT"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(spike_event)

// C11 atomics for lock-free operations
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L && !defined(__STDC_NO_ATOMICS__)
#include <stdatomic.h>
#define ATOMIC_UINT atomic_uint
#define ATOMIC_LOAD(ptr) atomic_load(ptr)
#define ATOMIC_STORE(ptr, val) atomic_store(ptr, val)
#define ATOMIC_FETCH_ADD(ptr, val) atomic_fetch_add(ptr, val)
#define ATOMIC_COMPARE_EXCHANGE(ptr, expected, desired) \
    atomic_compare_exchange_weak(ptr, expected, desired)
#else
// WARNING: This fallback is NOT thread-safe for concurrent access.
// Platforms without C11 atomics must use external synchronization
// (e.g., a mutex around all spike queue operations) to prevent
// data races. The CAS emulation below is NOT atomic.
#warning "C11 atomics unavailable - spike event queue is NOT thread-safe"
#define ATOMIC_UINT volatile uint32_t
#define ATOMIC_LOAD(ptr) (*(ptr))
#define ATOMIC_STORE(ptr, val) (*(ptr) = (val))
#define ATOMIC_FETCH_ADD(ptr, val) ((*(ptr))++)
#define ATOMIC_COMPARE_EXCHANGE(ptr, expected, desired) \
    ((*(ptr) == *(expected)) ? (*(ptr) = (desired), true) : (*(expected) = *(ptr), false))
#endif

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Internal spike train structure
 */
struct spike_train_struct {
    spike_event_t* events;   /**< Circular buffer */
    uint32_t capacity;
    uint32_t count;
    uint32_t head;           /**< Write position */
    uint64_t last_spike;
    float firing_rate;
    uint64_t window_start;   /**< For rate calculation */

    // Bio-async integration
    bio_module_context_t bio_ctx;
    bool bio_async_enabled;
};

/**
 * @brief Internal spike queue structure with atomics
 */
struct spike_queue_struct {
    spike_event_t* events;
    uint32_t capacity;
    uint32_t mask;           /**< capacity - 1 (for fast modulo) */
    ATOMIC_UINT head;
    ATOMIC_UINT tail;
    ATOMIC_UINT count;
    bool gpu_enabled;
    void* gpu_ptr;

    // Bio-async integration
    bio_module_context_t bio_ctx;
    bool bio_async_enabled;
};

//=============================================================================
// Spike Train Implementation
//=============================================================================

/**
 * @brief Create spike train
 */
spike_train_t* spike_train_create(uint32_t capacity)
{
    LOG_DEBUG("Creating spike train with capacity %u", capacity);

    // Guard: Validate capacity
    if (capacity == 0 || capacity > 1000000) {
        LOG_ERROR("Invalid spike train capacity: %u (must be 1-1000000)", capacity);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "spike_train_create: capacity is zero");
        return NULL;
    }

    spike_train_t* train = (spike_train_t*)nimcp_calloc(1, sizeof(spike_train_t));
    if (!train) {
        LOG_ERROR("Failed to allocate spike train structure");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "spike_train_create: train is NULL");
        return NULL;
    }

    train->events = (spike_event_t*)nimcp_calloc(capacity, sizeof(spike_event_t));
    if (!train->events) {
        LOG_ERROR("Failed to allocate spike train events buffer (capacity=%u)", capacity);
        nimcp_free(train);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "spike_train_create: train->events is NULL");
        return NULL;
    }

    train->capacity = capacity;
    train->count = 0;
    train->head = 0;
    train->last_spike = 0;
    train->firing_rate = 0.0F;
    train->window_start = 0;

    // Initialize bio-async integration
    train->bio_async_enabled = false;
    train->bio_ctx = NULL;
    if (bio_router_is_initialized()) {
        bio_module_info_t bio_info = {
            .module_id = BIO_MODULE_SPIKE_EVENT,
            .module_name = "spike_train",
            .inbox_capacity = 64,
            .user_data = train
        };
        train->bio_ctx = bio_router_register_module(&bio_info);
        if (train->bio_ctx) {
            train->bio_async_enabled = true;
            LOG_INFO("Spike train registered with bio_router");
        } else {
            LOG_WARN("Failed to register spike train with bio_router (async disabled)");
        }
    }

    // Security registration (TODO: implement proper security API)
    // Note: Security system registration is handled at a higher level

    LOG_INFO("Spike train created successfully (capacity=%u, bio_async=%s)",
             capacity, train->bio_async_enabled ? "enabled" : "disabled");

    return train;
}

/**
 * @brief Destroy spike train
 */
void spike_train_destroy(spike_train_t* train)
{
    if (!train) {
        return;
    }

    LOG_DEBUG("Destroying spike train (count=%u, capacity=%u)", train->count, train->capacity);

    // Unregister from bio-async
    if (train->bio_async_enabled && train->bio_ctx) {
        bio_router_unregister_module(train->bio_ctx);
        train->bio_ctx = NULL;
        LOG_DEBUG("Spike train unregistered from bio_router");
    }

    // Security unregistration (TODO: implement proper security API)
    // Note: Security system unregistration is handled at a higher level

    if (train->events) {
        nimcp_free(train->events);
    }

    nimcp_free(train);
    LOG_DEBUG("Spike train destroyed successfully");
}

/**
 * @brief Add spike to train
 */
bool spike_train_add(spike_train_t* train, uint64_t timestamp, float amplitude)
{
    // Guard: Validate inputs
    // WHAT: Allow timestamp 0 - it's a valid time (t=0)
    // WHY:  Tests and simulations often start at t=0
    // FIX:  Removed timestamp == 0 check (Issue #SPIKE-003)
    if (!train) {
        LOG_ERROR("spike_train_add: train is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "spike_train_add: train is NULL");
        return false;
    }

    // Process pending bio-async messages
    if (train->bio_async_enabled && train->bio_ctx) {
        bio_router_process_inbox(train->bio_ctx, 5);
    }

    LOG_DEBUG("Adding spike to train: timestamp=%lu, amplitude=%.3f", timestamp, amplitude);

    // Create spike event
    spike_event_t event = {
        .timestamp = timestamp,
        .source_id = 0,  // Will be set by caller if needed
        .target_id = 0,
        .synapse_id = 0,
        .amplitude = amplitude
    };

    // Add to circular buffer
    train->events[train->head] = event;
    train->head = (train->head + 1) % train->capacity;

    // Update count (saturate at capacity)
    if (train->count < train->capacity) {
        train->count++;
    } else {
        LOG_DEBUG("Spike train full (capacity=%u), overwriting oldest spike", train->capacity);
    }

    // Update last spike time
    train->last_spike = timestamp;

    // Send spike event via bio-async
    if (train->bio_async_enabled && train->bio_ctx) {
        /* Construct a spike event message with header + payload */
        struct {
            bio_message_header_t header;
            spike_event_t payload;
        } msg;

        bio_msg_init_header(&msg.header, BIO_MSG_LOGIC_SPIKE_EVENT,
                           BIO_MODULE_NEURON_MODEL, BIO_MODULE_PIPELINE,
                           sizeof(spike_event_t));
        msg.header.timestamp_us = timestamp;
        msg.payload = event;

        if (bio_router_send(train->bio_ctx, &msg, sizeof(msg), 0) != NIMCP_SUCCESS) {
            LOG_WARN("Failed to send spike event via bio_router");
        } else {
            LOG_DEBUG("Spike event sent via bio_router");
        }
    }

    return true;
}

/**
 * @brief Get last spike time
 */
uint64_t spike_train_get_last_spike(const spike_train_t* train)
{
    if (!train) {
        return 0;
    }

    return train->last_spike;
}

/**
 * @brief Get spike at index
 */
bool spike_train_get_spike(const spike_train_t* train, uint32_t index,
                           spike_event_t* event)
{
    // Guard: Validate inputs
    if (!train || !event || index >= train->count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "spike_train_get_spike: required parameter is NULL (train, event)");
        return false;
    }

    // Calculate circular buffer index
    // oldest = (head - count) % capacity
    // index_pos = (oldest + index) % capacity
    uint32_t oldest = (train->head + train->capacity - train->count) % train->capacity;
    uint32_t pos = (oldest + index) % train->capacity;

    *event = train->events[pos];
    return true;
}

/**
 * @brief Compute firing rate
 */
float spike_train_compute_rate(spike_train_t* train, uint64_t time_window)
{
    // Guard: Validate inputs
    if (!train || time_window == 0) {
        return 0.0F;
    }

    if (train->count == 0) {
        train->firing_rate = 0.0F;
        return 0.0F;
    }

    /**
     * WHAT: Use last spike timestamp as window end (not wall-clock time)
     * WHY:  Simulations use synthetic timestamps (0, 1000, 2000...), not real time
     * HOW:  Calculate window relative to last_spike instead of nimcp_time_get_us()
     * FIX:  Issue #SPIKE-004 - Firing rate always 0 in tests
     */
    uint64_t window_end = train->last_spike;
    uint64_t window_start = (window_end > time_window) ? (window_end - time_window) : 0;

    // Count spikes in time window
    uint32_t spike_count = 0;

    for (uint32_t i = 0; i < train->count; i++) {
        spike_event_t event;
        if (spike_train_get_spike(train, i, &event)) {
            if (event.timestamp >= window_start && event.timestamp <= window_end) {
                spike_count++;
            }
        }
    }

    // Convert to Hz (spikes per second)
    float time_window_sec = time_window / 1000000.0F;
    train->firing_rate = spike_count / time_window_sec;

    return train->firing_rate;
}

/**
 * @brief Clear spike train
 */
void spike_train_clear(spike_train_t* train)
{
    if (!train) {
        return;
    }

    train->count = 0;
    train->head = 0;
    train->last_spike = 0;
    train->firing_rate = 0.0F;
}

//=============================================================================
// Spike Queue Implementation (Lock-Free)
//=============================================================================

/**
 * @brief Round up to next power of 2
 */
static uint32_t next_power_of_2(uint32_t n)
{
    if (n == 0) {
        return 1;
    }

    n--;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    n++;

    return n;
}

/**
 * @brief Create spike queue
 */
spike_queue_t* spike_queue_create(uint32_t capacity, bool gpu_enabled)
{
    // Guard: Validate capacity
    if (capacity == 0 || capacity > 10000000) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "spike_queue_create: capacity is zero");
        return NULL;
    }

    // Round up to power of 2 for fast modulo
    capacity = next_power_of_2(capacity);

    spike_queue_t* queue = (spike_queue_t*)nimcp_calloc(1, sizeof(spike_queue_t));
    if (!queue) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "queue is NULL");

        return NULL;
    }

    queue->events = (spike_event_t*)nimcp_calloc(capacity, sizeof(spike_event_t));
    if (!queue->events) {
        nimcp_free(queue);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "spike_queue_create: queue->events is NULL");
        return NULL;
    }

    queue->capacity = capacity;
    queue->mask = capacity - 1;  // For fast modulo
    ATOMIC_STORE(&queue->head, 0);
    ATOMIC_STORE(&queue->tail, 0);
    ATOMIC_STORE(&queue->count, 0);
    queue->gpu_enabled = gpu_enabled;
    queue->gpu_ptr = NULL;

    // GPU memory allocation would go here if CUDA enabled
    #ifdef NIMCP_ENABLE_CUDA
    if (gpu_enabled) {
        // cudaMalloc(&queue->gpu_ptr, capacity * sizeof(spike_event_t));
        // Not implemented yet - requires CUDA
        queue->gpu_enabled = false;  // Disable for now
    }
    #else
    queue->gpu_enabled = false;  // CUDA not available
    #endif

    return queue;
}

/**
 * @brief Destroy spike queue
 */
void spike_queue_destroy(spike_queue_t* queue)
{
    if (!queue) {
        return;
    }

    // Free GPU memory if allocated
    #ifdef NIMCP_ENABLE_CUDA
    if (queue->gpu_ptr) {
        // cudaFree(queue->gpu_ptr);
    }
    #endif

    if (queue->events) {
        nimcp_free(queue->events);
    }

    nimcp_free(queue);
}

/**
 * @brief Push spike to queue (lock-free)
 */
bool spike_queue_push(spike_queue_t* queue, const spike_event_t* event)
{
    // Guard: Validate inputs
    if (!queue || !event) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "spike_queue_push: required parameter is NULL (queue, event)");
        return false;
    }

    // Security: Validate spike amplitude for NaN/Inf
    // WHAT: Prevent corrupted floating point values from propagating
    // WHY:  NaN/Inf spikes would corrupt downstream calculations
    // HOW:  Reject spikes with invalid amplitude values
    if (isnan(event->amplitude) || isinf(event->amplitude)) {
        LOG_WARN("Rejecting spike with invalid amplitude: source=%u target=%u amplitude=%f",
                 event->source_id, event->target_id, event->amplitude);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "spike_queue_push: validation failed");
        return false;
    }

    // Check if queue is full
    uint32_t current_count = ATOMIC_LOAD(&queue->count);
    if (current_count >= queue->capacity) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "spike_queue_push: capacity exceeded");
        return false;  // Queue full
    }

    // Get tail position and increment atomically
    uint32_t tail_pos = ATOMIC_FETCH_ADD(&queue->tail, 1);
    uint32_t index = tail_pos & queue->mask;

    // Write event
    queue->events[index] = *event;

    // Increment count
    ATOMIC_FETCH_ADD(&queue->count, 1);

    return true;
}

/**
 * @brief Pop spike from queue (lock-free)
 */
bool spike_queue_pop(spike_queue_t* queue, spike_event_t* event)
{
    // Guard: Validate inputs
    if (!queue || !event) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "spike_queue_pop: required parameter is NULL (queue, event)");
        return false;
    }

    /**
     * WHAT: Atomically reserve a slot by decrementing count first
     * WHY:  Prevent race where two threads both pass empty check with count=1
     * HOW:  Use CAS loop to atomically check and decrement count before reading
     * FIX:  Issue #SPIKE-005 - ConcurrentPop race condition
     */
    uint32_t old_count;
    do {
        old_count = ATOMIC_LOAD(&queue->count);
        if (old_count == 0) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "spike_queue_pop: old_count is zero");
            return false;  // Queue empty
        }
    } while (!ATOMIC_COMPARE_EXCHANGE(&queue->count, &old_count, old_count - 1));

    // Slot reserved - now get head position and increment atomically
    uint32_t head_pos = ATOMIC_FETCH_ADD(&queue->head, 1);
    uint32_t index = head_pos & queue->mask;

    // Read event
    *event = queue->events[index];

    return true;
}

/**
 * @brief Get queue size
 */
uint32_t spike_queue_size(const spike_queue_t* queue)
{
    if (!queue) {
        return 0;
    }

    return ATOMIC_LOAD(&queue->count);
}

/**
 * @brief Check if queue is empty
 */
bool spike_queue_is_empty(const spike_queue_t* queue)
{
    return spike_queue_size(queue) == 0;
}

/**
 * @brief Synchronize with GPU
 */
bool spike_queue_sync_gpu(spike_queue_t* queue, bool direction)
{
    // Guard: Validate queue
    if (!queue) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "spike_queue_sync_gpu: queue is NULL");
        return false;
    }

    // Guard: Check if GPU enabled
    if (!queue->gpu_enabled || !queue->gpu_ptr) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "spike_queue_sync_gpu: required parameter is NULL (queue->gpu_enabled, queue->gpu_ptr)");
        return false;  // GPU not enabled/available
    }

    #ifdef NIMCP_ENABLE_CUDA
    // TODO: Implement CUDA memory copy
    // if (direction) {
    //     // CPU -> GPU
    //     cudaMemcpy(queue->gpu_ptr, queue->events,
    //                queue->capacity * sizeof(spike_event_t),
    //                cudaMemcpyHostToDevice);
    // } else {
    //     // GPU -> CPU
    //     cudaMemcpy(queue->events, queue->gpu_ptr,
    //                queue->capacity * sizeof(spike_event_t),
    //                cudaMemcpyDeviceToHost);
    // }
    return true;
    #else
    (void)direction;  // Unused
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "spike_queue_sync_gpu: operation failed");
    return false;  // CUDA not available
    #endif
}
