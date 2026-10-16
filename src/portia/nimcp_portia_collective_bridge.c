#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_portia_collective_bridge.c - Portia-Collective Cognition Integration
//=============================================================================
/**
 * @file nimcp_portia_collective_bridge.c
 * @brief Implementation of Portia-Collective Cognition bridge
 *
 * @author NIMCP Development Team
 * @date 2025-12-31
 */

#include "portia/nimcp_portia_collective_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "portia/nimcp_portia_tier_switch.h"
#include "cognitive/collective_cognition/nimcp_collective_cognition.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_wiring_helpers.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>

#define LOG_MODULE "PORTIA_COLLECTIVE"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(portia_collective_bridge)

/* Bio-async module ID for Portia collective (0x2Exx range for Portia modules) */
#define BIO_MODULE_PORTIA_COLLECTIVE    0x2E10

//=============================================================================
// Forward Declarations
//=============================================================================

static int find_instance_index(
    const portia_collective_bridge_t* bridge,
    uint32_t instance_id
);

//=============================================================================
// Internal Structure
//=============================================================================

struct portia_collective_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    /* Bio-async context */
    bio_module_context_t bio_ctx;

    /* Configuration */
    portia_collective_config_t config;

    /* Connected systems */
    portia_tier_switch_t portia;
    collective_cognition_t* collective;

    /* Local state */
    uint32_t local_instance_id;
    uint32_t local_tier;
    float local_load;
    bool local_degraded;

    /* Collective state */
    collective_instance_state_t instances[PORTIA_COLLECTIVE_MAX_INSTANCES];
    uint32_t instance_count;
    uint32_t leader_id;
    bool is_leader;

    /* Statistics */
    portia_collective_stats_t stats;

    /* Timing */
    uint64_t last_update_ms;
    uint64_t last_broadcast_ms;

    /* State flags */
    bool initialized;
    bool bio_async_connected;
};

//=============================================================================
// Configuration API
//=============================================================================

void portia_collective_default_config(portia_collective_config_t* config) {
    if (!config) return;

    memset(config, 0, sizeof(*config));

    config->resource_strategy = COLLECTIVE_RESOURCE_COOPERATIVE;
    config->degradation_mode = DEGRADATION_COORD_COMPENSATING;

    config->offload_threshold = PORTIA_COLLECTIVE_OFFLOAD_THRESHOLD;
    config->receive_threshold = PORTIA_COLLECTIVE_RECEIVE_THRESHOLD;
    config->degradation_threshold = 0.7f;

    config->update_interval_ms = PORTIA_COLLECTIVE_DEFAULT_UPDATE_MS;
    config->state_timeout_ms = 5000;

    config->enable_bio_async = true;
    config->enable_proactive_offload = false;
    config->enable_leader_election = true;
    config->broadcast_tier_changes = true;
}

//=============================================================================
// Lifecycle API
//=============================================================================

portia_collective_bridge_t* portia_collective_create(
    const portia_collective_config_t* config,
    portia_tier_switch_t portia,
    collective_cognition_t* collective
) {
    portia_collective_bridge_t* bridge = nimcp_calloc(1, sizeof(*bridge));
    if (!bridge) {
        LOG_ERROR("Failed to allocate portia-collective bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate portia-collective bridge");
        return NULL;
    }

    /* Create mutex for thread safety */
    if (bridge_base_init(&bridge->base, 0, "portia_collective") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        LOG_ERROR("Failed to create mutex for portia-collective bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "Failed to create mutex for portia-collective bridge");
        nimcp_free(bridge);
        return NULL;
    }

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        portia_collective_default_config(&bridge->config);
    }

    /* Connect systems */
    bridge->portia = portia;
    bridge->collective = collective;

    /* Initialize local state */
    bridge->local_instance_id = (uint32_t)(nimcp_time_get_us() & 0xFFFF);
    bridge->local_tier = 3;  /* Assume FULL tier initially */
    bridge->local_load = 0.0f;
    bridge->local_degraded = false;

    /* Initialize collective state */
    bridge->instance_count = 0;
    bridge->leader_id = bridge->local_instance_id;
    bridge->is_leader = true;  /* Initially leader until others join */

    /* Timing */
    bridge->last_update_ms = nimcp_time_get_ms();
    bridge->last_broadcast_ms = 0;

    bridge->initialized = true;

    LOG_INFO("Portia-collective bridge created (local_id=%u)", bridge->local_instance_id);

    return bridge;
}

