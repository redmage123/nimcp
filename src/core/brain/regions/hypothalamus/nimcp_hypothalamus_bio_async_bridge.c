/**
 * @file nimcp_hypothalamus_bio_async_bridge.c
 * @brief Unified Hypothalamus Bio-Async Integration Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-10
 *
 * Implementation of the hypothalamus bio-async integration bridge.
 * See header for architectural documentation.
 */

#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_bio_async_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdio.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_math_constants.h"

BRIDGE_BOILERPLATE_MESH_ONLY(hypothalamus_bio_async_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


/* ============================================================================
 * Internal Structure
 * ============================================================================ */

/**
 * @brief Internal bridge structure
 */
struct hypo_bio_async_bridge_struct {
    bridge_base_t base;                     /**< Base bridge infrastructure */

    /* Connected systems */
    hypo_orchestrator_t orchestrator;       /**< Hypothalamus orchestrator */
    bio_router_t router;                    /**< Bio-async router */
    bio_module_context_t module_ctx;        /**< Bio-router module context */

    /* Configuration */
    hypo_bio_async_config_t config;         /**< Bridge configuration */

    /* Subscription registry */
    hypo_bio_subscription_t* subscriptions; /**< Module subscriptions */
    size_t subscription_count;              /**< Active subscription count */
    size_t subscription_capacity;           /**< Allocated subscription slots */

    /* Timing state */
    uint64_t last_drive_broadcast_us;       /**< Last drive broadcast time */
    uint64_t last_circadian_broadcast_us;   /**< Last circadian broadcast time */
    uint64_t creation_time_us;              /**< Bridge creation time */

    /* Statistics */
    hypo_bio_async_stats_t stats;           /**< Bridge statistics */

    /* Connection state */
    bool connected_to_router;               /**< Router connection flag */
    bool connected_to_orch;                 /**< Orchestrator connection flag */

    /* Message sequence counter */
    uint32_t sequence_id;                   /**< Message sequence counter */
};

/* ============================================================================
 * Forward Declarations for Message Handlers
 * ============================================================================ */

static nimcp_error_t handle_drive_request(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data);

static nimcp_error_t handle_modulate_drive(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data);

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Get current time in microseconds
 */
static uint64_t get_time_us(void) {
    return nimcp_time_get_us();
}

/**
 * @brief Initialize message header
 */
static void init_message_header(
    bio_message_header_t* header,
    bio_message_type_t type,
    uint32_t source,
    uint32_t target,
    uint32_t payload_size,
    nimcp_bio_channel_type_t channel,
    uint32_t* sequence_id
) {
    if (!header) return;

    header->type = type;
    header->sequence_id = (*sequence_id)++;
    header->source_module = source;
    header->target_module = target;
    header->timestamp_us = get_time_us();
    header->channel = channel;
    header->payload_size = payload_size;
    header->flags = 0;
}

/**
 * @brief Find subscription by module ID
 */
static hypo_bio_subscription_t* find_subscription(
    hypo_bio_async_bridge_t* bridge,
    uint32_t module_id
) {
    if (!bridge || !bridge->subscriptions) {
        return NULL;
    }

    for (size_t i = 0; i < bridge->subscription_count; i++) {
        if (bridge->subscriptions[i].module_id == module_id &&
            bridge->subscriptions[i].active) {
            return &bridge->subscriptions[i];
        }
    }
    return NULL;  /* Not found is a normal condition */
}

/**
 * @brief Count subscribers for a message type
 */
static uint32_t count_subscribers_for_type(
    const hypo_bio_async_bridge_t* bridge,
    hypo_bio_msg_type_t msg_type
) {
    if (!bridge || !bridge->subscriptions) return 0;

    uint32_t mask = (1U << msg_type);
    uint32_t count = 0;

    for (size_t i = 0; i < bridge->subscription_count; i++) {
        if (bridge->subscriptions[i].active &&
            (bridge->subscriptions[i].msg_type_mask & mask)) {
            count++;
        }
    }
    return count;
}

/* ============================================================================
 * Configuration API
 * ============================================================================ */

int hypo_bio_async_default_config(hypo_bio_async_config_t* config) {
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;

    }

    memset(config, 0, sizeof(hypo_bio_async_config_t));

    /* Broadcast timing */
    config->drive_broadcast_interval_ms = HYPO_BIO_DEFAULT_BROADCAST_INTERVAL_MS;
    config->circadian_broadcast_interval_ms = 1000;  /* 1 second */
    config->enable_auto_broadcast = true;

    /* Message handling */
    config->max_inbox_process_per_update = 32;
    config->message_ttl_ms = HYPO_BIO_MESSAGE_TTL_MS;

    /* Priority settings */
    config->urgent_drive_threshold = HYPO_BIO_URGENT_DRIVE_THRESHOLD;
    config->default_channel = BIO_CHANNEL_SEROTONIN;
    config->urgent_channel = BIO_CHANNEL_NOREPINEPHRINE;

    /* Subscription limits */
    config->max_subscriptions = HYPO_BIO_MAX_SUBSCRIPTIONS;

    /* Feature flags */
    config->enable_reward_routing = true;
    config->enable_stress_routing = true;
    config->enable_circadian_routing = true;
    config->enable_autonomic_routing = true;
    config->enable_logging = false;

    return 0;
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

hypo_bio_async_bridge_t* hypo_bio_async_bridge_create(
    const hypo_bio_async_config_t* config
) {
    hypo_bio_async_bridge_t* bridge = nimcp_malloc(sizeof(hypo_bio_async_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate hypothalamus bio-async bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bridge is NULL");

        return NULL;
    }

    memset(bridge, 0, sizeof(hypo_bio_async_bridge_t));

    /* Initialize base bridge */
    if (bridge_base_init(&bridge->base, BIO_MODULE_HYPOTHALAMUS,
                         "hypothalamus_bio_async") != 0) {
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "hypo_bio_async_bridge_create: bridge_base_init failed");
        return NULL;
    }

    /* Apply configuration */
    if (config) {
        memcpy(&bridge->config, config, sizeof(hypo_bio_async_config_t));
    } else {
        hypo_bio_async_default_config(&bridge->config);
    }

    /* Allocate subscription registry */
    bridge->subscription_capacity = bridge->config.max_subscriptions;
    bridge->subscriptions = nimcp_malloc(
        bridge->subscription_capacity * sizeof(hypo_bio_subscription_t));
    if (!bridge->subscriptions) {
        NIMCP_LOGGING_ERROR("Failed to allocate subscription registry");
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hypo_bio_async_bridge_create: bridge->subscriptions is NULL");
        return NULL;
    }
    memset(bridge->subscriptions, 0,
           bridge->subscription_capacity * sizeof(hypo_bio_subscription_t));

    /* Initialize timing */
    bridge->creation_time_us = get_time_us();
    bridge->last_drive_broadcast_us = bridge->creation_time_us;
    bridge->last_circadian_broadcast_us = bridge->creation_time_us;

    /* Initialize sequence counter */
    bridge->sequence_id = 1;

    NIMCP_LOGGING_INFO("Hypothalamus bio-async bridge created");
    return bridge;
}

void hypo_bio_async_bridge_destroy(hypo_bio_async_bridge_t* bridge) {
    if (!bridge) return;

    /* Disconnect if connected */
    if (bridge->connected_to_router) {
        hypo_bio_async_disconnect(bridge);
    }

    /* Free subscriptions */
    if (bridge->subscriptions) {
        nimcp_free(bridge->subscriptions);
        bridge->subscriptions = NULL;
    }

    /* Cleanup base bridge */
    bridge_base_cleanup(&bridge->base);

    nimcp_free(bridge);
    NIMCP_LOGGING_INFO("Hypothalamus bio-async bridge destroyed");
}

/* ============================================================================
 * Connection API
 * ============================================================================ */

int hypo_bio_async_connect(
    hypo_bio_async_bridge_t* bridge,
    hypo_orchestrator_t orch,
    bio_router_t router
) {
    if (!bridge || !orch) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_bio_async_connect: required parameter is NULL (bridge, orch)");
        return -1;
    }

    /* Use global router if none specified */
    if (!router) {
        router = bio_router_get_global();
        if (!router) {
            NIMCP_LOGGING_ERROR("No bio-router available");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_bio_async_connect: router is NULL");
            return -1;
        }
    }

    /* Store references */
    bridge->orchestrator = orch;
    bridge->router = router;

    /* Register with bio-router */
    bio_module_info_t info = {
        .module_id = BIO_MODULE_HYPOTHALAMUS,
        .module_name = "hypothalamus_bio_async",
        .inbox_capacity = HYPO_BIO_MAX_INBOX_SIZE,
        .user_data = bridge
    };

    bridge->module_ctx = bio_router_register_module(&info);
    if (!bridge->module_ctx) {
        NIMCP_LOGGING_ERROR("Failed to register with bio-router");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_bio_async_connect: bridge->module_ctx is NULL");
        return -1;
    }

    /* Register message handlers */
    bio_router_register_handler(bridge->module_ctx,
        BIO_MSG_HYPO_DRIVE_STATE, handle_drive_request);
    bio_router_register_handler(bridge->module_ctx,
        BIO_MSG_STATE_QUERY, handle_drive_request);

    /* Update connection state */
    bridge->connected_to_router = true;
    bridge->connected_to_orch = true;
    bridge->base.bridge_active = true;

    NIMCP_LOGGING_INFO("Hypothalamus bio-async bridge connected");
    return 0;
}

