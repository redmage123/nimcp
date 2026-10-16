/**
 * @file nimcp_raphe_bio_async_bridge.c
 * @brief Implementation of Raphe Nuclei Bio-Async Integration Bridge
 * @version 1.0.0
 * @date 2026-01-11
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "core/brain/regions/raphe/nimcp_raphe_bio_async_bridge.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <stdio.h>

//=============================================================================
#include <stddef.h>
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(raphe_bio_async_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


#define LOG_MODULE "RAPHE_BIO_ASYNC_BRIDGE"


/* Internal state cache */
typedef struct {
    float tonic_5ht;
    float ht_level;
    float mood;
    float impulse_control;
    float patience;
    float anxiety;
    float social_confidence;
    bool stress_active;
} raphe_internal_state_t;

struct raphe_bio_async_bridge_struct {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    raphe_bio_async_config_t config;
    nimcp_raphe_adapter_t adapter;
    bio_router_t router;
    raphe_bio_subscription_t* subscriptions;
    uint32_t subscription_count;
    uint32_t subscription_capacity;
    bool connected;
    uint64_t last_broadcast_us;
    uint32_t time_since_broadcast_ms;
    raphe_internal_state_t state;
    raphe_bio_async_stats_t stats;
};

static uint64_t get_timestamp_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

static raphe_bio_subscription_t* find_subscription(raphe_bio_async_bridge_t* b, uint32_t module_id) {
    for (uint32_t i = 0; i < b->subscription_count; i++) {
        if (b->subscriptions[i].module_id == module_id && b->subscriptions[i].active) {
            return &b->subscriptions[i];
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_subscription: validation failed");
    return NULL;
}

int raphe_bio_async_default_config(raphe_bio_async_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "raphe_bio_async_default_config: config is NULL");
        return -1;
    }
    config->ht_broadcast_interval_ms = RAPHE_BIO_DEFAULT_BROADCAST_INTERVAL_MS;
    config->enable_auto_broadcast = true;
    config->max_inbox_process_per_update = 32;
    config->message_ttl_ms = RAPHE_BIO_MESSAGE_TTL_MS;
    config->mood_change_threshold = RAPHE_BIO_MOOD_CHANGE_THRESHOLD;
    config->default_channel = BIO_CHANNEL_SEROTONIN;
    config->max_subscriptions = RAPHE_BIO_MAX_SUBSCRIPTIONS;
    config->enable_mood_routing = true;
    config->enable_social_routing = true;
    config->enable_plasticity_gating = true;
    config->enable_logging = false;
    return 0;
}

raphe_bio_async_bridge_t* raphe_bio_async_bridge_create(const raphe_bio_async_config_t* config) {
    raphe_bio_async_bridge_t* bridge = nimcp_calloc(1, sizeof(raphe_bio_async_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bridge is NULL");

        return NULL;

    }

    if (config) {
        bridge->config = *config;
    } else {
        raphe_bio_async_default_config(&bridge->config);
    }

    bridge->subscription_capacity = bridge->config.max_subscriptions;
    bridge->subscriptions = nimcp_calloc(bridge->subscription_capacity, sizeof(raphe_bio_subscription_t));
    if (!bridge->subscriptions) {
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "raphe_bio_async_bridge_create: bridge->subscriptions is NULL");
        return NULL;
    }

    bridge->last_broadcast_us = get_timestamp_us();
    NIMCP_LOGGING_INFO("Created %s bridge", "raphe_bio_async");
    return bridge;
}

void raphe_bio_async_bridge_destroy(raphe_bio_async_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "raphe_bio_async");
    if (bridge->connected) raphe_bio_async_disconnect(bridge);
    nimcp_free(bridge->subscriptions);
    nimcp_free(bridge);
}

int raphe_bio_async_connect(raphe_bio_async_bridge_t* bridge, nimcp_raphe_adapter_t adapter, bio_router_t router) {
    if (!bridge || !adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "raphe_bio_async_connect: required parameter is NULL (bridge, adapter)");
        return -1;
    }
    bridge->adapter = adapter;
    bridge->router = router;
    bridge->connected = true;
    return 0;
}

int raphe_bio_async_disconnect(raphe_bio_async_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "raphe_bio_async_disconnect: bridge is NULL");
        return -1;
    }
    bridge->adapter = NULL;
    bridge->router = NULL;
    bridge->connected = false;
    return 0;
}

