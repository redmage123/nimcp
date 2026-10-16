/**
 * @file nimcp_astrocyte_plasticity.c
 * @brief Astrocyte-Mediated Synaptic Plasticity Implementation
 * @version 1.0.0
 * @date 2025-12-19
 *
 * WHAT: Tripartite synapse model with astrocyte-neuron interactions
 * WHY:  Astrocytes actively modulate synaptic plasticity and transmission
 * HOW:  Track astrocyte state (D-serine, glutamate uptake, ATP, calcium waves)
 *       and compute effects on NMDA, transmission, and metaplasticity
 */

#include "plasticity/astrocyte/nimcp_astrocyte_plasticity.h"
#include "api/nimcp_api_exception.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(astrocyte_plasticity)

/* ============================================================================
 * Internal Structure
 * ============================================================================ */

struct astrocyte_plasticity_struct {
    /* Configuration */
    astrocyte_config_t config;

    /* Astrocyte states */
    astrocyte_state_t* states;
    uint32_t num_astrocytes;
    uint32_t capacity;

    /* Statistics */
    uint64_t total_updates;
    uint64_t calcium_waves_triggered;
    uint64_t gliotransmitter_releases;

    /* Thread safety - using platform-agnostic mutex */
    nimcp_platform_mutex_t* mutex;
};

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Exponential decay
 *
 * WHAT: Apply exponential decay to value
 * WHY:  Model natural decay processes (calcium, gliotransmitters)
 * HOW:  value *= exp(-dt / tau)
 */
static float exp_decay(float value, float tau_ms, float dt_ms) {
    if (tau_ms <= 0.0f) return value;
    return value * expf(-dt_ms / tau_ms);
}

/**
 * @brief Compute D-serine NMDA modulation factor
 *
 * WHAT: Map D-serine level to NMDA activation factor
 * WHY:  D-serine is NMDA co-agonist, required for full activation
 * HOW:  Sigmoidal function with threshold at 0.5
 */
static float compute_d_serine_factor(float d_serine_level) {
    /* Below threshold, LTP is impaired */
    if (d_serine_level < ASTROCYTE_D_SERINE_LTP_THRESHOLD) {
        /* Linear reduction below threshold */
        return 0.6f * (d_serine_level / ASTROCYTE_D_SERINE_LTP_THRESHOLD);
    }

    /* Above threshold, normal to enhanced function
     * Clamp normalized to [0, 1] to ensure factor stays in [0.6, 1.5] */
    float denom = 1.0f - ASTROCYTE_D_SERINE_LTP_THRESHOLD;
    if (denom < 1e-6f) denom = 1e-6f;  /* Guard: prevent div-by-zero if threshold ≈ 1.0 */
    float normalized = (d_serine_level - ASTROCYTE_D_SERINE_LTP_THRESHOLD) / denom;
    normalized = nimcp_clampf(normalized, 0.0f, 1.0f);
    return 0.6f + 0.9f * normalized;  /* Range: 0.6 - 1.5 */
}

/**
 * @brief Compute glutamate clearance time constant
 *
 * WHAT: Convert uptake rate to clearance time
 * WHY:  Slower uptake → longer clearance → spillover
 * HOW:  Inverse relationship: time = base_time / uptake_rate
 */
static float compute_glu_clearance_time(float uptake_rate) {
    float clamped_uptake = nimcp_clampf(uptake_rate, 0.1f, 1.0f);
    return ASTROCYTE_GLU_UPTAKE_TIME_MS / clamped_uptake;
}

/**
 * @brief Compute A1R inhibition from adenosine level
 *
 * WHAT: Map adenosine to A1R-mediated inhibition
 * WHY:  Adenosine A1R activation suppresses transmission
 * HOW:  Sigmoidal activation above threshold
 */
