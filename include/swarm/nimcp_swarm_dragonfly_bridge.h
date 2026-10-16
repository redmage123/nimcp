/**
 * @file nimcp_swarm_dragonfly_bridge.h
 * @brief Swarm Coordination Bridge for Dragonfly Interception
 *
 * WHAT: Connects dragonfly targeting system with drone swarm coordination
 * WHY:  Enable coordinated multi-drone pursuit and target sharing
 * HOW:  Collective workspace items + protocol messages + task scheduling
 *
 * INTEGRATION PIPELINE:
 * Local Dragonfly Detection → Collective Workspace → Swarm Coordination
 *                                                  → Multi-Drone Pursuit
 *
 * BIOLOGICAL INSPIRATION:
 * - Wolf pack hunting: Coordinated pursuit with role assignment
 * - Orca pod hunting: Target isolation and multi-directional approach
 * - Social insects: Stigmergic coordination via shared signals
 *
 * KEY FEATURES:
 * 1. TARGET SHARING: Share detected targets via collective workspace
 * 2. OPTIMAL ASSIGNMENT: Assign targets to drones via task scheduler
 * 3. COORDINATED PURSUIT: Multi-drone formations (wedge, pincer, encircle)
 * 4. TARGET HANDOFF: Transfer pursuit responsibility between drones
 *
 * USAGE:
 * @code
 *   // Create bridge on each drone
 *   swarm_dragonfly_bridge_config_t config = swarm_dragonfly_bridge_default_config();
 *   config.local_drone_id = my_drone_id;
 *   swarm_dragonfly_bridge_t* bridge = swarm_dragonfly_bridge_create(
 *       dragonfly_system, collective_workspace, &config);
 *
 *   // When local dragonfly detects target
 *   swarm_dragonfly_bridge_share_target(bridge, &target_info);
 *
 *   // Process incoming target discoveries
 *   swarm_dragonfly_bridge_process_updates(bridge);
 *
 *   // Get assignment for this drone
 *   swarm_target_assignment_t assignment;
 *   if (swarm_dragonfly_bridge_get_assignment(bridge, &assignment)) {
 *       // Pursue assigned target with formation role
 *   }
 *
 *   swarm_dragonfly_bridge_destroy(bridge);
 * @endcode
 *
 * @author NIMCP Team
 * @date 2024-12-29
 */

#ifndef NIMCP_SWARM_DRAGONFLY_BRIDGE_H
#define NIMCP_SWARM_DRAGONFLY_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "dragonfly/nimcp_dragonfly.h"
#include "dragonfly/nimcp_dragonfly_swarm.h"
#include "swarm/nimcp_collective_workspace.h"
#include "swarm/nimcp_swarm_protocol.h"

//=============================================================================
// Forward Declarations
//=============================================================================

typedef struct swarm_dragonfly_bridge_s swarm_dragonfly_bridge_t;
typedef struct swarm_task_scheduler swarm_task_scheduler_t;

//=============================================================================
// Constants
//=============================================================================

#define SWARM_DRAGONFLY_MAX_SHARED_TARGETS 16    /**< Max targets in collective workspace */
#define SWARM_DRAGONFLY_MAX_PURSUERS 4           /**< Max drones per target */
#define SWARM_DRAGONFLY_MAX_FORMATIONS 8         /**< Max active formations */

//=============================================================================
// Type Definitions
//=============================================================================

/**
 * @brief Pursuit formation type
 *
 * Different formations for coordinated multi-drone pursuit.
 */
typedef enum {
    PURSUIT_FORMATION_NONE = 0,       /**< No formation (solo pursuit) */
    PURSUIT_FORMATION_WEDGE,          /**< V-formation pursuit */
    PURSUIT_FORMATION_PINCER,         /**< Two-pronged approach */
    PURSUIT_FORMATION_ENCIRCLE,       /**< Surround target */
    PURSUIT_FORMATION_RELAY,          /**< Sequential pursuit handoffs */
    PURSUIT_FORMATION_SHADOW          /**< One pursues, others shadow */
} pursuit_formation_t;

/**
 * @brief Drone role in formation
 */
typedef enum {
    PURSUIT_ROLE_NONE = 0,            /**< Not assigned */
    PURSUIT_ROLE_LEAD,                /**< Primary pursuer */
    PURSUIT_ROLE_FLANK_LEFT,          /**< Left flanking position */
    PURSUIT_ROLE_FLANK_RIGHT,         /**< Right flanking position */
    PURSUIT_ROLE_BACKUP,              /**< Backup/trailing pursuer */
    PURSUIT_ROLE_BLOCKER,             /**< Cut off escape route */
    PURSUIT_ROLE_OBSERVER             /**< Observe and relay */
} pursuit_role_t;

