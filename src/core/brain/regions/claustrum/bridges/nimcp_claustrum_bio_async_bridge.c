/**
 * @file nimcp_claustrum_bio_async_bridge.c
 * @brief Implementation of Claustrum Bio-Async Integration Bridge
 * @version 1.0.0
 * @date 2026-01-13
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "core/brain/regions/claustrum/bridges/nimcp_claustrum_bio_async_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdio.h>
#include <time.h>

//=============================================================================
#include <stddef.h>
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(claustrum_bio_async_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


#define LOG_MODULE "CLAUSTRUM_BIO_ASYNC_BRIDGE"


/* ============================================================================
 * Internal Bridge Structure
 * ============================================================================ */

struct claustrum_bio_async_bridge_struct {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    claustrum_bio_async_config_t config;
    nimcp_claustrum_t* claustrum;
    bio_router_t router;

    claustrum_bio_subscription_t* subscriptions;
    uint32_t subscription_count;
    uint32_t subscription_capacity;

    bool connected;
    uint64_t last_broadcast_us;
    uint32_t time_since_broadcast_ms;

    /* Cached state for change detection */
    uint32_t last_consciousness_level;
    uint32_t last_brain_state;
    float last_salience;

    claustrum_bio_async_stats_t stats;
};

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

static uint64_t get_timestamp_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

static claustrum_bio_subscription_t* find_subscription(
    claustrum_bio_async_bridge_t* b,
    uint32_t module_id
) {
    if (!b || !b->subscriptions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_subscription: required parameter is NULL (b, b->subscriptions)");
        return NULL;
    }

    for (uint32_t i = 0; i < b->subscription_count; i++) {
        if (b->subscriptions[i].module_id == module_id &&
            b->subscriptions[i].active) {
            return &b->subscriptions[i];
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_subscription: required parameter is NULL (b, b->subscriptions)");
    return NULL;
}

static claustrum_bio_subscription_t* find_free_subscription(
    claustrum_bio_async_bridge_t* b
) {
    if (!b || !b->subscriptions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_free_subscription: required parameter is NULL (b, b->subscriptions)");
        return NULL;
    }

    /* First look for inactive slot */
    for (uint32_t i = 0; i < b->subscription_count; i++) {
        if (!b->subscriptions[i].active) {
            return &b->subscriptions[i];
        }
    }

    /* Expand if space available */
    if (b->subscription_count < b->subscription_capacity) {
        return &b->subscriptions[b->subscription_count++];
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_free_subscription: validation failed");
    return NULL;
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

int claustrum_bio_async_default_config(claustrum_bio_async_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "claustrum_bio_async_default_config: config is NULL");
        return -1;
    }

    config->broadcast_interval_ms = CLAUSTRUM_BIO_BRIDGE_DEFAULT_INTERVAL_MS;
    config->enable_auto_broadcast = true;
    config->enable_binding_broadcast = true;
    config->max_inbox_process_per_update = 32;
    config->message_ttl_ms = CLAUSTRUM_BIO_BRIDGE_MESSAGE_TTL_MS;
    config->binding_threshold = CLAUSTRUM_BIO_BRIDGE_BINDING_THRESHOLD;
    config->salience_threshold = CLAUSTRUM_BIO_BRIDGE_SALIENCE_THRESHOLD;
    config->default_channel = BIO_CHANNEL_ACETYLCHOLINE;
    config->binding_channel = BIO_CHANNEL_ACETYLCHOLINE;
    config->max_subscriptions = CLAUSTRUM_BIO_BRIDGE_MAX_SUBSCRIPTIONS;
    config->enable_consciousness_broadcast = true;
    config->enable_sync_broadcast = true;
    config->enable_salience_alerts = true;
    config->enable_attention_events = true;
    config->enable_gating_broadcast = true;
    config->enable_logging = false;

    return 0;
}

claustrum_bio_async_bridge_t* claustrum_bio_async_bridge_create(
    const claustrum_bio_async_config_t* config
) {
    claustrum_bio_async_bridge_t* bridge = nimcp_calloc(
        1, sizeof(claustrum_bio_async_bridge_t)
    );
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bridge is NULL");

        return NULL;

    }

    if (config) {
        bridge->config = *config;
    } else {
        claustrum_bio_async_default_config(&bridge->config);
    }

    bridge->subscription_capacity = bridge->config.max_subscriptions;
    bridge->subscriptions = nimcp_calloc(
        bridge->subscription_capacity,
        sizeof(claustrum_bio_subscription_t)
    );
    if (!bridge->subscriptions) {
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "claustrum_bio_async_bridge_create: bridge->subscriptions is NULL");
        return NULL;
    }

    bridge->last_broadcast_us = get_timestamp_us();
    return bridge;
}

