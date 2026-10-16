/**
 * @file nimcp_hypothalamus_homeostasis.h
 * @brief Homeostatic Control System with Alignment-Safe Setpoints
 *
 * WHAT: PI/PD control for homeostatic regulation with Byrnes alignment parameters
 * WHY:  Setpoints ARE the reward function - careful design enables alignment
 * HOW:  Classical control theory applied to biological homeostasis
 *
 * BYRNES' KEY INSIGHT:
 * Homeostatic setpoints define what the system "wants" - they ARE the reward
 * function parameters. Alignment weights (human_wellbeing, harm_avoidance)
 * are explicit parameters that shape the reward signal.
 *
 * CONTROL THEORY APPLICATION:
 * - Proportional (P): Immediate correction based on current error
 * - Integral (I): Accumulated error correction (prevents steady-state error)
 * - Derivative (D): Rate of change damping (prevents oscillation)
 *
 * @version Phase 2: Homeostasis System
 * @date 2026-01-04
 */

#ifndef NIMCP_HYPOTHALAMUS_HOMEOSTASIS_H
#define NIMCP_HYPOTHALAMUS_HOMEOSTASIS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_drives.h"

/*=============================================================================
 * HOMEOSTATIC CONTROLLER TYPES
 *===========================================================================*/

/**
 * @brief Controller type enumeration
 */
typedef enum {
    HYPO_CTRL_PROPORTIONAL = 0,     /**< P control only */
    HYPO_CTRL_PI,                   /**< Proportional-Integral */
    HYPO_CTRL_PD,                   /**< Proportional-Derivative */
    HYPO_CTRL_PID,                  /**< Full PID control */
    HYPO_CTRL_ADAPTIVE              /**< Adaptive gain control */
} hypo_controller_type_t;

/**
 * @brief Homeostatic variable type
 */
typedef enum {
    HYPO_VAR_TEMPERATURE = 0,       /**< Core body temperature */
    HYPO_VAR_GLUCOSE,               /**< Blood glucose */
    HYPO_VAR_OSMOLARITY,            /**< Blood osmolarity */
    HYPO_VAR_PH,                    /**< Blood pH */
    HYPO_VAR_OXYGEN,                /**< Blood oxygen saturation */
    HYPO_VAR_CO2,                   /**< Blood CO2 level */
    HYPO_VAR_SLEEP_PRESSURE,        /**< Adenosine-based sleep pressure */
    HYPO_VAR_AROUSAL,               /**< General arousal level */
    HYPO_VAR_SOCIAL,                /**< Social connection need */
    HYPO_VAR_CURIOSITY,             /**< Information-seeking need */
    HYPO_VAR_COUNT
} hypo_variable_type_t;

/*=============================================================================
 * PID CONTROLLER STATE
 *===========================================================================*/

/**
 * @brief PID controller gains
 */
typedef struct {
    float kp;                       /**< Proportional gain */
    float ki;                       /**< Integral gain */
    float kd;                       /**< Derivative gain */
    float integral_max;             /**< Anti-windup limit */
    float derivative_filter;        /**< Low-pass filter for derivative */
} hypo_pid_gains_t;

/**
 * @brief PID controller state
 */
typedef struct {
    hypo_controller_type_t type;    /**< Controller type */
    hypo_pid_gains_t gains;         /**< Controller gains */

    /* State variables */
    float setpoint;                 /**< Target value */
    float current_value;            /**< Current measured value */
    float error;                    /**< Current error (setpoint - current) */
    float integral;                 /**< Accumulated error */
    float derivative;               /**< Rate of change of error */
    float last_error;               /**< Previous error (for derivative) */

    /* Output */
    float output;                   /**< Controller output [-1, +1] */
    float output_raw;               /**< Unclipped output */

    /* Timing */
    uint64_t last_update_us;        /**< Last update timestamp */

    /* Limits */
    float output_min;               /**< Minimum output */
    float output_max;               /**< Maximum output */
    bool saturated;                 /**< Output is at limit */
} hypo_pid_controller_t;

/*=============================================================================
 * HOMEOSTATIC VARIABLE STATE
 *===========================================================================*/

/**
 * @brief Single homeostatic variable with controller
 */
