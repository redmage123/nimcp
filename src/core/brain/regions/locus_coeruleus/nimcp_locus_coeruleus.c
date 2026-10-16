/**
 * @file nimcp_locus_coeruleus.c
 * @brief Locus Coeruleus (LC) Module Implementation
 * @version 1.0.0
 * @date 2026-01-11
 */

#include "core/brain/regions/locus_coeruleus/nimcp_locus_coeruleus.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_learning_constants.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE_MESH_ONLY(locus_coeruleus, MESH_ADAPTER_CATEGORY_COGNITIVE)

static float exponential_decay(float value, float tau, float dt) {
    if (tau <= 0.0f) return value;
    return value * expf(-dt / tau);
}

static float approach_target(float current, float target, float tau, float dt) {
    if (tau <= 0.0f) return target;
    float alpha = 1.0f - expf(-dt / tau);
    return current + alpha * (target - current);
}

static uint32_t compute_input_hash(const float* input, uint32_t size) {
    /* Simple hash for input comparison */
    uint32_t hash = 0;
    for (uint32_t i = 0; i < size && i < 16; i++) {
        union { float f; uint32_t u; } conv;
        conv.f = input[i];
        hash ^= conv.u;
        hash = (hash << 5) | (hash >> 27);
    }
    return hash;
}

//=============================================================================
// Lifecycle Implementation
//=============================================================================

nimcp_lc_config_t nimcp_lc_default_config(void) {
    nimcp_lc_config_t config;
    memset(&config, 0, sizeof(config));

    /* Neuron parameters */
    config.num_neurons = 1000;
    config.tonic_rate_hz = LC_TONIC_BASELINE_HZ;
    config.phasic_rate_hz = LC_PHASIC_MAX_HZ;
    config.refractory_ms = LC_REFRACTORY_MS;

    /* NE parameters */
    config.ne_baseline_nm = LC_NE_BASELINE_NM;
    config.ne_max_nm = LC_NE_MAX_NM;
    config.ne_release_tau_ms = LC_NE_RELEASE_TAU_MS;
    config.ne_clearance_tau_ms = LC_NE_CLEARANCE_TAU_MS;
    config.ne_per_spike = 0.01f;

    /* Mode switching */
    config.mode_switch_threshold = LC_MODE_SWITCH_THRESHOLD;
    config.phasic_duration_ms = 200.0f;
    config.tonic_phasic_balance = 0.5f;

    /* Novelty detection */
    config.novelty_threshold = 0.3f;
    config.surprise_gain = 2.0f;
    config.habituation_rate = 0.01f;

    /* Arousal */
    config.arousal_coupling = 1.0f;
    config.vigilance_decay_rate = 0.001f;

    /* Autoreceptors */
    config.enable_autoreceptors = true;
    config.autoreceptor_gain = 0.5f;

    /* Callbacks */
    config.on_release = NULL;
    config.on_mode_change = NULL;
    config.on_novelty = NULL;
    config.callback_data = NULL;

    config.enable_logging = false;

    return config;
}

nimcp_lc_error_t nimcp_lc_init(nimcp_lc_system_t* lc, const nimcp_lc_config_t* config) {
    if (!lc) {
        return LC_ERR_NULL_PTR;
    }

    if (lc->initialized) {
        return LC_ERR_ALREADY_INITIALIZED;
    }

    memset(lc, 0, sizeof(nimcp_lc_system_t));

    /* Apply configuration */
    if (config) {
        lc->config = *config;
    } else {
        lc->config = nimcp_lc_default_config();
    }

    /* Initialize firing rates */
    lc->tonic_firing_rate = lc->config.tonic_rate_hz;
    lc->phasic_firing_rate = 0.0f;
    lc->ne_concentration = lc->config.ne_baseline_nm;
    lc->ne_release_rate = 0.0f;

    /* Initialize arousal state */
    lc->arousal_level = 0.5f;
    lc->alertness = 0.5f;
    lc->vigilance = 0.5f;

    /* Initialize novelty state */
    lc->novelty_signal = 0.0f;
    lc->surprise_magnitude = 0.0f;
    lc->exploration_drive = 0.5f;

    /* Initialize mode */
    lc->mode = LC_MODE_TONIC;
    lc->phasic_mode = false;
    lc->mode_switch_threshold = lc->config.mode_switch_threshold;
    lc->time_in_current_mode = 0.0f;

    /* Initialize neuron pool */
    lc->neurons.num_neurons = lc->config.num_neurons;
    lc->neurons.membrane_potential = -65.0f;
    lc->neurons.firing_rate = lc->config.tonic_rate_hz;
    lc->neurons.spike_probability = 0.0f;
    lc->neurons.adaptation = 0.0f;
    lc->neurons.fatigue = 0.0f;
    lc->neurons.excitatory_input = 0.0f;
    lc->neurons.inhibitory_input = 0.0f;
    lc->neurons.autoreceptor_feedback = 0.0f;
    lc->neurons.autoreceptors_enabled = lc->config.enable_autoreceptors;
    lc->neurons.refractory_remaining = 0.0f;
    lc->neurons.spikes_this_update = 0;

    /* Initialize history */
    lc->history_index = 0;
    lc->history_count = 0;
    lc->running_mean = 0.0f;
    lc->running_variance = 1.0f;

    /* Initialize state */
    lc->state = LC_STATE_IDLE;
    lc->status = LC_STATUS_NORMAL;
    lc->initialized = true;
    lc->current_time = 0.0f;
    lc->update_count = 0;

    return LC_OK;
}