void portia_collective_destroy(portia_collective_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "portia_collective");

    if (bridge->bio_async_connected) {
        portia_collective_disconnect_bio_async(bridge);
    }

    /* Destroy mutex */
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    LOG_INFO("Portia-collective bridge destroyed (local_id=%u)", bridge->local_instance_id);

    nimcp_free(bridge);
}

int portia_collective_reset(portia_collective_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "portia_collective_reset: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Reset collective state */
    memset(bridge->instances, 0, sizeof(bridge->instances));
    bridge->instance_count = 0;
    bridge->leader_id = bridge->local_instance_id;
    bridge->is_leader = true;

    /* Reset statistics */
    memset(&bridge->stats, 0, sizeof(bridge->stats));

    /* Reset timing */
    bridge->last_update_ms = nimcp_time_get_ms();
    bridge->last_broadcast_ms = 0;

    nimcp_mutex_unlock(bridge->base.mutex);

    LOG_DEBUG("Portia-collective bridge reset");

    return 0;
}

//=============================================================================
// Connection API
//=============================================================================

int portia_collective_connect_portia(
    portia_collective_bridge_t* bridge,
    portia_tier_switch_t portia
) {
    if (!bridge) {
        LOG_ERROR("Null bridge in connect_portia");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Null bridge in portia_collective_connect_portia");
        return -1;
    }

    bridge->portia = portia;

    if (portia) {
        /* Query current tier from portia */
        /* Note: Would need portia API to get current tier */
        LOG_INFO("Connected to Portia tier switch");
    } else {
        LOG_INFO("Disconnected from Portia tier switch");
    }

    return 0;
}

int portia_collective_connect_collective(
    portia_collective_bridge_t* bridge,
    collective_cognition_t* collective
) {
    if (!bridge) {
        LOG_ERROR("Null bridge in connect_collective");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Null bridge in portia_collective_connect_collective");
        return -1;
    }

    bridge->collective = collective;

    if (collective) {
        LOG_INFO("Connected to collective cognition system");
    } else {
        LOG_INFO("Disconnected from collective cognition system");
    }

    return 0;
}

//=============================================================================
// Internal Helpers
//=============================================================================

static int find_instance_index(
    const portia_collective_bridge_t* bridge,
    uint32_t instance_id
) {
    for (uint32_t i = 0; i < bridge->instance_count; ++i) {
        if (bridge->instances[i].instance_id == instance_id) {
            return (int)i;
        }
    }
    return -1;  /* Not found - normal search miss */
}

static void update_local_state(portia_collective_bridge_t* bridge) {
    /* Update local instance in the instances array */
    int idx = find_instance_index(bridge, bridge->local_instance_id);
    if (idx < 0) {
        /* Add ourselves if not present */
        if (bridge->instance_count < PORTIA_COLLECTIVE_MAX_INSTANCES) {
            idx = (int)bridge->instance_count++;
        } else {
            LOG_WARNING("Cannot update local state: max instances (%u) reached",
                        PORTIA_COLLECTIVE_MAX_INSTANCES);
            return;
        }
    }

    collective_instance_state_t* state = &bridge->instances[idx];
    state->instance_id = bridge->local_instance_id;
    state->tier = bridge->local_tier;
    state->load_factor = bridge->local_load;
    state->is_degraded = bridge->local_degraded;
    state->can_receive_tasks = (bridge->local_load < bridge->config.receive_threshold);
    state->is_leader = bridge->is_leader;
    state->last_update_ms = nimcp_time_get_ms();

    /* Query power/thermal from portia if connected */
    if (bridge->portia) {
        state->power_level = 1.0f;  /* Would query portia */
        state->thermal_headroom = 1.0f;
    } else {
        state->power_level = 1.0f;
        state->thermal_headroom = 1.0f;
    }
}

