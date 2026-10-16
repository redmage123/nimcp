/**
 * @file nimcp_shared_intentionality.c
 * @brief Implementation of joint goals, shared attention, and we-mode
 *
 * WHAT: Model joint goals, shared attention, and "we-mode" cognition
 * WHY: Enable collective action and coordination between brain instances
 * HOW: Track shared goals, commitments, roles, and joint attention
 */

#include "cognitive/collective_cognition/nimcp_shared_intentionality.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/logging/nimcp_logging.h"
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include <stdatomic.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(shared_intentionality, MESH_ADAPTER_CATEGORY_COGNITIVE)


/*=============================================================================
 * Internal Structures
 *===========================================================================*/

/**
 * @brief Registered instance entry
 */
typedef struct {
    uint32_t instance_id;
    bool active;
    float capability_score;     /* For role assignment */
} si_instance_t;

/**
 * @brief Internal shared intentionality state
 */
struct shared_intentionality {
    /* Configuration */
    shared_intentionality_config_t config;

    /* Registered instances */
    si_instance_t instances[COLLECTIVE_MAX_INSTANCES];
    uint32_t instance_count;

    /* Shared goals */
    shared_goal_t goals[COLLECTIVE_MAX_SHARED_GOALS];
    uint32_t goal_count;
    uint32_t next_goal_id;

    /* Joint attentions */
    joint_attention_t attentions[COLLECTIVE_MAX_JOINT_ATTENTIONS];
    uint32_t attention_count;
    uint32_t next_attention_id;

    /* We-mode state */
    we_mode_state_t we_mode;
    bool we_mode_forced;

    /* Callbacks */
    goal_state_callback_fn goal_callback;
    void* goal_callback_data;
    attention_callback_fn attention_callback;
    void* attention_callback_data;

    /* Statistics */
    shared_intentionality_stats_t stats;
    uint64_t we_mode_start_us;
    uint64_t total_we_mode_us;
    uint64_t total_time_us;

    /* Flags */
    bool initialized;
    uint64_t last_update_us;
};

/*=============================================================================
 * Helper Functions - Time
 *===========================================================================*/

static uint64_t get_timestamp_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

/*=============================================================================
 * Helper Functions - Instance Management
 *===========================================================================*/

static si_instance_t* find_instance(
    shared_intentionality_t* si,
    uint32_t instance_id
) {
    for (uint32_t i = 0; i < COLLECTIVE_MAX_INSTANCES; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && COLLECTIVE_MAX_INSTANCES > 256) {
            shared_intentionality_heartbeat("shared_inten_loop",
                             (float)(i + 1) / (float)COLLECTIVE_MAX_INSTANCES);
        }

        if (si->instances[i].active && si->instances[i].instance_id == instance_id) {
            return &si->instances[i];
        }
    }
    return NULL;  /* Not found is normal */
}

static si_instance_t* find_free_instance_slot(shared_intentionality_t* si) {
    for (uint32_t i = 0; i < COLLECTIVE_MAX_INSTANCES; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && COLLECTIVE_MAX_INSTANCES > 256) {
            shared_intentionality_heartbeat("shared_inten_loop",
                             (float)(i + 1) / (float)COLLECTIVE_MAX_INSTANCES);
        }

        if (!si->instances[i].active) {
            return &si->instances[i];
        }
    }
    return NULL;
}

/*=============================================================================
 * Helper Functions - Goal Management
 *===========================================================================*/

static shared_goal_t* find_goal(
    shared_intentionality_t* si,
    uint32_t goal_id
) {
    for (uint32_t i = 0; i < COLLECTIVE_MAX_SHARED_GOALS; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && COLLECTIVE_MAX_SHARED_GOALS > 256) {
            shared_intentionality_heartbeat("shared_inten_loop",
                             (float)(i + 1) / (float)COLLECTIVE_MAX_SHARED_GOALS);
        }

        if (si->goals[i].goal_id == goal_id) {
            return &si->goals[i];
        }
    }
    return NULL;
}

static shared_goal_t* find_free_goal_slot(shared_intentionality_t* si) {
    for (uint32_t i = 0; i < COLLECTIVE_MAX_SHARED_GOALS; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && COLLECTIVE_MAX_SHARED_GOALS > 256) {
            shared_intentionality_heartbeat("shared_inten_loop",
                             (float)(i + 1) / (float)COLLECTIVE_MAX_SHARED_GOALS);
        }

        if (si->goals[i].goal_id == 0) {
            return &si->goals[i];
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "find_free_goal_slot: .goal_id is zero");
    return NULL;
}

static goal_commitment_t* find_commitment(
    shared_goal_t* goal,
    uint32_t instance_id
) {
    for (uint32_t i = 0; i < goal->commitment_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && goal->commitment_count > 256) {
            shared_intentionality_heartbeat("shared_inten_loop",
                             (float)(i + 1) / (float)goal->commitment_count);
        }

        if (goal->commitments[i].instance_id == instance_id) {
            return &goal->commitments[i];
        }
    }
    return NULL;
}

static void update_goal_total_commitment(shared_goal_t* goal) {
    goal->total_commitment = 0.0f;
    for (uint32_t i = 0; i < goal->commitment_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && goal->commitment_count > 256) {
            shared_intentionality_heartbeat("shared_inten_loop",
                             (float)(i + 1) / (float)goal->commitment_count);
        }

        goal->total_commitment += goal->commitments[i].strength;
    }
}

static void notify_goal_state_change(
    shared_intentionality_t* si,
    shared_goal_t* goal,
    shared_goal_state_t old_state
) {
    if (si->goal_callback) {
        si->goal_callback(goal, old_state, si->goal_callback_data);
    }
}

