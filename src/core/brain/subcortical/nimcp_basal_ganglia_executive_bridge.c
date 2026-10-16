/**
 * @file nimcp_basal_ganglia_executive_bridge.c
 * @brief Basal ganglia-executive prefrontal bridge implementation
 */

#include "core/brain/subcortical/nimcp_basal_ganglia_executive_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/math/nimcp_math_helpers.h"
#include <string.h>
#include <math.h>

//=============================================================================
#include <stddef.h>
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(basal_ganglia_executive_bridge, MESH_ADAPTER_CATEGORY_SUBCORTICAL)


#define LOG_MODULE "BASAL_GANGLIA_EXECUTIVE_BRIDGE"


/* ============================================================================
 * Static Helpers
 * ============================================================================ */

static bge_goal_t* find_goal(bge_bridge_t* bridge, uint32_t goal_id) {
    for (uint32_t i = 0; i < bridge->num_goals; i++) {
        if (bridge->goals[i].goal_id == goal_id) {
            return &bridge->goals[i];
        }
    }
    return NULL;
}

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

void bge_bridge_default_config(bge_bridge_config_t* config) {
    if (!config) return;

    config->pfc_weight = BGE_DEFAULT_PFC_WEIGHT;
    config->habit_weight = 0.3f;
    config->load_threshold = BGE_DEFAULT_LOAD_THRESHOLD;
    config->conflict_threshold = 0.5f;
    config->switch_cost_duration_ms = 200.0f;
    config->inhibition_strength = 0.8f;
    config->enable_load_monitoring = true;
    config->enable_conflict_detection = true;
    config->enable_switch_cost = true;
}

bge_bridge_t* bge_bridge_create(const bge_bridge_config_t* config) {
    bge_bridge_t* bridge = nimcp_calloc(1, sizeof(bge_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bge_bridge_create: failed to allocate bridge");
        return NULL;
    }

    /* Apply config */
    if (config) {
        bridge->config = *config;
    } else {
        bge_bridge_default_config(&bridge->config);
    }

    /* Initialize goals */
    bridge->num_goals = 0;
    bridge->next_goal_id = 1;

    /* Initialize state */
    bridge->state.mode = BGE_CONTROL_GOAL_DIRECTED;
    bridge->state.pfc_influence = 1.0f;
    bridge->state.cognitive_load = 0.0f;
    bridge->state.habit_pressure = 0.0f;
    bridge->state.conflict_level = 0.0f;
    bridge->state.conflict_type = BGE_CONFLICT_NONE;
    bridge->state.active_goal_count = 0;

    /* Task switching */
    bridge->last_switch_time_ms = 0;
    bridge->in_switch_cost = false;

    /* Inhibition */
    bridge->inhibition_level = 0.0f;
    bridge->inhibited_action = 0;

    /* Create mutex */
    if (bridge_base_init(&bridge->base, 0, "basal_ganglia_executive") != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "bge_bridge_create: failed to initialize bridge base");
        nimcp_free(bridge);
        return NULL;
    }

    NIMCP_LOGGING_INFO("Created %s bridge", "basal_ganglia_executive");
    return bridge;
}

void bge_bridge_destroy(bge_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "basal_ganglia_executive");

    if (bridge->base.mutex) bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

int bge_bridge_reset(bge_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bge_bridge_reset: bridge is NULL");
        return -1;
    }

    bridge->num_goals = 0;

    bridge->state.mode = BGE_CONTROL_GOAL_DIRECTED;
    bridge->state.pfc_influence = 1.0f;
    bridge->state.cognitive_load = 0.0f;
    bridge->state.habit_pressure = 0.0f;
    bridge->state.conflict_level = 0.0f;
    bridge->state.conflict_type = BGE_CONFLICT_NONE;
    bridge->state.active_goal_count = 0;

    bridge->last_switch_time_ms = 0;
    bridge->in_switch_cost = false;
    bridge->inhibition_level = 0.0f;

    memset(&bridge->stats, 0, sizeof(bge_bridge_stats_t));

    return 0;
}

/* ============================================================================
 * Connection Functions
 * ============================================================================ */

int bge_bridge_connect_bg(bge_bridge_t* bridge, basal_ganglia_t* bg) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bge_bridge_connect_bg: bridge is NULL");
        return -1;
    }
    bridge->bg = bg;
    return 0;
}

