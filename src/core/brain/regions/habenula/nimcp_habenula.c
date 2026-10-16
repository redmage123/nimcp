/**
 * @file nimcp_habenula.c
 * @brief Habenula system implementation - Aversive learning and disappointment
 */

#include "core/brain/regions/habenula/nimcp_habenula.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE_MESH_ONLY(habenula, MESH_ADAPTER_CATEGORY_COGNITIVE)

static float lerp_f(float a, float b, float t) {
    return a + t * (b - a);
}

/*=============================================================================
 * Configuration API
 *===========================================================================*/

void nimcp_habenula_default_config(nimcp_habenula_config_t* config) {
    if (!config) return;

    config->baseline_firing_rate = HABENULA_DEFAULT_BASELINE_FIRING;
    config->max_firing_rate = HABENULA_DEFAULT_MAX_FIRING;
    config->lhb_weight = HABENULA_DEFAULT_LHB_WEIGHT;
    config->mhb_weight = HABENULA_DEFAULT_MHB_WEIGHT;
    config->vta_inhibition_gain = HABENULA_DEFAULT_VTA_INHIBITION_GAIN;
    config->raphe_modulation_gain = HABENULA_DEFAULT_RAPHE_MODULATION_GAIN;
    config->disappointment_decay = HABENULA_DEFAULT_DISAPPOINTMENT_DECAY;
    config->aversion_threshold = HABENULA_DEFAULT_AVERSION_THRESHOLD;
    config->enable_depression_model = true;
    config->enable_vta_feedback = true;
}

/*=============================================================================
 * Lifecycle API
 *===========================================================================*/

nimcp_habenula_error_t nimcp_habenula_init(nimcp_habenula_system_t* habenula,
                                           const nimcp_habenula_config_t* config) {
    if (!habenula) return HABENULA_ERROR_NULL;
    if (habenula->initialized) return HABENULA_ERROR_ALREADY_INITIALIZED;

    memset(habenula, 0, sizeof(*habenula));

    if (config) {
        habenula->config = *config;
    } else {
        nimcp_habenula_default_config(&habenula->config);
    }

    /* Initialize LHb state */
    habenula->lhb.firing_rate = habenula->config.baseline_firing_rate * 0.7f;
    habenula->lhb.disappointment = 0.0f;
    habenula->lhb.negative_rpe = 0.0f;
    habenula->lhb.expected_reward = 0.5f;
    habenula->lhb.received_reward = 0.5f;
    habenula->lhb.vta_inhibition_output = 0.0f;
    habenula->lhb.cumulative_disappointment = 0.0f;

    /* Initialize MHb state */
    habenula->mhb.firing_rate = habenula->config.baseline_firing_rate * 0.3f;
    habenula->mhb.aversion_level = 0.0f;
    habenula->mhb.withdrawal_state = 0.0f;
    habenula->mhb.ipn_output = 0.0f;
    habenula->mhb.nicotinic_sensitivity = 0.5f;

    /* Initialize combined neurons */
    habenula->neurons.combined_firing_rate = habenula->config.baseline_firing_rate;
    habenula->neurons.excitatory_input = 0.0f;
    habenula->neurons.inhibitory_input = 0.0f;
    habenula->neurons.adaptation = 0.0f;
    habenula->neurons.burst_count = 0;

    /* Initialize depression model */
    habenula->depression.helplessness_index = 0.0f;
    habenula->depression.chronic_hyperactivity = 0.0f;
    habenula->depression.coping_failure_count = 0.0f;
    habenula->depression.anhedonia_level = 0.0f;
    habenula->depression.is_depressed = false;
    habenula->depression.recovery_rate = 0.05f;

    habenula->mode = HABENULA_MODE_BASELINE;
    habenula->projection_count = 0;
    habenula->simulation_time = 0.0f;
    habenula->mode_duration = 0.0f;
    habenula->initialized = true;

    return HABENULA_OK;
}

nimcp_habenula_error_t nimcp_habenula_shutdown(nimcp_habenula_system_t* habenula) {
    if (!habenula) return HABENULA_ERROR_NULL;
    if (!habenula->initialized) return HABENULA_ERROR_NOT_INITIALIZED;

    habenula->initialized = false;
    return HABENULA_OK;
}

