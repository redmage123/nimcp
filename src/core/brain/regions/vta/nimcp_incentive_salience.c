/**
 * @file nimcp_incentive_salience.c
 * @brief Incentive salience ("wanting") implementation
 * @date 2026-01-11
 */

#include "core/brain/regions/vta/nimcp_incentive_salience.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_learning_constants.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE_MESH_ONLY(incentive_salience, MESH_ADAPTER_CATEGORY_COGNITIVE)

static nimcp_motivation_level_t classify_motivation(float wanting) {
    if (wanting < 0.1f) return MOTIVATION_NONE;
    if (wanting < 0.3f) return MOTIVATION_LOW;
    if (wanting < 0.6f) return MOTIVATION_MODERATE;
    if (wanting < 0.85f) return MOTIVATION_HIGH;
    return MOTIVATION_INTENSE;
}

/*=============================================================================
 * Lifecycle API
 *===========================================================================*/

nimcp_salience_config_t nimcp_salience_default_config(void) {
    nimcp_salience_config_t config;
    memset(&config, 0, sizeof(config));

    config.da_wanting_gain = SALIENCE_DEFAULT_DA_GAIN;
    config.wanting_baseline = 0.3f;
    config.wanting_decay = 0.01f;
    config.effort_sensitivity = SALIENCE_DEFAULT_EFFORT_K;
    config.delay_sensitivity = SALIENCE_DEFAULT_DELAY_K;
    config.urgency_boost = 0.2f;
    config.cue_salience_lr = NIMCP_LEARNING_RATE_COARSE;

    config.effort.physical_cost_k = 0.5f;
    config.effort.cognitive_cost_k = 0.3f;
    config.effort.delay_discount_k = 0.1f;
    config.effort.probability_weight = 0.9f;
    config.effort.loss_aversion = 2.0f;

    return config;
}

int nimcp_salience_init(
    nimcp_salience_system_t* system,
    const nimcp_salience_config_t* config
) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "system is NULL");

        return -1;
    }

    memset(system, 0, sizeof(*system));

    if (config) {
        system->config = *config;
    } else {
        system->config = nimcp_salience_default_config();
    }

    /* Initialize state */
    system->state.wanting = system->config.wanting_baseline;
    system->state.liking = 0.5f;
    system->state.motivation = system->config.wanting_baseline;
    system->state.vigor = 0.5f;
    system->state.level = classify_motivation(system->state.wanting);

    system->da_baseline = 50.0f;  /* Default baseline DA */
    system->initialized = true;

    return 0;
}

int nimcp_salience_shutdown(nimcp_salience_system_t* system) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "system is NULL");

        return -1;
    }

    memset(system, 0, sizeof(*system));
    return 0;
}

int nimcp_salience_reset(nimcp_salience_system_t* system) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "system is NULL");

        return -1;
    }

    nimcp_salience_config_t config = system->config;
    nimcp_salience_shutdown(system);
    return nimcp_salience_init(system, &config);
}

/*=============================================================================
 * Core Salience API
 *===========================================================================*/

int nimcp_salience_update(
    nimcp_salience_system_t* system,
    float da_level,
    float dt
) {
    if (!system || !system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_salience_update: required parameter is NULL (system, system->initialized)");
        return -1;
    }

    system->current_da = da_level;

    /* Update wanting based on DA */
    float wanting;
    nimcp_salience_compute_wanting(system, da_level, &wanting);

    /* Apply cue-triggered wanting boost */
    float cue_wanting = 0.0f;
    for (uint32_t i = 0; i < system->num_cues; i++) {
        if (system->cues[i].present) {
            cue_wanting += system->cues[i].salience * system->cues[i].da_boost;
        }
    }
    wanting += cue_wanting * 0.3f;

    /* Apply goal urgency */
    if (system->active_goal_id < system->num_goals) {
        nimcp_goal_t* goal = &system->goals[system->active_goal_id];
        if (goal->active) {
            wanting += goal->urgency * system->config.urgency_boost;
        }
    }

    /* Smooth update */
    float tau = 100.0f;
    system->state.wanting += (wanting - system->state.wanting) * (dt / tau);
    system->state.wanting = nimcp_clampf(system->state.wanting, 0.0f, 1.0f);

    /* Update vigor */
    system->state.vigor = system->state.wanting * (da_level / system->da_baseline);
    system->state.vigor = nimcp_clampf(system->state.vigor, 0.0f, 1.0f);

    /* Update overall motivation */
    system->state.motivation = (system->state.wanting + system->state.vigor) * 0.5f;

    /* Update level classification */
    system->state.level = classify_motivation(system->state.wanting);

    /* Decay cue salience for non-present cues */
    float decay = expf(-dt * system->config.wanting_decay);
    for (uint32_t i = 0; i < system->num_cues; i++) {
        if (!system->cues[i].present) {
            system->cues[i].da_boost *= decay;
        }
    }

    system->update_count++;
    system->total_wanting += system->state.wanting * dt;

    return 0;
}

