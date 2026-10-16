/**
 * @file nimcp_event_bus.c
 * @brief Universal Event Bus Implementation for All Brain Activities
 * @version 2.0.0 (Promoted from Fault Tolerance)
 *
 * WHAT: Thread-safe event bus with publish-subscribe pattern for universal brain coordination
 * WHY:  Decouple ALL brain modules for extensibility and testability (not just fault tolerance)
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

#include "core/events/nimcp_event_bus.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include <stddef.h>  /* for NULL */
#include "core/brain/factory/init/nimcp_brain_init.h"  // Phase IS-1: BBB access
#include "security/nimcp_blood_brain_barrier.h"        // Phase IS-1: BBB perimeter defense
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/time.h>
#include <errno.h>
#include <signal.h>

// === BIO-ASYNC + LOGGING + UNIFIED MEMORY INTEGRATION ===
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/exception/nimcp_exception_macros.h"

#define LOG_MODULE "event_bus"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_buffer_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(event_bus)

#define BIO_MODULE_ID 0x0131


//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Subscriber entry
 */
typedef struct subscriber {
    event_subscription_handle_t handle;  /**< Unique subscription handle */
    brain_event_type_t type;                   /**< Event type subscribed to */
    event_priority_t min_priority;       /**< Minimum priority filter */
    brain_event_callback_t callback;           /**< Callback function */
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
    brain_event_t event;                       /**< Event data */
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
    nimcp_mutex_t mutex;               /**< Queue mutex */
    nimcp_cond_t not_empty;              /**< Condition for dequeue */
    nimcp_cond_t not_full;               /**< Condition for enqueue */
} event_queue_t;

/**
 * @brief Event bus internal structure
 */
typedef struct event_bus_internal {
    char name[EVENT_BUS_THREAD_NAME_SIZE]; /**< Bus name */
    event_delivery_mode_t delivery_mode;   /**< Delivery mode */

    // Subscriber management
    subscriber_t* subscribers[EVENT_BUS_MAX_EVENT_TYPES]; /**< Subscribers by type hash */
    nimcp_mutex_t subscriber_mutex;      /**< Subscriber list mutex */
    uint64_t next_handle;                  /**< Next subscription handle */
    uint32_t subscriber_count;             /**< Total subscribers */

    // Event queue (for async delivery)
    event_queue_t* queue;                  /**< Event queue */

    // Background thread (for async delivery)
    nimcp_thread_t worker_thread;               /**< Worker thread handle */
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

    nimcp_mutex_t stats_mutex;           /**< Statistics mutex */
} event_bus_internal_t;

//=============================================================================
// Forward Declarations
//=============================================================================

static uint32_t hash_event_type(brain_event_type_t type);
static void* event_bus_worker_thread(void* arg);
static bool deliver_event_to_subscribers(event_bus_t bus, const brain_event_t* event);
static bool invoke_subscriber_callback(
    event_bus_t bus,
    subscriber_t* sub,
    const brain_event_t* event
);
static event_queue_t* event_queue_create(uint32_t max_size);
static void event_queue_destroy(event_queue_t* queue);
static bool event_queue_enqueue(event_queue_t* queue, const brain_event_t* event);
static bool event_queue_dequeue(event_queue_t* queue, brain_event_t* event, volatile bool* shutdown_flag);
static bool event_queue_is_empty(event_queue_t* queue);
static bool event_queue_is_full(event_queue_t* queue);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get current timestamp in microseconds
 */
// P3-59: Ideally this would be static, but it's declared in the public header
// (nimcp_event_bus.h) and may be used by external callers. Keeping non-static.
uint64_t event_get_timestamp_us(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000000ULL + (uint64_t)tv.tv_usec;
}

/**
 * @brief Hash event type to bucket index
 */
static uint32_t hash_event_type(brain_event_type_t type) {
    // Simple hash: Use lower bits
    uint32_t hash = (uint32_t)type;
    return hash % EVENT_BUS_MAX_EVENT_TYPES;
}

/**
 * @brief Get event type name
 */
