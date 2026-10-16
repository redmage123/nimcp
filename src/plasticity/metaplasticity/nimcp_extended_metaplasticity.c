/**
 * @file nimcp_extended_metaplasticity.c
 * @brief Extended Metaplasticity Implementation
 * @version 1.0.0
 * @date 2025-12-19
 */

#include "plasticity/metaplasticity/nimcp_extended_metaplasticity.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_threshold_constants.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(extended_metaplasticity)

/* ============================================================================
 * Internal Controller Structure
 * ============================================================================ */

typedef struct metaplasticity_controller_struct {
    extended_metaplasticity_config_t config;
    extended_metaplasticity_state_t** states;
    uint32_t num_synapses;

    /* Callback support */
    threshold_change_callback_t callback;
    void* callback_user_data;

    /* Statistics */
    metaplasticity_stats_t stats;

    /* Bio-async integration */
    bio_module_context_t bio_ctx;
    bool bio_async_enabled;

    /* Thread safety */
    nimcp_platform_mutex_t mutex;
} metaplasticity_controller_internal_t;

/* ============================================================================
 * Static Helper Functions
 * ============================================================================ */

/**
 * WHAT: Get sleep reset factor for sleep state
 * WHY:  Map sleep state to reset strength
 * HOW:  Lookup table based on biological evidence
 */
static float get_sleep_reset_factor(sleep_state_t sleep_state) {
    switch (sleep_state) {
        case SLEEP_STATE_AWAKE:
            return METAPLASTICITY_SLEEP_RESET_AWAKE;
        case SLEEP_STATE_DROWSY:
            return METAPLASTICITY_SLEEP_RESET_DROWSY;
        case SLEEP_STATE_LIGHT_NREM:
            return METAPLASTICITY_SLEEP_RESET_LIGHT_NREM;
        case SLEEP_STATE_DEEP_NREM:
            return METAPLASTICITY_SLEEP_RESET_DEEP_NREM;
        case SLEEP_STATE_REM:
            return METAPLASTICITY_SLEEP_RESET_REM;
        default:
            return 0.0f;
    }
}

/**
 * WHAT: Compute exponential decay weight for history
 * WHY:  Recent history matters more than distant past
 * HOW:  w(τ) = exp(-τ/tau_history)
 */
static inline float compute_history_weight(float age_ms, float tau_ms) {
    if (tau_ms <= 0.0f) return 0.0f;
    return expf(-age_ms / tau_ms);
}

/* ============================================================================
 * Factory Functions - Configuration Presets
 * ============================================================================ */

extended_metaplasticity_config_t metaplasticity_config_default(void) {
    extended_metaplasticity_config_t config = {
        .baseline_tau_ms = METAPLASTICITY_DEFAULT_BASELINE_TAU,
        .history_tau_ms = METAPLASTICITY_DEFAULT_HISTORY_TAU,
        .activity_tau_ms = 1000.0f,  // 1 second

        .history_size = METAPLASTICITY_DEFAULT_HISTORY_SIZE,
        .history_sample_interval_ms = 10000.0f,  // 10 seconds

        .min_theta = METAPLASTICITY_MIN_THRESHOLD,
        .max_theta = METAPLASTICITY_MAX_THRESHOLD,
        .initial_theta_baseline = METAPLASTICITY_DEFAULT_THETA_BASELINE,

        .da_sensitivity = NIMCP_SENSITIVITY_DEFAULT,
        .ne_sensitivity = NIMCP_SENSITIVITY_DEFAULT,
        .ach_sensitivity = NIMCP_SENSITIVITY_DEFAULT,
        .serotonin_sensitivity = NIMCP_SENSITIVITY_DEFAULT,

        .enable_sleep_reset = true,
        .sleep_reset_strength = 1.0f,

        .enable_neuromodulator_shifts = true,
        .enable_long_term_history = true,
        .enable_callbacks = false,
        .enable_bio_async = false
    };
    return config;
}

