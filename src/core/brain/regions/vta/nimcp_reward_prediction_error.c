/**
 * @file nimcp_reward_prediction_error.c
 * @brief Reward Prediction Error (RPE) implementation
 * @date 2026-01-11
 */

#include "core/brain/regions/vta/nimcp_reward_prediction_error.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE_MESH_ONLY(reward_prediction_error, MESH_ADAPTER_CATEGORY_COGNITIVE)

/*=============================================================================
 * Lifecycle API
 *===========================================================================*/

nimcp_rpe_config_t nimcp_rpe_default_config(void) {
    nimcp_rpe_config_t config;
    memset(&config, 0, sizeof(config));

    config.alpha = RPE_DEFAULT_ALPHA;
    config.gamma = RPE_DEFAULT_GAMMA;
    config.lambda = RPE_DEFAULT_LAMBDA;
    config.burst_threshold = RPE_BURST_THRESHOLD;
    config.pause_threshold = RPE_PAUSE_THRESHOLD;
    config.algorithm = RPE_ALGO_TD0;
    config.use_eligibility_traces = false;
    config.normalize_rpe = false;
    config.rpe_scale = 1.0f;

    return config;
}

int nimcp_rpe_init(nimcp_rpe_system_t* system, const nimcp_rpe_config_t* config) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "system is NULL");

        return -1;
    }

    memset(system, 0, sizeof(*system));

    if (config) {
        system->config = *config;
    } else {
        system->config = nimcp_rpe_default_config();
    }

    /* Initialize state values */
    for (int i = 0; i < RPE_MAX_STATES; i++) {
        system->state_values[i] = 0.0f;
        system->eligibility_traces[i] = 0.0f;
    }

    system->rpe_variance = 1.0f;  /* Avoid division by zero */
    system->initialized = true;

    return 0;
}

int nimcp_rpe_shutdown(nimcp_rpe_system_t* system) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "system is NULL");

        return -1;
    }

    memset(system, 0, sizeof(*system));
    return 0;
}

int nimcp_rpe_reset(nimcp_rpe_system_t* system) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "system is NULL");

        return -1;
    }

    nimcp_rpe_config_t config = system->config;
    nimcp_rpe_shutdown(system);
    return nimcp_rpe_init(system, &config);
}

/*=============================================================================
 * Core RPE API
 *===========================================================================*/

int nimcp_rpe_compute(
    nimcp_rpe_system_t* system,
    float reward,
    nimcp_rpe_result_t* result
) {
    if (!system || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_rpe_compute: required parameter is NULL (system, result)");
        return -1;
    }

    if (!system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_rpe_compute: system->initialized is NULL");
        return -1;
    }

    memset(result, 0, sizeof(*result));

    /* Get expected value */
    float expected = system->state_values[system->current_state];

    /* TD error: δ = r + γ*V(s') - V(s) */
    float next_value = 0.0f;
    if (system->current_state < RPE_MAX_STATES - 1) {
        next_value = system->state_values[system->current_state + 1];
    }

    float rpe = reward + system->config.gamma * next_value - expected;

    /* Normalize if configured */
    if (system->config.normalize_rpe && system->rpe_variance > 0.001f) {
        rpe /= sqrtf(system->rpe_variance);
    }

    /* Scale RPE */
    rpe *= system->config.rpe_scale;

    /* Store result */
    result->rpe = rpe;
    result->expected = expected;
    result->actual = reward;
    result->magnitude = fabsf(rpe);

    /* Classify RPE type */
    if (rpe > system->config.burst_threshold) {
        result->type = RPE_TYPE_POSITIVE;
        result->triggers_burst = true;
    } else if (rpe < system->config.pause_threshold) {
        result->type = RPE_TYPE_NEGATIVE;
        result->triggers_pause = true;
    } else {
        result->type = RPE_TYPE_NONE;
    }

    /* Map to DA response */
    result->da_response = nimcp_clampf(rpe, -1.0f, 1.0f);

    /* Update system state */
    system->current_rpe = rpe;
    system->last_result = *result;
    system->last_reward = reward;
    system->cumulative_reward += reward;
    system->reward_count++;
    system->average_reward = system->cumulative_reward / system->reward_count;

    /* Update RPE statistics for normalization */
    float delta = rpe - system->rpe_running_mean;
    system->rpe_running_mean += delta / (system->update_count + 1);
    float delta2 = rpe - system->rpe_running_mean;
    system->rpe_variance += (delta * delta2 - system->rpe_variance) / (system->update_count + 1);

    /* Update metrics */
    system->update_count++;
    if (rpe > 0) {
        system->positive_rpe_count++;
    } else if (rpe < 0) {
        system->negative_rpe_count++;
    }

    return 0;
}

