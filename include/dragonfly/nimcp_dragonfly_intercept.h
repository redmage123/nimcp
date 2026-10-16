/**
 * @file nimcp_dragonfly_intercept.h
 * @brief Interception Planning Module
 *
 * WHAT: Computes optimal interception trajectories to catch moving targets
 * WHY:  Dragonflies achieve 95% prey interception success using predictive steering
 * HOW:  Proportional navigation + lead angle calculation + energy optimization
 *
 * BIOLOGICAL REFERENCE:
 * - Mischiati et al. (2015) "Internal models direct dragonfly interception steering"
 * - Olberg et al. (2000) "Prey pursuit and interception in dragonflies"
 *
 * KEY FEATURES:
 * - Proportional Navigation (PN): constant bearing decrease
 * - Lead angle calculation for moving targets
 * - Time-to-intercept (TTI) computation
 * - Multiple strategies: head-on, parallel, pursuit, optimal
 * - Energy-efficient trajectory planning
 *
 * @author NIMCP Team
 * @date 2024-12-27
 */

#ifndef NIMCP_DRAGONFLY_INTERCEPT_H
#define NIMCP_DRAGONFLY_INTERCEPT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

//=============================================================================
// Forward Declarations
//=============================================================================

typedef struct dragonfly_interceptor_s dragonfly_interceptor_t;

//=============================================================================
// Constants
//=============================================================================

#define INTERCEPT_MAX_WAYPOINTS 32    /**< Maximum trajectory waypoints */

//=============================================================================
// Type Definitions
//=============================================================================

/**
 * @brief Interception strategy types
 */
typedef enum {
    INTERCEPT_PURSUIT,        /**< Pure pursuit (always aim at target) */
    INTERCEPT_LEAD,           /**< Lead pursuit (aim ahead of target) */
    INTERCEPT_PARALLEL,       /**< Parallel navigation (maintain bearing) */
    INTERCEPT_PN,             /**< Proportional Navigation (constant bearing rate) */
    INTERCEPT_OPTIMAL         /**< Energy-optimal trajectory */
} intercept_strategy_t;

/**
 * @brief Interception feasibility result
 */
typedef enum {
    INTERCEPT_FEASIBLE,       /**< Interception is achievable */
    INTERCEPT_TOO_FAST,       /**< Target faster than interceptor */
    INTERCEPT_OUT_OF_RANGE,   /**< Target too far */
    INTERCEPT_ESCAPING,       /**< Target escaping (diverging) */
    INTERCEPT_UNCERTAIN       /**< Cannot determine (evasive target) */
} intercept_feasibility_t;

/**
 * @brief Interceptor state (self position/velocity)
 */
typedef struct {
    float position[3];        /**< Current position (x, y, z) */
    float velocity[3];        /**< Current velocity (vx, vy, vz) */
    float max_speed;          /**< Maximum achievable speed */
    float max_accel;          /**< Maximum acceleration capability */
    float max_turn_rate;      /**< Maximum turn rate (rad/s) */
} interceptor_state_t;

/**
 * @brief Target state for interception
 */
typedef struct {
    float position[3];        /**< Current position (x, y, z) */
    float velocity[3];        /**< Current velocity (vx, vy, vz) */
    float acceleration[3];    /**< Current acceleration (ax, ay, az) */
    float confidence;         /**< State confidence [0,1] */
} target_state_t;

/**
 * @brief Computed interception solution
 */
typedef struct {
    intercept_feasibility_t feasibility;  /**< Is interception achievable? */
    intercept_strategy_t strategy;        /**< Strategy used */

    float intercept_point[3];             /**< Predicted intercept location */
    float intercept_time_s;               /**< Time to intercept (seconds) */
    float lead_angle_rad;                 /**< Lead angle (radians) */
    float heading_rad;                    /**< Required heading (radians) */

    float required_speed;                 /**< Speed needed for intercept */
    float required_accel[3];              /**< Acceleration command */

    float closing_speed;                  /**< Rate of range decrease */
    float miss_distance;                  /**< Predicted miss if unchanged */
    float confidence;                     /**< Solution confidence [0,1] */

    uint64_t timestamp_us;                /**< When computed */
} intercept_solution_t;

/**
 * @brief Interception trajectory waypoint
 */
typedef struct {
    float position[3];        /**< Waypoint position */
    float velocity[3];        /**< Velocity at waypoint */
    float time_s;             /**< Time to reach (seconds) */
    float energy_cost;        /**< Energy to reach this point */
} intercept_waypoint_t;

/**
 * @brief Complete interception trajectory
 */
typedef struct {
    intercept_waypoint_t waypoints[INTERCEPT_MAX_WAYPOINTS];
    uint32_t num_waypoints;
    float total_time_s;
    float total_energy;
    intercept_strategy_t strategy;
} intercept_trajectory_t;

/**
 * @brief Interceptor configuration
 */
typedef struct {
    /* Navigation parameters */
    float pn_gain;                    /**< Proportional navigation gain (3-5 typical) */
    float lead_time_factor;           /**< Lead time multiplier */
    float min_intercept_time_s;       /**< Minimum time to plan intercept */
    float max_intercept_time_s;       /**< Maximum lookahead for intercept */

    /* Constraints */
    float min_closing_speed;          /**< Minimum acceptable closing speed */
    float max_miss_distance;          /**< Maximum acceptable miss distance */
    float safety_margin;              /**< Safety factor for constraints */

    /* Energy optimization */
    bool optimize_energy;             /**< Enable energy-efficient planning */
    float energy_weight;              /**< Weight for energy vs time tradeoff */

    /* Strategy selection */
    intercept_strategy_t preferred_strategy;  /**< Default strategy */
    bool auto_select_strategy;        /**< Automatically choose best strategy */
} intercept_config_t;