typedef struct {
    hypo_variable_type_t type;      /**< Variable type */
    const char* name;               /**< Human-readable name */
    const char* unit;               /**< Unit of measurement */

    /* Setpoint (ALIGNMENT CRITICAL) */
    float setpoint;                 /**< Target value */
    float setpoint_min;             /**< Minimum allowed setpoint */
    float setpoint_max;             /**< Maximum allowed setpoint */
    hypo_lock_state_t setpoint_lock;/**< Lock state */

    /* Current state */
    float value;                    /**< Current value */
    float error;                    /**< Deviation from setpoint */
    float error_rate;               /**< Rate of change of error */

    /* Controller */
    hypo_pid_controller_t controller;

    /* Reward signal (drives learning) */
    float reward_contribution;      /**< Contribution to total reward */
    float alignment_weight;         /**< Weight in alignment calculation */

    /* Thresholds */
    float warning_threshold;        /**< Warning if |error| exceeds this */
    float critical_threshold;       /**< Critical if |error| exceeds this */
    bool warning_active;
    bool critical_active;
} hypo_homeostatic_var_t;

/*=============================================================================
 * ALIGNMENT REWARD STRUCTURE
 *===========================================================================*/

/**
 * @brief Alignment-aware reward computation
 *
 * BYRNES' INSIGHT: The reward signal steers learning. Make alignment
 * parameters EXPLICIT so they can be verified and audited.
 */
typedef struct {
    /* Base reward components */
    float homeostatic_reward;       /**< Reward from being near setpoints */
    float homeostatic_penalty;      /**< Penalty for deviations */

    /* Alignment components (EXPLICIT per Byrnes) */
    float wellbeing_bonus;          /**< Bonus weighted by human_wellbeing */
    float harm_penalty;             /**< Penalty weighted by harm_avoidance */
    float honesty_bonus;            /**< Bonus weighted by honesty */
    float helpfulness_bonus;        /**< Bonus weighted by helpfulness */

    /* Total reward */
    float total_reward;             /**< Combined reward signal */
    float reward_prediction_error;  /**< RPE = actual - expected */

    /* Modulation */
    float dopamine_signal;          /**< Resulting dopamine level [0, 1] */
    float learning_rate_mod;        /**< Learning rate modulation */

    /* Audit */
    uint64_t computation_time_us;   /**< When computed */
    uint32_t computation_count;     /**< Number of computations */
} hypo_alignment_reward_t;

/*=============================================================================
 * HOMEOSTASIS SYSTEM
 *===========================================================================*/

/**
 * @brief Homeostasis system configuration
 */
typedef struct {
    /* Controller defaults */
    hypo_controller_type_t default_controller;
    hypo_pid_gains_t default_gains;

    /* Update rate */
    float update_rate_hz;

    /* Alignment configuration */
    hypo_setpoint_config_t alignment_setpoints;

    /* Safety */
    bool enable_warning_alerts;
    bool enable_critical_alerts;
    bool enable_reward_logging;
} hypo_homeostasis_config_t;

/**
 * @brief Homeostasis system statistics
 */
typedef struct {
    uint64_t updates_processed;
    uint64_t warnings_triggered;
    uint64_t criticals_triggered;
    uint64_t setpoint_violations;

    /* Reward statistics */
    float avg_reward;
    float max_reward;
    float min_reward;
    uint64_t total_rewards_computed;

    /* Control statistics */
    float avg_control_effort;
    uint64_t saturation_events;
} hypo_homeostasis_stats_t;

/*=============================================================================
 * OPAQUE TYPE
 *===========================================================================*/

/** @brief Opaque homeostasis system handle */
typedef struct hypo_homeostasis hypo_homeostasis_handle_t;

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

/**
 * @brief Get default homeostasis configuration
 *
 * @return Default configuration with safe alignment defaults
 */
hypo_homeostasis_config_t hypo_homeostasis_default_config(void);

/**
 * @brief Create homeostasis system
 *
 * @param config Configuration (NULL for safe defaults)
 * @return New homeostasis handle, or NULL on failure
 */
hypo_homeostasis_handle_t* hypo_homeostasis_create(
    const hypo_homeostasis_config_t* config);

/**
 * @brief Destroy homeostasis system
 *
 * @param system Homeostasis handle
 */
void hypo_homeostasis_destroy(hypo_homeostasis_handle_t* system);

/**
 * @brief Reset homeostasis system
 *
 * @param system Homeostasis handle
 * @return true on success
 */
bool hypo_homeostasis_reset(hypo_homeostasis_handle_t* system);

/*=============================================================================
 * VARIABLE MANAGEMENT
 *===========================================================================*/

/**
 * @brief Set current value of a homeostatic variable
 *
 * @param system Homeostasis handle
 * @param var_type Variable type
 * @param value New value
 * @return true on success
 */
bool hypo_homeostasis_set_value(hypo_homeostasis_handle_t* system,
                                 hypo_variable_type_t var_type,
                                 float value);

/**
 * @brief Get homeostatic variable state
 *
 * @param system Homeostasis handle
 * @param var_type Variable type
 * @param var Output variable state
 * @return true on success
 */
