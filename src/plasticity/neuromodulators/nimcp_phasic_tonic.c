/**
 * @file nimcp_phasic_tonic.c
 * @brief Implementation of phasic-tonic neuromodulator dynamics (Phase C2.2 Enhancement #2)
 *
 * WHAT: Implements burst vs baseline neurotransmitter release
 * WHY:  Model reward prediction errors and learning signals
 * HOW:  Dual-mode concentration with exponential decay
 *
 * @version Phase C2.2 Enhancement #2
 * @date 2025-11-12
 */

#include "plasticity/neuromodulators/nimcp_phasic_tonic.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/tensor/nimcp_tensor.h"
#include "security/nimcp_security.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>

#define LOG_MODULE "plasticity_phasic_tonic"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(phasic_tonic)

// ============================================================================
// Tensor State Management
// ============================================================================

phasic_tonic_state_t phasic_tonic_state_create(void) {
    phasic_tonic_state_t state = {0};

    /* Create tensors for each parameter group */
    uint32_t tonic_dims[1] = {5};
    uint32_t phasic_dims[1] = {4};
    uint32_t limits_dims[1] = {2};
    uint32_t auto_dims[1] = {2};
    uint32_t output_dims[1] = {2};

    state.tonic_params = nimcp_tensor_zeros(tonic_dims, 1, NIMCP_DTYPE_F32);
    state.phasic_params = nimcp_tensor_zeros(phasic_dims, 1, NIMCP_DTYPE_F32);
    state.burst_limits = nimcp_tensor_zeros(limits_dims, 1, NIMCP_DTYPE_F32);
    state.autoreceptor_params = nimcp_tensor_zeros(auto_dims, 1, NIMCP_DTYPE_F32);
    state.output = nimcp_tensor_zeros(output_dims, 1, NIMCP_DTYPE_F32);

    state.in_burst_state = false;
    state.burst_start_time_us = 0;
    state.burst_duration_ms = 0;
    state.burst_count = 0;
    state.last_burst_time_us = 0;
    state.avg_inter_burst_interval = 0.0f;
    state.owns_tensors = true;

    return state;
}

void phasic_tonic_state_destroy(phasic_tonic_state_t* state) {
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "phasic_tonic_state_destroy: null state");
        return;
    }
    if (!state->owns_tensors) return;

    if (state->tonic_params) {
        nimcp_tensor_destroy(state->tonic_params);
        state->tonic_params = NULL;
    }
    if (state->phasic_params) {
        nimcp_tensor_destroy(state->phasic_params);
        state->phasic_params = NULL;
    }
    if (state->burst_limits) {
        nimcp_tensor_destroy(state->burst_limits);
        state->burst_limits = NULL;
    }
    if (state->autoreceptor_params) {
        nimcp_tensor_destroy(state->autoreceptor_params);
        state->autoreceptor_params = NULL;
    }
    if (state->output) {
        nimcp_tensor_destroy(state->output);
        state->output = NULL;
    }
}

// ============================================================================
// Backward Compatibility Accessors - Tonic Parameters
// ============================================================================

float phasic_tonic_get_tonic_level(const phasic_tonic_state_t* state) {
    if (!state || !state->tonic_params) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "phasic_tonic_get_tonic_level: null state or tensor");
        return 0.0f;
    }
    uint32_t idx[1] = {PHASIC_TONIC_IDX_TONIC_LEVEL};
    return (float)nimcp_tensor_get(state->tonic_params, idx);
}

float phasic_tonic_get_tonic_target(const phasic_tonic_state_t* state) {
    if (!state || !state->tonic_params) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "phasic_tonic_get_tonic_target: null state or tensor");
        return 0.0f;
    }
    uint32_t idx[1] = {PHASIC_TONIC_IDX_TONIC_TARGET};
    return (float)nimcp_tensor_get(state->tonic_params, idx);
}

