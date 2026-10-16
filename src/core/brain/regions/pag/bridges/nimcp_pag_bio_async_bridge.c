/**
 * @file nimcp_pag_bio_async_bridge.c
 * @brief Implementation of PAG Bio-Async Integration Bridge
 * @version 1.0.0
 * @date 2026-01-13
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "core/brain/regions/pag/bridges/nimcp_pag_bio_async_bridge.h"
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

BRIDGE_BOILERPLATE_MESH_ONLY(pag_bio_async_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


#define LOG_MODULE "PAG_BIO_ASYNC_BRIDGE"


/* ============================================================================
 * Internal Bridge Structure
 * ============================================================================ */

struct pag_bio_async_bridge_struct {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    pag_bio_async_config_t config;
    void* pag;                          /**< PAG instance (opaque) */
    bio_router_t router;

    pag_bio_subscription_t* subscriptions;
    uint32_t subscription_count;
    uint32_t subscription_capacity;

    bool connected;
    uint64_t last_broadcast_us;
    uint32_t time_since_broadcast_ms;

    pag_bio_async_stats_t stats;
};

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

static uint64_t get_timestamp_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

static pag_bio_subscription_t* find_subscription(
    pag_bio_async_bridge_t* bridge,
    uint32_t module_id
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NULL;

    }

    for (uint32_t i = 0; i < bridge->subscription_count; i++) {
        if (bridge->subscriptions[i].module_id == module_id &&
            bridge->subscriptions[i].active) {
            return &bridge->subscriptions[i];
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_subscription: operation failed");
    return NULL;
}

static void init_header(
    bio_message_header_t* header,
    uint32_t type,
    nimcp_bio_channel_type_t channel,
    uint32_t flags
) {
    if (!header) return;

    memset(header, 0, sizeof(bio_message_header_t));
    header->type = type;
    header->source_module = PAG_BIO_MODULE_ID;
    header->channel = channel;
    header->flags = flags;
    header->timestamp_us = get_timestamp_us();
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

int pag_bio_async_default_config(pag_bio_async_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pag_bio_async_default_config: config is NULL");
        return -1;
    }

    memset(config, 0, sizeof(pag_bio_async_config_t));

    config->broadcast_interval_ms = PAG_BIO_DEFAULT_BROADCAST_INTERVAL_MS;
    config->enable_auto_broadcast = true;
    config->enable_urgent_defense = true;
    config->max_inbox_process_per_update = 32;
    config->message_ttl_ms = PAG_BIO_MESSAGE_TTL_MS;
    config->default_channel = BIO_CHANNEL_NOREPINEPHRINE;
    config->defense_channel = BIO_CHANNEL_NOREPINEPHRINE;
    config->pain_channel = BIO_CHANNEL_SEROTONIN;
    config->max_subscriptions = PAG_BIO_MAX_SUBSCRIPTIONS;
    config->enable_defense_broadcast = true;
    config->enable_pain_broadcast = true;
    config->enable_emotion_broadcast = true;
    config->enable_vocal_broadcast = true;
    config->enable_autonomic_broadcast = true;
    config->enable_threat_broadcast = true;
    config->enable_logging = false;

    return 0;
}

pag_bio_async_bridge_t* pag_bio_async_bridge_create(
    const pag_bio_async_config_t* config
) {
    pag_bio_async_bridge_t* bridge = nimcp_calloc(1,
                                                   sizeof(pag_bio_async_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bridge is NULL");

        return NULL;

    }

    if (config) {
        bridge->config = *config;
    } else {
        pag_bio_async_default_config(&bridge->config);
    }

    bridge->subscription_capacity = bridge->config.max_subscriptions;
    bridge->subscriptions = nimcp_calloc(bridge->subscription_capacity,
                                         sizeof(pag_bio_subscription_t));
    if (!bridge->subscriptions) {
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pag_bio_async_bridge_create: bridge->subscriptions is NULL");
        return NULL;
    }

    bridge->last_broadcast_us = get_timestamp_us();
    return bridge;
}

void pag_bio_async_bridge_destroy(pag_bio_async_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "pag_bio_async");

    if (bridge->connected) {
        pag_bio_async_disconnect(bridge);
    }

    nimcp_free(bridge->subscriptions);
    nimcp_free(bridge);
}

/* ============================================================================
 * Connection API
 * ============================================================================ */

int pag_bio_async_connect(
    pag_bio_async_bridge_t* bridge,
    void* pag,
    bio_router_t router
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pag_bio_async_connect: bridge is NULL");
        return -1;
    }
    if (!pag) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pag_bio_async_connect: pag is NULL");
        return -1;
    }

    bridge->pag = pag;
    bridge->router = router;
    bridge->connected = true;
    bridge->last_broadcast_us = get_timestamp_us();

    return 0;
}

