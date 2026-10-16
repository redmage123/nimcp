/**
 * @file nimcp_thermo_bio_async_bridge.c
 * @brief Implementation of Thermodynamics Bio-Async Integration Bridge
 * @version 1.0.0
 * @date 2026-01-12
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "physics/bridges/nimcp_thermo_bio_async_bridge.h"
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

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(thermo_bio_async_bridge)

#define LOG_MODULE "THERMO_BIO_ASYNC_BRIDGE"


/* ============================================================================
 * Internal State Cache
 * ============================================================================ */

typedef struct {
    double initial_atp;
    double last_temperature;
    double last_entropy_rate;
    bool atp_warning_sent;
    bool atp_critical_sent;
} thermo_internal_state_t;

/* ============================================================================
 * Internal Bridge Structure
 * ============================================================================ */

struct thermo_bio_async_bridge_struct {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    thermo_bio_async_config_t config;
    nimcp_thermodynamic_state_t* state;
    bio_router_t router;

    thermo_bio_subscription_t* subscriptions;
    uint32_t subscription_count;
    uint32_t subscription_capacity;

    bool connected;
    uint64_t last_broadcast_us;
    uint32_t time_since_broadcast_ms;

    thermo_internal_state_t internal;
    thermo_bio_async_stats_t stats;
};

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

static uint64_t get_timestamp_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

static thermo_bio_subscription_t* find_subscription(
    thermo_bio_async_bridge_t* b, uint32_t module_id
) {
    for (uint32_t i = 0; i < b->subscription_count; i++) {
        if (b->subscriptions[i].module_id == module_id &&
            b->subscriptions[i].active) {
            return &b->subscriptions[i];
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_subscription: operation failed");
    return NULL;
}

static double kelvin_to_celsius(double k) {
    return k - 273.15;
}

static double compute_landauer_limit(double temp_k) {
    return NIMCP_THERMO_KB * temp_k * log(2.0);
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

int thermo_bio_async_default_config(thermo_bio_async_config_t* config) {
    if (!config) {
        LOG_ERROR("NULL config in thermo_bio_async_default_config");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL config pointer");
        return -1;
    }

    config->broadcast_interval_ms = THERMO_BIO_DEFAULT_BROADCAST_INTERVAL_MS;
    config->enable_auto_broadcast = true;
    config->max_inbox_process_per_update = 16;
    config->message_ttl_ms = THERMO_BIO_MESSAGE_TTL_MS;
    config->default_channel = BIO_CHANNEL_SEROTONIN;  /* Slow, metabolic */
    config->alert_channel = BIO_CHANNEL_NOREPINEPHRINE;  /* Urgent */
    config->max_subscriptions = THERMO_BIO_MAX_SUBSCRIPTIONS;
    config->atp_warning_threshold = THERMO_BIO_ATP_WARNING_THRESHOLD;
    config->atp_critical_threshold = THERMO_BIO_ATP_CRITICAL_THRESHOLD;
    config->enable_atp_alerts = true;
    config->enable_entropy_broadcast = true;
    config->enable_efficiency_broadcast = true;
    config->enable_landauer_tracking = true;
    config->enable_logging = false;

    return 0;
}

thermo_bio_async_bridge_t* thermo_bio_async_bridge_create(
    const thermo_bio_async_config_t* config
) {
    thermo_bio_async_bridge_t* bridge = nimcp_calloc(1, sizeof(*bridge));
    NIMCP_API_CHECK_ALLOC(bridge, "Failed to allocate thermo bio-async bridge");
    if (bridge_base_init(&bridge->base, 0, "thermo_bio_async") != 0) { nimcp_free(bridge); return NULL; }

    if (config) {
        bridge->config = *config;
    } else {
        thermo_bio_async_default_config(&bridge->config);
    }

    bridge->subscription_capacity = bridge->config.max_subscriptions;
    bridge->subscriptions = nimcp_calloc(bridge->subscription_capacity,
                                         sizeof(thermo_bio_subscription_t));
    if (!bridge->subscriptions) {
        LOG_ERROR("Failed to allocate thermo bio-async subscriptions");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate thermo bio-async subscriptions");
        nimcp_free(bridge);
        return NULL;
    }

    bridge->last_broadcast_us = get_timestamp_us();
    return bridge;
}

void thermo_bio_async_bridge_destroy(thermo_bio_async_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "thermo_bio_async");

    if (bridge->connected) {
        thermo_bio_async_disconnect(bridge);
    }

    nimcp_free(bridge->subscriptions);
    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

/* ============================================================================
 * Connection API
 * ============================================================================ */

int thermo_bio_async_connect(
    thermo_bio_async_bridge_t* bridge,
    nimcp_thermodynamic_state_t* state,
    bio_router_t router
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "thermo_bio_async_connect: required parameter is NULL (bridge, state)");
        return -1;
    }

    bridge->state = state;
    bridge->router = router;
    bridge->connected = true;

    /* Cache initial ATP for fraction calculations */
    bridge->internal.initial_atp = state->atp_available;
    bridge->internal.atp_warning_sent = false;
    bridge->internal.atp_critical_sent = false;

    return 0;
}

int thermo_bio_async_disconnect(thermo_bio_async_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "thermo_bio_async_disconnect: bridge is NULL");
        return -1;
    }

    bridge->state = NULL;
    bridge->router = NULL;
    bridge->connected = false;

    return 0;
}