static void elect_leader(portia_collective_bridge_t* bridge) {
    if (!bridge->config.enable_leader_election) return;

    /* Simple leader election: highest tier, then lowest load, then lowest ID */
    uint32_t best_id = bridge->local_instance_id;
    uint32_t best_tier = bridge->local_tier;
    float best_load = bridge->local_load;

    for (uint32_t i = 0; i < bridge->instance_count; ++i) {
        const collective_instance_state_t* inst = &bridge->instances[i];
        if (inst->is_degraded) continue;

        bool is_better = false;
        if (inst->tier > best_tier) {
            is_better = true;
        } else if (inst->tier == best_tier && inst->load_factor < best_load) {
            is_better = true;
        } else if (inst->tier == best_tier && inst->load_factor == best_load &&
                   inst->instance_id < best_id) {
            is_better = true;
        }

        if (is_better) {
            best_id = inst->instance_id;
            best_tier = inst->tier;
            best_load = inst->load_factor;
        }
    }

    if (best_id != bridge->leader_id) {
        bridge->leader_id = best_id;
        bridge->is_leader = (best_id == bridge->local_instance_id);
        bridge->stats.leader_changes++;
        LOG_INFO("Leader changed to instance %u", best_id);
    }
}

static void prune_stale_instances(portia_collective_bridge_t* bridge) {
    uint64_t now = nimcp_time_get_ms();
    uint32_t timeout = bridge->config.state_timeout_ms;

    uint32_t write_idx = 0;
    for (uint32_t read_idx = 0; read_idx < bridge->instance_count; ++read_idx) {
        collective_instance_state_t* inst = &bridge->instances[read_idx];

        /* Keep local instance and non-stale instances */
        /* Fix potential underflow when now < last_update_ms */
        if (inst->instance_id == bridge->local_instance_id ||
            (now >= inst->last_update_ms && (now - inst->last_update_ms) < timeout)) {
            if (write_idx != read_idx) {
                bridge->instances[write_idx] = bridge->instances[read_idx];
            }
            write_idx++;
        } else {
            LOG_DEBUG("Pruned stale instance %u", inst->instance_id);
        }
    }

    bridge->instance_count = write_idx;
}

//=============================================================================
// Bio-Async Message Handler
//=============================================================================

/**
 * @brief Handle incoming bio-async messages for the Portia collective
 *
 * WHAT: Process incoming bio-async messages from remote instances
 * WHY:  Enable distributed coordination of tier states and load balancing
 * HOW:  Switch on message type and dispatch to appropriate handler
 *
 * @param msg Pointer to message (header + payload)
 * @param msg_size Total message size
 * @param response_promise Promise for response (may be NULL)
 * @param user_data Pointer to portia_collective_bridge_t
 * @return NIMCP_SUCCESS or error code
 */
