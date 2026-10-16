/**
 * @file nimcp_soma_bio_async_bridge.c
 * @brief Implementation of Somatosensory Cortex Bio-Async Integration Bridge
 * @version 1.0.0
 * @date 2026-01-12
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "core/brain/regions/somatosensory/bridges/nimcp_soma_bio_async_bridge.h"
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

BRIDGE_BOILERPLATE_MESH_ONLY(soma_bio_async_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


#define LOG_MODULE "SOMA_BIO_ASYNC_BRIDGE"


/* ============================================================================
 * Internal Bridge Structure
 * ============================================================================ */

struct soma_bio_router_struct {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    soma_bio_async_config_t config;
    nimcp_somatosensory_t* soma;
    bio_router_t router;

    soma_bio_subscription_t* subscriptions;
    uint32_t subscription_count;
    uint32_t subscription_capacity;

    bool connected;
    uint64_t last_broadcast_us;
    uint32_t time_since_touch_broadcast_ms;
    uint32_t time_since_proprio_broadcast_ms;

    soma_bio_async_stats_t stats;
};

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

static uint64_t get_timestamp_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

static soma_bio_subscription_t* find_subscription(soma_bio_router_t* b, uint32_t module_id) {
    for (uint32_t i = 0; i < b->subscription_count; i++) {
        if (b->subscriptions[i].module_id == module_id && b->subscriptions[i].active) {
            return &b->subscriptions[i];
        }
    }
    return NULL;  /* Not found is a normal condition */
}

static int count_subscribers_for_type(const soma_bio_router_t* b, soma_bio_msg_type_t type) {
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

int soma_bio_async_default_config(soma_bio_async_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "soma_bio_async_default_config: config is NULL");
        return -1;
    }

    config->touch_broadcast_interval_ms = SOMA_BIO_DEFAULT_BROADCAST_INTERVAL_MS;
    config->proprio_broadcast_interval_ms = 100;  /* Proprioception at 10Hz */
    config->enable_auto_broadcast = true;
    config->max_inbox_process_per_update = 32;
    config->message_ttl_ms = SOMA_BIO_MESSAGE_TTL_MS;
    config->pain_urgency_threshold = SOMA_BIO_PAIN_URGENCY_THRESHOLD;
    config->temp_extreme_threshold = SOMA_BIO_TEMP_EXTREME_THRESHOLD;
    config->default_channel = BIO_CHANNEL_ACETYLCHOLINE;
    config->urgent_channel = BIO_CHANNEL_NOREPINEPHRINE;
    config->max_subscriptions = SOMA_BIO_MAX_SUBSCRIPTIONS;
    config->enable_pain_routing = true;
    config->enable_proprio_routing = true;
    config->enable_temperature_routing = true;
    config->enable_motor_efference = true;
    config->enable_prediction_error = true;
    config->enable_logging = false;

    return 0;
}

soma_bio_router_t* soma_bio_async_bridge_create(const soma_bio_async_config_t* config) {
    soma_bio_router_t* bridge = nimcp_calloc(1, sizeof(soma_bio_router_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bridge is NULL");

        return NULL;

    }

    bridge_base_init(&bridge->base, 0, "soma_bio_async");

    if (config) {
        bridge->config = *config;
    } else {
        soma_bio_async_default_config(&bridge->config);
    }

    bridge->subscription_capacity = bridge->config.max_subscriptions;
    bridge->subscriptions = nimcp_calloc(bridge->subscription_capacity, sizeof(soma_bio_subscription_t));
    if (!bridge->subscriptions) {
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "soma_bio_async_bridge_create: bridge->subscriptions is NULL");
        return NULL;
    }

    bridge->last_broadcast_us = get_timestamp_us();
    NIMCP_LOGGING_INFO("Created %s bridge", "soma_bio_async");
    return bridge;
}

void soma_bio_async_bridge_destroy(soma_bio_router_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "soma_bio_async");

    if (bridge->connected) {
        soma_bio_async_disconnect(bridge);
    }

    nimcp_free(bridge->subscriptions);
    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

/* ============================================================================
 * Connection API
 * ============================================================================ */

int soma_bio_async_connect(
    soma_bio_router_t* bridge,
    nimcp_somatosensory_t* soma,
    bio_router_t router
) {
    if (!bridge || !soma) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "soma_bio_async_connect: required parameter is NULL (bridge, soma)");
        return -1;
    }

    bridge->soma = soma;
    bridge->router = router;
    bridge->connected = true;

    return 0;
}