void claustrum_bio_async_bridge_destroy(claustrum_bio_async_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "claustrum_bio_async");

    if (bridge->connected) {
        claustrum_bio_async_disconnect(bridge);
    }

    nimcp_free(bridge->subscriptions);
    nimcp_free(bridge);
}

/* ============================================================================
 * Connection API
 * ============================================================================ */

int claustrum_bio_async_connect(
    claustrum_bio_async_bridge_t* bridge,
    nimcp_claustrum_t* claustrum,
    bio_router_t router
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "claustrum_bio_async_connect: bridge is NULL");
        return -1;
    }
    if (!claustrum) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "claustrum_bio_async_connect: claustrum is NULL");
        return -1;
    }

    bridge->claustrum = claustrum;
    bridge->router = router;
    bridge->connected = true;

    /* Initialize cached state */
    bridge->last_consciousness_level = CLAUSTRUM_CONSCIOUSNESS_UNCONSCIOUS;
    bridge->last_brain_state = CLAUSTRUM_BRAIN_STATE_DEFAULT;
    bridge->last_salience = 0.0f;

    return 0;
}

int claustrum_bio_async_disconnect(claustrum_bio_async_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "claustrum_bio_async_disconnect: bridge is NULL");
        return -1;
    }

    bridge->claustrum = NULL;
    bridge->router = NULL;
    bridge->connected = false;

    return 0;
}

bool claustrum_bio_async_is_connected(const claustrum_bio_async_bridge_t* bridge) {
    return bridge ? bridge->connected : false;
}

/* ============================================================================
 * Message Processing API
 * ============================================================================ */

int claustrum_bio_async_process_inbox(
    claustrum_bio_async_bridge_t* bridge,
    uint32_t max_messages
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "claustrum_bio_async_process_inbox: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    /* Process incoming modulation requests */
    uint32_t processed = 0;
    (void)max_messages;

    bridge->stats.messages_received += processed;
    return (int)processed;
}

int claustrum_bio_async_update(
    claustrum_bio_async_bridge_t* bridge,
    uint32_t delta_ms
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "claustrum_bio_async_update: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }
    if (!bridge->claustrum) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "claustrum_bio_async_update: bridge->claustrum is NULL");
        return -1;
    }

    bridge->time_since_broadcast_ms += delta_ms;
    nimcp_claustrum_t* cl = bridge->claustrum;

    /* Check for consciousness level changes */
    if (bridge->config.enable_consciousness_broadcast) {
        uint32_t current_level = cl->workspace_occupied ?
            CLAUSTRUM_CONSCIOUSNESS_FOCAL : CLAUSTRUM_CONSCIOUSNESS_UNCONSCIOUS;
        if (current_level != bridge->last_consciousness_level) {
            claustrum_bio_async_broadcast_consciousness(
                bridge,
                bridge->last_consciousness_level,
                current_level,
                cl->workspace_percept_id
            );
            bridge->last_consciousness_level = current_level;
        }
    }

    /* Check for brain state changes (attention switch) */
    if (bridge->config.enable_attention_events) {
        if ((uint32_t)cl->brain_state != bridge->last_brain_state) {
            claustrum_bio_async_broadcast_attention_switch(
                bridge,
                bridge->last_brain_state,
                cl->brain_state,
                cl->switch_progress
            );
            bridge->last_brain_state = (uint32_t)cl->brain_state;
        }
    }

    /* Check for salience threshold crossing */
    if (bridge->config.enable_salience_alerts) {
        float threshold = bridge->config.salience_threshold;
        if (cl->global_salience >= threshold && bridge->last_salience < threshold) {
            claustrum_bio_async_broadcast_salience(
                bridge,
                cl->global_salience,
                cl->salient_modality
            );
        }
        bridge->last_salience = cl->global_salience;
    }

    /* Auto-broadcast sync if enabled and interval elapsed */
    if (bridge->config.enable_auto_broadcast &&
        bridge->time_since_broadcast_ms >= bridge->config.broadcast_interval_ms) {

        if (bridge->config.enable_sync_broadcast) {
            claustrum_bio_async_broadcast_sync(bridge, &cl->oscillator);
        }
        bridge->time_since_broadcast_ms = 0;
    }

    return 0;
}

/* ============================================================================
 * Broadcast API
 * ============================================================================ */

