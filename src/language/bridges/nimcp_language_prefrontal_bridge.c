/**
 * @file nimcp_language_prefrontal_bridge.c
 * @brief Language-Prefrontal Executive Bridge Implementation
 *
 * Implements bidirectional integration between the Language Layer
 * and Prefrontal Cortex for executive control of language production.
 *
 * @version 1.0.0 - Phase LP1: Language-Prefrontal Integration
 * @date 2026-01-05
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "language/bridges/nimcp_language_prefrontal_bridge.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>

#define LOG_MODULE "LANG_PREFRONTAL"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(language_prefrontal_bridge)

//=============================================================================
// Internal Structures
//=============================================================================

typedef struct goal_entry {
    communication_goal_t goal;
    uint64_t start_time_ms;
    bool active;
    struct goal_entry* next;
} goal_entry_t;

typedef struct inhibition_entry {
    inhibition_signal_t signal;
    uint64_t start_time_ms;
    bool active;
} inhibition_entry_t;

struct language_prefrontal_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    /* Configuration */
    language_prefrontal_config_t config;

    /* Connected modules */
    language_orchestrator_t* language;
    prefrontal_adapter_t* prefrontal;
    broca_adapter_t* broca;
    wernicke_adapter_t* wernicke;
    bio_router_t bio_router;

    /* State */
    lp_bridge_state_t state;
    discourse_state_t discourse_state;
    uint64_t last_update_ms;

    /* Active goals */
    goal_entry_t* goals;
    uint32_t active_goal_count;
    uint32_t next_goal_id;

    /* Inhibition state */
    inhibition_entry_t* inhibitions;
    uint32_t inhibition_capacity;
    uint32_t active_inhibitions;

    /* Utterance queue */
    utterance_plan_t* utterance_queue;
    uint32_t queue_head;
    uint32_t queue_tail;
    uint32_t queue_size;

    /* Pending conflict */
    language_conflict_t* pending_conflict;
    bool conflict_needs_resolution;

    /* Callbacks */
    lp_goal_callback_t goal_callback;
    void* goal_callback_data;
    lp_inhibition_callback_t inhibition_callback;
    void* inhibition_callback_data;
    lp_conflict_callback_t conflict_callback;
    void* conflict_callback_data;

    /* Statistics */
    language_prefrontal_stats_t stats;
};

static goal_entry_t* find_goal(language_prefrontal_bridge_t* bridge, uint32_t goal_id) {
    goal_entry_t* entry = bridge->goals;
    while (entry) {
        if (entry->goal.goal_id == goal_id && entry->active) {
            return entry;
        }
        entry = entry->next;
    }
    return NULL;  /* Not found - normal lookup behavior */
}

static bool queue_utterance(language_prefrontal_bridge_t* bridge, const utterance_plan_t* plan) {
    uint32_t next_tail = (bridge->queue_tail + 1) % bridge->queue_size;
    if (next_tail == bridge->queue_head) {
        return false; /* Queue full - normal capacity behavior */
    }
    bridge->utterance_queue[bridge->queue_tail] = *plan;
    bridge->queue_tail = next_tail;
    return true;
}

static bool check_inhibition(language_prefrontal_bridge_t* bridge, const char* target) {
    for (uint32_t i = 0; i < bridge->inhibition_capacity; i++) {
        if (bridge->inhibitions[i].active) {
            if (bridge->inhibitions[i].signal.type == INHIBIT_ALL) {
                return true;
            }
            if (strstr(target, bridge->inhibitions[i].signal.target) != NULL) {
                return true;
            }
        }
    }
    return false;
}

