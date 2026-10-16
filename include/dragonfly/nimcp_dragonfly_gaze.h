/**
 * @file nimcp_dragonfly_gaze.h
 * @brief Predictive Gaze Stabilization System
 *
 * BIOLOGICAL REFERENCE:
 * Dragonflies stabilize their head independently of body movements,
 * keeping their gaze locked on targets even during aggressive pursuit
 * maneuvers. This is critical for maintaining visual tracking during
 * the rapid turns required for interception.
 *
 * WHAT: Implements gaze stabilization with target lock
 * WHY:  Maintains visual tracking during body maneuvers
 * HOW:  Predictive head movement with vestibular-ocular reflex
 *
 * @author NIMCP Team
 * @date 2024-12-29
 */

#ifndef NIMCP_DRAGONFLY_GAZE_H
#define NIMCP_DRAGONFLY_GAZE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

//=============================================================================
// Forward Declarations
//=============================================================================

typedef struct dragonfly_gaze_s* dragonfly_gaze_t;

//=============================================================================
// Constants
//=============================================================================

#define GAZE_MAX_TARGETS 4            /**< Max simultaneous gaze targets */

//=============================================================================
// Type Definitions
//=============================================================================

/**
 * @brief Gaze mode
 */
typedef enum {
    GAZE_MODE_FREE,           /**< Free gaze (no lock) */
    GAZE_MODE_STABILIZED,     /**< Horizon stabilized */
    GAZE_MODE_TARGET_LOCK,    /**< Locked on target */
    GAZE_MODE_PREDICTIVE,     /**< Predictive tracking */
    GAZE_MODE_SACCADE         /**< Rapid gaze shift */
} gaze_mode_t;

/**
 * @brief Gaze target type
 */
typedef enum {
    GAZE_TARGET_NONE,         /**< No target */
    GAZE_TARGET_PREY,         /**< Prey target */
    GAZE_TARGET_PREDATOR,     /**< Predator (threat) */
    GAZE_TARGET_LANDMARK,     /**< Environmental landmark */
    GAZE_TARGET_HORIZON       /**< Horizon reference */
} gaze_target_type_t;

/**
 * @brief Head state
 */
typedef struct {
    /* Orientation (relative to body) */
    float yaw_rad;            /**< Head yaw relative to body */
    float pitch_rad;          /**< Head pitch relative to body */
    float roll_rad;           /**< Head roll relative to body */

    /* Angular velocity */
    float yaw_rate;           /**< Yaw rate (rad/s) */
    float pitch_rate;         /**< Pitch rate (rad/s) */
    float roll_rate;          /**< Roll rate (rad/s) */

    /* Limits reached */
    bool yaw_limit;           /**< At yaw limit */
    bool pitch_limit;         /**< At pitch limit */
} head_state_t;

/**
 * @brief Body state (for compensation)
 */
typedef struct {
    /* Body orientation */
    float yaw_rad;            /**< Body yaw (world frame) */
    float pitch_rad;          /**< Body pitch */
    float roll_rad;           /**< Body roll */

    /* Body angular velocity */
    float yaw_rate;           /**< Body yaw rate */
    float pitch_rate;         /**< Body pitch rate */
    float roll_rate;          /**< Body roll rate */

    /* Planned maneuver */
    float planned_yaw_accel;  /**< Planned yaw acceleration */
    float planned_pitch_accel;/**< Planned pitch acceleration */
    float planned_roll_accel; /**< Planned roll acceleration */
} body_state_t;

/**
 * @brief Gaze target
 */
typedef struct {
    gaze_target_type_t type;  /**< Target type */
    float position[3];        /**< Target position (world) */
    float velocity[3];        /**< Target velocity */
    float priority;           /**< Target priority [0,1] */
    bool is_moving;           /**< Target is moving */
    uint64_t last_update_us;  /**< Last update time */
} gaze_target_t;

/**
 * @brief Gaze command output
 */
typedef struct {
    /* Head command */
    float yaw_cmd_rad;        /**< Commanded head yaw */
    float pitch_cmd_rad;      /**< Commanded head pitch */
    float roll_cmd_rad;       /**< Commanded head roll */

    /* Rates */
    float yaw_rate_cmd;       /**< Commanded yaw rate */
    float pitch_rate_cmd;     /**< Commanded pitch rate */

    /* Status */
    gaze_mode_t mode;         /**< Current gaze mode */
    float tracking_error_rad; /**< Angular error to target */
    float lock_quality;       /**< Lock quality [0,1] */

    /* Predictions */
    float predicted_target_az;/**< Predicted target azimuth */
    float predicted_target_el;/**< Predicted target elevation */
} gaze_command_t;

/**
 * @brief Saccade parameters
 */
typedef struct {
    float target_az_rad;      /**< Target azimuth */
    float target_el_rad;      /**< Target elevation */
    float duration_ms;        /**< Saccade duration */
    float peak_velocity;      /**< Peak angular velocity */
    bool in_progress;         /**< Saccade in progress */
    float progress;           /**< Saccade progress [0,1] */
} saccade_state_t;

/**
 * @brief Gaze configuration
 */