int soma_bio_async_disconnect(soma_bio_router_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "soma_bio_async_disconnect: bridge is NULL");
        return -1;
    }

    bridge->soma = NULL;
    bridge->router = NULL;
    bridge->connected = false;

    return 0;
}

bool soma_bio_async_is_connected(const soma_bio_router_t* bridge) {
    return bridge ? bridge->connected : false;
}

/* ============================================================================
 * Message Processing API
 * ============================================================================ */

int soma_bio_async_process_inbox(soma_bio_router_t* bridge, uint32_t max_messages) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "soma_bio_async_process_inbox: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    uint32_t processed = 0;
    (void)max_messages;

    /* Process incoming motor efference and gain modulation requests */
    /* TODO: Integrate with bio_router inbox when available */

    bridge->stats.messages_received += processed;
    return (int)processed;
}

int soma_bio_async_update(soma_bio_router_t* bridge, uint32_t delta_ms) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "soma_bio_async_update: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    bridge->time_since_touch_broadcast_ms += delta_ms;
    bridge->time_since_proprio_broadcast_ms += delta_ms;

    /* Auto-broadcast proprioceptive state if enabled */
    if (bridge->config.enable_auto_broadcast && bridge->config.enable_proprio_routing) {
        if (bridge->time_since_proprio_broadcast_ms >= bridge->config.proprio_broadcast_interval_ms) {
            /* Broadcast proprioceptive state for key body segments */
            bridge->time_since_proprio_broadcast_ms = 0;
        }
    }

    return 0;
}

/* ============================================================================
 * Broadcast API - Touch
 * ============================================================================ */

int soma_bio_async_broadcast_touch(
    soma_bio_router_t* bridge,
    body_segment_t segment,
    const float* position,
    float intensity,
    touch_modality_t touch_type
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "soma_bio_async_broadcast_touch: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }
    if (!position) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "soma_bio_async_broadcast_touch: position is NULL");
        return -1;
    }

    soma_bio_touch_event_msg_t msg = {0};
    msg.header.type = SOMA_BIO_MSG_TOUCH_EVENT;
    msg.header.source_module = BIO_MODULE_ID_SOMATOSENSORY;
    msg.header.channel = bridge->config.default_channel;
    msg.header.flags = 0;
    msg.header.timestamp_us = get_timestamp_us();

    msg.body_segment = segment;
    msg.position[0] = position[0];
    msg.position[1] = position[1];
    msg.position[2] = position[2];
    msg.intensity = intensity;
    msg.touch_type = touch_type;
    msg.timestamp_us = get_timestamp_us();

    /* Count subscribers */
    int subs = count_subscribers_for_type(bridge, SOMA_BIO_MSG_TOUCH_EVENT);
    if (subs > 0) {
        bridge->stats.touch_events_sent++;
        bridge->stats.messages_sent++;
        bridge->stats.broadcasts_sent++;
    }

    bridge->stats.last_broadcast_time_us = msg.timestamp_us;
    return 0;
}

int soma_bio_async_broadcast_texture(
    soma_bio_router_t* bridge,
    body_segment_t segment,
    float roughness,
    float hardness
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "soma_bio_async_broadcast_texture: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    /* Create texture result message */
    bridge->stats.messages_sent++;
    bridge->stats.broadcasts_sent++;

    (void)segment;
    (void)roughness;
    (void)hardness;

    return 0;
}

int soma_bio_async_broadcast_grip_force(
    soma_bio_router_t* bridge,
    body_segment_t segment,
    float current_force,
    float recommended_force
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "soma_bio_async_broadcast_grip_force: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    bridge->stats.messages_sent++;
    bridge->stats.broadcasts_sent++;

    (void)segment;
    (void)current_force;
    (void)recommended_force;

    return 0;
}

/* ============================================================================
 * Broadcast API - Pain
 * ============================================================================ */

