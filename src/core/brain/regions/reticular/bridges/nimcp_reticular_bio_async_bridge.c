/**
 * @file nimcp_reticular_bio_async_bridge.c
 * @brief Implementation of Reticular Formation Bio-Async Integration Bridge
 * @version 1.0.0
 * @date 2026-01-13
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "core/brain/regions/reticular/bridges/nimcp_reticular_bio_async_bridge.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
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

BRIDGE_BOILERPLATE_MESH_ONLY(reticular_bio_async_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


#define LOG_MODULE "RETICULAR_BIO_ASYNC_BRIDGE"


/* ============================================================================
 * Internal Bridge Structure
 * ============================================================================ */

struct reticular_bridge_struct {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    reticular_bridge_config_t config;
    nimcp_reticular_t* reticular;
    bio_router_t router;

    reticular_bridge_subscription_t* subscriptions;
    uint32_t subscription_count;
    uint32_t subscription_capacity;

    bool connected;
    uint64_t last_broadcast_us;
    uint32_t time_since_broadcast_ms;

    /* Tracking for delta broadcasts */
    float last_arousal_level;
    reticular_arousal_state_t last_arousal_state;

    reticular_bridge_stats_t stats;
};

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

static uint64_t get_timestamp_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

static reticular_bridge_subscription_t* find_subscription(
    reticular_bridge_t* b,
    uint32_t module_id
) {
    for (uint32_t i = 0; i < b->subscription_count; i++) {
        if (b->subscriptions[i].module_id == module_id && b->subscriptions[i].active) {
            return &b->subscriptions[i];
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_subscription: validation failed");
    return NULL;
}

static int count_subscribers_for_type(
    const reticular_bridge_t* b,
    reticular_bio_msg_type_t type
) {
    int count = 0;
    uint32_t mask = (1U << type);
    for (uint32_t i = 0; i < b->subscription_count; i++) {
        if (b->subscriptions[i].active && (b->subscriptions[i].msg_type_mask & mask)) {
            count++;
        }
    }
    return count;
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

int reticular_bridge_default_config(reticular_bridge_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reticular_bridge_default_config: config is NULL");
        return -1;
    }

    config->arousal_broadcast_interval_ms = RETICULAR_BRIDGE_DEFAULT_BROADCAST_INTERVAL_MS;
    config->enable_auto_broadcast = true;
    config->max_inbox_process_per_update = 32;
    config->message_ttl_ms = RETICULAR_BRIDGE_MESSAGE_TTL_MS;
    config->arousal_change_threshold = RETICULAR_BRIDGE_AROUSAL_THRESHOLD;
    config->modulator_change_threshold = RETICULAR_BRIDGE_MODULATOR_THRESHOLD;
    config->default_channel = BIO_CHANNEL_SEROTONIN;
    config->urgent_channel = BIO_CHANNEL_NOREPINEPHRINE;
    config->modulator_channel = BIO_CHANNEL_DOPAMINE;
    config->max_subscriptions = RETICULAR_BRIDGE_MAX_SUBSCRIPTIONS;
    config->enable_arousal_broadcast = true;
    config->enable_modulator_broadcast = true;
    config->enable_reflex_broadcast = true;
    config->enable_autonomic_broadcast = true;
    config->enable_sleep_wake_broadcast = true;
    config->enable_motor_tone_broadcast = true;
    config->enable_logging = false;

    return 0;
}

reticular_bridge_t* reticular_bridge_create(const reticular_bridge_config_t* config) {
    reticular_bridge_t* bridge = nimcp_calloc(1, sizeof(*bridge));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bridge is NULL");

        return NULL;

    }

    if (config) {
        bridge->config = *config;
    } else {
        reticular_bridge_default_config(&bridge->config);
    }

    bridge->subscription_capacity = bridge->config.max_subscriptions;
    bridge->subscriptions = nimcp_calloc(bridge->subscription_capacity, sizeof(*bridge->subscriptions));
    if (!bridge->subscriptions) {
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "reticular_bridge_create: bridge->subscriptions is NULL");
        return NULL;
    }

    bridge->last_broadcast_us = get_timestamp_us();
    bridge->last_arousal_level = 0.5f;
    bridge->last_arousal_state = RETICULAR_AROUSAL_RELAXED;

    NIMCP_LOGGING_INFO("Created %s bridge", "reticular_bio_async");
    return bridge;
}