int pag_bio_async_disconnect(pag_bio_async_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pag_bio_async_disconnect: bridge is NULL");
        return -1;
    }

    bridge->pag = NULL;
    bridge->router = NULL;
    bridge->connected = false;

    return 0;
}

bool pag_bio_async_is_connected(const pag_bio_async_bridge_t* bridge) {
    return bridge ? bridge->connected : false;
}

/* ============================================================================
 * Message Processing API
 * ============================================================================ */

int pag_bio_async_process_inbox(
    pag_bio_async_bridge_t* bridge,
    uint32_t max_messages
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pag_bio_async_process_inbox: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    uint32_t processed = 0;
    (void)max_messages;

    bridge->stats.messages_received += processed;
    return (int)processed;
}

int pag_bio_async_update(pag_bio_async_bridge_t* bridge, uint32_t delta_ms) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pag_bio_async_update: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    bridge->time_since_broadcast_ms += delta_ms;

    if (bridge->config.enable_auto_broadcast &&
        bridge->time_since_broadcast_ms >= bridge->config.broadcast_interval_ms) {
        bridge->time_since_broadcast_ms = 0;
        bridge->last_broadcast_us = get_timestamp_us();
    }

    return 0;
}

/* ============================================================================
 * Broadcast API - Defense State
 * ============================================================================ */

int pag_bio_async_broadcast_defense(
    pag_bio_async_bridge_t* bridge,
    pag_bio_defense_type_t defense_type,
    float intensity,
    pag_bio_threat_level_t threat_level
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pag_bio_async_broadcast_defense: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }
    if (!bridge->config.enable_defense_broadcast) return 0;

    pag_bio_defense_msg_t msg = {0};
    uint32_t flags = BIO_MSG_FLAG_BROADCAST;
    if (bridge->config.enable_urgent_defense) {
        flags |= BIO_MSG_FLAG_URGENT;
    }

    init_header(&msg.header, 0x5100, bridge->config.defense_channel, flags);

    msg.defense_type = defense_type;
    msg.defense_intensity = intensity;
    msg.active_coping = (defense_type == PAG_BIO_DEFENSE_FIGHT ||
                         defense_type == PAG_BIO_DEFENSE_FLIGHT);
    msg.threat_level = threat_level;
    msg.threat_intensity = intensity;
    msg.dlpag_activity = msg.active_coping ? intensity : 0.0f;
    msg.vlpag_activity = msg.active_coping ? 0.0f : intensity;
    msg.motor_output = intensity;
    msg.timestamp_us = msg.header.timestamp_us;

    bridge->stats.defense_broadcasts++;
    bridge->stats.broadcasts_sent++;
    bridge->stats.messages_sent++;
    bridge->last_broadcast_us = msg.timestamp_us;

    return 0;
}

/* ============================================================================
 * Broadcast API - Pain Modulation
 * ============================================================================ */