nimcp_habenula_error_t nimcp_habenula_reset(nimcp_habenula_system_t* habenula) {
    if (!habenula) return HABENULA_ERROR_NULL;
    if (!habenula->initialized) return HABENULA_ERROR_NOT_INITIALIZED;

    nimcp_habenula_config_t saved_config = habenula->config;
    nimcp_habenula_shutdown(habenula);
    return nimcp_habenula_init(habenula, &saved_config);
}

/*=============================================================================
 * Update API
 *===========================================================================*/

static void update_lhb(nimcp_habenula_system_t* habenula, float dt_sec) {
    nimcp_lhb_state_t* lhb = &habenula->lhb;

    /* Compute disappointment-driven firing */
    float disappointment_drive = lhb->disappointment * 30.0f; /* Disappointment increases firing */

    /* Target firing rate based on disappointment */
    float target_rate = habenula->config.baseline_firing_rate * 0.7f + disappointment_drive;

    /* Apply external inputs */
    target_rate += habenula->neurons.excitatory_input * 10.0f;
    target_rate -= habenula->neurons.inhibitory_input * 8.0f;

    /* Smooth transition to target */
    float rate_tau = 0.1f;
    lhb->firing_rate = lerp_f(lhb->firing_rate, target_rate, dt_sec / rate_tau);
    lhb->firing_rate = nimcp_clampf(lhb->firing_rate, 0.0f, habenula->config.max_firing_rate);

    /* Decay disappointment over time */
    lhb->disappointment *= (1.0f - habenula->config.disappointment_decay * dt_sec);
    lhb->disappointment = nimcp_clampf(lhb->disappointment, 0.0f, 1.0f);

    /* Compute VTA inhibition output */
    lhb->vta_inhibition_output = lhb->disappointment * habenula->config.vta_inhibition_gain;
    lhb->vta_inhibition_output = nimcp_clampf(lhb->vta_inhibition_output, 0.0f, 1.0f);
}

static void update_mhb(nimcp_habenula_system_t* habenula, float dt_sec) {
    nimcp_mhb_state_t* mhb = &habenula->mhb;

    /* Aversion-driven firing */
    float aversion_drive = mhb->aversion_level * 20.0f;

    /* Target firing rate */
    float target_rate = habenula->config.baseline_firing_rate * 0.3f + aversion_drive;

    /* Smooth transition */
    float rate_tau = 0.15f;
    mhb->firing_rate = lerp_f(mhb->firing_rate, target_rate, dt_sec / rate_tau);
    mhb->firing_rate = nimcp_clampf(mhb->firing_rate, 0.0f, habenula->config.max_firing_rate * 0.5f);

    /* Decay aversion */
    mhb->aversion_level *= (1.0f - 0.05f * dt_sec);
    mhb->aversion_level = nimcp_clampf(mhb->aversion_level, 0.0f, 1.0f);

    /* Update IPN output */
    mhb->ipn_output = mhb->firing_rate / (habenula->config.max_firing_rate * 0.5f);
}

static void update_depression_model(nimcp_habenula_system_t* habenula, float dt_sec) {
    nimcp_depression_model_t* dep = &habenula->depression;

    if (!habenula->config.enable_depression_model) return;

    /* Track chronic hyperactivity */
    if (habenula->lhb.firing_rate > habenula->config.baseline_firing_rate * 1.5f) {
        dep->chronic_hyperactivity += dt_sec;
    } else {
        dep->chronic_hyperactivity *= (1.0f - 0.1f * dt_sec);
    }

    /* Helplessness increases with accumulated disappointment and failures */
    float helplessness_increase = habenula->lhb.cumulative_disappointment * 0.01f * dt_sec;
    dep->helplessness_index += helplessness_increase;

    /* Natural recovery */
    dep->helplessness_index -= dep->recovery_rate * dt_sec;
    dep->helplessness_index = nimcp_clampf(dep->helplessness_index, 0.0f, 1.0f);

    /* Anhedonia correlates with helplessness */
    dep->anhedonia_level = dep->helplessness_index * 0.8f;

    /* Depression threshold */
    dep->is_depressed = (dep->helplessness_index > 0.6f &&
                         dep->chronic_hyperactivity > 10.0f);
}