void reticular_bridge_destroy(reticular_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "reticular_bio_async");

    if (bridge->connected) {
        reticular_bridge_disconnect(bridge);
    }

    nimcp_free(bridge->subscriptions);
    nimcp_free(bridge);
}

/* ============================================================================
 * Connection API
 * ============================================================================ */

int reticular_bridge_connect(
    reticular_bridge_t* bridge,
    nimcp_reticular_t* reticular,
    bio_router_t router
) {
    if (!bridge || !reticular) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reticular_bridge_connect: required parameter is NULL (bridge, reticular)");
        return -1;
    }

    bridge->reticular = reticular;
    bridge->router = router;
    bridge->connected = true;

    return 0;
}

int reticular_bridge_disconnect(reticular_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reticular_bridge_disconnect: bridge is NULL");
        return -1;
    }

    bridge->reticular = NULL;
    bridge->router = NULL;
    bridge->connected = false;

    return 0;
}

bool reticular_bridge_is_connected(const reticular_bridge_t* bridge) {
    return bridge ? bridge->connected : false;
}

/* ============================================================================
 * Message Processing API
 * ============================================================================ */

int reticular_bridge_process_inbox(reticular_bridge_t* bridge, uint32_t max_messages) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reticular_bridge_process_inbox: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    uint32_t processed = 0;
    (void)max_messages;

    bridge->stats.messages_received += processed;
    return (int)processed;
}

int reticular_bridge_update(reticular_bridge_t* bridge, uint32_t delta_ms) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reticular_bridge_update: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    bridge->time_since_broadcast_ms += delta_ms;

    return 0;
}

/* ============================================================================
 * Broadcast API
 * ============================================================================ */

int reticular_bridge_broadcast_arousal(
    reticular_bridge_t* bridge,
    float level,
    reticular_arousal_state_t state,
    bool is_transition
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reticular_bridge_broadcast_arousal: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }
    if (!bridge->config.enable_arousal_broadcast) return 0;

    reticular_bridge_arousal_msg_t msg = {0};
    msg.header.type = RETICULAR_BIO_MSG_AROUSAL_CHANGE;
    msg.header.source_module = BIO_MODULE_ID_RETICULAR;
    msg.header.channel = bridge->config.default_channel;
    msg.header.flags = is_transition ? BIO_MSG_FLAG_URGENT : 0;
    msg.header.timestamp_us = get_timestamp_us();

    msg.current_state = state;
    msg.previous_state = bridge->last_arousal_state;
    msg.arousal_level = level;
    msg.arousal_delta = level - bridge->last_arousal_level;
    msg.is_transition = is_transition;
    msg.is_emergency = (state == RETICULAR_AROUSAL_HYPERVIGILANT);
    msg.timestamp_us = get_timestamp_us();

    bridge->last_arousal_level = level;
    bridge->last_arousal_state = state;

    bridge->stats.arousal_broadcasts++;
    bridge->stats.messages_sent++;
    bridge->stats.broadcasts_sent++;
    bridge->stats.last_broadcast_time_us = msg.timestamp_us;

    return 0;
}

