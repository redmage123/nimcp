/**
 * @file nimcp_rsc_bio_async_bridge.c
 * @brief Implementation of Retrosplenial Cortex Bio-Async Integration Bridge
 * @version 1.0.0
 * @date 2026-01-13
 */

#include "core/brain/regions/retrosplenial/bridges/nimcp_rsc_bio_async_bridge.h"
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

BRIDGE_BOILERPLATE_MESH_ONLY(rsc_bio_async_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


#define LOG_MODULE "RSC_BIO_ASYNC_BRIDGE"


/* ============================================================================
 * Internal Bridge Structure
 * ============================================================================ */

struct rsc_bio_async_router_struct {
    rsc_bio_async_bridge_config_t config;
    nimcp_retrosplenial_t* rsc;
    bio_router_t router;

    rsc_bio_subscription_t* subscriptions;
    uint32_t subscription_count;
    uint32_t subscription_capacity;

    bool connected;
    uint64_t last_broadcast_us;
    uint32_t time_since_nav_broadcast_ms;
    uint32_t time_since_context_broadcast_ms;

    float prev_head_direction;

    rsc_bio_async_bridge_stats_t stats;
};

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

static uint64_t get_timestamp_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

static rsc_bio_subscription_t* find_subscription(rsc_bio_async_router_t* b, uint32_t mod_id) {
    for (uint32_t i = 0; i < b->subscription_count; i++) {
        if (b->subscriptions[i].module_id == mod_id && b->subscriptions[i].active) {
            return &b->subscriptions[i];
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_subscription: validation failed");
    return NULL;
}

static int count_subscribers_for_type(const rsc_bio_async_router_t* b, nimcp_rsc_bio_msg_type_t t) {
    int count = 0;
    uint32_t mask = (1U << t);
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

int rsc_bio_async_default_config(rsc_bio_async_bridge_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rsc_bio_async_default_config: config is NULL");
        return -1;
    }

    config->navigation_broadcast_interval_ms = RSC_BIO_DEFAULT_BROADCAST_INTERVAL_MS;
    config->context_broadcast_interval_ms = 100;  /* Context at 10Hz */
    config->enable_auto_broadcast = true;
    config->max_inbox_process_per_update = 32;
    config->message_ttl_ms = RSC_BIO_MESSAGE_TTL_MS;
    config->context_change_threshold = RSC_BIO_CONTEXT_CHANGE_THRESHOLD;
    config->novelty_threshold = RSC_BIO_NOVELTY_THRESHOLD;
    config->default_channel = BIO_CHANNEL_ACETYLCHOLINE;
    config->urgent_channel = BIO_CHANNEL_NOREPINEPHRINE;
    config->max_subscriptions = RSC_BIO_MAX_SUBSCRIPTIONS;
    config->enable_frame_transform_routing = true;
    config->enable_context_routing = true;
    config->enable_navigation_routing = true;
    config->enable_landmark_routing = true;
    config->enable_imagination_routing = true;
    config->enable_logging = false;

    return 0;
}

rsc_bio_async_router_t* rsc_bio_async_router_create(const rsc_bio_async_bridge_config_t* config) {
    rsc_bio_async_router_t* bridge = nimcp_calloc(1, sizeof(rsc_bio_async_router_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bridge is NULL");

        return NULL;

    }

    if (config) {
        bridge->config = *config;
    } else {
        rsc_bio_async_default_config(&bridge->config);
    }

    bridge->subscription_capacity = bridge->config.max_subscriptions;
    bridge->subscriptions = nimcp_calloc(bridge->subscription_capacity, sizeof(rsc_bio_subscription_t));
    if (!bridge->subscriptions) {
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "rsc_bio_async_router_create: bridge->subscriptions is NULL");
        return NULL;
    }

    bridge->last_broadcast_us = get_timestamp_us();
    NIMCP_LOGGING_INFO("Created %s bridge", "rsc_bio_async");
    return bridge;
}

void rsc_bio_async_router_destroy(rsc_bio_async_router_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "rsc_bio_async");

    if (bridge->connected) {
        rsc_bio_async_router_disconnect(bridge);
    }

    nimcp_free(bridge->subscriptions);
    nimcp_free(bridge);
}

/* ============================================================================
 * Connection API
 * ============================================================================ */

int rsc_bio_async_router_connect(
    rsc_bio_async_router_t* bridge,
    nimcp_retrosplenial_t* rsc,
    bio_router_t router
) {
    if (!bridge || !rsc) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rsc_bio_async_router_connect: required parameter is NULL (bridge, rsc)");
        return -1;
    }

    bridge->rsc = rsc;
    bridge->router = router;
    bridge->connected = true;

    return 0;
}

int rsc_bio_async_router_disconnect(rsc_bio_async_router_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rsc_bio_async_router_disconnect: bridge is NULL");
        return -1;
    }

    bridge->rsc = NULL;
    bridge->router = NULL;
    bridge->connected = false;

    return 0;
}

