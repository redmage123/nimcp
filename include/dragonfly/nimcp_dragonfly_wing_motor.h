/**
 * @file nimcp_dragonfly_wing_motor.h
 * @brief Four-Wing Motor Pattern Generator
 *
 * BIOLOGICAL REFERENCE:
 * Dragonflies have 4 independently controlled wings that can operate
 * asynchronously, enabling unique flight capabilities:
 * - Hover with minimal body movement
 * - Fly backwards
 * - Turn on a dime
 * - Reach speeds of 30+ mph
 *
 * WHAT: Generates wing motor patterns for flight control
 * WHY:  Enables biologically-realistic flight dynamics
 * HOW:  Central Pattern Generator (CPG) with phase coordination
 *
 * @author NIMCP Team
 * @date 2024-12-29
 */

#ifndef NIMCP_DRAGONFLY_WING_MOTOR_H
#define NIMCP_DRAGONFLY_WING_MOTOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

//=============================================================================
// Forward Declarations
//=============================================================================

typedef struct dragonfly_wing_motor_s* dragonfly_wing_motor_t;

//=============================================================================
// Constants
//=============================================================================

#define WING_COUNT 4                /**< 4 independent wings */
#define WING_MAX_FREQUENCY_HZ 40.0f /**< Maximum wing beat frequency */
#define WING_MIN_FREQUENCY_HZ 20.0f /**< Minimum wing beat frequency */

//=============================================================================
// Type Definitions
//=============================================================================

/**
 * @brief Individual wing identifier
 */
typedef enum {
    WING_LEFT_FORE,    /**< Left forewing */
    WING_LEFT_HIND,    /**< Left hindwing */
    WING_RIGHT_FORE,   /**< Right forewing */
    WING_RIGHT_HIND    /**< Right hindwing */
} wing_id_t;

/**
 * @brief Flight mode
 */
typedef enum {
    FLIGHT_MODE_HOVER,       /**< Stationary hover */
    FLIGHT_MODE_FORWARD,     /**< Forward flight */
    FLIGHT_MODE_BACKWARD,    /**< Backward flight */
    FLIGHT_MODE_ASCEND,      /**< Climbing flight */
    FLIGHT_MODE_DESCEND,     /**< Descending flight */
    FLIGHT_MODE_TURN_LEFT,   /**< Left turn */
    FLIGHT_MODE_TURN_RIGHT,  /**< Right turn */
    FLIGHT_MODE_PURSUIT,     /**< High-speed pursuit */
    FLIGHT_MODE_EVASIVE      /**< Evasive maneuver */
} flight_mode_t;

/**
 * @brief Individual wing state
 */
typedef struct {
    float stroke_amplitude;   /**< Stroke amplitude [0,1] */
    float stroke_frequency;   /**< Beat frequency (Hz) */
    float stroke_phase;       /**< Current phase [0, 2*PI] */
    float stroke_plane_angle; /**< Stroke plane tilt (rad) */
    float feather_angle;      /**< Wing feathering angle (rad) */
    float deviation_angle;    /**< Up-down deviation (rad) */
} wing_state_t;

/**
 * @brief Motor command for single wing
 */
typedef struct {
    wing_id_t wing;           /**< Which wing */
    float torque;             /**< Applied torque [0,1] */
    float angle_rad;          /**< Target angle */
    float velocity_rad_s;     /**< Target angular velocity */
} wing_command_t;

/**
 * @brief Complete wing motor output
 */
typedef struct {
    wing_state_t wings[WING_COUNT];  /**< State of all 4 wings */
    float net_thrust;                /**< Net thrust force */
    float net_torque[3];             /**< Net torque (roll, pitch, yaw) */
    uint64_t timestamp_us;           /**< Output timestamp */
} wing_motor_output_t;

/**
 * @brief Desired flight dynamics
 */
typedef struct {
    float velocity[3];        /**< Desired velocity (m/s) */
    float acceleration[3];    /**< Desired acceleration (m/s^2) */
    float angular_velocity[3];/**< Desired angular velocity (rad/s) */
    float heading_rad;        /**< Desired heading */
    float pitch_rad;          /**< Desired pitch */
    float roll_rad;           /**< Desired roll */
} flight_dynamics_t;

/**
 * @brief Wing motor configuration
 */
typedef struct {
    /* Wing geometry */
    float wing_length_m;           /**< Wing length */
    float wing_chord_m;            /**< Wing chord */
    float wing_mass_kg;            /**< Wing mass */

    /* CPG parameters */
    float base_frequency_hz;       /**< Base oscillation frequency */
    float frequency_range_hz;      /**< Frequency modulation range */
    float phase_coupling_strength; /**< Inter-wing phase coupling */

    /* Control gains */
    float thrust_gain;             /**< Thrust control gain */
    float roll_gain;               /**< Roll control gain */
    float pitch_gain;              /**< Pitch control gain */
    float yaw_gain;                /**< Yaw control gain */

    /* Limits */
    float max_amplitude;           /**< Maximum stroke amplitude */
    float max_frequency_hz;        /**< Maximum frequency */
    float max_asymmetry;           /**< Maximum left-right asymmetry */

    /* Energy model */
    bool enable_energy_model;      /**< Enable metabolic cost tracking */
    float energy_per_stroke_j;     /**< Energy cost per wing stroke */
} wing_motor_config_t;