int reticular_bridge_broadcast_modulator(
    reticular_bridge_t* bridge,
    reticular_modulator_t modulator,
    float concentration,
    bool is_burst
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reticular_bridge_broadcast_modulator: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }
    if (!bridge->config.enable_modulator_broadcast) return 0;

    reticular_bridge_modulator_msg_t msg = {0};
    msg.header.type = RETICULAR_BIO_MSG_NEUROMODULATOR;
    msg.header.source_module = BIO_MODULE_ID_RETICULAR;
    msg.header.channel = bridge->config.modulator_channel;
    msg.header.flags = is_burst ? BIO_MSG_FLAG_URGENT : 0;
    msg.header.timestamp_us = get_timestamp_us();

    msg.modulator = modulator;
    msg.concentration = concentration;
    msg.is_tonic = !is_burst;
    msg.is_burst = is_burst;

    /* Map modulator to source nucleus */
    switch (modulator) {
        case RETICULAR_MODULATOR_SEROTONIN:
            msg.source_nucleus = RETICULAR_NUCLEUS_RAPHE_DORSAL;
            break;
        case RETICULAR_MODULATOR_NOREPINEPHRINE:
            msg.source_nucleus = RETICULAR_NUCLEUS_LOCUS_COERULEUS;
            break;
        case RETICULAR_MODULATOR_ACETYLCHOLINE:
            msg.source_nucleus = RETICULAR_NUCLEUS_PEDUNCULOPONTINE;
            break;
        case RETICULAR_MODULATOR_DOPAMINE:
            msg.source_nucleus = RETICULAR_NUCLEUS_VTA;
            break;
        default:
            msg.source_nucleus = RETICULAR_NUCLEUS_RAPHE_DORSAL;
            break;
    }

    msg.timestamp_us = get_timestamp_us();

    bridge->stats.modulator_releases++;
    bridge->stats.messages_sent++;
    bridge->stats.broadcasts_sent++;
    bridge->stats.last_broadcast_time_us = msg.timestamp_us;

    return 0;
}

int reticular_bridge_broadcast_reflex(
    reticular_bridge_t* bridge,
    reticular_reflex_t reflex,
    float stimulus,
    float response
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reticular_bridge_broadcast_reflex: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }
    if (!bridge->config.enable_reflex_broadcast) return 0;

    reticular_bridge_reflex_msg_t msg = {0};
    msg.header.type = RETICULAR_BIO_MSG_REFLEX_TRIGGER;
    msg.header.source_module = BIO_MODULE_ID_RETICULAR;
    msg.header.channel = bridge->config.urgent_channel;
    msg.header.flags = BIO_MSG_FLAG_URGENT;
    msg.header.timestamp_us = get_timestamp_us();

    msg.reflex_type = reflex;
    msg.stimulus_strength = stimulus;
    msg.response_magnitude = response;
    msg.is_active = (response > 0.0f);
    msg.timestamp_us = get_timestamp_us();

    bridge->stats.reflex_triggers++;
    bridge->stats.messages_sent++;
    bridge->stats.broadcasts_sent++;
    bridge->stats.last_broadcast_time_us = msg.timestamp_us;

    return 0;
}

int reticular_bridge_broadcast_autonomic(
    reticular_bridge_t* bridge,
    reticular_autonomic_t function,
    float sympathetic,
    float parasympathetic
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reticular_bridge_broadcast_autonomic: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }
    if (!bridge->config.enable_autonomic_broadcast) return 0;

    reticular_bridge_autonomic_msg_t msg = {0};
    msg.header.type = RETICULAR_BIO_MSG_AUTONOMIC;
    msg.header.source_module = BIO_MODULE_ID_RETICULAR;
    msg.header.channel = bridge->config.default_channel;
    msg.header.flags = 0;
    msg.header.timestamp_us = get_timestamp_us();

    msg.function = function;
    msg.sympathetic_tone = sympathetic;
    msg.parasympathetic_tone = parasympathetic;
    msg.balance = sympathetic - parasympathetic;
    msg.is_stress_response = (sympathetic > 0.7f);
    msg.is_recovery = (parasympathetic > sympathetic);
    msg.timestamp_us = get_timestamp_us();

    bridge->stats.autonomic_changes++;
    bridge->stats.messages_sent++;
    bridge->stats.broadcasts_sent++;
    bridge->stats.last_broadcast_time_us = msg.timestamp_us;

    return 0;
}

