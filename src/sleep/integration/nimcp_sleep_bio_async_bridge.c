/**
 * @file nimcp_sleep_bio_async_bridge.c
 * @brief Implementation of Sleep-Wake System Bio-Async Integration Bridge
 * @version 1.0.0
 * @date 2026-01-13
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "sleep/integration/nimcp_sleep_bio_async_bridge.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(sleep_bio_async_bridge)

#define LOG_MODULE "SLEEP_BIO_ASYNC_BRIDGE"


/* ============================================================================
 * Internal State Cache
 * ============================================================================ */

typedef struct {
    sleep_state_t current_stage;
    float sleep_pressure;
    float adenosine_level;
    float circadian_phase;
    float dominant_frequency_hz;
} sleep_internal_state_t;

/* ============================================================================
 * Internal Bridge Structure
 * ============================================================================ */

struct sleep_bio_async_bridge_struct {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    sleep_bio_async_config_t config;
    sleep_system_t sleep_system;
    bio_router_t router;

    sleep_bio_subscription_t* subscriptions;
    uint32_t subscription_count;
    uint32_t subscription_capacity;

    bool connected;
    uint64_t last_broadcast_us;
    uint32_t time_since_broadcast_ms;

    /* Cached state */
    sleep_internal_state_t state;

    sleep_bio_async_stats_t stats;
};

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

static uint64_t sleep_get_timestamp_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

static sleep_bio_subscription_t* sleep_find_subscription(
    sleep_bio_async_bridge_t* b, uint32_t module_id
) {
    for (uint32_t i = 0; i < b->subscription_count; i++) {
        if (b->subscriptions[i].module_id == module_id && b->subscriptions[i].active) {
            return &b->subscriptions[i];
        }
    }
    return NULL;  /* Not found - normal lookup behavior */
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

int sleep_bio_async_default_config(sleep_bio_async_config_t* config) {
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;

    }

    config->state_broadcast_interval_ms = SLEEP_BIO_DEFAULT_BROADCAST_INTERVAL_MS;
    config->enable_auto_broadcast = true;
    config->max_inbox_process_per_update = 32;
    config->message_ttl_ms = SLEEP_BIO_MESSAGE_TTL_MS;
    config->default_channel = BIO_CHANNEL_SEROTONIN;
    config->consolidation_channel = BIO_CHANNEL_ACETYLCHOLINE;
    config->max_subscriptions = SLEEP_BIO_MAX_SUBSCRIPTIONS;
    config->enable_consolidation_routing = true;
    config->enable_replay_routing = true;
    config->enable_homeostasis_routing = true;
    config->enable_oscillation_routing = true;
    config->enable_logging = false;

    return 0;
}

sleep_bio_async_bridge_t* sleep_bio_async_bridge_create(
    const sleep_bio_async_config_t* config
) {
    sleep_bio_async_bridge_t* bridge = nimcp_calloc(1, sizeof(sleep_bio_async_bridge_t));
    NIMCP_API_CHECK_ALLOC(bridge, "sleep_bio_async_bridge_create: Failed to allocate bridge");

    if (config) {
        bridge->config = *config;
    } else {
        sleep_bio_async_default_config(&bridge->config);
    }

    bridge->subscription_capacity = bridge->config.max_subscriptions;
    bridge->subscriptions = nimcp_calloc(
        bridge->subscription_capacity, sizeof(sleep_bio_subscription_t)
    );
    if (!bridge->subscriptions) {
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "sleep_bio_async_bridge_create: bridge->subscriptions is NULL");
        return NULL;
    }

    bridge->last_broadcast_us = sleep_get_timestamp_us();
    bridge->state.current_stage = SLEEP_STATE_AWAKE;

    return bridge;
}

void sleep_bio_async_bridge_destroy(sleep_bio_async_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "sleep_bio_async");

    if (bridge->connected) {
        sleep_bio_async_disconnect(bridge);
    }

    nimcp_free(bridge->subscriptions);
    nimcp_free(bridge);
}

/* ============================================================================
 * Connection API
 * ============================================================================ */

int sleep_bio_async_connect(
    sleep_bio_async_bridge_t* bridge,
    sleep_system_t sleep_system,
    bio_router_t router
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    bridge->sleep_system = sleep_system;
    bridge->router = router;
    bridge->connected = true;

    return 0;
}

int sleep_bio_async_disconnect(sleep_bio_async_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    bridge->sleep_system = NULL;
    bridge->router = NULL;
    bridge->connected = false;

    return 0;
}

bool sleep_bio_async_is_connected(const sleep_bio_async_bridge_t* bridge) {
    return bridge ? bridge->connected : false;
}