int hypo_bio_async_disconnect(hypo_bio_async_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    if (bridge->module_ctx) {
        bio_router_unregister_module(bridge->module_ctx);
        bridge->module_ctx = NULL;
    }

    bridge->connected_to_router = false;
    bridge->connected_to_orch = false;
    bridge->base.bridge_active = false;

    NIMCP_LOGGING_INFO("Hypothalamus bio-async bridge disconnected");
    return 0;
}

bool hypo_bio_async_is_connected(const hypo_bio_async_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    return bridge->connected_to_router && bridge->connected_to_orch;
}

/* ============================================================================
 * Message Processing API
 * ============================================================================ */

int hypo_bio_async_process_inbox(
    hypo_bio_async_bridge_t* bridge,
    uint32_t max_messages
) {
    if (!bridge || !bridge->module_ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_bio_async_process_inbox: required parameter is NULL (bridge, bridge->module_ctx)");
        return -1;
    }

    uint32_t limit = max_messages;
    if (limit == 0) {
        limit = bridge->config.max_inbox_process_per_update;
    }

    uint32_t processed = bio_router_process_inbox(bridge->module_ctx, limit);
    bridge->stats.messages_received += processed;

    return (int)processed;
}

int hypo_bio_async_update(
    hypo_bio_async_bridge_t* bridge,
    uint32_t delta_ms
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    uint64_t now_us = get_time_us();
    (void)delta_ms;  /* Use actual time for accuracy */

    /* Process inbox */
    hypo_bio_async_process_inbox(bridge, 0);

    /* Auto-broadcast drive state if enabled */
    if (bridge->config.enable_auto_broadcast && bridge->connected_to_orch) {
        uint64_t drive_interval_us =
            (uint64_t)bridge->config.drive_broadcast_interval_ms * 1000;

        if (now_us - bridge->last_drive_broadcast_us >= drive_interval_us) {
            hypo_bio_async_broadcast_drive_state(bridge);
            bridge->last_drive_broadcast_us = now_us;
        }
    }

    /* Record update */
    bridge_base_record_update(&bridge->base);

    return 0;
}

