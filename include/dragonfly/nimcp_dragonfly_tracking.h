/**
 * @file nimcp_dragonfly_tracking.h
 * @brief CSTMD1-Inspired Target Tracking Module
 *
 * WHAT: Selective attention system that locks onto and tracks a single target
 * WHY:  Dragonflies use CSTMD1 neurons for winner-take-all target selection
 * HOW:  State machine with distractor suppression and predictive continuation
 *
 * BIOLOGICAL REFERENCE:
 * - Wiederman & O'Carroll (2013) "Selective attention in an insect visual neuron"
 * - Nordstrom & O'Carroll (2006) "Small object detection neurons in dragonflies"
 *
 * KEY FEATURES:
 * - Winner-take-all: Only ONE target tracked at a time (like CSTMD1)
 * - Distractor suppression: Non-locked targets get reduced gain
 * - Predictive continuation: Maintains track during brief occlusions
 * - Motion camouflage detection: Detect targets on collision course
 *
 * @author NIMCP Team
 * @date 2024-12-27
 */

#ifndef NIMCP_DRAGONFLY_TRACKING_H
#define NIMCP_DRAGONFLY_TRACKING_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

//=============================================================================
// Forward Declarations
//=============================================================================

typedef struct dragonfly_tracker_s dragonfly_tracker_t;

//=============================================================================
// Constants
//=============================================================================

#define TRACKER_MAX_OBSERVATIONS 32    /**< Maximum targets per update */
#define TRACKER_HISTORY_SIZE 64        /**< Position history buffer size */

//=============================================================================
// Type Definitions
//=============================================================================

/**
 * @brief Tracking state machine states
 *
 * State transitions:
 *   SEARCHING -> ACQUIRING (target detected above threshold)
 *   ACQUIRING -> LOCKED (confidence above lock threshold)
 *   ACQUIRING -> SEARCHING (target lost during acquisition)
 *   LOCKED -> PREDICTING (target occluded)
 *   LOCKED -> LOST (confidence below break threshold)
 *   PREDICTING -> LOCKED (target reacquired)
 *   PREDICTING -> LOST (occlusion timeout exceeded)
 *   LOST -> SEARCHING (automatic)
 */
typedef enum {
    TRACK_STATE_SEARCHING,    /**< No target locked, scanning for candidates */
    TRACK_STATE_ACQUIRING,    /**< Target detected, building lock confidence */
    TRACK_STATE_LOCKED,       /**< Actively tracking locked target */
    TRACK_STATE_PREDICTING,   /**< Target occluded, using prediction */
    TRACK_STATE_LOST          /**< Lock broken, transitioning to search */
} track_state_t;

/**
 * @brief Motion model for prediction during occlusion
 */
typedef enum {
    MOTION_MODEL_CONSTANT_VELOCITY,   /**< Straight-line motion (default) */
    MOTION_MODEL_CONSTANT_ACCEL,      /**< Smooth curves with acceleration */
    MOTION_MODEL_SINGER               /**< Maneuvering target (random accel) */
} motion_model_t;

/**
 * @brief Target observation from sensor
 */
typedef struct {
    uint32_t target_id;           /**< Unique target identifier (0 = unknown) */
    float position[3];            /**< Position (x, y, z) in world coordinates */
    float velocity[3];            /**< Velocity estimate (vx, vy, vz) if known */
    float size;                   /**< Apparent size (angular or metric) */
    float contrast;               /**< Visual contrast [0,1] */
    float confidence;             /**< Detection confidence [0,1] */
    uint64_t timestamp_us;        /**< Observation timestamp (microseconds) */
} target_observation_t;

/**
 * @brief Currently tracked target state
 */
typedef struct {
    uint32_t target_id;           /**< Unique target identifier */
    float position[3];            /**< Filtered position (x, y, z) */
    float velocity[3];            /**< Estimated velocity (vx, vy, vz) */
    float acceleration[3];        /**< Estimated acceleration (ax, ay, az) */
    float size;                   /**< Filtered apparent size */
    float confidence;             /**< Current tracking confidence [0,1] */
    track_state_t state;          /**< Current tracking state */
    uint64_t lock_time_us;        /**< Duration of lock (microseconds) */
    uint64_t last_seen_us;        /**< When target was last observed */
    uint32_t observations;        /**< Total observations of this target */
    bool is_approaching;          /**< Target on collision course */
    float time_to_contact;        /**< Estimated time to contact (seconds) */
} tracked_target_t;

/**
 * @brief Tracker configuration
 */