static float compute_a1r_inhibition(float adenosine_level) {
    if (adenosine_level < ASTROCYTE_ADENOSINE_A1R_THRESHOLD) {
        return 0.0f;
    }

    float above_threshold = adenosine_level - ASTROCYTE_ADENOSINE_A1R_THRESHOLD;
    float denom = 1.0f - ASTROCYTE_ADENOSINE_A1R_THRESHOLD;
    if (denom < 1e-6f) denom = 1e-6f;  /* Guard: prevent div-by-zero if threshold ≈ 1.0 */
    float normalized = above_threshold / denom;
    return 0.6f * normalized;  /* Max 60% inhibition */
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

int astrocyte_plasticity_default_config(astrocyte_config_t* config) {
    NIMCP_API_CHECK_NULL(config, -1, "Astrocyte config is NULL");

    /* Baseline gliotransmitter levels */
    config->baseline_d_serine = ASTROCYTE_D_SERINE_BASELINE;
    config->baseline_glu_uptake = ASTROCYTE_GLU_UPTAKE_BASELINE;
    config->baseline_atp_release = ASTROCYTE_ATP_BASELINE;
    config->baseline_calcium = 0.1f;

    /* Calcium wave parameters */
    config->ca_wave_freq_min = ASTROCYTE_CA_WAVE_FREQ_LOW;
    config->ca_wave_freq_max = ASTROCYTE_CA_WAVE_FREQ_HIGH;
    config->ca_wave_propagation_velocity = ASTROCYTE_CA_WAVE_VELOCITY;
    config->ca_wave_trigger_threshold = 0.7f;

    /* Release kinetics */
    config->d_serine_release_time_ms = ASTROCYTE_D_SERINE_RELEASE_MS;
    config->glu_uptake_time_ms = ASTROCYTE_GLU_UPTAKE_TIME_MS;
    config->atp_release_time_ms = 100.0f;

    /* Spatial parameters */
    config->coverage_radius_um = ASTROCYTE_CA_WAVE_PROPAGATION;
    config->max_synapses_per_astrocyte = 100000;  /* ~100k synapses per astrocyte */

    /* All features enabled by default */
    config->enable_d_serine_modulation = true;
    config->enable_glutamate_uptake = true;
    config->enable_atp_signaling = true;
    config->enable_calcium_waves = true;
    config->enable_reactive_astrogliosis = true;

    /* No callback by default */
    config->gliotransmitter_callback = NULL;
    config->callback_user_data = NULL;

    return 0;
}

astrocyte_plasticity_t astrocyte_plasticity_create(
    const astrocyte_config_t* config,
    uint32_t num_astrocytes
) {
    /* Guard: require at least one astrocyte */
    if (num_astrocytes == 0) {
        LOG_ERROR("Cannot create astrocyte system with 0 astrocytes");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "astrocyte_plasticity_create: num_astrocytes is 0");
        return NULL;
    }

    /* Allocate system */
    astrocyte_plasticity_t astro = (astrocyte_plasticity_t)
        nimcp_malloc(sizeof(struct astrocyte_plasticity_struct));
    if (!astro) {
        LOG_ERROR("Astrocyte system allocation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "astrocyte_plasticity_create: system allocation failed");
        return NULL;
    }
    memset(astro, 0, sizeof(struct astrocyte_plasticity_struct));

    /* Apply configuration */
    if (config) {
        memcpy(&astro->config, config, sizeof(astrocyte_config_t));
    } else {
        astrocyte_plasticity_default_config(&astro->config);
    }

    /* Allocate astrocyte states */
    astro->num_astrocytes = num_astrocytes;
    astro->capacity = num_astrocytes;
    astro->states = (astrocyte_state_t*)
        nimcp_malloc(sizeof(astrocyte_state_t) * num_astrocytes);
    if (!astro->states) {
        nimcp_free(astro);
        LOG_ERROR("Astrocyte states allocation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Astrocyte states allocation failed");
        return NULL;
    }

    /* Initialize states to baseline */
    for (uint32_t i = 0; i < num_astrocytes; i++) {
        astrocyte_state_t* state = &astro->states[i];
        memset(state, 0, sizeof(astrocyte_state_t));

        state->d_serine_level = astro->config.baseline_d_serine;
        state->glutamate_uptake_rate = astro->config.baseline_glu_uptake;
        state->atp_release_level = astro->config.baseline_atp_release;
        state->adenosine_level = 0.0f;

        state->calcium_baseline = astro->config.baseline_calcium;
        state->calcium_current = astro->config.baseline_calcium;
        state->calcium_wave_frequency = astro->config.ca_wave_freq_min;
        state->calcium_wave_amplitude = 0.0f;
        state->calcium_wave_active = false;

        state->reactive_state = ASTROCYTE_RESTING;
        state->a1_factor = 0.0f;
        state->a2_factor = 0.0f;

        state->coverage_radius_um = astro->config.coverage_radius_um;
        state->num_synapses_covered = 0;
        state->last_update_ms = 0;
        state->delta_time_s = 0.0f;
    }

    /* Create mutex using platform-agnostic API */
    astro->mutex = nimcp_platform_mutex_create();
    if (!astro->mutex) {
        nimcp_free(astro->states);
        nimcp_free(astro);
        LOG_ERROR("Astrocyte mutex creation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Astrocyte mutex creation failed");
        return NULL;
    }

    NIMCP_LOGGING_INFO("Astrocyte plasticity system created with %u astrocytes",
                       num_astrocytes);
    return astro;
}

void astrocyte_plasticity_destroy(astrocyte_plasticity_t astro) {
    if (!astro) return;

    /* Destroy mutex using platform-agnostic API.
     * BUG-7 note: nimcp_platform_mutex_destroy() only destroys internal state
     * (pthread_mutex_destroy). nimcp_platform_mutex_create() allocates with
     * nimcp_malloc(), so nimcp_free() is required to release the memory. */
    if (astro->mutex) {
        nimcp_platform_mutex_destroy(astro->mutex);
        nimcp_free(astro->mutex);
        astro->mutex = NULL;
    }

    /* Free states */
    if (astro->states) {
        nimcp_free(astro->states);
    }

    /* Free system */
    nimcp_free(astro);
    NIMCP_LOGGING_DEBUG("Astrocyte plasticity system destroyed");
}

/* ============================================================================
 * Update Implementation
 * ============================================================================ */

int astrocyte_plasticity_update(
    astrocyte_plasticity_t astro,
    uint32_t astrocyte_id,
    float synaptic_activity,
    uint64_t delta_ms
) {
    /* Guard clauses */
    NIMCP_API_CHECK_NULL(astro, -1, "Astrocyte system is NULL");
    NIMCP_API_CHECK(astrocyte_id < astro->num_astrocytes, -1, "Astrocyte ID out of range");

    nimcp_platform_mutex_lock(astro->mutex);

    astrocyte_state_t* state = &astro->states[astrocyte_id];
    float dt_s = delta_ms / 1000.0f;
    state->delta_time_s = dt_s;
    /* BUG-8 note: This stores the delta (interval) not an absolute timestamp.
     * The header documents last_update_ms as "Last state update timestamp" but
     * astrocyte_plasticity_update() receives delta_ms (elapsed time), not an
     * absolute timestamp. The field is not currently read back for ISI
     * computation, so renaming the field or adding a timestamp parameter would
     * be the proper fix. For now, document the inconsistency. */
    state->last_update_ms = delta_ms;

    /* Update calcium based on synaptic activity */
    float calcium_influx = synaptic_activity * 0.5f;
    state->calcium_current += calcium_influx;

    /* Calcium decay back to baseline */
    state->calcium_current = state->calcium_baseline +
        exp_decay(state->calcium_current - state->calcium_baseline, 1000.0f, delta_ms);

    /* D-serine release depends on calcium */
    if (astro->config.enable_d_serine_modulation) {
        float d_serine_production = state->calcium_current * 0.1f;
        state->d_serine_level += d_serine_production * dt_s;
        state->d_serine_level = exp_decay(state->d_serine_level,
            astro->config.d_serine_release_time_ms, delta_ms);
        state->d_serine_level = nimcp_clampf(state->d_serine_level, 0.0f, 1.5f);
    }

    /* Glutamate uptake modulation based on reactive state */
    if (astro->config.enable_glutamate_uptake) {
        float target_uptake = astro->config.baseline_glu_uptake;
        if (state->reactive_state == ASTROCYTE_A1_REACTIVE) {
            target_uptake = ASTROCYTE_GLU_UPTAKE_A1_IMPAIRED;
        }
        /* Exponential approach to target */
        state->glutamate_uptake_rate +=
            (target_uptake - state->glutamate_uptake_rate) * dt_s * 0.1f;
    }

    /* ATP release and adenosine production */
    if (astro->config.enable_atp_signaling) {
        /* ATP released proportional to calcium */
        float atp_production = state->calcium_current * 0.3f;
        state->atp_release_level += atp_production * dt_s;
        state->atp_release_level = exp_decay(state->atp_release_level,
            astro->config.atp_release_time_ms, delta_ms);

        /* ATP → adenosine conversion */
        float atp_to_adenosine = state->atp_release_level * 0.5f * dt_s;
        state->adenosine_level += atp_to_adenosine;
        state->adenosine_level = exp_decay(state->adenosine_level, 2000.0f, delta_ms);

        state->atp_release_level = nimcp_clampf(state->atp_release_level, 0.0f, 1.0f);
        state->adenosine_level = nimcp_clampf(state->adenosine_level, 0.0f, 1.0f);
    }

    /* Calcium wave propagation */
    if (astro->config.enable_calcium_waves && state->calcium_wave_active) {
        /* Decay wave amplitude */
        state->calcium_wave_amplitude = exp_decay(state->calcium_wave_amplitude,
                                                   500.0f, delta_ms);
        if (state->calcium_wave_amplitude < 0.1f) {
            state->calcium_wave_active = false;
        }
    }

    astro->total_updates++;
    nimcp_platform_mutex_unlock(astro->mutex);
    return 0;
}

/**
 * @brief Internal unlocked version of trigger_calcium_wave
 * @note Must be called while holding astro->mutex
 */
static int trigger_calcium_wave_unlocked(
    astrocyte_plasticity_t astro,
    uint32_t source_id,
    float amplitude
) {
    if (!astro->config.enable_calcium_waves) return 0;

    astrocyte_state_t* state = &astro->states[source_id];
    state->calcium_wave_active = true;
    state->calcium_wave_amplitude = nimcp_clampf(amplitude, 0.0f, 1.0f);
    state->calcium_current += amplitude * 0.5f;  /* Boost calcium */

    astro->calcium_waves_triggered++;

    NIMCP_LOGGING_DEBUG("Calcium wave triggered in astrocyte %u", source_id);
    return 0;
}

int astrocyte_plasticity_trigger_calcium_wave(
    astrocyte_plasticity_t astro,
    uint32_t source_id,
    float amplitude
) {
    /* Guard clauses */
    NIMCP_API_CHECK_NULL(astro, -1, "Astrocyte system is NULL");
    NIMCP_API_CHECK(source_id < astro->num_astrocytes, -1, "Source astrocyte ID out of range");
    if (!astro->config.enable_calcium_waves) return 0;

    nimcp_platform_mutex_lock(astro->mutex);
    int result = trigger_calcium_wave_unlocked(astro, source_id, amplitude);
    nimcp_platform_mutex_unlock(astro->mutex);

    return result;
}

int astrocyte_plasticity_release_gliotransmitter(
    astrocyte_plasticity_t astro,
    uint32_t astrocyte_id,
    gliotransmitter_type_t type,
    float amount
) {
    /* Guard clauses */
    NIMCP_API_CHECK_NULL(astro, -1, "Astrocyte system is NULL");
    NIMCP_API_CHECK(astrocyte_id < astro->num_astrocytes, -1, "Astrocyte ID out of range");

    nimcp_platform_mutex_lock(astro->mutex);

    astrocyte_state_t* state = &astro->states[astrocyte_id];
    float clamped_amount = nimcp_clampf(amount, 0.0f, 1.0f);

    /* Update appropriate gliotransmitter level */
    switch (type) {
        case GLIOTRANSMITTER_D_SERINE:
            if (astro->config.enable_d_serine_modulation) {
                state->d_serine_level = nimcp_clampf(
                    state->d_serine_level + clamped_amount, 0.0f, 1.5f);
            }
            break;

        case GLIOTRANSMITTER_ATP:
            if (astro->config.enable_atp_signaling) {
                state->atp_release_level = nimcp_clampf(
                    state->atp_release_level + clamped_amount, 0.0f, 1.0f);
            }
            break;

        case GLIOTRANSMITTER_ADENOSINE:
            if (astro->config.enable_atp_signaling) {
                state->adenosine_level = nimcp_clampf(
                    state->adenosine_level + clamped_amount, 0.0f, 1.0f);
            }
            break;

        case GLIOTRANSMITTER_GLUTAMATE:
            /* Note: Astrocytes can release glutamate, but we mainly model uptake */
            break;

        default:
            nimcp_platform_mutex_unlock(astro->mutex);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "astrocyte_plasticity_release_gliotransmitter: invalid gliotransmitter type");
            return -1;
    }

    astro->gliotransmitter_releases++;

    /* BUG-6 fix: Copy callback + data under lock, release mutex, THEN invoke
     * callback to avoid holding mutex during user callback execution */
    void (*cb)(gliotransmitter_type_t, float, void*) = astro->config.gliotransmitter_callback;
    void* cb_data = astro->config.callback_user_data;

    nimcp_platform_mutex_unlock(astro->mutex);

    /* Invoke callback AFTER unlock */
    if (cb) {
        cb(type, clamped_amount, cb_data);
    }

    return 0;
}

