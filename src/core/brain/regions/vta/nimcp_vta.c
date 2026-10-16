/**
 * @file nimcp_vta.c
 * @brief Ventral Tegmental Area (VTA) implementation
 * @date 2026-01-11
 */

#include "core/brain/regions/vta/nimcp_vta.h"
#include "core/brain/regions/vta/nimcp_dopamine_release.h"
#include "core/brain/regions/vta/nimcp_reward_prediction_error.h"
#include "core/brain/regions/vta/nimcp_incentive_salience.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <stdio.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_threshold_constants.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE_MESH_ONLY(vta, MESH_ADAPTER_CATEGORY_COGNITIVE)

static void update_neuron_pool(nimcp_vta_system_t* vta, float dt) {
    nimcp_vta_neuron_pool_t* neurons = &vta->neurons;

    /* Decay refractory */
    if (neurons->refractory_time > 0.0f) {
        neurons->refractory_time -= dt;
        if (neurons->refractory_time < 0.0f) {
            neurons->refractory_time = 0.0f;
        }
    }

    /* Compute net input */
    float net_input = neurons->excitatory_input - neurons->inhibitory_input;

    /* Update membrane potential */
    float tau = 20.0f;  /* ms */
    float rest = -65.0f; /* mV */
    float dv = (rest - neurons->membrane_potential + net_input * 10.0f) / tau * dt;
    neurons->membrane_potential += dv;

    /* Check for spiking */
    float threshold = -55.0f;
    if (neurons->membrane_potential >= threshold && neurons->refractory_time <= 0.0f) {
        neurons->spike_count++;
        neurons->membrane_potential = rest - 5.0f;  /* Reset with undershoot */
        neurons->refractory_time = 2.0f;  /* 2ms refractory */

        vta->metrics.total_spikes++;

        if (neurons->in_burst) {
            neurons->burst_spikes++;
        }
    }

    /* Compute instantaneous firing rate */
    float rate_tau = 100.0f;  /* ms */
    float target_rate;
    switch (vta->mode) {
        case VTA_MODE_PHASIC_EXCITATION:
            target_rate = VTA_PHASIC_MAX_RATE;
            break;
        case VTA_MODE_PHASIC_PAUSE:
            target_rate = 0.1f;
            break;
        case VTA_MODE_QUIESCENT:
            target_rate = 0.5f;
            break;
        default:
            target_rate = vta->tonic_firing_rate;
    }

    /* Add input modulation */
    target_rate *= (1.0f + net_input);
    target_rate = nimcp_clampf(target_rate, 0.0f, VTA_PHASIC_MAX_RATE);

    /* Smooth rate change */
    float rate_change = (target_rate - neurons->firing_rate) / rate_tau * dt;
    neurons->firing_rate += rate_change;
    neurons->firing_rate = nimcp_clampf(neurons->firing_rate, 0.0f, VTA_PHASIC_MAX_RATE);

    /* Decay inputs */
    float input_decay = expf(-dt / 50.0f);
    neurons->excitatory_input *= input_decay;
    neurons->inhibitory_input *= input_decay;
}

static void update_da_dynamics(nimcp_vta_system_t* vta, float dt) {
    /* DA release based on firing rate */
    float release_rate = vta->neurons.firing_rate * 0.5f;  /* nM per Hz-ms */

    /* Enhanced release during bursts */
    if (vta->mode == VTA_MODE_PHASIC_EXCITATION) {
        release_rate *= 2.0f;
    }

    vta->da_release_rate = release_rate;

    /* Update DA concentration */
    float da_release = release_rate * dt;
    float da_decay = vta->da_concentration * (dt / vta->config.da_decay_tau);

    vta->da_concentration += da_release - da_decay;
    vta->da_concentration = nimcp_clampf(vta->da_concentration, 0.0f, 1000.0f);

    vta->metrics.total_da_released += da_release;
}

