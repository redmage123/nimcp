/**
 * @file nimcp_rn_bio_async_bridge.c
 * @brief Implementation of Red Nucleus Bio-Async Integration Bridge
 * @version 1.0.0
 * @date 2026-01-13
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "core/brain/regions/red_nucleus/bridges/nimcp_rn_bio_async_bridge.h"
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

BRIDGE_BOILERPLATE_MESH_ONLY(rn_bio_async_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


#define LOG_MODULE "RN_BIO_ASYNC_BRIDGE"


/* ============================================================================
 * Internal Bridge Structure
 * ============================================================================ */

struct rn_bio_bridge_struct {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    rn_bio_bridge_config_t config;
    nimcp_red_nucleus_t* rn;
    bio_router_t router;

    rn_bio_bridge_subscription_t* subscriptions;
    uint32_t subscription_count;
    uint32_t subscription_capacity;

    bool connected;
    uint64_t last_broadcast_us;
    uint32_t time_since_broadcast_ms;
    uint64_t last_learning_broadcast_us;

    rn_bio_bridge_stats_t stats;
};

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

static uint64_t rn_bridge_get_timestamp_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

static rn_bio_bridge_subscription_t* rn_bridge_find_subscription(
    rn_bio_bridge_t* b,
    uint32_t module_id
) {
    for (uint32_t i = 0; i < b->subscription_count; i++) {
        if (b->subscriptions[i].module_id == module_id &&
            b->subscriptions[i].active) {
            return &b->subscriptions[i];
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rn_bridge_find_subscription: operation failed");
    return NULL;
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

int rn_bio_bridge_default_config(rn_bio_bridge_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rn_bio_bridge_default_config: config is NULL");
        return -1;
    }

    config->broadcast_interval_ms = RN_BIO_BRIDGE_DEFAULT_INTERVAL_MS;
    config->enable_auto_broadcast = true;
    config->enable_error_broadcast = true;
    config->max_inbox_process_per_update = 32;
    config->message_ttl_ms = RN_BIO_BRIDGE_MESSAGE_TTL_MS;
    config->error_learning_threshold = RN_BIO_BRIDGE_ERROR_THRESHOLD;
    config->default_channel = BIO_CHANNEL_ACETYLCHOLINE;
    config->motor_channel = BIO_CHANNEL_ACETYLCHOLINE;
    config->max_subscriptions = RN_BIO_BRIDGE_MAX_SUBSCRIPTIONS;
    config->enable_learning_broadcast = true;
    config->enable_coordination_broadcast = true;
    config->enable_posture_broadcast = true;
    config->enable_cerebellar_broadcast = true;
    config->enable_logging = false;

    return 0;
}

rn_bio_bridge_t* rn_bio_bridge_create(const rn_bio_bridge_config_t* config) {
    rn_bio_bridge_t* bridge = nimcp_calloc(1, sizeof(rn_bio_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bridge is NULL");

        return NULL;

    }

    if (config) {
        bridge->config = *config;
    } else {
        rn_bio_bridge_default_config(&bridge->config);
    }

    bridge->subscription_capacity = bridge->config.max_subscriptions;
    bridge->subscriptions = nimcp_calloc(bridge->subscription_capacity,
                                         sizeof(rn_bio_bridge_subscription_t));
    if (!bridge->subscriptions) {
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "rn_bio_bridge_create: bridge->subscriptions is NULL");
        return NULL;
    }

    bridge->last_broadcast_us = rn_bridge_get_timestamp_us();
    bridge->last_learning_broadcast_us = bridge->last_broadcast_us;
    NIMCP_LOGGING_INFO("Created %s bridge", "rn_bio_async");
    return bridge;
}

void rn_bio_bridge_destroy(rn_bio_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "rn_bio_async");

    if (bridge->connected) {
        rn_bio_bridge_disconnect(bridge);
    }

    nimcp_free(bridge->subscriptions);
    nimcp_free(bridge);
}

/* ============================================================================
 * Connection API
 * ============================================================================ */

int rn_bio_bridge_connect(
    rn_bio_bridge_t* bridge,
    nimcp_red_nucleus_t* rn,
    bio_router_t router
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rn_bio_bridge_connect: bridge is NULL");
        return -1;
    }
    if (!rn) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rn_bio_bridge_connect: rn is NULL");
        return -1;
    }

    bridge->rn = rn;
    bridge->router = router;
    bridge->connected = true;

    return 0;
}