static void update_combined_state(nimcp_habenula_system_t* habenula) {
    /* Weighted combination of LHb and MHb */
    habenula->neurons.combined_firing_rate =
        habenula->lhb.firing_rate * habenula->config.lhb_weight +
        habenula->mhb.firing_rate * habenula->config.mhb_weight;

    /* Clamp combined rate */
    habenula->neurons.combined_firing_rate = nimcp_clampf(
        habenula->neurons.combined_firing_rate,
        0.0f,
        habenula->config.max_firing_rate);

    /* Clear transient inputs */
    habenula->neurons.excitatory_input *= 0.5f;
    habenula->neurons.inhibitory_input *= 0.5f;
}

static void update_mode(nimcp_habenula_system_t* habenula, float dt_sec) {
    habenula->mode_duration += dt_sec;

    /* Auto-transition based on state */
    float combined_rate = habenula->neurons.combined_firing_rate;
    float baseline = habenula->config.baseline_firing_rate;

    if (combined_rate > baseline * 2.0f) {
        if (habenula->depression.chronic_hyperactivity > 5.0f) {
            habenula->mode = HABENULA_MODE_HYPERACTIVE;
        } else {
            habenula->mode = HABENULA_MODE_DISAPPOINTED;
        }
    } else if (combined_rate < baseline * 0.5f) {
        habenula->mode = HABENULA_MODE_SUPPRESSED;
    } else {
        habenula->mode = HABENULA_MODE_BASELINE;
    }
}

nimcp_habenula_error_t nimcp_habenula_update(nimcp_habenula_system_t* habenula,
                                              float dt_ms) {
    if (!habenula) return HABENULA_ERROR_NULL;
    if (!habenula->initialized) return HABENULA_ERROR_NOT_INITIALIZED;

    float dt_sec = dt_ms / 1000.0f;
    habenula->simulation_time += dt_sec;

    /* Update subsystems */
    update_lhb(habenula, dt_sec);
    update_mhb(habenula, dt_sec);
    update_depression_model(habenula, dt_sec);
    update_combined_state(habenula);
    update_mode(habenula, dt_sec);

    habenula->metrics.update_count++;

    return HABENULA_OK;
}

/*=============================================================================
 * Reward Prediction Error API
 *===========================================================================*/

nimcp_habenula_error_t nimcp_habenula_compute_negative_rpe(
    nimcp_habenula_system_t* habenula,
    float expected,
    float received,
    float* negative_rpe) {
    if (!habenula || !negative_rpe) return HABENULA_ERROR_NULL;
    if (!habenula->initialized) return HABENULA_ERROR_NOT_INITIALIZED;

    float rpe = received - expected;

    /* Habenula encodes NEGATIVE RPE */
    if (rpe < 0.0f) {
        *negative_rpe = -rpe; /* Convert to positive magnitude */
    } else {
        *negative_rpe = 0.0f; /* No response to positive RPE */
    }

    return HABENULA_OK;
}

nimcp_habenula_error_t nimcp_habenula_process_outcome(
    nimcp_habenula_system_t* habenula,
    float expected,
    float received) {
    if (!habenula) return HABENULA_ERROR_NULL;
    if (!habenula->initialized) return HABENULA_ERROR_NOT_INITIALIZED;

    /* Store values */
    habenula->lhb.expected_reward = expected;
    habenula->lhb.received_reward = received;

    /* Compute negative RPE */
    float rpe = received - expected;
    if (rpe < 0.0f) {
        /* Disappointment! */
        habenula->lhb.negative_rpe = -rpe;
        habenula->lhb.disappointment += -rpe;
        habenula->lhb.disappointment = nimcp_clampf(habenula->lhb.disappointment, 0.0f, 1.0f);
        habenula->lhb.cumulative_disappointment += -rpe;
        habenula->metrics.disappointment_events++;
        habenula->metrics.avg_negative_rpe =
            (habenula->metrics.avg_negative_rpe * (habenula->metrics.disappointment_events - 1) +
             (-rpe)) / habenula->metrics.disappointment_events;
    } else {
        /* Positive outcome suppresses habenula */
        habenula->lhb.negative_rpe = 0.0f;
        habenula->lhb.disappointment *= 0.5f; /* Reduce existing disappointment */
    }

    if (habenula->lhb.disappointment > habenula->metrics.peak_disappointment) {
        habenula->metrics.peak_disappointment = habenula->lhb.disappointment;
    }

    return HABENULA_OK;
}

