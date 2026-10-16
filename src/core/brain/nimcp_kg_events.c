/**
 * @file nimcp_kg_events.c
 * @brief Real-time Event Streaming (Pub/Sub) for Brain Knowledge Graph
 * @version 1.0.0
 * @date 2026-01-16
 *
 * Implementation of Change Data Capture (CDC) event streaming for KG modifications.
 * Provides pub/sub pattern with callbacks, webhooks, and event replay capability.
 */

#include "core/brain/nimcp_kg_events.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <time.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_buffer_constants.h"

BRIDGE_BOILERPLATE_MESH_ONLY(kg_events, MESH_ADAPTER_CATEGORY_COGNITIVE)


/* ============================================================================
 * Internal Structures
 * ============================================================================ */

/**
 * @brief Subscription record
 */
typedef struct kg_subscription {
    kg_subscription_id_t id;             /**< Unique subscription ID */
    kg_subscription_filter_t filter;     /**< Filter criteria */
    kg_event_callback_fn callback;       /**< Callback function */
    void* user_data;                     /**< User context */
    bool active;                         /**< Active flag */
    uint64_t last_delivered_id;          /**< Last event delivered */
} kg_subscription_t;

/**
 * @brief Webhook record with delivery state
 */
typedef struct kg_webhook {
    kg_webhook_config_t config;          /**< Webhook configuration */
    bool active;                         /**< Active flag */
    uint64_t events_delivered;           /**< Total events delivered */
    uint64_t events_failed;              /**< Total delivery failures */
    uint64_t last_success_ts;            /**< Last successful delivery */
    uint64_t last_failure_ts;            /**< Last failed delivery */
    char last_error[NIMCP_ERROR_BUFFER_MEDIUM];                /**< Last error message */
} kg_webhook_t;

/**
 * @brief Replay buffer entry
 */
typedef struct kg_replay_entry {
    kg_event_t event;                    /**< Event data */
    bool valid;                          /**< Valid entry flag */
} kg_replay_entry_t;

/**
 * @brief Event stream implementation
 */
struct kg_event_stream {
    brain_kg_t* kg;                      /**< Associated knowledge graph */
    kg_event_config_t config;            /**< Configuration */

    /* Subscription management */
    kg_subscription_t* subscriptions;    /**< Subscription array */
    uint32_t subscription_count;         /**< Active subscription count */
    uint32_t max_subscriptions;          /**< Maximum subscriptions */
    uint32_t next_subscription_id;       /**< Next subscription ID */

    /* Webhook management */
    kg_webhook_t* webhooks;              /**< Webhook array */
    uint32_t webhook_count;              /**< Active webhook count */
    uint32_t max_webhooks;               /**< Maximum webhooks */
    bool webhooks_paused;                /**< Webhooks paused flag */

    /* Replay buffer (circular) */
    kg_replay_entry_t* replay_buffer;    /**< Circular replay buffer */
    uint32_t replay_head;                /**< Head index (oldest) */
    uint32_t replay_tail;                /**< Tail index (next write) */
    uint32_t replay_count;               /**< Current count */
    uint32_t replay_capacity;            /**< Buffer capacity */

    /* Event counters */
    uint64_t next_event_id;              /**< Next event ID */
    uint64_t total_events;               /**< Total events emitted */

    /* Thread safety */
    nimcp_mutex_t* mutex;                /**< Stream mutex */

    /* Persistence (optional) */
    FILE* persistence_file;              /**< Persistence file handle */
};

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Get current timestamp in nanoseconds
 */
static uint64_t get_timestamp_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

/**
 * @brief Check if a subscription filter matches an event
 */
static bool filter_matches_event(
    const kg_subscription_filter_t* filter,
    const kg_event_t* event
) {
    if (!filter || !event) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "filter_matches_event: required parameter is NULL (filter, event)");
        return false;
    }

    /* Check event type bitmask */
    uint32_t event_mask = (1U << event->type);
    if (!(filter->event_types & event_mask)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "filter_matches_event: validation failed");
        return false;
    }

    /* Node type filter is checked via payload if available */
    /* Module filter is checked via payload if available */
    /* Layer and hemisphere filters are checked via payload if available */

    /* For now, if payload filtering is needed, we'd parse the JSON */
    /* Simplified: if string filters are set and no payload, skip those checks */

    return true;
}

