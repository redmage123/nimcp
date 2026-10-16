//=============================================================================
// nimcp_bg_vigor.c - Vigor and Effort Modulation System Implementation
//=============================================================================
/**
 * @file nimcp_bg_vigor.c
 * @brief Implementation of vigor/effort modulation with bidirectional data flow
 */

#include "core/brain/subcortical/nimcp_bg_vigor.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/math/nimcp_math_helpers.h"

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Vigor system internal state
 */
struct bgv_system {
    bgv_config_t config;                /**< Configuration */

    /* Action registry */
    bgv_action_vigor_t* actions;        /**< Action array */
    uint32_t num_actions;               /**< Number of registered actions */
    uint32_t max_actions;               /**< Maximum actions */

    /* Global state */
    bgv_state_t state;                  /**< Current vigor state */
    float dopamine_level;               /**< Current dopamine [0-1] */
    float motivation_level;             /**< Current motivation [0-1] */
    float urgency_level;                /**< Current urgency [0-1] */
    float fatigue_level;                /**< Current fatigue [0-1] */
    float reward_proximity;             /**< Proximity to reward [0-1] */

    /* Statistics */
    uint64_t total_computations;        /**< Total vigor computations */
    float cumulative_vigor;             /**< Sum of computed vigors */
    float cumulative_effort;            /**< Sum of computed efforts */
};

//=============================================================================
// Helper Functions
//=============================================================================

/**
 * @brief Find action by ID
 */
static bgv_action_vigor_t* bgv_find_action(bgv_system_t* system, uint32_t action_id) {
    if (!system || !system->actions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bgv_find_action: required parameter is NULL (system, system->actions)");
        return NULL;
    }

    for (uint32_t i = 0; i < system->num_actions; i++) {
        if (system->actions[i].action_id == action_id) {
            return &system->actions[i];
        }
    }
    return NULL;
}

/**
 * @brief Find action by ID (const version)
 */
static const bgv_action_vigor_t* bgv_find_action_const(const bgv_system_t* system,
                                                         uint32_t action_id) {
    if (!system || !system->actions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bgv_find_action: required parameter is NULL (system, system->actions)");
        return NULL;
    }

    for (uint32_t i = 0; i < system->num_actions; i++) {
        if (system->actions[i].action_id == action_id) {
            return &system->actions[i];
        }
    }
    return NULL;
}

/**
 * @brief Compute total effort cost
 */
static float bgv_compute_total_effort(const bgv_effort_t* effort) {
    /* Weighted combination of effort types */
    float total = effort->physical_cost * 0.4f +
                  effort->cognitive_cost * 0.3f +
                  effort->emotional_cost * 0.2f +
                  effort->temporal_cost * 0.1f;
    return nimcp_clampf(total, 0.0f, 1.0f);
}

/**
 * @brief Update global vigor state based on current levels
 */
