/**
 * @file nimcp_training_integration_hub.h
 * @brief Central hub for training module integration and event routing
 * @version 1.0.0
 * @date 2025
 *
 * WHAT: Central hub that connects all training modules, enabling event-driven
 *       communication and cross-module queries.
 *
 * WHY: Training modules need to communicate without tight coupling. The hub
 *      provides a publish-subscribe event system and request-response queries.
 *      This enables:
 *      - Curriculum learning to inform meta-learning of difficulty
 *      - Meta-learning to request difficulty adjustments from curriculum
 *      - Quantization-aware training to coordinate with distillation
 *      - Continual learning to share replay buffer with multi-task
 *      - Distributed training to coordinate gradient synchronization
 *
 * HOW: Modules register with the hub, subscribe to events, and publish events.
 *      The hub routes events to subscribers and handles cross-module queries.
 *
 * DESIGN PATTERNS:
 * - Mediator: Hub mediates all inter-module communication
 * - Observer: Publish-subscribe event system
 * - Registry: Module registration and lookup
 * - Command: Events encapsulate operations
 *
 * KEY INTEGRATION SCENARIOS:
 * 1. Curriculum <-> Meta-Learning: Difficulty informs task sampling
 * 2. Quantization <-> Distillation: Coordinate compression strategies
 * 3. Continual <-> Multi-Task: Share replay buffers, prevent forgetting
 * 4. Distributed <-> All: Coordinate gradient sync across all modules
 * 5. Optimization <-> Architecture: Hyperparameters inform NAS
 *
 * THREAD SAFETY: All functions are thread-safe. The hub uses internal
 * synchronization for concurrent access from multiple modules.
 *
 * PERFORMANCE:
 * - Event publish: O(s) where s = number of subscribers
 * - Module lookup: O(1) with hash-based registry
 * - Async publish: O(1) - queued for later processing
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_TRAINING_INTEGRATION_HUB_H
#define NIMCP_TRAINING_INTEGRATION_HUB_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "training/integration/nimcp_training_event_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * OPAQUE TYPE DECLARATIONS
 * ======================================================================== */

/**
 * WHAT: Opaque handle to training integration hub
 * WHY: Encapsulation - hide internal implementation details
 * HOW: Pimpl idiom - pointer to internal structure
 */
typedef struct training_integration_hub_struct* training_integration_hub_t;

/* ========================================================================
 * CONFIGURATION STRUCTURES
 * ======================================================================== */

/**
 * WHAT: Configuration for training integration hub
 * WHY: Customize hub behavior at creation time
 * HOW: Struct with all hub configuration parameters
 *
 * FIELDS:
 * - max_modules: Maximum number of modules that can register
 * - max_subscriptions: Maximum total subscriptions across all modules
 * - enable_async: Enable asynchronous event delivery
 * - event_queue_size: Size of async event queue (if async enabled)
 * - enable_metrics: Enable performance metrics collection
 */
typedef struct {
    uint32_t max_modules;           /* Maximum registered modules */
    uint32_t max_subscriptions;     /* Maximum total subscriptions */
    bool enable_async;              /* Enable async event delivery */
    uint32_t event_queue_size;      /* Async event queue size */
    bool enable_metrics;            /* Enable performance metrics */
} training_hub_config_t;

/**
 * WHAT: Information about a registered training module
 * WHY: Track module metadata for routing and queries
 * HOW: Struct with identification and state
 *
 * FIELDS:
 * - module_id: Unique module identifier
 * - category: Training category of module
 * - name: Human-readable module name
 * - context: Module-provided context pointer
 * - is_active: Whether module is currently active
 */
typedef struct {
    uint32_t module_id;             /* Unique module identifier */
    training_category_t category;   /* Module's training category */
    char name[64];                  /* Human-readable name */
    void* context;                  /* Module context pointer */
    bool is_active;                 /* Module active status */
} training_module_info_t;

