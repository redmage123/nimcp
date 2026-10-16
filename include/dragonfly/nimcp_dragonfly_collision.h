/**
 * @file nimcp_dragonfly_collision.h
 * @brief Collision Avoidance During Pursuit
 *
 * BIOLOGICAL REFERENCE:
 * Dragonflies must avoid obstacles while pursuing prey at high speed.
 * They use peripheral vision and rapid reflexes to navigate cluttered
 * environments like reed beds near water while maintaining pursuit.
 *
 * WHAT: Detects obstacles and plans avoidance maneuvers
 * WHY:  Enables safe hunting in cluttered environments
 * HOW:  Obstacle detection with trajectory replanning
 *
 * @author NIMCP Team
 * @date 2024-12-29
 */

#ifndef NIMCP_DRAGONFLY_COLLISION_H
#define NIMCP_DRAGONFLY_COLLISION_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

//=============================================================================
// Forward Declarations
//=============================================================================

typedef struct dragonfly_collision_s* dragonfly_collision_t;

//=============================================================================
// Constants
//=============================================================================

#define COLLISION_MAX_OBSTACLES 32    /**< Maximum tracked obstacles */
#define COLLISION_SECTORS 8           /**< Directional sectors */

//=============================================================================
// Type Definitions
//=============================================================================

/**
 * @brief Obstacle type
 */
typedef enum {
    OBSTACLE_UNKNOWN,         /**< Unknown obstacle */
    OBSTACLE_VEGETATION,      /**< Vegetation (branches, reeds) */
    OBSTACLE_STRUCTURE,       /**< Solid structure */
    OBSTACLE_WATER_SURFACE,   /**< Water surface below */
    OBSTACLE_PREDATOR,        /**< Moving predator */
    OBSTACLE_COMPETITOR       /**< Other dragonfly */
} obstacle_type_t;

/**
 * @brief Threat level
 */
typedef enum {
    THREAT_NONE,              /**< No threat */
    THREAT_LOW,               /**< Minor obstruction */
    THREAT_MEDIUM,            /**< Significant obstruction */
    THREAT_HIGH,              /**< Imminent collision */
    THREAT_CRITICAL           /**< Collision unavoidable without action */
} threat_level_t;

/**
 * @brief Avoidance action
 */
typedef enum {
    AVOID_NONE,               /**< No avoidance needed */
    AVOID_SLIGHT_LEFT,        /**< Slight left turn */
    AVOID_SLIGHT_RIGHT,       /**< Slight right turn */
    AVOID_HARD_LEFT,          /**< Hard left turn */
    AVOID_HARD_RIGHT,         /**< Hard right turn */
    AVOID_CLIMB,              /**< Climb over */
    AVOID_DIVE,               /**< Dive under */
    AVOID_BRAKE,              /**< Reduce speed */
    AVOID_ABORT               /**< Abort pursuit entirely */
} avoidance_action_t;

/**
 * @brief Detected obstacle
 */
typedef struct {
    uint32_t id;                  /**< Obstacle ID */
    obstacle_type_t type;         /**< Obstacle type */

    /* Position */
    float position[3];            /**< Center position */
    float extent[3];              /**< Bounding box extent */
    float distance_m;             /**< Distance to obstacle */

    /* Motion (if moving) */
    float velocity[3];            /**< Obstacle velocity */
    bool is_moving;               /**< Is obstacle moving */

    /* Threat assessment */
    threat_level_t threat;        /**< Threat level */
    float time_to_collision_s;    /**< TTC estimate */
    float closest_approach_m;     /**< Predicted closest approach */

    /* Temporal */
    uint64_t first_seen_us;       /**< First detection */
    uint64_t last_seen_us;        /**< Last update */
} detected_obstacle_t;

/**
 * @brief Collision threat summary
 */
typedef struct {
    /* Overall threat */
    threat_level_t max_threat;        /**< Maximum threat level */
    uint32_t obstacle_count;          /**< Number of obstacles */
    float min_ttc_s;                  /**< Minimum time to collision */

    /* Directional threats */
    threat_level_t sector_threat[COLLISION_SECTORS]; /**< Per-sector threat */
    float sector_clearance[COLLISION_SECTORS];       /**< Per-sector clearance */

    /* Primary threat */
    bool has_primary_threat;          /**< Primary threat identified */
    uint32_t primary_obstacle_id;     /**< Primary threat obstacle */
    float primary_ttc_s;              /**< Primary threat TTC */

    /* Path status */
    bool path_clear;                  /**< Current path is clear */
    float safe_path_fraction;         /**< Fraction of path that's safe */
} collision_summary_t;

/**
 * @brief Avoidance command
 */
typedef struct {
    /* Recommended action */
    avoidance_action_t action;        /**< Recommended action */
    float urgency;                    /**< Action urgency [0,1] */

    /* Trajectory modification */
    float heading_offset_rad;         /**< Heading adjustment */
    float pitch_offset_rad;           /**< Pitch adjustment */
    float speed_factor;               /**< Speed multiplier */

    /* Alternative path */
    float safe_direction[3];          /**< Safe direction vector */
    bool pursuit_compatible;          /**< Can maintain pursuit */

    /* Timing */
    float react_deadline_ms;          /**< Time to react */
    bool requires_immediate_action;   /**< Immediate action needed */
} avoidance_command_t;

