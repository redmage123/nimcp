/**
 * @file nimcp_glial_bio_async_bridge.c
 * @brief Implementation of Glial Integration System Bio-Async Bridge
 * @version 1.0.0
 * @date 2026-01-13
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "glial/integration/nimcp_glial_bio_async_bridge.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(glial_bio_async_bridge)

#define LOG_MODULE "GLIAL_BIO_ASYNC_BRIDGE"


/* ============================================================================
 * Internal State Cache
 * ============================================================================ */

typedef struct {
    float avg_calcium_level;
    float avg_myelination;
    uint32_t active_astrocytes;
    uint32_t active_microglia;
    float inflammation_level;
} glial_internal_state_t;

/* ============================================================================
 * Internal Bridge Structure
 * ============================================================================ */

struct glial_bio_async_bridge_struct {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    glial_bio_async_config_t config;
    glial_integration_t* glial_integration;
    bio_router_t router;

    glial_bio_subscription_t* subscriptions;
    uint32_t subscription_count;
    uint32_t subscription_capacity;

    bool connected;
    uint64_t last_broadcast_us;
    uint32_t time_since_broadcast_ms;

    /* Cached state */
    glial_internal_state_t state;

    glial_bio_async_stats_t stats;
};

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

static uint64_t glial_get_timestamp_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

static glial_bio_subscription_t* glial_find_subscription(
    glial_bio_async_bridge_t* b, uint32_t module_id
) {
    for (uint32_t i = 0; i < b->subscription_count; i++) {
        if (b->subscriptions[i].module_id == module_id && b->subscriptions[i].active) {
            return &b->subscriptions[i];
        }
    }
    return NULL;  /* Not found - no active subscription for this module */
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

int glial_bio_async_default_config(glial_bio_async_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "glial_bio_async_default_config: config is NULL");
        return -1;
    }

    config->state_broadcast_interval_ms = GLIAL_BIO_DEFAULT_BROADCAST_INTERVAL_MS;
    config->enable_auto_broadcast = true;
    config->max_inbox_process_per_update = 32;
    config->message_ttl_ms = GLIAL_BIO_MESSAGE_TTL_MS;
    config->default_channel = BIO_CHANNEL_SEROTONIN;
    config->wave_channel = BIO_CHANNEL_ACETYLCHOLINE;
    config->max_subscriptions = GLIAL_BIO_MAX_SUBSCRIPTIONS;
    config->enable_astrocyte_routing = true;
    config->enable_myelination_routing = true;
    config->enable_pruning_routing = true;
    config->enable_metabolic_routing = true;
    config->enable_logging = false;

    return 0;
}

glial_bio_async_bridge_t* glial_bio_async_bridge_create(
    const glial_bio_async_config_t* config
) {
    glial_bio_async_bridge_t* bridge = nimcp_calloc(1, sizeof(glial_bio_async_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bridge is NULL");

        return NULL;

    }

    if (config) {
        bridge->config = *config;
    } else {
        glial_bio_async_default_config(&bridge->config);
    }

    bridge->subscription_capacity = bridge->config.max_subscriptions;
    bridge->subscriptions = nimcp_calloc(
        bridge->subscription_capacity, sizeof(glial_bio_subscription_t)
    );
    if (!bridge->subscriptions) {
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "glial_bio_async_bridge_create: bridge->subscriptions is NULL");
        return NULL;
    }

    bridge->last_broadcast_us = glial_get_timestamp_us();

    return bridge;
}

void glial_bio_async_bridge_destroy(glial_bio_async_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "glial_bio_async");

    if (bridge->connected) {
        glial_bio_async_disconnect(bridge);
    }

    nimcp_free(bridge->subscriptions);
    nimcp_free(bridge);
}

/* ============================================================================
 * Connection API
 * ============================================================================ */

int glial_bio_async_connect(
    glial_bio_async_bridge_t* bridge,
    glial_integration_t* glial_integration,
    bio_router_t router
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "glial_bio_async_connect: bridge is NULL");
        return -1;
    }

    bridge->glial_integration = glial_integration;
    bridge->router = router;
    bridge->connected = true;

    return 0;
}

int glial_bio_async_disconnect(glial_bio_async_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "glial_bio_async_disconnect: bridge is NULL");
        return -1;
    }

    bridge->glial_integration = NULL;
    bridge->router = NULL;
    bridge->connected = false;

    return 0;
}

bool glial_bio_async_is_connected(const glial_bio_async_bridge_t* bridge) {
    return bridge ? bridge->connected : false;
}