static nimcp_error_t portia_collective_message_handler(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data
) {
    portia_collective_bridge_t* bridge = (portia_collective_bridge_t*)user_data;
    if (!bridge || !msg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "portia_collective_message_handler: required parameter is NULL (bridge, msg)");
        return NIMCP_ERROR_NULL_POINTER;
    }

    (void)response_promise;  /* Currently unused - fire-and-forget messages */

    const bio_message_header_t* header = (const bio_message_header_t*)msg;
    const void* payload = (const uint8_t*)msg + sizeof(bio_message_header_t);
    size_t payload_size = msg_size - sizeof(bio_message_header_t);

    switch (header->type) {
        case BIO_MSG_PORTIA_COLLECTIVE_TIER_CHANGE: {
            if (payload_size < sizeof(uint32_t) * 2) {
                return NIMCP_ERROR_INVALID_PARAM;
            }
            const uint32_t* data = (const uint32_t*)payload;
            uint32_t instance_id = data[0];
            uint32_t new_tier = data[1];
            return portia_collective_handle_remote_tier(bridge, instance_id, new_tier);
        }
        case BIO_MSG_PORTIA_COLLECTIVE_STATE_UPDATE: {
            /* Handle state update from remote instance */
            if (payload_size < sizeof(collective_instance_state_t)) {
                return NIMCP_ERROR_INVALID_PARAM;
            }
            const collective_instance_state_t* state =
                (const collective_instance_state_t*)payload;

            /* Update remote instance state */
            nimcp_mutex_lock(bridge->base.mutex);
            int idx = find_instance_index(bridge, state->instance_id);
            if (idx < 0 && bridge->instance_count < PORTIA_COLLECTIVE_MAX_INSTANCES) {
                idx = (int)bridge->instance_count++;
            }
            if (idx >= 0) {
                bridge->instances[idx] = *state;
                bridge->stats.tier_events_received++;
            }
            nimcp_mutex_unlock(bridge->base.mutex);
            return 0;
        }
        case BIO_MSG_PORTIA_COLLECTIVE_OFFLOAD_REQUEST: {
            /* Handle offload request from remote instance */
            if (payload_size < sizeof(uint32_t) * 2) {
                return NIMCP_ERROR_INVALID_PARAM;
            }
            const uint32_t* data = (const uint32_t*)payload;
            uint32_t source_id = data[0];
            /* uint32_t task_complexity_bits = data[1]; */
            /* Would process offload request and potentially accept task */
            LOG_DEBUG("Received offload request from instance %u", source_id);
            nimcp_mutex_lock(bridge->base.mutex);
            bridge->stats.tasks_received++;
            nimcp_mutex_unlock(bridge->base.mutex);
            return 0;
        }
        case BIO_MSG_PORTIA_COLLECTIVE_DEGRADATION: {
            /* Handle degradation notification from remote instance */
            if (payload_size < sizeof(uint32_t) * 2) {
                return NIMCP_ERROR_INVALID_PARAM;
            }
            const uint32_t* data = (const uint32_t*)payload;
            uint32_t instance_id = data[0];
            bool is_degraded = (data[1] != 0);

            nimcp_mutex_lock(bridge->base.mutex);
            int idx = find_instance_index(bridge, instance_id);
            if (idx >= 0) {
                bridge->instances[idx].is_degraded = is_degraded;
                bridge->instances[idx].last_update_ms = nimcp_time_get_ms();
            }
            nimcp_mutex_unlock(bridge->base.mutex);

            LOG_DEBUG("Instance %u degradation state: %s",
                      instance_id, is_degraded ? "degraded" : "normal");
            return 0;
        }
        default:
            LOG_DEBUG("Unknown message type: 0x%04X", header->type);
            return NIMCP_ERROR_INVALID_PARAM;
    }
}

//=============================================================================
// KG-Driven Wiring Callback
//=============================================================================

/**
 * @brief Wiring callback for KG-driven handler registration
 *
 * WHAT: Register message handlers based on discovered wiring from KG
 * WHY:  Enables runtime assembly - module discovers its handlers from KG
 * HOW:  Orchestrator invokes this with message types from HANDLES_MESSAGE relations
 */
static int portia_collective_wiring_handler_callback(
    bio_module_context_t ctx,
    const bio_message_type_t* message_types,
    uint32_t message_count,
    void* user_data
) {
    (void)user_data;

    int registered = 0;
    for (uint32_t i = 0; i < message_count; i++) {
        switch (message_types[i]) {
            case BIO_MSG_PORTIA_COLLECTIVE_TIER_CHANGE:
            case BIO_MSG_PORTIA_COLLECTIVE_STATE_UPDATE:
            case BIO_MSG_PORTIA_COLLECTIVE_OFFLOAD_REQUEST:
            case BIO_MSG_PORTIA_COLLECTIVE_DEGRADATION:
                bio_router_register_handler(ctx, message_types[i], portia_collective_message_handler);
                registered++;
                break;
            default:
                LOG_DEBUG("Unknown message type 0x%04X in wiring callback", message_types[i]);
                break;
        }
    }

    LOG_INFO("KG-driven wiring callback registered %d handlers", registered);
    return (registered > 0) ? 0 : -1;
}