typedef struct {
    /* Lock thresholds */
    float acquisition_threshold;      /**< Confidence to start acquisition [0,1] */
    float lock_threshold;             /**< Confidence to acquire lock [0,1] */
    float break_threshold;            /**< Confidence to break lock [0,1] */

    /* Timing parameters */
    float acquisition_time_ms;        /**< Time required to acquire lock */
    float prediction_horizon_ms;      /**< How far ahead to predict during occlusion */
    uint32_t max_occlusion_ms;        /**< Max time to maintain prediction */
    float lost_timeout_ms;            /**< Time in LOST state before SEARCHING */

    /* Attention parameters */
    float distractor_suppression;     /**< Gain reduction for non-targets [0,1] */
    float attention_radius;           /**< Spatial attention window radius */
    bool enable_motion_camouflage;    /**< Detect targets on collision course */

    /* Filter parameters */
    float process_noise;              /**< Motion model uncertainty (Q) */
    float measurement_noise;          /**< Observation uncertainty (R) */
    motion_model_t motion_model;      /**< Prediction motion model */

    /* Size selectivity (small target motion detector) */
    float min_target_size;            /**< Minimum detectable target size */
    float max_target_size;            /**< Maximum detectable target size */
    float optimal_target_size;        /**< Peak response target size */
} tracking_config_t;

/**
 * @brief Tracker statistics
 */
typedef struct {
    uint64_t total_observations;      /**< Total observations processed */
    uint64_t successful_locks;        /**< Number of successful lock acquisitions */
    uint64_t lock_breaks;             /**< Number of lock breaks */
    uint64_t prediction_uses;         /**< Times prediction was used */
    uint64_t reacquisitions;          /**< Times target was reacquired */
    float avg_lock_duration_ms;       /**< Average lock duration */
    float avg_confidence;             /**< Average tracking confidence */
    float max_prediction_error;       /**< Maximum prediction error observed */
    uint64_t distractors_suppressed;  /**< Distractor observations suppressed */
} tracking_stats_t;

//=============================================================================
// Configuration Functions
//=============================================================================

/**
 * @brief Get default tracking configuration
 *
 * WHAT: Returns biologically-plausible default configuration
 * WHY:  Based on dragonfly CSTMD1 neuron characteristics
 *
 * @return Default configuration structure
 */
tracking_config_t tracking_default_config(void);

/**
 * @brief Validate tracking configuration
 *
 * @param config Configuration to validate
 * @return true if valid, false otherwise
 */
bool tracking_validate_config(const tracking_config_t* config);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create a target tracker
 *
 * WHAT: Allocates and initializes tracker with given configuration
 * WHY:  Main entry point for creating tracking capability
 *
 * @param config Configuration (NULL for defaults)
 * @return Tracker handle or NULL on failure
 */
dragonfly_tracker_t* dragonfly_tracker_create(const tracking_config_t* config);

/**
 * @brief Destroy a tracker and free resources
 *
 * @param tracker Tracker handle (NULL-safe)
 */
void dragonfly_tracker_destroy(dragonfly_tracker_t* tracker);

/**
 * @brief Reset tracker to initial state
 *
 * @param tracker Tracker handle
 * @return 0 on success, -1 on error
 */
int dragonfly_tracker_reset(dragonfly_tracker_t* tracker);

//=============================================================================
// Core Tracking Functions
//=============================================================================

/**
 * @brief Process new observations and update tracking
 *
 * WHAT: Main tracking update function - processes observations, updates state
 * WHY:  Called each frame with new sensor observations
 * HOW:  Winner-take-all selection, Kalman filtering, state machine update
 *
 * @param tracker Tracker handle
 * @param observations Array of target observations
 * @param num_observations Number of observations
 * @param dt Time delta since last update (seconds)
 * @return 0 on success, -1 on error
 */
int dragonfly_tracker_update(
    dragonfly_tracker_t* tracker,
    const target_observation_t* observations,
    uint32_t num_observations,
    float dt
);

/**
 * @brief Predict target position at future time
 *
 * @param tracker Tracker handle
 * @param lookahead_ms Time ahead to predict (milliseconds)
 * @param position Output: predicted position [3]
 * @param velocity Output: predicted velocity [3] (can be NULL)
 * @return 0 on success, -1 on error (no lock or invalid input)
 */
int dragonfly_tracker_predict(
    dragonfly_tracker_t* tracker,
    float lookahead_ms,
    float position[3],
    float velocity[3]
);

/**
 * @brief Force lock onto specific target
 *
 * @param tracker Tracker handle
 * @param target_id Target ID to lock onto
 * @return 0 on success, -1 if target not currently observed
 */
