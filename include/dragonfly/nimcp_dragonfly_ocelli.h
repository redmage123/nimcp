/**
 * @file nimcp_dragonfly_ocelli.h
 * @brief Ocelli-based Attitude Stabilization System
 *
 * BIOLOGICAL REFERENCE:
 * Dragonflies possess three ocelli (simple eyes) on top of the head that
 * detect the horizon and provide rapid attitude stabilization during flight.
 * These complement the compound eyes for hunting.
 *
 * WHAT: Implements horizon detection and attitude correction
 * WHY:  Maintains stable flight platform during high-speed pursuit maneuvers
 * HOW:  Processes ocelli input to generate compensatory motor commands
 *
 * @author NIMCP Team
 * @date 2024-12-29
 */

#ifndef NIMCP_DRAGONFLY_OCELLI_H
#define NIMCP_DRAGONFLY_OCELLI_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

//=============================================================================
// Forward Declarations
//=============================================================================

typedef struct dragonfly_ocelli_s* dragonfly_ocelli_t;

//=============================================================================
// Constants
//=============================================================================

#define OCELLI_NUM_SENSORS 3      /**< Median + 2 lateral ocelli */
#define OCELLI_UPDATE_RATE_HZ 200 /**< Fast update for stabilization */

//=============================================================================
// Type Definitions
//=============================================================================

/**
 * @brief Individual ocellus type
 */
typedef enum {
    OCELLUS_MEDIAN,    /**< Top center - pitch detection */
    OCELLUS_LEFT,      /**< Left lateral - roll detection */
    OCELLUS_RIGHT      /**< Right lateral - roll detection */
} ocellus_type_t;

/**
 * @brief Ocelli input (simulated light intensity)
 */
typedef struct {
    float intensity[OCELLI_NUM_SENSORS];  /**< Light intensity [0,1] per ocellus */
    uint64_t timestamp_us;                /**< Measurement timestamp */
} ocelli_input_t;

/**
 * @brief Attitude estimate from ocelli
 */
typedef struct {
    float pitch_rad;        /**< Pitch angle (nose up positive) */
    float roll_rad;         /**< Roll angle (right wing down positive) */
    float pitch_rate;       /**< Pitch rate (rad/s) */
    float roll_rate;        /**< Roll rate (rad/s) */
    float confidence;       /**< Estimate confidence [0,1] */
    uint64_t timestamp_us;  /**< Estimate timestamp */
} ocelli_attitude_t;

/**
 * @brief Stabilization correction command
 */
typedef struct {
    float pitch_correction_rad;  /**< Desired pitch correction */
    float roll_correction_rad;   /**< Desired roll correction */
    float urgency;               /**< Correction urgency [0,1] */
    bool is_valid;               /**< Command validity flag */
} ocelli_correction_t;

/**
 * @brief Ocelli configuration
 */
typedef struct {
    /* Sensor geometry */
    float median_elevation_rad;      /**< Median ocellus elevation angle */
    float lateral_separation_rad;    /**< Angle between lateral ocelli */

    /* Processing */
    float horizon_threshold;         /**< Threshold for horizon detection */
    float smoothing_factor;          /**< Low-pass filter coefficient */

    /* Control gains */
    float pitch_gain;                /**< Pitch correction gain */
    float roll_gain;                 /**< Roll correction gain */
    float rate_damping;              /**< Rate damping factor */

    /* Limits */
    float max_pitch_correction_rad;  /**< Maximum pitch correction */
    float max_roll_correction_rad;   /**< Maximum roll correction */

    /* Integration */
    bool enable_rate_estimation;     /**< Enable rate estimation */
    bool enable_predictive;          /**< Enable predictive correction */
    float prediction_horizon_ms;     /**< Prediction time horizon */
} ocelli_config_t;

/**
 * @brief Ocelli statistics
 */
typedef struct {
    uint64_t updates_processed;      /**< Total updates */
    uint64_t corrections_issued;     /**< Corrections generated */
    float avg_pitch_error_rad;       /**< Average pitch error */
    float avg_roll_error_rad;        /**< Average roll error */
    float max_pitch_error_rad;       /**< Maximum pitch error seen */
    float max_roll_error_rad;        /**< Maximum roll error seen */
    float avg_response_time_us;      /**< Average processing time */
} ocelli_stats_t;

//=============================================================================
// Configuration Functions
//=============================================================================

/**
 * @brief Get default ocelli configuration
 */
ocelli_config_t ocelli_default_config(void);

/**
 * @brief Validate ocelli configuration
 */
bool ocelli_validate_config(const ocelli_config_t* config);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create ocelli system
 */
dragonfly_ocelli_t dragonfly_ocelli_create(const ocelli_config_t* config);

/**
 * @brief Destroy ocelli system
 */
void dragonfly_ocelli_destroy(dragonfly_ocelli_t ocelli);

/**
 * @brief Reset ocelli system
 */
int dragonfly_ocelli_reset(dragonfly_ocelli_t ocelli);

//=============================================================================
// Processing Functions
//=============================================================================

/**
 * @brief Process ocelli input and update attitude estimate
 *
 * @param ocelli Ocelli system handle
 * @param input Ocelli sensor readings
 * @return 0 on success, -1 on error
 */
int dragonfly_ocelli_process(
    dragonfly_ocelli_t ocelli,
    const ocelli_input_t* input
);

/**
 * @brief Get current attitude estimate
 *
 * @param ocelli Ocelli system handle
 * @param attitude Output attitude estimate
 * @return 0 on success, -1 on error
 */
int dragonfly_ocelli_get_attitude(
    const dragonfly_ocelli_t ocelli,
    ocelli_attitude_t* attitude
);

/**
 * @brief Get stabilization correction command
 *
 * @param ocelli Ocelli system handle
 * @param target_pitch Target pitch angle (0 = level)
 * @param target_roll Target roll angle (0 = level)
 * @param correction Output correction command
 * @return 0 on success, -1 on error
 */
int dragonfly_ocelli_get_correction(
    const dragonfly_ocelli_t ocelli,
    float target_pitch,
    float target_roll,
    ocelli_correction_t* correction
);

/**
 * @brief Simulate ocelli input from virtual horizon
 *
 * @param pitch_rad Current pitch angle
 * @param roll_rad Current roll angle
 * @param sun_elevation_rad Sun elevation angle
 * @param input Output simulated ocelli input
 * @return 0 on success, -1 on error
 */
int dragonfly_ocelli_simulate_input(
    float pitch_rad,
    float roll_rad,
    float sun_elevation_rad,
    ocelli_input_t* input
);

//=============================================================================
// Statistics and Configuration
//=============================================================================

/**
 * @brief Get ocelli statistics
 */
int dragonfly_ocelli_get_stats(
    const dragonfly_ocelli_t ocelli,
    ocelli_stats_t* stats
);

/**
 * @brief Reset ocelli statistics
 */
int dragonfly_ocelli_reset_stats(dragonfly_ocelli_t ocelli);

/**
 * @brief Update ocelli configuration
 */
int dragonfly_ocelli_set_config(
    dragonfly_ocelli_t ocelli,
    const ocelli_config_t* config
);

/**
 * @brief Get current ocelli configuration
 */
int dragonfly_ocelli_get_config(
    const dragonfly_ocelli_t ocelli,
    ocelli_config_t* config
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_DRAGONFLY_OCELLI_H */
