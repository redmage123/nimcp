/**
 * @file nimcp_raphe.c
 * @brief Raphe Nuclei implementation - Serotonin/Mood Regulation Center
 * @date 2026-01-11
 */

#include "core/brain/regions/raphe/nimcp_raphe.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_math_constants.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE_MESH_ONLY(raphe, MESH_ADAPTER_CATEGORY_COGNITIVE)

static float lerp(float a, float b, float t) {
    return a + t * (b - a);
}

/**
 * @brief Hyperbolic discounting function
 */
static float hyperbolic_discount(float amount, float delay, float k) {
    if (delay <= 0.0f) return amount;
    return amount / (1.0f + k * delay);
}

/*=============================================================================
 * Error Strings
 *===========================================================================*/

static const char* raphe_error_strings[] = {
    "Success",
    "Null pointer",
    "System not initialized",
    "System already initialized",
    "Invalid parameter",
    "Capacity exceeded",
    "Not found",
    "Invalid state",
    "Internal error"
};

const char* nimcp_raphe_error_string(nimcp_raphe_error_t error) {
    if (error < 0 || error > RAPHE_ERROR_INTERNAL) {
        return "Unknown error";
    }
    return raphe_error_strings[error];
}

/*=============================================================================
 * Lifecycle API
 *===========================================================================*/

nimcp_raphe_config_t nimcp_raphe_default_config(void) {
    nimcp_raphe_config_t config = {
        .baseline_firing_rate = RAPHE_DEFAULT_TONIC_RATE,
        .baseline_5ht = RAPHE_DEFAULT_5HT_BASELINE,
        .ht_decay_tau = 500.0f,        /* 500ms decay time constant */
        .mood_time_constant = 10000.0f, /* 10s mood change time */
        .impulse_sensitivity = 0.5f,
        .enable_autoreceptors = true,
        .autoreceptor_sensitivity = 0.3f
    };
    return config;
}

nimcp_raphe_error_t nimcp_raphe_init(nimcp_raphe_system_t* raphe,
                                     const nimcp_raphe_config_t* config) {
    if (!raphe) return RAPHE_ERROR_NULL;
    if (raphe->initialized) return RAPHE_ERROR_ALREADY_INITIALIZED;

    memset(raphe, 0, sizeof(nimcp_raphe_system_t));

    /* Apply configuration */
    if (config) {
        raphe->config = *config;
    } else {
        raphe->config = nimcp_raphe_default_config();
    }

    /* Initialize neuron pool */
    raphe->neurons.membrane_potential = -65.0f;  /* mV resting */
    raphe->neurons.firing_rate = raphe->config.baseline_firing_rate;
    raphe->neurons.excitatory_input = 0.0f;
    raphe->neurons.inhibitory_input = 0.0f;
    raphe->neurons.refractory_time = 0.0f;
    raphe->neurons.spike_count = 0;
    raphe->neurons.in_burst = false;

    /* Initialize 5-HT state */
    raphe->ht_concentration = raphe->config.baseline_5ht;
    raphe->ht_release_rate = 0.0f;
    raphe->tonic_firing_rate = raphe->config.baseline_firing_rate;

    /* Initialize mode */
    raphe->mode = RAPHE_MODE_TONIC;
    raphe->mode_duration = 0.0f;

    /* Initialize mood */
    raphe->mood.valence = RAPHE_DEFAULT_MOOD_NEUTRAL;
    raphe->mood.stability = 0.7f;
    raphe->mood.anxiety = 0.3f;
    raphe->mood.irritability = 0.2f;
    raphe->mood.state = MOOD_NEUTRAL;
    raphe->mood.mood_momentum = 0.0f;

    /* Initialize impulse control */
    raphe->impulse.inhibition_strength = 0.6f;
    raphe->impulse.patience = 0.5f;
    raphe->impulse.risk_aversion = 0.5f;
    raphe->impulse.impulsivity = 0.4f;

    /* Initialize temporal discounting */
    raphe->temporal.discount_rate = 0.1f;
    raphe->temporal.future_orientation = 0.5f;
    raphe->temporal.delay_tolerance = 0.5f;

    /* Initialize sleep-wake */
    raphe->sleep_wake.sleep_pressure = 0.0f;
    raphe->sleep_wake.wake_promotion = 0.7f;
    raphe->sleep_wake.circadian_phase = 12.0f;  /* Noon */

    /* Initialize projections */
    raphe->num_projections = 0;

    /* Initialize metrics */
    memset(&raphe->metrics, 0, sizeof(nimcp_raphe_metrics_t));

    /* Initialize timing */
    raphe->simulation_time = 0.0f;

    /* Initialize status */
    raphe->status = RAPHE_STATUS_NORMAL;

    raphe->initialized = true;
    return RAPHE_OK;
}