/* ============================================================================
 * Astrocyte → Plasticity Implementation
 * ============================================================================ */

int astrocyte_plasticity_get_effects(
    const astrocyte_plasticity_t astro,
    uint32_t astrocyte_id,
    astrocyte_plasticity_effects_t* effects
) {
    /* Guard clauses */
    NIMCP_API_CHECK_NULL(astro, -1, "Astrocyte system is NULL");
    NIMCP_API_CHECK_NULL(effects, -1, "Effects output pointer is NULL");
    NIMCP_API_CHECK(astrocyte_id < astro->num_astrocytes, -1, "Astrocyte ID out of range");

    nimcp_platform_mutex_lock(astro->mutex);

    const astrocyte_state_t* state = &astro->states[astrocyte_id];

    /* NMDA receptor modulation from D-serine */
    effects->nmda_coagonist_factor = compute_d_serine_factor(state->d_serine_level);
    effects->ltp_capacity_modulation = effects->nmda_coagonist_factor;

    /* STDP window modulation (lower D-serine → narrower window) */
    /* BUG-9 fix: Guard against division by zero if baseline constant is redefined to 0 */
    float d_serine_baseline = ASTROCYTE_D_SERINE_BASELINE > 1e-6f
        ? ASTROCYTE_D_SERINE_BASELINE : 1e-6f;
    effects->stdp_window_modulation =
        0.7f + 0.3f * (state->d_serine_level / d_serine_baseline);

    /* Glutamate clearance effects */
    effects->glutamate_clearance_time =
        compute_glu_clearance_time(state->glutamate_uptake_rate);
    effects->spillover_factor =
        nimcp_clampf(1.0f - state->glutamate_uptake_rate, 0.0f, 0.5f);
    effects->epsc_duration_factor =
        1.0f + effects->spillover_factor * 0.5f;

    /* A1R metaplasticity */
    effects->a1r_inhibition = compute_a1r_inhibition(state->adenosine_level);
    effects->transmission_suppression = effects->a1r_inhibition * 0.6f;
    effects->plasticity_threshold_shift = effects->a1r_inhibition * 0.2f;

    /* Network coordination from calcium waves */
    if (state->calcium_wave_active) {
        effects->synchronization_factor = state->calcium_wave_amplitude;
        effects->spatial_correlation = 0.7f;
    } else {
        effects->synchronization_factor = 0.0f;
        effects->spatial_correlation = 0.0f;
    }

    nimcp_platform_mutex_unlock(astro->mutex);
    return 0;
}

