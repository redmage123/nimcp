/**
 * @file nimcp_mesh_health_bridge.h
 * @brief Health Agent Mesh Integration Bridge
 *
 * WHAT: Integrates health agent heartbeats with mesh participants
 * WHY:  Distributed health monitoring through mesh consensus
 * HOW:  Heartbeat routing, health aggregation, mesh-wide status
 *
 * ARCHITECTURE:
 * ```
 * ┌─────────────────┐
 * │ Module A        │──┐
 * │ (Health Agent)  │  │
 * └─────────────────┘  │
 *                      │    ┌─────────────────┐
 * ┌─────────────────┐  ├───>│  Health Bridge  │
 * │ Module B        │──┤    │                 │
 * │ (Health Agent)  │  │    │ - Aggregate     │
 * └─────────────────┘  │    │ - Route         │
 *                      │    │ - Consensus     │
 * ┌─────────────────┐  │    └────────┬────────┘
 * │ Module C        │──┘             │
 * │ (Health Agent)  │                ▼
 * └─────────────────┘    ┌─────────────────────────┐
 *                        │      Mesh Network       │
 *                        │ - Channel health status │
 *                        │ - System-wide metrics   │
 *                        │ - Coordinator pool      │
 *                        └─────────────────────────┘
 * ```
 *
 * @author NIMCP Development Team
 * @date 2025-01-31
 * @version 1.0.0
 */

#ifndef NIMCP_MESH_HEALTH_BRIDGE_H
#define NIMCP_MESH_HEALTH_BRIDGE_H

#include "mesh/nimcp_mesh_types.h"
#include "utils/error/nimcp_error_codes.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct mesh_bootstrap mesh_bootstrap_t;
typedef struct mesh_health_bridge mesh_health_bridge_t;
typedef struct nimcp_health_agent nimcp_health_agent_t;

/* Alias for shorter name in mesh context */
typedef nimcp_health_agent_t health_agent_t;

/* ============================================================================
 * Health Status Types
 * ============================================================================ */

/**
 * @brief Module health status
 */
typedef enum mesh_health_status {
    MESH_HEALTH_UNKNOWN = 0,              /**< Status not yet determined */
    MESH_HEALTH_HEALTHY,                  /**< Operating normally */
    MESH_HEALTH_DEGRADED,                 /**< Degraded performance */
    MESH_HEALTH_UNHEALTHY,                /**< Significant issues */
    MESH_HEALTH_CRITICAL,                 /**< Critical condition */
    MESH_HEALTH_DEAD,                     /**< No heartbeat response */
} mesh_health_status_t;

/**
 * @brief Heartbeat operation type
 */
typedef enum mesh_heartbeat_op {
    MESH_HEARTBEAT_START = 0,             /**< Starting operation */
    MESH_HEARTBEAT_PROGRESS,              /**< Progress update */
    MESH_HEARTBEAT_COMPLETE,              /**< Operation complete */
    MESH_HEARTBEAT_ERROR,                 /**< Error occurred */
    MESH_HEARTBEAT_PING,                  /**< Simple ping */
} mesh_heartbeat_op_t;

/* ============================================================================
 * Health Record Structures
 * ============================================================================ */

/**
 * @brief Health record for a participant
 */
typedef struct mesh_health_record {
    mesh_participant_id_t participant_id; /**< Participant ID */
    mesh_health_status_t status;          /**< Current health status */

    /* Heartbeat tracking */
    uint64_t last_heartbeat_ns;           /**< Last heartbeat timestamp */
    uint64_t heartbeat_interval_ms;       /**< Expected interval */
    uint32_t missed_heartbeats;           /**< Consecutive missed heartbeats */

    /* Metrics */
    float cpu_usage;                      /**< CPU usage [0-1] */
    float memory_usage;                   /**< Memory usage [0-1] */
    float transaction_success_rate;       /**< Transaction success rate [0-1] */
    float avg_latency_ms;                 /**< Average latency */

    /* Health score (composite) */
    float health_score;                   /**< Composite health score [0-1] */

    /* Error tracking */
    uint32_t error_count;                 /**< Recent errors */
    uint64_t last_error_ns;               /**< Last error timestamp */

} mesh_health_record_t;

/**
 * @brief Channel health summary
 */