int bge_bridge_connect_executive(
    bge_bridge_t* bridge,
    executive_controller_t* exec
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bge_bridge_connect_executive: bridge is NULL");
        return -1;
    }
    bridge->exec = exec;
    return 0;
}

bool bge_bridge_is_connected(const bge_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    return bridge->bg != NULL && bridge->exec != NULL;
}

/* ============================================================================
 * Goal Management Functions
 * ============================================================================ */

int bge_bridge_register_goal(
    bge_bridge_t* bridge,
    uint32_t target_action,
    float priority,
    float value,
    uint32_t* goal_id
) {
    if (!bridge || bridge->num_goals >= BGE_MAX_ACTIVE_GOALS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "bge_bridge_register_goal: bridge is NULL");
        return -1;
    }

    bge_goal_t* goal = &bridge->goals[bridge->num_goals];

    goal->goal_id = bridge->next_goal_id++;
    goal->target_action = target_action;
    goal->priority = nimcp_clampf(priority, 0.0f, 1.0f);
    goal->value = value;
    goal->state = BGE_GOAL_PENDING;
    goal->start_time_ms = 0;
    goal->requires_inhibition = false;

    bridge->num_goals++;
    bridge->state.active_goal_count++;

    if (goal_id) *goal_id = goal->goal_id;

    return 0;
}

int bge_bridge_goal_achieved(bge_bridge_t* bridge, uint32_t goal_id) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bge_bridge_goal_achieved: bridge is NULL");
        return -1;
    }

    bge_goal_t* goal = find_goal(bridge, goal_id);
    if (!goal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bge_bridge_goal_achieved: goal is NULL");
        return -1;
    }

    goal->state = BGE_GOAL_ACHIEVED;
    if (bridge->state.active_goal_count > 0) {
        bridge->state.active_goal_count--;
    }

    return 0;
}

int bge_bridge_abandon_goal(bge_bridge_t* bridge, uint32_t goal_id) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bge_bridge_abandon_goal: bridge is NULL");
        return -1;
    }

    bge_goal_t* goal = find_goal(bridge, goal_id);
    if (!goal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bge_bridge_abandon_goal: goal is NULL");
        return -1;
    }

    goal->state = BGE_GOAL_ABANDONED;
    if (bridge->state.active_goal_count > 0) {
        bridge->state.active_goal_count--;
    }

    return 0;
}

bge_goal_state_t bge_bridge_get_goal_state(
    const bge_bridge_t* bridge,
    uint32_t goal_id
) {
    if (!bridge) return BGE_GOAL_ABANDONED;

    for (uint32_t i = 0; i < bridge->num_goals; i++) {
        if (bridge->goals[i].goal_id == goal_id) {
            return bridge->goals[i].state;
        }
    }

    return BGE_GOAL_ABANDONED;
}

uint32_t bge_bridge_get_top_goal(const bge_bridge_t* bridge) {
    if (!bridge || bridge->num_goals == 0) return 0;

    float max_priority = -1.0f;
    uint32_t top_id = 0;

    for (uint32_t i = 0; i < bridge->num_goals; i++) {
        if (bridge->goals[i].state == BGE_GOAL_ACTIVE ||
            bridge->goals[i].state == BGE_GOAL_PENDING) {
            if (bridge->goals[i].priority > max_priority) {
                max_priority = bridge->goals[i].priority;
                top_id = bridge->goals[i].goal_id;
            }
        }
    }

    return top_id;
}

/* ============================================================================
 * Control Mode Functions
 * ============================================================================ */