nimcp_lc_error_t nimcp_lc_shutdown(nimcp_lc_system_t* lc) {
    if (!lc) {
        return LC_ERR_NULL_PTR;
    }

    if (!lc->initialized) {
        return LC_ERR_NOT_INITIALIZED;
    }

    /* Clear all data */
    memset(lc, 0, sizeof(nimcp_lc_system_t));

    return LC_OK;
}

nimcp_lc_error_t nimcp_lc_reset(nimcp_lc_system_t* lc) {
    if (!lc) {
        return LC_ERR_NULL_PTR;
    }

    if (!lc->initialized) {
        return LC_ERR_NOT_INITIALIZED;
    }

    /* Save config */
    nimcp_lc_config_t saved_config = lc->config;

    /* Re-initialize */
    lc->initialized = false;
    return nimcp_lc_init(lc, &saved_config);
}

//=============================================================================
// Projection Implementation
//=============================================================================

nimcp_lc_error_t nimcp_lc_add_projection(
    nimcp_lc_system_t* lc,
    nimcp_lc_target_t target,
    const char* name,
    float strength,
    uint32_t* projection_id
) {
    if (!lc || !projection_id) {
        return LC_ERR_NULL_PTR;
    }

    if (!lc->initialized) {
        return LC_ERR_NOT_INITIALIZED;
    }

    if (lc->num_projections >= LC_MAX_PROJECTIONS) {
        return LC_ERR_CAPACITY_EXCEEDED;
    }

    nimcp_lc_projection_t* proj = &lc->projections[lc->num_projections];
    memset(proj, 0, sizeof(nimcp_lc_projection_t));

    proj->id = lc->num_projections;
    proj->target = target;
    if (name) {
        strncpy(proj->target_name, name, sizeof(proj->target_name) - 1);
    }

    proj->strength = nimcp_clampf(strength, 0.0f, 1.0f);
    proj->ne_sensitivity = 1.0f;
    proj->current_ne = 0.0f;

    /* Default dynamics based on target */
    switch (target) {
        case LC_TARGET_CORTEX:
            proj->conduction_delay_ms = 50.0f;
            proj->release_probability = 0.3f;
            proj->gain_modulation = 1.5f;
            break;
        case LC_TARGET_HIPPOCAMPUS:
            proj->conduction_delay_ms = 40.0f;
            proj->release_probability = 0.4f;
            proj->gain_modulation = 1.3f;
            break;
        case LC_TARGET_AMYGDALA:
            proj->conduction_delay_ms = 30.0f;
            proj->release_probability = 0.5f;
            proj->gain_modulation = 1.8f;
            break;
        default:
            proj->conduction_delay_ms = 50.0f;
            proj->release_probability = 0.3f;
            proj->gain_modulation = 1.0f;
            break;
    }

    proj->uptake_rate = 0.01f;
    proj->noise_suppression = 0.8f;
    proj->active = true;
    proj->time_since_release = 0.0f;

    *projection_id = proj->id;
    lc->num_projections++;

    return LC_OK;
}

nimcp_lc_projection_t* nimcp_lc_get_projection(
    nimcp_lc_system_t* lc,
    uint32_t projection_id
) {
    if (!lc || !lc->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,

                "nimcp_lc_get_projection: invalid parameters");

            return NULL;
    }

    if (projection_id >= lc->num_projections) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "nimcp_lc_get_projection: capacity exceeded");
        return NULL;
    }

    return &lc->projections[projection_id];
}