nimcp_raphe_error_t nimcp_raphe_shutdown(nimcp_raphe_system_t* raphe) {
    if (!raphe) return RAPHE_ERROR_NULL;
    if (!raphe->initialized) return RAPHE_ERROR_NOT_INITIALIZED;

    raphe->initialized = false;
    return RAPHE_OK;
}

nimcp_raphe_error_t nimcp_raphe_reset(nimcp_raphe_system_t* raphe) {
    if (!raphe) return RAPHE_ERROR_NULL;

    nimcp_raphe_config_t config = raphe->config;
    raphe->initialized = false;
    return nimcp_raphe_init(raphe, &config);
}

/*=============================================================================
 * Update API
 *===========================================================================*/

nimcp_raphe_error_t nimcp_raphe_update(nimcp_raphe_system_t* raphe, float dt) {
    if (!raphe) return RAPHE_ERROR_NULL;
    if (!raphe->initialized) return RAPHE_ERROR_NOT_INITIALIZED;

    float dt_sec = dt / 1000.0f;

    /* 1. Update neuron pool firing */
    float net_input = raphe->neurons.excitatory_input - raphe->neurons.inhibitory_input;

    /* Mode-based firing rate adjustment */
    float target_rate = raphe->config.baseline_firing_rate;
    switch (raphe->mode) {
        case RAPHE_MODE_ELEVATED:
            target_rate *= 2.0f;
            break;
        case RAPHE_MODE_SUPPRESSED:
            target_rate *= 0.3f;
            break;
        case RAPHE_MODE_SLEEP:
            target_rate *= 0.1f;
            break;
        default:
            break;
    }

    /* Apply net input */
    target_rate += net_input * 5.0f;
    target_rate = nimcp_clampf(target_rate, 0.1f, RAPHE_PHASIC_MAX_RATE);

    /* Smooth firing rate transition */
    float rate_alpha = 1.0f - expf(-dt / 200.0f);
    raphe->neurons.firing_rate = lerp(raphe->neurons.firing_rate, target_rate, rate_alpha);

    /* Autoreceptor feedback (5-HT1A inhibits firing) */
    if (raphe->config.enable_autoreceptors) {
        float ht_ratio = (fabsf(raphe->config.baseline_5ht) > 1e-10f) ?
            (raphe->ht_concentration / raphe->config.baseline_5ht) : 1.0f;
        float autoreceptor_inhibition = (ht_ratio - 1.0f) *
                                        raphe->config.autoreceptor_sensitivity;
        raphe->neurons.firing_rate -= autoreceptor_inhibition;
        raphe->neurons.firing_rate = nimcp_clampf(raphe->neurons.firing_rate, 0.1f,
                                             RAPHE_PHASIC_MAX_RATE);
    }

    /* 2. Update 5-HT concentration */
    /* Release based on firing rate */
    float release_per_spike = 0.5f;  /* nM per spike */
    float release = raphe->neurons.firing_rate * release_per_spike * dt_sec;

    /* Decay (reuptake + metabolism) */
    float decay_rate = (fabsf(raphe->config.ht_decay_tau) > 1e-10f) ? (1.0f / raphe->config.ht_decay_tau) : 0.0f;
    float decay = raphe->ht_concentration * decay_rate * dt;

    raphe->ht_concentration += release - decay;
    raphe->ht_concentration = nimcp_clampf(raphe->ht_concentration, 1.0f, 200.0f);
    raphe->ht_release_rate = (fabsf(dt_sec) > 1e-10f) ? (release / dt_sec) : 0.0f;

    /* 3. Update mood state */
    float ht_ratio = (fabsf(raphe->config.baseline_5ht) > 1e-10f) ?
        (raphe->ht_concentration / raphe->config.baseline_5ht) : 1.0f;

    /* Higher 5-HT -> better mood (more positive valence) */
    float mood_target = (ht_ratio - 1.0f) * 0.5f;
    mood_target = nimcp_clampf(mood_target, -1.0f, 1.0f);

    float mood_alpha = (fabsf(raphe->config.mood_time_constant) > 1e-10f) ?
        (1.0f - expf(-dt / raphe->config.mood_time_constant)) : 1.0f;
    float old_valence = raphe->mood.valence;
    raphe->mood.valence = lerp(raphe->mood.valence, mood_target, mood_alpha);
    raphe->mood.mood_momentum = (fabsf(dt_sec) > 1e-10f) ?
        ((raphe->mood.valence - old_valence) / dt_sec) : 0.0f;

    /* Update stability (higher 5-HT -> more stable) */
    float stability_target = 0.5f + (ht_ratio - 1.0f) * 0.3f;
    stability_target = nimcp_clampf(stability_target, 0.2f, 0.95f);
    raphe->mood.stability = lerp(raphe->mood.stability, stability_target, mood_alpha * 0.5f);

    /* Update anxiety (lower 5-HT -> more anxiety) */
    float anxiety_target = 0.5f - (ht_ratio - 1.0f) * 0.4f;
    anxiety_target = nimcp_clampf(anxiety_target, 0.0f, 1.0f);
    raphe->mood.anxiety = lerp(raphe->mood.anxiety, anxiety_target, mood_alpha);

    /* Update irritability */
    float irritability_target = 0.3f - (ht_ratio - 1.0f) * 0.3f;
    irritability_target = nimcp_clampf(irritability_target, 0.0f, 1.0f);
    raphe->mood.irritability = lerp(raphe->mood.irritability, irritability_target, mood_alpha);

    /* Categorize mood state */
    nimcp_mood_state_t prev_state = raphe->mood.state;
    if (raphe->mood.valence < -0.6f) {
        raphe->mood.state = MOOD_SEVERELY_DEPRESSED;
    } else if (raphe->mood.valence < -0.3f) {
        raphe->mood.state = MOOD_DEPRESSED;
    } else if (raphe->mood.valence < 0.3f) {
        raphe->mood.state = MOOD_NEUTRAL;
    } else if (raphe->mood.valence < 0.6f) {
        raphe->mood.state = MOOD_POSITIVE;
    } else {
        raphe->mood.state = MOOD_EUPHORIC;
    }

    if (raphe->mood.state != prev_state) {
        raphe->metrics.mood_transitions++;
    }

    /* 4. Update impulse control state */
    /* Higher 5-HT -> stronger inhibition */
    float inhibition_target = 0.5f + (ht_ratio - 1.0f) * raphe->config.impulse_sensitivity;
    inhibition_target = nimcp_clampf(inhibition_target, 0.1f, 0.95f);
    raphe->impulse.inhibition_strength = lerp(raphe->impulse.inhibition_strength,
                                               inhibition_target, mood_alpha);
    raphe->impulse.impulsivity = 1.0f - raphe->impulse.inhibition_strength;

    /* Higher 5-HT -> more patience */
    float patience_target = 0.5f + (ht_ratio - 1.0f) * 0.4f;
    patience_target = nimcp_clampf(patience_target, 0.1f, 0.9f);
    raphe->impulse.patience = lerp(raphe->impulse.patience, patience_target, mood_alpha);

    /* Higher 5-HT -> more risk aversion */
    float risk_target = 0.5f + (ht_ratio - 1.0f) * 0.3f;
    risk_target = nimcp_clampf(risk_target, 0.1f, 0.9f);
    raphe->impulse.risk_aversion = lerp(raphe->impulse.risk_aversion, risk_target, mood_alpha);

    /* 5. Update temporal discounting */
    /* Higher 5-HT -> lower discount rate (more patient) */
    float k_target = 0.1f - (ht_ratio - 1.0f) * 0.05f;
    k_target = nimcp_clampf(k_target, 0.01f, 0.5f);
    raphe->temporal.discount_rate = lerp(raphe->temporal.discount_rate, k_target, mood_alpha);

    /* Higher 5-HT -> more future-oriented */
    float future_target = 0.5f + (ht_ratio - 1.0f) * 0.3f;
    future_target = nimcp_clampf(future_target, 0.1f, 0.9f);
    raphe->temporal.future_orientation = lerp(raphe->temporal.future_orientation,
                                               future_target, mood_alpha);

    raphe->temporal.delay_tolerance = (raphe->impulse.patience +
                                        raphe->temporal.future_orientation) / 2.0f;

    /* 6. Update projections */
    for (uint32_t i = 0; i < raphe->num_projections; i++) {
        if (raphe->projections[i].enabled) {
            raphe->projections[i].ht_delivered = raphe->ht_concentration *
                                                  raphe->projections[i].weight;
        }
    }

    /* 7. Update status */
    if (raphe->ht_concentration < raphe->config.baseline_5ht * 0.5f) {
        raphe->status = RAPHE_STATUS_HYPOSEROTONERGIC;
    } else if (raphe->ht_concentration > raphe->config.baseline_5ht * 2.0f) {
        raphe->status = RAPHE_STATUS_HYPERSEROTONERGIC;
    } else {
        raphe->status = RAPHE_STATUS_NORMAL;
    }

    /* 8. Decay inputs */
    raphe->neurons.excitatory_input *= (1.0f - 0.1f * dt_sec);
    raphe->neurons.inhibitory_input *= (1.0f - 0.1f * dt_sec);

    /* 9. Update metrics */
    raphe->metrics.update_count++;
    raphe->metrics.total_simulation_time += dt_sec;
    raphe->metrics.total_5ht_released += release;

    float avg_alpha = 0.001f;
    raphe->metrics.avg_mood_valence = lerp(raphe->metrics.avg_mood_valence,
                                           raphe->mood.valence, avg_alpha);

    if (raphe->mood.valence < -0.3f) {
        raphe->metrics.time_depressed += dt_sec;
    }
    if (raphe->mood.valence > 0.3f) {
        raphe->metrics.time_positive += dt_sec;
    }

    /* 10. Update mode duration */
    raphe->mode_duration += dt;
    raphe->simulation_time += dt;

    return RAPHE_OK;
}

