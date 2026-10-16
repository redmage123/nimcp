#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_temporal_accumulator.c - Temporal Integration Implementation
//=============================================================================

#include "middleware/buffering/nimcp_temporal_accumulator.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "api/nimcp_api_exception.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"

#define LOG_MODULE "middleware_temporal_accumulator"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(temporal_accumulator)

#include <string.h>
#include <math.h>
#include <float.h>

/**
 * @brief Channel state structure
 *
 * WHAT: Per-channel accumulator state
 * WHY:  Track independent temporal dynamics per channel
 * HOW:  Store current value, derivatives, and extrema
 */
typedef struct {
    float value;           /**< Current accumulated value */
    float prev_value;      /**< Previous value (for rate of change) */
    float rate_of_change;  /**< Current rate of change */
    float peak_value;      /**< Peak value seen */
    float valley_value;    /**< Minimum value seen */
    uint64_t update_count; /**< Number of updates */
} channel_state_t;

/**
 * @brief Temporal accumulator structure
 *
 * WHAT: Multi-channel temporal integrator
 * WHY:  Model neural temporal dynamics
 * HOW:  Array of channel states with configurable integration
 */
struct temporal_accumulator {
    size_t num_channels;        /**< Number of channels */
    float alpha;                /**< Smoothing factor (0-1) */
    integration_mode_t mode;    /**< Integration mode */
    channel_state_t* channels;  /**< Channel states */
};

//=============================================================================
// HELPER FUNCTIONS
//=============================================================================

/**
 * @brief Apply exponential moving average
 *
 * WHAT: EMA integration: new_value = alpha * input + (1-alpha) * old_value
 * WHY:  Simple, efficient temporal smoothing
 * HOW:  Weighted average of current and previous value
 */
static float integrate_ema(float current, float input, float alpha) {
    return alpha * input + (1.0F - alpha) * current;
}

/**
 * @brief Apply leaky integrator
 *
 * WHAT: Leaky integrator: dv/dt = (input - value) / tau
 * WHY:  Models biological membrane dynamics (RC circuit)
 * HOW:  Exponential approach to input with time constant
 */
static float integrate_leaky(float current, float input, float alpha, float dt) {
    // Convert alpha to time constant
    // alpha = dt / (tau + dt), so tau = dt * (1/alpha - 1)
    // For small dt: new = current + (input - current) * alpha
    return current + (input - current) * alpha * dt;
}

/**
 * @brief Apply adaptive integration
 *
 * WHAT: Adaptive integration with change detection
 * WHY:  Respond faster to rapid changes, slower to stable signals
 * HOW:  Adjust alpha based on rate of change
 */
static float integrate_adaptive(
    float current,
    float input,
    float alpha,
    float rate_of_change,
    float dt
) {
    // Increase alpha for rapid changes
    float change_magnitude = fabsf(rate_of_change);
    float adaptive_alpha = alpha * (1.0F + change_magnitude);

    // Clamp to [0, 1]
    if (adaptive_alpha > 1.0F) adaptive_alpha = 1.0F;

    return integrate_leaky(current, input, adaptive_alpha, dt);
}

//=============================================================================
// LIFECYCLE
//=============================================================================

temporal_accumulator_t* temporal_accumulator_create(
    size_t num_channels,
    float alpha,
    integration_mode_t mode
) {
    // Guard: validate inputs
    if (num_channels == 0 || alpha < 0.0F || alpha > 1.0F) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "temporal_accumulator_create: invalid num_channels or alpha");
        return NULL;
    }

    // Allocate structure
    temporal_accumulator_t* acc = nimcp_calloc(1, sizeof(temporal_accumulator_t));
    if (!acc) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "temporal_accumulator_create: failed to allocate accumulator");
        return NULL;
    }

    // Allocate channel states
    acc->channels = nimcp_calloc(num_channels, sizeof(channel_state_t));
    if (!acc->channels) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "temporal_accumulator_create: failed to allocate channels");
        nimcp_free(acc);
        return NULL;
    }

    // Initialize configuration
    acc->num_channels = num_channels;
    acc->alpha = alpha;
    acc->mode = mode;

    // Initialize channel states
    for (size_t i = 0; i < num_channels; i++) {
        acc->channels[i].value = 0.0F;
        acc->channels[i].prev_value = 0.0F;
        acc->channels[i].rate_of_change = 0.0F;
        acc->channels[i].peak_value = -FLT_MAX;
        acc->channels[i].valley_value = FLT_MAX;
        acc->channels[i].update_count = 0;
    }

    return acc;
}

void temporal_accumulator_destroy(temporal_accumulator_t* accumulator) {
    if (!accumulator) return;

    nimcp_free(accumulator->channels);
    nimcp_free(accumulator);
}

//=============================================================================
// INTEGRATION OPERATIONS
//=============================================================================

bool temporal_accumulator_update(
    temporal_accumulator_t* accumulator,
    size_t channel,
    float value,
    float dt
) {
    // Guard: validate inputs
    if (!accumulator || channel >= accumulator->num_channels || dt <= 0.0F) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "temporal_accumulator_update: accumulator is NULL");
        return false;
    }

    channel_state_t* ch = &accumulator->channels[channel];

    // Store previous value
    ch->prev_value = ch->value;

    // Apply integration based on mode
    switch (accumulator->mode) {
        case INTEGRATION_EMA:
            ch->value = integrate_ema(ch->value, value, accumulator->alpha);
            break;

        case INTEGRATION_LEAKY:
            ch->value = integrate_leaky(ch->value, value, accumulator->alpha, dt);
            break;

        case INTEGRATION_ADAPTIVE:
            ch->value = integrate_adaptive(
                ch->value, value, accumulator->alpha, ch->rate_of_change, dt
            );
            break;

        default:
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "temporal_accumulator_update: operation failed");
            return false;
    }

    // Update rate of change
    ch->rate_of_change = (ch->value - ch->prev_value) / dt;

    // Update extrema
    if (ch->value > ch->peak_value) ch->peak_value = ch->value;
    if (ch->value < ch->valley_value) ch->valley_value = ch->value;

    // Update count
    ch->update_count++;

    return true;
}