float astrocyte_plasticity_get_d_serine_factor(
    const astrocyte_plasticity_t astro,
    uint32_t astrocyte_id
) {
    if (!astro) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "astrocyte_plasticity_get_d_serine_factor: astro is NULL");
        return 1.0f;
    }
    if (astrocyte_id >= astro->num_astrocytes) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "astrocyte_plasticity_get_d_serine_factor: astrocyte_id out of range");
        return 1.0f;
    }

    nimcp_platform_mutex_lock(astro->mutex);
    float d_serine = astro->states[astrocyte_id].d_serine_level;
    nimcp_platform_mutex_unlock(astro->mutex);

    return compute_d_serine_factor(d_serine);
}

float astrocyte_plasticity_get_glu_clearance_time(
    const astrocyte_plasticity_t astro,
    uint32_t astrocyte_id
) {
    if (!astro) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "astrocyte_plasticity_get_glu_clearance_time: astro is NULL");
        return ASTROCYTE_GLU_UPTAKE_TIME_MS;
    }
    if (astrocyte_id >= astro->num_astrocytes) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "astrocyte_plasticity_get_glu_clearance_time: astrocyte_id out of range");
        return ASTROCYTE_GLU_UPTAKE_TIME_MS;
    }

    nimcp_platform_mutex_lock(astro->mutex);
    float uptake_rate = astro->states[astrocyte_id].glutamate_uptake_rate;
    nimcp_platform_mutex_unlock(astro->mutex);

    return compute_glu_clearance_time(uptake_rate);
}

