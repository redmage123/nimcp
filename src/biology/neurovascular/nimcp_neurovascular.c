/**
 * @file nimcp_neurovascular.c
 * @brief Neurovascular Coupling Module Implementation
 * @version 1.0.0
 * @date 2026-01-11
 */

#include "biology/neurovascular/nimcp_neurovascular.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>

#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(neurovascular)

//=============================================================================
// Internal Constants
//=============================================================================

/** Balloon model time constants */
#define BALLOON_TAU_MTT     2.0f        /* Mean transit time (s) */
#define BALLOON_TAU_F       0.5f        /* Flow signal decay (s) */
#define BALLOON_ALPHA       0.38f       /* Grubb's exponent */
#define BALLOON_E0          0.4f        /* Resting O2 extraction */

/** BOLD signal parameters */
#define BOLD_V0             0.02f       /* Resting blood volume fraction */
#define BOLD_K1             4.3f        /* T2* relaxivity of dHb */
#define BOLD_K2             1.3f        /* Intravascular contribution */
#define BOLD_K3             0.4f        /* Extravascular contribution */

/** Double-gamma HRF parameters */
#define HRF_GAMMA1_SHAPE    6.0f        /* First gamma shape */
#define HRF_GAMMA1_SCALE    1.0f        /* First gamma scale */
#define HRF_GAMMA2_SHAPE    16.0f       /* Second gamma shape */
#define HRF_GAMMA2_SCALE    1.0f        /* Second gamma scale */
#define HRF_GAMMA2_RATIO    0.167f      /* Undershoot ratio */

/**
 * @brief Gamma function approximation (Stirling's approximation for integer args)
 */
static float gamma_pdf(float t, float shape, float scale) {
    if (t <= 0.0f) return 0.0f;

    float x = t / scale;
    float log_pdf = (shape - 1.0f) * logf(x) - x - lgammaf(shape) - logf(scale);
    return expf(log_pdf);
}

/**
 * @brief Double-gamma HRF kernel
 */
static float double_gamma_hrf(float t, float time_to_peak, float undershoot_ratio) {
    /* First gamma (positive response) */
    float g1_shape = HRF_GAMMA1_SHAPE;
    float g1_scale = time_to_peak / g1_shape;
    float g1 = gamma_pdf(t, g1_shape, g1_scale);

    /* Second gamma (undershoot) */
    float g2_shape = HRF_GAMMA2_SHAPE;
    float g2_scale = (time_to_peak * 2.5f) / g2_shape;
    float g2 = gamma_pdf(t, g2_shape, g2_scale);

    return g1 - undershoot_ratio * g2;
}

/**
 * @brief Calculate BOLD signal using Balloon model
 */
static float calculate_bold(
    float cbf_change,       /* Relative CBF (1.0 = baseline) */
    float cbv_change,       /* Relative CBV */
    float e0,               /* Resting extraction fraction */
    float v0                /* Resting blood volume fraction */
) {
    /* Nonlinear BOLD signal equation */
    /* BOLD = V0 * (k1*(1-q) + k2*(1-q/v) + k3*(1-v)) */

    float f = cbf_change;
    float v = cbv_change;

    /* Oxygen extraction (nonlinear) */
    float q;
    if (f > 0.0f) {
        q = (1.0f - powf(1.0f - e0, 1.0f / f)) / e0;
    } else {
        q = 1.0f;
    }

    /* Relative deoxyhemoglobin change */
    float dq = q * v;

    /* BOLD signal (simplified) */
    if (fabsf(v) < 1e-7f) v = 1e-7f;  /* Prevent division by zero */
    float bold = v0 * (BOLD_K1 * (1.0f - q) +
                       BOLD_K2 * (1.0f - dq / v) +
                       BOLD_K3 * (1.0f - v));

    return bold * 100.0f;  /* Convert to percent */
}

/**
 * @brief Determine NVC state from activity and CBF
 */