bool rsc_bio_async_router_is_connected(const rsc_bio_async_router_t* bridge) {
    return bridge ? bridge->connected : false;
}

/* ============================================================================
 * Message Processing API
 * ============================================================================ */

int rsc_bio_async_router_process_inbox(rsc_bio_async_router_t* bridge, uint32_t max_messages) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rsc_bio_async_router_process_inbox: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    uint32_t processed = 0;
    (void)max_messages;

    /* Process incoming context requests and transform requests */
    /* TODO: Integrate with bio_router inbox when available */

    bridge->stats.messages_received += processed;
    return (int)processed;
}

int rsc_bio_async_router_update(rsc_bio_async_router_t* bridge, uint32_t delta_ms) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rsc_bio_async_router_update: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    bridge->time_since_nav_broadcast_ms += delta_ms;
    bridge->time_since_context_broadcast_ms += delta_ms;

    /* Auto-broadcast navigation state if enabled */
    if (bridge->config.enable_auto_broadcast && bridge->config.enable_navigation_routing) {
        if (bridge->time_since_nav_broadcast_ms >= bridge->config.navigation_broadcast_interval_ms) {
            bridge->time_since_nav_broadcast_ms = 0;
        }
    }

    return 0;
}

/* ============================================================================
 * Broadcast API - Frame Transformation
 * ============================================================================ */

int rsc_bio_async_broadcast_frame_transform(
    rsc_bio_async_router_t* bridge,
    nimcp_rsc_frame_t source_frame,
    nimcp_rsc_frame_t target_frame,
    const float* input_pos,
    const float* output_pos,
    float accuracy
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rsc_bio_async_broadcast_frame_transform: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }
    if (!input_pos || !output_pos) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rsc_bio_async_broadcast_frame_transform: required parameter is NULL (input_pos, output_pos)");
        return -1;
    }
    if (!bridge->config.enable_frame_transform_routing) return 0;

    rsc_bio_frame_transform_msg_t msg = {0};
    msg.header.type = RSC_BIO_MSG_FRAME_TRANSFORM;
    msg.header.source_module = BIO_MODULE_ID_RSC;
    msg.header.channel = bridge->config.default_channel;
    msg.header.flags = 0;
    msg.header.timestamp_us = get_timestamp_us();

    msg.source_frame = source_frame;
    msg.target_frame = target_frame;
    msg.accuracy = accuracy;
    memcpy(msg.input_position, input_pos, 3 * sizeof(float));
    memcpy(msg.output_position, output_pos, 3 * sizeof(float));
    msg.timestamp_us = get_timestamp_us();

    int subs = count_subscribers_for_type(bridge, RSC_BIO_MSG_FRAME_TRANSFORM);
    if (subs > 0) {
        bridge->stats.frame_transforms_sent++;
        bridge->stats.messages_sent++;
        bridge->stats.broadcasts_sent++;
    }

    bridge->stats.last_broadcast_time_us = msg.timestamp_us;
    return 0;
}

/* ============================================================================
 * Broadcast API - Context
 * ============================================================================ */