/* ============================================================================
 * Message Processing API
 * ============================================================================ */

int glial_bio_async_process_inbox(
    glial_bio_async_bridge_t* bridge,
    uint32_t max_messages
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "glial_bio_async_process_inbox: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    uint32_t processed = 0;
    (void)max_messages;

    bridge->stats.messages_received += processed;
    return (int)processed;
}

int glial_bio_async_update(
    glial_bio_async_bridge_t* bridge,
    uint32_t delta_ms
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "glial_bio_async_update: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    bridge->time_since_broadcast_ms += delta_ms;

    if (bridge->config.enable_auto_broadcast &&
        bridge->time_since_broadcast_ms >= bridge->config.state_broadcast_interval_ms) {
        bridge->time_since_broadcast_ms = 0;
    }

    return 0;
}

/* ============================================================================
 * Broadcast API
 * ============================================================================ */

int glial_bio_async_broadcast_astrocyte_signal(
    glial_bio_async_bridge_t* bridge,
    uint32_t astrocyte_id,
    float calcium_level,
    float activity_level
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "glial_bio_async_broadcast_astrocyte_signal: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }
    if (!bridge->config.enable_astrocyte_routing) return 0;

    glial_bio_astrocyte_msg_t msg = {0};
    msg.header.type = BIO_MSG_ASTROCYTE_CALCIUM_WAVE;
    msg.header.source_module = BIO_MODULE_GLIAL_INTEGRATION;
    msg.header.channel = bridge->config.default_channel;
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;
    msg.header.timestamp_us = glial_get_timestamp_us();

    msg.astrocyte_id = astrocyte_id;
    msg.calcium_level = calcium_level;
    msg.activity_level = activity_level;
    msg.timestamp_us = msg.header.timestamp_us;

    bridge->stats.astrocyte_signals_sent++;
    bridge->stats.broadcasts_sent++;
    bridge->last_broadcast_us = msg.timestamp_us;

    return 0;
}

int glial_bio_async_broadcast_calcium_wave(
    glial_bio_async_bridge_t* bridge,
    uint32_t wave_id,
    uint32_t source_astrocyte,
    float wave_amplitude
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "glial_bio_async_broadcast_calcium_wave: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }
    if (!bridge->config.enable_astrocyte_routing) return 0;

    glial_bio_calcium_wave_msg_t msg = {0};
    msg.header.type = BIO_MSG_ASTROCYTE_CALCIUM_WAVE;
    msg.header.source_module = BIO_MODULE_GLIAL_INTEGRATION;
    msg.header.channel = bridge->config.wave_channel;
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;
    msg.header.timestamp_us = glial_get_timestamp_us();

    msg.wave_id = wave_id;
    msg.source_astrocyte = source_astrocyte;
    msg.wave_amplitude = wave_amplitude;
    msg.propagation_speed = 20.0f; /* um/s typical */
    msg.wave_onset_us = msg.header.timestamp_us;
    msg.timestamp_us = msg.header.timestamp_us;

    bridge->stats.calcium_waves_sent++;
    bridge->stats.broadcasts_sent++;

    return 0;
}

int glial_bio_async_broadcast_myelination(
    glial_bio_async_bridge_t* bridge,
    uint32_t oligodendrocyte_id,
    uint32_t neuron_id,
    float myelination_factor
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "glial_bio_async_broadcast_myelination: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }
    if (!bridge->config.enable_myelination_routing) return 0;

    glial_bio_myelination_msg_t msg = {0};
    msg.header.type = BIO_MSG_OLIGODENDROCYTE_MYELINATE;
    msg.header.source_module = BIO_MODULE_GLIAL_INTEGRATION;
    msg.header.channel = bridge->config.default_channel;
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;
    msg.header.timestamp_us = glial_get_timestamp_us();

    msg.oligodendrocyte_id = oligodendrocyte_id;
    msg.neuron_id = neuron_id;
    msg.myelination_factor = myelination_factor;
    msg.conduction_multiplier = 1.0f + myelination_factor * 49.0f; /* Up to 50x speedup */
    msg.is_complete = (myelination_factor >= 0.95f);
    msg.timestamp_us = msg.header.timestamp_us;

    bridge->stats.myelination_events_sent++;
    bridge->stats.broadcasts_sent++;

    return 0;
}