bool raphe_bio_async_is_connected(const raphe_bio_async_bridge_t* bridge) {
    return bridge ? bridge->connected : false;
}

int raphe_bio_async_process_inbox(raphe_bio_async_bridge_t* bridge, uint32_t max_messages) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "raphe_bio_async_process_inbox: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }
    (void)max_messages;
    return 0;
}

int raphe_bio_async_update(raphe_bio_async_bridge_t* bridge, uint32_t delta_ms) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "raphe_bio_async_update: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }
    bridge->time_since_broadcast_ms += delta_ms;
    if (bridge->config.enable_auto_broadcast &&
        bridge->time_since_broadcast_ms >= bridge->config.ht_broadcast_interval_ms) {
        raphe_bio_async_broadcast_5ht_state(bridge);
        bridge->time_since_broadcast_ms = 0;
    }
    return 0;
}

int raphe_bio_async_broadcast_5ht_state(raphe_bio_async_bridge_t* bridge) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "raphe_bio_async_broadcast_5ht_state: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    raphe_bio_5ht_state_msg_t msg = {0};
    msg.header.type = BIO_MSG_RAPHE_5HT_STATE;
    msg.header.source_module = BIO_MODULE_RAPHE;
    msg.header.channel = bridge->config.default_channel;
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;
    msg.header.timestamp_us = get_timestamp_us();

    /* Use cached internal state */
    msg.tonic_5ht_level = bridge->state.tonic_5ht;
    msg.total_5ht_level = bridge->state.ht_level;
    msg.mood_level = bridge->state.mood;
    msg.impulse_inhibition = bridge->state.impulse_control;
    msg.patience_level = bridge->state.patience;
    msg.anxiety_level = bridge->state.anxiety;
    msg.social_confidence = bridge->state.social_confidence;
    msg.stress_activated = bridge->state.stress_active;
    msg.timestamp_us = msg.header.timestamp_us;

    bridge->stats.ht_state_broadcasts++;
    bridge->stats.broadcasts_sent++;
    bridge->last_broadcast_us = msg.timestamp_us;
    return 0;
}

/* State update function for external callers */
int raphe_bio_async_update_state(
    raphe_bio_async_bridge_t* bridge,
    float ht_level,
    float mood,
    float impulse_control,
    float patience,
    float anxiety
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "raphe_bio_async_update_state: bridge is NULL");
        return -1;
    }

    bridge->state.ht_level = ht_level;
    bridge->state.tonic_5ht = ht_level * 0.8f;
    bridge->state.mood = mood;
    bridge->state.impulse_control = impulse_control;
    bridge->state.patience = patience;
    bridge->state.anxiety = anxiety;
    bridge->state.social_confidence = mood * 0.7f;
    bridge->state.stress_active = (anxiety > 0.7f);

    return 0;
}

int raphe_bio_async_broadcast_mood_change(raphe_bio_async_bridge_t* bridge, float prev_mood, float cur_mood, uint32_t source) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "raphe_bio_async_broadcast_mood_change: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }
    if (!bridge->config.enable_mood_routing) return 0;

    raphe_bio_mood_change_msg_t msg = {0};
    msg.header.type = BIO_MSG_RAPHE_MOOD_CHANGE;
    msg.header.source_module = BIO_MODULE_RAPHE;
    msg.header.channel = bridge->config.default_channel;
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;
    msg.header.timestamp_us = get_timestamp_us();

    msg.previous_mood = prev_mood;
    msg.current_mood = cur_mood;
    msg.change_rate = cur_mood - prev_mood;
    msg.trigger_source = source;
    msg.timestamp_us = msg.header.timestamp_us;

    bridge->stats.mood_changes_sent++;
    bridge->stats.broadcasts_sent++;
    return 0;
}

int raphe_bio_async_broadcast_impulse_control(raphe_bio_async_bridge_t* bridge, float inhibition, bool active) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "raphe_bio_async_broadcast_impulse_control: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    raphe_bio_impulse_control_msg_t msg = {0};
    msg.header.type = BIO_MSG_RAPHE_IMPULSE_CONTROL;
    msg.header.source_module = BIO_MODULE_RAPHE;
    msg.header.channel = bridge->config.default_channel;
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;
    msg.header.timestamp_us = get_timestamp_us();

    msg.impulse_inhibition = inhibition;
    msg.urgency_threshold = 1.0f - inhibition;
    msg.inhibit_active = active;
    msg.duration_us = 1000000;
    msg.timestamp_us = msg.header.timestamp_us;

    bridge->stats.impulse_signals_sent++;
    bridge->stats.broadcasts_sent++;
    return 0;
}

