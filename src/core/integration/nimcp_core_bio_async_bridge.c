/**
 * @file nimcp_core_bio_async_bridge.c
 * @brief Core Module Bio-Async Integration Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-13
 *
 * WHAT: Implementation of bio-async integration for core NIMCP modules
 * WHY:  Enable inter-module communication for brain, medulla, neurons, etc.
 * HOW:  Uses bio-router for message passing, provides spike routing engine
 *
 * @author NIMCP Development Team
 */

#include "core/integration/nimcp_core_bio_async_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/time/nimcp_time.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdio.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(core_bio_async_bridge)

/* ============================================================================
 * Internal Structures
 * ============================================================================ */

/**
 * @brief Pending spike entry for deferred routing
 */
typedef struct {
    uint32_t neuron_id;                 /**< Source neuron ID */
    float amplitude;                    /**< Spike amplitude */
    uint64_t timestamp_us;              /**< Spike timestamp */
    bool is_inhibitory;                 /**< Inhibitory neuron flag */
} pending_spike_t;

/**
 * @brief Core bio-async bridge internal structure
 */
struct core_bio_async_bridge_struct {
    /* Base bridge infrastructure */
    bridge_base_t base;

    /* Configuration */
    core_bio_async_config_t config;

    /* Module registry */
    core_module_entry_t modules[CORE_BIO_MAX_MODULES];
    uint32_t module_count;

    /* Spike routing queue */
    pending_spike_t* pending_spikes;
    uint32_t pending_spike_count;
    uint32_t pending_spike_capacity;

    /* State tracking */
    uint64_t last_state_broadcast_us;
    bool connected;

    /* Statistics */
    core_bio_async_stats_t stats;

    /* Health tracking */
    float module_health[CORE_MODULE_TYPE_COUNT];
    uint64_t module_last_seen[CORE_MODULE_TYPE_COUNT];
};

/* ============================================================================
 * Forward Declarations (Internal handlers)
 * ============================================================================ */

static nimcp_error_t core_bio_handle_brain_state_query(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data);

static nimcp_error_t core_bio_handle_spike_route(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data);

static nimcp_error_t core_bio_handle_health_check(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data);

/* ============================================================================
 * Message Type Names
 * ============================================================================ */

static const char* g_core_msg_type_names[] = {
    "BRAIN_STATE",
    "BRAIN_STATE_QUERY",
    "BRAIN_CONFIG_UPDATE",
    "BRAIN_REGION_ACTIVATED",
    "NEURON_SPIKE",
    "NEURON_SPIKE_BATCH",
    "SPIKE_ROUTE_REQUEST",
    "SPIKE_ROUTE_COMPLETE",
    "MEDULLA_STATE",
    "MEDULLA_AROUSAL",
    "MEDULLA_PROTECTION",
    "MEDULLA_CIRCADIAN",
    "NEURON_TYPE_REGISTER",
    "NEURON_TYPE_QUERY",
    "NEURON_TYPE_UPDATE",
    "TOPOLOGY_UPDATE",
    "TOPOLOGY_QUERY",
    "CONNECTION_ADDED",
    "CONNECTION_REMOVED",
    "LOGIC_GATE_UPDATE",
    "LOGIC_CIRCUIT_STEP",
    "SYNC_REQUEST",
    "SYNC_COMPLETE",
    "HEALTH_CHECK",
    "HEALTH_RESPONSE"
};

static const char* g_core_module_type_names[] = {
    "brain",
    "medulla",
    "neuron_types",
    "topology",
    "neural_logic",
    "neuralnet",
    "axon",
    "synapse",
    "cortical_column"
};

/* ============================================================================
 * Configuration API
 * ============================================================================ */

int core_bio_async_default_config(core_bio_async_config_t* config)
{
    /* Guard clause */
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;
    }

    /* Spike routing defaults */
    config->max_pending_spikes = CORE_BIO_MAX_PENDING_SPIKES;
    config->spike_batch_size = CORE_BIO_DEFAULT_SPIKE_BATCH;
    config->enable_priority_routing = true;
    config->priority_threshold = CORE_BIO_INHIBITORY_PRIORITY_THRESHOLD;

    /* State broadcast defaults */
    config->state_broadcast_interval_ms = CORE_BIO_STATE_BROADCAST_INTERVAL_MS;
    config->enable_auto_broadcast = true;

    /* Message handling defaults */
    config->max_inbox_process = 32;
    config->message_ttl_ms = CORE_BIO_MESSAGE_TTL_MS;

    /* Channel configuration - biologically-appropriate channels */
    config->spike_channel = BIO_CHANNEL_ACETYLCHOLINE;  /* Fast, attention-based */
    config->state_channel = BIO_CHANNEL_SEROTONIN;      /* Slow, state coordination */
    config->urgent_channel = BIO_CHANNEL_NOREPINEPHRINE; /* Priority alerting */

    /* Feature flags */
    config->enable_spike_routing = true;
    config->enable_topology_sync = true;
    config->enable_health_monitoring = true;
    config->enable_logging = false;

    return 0;
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

