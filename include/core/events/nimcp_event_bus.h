/**
 * @file nimcp_event_bus.h
 * @brief Comprehensive Event Bus for All Brain Activities
 * @version 2.0.0
 * @date 2025-11-20
 *
 * WHAT: Extended event-driven architecture covering all brain computational activities
 * WHY:  Enable comprehensive monitoring, coordination, and debugging of brain operations
 * HOW:  Publish-subscribe pattern with type-safe events for all brain subsystems
 *
 * ARCHITECTURE:
 * - **Event Types**: Strongly-typed events for training, inference, neuron, and cognitive activities
 * - **Subscribers**: Callback-based subscribers with context data
 * - **Event Queue**: Thread-safe queue for async event processing
 * - **Delivery**: Immediate or background thread delivery options
 * - **Decoupling**: Publishers (brain modules) don't know about subscribers
 *
 * BRAIN ACTIVITY COVERAGE:
 * - Training Events: Epoch, batch, weight updates, learning rate changes
 * - Inference Events: Prediction lifecycle, forward passes, decisions
 * - Neuron-Level Events: Spikes, synapse updates, plasticity changes
 * - Cognitive Events: Memory operations, attention, consolidation
 * - Fault Tolerance Events: Recovery, diagnostics, health monitoring
 *
 * EVENT FLOW EXAMPLE:
 * 1. Brain starts training → publishes EVENT_TRAINING_STARTED
 * 2. Optimizer processes batch → publishes EVENT_TRAINING_BATCH_COMPLETE
 * 3. Weight updater applies changes → publishes EVENT_WEIGHT_UPDATE
 * 4. Health monitor subscribes to all → updates performance metrics
 * 5. Memory manager subscribes to cognitive events → optimizes storage
 *
 * INTEGRATION POINTS:
 * - Brain Core: Publishes training and inference events
 * - Optimizer: Publishes weight updates and learning rate changes
 * - Memory Manager: Publishes memory events
 * - Neuron Network: Publishes spike and synapse events
 * - Recovery System: Subscribes to critical events for fault handling
 * - Health Monitor: Subscribes to all events for system metrics
 *
 * THREAD SAFETY:
 * - All operations are thread-safe using mutex protection
 * - Event delivery can be immediate (same thread) or async (background thread)
 * - Callback errors are isolated and logged
 *
 * @author NIMCP Team
 */

#ifndef NIMCP_EVENT_BUS_H
#define NIMCP_EVENT_BUS_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

#define EVENT_BUS_MAX_SUBSCRIBERS 512    /**< Max subscribers per event type */
#define EVENT_BUS_MAX_EVENT_TYPES 128    /**< Max event types */
#define EVENT_BUS_QUEUE_SIZE 2048        /**< Event queue size */
#define EVENT_BUS_MAX_DATA_SIZE 1024     /**< Max event data size */
#define EVENT_BUS_THREAD_NAME_SIZE 64    /**< Thread name buffer size */

//=============================================================================
// Event Types
//=============================================================================

/**
 * @brief Comprehensive event types for all brain activities
 *
 * WHAT: Enumeration of all event types in the brain and fault tolerance systems
 * WHY:  Type-safe event classification for targeted subscriptions and monitoring
 * HOW:  Each module publishes specific event types, subscribers filter by type
 *
 * EVENT NUMBERING SCHEME:
 * - 0x0000-0x0FFF: Reserved
 * - 0x1000-0x1FFF: Diagnostic Events
 * - 0x2000-0x2FFF: Recovery Events
 * - 0x3000-0x3FFF: Checkpoint Events
 * - 0x4000-0x4FFF: Health Monitor Events
 * - 0x5000-0x5FFF: System Events
 * - 0x6000-0x6FFF: Custom User Events
 * - 0x7000-0x7FFF: Training Events
 * - 0x8000-0x8FFF: Inference Events
 * - 0x9000-0x9FFF: Neuron-Level Events
 * - 0xA000-0xAFFF: Cognitive Events
 * - 0xB000-0xBFFF: Network Topology Events
 * - 0xC000-0xCFFF: Logic & Reasoning Events
 */