float phasic_tonic_get_tonic_min(const phasic_tonic_state_t* state) {
    if (!state || !state->tonic_params) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "phasic_tonic_get_tonic_min: null state or tensor");
        return 0.0f;
    }
    uint32_t idx[1] = {PHASIC_TONIC_IDX_TONIC_MIN};
    return (float)nimcp_tensor_get(state->tonic_params, idx);
}

float phasic_tonic_get_tonic_max(const phasic_tonic_state_t* state) {
    if (!state || !state->tonic_params) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "phasic_tonic_get_tonic_max: null state or tensor");
        return 0.0f;
    }
    uint32_t idx[1] = {PHASIC_TONIC_IDX_TONIC_MAX};
    return (float)nimcp_tensor_get(state->tonic_params, idx);
}

float phasic_tonic_get_homeostatic_tau(const phasic_tonic_state_t* state) {
    if (!state || !state->tonic_params) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "phasic_tonic_get_homeostatic_tau: null state or tensor");
        return 0.0f;
    }
    uint32_t idx[1] = {PHASIC_TONIC_IDX_HOMEOSTATIC_TAU};
    return (float)nimcp_tensor_get(state->tonic_params, idx);
}

void phasic_tonic_set_tonic_level(phasic_tonic_state_t* state, float value) {
    if (!state || !state->tonic_params) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "phasic_tonic_set_tonic_level: null state or tensor");
        return;
    }
    uint32_t idx[1] = {PHASIC_TONIC_IDX_TONIC_LEVEL};
    nimcp_tensor_set(state->tonic_params, idx, value);
}

// ============================================================================
// Backward Compatibility Accessors - Phasic Parameters
// ============================================================================

float phasic_tonic_get_phasic_burst(const phasic_tonic_state_t* state) {
    if (!state || !state->phasic_params) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "phasic_tonic_get_phasic_burst: null state or tensor");
        return 0.0f;
    }
    uint32_t idx[1] = {PHASIC_TONIC_IDX_PHASIC_BURST};
    return (float)nimcp_tensor_get(state->phasic_params, idx);
}

float phasic_tonic_get_burst_decay_tau(const phasic_tonic_state_t* state) {
    if (!state || !state->phasic_params) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "phasic_tonic_get_burst_decay_tau: null state or tensor");
        return 0.0f;
    }
    uint32_t idx[1] = {PHASIC_TONIC_IDX_BURST_DECAY_TAU};
    return (float)nimcp_tensor_get(state->phasic_params, idx);
}

float phasic_tonic_get_burst_amplitude_scale(const phasic_tonic_state_t* state) {
    if (!state || !state->phasic_params) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "phasic_tonic_get_burst_amplitude_scale: null state or tensor");
        return 0.0f;
    }
    uint32_t idx[1] = {PHASIC_TONIC_IDX_BURST_AMP_SCALE};
    return (float)nimcp_tensor_get(state->phasic_params, idx);
}

float phasic_tonic_get_burst_threshold(const phasic_tonic_state_t* state) {
    if (!state || !state->phasic_params) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "phasic_tonic_get_burst_threshold: null state or tensor");
        return 0.0f;
    }
    uint32_t idx[1] = {PHASIC_TONIC_IDX_BURST_THRESHOLD};
    return (float)nimcp_tensor_get(state->phasic_params, idx);
}

float phasic_tonic_get_max_burst_amplitude(const phasic_tonic_state_t* state) {
    if (!state || !state->burst_limits) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "phasic_tonic_get_max_burst_amplitude: null state or tensor");
        return 0.0f;
    }
    uint32_t idx[1] = {PHASIC_TONIC_IDX_MAX_BURST_AMP};
    return (float)nimcp_tensor_get(state->burst_limits, idx);
}

float phasic_tonic_get_min_burst_amplitude(const phasic_tonic_state_t* state) {
    if (!state || !state->burst_limits) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "phasic_tonic_get_min_burst_amplitude: null state or tensor");
        return 0.0f;
    }
    uint32_t idx[1] = {PHASIC_TONIC_IDX_MIN_BURST_AMP};
    return (float)nimcp_tensor_get(state->burst_limits, idx);
}