/* ============================================================================
 * Broadcast API
 * ============================================================================ */

int hypo_bio_async_broadcast_drive_state(hypo_bio_async_bridge_t* bridge) {
    if (!bridge || !bridge->connected_to_router || !bridge->orchestrator) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_bio_async_broadcast_drive_state: required parameter is NULL (bridge, bridge->connected_to_router, bridge->orchestrator)");
        return -1;
    }

    /* Get drive state from orchestrator */
    hypo_unified_drive_state_t drive_state;
    if (hypo_orch_get_drive_state(bridge->orchestrator, &drive_state) != 0) {
        bridge->stats.handler_errors++;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hypo_bio_async_broadcast_drive_state: validation failed");
        return -1;
    }

    /* Build drive state message */
    hypo_bio_drive_state_msg_t msg;
    memset(&msg, 0, sizeof(msg));

    init_message_header(&msg.header, BIO_MSG_HYPO_DRIVE_STATE,
        BIO_MODULE_HYPOTHALAMUS, 0, sizeof(msg),
        bridge->config.default_channel, &bridge->sequence_id);
    msg.header.flags |= BIO_MSG_FLAG_BROADCAST;

    /* Copy drive levels from bridge_drives array */
    /* Note: bridge_drives is indexed by bridge type, not drive type */
    /* Map to drive levels array using available bridge data */
    size_t max_drives = sizeof(msg.drive_levels) / sizeof(msg.drive_levels[0]);
    for (size_t i = 0; i < max_drives && i < HYPO_BRIDGE_COUNT; i++) {
        msg.drive_levels[i] = drive_state.bridge_drives[i].drive_level;
        msg.drive_urgencies[i] = drive_state.bridge_drives[i].drive_level;
    }

    msg.primary_drive = (hypo_drive_type_t)drive_state.primary_drive_type;
    msg.primary_urgency = drive_state.unified_drive_level;
    msg.unified_drive_level = drive_state.unified_drive_level;
    msg.timestamp_us = get_time_us();

    /* Broadcast to router */
    nimcp_error_t err = bio_router_broadcast(bridge->module_ctx, &msg, sizeof(msg));
    if (err != NIMCP_SUCCESS) {
        bridge->stats.routing_errors++;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hypo_bio_async_broadcast_drive_state: validation failed");
        return -1;
    }

    bridge->stats.drive_state_broadcasts++;
    bridge->stats.broadcasts_sent++;
    bridge->stats.messages_sent++;

    return 0;
}