int rn_bio_bridge_disconnect(rn_bio_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rn_bio_bridge_disconnect: bridge is NULL");
        return -1;
    }

    bridge->rn = NULL;
    bridge->router = NULL;
    bridge->connected = false;

    return 0;
}

bool rn_bio_bridge_is_connected(const rn_bio_bridge_t* bridge) {
    return bridge ? bridge->connected : false;
}

/* ============================================================================
 * Message Processing API
 * ============================================================================ */

int rn_bio_bridge_process_inbox(rn_bio_bridge_t* bridge, uint32_t max_messages) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rn_bio_bridge_process_inbox: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    uint32_t processed = 0;
    (void)max_messages;

    bridge->stats.messages_received += processed;
    return (int)processed;
}

int rn_bio_bridge_update(rn_bio_bridge_t* bridge, uint32_t delta_ms) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rn_bio_bridge_update: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    bridge->time_since_broadcast_ms += delta_ms;

    /* Auto-broadcast if enabled and interval elapsed */
    if (bridge->config.enable_auto_broadcast &&
        bridge->time_since_broadcast_ms >= bridge->config.broadcast_interval_ms) {

        bridge->time_since_broadcast_ms = 0;
        bridge->last_broadcast_us = rn_bridge_get_timestamp_us();
    }

    return 0;
}

/* ============================================================================
 * Broadcast API
 * ============================================================================ */

int rn_bio_bridge_broadcast_motor_cmd(
    rn_bio_bridge_t* bridge,
    uint32_t command_type,
    uint32_t effector_id,
    float magnitude,
    float value_x,
    float value_y,
    float value_z
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rn_bio_bridge_broadcast_motor_cmd: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    rn_bio_bridge_motor_cmd_msg_t msg = {0};
    msg.header.type = BIO_MSG_MOTOR_ACTION_EXECUTED;
    msg.header.source_module = RN_BIO_BRIDGE_MODULE_ID;
    msg.header.channel = bridge->config.motor_channel;
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;
    msg.header.timestamp_us = rn_bridge_get_timestamp_us();

    msg.command_type = command_type;
    msg.effector_id = effector_id;
    msg.magnitude = magnitude;
    msg.value_x = value_x;
    msg.value_y = value_y;
    msg.value_z = value_z;
    msg.timestamp_us = msg.header.timestamp_us;

    bridge->stats.motor_cmd_broadcasts++;
    bridge->stats.broadcasts_sent++;
    bridge->last_broadcast_us = msg.timestamp_us;

    return 0;
}

int rn_bio_bridge_broadcast_error(
    rn_bio_bridge_t* bridge,
    uint32_t error_type,
    uint32_t effector_id,
    float error_magnitude,
    float error_x,
    float error_y,
    float error_z
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rn_bio_bridge_broadcast_error: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    rn_bio_bridge_error_msg_t msg = {0};
    msg.header.type = BIO_MSG_ERROR_DETECTED;
    msg.header.source_module = RN_BIO_BRIDGE_MODULE_ID;
    msg.header.channel = bridge->config.default_channel;
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;
    msg.header.timestamp_us = rn_bridge_get_timestamp_us();

    msg.error_type = error_type;
    msg.effector_id = effector_id;
    msg.error_magnitude = error_magnitude;
    msg.error_x = error_x;
    msg.error_y = error_y;
    msg.error_z = error_z;

    /* Check if this triggers learning */
    float threshold = bridge->config.error_learning_threshold;
    if (error_magnitude > threshold || error_magnitude < -threshold) {
        msg.triggers_learning = true;
    }

    msg.timestamp_us = msg.header.timestamp_us;

    bridge->stats.error_broadcasts++;
    bridge->stats.broadcasts_sent++;

    return 0;
}

int rn_bio_bridge_broadcast_learning(
    rn_bio_bridge_t* bridge,
    uint32_t effector_id,
    float learning_rate,
    float skill_level,
    float avg_error
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rn_bio_bridge_broadcast_learning: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }
    if (!bridge->config.enable_learning_broadcast) return 0;

    /* Rate limit learning broadcasts */
    uint64_t now = rn_bridge_get_timestamp_us();
    uint64_t elapsed_ms = (now - bridge->last_learning_broadcast_us) / 1000;
    if (elapsed_ms < RN_BIO_BRIDGE_LEARNING_MIN_MS) return 0;

    rn_bio_bridge_learning_msg_t msg = {0};
    msg.header.type = BIO_MSG_LEARNING_RATE_UPDATE;
    msg.header.source_module = RN_BIO_BRIDGE_MODULE_ID;
    msg.header.channel = bridge->config.default_channel;
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;
    msg.header.timestamp_us = now;

    msg.effector_id = effector_id;
    msg.learning_rate = learning_rate;
    msg.skill_level = skill_level;
    msg.avg_error = avg_error;
    msg.timestamp_us = now;

    bridge->stats.learning_broadcasts++;
    bridge->stats.broadcasts_sent++;
    bridge->last_learning_broadcast_us = now;

    return 0;
}

