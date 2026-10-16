/**
 * @file nimcp_ofc_bio_async_bridge.c
 * @brief Orbitofrontal Cortex Bio-Async Integration Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-13
 *
 * WHAT: Implementation of OFC bio-async messaging bridge
 * WHY:  Enable OFC to communicate value and decision signals via bio-router
 * HOW:  Register with router, manage subscriptions, broadcast typed messages
 *
 * @author NIMCP Development Team
 */

#include "core/brain/regions/ofc/bridges/nimcp_ofc_bio_async_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/time/nimcp_time.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <stdio.h>

//=============================================================================
#include <stddef.h>
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(ofc_bio_async_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


#define LOG_MODULE "OFC_BIO_ASYNC_BRIDGE"


/* ============================================================================
 * Internal Bridge Structure
 * ============================================================================ */

struct ofc_bio_async_bridge_struct {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    /* Configuration */
    ofc_bio_async_config_t config;

    /* Connection state */
    void* ofc;                              /**< Connected OFC instance */
    bio_router_t router;                    /**< Bio-router handle */
    bio_module_context_t module_ctx;        /**< Module context */
    bool connected;                         /**< Connection state */

    /* Subscriptions */
    ofc_bio_subscription_t subscriptions[OFC_BIO_MAX_SUBSCRIPTIONS];
    uint32_t subscription_count;

    /* Timing */
    uint64_t last_broadcast_us;             /**< Last broadcast timestamp */
    uint32_t time_since_broadcast_ms;       /**< Time accumulator */

    /* Statistics */
    ofc_bio_async_stats_t stats;

};

/* ============================================================================
 * Message Type Names
 * ============================================================================ */

static const char* g_ofc_msg_type_names[] = {
    "VALUE_UPDATE",
    "DECISION_EVENT",
    "RPE",
    "REVERSAL",
    "EXPECTED_VALUE",
    "CONTEXT_CHANGE",
    "RISK_SIGNAL",
    "SOCIAL_VALUE"
};

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Get current time in microseconds
 */
static uint64_t get_time_us(void)
{
    return nimcp_time_now_us();
}

/**
 * @brief Initialize message header
 */
static void init_msg_header(
    bio_message_header_t* header,
    bio_message_type_t type,
    uint32_t payload_size)
{
    if (!header) return;

    memset(header, 0, sizeof(*header));
    header->type = type;
    header->source_module = OFC_BIO_MODULE_ID;
    header->target_module = 0; /* Broadcast */
    header->timestamp_us = get_time_us();
    header->payload_size = payload_size;
    header->flags = BIO_MSG_FLAG_BROADCAST;
}

/* ============================================================================
 * Lifecycle API Implementation
 * ============================================================================ */

int ofc_bio_async_default_config(ofc_bio_async_config_t* config)
{
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ofc_bio_async_default_config: config is NULL");
        return -1;
    }

    memset(config, 0, sizeof(*config));

    /* Broadcast timing */
    config->broadcast_interval_ms = OFC_BIO_DEFAULT_BROADCAST_INTERVAL_MS;
    config->enable_auto_broadcast = true;
    config->enable_decision_broadcast = true;

    /* Message handling */
    config->max_inbox_process_per_update = 32;
    config->message_ttl_ms = OFC_BIO_MESSAGE_TTL_MS;

    /* Priority settings */
    config->rpe_threshold = OFC_BIO_RPE_SIGNIFICANCE;
    config->default_channel = BIO_CHANNEL_ACETYLCHOLINE;
    config->rpe_channel = BIO_CHANNEL_DOPAMINE;

    /* Subscription limits */
    config->max_subscriptions = OFC_BIO_MAX_SUBSCRIPTIONS;

    /* Feature flags */
    config->enable_value_broadcast = true;
    config->enable_rpe_broadcast = true;
    config->enable_reversal_broadcast = true;
    config->enable_context_broadcast = true;
    config->enable_logging = false;

    return 0;
}

