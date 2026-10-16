/**
 * @file nimcp_dragonfly_tsdn.h
 * @brief Target-Selective Descending Neuron (TSDN) Population Vector Encoding
 *
 * WHAT: Population vector encoding of target direction using 16 TSDNs
 * WHY:  Dragonflies encode prey direction efficiently with 16 neurons spanning 360 degrees
 * HOW:  Each TSDN has a preferred direction; population vector = weighted sum of preferred
 *       directions by firing rates
 *
 * BIOLOGICAL BASIS:
 * - Dragonflies have 16 TSDNs (8 pairs) in descending nerve cord
 * - Each TSDN fires maximally for targets in its preferred direction
 * - Tuning curves are broad (cosine), overlap significantly
 * - Population vector readout gives precise direction from coarse neurons
 * - Olberg et al. (2007), Gonzalez-Bellido et al. (2013)
 *
 * DESIGN:
 * - 16 neurons with preferred directions at 22.5 degree intervals
 * - Cosine tuning: firing_rate = max(0, cos(angle - preferred_dir))^tuning_exponent
 * - Population vector: sum(firing_rate[i] * unit_vector(preferred_dir[i]))
 * - Supports 2D (azimuth) and 3D (azimuth + elevation) encoding
 *
 * @author NIMCP Team
 * @date 2024-12-27
 */

#ifndef NIMCP_DRAGONFLY_TSDN_H
#define NIMCP_DRAGONFLY_TSDN_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "constants/nimcp_math_constants.h"

//=============================================================================
// Constants
//=============================================================================

/** Number of TSDNs in the population (8 pairs, 22.5 degree spacing) */
#define TSDN_NEURON_COUNT 16

/** Angular spacing between adjacent TSDNs (radians) */
#define TSDN_ANGULAR_SPACING (NIMCP_TWO_PI_F / TSDN_NEURON_COUNT)

/** Default tuning width for cosine tuning (radians, ~60 degrees FWHM) */
#define TSDN_DEFAULT_TUNING_WIDTH 1.0472f

/** Default tuning exponent for sharper selectivity */
#define TSDN_DEFAULT_TUNING_EXPONENT 2.0f

/** Minimum firing rate threshold for active neuron */
#define TSDN_MIN_FIRING_RATE 0.01f

/** Maximum firing rate (normalized) */
#define TSDN_MAX_FIRING_RATE 1.0f

//=============================================================================
// Configuration Types
//=============================================================================

/**
 * @brief TSDN tuning curve type
 */
typedef enum {
    TSDN_TUNING_COSINE = 0,      /**< Cosine tuning (biological default) */
    TSDN_TUNING_GAUSSIAN,         /**< Gaussian tuning */
    TSDN_TUNING_VON_MISES         /**< Von Mises (circular Gaussian) */
} tsdn_tuning_type_t;

/**
 * @brief TSDN dimensionality mode
 */
typedef enum {
    TSDN_MODE_2D = 0,             /**< Azimuth only (horizontal plane) */
    TSDN_MODE_3D                  /**< Azimuth + elevation */
} tsdn_mode_t;

/**
 * @brief TSDN population configuration
 */
typedef struct tsdn_config_s {
    /* Tuning parameters */
    tsdn_tuning_type_t tuning_type;     /**< Type of tuning curve */
    float tuning_width;                  /**< Width parameter (radians) */
    float tuning_exponent;               /**< Exponent for cosine tuning */

    /* Dimensionality */
    tsdn_mode_t mode;                    /**< 2D or 3D mode */

    /* Noise and adaptation */
    float baseline_noise;                /**< Baseline firing noise [0,1] */
    float adaptation_rate;               /**< Firing rate adaptation (Hz) */
    bool enable_adaptation;              /**< Enable rate adaptation */

    /* Gain modulation */
    float gain;                          /**< Global gain multiplier */
    bool enable_gain_modulation;         /**< Allow external gain control */

    /* Elevation neurons (3D mode only) */
    uint32_t elevation_neurons;          /**< Number of elevation-tuned neurons */
    float elevation_range;               /**< Elevation range (radians, +/- from horizontal) */
} tsdn_config_t;