typedef enum {
    //=====================================================================
    // Fault Tolerance Events (0x1000-0x5FFF)
    //=====================================================================

    // Diagnostic Events (0x1000-0x1FFF)
    EVENT_ERROR_DETECTED = 0x1000,           /**< Error detected by diagnostics */
    EVENT_DIAGNOSTICS_STARTED = 0x1001,      /**< Diagnostic analysis started */
    EVENT_DIAGNOSTICS_COMPLETE = 0x1002,     /**< Diagnostic analysis complete */
    EVENT_MEMORY_CORRUPTION_DETECTED = 0x1003, /**< Memory corruption found */
    EVENT_NUMERICAL_INSTABILITY_DETECTED = 0x1004, /**< NaN/Inf detected */
    EVENT_CRASH_PATTERN_DETECTED = 0x1005,   /**< Recurring crash pattern */

    // Recovery Events (0x2000-0x2FFF)
    EVENT_RECOVERY_STARTED = 0x2000,         /**< Recovery action started */
    EVENT_RECOVERY_COMPLETE = 0x2001,        /**< Recovery completed successfully */
    EVENT_RECOVERY_FAILED = 0x2002,          /**< Recovery failed */
    EVENT_ROLLBACK_STARTED = 0x2003,         /**< State rollback started */
    EVENT_ROLLBACK_COMPLETE = 0x2004,        /**< State rollback completed */
    EVENT_PARAMETER_ADJUSTED = 0x2005,       /**< System parameter adjusted */
    EVENT_FALLBACK_TRIGGERED = 0x2006,       /**< Fallback mode activated */

    // Checkpoint Events (0x3000-0x3FFF)
    EVENT_CHECKPOINT_CREATED = 0x3000,       /**< Checkpoint saved */
    EVENT_CHECKPOINT_LOADED = 0x3001,        /**< Checkpoint restored */
    EVENT_CHECKPOINT_DELETED = 0x3002,       /**< Checkpoint removed */
    EVENT_CHECKPOINT_CORRUPTED = 0x3003,     /**< Checkpoint integrity failed */

    // Health Monitor Events (0x4000-0x4FFF)
    EVENT_HEALTH_DEGRADED = 0x4000,          /**< Health score below threshold */
    EVENT_HEALTH_CRITICAL = 0x4001,          /**< Health critical, action needed */
    EVENT_HEALTH_RECOVERED = 0x4002,         /**< Health returned to normal */
    EVENT_ANOMALY_DETECTED = 0x4003,         /**< Statistical anomaly detected */
    EVENT_FAILURE_PREDICTED = 0x4004,        /**< Predictive failure warning */
    EVENT_RESOURCE_EXHAUSTION = 0x4005,      /**< Resource limit approaching */

    // System Events (0x5000-0x5FFF)
    EVENT_SYSTEM_STARTED = 0x5000,           /**< System initialization complete */
    EVENT_SYSTEM_SHUTDOWN = 0x5001,          /**< System shutdown initiated */
    EVENT_EMERGENCY_SHUTDOWN = 0x5002,       /**< Emergency shutdown triggered */
    EVENT_CONFIGURATION_CHANGED = 0x5003,    /**< System configuration updated */

    // Custom Events (0x6000-0x6FFF)
    EVENT_CUSTOM_USER = 0x6000,              /**< User-defined events start here */

    //=====================================================================
    // Training Events (0x7000-0x7FFF)
    //=====================================================================

    EVENT_TRAINING_STARTED = 0x7000,         /**< Training process initiated */
    EVENT_TRAINING_EPOCH_COMPLETE = 0x7001,  /**< Epoch finished successfully */
    EVENT_TRAINING_BATCH_COMPLETE = 0x7002,  /**< Batch processed completely */
    EVENT_WEIGHT_UPDATE = 0x7003,            /**< Weights updated by optimizer */
    EVENT_LEARNING_RATE_CHANGE = 0x7004,     /**< Learning rate adjusted */
    EVENT_TRAINING_CONVERGED = 0x7005,       /**< Training convergence detected */
    EVENT_TRAINING_DIVERGED = 0x7006,        /**< Training divergence detected */
    EVENT_TRAINING_PAUSED = 0x7007,          /**< Training paused by user/system */
    EVENT_TRAINING_RESUMED = 0x7008,         /**< Training resumed after pause */
    EVENT_TRAINING_COMPLETED = 0x7009,       /**< Full training finished */
    EVENT_LOSS_THRESHOLD_CROSSED = 0x700A,   /**< Loss reached target threshold */
    EVENT_GRADIENT_EXPLOSION_DETECTED = 0x700B, /**< Gradient explosion warning */
    EVENT_GRADIENT_VANISHING_DETECTED = 0x700C, /**< Gradient vanishing warning */

    //=====================================================================
    // Inference Events (0x8000-0x8FFF)
    //=====================================================================

    EVENT_INFERENCE_STARTED = 0x8000,        /**< Inference process initiated */
    EVENT_INFERENCE_COMPLETE = 0x8001,       /**< Inference finished successfully */
    EVENT_FORWARD_PASS_COMPLETE = 0x8002,    /**< Forward pass through network done */
    EVENT_DECISION_MADE = 0x8003,            /**< Decision finalized and output ready */
    EVENT_BATCH_INFERENCE_COMPLETE = 0x8004, /**< Batch inference finished */
    EVENT_PREDICTION_THRESHOLD_EXCEEDED = 0x8005, /**< Confidence above threshold */
    EVENT_ATTENTION_COMPUTED = 0x8006,       /**< Attention weights calculated */
    EVENT_FEATURE_EXTRACTED = 0x8007,        /**< Feature extraction completed */
    EVENT_INFERENCE_LATENCY_HIGH = 0x8008,   /**< Inference exceeded latency SLA */

    //=====================================================================
    // Neuron-Level Events (0x9000-0x9FFF)
    //=====================================================================

    EVENT_NEURON_SPIKE = 0x9000,             /**< Neuron activation/spike event */
    EVENT_SYNAPSE_WEIGHT_CHANGE = 0x9001,    /**< Synaptic weight updated */
    EVENT_PLASTICITY_UPDATE = 0x9002,        /**< Synaptic plasticity applied */
    EVENT_SYNAPSE_CREATED = 0x9003,          /**< New synapse formed */
    EVENT_SYNAPSE_PRUNED = 0x9004,           /**< Synapse removed/pruned */
    EVENT_NEURON_CREATED = 0x9005,           /**< New neuron instantiated */
    EVENT_NEURON_PRUNED = 0x9006,            /**< Neuron removed from network */
    EVENT_ACTIVATION_SATURATION = 0x9007,    /**< Neuron activation saturated */
    EVENT_LONG_TERM_POTENTIATION = 0x9008,   /**< LTP mechanism triggered */
    EVENT_LONG_TERM_DEPRESSION = 0x9009,     /**< LTD mechanism triggered */

    //=====================================================================
    // Cognitive Events (0xA000-0xAFFF)
    //=====================================================================

    EVENT_WORKING_MEMORY_UPDATE = 0xA000,    /**< Working memory modified */
    EVENT_ATTENTION_SHIFT = 0xA001,          /**< Attention focus changed */
    EVENT_CONSOLIDATION_STARTED = 0xA002,    /**< Memory consolidation initiated */
    EVENT_EPISODIC_MEMORY_STORED = 0xA003,   /**< Episodic memory item saved */
    EVENT_SEMANTIC_MEMORY_STORED = 0xA004,   /**< Semantic memory item saved */
    EVENT_MEMORY_RECALLED = 0xA005,          /**< Memory retrieved/recalled */
    EVENT_MEMORY_CONSOLIDATED = 0xA006,      /**< Memory consolidation completed */
    EVENT_MEMORY_FORGOTTEN = 0xA007,         /**< Memory item forgotten/discarded */
    EVENT_GOAL_SET = 0xA008,                 /**< Goal established */
    EVENT_GOAL_ACHIEVED = 0xA009,            /**< Goal completed successfully */
    EVENT_GOAL_ABANDONED = 0xA00A,           /**< Goal abandoned */
    EVENT_PLAN_CREATED = 0xA00B,             /**< Plan generated */
    EVENT_PLAN_EXECUTED = 0xA00C,            /**< Plan execution started */
    EVENT_PLAN_COMPLETED = 0xA00D,           /**< Plan execution completed */
    EVENT_PLAN_FAILED = 0xA00E,              /**< Plan execution failed */
    EVENT_EXPECTATION_MISMATCH = 0xA00F,     /**< Prediction error detected */

    //=====================================================================
    // Network Topology Events (0xB000-0xBFFF)
    //=====================================================================

    EVENT_TOPOLOGY_CHANGED = 0xB000,         /**< Network structure modified */
    EVENT_LAYER_ADDED = 0xB001,              /**< New layer added */
    EVENT_LAYER_REMOVED = 0xB002,            /**< Layer removed */
    EVENT_CONNECTION_ESTABLISHED = 0xB003,   /**< New connection created */
    EVENT_CONNECTION_SEVERED = 0xB004,       /**< Connection removed */

    //=====================================================================
    // Logic & Reasoning Events (0xC000-0xCFFF)
    //=====================================================================

    EVENT_LOGIC_GATE_EVALUATED = 0xC000,     /**< Logic gate evaluated (AND/OR/NOT) */
    EVENT_LOGIC_INFERENCE_STARTED = 0xC001,  /**< Logic inference process initiated */
    EVENT_LOGIC_INFERENCE_COMPLETE = 0xC002, /**< Logic inference completed successfully */
    EVENT_FACT_ADDED = 0xC003,               /**< New fact added to knowledge base */
    EVENT_RULE_ADDED = 0xC004,               /**< New rule added to knowledge base */
    EVENT_UNIFICATION_SUCCEEDED = 0xC005,    /**< Term unification succeeded */
    EVENT_UNIFICATION_FAILED = 0xC006,       /**< Term unification failed */
    EVENT_FORWARD_CHAIN_STEP = 0xC007,       /**< Forward chaining step executed */
    EVENT_BACKWARD_CHAIN_STEP = 0xC008,      /**< Backward chaining step executed */
    EVENT_PROOF_FOUND = 0xC009,              /**< Proof successfully found */
    EVENT_PROOF_FAILED = 0xC00A,             /**< Proof attempt failed */
    EVENT_CONTRADICTION_DETECTED = 0xC00B,   /**< Logical contradiction detected */
    EVENT_NOVEL_FACT_DERIVED = 0xC00C,       /**< Novel fact derived from reasoning */

    //=====================================================================
    // Special Events
    //=====================================================================

    EVENT_ALL = 0xFFFF,                      /**< Subscribe to all events */
    EVENT_NONE = 0x0000                      /**< No event */
} brain_event_type_t;

