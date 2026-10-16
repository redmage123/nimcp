//=============================================================================
// nimcp_portia_collective_bridge.h - Portia-Collective Cognition Integration
//=============================================================================
/**
 * @file nimcp_portia_collective_bridge.h
 * @brief Bidirectional integration between Portia resource management and
 *        Collective Cognition distributed consciousness system
 *
 * WHAT: Integration layer connecting Portia tier management with collective
 *       cognition for distributed resource optimization
 * WHY:  Enable resource-aware distributed cognition across multiple brain
 *       instances with coordinated tier management and load balancing
 * HOW:  Share resource state across collective, coordinate degradation,
 *       enable collective load balancing based on individual tier states
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * COLLECTIVE RESOURCE MANAGEMENT:
 * --------------------------------
 * 1. Social Brain Hypothesis:
 *    - Groups coordinate resource usage collectively
 *    - Leaders may sacrifice resources for group benefit
 *    - Weak members receive support from group
 *
 * 2. Distributed Load Balancing:
 *    - High-resource instances take on more computation
 *    - Low-resource instances offload to others
 *    - Collective maintains minimum viable processing
 *
 * 3. Collective Degradation:
 *    - When one instance degrades, others compensate
 *    - Critical functions maintain priority across collective
 *    - Graceful collective-wide tier reduction under pressure
 *
 * PORTIA SPIDER ANALOGY:
 * ----------------------
 * Like Portia fimbriata hunting in groups:
 * - Scouts (constrained tier) gather information
 * - Attackers (full tier) perform complex tasks
 * - Collective adapts strategy based on available resources
 *
 * ARCHITECTURE:
 * ```
 * +==========================================================================+
 * |                   PORTIA-COLLECTIVE COGNITION BRIDGE                      |
 * +==========================================================================+
 * |                                                                           |
 * |   +---------------------------+      +-------------------------------+   |
 * |   |   PORTIA TIER SWITCH      |      |    COLLECTIVE COGNITION       |   |
 * |   |                           |      |                               |   |
 * |   |  - Local tier state       |<---->|  - Hyperscanning sync         |   |
 * |   |  - Power/thermal state    |      |  - Extended mind capacity     |   |
 * |   |  - Degradation events     |      |  - Collective phi             |   |
 * |   +---------------------------+      +-------------------------------+   |
 * |                 |                                   |                     |
 * |                 v                                   v                     |
 * |   +---------------------------+      +-------------------------------+   |
 * |   |   COLLECTIVE TIER STATE   |      |   DISTRIBUTED LOAD BALANCER   |   |
 * |   |                           |      |                               |   |
 * |   |  Instance 1: FULL         |      |  - Offload heavy tasks        |   |
 * |   |  Instance 2: MEDIUM       |----->|  - Coordinate degradation     |   |
 * |   |  Instance 3: CONSTRAINED  |      |  - Balance collective load    |   |
 * |   +---------------------------+      +-------------------------------+   |
 * |                                                                           |
 * +==========================================================================+
 * ```
 *
 * @author NIMCP Development Team
 * @date 2025-12-31
 * @version 1.0.0
 */

#ifndef NIMCP_PORTIA_COLLECTIVE_BRIDGE_H
#define NIMCP_PORTIA_COLLECTIVE_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations
//=============================================================================

typedef struct portia_collective_bridge portia_collective_bridge_t;
typedef struct portia_tier_switch_struct* portia_tier_switch_t;
typedef struct collective_cognition collective_cognition_t;

//=============================================================================
// Constants
//=============================================================================

/** Bio-async module ID */
#define BIO_MODULE_PORTIA_COLLECTIVE 0x2E10

/** Maximum instances tracked */
#define PORTIA_COLLECTIVE_MAX_INSTANCES 16

/** Default update interval */
#define PORTIA_COLLECTIVE_DEFAULT_UPDATE_MS 100

/** Offload thresholds */
#define PORTIA_COLLECTIVE_OFFLOAD_THRESHOLD 0.8f   /**< Load > 80% triggers offload */
#define PORTIA_COLLECTIVE_RECEIVE_THRESHOLD 0.4f   /**< Load < 40% can receive */

//=============================================================================
// Types
//=============================================================================

/**
 * @brief Resource sharing strategy for collective
 */
typedef enum {
    COLLECTIVE_RESOURCE_ISOLATED = 0,   /**< No resource sharing */
    COLLECTIVE_RESOURCE_COOPERATIVE,    /**< Cooperative sharing on request */
    COLLECTIVE_RESOURCE_PROACTIVE,      /**< Proactive load balancing */
    COLLECTIVE_RESOURCE_LEADER_DRIVEN   /**< Leader coordinates allocation */
} collective_resource_strategy_t;

