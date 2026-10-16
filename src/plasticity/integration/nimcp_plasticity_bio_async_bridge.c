/**
 * @file nimcp_plasticity_bio_async_bridge.c
 * @brief Implementation of Plasticity Bio-Async Integration Bridge
 * @version 1.0.0
 * @date 2026-01-13
 */

#include "plasticity/integration/nimcp_plasticity_bio_async_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <time.h>

#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "security/nimcp_bbb_helpers.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(plasticity_bio_async_bridge)

/* Security integration */
#define LOG_MODULE "PLASTICITY_BIO_ASYNC_BRIDGE"


/* ============================================================================
 * Internal Bridge Structure
 * ============================================================================ */

struct plasticity_bio_async_bridge_struct {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    plasticity_bio_bridge_config_t config;
    plasticity_coordinator_t* coordinator;
    bio_router_t router;

    /* Registered plasticity modules */
    plasticity_bio_module_entry_t modules[PLASTICITY_BIO_MAX_MODULES];
    uint32_t module_count;

    /* Subscriptions */
    plasticity_bio_subscription_t* subscriptions;
    uint32_t subscription_count;
    uint32_t subscription_capacity;

    /* Connection state */
    bool connected;
    uint64_t last_broadcast_us;
    uint32_t time_since_state_broadcast_ms;
    uint32_t time_since_energy_report_ms;

    /* Batch processing */
    uint32_t current_batch_id;
    uint32_t batch_synapse_count;
    uint32_t batch_ltp_count;
    uint32_t batch_ltd_count;
    float batch_weight_sum;

    /* Statistics */
    plasticity_bio_async_stats_t stats;
};

BRIDGE_DEFINE_SECURITY_SETTERS(plasticity_bio_async_bridge)

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

static uint64_t get_timestamp_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

static plasticity_bio_subscription_t* find_subscription(
    plasticity_bio_async_bridge_t* b,
    uint32_t module_id
) {
    if (!b || !b->subscriptions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_subscription: required parameter is NULL (b, b->subscriptions)");
        return NULL;
    }

    for (uint32_t i = 0; i < b->subscription_count; i++) {
        if (b->subscriptions[i].module_id == module_id &&
            b->subscriptions[i].active) {
            return &b->subscriptions[i];
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_subscription: required parameter is NULL (b, b->subscriptions)");
    return NULL;
}

static plasticity_bio_module_entry_t* find_module(
    plasticity_bio_async_bridge_t* b,
    plasticity_module_type_t type
) {
    if (!b) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "b is NULL");

        return NULL;

    }

    for (uint32_t i = 0; i < b->module_count; i++) {
        if (b->modules[i].type == type) {
            return &b->modules[i];
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_module: validation failed");
    return NULL;
}

static void init_message_header(
    bio_message_header_t* header,
    uint32_t msg_type,
    nimcp_bio_channel_type_t channel,
    uint32_t flags
) {
    if (!header) return;

    header->type = msg_type;
    header->source_module = PLASTICITY_BIO_MODULE_ID;
    header->channel = channel;
    header->flags = flags;
    header->timestamp_us = get_timestamp_us();
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

int plasticity_bio_async_default_config(plasticity_bio_bridge_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "plasticity_bio_async_default_config: config is NULL");
        return PLASTICITY_BIO_ERROR_NULL_PARAM;
    }

    config->state_broadcast_interval_ms = PLASTICITY_BIO_DEFAULT_BROADCAST_INTERVAL_MS;
    config->energy_report_interval_ms = 1000;  /* 1 second */
    config->enable_auto_broadcast = true;
    config->enable_batch_mode = true;
    config->batch_size = 64;

    config->max_inbox_process_per_update = 64;
    config->message_ttl_ms = PLASTICITY_BIO_MESSAGE_TTL_MS;

    config->weight_change_threshold = PLASTICITY_BIO_WEIGHT_CHANGE_THRESHOLD;
    config->default_channel = BIO_CHANNEL_DOPAMINE;  /* Reward-associated */
    config->urgent_channel = BIO_CHANNEL_NOREPINEPHRINE;

    config->max_subscriptions = PLASTICITY_BIO_MAX_SUBSCRIPTIONS;
    config->max_registered_modules = PLASTICITY_BIO_MAX_MODULES;

    config->enable_weight_broadcasts = true;
    config->enable_consolidation_broadcasts = true;
    config->enable_ltp_ltd_broadcasts = true;
    config->enable_energy_tracking = true;
    config->enable_conflict_broadcasts = true;
    config->enable_logging = false;

    return PLASTICITY_BIO_OK;
}

plasticity_bio_async_bridge_t* plasticity_bio_async_bridge_create(
    const plasticity_bio_bridge_config_t* config
) {
    plasticity_bio_async_bridge_t* bridge = nimcp_calloc(
        1, sizeof(plasticity_bio_async_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "plasticity_bio_async_bridge_create: bridge allocation failed");
        return NULL;
    }

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        plasticity_bio_async_default_config(&bridge->config);
    }

    /* Allocate subscriptions */
    bridge->subscription_capacity = bridge->config.max_subscriptions;
    bridge->subscriptions = nimcp_calloc(
        bridge->subscription_capacity,
        sizeof(plasticity_bio_subscription_t));
    if (!bridge->subscriptions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "plasticity_bio_async_bridge_create: failed to allocate subscriptions");
        goto cleanup;
    }

    /* Create mutex */
    if (bridge_base_init(&bridge->base, 0, "plasticity_bio_async") != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "plasticity_bio_async_bridge_create: failed to initialize bridge base");
        goto cleanup;
    }
    if (!bridge->base.mutex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "plasticity_bio_async_bridge_create: failed to create mutex");
        goto cleanup;
    }

    bridge->last_broadcast_us = get_timestamp_us();
    return bridge;