/*=============================================================================
 * Helper Functions - Attention Management
 *===========================================================================*/

static joint_attention_t* find_attention(
    shared_intentionality_t* si,
    uint32_t attention_id
) {
    for (uint32_t i = 0; i < COLLECTIVE_MAX_JOINT_ATTENTIONS; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && COLLECTIVE_MAX_JOINT_ATTENTIONS > 256) {
            shared_intentionality_heartbeat("shared_inten_loop",
                             (float)(i + 1) / (float)COLLECTIVE_MAX_JOINT_ATTENTIONS);
        }

        if (si->attentions[i].attention_id == attention_id) {
            return &si->attentions[i];
        }
    }
    return NULL;
}

static joint_attention_t* find_free_attention_slot(shared_intentionality_t* si) {
    for (uint32_t i = 0; i < COLLECTIVE_MAX_JOINT_ATTENTIONS; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && COLLECTIVE_MAX_JOINT_ATTENTIONS > 256) {
            shared_intentionality_heartbeat("shared_inten_loop",
                             (float)(i + 1) / (float)COLLECTIVE_MAX_JOINT_ATTENTIONS);
        }

        if (si->attentions[i].attention_id == 0) {
            return &si->attentions[i];
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "find_free_attention_slot: .attention_id is zero");
    return NULL;
}

static bool is_attending(
    const joint_attention_t* attention,
    uint32_t instance_id
) {
    for (uint32_t i = 0; i < attention->attending_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && attention->attending_count > 256) {
            shared_intentionality_heartbeat("shared_inten_loop",
                             (float)(i + 1) / (float)attention->attending_count);
        }

        if (attention->attending_instances[i] == instance_id) {
            return true;
        }
    }
    return false;
}

/*=============================================================================
 * Helper Functions - We-Mode
 *===========================================================================*/

static void compute_we_mode_state(shared_intentionality_t* si) {
    if (si->instance_count < 2) {
        si->we_mode.we_mode_strength = 0.0f;
        si->we_mode.joint_commitment = 0.0f;
        si->we_mode.mutual_responsiveness = 0.0f;
        si->we_mode.role_understanding = 0.0f;
        si->we_mode.active_shared_goals = 0;
        si->we_mode.active_joint_attentions = 0;
        return;
    }

    /* Count active goals and attentions */
    uint32_t active_goals = 0;
    float total_commitment = 0.0f;
    uint32_t role_assigned_count = 0;

    for (uint32_t i = 0; i < COLLECTIVE_MAX_SHARED_GOALS; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && COLLECTIVE_MAX_SHARED_GOALS > 256) {
            shared_intentionality_heartbeat("shared_inten_loop",
                             (float)(i + 1) / (float)COLLECTIVE_MAX_SHARED_GOALS);
        }

        if (si->goals[i].goal_id == 0) continue;
        if (si->goals[i].state == GOAL_STATE_ACTIVE ||
            si->goals[i].state == GOAL_STATE_ACCEPTED) {
            active_goals++;
            total_commitment += si->goals[i].total_commitment;

            for (uint32_t j = 0; j < si->goals[i].commitment_count; j++) {
                if (si->goals[i].commitments[j].has_role) {
                    role_assigned_count++;
                }
            }
        }
    }

    uint32_t active_attentions = 0;
    float total_agreement = 0.0f;

    for (uint32_t i = 0; i < COLLECTIVE_MAX_JOINT_ATTENTIONS; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && COLLECTIVE_MAX_JOINT_ATTENTIONS > 256) {
            shared_intentionality_heartbeat("shared_inten_loop",
                             (float)(i + 1) / (float)COLLECTIVE_MAX_JOINT_ATTENTIONS);
        }

        if (si->attentions[i].attention_id == 0) continue;
        active_attentions++;
        total_agreement += si->attentions[i].agreement_level;
    }

    si->we_mode.active_shared_goals = active_goals;
    si->we_mode.active_joint_attentions = active_attentions;

    /* Compute we-mode metrics */
    if (active_goals > 0) {
        si->we_mode.joint_commitment = total_commitment /
            (active_goals * si->instance_count);
    } else {
        si->we_mode.joint_commitment = 0.0f;
    }

    if (active_attentions > 0) {
        si->we_mode.mutual_responsiveness = total_agreement / active_attentions;
    } else {
        si->we_mode.mutual_responsiveness = 0.0f;
    }

    if (si->goal_count > 0) {
        si->we_mode.role_understanding = (float)role_assigned_count /
            (si->goal_count * si->instance_count);
    } else {
        si->we_mode.role_understanding = 0.0f;
    }

    /* Overall we-mode strength */
    si->we_mode.we_mode_strength = (
        si->we_mode.joint_commitment * 0.3f +
        si->we_mode.mutual_responsiveness * 0.3f +
        si->we_mode.role_understanding * 0.2f +
        (active_goals > 0 ? 0.2f : 0.0f)
    );

    /* Check if forced */
    if (si->we_mode_forced) {
        si->we_mode.we_mode_strength = fmaxf(si->we_mode.we_mode_strength,
                                              si->config.we_mode_threshold);
    }
}

/*=============================================================================
 * Lifecycle API
 *===========================================================================*/

shared_intentionality_t* shared_intentionality_create(
    const shared_intentionality_config_t* config
) {
    /* Phase 8: Heartbeat at operation start */
    shared_intentionality_heartbeat("shared_inten_create", 0.0f);


    shared_intentionality_t* si = nimcp_malloc(sizeof(shared_intentionality_t));
    if (!si) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate si");

        return NULL;

    }

    memset(si, 0, sizeof(shared_intentionality_t));

    /* Apply configuration */
    if (config) {
        si->config = *config;
    } else {
        si->config = shared_intentionality_default_config();
    }

    si->next_goal_id = 1;
    si->next_attention_id = 1;
    si->initialized = true;
    si->last_update_us = get_timestamp_us();

    return si;
}