static void bgv_update_state(bgv_system_t* system) {
    float effective_vigor = system->dopamine_level * (1.0f - system->fatigue_level);

    if (effective_vigor < 0.2f) {
        system->state = BGV_STATE_BRADYKINETIC;
    } else if (effective_vigor < 0.4f) {
        system->state = BGV_STATE_REDUCED;
    } else if (effective_vigor > 0.9f) {
        system->state = BGV_STATE_HYPERKINETIC;
    } else if (effective_vigor > 0.7f && system->motivation_level > 0.7f) {
        system->state = BGV_STATE_ENHANCED;
    } else {
        system->state = BGV_STATE_NORMAL;
    }
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

void bgv_default_config(bgv_config_t* config) {
    if (!config) return;

    config->max_actions = BGV_MAX_ACTIONS;
    config->base_vigor = BGV_DEFAULT_VIGOR;
    config->dopamine_sensitivity = 1.0f;
    config->motivation_weight = 0.3f;
    config->fatigue_rate = 0.1f;
    config->recovery_rate = 0.05f;
    config->effort_sensitivity = 0.5f;
    config->enable_effort_discounting = true;
    config->enable_fatigue = true;
}

bgv_system_t* bgv_create(const bgv_config_t* config) {
    bgv_config_t default_config;
    if (!config) {
        bgv_default_config(&default_config);
        config = &default_config;
    }

    bgv_system_t* system = (bgv_system_t*)nimcp_calloc(1, sizeof(bgv_system_t));
    if (!system) {
        NIMCP_LOGGING_ERROR("Failed to allocate vigor system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bgv_create: system is NULL");
        return NULL;
    }

    /* Copy configuration */
    memcpy(&system->config, config, sizeof(bgv_config_t));

    /* Allocate action array */
    system->max_actions = config->max_actions;
    system->actions = (bgv_action_vigor_t*)nimcp_calloc(system->max_actions,
                                                   sizeof(bgv_action_vigor_t));
    if (!system->actions) {
        NIMCP_LOGGING_ERROR("Failed to allocate action array");
        nimcp_free(system);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bgv_create: system->actions is NULL");
        return NULL;
    }

    /* Initialize state */
    system->state = BGV_STATE_NORMAL;
    system->dopamine_level = 0.5f;
    system->motivation_level = 0.5f;
    system->urgency_level = 0.0f;
    system->fatigue_level = 0.0f;
    system->reward_proximity = 0.0f;
    system->num_actions = 0;

    NIMCP_LOGGING_DEBUG("Vigor system created: max_actions=%u", system->max_actions);
    return system;
}

void bgv_destroy(bgv_system_t* system) {
    if (!system) return;

    if (system->actions) {
        nimcp_free(system->actions);
    }
    nimcp_free(system);

    NIMCP_LOGGING_DEBUG("Vigor system destroyed");
}

int bgv_reset(bgv_system_t* system) {
    if (!system) return NIMCP_ERROR_NULL_POINTER;

    /* Reset global state */
    system->state = BGV_STATE_NORMAL;
    system->dopamine_level = 0.5f;
    system->motivation_level = 0.5f;
    system->urgency_level = 0.0f;
    system->fatigue_level = 0.0f;
    system->reward_proximity = 0.0f;

    /* Reset action statistics */
    for (uint32_t i = 0; i < system->num_actions; i++) {
        system->actions[i].current_vigor = system->actions[i].base_vigor;
        system->actions[i].avg_vigor = system->actions[i].base_vigor;
        system->actions[i].execution_count = 0;
    }

    /* Reset statistics */
    system->total_computations = 0;
    system->cumulative_vigor = 0.0f;
    system->cumulative_effort = 0.0f;

    NIMCP_LOGGING_DEBUG("Vigor system reset");
    return NIMCP_OK;
}

//=============================================================================
// Action Registration Functions
//=============================================================================

int bgv_register_action(bgv_system_t* system,
                         uint32_t action_id,
                         float physical_cost,
                         float cognitive_cost,
                         float base_duration_ms) {
    if (!system) return NIMCP_ERROR_NULL_POINTER;

    /* Check for duplicate */
    if (bgv_find_action(system, action_id) != NULL) {
        NIMCP_LOGGING_WARN("Action %u already registered", action_id);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_ALREADY_EXISTS, "bg_vigor: error condition");
        return NIMCP_ERROR_ALREADY_EXISTS;
    }

    /* Check capacity */
    if (system->num_actions >= system->max_actions) {
        NIMCP_LOGGING_ERROR("Action registry full");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_CAPACITY_EXCEEDED, "bg_vigor: error condition");
        return NIMCP_ERROR_CAPACITY_EXCEEDED;
    }

    /* Initialize action */
    bgv_action_vigor_t* action = &system->actions[system->num_actions];
    memset(action, 0, sizeof(bgv_action_vigor_t));

    action->action_id = action_id;
    action->base_vigor = system->config.base_vigor;
    action->current_vigor = action->base_vigor;
    action->vigor_gain = 1.0f;
    action->avg_vigor = action->base_vigor;
    action->avg_duration_ms = base_duration_ms;

    /* Set effort costs */
    action->effort.action_id = action_id;
    action->effort.physical_cost = nimcp_clampf(physical_cost, 0.0f, 1.0f);
    action->effort.cognitive_cost = nimcp_clampf(cognitive_cost, 0.0f, 1.0f);
    action->effort.emotional_cost = 0.0f;
    action->effort.temporal_cost = 0.0f;
    action->effort.total_cost = bgv_compute_total_effort(&action->effort);
    action->effort.subjective_cost = action->effort.total_cost;

    system->num_actions++;

    NIMCP_LOGGING_DEBUG("Registered action %u: phys=%.2f, cog=%.2f, dur=%.1fms",
                        action_id, physical_cost, cognitive_cost, base_duration_ms);
    return NIMCP_OK;
}

