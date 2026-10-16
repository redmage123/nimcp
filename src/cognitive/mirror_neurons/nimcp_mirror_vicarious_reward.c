/**
 * @file nimcp_mirror_vicarious_reward.c
 * @brief Mirror Neuron Vicarious Reward Implementation
 * @version 1.0.0
 * @date 2025-01-05
 *
 * WHAT: Implements vicarious reward processing through mirror neuron observation
 * WHY:  Social learning requires experiencing others' rewards vicariously
 * HOW:  Mirror activation + social modulation → dopamine proxy signal
 */

#include "cognitive/mirror_neurons/nimcp_mirror_vicarious_reward.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_learning_constants.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE(mirror_vicarious_reward, MESH_ADAPTER_CATEGORY_COGNITIVE)


//=============================================================================
// Internal Structure
//=============================================================================

struct vicarious_reward_system {
    vicarious_reward_config_t config;
    vicarious_state_t state;

    /** Agent states */
    vicarious_agent_state_t agents[VICARIOUS_MAX_AGENTS];
    uint32_t agent_count;

    /** Action-outcome associations */
    action_outcome_assoc_t associations[VICARIOUS_MAX_ASSOCIATIONS];
    uint32_t association_count;

    /** Dopamine proxy state */
    float current_dopamine;
    float baseline_dopamine;

    /** Statistics */
    vicarious_reward_stats_t stats;

    /** Thread safety */
    nimcp_mutex_t* mutex;

    /** Bio-async */
    bool bio_async_registered;
};

static inline float lerp_f(float a, float b, float t) {
    return a + t * (b - a);
}

/**
 * @brief Find or create agent state
 */
static vicarious_agent_state_t* find_or_create_agent(
    vicarious_reward_system_t* system,
    uint32_t agent_id
) {
    /* Search existing */
    for (uint32_t i = 0; i < system->agent_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->agent_count > 256) {
            mirror_vicarious_reward_heartbeat("mirror_vicar_loop",
                             (float)(i + 1) / (float)system->agent_count);
        }

        if (system->agents[i].agent_id == agent_id) {
            return &system->agents[i];
        }
    }

    /* Create new if space available */
    if (system->agent_count < VICARIOUS_MAX_AGENTS) {
        vicarious_agent_state_t* agent = &system->agents[system->agent_count++];
        memset(agent, 0, sizeof(vicarious_agent_state_t));
        agent->agent_id = agent_id;
        agent->active = true;
        agent->relation = SOCIAL_RELATION_UNKNOWN;
        agent->social_distance = 0.5f;
        agent->familiarity = 0.0f;
        agent->trust = 0.5f;
        agent->first_observation_us = nimcp_time_now_us();
        return agent;
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_or_create_agent: operation failed");
    return NULL;
}

/**
 * @brief Find action-outcome association
 */
static action_outcome_assoc_t* find_or_create_association(
    vicarious_reward_system_t* system,
    uint32_t action_id
) {
    /* Search existing */
    for (uint32_t i = 0; i < system->association_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->association_count > 256) {
            mirror_vicarious_reward_heartbeat("mirror_vicar_loop",
                             (float)(i + 1) / (float)system->association_count);
        }

        if (system->associations[i].action_id == action_id) {
            return &system->associations[i];
        }
    }

    /* Create new if space available */
    if (system->association_count < VICARIOUS_MAX_ASSOCIATIONS) {
        action_outcome_assoc_t* assoc = &system->associations[system->association_count++];
        memset(assoc, 0, sizeof(action_outcome_assoc_t));
        assoc->action_id = action_id;
        assoc->expected_reward = 0.0f;
        assoc->expected_probability = 0.5f;
        assoc->expected_delay_ms = 0.0f;
        assoc->last_update_us = nimcp_time_now_us();
        return assoc;
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_or_create_association: operation failed");
    return NULL;
}

/**
 * @brief Get social relation weight
 */
static float get_relation_weight(
    const vicarious_reward_config_t* config,
    social_relation_t relation
) {
    switch (relation) {
        case SOCIAL_RELATION_KIN:
            return config->kin_weight;
        case SOCIAL_RELATION_INGROUP:
        case SOCIAL_RELATION_COOPERATOR:
            return config->ingroup_weight;
        case SOCIAL_RELATION_OUTGROUP:
            return config->outgroup_weight;
        case SOCIAL_RELATION_COMPETITOR:
            return 1.0f - config->competition_inversion;
        case SOCIAL_RELATION_SELF:
            return 1.0f;  /* Direct reward */
        default:
            return 0.5f;
    }
}

