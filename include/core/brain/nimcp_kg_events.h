/**
 * @file nimcp_kg_events.h
 * @brief Real-time Event Streaming (Pub/Sub) for Brain Knowledge Graph
 * @version 1.0.0
 * @date 2026-01-16
 *
 * WHAT: Change Data Capture (CDC) event streaming for KG modifications
 * WHY:  Enable real-time monitoring and reactive processing of KG changes
 * HOW:  Pub/Sub pattern with callbacks, webhooks, and event replay capability
 *
 * EVENT FLOW:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                    KG EVENT STREAMING ARCHITECTURE                         ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌─────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                       KG MUTATIONS                                   │  ║
 * ║   │  add_node, update_node, delete_node, add_edge, update_edge, ...     │  ║
 * ║   └─────────────────────────────────────────────────────────────────────┘  ║
 * ║                                   │                                        ║
 * ║                                   ▼                                        ║
 * ║   ┌─────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                      EVENT STREAM                                    │  ║
 * ║   │  ┌─────┐ ┌─────┐ ┌─────┐ ┌─────┐ ┌─────┐ ┌─────┐ ┌─────┐          │  ║
 * ║   │  │ E1  │→│ E2  │→│ E3  │→│ E4  │→│ E5  │→│ E6  │→│ ... │          │  ║
 * ║   │  └─────┘ └─────┘ └─────┘ └─────┘ └─────┘ └─────┘ └─────┘          │  ║
 * ║   │           Monotonic IDs, Transaction IDs, Timestamps               │  ║
 * ║   └─────────────────────────────────────────────────────────────────────┘  ║
 * ║                                   │                                        ║
 * ║            ┌──────────────────────┼──────────────────────┐                ║
 * ║            ▼                      ▼                      ▼                ║
 * ║   ┌─────────────────┐   ┌─────────────────┐   ┌─────────────────┐        ║
 * ║   │   SUBSCRIBERS   │   │    WEBHOOKS     │   │  EVENT REPLAY   │        ║
 * ║   │   (Callbacks)   │   │   (HTTP POST)   │   │  (Historical)   │        ║
 * ║   │                 │   │                 │   │                 │        ║
 * ║   │ - Filtered by   │   │ - Batched       │   │ - By event ID   │        ║
 * ║   │   event type    │   │ - Signed        │   │ - By timestamp  │        ║
 * ║   │ - Filtered by   │   │ - Retried       │   │ - By range      │        ║
 * ║   │   module/layer  │   │                 │   │                 │        ║
 * ║   └─────────────────┘   └─────────────────┘   └─────────────────┘        ║
 * ║                                                                            ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * BIOLOGICAL BASIS:
 * Models neural broadcast mechanisms where significant state changes
 * propagate to multiple downstream systems for coordinated response.
 * Similar to neuromodulatory broadcasting in biological neural networks.
 *
 * THREAD SAFETY: All operations are thread-safe via internal synchronization.
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_KG_EVENTS_H
#define NIMCP_KG_EVENTS_H

#include "core/brain/nimcp_brain_kg.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Maximum subscribers per event stream */
#define KG_EVENTS_MAX_SUBSCRIBERS       64

/** Maximum webhooks per event stream */
#define KG_EVENTS_MAX_WEBHOOKS          16

/** Maximum events in replay buffer (circular) */
#define KG_EVENTS_REPLAY_BUFFER_SIZE    4096

/** Maximum payload size in bytes */
#define KG_EVENTS_MAX_PAYLOAD_SIZE      8192

/** Maximum URL length for webhooks */
#define KG_EVENTS_MAX_URL_LEN           256

/** Maximum authorization header length */
#define KG_EVENTS_MAX_AUTH_LEN          256

/** Default webhook timeout in milliseconds */
#define KG_EVENTS_DEFAULT_TIMEOUT_MS    5000

/** Default webhook retry count */
#define KG_EVENTS_DEFAULT_RETRY_COUNT   3

/** Default webhook batch size */
#define KG_EVENTS_DEFAULT_BATCH_SIZE    10