static void update_projections(nimcp_vta_system_t* vta, float dt) {
    for (uint32_t i = 0; i < vta->num_projections; i++) {
        nimcp_vta_projection_t* proj = &vta->projections[i];
        if (!proj->enabled) continue;

        /* DA delivered based on VTA concentration and weight */
        float delivery_rate = vta->da_concentration * proj->weight * 0.01f;
        float decay_rate = proj->da_delivered * (dt / 200.0f);

        proj->da_delivered += delivery_rate * dt - decay_rate;
        proj->da_delivered = nimcp_clampf(proj->da_delivered, 0.0f, 500.0f);

        /* Update receptor activation (simplified Hill equation) */
        float da = proj->da_delivered;
        float kd_d1 = 30.0f;   /* nM - D1 lower affinity */
        float kd_d2 = 10.0f;   /* nM - D2 higher affinity */

        proj->d1_activation = da / (da + kd_d1);
        proj->d2_activation = da / (da + kd_d2);
    }
}

static void update_mode(nimcp_vta_system_t* vta, float dt) {
    vta->mode_duration += dt;

    /* Handle phasic state timeouts */
    if (vta->mode == VTA_MODE_PHASIC_EXCITATION || vta->mode == VTA_MODE_PHASIC_PAUSE) {
        vta->phasic_burst_remaining -= dt;
        if (vta->phasic_burst_remaining <= 0.0f) {
            vta->mode = VTA_MODE_TONIC;
            vta->mode_duration = 0.0f;
            vta->phasic_burst_remaining = 0.0f;
            vta->neurons.in_burst = false;
        }
    }

    /* Check for mode transition based on RPE */
    float rpe = vta->reward.prediction_error;

    if (vta->mode == VTA_MODE_TONIC) {
        if (rpe > vta->config.burst_threshold) {
            vta->mode = VTA_MODE_PHASIC_EXCITATION;
            vta->mode_duration = 0.0f;
            vta->phasic_burst_remaining = 200.0f;  /* 200ms burst */
            vta->neurons.in_burst = true;
            vta->neurons.burst_spikes = 0;
            vta->metrics.burst_count++;
        } else if (rpe < vta->config.pause_threshold) {
            vta->mode = VTA_MODE_PHASIC_PAUSE;
            vta->mode_duration = 0.0f;
            vta->phasic_burst_remaining = 300.0f;  /* 300ms pause */
            vta->metrics.pause_count++;
        }
    }

    /* Update status */
    if (vta->da_concentration < VTA_DEFAULT_DA_BASELINE * 0.5f) {
        vta->status = VTA_STATUS_HYPODOPAMINERGIC;
    } else if (vta->da_concentration > VTA_DEFAULT_DA_BASELINE * 2.0f) {
        vta->status = VTA_STATUS_HYPERDOPAMINERGIC;
    } else {
        vta->status = VTA_STATUS_NORMAL;
    }
}

static void update_motivation(nimcp_vta_system_t* vta, float dt) {
    nimcp_motivation_state_t* mot = &vta->motivation;

    /* Wanting driven by DA relative to baseline */
    float da_ratio = vta->da_concentration / VTA_DEFAULT_DA_BASELINE;
    float wanting_target = nimcp_clampf(da_ratio - 0.5f, 0.0f, 1.0f);

    /* Smooth update */
    float tau = 100.0f;
    mot->wanting += (wanting_target - mot->wanting) * (dt / tau);
    mot->wanting = nimcp_clampf(mot->wanting, 0.0f, 1.0f);

    /* Effort willingness scales with DA */
    mot->effort_willingness = nimcp_clampf(da_ratio * 0.5f, 0.0f, 1.0f);
}

/*=============================================================================
 * Lifecycle API
 *===========================================================================*/

nimcp_vta_config_t nimcp_vta_default_config(void) {
    nimcp_vta_config_t config;
    memset(&config, 0, sizeof(config));

    config.baseline_firing_rate = VTA_DEFAULT_TONIC_RATE;
    config.baseline_da = VTA_DEFAULT_DA_BASELINE;
    config.learning_rate = VTA_DEFAULT_LEARNING_RATE;
    config.discount_factor = VTA_DEFAULT_DISCOUNT;
    config.rpe_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    config.da_decay_tau = 200.0f;
    config.burst_threshold = 0.3f;
    config.pause_threshold = -0.3f;
    config.enable_autoreceptors = true;

    return config;
}