/**
 * @brief Target status in swarm coordination
 */
typedef enum {
    TARGET_STATUS_DETECTED = 0,       /**< Initially detected */
    TARGET_STATUS_SHARED,             /**< Shared with swarm */
    TARGET_STATUS_ASSIGNED,           /**< Assigned to pursuer(s) */
    TARGET_STATUS_PURSUING,           /**< Active pursuit */
    TARGET_STATUS_INTERCEPTED,        /**< Interception in progress */
    TARGET_STATUS_LOST,               /**< Lost track */
    TARGET_STATUS_ABANDONED           /**< Pursuit abandoned */
} swarm_target_status_t;

/**
 * @brief Shared target information
 *
 * Information broadcast to swarm about detected target.
 */
typedef struct {
    /* Target identification */
    uint32_t target_id;               /**< Unique target ID (drone_id << 16 | local_id) */
    uint16_t detecting_drone;         /**< Drone that detected target */

    /* Target state */
    float position[3];                /**< Current position (m) */
    float velocity[3];                /**< Current velocity (m/s) */
    float predicted_position[3];      /**< Predicted position at intercept */

    /* Target characteristics */
    float size_estimate;              /**< Estimated size (m) */
    float intercept_difficulty;       /**< Difficulty [0,1] (based on speed, evasion) */
    float priority;                   /**< Priority [0,1] (from dragonfly system) */

    /* Swarm context */
    swarm_position_t swarm_position;  /**< Position in prey swarm (if applicable) */
    uint32_t prey_cluster_id;         /**< Prey cluster ID (if in swarm) */

    /* Timing */
    uint64_t detection_time_us;       /**< When first detected */
    uint64_t update_time_us;          /**< Last update time */
    float staleness_s;                /**< Time since last update */

    /* Pursuit status */
    swarm_target_status_t status;     /**< Current status */
    uint16_t assigned_pursuers[SWARM_DRAGONFLY_MAX_PURSUERS];  /**< Assigned drone IDs */
    uint8_t num_pursuers;             /**< Number of assigned pursuers */
    pursuit_formation_t formation;    /**< Active formation */
} shared_target_t;

/**
 * @brief Target assignment for this drone
 */
typedef struct {
    /* Assigned target */
    uint32_t target_id;               /**< Target to pursue */
    shared_target_t target;           /**< Target details */

    /* Role assignment */
    pursuit_role_t role;              /**< This drone's role */
    pursuit_formation_t formation;    /**< Formation type */

    /* Formation position */
    float formation_offset[3];        /**< Offset from target for this role */
    float approach_angle;             /**< Approach angle (radians) */

    /* Coordination */
    uint16_t lead_drone;              /**< Lead drone ID */
    uint16_t partner_drones[3];       /**< Partner drone IDs */
    uint8_t num_partners;             /**< Number of partners */

    /* Timing */
    float estimated_intercept_time_s; /**< Estimated time to intercept */
    uint64_t assigned_time_us;        /**< When assigned */

    /* Priority */
    float urgency;                    /**< Urgency [0,1] */
    bool is_primary;                  /**< Is this the primary pursuer? */
} swarm_target_assignment_t;

/**
 * @brief Formation coordination state
 */