int pag_bio_async_broadcast_pain_mod(
    pag_bio_async_bridge_t* bridge,
    float analgesia_level,
    float descending_inhibition,
    float pain_intensity
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pag_bio_async_broadcast_pain_mod: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }
    if (!bridge->config.enable_pain_broadcast) return 0;

    pag_bio_pain_mod_msg_t msg = {0};
    init_header(&msg.header, 0x5101, bridge->config.pain_channel,
                BIO_MSG_FLAG_BROADCAST);

    msg.analgesia_level = analgesia_level;
    msg.descending_inhibition = descending_inhibition;
    msg.opioid_activity = analgesia_level * 0.6f;
    msg.non_opioid_activity = analgesia_level * 0.4f;
    msg.pain_intensity = pain_intensity;
    msg.timestamp_us = msg.header.timestamp_us;

    bridge->stats.pain_mod_broadcasts++;
    bridge->stats.broadcasts_sent++;
    bridge->stats.messages_sent++;
    bridge->last_broadcast_us = msg.timestamp_us;

    return 0;
}

/* ============================================================================
 * Broadcast API - Emotion
 * ============================================================================ */

int pag_bio_async_broadcast_emotion(
    pag_bio_async_bridge_t* bridge,
    pag_bio_emotion_type_t emotion,
    float intensity,
    float valence
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pag_bio_async_broadcast_emotion: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }
    if (!bridge->config.enable_emotion_broadcast) return 0;

    pag_bio_emotion_msg_t msg = {0};
    init_header(&msg.header, 0x5102, bridge->config.default_channel,
                BIO_MSG_FLAG_BROADCAST);

    msg.dominant = emotion;
    msg.emotional_intensity = intensity;
    msg.valence = valence;
    msg.arousal = intensity;

    switch (emotion) {
        case PAG_BIO_EMOTION_FEAR:   msg.fear_level = intensity; break;
        case PAG_BIO_EMOTION_RAGE:   msg.rage_level = intensity; break;
        case PAG_BIO_EMOTION_PAIN:   msg.pain_affect = intensity; break;
        case PAG_BIO_EMOTION_PANIC:  msg.panic_level = intensity; break;
        case PAG_BIO_EMOTION_MATERNAL: msg.maternal_level = intensity; break;
        default: break;
    }

    msg.source_region = PAG_BIO_MODULE_ID;
    msg.timestamp_us = msg.header.timestamp_us;

    bridge->stats.emotion_broadcasts++;
    bridge->stats.broadcasts_sent++;
    bridge->stats.messages_sent++;
    bridge->last_broadcast_us = msg.timestamp_us;

    return 0;
}

/* ============================================================================
 * Broadcast API - Vocalization
 * ============================================================================ */

int pag_bio_async_broadcast_vocalization(
    pag_bio_async_bridge_t* bridge,
    pag_bio_vocal_type_t vocal_type,
    float intensity,
    float urgency
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pag_bio_async_broadcast_vocalization: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }
    if (!bridge->config.enable_vocal_broadcast) return 0;

    pag_bio_vocal_msg_t msg = {0};
    uint32_t flags = BIO_MSG_FLAG_BROADCAST;
    if (urgency > 0.7f) {
        flags |= BIO_MSG_FLAG_URGENT;
    }

    init_header(&msg.header, 0x5103, bridge->config.default_channel, flags);

    msg.vocal_type = vocal_type;
    msg.intensity = intensity;
    msg.urgency = urgency;
    msg.duration_ms = 500.0f;
    msg.is_active = (intensity > 0.0f);
    msg.pitch_modulation = (urgency > 0.5f) ? 0.5f : 0.0f;
    msg.volume_modulation = intensity;
    msg.timestamp_us = msg.header.timestamp_us;

    bridge->stats.vocal_broadcasts++;
    bridge->stats.broadcasts_sent++;
    bridge->stats.messages_sent++;
    bridge->last_broadcast_us = msg.timestamp_us;

    return 0;
}

/* ============================================================================
 * Broadcast API - Autonomic
 * ============================================================================ */

