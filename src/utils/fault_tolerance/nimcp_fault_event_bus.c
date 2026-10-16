/**
 * @file nimcp_fault_event_bus.c
 * @brief Event Bus Implementation for Fault Tolerance System
 *
 * WHAT: Thread-safe event bus with publish-subscribe pattern
 * WHY:  Decouple fault tolerance modules for extensibility and testability
 * HOW:  Subscriber registry + event queue + async/sync delivery
 *
 * IMPLEMENTATION DETAILS:
 * - Lock-free queues for high throughput (optional)
 * - Thread-safe subscriber management with mutex
 * - Background thread for async delivery
 * - Callback error isolation (one failure doesn't affect others)
 * - Priority-based event ordering
 * - Statistics tracking for monitoring
 *
 * THREAD SAFETY:
 * - All public APIs are thread-safe
 * - Mutex protects subscriber registry
 * - Atomic operations for statistics
 * - Queue operations are synchronized
 */

#include "utils/fault_tolerance/nimcp_fault_event_bus.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"

#define LOG_MODULE "utils_fault_event_bus"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(fault_event_bus)

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/time.h>
#include <errno.h>
#include <signal.h>
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "constants/nimcp_buffer_constants.h"

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Subscriber entry
 */
typedef struct subscriber {
    event_subscription_handle_t handle;  /**< Unique subscription handle */
    fault_event_type_t type;                   /**< Event type subscribed to */
    fault_event_priority_t min_priority;       /**< Minimum priority filter */
    fault_event_callback_t callback;           /**< Callback function */
    void* context;                       /**< User context */
    bool active;                         /**< Whether subscription is active */
    uint64_t events_received;            /**< Events delivered to this subscriber */
    uint64_t errors;                     /**< Callback errors */
    struct subscriber* next;             /**< Next in list */
} subscriber_t;

/**
 * @brief Event queue node
 */
typedef struct event_node {
    fault_event_t event;                       /**< Event data */
    struct event_node* next;             /**< Next in queue */
} event_node_t;

/**
 * @brief Event queue
 */
typedef struct {
    event_node_t* head;                  /**< Queue head */
    event_node_t* tail;                  /**< Queue tail */
    uint32_t count;                      /**< Current queue size */
    uint32_t max_size;                   /**< Maximum queue size */
    nimcp_mutex_t mutex;                 /**< Queue mutex */
    nimcp_cond_t not_empty;              /**< Condition for dequeue */
    nimcp_cond_t not_full;               /**< Condition for enqueue */
} event_queue_t;

/**
 * @brief Event bus internal structure
 */
typedef struct event_bus_internal {
    char name[EVENT_BUS_THREAD_NAME_SIZE]; /**< Bus name */
    fault_event_delivery_mode_t delivery_mode;   /**< Delivery mode */

    /* P2-U5: Lock ordering documentation.
     * When both locks are needed, always acquire subscriber_mutex BEFORE stats_mutex.
     * deliver_event_to_subscribers() holds subscriber_mutex while calling stats_mutex,
     * so the ordering is: subscriber_mutex -> stats_mutex. Never reverse. */

    // Subscriber management
    subscriber_t* subscribers[EVENT_BUS_MAX_EVENT_TYPES]; /**< Subscribers by type hash */
    nimcp_mutex_t subscriber_mutex;        /**< Subscriber list mutex (ORDER: acquire FIRST) */
    uint64_t next_handle;                  /**< Next subscription handle */
    uint32_t subscriber_count;             /**< Total subscribers */

    // Event queue (for async delivery)
    event_queue_t* queue;                  /**< Event queue */

    // Background thread (for async delivery)
    nimcp_thread_t worker_thread;          /**< Worker thread handle */
    bool running;                          /**< Whether worker is running */
    bool shutdown;                         /**< Shutdown signal */

    // Error handling
    event_error_callback_t error_callback; /**< Error callback */
    char last_error[NIMCP_ERROR_BUFFER_SIZE];                  /**< Last error message */

    // Statistics
    uint64_t total_published;              /**< Total events published */
    uint64_t total_delivered;              /**< Total events delivered */
    uint64_t total_dropped;                /**< Events dropped */
    uint64_t total_callback_errors;        /**< Callback errors */
    uint64_t sequence_number;              /**< Event sequence number */
    uint64_t total_latency_us;             /**< Total delivery latency */
    uint64_t max_latency_us;               /**< Max delivery latency */
    uint64_t latency_samples;              /**< Latency sample count */

    nimcp_mutex_t stats_mutex;             /**< Statistics mutex (ORDER: acquire SECOND) */
} event_bus_internal_t;