bool thermo_bio_async_is_connected(const thermo_bio_async_bridge_t* bridge) {
    return bridge ? bridge->connected : false;
}

/* ============================================================================
 * Message Processing API
 * ============================================================================ */

int thermo_bio_async_process_inbox(
    thermo_bio_async_bridge_t* bridge,
    uint32_t max_messages
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "thermo_bio_async_process_inbox: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    uint32_t processed = 0;
    (void)max_messages;

    bridge->stats.messages_received += processed;
    return (int)processed;
}

int thermo_bio_async_update(thermo_bio_async_bridge_t* bridge, uint32_t delta_ms) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "thermo_bio_async_update: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    bridge->time_since_broadcast_ms += delta_ms;

    /* Check ATP thresholds and send alerts */
    if (bridge->config.enable_atp_alerts && bridge->state) {
        double atp_frac = bridge->state->atp_available / bridge->internal.initial_atp;

        if (atp_frac <= bridge->config.atp_critical_threshold &&
            !bridge->internal.atp_critical_sent) {
            thermo_bio_async_send_atp_critical(bridge, 2);  /* Emergency */
            bridge->internal.atp_critical_sent = true;
        } else if (atp_frac <= bridge->config.atp_warning_threshold &&
                   !bridge->internal.atp_warning_sent) {
            thermo_bio_async_send_atp_critical(bridge, 0);  /* Warning */
            bridge->internal.atp_warning_sent = true;
        }
    }

    /* Auto-broadcast if interval elapsed */
    if (bridge->config.enable_auto_broadcast &&
        bridge->time_since_broadcast_ms >= bridge->config.broadcast_interval_ms) {

        thermo_bio_async_broadcast_atp_level(bridge);

        if (bridge->config.enable_entropy_broadcast) {
            thermo_bio_async_broadcast_entropy(bridge);
        }

        if (bridge->config.enable_efficiency_broadcast) {
            thermo_bio_async_broadcast_efficiency(bridge);
        }

        bridge->time_since_broadcast_ms = 0;
    }

    return 0;
}

/* ============================================================================
 * Broadcast API
 * ============================================================================ */

int thermo_bio_async_broadcast_temperature(
    thermo_bio_async_bridge_t* bridge,
    double temperature_k
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "thermo_bio_async_broadcast_temperature: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    thermo_bio_temperature_msg_t msg = {0};
    msg.header.type = 0x1320;  /* THERMO_BIO_MSG_TEMPERATURE */
    msg.header.source_module = 0x4501;  /* BIO_MODULE_PHYSICS_THERMO */
    msg.header.channel = bridge->config.default_channel;
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;
    msg.header.timestamp_us = get_timestamp_us();

    msg.temperature_k = temperature_k;
    msg.temperature_c = kelvin_to_celsius(temperature_k);
    msg.temperature_change_rate = (temperature_k - bridge->internal.last_temperature) /
                                  (bridge->config.broadcast_interval_ms / 1000.0);
    msg.landauer_limit = compute_landauer_limit(temperature_k);
    msg.module_id = bridge->state ? bridge->state->module_id : 0;
    msg.timestamp_us = msg.header.timestamp_us;

    bridge->internal.last_temperature = temperature_k;
    bridge->stats.temperature_broadcasts++;
    bridge->stats.broadcasts_sent++;
    bridge->last_broadcast_us = msg.timestamp_us;

    return 0;
}