/**
 * WHAT: Statistics about hub operation
 * WHY: Monitor hub performance and health
 * HOW: Counters and metrics collected during operation
 */
typedef struct {
    uint32_t registered_modules;    /* Currently registered modules */
    uint32_t active_subscriptions;  /* Currently active subscriptions */
    uint64_t events_published;      /* Total events published */
    uint64_t events_delivered;      /* Total events delivered to subscribers */
    uint64_t events_dropped;        /* Events dropped (queue full, etc.) */
    uint64_t queries_processed;     /* Total queries processed */
    uint64_t queries_failed;        /* Queries that failed */
    uint32_t async_queue_depth;     /* Current async queue depth */
    uint32_t async_queue_max;       /* Maximum async queue depth seen */
    uint64_t avg_delivery_time_us;  /* Average event delivery time (microseconds) */

    /* Training-specific metrics */
    uint64_t difficulty_updates;    /* Curriculum difficulty update events */
    uint64_t lr_adjustments;        /* Learning rate adjustment events */
    uint64_t task_switches;         /* Meta-learning/multi-task switches */
    uint64_t checkpoints_saved;     /* Checkpoint save events */
} training_hub_stats_t;

/* ========================================================================
 * PREDEFINED MODULE IDS
 * ======================================================================== */

/**
 * WHAT: Predefined module IDs for standard training modules
 * WHY: Enable consistent identification across the system
 * HOW: Fixed IDs in non-overlapping ranges
 */
#define TRAINING_MODULE_CURRICULUM_LEARNING     0x2001
#define TRAINING_MODULE_META_LEARNING           0x2002
#define TRAINING_MODULE_HYPERPARAMETER_OPT      0x2003
#define TRAINING_MODULE_AUTO_ARCHITECTURE       0x2004
#define TRAINING_MODULE_QUANTIZATION_AWARE      0x2005
#define TRAINING_MODULE_KNOWLEDGE_DISTILLATION  0x2006
#define TRAINING_MODULE_MIXED_PRECISION         0x2007
#define TRAINING_MODULE_DISTRIBUTED_TRAINING    0x2008
#define TRAINING_MODULE_GRADIENT_SCALING        0x2009
#define TRAINING_MODULE_ADVERSARIAL_TRAINING    0x200A
#define TRAINING_MODULE_CONTINUAL_LEARNING      0x200B
#define TRAINING_MODULE_MULTI_TASK              0x200C
#define TRAINING_MODULE_DATA_PIPELINE           0x200D
#define TRAINING_MODULE_CHECKPOINT              0x200E
#define TRAINING_MODULE_LR_SCHEDULER            0x200F
#define TRAINING_MODULE_OPTIMIZER               0x2010
#define TRAINING_MODULE_GRADIENT_MANAGER        0x2011
#define TRAINING_MODULE_SNN_BACKPROP            0x2012

/* ========================================================================
 * DEFAULT CONFIGURATION
 * ======================================================================== */

/**
 * WHAT: Get default hub configuration
 * WHY: Provide sensible defaults for common use cases
 * HOW: Return pre-configured struct
 *
 * DEFAULT VALUES:
 * - max_modules: 32
 * - max_subscriptions: 128
 * - enable_async: true
 * - event_queue_size: 512
 * - enable_metrics: true
 *
 * @return Default configuration struct
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
training_hub_config_t training_hub_default_config(void);

/* ========================================================================
 * LIFECYCLE MANAGEMENT
 * ======================================================================== */

/**
 * WHAT: Create a new training integration hub
 * WHY: Initialize hub for module registration and event routing
 * HOW: Allocate resources, initialize internal structures
 *
 * @param config Hub configuration (NULL for defaults)
 * @return Hub handle, or NULL on error
 *
 * ERRORS:
 * - Returns NULL if memory allocation fails
 * - Returns NULL if async thread creation fails (when async enabled)
 *
 * MEMORY: Caller must call training_hub_destroy() when done
 *
 * COMPLEXITY: O(max_modules + max_subscriptions)
 * THREAD-SAFE: Yes
 */