cleanup:
    if (bridge->base.mutex) { bridge_base_cleanup(&bridge->base); }
    nimcp_free(bridge->subscriptions);
    nimcp_free(bridge);
    return NULL;
}

void plasticity_bio_async_bridge_destroy(plasticity_bio_async_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "plasticity_bio_async");

    if (bridge->connected) {
        plasticity_bio_async_disconnect(bridge);
    }

    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge->subscriptions);
    nimcp_free(bridge);
}

/* ============================================================================
 * Connection API
 * ============================================================================ */

int plasticity_bio_async_connect(
    plasticity_bio_async_bridge_t* bridge,
    plasticity_coordinator_t* coordinator,
    bio_router_t router
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "plasticity_bio_async_connect: bridge is NULL");
        return PLASTICITY_BIO_ERROR_NULL_PARAM;
    }
    if (bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "plasticity_bio_async_connect: already connected");
        return PLASTICITY_BIO_ERROR_ALREADY_CONNECTED;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->coordinator = coordinator;
    bridge->router = router;
    bridge->connected = true;
    bridge->last_broadcast_us = get_timestamp_us();

    nimcp_mutex_unlock(bridge->base.mutex);
    return PLASTICITY_BIO_OK;
}

int plasticity_bio_async_disconnect(plasticity_bio_async_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "plasticity_bio_async_disconnect: bridge is NULL");
        return PLASTICITY_BIO_ERROR_NULL_PARAM;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->coordinator = NULL;
    bridge->router = NULL;
    bridge->connected = false;

    nimcp_mutex_unlock(bridge->base.mutex);
    return PLASTICITY_BIO_OK;
}

bool plasticity_bio_async_is_connected(const plasticity_bio_async_bridge_t* bridge) {
    return bridge ? bridge->connected : false;
}

/* ============================================================================
 * Module Registration API
 * ============================================================================ */

int plasticity_bio_async_register_module(
    plasticity_bio_async_bridge_t* bridge,
    plasticity_module_type_t type,
    const char* name,
    void* handle
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "plasticity_bio_async_register_module: bridge is NULL");
        return PLASTICITY_BIO_ERROR_NULL_PARAM;
    }
    if (type >= PLASTICITY_MODULE_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "plasticity_bio_async_register_module: invalid module type");
        return PLASTICITY_BIO_ERROR_INVALID_TYPE;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Check if already registered */
    if (find_module(bridge, type)) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return PLASTICITY_BIO_ERROR_ALREADY_CONNECTED;
    }

    /* Check capacity */
    if (bridge->module_count >= bridge->config.max_registered_modules) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "plasticity_bio_async_register_module: max modules reached");
        nimcp_mutex_unlock(bridge->base.mutex);
        return PLASTICITY_BIO_ERROR_NO_MEMORY;
    }

    /* Add module */
    plasticity_bio_module_entry_t* entry = &bridge->modules[bridge->module_count++];
    entry->type = type;
    entry->name = name;
    entry->handle = handle;
    entry->enabled = true;
    entry->updates_received = 0;
    entry->last_update_time = get_timestamp_us();

    bridge->stats.registered_modules = bridge->module_count;

    nimcp_mutex_unlock(bridge->base.mutex);
    return PLASTICITY_BIO_OK;
}