//=============================================================================
// Update API
//=============================================================================

int portia_collective_update(
    portia_collective_bridge_t* bridge,
    uint64_t delta_ms
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "portia_collective_update: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    (void)delta_ms;  /* Used for timing calculations if needed */

    uint64_t now = nimcp_time_get_ms();

    /* Update local state */
    update_local_state(bridge);

    /* Prune stale instances */
    prune_stale_instances(bridge);

    /* Re-elect leader if needed */
    elect_leader(bridge);

    /* Broadcast state if interval elapsed */
    if (bridge->config.broadcast_tier_changes &&
        (now - bridge->last_broadcast_ms) >= bridge->config.update_interval_ms) {

        /* Would broadcast via bio-async or collective cognition messaging */
        bridge->last_broadcast_ms = now;
    }

    /* Update average load statistic */
    float total_load = 0.0f;
    for (uint32_t i = 0; i < bridge->instance_count; ++i) {
        total_load += bridge->instances[i].load_factor;
    }
    if (bridge->instance_count > 0) {
        bridge->stats.avg_collective_load = total_load / (float)bridge->instance_count;
    }

    bridge->last_update_ms = now;

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int portia_collective_broadcast_tier(
    portia_collective_bridge_t* bridge,
    uint32_t new_tier
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "portia_collective_broadcast_tier: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    uint32_t local_id = bridge->local_instance_id;
    bool should_broadcast = bridge->bio_async_connected && bridge->bio_ctx;
    bio_module_context_t bio_ctx = bridge->bio_ctx;

    bridge->local_tier = new_tier;
    bridge->stats.tier_broadcasts++;

    /* Update local state immediately */
    update_local_state(bridge);

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Send tier change via bio-async when connected */
    if (should_broadcast) {
        /* Prepare message with header */
        uint8_t msg_buf[sizeof(bio_message_header_t) + sizeof(uint32_t) * 2];
        bio_message_header_t* header = (bio_message_header_t*)msg_buf;
        memset(header, 0, sizeof(*header));
        header->type = BIO_MSG_PORTIA_COLLECTIVE_TIER_CHANGE;
        header->source_module = BIO_MODULE_PORTIA_COLLECTIVE;
        header->target_module = 0;  /* Broadcast */
        header->payload_size = sizeof(uint32_t) * 2;
        header->channel = BIO_CHANNEL_DOPAMINE;  /* Resource state uses DA */
        header->flags = BIO_MSG_FLAG_BROADCAST;

        uint32_t* payload = (uint32_t*)(msg_buf + sizeof(bio_message_header_t));
        payload[0] = local_id;
        payload[1] = new_tier;

        bio_router_broadcast(bio_ctx, msg_buf, sizeof(msg_buf));
        LOG_DEBUG("Broadcast tier change via bio-async: tier=%u", new_tier);
    } else {
        LOG_DEBUG("Broadcasting tier change: %u (bio-async not connected)", new_tier);
    }

    return 0;
}

int portia_collective_handle_remote_tier(
    portia_collective_bridge_t* bridge,
    uint32_t instance_id,
    uint32_t new_tier
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "portia_collective_handle_remote_tier: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    if (instance_id == bridge->local_instance_id) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0;  /* Ignore self */
    }

    int idx = find_instance_index(bridge, instance_id);
    if (idx < 0) {
        /* New instance - add it */
        if (bridge->instance_count >= PORTIA_COLLECTIVE_MAX_INSTANCES) {
            LOG_WARNING("Cannot add instance %u: max instances reached", instance_id);
            nimcp_mutex_unlock(bridge->base.mutex);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "portia_collective_handle_remote_tier: capacity exceeded");
            return -1;
        }
        idx = (int)bridge->instance_count++;
        memset(&bridge->instances[idx], 0, sizeof(bridge->instances[idx]));
        bridge->instances[idx].instance_id = instance_id;
    }

    bridge->instances[idx].tier = new_tier;
    bridge->instances[idx].last_update_ms = nimcp_time_get_ms();
    bridge->stats.tier_events_received++;

    /* Re-evaluate leader */
    elect_leader(bridge);

    nimcp_mutex_unlock(bridge->base.mutex);

    LOG_DEBUG("Received tier %u from instance %u", new_tier, instance_id);

    return 0;
}