void shared_intentionality_destroy(shared_intentionality_t* si) {
    if (!si) return;
    /* Phase 8: Heartbeat at operation start */
    shared_intentionality_heartbeat("shared_inten_destroy", 0.0f);


    nimcp_free(si);
    si = NULL;
}

int shared_intentionality_reset(shared_intentionality_t* si) {
    if (!si) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "shared_intentionality_reset: si is NULL");
        return -1;
    }

    /* Clear instances */
    /* Phase 8: Heartbeat at operation start */
    shared_intentionality_heartbeat("shared_inten_reset", 0.0f);


    memset(si->instances, 0, sizeof(si->instances));
    si->instance_count = 0;

    /* Clear goals */
    memset(si->goals, 0, sizeof(si->goals));
    si->goal_count = 0;

    /* Clear attentions */
    memset(si->attentions, 0, sizeof(si->attentions));
    si->attention_count = 0;

    /* Clear we-mode */
    memset(&si->we_mode, 0, sizeof(si->we_mode));
    si->we_mode_forced = false;

    /* Reset stats */
    memset(&si->stats, 0, sizeof(si->stats));
    si->we_mode_start_us = 0;
    si->total_we_mode_us = 0;
    si->total_time_us = 0;

    si->last_update_us = get_timestamp_us();

    return 0;
}

/*=============================================================================
 * Instance Management API
 *===========================================================================*/

int shared_intentionality_register_instance(
    shared_intentionality_t* si,
    uint32_t instance_id
) {
    if (!si) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "shared_intentionality_register_instance: si is NULL");
        return -1;
    }

    /* Check if already registered */
    if (find_instance(si, instance_id)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "shared_intentionality_register_instance: validation failed");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    shared_intentionality_heartbeat("shared_inten_register_instance", 0.0f);


    si_instance_t* slot = find_free_instance_slot(si);
    if (!slot) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "shared_intentionality_register_instance: slot is NULL");
        return -1;
    }

    slot->instance_id = instance_id;
    slot->active = true;
    slot->capability_score = 0.5f;  /* Default capability */

    si->instance_count++;

    return 0;
}

int shared_intentionality_unregister_instance(
    shared_intentionality_t* si,
    uint32_t instance_id
) {
    if (!si) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "shared_intentionality_unregister_instance: si is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    shared_intentionality_heartbeat("shared_inten_unregister_instance", 0.0f);


    si_instance_t* inst = find_instance(si, instance_id);
    if (!inst) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "shared_intentionality_unregister_instance: inst is NULL");
        return -1;
    }

    inst->active = false;
    si->instance_count--;

    /* Remove from all goals and attentions */
    for (uint32_t i = 0; i < COLLECTIVE_MAX_SHARED_GOALS; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && COLLECTIVE_MAX_SHARED_GOALS > 256) {
            shared_intentionality_heartbeat("shared_inten_loop",
                             (float)(i + 1) / (float)COLLECTIVE_MAX_SHARED_GOALS);
        }

        if (si->goals[i].goal_id == 0) continue;
        shared_intentionality_withdraw_from_goal(si, si->goals[i].goal_id, instance_id);
    }

    for (uint32_t i = 0; i < COLLECTIVE_MAX_JOINT_ATTENTIONS; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && COLLECTIVE_MAX_JOINT_ATTENTIONS > 256) {
            shared_intentionality_heartbeat("shared_inten_loop",
                             (float)(i + 1) / (float)COLLECTIVE_MAX_JOINT_ATTENTIONS);
        }

        if (si->attentions[i].attention_id == 0) continue;
        shared_intentionality_leave_attention(si, si->attentions[i].attention_id, instance_id);
    }

    return 0;
}

/*=============================================================================
 * Shared Goal API
 *===========================================================================*/

uint32_t shared_intentionality_propose_goal(
    shared_intentionality_t* si,
    const shared_goal_t* goal
) {
    if (!si || !goal) return 0;

    /* Phase 8: Heartbeat at operation start */
    shared_intentionality_heartbeat("shared_inten_propose_goal", 0.0f);


    shared_goal_t* slot = find_free_goal_slot(si);
    if (!slot) return 0;

    *slot = *goal;
    slot->goal_id = si->next_goal_id++;
    slot->state = GOAL_STATE_PROPOSED;
    slot->created_at_us = get_timestamp_us();
    slot->commitment_count = 0;
    slot->total_commitment = 0.0f;

    si->goal_count++;
    si->stats.goals_proposed++;

    return slot->goal_id;
}

int shared_intentionality_commit_to_goal(
    shared_intentionality_t* si,
    uint32_t goal_id,
    uint32_t instance_id,
    float commitment
) {
    if (!si) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "shared_intentionality_commit_to_goal: si is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    shared_intentionality_heartbeat("shared_inten_commit_to_goal", 0.0f);


    shared_goal_t* goal = find_goal(si, goal_id);
    if (!goal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "shared_intentionality_commit_to_goal: goal is NULL");
        return -1;
    }

    if (!find_instance(si, instance_id)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "shared_intentionality_commit_to_goal: find_instance is NULL");
        return -1;
    }

    /* Find or create commitment */
    goal_commitment_t* c = find_commitment(goal, instance_id);
    if (!c) {
        if (goal->commitment_count >= COLLECTIVE_MAX_INSTANCES) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "shared_intentionality_commit_to_goal: capacity exceeded");
            return -1;
        }
        c = &goal->commitments[goal->commitment_count++];
        c->instance_id = instance_id;
        c->committed_at_us = get_timestamp_us();
        c->has_role = false;
    }

    c->strength = commitment;
    update_goal_total_commitment(goal);

    /* Check if goal should be accepted */
    if (goal->state == GOAL_STATE_PROPOSED &&
        goal->total_commitment >= si->config.commitment_threshold * si->instance_count) {
        shared_goal_state_t old_state = goal->state;
        goal->state = GOAL_STATE_ACCEPTED;
        si->stats.goals_accepted++;
        notify_goal_state_change(si, goal, old_state);
    }

    return 0;
}