static nimcp_nvc_state_t determine_nvc_state(
    float activity,
    float cbf_ratio,
    float time_since_activation
) {
    float cbf_change = cbf_ratio - 1.0f;

    if (activity > 0.5f && cbf_change > 0.1f) {
        return NVC_STATE_PEAK;
    } else if (activity > 0.1f || cbf_change > 0.05f) {
        return NVC_STATE_ACTIVATED;
    } else if (cbf_change < -0.02f) {
        return NVC_STATE_UNDERSHOOT;
    } else if (time_since_activation > 0.0f && time_since_activation < 15.0f) {
        return NVC_STATE_RECOVERING;
    } else {
        return NVC_STATE_RESTING;
    }
}

/**
 * @brief Initialize default configuration
 */
static void init_default_config(nimcp_nvc_config_t* config) {
    config->baseline_cbf = NVC_BASELINE_CBF;
    config->baseline_cbv = NVC_BASELINE_CBV;
    config->baseline_oef = NVC_BASELINE_OEF;

    config->hrf_time_to_peak = NVC_HRF_TIME_TO_PEAK;
    config->hrf_undershoot_ratio = NVC_UNDERSHOOT_DEPTH;
    config->hrf_duration = NVC_HRF_DURATION;

    config->coupling_strength = 1.0f;
    config->coupling_delay = NVC_COUPLING_DELAY;

    config->max_cbf_increase = NVC_MAX_CBF_INCREASE;
    config->max_cbv_increase = 1.5f;

    config->tau_mtt = BALLOON_TAU_MTT;
    config->alpha_grubb = BALLOON_ALPHA;
    config->e0 = BALLOON_E0;

    config->on_perfusion_change = NULL;
    config->on_bold_update = NULL;
    config->callback_data = NULL;
}

/**
 * @brief Initialize unit with defaults
 */
static void init_unit_defaults(
    nimcp_nvc_unit_t* unit,
    uint32_t id,
    const nimcp_nvc_config_t* config
) {
    memset(unit, 0, sizeof(nimcp_nvc_unit_t));

    unit->id = id;
    snprintf(unit->name, sizeof(unit->name), "NVU_%u", id);

    /* Blood flow baseline */
    unit->cbf = config->baseline_cbf;
    unit->cbf_baseline = config->baseline_cbf;
    unit->cbf_target = config->baseline_cbf;

    unit->cbv = config->baseline_cbv;
    unit->cbv_baseline = config->baseline_cbv;

    unit->oef = config->baseline_oef;
    unit->cmro2 = config->baseline_cbf * config->baseline_oef;

    /* Vessel state */
    unit->vessel_diameter = 1.0f;
    unit->vessel_tone = 0.5f;

    /* Astrocyte coupling */
    unit->astrocyte_coupling = 1.0f;

    /* Initialize HRF */
    nimcp_nvc_init_hrf(&unit->hrf, 0.5f);

    /* BOLD baseline */
    unit->bold.signal = 0.0f;
    unit->bold.cbf_change = 0.0f;
    unit->bold.cbv_change = 0.0f;
    unit->bold.oef = config->baseline_oef;

    unit->state = NVC_STATE_RESTING;
    unit->initialized = true;
}

//=============================================================================
// Lifecycle API Implementation
//=============================================================================

nimcp_nvc_error_t nimcp_nvc_init(
    nimcp_nvc_system_t* system,
    const nimcp_nvc_config_t* config
) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Neurovascular system is NULL");
        return NVC_ERR_NULL_PTR;
    }

    memset(system, 0, sizeof(nimcp_nvc_system_t));

    /* Apply configuration */
    if (config) {
        memcpy(&system->config, config, sizeof(nimcp_nvc_config_t));
    } else {
        init_default_config(&system->config);
    }

    /* Initialize global state */
    system->global_cbf = system->config.baseline_cbf;
    system->global_bold = 0.0f;
    system->perfusion_reserve = system->config.max_cbf_increase - 1.0f;

    system->initialized = true;
    system->update_count = 0;
    system->current_time = 0.0f;

    return NVC_OK;
}

nimcp_nvc_error_t nimcp_nvc_shutdown(nimcp_nvc_system_t* system) {
    if (!system) {
        return NVC_ERR_NULL_PTR;
    }

    memset(system, 0, sizeof(nimcp_nvc_system_t));

    return NVC_OK;
}