//=============================================================================
// Load Balancing API
//=============================================================================

/**
 * @brief Internal unlocked version of request_offload
 * @note Caller must hold bridge->base.mutex
 */
static int request_offload_unlocked(
    portia_collective_bridge_t* bridge,
    float task_complexity,
    uint32_t* target_instance
) {
    /* Find best instance to receive task */
    uint32_t best_id = 0;
    float best_score = -1.0f;

    for (uint32_t i = 0; i < bridge->instance_count; ++i) {
        const collective_instance_state_t* inst = &bridge->instances[i];

        /* Skip self, degraded, and overloaded instances */
        if (inst->instance_id == bridge->local_instance_id) continue;
        if (inst->is_degraded) continue;
        if (!inst->can_receive_tasks) continue;

        /* Score based on tier, available capacity, and power */
        float capacity = 1.0f - inst->load_factor;
        float tier_score = (float)inst->tier / 3.0f;
        float score = capacity * 0.5f + tier_score * 0.3f + inst->power_level * 0.2f;

        /* Penalize if complexity exceeds tier capability */
        if (task_complexity > tier_score) {
            score *= 0.5f;
        }

        if (score > best_score) {
            best_score = score;
            best_id = inst->instance_id;
        }
    }

    if (best_score < 0.0f) {
        return -1;  /* No suitable target found - normal condition */
    }

    *target_instance = best_id;
    bridge->stats.tasks_offloaded++;

    return 0;
}

int portia_collective_request_offload(
    portia_collective_bridge_t* bridge,
    float task_complexity,
    uint32_t* target_instance
) {
    if (!bridge || !target_instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "portia_collective_request_offload: required parameter is NULL (bridge, target_instance)");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    int result = request_offload_unlocked(bridge, task_complexity, target_instance);

    nimcp_mutex_unlock(bridge->base.mutex);

    if (result == 0) {
        LOG_DEBUG("Offload request: complexity=%.2f -> instance %u",
                  task_complexity, *target_instance);
    }

    return result;
}

bool portia_collective_can_receive(const portia_collective_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    bool result = !bridge->local_degraded &&
                  bridge->local_load < bridge->config.receive_threshold;

    nimcp_mutex_unlock(bridge->base.mutex);

    return result;
}

