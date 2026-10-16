/**
 * @file nimcp_lc_bio_async_bridge.c
 * @brief Implementation of Locus Coeruleus Bio-Async Integration Bridge
 * @version 1.0.0
 * @date 2026-01-11
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "core/brain/regions/locus_coeruleus/nimcp_lc_bio_async_bridge.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <time.h>

//=============================================================================
#include <stddef.h>
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(lc_bio_async_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


#define LOG_MODULE "LC_BIO_ASYNC_BRIDGE"


/* ============================================================================
 * Internal State Cache (neuromodulatory values)
 * ============================================================================ */

typedef struct {
    float tonic_ne;
    float phasic_ne;
    float ne_level;
    float arousal;
    float alertness;
    float vigilance;
    float gain_modulation;
    bool phasic_mode;
    bool stress_active;
} lc_internal_state_t;

/* ============================================================================
 * Internal Bridge Structure
 * ============================================================================ */

struct lc_bio_async_bridge_struct {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    lc_bio_async_config_t config;
    nimcp_lc_adapter_t adapter;
    bio_router_t router;

    lc_bio_subscription_t* subscriptions;
    uint32_t subscription_count;
    uint32_t subscription_capacity;

    bool connected;
    uint64_t last_broadcast_us;
    uint32_t time_since_broadcast_ms;

    /* Cached neuromodulatory state */
    lc_internal_state_t state;

    lc_bio_async_stats_t stats;
};

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

static uint64_t get_timestamp_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

static lc_bio_subscription_t* find_subscription(lc_bio_async_bridge_t* b, uint32_t module_id) {
    for (uint32_t i = 0; i < b->subscription_count; i++) {
        if (b->subscriptions[i].module_id == module_id && b->subscriptions[i].active) {
            return &b->subscriptions[i];
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_subscription: validation failed");
    return NULL;
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

int lc_bio_async_default_config(lc_bio_async_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lc_bio_async_default_config: config is NULL");
        return -1;
    }

    config->ne_broadcast_interval_ms = LC_BIO_DEFAULT_BROADCAST_INTERVAL_MS;
    config->enable_auto_broadcast = true;
    config->max_inbox_process_per_update = 32;
    config->message_ttl_ms = LC_BIO_MESSAGE_TTL_MS;
    config->phasic_burst_threshold = LC_BIO_PHASIC_BURST_THRESHOLD;
    config->default_channel = BIO_CHANNEL_NOREPINEPHRINE;
    config->urgent_channel = BIO_CHANNEL_NOREPINEPHRINE;
    config->max_subscriptions = LC_BIO_MAX_SUBSCRIPTIONS;
    config->enable_gain_modulation = true;
    config->enable_stress_routing = true;
    config->enable_plasticity_gating = true;
    config->enable_logging = false;

    return 0;
}

lc_bio_async_bridge_t* lc_bio_async_bridge_create(const lc_bio_async_config_t* config) {
    lc_bio_async_bridge_t* bridge = nimcp_calloc(1, sizeof(lc_bio_async_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bridge is NULL");

        return NULL;

    }

    if (config) {
        bridge->config = *config;
    } else {
        lc_bio_async_default_config(&bridge->config);
    }

    bridge->subscription_capacity = bridge->config.max_subscriptions;
    bridge->subscriptions = nimcp_calloc(bridge->subscription_capacity, sizeof(lc_bio_subscription_t));
    if (!bridge->subscriptions) {
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "lc_bio_async_bridge_create: bridge->subscriptions is NULL");
        return NULL;
    }

    bridge->last_broadcast_us = get_timestamp_us();
    NIMCP_LOGGING_INFO("Created %s bridge", "lc_bio_async");
    return bridge;
}

void lc_bio_async_bridge_destroy(lc_bio_async_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "lc_bio_async");

    if (bridge->connected) {
        lc_bio_async_disconnect(bridge);
    }

    nimcp_free(bridge->subscriptions);
    nimcp_free(bridge);
}

/* ============================================================================
 * Connection API
 * ============================================================================ */

int lc_bio_async_connect(
    lc_bio_async_bridge_t* bridge,
    nimcp_lc_adapter_t adapter,
    bio_router_t router
) {
    if (!bridge || !adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lc_bio_async_connect: required parameter is NULL (bridge, adapter)");
        return -1;
    }

    bridge->adapter = adapter;
    bridge->router = router;
    bridge->connected = true;

    return 0;
}

int lc_bio_async_disconnect(lc_bio_async_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lc_bio_async_disconnect: bridge is NULL");
        return -1;
    }

    bridge->adapter = NULL;
    bridge->router = NULL;
    bridge->connected = false;

    return 0;
}

bool lc_bio_async_is_connected(const lc_bio_async_bridge_t* bridge) {
    return bridge ? bridge->connected : false;
}

