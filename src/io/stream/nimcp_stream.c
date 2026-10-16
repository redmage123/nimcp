
#define LOG_MODULE "nimcp_stream"
#define LOG_MODULE_ID 0x052F
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(stream)

/**
 * @file nimcp_stream.c
 * @brief Continuous input streaming implementation
 *
 * WHAT: Implements non-blocking continuous input processing for brains
 * WHY: Enable active consciousness with continuous sensory input
 * HOW: Ring buffer + background thread + event callbacks
 *
 * ARCHITECTURE:
 *   Producer (App)  →  [Ring Buffer]  →  Consumer (Thread)
 *                                      ↓
 *                         [Brain Processing]
 *                                      ↓
 *                         [Decision Cache] ← App reads
 *                                      ↓
 *                         [Event Callbacks] → App notified
 *
 * THREAD SAFETY:
 * - Ring buffer uses atomic operations
 * - Decision cache uses mutex
 * - Statistics use atomic counters
 * - Event callbacks executed in processing thread context
 */

#include "io/stream/nimcp_stream.h"
#include "constants/nimcp_constants.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "utils/memory/nimcp_memory.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/thread/nimcp_atomic.h"
#include "utils/time/nimcp_time.h"
#include "security/nimcp_security_integration.h"
#include "utils/logging/nimcp_logging.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"


//=============================================================================
// Global State for Module-Level Security Registration
//=============================================================================

/**
 * WHAT: Global security context and module ID for stream module
 * WHY: Track module-level security registration for all streams
 * HOW: Set during stream_init(), used by all stream operations
 */
static nimcp_sec_integration_t* g_stream_security_ctx = NULL;
static uint32_t g_stream_security_module_id = 0;
static nimcp_atomic_bool_t g_stream_initialized = {0};
static nimcp_atomic_bool_t g_stream_initializing = {0};  // Guard against concurrent init

//=============================================================================
// Thread-Local Error Handling
//=============================================================================

/**
 * WHAT: Thread-local error message storage
 * WHY: Thread-safe error reporting without mutexes
 * PATTERN: Thread-local storage
 */
static __thread char g_stream_error[NIMCP_ERROR_BUFFER_LARGE] = {0};

static void stream_set_error(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    vsnprintf(g_stream_error, sizeof(g_stream_error), format, args);
    va_end(args);
}

const char* brain_stream_get_last_error(void)
{
    return g_stream_error;
}

//=============================================================================
// Ring Buffer Implementation (Lock-Free Producer-Consumer)
//=============================================================================

/**
 * WHAT: Single input entry in ring buffer
 * WHY: Store input features with metadata
 */
typedef struct {
    float* features;        // Feature vector (heap-allocated)
    uint32_t num_features;  // Size of feature vector
    uint64_t timestamp;     // When input occurred
    bool valid;             // Is this slot occupied?
} stream_input_t;

/**
 * WHAT: Lock-free ring buffer for stream inputs
 * WHY: High-performance producer-consumer queue
 * HOW: Atomic head/tail pointers, power-of-2 size for fast modulo
 *
 * PATTERN: Producer-Consumer with lock-free queue
 * COMPLEXITY: O(1) for enqueue/dequeue
 */
typedef struct {
    stream_input_t* buffer;  // Circular buffer
    uint32_t capacity;       // Buffer size (power of 2)
    uint32_t mask;           // capacity - 1 (for fast modulo)

    atomic_uint_fast32_t head;  // Producer writes here
    atomic_uint_fast32_t tail;  // Consumer reads here

    nimcp_mutex_t tail_lock;    // Protects tail advancement (drop_on_full vs dequeue race)

    atomic_uint_fast64_t enqueued;  // Total enqueued (stats)
    atomic_uint_fast64_t dequeued;  // Total dequeued (stats)
    atomic_uint_fast64_t dropped;   // Dropped due to full (stats)
} ring_buffer_t;

/**
 * WHAT: Create ring buffer
 * WHY: Initialize lock-free queue structure
 * HOW: Allocates power-of-2 sized buffer with atomic indices
 *
 * @param capacity Buffer size (will be rounded up to power of 2)
 * @return Ring buffer or NULL on error
 */
static ring_buffer_t* ring_buffer_create(uint32_t capacity)
{
    /**
     * WHAT: Round up to next power of 2
     * WHY: Allows fast modulo with bitwise AND instead of division
     * HOW: Sets all bits below highest set bit, then adds 1
     */
    capacity--;
    capacity |= capacity >> 1;
    capacity |= capacity >> 2;
    capacity |= capacity >> 4;
    capacity |= capacity >> 8;
    capacity |= capacity >> 16;
    capacity++;

    ring_buffer_t* rb = nimcp_calloc(1, sizeof(ring_buffer_t));
    if (!rb) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, sizeof(ring_buffer_t),
                          "Failed to allocate ring buffer structure");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "rb is NULL");

        return NULL;
    }

    rb->buffer = nimcp_calloc(capacity, sizeof(stream_input_t));
    if (!rb->buffer) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, capacity * sizeof(stream_input_t),
                          "Failed to allocate ring buffer entries");
        nimcp_free(rb);
        return NULL;
    }

    rb->capacity = capacity;
    rb->mask = capacity - 1;
    atomic_store(&rb->head, 0);
    atomic_store(&rb->tail, 0);
    nimcp_mutex_init(&rb->tail_lock, NULL);
    atomic_store(&rb->enqueued, 0);
    atomic_store(&rb->dequeued, 0);
    atomic_store(&rb->dropped, 0);

    return rb;
}