float astrocyte_plasticity_get_a1r_inhibition(
    const astrocyte_plasticity_t astro,
    uint32_t astrocyte_id
) {
    if (!astro) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "astrocyte_plasticity_get_a1r_inhibition: astro is NULL");
        return 0.0f;
    }
    if (astrocyte_id >= astro->num_astrocytes) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "astrocyte_plasticity_get_a1r_inhibition: astrocyte_id out of range");
        return 0.0f;
    }

    nimcp_platform_mutex_lock(astro->mutex);
    float adenosine = astro->states[astrocyte_id].adenosine_level;
    nimcp_platform_mutex_unlock(astro->mutex);

    return compute_a1r_inhibition(adenosine);
}

/* ============================================================================
 * Plasticity → Astrocyte Implementation
 * ============================================================================ */

int astrocyte_plasticity_notify_glutamate_release(
    astrocyte_plasticity_t astro,
    uint32_t astrocyte_id,
    float glutamate_amount
) {
    /* Guard clauses */
    NIMCP_API_CHECK_NULL(astro, -1, "Astrocyte system is NULL");
    NIMCP_API_CHECK(astrocyte_id < astro->num_astrocytes, -1, "Astrocyte ID out of range");

    nimcp_platform_mutex_lock(astro->mutex);

    astrocyte_state_t* state = &astro->states[astrocyte_id];

    /* Glutamate activates mGluRs, triggers calcium */
    float calcium_increase = glutamate_amount * 0.3f;
    state->calcium_current += calcium_increase;
    state->calcium_current = nimcp_clampf(state->calcium_current, 0.0f, 2.0f);

    /* High calcium can trigger wave - use unlocked version since we hold mutex */
    if (state->calcium_current > astro->config.ca_wave_trigger_threshold) {
        if (!state->calcium_wave_active && astro->config.enable_calcium_waves) {
            trigger_calcium_wave_unlocked(astro, astrocyte_id, 0.7f);
        }
    }

    nimcp_platform_mutex_unlock(astro->mutex);
    return 0;
}