typedef struct mesh_channel_health {
    mesh_channel_id_t channel_id;         /**< Channel ID */
    mesh_health_status_t status;          /**< Overall channel status */

    /* Participant stats */
    size_t total_participants;            /**< Total participants */
    size_t healthy_participants;          /**< Healthy count */
    size_t degraded_participants;         /**< Degraded count */
    size_t unhealthy_participants;        /**< Unhealthy count */
    size_t dead_participants;             /**< Dead/unresponsive count */

    /* Aggregate metrics */
    float avg_health_score;               /**< Average health score */
    float min_health_score;               /**< Minimum health score */
    float consensus_latency_ms;           /**< Consensus latency */
    float transaction_throughput;         /**< Transactions per second */

} mesh_channel_health_t;

/**
 * @brief System-wide health summary
 */
typedef struct mesh_system_health {
    mesh_health_status_t status;          /**< Overall system status */

    /* Channel summaries */
    mesh_channel_health_t channels[8];    /**< Per-channel health */
    size_t channel_count;                 /**< Number of channels */

    /* System metrics */
    size_t total_participants;            /**< Total participants */
    size_t healthy_percentage;            /**< Percentage healthy */
    float system_health_score;            /**< System health score [0-1] */
    float system_free_energy;             /**< System free energy */

    /* Coordinator pool health */
    size_t coordinator_pool_size;         /**< Coordinator pool size */
    size_t active_coordinators;           /**< Active coordinators */
    bool leader_healthy;                  /**< Leader is healthy */

    /* Ordering service health */
    bool ordering_service_healthy;        /**< Ordering service status */
    uint64_t last_ordered_tx_ns;          /**< Last ordered transaction */

    /* Timestamp */
    uint64_t computed_at_ns;              /**< When computed */

} mesh_system_health_t;

/* ============================================================================
 * Bridge Configuration
 * ============================================================================ */

/**
 * @brief Health bridge configuration
 */
typedef struct mesh_health_bridge_config {
    /* Heartbeat settings */
    uint64_t default_heartbeat_interval_ms; /**< Default heartbeat interval */
    uint32_t missed_heartbeat_threshold;    /**< Threshold for unhealthy */
    uint32_t dead_threshold;                /**< Threshold for dead */

    /* Health score weights */
    float weight_cpu;                     /**< Weight for CPU usage */
    float weight_memory;                  /**< Weight for memory usage */
    float weight_tx_success;              /**< Weight for transaction success */
    float weight_latency;                 /**< Weight for latency */
    float weight_errors;                  /**< Weight for errors */

    /* Thresholds */
    float degraded_threshold;             /**< Score below = degraded */
    float unhealthy_threshold;            /**< Score below = unhealthy */
    float critical_threshold;             /**< Score below = critical */

    /* Routing */
    bool route_heartbeats_through_mesh;   /**< Route via mesh transactions */
    bool enable_consensus_health;         /**< Enable consensus on health */

    /* Logging */
    bool verbose_logging;

} mesh_health_bridge_config_t;

/**
 * @brief Bridge statistics
 */
typedef struct mesh_health_bridge_stats {
    uint64_t heartbeats_received;         /**< Total heartbeats received */
    uint64_t heartbeats_routed;           /**< Heartbeats routed through mesh */
    uint64_t health_checks_performed;     /**< Health checks performed */
    uint64_t status_changes;              /**< Status changes detected */
    uint64_t dead_detections;             /**< Dead participant detections */
    uint64_t recovery_detections;         /**< Recovery detections */

    /* Per-status counts */
    size_t current_healthy;
    size_t current_degraded;
    size_t current_unhealthy;
    size_t current_critical;
    size_t current_dead;

} mesh_health_bridge_stats_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default bridge configuration
 *
 * @param config Output configuration
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_health_bridge_default_config(
    mesh_health_bridge_config_t* config
);

/**
 * @brief Create health bridge
 *
 * @param bootstrap Mesh bootstrap handle
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
mesh_health_bridge_t* mesh_health_bridge_create(
    mesh_bootstrap_t* bootstrap,
    const mesh_health_bridge_config_t* config
);

/**
 * @brief Destroy health bridge
 *
 * @param bridge Bridge to destroy (NULL-safe)
 */
void mesh_health_bridge_destroy(mesh_health_bridge_t* bridge);

/* ============================================================================
 * Registration API
 * ============================================================================ */

