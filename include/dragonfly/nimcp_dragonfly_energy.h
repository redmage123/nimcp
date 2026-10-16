/**
 * @file nimcp_dragonfly_energy.h
 * @brief Energy-Optimal Pursuit Planning
 *
 * BIOLOGICAL REFERENCE:
 * Dragonflies must balance pursuit success against energy expenditure.
 * A hunt that succeeds but uses more energy than gained is net negative.
 * Real dragonflies abort pursuits of low-value or difficult targets.
 *
 * WHAT: Models metabolic costs and optimizes pursuit energy
 * WHY:  Enables biologically realistic pursuit decisions
 * HOW:  Energy budget tracking with pursuit optimization
 *
 * @author NIMCP Team
 * @date 2024-12-29
 */

#ifndef NIMCP_DRAGONFLY_ENERGY_H
#define NIMCP_DRAGONFLY_ENERGY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "dragonfly/nimcp_dragonfly_intercept.h"

//=============================================================================
// Forward Declarations
//=============================================================================

typedef struct dragonfly_energy_s* dragonfly_energy_t;

//=============================================================================
// Type Definitions
//=============================================================================

/**
 * @brief Energy state classification
 */
typedef enum {
    ENERGY_STATE_FULL,        /**< Fully energized */
    ENERGY_STATE_ADEQUATE,    /**< Sufficient for hunting */
    ENERGY_STATE_LOW,         /**< Should conserve */
    ENERGY_STATE_CRITICAL,    /**< Must rest */
    ENERGY_STATE_DEPLETED     /**< Cannot hunt */
} energy_state_t;

/**
 * @brief Activity type for energy cost
 */
typedef enum {
    ACTIVITY_REST,            /**< Resting/perching */
    ACTIVITY_HOVER,           /**< Hovering in place */
    ACTIVITY_PATROL,          /**< Patrol flight */
    ACTIVITY_PURSUIT,         /**< Active pursuit */
    ACTIVITY_INTERCEPT,       /**< High-speed interception */
    ACTIVITY_EVASION          /**< Evasive maneuver */
} activity_type_t;

/**
 * @brief Pursuit energy estimate
 */
typedef struct {
    /* Cost estimate */
    float estimated_energy_j;     /**< Estimated energy cost */
    float energy_per_second_w;    /**< Power requirement */
    float pursuit_duration_s;     /**< Estimated duration */

    /* Outcome estimate */
    float energy_if_success_j;    /**< Energy if successful */
    float energy_if_failure_j;    /**< Energy if failed */
    float expected_energy_j;      /**< Expected (prob-weighted) */

    /* Value assessment */
    float prey_energy_value_j;    /**< Energy value of prey */
    float net_energy_expected_j;  /**< Expected net energy gain */
    bool economically_viable;     /**< Worth pursuing? */
    float roi;                    /**< Return on investment */

    /* Budget check */
    bool within_budget;           /**< Can afford this pursuit */
    float reserve_after_j;        /**< Reserve if successful */
} pursuit_energy_t;

/**
 * @brief Energy budget
 */
typedef struct {
    float current_energy_j;       /**< Current energy reserves */
    float max_energy_j;           /**< Maximum capacity */
    float reserve_minimum_j;      /**< Minimum reserve to maintain */

    /* Rates */
    float resting_rate_w;         /**< Resting metabolic rate */
    float current_rate_w;         /**< Current burn rate */

    /* State */
    energy_state_t state;         /**< Current energy state */
    float time_to_critical_s;     /**< Time until critical */
    float time_to_depletion_s;    /**< Time until depleted */

    /* Session tracking */
    float energy_spent_j;         /**< Energy spent this session */
    float energy_gained_j;        /**< Energy gained from prey */
} energy_budget_t;

/**
 * @brief Energy optimization result
 */
typedef struct {
    /* Optimal strategy */
    intercept_strategy_t best_strategy;  /**< Most efficient strategy */
    float optimal_speed;          /**< Energy-optimal speed */
    float optimal_pursuit_time_s; /**< Optimal pursuit duration */

    /* Trade-offs */
    float speed_vs_efficiency;    /**< Speed-efficiency trade-off point */
    float success_vs_cost;        /**< Success-cost trade-off point */

    /* Recommendation */
    bool should_pursue;           /**< Overall recommendation */
    const char* decision_reason;  /**< Reason for decision */
} energy_optimization_t;

/**
 * @brief Energy system configuration
 */
