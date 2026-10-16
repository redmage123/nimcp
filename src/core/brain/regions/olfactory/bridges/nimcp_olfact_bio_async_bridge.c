/**
 * @file nimcp_olfact_bio_async_bridge.c
 * @brief Implementation of Olfactory Cortex Bio-Async Integration Bridge
 * @version 1.0.0
 * @date 2026-01-12
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "core/brain/regions/olfactory/bridges/nimcp_olfact_bio_async_bridge.h"
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

BRIDGE_BOILERPLATE_MESH_ONLY(olfact_bio_async_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


#define LOG_MODULE "OLFACT_BIO_ASYNC_BRIDGE"


/* ============================================================================
 * Internal Bridge Structure
 * ============================================================================ */

struct olfact_bio_async_bridge_struct {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    olfact_bio_async_config_t config;
    nimcp_olfactory_t* olfact;
    bio_router_t router;

    olfact_bio_subscription_t* subscriptions;
    uint32_t subscription_count;
    uint32_t subscription_capacity;

    bool connected;
    uint64_t last_broadcast_us;
    uint32_t time_since_broadcast_ms;
    uint32_t current_odor_id;

    olfact_bio_async_stats_t stats;
};

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

static uint64_t get_timestamp_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

static olfact_bio_subscription_t* find_subscription(olfact_bio_async_bridge_t* b, uint32_t module_id) {
    for (uint32_t i = 0; i < b->subscription_count; i++) {
        if (b->subscriptions[i].module_id == module_id && b->subscriptions[i].active) {
            return &b->subscriptions[i];
        }
    }
    return NULL;  /* Not found is a normal condition */
}

static int count_subscribers_for_type(const olfact_bio_async_bridge_t* b, olfact_bio_msg_type_t type) {
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

int olfact_bio_async_default_config(olfact_bio_async_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "olfact_bio_async_default_config: config is NULL");
        return -1;
    }

    config->odor_broadcast_interval_ms = OLFACT_BIO_DEFAULT_BROADCAST_INTERVAL_MS;
    config->enable_auto_broadcast = true;
    config->max_inbox_process_per_update = 32;
    config->message_ttl_ms = OLFACT_BIO_MESSAGE_TTL_MS;
    config->danger_odor_threshold = OLFACT_BIO_DANGER_THRESHOLD;
    config->food_salience_threshold = OLFACT_BIO_FOOD_THRESHOLD;
    config->default_channel = BIO_CHANNEL_ACETYLCHOLINE;
    config->urgent_channel = BIO_CHANNEL_NOREPINEPHRINE;
    config->max_subscriptions = OLFACT_BIO_MAX_SUBSCRIPTIONS;
    config->enable_memory_triggers = true;
    config->enable_food_signals = true;
    config->enable_danger_alerts = true;
    config->enable_hedonic_routing = true;
    config->enable_logging = false;

    return 0;
}

olfact_bio_async_bridge_t* olfact_bio_async_bridge_create(const olfact_bio_async_config_t* config) {
    olfact_bio_async_bridge_t* bridge = nimcp_calloc(1, sizeof(olfact_bio_async_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bridge is NULL");

        return NULL;

    }

    bridge_base_init(&bridge->base, 0, "olfact_bio_async");

    if (config) {
        bridge->config = *config;
    } else {
        olfact_bio_async_default_config(&bridge->config);
    }

    bridge->subscription_capacity = bridge->config.max_subscriptions;
    bridge->subscriptions = nimcp_calloc(bridge->subscription_capacity, sizeof(olfact_bio_subscription_t));
    if (!bridge->subscriptions) {
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "olfact_bio_async_bridge_create: bridge->subscriptions is NULL");
        return NULL;
    }

    bridge->last_broadcast_us = get_timestamp_us();
    bridge->current_odor_id = 1;
    NIMCP_LOGGING_INFO("Created %s bridge", "olfact_bio_async");
    return bridge;
}

void olfact_bio_async_bridge_destroy(olfact_bio_async_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "olfact_bio_async");

    if (bridge->connected) {
        olfact_bio_async_disconnect(bridge);
    }

    nimcp_free(bridge->subscriptions);
    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

/* ============================================================================
 * Connection API
 * ============================================================================ */

int olfact_bio_async_connect(
    olfact_bio_async_bridge_t* bridge,
    nimcp_olfactory_t* olfact,
    bio_router_t router
) {
    if (!bridge || !olfact) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "olfact_bio_async_connect: required parameter is NULL (bridge, olfact)");
        return -1;
    }

    bridge->olfact = olfact;
    bridge->router = router;
    bridge->connected = true;

    return 0;
}

int olfact_bio_async_disconnect(olfact_bio_async_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "olfact_bio_async_disconnect: bridge is NULL");
        return -1;
    }

    bridge->olfact = NULL;
    bridge->router = NULL;
    bridge->connected = false;

    return 0;
}