/**
 * @brief Event priority for delivery ordering
 */
typedef enum {
    EVENT_PRIORITY_LOW = 0,      /**< Low priority, deliver when convenient */
    EVENT_PRIORITY_NORMAL = 1,   /**< Normal priority (default) */
    EVENT_PRIORITY_HIGH = 2,     /**< High priority, deliver soon */
    EVENT_PRIORITY_CRITICAL = 3  /**< Critical priority, deliver immediately */
} event_priority_t;

/**
 * @brief Event delivery mode
 */
typedef enum {
    EVENT_DELIVERY_IMMEDIATE = 0,  /**< Deliver synchronously in caller's thread */
    EVENT_DELIVERY_ASYNC = 1       /**< Deliver asynchronously in background thread */
} event_delivery_mode_t;

//=============================================================================
// Event Data Structure
//=============================================================================

/**
 * @brief Event data payload
 *
 * WHAT: Generic event data container
 * WHY:  Store arbitrary event-specific data
 * HOW:  Opaque byte array with size tracking
 */
typedef struct {
    uint8_t data[EVENT_BUS_MAX_DATA_SIZE]; /**< Event data bytes */
    size_t size;                           /**< Actual data size */
} event_data_t;

/**
 * @brief Complete event structure
 *
 * WHAT: Complete event with type, data, and metadata
 * WHY:  Self-contained event for queue and delivery
 * HOW:  Contains all information needed for processing
 */