/**
 * @brief Degradation coordination mode
 */
typedef enum {
    DEGRADATION_COORD_NONE = 0,         /**< Independent degradation */
    DEGRADATION_COORD_SYNCHRONIZED,     /**< All degrade together */
    DEGRADATION_COORD_CASCADING,        /**< Staged degradation */
    DEGRADATION_COORD_COMPENSATING      /**< Others compensate for degraded */
} degradation_coordination_t;

/**
 * @brief Per-instance resource state in collective
 */
typedef struct {
    uint32_t instance_id;               /**< Instance identifier */
    uint32_t tier;                      /**< Current platform tier (0-3) */
    float load_factor;                  /**< Current load (0.0-1.0) */
    float power_level;                  /**< Battery/power level (0.0-1.0) */
    float thermal_headroom;             /**< Thermal margin (0.0-1.0) */
    bool is_degraded;                   /**< Currently in degraded mode */
    bool can_receive_tasks;             /**< Able to accept offloaded tasks */
    bool is_leader;                     /**< Is collective leader */
    uint64_t last_update_ms;            /**< Last state update timestamp */
} collective_instance_state_t;

/**
 * @brief Collective resource summary
 */
typedef struct {
    uint32_t total_instances;           /**< Total connected instances */
    uint32_t instances_full_tier;       /**< Instances at full tier */
    uint32_t instances_degraded;        /**< Instances degraded */
    float average_load;                 /**< Average load across collective */
    float average_tier;                 /**< Average tier (0.0-3.0) */
    float collective_capacity;          /**< Total available capacity */
    float collective_utilization;       /**< Overall utilization */
    uint32_t leader_instance_id;        /**< Current leader */
    bool collective_stressed;           /**< Collective under resource pressure */
} collective_resource_summary_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Strategy settings */
    collective_resource_strategy_t resource_strategy;
    degradation_coordination_t degradation_mode;

    /* Thresholds */
    float offload_threshold;            /**< Load threshold to offload */
    float receive_threshold;            /**< Load threshold to receive */
    float degradation_threshold;        /**< Collective stress for degradation */

    /* Update settings */
    uint32_t update_interval_ms;        /**< State broadcast interval */
    uint32_t state_timeout_ms;          /**< Instance state timeout */

    /* Feature flags */
    bool enable_bio_async;              /**< Enable bio-async messaging */
    bool enable_proactive_offload;      /**< Proactive task offloading */
    bool enable_leader_election;        /**< Automatic leader election */
    bool broadcast_tier_changes;        /**< Broadcast tier changes to collective */
} portia_collective_config_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t tier_broadcasts;           /**< Tier changes broadcast */
    uint64_t tier_events_received;      /**< Tier events from collective */
    uint64_t tasks_offloaded;           /**< Tasks offloaded to others */
    uint64_t tasks_received;            /**< Tasks received from others */
    uint64_t degradation_events;        /**< Degradation coordination events */
    uint64_t leader_changes;            /**< Leader election changes */
    float avg_collective_load;          /**< Average collective load */
    float avg_response_time_ms;         /**< Average coordination latency */
} portia_collective_stats_t;

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Get default configuration
 * @param config Output configuration structure
 */
void portia_collective_default_config(portia_collective_config_t* config);

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create portia-collective bridge
 *
 * @param config Configuration (NULL for defaults)
 * @param portia Portia tier switch system (optional)
 * @param collective Collective cognition system (optional)
 * @return Bridge handle or NULL on failure
 */
portia_collective_bridge_t* portia_collective_create(
    const portia_collective_config_t* config,
    portia_tier_switch_t portia,
    collective_cognition_t* collective
);

/**
 * @brief Destroy bridge
 * @param bridge Bridge to destroy
 */
void portia_collective_destroy(portia_collective_bridge_t* bridge);

/**
 * @brief Reset bridge state
 * @param bridge Bridge to reset
 * @return 0 on success, -1 on error
 */
int portia_collective_reset(portia_collective_bridge_t* bridge);

//=============================================================================
// Connection API
//=============================================================================

/**
 * @brief Connect to Portia tier switch
 *
 * @param bridge Bridge to connect
 * @param portia Portia tier switch system
 * @return 0 on success, -1 on error
 */
int portia_collective_connect_portia(
    portia_collective_bridge_t* bridge,
    portia_tier_switch_t portia
);

/**
 * @brief Connect to collective cognition
 *
 * @param bridge Bridge to connect
 * @param collective Collective cognition system
 * @return 0 on success, -1 on error
 */