int plasticity_bio_async_unregister_module(
    plasticity_bio_async_bridge_t* bridge,
    plasticity_module_type_t type
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "plasticity_bio_async_unregister_module: bridge is NULL");
        return PLASTICITY_BIO_ERROR_NULL_PARAM;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    for (uint32_t i = 0; i < bridge->module_count; i++) {
        if (bridge->modules[i].type == type) {
            /* Shift remaining modules down */
            for (uint32_t j = i; j < bridge->module_count - 1; j++) {
                bridge->modules[j] = bridge->modules[j + 1];
            }
            bridge->module_count--;
            bridge->stats.registered_modules = bridge->module_count;
            nimcp_mutex_unlock(bridge->base.mutex);
            return PLASTICITY_BIO_OK;
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return PLASTICITY_BIO_ERROR_NOT_FOUND;
}

int plasticity_bio_async_set_module_enabled(
    plasticity_bio_async_bridge_t* bridge,
    plasticity_module_type_t type,
    bool enabled
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "plasticity_bio_async_set_module_enabled: bridge is NULL");
        return PLASTICITY_BIO_ERROR_NULL_PARAM;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    plasticity_bio_module_entry_t* entry = find_module(bridge, type);
    if (!entry) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return PLASTICITY_BIO_ERROR_NOT_FOUND;
    }

    entry->enabled = enabled;
    nimcp_mutex_unlock(bridge->base.mutex);
    return PLASTICITY_BIO_OK;
}

/* ============================================================================
 * Message Processing API
 * ============================================================================ */

int plasticity_bio_async_process_inbox(
    plasticity_bio_async_bridge_t* bridge,
    uint32_t max_messages
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "plasticity_bio_async_process_inbox: bridge is NULL");
        return PLASTICITY_BIO_ERROR_NOT_CONNECTED;
    }
    if (!bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "plasticity_bio_async_process_inbox: not connected");
        return PLASTICITY_BIO_ERROR_NOT_CONNECTED;
    }

    /* Process incoming modulation requests, spike timing events */
    uint32_t processed = 0;
    (void)max_messages;

    bridge->stats.messages_received += processed;
    return (int)processed;
}

int plasticity_bio_async_update(
    plasticity_bio_async_bridge_t* bridge,
    uint32_t delta_ms
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "plasticity_bio_async_update: bridge is NULL");
        return PLASTICITY_BIO_ERROR_NOT_CONNECTED;
    }
    if (!bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "plasticity_bio_async_update: not connected");
        return PLASTICITY_BIO_ERROR_NOT_CONNECTED;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->time_since_state_broadcast_ms += delta_ms;
    bridge->time_since_energy_report_ms += delta_ms;

    /* Auto-broadcast state if enabled and interval elapsed */
    if (bridge->config.enable_auto_broadcast &&
        bridge->time_since_state_broadcast_ms >= bridge->config.state_broadcast_interval_ms) {

        if (bridge->coordinator) {
            plasticity_coordinator_state_t state =
                plasticity_coordinator_get_state(bridge->coordinator);
            plasticity_bio_async_broadcast_state_change(bridge, state, state);
        }
        bridge->time_since_state_broadcast_ms = 0;
    }

    /* Energy report if enabled and interval elapsed */
    if (bridge->config.enable_energy_tracking &&
        bridge->time_since_energy_report_ms >= bridge->config.energy_report_interval_ms) {

        if (bridge->coordinator) {
            float rate = plasticity_coordinator_get_energy_rate(bridge->coordinator);
            plasticity_bio_async_broadcast_energy_report(bridge, 0, rate, 0);
        }
        bridge->time_since_energy_report_ms = 0;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return PLASTICITY_BIO_OK;
}

/* ============================================================================
 * Learning Coordination API
 * ============================================================================ */

