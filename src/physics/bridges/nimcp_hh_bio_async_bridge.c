/**
 * @file nimcp_hh_bio_async_bridge.c
 * @brief Implementation of Hodgkin-Huxley Bio-Async Integration Bridge
 * @version 1.0.0
 * @date 2026-01-12
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "physics/bridges/nimcp_hh_bio_async_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <time.h>

#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(hh_bio_async_bridge)

#define LOG_MODULE "HH_BIO_ASYNC_BRIDGE"


/* ============================================================================
 * Internal State Cache
 * ============================================================================ */

typedef struct {
    float last_voltage;
    float voltage_derivative;
    uint32_t spike_count;
    float last_spike_time;
    float firing_rate;
} hh_internal_state_t;

/* ============================================================================
 * Internal Bridge Structure
 * ============================================================================ */

struct hh_bio_async_bridge_struct {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    hh_bio_async_config_t config;
    nimcp_hh_neuron_t* neuron;
    nimcp_hh_population_t* population;
    bio_router_t router;

    hh_bio_subscription_t* subscriptions;
    uint32_t subscription_count;
    uint32_t subscription_capacity;

    bool connected;
    uint64_t last_broadcast_us;
    uint32_t time_since_broadcast_ms;

    /* Cached neuron state for performance */
    hh_internal_state_t state;

    hh_bio_async_stats_t stats;
};

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

static uint64_t get_timestamp_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

static hh_bio_subscription_t* find_subscription(hh_bio_async_bridge_t* b, uint32_t module_id) {
    for (uint32_t i = 0; i < b->subscription_count; i++) {
        if (b->subscriptions[i].module_id == module_id && b->subscriptions[i].active) {
            return &b->subscriptions[i];
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_subscription: validation failed");
    return NULL;
}

static float compute_phi(float temperature) {
    float Q10 = NIMCP_HH_Q10_RATE;
    float T_ref = NIMCP_HH_TEMPERATURE_REF;
    return powf(Q10, (temperature - T_ref) / 10.0f);
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

int hh_bio_async_default_config(hh_bio_async_config_t* config) {
    if (!config) {
        LOG_ERROR("NULL config in hh_bio_async_default_config");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL config pointer");
        return -1;
    }

    config->broadcast_interval_ms = HH_BIO_DEFAULT_BROADCAST_INTERVAL_MS;
    config->enable_auto_broadcast = true;
    config->enable_spike_broadcast = true;
    config->max_inbox_process_per_update = 32;
    config->message_ttl_ms = HH_BIO_MESSAGE_TTL_MS;
    config->spike_threshold_mv = HH_BIO_SPIKE_THRESHOLD;
    config->default_channel = BIO_CHANNEL_ACETYLCHOLINE;  /* Fast signaling */
    config->spike_channel = BIO_CHANNEL_ACETYLCHOLINE;
    config->max_subscriptions = HH_BIO_MAX_SUBSCRIPTIONS;
    config->enable_conductance_broadcast = false;
    config->enable_gating_broadcast = false;
    config->enable_threshold_alerts = true;
    config->enable_population_stats = true;
    config->enable_logging = false;

    return 0;
}

hh_bio_async_bridge_t* hh_bio_async_bridge_create(const hh_bio_async_config_t* config) {
    hh_bio_async_bridge_t* bridge = nimcp_calloc(1, sizeof(hh_bio_async_bridge_t));
    NIMCP_API_CHECK_ALLOC(bridge, "Failed to allocate HH bio-async bridge");
    if (bridge_base_init(&bridge->base, 0, "hh_bio_async") != 0) { nimcp_free(bridge); return NULL; }

    if (config) {
        bridge->config = *config;
    } else {
        hh_bio_async_default_config(&bridge->config);
    }

    bridge->subscription_capacity = bridge->config.max_subscriptions;
    bridge->subscriptions = nimcp_calloc(bridge->subscription_capacity,
                                         sizeof(hh_bio_subscription_t));
    if (!bridge->subscriptions) {
        LOG_ERROR("Failed to allocate HH bio-async subscriptions");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate HH bio-async subscriptions");
        nimcp_free(bridge);
        return NULL;
    }

    bridge->last_broadcast_us = get_timestamp_us();
    NIMCP_LOGGING_INFO("Created %s bridge", "hh_bio_async");
    return bridge;
}

void hh_bio_async_bridge_destroy(hh_bio_async_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "hh_bio_async");

    if (bridge->connected) {
        hh_bio_async_disconnect(bridge);
    }

    nimcp_free(bridge->subscriptions);
    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

/* ============================================================================
 * Connection API
 * ============================================================================ */

int hh_bio_async_connect(
    hh_bio_async_bridge_t* bridge,
    nimcp_hh_neuron_t* neuron,
    nimcp_hh_population_t* population,
    bio_router_t router
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hh_bio_async_connect: bridge is NULL");
        return -1;
    }
    if (!neuron && !population) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hh_bio_async_connect: required parameter is NULL (neuron, population)");
        return -1;
    }

    bridge->neuron = neuron;
    bridge->population = population;
    bridge->router = router;
    bridge->connected = true;

    /* Initialize state cache */
    if (neuron) {
        bridge->state.last_voltage = neuron->V;
        bridge->state.spike_count = neuron->spike_count;
    }

    return 0;
}

int hh_bio_async_disconnect(hh_bio_async_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hh_bio_async_disconnect: bridge is NULL");
        return -1;
    }

    bridge->neuron = NULL;
    bridge->population = NULL;
    bridge->router = NULL;
    bridge->connected = false;

    return 0;
}

