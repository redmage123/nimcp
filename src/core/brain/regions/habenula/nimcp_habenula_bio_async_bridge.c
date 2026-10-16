/**
 * @file nimcp_habenula_bio_async_bridge.c
 * @brief Implementation of Habenula Bio-Async Integration Bridge
 * @version 1.0.0
 * @date 2026-01-11
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "core/brain/regions/habenula/nimcp_habenula_bio_async_bridge.h"
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

BRIDGE_BOILERPLATE_MESH_ONLY(habenula_bio_async_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


#define LOG_MODULE "HABENULA_BIO_ASYNC_BRIDGE"


/* Internal state cache */
typedef struct {
    float activation;
    float negative_rpe;
    float disappointment;
    float avoidance_drive;
    float vta_inhibition;
    float raphe_inhibition;
    bool punishment_detected;
    bool aversive_state_active;
} hab_internal_state_t;

struct hab_bio_async_bridge_struct {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    hab_bio_async_config_t config;
    nimcp_habenula_adapter_t adapter;
    bio_router_t router;
    hab_bio_subscription_t* subscriptions;
    uint32_t subscription_count;
    uint32_t subscription_capacity;
    bool connected;
    uint64_t last_broadcast_us;
    uint32_t time_since_broadcast_ms;
    hab_internal_state_t state;
    hab_bio_async_stats_t stats;
};

static uint64_t get_timestamp_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

static hab_bio_subscription_t* find_subscription(hab_bio_async_bridge_t* b, uint32_t module_id) {
    for (uint32_t i = 0; i < b->subscription_count; i++) {
        if (b->subscriptions[i].module_id == module_id && b->subscriptions[i].active) {
            return &b->subscriptions[i];
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_subscription: validation failed");
    return NULL;
}

int hab_bio_async_default_config(hab_bio_async_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hab_bio_async_default_config: config is NULL");
        return -1;
    }
    config->state_broadcast_interval_ms = HAB_BIO_DEFAULT_BROADCAST_INTERVAL_MS;
    config->enable_auto_broadcast = true;
    config->max_inbox_process_per_update = 32;
    config->message_ttl_ms = HAB_BIO_MESSAGE_TTL_MS;
    config->negative_rpe_threshold = HAB_BIO_NEGATIVE_RPE_THRESHOLD;
    config->default_channel = BIO_CHANNEL_SEROTONIN; /* Habenula modulates 5-HT */
    config->max_subscriptions = HAB_BIO_MAX_SUBSCRIPTIONS;
    config->enable_vta_inhibition = true;
    config->enable_raphe_inhibition = true;
    config->enable_avoidance_routing = true;
    config->enable_plasticity_gating = true;
    config->enable_logging = false;
    return 0;
}

hab_bio_async_bridge_t* hab_bio_async_bridge_create(const hab_bio_async_config_t* config) {
    hab_bio_async_bridge_t* bridge = nimcp_calloc(1, sizeof(hab_bio_async_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bridge is NULL");

        return NULL;

    }

    if (config) {
        bridge->config = *config;
    } else {
        hab_bio_async_default_config(&bridge->config);
    }

    bridge->subscription_capacity = bridge->config.max_subscriptions;
    bridge->subscriptions = nimcp_calloc(bridge->subscription_capacity, sizeof(hab_bio_subscription_t));
    if (!bridge->subscriptions) {
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hab_bio_async_bridge_create: bridge->subscriptions is NULL");
        return NULL;
    }

    bridge->last_broadcast_us = get_timestamp_us();
    NIMCP_LOGGING_INFO("Created %s bridge", "habenula_bio_async");
    return bridge;
}

void hab_bio_async_bridge_destroy(hab_bio_async_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "habenula_bio_async");
    if (bridge->connected) hab_bio_async_disconnect(bridge);
    nimcp_free(bridge->subscriptions);
    nimcp_free(bridge);
}

int hab_bio_async_connect(hab_bio_async_bridge_t* bridge, nimcp_habenula_adapter_t adapter, bio_router_t router) {
    if (!bridge || !adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hab_bio_async_connect: required parameter is NULL (bridge, adapter)");
        return -1;
    }
    bridge->adapter = adapter;
    bridge->router = router;
    bridge->connected = true;
    return 0;
}

int hab_bio_async_disconnect(hab_bio_async_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hab_bio_async_disconnect: bridge is NULL");
        return -1;
    }
    bridge->adapter = NULL;
    bridge->router = NULL;
    bridge->connected = false;
    return 0;
}

