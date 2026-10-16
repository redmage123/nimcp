/**
 * @file nimcp_stream.h
 * @brief Continuous input streaming API for active consciousness
 *
 * WHAT: Provides non-blocking continuous input stream processing for brains
 * WHY: Active consciousness needs continuous sensory input, not one-shot queries
 * HOW: Producer-consumer pattern with ring buffer and optional background thread
 *
 * DESIGN PATTERNS:
 * - Producer-Consumer: Input queue with thread-safe operations
 * - Observer Pattern: Event callbacks for stream conditions
 * - Strategy Pattern: Different processing modes (blocking, non-blocking, batched)
 * - Factory Pattern: Stream creation with validated configuration
 *
 * ARCHITECTURE:
 *   Application Thread          Stream Thread (Optional)
 *   ================            ===================
 *   brain_stream_feed()  →  [Ring Buffer]  →  Process inputs
 *                                           ↓
 *   brain_stream_get_decision()  ←  [Decision Cache]
 *                                           ↓
 *   stream_callback()  ←───────────  Events (salience, surprise, etc.)
 *
 * THREAD SAFETY:
 * - All functions are thread-safe using mutexes
 * - Ring buffer uses lock-free operations where possible
 * - Decision cache is mutex-protected
 *
 * PERFORMANCE:
 * - Feed input: O(1) - just enqueue
 * - Get decision: O(1) - read from cache
 * - Background processing: Configurable batch size and frequency
 *
 * EXAMPLE:
 * @code
 *   // Create stream
 *   stream_config_t config = {
 *       .mode = STREAM_MODE_BACKGROUND,
 *       .buffer_size = 1000,
 *       .processing_interval_ms = 100,
 *       .on_high_salience = my_callback
 *   };
 *
 *   brain_stream_t stream = brain_create_stream(brain, &config);
 *
 *   // Feed continuous input (non-blocking)
 *   while (sensory_input_available()) {
 *       brain_stream_feed(stream, features, 13, timestamp);
 *   }
 *
 *   // Get latest decision (non-blocking)
 *   brain_decision_t* decision = brain_stream_get_decision(stream);
 *
 *   // Cleanup
 *   brain_destroy_stream(stream);
 * @endcode
 */

#ifndef NIMCP_STREAM_H
#define NIMCP_STREAM_H

#include <stdbool.h>
#include <stdint.h>
#include "core/brain/nimcp_brain.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "security/nimcp_security_integration.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations and Opaque Types
//=============================================================================

/**
 * WHAT: Opaque stream handle
 * WHY: Hides implementation details, allows interface evolution
 * PATTERN: Opaque pointer (Pimpl idiom)
 */
typedef struct brain_stream_struct* brain_stream_t;

//=============================================================================
// Stream Modes and Configuration
//=============================================================================

/**
 * WHAT: Stream processing modes
 * WHY: Different applications need different processing strategies
 * HOW: Strategy pattern - behavior changes based on mode
 */
typedef enum {
    /**
     * WHAT: Synchronous processing - process on feed
     * WHY: Simple, predictable, no background threads
     * WHEN: Low-frequency input, deterministic timing needed
     */
    STREAM_MODE_SYNCHRONOUS,

    /**
     * WHAT: Asynchronous processing - background thread processes queue
     * WHY: Non-blocking input feeding, parallel processing
     * WHEN: High-frequency input, performance critical
     */
    STREAM_MODE_BACKGROUND,

    /**
     * WHAT: Batched processing - accumulate then process in batches
     * WHY: Maximum throughput via batch optimization
     * WHEN: Very high frequency input, latency tolerance
     */
    STREAM_MODE_BATCHED
} stream_mode_t;

/**
 * WHAT: Stream event types for callbacks
 * WHY: Notify application of interesting conditions
 * PATTERN: Observer pattern - publish-subscribe for events
 */
typedef enum {
    STREAM_EVENT_HIGH_SALIENCE,  /**< Input has high attention value */
    STREAM_EVENT_HIGH_SURPRISE,  /**< Unexpected input (prediction error) */
    STREAM_EVENT_DECISION_READY, /**< New decision available */
    STREAM_EVENT_BUFFER_FULL,    /**< Input buffer is full (backpressure) */
    STREAM_EVENT_ERROR           /**< Processing error occurred */
} stream_event_type_t;

