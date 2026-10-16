/**
 * @file nimcp_arousal_state.c
 * @brief Implementation of arousal state management with hysteresis
 *
 * WHAT: Implements RAS-inspired arousal state transitions
 * WHY: Provides stable arousal regulation for brainstem control
 * HOW: Hysteresis prevents oscillation, rate-limiting ensures smooth transitions
 */

#include "core/medulla/nimcp_arousal_state.h"
#include "api/nimcp_api_exception.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/time/nimcp_time.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(arousal_state)

// State threshold boundaries (continuous arousal level ranges)
static const float STATE_THRESHOLDS[AROUSAL_STATE_COUNT + 1] = {
    0.00f,  // COMA start
    0.05f,  // DEEP_SLEEP start
    0.15f,  // LIGHT_SLEEP start
    0.30f,  // DROWSY start
    0.40f,  // RELAXED start
    0.55f,  // ALERT start
    0.70f,  // VIGILANT start
    0.85f,  // HYPERAROUSED start
    0.95f,  // PANIC start
    1.00f   // Upper bound
};

/**
 * WHAT: Determines discrete state label from continuous arousal level
 * WHY: Maps continuous space to discrete labels for logging/display only
 * HOW: Finds first threshold exceeded by arousal level.
 *      Public API: labels are derived FROM continuous values.
 */
arousal_state_enum_t arousal_state_label_from_level(float level) {
    // Clamp level to valid range
    if (level < 0.0f) level = 0.0f;
    if (level > 1.0f) level = 1.0f;

    // Find appropriate state by checking thresholds
    for (int i = 0; i < AROUSAL_STATE_COUNT; i++) {
        if (level < STATE_THRESHOLDS[i + 1]) {
            return (arousal_state_enum_t)i;
        }
    }

    return AROUSAL_STATE_PANIC;  // Max state
}

/**
 * WHAT: Internal wrapper for backward compatibility
 */
static arousal_state_enum_t get_state_from_level(float level) {
    return arousal_state_label_from_level(level);
}

/**
 * WHAT: Gets threshold for transitioning into a state
 * WHY: Hysteresis requires different thresholds for up/down transitions
 * HOW: Adds margin when transitioning up, subtracts when transitioning down
 */
static float get_transition_threshold(
    arousal_state_enum_t from_state,
    arousal_state_enum_t to_state,
    float hysteresis_margin
) {
    float base_threshold = STATE_THRESHOLDS[to_state];

    if (to_state > from_state) {
        // Transitioning up: require level to exceed threshold + margin
        return base_threshold + hysteresis_margin;
    } else {
        // Transitioning down: require level to fall below threshold - margin
        return base_threshold - hysteresis_margin;
    }
}

/**
 * WHAT: Checks if state transition should occur with hysteresis
 * WHY: Prevents rapid oscillation between adjacent states
 * HOW: Requires sustained threshold crossing for min_dwell_time
 */
static bool should_transition(
    arousal_state_t* state,
    arousal_state_enum_t new_state,
    uint64_t current_time_ms
) {
    // Guard: No transition if state unchanged
    if (new_state == state->current_state) {
        // Reset transition tracking
        state->transition.threshold_sustained = false;
        return false;
    }

    // Guard: Check minimum dwell time in current state
    uint64_t time_in_state = current_time_ms - state->last_transition_time_ms;
    if (time_in_state < state->config.min_dwell_time_ms) {
        return false;
    }

    // Guard: Hysteresis disabled, transition immediately
    if (!state->config.enable_hysteresis) {
        return true;
    }

    // Check if this is a new target state
    if (state->transition.target_state != new_state) {
        state->transition.target_state = new_state;
        state->transition.threshold_cross_time_ms = current_time_ms;
        state->transition.threshold_sustained = false;
        return false;
    }

    // Check if threshold has been sustained long enough
    uint64_t sustained_duration = current_time_ms - state->transition.threshold_cross_time_ms;
    if (sustained_duration >= state->config.min_dwell_time_ms) {
        state->transition.threshold_sustained = true;
        return true;
    }

    return false;
}

/**
 * WHAT: Applies rate-limiting to arousal level changes
 * WHY: Prevents unrealistic instantaneous arousal jumps
 * HOW: Clamps delta to max_rate_per_sec * delta_time
 */