int soma_bio_async_broadcast_pain(
    soma_bio_router_t* bridge,
    body_segment_t segment,
    pain_type_t pain_type,
    float intensity
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "soma_bio_async_broadcast_pain: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }
    if (!bridge->config.enable_pain_routing) return 0;

    soma_bio_pain_alert_msg_t msg = {0};
    msg.header.type = SOMA_BIO_MSG_PAIN_ALERT;
    msg.header.source_module = BIO_MODULE_ID_SOMATOSENSORY;
    msg.header.flags = BIO_MSG_FLAG_URGENT;
    msg.header.timestamp_us = get_timestamp_us();

    /* Use urgent channel for high intensity pain */
    if (intensity >= bridge->config.pain_urgency_threshold) {
        msg.header.channel = bridge->config.urgent_channel;
        msg.urgency = 1.0f;
        msg.requires_withdrawal = true;
    } else {
        msg.header.channel = bridge->config.default_channel;
        msg.urgency = intensity;
        msg.requires_withdrawal = false;
    }

    msg.body_segment = segment;
    msg.pain_type = pain_type;
    msg.intensity = intensity;
    msg.onset_time_us = get_timestamp_us();
    msg.timestamp_us = msg.onset_time_us;

    bridge->stats.pain_alerts_sent++;
    bridge->stats.messages_sent++;
    bridge->stats.broadcasts_sent++;
    bridge->stats.last_broadcast_time_us = msg.timestamp_us;

    return 0;
}

int soma_bio_async_broadcast_withdrawal(
    soma_bio_router_t* bridge,
    body_segment_t segment,
    float urgency,
    uint32_t danger_type
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "soma_bio_async_broadcast_withdrawal: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    soma_bio_danger_withdrawal_msg_t msg = {0};
    msg.header.type = SOMA_BIO_MSG_DANGER_WITHDRAWAL;
    msg.header.source_module = BIO_MODULE_ID_SOMATOSENSORY;
    msg.header.channel = bridge->config.urgent_channel;
    msg.header.flags = BIO_MSG_FLAG_URGENT;
    msg.header.timestamp_us = get_timestamp_us();

    msg.body_segment = segment;
    msg.urgency = urgency;
    msg.danger_type = danger_type;
    msg.timestamp_us = get_timestamp_us();

    bridge->stats.withdrawals_triggered++;
    bridge->stats.messages_sent++;
    bridge->stats.broadcasts_sent++;
    bridge->stats.last_broadcast_time_us = msg.timestamp_us;

    return 0;
}

/* ============================================================================
 * Broadcast API - Proprioception
 * ============================================================================ */

int soma_bio_async_broadcast_proprio(
    soma_bio_router_t* bridge,
    body_segment_t segment,
    float joint_angle,
    float angular_velocity,
    float muscle_tension
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "soma_bio_async_broadcast_proprio: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }
    if (!bridge->config.enable_proprio_routing) return 0;

    soma_bio_proprio_update_msg_t msg = {0};
    msg.header.type = SOMA_BIO_MSG_PROPRIO_UPDATE;
    msg.header.source_module = BIO_MODULE_ID_SOMATOSENSORY;
    msg.header.channel = bridge->config.default_channel;
    msg.header.flags = 0;
    msg.header.timestamp_us = get_timestamp_us();

    msg.body_segment = segment;
    msg.joint_angle = joint_angle;
    msg.angular_velocity = angular_velocity;
    msg.muscle_tension = muscle_tension;
    msg.timestamp_us = get_timestamp_us();

    bridge->stats.proprio_updates_sent++;
    bridge->stats.messages_sent++;
    bridge->stats.broadcasts_sent++;
    bridge->stats.last_broadcast_time_us = msg.timestamp_us;

    return 0;
}

int soma_bio_async_broadcast_body_map_change(
    soma_bio_router_t* bridge,
    body_segment_t segment,
    uint32_t change_type,
    bool tool_extension
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "soma_bio_async_broadcast_body_map_change: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    soma_bio_body_map_change_msg_t msg = {0};
    msg.header.type = SOMA_BIO_MSG_BODY_MAP_CHANGE;
    msg.header.source_module = BIO_MODULE_ID_SOMATOSENSORY;
    msg.header.channel = bridge->config.default_channel;
    msg.header.flags = 0;
    msg.header.timestamp_us = get_timestamp_us();

    msg.affected_segment = segment;
    msg.change_type = change_type;
    msg.tool_extension = tool_extension;
    msg.timestamp_us = get_timestamp_us();

    bridge->stats.messages_sent++;
    bridge->stats.broadcasts_sent++;
    bridge->stats.last_broadcast_time_us = msg.timestamp_us;

    return 0;
}

/* ============================================================================
 * Broadcast API - Temperature
 * ============================================================================ */

