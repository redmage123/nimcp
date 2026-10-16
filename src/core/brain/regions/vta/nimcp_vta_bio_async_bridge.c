/**
 * @file nimcp_vta_bio_async_bridge.c
 * @brief Implementation of VTA Bio-Async Integration Bridge
 * @version 1.0.0
 * @date 2026-01-11
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "core/brain/regions/vta/nimcp_vta_bio_async_bridge.h"
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

BRIDGE_BOILERPLATE_MESH_ONLY(vta_bio_async_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


#define LOG_MODULE "VTA_BIO_ASYNC_BRIDGE"


/* Internal state cache */
typedef struct {
    float tonic_da;
    float phasic_da;
    float da_level;
    float motivation;
    float value_estimate;
    float incentive_salience;
    bool reward_predicted;
    bool phasic_mode;
} vta_internal_state_t;

struct vta_bio_async_bridge_struct {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    vta_bio_async_config_t config;
    nimcp_vta_adapter_t adapter;
    bio_router_t router;
    vta_bio_subscription_t* subscriptions;
    uint32_t subscription_count;
    uint32_t subscription_capacity;
    bool connected;
    uint64_t last_broadcast_us;
    uint32_t time_since_broadcast_ms;
    vta_internal_state_t state;
    vta_bio_async_stats_t stats;
};

static uint64_t get_timestamp_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

static vta_bio_subscription_t* find_subscription(vta_bio_async_bridge_t* b, uint32_t module_id) {
    for (uint32_t i = 0; i < b->subscription_count; i++) {
        if (b->subscriptions[i].module_id == module_id && b->subscriptions[i].active) {
            return &b->subscriptions[i];
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_subscription: validation failed");
    return NULL;
}

int vta_bio_async_default_config(vta_bio_async_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vta_bio_async_default_config: config is NULL");
        return -1;
    }
    config->da_broadcast_interval_ms = VTA_BIO_DEFAULT_BROADCAST_INTERVAL_MS;
    config->enable_auto_broadcast = true;
    config->max_inbox_process_per_update = 32;
    config->message_ttl_ms = VTA_BIO_MESSAGE_TTL_MS;
    config->phasic_da_threshold = VTA_BIO_PHASIC_DA_THRESHOLD;
    config->default_channel = BIO_CHANNEL_DOPAMINE;
    config->urgent_channel = BIO_CHANNEL_DOPAMINE;
    config->max_subscriptions = VTA_BIO_MAX_SUBSCRIPTIONS;
    config->enable_rpe_routing = true;
    config->enable_motivation_routing = true;
    config->enable_plasticity_gating = true;
    config->enable_logging = false;
    return 0;
}

vta_bio_async_bridge_t* vta_bio_async_bridge_create(const vta_bio_async_config_t* config) {
    vta_bio_async_bridge_t* bridge = nimcp_calloc(1, sizeof(vta_bio_async_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bridge is NULL");

        return NULL;

    }

    if (config) {
        bridge->config = *config;
    } else {
        vta_bio_async_default_config(&bridge->config);
    }

    bridge->subscription_capacity = bridge->config.max_subscriptions;
    bridge->subscriptions = nimcp_calloc(bridge->subscription_capacity, sizeof(vta_bio_subscription_t));
    if (!bridge->subscriptions) {
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "vta_bio_async_bridge_create: bridge->subscriptions is NULL");
        return NULL;
    }

    bridge->last_broadcast_us = get_timestamp_us();
    NIMCP_LOGGING_INFO("Created %s bridge", "vta_bio_async");
    return bridge;
}

void vta_bio_async_bridge_destroy(vta_bio_async_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "vta_bio_async");
    if (bridge->connected) vta_bio_async_disconnect(bridge);
    nimcp_free(bridge->subscriptions);
    nimcp_free(bridge);
}

int vta_bio_async_connect(vta_bio_async_bridge_t* bridge, nimcp_vta_adapter_t adapter, bio_router_t router) {
    if (!bridge || !adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vta_bio_async_connect: required parameter is NULL (bridge, adapter)");
        return -1;
    }
    bridge->adapter = adapter;
    bridge->router = router;
    bridge->connected = true;
    return 0;
}