/**
 * @brief Compute social modulation internal
 */
static float compute_social_modulation(
    const vicarious_reward_system_t* system,
    const vicarious_agent_state_t* agent
) {
    if (!agent) return 1.0f;

    /* Base modulation from relation */
    float relation_mod = get_relation_weight(&system->config, agent->relation);

    /* Distance attenuation */
    float distance_mod = expf(-agent->social_distance * system->config.social_distance_decay);

    /* Familiarity boost */
    float familiarity_mod = 1.0f + agent->familiarity * system->config.familiarity_boost;

    return relation_mod * distance_mod * familiarity_mod;
}

/**
 * @brief Determine vicarious response type
 */
static vicarious_response_t determine_response_type(
    const vicarious_reward_system_t* system,
    const vicarious_agent_state_t* agent,
    float observed_reward
) {
    if (!agent) return VICARIOUS_RESPONSE_EMPATHIC;

    bool is_competitor = (agent->relation == SOCIAL_RELATION_COMPETITOR);
    bool is_outgroup = (agent->relation == SOCIAL_RELATION_OUTGROUP);

    if (observed_reward > 0.1f) {
        /* Positive reward observed */
        if (is_competitor && system->config.enable_envy) {
            return VICARIOUS_RESPONSE_ENVY;
        }
        return VICARIOUS_RESPONSE_EMPATHIC;
    } else if (observed_reward < -0.1f) {
        /* Negative reward (loss/pain) observed */
        if ((is_competitor || is_outgroup) && system->config.enable_schadenfreude) {
            return VICARIOUS_RESPONSE_SCHADENFREUDE;
        }
        return VICARIOUS_RESPONSE_SYMPATHETIC;
    }

    return VICARIOUS_RESPONSE_NONE;
}

/**
 * @brief Update agent history
 */
static void update_agent_history(
    vicarious_agent_state_t* agent,
    float reward,
    float prediction
) {
    if (!agent) return;

    agent->reward_history[agent->history_index] = reward;
    agent->prediction_history[agent->history_index] = prediction;
    agent->history_index = (agent->history_index + 1) % VICARIOUS_HISTORY_SIZE;
    if (agent->history_count < VICARIOUS_HISTORY_SIZE) {
        agent->history_count++;
    }

    /* Update running averages */
    if (isfinite(reward)) {
        agent->avg_reward = agent->avg_reward * 0.9f + reward * 0.1f;
    }
    float rpe = reward - prediction;
    if (isfinite(rpe)) {
        agent->avg_prediction_error = agent->avg_prediction_error * 0.9f + fabsf(rpe) * 0.1f;
    }

    /* Update variance estimate */
    float diff = reward - agent->avg_reward;
    if (isfinite(diff)) {
        agent->reward_variance = agent->reward_variance * 0.95f + diff * diff * 0.05f;
    }

    agent->observations_count++;
    agent->last_observation_us = nimcp_time_now_us();
}

//=============================================================================
// Public API Implementation
//=============================================================================

vicarious_reward_config_t vicarious_reward_config_default(void) {
    /* Phase 8: Heartbeat at operation start */
    mirror_vicarious_reward_heartbeat("mirror_vicar_vicarious_reward_con", 0.0f);


    vicarious_reward_config_t config = {
        .vicarious_gain = 0.5f,              /* 50% of direct reward */
        .social_distance_decay = 2.0f,
        .familiarity_boost = 0.5f,

        .competition_inversion = 0.3f,       /* Mild inversion for competitors */
        .enable_schadenfreude = true,
        .enable_envy = true,

        .learning_rate = NIMCP_LEARNING_RATE_COARSE,
        .prediction_decay = 0.01f,
        .eligibility_trace_decay = NIMCP_EMA_DECAY_FAST,

        .imitation_threshold = 0.3f,
        .imitation_familiar_bonus = 0.2f,

        .ingroup_weight = 1.2f,
        .outgroup_weight = 0.6f,
        .kin_weight = 1.5f,

        .enable_simd = true,
        .bio_async_enabled = true
    };
    return config;
}