/**
 * @brief Wing motor statistics
 */
typedef struct {
    uint64_t cycles_generated;     /**< Total CPG cycles */
    uint64_t strokes_total;        /**< Total wing strokes */
    float avg_frequency_hz;        /**< Average beat frequency */
    float avg_amplitude;           /**< Average stroke amplitude */
    float total_energy_j;          /**< Total energy expended */
    float max_thrust_achieved;     /**< Maximum thrust achieved */
    float avg_efficiency;          /**< Average propulsive efficiency */
} wing_motor_stats_t;

//=============================================================================
// Configuration Functions
//=============================================================================

/**
 * @brief Get default wing motor configuration
 */
wing_motor_config_t wing_motor_default_config(void);

/**
 * @brief Validate wing motor configuration
 */
bool wing_motor_validate_config(const wing_motor_config_t* config);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create wing motor system
 */
dragonfly_wing_motor_t dragonfly_wing_motor_create(const wing_motor_config_t* config);

/**
 * @brief Destroy wing motor system
 */
void dragonfly_wing_motor_destroy(dragonfly_wing_motor_t motor);

/**
 * @brief Reset wing motor system
 */
int dragonfly_wing_motor_reset(dragonfly_wing_motor_t motor);

//=============================================================================
// Control Functions
//=============================================================================

/**
 * @brief Set flight mode
 *
 * @param motor Wing motor handle
 * @param mode Desired flight mode
 * @return 0 on success, -1 on error
 */
int dragonfly_wing_motor_set_mode(
    dragonfly_wing_motor_t motor,
    flight_mode_t mode
);

/**
 * @brief Set desired flight dynamics
 *
 * @param motor Wing motor handle
 * @param dynamics Desired dynamics
 * @return 0 on success, -1 on error
 */
int dragonfly_wing_motor_set_dynamics(
    dragonfly_wing_motor_t motor,
    const flight_dynamics_t* dynamics
);

/**
 * @brief Step the CPG and generate wing commands
 *
 * @param motor Wing motor handle
 * @param dt_s Time step in seconds
 * @param output Output wing motor commands
 * @return 0 on success, -1 on error
 */
int dragonfly_wing_motor_step(
    dragonfly_wing_motor_t motor,
    float dt_s,
    wing_motor_output_t* output
);

/**
 * @brief Apply pursuit-optimized wing pattern
 *
 * @param motor Wing motor handle
 * @param heading_error Heading error to target (rad)
 * @param pitch_error Pitch error to target (rad)
 * @param speed_demand Speed demand [0,1]
 * @return 0 on success, -1 on error
 */
int dragonfly_wing_motor_pursuit_pattern(
    dragonfly_wing_motor_t motor,
    float heading_error,
    float pitch_error,
    float speed_demand
);

/**
 * @brief Apply hover stabilization pattern
 *
 * @param motor Wing motor handle
 * @param roll_error Roll error from level (rad)
 * @param pitch_error Pitch error from level (rad)
 * @param altitude_error Altitude error (m)
 * @return 0 on success, -1 on error
 */
int dragonfly_wing_motor_hover_pattern(
    dragonfly_wing_motor_t motor,
    float roll_error,
    float pitch_error,
    float altitude_error
);

//=============================================================================
// Query Functions
//=============================================================================

/**
 * @brief Get current wing state
 */
int dragonfly_wing_motor_get_state(
    const dragonfly_wing_motor_t motor,
    wing_id_t wing,
    wing_state_t* state
);

/**
 * @brief Get all wing states
 */
int dragonfly_wing_motor_get_all_states(
    const dragonfly_wing_motor_t motor,
    wing_state_t states[WING_COUNT]
);

/**
 * @brief Get current flight mode
 */
flight_mode_t dragonfly_wing_motor_get_mode(const dragonfly_wing_motor_t motor);

/**
 * @brief Get current energy consumption rate
 */
float dragonfly_wing_motor_get_power_w(const dragonfly_wing_motor_t motor);

//=============================================================================
// Statistics and Configuration
//=============================================================================

/**
 * @brief Get wing motor statistics
 */
int dragonfly_wing_motor_get_stats(
    const dragonfly_wing_motor_t motor,
    wing_motor_stats_t* stats
);

/**
 * @brief Reset wing motor statistics
 */
int dragonfly_wing_motor_reset_stats(dragonfly_wing_motor_t motor);

/**
 * @brief Update wing motor configuration
 */
int dragonfly_wing_motor_set_config(
    dragonfly_wing_motor_t motor,
    const wing_motor_config_t* config
);

/**
 * @brief Get flight mode name
 */
const char* dragonfly_flight_mode_name(flight_mode_t mode);

/**
 * @brief Get wing name
 */
const char* dragonfly_wing_name(wing_id_t wing);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_DRAGONFLY_WING_MOTOR_H */
