/**
 * @file nimcp_tactile_motor_bridge.h
 * @brief Tactile-Motor Integration Bridge (Somatosensory-Motor Coordination)
 * @version 1.0.0
 * @date 2026-01-12
 *
 * WHAT: Bridge integrating somatosensory cortex with motor systems for
 *       coordinated touch-guided movement, haptic exploration, and
 *       sensorimotor learning.
 *
 * WHY: Touch and movement are intimately linked:
 *      - Active touch exploration requires motor control
 *      - Grip force modulation depends on tactile feedback
 *      - Object manipulation requires sensorimotor integration
 *      - Motor learning uses proprioceptive feedback
 *
 * HOW: Coordinates tactile feedback with motor commands, implements grip
 *      force control loops, supports haptic exploration strategies, and
 *      enables sensorimotor prediction.
 *
 * BIOLOGICAL BASIS:
 * =================
 * - Primary motor cortex (M1) adjacent to S1
 * - Dense cortico-cortical connections
 * - Premotor and supplementary motor areas
 * - Basal ganglia motor loops
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_TACTILE_MOTOR_BRIDGE_H
#define NIMCP_TACTILE_MOTOR_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "core/brain/regions/somatosensory/nimcp_somatosensory.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Type Definitions
 * ============================================================================ */

/**
 * @brief Touch feedback event for grip control
 *
 * Specialized touch event structure for tactile-motor integration,
 * focusing on grip feedback parameters.
 */
typedef struct {
    float pressure;                 /**< Contact pressure (force per area) */
    float slip_velocity;            /**< Slip velocity (tangential movement) */
    float normal_force;             /**< Normal force component */
    float tangential_force;         /**< Tangential force component */
    float contact_area;             /**< Contact area estimate */
    float temperature;              /**< Object temperature */
    body_segment_t segment;         /**< Body segment (e.g., finger) */
    uint64_t timestamp_us;          /**< Event timestamp */
} touch_event_t;

/* ============================================================================
 * Constants
 * ============================================================================ */

#define TACTILE_MOTOR_MAX_EFFECTORS     16
#define TACTILE_MOTOR_GRIP_GAIN         0.5f
#define TACTILE_MOTOR_SLIP_THRESHOLD    0.1f

/* ============================================================================
 * Enumerations
 * ============================================================================ */

typedef enum {
    TACTILE_MOTOR_MODE_IDLE = 0,
    TACTILE_MOTOR_MODE_REACHING,
    TACTILE_MOTOR_MODE_GRASPING,
    TACTILE_MOTOR_MODE_MANIPULATING,
    TACTILE_MOTOR_MODE_EXPLORING,
    TACTILE_MOTOR_MODE_RELEASING
} tactile_motor_mode_t;

typedef enum {
    TACTILE_MOTOR_STATUS_IDLE = 0,
    TACTILE_MOTOR_STATUS_ACTIVE,
    TACTILE_MOTOR_STATUS_ADJUSTING,
    TACTILE_MOTOR_STATUS_ERROR
} tactile_motor_status_t;

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/**
 * @brief Grip state
 */
typedef struct {
    float grip_force;                   /**< Current grip force */
    float slip_margin;                  /**< Slip safety margin */
    float object_weight_estimate;       /**< Estimated object weight */
    float friction_estimate;            /**< Estimated friction */
    bool is_slipping;                   /**< Slip detected */
    bool stable_grasp;                  /**< Grasp is stable */
} tactile_motor_grip_t;

/**
 * @brief Motor command with tactile feedback
 */
typedef struct {
    uint32_t effector_id;               /**< Target effector */
    float* command;                     /**< Motor command */
    uint32_t command_dim;               /**< Command dimensionality */
    float* predicted_feedback;          /**< Predicted tactile feedback */
    float* actual_feedback;             /**< Actual tactile feedback */
    float prediction_error;             /**< Feedback prediction error */
} tactile_motor_command_t;

/**
 * @brief Exploration trajectory
 */
