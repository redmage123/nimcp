/**
 * @file nimcp_dragonfly.h
 * @brief Main Dragonfly Coordinator Module
 *
 * WHAT: Unified dragonfly-inspired target tracking and interception system
 * WHY:  Dragonflies achieve 95% hunting success using coordinated neural circuits
 * HOW:  Integrates TSDN, tracking, prediction, and interception subsystems
 *
 * BIOLOGICAL REFERENCE:
 * - Gonzalez-Bellido et al. (2013) "Eight pairs of descending neurons"
 * - Mischiati et al. (2015) "Internal models direct dragonfly interception"
 * - Wiederman & O'Carroll (2017) "Computational principles of STMD neurons"
 *
 * PIPELINE:
 * 1. TSDN: Encode target direction as population vector (16 neurons)
 * 2. CSTMD1: Lock attention on single target (winner-take-all)
 * 3. Prediction: Estimate trajectory using IMM filter
 * 4. Interception: Compute optimal pursuit using PN guidance
 *
 * @author NIMCP Team
 * @date 2024-12-27
 */

#ifndef NIMCP_DRAGONFLY_H
#define NIMCP_DRAGONFLY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "dragonfly/nimcp_dragonfly_tsdn.h"
#include "dragonfly/nimcp_dragonfly_tracking.h"
#include "dragonfly/nimcp_dragonfly_prediction.h"
#include "dragonfly/nimcp_dragonfly_intercept.h"

//=============================================================================
// Forward Declarations
//=============================================================================

typedef struct dragonfly_system_s dragonfly_system_t;

//=============================================================================
// Constants
//=============================================================================

#define DRAGONFLY_MAX_TARGETS 8       /**< Maximum simultaneous target candidates */
#define DRAGONFLY_VERSION_MAJOR 1
#define DRAGONFLY_VERSION_MINOR 0
#define DRAGONFLY_VERSION_PATCH 0

//=============================================================================
// Type Definitions
//=============================================================================

/**
 * @brief System operating mode
 */
typedef enum {
    DRAGONFLY_MODE_IDLE,          /**< Waiting for targets */
    DRAGONFLY_MODE_SCANNING,      /**< Actively scanning for targets */
    DRAGONFLY_MODE_TRACKING,      /**< Locked onto target */
    DRAGONFLY_MODE_PURSUING,      /**< Active pursuit */
    DRAGONFLY_MODE_INTERCEPTING   /**< Final interception phase */
} dragonfly_mode_t;

/**
 * @brief Hunt outcome
 */
typedef enum {
    DRAGONFLY_HUNT_IN_PROGRESS,   /**< Still pursuing */
    DRAGONFLY_HUNT_SUCCESS,       /**< Target intercepted */
    DRAGONFLY_HUNT_ESCAPED,       /**< Target escaped */
    DRAGONFLY_HUNT_ABORTED,       /**< Hunt terminated early */
    DRAGONFLY_HUNT_TIMEOUT        /**< Time limit exceeded */
} dragonfly_hunt_result_t;

/**
 * @brief Visual detection (input from visual cortex)
 */
typedef struct {
    float position[3];            /**< Position in world coordinates */
    float size;                   /**< Angular size (radians) */
    float contrast;               /**< Contrast against background [0,1] */
    float motion_direction_rad;   /**< Direction of motion */
    float motion_speed;           /**< Speed of motion */
    uint64_t timestamp_us;        /**< Detection timestamp */
    uint32_t id;                  /**< Unique detection ID */
} dragonfly_detection_t;

/**
 * @brief Motor command output
 */
typedef struct {
    float heading_rad;            /**< Desired heading angle */
    float pitch_rad;              /**< Desired pitch angle */
    float velocity[3];            /**< Desired velocity */
    float acceleration[3];        /**< Acceleration command */
    float urgency;                /**< Command urgency [0,1] */
    uint64_t timestamp_us;        /**< Command timestamp */
} dragonfly_motor_cmd_t;

/**
 * @brief Self-state (proprioception)
 */
typedef struct {
    float position[3];            /**< Current position */
    float velocity[3];            /**< Current velocity */
    float heading_rad;            /**< Current heading */
    float pitch_rad;              /**< Current pitch */
    float max_speed;              /**< Maximum achievable speed */
    float max_accel;              /**< Maximum acceleration */
    float max_turn_rate;          /**< Maximum turn rate (rad/s) */
    float energy_level;           /**< Remaining energy [0,1] */
} dragonfly_self_state_t;