int bge_bridge_update_control(
    bge_bridge_t* bridge,
    uint64_t current_time_ms
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bge_bridge_update_control: bridge is NULL");
        return -1;
    }

    /* Read cognitive load from executive if connected */
    if (bridge->exec) {
        bridge->state.cognitive_load = executive_get_cognitive_load(bridge->exec);
    }

    /* Update switch cost state */
    if (bridge->in_switch_cost) {
        float elapsed = (float)(current_time_ms - bridge->last_switch_time_ms);
        if (elapsed >= bridge->config.switch_cost_duration_ms) {
            bridge->in_switch_cost = false;
        }
    }

    /* Compute PFC influence (inversely related to load) */
    if (bridge->config.enable_load_monitoring) {
        bridge->state.pfc_influence = 1.0f - bridge->state.cognitive_load * 0.5f;
        bridge->state.pfc_influence = nimcp_clampf(bridge->state.pfc_influence, 0.2f, 1.0f);
    }

    /* Compute habit pressure (higher when load is high) */
    bridge->state.habit_pressure = bridge->state.cognitive_load * bridge->config.habit_weight;

    /* Determine control mode */
    if (bridge->inhibition_level > 0.5f) {
        bridge->state.mode = BGE_CONTROL_SUPPRESSED;
    } else if (bridge->state.cognitive_load >= BGE_HABIT_TAKEOVER_LOAD) {
        bridge->state.mode = BGE_CONTROL_HABITUAL;
    } else if (bridge->state.cognitive_load >= bridge->config.load_threshold) {
        bridge->state.mode = BGE_CONTROL_MIXED;
    } else {
        bridge->state.mode = BGE_CONTROL_GOAL_DIRECTED;
    }

    /* Detect conflicts */
    if (bridge->config.enable_conflict_detection) {
        bge_conflict_type_t type;
        float level;
        bge_bridge_detect_conflict(bridge, &type, &level);
    }

    /* Update BG mode if connected */
    if (bridge->bg) {
        bool habitual = (bridge->state.mode == BGE_CONTROL_HABITUAL);
        basal_ganglia_set_habit_mode(bridge->bg, habitual);
    }

    /* Update statistics */
    bridge->stats.avg_cognitive_load =
        (bridge->stats.avg_cognitive_load * bridge->stats.total_decisions +
         bridge->state.cognitive_load) / (bridge->stats.total_decisions + 1);
    bridge->stats.avg_pfc_influence =
        (bridge->stats.avg_pfc_influence * bridge->stats.total_decisions +
         bridge->state.pfc_influence) / (bridge->stats.total_decisions + 1);

    return 0;
}

int bge_bridge_apply_control(
    bge_bridge_t* bridge,
    float* action_values,
    uint32_t num_actions
) {
    if (!bridge || !action_values) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bge_bridge_apply_control: required parameter is NULL (bridge, action_values)");
        return -1;
    }

    bridge->stats.total_decisions++;

    /* Handle suppression mode */
    if (bridge->state.mode == BGE_CONTROL_SUPPRESSED) {
        for (uint32_t i = 0; i < num_actions; i++) {
            action_values[i] *= (1.0f - bridge->inhibition_level);
        }
        bridge->stats.inhibited_count++;
        return 0;
    }

    /* Boost goal-relevant actions */
    for (uint32_t i = 0; i < bridge->num_goals; i++) {
        bge_goal_t* goal = &bridge->goals[i];
        if (goal->state != BGE_GOAL_ACTIVE && goal->state != BGE_GOAL_PENDING) {
            continue;
        }
        if (goal->target_action < num_actions) {
            float boost = goal->priority * bridge->state.pfc_influence *
                         bridge->config.pfc_weight;
            action_values[goal->target_action] += boost;
        }
    }

    /* Apply switch cost penalty if active */
    if (bridge->in_switch_cost && bridge->config.enable_switch_cost) {
        for (uint32_t i = 0; i < num_actions; i++) {
            action_values[i] *= 0.8f;  /* 20% performance reduction */
        }
    }

    /* Apply specific action inhibition */
    if (bridge->inhibition_level > 0.0f &&
        bridge->inhibited_action < num_actions) {
        action_values[bridge->inhibited_action] *=
            (1.0f - bridge->inhibition_level * bridge->config.inhibition_strength);
    }

    /* Update mode-specific statistics */
    switch (bridge->state.mode) {
        case BGE_CONTROL_GOAL_DIRECTED:
            bridge->stats.goal_directed_count++;
            break;
        case BGE_CONTROL_HABITUAL:
            bridge->stats.habitual_count++;
            break;
        default:
            break;
    }

    return 0;
}

bge_control_mode_t bge_bridge_get_mode(const bge_bridge_t* bridge) {
    if (!bridge) return BGE_CONTROL_GOAL_DIRECTED;
    return bridge->state.mode;
}

