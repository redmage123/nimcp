//=============================================================================
// nimcp_bg_hierarchical_rl.c - Hierarchical RL with Options Framework
//=============================================================================

#include "core/brain/subcortical/nimcp_bg_hierarchical_rl.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_constants.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE_MESH_ONLY(bg_hierarchical_rl, MESH_ADAPTER_CATEGORY_SUBCORTICAL)


/* ============================================================================
 * INTERNAL STRUCTURES
 * ============================================================================ */

struct bg_hrl_system {
    /* Configuration */
    bg_hrl_config_t config;

    /* Options */
    bg_option_t* options;
    uint32_t num_options;

    /* Primitives */
    bg_primitive_t* primitives;
    uint32_t num_primitives;

    /* Goals */
    bg_goal_t* goals;
    uint32_t num_goals;

    /* Eligibility traces */
    bg_option_trace_t* traces;
    uint32_t num_traces;

    /* Current execution state */
    uint32_t active_option;
    uint32_t current_hierarchy_depth;
    uint32_t* option_stack;
    uint32_t stack_depth;

    /* Statistics */
    bg_hrl_stats_t stats;

    /* Thread safety */
    nimcp_mutex_t* mutex;
};

/* ============================================================================
 * LIFECYCLE IMPLEMENTATION
 * ============================================================================ */

void bg_hrl_default_config(bg_hrl_config_t* config) {
    if (!config) return;

    config->max_options = BG_HRL_MAX_OPTIONS;
    config->max_primitives = BG_HRL_MAX_PRIMITIVES;
    config->max_goals = BG_HRL_MAX_GOALS;
    config->state_dim = 64;

    config->option_learning_rate = BG_HRL_OPTION_LEARNING_RATE;
    config->intra_option_lr = BG_HRL_INTRA_OPTION_LR;
    config->termination_lr = BG_HRL_TERMINATION_LR;
    config->discount_factor = NIMCP_REWARD_DISCOUNT_DEFAULT;
    config->trace_decay = 0.9f;

    config->enable_option_discovery = true;
    config->discovery_threshold = BG_HRL_DISCOVERY_THRESHOLD;
    config->min_sequence_for_discovery = 3;

    config->enable_interruption = true;
    config->interruption_cost = 0.1f;
}

bg_hrl_system_t* bg_hrl_create(const bg_hrl_config_t* config) {
    bg_hrl_system_t* system = nimcp_calloc(1, sizeof(bg_hrl_system_t));
    if (!system) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "system is NULL");

        return NULL;

    }

    /* Apply configuration */
    if (config) {
        system->config = *config;
    } else {
        bg_hrl_default_config(&system->config);
    }

    /* Allocate options */
    system->options = nimcp_calloc(system->config.max_options, sizeof(bg_option_t));
    if (!system->options) {
        nimcp_free(system);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bg_hrl_create: system->options is NULL");
        return NULL;
    }

    /* Allocate primitives */
    system->primitives = nimcp_calloc(system->config.max_primitives, sizeof(bg_primitive_t));
    if (!system->primitives) {
        nimcp_free(system->options);
        nimcp_free(system);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bg_hrl_create: system->primitives is NULL");
        return NULL;
    }

    /* Allocate goals */
    system->goals = nimcp_calloc(system->config.max_goals, sizeof(bg_goal_t));
    if (!system->goals) {
        nimcp_free(system->primitives);
        nimcp_free(system->options);
        nimcp_free(system);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bg_hrl_create: system->goals is NULL");
        return NULL;
    }

    /* Allocate option stack */
    system->option_stack = nimcp_calloc(BG_HRL_MAX_HIERARCHY_DEPTH, sizeof(uint32_t));
    if (!system->option_stack) {
        nimcp_free(system->goals);
        nimcp_free(system->primitives);
        nimcp_free(system->options);
        nimcp_free(system);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bg_hrl_create: system->option_stack is NULL");
        return NULL;
    }

    /* Initialize state */
    system->num_options = 0;
    system->num_primitives = 0;
    system->num_goals = 0;
    system->active_option = UINT32_MAX;
    system->current_hierarchy_depth = 0;
    system->stack_depth = 0;

    /* Create mutex */
    system->mutex = nimcp_mutex_create(NULL);
    if (!system->mutex) {
        bg_hrl_destroy(system);
        return NULL;
    }

    return system;
}

