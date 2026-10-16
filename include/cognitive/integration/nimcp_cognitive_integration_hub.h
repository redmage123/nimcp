/**
 * @file nimcp_cognitive_integration_hub.h
 * @brief Central hub for cognitive module integration and event routing
 * @version 1.0.0
 * @date 2025
 *
 * WHAT: Central hub that connects all cognitive modules, enabling event-driven
 *       communication and cross-module queries.
 *
 * WHY: Cognitive modules need to communicate without tight coupling. The hub
 *      provides a publish-subscribe event system and request-response queries.
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

#ifndef NIMCP_COGNITIVE_INTEGRATION_HUB_H
#define NIMCP_COGNITIVE_INTEGRATION_HUB_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "cognitive/integration/nimcp_cognitive_event_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * OPAQUE TYPE DECLARATIONS
 * ======================================================================== */

/**
 * WHAT: Opaque handle to cognitive integration hub
 * WHY: Encapsulation - hide internal implementation details
 * HOW: Pimpl idiom - pointer to internal structure
 */
typedef struct cognitive_integration_hub_struct* cognitive_integration_hub_t;

/* ========================================================================
 * CONFIGURATION STRUCTURES
 * ======================================================================== */

/**
 * WHAT: Configuration for cognitive integration hub
 * WHY: Customize hub behavior at creation time
 * HOW: Struct with all hub configuration parameters
 *
 * FIELDS:
 * - max_modules: Maximum number of modules that can register
 * - max_subscriptions: Maximum total subscriptions across all modules
 * - enable_async: Enable asynchronous event delivery
 * - event_queue_size: Size of async event queue (if async enabled)
 */
typedef struct {
    uint32_t max_modules;           /* Maximum registered modules */
    uint32_t max_subscriptions;     /* Maximum total subscriptions */
    bool enable_async;              /* Enable async event delivery */
    uint32_t event_queue_size;      /* Async event queue size */
} cognitive_hub_config_t;

/**
 * WHAT: Information about a registered cognitive module
 * WHY: Track module metadata for routing and queries
 * HOW: Struct with identification and state
 *
 * FIELDS:
 * - module_id: Unique module identifier
 * - category: Cognitive category of module
 * - name: Human-readable module name
 * - context: Module-provided context pointer
 * - is_active: Whether module is currently active
 */
typedef struct {
    uint32_t module_id;             /* Unique module identifier */
    cognitive_category_t category;  /* Module's cognitive category */
    char name[64];                  /* Human-readable name */
    void* context;                  /* Module context pointer */
    bool is_active;                 /* Module active status */
} cognitive_module_info_t;

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
} cognitive_hub_stats_t;

/* ========================================================================
 * DEFAULT CONFIGURATION
 * ======================================================================== */

/**
 * WHAT: Get default hub configuration
 * WHY: Provide sensible defaults for common use cases
 * HOW: Return pre-configured struct
 *
 * DEFAULT VALUES:
 * - max_modules: 64
 * - max_subscriptions: 256
 * - enable_async: true
 * - event_queue_size: 1024
 *
 * @return Default configuration struct
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
cognitive_hub_config_t cognitive_hub_default_config(void);

/* ========================================================================
 * LIFECYCLE MANAGEMENT
 * ======================================================================== */

/**
 * WHAT: Create a new cognitive integration hub
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
 * MEMORY: Caller must call cognitive_hub_destroy() when done
 *
 * COMPLEXITY: O(max_modules + max_subscriptions)
 * THREAD-SAFE: Yes
 */
cognitive_integration_hub_t cognitive_hub_create(const cognitive_hub_config_t* config);

/**
 * WHAT: Destroy cognitive integration hub
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
void cognitive_hub_destroy(cognitive_integration_hub_t hub);

/* ========================================================================
 * MODULE REGISTRATION
 * ======================================================================== */

/**
 * WHAT: Register a cognitive module with the hub
 * WHY: Enable module to publish events and receive subscriptions
 * HOW: Add module to registry with metadata
 *
 * @param hub Integration hub
 * @param module_id Unique module identifier
 * @param category Cognitive category of module
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
int cognitive_hub_register_module(cognitive_integration_hub_t hub,
                                  uint32_t module_id,
                                  cognitive_category_t category,
                                  const char* name,
                                  void* context);

/**
 * WHAT: Unregister a cognitive module from the hub
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
int cognitive_hub_unregister_module(cognitive_integration_hub_t hub,
                                    uint32_t module_id);

/* ========================================================================
 * EVENT SUBSCRIPTION
 * ======================================================================== */

/**
 * WHAT: Subscribe to cognitive events of a specific type
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
int cognitive_hub_subscribe(cognitive_integration_hub_t hub,
                            uint32_t subscriber_id,
                            cognitive_event_type_t event_type,
                            cognitive_event_callback_t callback,
                            void* user_data);

/**
 * WHAT: Unsubscribe from cognitive events of a specific type
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
int cognitive_hub_unsubscribe(cognitive_integration_hub_t hub,
                              uint32_t subscriber_id,
                              cognitive_event_type_t event_type);

/* ========================================================================
 * EVENT PUBLISHING
 * ======================================================================== */

/**
 * WHAT: Publish cognitive event synchronously
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
int cognitive_hub_publish(cognitive_integration_hub_t hub,
                          uint32_t publisher_id,
                          cognitive_event_type_t event_type,
                          const cognitive_event_data_t* data);

/**
 * WHAT: Publish cognitive event asynchronously
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
int cognitive_hub_publish_async(cognitive_integration_hub_t hub,
                                uint32_t publisher_id,
                                cognitive_event_type_t event_type,
                                const cognitive_event_data_t* data);

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
int cognitive_hub_query_module(cognitive_integration_hub_t hub,
                               uint32_t requester_id,
                               uint32_t target_id,
                               const cognitive_query_t* query,
                               cognitive_query_result_t* result);

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
int cognitive_hub_get_module_info(cognitive_integration_hub_t hub,
                                  uint32_t module_id,
                                  cognitive_module_info_t* info);

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
typedef int (*cognitive_query_handler_t)(const cognitive_query_t* query,
                                         cognitive_query_result_t* result,
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
int cognitive_hub_register_query_handler(cognitive_integration_hub_t hub,
                                         uint32_t module_id,
                                         cognitive_query_handler_t handler);

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
int cognitive_hub_set_module_active(cognitive_integration_hub_t hub,
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
int cognitive_hub_get_stats(cognitive_integration_hub_t hub,
                            cognitive_hub_stats_t* stats);

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
int cognitive_hub_reset_stats(cognitive_integration_hub_t hub);

/* ========================================================================
 * CATEGORY-BASED OPERATIONS
 * ======================================================================== */

/**
 * WHAT: Get all modules in a cognitive category
 * WHY: Enable category-wide operations (e.g., broadcast to all memory modules)
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
int cognitive_hub_get_modules_by_category(cognitive_integration_hub_t hub,
                                          cognitive_category_t category,
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
int cognitive_hub_publish_to_category(cognitive_integration_hub_t hub,
                                      uint32_t publisher_id,
                                      cognitive_category_t category,
                                      cognitive_event_type_t event_type,
                                      const cognitive_event_data_t* data);

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
int cognitive_hub_flush_async_queue(cognitive_integration_hub_t hub,
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
uint32_t cognitive_hub_get_async_queue_depth(cognitive_integration_hub_t hub);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_COGNITIVE_INTEGRATION_HUB_H */