typedef struct {
    /* Formation */
    pursuit_formation_t type;         /**< Formation type */
    uint32_t target_id;               /**< Target being pursued */

    /* Participants */
    uint16_t drone_ids[SWARM_DRAGONFLY_MAX_PURSUERS];  /**< Participating drones */
    pursuit_role_t roles[SWARM_DRAGONFLY_MAX_PURSUERS]; /**< Roles for each */
    uint8_t num_drones;               /**< Number of drones */
    uint16_t lead_drone;              /**< Lead drone ID */

    /* Geometry */
    float formation_center[3];        /**< Formation center position */
    float formation_heading;          /**< Formation heading (radians) */
    float spread;                     /**< Formation spread (m) */

    /* Status */
    float coherence;                  /**< Formation coherence [0,1] */
    bool is_active;                   /**< Formation active? */
    uint64_t start_time_us;           /**< Formation start time */
} formation_state_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Identity */
    uint16_t local_drone_id;          /**< This drone's ID */
    uint16_t swarm_size;              /**< Total drones in swarm */

    /* Sharing policy */
    float share_threshold;            /**< Min priority to share target [0,1] */
    float salience_boost;             /**< Salience boost for targets [0,1] */
    bool share_all_targets;           /**< Share all vs high-priority only */

    /* Assignment policy */
    float max_pursuit_distance_m;     /**< Max distance to accept target */
    float min_intercept_probability;  /**< Min intercept probability to accept */
    uint32_t max_simultaneous_pursuits; /**< Max targets this drone pursues */

    /* Formation preferences */
    pursuit_formation_t preferred_formation; /**< Preferred formation type */
    float formation_spread_m;         /**< Formation spread distance */
    bool enable_coordinated_pursuit;  /**< Enable multi-drone formations */

    /* Handoff settings */
    float handoff_threshold_s;        /**< Time-to-boundary for handoff */
    float handoff_overlap_s;          /**< Handoff overlap period */

    /* Update rates */
    float share_update_interval_ms;   /**< Target update broadcast interval */
    float assignment_interval_ms;     /**< Re-assignment evaluation interval */

    /* Bio-async integration */
    bool use_bio_async;               /**< Use bio-async messaging */
    float broadcast_urgency;          /**< Default urgency for broadcasts */
} swarm_dragonfly_bridge_config_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    /* Targets */
    uint64_t targets_shared;          /**< Targets shared to swarm */
    uint64_t targets_received;        /**< Targets received from swarm */
    uint64_t targets_assigned;        /**< Targets assigned to this drone */
    uint64_t targets_intercepted;     /**< Successful interceptions */

    /* Coordination */
    uint64_t formations_joined;       /**< Formations participated in */
    uint64_t handoffs_sent;           /**< Handoffs sent to other drones */
    uint64_t handoffs_received;       /**< Handoffs received from others */

    /* Performance */
    float avg_interception_time_s;    /**< Average time from share to intercept */
    float formation_success_rate;     /**< Formation hunt success rate */
    float coordination_efficiency;    /**< Efficiency vs solo hunting */

    /* Errors */
    uint64_t assignment_conflicts;    /**< Assignment conflicts resolved */
    uint64_t lost_targets;            /**< Targets lost during coordination */
} swarm_dragonfly_bridge_stats_t;

//=============================================================================
// Configuration Functions
//=============================================================================

/**
 * @brief Get default bridge configuration
 */
swarm_dragonfly_bridge_config_t swarm_dragonfly_bridge_default_config(void);

/**
 * @brief Validate bridge configuration
 */
bool swarm_dragonfly_bridge_validate_config(const swarm_dragonfly_bridge_config_t* config);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create swarm-dragonfly bridge
 *
 * @param dragonfly Local dragonfly system
 * @param workspace Collective workspace for target sharing
 * @param scheduler Task scheduler for assignment (can be NULL)
 * @param config Bridge configuration
 * @return Bridge handle or NULL on error
 */
swarm_dragonfly_bridge_t* swarm_dragonfly_bridge_create(
    dragonfly_system_t* dragonfly,
    collective_workspace_t* workspace,
    swarm_task_scheduler_t* scheduler,
    const swarm_dragonfly_bridge_config_t* config
);

/**
 * @brief Destroy bridge
 */
void swarm_dragonfly_bridge_destroy(swarm_dragonfly_bridge_t* bridge);

/**
 * @brief Reset bridge state
 */
int swarm_dragonfly_bridge_reset(swarm_dragonfly_bridge_t* bridge);

//=============================================================================
// Target Sharing Functions
//=============================================================================

/**
 * @brief Share detected target with swarm
 *
 * WHAT: Broadcast target to collective workspace
 * WHY:  Enable coordinated pursuit
 * HOW:  Create workspace item with target info
 *
 * @param bridge Swarm-dragonfly bridge
 * @param target Target information
 * @return 0 on success, -1 on error
 */
int swarm_dragonfly_bridge_share_target(
    swarm_dragonfly_bridge_t* bridge,
    const shared_target_t* target
);

/**
 * @brief Share target from dragonfly tracking
 *
 * Convenience function to share target directly from dragonfly track.
 *
 * @param bridge Swarm-dragonfly bridge
 * @param track_id Dragonfly track ID
 * @param priority Target priority [0,1]
 * @return 0 on success, -1 on error
 */
int swarm_dragonfly_bridge_share_track(
    swarm_dragonfly_bridge_t* bridge,
    uint32_t track_id,
    float priority
);

/**
 * @brief Update shared target information
 *
 * @param bridge Swarm-dragonfly bridge
 * @param target_id Target to update
 * @param position New position (can be NULL to skip)
 * @param velocity New velocity (can be NULL to skip)
 * @param status New status
 * @return 0 on success, -1 on error
 */