extended_metaplasticity_config_t metaplasticity_config_fast(void) {
    extended_metaplasticity_config_t config = metaplasticity_config_default();
    config.baseline_tau_ms = 600000.0f;  // 10 minutes (faster)
    config.history_tau_ms = 3600000.0f;  // 1 hour (shorter history)
    config.da_sensitivity = 1.5f;  // More sensitive to DA
    config.sleep_reset_strength = 1.5f;  // Stronger reset
    return config;
}

extended_metaplasticity_config_t metaplasticity_config_slow(void) {
    extended_metaplasticity_config_t config = metaplasticity_config_default();
    config.baseline_tau_ms = 7200000.0f;  // 2 hours (slower)
    config.history_tau_ms = 43200000.0f;  // 12 hours (longer history)
    config.da_sensitivity = 0.5f;  // Less sensitive to DA
    config.sleep_reset_strength = 0.5f;  // Weaker reset
    return config;
}

extended_metaplasticity_config_t metaplasticity_config_hippocampal(void) {
    extended_metaplasticity_config_t config = metaplasticity_config_default();
    config.baseline_tau_ms = 1800000.0f;  // 30 minutes (fast baseline)
    config.history_tau_ms = 7200000.0f;   // 2 hours (episodic timescale)
    config.da_sensitivity = 2.0f;  // Very sensitive to reward
    config.sleep_reset_strength = 1.2f;  // Enhanced consolidation
    return config;
}

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

extended_metaplasticity_state_t* metaplasticity_state_create(
    const extended_metaplasticity_config_t* config
) {
    /* Use default config if NULL */
    extended_metaplasticity_config_t default_config;
    if (!config) {
        default_config = metaplasticity_config_default();
        config = &default_config;
    }

    /* Allocate state structure */
    extended_metaplasticity_state_t* state =
        (extended_metaplasticity_state_t*)nimcp_malloc(sizeof(extended_metaplasticity_state_t));
    if (!state) {
        NIMCP_LOGGING_ERROR("Failed to allocate metaplasticity state");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "metaplasticity_state_create: failed to allocate state");
        return NULL;
    }

    /* Initialize fields */
    memset(state, 0, sizeof(extended_metaplasticity_state_t));

    state->theta_baseline = config->initial_theta_baseline;
    state->theta_baseline_target = config->initial_theta_baseline;
    state->theta_effective = config->initial_theta_baseline;

    state->current_sleep_state = SLEEP_STATE_AWAKE;
    state->sleep_reset_factor = 0.0f;
    state->pre_sleep_theta = config->initial_theta_baseline;

    /* Allocate history buffer if enabled */
    if (config->enable_long_term_history && config->history_size > 0) {
        state->history = (threshold_history_entry_t*)nimcp_malloc(
            config->history_size * sizeof(threshold_history_entry_t)
        );
        if (!state->history) {
            NIMCP_LOGGING_ERROR("Failed to allocate history buffer");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "metaplasticity_state_create: failed to allocate history buffer");
            goto cleanup;
        }
        memset(state->history, 0, config->history_size * sizeof(threshold_history_entry_t));
        state->history_size = config->history_size;
    } else {
        state->history = NULL;
        state->history_size = 0;
    }

    state->history_index = 0;
    state->history_count = 0;

    /* Initialize mutex */
    if (nimcp_platform_mutex_init(&state->lock, false) != 0) {
        NIMCP_LOGGING_ERROR("Failed to initialize mutex");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "metaplasticity_state_create: mutex init failed");
        goto cleanup;
    }

    state->last_update_ms = nimcp_time_get_ms();

    NIMCP_LOGGING_DEBUG("Created metaplasticity state with history size %u",
                        state->history_size);

    return state;

cleanup:
    nimcp_free(state->history);
    nimcp_free(state);
    return NULL;
}

void metaplasticity_state_destroy(extended_metaplasticity_state_t* state) {
    if (!state) return;

    /* Destroy mutex */
    nimcp_platform_mutex_destroy(&state->lock);

    /* Free history buffer */
    if (state->history) {
        nimcp_free(state->history);
    }

    /* Free state structure */
    nimcp_free(state);

    NIMCP_LOGGING_DEBUG("Destroyed metaplasticity state");
}