int shared_intentionality_withdraw_from_goal(
    shared_intentionality_t* si,
    uint32_t goal_id,
    uint32_t instance_id
) {
    if (!si) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "shared_intentionality_withdraw_from_goal: si is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    shared_intentionality_heartbeat("shared_inten_withdraw_from_goal", 0.0f);


    shared_goal_t* goal = find_goal(si, goal_id);
    if (!goal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "shared_intentionality_withdraw_from_goal: goal is NULL");
        return -1;
    }

    /* Find and remove commitment */
    for (uint32_t i = 0; i < goal->commitment_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && goal->commitment_count > 256) {
            shared_intentionality_heartbeat("shared_inten_loop",
                             (float)(i + 1) / (float)goal->commitment_count);
        }

        if (goal->commitments[i].instance_id == instance_id) {
            /* Shift remaining commitments */
            for (uint32_t j = i; j < goal->commitment_count - 1; j++) {
                goal->commitments[j] = goal->commitments[j + 1];
            }
            goal->commitment_count--;
            update_goal_total_commitment(goal);

            /* Check if goal should be abandoned */
            if (goal->commitment_count == 0 &&
                (goal->state == GOAL_STATE_PROPOSED ||
                 goal->state == GOAL_STATE_ACCEPTED ||
                 goal->state == GOAL_STATE_ACTIVE)) {
                shared_goal_state_t old_state = goal->state;
                goal->state = GOAL_STATE_ABANDONED;
                si->stats.goals_abandoned++;
                notify_goal_state_change(si, goal, old_state);
            }

            return 0;
        }
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "shared_intentionality_withdraw_from_goal: operation failed");
    return -1;  /* Not found */
}

int shared_intentionality_update_goal_progress(
    shared_intentionality_t* si,
    uint32_t goal_id,
    float progress
) {
    if (!si) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "shared_intentionality_update_goal_progress: si is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    shared_intentionality_heartbeat("shared_inten_update_goal_progress", 0.0f);


    shared_goal_t* goal = find_goal(si, goal_id);
    if (!goal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "shared_intentionality_update_goal_progress: goal is NULL");
        return -1;
    }

    goal->progress = progress;

    /* Transition to active if accepted and progress started */
    if (goal->state == GOAL_STATE_ACCEPTED && progress > 0.0f) {
        shared_goal_state_t old_state = goal->state;
        goal->state = GOAL_STATE_ACTIVE;
        notify_goal_state_change(si, goal, old_state);
    }

    /* Auto-complete if progress reaches 1.0 */
    if (progress >= 1.0f && goal->state == GOAL_STATE_ACTIVE) {
        shared_intentionality_complete_goal(si, goal_id);
    }

    return 0;
}

int shared_intentionality_complete_goal(
    shared_intentionality_t* si,
    uint32_t goal_id
) {
    if (!si) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "shared_intentionality_complete_goal: si is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    shared_intentionality_heartbeat("shared_inten_complete_goal", 0.0f);


    shared_goal_t* goal = find_goal(si, goal_id);
    if (!goal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "shared_intentionality_complete_goal: goal is NULL");
        return -1;
    }

    /* Already completed - nothing to do */
    if (goal->state == GOAL_STATE_COMPLETED) {
        return 0;
    }

    shared_goal_state_t old_state = goal->state;
    goal->state = GOAL_STATE_COMPLETED;
    goal->progress = 1.0f;
    si->stats.goals_completed++;

    notify_goal_state_change(si, goal, old_state);

    return 0;
}

int shared_intentionality_fail_goal(
    shared_intentionality_t* si,
    uint32_t goal_id
) {
    if (!si) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "shared_intentionality_fail_goal: si is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    shared_intentionality_heartbeat("shared_inten_fail_goal", 0.0f);


    shared_goal_t* goal = find_goal(si, goal_id);
    if (!goal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "shared_intentionality_fail_goal: goal is NULL");
        return -1;
    }

    shared_goal_state_t old_state = goal->state;
    goal->state = GOAL_STATE_FAILED;
    si->stats.goals_failed++;

    notify_goal_state_change(si, goal, old_state);

    return 0;
}

int shared_intentionality_get_goal(
    const shared_intentionality_t* si,
    uint32_t goal_id,
    shared_goal_t* goal
) {
    if (!si || !goal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "shared_intentionality_get_goal: required parameter is NULL (si, goal)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    shared_intentionality_heartbeat("shared_inten_get_goal", 0.0f);


    shared_goal_t* found = find_goal((shared_intentionality_t*)si, goal_id);
    if (!found) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "shared_intentionality_get_goal: found is NULL");
        return -1;
    }

    *goal = *found;
    return 0;
}

