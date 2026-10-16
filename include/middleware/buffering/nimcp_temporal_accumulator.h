//=============================================================================
// nimcp_temporal_accumulator.h - Temporal Integration and Accumulation
//=============================================================================

#ifndef NIMCP_TEMPORAL_ACCUMULATOR_H
#define NIMCP_TEMPORAL_ACCUMULATOR_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file nimcp_temporal_accumulator.h
 * @brief Temporal integration with biological time constants
 *
 * WHAT: Leaky integrator and exponential moving average for neural signals
 * WHY:  Model temporal dynamics of neurons and synapses
 * HOW:  Exponential decay with configurable time constants
 *
 * FEATURES:
 * - Exponential moving average (EMA)
 * - Leaky integrator (biological membrane dynamics)
 * - Adaptive integration (change detection)
 * - Multi-channel accumulation
 * - Event-triggered reset
 */

// Integration mode
typedef enum {
    INTEGRATION_EMA,        /**< Exponential moving average */
    INTEGRATION_LEAKY,      /**< Leaky integrator (RC circuit model) */
    INTEGRATION_ADAPTIVE    /**< Adaptive integration with change detection */
} integration_mode_t;

// Accumulator state
typedef struct {
    float value;           /**< Current accumulated value */
    float rate_of_change;  /**< Current rate of change */
    float peak_value;      /**< Peak value seen */
    float valley_value;    /**< Minimum value seen */
    uint64_t update_count; /**< Number of updates */
} accumulator_state_t;

// Opaque temporal accumulator type
typedef struct temporal_accumulator temporal_accumulator_t;

//=============================================================================
// LIFECYCLE
//=============================================================================

/**
 * @brief Create temporal accumulator
 *
 * WHAT: Allocate accumulator with specified integration parameters
 * WHY:  Model temporal dynamics of neural processes
 * HOW:  Initialize with time constant and integration mode
 *
 * @param num_channels Number of parallel channels (for multi-dimensional signals)
 * @param alpha Smoothing factor (0-1): 0=no update, 1=immediate, 0.1=slow
 * @param mode Integration mode
 * @return Temporal accumulator or NULL on failure
 */
temporal_accumulator_t* temporal_accumulator_create(
    size_t num_channels,
    float alpha,
    integration_mode_t mode
);

/**
 * @brief Destroy temporal accumulator
 *
 * WHAT: Free all accumulator resources
 * WHY:  Prevent memory leaks
 * HOW:  Free channels and structure
 *
 * @param accumulator Accumulator to destroy (can be NULL)
 */
void temporal_accumulator_destroy(temporal_accumulator_t* accumulator);

//=============================================================================
// INTEGRATION OPERATIONS
//=============================================================================

/**
 * @brief Update accumulator with new value
 *
 * WHAT: Integrate new sample into accumulator
 * WHY:  Update temporal state based on new input
 * HOW:  Apply integration equation based on mode
 *
 * @param accumulator Accumulator to update
 * @param channel Channel index
 * @param value New sample value
 * @param dt Time step (in seconds or simulation units)
 * @return true on success
 */
bool temporal_accumulator_update(
    temporal_accumulator_t* accumulator,
    size_t channel,
    float value,
    float dt
);

/**
 * @brief Update all channels at once
 *
 * WHAT: Batch update for multi-channel signals
 * WHY:  More efficient than individual channel updates
 * HOW:  Update each channel with corresponding value
 *
 * @param accumulator Accumulator to update
 * @param values Array of values (one per channel)
 * @param dt Time step
 * @return Number of channels successfully updated
 */
size_t temporal_accumulator_update_all(
    temporal_accumulator_t* accumulator,
    const float* values,
    float dt
);

/**
 * @brief Get current accumulated value
 *
 * WHAT: Retrieve current integration result
 * WHY:  Read accumulated neural signal
 * HOW:  Return value field from channel state
 *
 * @param accumulator Accumulator to query
 * @param channel Channel index
 * @return Current accumulated value, or 0.0 if invalid
 */
float temporal_accumulator_get_value(
    const temporal_accumulator_t* accumulator,
    size_t channel
);

/**
 * @brief Get all channel values
 *
 * WHAT: Retrieve values from all channels
 * WHY:  Read multi-dimensional accumulated signal
 * HOW:  Copy values to output array
 *
 * @param accumulator Accumulator to query
 * @param values Output array (must hold num_channels elements)
 * @param max_channels Size of output array
 * @return Number of values copied
 */
size_t temporal_accumulator_get_all_values(
    const temporal_accumulator_t* accumulator,
    float* values,
    size_t max_channels
);

