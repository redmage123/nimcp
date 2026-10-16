/**
 * @file nimcp_fault_state_machine.c
 * @brief Fault Tolerance State Machine Implementation
 */

#include "utils/fault_tolerance/nimcp_fault_state_machine.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"

#define LOG_MODULE "utils_fault_state_machine"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(fault_state_machine)

#include <stdlib.h>
#include <string.h>
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/logging/nimcp_logging.h"

/* =============================================================================
 * State Transition Matrix
 * Defines valid transitions between states
 * ============================================================================= */

/* Transition validity matrix: [from_state][to_state] */
static const bool transition_matrix[NIMCP_STATE_COUNT][NIMCP_STATE_COUNT] = {
    /* TO:      HEALTHY, DEGRADED, RECOVERING, FAILED, SHUTDOWN */
    /* HEALTHY */    {false,   true,     false,      true,   true},
    /* DEGRADED */   {true,    false,    true,       true,   true},
    /* RECOVERING */ {true,    true,     false,      true,   true},
    /* FAILED */     {false,   false,    true,       false,  true},
    /* SHUTDOWN */   {false,   false,    false,      false,  false}
};

/* =============================================================================
 * Core State Machine Functions
 * ============================================================================= */

nimcp_state_machine_t* nimcp_state_machine_create(void) {
    nimcp_state_machine_t* sm = (nimcp_state_machine_t*)nimcp_calloc(1, sizeof(nimcp_state_machine_t));
    NIMCP_API_CHECK_ALLOC(sm, "nimcp_state_machine_create: allocation failed");

    /* Initialize to HEALTHY state */
    sm->current_state = NIMCP_STATE_HEALTHY;
    sm->state_entry_time = time(NULL);

    /* Clear history */
    memset(&sm->history, 0, sizeof(nimcp_state_history_t));

    /* Clear callbacks */
    memset(sm->on_entry, 0, sizeof(sm->on_entry));
    memset(sm->on_exit, 0, sizeof(sm->on_exit));
    sm->guard = NULL;
    sm->user_data = NULL;

    /* Clear statistics */
    memset(sm->state_durations, 0, sizeof(sm->state_durations));
    sm->transition_attempts = 0;
    sm->transition_failures = 0;

    return sm;
}

void nimcp_state_machine_destroy(nimcp_state_machine_t* sm) {
    if (!sm) {
        return;
    }

    /* Update duration for current state before destruction */
    time_t now = time(NULL);
    if (sm->state_entry_time > 0 && now >= sm->state_entry_time) {
        sm->state_durations[sm->current_state] += (now - sm->state_entry_time);
    }

    nimcp_free(sm);
}

nimcp_transition_result_t nimcp_state_machine_transition(
    nimcp_state_machine_t* sm,
    nimcp_brain_state_t new_state,
    uint32_t reason_code
) {
    if (!sm) {
        return NIMCP_TRANSITION_ERROR;
    }

    /* Validate state enum */
    if (new_state >= NIMCP_STATE_COUNT) {
        return NIMCP_TRANSITION_INVALID;
    }

    sm->transition_attempts++;

    nimcp_brain_state_t old_state = sm->current_state;

    /* Check if this is a self-transition (no-op) */
    if (old_state == new_state) {
        return NIMCP_TRANSITION_SUCCESS;
    }

    /* Validate transition is allowed */
    if (!nimcp_state_machine_is_valid_transition(old_state, new_state)) {
        sm->transition_failures++;
        return NIMCP_TRANSITION_INVALID;
    }

    /* Check guard function if set */
    if (sm->guard && !sm->guard(old_state, new_state, sm->user_data)) {
        sm->transition_failures++;
        return NIMCP_TRANSITION_BLOCKED;
    }

    /* Execute exit callback for old state */
    if (sm->on_exit[old_state]) {
        if (!sm->on_exit[old_state](old_state, sm->user_data)) {
            sm->transition_failures++;
            return NIMCP_TRANSITION_CALLBACK_FAILED;
        }
    }

    /* Update state duration for old state */
    time_t now = time(NULL);
    if (sm->state_entry_time > 0 && now >= sm->state_entry_time) {
        sm->state_durations[old_state] += (now - sm->state_entry_time);
    }

    /* Perform state transition */
    sm->current_state = new_state;
    sm->state_entry_time = now;

    /* Execute entry callback for new state */
    if (sm->on_entry[new_state]) {
        if (!sm->on_entry[new_state](new_state, sm->user_data)) {
            /* Callback failed - should we rollback? For now, we stay in new state */
            /* but mark as callback failure */
            sm->transition_failures++;
            return NIMCP_TRANSITION_CALLBACK_FAILED;
        }
    }

    /* Record transition in history */
    uint32_t idx = sm->history.head;
    sm->history.transitions[idx].from_state = old_state;
    sm->history.transitions[idx].to_state = new_state;
    sm->history.transitions[idx].timestamp = now;
    sm->history.transitions[idx].reason_code = reason_code;

    sm->history.head = (sm->history.head + 1) % NIMCP_STATE_HISTORY_SIZE;
    if (sm->history.count < NIMCP_STATE_HISTORY_SIZE) {
        sm->history.count++;
    }

    return NIMCP_TRANSITION_SUCCESS;
}

nimcp_brain_state_t nimcp_state_machine_get_state(const nimcp_state_machine_t* sm) {
    if (!sm) {
        return NIMCP_STATE_FAILED;
    }
    return sm->current_state;
}