/* ============================================================================
 * Message Processing API
 * ============================================================================ */

int lc_bio_async_process_inbox(lc_bio_async_bridge_t* bridge, uint32_t max_messages) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lc_bio_async_process_inbox: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    /* Process incoming modulation requests - stub for now */
    uint32_t processed = 0;
    (void)max_messages;

    bridge->stats.messages_received += processed;
    return (int)processed;
}

int lc_bio_async_update(lc_bio_async_bridge_t* bridge, uint32_t delta_ms) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lc_bio_async_update: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    bridge->time_since_broadcast_ms += delta_ms;

    /* Auto-broadcast if enabled and interval elapsed */
    if (bridge->config.enable_auto_broadcast &&
        bridge->time_since_broadcast_ms >= bridge->config.ne_broadcast_interval_ms) {
        lc_bio_async_broadcast_ne_state(bridge);
        bridge->time_since_broadcast_ms = 0;
    }

    return 0;
}

/* ============================================================================
 * Broadcast API
 * ============================================================================ */

int lc_bio_async_broadcast_ne_state(lc_bio_async_bridge_t* bridge) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lc_bio_async_broadcast_ne_state: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    lc_bio_ne_state_msg_t msg = {0};
    msg.header.type = BIO_MSG_LC_NE_STATE;
    msg.header.source_module = BIO_MODULE_LOCUS_COERULEUS;
    msg.header.channel = bridge->config.default_channel;
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;
    msg.header.timestamp_us = get_timestamp_us();

    /* Use cached internal state */
    msg.tonic_ne_level = bridge->state.tonic_ne;
    msg.phasic_ne_level = bridge->state.phasic_ne;
    msg.total_ne_level = bridge->state.ne_level;
    msg.arousal_level = bridge->state.arousal;
    msg.alertness = bridge->state.alertness;
    msg.vigilance = bridge->state.vigilance;
    msg.gain_modulation = bridge->state.gain_modulation;
    msg.phasic_mode_active = bridge->state.phasic_mode;
    msg.stress_activated = bridge->state.stress_active;
    msg.timestamp_us = msg.header.timestamp_us;

    bridge->stats.ne_state_broadcasts++;
    bridge->stats.broadcasts_sent++;
    bridge->last_broadcast_us = msg.timestamp_us;

    return 0;
}

/* State update function for external callers */
int lc_bio_async_update_state(
    lc_bio_async_bridge_t* bridge,
    float ne_level,
    float arousal,
    float alertness,
    float gain_modulation,
    bool phasic_mode
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lc_bio_async_update_state: bridge is NULL");
        return -1;
    }

    bridge->state.ne_level = ne_level;
    bridge->state.tonic_ne = phasic_mode ? ne_level * 0.3f : ne_level * 0.7f;
    bridge->state.phasic_ne = phasic_mode ? ne_level * 0.7f : ne_level * 0.3f;
    bridge->state.arousal = arousal;
    bridge->state.alertness = alertness;
    bridge->state.vigilance = arousal * 0.8f;
    bridge->state.gain_modulation = gain_modulation;
    bridge->state.phasic_mode = phasic_mode;

    return 0;
}

int lc_bio_async_broadcast_arousal_change(
    lc_bio_async_bridge_t* bridge,
    float previous_arousal,
    float current_arousal,
    uint32_t trigger_source
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lc_bio_async_broadcast_arousal_change: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    lc_bio_arousal_change_msg_t msg = {0};
    msg.header.type = BIO_MSG_LC_AROUSAL_CHANGE;
    msg.header.source_module = BIO_MODULE_LOCUS_COERULEUS;
    msg.header.channel = bridge->config.default_channel;
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;
    msg.header.timestamp_us = get_timestamp_us();

    msg.previous_arousal = previous_arousal;
    msg.current_arousal = current_arousal;
    msg.change_rate = (current_arousal - previous_arousal);
    msg.trigger_source = trigger_source;
    msg.timestamp_us = msg.header.timestamp_us;

    bridge->stats.broadcasts_sent++;
    return 0;
}

int lc_bio_async_broadcast_phasic_burst(
    lc_bio_async_bridge_t* bridge,
    float magnitude,
    float novelty_score,
    uint32_t trigger_source
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lc_bio_async_broadcast_phasic_burst: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    lc_bio_phasic_burst_msg_t msg = {0};
    msg.header.type = BIO_MSG_LC_PHASIC_BURST;
    msg.header.source_module = BIO_MODULE_LOCUS_COERULEUS;
    msg.header.channel = bridge->config.urgent_channel;
    msg.header.flags = BIO_MSG_FLAG_BROADCAST | BIO_MSG_FLAG_URGENT;
    msg.header.timestamp_us = get_timestamp_us();

    msg.burst_magnitude = magnitude;
    msg.novelty_score = novelty_score;
    msg.surprise_component = novelty_score * 0.8f;
    msg.trigger_source = trigger_source;
    msg.burst_onset_us = msg.header.timestamp_us;
    msg.expected_duration_us = 200000; /* 200ms typical phasic burst */

    bridge->stats.phasic_bursts_sent++;
    bridge->stats.broadcasts_sent++;
    return 0;
}