void metaplasticity_state_reset(
    extended_metaplasticity_state_t* state,
    const extended_metaplasticity_config_t* config
) {
    if (!state) return;

    nimcp_platform_mutex_lock(&state->lock);

    /* Reset thresholds */
    float initial_theta = config ? config->initial_theta_baseline :
                                   METAPLASTICITY_DEFAULT_THETA_BASELINE;
    state->theta_baseline = initial_theta;
    state->theta_baseline_target = initial_theta;
    state->theta_effective = initial_theta;

    /* Reset activity tracking */
    state->activity_avg = 0.0f;
    state->activity_squared_avg = 0.0f;

    /* Clear history buffer */
    if (state->history) {
        memset(state->history, 0, state->history_size * sizeof(threshold_history_entry_t));
    }
    state->history_index = 0;
    state->history_count = 0;

    /* Reset neuromodulator effects */
    state->da_shift = 0.0f;
    state->ne_shift = 0.0f;
    state->ach_modulation = 0.0f;
    state->serotonin_shift = 0.0f;

    /* Reset sleep state */
    state->current_sleep_state = SLEEP_STATE_AWAKE;
    state->sleep_reset_factor = 0.0f;
    state->pre_sleep_theta = initial_theta;

    state->last_update_ms = nimcp_time_get_ms();

    nimcp_platform_mutex_unlock(&state->lock);

    NIMCP_LOGGING_DEBUG("Reset metaplasticity state to baseline %.3f", initial_theta);
}

/* ============================================================================
 * Core Update Functions
 * ============================================================================ */

int metaplasticity_update_baseline(
    extended_metaplasticity_state_t* state,
    float current_activity,
    float dt,
    const extended_metaplasticity_config_t* config
) {
    /* Guard clauses */
    if (!state) {
        NIMCP_LOGGING_ERROR("NULL state in baseline update");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metaplasticity_update_baseline: state is NULL");
        return -1;
    }
    if (!config) {
        NIMCP_LOGGING_ERROR("NULL config in baseline update");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metaplasticity_update_baseline: config is NULL");
        return -1;
    }

    if (dt <= 0.0f || config->baseline_tau_ms <= 0.0f) {
        return 0;  // No update
    }

    nimcp_platform_mutex_lock(&state->lock);

    /* Update activity averages */
    float alpha = 1.0f - expf(-dt / config->activity_tau_ms);
    state->activity_avg = state->activity_avg * (1.0f - alpha) + current_activity * alpha;
    if (!isfinite(state->activity_avg)) state->activity_avg = current_activity;

    float r_squared = current_activity * current_activity;
    state->activity_squared_avg = state->activity_squared_avg * (1.0f - alpha) +
                                   r_squared * alpha;

    /* Update baseline threshold target */
    state->theta_baseline_target = state->activity_squared_avg;

    /* Adapt baseline toward target */
    float baseline_alpha = 1.0f - expf(-dt / config->baseline_tau_ms);
    float old_baseline = state->theta_baseline;
    state->theta_baseline = state->theta_baseline * (1.0f - baseline_alpha) +
                            state->theta_baseline_target * baseline_alpha;

    /* Clamp to bounds */
    state->theta_baseline = nimcp_clampf(state->theta_baseline,
                                        config->min_theta,
                                        config->max_theta);

    nimcp_platform_mutex_unlock(&state->lock);

    /* Debug logging for significant changes */
    if (fabsf(state->theta_baseline - old_baseline) > 0.01f) {
        NIMCP_LOGGING_DEBUG("Baseline threshold: %.3f -> %.3f (target: %.3f)",
                           old_baseline, state->theta_baseline,
                           state->theta_baseline_target);
    }

    return 0;
}