core_bio_async_bridge_t* core_bio_async_bridge_create(
    const core_bio_async_config_t* config)
{
    /* Allocate bridge structure */
    core_bio_async_bridge_t* bridge = nimcp_malloc(sizeof(core_bio_async_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate core bio-async bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "core_bio_async_bridge_create: bridge is NULL");
        return NULL;
    }
    memset(bridge, 0, sizeof(core_bio_async_bridge_t));

    /* Initialize base bridge */
    if (bridge_base_init(&bridge->base, BIO_MODULE_BRAIN, "core_bio_async") != 0) {
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "core_bio_async_bridge_create: validation failed");
        return NULL;
    }

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        core_bio_async_default_config(&bridge->config);
    }

    /* Allocate spike queue */
    bridge->pending_spike_capacity = bridge->config.max_pending_spikes;
    bridge->pending_spikes = nimcp_malloc(
        sizeof(pending_spike_t) * bridge->pending_spike_capacity);
    if (!bridge->pending_spikes) {
        NIMCP_LOGGING_ERROR("Failed to allocate spike queue");
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "core_bio_async_bridge_create: bridge->pending_spikes is NULL");
        return NULL;
    }
    bridge->pending_spike_count = 0;

    /* Initialize health tracking */
    for (uint32_t i = 0; i < CORE_MODULE_TYPE_COUNT; i++) {
        bridge->module_health[i] = 1.0f;
        bridge->module_last_seen[i] = 0;
    }

    NIMCP_LOGGING_INFO("Created core bio-async bridge");
    return bridge;
}

void core_bio_async_bridge_destroy(core_bio_async_bridge_t* bridge)
{
    if (!bridge) {
        return;
    }

    /* Disconnect first */
    if (bridge->connected) {
        core_bio_async_disconnect(bridge);
    }

    /* Free spike queue */
    if (bridge->pending_spikes) {
        nimcp_free(bridge->pending_spikes);
        bridge->pending_spikes = NULL;
    }

    /* Cleanup base */
    bridge_base_cleanup(&bridge->base);

    /* Free structure */
    nimcp_free(bridge);
    NIMCP_LOGGING_INFO("Destroyed core bio-async bridge");
}

/* ============================================================================
 * Connection API
 * ============================================================================ */

int core_bio_async_connect(
    core_bio_async_bridge_t* bridge,
    bio_router_t router)
{
    /* Guard clauses */
    if (!bridge) {
        return CORE_BIO_ERROR_NOT_INITIALIZED;
    }
    if (bridge->connected) {
        return 0;  /* Already connected */
    }

    /* Use global router if not specified */
    bio_router_t actual_router = router ? router : bio_router_get_global();
    if (!actual_router && !bio_router_is_initialized()) {
        NIMCP_LOGGING_WARN("Bio-router not available, skipping connection");
        return CORE_BIO_ERROR_ROUTER_NOT_AVAILABLE;
    }

    /* Connect base bridge to bio-async */
    if (bridge_base_connect_bio_async(&bridge->base) != 0) {
        NIMCP_LOGGING_ERROR("Failed to connect base bridge to bio-async");
        return CORE_BIO_ERROR_ROUTER_NOT_AVAILABLE;
    }

    /* Register message handlers */
    if (bridge->base.bio_ctx) {
        bio_router_register_handler(
            bridge->base.bio_ctx,
            BIO_MSG_BRAIN_STATE_QUERY,
            core_bio_handle_brain_state_query);

        bio_router_register_handler(
            bridge->base.bio_ctx,
            BIO_MSG_SIGNAL_ROUTE_REQUEST,
            core_bio_handle_spike_route);

        bio_router_register_handler(
            bridge->base.bio_ctx,
            BIO_MSG_HEALTH_CHECK,
            core_bio_handle_health_check);
    }

    bridge->connected = true;
    bridge->last_state_broadcast_us = nimcp_time_get_us();

    NIMCP_LOGGING_INFO("Core bio-async bridge connected to router");
    return 0;
}