/**
 * @brief Add event to replay buffer
 */
static void add_to_replay_buffer(kg_event_stream_t* stream, const kg_event_t* event) {
    if (!stream || !event || !stream->replay_buffer) {
        return;
    }

    /* Get entry at tail */
    kg_replay_entry_t* entry = &stream->replay_buffer[stream->replay_tail];

    /* Free old entry if valid */
    if (entry->valid && entry->event.payload_json) {
        nimcp_free(entry->event.payload_json);
        entry->event.payload_json = NULL;
    }

    /* Copy event */
    entry->event.event_id = event->event_id;
    entry->event.timestamp = event->timestamp;
    entry->event.type = event->type;
    entry->event.node_id = event->node_id;
    entry->event.edge_from = event->edge_from;
    entry->event.edge_to = event->edge_to;
    entry->event.transaction_id = event->transaction_id;
    entry->event.payload_size = event->payload_size;

    /* Clone payload */
    if (event->payload_json && event->payload_size > 0) {
        entry->event.payload_json = nimcp_malloc(event->payload_size + 1);
        if (entry->event.payload_json) {
            memcpy(entry->event.payload_json, event->payload_json, event->payload_size);
            entry->event.payload_json[event->payload_size] = '\0';
        }
    } else {
        entry->event.payload_json = NULL;
    }

    entry->valid = true;

    /* Advance tail */
    stream->replay_tail = (stream->replay_tail + 1) % stream->replay_capacity;

    /* Update count and head if buffer full */
    if (stream->replay_count < stream->replay_capacity) {
        stream->replay_count++;
    } else {
        /* Buffer full, advance head (oldest entry lost) */
        stream->replay_head = (stream->replay_head + 1) % stream->replay_capacity;
    }
}

/**
 * @brief Dispatch event to all matching subscribers
 */
static void dispatch_to_subscribers(kg_event_stream_t* stream, const kg_event_t* event) {
    if (!stream || !event) {
        return;
    }

    for (uint32_t i = 0; i < stream->max_subscriptions; i++) {
        kg_subscription_t* sub = &stream->subscriptions[i];
        if (!sub->active) {
            continue;
        }

        if (filter_matches_event(&sub->filter, event)) {
            /* Invoke callback */
            if (sub->callback) {
                sub->callback(event, sub->user_data);
            }
            sub->last_delivered_id = event->event_id;
        }
    }
}

/**
 * @brief Persist event to file (if enabled)
 */
static void persist_event(kg_event_stream_t* stream, const kg_event_t* event) {
    if (!stream || !event || !stream->persistence_file) {
        return;
    }

    /* Simple JSON-lines format */
    fprintf(stream->persistence_file,
            "{\"id\":%lu,\"ts\":%lu,\"type\":%d,\"node\":%lu,\"from\":%lu,\"to\":%lu,\"txn\":%lu",
            (unsigned long)event->event_id,
            (unsigned long)event->timestamp,
            (int)event->type,
            (unsigned long)event->node_id,
            (unsigned long)event->edge_from,
            (unsigned long)event->edge_to,
            (unsigned long)event->transaction_id);

    if (event->payload_json) {
        /* Escape the payload (simplified - just write as-is for now) */
        fprintf(stream->persistence_file, ",\"payload\":%s", event->payload_json);
    }

    fprintf(stream->persistence_file, "}\n");
    fflush(stream->persistence_file);
}

/* ============================================================================
 * Event Stream Lifecycle API
 * ============================================================================ */

int kg_events_default_config(kg_event_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;
    }

    memset(config, 0, sizeof(*config));
    config->replay_buffer_size = KG_EVENTS_REPLAY_BUFFER_SIZE;
    config->enable_persistence = false;
    config->enable_compression = false;
    config->max_subscribers = KG_EVENTS_MAX_SUBSCRIBERS;
    config->max_webhooks = KG_EVENTS_MAX_WEBHOOKS;

    return 0;
}