int swarm_dragonfly_bridge_update_target(
    swarm_dragonfly_bridge_t* bridge,
    uint32_t target_id,
    const float position[3],
    const float velocity[3],
    swarm_target_status_t status
);

/**
 * @brief Remove target from swarm coordination
 *
 * @param bridge Swarm-dragonfly bridge
 * @param target_id Target to remove
 * @param reason Removal reason (intercepted, lost, abandoned)
 * @return 0 on success, -1 on error
 */
int swarm_dragonfly_bridge_remove_target(
    swarm_dragonfly_bridge_t* bridge,
    uint32_t target_id,
    swarm_target_status_t reason
);

//=============================================================================
// Target Reception Functions
//=============================================================================

/**
 * @brief Process incoming swarm updates
 *
 * WHAT: Process workspace items and protocol messages
 * WHY:  Receive target discoveries from other drones
 * HOW:  Scan collective workspace for new/updated targets
 *
 * @param bridge Swarm-dragonfly bridge
 * @return Number of updates processed, -1 on error
 */
int swarm_dragonfly_bridge_process_updates(swarm_dragonfly_bridge_t* bridge);

/**
 * @brief Get shared targets from collective workspace
 *
 * @param bridge Swarm-dragonfly bridge
 * @param targets Output array
 * @param max_targets Maximum targets to return
 * @param num_targets Output: actual count
 * @return 0 on success, -1 on error
 */
int swarm_dragonfly_bridge_get_shared_targets(
    const swarm_dragonfly_bridge_t* bridge,
    shared_target_t* targets,
    uint32_t max_targets,
    uint32_t* num_targets
);

/**
 * @brief Get specific shared target
 *
 * @param bridge Swarm-dragonfly bridge
 * @param target_id Target ID
 * @param target Output target info
 * @return 0 on success, -1 if not found
 */
int swarm_dragonfly_bridge_get_target(
    const swarm_dragonfly_bridge_t* bridge,
    uint32_t target_id,
    shared_target_t* target
);

//=============================================================================
// Target Assignment Functions
//=============================================================================

/**
 * @brief Get target assignment for this drone
 *
 * @param bridge Swarm-dragonfly bridge
 * @param assignment Output assignment
 * @return true if assigned, false if no assignment
 */
bool swarm_dragonfly_bridge_get_assignment(
    const swarm_dragonfly_bridge_t* bridge,
    swarm_target_assignment_t* assignment
);

/**
 * @brief Request assignment to target
 *
 * @param bridge Swarm-dragonfly bridge
 * @param target_id Target to pursue
 * @param urgency Request urgency [0,1]
 * @return true if assigned, false if denied
 */
bool swarm_dragonfly_bridge_request_assignment(
    swarm_dragonfly_bridge_t* bridge,
    uint32_t target_id,
    float urgency
);

/**
 * @brief Release current assignment
 *
 * @param bridge Swarm-dragonfly bridge
 * @param reason Reason for release
 * @return 0 on success, -1 on error
 */
int swarm_dragonfly_bridge_release_assignment(
    swarm_dragonfly_bridge_t* bridge,
    swarm_target_status_t reason
);

/**
 * @brief Report interception result
 *
 * @param bridge Swarm-dragonfly bridge
 * @param target_id Intercepted target
 * @param success true if successful
 * @return 0 on success, -1 on error
 */
int swarm_dragonfly_bridge_report_intercept(
    swarm_dragonfly_bridge_t* bridge,
    uint32_t target_id,
    bool success
);

//=============================================================================
// Formation Functions
//=============================================================================

/**
 * @brief Create coordinated pursuit formation
 *
 * @param bridge Swarm-dragonfly bridge
 * @param target_id Target to pursue
 * @param formation Formation type
 * @param drone_ids Participating drones (NULL = auto-select)
 * @param num_drones Number of drones
 * @return Formation ID or 0 on error
 */
uint32_t swarm_dragonfly_bridge_create_formation(
    swarm_dragonfly_bridge_t* bridge,
    uint32_t target_id,
    pursuit_formation_t formation,
    const uint16_t* drone_ids,
    uint8_t num_drones
);

/**
 * @brief Join existing formation
 *
 * @param bridge Swarm-dragonfly bridge
 * @param formation_id Formation to join
 * @param preferred_role Preferred role (PURSUIT_ROLE_NONE = any)
 * @return Assigned role, PURSUIT_ROLE_NONE if failed
 */
pursuit_role_t swarm_dragonfly_bridge_join_formation(
    swarm_dragonfly_bridge_t* bridge,
    uint32_t formation_id,
    pursuit_role_t preferred_role
);

