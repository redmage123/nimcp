#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_event_bus.c - Event Bus Implementation
//=============================================================================

#include "middleware/events/nimcp_event_bus.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/validation/nimcp_common.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/logging/nimcp_logging.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"

#define LOG_MODULE "middleware_event_bus"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(event_bus)

#include <unistd.h>
#include "security/nimcp_blood_brain_barrier.h"
#include "utils/platform/nimcp_platform_once.h"

// Global BBB security system
static bbb_system_t g_bbb_system = NULL;

// Thread-safe initialization control
static nimcp_platform_once_t g_security_init_once = NIMCP_PLATFORM_ONCE_INIT;

//=============================================================================
// Security Initialization
//=============================================================================

/**
 * @brief Internal initialization routine (called exactly once)
 *
 * WHAT: Create and configure BBB system for input validation
 * WHY: Protect against malicious external input
 * HOW: Initialize with conservative security settings
 *
 * THREAD-SAFETY: Called via nimcp_platform_once(), guaranteed single execution
 */
/* Forward declaration for atexit registration */
static void event_bus_security_cleanup(void);

static void event_bus_security_init_internal(void) {
    bbb_config_t config = bbb_default_config();
    config.strict_mode = false;  // Don't block, just log
    config.default_action = BBB_ACTION_LOG;
    config.input.validate_strings = true;
    config.input.validate_integers = true;
    config.input.max_string_length = 4096;  // Reasonable limit

    g_bbb_system = bbb_system_create(&config);
    if (!g_bbb_system) {
        LOG_ERROR("event_bus: Failed to initialize security subsystem");
    } else {
        /* MEMORY LEAK FIX: Register cleanup with atexit to prevent memory leak */
        if (atexit(event_bus_security_cleanup) != 0) {
            LOG_WARN("event_bus: Failed to register atexit cleanup handler");
        }
        LOG_INFO("event_bus: Security subsystem initialized");
    }
}

/**
 * @brief Initialize security subsystem for event_bus (thread-safe)
 *
 * WHAT: Ensure BBB system is initialized exactly once
 * WHY: Prevent race conditions on first access from multiple threads
 * HOW: Use nimcp_platform_once() for thread-safe one-time initialization
 */
static void event_bus_security_init(void) {
    nimcp_platform_once(&g_security_init_once, event_bus_security_init_internal);
}

/**
 * @brief Cleanup security subsystem
 */
static void event_bus_security_cleanup(void) {
    if (g_bbb_system) {
        bbb_system_destroy(g_bbb_system);
        g_bbb_system = NULL;
    }
    // P2 fix: Reset once flag so security can be re-initialized after cleanup
    // (important for tests that shutdown and reinit)
    g_security_init_once = (nimcp_platform_once_t)NIMCP_PLATFORM_ONCE_INIT;
}

//=============================================================================
// Internal Structure
//=============================================================================

struct event_bus_struct {
    event_queue_t queue;
    subscriber_manager_t subscribers;

    // Async delivery thread
    bool async_delivery;
    nimcp_thread_t delivery_thread;
    _Atomic bool running;  // P2 fix: Make running flag atomic for thread-safe access
    uint32_t delivery_thread_sleep_us;

    // Bio-async integration
    bio_module_context_t bio_ctx;
    bool bio_async_enabled;

    // Statistics
    uint64_t events_published;
    uint64_t events_delivered;
    uint64_t events_dropped;

    nimcp_platform_mutex_t mutex;
};

//=============================================================================
// Delivery Thread
//=============================================================================

static void* delivery_thread_fn(void* arg) {
    event_bus_t bus = (event_bus_t)arg;

    while (__atomic_load_n(&bus->running, __ATOMIC_ACQUIRE)) {
        // Process batch of events
        event_t events[NIMCP_EVENT_BATCH_SIZE];
        uint32_t count = event_queue_dequeue_batch(bus->queue, events, NIMCP_EVENT_BATCH_SIZE);

        /* P4 fix: Accumulate local batch count, update shared counter once after
         * the batch instead of locking/unlocking mutex per event. */
        uint32_t batch_delivered = 0;
        for (uint32_t i = 0; i < count; i++) {
            subscriber_dispatch_event(bus->subscribers, &events[i]);
            event_free(&events[i]);
            batch_delivered++;
        }

        if (batch_delivered > 0) {
            nimcp_platform_mutex_lock(&bus->mutex);
            bus->events_delivered += batch_delivered;
            nimcp_platform_mutex_unlock(&bus->mutex);
        }

        // Sleep if no events
        if (count == 0) {
            usleep(bus->delivery_thread_sleep_us);
        }
    }

    // P2 fix: Removed false-positive NIMCP_THROW_TO_IMMUNE on normal thread exit.
    // Worker thread exiting because bus->running was set to false is expected behavior.
    return NULL;
}