int bgv_set_additional_costs(bgv_system_t* system,
                              uint32_t action_id,
                              float emotional_cost,
                              float temporal_cost) {
    if (!system) return NIMCP_ERROR_NULL_POINTER;

    bgv_action_vigor_t* action = bgv_find_action(system, action_id);
    if (!action) return NIMCP_ERROR_NOT_FOUND;

    action->effort.emotional_cost = nimcp_clampf(emotional_cost, 0.0f, 1.0f);
    action->effort.temporal_cost = nimcp_clampf(temporal_cost, 0.0f, 1.0f);
    action->effort.total_cost = bgv_compute_total_effort(&action->effort);

    return NIMCP_OK;
}

int bgv_unregister_action(bgv_system_t* system, uint32_t action_id) {
    if (!system) return NIMCP_ERROR_NULL_POINTER;

    for (uint32_t i = 0; i < system->num_actions; i++) {
        if (system->actions[i].action_id == action_id) {
            /* Shift remaining actions */
            if (i < system->num_actions - 1) {
                memmove(&system->actions[i],
                        &system->actions[i + 1],
                        (system->num_actions - i - 1) * sizeof(bgv_action_vigor_t));
            }
            system->num_actions--;
            return NIMCP_OK;
        }
    }

    return NIMCP_ERROR_NOT_FOUND;  /* Action not found - normal lookup miss */
}

//=============================================================================
// Vigor Computation Functions
//=============================================================================

int bgv_compute_vigor(bgv_system_t* system,
                       uint32_t action_id,
                       float* vigor) {
    if (!system || !vigor) return NIMCP_ERROR_NULL_POINTER;

    bgv_action_vigor_t* action = bgv_find_action(system, action_id);
    if (!action) return NIMCP_ERROR_NOT_FOUND;

    /* Base vigor starts from action's base level */
    float computed = action->base_vigor;

    /* Dopamine modulation (primary driver of vigor) */
    /* Low DA = bradykinesia, High DA = increased vigor */
    float da_factor = system->dopamine_level * system->config.dopamine_sensitivity;
    computed *= (0.5f + da_factor);

    /* Motivation enhancement */
    float motivation_boost = system->motivation_level * system->config.motivation_weight;
    computed += motivation_boost;

    /* Urgency enhancement */
    computed += system->urgency_level * 0.2f;

    /* Reward proximity enhancement (vigor increases near reward) */
    computed += system->reward_proximity * 0.15f;

    /* Fatigue reduction */
    if (system->config.enable_fatigue) {
        computed *= (1.0f - system->fatigue_level * 0.5f);
    }

    /* Effort cost reduction (high effort actions get less vigor unless highly motivated) */
    float effort_penalty = action->effort.total_cost * system->config.effort_sensitivity;
    effort_penalty *= (1.0f - system->motivation_level);  /* Motivation overcomes effort */
    computed -= effort_penalty * 0.2f;

    /* Apply action-specific gain */
    computed *= action->vigor_gain;

    /* Clamp to valid range */
    computed = nimcp_clampf(computed, BGV_MIN_VIGOR, BGV_MAX_VIGOR);

    /* Update action's current vigor */
    action->current_vigor = computed;

    /* Update statistics */
    system->total_computations++;
    system->cumulative_vigor += computed;

    *vigor = computed;
    return NIMCP_OK;
}