ofc_bio_async_bridge_t* ofc_bio_async_bridge_create(
    const ofc_bio_async_config_t* config)
{
    ofc_bio_async_bridge_t* bridge = nimcp_malloc(sizeof(*bridge));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "ofc_bio_async_bridge_create: failed to allocate bridge");
        return NULL;
    }

    memset(bridge, 0, sizeof(*bridge));

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        ofc_bio_async_default_config(&bridge->config);
    }

    /* Create mutex */
    if (bridge_base_init(&bridge->base, 0, "ofc_bio_async") != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "ofc_bio_async_bridge_create: failed to initialize bridge base");
        nimcp_free(bridge);
        return NULL;
    }
    if (!bridge->base.mutex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "ofc_bio_async_bridge_create: mutex is NULL after init");
        nimcp_free(bridge);
        return NULL;
    }

    return bridge;
}

void ofc_bio_async_bridge_destroy(ofc_bio_async_bridge_t* bridge)
{
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "ofc_bio_async");

    /* Disconnect if connected */
    if (bridge->connected) {
        ofc_bio_async_disconnect(bridge);
    }

    /* Free mutex */
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
}

/* ============================================================================
 * Connection API Implementation
 * ============================================================================ */

int ofc_bio_async_connect(
    ofc_bio_async_bridge_t* bridge,
    void* ofc,
    bio_router_t router)
{
    if (!bridge || !router) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ofc_bio_async_connect: required parameter is NULL (bridge, router)");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    if (bridge->connected) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "ofc_bio_async_connect: validation failed");
        return -1; /* Already connected */
    }

    /* Store references */
    bridge->ofc = ofc;
    bridge->router = router;

    /* Register with router */
    bio_module_info_t info = {
        .module_id = OFC_BIO_MODULE_ID,
        .module_name = "OFC",
        .inbox_capacity = OFC_BIO_MAX_INBOX_SIZE,
        .user_data = bridge
    };

    bridge->module_ctx = bio_router_register_module(&info);
    if (!bridge->module_ctx) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ofc_bio_async_connect: bridge->module_ctx is NULL");
        return -1;
    }

    bridge->connected = true;
    bridge->last_broadcast_us = get_time_us();

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int ofc_bio_async_disconnect(ofc_bio_async_bridge_t* bridge)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ofc_bio_async_disconnect: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    if (!bridge->connected) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0; /* Already disconnected */
    }

    /* Unregister from router */
    if (bridge->module_ctx) {
        bio_router_unregister_module(bridge->module_ctx);
        bridge->module_ctx = NULL;
    }

    bridge->connected = false;
    bridge->ofc = NULL;
    bridge->router = NULL;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

bool ofc_bio_async_is_connected(const ofc_bio_async_bridge_t* bridge)
{
    if (!bridge) {
        return false;
    }
    return bridge->connected;
}

/* ============================================================================
 * Message Processing API Implementation
 * ============================================================================ */

int ofc_bio_async_process_inbox(
    ofc_bio_async_bridge_t* bridge,
    uint32_t max_messages)
{
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ofc_bio_async_process_inbox: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    uint32_t processed = bio_router_process_inbox(
        bridge->module_ctx,
        max_messages
    );

    bridge->stats.messages_received += processed;

    nimcp_mutex_unlock(bridge->base.mutex);
    return (int)processed;
}