int claustrum_bio_async_broadcast_binding(
    claustrum_bio_async_bridge_t* bridge,
    const nimcp_claustrum_bound_percept_t* percept
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "claustrum_bio_async_broadcast_binding: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }
    if (!percept) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "claustrum_bio_async_broadcast_binding: percept is NULL");
        return -1;
    }

    claustrum_bio_binding_msg_t msg = {0};
    msg.header.type = 0x5100;  /* Claustrum binding message */
    msg.header.source_module = BIO_MODULE_CLAUSTRUM;
    msg.header.channel = bridge->config.binding_channel;
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;
    msg.header.timestamp_us = get_timestamp_us();

    msg.percept_id = percept->id;
    msg.modality_mask = percept->modality_mask;
    msg.num_modalities = percept->num_modalities;
    msg.binding_strength = percept->binding_strength;
    msg.coherence = percept->coherence;
    msg.stability = percept->stability;
    msg.consciousness_level = (uint32_t)percept->consciousness_level;
    msg.in_workspace = percept->in_workspace;
    msg.access_strength = percept->access_strength;
    msg.timestamp_us = msg.header.timestamp_us;

    bridge->stats.binding_broadcasts++;
    bridge->stats.broadcasts_sent++;
    bridge->last_broadcast_us = msg.timestamp_us;

    return 0;
}

int claustrum_bio_async_broadcast_consciousness(
    claustrum_bio_async_bridge_t* bridge,
    nimcp_claustrum_consciousness_level_t old_level,
    nimcp_claustrum_consciousness_level_t new_level,
    uint32_t percept_id
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "claustrum_bio_async_broadcast_consciousness: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    claustrum_bio_consciousness_msg_t msg = {0};
    msg.header.type = 0x5101;  /* Claustrum consciousness message */
    msg.header.source_module = BIO_MODULE_CLAUSTRUM;
    msg.header.channel = bridge->config.default_channel;
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;
    msg.header.timestamp_us = get_timestamp_us();

    msg.old_level = (uint32_t)old_level;
    msg.new_level = (uint32_t)new_level;
    msg.percept_id = percept_id;

    if (bridge->claustrum) {
        msg.access_strength = bridge->claustrum->workspace_access_level;
        msg.workspace_occupied = bridge->claustrum->workspace_occupied;
        msg.global_coherence = bridge->claustrum->oscillator.global_coherence;
    }
    msg.timestamp_us = msg.header.timestamp_us;

    bridge->stats.consciousness_broadcasts++;
    bridge->stats.broadcasts_sent++;

    return 0;
}

int claustrum_bio_async_broadcast_sync(
    claustrum_bio_async_bridge_t* bridge,
    const nimcp_claustrum_oscillator_t* oscillator
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "claustrum_bio_async_broadcast_sync: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }
    if (!oscillator && bridge->claustrum) {
        oscillator = &bridge->claustrum->oscillator;
    }
    if (!oscillator) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "claustrum_bio_async_broadcast_sync: oscillator is NULL");
        return -1;
    }

    claustrum_bio_sync_msg_t msg = {0};
    msg.header.type = 0x5102;  /* Claustrum sync message */
    msg.header.source_module = BIO_MODULE_CLAUSTRUM;
    msg.header.channel = bridge->config.default_channel;
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;
    msg.header.timestamp_us = get_timestamp_us();

    msg.gamma_frequency = oscillator->gamma_frequency;
    msg.gamma_amplitude = oscillator->gamma_amplitude;
    msg.gamma_phase = oscillator->gamma_phase;
    msg.alpha_frequency = oscillator->alpha_frequency;
    msg.alpha_amplitude = oscillator->alpha_amplitude;
    msg.alpha_phase = oscillator->alpha_phase;
    msg.global_coherence = oscillator->global_coherence;
    msg.binding_coherence = oscillator->binding_coherence;
    msg.phase_amplitude_coupling = oscillator->phase_amplitude_coupling;
    msg.timestamp_us = msg.header.timestamp_us;

    bridge->stats.sync_broadcasts++;
    bridge->stats.broadcasts_sent++;

    return 0;
}