int dragonfly_tracker_force_lock(
    dragonfly_tracker_t* tracker,
    uint32_t target_id
);

/**
 * @brief Break current lock and return to searching
 *
 * @param tracker Tracker handle
 * @return 0 on success, -1 on error
 */
int dragonfly_tracker_break_lock(dragonfly_tracker_t* tracker);

//=============================================================================
// Query Functions
//=============================================================================

/**
 * @brief Get currently tracked target
 *
 * @param tracker Tracker handle
 * @return Pointer to tracked target state, or NULL if not tracking
 *
 * @note Returned pointer is valid until next update call
 */
const tracked_target_t* dragonfly_tracker_get_target(
    const dragonfly_tracker_t* tracker
);

/**
 * @brief Get current tracking state
 *
 * @param tracker Tracker handle
 * @return Current state (TRACK_STATE_SEARCHING if invalid)
 */
track_state_t dragonfly_tracker_get_state(const dragonfly_tracker_t* tracker);

/**
 * @brief Get attention gain for a specific target
 *
 * WHAT: Returns attention-modulated gain for a target
 * WHY:  Locked target gets full gain, distractors are suppressed
 *
 * @param tracker Tracker handle
 * @param target_id Target ID to query
 * @return Gain [0,1] where 1 = full attention, 0 = fully suppressed
 */
float dragonfly_tracker_get_gain(
    const dragonfly_tracker_t* tracker,
    uint32_t target_id
);

/**
 * @brief Get tracking statistics
 *
 * @param tracker Tracker handle
 * @param stats Output: statistics structure
 * @return 0 on success, -1 on error
 */
int dragonfly_tracker_get_stats(
    const dragonfly_tracker_t* tracker,
    tracking_stats_t* stats
);

/**
 * @brief Reset tracking statistics
 *
 * @param tracker Tracker handle
 * @return 0 on success, -1 on error
 */
int dragonfly_tracker_reset_stats(dragonfly_tracker_t* tracker);

/**
 * @brief Check if tracker is actively locked on a target
 *
 * @param tracker Tracker handle
 * @return true if LOCKED or PREDICTING state
 */
bool dragonfly_tracker_is_locked(const dragonfly_tracker_t* tracker);

/**
 * @brief Get locked target ID
 *
 * @param tracker Tracker handle
 * @return Target ID or 0 if not locked
 */
uint32_t dragonfly_tracker_get_locked_id(const dragonfly_tracker_t* tracker);

//=============================================================================
// Configuration Functions
//=============================================================================

/**
 * @brief Update configuration (takes effect on next update)
 *
 * @param tracker Tracker handle
 * @param config New configuration
 * @return 0 on success, -1 on invalid config
 */
int dragonfly_tracker_set_config(
    dragonfly_tracker_t* tracker,
    const tracking_config_t* config
);

/**
 * @brief Get current configuration
 *
 * @param tracker Tracker handle
 * @param config Output: current configuration
 * @return 0 on success, -1 on error
 */
int dragonfly_tracker_get_config(
    const dragonfly_tracker_t* tracker,
    tracking_config_t* config
);

//=============================================================================
// Advanced Functions
//=============================================================================

/**
 * @brief Get position history for locked target
 *
 * @param tracker Tracker handle
 * @param positions Output: array of position triplets [x,y,z,x,y,z,...]
 * @param max_positions Maximum positions to return
 * @param count Output: actual number of positions
 * @return 0 on success, -1 on error
 */
int dragonfly_tracker_get_history(
    const dragonfly_tracker_t* tracker,
    float* positions,
    uint32_t max_positions,
    uint32_t* count
);

/**
 * @brief Get prediction confidence (decreases during occlusion)
 *
 * @param tracker Tracker handle
 * @return Confidence [0,1], 0 if not locked
 */
float dragonfly_tracker_get_prediction_confidence(
    const dragonfly_tracker_t* tracker
);

/**
 * @brief Set external velocity estimate (from sensor fusion)
 *
 * @param tracker Tracker handle
 * @param velocity External velocity estimate [3]
 * @param confidence Estimate confidence [0,1]
 * @return 0 on success, -1 on error
 */
int dragonfly_tracker_set_external_velocity(
    dragonfly_tracker_t* tracker,
    const float velocity[3],
    float confidence
);

//=============================================================================
// State Name Utility
//=============================================================================

/**
 * @brief Get human-readable state name
 *
 * @param state Tracking state
 * @return State name string
 */
const char* dragonfly_tracker_state_name(track_state_t state);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_DRAGONFLY_TRACKING_H */