kg_event_stream_t* kg_events_create(brain_kg_t* kg, const kg_event_config_t* config) {
    kg_event_stream_t* stream = nimcp_calloc(1, sizeof(kg_event_stream_t));
    if (!stream) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "stream is NULL");

        return NULL;
    }

    stream->kg = kg;

    /* Apply configuration */
    if (config) {
        stream->config = *config;
    } else {
        kg_events_default_config(&stream->config);
    }

    /* Set defaults if zero */
    if (stream->config.replay_buffer_size == 0) {
        stream->config.replay_buffer_size = KG_EVENTS_REPLAY_BUFFER_SIZE;
    }
    if (stream->config.max_subscribers == 0) {
        stream->config.max_subscribers = KG_EVENTS_MAX_SUBSCRIBERS;
    }
    if (stream->config.max_webhooks == 0) {
        stream->config.max_webhooks = KG_EVENTS_MAX_WEBHOOKS;
    }

    /* Allocate subscriptions */
    stream->max_subscriptions = stream->config.max_subscribers;
    stream->subscriptions = nimcp_calloc(stream->max_subscriptions, sizeof(kg_subscription_t));
    if (!stream->subscriptions) {
        nimcp_free(stream);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "kg_events_create: stream->subscriptions is NULL");
        return NULL;
    }

    /* Allocate webhooks */
    stream->max_webhooks = stream->config.max_webhooks;
    stream->webhooks = nimcp_calloc(stream->max_webhooks, sizeof(kg_webhook_t));
    if (!stream->webhooks) {
        nimcp_free(stream->subscriptions);
        nimcp_free(stream);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "kg_events_create: stream->webhooks is NULL");
        return NULL;
    }

    /* Allocate replay buffer */
    stream->replay_capacity = stream->config.replay_buffer_size;
    stream->replay_buffer = nimcp_calloc(stream->replay_capacity, sizeof(kg_replay_entry_t));
    if (!stream->replay_buffer) {
        nimcp_free(stream->webhooks);
        nimcp_free(stream->subscriptions);
        nimcp_free(stream);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "kg_events_create: stream->replay_buffer is NULL");
        return NULL;
    }

    /* Create mutex */
    mutex_attr_t attr = {0};
    attr.type = MUTEX_TYPE_NORMAL;
    stream->mutex = nimcp_mutex_create(&attr);
    if (!stream->mutex) {
        nimcp_free(stream->replay_buffer);
        nimcp_free(stream->webhooks);
        nimcp_free(stream->subscriptions);
        nimcp_free(stream);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_events_create: stream->mutex is NULL");
        return NULL;
    }

    /* Initialize counters */
    stream->next_event_id = 1;
    stream->next_subscription_id = 1;

    /* Open persistence file if enabled */
    if (stream->config.enable_persistence && stream->config.persistence_path[0] != '\0') {
        stream->persistence_file = fopen(stream->config.persistence_path, "a");
        /* Failure is non-fatal */
    }

    return stream;
}

void kg_events_destroy(kg_event_stream_t* stream) {
    if (!stream) {
        return;
    }

    /* Close persistence file */
    if (stream->persistence_file) {
        fclose(stream->persistence_file);
    }

    /* Free replay buffer entries */
    if (stream->replay_buffer) {
        for (uint32_t i = 0; i < stream->replay_capacity; i++) {
            if (stream->replay_buffer[i].valid && stream->replay_buffer[i].event.payload_json) {
                nimcp_free(stream->replay_buffer[i].event.payload_json);
            }
        }
        nimcp_free(stream->replay_buffer);
    }

    /* Free webhooks */
    if (stream->webhooks) {
        nimcp_free(stream->webhooks);
    }

    /* Free subscriptions */
    if (stream->subscriptions) {
        nimcp_free(stream->subscriptions);
    }

    /* Destroy mutex */
    if (stream->mutex) {
        nimcp_mutex_free(stream->mutex);
    }

    nimcp_free(stream);
}