int metaplasticity_update_history(
    extended_metaplasticity_state_t* state,
    float current_activity,
    uint64_t timestamp_ms,
    const extended_metaplasticity_config_t* config
) {
    /* Guard clauses */
    if (!state) {
        NIMCP_LOGGING_ERROR("NULL state in history update");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metaplasticity_update_history: state is NULL");
        return -1;
    }
    if (!config) {
        NIMCP_LOGGING_ERROR("NULL config in history update");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metaplasticity_update_history: config is NULL");
        return -1;
    }

    /* Skip if history disabled */
    if (!config->enable_long_term_history || !state->history || state->history_size == 0) {
        return 0;
    }

    nimcp_platform_mutex_lock(&state->lock);

    /* Add to circular buffer */
    uint32_t write_idx = state->history_index;
    state->history[write_idx].activity_squared = current_activity * current_activity;
    state->history[write_idx].timestamp_ms = timestamp_ms;

    /* Advance circular buffer */
    state->history_index = (state->history_index + 1) % state->history_size;
    if (state->history_count < state->history_size) {
        state->history_count++;
    }

    nimcp_platform_mutex_unlock(&state->lock);

    return 0;
}

int metaplasticity_apply_neuromodulator_shifts(
    extended_metaplasticity_state_t* state,
    const neuromodulator_levels_t* neuromod_levels,
    const extended_metaplasticity_config_t* config
) {
    /* Guard clauses */
    if (!state) {
        NIMCP_LOGGING_ERROR("NULL state in neuromodulator shifts");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metaplasticity_apply_neuromodulator_shifts: state is NULL");
        return -1;
    }
    if (!config) {
        NIMCP_LOGGING_ERROR("NULL config in neuromodulator shifts");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metaplasticity_apply_neuromodulator_shifts: config is NULL");
        return -1;
    }

    /* Skip if disabled or no neuromodulators */
    if (!config->enable_neuromodulator_shifts || !neuromod_levels) {
        state->da_shift = 0.0f;
        state->ne_shift = 0.0f;
        state->ach_modulation = 0.0f;
        state->serotonin_shift = 0.0f;
        return 0;
    }

    nimcp_platform_mutex_lock(&state->lock);

    /* Compute threshold shifts from neuromodulators */
    state->da_shift = METAPLASTICITY_DA_SHIFT_FACTOR *
                      neuromod_levels->dopamine *
                      config->da_sensitivity;

    state->ne_shift = METAPLASTICITY_NE_SHIFT_FACTOR *
                      neuromod_levels->norepinephrine *
                      config->ne_sensitivity;

    state->ach_modulation = METAPLASTICITY_ACH_SHIFT_FACTOR *
                            neuromod_levels->acetylcholine *
                            config->ach_sensitivity;

    state->serotonin_shift = METAPLASTICITY_5HT_SHIFT_FACTOR *
                             neuromod_levels->serotonin *
                             config->serotonin_sensitivity;

    nimcp_platform_mutex_unlock(&state->lock);

    NIMCP_LOGGING_DEBUG("Neuromodulator shifts: DA=%.3f, NE=%.3f, 5HT=%.3f",
                       state->da_shift, state->ne_shift, state->serotonin_shift);

    return 0;
}