int vta_bio_async_disconnect(vta_bio_async_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vta_bio_async_disconnect: bridge is NULL");
        return -1;
    }
    bridge->adapter = NULL;
    bridge->router = NULL;
    bridge->connected = false;
    return 0;
}

bool vta_bio_async_is_connected(const vta_bio_async_bridge_t* bridge) {
    return bridge ? bridge->connected : false;
}

int vta_bio_async_process_inbox(vta_bio_async_bridge_t* bridge, uint32_t max_messages) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vta_bio_async_process_inbox: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }
    (void)max_messages;
    return 0;
}

int vta_bio_async_update(vta_bio_async_bridge_t* bridge, uint32_t delta_ms) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vta_bio_async_update: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }
    bridge->time_since_broadcast_ms += delta_ms;
    if (bridge->config.enable_auto_broadcast &&
        bridge->time_since_broadcast_ms >= bridge->config.da_broadcast_interval_ms) {
        vta_bio_async_broadcast_da_state(bridge);
        bridge->time_since_broadcast_ms = 0;
    }
    return 0;
}

int vta_bio_async_broadcast_da_state(vta_bio_async_bridge_t* bridge) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vta_bio_async_broadcast_da_state: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    vta_bio_da_state_msg_t msg = {0};
    msg.header.type = BIO_MSG_VTA_DA_STATE;
    msg.header.source_module = BIO_MODULE_VTA;
    msg.header.channel = bridge->config.default_channel;
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;
    msg.header.timestamp_us = get_timestamp_us();

    /* Use cached internal state */
    msg.tonic_da_level = bridge->state.tonic_da;
    msg.phasic_da_level = bridge->state.phasic_da;
    msg.total_da_level = bridge->state.da_level;
    msg.motivation_level = bridge->state.motivation;
    msg.value_estimate = bridge->state.value_estimate;
    msg.incentive_salience = bridge->state.incentive_salience;
    msg.reward_predicted = bridge->state.reward_predicted;
    msg.phasic_mode_active = bridge->state.phasic_mode;
    msg.timestamp_us = msg.header.timestamp_us;

    bridge->stats.da_state_broadcasts++;
    bridge->stats.broadcasts_sent++;
    bridge->last_broadcast_us = msg.timestamp_us;
    return 0;
}

/* State update function for external callers */
int vta_bio_async_update_state(
    vta_bio_async_bridge_t* bridge,
    float da_level,
    float motivation,
    float value_estimate,
    bool phasic_mode
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vta_bio_async_update_state: bridge is NULL");
        return -1;
    }

    bridge->state.da_level = da_level;
    bridge->state.tonic_da = phasic_mode ? da_level * 0.3f : da_level * 0.7f;
    bridge->state.phasic_da = phasic_mode ? da_level * 0.7f : da_level * 0.3f;
    bridge->state.motivation = motivation;
    bridge->state.value_estimate = value_estimate;
    bridge->state.incentive_salience = motivation * 0.8f;
    bridge->state.phasic_mode = phasic_mode;

    return 0;
}

int vta_bio_async_broadcast_rpe(vta_bio_async_bridge_t* bridge, float rpe, float predicted, float actual) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vta_bio_async_broadcast_rpe: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }
    if (!bridge->config.enable_rpe_routing) return 0;

    vta_bio_rpe_msg_t msg = {0};
    msg.header.type = BIO_MSG_VTA_RPE;
    msg.header.source_module = BIO_MODULE_VTA;
    msg.header.channel = bridge->config.default_channel;
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;
    msg.header.timestamp_us = get_timestamp_us();

    msg.rpe = rpe;
    msg.predicted_value = predicted;
    msg.actual_value = actual;
    msg.confidence = 1.0f - fabsf(rpe) * 0.5f;
    msg.timestamp_us = msg.header.timestamp_us;

    bridge->stats.rpe_signals_sent++;
    bridge->stats.broadcasts_sent++;
    return 0;
}