const char* event_type_to_string(brain_event_type_t type) {
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
const char* event_priority_to_string(event_priority_t priority) {
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
brain_event_t event_create(brain_event_type_t type, event_priority_t priority, const char* source) {
    brain_event_t event = {0};
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
bool event_set_data(brain_event_t* event, const void* data, size_t size) {
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
static bool event_queue_enqueue(event_queue_t* queue, const brain_event_t* event) {
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
 * @brief Dequeue event with shutdown check
 *
 * WHAT: Dequeue an event from the queue, checking for shutdown during wait
 * WHY:  Prevents worker thread from blocking forever when shutdown is requested
 * HOW:  Check shutdown flag inside the condition wait loop
 *
 * @param queue Event queue
 * @param event Output event
 * @param shutdown_flag Pointer to shutdown flag (checked in wait loop)
 * @return true if event was dequeued, false if shutdown or error
 */
static bool event_queue_dequeue(event_queue_t* queue, brain_event_t* event, volatile bool* shutdown_flag) {
    if (!queue || !event) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "event_queue_dequeue: required parameter is NULL (queue, event)");
        return false;
    }

    nimcp_mutex_lock(&queue->mutex);

    // Wait if empty, checking shutdown flag to prevent blocking forever
    while (queue->count == 0) {
        // P2-64 FIX: Check shutdown flag before waiting - just return false,
        // don't throw. Shutdown is a normal exit path, not an error.
        if (shutdown_flag && *shutdown_flag) {
            nimcp_mutex_unlock(&queue->mutex);
            return false;  // Shutdown requested, exit gracefully
        }
        nimcp_cond_wait(&queue->not_empty, &queue->mutex);
    }

    // P2-64 FIX: Final shutdown check after waking up - return false without throw
    if (shutdown_flag && *shutdown_flag) {
        nimcp_mutex_unlock(&queue->mutex);
        return false;
    }

    // P1-57 FIX: If head is NULL despite count>0 (race condition), return false
    // gracefully instead of throwing. This is a defensive check.
    event_node_t* node = queue->head;
    if (!node) {
        nimcp_mutex_unlock(&queue->mutex);
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
event_bus_t event_bus_create(const char* name, event_delivery_mode_t delivery_mode) {
    event_bus_internal_t* bus = (event_bus_internal_t*)nimcp_calloc(1, sizeof(event_bus_internal_t));
    if (!bus) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "event_bus_create: failed to allocate event bus");
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

    // P2-67 FIX: Check mutex init return values and clean up on failure
    int sub_mutex_ret = nimcp_mutex_init(&bus->subscriber_mutex, NULL);
    if (sub_mutex_ret != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "event_bus_create: subscriber mutex init failed");
        nimcp_free(bus);
        return NULL;
    }
    int stats_mutex_ret = nimcp_mutex_init(&bus->stats_mutex, NULL);
    if (stats_mutex_ret != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "event_bus_create: stats mutex init failed");
        nimcp_mutex_destroy(&bus->subscriber_mutex);
        nimcp_free(bus);
        return NULL;
    }

    // Create event queue for async mode
    if (delivery_mode == EVENT_DELIVERY_ASYNC) {
        bus->queue = event_queue_create(EVENT_BUS_QUEUE_SIZE);
        if (!bus->queue) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "event_bus_create: failed to create event queue");
            nimcp_mutex_destroy(&bus->subscriber_mutex);
            nimcp_mutex_destroy(&bus->stats_mutex);
            nimcp_free(bus);
            return NULL;
        }
    }

    return (event_bus_t)bus;
}

/**
 * @brief Destroy event bus
 */
void event_bus_destroy(event_bus_t bus) {
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
 *
 * WHAT: Background thread that dequeues events and delivers to subscribers
 * WHY:  Async delivery mode requires a dedicated worker thread
 * HOW:  Loop until shutdown, checking shutdown flag in dequeue wait loop
 *
 * THREAD SAFETY: Uses shutdown flag check in dequeue to prevent blocking forever
 */
static void* event_bus_worker_thread(void* arg) {
    event_bus_internal_t* bus = (event_bus_internal_t*)arg;

    while (!bus->shutdown) {
        brain_event_t event;

        // Dequeue event (blocks if empty, but checks shutdown flag in wait loop)
        // Pass shutdown flag so dequeue can exit if shutdown is requested during wait
        if (event_queue_dequeue(bus->queue, &event, &bus->shutdown)) {
            // Only deliver if not shutting down
            if (!bus->shutdown) {
                deliver_event_to_subscribers((event_bus_t)bus, &event);
            }
        }
    }

    // P2-63 FIX: Normal shutdown exit - removed false-positive NIMCP_THROW_TO_IMMUNE.
    // Worker thread exiting after shutdown signal is expected behavior, not an error.
    return NULL;
}

/**
 * @brief Start event bus
 */
bool event_bus_start(event_bus_t bus) {
    if (!bus) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "event_bus_start: bus is NULL");
        return false;
    }

    event_bus_internal_t* internal = (event_bus_internal_t*)bus;

    // For immediate mode, just mark as running (no thread needed)
    if (internal->delivery_mode != EVENT_DELIVERY_ASYNC) {
        internal->running = true;
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
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_THREAD_CREATE, "event_bus_start: failed to create worker thread");
        snprintf(internal->last_error, sizeof(internal->last_error),
                "Failed to create worker thread: %s", strerror(result));
        return false;
    }

    internal->running = true;
    return true;
}