//=============================================================================
// STATE QUERY
//=============================================================================

/**
 * @brief Get channel state
 *
 * WHAT: Retrieve complete state for a channel
 * WHY:  Access detailed accumulator information
 * HOW:  Copy state structure
 *
 * @param accumulator Accumulator to query
 * @param channel Channel index
 * @param state Output for state
 * @return true on success
 */
bool temporal_accumulator_get_state(
    const temporal_accumulator_t* accumulator,
    size_t channel,
    accumulator_state_t* state
);

/**
 * @brief Get rate of change
 *
 * WHAT: Calculate temporal derivative
 * WHY:  Detect rapid changes in signal
 * HOW:  Return tracked rate of change
 *
 * @param accumulator Accumulator to query
 * @param channel Channel index
 * @return Rate of change, or 0.0 if invalid
 */
float temporal_accumulator_rate_of_change(
    const temporal_accumulator_t* accumulator,
    size_t channel
);

/**
 * @brief Get peak value
 *
 * WHAT: Return maximum value seen
 * WHY:  Track signal extrema
 * HOW:  Return peak field
 *
 * @param accumulator Accumulator to query
 * @param channel Channel index
 * @return Peak value, or 0.0 if invalid
 */
float temporal_accumulator_peak(
    const temporal_accumulator_t* accumulator,
    size_t channel
);

/**
 * @brief Get valley value
 *
 * WHAT: Return minimum value seen
 * WHY:  Track signal extrema
 * HOW:  Return valley field
 *
 * @param accumulator Accumulator to query
 * @param channel Channel index
 * @return Valley value, or 0.0 if invalid
 */
float temporal_accumulator_valley(
    const temporal_accumulator_t* accumulator,
    size_t channel
);

//=============================================================================
// CONFIGURATION
//=============================================================================

/**
 * @brief Set integration alpha
 *
 * WHAT: Change smoothing factor
 * WHY:  Adapt integration speed to different signals
 * HOW:  Update alpha parameter
 *
 * @param accumulator Accumulator to configure
 * @param alpha New smoothing factor (0-1)
 * @return true on success
 */
bool temporal_accumulator_set_alpha(
    temporal_accumulator_t* accumulator,
    float alpha
);

/**
 * @brief Get integration alpha
 *
 * WHAT: Retrieve current smoothing factor
 * WHY:  Check accumulator configuration
 * HOW:  Return alpha field
 *
 * @param accumulator Accumulator to query
 * @return Current alpha value
 */
float temporal_accumulator_get_alpha(
    const temporal_accumulator_t* accumulator
);

/**
 * @brief Set integration mode
 *
 * WHAT: Change integration algorithm
 * WHY:  Switch between EMA, leaky, and adaptive modes
 * HOW:  Update mode parameter
 *
 * @param accumulator Accumulator to configure
 * @param mode New integration mode
 * @return true on success
 */
bool temporal_accumulator_set_mode(
    temporal_accumulator_t* accumulator,
    integration_mode_t mode
);

/**
 * @brief Get integration mode
 *
 * WHAT: Retrieve current integration mode
 * WHY:  Check accumulator configuration
 * HOW:  Return mode field
 *
 * @param accumulator Accumulator to query
 * @return Current integration mode
 */
integration_mode_t temporal_accumulator_get_mode(
    const temporal_accumulator_t* accumulator
);

//=============================================================================
// MANAGEMENT
//=============================================================================

/**
 * @brief Reset channel to initial state
 *
 * WHAT: Clear accumulated value and statistics
 * WHY:  Reset on event or state change
 * HOW:  Zero out channel state
 *
 * @param accumulator Accumulator to reset
 * @param channel Channel index
 * @return true on success
 */
bool temporal_accumulator_reset_channel(
    temporal_accumulator_t* accumulator,
    size_t channel
);

/**
 * @brief Reset all channels
 *
 * WHAT: Clear all accumulated values
 * WHY:  Global reset on major event
 * HOW:  Reset each channel to initial state
 *
 * @param accumulator Accumulator to reset
 */
void temporal_accumulator_reset_all(temporal_accumulator_t* accumulator);

/**
 * @brief Get number of channels
 *
 * WHAT: Return channel count
 * WHY:  Determine accumulator dimensionality
 * HOW:  Return num_channels field
 *
 * @param accumulator Accumulator to query
 * @return Number of channels
 */
size_t temporal_accumulator_num_channels(
    const temporal_accumulator_t* accumulator
);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_TEMPORAL_ACCUMULATOR_H