nimcp_vta_error_t nimcp_vta_init(nimcp_vta_system_t* vta, const nimcp_vta_config_t* config) {
    if (!vta) {
        return VTA_ERROR_NULL;
    }

    if (vta->initialized) {
        return VTA_ERROR_ALREADY_INITIALIZED;
    }

    memset(vta, 0, sizeof(*vta));

    /* Apply configuration */
    if (config) {
        vta->config = *config;
    } else {
        vta->config = nimcp_vta_default_config();
    }

    /* Initialize neuron pool */
    vta->neurons.membrane_potential = -65.0f;
    vta->neurons.firing_rate = vta->config.baseline_firing_rate;

    /* Initialize DA state */
    vta->da_concentration = vta->config.baseline_da;
    vta->tonic_firing_rate = vta->config.baseline_firing_rate;

    /* Initialize mode */
    vta->mode = VTA_MODE_TONIC;

    /* Initialize motivation */
    vta->motivation.wanting = 0.5f;
    vta->motivation.liking = 0.5f;
    vta->motivation.effort_willingness = 0.5f;

    vta->status = VTA_STATUS_NORMAL;
    vta->initialized = true;

    return VTA_OK;
}

nimcp_vta_error_t nimcp_vta_shutdown(nimcp_vta_system_t* vta) {
    if (!vta) {
        return VTA_ERROR_NULL;
    }

    memset(vta, 0, sizeof(*vta));
    return VTA_OK;
}

nimcp_vta_error_t nimcp_vta_reset(nimcp_vta_system_t* vta) {
    if (!vta) {
        return VTA_ERROR_NULL;
    }

    nimcp_vta_config_t config = vta->config;
    nimcp_vta_shutdown(vta);
    return nimcp_vta_init(vta, &config);
}

/*=============================================================================
 * Update API
 *===========================================================================*/

nimcp_vta_error_t nimcp_vta_update(nimcp_vta_system_t* vta, float dt) {
    if (!vta) {
        return VTA_ERROR_NULL;
    }

    if (!vta->initialized) {
        return VTA_ERROR_NOT_INITIALIZED;
    }

    if (dt <= 0.0f || dt > 1000.0f) {
        return VTA_ERROR_INVALID_PARAM;
    }

    /* Update components */
    update_neuron_pool(vta, dt);
    update_da_dynamics(vta, dt);
    update_projections(vta, dt);
    update_mode(vta, dt);
    update_motivation(vta, dt);

    /* Update timing and metrics */
    vta->simulation_time += dt;
    vta->metrics.update_count++;
    vta->metrics.total_simulation_time += dt;

    /* Decay RPE */
    float rpe_decay = expf(-dt / 100.0f);
    vta->reward.prediction_error *= rpe_decay;

    return VTA_OK;
}

/*=============================================================================
 * Reward Processing API
 *===========================================================================*/

nimcp_vta_error_t nimcp_vta_signal_reward(nimcp_vta_system_t* vta, float reward_magnitude) {
    if (!vta || !vta->initialized) {
        return vta ? VTA_ERROR_NOT_INITIALIZED : VTA_ERROR_NULL;
    }

    nimcp_reward_state_t* r = &vta->reward;

    r->actual_reward = reward_magnitude;
    r->prediction_error = reward_magnitude - r->expected_reward;

    /* Update statistics */
    r->cumulative_reward += reward_magnitude;
    r->reward_count++;
    r->average_reward = r->cumulative_reward / (float)r->reward_count;
    r->last_reward_time = vta->simulation_time;

    /* Update metrics */
    vta->metrics.reward_events++;
    if (r->prediction_error > 0) {
        vta->metrics.positive_rpe_sum += r->prediction_error;
    } else {
        vta->metrics.negative_rpe_sum += r->prediction_error;
    }

    /* Apply RPE effect on DA */
    if (r->prediction_error > vta->config.burst_threshold) {
        nimcp_vta_trigger_burst(vta, fabsf(r->prediction_error), 200.0f);
    } else if (r->prediction_error < vta->config.pause_threshold) {
        nimcp_vta_trigger_pause(vta, fabsf(r->prediction_error), 300.0f);
    }

    return VTA_OK;
}

nimcp_vta_error_t nimcp_vta_set_expectation(nimcp_vta_system_t* vta, float expected_value) {
    if (!vta || !vta->initialized) {
        return vta ? VTA_ERROR_NOT_INITIALIZED : VTA_ERROR_NULL;
    }

    vta->reward.expected_reward = expected_value;
    return VTA_OK;
}