/** Default webhook batch timeout in milliseconds */
#define KG_EVENTS_DEFAULT_BATCH_TIMEOUT_MS  1000

/* ============================================================================
 * Event Types Enumeration
 * ============================================================================ */

/**
 * @brief Event types for CDC (Change Data Capture)
 *
 * WHAT: Enumeration of all trackable KG modification events
 * WHY:  Enable fine-grained subscription filtering
 * HOW:  Bitmask-compatible values for efficient filtering
 */
typedef enum {
    KG_EVENT_NODE_CREATED = 0,       /**< New node added to KG */
    KG_EVENT_NODE_UPDATED,           /**< Existing node modified */
    KG_EVENT_NODE_DELETED,           /**< Node removed from KG */
    KG_EVENT_EDGE_CREATED,           /**< New edge added to KG */
    KG_EVENT_EDGE_UPDATED,           /**< Existing edge modified */
    KG_EVENT_EDGE_DELETED,           /**< Edge removed from KG */
    KG_EVENT_WEIGHT_CHANGED,         /**< Edge weight modified */
    KG_EVENT_NEUROMOD_CHANGED,       /**< Neuromodulator levels changed */
    KG_EVENT_TOPOLOGY_CHANGED,       /**< Significant topology change */
    KG_EVENT_CHECKPOINT_CREATED,     /**< Checkpoint/snapshot created */
    KG_EVENT_TYPE_COUNT              /**< Number of event types */
} kg_event_type_t;

/* ============================================================================
 * Event Type Bitmasks for Filtering
 * ============================================================================ */

/** Bitmask for node created events */
#define KG_EVENT_MASK_NODE_CREATED      (1U << KG_EVENT_NODE_CREATED)

/** Bitmask for node updated events */
#define KG_EVENT_MASK_NODE_UPDATED      (1U << KG_EVENT_NODE_UPDATED)

/** Bitmask for node deleted events */
#define KG_EVENT_MASK_NODE_DELETED      (1U << KG_EVENT_NODE_DELETED)

/** Bitmask for edge created events */
#define KG_EVENT_MASK_EDGE_CREATED      (1U << KG_EVENT_EDGE_CREATED)

/** Bitmask for edge updated events */
#define KG_EVENT_MASK_EDGE_UPDATED      (1U << KG_EVENT_EDGE_UPDATED)

/** Bitmask for edge deleted events */
#define KG_EVENT_MASK_EDGE_DELETED      (1U << KG_EVENT_EDGE_DELETED)

/** Bitmask for weight changed events */
#define KG_EVENT_MASK_WEIGHT_CHANGED    (1U << KG_EVENT_WEIGHT_CHANGED)

/** Bitmask for neuromodulator changed events */
#define KG_EVENT_MASK_NEUROMOD_CHANGED  (1U << KG_EVENT_NEUROMOD_CHANGED)

/** Bitmask for topology changed events */
#define KG_EVENT_MASK_TOPOLOGY_CHANGED  (1U << KG_EVENT_TOPOLOGY_CHANGED)

/** Bitmask for checkpoint created events */
#define KG_EVENT_MASK_CHECKPOINT_CREATED (1U << KG_EVENT_CHECKPOINT_CREATED)

/** Bitmask for all node events */
#define KG_EVENT_MASK_ALL_NODE          (KG_EVENT_MASK_NODE_CREATED | \
                                         KG_EVENT_MASK_NODE_UPDATED | \
                                         KG_EVENT_MASK_NODE_DELETED)

/** Bitmask for all edge events */
#define KG_EVENT_MASK_ALL_EDGE          (KG_EVENT_MASK_EDGE_CREATED | \
                                         KG_EVENT_MASK_EDGE_UPDATED | \
                                         KG_EVENT_MASK_EDGE_DELETED)

/** Bitmask for all events */
#define KG_EVENT_MASK_ALL               ((1U << KG_EVENT_TYPE_COUNT) - 1)

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/**
 * @brief KG change event structure
 *
 * WHAT: Complete event record for a KG modification
 * WHY:  Provide all context needed to understand and replay the change
 * HOW:  Contains event metadata, affected entities, and optional payload
 */