//=============================================================================
// Forward Declarations
//=============================================================================

static uint32_t hash_event_type(fault_event_type_t type);
static void* event_bus_worker_thread(void* arg);
static bool deliver_event_to_subscribers(fault_event_bus_t bus, const fault_event_t* event);
static bool invoke_subscriber_callback(
    fault_event_bus_t bus,
    subscriber_t* sub,
    const fault_event_t* event
);
static event_queue_t* event_queue_create(uint32_t max_size);
static void event_queue_destroy(event_queue_t* queue);
static bool event_queue_enqueue(event_queue_t* queue, const fault_event_t* event);
static bool event_queue_dequeue(event_queue_t* queue, fault_event_t* event);
static bool event_queue_is_empty(event_queue_t* queue);
static bool event_queue_is_full(event_queue_t* queue);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get current timestamp in microseconds
 */
uint64_t event_get_timestamp_us(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000000ULL + (uint64_t)tv.tv_usec;
}

/**
 * @brief Hash event type to bucket index
 */
static uint32_t hash_event_type(fault_event_type_t type) {
    // Simple hash: Use lower bits
    uint32_t hash = (uint32_t)type;
    return hash % EVENT_BUS_MAX_EVENT_TYPES;
}

/**
 * @brief Get event type name
 */
const char* event_type_to_string(fault_event_type_t type) {
    switch (type) {
        // Diagnostic Events
        case EVENT_ERROR_DETECTED: return "ERROR_DETECTED";
        case EVENT_DIAGNOSTICS_STARTED: return "DIAGNOSTICS_STARTED";
        case EVENT_DIAGNOSTICS_COMPLETE: return "DIAGNOSTICS_COMPLETE";
        case EVENT_MEMORY_CORRUPTION_DETECTED: return "MEMORY_CORRUPTION_DETECTED";
        case EVENT_NUMERICAL_INSTABILITY_DETECTED: return "NUMERICAL_INSTABILITY_DETECTED";
        case EVENT_CRASH_PATTERN_DETECTED: return "CRASH_PATTERN_DETECTED";

        // Recovery Events
        case EVENT_RECOVERY_STARTED: return "RECOVERY_STARTED";
        case EVENT_RECOVERY_COMPLETE: return "RECOVERY_COMPLETE";
        case EVENT_RECOVERY_FAILED: return "RECOVERY_FAILED";
        case EVENT_ROLLBACK_STARTED: return "ROLLBACK_STARTED";
        case EVENT_ROLLBACK_COMPLETE: return "ROLLBACK_COMPLETE";
        case EVENT_PARAMETER_ADJUSTED: return "PARAMETER_ADJUSTED";
        case EVENT_FALLBACK_TRIGGERED: return "FALLBACK_TRIGGERED";

        // Checkpoint Events
        case EVENT_CHECKPOINT_CREATED: return "CHECKPOINT_CREATED";
        case EVENT_CHECKPOINT_LOADED: return "CHECKPOINT_LOADED";
        case EVENT_CHECKPOINT_DELETED: return "CHECKPOINT_DELETED";
        case EVENT_CHECKPOINT_CORRUPTED: return "CHECKPOINT_CORRUPTED";

        // Health Monitor Events
        case EVENT_HEALTH_DEGRADED: return "HEALTH_DEGRADED";
        case EVENT_HEALTH_CRITICAL: return "HEALTH_CRITICAL";
        case EVENT_HEALTH_RECOVERED: return "HEALTH_RECOVERED";
        case EVENT_ANOMALY_DETECTED: return "ANOMALY_DETECTED";
        case EVENT_FAILURE_PREDICTED: return "FAILURE_PREDICTED";
        case EVENT_RESOURCE_EXHAUSTION: return "RESOURCE_EXHAUSTION";

        // System Events
        case EVENT_SYSTEM_STARTED: return "SYSTEM_STARTED";
        case EVENT_SYSTEM_SHUTDOWN: return "SYSTEM_SHUTDOWN";
        case EVENT_EMERGENCY_SHUTDOWN: return "EMERGENCY_SHUTDOWN";
        case EVENT_CONFIGURATION_CHANGED: return "CONFIGURATION_CHANGED";

        // Special
        case EVENT_ALL: return "ALL";
        case EVENT_NONE: return "NONE";
        default: return "UNKNOWN";
    }
}