nimcp_lc_projection_t* nimcp_lc_get_projection_by_target(
    nimcp_lc_system_t* lc,
    nimcp_lc_target_t target
) {
    if (!lc || !lc->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,

                "nimcp_lc_get_projection_by_target: invalid parameters");

            return NULL;
    }

    for (uint32_t i = 0; i < lc->num_projections; i++) {
        if (lc->projections[i].target == target) {
            return &lc->projections[i];
        }
    }

    return NULL;
}

nimcp_lc_error_t nimcp_lc_set_projection_params(
    nimcp_lc_projection_t* projection,
    float sensitivity,
    float delay,
    float uptake_rate
) {
    if (!projection) {
        return LC_ERR_NULL_PTR;
    }

    projection->ne_sensitivity = nimcp_clampf(sensitivity, 0.0f, 5.0f);
    projection->conduction_delay_ms = nimcp_clampf(delay, 0.0f, 500.0f);
    projection->uptake_rate = nimcp_clampf(uptake_rate, 0.0f, 1.0f);

    return LC_OK;
}

//=============================================================================
// Core Update Implementation
//=============================================================================

static void update_neuron_pool(nimcp_lc_system_t* lc, float dt) {
    nimcp_lc_neuron_pool_t* neurons = &lc->neurons;

    /* Update refractory state */
    if (neurons->refractory_remaining > 0.0f) {
        neurons->refractory_remaining -= dt;
        if (neurons->refractory_remaining < 0.0f) {
            neurons->refractory_remaining = 0.0f;
        }
    }

    /* Compute net input */
    float net_input = neurons->excitatory_input - neurons->inhibitory_input;

    /* Apply autoreceptor feedback */
    if (neurons->autoreceptors_enabled) {
        float autoreceptor_inhib = neurons->autoreceptor_feedback * lc->config.autoreceptor_gain;
        net_input -= autoreceptor_inhib;
    }

    /* Apply adaptation */
    net_input -= neurons->adaptation;

    /* Update membrane potential (simplified) */
    float target_vm = -65.0f + net_input * 20.0f;
    float vm_tau = 20.0f;
    float alpha = 1.0f - expf(-dt / vm_tau);
    neurons->membrane_potential += alpha * (target_vm - neurons->membrane_potential);

    /* Compute firing rate based on mode */
    float target_rate;
    if (lc->mode == LC_MODE_PHASIC) {
        target_rate = lc->phasic_firing_rate;
    } else if (lc->mode == LC_MODE_QUIESCENT) {
        target_rate = 0.1f;
    } else {
        target_rate = lc->tonic_firing_rate;
    }

    /* Modulate by input */
    target_rate *= (1.0f + net_input);
    target_rate = nimcp_clampf(target_rate, 0.0f, LC_PHASIC_MAX_HZ);

    /* Smooth firing rate change */
    float rate_tau = 50.0f;
    alpha = 1.0f - expf(-dt / rate_tau);
    neurons->firing_rate += alpha * (target_rate - neurons->firing_rate);

    /* Compute spike probability */
    neurons->spike_probability = neurons->firing_rate * dt / 1000.0f;
    neurons->spike_probability = nimcp_clampf(neurons->spike_probability, 0.0f, 1.0f);

    /* Generate spikes (simplified) */
    neurons->spikes_this_update = (uint32_t)(neurons->spike_probability * neurons->num_neurons);

    /* Update adaptation */
    float adaptation_rate = 0.001f;
    float adaptation_target = neurons->firing_rate * 0.1f;
    neurons->adaptation += adaptation_rate * dt * (adaptation_target - neurons->adaptation);

    /* Update fatigue */
    float fatigue_rate = 0.0001f * neurons->firing_rate;
    float fatigue_recovery = 0.01f;
    neurons->fatigue += dt * (fatigue_rate - fatigue_recovery * neurons->fatigue);
    neurons->fatigue = nimcp_clampf(neurons->fatigue, 0.0f, 1.0f);

    /* Reset inputs for next cycle */
    neurons->excitatory_input *= 0.9f;
    neurons->inhibitory_input *= 0.9f;
}