/*=============================================================================
 * Mood Regulation API
 *===========================================================================*/

nimcp_raphe_error_t nimcp_raphe_get_mood(nimcp_raphe_system_t* raphe, float* valence) {
    if (!raphe || !valence) return RAPHE_ERROR_NULL;
    if (!raphe->initialized) return RAPHE_ERROR_NOT_INITIALIZED;

    *valence = raphe->mood.valence;
    return RAPHE_OK;
}

nimcp_raphe_error_t nimcp_raphe_get_mood_state(nimcp_raphe_system_t* raphe,
                                               nimcp_mood_state_t* state) {
    if (!raphe || !state) return RAPHE_ERROR_NULL;
    if (!raphe->initialized) return RAPHE_ERROR_NOT_INITIALIZED;

    *state = raphe->mood.state;
    return RAPHE_OK;
}

nimcp_raphe_error_t nimcp_raphe_apply_mood_input(nimcp_raphe_system_t* raphe, float input) {
    if (!raphe) return RAPHE_ERROR_NULL;
    if (!raphe->initialized) return RAPHE_ERROR_NOT_INITIALIZED;

    /* Positive input -> increase mood valence target */
    raphe->mood.valence += input * 0.1f;
    raphe->mood.valence = nimcp_clampf(raphe->mood.valence, -1.0f, 1.0f);
    return RAPHE_OK;
}