void phasic_tonic_set_phasic_burst(phasic_tonic_state_t* state, float value) {
    if (!state || !state->phasic_params) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "phasic_tonic_set_phasic_burst: null state or tensor");
        return;
    }
    uint32_t idx[1] = {PHASIC_TONIC_IDX_PHASIC_BURST};
    nimcp_tensor_set(state->phasic_params, idx, value);
}

// ============================================================================
// Backward Compatibility Accessors - Autoreceptor Parameters
// ============================================================================

float phasic_tonic_get_autoreceptor_sensitivity(const phasic_tonic_state_t* state) {
    if (!state || !state->autoreceptor_params) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "phasic_tonic_get_autoreceptor_sensitivity: null state or tensor");
        return 0.0f;
    }
    uint32_t idx[1] = {PHASIC_TONIC_IDX_AUTO_SENSITIVITY};
    return (float)nimcp_tensor_get(state->autoreceptor_params, idx);
}

float phasic_tonic_get_feedback_tau(const phasic_tonic_state_t* state) {
    if (!state || !state->autoreceptor_params) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "phasic_tonic_get_feedback_tau: null state or tensor");
        return 0.0f;
    }
    uint32_t idx[1] = {PHASIC_TONIC_IDX_FEEDBACK_TAU};
    return (float)nimcp_tensor_get(state->autoreceptor_params, idx);
}

// ============================================================================
// Backward Compatibility Accessors - Output Parameters
// ============================================================================

float phasic_tonic_get_total_concentration(const phasic_tonic_state_t* state) {
    if (!state || !state->output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "phasic_tonic_get_total_concentration: null state or tensor");
        return 0.0f;
    }
    uint32_t idx[1] = {PHASIC_TONIC_IDX_TOTAL_CONC};
    return (float)nimcp_tensor_get(state->output, idx);
}

float phasic_tonic_get_release_rate_value(const phasic_tonic_state_t* state) {
    if (!state || !state->output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "phasic_tonic_get_release_rate_value: null state or tensor");
        return 0.0f;
    }
    uint32_t idx[1] = {PHASIC_TONIC_IDX_RELEASE_RATE};
    return (float)nimcp_tensor_get(state->output, idx);
}

void phasic_tonic_set_total_concentration(phasic_tonic_state_t* state, float value) {
    if (!state || !state->output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "phasic_tonic_set_total_concentration: null state or tensor");
        return;
    }
    uint32_t idx[1] = {PHASIC_TONIC_IDX_TOTAL_CONC};
    nimcp_tensor_set(state->output, idx, value);
}

void phasic_tonic_set_release_rate_value(phasic_tonic_state_t* state, float value) {
    if (!state || !state->output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "phasic_tonic_set_release_rate_value: null state or tensor");
        return;
    }
    uint32_t idx[1] = {PHASIC_TONIC_IDX_RELEASE_RATE};
    nimcp_tensor_set(state->output, idx, value);
}

// ============================================================================
// Default Configurations
// ============================================================================

phasic_tonic_config_t phasic_tonic_config_dopamine_default(void) {
    phasic_tonic_config_t config = {
        .initial_tonic = DOPAMINE_TONIC_BASELINE,
        .tonic_target = DOPAMINE_TONIC_BASELINE,
        .tonic_range_min = DOPAMINE_TONIC_RANGE_MIN,
        .tonic_range_max = DOPAMINE_TONIC_RANGE_MAX,
        .homeostatic_tau = DOPAMINE_HOMEOSTATIC_TAU,

        .burst_decay_tau = DOPAMINE_BURST_DECAY_TAU,
        .burst_amplitude_scale = 1.0F,
        .max_burst_amplitude = DOPAMINE_PHASIC_PEAK,

        .autoreceptor_sensitivity = 0.3F,  // Moderate D2 autoreceptor feedback
        .feedback_tau = 1.0F
    };
    return config;
}

