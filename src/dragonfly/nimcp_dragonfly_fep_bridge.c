/**
 * @file nimcp_dragonfly_fep_bridge.c
 * @brief Dragonfly-to-Free Energy Principle Integration Bridge Implementation
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "dragonfly/nimcp_dragonfly_fep_bridge.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "constants/nimcp_constants.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(dragonfly_fep_bridge)

#define LOG_MODULE "DRAGONFLY_FEP_BRIDGE"


//=============================================================================
// Internal Structures
//=============================================================================

struct dragonfly_fep_bridge_s {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    dragonfly_system_t* dragonfly;
    void* fep_system;
    dragonfly_fep_config_t config;

    /* Current state beliefs */
    float beliefs[DRAGONFLY_FEP_MAX_STATE_DIM];
    float precision[DRAGONFLY_FEP_MAX_STATE_DIM];
    uint32_t state_dim;

    /* Generative model */
    dragonfly_fep_model_t current_model;
    float model_evidence[5];  /* One per model type */

    /* Prediction errors */
    dragonfly_fep_errors_t current_errors;
    float free_energy;
    float surprise;

    /* Active inference */
    dragonfly_fep_action_t current_action;
    float expected_free_energy_cache[4];  /* One per action type */

    /* Observations */
    float last_observations[DRAGONFLY_FEP_MAX_OBS_DIM];
    uint32_t obs_dim;

    /* Timing */
    float current_time_ms;

    /* Statistics */
    dragonfly_fep_stats_t stats;
};

//=============================================================================
// Helper Functions
//=============================================================================

static float compute_gaussian_log_prob(float x, float mean, float precision) {
    float diff = x - mean;
    return -0.5f * precision * diff * diff;
}

static float compute_model_prediction(
    dragonfly_fep_model_t model,
    const float* state,
    float dt_ms,
    float* predicted
) {
    /* Simple motion models for prediction */
    switch (model) {
        case FEP_MODEL_LINEAR:
            /* Position only, no velocity */
            predicted[0] = state[0];
            predicted[1] = state[1];
            predicted[2] = state[2];
            return 0.9f;

        case FEP_MODEL_CONSTANT_VELOCITY:
            /* x' = x + v*dt */
            predicted[0] = state[0] + state[3] * dt_ms * 0.001f;
            predicted[1] = state[1] + state[4] * dt_ms * 0.001f;
            predicted[2] = state[2] + state[5] * dt_ms * 0.001f;
            predicted[3] = state[3];
            predicted[4] = state[4];
            predicted[5] = state[5];
            return 0.85f;

        case FEP_MODEL_CONSTANT_ACCELERATION:
            /* x' = x + v*dt + 0.5*a*dt^2 */
            {
                float dt_s = dt_ms * 0.001f;
                predicted[0] = state[0] + state[3] * dt_s + 0.5f * state[6] * dt_s * dt_s;
                predicted[1] = state[1] + state[4] * dt_s + 0.5f * state[7] * dt_s * dt_s;
                predicted[2] = state[2] + state[5] * dt_s + 0.5f * state[8] * dt_s * dt_s;
                predicted[3] = state[3] + state[6] * dt_s;
                predicted[4] = state[4] + state[7] * dt_s;
                predicted[5] = state[5] + state[8] * dt_s;
                predicted[6] = state[6];
                predicted[7] = state[7];
                predicted[8] = state[8];
            }
            return 0.8f;

        case FEP_MODEL_MANEUVERING:
            /* Variable acceleration model */
            {
                float dt_s = dt_ms * 0.001f;
                float jerk = 0.1f;  /* Assume some jerk */
                predicted[0] = state[0] + state[3] * dt_s;
                predicted[1] = state[1] + state[4] * dt_s;
                predicted[2] = state[2] + state[5] * dt_s;
                predicted[3] = state[3] + jerk * dt_s;
                predicted[4] = state[4] + jerk * dt_s;
                predicted[5] = state[5];
            }
            return 0.7f;

        case FEP_MODEL_EVASIVE:
            /* Random-ish evasive maneuvers */
            {
                float dt_s = dt_ms * 0.001f;
                float evasion = 0.5f;
                predicted[0] = state[0] + state[3] * dt_s + evasion * 0.1f;
                predicted[1] = state[1] + state[4] * dt_s - evasion * 0.1f;
                predicted[2] = state[2] + state[5] * dt_s;
                predicted[3] = state[3] * 0.9f;
                predicted[4] = state[4] * 0.9f;
                predicted[5] = state[5];
            }
            return 0.6f;

        default:
            memcpy(predicted, state, 6 * sizeof(float));
            return 0.5f;
    }
}