int hypo_bio_async_broadcast_circadian(
    hypo_bio_async_bridge_t* bridge,
    float phase
) {
    if (!bridge || !bridge->connected_to_router) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_bio_async_broadcast_circadian: required parameter is NULL (bridge, bridge->connected_to_router)");
        return -1;
    }
    if (!bridge->config.enable_circadian_routing) return 0;

    hypo_bio_circadian_msg_t msg;
    memset(&msg, 0, sizeof(msg));

    init_message_header(&msg.header, BIO_MSG_CIRCADIAN_PHASE_CHANGE,
        BIO_MODULE_HYPOTHALAMUS, 0, sizeof(msg),
        bridge->config.default_channel, &bridge->sequence_id);
    msg.header.flags |= BIO_MSG_FLAG_BROADCAST;

    msg.phase = phase;
    msg.phase_normalized = phase / (NIMCP_TWO_PI_F);
    msg.alertness_factor = 0.5f + 0.5f * cosf(phase);  /* Simple model */
    msg.sleep_propensity = 0.5f - 0.5f * cosf(phase);

    /* Convert phase to time of day (assuming phase 0 = midnight) */
    float hours = (phase / (NIMCP_TWO_PI_F)) * 24.0f;
    msg.time_of_day_hours = (uint32_t)hours;
    msg.time_of_day_minutes = (uint32_t)((hours - msg.time_of_day_hours) * 60.0f);
    msg.timestamp_us = get_time_us();

    nimcp_error_t err = bio_router_broadcast(bridge->module_ctx, &msg, sizeof(msg));
    if (err != NIMCP_SUCCESS) {
        bridge->stats.routing_errors++;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hypo_bio_async_broadcast_circadian: validation failed");
        return -1;
    }

    bridge->stats.circadian_broadcasts++;
    bridge->stats.broadcasts_sent++;
    bridge->stats.messages_sent++;

    return 0;
}

int hypo_bio_async_broadcast_stress(
    hypo_bio_async_bridge_t* bridge,
    float cortisol
) {
    if (!bridge || !bridge->connected_to_router) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_bio_async_broadcast_stress: required parameter is NULL (bridge, bridge->connected_to_router)");
        return -1;
    }
    if (!bridge->config.enable_stress_routing) return 0;

    hypo_bio_stress_msg_t msg;
    memset(&msg, 0, sizeof(msg));

    init_message_header(&msg.header, BIO_MSG_STRESS_RESPONSE,
        BIO_MODULE_HYPOTHALAMUS, 0, sizeof(msg),
        bridge->config.urgent_channel, &bridge->sequence_id);
    msg.header.flags |= BIO_MSG_FLAG_BROADCAST;

    msg.stress_level = cortisol;
    msg.cortisol_level = cortisol;
    msg.crh_level = cortisol * 0.8f;  /* CRH precedes cortisol */
    msg.is_acute = (cortisol > 0.7f);
    msg.hpa_axis_active = (cortisol > 0.3f);
    msg.stressor_source = 0;
    msg.stress_onset_us = get_time_us();
    msg.timestamp_us = msg.stress_onset_us;

    nimcp_error_t err = bio_router_broadcast(bridge->module_ctx, &msg, sizeof(msg));
    if (err != NIMCP_SUCCESS) {
        bridge->stats.routing_errors++;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hypo_bio_async_broadcast_stress: validation failed");
        return -1;
    }

    bridge->stats.stress_broadcasts++;
    bridge->stats.broadcasts_sent++;
    bridge->stats.messages_sent++;

    return 0;
}

int hypo_bio_async_broadcast_arousal(
    hypo_bio_async_bridge_t* bridge,
    float arousal
) {
    if (!bridge || !bridge->connected_to_router) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_bio_async_broadcast_arousal: required parameter is NULL (bridge, bridge->connected_to_router)");
        return -1;
    }

    hypo_bio_arousal_msg_t msg;
    memset(&msg, 0, sizeof(msg));

    init_message_header(&msg.header, BIO_MSG_HYPO_AROUSAL_CHANGE,
        BIO_MODULE_HYPOTHALAMUS, 0, sizeof(msg),
        bridge->config.default_channel, &bridge->sequence_id);
    msg.header.flags |= BIO_MSG_FLAG_BROADCAST;

    msg.arousal_level = arousal;
    msg.alertness = arousal * 0.9f;
    msg.orexin_level = arousal * 0.7f;
    msg.histamine_level = arousal * 0.6f;
    msg.is_sleep_deprived = (arousal < 0.3f);
    msg.vigilance = arousal * 0.8f;
    msg.timestamp_us = get_time_us();

    nimcp_error_t err = bio_router_broadcast(bridge->module_ctx, &msg, sizeof(msg));
    if (err != NIMCP_SUCCESS) {
        bridge->stats.routing_errors++;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hypo_bio_async_broadcast_arousal: validation failed");
        return -1;
    }

    bridge->stats.broadcasts_sent++;
    bridge->stats.messages_sent++;

    return 0;
}