//=============================================================================
// Output Types
//=============================================================================

/**
 * @brief Decoded population vector
 *
 * WHAT: Result of population vector decoding from TSDN firing rates
 * WHY:  Provides direction, magnitude (confidence), and timing
 * HOW:  Computed from weighted sum of preferred directions
 */
typedef struct tsdn_vector_s {
    float direction;          /**< Azimuth direction (radians, 0 = forward, CCW positive) */
    float elevation;          /**< Elevation angle (radians, 0 = horizontal, up positive) */
    float magnitude;          /**< Population vector length [0,1], confidence measure */
    float angular_velocity;   /**< Estimated angular velocity (rad/s) */
    uint64_t timestamp_us;    /**< Timestamp of computation (microseconds) */
    bool valid;               /**< True if vector is valid (magnitude > threshold) */
} tsdn_vector_t;

/**
 * @brief TSDN population state
 *
 * WHAT: Current state of all 16 TSDNs
 * WHY:  Expose firing rates and preferred directions for debugging/visualization
 * HOW:  Array access to individual neuron states
 */
typedef struct tsdn_state_s {
    float firing_rate[TSDN_NEURON_COUNT];       /**< Current firing rates [0,1] */
    float preferred_direction[TSDN_NEURON_COUNT]; /**< Preferred directions (radians) */
    float adaptation[TSDN_NEURON_COUNT];        /**< Adaptation state */
    uint64_t spike_count[TSDN_NEURON_COUNT];    /**< Total spike count per neuron */
    uint64_t last_update_us;                     /**< Last update timestamp */
} tsdn_state_t;

/**
 * @brief TSDN statistics
 */
typedef struct tsdn_stats_s {
    uint64_t encode_calls;        /**< Number of encode operations */
    uint64_t decode_calls;        /**< Number of decode operations */
    uint64_t total_spikes;        /**< Total spikes across all TSDNs */
    float avg_magnitude;          /**< Average population vector magnitude */
    float avg_firing_rate;        /**< Average firing rate across TSDNs */
    float max_direction_error;    /**< Maximum encoding/decoding error (radians) */
} tsdn_stats_t;

//=============================================================================
// Main Structure
//=============================================================================

/**
 * @brief TSDN population context
 *
 * WHAT: Opaque handle to TSDN population
 * WHY:  Encapsulate internal state
 * HOW:  Created by tsdn_create, destroyed by tsdn_destroy
 */
typedef struct tsdn_population_s tsdn_population_t;

//=============================================================================
// Configuration Functions
//=============================================================================

/**
 * @brief Initialize TSDN config with biological defaults
 *
 * WHAT: Set default configuration values
 * WHY:  Convenient initialization with biologically-plausible parameters
 * HOW:  Cosine tuning, 2D mode, standard width
 *
 * @param config Configuration to initialize
 */
void tsdn_config_default(tsdn_config_t* config);

/**
 * @brief Validate TSDN configuration
 *
 * @param config Configuration to validate
 * @return 0 if valid, error code otherwise
 */
int tsdn_config_validate(const tsdn_config_t* config);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create TSDN population
 *
 * WHAT: Allocate and initialize TSDN population
 * WHY:  Set up 16 neurons with proper preferred directions
 * HOW:  Allocate context, initialize tuning curves, set initial state
 *
 * @param config Population configuration (NULL for defaults)
 * @return Population handle or NULL on failure
 */
tsdn_population_t* tsdn_create(const tsdn_config_t* config);

/**
 * @brief Destroy TSDN population
 *
 * WHAT: Free TSDN resources
 * WHY:  Clean up memory
 * HOW:  Free all allocated memory
 *
 * @param pop Population to destroy
 */
void tsdn_destroy(tsdn_population_t* pop);

/**
 * @brief Reset TSDN population to initial state
 *
 * @param pop Population to reset
 * @return 0 on success, error code on failure
 */