int reticular_bridge_broadcast_sleep_wake(
    reticular_bridge_t* bridge,
    reticular_arousal_state_t from_state,
    reticular_arousal_state_t to_state,
    float progress
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reticular_bridge_broadcast_sleep_wake: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }
    if (!bridge->config.enable_sleep_wake_broadcast) return 0;

    reticular_bridge_sleep_wake_msg_t msg = {0};
    msg.header.type = RETICULAR_BIO_MSG_SLEEP_STAGE;
    msg.header.source_module = BIO_MODULE_ID_RETICULAR;
    msg.header.channel = bridge->config.default_channel;
    msg.header.flags = BIO_MSG_FLAG_URGENT;
    msg.header.timestamp_us = get_timestamp_us();

    msg.from_state = from_state;
    msg.to_state = to_state;
    msg.transition_progress = progress;

    /* Determine transition type */
    bool from_sleep = (from_state <= RETICULAR_AROUSAL_REM_SLEEP);
    bool to_sleep = (to_state <= RETICULAR_AROUSAL_REM_SLEEP);
    msg.is_wake_to_sleep = (!from_sleep && to_sleep);
    msg.is_sleep_to_wake = (from_sleep && !to_sleep);
    msg.is_rem_entry = (to_state == RETICULAR_AROUSAL_REM_SLEEP);
    msg.is_rem_exit = (from_state == RETICULAR_AROUSAL_REM_SLEEP);

    msg.timestamp_us = get_timestamp_us();

    bridge->stats.sleep_wake_transitions++;
    bridge->stats.messages_sent++;
    bridge->stats.broadcasts_sent++;
    bridge->stats.last_broadcast_time_us = msg.timestamp_us;

    return 0;
}

int reticular_bridge_broadcast_motor_tone(
    reticular_bridge_t* bridge,
    float postural,
    float atonia,
    bool rem_active
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reticular_bridge_broadcast_motor_tone: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }
    if (!bridge->config.enable_motor_tone_broadcast) return 0;

    reticular_bridge_motor_tone_msg_t msg = {0};
    msg.header.type = RETICULAR_BIO_MSG_MOTOR_TONE;
    msg.header.source_module = BIO_MODULE_ID_RETICULAR;
    msg.header.channel = bridge->config.default_channel;
    msg.header.flags = rem_active ? BIO_MSG_FLAG_URGENT : 0;
    msg.header.timestamp_us = get_timestamp_us();

    msg.postural_tone = postural;
    msg.atonia_level = atonia;
    msg.rem_atonia_active = rem_active;
    msg.limb_tone = postural * (1.0f - atonia);
    msg.timestamp_us = get_timestamp_us();

    bridge->stats.motor_tone_adjustments++;
    bridge->stats.messages_sent++;
    bridge->stats.broadcasts_sent++;
    bridge->stats.last_broadcast_time_us = msg.timestamp_us;

    return 0;
}

/* ============================================================================
 * Subscription API
 * ============================================================================ */

int reticular_bridge_subscribe_module(
    reticular_bridge_t* bridge,
    uint32_t module_id,
    uint32_t msg_types
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reticular_bridge_subscribe_module: bridge is NULL");
        return -1;
    }

    reticular_bridge_subscription_t* existing = find_subscription(bridge, module_id);
    if (existing) {
        existing->msg_type_mask |= msg_types;
        return 0;
    }

    if (bridge->subscription_count >= bridge->subscription_capacity) {
        bridge->stats.routing_errors++;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "reticular_bridge_subscribe_module: capacity exceeded");
        return -1;
    }

    for (uint32_t i = 0; i < bridge->subscription_capacity; i++) {
        if (!bridge->subscriptions[i].active) {
            bridge->subscriptions[i].module_id = module_id;
            bridge->subscriptions[i].msg_type_mask = msg_types;
            bridge->subscriptions[i].active = true;
            bridge->subscriptions[i].subscription_time = get_timestamp_us();
            bridge->subscriptions[i].messages_sent = 0;
            bridge->subscription_count++;

            if (bridge->subscription_count > bridge->stats.peak_subscriptions) {
                bridge->stats.peak_subscriptions = bridge->subscription_count;
            }
            bridge->stats.active_subscriptions = bridge->subscription_count;
            return 0;
        }
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "reticular_bridge_subscribe_module: validation failed");
    return -1;
}