int rsc_bio_async_broadcast_context(
    rsc_bio_async_router_t* bridge,
    const float* context_vector,
    uint32_t context_dim,
    nimcp_rsc_context_type_t dominant_type,
    float strength
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rsc_bio_async_broadcast_context: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }
    if (!context_vector || context_dim == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "rsc_bio_async_broadcast_context: context_vector is NULL");
        return -1;
    }
    if (!bridge->config.enable_context_routing) return 0;

    rsc_bio_context_update_msg_t msg = {0};
    msg.header.type = RSC_BIO_MSG_CONTEXT;
    msg.header.source_module = BIO_MODULE_ID_RSC;
    msg.header.channel = bridge->config.default_channel;
    msg.header.flags = 0;
    msg.header.timestamp_us = get_timestamp_us();

    uint32_t copy_dim = (context_dim < 128) ? context_dim : 128;
    memcpy(msg.context_vector, context_vector, copy_dim * sizeof(float));
    msg.context_dim = copy_dim;
    msg.dominant_type = dominant_type;
    msg.context_strength = strength;
    msg.timestamp_us = get_timestamp_us();

    bridge->stats.context_updates_sent++;
    bridge->stats.messages_sent++;
    bridge->stats.broadcasts_sent++;
    bridge->stats.last_broadcast_time_us = msg.timestamp_us;

    return 0;
}

/* ============================================================================
 * Broadcast API - Head Direction
 * ============================================================================ */

int rsc_bio_async_broadcast_head_direction(
    rsc_bio_async_router_t* bridge,
    float head_direction,
    float angular_velocity,
    float confidence
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rsc_bio_async_broadcast_head_direction: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    rsc_bio_head_direction_msg_t msg = {0};
    msg.header.type = RSC_BIO_MSG_HEAD_DIRECTION;
    msg.header.source_module = BIO_MODULE_ID_RSC;
    msg.header.channel = bridge->config.default_channel;
    msg.header.flags = 0;
    msg.header.timestamp_us = get_timestamp_us();

    msg.head_direction = head_direction;
    msg.previous_direction = bridge->prev_head_direction;
    msg.angular_velocity = angular_velocity;
    msg.confidence = confidence;
    msg.timestamp_us = get_timestamp_us();

    bridge->prev_head_direction = head_direction;

    bridge->stats.head_direction_sent++;
    bridge->stats.messages_sent++;
    bridge->stats.broadcasts_sent++;
    bridge->stats.last_broadcast_time_us = msg.timestamp_us;

    return 0;
}

/* ============================================================================
 * Broadcast API - Landmarks
 * ============================================================================ */

int rsc_bio_async_broadcast_landmark(
    rsc_bio_async_router_t* bridge,
    uint32_t landmark_id,
    const char* name,
    const float* position,
    float recognition_strength
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rsc_bio_async_broadcast_landmark: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }
    if (!position) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rsc_bio_async_broadcast_landmark: position is NULL");
        return -1;
    }
    if (!bridge->config.enable_landmark_routing) return 0;

    rsc_bio_landmark_msg_t msg = {0};
    msg.header.type = RSC_BIO_MSG_LANDMARK_DETECTED;
    msg.header.source_module = BIO_MODULE_ID_RSC;
    msg.header.channel = bridge->config.default_channel;
    msg.header.flags = 0;
    msg.header.timestamp_us = get_timestamp_us();

    msg.landmark_id = landmark_id;
    if (name) {
        strncpy(msg.landmark_name, name, sizeof(msg.landmark_name) - 1);
    }
    memcpy(msg.position, position, 3 * sizeof(float));
    msg.recognition_strength = recognition_strength;
    msg.timestamp_us = get_timestamp_us();

    bridge->stats.landmarks_sent++;
    bridge->stats.messages_sent++;
    bridge->stats.broadcasts_sent++;
    bridge->stats.last_broadcast_time_us = msg.timestamp_us;

    return 0;
}

/* ============================================================================
 * Broadcast API - Scene Familiarity
 * ============================================================================ */