bool olfact_bio_async_is_connected(const olfact_bio_async_bridge_t* bridge) {
    return bridge ? bridge->connected : false;
}

/* ============================================================================
 * Message Processing API
 * ============================================================================ */

int olfact_bio_async_process_inbox(olfact_bio_async_bridge_t* bridge, uint32_t max_messages) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "olfact_bio_async_process_inbox: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    uint32_t processed = 0;
    (void)max_messages;

    bridge->stats.messages_received += processed;
    return (int)processed;
}

int olfact_bio_async_update(olfact_bio_async_bridge_t* bridge, uint32_t delta_ms) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "olfact_bio_async_update: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    bridge->time_since_broadcast_ms += delta_ms;

    return 0;
}

/* ============================================================================
 * Broadcast API
 * ============================================================================ */

int olfact_bio_async_broadcast_odor_detected(
    olfact_bio_async_bridge_t* bridge,
    float intensity,
    bool is_novel
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "olfact_bio_async_broadcast_odor_detected: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    olfact_bio_odor_detected_msg_t msg = {0};
    msg.header.type = OLFACT_BIO_MSG_ODOR_DETECTED;
    msg.header.source_module = BIO_MODULE_ID_OLFACTORY;
    msg.header.channel = bridge->config.default_channel;
    msg.header.flags = is_novel ? BIO_MSG_FLAG_URGENT : 0;
    msg.header.timestamp_us = get_timestamp_us();

    msg.intensity = intensity;
    msg.is_novel = is_novel;
    msg.is_salient = (intensity > 0.7f) || is_novel;
    msg.odor_id = bridge->current_odor_id++;
    msg.timestamp_us = get_timestamp_us();

    bridge->stats.odors_detected++;
    bridge->stats.messages_sent++;
    bridge->stats.broadcasts_sent++;
    bridge->stats.last_broadcast_time_us = msg.timestamp_us;

    return 0;
}

int olfact_bio_async_broadcast_odor_identified(
    olfact_bio_async_bridge_t* bridge,
    const char* name,
    odor_category_t category,
    float confidence
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "olfact_bio_async_broadcast_odor_identified: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    olfact_bio_odor_identified_msg_t msg = {0};
    msg.header.type = OLFACT_BIO_MSG_ODOR_IDENTIFIED;
    msg.header.source_module = BIO_MODULE_ID_OLFACTORY;
    msg.header.channel = bridge->config.default_channel;
    msg.header.flags = 0;
    msg.header.timestamp_us = get_timestamp_us();

    if (name) {
        strncpy(msg.odor_name, name, sizeof(msg.odor_name) - 1);
    }
    msg.category = category;
    msg.confidence = confidence;
    msg.timestamp_us = get_timestamp_us();

    bridge->stats.odors_identified++;
    bridge->stats.messages_sent++;
    bridge->stats.broadcasts_sent++;
    bridge->stats.last_broadcast_time_us = msg.timestamp_us;

    return 0;
}

int olfact_bio_async_broadcast_hedonic(
    olfact_bio_async_bridge_t* bridge,
    hedonic_valence_t valence,
    float score
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "olfact_bio_async_broadcast_hedonic: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }
    if (!bridge->config.enable_hedonic_routing) return 0;

    olfact_bio_hedonic_value_msg_t msg = {0};
    msg.header.type = OLFACT_BIO_MSG_HEDONIC_VALUE;
    msg.header.source_module = BIO_MODULE_ID_OLFACTORY;
    msg.header.channel = bridge->config.default_channel;
    msg.header.flags = 0;
    msg.header.timestamp_us = get_timestamp_us();

    msg.valence = valence;
    msg.hedonic_score = score;

    /* Set emotional flags based on valence */
    msg.triggers_disgust = (valence == HEDONIC_VERY_UNPLEASANT);
    msg.triggers_attraction = (valence == HEDONIC_VERY_PLEASANT || valence == HEDONIC_PLEASANT);
    msg.triggers_fear = false;  /* Set by context */

    msg.timestamp_us = get_timestamp_us();

    bridge->stats.hedonic_sent++;
    bridge->stats.messages_sent++;
    bridge->stats.broadcasts_sent++;
    bridge->stats.last_broadcast_time_us = msg.timestamp_us;

    return 0;
}