/**
 * WHAT: Stream event data
 * WHY: Provide context about event to callback
 */
typedef struct {
    stream_event_type_t type;   /**< Event type */
    uint64_t timestamp;         /**< When event occurred */
    float magnitude;            /**< Event strength (0.0-1.0) */
    brain_decision_t* decision; /**< Associated decision (if applicable) */
    const char* message;        /**< Human-readable description */
} stream_event_t;

/**
 * WHAT: Stream event callback function type
 * WHY: Allow application to react to stream events
 * PATTERN: Observer pattern - callback is the observer
 *
 * @param event Event that occurred
 * @param context User-provided context pointer
 */
typedef void (*stream_event_callback_fn)(const stream_event_t* event, void* context);

/**
 * WHAT: Stream configuration
 * WHY: Flexible configuration for different use cases
 * PATTERN: Builder pattern - step-by-step configuration
 */
typedef struct {
    // Mode configuration
    stream_mode_t mode; /**< Processing mode */

    // Buffer configuration
    uint32_t buffer_size; /**< Input queue size (must be power of 2) */
    bool drop_on_full;    /**< Drop old inputs if buffer full? */

    // Processing configuration
    uint32_t processing_interval_ms; /**< How often to process (background mode) */
    uint32_t batch_size;             /**< Batch size (batched mode) */

    // Thresholds for events
    float high_salience_threshold; /**< Trigger callback above this salience */
    float high_surprise_threshold; /**< Trigger callback above this surprise */

    // Event callbacks (Observer pattern)
    stream_event_callback_fn on_high_salience;  /**< Called on high attention */
    stream_event_callback_fn on_high_surprise;  /**< Called on surprise */
    stream_event_callback_fn on_decision_ready; /**< Called when decision ready */
    stream_event_callback_fn on_buffer_full;    /**< Called on backpressure */
    stream_event_callback_fn on_error;          /**< Called on error */
    void* callback_context;                     /**< User context for callbacks */

    // Performance tuning
    bool enable_decision_caching;    /**< Cache decisions for fast retrieval? */
    bool enable_salience_evaluation; /**< Compute salience scores? */

    // Memory Integration (Phase IO-1)
    bool use_unified_memory;              /**< Use unified memory for ring buffer */
    unified_mem_manager_t memory_manager; /**< External memory manager (NULL = create internal) */

    // Security Integration (Phase IO-2)
    bool enable_security;                       /**< Enable security module integration */
    nimcp_sec_integration_t* security_context;  /**< External security context (NULL = use global) */

} stream_config_t;

//=============================================================================
// Configuration Helpers
//=============================================================================

/**
 * WHAT: Get default stream configuration
 * WHY: Sensible defaults for most use cases
 * HOW: Returns pre-configured struct with balanced settings
 *
 * @return Default configuration struct
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
stream_config_t stream_default_config(void);

//=============================================================================
// Stream Lifecycle Functions (Factory Pattern)
//=============================================================================

/**
 * WHAT: Create input stream for brain
 * WHY: Factory function - single creation point with validation
 * HOW: Validates config, allocates resources, starts threads if needed
 *
 * COMPLEXITY: O(1) for structure allocation, O(n) if pre-allocating buffer
 * THREAD-SAFE: Yes
 * PATTERN: Factory pattern
 *
 * @param brain Brain to attach stream to
 * @param config Stream configuration (copied internally)
 * @return Stream handle or NULL on error
 *
 * ERROR CONDITIONS:
 * - NULL brain: Returns NULL, sets error "Invalid brain"
 * - NULL config: Returns NULL, sets error "Invalid config"
 * - Invalid buffer_size: Returns NULL, sets error "Buffer size must be power of 2"
 * - Thread creation failure: Returns NULL, sets error "Failed to create thread"
 */
brain_stream_t brain_create_stream(brain_t brain, const stream_config_t* config);

/**
 * WHAT: Destroy stream and free resources
 * WHY: Clean shutdown of threads and memory
 * HOW: Stops threads, drains queue, frees memory
 *
 * COMPLEXITY: O(n) where n = pending inputs in queue
 * THREAD-SAFE: Yes (but stream should not be used after this call)
 *
 * @param stream Stream to destroy (NULL is safe)
 *
 * BEHAVIOR:
 * - Stops background thread if running
 * - Processes or drops pending inputs (configurable)
 * - Frees all allocated memory
 * - NULL stream is safe (no-op)
 */