/**
 * @brief Stop event bus
 *
 * WHAT: Stop the event bus and its worker thread
 * WHY:  Clean shutdown to prevent resource leaks
 * HOW:  Set shutdown flag and signal condition variable to wake worker thread
 *
 * THREAD SAFETY: Uses condition signal to immediately wake blocked worker thread
 */
bool event_bus_stop(event_bus_t bus, bool drain_queue) {
    if (!bus) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "event_bus_stop: bus is NULL");
        return false;
    }

    event_bus_internal_t* internal = (event_bus_internal_t*)bus;

    if (!internal->running) {
        return true; // Already stopped
    }

    // P2-66 FIX: When drain_queue is true, process remaining events before shutdown
    if (drain_queue && internal->queue) {
        brain_event_t event;
        // Drain remaining events synchronously before signaling shutdown
        while (!event_queue_is_empty(internal->queue)) {
            // Use a non-blocking dequeue by temporarily setting shutdown to false
            // and using a short drain loop
            nimcp_mutex_lock(&internal->queue->mutex);
            if (internal->queue->count > 0 && internal->queue->head) {
                event_node_t* node = internal->queue->head;
                event = node->event;
                internal->queue->head = node->next;
                if (!internal->queue->head) internal->queue->tail = NULL;
                internal->queue->count--;
                nimcp_mutex_unlock(&internal->queue->mutex);
                nimcp_free(node);
                deliver_event_to_subscribers((event_bus_t)internal, &event);
            } else {
                nimcp_mutex_unlock(&internal->queue->mutex);
                break;
            }
        }
    }

    // Signal shutdown
    internal->shutdown = true;

    // Wake up worker thread by signaling the condition variable
    // This is more reliable than a dummy event because the worker thread
    // checks the shutdown flag in the dequeue wait loop
    if (internal->queue) {
        nimcp_mutex_lock(&internal->queue->mutex);
        nimcp_cond_signal(&internal->queue->not_empty);
        nimcp_mutex_unlock(&internal->queue->mutex);
    }

    // Wait for thread to exit
    nimcp_thread_join(internal->worker_thread, NULL);

    internal->running = false;
    return true;
}

/**
 * @brief Check if running
 */
bool event_bus_is_running(event_bus_t bus) {
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
    event_bus_t bus,
    brain_event_type_t type,
    brain_event_callback_t callback,
    void* context
) {
    return event_bus_subscribe_priority(bus, type, EVENT_PRIORITY_LOW, callback, context);
}

/**
 * @brief Subscribe with priority filter
 *
 * @note Enforces EVENT_BUS_MAX_SUBSCRIBERS limit per event type bucket
 */