nimcp_raphe_error_t nimcp_raphe_get_anxiety(nimcp_raphe_system_t* raphe, float* anxiety) {
    if (!raphe || !anxiety) return RAPHE_ERROR_NULL;
    if (!raphe->initialized) return RAPHE_ERROR_NOT_INITIALIZED;

    *anxiety = raphe->mood.anxiety;
    return RAPHE_OK;
}

nimcp_raphe_error_t nimcp_raphe_modulate_anxiety(nimcp_raphe_system_t* raphe, float input) {
    if (!raphe) return RAPHE_ERROR_NULL;
    if (!raphe->initialized) return RAPHE_ERROR_NOT_INITIALIZED;

    raphe->mood.anxiety += input * 0.2f;
    raphe->mood.anxiety = nimcp_clampf(raphe->mood.anxiety, 0.0f, 1.0f);
    return RAPHE_OK;
}

/*=============================================================================
 * Impulse Control API
 *===========================================================================*/

nimcp_raphe_error_t nimcp_raphe_get_inhibition(nimcp_raphe_system_t* raphe, float* inhibition) {
    if (!raphe || !inhibition) return RAPHE_ERROR_NULL;
    if (!raphe->initialized) return RAPHE_ERROR_NOT_INITIALIZED;

    *inhibition = raphe->impulse.inhibition_strength;
    return RAPHE_OK;
}