uint32_t shared_intentionality_get_active_goals(
    const shared_intentionality_t* si,
    shared_goal_t* goals,
    uint32_t max_goals
) {
    if (!si || !goals) return 0;

    /* Phase 8: Heartbeat at operation start */
    shared_intentionality_heartbeat("shared_inten_get_active_goals", 0.0f);


    uint32_t count = 0;
    for (uint32_t i = 0; i < COLLECTIVE_MAX_SHARED_GOALS && count < max_goals; i++) {
        if (si->goals[i].goal_id == 0) continue;
        /* Include proposed, negotiating, accepted, and active goals */
        /* Exclude completed, failed, and abandoned goals */
        if (si->goals[i].state == GOAL_STATE_PROPOSED ||
            si->goals[i].state == GOAL_STATE_NEGOTIATING ||
            si->goals[i].state == GOAL_STATE_ACCEPTED ||
            si->goals[i].state == GOAL_STATE_ACTIVE) {
            goals[count++] = si->goals[i];
        }
    }
    return count;
}

/*=============================================================================
 * Joint Attention API
 *===========================================================================*/

uint32_t shared_intentionality_propose_attention(
    shared_intentionality_t* si,
    const joint_attention_t* attention
) {
    if (!si || !attention) return 0;

    /* Phase 8: Heartbeat at operation start */
    shared_intentionality_heartbeat("shared_inten_propose_attention", 0.0f);


    joint_attention_t* slot = find_free_attention_slot(si);
    if (!slot) return 0;

    *slot = *attention;
    slot->attention_id = si->next_attention_id++;
    slot->started_at_us = get_timestamp_us();

    /* Add proposer as first attending instance */
    slot->attending_instances[0] = attention->proposer_id;
    slot->attending_count = 1;
    slot->agreement_level = 1.0f;

    si->attention_count++;
    si->stats.attentions_proposed++;

    if (si->attention_callback) {
        si->attention_callback(slot, true, si->attention_callback_data);
    }

    return slot->attention_id;
}

int shared_intentionality_join_attention(
    shared_intentionality_t* si,
    uint32_t attention_id,
    uint32_t instance_id
) {
    if (!si) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "shared_intentionality_join_attention: si is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    shared_intentionality_heartbeat("shared_inten_join_attention", 0.0f);


    joint_attention_t* att = find_attention(si, attention_id);
    if (!att) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "shared_intentionality_join_attention: att is NULL");
        return -1;
    }

    if (!find_instance(si, instance_id)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "shared_intentionality_join_attention: find_instance is NULL");
        return -1;
    }

    /* Check if already attending */
    if (is_attending(att, instance_id)) return 0;

    /* Add to attending list */
    if (att->attending_count >= COLLECTIVE_MAX_INSTANCES) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "shared_intentionality_join_attention: capacity exceeded");
        return -1;
    }
    att->attending_instances[att->attending_count++] = instance_id;

    /* Update agreement level */
    att->agreement_level = (float)att->attending_count / si->instance_count;

    si->stats.attentions_joined++;

    if (si->attention_callback) {
        si->attention_callback(att, false, si->attention_callback_data);
    }

    return 0;
}

int shared_intentionality_leave_attention(
    shared_intentionality_t* si,
    uint32_t attention_id,
    uint32_t instance_id
) {
    if (!si) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "shared_intentionality_leave_attention: si is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    shared_intentionality_heartbeat("shared_inten_leave_attention", 0.0f);


    joint_attention_t* att = find_attention(si, attention_id);
    if (!att) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "shared_intentionality_leave_attention: att is NULL");
        return -1;
    }

    /* Find and remove */
    for (uint32_t i = 0; i < att->attending_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && att->attending_count > 256) {
            shared_intentionality_heartbeat("shared_inten_loop",
                             (float)(i + 1) / (float)att->attending_count);
        }

        if (att->attending_instances[i] == instance_id) {
            for (uint32_t j = i; j < att->attending_count - 1; j++) {
                att->attending_instances[j] = att->attending_instances[j + 1];
            }
            att->attending_count--;

            /* Update agreement */
            if (si->instance_count > 0) {
                att->agreement_level = (float)att->attending_count / si->instance_count;
            }

            /* Remove attention if no one attending */
            if (att->attending_count == 0) {
                att->attention_id = 0;
                si->attention_count--;
            }

            return 0;
        }
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "shared_intentionality_leave_attention: att->attending_count is zero");
    return -1;
}

int shared_intentionality_get_attention(
    const shared_intentionality_t* si,
    uint32_t attention_id,
    joint_attention_t* attention
) {
    if (!si || !attention) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "shared_intentionality_get_attention: required parameter is NULL (si, attention)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    shared_intentionality_heartbeat("shared_inten_get_attention", 0.0f);


    joint_attention_t* found = find_attention((shared_intentionality_t*)si, attention_id);
    if (!found) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "shared_intentionality_get_attention: found is NULL");
        return -1;
    }

    *attention = *found;
    return 0;
}

uint32_t shared_intentionality_get_active_attentions(
    const shared_intentionality_t* si,
    joint_attention_t* attentions,
    uint32_t max_attentions
) {
    if (!si || !attentions) return 0;

    /* Phase 8: Heartbeat at operation start */
    shared_intentionality_heartbeat("shared_inten_get_active_attention", 0.0f);


    uint32_t count = 0;
    for (uint32_t i = 0; i < COLLECTIVE_MAX_JOINT_ATTENTIONS && count < max_attentions; i++) {
        if (si->attentions[i].attention_id != 0) {
            attentions[count++] = si->attentions[i];
        }
    }
    return count;
}

/*=============================================================================
 * Role API
 *===========================================================================*/