int portia_collective_rebalance(portia_collective_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "portia_collective_rebalance: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    if (!bridge->is_leader) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0;  /* Only leader rebalances */
    }

    /* Simple rebalancing: identify overloaded and underloaded instances */
    uint32_t redistributed = 0;

    for (uint32_t i = 0; i < bridge->instance_count; ++i) {
        collective_instance_state_t* inst = &bridge->instances[i];

        if (inst->load_factor > bridge->config.offload_threshold &&
            !inst->is_degraded) {
            /* This instance needs to offload */
            uint32_t target = 0;
            if (request_offload_unlocked(bridge, 0.5f, &target) == 0) {
                redistributed++;
                /* Would send offload request message */
            }
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    LOG_DEBUG("Rebalance complete: %u tasks redistributed", redistributed);

    return (int)redistributed;
}

//=============================================================================
// Degradation API
//=============================================================================

/**
 * @brief Internal unlocked version of request_compensation
 * @note Caller must hold bridge->base.mutex
 */
static int request_compensation_unlocked(portia_collective_bridge_t* bridge) {
    if (!bridge->local_degraded) return 0;

    LOG_INFO("Requesting compensation from collective (local degraded)");

    /* Would broadcast compensation request to collective */
    /* Other instances would take over critical functions */

    return 0;
}

int portia_collective_coordinate_degradation(
    portia_collective_bridge_t* bridge,
    bool local_degraded
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "portia_collective_coordinate_degradation: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    bool was_degraded = bridge->local_degraded;
    bridge->local_degraded = local_degraded;

    if (local_degraded && !was_degraded) {
        bridge->stats.degradation_events++;

        switch (bridge->config.degradation_mode) {
            case DEGRADATION_COORD_COMPENSATING:
                /* Request others to compensate */
                request_compensation_unlocked(bridge);
                break;

            case DEGRADATION_COORD_SYNCHRONIZED:
                /* Would broadcast degradation to all */
                break;

            case DEGRADATION_COORD_CASCADING:
                /* Would trigger staged degradation */
                break;

            case DEGRADATION_COORD_NONE:
            default:
                break;
        }
    }

    update_local_state(bridge);

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int portia_collective_request_compensation(portia_collective_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "portia_collective_request_compensation: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    int result = request_compensation_unlocked(bridge);

    nimcp_mutex_unlock(bridge->base.mutex);

    return result;
}

//=============================================================================
// Query API
//=============================================================================

int portia_collective_get_summary(
    const portia_collective_bridge_t* bridge,
    collective_resource_summary_t* summary
) {
    if (!bridge || !summary) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "portia_collective_get_summary: required parameter is NULL (bridge, summary)");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    memset(summary, 0, sizeof(*summary));

    summary->total_instances = bridge->instance_count;
    summary->leader_instance_id = bridge->leader_id;

    float total_load = 0.0f;
    float total_tier = 0.0f;

    for (uint32_t i = 0; i < bridge->instance_count; ++i) {
        const collective_instance_state_t* inst = &bridge->instances[i];

        if (inst->tier == 3) {  /* PLATFORM_TIER_FULL */
            summary->instances_full_tier++;
        }
        if (inst->is_degraded) {
            summary->instances_degraded++;
        }

        total_load += inst->load_factor;
        total_tier += (float)inst->tier;
    }

    if (bridge->instance_count > 0) {
        summary->average_load = total_load / (float)bridge->instance_count;
        summary->average_tier = total_tier / (float)bridge->instance_count;
    }

    summary->collective_capacity = (float)bridge->instance_count * summary->average_tier / 3.0f;
    summary->collective_utilization = summary->average_load;
    summary->collective_stressed = (summary->average_load > bridge->config.degradation_threshold);

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int portia_collective_get_instance_state(
    const portia_collective_bridge_t* bridge,
    uint32_t instance_id,
    collective_instance_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "portia_collective_get_instance_state: required parameter is NULL (bridge, state)");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    int idx = find_instance_index(bridge, instance_id);
    if (idx < 0) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "portia_collective_get_instance_state: validation failed");
        return -1;
    }

    *state = bridge->instances[idx];

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

uint32_t portia_collective_get_local_id(const portia_collective_bridge_t* bridge) {
    if (!bridge) return 0;

    nimcp_mutex_lock(bridge->base.mutex);
    uint32_t local_id = bridge->local_instance_id;
    nimcp_mutex_unlock(bridge->base.mutex);

    return local_id;
}

bool portia_collective_is_leader(const portia_collective_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bool is_leader = bridge->is_leader;
    nimcp_mutex_unlock(bridge->base.mutex);

    return is_leader;
}

//=============================================================================
// Statistics API
//=============================================================================