//=============================================================================
// Configuration
//=============================================================================

int dragonfly_fep_bridge_default_config(dragonfly_fep_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_fep_bridge_default_config: config is NULL");
        return -1;
    }

    /* Model settings */
    config->default_model = FEP_MODEL_CONSTANT_VELOCITY;
    config->auto_model_selection = true;
    config->model_evidence_threshold = 0.7f;

    /* Precision settings */
    config->precision_mode = FEP_PRECISION_ADAPTIVE;
    config->sensory_precision = DRAGONFLY_FEP_DEFAULT_PRECISION;
    config->proprioceptive_precision = 0.8f;
    config->prior_precision = 0.5f;

    /* Inference settings */
    config->learning_rate = 0.1f;
    config->inference_steps = 10;
    config->action_precision = 1.0f;

    /* Integration */
    config->use_tsdn_predictions = true;
    config->use_tracking_observations = true;
    config->prediction_horizon_ms = 100.0f;

    return 0;
}

int dragonfly_fep_bridge_validate_config(const dragonfly_fep_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_fep_bridge_validate_config: config is NULL");
        return -1;
    }

    if (config->default_model > FEP_MODEL_EVASIVE) {
        return -1;
    }
    if (config->precision_mode > FEP_PRECISION_HIERARCHICAL) {
        return -1;
    }
    if (config->sensory_precision < 0.0f) {
        return -1;
    }
    if (config->proprioceptive_precision < 0.0f) {
        return -1;
    }
    if (config->prior_precision < 0.0f) {
        return -1;
    }
    if (config->learning_rate <= 0.0f || config->learning_rate > 1.0f) {
        return -1;
    }
    if (config->inference_steps == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dragonfly_fep_bridge_validate_config: config->inference_steps is zero");
        return -1;
    }
    if (config->prediction_horizon_ms < 0.0f) {
        return -1;
    }

    return 0;
}

//=============================================================================
// Lifecycle
//=============================================================================

dragonfly_fep_bridge_t* dragonfly_fep_bridge_create(
    dragonfly_system_t* dragonfly,
    void* fep_system,
    const dragonfly_fep_config_t* config
) {
    dragonfly_fep_bridge_t* bridge = nimcp_calloc(1, sizeof(dragonfly_fep_bridge_t));
    NIMCP_API_CHECK_ALLOC(bridge, "dragonfly_fep_bridge_create: failed to allocate bridge");

    if (config) {
        if (dragonfly_fep_bridge_validate_config(config) != 0) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dragonfly_fep_bridge_create: invalid config");
            nimcp_free(bridge);
            return NULL;
        }
        bridge->config = *config;
    } else {
        dragonfly_fep_bridge_default_config(&bridge->config);
    }

    bridge->dragonfly = dragonfly;
    bridge->fep_system = fep_system;

    /* Initialize state beliefs (6D: position + velocity) */
    bridge->state_dim = 6;
    for (uint32_t i = 0; i < bridge->state_dim; i++) {
        bridge->beliefs[i] = 0.0f;
        bridge->precision[i] = bridge->config.prior_precision;
    }

    /* Initialize model */
    bridge->current_model = bridge->config.default_model;
    for (int i = 0; i < 5; i++) {
        bridge->model_evidence[i] = 0.2f;  /* Uniform prior */
    }

    bridge->current_action = FEP_ACTION_OBSERVE;

    /* Initialize bridge base infrastructure (mutex + metrics) */
    if (bridge_base_init(&bridge->base, 0, "dragonfly_fep") != 0) {
        nimcp_free(bridge);
        return NULL;
    }

    return bridge;
}

void dragonfly_fep_bridge_destroy(dragonfly_fep_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "dragonfly_fep");
    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