int rsc_bio_async_broadcast_scene_familiarity(
    rsc_bio_async_router_t* bridge,
    nimcp_rsc_familiarity_t familiarity_level,
    float familiarity_score,
    float scene_coherence
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rsc_bio_async_broadcast_scene_familiarity: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    rsc_bio_scene_familiarity_msg_t msg = {0};
    msg.header.type = RSC_BIO_MSG_SCENE_FAMILIARITY;
    msg.header.source_module = BIO_MODULE_ID_RSC;
    msg.header.channel = bridge->config.default_channel;
    msg.header.flags = 0;
    msg.header.timestamp_us = get_timestamp_us();

    /* Novel scenes may need attention */
    if (familiarity_score < bridge->config.novelty_threshold) {
        msg.header.channel = bridge->config.urgent_channel;
        msg.is_novel = true;
    }

    msg.familiarity_level = familiarity_level;
    msg.familiarity_score = familiarity_score;
    msg.scene_coherence = scene_coherence;
    msg.timestamp_us = get_timestamp_us();

    bridge->stats.scene_familiarity_sent++;
    bridge->stats.messages_sent++;
    bridge->stats.broadcasts_sent++;
    bridge->stats.last_broadcast_time_us = msg.timestamp_us;

    return 0;
}

/* ============================================================================
 * Broadcast API - Imagination
 * ============================================================================ */

int rsc_bio_async_broadcast_imagination_state(
    rsc_bio_async_router_t* bridge,
    nimcp_rsc_imagine_mode_t mode,
    bool active,
    float vividness,
    float plausibility
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rsc_bio_async_broadcast_imagination_state: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }
    if (!bridge->config.enable_imagination_routing) return 0;

    rsc_bio_imagination_state_msg_t msg = {0};
    msg.header.type = RSC_BIO_MSG_IMAGINATION_STATE;
    msg.header.source_module = BIO_MODULE_ID_RSC;
    msg.header.channel = bridge->config.default_channel;
    msg.header.flags = 0;
    msg.header.timestamp_us = get_timestamp_us();

    msg.mode = mode;
    msg.active = active;
    msg.vividness = vividness;
    msg.plausibility = plausibility;
    msg.timestamp_us = get_timestamp_us();

    bridge->stats.imagination_updates_sent++;
    bridge->stats.messages_sent++;
    bridge->stats.broadcasts_sent++;
    bridge->stats.last_broadcast_time_us = msg.timestamp_us;

    return 0;
}

/* ============================================================================
 * Broadcast API - Navigation
 * ============================================================================ */

int rsc_bio_async_broadcast_navigation(
    rsc_bio_async_router_t* bridge,
    const float* position,
    float heading,
    float speed,
    float pose_confidence
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rsc_bio_async_broadcast_navigation: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }
    if (!position) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rsc_bio_async_broadcast_navigation: position is NULL");
        return -1;
    }
    if (!bridge->config.enable_navigation_routing) return 0;

    rsc_bio_navigation_msg_t msg = {0};
    msg.header.type = RSC_BIO_MSG_NAVIGATION;
    msg.header.source_module = BIO_MODULE_ID_RSC;
    msg.header.channel = bridge->config.default_channel;
    msg.header.flags = 0;
    msg.header.timestamp_us = get_timestamp_us();

    memcpy(msg.position, position, 3 * sizeof(float));
    msg.heading = heading;
    msg.speed = speed;
    msg.pose_confidence = pose_confidence;
    msg.timestamp_us = get_timestamp_us();

    bridge->stats.navigation_updates_sent++;
    bridge->stats.messages_sent++;
    bridge->stats.broadcasts_sent++;
    bridge->stats.last_broadcast_time_us = msg.timestamp_us;

    return 0;
}

/* ============================================================================
 * Subscription Management API
 * ============================================================================ */

int rsc_bio_async_subscribe_module(
    rsc_bio_async_router_t* bridge,
    uint32_t module_id,
    uint32_t msg_types
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rsc_bio_async_subscribe_module: bridge is NULL");
        return -1;
    }

    /* Check for existing subscription */
    rsc_bio_subscription_t* existing = find_subscription(bridge, module_id);
    if (existing) {
        existing->msg_type_mask |= msg_types;
        return 0;
    }

    /* Check capacity */
    if (bridge->subscription_count >= bridge->subscription_capacity) {
        bridge->stats.routing_errors++;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "rsc_bio_async_subscribe_module: capacity exceeded");
        return -1;
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

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "rsc_bio_async_subscribe_module: validation failed");
    return -1;
}

