/**
 * @file nimcp_training_event_adapter.c
 * @brief Middleware adapter for training event handling
 *
 * WHAT: Connects middleware event bus to training event handling
 * WHY:  Enable event-driven training and learning coordination
 * HOW:  Subscribe to neural events, dispatch to training modules
 *
 * @author NIMCP Development Team
 * @date 2025-11-20
 */

#include "middleware/training/nimcp_training_event_adapter.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include <string.h>
#include <inttypes.h>

/* ============================================================================
 * Constants
 * ============================================================================ */

#define MAX_EVENT_HANDLERS 32
#define MAX_EVENT_QUEUE    256

/* ============================================================================
 * Internal Structures
 * ============================================================================ */

typedef struct {
    training_event_handler_fn handler;
    void* user_data;
    training_event_type_t event_type;
    bool active;
} event_handler_entry_t;

struct training_event_adapter_struct {
    training_event_adapter_config_t config;
    event_handler_entry_t handlers[MAX_EVENT_HANDLERS];
    uint32_t num_handlers;
    training_event_data_t* event_queue[MAX_EVENT_QUEUE];
    uint32_t queue_head;
    uint32_t queue_tail;
    uint32_t queue_count;
    uint64_t events_published;
    uint64_t events_processed;
};

/* ============================================================================
 * Helper: Deep copy an event_data into a newly allocated copy
 * ============================================================================ */

static training_event_data_t* deep_copy_event(const training_event_data_t* src) {
    if (!src) {
        return NULL;
    }

    training_event_data_t* copy = nimcp_calloc(1, sizeof(training_event_data_t));
    if (!copy) {
        LOG_ERROR("Failed to allocate training event data copy");
        return NULL;
    }

    copy->type = src->type;
    copy->timestamp = src->timestamp;
    copy->strength = src->strength;
    copy->num_channels = src->num_channels;
    copy->custom_data = src->custom_data;

    if (src->num_channels > 0 && src->channel_ids) {
        copy->channel_ids = nimcp_calloc(src->num_channels, sizeof(uint32_t));
        if (!copy->channel_ids) {
            LOG_ERROR("Failed to allocate channel_ids for event copy");
            nimcp_free(copy);
            return NULL;
        }
        memcpy(copy->channel_ids, src->channel_ids,
               src->num_channels * sizeof(uint32_t));
    } else {
        copy->channel_ids = NULL;
    }

    return copy;
}

/* ============================================================================
 * Public API
 * ============================================================================ */

training_event_adapter_t training_event_adapter_create(
    const training_event_adapter_config_t* config)
{
    if (!config) {
        LOG_ERROR("NULL config passed to training_event_adapter_create");
        return NULL;
    }

    struct training_event_adapter_struct* adapter =
        nimcp_calloc(1, sizeof(struct training_event_adapter_struct));
    if (!adapter) {
        LOG_ERROR("Failed to allocate training event adapter");
        return NULL;
    }

    adapter->config = *config;
    adapter->num_handlers = 0;
    adapter->queue_head = 0;
    adapter->queue_tail = 0;
    adapter->queue_count = 0;
    adapter->events_published = 0;
    adapter->events_processed = 0;

    LOG_INFO("Training event adapter created (patterns=%d bursts=%d synchrony=%d "
             "rewards=%d errors=%d)",
             config->monitor_patterns, config->monitor_bursts,
             config->monitor_synchrony, config->monitor_rewards,
             config->monitor_errors);

    return adapter;
}

void training_event_adapter_destroy(training_event_adapter_t adapter) {
    if (!adapter) {
        return;
    }

    /* Drain remaining queued events */
    while (adapter->queue_count > 0) {
        training_event_data_t* evt = adapter->event_queue[adapter->queue_head];
        adapter->queue_head = (adapter->queue_head + 1) % MAX_EVENT_QUEUE;
        adapter->queue_count--;
        training_event_data_destroy(evt);
    }

    LOG_INFO("Training event adapter destroyed (published=%" PRIu64
             " processed=%" PRIu64 ")",
             adapter->events_published, adapter->events_processed);

    nimcp_free(adapter);
}