training_integration_hub_t training_hub_create(const training_hub_config_t* config);

/**
 * WHAT: Destroy training integration hub
 * WHY: Release all resources and cleanup
 * HOW: Stop async thread, unregister modules, free memory
 *
 * @param hub Hub to destroy
 *
 * BLOCKING: May block waiting for async queue to drain
 *
 * SAFETY: Safe to call with NULL
 * MEMORY: Frees all hub-owned allocations
 *
 * COMPLEXITY: O(registered_modules + active_subscriptions)
 * THREAD-SAFE: Yes
 */
void training_hub_destroy(training_integration_hub_t hub);

/* ========================================================================
 * MODULE REGISTRATION
 * ======================================================================== */

/**
 * WHAT: Register a training module with the hub
 * WHY: Enable module to publish events and receive subscriptions
 * HOW: Add module to registry with metadata
 *
 * @param hub Integration hub
 * @param module_id Unique module identifier
 * @param category Training category of module
 * @param name Human-readable module name (max 63 chars)
 * @param context Module-provided context (can be NULL)
 * @return 0 on success, -1 on error
 *
 * ERRORS:
 * - Returns -1 if hub is NULL
 * - Returns -1 if module_id already registered
 * - Returns -1 if max_modules limit reached
 * - Returns -1 if category is invalid
 *
 * COMPLEXITY: O(1) average (hash-based)
 * THREAD-SAFE: Yes
 */
int training_hub_register_module(training_integration_hub_t hub,
                                  uint32_t module_id,
                                  training_category_t category,
                                  const char* name,
                                  void* context);

/**
 * WHAT: Unregister a training module from the hub
 * WHY: Remove module when it's shutting down or no longer needed
 * HOW: Remove from registry, cancel subscriptions, cleanup
 *
 * @param hub Integration hub
 * @param module_id Module identifier to unregister
 * @return 0 on success, -1 on error
 *
 * ERRORS:
 * - Returns -1 if hub is NULL
 * - Returns -1 if module_id not registered
 *
 * SIDE EFFECTS: All subscriptions by this module are cancelled
 *
 * COMPLEXITY: O(subscriptions) to cleanup subscriptions
 * THREAD-SAFE: Yes
 */
int training_hub_unregister_module(training_integration_hub_t hub,
                                    uint32_t module_id);

/* ========================================================================
 * EVENT SUBSCRIPTION
 * ======================================================================== */

/**
 * WHAT: Subscribe to training events of a specific type
 * WHY: Enable modules to receive notifications for events of interest
 * HOW: Register callback for event type
 *
 * @param hub Integration hub
 * @param subscriber_id Module ID of subscriber
 * @param event_type Type of events to subscribe to
 * @param callback Function to call when event occurs
 * @param user_data User-provided context passed to callback
 * @return 0 on success, -1 on error
 *
 * ERRORS:
 * - Returns -1 if hub is NULL
 * - Returns -1 if subscriber_id not registered
 * - Returns -1 if callback is NULL
 * - Returns -1 if max_subscriptions limit reached
 * - Returns -1 if event_type is invalid
 *
 * DUPLICATE HANDLING: If already subscribed to this event type,
 * the existing subscription is updated with new callback/user_data
 *
 * COMPLEXITY: O(1) average
 * THREAD-SAFE: Yes
 */
int training_hub_subscribe(training_integration_hub_t hub,
                            uint32_t subscriber_id,
                            training_event_type_t event_type,
                            training_event_callback_t callback,
                            void* user_data);