int nimcp_salience_compute_wanting(
    nimcp_salience_system_t* system,
    float da_level,
    float* wanting
) {
    if (!system || !wanting) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_salience_compute_wanting: required parameter is NULL (system, wanting)");
        return -1;
    }

    if (!system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_salience_compute_wanting: system->initialized is NULL");
        return -1;
    }

    /* Wanting scales with DA relative to baseline */
    float da_ratio = da_level / system->da_baseline;

    /* Non-linear mapping: wanting increases faster at higher DA */
    float raw_wanting = system->config.wanting_baseline +
                        (da_ratio - 1.0f) * system->config.da_wanting_gain;

    *wanting = nimcp_clampf(raw_wanting, 0.0f, 1.0f);
    return 0;
}

int nimcp_salience_get_wanting(
    nimcp_salience_system_t* system,
    float* wanting
) {
    if (!system || !wanting) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_salience_get_wanting: required parameter is NULL (system, wanting)");
        return -1;
    }

    if (!system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_salience_get_wanting: system->initialized is NULL");
        return -1;
    }

    *wanting = system->state.wanting;
    return 0;
}

int nimcp_salience_get_motivation(
    nimcp_salience_system_t* system,
    nimcp_motivation_level_t* level
) {
    if (!system || !level) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_salience_get_motivation: required parameter is NULL (system, level)");
        return -1;
    }

    if (!system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_salience_get_motivation: system->initialized is NULL");
        return -1;
    }

    *level = system->state.level;
    return 0;
}

int nimcp_salience_get_vigor(
    nimcp_salience_system_t* system,
    float* vigor
) {
    if (!system || !vigor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_salience_get_vigor: required parameter is NULL (system, vigor)");
        return -1;
    }

    if (!system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_salience_get_vigor: system->initialized is NULL");
        return -1;
    }

    *vigor = system->state.vigor;
    return 0;
}

/*=============================================================================
 * Goal API
 *===========================================================================*/

int nimcp_salience_add_goal(
    nimcp_salience_system_t* system,
    nimcp_goal_type_t type,
    float value,
    float effort,
    float delay,
    uint32_t* goal_id
) {
    if (!system || !goal_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_salience_add_goal: required parameter is NULL (system, goal_id)");
        return -1;
    }

    if (!system->initialized || system->num_goals >= SALIENCE_MAX_GOALS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_salience_add_goal: system->initialized is NULL");
        return -1;
    }

    nimcp_goal_t* goal = &system->goals[system->num_goals];
    memset(goal, 0, sizeof(*goal));

    goal->id = system->num_goals;
    goal->type = type;
    goal->value = value;
    goal->effort_required = effort;
    goal->time_to_reward = delay;
    goal->probability = 0.8f;  /* Default success probability */
    goal->distance = 0.0f;
    goal->urgency = 0.0f;
    goal->active = true;

    *goal_id = goal->id;
    system->num_goals++;

    return 0;
}

int nimcp_salience_remove_goal(
    nimcp_salience_system_t* system,
    uint32_t goal_id
) {
    if (!system || !system->initialized || goal_id >= system->num_goals) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_salience_remove_goal: required parameter is NULL (system, system->initialized)");
        return -1;
    }

    system->goals[goal_id].active = false;
    return 0;
}

int nimcp_salience_set_active_goal(
    nimcp_salience_system_t* system,
    uint32_t goal_id
) {
    if (!system || !system->initialized || goal_id >= system->num_goals) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_salience_set_active_goal: required parameter is NULL (system, system->initialized)");
        return -1;
    }

    system->active_goal_id = goal_id;
    return 0;
}

int nimcp_salience_update_goal_progress(
    nimcp_salience_system_t* system,
    uint32_t goal_id,
    float progress
) {
    if (!system || !system->initialized || goal_id >= system->num_goals) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_salience_update_goal_progress: required parameter is NULL (system, system->initialized)");
        return -1;
    }

    system->goals[goal_id].distance = nimcp_clampf(progress, 0.0f, 1.0f);
    return 0;
}

int nimcp_salience_goal_achieved(
    nimcp_salience_system_t* system,
    uint32_t goal_id,
    float actual_reward
) {
    if (!system || !system->initialized || goal_id >= system->num_goals) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_salience_goal_achieved: required parameter is NULL (system, system->initialized)");
        return -1;
    }

    nimcp_goal_t* goal = &system->goals[goal_id];
    goal->active = false;
    goal->distance = 1.0f;

    /* Update statistics */
    system->goals_achieved++;
    system->total_effort_expended += goal->effort_required;

    /* Liking from goal achievement */
    system->state.liking = nimcp_clampf(actual_reward / goal->value, 0.0f, 1.0f);

    return 0;
}