void bg_hrl_destroy(bg_hrl_system_t* system) {
    if (!system) return;

    if (system->mutex) {
        nimcp_mutex_free(system->mutex);
    }

    nimcp_free(system->option_stack);
    nimcp_free(system->goals);
    nimcp_free(system->primitives);
    nimcp_free(system->options);
    nimcp_free(system->traces);
    nimcp_free(system);
}

int bg_hrl_reset(bg_hrl_system_t* system) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bg_hrl_reset: system is NULL");
        return -1;
    }

    nimcp_mutex_lock(system->mutex);

    /* Reset options state */
    for (uint32_t i = 0; i < system->num_options; i++) {
        system->options[i].state = BG_OPTION_STATE_INACTIVE;
        system->options[i].current_step = 0;
    }

    /* Reset goals state */
    for (uint32_t i = 0; i < system->num_goals; i++) {
        system->goals[i].state = BG_GOAL_STATE_INACTIVE;
        system->goals[i].progress = 0.0f;
    }

    /* Reset execution state */
    system->active_option = UINT32_MAX;
    system->current_hierarchy_depth = 0;
    system->stack_depth = 0;
    memset(system->option_stack, 0, BG_HRL_MAX_HIERARCHY_DEPTH * sizeof(uint32_t));

    /* Reset statistics */
    memset(&system->stats, 0, sizeof(bg_hrl_stats_t));

    nimcp_mutex_unlock(system->mutex);
    return 0;
}

/* ============================================================================
 * OPTION MANAGEMENT IMPLEMENTATION
 * ============================================================================ */

int bg_hrl_register_primitive(bg_hrl_system_t* system,
                               const bg_primitive_t* primitive) {
    if (!system || !primitive) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bg_hrl_reset: required parameter is NULL (system, primitive)");
        return -1;
    }

    nimcp_mutex_lock(system->mutex);

    if (system->num_primitives >= system->config.max_primitives) {
        nimcp_mutex_unlock(system->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "bg_hrl_reset: capacity exceeded");
        return -1;
    }

    system->primitives[system->num_primitives] = *primitive;
    system->primitives[system->num_primitives].id = system->num_primitives;
    system->num_primitives++;

    nimcp_mutex_unlock(system->mutex);
    return 0;
}

int bg_hrl_create_option(bg_hrl_system_t* system,
                          const bg_option_t* option_def,
                          uint32_t* out_id) {
    if (!system || !option_def) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bg_hrl_reset: required parameter is NULL (system, option_def)");
        return -1;
    }

    nimcp_mutex_lock(system->mutex);

    if (system->num_options >= system->config.max_options) {
        nimcp_mutex_unlock(system->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "bg_hrl_reset: capacity exceeded");
        return -1;
    }

    uint32_t id = system->num_options;
    system->options[id] = *option_def;
    system->options[id].id = id;
    system->options[id].state = BG_OPTION_STATE_INACTIVE;
    system->num_options++;

    system->stats.total_options = system->num_options;

    if (out_id) *out_id = id;

    nimcp_mutex_unlock(system->mutex);
    return 0;
}

int bg_hrl_create_sequence_option(bg_hrl_system_t* system,
                                   const char* name,
                                   const uint32_t* actions,
                                   uint32_t num_actions,
                                   uint32_t* out_id) {
    if (!system || !name || !actions || num_actions == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bg_hrl_reset: required parameter is NULL (system, name, actions)");
        return -1;
    }
    if (num_actions > BG_HRL_MAX_SEQUENCE_LEN) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "bg_hrl_reset: validation failed");
        return -1;
    }

    bg_option_t option;
    memset(&option, 0, sizeof(option));
    strncpy(option.name, name, sizeof(option.name) - 1);
    option.type = BG_OPTION_TYPE_SEQUENCE;
    option.sequence_length = num_actions;

    return bg_hrl_create_option(system, &option, out_id);
}