int hypo_bio_async_broadcast_autonomic(
    hypo_bio_async_bridge_t* bridge,
    float sympathetic,
    float parasympathetic
) {
    if (!bridge || !bridge->connected_to_router) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_bio_async_broadcast_autonomic: required parameter is NULL (bridge, bridge->connected_to_router)");
        return -1;
    }
    if (!bridge->config.enable_autonomic_routing) return 0;

    hypo_bio_autonomic_msg_t msg;
    memset(&msg, 0, sizeof(msg));

    init_message_header(&msg.header, BIO_MSG_AUTONOMIC_STATE_CHANGE,
        BIO_MODULE_HYPOTHALAMUS, 0, sizeof(msg),
        bridge->config.default_channel, &bridge->sequence_id);
    msg.header.flags |= BIO_MSG_FLAG_BROADCAST;

    msg.sympathetic_tone = sympathetic;
    msg.parasympathetic_tone = parasympathetic;
    msg.autonomic_balance = sympathetic - parasympathetic;
    msg.heart_rate_mod = 1.0f + 0.3f * (sympathetic - parasympathetic);
    msg.respiratory_mod = 1.0f + 0.2f * (sympathetic - parasympathetic);
    msg.timestamp_us = get_time_us();

    nimcp_error_t err = bio_router_broadcast(bridge->module_ctx, &msg, sizeof(msg));
    if (err != NIMCP_SUCCESS) {
        bridge->stats.routing_errors++;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hypo_bio_async_broadcast_autonomic: validation failed");
        return -1;
    }

    bridge->stats.broadcasts_sent++;
    bridge->stats.messages_sent++;

    return 0;
}

int hypo_bio_async_broadcast_homeostatic_alert(
    hypo_bio_async_bridge_t* bridge,
    uint32_t variable_id,
    float current_value,
    float setpoint,
    bool is_critical
) {
    if (!bridge || !bridge->connected_to_router) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_bio_async_broadcast_homeostatic_alert: required parameter is NULL (bridge, bridge->connected_to_router)");
        return -1;
    }

    hypo_bio_homeostatic_alert_msg_t msg;
    memset(&msg, 0, sizeof(msg));

    nimcp_bio_channel_type_t channel = is_critical ?
        bridge->config.urgent_channel : bridge->config.default_channel;

    init_message_header(&msg.header, BIO_MSG_HOMEOSTATIC_ALERT,
        BIO_MODULE_HYPOTHALAMUS, 0, sizeof(msg),
        channel, &bridge->sequence_id);
    msg.header.flags |= BIO_MSG_FLAG_BROADCAST;
    if (is_critical) {
        msg.header.flags |= BIO_MSG_FLAG_URGENT;
    }

    msg.variable_id = variable_id;
    msg.current_value = current_value;
    msg.setpoint = setpoint;
    msg.deviation = current_value - setpoint;
    msg.severity = fabsf(msg.deviation) / (fabsf(setpoint) + 0.001f);
    if (msg.severity > 1.0f) msg.severity = 1.0f;
    msg.is_critical = is_critical;
    msg.timestamp_us = get_time_us();

    nimcp_error_t err = bio_router_broadcast(bridge->module_ctx, &msg, sizeof(msg));
    if (err != NIMCP_SUCCESS) {
        bridge->stats.routing_errors++;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hypo_bio_async_broadcast_homeostatic_alert: validation failed");
        return -1;
    }

    bridge->stats.broadcasts_sent++;
    bridge->stats.messages_sent++;

    return 0;
}