static void update_ne_dynamics(nimcp_lc_system_t* lc, float dt) {
    /* Compute NE release from firing */
    float spikes_per_ms = lc->neurons.firing_rate / 1000.0f;
    float release_rate = spikes_per_ms * lc->config.ne_per_spike * lc->neurons.num_neurons;

    /* Apply release */
    lc->ne_release_rate = release_rate;
    lc->ne_concentration += release_rate * dt;

    /* Apply clearance */
    float clearance = (fabsf(lc->config.ne_clearance_tau_ms) > 1e-10f) ?
        lc->ne_concentration * (dt / lc->config.ne_clearance_tau_ms) : 0.0f;
    lc->ne_concentration -= clearance;

    /* Clamp to valid range */
    lc->ne_concentration = nimcp_clampf(lc->ne_concentration, 0.0f, lc->config.ne_max_nm);

    /* Update autoreceptor feedback based on NE level */
    float normalized_ne = (fabsf(lc->config.ne_baseline_nm) > 1e-10f) ?
        lc->ne_concentration / lc->config.ne_baseline_nm : 0.0f;
    lc->neurons.autoreceptor_feedback = normalized_ne * 0.5f;

    /* Update metrics */
    lc->metrics.total_ne_released += release_rate * dt;
    if (lc->ne_concentration > lc->metrics.peak_ne_concentration) {
        lc->metrics.peak_ne_concentration = lc->ne_concentration;
    }
}

static void update_projections(nimcp_lc_system_t* lc, float dt) {
    for (uint32_t i = 0; i < lc->num_projections; i++) {
        nimcp_lc_projection_t* proj = &lc->projections[i];
        if (!proj->active) continue;

        proj->time_since_release += dt;

        /* Compute NE at target with conduction delay */
        float release = lc->ne_release_rate * proj->strength * proj->release_probability;

        /* Simple delay model */
        if (proj->time_since_release >= proj->conduction_delay_ms) {
            proj->current_ne += release * dt;
        }

        /* Apply uptake */
        float uptake = proj->current_ne * proj->uptake_rate * dt;
        proj->current_ne -= uptake;
        proj->current_ne = nimcp_clampf(proj->current_ne, 0.0f, LC_NE_MAX_NM);

        /* Invoke callback if configured */
        if (lc->config.on_release && proj->current_ne > 0.1f) {
            lc->config.on_release(lc, proj->target, proj->current_ne,
                                   lc->config.callback_data);
        }
    }
}

static void update_arousal_state(nimcp_lc_system_t* lc, float dt) {
    /* Map NE to arousal (inverted-U relationship) */
    float normalized_ne = (fabsf(lc->config.ne_baseline_nm) > 1e-10f) ?
        lc->ne_concentration / lc->config.ne_baseline_nm : 0.0f;

    /* Simple mapping: optimal around 1.0-2.0x baseline */
    float target_arousal;
    if (normalized_ne < 0.5f) {
        target_arousal = normalized_ne;
    } else if (normalized_ne < 3.0f) {
        target_arousal = 0.5f + (normalized_ne - 0.5f) * 0.2f;
    } else {
        target_arousal = 1.0f - (normalized_ne - 3.0f) * 0.1f;
    }
    target_arousal = nimcp_clampf(target_arousal, 0.0f, 1.0f);

    /* Smooth arousal change */
    float arousal_tau = 500.0f;
    float alpha = 1.0f - expf(-dt / arousal_tau);
    lc->arousal_level += alpha * (target_arousal - lc->arousal_level);

    /* Update alertness (follows arousal more quickly) */
    float alert_tau = 200.0f;
    alpha = 1.0f - expf(-dt / alert_tau);
    float target_alert = lc->arousal_level * (1.0f + lc->novelty_signal);
    target_alert = nimcp_clampf(target_alert, 0.0f, 1.0f);
    lc->alertness += alpha * (target_alert - lc->alertness);

    /* Update vigilance (decays without sustained input) */
    float vig_decay = lc->config.vigilance_decay_rate * dt;
    float vig_boost = lc->arousal_level * 0.001f * dt;
    lc->vigilance += vig_boost - vig_decay;
    lc->vigilance = nimcp_clampf(lc->vigilance, 0.0f, 1.0f);

    /* Update metrics */
    lc->metrics.mean_arousal = (lc->metrics.mean_arousal * lc->update_count + lc->arousal_level) /
                                (lc->update_count + 1);
    lc->metrics.mean_vigilance = (lc->metrics.mean_vigilance * lc->update_count + lc->vigilance) /
                                  (lc->update_count + 1);
}