int core_bio_async_disconnect(core_bio_async_bridge_t* bridge)
{
    /* Guard clause */
    if (!bridge) {
        return CORE_BIO_ERROR_NOT_INITIALIZED;
    }
    if (!bridge->connected) {
        return 0;  /* Already disconnected */
    }

    /* Unregister all modules */
    for (uint32_t i = 0; i < CORE_MODULE_TYPE_COUNT; i++) {
        if (bridge->modules[i].active) {
            core_bio_async_unregister_module(bridge, (core_module_type_t)i);
        }
    }

    /* Disconnect base bridge */
    bridge_base_disconnect_bio_async(&bridge->base);

    bridge->connected = false;
    NIMCP_LOGGING_INFO("Core bio-async bridge disconnected");
    return 0;
}

bool core_bio_async_is_connected(const core_bio_async_bridge_t* bridge)
{
    return bridge && bridge->connected;
}

/* ============================================================================
 * Module Registration API
 * ============================================================================ */

int core_bio_async_register_module(
    core_bio_async_bridge_t* bridge,
    core_module_type_t type,
    const char* name,
    void* module_ptr)
{
    /* Guard clauses */
    if (!bridge) {
        return CORE_BIO_ERROR_NOT_INITIALIZED;
    }
    if (type >= CORE_MODULE_TYPE_COUNT) {
        return CORE_BIO_ERROR_INVALID_MODULE_TYPE;
    }
    if (bridge->modules[type].active) {
        return CORE_BIO_ERROR_MODULE_ALREADY_REGISTERED;
    }
    if (bridge->module_count >= CORE_BIO_MAX_MODULES) {
        return CORE_BIO_ERROR_MAX_MODULES_EXCEEDED;
    }

    /* Lock for thread safety */
    BRIDGE_LOCK(bridge);

    /* Set up module entry */
    core_module_entry_t* entry = &bridge->modules[type];
    entry->type = type;
    entry->name = name ? name : g_core_module_type_names[type];
    entry->module_ptr = module_ptr;
    entry->active = true;
    entry->registration_time = nimcp_time_get_us();
    entry->messages_sent = 0;
    entry->messages_received = 0;

    /* Assign bio-module ID based on type */
    switch (type) {
        case CORE_MODULE_BRAIN:
            entry->bio_module_id = BIO_MODULE_BRAIN;
            break;
        case CORE_MODULE_NEURON_TYPES:
            entry->bio_module_id = BIO_MODULE_NEURON_MODEL;
            break;
        case CORE_MODULE_TOPOLOGY:
            entry->bio_module_id = BIO_MODULE_TOPOLOGY;
            break;
        case CORE_MODULE_NEURAL_LOGIC:
            entry->bio_module_id = BIO_MODULE_NEURAL_LOGIC;
            break;
        case CORE_MODULE_NEURALNET:
            entry->bio_module_id = BIO_MODULE_NEURALNET;
            break;
        case CORE_MODULE_AXON:
            entry->bio_module_id = BIO_MODULE_AXON;
            break;
        case CORE_MODULE_SYNAPSE:
            entry->bio_module_id = BIO_MODULE_SYNAPSE;
            break;
        case CORE_MODULE_CORTICAL_COLUMN:
            entry->bio_module_id = BIO_MODULE_CORTICAL_COLUMN;
            break;
        default:
            entry->bio_module_id = BIO_MODULE_BRAIN_REGION;
            break;
    }

    /* Register with bio-router if connected */
    if (bridge->connected && bio_router_is_initialized()) {
        bio_module_info_t info = {
            .module_id = entry->bio_module_id,
            .module_name = entry->name,
            .inbox_capacity = 0,  /* Use default */
            .user_data = bridge
        };
        entry->bio_ctx = bio_router_register_module(&info);
    }

    bridge->module_count++;
    if (bridge->module_count > bridge->stats.peak_modules) {
        bridge->stats.peak_modules = bridge->module_count;
    }
    bridge->stats.registered_modules = bridge->module_count;

    BRIDGE_UNLOCK(bridge);

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_INFO("Registered core module: %s (type=%u)",
            entry->name, (unsigned)type);
    }

    return 0;
}

int core_bio_async_unregister_module(
    core_bio_async_bridge_t* bridge,
    core_module_type_t type)
{
    /* Guard clauses */
    if (!bridge) {
        return CORE_BIO_ERROR_NOT_INITIALIZED;
    }
    if (type >= CORE_MODULE_TYPE_COUNT) {
        return CORE_BIO_ERROR_INVALID_MODULE_TYPE;
    }
    if (!bridge->modules[type].active) {
        return CORE_BIO_ERROR_MODULE_NOT_FOUND;
    }

    BRIDGE_LOCK(bridge);

    core_module_entry_t* entry = &bridge->modules[type];

    /* Unregister from bio-router */
    if (entry->bio_ctx) {
        bio_router_unregister_module(entry->bio_ctx);
        entry->bio_ctx = NULL;
    }

    /* Clear entry */
    entry->active = false;
    entry->module_ptr = NULL;
    bridge->module_count--;
    bridge->stats.registered_modules = bridge->module_count;

    BRIDGE_UNLOCK(bridge);

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_INFO("Unregistered core module: %s", entry->name);
    }

    return 0;
}