int vta_bio_async_broadcast_da_burst(vta_bio_async_bridge_t* bridge, float magnitude, float reward, uint32_t source) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vta_bio_async_broadcast_da_burst: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    vta_bio_da_burst_msg_t msg = {0};
    msg.header.type = BIO_MSG_VTA_DOPAMINE_BURST;
    msg.header.source_module = BIO_MODULE_VTA;
    msg.header.channel = bridge->config.urgent_channel;
    msg.header.flags = BIO_MSG_FLAG_BROADCAST | BIO_MSG_FLAG_URGENT;
    msg.header.timestamp_us = get_timestamp_us();

    msg.burst_magnitude = magnitude;
    msg.reward_magnitude = reward;
    msg.reward_source = source;
    msg.burst_onset_us = msg.header.timestamp_us;
    msg.expected_duration_us = 300000;

    bridge->stats.da_bursts_sent++;
    bridge->stats.broadcasts_sent++;
    return 0;
}

int vta_bio_async_broadcast_da_dip(vta_bio_async_bridge_t* bridge, float magnitude, float expected) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vta_bio_async_broadcast_da_dip: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    vta_bio_da_dip_msg_t msg = {0};
    msg.header.type = BIO_MSG_VTA_DOPAMINE_DIP;
    msg.header.source_module = BIO_MODULE_VTA;
    msg.header.channel = bridge->config.default_channel;
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;
    msg.header.timestamp_us = get_timestamp_us();

    msg.dip_magnitude = magnitude;
    msg.omission_severity = magnitude;
    msg.expected_reward = expected;
    msg.dip_onset_us = msg.header.timestamp_us;
    msg.timestamp_us = msg.header.timestamp_us;

    bridge->stats.da_dips_sent++;
    bridge->stats.broadcasts_sent++;
    return 0;
}

int vta_bio_async_send_plasticity_gate(vta_bio_async_bridge_t* bridge, float gate_strength, float lr_multiplier, uint32_t target) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vta_bio_async_send_plasticity_gate: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }
    if (!bridge->config.enable_plasticity_gating) return 0;

    vta_bio_plasticity_gate_msg_t msg = {0};
    msg.header.type = BIO_MSG_VTA_PLASTICITY_GATE;
    msg.header.source_module = BIO_MODULE_VTA;
    msg.header.target_module = target;
    msg.header.channel = bridge->config.default_channel;
    msg.header.timestamp_us = get_timestamp_us();

    msg.gate_strength = gate_strength;
    msg.learning_rate_multiplier = lr_multiplier;
    msg.gate_open = (gate_strength > 0.3f);
    msg.is_reward_gate = true;
    msg.target_module = target;
    msg.gate_duration_us = 500000;
    msg.timestamp_us = msg.header.timestamp_us;

    if (target == 0) msg.header.flags = BIO_MSG_FLAG_BROADCAST;

    bridge->stats.plasticity_gates_sent++;
    bridge->stats.messages_sent++;
    return 0;
}

int vta_bio_async_broadcast_motivation(vta_bio_async_bridge_t* bridge, float motivation, float effort) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vta_bio_async_broadcast_motivation: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }
    if (!bridge->config.enable_motivation_routing) return 0;

    vta_bio_motivation_msg_t msg = {0};
    msg.header.type = BIO_MSG_VTA_MOTIVATION_UPDATE;
    msg.header.source_module = BIO_MODULE_VTA;
    msg.header.channel = bridge->config.default_channel;
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;
    msg.header.timestamp_us = get_timestamp_us();

    msg.motivation_level = motivation;
    msg.effort_willingness = effort;
    msg.goal_proximity = 0.5f;
    msg.timestamp_us = msg.header.timestamp_us;

    bridge->stats.broadcasts_sent++;
    return 0;
}

int vta_bio_async_broadcast_value_update(vta_bio_async_bridge_t* bridge, float new_value, uint32_t context_id) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vta_bio_async_broadcast_value_update: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }
    (void)new_value;
    (void)context_id;
    bridge->stats.broadcasts_sent++;
    return vta_bio_async_broadcast_da_state(bridge);
}

int vta_bio_async_broadcast_tonic_shift(vta_bio_async_bridge_t* bridge, float new_tonic) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vta_bio_async_broadcast_tonic_shift: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }
    (void)new_tonic;
    bridge->stats.broadcasts_sent++;
    return vta_bio_async_broadcast_da_state(bridge);
}