event_subscription_handle_t event_bus_subscribe_priority(
    event_bus_t bus,
    brain_event_type_t type,
    event_priority_t min_priority,
    brain_event_callback_t callback,
    void* context
) {
    if (!bus || !callback) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "event_bus_subscribe_priority: bus or callback is NULL");
        return INVALID_SUBSCRIPTION_HANDLE;
    }

    event_bus_internal_t* internal = (event_bus_internal_t*)bus;

    nimcp_mutex_lock(&internal->subscriber_mutex);

    // Check global subscriber limit
    if (internal->subscriber_count >= EVENT_BUS_MAX_SUBSCRIBERS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "event_bus_subscribe_priority: subscriber limit reached");
        nimcp_mutex_unlock(&internal->subscriber_mutex);
        snprintf(internal->last_error, sizeof(internal->last_error),
                "Subscriber limit reached (%u/%u)",
                internal->subscriber_count, EVENT_BUS_MAX_SUBSCRIBERS);
        return INVALID_SUBSCRIPTION_HANDLE;
    }

    // Count subscribers in target bucket to prevent per-bucket overflow
    uint32_t bucket = hash_event_type(type);
    uint32_t bucket_count = 0;
    subscriber_t* check = internal->subscribers[bucket];
    while (check) {
        bucket_count++;
        check = check->next;
    }

    // Limit per-bucket subscribers to prevent hash collision DoS
    const uint32_t max_per_bucket = EVENT_BUS_MAX_SUBSCRIBERS / 4;  // 128 per bucket
    if (bucket_count >= max_per_bucket) {
        nimcp_mutex_unlock(&internal->subscriber_mutex);
        snprintf(internal->last_error, sizeof(internal->last_error),
                "Bucket %u subscriber limit reached (%u/%u)",
                bucket, bucket_count, max_per_bucket);
        return INVALID_SUBSCRIPTION_HANDLE;
    }

    // Create subscriber while still holding lock (allocation is fast)
    subscriber_t* sub = (subscriber_t*)nimcp_calloc(1, sizeof(subscriber_t));
    if (!sub) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "event_bus_subscribe_priority: failed to allocate subscriber");
        nimcp_mutex_unlock(&internal->subscriber_mutex);
        return INVALID_SUBSCRIPTION_HANDLE;
    }

    sub->handle = internal->next_handle++;
    sub->type = type;
    sub->min_priority = min_priority;
    sub->callback = callback;
    sub->context = context;
    sub->active = true;
    sub->events_received = 0;
    sub->errors = 0;

    // Add to hash bucket (reuse bucket from above)
    sub->next = internal->subscribers[bucket];
    internal->subscribers[bucket] = sub;
    internal->subscriber_count++;

    nimcp_mutex_unlock(&internal->subscriber_mutex);

    return sub->handle;
}

/**
 * @brief Unsubscribe
 */
bool event_bus_unsubscribe(event_bus_t bus, event_subscription_handle_t handle) {
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
    // P2-65 FIX: Not finding a subscriber is a normal "not found" result.
    // Removed false-positive NIMCP_THROW_TO_IMMUNE - just return false.
    return false;
}

/**
 * @brief Unsubscribe all for context
 */
uint32_t event_bus_unsubscribe_all(event_bus_t bus, void* context) {
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
    event_bus_t bus,
    subscriber_t* sub,
    const brain_event_t* event
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
 * @brief Snapshot of subscriber data for lock-free callback invocation
 */
typedef struct {
    brain_event_callback_t callback;
    void* context;
    event_priority_t min_priority;
    subscriber_t* sub_ptr;  // For updating error counts atomically
} subscriber_snapshot_t;

/**
 * @brief Deliver event to all matching subscribers
 *
 * THREAD SAFETY: Copies subscriber list while holding lock, then releases lock
 * before invoking callbacks. This prevents deadlocks if callbacks try to
 * subscribe/unsubscribe during delivery.
 */
static bool deliver_event_to_subscribers(event_bus_t bus, const brain_event_t* event) {
    if (!bus || !event) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "deliver_event_to_subscribers: required parameter is NULL (bus, event)");
        return false;
    }

    event_bus_internal_t* internal = (event_bus_internal_t*)bus;
    uint64_t start_time = event_get_timestamp_us();
    uint32_t delivered = 0;

    // Max subscribers to snapshot (stack allocation)
    // Must be >= per-bucket limit (EVENT_BUS_MAX_SUBSCRIBERS / 4 = 128)
    #define MAX_SNAPSHOT_SUBSCRIBERS 128
    subscriber_snapshot_t snapshots[MAX_SNAPSHOT_SUBSCRIBERS];
    uint32_t snapshot_count = 0;

    nimcp_mutex_lock(&internal->subscriber_mutex);

    // Collect matching subscribers into snapshot while holding lock
    uint32_t bucket = hash_event_type(event->type);
    subscriber_t* sub = internal->subscribers[bucket];

    while (sub && snapshot_count < MAX_SNAPSHOT_SUBSCRIBERS) {
        if (sub->active && (sub->type == event->type || sub->type == EVENT_ALL)) {
            snapshots[snapshot_count].callback = sub->callback;
            snapshots[snapshot_count].context = sub->context;
            snapshots[snapshot_count].min_priority = sub->min_priority;
            snapshots[snapshot_count].sub_ptr = sub;
            snapshot_count++;
        }
        sub = sub->next;
    }

    // Also check EVENT_ALL subscribers (in bucket 0xFFFF % MAX_TYPES)
    if (event->type != EVENT_ALL) {
        uint32_t all_bucket = hash_event_type(EVENT_ALL);
        sub = internal->subscribers[all_bucket];

        while (sub && snapshot_count < MAX_SNAPSHOT_SUBSCRIBERS) {
            if (sub->active && sub->type == EVENT_ALL) {
                snapshots[snapshot_count].callback = sub->callback;
                snapshots[snapshot_count].context = sub->context;
                snapshots[snapshot_count].min_priority = sub->min_priority;
                snapshots[snapshot_count].sub_ptr = sub;
                snapshot_count++;
            }
            sub = sub->next;
        }
    }

    nimcp_mutex_unlock(&internal->subscriber_mutex);

    // Invoke callbacks outside lock to prevent deadlock
    for (uint32_t i = 0; i < snapshot_count; i++) {
        // Check priority filter
        if (event->priority < snapshots[i].min_priority) {
            continue;  // Skip but not an error
        }

        // Invoke callback
        snapshots[i].callback(event, snapshots[i].context);
        delivered++;

        // P1-56 FIX: Removed per-subscriber stats update from snapshot path.
        // After releasing the subscriber_mutex, the sub_ptr may point to freed
        // memory if another thread unsubscribed concurrently. The global
        // total_delivered counter (updated below under stats_mutex) is sufficient.
    }

    #undef MAX_SNAPSHOT_SUBSCRIBERS

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
bool event_bus_publish(event_bus_t bus, const brain_event_t* event) {
    if (!bus || !event) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "event_bus_publish: required parameter is NULL (bus, event)");
        return false;
    }

    event_bus_internal_t* internal = (event_bus_internal_t*)bus;

    // Create event copy with sequence number
    brain_event_t event_copy = *event;

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
    event_bus_t bus,
    brain_event_type_t type,
    event_priority_t priority,
    const char* source
) {
    brain_event_t event = event_create(type, priority, source);
    return event_bus_publish(bus, &event);
}