bool core_bio_async_get_module(
    const core_bio_async_bridge_t* bridge,
    core_module_type_t type,
    core_module_entry_t* entry)
{
    if (!bridge || type >= CORE_MODULE_TYPE_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "core_bio_async_get_module: bridge is NULL");
        return false;
    }
    if (!bridge->modules[type].active) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "core_bio_async_get_module: bridge->modules is NULL");
        return false;
    }
    if (entry) {
        *entry = bridge->modules[type];
    }
    return true;
}

uint32_t core_bio_async_get_module_count(const core_bio_async_bridge_t* bridge)
{
    return bridge ? bridge->module_count : 0;
}

/* ============================================================================
 * Message Processing API
 * ============================================================================ */

int core_bio_async_process_inbox(
    core_bio_async_bridge_t* bridge,
    uint32_t max_messages)
{
    /* Guard clause */
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "core_bio_async_process_inbox: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    /* Process base bridge inbox */
    uint32_t limit = max_messages > 0 ? max_messages : bridge->config.max_inbox_process;
    uint32_t processed = 0;

    if (bridge->base.bio_ctx) {
        processed = bio_router_process_inbox(bridge->base.bio_ctx, limit);
        bridge->stats.messages_received += processed;
    }

    /* Also process individual module inboxes */
    for (uint32_t i = 0; i < CORE_MODULE_TYPE_COUNT && processed < limit; i++) {
        if (bridge->modules[i].active && bridge->modules[i].bio_ctx) {
            uint32_t mod_processed = bio_router_process_inbox(
                bridge->modules[i].bio_ctx, limit - processed);
            bridge->modules[i].messages_received += mod_processed;
            processed += mod_processed;
        }
    }

    return (int)processed;
}

int core_bio_async_update(
    core_bio_async_bridge_t* bridge,
    uint32_t delta_ms)
{
    /* Guard clause */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    uint64_t now_us = nimcp_time_get_us();

    /* Process pending spikes */
    if (bridge->config.enable_spike_routing && bridge->pending_spike_count > 0) {
        core_bio_async_process_pending_spikes(bridge);
    }

    /* Auto-broadcast state if enabled */
    if (bridge->config.enable_auto_broadcast) {
        uint64_t interval_us = (uint64_t)bridge->config.state_broadcast_interval_ms * 1000;
        if (now_us - bridge->last_state_broadcast_us >= interval_us) {
            /* Broadcast brain state if brain module registered */
            if (bridge->modules[CORE_MODULE_BRAIN].active) {
                core_bio_async_broadcast_brain_state(bridge, 0, 0, 0.0f);
            }
            bridge->last_state_broadcast_us = now_us;
        }
    }

    /* Record update */
    bridge_base_record_update(&bridge->base);

    (void)delta_ms;  /* Used for timing calculations if needed */
    return 0;
}

/* ============================================================================
 * Brain State Coordination API
 * ============================================================================ */

int core_bio_async_broadcast_brain_state(
    core_bio_async_bridge_t* bridge,
    uint32_t active_neurons,
    uint32_t total_neurons,
    float activity_level)
{
    /* Guard clause */
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "core_bio_async_broadcast_brain_state: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    /* Build message */
    core_bio_brain_state_msg_t msg;
    memset(&msg, 0, sizeof(msg));

    msg.header.type = BIO_MSG_BRAIN_STATE_RESPONSE;
    msg.header.source_module = BIO_MODULE_BRAIN;
    msg.header.target_module = 0;  /* Broadcast */
    msg.header.timestamp_us = nimcp_time_get_us();
    msg.header.channel = bridge->config.state_channel;
    msg.header.payload_size = sizeof(msg) - sizeof(bio_message_header_t);
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;

    msg.active_neurons = active_neurons;
    msg.total_neurons = total_neurons;
    msg.global_activity_level = activity_level;
    msg.timestamp_us = msg.header.timestamp_us;

    /* Broadcast */
    if (bridge->base.bio_ctx) {
        nimcp_error_t err = bio_router_broadcast(
            bridge->base.bio_ctx, &msg, sizeof(msg));
        if (err == NIMCP_SUCCESS) {
            bridge->stats.brain_state_broadcasts++;
            bridge->stats.broadcasts_sent++;
            bridge->stats.messages_sent++;
            return 0;
        }
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "core_bio_async_broadcast_brain_state: validation failed");
    return -1;
}