typedef struct {
    uint64_t event_id;                   /**< Unique event ID (monotonic) */
    uint64_t timestamp;                  /**< Event timestamp (nanoseconds) */
    kg_event_type_t type;                /**< Event type */
    brain_kg_node_id_t node_id;          /**< Affected node ID */
    brain_kg_node_id_t edge_from;        /**< For edge events: source node */
    brain_kg_node_id_t edge_to;          /**< For edge events: target node */
    char* payload_json;                  /**< Event payload as JSON (heap allocated) */
    uint32_t payload_size;               /**< Payload size in bytes */
    uint64_t transaction_id;             /**< Transaction this event belongs to */
} kg_event_t;

/**
 * @brief Subscription filter configuration
 *
 * WHAT: Filter criteria for event subscriptions
 * WHY:  Allow subscribers to receive only relevant events
 * HOW:  Combine bitmasks and string filters for precise matching
 */
typedef struct {
    uint32_t event_types;                /**< Bitmask of event types to receive */
    char node_type_filter[64];           /**< Filter by node type (empty = all) */
    char module_filter[64];              /**< Filter by module name pattern */
    uint8_t layer_filter;                /**< Filter by cortical layer (0 = all) */
    uint8_t hemisphere_filter;           /**< Filter by hemisphere (0 = all) */
    bool include_payload;                /**< Include full payload in events */
} kg_subscription_filter_t;

/**
 * @brief Event callback function type
 *
 * @param event The event that occurred (read-only)
 * @param user_data User-provided context from subscription
 *
 * WHAT: Callback signature for event notifications
 * WHY:  Decouple event production from consumption
 * HOW:  Subscriber provides function matching this signature
 */
typedef void (*kg_event_callback_fn)(const kg_event_t* event, void* user_data);

/**
 * @brief Webhook configuration structure
 *
 * WHAT: Configuration for HTTP webhook delivery
 * WHY:  Enable external system integration via HTTP
 * HOW:  Configurable URL, auth, batching, and retry behavior
 */
typedef struct {
    char url[KG_EVENTS_MAX_URL_LEN];     /**< Webhook URL (HTTPS recommended) */
    char auth_header[KG_EVENTS_MAX_AUTH_LEN]; /**< Authorization header value */
    uint32_t timeout_ms;                 /**< Request timeout in milliseconds */
    uint32_t retry_count;                /**< Number of retries on failure */
    uint32_t batch_size;                 /**< Events per request */
    uint32_t batch_timeout_ms;           /**< Max wait time for batch fill */
    bool enable_signature;               /**< Sign webhook payload (HMAC) */
} kg_webhook_config_t;

/**
 * @brief Event stream configuration
 *
 * WHAT: Configuration for the event stream system
 * WHY:  Allow tuning of buffer sizes and behavior
 * HOW:  Set at stream creation time
 */
typedef struct {
    uint32_t replay_buffer_size;         /**< Size of replay buffer (0 = default) */
    bool enable_persistence;             /**< Persist events to disk */
    char persistence_path[256];          /**< Path for event persistence */
    bool enable_compression;             /**< Compress event payloads */
    uint32_t max_subscribers;            /**< Maximum subscribers (0 = default) */
    uint32_t max_webhooks;               /**< Maximum webhooks (0 = default) */
} kg_event_config_t;

/* ============================================================================
 * Opaque Types
 * ============================================================================ */

/**
 * @brief Event stream handle (opaque)
 */
typedef struct kg_event_stream kg_event_stream_t;

/**
 * @brief Subscription identifier
 */
typedef uint32_t kg_subscription_id_t;

/** Invalid subscription ID sentinel */
#define KG_SUBSCRIPTION_INVALID         UINT32_MAX

/* ============================================================================
 * Event Stream Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default event stream configuration
 *
 * WHAT: Initialize config struct with sensible defaults
 * WHY:  Simplify stream creation for common use cases
 * HOW:  Sets default buffer sizes, disables optional features
 *
 * @param config Output configuration structure
 * @return 0 on success, -1 on NULL pointer
 */