nimcp_vta_error_t nimcp_vta_get_rpe(nimcp_vta_system_t* vta, float* rpe) {
    if (!vta || !rpe) {
        return VTA_ERROR_NULL;
    }

    if (!vta->initialized) {
        return VTA_ERROR_NOT_INITIALIZED;
    }

    *rpe = vta->reward.prediction_error;
    return VTA_OK;
}

nimcp_vta_error_t nimcp_vta_compute_rpe(
    nimcp_vta_system_t* vta,
    float actual_reward,
    float* rpe
) {
    if (!vta || !rpe) {
        return VTA_ERROR_NULL;
    }

    if (!vta->initialized) {
        return VTA_ERROR_NOT_INITIALIZED;
    }

    *rpe = actual_reward - vta->reward.expected_reward;
    return VTA_OK;
}

/*=============================================================================
 * Motivation API
 *===========================================================================*/

nimcp_vta_error_t nimcp_vta_modulate_motivation(
    nimcp_vta_system_t* vta,
    float goal_value,
    float* motivation
) {
    if (!vta || !motivation) {
        return VTA_ERROR_NULL;
    }

    if (!vta->initialized) {
        return VTA_ERROR_NOT_INITIALIZED;
    }

    /* Motivation = DA-driven wanting * goal value */
    float base_motivation = vta->motivation.wanting;
    *motivation = base_motivation * nimcp_clampf(goal_value, 0.0f, 2.0f);
    *motivation = nimcp_clampf(*motivation, 0.0f, 1.0f);

    return VTA_OK;
}

nimcp_vta_error_t nimcp_vta_get_wanting(nimcp_vta_system_t* vta, float* wanting) {
    if (!vta || !wanting) {
        return VTA_ERROR_NULL;
    }

    if (!vta->initialized) {
        return VTA_ERROR_NOT_INITIALIZED;
    }

    *wanting = vta->motivation.wanting;
    return VTA_OK;
}

nimcp_vta_error_t nimcp_vta_get_liking(nimcp_vta_system_t* vta, float* liking) {
    if (!vta || !liking) {
        return VTA_ERROR_NULL;
    }

    if (!vta->initialized) {
        return VTA_ERROR_NOT_INITIALIZED;
    }

    *liking = vta->motivation.liking;
    return VTA_OK;
}

nimcp_vta_error_t nimcp_vta_compute_effort_utility(
    nimcp_vta_system_t* vta,
    float reward_value,
    float effort_required,
    float* net_utility
) {
    if (!vta || !net_utility) {
        return VTA_ERROR_NULL;
    }

    if (!vta->initialized) {
        return VTA_ERROR_NOT_INITIALIZED;
    }

    /* Utility = reward - effort_cost * (1 - effort_willingness) */
    float effort_cost = effort_required * (1.0f - vta->motivation.effort_willingness);
    *net_utility = reward_value - effort_cost;

    return VTA_OK;
}

/*=============================================================================
 * DA Control API
 *===========================================================================*/

nimcp_vta_error_t nimcp_vta_apply_excitation(nimcp_vta_system_t* vta, float strength) {
    if (!vta || !vta->initialized) {
        return vta ? VTA_ERROR_NOT_INITIALIZED : VTA_ERROR_NULL;
    }

    vta->neurons.excitatory_input += nimcp_clampf(strength, 0.0f, 2.0f);
    return VTA_OK;
}

nimcp_vta_error_t nimcp_vta_apply_inhibition(nimcp_vta_system_t* vta, float strength) {
    if (!vta || !vta->initialized) {
        return vta ? VTA_ERROR_NOT_INITIALIZED : VTA_ERROR_NULL;
    }

    vta->neurons.inhibitory_input += nimcp_clampf(strength, 0.0f, 2.0f);
    return VTA_OK;
}

nimcp_vta_error_t nimcp_vta_trigger_burst(
    nimcp_vta_system_t* vta,
    float intensity,
    float duration_ms
) {
    if (!vta || !vta->initialized) {
        return vta ? VTA_ERROR_NOT_INITIALIZED : VTA_ERROR_NULL;
    }

    vta->mode = VTA_MODE_PHASIC_EXCITATION;
    vta->mode_duration = 0.0f;
    vta->phasic_burst_remaining = duration_ms;
    vta->neurons.in_burst = true;
    vta->neurons.burst_spikes = 0;

    /* Boost excitation */
    vta->neurons.excitatory_input += intensity;

    vta->metrics.burst_count++;
    return VTA_OK;
}