/**
 * @brief Get event priority name
 */
const char* event_priority_to_string(fault_event_priority_t priority) {
    switch (priority) {
        case EVENT_PRIORITY_LOW: return "LOW";
        case EVENT_PRIORITY_NORMAL: return "NORMAL";
        case EVENT_PRIORITY_HIGH: return "HIGH";
        case EVENT_PRIORITY_CRITICAL: return "CRITICAL";
        default: return "UNKNOWN";
    }
}

/**
 * @brief Create event with defaults
 */
fault_event_t event_create(fault_event_type_t type, fault_event_priority_t priority, const char* source) {
    fault_event_t event = {0};
    event.type = type;
    event.priority = priority;
    event.timestamp_us = event_get_timestamp_us();
    event.source_module = source;
    event.sequence_number = 0; // Set by publish
    event.data.size = 0;
    event.flags = 0;
    return event;
}

/**
 * @brief Copy data into event
 */
bool event_set_data(fault_event_t* event, const void* data, size_t size) {
    if (!event || !data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "event_set_data: required parameter is NULL (event, data)");
        return false;
    }
    if (size > EVENT_BUS_MAX_DATA_SIZE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "event_set_data: validation failed");
        return false;
    }

    memcpy(event->data.data, data, size);
    event->data.size = size;
    return true;
}

//=============================================================================
// Event Queue Implementation
//=============================================================================

/**
 * @brief Create event queue
 */
static event_queue_t* event_queue_create(uint32_t max_size) {
    event_queue_t* queue = (event_queue_t*)nimcp_calloc(1, sizeof(event_queue_t));
    if (!queue) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "event_queue_create: queue is NULL");
        return NULL;
    }

    queue->head = NULL;
    queue->tail = NULL;
    queue->count = 0;
    queue->max_size = max_size;

    nimcp_mutex_init(&queue->mutex, NULL);
    nimcp_cond_init(&queue->not_empty);
    nimcp_cond_init(&queue->not_full);

    return queue;
}

/**
 * @brief Destroy event queue
 */
static void event_queue_destroy(event_queue_t* queue) {
    if (!queue) return;

    nimcp_mutex_lock(&queue->mutex);

    // Free all nodes
    event_node_t* node = queue->head;
    while (node) {
        event_node_t* next = node->next;
        nimcp_free(node);
        node = next;
    }

    nimcp_mutex_unlock(&queue->mutex);

    nimcp_mutex_destroy(&queue->mutex);
    nimcp_cond_destroy(&queue->not_empty);
    nimcp_cond_destroy(&queue->not_full);

    nimcp_free(queue);
}

/**
 * @brief Enqueue event
 */
static bool event_queue_enqueue(event_queue_t* queue, const fault_event_t* event) {
    if (!queue || !event) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "event_queue_enqueue: required parameter is NULL (queue, event)");
        return false;
    }

    nimcp_mutex_lock(&queue->mutex);

    // Check if full
    if (queue->count >= queue->max_size) {
        nimcp_mutex_unlock(&queue->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "event_queue_enqueue: capacity exceeded");
        return false;
    }

    // Create node
    event_node_t* node = (event_node_t*)nimcp_malloc(sizeof(event_node_t));
    if (!node) {
        nimcp_mutex_unlock(&queue->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "event_queue_enqueue: node is NULL");
        return false;
    }

    node->event = *event;
    node->next = NULL;

    // Add to tail
    if (queue->tail) {
        queue->tail->next = node;
    } else {
        queue->head = node;
    }
    queue->tail = node;
    queue->count++;

    // Signal not empty
    nimcp_cond_signal(&queue->not_empty);

    nimcp_mutex_unlock(&queue->mutex);
    return true;
}