vicarious_reward_system_t* vicarious_reward_create(
    const vicarious_reward_config_t* config
) {
    /* Phase 8: Heartbeat at operation start */
    mirror_vicarious_reward_heartbeat("mirror_vicar_vicarious_reward_cre", 0.0f);


    vicarious_reward_system_t* system = nimcp_calloc(1, sizeof(vicarious_reward_system_t));
    if (!system) {
        nimcp_log(LOG_LEVEL_ERROR, "Vicarious Reward: Failed to allocate system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "vicarious_reward_create: system is NULL");
        return NULL;
    }

    /* Apply configuration */
    if (config) {
        system->config = *config;
    } else {
        system->config = vicarious_reward_config_default();
    }

    /* Initialize state */
    system->state = VICARIOUS_STATE_IDLE;
    system->agent_count = 0;
    system->association_count = 0;
    system->current_dopamine = 0.5f;
    system->baseline_dopamine = 0.5f;

    /* Create mutex */
    mutex_attr_t attr = {0};
    attr.type = MUTEX_TYPE_NORMAL;
    system->mutex = nimcp_mutex_create(&attr);
    if (!system->mutex) {
        nimcp_log(LOG_LEVEL_ERROR, "Vicarious Reward: Failed to create mutex");
        nimcp_free(system);
        system = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "vicarious_reward_create: system->mutex is NULL");
        return NULL;
    }

    /* Register with bio-async if enabled */
    if (system->config.bio_async_enabled) {
        vicarious_reward_register_bio_async(system);
    }

    nimcp_log(LOG_LEVEL_INFO, "Vicarious Reward: Created system (gain=%.2f)",
              system->config.vicarious_gain);

    return system;
}

void vicarious_reward_destroy(vicarious_reward_system_t* system) {
    if (!system) return;

    /* Phase 8: Heartbeat at operation start */
    mirror_vicarious_reward_heartbeat("mirror_vicar_vicarious_reward_des", 0.0f);


    if (system->bio_async_registered) {
        vicarious_reward_unregister_bio_async(system);
    }

    if (system->mutex) {
        nimcp_mutex_destroy(system->mutex);
    }

    nimcp_free(system);
    system = NULL;
    nimcp_log(LOG_LEVEL_DEBUG, "Vicarious Reward: Destroyed system");
}

//=============================================================================
// Core Processing API
//=============================================================================