nimcp_habenula_error_t nimcp_habenula_get_disappointment(
    nimcp_habenula_system_t* habenula,
    float* disappointment) {
    if (!habenula || !disappointment) return HABENULA_ERROR_NULL;
    if (!habenula->initialized) return HABENULA_ERROR_NOT_INITIALIZED;

    *disappointment = habenula->lhb.disappointment;
    return HABENULA_OK;
}

nimcp_habenula_error_t nimcp_habenula_apply_aversive(
    nimcp_habenula_system_t* habenula,
    float intensity) {
    if (!habenula) return HABENULA_ERROR_NULL;
    if (!habenula->initialized) return HABENULA_ERROR_NOT_INITIALIZED;

    intensity = nimcp_clampf(intensity, 0.0f, 1.0f);

    /* Aversive stimuli activate both LHb and MHb */
    habenula->lhb.disappointment += intensity * 0.5f;
    habenula->lhb.disappointment = nimcp_clampf(habenula->lhb.disappointment, 0.0f, 1.0f);

    habenula->mhb.aversion_level += intensity;
    habenula->mhb.aversion_level = nimcp_clampf(habenula->mhb.aversion_level, 0.0f, 1.0f);

    habenula->metrics.aversion_events++;

    return HABENULA_OK;
}

/*=============================================================================
 * VTA Inhibition API
 *===========================================================================*/

nimcp_habenula_error_t nimcp_habenula_get_vta_inhibition(
    nimcp_habenula_system_t* habenula,
    float* inhibition) {
    if (!habenula || !inhibition) return HABENULA_ERROR_NULL;
    if (!habenula->initialized) return HABENULA_ERROR_NOT_INITIALIZED;

    *inhibition = habenula->lhb.vta_inhibition_output;
    return HABENULA_OK;
}

nimcp_habenula_error_t nimcp_habenula_apply_vta_feedback(
    nimcp_habenula_system_t* habenula,
    float da_level) {
    if (!habenula) return HABENULA_ERROR_NULL;
    if (!habenula->initialized) return HABENULA_ERROR_NOT_INITIALIZED;

    if (!habenula->config.enable_vta_feedback) return HABENULA_OK;

    /* High DA inhibits habenula (reciprocal inhibition) */
    float normalized_da = nimcp_clampf(da_level / 100.0f, 0.0f, 1.0f);
    if (normalized_da > 0.6f) {
        habenula->neurons.inhibitory_input += (normalized_da - 0.6f) * 0.5f;
    }

    return HABENULA_OK;
}

/*=============================================================================
 * Raphe Modulation API
 *===========================================================================*/

nimcp_habenula_error_t nimcp_habenula_get_raphe_modulation(
    nimcp_habenula_system_t* habenula,
    float* modulation) {
    if (!habenula || !modulation) return HABENULA_ERROR_NULL;
    if (!habenula->initialized) return HABENULA_ERROR_NOT_INITIALIZED;

    /* Habenula activation affects Raphe
     * High habenula -> increased Raphe (can contribute to mood effects) */
    float normalized_rate = habenula->neurons.combined_firing_rate /
                           habenula->config.max_firing_rate;
    *modulation = normalized_rate * habenula->config.raphe_modulation_gain;

    return HABENULA_OK;
}

/*=============================================================================
 * Avoidance Learning API
 *===========================================================================*/

nimcp_habenula_error_t nimcp_habenula_get_avoidance_signal(
    nimcp_habenula_system_t* habenula,
    float* avoidance) {
    if (!habenula || !avoidance) return HABENULA_ERROR_NULL;
    if (!habenula->initialized) return HABENULA_ERROR_NOT_INITIALIZED;

    /* Avoidance signal based on disappointment and aversion */
    *avoidance = (habenula->lhb.disappointment + habenula->mhb.aversion_level) * 0.5f;
    *avoidance = nimcp_clampf(*avoidance, 0.0f, 1.0f);

    return HABENULA_OK;
}