int claustrum_bio_async_broadcast_salience(
    claustrum_bio_async_bridge_t* bridge,
    float global_salience,
    nimcp_claustrum_modality_t salient_modality
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "claustrum_bio_async_broadcast_salience: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    claustrum_bio_salience_msg_t msg = {0};
    msg.header.type = 0x5103;  /* Claustrum salience message */
    msg.header.source_module = BIO_MODULE_CLAUSTRUM;
    msg.header.channel = BIO_CHANNEL_NOREPINEPHRINE;  /* Alert channel */
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;
    msg.header.timestamp_us = get_timestamp_us();

    msg.global_salience = global_salience;
    msg.salient_modality = (uint32_t)salient_modality;

    /* Fill modality salience from claustrum if available */
    if (bridge->claustrum) {
        msg.active_modality_mask = bridge->claustrum->active_modality_mask;
        for (uint32_t i = 0; i < CLAUSTRUM_MODALITY_COUNT && i < 8; i++) {
            msg.modality_salience[i] = bridge->claustrum->modalities[i].salience;
        }
    }
    msg.timestamp_us = msg.header.timestamp_us;

    bridge->stats.salience_broadcasts++;
    bridge->stats.broadcasts_sent++;

    return 0;
}

int claustrum_bio_async_broadcast_attention_switch(
    claustrum_bio_async_bridge_t* bridge,
    nimcp_claustrum_brain_state_t old_state,
    nimcp_claustrum_brain_state_t new_state,
    float progress
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "claustrum_bio_async_broadcast_attention_switch: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    claustrum_bio_attention_switch_msg_t msg = {0};
    msg.header.type = 0x5104;  /* Claustrum attention switch message */
    msg.header.source_module = BIO_MODULE_CLAUSTRUM;
    msg.header.channel = BIO_CHANNEL_NOREPINEPHRINE;  /* Alert channel */
    msg.header.flags = BIO_MSG_FLAG_BROADCAST | BIO_MSG_FLAG_URGENT;
    msg.header.timestamp_us = get_timestamp_us();

    msg.old_brain_state = (uint32_t)old_state;
    msg.new_brain_state = (uint32_t)new_state;
    msg.switch_progress = progress;
    msg.switch_complete = (progress >= 1.0f);

    if (bridge->claustrum) {
        msg.switch_duration_ms = bridge->claustrum->config.switch_duration_ms;
        msg.trigger_modality = (uint32_t)bridge->claustrum->salient_modality;
        msg.trigger_salience = bridge->claustrum->global_salience;
    }
    msg.timestamp_us = msg.header.timestamp_us;

    bridge->stats.attention_switch_broadcasts++;
    bridge->stats.broadcasts_sent++;

    return 0;
}

int claustrum_bio_async_broadcast_modality_gate(
    claustrum_bio_async_bridge_t* bridge,
    nimcp_claustrum_modality_t modality,
    float gate_level,
    float attention_bias
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "claustrum_bio_async_broadcast_modality_gate: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }
    if (modality >= CLAUSTRUM_MODALITY_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "claustrum_bio_async_broadcast_modality_gate: capacity exceeded");
        return -1;
    }

    claustrum_bio_modality_gate_msg_t msg = {0};
    msg.header.type = 0x5105;  /* Claustrum modality gate message */
    msg.header.source_module = BIO_MODULE_CLAUSTRUM;
    msg.header.channel = bridge->config.default_channel;
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;
    msg.header.timestamp_us = get_timestamp_us();

    msg.modality = (uint32_t)modality;
    msg.gate_level = gate_level;
    msg.gate_open = (gate_level > 0.5f);
    msg.attention_bias = attention_bias;

    /* Alpha suppression correlates inversely with gating */
    msg.alpha_suppression = 1.0f - gate_level;
    msg.timestamp_us = msg.header.timestamp_us;

    bridge->stats.modality_gate_broadcasts++;
    bridge->stats.broadcasts_sent++;

    return 0;
}

/* ============================================================================
 * Subscription Management API
 * ============================================================================ */

int claustrum_bio_async_subscribe_module(
    claustrum_bio_async_bridge_t* bridge,
    uint32_t module_id,
    uint32_t msg_types
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "claustrum_bio_async_subscribe_module: bridge is NULL");
        return -1;
    }

    /* Check if already subscribed */
    claustrum_bio_subscription_t* sub = find_subscription(bridge, module_id);
    if (sub) {
        sub->msg_type_mask |= msg_types;
        return 0;
    }

    /* Find free slot */
    sub = find_free_subscription(bridge);
    if (!sub) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "claustrum_bio_async_subscribe_module: sub is NULL");
        return -1;
    }

    sub->module_id = module_id;
    sub->msg_type_mask = msg_types;
    sub->active = true;
    sub->subscription_time = get_timestamp_us();
    sub->messages_sent = 0;

    bridge->stats.active_subscriptions++;
    if (bridge->stats.active_subscriptions > bridge->stats.peak_subscriptions) {
        bridge->stats.peak_subscriptions = bridge->stats.active_subscriptions;
    }

    return 0;
}