int core_bio_async_broadcast_medulla_state(
    core_bio_async_bridge_t* bridge,
    float arousal_level,
    float protection_level,
    float circadian_phase)
{
    /* Guard clause */
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "core_bio_async_broadcast_medulla_state: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    /* Build message */
    core_bio_medulla_msg_t msg;
    memset(&msg, 0, sizeof(msg));

    msg.header.type = BIO_MSG_MEDULLA_STATE;
    msg.header.source_module = BIO_MODULE_BRAIN;
    msg.header.target_module = 0;  /* Broadcast */
    msg.header.timestamp_us = nimcp_time_get_us();
    msg.header.channel = bridge->config.state_channel;
    msg.header.payload_size = sizeof(msg) - sizeof(bio_message_header_t);
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;

    msg.arousal_level = arousal_level;
    msg.protection_level = protection_level;
    msg.circadian_phase = circadian_phase;
    msg.timestamp_us = msg.header.timestamp_us;

    /* Broadcast */
    if (bridge->base.bio_ctx) {
        nimcp_error_t err = bio_router_broadcast(
            bridge->base.bio_ctx, &msg, sizeof(msg));
        if (err == NIMCP_SUCCESS) {
            bridge->stats.medulla_state_broadcasts++;
            bridge->stats.broadcasts_sent++;
            bridge->stats.messages_sent++;
            return 0;
        }
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "core_bio_async_broadcast_medulla_state: validation failed");
    return -1;
}

int core_bio_async_broadcast_topology_update(
    core_bio_async_bridge_t* bridge,
    uint32_t connections_added,
    uint32_t connections_removed,
    uint32_t neurons_added)
{
    /* Guard clause */
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "core_bio_async_broadcast_topology_update: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    /* Build message */
    core_bio_topology_msg_t msg;
    memset(&msg, 0, sizeof(msg));

    msg.header.type = BIO_MSG_NETWORK_TOPOLOGY_RESPONSE;
    msg.header.source_module = BIO_MODULE_TOPOLOGY;
    msg.header.target_module = 0;  /* Broadcast */
    msg.header.timestamp_us = nimcp_time_get_us();
    msg.header.channel = bridge->config.state_channel;
    msg.header.payload_size = sizeof(msg) - sizeof(bio_message_header_t);
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;

    msg.connections_added = connections_added;
    msg.connections_removed = connections_removed;
    msg.neurons_added = neurons_added;
    msg.timestamp_us = msg.header.timestamp_us;

    /* Broadcast */
    if (bridge->base.bio_ctx) {
        nimcp_error_t err = bio_router_broadcast(
            bridge->base.bio_ctx, &msg, sizeof(msg));
        if (err == NIMCP_SUCCESS) {
            bridge->stats.topology_updates++;
            bridge->stats.broadcasts_sent++;
            bridge->stats.messages_sent++;
            return 0;
        }
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "core_bio_async_broadcast_topology_update: validation failed");
    return -1;
}

/* ============================================================================
 * Spike Routing API
 * ============================================================================ */

int core_bio_async_route_spike(
    core_bio_async_bridge_t* bridge,
    uint32_t neuron_id,
    float amplitude,
    bool is_inhibitory)
{
    /* Guard clause */
    if (!bridge || !bridge->connected) {
        return CORE_BIO_ERROR_NOT_INITIALIZED;
    }
    if (!bridge->config.enable_spike_routing) {
        return 0;  /* Routing disabled */
    }

    /* Build spike message */
    core_bio_spike_msg_t msg;
    memset(&msg, 0, sizeof(msg));

    msg.header.type = BIO_MSG_NEURON_ACTIVATION_REQUEST;
    msg.header.source_module = BIO_MODULE_BRAIN;
    msg.header.target_module = 0;  /* Broadcast to all neurons */
    msg.header.timestamp_us = nimcp_time_get_us();
    msg.header.payload_size = sizeof(msg) - sizeof(bio_message_header_t);

    /* Use priority channel for inhibitory neurons */
    if (is_inhibitory && bridge->config.enable_priority_routing) {
        msg.header.channel = bridge->config.urgent_channel;
        msg.header.flags = BIO_MSG_FLAG_URGENT | BIO_MSG_FLAG_BROADCAST;
        msg.high_priority = true;
    } else {
        msg.header.channel = bridge->config.spike_channel;
        msg.header.flags = BIO_MSG_FLAG_BROADCAST;
        msg.high_priority = false;
    }

    msg.neuron_id = neuron_id;
    msg.spike_amplitude = amplitude;
    msg.is_inhibitory = is_inhibitory;
    msg.spike_time_us = msg.header.timestamp_us;

    /* Broadcast spike */
    if (bridge->base.bio_ctx) {
        uint64_t start_us = nimcp_time_get_us();
        nimcp_error_t err = bio_router_broadcast(
            bridge->base.bio_ctx, &msg, sizeof(msg));

        if (err == NIMCP_SUCCESS) {
            uint64_t latency_us = nimcp_time_get_us() - start_us;
            bridge->stats.spikes_routed++;
            bridge->stats.messages_sent++;

            /* Update latency stats */
            float latency = (float)latency_us;
            bridge->stats.avg_routing_latency_us =
                (bridge->stats.avg_routing_latency_us * 0.99f) + (latency * 0.01f);
            if (latency > bridge->stats.max_routing_latency_us) {
                bridge->stats.max_routing_latency_us = latency;
            }
            return 0;
        } else {
            bridge->stats.routing_errors++;
            return CORE_BIO_ERROR_SPIKE_ROUTING_FAILED;
        }
    }

    return CORE_BIO_ERROR_ROUTER_NOT_AVAILABLE;
}