bool hh_bio_async_is_connected(const hh_bio_async_bridge_t* bridge) {
    return bridge ? bridge->connected : false;
}

/* ============================================================================
 * Message Processing API
 * ============================================================================ */

int hh_bio_async_process_inbox(hh_bio_async_bridge_t* bridge, uint32_t max_messages) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hh_bio_async_process_inbox: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    /* Process incoming modulation requests (current injection, temp changes) */
    uint32_t processed = 0;
    (void)max_messages;

    bridge->stats.messages_received += processed;
    return (int)processed;
}

int hh_bio_async_update(hh_bio_async_bridge_t* bridge, uint32_t delta_ms) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hh_bio_async_update: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    bridge->time_since_broadcast_ms += delta_ms;

    /* Check for spike events if neuron connected */
    if (bridge->neuron && bridge->config.enable_spike_broadcast) {
        if (bridge->neuron->spiked) {
            hh_bio_async_broadcast_spike(bridge, bridge->neuron, get_timestamp_us());
        }
    }

    /* Check for threshold proximity */
    if (bridge->neuron && bridge->config.enable_threshold_alerts) {
        float dist = bridge->neuron->V - bridge->neuron->config.V_rest;
        float threshold_dist = fabsf(NIMCP_HH_SPIKE_THRESHOLD - bridge->neuron->V);
        if (threshold_dist < HH_BIO_THRESHOLD_ALERT_ZONE && dist > 0) {
            hh_bio_async_send_threshold_alert(bridge, bridge->neuron);
        }
    }

    /* Auto-broadcast if enabled and interval elapsed */
    if (bridge->config.enable_auto_broadcast &&
        bridge->time_since_broadcast_ms >= bridge->config.broadcast_interval_ms) {

        if (bridge->neuron) {
            hh_bio_async_broadcast_voltage(bridge, bridge->neuron);
        }
        if (bridge->population && bridge->config.enable_population_stats) {
            hh_bio_async_broadcast_population_rate(bridge, bridge->population);
        }
        bridge->time_since_broadcast_ms = 0;
    }

    /* Update state cache */
    if (bridge->neuron) {
        float dv = bridge->neuron->V - bridge->state.last_voltage;
        bridge->state.voltage_derivative = dv / (float)delta_ms;
        bridge->state.last_voltage = bridge->neuron->V;
    }

    return 0;
}

/* ============================================================================
 * Broadcast API
 * ============================================================================ */