int dragonfly_fep_bridge_reset(dragonfly_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_fep_bridge_reset: bridge is NULL");
        return -1;
    }

    /* Reset beliefs */
    for (uint32_t i = 0; i < bridge->state_dim; i++) {
        bridge->beliefs[i] = 0.0f;
        bridge->precision[i] = bridge->config.prior_precision;
    }

    /* Reset model evidence */
    for (int i = 0; i < 5; i++) {
        bridge->model_evidence[i] = 0.2f;
    }
    bridge->current_model = bridge->config.default_model;

    /* Reset errors */
    memset(&bridge->current_errors, 0, sizeof(bridge->current_errors));
    bridge->free_energy = 0.0f;
    bridge->surprise = 0.0f;

    bridge->current_action = FEP_ACTION_OBSERVE;
    bridge->current_time_ms = 0.0f;
    memset(&bridge->stats, 0, sizeof(bridge->stats));

    return 0;
}

//=============================================================================
// Prediction Error
//=============================================================================

int dragonfly_fep_compute_errors(
    dragonfly_fep_bridge_t* bridge,
    const float* observations,
    uint32_t obs_dim,
    dragonfly_fep_errors_t* errors
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_fep_compute_errors: bridge is NULL");
        return -1;
    }
    if (!observations) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_fep_compute_errors: observations is NULL");
        return -1;
    }
    if (!errors) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_fep_compute_errors: errors is NULL");
        return -1;
    }

    /* Store observations */
    bridge->obs_dim = (obs_dim > DRAGONFLY_FEP_MAX_OBS_DIM) ?
        DRAGONFLY_FEP_MAX_OBS_DIM : obs_dim;
    memcpy(bridge->last_observations, observations, bridge->obs_dim * sizeof(float));

    /* Compute sensory prediction error */
    float sensory_error = 0.0f;
    uint32_t compare_dim = (bridge->obs_dim < bridge->state_dim) ?
        bridge->obs_dim : bridge->state_dim;

    for (uint32_t i = 0; i < compare_dim; i++) {
        float diff = observations[i] - bridge->beliefs[i];
        sensory_error += diff * diff * bridge->config.sensory_precision;
    }
    sensory_error = (compare_dim > 0) ? sqrtf(sensory_error / (float)compare_dim) : 0.0f;

    /* Compute model prediction error */
    float predicted[DRAGONFLY_FEP_MAX_STATE_DIM];
    compute_model_prediction(bridge->current_model, bridge->beliefs, 16.0f, predicted);

    float model_error = 0.0f;
    for (uint32_t i = 0; i < compare_dim; i++) {
        float diff = observations[i] - predicted[i];
        model_error += diff * diff;
    }
    model_error = (compare_dim > 0) ? sqrtf(model_error / (float)compare_dim) : 0.0f;

    /* Precision-weighted error */
    float precision_weighted = 0.0f;
    for (uint32_t i = 0; i < compare_dim; i++) {
        float diff = observations[i] - bridge->beliefs[i];
        precision_weighted += diff * diff * bridge->precision[i];
    }
    precision_weighted = (compare_dim > 0) ? sqrtf(precision_weighted / (float)compare_dim) : 0.0f;

    /* Total free energy (simplified variational free energy) */
    float free_energy = sensory_error + model_error * 0.5f +
        bridge->config.prior_precision * 0.1f;

    /* Store results */
    errors->sensory_error = sensory_error;
    errors->proprioceptive_error = 0.0f;  /* Would need proprioceptive input */
    errors->model_error = model_error;
    errors->total_free_energy = free_energy;
    errors->precision_weighted_error = precision_weighted;

    bridge->current_errors = *errors;
    bridge->free_energy = free_energy;
    bridge->surprise = sensory_error;

    {
        float new_avg = (bridge->stats.avg_prediction_error * bridge->stats.predictions_made + sensory_error) /
            (bridge->stats.predictions_made + 1);
        if (isfinite(new_avg)) bridge->stats.avg_prediction_error = new_avg;
    }
    bridge->stats.predictions_made++;

    return 0;
}

float dragonfly_fep_get_free_energy(const dragonfly_fep_bridge_t* bridge) {
    if (!bridge) return 0.0f;
    return bridge->free_energy;
}

float dragonfly_fep_get_surprise(const dragonfly_fep_bridge_t* bridge) {
    if (!bridge) return 0.0f;
    return bridge->surprise;
}