static float apply_rate_limiting(
    float current_level,
    float target_level,
    float delta_sec,
    float max_rate_per_sec
) {
    float delta = target_level - current_level;
    float max_delta = max_rate_per_sec * delta_sec;

    if (fabsf(delta) <= max_delta) {
        return target_level;
    }

    if (delta > 0.0f) {
        return current_level + max_delta;
    } else {
        return current_level - max_delta;
    }
}

arousal_state_t* arousal_state_create(const arousal_state_config_t* config) {
    // Allocate main structure
    arousal_state_t* state = (arousal_state_t*)nimcp_malloc(sizeof(arousal_state_t));
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "arousal_state_create: failed to allocate arousal state");
        NIMCP_LOGGING_ERROR("Failed to allocate arousal state");
        return NULL;
    }

    memset(state, 0, sizeof(arousal_state_t));

    // Initialize configuration
    if (config) {
        state->config = *config;
    } else {
        arousal_state_default_config(&state->config);
    }

    // Initialize core state to RELAXED (default waking state)
    state->current_state = AROUSAL_STATE_RELAXED;
    state->arousal_level = 0.47f;  // Mid-RELAXED range

    // Initialize transition tracking
    state->transition.target_state = AROUSAL_STATE_RELAXED;
    state->transition.threshold_cross_time_ms = 0;
    state->transition.threshold_sustained = false;

    // Initialize timestamps
    uint64_t current_time = nimcp_time_get_ms();
    state->last_transition_time_ms = current_time;
    state->last_update_time_ms = current_time;

    // Initialize stimulus accumulator
    state->stimulus_accumulator = 0.0f;

    // Create mutex for thread safety
    state->mutex = nimcp_platform_mutex_create();
    if (!state->mutex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "arousal_state_create: failed to create mutex");
        NIMCP_LOGGING_ERROR("Failed to create arousal state mutex");
        nimcp_free(state);
        return NULL;
    }

    // Bio-async starts disabled
    state->bio_async_enabled = false;
    state->bio_ctx = NULL;

    NIMCP_LOGGING_INFO("Created arousal state manager (initial: RELAXED, level: 0.47)");
    return state;
}

void arousal_state_destroy(arousal_state_t* state) {
    // Guard: NULL check
    if (!state) {
        return;
    }

    // Disconnect bio-async if connected
    if (state->bio_async_enabled) {
        arousal_state_disconnect_bio_async(state);
    }

    // Destroy mutex
    if (state->mutex) {
        nimcp_platform_mutex_destroy(state->mutex);
        nimcp_free(state->mutex);
        state->mutex = NULL;
        state->mutex = NULL;
    }

    // Free main structure
    nimcp_free(state);
    NIMCP_LOGGING_INFO("Destroyed arousal state manager");
}

int arousal_state_default_config(arousal_state_config_t* config) {
    // Guard: NULL check
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "arousal_state_default_config: config is NULL");
        NIMCP_LOGGING_ERROR("NULL config pointer");
        return NIMCP_ERROR_NULL_POINTER;
    }

    // Hysteresis margin: 5% prevents oscillation at boundaries
    config->hysteresis_margin = 0.05f;

    // Minimum dwell time: 500ms ensures stability (based on RAS dynamics)
    config->min_dwell_time_ms = 500.0f;

    // Maximum rate: 0.5/sec allows 0-1 in 2 seconds (realistic arousal dynamics)
    config->max_rate_per_sec = 0.5f;

    // Stimulus decay: 0.1/sec gives ~10 second half-life for external stimuli
    config->stimulus_decay_rate = 0.1f;

    // Enable hysteresis and rate-limiting by default
    config->enable_hysteresis = true;
    config->enable_rate_limiting = true;

    return 0;
}