int bg_hrl_create_hierarchical_option(bg_hrl_system_t* system,
                                       const char* name,
                                       const uint32_t* sub_options,
                                       uint32_t num_sub,
                                       uint32_t* out_id) {
    if (!system || !name || !sub_options || num_sub == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bg_hrl_reset: required parameter is NULL (system, name, sub_options)");
        return -1;
    }

    bg_option_t option;
    memset(&option, 0, sizeof(option));
    strncpy(option.name, name, sizeof(option.name) - 1);
    option.type = BG_OPTION_TYPE_HIERARCHICAL;
    option.num_sub_options = num_sub;

    return bg_hrl_create_option(system, &option, out_id);
}

const bg_option_t* bg_hrl_get_option(const bg_hrl_system_t* system,
                                      uint32_t option_id) {
    if (!system || option_id >= system->num_options) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bg_hrl_reset: system is NULL");
        return NULL;
    }
    return &system->options[option_id];
}

/* ============================================================================
 * GOAL MANAGEMENT IMPLEMENTATION
 * ============================================================================ */

int bg_hrl_register_goal(bg_hrl_system_t* system,
                          const bg_goal_t* goal,
                          uint32_t* out_id) {
    if (!system || !goal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bg_hrl_reset: required parameter is NULL (system, goal)");
        return -1;
    }

    nimcp_mutex_lock(system->mutex);

    if (system->num_goals >= system->config.max_goals) {
        nimcp_mutex_unlock(system->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "bg_hrl_reset: capacity exceeded");
        return -1;
    }

    uint32_t id = system->num_goals;
    system->goals[id] = *goal;
    system->goals[id].id = id;
    system->goals[id].state = BG_GOAL_STATE_INACTIVE;
    system->num_goals++;

    system->stats.total_goals = system->num_goals;

    if (out_id) *out_id = id;

    nimcp_mutex_unlock(system->mutex);
    return 0;
}

int bg_hrl_activate_goal(bg_hrl_system_t* system, uint32_t goal_id) {
    if (!system || goal_id >= system->num_goals) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "bg_hrl_activate_goal: system is NULL");
        return -1;
    }

    nimcp_mutex_lock(system->mutex);
    system->goals[goal_id].state = BG_GOAL_STATE_ACTIVE;
    nimcp_mutex_unlock(system->mutex);
    return 0;
}

int bg_hrl_deactivate_goal(bg_hrl_system_t* system, uint32_t goal_id) {
    if (!system || goal_id >= system->num_goals) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "bg_hrl_deactivate_goal: system is NULL");
        return -1;
    }

    nimcp_mutex_lock(system->mutex);
    system->goals[goal_id].state = BG_GOAL_STATE_INACTIVE;
    nimcp_mutex_unlock(system->mutex);
    return 0;
}

int bg_hrl_update_goal_progress(bg_hrl_system_t* system,
                                 uint32_t goal_id,
                                 const float* current_state) {
    if (!system || goal_id >= system->num_goals) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "bg_hrl_deactivate_goal: system is NULL");
        return -1;
    }

    nimcp_mutex_lock(system->mutex);

    bg_goal_t* goal = &system->goals[goal_id];

    /* Simplified progress calculation */
    if (current_state && goal->goal_state && goal->goal_dim > 0) {
        float dist = 0.0f;
        for (uint32_t i = 0; i < goal->goal_dim; i++) {
            float diff = goal->goal_state[i] - current_state[i];
            dist += diff * diff;
        }
        dist = sqrtf(dist);
        goal->progress = nimcp_clampf(1.0f - dist, 0.0f, 1.0f);

        if (goal->progress >= goal->achievement_threshold) {
            goal->state = BG_GOAL_STATE_ACHIEVED;
            system->stats.goals_achieved++;
        }
    }

    nimcp_mutex_unlock(system->mutex);
    return 0;
}

/* ============================================================================
 * EXECUTION IMPLEMENTATION
 * ============================================================================ */