int hypo_bio_async_send_urgent_drive(
    hypo_bio_async_bridge_t* bridge,
    hypo_drive_type_t drive_type,
    float drive_level,
    float urgency
) {
    if (!bridge || !bridge->connected_to_router) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_bio_async_send_urgent_drive: required parameter is NULL (bridge, bridge->connected_to_router)");
        return -1;
    }

    hypo_bio_drive_urgent_msg_t msg;
    memset(&msg, 0, sizeof(msg));

    init_message_header(&msg.header, BIO_MSG_HYPO_SURVIVAL_PRIORITY,
        BIO_MODULE_HYPOTHALAMUS, 0, sizeof(msg),
        bridge->config.urgent_channel, &bridge->sequence_id);
    msg.header.flags |= BIO_MSG_FLAG_BROADCAST | BIO_MSG_FLAG_URGENT;

    msg.drive_type = drive_type;
    msg.drive_level = drive_level;
    msg.urgency = urgency;
    msg.deviation_from_setpoint = drive_level - 0.5f;  /* Assume 0.5 setpoint */
    msg.timestamp_us = get_time_us();

    nimcp_error_t err = bio_router_broadcast(bridge->module_ctx, &msg, sizeof(msg));
    if (err != NIMCP_SUCCESS) {
        bridge->stats.routing_errors++;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hypo_bio_async_send_urgent_drive: validation failed");
        return -1;
    }

    bridge->stats.urgent_notifications++;
    bridge->stats.broadcasts_sent++;
    bridge->stats.messages_sent++;

    return 0;
}

int hypo_bio_async_send_reward(
    hypo_bio_async_bridge_t* bridge,
    float reward,
    uint32_t target_module
) {
    if (!bridge || !bridge->connected_to_router) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_bio_async_send_reward: required parameter is NULL (bridge, bridge->connected_to_router)");
        return -1;
    }
    if (!bridge->config.enable_reward_routing) return 0;

    hypo_bio_reward_msg_t msg;
    memset(&msg, 0, sizeof(msg));

    init_message_header(&msg.header, BIO_MSG_HYPO_REWARD_SIGNAL,
        BIO_MODULE_HYPOTHALAMUS, target_module, sizeof(msg),
        BIO_CHANNEL_DOPAMINE, &bridge->sequence_id);

    if (target_module == 0) {
        msg.header.flags |= BIO_MSG_FLAG_BROADCAST;
    }

    msg.reward_magnitude = reward;
    msg.prediction_error = reward;  /* Simplified: assume unexpected */
    msg.source_drive = HYPO_DRIVE_HUNGER;  /* Default */
    msg.is_intrinsic = false;
    msg.dopamine_release = (reward + 1.0f) / 2.0f;  /* Map to [0,1] */
    msg.target_module = target_module;
    msg.timestamp_us = get_time_us();

    nimcp_error_t err;
    if (target_module == 0) {
        err = bio_router_broadcast(bridge->module_ctx, &msg, sizeof(msg));
    } else {
        err = bio_router_send(bridge->module_ctx, &msg, sizeof(msg), 0);
    }

    if (err != NIMCP_SUCCESS) {
        bridge->stats.routing_errors++;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hypo_bio_async_send_reward: validation failed");
        return -1;
    }

    bridge->stats.reward_signals_sent++;
    bridge->stats.messages_sent++;

    return 0;
}

int hypo_bio_async_broadcast_temperature(
    hypo_bio_async_bridge_t* bridge,
    float core_temp,
    float setpoint_temp
) {
    if (!bridge || !bridge->connected_to_router) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_bio_async_broadcast_temperature: required parameter is NULL (bridge, bridge->connected_to_router)");
        return -1;
    }

    hypo_bio_temperature_msg_t msg;
    memset(&msg, 0, sizeof(msg));

    init_message_header(&msg.header, BIO_MSG_TEMPERATURE_REGULATION,
        BIO_MODULE_HYPOTHALAMUS, 0, sizeof(msg),
        bridge->config.default_channel, &bridge->sequence_id);
    msg.header.flags |= BIO_MSG_FLAG_BROADCAST;

    msg.core_temperature = core_temp;
    msg.setpoint_temperature = setpoint_temp;
    msg.deviation = core_temp - setpoint_temp;
    msg.is_fever = (setpoint_temp > 37.5f);
    msg.needs_cooling = (msg.deviation > 0.5f);
    msg.needs_heating = (msg.deviation < -0.5f);
    msg.timestamp_us = get_time_us();

    nimcp_error_t err = bio_router_broadcast(bridge->module_ctx, &msg, sizeof(msg));
    if (err != NIMCP_SUCCESS) {
        bridge->stats.routing_errors++;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hypo_bio_async_broadcast_temperature: validation failed");
        return -1;
    }

    bridge->stats.broadcasts_sent++;
    bridge->stats.messages_sent++;

    return 0;
}