int dragonfly_fep_update_precision(
    dragonfly_fep_bridge_t* bridge,
    float sensory_reliability,
    float proprioceptive_reliability
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_fep_update_precision: bridge is NULL");
        return -1;
    }

    if (bridge->config.precision_mode == FEP_PRECISION_ADAPTIVE) {
        /* Update precision based on reliability */
        bridge->config.sensory_precision =
            DRAGONFLY_FEP_DEFAULT_PRECISION * sensory_reliability;
        bridge->config.proprioceptive_precision =
            0.8f * proprioceptive_reliability;

        /* Update state precision */
        for (uint32_t i = 0; i < 3; i++) {
            bridge->precision[i] = bridge->config.sensory_precision;
        }
        for (uint32_t i = 3; i < 6; i++) {
            bridge->precision[i] = bridge->config.proprioceptive_precision;
        }
    }

    return 0;
}

//=============================================================================
// Active Inference
//=============================================================================

int dragonfly_fep_infer_state(
    dragonfly_fep_bridge_t* bridge,
    const float* observations,
    uint32_t obs_dim,
    dragonfly_fep_inference_t* inference
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_fep_infer_state: bridge is NULL");
        return -1;
    }
    if (!observations) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_fep_infer_state: observations is NULL");
        return -1;
    }
    if (!inference) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_fep_infer_state: inference is NULL");
        return -1;
    }

    /* Perform belief update (gradient descent on free energy) */
    uint32_t compare_dim = (obs_dim < bridge->state_dim) ? obs_dim : bridge->state_dim;

    for (uint32_t step = 0; step < bridge->config.inference_steps; step++) {
        for (uint32_t i = 0; i < compare_dim; i++) {
            /* Prediction error gradient */
            float error = observations[i] - bridge->beliefs[i];

            /* Update belief with learning rate */
            bridge->beliefs[i] += bridge->config.learning_rate * error *
                bridge->precision[i];
        }
        bridge->stats.inference_steps_total++;
    }

    /* Copy to output */
    memcpy(inference->beliefs, bridge->beliefs, bridge->state_dim * sizeof(float));
    memcpy(inference->precision, bridge->precision, bridge->state_dim * sizeof(float));
    inference->state_dim = bridge->state_dim;
    inference->current_action = bridge->current_action;
    inference->expected_free_energy = bridge->expected_free_energy_cache[bridge->current_action];

    return 0;
}

int dragonfly_fep_select_action(
    dragonfly_fep_bridge_t* bridge,
    dragonfly_fep_action_t* action,
    float* action_value
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_fep_select_action: bridge is NULL");
        return -1;
    }
    if (!action) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_fep_select_action: action is NULL");
        return -1;
    }

    /* Compute expected free energy for each action */
    float min_efe = 1e10f;
    dragonfly_fep_action_t best_action = FEP_ACTION_OBSERVE;

    for (int a = 0; a < 4; a++) {
        float efe = dragonfly_fep_expected_free_energy(bridge, (dragonfly_fep_action_t)a);
        bridge->expected_free_energy_cache[a] = efe;

        if (efe < min_efe) {
            min_efe = efe;
            best_action = (dragonfly_fep_action_t)a;
        }
    }

    *action = best_action;
    if (action_value) {
        *action_value = -min_efe;  /* Convert to value (higher is better) */
    }

    return 0;
}

int dragonfly_fep_apply_action(
    dragonfly_fep_bridge_t* bridge,
    dragonfly_fep_action_t action
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_fep_apply_action: bridge is NULL");
        return -1;
    }
    if (action > FEP_ACTION_PREDICT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dragonfly_fep_apply_action: invalid action");
        return -1;
    }

    bridge->current_action = action;

    /* Action modifies precision and model selection */
    switch (action) {
        case FEP_ACTION_OBSERVE:
            /* Increase sensory precision */
            bridge->config.sensory_precision *= 1.1f;
            break;

        case FEP_ACTION_PURSUIT:
            /* Shift to maneuvering model */
            if (bridge->config.auto_model_selection) {
                bridge->current_model = FEP_MODEL_MANEUVERING;
            }
            break;

        case FEP_ACTION_INTERCEPT:
            /* Maximum precision on position */
            for (uint32_t i = 0; i < 3; i++) {
                bridge->precision[i] = 2.0f;
            }
            break;

        case FEP_ACTION_PREDICT:
            /* Shift to predictive model */
            if (bridge->config.auto_model_selection) {
                bridge->current_model = FEP_MODEL_CONSTANT_ACCELERATION;
            }
            break;
    }

    return 0;
}