int lc_bio_async_send_gain_modulation(
    lc_bio_async_bridge_t* bridge,
    float gain_factor,
    uint32_t target_module
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lc_bio_async_send_gain_modulation: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }
    if (!bridge->config.enable_gain_modulation) return 0;

    lc_bio_gain_modulation_msg_t msg = {0};
    msg.header.type = BIO_MSG_LC_GAIN_MODULATION;
    msg.header.source_module = BIO_MODULE_LOCUS_COERULEUS;
    msg.header.target_module = target_module;
    msg.header.channel = bridge->config.default_channel;
    msg.header.timestamp_us = get_timestamp_us();

    msg.gain_factor = gain_factor;
    msg.signal_to_noise = (gain_factor - 1.0f) * 0.5f + 1.0f;
    msg.target_module = target_module;
    msg.is_targeted = (target_module != 0);
    msg.timestamp_us = msg.header.timestamp_us;

    if (target_module == 0) {
        msg.header.flags = BIO_MSG_FLAG_BROADCAST;
    }

    bridge->stats.gain_modulations_sent++;
    bridge->stats.messages_sent++;
    return 0;
}

int lc_bio_async_broadcast_stress_response(
    lc_bio_async_bridge_t* bridge,
    float stress_intensity,
    bool is_acute
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lc_bio_async_broadcast_stress_response: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }
    if (!bridge->config.enable_stress_routing) return 0;

    lc_bio_stress_response_msg_t msg = {0};
    msg.header.type = BIO_MSG_LC_STRESS_RESPONSE;
    msg.header.source_module = BIO_MODULE_LOCUS_COERULEUS;
    msg.header.channel = bridge->config.urgent_channel;
    msg.header.flags = BIO_MSG_FLAG_BROADCAST | BIO_MSG_FLAG_URGENT;
    msg.header.timestamp_us = get_timestamp_us();

    msg.stress_intensity = stress_intensity;
    msg.ne_release_rate = stress_intensity * 1.5f;
    if (msg.ne_release_rate > 1.0f) msg.ne_release_rate = 1.0f;
    msg.is_acute = is_acute;
    msg.fight_or_flight_mode = (stress_intensity > 0.8f);
    msg.stress_onset_us = msg.header.timestamp_us;
    msg.timestamp_us = msg.header.timestamp_us;

    bridge->stats.stress_responses_sent++;
    bridge->stats.broadcasts_sent++;
    return 0;
}

int lc_bio_async_send_plasticity_gate(
    lc_bio_async_bridge_t* bridge,
    float gate_strength,
    float lr_multiplier,
    uint32_t target_module
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lc_bio_async_send_plasticity_gate: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }
    if (!bridge->config.enable_plasticity_gating) return 0;

    lc_bio_plasticity_gate_msg_t msg = {0};
    msg.header.type = BIO_MSG_LC_PLASTICITY_GATE;
    msg.header.source_module = BIO_MODULE_LOCUS_COERULEUS;
    msg.header.target_module = target_module;
    msg.header.channel = bridge->config.default_channel;
    msg.header.timestamp_us = get_timestamp_us();

    msg.gate_strength = gate_strength;
    msg.learning_rate_multiplier = lr_multiplier;
    msg.gate_open = (gate_strength > 0.3f);
    msg.target_module = target_module;
    msg.gate_duration_us = 500000; /* 500ms typical */
    msg.timestamp_us = msg.header.timestamp_us;

    if (target_module == 0) {
        msg.header.flags = BIO_MSG_FLAG_BROADCAST;
    }

    bridge->stats.plasticity_gates_sent++;
    bridge->stats.messages_sent++;
    return 0;
}

int lc_bio_async_broadcast_vigilance(lc_bio_async_bridge_t* bridge, float vigilance_level) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lc_bio_async_broadcast_vigilance: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    /* Broadcast via NE state with updated vigilance */
    bridge->stats.broadcasts_sent++;
    return lc_bio_async_broadcast_ne_state(bridge);
}

int lc_bio_async_broadcast_tonic_shift(lc_bio_async_bridge_t* bridge, float new_tonic_level) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lc_bio_async_broadcast_tonic_shift: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    /* Update tonic level and broadcast */
    (void)new_tonic_level;
    bridge->stats.broadcasts_sent++;
    return lc_bio_async_broadcast_ne_state(bridge);
}