int plasticity_bio_async_broadcast_weight_update(
    plasticity_bio_async_bridge_t* bridge,
    uint32_t synapse_id,
    float old_weight,
    float new_weight,
    plasticity_module_type_t source_module
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "plasticity_bio_async_broadcast_weight_update: bridge is NULL");
        return PLASTICITY_BIO_ERROR_NOT_CONNECTED;
    }
    if (!bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "plasticity_bio_async_broadcast_weight_update: not connected");
        return PLASTICITY_BIO_ERROR_NOT_CONNECTED;
    }

    float change = new_weight - old_weight;
    if (fabsf(change) < bridge->config.weight_change_threshold) {
        return PLASTICITY_BIO_OK;  /* Below threshold, skip broadcast */
    }

    nimcp_mutex_lock(bridge->base.mutex);

    plasticity_bio_weight_update_msg_t msg = {0};
    init_message_header(&msg.header, 0x2100,
                       bridge->config.default_channel,
                       BIO_MSG_FLAG_BROADCAST);

    msg.synapse_id = synapse_id;
    msg.old_weight = old_weight;
    msg.new_weight = new_weight;
    msg.weight_change = change;
    msg.source_module = source_module;
    msg.is_potentiation = (change > 0);
    msg.timestamp_us = msg.header.timestamp_us;

    bridge->stats.weight_update_broadcasts++;
    bridge->stats.broadcasts_sent++;

    /* Update batch tracking */
    if (bridge->config.enable_batch_mode) {
        bridge->batch_synapse_count++;
        bridge->batch_weight_sum += change;
        if (change > 0) bridge->batch_ltp_count++;
        else bridge->batch_ltd_count++;
    }

    /* Update per-module stats */
    switch (source_module) {
        case PLASTICITY_MODULE_STDP:
            bridge->stats.stdp_updates++;
            break;
        case PLASTICITY_MODULE_BCM:
            bridge->stats.bcm_updates++;
            break;
        case PLASTICITY_MODULE_HOMEOSTATIC:
            bridge->stats.homeostatic_updates++;
            break;
        case PLASTICITY_MODULE_ELIGIBILITY:
            bridge->stats.eligibility_updates++;
            break;
        default:
            break;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return PLASTICITY_BIO_OK;
}

int plasticity_bio_async_broadcast_consolidation(
    plasticity_bio_async_bridge_t* bridge,
    uint32_t num_synapses,
    float mean_change,
    bool triggered_by_sleep
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "plasticity_bio_async_broadcast_consolidation: bridge is NULL");
        return PLASTICITY_BIO_ERROR_NOT_CONNECTED;
    }
    if (!bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "plasticity_bio_async_broadcast_consolidation: not connected");
        return PLASTICITY_BIO_ERROR_NOT_CONNECTED;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    plasticity_bio_consolidation_msg_t msg = {0};
    init_message_header(&msg.header, 0x2101,
                       bridge->config.urgent_channel,
                       BIO_MSG_FLAG_BROADCAST | BIO_MSG_FLAG_URGENT);

    msg.num_synapses_consolidated = num_synapses;
    msg.mean_weight_change = mean_change;
    msg.triggered_by_sleep = triggered_by_sleep;
    msg.timestamp_us = msg.header.timestamp_us;

    bridge->stats.consolidation_broadcasts++;
    bridge->stats.broadcasts_sent++;

    nimcp_mutex_unlock(bridge->base.mutex);
    return PLASTICITY_BIO_OK;
}

int plasticity_bio_async_broadcast_ltp(
    plasticity_bio_async_bridge_t* bridge,
    uint32_t synapse_id,
    float magnitude,
    float delta_t,
    plasticity_module_type_t mechanism
) {
    if (!bridge || !bridge->connected) return PLASTICITY_BIO_ERROR_NOT_CONNECTED;

    nimcp_mutex_lock(bridge->base.mutex);

    plasticity_bio_ltp_ltd_msg_t msg = {0};
    init_message_header(&msg.header, 0x2104,
                       bridge->config.default_channel,
                       BIO_MSG_FLAG_BROADCAST);

    msg.synapse_id = synapse_id;
    msg.magnitude = magnitude;
    msg.delta_t = delta_t;
    msg.mechanism = mechanism;
    msg.timestamp_us = msg.header.timestamp_us;

    bridge->stats.ltp_events++;
    bridge->stats.broadcasts_sent++;

    nimcp_mutex_unlock(bridge->base.mutex);
    return PLASTICITY_BIO_OK;
}