/**
 * @brief Register a health agent for a participant
 *
 * @param bridge Health bridge handle
 * @param participant_id Participant ID
 * @param agent Health agent handle
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_health_bridge_register_agent(
    mesh_health_bridge_t* bridge,
    mesh_participant_id_t participant_id,
    health_agent_t* agent
);

/**
 * @brief Unregister a health agent
 *
 * @param bridge Health bridge handle
 * @param participant_id Participant ID to unregister
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_health_bridge_unregister_agent(
    mesh_health_bridge_t* bridge,
    mesh_participant_id_t participant_id
);

/* ============================================================================
 * Heartbeat API
 * ============================================================================ */

/**
 * @brief Send heartbeat for a participant
 *
 * @param bridge Health bridge handle
 * @param participant_id Participant ID
 * @param op Heartbeat operation type
 * @param progress Progress value [0-100] for PROGRESS ops
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_health_bridge_heartbeat(
    mesh_health_bridge_t* bridge,
    mesh_participant_id_t participant_id,
    mesh_heartbeat_op_t op,
    uint8_t progress
);

/**
 * @brief Update metrics for a participant
 *
 * @param bridge Health bridge handle
 * @param participant_id Participant ID
 * @param metrics Health metrics to update
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_health_bridge_update_metrics(
    mesh_health_bridge_t* bridge,
    mesh_participant_id_t participant_id,
    const health_metrics_t* metrics
);

/**
 * @brief Check for missed heartbeats
 *
 * Should be called periodically to detect dead participants.
 *
 * @param bridge Health bridge handle
 * @return Number of participants newly marked as dead
 */
size_t mesh_health_bridge_check_heartbeats(mesh_health_bridge_t* bridge);

/* ============================================================================
 * Health Query API
 * ============================================================================ */

/**
 * @brief Get health record for a participant
 *
 * @param bridge Health bridge handle
 * @param participant_id Participant ID
 * @param record_out Output health record
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_health_bridge_get_health(
    const mesh_health_bridge_t* bridge,
    mesh_participant_id_t participant_id,
    mesh_health_record_t* record_out
);

/**
 * @brief Get channel health summary
 *
 * @param bridge Health bridge handle
 * @param channel_id Channel ID
 * @param health_out Output channel health
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_health_bridge_get_channel_health(
    const mesh_health_bridge_t* bridge,
    mesh_channel_id_t channel_id,
    mesh_channel_health_t* health_out
);

/**
 * @brief Get system-wide health summary
 *
 * @param bridge Health bridge handle
 * @param health_out Output system health
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_health_bridge_get_system_health(
    const mesh_health_bridge_t* bridge,
    mesh_system_health_t* health_out
);

/**
 * @brief Check if participant is healthy
 *
 * @param bridge Health bridge handle
 * @param participant_id Participant ID
 * @return true if healthy or degraded
 */
bool mesh_health_bridge_is_healthy(
    const mesh_health_bridge_t* bridge,
    mesh_participant_id_t participant_id
);

/**
 * @brief Check if channel is healthy
 *
 * @param bridge Health bridge handle
 * @param channel_id Channel ID
 * @return true if majority of participants healthy
 */
bool mesh_health_bridge_is_channel_healthy(
    const mesh_health_bridge_t* bridge,
    mesh_channel_id_t channel_id
);

/* ============================================================================
 * Statistics API
 * ============================================================================ */

/**
 * @brief Get bridge statistics
 *
 * @param bridge Health bridge handle
 * @param stats Output statistics
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_health_bridge_get_stats(
    const mesh_health_bridge_t* bridge,
    mesh_health_bridge_stats_t* stats
);

/**
 * @brief Reset bridge statistics
 *
 * @param bridge Health bridge handle
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_health_bridge_reset_stats(
    mesh_health_bridge_t* bridge
);

/* ============================================================================
 * Bootstrap Integration
 * ============================================================================ */

/**
 * @brief Get health bridge from bootstrap
 *
 * @param bootstrap Mesh bootstrap handle
 * @return Health bridge or NULL
 */
mesh_health_bridge_t* mesh_bootstrap_get_health_bridge(
    mesh_bootstrap_t* bootstrap
);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Convert health status to string
 *
 * @param status Health status
 * @return Status string
 */
const char* mesh_health_status_to_string(mesh_health_status_t status);

/**
 * @brief Convert heartbeat op to string
 *
 * @param op Heartbeat operation
 * @return Operation string
 */
const char* mesh_heartbeat_op_to_string(mesh_heartbeat_op_t op);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MESH_HEALTH_BRIDGE_H */