int nimcp_rpe_learn(nimcp_rpe_system_t* system, float rpe) {
    if (!system || !system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_rpe_learn: required parameter is NULL (system, system->initialized)");
        return -1;
    }

    float alpha = system->config.alpha;

    if (system->config.use_eligibility_traces) {
        /* TD(lambda) learning */
        for (int i = 0; i < RPE_MAX_STATES; i++) {
            if (system->eligibility_traces[i] > 0.001f) {
                system->state_values[i] += alpha * rpe * system->eligibility_traces[i];
            }
        }
    } else {
        /* TD(0) learning - update current state only */
        system->state_values[system->current_state] += alpha * rpe;
    }

    system->total_learning += fabsf(alpha * rpe);
    return 0;
}

int nimcp_rpe_get_expectation(
    nimcp_rpe_system_t* system,
    float* expected
) {
    if (!system || !expected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_rpe_get_expectation: required parameter is NULL (system, expected)");
        return -1;
    }

    if (!system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_rpe_get_expectation: system->initialized is NULL");
        return -1;
    }

    *expected = system->state_values[system->current_state];
    return 0;
}

int nimcp_rpe_set_expectation(
    nimcp_rpe_system_t* system,
    float expected
) {
    if (!system || !system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_rpe_set_expectation: required parameter is NULL (system, system->initialized)");
        return -1;
    }

    system->state_values[system->current_state] = expected;
    return 0;
}

/*=============================================================================
 * State Transition API
 *===========================================================================*/

int nimcp_rpe_transition_state(
    nimcp_rpe_system_t* system,
    uint32_t new_state
) {
    if (!system || !system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_rpe_transition_state: required parameter is NULL (system, system->initialized)");
        return -1;
    }

    if (new_state >= RPE_MAX_STATES) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "nimcp_rpe_transition_state: capacity exceeded");
        return -1;
    }

    system->previous_state = system->current_state;
    system->current_state = new_state;

    /* Update eligibility traces */
    if (system->config.use_eligibility_traces) {
        /* Decay all traces */
        for (int i = 0; i < RPE_MAX_STATES; i++) {
            system->eligibility_traces[i] *= system->config.gamma * system->config.lambda;
        }
        /* Set current state trace to 1 */
        system->eligibility_traces[new_state] = 1.0f;
    }

    return 0;
}

int nimcp_rpe_get_state_value(
    nimcp_rpe_system_t* system,
    uint32_t state,
    float* value
) {
    if (!system || !value) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_rpe_get_state_value: required parameter is NULL (system, value)");
        return -1;
    }

    if (!system->initialized || state >= RPE_MAX_STATES) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_rpe_get_state_value: system->initialized is NULL");
        return -1;
    }

    *value = system->state_values[state];
    return 0;
}

int nimcp_rpe_set_state_value(
    nimcp_rpe_system_t* system,
    uint32_t state,
    float value
) {
    if (!system || !system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_rpe_set_state_value: required parameter is NULL (system, system->initialized)");
        return -1;
    }

    if (state >= RPE_MAX_STATES) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "nimcp_rpe_set_state_value: capacity exceeded");
        return -1;
    }

    system->state_values[state] = value;
    return 0;
}

/*=============================================================================
 * Cue API
 *===========================================================================*/

int nimcp_rpe_add_cue(
    nimcp_rpe_system_t* system,
    float initial_value,
    uint32_t* cue_id
) {
    if (!system || !cue_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_rpe_add_cue: required parameter is NULL (system, cue_id)");
        return -1;
    }

    if (!system->initialized || system->num_cues >= RPE_MAX_CUES) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_rpe_add_cue: system->initialized is NULL");
        return -1;
    }

    nimcp_reward_cue_t* cue = &system->cues[system->num_cues];
    memset(cue, 0, sizeof(*cue));

    cue->id = system->num_cues;
    cue->predictive_value = initial_value;
    cue->eligibility = 0.0f;
    cue->delay = 1000.0f;  /* Default 1 second */

    *cue_id = cue->id;
    system->num_cues++;

    return 0;
}

int nimcp_rpe_cue_onset(
    nimcp_rpe_system_t* system,
    uint32_t cue_id,
    float delay_hint
) {
    if (!system || !system->initialized || cue_id >= system->num_cues) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_rpe_cue_onset: required parameter is NULL (system, system->initialized)");
        return -1;
    }

    nimcp_reward_cue_t* cue = &system->cues[cue_id];
    cue->active = true;
    cue->onset_time = 0.0f;  /* Reset timing */
    cue->eligibility = 1.0f;

    if (delay_hint > 0.0f) {
        cue->delay = delay_hint;
    }

    cue->exposure_count++;
    return 0;
}

int nimcp_rpe_cue_offset(
    nimcp_rpe_system_t* system,
    uint32_t cue_id
) {
    if (!system || !system->initialized || cue_id >= system->num_cues) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_rpe_cue_offset: required parameter is NULL (system, system->initialized)");
        return -1;
    }

    system->cues[cue_id].active = false;
    return 0;
}