nimcp_nvc_error_t nimcp_nvc_reset(nimcp_nvc_system_t* system) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Neurovascular system is NULL in reset");
        return NVC_ERR_NULL_PTR;
    }

    if (!system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "Neurovascular system not initialized");
        return NVC_ERR_NOT_INITIALIZED;
    }

    /* Reset all units */
    for (uint32_t i = 0; i < system->num_units; i++) {
        nimcp_nvc_unit_t* unit = &system->units[i];

        unit->neural_activity = 0.0f;
        memset(unit->activity_history, 0, sizeof(unit->activity_history));
        unit->history_index = 0;

        unit->cbf = unit->cbf_baseline;
        unit->cbf_target = unit->cbf_baseline;
        unit->cbv = unit->cbv_baseline;
        unit->oef = system->config.baseline_oef;

        unit->vessel_diameter = 1.0f;
        unit->vessel_tone = 0.5f;

        unit->no_level = 0.0f;
        unit->prostaglandin_level = 0.0f;
        unit->adenosine_level = 0.0f;
        unit->potassium_level = 0.0f;
        unit->astrocyte_calcium = 0.0f;

        unit->bold.signal = 0.0f;
        unit->bold.cbf_change = 0.0f;
        unit->bold.cbv_change = 0.0f;

        unit->state = NVC_STATE_RESTING;
        unit->time_since_activation = -1.0f;
    }

    /* Reset global state */
    system->global_cbf = system->config.baseline_cbf;
    system->global_bold = 0.0f;
    system->perfusion_reserve = system->config.max_cbf_increase - 1.0f;

    memset(&system->metrics, 0, sizeof(nimcp_nvc_metrics_t));

    system->update_count = 0;
    system->current_time = 0.0f;

    return NVC_OK;
}

//=============================================================================
// Unit Management API Implementation
//=============================================================================

nimcp_nvc_error_t nimcp_nvc_add_unit(
    nimcp_nvc_system_t* system,
    const char* name,
    const float position[3],
    uint32_t* unit_id
) {
    if (!system || !name || !position || !unit_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NVC add_unit: NULL argument");
        return NVC_ERR_NULL_PTR;
    }

    if (!system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "NVC system not initialized in add_unit");
        return NVC_ERR_NOT_INITIALIZED;
    }

    if (system->num_units >= NVC_MAX_UNITS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "NVC unit capacity exceeded");
        return NVC_ERR_CAPACITY_EXCEEDED;
    }

    uint32_t id = system->num_units;
    nimcp_nvc_unit_t* unit = &system->units[id];

    init_unit_defaults(unit, id, &system->config);
    strncpy(unit->name, name, sizeof(unit->name) - 1);
    unit->name[sizeof(unit->name) - 1] = '\0';
    unit->position[0] = position[0];
    unit->position[1] = position[1];
    unit->position[2] = position[2];

    system->num_units++;
    *unit_id = id;

    system->metrics.total_units++;

    return NVC_OK;
}

nimcp_nvc_unit_t* nimcp_nvc_get_unit(
    nimcp_nvc_system_t* system,
    uint32_t unit_id
) {
    if (!system || !system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_nvc_get_unit: required parameter is NULL (system, system->initialized)");
        return NULL;
    }

    if (unit_id >= system->num_units) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_nvc_get_unit: unit_id out of range");
        return NULL;
    }

    return &system->units[unit_id];
}

nimcp_nvc_error_t nimcp_nvc_remove_unit(
    nimcp_nvc_system_t* system,
    uint32_t unit_id
) {
    if (!system) {
        return NVC_ERR_NULL_PTR;
    }

    if (!system->initialized) {
        return NVC_ERR_NOT_INITIALIZED;
    }

    if (unit_id >= system->num_units) {
        return NVC_ERR_UNIT_NOT_FOUND;
    }

    /* Shift units down */
    for (uint32_t i = unit_id; i < system->num_units - 1; i++) {
        memcpy(&system->units[i], &system->units[i + 1], sizeof(nimcp_nvc_unit_t));
        system->units[i].id = i;
    }

    system->num_units--;

    return NVC_OK;
}

//=============================================================================
// Neural Activity API Implementation
//=============================================================================