static void expire_inhibitions(language_prefrontal_bridge_t* bridge, uint64_t timestamp_ms) {
    for (uint32_t i = 0; i < bridge->inhibition_capacity; i++) {
        if (bridge->inhibitions[i].active &&
            bridge->inhibitions[i].signal.duration_ms > 0) {
            uint64_t elapsed = timestamp_ms - bridge->inhibitions[i].start_time_ms;
            if (elapsed >= bridge->inhibitions[i].signal.duration_ms) {
                bridge->inhibitions[i].active = false;
                bridge->active_inhibitions--;
            }
        }
    }
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

language_prefrontal_config_t language_prefrontal_default_config(void) {
    return (language_prefrontal_config_t){
        .update_interval_ms = LP_DEFAULT_UPDATE_INTERVAL_MS,
        .max_discourse_goals = LP_DEFAULT_MAX_DISCOURSE_GOALS,
        .max_utterance_queue = LP_DEFAULT_MAX_UTTERANCE_QUEUE,
        .inhibition_threshold = LP_DEFAULT_INHIBITION_THRESHOLD,
        .conflict_threshold = LP_DEFAULT_CONFLICT_THRESHOLD,
        .planning_horizon = LP_DEFAULT_PLANNING_HORIZON,
        .enable_goal_directed_speech = true,
        .enable_discourse_planning = true,
        .enable_inhibitory_control = true,
        .enable_conflict_monitoring = true,
        .enable_language_switching = false,
        .enable_bio_async = false
    };
}

language_prefrontal_bridge_t* language_prefrontal_bridge_create(
    language_orchestrator_t* language,
    prefrontal_adapter_t* prefrontal,
    const language_prefrontal_config_t* config
) {
    if (!prefrontal) {
        LOG_ERROR(LOG_MODULE, "Prefrontal adapter required");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "prefrontal is NULL");

        return NULL;
    }

    language_prefrontal_bridge_t* bridge = nimcp_calloc(1, sizeof(language_prefrontal_bridge_t));
    if (!bridge) {
        LOG_ERROR(LOG_MODULE, "Failed to allocate bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "language_prefrontal_bridge_create: allocation failed");

        return NULL;
    }

    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = language_prefrontal_default_config();
    }

    bridge->language = language;
    bridge->prefrontal = prefrontal;

    /* Allocate inhibition array */
    bridge->inhibition_capacity = 16;
    bridge->inhibitions = nimcp_calloc(bridge->inhibition_capacity, sizeof(inhibition_entry_t));
    if (!bridge->inhibitions) {
        language_prefrontal_bridge_destroy(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "language_prefrontal_bridge_create: bridge->inhibitions is NULL");
        return NULL;
    }

    /* Allocate utterance queue */
    bridge->queue_size = bridge->config.max_utterance_queue;
    bridge->utterance_queue = nimcp_calloc(bridge->queue_size, sizeof(utterance_plan_t));
    if (!bridge->utterance_queue) {
        language_prefrontal_bridge_destroy(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "language_prefrontal_bridge_create: bridge->utterance_queue is NULL");
        return NULL;
    }

    /* Allocate pending conflict */
    bridge->pending_conflict = nimcp_calloc(1, sizeof(language_conflict_t));
    if (!bridge->pending_conflict) {
        language_prefrontal_bridge_destroy(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "language_prefrontal_bridge_create: bridge->pending_conflict is NULL");
        return NULL;
    }

    bridge->state = LP_STATE_IDLE;
    bridge->discourse_state = DISCOURSE_IDLE;
    bridge->next_goal_id = 1;

    LOG_INFO(LOG_MODULE, "Language-Prefrontal bridge created");
    return bridge;
}

void language_prefrontal_bridge_destroy(language_prefrontal_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "language_prefrontal");

    /* Free goals */
    goal_entry_t* g = bridge->goals;
    while (g) {
        goal_entry_t* next = g->next;
        nimcp_free(g);
        g = next;
    }

    if (bridge->inhibitions) nimcp_free(bridge->inhibitions);
    if (bridge->utterance_queue) nimcp_free(bridge->utterance_queue);
    if (bridge->pending_conflict) nimcp_free(bridge->pending_conflict);

    nimcp_free(bridge);
    LOG_INFO(LOG_MODULE, "Language-Prefrontal bridge destroyed");
}

int language_prefrontal_bridge_reset(language_prefrontal_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* Clear goals */
    goal_entry_t* g = bridge->goals;
    while (g) {
        goal_entry_t* next = g->next;
        nimcp_free(g);
        g = next;
    }
    bridge->goals = NULL;
    bridge->active_goal_count = 0;

    /* Clear inhibitions */
    for (uint32_t i = 0; i < bridge->inhibition_capacity; i++) {
        bridge->inhibitions[i].active = false;
    }
    bridge->active_inhibitions = 0;

    /* Clear queue */
    bridge->queue_head = 0;
    bridge->queue_tail = 0;

    /* Reset state */
    bridge->state = LP_STATE_IDLE;
    bridge->discourse_state = DISCOURSE_IDLE;
    bridge->conflict_needs_resolution = false;

    return 0;
}

//=============================================================================
// Connection Functions
//=============================================================================