int core_bio_async_route_spike_batch(
    core_bio_async_bridge_t* bridge,
    const uint32_t* neuron_ids,
    const float* amplitudes,
    uint32_t spike_count)
{
    /* Guard clauses */
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "core_bio_async_route_spike_batch: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }
    if (!neuron_ids || !amplitudes || spike_count == 0) {
        return 0;
    }

    /* Build batch message */
    core_bio_spike_batch_msg_t msg;
    memset(&msg, 0, sizeof(msg));

    msg.header.type = BIO_MSG_NEURON_ACTIVATION_REQUEST;
    msg.header.source_module = BIO_MODULE_BRAIN;
    msg.header.target_module = 0;  /* Broadcast */
    msg.header.timestamp_us = nimcp_time_get_us();
    msg.header.channel = bridge->config.spike_channel;
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;

    /* Limit to batch size */
    uint32_t batch_size = spike_count < CORE_BIO_DEFAULT_SPIKE_BATCH ?
        spike_count : CORE_BIO_DEFAULT_SPIKE_BATCH;

    msg.spike_count = batch_size;
    msg.batch_start_time_us = msg.header.timestamp_us;
    msg.batch_end_time_us = msg.header.timestamp_us;
    msg.header.payload_size = sizeof(msg) - sizeof(bio_message_header_t);

    /* Copy spike data */
    for (uint32_t i = 0; i < batch_size; i++) {
        msg.neuron_ids[i] = neuron_ids[i];
        msg.amplitudes[i] = amplitudes[i];
        msg.times_us[i] = msg.header.timestamp_us;
    }

    /* Broadcast batch */
    int routed = 0;
    if (bridge->base.bio_ctx) {
        nimcp_error_t err = bio_router_broadcast(
            bridge->base.bio_ctx, &msg, sizeof(msg));
        if (err == NIMCP_SUCCESS) {
            routed = (int)batch_size;
            bridge->stats.spikes_routed += batch_size;
            bridge->stats.spike_batches_processed++;
            bridge->stats.messages_sent++;
        }
    }

    /* Handle remaining spikes recursively if any */
    if (spike_count > CORE_BIO_DEFAULT_SPIKE_BATCH) {
        int remaining = core_bio_async_route_spike_batch(
            bridge,
            neuron_ids + CORE_BIO_DEFAULT_SPIKE_BATCH,
            amplitudes + CORE_BIO_DEFAULT_SPIKE_BATCH,
            spike_count - CORE_BIO_DEFAULT_SPIKE_BATCH);
        if (remaining > 0) {
            routed += remaining;
        }
    }

    return routed;
}

int core_bio_async_queue_spike(
    core_bio_async_bridge_t* bridge,
    uint32_t neuron_id,
    float amplitude)
{
    /* Guard clauses */
    if (!bridge) {
        return CORE_BIO_ERROR_NOT_INITIALIZED;
    }

    BRIDGE_LOCK(bridge);

    /* Check capacity */
    if (bridge->pending_spike_count >= bridge->pending_spike_capacity) {
        BRIDGE_UNLOCK(bridge);
        bridge->stats.spikes_dropped++;
        return CORE_BIO_ERROR_MAX_MODULES_EXCEEDED;  /* Queue full */
    }

    /* Add to queue */
    pending_spike_t* spike = &bridge->pending_spikes[bridge->pending_spike_count];
    spike->neuron_id = neuron_id;
    spike->amplitude = amplitude;
    spike->timestamp_us = nimcp_time_get_us();
    spike->is_inhibitory = false;  /* Default, can be enhanced */

    bridge->pending_spike_count++;

    BRIDGE_UNLOCK(bridge);
    return 0;
}