int plasticity_bio_async_broadcast_ltd(
    plasticity_bio_async_bridge_t* bridge,
    uint32_t synapse_id,
    float magnitude,
    float delta_t,
    plasticity_module_type_t mechanism
) {
    if (!bridge || !bridge->connected) return PLASTICITY_BIO_ERROR_NOT_CONNECTED;

    nimcp_mutex_lock(bridge->base.mutex);

    plasticity_bio_ltp_ltd_msg_t msg = {0};
    init_message_header(&msg.header, 0x2105,
                       bridge->config.default_channel,
                       BIO_MSG_FLAG_BROADCAST);

    msg.synapse_id = synapse_id;
    msg.magnitude = magnitude;
    msg.delta_t = delta_t;
    msg.mechanism = mechanism;
    msg.timestamp_us = msg.header.timestamp_us;

    bridge->stats.ltd_events++;
    bridge->stats.broadcasts_sent++;

    nimcp_mutex_unlock(bridge->base.mutex);
    return PLASTICITY_BIO_OK;
}

int plasticity_bio_async_broadcast_scaling(
    plasticity_bio_async_bridge_t* bridge,
    uint32_t neuron_id,
    float scaling_factor,
    float target_rate,
    float actual_rate
) {
    if (!bridge || !bridge->connected) return PLASTICITY_BIO_ERROR_NOT_CONNECTED;

    nimcp_mutex_lock(bridge->base.mutex);

    plasticity_bio_scaling_msg_t msg = {0};
    init_message_header(&msg.header, 0x2103,
                       bridge->config.default_channel,
                       BIO_MSG_FLAG_BROADCAST);

    msg.neuron_id = neuron_id;
    msg.scaling_factor = scaling_factor;
    msg.target_rate = target_rate;
    msg.actual_rate = actual_rate;
    msg.rate_deviation = actual_rate - target_rate;
    msg.upscaling = (scaling_factor > 1.0f);
    msg.timestamp_us = msg.header.timestamp_us;

    bridge->stats.scaling_events++;
    bridge->stats.broadcasts_sent++;

    nimcp_mutex_unlock(bridge->base.mutex);
    return PLASTICITY_BIO_OK;
}

int plasticity_bio_async_broadcast_eligibility_convert(
    plasticity_bio_async_bridge_t* bridge,
    uint32_t synapse_id,
    float trace_value,
    float weight_change,
    float reward_signal
) {
    if (!bridge || !bridge->connected) return PLASTICITY_BIO_ERROR_NOT_CONNECTED;

    nimcp_mutex_lock(bridge->base.mutex);

    plasticity_bio_eligibility_msg_t msg = {0};
    init_message_header(&msg.header, 0x2102,
                       bridge->config.default_channel,
                       BIO_MSG_FLAG_BROADCAST);

    msg.synapse_id = synapse_id;
    msg.eligibility_trace = trace_value;
    msg.weight_change = weight_change;
    msg.reward_signal = reward_signal;
    msg.dopamine_gated = (reward_signal != 0);
    msg.timestamp_us = msg.header.timestamp_us;

    bridge->stats.broadcasts_sent++;

    nimcp_mutex_unlock(bridge->base.mutex);
    return PLASTICITY_BIO_OK;
}

int plasticity_bio_async_broadcast_state_change(
    plasticity_bio_async_bridge_t* bridge,
    plasticity_coordinator_state_t old_state,
    plasticity_coordinator_state_t new_state
) {
    if (!bridge || !bridge->connected) return PLASTICITY_BIO_ERROR_NOT_CONNECTED;

    nimcp_mutex_lock(bridge->base.mutex);

    plasticity_bio_state_msg_t msg = {0};
    init_message_header(&msg.header, 0x2106,
                       bridge->config.default_channel,
                       BIO_MSG_FLAG_BROADCAST);

    msg.old_state = old_state;
    msg.new_state = new_state;
    msg.active_mechanisms = bridge->module_count;
    msg.timestamp_us = msg.header.timestamp_us;

    bridge->stats.broadcasts_sent++;
    bridge->last_broadcast_us = msg.timestamp_us;

    nimcp_mutex_unlock(bridge->base.mutex);
    return PLASTICITY_BIO_OK;
}