typedef struct {
    brain_event_type_t type;           /**< Event type */
    event_priority_t priority;   /**< Event priority */
    event_data_t data;           /**< Event payload */
    uint64_t timestamp_us;       /**< Event timestamp (microseconds) */
    uint64_t sequence_number;    /**< Sequence number for ordering */
    const char* source_module;   /**< Module that published event */
    uint32_t flags;              /**< Event flags (reserved) */
} brain_event_t;

//=============================================================================
// Subscriber Callback
//=============================================================================

/**
 * @brief Event callback function type
 *
 * WHAT: Callback function signature for event subscribers
 * WHY:  Standardized interface for event notification
 * HOW:  Called when matching event is published
 *
 * @param event The event that was published
 * @param context User-provided context data
 */
typedef void (*brain_event_callback_t)(const brain_event_t* event, void* context);

/**
 * @brief Error callback for handling callback exceptions
 *
 * WHAT: Callback for handling errors during event delivery
 * WHY:  Prevent one failing callback from affecting others
 * HOW:  Called when a subscriber callback throws or crashes
 *
 * @param event The event being delivered
 * @param subscriber_context The subscriber's context
 * @param error_message Human-readable error description
 */
typedef void (*event_error_callback_t)(
    const brain_event_t* event,
    void* subscriber_context,
    const char* error_message
);