nimcp_nvc_error_t nimcp_nvc_set_activity(
    nimcp_nvc_unit_t* unit,
    float activity
) {
    if (!unit) {
        return NVC_ERR_NULL_PTR;
    }

    unit->neural_activity = nimcp_clampf(activity, 0.0f, 1.0f);

    /* Record in history */
    unit->activity_history[unit->history_index] = unit->neural_activity;
    unit->history_index = (unit->history_index + 1) % NVC_HRF_KERNEL_SIZE;

    /* Reset time since activation if significant activity */
    if (activity > 0.1f) {
        unit->time_since_activation = 0.0f;
    }

    return NVC_OK;
}

nimcp_nvc_error_t nimcp_nvc_apply_stimulus(
    nimcp_nvc_unit_t* unit,
    float amplitude,
    float duration
) {
    if (!unit) {
        return NVC_ERR_NULL_PTR;
    }

    /* Just set activity for now - duration handled by update loop */
    unit->neural_activity = nimcp_clampf(amplitude, 0.0f, 1.0f);
    unit->time_since_activation = 0.0f;

    (void)duration;  /* Duration would be handled externally */

    return NVC_OK;
}

nimcp_nvc_error_t nimcp_nvc_set_vasoactive(
    nimcp_nvc_unit_t* unit,
    nimcp_nvc_mechanism_t mechanism,
    float level
) {
    if (!unit) {
        return NVC_ERR_NULL_PTR;
    }

    level = nimcp_clampf(level, 0.0f, 10.0f);

    switch (mechanism) {
        case NVC_MECHANISM_NO:
            unit->no_level = level;
            break;
        case NVC_MECHANISM_PROSTAGLANDIN:
            unit->prostaglandin_level = level;
            break;
        case NVC_MECHANISM_K_CHANNEL:
            unit->potassium_level = level;
            break;
        case NVC_MECHANISM_ADENOSINE:
            unit->adenosine_level = level;
            break;
        default:
            return NVC_ERR_INVALID_PARAM;
    }

    return NVC_OK;
}

//=============================================================================
// Blood Flow API Implementation
//=============================================================================

nimcp_nvc_error_t nimcp_nvc_get_cbf(
    const nimcp_nvc_unit_t* unit,
    float* cbf
) {
    if (!unit || !cbf) {
        return NVC_ERR_NULL_PTR;
    }

    *cbf = unit->cbf;

    return NVC_OK;
}

nimcp_nvc_error_t nimcp_nvc_get_cbf_change(
    const nimcp_nvc_unit_t* unit,
    float* change
) {
    if (!unit || !change) {
        return NVC_ERR_NULL_PTR;
    }

    *change = ((unit->cbf / unit->cbf_baseline) - 1.0f) * 100.0f;

    return NVC_OK;
}

nimcp_nvc_error_t nimcp_nvc_get_oef(
    const nimcp_nvc_unit_t* unit,
    float* oef
) {
    if (!unit || !oef) {
        return NVC_ERR_NULL_PTR;
    }

    *oef = unit->oef;

    return NVC_OK;
}

//=============================================================================
// BOLD Signal API Implementation
//=============================================================================

nimcp_nvc_error_t nimcp_nvc_get_bold(
    const nimcp_nvc_unit_t* unit,
    float* bold
) {
    if (!unit || !bold) {
        return NVC_ERR_NULL_PTR;
    }

    *bold = unit->bold.signal;

    return NVC_OK;
}

nimcp_nvc_error_t nimcp_nvc_get_bold_state(
    const nimcp_nvc_unit_t* unit,
    nimcp_nvc_bold_t* bold_state
) {
    if (!unit || !bold_state) {
        return NVC_ERR_NULL_PTR;
    }

    memcpy(bold_state, &unit->bold, sizeof(nimcp_nvc_bold_t));

    return NVC_OK;
}