nimcp_habenula_error_t nimcp_habenula_should_avoid(
    nimcp_habenula_system_t* habenula,
    float stimulus_value,
    bool* should_avoid) {
    if (!habenula || !should_avoid) return HABENULA_ERROR_NULL;
    if (!habenula->initialized) return HABENULA_ERROR_NOT_INITIALIZED;

    float avoidance;
    nimcp_habenula_get_avoidance_signal(habenula, &avoidance);

    /* Should avoid if avoidance signal exceeds threshold and stimulus has negative value */
    *should_avoid = (avoidance > habenula->config.aversion_threshold) ||
                    (stimulus_value < -habenula->config.aversion_threshold);

    return HABENULA_OK;
}

/*=============================================================================
 * Depression Model API
 *===========================================================================*/

nimcp_habenula_error_t nimcp_habenula_get_helplessness(
    nimcp_habenula_system_t* habenula,
    float* helplessness) {
    if (!habenula || !helplessness) return HABENULA_ERROR_NULL;
    if (!habenula->initialized) return HABENULA_ERROR_NOT_INITIALIZED;

    *helplessness = habenula->depression.helplessness_index;
    return HABENULA_OK;
}

nimcp_habenula_error_t nimcp_habenula_get_anhedonia(
    nimcp_habenula_system_t* habenula,
    float* anhedonia) {
    if (!habenula || !anhedonia) return HABENULA_ERROR_NULL;
    if (!habenula->initialized) return HABENULA_ERROR_NOT_INITIALIZED;

    *anhedonia = habenula->depression.anhedonia_level;
    return HABENULA_OK;
}

nimcp_habenula_error_t nimcp_habenula_is_depressed(
    nimcp_habenula_system_t* habenula,
    bool* is_depressed) {
    if (!habenula || !is_depressed) return HABENULA_ERROR_NULL;
    if (!habenula->initialized) return HABENULA_ERROR_NOT_INITIALIZED;

    *is_depressed = habenula->depression.is_depressed;
    return HABENULA_OK;
}

nimcp_habenula_error_t nimcp_habenula_record_coping_failure(
    nimcp_habenula_system_t* habenula) {
    if (!habenula) return HABENULA_ERROR_NULL;
    if (!habenula->initialized) return HABENULA_ERROR_NOT_INITIALIZED;

    habenula->depression.coping_failure_count += 1.0f;
    habenula->depression.helplessness_index += 0.1f;
    habenula->depression.helplessness_index = nimcp_clampf(
        habenula->depression.helplessness_index, 0.0f, 1.0f);

    return HABENULA_OK;
}

nimcp_habenula_error_t nimcp_habenula_record_coping_success(
    nimcp_habenula_system_t* habenula) {
    if (!habenula) return HABENULA_ERROR_NULL;
    if (!habenula->initialized) return HABENULA_ERROR_NOT_INITIALIZED;

    /* Success reduces helplessness */
    habenula->depression.helplessness_index -= 0.15f;
    habenula->depression.helplessness_index = nimcp_clampf(
        habenula->depression.helplessness_index, 0.0f, 1.0f);

    /* Also reduces disappointment */
    habenula->lhb.disappointment *= 0.8f;

    return HABENULA_OK;
}

/*=============================================================================
 * Mode API
 *===========================================================================*/

nimcp_habenula_error_t nimcp_habenula_get_mode(nimcp_habenula_system_t* habenula,
                                                nimcp_habenula_mode_t* mode) {
    if (!habenula || !mode) return HABENULA_ERROR_NULL;
    if (!habenula->initialized) return HABENULA_ERROR_NOT_INITIALIZED;

    *mode = habenula->mode;
    return HABENULA_OK;
}

nimcp_habenula_error_t nimcp_habenula_set_mode(nimcp_habenula_system_t* habenula,
                                                nimcp_habenula_mode_t mode) {
    if (!habenula) return HABENULA_ERROR_NULL;
    if (!habenula->initialized) return HABENULA_ERROR_NOT_INITIALIZED;

    habenula->mode = mode;
    habenula->mode_duration = 0.0f;

    return HABENULA_OK;
}

/*=============================================================================
 * Input API
 *===========================================================================*/