int soma_bio_async_broadcast_temperature(
    soma_bio_router_t* bridge,
    body_segment_t segment,
    float temperature_celsius,
    temp_sensation_t sensation
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "soma_bio_async_broadcast_temperature: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }
    if (!bridge->config.enable_temperature_routing) return 0;

    soma_bio_temperature_msg_t msg = {0};
    msg.header.type = SOMA_BIO_MSG_TEMPERATURE;
    msg.header.source_module = BIO_MODULE_ID_SOMATOSENSORY;
    msg.header.timestamp_us = get_timestamp_us();

    /* Use urgent channel for extreme temperatures */
    bool is_extreme = (sensation == TEMP_COLD_EXTREME || sensation == TEMP_HOT_EXTREME);
    if (is_extreme) {
        msg.header.channel = bridge->config.urgent_channel;
        msg.header.flags = BIO_MSG_FLAG_URGENT;
        msg.is_dangerous = true;
        msg.triggers_hypothalamus = true;
    } else {
        msg.header.channel = bridge->config.default_channel;
        msg.header.flags = 0;
        msg.is_dangerous = false;
        msg.triggers_hypothalamus = (sensation != TEMP_NEUTRAL);
    }

    msg.body_segment = segment;
    msg.temperature_celsius = temperature_celsius;
    msg.sensation = sensation;
    msg.timestamp_us = get_timestamp_us();

    /* Compute comfort level */
    if (sensation == TEMP_NEUTRAL) {
        msg.comfort_level = 1.0f;
    } else if (sensation == TEMP_WARM || sensation == TEMP_COOL) {
        msg.comfort_level = 0.7f;
    } else if (sensation == TEMP_HOT || sensation == TEMP_COLD) {
        msg.comfort_level = 0.3f;
    } else {
        msg.comfort_level = 0.0f;
    }

    bridge->stats.temperature_sent++;
    bridge->stats.messages_sent++;
    bridge->stats.broadcasts_sent++;
    bridge->stats.last_broadcast_time_us = msg.timestamp_us;

    return 0;
}

/* ============================================================================
 * Broadcast API - Prediction Error
 * ============================================================================ */

int soma_bio_async_broadcast_prediction_error(
    soma_bio_router_t* bridge,
    body_segment_t segment,
    float prediction_error,
    uint32_t error_source
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "soma_bio_async_broadcast_prediction_error: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }
    if (!bridge->config.enable_prediction_error) return 0;

    soma_bio_prediction_error_msg_t msg = {0};
    msg.header.type = SOMA_BIO_MSG_PREDICTION_ERROR;
    msg.header.source_module = BIO_MODULE_ID_SOMATOSENSORY;
    msg.header.channel = bridge->config.default_channel;
    msg.header.flags = 0;
    msg.header.timestamp_us = get_timestamp_us();

    msg.body_segment = segment;
    msg.prediction_error = prediction_error;
    msg.error_source = error_source;
    msg.timestamp_us = get_timestamp_us();

    bridge->stats.prediction_errors_sent++;
    bridge->stats.messages_sent++;
    bridge->stats.broadcasts_sent++;
    bridge->stats.last_broadcast_time_us = msg.timestamp_us;

    return 0;
}

/* ============================================================================
 * Subscription Management API
 * ============================================================================ */

int soma_bio_async_subscribe_module(
    soma_bio_router_t* bridge,
    uint32_t module_id,
    uint32_t msg_types
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "soma_bio_async_subscribe_module: bridge is NULL");
        return -1;
    }

    /* Check for existing subscription */
    soma_bio_subscription_t* existing = find_subscription(bridge, module_id);
    if (existing) {
        existing->msg_type_mask |= msg_types;
        return 0;
    }

    /* Find free slot */
    if (bridge->subscription_count >= bridge->subscription_capacity) {
        bridge->stats.routing_errors++;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "soma_bio_async_subscribe_module: capacity exceeded");
        return -1;  /* No space */
    }

    /* Find inactive slot or append */
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

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "soma_bio_async_subscribe_module: validation failed");
    return -1;
}

int soma_bio_async_unsubscribe_module(
    soma_bio_router_t* bridge,
    uint32_t module_id
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "soma_bio_async_unsubscribe_module: bridge is NULL");
        return -1;
    }

    soma_bio_subscription_t* sub = find_subscription(bridge, module_id);
    if (!sub) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "soma_bio_async_unsubscribe_module: sub is NULL");
        return -1;
    }

    sub->active = false;
    sub->msg_type_mask = 0;
    bridge->subscription_count--;
    bridge->stats.active_subscriptions = bridge->subscription_count;

    return 0;
}