nimcp_nvc_error_t nimcp_nvc_generate_fmri(
    nimcp_nvc_unit_t* unit,
    const float* stimulus_times,
    uint32_t num_stimuli,
    float dt,
    float duration,
    float* timeseries,
    uint32_t* num_samples
) {
    if (!unit || !stimulus_times || !timeseries || !num_samples) {
        return NVC_ERR_NULL_PTR;
    }

    uint32_t n_samples = (uint32_t)(duration / dt) + 1;
    *num_samples = n_samples;

    /* Generate neural stimulus function */
    float* neural_input = (float*)nimcp_calloc(n_samples, sizeof(float));
    if (!neural_input) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, n_samples * sizeof(float), "Neural input array allocation failed in fMRI generation");
        return NVC_ERR_NO_MEMORY;
    }

    /* Place stimuli */
    for (uint32_t i = 0; i < num_stimuli; i++) {
        uint32_t idx = (uint32_t)(stimulus_times[i] / dt);
        if (idx < n_samples) {
            neural_input[idx] = 1.0f;
        }
    }

    /* Convolve with HRF */
    for (uint32_t i = 0; i < n_samples; i++) {
        float t = i * dt;
        timeseries[i] = 0.0f;

        for (uint32_t j = 0; j <= i && j < NVC_HRF_KERNEL_SIZE; j++) {
            float hrf_val = unit->hrf.kernel[j];
            if (i >= j) {
                timeseries[i] += neural_input[i - j] * hrf_val;
            }
        }

        /* Scale to BOLD signal range */
        timeseries[i] *= 2.0f;  /* ~2% peak BOLD */
    }

    nimcp_free(neural_input);

    return NVC_OK;
}

//=============================================================================
// HRF API Implementation
//=============================================================================

nimcp_nvc_error_t nimcp_nvc_init_hrf(
    nimcp_nvc_hrf_t* hrf,
    float dt
) {
    if (!hrf) {
        return NVC_ERR_NULL_PTR;
    }

    memset(hrf, 0, sizeof(nimcp_nvc_hrf_t));

    hrf->dt = dt;
    hrf->time_to_peak = NVC_HRF_TIME_TO_PEAK;
    hrf->undershoot_ratio = HRF_GAMMA2_RATIO;
    hrf->kernel_length = NVC_HRF_KERNEL_SIZE;

    /* Generate double-gamma kernel */
    float max_val = 0.0f;
    for (uint32_t i = 0; i < NVC_HRF_KERNEL_SIZE; i++) {
        float t = i * dt;
        hrf->kernel[i] = double_gamma_hrf(t, hrf->time_to_peak, hrf->undershoot_ratio);
        if (hrf->kernel[i] > max_val) {
            max_val = hrf->kernel[i];
        }
    }

    /* Normalize */
    if (max_val > 0.0f) {
        for (uint32_t i = 0; i < NVC_HRF_KERNEL_SIZE; i++) {
            hrf->kernel[i] /= max_val;
        }
    }

    /* Calculate FWHM */
    float half_max = 0.5f;
    uint32_t left = 0, right = 0;
    for (uint32_t i = 0; i < NVC_HRF_KERNEL_SIZE; i++) {
        if (hrf->kernel[i] > half_max && left == 0) {
            left = i;
        }
        if (hrf->kernel[i] > half_max) {
            right = i;
        }
    }
    hrf->fwhm = (right - left) * dt;

    return NVC_OK;
}

nimcp_nvc_error_t nimcp_nvc_set_hrf_params(
    nimcp_nvc_hrf_t* hrf,
    float time_to_peak,
    float undershoot_ratio,
    float fwhm
) {
    if (!hrf) {
        return NVC_ERR_NULL_PTR;
    }

    hrf->time_to_peak = time_to_peak;
    hrf->undershoot_ratio = undershoot_ratio;
    hrf->fwhm = fwhm;

    /* Regenerate kernel */
    float max_val = 0.0f;
    for (uint32_t i = 0; i < hrf->kernel_length; i++) {
        float t = i * hrf->dt;
        hrf->kernel[i] = double_gamma_hrf(t, time_to_peak, undershoot_ratio);
        if (hrf->kernel[i] > max_val) {
            max_val = hrf->kernel[i];
        }
    }

    if (max_val > 0.0f) {
        for (uint32_t i = 0; i < hrf->kernel_length; i++) {
            hrf->kernel[i] /= max_val;
        }
    }

    return NVC_OK;
}

nimcp_nvc_error_t nimcp_nvc_convolve_hrf(
    const nimcp_nvc_hrf_t* hrf,
    const float* activity_history,
    uint32_t history_length,
    float* response
) {
    if (!hrf || !activity_history || !response) {
        return NVC_ERR_NULL_PTR;
    }

    *response = 0.0f;

    uint32_t conv_length = (history_length < hrf->kernel_length) ?
                           history_length : hrf->kernel_length;

    for (uint32_t i = 0; i < conv_length; i++) {
        *response += activity_history[i] * hrf->kernel[i];
    }

    return NVC_OK;
}