int astrocyte_plasticity_notify_ltp_induction(
    astrocyte_plasticity_t astro,
    uint32_t astrocyte_id
) {
    /* Guard clauses */
    NIMCP_API_CHECK_NULL(astro, -1, "Astrocyte system is NULL");
    NIMCP_API_CHECK(astrocyte_id < astro->num_astrocytes, -1, "Astrocyte ID out of range");

    nimcp_platform_mutex_lock(astro->mutex);

    astrocyte_state_t* state = &astro->states[astrocyte_id];

    /* LTP induction → strong astrocyte activation */
    state->calcium_current += 0.5f;

    /* Increase D-serine release to support further LTP */
    if (astro->config.enable_d_serine_modulation) {
        state->d_serine_level = nimcp_clampf(state->d_serine_level + 0.2f, 0.0f, 1.5f);
    }

    /* Trigger calcium wave for network coordination - use unlocked version since we hold mutex */
    if (astro->config.enable_calcium_waves) {
        trigger_calcium_wave_unlocked(astro, astrocyte_id, 0.8f);
    }

    nimcp_platform_mutex_unlock(astro->mutex);
    return 0;
}

/* ============================================================================
 * Reactive Astrogliosis Implementation
 * ============================================================================ */

int astrocyte_plasticity_set_reactive_state(
    astrocyte_plasticity_t astro,
    uint32_t astrocyte_id,
    astrocyte_reactive_state_t state_type,
    float intensity
) {
    /* Guard clauses */
    NIMCP_API_CHECK_NULL(astro, -1, "Astrocyte system is NULL");
    NIMCP_API_CHECK(astrocyte_id < astro->num_astrocytes, -1, "Astrocyte ID out of range");
    if (!astro->config.enable_reactive_astrogliosis) return 0;

    nimcp_platform_mutex_lock(astro->mutex);

    astrocyte_state_t* state = &astro->states[astrocyte_id];
    float clamped_intensity = nimcp_clampf(intensity, 0.0f, 1.0f);

    state->reactive_state = state_type;

    switch (state_type) {
        case ASTROCYTE_RESTING:
            /* Return to baseline */
            state->a1_factor = 0.0f;
            state->a2_factor = 0.0f;
            state->d_serine_level = astro->config.baseline_d_serine;
            state->glutamate_uptake_rate = astro->config.baseline_glu_uptake;
            break;

        case ASTROCYTE_A1_REACTIVE:
            /* Neurotoxic state: reduced D-serine, impaired uptake
             * Intensity modulates the severity of reduction */
            state->a1_factor = clamped_intensity;
            state->a2_factor = 0.0f;
            state->d_serine_level = astro->config.baseline_d_serine -
                (astro->config.baseline_d_serine - ASTROCYTE_D_SERINE_A1_REDUCTION) * clamped_intensity;
            state->glutamate_uptake_rate = astro->config.baseline_glu_uptake -
                (astro->config.baseline_glu_uptake - ASTROCYTE_GLU_UPTAKE_A1_IMPAIRED) * clamped_intensity;
            break;

        case ASTROCYTE_A2_REACTIVE:
            /* Neuroprotective state: enhanced support */
            state->a1_factor = 0.0f;
            state->a2_factor = clamped_intensity;
            state->d_serine_level = astro->config.baseline_d_serine * 1.2f;
            state->glutamate_uptake_rate = ASTROCYTE_GLU_UPTAKE_FAST;
            break;

        case ASTROCYTE_MIXED_REACTIVE:
            /* Mixed phenotype */
            state->a1_factor = clamped_intensity * 0.5f;
            state->a2_factor = clamped_intensity * 0.5f;
            break;
    }

    nimcp_platform_mutex_unlock(astro->mutex);

    NIMCP_LOGGING_INFO("Astrocyte %u set to reactive state %d (intensity %.2f)",
                       astrocyte_id, state_type, clamped_intensity);
    return 0;
}