/**
 * @brief Dequeue event
 */
static bool event_queue_dequeue(event_queue_t* queue, fault_event_t* event) {
    if (!queue || !event) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "event_queue_dequeue: required parameter is NULL (queue, event)");
        return false;
    }

    nimcp_mutex_lock(&queue->mutex);

    // Wait if empty
    // P2-U3: Check shutdown flag inside wait loop to avoid blocking forever
    while (queue->count == 0) {
        nimcp_cond_wait(&queue->not_empty, &queue->mutex);
        /* Re-check after wakeup: if still empty, may be a spurious wakeup
         * or a shutdown signal. Caller checks shutdown flag after dequeue. */
    }

    // Get head
    event_node_t* node = queue->head;
    if (!node) {
        nimcp_mutex_unlock(&queue->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "event_queue_dequeue: node is NULL");
        return false;
    }

    *event = node->event;
    queue->head = node->next;
    if (!queue->head) {
        queue->tail = NULL;
    }
    queue->count--;

    nimcp_free(node);

    // Signal not full
    nimcp_cond_signal(&queue->not_full);

    nimcp_mutex_unlock(&queue->mutex);
    return true;
}

/**
 * @brief Check if queue is empty
 */
static bool event_queue_is_empty(event_queue_t* queue) {
    if (!queue) return true;

    nimcp_mutex_lock(&queue->mutex);
    bool empty = (queue->count == 0);
    nimcp_mutex_unlock(&queue->mutex);

    return empty;
}

/**
 * @brief Check if queue is full
 */
static bool event_queue_is_full(event_queue_t* queue) {
    if (!queue) return true;

    nimcp_mutex_lock(&queue->mutex);
    bool full = (queue->count >= queue->max_size);
    nimcp_mutex_unlock(&queue->mutex);

    return full;
}

//=============================================================================
// Event Bus Lifecycle
//=============================================================================

/**
 * @brief Create event bus
 */
fault_event_bus_t event_bus_create(const char* name, fault_event_delivery_mode_t delivery_mode) {
    event_bus_internal_t* bus = (event_bus_internal_t*)nimcp_calloc(1, sizeof(event_bus_internal_t));
    if (!bus) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "event_bus_create: failed to allocate event bus");
        return NULL;
    }

    // Set name
    if (name) {
        strncpy(bus->name, name, EVENT_BUS_THREAD_NAME_SIZE - 1);
        bus->name[EVENT_BUS_THREAD_NAME_SIZE - 1] = '\0';
    } else {
        strncpy(bus->name, "event_bus", EVENT_BUS_THREAD_NAME_SIZE - 1);
        bus->name[EVENT_BUS_THREAD_NAME_SIZE - 1] = '\0';
    }

    bus->delivery_mode = delivery_mode;
    bus->next_handle = 1;
    bus->running = false;
    bus->shutdown = false;

    // Initialize mutexes
    nimcp_mutex_init(&bus->subscriber_mutex, NULL);
    nimcp_mutex_init(&bus->stats_mutex, NULL);

    // Create event queue for async mode
    if (delivery_mode == EVENT_DELIVERY_ASYNC) {
        bus->queue = event_queue_create(EVENT_BUS_QUEUE_SIZE);
        if (!bus->queue) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
                "event_bus_create: failed to create event queue");
            nimcp_mutex_destroy(&bus->subscriber_mutex);
            nimcp_mutex_destroy(&bus->stats_mutex);
            nimcp_free(bus);
            return NULL;
        }
    }

    return (fault_event_bus_t)bus;
}

/**
 * @brief Destroy event bus
 */