int arousal_state_update(arousal_state_t* state, float delta_ms) {
    // Guard: NULL check
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "arousal_state_update: state is NULL");
        NIMCP_LOGGING_ERROR("NULL state pointer");
        return NIMCP_ERROR_NULL_POINTER;
    }

    // Guard: Invalid delta
    if (delta_ms < 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "arousal_state_update: delta_ms is negative");
        NIMCP_LOGGING_ERROR("Invalid negative delta_ms: %f", delta_ms);
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_platform_mutex_lock(state->mutex);

    uint64_t current_time_ms = nimcp_time_get_ms();
    float delta_sec = delta_ms / 1000.0f;

    // Apply stimulus decay (exponential decay)
    if (state->stimulus_accumulator != 0.0f) {
        float decay_factor = expf(-state->config.stimulus_decay_rate * delta_sec);
        state->stimulus_accumulator *= decay_factor;

        // Zero out very small values
        if (fabsf(state->stimulus_accumulator) < 0.001f) {
            state->stimulus_accumulator = 0.0f;
        }
    }

    // Apply accumulated stimulus to arousal level
    float target_level = state->arousal_level + state->stimulus_accumulator;

    // Clamp to valid range
    if (target_level < 0.0f) target_level = 0.0f;
    if (target_level > 1.0f) target_level = 1.0f;

    // Apply rate-limiting if enabled
    if (state->config.enable_rate_limiting) {
        state->arousal_level = apply_rate_limiting(
            state->arousal_level,
            target_level,
            delta_sec,
            state->config.max_rate_per_sec
        );
    } else {
        state->arousal_level = target_level;
    }

    // Determine natural state from current arousal level
    arousal_state_enum_t natural_state = get_state_from_level(state->arousal_level);

    // Check if we should transition with hysteresis
    if (should_transition(state, natural_state, current_time_ms)) {
        arousal_state_enum_t old_state = state->current_state;
        state->current_state = natural_state;
        state->last_transition_time_ms = current_time_ms;

        NIMCP_LOGGING_INFO("Arousal state transition: %s -> %s (level: %.3f)",
            arousal_state_get_state_name(old_state),
            arousal_state_get_state_name(natural_state),
            state->arousal_level);

        // TODO: Send bio-async message on state change
    }

    state->last_update_time_ms = current_time_ms;

    nimcp_platform_mutex_unlock(state->mutex);
    return 0;
}

int arousal_state_get_level(const arousal_state_t* state, float* out_level) {
    // Guard: NULL checks
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "arousal_state_get_level: state is NULL");
        NIMCP_LOGGING_ERROR("NULL state pointer");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!out_level) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "arousal_state_get_level: out_level is NULL");
        NIMCP_LOGGING_ERROR("NULL out_level pointer");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_platform_mutex_lock(state->mutex);
    *out_level = state->arousal_level;
    nimcp_platform_mutex_unlock(state->mutex);

    return 0;
}

int arousal_state_get_state(const arousal_state_t* state, arousal_state_enum_t* out_state) {
    // Guard: NULL checks
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "arousal_state_get_state: state is NULL");
        NIMCP_LOGGING_ERROR("NULL state pointer");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!out_state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "arousal_state_get_state: out_state is NULL");
        NIMCP_LOGGING_ERROR("NULL out_state pointer");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_platform_mutex_lock(state->mutex);
    *out_state = state->current_state;
    nimcp_platform_mutex_unlock(state->mutex);

    return 0;
}

int arousal_state_set_target(arousal_state_t* state, float target_level) {
    // Guard: NULL check
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "arousal_state_set_target: state is NULL");
        NIMCP_LOGGING_ERROR("NULL state pointer");
        return NIMCP_ERROR_NULL_POINTER;
    }

    // Guard: Range check
    if (target_level < 0.0f || target_level > 1.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "arousal_state_set_target: target_level out of range");
        NIMCP_LOGGING_ERROR("Invalid target_level: %f (must be 0.0-1.0)", target_level);
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_platform_mutex_lock(state->mutex);

    // Set stimulus to move toward target
    state->stimulus_accumulator = target_level - state->arousal_level;

    nimcp_platform_mutex_unlock(state->mutex);

    NIMCP_LOGGING_DEBUG("Set arousal target: %.3f (current: %.3f)",
        target_level, state->arousal_level);
    return 0;
}

