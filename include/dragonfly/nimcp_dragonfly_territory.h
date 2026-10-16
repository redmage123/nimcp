/**
 * @file nimcp_dragonfly_territory.h
 * @brief Territorial Behavior and Patrol System
 *
 * BIOLOGICAL REFERENCE:
 * Male dragonflies establish and defend territories, typically along
 * water edges where females lay eggs. They patrol regular routes and
 * chase intruders while maintaining awareness of prey opportunities.
 *
 * WHAT: Manages territorial boundaries and patrol behavior
 * WHY:  Enables realistic dragonfly behavior patterns
 * HOW:  Spatial memory with patrol route optimization
 *
 * @author NIMCP Team
 * @date 2024-12-29
 */

#ifndef NIMCP_DRAGONFLY_TERRITORY_H
#define NIMCP_DRAGONFLY_TERRITORY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

//=============================================================================
// Forward Declarations
//=============================================================================

typedef struct dragonfly_territory_s* dragonfly_territory_t;

//=============================================================================
// Constants
//=============================================================================

#define TERRITORY_MAX_WAYPOINTS 32    /**< Maximum patrol waypoints */
#define TERRITORY_MAX_LANDMARKS 64    /**< Maximum remembered landmarks */
#define TERRITORY_MAX_INTRUDERS 8     /**< Maximum tracked intruders */

//=============================================================================
// Type Definitions
//=============================================================================

/**
 * @brief Territory activity type
 */
typedef enum {
    TERRITORY_PATROLLING,      /**< Regular patrol flight */
    TERRITORY_PERCHING,        /**< Resting on perch */
    TERRITORY_CHASING,         /**< Chasing intruder */
    TERRITORY_DISPLAYING,      /**< Territorial display */
    TERRITORY_HUNTING,         /**< Opportunistic hunting */
    TERRITORY_ABSENT           /**< Away from territory */
} territory_activity_t;

/**
 * @brief Intruder type
 */
typedef enum {
    INTRUDER_MALE_SAME,        /**< Same species male */
    INTRUDER_MALE_OTHER,       /**< Different species male */
    INTRUDER_FEMALE,           /**< Female (potential mate) */
    INTRUDER_PREDATOR,         /**< Predator (bird, etc.) */
    INTRUDER_UNKNOWN           /**< Unknown intruder */
} intruder_type_t;

/**
 * @brief Response to intruder
 */
typedef enum {
    RESPONSE_IGNORE,           /**< Ignore intruder */
    RESPONSE_MONITOR,          /**< Watch but don't act */
    RESPONSE_DISPLAY,          /**< Territorial display */
    RESPONSE_CHASE,            /**< Chase away */
    RESPONSE_FLEE,             /**< Flee from predator */
    RESPONSE_COURT             /**< Courtship behavior */
} intruder_response_t;

/**
 * @brief Landmark type
 */
typedef enum {
    LANDMARK_PERCH,            /**< Favorite perch location */
    LANDMARK_WATER,            /**< Water edge */
    LANDMARK_BOUNDARY,         /**< Territory boundary marker */
    LANDMARK_PREY_RICH,        /**< Prey-rich area */
    LANDMARK_DANGER,           /**< Dangerous area to avoid */
    LANDMARK_COMPETITOR        /**< Competitor's perch */
} landmark_type_t;

/**
 * @brief Spatial landmark
 */
typedef struct {
    float position[3];         /**< Position */
    landmark_type_t type;      /**< Landmark type */
    float importance;          /**< Importance weight [0,1] */
    uint64_t last_visit_us;    /**< Last visit timestamp */
    uint32_t visit_count;      /**< Number of visits */
} territory_landmark_t;

/**
 * @brief Patrol waypoint
 */
typedef struct {
    float position[3];         /**< Waypoint position */
    float loiter_time_s;       /**< Time to spend at waypoint */
    float scan_radius_m;       /**< Radius to scan around waypoint */
    uint32_t priority;         /**< Visitation priority */
} patrol_waypoint_t;

/**
 * @brief Tracked intruder
 */
typedef struct {
    uint32_t id;               /**< Intruder ID */
    float position[3];         /**< Current position */
    float velocity[3];         /**< Current velocity */
    intruder_type_t type;      /**< Intruder classification */
    float threat_level;        /**< Threat level [0,1] */
    uint64_t first_seen_us;    /**< First detection time */
    uint64_t last_seen_us;     /**< Last detection time */
    intruder_response_t response; /**< Current response */
} tracked_intruder_t;

/**
 * @brief Territory boundary
 */
typedef struct {
    float center[3];           /**< Territory center */
    float radius_m;            /**< Approximate radius */
    float boundary_points[TERRITORY_MAX_WAYPOINTS][3]; /**< Boundary vertices */
    uint32_t num_boundary_points; /**< Number of boundary points */
} territory_boundary_t;

/**
 * @brief Territory state
 */
typedef struct {
    territory_activity_t activity;  /**< Current activity */
    float current_position[3];      /**< Current position */
    uint32_t current_waypoint;      /**< Current patrol waypoint index */
    float patrol_progress;          /**< Progress through patrol [0,1] */

    /* Intruders */
    uint32_t num_intruders;         /**< Number of tracked intruders */
    uint32_t primary_intruder_id;   /**< Primary intruder being addressed */

    /* Time tracking */
    uint64_t time_on_patrol_us;     /**< Time spent patrolling */
    uint64_t time_on_perch_us;      /**< Time spent perching */
    uint64_t last_intruder_us;      /**< Time since last intruder */
} territory_state_t;