/**
 * WHAT: Destroy ring buffer
 * WHY: Free all resources
 * HOW: Frees remaining entries, then buffer, then structure
 */
static void ring_buffer_destroy(ring_buffer_t* rb)
{
    if (!rb)
        return;

    /**
     * WHAT: Free any remaining entries
     * WHY: Prevent memory leaks from unprocessed inputs
     * HOW: Iterate through valid slots and free feature arrays
     */
    if (rb->buffer) {
        for (uint32_t i = 0; i < rb->capacity; i++) {
            if (rb->buffer[i].valid && rb->buffer[i].features) {
                nimcp_free(rb->buffer[i].features);
            }
        }
        nimcp_free(rb->buffer);
    }

    nimcp_mutex_destroy(&rb->tail_lock);
    nimcp_free(rb);
}

/**
 * WHAT: Enqueue input to ring buffer (producer)
 * WHY: Add input to processing queue
 * HOW: Lock-free CAS operation on head pointer
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (lock-free)
 *
 * @param rb Ring buffer
 * @param features Input features (copied)
 * @param num_features Size of feature vector
 * @param timestamp Input timestamp
 * @param drop_on_full Drop oldest if full?
 * @return true if enqueued, false if full
 */
static bool ring_buffer_enqueue(ring_buffer_t* rb, const float* features, uint32_t num_features,
                                uint64_t timestamp, bool drop_on_full)
{
    /**
     * WHAT: Get current head position atomically
     * WHY: Multiple producers might be enqueueing simultaneously
     */
    uint32_t head = atomic_load(&rb->head);
    uint32_t tail = atomic_load(&rb->tail);

    /**
     * WHAT: Check if buffer is full
     * WHY: Prevent overwriting unprocessed data
     * HOW: Full when next_head == tail
     */
    uint32_t next_head = (head + 1) & rb->mask;
    if (next_head == tail) {
        if (drop_on_full) {
            /**
             * WHAT: Drop oldest entry by advancing tail
             * WHY: Backpressure handling - prefer new data
             * HOW: Lock tail_lock to prevent race with consumer dequeue,
             *      free old entry, advance tail, then unlock
             *
             * THREAD-SAFETY: The tail_lock prevents a TOCTOU race where
             * the consumer reads buffer[tail] while the producer frees it.
             * Without this lock, the consumer could get a dangling pointer.
             */
            nimcp_mutex_lock(&rb->tail_lock);
            // Re-read tail under lock in case consumer advanced it
            tail = atomic_load(&rb->tail);
            next_head = (head + 1) & rb->mask;
            if (next_head == tail) {
                // Still full - drop oldest
                stream_input_t* old_entry = &rb->buffer[tail];
                if (old_entry->valid && old_entry->features) {
                    nimcp_free(old_entry->features);
                    old_entry->features = NULL;
                }
                old_entry->valid = false;
                atomic_store(&rb->tail, (tail + 1) & rb->mask);
                atomic_fetch_add(&rb->dropped, 1);
            }
            nimcp_mutex_unlock(&rb->tail_lock);
        } else {
            atomic_fetch_add(&rb->dropped, 1);
            return false;  /* Buffer full is normal operational state */
        }
    }

    /**
     * WHAT: Copy features to buffer slot
     * WHY: Input lifetime is managed by caller, we need our own copy
     * HOW: Allocate and memcpy
     */
    stream_input_t* entry = &rb->buffer[head];

    // Free old features if slot was previously used
    if (entry->valid && entry->features) {
        nimcp_free(entry->features);
    }

    entry->features = nimcp_malloc(num_features * sizeof(float));
    if (!entry->features) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, num_features * sizeof(float),
                          "Failed to allocate stream input features buffer");
        return false;
    }

    memcpy(entry->features, features, num_features * sizeof(float));
    entry->num_features = num_features;
    entry->timestamp = timestamp;
    entry->valid = true;

    /**
     * WHAT: Publish the entry by advancing head
     * WHY: Make entry visible to consumer
     * HOW: Atomic store (release semantics)
     */
    atomic_store(&rb->head, next_head);
    atomic_fetch_add(&rb->enqueued, 1);

    return true;
}

/**
 * WHAT: Dequeue input from ring buffer (consumer)
 * WHY: Get next input for processing
 * HOW: Lock-free read from tail position
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (single consumer assumed)
 *
 * @param rb Ring buffer
 * @param features Output parameter for features (caller must free)
 * @param num_features Output parameter for size
 * @param timestamp Output parameter for timestamp
 * @return true if dequeued, false if empty
 */