int raphe_bio_async_broadcast_patience(raphe_bio_async_bridge_t* bridge, float patience, float delay_tolerance) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "raphe_bio_async_broadcast_patience: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    raphe_bio_patience_msg_t msg = {0};
    msg.header.type = BIO_MSG_RAPHE_PATIENCE_SIGNAL;
    msg.header.source_module = BIO_MODULE_RAPHE;
    msg.header.channel = bridge->config.default_channel;
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;
    msg.header.timestamp_us = get_timestamp_us();

    msg.patience_level = patience;
    msg.delay_tolerance_ms = delay_tolerance;
    msg.reward_discounting = 1.0f / (1.0f + patience);
    msg.timestamp_us = msg.header.timestamp_us;

    bridge->stats.broadcasts_sent++;
    return 0;
}

int raphe_bio_async_broadcast_anxiety(raphe_bio_async_bridge_t* bridge, float anxiety, float social_anxiety) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "raphe_bio_async_broadcast_anxiety: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    raphe_bio_anxiety_msg_t msg = {0};
    msg.header.type = BIO_MSG_RAPHE_ANXIETY_STATE;
    msg.header.source_module = BIO_MODULE_RAPHE;
    msg.header.channel = bridge->config.default_channel;
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;
    msg.header.timestamp_us = get_timestamp_us();

    msg.anxiety_level = anxiety;
    msg.social_anxiety = social_anxiety;
    msg.generalized_anxiety = anxiety;
    msg.panic_threshold_reached = (anxiety > 0.9f);
    msg.timestamp_us = msg.header.timestamp_us;

    bridge->stats.broadcasts_sent++;
    return 0;
}

int raphe_bio_async_broadcast_social(raphe_bio_async_bridge_t* bridge, float confidence, float approach) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "raphe_bio_async_broadcast_social: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }
    if (!bridge->config.enable_social_routing) return 0;

    raphe_bio_social_msg_t msg = {0};
    msg.header.type = BIO_MSG_RAPHE_SOCIAL_SIGNAL;
    msg.header.source_module = BIO_MODULE_RAPHE;
    msg.header.channel = bridge->config.default_channel;
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;
    msg.header.timestamp_us = get_timestamp_us();

    msg.social_confidence = confidence;
    msg.social_approach = approach;
    msg.dominance_signal = confidence * approach;
    msg.timestamp_us = msg.header.timestamp_us;

    bridge->stats.social_signals_sent++;
    bridge->stats.broadcasts_sent++;
    return 0;
}

int raphe_bio_async_send_plasticity_gate(raphe_bio_async_bridge_t* bridge, float gate_strength, float lr_mult, uint32_t target) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "raphe_bio_async_send_plasticity_gate: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }
    if (!bridge->config.enable_plasticity_gating) return 0;

    raphe_bio_plasticity_gate_msg_t msg = {0};
    msg.header.type = BIO_MSG_RAPHE_PLASTICITY_GATE;
    msg.header.source_module = BIO_MODULE_RAPHE;
    msg.header.target_module = target;
    msg.header.channel = bridge->config.default_channel;
    msg.header.timestamp_us = get_timestamp_us();

    msg.gate_strength = gate_strength;
    msg.learning_rate_multiplier = lr_mult;
    msg.gate_open = (gate_strength > 0.3f);
    msg.target_module = target;
    msg.gate_duration_us = 1000000;
    msg.timestamp_us = msg.header.timestamp_us;

    if (target == 0) msg.header.flags = BIO_MSG_FLAG_BROADCAST;

    bridge->stats.plasticity_gates_sent++;
    bridge->stats.messages_sent++;
    return 0;
}

int raphe_bio_async_broadcast_tonic_shift(raphe_bio_async_bridge_t* bridge, float new_tonic) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "raphe_bio_async_broadcast_tonic_shift: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }
    (void)new_tonic;
    bridge->stats.broadcasts_sent++;
    return raphe_bio_async_broadcast_5ht_state(bridge);
}