int core_bio_async_process_pending_spikes(core_bio_async_bridge_t* bridge)
{
    /* Guard clause */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    BRIDGE_LOCK(bridge);

    uint32_t count = bridge->pending_spike_count;
    if (count == 0) {
        BRIDGE_UNLOCK(bridge);
        return 0;
    }

    /* Process in batches */
    int processed = 0;
    uint32_t batch_start = 0;

    while (batch_start < count) {
        uint32_t batch_size = count - batch_start;
        if (batch_size > bridge->config.spike_batch_size) {
            batch_size = bridge->config.spike_batch_size;
        }

        /* Extract batch data */
        uint32_t neuron_ids[CORE_BIO_DEFAULT_SPIKE_BATCH];
        float amplitudes[CORE_BIO_DEFAULT_SPIKE_BATCH];

        for (uint32_t i = 0; i < batch_size; i++) {
            neuron_ids[i] = bridge->pending_spikes[batch_start + i].neuron_id;
            amplitudes[i] = bridge->pending_spikes[batch_start + i].amplitude;
        }

        BRIDGE_UNLOCK(bridge);

        /* Route batch */
        int routed = core_bio_async_route_spike_batch(
            bridge, neuron_ids, amplitudes, batch_size);
        if (routed > 0) {
            processed += routed;
        }

        BRIDGE_LOCK(bridge);
        batch_start += batch_size;
    }

    /* Clear queue */
    bridge->pending_spike_count = 0;

    BRIDGE_UNLOCK(bridge);
    return processed;
}

uint32_t core_bio_async_get_pending_spike_count(
    const core_bio_async_bridge_t* bridge)
{
    return bridge ? bridge->pending_spike_count : 0;
}

/* ============================================================================
 * Health Monitoring API
 * ============================================================================ */

int core_bio_async_health_check_all(core_bio_async_bridge_t* bridge)
{
    /* Guard clause */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    int unhealthy = 0;

    for (uint32_t i = 0; i < CORE_MODULE_TYPE_COUNT; i++) {
        if (bridge->modules[i].active) {
            float health;
            if (!core_bio_async_get_module_health(bridge, (core_module_type_t)i, &health)) {
                unhealthy++;
            } else if (health < 0.5f) {
                unhealthy++;
            }
        }
    }

    bridge->stats.health_checks++;
    return unhealthy;
}

bool core_bio_async_get_module_health(
    const core_bio_async_bridge_t* bridge,
    core_module_type_t type,
    float* health_score)
{
    if (!bridge || type >= CORE_MODULE_TYPE_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "core_bio_async_get_module_health: bridge is NULL");
        return false;
    }
    if (!bridge->modules[type].active) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "core_bio_async_get_module_health: bridge->modules is NULL");
        return false;
    }

    if (health_score) {
        *health_score = bridge->module_health[type];
    }

    return bridge->module_health[type] >= 0.5f;
}

/* ============================================================================
 * Statistics and Diagnostics API
 * ============================================================================ */

int core_bio_async_get_stats(
    const core_bio_async_bridge_t* bridge,
    core_bio_async_stats_t* stats)
{
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "core_bio_async_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }
    *stats = bridge->stats;
    return 0;
}

int core_bio_async_reset_stats(core_bio_async_bridge_t* bridge)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    BRIDGE_LOCK(bridge);
    memset(&bridge->stats, 0, sizeof(core_bio_async_stats_t));
    bridge->stats.registered_modules = bridge->module_count;
    bridge->stats.peak_modules = bridge->module_count;
    BRIDGE_UNLOCK(bridge);

    return 0;
}

const char* core_bio_msg_type_name(core_bio_msg_type_t msg_type)
{
    if (msg_type < CORE_MSG_TYPE_COUNT) {
        return g_core_msg_type_names[msg_type];
    }
    return "UNKNOWN";
}

const char* core_module_type_name(core_module_type_t module_type)
{
    if (module_type < CORE_MODULE_TYPE_COUNT) {
        return g_core_module_type_names[module_type];
    }
    return "unknown";
}