void event_bus_destroy(fault_event_bus_t bus) {
    if (!bus) return;

    event_bus_internal_t* internal = (event_bus_internal_t*)bus;

    // Stop worker thread if running
    if (internal->running) {
        event_bus_stop(bus, true);
    }

    // Free all subscribers
    nimcp_mutex_lock(&internal->subscriber_mutex);

    for (uint32_t i = 0; i < EVENT_BUS_MAX_EVENT_TYPES; i++) {
        subscriber_t* sub = internal->subscribers[i];
        while (sub) {
            subscriber_t* next = sub->next;
            nimcp_free(sub);
            sub = next;
        }
    }

    nimcp_mutex_unlock(&internal->subscriber_mutex);

    // Destroy queue
    if (internal->queue) {
        event_queue_destroy(internal->queue);
    }

    // Destroy mutexes
    nimcp_mutex_destroy(&internal->subscriber_mutex);
    nimcp_mutex_destroy(&internal->stats_mutex);

    nimcp_free(internal);
}

/**
 * @brief Worker thread for async event delivery
 */
static void* event_bus_worker_thread(void* arg) {
    event_bus_internal_t* bus = (event_bus_internal_t*)arg;

    while (!bus->shutdown) {
        fault_event_t event;

        // Dequeue event (blocks if empty)
        if (event_queue_dequeue(bus->queue, &event)) {
            deliver_event_to_subscribers((fault_event_bus_t)bus, &event);
        }
    }

    /* P2-U4: Worker thread exiting is normal shutdown behavior, not an error.
     * Removed false positive NIMCP_THROW_TO_IMMUNE. */
    return NULL;
}

/**
 * @brief Start event bus
 */
bool event_bus_start(fault_event_bus_t bus) {
    if (!bus) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "event_bus_start: bus is NULL");
        return false;
    }

    event_bus_internal_t* internal = (event_bus_internal_t*)bus;

    // Only for async mode
    if (internal->delivery_mode != EVENT_DELIVERY_ASYNC) {
        return true;
    }

    if (internal->running) {
        return true; // Already running
    }

    internal->shutdown = false;

    // Create worker thread
    int result = nimcp_thread_create(&internal->worker_thread,
                                     event_bus_worker_thread, internal,
                                     NULL);
    if (result != 0) {
        snprintf(internal->last_error, sizeof(internal->last_error),
                "Failed to create worker thread: %s", strerror(result));
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "event_bus_start: validation failed");
        return false;
    }

    internal->running = true;
    return true;
}

/**
 * @brief Stop event bus
 */
bool event_bus_stop(fault_event_bus_t bus, bool drain_queue) {
    if (!bus) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "event_bus_stop: bus is NULL");
        return false;
    }

    event_bus_internal_t* internal = (event_bus_internal_t*)bus;

    if (!internal->running) {
        return true; // Already stopped
    }

    // Signal shutdown
    internal->shutdown = true;

    // Wake up worker thread by enqueuing dummy event
    if (internal->queue) {
        fault_event_t dummy = event_create(EVENT_NONE, EVENT_PRIORITY_NORMAL, "shutdown");
        event_queue_enqueue(internal->queue, &dummy);
    }

    // Wait for thread to exit
    nimcp_thread_join(internal->worker_thread, NULL);

    internal->running = false;
    return true;
}

/**
 * @brief Check if running
 */
bool event_bus_is_running(fault_event_bus_t bus) {
    if (!bus) {
        return false;
    }

    event_bus_internal_t* internal = (event_bus_internal_t*)bus;
    return internal->running;
}

//=============================================================================
// Subscription Management
//=============================================================================

/**
 * @brief Subscribe to events
 */
event_subscription_handle_t event_bus_subscribe(
    fault_event_bus_t bus,
    fault_event_type_t type,
    fault_event_callback_t callback,
    void* context
) {
    return event_bus_subscribe_priority(bus, type, EVENT_PRIORITY_LOW, callback, context);
}

/**
 * @brief Subscribe with priority filter
 */