astrocyte_reactive_state_t astrocyte_plasticity_get_reactive_state(
    const astrocyte_plasticity_t astro,
    uint32_t astrocyte_id
) {
    if (!astro) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "astrocyte_plasticity_get_reactive_state: astro is NULL");
        return ASTROCYTE_RESTING;
    }
    if (astrocyte_id >= astro->num_astrocytes) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "astrocyte_plasticity_get_reactive_state: astrocyte_id out of range");
        return ASTROCYTE_RESTING;
    }

    nimcp_platform_mutex_lock(astro->mutex);
    astrocyte_reactive_state_t state = astro->states[astrocyte_id].reactive_state;
    nimcp_platform_mutex_unlock(astro->mutex);

    return state;
}

/* ============================================================================
 * Query Implementation
 * ============================================================================ */

int astrocyte_plasticity_get_state(
    const astrocyte_plasticity_t astro,
    uint32_t astrocyte_id,
    astrocyte_state_t* state
) {
    /* Guard clauses */
    NIMCP_API_CHECK_NULL(astro, -1, "Astrocyte system is NULL");
    NIMCP_API_CHECK_NULL(state, -1, "State output pointer is NULL");
    NIMCP_API_CHECK(astrocyte_id < astro->num_astrocytes, -1, "Astrocyte ID out of range");

    nimcp_platform_mutex_lock(astro->mutex);
    memcpy(state, &astro->states[astrocyte_id], sizeof(astrocyte_state_t));
    nimcp_platform_mutex_unlock(astro->mutex);

    return 0;
}

bool astrocyte_plasticity_is_calcium_wave_active(
    const astrocyte_plasticity_t astro,
    uint32_t astrocyte_id
) {
    if (!astro) {
        return false;
    }
    if (astrocyte_id >= astro->num_astrocytes) {
        return false;
    }

    nimcp_platform_mutex_lock(astro->mutex);
    bool active = astro->states[astrocyte_id].calcium_wave_active;
    nimcp_platform_mutex_unlock(astro->mutex);

    return active;
}

uint32_t astrocyte_plasticity_get_num_astrocytes(
    const astrocyte_plasticity_t astro
) {
    if (!astro) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "astrocyte_plasticity_get_num_astrocytes: astro is NULL");
        return 0;
    }
    return astro->num_astrocytes;
}