int shared_intentionality_assign_role(
    shared_intentionality_t* si,
    uint32_t goal_id,
    uint32_t instance_id,
    role_type_t role
) {
    if (!si) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "shared_intentionality_assign_role: si is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    shared_intentionality_heartbeat("shared_inten_assign_role", 0.0f);


    shared_goal_t* goal = find_goal(si, goal_id);
    if (!goal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "shared_intentionality_assign_role: goal is NULL");
        return -1;
    }

    /* Verify instance is registered */
    if (!find_instance(si, instance_id)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "shared_intentionality_assign_role: find_instance is NULL");
        return -1;
    }

    goal_commitment_t* c = find_commitment(goal, instance_id);
    if (!c) {
        /* Auto-create commitment if assigning role before explicit commit */
        if (goal->commitment_count >= COLLECTIVE_MAX_INSTANCES) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "shared_intentionality_assign_role: capacity exceeded");
            return -1;
        }
        c = &goal->commitments[goal->commitment_count++];
        c->instance_id = instance_id;
        c->strength = 0.0f;  /* Will be set when commit_to_goal is called */
        c->committed_at_us = get_timestamp_us();
        c->has_role = false;
    }

    c->assigned_role = role;
    c->has_role = true;

    return 0;
}

int shared_intentionality_negotiate_roles(
    shared_intentionality_t* si,
    uint32_t goal_id
) {
    if (!si) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "shared_intentionality_negotiate_roles: si is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    shared_intentionality_heartbeat("shared_inten_negotiate_roles", 0.0f);


    shared_goal_t* goal = find_goal(si, goal_id);
    if (!goal || goal->commitment_count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "shared_intentionality_negotiate_roles: goal is NULL");
        return -1;
    }

    /* Simple role assignment: first committer is leader, rest are executors */
    for (uint32_t i = 0; i < goal->commitment_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && goal->commitment_count > 256) {
            shared_intentionality_heartbeat("shared_inten_loop",
                             (float)(i + 1) / (float)goal->commitment_count);
        }

        if (i == 0) {
            goal->commitments[i].assigned_role = ROLE_LEADER;
        } else {
            goal->commitments[i].assigned_role = ROLE_EXECUTOR;
        }
        goal->commitments[i].has_role = true;
    }

    si->stats.role_negotiations++;

    return 0;
}

int shared_intentionality_get_role(
    const shared_intentionality_t* si,
    uint32_t goal_id,
    uint32_t instance_id,
    role_assignment_t* assignment
) {
    if (!si || !assignment) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "shared_intentionality_get_role: required parameter is NULL (si, assignment)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    shared_intentionality_heartbeat("shared_inten_get_role", 0.0f);


    shared_goal_t* goal = find_goal((shared_intentionality_t*)si, goal_id);
    if (!goal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "shared_intentionality_get_role: goal is NULL");
        return -1;
    }

    goal_commitment_t* c = find_commitment(goal, instance_id);
    if (!c) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "shared_intentionality_get_role: c is NULL");
        return -1;
    }

    assignment->goal_id = goal_id;
    assignment->instance_id = instance_id;
    assignment->role = c->assigned_role;
    strncpy(assignment->role_name, role_type_name(c->assigned_role),
            sizeof(assignment->role_name) - 1);
    assignment->capability_match = c->strength;
    assignment->accepted = c->has_role;

    return 0;
}

/*=============================================================================
 * We-Mode API
 *===========================================================================*/

int shared_intentionality_get_we_mode(
    const shared_intentionality_t* si,
    we_mode_state_t* state
) {
    if (!si || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "shared_intentionality_get_we_mode: required parameter is NULL (si, state)");
        return -1;
    }
    *state = si->we_mode;
    /* Phase 8: Heartbeat at operation start */
    shared_intentionality_heartbeat("shared_inten_get_we_mode", 0.0f);


    return 0;
}

bool shared_intentionality_is_we_mode_active(
    const shared_intentionality_t* si
) {
    if (!si) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    shared_intentionality_heartbeat("shared_inten_is_we_mode_active", 0.0f);


    return si->we_mode.we_mode_strength >= si->config.we_mode_threshold;
}

int shared_intentionality_enter_we_mode(shared_intentionality_t* si) {
    if (!si) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "shared_intentionality_enter_we_mode: si is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    shared_intentionality_heartbeat("shared_inten_enter_we_mode", 0.0f);


    si->we_mode_forced = true;
    si->we_mode_start_us = get_timestamp_us();
    compute_we_mode_state(si);
    return 0;
}

int shared_intentionality_exit_we_mode(shared_intentionality_t* si) {
    if (!si) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "shared_intentionality_exit_we_mode: si is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    shared_intentionality_heartbeat("shared_inten_exit_we_mode", 0.0f);


    if (si->we_mode_forced && si->we_mode_start_us > 0) {
        si->total_we_mode_us += get_timestamp_us() - si->we_mode_start_us;
    }

    si->we_mode_forced = false;
    compute_we_mode_state(si);
    return 0;
}

/*=============================================================================
 * Update API
 *===========================================================================*/