int nimcp_salience_get_goal_wanting(
    nimcp_salience_system_t* system,
    uint32_t goal_id,
    float* wanting
) {
    if (!system || !wanting) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_salience_get_goal_wanting: required parameter is NULL (system, wanting)");
        return -1;
    }

    if (!system->initialized || goal_id >= system->num_goals) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_salience_get_goal_wanting: system->initialized is NULL");
        return -1;
    }

    nimcp_goal_t* goal = &system->goals[goal_id];
    if (!goal->active) {
        *wanting = 0.0f;
        return 0;
    }

    /* Compute utility to derive wanting */
    nimcp_utility_result_t result;
    nimcp_salience_compute_utility(
        system,
        goal->value,
        goal->effort_required * (1.0f - goal->distance),
        goal->time_to_reward * (1.0f - goal->distance),
        goal->probability,
        &result
    );

    /* Wanting scaled by DA and utility */
    float da_ratio = system->current_da / system->da_baseline;
    *wanting = result.net_utility * da_ratio * system->state.wanting;
    *wanting = nimcp_clampf(*wanting, 0.0f, 1.0f);

    return 0;
}

/*=============================================================================
 * Effort-Utility API
 *===========================================================================*/

int nimcp_salience_compute_utility(
    nimcp_salience_system_t* system,
    float reward_value,
    float effort_required,
    float delay,
    float probability,
    nimcp_utility_result_t* result
) {
    if (!system || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_salience_compute_utility: required parameter is NULL (system, result)");
        return -1;
    }

    if (!system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_salience_compute_utility: system->initialized is NULL");
        return -1;
    }

    memset(result, 0, sizeof(*result));

    /* Subjective value (with probability weighting) */
    float pw = powf(probability, system->config.effort.probability_weight);
    result->reward_value = reward_value * pw;

    /* Effort cost */
    result->effort_cost = effort_required * system->config.effort_sensitivity;

    /* Delay discounting (hyperbolic) */
    float k = system->config.delay_sensitivity;
    result->delay_cost = reward_value * (1.0f - 1.0f / (1.0f + k * delay / 1000.0f));

    /* Probability-adjusted value */
    result->probability_adjusted = result->reward_value;

    /* Net utility */
    result->net_utility = result->reward_value - result->effort_cost - result->delay_cost;

    /* Worth pursuing if positive utility */
    result->worth_pursuing = result->net_utility > 0.0f;

    return 0;
}

int nimcp_salience_compute_effort_cost(
    nimcp_salience_system_t* system,
    float effort,
    nimcp_effort_type_t type,
    float* cost
) {
    if (!system || !cost) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_salience_compute_effort_cost: required parameter is NULL (system, cost)");
        return -1;
    }

    if (!system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_salience_compute_effort_cost: system->initialized is NULL");
        return -1;
    }

    float k;
    switch (type) {
        case EFFORT_PHYSICAL:
            k = system->config.effort.physical_cost_k;
            break;
        case EFFORT_COGNITIVE:
            k = system->config.effort.cognitive_cost_k;
            break;
        case EFFORT_SOCIAL:
            k = (system->config.effort.physical_cost_k + system->config.effort.cognitive_cost_k) * 0.5f;
            break;
        case EFFORT_TEMPORAL:
            k = system->config.effort.delay_discount_k;
            break;
        default:
            k = system->config.effort_sensitivity;
    }

    *cost = effort * k;
    return 0;
}

int nimcp_salience_apply_delay_discount(
    nimcp_salience_system_t* system,
    float value,
    float delay,
    float* discounted
) {
    if (!system || !discounted) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_salience_apply_delay_discount: required parameter is NULL (system, discounted)");
        return -1;
    }

    if (!system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_salience_apply_delay_discount: system->initialized is NULL");
        return -1;
    }

    /* Hyperbolic discounting */
    float k = system->config.delay_sensitivity;
    *discounted = value / (1.0f + k * delay / 1000.0f);

    return 0;
}

int nimcp_salience_is_worth_pursuing(
    nimcp_salience_system_t* system,
    float reward,
    float effort,
    float delay,
    bool* worth_it
) {
    if (!system || !worth_it) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_salience_is_worth_pursuing: required parameter is NULL (system, worth_it)");
        return -1;
    }

    if (!system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_salience_is_worth_pursuing: system->initialized is NULL");
        return -1;
    }

    nimcp_utility_result_t result;
    nimcp_salience_compute_utility(system, reward, effort, delay, 1.0f, &result);
    *worth_it = result.worth_pursuing;

    return 0;
}

