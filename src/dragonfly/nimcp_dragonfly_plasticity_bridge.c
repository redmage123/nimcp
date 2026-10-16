/**
 * @file nimcp_dragonfly_plasticity_bridge.c
 * @brief Plasticity-Dragonfly Integration Bridge Implementation
 *
 * WHAT: Enables learning-based adaptation of hunting circuits
 * WHY:  Improves hunting success through experience
 * HOW:  Reward-modulated plasticity on TSDN and prediction parameters
 *
 * @author NIMCP Team
 * @date 2024-12-29
 */

#include "dragonfly/nimcp_dragonfly_plasticity_bridge.h"
#include "constants/nimcp_constants.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>
#include <time.h>

#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_math_constants.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(dragonfly_plasticity_bridge)

#define LOG_MODULE "DRAGONFLY_PLASTICITY_BRIDGE"


//=============================================================================
// Constants
//=============================================================================


//=============================================================================
// Local Helpers
//=============================================================================

static inline uint64_t get_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

//=============================================================================
// Internal Structure
//=============================================================================

struct dragonfly_plasticity_bridge_s {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */

    /* Configuration */
    dragonfly_plasticity_config_t config;

    /* Connected systems */
    dragonfly_system_t* dragonfly;
    plasticity_coordinator_t plasticity;
    bool connected;

    /* Adaptation state */
    dragonfly_plasticity_state_t state;

    /* Statistics */
    dragonfly_plasticity_stats_t stats;

    /* Timing */
    uint64_t creation_time_us;
    uint64_t last_update_us;
};

//=============================================================================
// Configuration Functions
//=============================================================================

dragonfly_plasticity_config_t dragonfly_plasticity_default_config(void) {
    dragonfly_plasticity_config_t config = {
        /* Learning rates */
        .tsdn_learning_rate = NIMCP_LEARNING_RATE_DEFAULT,
        .imm_learning_rate = 0.05f,
        .intercept_learning_rate = 0.02f,

        /* Eligibility traces */
        .eligibility_decay = NIMCP_ELIGIBILITY_DECAY_DEFAULT,
        .enable_eligibility = true,

        /* Reward modulation */
        .success_reward = 1.0f,
        .failure_punishment = -0.5f,
        .prediction_error_weight = 0.3f,

        /* Constraints */
        .min_nav_gain = 1.0f,
        .max_nav_gain = 5.0f,
        .min_tuning_width = 0.1f,
        .max_tuning_width = 1.0f,

        /* Homeostasis */
        .enable_homeostasis = true,
        .homeostasis_rate = NIMCP_LEARNING_RATE_DEFAULT,
        .target_activity = 0.3f,

        /* Metaplasticity */
        .enable_metaplasticity = false,
        .metaplasticity_rate = 0.001f
    };
    return config;
}

bool dragonfly_plasticity_validate_config(const dragonfly_plasticity_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_plasticity_validate_config: config is NULL");
        return false;
    }

    if (config->tsdn_learning_rate < 0.0f ||
        config->tsdn_learning_rate > 1.0f) return false;
    if (config->imm_learning_rate < 0.0f ||
        config->imm_learning_rate > 1.0f) return false;
    if (config->intercept_learning_rate < 0.0f ||
        config->intercept_learning_rate > 1.0f) return false;

    if (config->eligibility_decay < 0.0f ||
        config->eligibility_decay > 1.0f) return false;

    if (config->min_nav_gain <= 0.0f) {
        return false;
    }
    if (config->max_nav_gain < config->min_nav_gain) {
        return false;
    }
    if (config->min_tuning_width <= 0.0f) {
        return false;
    }
    if (config->max_tuning_width < config->min_tuning_width) {
        return false;
    }

    if (config->homeostasis_rate < 0.0f) {
        return false;
    }
    if (config->target_activity < 0.0f ||
        config->target_activity > 1.0f) return false;

    return true;
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