static void update_mode_state(nimcp_lc_system_t* lc, float dt) {
    lc->time_in_current_mode += dt;

    /* Check for mode transitions */
    nimcp_lc_mode_t new_mode = lc->mode;

    if (lc->mode == LC_MODE_PHASIC) {
        /* Exit phasic if duration exceeded or novelty subsided */
        if (lc->time_in_current_mode > lc->config.phasic_duration_ms ||
            lc->novelty_signal < 0.1f) {
            new_mode = LC_MODE_TONIC;
        }
    } else if (lc->mode == LC_MODE_TONIC) {
        /* Enter phasic on high novelty */
        if (lc->novelty_signal > lc->mode_switch_threshold) {
            new_mode = LC_MODE_PHASIC;
        }
        /* Enter quiescent on very low arousal */
        if (lc->arousal_level < 0.1f) {
            new_mode = LC_MODE_QUIESCENT;
        }
    } else if (lc->mode == LC_MODE_QUIESCENT) {
        /* Exit quiescent on increased arousal */
        if (lc->arousal_level > 0.3f) {
            new_mode = LC_MODE_TONIC;
        }
    }

    /* Apply mode change */
    if (new_mode != lc->mode) {
        nimcp_lc_mode_t old_mode = lc->mode;
        lc->mode = new_mode;
        lc->phasic_mode = (new_mode == LC_MODE_PHASIC);
        lc->time_in_current_mode = 0.0f;

        /* Update phasic firing rate */
        if (new_mode == LC_MODE_PHASIC) {
            lc->phasic_firing_rate = lc->config.phasic_rate_hz *
                                      (0.5f + 0.5f * lc->novelty_signal);
        } else {
            lc->phasic_firing_rate = 0.0f;
        }

        lc->metrics.mode_transitions++;

        if (lc->config.on_mode_change) {
            lc->config.on_mode_change(lc, old_mode, new_mode,
                                       lc->config.callback_data);
        }
    }

    /* Update mode time metrics */
    if (lc->mode == LC_MODE_TONIC) {
        lc->metrics.time_in_tonic += dt / 1000.0f;
    } else if (lc->mode == LC_MODE_PHASIC) {
        lc->metrics.time_in_phasic += dt / 1000.0f;
    }
}

static void update_status(nimcp_lc_system_t* lc) {
    nimcp_lc_status_t new_status = LC_STATUS_NORMAL;

    float normalized_ne = (fabsf(lc->config.ne_baseline_nm) > 1e-10f) ?
        lc->ne_concentration / lc->config.ne_baseline_nm : 0.0f;

    if (normalized_ne > 5.0f) {
        new_status = LC_STATUS_STRESSED;
    } else if (normalized_ne > 3.0f) {
        new_status = LC_STATUS_HYPERACTIVE;
    } else if (normalized_ne < 0.2f) {
        new_status = LC_STATUS_DEPLETED;
    } else if (normalized_ne < 0.5f) {
        new_status = LC_STATUS_HYPOACTIVE;
    }

    lc->status = new_status;
}

nimcp_lc_error_t nimcp_lc_update(nimcp_lc_system_t* lc, float dt) {
    if (!lc) {
        return LC_ERR_NULL_PTR;
    }

    if (!lc->initialized) {
        return LC_ERR_NOT_INITIALIZED;
    }

    if (dt <= 0.0f) {
        return LC_ERR_INVALID_PARAM;
    }

    /* Update subsystems */
    update_neuron_pool(lc, dt);
    update_ne_dynamics(lc, dt);
    update_projections(lc, dt);
    update_arousal_state(lc, dt);
    update_mode_state(lc, dt);
    update_status(lc);

    /* Decay novelty signal */
    lc->novelty_signal = exponential_decay(lc->novelty_signal, 200.0f, dt);
    lc->surprise_magnitude = exponential_decay(lc->surprise_magnitude, 100.0f, dt);

    /* Update time and counts */
    lc->current_time += dt;
    lc->update_count++;

    /* Update metrics */
    lc->metrics.total_simulation_time = lc->current_time / 1000.0f;
    lc->metrics.update_count = lc->update_count;
    lc->metrics.total_spikes += lc->neurons.spikes_this_update;
    lc->metrics.mean_firing_rate = (lc->metrics.mean_firing_rate * (lc->update_count - 1) +
                                     lc->neurons.firing_rate) / lc->update_count;
    if (lc->neurons.firing_rate > lc->metrics.peak_firing_rate) {
        lc->metrics.peak_firing_rate = lc->neurons.firing_rate;
    }
    lc->metrics.mean_ne_concentration = (lc->metrics.mean_ne_concentration * (lc->update_count - 1) +
                                          lc->ne_concentration) / lc->update_count;

    return LC_OK;
}

//=============================================================================
// Novelty Detection Implementation
//=============================================================================