size_t temporal_accumulator_update_all(
    temporal_accumulator_t* accumulator,
    const float* values,
    float dt
) {
    // Guard: validate inputs
    if (!accumulator || !values || dt <= 0.0F) return 0;

    size_t updated = 0;
    for (size_t i = 0; i < accumulator->num_channels; i++) {
        if (temporal_accumulator_update(accumulator, i, values[i], dt)) {
            updated++;
        }
    }

    return updated;
}

float temporal_accumulator_get_value(
    const temporal_accumulator_t* accumulator,
    size_t channel
) {
    // Guard: validate inputs
    if (!accumulator || channel >= accumulator->num_channels) return 0.0F;

    return accumulator->channels[channel].value;
}

size_t temporal_accumulator_get_all_values(
    const temporal_accumulator_t* accumulator,
    float* values,
    size_t max_channels
) {
    // Guard: validate inputs
    if (!accumulator || !values || max_channels == 0) return 0;

    size_t count = (accumulator->num_channels < max_channels) ?
        accumulator->num_channels : max_channels;

    for (size_t i = 0; i < count; i++) {
        values[i] = accumulator->channels[i].value;
    }

    return count;
}

//=============================================================================
// STATE QUERY
//=============================================================================

bool temporal_accumulator_get_state(
    const temporal_accumulator_t* accumulator,
    size_t channel,
    accumulator_state_t* state
) {
    // Guard: validate inputs
    if (!accumulator || channel >= accumulator->num_channels || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "temporal_accumulator_get_state: required parameter is NULL (accumulator, state)");
        return false;
    }

    channel_state_t* ch = &accumulator->channels[channel];

    state->value = ch->value;
    state->rate_of_change = ch->rate_of_change;
    state->peak_value = ch->peak_value;
    state->valley_value = ch->valley_value;
    state->update_count = ch->update_count;

    return true;
}

float temporal_accumulator_rate_of_change(
    const temporal_accumulator_t* accumulator,
    size_t channel
) {
    // Guard: validate inputs
    if (!accumulator || channel >= accumulator->num_channels) return 0.0F;

    return accumulator->channels[channel].rate_of_change;
}

float temporal_accumulator_peak(
    const temporal_accumulator_t* accumulator,
    size_t channel
) {
    // Guard: validate inputs
    if (!accumulator || channel >= accumulator->num_channels) return 0.0F;

    float peak = accumulator->channels[channel].peak_value;
    return (peak == -FLT_MAX) ? 0.0F : peak;
}

float temporal_accumulator_valley(
    const temporal_accumulator_t* accumulator,
    size_t channel
) {
    // Guard: validate inputs
    if (!accumulator || channel >= accumulator->num_channels) return 0.0F;

    float valley = accumulator->channels[channel].valley_value;
    return (valley == FLT_MAX) ? 0.0F : valley;
}

//=============================================================================
// CONFIGURATION
//=============================================================================

bool temporal_accumulator_set_alpha(
    temporal_accumulator_t* accumulator,
    float alpha
) {
    // Guard: validate inputs
    if (!accumulator || alpha < 0.0F || alpha > 1.0F) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "temporal_accumulator_set_alpha: accumulator is NULL");
        return false;
    }

    accumulator->alpha = alpha;
    return true;
}

float temporal_accumulator_get_alpha(
    const temporal_accumulator_t* accumulator
) {
    // Guard: validate input
    if (!accumulator) return 0.0F;

    return accumulator->alpha;
}

bool temporal_accumulator_set_mode(
    temporal_accumulator_t* accumulator,
    integration_mode_t mode
) {
    // Guard: validate input
    if (!accumulator) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "temporal_accumulator_set_mode: accumulator is NULL");
        return false;
    }

    accumulator->mode = mode;
    return true;
}

integration_mode_t temporal_accumulator_get_mode(
    const temporal_accumulator_t* accumulator
) {
    // Guard: validate input
    if (!accumulator) return INTEGRATION_EMA;

    return accumulator->mode;
}

//=============================================================================
// MANAGEMENT
//=============================================================================

bool temporal_accumulator_reset_channel(
    temporal_accumulator_t* accumulator,
    size_t channel
) {
    // Guard: validate inputs
    if (!accumulator || channel >= accumulator->num_channels) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "temporal_accumulator_reset_channel: accumulator is NULL");
        return false;
    }

    channel_state_t* ch = &accumulator->channels[channel];

    ch->value = 0.0F;
    ch->prev_value = 0.0F;
    ch->rate_of_change = 0.0F;
    ch->peak_value = -FLT_MAX;
    ch->valley_value = FLT_MAX;
    ch->update_count = 0;

    return true;
}

void temporal_accumulator_reset_all(temporal_accumulator_t* accumulator) {
    // Guard: validate input
    if (!accumulator) return;

    for (size_t i = 0; i < accumulator->num_channels; i++) {
        temporal_accumulator_reset_channel(accumulator, i);
    }
}

size_t temporal_accumulator_num_channels(
    const temporal_accumulator_t* accumulator
) {
    // Guard: validate input
    if (!accumulator) return 0;

    return accumulator->num_channels;
}