int plasticity_bio_async_broadcast_energy_report(
    plasticity_bio_async_bridge_t* bridge,
    float total_energy,
    float energy_rate,
    float budget_utilization
) {
    if (!bridge || !bridge->connected) return PLASTICITY_BIO_ERROR_NOT_CONNECTED;

    nimcp_mutex_lock(bridge->base.mutex);

    plasticity_bio_energy_msg_t msg = {0};
    init_message_header(&msg.header, 0x2107,
                       bridge->config.default_channel,
                       BIO_MSG_FLAG_BROADCAST);

    msg.total_energy_consumed = total_energy;
    msg.energy_rate = energy_rate;
    msg.budget_utilization = budget_utilization;
    msg.timestamp_us = msg.header.timestamp_us;

    bridge->stats.energy_reports_sent++;
    bridge->stats.total_energy_reported += total_energy;
    bridge->stats.broadcasts_sent++;

    nimcp_mutex_unlock(bridge->base.mutex);
    return PLASTICITY_BIO_OK;
}

int plasticity_bio_async_broadcast_conflict(
    plasticity_bio_async_bridge_t* bridge,
    uint32_t synapse_id,
    plasticity_module_type_t mech_a,
    plasticity_module_type_t mech_b,
    float change_a,
    float change_b,
    float resolved
) {
    if (!bridge || !bridge->connected) return PLASTICITY_BIO_ERROR_NOT_CONNECTED;

    nimcp_mutex_lock(bridge->base.mutex);

    plasticity_bio_conflict_msg_t msg = {0};
    init_message_header(&msg.header, 0x2108,
                       bridge->config.default_channel,
                       BIO_MSG_FLAG_BROADCAST);

    msg.synapse_id = synapse_id;
    msg.mechanism_a = mech_a;
    msg.mechanism_b = mech_b;
    msg.weight_change_a = change_a;
    msg.weight_change_b = change_b;
    msg.resolved_change = resolved;
    msg.timestamp_us = msg.header.timestamp_us;

    bridge->stats.conflict_resolutions++;
    bridge->stats.broadcasts_sent++;

    nimcp_mutex_unlock(bridge->base.mutex);
    return PLASTICITY_BIO_OK;
}

int plasticity_bio_async_notify_spike_timing(
    plasticity_bio_async_bridge_t* bridge,
    uint32_t neuron_id,
    bool is_presynaptic,
    float spike_time_ms
) {
    if (!bridge || !bridge->connected) return PLASTICITY_BIO_ERROR_NOT_CONNECTED;

    nimcp_mutex_lock(bridge->base.mutex);

    plasticity_bio_spike_timing_msg_t msg = {0};
    init_message_header(&msg.header, 0x2109,
                       bridge->config.default_channel,
                       BIO_MSG_FLAG_BROADCAST);

    msg.neuron_id = neuron_id;
    msg.is_presynaptic = is_presynaptic;
    msg.spike_time_ms = spike_time_ms;
    msg.timestamp_us = msg.header.timestamp_us;

    bridge->stats.messages_sent++;

    nimcp_mutex_unlock(bridge->base.mutex);
    return PLASTICITY_BIO_OK;
}

int plasticity_bio_async_request_rate_modulation(
    plasticity_bio_async_bridge_t* bridge,
    float modulation_factor,
    plasticity_module_type_t target_module,
    float dopamine,
    float norepinephrine,
    float acetylcholine
) {
    if (!bridge || !bridge->connected) return PLASTICITY_BIO_ERROR_NOT_CONNECTED;

    nimcp_mutex_lock(bridge->base.mutex);

    plasticity_bio_rate_mod_msg_t msg = {0};
    init_message_header(&msg.header, 0x210A,
                       bridge->config.default_channel,
                       BIO_MSG_FLAG_BROADCAST);

    msg.modulation_factor = modulation_factor;
    msg.target_module = target_module;
    msg.dopamine_level = dopamine;
    msg.norepinephrine_level = norepinephrine;
    msg.acetylcholine_level = acetylcholine;
    msg.timestamp_us = msg.header.timestamp_us;

    bridge->stats.messages_sent++;

    nimcp_mutex_unlock(bridge->base.mutex);
    return PLASTICITY_BIO_OK;
}