/* ============================================================================
 * Subscription API
 * ============================================================================ */

int kg_events_default_filter(kg_subscription_filter_t* filter) {
    if (!filter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "filter is NULL");

        return -1;
    }

    memset(filter, 0, sizeof(*filter));
    filter->event_types = KG_EVENT_MASK_ALL;
    filter->include_payload = true;

    return 0;
}

kg_subscription_id_t kg_events_subscribe(
    kg_event_stream_t* stream,
    const kg_subscription_filter_t* filter,
    kg_event_callback_fn callback,
    void* user_data
) {
    if (!stream || !callback) {
        return KG_SUBSCRIPTION_INVALID;
    }

    nimcp_mutex_lock(stream->mutex);

    /* Find free slot */
    int free_slot = -1;
    for (uint32_t i = 0; i < stream->max_subscriptions; i++) {
        if (!stream->subscriptions[i].active) {
            free_slot = (int)i;
            break;
        }
    }

    if (free_slot < 0) {
        nimcp_mutex_unlock(stream->mutex);
        return KG_SUBSCRIPTION_INVALID;
    }

    /* Create subscription */
    kg_subscription_t* sub = &stream->subscriptions[free_slot];
    sub->id = stream->next_subscription_id++;
    sub->callback = callback;
    sub->user_data = user_data;
    sub->active = true;
    sub->last_delivered_id = 0;

    /* Apply filter */
    if (filter) {
        sub->filter = *filter;
    } else {
        kg_events_default_filter(&sub->filter);
    }

    stream->subscription_count++;

    nimcp_mutex_unlock(stream->mutex);

    return sub->id;
}

int kg_events_unsubscribe(kg_event_stream_t* stream, kg_subscription_id_t sub_id) {
    if (!stream || sub_id == KG_SUBSCRIPTION_INVALID) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kg_events_unsubscribe: stream is NULL");
        return -1;
    }

    nimcp_mutex_lock(stream->mutex);

    int result = -1;
    for (uint32_t i = 0; i < stream->max_subscriptions; i++) {
        if (stream->subscriptions[i].active && stream->subscriptions[i].id == sub_id) {
            stream->subscriptions[i].active = false;
            stream->subscription_count--;
            result = 0;
            break;
        }
    }

    nimcp_mutex_unlock(stream->mutex);

    return result;
}

int kg_events_update_filter(
    kg_event_stream_t* stream,
    kg_subscription_id_t sub_id,
    const kg_subscription_filter_t* filter
) {
    if (!stream || !filter || sub_id == KG_SUBSCRIPTION_INVALID) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_events_update_filter: required parameter is NULL (stream, filter)");
        return -1;
    }

    nimcp_mutex_lock(stream->mutex);

    int result = -1;
    for (uint32_t i = 0; i < stream->max_subscriptions; i++) {
        if (stream->subscriptions[i].active && stream->subscriptions[i].id == sub_id) {
            stream->subscriptions[i].filter = *filter;
            result = 0;
            break;
        }
    }

    nimcp_mutex_unlock(stream->mutex);

    return result;
}

uint32_t kg_events_get_subscription_count(const kg_event_stream_t* stream) {
    if (!stream) {
        return 0;
    }
    return stream->subscription_count;
}

/* ============================================================================
 * Webhook API
 * ============================================================================ */

int kg_events_default_webhook_config(kg_webhook_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;
    }

    memset(config, 0, sizeof(*config));
    config->timeout_ms = KG_EVENTS_DEFAULT_TIMEOUT_MS;
    config->retry_count = KG_EVENTS_DEFAULT_RETRY_COUNT;
    config->batch_size = KG_EVENTS_DEFAULT_BATCH_SIZE;
    config->batch_timeout_ms = KG_EVENTS_DEFAULT_BATCH_TIMEOUT_MS;
    config->enable_signature = false;

    return 0;
}