static bool ring_buffer_dequeue(ring_buffer_t* rb, float** features, uint32_t* num_features,
                                uint64_t* timestamp)
{
    /**
     * WHAT: Lock tail_lock to prevent race with drop_on_full in enqueue
     * WHY: Both dequeue and drop_on_full read buffer[tail] and advance tail.
     *      Without mutual exclusion, the producer could free buffer[tail].features
     *      while the consumer is reading it, causing use-after-free / double-free.
     */
    nimcp_mutex_lock(&rb->tail_lock);

    uint32_t tail = atomic_load(&rb->tail);
    uint32_t head = atomic_load(&rb->head);

    /**
     * WHAT: Check if buffer is empty
     * WHY: Can't dequeue from empty buffer
     * HOW: Empty when tail == head
     */
    if (tail == head) {
        nimcp_mutex_unlock(&rb->tail_lock);
        return false;  /* Buffer empty is normal operational state */
    }

    stream_input_t* entry = &rb->buffer[tail];

    /**
     * WHAT: Return entry data (transfer ownership)
     * WHY: Caller now owns the feature array
     * HOW: Return pointer, mark entry invalid
     */
    *features = entry->features;
    *num_features = entry->num_features;
    *timestamp = entry->timestamp;

    entry->features = NULL;  // Ownership transferred
    entry->valid = false;

    /**
     * WHAT: Advance tail to make slot available
     * WHY: Allow producer to reuse this slot
     * HOW: Atomic store (release semantics)
     */
    atomic_store(&rb->tail, (tail + 1) & rb->mask);
    atomic_fetch_add(&rb->dequeued, 1);

    nimcp_mutex_unlock(&rb->tail_lock);
    return true;
}

/**
 * WHAT: Get current queue size
 * WHY: Monitoring and backpressure detection
 * HOW: Calculate head - tail (with wrap-around)
 */
static uint32_t ring_buffer_size(ring_buffer_t* rb)
{
    uint32_t head = atomic_load(&rb->head);
    uint32_t tail = atomic_load(&rb->tail);
    return (head - tail) & rb->mask;
}

//=============================================================================
// Stream Structure (Opaque Implementation)
//=============================================================================

/**
 * WHAT: Complete stream state
 * WHY: Encapsulates all stream data and threads
 * PATTERN: Opaque pointer (Pimpl idiom) - hides implementation
 */
struct brain_stream_struct {
    // Brain association
    brain_t brain;  // Brain being streamed to

    // Configuration (copied from user)
    stream_config_t config;

    // Input queue (Producer-Consumer pattern)
    ring_buffer_t* input_queue;

    // Output cache (mutex-protected)
    nimcp_mutex_t decision_lock;
    brain_decision_t* cached_decision;  // Most recent decision
    float cached_salience;              // Most recent salience

    // Background processing thread (if BACKGROUND or BATCHED mode)
    nimcp_thread_t processing_thread;
    bool thread_running;
    bool thread_should_stop;
    nimcp_mutex_t control_lock;  // For pause/resume

    // State flags
    bool paused;

    // Statistics (atomic for thread-safe access)
    atomic_uint_fast64_t stats_inputs_fed;
    atomic_uint_fast64_t stats_inputs_processed;
    atomic_uint_fast64_t stats_decisions_generated;
    atomic_uint_fast64_t stats_processing_time_ns;  // Accumulated time

    // Timing for throughput calculation
    uint64_t last_stats_time;
    uint64_t last_stats_count;

    // Memory Integration (Phase IO-1)
    unified_mem_manager_t memory_manager;  // Unified memory manager
    bool owns_memory_manager;              // Did we create it internally?
    uint64_t pool_allocations;             // Allocations from pool
    uint64_t malloc_allocations;           // Fallback allocations

    // Security Integration (Phase IO-2)
    nimcp_sec_integration_t* security_ctx; // Security context
    uint32_t security_region_id;           // Registered memory region
    bool security_registered;              // Is this stream registered?
};

//=============================================================================
// Forward Declarations for Processing Thread
//=============================================================================

static void* stream_processing_thread(void* arg);
static void stream_process_single_input(brain_stream_t stream, const float* features,
                                        uint32_t num_features, uint64_t timestamp);

//=============================================================================
// Stream Creation and Destruction (Factory Pattern)
//=============================================================================

/**
 * WHAT: Validate stream configuration
 * WHY: Guard clause pattern - fail fast on invalid config
 * HOW: Check all configuration parameters for validity
 *
 * @param config Configuration to validate
 * @return true if valid, false otherwise
 */
static bool validate_stream_config(const stream_config_t* config)
{
    if (!config) {
        stream_set_error("NULL configuration");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "validate_stream_config: config is NULL");
        return false;
    }

    /**
     * WHAT: Validate buffer size is power of 2
     * WHY: Ring buffer requires power-of-2 for fast modulo
     * HOW: Check if (size & (size-1)) == 0
     */
    if (config->buffer_size == 0) {
        stream_set_error("Buffer size cannot be zero");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "validate_stream_config: config->buffer_size is zero");
        return false;
    }

    // Mode-specific validation
    if (config->mode == STREAM_MODE_BACKGROUND && config->processing_interval_ms == 0) {
        stream_set_error("Background mode requires non-zero processing interval");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "validate_stream_config: config->processing_interval_ms is zero");
        return false;
    }

    if (config->mode == STREAM_MODE_BATCHED && config->batch_size == 0) {
        stream_set_error("Batched mode requires non-zero batch size");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "validate_stream_config: config->batch_size is zero");
        return false;
    }

    return true;
}

/**
 * WHAT: Get default stream configuration
 * WHY: Provide sensible defaults for common use cases
 * HOW: Returns pre-initialized config struct
 *
 * DEFAULT CONFIGURATION:
 * - Mode: SYNCHRONOUS (simplest, no threads)
 * - Buffer: 1024 entries (power of 2)
 * - Processing: 100ms interval for background mode
 * - Batch size: 10 inputs per batch
 * - Thresholds: 0.8 for high salience/surprise
 * - Callbacks: All NULL (user must set)
 * - Caching: Enabled for performance
 * - Salience: Enabled for attention filtering
 *
 * @return Default configuration struct
 */
