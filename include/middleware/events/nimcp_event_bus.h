//=============================================================================
// nimcp_event_bus.h - Event Bus (Pub/Sub System)
//=============================================================================

#ifndef NIMCP_EVENT_BUS_H
#define NIMCP_EVENT_BUS_H

#include "middleware/events/nimcp_event_types.h"
#include "middleware/events/nimcp_event_queue.h"
#include "middleware/events/nimcp_event_subscriber.h"
#include "async/nimcp_future.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file nimcp_event_bus.h
 * @brief Thread-safe publish/subscribe event bus with async support
 *
 * WHAT: Central event routing and delivery system with futures
 * WHY:  Decouple event producers from consumers with async confirmation
 * HOW:  Queue + subscriber manager + async delivery thread + promises
 */

//=============================================================================
// Types
//=============================================================================

typedef struct event_bus_struct* event_bus_t;

typedef struct {
    uint32_t queue_capacity;
    overflow_policy_t overflow_policy;
    bool async_delivery;        /**< Async delivery thread? */
    uint32_t delivery_thread_sleep_us; /**< Sleep between checks */
    bool enable_bio_async;      /**< Enable bio-async integration */
} event_bus_config_t;

typedef struct {
    uint64_t events_published;
    uint64_t events_delivered;
    uint64_t events_dropped;
    uint32_t active_subscribers;
    uint32_t queue_size;
    uint64_t async_publishes;      /**< Async publish count */
    uint64_t request_responses;    /**< Request-response count */
} event_bus_stats_t;

//=============================================================================
// Lifecycle
//=============================================================================

/**
 * @brief Create event bus
 */
event_bus_t event_bus_create(const event_bus_config_t* config);

/**
 * @brief Destroy event bus
 */
void event_bus_destroy(event_bus_t bus);

//=============================================================================
// Publish/Subscribe
//=============================================================================

/**
 * @brief Publish event (add to delivery queue)
 *
 * COMPLEXITY: O(log n) queue insertion
 * THREAD-SAFE: Yes
 */
bool event_bus_publish(event_bus_t bus, const event_t* event);

/**
 * @brief Publish event asynchronously with delivery confirmation
 *
 * WHAT: Publish event and get future for delivery confirmation
 * WHY:  Enable async event processing with completion tracking
 * HOW:  Queue event, create promise, resolve after delivery
 *
 * COMPLEXITY: O(log n) queue insertion + O(1) promise creation
 * THREAD-SAFE: Yes
 *
 * @param bus Event bus handle
 * @param event Event to publish
 * @return Future that completes when event is delivered to all subscribers,
 *         or NULL on error. Future result is uint32_t (number of subscribers notified).
 *
 * EXAMPLE:
 * ```c
 * event_t event = event_create_pattern_detected(...);
 * nimcp_future_t future = event_bus_publish_async(bus, &event);
 * if (future) {
 *     // Do other work...
 *     if (nimcp_future_wait_timeout(future, 1000)) {
 *         uint32_t count;
 *         nimcp_future_get(future, &count);
 *         printf("Delivered to %u subscribers\n", count);
 *     }
 *     nimcp_future_destroy(future);
 * }
 * ```
 */
nimcp_future_t event_bus_publish_async(event_bus_t bus, const event_t* event);

/**
 * @brief Request-response pattern with async future
 *
 * WHAT: Publish request event and wait for response event
 * WHY:  Enable synchronous-style async communication patterns
 * HOW:  Subscribe to response type, publish request, return future
 *
 * COMPLEXITY: O(log n) + O(m) where n = queue size, m = subscribers
 * THREAD-SAFE: Yes
 *
 * @param bus Event bus handle
 * @param request Request event to publish
 * @param response_type Expected response event type
 * @param timeout_ms Maximum time to wait for response (0 = no timeout)
 * @return Future containing response event, or NULL on error.
 *         Future will fail on timeout or if no response received.
 *
 * EXAMPLE:
 * ```c
 * event_t request = event_create_custom(...);
 * nimcp_future_t future = event_bus_request_async(
 *     bus, &request, EVENT_TYPE_CUSTOM, 5000
 * );
 * if (future && nimcp_future_wait(future)) {
 *     event_t response;
 *     nimcp_future_get(future, &response);
 *     // Process response
 *     event_free(&response);
 * }
 * nimcp_future_destroy(future);
 * ```
 */
nimcp_future_t event_bus_request_async(event_bus_t bus,
                                       const event_t* request,
                                       event_type_t response_type,
                                       uint32_t timeout_ms);

/**
 * @brief Subscribe to events
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 *
 * @return Subscription handle for unsubscribe
 */
subscription_handle_t event_bus_subscribe(event_bus_t bus,
                                          event_callback_fn callback,
                                          void* context,
                                          const subscription_config_t* config);

/**
 * @brief Unsubscribe
 */
bool event_bus_unsubscribe(event_bus_t bus, subscription_handle_t handle);

//=============================================================================
// Control
//=============================================================================

/**
 * @brief Process pending events (manual delivery)
 *
 * WHAT: Dequeue and deliver up to max_events
 * WHY:  For sync delivery mode or manual control
 * HOW:  Dequeue batch, dispatch each
 *
 * @return Number of events processed
 */
uint32_t event_bus_process_events(event_bus_t bus, uint32_t max_events);

/**
 * @brief Get statistics
 */
bool event_bus_get_stats(event_bus_t bus, event_bus_stats_t* stats);

/**
 * @brief Default config
 */
event_bus_config_t event_bus_default_config(void);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_EVENT_BUS_H