int bg_hrl_select_option(bg_hrl_system_t* system,
                          const float* state,
                          uint32_t* out_option_id) {
    if (!system || !out_option_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bg_hrl_deactivate_goal: required parameter is NULL (system, out_option_id)");
        return -1;
    }
    (void)state; /* Unused in stub */

    nimcp_mutex_lock(system->mutex);

    /* Simple selection: pick first available option */
    uint32_t selected = UINT32_MAX;
    float best_value = -1e9f;

    for (uint32_t i = 0; i < system->num_options; i++) {
        if (system->options[i].state == BG_OPTION_STATE_INACTIVE) {
            if (system->options[i].value > best_value) {
                best_value = system->options[i].value;
                selected = i;
            }
        }
    }

    if (selected != UINT32_MAX) {
        system->active_option = selected;
        system->options[selected].state = BG_OPTION_STATE_INITIATING;
        system->stats.active_options = 1;
    }

    *out_option_id = selected;

    nimcp_mutex_unlock(system->mutex);
    return (selected != UINT32_MAX) ? 0 : -1;
}

int bg_hrl_get_action(bg_hrl_system_t* system,
                       const float* state,
                       uint32_t* out_action_id) {
    if (!system || !out_action_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bg_hrl_deactivate_goal: required parameter is NULL (system, out_action_id)");
        return -1;
    }
    (void)state; /* Unused in stub */

    nimcp_mutex_lock(system->mutex);

    if (system->active_option == UINT32_MAX) {
        nimcp_mutex_unlock(system->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "bg_hrl_deactivate_goal: validation failed");
        return -1;
    }

    bg_option_t* opt = &system->options[system->active_option];

    /* For sequence options, return next action in sequence */
    if (opt->type == BG_OPTION_TYPE_SEQUENCE && opt->action_sequence) {
        if (opt->current_step < opt->sequence_length) {
            *out_action_id = opt->action_sequence[opt->current_step];
            opt->current_step++;
        } else {
            nimcp_mutex_unlock(system->mutex);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "bg_hrl_deactivate_goal: validation failed");
            return -1;
        }
    } else {
        /* Default: return primitive 0 */
        *out_action_id = 0;
    }

    nimcp_mutex_unlock(system->mutex);
    return 0;
}

bool bg_hrl_should_terminate(bg_hrl_system_t* system,
                              const float* state) {
    if (!system) return true;
    (void)state; /* Unused in stub */

    nimcp_mutex_lock(system->mutex);

    bool should_term = false;

    if (system->active_option != UINT32_MAX) {
        bg_option_t* opt = &system->options[system->active_option];

        /* Check max steps */
        if (opt->current_step >= opt->termination.max_steps) {
            should_term = true;
        }

        /* Check sequence completion */
        if (opt->type == BG_OPTION_TYPE_SEQUENCE &&
            opt->current_step >= opt->sequence_length) {
            should_term = true;
        }
    } else {
        should_term = true;
    }

    nimcp_mutex_unlock(system->mutex);
    return should_term;
}

int bg_hrl_interrupt_option(bg_hrl_system_t* system) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bg_hrl_interrupt_option: system is NULL");
        return -1;
    }

    nimcp_mutex_lock(system->mutex);

    if (system->active_option != UINT32_MAX) {
        system->options[system->active_option].state = BG_OPTION_STATE_ABORTED;
        system->active_option = UINT32_MAX;
        system->stats.active_options = 0;
    }

    nimcp_mutex_unlock(system->mutex);
    return 0;
}

int bg_hrl_step(bg_hrl_system_t* system,
                 const float* state,
                 float reward,
                 float dt_ms) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bg_hrl_interrupt_option: system is NULL");
        return -1;
    }
    (void)state;
    (void)reward;
    (void)dt_ms;

    nimcp_mutex_lock(system->mutex);

    /* Update active option if any */
    if (system->active_option != UINT32_MAX) {
        bg_option_t* opt = &system->options[system->active_option];

        if (opt->state == BG_OPTION_STATE_INITIATING) {
            opt->state = BG_OPTION_STATE_EXECUTING;
        }

        opt->execution_count++;
        opt->avg_duration += dt_ms;
        opt->avg_reward = opt->avg_reward * 0.9f + reward * 0.1f;
    }

    nimcp_mutex_unlock(system->mutex);
    return 0;
}

/* ============================================================================
 * LEARNING IMPLEMENTATION
 * ============================================================================ */