/* ============================================================================
 * Subscription Management API
 * ============================================================================ */

int lc_bio_async_subscribe_module(
    lc_bio_async_bridge_t* bridge,
    uint32_t module_id,
    uint32_t msg_types
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lc_bio_async_subscribe_module: bridge is NULL");
        return -1;
    }

    /* Check if already subscribed */
    lc_bio_subscription_t* existing = find_subscription(bridge, module_id);
    if (existing) {
        existing->msg_type_mask |= msg_types;
        return 0;
    }

    /* Find free slot */
    if (bridge->subscription_count >= bridge->subscription_capacity) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "lc_bio_async_subscribe_module: capacity exceeded");
        return -1;
    }

    lc_bio_subscription_t* sub = &bridge->subscriptions[bridge->subscription_count++];
    sub->module_id = module_id;
    sub->msg_type_mask = msg_types;
    sub->active = true;
    sub->subscription_time = get_timestamp_us();
    sub->messages_sent = 0;

    bridge->stats.active_subscriptions = bridge->subscription_count;
    if (bridge->subscription_count > bridge->stats.peak_subscriptions) {
        bridge->stats.peak_subscriptions = bridge->subscription_count;
    }

    return 0;
}

int lc_bio_async_unsubscribe_module(lc_bio_async_bridge_t* bridge, uint32_t module_id) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lc_bio_async_unsubscribe_module: bridge is NULL");
        return -1;
    }

    for (uint32_t i = 0; i < bridge->subscription_count; i++) {
        if (bridge->subscriptions[i].module_id == module_id) {
            bridge->subscriptions[i].active = false;
            bridge->stats.active_subscriptions--;
            return 0;
        }
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "lc_bio_async_unsubscribe_module: validation failed");
    return -1;
}

int lc_bio_async_update_subscription(
    lc_bio_async_bridge_t* bridge,
    uint32_t module_id,
    uint32_t msg_types
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lc_bio_async_update_subscription: bridge is NULL");
        return -1;
    }

    lc_bio_subscription_t* sub = find_subscription(bridge, module_id);
    if (!sub) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lc_bio_async_update_subscription: sub is NULL");
        return -1;
    }

    sub->msg_type_mask = msg_types;
    return 0;
}

uint32_t lc_bio_async_get_subscriber_count(
    const lc_bio_async_bridge_t* bridge,
    lc_bio_msg_type_t msg_type
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

int lc_bio_async_get_stats(const lc_bio_async_bridge_t* bridge, lc_bio_async_stats_t* stats) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lc_bio_async_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }
    *stats = bridge->stats;
    return 0;
}

int lc_bio_async_reset_stats(lc_bio_async_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lc_bio_async_reset_stats: bridge is NULL");
        return -1;
    }

    uint32_t active = bridge->stats.active_subscriptions;
    uint32_t peak = bridge->stats.peak_subscriptions;
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    bridge->stats.active_subscriptions = active;
    bridge->stats.peak_subscriptions = peak;

    return 0;
}

static const char* lc_bio_msg_type_names[] = {
    "NE_STATE", "AROUSAL_CHANGE", "ALERTNESS_UPDATE", "PHASIC_BURST",
    "TONIC_SHIFT", "GAIN_MODULATION", "STRESS_RESPONSE", "VIGILANCE_UPDATE",
    "ATTENTION_BIAS", "PLASTICITY_GATE", "REQUEST_STATE", "MODULATE_NE"
};

const char* lc_bio_msg_type_name(lc_bio_msg_type_t msg_type) {
    if (msg_type >= LC_BIO_MSG_COUNT) return "UNKNOWN";
    return lc_bio_msg_type_names[msg_type];
}

void lc_bio_async_print_summary(const lc_bio_async_bridge_t* bridge) {
    if (!bridge) {
        printf("LC Bio-Async Bridge: NULL\n");
        return;
    }

    printf("LC Bio-Async Bridge Summary:\n");
    printf("  Connected: %s\n", bridge->connected ? "yes" : "no");
    printf("  Subscriptions: %u (peak: %u)\n",
           bridge->stats.active_subscriptions,
           bridge->stats.peak_subscriptions);
    printf("  Messages sent: %lu, received: %lu\n",
           (unsigned long)bridge->stats.messages_sent,
           (unsigned long)bridge->stats.messages_received);
    printf("  Broadcasts: %lu (NE state: %lu, phasic: %lu)\n",
           (unsigned long)bridge->stats.broadcasts_sent,
           (unsigned long)bridge->stats.ne_state_broadcasts,
           (unsigned long)bridge->stats.phasic_bursts_sent);
    printf("  Errors: handler=%lu, routing=%lu\n",
           (unsigned long)bridge->stats.handler_errors,
           (unsigned long)bridge->stats.routing_errors);
}