int metaplasticity_apply_sleep_reset(
    extended_metaplasticity_state_t* state,
    sleep_state_t sleep_state,
    const extended_metaplasticity_config_t* config
) {
    /* Guard clauses */
    if (!state) {
        NIMCP_LOGGING_ERROR("NULL state in sleep reset");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metaplasticity_apply_sleep_reset: state is NULL");
        return -1;
    }
    if (!config) {
        NIMCP_LOGGING_ERROR("NULL config in sleep reset");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metaplasticity_apply_sleep_reset: config is NULL");
        return -1;
    }

    /* Skip if disabled */
    if (!config->enable_sleep_reset) {
        return 0;
    }

    nimcp_platform_mutex_lock(&state->lock);

    /* Detect sleep state transitions */
    sleep_state_t old_state = state->current_sleep_state;
    state->current_sleep_state = sleep_state;

    /* On wake->sleep transition, save pre-sleep threshold */
    if (old_state == SLEEP_STATE_AWAKE && sleep_state != SLEEP_STATE_AWAKE) {
        state->pre_sleep_theta = state->theta_effective;
        NIMCP_LOGGING_DEBUG("Sleep onset, saving threshold: %.3f", state->pre_sleep_theta);
    }

    /* Get reset factor for current state */
    float base_reset = get_sleep_reset_factor(sleep_state);
    state->sleep_reset_factor = base_reset * config->sleep_reset_strength;

    /* Apply reset if in sleep state */
    if (state->sleep_reset_factor > 0.0f) {
        float old_theta = state->theta_effective;

        /* θ_m ← θ_m × (1 - reset) + θ_baseline × reset */
        state->theta_effective = state->theta_effective * (1.0f - state->sleep_reset_factor) +
                                 state->theta_baseline * state->sleep_reset_factor;

        /* Clamp */
        state->theta_effective = nimcp_clampf(state->theta_effective,
                                            config->min_theta,
                                            config->max_theta);

        if (fabsf(state->theta_effective - old_theta) > 0.01f) {
            NIMCP_LOGGING_DEBUG("Sleep reset (state=%d): %.3f -> %.3f (reset=%.2f)",
                               sleep_state, old_theta, state->theta_effective,
                               state->sleep_reset_factor);
        }
    }

    nimcp_platform_mutex_unlock(&state->lock);

    return 0;
}

float metaplasticity_compute_effective_threshold(
    const extended_metaplasticity_state_t* state,
    const extended_metaplasticity_config_t* config
) {
    /* Guard clauses */
    if (!state) {
        NIMCP_LOGGING_ERROR("NULL state in compute effective threshold");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metaplasticity_compute_effective_threshold: state is NULL");
        return METAPLASTICITY_DEFAULT_THETA_BASELINE;
    }
    if (!config) {
        NIMCP_LOGGING_ERROR("NULL config in compute effective threshold");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metaplasticity_compute_effective_threshold: config is NULL");
        return METAPLASTICITY_DEFAULT_THETA_BASELINE;
    }

    /* Start with baseline */
    float theta = state->theta_baseline;

    /* Add history contribution if enabled */
    if (config->enable_long_term_history && state->history && state->history_count > 0) {
        uint64_t current_time = nimcp_time_get_ms();
        float weighted_sum = 0.0f;
        float weight_sum = 0.0f;

        /* Integrate over history with exponential decay */
        for (uint32_t i = 0; i < state->history_count; i++) {
            const threshold_history_entry_t* entry = &state->history[i];

            /* Compute age */
            float age_ms = (float)(current_time - entry->timestamp_ms);
            if (age_ms < 0.0f) continue;  // Future entry (shouldn't happen)

            /* Compute weight */
            float weight = compute_history_weight(age_ms, config->history_tau_ms);

            /* Accumulate */
            weighted_sum += weight * entry->activity_squared;
            weight_sum += weight;
        }

        /* Add history contribution (normalized) */
        if (weight_sum > 0.0f) {
            theta += (weighted_sum / weight_sum) * 0.2f;  // 20% contribution from history
        }
    }

    /* Apply neuromodulator shifts */
    float neuromod_factor = 1.0f - state->da_shift - state->ne_shift + state->serotonin_shift;
    theta *= neuromod_factor;

    /* Clamp */
    theta = nimcp_clampf(theta, config->min_theta, config->max_theta);

    return theta;
}