int glial_bio_async_broadcast_prune_event(
    glial_bio_async_bridge_t* bridge,
    uint32_t microglia_id,
    uint32_t synapse_id,
    float synapse_weight_before
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "glial_bio_async_broadcast_prune_event: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }
    if (!bridge->config.enable_pruning_routing) return 0;

    glial_bio_prune_msg_t msg = {0};
    msg.header.type = BIO_MSG_MICROGLIA_PRUNE_REQUEST;
    msg.header.source_module = BIO_MODULE_GLIAL_INTEGRATION;
    msg.header.channel = bridge->config.default_channel;
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;
    msg.header.timestamp_us = glial_get_timestamp_us();

    msg.microglia_id = microglia_id;
    msg.synapse_id = synapse_id;
    msg.synapse_weight_before = synapse_weight_before;
    msg.pruning_reason = (synapse_weight_before < 0.01f) ? 0 : 1; /* weak vs tagged */
    msg.timestamp_us = msg.header.timestamp_us;

    bridge->stats.prune_events_sent++;
    bridge->stats.broadcasts_sent++;

    return 0;
}

int glial_bio_async_broadcast_surveillance(
    glial_bio_async_bridge_t* bridge,
    uint32_t microglia_id,
    uint32_t region_id,
    float threat_detected
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "glial_bio_async_broadcast_surveillance: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }
    if (!bridge->config.enable_pruning_routing) return 0;

    glial_bio_surveillance_msg_t msg = {0};
    msg.header.type = BIO_MSG_MICROGLIA_ALERT;
    msg.header.source_module = BIO_MODULE_GLIAL_INTEGRATION;
    msg.header.channel = (threat_detected > 0.5f) ?
        BIO_CHANNEL_NOREPINEPHRINE : bridge->config.default_channel;
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;
    if (threat_detected > 0.5f) {
        msg.header.flags |= BIO_MSG_FLAG_URGENT;
    }
    msg.header.timestamp_us = glial_get_timestamp_us();

    msg.microglia_id = microglia_id;
    msg.region_id = region_id;
    msg.threat_detected = threat_detected;
    msg.is_activated = (threat_detected > 0.3f);
    msg.is_phagocytic = (threat_detected > 0.7f);
    msg.timestamp_us = msg.header.timestamp_us;

    bridge->stats.broadcasts_sent++;

    return 0;
}

int glial_bio_async_broadcast_metabolic_support(
    glial_bio_async_bridge_t* bridge,
    uint32_t astrocyte_id,
    uint32_t target_neuron_id,
    float atp_delivered
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "glial_bio_async_broadcast_metabolic_support: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }
    if (!bridge->config.enable_metabolic_routing) return 0;

    glial_bio_metabolic_msg_t msg = {0};
    msg.header.type = BIO_MSG_METABOLIC_SUPPLY;
    msg.header.source_module = BIO_MODULE_GLIAL_INTEGRATION;
    msg.header.channel = bridge->config.default_channel;
    msg.header.timestamp_us = glial_get_timestamp_us();

    if (target_neuron_id == 0) {
        msg.header.flags = BIO_MSG_FLAG_BROADCAST;
    } else {
        msg.header.target_module = target_neuron_id;
    }

    msg.astrocyte_id = astrocyte_id;
    msg.target_neuron_id = target_neuron_id;
    msg.atp_delivered = atp_delivered;
    msg.glucose_delivered = atp_delivered * 0.3f;
    msg.lactate_delivered = atp_delivered * 0.1f;
    msg.supply_efficiency = 0.9f;
    msg.timestamp_us = msg.header.timestamp_us;

    bridge->stats.metabolic_supports_sent++;
    bridge->stats.broadcasts_sent++;

    return 0;
}

int glial_bio_async_broadcast_inflammation(
    glial_bio_async_bridge_t* bridge,
    uint32_t region_id,
    float inflammation_level,
    bool is_acute
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "glial_bio_async_broadcast_inflammation: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    glial_bio_inflammation_msg_t msg = {0};
    msg.header.type = BIO_MSG_INFLAMMATION_CHANGE;
    msg.header.source_module = BIO_MODULE_GLIAL_INTEGRATION;
    msg.header.channel = (inflammation_level > 0.7f) ?
        BIO_CHANNEL_NOREPINEPHRINE : bridge->config.default_channel;
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;
    if (inflammation_level > 0.7f) {
        msg.header.flags |= BIO_MSG_FLAG_URGENT;
    }
    msg.header.timestamp_us = glial_get_timestamp_us();

    msg.region_id = region_id;
    msg.inflammation_level = inflammation_level;
    msg.previous_level = bridge->state.inflammation_level;
    msg.is_acute = is_acute;
    msg.is_resolving = (inflammation_level < bridge->state.inflammation_level);
    msg.timestamp_us = msg.header.timestamp_us;

    bridge->state.inflammation_level = inflammation_level;
    bridge->stats.broadcasts_sent++;

    return 0;
}