phasic_tonic_config_t phasic_tonic_config_serotonin_default(void) {
    phasic_tonic_config_t config = {
        .initial_tonic = SEROTONIN_TONIC_BASELINE,
        .tonic_target = SEROTONIN_TONIC_BASELINE,
        .tonic_range_min = SEROTONIN_TONIC_BASELINE * 0.5F,
        .tonic_range_max = SEROTONIN_TONIC_BASELINE * 2.0F,
        .homeostatic_tau = 120.0F,  // Slower than dopamine

        .burst_decay_tau = SEROTONIN_BURST_DECAY_TAU,
        .burst_amplitude_scale = 0.8F,
        .max_burst_amplitude = SEROTONIN_TONIC_BASELINE * 10.0F,

        .autoreceptor_sensitivity = 0.5F,  // Strong 5-HT1A autoreceptor
        .feedback_tau = 2.0F
    };
    return config;
}

phasic_tonic_config_t phasic_tonic_config_norepinephrine_default(void) {
    phasic_tonic_config_t config = {
        .initial_tonic = NOREPINEPHRINE_TONIC_BASELINE,
        .tonic_target = NOREPINEPHRINE_TONIC_BASELINE,
        .tonic_range_min = NOREPINEPHRINE_TONIC_BASELINE * 0.3F,
        .tonic_range_max = NOREPINEPHRINE_TONIC_BASELINE * 3.0F,
        .homeostatic_tau = 30.0F,  // Fast arousal regulation

        .burst_decay_tau = NOREPINEPHRINE_BURST_DECAY_TAU,
        .burst_amplitude_scale = 1.2F,  // Strong bursts for salience
        .max_burst_amplitude = NOREPINEPHRINE_TONIC_BASELINE * 20.0F,

        .autoreceptor_sensitivity = 0.4F,  // α2 autoreceptor
        .feedback_tau = 0.5F
    };
    return config;
}

// ============================================================================
// Initialization
// ============================================================================

void phasic_tonic_init(
    phasic_tonic_state_t* state,
    const phasic_tonic_config_t* config,
    uint64_t current_time_us
) {
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "phasic_tonic_init: null state");
        return;
    }
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "phasic_tonic_init: null config");
        return;
    }

    /* Create tensor storage if not already allocated */
    if (!state->tonic_params) {
        *state = phasic_tonic_state_create();
    }

    uint32_t idx[1];

    /* Tonic initialization */
    idx[0] = PHASIC_TONIC_IDX_TONIC_LEVEL;
    nimcp_tensor_set(state->tonic_params, idx, config->initial_tonic);
    idx[0] = PHASIC_TONIC_IDX_TONIC_TARGET;
    nimcp_tensor_set(state->tonic_params, idx, config->tonic_target);
    idx[0] = PHASIC_TONIC_IDX_TONIC_MIN;
    nimcp_tensor_set(state->tonic_params, idx, config->tonic_range_min);
    idx[0] = PHASIC_TONIC_IDX_TONIC_MAX;
    nimcp_tensor_set(state->tonic_params, idx, config->tonic_range_max);
    idx[0] = PHASIC_TONIC_IDX_HOMEOSTATIC_TAU;
    nimcp_tensor_set(state->tonic_params, idx, config->homeostatic_tau);

    /* Phasic initialization */
    idx[0] = PHASIC_TONIC_IDX_PHASIC_BURST;
    nimcp_tensor_set(state->phasic_params, idx, 0.0F);
    idx[0] = PHASIC_TONIC_IDX_BURST_DECAY_TAU;
    nimcp_tensor_set(state->phasic_params, idx, config->burst_decay_tau);
    idx[0] = PHASIC_TONIC_IDX_BURST_AMP_SCALE;
    nimcp_tensor_set(state->phasic_params, idx, config->burst_amplitude_scale);
    idx[0] = PHASIC_TONIC_IDX_BURST_THRESHOLD;
    nimcp_tensor_set(state->phasic_params, idx, config->tonic_target * 0.1F);

    state->in_burst_state = false;
    state->burst_start_time_us = 0;
    state->burst_duration_ms = DOPAMINE_BURST_DURATION_MS;  /* Default */

    /* Burst limits */
    idx[0] = PHASIC_TONIC_IDX_MAX_BURST_AMP;
    nimcp_tensor_set(state->burst_limits, idx, config->max_burst_amplitude);
    idx[0] = PHASIC_TONIC_IDX_MIN_BURST_AMP;
    nimcp_tensor_set(state->burst_limits, idx, config->tonic_target * 0.5F);

    /* Autoreceptor */
    idx[0] = PHASIC_TONIC_IDX_AUTO_SENSITIVITY;
    nimcp_tensor_set(state->autoreceptor_params, idx, config->autoreceptor_sensitivity);
    idx[0] = PHASIC_TONIC_IDX_FEEDBACK_TAU;
    nimcp_tensor_set(state->autoreceptor_params, idx, config->feedback_tau);

    /* Output */
    idx[0] = PHASIC_TONIC_IDX_TOTAL_CONC;
    nimcp_tensor_set(state->output, idx, config->initial_tonic);
    idx[0] = PHASIC_TONIC_IDX_RELEASE_RATE;
    nimcp_tensor_set(state->output, idx, 0.0F);

    /* Statistics */
    state->burst_count = 0;
    state->last_burst_time_us = current_time_us;
    state->avg_inter_burst_interval = 0.0F;
}