nimcp_vta_error_t nimcp_vta_trigger_pause(
    nimcp_vta_system_t* vta,
    float depth,
    float duration_ms
) {
    if (!vta || !vta->initialized) {
        return vta ? VTA_ERROR_NOT_INITIALIZED : VTA_ERROR_NULL;
    }

    vta->mode = VTA_MODE_PHASIC_PAUSE;
    vta->mode_duration = 0.0f;
    vta->phasic_burst_remaining = duration_ms;

    /* Boost inhibition */
    vta->neurons.inhibitory_input += depth;

    vta->metrics.pause_count++;
    return VTA_OK;
}

nimcp_vta_error_t nimcp_vta_get_da(nimcp_vta_system_t* vta, float* da) {
    if (!vta || !da) {
        return VTA_ERROR_NULL;
    }

    if (!vta->initialized) {
        return VTA_ERROR_NOT_INITIALIZED;
    }

    *da = vta->da_concentration;
    return VTA_OK;
}

nimcp_vta_error_t nimcp_vta_get_firing_rate(nimcp_vta_system_t* vta, float* rate) {
    if (!vta || !rate) {
        return VTA_ERROR_NULL;
    }

    if (!vta->initialized) {
        return VTA_ERROR_NOT_INITIALIZED;
    }

    *rate = vta->neurons.firing_rate;
    return VTA_OK;
}

/*=============================================================================
 * Mode API
 *===========================================================================*/

nimcp_vta_error_t nimcp_vta_get_mode(nimcp_vta_system_t* vta, nimcp_vta_mode_t* mode) {
    if (!vta || !mode) {
        return VTA_ERROR_NULL;
    }

    if (!vta->initialized) {
        return VTA_ERROR_NOT_INITIALIZED;
    }

    *mode = vta->mode;
    return VTA_OK;
}

nimcp_vta_error_t nimcp_vta_set_mode(nimcp_vta_system_t* vta, nimcp_vta_mode_t mode) {
    if (!vta || !vta->initialized) {
        return vta ? VTA_ERROR_NOT_INITIALIZED : VTA_ERROR_NULL;
    }

    vta->mode = mode;
    vta->mode_duration = 0.0f;
    return VTA_OK;
}

/*=============================================================================
 * Projection API
 *===========================================================================*/

nimcp_vta_error_t nimcp_vta_add_projection(
    nimcp_vta_system_t* vta,
    nimcp_vta_target_t target,
    const char* name,
    float weight,
    uint32_t* id
) {
    if (!vta || !id) {
        return VTA_ERROR_NULL;
    }

    if (!vta->initialized) {
        return VTA_ERROR_NOT_INITIALIZED;
    }

    if (vta->num_projections >= VTA_MAX_PROJECTIONS) {
        return VTA_ERROR_CAPACITY;
    }

    nimcp_vta_projection_t* proj = &vta->projections[vta->num_projections];
    proj->id = vta->num_projections;
    proj->target = target;
    proj->weight = nimcp_clampf(weight, 0.0f, 1.0f);
    proj->enabled = true;

    if (name) {
        strncpy(proj->name, name, VTA_MAX_NAME_LEN - 1);
        proj->name[VTA_MAX_NAME_LEN - 1] = '\0';
    }

    *id = proj->id;
    vta->num_projections++;

    return VTA_OK;
}

nimcp_vta_projection_t* nimcp_vta_get_projection(nimcp_vta_system_t* vta, uint32_t id) {
    if (!vta || !vta->initialized || id >= vta->num_projections) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,

                "nimcp_vta_get_projection: invalid parameters");

            return NULL;
    }
    return &vta->projections[id];
}

