/**
 * @file nimcp_hypothalamus_executive_bridge.c
 * @brief Implementation of Hypothalamus -> Executive Bridge
 *
 * WHAT: Bridge between hypothalamus drives and executive goal management
 * WHY:  Drive states must influence goal priorities (survival over growth)
 * HOW:  Maps drive urgencies to goal priorities, enables survival interrupts
 *
 * @version Phase 6: Executive Bridge
 * @date 2026-01-04
 */

#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_executive_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/time/nimcp_time.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

//=============================================================================
#include <stddef.h>
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(hypothalamus_executive_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


#define LOG_MODULE "HYPOTHALAMUS_EXECUTIVE_BRIDGE"


/*=============================================================================
 * CONSTANTS
 *===========================================================================*/

#define HYPO_EXEC_BRIDGE_MODULE_ID  0x1160
#define GOAL_ID_COUNTER_START       1000

/*=============================================================================
 * INTERNAL HELPERS
 *===========================================================================*/

/**
 * @brief Find goal by ID
 */
static hypo_exec_goal_t* find_goal(hypo_exec_bridge_t* bridge, uint32_t goal_id) {
    for (uint32_t i = 0; i < bridge->goal_count; i++) {
        if (bridge->goals[i].goal_id == goal_id) {
            return &bridge->goals[i];
        }
    }
    return NULL;
}

/**
 * @brief Find goal by ID (const version)
 */
static const hypo_exec_goal_t* find_goal_const(const hypo_exec_bridge_t* bridge, uint32_t goal_id) {
    for (uint32_t i = 0; i < bridge->goal_count; i++) {
        if (bridge->goals[i].goal_id == goal_id) {
            return &bridge->goals[i];
        }
    }
    return NULL;
}

/**
 * @brief Generate next goal ID
 */
static uint32_t next_goal_id(hypo_exec_bridge_t* bridge) {
    static uint32_t counter = GOAL_ID_COUNTER_START;
    return counter++;
}

/**
 * @brief Map drive to primary goal category
 */
static hypo_goal_category_t drive_to_category(hypo_drive_type_t drive) {
    switch (drive) {
        case HYPO_DRIVE_SAFETY:
            return HYPO_GOAL_CAT_SURVIVAL;
        case HYPO_DRIVE_HUNGER:
        case HYPO_DRIVE_THIRST:
        case HYPO_DRIVE_TEMPERATURE:
        case HYPO_DRIVE_FATIGUE:
            return HYPO_GOAL_CAT_PHYSIOLOGICAL;
        case HYPO_DRIVE_SOCIAL:
            return HYPO_GOAL_CAT_SOCIAL;
        case HYPO_DRIVE_CURIOSITY:
            return HYPO_GOAL_CAT_COGNITIVE;
        case HYPO_DRIVE_AUTONOMY:
        case HYPO_DRIVE_COMPETENCE:
            return HYPO_GOAL_CAT_GROWTH;
        default:
            return HYPO_GOAL_CAT_EXTERNAL;
    }
}

/**
 * @brief Compute category boost from drive urgencies
 */
static void compute_category_boosts(hypo_exec_bridge_t* bridge, float* boosts) {
    /* Initialize to base weights */
    for (int i = 0; i < HYPO_GOAL_CAT_COUNT; i++) {
        boosts[i] = bridge->config.category_weights[i];
    }

    /* Get drive urgencies */
    float urgencies[HYPO_DRIVE_COUNT];
    if (!hypo_drive_get_urgencies(bridge->drives, urgencies)) {
        return;
    }

    /* Apply drive->category mapping */
    for (int d = 0; d < HYPO_DRIVE_COUNT; d++) {
        for (int c = 0; c < HYPO_GOAL_CAT_COUNT; c++) {
            boosts[c] += urgencies[d] * bridge->config.drive_category_map[d][c]
                        * bridge->config.drive_boost_scale;
        }
    }

    /* Clamp to reasonable range */
    for (int i = 0; i < HYPO_GOAL_CAT_COUNT; i++) {
        if (boosts[i] < 0.0f) boosts[i] = 0.0f;
        if (boosts[i] > 3.0f) boosts[i] = 3.0f;  /* Max 3x boost */
    }
}

/**
 * @brief Check if survival interrupt is needed
 */
static bool check_survival_interrupt(hypo_exec_bridge_t* bridge, hypo_exec_interrupt_t* interrupt) {
    if (!bridge->config.enable_survival_interrupts) {
        return false;
    }

    /* Get drive urgencies */
    float urgencies[HYPO_DRIVE_COUNT];
    if (!hypo_drive_get_urgencies(bridge->drives, urgencies)) {
        return false;
    }

    /* Check survival drives */
    float max_urgency = 0.0f;
    hypo_drive_type_t urgent_drive = HYPO_DRIVE_SAFETY;

    /* Safety is always survival-critical */
    if (urgencies[HYPO_DRIVE_SAFETY] > max_urgency) {
        max_urgency = urgencies[HYPO_DRIVE_SAFETY];
        urgent_drive = HYPO_DRIVE_SAFETY;
    }

    /* Physiological drives become survival-critical at high urgency */
    if (urgencies[HYPO_DRIVE_HUNGER] > max_urgency) {
        max_urgency = urgencies[HYPO_DRIVE_HUNGER];
        urgent_drive = HYPO_DRIVE_HUNGER;
    }
    if (urgencies[HYPO_DRIVE_THIRST] > max_urgency) {
        max_urgency = urgencies[HYPO_DRIVE_THIRST];
        urgent_drive = HYPO_DRIVE_THIRST;
    }
    if (urgencies[HYPO_DRIVE_TEMPERATURE] > max_urgency) {
        max_urgency = urgencies[HYPO_DRIVE_TEMPERATURE];
        urgent_drive = HYPO_DRIVE_TEMPERATURE;
    }
    if (urgencies[HYPO_DRIVE_FATIGUE] > max_urgency) {
        max_urgency = urgencies[HYPO_DRIVE_FATIGUE];
        urgent_drive = HYPO_DRIVE_FATIGUE;
    }

    /* Check thresholds */
    if (max_urgency >= bridge->config.critical_interrupt_threshold) {
        interrupt->level = HYPO_INTERRUPT_CRITICAL;
    } else if (max_urgency >= bridge->config.survival_interrupt_threshold) {
        interrupt->level = HYPO_INTERRUPT_URGENT;
    } else if (max_urgency >= bridge->config.survival_interrupt_threshold * 0.7f) {
        interrupt->level = HYPO_INTERRUPT_PRIORITIZE;
    } else if (max_urgency >= bridge->config.survival_interrupt_threshold * 0.5f) {
        interrupt->level = HYPO_INTERRUPT_SUGGEST;
    } else {
        return false;
    }

    /* Fill interrupt details */
    interrupt->source_drive = urgent_drive;
    interrupt->drive_urgency = max_urgency;
    interrupt->should_suspend_current = (interrupt->level >= HYPO_INTERRUPT_URGENT);
    interrupt->inject_category = drive_to_category(urgent_drive);
    interrupt->timestamp_us = nimcp_time_get_us();

    /* Generate recommended goal description */
    switch (urgent_drive) {
        case HYPO_DRIVE_SAFETY:
            snprintf(interrupt->recommended_goal, HYPO_EXEC_MAX_GOAL_DESC,
                     "Address safety threat immediately");
            interrupt->duration_hint_ms = 5000;  /* 5 seconds */
            break;
        case HYPO_DRIVE_HUNGER:
            snprintf(interrupt->recommended_goal, HYPO_EXEC_MAX_GOAL_DESC,
                     "Seek food/energy source");
            interrupt->duration_hint_ms = 60000;  /* 1 minute */
            break;
        case HYPO_DRIVE_THIRST:
            snprintf(interrupt->recommended_goal, HYPO_EXEC_MAX_GOAL_DESC,
                     "Seek water/hydration");
            interrupt->duration_hint_ms = 60000;
            break;
        case HYPO_DRIVE_TEMPERATURE:
            snprintf(interrupt->recommended_goal, HYPO_EXEC_MAX_GOAL_DESC,
                     "Regulate temperature");
            interrupt->duration_hint_ms = 30000;
            break;
        case HYPO_DRIVE_FATIGUE:
            snprintf(interrupt->recommended_goal, HYPO_EXEC_MAX_GOAL_DESC,
                     "Rest and recover energy");
            interrupt->duration_hint_ms = 300000;  /* 5 minutes */
            break;
        default:
            snprintf(interrupt->recommended_goal, HYPO_EXEC_MAX_GOAL_DESC,
                     "Address urgent drive: %s",
                     hypo_drive_type_string(urgent_drive));
            interrupt->duration_hint_ms = 60000;
            break;
    }

    return true;
}

/**
 * @brief Update goal effective priorities
 */
static void update_goal_priorities(hypo_exec_bridge_t* bridge, float* category_boosts) {
    for (uint32_t i = 0; i < bridge->goal_count; i++) {
        hypo_exec_goal_t* goal = &bridge->goals[i];

        if (!goal->active || goal->completed) {
            continue;
        }

        /* Get category boost */
        float cat_boost = category_boosts[goal->category];

        /* Get drive relevance boost */
        float urgencies[HYPO_DRIVE_COUNT];
        float drive_boost = 1.0f;
        if (hypo_drive_get_urgencies(bridge->drives, urgencies)) {
            drive_boost = 1.0f + urgencies[goal->primary_drive] * goal->drive_relevance;
        }

        /* Compute effective priority */
        goal->drive_boost = cat_boost * drive_boost;
        goal->effective_priority = goal->base_priority * goal->drive_boost;

        /* Clamp to [0, 1] */
        if (goal->effective_priority > 1.0f) {
            goal->effective_priority = 1.0f;
        }

        /* Check if blocked by interrupt */
        if (bridge->interrupt_active && bridge->config.block_growth_during_survival) {
            if (goal->category == HYPO_GOAL_CAT_GROWTH ||
                goal->category == HYPO_GOAL_CAT_COGNITIVE) {
                goal->blocked = true;
                bridge->goals_blocked++;
            }
        } else {
            goal->blocked = false;
        }

        /* Track boosted goals */
        if (goal->drive_boost > 1.2f) {
            bridge->goals_boosted++;
        }
    }
}

/*=============================================================================
 * BIO-ASYNC MESSAGE HANDLERS
 *===========================================================================*/

/**
 * @brief Handle drive state message
 */
static nimcp_error_t exec_handle_drive_state(const void* msg, size_t msg_size,
                                              nimcp_bio_promise_t promise, void* ctx) {
    hypo_exec_bridge_t* bridge = (hypo_exec_bridge_t*)ctx;
    if (!bridge) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Drive state changed, update priorities */
    hypo_exec_bridge_update_priorities(bridge);

    /* Broadcast if enabled */
    if (bridge->config.broadcast_enabled) {
        hypo_exec_bridge_broadcast_priorities(bridge);
    }

    return NIMCP_SUCCESS;
}

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

hypo_exec_bridge_config_t hypo_exec_bridge_default_config(void) {
    hypo_exec_bridge_config_t config = {0};

    /* Interrupt thresholds */
    config.survival_interrupt_threshold = HYPO_EXEC_SURVIVAL_THRESHOLD;
    config.critical_interrupt_threshold = 0.95f;

    /* Priority computation */
    config.drive_boost_scale = 1.5f;

    /* Category base weights */
    config.category_weights[HYPO_GOAL_CAT_SURVIVAL] = 1.5f;
    config.category_weights[HYPO_GOAL_CAT_PHYSIOLOGICAL] = 1.2f;
    config.category_weights[HYPO_GOAL_CAT_SOCIAL] = 1.0f;
    config.category_weights[HYPO_GOAL_CAT_COGNITIVE] = 1.0f;
    config.category_weights[HYPO_GOAL_CAT_GROWTH] = 0.9f;
    config.category_weights[HYPO_GOAL_CAT_EXTERNAL] = 1.0f;

    /* Drive-category mapping (how each drive affects each category) */
    /* Safety drive strongly affects survival category */
    config.drive_category_map[HYPO_DRIVE_SAFETY][HYPO_GOAL_CAT_SURVIVAL] = 2.0f;
    config.drive_category_map[HYPO_DRIVE_SAFETY][HYPO_GOAL_CAT_PHYSIOLOGICAL] = 0.3f;

    /* Physiological drives affect their category and survival */
    config.drive_category_map[HYPO_DRIVE_HUNGER][HYPO_GOAL_CAT_PHYSIOLOGICAL] = 1.5f;
    config.drive_category_map[HYPO_DRIVE_HUNGER][HYPO_GOAL_CAT_SURVIVAL] = 0.5f;
    config.drive_category_map[HYPO_DRIVE_THIRST][HYPO_GOAL_CAT_PHYSIOLOGICAL] = 1.5f;
    config.drive_category_map[HYPO_DRIVE_THIRST][HYPO_GOAL_CAT_SURVIVAL] = 0.5f;
    config.drive_category_map[HYPO_DRIVE_TEMPERATURE][HYPO_GOAL_CAT_PHYSIOLOGICAL] = 1.2f;
    config.drive_category_map[HYPO_DRIVE_FATIGUE][HYPO_GOAL_CAT_PHYSIOLOGICAL] = 1.0f;
    config.drive_category_map[HYPO_DRIVE_FATIGUE][HYPO_GOAL_CAT_GROWTH] = -0.5f;  /* Fatigue reduces growth */

    /* Social drive affects social category */
    config.drive_category_map[HYPO_DRIVE_SOCIAL][HYPO_GOAL_CAT_SOCIAL] = 1.5f;

    /* Curiosity affects cognitive category */
    config.drive_category_map[HYPO_DRIVE_CURIOSITY][HYPO_GOAL_CAT_COGNITIVE] = 1.5f;

    /* Growth drives affect growth category */
    config.drive_category_map[HYPO_DRIVE_AUTONOMY][HYPO_GOAL_CAT_GROWTH] = 1.2f;
    config.drive_category_map[HYPO_DRIVE_COMPETENCE][HYPO_GOAL_CAT_GROWTH] = 1.2f;

    /* Behavior flags */
    config.enable_survival_interrupts = true;
    config.enable_priority_updates = true;
    config.block_growth_during_survival = true;

    /* Integration */
    config.broadcast_enabled = true;

    return config;
}

hypo_exec_bridge_t* hypo_exec_bridge_create(
    hypo_drive_system_handle_t* drives,
    const hypo_exec_bridge_config_t* config) {

    if (!drives) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_exec_bridge_create: drives is NULL");
        return NULL;
    }

    hypo_exec_bridge_t* bridge = nimcp_calloc(1, sizeof(hypo_exec_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hypo_exec_bridge_create: failed to allocate bridge");
        return NULL;
    }

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = hypo_exec_bridge_default_config();
    }

    /* Store references */
    bridge->drives = drives;

    /* Initialize state */
    bridge->goal_count = 0;
    bridge->active_goal_id = 0;
    bridge->interrupt_active = false;

    /* Initialize bridge base (creates mutex, etc.) */
    if (bridge_base_init(&bridge->base, 0, "hypothalamus_executive") != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hypo_exec_bridge_create: bridge_base_init failed");
        nimcp_free(bridge);
        return NULL;
    }

    return bridge;
}