int language_prefrontal_connect_broca(
    language_prefrontal_bridge_t* bridge,
    broca_adapter_t* broca
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    bridge->broca = broca;
    LOG_INFO(LOG_MODULE, "Connected to Broca adapter");
    return 0;
}

int language_prefrontal_connect_wernicke(
    language_prefrontal_bridge_t* bridge,
    wernicke_adapter_t* wernicke
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    bridge->wernicke = wernicke;
    LOG_INFO(LOG_MODULE, "Connected to Wernicke adapter");
    return 0;
}

int language_prefrontal_connect_bio_async(
    language_prefrontal_bridge_t* bridge,
    bio_router_t router
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    bridge->bio_router = router;
    LOG_INFO(LOG_MODULE, "Connected to bio-async router");
    return 0;
}

//=============================================================================
// Update Functions
//=============================================================================

int language_prefrontal_bridge_update(
    language_prefrontal_bridge_t* bridge,
    uint64_t timestamp_ms
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    int events = 0;

    /* Expire timed inhibitions */
    expire_inhibitions(bridge, timestamp_ms);

    /* Check for conflict resolution needs */
    if (bridge->conflict_needs_resolution && bridge->conflict_callback) {
        bridge->conflict_callback(bridge->pending_conflict, bridge->conflict_callback_data);
        bridge->stats.conflicts_detected++;
        bridge->state = LP_STATE_CONFLICT;
        events++;
    }

    /* Update goal progress */
    goal_entry_t* g = bridge->goals;
    while (g) {
        if (g->active && g->goal.deadline_ms > 0 && timestamp_ms > g->goal.deadline_ms) {
            /* Goal expired */
            g->active = false;
            bridge->active_goal_count--;
            bridge->stats.goals_completed++; /* Mark as completed even if failed */
        }
        g = g->next;
    }

    /* Update state based on active goals */
    if (bridge->active_goal_count > 0 && bridge->state == LP_STATE_IDLE) {
        bridge->state = LP_STATE_GOAL_ACTIVE;
    } else if (bridge->active_goal_count == 0 && bridge->state == LP_STATE_GOAL_ACTIVE) {
        bridge->state = LP_STATE_IDLE;
    }

    bridge->last_update_ms = timestamp_ms;
    bridge->stats.current_state = bridge->state;
    bridge->stats.active_goals = bridge->active_goal_count;

    return events;
}

//=============================================================================
// Goal Management
//=============================================================================

int language_prefrontal_set_communication_goal(
    language_prefrontal_bridge_t* bridge,
    const communication_goal_t* goal
) {
    if (!bridge || !goal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_prefrontal_set_communication_goal: required parameter is NULL (bridge, goal)");
        return -1;
    }

    goal_entry_t* entry = nimcp_calloc(1, sizeof(goal_entry_t));
    if (!entry) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "language_prefrontal_set_communication_goal: allocation failed");

        return -1;

    }

    entry->goal = *goal;
    entry->goal.goal_id = bridge->next_goal_id++;
    entry->start_time_ms = nimcp_time_now_us() / 1000;
    entry->active = true;
    entry->next = bridge->goals;
    bridge->goals = entry;
    bridge->active_goal_count++;

    bridge->stats.goals_received++;

    /* Notify via callback */
    if (bridge->goal_callback) {
        bridge->goal_callback(&entry->goal, bridge->goal_callback_data);
    }

    bridge->state = LP_STATE_GOAL_ACTIVE;
    return (int)entry->goal.goal_id;
}

int language_prefrontal_cancel_goal(
    language_prefrontal_bridge_t* bridge,
    uint32_t goal_id
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    goal_entry_t* g = find_goal(bridge, goal_id);
    if (!g) {
        return -1;  /* Goal not found - normal lookup behavior */
    }

    g->active = false;
    bridge->active_goal_count--;
    return 0;
}

int language_prefrontal_get_active_goals(
    language_prefrontal_bridge_t* bridge,
    communication_goal_t* goals,
    uint32_t max_goals
) {
    if (!bridge || !goals) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_prefrontal_get_active_goals: required parameter is NULL (bridge, goals)");
        return -1;
    }

    uint32_t count = 0;
    goal_entry_t* g = bridge->goals;
    while (g && count < max_goals) {
        if (g->active) {
            goals[count++] = g->goal;
        }
        g = g->next;
    }
    return (int)count;
}

//=============================================================================
// Utterance Planning
//=============================================================================