stream_config_t stream_default_config(void)
{
    stream_config_t config = {.mode = STREAM_MODE_SYNCHRONOUS,

                              // Buffer configuration
                              .buffer_size = 1024,
                              .drop_on_full = false,  // Block instead of dropping

                              // Processing configuration
                              .processing_interval_ms = 100,  // 100ms for background mode
                              .batch_size = 10,               // 10 inputs per batch

                              // Thresholds for events
                              .high_salience_threshold = NIMCP_CONFIDENCE_HIGH,
                              .high_surprise_threshold = NIMCP_CONFIDENCE_HIGH,

                              // Event callbacks (NULL = not used)
                              .on_high_salience = NULL,
                              .on_high_surprise = NULL,
                              .on_decision_ready = NULL,
                              .on_buffer_full = NULL,
                              .on_error = NULL,
                              .callback_context = NULL,

                              // Performance tuning
                              .enable_decision_caching = true,
                              .enable_salience_evaluation = true};

    return config;
}

brain_stream_t brain_create_stream(brain_t brain, const stream_config_t* config)
{
    /**
     * WHAT: Validate parameters (Guard clauses)
     * WHY: Fail fast pattern - catch errors before allocation
     */
    if (!brain) {
        stream_set_error("NULL brain");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain is NULL");

        return NULL;
    }

    if (!validate_stream_config(config)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_create_stream: validate_stream_config is NULL");
        return NULL;  // Error already set
    }

    /**
     * WHAT: Allocate stream structure
     * WHY: Create opaque handle for stream
     * HOW: Use tracked memory allocation
     */
    brain_stream_t stream = nimcp_calloc(1, sizeof(struct brain_stream_struct));
    if (!stream) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, sizeof(struct brain_stream_struct),
                          "Failed to allocate brain stream structure");
        stream_set_error("Failed to allocate stream structure");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "stream is NULL");

        return NULL;
    }

    // Initialize brain reference
    stream->brain = brain;

    // Copy configuration
    memcpy(&stream->config, config, sizeof(stream_config_t));

    /**
     * WHAT: Create input queue (Producer-Consumer pattern)
     * WHY: Decouple input feeding from processing
     * HOW: Lock-free ring buffer
     */
    stream->input_queue = ring_buffer_create(config->buffer_size);
    if (!stream->input_queue) {
        stream_set_error("Failed to create input queue");
        nimcp_free(stream);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "brain_create_stream: stream->input_queue is NULL");
        return NULL;
    }

    // Initialize mutex for decision cache
    nimcp_mutex_init(&stream->decision_lock, NULL);
    nimcp_mutex_init(&stream->control_lock, NULL);

    stream->cached_decision = NULL;
    stream->cached_salience = 0.0F;
    stream->paused = false;

    // Initialize statistics
    atomic_store(&stream->stats_inputs_fed, 0);
    atomic_store(&stream->stats_inputs_processed, 0);
    atomic_store(&stream->stats_decisions_generated, 0);
    atomic_store(&stream->stats_processing_time_ns, 0);

    stream->last_stats_time = nimcp_time_monotonic_ns();
    stream->last_stats_count = 0;

    /**
     * WHAT: Start background thread if needed
     * WHY: BACKGROUND and BATCHED modes require async processing
     * HOW: Create pthread with processing function
     */
    if (config->mode == STREAM_MODE_BACKGROUND || config->mode == STREAM_MODE_BATCHED) {
        // Initialize threading subsystem if not already done
        nimcp_thread_init();

        stream->thread_running = false;
        stream->thread_should_stop = false;

        if (nimcp_thread_create(&stream->processing_thread, stream_processing_thread, stream,
                                NULL) != 0) {
            stream_set_error("Failed to create processing thread");
            ring_buffer_destroy(stream->input_queue);
            nimcp_mutex_destroy(&stream->decision_lock);
            nimcp_mutex_destroy(&stream->control_lock);
            nimcp_free(stream);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "brain_create_stream: operation failed");
            return NULL;
        }

        stream->thread_running = true;
    }

    //=========================================================================
    // PHASE IO-1: Unified Memory Integration
    //=========================================================================
    if (config->use_unified_memory) {
        if (config->memory_manager) {
            // Use provided memory manager
            stream->memory_manager = config->memory_manager;
            stream->owns_memory_manager = false;
        } else {
            // Create internal memory manager
            unified_mem_config_t mem_config = unified_mem_default_config();
            mem_config.enable_cow = true;
            mem_config.enable_tracking = true;
            stream->memory_manager = unified_mem_create(&mem_config);
            stream->owns_memory_manager = true;

            if (!stream->memory_manager) {
                stream_set_error("Failed to create unified memory manager");
                if (stream->thread_running) {
                    stream->thread_should_stop = true;
                    nimcp_thread_join(stream->processing_thread, NULL);
                }
                ring_buffer_destroy(stream->input_queue);
                nimcp_mutex_destroy(&stream->decision_lock);
                nimcp_mutex_destroy(&stream->control_lock);
                nimcp_free(stream);
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_create_stream: validation failed");
                return NULL;
            }
        }
        LOG_DEBUG("Stream using unified memory with CoW support");
    }

    //=========================================================================
    // PHASE IO-2: Security Module Registration
    //=========================================================================
    if (config->enable_security) {
        // Prefer provided security context, fall back to global
        stream->security_ctx = config->security_context ? config->security_context
                                                        : g_stream_security_ctx;

        if (stream->security_ctx && g_stream_security_module_id != 0) {
            // Register stream configuration as a monitored region
            nimcp_result_t result = nimcp_sec_register_region(
                stream->security_ctx,
                g_stream_security_module_id,
                "stream_config",
                &stream->config,
                sizeof(stream_config_t),
                &stream->security_region_id
            );

            if (result == NIMCP_SUCCESS) {
                stream->security_registered = true;
                LOG_DEBUG("Stream registered with security (region: %u)",
                         stream->security_region_id);

                // Record successful initialization
                NIMCP_SEC_SUCCESS(stream->security_ctx, g_stream_security_module_id);
            } else {
                LOG_WARNING("Failed to register stream with security");
            }
        }
    }

    return stream;
}