// ============================================================================
// Core Update Function
// ============================================================================

void phasic_tonic_update(
    phasic_tonic_state_t* state,
    float dt,
    uint64_t current_time_us
) {
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "phasic_tonic_update: null state");
        return;
    }

    /* Get current values from tensors using accessors */
    float tonic_level = phasic_tonic_get_tonic_level(state);
    float tonic_target = phasic_tonic_get_tonic_target(state);
    float tonic_min = phasic_tonic_get_tonic_min(state);
    float tonic_max = phasic_tonic_get_tonic_max(state);
    float homeostatic_tau = phasic_tonic_get_homeostatic_tau(state);
    float phasic_burst = phasic_tonic_get_phasic_burst(state);
    float burst_decay_tau = phasic_tonic_get_burst_decay_tau(state);
    float burst_threshold = phasic_tonic_get_burst_threshold(state);
    float autoreceptor_sensitivity = phasic_tonic_get_autoreceptor_sensitivity(state);

    /* === 1. Update Homeostatic Tonic Regulation === */
    float tonic_alpha = expf(-dt / homeostatic_tau);
    tonic_level = tonic_alpha * tonic_level + (1.0F - tonic_alpha) * tonic_target;

    /* Clamp tonic to physiological range */
    if (tonic_level < tonic_min) {
        tonic_level = tonic_min;
    } else if (tonic_level > tonic_max) {
        tonic_level = tonic_max;
    }
    phasic_tonic_set_tonic_level(state, tonic_level);

    /* === 2. Update Phasic Burst Decay === */
    if (state->in_burst_state) {
        /* Exponential decay */
        float decay_alpha = expf(-dt / burst_decay_tau);
        phasic_burst *= decay_alpha;

        /* Check burst termination */
        uint64_t elapsed_us = current_time_us - state->burst_start_time_us;
        uint64_t duration_us = (uint64_t)state->burst_duration_ms * 1000ULL;

        if (elapsed_us >= duration_us || phasic_burst < burst_threshold) {
            state->in_burst_state = false;
            phasic_burst = 0.0F;
        }
        phasic_tonic_set_phasic_burst(state, phasic_burst);
    }

    /* === 3. Apply Autoreceptor Feedback === */
    /* High concentration -> negative feedback -> reduced release */
    float total = tonic_level + phasic_burst;
    float feedback_factor = 1.0F / (1.0F + autoreceptor_sensitivity * total);

    /* === 4. Compute Release Rate === */
    /* Release rate proportional to concentration and feedback */
    float release_rate = total * feedback_factor / dt;
    phasic_tonic_set_release_rate_value(state, release_rate);

    /* === 5. Update Total Concentration === */
    phasic_tonic_set_total_concentration(state, total);
}