nimcp_lc_error_t nimcp_lc_detect_novelty(
    nimcp_lc_system_t* lc,
    const float* sensory_input,
    uint32_t input_size,
    float* novelty_score
) {
    if (!lc || !novelty_score) {
        return LC_ERR_NULL_PTR;
    }

    if (!lc->initialized) {
        return LC_ERR_NOT_INITIALIZED;
    }

    if (!sensory_input || input_size == 0) {
        *novelty_score = 0.0f;
        return LC_OK;
    }

    /* Compute input magnitude */
    float input_magnitude = 0.0f;
    for (uint32_t i = 0; i < input_size; i++) {
        input_magnitude += sensory_input[i] * sensory_input[i];
    }
    input_magnitude = sqrtf(input_magnitude / input_size);

    /* Update running statistics */
    float old_mean = lc->running_mean;
    float old_var = lc->running_variance;

    float learning_rate = NIMCP_LEARNING_RATE_COARSE;
    lc->running_mean += learning_rate * (input_magnitude - lc->running_mean);

    float diff = input_magnitude - old_mean;
    lc->running_variance += learning_rate * (diff * diff - lc->running_variance);
    if (lc->running_variance < 0.01f) {
        lc->running_variance = 0.01f;
    }

    /* Compute z-score (statistical novelty) */
    float z_score = fabsf(input_magnitude - lc->running_mean) / sqrtf(lc->running_variance);

    /* Convert to 0-1 novelty score */
    float novelty = 1.0f - expf(-z_score * z_score / 4.0f);

    /* Apply surprise gain */
    novelty *= lc->config.surprise_gain;
    novelty = nimcp_clampf(novelty, 0.0f, 1.0f);

    /* Store in history */
    lc->input_history[lc->history_index] = input_magnitude;
    lc->history_index = (lc->history_index + 1) % LC_HISTORY_SIZE;
    if (lc->history_count < LC_HISTORY_SIZE) {
        lc->history_count++;
    }

    /* Update system state */
    if (novelty > lc->novelty_signal) {
        lc->novelty_signal = novelty;
    }
    lc->surprise_magnitude = novelty;

    /* Update exploration drive */
    lc->exploration_drive = 0.3f + 0.7f * (1.0f - novelty);

    /* Update metrics */
    lc->metrics.mean_novelty = (lc->metrics.mean_novelty * lc->metrics.novelty_events +
                                 novelty) / (lc->metrics.novelty_events + 1);
    if (novelty > lc->config.novelty_threshold) {
        lc->metrics.novelty_events++;
    }

    /* Invoke callback */
    if (lc->config.on_novelty && novelty > lc->config.novelty_threshold) {
        lc->config.on_novelty(lc, novelty, sensory_input, input_size,
                               lc->config.callback_data);
    }

    *novelty_score = novelty;
    return LC_OK;
}

//=============================================================================
// Arousal Modulation Implementation
//=============================================================================

nimcp_lc_error_t nimcp_lc_modulate_arousal(nimcp_lc_system_t* lc, float target_arousal) {
    if (!lc) {
        return LC_ERR_NULL_PTR;
    }

    if (!lc->initialized) {
        return LC_ERR_NOT_INITIALIZED;
    }

    target_arousal = nimcp_clampf(target_arousal, 0.0f, 1.0f);

    /* Convert target arousal to NE level */
    /* Higher arousal requires more NE */
    float target_ne = lc->config.ne_baseline_nm * (0.5f + 1.5f * target_arousal);

    /* Adjust tonic rate to achieve target NE */
    float ne_ratio = (fabsf(lc->config.ne_baseline_nm) > 1e-10f) ?
        target_ne / lc->config.ne_baseline_nm : 1.0f;
    lc->tonic_firing_rate = lc->config.tonic_rate_hz * ne_ratio;
    lc->tonic_firing_rate = nimcp_clampf(lc->tonic_firing_rate, 0.1f, lc->config.phasic_rate_hz * 0.5f);

    return LC_OK;
}

nimcp_lc_error_t nimcp_lc_trigger_attention_reset(nimcp_lc_system_t* lc) {
    if (!lc) {
        return LC_ERR_NULL_PTR;
    }

    if (!lc->initialized) {
        return LC_ERR_NOT_INITIALIZED;
    }

    /* Trigger phasic burst */
    lc->novelty_signal = 1.0f;
    lc->surprise_magnitude = 0.8f;

    /* Force mode switch to phasic */
    if (lc->mode != LC_MODE_PHASIC) {
        nimcp_lc_mode_t old_mode = lc->mode;
        lc->mode = LC_MODE_PHASIC;
        lc->phasic_mode = true;
        lc->phasic_firing_rate = lc->config.phasic_rate_hz;
        lc->time_in_current_mode = 0.0f;

        lc->metrics.total_bursts++;

        if (lc->config.on_mode_change) {
            lc->config.on_mode_change(lc, old_mode, LC_MODE_PHASIC,
                                       lc->config.callback_data);
        }
    }

    return LC_OK;
}