int bg_hrl_update_values(bg_hrl_system_t* system,
                          const float* state,
                          float reward,
                          const float* next_state,
                          bool terminal) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bg_hrl_interrupt_option: system is NULL");
        return -1;
    }
    (void)state;
    (void)next_state;
    (void)terminal;

    nimcp_mutex_lock(system->mutex);

    /* Simple value update for active option */
    if (system->active_option != UINT32_MAX) {
        bg_option_t* opt = &system->options[system->active_option];
        float td_error = reward - opt->value;
        opt->value += system->config.option_learning_rate * td_error;
    }

    nimcp_mutex_unlock(system->mutex);
    return 0;
}

int bg_hrl_update_intra_option(bg_hrl_system_t* system,
                                uint32_t option_id,
                                const float* state,
                                uint32_t action,
                                float reward) {
    if (!system || option_id >= system->num_options) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "bg_hrl_interrupt_option: system is NULL");
        return -1;
    }
    nimcp_mutex_lock(system->mutex);

    bg_option_t* opt = &system->options[option_id];
    if (opt->state == BG_OPTION_STATE_EXECUTING) {
        /* Intra-option learning: update policy Q-values for the taken action.
         * Uses simple TD(0) update: Q(s,a) += alpha * (r - Q(s,a)) */
        float alpha = system->config.option_learning_rate;
        opt->avg_reward = (1.0f - alpha) * opt->avg_reward + alpha * reward;

        /* Update policy weights if available */
        if (opt->policy_weights && action < opt->policy_action_dim && state) {
            for (uint32_t s = 0; s < opt->policy_state_dim; s++) {
                float td_error = reward - opt->policy_weights[s][action];
                opt->policy_weights[s][action] += alpha * td_error * state[s];
            }
        }
    }

    nimcp_mutex_unlock(system->mutex);
    return 0;
}

int bg_hrl_update_termination(bg_hrl_system_t* system,
                               uint32_t option_id,
                               const float* state,
                               bool did_terminate) {
    if (!system || option_id >= system->num_options) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "bg_hrl_interrupt_option: system is NULL");
        return -1;
    }
    (void)state;

    nimcp_mutex_lock(system->mutex);

    bg_option_t* opt = &system->options[option_id];

    /* Update learned termination probability */
    float target = did_terminate ? 1.0f : 0.0f;
    opt->termination.learned_probability +=
        system->config.termination_lr * (target - opt->termination.learned_probability);

    nimcp_mutex_unlock(system->mutex);
    return 0;
}

int bg_hrl_discover_options(bg_hrl_system_t* system) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bg_hrl_discover_options: system is NULL");
        return -1;
    }

    nimcp_mutex_lock(system->mutex);

    /* Stub: increment discovery counter */
    system->stats.options_discovered++;

    nimcp_mutex_unlock(system->mutex);
    return 0;
}

/* ============================================================================
 * QUERY IMPLEMENTATION
 * ============================================================================ */

uint32_t bg_hrl_get_active_option(const bg_hrl_system_t* system) {
    if (!system) return UINT32_MAX;
    return system->active_option;
}

uint32_t bg_hrl_get_hierarchy_depth(const bg_hrl_system_t* system) {
    if (!system) return 0;
    return system->current_hierarchy_depth;
}

float bg_hrl_get_option_value(const bg_hrl_system_t* system,
                               uint32_t option_id,
                               const float* state) {
    if (!system || option_id >= system->num_options) return 0.0f;
    (void)state;
    return system->options[option_id].value;
}

int bg_hrl_get_option_q_values(const bg_hrl_system_t* system,
                                const float* state,
                                float* q_values) {
    if (!system || !q_values) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bg_hrl_get_hierarchy_depth: required parameter is NULL (system, q_values)");
        return -1;
    }
    (void)state;

    for (uint32_t i = 0; i < system->num_options; i++) {
        q_values[i] = system->options[i].value;
    }
    return 0;
}

int bg_hrl_get_stats(const bg_hrl_system_t* system, bg_hrl_stats_t* stats) {
    if (!system || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bg_hrl_get_stats: required parameter is NULL (system, stats)");
        return -1;
    }
    *stats = system->stats;
    return 0;
}