int portia_collective_get_stats(
    const portia_collective_bridge_t* bridge,
    portia_collective_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "portia_collective_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

void portia_collective_reset_stats(portia_collective_bridge_t* bridge) {
    if (!bridge) return;

    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    nimcp_mutex_unlock(bridge->base.mutex);
}

//=============================================================================
// Bio-Async API
//=============================================================================

int portia_collective_connect_bio_async(portia_collective_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "portia_collective_connect_bio_async: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* If already connected, disconnect first */
    if (bridge->bio_async_connected && bridge->bio_ctx) {
        nimcp_mutex_unlock(bridge->base.mutex);
        portia_collective_disconnect_bio_async(bridge);
        nimcp_mutex_lock(bridge->base.mutex);
    }

    /* Check if router is initialized */
    if (!bio_router_is_initialized()) {
        LOG_WARNING("Bio-async router not initialized, skipping registration");
        bridge->bio_async_connected = false;
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0;
    }

    /* Register with the bio-async router */
    bio_module_info_t info = {
        .module_id = BIO_MODULE_PORTIA_COLLECTIVE,
        .module_name = "portia_collective_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->bio_ctx = bio_router_register_module(&info);
    if (!bridge->bio_ctx) {
        LOG_WARNING("Failed to register with bio-async router");
        bridge->bio_async_connected = false;
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "portia_collective_connect_bio_async: bridge->bio_ctx is NULL");
        return -1;
    }

    /* KG-Driven Wiring: Register callback for orchestrator to invoke */
    nimcp_error_t cb_result = bio_router_register_wiring_callback(
        BIO_MODULE_PORTIA_COLLECTIVE,
        (void*)portia_collective_wiring_handler_callback,
        bridge
    );

    if (cb_result == NIMCP_SUCCESS) {
        LOG_INFO("Bio-async registered with KG-driven wiring callback (module_id=0x%04X)",
                 BIO_MODULE_PORTIA_COLLECTIVE);
    } else {
        /* Fallback: Direct registration if orchestrator not available */
        LEGACY_HANDLER_REGISTRATION(
            bio_router_register_handler(bridge->bio_ctx, BIO_MSG_PORTIA_COLLECTIVE_TIER_CHANGE,
                                        portia_collective_message_handler)
        );
        LEGACY_HANDLER_REGISTRATION(
            bio_router_register_handler(bridge->bio_ctx, BIO_MSG_PORTIA_COLLECTIVE_STATE_UPDATE,
                                        portia_collective_message_handler)
        );
        LEGACY_HANDLER_REGISTRATION(
            bio_router_register_handler(bridge->bio_ctx, BIO_MSG_PORTIA_COLLECTIVE_OFFLOAD_REQUEST,
                                        portia_collective_message_handler)
        );
        LEGACY_HANDLER_REGISTRATION(
            bio_router_register_handler(bridge->bio_ctx, BIO_MSG_PORTIA_COLLECTIVE_DEGRADATION,
                                        portia_collective_message_handler)
        );
        LOG_INFO("Bio-async registered with legacy handler registration (module_id=0x%04X)",
                 BIO_MODULE_PORTIA_COLLECTIVE);
    }

    bridge->bio_async_connected = true;

    nimcp_mutex_unlock(bridge->base.mutex);

    LOG_INFO("Connected to bio-async router (module_id=0x%04X)", BIO_MODULE_PORTIA_COLLECTIVE);
    return 0;
}

int portia_collective_disconnect_bio_async(portia_collective_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "portia_collective_disconnect_bio_async: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    if (bridge->bio_async_connected && bridge->bio_ctx) {
        /* Unregister handlers first */
        bio_router_unregister_handler(bridge->bio_ctx, BIO_MSG_PORTIA_COLLECTIVE_TIER_CHANGE);
        bio_router_unregister_handler(bridge->bio_ctx, BIO_MSG_PORTIA_COLLECTIVE_STATE_UPDATE);
        bio_router_unregister_handler(bridge->bio_ctx, BIO_MSG_PORTIA_COLLECTIVE_OFFLOAD_REQUEST);
        bio_router_unregister_handler(bridge->bio_ctx, BIO_MSG_PORTIA_COLLECTIVE_DEGRADATION);

        /* Unregister module */
        bio_router_unregister_module(bridge->bio_ctx);
        bridge->bio_ctx = NULL;
    }

    bridge->bio_async_connected = false;

    nimcp_mutex_unlock(bridge->base.mutex);

    LOG_INFO("Disconnected from bio-async router");
    return 0;
}