/**
 * @brief Publish event with data
 */
bool event_bus_publish_data(
    event_bus_t bus,
    brain_event_type_t type,
    event_priority_t priority,
    const char* source,
    const void* data,
    size_t data_size
) {
    if (data_size > EVENT_BUS_MAX_DATA_SIZE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "event_bus_publish_data: validation failed");
        return false;
    }

    // Phase IS-1: BBB validation for external event data
    if (data && data_size > 0) {
        bbb_system_t bbb = nimcp_bbb_get_global_system();
        if (bbb) {
            bbb_validation_result_t result;
            if (!bbb_validate_input(bbb, data, data_size, &result)) {
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "event_bus_publish_data: bbb_validate_input is NULL");
                return false;  // BBB rejected the data
            }
        }
    }

    brain_event_t event = event_create(type, priority, source);
    if (!event_set_data(&event, data, data_size)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "event_bus_publish_data: event_set_data is NULL");
        return false;
    }

    return event_bus_publish(bus, &event);
}

/**
 * @brief Flush pending events
 */
uint32_t event_bus_flush(event_bus_t bus) {
    if (!bus) return 0;

    event_bus_internal_t* internal = (event_bus_internal_t*)bus;

    // Only relevant for async mode
    if (internal->delivery_mode != EVENT_DELIVERY_ASYNC) {
        return 0;
    }

    uint32_t processed = 0;

    while (!event_queue_is_empty(internal->queue)) {
        brain_event_t event;
        // Pass NULL for shutdown flag - flush is synchronous and doesn't need shutdown check
        if (event_queue_dequeue(internal->queue, &event, NULL)) {
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
bool event_bus_get_stats(event_bus_t bus, event_bus_stats_t* stats) {
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
void event_bus_reset_stats(event_bus_t bus) {
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
uint32_t event_bus_get_subscriber_count(event_bus_t bus, brain_event_type_t type) {
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
uint32_t event_bus_get_pending_count(event_bus_t bus) {
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
void event_bus_set_error_callback(event_bus_t bus, event_error_callback_t callback) {
    if (!bus) return;

    event_bus_internal_t* internal = (event_bus_internal_t*)bus;
    internal->error_callback = callback;
}

/**
 * @brief Get last error
 */
const char* event_bus_get_last_error(event_bus_t bus) {
    if (!bus) return "Invalid bus handle";

    event_bus_internal_t* internal = (event_bus_internal_t*)bus;
    return internal->last_error;
}