int bge_bridge_set_mode(bge_bridge_t* bridge, bge_control_mode_t mode) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bge_bridge_set_mode: bridge is NULL");
        return -1;
    }
    bridge->state.mode = mode;
    return 0;
}

float bge_bridge_get_pfc_influence(const bge_bridge_t* bridge) {
    if (!bridge) return 0.0f;
    return bridge->state.pfc_influence;
}

/* ============================================================================
 * Inhibition Functions
 * ============================================================================ */

int bge_bridge_inhibit_action(
    bge_bridge_t* bridge,
    uint32_t action_id,
    float strength
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bge_bridge_inhibit_action: bridge is NULL");
        return -1;
    }

    bridge->inhibited_action = action_id;
    bridge->inhibition_level = nimcp_clampf(strength, 0.0f, 1.0f);

    return 0;
}

int bge_bridge_release_inhibition(
    bge_bridge_t* bridge,
    uint32_t action_id
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bge_bridge_release_inhibition: bridge is NULL");
        return -1;
    }

    if (bridge->inhibited_action == action_id) {
        bridge->inhibition_level = 0.0f;
    }

    return 0;
}

bool bge_bridge_is_inhibited(
    const bge_bridge_t* bridge,
    uint32_t action_id
) {
    if (!bridge) {
        return false;
    }
    return bridge->inhibited_action == action_id && bridge->inhibition_level > 0.0f;
}

int bge_bridge_global_stop(bge_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bge_bridge_global_stop: bridge is NULL");
        return -1;
    }

    bridge->inhibition_level = 1.0f;
    bridge->state.mode = BGE_CONTROL_SUPPRESSED;

    /* Trigger hyperdirect pathway in BG */
    if (bridge->bg) {
        basal_ganglia_suppress_action(bridge->bg, 1.0f);
    }

    return 0;
}

/* ============================================================================
 * Conflict Detection Functions
 * ============================================================================ */

bool bge_bridge_detect_conflict(
    bge_bridge_t* bridge,
    bge_conflict_type_t* conflict_type,
    float* conflict_level
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bge_bridge_detect_conflict: bridge is NULL");
        return false;
    }

    float level = 0.0f;
    bge_conflict_type_t type = BGE_CONFLICT_NONE;

    /* Check for goal-goal conflict */
    if (bridge->num_goals >= 2) {
        /* Count active goals at same priority level */
        uint32_t high_priority_count = 0;
        for (uint32_t i = 0; i < bridge->num_goals; i++) {
            if ((bridge->goals[i].state == BGE_GOAL_ACTIVE ||
                 bridge->goals[i].state == BGE_GOAL_PENDING) &&
                bridge->goals[i].priority > 0.7f) {
                high_priority_count++;
            }
        }
        if (high_priority_count >= 2) {
            type = BGE_CONFLICT_GOAL_GOAL;
            level = 0.5f + (high_priority_count - 2) * 0.2f;
        }
    }

    /* Check for goal-habit conflict */
    if (bridge->bg && type == BGE_CONFLICT_NONE) {
        if (basal_ganglia_is_habit_mode(bridge->bg) && bridge->num_goals > 0) {
            /* Check if habit differs from goal */
            uint32_t top_goal_id = bge_bridge_get_top_goal(bridge);
            bge_goal_t* top_goal = find_goal(bridge, top_goal_id);

            if (top_goal) {
                uint32_t habit_action;
                if (basal_ganglia_check_habit(bridge->bg, 0, &habit_action)) {
                    if (habit_action != top_goal->target_action) {
                        type = BGE_CONFLICT_GOAL_HABIT;
                        level = bridge->state.habit_pressure;
                    }
                }
            }
        }
    }

    /* Check for response conflict from BG */
    if (bridge->bg && type == BGE_CONFLICT_NONE) {
        float bg_conflict = basal_ganglia_get_conflict(bridge->bg);
        if (bg_conflict > bridge->config.conflict_threshold) {
            type = BGE_CONFLICT_RESPONSE;
            level = bg_conflict;
        }
    }

    bridge->state.conflict_type = type;
    bridge->state.conflict_level = level;

    if (conflict_type) *conflict_type = type;
    if (conflict_level) *conflict_level = level;

    if (type != BGE_CONFLICT_NONE) {
        bridge->stats.conflict_events++;
        bridge->stats.avg_conflict_level =
            (bridge->stats.avg_conflict_level * (bridge->stats.conflict_events - 1) +
             level) / bridge->stats.conflict_events;
    }

    return type != BGE_CONFLICT_NONE;
}