nimcp_vta_projection_t* nimcp_vta_get_projection_by_target(
    nimcp_vta_system_t* vta,
    nimcp_vta_target_t target
) {
    if (!vta || !vta->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,

                "nimcp_vta_get_projection_by_target: invalid parameters");

            return NULL;
    }

    for (uint32_t i = 0; i < vta->num_projections; i++) {
        if (vta->projections[i].target == target && vta->projections[i].enabled) {
            return &vta->projections[i];
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_vta_get_projection_by_target: validation failed");
    return NULL;
}

nimcp_vta_error_t nimcp_vta_get_da_at_target(
    nimcp_vta_system_t* vta,
    nimcp_vta_target_t target,
    float* da
) {
    if (!vta || !da) {
        return VTA_ERROR_NULL;
    }

    if (!vta->initialized) {
        return VTA_ERROR_NOT_INITIALIZED;
    }

    nimcp_vta_projection_t* proj = nimcp_vta_get_projection_by_target(vta, target);
    if (!proj) {
        *da = 0.0f;
        return VTA_ERROR_NOT_FOUND;
    }

    *da = proj->da_delivered;
    return VTA_OK;
}

nimcp_vta_error_t nimcp_vta_get_receptor_balance(
    nimcp_vta_system_t* vta,
    nimcp_vta_target_t target,
    float* d1_d2_ratio
) {
    if (!vta || !d1_d2_ratio) {
        return VTA_ERROR_NULL;
    }

    if (!vta->initialized) {
        return VTA_ERROR_NOT_INITIALIZED;
    }

    nimcp_vta_projection_t* proj = nimcp_vta_get_projection_by_target(vta, target);
    if (!proj) {
        return VTA_ERROR_NOT_FOUND;
    }

    if (proj->d2_activation > 0.001f) {
        *d1_d2_ratio = proj->d1_activation / proj->d2_activation;
    } else {
        *d1_d2_ratio = proj->d1_activation > 0.0f ? 10.0f : 1.0f;
    }

    return VTA_OK;
}

/*=============================================================================
 * Status API
 *===========================================================================*/

nimcp_vta_error_t nimcp_vta_get_state(
    nimcp_vta_system_t* vta,
    float* da,
    float* rpe,
    float* wanting,
    nimcp_vta_mode_t* mode
) {
    if (!vta) {
        return VTA_ERROR_NULL;
    }

    if (!vta->initialized) {
        return VTA_ERROR_NOT_INITIALIZED;
    }

    if (da) *da = vta->da_concentration;
    if (rpe) *rpe = vta->reward.prediction_error;
    if (wanting) *wanting = vta->motivation.wanting;
    if (mode) *mode = vta->mode;

    return VTA_OK;
}

nimcp_vta_error_t nimcp_vta_get_status(nimcp_vta_system_t* vta, nimcp_vta_status_t* status) {
    if (!vta || !status) {
        return VTA_ERROR_NULL;
    }

    if (!vta->initialized) {
        return VTA_ERROR_NOT_INITIALIZED;
    }

    *status = vta->status;
    return VTA_OK;
}

nimcp_vta_error_t nimcp_vta_get_metrics(nimcp_vta_system_t* vta, nimcp_vta_metrics_t* metrics) {
    if (!vta || !metrics) {
        return VTA_ERROR_NULL;
    }

    if (!vta->initialized) {
        return VTA_ERROR_NOT_INITIALIZED;
    }

    *metrics = vta->metrics;
    return VTA_OK;
}

nimcp_vta_error_t nimcp_vta_reset_metrics(nimcp_vta_system_t* vta) {
    if (!vta || !vta->initialized) {
        return vta ? VTA_ERROR_NOT_INITIALIZED : VTA_ERROR_NULL;
    }

    memset(&vta->metrics, 0, sizeof(vta->metrics));
    return VTA_OK;
}

/*=============================================================================
 * Utility API
 *===========================================================================*/

const char* nimcp_vta_error_string(nimcp_vta_error_t error) {
    switch (error) {
        case VTA_OK: return "OK";
        case VTA_ERROR_NULL: return "Null pointer";
        case VTA_ERROR_NOT_INITIALIZED: return "Not initialized";
        case VTA_ERROR_ALREADY_INITIALIZED: return "Already initialized";
        case VTA_ERROR_INVALID_PARAM: return "Invalid parameter";
        case VTA_ERROR_CAPACITY: return "Capacity exceeded";
        case VTA_ERROR_NOT_FOUND: return "Not found";
        case VTA_ERROR_STATE: return "Invalid state";
        case VTA_ERROR_INTERNAL: return "Internal error";
        default: return "Unknown error";
    }
}