int kg_events_default_config(kg_event_config_t* config);

/**
 * @brief Create event stream attached to knowledge graph
 *
 * WHAT: Initialize event streaming for a KG instance
 * WHY:  Enable change tracking and notification
 * HOW:  Allocate stream state, register with KG for change notifications
 *
 * @param kg Brain knowledge graph to attach to
 * @param config Configuration (NULL for defaults)
 * @return Event stream handle or NULL on error
 */
kg_event_stream_t* kg_events_create(brain_kg_t* kg, const kg_event_config_t* config);

/**
 * @brief Destroy event stream and free resources
 *
 * WHAT: Clean up event stream state
 * WHY:  Release memory, close connections, notify subscribers
 * HOW:  Unsubscribe all, flush pending webhooks, free memory
 *
 * @param stream Event stream to destroy (NULL safe)
 */
void kg_events_destroy(kg_event_stream_t* stream);

/* ============================================================================
 * Subscription API
 * ============================================================================ */

/**
 * @brief Get default subscription filter
 *
 * WHAT: Initialize filter with defaults (all events, no filtering)
 * WHY:  Simplify subscription creation
 * HOW:  Sets event_types to all, clears string filters
 *
 * @param filter Output filter structure
 * @return 0 on success, -1 on NULL pointer
 */
int kg_events_default_filter(kg_subscription_filter_t* filter);

/**
 * @brief Subscribe to KG events with filtering
 *
 * WHAT: Register callback to receive matching events
 * WHY:  Enable reactive processing of KG changes
 * HOW:  Register callback with filter, return subscription ID
 *
 * @param stream Event stream handle
 * @param filter Subscription filter (NULL for all events)
 * @param callback Function to call when events match
 * @param user_data Context passed to callback
 * @return Subscription ID or KG_SUBSCRIPTION_INVALID on error
 */
kg_subscription_id_t kg_events_subscribe(
    kg_event_stream_t* stream,
    const kg_subscription_filter_t* filter,
    kg_event_callback_fn callback,
    void* user_data
);

/**
 * @brief Unsubscribe from events
 *
 * WHAT: Remove a subscription by ID
 * WHY:  Stop receiving events when no longer needed
 * HOW:  Remove from subscription list, free resources
 *
 * @param stream Event stream handle
 * @param sub_id Subscription ID to remove
 * @return 0 on success, -1 if not found
 */
int kg_events_unsubscribe(kg_event_stream_t* stream, kg_subscription_id_t sub_id);

/**
 * @brief Update subscription filter
 *
 * WHAT: Modify filter criteria for existing subscription
 * WHY:  Change what events are received without resubscribing
 * HOW:  Replace filter in subscription record
 *
 * @param stream Event stream handle
 * @param sub_id Subscription ID to update
 * @param filter New filter criteria
 * @return 0 on success, -1 if not found
 */
int kg_events_update_filter(
    kg_event_stream_t* stream,
    kg_subscription_id_t sub_id,
    const kg_subscription_filter_t* filter
);

/**
 * @brief Get subscription count
 *
 * @param stream Event stream handle
 * @return Number of active subscriptions, or 0 on error
 */
uint32_t kg_events_get_subscription_count(const kg_event_stream_t* stream);

/* ============================================================================
 * Webhook API
 * ============================================================================ */

/**
 * @brief Get default webhook configuration
 *
 * WHAT: Initialize webhook config with sensible defaults
 * WHY:  Simplify webhook setup for common cases
 * HOW:  Sets default timeouts, retries, batch sizes
 *
 * @param config Output configuration structure
 * @return 0 on success, -1 on NULL pointer
 */
int kg_events_default_webhook_config(kg_webhook_config_t* config);

/**
 * @brief Add webhook for event delivery
 *
 * WHAT: Register HTTP endpoint for event notification
 * WHY:  Integrate with external systems via HTTP
 * HOW:  Add webhook to list, start delivery thread if needed
 *
 * @param stream Event stream handle
 * @param config Webhook configuration
 * @return 0 on success, -1 on error (max reached or invalid config)
 */