void brain_destroy_stream(brain_stream_t stream)
{
    if (!stream)
        return;

    //=========================================================================
    // PHASE IO-2: Unregister from security
    //=========================================================================
    if (stream->security_registered && stream->security_ctx) {
        // Record successful operation before unregistering
        NIMCP_SEC_SUCCESS(stream->security_ctx, g_stream_security_module_id);

        // Unregister memory region
        nimcp_sec_unregister_region(stream->security_ctx, stream->security_region_id);
        LOG_DEBUG("Stream unregistered from security");
    }

    /**
     * WHAT: Stop background thread if running
     * WHY: Clean shutdown before freeing resources
     * HOW: Set stop flag, wait for thread to finish
     */
    if (stream->thread_running) {
        stream->thread_should_stop = true;
        nimcp_thread_join(stream->processing_thread, NULL);
    }

    //=========================================================================
    // PHASE IO-1: Clean up unified memory
    //=========================================================================
    if (stream->memory_manager && stream->owns_memory_manager) {
        unified_mem_destroy(stream->memory_manager);
        LOG_DEBUG("Stream unified memory manager destroyed");
    }

    // Destroy input queue (frees any pending inputs)
    ring_buffer_destroy(stream->input_queue);

    // Free cached decision
    if (stream->cached_decision) {
        brain_free_decision(stream->cached_decision);
    }

    // Destroy mutexes
    nimcp_mutex_destroy(&stream->decision_lock);
    nimcp_mutex_destroy(&stream->control_lock);

    // Free stream structure
    nimcp_free(stream);
}

//=============================================================================
// Stream Input Functions (Producer Side)
//=============================================================================

bool brain_stream_feed(brain_stream_t stream, const float* features, uint32_t num_features,
                       uint64_t timestamp)
{
    /**
     * WHAT: Validate parameters (Guard clauses)
     * WHY: Prevent invalid operations
     */
    if (!stream) {
        stream_set_error("NULL stream");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_stream_feed: stream is NULL");
        return false;
    }

    if (!features) {
        stream_set_error("NULL features");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_stream_feed: features is NULL");
        return false;
    }

    /**
     * WHAT: Validate feature size matches brain input dimension
     * WHY: Prevent buffer overflows and memory corruption (Issue #5555)
     * HOW: Check num_features against brain's expected input size
     */
    uint32_t expected_inputs = brain_get_num_inputs(stream->brain);
    if (num_features != expected_inputs) {
        stream_set_error("Feature size mismatch: got %u, expected %u",
                        num_features, expected_inputs);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_stream_feed: validation failed");
        return false;
    }

    /**
     * WHAT: Handle based on stream mode (Strategy pattern)
     * WHY: Different modes have different processing strategies
     * HOW: Switch on mode, delegate to appropriate handler
     */
    atomic_fetch_add(&stream->stats_inputs_fed, 1);

    if (stream->config.mode == STREAM_MODE_SYNCHRONOUS) {
        /**
         * WHAT: Process immediately in caller's thread
         * WHY: Synchronous mode has no background thread
         * HOW: Direct function call
         */
        stream_process_single_input(stream, features, num_features, timestamp);
        return true;
    } else {
        /**
         * WHAT: Enqueue for background processing
         * WHY: Non-blocking for caller
         * HOW: Add to ring buffer, background thread will process
         */
        return ring_buffer_enqueue(stream->input_queue, features, num_features, timestamp,
                                   stream->config.drop_on_full);
    }
}

uint32_t brain_stream_feed_batch(brain_stream_t stream, const float** features,
                                 uint32_t num_samples, uint32_t num_features,
                                 const uint64_t* timestamps)
{
    if (!stream || !features || !timestamps) {
        return 0;
    }

    uint32_t enqueued = 0;
    for (uint32_t i = 0; i < num_samples; i++) {
        if (brain_stream_feed(stream, features[i], num_features, timestamps[i])) {
            enqueued++;
        }
    }

    return enqueued;
}

//=============================================================================
// Stream Output Functions (Consumer Side)
//=============================================================================