// ============================================================================
// Burst Triggering
// ============================================================================

bool phasic_tonic_trigger_burst(
    phasic_tonic_state_t* state,
    float amplitude,
    uint32_t duration_ms,
    uint64_t current_time_us
) {
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "phasic_tonic_trigger_burst: null state");
        return false;
    }

    float min_burst_amp = phasic_tonic_get_min_burst_amplitude(state);
    float max_burst_amp = phasic_tonic_get_max_burst_amplitude(state);
    float burst_amp_scale = phasic_tonic_get_burst_amplitude_scale(state);
    float phasic_burst = phasic_tonic_get_phasic_burst(state);
    float tonic_level = phasic_tonic_get_tonic_level(state);

    /* Check minimum amplitude threshold */
    if (amplitude < min_burst_amp) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "phasic_tonic_trigger_burst: validation failed");
        return false;  /* Too weak to trigger */
    }

    /* Clamp amplitude to maximum */
    if (amplitude > max_burst_amp) {
        amplitude = max_burst_amp;
    }

    /* Apply amplitude scaling */
    amplitude *= burst_amp_scale;

    /* If already bursting, add to current burst (superposition) */
    if (state->in_burst_state) {
        phasic_burst += amplitude;
        if (phasic_burst > max_burst_amp) {
            phasic_burst = max_burst_amp;
        }
        phasic_tonic_set_phasic_burst(state, phasic_burst);
        /* Don't reset timing - let current burst continue */
    } else {
        /* Start new burst */
        phasic_burst = amplitude;
        phasic_tonic_set_phasic_burst(state, phasic_burst);
        state->in_burst_state = true;
        state->burst_start_time_us = current_time_us;
        state->burst_duration_ms = (duration_ms > 0) ? duration_ms : DOPAMINE_BURST_DURATION_MS;

        /* Update statistics */
        state->burst_count++;

        /* Update average inter-burst interval */
        if (state->burst_count > 1) {
            float interval = (current_time_us - state->last_burst_time_us) / 1000000.0F;
            float alpha = 0.1F;  /* Exponential moving average */
            state->avg_inter_burst_interval = alpha * interval +
                                             (1.0F - alpha) * state->avg_inter_burst_interval;
        }

        state->last_burst_time_us = current_time_us;
    }

    /* Update total concentration immediately so it reflects the burst */
    phasic_tonic_set_total_concentration(state, tonic_level + phasic_burst);

    return true;
}

void phasic_tonic_induce_dip(
    phasic_tonic_state_t* state,
    float magnitude
) {
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "phasic_tonic_induce_dip: null state");
        return;
    }

    /* Clamp magnitude */
    if (magnitude < 0.0F) magnitude = 0.0F;
    if (magnitude > 1.0F) magnitude = 1.0F;

    float tonic_level = phasic_tonic_get_tonic_level(state);
    float tonic_min = phasic_tonic_get_tonic_min(state);
    float phasic_burst = phasic_tonic_get_phasic_burst(state);

    /* Temporarily reduce tonic level */
    float reduction = tonic_level * magnitude;
    tonic_level -= reduction;

    /* Ensure doesn't go below minimum */
    if (tonic_level < tonic_min) {
        tonic_level = tonic_min;
    }
    phasic_tonic_set_tonic_level(state, tonic_level);

    /* Update total concentration immediately */
    phasic_tonic_set_total_concentration(state, tonic_level + phasic_burst);
}

// ============================================================================
// TD Error Encoding
// ============================================================================