int hypo_bio_async_broadcast_fatigue(
    hypo_bio_async_bridge_t* bridge,
    float fatigue,
    float sleep_pressure
) {
    if (!bridge || !bridge->connected_to_router) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_bio_async_broadcast_fatigue: required parameter is NULL (bridge, bridge->connected_to_router)");
        return -1;
    }

    hypo_bio_fatigue_msg_t msg;
    memset(&msg, 0, sizeof(msg));

    init_message_header(&msg.header, BIO_MSG_HYPO_SLEEP_PRESSURE,
        BIO_MODULE_HYPOTHALAMUS, 0, sizeof(msg),
        bridge->config.default_channel, &bridge->sequence_id);
    msg.header.flags |= BIO_MSG_FLAG_BROADCAST;

    msg.fatigue_level = fatigue;
    msg.sleep_pressure = sleep_pressure;
    msg.adenosine_level = sleep_pressure * 0.8f;
    msg.time_awake_hours = sleep_pressure * 24.0f;  /* Rough estimate */
    msg.sleep_deprived = (sleep_pressure > 0.7f);
    msg.timestamp_us = get_time_us();

    nimcp_error_t err = bio_router_broadcast(bridge->module_ctx, &msg, sizeof(msg));
    if (err != NIMCP_SUCCESS) {
        bridge->stats.routing_errors++;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hypo_bio_async_broadcast_fatigue: validation failed");
        return -1;
    }

    bridge->stats.broadcasts_sent++;
    bridge->stats.messages_sent++;

    return 0;
}

/* ============================================================================
 * Subscription Management API
 * ============================================================================ */

int hypo_bio_async_subscribe_module(
    hypo_bio_async_bridge_t* bridge,
    uint32_t module_id,
    uint32_t msg_types
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    BRIDGE_LOCK(bridge);

    /* Check if already subscribed */
    hypo_bio_subscription_t* existing = find_subscription(bridge, module_id);
    if (existing) {
        existing->msg_type_mask = msg_types;
        BRIDGE_UNLOCK(bridge);
        return 0;
    }

    /* Check capacity */
    if (bridge->subscription_count >= bridge->subscription_capacity) {
        BRIDGE_UNLOCK(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "hypo_bio_async_subscribe_module: capacity exceeded");
        return -1;
    }

    /* Add new subscription */
    hypo_bio_subscription_t* sub = &bridge->subscriptions[bridge->subscription_count];
    sub->module_id = module_id;
    sub->msg_type_mask = msg_types;
    sub->active = true;
    sub->subscription_time = get_time_us();
    sub->messages_sent = 0;

    bridge->subscription_count++;
    bridge->stats.active_subscriptions++;

    if (bridge->stats.active_subscriptions > bridge->stats.peak_subscriptions) {
        bridge->stats.peak_subscriptions = bridge->stats.active_subscriptions;
    }

    BRIDGE_UNLOCK(bridge);
    return 0;
}