bool vicarious_reward_process(
    vicarious_reward_system_t* system,
    const vicarious_observation_t* observation,
    vicarious_response_result_t* result
) {
    if (!system || !observation || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "vicarious_reward_process: required parameter is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_vicarious_reward_heartbeat("mirror_vicar_vicarious_reward_pro", 0.0f);


    nimcp_mutex_lock(system->mutex);

    uint64_t start_time = nimcp_time_now_us();
    memset(result, 0, sizeof(vicarious_response_result_t));
    result->timestamp_us = start_time;
    result->baseline_dopamine = system->current_dopamine;

    system->state = VICARIOUS_STATE_OBSERVING;

    /* Get or create agent state */
    vicarious_agent_state_t* agent = find_or_create_agent(system, observation->agent_id);
    if (!agent) {
        nimcp_mutex_unlock(system->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vicarious_reward_process: agent is NULL");
        return false;
    }

    /* Update agent with observation data */
    if (observation->social_relation != SOCIAL_RELATION_UNKNOWN) {
        agent->relation = observation->social_relation;
    }
    agent->social_distance = observation->social_distance;
    if (observation->familiarity > agent->familiarity) {
        agent->familiarity = lerp_f(agent->familiarity, observation->familiarity, 0.1f);
    }

    /* Get or create action association */
    action_outcome_assoc_t* assoc = find_or_create_association(system, observation->action_id);

    /* Compute prediction */
    system->state = VICARIOUS_STATE_PREDICTING;
    result->expected_reward = assoc ? assoc->expected_reward : 0.0f;

    /* Compute social modulation */
    result->social_modulation = compute_social_modulation(system, agent);
    result->familiarity_modulation = 1.0f + agent->familiarity * system->config.familiarity_boost;

    /* Competition modulation */
    if (observation->is_competitive_context ||
        agent->relation == SOCIAL_RELATION_COMPETITOR) {
        result->competition_modulation = 1.0f - system->config.competition_inversion;
    } else {
        result->competition_modulation = 1.0f;
    }

    /* Process observed outcome */
    system->state = VICARIOUS_STATE_PROCESSING_OUTCOME;

    /* Convert observation to reward value */
    float raw_reward = 0.0f;
    switch (observation->reward_type) {
        case VICARIOUS_REWARD_POSITIVE:
            raw_reward = observation->reward_magnitude;
            break;
        case VICARIOUS_REWARD_NEGATIVE:
            raw_reward = -observation->reward_magnitude;
            break;
        case VICARIOUS_REWARD_RELIEF:
            raw_reward = observation->reward_magnitude * 0.7f;  /* Relief is positive but less */
            break;
        case VICARIOUS_REWARD_ANTICIPATORY:
            raw_reward = observation->reward_magnitude * observation->reward_probability;
            break;
        default:
            raw_reward = 0.0f;
    }

    /* Apply vicarious gain and modulations */
    float modulated_reward = raw_reward * system->config.vicarious_gain;
    modulated_reward *= result->social_modulation;
    modulated_reward *= result->competition_modulation;
    modulated_reward *= observation->mirror_activation;

    /* Determine response type */
    result->response_type = determine_response_type(system, agent, raw_reward);

    /* Adjust for schadenfreude/envy */
    if (result->response_type == VICARIOUS_RESPONSE_SCHADENFREUDE) {
        /* Invert: other's loss becomes our gain */
        modulated_reward = -modulated_reward * 0.5f;
        system->stats.schadenfreude_responses++;
    } else if (result->response_type == VICARIOUS_RESPONSE_ENVY) {
        /* Invert: other's gain becomes our distress */
        modulated_reward = -modulated_reward * 0.3f;
        system->stats.envy_responses++;
    } else if (result->response_type == VICARIOUS_RESPONSE_EMPATHIC) {
        system->stats.empathic_responses++;
    } else if (result->response_type == VICARIOUS_RESPONSE_SYMPATHETIC) {
        system->stats.sympathetic_responses++;
    }

    result->vicarious_reward = nimcp_clampf(modulated_reward, -1.0f, 1.0f);
    result->response_intensity = fabsf(modulated_reward);

    /* Compute prediction error */
    result->reward_prediction_error = raw_reward - result->expected_reward;

    /* Compute dopamine delta */
    float rpe_scaled = result->reward_prediction_error * system->config.vicarious_gain;
    result->dopamine_delta = nimcp_clampf(rpe_scaled, -0.5f, 0.5f);

    /* Update dopamine state */
    system->current_dopamine = nimcp_clampf(
        system->baseline_dopamine + result->dopamine_delta,
        0.0f, 1.0f
    );

    /* Update learning */
    system->state = VICARIOUS_STATE_UPDATING;

    if (assoc) {
        float lr = system->config.learning_rate * observation->observation_confidence;
        assoc->expected_reward += lr * result->reward_prediction_error;
        assoc->expected_reward = nimcp_clampf(assoc->expected_reward, -1.0f, 1.0f);

        /* Update probability estimate */
        float prob_target = (raw_reward > 0.1f) ? 1.0f : 0.0f;
        assoc->expected_probability += lr * (prob_target - assoc->expected_probability);

        /* Update delay estimate */
        float delay = (float)(observation->reward_time_us - observation->action_time_us) / 1000.0f;
        assoc->expected_delay_ms += lr * (delay - assoc->expected_delay_ms);

        assoc->observation_count++;
        assoc->value_confidence = 1.0f - expf(-0.1f * (float)assoc->observation_count);
        assoc->last_update_us = nimcp_time_now_us();

        result->value_update = lr * result->reward_prediction_error;
    }

    /* Update agent history */
    update_agent_history(agent, raw_reward, result->expected_reward);

    /* Compute imitation likelihood */
    if (result->vicarious_reward > system->config.imitation_threshold) {
        result->should_imitate = true;
        result->imitation_likelihood = nimcp_clampf(
            result->vicarious_reward +
            agent->familiarity * system->config.imitation_familiar_bonus,
            0.0f, 1.0f
        );
        system->stats.imitation_recommendations++;
        system->stats.avg_imitation_likelihood =
            system->stats.avg_imitation_likelihood * 0.95f +
            result->imitation_likelihood * 0.05f;
    } else {
        result->should_imitate = false;
        result->imitation_likelihood = 0.0f;
    }

    /* Update statistics */
    result->processing_latency_ms = (float)(nimcp_time_now_us() - start_time) / 1000.0f;

    system->stats.total_observations++;
    if (raw_reward > 0.1f) {
        system->stats.positive_rewards++;
    } else if (raw_reward < -0.1f) {
        system->stats.negative_rewards++;
    }

    system->stats.avg_vicarious_reward =
        system->stats.avg_vicarious_reward * 0.95f + result->vicarious_reward * 0.05f;
    system->stats.avg_prediction_error =
        system->stats.avg_prediction_error * 0.95f + fabsf(result->reward_prediction_error) * 0.05f;
    system->stats.avg_social_modulation =
        system->stats.avg_social_modulation * 0.95f + result->social_modulation * 0.05f;

    system->stats.active_agents = system->agent_count;
    system->stats.learned_associations = system->association_count;

    system->state = VICARIOUS_STATE_IDLE;

    nimcp_mutex_unlock(system->mutex);
    return true;
}

uint32_t vicarious_reward_process_batch(
    vicarious_reward_system_t* system,
    const vicarious_observation_t* observations,
    vicarious_response_result_t* results,
    uint32_t count
) {
    if (!system || !observations || !results || count == 0) {
        if (!system || !observations || !results) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "vicarious_reward_process_batch: required parameter is NULL");
            return 0;
        }
        return 0;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_vicarious_reward_heartbeat("mirror_vicar_vicarious_reward_pro", 0.0f);


    uint32_t processed = 0;
    for (uint32_t i = 0; i < count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && count > 256) {
            mirror_vicarious_reward_heartbeat("mirror_vicar_loop",
                             (float)(i + 1) / (float)count);
        }

        if (vicarious_reward_process(system, &observations[i], &results[i])) {
            processed++;
        }
    }

    if (system->config.enable_simd && count >= VICARIOUS_SIMD_THRESHOLD) {
        system->stats.simd_operations++;
    }

    return processed;
}