/*=============================================================================
 * Cue API
 *===========================================================================*/

int nimcp_salience_add_cue(
    nimcp_salience_system_t* system,
    float initial_salience,
    uint32_t* cue_id
) {
    if (!system || !cue_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_salience_add_cue: required parameter is NULL (system, cue_id)");
        return -1;
    }

    if (!system->initialized || system->num_cues >= SALIENCE_MAX_CUES) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_salience_add_cue: system->initialized is NULL");
        return -1;
    }

    nimcp_salience_cue_t* cue = &system->cues[system->num_cues];
    memset(cue, 0, sizeof(*cue));

    cue->id = system->num_cues;
    cue->salience = nimcp_clampf(initial_salience, 0.0f, 1.0f);
    cue->da_boost = initial_salience * 0.5f;
    cue->decay_rate = 0.01f;

    *cue_id = cue->id;
    system->num_cues++;

    return 0;
}

int nimcp_salience_cue_present(
    nimcp_salience_system_t* system,
    uint32_t cue_id
) {
    if (!system || !system->initialized || cue_id >= system->num_cues) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_salience_cue_present: required parameter is NULL (system, system->initialized)");
        return -1;
    }

    system->cues[cue_id].present = true;
    system->cues[cue_id].association_count++;
    return 0;
}

int nimcp_salience_cue_absent(
    nimcp_salience_system_t* system,
    uint32_t cue_id
) {
    if (!system || !system->initialized || cue_id >= system->num_cues) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_salience_cue_absent: required parameter is NULL (system, system->initialized)");
        return -1;
    }

    system->cues[cue_id].present = false;
    return 0;
}

int nimcp_salience_update_cue(
    nimcp_salience_system_t* system,
    uint32_t cue_id,
    float reward_received
) {
    if (!system || !system->initialized || cue_id >= system->num_cues) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_salience_update_cue: required parameter is NULL (system, system->initialized)");
        return -1;
    }

    nimcp_salience_cue_t* cue = &system->cues[cue_id];

    /* Update salience based on reward */
    float lr = system->config.cue_salience_lr;
    float delta = reward_received - cue->salience;
    cue->salience += lr * delta;
    cue->salience = nimcp_clampf(cue->salience, 0.0f, 1.0f);

    /* Update DA boost */
    cue->da_boost = cue->salience * 0.5f;

    return 0;
}

int nimcp_salience_get_cue_wanting(
    nimcp_salience_system_t* system,
    float* cue_wanting
) {
    if (!system || !cue_wanting) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_salience_get_cue_wanting: required parameter is NULL (system, cue_wanting)");
        return -1;
    }

    if (!system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_salience_get_cue_wanting: system->initialized is NULL");
        return -1;
    }

    float total = 0.0f;
    for (uint32_t i = 0; i < system->num_cues; i++) {
        if (system->cues[i].present) {
            total += system->cues[i].salience;
        }
    }

    *cue_wanting = nimcp_clampf(total, 0.0f, 1.0f);
    return 0;
}

/*=============================================================================
 * Liking API
 *===========================================================================*/

int nimcp_salience_signal_liking(
    nimcp_salience_system_t* system,
    float liking
) {
    if (!system || !system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_salience_signal_liking: required parameter is NULL (system, system->initialized)");
        return -1;
    }

    system->state.liking = nimcp_clampf(liking, 0.0f, 1.0f);
    return 0;
}

int nimcp_salience_get_liking(
    nimcp_salience_system_t* system,
    float* liking
) {
    if (!system || !liking) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_salience_get_liking: required parameter is NULL (system, liking)");
        return -1;
    }

    if (!system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_salience_get_liking: system->initialized is NULL");
        return -1;
    }

    *liking = system->state.liking;
    return 0;
}

int nimcp_salience_get_wanting_liking_ratio(
    nimcp_salience_system_t* system,
    float* ratio
) {
    if (!system || !ratio) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_salience_get_wanting_liking_ratio: required parameter is NULL (system, ratio)");
        return -1;
    }

    if (!system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_salience_get_wanting_liking_ratio: system->initialized is NULL");
        return -1;
    }

    if (system->state.liking > 0.001f) {
        *ratio = system->state.wanting / system->state.liking;
    } else {
        *ratio = system->state.wanting > 0.0f ? 10.0f : 1.0f;
    }

    return 0;
}

/*=============================================================================
 * State API
 *===========================================================================*/

int nimcp_salience_get_state(
    nimcp_salience_system_t* system,
    nimcp_salience_state_t* state
) {
    if (!system || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_salience_get_state: required parameter is NULL (system, state)");
        return -1;
    }

    if (!system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_salience_get_state: system->initialized is NULL");
        return -1;
    }

    *state = system->state;
    return 0;
}
