//=============================================================================
// nimcp_event_subscriber.h - Event Subscriber Management
//=============================================================================

#ifndef NIMCP_EVENT_SUBSCRIBER_H
#define NIMCP_EVENT_SUBSCRIBER_H

#include "middleware/events/nimcp_event_types.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file nimcp_event_subscriber.h
 * @brief Event subscriber registration and callback management
 *
 * WHAT: Manages event subscriptions and callback dispatch
 * WHY:  Enable observer pattern for event-driven architecture
 * HOW:  Callback registry with filtering predicates
 *
 * DESIGN PATTERNS:
 * - Observer: Subscribers notified on events
 * - Strategy: Filter predicates for selective subscription
 * - Handle-Body: Opaque subscription handles
 */

//=============================================================================
// Types and Callbacks
//=============================================================================

/**
 * @brief Subscription handle (opaque)
 */
typedef uint64_t subscription_handle_t;
#define SUBSCRIPTION_HANDLE_INVALID 0

/**
 * @brief Event callback function
 *
 * @param event Event that occurred
 * @param context User context
 */
typedef void (*event_callback_fn)(const event_t* event, void* context);

/**
 * @brief Event filter predicate
 *
 * @param event Event to test
 * @param context Filter context
 * @return true if event should be delivered to subscriber
 */
typedef bool (*event_predicate_fn)(const event_t* event, void* context);

/**
 * @brief Subscriber priority
 */
typedef enum {
    SUBSCRIBER_PRIORITY_HIGHEST = 0,
    SUBSCRIBER_PRIORITY_HIGH = 1,
    SUBSCRIBER_PRIORITY_NORMAL = 2,
    SUBSCRIBER_PRIORITY_LOW = 3,
    SUBSCRIBER_PRIORITY_LOWEST = 4
} subscriber_priority_t;

/**
 * @brief Subscription configuration
 */
typedef struct {
    event_type_t* event_types;      /**< Types to subscribe to (NULL = all) */
    uint32_t num_types;             /**< Number of types */
    event_source_t* event_sources;  /**< Sources to filter (NULL = all) */
    uint32_t num_sources;           /**< Number of sources */
    event_predicate_fn predicate;   /**< Custom filter (NULL = no filter) */
    void* predicate_context;        /**< Context for predicate */
    subscriber_priority_t priority; /**< Callback priority */
} subscription_config_t;

/**
 * @brief Subscriber statistics
 */
typedef struct {
    uint64_t events_received;       /**< Total events received */
    uint64_t events_dropped;        /**< Events dropped (filter) */
    uint64_t callback_invocations;  /**< Times callback invoked */
    uint64_t total_callback_time_us; /**< Total time in callbacks */
} subscriber_stats_t;

/**
 * @brief Opaque subscriber manager
 */
typedef struct subscriber_manager_struct* subscriber_manager_t;

//=============================================================================
// Subscriber Manager Lifecycle
//=============================================================================

/**
 * @brief Create subscriber manager
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
subscriber_manager_t subscriber_manager_create(void);

/**
 * @brief Destroy subscriber manager
 *
 * COMPLEXITY: O(n) where n = subscribers
 * THREAD-SAFE: Yes
 */
void subscriber_manager_destroy(subscriber_manager_t manager);

//=============================================================================
// Subscription Management
//=============================================================================

/**
 * @brief Subscribe to events
 *
 * WHAT: Register callback for events matching config
 * WHY:  Enable observer pattern
 * HOW:  Store callback, config, and generate handle
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 *
 * @param manager Subscriber manager
 * @param callback Callback function
 * @param context User context
 * @param config Subscription config (NULL for all events)
 * @return Subscription handle or INVALID on error
 */
subscription_handle_t subscriber_subscribe(subscriber_manager_t manager,
                                           event_callback_fn callback,
                                           void* context,
                                           const subscription_config_t* config);

/**
 * @brief Unsubscribe from events
 *
 * COMPLEXITY: O(n)
 * THREAD-SAFE: Yes
 *
 * @param manager Subscriber manager
 * @param handle Subscription handle
 * @return true on success
 */
bool subscriber_unsubscribe(subscriber_manager_t manager, subscription_handle_t handle);

/**
 * @brief Pause subscription (stop receiving events)
 *
 * COMPLEXITY: O(n)
 * THREAD-SAFE: Yes
 */
bool subscriber_pause(subscriber_manager_t manager, subscription_handle_t handle);

/**
 * @brief Resume subscription
 *
 * COMPLEXITY: O(n)
 * THREAD-SAFE: Yes
 */
bool subscriber_resume(subscriber_manager_t manager, subscription_handle_t handle);

//=============================================================================
// Event Dispatch
//=============================================================================

/**
 * @brief Dispatch event to subscribers
 *
 * WHAT: Call all matching subscriber callbacks
 * WHY:  Notify observers of event
 * HOW:  Filter subscribers, sort by priority, invoke callbacks
 *
 * COMPLEXITY: O(s) where s = matching subscribers
 * THREAD-SAFE: Yes
 *
 * @param manager Subscriber manager
 * @param event Event to dispatch
 * @return Number of subscribers notified
 */
uint32_t subscriber_dispatch_event(subscriber_manager_t manager, const event_t* event);

//=============================================================================
// Subscriber Statistics
//=============================================================================

/**
 * @brief Get subscriber statistics
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
bool subscriber_get_stats(subscriber_manager_t manager, subscription_handle_t handle,
                          subscriber_stats_t* stats);

/**
 * @brief Get total number of subscribers
 */
uint32_t subscriber_get_count(subscriber_manager_t manager);

//=============================================================================
// Utility
//=============================================================================

/**
 * @brief Create default subscription config
 */
subscription_config_t subscriber_default_config(void);

/**
 * @brief Get last error
 */
const char* subscriber_get_last_error(void);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_EVENT_SUBSCRIBER_H