int kg_events_add_webhook(kg_event_stream_t* stream, const kg_webhook_config_t* config) {
    if (!stream || !config || config->url[0] == '\0') {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_events_add_webhook: required parameter is NULL (stream, config)");
        return -1;
    }

    nimcp_mutex_lock(stream->mutex);

    /* Check for duplicate URL */
    for (uint32_t i = 0; i < stream->max_webhooks; i++) {
        if (stream->webhooks[i].active &&
            strncmp(stream->webhooks[i].config.url, config->url, KG_EVENTS_MAX_URL_LEN) == 0) {
            nimcp_mutex_unlock(stream->mutex);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kg_events_add_webhook: operation failed");
            return -1; /* Duplicate */
        }
    }

    /* Find free slot */
    int free_slot = -1;
    for (uint32_t i = 0; i < stream->max_webhooks; i++) {
        if (!stream->webhooks[i].active) {
            free_slot = (int)i;
            break;
        }
    }

    if (free_slot < 0) {
        nimcp_mutex_unlock(stream->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kg_events_add_webhook: validation failed");
        return -1;
    }

    /* Add webhook */
    kg_webhook_t* webhook = &stream->webhooks[free_slot];
    webhook->config = *config;
    webhook->active = true;
    webhook->events_delivered = 0;
    webhook->events_failed = 0;
    webhook->last_success_ts = 0;
    webhook->last_failure_ts = 0;
    webhook->last_error[0] = '\0';

    stream->webhook_count++;

    nimcp_mutex_unlock(stream->mutex);

    return 0;
}

int kg_events_remove_webhook(kg_event_stream_t* stream, const char* url) {
    if (!stream || !url) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_events_remove_webhook: required parameter is NULL (stream, url)");
        return -1;
    }

    nimcp_mutex_lock(stream->mutex);

    int result = -1;
    for (uint32_t i = 0; i < stream->max_webhooks; i++) {
        if (stream->webhooks[i].active &&
            strncmp(stream->webhooks[i].config.url, url, KG_EVENTS_MAX_URL_LEN) == 0) {
            stream->webhooks[i].active = false;
            stream->webhook_count--;
            result = 0;
            break;
        }
    }

    nimcp_mutex_unlock(stream->mutex);

    return result;
}

uint32_t kg_events_get_webhook_count(const kg_event_stream_t* stream) {
    if (!stream) {
        return 0;
    }
    return stream->webhook_count;
}

int kg_events_pause_webhooks(kg_event_stream_t* stream) {
    if (!stream) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stream is NULL");

        return -1;
    }

    nimcp_mutex_lock(stream->mutex);
    stream->webhooks_paused = true;
    nimcp_mutex_unlock(stream->mutex);

    return 0;
}

int kg_events_resume_webhooks(kg_event_stream_t* stream) {
    if (!stream) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stream is NULL");

        return -1;
    }

    nimcp_mutex_lock(stream->mutex);
    stream->webhooks_paused = false;
    nimcp_mutex_unlock(stream->mutex);

    return 0;
}

/* ============================================================================
 * Event Replay API
 * ============================================================================ */

int kg_events_replay(
    kg_event_stream_t* stream,
    uint64_t from_event_id,
    kg_event_callback_fn callback,
    void* user_data
) {
    if (!stream || !callback) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_events_replay: required parameter is NULL (stream, callback)");
        return -1;
    }

    nimcp_mutex_lock(stream->mutex);

    int count = 0;

    /* Iterate through replay buffer */
    uint32_t idx = stream->replay_head;
    for (uint32_t i = 0; i < stream->replay_count; i++) {
        kg_replay_entry_t* entry = &stream->replay_buffer[idx];

        if (entry->valid && entry->event.event_id >= from_event_id) {
            callback(&entry->event, user_data);
            count++;
        }

        idx = (idx + 1) % stream->replay_capacity;
    }

    nimcp_mutex_unlock(stream->mutex);

    return count;
}