int pag_bio_async_broadcast_autonomic(
    pag_bio_async_bridge_t* bridge,
    float heart_rate_mod,
    float respiratory_mod,
    float muscle_tone
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pag_bio_async_broadcast_autonomic: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }
    if (!bridge->config.enable_autonomic_broadcast) return 0;

    pag_bio_autonomic_msg_t msg = {0};
    init_header(&msg.header, 0x5104, bridge->config.default_channel,
                BIO_MSG_FLAG_BROADCAST);

    msg.heart_rate_mod = heart_rate_mod;
    msg.blood_pressure_mod = heart_rate_mod * 0.8f;
    msg.vasoconstriction = (heart_rate_mod > 0) ? heart_rate_mod * 0.5f : 0.0f;
    msg.respiratory_rate_mod = respiratory_mod;
    msg.respiratory_depth_mod = respiratory_mod * 0.5f;
    msg.apnea_triggered = (respiratory_mod < -0.8f);
    msg.pupil_dilation = (heart_rate_mod > 0) ? heart_rate_mod * 0.7f : 0.0f;
    msg.sweating = (heart_rate_mod > 0.5f) ? heart_rate_mod * 0.4f : 0.0f;
    msg.muscle_tone = muscle_tone;
    msg.tonic_immobility = (muscle_tone < 0.1f && respiratory_mod < -0.5f);
    msg.timestamp_us = msg.header.timestamp_us;

    bridge->stats.autonomic_broadcasts++;
    bridge->stats.broadcasts_sent++;
    bridge->stats.messages_sent++;
    bridge->last_broadcast_us = msg.timestamp_us;

    return 0;
}

/* ============================================================================
 * Broadcast API - Threat Level
 * ============================================================================ */

int pag_bio_async_broadcast_threat(
    pag_bio_async_bridge_t* bridge,
    pag_bio_threat_level_t level,
    float intensity,
    pag_bio_defense_type_t recommended
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pag_bio_async_broadcast_threat: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }
    if (!bridge->config.enable_threat_broadcast) return 0;

    pag_bio_threat_msg_t msg = {0};
    uint32_t flags = BIO_MSG_FLAG_BROADCAST;
    if (level >= PAG_BIO_THREAT_IMMINENT) {
        flags |= BIO_MSG_FLAG_URGENT;
    }

    init_header(&msg.header, 0x5105, bridge->config.defense_channel, flags);

    msg.level = level;
    msg.intensity = intensity;
    msg.recommended = recommended;
    msg.confidence = 0.8f;
    msg.escape_possible = (recommended == PAG_BIO_DEFENSE_FLIGHT);
    msg.fight_viable = (recommended == PAG_BIO_DEFENSE_FIGHT);
    msg.detection_time_us = msg.header.timestamp_us;
    msg.timestamp_us = msg.header.timestamp_us;

    bridge->stats.threat_broadcasts++;
    bridge->stats.broadcasts_sent++;
    bridge->stats.messages_sent++;
    bridge->last_broadcast_us = msg.timestamp_us;

    return 0;
}

/* ============================================================================
 * Subscription Management API
 * ============================================================================ */

int pag_bio_async_subscribe_module(
    pag_bio_async_bridge_t* bridge,
    uint32_t module_id,
    uint32_t msg_types
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pag_bio_async_subscribe_module: bridge is NULL");
        return -1;
    }

    pag_bio_subscription_t* existing = find_subscription(bridge, module_id);
    if (existing) {
        existing->msg_type_mask |= msg_types;
        return 0;
    }

    if (bridge->subscription_count >= bridge->subscription_capacity) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "pag_bio_async_subscribe_module: capacity exceeded");
        return -1;
    }

    pag_bio_subscription_t* sub =
        &bridge->subscriptions[bridge->subscription_count++];
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