/* ============================================================================
 * Message Processing API
 * ============================================================================ */

int sleep_bio_async_process_inbox(
    sleep_bio_async_bridge_t* bridge,
    uint32_t max_messages
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sleep_bio_async_process_inbox: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    uint32_t processed = 0;
    (void)max_messages;

    bridge->stats.messages_received += processed;
    return (int)processed;
}

int sleep_bio_async_update(
    sleep_bio_async_bridge_t* bridge,
    uint32_t delta_ms
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sleep_bio_async_update: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    bridge->time_since_broadcast_ms += delta_ms;

    if (bridge->config.enable_auto_broadcast &&
        bridge->time_since_broadcast_ms >= bridge->config.state_broadcast_interval_ms) {
        /* Broadcast current adenosine level as periodic update */
        sleep_bio_async_broadcast_adenosine(
            bridge,
            bridge->state.adenosine_level,
            bridge->state.sleep_pressure
        );
        bridge->time_since_broadcast_ms = 0;
    }

    return 0;
}

/* ============================================================================
 * Broadcast API
 * ============================================================================ */

int sleep_bio_async_broadcast_stage_transition(
    sleep_bio_async_bridge_t* bridge,
    sleep_state_t previous_stage,
    sleep_state_t current_stage
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sleep_bio_async_broadcast_stage_transition: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    sleep_bio_stage_transition_msg_t msg = {0};
    msg.header.type = BIO_MSG_SLEEP_STAGE_CHANGE;
    msg.header.source_module = BIO_MODULE_CONSOLIDATION_SLEEP;
    msg.header.channel = bridge->config.default_channel;
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;
    msg.header.timestamp_us = sleep_get_timestamp_us();

    msg.previous_stage = previous_stage;
    msg.current_stage = current_stage;
    msg.sleep_pressure = bridge->state.sleep_pressure;
    msg.adenosine_level = bridge->state.adenosine_level;
    msg.circadian_phase = bridge->state.circadian_phase;
    msg.timestamp_us = msg.header.timestamp_us;

    bridge->state.current_stage = current_stage;
    bridge->stats.stage_transitions_sent++;
    bridge->stats.broadcasts_sent++;
    bridge->last_broadcast_us = msg.timestamp_us;

    return 0;
}

int sleep_bio_async_broadcast_consolidation_start(
    sleep_bio_async_bridge_t* bridge,
    sleep_state_t during_stage
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sleep_bio_async_broadcast_consolidation_start: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }
    if (!bridge->config.enable_consolidation_routing) return 0;

    sleep_bio_consolidation_msg_t msg = {0};
    msg.header.type = BIO_MSG_CONSOLIDATION_START;
    msg.header.source_module = BIO_MODULE_CONSOLIDATION_SLEEP;
    msg.header.channel = bridge->config.consolidation_channel;
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;
    msg.header.timestamp_us = sleep_get_timestamp_us();

    msg.during_stage = during_stage;
    msg.timestamp_us = msg.header.timestamp_us;

    bridge->stats.consolidation_events_sent++;
    bridge->stats.broadcasts_sent++;

    return 0;
}

int sleep_bio_async_broadcast_consolidation_complete(
    sleep_bio_async_bridge_t* bridge,
    uint32_t memories_replayed,
    float efficiency
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sleep_bio_async_broadcast_consolidation_complete: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }
    if (!bridge->config.enable_consolidation_routing) return 0;

    sleep_bio_consolidation_msg_t msg = {0};
    msg.header.type = BIO_MSG_CONSOLIDATION_COMPLETE;
    msg.header.source_module = BIO_MODULE_CONSOLIDATION_SLEEP;
    msg.header.channel = bridge->config.consolidation_channel;
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;
    msg.header.timestamp_us = sleep_get_timestamp_us();

    msg.memories_replayed = memories_replayed;
    msg.consolidation_efficiency = efficiency;
    msg.timestamp_us = msg.header.timestamp_us;

    bridge->stats.consolidation_events_sent++;
    bridge->stats.broadcasts_sent++;

    return 0;
}