int bgv_compute_effort(bgv_system_t* system,
                        uint32_t action_id,
                        bgv_effort_t* effort) {
    if (!system || !effort) return NIMCP_ERROR_NULL_POINTER;

    bgv_action_vigor_t* action = bgv_find_action(system, action_id);
    if (!action) return NIMCP_ERROR_NOT_FOUND;

    /* Copy base effort costs */
    memcpy(effort, &action->effort, sizeof(bgv_effort_t));

    /* Compute subjective cost (dopamine modulates perceived effort) */
    /* High dopamine makes effort seem less costly */
    float da_discount = system->dopamine_level * 0.3f;
    effort->subjective_cost = effort->total_cost * (1.0f - da_discount);

    /* Motivation reduces subjective effort */
    effort->subjective_cost *= (1.0f - system->motivation_level * 0.2f);

    /* Fatigue increases subjective effort */
    if (system->config.enable_fatigue) {
        effort->subjective_cost *= (1.0f + system->fatigue_level * 0.3f);
    }

    effort->subjective_cost = nimcp_clampf(effort->subjective_cost, 0.0f, 1.0f);

    /* Update statistics */
    system->cumulative_effort += effort->subjective_cost;

    return NIMCP_OK;
}

float bgv_get_motor_scaling(const bgv_system_t* system, uint32_t action_id) {
    if (!system) return 1.0f;

    const bgv_action_vigor_t* action = bgv_find_action_const(system, action_id);
    if (!action) return 1.0f;

    /* Motor scaling is based on current vigor relative to base */
    float scaling = action->current_vigor / action->base_vigor;

    /* Clamp to reasonable range */
    return nimcp_clampf(scaling, 0.5f, 2.0f);
}

float bgv_predict_duration(const bgv_system_t* system, uint32_t action_id) {
    if (!system) return 0.0f;

    const bgv_action_vigor_t* action = bgv_find_action_const(system, action_id);
    if (!action) return 0.0f;

    /* Higher vigor = shorter duration */
    /* Duration inversely proportional to vigor */
    if (action->current_vigor <= 0.0f) return action->avg_duration_ms;

    float vigor_ratio = action->base_vigor / action->current_vigor;
    return action->avg_duration_ms * vigor_ratio;
}

//=============================================================================
// Modulation Functions
//=============================================================================

int bgv_set_dopamine(bgv_system_t* system, float dopamine) {
    if (!system) return NIMCP_ERROR_NULL_POINTER;

    system->dopamine_level = nimcp_clampf(dopamine, 0.0f, 1.0f);
    bgv_update_state(system);

    return NIMCP_OK;
}

int bgv_set_motivation(bgv_system_t* system, float motivation) {
    if (!system) return NIMCP_ERROR_NULL_POINTER;

    system->motivation_level = nimcp_clampf(motivation, 0.0f, 1.0f);
    bgv_update_state(system);

    return NIMCP_OK;
}

int bgv_set_urgency(bgv_system_t* system, float urgency) {
    if (!system) return NIMCP_ERROR_NULL_POINTER;

    system->urgency_level = nimcp_clampf(urgency, 0.0f, 1.0f);

    return NIMCP_OK;
}

int bgv_set_fatigue(bgv_system_t* system, float fatigue) {
    if (!system) return NIMCP_ERROR_NULL_POINTER;

    system->fatigue_level = nimcp_clampf(fatigue, 0.0f, 1.0f);
    bgv_update_state(system);

    return NIMCP_OK;
}

int bgv_set_reward_proximity(bgv_system_t* system, float proximity) {
    if (!system) return NIMCP_ERROR_NULL_POINTER;

    system->reward_proximity = nimcp_clampf(proximity, 0.0f, 1.0f);

    return NIMCP_OK;
}

//=============================================================================
// Bidirectional Data Flow Functions
//=============================================================================