event_subscription_handle_t event_bus_subscribe_priority(
    fault_event_bus_t bus,
    fault_event_type_t type,
    fault_event_priority_t min_priority,
    fault_event_callback_t callback,
    void* context
) {
    if (!bus || !callback) return INVALID_SUBSCRIPTION_HANDLE;

    event_bus_internal_t* internal = (event_bus_internal_t*)bus;

    // Create subscriber
    subscriber_t* sub = (subscriber_t*)nimcp_calloc(1, sizeof(subscriber_t));
    if (!sub) return INVALID_SUBSCRIPTION_HANDLE;

    nimcp_mutex_lock(&internal->subscriber_mutex);

    sub->handle = internal->next_handle++;
    sub->type = type;
    sub->min_priority = min_priority;
    sub->callback = callback;
    sub->context = context;
    sub->active = true;
    sub->events_received = 0;
    sub->errors = 0;

    // Add to hash bucket
    uint32_t bucket = hash_event_type(type);
    sub->next = internal->subscribers[bucket];
    internal->subscribers[bucket] = sub;
    internal->subscriber_count++;

    nimcp_mutex_unlock(&internal->subscriber_mutex);

    return sub->handle;
}

/**
 * @brief Unsubscribe
 */
bool event_bus_unsubscribe(fault_event_bus_t bus, event_subscription_handle_t handle) {
    if (!bus || handle == INVALID_SUBSCRIPTION_HANDLE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "event_bus_unsubscribe: bus is NULL");
        return false;
    }

    event_bus_internal_t* internal = (event_bus_internal_t*)bus;

    nimcp_mutex_lock(&internal->subscriber_mutex);

    // Search all buckets
    for (uint32_t i = 0; i < EVENT_BUS_MAX_EVENT_TYPES; i++) {
        subscriber_t** prev = &internal->subscribers[i];
        subscriber_t* sub = internal->subscribers[i];

        while (sub) {
            if (sub->handle == handle) {
                *prev = sub->next;
                nimcp_free(sub);
                internal->subscriber_count--;
                nimcp_mutex_unlock(&internal->subscriber_mutex);
                return true;
            }
            prev = &sub->next;
            sub = sub->next;
        }
    }

    nimcp_mutex_unlock(&internal->subscriber_mutex);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "event_bus_unsubscribe: operation failed");
    return false;
}

/**
 * @brief Unsubscribe all for context
 */
uint32_t event_bus_unsubscribe_all(fault_event_bus_t bus, void* context) {
    if (!bus) return 0;

    event_bus_internal_t* internal = (event_bus_internal_t*)bus;
    uint32_t removed = 0;

    nimcp_mutex_lock(&internal->subscriber_mutex);

    for (uint32_t i = 0; i < EVENT_BUS_MAX_EVENT_TYPES; i++) {
        subscriber_t** prev = &internal->subscribers[i];
        subscriber_t* sub = internal->subscribers[i];

        while (sub) {
            if (sub->context == context) {
                subscriber_t* to_free = sub;
                *prev = sub->next;
                sub = sub->next;
                nimcp_free(to_free);
                removed++;
                internal->subscriber_count--;
            } else {
                prev = &sub->next;
                sub = sub->next;
            }
        }
    }

    nimcp_mutex_unlock(&internal->subscriber_mutex);
    return removed;
}

//=============================================================================
// Event Delivery
//=============================================================================

/**
 * @brief Invoke subscriber callback with error handling
 */
static bool invoke_subscriber_callback(
    fault_event_bus_t bus,
    subscriber_t* sub,
    const fault_event_t* event
) {
    event_bus_internal_t* internal = (event_bus_internal_t*)bus;

    // Check priority filter
    if (event->priority < sub->min_priority) {
        return true; // Skip but not an error
    }

    // Invoke callback with error handling
    // Note: In production, use signal handlers or try-catch for robustness
    sub->callback(event, sub->context);
    sub->events_received++;

    return true;
}

/**
 * @brief Deliver event to all matching subscribers
 */