float dragonfly_fep_expected_free_energy(
    const dragonfly_fep_bridge_t* bridge,
    dragonfly_fep_action_t action
) {
    if (!bridge) return 1e10f;

    /* Simplified expected free energy calculation */
    float epistemic_value = 0.0f;  /* Information gain */
    float pragmatic_value = 0.0f;  /* Goal achievement */

    switch (action) {
        case FEP_ACTION_OBSERVE:
            /* High epistemic value, low pragmatic */
            epistemic_value = 0.8f;
            pragmatic_value = 0.2f;
            break;

        case FEP_ACTION_PURSUIT:
            /* Balanced */
            epistemic_value = 0.5f;
            pragmatic_value = 0.6f;
            break;

        case FEP_ACTION_INTERCEPT:
            /* High pragmatic value */
            epistemic_value = 0.3f;
            pragmatic_value = 0.9f;
            break;

        case FEP_ACTION_PREDICT:
            /* High epistemic value */
            epistemic_value = 0.9f;
            pragmatic_value = 0.3f;
            break;

        default:
            break;
    }

    /* Expected free energy is negative of value (we minimize EFE) */
    float efe = -(epistemic_value + pragmatic_value) +
        bridge->surprise * 0.5f;

    return efe;
}

//=============================================================================
// Generative Model
//=============================================================================

int dragonfly_fep_set_model(
    dragonfly_fep_bridge_t* bridge,
    dragonfly_fep_model_t model
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_fep_set_model: bridge is NULL");
        return -1;
    }
    if (model > FEP_MODEL_EVASIVE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dragonfly_fep_set_model: invalid model");
        return -1;
    }

    if (bridge->current_model != model) {
        bridge->stats.model_switches++;
    }
    bridge->current_model = model;
    return 0;
}

dragonfly_fep_model_t dragonfly_fep_get_best_model(
    const dragonfly_fep_bridge_t* bridge
) {
    if (!bridge) return FEP_MODEL_LINEAR;

    float best_evidence = -1e10f;
    dragonfly_fep_model_t best_model = FEP_MODEL_LINEAR;

    for (int i = 0; i < 5; i++) {
        if (bridge->model_evidence[i] > best_evidence) {
            best_evidence = bridge->model_evidence[i];
            best_model = (dragonfly_fep_model_t)i;
        }
    }

    return best_model;
}

float dragonfly_fep_get_model_evidence(
    const dragonfly_fep_bridge_t* bridge,
    dragonfly_fep_model_t model
) {
    if (!bridge) return 0.0f;
    if (model > FEP_MODEL_EVASIVE) return 0.0f;

    return bridge->model_evidence[model];
}

int dragonfly_fep_predict(
    dragonfly_fep_bridge_t* bridge,
    float horizon_ms,
    float* predicted_state,
    uint32_t state_dim
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_fep_predict: bridge is NULL");
        return -1;
    }
    if (!predicted_state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_fep_predict: predicted_state is NULL");
        return -1;
    }

    float temp_state[DRAGONFLY_FEP_MAX_STATE_DIM];
    memcpy(temp_state, bridge->beliefs, bridge->state_dim * sizeof(float));

    /* Iteratively predict forward */
    float dt_ms = 16.0f;  /* 60 Hz steps */
    float t = 0.0f;

    while (t < horizon_ms) {
        float next_state[DRAGONFLY_FEP_MAX_STATE_DIM];
        compute_model_prediction(bridge->current_model, temp_state, dt_ms, next_state);
        memcpy(temp_state, next_state, bridge->state_dim * sizeof(float));
        t += dt_ms;
    }

    /* Copy result */
    uint32_t copy_dim = (state_dim < bridge->state_dim) ? state_dim : bridge->state_dim;
    memcpy(predicted_state, temp_state, copy_dim * sizeof(float));

    return (int)copy_dim;
}

//=============================================================================
// Integration
//=============================================================================

int dragonfly_fep_connect_dragonfly(
    dragonfly_fep_bridge_t* bridge,
    dragonfly_system_t* dragonfly
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_fep_connect_dragonfly: bridge is NULL");
        return -1;
    }
    bridge->dragonfly = dragonfly;
    return 0;
}

int dragonfly_fep_connect_system(
    dragonfly_fep_bridge_t* bridge,
    void* fep_system
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_fep_connect_system: bridge is NULL");
        return -1;
    }
    bridge->fep_system = fep_system;
    return 0;
}