int metaplasticity_update(
    extended_metaplasticity_state_t* state,
    float current_activity,
    const neuromodulator_levels_t* neuromod_levels,
    float dt,
    const extended_metaplasticity_config_t* config
) {
    /* Guard clauses */
    if (!state) {
        NIMCP_LOGGING_ERROR("NULL state in metaplasticity update");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metaplasticity_update: state is NULL");
        return -1;
    }
    if (!config) {
        NIMCP_LOGGING_ERROR("NULL config in metaplasticity update");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metaplasticity_update: config is NULL");
        return -1;
    }

    /* Update baseline threshold */
    if (metaplasticity_update_baseline(state, current_activity, dt, config) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "metaplasticity_update: validation failed");
        return -1;
    }

    /* Update history (sample at intervals) */
    uint64_t current_time = nimcp_time_get_ms();
    uint64_t time_since_last = current_time - state->last_update_ms;

    if (config->enable_long_term_history &&
        time_since_last >= (uint64_t)config->history_sample_interval_ms) {
        metaplasticity_update_history(state, current_activity, current_time, config);
        state->last_update_ms = current_time;
    }

    /* Apply neuromodulator shifts */
    if (metaplasticity_apply_neuromodulator_shifts(state, neuromod_levels, config) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "metaplasticity_update: validation failed");
        return -1;
    }

    /* Apply sleep reset */
    if (metaplasticity_apply_sleep_reset(state, state->current_sleep_state, config) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "metaplasticity_update: validation failed");
        return -1;
    }

    /* Compute final effective threshold */
    float old_effective = state->theta_effective;
    state->theta_effective = metaplasticity_compute_effective_threshold(state, config);

    /* Log significant changes */
    if (fabsf(state->theta_effective - old_effective) > 0.05f) {
        NIMCP_LOGGING_DEBUG("Effective threshold changed: %.3f -> %.3f",
                           old_effective, state->theta_effective);
    }

    return 0;
}

/* ============================================================================
 * Query Functions
 * ============================================================================ */

float metaplasticity_get_threshold(const extended_metaplasticity_state_t* state) {
    if (!state) return METAPLASTICITY_DEFAULT_THETA_BASELINE;
    return state->theta_effective;
}

float metaplasticity_get_baseline(const extended_metaplasticity_state_t* state) {
    if (!state) return METAPLASTICITY_DEFAULT_THETA_BASELINE;
    return state->theta_baseline;
}

float metaplasticity_get_plasticity_rate(
    const extended_metaplasticity_state_t* state,
    float current_activity,
    float base_rate
) {
    if (!state) return base_rate;

    /* Plasticity rate proportional to distance from threshold */
    float distance = fabsf(current_activity - state->theta_effective);
    float modulation = 1.0f + distance;  // Linear scaling

    return base_rate * modulation;
}

bool metaplasticity_will_induce_ltp(
    const extended_metaplasticity_state_t* state,
    float activity
) {
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metaplasticity_will_induce_ltp: state is NULL");
        return false;
    }
    return activity > state->theta_effective;
}

/* ============================================================================
 * Controller Functions (Multi-synapse Management)
 * ============================================================================ */

metaplasticity_controller_t metaplasticity_controller_create(
    const extended_metaplasticity_config_t* config,
    uint32_t num_synapses
) {
    /* Guard clauses */
    if (num_synapses == 0) {
        NIMCP_LOGGING_ERROR("Cannot create controller with 0 synapses");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "metaplasticity_controller_create: num_synapses is 0");
        return NULL;
    }

    /* Use default config if NULL */
    extended_metaplasticity_config_t default_config;
    if (!config) {
        default_config = metaplasticity_config_default();
        config = &default_config;
    }

    /* Allocate controller */
    metaplasticity_controller_internal_t* controller =
        (metaplasticity_controller_internal_t*)nimcp_malloc(
            sizeof(metaplasticity_controller_internal_t)
        );
    if (!controller) {
        NIMCP_LOGGING_ERROR("Failed to allocate controller");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "metaplasticity_controller_create: failed to allocate controller");
        return NULL;
    }

    memset(controller, 0, sizeof(metaplasticity_controller_internal_t));
    controller->config = *config;
    controller->num_synapses = num_synapses;

    /* Allocate state array */
    controller->states = (extended_metaplasticity_state_t**)nimcp_malloc(
        num_synapses * sizeof(extended_metaplasticity_state_t*)
    );
    if (!controller->states) {
        NIMCP_LOGGING_ERROR("Failed to allocate state array");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "metaplasticity_controller_create: failed to allocate state array");
        goto ctrl_cleanup;
    }
    memset(controller->states, 0, num_synapses * sizeof(extended_metaplasticity_state_t*));

    /* Create individual states */
    for (uint32_t i = 0; i < num_synapses; i++) {
        controller->states[i] = metaplasticity_state_create(config);
        if (!controller->states[i]) {
            NIMCP_LOGGING_ERROR("Failed to create state %u", i);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "metaplasticity_controller_create: state creation failed");
            goto ctrl_cleanup;
        }
    }

    /* Initialize mutex */
    if (nimcp_platform_mutex_init(&controller->mutex, false) != 0) {
        NIMCP_LOGGING_ERROR("Failed to initialize controller mutex");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "metaplasticity_controller_create: mutex init failed");
        goto ctrl_cleanup;
    }

    NIMCP_LOGGING_INFO("Created metaplasticity controller for %u synapses", num_synapses);

    return (metaplasticity_controller_t)controller;