typedef struct {
    /* Capacity */
    float max_energy_j;           /**< Maximum energy capacity */
    float reserve_fraction;       /**< Fraction to keep as reserve */

    /* Metabolic rates (Watts) */
    float rest_power_w;           /**< Resting metabolic rate */
    float hover_power_w;          /**< Hovering power */
    float patrol_power_w;         /**< Patrol flight power */
    float pursuit_power_w;        /**< Active pursuit power */
    float max_power_w;            /**< Maximum power output */

    /* Acceleration cost */
    float accel_cost_j_per_ms2;   /**< Cost per m/s² acceleration */
    float turn_cost_j_per_rad;    /**< Cost per radian of turn */

    /* Prey values */
    float small_prey_value_j;     /**< Energy from small prey */
    float medium_prey_value_j;    /**< Energy from medium prey */
    float large_prey_value_j;     /**< Energy from large prey */

    /* Decision thresholds */
    float min_roi_threshold;      /**< Minimum ROI to pursue */
    float abort_roi_threshold;    /**< ROI to abort mid-pursuit */
    float critical_reserve_j;     /**< Critical reserve level */

    /* Recovery */
    float recovery_rate_w;        /**< Energy recovery rate when resting */
} energy_config_t;

/**
 * @brief Energy system statistics
 */
typedef struct {
    float total_energy_spent_j;   /**< Total energy spent */
    float total_energy_gained_j;  /**< Total energy gained */
    float net_energy_j;           /**< Net energy balance */
    uint64_t pursuits_attempted;  /**< Pursuits attempted */
    uint64_t pursuits_aborted_energy; /**< Aborted for energy */
    uint64_t economically_successful; /**< Net positive hunts */
    float avg_roi;                /**< Average ROI of hunts */
    float hunt_efficiency;        /**< Success energy / total energy */
} energy_stats_t;

//=============================================================================
// Configuration Functions
//=============================================================================

/**
 * @brief Get default energy configuration
 */
energy_config_t energy_default_config(void);

/**
 * @brief Validate energy configuration
 */
bool energy_validate_config(const energy_config_t* config);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create energy system
 */
dragonfly_energy_t dragonfly_energy_create(const energy_config_t* config);

/**
 * @brief Destroy energy system
 */
void dragonfly_energy_destroy(dragonfly_energy_t energy);

/**
 * @brief Reset energy system (full energy)
 */
int dragonfly_energy_reset(dragonfly_energy_t energy);

//=============================================================================
// Update Functions
//=============================================================================

/**
 * @brief Update energy (time passage)
 */
int dragonfly_energy_update(
    dragonfly_energy_t energy,
    activity_type_t activity,
    float dt_s
);

/**
 * @brief Spend energy
 */
int dragonfly_energy_spend(
    dragonfly_energy_t energy,
    float amount_j,
    const char* reason
);

/**
 * @brief Gain energy (from prey)
 */
int dragonfly_energy_gain(
    dragonfly_energy_t energy,
    float amount_j
);

/**
 * @brief Report prey capture
 */
int dragonfly_energy_capture_prey(
    dragonfly_energy_t energy,
    float prey_size,
    float pursuit_energy_j
);

//=============================================================================
// Estimation Functions
//=============================================================================

/**
 * @brief Estimate pursuit energy cost
 */
int dragonfly_energy_estimate_pursuit(
    const dragonfly_energy_t energy,
    const intercept_solution_t* solution,
    float prey_size,
    float success_probability,
    pursuit_energy_t* estimate
);

/**
 * @brief Get optimal pursuit parameters
 */
int dragonfly_energy_optimize_pursuit(
    const dragonfly_energy_t energy,
    const interceptor_state_t* self,
    const target_state_t* target,
    float prey_size,
    energy_optimization_t* optimization
);

/**
 * @brief Check if pursuit is affordable
 */
bool dragonfly_energy_can_afford(
    const dragonfly_energy_t energy,
    float estimated_cost_j
);

//=============================================================================
// Query Functions
//=============================================================================

/**
 * @brief Get current energy budget
 */
int dragonfly_energy_get_budget(
    const dragonfly_energy_t energy,
    energy_budget_t* budget
);

/**
 * @brief Get energy state
 */
energy_state_t dragonfly_energy_get_state(const dragonfly_energy_t energy);

/**
 * @brief Get current energy level
 */
float dragonfly_energy_get_level(const dragonfly_energy_t energy);

/**
 * @brief Get energy statistics
 */
int dragonfly_energy_get_stats(
    const dragonfly_energy_t energy,
    energy_stats_t* stats
);

/**
 * @brief Get energy state name
 */
const char* dragonfly_energy_state_name(energy_state_t state);

/**
 * @brief Get activity name
 */
const char* dragonfly_activity_name(activity_type_t activity);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_DRAGONFLY_ENERGY_H */