/**
 * WHAT: Unsubscribe from training events of a specific type
 * WHY: Stop receiving notifications for events no longer needed
 * HOW: Remove callback registration
 *
 * @param hub Integration hub
 * @param subscriber_id Module ID of subscriber
 * @param event_type Type of events to unsubscribe from
 * @return 0 on success, -1 on error
 *
 * ERRORS:
 * - Returns -1 if hub is NULL
 * - Returns -1 if no matching subscription found
 *
 * COMPLEXITY: O(1) average
 * THREAD-SAFE: Yes
 */
int training_hub_unsubscribe(training_integration_hub_t hub,
                              uint32_t subscriber_id,
                              training_event_type_t event_type);

/* ========================================================================
 * EVENT PUBLISHING
 * ======================================================================== */

/**
 * WHAT: Publish training event synchronously
 * WHY: Notify all subscribers of an event immediately
 * HOW: Iterate through subscribers, invoke callbacks
 *
 * @param hub Integration hub
 * @param publisher_id Module ID of publisher
 * @param event_type Type of event
 * @param data Event data (copied for delivery)
 * @return 0 on success, -1 on error
 *
 * ERRORS:
 * - Returns -1 if hub is NULL
 * - Returns -1 if publisher_id not registered
 * - Returns -1 if event_type is invalid
 *
 * BLOCKING: Yes - waits for all callbacks to complete
 *
 * CALLBACK ERRORS: If a callback returns -1, delivery continues
 * to remaining subscribers. Error is logged but not propagated.
 *
 * COMPLEXITY: O(s) where s = number of subscribers
 * THREAD-SAFE: Yes
 */
int training_hub_publish(training_integration_hub_t hub,
                          uint32_t publisher_id,
                          training_event_type_t event_type,
                          const training_event_data_t* data);

/**
 * WHAT: Publish training event asynchronously
 * WHY: Non-blocking event delivery for time-critical publishers
 * HOW: Queue event for background delivery
 *
 * @param hub Integration hub
 * @param publisher_id Module ID of publisher
 * @param event_type Type of event
 * @param data Event data (deep copied to queue)
 * @return 0 on success, -1 on error
 *
 * ERRORS:
 * - Returns -1 if hub is NULL
 * - Returns -1 if publisher_id not registered
 * - Returns -1 if async not enabled
 * - Returns -1 if queue is full
 * - Returns -1 if event_type is invalid
 *
 * NON-BLOCKING: Returns immediately after queueing
 *
 * MEMORY: Event data is deep-copied, caller can free immediately
 *
 * COMPLEXITY: O(1) to queue, delivery is O(s) in background
 * THREAD-SAFE: Yes
 */
int training_hub_publish_async(training_integration_hub_t hub,
                                uint32_t publisher_id,
                                training_event_type_t event_type,
                                const training_event_data_t* data);

/* ========================================================================
 * INTER-MODULE QUERIES
 * ======================================================================== */

/**
 * WHAT: Query another module through the hub
 * WHY: Request-response communication between modules
 * HOW: Route query to target module, wait for response
 *
 * @param hub Integration hub
 * @param requester_id Module ID of requester
 * @param target_id Module ID of target
 * @param query Query to send
 * @param result Output: query result
 * @return 0 on success, -1 on error
 *
 * ERRORS:
 * - Returns -1 if hub is NULL
 * - Returns -1 if requester_id not registered
 * - Returns -1 if target_id not registered
 * - Returns -1 if target module is inactive
 * - Returns -1 if query handler not registered
 *
 * BLOCKING: Yes - waits for target module response
 *
 * COMPLEXITY: O(1) for routing, target-dependent for response
 * THREAD-SAFE: Yes
 */
int training_hub_query_module(training_integration_hub_t hub,
                               uint32_t requester_id,
                               uint32_t target_id,
                               const training_query_t* query,
                               training_query_result_t* result);