int kg_events_add_webhook(kg_event_stream_t* stream, const kg_webhook_config_t* config);

/**
 * @brief Remove webhook by URL
 *
 * WHAT: Unregister webhook endpoint
 * WHY:  Stop sending events to endpoint
 * HOW:  Remove from list, flush pending events
 *
 * @param stream Event stream handle
 * @param url Webhook URL to remove
 * @return 0 on success, -1 if not found
 */
int kg_events_remove_webhook(kg_event_stream_t* stream, const char* url);

/**
 * @brief Get webhook count
 *
 * @param stream Event stream handle
 * @return Number of registered webhooks, or 0 on error
 */
uint32_t kg_events_get_webhook_count(const kg_event_stream_t* stream);

/**
 * @brief Pause webhook delivery
 *
 * WHAT: Temporarily stop sending events to webhooks
 * WHY:  Handle maintenance or error conditions
 * HOW:  Set paused flag, buffer events
 *
 * @param stream Event stream handle
 * @return 0 on success
 */
int kg_events_pause_webhooks(kg_event_stream_t* stream);

/**
 * @brief Resume webhook delivery
 *
 * WHAT: Resume sending events to webhooks
 * WHY:  Restore normal operation after pause
 * HOW:  Clear paused flag, flush buffered events
 *
 * @param stream Event stream handle
 * @return 0 on success
 */
int kg_events_resume_webhooks(kg_event_stream_t* stream);

/* ============================================================================
 * Event Replay API
 * ============================================================================ */

/**
 * @brief Replay events starting from a specific event ID
 *
 * WHAT: Re-deliver historical events to callback
 * WHY:  Enable catch-up after reconnection or failure
 * HOW:  Read from replay buffer, invoke callback for each event
 *
 * @param stream Event stream handle
 * @param from_event_id Starting event ID (inclusive)
 * @param callback Function to receive replayed events
 * @param user_data Context passed to callback
 * @return Number of events replayed, or -1 on error
 */
int kg_events_replay(
    kg_event_stream_t* stream,
    uint64_t from_event_id,
    kg_event_callback_fn callback,
    void* user_data
);

/**
 * @brief Replay events within a timestamp range
 *
 * WHAT: Re-deliver events from a time period
 * WHY:  Analyze historical changes in a time window
 * HOW:  Read from replay buffer by timestamp, invoke callback
 *
 * @param stream Event stream handle
 * @param start_timestamp Start time (nanoseconds, inclusive)
 * @param end_timestamp End time (nanoseconds, inclusive)
 * @param callback Function to receive replayed events
 * @param user_data Context passed to callback
 * @return Number of events replayed, or -1 on error
 */
int kg_events_replay_range(
    kg_event_stream_t* stream,
    uint64_t start_timestamp,
    uint64_t end_timestamp,
    kg_event_callback_fn callback,
    void* user_data
);

/**
 * @brief Replay events with a filter
 *
 * WHAT: Re-deliver filtered historical events
 * WHY:  Replay only relevant events
 * HOW:  Apply filter during replay
 *
 * @param stream Event stream handle
 * @param from_event_id Starting event ID (inclusive)
 * @param filter Filter criteria (NULL for all)
 * @param callback Function to receive replayed events
 * @param user_data Context passed to callback
 * @return Number of events replayed, or -1 on error
 */
int kg_events_replay_filtered(
    kg_event_stream_t* stream,
    uint64_t from_event_id,
    const kg_subscription_filter_t* filter,
    kg_event_callback_fn callback,
    void* user_data
);

/* ============================================================================
 * Event Log Management API
 * ============================================================================ */

/**
 * @brief Get the latest event ID
 *
 * WHAT: Query the most recent event ID
 * WHY:  Track position in event stream for replay
 * HOW:  Return current event counter
 *
 * @param stream Event stream handle
 * @return Latest event ID, or 0 if no events
 */
uint64_t kg_events_get_latest_id(const kg_event_stream_t* stream);