int portia_collective_connect_collective(
    portia_collective_bridge_t* bridge,
    collective_cognition_t* collective
);

//=============================================================================
// Update API
//=============================================================================

/**
 * @brief Main update cycle
 *
 * Broadcasts local state, receives collective state, coordinates resources.
 *
 * @param bridge Bridge to update
 * @param delta_ms Time since last update
 * @return 0 on success, -1 on error
 */
int portia_collective_update(
    portia_collective_bridge_t* bridge,
    uint64_t delta_ms
);

/**
 * @brief Broadcast local tier change to collective
 *
 * @param bridge Bridge to use
 * @param new_tier New tier level (0-3)
 * @return 0 on success, -1 on error
 */
int portia_collective_broadcast_tier(
    portia_collective_bridge_t* bridge,
    uint32_t new_tier
);

/**
 * @brief Handle tier change from remote instance
 *
 * @param bridge Bridge to update
 * @param instance_id Remote instance ID
 * @param new_tier New tier level
 * @return 0 on success, -1 on error
 */
int portia_collective_handle_remote_tier(
    portia_collective_bridge_t* bridge,
    uint32_t instance_id,
    uint32_t new_tier
);

//=============================================================================
// Load Balancing API
//=============================================================================

/**
 * @brief Request task offload to collective
 *
 * @param bridge Bridge to use
 * @param task_complexity Complexity score (0.0-1.0)
 * @param target_instance Output: recommended target instance
 * @return 0 on success (target found), -1 if no suitable target
 */
int portia_collective_request_offload(
    portia_collective_bridge_t* bridge,
    float task_complexity,
    uint32_t* target_instance
);

/**
 * @brief Check if local instance can receive tasks
 *
 * @param bridge Bridge to query
 * @return true if can receive, false otherwise
 */
bool portia_collective_can_receive(const portia_collective_bridge_t* bridge);

/**
 * @brief Trigger collective load rebalancing
 *
 * @param bridge Bridge to use
 * @return Number of tasks redistributed, -1 on error
 */
int portia_collective_rebalance(portia_collective_bridge_t* bridge);

//=============================================================================
// Degradation API
//=============================================================================

/**
 * @brief Coordinate degradation with collective
 *
 * @param bridge Bridge to use
 * @param local_degraded Whether local instance is degraded
 * @return 0 on success, -1 on error
 */
int portia_collective_coordinate_degradation(
    portia_collective_bridge_t* bridge,
    bool local_degraded
);

/**
 * @brief Request compensation from collective
 *
 * When degraded, request other instances to take over critical functions.
 *
 * @param bridge Bridge to use
 * @return 0 on success, -1 on error
 */
int portia_collective_request_compensation(
    portia_collective_bridge_t* bridge
);

//=============================================================================
// Query API
//=============================================================================

/**
 * @brief Get collective resource summary
 *
 * @param bridge Bridge to query
 * @param summary Output summary structure
 * @return 0 on success, -1 on error
 */
int portia_collective_get_summary(
    const portia_collective_bridge_t* bridge,
    collective_resource_summary_t* summary
);

/**
 * @brief Get instance state
 *
 * @param bridge Bridge to query
 * @param instance_id Instance to query
 * @param state Output state structure
 * @return 0 on success, -1 if instance not found
 */
int portia_collective_get_instance_state(
    const portia_collective_bridge_t* bridge,
    uint32_t instance_id,
    collective_instance_state_t* state
);

/**
 * @brief Get local instance ID
 *
 * @param bridge Bridge to query
 * @return Local instance ID
 */
uint32_t portia_collective_get_local_id(const portia_collective_bridge_t* bridge);

/**
 * @brief Check if local instance is leader
 *
 * @param bridge Bridge to query
 * @return true if leader
 */
bool portia_collective_is_leader(const portia_collective_bridge_t* bridge);

//=============================================================================
// Statistics API
//=============================================================================

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge to query
 * @param stats Output statistics structure
 * @return 0 on success, -1 on error
 */
int portia_collective_get_stats(
    const portia_collective_bridge_t* bridge,
    portia_collective_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param bridge Bridge to reset
 */
void portia_collective_reset_stats(portia_collective_bridge_t* bridge);

//=============================================================================
// Bio-Async API
//=============================================================================

/**
 * @brief Connect to bio-async router
 *
 * @param bridge Bridge to connect
 * @return 0 on success, -1 on error
 */
int portia_collective_connect_bio_async(portia_collective_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * @param bridge Bridge to disconnect
 * @return 0 on success, -1 on error
 */
int portia_collective_disconnect_bio_async(portia_collective_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PORTIA_COLLECTIVE_BRIDGE_H */