void brain_destroy_stream(brain_stream_t stream);

//=============================================================================
// Stream Input Functions (Producer Side)
//=============================================================================

/**
 * WHAT: Feed input to stream (non-blocking)
 * WHY: Continuous sensory input for active consciousness
 * HOW: Enqueues input to ring buffer, triggers processing
 *
 * COMPLEXITY: O(1) - just enqueue operation
 * THREAD-SAFE: Yes (lock-free ring buffer)
 * NON-BLOCKING: Yes (unless STREAM_MODE_SYNCHRONOUS)
 *
 * @param stream Stream handle
 * @param features Input feature vector
 * @param num_features Size of feature vector (must match brain)
 * @param timestamp Timestamp of input (for temporal learning)
 * @return true if input accepted, false if buffer full or error
 *
 * BEHAVIOR:
 * - SYNCHRONOUS mode: Processes immediately (blocks)
 * - BACKGROUND mode: Enqueues and returns immediately
 * - BATCHED mode: Enqueues, processes when batch full
 *
 * ERROR CONDITIONS:
 * - NULL stream: Returns false
 * - NULL features: Returns false
 * - num_features mismatch: Returns false
 * - Buffer full: Returns false (or drops old input if drop_on_full)
 */
bool brain_stream_feed(brain_stream_t stream, const float* features, uint32_t num_features,
                       uint64_t timestamp);

/**
 * WHAT: Feed batch of inputs efficiently
 * WHY: Bulk input submission is more efficient than individual feeds
 * HOW: Enqueues multiple inputs with single lock acquisition
 *
 * COMPLEXITY: O(n) where n = num_samples
 * THREAD-SAFE: Yes
 *
 * @param stream Stream handle
 * @param features Array of feature vectors
 * @param num_samples Number of samples in batch
 * @param num_features Features per sample
 * @param timestamps Timestamp for each sample
 * @return Number of samples successfully enqueued
 */
uint32_t brain_stream_feed_batch(brain_stream_t stream, const float** features,
                                 uint32_t num_samples, uint32_t num_features,
                                 const uint64_t* timestamps);

//=============================================================================
// Stream Output Functions (Consumer Side)
//=============================================================================

/**
 * WHAT: Get latest decision from stream (non-blocking)
 * WHY: Retrieve processed results without waiting
 * HOW: Reads from decision cache (updated by processing thread)
 *
 * COMPLEXITY: O(1) - just cache read
 * THREAD-SAFE: Yes (mutex-protected cache)
 * NON-BLOCKING: Yes
 *
 * @param stream Stream handle
 * @return Latest decision or NULL if no decision ready
 *
 * OWNERSHIP: Caller must free with brain_free_decision()
 *
 * BEHAVIOR:
 * - Returns most recent decision
 * - NULL if no inputs processed yet
 * - Decision is a COPY (safe to use after stream destruction)
 */
brain_decision_t* brain_stream_get_decision(brain_stream_t stream);

/**
 * WHAT: Get current salience score (attention value)
 * WHY: Quick check of "interestingness" without full decision
 * HOW: Fast evaluation using partial network activation
 *
 * COMPLEXITY: O(1) - reads cached value
 * THREAD-SAFE: Yes
 *
 * @param stream Stream handle
 * @return Current salience score (0.0-1.0), or -1.0 on error
 */
float brain_stream_get_salience(brain_stream_t stream);

/**
 * WHAT: Get stream processing statistics
 * WHY: Monitor performance and backpressure
 * HOW: Reads internal counters and timings
 *
 * @param stream Stream handle
 * @param stats Output parameter for statistics
 * @return true on success, false on error
 */
typedef struct {
    uint64_t inputs_fed;          /**< Total inputs fed to stream */
    uint64_t inputs_processed;    /**< Total inputs processed */
    uint64_t inputs_dropped;      /**< Inputs dropped due to backpressure */
    uint64_t decisions_generated; /**< Total decisions generated */

    float avg_processing_time_ms; /**< Average processing time per input */
    float avg_queue_depth;        /**< Average queue utilization */
    float current_throughput;     /**< Inputs/second currently */

    uint32_t queue_size;     /**< Current queue size */
    uint32_t queue_capacity; /**< Maximum queue capacity */
} stream_stats_t;