/**
 * @brief Territory configuration
 */
typedef struct {
    /* Territory definition */
    float default_radius_m;         /**< Default territory radius */
    bool auto_learn_boundaries;     /**< Learn boundaries from behavior */

    /* Patrol settings */
    float patrol_speed_m_s;         /**< Patrol flight speed */
    float waypoint_tolerance_m;     /**< Distance to consider "at" waypoint */
    float scan_time_s;              /**< Time to scan at each waypoint */

    /* Intruder response */
    float intruder_detect_radius_m; /**< Intruder detection radius */
    float chase_abandon_distance_m; /**< Distance to abandon chase */
    float chase_max_duration_s;     /**< Maximum chase duration */

    /* Energy management */
    float perch_energy_threshold;   /**< Energy level to seek perch */
    float perch_duration_s;         /**< Time to rest on perch */

    /* Learning */
    bool enable_route_optimization; /**< Optimize patrol route over time */
    float landmark_decay_rate;      /**< Rate of landmark importance decay */
} territory_config_t;

/**
 * @brief Territory statistics
 */
typedef struct {
    uint64_t patrols_completed;     /**< Complete patrol circuits */
    uint64_t intruders_detected;    /**< Total intruders detected */
    uint64_t chases_initiated;      /**< Chases started */
    uint64_t chases_successful;     /**< Intruders driven away */
    uint64_t prey_caught_in_territory; /**< Prey caught while patrolling */
    float avg_patrol_duration_s;    /**< Average patrol duration */
    float territory_coverage;       /**< Fraction of territory visited [0,1] */
} territory_stats_t;

//=============================================================================
// Configuration Functions
//=============================================================================

/**
 * @brief Get default territory configuration
 */
territory_config_t territory_default_config(void);

/**
 * @brief Validate territory configuration
 */
bool territory_validate_config(const territory_config_t* config);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create territory system
 */
dragonfly_territory_t dragonfly_territory_create(const territory_config_t* config);

/**
 * @brief Destroy territory system
 */
void dragonfly_territory_destroy(dragonfly_territory_t territory);

/**
 * @brief Reset territory system
 */
int dragonfly_territory_reset(dragonfly_territory_t territory);

//=============================================================================
// Territory Definition Functions
//=============================================================================

/**
 * @brief Set territory center
 */
int dragonfly_territory_set_center(
    dragonfly_territory_t territory,
    const float center[3],
    float radius_m
);

/**
 * @brief Add landmark to territory
 */
int dragonfly_territory_add_landmark(
    dragonfly_territory_t territory,
    const territory_landmark_t* landmark
);

/**
 * @brief Add patrol waypoint
 */
int dragonfly_territory_add_waypoint(
    dragonfly_territory_t territory,
    const patrol_waypoint_t* waypoint
);

/**
 * @brief Generate patrol route (auto-optimize)
 */
int dragonfly_territory_generate_route(dragonfly_territory_t territory);

//=============================================================================
// Update Functions
//=============================================================================

/**
 * @brief Update territory system
 *
 * @param territory Territory handle
 * @param current_pos Current position
 * @param dt_s Time step
 * @return 0 on success, -1 on error
 */
int dragonfly_territory_update(
    dragonfly_territory_t territory,
    const float current_pos[3],
    float dt_s
);

/**
 * @brief Report potential intruder
 */
int dragonfly_territory_report_intruder(
    dragonfly_territory_t territory,
    uint32_t id,
    const float position[3],
    const float velocity[3],
    float size,
    float threat_level
);

/**
 * @brief Get next patrol waypoint
 */
int dragonfly_territory_get_next_waypoint(
    const dragonfly_territory_t territory,
    float waypoint[3]
);

/**
 * @brief Get intruder response recommendation
 */
int dragonfly_territory_get_response(
    const dragonfly_territory_t territory,
    uint32_t intruder_id,
    intruder_response_t* response,
    float chase_vector[3]
);

//=============================================================================
// Query Functions
//=============================================================================

/**
 * @brief Check if position is within territory
 */
bool dragonfly_territory_contains(
    const dragonfly_territory_t territory,
    const float position[3]
);

/**
 * @brief Get territory state
 */
int dragonfly_territory_get_state(
    const dragonfly_territory_t territory,
    territory_state_t* state
);

/**
 * @brief Get territory boundary
 */
int dragonfly_territory_get_boundary(
    const dragonfly_territory_t territory,
    territory_boundary_t* boundary
);

/**
 * @brief Get tracked intruders
 */
int dragonfly_territory_get_intruders(
    const dragonfly_territory_t territory,
    tracked_intruder_t* intruders,
    uint32_t max_intruders,
    uint32_t* num_intruders
);

//=============================================================================
// Statistics
//=============================================================================

/**
 * @brief Get territory statistics
 */
int dragonfly_territory_get_stats(
    const dragonfly_territory_t territory,
    territory_stats_t* stats
);

/**
 * @brief Get activity name
 */
const char* dragonfly_territory_activity_name(territory_activity_t activity);

/**
 * @brief Get intruder type name
 */
const char* dragonfly_intruder_type_name(intruder_type_t type);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_DRAGONFLY_TERRITORY_H */