int bgv_process_bidir(bgv_system_t* system, bgv_bidir_data_t* data) {
    if (!system || !data) return NIMCP_ERROR_NULL_POINTER;

    /* Process inputs */
    system->dopamine_level = nimcp_clampf(data->dopamine_level, 0.0f, 1.0f);
    system->motivation_level = nimcp_clampf(data->motivation_signal, 0.0f, 1.0f);
    system->urgency_level = nimcp_clampf(data->urgency_signal, 0.0f, 1.0f);
    system->reward_proximity = nimcp_clampf(data->reward_proximity, 0.0f, 1.0f);

    if (data->fatigue_input > 0.0f) {
        system->fatigue_level = nimcp_clampf(data->fatigue_input, 0.0f, 1.0f);
    }

    /* Update global state */
    bgv_update_state(system);

    /* Compute vigor for specified action */
    float vigor = 0.0f;
    int ret = bgv_compute_vigor(system, data->action_id, &vigor);
    if (ret != NIMCP_OK) {
        /* Action not found - use default values */
        data->computed_vigor = system->config.base_vigor *
                               (0.5f + system->dopamine_level * 0.5f);
        data->effort_cost = 0.5f;
        data->motor_scaling = 1.0f;
        data->predicted_duration_ms = 100.0f;
        data->vigor_state = system->state;
        data->action_recommended = true;
        return NIMCP_OK;
    }

    /* Set vigor output */
    data->computed_vigor = vigor;

    /* Compute effort if requested */
    if (data->compute_effort) {
        bgv_effort_t effort;
        if (bgv_compute_effort(system, data->action_id, &effort) == NIMCP_OK) {
            data->effort_cost = effort.subjective_cost;
        } else {
            data->effort_cost = 0.5f;
        }
    } else {
        const bgv_action_vigor_t* action = bgv_find_action_const(system, data->action_id);
        data->effort_cost = action ? action->effort.subjective_cost : 0.5f;
    }

    /* Set motor scaling */
    data->motor_scaling = bgv_get_motor_scaling(system, data->action_id);

    /* Set predicted duration */
    data->predicted_duration_ms = bgv_predict_duration(system, data->action_id);

    /* Set vigor state */
    data->vigor_state = system->state;

    /* Determine if action is worth the effort */
    /* Action recommended if subjective benefit exceeds cost */
    float benefit = data->computed_vigor * (1.0f + data->reward_proximity);
    data->action_recommended = (benefit > data->effort_cost * 0.5f);

    return NIMCP_OK;
}

float bgv_get_effort_benefit_ratio(const bgv_system_t* system,
                                    uint32_t action_id,
                                    float expected_reward) {
    if (!system) return 0.0f;

    const bgv_action_vigor_t* action = bgv_find_action_const(system, action_id);
    if (!action) return 0.0f;

    /* Avoid division by zero */
    if (action->effort.subjective_cost <= 0.001f) {
        return expected_reward * 10.0f;  /* Very favorable if no effort */
    }

    return expected_reward / action->effort.subjective_cost;
}

//=============================================================================
// Update Functions
//=============================================================================

int bgv_step(bgv_system_t* system, float dt_ms) {
    if (!system) return NIMCP_ERROR_NULL_POINTER;

    /* Process natural recovery if fatigued */
    if (system->config.enable_fatigue && system->fatigue_level > 0.0f) {
        float recovery = system->config.recovery_rate * (dt_ms / 1000.0f);
        system->fatigue_level = nimcp_clampf(system->fatigue_level - recovery, 0.0f, 1.0f);
    }

    /* Update vigor state */
    bgv_update_state(system);

    return NIMCP_OK;
}

int bgv_update_action_stats(bgv_system_t* system,
                             uint32_t action_id,
                             float actual_vigor,
                             float actual_duration_ms) {
    if (!system) return NIMCP_ERROR_NULL_POINTER;

    bgv_action_vigor_t* action = bgv_find_action(system, action_id);
    if (!action) return NIMCP_ERROR_NOT_FOUND;

    /* Update running averages */
    action->execution_count++;
    float alpha = 0.1f;  /* Learning rate for averages */

    action->avg_vigor = action->avg_vigor * (1.0f - alpha) + actual_vigor * alpha;
    action->avg_duration_ms = action->avg_duration_ms * (1.0f - alpha) +
                              actual_duration_ms * alpha;

    return NIMCP_OK;
}