dragonfly_plasticity_bridge_t dragonfly_plasticity_bridge_create(
    const dragonfly_plasticity_config_t* config
) {
    dragonfly_plasticity_config_t cfg = config ? *config : dragonfly_plasticity_default_config();

    if (!dragonfly_plasticity_validate_config(&cfg)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "dragonfly_plasticity_bridge_create: invalid configuration");
        return NULL;
    }

    dragonfly_plasticity_bridge_t bridge = nimcp_calloc(1, sizeof(struct dragonfly_plasticity_bridge_s));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "dragonfly_plasticity_bridge_create: failed to allocate bridge");
        return NULL;
    }

    bridge->config = cfg;
    bridge->creation_time_us = get_time_us();

    /* Initialize TSDN tuning with default values */
    for (int i = 0; i < PLASTICITY_TSDN_NEURONS; i++) {
        bridge->state.tsdn_state.preferred_direction[i] =
            2.0f * (float)M_PI * (float)i / (float)PLASTICITY_TSDN_NEURONS;
        bridge->state.tsdn_state.tuning_width[i] = 0.5f;
        bridge->state.tsdn_state.gain[i] = 1.0f;
        bridge->state.tsdn_state.baseline[i] = 0.1f;
    }

    /* Initialize IMM priors */
    for (int i = 0; i < PLASTICITY_IMM_MODELS; i++) {
        bridge->state.imm_state.model_priors[i] = 1.0f / PLASTICITY_IMM_MODELS;
        bridge->state.imm_state.model_success_rate[i] = 0.5f;
        for (int j = 0; j < PLASTICITY_IMM_MODELS; j++) {
            bridge->state.imm_state.transition_matrix[i][j] =
                (i == j) ? 0.8f : 0.2f / (PLASTICITY_IMM_MODELS - 1);
        }
    }

    /* Initialize interception parameters */
    bridge->state.intercept_state.nav_gain = 3.0f;
    bridge->state.intercept_state.lead_time_factor = 1.0f;
    bridge->state.intercept_state.pursuit_aggressiveness = 0.7f;
    bridge->state.intercept_state.abort_threshold = 0.3f;

    if (bridge_base_init(&bridge->base, 0, "dragonfly_plasticity") != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED,
            "dragonfly_plasticity_bridge_create: failed to initialize base bridge");
        nimcp_free(bridge);
        return NULL;
    }
    if (!bridge->base.mutex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED,
            "dragonfly_plasticity_bridge_create: base bridge mutex is NULL");
        nimcp_free(bridge);
        return NULL;
    }

    return bridge;
}

void dragonfly_plasticity_bridge_destroy(dragonfly_plasticity_bridge_t bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "dragonfly_plasticity");

    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
}