nimcp_raphe_error_t nimcp_raphe_get_patience(nimcp_raphe_system_t* raphe, float* patience) {
    if (!raphe || !patience) return RAPHE_ERROR_NULL;
    if (!raphe->initialized) return RAPHE_ERROR_NOT_INITIALIZED;

    *patience = raphe->impulse.patience;
    return RAPHE_OK;
}

nimcp_raphe_error_t nimcp_raphe_get_impulsivity(nimcp_raphe_system_t* raphe, float* impulsivity) {
    if (!raphe || !impulsivity) return RAPHE_ERROR_NULL;
    if (!raphe->initialized) return RAPHE_ERROR_NOT_INITIALIZED;

    *impulsivity = raphe->impulse.impulsivity;
    return RAPHE_OK;
}

nimcp_raphe_error_t nimcp_raphe_compute_inhibition(nimcp_raphe_system_t* raphe,
                                                   float impulse_strength,
                                                   float* inhibition_output) {
    if (!raphe || !inhibition_output) return RAPHE_ERROR_NULL;
    if (!raphe->initialized) return RAPHE_ERROR_NOT_INITIALIZED;

    /* Net inhibition = inhibition strength - impulse strength */
    float net = raphe->impulse.inhibition_strength - impulse_strength;
    *inhibition_output = nimcp_clampf(net, -1.0f, 1.0f);
    return RAPHE_OK;
}

/*=============================================================================
 * Temporal Discounting API
 *===========================================================================*/

nimcp_raphe_error_t nimcp_raphe_get_discount_rate(nimcp_raphe_system_t* raphe, float* rate) {
    if (!raphe || !rate) return RAPHE_ERROR_NULL;
    if (!raphe->initialized) return RAPHE_ERROR_NOT_INITIALIZED;

    *rate = raphe->temporal.discount_rate;
    return RAPHE_OK;
}

nimcp_raphe_error_t nimcp_raphe_discount_value(nimcp_raphe_system_t* raphe,
                                               float value,
                                               float delay,
                                               float* discounted_value) {
    if (!raphe || !discounted_value) return RAPHE_ERROR_NULL;
    if (!raphe->initialized) return RAPHE_ERROR_NOT_INITIALIZED;
    if (value < 0.0f) return RAPHE_ERROR_INVALID_PARAM;

    *discounted_value = hyperbolic_discount(value, delay, raphe->temporal.discount_rate);
    return RAPHE_OK;
}

nimcp_raphe_error_t nimcp_raphe_get_future_orientation(nimcp_raphe_system_t* raphe,
                                                       float* orientation) {
    if (!raphe || !orientation) return RAPHE_ERROR_NULL;
    if (!raphe->initialized) return RAPHE_ERROR_NOT_INITIALIZED;

    *orientation = raphe->temporal.future_orientation;
    return RAPHE_OK;
}

/*=============================================================================
 * 5-HT Control API
 *===========================================================================*/

nimcp_raphe_error_t nimcp_raphe_apply_excitation(nimcp_raphe_system_t* raphe, float strength) {
    if (!raphe) return RAPHE_ERROR_NULL;
    if (!raphe->initialized) return RAPHE_ERROR_NOT_INITIALIZED;

    raphe->neurons.excitatory_input += nimcp_clampf(strength, 0.0f, 1.0f);
    return RAPHE_OK;
}

nimcp_raphe_error_t nimcp_raphe_apply_inhibition(nimcp_raphe_system_t* raphe, float strength) {
    if (!raphe) return RAPHE_ERROR_NULL;
    if (!raphe->initialized) return RAPHE_ERROR_NOT_INITIALIZED;

    raphe->neurons.inhibitory_input += nimcp_clampf(strength, 0.0f, 1.0f);
    return RAPHE_OK;
}

nimcp_raphe_error_t nimcp_raphe_get_5ht(nimcp_raphe_system_t* raphe, float* ht) {
    if (!raphe || !ht) return RAPHE_ERROR_NULL;
    if (!raphe->initialized) return RAPHE_ERROR_NOT_INITIALIZED;

    *ht = raphe->ht_concentration;
    return RAPHE_OK;
}

nimcp_raphe_error_t nimcp_raphe_get_firing_rate(nimcp_raphe_system_t* raphe, float* rate) {
    if (!raphe || !rate) return RAPHE_ERROR_NULL;
    if (!raphe->initialized) return RAPHE_ERROR_NOT_INITIALIZED;

    *rate = raphe->neurons.firing_rate;
    return RAPHE_OK;
}