int rn_bio_bridge_broadcast_limb_coord(
    rn_bio_bridge_t* bridge,
    uint32_t primary_effector,
    uint32_t secondary_effector,
    float coordination_strength,
    float synchrony
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rn_bio_bridge_broadcast_limb_coord: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }
    if (!bridge->config.enable_coordination_broadcast) return 0;

    rn_bio_bridge_limb_coord_msg_t msg = {0};
    msg.header.type = BIO_MSG_MOTOR_ACTION_EXECUTED;
    msg.header.source_module = RN_BIO_BRIDGE_MODULE_ID;
    msg.header.channel = bridge->config.motor_channel;
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;
    msg.header.timestamp_us = rn_bridge_get_timestamp_us();

    msg.primary_effector = primary_effector;
    msg.secondary_effector = secondary_effector;
    msg.coordination_strength = coordination_strength;
    msg.synchrony = synchrony;
    msg.timestamp_us = msg.header.timestamp_us;

    bridge->stats.coordination_broadcasts++;
    bridge->stats.broadcasts_sent++;

    return 0;
}

int rn_bio_bridge_broadcast_posture(
    rn_bio_bridge_t* bridge,
    float adjustment_x,
    float adjustment_y,
    float adjustment_z,
    float urgency
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rn_bio_bridge_broadcast_posture: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }
    if (!bridge->config.enable_posture_broadcast) return 0;

    rn_bio_bridge_posture_msg_t msg = {0};
    msg.header.type = BIO_MSG_MOTOR_ACTION_EXECUTED;
    msg.header.source_module = RN_BIO_BRIDGE_MODULE_ID;
    msg.header.channel = bridge->config.motor_channel;
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;
    if (urgency > 0.8f) {
        msg.header.flags |= BIO_MSG_FLAG_URGENT;
    }
    msg.header.timestamp_us = rn_bridge_get_timestamp_us();

    msg.adjustment_x = adjustment_x;
    msg.adjustment_y = adjustment_y;
    msg.adjustment_z = adjustment_z;
    msg.urgency = urgency;
    msg.timestamp_us = msg.header.timestamp_us;

    bridge->stats.posture_broadcasts++;
    bridge->stats.broadcasts_sent++;

    return 0;
}

int rn_bio_bridge_broadcast_cerebellar(
    rn_bio_bridge_t* bridge,
    float dentate_activity,
    float correction_x,
    float correction_y,
    float correction_z
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rn_bio_bridge_broadcast_cerebellar: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }
    if (!bridge->config.enable_cerebellar_broadcast) return 0;

    rn_bio_bridge_cerebellar_msg_t msg = {0};
    msg.header.type = BIO_MSG_MOTOR_ACTION_EXECUTED;
    msg.header.source_module = RN_BIO_BRIDGE_MODULE_ID;
    msg.header.channel = bridge->config.default_channel;
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;
    msg.header.timestamp_us = rn_bridge_get_timestamp_us();

    msg.dentate_activity = dentate_activity;
    msg.correction_x = correction_x;
    msg.correction_y = correction_y;
    msg.correction_z = correction_z;
    msg.timestamp_us = msg.header.timestamp_us;

    bridge->stats.cerebellar_broadcasts++;
    bridge->stats.broadcasts_sent++;

    return 0;
}

/* ============================================================================
 * Subscription Management API
 * ============================================================================ */