int sleep_bio_async_broadcast_replay(
    sleep_bio_async_bridge_t* bridge,
    uint32_t memory_id,
    float replay_strength,
    float emotional_salience
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sleep_bio_async_broadcast_replay: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }
    if (!bridge->config.enable_replay_routing) return 0;

    sleep_bio_replay_msg_t msg = {0};
    msg.header.type = BIO_MSG_REPLAY_EVENT;
    msg.header.source_module = BIO_MODULE_CONSOLIDATION_SLEEP;
    msg.header.channel = bridge->config.consolidation_channel;
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;
    msg.header.timestamp_us = sleep_get_timestamp_us();

    msg.memory_id = memory_id;
    msg.replay_strength = replay_strength;
    msg.replay_speed_multiplier = 15.0f;
    msg.emotional_salience = emotional_salience;
    msg.during_stage = bridge->state.current_stage;
    msg.timestamp_us = msg.header.timestamp_us;

    bridge->stats.replay_events_sent++;
    bridge->stats.broadcasts_sent++;

    return 0;
}

int sleep_bio_async_broadcast_homeostasis(
    sleep_bio_async_bridge_t* bridge,
    float total_weight_before,
    float total_weight_after,
    uint32_t synapses_pruned
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sleep_bio_async_broadcast_homeostasis: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }
    if (!bridge->config.enable_homeostasis_routing) return 0;

    sleep_bio_homeostasis_msg_t msg = {0};
    msg.header.type = BIO_MSG_HOMEOSTATIC_ADJUSTMENT;
    msg.header.source_module = BIO_MODULE_CONSOLIDATION_SLEEP;
    msg.header.channel = bridge->config.default_channel;
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;
    msg.header.timestamp_us = sleep_get_timestamp_us();

    msg.total_weight_before = total_weight_before;
    msg.total_weight_after = total_weight_after;
    msg.downscaling_factor = (total_weight_before > 0.0f) ?
        (total_weight_after / total_weight_before) : 1.0f;
    msg.synapses_pruned = synapses_pruned;
    msg.timestamp_us = msg.header.timestamp_us;

    bridge->stats.homeostasis_updates_sent++;
    bridge->stats.broadcasts_sent++;

    return 0;
}

int sleep_bio_async_broadcast_adenosine(
    sleep_bio_async_bridge_t* bridge,
    float adenosine_level,
    float sleep_pressure
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sleep_bio_async_broadcast_adenosine: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    sleep_bio_adenosine_msg_t msg = {0};
    msg.header.type = BIO_MSG_ADENOSINE_UPDATE;
    msg.header.source_module = BIO_MODULE_CONSOLIDATION_SLEEP;
    msg.header.channel = bridge->config.default_channel;
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;
    msg.header.timestamp_us = sleep_get_timestamp_us();

    msg.adenosine_level = adenosine_level;
    msg.sleep_pressure = sleep_pressure;
    msg.sleep_needed = (sleep_pressure > 0.8f);
    msg.timestamp_us = msg.header.timestamp_us;

    bridge->state.adenosine_level = adenosine_level;
    bridge->state.sleep_pressure = sleep_pressure;

    bridge->stats.broadcasts_sent++;
    bridge->last_broadcast_us = msg.timestamp_us;

    return 0;
}

int sleep_bio_async_broadcast_oscillation(
    sleep_bio_async_bridge_t* bridge,
    float dominant_frequency_hz,
    sleep_state_t current_stage
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sleep_bio_async_broadcast_oscillation: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }
    if (!bridge->config.enable_oscillation_routing) return 0;

    sleep_bio_oscillation_msg_t msg = {0};
    msg.header.type = BIO_MSG_SLEEP_STAGE_CHANGE;
    msg.header.source_module = BIO_MODULE_CONSOLIDATION_SLEEP;
    msg.header.channel = bridge->config.default_channel;
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;
    msg.header.timestamp_us = sleep_get_timestamp_us();

    msg.dominant_frequency_hz = dominant_frequency_hz;
    msg.current_stage = current_stage;
    msg.timestamp_us = msg.header.timestamp_us;

    /* Estimate band powers based on dominant frequency */
    msg.delta_power = (dominant_frequency_hz < 4.0f) ? 0.8f : 0.1f;
    msg.theta_power = (dominant_frequency_hz >= 4.0f && dominant_frequency_hz < 8.0f) ? 0.7f : 0.2f;
    msg.alpha_power = (dominant_frequency_hz >= 8.0f && dominant_frequency_hz < 13.0f) ? 0.6f : 0.1f;
    msg.beta_power = (dominant_frequency_hz >= 13.0f && dominant_frequency_hz < 30.0f) ? 0.5f : 0.1f;
    msg.gamma_power = (dominant_frequency_hz >= 30.0f) ? 0.4f : 0.1f;

    bridge->state.dominant_frequency_hz = dominant_frequency_hz;
    bridge->stats.broadcasts_sent++;

    return 0;
}

/* ============================================================================
 * Subscription Management API
 * ============================================================================ */