int hh_bio_async_broadcast_voltage(
    hh_bio_async_bridge_t* bridge,
    const nimcp_hh_neuron_t* neuron
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hh_bio_async_broadcast_voltage: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }
    if (!neuron) neuron = bridge->neuron;
    if (!neuron) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hh_bio_async_broadcast_voltage: neuron is NULL");
        return -1;
    }

    hh_bio_voltage_msg_t msg = {0};
    msg.header.type = 0x1300;  /* BIO_MSG_PHYSICS_HH_VOLTAGE */
    msg.header.source_module = 0x4500;  /* BIO_MODULE_PHYSICS_HH */
    msg.header.channel = bridge->config.default_channel;
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;
    msg.header.timestamp_us = get_timestamp_us();

    msg.membrane_voltage = neuron->V;
    msg.previous_voltage = neuron->V_prev;
    msg.voltage_derivative = bridge->state.voltage_derivative;

    /* Gating variables from Na channel */
    msg.m = neuron->channels[NIMCP_ION_CHANNEL_NA].activation.value;
    msg.h = neuron->channels[NIMCP_ION_CHANNEL_NA].inactivation.value;
    msg.n = neuron->channels[NIMCP_ION_CHANNEL_K].activation.value;

    msg.neuron_id = neuron->module_id;
    msg.population_id = 0;
    msg.timestamp_us = msg.header.timestamp_us;

    bridge->stats.voltage_broadcasts++;
    bridge->stats.broadcasts_sent++;
    bridge->last_broadcast_us = msg.timestamp_us;

    return 0;
}

int hh_bio_async_broadcast_spike(
    hh_bio_async_bridge_t* bridge,
    const nimcp_hh_neuron_t* neuron,
    uint64_t spike_time_us
) {
    if (!bridge || !bridge->connected || !neuron) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hh_bio_async_broadcast_spike: required parameter is NULL (bridge, bridge->connected, neuron)");
        return -1;
    }

    hh_bio_spike_msg_t msg = {0};
    msg.header.type = 0x1301;  /* BIO_MSG_PHYSICS_HH_SPIKE */
    msg.header.source_module = 0x4500;
    msg.header.channel = bridge->config.spike_channel;
    msg.header.flags = BIO_MSG_FLAG_BROADCAST | BIO_MSG_FLAG_URGENT;
    msg.header.timestamp_us = get_timestamp_us();

    msg.spike_amplitude = neuron->V;
    msg.spike_width = 1.0f;  /* Typical HH spike ~1ms */
    msg.interspike_interval = neuron->time - neuron->last_spike_time;
    msg.pre_spike_voltage = neuron->V_prev;
    msg.instantaneous_rate = neuron->avg_firing_rate;
    msg.neuron_id = neuron->module_id;
    msg.spike_number = neuron->spike_count;
    msg.spike_time_us = spike_time_us;

    bridge->stats.spike_broadcasts++;
    bridge->stats.broadcasts_sent++;

    return 0;
}

int hh_bio_async_broadcast_conductance(
    hh_bio_async_bridge_t* bridge,
    const nimcp_hh_neuron_t* neuron
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hh_bio_async_broadcast_conductance: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }
    if (!neuron) neuron = bridge->neuron;
    if (!neuron) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hh_bio_async_broadcast_conductance: neuron is NULL");
        return -1;
    }

    hh_bio_conductance_msg_t msg = {0};
    msg.header.type = 0x1302;
    msg.header.source_module = 0x4500;
    msg.header.channel = bridge->config.default_channel;
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;
    msg.header.timestamp_us = get_timestamp_us();

    msg.g_Na = neuron->channels[NIMCP_ION_CHANNEL_NA].g_current;
    msg.g_K = neuron->channels[NIMCP_ION_CHANNEL_K].g_current;
    msg.g_L = neuron->channels[NIMCP_ION_CHANNEL_LEAK].g_current;
    msg.g_Ca = neuron->channels[NIMCP_ION_CHANNEL_CA_L].g_current;

    msg.I_Na = neuron->channels[NIMCP_ION_CHANNEL_NA].I_current;
    msg.I_K = neuron->channels[NIMCP_ION_CHANNEL_K].I_current;
    msg.I_L = neuron->channels[NIMCP_ION_CHANNEL_LEAK].I_current;
    msg.I_total = msg.I_Na + msg.I_K + msg.I_L;

    msg.neuron_id = neuron->module_id;
    msg.timestamp_us = msg.header.timestamp_us;

    bridge->stats.conductance_broadcasts++;
    bridge->stats.broadcasts_sent++;

    return 0;
}