bool hab_bio_async_is_connected(const hab_bio_async_bridge_t* bridge) {
    return bridge ? bridge->connected : false;
}

int hab_bio_async_process_inbox(hab_bio_async_bridge_t* bridge, uint32_t max_messages) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hab_bio_async_process_inbox: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }
    (void)max_messages;
    return 0;
}

int hab_bio_async_update(hab_bio_async_bridge_t* bridge, uint32_t delta_ms) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hab_bio_async_update: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }
    bridge->time_since_broadcast_ms += delta_ms;
    if (bridge->config.enable_auto_broadcast &&
        bridge->time_since_broadcast_ms >= bridge->config.state_broadcast_interval_ms) {
        hab_bio_async_broadcast_state(bridge);
        bridge->time_since_broadcast_ms = 0;
    }
    return 0;
}

int hab_bio_async_broadcast_state(hab_bio_async_bridge_t* bridge) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hab_bio_async_broadcast_state: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    hab_bio_state_msg_t msg = {0};
    msg.header.type = BIO_MSG_HABENULA_STATE;
    msg.header.source_module = BIO_MODULE_HABENULA;
    msg.header.channel = bridge->config.default_channel;
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;
    msg.header.timestamp_us = get_timestamp_us();

    /* Use cached internal state */
    msg.activation_level = bridge->state.activation;
    msg.negative_rpe = bridge->state.negative_rpe;
    msg.disappointment_level = bridge->state.disappointment;
    msg.avoidance_drive = bridge->state.avoidance_drive;
    msg.vta_inhibition_output = bridge->state.vta_inhibition;
    msg.raphe_inhibition_output = bridge->state.raphe_inhibition;
    msg.punishment_detected = bridge->state.punishment_detected;
    msg.aversive_state_active = bridge->state.aversive_state_active;
    msg.timestamp_us = msg.header.timestamp_us;

    bridge->stats.state_broadcasts++;
    bridge->stats.broadcasts_sent++;
    bridge->last_broadcast_us = msg.timestamp_us;
    return 0;
}

/* State update function for external callers */
int hab_bio_async_update_state(
    hab_bio_async_bridge_t* bridge,
    float activation,
    float negative_rpe,
    float disappointment,
    float avoidance_drive,
    bool punishment_detected
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hab_bio_async_update_state: bridge is NULL");
        return -1;
    }

    bridge->state.activation = activation;
    bridge->state.negative_rpe = negative_rpe;
    bridge->state.disappointment = disappointment;
    bridge->state.avoidance_drive = avoidance_drive;
    bridge->state.vta_inhibition = activation * 0.8f;
    bridge->state.raphe_inhibition = activation * 0.5f;
    bridge->state.punishment_detected = punishment_detected;
    bridge->state.aversive_state_active = (activation > 0.5f);

    return 0;
}