ctrl_cleanup:
    if (controller->states) {
        for (uint32_t j = 0; j < num_synapses; j++) {
            metaplasticity_state_destroy(controller->states[j]);
        }
        nimcp_free(controller->states);
    }
    nimcp_free(controller);
    return NULL;
}

void metaplasticity_controller_destroy(metaplasticity_controller_t controller) {
    if (!controller) return;

    metaplasticity_controller_internal_t* ctrl =
        (metaplasticity_controller_internal_t*)controller;

    /* Destroy mutex */
    nimcp_platform_mutex_destroy(&ctrl->mutex);

    /* Destroy all states */
    if (ctrl->states) {
        for (uint32_t i = 0; i < ctrl->num_synapses; i++) {
            metaplasticity_state_destroy(ctrl->states[i]);
        }
        nimcp_free(ctrl->states);
    }

    /* Free controller */
    nimcp_free(ctrl);

    NIMCP_LOGGING_INFO("Destroyed metaplasticity controller");
}

int metaplasticity_controller_update_all(
    metaplasticity_controller_t controller,
    const float* activities,
    const neuromodulator_levels_t* neuromod_levels,
    float dt
) {
    /* Guard clauses */
    if (!controller) {
        NIMCP_LOGGING_ERROR("NULL controller in controller update");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metaplasticity_controller_update_all: controller is NULL");
        return -1;
    }
    if (!activities) {
        NIMCP_LOGGING_ERROR("NULL activities in controller update");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metaplasticity_controller_update_all: activities is NULL");
        return -1;
    }

    metaplasticity_controller_internal_t* ctrl =
        (metaplasticity_controller_internal_t*)controller;

    /* Update each synapse */
    for (uint32_t i = 0; i < ctrl->num_synapses; i++) {
        if (metaplasticity_update(ctrl->states[i], activities[i],
                                  neuromod_levels, dt, &ctrl->config) != 0) {
            NIMCP_LOGGING_WARN("Failed to update synapse %u", i);
            /* Continue with other synapses */
        }
    }

    return 0;
}

int metaplasticity_controller_get_stats(
    metaplasticity_controller_t controller,
    metaplasticity_stats_t* stats
) {
    /* Guard clauses */
    if (!controller) {
        NIMCP_LOGGING_ERROR("NULL controller in get stats");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metaplasticity_controller_get_stats: controller is NULL");
        return -1;
    }
    if (!stats) {
        NIMCP_LOGGING_ERROR("NULL stats in get stats");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metaplasticity_controller_get_stats: stats is NULL");
        return -1;
    }

    metaplasticity_controller_internal_t* ctrl =
        (metaplasticity_controller_internal_t*)controller;

    nimcp_platform_mutex_lock(&ctrl->mutex);

    /* Compute aggregate statistics */
    memset(stats, 0, sizeof(metaplasticity_stats_t));

    float sum_baseline = 0.0f;
    float sum_effective = 0.0f;
    float sum_da_shift = 0.0f;
    float sum_sleep_reset = 0.0f;

    for (uint32_t i = 0; i < ctrl->num_synapses; i++) {
        extended_metaplasticity_state_t* state = ctrl->states[i];
        sum_baseline += state->theta_baseline;
        sum_effective += state->theta_effective;
        sum_da_shift += state->da_shift;
        sum_sleep_reset += state->sleep_reset_factor;
    }

    uint32_t n = ctrl->num_synapses;
    stats->mean_theta_baseline = sum_baseline / n;
    stats->mean_theta_effective = sum_effective / n;
    stats->mean_da_shift = sum_da_shift / n;
    stats->mean_sleep_reset = sum_sleep_reset / n;

    /* Compute variance */
    float var_sum = 0.0f;
    for (uint32_t i = 0; i < ctrl->num_synapses; i++) {
        float diff = ctrl->states[i]->theta_effective - stats->mean_theta_effective;
        var_sum += diff * diff;
    }
    stats->theta_variance = var_sum / n;

    /* Copy controller stats */
    stats->total_updates = ctrl->stats.total_updates;
    stats->sleep_resets = ctrl->stats.sleep_resets;
    stats->threshold_changes = ctrl->stats.threshold_changes;

    nimcp_platform_mutex_unlock(&ctrl->mutex);

    return 0;
}