//=============================================================================
// Lifecycle
//=============================================================================

event_bus_config_t event_bus_default_config(void) {
    event_bus_config_t config = {0};
    config.queue_capacity = NIMCP_BUFFER_CAPACITY_DEFAULT;
    config.overflow_policy = OVERFLOW_POLICY_DROP_OLDEST;
    config.async_delivery = false; // Manual by default
    config.delivery_thread_sleep_us = 1000; // 1ms
    config.enable_bio_async = false;
    return config;
}

event_bus_t event_bus_create(const event_bus_config_t* config) {
    event_bus_config_t cfg = config ? *config : event_bus_default_config();

    event_bus_t bus = nimcp_calloc(1, sizeof(struct event_bus_struct));
    if (!bus) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bus is NULL");

        return NULL;

    }

    // Create queue
    event_queue_config_t queue_cfg = event_queue_default_config();
    queue_cfg.capacity = cfg.queue_capacity;
    queue_cfg.overflow_policy = cfg.overflow_policy;

    bus->queue = event_queue_create(&queue_cfg);
    if (!bus->queue) {
        nimcp_free(bus);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "event_bus_create: bus->queue is NULL");
        return NULL;
    }

    // Create subscriber manager
    bus->subscribers = subscriber_manager_create();
    if (!bus->subscribers) {
        event_queue_destroy(bus->queue);
        nimcp_free(bus);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "event_bus_create: bus->subscribers is NULL");
        return NULL;
    }

    if (nimcp_platform_mutex_init(&bus->mutex, false) != 0) {
        subscriber_manager_destroy(bus->subscribers);
        event_queue_destroy(bus->queue);
        nimcp_free(bus);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "event_bus_create: validation failed");
        return NULL;
    }

    bus->async_delivery = cfg.async_delivery;
    bus->delivery_thread_sleep_us = cfg.delivery_thread_sleep_us;

    // Bio-async registration
    bus->bio_ctx = NULL;
    bus->bio_async_enabled = false;
    if (cfg.enable_bio_async && bio_router_is_initialized()) {
        bio_module_info_t bio_info = {
            .module_id = BIO_MODULE_EVENT_BUS,
            .module_name = "event_bus",
            .inbox_capacity = NIMCP_INBOX_CAPACITY_MEDIUM,
            .user_data = bus
        };
        bus->bio_ctx = bio_router_register_module(&bio_info);
        if (bus->bio_ctx) {
            bus->bio_async_enabled = true;
            LOG_INFO(LOG_MODULE, "Bio-async integration enabled");
        } else {
            LOG_WARN(LOG_MODULE, "Bio-async registration failed");
        }
    }

    // Start async delivery thread if enabled
    if (bus->async_delivery) {
        __atomic_store_n(&bus->running, true, __ATOMIC_RELEASE);  // P2 fix: atomic store
        if (nimcp_thread_create(&bus->delivery_thread, delivery_thread_fn, bus, NULL) != NIMCP_SUCCESS) {
            if (bus->bio_async_enabled && bus->bio_ctx) {
                bio_router_unregister_module(bus->bio_ctx);
            }
            nimcp_platform_mutex_destroy(&bus->mutex);
            subscriber_manager_destroy(bus->subscribers);
            event_queue_destroy(bus->queue);
            nimcp_free(bus);
            LOG_ERROR(LOG_MODULE, "Failed to create delivery thread");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "event_bus_create: validation failed");
            return NULL;
        }
        LOG_INFO(LOG_MODULE, "Async delivery thread started");
    }

    LOG_INFO(LOG_MODULE, "Event bus created (capacity=%u, async=%d, bio_async=%d)",
             cfg.queue_capacity, cfg.async_delivery, bus->bio_async_enabled);
    return bus;
}