int bgv_apply_fatigue(bgv_system_t* system, uint32_t action_id) {
    if (!system) return NIMCP_ERROR_NULL_POINTER;
    if (!system->config.enable_fatigue) return NIMCP_OK;

    const bgv_action_vigor_t* action = bgv_find_action_const(system, action_id);
    if (!action) return NIMCP_ERROR_NOT_FOUND;

    /* Fatigue increases based on effort cost */
    float fatigue_increase = action->effort.total_cost * system->config.fatigue_rate;
    system->fatigue_level = nimcp_clampf(system->fatigue_level + fatigue_increase, 0.0f, 1.0f);

    bgv_update_state(system);

    return NIMCP_OK;
}

int bgv_process_recovery(bgv_system_t* system, float dt_ms) {
    if (!system) return NIMCP_ERROR_NULL_POINTER;

    if (!system->config.enable_fatigue) return NIMCP_OK;

    /* Recovery proportional to time */
    float recovery = system->config.recovery_rate * (dt_ms / 1000.0f);

    /* Recovery is faster at higher dopamine levels */
    recovery *= (1.0f + system->dopamine_level * 0.5f);

    system->fatigue_level = nimcp_clampf(system->fatigue_level - recovery, 0.0f, 1.0f);

    bgv_update_state(system);

    return NIMCP_OK;
}

//=============================================================================
// Query Functions
//=============================================================================

bgv_state_t bgv_get_state(const bgv_system_t* system) {
    if (!system) return BGV_STATE_NORMAL;
    return system->state;
}

const bgv_action_vigor_t* bgv_get_action(const bgv_system_t* system, uint32_t action_id) {
    return bgv_find_action_const(system, action_id);
}

float bgv_get_fatigue(const bgv_system_t* system) {
    if (!system) return 0.0f;
    return system->fatigue_level;
}

int bgv_get_stats(const bgv_system_t* system, bgv_stats_t* stats) {
    if (!system || !stats) return NIMCP_ERROR_NULL_POINTER;

    memset(stats, 0, sizeof(bgv_stats_t));

    stats->current_state = system->state;
    stats->fatigue_level = system->fatigue_level;
    stats->motivation_level = system->motivation_level;
    stats->dopamine_level = system->dopamine_level;
    stats->total_actions = system->total_computations;

    /* Compute averages */
    if (system->total_computations > 0) {
        stats->avg_vigor = system->cumulative_vigor / (float)system->total_computations;
        stats->avg_effort = system->cumulative_effort / (float)system->total_computations;
    } else {
        stats->avg_vigor = system->config.base_vigor;
        stats->avg_effort = 0.5f;
    }

    return NIMCP_OK;
}

//=============================================================================
// Utility Functions
//=============================================================================

const char* bgv_state_name(bgv_state_t state) {
    switch (state) {
        case BGV_STATE_NORMAL:      return "Normal";
        case BGV_STATE_ENHANCED:    return "Enhanced";
        case BGV_STATE_REDUCED:     return "Reduced";
        case BGV_STATE_BRADYKINETIC: return "Bradykinetic";
        case BGV_STATE_HYPERKINETIC: return "Hyperkinetic";
        default:                     return "Unknown";
    }
}

const char* bgv_effort_type_name(bgv_effort_type_t type) {
    switch (type) {
        case BGV_EFFORT_PHYSICAL:   return "Physical";
        case BGV_EFFORT_COGNITIVE:  return "Cognitive";
        case BGV_EFFORT_EMOTIONAL:  return "Emotional";
        case BGV_EFFORT_TEMPORAL:   return "Temporal";
        default:                     return "Unknown";
    }
}