int thermo_bio_async_broadcast_heat_flow(
    thermo_bio_async_bridge_t* bridge,
    double heat_dissipation,
    double heat_absorbed
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "thermo_bio_async_broadcast_heat_flow: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    thermo_bio_heat_flow_msg_t msg = {0};
    msg.header.type = 0x1321;
    msg.header.source_module = 0x4501;
    msg.header.channel = bridge->config.default_channel;
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;
    msg.header.timestamp_us = get_timestamp_us();

    msg.heat_dissipation_rate = heat_dissipation;
    msg.heat_absorbed = heat_absorbed;
    msg.net_heat_flow = heat_dissipation - heat_absorbed;
    msg.module_id = bridge->state ? bridge->state->module_id : 0;
    msg.timestamp_us = msg.header.timestamp_us;

    bridge->stats.broadcasts_sent++;
    return 0;
}

int thermo_bio_async_broadcast_entropy(thermo_bio_async_bridge_t* bridge) {
    if (!bridge || !bridge->connected || !bridge->state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "thermo_bio_async_broadcast_entropy: required parameter is NULL (bridge, bridge->connected, bridge->state)");
        return -1;
    }

    thermo_bio_entropy_msg_t msg = {0};
    msg.header.type = 0x1322;
    msg.header.source_module = 0x4501;
    msg.header.channel = bridge->config.default_channel;
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;
    msg.header.timestamp_us = get_timestamp_us();

    msg.entropy_production_rate = bridge->state->entropy_production_rate;
    msg.total_entropy_produced = bridge->state->total_entropy_produced;
    msg.free_energy_dissipated = bridge->state->free_energy_dissipated;
    msg.irreversible_component = bridge->state->entropy_production_rate * 0.7;
    msg.heat_component = bridge->state->entropy_production_rate * 0.3;
    msg.module_id = bridge->state->module_id;
    msg.timestamp_us = msg.header.timestamp_us;

    bridge->stats.entropy_broadcasts++;
    bridge->stats.broadcasts_sent++;
    return 0;
}

int thermo_bio_async_broadcast_atp_level(thermo_bio_async_bridge_t* bridge) {
    if (!bridge || !bridge->connected || !bridge->state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "thermo_bio_async_broadcast_atp_level: required parameter is NULL (bridge, bridge->connected, bridge->state)");
        return -1;
    }

    thermo_bio_atp_level_msg_t msg = {0};
    msg.header.type = 0x1323;
    msg.header.source_module = 0x4501;
    msg.header.channel = bridge->config.default_channel;
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;
    msg.header.timestamp_us = get_timestamp_us();

    msg.atp_available = bridge->state->atp_available;
    msg.atp_consumption_rate = bridge->state->atp_consumption_rate;
    msg.atp_regeneration_rate = 0;  /* Would come from metabolism model */
    msg.atp_fraction = bridge->state->atp_available / bridge->internal.initial_atp;

    /* Estimate time to depletion */
    if (msg.atp_consumption_rate > 0) {
        msg.time_to_depletion_s = msg.atp_available / msg.atp_consumption_rate;
    } else {
        msg.time_to_depletion_s = INFINITY;
    }

    msg.is_critical = (msg.atp_fraction <= bridge->config.atp_critical_threshold);
    msg.is_warning = (msg.atp_fraction <= bridge->config.atp_warning_threshold);
    msg.module_id = bridge->state->module_id;
    msg.timestamp_us = msg.header.timestamp_us;

    bridge->stats.atp_broadcasts++;
    bridge->stats.broadcasts_sent++;
    return 0;
}

int thermo_bio_async_send_atp_critical(
    thermo_bio_async_bridge_t* bridge,
    uint32_t severity
) {
    if (!bridge || !bridge->connected || !bridge->state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "thermo_bio_async_send_atp_critical: required parameter is NULL (bridge, bridge->connected, bridge->state)");
        return -1;
    }

    thermo_bio_atp_critical_msg_t msg = {0};
    msg.header.type = 0x1324;
    msg.header.source_module = 0x4501;
    msg.header.channel = bridge->config.alert_channel;
    msg.header.flags = BIO_MSG_FLAG_BROADCAST | BIO_MSG_FLAG_URGENT;
    msg.header.timestamp_us = get_timestamp_us();

    msg.atp_remaining = bridge->state->atp_available;
    msg.atp_fraction = bridge->state->atp_available / bridge->internal.initial_atp;
    msg.consumption_rate = bridge->state->atp_consumption_rate;

    if (msg.consumption_rate > 0) {
        msg.time_to_depletion_s = msg.atp_remaining / msg.consumption_rate;
    } else {
        msg.time_to_depletion_s = INFINITY;
    }

    msg.severity = severity;
    msg.module_id = bridge->state->module_id;
    msg.timestamp_us = msg.header.timestamp_us;

    bridge->stats.atp_alerts_sent++;
    bridge->stats.messages_sent++;
    return 0;
}