//=============================================================================
// Update API Implementation
//=============================================================================

nimcp_nvc_error_t nimcp_nvc_update_unit(
    nimcp_nvc_system_t* system,
    nimcp_nvc_unit_t* unit,
    float dt
) {
    if (!system || !unit) {
        return NVC_ERR_NULL_PTR;
    }

    float dt_sec = dt / 1000.0f;

    /* Update time tracking */
    if (unit->time_since_activation >= 0.0f) {
        unit->time_since_activation += dt_sec;
    }

    /* Calculate neural-vascular coupling signal */
    float nvc_signal = 0.0f;

    /* Direct neural activity */
    nvc_signal += unit->neural_activity * system->config.coupling_strength;

    /* Vasoactive signals */
    nvc_signal += unit->no_level * 0.3f;
    nvc_signal += unit->prostaglandin_level * 0.2f;
    nvc_signal += unit->adenosine_level * 0.3f;
    nvc_signal += unit->potassium_level * 0.1f;

    /* Astrocyte modulation */
    nvc_signal *= unit->astrocyte_coupling;

    /* Convolve with HRF for hemodynamic response */
    float hrf_response;
    nimcp_nvc_convolve_hrf(&unit->hrf, unit->activity_history, NVC_HRF_KERNEL_SIZE,
                          &hrf_response);

    /* Calculate target CBF */
    float cbf_increase = 1.0f + hrf_response * (system->config.max_cbf_increase - 1.0f);
    unit->cbf_target = unit->cbf_baseline * nimcp_clampf(cbf_increase, 0.5f,
                                                   system->config.max_cbf_increase);

    /* Smooth CBF transition (time constant ~2s) */
    float tau_cbf = 2.0f;
    unit->cbf += (unit->cbf_target - unit->cbf) * (1.0f - expf(-dt_sec / tau_cbf));

    /* CBV follows CBF with Grubb's exponent */
    float f_ratio = unit->cbf / unit->cbf_baseline;
    float v_ratio = powf(f_ratio, system->config.alpha_grubb);
    unit->cbv = unit->cbv_baseline * nimcp_clampf(v_ratio, 0.8f, system->config.max_cbv_increase);

    /* Update OEF (inverse relationship with CBF for constant CMRO2) */
    if (f_ratio > 0.0f) {
        /* Assume CMRO2 increases slightly with activity */
        float cmro2_ratio = 1.0f + unit->neural_activity * 0.2f;
        unit->oef = system->config.baseline_oef * cmro2_ratio / f_ratio;
        unit->oef = nimcp_clampf(unit->oef, 0.1f, 0.8f);
    }

    /* Update vessel state */
    unit->vessel_diameter = sqrtf(v_ratio);  /* Diameter from volume */
    unit->vessel_tone = 1.0f - (f_ratio - 1.0f) / (system->config.max_cbf_increase - 1.0f);
    unit->vessel_tone = nimcp_clampf(unit->vessel_tone, 0.0f, 1.0f);

    /* Calculate BOLD signal */
    float old_bold = unit->bold.signal;
    unit->bold.cbf_change = f_ratio;
    unit->bold.cbv_change = v_ratio;
    unit->bold.oef = unit->oef;
    unit->bold.deoxyhemoglobin = unit->oef * v_ratio;

    unit->bold.signal = calculate_bold(f_ratio, v_ratio,
                                       system->config.e0, BOLD_V0);

    /* Perfusion callback */
    if (fabsf(unit->cbf - unit->cbf_baseline) > 5.0f) {
        if (system->config.on_perfusion_change) {
            system->config.on_perfusion_change(unit, old_bold, unit->bold.signal,
                                               system->config.callback_data);
        }
    }

    /* BOLD callback */
    if (fabsf(unit->bold.signal - old_bold) > 0.1f) {
        if (system->config.on_bold_update) {
            system->config.on_bold_update(unit, unit->bold.signal,
                                          system->config.callback_data);
        }
    }

    /* Update state */
    unit->state = determine_nvc_state(unit->neural_activity, f_ratio,
                                      unit->time_since_activation);

    /* Decay neural activity */
    unit->neural_activity *= expf(-dt_sec / 0.5f);

    return NVC_OK;
}