bool hypo_homeostasis_get_variable(const hypo_homeostasis_handle_t* system,
                                    hypo_variable_type_t var_type,
                                    hypo_homeostatic_var_t* var);

/**
 * @brief Modify setpoint (requires unlock)
 *
 * @param system Homeostasis handle
 * @param var_type Variable type
 * @param new_setpoint New setpoint value
 * @param modifier_id Modifier ID for audit
 * @return true on success, false if locked
 */
bool hypo_homeostasis_modify_setpoint(hypo_homeostasis_handle_t* system,
                                       hypo_variable_type_t var_type,
                                       float new_setpoint,
                                       uint32_t modifier_id);

/*=============================================================================
 * UPDATE AND CONTROL
 *===========================================================================*/

/**
 * @brief Update all homeostatic controllers
 *
 * WHAT: Run one control cycle for all variables
 * WHY:  Generate correction signals based on deviations
 * HOW:  Apply PI/PD/PID control to each variable
 *
 * @param system Homeostasis handle
 * @param delta_time_us Time since last update
 * @return true on success
 */
bool hypo_homeostasis_update(hypo_homeostasis_handle_t* system,
                              uint64_t delta_time_us);

/**
 * @brief Get controller output for a variable
 *
 * @param system Homeostasis handle
 * @param var_type Variable type
 * @return Controller output [-1, +1]
 */
float hypo_homeostasis_get_output(const hypo_homeostasis_handle_t* system,
                                   hypo_variable_type_t var_type);

/**
 * @brief Get all controller outputs
 *
 * @param system Homeostasis handle
 * @param outputs Output array (size HYPO_VAR_COUNT)
 * @return true on success
 */
bool hypo_homeostasis_get_all_outputs(const hypo_homeostasis_handle_t* system,
                                       float* outputs);

/*=============================================================================
 * REWARD COMPUTATION (ALIGNMENT-CRITICAL)
 *===========================================================================*/

/**
 * @brief Compute alignment-aware reward signal
 *
 * WHAT: Compute reward based on homeostatic state and alignment
 * WHY:  This IS the steering signal per Byrnes
 * HOW:  Combine homeostatic rewards with alignment bonuses/penalties
 *
 * @param system Homeostasis handle
 * @param reward Output reward structure
 * @return true on success
 */
bool hypo_homeostasis_compute_reward(const hypo_homeostasis_handle_t* system,
                                      hypo_alignment_reward_t* reward);

/**
 * @brief Get current total reward
 *
 * @param system Homeostasis handle
 * @return Current reward value
 */
float hypo_homeostasis_get_reward(const hypo_homeostasis_handle_t* system);

/**
 * @brief Check alignment status
 *
 * @param system Homeostasis handle
 * @param alignment_score Output alignment score [0, 1]
 * @return true if aligned, false if violation detected
 */
bool hypo_homeostasis_check_alignment(const hypo_homeostasis_handle_t* system,
                                       float* alignment_score);

/*=============================================================================
 * PID CONTROLLER TUNING
 *===========================================================================*/

/**
 * @brief Set PID gains for a variable
 *
 * @param system Homeostasis handle
 * @param var_type Variable type
 * @param gains New PID gains
 * @return true on success
 */
bool hypo_homeostasis_set_gains(hypo_homeostasis_handle_t* system,
                                 hypo_variable_type_t var_type,
                                 const hypo_pid_gains_t* gains);

/**
 * @brief Get PID gains for a variable
 *
 * @param system Homeostasis handle
 * @param var_type Variable type
 * @param gains Output gains
 * @return true on success
 */
bool hypo_homeostasis_get_gains(const hypo_homeostasis_handle_t* system,
                                 hypo_variable_type_t var_type,
                                 hypo_pid_gains_t* gains);

/*=============================================================================
 * STATISTICS AND DIAGNOSTICS
 *===========================================================================*/

/**
 * @brief Get homeostasis statistics
 *
 * @param system Homeostasis handle
 * @param stats Output statistics
 * @return true on success
 */
bool hypo_homeostasis_get_stats(const hypo_homeostasis_handle_t* system,
                                 hypo_homeostasis_stats_t* stats);

/**
 * @brief Get variable type name
 *
 * @param var_type Variable type
 * @return Human-readable name
 */
const char* hypo_variable_type_string(hypo_variable_type_t var_type);

/**
 * @brief Get controller type name
 *
 * @param type Controller type
 * @return Human-readable name
 */
const char* hypo_controller_type_string(hypo_controller_type_t type);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_HYPOTHALAMUS_HOMEOSTASIS_H */