int olfact_bio_async_broadcast_memory_trigger(
    olfact_bio_async_bridge_t* bridge,
    uint32_t memory_id,
    float strength
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "olfact_bio_async_broadcast_memory_trigger: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }
    if (!bridge->config.enable_memory_triggers) return 0;

    olfact_bio_memory_trigger_msg_t msg = {0};
    msg.header.type = OLFACT_BIO_MSG_MEMORY_TRIGGER;
    msg.header.source_module = BIO_MODULE_ID_OLFACTORY;
    msg.header.channel = bridge->config.default_channel;
    msg.header.flags = (strength > 0.8f) ? BIO_MSG_FLAG_URGENT : 0;
    msg.header.timestamp_us = get_timestamp_us();

    msg.memory_id = memory_id;
    msg.memory_strength = strength;
    msg.is_episodic = true;
    msg.timestamp_us = get_timestamp_us();

    bridge->stats.memory_triggers_sent++;
    bridge->stats.messages_sent++;
    bridge->stats.broadcasts_sent++;
    bridge->stats.last_broadcast_time_us = msg.timestamp_us;

    return 0;
}

int olfact_bio_async_broadcast_food_signal(
    olfact_bio_async_bridge_t* bridge,
    float salience,
    bool is_sweet,
    bool is_savory
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "olfact_bio_async_broadcast_food_signal: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }
    if (!bridge->config.enable_food_signals) return 0;

    olfact_bio_food_signal_msg_t msg = {0};
    msg.header.type = OLFACT_BIO_MSG_FOOD_SIGNAL;
    msg.header.source_module = BIO_MODULE_ID_OLFACTORY;
    msg.header.channel = bridge->config.default_channel;
    msg.header.flags = 0;
    msg.header.timestamp_us = get_timestamp_us();

    msg.food_salience = salience;
    msg.appetite_stimulation = salience * 0.8f;
    msg.is_sweet = is_sweet;
    msg.is_savory = is_savory;
    msg.is_spoiled = false;
    msg.timestamp_us = get_timestamp_us();

    bridge->stats.food_signals_sent++;
    bridge->stats.messages_sent++;
    bridge->stats.broadcasts_sent++;
    bridge->stats.last_broadcast_time_us = msg.timestamp_us;

    return 0;
}

int olfact_bio_async_broadcast_danger_odor(
    olfact_bio_async_bridge_t* bridge,
    uint32_t danger_type,
    float intensity
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "olfact_bio_async_broadcast_danger_odor: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }
    if (!bridge->config.enable_danger_alerts) return 0;

    olfact_bio_danger_odor_msg_t msg = {0};
    msg.header.type = OLFACT_BIO_MSG_DANGER_ODOR;
    msg.header.source_module = BIO_MODULE_ID_OLFACTORY;
    msg.header.channel = bridge->config.urgent_channel;
    msg.header.flags = BIO_MSG_FLAG_URGENT;
    msg.header.timestamp_us = get_timestamp_us();

    msg.danger_type = danger_type;
    msg.danger_intensity = intensity;
    msg.urgency = intensity;

    /* Decode danger type */
    msg.is_smoke = (danger_type == 1);
    msg.is_gas = (danger_type == 2);
    msg.is_chemical = (danger_type == 3);
    msg.is_decay = (danger_type == 4);

    msg.timestamp_us = get_timestamp_us();

    bridge->stats.danger_alerts_sent++;
    bridge->stats.messages_sent++;
    bridge->stats.broadcasts_sent++;
    bridge->stats.last_broadcast_time_us = msg.timestamp_us;

    return 0;
}

int olfact_bio_async_broadcast_sniff_cycle(
    olfact_bio_async_bridge_t* bridge,
    sniff_phase_t phase,
    float strength
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "olfact_bio_async_broadcast_sniff_cycle: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    olfact_bio_sniff_cycle_msg_t msg = {0};
    msg.header.type = OLFACT_BIO_MSG_SNIFF_CYCLE;
    msg.header.source_module = BIO_MODULE_ID_OLFACTORY;
    msg.header.channel = bridge->config.default_channel;
    msg.header.flags = 0;
    msg.header.timestamp_us = get_timestamp_us();

    msg.phase = phase;
    msg.sniff_strength = strength;
    msg.timestamp_us = get_timestamp_us();

    bridge->stats.messages_sent++;
    bridge->stats.broadcasts_sent++;
    bridge->stats.last_broadcast_time_us = msg.timestamp_us;

    return 0;
}

int olfact_bio_async_broadcast_adaptation(
    olfact_bio_async_bridge_t* bridge,
    float level,
    bool fully_adapted
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "olfact_bio_async_broadcast_adaptation: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    olfact_bio_adaptation_msg_t msg = {0};
    msg.header.type = OLFACT_BIO_MSG_ADAPTATION;
    msg.header.source_module = BIO_MODULE_ID_OLFACTORY;
    msg.header.channel = bridge->config.default_channel;
    msg.header.flags = 0;
    msg.header.timestamp_us = get_timestamp_us();

    msg.adaptation_level = level;
    msg.fully_adapted = fully_adapted;
    msg.time_constant_ms = 2000.0f;  /* Standard olfactory adaptation tau */
    msg.timestamp_us = get_timestamp_us();

    bridge->stats.messages_sent++;
    bridge->stats.broadcasts_sent++;
    bridge->stats.last_broadcast_time_us = msg.timestamp_us;

    return 0;
}