nimcp_nvc_error_t nimcp_nvc_update(
    nimcp_nvc_system_t* system,
    float dt
) {
    if (!system) {
        return NVC_ERR_NULL_PTR;
    }

    if (!system->initialized) {
        return NVC_ERR_NOT_INITIALIZED;
    }

    float dt_sec = dt / 1000.0f;

    /* Reset per-update metrics */
    system->metrics.activated_units = 0;
    float total_cbf = 0.0f;
    float total_bold = 0.0f;
    float max_cbf = 0.0f, min_cbf = 1e9f;
    float max_bold = -1e9f, min_bold = 1e9f;

    /* Update all units */
    for (uint32_t i = 0; i < system->num_units; i++) {
        nimcp_nvc_error_t err = nimcp_nvc_update_unit(system, &system->units[i], dt);
        if (err != NVC_OK) {
            return err;
        }

        nimcp_nvc_unit_t* unit = &system->units[i];

        total_cbf += unit->cbf;
        total_bold += unit->bold.signal;

        if (unit->cbf > max_cbf) max_cbf = unit->cbf;
        if (unit->cbf < min_cbf) min_cbf = unit->cbf;
        if (unit->bold.signal > max_bold) max_bold = unit->bold.signal;
        if (unit->bold.signal < min_bold) min_bold = unit->bold.signal;

        if (unit->state == NVC_STATE_ACTIVATED || unit->state == NVC_STATE_PEAK) {
            system->metrics.activated_units++;
        }
    }

    /* Update global values */
    if (system->num_units > 0) {
        system->global_cbf = total_cbf / system->num_units;
        system->global_bold = total_bold / system->num_units;

        system->metrics.mean_cbf = system->global_cbf;
        system->metrics.mean_bold = system->global_bold;
        system->metrics.max_cbf = max_cbf;
        system->metrics.min_cbf = min_cbf;
        system->metrics.max_bold = max_bold;
        system->metrics.min_bold = min_bold;
    }

    /* Update perfusion reserve */
    system->perfusion_reserve = system->config.max_cbf_increase -
                               (system->global_cbf / system->config.baseline_cbf);

    /* Time tracking */
    system->current_time += dt_sec;
    system->metrics.total_simulation_time += dt_sec;
    system->metrics.update_count++;
    system->update_count++;

    return NVC_OK;
}

//=============================================================================
// Metrics API Implementation
//=============================================================================

nimcp_nvc_error_t nimcp_nvc_get_metrics(
    const nimcp_nvc_system_t* system,
    nimcp_nvc_metrics_t* metrics
) {
    if (!system || !metrics) {
        return NVC_ERR_NULL_PTR;
    }

    memcpy(metrics, &system->metrics, sizeof(nimcp_nvc_metrics_t));

    return NVC_OK;
}

nimcp_nvc_state_t nimcp_nvc_get_state(const nimcp_nvc_unit_t* unit) {
    if (!unit) {
        return NVC_STATE_RESTING;
    }
    return unit->state;
}

const char* nimcp_nvc_error_string(nimcp_nvc_error_t error) {
    switch (error) {
        case NVC_OK:
            return "OK";
        case NVC_ERR_NULL_PTR:
            return "Null pointer";
        case NVC_ERR_INVALID_PARAM:
            return "Invalid parameter";
        case NVC_ERR_NOT_INITIALIZED:
            return "Not initialized";
        case NVC_ERR_ALREADY_INITIALIZED:
            return "Already initialized";
        case NVC_ERR_NO_MEMORY:
            return "Out of memory";
        case NVC_ERR_UNIT_NOT_FOUND:
            return "Unit not found";
        case NVC_ERR_CAPACITY_EXCEEDED:
            return "Capacity exceeded";
        case NVC_ERR_HYPOPERFUSION:
            return "Hypoperfusion detected";
        case NVC_ERR_HYPERPERFUSION:
            return "Hyperperfusion detected";
        default:
            return "Unknown error";
    }
}