int hypo_bio_async_unsubscribe_module(
    hypo_bio_async_bridge_t* bridge,
    uint32_t module_id
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    BRIDGE_LOCK(bridge);

    hypo_bio_subscription_t* sub = find_subscription(bridge, module_id);
    if (!sub) {
        BRIDGE_UNLOCK(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_bio_async_unsubscribe_module: sub is NULL");
        return -1;
    }

    sub->active = false;
    bridge->stats.active_subscriptions--;

    BRIDGE_UNLOCK(bridge);
    return 0;
}

int hypo_bio_async_update_subscription(
    hypo_bio_async_bridge_t* bridge,
    uint32_t module_id,
    uint32_t msg_types
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    BRIDGE_LOCK(bridge);

    hypo_bio_subscription_t* sub = find_subscription(bridge, module_id);
    if (!sub) {
        BRIDGE_UNLOCK(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_bio_async_update_subscription: sub is NULL");
        return -1;
    }

    sub->msg_type_mask = msg_types;

    BRIDGE_UNLOCK(bridge);
    return 0;
}

uint32_t hypo_bio_async_get_subscriber_count(
    const hypo_bio_async_bridge_t* bridge,
    hypo_bio_msg_type_t msg_type
) {
    return count_subscribers_for_type(bridge, msg_type);
}

/* ============================================================================
 * Statistics API
 * ============================================================================ */

int hypo_bio_async_get_stats(
    const hypo_bio_async_bridge_t* bridge,
    hypo_bio_async_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_bio_async_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }

    memcpy(stats, &bridge->stats, sizeof(hypo_bio_async_stats_t));
    return 0;
}

int hypo_bio_async_reset_stats(hypo_bio_async_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    BRIDGE_LOCK(bridge);
    memset(&bridge->stats, 0, sizeof(hypo_bio_async_stats_t));
    bridge->stats.active_subscriptions = (uint32_t)bridge->subscription_count;
    BRIDGE_UNLOCK(bridge);

    return 0;
}

const char* hypo_bio_msg_type_name(hypo_bio_msg_type_t msg_type) {
    switch (msg_type) {
        case HYPO_BIO_MSG_DRIVE_STATE:      return "DRIVE_STATE";
        case HYPO_BIO_MSG_DRIVE_URGENT:     return "DRIVE_URGENT";
        case HYPO_BIO_MSG_HOMEOSTATIC_ALERT: return "HOMEOSTATIC_ALERT";
        case HYPO_BIO_MSG_CIRCADIAN_PHASE:  return "CIRCADIAN_PHASE";
        case HYPO_BIO_MSG_STRESS_LEVEL:     return "STRESS_LEVEL";
        case HYPO_BIO_MSG_AROUSAL_STATE:    return "AROUSAL_STATE";
        case HYPO_BIO_MSG_AUTONOMIC_STATE:  return "AUTONOMIC_STATE";
        case HYPO_BIO_MSG_REWARD_SIGNAL:    return "REWARD_SIGNAL";
        case HYPO_BIO_MSG_TEMPERATURE:      return "TEMPERATURE";
        case HYPO_BIO_MSG_FATIGUE_LEVEL:    return "FATIGUE_LEVEL";
        case HYPO_BIO_MSG_REQUEST_DRIVE:    return "REQUEST_DRIVE";
        case HYPO_BIO_MSG_MODULATE_DRIVE:   return "MODULATE_DRIVE";
        default:                             return "UNKNOWN";
    }
}

void hypo_bio_async_print_summary(const hypo_bio_async_bridge_t* bridge) {
    if (!bridge) {
        printf("Hypothalamus Bio-Async Bridge: NULL\n");
        return;
    }

    printf("\n=== Hypothalamus Bio-Async Bridge Summary ===\n");
    printf("Connected: %s (Router: %s, Orch: %s)\n",
           bridge->base.bridge_active ? "Yes" : "No",
           bridge->connected_to_router ? "Yes" : "No",
           bridge->connected_to_orch ? "Yes" : "No");

    printf("\nSubscriptions: %zu active (peak: %u)\n",
           bridge->subscription_count,
           bridge->stats.peak_subscriptions);

    printf("\nMessage Statistics:\n");
    printf("  Sent: %lu  Received: %lu  Dropped: %lu\n",
           (unsigned long)bridge->stats.messages_sent,
           (unsigned long)bridge->stats.messages_received,
           (unsigned long)bridge->stats.messages_dropped);
    printf("  Broadcasts: %lu  Errors: %lu\n",
           (unsigned long)bridge->stats.broadcasts_sent,
           (unsigned long)bridge->stats.routing_errors);

    printf("\nBroadcast Counts:\n");
    printf("  Drive State: %lu  Circadian: %lu  Stress: %lu\n",
           (unsigned long)bridge->stats.drive_state_broadcasts,
           (unsigned long)bridge->stats.circadian_broadcasts,
           (unsigned long)bridge->stats.stress_broadcasts);
    printf("  Urgent: %lu  Rewards: %lu\n",
           (unsigned long)bridge->stats.urgent_notifications,
           (unsigned long)bridge->stats.reward_signals_sent);

    printf("============================================\n\n");
}

/* ============================================================================
 * Message Handlers (Static)
 * ============================================================================ */

static nimcp_error_t handle_drive_request(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data
) {
    (void)msg_size;

    hypo_bio_async_bridge_t* bridge = (hypo_bio_async_bridge_t*)user_data;
    if (!bridge || !msg) return NIMCP_ERROR_NULL_POINTER;

    const bio_message_header_t* header = (const bio_message_header_t*)msg;
    (void)header;

    /* Broadcast current drive state in response */
    hypo_bio_async_broadcast_drive_state(bridge);

    /* Complete response promise if provided */
    if (response_promise) {
        int result = 0;
        nimcp_bio_promise_complete(response_promise, &result);
    }

    return NIMCP_SUCCESS;
}

static nimcp_error_t handle_modulate_drive(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data
) {
    (void)msg_size;

    hypo_bio_async_bridge_t* bridge = (hypo_bio_async_bridge_t*)user_data;
    if (!bridge || !msg) return NIMCP_ERROR_NULL_POINTER;

    /* Parse modulation request */
    const hypo_bio_modulate_request_msg_t* request =
        (const hypo_bio_modulate_request_msg_t*)msg;

    /* Forward to orchestrator for processing */
    if (bridge->orchestrator) {
        hypo_orch_report_drive(
            bridge->orchestrator,
            0,  /* bridge_id - would need to look up */
            (uint32_t)request->drive_type,
            request->modulation_amount,
            HYPO_URGENCY_MODERATE,
            "External modulation request"
        );
    }

    /* Complete response promise if provided */
    if (response_promise) {
        int result = 0;
        nimcp_bio_promise_complete(response_promise, &result);
    }

    return NIMCP_SUCCESS;
}