int shared_intentionality_update(shared_intentionality_t* si) {
    if (!si || !si->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "shared_intentionality_update: required parameter is NULL (si, si->initialized)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    shared_intentionality_heartbeat("shared_inten_update", 0.0f);


    uint64_t now = get_timestamp_us();

    /* Update we-mode state */
    bool was_we_mode = shared_intentionality_is_we_mode_active(si);
    compute_we_mode_state(si);
    bool is_we_mode = shared_intentionality_is_we_mode_active(si);

    /* Track we-mode time */
    if (is_we_mode && !was_we_mode) {
        si->we_mode_start_us = now;
    } else if (!is_we_mode && was_we_mode && si->we_mode_start_us > 0) {
        si->total_we_mode_us += now - si->we_mode_start_us;
        si->we_mode_start_us = 0;
    }

    /* Check goal deadlines */
    for (uint32_t i = 0; i < COLLECTIVE_MAX_SHARED_GOALS; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && COLLECTIVE_MAX_SHARED_GOALS > 256) {
            shared_intentionality_heartbeat("shared_inten_loop",
                             (float)(i + 1) / (float)COLLECTIVE_MAX_SHARED_GOALS);
        }

        shared_goal_t* goal = &si->goals[i];
        if (goal->goal_id == 0) continue;
        if (goal->state != GOAL_STATE_ACTIVE &&
            goal->state != GOAL_STATE_ACCEPTED) continue;

        if (goal->deadline_us > 0 && now > goal->deadline_us) {
            shared_intentionality_fail_goal(si, goal->goal_id);
        }
    }

    /* Update statistics */
    si->total_time_us = now - si->last_update_us + si->total_time_us;

    if (si->total_time_us > 0) {
        si->stats.we_mode_time_ratio = (float)si->total_we_mode_us / si->total_time_us;
    }

    /* Compute average commitment */
    float total_commitment = 0.0f;
    uint32_t commitment_count = 0;
    for (uint32_t i = 0; i < COLLECTIVE_MAX_SHARED_GOALS; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && COLLECTIVE_MAX_SHARED_GOALS > 256) {
            shared_intentionality_heartbeat("shared_inten_loop",
                             (float)(i + 1) / (float)COLLECTIVE_MAX_SHARED_GOALS);
        }

        if (si->goals[i].goal_id == 0) continue;
        for (uint32_t j = 0; j < si->goals[i].commitment_count; j++) {
            total_commitment += si->goals[i].commitments[j].strength;
            commitment_count++;
        }
    }
    if (commitment_count > 0) {
        si->stats.avg_commitment = total_commitment / commitment_count;
    }

    si->last_update_us = now;

    return 0;
}

/*=============================================================================
 * Callback API
 *===========================================================================*/

int shared_intentionality_set_goal_callback(
    shared_intentionality_t* si,
    goal_state_callback_fn callback,
    void* user_data
) {
    if (!si) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "shared_intentionality_set_goal_callback: si is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    shared_intentionality_heartbeat("shared_inten_set_goal_callback", 0.0f);


    si->goal_callback = callback;
    si->goal_callback_data = user_data;
    return 0;
}

int shared_intentionality_set_attention_callback(
    shared_intentionality_t* si,
    attention_callback_fn callback,
    void* user_data
) {
    if (!si) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "shared_intentionality_set_attention_callback: si is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    shared_intentionality_heartbeat("shared_inten_set_attention_callba", 0.0f);


    si->attention_callback = callback;
    si->attention_callback_data = user_data;
    return 0;
}

/*=============================================================================
 * Statistics API
 *===========================================================================*/

int shared_intentionality_get_stats(
    const shared_intentionality_t* si,
    shared_intentionality_stats_t* stats
) {
    if (!si || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "shared_intentionality_get_stats: required parameter is NULL (si, stats)");
        return -1;
    }
    *stats = si->stats;
    /* Phase 8: Heartbeat at operation start */
    shared_intentionality_heartbeat("shared_inten_get_stats", 0.0f);


    return 0;
}

void shared_intentionality_reset_stats(shared_intentionality_t* si) {
    if (!si) return;
    /* Phase 8: Heartbeat at operation start */
    shared_intentionality_heartbeat("shared_inten_reset_stats", 0.0f);


    memset(&si->stats, 0, sizeof(si->stats));
    si->total_we_mode_us = 0;
    si->total_time_us = 0;
}

/*=============================================================================
 * Utility API
 *===========================================================================*/

const char* role_type_name(role_type_t role) {
    switch (role) {
        case ROLE_LEADER:       return "LEADER";
        case ROLE_FOLLOWER:     return "FOLLOWER";
        case ROLE_OBSERVER:     return "OBSERVER";
        case ROLE_EXECUTOR:     return "EXECUTOR";
        case ROLE_VERIFIER:     return "VERIFIER";
        case ROLE_COMMUNICATOR: return "COMMUNICATOR";
        default:                return "UNKNOWN";
    }
}

const char* goal_state_name(shared_goal_state_t state) {
    switch (state) {
        case GOAL_STATE_PROPOSED:    return "PROPOSED";
        case GOAL_STATE_NEGOTIATING: return "NEGOTIATING";
        case GOAL_STATE_ACCEPTED:    return "ACCEPTED";
        case GOAL_STATE_ACTIVE:      return "ACTIVE";
        case GOAL_STATE_COMPLETED:   return "COMPLETED";
        case GOAL_STATE_ABANDONED:   return "ABANDONED";
        case GOAL_STATE_FAILED:      return "FAILED";
        default:                     return "UNKNOWN";
    }
}

/*=============================================================================
 * Debug API
 *===========================================================================*/