//=============================================================================
// Subscriber Handle
//=============================================================================

/**
 * @brief Opaque subscriber handle
 *
 * WHAT: Unique identifier for a subscription
 * WHY:  Enable unsubscribe operations
 * HOW:  Internal identifier returned by subscribe
 */
typedef uint64_t event_subscription_handle_t;

#define INVALID_SUBSCRIPTION_HANDLE 0

//=============================================================================
// Event Bus Handle
//=============================================================================

/**
 * @brief Opaque event bus handle
 */
typedef struct event_bus_internal* event_bus_t;

//=============================================================================
// Event Bus Lifecycle API
//=============================================================================

/**
 * @brief Create event bus
 *
 * WHAT: Creates and initializes event bus
 * WHY:  Enable decoupled event-driven architecture
 * HOW:  Allocates bus, initializes queues, starts background thread (if async)
 *
 * @param name Bus name for debugging (optional, can be NULL)
 * @param delivery_mode Immediate or async delivery
 * @return Event bus handle, NULL on failure
 */
event_bus_t event_bus_create(const char* name, event_delivery_mode_t delivery_mode);

/**
 * @brief Destroy event bus
 *
 * WHAT: Destroys event bus and frees resources
 * WHY:  Clean shutdown and prevent memory leaks
 * HOW:  Stops background thread, delivers pending events, frees memory
 *
 * @param bus Event bus handle
 */
void event_bus_destroy(event_bus_t bus);

/**
 * @brief Start event bus processing
 *
 * WHAT: Start background event processing thread
 * WHY:  Enable async event delivery
 * HOW:  Creates thread, processes events from queue
 *
 * NOTE: Only needed for async delivery mode
 *
 * @param bus Event bus handle
 * @return true on success, false on failure
 */
bool event_bus_start(event_bus_t bus);

/**
 * @brief Stop event bus processing
 *
 * WHAT: Stop background processing thread
 * WHY:  Graceful shutdown
 * HOW:  Signals thread to stop, waits for completion
 *
 * @param bus Event bus handle
 * @param drain_queue If true, process all pending events before stopping
 * @return true on success, false on failure
 */
bool event_bus_stop(event_bus_t bus, bool drain_queue);

/**
 * @brief Check if event bus is running
 *
 * @param bus Event bus handle
 * @return true if running, false otherwise
 */
bool event_bus_is_running(event_bus_t bus);

//=============================================================================
// Subscription API
//=============================================================================