int ofc_bio_async_update(
    ofc_bio_async_bridge_t* bridge,
    uint32_t delta_ms)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ofc_bio_async_update: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->time_since_broadcast_ms += delta_ms;

    /* Check for auto-broadcast */
    if (bridge->config.enable_auto_broadcast &&
        bridge->time_since_broadcast_ms >= bridge->config.broadcast_interval_ms) {
        bridge->time_since_broadcast_ms = 0;
        bridge->last_broadcast_us = get_time_us();
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Broadcast API Implementation
 * ============================================================================ */

int ofc_bio_async_broadcast_value(
    ofc_bio_async_bridge_t* bridge,
    uint32_t stimulus_id,
    float value,
    float confidence)
{
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ofc_bio_async_broadcast_value: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }
    if (!bridge->config.enable_value_broadcast) return 0;

    nimcp_mutex_lock(bridge->base.mutex);

    ofc_bio_value_msg_t msg;
    memset(&msg, 0, sizeof(msg));

    init_msg_header(&msg.header, BIO_MSG_BRAIN_STATE_RESPONSE,
                    sizeof(msg) - sizeof(msg.header));

    msg.stimulus_id = stimulus_id;
    msg.integrated_value = value;
    msg.confidence = confidence;
    msg.timestamp_us = get_time_us();

    nimcp_error_t err = bio_router_broadcast(
        bridge->module_ctx, &msg, sizeof(msg));

    if (err == NIMCP_SUCCESS) {
        bridge->stats.value_broadcasts++;
        bridge->stats.broadcasts_sent++;
    } else {
        bridge->stats.routing_errors++;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return (err == NIMCP_SUCCESS) ? 0 : -1;
}

int ofc_bio_async_broadcast_decision(
    ofc_bio_async_bridge_t* bridge,
    uint32_t chosen_option,
    float value,
    float confidence,
    float reaction_time_ms)
{
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ofc_bio_async_broadcast_decision: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }
    if (!bridge->config.enable_decision_broadcast) return 0;

    nimcp_mutex_lock(bridge->base.mutex);

    ofc_bio_decision_msg_t msg;
    memset(&msg, 0, sizeof(msg));

    init_msg_header(&msg.header, BIO_MSG_DECISION_RESPONSE,
                    sizeof(msg) - sizeof(msg.header));
    msg.header.flags |= BIO_MSG_FLAG_URGENT;

    msg.chosen_option = chosen_option;
    msg.decision_value = value;
    msg.confidence = confidence;
    msg.reaction_time_ms = reaction_time_ms;
    msg.timestamp_us = get_time_us();

    nimcp_error_t err = bio_router_broadcast(
        bridge->module_ctx, &msg, sizeof(msg));

    if (err == NIMCP_SUCCESS) {
        bridge->stats.decision_broadcasts++;
        bridge->stats.broadcasts_sent++;
    } else {
        bridge->stats.routing_errors++;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return (err == NIMCP_SUCCESS) ? 0 : -1;
}

int ofc_bio_async_broadcast_rpe(
    ofc_bio_async_bridge_t* bridge,
    uint32_t stimulus_id,
    float rpe,
    float received,
    float expected)
{
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ofc_bio_async_broadcast_rpe: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }
    if (!bridge->config.enable_rpe_broadcast) return 0;

    /* Skip insignificant RPEs */
    if (rpe > -bridge->config.rpe_threshold &&
        rpe < bridge->config.rpe_threshold) {
        return 0;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    ofc_bio_rpe_msg_t msg;
    memset(&msg, 0, sizeof(msg));

    init_msg_header(&msg.header, BIO_MSG_NEUROMODULATOR_RELEASE,
                    sizeof(msg) - sizeof(msg.header));
    msg.header.channel = bridge->config.rpe_channel;

    msg.stimulus_id = stimulus_id;
    msg.prediction_error = rpe;
    msg.received_reward = received;
    msg.expected_reward = expected;
    msg.positive_surprise = (rpe > 0);
    msg.learning_signal = rpe;
    msg.timestamp_us = get_time_us();

    nimcp_error_t err = bio_router_broadcast(
        bridge->module_ctx, &msg, sizeof(msg));

    if (err == NIMCP_SUCCESS) {
        bridge->stats.rpe_broadcasts++;
        bridge->stats.broadcasts_sent++;
    } else {
        bridge->stats.routing_errors++;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return (err == NIMCP_SUCCESS) ? 0 : -1;
}

int ofc_bio_async_broadcast_reversal(
    ofc_bio_async_bridge_t* bridge,
    uint32_t old_stimulus,
    uint32_t new_stimulus,
    float reversal_strength)
{
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ofc_bio_async_broadcast_reversal: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }
    if (!bridge->config.enable_reversal_broadcast) return 0;

    nimcp_mutex_lock(bridge->base.mutex);

    ofc_bio_reversal_msg_t msg;
    memset(&msg, 0, sizeof(msg));

    init_msg_header(&msg.header, BIO_MSG_ATTENTION_SHIFT,
                    sizeof(msg) - sizeof(msg.header));
    msg.header.flags |= BIO_MSG_FLAG_URGENT;

    msg.old_high_value_stimulus = old_stimulus;
    msg.new_high_value_stimulus = new_stimulus;
    msg.reversal_strength = reversal_strength;
    msg.exploration_boost = reversal_strength * 0.5f;
    msg.requires_relearning = (reversal_strength > 0.7f);
    msg.timestamp_us = get_time_us();

    nimcp_error_t err = bio_router_broadcast(
        bridge->module_ctx, &msg, sizeof(msg));

    if (err == NIMCP_SUCCESS) {
        bridge->stats.reversal_broadcasts++;
        bridge->stats.broadcasts_sent++;
    } else {
        bridge->stats.routing_errors++;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return (err == NIMCP_SUCCESS) ? 0 : -1;
}

int ofc_bio_async_broadcast_expected_value(
    ofc_bio_async_bridge_t* bridge,
    uint32_t stimulus_id,
    float expected_value,
    float variance)
{
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ofc_bio_async_broadcast_expected_value: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    ofc_bio_expected_value_msg_t msg;
    memset(&msg, 0, sizeof(msg));

    init_msg_header(&msg.header, BIO_MSG_BRAIN_STATE_RESPONSE,
                    sizeof(msg) - sizeof(msg.header));

    msg.stimulus_id = stimulus_id;
    msg.expected_value = expected_value;
    msg.value_variance = variance;
    msg.timestamp_us = get_time_us();

    nimcp_error_t err = bio_router_broadcast(
        bridge->module_ctx, &msg, sizeof(msg));

    if (err == NIMCP_SUCCESS) {
        bridge->stats.broadcasts_sent++;
    } else {
        bridge->stats.routing_errors++;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return (err == NIMCP_SUCCESS) ? 0 : -1;
}

int ofc_bio_async_broadcast_context_change(
    ofc_bio_async_bridge_t* bridge,
    uint32_t old_context,
    uint32_t new_context,
    float similarity)
{
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ofc_bio_async_broadcast_context_change: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }
    if (!bridge->config.enable_context_broadcast) return 0;

    nimcp_mutex_lock(bridge->base.mutex);

    ofc_bio_context_msg_t msg;
    memset(&msg, 0, sizeof(msg));

    init_msg_header(&msg.header, BIO_MSG_ATTENTION_SHIFT,
                    sizeof(msg) - sizeof(msg.header));

    msg.old_context_id = old_context;
    msg.new_context_id = new_context;
    msg.context_similarity = similarity;
    msg.is_boundary = (similarity < 0.3f);
    msg.is_gradual = (similarity >= 0.3f && similarity < 0.7f);
    msg.value_reset_factor = 1.0f - similarity;
    msg.generalization_factor = similarity;
    msg.timestamp_us = get_time_us();

    nimcp_error_t err = bio_router_broadcast(
        bridge->module_ctx, &msg, sizeof(msg));

    if (err == NIMCP_SUCCESS) {
        bridge->stats.context_broadcasts++;
        bridge->stats.broadcasts_sent++;
    } else {
        bridge->stats.routing_errors++;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return (err == NIMCP_SUCCESS) ? 0 : -1;
}

/* ============================================================================
 * Subscription Management API Implementation
 * ============================================================================ */

int ofc_bio_async_subscribe_module(
    ofc_bio_async_bridge_t* bridge,
    uint32_t module_id,
    uint32_t msg_types)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ofc_bio_async_subscribe_module: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Check for existing subscription */
    for (uint32_t i = 0; i < bridge->subscription_count; i++) {
        if (bridge->subscriptions[i].module_id == module_id) {
            bridge->subscriptions[i].msg_type_mask = msg_types;
            bridge->subscriptions[i].active = true;
            nimcp_mutex_unlock(bridge->base.mutex);
            return 0;
        }
    }

    /* Add new subscription */
    if (bridge->subscription_count >= bridge->config.max_subscriptions) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "ofc_bio_async_subscribe_module: capacity exceeded");
        return -1;
    }

    ofc_bio_subscription_t* sub =
        &bridge->subscriptions[bridge->subscription_count++];
    sub->module_id = module_id;
    sub->msg_type_mask = msg_types;
    sub->active = true;
    sub->subscription_time = get_time_us();
    sub->messages_sent = 0;

    bridge->stats.active_subscriptions = bridge->subscription_count;
    if (bridge->subscription_count > bridge->stats.peak_subscriptions) {
        bridge->stats.peak_subscriptions = bridge->subscription_count;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int ofc_bio_async_unsubscribe_module(
    ofc_bio_async_bridge_t* bridge,
    uint32_t module_id)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ofc_bio_async_unsubscribe_module: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    for (uint32_t i = 0; i < bridge->subscription_count; i++) {
        if (bridge->subscriptions[i].module_id == module_id) {
            /* Shift remaining subscriptions */
            for (uint32_t j = i; j < bridge->subscription_count - 1; j++) {
                bridge->subscriptions[j] = bridge->subscriptions[j + 1];
            }
            bridge->subscription_count--;
            bridge->stats.active_subscriptions = bridge->subscription_count;
            nimcp_mutex_unlock(bridge->base.mutex);
            return 0;
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "ofc_bio_async_unsubscribe_module: operation failed");
    return -1; /* Not found */
}

int ofc_bio_async_update_subscription(
    ofc_bio_async_bridge_t* bridge,
    uint32_t module_id,
    uint32_t msg_types)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ofc_bio_async_update_subscription: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    for (uint32_t i = 0; i < bridge->subscription_count; i++) {
        if (bridge->subscriptions[i].module_id == module_id) {
            bridge->subscriptions[i].msg_type_mask = msg_types;
            nimcp_mutex_unlock(bridge->base.mutex);
            return 0;
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "ofc_bio_async_update_subscription: validation failed");
    return -1; /* Not found */
}

uint32_t ofc_bio_async_get_subscriber_count(
    const ofc_bio_async_bridge_t* bridge,
    ofc_bio_msg_type_t msg_type)
{
    if (!bridge || msg_type >= OFC_BIO_MSG_COUNT) return 0;

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
 * Statistics and Diagnostics API Implementation
 * ============================================================================ */

int ofc_bio_async_get_stats(
    const ofc_bio_async_bridge_t* bridge,
    ofc_bio_async_stats_t* stats)
{
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ofc_bio_async_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }

    *stats = bridge->stats;
    return 0;
}

int ofc_bio_async_reset_stats(ofc_bio_async_bridge_t* bridge)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ofc_bio_async_reset_stats: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    bridge->stats.active_subscriptions = bridge->subscription_count;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

const char* ofc_bio_msg_type_name(ofc_bio_msg_type_t msg_type)
{
    if (msg_type >= OFC_BIO_MSG_COUNT) {
        return "UNKNOWN";
    }
    return g_ofc_msg_type_names[msg_type];
}

void ofc_bio_async_print_summary(const ofc_bio_async_bridge_t* bridge)
{
    if (!bridge) {
        printf("OFC Bio-Async Bridge: NULL\n");
        return;
    }

    printf("=== OFC Bio-Async Bridge Summary ===\n");
    printf("Connected: %s\n", bridge->connected ? "yes" : "no");
    printf("Subscriptions: %u (peak: %u)\n",
           bridge->stats.active_subscriptions,
           bridge->stats.peak_subscriptions);
    printf("Messages sent: %lu\n", (unsigned long)bridge->stats.messages_sent);
    printf("Messages received: %lu\n",
           (unsigned long)bridge->stats.messages_received);
    printf("Broadcasts: %lu\n", (unsigned long)bridge->stats.broadcasts_sent);
    printf("  Value: %lu\n", (unsigned long)bridge->stats.value_broadcasts);
    printf("  Decision: %lu\n",
           (unsigned long)bridge->stats.decision_broadcasts);
    printf("  RPE: %lu\n", (unsigned long)bridge->stats.rpe_broadcasts);
    printf("  Reversal: %lu\n",
           (unsigned long)bridge->stats.reversal_broadcasts);
    printf("  Context: %lu\n", (unsigned long)bridge->stats.context_broadcasts);
    printf("Errors: handler=%lu, routing=%lu\n",
           (unsigned long)bridge->stats.handler_errors,
           (unsigned long)bridge->stats.routing_errors);
    printf("====================================\n");
}
