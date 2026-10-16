#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_event_subscriber.c - Event Subscriber Implementation
//=============================================================================

#include "middleware/events/nimcp_event_subscriber.h"
#include "constants/nimcp_buffer_constants.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/time/nimcp_time.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"

#define LOG_MODULE "middleware_event_subscriber"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(event_subscriber)

#include <string.h>
#include <stdio.h>
#include "security/nimcp_blood_brain_barrier.h"

//=============================================================================
// Internal Structures
//=============================================================================

typedef struct subscriber_entry {
    subscription_handle_t handle;
    event_callback_fn callback;
    void* context;
    subscription_config_t config;
    bool paused;
    subscriber_stats_t stats;
    struct subscriber_entry* next;
} subscriber_entry_t;

struct subscriber_manager_struct {
    subscriber_entry_t* subscribers;
    uint32_t subscriber_count;
    subscription_handle_t next_handle;
    nimcp_platform_mutex_t mutex;
};

//=============================================================================
// Error Handling
//=============================================================================

static __thread char last_error[NIMCP_ERROR_BUFFER_SIZE] = {0};

static void set_error(const char* msg) {
    snprintf(last_error, sizeof(last_error), "%s", msg);
}

const char* subscriber_get_last_error(void) {
    return last_error;
}

//=============================================================================
// Helper Functions
//=============================================================================

/**
 * WHAT: Check if event matches subscription config
 * WHY:  Filter events before delivery
 * COMPLEXITY: O(1) average case
 */
static bool event_matches_subscription(const event_t* event,
                                        const subscription_config_t* config) {
    // Check event type filter
    if (config->num_types > 0) {
        bool type_match = false;
        for (uint32_t i = 0; i < config->num_types; i++) {
            if (event->type == config->event_types[i]) {
                type_match = true;
                break;
            }
        }
        if (!type_match) {
            return false;  /* Event type filter non-match is normal filtering */
        }
    }

    // Check source filter
    if (config->num_sources > 0) {
        bool source_match = false;
        for (uint32_t i = 0; i < config->num_sources; i++) {
            if (event->source == config->event_sources[i]) {
                source_match = true;
                break;
            }
        }
        if (!source_match) {
            return false;  /* Event source filter non-match is normal filtering */
        }
    }

    // Check custom predicate
    if (config->predicate) {
        if (!config->predicate(event, config->predicate_context)) {
            return false;  /* Predicate rejection is normal filtering */
        }
    }

    return true;
}

//=============================================================================
// Lifecycle
//=============================================================================

subscription_config_t subscriber_default_config(void) {
    subscription_config_t config = {0};
    config.priority = SUBSCRIBER_PRIORITY_NORMAL;
    return config;
}

subscriber_manager_t subscriber_manager_create(void) {
    subscriber_manager_t manager = nimcp_calloc(1, sizeof(struct subscriber_manager_struct));
    if (!manager) {
        set_error("Failed to allocate manager");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "manager is NULL");

        return NULL;
    }

    if (nimcp_platform_mutex_init(&manager->mutex, false) != 0) {
        nimcp_free(manager);
        set_error("Failed to create mutex");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "subscriber_manager_create: validation failed");
        return NULL;
    }

    manager->next_handle = 1;
    return manager;
}

void subscriber_manager_destroy(subscriber_manager_t manager) {
    if (!manager) return;

    nimcp_platform_mutex_lock(&manager->mutex);

    subscriber_entry_t* entry = manager->subscribers;
    while (entry) {
        subscriber_entry_t* next = entry->next;

        // Free config arrays if allocated
        nimcp_free(entry->config.event_types);
        nimcp_free(entry->config.event_sources);
        nimcp_free(entry);

        entry = next;
    }

    nimcp_platform_mutex_unlock(&manager->mutex);
    nimcp_platform_mutex_destroy(&manager->mutex);
    nimcp_free(manager);
}

//=============================================================================
// Subscription Management
//=============================================================================

subscription_handle_t subscriber_subscribe(subscriber_manager_t manager,
                                           event_callback_fn callback,
                                           void* context,
                                           const subscription_config_t* config) {
    if (!manager || !callback) {
        set_error("Invalid parameters");
        return SUBSCRIPTION_HANDLE_INVALID;
    }

    subscription_config_t cfg = config ? *config : subscriber_default_config();

    subscriber_entry_t* entry = nimcp_calloc(1, sizeof(subscriber_entry_t));
    if (!entry) {
        set_error("Failed to allocate entry");
        return SUBSCRIPTION_HANDLE_INVALID;
    }

    nimcp_platform_mutex_lock(&manager->mutex);

    entry->handle = manager->next_handle++;
    entry->callback = callback;
    entry->context = context;
    entry->config = cfg;

    // Deep copy config arrays
    if (cfg.num_types > 0) {
        entry->config.event_types = nimcp_malloc(cfg.num_types * sizeof(event_type_t));
        if (entry->config.event_types) {
            memcpy(entry->config.event_types, cfg.event_types,
                   cfg.num_types * sizeof(event_type_t));
        }
    }

    if (cfg.num_sources > 0) {
        entry->config.event_sources = nimcp_malloc(cfg.num_sources * sizeof(event_source_t));
        if (entry->config.event_sources) {
            memcpy(entry->config.event_sources, cfg.event_sources,
                   cfg.num_sources * sizeof(event_source_t));
        }
    }

    // Add to linked list
    entry->next = manager->subscribers;
    manager->subscribers = entry;
    manager->subscriber_count++;

    subscription_handle_t handle = entry->handle;

    nimcp_platform_mutex_unlock(&manager->mutex);
    return handle;
}