int hh_bio_async_broadcast_gating(
    hh_bio_async_bridge_t* bridge,
    const nimcp_hh_neuron_t* neuron
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hh_bio_async_broadcast_gating: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }
    if (!neuron) neuron = bridge->neuron;
    if (!neuron) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hh_bio_async_broadcast_gating: neuron is NULL");
        return -1;
    }

    hh_bio_gating_msg_t msg = {0};
    msg.header.type = 0x1303;
    msg.header.source_module = 0x4500;
    msg.header.channel = bridge->config.default_channel;
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;
    msg.header.timestamp_us = get_timestamp_us();

    const nimcp_ion_channel_t* na = &neuron->channels[NIMCP_ION_CHANNEL_NA];
    const nimcp_ion_channel_t* k = &neuron->channels[NIMCP_ION_CHANNEL_K];

    msg.m_value = na->activation.value;
    msg.m_inf = na->activation.inf;
    msg.m_tau = na->activation.tau;

    msg.h_value = na->inactivation.value;
    msg.h_inf = na->inactivation.inf;
    msg.h_tau = na->inactivation.tau;

    msg.n_value = k->activation.value;
    msg.n_inf = k->activation.inf;
    msg.n_tau = k->activation.tau;

    msg.neuron_id = neuron->module_id;
    msg.timestamp_us = msg.header.timestamp_us;

    bridge->stats.broadcasts_sent++;
    return 0;
}

int hh_bio_async_broadcast_population_rate(
    hh_bio_async_bridge_t* bridge,
    const nimcp_hh_population_t* population
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hh_bio_async_broadcast_population_rate: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }
    if (!population) population = bridge->population;
    if (!population) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hh_bio_async_broadcast_population_rate: population is NULL");
        return -1;
    }

    hh_bio_population_rate_msg_t msg = {0};
    msg.header.type = 0x1306;
    msg.header.source_module = 0x4500;
    msg.header.channel = bridge->config.default_channel;
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;
    msg.header.timestamp_us = get_timestamp_us();

    msg.mean_rate = population->mean_firing_rate;
    msg.synchrony = population->synchrony;
    msg.total_neurons = population->count;

    /* Count active neurons */
    uint32_t active = 0;
    for (uint32_t i = 0; i < population->count; i++) {
        if (population->neurons[i].avg_firing_rate > 0.1f) {
            active++;
        }
    }
    msg.active_neurons = active;

    msg.window_ms = 100.0f;
    msg.population_id = population->module_id;
    msg.timestamp_us = msg.header.timestamp_us;

    bridge->stats.population_broadcasts++;
    bridge->stats.broadcasts_sent++;

    return 0;
}

int hh_bio_async_send_threshold_alert(
    hh_bio_async_bridge_t* bridge,
    const nimcp_hh_neuron_t* neuron
) {
    if (!bridge || !bridge->connected || !neuron) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hh_bio_async_send_threshold_alert: required parameter is NULL (bridge, bridge->connected, neuron)");
        return -1;
    }

    hh_bio_threshold_alert_msg_t msg = {0};
    msg.header.type = 0x1307;
    msg.header.source_module = 0x4500;
    msg.header.channel = bridge->config.spike_channel;
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;
    msg.header.timestamp_us = get_timestamp_us();

    msg.current_voltage = neuron->V;
    msg.threshold_voltage = NIMCP_HH_SPIKE_THRESHOLD;
    msg.distance_to_threshold = NIMCP_HH_SPIKE_THRESHOLD - neuron->V;
    msg.voltage_trend = bridge->state.voltage_derivative;
    msg.approaching_threshold = (bridge->state.voltage_derivative > 0);
    msg.neuron_id = neuron->module_id;
    msg.timestamp_us = msg.header.timestamp_us;

    bridge->stats.threshold_alerts_sent++;
    bridge->stats.messages_sent++;

    return 0;
}

int hh_bio_async_broadcast_temperature(
    hh_bio_async_bridge_t* bridge,
    float old_temp,
    float new_temp
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hh_bio_async_broadcast_temperature: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    hh_bio_temperature_msg_t msg = {0};
    msg.header.type = 0x1305;
    msg.header.source_module = 0x4500;
    msg.header.channel = bridge->config.default_channel;
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;
    msg.header.timestamp_us = get_timestamp_us();

    msg.new_temperature = new_temp;
    msg.old_temperature = old_temp;
    msg.phi_factor = compute_phi(new_temp);
    msg.source_module = 0x4500;
    msg.timestamp_us = msg.header.timestamp_us;

    bridge->stats.broadcasts_sent++;
    return 0;
}