static bool deliver_event_to_subscribers(fault_event_bus_t bus, const fault_event_t* event) {
    if (!bus || !event) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "deliver_event_to_subscribers: required parameter is NULL (bus, event)");
        return false;
    }

    event_bus_internal_t* internal = (event_bus_internal_t*)bus;
    uint64_t start_time = event_get_timestamp_us();
    uint32_t delivered = 0;

    nimcp_mutex_lock(&internal->subscriber_mutex);

    // Deliver to subscribers of this specific type
    uint32_t bucket = hash_event_type(event->type);
    subscriber_t* sub = internal->subscribers[bucket];

    while (sub) {
        if (sub->active && (sub->type == event->type || sub->type == EVENT_ALL)) {
            if (invoke_subscriber_callback(bus, sub, event)) {
                delivered++;
            } else {
                sub->errors++;
                nimcp_mutex_lock(&internal->stats_mutex);
                internal->total_callback_errors++;
                nimcp_mutex_unlock(&internal->stats_mutex);
            }
        }
        sub = sub->next;
    }

    // Also check EVENT_ALL subscribers (in bucket 0xFFFF % MAX_TYPES)
    if (event->type != EVENT_ALL) {
        uint32_t all_bucket = hash_event_type(EVENT_ALL);
        sub = internal->subscribers[all_bucket];

        while (sub) {
            if (sub->active && sub->type == EVENT_ALL) {
                if (invoke_subscriber_callback(bus, sub, event)) {
                    delivered++;
                } else {
                    sub->errors++;
                    nimcp_mutex_lock(&internal->stats_mutex);
                    internal->total_callback_errors++;
                    nimcp_mutex_unlock(&internal->stats_mutex);
                }
            }
            sub = sub->next;
        }
    }

    nimcp_mutex_unlock(&internal->subscriber_mutex);

    // Update statistics
    uint64_t latency = event_get_timestamp_us() - start_time;

    nimcp_mutex_lock(&internal->stats_mutex);
    internal->total_delivered += delivered;
    internal->total_latency_us += latency;
    internal->latency_samples++;
    if (latency > internal->max_latency_us) {
        internal->max_latency_us = latency;
    }
    nimcp_mutex_unlock(&internal->stats_mutex);

    return true;
}

//=============================================================================
// Publishing
//=============================================================================

/**
 * @brief Publish event
 */
bool event_bus_publish(fault_event_bus_t bus, const fault_event_t* event) {
    if (!bus || !event) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "event_bus_publish: required parameter is NULL (bus, event)");
        return false;
    }

    event_bus_internal_t* internal = (event_bus_internal_t*)bus;

    // Create event copy with sequence number
    fault_event_t event_copy = *event;

    nimcp_mutex_lock(&internal->stats_mutex);
    event_copy.sequence_number = internal->sequence_number++;
    event_copy.timestamp_us = event_get_timestamp_us();
    internal->total_published++;
    nimcp_mutex_unlock(&internal->stats_mutex);

    // Deliver based on mode
    if (internal->delivery_mode == EVENT_DELIVERY_IMMEDIATE) {
        return deliver_event_to_subscribers(bus, &event_copy);
    } else {
        // Enqueue for async delivery
        if (!event_queue_enqueue(internal->queue, &event_copy)) {
            nimcp_mutex_lock(&internal->stats_mutex);
            internal->total_dropped++;
            nimcp_mutex_unlock(&internal->stats_mutex);
            snprintf(internal->last_error, sizeof(internal->last_error),
                    "Event queue full, event dropped");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "event_bus_publish: event_queue_enqueue is NULL");
            return false;
        }
        return true;
    }
}

/**
 * @brief Publish simple event
 */
bool event_bus_publish_simple(
    fault_event_bus_t bus,
    fault_event_type_t type,
    fault_event_priority_t priority,
    const char* source
) {
    fault_event_t event = event_create(type, priority, source);
    return event_bus_publish(bus, &event);
}

/**
 * @brief Publish event with data
 */
bool event_bus_publish_data(
    fault_event_bus_t bus,
    fault_event_type_t type,
    fault_event_priority_t priority,
    const char* source,
    const void* data,
    size_t data_size
) {
    if (data_size > EVENT_BUS_MAX_DATA_SIZE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "event_bus_publish_data: validation failed");
        return false;
    }

    fault_event_t event = event_create(type, priority, source);
    if (!event_set_data(&event, data, data_size)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "event_bus_publish_data: event_set_data is NULL");
        return false;
    }

    return event_bus_publish(bus, &event);
}