int sleep_bio_async_subscribe_module(
    sleep_bio_async_bridge_t* bridge,
    uint32_t module_id,
    uint32_t msg_types
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    sleep_bio_subscription_t* existing = sleep_find_subscription(bridge, module_id);
    if (existing) {
        existing->msg_type_mask |= msg_types;
        return 0;
    }

    if (bridge->subscription_count >= bridge->subscription_capacity) {
        return -1;  /* Pool exhaustion is normal operational state */
    }

    sleep_bio_subscription_t* sub = &bridge->subscriptions[bridge->subscription_count++];
    sub->module_id = module_id;
    sub->msg_type_mask = msg_types;
    sub->active = true;
    sub->subscription_time = sleep_get_timestamp_us();
    sub->messages_sent = 0;

    bridge->stats.active_subscriptions = bridge->subscription_count;
    if (bridge->subscription_count > bridge->stats.peak_subscriptions) {
        bridge->stats.peak_subscriptions = bridge->subscription_count;
    }

    return 0;
}

int sleep_bio_async_unsubscribe_module(
    sleep_bio_async_bridge_t* bridge,
    uint32_t module_id
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    for (uint32_t i = 0; i < bridge->subscription_count; i++) {
        if (bridge->subscriptions[i].module_id == module_id) {
            bridge->subscriptions[i].active = false;
            bridge->stats.active_subscriptions--;
            return 0;
        }
    }

    return -1;  /* Module not found in subscription list - normal lookup behavior */
}

uint32_t sleep_bio_async_get_subscriber_count(
    const sleep_bio_async_bridge_t* bridge,
    sleep_bio_msg_type_t msg_type
) {
    if (!bridge) return 0;

    uint32_t count = 0;
    uint32_t mask = (1U << msg_type);

    for (uint32_t i = 0; i < bridge->subscription_count; i++) {
        if (bridge->subscriptions[i].active &&
            (bridge->subscriptions[i].msg_type_mask & mask)) {
            count++;
        }
    }

    return count;
}

/* ============================================================================
 * Statistics and Diagnostics API
 * ============================================================================ */

int sleep_bio_async_get_stats(
    const sleep_bio_async_bridge_t* bridge,
    sleep_bio_async_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sleep_bio_async_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }
    *stats = bridge->stats;
    return 0;
}

int sleep_bio_async_reset_stats(sleep_bio_async_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    uint32_t active = bridge->stats.active_subscriptions;
    uint32_t peak = bridge->stats.peak_subscriptions;
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    bridge->stats.active_subscriptions = active;
    bridge->stats.peak_subscriptions = peak;

    return 0;
}

static const char* sleep_bio_msg_type_names[] = {
    "STAGE_TRANSITION", "CONSOLIDATION_START", "CONSOLIDATION_COMPLETE",
    "REPLAY_EVENT", "HOMEOSTASIS_UPDATE", "ADENOSINE_LEVEL",
    "PRESSURE_UPDATE", "OSCILLATION_CHANGE", "CIRCADIAN_SYNC",
    "WAKE_REQUEST", "SLEEP_REQUEST", "QUERY_STATE"
};

const char* sleep_bio_msg_type_name(sleep_bio_msg_type_t msg_type) {
    if (msg_type >= SLEEP_BIO_MSG_COUNT) return "UNKNOWN";
    return sleep_bio_msg_type_names[msg_type];
}

void sleep_bio_async_print_summary(const sleep_bio_async_bridge_t* bridge) {
    if (!bridge) {
        printf("Sleep Bio-Async Bridge: NULL\n");
        return;
    }

    printf("Sleep Bio-Async Bridge Summary:\n");
    printf("  Connected: %s\n", bridge->connected ? "yes" : "no");
    printf("  Current stage: %d\n", bridge->state.current_stage);
    printf("  Subscriptions: %u (peak: %u)\n",
           bridge->stats.active_subscriptions,
           bridge->stats.peak_subscriptions);
    printf("  Messages sent: %lu, received: %lu\n",
           (unsigned long)bridge->stats.messages_sent,
           (unsigned long)bridge->stats.messages_received);
    printf("  Broadcasts: %lu (stage: %lu, consolidation: %lu, replay: %lu)\n",
           (unsigned long)bridge->stats.broadcasts_sent,
           (unsigned long)bridge->stats.stage_transitions_sent,
           (unsigned long)bridge->stats.consolidation_events_sent,
           (unsigned long)bridge->stats.replay_events_sent);
    printf("  Errors: handler=%lu, routing=%lu\n",
           (unsigned long)bridge->stats.handler_errors,
           (unsigned long)bridge->stats.routing_errors);
}
