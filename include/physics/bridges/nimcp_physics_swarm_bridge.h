//=============================================================================
// nimcp_physics_swarm_bridge.h - Physics Layer to Swarm System Bridge
//=============================================================================
/**
 * @file nimcp_physics_swarm_bridge.h
 * @brief Bridge connecting Phase 1 Physics modules with Swarm Distribution
 *
 * WHAT: Enables distributed physics simulation across swarm nodes.
 *
 * WHY:  Large-scale biophysics requires distributed computation:
 *       - Partition HH populations across nodes
 *       - Synchronize ephaptic fields at boundaries
 *       - Share thermodynamic state for consistency
 *
 * HOW:  - Serializes physics state for network transfer
 *       - Manages boundary synchronization
 *       - Coordinates distributed updates
 *
 * @author NIMCP Development Team
 * @date 2026-01-13
 * @version 1.0.0
 */

#ifndef NIMCP_PHYSICS_SWARM_BRIDGE_H
#define NIMCP_PHYSICS_SWARM_BRIDGE_H

#include <stdbool.h>
#include <stdint.h>
#include "common/nimcp_export.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** Module name for logging */
#define PHYSICS_SWARM_MODULE_NAME    "physics_swarm_bridge"

/** Maximum nodes in swarm */
#define PHYSICS_SWARM_MAX_NODES      64

/** Default sync interval (ms) */
#define PHYSICS_SWARM_SYNC_INTERVAL  10.0f

//=============================================================================
// Type Definitions
//=============================================================================

/**
 * @brief Physics partition descriptor
 */
typedef struct {
    /** Partition ID */
    uint32_t partition_id;

    /** Node ID owning this partition */
    uint32_t node_id;

    /** Start neuron index */
    uint32_t neuron_start;

    /** Number of neurons */
    uint32_t neuron_count;

    /** Spatial bounds (x, y, z min/max) */
    float bounds[6];

    /** Is this a boundary partition? */
    bool is_boundary;
} physics_swarm_partition_t;

/**
 * @brief Boundary synchronization data
 */
typedef struct {
    /** Source partition */
    uint32_t source_partition;

    /** Target partition */
    uint32_t target_partition;

    /** Boundary field values */
    float* boundary_fields;

    /** Number of boundary points */
    uint32_t num_points;

    /** Timestamp */
    float timestamp_ms;
} physics_swarm_boundary_t;

/**
 * @brief Swarm physics state for serialization
 */
typedef struct {
    /** Global temperature */
    float temperature;

    /** Global ATP level */
    float atp_level;

    /** Global phase coherence */
    float coherence;

    /** Total firing rate */
    float total_rate;

    /** Number of active partitions */
    uint32_t active_partitions;

    /** Simulation time */
    float sim_time_ms;
} physics_swarm_state_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /** Node ID for this instance */
    uint32_t node_id;

    /** Total nodes in swarm */
    uint32_t total_nodes;

    /** Synchronization interval (ms) */
    float sync_interval_ms;

    /** Enable boundary synchronization */
    bool enable_boundary_sync;

    /** Enable state broadcasting */
    bool enable_state_broadcast;
} physics_swarm_config_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    /** Sync operations performed */
    uint64_t sync_count;

    /** Boundary updates received */
    uint64_t boundary_updates;

    /** State broadcasts sent */
    uint64_t broadcasts_sent;

    /** Last sync timestamp */
    float last_sync_ms;
} physics_swarm_stats_t;

/**
 * @brief Opaque bridge structure
 */
typedef struct physics_swarm_bridge_struct physics_swarm_bridge_t;

//=============================================================================
// API Functions
//=============================================================================

/**
 * @brief Get default configuration
 */
NIMCP_EXPORT int physics_swarm_default_config(physics_swarm_config_t* config);

/**
 * @brief Create swarm bridge
 */
NIMCP_EXPORT physics_swarm_bridge_t* physics_swarm_bridge_create(
    const physics_swarm_config_t* config
);

/**
 * @brief Destroy bridge
 */
NIMCP_EXPORT void physics_swarm_bridge_destroy(physics_swarm_bridge_t* bridge);

/**
 * @brief Register partition for this node
 */
NIMCP_EXPORT int physics_swarm_register_partition(
    physics_swarm_bridge_t* bridge,
    const physics_swarm_partition_t* partition
);

/**
 * @brief Get local partition info
 */
NIMCP_EXPORT int physics_swarm_get_partition(
    const physics_swarm_bridge_t* bridge,
    uint32_t index,
    physics_swarm_partition_t* partition
);

/**
 * @brief Serialize local state for broadcast
 */
NIMCP_EXPORT int physics_swarm_serialize_state(
    physics_swarm_bridge_t* bridge,
    physics_swarm_state_t* state
);

/**
 * @brief Receive remote state update
 */
NIMCP_EXPORT int physics_swarm_receive_state(
    physics_swarm_bridge_t* bridge,
    uint32_t source_node,
    const physics_swarm_state_t* state
);

/**
 * @brief Synchronize boundary with neighbor
 */
NIMCP_EXPORT int physics_swarm_sync_boundary(
    physics_swarm_bridge_t* bridge,
    const physics_swarm_boundary_t* boundary
);

/**
 * @brief Update bridge (call each timestep)
 */
NIMCP_EXPORT int physics_swarm_update(
    physics_swarm_bridge_t* bridge,
    float dt
);

/**
 * @brief Get statistics
 */
NIMCP_EXPORT int physics_swarm_get_stats(
    const physics_swarm_bridge_t* bridge,
    physics_swarm_stats_t* stats
);

/**
 * @brief Check if sync is needed
 */
NIMCP_EXPORT bool physics_swarm_needs_sync(
    const physics_swarm_bridge_t* bridge
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PHYSICS_SWARM_BRIDGE_H */