int dragonfly_fep_update(dragonfly_fep_bridge_t* bridge, float dt_ms) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_fep_update: bridge is NULL");
        return -1;
    }

    bridge->current_time_ms += dt_ms;

    /* Update beliefs with motion model */
    float predicted[DRAGONFLY_FEP_MAX_STATE_DIM];
    float confidence = compute_model_prediction(
        bridge->current_model, bridge->beliefs, dt_ms, predicted);

    /* Blend prediction with current beliefs */
    float blend = 0.3f;
    for (uint32_t i = 0; i < bridge->state_dim; i++) {
        bridge->beliefs[i] = (1.0f - blend) * bridge->beliefs[i] + blend * predicted[i];
    }

    /* Update model evidence based on prediction accuracy */
    float evidence_update = confidence * (1.0f - bridge->surprise);
    bridge->model_evidence[bridge->current_model] =
        0.9f * bridge->model_evidence[bridge->current_model] +
        0.1f * evidence_update;

    /* Auto model selection */
    if (bridge->config.auto_model_selection) {
        dragonfly_fep_model_t best = dragonfly_fep_get_best_model(bridge);
        if (best != bridge->current_model &&
            bridge->model_evidence[best] > bridge->config.model_evidence_threshold) {
            dragonfly_fep_set_model(bridge, best);
        }
    }

    /* Update stats */
    {
        float ema_fe = (bridge->stats.avg_free_energy * NIMCP_EMA_DECAY_DEFAULT) + (bridge->free_energy * NIMCP_LEARNING_RATE_DEFAULT);
        if (isfinite(ema_fe)) bridge->stats.avg_free_energy = ema_fe;
        float ema_prec = (bridge->stats.avg_precision * NIMCP_EMA_DECAY_DEFAULT) + (bridge->precision[0] * NIMCP_LEARNING_RATE_DEFAULT);
        if (isfinite(ema_prec)) bridge->stats.avg_precision = ema_prec;
    }

    for (int i = 0; i < 5; i++) {
        bridge->stats.model_evidence[i] = bridge->model_evidence[i];
    }

    return 0;
}

int dragonfly_fep_sync_with_tracker(dragonfly_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_fep_sync_with_tracker: bridge is NULL");
        return -1;
    }
    /* Would sync beliefs with tracker state */
    return 0;
}

int dragonfly_fep_sync_with_tsdn(dragonfly_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_fep_sync_with_tsdn: bridge is NULL");
        return -1;
    }
    /* Would sync with TSDN population vector */
    return 0;
}

//=============================================================================
// Statistics
//=============================================================================

int dragonfly_fep_bridge_get_stats(
    const dragonfly_fep_bridge_t* bridge,
    dragonfly_fep_stats_t* stats
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_fep_bridge_get_stats: bridge is NULL");
        return -1;
    }
    if (!stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_fep_bridge_get_stats: stats is NULL");
        return -1;
    }
    *stats = bridge->stats;
    return 0;
}

int dragonfly_fep_bridge_reset_stats(dragonfly_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_fep_bridge_reset_stats: bridge is NULL");
        return -1;
    }
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    return 0;
}

//=============================================================================
// Utility
//=============================================================================

const char* dragonfly_fep_model_name(dragonfly_fep_model_t model) {
    switch (model) {
        case FEP_MODEL_LINEAR: return "linear";
        case FEP_MODEL_CONSTANT_VELOCITY: return "constant_velocity";
        case FEP_MODEL_CONSTANT_ACCELERATION: return "constant_acceleration";
        case FEP_MODEL_MANEUVERING: return "maneuvering";
        case FEP_MODEL_EVASIVE: return "evasive";
        default: return "unknown";
    }
}

const char* dragonfly_fep_action_name(dragonfly_fep_action_t action) {
    switch (action) {
        case FEP_ACTION_OBSERVE: return "observe";
        case FEP_ACTION_PURSUIT: return "pursuit";
        case FEP_ACTION_INTERCEPT: return "intercept";
        case FEP_ACTION_PREDICT: return "predict";
        default: return "unknown";
    }
}

const char* dragonfly_fep_precision_mode_name(dragonfly_fep_precision_mode_t mode) {
    switch (mode) {
        case FEP_PRECISION_FIXED: return "fixed";
        case FEP_PRECISION_ADAPTIVE: return "adaptive";
        case FEP_PRECISION_HIERARCHICAL: return "hierarchical";
        default: return "unknown";
    }
}