int rn_bio_bridge_subscribe_module(
    rn_bio_bridge_t* bridge,
    uint32_t module_id,
    uint32_t msg_types
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rn_bio_bridge_subscribe_module: bridge is NULL");
        return -1;
    }

    /* Check if already subscribed */
    rn_bio_bridge_subscription_t* existing;
    existing = rn_bridge_find_subscription(bridge, module_id);
    if (existing) {
        existing->msg_type_mask |= msg_types;
        return 0;
    }

    /* Find free slot */
    if (bridge->subscription_count >= bridge->subscription_capacity) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "rn_bio_bridge_subscribe_module: capacity exceeded");
        return -1;
    }

    rn_bio_bridge_subscription_t* sub;
    sub = &bridge->subscriptions[bridge->subscription_count++];
    sub->module_id = module_id;
    sub->msg_type_mask = msg_types;
    sub->active = true;
    sub->subscription_time = rn_bridge_get_timestamp_us();
    sub->messages_sent = 0;

    bridge->stats.active_subscriptions = bridge->subscription_count;
    if (bridge->subscription_count > bridge->stats.peak_subscriptions) {
        bridge->stats.peak_subscriptions = bridge->subscription_count;
    }

    return 0;
}

int rn_bio_bridge_unsubscribe_module(rn_bio_bridge_t* bridge, uint32_t module_id) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rn_bio_bridge_unsubscribe_module: bridge is NULL");
        return -1;
    }

    for (uint32_t i = 0; i < bridge->subscription_count; i++) {
        if (bridge->subscriptions[i].module_id == module_id) {
            bridge->subscriptions[i].active = false;
            bridge->stats.active_subscriptions--;
            return 0;
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "rn_bio_bridge_unsubscribe_module: validation failed");
    return -1;
}

int rn_bio_bridge_update_subscription(
    rn_bio_bridge_t* bridge,
    uint32_t module_id,
    uint32_t msg_types
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rn_bio_bridge_update_subscription: bridge is NULL");
        return -1;
    }

    rn_bio_bridge_subscription_t* sub;
    sub = rn_bridge_find_subscription(bridge, module_id);
    if (!sub) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rn_bio_bridge_update_subscription: sub is NULL");
        return -1;
    }

    sub->msg_type_mask = msg_types;
    return 0;
}

uint32_t rn_bio_bridge_get_subscriber_count(
    const rn_bio_bridge_t* bridge,
    rn_bio_msg_type_t msg_type
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

int rn_bio_bridge_get_stats(
    const rn_bio_bridge_t* bridge,
    rn_bio_bridge_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rn_bio_bridge_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }
    *stats = bridge->stats;
    return 0;
}

int rn_bio_bridge_reset_stats(rn_bio_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rn_bio_bridge_reset_stats: bridge is NULL");
        return -1;
    }
    memset(&bridge->stats, 0, sizeof(rn_bio_bridge_stats_t));
    return 0;
}

static const char* rn_bridge_msg_type_names[] = {
    "MOTOR_CMD",
    "ERROR_SIGNAL",
    "LEARNING_UPDATE",
    "CEREBELLAR_INPUT",
    "OLIVARY_OUTPUT",
    "THALAMIC_OUTPUT",
    "POSTURE_ADJUST",
    "STATE_REQUEST"
};

const char* rn_bio_bridge_msg_type_name(rn_bio_msg_type_t msg_type) {
    if (msg_type >= RN_BIO_MSG_COUNT) return "UNKNOWN";
    return rn_bridge_msg_type_names[msg_type];
}

void rn_bio_bridge_print_summary(const rn_bio_bridge_t* bridge) {
    if (!bridge) {
        printf("RN Bio-Async Bridge: NULL\n");
        return;
    }

    printf("=== Red Nucleus Bio-Async Bridge Summary ===\n");
    printf("Connected: %s\n", bridge->connected ? "Yes" : "No");
    printf("Module ID: 0x%04X\n", RN_BIO_BRIDGE_MODULE_ID);
    printf("Subscriptions: %u/%u\n",
           bridge->stats.active_subscriptions, bridge->subscription_capacity);
    printf("Messages sent: %lu\n", (unsigned long)bridge->stats.messages_sent);
    printf("Broadcasts: %lu\n", (unsigned long)bridge->stats.broadcasts_sent);
    printf("  Motor Cmd: %lu\n", (unsigned long)bridge->stats.motor_cmd_broadcasts);
    printf("  Error: %lu\n", (unsigned long)bridge->stats.error_broadcasts);
    printf("  Learning: %lu\n", (unsigned long)bridge->stats.learning_broadcasts);
    printf("  Coord: %lu\n", (unsigned long)bridge->stats.coordination_broadcasts);
    printf("  Posture: %lu\n", (unsigned long)bridge->stats.posture_broadcasts);
    printf("  Cerebellar: %lu\n", (unsigned long)bridge->stats.cerebellar_broadcasts);
    printf("Errors: %lu\n", (unsigned long)bridge->stats.handler_errors);
    printf("============================================\n");
}