/* =============================================================================
 * Callback Registration
 * ============================================================================= */

bool nimcp_state_machine_set_entry_callback(
    nimcp_state_machine_t* sm,
    nimcp_brain_state_t state,
    nimcp_state_callback_t callback
) {
    if (!sm || state >= NIMCP_STATE_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,

                "nimcp_state_machine_set_entry_callback: invalid parameters");

            return false;
    }
    sm->on_entry[state] = callback;
    return true;
}

bool nimcp_state_machine_set_exit_callback(
    nimcp_state_machine_t* sm,
    nimcp_brain_state_t state,
    nimcp_state_callback_t callback
) {
    if (!sm || state >= NIMCP_STATE_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,

                "nimcp_state_machine_set_exit_callback: invalid parameters");

            return false;
    }
    sm->on_exit[state] = callback;
    return true;
}

bool nimcp_state_machine_set_guard(
    nimcp_state_machine_t* sm,
    nimcp_state_guard_t guard
) {
    if (!sm) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "nimcp_state_machine_set_guard: sm is NULL");

            return false;
    }
    sm->guard = guard;
    return true;
}

void nimcp_state_machine_set_user_data(nimcp_state_machine_t* sm, void* user_data) {
    if (sm) {
        sm->user_data = user_data;
    }
}

/* =============================================================================
 * State Validation
 * ============================================================================= */

bool nimcp_state_machine_is_valid_transition(
    nimcp_brain_state_t from_state,
    nimcp_brain_state_t to_state
) {
    if (from_state >= NIMCP_STATE_COUNT || to_state >= NIMCP_STATE_COUNT) {
        return false;
    }
    return transition_matrix[from_state][to_state];
}

bool nimcp_state_machine_is_terminal(nimcp_brain_state_t state) {
    return (state == NIMCP_STATE_SHUTDOWN);
}

/* =============================================================================
 * History and Statistics
 * ============================================================================= */

const nimcp_state_transition_t* nimcp_state_machine_get_history(
    const nimcp_state_machine_t* sm,
    uint32_t* count
) {
    if (!sm || !count) {
        if (count) *count = 0;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_state_machine_get_history: validation failed");
        return NULL;
    }

    *count = sm->history.count;
    if (*count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_state_machine_get_history: count is zero");
        return NULL;
    }

    /* Return pointer to transitions array */
    /* Caller must handle circular buffer logic if needed */
    return sm->history.transitions;
}

uint64_t nimcp_state_machine_get_current_state_duration(const nimcp_state_machine_t* sm) {
    if (!sm) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "nimcp_state_machine_get_current_state_duration: sm is NULL");

            return 0;
    }

    time_t now = time(NULL);
    if (sm->state_entry_time > 0 && now >= sm->state_entry_time) {
        return (uint64_t)(now - sm->state_entry_time);
    }

    return 0;
}

uint64_t nimcp_state_machine_get_total_state_duration(
    const nimcp_state_machine_t* sm,
    nimcp_brain_state_t state
) {
    if (!sm || state >= NIMCP_STATE_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,

                "nimcp_state_machine_get_total_state_duration: invalid parameters");

            return 0;
    }

    uint64_t total = sm->state_durations[state];

    /* If currently in this state, add current duration */
    if (sm->current_state == state) {
        total += nimcp_state_machine_get_current_state_duration(sm);
    }

    return total;
}

bool nimcp_state_machine_get_statistics(
    const nimcp_state_machine_t* sm,
    uint32_t* attempts,
    uint32_t* failures
) {
    if (!sm || !attempts || !failures) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,

                "nimcp_state_machine_get_statistics: invalid parameters");

            return false;
    }

    *attempts = sm->transition_attempts;
    *failures = sm->transition_failures;
    return true;
}

void nimcp_state_machine_reset_statistics(nimcp_state_machine_t* sm) {
    if (!sm) {
        return;
    }

    /* Reset duration counters */
    memset(sm->state_durations, 0, sizeof(sm->state_durations));

    /* Reset transition counters */
    sm->transition_attempts = 0;
    sm->transition_failures = 0;

    /* Reset entry time for current state */
    sm->state_entry_time = time(NULL);
}

/* =============================================================================
 * Utility Functions
 * ============================================================================= */

const char* nimcp_state_to_string(nimcp_brain_state_t state) {
    switch (state) {
        case NIMCP_STATE_HEALTHY:    return "HEALTHY";
        case NIMCP_STATE_DEGRADED:   return "DEGRADED";
        case NIMCP_STATE_RECOVERING: return "RECOVERING";
        case NIMCP_STATE_FAILED:     return "FAILED";
        case NIMCP_STATE_SHUTDOWN:   return "SHUTDOWN";
        default:                     return "UNKNOWN";
    }
}

const char* nimcp_transition_result_to_string(nimcp_transition_result_t result) {
    switch (result) {
        case NIMCP_TRANSITION_SUCCESS:          return "SUCCESS";
        case NIMCP_TRANSITION_INVALID:          return "INVALID";
        case NIMCP_TRANSITION_BLOCKED:          return "BLOCKED";
        case NIMCP_TRANSITION_CALLBACK_FAILED:  return "CALLBACK_FAILED";
        case NIMCP_TRANSITION_ERROR:            return "ERROR";
        default:                                return "UNKNOWN";
    }
}