bool training_event_adapter_register_handler(
    training_event_adapter_t adapter,
    training_event_type_t event_type,
    training_event_handler_fn handler,
    void* user_data)
{
    if (!adapter || !handler) {
        LOG_ERROR("NULL adapter or handler in register_handler");
        return false;
    }

    if (adapter->num_handlers >= MAX_EVENT_HANDLERS) {
        LOG_ERROR("Maximum event handlers (%d) reached", MAX_EVENT_HANDLERS);
        return false;
    }

    event_handler_entry_t* entry = &adapter->handlers[adapter->num_handlers];
    entry->handler = handler;
    entry->user_data = user_data;
    entry->event_type = event_type;
    entry->active = true;
    adapter->num_handlers++;

    LOG_DEBUG("Registered training event handler for type %d (total=%u)",
              (int)event_type, adapter->num_handlers);

    return true;
}

bool training_event_adapter_publish(
    training_event_adapter_t adapter,
    const training_event_data_t* event)
{
    if (!adapter || !event) {
        LOG_ERROR("NULL adapter or event in publish");
        return false;
    }

    if (adapter->queue_count >= MAX_EVENT_QUEUE) {
        LOG_WARN("Training event queue full (%d), dropping event type %d",
                 MAX_EVENT_QUEUE, (int)event->type);
        return false;
    }

    training_event_data_t* copy = deep_copy_event(event);
    if (!copy) {
        return false;
    }

    adapter->event_queue[adapter->queue_tail] = copy;
    adapter->queue_tail = (adapter->queue_tail + 1) % MAX_EVENT_QUEUE;
    adapter->queue_count++;
    adapter->events_published++;

    return true;
}

uint32_t training_event_adapter_process_events(training_event_adapter_t adapter) {
    if (!adapter) {
        return 0;
    }

    uint32_t processed = 0;

    while (adapter->queue_count > 0) {
        training_event_data_t* evt = adapter->event_queue[adapter->queue_head];
        adapter->queue_head = (adapter->queue_head + 1) % MAX_EVENT_QUEUE;
        adapter->queue_count--;

        /* Dispatch to all matching active handlers */
        for (uint32_t i = 0; i < adapter->num_handlers; i++) {
            event_handler_entry_t* entry = &adapter->handlers[i];
            if (entry->active && entry->event_type == evt->type) {
                entry->handler(evt, entry->user_data);
            }
        }

        training_event_data_destroy(evt);
        processed++;
    }

    adapter->events_processed += processed;
    return processed;
}

training_event_adapter_config_t training_event_adapter_default_config(
    event_bus_t* event_bus)
{
    training_event_adapter_config_t config;
    memset(&config, 0, sizeof(config));

    config.event_bus = event_bus;
    config.monitor_patterns = true;
    config.monitor_bursts = true;
    config.monitor_synchrony = true;
    config.monitor_rewards = true;
    config.monitor_errors = true;
    config.pattern_threshold = 0.7f;
    config.synchrony_threshold = 0.8f;

    return config;
}

training_event_data_t* training_event_data_create(
    training_event_type_t type,
    uint32_t num_channels)
{
    training_event_data_t* event = nimcp_calloc(1, sizeof(training_event_data_t));
    if (!event) {
        LOG_ERROR("Failed to allocate training event data");
        return NULL;
    }

    event->type = type;
    event->num_channels = num_channels;
    event->custom_data = NULL;
    event->strength = 0.0f;
    event->timestamp = 0;

    if (num_channels > 0) {
        event->channel_ids = nimcp_calloc(num_channels, sizeof(uint32_t));
        if (!event->channel_ids) {
            LOG_ERROR("Failed to allocate channel_ids array (%u channels)",
                      num_channels);
            nimcp_free(event);
            return NULL;
        }
    } else {
        event->channel_ids = NULL;
    }

    return event;
}

void training_event_data_destroy(training_event_data_t* event) {
    if (!event) {
        return;
    }

    if (event->channel_ids) {
        nimcp_free(event->channel_ids);
    }

    nimcp_free(event);
}