float vicarious_reward_predict(
    vicarious_reward_system_t* system,
    uint32_t agent_id,
    uint32_t action_id
) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "vicarious_reward_predict: system is NULL");
        return 0.0f;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_vicarious_reward_heartbeat("mirror_vicar_vicarious_reward_pre", 0.0f);


    nimcp_mutex_lock(system->mutex);

    float prediction = 0.0f;

    /* Find action association */
    for (uint32_t i = 0; i < system->association_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->association_count > 256) {
            mirror_vicarious_reward_heartbeat("mirror_vicar_loop",
                             (float)(i + 1) / (float)system->association_count);
        }

        if (system->associations[i].action_id == action_id) {
            prediction = system->associations[i].expected_reward;
            break;
        }
    }

    /* Modulate by agent if known */
    vicarious_agent_state_t* agent = NULL;
    for (uint32_t i = 0; i < system->agent_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->agent_count > 256) {
            mirror_vicarious_reward_heartbeat("mirror_vicar_loop",
                             (float)(i + 1) / (float)system->agent_count);
        }

        if (system->agents[i].agent_id == agent_id) {
            agent = &system->agents[i];
            break;
        }
    }

    if (agent) {
        /* Weight prediction by agent's historical reliability */
        float reliability = 1.0f - agent->avg_prediction_error;
        prediction *= reliability;
    }

    nimcp_mutex_unlock(system->mutex);
    return prediction;
}

//=============================================================================
// Agent State API
//=============================================================================

vicarious_agent_state_t* vicarious_reward_get_agent(
    vicarious_reward_system_t* system,
    uint32_t agent_id
) {
    if (!system) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "system is NULL");

        return NULL;

    }

    /* Phase 8: Heartbeat at operation start */
    mirror_vicarious_reward_heartbeat("mirror_vicar_vicarious_reward_get", 0.0f);


    nimcp_mutex_lock(system->mutex);
    vicarious_agent_state_t* agent = find_or_create_agent(system, agent_id);
    nimcp_mutex_unlock(system->mutex);

    return agent;
}