int tsdn_reset(tsdn_population_t* pop);

//=============================================================================
// Encoding Functions
//=============================================================================

/**
 * @brief Encode target position as TSDN firing rates
 *
 * WHAT: Convert target (x,y) position to TSDN firing pattern
 * WHY:  Map spatial position to neural representation
 * HOW:  Compute angle to target, activate TSDNs based on tuning curves
 *
 * ALGORITHM:
 * 1. Compute angle: atan2(target_y - self_y, target_x - self_x)
 * 2. For each TSDN: firing_rate = max(0, cos(angle - preferred))^exponent
 * 3. Apply gain and noise
 * 4. Update adaptation state
 *
 * @param pop TSDN population
 * @param target_x Target X position (relative to self)
 * @param target_y Target Y position (relative to self)
 * @return Decoded population vector
 */
tsdn_vector_t tsdn_encode(tsdn_population_t* pop, float target_x, float target_y);

/**
 * @brief Encode target position in 3D
 *
 * @param pop TSDN population (must be in 3D mode)
 * @param target_x Target X position
 * @param target_y Target Y position
 * @param target_z Target Z position (height)
 * @return Decoded population vector with elevation
 */
tsdn_vector_t tsdn_encode_3d(
    tsdn_population_t* pop,
    float target_x,
    float target_y,
    float target_z
);

/**
 * @brief Encode direction directly (skip position computation)
 *
 * @param pop TSDN population
 * @param direction_rad Direction in radians
 * @return Decoded population vector
 */
tsdn_vector_t tsdn_encode_direction(tsdn_population_t* pop, float direction_rad);

/**
 * @brief Encode direction with elevation
 *
 * @param pop TSDN population (must be in 3D mode)
 * @param azimuth_rad Azimuth direction (radians)
 * @param elevation_rad Elevation angle (radians)
 * @return Decoded population vector
 */
tsdn_vector_t tsdn_encode_direction_3d(
    tsdn_population_t* pop,
    float azimuth_rad,
    float elevation_rad
);

//=============================================================================
// Decoding Functions
//=============================================================================

/**
 * @brief Decode population vector from current firing rates
 *
 * WHAT: Compute direction from TSDN firing pattern
 * WHY:  Read out encoded direction from neural activity
 * HOW:  Weighted vector sum of preferred directions
 *
 * ALGORITHM:
 * 1. For each TSDN with firing_rate > threshold:
 *    vector += firing_rate * unit_vector(preferred_direction)
 * 2. Direction = atan2(vector.y, vector.x)
 * 3. Magnitude = ||vector|| / max_possible
 *
 * @param pop TSDN population
 * @return Decoded population vector
 */
tsdn_vector_t tsdn_decode(const tsdn_population_t* pop);

/**
 * @brief Decode direction from external firing rates
 *
 * WHAT: Decode without modifying population state
 * WHY:  Analyze arbitrary firing patterns
 * HOW:  Population vector computation on provided rates
 *
 * @param pop TSDN population (for preferred directions)
 * @param firing_rates External firing rates [TSDN_NEURON_COUNT]
 * @return Decoded population vector
 */
tsdn_vector_t tsdn_decode_external(
    const tsdn_population_t* pop,
    const float* firing_rates
);

//=============================================================================
// State Access Functions
//=============================================================================

/**
 * @brief Get current TSDN population state
 *
 * @param pop TSDN population
 * @param state Output: current state (copied)
 * @return 0 on success, error code on failure
 */
int tsdn_get_state(const tsdn_population_t* pop, tsdn_state_t* state);

/**
 * @brief Get firing rate of specific TSDN
 *
 * @param pop TSDN population
 * @param neuron_id Neuron index [0, TSDN_NEURON_COUNT-1]
 * @param rate Output: firing rate
 * @return 0 on success, error code on failure
 */
int tsdn_get_firing_rate(
    const tsdn_population_t* pop,
    uint32_t neuron_id,
    float* rate
);