/**
 * @brief Complete target information
 */
typedef struct {
    uint32_t id;                      /**< Target identifier */
    track_state_t state;              /**< Tracking state */

    /* Current estimate */
    float position[3];                /**< Estimated position */
    float velocity[3];                /**< Estimated velocity */
    float acceleration[3];            /**< Estimated acceleration */

    /* Predictions */
    float predicted_position[3];      /**< Future position */
    float time_to_intercept_s;        /**< Time to reach */

    /* Quality metrics */
    float confidence;                 /**< Overall confidence [0,1] */
    float threat_level;               /**< Threat/priority [0,1] */
    evasion_type_t evasion_type;      /**< Detected evasion maneuver */

    /* Interception */
    intercept_feasibility_t feasibility;  /**< Can we intercept? */
    float intercept_point[3];             /**< Where to intercept */
    float lead_angle_rad;                 /**< Required lead angle */
} dragonfly_target_info_t;

/**
 * @brief System configuration
 */
typedef struct {
    /* Subsystem configs */
    tsdn_config_t tsdn_config;
    tracking_config_t tracker_config;
    prediction_config_t prediction_config;
    intercept_config_t intercept_config;

    /* System parameters */
    float min_target_size;            /**< Minimum target angular size */
    float max_target_distance;        /**< Maximum engagement distance */
    float abort_distance;             /**< Distance to abort pursuit */
    float intercept_threshold;        /**< Distance for successful intercept */
    float pursuit_timeout_s;          /**< Maximum pursuit time */

    /* Mode transitions */
    float lock_threshold;             /**< Confidence to lock target */
    float pursue_threshold;           /**< Confidence to start pursuit */
    float abort_threshold;            /**< Confidence to abort */

    /* Energy management */
    bool energy_aware;                /**< Consider energy in decisions */
    float min_energy_reserve;         /**< Minimum energy to maintain */
} dragonfly_config_t;

/**
 * @brief System statistics
 */
typedef struct {
    /* Pipeline counts */
    uint64_t detections_processed;    /**< Total detections received */
    uint64_t targets_tracked;         /**< Targets that reached LOCKED */
    uint64_t pursuits_initiated;      /**< Pursuits started */
    uint64_t intercepts_attempted;    /**< Interception attempts */

    /* Outcomes */
    uint64_t successful_intercepts;   /**< Successful catches */
    uint64_t escaped_targets;         /**< Targets that escaped */
    uint64_t aborted_pursuits;        /**< Pursuits aborted */

    /* Performance */
    float avg_track_duration_s;       /**< Average tracking time */
    float avg_pursuit_duration_s;     /**< Average pursuit time */
    float success_rate;               /**< Overall success rate */
    float avg_intercept_distance;     /**< Average intercept distance */

    /* Timing */
    float avg_update_time_us;         /**< Average update cycle time */
    uint64_t total_updates;           /**< Total update cycles */

    /* Subsystem stats */
    tsdn_stats_t tsdn_stats;
    tracking_stats_t tracker_stats;
    prediction_stats_t prediction_stats;
    intercept_stats_t intercept_stats;
} dragonfly_stats_t;

/**
 * @brief Simplified system state for cognitive bridge
 */
typedef struct {
    uint32_t target_id;               /**< Currently tracked target ID */
    float confidence;                 /**< Tracking confidence [0,1] */
    float time_to_intercept_ms;       /**< Time to intercept in ms */
    bool is_tracking;                 /**< Currently tracking a target */
    bool evasion_detected;            /**< Target performing evasive maneuver */
} dragonfly_state_t;

//=============================================================================
// Configuration Functions
//=============================================================================

/**
 * @brief Get default system configuration
 */
dragonfly_config_t dragonfly_default_config(void);

/**
 * @brief Validate system configuration
 */
bool dragonfly_validate_config(const dragonfly_config_t* config);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create dragonfly system
 */
dragonfly_system_t* dragonfly_system_create(const dragonfly_config_t* config);

/**
 * @brief Destroy system and free resources
 */
void dragonfly_system_destroy(dragonfly_system_t* system);

/**
 * @brief Reset system to initial state
 */
int dragonfly_system_reset(dragonfly_system_t* system);

//=============================================================================
// Core Pipeline Functions
//=============================================================================