nimcp_habenula_error_t nimcp_habenula_apply_excitation(
    nimcp_habenula_system_t* habenula,
    float strength) {
    if (!habenula) return HABENULA_ERROR_NULL;
    if (!habenula->initialized) return HABENULA_ERROR_NOT_INITIALIZED;

    habenula->neurons.excitatory_input += nimcp_clampf(strength, 0.0f, 1.0f);
    return HABENULA_OK;
}

nimcp_habenula_error_t nimcp_habenula_apply_inhibition(
    nimcp_habenula_system_t* habenula,
    float strength) {
    if (!habenula) return HABENULA_ERROR_NULL;
    if (!habenula->initialized) return HABENULA_ERROR_NOT_INITIALIZED;

    habenula->neurons.inhibitory_input += nimcp_clampf(strength, 0.0f, 1.0f);
    return HABENULA_OK;
}

/*=============================================================================
 * Firing Rate API
 *===========================================================================*/

nimcp_habenula_error_t nimcp_habenula_get_firing_rate(
    nimcp_habenula_system_t* habenula,
    float* rate) {
    if (!habenula || !rate) return HABENULA_ERROR_NULL;
    if (!habenula->initialized) return HABENULA_ERROR_NOT_INITIALIZED;

    *rate = habenula->neurons.combined_firing_rate;
    return HABENULA_OK;
}

nimcp_habenula_error_t nimcp_habenula_get_region_firing_rate(
    nimcp_habenula_system_t* habenula,
    nimcp_habenula_region_t region,
    float* rate) {
    if (!habenula || !rate) return HABENULA_ERROR_NULL;
    if (!habenula->initialized) return HABENULA_ERROR_NOT_INITIALIZED;

    switch (region) {
        case HABENULA_REGION_LHB:
            *rate = habenula->lhb.firing_rate;
            break;
        case HABENULA_REGION_MHB:
            *rate = habenula->mhb.firing_rate;
            break;
        case HABENULA_REGION_BOTH:
            *rate = habenula->neurons.combined_firing_rate;
            break;
        default:
            return HABENULA_ERROR_INVALID_PARAM;
    }

    return HABENULA_OK;
}

/*=============================================================================
 * Projection API
 *===========================================================================*/

nimcp_habenula_error_t nimcp_habenula_add_projection(
    nimcp_habenula_system_t* habenula,
    nimcp_habenula_target_t target,
    nimcp_habenula_region_t source,
    float weight,
    bool is_inhibitory) {
    if (!habenula) return HABENULA_ERROR_NULL;
    if (!habenula->initialized) return HABENULA_ERROR_NOT_INITIALIZED;
    if (habenula->projection_count >= HABENULA_MAX_PROJECTIONS) {
        return HABENULA_ERROR_FULL;
    }

    nimcp_habenula_projection_t* proj =
        &habenula->projections[habenula->projection_count];

    proj->target = target;
    proj->source = source;
    proj->weight = nimcp_clampf(weight, 0.0f, 1.0f);
    proj->delay_ms = 5.0f; /* Default delay */
    proj->is_inhibitory = is_inhibitory;
    proj->active = true;
    proj->target_data = NULL;

    habenula->projection_count++;

    return HABENULA_OK;
}

nimcp_habenula_error_t nimcp_habenula_get_projection(
    nimcp_habenula_system_t* habenula,
    uint32_t index,
    nimcp_habenula_projection_t* projection) {
    if (!habenula || !projection) return HABENULA_ERROR_NULL;
    if (!habenula->initialized) return HABENULA_ERROR_NOT_INITIALIZED;
    if (index >= habenula->projection_count) return HABENULA_ERROR_INVALID_PARAM;

    *projection = habenula->projections[index];
    return HABENULA_OK;
}