int metaplasticity_controller_set_sleep_state(
    metaplasticity_controller_t controller,
    sleep_state_t sleep_state
) {
    /* Guard clauses */
    if (!controller) {
        NIMCP_LOGGING_ERROR("NULL controller in set sleep state");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metaplasticity_controller_set_sleep_state: controller is NULL");
        return -1;
    }

    metaplasticity_controller_internal_t* ctrl =
        (metaplasticity_controller_internal_t*)controller;

    /* Apply to all synapses */
    for (uint32_t i = 0; i < ctrl->num_synapses; i++) {
        if (metaplasticity_apply_sleep_reset(ctrl->states[i], sleep_state,
                                            &ctrl->config) != 0) {
            NIMCP_LOGGING_WARN("Failed to set sleep state for synapse %u", i);
        }
    }

    /* Track sleep resets */
    if (sleep_state != SLEEP_STATE_AWAKE) {
        ctrl->stats.sleep_resets++;
    }

    NIMCP_LOGGING_DEBUG("Set sleep state to %d for %u synapses",
                       sleep_state, ctrl->num_synapses);

    return 0;
}

int metaplasticity_controller_set_callback(
    metaplasticity_controller_t controller,
    threshold_change_callback_t callback,
    void* user_data
) {
    /* Guard clauses */
    if (!controller) {
        NIMCP_LOGGING_ERROR("NULL controller in set callback");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metaplasticity_controller_set_callback: controller is NULL");
        return -1;
    }

    metaplasticity_controller_internal_t* ctrl =
        (metaplasticity_controller_internal_t*)controller;

    nimcp_platform_mutex_lock(&ctrl->mutex);
    ctrl->callback = callback;
    ctrl->callback_user_data = user_data;
    nimcp_platform_mutex_unlock(&ctrl->mutex);

    NIMCP_LOGGING_DEBUG("Registered threshold change callback");

    return 0;
}

/* ============================================================================
 * Module Management
 * ============================================================================ */

static struct {
    bool initialized;
    extended_metaplasticity_config_t config;
    bio_module_context_t bio_ctx;
} g_module_state = { .initialized = false };

bool metaplasticity_module_init(const extended_metaplasticity_config_t* config) {
    if (g_module_state.initialized) {
        NIMCP_LOGGING_WARN("Metaplasticity module already initialized");
        return true;
    }

    /* Store config */
    if (config) {
        g_module_state.config = *config;
    } else {
        g_module_state.config = metaplasticity_config_default();
    }

    /* Initialize bio-async if enabled */
    if (g_module_state.config.enable_bio_async) {
        /* Bio-async initialization would go here */
        NIMCP_LOGGING_INFO("Bio-async enabled for metaplasticity module");
    }

    g_module_state.initialized = true;
    NIMCP_LOGGING_INFO("Metaplasticity module initialized");

    return true;
}

void metaplasticity_module_destroy(void) {
    if (!g_module_state.initialized) {
        return;
    }

    /* Cleanup bio-async if enabled */
    if (g_module_state.config.enable_bio_async) {
        /* Bio-async cleanup would go here */
    }

    g_module_state.initialized = false;
    NIMCP_LOGGING_INFO("Metaplasticity module destroyed");
}