/**
 * @brief Subscribe to event type
 *
 * WHAT: Register callback for specific event type
 * WHY:  Receive notifications when events are published
 * HOW:  Adds callback to subscriber list for event type
 *
 * EXAMPLE:
 * ```c
 * void on_training_complete(const brain_event_t* event, void* context) {
 *     // Handle training completion...
 * }
 *
 * event_subscription_handle_t handle =
 *     event_bus_subscribe(bus, EVENT_TRAINING_EPOCH_COMPLETE, on_training_complete, my_context);
 * ```
 *
 * @param bus Event bus handle
 * @param type Event type to subscribe to (or EVENT_ALL for all events)
 * @param callback Callback function
 * @param context User context data (passed to callback)
 * @return Subscription handle (use for unsubscribe), INVALID_SUBSCRIPTION_HANDLE on error
 */
event_subscription_handle_t event_bus_subscribe(
    event_bus_t bus,
    brain_event_type_t type,
    brain_event_callback_t callback,
    void* context
);

/**
 * @brief Subscribe with priority filter
 *
 * WHAT: Subscribe to events with minimum priority
 * WHY:  Filter out low-priority events
 * HOW:  Only deliver events with priority >= min_priority
 *
 * @param bus Event bus handle
 * @param type Event type
 * @param min_priority Minimum priority to receive
 * @param callback Callback function
 * @param context User context
 * @return Subscription handle
 */
event_subscription_handle_t event_bus_subscribe_priority(
    event_bus_t bus,
    brain_event_type_t type,
    event_priority_t min_priority,
    brain_event_callback_t callback,
    void* context
);

/**
 * @brief Unsubscribe from events
 *
 * WHAT: Remove subscription
 * WHY:  Stop receiving event notifications
 * HOW:  Removes callback from subscriber list
 *
 * @param bus Event bus handle
 * @param handle Subscription handle from subscribe
 * @return true on success, false if handle invalid
 */
bool event_bus_unsubscribe(event_bus_t bus, event_subscription_handle_t handle);

/**
 * @brief Unsubscribe all subscriptions for a context
 *
 * WHAT: Remove all subscriptions using a specific context
 * WHY:  Clean up all subscriptions when component shuts down
 * HOW:  Finds and removes all subscriptions with matching context
 *
 * @param bus Event bus handle
 * @param context Context pointer to match
 * @return Number of subscriptions removed
 */
uint32_t event_bus_unsubscribe_all(event_bus_t bus, void* context);

//=============================================================================
// Publishing API
//=============================================================================

/**
 * @brief Publish event
 *
 * WHAT: Publish event to all subscribers
 * WHY:  Notify interested parties of system events
 * HOW:  Queues event (async) or delivers immediately (sync)
 *
 * EXAMPLE:
 * ```c
 * brain_event_t event = {
 *     .type = EVENT_TRAINING_EPOCH_COMPLETE,
 *     .priority = EVENT_PRIORITY_NORMAL,
 *     .source_module = "brain_core"
 * };
 * event_bus_publish(bus, &event);
 * ```
 *
 * @param bus Event bus handle
 * @param event Event to publish
 * @return true on success, false on failure
 */
bool event_bus_publish(event_bus_t bus, const brain_event_t* event);

/**
 * @brief Publish simple event (no data payload)
 *
 * WHAT: Publish event with only type and priority
 * WHY:  Convenience for simple notifications
 * HOW:  Creates event with no data, publishes
 *
 * @param bus Event bus handle
 * @param type Event type
 * @param priority Event priority
 * @param source Source module name (optional)
 * @return true on success, false on failure
 */
bool event_bus_publish_simple(
    event_bus_t bus,
    brain_event_type_t type,
    event_priority_t priority,
    const char* source
);

/**
 * @brief Publish event with data payload
 *
 * WHAT: Publish event with typed data payload
 * WHY:  Convenience for events with data
 * HOW:  Copies data into event, publishes
 *
 * @param bus Event bus handle
 * @param type Event type
 * @param priority Event priority
 * @param source Source module name
 * @param data Data payload (will be copied)
 * @param data_size Size of data in bytes
 * @return true on success, false on failure
 */