bool brain_stream_get_stats(brain_stream_t stream, stream_stats_t* stats);

/**
 * WHAT: Reset stream statistics
 * WHY: Allow clearing counters for fresh measurements
 * HOW: Resets all atomic counters to zero
 *
 * @param stream Stream handle
 * @return true on success, false on error
 */
bool brain_stream_reset_stats(brain_stream_t stream);

//=============================================================================
// Stream Control Functions
//=============================================================================

/**
 * WHAT: Pause stream processing
 * WHY: Temporarily stop processing without destroying stream
 * HOW: Sets pause flag, waits for current processing to complete
 *
 * @param stream Stream handle
 * @return true on success
 */
bool brain_stream_pause(brain_stream_t stream);

/**
 * WHAT: Resume stream processing
 * WHY: Restart after pause
 * HOW: Clears pause flag, restarts thread if needed
 *
 * @param stream Stream handle
 * @return true on success
 */
bool brain_stream_resume(brain_stream_t stream);

/**
 * WHAT: Flush stream - process all pending inputs
 * WHY: Ensure all inputs processed before proceeding
 * HOW: Blocks until queue is empty
 *
 * BLOCKING: Yes - waits for queue to drain
 *
 * @param stream Stream handle
 * @param timeout_ms Maximum time to wait (0 = infinite)
 * @return true if flushed successfully, false on timeout
 */
bool brain_stream_flush(brain_stream_t stream, uint32_t timeout_ms);

/**
 * WHAT: Clear stream - drop all pending inputs
 * WHY: Reset stream state
 * HOW: Empties queue without processing
 *
 * @param stream Stream handle
 * @return true on success
 */
bool brain_stream_clear(brain_stream_t stream);

//=============================================================================
// Error Handling
//=============================================================================

/**
 * WHAT: Get last stream error message
 * WHY: Thread-safe error reporting
 * HOW: Thread-local storage for error strings
 *
 * @return Error message string (valid until next stream call)
 */
const char* brain_stream_get_last_error(void);

//=============================================================================
// Memory and Security Integration API (Phase IO-1, IO-2)
//=============================================================================

/**
 * @brief Extended stream statistics with memory and security info
 */
typedef struct {
    // Include base statistics
    stream_stats_t base;

    // Memory pool statistics
    bool using_unified_memory;      /**< Is unified memory enabled? */
    size_t cow_memory_saved;        /**< Bytes saved via CoW sharing */
    size_t pool_allocations;        /**< Allocations from pool */
    size_t malloc_allocations;      /**< Fallback malloc allocations */
    size_t ring_buffer_memory;      /**< Ring buffer memory usage */

    // Security statistics
    bool security_registered;       /**< Is security module registered? */
    uint32_t security_module_id;    /**< Security module ID */
    uint64_t security_interactions; /**< Total security interactions */
    uint64_t security_anomalies;    /**< Security anomalies detected */
    double trust_score;             /**< Current trust score (0.0-1.0) */
} stream_extended_stats_t;

/**
 * @brief Get extended stream statistics including memory and security
 *
 * @param stream Stream handle
 * @param stats Output statistics structure
 * @return true on success, false on error
 */
bool brain_stream_get_extended_stats(brain_stream_t stream, stream_extended_stats_t* stats);

/**
 * @brief Initialize Stream module with security registration
 *
 * Call this once at application startup to register the Stream module
 * with the security system. This enables trust tracking for all streams.
 *
 * @param security_ctx Security integration context (NULL to skip)
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t stream_init(nimcp_sec_integration_t* security_ctx);

/**
 * @brief Shutdown Stream module
 *
 * Unregisters from security module and cleans up global state.
 */
void stream_shutdown(void);

/**
 * @brief Get the Stream module's security ID
 *
 * @return Security module ID (0 if not registered)
 */
uint32_t stream_get_security_module_id(void);

#ifdef __cplusplus
}
#endif

#endif  // NIMCP_STREAM_H