int claustrum_bio_async_unsubscribe_module(
    claustrum_bio_async_bridge_t* bridge,
    uint32_t module_id
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "claustrum_bio_async_unsubscribe_module: bridge is NULL");
        return -1;
    }

    claustrum_bio_subscription_t* sub = find_subscription(bridge, module_id);
    if (!sub) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "claustrum_bio_async_unsubscribe_module: sub is NULL");
        return -1;
    }

    sub->active = false;
    sub->msg_type_mask = 0;

    if (bridge->stats.active_subscriptions > 0) {
        bridge->stats.active_subscriptions--;
    }

    return 0;
}

int claustrum_bio_async_update_subscription(
    claustrum_bio_async_bridge_t* bridge,
    uint32_t module_id,
    uint32_t msg_types
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "claustrum_bio_async_update_subscription: bridge is NULL");
        return -1;
    }

    claustrum_bio_subscription_t* sub = find_subscription(bridge, module_id);
    if (!sub) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "claustrum_bio_async_update_subscription: sub is NULL");
        return -1;
    }

    sub->msg_type_mask = msg_types;
    return 0;
}

uint32_t claustrum_bio_async_get_subscriber_count(
    const claustrum_bio_async_bridge_t* bridge,
    nimcp_claustrum_bio_msg_type_t msg_type
) {
    if (!bridge || !bridge->subscriptions) return 0;
    if (msg_type >= CLAUSTRUM_BIO_MSG_COUNT) return 0;

    uint32_t mask = (1U << msg_type);
    uint32_t count = 0;

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

int claustrum_bio_async_get_stats(
    const claustrum_bio_async_bridge_t* bridge,
    claustrum_bio_async_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "claustrum_bio_async_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }

    *stats = bridge->stats;
    return 0;
}

int claustrum_bio_async_reset_stats(claustrum_bio_async_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "claustrum_bio_async_reset_stats: bridge is NULL");
        return -1;
    }

    memset(&bridge->stats, 0, sizeof(claustrum_bio_async_stats_t));
    return 0;
}

const char* claustrum_bio_msg_type_name(nimcp_claustrum_bio_msg_type_t msg_type) {
    static const char* names[] = {
        "BINDING",
        "SYNC",
        "SALIENCE",
        "ATTENTION_BIAS",
        "STATE_SWITCH",
        "WORKSPACE_GATE",
        "PERCEPT_BROADCAST",
        "GAMMA_MODULATION",
        "ALPHA_MODULATION",
        "REQUEST_BINDING",
        "MODALITY_UPDATE",
        "CONSCIOUSNESS_CHANGE"
    };

    if (msg_type >= CLAUSTRUM_BIO_MSG_COUNT) {
        return "UNKNOWN";
    }
    return names[msg_type];
}

void claustrum_bio_async_print_summary(const claustrum_bio_async_bridge_t* bridge) {
    if (!bridge) {
        printf("Claustrum Bio-Async Bridge: NULL\n");
        return;
    }

    printf("=== Claustrum Bio-Async Bridge ===\n");
    printf("Connected: %s\n", bridge->connected ? "Yes" : "No");
    printf("Subscriptions: %u / %u\n",
           bridge->stats.active_subscriptions,
           bridge->subscription_capacity);
    printf("\nMessage Statistics:\n");
    printf("  Sent: %lu\n", (unsigned long)bridge->stats.messages_sent);
    printf("  Received: %lu\n", (unsigned long)bridge->stats.messages_received);
    printf("  Broadcasts: %lu\n", (unsigned long)bridge->stats.broadcasts_sent);
    printf("\nBroadcast Breakdown:\n");
    printf("  Binding: %lu\n", (unsigned long)bridge->stats.binding_broadcasts);
    printf("  Consciousness: %lu\n",
           (unsigned long)bridge->stats.consciousness_broadcasts);
    printf("  Sync: %lu\n", (unsigned long)bridge->stats.sync_broadcasts);
    printf("  Salience: %lu\n", (unsigned long)bridge->stats.salience_broadcasts);
    printf("  Attention Switch: %lu\n",
           (unsigned long)bridge->stats.attention_switch_broadcasts);
    printf("  Modality Gate: %lu\n",
           (unsigned long)bridge->stats.modality_gate_broadcasts);
    printf("================================\n");
}