nimcp_habenula_error_t nimcp_habenula_get_output_to_target(
    nimcp_habenula_system_t* habenula,
    nimcp_habenula_target_t target,
    float* output) {
    if (!habenula || !output) return HABENULA_ERROR_NULL;
    if (!habenula->initialized) return HABENULA_ERROR_NOT_INITIALIZED;

    *output = 0.0f;

    /* Find projections to this target */
    for (uint32_t i = 0; i < habenula->projection_count; i++) {
        if (habenula->projections[i].target == target &&
            habenula->projections[i].active) {

            float source_rate = 0.0f;
            switch (habenula->projections[i].source) {
                case HABENULA_REGION_LHB:
                    source_rate = habenula->lhb.firing_rate;
                    break;
                case HABENULA_REGION_MHB:
                    source_rate = habenula->mhb.firing_rate;
                    break;
                case HABENULA_REGION_BOTH:
                    source_rate = habenula->neurons.combined_firing_rate;
                    break;
            }

            float contribution = source_rate / habenula->config.max_firing_rate;
            contribution *= habenula->projections[i].weight;

            if (habenula->projections[i].is_inhibitory) {
                *output -= contribution;
            } else {
                *output += contribution;
            }
        }
    }

    /* Default output for VTA if no explicit projection */
    if (target == HABENULA_TARGET_VTA && *output == 0.0f) {
        *output = -habenula->lhb.vta_inhibition_output; /* Inhibitory to VTA */
    }

    return HABENULA_OK;
}

/*=============================================================================
 * State API
 *===========================================================================*/

nimcp_habenula_error_t nimcp_habenula_get_state(
    nimcp_habenula_system_t* habenula,
    float* firing_rate,
    float* disappointment,
    float* aversion,
    float* vta_inhibition) {
    if (!habenula) return HABENULA_ERROR_NULL;
    if (!habenula->initialized) return HABENULA_ERROR_NOT_INITIALIZED;

    if (firing_rate) *firing_rate = habenula->neurons.combined_firing_rate;
    if (disappointment) *disappointment = habenula->lhb.disappointment;
    if (aversion) *aversion = habenula->mhb.aversion_level;
    if (vta_inhibition) *vta_inhibition = habenula->lhb.vta_inhibition_output;

    return HABENULA_OK;
}

nimcp_habenula_error_t nimcp_habenula_get_status(
    nimcp_habenula_system_t* habenula,
    nimcp_habenula_status_t* status) {
    if (!habenula || !status) return HABENULA_ERROR_NULL;
    if (!habenula->initialized) return HABENULA_ERROR_NOT_INITIALIZED;

    float rate = habenula->neurons.combined_firing_rate;
    float baseline = habenula->config.baseline_firing_rate;

    if (habenula->depression.is_depressed || rate > baseline * 2.5f) {
        *status = HABENULA_STATUS_HYPERACTIVE;
    } else if (rate < baseline * 0.3f) {
        *status = HABENULA_STATUS_HYPOACTIVE;
    } else if (habenula->lhb.disappointment > 0.8f || habenula->mhb.aversion_level > 0.8f) {
        *status = HABENULA_STATUS_UNSTABLE;
    } else {
        *status = HABENULA_STATUS_NORMAL;
    }

    return HABENULA_OK;
}

/*=============================================================================
 * Metrics API
 *===========================================================================*/

nimcp_habenula_error_t nimcp_habenula_get_metrics(
    nimcp_habenula_system_t* habenula,
    nimcp_habenula_metrics_t* metrics) {
    if (!habenula || !metrics) return HABENULA_ERROR_NULL;
    if (!habenula->initialized) return HABENULA_ERROR_NOT_INITIALIZED;

    *metrics = habenula->metrics;
    return HABENULA_OK;
}

nimcp_habenula_error_t nimcp_habenula_reset_metrics(
    nimcp_habenula_system_t* habenula) {
    if (!habenula) return HABENULA_ERROR_NULL;
    if (!habenula->initialized) return HABENULA_ERROR_NOT_INITIALIZED;

    memset(&habenula->metrics, 0, sizeof(habenula->metrics));
    return HABENULA_OK;
}

/*=============================================================================
 * Utility API
 *===========================================================================*/

const char* nimcp_habenula_error_string(nimcp_habenula_error_t error) {
    switch (error) {
        case HABENULA_OK: return "OK";
        case HABENULA_ERROR_NULL: return "Null pointer";
        case HABENULA_ERROR_NOT_INITIALIZED: return "Not initialized";
        case HABENULA_ERROR_ALREADY_INITIALIZED: return "Already initialized";
        case HABENULA_ERROR_INVALID_PARAM: return "Invalid parameter";
        case HABENULA_ERROR_RESOURCE: return "Resource error";
        case HABENULA_ERROR_STATE: return "Invalid state";
        case HABENULA_ERROR_FULL: return "Capacity full";
        default: return "Unknown error";
    }
}