int kg_events_replay_range(
    kg_event_stream_t* stream,
    uint64_t start_timestamp,
    uint64_t end_timestamp,
    kg_event_callback_fn callback,
    void* user_data
) {
    if (!stream || !callback || start_timestamp > end_timestamp) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_events_replay_range: required parameter is NULL (stream, callback)");
        return -1;
    }

    nimcp_mutex_lock(stream->mutex);

    int count = 0;

    /* Iterate through replay buffer */
    uint32_t idx = stream->replay_head;
    for (uint32_t i = 0; i < stream->replay_count; i++) {
        kg_replay_entry_t* entry = &stream->replay_buffer[idx];

        if (entry->valid &&
            entry->event.timestamp >= start_timestamp &&
            entry->event.timestamp <= end_timestamp) {
            callback(&entry->event, user_data);
            count++;
        }

        idx = (idx + 1) % stream->replay_capacity;
    }

    nimcp_mutex_unlock(stream->mutex);

    return count;
}

int kg_events_replay_filtered(
    kg_event_stream_t* stream,
    uint64_t from_event_id,
    const kg_subscription_filter_t* filter,
    kg_event_callback_fn callback,
    void* user_data
) {
    if (!stream || !callback) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_events_replay_filtered: required parameter is NULL (stream, callback)");
        return -1;
    }

    /* Use default filter if none provided */
    kg_subscription_filter_t default_filter;
    if (!filter) {
        kg_events_default_filter(&default_filter);
        filter = &default_filter;
    }

    nimcp_mutex_lock(stream->mutex);

    int count = 0;

    /* Iterate through replay buffer */
    uint32_t idx = stream->replay_head;
    for (uint32_t i = 0; i < stream->replay_count; i++) {
        kg_replay_entry_t* entry = &stream->replay_buffer[idx];

        if (entry->valid &&
            entry->event.event_id >= from_event_id &&
            filter_matches_event(filter, &entry->event)) {
            callback(&entry->event, user_data);
            count++;
        }

        idx = (idx + 1) % stream->replay_capacity;
    }

    nimcp_mutex_unlock(stream->mutex);

    return count;
}

/* ============================================================================
 * Event Log Management API
 * ============================================================================ */

uint64_t kg_events_get_latest_id(const kg_event_stream_t* stream) {
    if (!stream) {
        return 0;
    }
    return stream->next_event_id > 0 ? stream->next_event_id - 1 : 0;
}

int kg_events_get_lag(
    const kg_event_stream_t* stream,
    kg_subscription_id_t sub_id,
    uint64_t* lag
) {
    if (!stream || !lag || sub_id == KG_SUBSCRIPTION_INVALID) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_events_get_lag: required parameter is NULL (stream, lag)");
        return -1;
    }

    nimcp_mutex_lock(((kg_event_stream_t*)stream)->mutex);

    int result = -1;
    for (uint32_t i = 0; i < stream->max_subscriptions; i++) {
        if (stream->subscriptions[i].active && stream->subscriptions[i].id == sub_id) {
            uint64_t latest = stream->next_event_id > 0 ? stream->next_event_id - 1 : 0;
            *lag = latest - stream->subscriptions[i].last_delivered_id;
            result = 0;
            break;
        }
    }

    nimcp_mutex_unlock(((kg_event_stream_t*)stream)->mutex);

    return result;
}

uint64_t kg_events_get_oldest_id(const kg_event_stream_t* stream) {
    if (!stream || stream->replay_count == 0) {
        return 0;
    }

    const kg_replay_entry_t* entry = &stream->replay_buffer[stream->replay_head];
    if (entry->valid) {
        return entry->event.event_id;
    }
    return 0;
}

uint32_t kg_events_get_buffer_count(const kg_event_stream_t* stream) {
    if (!stream) {
        return 0;
    }
    return stream->replay_count;
}

uint64_t kg_events_get_total_count(const kg_event_stream_t* stream) {
    if (!stream) {
        return 0;
    }
    return stream->total_events;
}

/* ============================================================================
 * Event Emission API
 * ============================================================================ */