int arousal_state_apply_stimulus(arousal_state_t* state, float stimulus_magnitude) {
    // Guard: NULL check
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "arousal_state_apply_stimulus: state is NULL");
        NIMCP_LOGGING_ERROR("NULL state pointer");
        return NIMCP_ERROR_NULL_POINTER;
    }

    // Guard: Range check
    if (stimulus_magnitude < -1.0f || stimulus_magnitude > 1.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "arousal_state_apply_stimulus: magnitude out of range");
        NIMCP_LOGGING_ERROR("Invalid stimulus_magnitude: %f (must be -1.0 to 1.0)",
            stimulus_magnitude);
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_platform_mutex_lock(state->mutex);

    // Add stimulus to accumulator (clamped addition)
    state->stimulus_accumulator += stimulus_magnitude;

    // Clamp accumulator to prevent extreme values
    if (state->stimulus_accumulator < -1.0f) {
        state->stimulus_accumulator = -1.0f;
    }
    if (state->stimulus_accumulator > 1.0f) {
        state->stimulus_accumulator = 1.0f;
    }

    nimcp_platform_mutex_unlock(state->mutex);

    NIMCP_LOGGING_DEBUG("Applied arousal stimulus: %+.3f (accumulator: %+.3f)",
        stimulus_magnitude, state->stimulus_accumulator);
    return 0;
}

const char* arousal_state_get_state_name(arousal_state_enum_t state_enum) {
    switch (state_enum) {
        case AROUSAL_STATE_COMA:          return "COMA";
        case AROUSAL_STATE_DEEP_SLEEP:    return "DEEP_SLEEP";
        case AROUSAL_STATE_LIGHT_SLEEP:   return "LIGHT_SLEEP";
        case AROUSAL_STATE_DROWSY:        return "DROWSY";
        case AROUSAL_STATE_RELAXED:       return "RELAXED";
        case AROUSAL_STATE_ALERT:         return "ALERT";
        case AROUSAL_STATE_VIGILANT:      return "VIGILANT";
        case AROUSAL_STATE_HYPERAROUSED:  return "HYPERAROUSED";
        case AROUSAL_STATE_PANIC:         return "PANIC";
        default:                          return "UNKNOWN";
    }
}

/**
 * WHAT: Sigmoid function for smooth 0-1 transitions
 * WHY: Models biological activation curves without step discontinuities
 * HOW: 1 / (1 + exp(-k * (x - midpoint)))
 */
static float sigmoid(float x, float midpoint, float steepness) {
    return 1.0f / (1.0f + expf(-steepness * (x - midpoint)));
}

/**
 * WHAT: Inverted-U (Yerkes-Dodson) curve
 * WHY: Models optimal performance at moderate arousal, decline at extremes
 * HOW: Gaussian-like peak at 'optimal', width controlled by 'sigma'
 */
static float inverted_u(float x, float optimal, float sigma) {
    float dev = x - optimal;
    return expf(-(dev * dev) / (2.0f * sigma * sigma));
}

int arousal_compute_parameters(float arousal_level, arousal_params_t* out) {
    // Guard: NULL check
    if (!out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "arousal_compute_parameters: out is NULL");
        NIMCP_LOGGING_ERROR("NULL output pointer");
        return NIMCP_ERROR_NULL_POINTER;
    }

    // Clamp input to valid range
    if (arousal_level < 0.0f) arousal_level = 0.0f;
    if (arousal_level > 1.0f) arousal_level = 1.0f;

    // Cortical activation: sigmoid centered at 0.35, steep transition
    // Low arousal (coma/sleep) = near 0, high arousal = near 1
    out->cortical_activation = sigmoid(arousal_level, 0.35f, 10.0f);

    // Cognitive gain: Yerkes-Dodson inverted-U, optimal around 0.6 (alert)
    // sigma=0.25 gives a broad peak covering relaxed-to-vigilant range
    out->cognitive_gain = inverted_u(arousal_level, 0.6f, 0.25f);

    // Sensory gating: U-shaped (tight at moderate, open at extremes)
    // During sleep: sensory gates open (dreams, external stimuli)
    // During panic: gates open (threat detection, hypervigilance)
    // During alert: gates tight (focused attention filters noise)
    float gating_tightness = inverted_u(arousal_level, 0.6f, 0.3f);
    out->sensory_gating = 1.0f - gating_tightness;

    // Muscle tone: sigmoid, atonia during deep sleep, hypertonia during panic
    // Centered at 0.3 (transition from sleep to wake)
    out->muscle_tone = sigmoid(arousal_level, 0.3f, 8.0f);

    // Metabolic rate: linear from 0.3 (basal in coma) to 1.0 (max in panic)
    out->metabolic_rate = 0.3f + 0.7f * arousal_level;

    // Vigilance factor: exponential rise, saturating near 1.0
    // Models increasing environmental scanning with arousal
    out->vigilance_factor = 1.0f - expf(-3.0f * arousal_level);

    // Emotional reactivity: exponential increase above 0.5
    // Low arousal: minimal reactivity; high arousal: explosive reactivity
    float above_threshold = arousal_level - 0.5f;
    if (above_threshold < 0.0f) above_threshold = 0.0f;
    out->emotional_reactivity = 1.0f - expf(-4.0f * above_threshold);

    // Memory consolidation: inverted-U, best during moderate-low arousal
    // Peak around 0.35 (light sleep / drowsy -- where consolidation happens)
    out->memory_consolidation = inverted_u(arousal_level, 0.35f, 0.2f);

    // Free energy: deviation from optimal arousal (alert ~0.6)
    // Uses inverted-U centered at 0.6, inverted: 1 - peak = energy
    float optimality = inverted_u(arousal_level, 0.6f, 0.3f);
    out->free_energy = 1.0f - optimality;

    return 0;
}