/**
 * @brief Interceptor statistics
 */
typedef struct {
    uint64_t solutions_computed;      /**< Total solutions computed */
    uint64_t feasible_count;          /**< Feasible solutions */
    uint64_t infeasible_count;        /**< Infeasible solutions */
    float avg_compute_time_us;        /**< Average computation time */
    float avg_intercept_time_s;       /**< Average intercept time */
    float avg_lead_angle_rad;         /**< Average lead angle */
    uint32_t strategy_usage[5];       /**< Per-strategy usage count */
} intercept_stats_t;

//=============================================================================
// Configuration Functions
//=============================================================================

/**
 * @brief Get default interceptor configuration
 */
intercept_config_t intercept_default_config(void);

/**
 * @brief Validate interceptor configuration
 */
bool intercept_validate_config(const intercept_config_t* config);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create an interceptor planner
 */
dragonfly_interceptor_t* dragonfly_interceptor_create(const intercept_config_t* config);

/**
 * @brief Destroy interceptor and free resources
 */
void dragonfly_interceptor_destroy(dragonfly_interceptor_t* interceptor);

/**
 * @brief Reset interceptor state
 */
int dragonfly_interceptor_reset(dragonfly_interceptor_t* interceptor);

//=============================================================================
// Core Interception Functions
//=============================================================================

/**
 * @brief Compute interception solution
 *
 * WHAT: Main interception planning function
 * WHY:  Computes optimal intercept point and required maneuver
 * HOW:  Uses specified strategy (PN, lead pursuit, etc.)
 *
 * @param interceptor Interceptor planner
 * @param self Current interceptor state
 * @param target Current target state
 * @param solution Output: interception solution
 * @return 0 on success, -1 on error
 */
int dragonfly_intercept_compute(
    dragonfly_interceptor_t* interceptor,
    const interceptor_state_t* self,
    const target_state_t* target,
    intercept_solution_t* solution
);

/**
 * @brief Compute with specific strategy
 */
int dragonfly_intercept_compute_strategy(
    dragonfly_interceptor_t* interceptor,
    const interceptor_state_t* self,
    const target_state_t* target,
    intercept_strategy_t strategy,
    intercept_solution_t* solution
);

/**
 * @brief Check if interception is feasible
 */
intercept_feasibility_t dragonfly_intercept_check_feasibility(
    dragonfly_interceptor_t* interceptor,
    const interceptor_state_t* self,
    const target_state_t* target
);

/**
 * @brief Compute required acceleration command
 *
 * @param interceptor Interceptor planner
 * @param self Current interceptor state
 * @param target Target state
 * @param accel_cmd Output: acceleration command [3]
 * @return 0 on success, -1 on error
 */
int dragonfly_intercept_get_command(
    dragonfly_interceptor_t* interceptor,
    const interceptor_state_t* self,
    const target_state_t* target,
    float accel_cmd[3]
);

//=============================================================================
// Trajectory Planning Functions
//=============================================================================

/**
 * @brief Plan complete interception trajectory
 */
int dragonfly_intercept_plan_trajectory(
    dragonfly_interceptor_t* interceptor,
    const interceptor_state_t* self,
    const target_state_t* target,
    intercept_trajectory_t* trajectory
);

/**
 * @brief Get trajectory waypoint at time
 */
int dragonfly_intercept_get_waypoint(
    const intercept_trajectory_t* trajectory,
    float time_s,
    intercept_waypoint_t* waypoint
);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Compute time-to-intercept
 */
float dragonfly_intercept_time_to_intercept(
    const interceptor_state_t* self,
    const target_state_t* target
);

/**
 * @brief Compute lead angle for moving target
 */
float dragonfly_intercept_lead_angle(
    const interceptor_state_t* self,
    const target_state_t* target
);

/**
 * @brief Compute bearing to target
 */
float dragonfly_intercept_bearing(
    const interceptor_state_t* self,
    const target_state_t* target
);

/**
 * @brief Compute closing speed
 */
float dragonfly_intercept_closing_speed(
    const interceptor_state_t* self,
    const target_state_t* target
);

/**
 * @brief Compute range to target
 */
float dragonfly_intercept_range(
    const interceptor_state_t* self,
    const target_state_t* target
);

//=============================================================================
// Statistics and Configuration
//=============================================================================

/**
 * @brief Get interceptor statistics
 */
int dragonfly_interceptor_get_stats(
    const dragonfly_interceptor_t* interceptor,
    intercept_stats_t* stats
);

/**
 * @brief Reset statistics
 */
int dragonfly_interceptor_reset_stats(dragonfly_interceptor_t* interceptor);

/**
 * @brief Update configuration
 */
int dragonfly_interceptor_set_config(
    dragonfly_interceptor_t* interceptor,
    const intercept_config_t* config
);

/**
 * @brief Get current configuration
 */
int dragonfly_interceptor_get_config(
    const dragonfly_interceptor_t* interceptor,
    intercept_config_t* config
);

//=============================================================================
// Name Functions
//=============================================================================

/**
 * @brief Get strategy name
 */
const char* dragonfly_strategy_name(intercept_strategy_t strategy);

/**
 * @brief Get feasibility name
 */
const char* dragonfly_feasibility_name(intercept_feasibility_t feasibility);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_DRAGONFLY_INTERCEPT_H */