brain_decision_t* brain_stream_get_decision(brain_stream_t stream)
{
    if (!stream) {
        stream_set_error("NULL stream");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stream is NULL");

        return NULL;
    }

    /**
     * WHAT: Deprecated - streams no longer cache decisions
     * WHY: Brain already caches decisions efficiently
     * HOW: Return NULL
     *
     * BUG FIX: Stream decision caching caused double-free bugs.
     * Removed caching to fix. Applications should call brain_decide()
     * directly if they need the decision, or use stream callbacks.
     *
     * @deprecated This function now always returns NULL. Use callbacks instead.
     */
    return NULL;  /* Deprecated function - always returns NULL */
}

float brain_stream_get_salience(brain_stream_t stream)
{
    if (!stream) {
        return -1.0F;
    }

    nimcp_mutex_lock(&stream->decision_lock);
    float salience = stream->cached_salience;
    nimcp_mutex_unlock(&stream->decision_lock);

    return salience;
}

bool brain_stream_get_stats(brain_stream_t stream, stream_stats_t* stats)
{
    if (!stream || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_stream_get_stats: required parameter is NULL (stream, stats)");
        return false;
    }

    /**
     * WHAT: Gather statistics from atomic counters
     * WHY: Provide visibility into stream performance
     * HOW: Read atomics, calculate derived metrics
     */
    stats->inputs_fed = atomic_load(&stream->stats_inputs_fed);
    stats->inputs_processed = atomic_load(&stream->stats_inputs_processed);
    stats->inputs_dropped = atomic_load(&stream->input_queue->dropped);
    stats->decisions_generated = atomic_load(&stream->stats_decisions_generated);

    // Calculate average processing time
    uint64_t total_time_ns = atomic_load(&stream->stats_processing_time_ns);
    uint64_t processed = stats->inputs_processed;
    stats->avg_processing_time_ms =
        processed > 0 ? (float) total_time_ns / processed / 1000000.0F : 0.0F;

    // Calculate current throughput
    uint64_t elapsed_ns = nimcp_time_elapsed_ns(stream->last_stats_time);
    double elapsed = elapsed_ns / 1e9;

    uint64_t count_delta = stats->inputs_processed - stream->last_stats_count;
    stats->current_throughput = elapsed > 0 ? (float) count_delta / elapsed : 0.0F;

    // Queue stats
    stats->queue_size = ring_buffer_size(stream->input_queue);
    stats->queue_capacity = stream->input_queue->capacity;
    stats->avg_queue_depth = (float) stats->queue_size / stats->queue_capacity;

    return true;
}

/**
 * WHAT: Reset stream statistics
 * WHY: Allow clearing counters for fresh measurements
 * HOW: Atomically reset all statistics counters to zero
 *
 * THREAD-SAFE: Yes (atomic operations)
 *
 * @param stream Stream handle
 * @return true on success, false on error
 */
bool brain_stream_reset_stats(brain_stream_t stream)
{
    if (!stream) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_stream_reset_stats: stream is NULL");
        return false;
    }

    /**
     * WHAT: Reset all atomic counters
     * WHY: Provide clean slate for new measurement period
     * HOW: Atomic store of zero to all counters
     */
    atomic_store(&stream->stats_inputs_fed, 0);
    atomic_store(&stream->stats_inputs_processed, 0);
    atomic_store(&stream->stats_decisions_generated, 0);
    atomic_store(&stream->stats_processing_time_ns, 0);

    /**
     * WHAT: Reset ring buffer statistics
     * WHY: Input queue has its own drop counter
     * HOW: Direct atomic store to ring buffer counters
     */
    atomic_store(&stream->input_queue->dropped, 0);
    atomic_store(&stream->input_queue->enqueued, 0);
    atomic_store(&stream->input_queue->dequeued, 0);

    /**
     * WHAT: Reset timing for throughput calculation
     * WHY: Throughput is calculated as delta over time
     * HOW: Record new baseline timestamp and count
     */
    stream->last_stats_time = nimcp_time_monotonic_ns();
    stream->last_stats_count = 0;

    return true;
}

//=============================================================================
// Stream Processing (Core Logic)
//=============================================================================

/**
 * WHAT: Process single input through brain
 * WHY: Core processing logic shared by all modes
 * HOW: Call brain_decide(), cache result, trigger callbacks
 *
 * @param stream Stream handle
 * @param features Input features
 * @param num_features Size of feature vector
 * @param timestamp Input timestamp
 */