int thermo_bio_async_broadcast_energy_budget(
    thermo_bio_async_bridge_t* bridge,
    const nimcp_energy_budget_t* budget
) {
    if (!bridge || !bridge->connected || !budget) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "thermo_bio_async_broadcast_energy_budget: required parameter is NULL (bridge, bridge->connected, budget)");
        return -1;
    }

    thermo_bio_energy_budget_msg_t msg = {0};
    msg.header.type = 0x1325;
    msg.header.source_module = 0x4501;
    msg.header.channel = bridge->config.default_channel;
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;
    msg.header.timestamp_us = get_timestamp_us();

    msg.total_energy_consumed = budget->total;
    msg.power_consumption = bridge->state ? bridge->state->power_consumption : 0;
    msg.ion_pumping_energy = budget->ion_pumping;
    msg.synaptic_energy = budget->synaptic;
    msg.computation_energy = budget->computation;
    msg.housekeeping_energy = budget->housekeeping;
    msg.waste_heat = budget->waste_heat;
    msg.time_period_s = budget->time_period;
    msg.module_id = bridge->state ? bridge->state->module_id : 0;
    msg.timestamp_us = msg.header.timestamp_us;

    bridge->stats.broadcasts_sent++;
    return 0;
}

int thermo_bio_async_broadcast_landauer_cost(
    thermo_bio_async_bridge_t* bridge,
    const nimcp_landauer_cost_t* cost
) {
    if (!bridge || !bridge->connected || !cost) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "thermo_bio_async_broadcast_landauer_cost: required parameter is NULL (bridge, bridge->connected, cost)");
        return -1;
    }

    thermo_bio_landauer_cost_msg_t msg = {0};
    msg.header.type = 0x1326;
    msg.header.source_module = 0x4501;
    msg.header.channel = bridge->config.default_channel;
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;
    msg.header.timestamp_us = get_timestamp_us();

    msg.bits_erased = cost->bits_erased;
    msg.minimum_cost = cost->minimum_cost;
    msg.actual_cost = cost->actual_cost;
    msg.efficiency = cost->efficiency;
    msg.cost_per_bit = cost->bits_erased > 0 ?
                       cost->actual_cost / cost->bits_erased : 0;
    msg.landauer_limit_per_bit = compute_landauer_limit(cost->temperature_k);
    msg.module_id = bridge->state ? bridge->state->module_id : 0;
    msg.timestamp_us = msg.header.timestamp_us;

    bridge->stats.broadcasts_sent++;
    return 0;
}

int thermo_bio_async_broadcast_efficiency(thermo_bio_async_bridge_t* bridge) {
    if (!bridge || !bridge->connected || !bridge->state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "thermo_bio_async_broadcast_efficiency: required parameter is NULL (bridge, bridge->connected, bridge->state)");
        return -1;
    }

    thermo_bio_efficiency_msg_t msg = {0};
    msg.header.type = 0x1327;
    msg.header.source_module = 0x4501;
    msg.header.channel = bridge->config.default_channel;
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;
    msg.header.timestamp_us = get_timestamp_us();

    msg.computational_efficiency = bridge->state->computational_efficiency;
    msg.thermodynamic_efficiency = bridge->state->thermodynamic_efficiency;
    msg.landauer_efficiency = bridge->state->landauer_efficiency;
    msg.energy_per_bit = bridge->state->energy_per_bit;
    msg.total_energy = bridge->state->total_energy_consumed;
    msg.useful_work = bridge->state->total_energy_consumed *
                      bridge->state->computational_efficiency;
    msg.module_id = bridge->state->module_id;
    msg.timestamp_us = msg.header.timestamp_us;

    bridge->stats.efficiency_broadcasts++;
    bridge->stats.broadcasts_sent++;
    return 0;
}