/**
 * @brief Process visual detection
 *
 * WHAT: Feed new detection into the pipeline
 * WHY:  Entry point for visual cortex input
 * HOW:  Updates TSDN encoding and tracking state
 *
 * @param system Dragonfly system
 * @param detection Visual detection from cortex
 * @return 0 on success, -1 on error
 */
int dragonfly_process_detection(
    dragonfly_system_t* system,
    const dragonfly_detection_t* detection
);

/**
 * @brief Update system with current self-state
 *
 * @param system Dragonfly system
 * @param self_state Current proprioceptive state
 * @return 0 on success, -1 on error
 */
int dragonfly_update_self_state(
    dragonfly_system_t* system,
    const dragonfly_self_state_t* self_state
);

/**
 * @brief Run one update cycle
 *
 * WHAT: Main processing loop iteration
 * WHY:  Advances predictions, updates interception plan
 * HOW:  Runs all subsystems in sequence
 *
 * @param system Dragonfly system
 * @param dt_s Time step in seconds
 * @return 0 on success, -1 on error
 */
int dragonfly_update(dragonfly_system_t* system, float dt_s);

/**
 * @brief Get current motor command
 *
 * @param system Dragonfly system
 * @param cmd Output: motor command
 * @return 0 on success, -1 on error
 */
int dragonfly_get_motor_command(
    const dragonfly_system_t* system,
    dragonfly_motor_cmd_t* cmd
);

//=============================================================================
// State Query Functions
//=============================================================================

/**
 * @brief Get current operating mode
 */
dragonfly_mode_t dragonfly_get_mode(const dragonfly_system_t* system);

/**
 * @brief Get hunt result (if concluded)
 */
dragonfly_hunt_result_t dragonfly_get_hunt_result(const dragonfly_system_t* system);

/**
 * @brief Get primary target info
 */
int dragonfly_get_primary_target(
    const dragonfly_system_t* system,
    dragonfly_target_info_t* target
);

/**
 * @brief Get simplified system state for cognitive bridge
 */
int dragonfly_get_state(
    const dragonfly_system_t* system,
    dragonfly_state_t* state
);

/**
 * @brief Get all tracked targets
 */
int dragonfly_get_all_targets(
    const dragonfly_system_t* system,
    dragonfly_target_info_t* targets,
    uint32_t max_targets,
    uint32_t* num_targets
);

/**
 * @brief Get TSDN population vector
 */
int dragonfly_get_tsdn_vector(
    const dragonfly_system_t* system,
    tsdn_vector_t* vector
);

/**
 * @brief Get current prediction
 */
int dragonfly_get_prediction(
    const dragonfly_system_t* system,
    trajectory_prediction_t* prediction
);

/**
 * @brief Get current interception solution
 */
int dragonfly_get_intercept_solution(
    const dragonfly_system_t* system,
    intercept_solution_t* solution
);

//=============================================================================
// Control Functions
//=============================================================================

/**
 * @brief Start scanning for targets
 */
int dragonfly_start_scan(dragonfly_system_t* system);

/**
 * @brief Lock onto specific target
 */
int dragonfly_lock_target(dragonfly_system_t* system, uint32_t target_id);

/**
 * @brief Start pursuit of locked target
 */
int dragonfly_start_pursuit(dragonfly_system_t* system);

/**
 * @brief Abort current pursuit
 */
int dragonfly_abort_pursuit(dragonfly_system_t* system);

/**
 * @brief Go idle
 */
int dragonfly_go_idle(dragonfly_system_t* system);

//=============================================================================
// Statistics and Configuration
//=============================================================================

/**
 * @brief Get system statistics
 */
int dragonfly_get_stats(
    const dragonfly_system_t* system,
    dragonfly_stats_t* stats
);

/**
 * @brief Reset statistics
 */
int dragonfly_reset_stats(dragonfly_system_t* system);

/**
 * @brief Update configuration
 */
int dragonfly_set_config(
    dragonfly_system_t* system,
    const dragonfly_config_t* config
);

/**
 * @brief Get current configuration
 */
int dragonfly_get_config(
    const dragonfly_system_t* system,
    dragonfly_config_t* config
);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get mode name
 */
const char* dragonfly_mode_name(dragonfly_mode_t mode);

/**
 * @brief Get hunt result name
 */
const char* dragonfly_hunt_result_name(dragonfly_hunt_result_t result);

/**
 * @brief Get version string
 */
const char* dragonfly_version(void);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_DRAGONFLY_H */