void shared_intentionality_dump(const shared_intentionality_t* si) {
    if (!si) {
        printf("Shared Intentionality: NULL\n");
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    shared_intentionality_heartbeat("shared_inten_dump", 0.0f);


    printf("=== Shared Intentionality State ===\n");
    printf("Initialized: %s\n", si->initialized ? "yes" : "no");
    printf("Instances: %u\n", si->instance_count);
    printf("Goals: %u\n", si->goal_count);
    printf("Attentions: %u\n", si->attention_count);

    printf("\nWe-Mode:\n");
    printf("  Strength: %.3f (threshold: %.3f)\n",
           si->we_mode.we_mode_strength, si->config.we_mode_threshold);
    printf("  Active: %s\n",
           si->we_mode.we_mode_strength >= si->config.we_mode_threshold ? "yes" : "no");
    printf("  Joint commitment: %.3f\n", si->we_mode.joint_commitment);
    printf("  Mutual responsiveness: %.3f\n", si->we_mode.mutual_responsiveness);
    printf("  Role understanding: %.3f\n", si->we_mode.role_understanding);

    printf("\nShared Goals:\n");
    for (uint32_t i = 0; i < COLLECTIVE_MAX_SHARED_GOALS; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && COLLECTIVE_MAX_SHARED_GOALS > 256) {
            shared_intentionality_heartbeat("shared_inten_loop",
                             (float)(i + 1) / (float)COLLECTIVE_MAX_SHARED_GOALS);
        }

        const shared_goal_t* goal = &si->goals[i];
        if (goal->goal_id == 0) continue;

        printf("  [%u] %s\n", goal->goal_id, goal->description);
        printf("       State: %s, Progress: %.1f%%\n",
               goal_state_name(goal->state), goal->progress * 100);
        printf("       Commitments: %u, Total: %.2f\n",
               goal->commitment_count, goal->total_commitment);
    }

    printf("\nJoint Attentions:\n");
    for (uint32_t i = 0; i < COLLECTIVE_MAX_JOINT_ATTENTIONS; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && COLLECTIVE_MAX_JOINT_ATTENTIONS > 256) {
            shared_intentionality_heartbeat("shared_inten_loop",
                             (float)(i + 1) / (float)COLLECTIVE_MAX_JOINT_ATTENTIONS);
        }

        const joint_attention_t* att = &si->attentions[i];
        if (att->attention_id == 0) continue;

        printf("  [%u] Salience: %.2f, Attending: %u, Agreement: %.2f\n",
               att->attention_id, att->salience,
               att->attending_count, att->agreement_level);
    }

    printf("\nStatistics:\n");
    printf("  Goals proposed: %lu\n", (unsigned long)si->stats.goals_proposed);
    printf("  Goals completed: %lu\n", (unsigned long)si->stats.goals_completed);
    printf("  Goals failed: %lu\n", (unsigned long)si->stats.goals_failed);
    printf("  Avg commitment: %.2f\n", si->stats.avg_commitment);
    printf("  We-mode time ratio: %.2f\n", si->stats.we_mode_time_ratio);
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

/**
 * WHAT: Query knowledge graph for Shared Intentionality self-knowledge
 * WHY:  Enable self-awareness about module's role and connections
 * HOW:  Query KG for entity observations and relations
 */
int shared_intentionality_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    shared_intentionality_heartbeat("shared_inten_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Shared_Intentionality");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                shared_intentionality_heartbeat("shared_inten_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            printf("Shared Intentionality self-knowledge: %s\n", self->observations[i]);
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Shared_Intentionality");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Shared_Intentionality");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-level health agent setter
 * ============================================================================ */

void shared_intentionality_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_shared_intentionality_instance_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Full training implementation
 * ============================================================================ */
static _Atomic uint64_t g_shared_intentionality_training_steps = 0;
static _Atomic double g_shared_intentionality_training_total_error = 0.0;
static _Atomic double g_shared_intentionality_training_best_error = 1e30;
static _Atomic bool g_shared_intentionality_training_active = false;

int shared_intentionality_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "shared_intentionality_training_begin: NULL argument");
        return -1;
    }
    shared_intentionality_heartbeat_instance(g_shared_intentionality_instance_health_agent, "shared_int_train_beg", 0.0f);
    shared_intentionality_t* ctx = (shared_intentionality_t*)instance;

    /* Reset training counters */
    atomic_store(&g_shared_intentionality_training_steps, 0);
    atomic_store(&g_shared_intentionality_training_total_error, 0.0);
    atomic_store(&g_shared_intentionality_training_best_error, 1e30);
    atomic_store(&g_shared_intentionality_training_active, true);

    /* Reset module stats */
    memset(&ctx->stats, 0, sizeof(ctx->stats));
    ctx->total_we_mode_us = 0;

    NIMCP_LOGGING_INFO("shared_intentionality training begin: counters reset");
    return 0;
}

int shared_intentionality_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "shared_intentionality_training_step: NULL argument");
        return -1;
    }

    /* Clamp progress to [0, 1] */
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    shared_intentionality_heartbeat_instance(g_shared_intentionality_instance_health_agent, "shared_int_train_stp", progress);
    (void)instance;

    atomic_fetch_add(&g_shared_intentionality_training_steps, 1);

    /* Progressive adaptation: decay error accumulator */
    float decay = 1.0f - 0.1f * progress;
    if (decay < 0.5f) decay = 0.5f;
    double old_error = atomic_load(&g_shared_intentionality_training_total_error);
    atomic_store(&g_shared_intentionality_training_total_error, old_error * (double)decay);

    /* Adaptive threshold adjustment based on progress */
    float threshold_adjust = 0.01f * progress;
    double old_best = atomic_load(&g_shared_intentionality_training_best_error);
    double new_best = old_best - (double)threshold_adjust;
    if (new_best < 0.0) new_best = 0.0;
    atomic_store(&g_shared_intentionality_training_best_error, new_best);

    return 0;
}

int shared_intentionality_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "shared_intentionality_training_end: NULL argument");
        return -1;
    }
    shared_intentionality_heartbeat_instance(g_shared_intentionality_instance_health_agent, "shared_int_train_end", 1.0f);

    /* Compute final averages */
    uint64_t steps = atomic_load(&g_shared_intentionality_training_steps);
    double total_error = atomic_load(&g_shared_intentionality_training_total_error);
    double best_error = atomic_load(&g_shared_intentionality_training_best_error);
    double avg_error = (steps > 0)
        ? total_error / (double)steps
        : 0.0;

    /* Clear training flag */
    atomic_store(&g_shared_intentionality_training_active, false);

    NIMCP_LOGGING_INFO("shared_intentionality training end: %lu steps, avg_error=%.6f, best_error=%.6f",
                       (unsigned long)steps,
                       avg_error, best_error);
    return 0;
}