int hab_bio_async_broadcast_negative_rpe(hab_bio_async_bridge_t* bridge, float rpe, float expected, float actual) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hab_bio_async_broadcast_negative_rpe: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    hab_bio_negative_rpe_msg_t msg = {0};
    msg.header.type = BIO_MSG_HABENULA_NEGATIVE_RPE;
    msg.header.source_module = BIO_MODULE_HABENULA;
    msg.header.channel = bridge->config.default_channel;
    msg.header.flags = BIO_MSG_FLAG_BROADCAST | BIO_MSG_FLAG_URGENT;
    msg.header.timestamp_us = get_timestamp_us();

    msg.negative_rpe = rpe;
    msg.expected_value = expected;
    msg.actual_value = actual;
    msg.timestamp_us = msg.header.timestamp_us;

    bridge->stats.negative_rpe_signals++;
    bridge->stats.broadcasts_sent++;
    return 0;
}

int hab_bio_async_broadcast_punishment(hab_bio_async_bridge_t* bridge, float intensity, uint32_t type, bool unexpected) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hab_bio_async_broadcast_punishment: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    hab_bio_punishment_msg_t msg = {0};
    msg.header.type = BIO_MSG_HABENULA_PUNISHMENT_SIGNAL;
    msg.header.source_module = BIO_MODULE_HABENULA;
    msg.header.channel = bridge->config.default_channel;
    msg.header.flags = BIO_MSG_FLAG_BROADCAST | BIO_MSG_FLAG_URGENT;
    msg.header.timestamp_us = get_timestamp_us();

    msg.punishment_intensity = intensity;
    msg.punishment_type = type;
    msg.unexpected = unexpected;
    msg.punishment_onset_us = msg.header.timestamp_us;
    msg.timestamp_us = msg.header.timestamp_us;

    bridge->stats.punishment_signals++;
    bridge->stats.broadcasts_sent++;
    return 0;
}

int hab_bio_async_broadcast_disappointment(hab_bio_async_bridge_t* bridge, float level, float expected_reward) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hab_bio_async_broadcast_disappointment: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    hab_bio_disappointment_msg_t msg = {0};
    msg.header.type = BIO_MSG_HABENULA_DISAPPOINTMENT;
    msg.header.source_module = BIO_MODULE_HABENULA;
    msg.header.channel = bridge->config.default_channel;
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;
    msg.header.timestamp_us = get_timestamp_us();

    msg.disappointment_level = level;
    msg.expected_reward = expected_reward;
    msg.actual_reward = 0.0f;
    msg.timestamp_us = msg.header.timestamp_us;

    bridge->stats.disappointment_signals++;
    bridge->stats.broadcasts_sent++;
    return 0;
}

int hab_bio_async_send_avoidance_trigger(hab_bio_async_bridge_t* bridge, float strength, uint32_t stimulus_id, bool urgent) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hab_bio_async_send_avoidance_trigger: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }
    if (!bridge->config.enable_avoidance_routing) return 0;

    hab_bio_avoidance_trigger_msg_t msg = {0};
    msg.header.type = BIO_MSG_HABENULA_AVOIDANCE_TRIGGER;
    msg.header.source_module = BIO_MODULE_HABENULA;
    msg.header.channel = bridge->config.default_channel;
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;
    if (urgent) msg.header.flags |= BIO_MSG_FLAG_URGENT;
    msg.header.timestamp_us = get_timestamp_us();

    msg.avoidance_strength = strength;
    msg.stimulus_id = stimulus_id;
    msg.urgent = urgent;
    msg.timestamp_us = msg.header.timestamp_us;

    bridge->stats.messages_sent++;
    return 0;
}

int hab_bio_async_send_vta_inhibition(hab_bio_async_bridge_t* bridge, float strength) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hab_bio_async_send_vta_inhibition: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }
    if (!bridge->config.enable_vta_inhibition) return 0;

    hab_bio_inhibit_msg_t msg = {0};
    msg.header.type = BIO_MSG_HABENULA_VTA_INHIBIT;
    msg.header.source_module = BIO_MODULE_HABENULA;
    msg.header.target_module = BIO_MODULE_VTA;
    msg.header.channel = BIO_CHANNEL_DOPAMINE;
    msg.header.timestamp_us = get_timestamp_us();

    msg.inhibition_strength = strength;
    msg.target_module = BIO_MODULE_VTA;
    msg.inhibition_duration_us = 500000;
    msg.timestamp_us = msg.header.timestamp_us;

    bridge->stats.vta_inhibitions_sent++;
    bridge->stats.messages_sent++;
    return 0;
}