int language_prefrontal_submit_utterance_plan(
    language_prefrontal_bridge_t* bridge,
    const utterance_plan_t* plan
) {
    if (!bridge || !plan) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_prefrontal_submit_utterance_plan: required parameter is NULL (bridge, plan)");
        return -1;
    }

    if (!queue_utterance(bridge, plan)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_CAPACITY_EXCEEDED, "language_prefrontal_submit_utterance_plan: utterance queue is full");
        return -1;
    }

    bridge->stats.plans_generated++;
    bridge->state = LP_STATE_PLANNING;
    return 0;
}

int language_prefrontal_modify_plan(
    language_prefrontal_bridge_t* bridge,
    uint32_t plan_id,
    const utterance_plan_t* modified_plan
) {
    if (!bridge || !modified_plan) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_prefrontal_modify_plan: required parameter is NULL (bridge, modified_plan)");
        return -1;
    }

    /* Search queue for matching plan */
    for (uint32_t i = bridge->queue_head; i != bridge->queue_tail;
         i = (i + 1) % bridge->queue_size) {
        if (bridge->utterance_queue[i].plan_id == plan_id) {
            bridge->utterance_queue[i] = *modified_plan;
            bridge->utterance_queue[i].plan_id = plan_id;
            return 0;
        }
    }
    return -1;  /* Plan not found - normal lookup behavior */
}

//=============================================================================
// Inhibitory Control
//=============================================================================

int language_prefrontal_apply_inhibition(
    language_prefrontal_bridge_t* bridge,
    const inhibition_signal_t* signal
) {
    if (!bridge || !signal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_prefrontal_apply_inhibition: required parameter is NULL (bridge, signal)");
        return -1;
    }

    /* Find empty slot */
    for (uint32_t i = 0; i < bridge->inhibition_capacity; i++) {
        if (!bridge->inhibitions[i].active) {
            bridge->inhibitions[i].signal = *signal;
            bridge->inhibitions[i].start_time_ms = nimcp_time_now_us() / 1000;
            bridge->inhibitions[i].active = true;
            bridge->active_inhibitions++;
            bridge->stats.inhibitions_triggered++;

            if (bridge->inhibition_callback) {
                bridge->inhibition_callback(signal, bridge->inhibition_callback_data);
            }

            bridge->state = LP_STATE_INHIBITING;
            return 0;
        }
    }
    return -1;  /* No inhibition slots available - normal capacity behavior */
}

int language_prefrontal_release_inhibition(
    language_prefrontal_bridge_t* bridge,
    inhibition_type_t type
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    int released = 0;
    for (uint32_t i = 0; i < bridge->inhibition_capacity; i++) {
        if (bridge->inhibitions[i].active &&
            bridge->inhibitions[i].signal.type == type) {
            bridge->inhibitions[i].active = false;
            bridge->active_inhibitions--;
            released++;
        }
    }

    if (bridge->active_inhibitions == 0 && bridge->state == LP_STATE_INHIBITING) {
        bridge->state = LP_STATE_IDLE;
    }

    return released;
}

bool language_prefrontal_is_inhibited(
    const language_prefrontal_bridge_t* bridge,
    const char* word_or_topic
) {
    if (!bridge || !word_or_topic) {
        return false;
    }
    return check_inhibition((language_prefrontal_bridge_t*)bridge, word_or_topic);
}

//=============================================================================
// Status Reporting
//=============================================================================

int language_prefrontal_report_production_status(
    language_prefrontal_bridge_t* bridge,
    const production_status_t* status
) {
    if (!bridge || !status) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_prefrontal_report_production_status: required parameter is NULL (bridge, status)");
        return -1;
    }

    /* Update fluency stats */
    float n = (float)(bridge->stats.goals_completed + 1);
    bridge->stats.avg_production_fluency =
        ((n - 1.0f) * bridge->stats.avg_production_fluency + status->fluency) / n;

    if (status->is_speaking) {
        bridge->state = LP_STATE_MONITORING;
    }

    return 0;
}

int language_prefrontal_report_goal_complete(
    language_prefrontal_bridge_t* bridge,
    uint32_t goal_id,
    bool success
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    goal_entry_t* g = find_goal(bridge, goal_id);
    if (!g) {
        return -1;  /* Goal not found - normal lookup behavior */
    }

    g->active = false;
    bridge->active_goal_count--;
    bridge->stats.goals_completed++;

    if (success) {
        float n = (float)bridge->stats.goals_completed;
        bridge->stats.avg_goal_success_rate =
            ((n - 1.0f) * bridge->stats.avg_goal_success_rate + 1.0f) / n;
    }

    /* Calculate completion time */
    uint64_t now = nimcp_time_now_us() / 1000;
    uint64_t duration = now - g->start_time_ms;
    float fn = (float)bridge->stats.goals_completed;
    bridge->stats.avg_goal_completion_ms =
        ((fn - 1.0f) * bridge->stats.avg_goal_completion_ms + (float)duration) / fn;

    return 0;
}