/*=============================================================================
 * Mode API
 *===========================================================================*/

nimcp_raphe_error_t nimcp_raphe_get_mode(nimcp_raphe_system_t* raphe, nimcp_raphe_mode_t* mode) {
    if (!raphe || !mode) return RAPHE_ERROR_NULL;
    if (!raphe->initialized) return RAPHE_ERROR_NOT_INITIALIZED;

    *mode = raphe->mode;
    return RAPHE_OK;
}

nimcp_raphe_error_t nimcp_raphe_set_mode(nimcp_raphe_system_t* raphe, nimcp_raphe_mode_t mode) {
    if (!raphe) return RAPHE_ERROR_NULL;
    if (!raphe->initialized) return RAPHE_ERROR_NOT_INITIALIZED;
    if (mode > RAPHE_MODE_SLEEP) return RAPHE_ERROR_INVALID_PARAM;

    raphe->mode = mode;
    raphe->mode_duration = 0.0f;
    return RAPHE_OK;
}

/*=============================================================================
 * Projection API
 *===========================================================================*/

nimcp_raphe_error_t nimcp_raphe_add_projection(nimcp_raphe_system_t* raphe,
                                               nimcp_raphe_target_t target,
                                               const char* name,
                                               float weight,
                                               uint32_t* id) {
    if (!raphe || !name || !id) return RAPHE_ERROR_NULL;
    if (!raphe->initialized) return RAPHE_ERROR_NOT_INITIALIZED;
    if (raphe->num_projections >= RAPHE_MAX_PROJECTIONS) return RAPHE_ERROR_CAPACITY;
    if (target >= RAPHE_TARGET_COUNT) return RAPHE_ERROR_INVALID_PARAM;

    nimcp_raphe_projection_t* proj = &raphe->projections[raphe->num_projections];

    proj->id = raphe->num_projections;
    proj->target = target;
    strncpy(proj->name, name, RAPHE_MAX_NAME_LEN - 1);
    proj->name[RAPHE_MAX_NAME_LEN - 1] = '\0';
    proj->weight = nimcp_clampf(weight, 0.0f, 1.0f);
    proj->ht_delivered = 0.0f;
    proj->enabled = true;

    for (int i = 0; i < HT_RECEPTOR_COUNT; i++) {
        proj->receptor_activation[i] = 0.0f;
    }

    *id = proj->id;
    raphe->num_projections++;

    return RAPHE_OK;
}

nimcp_raphe_projection_t* nimcp_raphe_get_projection(nimcp_raphe_system_t* raphe, uint32_t id) {
    if (!raphe || !raphe->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_raphe_get_projection: parameter is NULL");
        return NULL;
    }
    if (id >= raphe->num_projections) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_raphe_get_projection: parameter is NULL");
        return NULL;
    }

    return &raphe->projections[id];
}