void core_bio_async_print_summary(const core_bio_async_bridge_t* bridge)
{
    if (!bridge) {
        printf("Core Bio-Async Bridge: NULL\n");
        return;
    }

    printf("=== Core Bio-Async Bridge Summary ===\n");
    printf("Connected: %s\n", bridge->connected ? "yes" : "no");
    printf("Registered modules: %u\n", bridge->module_count);
    printf("\n");

    printf("Registered Modules:\n");
    for (uint32_t i = 0; i < CORE_MODULE_TYPE_COUNT; i++) {
        if (bridge->modules[i].active) {
            printf("  - %s (bio_id=0x%04X, msgs_sent=%lu, msgs_recv=%lu)\n",
                bridge->modules[i].name,
                (unsigned)bridge->modules[i].bio_module_id,
                (unsigned long)bridge->modules[i].messages_sent,
                (unsigned long)bridge->modules[i].messages_received);
        }
    }
    printf("\n");

    printf("Statistics:\n");
    printf("  Messages sent: %lu\n", (unsigned long)bridge->stats.messages_sent);
    printf("  Messages received: %lu\n", (unsigned long)bridge->stats.messages_received);
    printf("  Broadcasts sent: %lu\n", (unsigned long)bridge->stats.broadcasts_sent);
    printf("  Spikes routed: %lu\n", (unsigned long)bridge->stats.spikes_routed);
    printf("  Spike batches: %lu\n", (unsigned long)bridge->stats.spike_batches_processed);
    printf("  Pending spikes: %u\n", bridge->pending_spike_count);
    printf("  Avg routing latency: %.2f us\n", bridge->stats.avg_routing_latency_us);
    printf("  Max routing latency: %.2f us\n", bridge->stats.max_routing_latency_us);
    printf("  Handler errors: %lu\n", (unsigned long)bridge->stats.handler_errors);
    printf("  Routing errors: %lu\n", (unsigned long)bridge->stats.routing_errors);
    printf("=====================================\n");
}

/* ============================================================================
 * Internal Message Handlers
 * ============================================================================ */

static nimcp_error_t core_bio_handle_brain_state_query(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data)
{
    core_bio_async_bridge_t* bridge = (core_bio_async_bridge_t*)user_data;
    if (!bridge) {
        return NIMCP_BIO_ERROR_NOT_INITIALIZED;
    }

    (void)msg;
    (void)msg_size;

    /* Build response */
    core_bio_brain_state_msg_t response;
    memset(&response, 0, sizeof(response));

    response.header.type = BIO_MSG_BRAIN_STATE_RESPONSE;
    response.header.source_module = BIO_MODULE_BRAIN;
    response.header.timestamp_us = nimcp_time_get_us();
    response.header.payload_size = sizeof(response) - sizeof(bio_message_header_t);
    response.timestamp_us = response.header.timestamp_us;

    /* Complete promise if provided */
    if (response_promise) {
        nimcp_bio_promise_complete(response_promise, &response);
    }

    bridge->stats.messages_received++;
    return NIMCP_SUCCESS;
}

static nimcp_error_t core_bio_handle_spike_route(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data)
{
    core_bio_async_bridge_t* bridge = (core_bio_async_bridge_t*)user_data;
    if (!bridge || !msg) {
        return NIMCP_BIO_ERROR_NOT_INITIALIZED;
    }

    const core_bio_spike_route_msg_t* route_msg = (const core_bio_spike_route_msg_t*)msg;
    if (msg_size < sizeof(bio_message_header_t)) {
        return NIMCP_BIO_ERROR_INVALID_CHANNEL;
    }

    /* Route the spike */
    int result = core_bio_async_route_spike(
        bridge,
        route_msg->source_neuron_id,
        route_msg->spike_amplitude,
        false  /* Default to excitatory */
    );

    /* Complete promise with result */
    if (response_promise) {
        nimcp_bio_promise_complete(response_promise, &result);
    }

    bridge->stats.messages_received++;
    return (result == 0) ? NIMCP_SUCCESS : NIMCP_BIO_ERROR_NOT_INITIALIZED;
}

static nimcp_error_t core_bio_handle_health_check(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data)
{
    core_bio_async_bridge_t* bridge = (core_bio_async_bridge_t*)user_data;
    if (!bridge) {
        return NIMCP_BIO_ERROR_NOT_INITIALIZED;
    }

    (void)msg;
    (void)msg_size;

    /* Build health response */
    core_bio_health_msg_t response;
    memset(&response, 0, sizeof(response));

    response.header.type = BIO_MSG_HEALTH_RESPONSE;
    response.header.source_module = BIO_MODULE_BRAIN;
    response.header.timestamp_us = nimcp_time_get_us();
    response.header.payload_size = sizeof(response) - sizeof(bio_message_header_t);

    response.module_type = CORE_MODULE_BRAIN;
    response.is_healthy = bridge->connected;
    response.health_score = bridge->connected ? 1.0f : 0.0f;
    response.messages_processed = bridge->stats.messages_received;
    response.errors_encountered = bridge->stats.handler_errors + bridge->stats.routing_errors;
    response.timestamp_us = response.header.timestamp_us;

    /* Complete promise */
    if (response_promise) {
        nimcp_bio_promise_complete(response_promise, &response);
    }

    bridge->stats.health_checks++;
    bridge->stats.messages_received++;
    return NIMCP_SUCCESS;
}