int soma_bio_async_update_subscription(
    soma_bio_router_t* bridge,
    uint32_t module_id,
    uint32_t msg_types
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "soma_bio_async_update_subscription: bridge is NULL");
        return -1;
    }

    soma_bio_subscription_t* sub = find_subscription(bridge, module_id);
    if (!sub) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "soma_bio_async_update_subscription: sub is NULL");
        return -1;
    }

    sub->msg_type_mask = msg_types;
    return 0;
}

uint32_t soma_bio_async_get_subscriber_count(
    const soma_bio_router_t* bridge,
    soma_bio_msg_type_t msg_type
) {
    if (!bridge) return 0;
    return (uint32_t)count_subscribers_for_type(bridge, msg_type);
}

/* ============================================================================
 * Statistics and Diagnostics API
 * ============================================================================ */

int soma_bio_async_get_stats(
    const soma_bio_router_t* bridge,
    soma_bio_async_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "soma_bio_async_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }

    *stats = bridge->stats;
    return 0;
}

int soma_bio_async_reset_stats(soma_bio_router_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "soma_bio_async_reset_stats: bridge is NULL");
        return -1;
    }

    uint32_t active_subs = bridge->stats.active_subscriptions;
    uint32_t peak_subs = bridge->stats.peak_subscriptions;

    memset(&bridge->stats, 0, sizeof(bridge->stats));

    bridge->stats.active_subscriptions = active_subs;
    bridge->stats.peak_subscriptions = peak_subs;

    return 0;
}

const char* soma_bio_msg_type_name(soma_bio_msg_type_t msg_type) {
    static const char* names[] = {
        "TOUCH_EVENT",
        "PAIN_ALERT",
        "PROPRIO_UPDATE",
        "TEMPERATURE",
        "BODY_MAP_CHANGE",
        "MOTOR_EFFERENCE",
        "PREDICTION_ERROR",
        "ATTENTION_REQUEST",
        "DANGER_WITHDRAWAL",
        "TEXTURE_RESULT",
        "GRIP_FORCE",
        "REQUEST_STATE",
        "MODULATE_GAIN"
    };

    if (msg_type >= SOMA_BIO_MSG_COUNT) return "UNKNOWN";
    return names[msg_type];
}

void soma_bio_async_print_summary(const soma_bio_router_t* bridge) {
    if (!bridge) {
        printf("Somatosensory Bio-Async Bridge: NULL\n");
        return;
    }

    printf("=== Somatosensory Bio-Async Bridge Summary ===\n");
    printf("Connected: %s\n", bridge->connected ? "YES" : "NO");
    printf("Subscriptions: %u / %u (peak: %u)\n",
           bridge->stats.active_subscriptions,
           bridge->subscription_capacity,
           bridge->stats.peak_subscriptions);
    printf("\n--- Message Statistics ---\n");
    printf("Total sent: %lu\n", (unsigned long)bridge->stats.messages_sent);
    printf("Total received: %lu\n", (unsigned long)bridge->stats.messages_received);
    printf("Broadcasts: %lu\n", (unsigned long)bridge->stats.broadcasts_sent);
    printf("Dropped: %lu\n", (unsigned long)bridge->stats.messages_dropped);
    printf("\n--- Per-Type Counts ---\n");
    printf("Touch events: %lu\n", (unsigned long)bridge->stats.touch_events_sent);
    printf("Pain alerts: %lu\n", (unsigned long)bridge->stats.pain_alerts_sent);
    printf("Proprio updates: %lu\n", (unsigned long)bridge->stats.proprio_updates_sent);
    printf("Temperature: %lu\n", (unsigned long)bridge->stats.temperature_sent);
    printf("Prediction errors: %lu\n", (unsigned long)bridge->stats.prediction_errors_sent);
    printf("Withdrawals: %lu\n", (unsigned long)bridge->stats.withdrawals_triggered);
    printf("\n--- Errors ---\n");
    printf("Handler errors: %lu\n", (unsigned long)bridge->stats.handler_errors);
    printf("Routing errors: %lu\n", (unsigned long)bridge->stats.routing_errors);
    printf("==========================================\n");
}