void vicarious_reward_set_relation(
    vicarious_reward_system_t* system,
    uint32_t agent_id,
    social_relation_t relation
) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "vicarious_reward_set_relation: system is NULL");
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_vicarious_reward_heartbeat("mirror_vicar_vicarious_reward_set", 0.0f);


    nimcp_mutex_lock(system->mutex);
    vicarious_agent_state_t* agent = find_or_create_agent(system, agent_id);
    if (agent) {
        agent->relation = relation;
    }
    nimcp_mutex_unlock(system->mutex);
}

void vicarious_reward_update_familiarity(
    vicarious_reward_system_t* system,
    uint32_t agent_id,
    float delta
) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "vicarious_reward_update_familiarity: system is NULL");
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_vicarious_reward_heartbeat("mirror_vicar_vicarious_reward_upd", 0.0f);


    nimcp_mutex_lock(system->mutex);
    vicarious_agent_state_t* agent = find_or_create_agent(system, agent_id);
    if (agent) {
        agent->familiarity = nimcp_clampf(agent->familiarity + delta, 0.0f, 1.0f);
    }
    nimcp_mutex_unlock(system->mutex);
}

void vicarious_reward_set_competitive(
    vicarious_reward_system_t* system,
    uint32_t agent_id,
    bool is_competitor
) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "vicarious_reward_set_competitive: system is NULL");
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_vicarious_reward_heartbeat("mirror_vicar_vicarious_reward_set", 0.0f);


    nimcp_mutex_lock(system->mutex);
    vicarious_agent_state_t* agent = find_or_create_agent(system, agent_id);
    if (agent) {
        agent->relation = is_competitor ? SOCIAL_RELATION_COMPETITOR : agent->relation;
    }
    nimcp_mutex_unlock(system->mutex);
}

//=============================================================================
// Value Learning API
//=============================================================================

float vicarious_reward_get_action_value(
    const vicarious_reward_system_t* system,
    uint32_t action_id
) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "vicarious_reward_get_action_value: system is NULL");
        return 0.0f;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_vicarious_reward_heartbeat("mirror_vicar_vicarious_reward_get", 0.0f);


    for (uint32_t i = 0; i < system->association_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->association_count > 256) {
            mirror_vicarious_reward_heartbeat("mirror_vicar_loop",
                             (float)(i + 1) / (float)system->association_count);
        }

        if (system->associations[i].action_id == action_id) {
            return system->associations[i].expected_reward;
        }
    }
    return 0.0f;
}

const action_outcome_assoc_t* vicarious_reward_get_association(
    const vicarious_reward_system_t* system,
    uint32_t action_id
) {
    if (!system) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "system is NULL");

        return NULL;

    }

    /* Phase 8: Heartbeat at operation start */
    mirror_vicarious_reward_heartbeat("mirror_vicar_vicarious_reward_get", 0.0f);


    for (uint32_t i = 0; i < system->association_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->association_count > 256) {
            mirror_vicarious_reward_heartbeat("mirror_vicar_loop",
                             (float)(i + 1) / (float)system->association_count);
        }

        if (system->associations[i].action_id == action_id) {
            return &system->associations[i];
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vicarious_reward_get_association: validation failed");
    return NULL;
}

void vicarious_reward_decay_predictions(
    vicarious_reward_system_t* system,
    float dt_sec
) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "vicarious_reward_decay_predictions: system is NULL");
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_vicarious_reward_heartbeat("mirror_vicar_vicarious_reward_dec", 0.0f);


    nimcp_mutex_lock(system->mutex);

    float decay = expf(-system->config.prediction_decay * dt_sec);

    for (uint32_t i = 0; i < system->association_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->association_count > 256) {
            mirror_vicarious_reward_heartbeat("mirror_vicar_loop",
                             (float)(i + 1) / (float)system->association_count);
        }

        system->associations[i].expected_reward *= decay;
        system->associations[i].value_confidence *= decay;
    }

    nimcp_mutex_unlock(system->mutex);
}

//=============================================================================
// Dopamine Signal API
//=============================================================================