nimcp_raphe_projection_t* nimcp_raphe_get_projection_by_target(nimcp_raphe_system_t* raphe,
                                                               nimcp_raphe_target_t target) {
    if (!raphe || !raphe->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_raphe_get_projection_by_target: parameter is NULL");
        return NULL;
    }

    for (uint32_t i = 0; i < raphe->num_projections; i++) {
        if (raphe->projections[i].target == target) {
            return &raphe->projections[i];
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_raphe_get_projection: validation failed");
    return NULL;
}

nimcp_raphe_error_t nimcp_raphe_get_5ht_at_target(nimcp_raphe_system_t* raphe,
                                                  nimcp_raphe_target_t target,
                                                  float* ht) {
    if (!raphe || !ht) return RAPHE_ERROR_NULL;
    if (!raphe->initialized) return RAPHE_ERROR_NOT_INITIALIZED;

    nimcp_raphe_projection_t* proj = nimcp_raphe_get_projection_by_target(raphe, target);
    if (!proj) return RAPHE_ERROR_NOT_FOUND;

    *ht = proj->ht_delivered;
    return RAPHE_OK;
}

/*=============================================================================
 * Sleep-Wake API
 *===========================================================================*/

nimcp_raphe_error_t nimcp_raphe_get_sleep_pressure(nimcp_raphe_system_t* raphe, float* pressure) {
    if (!raphe || !pressure) return RAPHE_ERROR_NULL;
    if (!raphe->initialized) return RAPHE_ERROR_NOT_INITIALIZED;

    *pressure = raphe->sleep_wake.sleep_pressure;
    return RAPHE_OK;
}

nimcp_raphe_error_t nimcp_raphe_set_circadian_phase(nimcp_raphe_system_t* raphe, float phase) {
    if (!raphe) return RAPHE_ERROR_NULL;
    if (!raphe->initialized) return RAPHE_ERROR_NOT_INITIALIZED;

    raphe->sleep_wake.circadian_phase = fmodf(phase, 24.0f);
    if (raphe->sleep_wake.circadian_phase < 0.0f) {
        raphe->sleep_wake.circadian_phase += 24.0f;
    }
    return RAPHE_OK;
}

nimcp_raphe_error_t nimcp_raphe_update_sleep_wake(nimcp_raphe_system_t* raphe, float dt) {
    if (!raphe) return RAPHE_ERROR_NULL;
    if (!raphe->initialized) return RAPHE_ERROR_NOT_INITIALIZED;

    float dt_hours = dt / (1000.0f * 3600.0f);

    /* Update circadian phase */
    raphe->sleep_wake.circadian_phase += dt_hours;
    if (raphe->sleep_wake.circadian_phase >= 24.0f) {
        raphe->sleep_wake.circadian_phase -= 24.0f;
    }

    /* Sleep pressure builds during wakefulness */
    if (raphe->mode != RAPHE_MODE_SLEEP) {
        raphe->sleep_wake.sleep_pressure += 0.01f * dt_hours;
        raphe->sleep_wake.sleep_pressure = nimcp_clampf(raphe->sleep_wake.sleep_pressure, 0.0f, 1.0f);
    } else {
        /* Sleep pressure dissipates during sleep */
        raphe->sleep_wake.sleep_pressure -= 0.02f * dt_hours;
        raphe->sleep_wake.sleep_pressure = nimcp_clampf(raphe->sleep_wake.sleep_pressure, 0.0f, 1.0f);
    }

    /* Circadian modulation of wake promotion */
    /* Peak wakefulness around noon (12), minimum around 3am */
    float circadian_factor = cosf((raphe->sleep_wake.circadian_phase - 12.0f) * NIMCP_PI_F / 12.0f);
    raphe->sleep_wake.wake_promotion = 0.5f + 0.3f * circadian_factor -
                                        0.3f * raphe->sleep_wake.sleep_pressure;
    raphe->sleep_wake.wake_promotion = nimcp_clampf(raphe->sleep_wake.wake_promotion, 0.0f, 1.0f);

    return RAPHE_OK;
}

/*=============================================================================
 * Status API
 *===========================================================================*/

nimcp_raphe_error_t nimcp_raphe_get_state(nimcp_raphe_system_t* raphe,
                                          float* ht,
                                          float* mood,
                                          float* anxiety,
                                          nimcp_raphe_mode_t* mode) {
    if (!raphe) return RAPHE_ERROR_NULL;
    if (!raphe->initialized) return RAPHE_ERROR_NOT_INITIALIZED;

    if (ht) *ht = raphe->ht_concentration;
    if (mood) *mood = raphe->mood.valence;
    if (anxiety) *anxiety = raphe->mood.anxiety;
    if (mode) *mode = raphe->mode;

    return RAPHE_OK;
}

nimcp_raphe_error_t nimcp_raphe_get_status(nimcp_raphe_system_t* raphe,
                                           nimcp_raphe_status_t* status) {
    if (!raphe || !status) return RAPHE_ERROR_NULL;
    if (!raphe->initialized) return RAPHE_ERROR_NOT_INITIALIZED;

    *status = raphe->status;
    return RAPHE_OK;
}

nimcp_raphe_error_t nimcp_raphe_get_metrics(nimcp_raphe_system_t* raphe,
                                            nimcp_raphe_metrics_t* metrics) {
    if (!raphe || !metrics) return RAPHE_ERROR_NULL;
    if (!raphe->initialized) return RAPHE_ERROR_NOT_INITIALIZED;

    *metrics = raphe->metrics;
    return RAPHE_OK;
}

nimcp_raphe_error_t nimcp_raphe_reset_metrics(nimcp_raphe_system_t* raphe) {
    if (!raphe) return RAPHE_ERROR_NULL;
    if (!raphe->initialized) return RAPHE_ERROR_NOT_INITIALIZED;

    memset(&raphe->metrics, 0, sizeof(nimcp_raphe_metrics_t));
    return RAPHE_OK;
}
