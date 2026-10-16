#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_physics_swarm_bridge.c - Physics Layer to Swarm System Bridge
//=============================================================================

#include "utils/bridge/nimcp_bridge_base.h"
#include "physics/bridges/nimcp_physics_swarm_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(physics_swarm_bridge)

#define LOG_MODULE "PHYSICS_SWARM_BRIDGE"


//=============================================================================
// Internal Structure
//=============================================================================

struct physics_swarm_bridge_struct {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    physics_swarm_config_t config;

    /** Local partitions */
    physics_swarm_partition_t partitions[PHYSICS_SWARM_MAX_NODES];
    uint32_t num_partitions;

    /** Remote state cache */
    physics_swarm_state_t remote_states[PHYSICS_SWARM_MAX_NODES];
    bool remote_valid[PHYSICS_SWARM_MAX_NODES];

    /** Local state */
    physics_swarm_state_t local_state;

    /** Timing */
    float sync_timer;
    float sim_time;

    /** Statistics */
    physics_swarm_stats_t stats;

    bool initialized;
};

//=============================================================================
// Configuration API
//=============================================================================

int physics_swarm_default_config(physics_swarm_config_t* config) {
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;

    }

    config->node_id = 0;
    config->total_nodes = 1;
    config->sync_interval_ms = PHYSICS_SWARM_SYNC_INTERVAL;
    config->enable_boundary_sync = true;
    config->enable_state_broadcast = true;

    return 0;
}

//=============================================================================
// Lifecycle API
//=============================================================================

physics_swarm_bridge_t* physics_swarm_bridge_create(
    const physics_swarm_config_t* config
) {
    physics_swarm_bridge_t* bridge = nimcp_calloc(1, sizeof(*bridge));
    NIMCP_API_CHECK_ALLOC(bridge, "Failed to allocate physics-swarm bridge");
    if (bridge_base_init(&bridge->base, 0, "physics_swarm") != 0) { nimcp_free(bridge); return NULL; }

    if (config) {
        bridge->config = *config;
    } else {
        physics_swarm_default_config(&bridge->config);
    }

    /* Initialize local state */
    bridge->local_state.temperature = 37.0f;
    bridge->local_state.atp_level = 1.0f;
    bridge->local_state.coherence = 0.5f;

    bridge->initialized = true;

    NIMCP_LOG_INFO(PHYSICS_SWARM_MODULE_NAME,
        "Physics-swarm bridge created: node=%u/%u, sync_interval=%.1fms",
        bridge->config.node_id, bridge->config.total_nodes,
        bridge->config.sync_interval_ms);

    return bridge;
}

void physics_swarm_bridge_destroy(physics_swarm_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "physics_swarm");

    NIMCP_LOG_INFO(PHYSICS_SWARM_MODULE_NAME,
        "Bridge destroyed - syncs: %lu, boundaries: %lu, broadcasts: %lu",
        (unsigned long)bridge->stats.sync_count,
        (unsigned long)bridge->stats.boundary_updates,
        (unsigned long)bridge->stats.broadcasts_sent);

    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

//=============================================================================
// Partition Management
//=============================================================================

int physics_swarm_register_partition(
    physics_swarm_bridge_t* bridge,
    const physics_swarm_partition_t* partition
) {
    if (!bridge || !partition) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "physics_swarm_register_partition: required parameter is NULL (bridge, partition)");
        return -1;
    }
    if (bridge->num_partitions >= PHYSICS_SWARM_MAX_NODES) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "physics_swarm_register_partition: capacity exceeded");
        return -1;
    }

    bridge->partitions[bridge->num_partitions] = *partition;
    bridge->num_partitions++;

    NIMCP_LOG_DEBUG(PHYSICS_SWARM_MODULE_NAME,
        "Registered partition %u: neurons %u-%u, boundary=%d",
        partition->partition_id,
        partition->neuron_start,
        partition->neuron_start + partition->neuron_count,
        partition->is_boundary);

    return 0;
}

int physics_swarm_get_partition(
    const physics_swarm_bridge_t* bridge,
    uint32_t index,
    physics_swarm_partition_t* partition
) {
    if (!bridge || !partition) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "physics_swarm_get_partition: required parameter is NULL (bridge, partition)");
        return -1;
    }
    if (index >= bridge->num_partitions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "physics_swarm_get_partition: capacity exceeded");
        return -1;
    }

    *partition = bridge->partitions[index];
    return 0;
}

//=============================================================================
// State Synchronization
//=============================================================================

int physics_swarm_serialize_state(
    physics_swarm_bridge_t* bridge,
    physics_swarm_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "physics_swarm_serialize_state: required parameter is NULL (bridge, state)");
        return -1;
    }

    bridge->local_state.active_partitions = bridge->num_partitions;
    bridge->local_state.sim_time_ms = bridge->sim_time;

    *state = bridge->local_state;
    bridge->stats.broadcasts_sent++;

    return 0;
}

int physics_swarm_receive_state(
    physics_swarm_bridge_t* bridge,
    uint32_t source_node,
    const physics_swarm_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "physics_swarm_receive_state: required parameter is NULL (bridge, state)");
        return -1;
    }
    if (source_node >= PHYSICS_SWARM_MAX_NODES) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "physics_swarm_receive_state: capacity exceeded");
        return -1;
    }

    bridge->remote_states[source_node] = *state;
    bridge->remote_valid[source_node] = true;

    NIMCP_LOG_DEBUG(PHYSICS_SWARM_MODULE_NAME,
        "Received state from node %u: T=%.1f, ATP=%.2f, coherence=%.2f",
        source_node, state->temperature, state->atp_level, state->coherence);

    return 0;
}

int physics_swarm_sync_boundary(
    physics_swarm_bridge_t* bridge,
    const physics_swarm_boundary_t* boundary
) {
    if (!bridge || !boundary) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "physics_swarm_sync_boundary: required parameter is NULL (bridge, boundary)");
        return -1;
    }

    /* In full implementation, would apply boundary field values */
    bridge->stats.boundary_updates++;

    NIMCP_LOG_DEBUG(PHYSICS_SWARM_MODULE_NAME,
        "Boundary sync: partition %u -> %u, %u points",
        boundary->source_partition,
        boundary->target_partition,
        boundary->num_points);

    return 0;
}

//=============================================================================
// Update API
//=============================================================================

int physics_swarm_update(physics_swarm_bridge_t* bridge, float dt) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    bridge->sync_timer += dt;
    bridge->sim_time += dt;

    /* Check if sync needed */
    if (bridge->sync_timer >= bridge->config.sync_interval_ms) {
        bridge->sync_timer = 0.0f;
        bridge->stats.sync_count++;
        bridge->stats.last_sync_ms = bridge->sim_time;

        /* In full implementation, would trigger actual network sync */
        NIMCP_LOG_DEBUG(PHYSICS_SWARM_MODULE_NAME,
            "Sync triggered at t=%.1fms", bridge->sim_time);
    }

    return 0;
}

int physics_swarm_get_stats(
    const physics_swarm_bridge_t* bridge,
    physics_swarm_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "physics_swarm_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }
    *stats = bridge->stats;
    return 0;
}

bool physics_swarm_needs_sync(const physics_swarm_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "physics_swarm_needs_sync: bridge is NULL");
        return false;
    }
    return bridge->sync_timer >= bridge->config.sync_interval_ms;
}