/**
 * @brief Leave formation
 *
 * @param bridge Swarm-dragonfly bridge
 * @param formation_id Formation to leave
 * @return 0 on success, -1 on error
 */
int swarm_dragonfly_bridge_leave_formation(
    swarm_dragonfly_bridge_t* bridge,
    uint32_t formation_id
);

/**
 * @brief Get current formation state
 *
 * @param bridge Swarm-dragonfly bridge
 * @param state Output formation state
 * @return true if in formation, false otherwise
 */
bool swarm_dragonfly_bridge_get_formation(
    const swarm_dragonfly_bridge_t* bridge,
    formation_state_t* state
);

/**
 * @brief Update formation position
 *
 * Report this drone's position to formation coordination.
 *
 * @param bridge Swarm-dragonfly bridge
 * @param position This drone's position
 * @param velocity This drone's velocity
 * @return 0 on success, -1 on error
 */
int swarm_dragonfly_bridge_update_formation_position(
    swarm_dragonfly_bridge_t* bridge,
    const float position[3],
    const float velocity[3]
);

//=============================================================================
// Handoff Functions
//=============================================================================

/**
 * @brief Initiate target handoff to another drone
 *
 * @param bridge Swarm-dragonfly bridge
 * @param target_id Target to hand off
 * @param receiving_drone Drone to receive target (0 = auto-select)
 * @return 0 on success, -1 on error
 */
int swarm_dragonfly_bridge_initiate_handoff(
    swarm_dragonfly_bridge_t* bridge,
    uint32_t target_id,
    uint16_t receiving_drone
);

/**
 * @brief Accept incoming handoff
 *
 * @param bridge Swarm-dragonfly bridge
 * @param target_id Target being handed off
 * @param sending_drone Drone sending handoff
 * @return true if accepted, false if rejected
 */
bool swarm_dragonfly_bridge_accept_handoff(
    swarm_dragonfly_bridge_t* bridge,
    uint32_t target_id,
    uint16_t sending_drone
);

/**
 * @brief Reject incoming handoff
 *
 * @param bridge Swarm-dragonfly bridge
 * @param target_id Target being handed off
 * @param sending_drone Drone sending handoff
 * @return 0 on success, -1 on error
 */
int swarm_dragonfly_bridge_reject_handoff(
    swarm_dragonfly_bridge_t* bridge,
    uint32_t target_id,
    uint16_t sending_drone
);

//=============================================================================
// Query Functions
//=============================================================================

/**
 * @brief Get bridge statistics
 *
 * @param bridge Swarm-dragonfly bridge
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int swarm_dragonfly_bridge_get_stats(
    const swarm_dragonfly_bridge_t* bridge,
    swarm_dragonfly_bridge_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param bridge Swarm-dragonfly bridge
 * @return 0 on success, -1 on error
 */
int swarm_dragonfly_bridge_reset_stats(swarm_dragonfly_bridge_t* bridge);

/**
 * @brief Get count of active shared targets
 *
 * @param bridge Swarm-dragonfly bridge
 * @return Number of active targets
 */
uint32_t swarm_dragonfly_bridge_target_count(
    const swarm_dragonfly_bridge_t* bridge
);

/**
 * @brief Check if this drone is in a formation
 *
 * @param bridge Swarm-dragonfly bridge
 * @return true if in formation
 */
bool swarm_dragonfly_bridge_in_formation(
    const swarm_dragonfly_bridge_t* bridge
);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get formation type name
 */
const char* pursuit_formation_name(pursuit_formation_t formation);

/**
 * @brief Get pursuit role name
 */
const char* pursuit_role_name(pursuit_role_t role);

/**
 * @brief Get target status name
 */
const char* swarm_target_status_name(swarm_target_status_t status);

/**
 * @brief Compute optimal formation for target
 *
 * @param target Target characteristics
 * @param num_drones Available drones
 * @return Recommended formation
 */
pursuit_formation_t swarm_dragonfly_compute_formation(
    const shared_target_t* target,
    uint8_t num_drones
);

/**
 * @brief Compute formation positions
 *
 * @param formation Formation type
 * @param target_pos Target position
 * @param target_vel Target velocity
 * @param num_drones Number of drones
 * @param offsets Output: position offsets for each drone
 * @return 0 on success, -1 on error
 */
int swarm_dragonfly_compute_formation_positions(
    pursuit_formation_t formation,
    const float target_pos[3],
    const float target_vel[3],
    uint8_t num_drones,
    float offsets[][3]
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SWARM_DRAGONFLY_BRIDGE_H */