typedef struct {
    /* Head limits */
    float max_yaw_rad;            /**< Maximum head yaw */
    float max_pitch_up_rad;       /**< Maximum pitch up */
    float max_pitch_down_rad;     /**< Maximum pitch down */
    float max_roll_rad;           /**< Maximum head roll */

    /* Movement limits */
    float max_yaw_rate;           /**< Maximum yaw rate (rad/s) */
    float max_pitch_rate;         /**< Maximum pitch rate */
    float max_roll_rate;          /**< Maximum roll rate */

    /* Tracking gains */
    float pursuit_gain;           /**< Smooth pursuit gain */
    float vor_gain;               /**< Vestibular-ocular reflex gain */
    float prediction_horizon_ms;  /**< Prediction time horizon */

    /* Saccade parameters */
    float saccade_threshold_rad;  /**< Error to trigger saccade */
    float saccade_latency_ms;     /**< Saccade latency */
    float saccade_duration_ms;    /**< Typical saccade duration */

    /* Stabilization */
    float stabilization_gain;     /**< Horizon stabilization gain */
    bool enable_predictive;       /**< Enable predictive tracking */
    bool enable_vor;              /**< Enable vestibular-ocular reflex */
} gaze_config_t;

/**
 * @brief Gaze statistics
 */
typedef struct {
    uint64_t updates;             /**< Total updates */
    uint64_t saccades;            /**< Total saccades */
    uint64_t target_locks;        /**< Times locked on target */
    float avg_tracking_error_rad; /**< Average tracking error */
    float avg_lock_quality;       /**< Average lock quality */
    float time_on_target_s;       /**< Total time on target */
    uint32_t target_losses;       /**< Times target was lost */
} gaze_stats_t;

//=============================================================================
// Configuration Functions
//=============================================================================

/**
 * @brief Get default gaze configuration
 */
gaze_config_t gaze_default_config(void);

/**
 * @brief Validate gaze configuration
 */
bool gaze_validate_config(const gaze_config_t* config);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create gaze system
 */
dragonfly_gaze_t dragonfly_gaze_create(const gaze_config_t* config);

/**
 * @brief Destroy gaze system
 */
void dragonfly_gaze_destroy(dragonfly_gaze_t gaze);

/**
 * @brief Reset gaze system
 */
int dragonfly_gaze_reset(dragonfly_gaze_t gaze);

//=============================================================================
// Target Functions
//=============================================================================

/**
 * @brief Set primary gaze target
 */
int dragonfly_gaze_set_target(
    dragonfly_gaze_t gaze,
    const gaze_target_t* target
);

/**
 * @brief Update target position
 */
int dragonfly_gaze_update_target(
    dragonfly_gaze_t gaze,
    const float position[3],
    const float velocity[3]
);

/**
 * @brief Clear target (free gaze)
 */
int dragonfly_gaze_clear_target(dragonfly_gaze_t gaze);

/**
 * @brief Lock gaze on target
 */
int dragonfly_gaze_lock(dragonfly_gaze_t gaze);

/**
 * @brief Release gaze lock
 */
int dragonfly_gaze_unlock(dragonfly_gaze_t gaze);

//=============================================================================
// Update Functions
//=============================================================================

/**
 * @brief Update gaze system
 *
 * @param gaze Gaze system handle
 * @param body_state Current body state (for VOR compensation)
 * @param self_position Self position in world
 * @param dt_s Time step
 * @param command Output gaze command
 * @return 0 on success, -1 on error
 */
int dragonfly_gaze_update(
    dragonfly_gaze_t gaze,
    const body_state_t* body_state,
    const float self_position[3],
    float dt_s,
    gaze_command_t* command
);

/**
 * @brief Trigger saccade to new target
 */
int dragonfly_gaze_saccade_to(
    dragonfly_gaze_t gaze,
    float target_az_rad,
    float target_el_rad
);

/**
 * @brief Set stabilization mode
 */
int dragonfly_gaze_set_mode(dragonfly_gaze_t gaze, gaze_mode_t mode);

//=============================================================================
// Query Functions
//=============================================================================

/**
 * @brief Get current head state
 */
int dragonfly_gaze_get_head_state(
    const dragonfly_gaze_t gaze,
    head_state_t* state
);

/**
 * @brief Get current gaze direction (world frame)
 */
int dragonfly_gaze_get_direction(
    const dragonfly_gaze_t gaze,
    float direction[3]
);

/**
 * @brief Get current gaze mode
 */
gaze_mode_t dragonfly_gaze_get_mode(const dragonfly_gaze_t gaze);

/**
 * @brief Check if gaze is locked on target
 */
bool dragonfly_gaze_is_locked(const dragonfly_gaze_t gaze);

/**
 * @brief Get tracking error
 */
float dragonfly_gaze_get_error(const dragonfly_gaze_t gaze);

/**
 * @brief Get gaze statistics
 */
int dragonfly_gaze_get_stats(
    const dragonfly_gaze_t gaze,
    gaze_stats_t* stats
);

/**
 * @brief Get gaze mode name
 */
const char* dragonfly_gaze_mode_name(gaze_mode_t mode);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_DRAGONFLY_GAZE_H */