int hab_bio_async_send_raphe_inhibition(hab_bio_async_bridge_t* bridge, float strength) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hab_bio_async_send_raphe_inhibition: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }
    if (!bridge->config.enable_raphe_inhibition) return 0;

    hab_bio_inhibit_msg_t msg = {0};
    msg.header.type = BIO_MSG_HABENULA_RAPHE_INHIBIT;
    msg.header.source_module = BIO_MODULE_HABENULA;
    msg.header.target_module = BIO_MODULE_RAPHE;
    msg.header.channel = BIO_CHANNEL_SEROTONIN;
    msg.header.timestamp_us = get_timestamp_us();

    msg.inhibition_strength = strength;
    msg.target_module = BIO_MODULE_RAPHE;
    msg.inhibition_duration_us = 500000;
    msg.timestamp_us = msg.header.timestamp_us;

    bridge->stats.raphe_inhibitions_sent++;
    bridge->stats.messages_sent++;
    return 0;
}

int hab_bio_async_broadcast_relief(hab_bio_async_bridge_t* bridge, float magnitude, float expected_punishment) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hab_bio_async_broadcast_relief: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    hab_bio_relief_msg_t msg = {0};
    msg.header.type = BIO_MSG_HABENULA_RELIEF_SIGNAL;
    msg.header.source_module = BIO_MODULE_HABENULA;
    msg.header.channel = bridge->config.default_channel;
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;
    msg.header.timestamp_us = get_timestamp_us();

    msg.relief_magnitude = magnitude;
    msg.expected_punishment = expected_punishment;
    msg.timestamp_us = msg.header.timestamp_us;

    bridge->stats.relief_signals++;
    bridge->stats.broadcasts_sent++;
    return 0;
}

int hab_bio_async_send_plasticity_gate(hab_bio_async_bridge_t* bridge, float gate_strength, float lr_mult, uint32_t target) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hab_bio_async_send_plasticity_gate: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }
    if (!bridge->config.enable_plasticity_gating) return 0;

    hab_bio_plasticity_gate_msg_t msg = {0};
    msg.header.type = BIO_MSG_HABENULA_PLASTICITY_GATE;
    msg.header.source_module = BIO_MODULE_HABENULA;
    msg.header.target_module = target;
    msg.header.channel = bridge->config.default_channel;
    msg.header.timestamp_us = get_timestamp_us();

    msg.gate_strength = gate_strength;
    msg.learning_rate_multiplier = lr_mult;
    msg.gate_open = (gate_strength > 0.3f);
    msg.is_aversive_gate = true;
    msg.target_module = target;
    msg.gate_duration_us = 500000;
    msg.timestamp_us = msg.header.timestamp_us;

    if (target == 0) msg.header.flags = BIO_MSG_FLAG_BROADCAST;

    bridge->stats.plasticity_gates_sent++;
    bridge->stats.messages_sent++;
    return 0;
}

int hab_bio_async_subscribe_module(hab_bio_async_bridge_t* bridge, uint32_t module_id, uint32_t msg_types) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hab_bio_async_subscribe_module: bridge is NULL");
        return -1;
    }
    hab_bio_subscription_t* existing = find_subscription(bridge, module_id);
    if (existing) {
        existing->msg_type_mask |= msg_types;
        return 0;
    }
    if (bridge->subscription_count >= bridge->subscription_capacity) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "hab_bio_async_subscribe_module: capacity exceeded");
        return -1;
    }

    hab_bio_subscription_t* sub = &bridge->subscriptions[bridge->subscription_count++];
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