int reticular_bridge_unsubscribe_module(reticular_bridge_t* bridge, uint32_t module_id) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reticular_bridge_unsubscribe_module: bridge is NULL");
        return -1;
    }

    reticular_bridge_subscription_t* sub = find_subscription(bridge, module_id);
    if (!sub) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reticular_bridge_unsubscribe_module: sub is NULL");
        return -1;
    }

    sub->active = false;
    sub->msg_type_mask = 0;
    bridge->subscription_count--;
    bridge->stats.active_subscriptions = bridge->subscription_count;

    return 0;
}

uint32_t reticular_bridge_get_subscriber_count(
    const reticular_bridge_t* bridge,
    reticular_bio_msg_type_t msg_type
) {
    if (!bridge) return 0;
    return (uint32_t)count_subscribers_for_type(bridge, msg_type);
}

/* ============================================================================
 * Statistics API
 * ============================================================================ */

int reticular_bridge_get_stats(
    const reticular_bridge_t* bridge,
    reticular_bridge_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reticular_bridge_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }
    *stats = bridge->stats;
    return 0;
}

int reticular_bridge_reset_stats(reticular_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reticular_bridge_reset_stats: bridge is NULL");
        return -1;
    }

    uint32_t active_subs = bridge->stats.active_subscriptions;
    uint32_t peak_subs = bridge->stats.peak_subscriptions;

    memset(&bridge->stats, 0, sizeof(bridge->stats));

    bridge->stats.active_subscriptions = active_subs;
    bridge->stats.peak_subscriptions = peak_subs;

    return 0;
}

const char* reticular_bridge_msg_type_name(reticular_bio_msg_type_t msg_type) {
    static const char* names[] = {
        "AROUSAL_CHANGE",
        "SLEEP_STAGE",
        "NEUROMODULATOR",
        "AUTONOMIC",
        "REFLEX_TRIGGER",
        "MOTOR_TONE",
        "PAIN_MODULATION",
        "SENSORY_GATE",
        "ATTENTION_ALERT",
        "STATE_REQUEST"
    };

    if (msg_type >= RETICULAR_BIO_MSG_COUNT) return "UNKNOWN";
    return names[msg_type];
}

void reticular_bridge_print_summary(const reticular_bridge_t* bridge) {
    if (!bridge) {
        printf("Reticular Bio-Async Bridge: NULL\n");
        return;
    }

    printf("=== Reticular Formation Bio-Async Bridge Summary ===\n");
    printf("Connected: %s\n", bridge->connected ? "YES" : "NO");
    printf("Subscriptions: %u / %u (peak: %u)\n",
           bridge->stats.active_subscriptions,
           bridge->subscription_capacity,
           bridge->stats.peak_subscriptions);
    printf("\n--- Message Statistics ---\n");
    printf("Total sent: %lu\n", (unsigned long)bridge->stats.messages_sent);
    printf("Broadcasts: %lu\n", (unsigned long)bridge->stats.broadcasts_sent);
    printf("\n--- Per-Type Counts ---\n");
    printf("Arousal broadcasts: %lu\n", (unsigned long)bridge->stats.arousal_broadcasts);
    printf("Modulator releases: %lu\n", (unsigned long)bridge->stats.modulator_releases);
    printf("Reflex triggers: %lu\n", (unsigned long)bridge->stats.reflex_triggers);
    printf("Autonomic changes: %lu\n", (unsigned long)bridge->stats.autonomic_changes);
    printf("Sleep-wake transitions: %lu\n", (unsigned long)bridge->stats.sleep_wake_transitions);
    printf("Motor tone adjustments: %lu\n", (unsigned long)bridge->stats.motor_tone_adjustments);
    printf("====================================================\n");
}