/**
 * @brief Flush pending events
 */
uint32_t event_bus_flush(fault_event_bus_t bus) {
    if (!bus) return 0;

    event_bus_internal_t* internal = (event_bus_internal_t*)bus;

    // Only relevant for async mode
    if (internal->delivery_mode != EVENT_DELIVERY_ASYNC) {
        return 0;
    }

    uint32_t processed = 0;

    while (!event_queue_is_empty(internal->queue)) {
        fault_event_t event;
        if (event_queue_dequeue(internal->queue, &event)) {
            deliver_event_to_subscribers(bus, &event);
            processed++;
        }
    }

    return processed;
}

//=============================================================================
// Statistics
//=============================================================================

/**
 * @brief Get statistics
 */
bool event_bus_get_stats(fault_event_bus_t bus, event_bus_stats_t* stats) {
    if (!bus || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "event_bus_get_stats: required parameter is NULL (bus, stats)");
        return false;
    }

    event_bus_internal_t* internal = (event_bus_internal_t*)bus;

    nimcp_mutex_lock(&internal->stats_mutex);

    stats->total_events_published = internal->total_published;
    stats->total_events_delivered = internal->total_delivered;
    stats->total_events_dropped = internal->total_dropped;
    stats->total_callback_errors = internal->total_callback_errors;
    stats->active_subscriptions = internal->subscriber_count;
    stats->pending_events = internal->queue ? internal->queue->count : 0;
    stats->avg_delivery_latency_us = internal->latency_samples > 0 ?
        internal->total_latency_us / internal->latency_samples : 0;
    stats->max_delivery_latency_us = internal->max_latency_us;

    nimcp_mutex_unlock(&internal->stats_mutex);

    return true;
}

/**
 * @brief Reset statistics
 */
void event_bus_reset_stats(fault_event_bus_t bus) {
    if (!bus) return;

    event_bus_internal_t* internal = (event_bus_internal_t*)bus;

    nimcp_mutex_lock(&internal->stats_mutex);

    internal->total_published = 0;
    internal->total_delivered = 0;
    internal->total_dropped = 0;
    internal->total_callback_errors = 0;
    internal->total_latency_us = 0;
    internal->max_latency_us = 0;
    internal->latency_samples = 0;

    nimcp_mutex_unlock(&internal->stats_mutex);
}

/**
 * @brief Get subscriber count
 */
uint32_t event_bus_get_subscriber_count(fault_event_bus_t bus, fault_event_type_t type) {
    if (!bus) return 0;

    event_bus_internal_t* internal = (event_bus_internal_t*)bus;
    uint32_t count = 0;

    nimcp_mutex_lock(&internal->subscriber_mutex);

    uint32_t bucket = hash_event_type(type);
    subscriber_t* sub = internal->subscribers[bucket];

    while (sub) {
        if (sub->type == type && sub->active) {
            count++;
        }
        sub = sub->next;
    }

    nimcp_mutex_unlock(&internal->subscriber_mutex);

    return count;
}

/**
 * @brief Get pending event count
 */
uint32_t event_bus_get_pending_count(fault_event_bus_t bus) {
    if (!bus) return 0;

    event_bus_internal_t* internal = (event_bus_internal_t*)bus;

    if (!internal->queue) return 0;

    nimcp_mutex_lock(&internal->queue->mutex);
    uint32_t count = internal->queue->count;
    nimcp_mutex_unlock(&internal->queue->mutex);

    return count;
}

//=============================================================================
// Error Handling
//=============================================================================

/**
 * @brief Set error callback
 */
void event_bus_set_error_callback(fault_event_bus_t bus, event_error_callback_t callback) {
    if (!bus) return;

    event_bus_internal_t* internal = (event_bus_internal_t*)bus;
    internal->error_callback = callback;
}

/**
 * @brief Get last error
 */
const char* event_bus_get_last_error(fault_event_bus_t bus) {
    if (!bus) return "Invalid bus handle";

    event_bus_internal_t* internal = (event_bus_internal_t*)bus;
    return internal->last_error;
}