int vta_bio_async_subscribe_module(vta_bio_async_bridge_t* bridge, uint32_t module_id, uint32_t msg_types) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vta_bio_async_subscribe_module: bridge is NULL");
        return -1;
    }
    vta_bio_subscription_t* existing = find_subscription(bridge, module_id);
    if (existing) {
        existing->msg_type_mask |= msg_types;
        return 0;
    }
    if (bridge->subscription_count >= bridge->subscription_capacity) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "vta_bio_async_subscribe_module: capacity exceeded");
        return -1;
    }

    vta_bio_subscription_t* sub = &bridge->subscriptions[bridge->subscription_count++];
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

int vta_bio_async_unsubscribe_module(vta_bio_async_bridge_t* bridge, uint32_t module_id) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vta_bio_async_unsubscribe_module: bridge is NULL");
        return -1;
    }
    for (uint32_t i = 0; i < bridge->subscription_count; i++) {
        if (bridge->subscriptions[i].module_id == module_id) {
            bridge->subscriptions[i].active = false;
            bridge->stats.active_subscriptions--;
            return 0;
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "vta_bio_async_unsubscribe_module: validation failed");
    return -1;
}

int vta_bio_async_update_subscription(vta_bio_async_bridge_t* bridge, uint32_t module_id, uint32_t msg_types) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vta_bio_async_update_subscription: bridge is NULL");
        return -1;
    }
    vta_bio_subscription_t* sub = find_subscription(bridge, module_id);
    if (!sub) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vta_bio_async_update_subscription: sub is NULL");
        return -1;
    }
    sub->msg_type_mask = msg_types;
    return 0;
}

uint32_t vta_bio_async_get_subscriber_count(const vta_bio_async_bridge_t* bridge, vta_bio_msg_type_t msg_type) {
    if (!bridge) return 0;
    uint32_t count = 0;
    uint32_t mask = (1U << msg_type);
    for (uint32_t i = 0; i < bridge->subscription_count; i++) {
        if (bridge->subscriptions[i].active && (bridge->subscriptions[i].msg_type_mask & mask)) count++;
    }
    return count;
}

int vta_bio_async_get_stats(const vta_bio_async_bridge_t* bridge, vta_bio_async_stats_t* stats) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vta_bio_async_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }
    *stats = bridge->stats;
    return 0;
}

int vta_bio_async_reset_stats(vta_bio_async_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vta_bio_async_reset_stats: bridge is NULL");
        return -1;
    }
    uint32_t active = bridge->stats.active_subscriptions;
    uint32_t peak = bridge->stats.peak_subscriptions;
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    bridge->stats.active_subscriptions = active;
    bridge->stats.peak_subscriptions = peak;
    return 0;
}

static const char* vta_bio_msg_type_names[] = {
    "DA_STATE", "RPE", "DA_BURST", "DA_DIP", "TONIC_CHANGE",
    "MOTIVATION", "VALUE_UPDATE", "INCENTIVE_SALIENCE",
    "LEARNING_SIGNAL", "PLASTICITY_GATE", "REQUEST_STATE", "MODULATE_DA"
};

const char* vta_bio_msg_type_name(vta_bio_msg_type_t msg_type) {
    if (msg_type >= VTA_BIO_MSG_COUNT) return "UNKNOWN";
    return vta_bio_msg_type_names[msg_type];
}

void vta_bio_async_print_summary(const vta_bio_async_bridge_t* bridge) {
    if (!bridge) { printf("VTA Bio-Async Bridge: NULL\n"); return; }
    printf("VTA Bio-Async Bridge Summary:\n");
    printf("  Connected: %s\n", bridge->connected ? "yes" : "no");
    printf("  Subscriptions: %u (peak: %u)\n", bridge->stats.active_subscriptions, bridge->stats.peak_subscriptions);
    printf("  Messages sent: %lu, received: %lu\n", (unsigned long)bridge->stats.messages_sent, (unsigned long)bridge->stats.messages_received);
    printf("  Broadcasts: %lu (DA state: %lu, RPE: %lu, bursts: %lu)\n",
           (unsigned long)bridge->stats.broadcasts_sent, (unsigned long)bridge->stats.da_state_broadcasts,
           (unsigned long)bridge->stats.rpe_signals_sent, (unsigned long)bridge->stats.da_bursts_sent);
}