void hypo_exec_bridge_destroy(hypo_exec_bridge_t* bridge) {
    if (!bridge) {
        return;
    }
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "hypothalamus_executive");

    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
}

void hypo_exec_bridge_reset(hypo_exec_bridge_t* bridge) {
    if (!bridge) {
        return;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Clear goals */
    bridge->goal_count = 0;
    bridge->active_goal_id = 0;

    /* Clear interrupt */
    bridge->interrupt_active = false;
    memset(&bridge->current_interrupt, 0, sizeof(bridge->current_interrupt));

    /* Reset statistics */
    bridge->priority_updates = 0;
    bridge->interrupts_triggered = 0;
    bridge->goals_blocked = 0;
    bridge->goals_boosted = 0;

    nimcp_mutex_unlock(bridge->base.mutex);
}

/*=============================================================================
 * GOAL MANAGEMENT
 *===========================================================================*/

uint32_t hypo_exec_bridge_register_goal(
    hypo_exec_bridge_t* bridge,
    const char* description,
    hypo_goal_category_t category,
    hypo_drive_type_t primary_drive,
    float base_priority) {

    if (!bridge || !description) {
        return 0;
    }

    if (bridge->goal_count >= HYPO_EXEC_MAX_GOALS) {
        return 0;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    uint32_t goal_id = next_goal_id(bridge);
    hypo_exec_goal_t* goal = &bridge->goals[bridge->goal_count];

    goal->goal_id = goal_id;
    strncpy(goal->description, description, HYPO_EXEC_MAX_GOAL_DESC - 1);
    goal->description[HYPO_EXEC_MAX_GOAL_DESC - 1] = '\0';
    goal->category = category;
    goal->primary_drive = primary_drive;
    goal->base_priority = base_priority;
    goal->drive_boost = 1.0f;
    goal->effective_priority = base_priority;
    goal->drive_relevance = 1.0f;
    goal->active = true;
    goal->blocked = false;
    goal->completed = false;
    goal->created_us = nimcp_time_get_us();
    goal->activated_us = 0;
    goal->deadline_us = 0;

    bridge->goal_count++;

    nimcp_mutex_unlock(bridge->base.mutex);

    return goal_id;
}

bool hypo_exec_bridge_unregister_goal(
    hypo_exec_bridge_t* bridge,
    uint32_t goal_id) {

    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_exec_bridge_reset: bridge is NULL");
        return false;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    bool found = false;
    for (uint32_t i = 0; i < bridge->goal_count; i++) {
        if (bridge->goals[i].goal_id == goal_id) {
            /* Shift remaining goals down */
            for (uint32_t j = i; j < bridge->goal_count - 1; j++) {
                bridge->goals[j] = bridge->goals[j + 1];
            }
            bridge->goal_count--;
            found = true;
            break;
        }
    }

    /* Clear active goal if it was removed */
    if (bridge->active_goal_id == goal_id) {
        bridge->active_goal_id = 0;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return found;
}

bool hypo_exec_bridge_activate_goal(
    hypo_exec_bridge_t* bridge,
    uint32_t goal_id) {

    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_exec_bridge_reset: bridge is NULL");
        return false;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    hypo_exec_goal_t* goal = find_goal(bridge, goal_id);
    if (!goal) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_exec_bridge_reset: goal is NULL");
        return false;
    }

    /* Check if blocked by interrupt */
    if (goal->blocked) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "hypo_exec_bridge_reset: validation failed");
        return false;
    }

    goal->activated_us = nimcp_time_get_us();
    bridge->active_goal_id = goal_id;

    nimcp_mutex_unlock(bridge->base.mutex);

    return true;
}