/**
 * WHAT: Get information about a registered module
 * WHY: Query module metadata for routing decisions
 * HOW: Lookup module in registry, copy info to output
 *
 * @param hub Integration hub
 * @param module_id Module ID to query
 * @param info Output: module information
 * @return 0 on success, -1 on error
 *
 * ERRORS:
 * - Returns -1 if hub is NULL
 * - Returns -1 if module_id not registered
 * - Returns -1 if info is NULL
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int training_hub_get_module_info(training_integration_hub_t hub,
                                  uint32_t module_id,
                                  training_module_info_t* info);

/* ========================================================================
 * QUERY HANDLER REGISTRATION
 * ======================================================================== */

/**
 * WHAT: Query handler callback type
 * WHY: Enable modules to respond to queries
 * HOW: Function pointer with query and result parameters
 *
 * PARAMETERS:
 * - query: Incoming query
 * - result: Output result (must be filled by handler)
 * - context: Module context from registration
 *
 * RETURN: 0 on success, -1 on error
 */
typedef int (*training_query_handler_t)(const training_query_t* query,
                                         training_query_result_t* result,
                                         void* context);

/**
 * WHAT: Register query handler for a module
 * WHY: Enable module to receive and respond to queries
 * HOW: Associate handler function with module
 *
 * @param hub Integration hub
 * @param module_id Module ID registering handler
 * @param handler Query handler function
 * @return 0 on success, -1 on error
 *
 * ERRORS:
 * - Returns -1 if hub is NULL
 * - Returns -1 if module_id not registered
 * - Returns -1 if handler is NULL
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int training_hub_register_query_handler(training_integration_hub_t hub,
                                         uint32_t module_id,
                                         training_query_handler_t handler);

/* ========================================================================
 * MODULE STATE MANAGEMENT
 * ======================================================================== */

/**
 * WHAT: Set module active status
 * WHY: Enable/disable module without unregistering
 * HOW: Update active flag in registry
 *
 * @param hub Integration hub
 * @param module_id Module ID
 * @param is_active New active status
 * @return 0 on success, -1 on error
 *
 * EFFECTS: Inactive modules do not receive events or queries
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int training_hub_set_module_active(training_integration_hub_t hub,
                                    uint32_t module_id,
                                    bool is_active);

/* ========================================================================
 * STATISTICS AND MONITORING
 * ======================================================================== */

/**
 * WHAT: Get hub statistics
 * WHY: Monitor hub performance and health
 * HOW: Copy current statistics to output struct
 *
 * @param hub Integration hub
 * @param stats Output: statistics struct
 * @return 0 on success, -1 on error
 *
 * ERRORS:
 * - Returns -1 if hub is NULL
 * - Returns -1 if stats is NULL
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int training_hub_get_stats(training_integration_hub_t hub,
                            training_hub_stats_t* stats);

/**
 * WHAT: Reset hub statistics
 * WHY: Clear counters for new measurement period
 * HOW: Zero all statistical counters
 *
 * @param hub Integration hub
 * @return 0 on success, -1 on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int training_hub_reset_stats(training_integration_hub_t hub);

/* ========================================================================
 * CATEGORY-BASED OPERATIONS
 * ======================================================================== */

/**
 * WHAT: Get all modules in a training category
 * WHY: Enable category-wide operations (e.g., broadcast to all optimization modules)
 * HOW: Filter registry by category
 *
 * @param hub Integration hub
 * @param category Category to filter by
 * @param module_ids Output array (caller allocated)
 * @param max_modules Maximum modules to return
 * @param count Output: actual number of modules found
 * @return 0 on success, -1 on error
 *
 * ERRORS:
 * - Returns -1 if hub is NULL
 * - Returns -1 if category is invalid
 *
 * COMPLEXITY: O(registered_modules)
 * THREAD-SAFE: Yes
 */
int training_hub_get_modules_by_category(training_integration_hub_t hub,
                                          training_category_t category,
                                          uint32_t* module_ids,
                                          uint32_t max_modules,
                                          uint32_t* count);