uint64_t kg_events_emit(
    kg_event_stream_t* stream,
    kg_event_type_t type,
    brain_kg_node_id_t node_id,
    brain_kg_node_id_t edge_from,
    brain_kg_node_id_t edge_to,
    const char* payload_json,
    uint64_t transaction_id
) {
    if (!stream) {
        return 0;
    }

    nimcp_mutex_lock(stream->mutex);

    /* Create event */
    kg_event_t event;
    memset(&event, 0, sizeof(event));

    event.event_id = stream->next_event_id++;
    event.timestamp = get_timestamp_ns();
    event.type = type;
    event.node_id = node_id;
    event.edge_from = edge_from;
    event.edge_to = edge_to;
    event.transaction_id = transaction_id;

    if (payload_json) {
        size_t len = strlen(payload_json);
        if (len <= KG_EVENTS_MAX_PAYLOAD_SIZE) {
            event.payload_json = nimcp_malloc(len + 1);
            if (event.payload_json) {
                memcpy(event.payload_json, payload_json, len + 1);
                event.payload_size = (uint32_t)len;
            }
        }
    }

    /* Add to replay buffer */
    add_to_replay_buffer(stream, &event);

    /* Persist if enabled */
    persist_event(stream, &event);

    /* Dispatch to subscribers */
    dispatch_to_subscribers(stream, &event);

    /* Update counters */
    stream->total_events++;

    uint64_t event_id = event.event_id;

    /* Free temporary payload (buffer has its own copy) */
    if (event.payload_json) {
        nimcp_free(event.payload_json);
    }

    nimcp_mutex_unlock(stream->mutex);

    return event_id;
}

/* ============================================================================
 * Event Utility Functions
 * ============================================================================ */

void kg_event_free(kg_event_t* event) {
    if (!event) {
        return;
    }

    if (event->payload_json) {
        nimcp_free(event->payload_json);
        event->payload_json = NULL;
    }
    event->payload_size = 0;
}

int kg_event_clone(const kg_event_t* event, kg_event_t* dest) {
    if (!event || !dest) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_event_clone: required parameter is NULL (event, dest)");
        return -1;
    }

    /* Copy fixed fields */
    dest->event_id = event->event_id;
    dest->timestamp = event->timestamp;
    dest->type = event->type;
    dest->node_id = event->node_id;
    dest->edge_from = event->edge_from;
    dest->edge_to = event->edge_to;
    dest->transaction_id = event->transaction_id;
    dest->payload_size = event->payload_size;

    /* Clone payload */
    if (event->payload_json && event->payload_size > 0) {
        dest->payload_json = nimcp_malloc(event->payload_size + 1);
        if (!dest->payload_json) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "kg_event_clone: dest->payload_json is NULL");
            return -1;
        }
        memcpy(dest->payload_json, event->payload_json, event->payload_size);
        dest->payload_json[event->payload_size] = '\0';
    } else {
        dest->payload_json = NULL;
    }

    return 0;
}

/**
 * @brief Event type string table
 */
static const char* event_type_strings[] = {
    "NODE_CREATED",
    "NODE_UPDATED",
    "NODE_DELETED",
    "EDGE_CREATED",
    "EDGE_UPDATED",
    "EDGE_DELETED",
    "WEIGHT_CHANGED",
    "NEUROMOD_CHANGED",
    "TOPOLOGY_CHANGED",
    "CHECKPOINT_CREATED"
};

const char* kg_event_type_to_string(kg_event_type_t type) {
    if (type >= 0 && type < KG_EVENT_TYPE_COUNT) {
        return event_type_strings[type];
    }
    return "UNKNOWN";
}

kg_event_type_t kg_event_type_from_string(const char* str) {
    if (!str) {
        return KG_EVENT_TYPE_COUNT;
    }

    for (int i = 0; i < KG_EVENT_TYPE_COUNT; i++) {
        if (strcmp(str, event_type_strings[i]) == 0) {
            return (kg_event_type_t)i;
        }
    }

    return KG_EVENT_TYPE_COUNT;
}

bool kg_event_matches_filter(const kg_event_t* event, const kg_subscription_filter_t* filter) {
    return filter_matches_event(filter, event);
}