/**
 * @brief Get subscription lag (events behind)
 *
 * WHAT: Calculate how many events a subscription is behind
 * WHY:  Monitor subscription health and backpressure
 * HOW:  Compare last delivered ID with latest ID
 *
 * @param stream Event stream handle
 * @param sub_id Subscription to query
 * @param lag Output: number of events behind
 * @return 0 on success, -1 if subscription not found
 */
int kg_events_get_lag(
    const kg_event_stream_t* stream,
    kg_subscription_id_t sub_id,
    uint64_t* lag
);

/**
 * @brief Get oldest event ID in replay buffer
 *
 * WHAT: Query the oldest available event for replay
 * WHY:  Determine replay range availability
 * HOW:  Return head of circular buffer
 *
 * @param stream Event stream handle
 * @return Oldest event ID, or 0 if buffer empty
 */
uint64_t kg_events_get_oldest_id(const kg_event_stream_t* stream);

/**
 * @brief Get event count in replay buffer
 *
 * @param stream Event stream handle
 * @return Number of events in buffer
 */
uint32_t kg_events_get_buffer_count(const kg_event_stream_t* stream);

/**
 * @brief Get total events emitted since stream creation
 *
 * @param stream Event stream handle
 * @return Total event count
 */
uint64_t kg_events_get_total_count(const kg_event_stream_t* stream);

/* ============================================================================
 * Event Emission API (Internal Use)
 * ============================================================================ */

/**
 * @brief Emit an event to the stream
 *
 * WHAT: Add event to stream and notify subscribers
 * WHY:  Called by KG when modifications occur
 * HOW:  Assign ID, timestamp, add to buffer, dispatch to subscribers
 *
 * @note This is typically called internally by the KG implementation.
 *       External callers should not normally use this function.
 *
 * @param stream Event stream handle
 * @param type Event type
 * @param node_id Affected node ID
 * @param edge_from Source node (for edge events)
 * @param edge_to Target node (for edge events)
 * @param payload_json Optional JSON payload (copied)
 * @param transaction_id Transaction ID (0 for standalone)
 * @return Event ID assigned, or 0 on error
 */
uint64_t kg_events_emit(
    kg_event_stream_t* stream,
    kg_event_type_t type,
    brain_kg_node_id_t node_id,
    brain_kg_node_id_t edge_from,
    brain_kg_node_id_t edge_to,
    const char* payload_json,
    uint64_t transaction_id
);

/* ============================================================================
 * Event Utility Functions
 * ============================================================================ */

/**
 * @brief Free event resources
 *
 * WHAT: Release memory allocated in event structure
 * WHY:  Clean up after event processing
 * HOW:  Free payload_json if allocated
 *
 * @param event Event to free (NULL safe)
 * @note Only frees internal allocations, not the event struct itself
 */
void kg_event_free(kg_event_t* event);

/**
 * @brief Clone an event
 *
 * WHAT: Create a deep copy of an event
 * WHY:  Preserve event for later processing
 * HOW:  Copy all fields, duplicate payload string
 *
 * @param event Event to clone
 * @param dest Destination event (caller allocated)
 * @return 0 on success, -1 on error
 */
int kg_event_clone(const kg_event_t* event, kg_event_t* dest);

/**
 * @brief Convert event type to string
 *
 * @param type Event type
 * @return Human-readable string
 */
const char* kg_event_type_to_string(kg_event_type_t type);

/**
 * @brief Parse event type from string
 *
 * @param str String representation
 * @return Event type, or KG_EVENT_TYPE_COUNT if invalid
 */
kg_event_type_t kg_event_type_from_string(const char* str);

/**
 * @brief Check if event matches filter
 *
 * WHAT: Test if an event passes filter criteria
 * WHY:  Manual filtering outside subscription system
 * HOW:  Check bitmask and string filters
 *
 * @param event Event to check
 * @param filter Filter criteria
 * @return true if event matches, false otherwise
 */
bool kg_event_matches_filter(const kg_event_t* event, const kg_subscription_filter_t* filter);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_KG_EVENTS_H */