int plasticity_bio_async_complete_batch(
    plasticity_bio_async_bridge_t* bridge,
    uint32_t batch_id,
    uint32_t synapses_updated,
    uint32_t ltp_count,
    uint32_t ltd_count,
    float mean_change
) {
    if (!bridge || !bridge->connected) return PLASTICITY_BIO_ERROR_NOT_CONNECTED;

    nimcp_mutex_lock(bridge->base.mutex);

    plasticity_bio_batch_msg_t msg = {0};
    init_message_header(&msg.header, 0x210F,
                       bridge->config.default_channel,
                       BIO_MSG_FLAG_BROADCAST);

    msg.batch_id = batch_id;
    msg.synapses_updated = synapses_updated;
    msg.ltp_count = ltp_count;
    msg.ltd_count = ltd_count;
    msg.mean_weight_change = mean_change;
    msg.timestamp_us = msg.header.timestamp_us;

    bridge->stats.batch_completions++;
    bridge->stats.broadcasts_sent++;

    /* Reset batch tracking */
    bridge->current_batch_id++;
    bridge->batch_synapse_count = 0;
    bridge->batch_ltp_count = 0;
    bridge->batch_ltd_count = 0;
    bridge->batch_weight_sum = 0;

    nimcp_mutex_unlock(bridge->base.mutex);
    return PLASTICITY_BIO_OK;
}

/* ============================================================================
 * Subscription Management API
 * ============================================================================ */

int plasticity_bio_async_subscribe_module(
    plasticity_bio_async_bridge_t* bridge,
    uint32_t module_id,
    uint32_t msg_types
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "plasticity_bio_async_subscribe_module: bridge is NULL");
        return PLASTICITY_BIO_ERROR_NULL_PARAM;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Check if already subscribed */
    plasticity_bio_subscription_t* existing = find_subscription(bridge, module_id);
    if (existing) {
        existing->msg_type_mask |= msg_types;
        nimcp_mutex_unlock(bridge->base.mutex);
        return PLASTICITY_BIO_OK;
    }

    /* Check capacity */
    if (bridge->subscription_count >= bridge->subscription_capacity) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "plasticity_bio_async_subscribe_module: subscription capacity exceeded");
        nimcp_mutex_unlock(bridge->base.mutex);
        return PLASTICITY_BIO_ERROR_SUBSCRIPTION_FULL;
    }

    /* Add subscription */
    plasticity_bio_subscription_t* sub =
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

    nimcp_mutex_unlock(bridge->base.mutex);
    return PLASTICITY_BIO_OK;
}

int plasticity_bio_async_unsubscribe_module(
    plasticity_bio_async_bridge_t* bridge,
    uint32_t module_id
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "plasticity_bio_async_unsubscribe_module: bridge is NULL");
        return PLASTICITY_BIO_ERROR_NULL_PARAM;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    for (uint32_t i = 0; i < bridge->subscription_count; i++) {
        if (bridge->subscriptions[i].module_id == module_id) {
            bridge->subscriptions[i].active = false;
            bridge->stats.active_subscriptions--;
            nimcp_mutex_unlock(bridge->base.mutex);
            return PLASTICITY_BIO_OK;
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return PLASTICITY_BIO_ERROR_NOT_FOUND;
}

int plasticity_bio_async_update_subscription(
    plasticity_bio_async_bridge_t* bridge,
    uint32_t module_id,
    uint32_t msg_types
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "plasticity_bio_async_update_subscription: bridge is NULL");
        return PLASTICITY_BIO_ERROR_NULL_PARAM;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    plasticity_bio_subscription_t* sub = find_subscription(bridge, module_id);
    if (!sub) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return PLASTICITY_BIO_ERROR_NOT_FOUND;
    }

    sub->msg_type_mask = msg_types;
    nimcp_mutex_unlock(bridge->base.mutex);
    return PLASTICITY_BIO_OK;
}

uint32_t plasticity_bio_async_get_subscriber_count(
    const plasticity_bio_async_bridge_t* bridge,
    plasticity_bio_msg_type_t msg_type
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

int plasticity_bio_async_get_stats(
    const plasticity_bio_async_bridge_t* bridge,
    plasticity_bio_async_stats_t* stats
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "plasticity_bio_async_get_stats: bridge is NULL");
        return PLASTICITY_BIO_ERROR_NULL_PARAM;
    }
    if (!stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "plasticity_bio_async_get_stats: stats is NULL");
        return PLASTICITY_BIO_ERROR_NULL_PARAM;
    }
    *stats = bridge->stats;
    return PLASTICITY_BIO_OK;
}

int plasticity_bio_async_reset_stats(plasticity_bio_async_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "plasticity_bio_async_reset_stats: bridge is NULL");
        return PLASTICITY_BIO_ERROR_NULL_PARAM;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(plasticity_bio_async_stats_t));
    bridge->stats.registered_modules = bridge->module_count;
    bridge->stats.active_subscriptions = bridge->subscription_count;
    nimcp_mutex_unlock(bridge->base.mutex);

    return PLASTICITY_BIO_OK;
}