void event_bus_destroy(event_bus_t bus) {
    if (!bus) return;

    LOG_DEBUG(LOG_MODULE, "Destroying event bus");

    // Stop delivery thread
    if (bus->async_delivery) {
        __atomic_store_n(&bus->running, false, __ATOMIC_RELEASE);  // P2 fix: atomic store
        nimcp_thread_join(bus->delivery_thread, NULL);
        LOG_DEBUG(LOG_MODULE, "Delivery thread stopped");
    }

    // Unregister from bio-async
    if (bus->bio_async_enabled && bus->bio_ctx) {
        bio_router_unregister_module(bus->bio_ctx);
        bus->bio_ctx = NULL;
        bus->bio_async_enabled = false;
        LOG_DEBUG(LOG_MODULE, "Bio-async unregistered");
    }

    subscriber_manager_destroy(bus->subscribers);
    event_queue_destroy(bus->queue);
    nimcp_platform_mutex_destroy(&bus->mutex);
    nimcp_free(bus);
    LOG_INFO(LOG_MODULE, "Event bus destroyed");
}

//=============================================================================
// Pub/Sub
//=============================================================================

bool event_bus_publish(event_bus_t bus, const event_t* event) {
    // Process pending bio-async messages
    if (bus && bus->bio_ctx) {
        bio_router_process_inbox(bus->bio_ctx, 5);
    }

    if (!bus || !event) {
        LOG_ERROR(LOG_MODULE, "Invalid bus or event");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "event_bus_publish: required parameter is NULL (bus, event)");
        return false;
    }

    bool success = event_queue_enqueue(bus->queue, event);

    nimcp_platform_mutex_lock(&bus->mutex);
    if (success) {
        bus->events_published++;
        LOG_DEBUG(LOG_MODULE, "Event published (type=%u, total=%llu)",
                  event->header.type, (unsigned long long)bus->events_published);
    } else {
        bus->events_dropped++;
        LOG_WARN(LOG_MODULE, "Event dropped (queue full, total_dropped=%llu)",
                 (unsigned long long)bus->events_dropped);
    }
    nimcp_platform_mutex_unlock(&bus->mutex);

    return success;
}

subscription_handle_t event_bus_subscribe(event_bus_t bus,
                                          event_callback_fn callback,
                                          void* context,
                                          const subscription_config_t* config) {
    if (!bus) return SUBSCRIPTION_HANDLE_INVALID;
    return subscriber_subscribe(bus->subscribers, callback, context, config);
}

bool event_bus_unsubscribe(event_bus_t bus, subscription_handle_t handle) {
    if (!bus) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "event_bus_unsubscribe: bus is NULL");
        return false;
    }
    return subscriber_unsubscribe(bus->subscribers, handle);
}

//=============================================================================
// Manual Processing
//=============================================================================

uint32_t event_bus_process_events(event_bus_t bus, uint32_t max_events) {
    if (!bus) return 0;

    /* P2-MW-07 fix: Reduced batch from 256 to 64 to limit stack usage.
     * event_t is ~128 bytes, so 64 events = ~8KB (safe for default stacks).
     * Previously 256 events = ~32KB which risks stack overflow on constrained platforms. */
    event_t events[64];
    uint32_t batch_size = (max_events < 64) ? max_events : 64;
    uint32_t count = event_queue_dequeue_batch(bus->queue, events, batch_size);

    if (count > 0) {
        LOG_DEBUG(LOG_MODULE, "Processing %u events", count);
    }

    /* P4 fix: Batch counter update - single lock/unlock instead of per-event */
    for (uint32_t i = 0; i < count; i++) {
        subscriber_dispatch_event(bus->subscribers, &events[i]);
        event_free(&events[i]);
    }

    if (count > 0) {
        nimcp_platform_mutex_lock(&bus->mutex);
        bus->events_delivered += count;
        nimcp_platform_mutex_unlock(&bus->mutex);
    }

    return count;
}

//=============================================================================
// Statistics
//=============================================================================

bool event_bus_get_stats(event_bus_t bus, event_bus_stats_t* stats) {
    if (!bus || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "event_bus_get_stats: required parameter is NULL (bus, stats)");
        return false;
    }

    nimcp_platform_mutex_lock(&bus->mutex);
    stats->events_published = bus->events_published;
    stats->events_delivered = bus->events_delivered;
    stats->active_subscribers = subscriber_get_count(bus->subscribers);
    stats->queue_size = event_queue_size(bus->queue);

    // Get dropped count from queue stats (handles DROP_OLDEST policy correctly)
    event_queue_stats_t queue_stats;
    if (event_queue_get_stats(bus->queue, &queue_stats)) {
        stats->events_dropped = queue_stats.total_dropped;
    } else {
        stats->events_dropped = bus->events_dropped;
    }

    nimcp_platform_mutex_unlock(&bus->mutex);

    return true;
}