float vicarious_reward_get_dopamine(const vicarious_reward_system_t* system) {
    /* Phase 8: Heartbeat at operation start */
    mirror_vicarious_reward_heartbeat("mirror_vicar_vicarious_reward_get", 0.0f);


    return system ? system->current_dopamine : 0.5f;
}

void vicarious_reward_inject_direct(
    vicarious_reward_system_t* system,
    float direct_reward
) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "vicarious_reward_inject_direct: system is NULL");
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_vicarious_reward_heartbeat("mirror_vicar_vicarious_reward_inj", 0.0f);


    nimcp_mutex_lock(system->mutex);

    /* Direct reward has full effect */
    float dopamine_change = nimcp_clampf(direct_reward, -0.5f, 0.5f);
    system->current_dopamine = nimcp_clampf(
        system->current_dopamine + dopamine_change,
        0.0f, 1.0f
    );

    nimcp_mutex_unlock(system->mutex);
}

//=============================================================================
// Social Modulation API
//=============================================================================

float vicarious_reward_social_modulation(
    const vicarious_reward_system_t* system,
    uint32_t agent_id
) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "vicarious_reward_social_modulation: system is NULL");
        return 1.0f;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_vicarious_reward_heartbeat("mirror_vicar_vicarious_reward_soc", 0.0f);


    const vicarious_agent_state_t* agent = NULL;
    for (uint32_t i = 0; i < system->agent_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->agent_count > 256) {
            mirror_vicarious_reward_heartbeat("mirror_vicar_loop",
                             (float)(i + 1) / (float)system->agent_count);
        }

        if (system->agents[i].agent_id == agent_id) {
            agent = &system->agents[i];
            break;
        }
    }

    return compute_social_modulation(system, agent);
}

bool vicarious_reward_is_schadenfreude(
    const vicarious_reward_system_t* system,
    uint32_t agent_id,
    float observed_negative_reward
) {
    if (!system || !system->config.enable_schadenfreude) {
        if (!system) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "vicarious_reward_is_schadenfreude: system is NULL");
            return false;
        }
        return false;
    }
    if (observed_negative_reward >= 0.0f) return false;

    /* Phase 8: Heartbeat at operation start */
    mirror_vicarious_reward_heartbeat("mirror_vicar_vicarious_reward_is_", 0.0f);


    const vicarious_agent_state_t* agent = NULL;
    for (uint32_t i = 0; i < system->agent_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->agent_count > 256) {
            mirror_vicarious_reward_heartbeat("mirror_vicar_loop",
                             (float)(i + 1) / (float)system->agent_count);
        }

        if (system->agents[i].agent_id == agent_id) {
            agent = &system->agents[i];
            break;
        }
    }

    if (!agent) {
        return false;
    }

    return (agent->relation == SOCIAL_RELATION_COMPETITOR ||
            agent->relation == SOCIAL_RELATION_OUTGROUP);
}

bool vicarious_reward_is_envy(
    const vicarious_reward_system_t* system,
    uint32_t agent_id,
    float observed_positive_reward
) {
    if (!system || !system->config.enable_envy) {
        if (!system) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "vicarious_reward_is_envy: system is NULL");
            return false;
        }
        return false;
    }
    if (observed_positive_reward <= 0.0f) return false;

    /* Phase 8: Heartbeat at operation start */
    mirror_vicarious_reward_heartbeat("mirror_vicar_vicarious_reward_is_", 0.0f);


    const vicarious_agent_state_t* agent = NULL;
    for (uint32_t i = 0; i < system->agent_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->agent_count > 256) {
            mirror_vicarious_reward_heartbeat("mirror_vicar_loop",
                             (float)(i + 1) / (float)system->agent_count);
        }

        if (system->agents[i].agent_id == agent_id) {
            agent = &system->agents[i];
            break;
        }
    }

    if (!agent) {
        return false;
    }

    return (agent->relation == SOCIAL_RELATION_COMPETITOR);
}

//=============================================================================
// SIMD Optimization API
//=============================================================================