typedef struct {
    float* waypoints;                   /**< Exploration waypoints */
    uint32_t num_waypoints;             /**< Number of waypoints */
    uint32_t current_waypoint;          /**< Current waypoint index */
    float* collected_samples;           /**< Collected tactile samples */
    uint32_t num_samples;               /**< Number of samples */
    bool exploration_complete;          /**< Exploration finished */
} tactile_motor_exploration_t;

/* ============================================================================
 * Configuration
 * ============================================================================ */

typedef struct {
    uint32_t max_effectors;
    float grip_gain;
    float slip_threshold;
    float exploration_speed;
    float force_limit;
    bool enable_slip_detection;
    bool enable_force_control;
    bool enable_exploration;
    bool enable_logging;
} tactile_motor_config_t;

typedef struct {
    uint64_t commands_sent;
    uint64_t feedback_received;
    uint64_t slip_events;
    uint64_t grip_adjustments;
    uint64_t explorations_completed;
    float avg_prediction_error;
    float avg_grip_force;
} tactile_motor_stats_t;

/* ============================================================================
 * Handle
 * ============================================================================ */

typedef struct tactile_motor_bridge_struct tactile_motor_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

int tactile_motor_default_config(tactile_motor_config_t* config);
tactile_motor_bridge_t* tactile_motor_bridge_create(const tactile_motor_config_t* config);
void tactile_motor_bridge_destroy(tactile_motor_bridge_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

int tactile_motor_connect(tactile_motor_bridge_t* bridge, nimcp_somatosensory_t* soma, void* motor_cortex);
int tactile_motor_disconnect(tactile_motor_bridge_t* bridge);
bool tactile_motor_is_connected(const tactile_motor_bridge_t* bridge);

/* ============================================================================
 * Grip Control API
 * ============================================================================ */

int tactile_motor_init_grasp(tactile_motor_bridge_t* bridge, uint32_t effector);
int tactile_motor_update_grip(tactile_motor_bridge_t* bridge, const touch_event_t* feedback, tactile_motor_grip_t* grip);
int tactile_motor_adjust_grip_force(tactile_motor_bridge_t* bridge, float target_force);
int tactile_motor_detect_slip(tactile_motor_bridge_t* bridge, const touch_event_t* feedback, bool* slipping);
int tactile_motor_release_grasp(tactile_motor_bridge_t* bridge);

/* ============================================================================
 * Motor Command API
 * ============================================================================ */

int tactile_motor_send_command(tactile_motor_bridge_t* bridge, const tactile_motor_command_t* cmd);
int tactile_motor_receive_feedback(tactile_motor_bridge_t* bridge, uint32_t effector, float* feedback);
int tactile_motor_compute_prediction_error(tactile_motor_bridge_t* bridge, tactile_motor_command_t* cmd);

/* ============================================================================
 * Exploration API
 * ============================================================================ */

int tactile_motor_start_exploration(tactile_motor_bridge_t* bridge, const float* start_pos, const float* bounds, tactile_motor_exploration_t* exploration);
int tactile_motor_step_exploration(tactile_motor_bridge_t* bridge, tactile_motor_exploration_t* exploration, touch_event_t* sample);
int tactile_motor_finish_exploration(tactile_motor_bridge_t* bridge, tactile_motor_exploration_t* exploration);

/* ============================================================================
 * Mode Control API
 * ============================================================================ */

int tactile_motor_set_mode(tactile_motor_bridge_t* bridge, tactile_motor_mode_t mode);
tactile_motor_mode_t tactile_motor_get_mode(const tactile_motor_bridge_t* bridge);
tactile_motor_status_t tactile_motor_get_status(const tactile_motor_bridge_t* bridge);

/* ============================================================================
 * Statistics API
 * ============================================================================ */

int tactile_motor_get_stats(const tactile_motor_bridge_t* bridge, tactile_motor_stats_t* stats);
int tactile_motor_reset_stats(tactile_motor_bridge_t* bridge);
void tactile_motor_print_summary(const tactile_motor_bridge_t* bridge);

/* ============================================================================
 * Cleanup
 * ============================================================================ */

void tactile_motor_exploration_free(tactile_motor_exploration_t* exploration);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_TACTILE_MOTOR_BRIDGE_H */