static const char* plasticity_msg_type_names[] = {
    "WEIGHT_UPDATE",
    "CONSOLIDATION",
    "ELIGIBILITY_CONVERT",
    "SCALING_EVENT",
    "LTP_EVENT",
    "LTD_EVENT",
    "STATE_UPDATE",
    "ENERGY_REPORT",
    "CONFLICT_RESOLVED",
    "SPIKE_TIMING",
    "RATE_MODULATION",
    "THRESHOLD_UPDATE",
    "DENDRITIC_EVENT",
    "METAPLASTICITY",
    "STRUCTURAL_CHANGE",
    "BATCH_COMPLETE"
};

const char* plasticity_bio_msg_type_name(plasticity_bio_msg_type_t msg_type) {
    if (msg_type >= PLASTICITY_MSG_COUNT) return "UNKNOWN";
    return plasticity_msg_type_names[msg_type];
}

static const char* plasticity_module_type_names[] = {
    "STDP",
    "BCM",
    "HOMEOSTATIC",
    "ELIGIBILITY",
    "DENDRITIC",
    "STP",
    "ADAPTIVE",
    "PREDICTIVE",
    "METAPLASTICITY",
    "STRUCTURAL",
    "HETEROSYNAPTIC",
    "CALCIUM",
    "PROTEIN"
};

const char* plasticity_bio_module_type_name(plasticity_module_type_t module_type) {
    if (module_type >= PLASTICITY_MODULE_COUNT) return "UNKNOWN";
    return plasticity_module_type_names[module_type];
}

void plasticity_bio_async_print_summary(const plasticity_bio_async_bridge_t* bridge) {
    if (!bridge) {
        printf("Plasticity Bio-Async Bridge: NULL\n");
        return;
    }

    printf("=== Plasticity Bio-Async Bridge Summary ===\n");
    printf("Connected: %s\n", bridge->connected ? "Yes" : "No");
    printf("Registered Modules: %u\n", bridge->module_count);
    printf("Subscriptions: %u/%u\n",
           bridge->stats.active_subscriptions, bridge->subscription_capacity);
    printf("\n");

    printf("Messages:\n");
    printf("  Sent: %lu\n", (unsigned long)bridge->stats.messages_sent);
    printf("  Received: %lu\n", (unsigned long)bridge->stats.messages_received);
    printf("  Broadcasts: %lu\n", (unsigned long)bridge->stats.broadcasts_sent);
    printf("  Dropped: %lu\n", (unsigned long)bridge->stats.messages_dropped);
    printf("\n");

    printf("Plasticity Events:\n");
    printf("  Weight Updates: %lu\n",
           (unsigned long)bridge->stats.weight_update_broadcasts);
    printf("  LTP Events: %lu\n", (unsigned long)bridge->stats.ltp_events);
    printf("  LTD Events: %lu\n", (unsigned long)bridge->stats.ltd_events);
    printf("  Scaling Events: %lu\n", (unsigned long)bridge->stats.scaling_events);
    printf("  Consolidations: %lu\n",
           (unsigned long)bridge->stats.consolidation_broadcasts);
    printf("  Conflict Resolutions: %lu\n",
           (unsigned long)bridge->stats.conflict_resolutions);
    printf("  Batch Completions: %lu\n",
           (unsigned long)bridge->stats.batch_completions);
    printf("\n");

    printf("Per-Module Updates:\n");
    printf("  STDP: %lu\n", (unsigned long)bridge->stats.stdp_updates);
    printf("  BCM: %lu\n", (unsigned long)bridge->stats.bcm_updates);
    printf("  Homeostatic: %lu\n", (unsigned long)bridge->stats.homeostatic_updates);
    printf("  Eligibility: %lu\n", (unsigned long)bridge->stats.eligibility_updates);
    printf("\n");

    printf("Energy Tracking:\n");
    printf("  Reports Sent: %u\n", bridge->stats.energy_reports_sent);
    printf("  Total Reported: %.2f\n", bridge->stats.total_energy_reported);
    printf("\n");

    printf("Errors: %lu\n", (unsigned long)bridge->stats.handler_errors);
    printf("===========================================\n");
}