bool event_bus_publish_data(
    event_bus_t bus,
    brain_event_type_t type,
    event_priority_t priority,
    const char* source,
    const void* data,
    size_t data_size
);

/**
 * @brief Flush pending events
 *
 * WHAT: Process all pending events immediately
 * WHY:  Ensure events are delivered before shutdown or critical operation
 * HOW:  Processes entire event queue
 *
 * NOTE: Only relevant for async delivery mode
 *
 * @param bus Event bus handle
 * @return Number of events processed
 */
uint32_t event_bus_flush(event_bus_t bus);

//=============================================================================
// Statistics and Monitoring API
//=============================================================================

/**
 * @brief Event bus statistics
 */
typedef struct {
    uint64_t total_events_published;     /**< Total events published */
    uint64_t total_events_delivered;     /**< Total events delivered to subscribers */
    uint64_t total_events_dropped;       /**< Events dropped (queue full) */
    uint64_t total_callback_errors;      /**< Callback errors encountered */
    uint32_t active_subscriptions;       /**< Current subscription count */
    uint32_t pending_events;             /**< Events in queue */
    uint64_t avg_delivery_latency_us;    /**< Average delivery latency */
    uint64_t max_delivery_latency_us;    /**< Maximum delivery latency */
} event_bus_stats_t;

/**
 * @brief Get event bus statistics
 *
 * @param bus Event bus handle
 * @param stats Output parameter for statistics
 * @return true on success, false on failure
 */
bool event_bus_get_stats(event_bus_t bus, event_bus_stats_t* stats);

/**
 * @brief Reset event bus statistics
 *
 * @param bus Event bus handle
 */
void event_bus_reset_stats(event_bus_t bus);

/**
 * @brief Get subscriber count for event type
 *
 * @param bus Event bus handle
 * @param type Event type
 * @return Number of subscribers for this type
 */
uint32_t event_bus_get_subscriber_count(event_bus_t bus, brain_event_type_t type);

/**
 * @brief Get pending event count
 *
 * @param bus Event bus handle
 * @return Number of events waiting in queue
 */
uint32_t event_bus_get_pending_count(event_bus_t bus);

//=============================================================================
// Error Handling API
//=============================================================================

/**
 * @brief Set error callback for subscriber failures
 *
 * WHAT: Register callback for handling subscriber errors
 * WHY:  Monitor and handle callback failures
 * HOW:  Called when a subscriber callback throws or crashes
 *
 * @param bus Event bus handle
 * @param callback Error callback function
 */
void event_bus_set_error_callback(event_bus_t bus, event_error_callback_t callback);

/**
 * @brief Get last error message
 *
 * @param bus Event bus handle
 * @return Last error message (static buffer, do not free)
 */
const char* event_bus_get_last_error(event_bus_t bus);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get event type name
 *
 * @param type Event type
 * @return String name (static, do not free)
 */
const char* event_type_to_string(brain_event_type_t type);

/**
 * @brief Get event priority name
 *
 * @param priority Event priority
 * @return String name (static, do not free)
 */
const char* event_priority_to_string(event_priority_t priority);

/**
 * @brief Create event with defaults
 *
 * WHAT: Create event structure with default values
 * WHY:  Convenience for event creation
 * HOW:  Initializes with type, priority, timestamp
 *
 * @param type Event type
 * @param priority Event priority
 * @param source Source module name
 * @return Initialized event structure
 */
brain_event_t event_create(brain_event_type_t type, event_priority_t priority, const char* source);

/**
 * @brief Copy data into event
 *
 * WHAT: Copy data payload into event structure
 * WHY:  Safe data copying with size checking
 * HOW:  Validates size, copies data
 *
 * @param event Event to populate
 * @param data Data to copy
 * @param size Data size
 * @return true on success, false if data too large
 */
bool event_set_data(brain_event_t* event, const void* data, size_t size);

/**
 * @brief Get current timestamp in microseconds
 *
 * @return Timestamp in microseconds since epoch
 */
uint64_t event_get_timestamp_us(void);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_EVENT_BUS_H