/* ============================================================================
 * Subscription Management API
 * ============================================================================ */

int thermo_bio_async_subscribe_module(
    thermo_bio_async_bridge_t* bridge,
    uint32_t module_id,
    uint32_t msg_types
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "thermo_bio_async_subscribe_module: bridge is NULL");
        return -1;
    }

    thermo_bio_subscription_t* existing = find_subscription(bridge, module_id);
    if (existing) {
        existing->msg_type_mask |= msg_types;
        return 0;
    }

    if (bridge->subscription_count >= bridge->subscription_capacity) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "thermo_bio_async_subscribe_module: capacity exceeded");
        return -1;
    }

    thermo_bio_subscription_t* sub = &bridge->subscriptions[bridge->subscription_count++];
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

int thermo_bio_async_unsubscribe_module(
    thermo_bio_async_bridge_t* bridge,
    uint32_t module_id
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "thermo_bio_async_unsubscribe_module: bridge is NULL");
        return -1;
    }

    for (uint32_t i = 0; i < bridge->subscription_count; i++) {
        if (bridge->subscriptions[i].module_id == module_id) {
            bridge->subscriptions[i].active = false;
            bridge->stats.active_subscriptions--;
            return 0;
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "thermo_bio_async_unsubscribe_module: validation failed");
    return -1;
}

int thermo_bio_async_update_subscription(
    thermo_bio_async_bridge_t* bridge,
    uint32_t module_id,
    uint32_t msg_types
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "thermo_bio_async_update_subscription: bridge is NULL");
        return -1;
    }

    thermo_bio_subscription_t* sub = find_subscription(bridge, module_id);
    if (!sub) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "thermo_bio_async_update_subscription: sub is NULL");
        return -1;
    }

    sub->msg_type_mask = msg_types;
    return 0;
}

uint32_t thermo_bio_async_get_subscriber_count(
    const thermo_bio_async_bridge_t* bridge,
    thermo_bio_msg_type_t msg_type
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

int thermo_bio_async_get_stats(
    const thermo_bio_async_bridge_t* bridge,
    thermo_bio_async_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "thermo_bio_async_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }
    *stats = bridge->stats;
    return 0;
}

int thermo_bio_async_reset_stats(thermo_bio_async_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "thermo_bio_async_reset_stats: bridge is NULL");
        return -1;
    }
    memset(&bridge->stats, 0, sizeof(thermo_bio_async_stats_t));
    return 0;
}

static const char* thermo_msg_type_names[] = {
    "TEMPERATURE",
    "HEAT_FLOW",
    "ENTROPY",
    "ATP_LEVEL",
    "ATP_CRITICAL",
    "ENERGY_BUDGET",
    "LANDAUER_COST",
    "EFFICIENCY"
};

const char* thermo_bio_msg_type_name(thermo_bio_msg_type_t msg_type) {
    if (msg_type >= THERMO_BIO_MSG_COUNT) return "UNKNOWN";
    return thermo_msg_type_names[msg_type];
}

void thermo_bio_async_print_summary(const thermo_bio_async_bridge_t* bridge) {
    if (!bridge) {
        printf("Thermo Bio-Async Bridge: NULL\n");
        return;
    }

    printf("=== Thermo Bio-Async Bridge Summary ===\n");
    printf("Connected: %s\n", bridge->connected ? "Yes" : "No");
    printf("Subscriptions: %u/%u\n",
           bridge->stats.active_subscriptions, bridge->subscription_capacity);
    printf("Messages sent: %lu\n", (unsigned long)bridge->stats.messages_sent);
    printf("Broadcasts: %lu\n", (unsigned long)bridge->stats.broadcasts_sent);
    printf("  Temperature: %lu\n", (unsigned long)bridge->stats.temperature_broadcasts);
    printf("  ATP: %lu\n", (unsigned long)bridge->stats.atp_broadcasts);
    printf("  Entropy: %lu\n", (unsigned long)bridge->stats.entropy_broadcasts);
    printf("  Efficiency: %lu\n", (unsigned long)bridge->stats.efficiency_broadcasts);
    printf("ATP Alerts: %lu\n", (unsigned long)bridge->stats.atp_alerts_sent);
    printf("Errors: %lu\n", (unsigned long)bridge->stats.handler_errors);
    printf("========================================\n");
}