int language_prefrontal_report_conflict(
    language_prefrontal_bridge_t* bridge,
    const language_conflict_t* conflict
) {
    if (!bridge || !conflict) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_prefrontal_report_conflict: required parameter is NULL (bridge, conflict)");
        return -1;
    }

    *bridge->pending_conflict = *conflict;
    bridge->conflict_needs_resolution = conflict->needs_resolution;

    if (bridge->config.enable_conflict_monitoring) {
        bridge->state = LP_STATE_CONFLICT;
    }

    return 0;
}

int language_prefrontal_request_decision(
    language_prefrontal_bridge_t* bridge,
    const language_conflict_t* conflict,
    uint32_t* selected_option
) {
    if (!bridge || !conflict || !selected_option) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_prefrontal_request_decision: required parameter is NULL (bridge, conflict, selected_option)");
        return -1;
    }

    /* Simple heuristic: select highest value option */
    uint32_t best = 0;
    float best_value = conflict->option_values[0];
    for (uint32_t i = 1; i < conflict->option_count && i < 4; i++) {
        if (conflict->option_values[i] > best_value) {
            best_value = conflict->option_values[i];
            best = i;
        }
    }

    *selected_option = best;
    bridge->stats.conflicts_resolved++;
    bridge->conflict_needs_resolution = false;

    if (bridge->state == LP_STATE_CONFLICT) {
        bridge->state = LP_STATE_GOAL_ACTIVE;
    }

    return 0;
}

//=============================================================================
// Discourse Management
//=============================================================================

discourse_state_t language_prefrontal_get_discourse_state(
    const language_prefrontal_bridge_t* bridge
) {
    if (!bridge) return DISCOURSE_IDLE;
    return bridge->discourse_state;
}

int language_prefrontal_set_discourse_state(
    language_prefrontal_bridge_t* bridge,
    discourse_state_t state
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    bridge->discourse_state = state;
    return 0;
}

//=============================================================================
// Callback Registration
//=============================================================================

int language_prefrontal_set_goal_callback(
    language_prefrontal_bridge_t* bridge,
    lp_goal_callback_t callback,
    void* user_data
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    bridge->goal_callback = callback;
    bridge->goal_callback_data = user_data;
    return 0;
}

int language_prefrontal_set_inhibition_callback(
    language_prefrontal_bridge_t* bridge,
    lp_inhibition_callback_t callback,
    void* user_data
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    bridge->inhibition_callback = callback;
    bridge->inhibition_callback_data = user_data;
    return 0;
}

int language_prefrontal_set_conflict_callback(
    language_prefrontal_bridge_t* bridge,
    lp_conflict_callback_t callback,
    void* user_data
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    bridge->conflict_callback = callback;
    bridge->conflict_callback_data = user_data;
    return 0;
}

//=============================================================================
// Status and Statistics
//=============================================================================

lp_bridge_state_t language_prefrontal_get_state(
    const language_prefrontal_bridge_t* bridge
) {
    if (!bridge) return LP_STATE_ERROR;
    return bridge->state;
}

int language_prefrontal_get_stats(
    const language_prefrontal_bridge_t* bridge,
    language_prefrontal_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_prefrontal_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }
    *stats = bridge->stats;
    return 0;
}

void language_prefrontal_reset_stats(language_prefrontal_bridge_t* bridge) {
    if (!bridge) return;
    memset(&bridge->stats, 0, sizeof(language_prefrontal_stats_t));
}

int language_prefrontal_get_config(
    const language_prefrontal_bridge_t* bridge,
    language_prefrontal_config_t* config
) {
    if (!bridge || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_prefrontal_get_config: required parameter is NULL (bridge, config)");
        return -1;
    }
    *config = bridge->config;
    return 0;
}

int language_prefrontal_set_config(
    language_prefrontal_bridge_t* bridge,
    const language_prefrontal_config_t* config
) {
    if (!bridge || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_prefrontal_set_config: required parameter is NULL (bridge, config)");
        return -1;
    }
    bridge->config = *config;
    return 0;
}