bool subscriber_unsubscribe(subscriber_manager_t manager, subscription_handle_t handle) {
    if (!manager || handle == SUBSCRIPTION_HANDLE_INVALID) {
        return false;
    }

    nimcp_platform_mutex_lock(&manager->mutex);

    subscriber_entry_t** prev_next = &manager->subscribers;
    subscriber_entry_t* entry = manager->subscribers;

    while (entry) {
        if (entry->handle == handle) {
            *prev_next = entry->next;

            nimcp_free(entry->config.event_types);
            nimcp_free(entry->config.event_sources);
            nimcp_free(entry);

            manager->subscriber_count--;

            nimcp_platform_mutex_unlock(&manager->mutex);
            return true;
        }

        prev_next = &entry->next;
        entry = entry->next;
    }

    nimcp_platform_mutex_unlock(&manager->mutex);
    return false;
}

bool subscriber_pause(subscriber_manager_t manager, subscription_handle_t handle) {
    if (!manager || handle == SUBSCRIPTION_HANDLE_INVALID) {
        return false;
    }

    nimcp_platform_mutex_lock(&manager->mutex);

    subscriber_entry_t* entry = manager->subscribers;
    while (entry) {
        if (entry->handle == handle) {
            entry->paused = true;
            nimcp_platform_mutex_unlock(&manager->mutex);
            return true;
        }
        entry = entry->next;
    }

    nimcp_platform_mutex_unlock(&manager->mutex);
    return false;
}

bool subscriber_resume(subscriber_manager_t manager, subscription_handle_t handle) {
    if (!manager || handle == SUBSCRIPTION_HANDLE_INVALID) {
        return false;
    }

    nimcp_platform_mutex_lock(&manager->mutex);

    subscriber_entry_t* entry = manager->subscribers;
    while (entry) {
        if (entry->handle == handle) {
            entry->paused = false;
            nimcp_platform_mutex_unlock(&manager->mutex);
            return true;
        }
        entry = entry->next;
    }

    nimcp_platform_mutex_unlock(&manager->mutex);
    return false;
}

//=============================================================================
// Event Dispatch
//=============================================================================

uint32_t subscriber_dispatch_event(subscriber_manager_t manager, const event_t* event) {
    if (!manager || !event) return 0;

    nimcp_platform_mutex_lock(&manager->mutex);

    uint32_t notified = 0;

    // Dispatch to all matching subscribers
    subscriber_entry_t* entry = manager->subscribers;
    while (entry) {
        if (!entry->paused && event_matches_subscription(event, &entry->config)) {
            uint64_t start = nimcp_time_get_us();

            entry->callback(event, entry->context);

            uint64_t elapsed = nimcp_time_get_us() - start;
            entry->stats.callback_invocations++;
            entry->stats.total_callback_time_us += elapsed;
            entry->stats.events_received++;

            notified++;
        } else if (!entry->paused) {
            entry->stats.events_dropped++;
        }

        entry = entry->next;
    }

    nimcp_platform_mutex_unlock(&manager->mutex);
    return notified;
}

//=============================================================================
// Statistics
//=============================================================================

bool subscriber_get_stats(subscriber_manager_t manager, subscription_handle_t handle,
                          subscriber_stats_t* stats) {
    if (!manager || handle == SUBSCRIPTION_HANDLE_INVALID || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "subscriber_dispatch_event: required parameter is NULL (manager, stats)");
        return false;
    }

    nimcp_platform_mutex_lock(&manager->mutex);

    subscriber_entry_t* entry = manager->subscribers;
    while (entry) {
        if (entry->handle == handle) {
            *stats = entry->stats;
            nimcp_platform_mutex_unlock(&manager->mutex);
            return true;
        }
        entry = entry->next;
    }

    nimcp_platform_mutex_unlock(&manager->mutex);
    return false;  /* Handle not found - normal search behavior */
}

uint32_t subscriber_get_count(subscriber_manager_t manager) {
    if (!manager) return 0;

    nimcp_platform_mutex_lock(&manager->mutex);
    uint32_t count = manager->subscriber_count;
    nimcp_platform_mutex_unlock(&manager->mutex);

    return count;
}