int nimcp_rpe_get_cue_value(
    nimcp_rpe_system_t* system,
    uint32_t cue_id,
    float* value
) {
    if (!system || !value) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_rpe_get_cue_value: required parameter is NULL (system, value)");
        return -1;
    }

    if (!system->initialized || cue_id >= system->num_cues) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_rpe_get_cue_value: system->initialized is NULL");
        return -1;
    }

    *value = system->cues[cue_id].predictive_value;
    return 0;
}

int nimcp_rpe_update_cue_learning(
    nimcp_rpe_system_t* system,
    float actual_reward
) {
    if (!system || !system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_rpe_update_cue_learning: required parameter is NULL (system, system->initialized)");
        return -1;
    }

    float alpha = system->config.alpha;

    for (uint32_t i = 0; i < system->num_cues; i++) {
        nimcp_reward_cue_t* cue = &system->cues[i];
        if (cue->eligibility > 0.001f) {
            float error = actual_reward - cue->predictive_value;
            cue->predictive_value += alpha * error * cue->eligibility;
            cue->predictive_value = nimcp_clampf(cue->predictive_value, -10.0f, 10.0f);
        }
    }

    return 0;
}

/*=============================================================================
 * Eligibility Trace API
 *===========================================================================*/

int nimcp_rpe_update_traces(nimcp_rpe_system_t* system, float dt) {
    if (!system || !system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_rpe_update_traces: required parameter is NULL (system, system->initialized)");
        return -1;
    }

    float decay = expf(-dt / (1000.0f * system->config.lambda));

    for (int i = 0; i < RPE_MAX_STATES; i++) {
        system->eligibility_traces[i] *= decay;
    }

    /* Decay cue eligibilities */
    for (uint32_t i = 0; i < system->num_cues; i++) {
        if (system->cues[i].active) {
            system->cues[i].onset_time += dt;
        } else {
            system->cues[i].eligibility *= decay;
        }
    }

    return 0;
}

int nimcp_rpe_reset_traces(nimcp_rpe_system_t* system) {
    if (!system || !system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_rpe_reset_traces: required parameter is NULL (system, system->initialized)");
        return -1;
    }

    for (int i = 0; i < RPE_MAX_STATES; i++) {
        system->eligibility_traces[i] = 0.0f;
    }

    for (uint32_t i = 0; i < system->num_cues; i++) {
        system->cues[i].eligibility = 0.0f;
    }

    return 0;
}

int nimcp_rpe_get_eligibility(
    nimcp_rpe_system_t* system,
    uint32_t state,
    float* eligibility
) {
    if (!system || !eligibility) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_rpe_get_eligibility: required parameter is NULL (system, eligibility)");
        return -1;
    }

    if (!system->initialized || state >= RPE_MAX_STATES) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_rpe_get_eligibility: system->initialized is NULL");
        return -1;
    }

    *eligibility = system->eligibility_traces[state];
    return 0;
}

/*=============================================================================
 * Update API
 *===========================================================================*/

int nimcp_rpe_update(nimcp_rpe_system_t* system, float dt) {
    if (!system || !system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_rpe_update: required parameter is NULL (system, system->initialized)");
        return -1;
    }

    /* Update timing */
    system->time_since_last_reward += dt;

    /* Update eligibility traces */
    if (system->config.use_eligibility_traces) {
        nimcp_rpe_update_traces(system, dt);
    }

    return 0;
}

/*=============================================================================
 * Query API
 *===========================================================================*/

int nimcp_rpe_get_current(nimcp_rpe_system_t* system, float* rpe) {
    if (!system || !rpe) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_rpe_get_current: required parameter is NULL (system, rpe)");
        return -1;
    }

    if (!system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_rpe_get_current: system->initialized is NULL");
        return -1;
    }

    *rpe = system->current_rpe;
    return 0;
}

int nimcp_rpe_get_last_result(
    nimcp_rpe_system_t* system,
    nimcp_rpe_result_t* result
) {
    if (!system || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_rpe_get_last_result: required parameter is NULL (system, result)");
        return -1;
    }

    if (!system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_rpe_get_last_result: system->initialized is NULL");
        return -1;
    }

    *result = system->last_result;
    return 0;
}

nimcp_rpe_type_t nimcp_rpe_classify(float rpe, float threshold) {
    if (rpe > threshold) {
        return RPE_TYPE_POSITIVE;
    } else if (rpe < -threshold) {
        return RPE_TYPE_NEGATIVE;
    }
    return RPE_TYPE_NONE;
}

int nimcp_rpe_to_da_response(
    nimcp_rpe_system_t* system,
    float rpe,
    float* da_response
) {
    if (!system || !da_response) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_rpe_to_da_response: required parameter is NULL (system, da_response)");
        return -1;
    }

    if (!system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_rpe_to_da_response: system->initialized is NULL");
        return -1;
    }

    /* Map RPE to DA response using sigmoid-like function */
    *da_response = tanhf(rpe * system->config.rpe_scale);
    return 0;
}