int hab_bio_async_unsubscribe_module(hab_bio_async_bridge_t* bridge, uint32_t module_id) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hab_bio_async_unsubscribe_module: bridge is NULL");
        return -1;
    }
    for (uint32_t i = 0; i < bridge->subscription_count; i++) {
        if (bridge->subscriptions[i].module_id == module_id) {
            bridge->subscriptions[i].active = false;
            bridge->stats.active_subscriptions--;
            return 0;
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hab_bio_async_unsubscribe_module: validation failed");
    return -1;
}

int hab_bio_async_update_subscription(hab_bio_async_bridge_t* bridge, uint32_t module_id, uint32_t msg_types) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hab_bio_async_update_subscription: bridge is NULL");
        return -1;
    }
    hab_bio_subscription_t* sub = find_subscription(bridge, module_id);
    if (!sub) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hab_bio_async_update_subscription: sub is NULL");
        return -1;
    }
    sub->msg_type_mask = msg_types;
    return 0;
}

uint32_t hab_bio_async_get_subscriber_count(const hab_bio_async_bridge_t* bridge, hab_bio_msg_type_t msg_type) {
    if (!bridge) return 0;
    uint32_t count = 0;
    uint32_t mask = (1U << msg_type);
    for (uint32_t i = 0; i < bridge->subscription_count; i++) {
        if (bridge->subscriptions[i].active && (bridge->subscriptions[i].msg_type_mask & mask)) count++;
    }
    return count;
}

int hab_bio_async_get_stats(const hab_bio_async_bridge_t* bridge, hab_bio_async_stats_t* stats) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hab_bio_async_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }
    *stats = bridge->stats;
    return 0;
}

int hab_bio_async_reset_stats(hab_bio_async_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hab_bio_async_reset_stats: bridge is NULL");
        return -1;
    }
    uint32_t active = bridge->stats.active_subscriptions;
    uint32_t peak = bridge->stats.peak_subscriptions;
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    bridge->stats.active_subscriptions = active;
    bridge->stats.peak_subscriptions = peak;
    return 0;
}

static const char* hab_bio_msg_type_names[] = {
    "STATE", "NEGATIVE_RPE", "PUNISHMENT", "DISAPPOINTMENT", "AVOIDANCE_TRIGGER",
    "VTA_INHIBIT", "RAPHE_INHIBIT", "AVERSIVE_LEARNING", "RELIEF", "PLASTICITY_GATE",
    "REQUEST_STATE", "MODULATE"
};

const char* hab_bio_msg_type_name(hab_bio_msg_type_t msg_type) {
    if (msg_type >= HAB_BIO_MSG_COUNT) return "UNKNOWN";
    return hab_bio_msg_type_names[msg_type];
}

void hab_bio_async_print_summary(const hab_bio_async_bridge_t* bridge) {
    if (!bridge) { printf("Habenula Bio-Async Bridge: NULL\n"); return; }
    printf("Habenula Bio-Async Bridge Summary:\n");
    printf("  Connected: %s\n", bridge->connected ? "yes" : "no");
    printf("  Subscriptions: %u (peak: %u)\n", bridge->stats.active_subscriptions, bridge->stats.peak_subscriptions);
    printf("  Messages sent: %lu, received: %lu\n", (unsigned long)bridge->stats.messages_sent, (unsigned long)bridge->stats.messages_received);
    printf("  Broadcasts: %lu (state: %lu, neg RPE: %lu, punishment: %lu)\n",
           (unsigned long)bridge->stats.broadcasts_sent, (unsigned long)bridge->stats.state_broadcasts,
           (unsigned long)bridge->stats.negative_rpe_signals, (unsigned long)bridge->stats.punishment_signals);
    printf("  Inhibitions: VTA=%lu, Raphe=%lu\n",
           (unsigned long)bridge->stats.vta_inhibitions_sent, (unsigned long)bridge->stats.raphe_inhibitions_sent);
}