/**
 * @brief Get preferred direction of specific TSDN
 *
 * @param pop TSDN population
 * @param neuron_id Neuron index [0, TSDN_NEURON_COUNT-1]
 * @param direction Output: preferred direction (radians)
 * @return 0 on success, error code on failure
 */
int tsdn_get_preferred_direction(
    const tsdn_population_t* pop,
    uint32_t neuron_id,
    float* direction
);

/**
 * @brief Set firing rates externally (for testing/integration)
 *
 * @param pop TSDN population
 * @param firing_rates New firing rates [TSDN_NEURON_COUNT]
 * @return 0 on success, error code on failure
 */
int tsdn_set_firing_rates(tsdn_population_t* pop, const float* firing_rates);

//=============================================================================
// Gain Control Functions
//=============================================================================

/**
 * @brief Set global gain multiplier
 *
 * WHAT: Modulate overall TSDN sensitivity
 * WHY:  Top-down attention, arousal modulation
 * HOW:  All firing rates scaled by gain
 *
 * @param pop TSDN population
 * @param gain Gain multiplier [0, inf)
 * @return 0 on success, error code on failure
 */
int tsdn_set_gain(tsdn_population_t* pop, float gain);

/**
 * @brief Get current gain
 *
 * @param pop TSDN population
 * @param gain Output: current gain
 * @return 0 on success
 */
int tsdn_get_gain(const tsdn_population_t* pop, float* gain);

/**
 * @brief Apply predictive facilitation
 *
 * WHAT: Boost gain for TSDNs in predicted direction
 * WHY:  Dragonflies pre-activate neurons along predicted trajectory
 * HOW:  Gaussian gain boost centered on predicted direction
 *
 * @param pop TSDN population
 * @param predicted_direction Predicted target direction (radians)
 * @param facilitation_strength Boost amount [0, 1]
 * @param facilitation_width Angular width of boost (radians)
 * @return 0 on success, error code on failure
 */
int tsdn_apply_facilitation(
    tsdn_population_t* pop,
    float predicted_direction,
    float facilitation_strength,
    float facilitation_width
);

/**
 * @brief Clear predictive facilitation
 *
 * @param pop TSDN population
 * @return 0 on success
 */
int tsdn_clear_facilitation(tsdn_population_t* pop);

//=============================================================================
// Update Functions
//=============================================================================

/**
 * @brief Update TSDN population state
 *
 * WHAT: Advance time-dependent processes
 * WHY:  Handle adaptation, decay, temporal dynamics
 * HOW:  Update adaptation state, apply decay
 *
 * @param pop TSDN population
 * @param dt Time step (seconds)
 * @return 0 on success, error code on failure
 */
int tsdn_update(tsdn_population_t* pop, float dt);

//=============================================================================
// Statistics Functions
//=============================================================================

/**
 * @brief Get TSDN statistics
 *
 * @param pop TSDN population
 * @param stats Output: statistics (copied)
 * @return 0 on success
 */
int tsdn_get_stats(const tsdn_population_t* pop, tsdn_stats_t* stats);

/**
 * @brief Reset statistics
 *
 * @param pop TSDN population
 */
void tsdn_reset_stats(tsdn_population_t* pop);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Normalize angle to [-PI, PI]
 *
 * @param angle Angle in radians
 * @return Normalized angle
 */
float tsdn_normalize_angle(float angle);

/**
 * @brief Compute angular difference (shortest path)
 *
 * @param angle1 First angle (radians)
 * @param angle2 Second angle (radians)
 * @return Signed angular difference [-PI, PI]
 */
float tsdn_angular_diff(float angle1, float angle2);

/**
 * @brief Compute tuning curve response
 *
 * @param angle_diff Angular difference from preferred direction
 * @param tuning_type Type of tuning curve
 * @param tuning_width Width parameter
 * @param tuning_exponent Exponent (for cosine tuning)
 * @return Firing rate [0, 1]
 */
float tsdn_tuning_response(
    float angle_diff,
    tsdn_tuning_type_t tuning_type,
    float tuning_width,
    float tuning_exponent
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_DRAGONFLY_TSDN_H */