int rsc_bio_async_unsubscribe_module(
    rsc_bio_async_router_t* bridge,
    uint32_t module_id
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rsc_bio_async_unsubscribe_module: bridge is NULL");
        return -1;
    }

    rsc_bio_subscription_t* sub = find_subscription(bridge, module_id);
    if (!sub) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rsc_bio_async_unsubscribe_module: sub is NULL");
        return -1;
    }

    sub->active = false;
    sub->msg_type_mask = 0;
    bridge->subscription_count--;
    bridge->stats.active_subscriptions = bridge->subscription_count;

    return 0;
}

int rsc_bio_async_update_subscription(
    rsc_bio_async_router_t* bridge,
    uint32_t module_id,
    uint32_t msg_types
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rsc_bio_async_update_subscription: bridge is NULL");
        return -1;
    }

    rsc_bio_subscription_t* sub = find_subscription(bridge, module_id);
    if (!sub) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rsc_bio_async_update_subscription: sub is NULL");
        return -1;
    }

    sub->msg_type_mask = msg_types;
    return 0;
}

uint32_t rsc_bio_async_get_subscriber_count(
    const rsc_bio_async_router_t* bridge,
    nimcp_rsc_bio_msg_type_t msg_type
) {
    if (!bridge) return 0;
    return (uint32_t)count_subscribers_for_type(bridge, msg_type);
}

/* ============================================================================
 * Statistics and Diagnostics API
 * ============================================================================ */

int rsc_bio_async_get_stats(
    const rsc_bio_async_router_t* bridge,
    rsc_bio_async_bridge_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rsc_bio_async_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }

    *stats = bridge->stats;
    return 0;
}

int rsc_bio_async_reset_stats(rsc_bio_async_router_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rsc_bio_async_reset_stats: bridge is NULL");
        return -1;
    }

    uint32_t active_subs = bridge->stats.active_subscriptions;
    uint32_t peak_subs = bridge->stats.peak_subscriptions;

    memset(&bridge->stats, 0, sizeof(bridge->stats));

    bridge->stats.active_subscriptions = active_subs;
    bridge->stats.peak_subscriptions = peak_subs;

    return 0;
}

const char* rsc_bio_msg_type_name(nimcp_rsc_bio_msg_type_t msg_type) {
    static const char* names[] = {
        "CONTEXT",
        "NAVIGATION",
        "SCENE_FAMILIARITY",
        "FRAME_TRANSFORM",
        "LANDMARK_DETECTED",
        "HEAD_DIRECTION",
        "IMAGINATION_STATE",
        "CONTEXT_REQUEST",
        "TRANSFORM_REQUEST"
    };

    if (msg_type >= RSC_BIO_MSG_COUNT) return "UNKNOWN";
    return names[msg_type];
}

void rsc_bio_async_print_summary(const rsc_bio_async_router_t* bridge) {
    if (!bridge) {
        printf("RSC Bio-Async Bridge: NULL\n");
        return;
    }

    printf("=== RSC Bio-Async Bridge Summary ===\n");
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
    printf("Frame transforms: %lu\n", (unsigned long)bridge->stats.frame_transforms_sent);
    printf("Context updates: %lu\n", (unsigned long)bridge->stats.context_updates_sent);
    printf("Head direction: %lu\n", (unsigned long)bridge->stats.head_direction_sent);
    printf("Landmarks: %lu\n", (unsigned long)bridge->stats.landmarks_sent);
    printf("Scene familiarity: %lu\n", (unsigned long)bridge->stats.scene_familiarity_sent);
    printf("Imagination: %lu\n", (unsigned long)bridge->stats.imagination_updates_sent);
    printf("Navigation: %lu\n", (unsigned long)bridge->stats.navigation_updates_sent);
    printf("\n--- Errors ---\n");
    printf("Handler errors: %lu\n", (unsigned long)bridge->stats.handler_errors);
    printf("Routing errors: %lu\n", (unsigned long)bridge->stats.routing_errors);
    printf("====================================\n");
}