bool hypo_exec_bridge_complete_goal(
    hypo_exec_bridge_t* bridge,
    uint32_t goal_id,
    float satisfaction) {

    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_exec_bridge_reset: bridge is NULL");
        return false;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    hypo_exec_goal_t* goal = find_goal(bridge, goal_id);
    if (!goal) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_exec_bridge_reset: goal is NULL");
        return false;
    }

    goal->completed = true;
    goal->active = false;

    /* Notify hypothalamus of drive satisfaction */
    if (satisfaction > 0.0f && bridge->drives) {
        hypo_drive_satisfy(bridge->drives, goal->primary_drive, satisfaction);
    }

    /* Clear active goal if this was it */
    if (bridge->active_goal_id == goal_id) {
        bridge->active_goal_id = 0;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return true;
}

bool hypo_exec_bridge_get_goal(
    const hypo_exec_bridge_t* bridge,
    uint32_t goal_id,
    hypo_exec_goal_t* goal) {

    if (!bridge || !goal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_exec_bridge_reset: required parameter is NULL (bridge, goal)");
        return false;
    }

    nimcp_mutex_lock(((hypo_exec_bridge_t*)bridge)->base.mutex);

    const hypo_exec_goal_t* found = find_goal_const(bridge, goal_id);
    if (!found) {
        nimcp_mutex_unlock(((hypo_exec_bridge_t*)bridge)->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_exec_bridge_reset: found is NULL");
        return false;
    }

    *goal = *found;

    nimcp_mutex_unlock(((hypo_exec_bridge_t*)bridge)->base.mutex);

    return true;
}

/*=============================================================================
 * PRIORITY COMPUTATION
 *===========================================================================*/

hypo_exec_priority_update_t hypo_exec_bridge_update_priorities(
    hypo_exec_bridge_t* bridge) {

    hypo_exec_priority_update_t update = {0};

    if (!bridge) {
        return update;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Compute category boosts from drives */
    compute_category_boosts(bridge, update.category_boosts);

    /* Check for survival interrupt */
    hypo_exec_interrupt_t interrupt;
    if (check_survival_interrupt(bridge, &interrupt)) {
        if (!bridge->interrupt_active ||
            interrupt.level > bridge->current_interrupt.level) {
            bridge->interrupt_active = true;
            bridge->current_interrupt = interrupt;
            bridge->interrupt_start_us = nimcp_time_get_us();
            bridge->interrupts_triggered++;
            update.interrupt_active = true;
            update.interrupt = interrupt;
        }
    }

    /* Update individual goal priorities */
    update_goal_priorities(bridge, update.category_boosts);

    /* Build priority update output */
    update.goal_count = 0;
    for (uint32_t i = 0; i < bridge->goal_count && update.goal_count < HYPO_EXEC_MAX_GOALS; i++) {
        if (bridge->goals[i].active && !bridge->goals[i].completed) {
            update.goal_ids[update.goal_count] = bridge->goals[i].goal_id;
            update.new_priorities[update.goal_count] = bridge->goals[i].effective_priority;
            update.goal_count++;
        }
    }

    /* Get drive urgencies */
    hypo_drive_get_urgencies(bridge->drives, update.drive_urgencies);
    update.dominant_drive = hypo_drive_get_priority(bridge->drives);

    update.timestamp_us = nimcp_time_get_us();

    /* Cache last update */
    bridge->last_update = update;
    bridge->priority_updates++;

    nimcp_mutex_unlock(bridge->base.mutex);

    return update;
}

uint32_t hypo_exec_bridge_get_top_priority_goal(
    const hypo_exec_bridge_t* bridge) {

    if (!bridge) {
        return 0;
    }

    nimcp_mutex_lock(((hypo_exec_bridge_t*)bridge)->base.mutex);

    float max_priority = -1.0f;
    uint32_t top_goal_id = 0;

    for (uint32_t i = 0; i < bridge->goal_count; i++) {
        const hypo_exec_goal_t* goal = &bridge->goals[i];
        if (goal->active && !goal->completed && !goal->blocked) {
            if (goal->effective_priority > max_priority) {
                max_priority = goal->effective_priority;
                top_goal_id = goal->goal_id;
            }
        }
    }

    nimcp_mutex_unlock(((hypo_exec_bridge_t*)bridge)->base.mutex);

    return top_goal_id;
}

bool hypo_exec_bridge_get_priority_order(
    const hypo_exec_bridge_t* bridge,
    uint32_t* goal_ids,
    uint32_t* count) {

    if (!bridge || !goal_ids || !count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (bridge, goal_ids, count)");
        return false;
    }

    nimcp_mutex_lock(((hypo_exec_bridge_t*)bridge)->base.mutex);

    /* Collect active goals */
    typedef struct {
        uint32_t id;
        float priority;
    } goal_entry_t;

    goal_entry_t entries[HYPO_EXEC_MAX_GOALS];
    uint32_t n = 0;

    for (uint32_t i = 0; i < bridge->goal_count; i++) {
        const hypo_exec_goal_t* goal = &bridge->goals[i];
        if (goal->active && !goal->completed) {
            entries[n].id = goal->goal_id;
            entries[n].priority = goal->effective_priority;
            n++;
        }
    }

    /* Simple bubble sort by priority (descending) */
    for (uint32_t i = 0; i < n; i++) {
        for (uint32_t j = i + 1; j < n; j++) {
            if (entries[j].priority > entries[i].priority) {
                goal_entry_t tmp = entries[i];
                entries[i] = entries[j];
                entries[j] = tmp;
            }
        }
    }

    /* Copy to output */
    for (uint32_t i = 0; i < n; i++) {
        goal_ids[i] = entries[i].id;
    }
    *count = n;

    nimcp_mutex_unlock(((hypo_exec_bridge_t*)bridge)->base.mutex);

    return true;
}

float hypo_exec_bridge_get_goal_priority(
    const hypo_exec_bridge_t* bridge,
    uint32_t goal_id) {

    if (!bridge) {
        return -1.0f;
    }

    nimcp_mutex_lock(((hypo_exec_bridge_t*)bridge)->base.mutex);

    const hypo_exec_goal_t* goal = find_goal_const(bridge, goal_id);
    float priority = goal ? goal->effective_priority : -1.0f;

    nimcp_mutex_unlock(((hypo_exec_bridge_t*)bridge)->base.mutex);

    return priority;
}

/*=============================================================================
 * SURVIVAL INTERRUPTS
 *===========================================================================*/

bool hypo_exec_bridge_check_interrupt(
    hypo_exec_bridge_t* bridge,
    hypo_exec_interrupt_t* interrupt) {

    if (!bridge || !interrupt) {
        return false;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    bool has_interrupt = bridge->interrupt_active;
    if (has_interrupt) {
        *interrupt = bridge->current_interrupt;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return has_interrupt;
}

bool hypo_exec_bridge_acknowledge_interrupt(
    hypo_exec_bridge_t* bridge) {

    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: bridge is NULL");
        return false;
    }

    /* Acknowledgment doesn't clear the interrupt, just notes it was received */
    return bridge->interrupt_active;
}

bool hypo_exec_bridge_clear_interrupt(
    hypo_exec_bridge_t* bridge) {

    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: bridge is NULL");
        return false;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->interrupt_active = false;
    memset(&bridge->current_interrupt, 0, sizeof(bridge->current_interrupt));

    /* Unblock goals */
    for (uint32_t i = 0; i < bridge->goal_count; i++) {
        bridge->goals[i].blocked = false;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return true;
}

hypo_interrupt_level_t hypo_exec_bridge_get_interrupt_level(
    const hypo_exec_bridge_t* bridge) {

    if (!bridge) {
        return HYPO_INTERRUPT_NONE;
    }

    return bridge->interrupt_active ? bridge->current_interrupt.level : HYPO_INTERRUPT_NONE;
}

/*=============================================================================
 * DRIVE-GOAL QUERIES
 *===========================================================================*/

bool hypo_exec_bridge_get_goals_for_drive(
    const hypo_exec_bridge_t* bridge,
    hypo_drive_type_t drive,
    uint32_t* goal_ids,
    uint32_t* count) {

    if (!bridge || !goal_ids || !count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (bridge, goal_ids, count)");
        return false;
    }

    nimcp_mutex_lock(((hypo_exec_bridge_t*)bridge)->base.mutex);

    *count = 0;
    for (uint32_t i = 0; i < bridge->goal_count && *count < HYPO_EXEC_MAX_GOALS; i++) {
        const hypo_exec_goal_t* goal = &bridge->goals[i];
        if (goal->primary_drive == drive && goal->active && !goal->completed) {
            goal_ids[*count] = goal->goal_id;
            (*count)++;
        }
    }

    nimcp_mutex_unlock(((hypo_exec_bridge_t*)bridge)->base.mutex);

    return true;
}

float hypo_exec_bridge_get_category_boost(
    const hypo_exec_bridge_t* bridge,
    hypo_goal_category_t category) {

    if (!bridge || category >= HYPO_GOAL_CAT_COUNT) {
        return 1.0f;
    }

    return bridge->last_update.category_boosts[category];
}

bool hypo_exec_bridge_is_category_blocked(
    const hypo_exec_bridge_t* bridge,
    hypo_goal_category_t category) {

    if (!bridge) {
        return false;
    }

    if (!bridge->interrupt_active) {
        return false;
    }

    if (!bridge->config.block_growth_during_survival) {
        return false;
    }

    /* Only growth and cognitive categories get blocked during survival */
    return (category == HYPO_GOAL_CAT_GROWTH || category == HYPO_GOAL_CAT_COGNITIVE);
}

/*=============================================================================
 * BIO-ASYNC INTEGRATION
 *===========================================================================*/

bool hypo_exec_bridge_register_bio(
    hypo_exec_bridge_t* bridge,
    bool use_kg_wiring) {

    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: bridge is NULL");
        return false;
    }

    /* Register module */
    bio_module_info_t info = {
        .module_id = HYPO_EXEC_BRIDGE_MODULE_ID,
        .module_name = "hypothalamus_exec_bridge",
        .inbox_capacity = 0,  /* No inbox needed for now */
        .user_data = bridge
    };

    bridge->bio_ctx = bio_router_register_module(&info);
    if (!bridge->bio_ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: bridge->bio_ctx is NULL");
        return false;
    }

    /* Register handler for drive state updates */
    if (bio_router_register_handler(bridge->bio_ctx, BIO_MSG_HYPO_DRIVE_STATE,
                                     exec_handle_drive_state) != NIMCP_SUCCESS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: bridge->bio_ctx is NULL");
        return false;
    }

    return true;
}

uint32_t hypo_exec_bridge_process_bio(
    hypo_exec_bridge_t* bridge,
    uint32_t max_messages) {

    if (!bridge || !bridge->bio_ctx) {
        return 0;
    }

    return bio_router_process_inbox(bridge->bio_ctx, max_messages);
}

nimcp_error_t hypo_exec_bridge_broadcast_priorities(
    hypo_exec_bridge_t* bridge) {

    if (!bridge || !bridge->bio_ctx) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    if (!bridge->config.broadcast_enabled) {
        return NIMCP_SUCCESS;
    }

    /* Construct message */
    struct {
        bio_message_header_t header;
        hypo_exec_priority_update_t update;
    } msg;

    msg.header.type = BIO_MSG_EXEC_PRIORITY_UPDATE;
    msg.header.timestamp_us = nimcp_time_get_us();
    msg.header.source_module = HYPO_EXEC_BRIDGE_MODULE_ID;
    msg.header.target_module = 0;  /* Broadcast to all */
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;
    msg.header.payload_size = sizeof(hypo_exec_priority_update_t);
    msg.update = bridge->last_update;

    return bio_router_broadcast(bridge->bio_ctx, &msg.header, sizeof(msg));
}

nimcp_error_t hypo_exec_bridge_broadcast_interrupt(
    hypo_exec_bridge_t* bridge,
    const hypo_exec_interrupt_t* interrupt) {

    if (!bridge || !bridge->bio_ctx || !interrupt) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Construct message */
    struct {
        bio_message_header_t header;
        hypo_exec_interrupt_t interrupt;
    } msg;

    msg.header.type = BIO_MSG_EXEC_INTERRUPT;
    msg.header.timestamp_us = nimcp_time_get_us();
    msg.header.source_module = HYPO_EXEC_BRIDGE_MODULE_ID;
    msg.header.target_module = 0;  /* Broadcast to all */
    msg.header.flags = BIO_MSG_FLAG_BROADCAST | BIO_MSG_FLAG_URGENT;  /* Interrupts are high priority */
    msg.header.payload_size = sizeof(hypo_exec_interrupt_t);
    msg.interrupt = *interrupt;

    return bio_router_broadcast(bridge->bio_ctx, &msg.header, sizeof(msg));
}

/*=============================================================================
 * STATISTICS
 *===========================================================================*/

void hypo_exec_bridge_get_stats(
    const hypo_exec_bridge_t* bridge,
    uint64_t* priority_updates,
    uint64_t* interrupts,
    uint64_t* goals_blocked,
    uint64_t* goals_boosted) {

    if (!bridge) {
        return;
    }

    if (priority_updates) *priority_updates = bridge->priority_updates;
    if (interrupts) *interrupts = bridge->interrupts_triggered;
    if (goals_blocked) *goals_blocked = bridge->goals_blocked;
    if (goals_boosted) *goals_boosted = bridge->goals_boosted;
}

/*=============================================================================
 * UTILITY FUNCTIONS
 *===========================================================================*/

const char* hypo_goal_category_string(hypo_goal_category_t category) {
    switch (category) {
        case HYPO_GOAL_CAT_SURVIVAL:       return "SURVIVAL";
        case HYPO_GOAL_CAT_PHYSIOLOGICAL:  return "PHYSIOLOGICAL";
        case HYPO_GOAL_CAT_SOCIAL:         return "SOCIAL";
        case HYPO_GOAL_CAT_COGNITIVE:      return "COGNITIVE";
        case HYPO_GOAL_CAT_GROWTH:         return "GROWTH";
        case HYPO_GOAL_CAT_EXTERNAL:       return "EXTERNAL";
        default:                           return "UNKNOWN";
    }
}

const char* hypo_interrupt_level_string(hypo_interrupt_level_t level) {
    switch (level) {
        case HYPO_INTERRUPT_NONE:       return "NONE";
        case HYPO_INTERRUPT_SUGGEST:    return "SUGGEST";
        case HYPO_INTERRUPT_PRIORITIZE: return "PRIORITIZE";
        case HYPO_INTERRUPT_URGENT:     return "URGENT";
        case HYPO_INTERRUPT_CRITICAL:   return "CRITICAL";
        default:                        return "UNKNOWN";
    }
}