/**
 * @brief Collision avoidance configuration
 */
typedef struct {
    /* Detection */
    float detection_range_m;          /**< Maximum detection range */
    float detection_cone_rad;         /**< Detection cone half-angle */
    float peripheral_range_m;         /**< Peripheral detection range */

    /* Safety margins */
    float min_clearance_m;            /**< Minimum clearance */
    float ttc_warning_threshold_s;    /**< TTC for warning */
    float ttc_critical_threshold_s;   /**< TTC for critical */

    /* Response parameters */
    float max_avoidance_angle_rad;    /**< Maximum avoidance turn */
    float avoidance_aggression;       /**< Avoidance aggressiveness [0,1] */
    bool allow_pursuit_abort;         /**< Can recommend pursuit abort */

    /* Prediction */
    float prediction_horizon_s;       /**< Prediction time horizon */
    bool use_obstacle_velocity;       /**< Account for moving obstacles */

    /* Priorities */
    float pursuit_vs_safety;          /**< Pursuit vs safety balance [0,1] */
} collision_config_t;

/**
 * @brief Collision system statistics
 */
typedef struct {
    uint64_t obstacles_detected;      /**< Total obstacles detected */
    uint64_t avoidance_commands;      /**< Avoidance commands issued */
    uint64_t critical_threats;        /**< Critical threats encountered */
    uint64_t pursuits_aborted;        /**< Pursuits aborted for safety */
    float avg_min_clearance_m;        /**< Average minimum clearance */
    uint32_t near_misses;             /**< Very close calls */
} collision_stats_t;

//=============================================================================
// Configuration Functions
//=============================================================================

/**
 * @brief Get default collision configuration
 */
collision_config_t collision_default_config(void);

/**
 * @brief Validate collision configuration
 */
bool collision_validate_config(const collision_config_t* config);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create collision avoidance system
 */
dragonfly_collision_t dragonfly_collision_create(const collision_config_t* config);

/**
 * @brief Destroy collision avoidance system
 */
void dragonfly_collision_destroy(dragonfly_collision_t collision);

/**
 * @brief Reset collision avoidance system
 */
int dragonfly_collision_reset(dragonfly_collision_t collision);

//=============================================================================
// Detection Functions
//=============================================================================

/**
 * @brief Add/update obstacle detection
 */
int dragonfly_collision_add_obstacle(
    dragonfly_collision_t collision,
    const detected_obstacle_t* obstacle
);

/**
 * @brief Remove obstacle
 */
int dragonfly_collision_remove_obstacle(
    dragonfly_collision_t collision,
    uint32_t obstacle_id
);

/**
 * @brief Clear all obstacles
 */
int dragonfly_collision_clear(dragonfly_collision_t collision);

/**
 * @brief Update with depth map
 */
int dragonfly_collision_update_depth(
    dragonfly_collision_t collision,
    const float* depth_map,
    uint32_t width,
    uint32_t height,
    float fov_rad
);

//=============================================================================
// Analysis Functions
//=============================================================================

/**
 * @brief Analyze current collision threats
 */
int dragonfly_collision_analyze(
    dragonfly_collision_t collision,
    const float self_position[3],
    const float self_velocity[3],
    collision_summary_t* summary
);

/**
 * @brief Check if path is clear
 */
bool dragonfly_collision_path_clear(
    dragonfly_collision_t collision,
    const float start[3],
    const float end[3],
    float* min_clearance
);

/**
 * @brief Get avoidance command
 */
int dragonfly_collision_get_avoidance(
    dragonfly_collision_t collision,
    const float self_position[3],
    const float self_velocity[3],
    const float pursuit_direction[3],
    avoidance_command_t* command
);

/**
 * @brief Find safest direction
 */
int dragonfly_collision_find_safe_direction(
    dragonfly_collision_t collision,
    const float self_position[3],
    const float preferred_direction[3],
    float safe_direction[3]
);

//=============================================================================
// Query Functions
//=============================================================================

/**
 * @brief Get detected obstacles
 */
int dragonfly_collision_get_obstacles(
    const dragonfly_collision_t collision,
    detected_obstacle_t* obstacles,
    uint32_t max_obstacles,
    uint32_t* num_obstacles
);

/**
 * @brief Get collision statistics
 */
int dragonfly_collision_get_stats(
    const dragonfly_collision_t collision,
    collision_stats_t* stats
);

/**
 * @brief Get obstacle type name
 */
const char* dragonfly_obstacle_type_name(obstacle_type_t type);

/**
 * @brief Get threat level name
 */
const char* dragonfly_threat_level_name(threat_level_t level);

/**
 * @brief Get avoidance action name
 */
const char* dragonfly_avoidance_action_name(avoidance_action_t action);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_DRAGONFLY_COLLISION_H */