//=============================================================================
// Mode Control Implementation
//=============================================================================

nimcp_lc_error_t nimcp_lc_set_mode(nimcp_lc_system_t* lc, nimcp_lc_mode_t mode) {
    if (!lc) {
        return LC_ERR_NULL_PTR;
    }

    if (!lc->initialized) {
        return LC_ERR_NOT_INITIALIZED;
    }

    if (mode >= LC_MODE_COUNT) {
        return LC_ERR_INVALID_PARAM;
    }

    if (mode != lc->mode) {
        nimcp_lc_mode_t old_mode = lc->mode;
        lc->mode = mode;
        lc->phasic_mode = (mode == LC_MODE_PHASIC);
        lc->time_in_current_mode = 0.0f;

        if (mode == LC_MODE_PHASIC) {
            lc->phasic_firing_rate = lc->config.phasic_rate_hz;
            lc->metrics.total_bursts++;
        } else {
            lc->phasic_firing_rate = 0.0f;
        }

        lc->metrics.mode_transitions++;

        if (lc->config.on_mode_change) {
            lc->config.on_mode_change(lc, old_mode, mode, lc->config.callback_data);
        }
    }

    return LC_OK;
}

nimcp_lc_mode_t nimcp_lc_get_mode(const nimcp_lc_system_t* lc) {
    if (!lc || !lc->initialized) {
        return LC_MODE_TONIC;
    }
    return lc->mode;
}

nimcp_lc_error_t nimcp_lc_trigger_burst(
    nimcp_lc_system_t* lc,
    float intensity,
    float duration
) {
    if (!lc) {
        return LC_ERR_NULL_PTR;
    }

    if (!lc->initialized) {
        return LC_ERR_NOT_INITIALIZED;
    }

    intensity = nimcp_clampf(intensity, 0.0f, 1.0f);
    duration = nimcp_clampf(duration, 10.0f, 1000.0f);

    /* Set phasic mode with specified intensity */
    lc->phasic_firing_rate = lc->config.phasic_rate_hz * intensity;

    if (lc->mode != LC_MODE_PHASIC) {
        nimcp_lc_mode_t old_mode = lc->mode;
        lc->mode = LC_MODE_PHASIC;
        lc->phasic_mode = true;
        lc->time_in_current_mode = 0.0f;

        /* Temporarily override phasic duration */
        /* Note: This is simplified; real implementation might use a separate timer */

        lc->metrics.total_bursts++;
        lc->metrics.mode_transitions++;

        if (lc->config.on_mode_change) {
            lc->config.on_mode_change(lc, old_mode, LC_MODE_PHASIC,
                                       lc->config.callback_data);
        }
    }

    return LC_OK;
}

//=============================================================================
// Input API Implementation
//=============================================================================

nimcp_lc_error_t nimcp_lc_apply_excitation(nimcp_lc_system_t* lc, float input) {
    if (!lc) {
        return LC_ERR_NULL_PTR;
    }

    if (!lc->initialized) {
        return LC_ERR_NOT_INITIALIZED;
    }

    input = nimcp_clampf(input, 0.0f, 1.0f);
    lc->neurons.excitatory_input += input;

    return LC_OK;
}

nimcp_lc_error_t nimcp_lc_apply_inhibition(nimcp_lc_system_t* lc, float input) {
    if (!lc) {
        return LC_ERR_NULL_PTR;
    }

    if (!lc->initialized) {
        return LC_ERR_NOT_INITIALIZED;
    }

    input = nimcp_clampf(input, 0.0f, 1.0f);
    lc->neurons.inhibitory_input += input;

    return LC_OK;
}

nimcp_lc_error_t nimcp_lc_signal_stress(nimcp_lc_system_t* lc, float stress_level) {
    if (!lc) {
        return LC_ERR_NULL_PTR;
    }

    if (!lc->initialized) {
        return LC_ERR_NOT_INITIALIZED;
    }

    stress_level = nimcp_clampf(stress_level, 0.0f, 1.0f);

    /* Stress increases excitation */
    lc->neurons.excitatory_input += stress_level * 0.5f;

    /* High stress can trigger attention reset */
    if (stress_level > 0.7f) {
        nimcp_lc_trigger_attention_reset(lc);
    }

    /* Update status */
    if (stress_level > 0.8f) {
        lc->status = LC_STATUS_STRESSED;
    }

    return LC_OK;
}

//=============================================================================
// Output API Implementation
//=============================================================================