int arousal_state_get_parameters(const arousal_state_t* state, arousal_params_t* out) {
    // Guard: NULL checks
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "arousal_state_get_parameters: state is NULL");
        NIMCP_LOGGING_ERROR("NULL state pointer");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "arousal_state_get_parameters: out is NULL");
        NIMCP_LOGGING_ERROR("NULL output pointer");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_platform_mutex_lock(state->mutex);
    float level = state->arousal_level;
    nimcp_platform_mutex_unlock(state->mutex);

    return arousal_compute_parameters(level, out);
}

int arousal_state_connect_bio_async(arousal_state_t* state) {
    // Guard: NULL check
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "arousal_state_connect_bio_async: state is NULL");
        NIMCP_LOGGING_ERROR("NULL state pointer");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_platform_mutex_lock(state->mutex);

    // Guard: Already connected
    if (state->bio_async_enabled) {
        nimcp_platform_mutex_unlock(state->mutex);
        NIMCP_LOGGING_WARN("Arousal state already connected to bio-async");
        return 0;
    }

    // Register with bio-async router
    bio_module_info_t info = {
        .module_id = BIO_MODULE_AROUSAL_STATE,
        .module_name = "arousal_state",
        .inbox_capacity = 32,
        .user_data = state
    };

    state->bio_ctx = bio_router_register_module(&info);
    if (state->bio_ctx) {
        state->bio_async_enabled = true;
        nimcp_platform_mutex_unlock(state->mutex);
        NIMCP_LOGGING_INFO("Arousal state connected to bio-async router");
        return 0;
    } else {
        nimcp_platform_mutex_unlock(state->mutex);
        NIMCP_LOGGING_INFO("Bio-async router not available, skipping registration");
        return 0;  // Not an error, just not available
    }
}

int arousal_state_disconnect_bio_async(arousal_state_t* state) {
    // Guard: NULL check
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "arousal_state_disconnect_bio_async: state is NULL");
        NIMCP_LOGGING_ERROR("NULL state pointer");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_platform_mutex_lock(state->mutex);

    // Guard: Not connected
    if (!state->bio_async_enabled) {
        nimcp_platform_mutex_unlock(state->mutex);
        return 0;
    }

    // Unregister from bio-async router
    if (state->bio_ctx) {
        bio_router_unregister_module(state->bio_ctx);
        state->bio_ctx = NULL;
    }

    state->bio_async_enabled = false;
    nimcp_platform_mutex_unlock(state->mutex);

    NIMCP_LOGGING_INFO("Arousal state disconnected from bio-async router");
    return 0;
}

bool arousal_state_is_bio_async_connected(const arousal_state_t* state) {
    // Guard: NULL check
    if (!state) {
        return false;
    }

    nimcp_platform_mutex_lock(state->mutex);
    bool connected = state->bio_async_enabled;
    nimcp_platform_mutex_unlock(state->mutex);

    return connected;
}