/* ============================================================================
 * Subscription API
 * ============================================================================ */

int olfact_bio_async_subscribe_module(
    olfact_bio_async_bridge_t* bridge,
    uint32_t module_id,
    uint32_t msg_types
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "olfact_bio_async_subscribe_module: bridge is NULL");
        return -1;
    }

    olfact_bio_subscription_t* existing = find_subscription(bridge, module_id);
    if (existing) {
        existing->msg_type_mask |= msg_types;
        return 0;
    }

    if (bridge->subscription_count >= bridge->subscription_capacity) {
        bridge->stats.routing_errors++;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "olfact_bio_async_subscribe_module: capacity exceeded");
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

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "olfact_bio_async_subscribe_module: validation failed");
    return -1;
}

int olfact_bio_async_unsubscribe_module(
    olfact_bio_async_bridge_t* bridge,
    uint32_t module_id
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "olfact_bio_async_unsubscribe_module: bridge is NULL");
        return -1;
    }

    olfact_bio_subscription_t* sub = find_subscription(bridge, module_id);
    if (!sub) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "olfact_bio_async_unsubscribe_module: sub is NULL");
        return -1;
    }

    sub->active = false;
    sub->msg_type_mask = 0;
    bridge->subscription_count--;
    bridge->stats.active_subscriptions = bridge->subscription_count;

    return 0;
}

uint32_t olfact_bio_async_get_subscriber_count(
    const olfact_bio_async_bridge_t* bridge,
    olfact_bio_msg_type_t type
) {
    if (!bridge) return 0;
    return (uint32_t)count_subscribers_for_type(bridge, type);
}

/* ============================================================================
 * Statistics API
 * ============================================================================ */

int olfact_bio_async_get_stats(
    const olfact_bio_async_bridge_t* bridge,
    olfact_bio_async_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "olfact_bio_async_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }
    *stats = bridge->stats;
    return 0;
}

int olfact_bio_async_reset_stats(olfact_bio_async_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "olfact_bio_async_reset_stats: bridge is NULL");
        return -1;
    }

    uint32_t active_subs = bridge->stats.active_subscriptions;
    uint32_t peak_subs = bridge->stats.peak_subscriptions;

    memset(&bridge->stats, 0, sizeof(bridge->stats));

    bridge->stats.active_subscriptions = active_subs;
    bridge->stats.peak_subscriptions = peak_subs;

    return 0;
}

const char* olfact_bio_msg_type_name(olfact_bio_msg_type_t msg_type) {
    static const char* names[] = {
        "ODOR_DETECTED",
        "ODOR_IDENTIFIED",
        "HEDONIC_VALUE",
        "MEMORY_TRIGGER",
        "SNIFF_CYCLE",
        "FOOD_SIGNAL",
        "DANGER_ODOR",
        "ADAPTATION",
        "CONCENTRATION",
        "FAMILIARITY",
        "REQUEST_STATE",
        "MODULATE_ATTENTION"
    };

    if (msg_type >= OLFACT_BIO_MSG_COUNT) return "UNKNOWN";
    return names[msg_type];
}

void olfact_bio_async_print_summary(const olfact_bio_async_bridge_t* bridge) {
    if (!bridge) {
        printf("Olfactory Bio-Async Bridge: NULL\n");
        return;
    }

    printf("=== Olfactory Bio-Async Bridge Summary ===\n");
    printf("Connected: %s\n", bridge->connected ? "YES" : "NO");
    printf("Subscriptions: %u / %u (peak: %u)\n",
           bridge->stats.active_subscriptions,
           bridge->subscription_capacity,
           bridge->stats.peak_subscriptions);
    printf("\n--- Message Statistics ---\n");
    printf("Total sent: %lu\n", (unsigned long)bridge->stats.messages_sent);
    printf("Broadcasts: %lu\n", (unsigned long)bridge->stats.broadcasts_sent);
    printf("\n--- Per-Type Counts ---\n");
    printf("Odors detected: %lu\n", (unsigned long)bridge->stats.odors_detected);
    printf("Odors identified: %lu\n", (unsigned long)bridge->stats.odors_identified);
    printf("Hedonic signals: %lu\n", (unsigned long)bridge->stats.hedonic_sent);
    printf("Memory triggers: %lu\n", (unsigned long)bridge->stats.memory_triggers_sent);
    printf("Food signals: %lu\n", (unsigned long)bridge->stats.food_signals_sent);
    printf("Danger alerts: %lu\n", (unsigned long)bridge->stats.danger_alerts_sent);
    printf("==========================================\n");
}