static void stream_process_single_input(brain_stream_t stream, const float* features,
                                        uint32_t num_features, uint64_t timestamp)
{
    uint64_t start_time = nimcp_time_monotonic_ns();

    /**
     * WHAT: Run brain decision
     * WHY: Get neural network output
     * HOW: Call brain API with features
     */
    brain_decision_t* decision = brain_decide(stream->brain, features, num_features);

    if (decision) {
        /**
         * WHAT: Update stream state with decision info (but don't cache decision pointer)
         * WHY: brain_decide() returns a decision that caller owns. If we cache it here,
         *      we risk double-free when both stream_destroy and caller try to free it.
         *      Instead, just track salience and let brain handle decision caching.
         * HOW: Extract info we need, then free the decision
         *
         * BUG FIX: Previously cached decision pointer, causing double-free with brain's cache
         */
        nimcp_mutex_lock(&stream->decision_lock);

        // Update salience (simplified: use confidence as proxy)
        stream->cached_salience = decision->confidence;

        // Free old cached decision
        if (stream->cached_decision) {
            brain_free_decision(stream->cached_decision);
        }
        stream->cached_decision = NULL;  // Don't cache - brain already does

        nimcp_mutex_unlock(&stream->decision_lock);

        atomic_fetch_add(&stream->stats_decisions_generated, 1);

        /**
         * WHAT: Trigger event callbacks if thresholds exceeded
         * WHY: Observer pattern - notify application of interesting events
         * HOW: Check thresholds, call user callbacks
         */
        if (stream->config.enable_salience_evaluation &&
            decision->confidence > stream->config.high_salience_threshold &&
            stream->config.on_high_salience) {
            stream_event_t event = {.type = STREAM_EVENT_HIGH_SALIENCE,
                                    .timestamp = timestamp,
                                    .magnitude = decision->confidence,
                                    .decision = decision,
                                    .message = "High salience detected"};

            stream->config.on_high_salience(&event, stream->config.callback_context);
        }

        if (stream->config.on_decision_ready) {
            stream_event_t event = {.type = STREAM_EVENT_DECISION_READY,
                                    .timestamp = timestamp,
                                    .magnitude = 1.0F,
                                    .decision = decision,
                                    .message = "Decision ready"};

            stream->config.on_decision_ready(&event, stream->config.callback_context);
        }

        // Free the decision since we own it and don't need to cache it
        brain_free_decision(decision);
    }

    atomic_fetch_add(&stream->stats_inputs_processed, 1);

    // Track processing time
    uint64_t elapsed_ns = nimcp_time_elapsed_ns(start_time);
    atomic_fetch_add(&stream->stats_processing_time_ns, elapsed_ns);
}

/**
 * WHAT: Background processing thread function
 * WHY: Asynchronous input processing
 * HOW: Loop dequeuing and processing inputs until stopped
 *
 * PATTERN: Producer-Consumer - this is the consumer
 *
 * @param arg Stream handle (cast from void*)
 * @return NULL (unused)
 */
static void* stream_processing_thread(void* arg)
{
    brain_stream_t stream = (brain_stream_t) arg;

    /**
     * WHAT: Main processing loop
     * WHY: Continuously process inputs until shutdown
     * HOW: Dequeue → Process → Sleep (if no work)
     */
    while (!stream->thread_should_stop) {
        /**
         * WHAT: Check if paused
         * WHY: Allow temporary suspension of processing
         * HOW: Sleep while paused
         */
        nimcp_mutex_lock(&stream->control_lock);
        bool is_paused = stream->paused;
        nimcp_mutex_unlock(&stream->control_lock);

        if (is_paused) {
            usleep(10000);  // 10ms
            continue;
        }

        /**
         * WHAT: Dequeue and process inputs
         * WHY: Work through input queue
         * HOW: Dequeue until empty or batch size reached
         */
        bool processed_any = false;
        uint32_t batch_count = 0;
        uint32_t max_batch =
            stream->config.mode == STREAM_MODE_BATCHED ? stream->config.batch_size : 1;

        while (batch_count < max_batch) {
            float* features;
            uint32_t num_features;
            uint64_t timestamp;

            if (ring_buffer_dequeue(stream->input_queue, &features, &num_features, &timestamp)) {
                stream_process_single_input(stream, features, num_features, timestamp);
                nimcp_free(features);  // Free after processing
                processed_any = true;
                batch_count++;
            } else {
                break;  // Queue empty
            }
        }

        /**
         * WHAT: Sleep if no work
         * WHY: Avoid busy-waiting (CPU waste)
         * HOW: usleep for configured interval
         */
        if (!processed_any) {
            usleep(stream->config.processing_interval_ms * 1000);
        }
    }

    return NULL;  /* Worker thread normal exit */
}

//=============================================================================
// Stream Control Functions
//=============================================================================

bool brain_stream_pause(brain_stream_t stream)
{
    if (!stream) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_stream_pause: stream is NULL");
        return false;
    }

    nimcp_mutex_lock(&stream->control_lock);
    stream->paused = true;
    nimcp_mutex_unlock(&stream->control_lock);

    return true;
}

bool brain_stream_resume(brain_stream_t stream)
{
    if (!stream) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_stream_resume: stream is NULL");
        return false;
    }

    nimcp_mutex_lock(&stream->control_lock);
    stream->paused = false;
    nimcp_mutex_unlock(&stream->control_lock);

    return true;
}

bool brain_stream_flush(brain_stream_t stream, uint32_t timeout_ms)
{
    if (!stream) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_stream_flush: stream is NULL");
        return false;
    }

    /**
     * WHAT: Wait for queue to drain
     * WHY: Ensure all inputs processed before continuing
     * HOW: Poll queue size with timeout
     */
    uint32_t elapsed = 0;
    uint32_t sleep_interval = 10;  // 10ms

    while (ring_buffer_size(stream->input_queue) > 0) {
        if (timeout_ms > 0 && elapsed >= timeout_ms) {
            return false;  /* Timeout is normal operational state */
        }

        usleep(sleep_interval * 1000);
        elapsed += sleep_interval;
    }

    return true;
}

bool brain_stream_clear(brain_stream_t stream)
{
    if (!stream) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_stream_clear: stream is NULL");
        return false;
    }

    /**
     * WHAT: Drop all pending inputs
     * WHY: Reset stream state
     * HOW: Dequeue and free all entries
     */
    float* features;
    uint32_t num_features;
    uint64_t timestamp;

    while (ring_buffer_dequeue(stream->input_queue, &features, &num_features, &timestamp)) {
        nimcp_free(features);
    }

    return true;
}