int pag_bio_async_unsubscribe_module(
    pag_bio_async_bridge_t* bridge,
    uint32_t module_id
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pag_bio_async_unsubscribe_module: bridge is NULL");
        return -1;
    }

    pag_bio_subscription_t* sub = find_subscription(bridge, module_id);
    if (!sub) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pag_bio_async_unsubscribe_module: sub is NULL");
        return -1;
    }

    sub->active = false;
    bridge->stats.active_subscriptions--;

    return 0;
}

int pag_bio_async_update_subscription(
    pag_bio_async_bridge_t* bridge,
    uint32_t module_id,
    uint32_t msg_types
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pag_bio_async_update_subscription: bridge is NULL");
        return -1;
    }

    pag_bio_subscription_t* sub = find_subscription(bridge, module_id);
    if (!sub) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pag_bio_async_update_subscription: sub is NULL");
        return -1;
    }

    sub->msg_type_mask = msg_types;
    return 0;
}

uint32_t pag_bio_async_get_subscriber_count(
    const pag_bio_async_bridge_t* bridge,
    pag_bio_bridge_msg_type_t msg_type
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

int pag_bio_async_get_stats(
    const pag_bio_async_bridge_t* bridge,
    pag_bio_async_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pag_bio_async_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }

    *stats = bridge->stats;
    return 0;
}

int pag_bio_async_reset_stats(pag_bio_async_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pag_bio_async_reset_stats: bridge is NULL");
        return -1;
    }

    uint32_t active = bridge->stats.active_subscriptions;
    uint32_t peak = bridge->stats.peak_subscriptions;

    memset(&bridge->stats, 0, sizeof(pag_bio_async_stats_t));

    bridge->stats.active_subscriptions = active;
    bridge->stats.peak_subscriptions = peak;

    return 0;
}

const char* pag_bio_msg_type_name(pag_bio_bridge_msg_type_t msg_type) {
    switch (msg_type) {
        case PAG_BIO_MSG_DEFENSE_STATE: return "DEFENSE_STATE";
        case PAG_BIO_MSG_PAIN_MOD:      return "PAIN_MOD";
        case PAG_BIO_MSG_EMOTION:       return "EMOTION";
        case PAG_BIO_MSG_VOCALIZATION:  return "VOCALIZATION";
        case PAG_BIO_MSG_AUTONOMIC:     return "AUTONOMIC";
        case PAG_BIO_MSG_THREAT_LEVEL:  return "THREAT_LEVEL";
        default:                        return "UNKNOWN";
    }
}

void pag_bio_async_print_summary(const pag_bio_async_bridge_t* bridge) {
    if (!bridge) {
        printf("PAG Bio-Async Bridge: NULL\n");
        return;
    }

    printf("=== PAG Bio-Async Bridge Summary ===\n");
    printf("Connected: %s\n", bridge->connected ? "yes" : "no");
    printf("Active subscriptions: %u (peak: %u)\n",
           bridge->stats.active_subscriptions,
           bridge->stats.peak_subscriptions);
    printf("Messages sent: %lu\n", (unsigned long)bridge->stats.messages_sent);
    printf("Broadcasts sent: %lu\n",
           (unsigned long)bridge->stats.broadcasts_sent);
    printf("  Defense: %lu\n",
           (unsigned long)bridge->stats.defense_broadcasts);
    printf("  Pain mod: %lu\n",
           (unsigned long)bridge->stats.pain_mod_broadcasts);
    printf("  Emotion: %lu\n",
           (unsigned long)bridge->stats.emotion_broadcasts);
    printf("  Vocal: %lu\n",
           (unsigned long)bridge->stats.vocal_broadcasts);
    printf("  Autonomic: %lu\n",
           (unsigned long)bridge->stats.autonomic_broadcasts);
    printf("  Threat: %lu\n",
           (unsigned long)bridge->stats.threat_broadcasts);
    printf("Errors: handler=%lu, routing=%lu\n",
           (unsigned long)bridge->stats.handler_errors,
           (unsigned long)bridge->stats.routing_errors);
}