void vicarious_reward_simd_social_mod(
    const float* distances,
    const float* familiarities,
    const float* relation_weights,
    float* modulations,
    uint32_t count,
    const vicarious_reward_config_t* config
) {
    if (!config) return;

    /* Scalar implementation - could be vectorized */
    /* Phase 8: Heartbeat at operation start */
    mirror_vicarious_reward_heartbeat("mirror_vicar_vicarious_reward_sim", 0.0f);


    for (uint32_t i = 0; i < count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && count > 256) {
            mirror_vicarious_reward_heartbeat("mirror_vicar_loop",
                             (float)(i + 1) / (float)count);
        }

        float distance_mod = expf(-distances[i] * config->social_distance_decay);
        float familiarity_mod = 1.0f + familiarities[i] * config->familiarity_boost;
        modulations[i] = relation_weights[i] * distance_mod * familiarity_mod;
    }
}

void vicarious_reward_simd_rpe(
    const float* observed_rewards,
    const float* predicted_rewards,
    const float* modulations,
    float* rpes,
    uint32_t count
) {
    /* Phase 8: Heartbeat at operation start */
    mirror_vicarious_reward_heartbeat("mirror_vicar_vicarious_reward_sim", 0.0f);


    for (uint32_t i = 0; i < count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && count > 256) {
            mirror_vicarious_reward_heartbeat("mirror_vicar_loop",
                             (float)(i + 1) / (float)count);
        }

        rpes[i] = (observed_rewards[i] - predicted_rewards[i]) * modulations[i];
    }
}

//=============================================================================
// Bio-Async Integration
//=============================================================================

bool vicarious_reward_register_bio_async(vicarious_reward_system_t* system) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vicarious_reward_register_bio_async: system is NULL");
        return false;
    }

    /* Bio-async registration would go here */
    /* Phase 8: Heartbeat at operation start */
    mirror_vicarious_reward_heartbeat("mirror_vicar_vicarious_reward_reg", 0.0f);


    system->bio_async_registered = true;
    nimcp_log(LOG_LEVEL_DEBUG, "Vicarious Reward: Registered with bio-async");
    return true;
}

void vicarious_reward_unregister_bio_async(vicarious_reward_system_t* system) {
    if (!system) return;
    /* Phase 8: Heartbeat at operation start */
    mirror_vicarious_reward_heartbeat("mirror_vicar_vicarious_reward_unr", 0.0f);


    system->bio_async_registered = false;
    nimcp_log(LOG_LEVEL_DEBUG, "Vicarious Reward: Unregistered from bio-async");
}

//=============================================================================
// Statistics API
//=============================================================================

bool vicarious_reward_get_stats(
    const vicarious_reward_system_t* system,
    vicarious_reward_stats_t* stats
) {
    if (!system || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "vicarious_reward_get_stats: required parameter is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_vicarious_reward_heartbeat("mirror_vicar_vicarious_reward_get", 0.0f);


    nimcp_mutex_lock(((vicarious_reward_system_t*)system)->mutex);
    *stats = system->stats;
    nimcp_mutex_unlock(((vicarious_reward_system_t*)system)->mutex);

    return true;
}

void vicarious_reward_reset_stats(vicarious_reward_system_t* system) {
    if (!system) return;

    /* Phase 8: Heartbeat at operation start */
    mirror_vicarious_reward_heartbeat("mirror_vicar_vicarious_reward_res", 0.0f);


    nimcp_mutex_lock(system->mutex);
    memset(&system->stats, 0, sizeof(vicarious_reward_stats_t));
    nimcp_mutex_unlock(system->mutex);
}

/* ============================================================================
 * Phase 8: Instance-level health agent setter
 * ============================================================================ */
void mirror_vicarious_reward_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_mirror_vicarious_reward_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training stubs
 * ============================================================================ */
int mirror_vicarious_reward_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "mirror_vicarious_reward_training_begin: NULL argument");
        return -1;
    }
    mirror_vicarious_reward_heartbeat_instance(NULL, "mirror_vicarious_reward_training_begin", 0.0f);
    return 0;
}

int mirror_vicarious_reward_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "mirror_vicarious_reward_training_end: NULL argument");
        return -1;
    }
    mirror_vicarious_reward_heartbeat_instance(NULL, "mirror_vicarious_reward_training_end", 1.0f);
    return 0;
}

int mirror_vicarious_reward_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "mirror_vicarious_reward_training_step: NULL argument");
        return -1;
    }
    mirror_vicarious_reward_heartbeat_instance(NULL, "mirror_vicarious_reward_training_step", progress);
    return 0;
}