/* ============================================================================
 * Subscription Management API
 * ============================================================================ */

int glial_bio_async_subscribe_module(
    glial_bio_async_bridge_t* bridge,
    uint32_t module_id,
    uint32_t msg_types
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "glial_bio_async_subscribe_module: bridge is NULL");
        return -1;
    }

    glial_bio_subscription_t* existing = glial_find_subscription(bridge, module_id);
    if (existing) {
        existing->msg_type_mask |= msg_types;
        return 0;
    }

    if (bridge->subscription_count >= bridge->subscription_capacity) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "glial_bio_async_subscribe_module: capacity exceeded");
        return -1;
    }

    glial_bio_subscription_t* sub = &bridge->subscriptions[bridge->subscription_count++];
    sub->module_id = module_id;
    sub->msg_type_mask = msg_types;
    sub->active = true;
    sub->subscription_time = glial_get_timestamp_us();
    sub->messages_sent = 0;

    bridge->stats.active_subscriptions = bridge->subscription_count;
    if (bridge->subscription_count > bridge->stats.peak_subscriptions) {
        bridge->stats.peak_subscriptions = bridge->subscription_count;
    }

    return 0;
}

int glial_bio_async_unsubscribe_module(
    glial_bio_async_bridge_t* bridge,
    uint32_t module_id
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "glial_bio_async_unsubscribe_module: bridge is NULL");
        return -1;
    }

    for (uint32_t i = 0; i < bridge->subscription_count; i++) {
        if (bridge->subscriptions[i].module_id == module_id) {
            bridge->subscriptions[i].active = false;
            bridge->stats.active_subscriptions--;
            return 0;
        }
    }

    return -1;  /* Module not found - normal lookup miss */
}

uint32_t glial_bio_async_get_subscriber_count(
    const glial_bio_async_bridge_t* bridge,
    glial_bio_msg_type_t msg_type
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

int glial_bio_async_get_stats(
    const glial_bio_async_bridge_t* bridge,
    glial_bio_async_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "glial_bio_async_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }
    *stats = bridge->stats;
    return 0;
}

int glial_bio_async_reset_stats(glial_bio_async_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "glial_bio_async_reset_stats: bridge is NULL");
        return -1;
    }

    uint32_t active = bridge->stats.active_subscriptions;
    uint32_t peak = bridge->stats.peak_subscriptions;
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    bridge->stats.active_subscriptions = active;
    bridge->stats.peak_subscriptions = peak;

    return 0;
}

static const char* glial_bio_msg_type_names[] = {
    "ASTROCYTE_SIGNAL", "CALCIUM_WAVE", "GLIOTRANSMITTER",
    "SYNAPTIC_MODULATION", "MYELINATION", "CONDUCTION_CHANGE",
    "PRUNE_EVENT", "SURVEILLANCE", "INFLAMMATION",
    "METABOLIC_SUPPORT", "ATP_REQUEST", "QUERY_STATE"
};

const char* glial_bio_msg_type_name(glial_bio_msg_type_t msg_type) {
    if (msg_type >= GLIAL_BIO_MSG_COUNT) return "UNKNOWN";
    return glial_bio_msg_type_names[msg_type];
}

void glial_bio_async_print_summary(const glial_bio_async_bridge_t* bridge) {
    if (!bridge) {
        printf("Glial Bio-Async Bridge: NULL\n");
        return;
    }

    printf("Glial Bio-Async Bridge Summary:\n");
    printf("  Connected: %s\n", bridge->connected ? "yes" : "no");
    printf("  Subscriptions: %u (peak: %u)\n",
           bridge->stats.active_subscriptions,
           bridge->stats.peak_subscriptions);
    printf("  Messages sent: %lu, received: %lu\n",
           (unsigned long)bridge->stats.messages_sent,
           (unsigned long)bridge->stats.messages_received);
    printf("  Broadcasts: %lu (astrocyte: %lu, waves: %lu, myelin: %lu, prune: %lu)\n",
           (unsigned long)bridge->stats.broadcasts_sent,
           (unsigned long)bridge->stats.astrocyte_signals_sent,
           (unsigned long)bridge->stats.calcium_waves_sent,
           (unsigned long)bridge->stats.myelination_events_sent,
           (unsigned long)bridge->stats.prune_events_sent);
    printf("  Errors: handler=%lu, routing=%lu\n",
           (unsigned long)bridge->stats.handler_errors,
           (unsigned long)bridge->stats.routing_errors);
}