int dragonfly_plasticity_bridge_connect(
    dragonfly_plasticity_bridge_t bridge,
    dragonfly_system_t* dragonfly,
    plasticity_coordinator_t plasticity
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_plasticity_bridge_connect: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->dragonfly = dragonfly;
    bridge->plasticity = plasticity;
    bridge->connected = (dragonfly != NULL);
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int dragonfly_plasticity_bridge_disconnect(dragonfly_plasticity_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_plasticity_bridge_disconnect: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->dragonfly = NULL;
    bridge->plasticity = NULL;
    bridge->connected = false;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

//=============================================================================
// Learning Functions
//=============================================================================

int dragonfly_plasticity_learn(
    dragonfly_plasticity_bridge_t bridge,
    const plasticity_event_t* event
) {
    if (!bridge || !event) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_plasticity_learn: required parameter is NULL (bridge, event)");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    float reward = 0.0f;
    if (event->signal == LEARN_REWARD) {
        reward = event->magnitude * bridge->config.success_reward;
        bridge->stats.reward_events++;
    } else if (event->signal == LEARN_PUNISHMENT) {
        reward = -event->magnitude * fabsf(bridge->config.failure_punishment);
        bridge->stats.punishment_events++;
    } else if (event->signal == LEARN_PREDICTION_ERROR) {
        reward = -event->prediction_error * bridge->config.prediction_error_weight;
    }

    bridge->state.cumulative_reward += reward;
    bridge->state.learning_events++;

    /* Update TSDN tuning based on direction */
    int nearest_neuron = (int)(event->target_direction_rad /
                               (2.0f * (float)M_PI) * PLASTICITY_TSDN_NEURONS);
    nearest_neuron = nearest_neuron % PLASTICITY_TSDN_NEURONS;
    if (nearest_neuron < 0) nearest_neuron += PLASTICITY_TSDN_NEURONS;

    /* Apply eligibility-modulated learning to TSDN */
    if (bridge->config.enable_eligibility) {
        float eligibility = bridge->state.tsdn_eligibility[nearest_neuron];
        float delta = bridge->config.tsdn_learning_rate * reward * eligibility;
        bridge->state.tsdn_state.gain[nearest_neuron] += delta;
        bridge->state.tsdn_state.gain[nearest_neuron] =
            nimcp_clampf(bridge->state.tsdn_state.gain[nearest_neuron], 0.1f, 2.0f);
        bridge->stats.tsdn_updates++;
    }

    /* Update IMM priors based on motion model */
    if (event->motion_model < PLASTICITY_IMM_MODELS) {
        float delta = bridge->config.imm_learning_rate * reward;
        bridge->state.imm_state.model_priors[event->motion_model] += delta;

        /* Normalize priors */
        float sum = 0.0f;
        for (int i = 0; i < PLASTICITY_IMM_MODELS; i++) {
            bridge->state.imm_state.model_priors[i] =
                nimcp_clampf(bridge->state.imm_state.model_priors[i], 0.01f, 1.0f);
            sum += bridge->state.imm_state.model_priors[i];
        }
        for (int i = 0; i < PLASTICITY_IMM_MODELS; i++) {
            bridge->state.imm_state.model_priors[i] /= sum;
        }
        bridge->stats.imm_updates++;
    }

    /* Update interception parameters */
    float nav_delta = bridge->config.intercept_learning_rate * reward * 0.1f;
    bridge->state.intercept_state.nav_gain += nav_delta;
    bridge->state.intercept_state.nav_gain =
        nimcp_clampf(bridge->state.intercept_state.nav_gain,
                bridge->config.min_nav_gain, bridge->config.max_nav_gain);
    bridge->stats.nav_gain_change += fabsf(nav_delta);
    bridge->stats.intercept_updates++;

    bridge->stats.learning_progress =
        nimcp_clampf(bridge->state.cumulative_reward * 0.01f + 0.5f, 0.0f, 1.0f);

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int dragonfly_plasticity_reward(
    dragonfly_plasticity_bridge_t bridge,
    float target_direction,
    float target_speed,
    uint32_t motion_model,
    float miss_distance
) {
    plasticity_event_t event = {
        .signal = LEARN_REWARD,
        .magnitude = 1.0f - nimcp_clampf(miss_distance * 10.0f, 0.0f, 0.9f),
        .target_direction_rad = target_direction,
        .target_speed = target_speed,
        .motion_model = motion_model,
        .miss_distance = miss_distance,
        .hunt_success = true,
        .timestamp_us = get_time_us()
    };

    return dragonfly_plasticity_learn(bridge, &event);
}

int dragonfly_plasticity_punish(
    dragonfly_plasticity_bridge_t bridge,
    float target_direction,
    float target_speed,
    uint32_t motion_model,
    float miss_distance,
    const char* reason
) {
    (void)reason;  /* Currently unused */

    plasticity_event_t event = {
        .signal = LEARN_PUNISHMENT,
        .magnitude = 0.5f + nimcp_clampf(miss_distance * 5.0f, 0.0f, 0.5f),
        .target_direction_rad = target_direction,
        .target_speed = target_speed,
        .motion_model = motion_model,
        .miss_distance = miss_distance,
        .hunt_success = false,
        .timestamp_us = get_time_us()
    };

    return dragonfly_plasticity_learn(bridge, &event);
}

int dragonfly_plasticity_update_eligibility(
    dragonfly_plasticity_bridge_t bridge,
    float target_direction,
    uint32_t motion_model,
    float dt_s
) {
    if (!bridge || dt_s <= 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dragonfly_plasticity_update_eligibility: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Decay all eligibility traces */
    float decay = powf(bridge->config.eligibility_decay, dt_s * 100.0f);
    for (int i = 0; i < PLASTICITY_TSDN_NEURONS; i++) {
        bridge->state.tsdn_eligibility[i] *= decay;
    }
    for (int i = 0; i < PLASTICITY_IMM_MODELS; i++) {
        bridge->state.imm_eligibility[i] *= decay;
    }

    /* Boost eligibility for active direction/model */
    int nearest_neuron = (int)(target_direction /
                               (2.0f * (float)M_PI) * PLASTICITY_TSDN_NEURONS);
    nearest_neuron = nearest_neuron % PLASTICITY_TSDN_NEURONS;
    if (nearest_neuron < 0) nearest_neuron += PLASTICITY_TSDN_NEURONS;

    bridge->state.tsdn_eligibility[nearest_neuron] = 1.0f;

    if (motion_model < PLASTICITY_IMM_MODELS) {
        bridge->state.imm_eligibility[motion_model] = 1.0f;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

//=============================================================================
// Adaptation Functions
//=============================================================================

int dragonfly_plasticity_apply_adaptations(dragonfly_plasticity_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_plasticity_apply_adaptations: bridge is NULL");
        return -1;
    }

    /* In a full implementation, this would apply the learned parameters
       back to the dragonfly system's TSDN, IMM, and interception modules */

    return 0;
}

int dragonfly_plasticity_reset_adaptations(dragonfly_plasticity_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_plasticity_reset_adaptations: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Reset to defaults */
    for (int i = 0; i < PLASTICITY_TSDN_NEURONS; i++) {
        bridge->state.tsdn_state.gain[i] = 1.0f;
        bridge->state.tsdn_state.tuning_width[i] = 0.5f;
        bridge->state.tsdn_eligibility[i] = 0.0f;
    }

    for (int i = 0; i < PLASTICITY_IMM_MODELS; i++) {
        bridge->state.imm_state.model_priors[i] = 1.0f / PLASTICITY_IMM_MODELS;
        bridge->state.imm_eligibility[i] = 0.0f;
    }

    bridge->state.intercept_state.nav_gain = 3.0f;
    bridge->state.intercept_state.lead_time_factor = 1.0f;
    bridge->state.intercept_state.pursuit_aggressiveness = 0.7f;

    bridge->state.cumulative_reward = 0.0f;
    bridge->state.learning_events = 0;

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int dragonfly_plasticity_get_tsdn_params(
    const dragonfly_plasticity_bridge_t bridge,
    tsdn_tuning_state_t* params
) {
    if (!bridge || !params) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_plasticity_get_tsdn_params: required parameter is NULL (bridge, params)");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    *params = bridge->state.tsdn_state;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int dragonfly_plasticity_get_imm_params(
    const dragonfly_plasticity_bridge_t bridge,
    imm_adaptation_state_t* params
) {
    if (!bridge || !params) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_plasticity_get_imm_params: required parameter is NULL (bridge, params)");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    *params = bridge->state.imm_state;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int dragonfly_plasticity_get_intercept_params(
    const dragonfly_plasticity_bridge_t bridge,
    intercept_adaptation_state_t* params
) {
    if (!bridge || !params) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_plasticity_get_intercept_params: required parameter is NULL (bridge, params)");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    *params = bridge->state.intercept_state;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

//=============================================================================
// Query Functions
//=============================================================================

int dragonfly_plasticity_get_state(
    const dragonfly_plasticity_bridge_t bridge,
    dragonfly_plasticity_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_plasticity_get_state: required parameter is NULL (bridge, state)");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    *state = bridge->state;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int dragonfly_plasticity_get_stats(
    const dragonfly_plasticity_bridge_t bridge,
    dragonfly_plasticity_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_plasticity_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

const char* dragonfly_learning_signal_name(learning_signal_t signal) {
    switch (signal) {
        case LEARN_REWARD:           return "REWARD";
        case LEARN_PUNISHMENT:       return "PUNISHMENT";
        case LEARN_PREDICTION_ERROR: return "PREDICTION_ERROR";
        case LEARN_NOVELTY:          return "NOVELTY";
        case LEARN_SURPRISE:         return "SURPRISE";
        default:                     return "UNKNOWN";
    }
}

const char* dragonfly_plasticity_target_name(plasticity_target_t target) {
    switch (target) {
        case PLAST_TARGET_TSDN_TUNING:  return "TSDN_TUNING";
        case PLAST_TARGET_TSDN_GAIN:    return "TSDN_GAIN";
        case PLAST_TARGET_IMM_PRIORS:   return "IMM_PRIORS";
        case PLAST_TARGET_NAV_GAIN:     return "NAV_GAIN";
        case PLAST_TARGET_LEAD_FACTOR:  return "LEAD_FACTOR";
        case PLAST_TARGET_ATTENTION:    return "ATTENTION";
        default:                        return "UNKNOWN";
    }
}