int raphe_bio_async_subscribe_module(raphe_bio_async_bridge_t* bridge, uint32_t module_id, uint32_t msg_types) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "raphe_bio_async_subscribe_module: bridge is NULL");
        return -1;
    }
    raphe_bio_subscription_t* existing = find_subscription(bridge, module_id);
    if (existing) {
        existing->msg_type_mask |= msg_types;
        return 0;
    }
    if (bridge->subscription_count >= bridge->subscription_capacity) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "raphe_bio_async_subscribe_module: capacity exceeded");
        return -1;
    }

    raphe_bio_subscription_t* sub = &bridge->subscriptions[bridge->subscription_count++];
    sub->module_id = module_id;
    sub->msg_type_mask = msg_types;
    sub->active = true;
    sub->subscription_time = get_timestamp_us();

    bridge->stats.active_subscriptions = bridge->subscription_count;
    if (bridge->subscription_count > bridge->stats.peak_subscriptions) {
        bridge->stats.peak_subscriptions = bridge->subscription_count;
    }
    return 0;
}

int raphe_bio_async_unsubscribe_module(raphe_bio_async_bridge_t* bridge, uint32_t module_id) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "raphe_bio_async_unsubscribe_module: bridge is NULL");
        return -1;
    }
    for (uint32_t i = 0; i < bridge->subscription_count; i++) {
        if (bridge->subscriptions[i].module_id == module_id) {
            bridge->subscriptions[i].active = false;
            bridge->stats.active_subscriptions--;
            return 0;
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "raphe_bio_async_unsubscribe_module: validation failed");
    return -1;
}

int raphe_bio_async_update_subscription(raphe_bio_async_bridge_t* bridge, uint32_t module_id, uint32_t msg_types) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "raphe_bio_async_update_subscription: bridge is NULL");
        return -1;
    }
    raphe_bio_subscription_t* sub = find_subscription(bridge, module_id);
    if (!sub) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "raphe_bio_async_update_subscription: sub is NULL");
        return -1;
    }
    sub->msg_type_mask = msg_types;
    return 0;
}

uint32_t raphe_bio_async_get_subscriber_count(const raphe_bio_async_bridge_t* bridge, raphe_bio_msg_type_t msg_type) {
    if (!bridge) return 0;
    uint32_t count = 0;
    uint32_t mask = (1U << msg_type);
    for (uint32_t i = 0; i < bridge->subscription_count; i++) {
        if (bridge->subscriptions[i].active && (bridge->subscriptions[i].msg_type_mask & mask)) count++;
    }
    return count;
}

int raphe_bio_async_get_stats(const raphe_bio_async_bridge_t* bridge, raphe_bio_async_stats_t* stats) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "raphe_bio_async_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }
    *stats = bridge->stats;
    return 0;
}

int raphe_bio_async_reset_stats(raphe_bio_async_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "raphe_bio_async_reset_stats: bridge is NULL");
        return -1;
    }
    uint32_t active = bridge->stats.active_subscriptions;
    uint32_t peak = bridge->stats.peak_subscriptions;
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    bridge->stats.active_subscriptions = active;
    bridge->stats.peak_subscriptions = peak;
    return 0;
}

static const char* raphe_bio_msg_type_names[] = {
    "5HT_STATE", "MOOD_CHANGE", "IMPULSE_CONTROL", "PATIENCE", "TONIC_SHIFT",
    "CIRCADIAN_MOD", "PAIN_MOD", "ANXIETY", "SOCIAL", "PLASTICITY_GATE",
    "REQUEST_STATE", "MODULATE_5HT"
};

const char* raphe_bio_msg_type_name(raphe_bio_msg_type_t msg_type) {
    if (msg_type >= RAPHE_BIO_MSG_COUNT) return "UNKNOWN";
    return raphe_bio_msg_type_names[msg_type];
}

void raphe_bio_async_print_summary(const raphe_bio_async_bridge_t* bridge) {
    if (!bridge) { printf("Raphe Bio-Async Bridge: NULL\n"); return; }
    printf("Raphe Bio-Async Bridge Summary:\n");
    printf("  Connected: %s\n", bridge->connected ? "yes" : "no");
    printf("  Subscriptions: %u (peak: %u)\n", bridge->stats.active_subscriptions, bridge->stats.peak_subscriptions);
    printf("  Messages sent: %lu, received: %lu\n", (unsigned long)bridge->stats.messages_sent, (unsigned long)bridge->stats.messages_received);
    printf("  Broadcasts: %lu (5HT state: %lu, mood: %lu, social: %lu)\n",
           (unsigned long)bridge->stats.broadcasts_sent, (unsigned long)bridge->stats.ht_state_broadcasts,
           (unsigned long)bridge->stats.mood_changes_sent, (unsigned long)bridge->stats.social_signals_sent);
}