//=============================================================================
// Module Initialization and Security Registration (Phase IO-2)
//=============================================================================

/**
 * WHAT: Initialize Stream module with security registration
 * WHY: Enable trust tracking and integrity monitoring
 * HOW: Register as I/O module with security system
 */
nimcp_result_t stream_init(nimcp_sec_integration_t* security_ctx)
{
    if (nimcp_atomic_load_bool(&g_stream_initialized, NIMCP_MEMORY_ORDER_ACQUIRE)) {
        return NIMCP_SUCCESS;  // Already initialized
    }

    // Prevent concurrent initialization: only one thread proceeds
    bool expected = false;
    if (!nimcp_atomic_compare_exchange_bool(&g_stream_initializing, &expected, true,
                                            NIMCP_MEMORY_ORDER_ACQ_REL)) {
        // Another thread is initializing, spin-wait for completion
        while (!nimcp_atomic_load_bool(&g_stream_initialized, NIMCP_MEMORY_ORDER_ACQUIRE)) {
            // busy wait
        }
        return NIMCP_SUCCESS;
    }

    // Double-check after acquiring the initializing flag
    if (nimcp_atomic_load_bool(&g_stream_initialized, NIMCP_MEMORY_ORDER_ACQUIRE)) {
        nimcp_atomic_store_bool(&g_stream_initializing, false, NIMCP_MEMORY_ORDER_RELEASE);
        return NIMCP_SUCCESS;
    }

    // Store security context
    g_stream_security_ctx = security_ctx;

    // Register with security module if context provided
    if (security_ctx) {
        nimcp_result_t result = nimcp_sec_register_module(
            security_ctx,
            "stream",
            NIMCP_SEC_CAT_IO,
            &g_stream_security_module_id
        );

        if (result != NIMCP_SUCCESS) {
            stream_set_error("Failed to register Stream module with security");
            nimcp_atomic_store_bool(&g_stream_initializing, false, NIMCP_MEMORY_ORDER_RELEASE);
            return result;
        }

        LOG_INFO("Stream module registered with security (ID: %u)", g_stream_security_module_id);
    }

    nimcp_atomic_store_bool(&g_stream_initialized, true, NIMCP_MEMORY_ORDER_RELEASE);
    nimcp_atomic_store_bool(&g_stream_initializing, false, NIMCP_MEMORY_ORDER_RELEASE);
    return NIMCP_SUCCESS;
}

/**
 * WHAT: Shutdown Stream module
 * WHY: Unregister from security module
 * HOW: Call security unregister
 */
void stream_shutdown(void)
{
    if (!nimcp_atomic_load_bool(&g_stream_initialized, NIMCP_MEMORY_ORDER_ACQUIRE)) {
        return;
    }

    // Unregister from security module
    if (g_stream_security_ctx && g_stream_security_module_id != 0) {
        nimcp_sec_unregister_module(g_stream_security_ctx, g_stream_security_module_id);
        LOG_INFO("Stream module unregistered from security");
    }

    g_stream_security_ctx = NULL;
    g_stream_security_module_id = 0;
    nimcp_atomic_store_bool(&g_stream_initialized, false, NIMCP_MEMORY_ORDER_RELEASE);
}

/**
 * WHAT: Get Stream module's security ID
 * WHY: Allow external code to reference this module
 * HOW: Return global module ID
 */
uint32_t stream_get_security_module_id(void)
{
    return g_stream_security_module_id;
}

/**
 * WHAT: Get extended stream statistics
 * WHY: Monitor memory and security performance
 * HOW: Aggregate statistics from stream and subsystems
 */
bool brain_stream_get_extended_stats(brain_stream_t stream, stream_extended_stats_t* stats)
{
    if (!stream || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_stream_get_extended_stats: required parameter is NULL (stream, stats)");
        return false;
    }

    memset(stats, 0, sizeof(stream_extended_stats_t));

    // Get base statistics
    if (!brain_stream_get_stats(stream, &stats->base)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_stream_get_extended_stats: brain_stream_get_stats is NULL");
        return false;
    }

    // Memory statistics
    stats->using_unified_memory = (stream->memory_manager != NULL);
    stats->pool_allocations = stream->pool_allocations;
    stats->malloc_allocations = stream->malloc_allocations;
    stats->ring_buffer_memory = stream->input_queue ?
        stream->input_queue->capacity * sizeof(stream_input_t) : 0;

    if (stream->memory_manager) {
        unified_mem_stats_t mem_stats;
        if (unified_mem_get_stats(stream->memory_manager, &mem_stats)) {
            stats->cow_memory_saved = mem_stats.memory_saved_bytes;
        }
    }

    // Security statistics
    stats->security_registered = stream->security_registered;
    if (stream->security_registered && stream->security_ctx) {
        stats->security_module_id = g_stream_security_module_id;

        nimcp_sec_module_info_t mod_info;
        if (nimcp_sec_get_module_info(stream->security_ctx,
                                      g_stream_security_module_id,
                                      &mod_info) == NIMCP_SUCCESS) {
            stats->security_interactions = mod_info.interaction_count;
            stats->security_anomalies = mod_info.anomaly_count;
            stats->trust_score = mod_info.trust_score.expected_trust;
        }
    }

    return true;
}