/* ============================================================================
 * Subscription Management API
 * ============================================================================ */

int hh_bio_async_subscribe_module(
    hh_bio_async_bridge_t* bridge,
    uint32_t module_id,
    uint32_t msg_types
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hh_bio_async_subscribe_module: bridge is NULL");
        return -1;
    }

    /* Check if already subscribed */
    hh_bio_subscription_t* existing = find_subscription(bridge, module_id);
    if (existing) {
        existing->msg_type_mask |= msg_types;
        return 0;
    }

    /* Find free slot */
    if (bridge->subscription_count >= bridge->subscription_capacity) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "hh_bio_async_subscribe_module: capacity exceeded");
        return -1;
    }

    hh_bio_subscription_t* sub = &bridge->subscriptions[bridge->subscription_count++];
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

int hh_bio_async_unsubscribe_module(hh_bio_async_bridge_t* bridge, uint32_t module_id) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hh_bio_async_unsubscribe_module: bridge is NULL");
        return -1;
    }

    for (uint32_t i = 0; i < bridge->subscription_count; i++) {
        if (bridge->subscriptions[i].module_id == module_id) {
            bridge->subscriptions[i].active = false;
            bridge->stats.active_subscriptions--;
            return 0;
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hh_bio_async_unsubscribe_module: validation failed");
    return -1;
}

int hh_bio_async_update_subscription(
    hh_bio_async_bridge_t* bridge,
    uint32_t module_id,
    uint32_t msg_types
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hh_bio_async_update_subscription: bridge is NULL");
        return -1;
    }

    hh_bio_subscription_t* sub = find_subscription(bridge, module_id);
    if (!sub) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hh_bio_async_update_subscription: sub is NULL");
        return -1;
    }

    sub->msg_type_mask = msg_types;
    return 0;
}

uint32_t hh_bio_async_get_subscriber_count(
    const hh_bio_async_bridge_t* bridge,
    hh_bio_msg_type_t msg_type
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

int hh_bio_async_get_stats(
    const hh_bio_async_bridge_t* bridge,
    hh_bio_async_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hh_bio_async_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }
    *stats = bridge->stats;
    return 0;
}

int hh_bio_async_reset_stats(hh_bio_async_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hh_bio_async_reset_stats: bridge is NULL");
        return -1;
    }
    memset(&bridge->stats, 0, sizeof(hh_bio_async_stats_t));
    return 0;
}

static const char* hh_msg_type_names[] = {
    "VOLTAGE_STATE",
    "SPIKE_EVENT",
    "CONDUCTANCE",
    "GATING_STATE",
    "CURRENT_REQUEST",
    "TEMPERATURE_CHANGE",
    "POPULATION_RATE",
    "THRESHOLD_ALERT"
};

const char* hh_bio_msg_type_name(hh_bio_msg_type_t msg_type) {
    if (msg_type >= HH_BIO_MSG_COUNT) return "UNKNOWN";
    return hh_msg_type_names[msg_type];
}

void hh_bio_async_print_summary(const hh_bio_async_bridge_t* bridge) {
    if (!bridge) {
        printf("HH Bio-Async Bridge: NULL\n");
        return;
    }

    printf("=== HH Bio-Async Bridge Summary ===\n");
    printf("Connected: %s\n", bridge->connected ? "Yes" : "No");
    printf("Subscriptions: %u/%u\n",
           bridge->stats.active_subscriptions, bridge->subscription_capacity);
    printf("Messages sent: %lu\n", (unsigned long)bridge->stats.messages_sent);
    printf("Broadcasts: %lu\n", (unsigned long)bridge->stats.broadcasts_sent);
    printf("  Voltage: %lu\n", (unsigned long)bridge->stats.voltage_broadcasts);
    printf("  Spike: %lu\n", (unsigned long)bridge->stats.spike_broadcasts);
    printf("  Population: %lu\n", (unsigned long)bridge->stats.population_broadcasts);
    printf("Errors: %lu\n", (unsigned long)bridge->stats.handler_errors);
    printf("===================================\n");
}