nimcp_lc_error_t nimcp_lc_get_ne_at_target(
    const nimcp_lc_system_t* lc,
    nimcp_lc_target_t target,
    float* ne_concentration
) {
    if (!lc || !ne_concentration) {
        return LC_ERR_NULL_PTR;
    }

    if (!lc->initialized) {
        return LC_ERR_NOT_INITIALIZED;
    }

    /* Find projection to target */
    for (uint32_t i = 0; i < lc->num_projections; i++) {
        if (lc->projections[i].target == target && lc->projections[i].active) {
            *ne_concentration = lc->projections[i].current_ne;
            return LC_OK;
        }
    }

    /* No projection found, return scaled global NE */
    *ne_concentration = lc->ne_concentration * 0.1f;
    return LC_OK;
}

nimcp_lc_error_t nimcp_lc_get_gain_modulation(
    const nimcp_lc_system_t* lc,
    nimcp_lc_target_t target,
    float* gain
) {
    if (!lc || !gain) {
        return LC_ERR_NULL_PTR;
    }

    if (!lc->initialized) {
        return LC_ERR_NOT_INITIALIZED;
    }

    /* Find projection to target */
    for (uint32_t i = 0; i < lc->num_projections; i++) {
        if (lc->projections[i].target == target && lc->projections[i].active) {
            /* Compute gain based on NE level at target */
            float ne = lc->projections[i].current_ne;
            float normalized_ne = (fabsf(lc->config.ne_baseline_nm) > 1e-10f) ?
                ne / lc->config.ne_baseline_nm : 0.0f;

            /* Inverted-U gain modulation */
            float optimal = 2.0f;
            float gain_val = 1.0f + 0.5f * (1.0f - fabsf(normalized_ne - optimal) / optimal);
            gain_val *= lc->projections[i].gain_modulation;
            gain_val = nimcp_clampf(gain_val, 0.5f, 2.5f);

            *gain = gain_val;
            return LC_OK;
        }
    }

    /* Default gain */
    *gain = 1.0f;
    return LC_OK;
}

nimcp_lc_error_t nimcp_lc_get_firing_rate(const nimcp_lc_system_t* lc, float* rate_hz) {
    if (!lc || !rate_hz) {
        return LC_ERR_NULL_PTR;
    }

    if (!lc->initialized) {
        return LC_ERR_NOT_INITIALIZED;
    }

    *rate_hz = lc->neurons.firing_rate;
    return LC_OK;
}

//=============================================================================
// State/Metrics API Implementation
//=============================================================================

nimcp_lc_state_t nimcp_lc_get_state(const nimcp_lc_system_t* lc) {
    if (!lc || !lc->initialized) {
        return LC_STATE_IDLE;
    }
    return lc->state;
}

nimcp_lc_status_t nimcp_lc_get_status(const nimcp_lc_system_t* lc) {
    if (!lc || !lc->initialized) {
        return LC_STATUS_NORMAL;
    }
    return lc->status;
}

nimcp_lc_error_t nimcp_lc_get_metrics(
    const nimcp_lc_system_t* lc,
    nimcp_lc_metrics_t* metrics
) {
    if (!lc || !metrics) {
        return LC_ERR_NULL_PTR;
    }

    if (!lc->initialized) {
        return LC_ERR_NOT_INITIALIZED;
    }

    *metrics = lc->metrics;
    return LC_OK;
}

nimcp_lc_error_t nimcp_lc_reset_metrics(nimcp_lc_system_t* lc) {
    if (!lc) {
        return LC_ERR_NULL_PTR;
    }

    if (!lc->initialized) {
        return LC_ERR_NOT_INITIALIZED;
    }

    memset(&lc->metrics, 0, sizeof(nimcp_lc_metrics_t));
    return LC_OK;
}

const char* nimcp_lc_error_string(nimcp_lc_error_t error) {
    switch (error) {
        case LC_OK: return "OK";
        case LC_ERR_NULL_PTR: return "Null pointer";
        case LC_ERR_INVALID_PARAM: return "Invalid parameter";
        case LC_ERR_NOT_INITIALIZED: return "Not initialized";
        case LC_ERR_ALREADY_INITIALIZED: return "Already initialized";
        case LC_ERR_NO_MEMORY: return "No memory";
        case LC_ERR_PROJECTION_NOT_FOUND: return "Projection not found";
        case LC_ERR_CAPACITY_EXCEEDED: return "Capacity exceeded";
        case LC_ERR_INVALID_STATE: return "Invalid state";
        case LC_ERR_REFRACTORY: return "In refractory period";
        default: return "Unknown error";
    }
}