bool phasic_tonic_encode_td_error(
    phasic_tonic_state_t* state,
    float td_error,
    uint64_t current_time_us
) {
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "phasic_tonic_encode_td_error: null state");
        return false;
    }

    float max_burst_amp = phasic_tonic_get_max_burst_amplitude(state);

    if (td_error > 0.0F) {
        /* Positive TD error -> phasic burst */
        /* Scale burst amplitude proportional to error magnitude */
        float burst_amplitude = td_error * max_burst_amp;
        return phasic_tonic_trigger_burst(state, burst_amplitude, 0, current_time_us);

    } else if (td_error < 0.0F) {
        /* Negative TD error -> tonic dip */
        float dip_magnitude = fabsf(td_error);  /* Convert to positive */
        phasic_tonic_induce_dip(state, dip_magnitude * 0.5F);  /* 50% scaling */
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "phasic_tonic_encode_td_error: operation failed");
        return false;

    } else {
        // Zero TD error → no change (expected outcome)
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "phasic_tonic_encode_td_error: operation failed");
        return false;
    }
}

// ============================================================================
// Accessors
// ============================================================================

float phasic_tonic_get_concentration(const phasic_tonic_state_t* state) {
    return phasic_tonic_get_total_concentration(state);
}

float phasic_tonic_get_release_rate(const phasic_tonic_state_t* state) {
    return phasic_tonic_get_release_rate_value(state);
}

void phasic_tonic_set_tonic_target(
    phasic_tonic_state_t* state,
    float new_target
) {
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "phasic_tonic_set_tonic_target: null state");
        return;
    }

    float tonic_min = phasic_tonic_get_tonic_min(state);
    float tonic_max = phasic_tonic_get_tonic_max(state);

    /* Clamp to physiological range */
    if (new_target < tonic_min) {
        new_target = tonic_min;
    } else if (new_target > tonic_max) {
        new_target = tonic_max;
    }

    uint32_t idx[1] = {PHASIC_TONIC_IDX_TONIC_TARGET};
    nimcp_tensor_set(state->tonic_params, idx, new_target);
}

void phasic_tonic_apply_autoreceptor_modulation(
    phasic_tonic_state_t* state,
    float modulation
) {
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "phasic_tonic_apply_autoreceptor_modulation: null state");
        return;
    }

    /* Clamp modulation to reasonable range */
    if (modulation < 0.0F) modulation = 0.0F;
    if (modulation > 2.0F) modulation = 2.0F;

    float tonic_level = phasic_tonic_get_tonic_level(state);
    float tonic_min = phasic_tonic_get_tonic_min(state);
    float tonic_max = phasic_tonic_get_tonic_max(state);

    /* Apply to current tonic level (immediate effect) */
    tonic_level *= modulation;

    /* Clamp to range */
    if (tonic_level < tonic_min) {
        tonic_level = tonic_min;
    } else if (tonic_level > tonic_max) {
        tonic_level = tonic_max;
    }
    phasic_tonic_set_tonic_level(state, tonic_level);
}

// ============================================================================
// Statistics and Monitoring
// ============================================================================

void phasic_tonic_get_burst_statistics(
    const phasic_tonic_state_t* state,
    uint32_t* burst_count,
    float* avg_interval,
    float* time_since_last,
    uint64_t current_time_us
) {
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "phasic_tonic_get_burst_statistics: null state");
        return;
    }

    if (burst_count) {
        *burst_count = state->burst_count;
    }

    if (avg_interval) {
        *avg_interval = state->avg_inter_burst_interval;
    }

    if (time_since_last) {
        uint64_t elapsed_us = current_time_us - state->last_burst_time_us;
        *time_since_last = elapsed_us / 1000000.0F;  /* Convert to seconds */
    }
}

void phasic_tonic_reset(
    phasic_tonic_state_t* state,
    uint64_t current_time_us
) {
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "phasic_tonic_reset: null state");
        return;
    }

    float tonic_target = phasic_tonic_get_tonic_target(state);

    /* Reset to baseline */
    phasic_tonic_set_tonic_level(state, tonic_target);
    phasic_tonic_set_phasic_burst(state, 0.0F);
    state->in_burst_state = false;
    phasic_tonic_set_total_concentration(state, tonic_target);
    phasic_tonic_set_release_rate_value(state, 0.0F);

    /* Reset statistics */
    state->burst_count = 0;
    state->last_burst_time_us = current_time_us;
    state->avg_inter_burst_interval = 0.0F;
}