/**
 * WHAT: Publish event to all modules in a category
 * WHY: Enable category-wide broadcasts
 * HOW: Iterate category modules, deliver event
 *
 * @param hub Integration hub
 * @param publisher_id Module ID of publisher
 * @param category Target category
 * @param event_type Type of event
 * @param data Event data
 * @return 0 on success, -1 on error
 *
 * COMPLEXITY: O(modules_in_category * subscribers_per_module)
 * THREAD-SAFE: Yes
 */
int training_hub_publish_to_category(training_integration_hub_t hub,
                                      uint32_t publisher_id,
                                      training_category_t category,
                                      training_event_type_t event_type,
                                      const training_event_data_t* data);

/* ========================================================================
 * ASYNC QUEUE MANAGEMENT
 * ======================================================================== */

/**
 * WHAT: Flush async event queue
 * WHY: Ensure all pending events are delivered
 * HOW: Wait for queue to drain
 *
 * @param hub Integration hub
 * @param timeout_ms Maximum time to wait (0 = forever)
 * @return 0 on success, -1 on timeout or error
 *
 * BLOCKING: Yes - waits for queue to drain or timeout
 *
 * COMPLEXITY: O(queued_events)
 * THREAD-SAFE: Yes
 */
int training_hub_flush_async_queue(training_integration_hub_t hub,
                                    uint32_t timeout_ms);

/**
 * WHAT: Get current async queue depth
 * WHY: Monitor queue saturation
 * HOW: Return current queue size
 *
 * @param hub Integration hub
 * @return Queue depth, or 0 if async disabled or error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
uint32_t training_hub_get_async_queue_depth(training_integration_hub_t hub);

/* ========================================================================
 * CONVENIENCE HELPERS FOR COMMON TRAINING EVENTS
 * ======================================================================== */

/**
 * WHAT: Helper to publish difficulty update event
 * WHY: Common operation for curriculum learning
 * HOW: Wrap payload in event and publish
 *
 * @param hub Integration hub
 * @param publisher_id Module ID of publisher
 * @param old_difficulty Previous difficulty level
 * @param new_difficulty New difficulty level
 * @return 0 on success, -1 on error
 */
int training_hub_publish_difficulty_update(training_integration_hub_t hub,
                                            uint32_t publisher_id,
                                            float old_difficulty,
                                            float new_difficulty);

/**
 * WHAT: Helper to publish loss computed event
 * WHY: Common operation for all training modules
 * HOW: Wrap loss value in event and publish
 *
 * @param hub Integration hub
 * @param publisher_id Module ID of publisher
 * @param epoch Current epoch
 * @param batch Current batch
 * @param loss Loss value
 * @return 0 on success, -1 on error
 */
int training_hub_publish_loss(training_integration_hub_t hub,
                               uint32_t publisher_id,
                               uint32_t epoch,
                               uint32_t batch,
                               float loss);

/**
 * WHAT: Helper to publish learning rate adjustment event
 * WHY: Common operation for LR schedulers
 * HOW: Wrap LR change in event and publish
 *
 * @param hub Integration hub
 * @param publisher_id Module ID of publisher
 * @param old_lr Previous learning rate
 * @param new_lr New learning rate
 * @return 0 on success, -1 on error
 */
int training_hub_publish_lr_adjustment(training_integration_hub_t hub,
                                        uint32_t publisher_id,
                                        float old_lr,
                                        float new_lr);

/**
 * WHAT: Helper to publish epoch complete event
 * WHY: Common operation for training loops
 * HOW: Wrap epoch info in event and publish
 *
 * @param hub Integration hub
 * @param publisher_id Module ID of publisher
 * @param epoch Completed epoch number
 * @param avg_loss Average loss for epoch
 * @return 0 on success, -1 on error
 */
int training_hub_publish_epoch_complete(training_integration_hub_t hub,
                                         uint32_t publisher_id,
                                         uint32_t epoch,
                                         float avg_loss);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_TRAINING_INTEGRATION_HUB_H */