float bge_bridge_get_conflict(const bge_bridge_t* bridge) {
    if (!bridge) return 0.0f;
    return bridge->state.conflict_level;
}

/* ============================================================================
 * Task Switch Functions
 * ============================================================================ */

int bge_bridge_task_switch(
    bge_bridge_t* bridge,
    uint32_t new_task_id,
    uint64_t current_time_ms
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bge_bridge_task_switch: bridge is NULL");
        return -1;
    }

    bridge->last_switch_time_ms = current_time_ms;
    bridge->in_switch_cost = bridge->config.enable_switch_cost;

    /* Trigger dopamine dip during switch */
    if (bridge->bg) {
        float current_da = basal_ganglia_get_dopamine(bridge->bg);
        basal_ganglia_set_dopamine(bridge->bg,
                                   current_da - BGE_SWITCH_COST_DA_DIP);
    }

    bridge->stats.switch_events++;

    return 0;
}

bool bge_bridge_in_switch_cost(const bge_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bge_bridge_in_switch_cost: bridge is NULL");
        return false;
    }
    return bridge->in_switch_cost;
}

float bge_bridge_get_switch_cost_remaining(
    const bge_bridge_t* bridge,
    uint64_t current_time_ms
) {
    if (!bridge || !bridge->in_switch_cost) return 0.0f;

    float elapsed = (float)(current_time_ms - bridge->last_switch_time_ms);
    float remaining = bridge->config.switch_cost_duration_ms - elapsed;

    return remaining > 0 ? remaining : 0.0f;
}

/* ============================================================================
 * State Query Functions
 * ============================================================================ */

int bge_bridge_get_state(
    const bge_bridge_t* bridge,
    bge_control_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bge_bridge_get_state: required parameter is NULL (bridge, state)");
        return -1;
    }
    *state = bridge->state;
    return 0;
}

float bge_bridge_get_cognitive_load(const bge_bridge_t* bridge) {
    if (!bridge) return 0.0f;
    return bridge->state.cognitive_load;
}

float bge_bridge_get_habit_pressure(const bge_bridge_t* bridge) {
    if (!bridge) return 0.0f;
    return bridge->state.habit_pressure;
}

/* ============================================================================
 * Statistics Functions
 * ============================================================================ */

int bge_bridge_get_stats(
    const bge_bridge_t* bridge,
    bge_bridge_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bge_bridge_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }
    *stats = bridge->stats;
    return 0;
}

void bge_bridge_reset_stats(bge_bridge_t* bridge) {
    if (!bridge) return;
    memset(&bridge->stats, 0, sizeof(bge_bridge_stats_t));
}

const char* bge_control_mode_name(bge_control_mode_t mode) {
    switch (mode) {
        case BGE_CONTROL_GOAL_DIRECTED: return "goal_directed";
        case BGE_CONTROL_HABITUAL: return "habitual";
        case BGE_CONTROL_MIXED: return "mixed";
        case BGE_CONTROL_SUPPRESSED: return "suppressed";
        default: return "unknown";
    }
}

const char* bge_goal_state_name(bge_goal_state_t state) {
    switch (state) {
        case BGE_GOAL_PENDING: return "pending";
        case BGE_GOAL_ACTIVE: return "active";
        case BGE_GOAL_ACHIEVED: return "achieved";
        case BGE_GOAL_ABANDONED: return "abandoned";
        case BGE_GOAL_BLOCKED: return "blocked";
        default: return "unknown";
    }
}

const char* bge_conflict_type_name(bge_conflict_type_t type) {
    switch (type) {
        case BGE_CONFLICT_NONE: return "none";
        case BGE_CONFLICT_GOAL_HABIT: return "goal_habit";
        case BGE_CONFLICT_GOAL_GOAL: return "goal_goal";
        case BGE_CONFLICT_RESPONSE: return "response";
        default: return "unknown";
    }
}
