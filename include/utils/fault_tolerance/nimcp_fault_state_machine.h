/**
 * @file nimcp_fault_state_machine.h
 * @brief Fault Tolerance State Machine
 *
 * WHAT: Explicit brain state management (HEALTHY, DEGRADED, RECOVERING, FAILED, SHUTDOWN)
 * WHY:  Clear state transitions, validation, debugging, prevent invalid state changes
 * HOW:  State enum, transition rules, guards, history tracking, entry/exit callbacks
 *
 * State Transition Rules:
 * - HEALTHY -> DEGRADED, FAILED, SHUTDOWN
 * - DEGRADED -> HEALTHY, RECOVERING, FAILED, SHUTDOWN
 * - RECOVERING -> HEALTHY, DEGRADED, FAILED, SHUTDOWN
 * - FAILED -> RECOVERING, SHUTDOWN
 * - SHUTDOWN -> (terminal state)
 */

#ifndef NIMCP_FAULT_STATE_MACHINE_H
#define NIMCP_FAULT_STATE_MACHINE_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Brain operational states
 */
typedef enum {
    NIMCP_STATE_HEALTHY = 0,    /**< Normal operation */
    NIMCP_STATE_DEGRADED,       /**< Reduced functionality */
    NIMCP_STATE_RECOVERING,     /**< Attempting to restore */
    NIMCP_STATE_FAILED,         /**< Critical failure */
    NIMCP_STATE_SHUTDOWN,       /**< Orderly shutdown (terminal) */
    NIMCP_STATE_COUNT           /**< Number of states */
} nimcp_brain_state_t;

/**
 * @brief Transition result codes
 */
typedef enum {
    NIMCP_TRANSITION_SUCCESS = 0,       /**< Transition completed */
    NIMCP_TRANSITION_INVALID,           /**< Invalid transition */
    NIMCP_TRANSITION_BLOCKED,           /**< Transition blocked by guard */
    NIMCP_TRANSITION_CALLBACK_FAILED,   /**< Entry/exit callback failed */
    NIMCP_TRANSITION_ERROR              /**< Internal error */
} nimcp_transition_result_t;

/**
 * @brief State transition event record
 */
typedef struct {
    nimcp_brain_state_t from_state;     /**< Source state */
    nimcp_brain_state_t to_state;       /**< Target state */
    time_t timestamp;                   /**< Transition time */
    uint32_t reason_code;               /**< Application-defined reason */
} nimcp_state_transition_t;

/**
 * @brief State history tracking (circular buffer)
 */
#define NIMCP_STATE_HISTORY_SIZE 64

typedef struct {
    nimcp_state_transition_t transitions[NIMCP_STATE_HISTORY_SIZE];
    uint32_t head;          /**< Next write position */
    uint32_t count;         /**< Number of transitions recorded */
} nimcp_state_history_t;

/**
 * @brief State entry/exit callback
 * @param state The state being entered/exited
 * @param user_data User-provided context
 * @return true if callback succeeds, false to block transition
 */
typedef bool (*nimcp_state_callback_t)(nimcp_brain_state_t state, void* user_data);

/**
 * @brief State guard function (validates transitions)
 * @param from_state Source state
 * @param to_state Target state
 * @param user_data User-provided context
 * @return true if transition is allowed, false to block
 */
typedef bool (*nimcp_state_guard_t)(nimcp_brain_state_t from_state,
                                    nimcp_brain_state_t to_state,
                                    void* user_data);

/**
 * @brief State machine instance
 */
typedef struct {
    nimcp_brain_state_t current_state;      /**< Current state */
    nimcp_state_history_t history;          /**< Transition history */

    /* Callbacks */
    nimcp_state_callback_t on_entry[NIMCP_STATE_COUNT];
    nimcp_state_callback_t on_exit[NIMCP_STATE_COUNT];
    nimcp_state_guard_t guard;
    void* user_data;

    /* Statistics */
    uint64_t state_durations[NIMCP_STATE_COUNT];  /**< Total time in each state (seconds) */
    time_t state_entry_time;                       /**< When current state was entered */
    uint32_t transition_attempts;                  /**< Total transition attempts */
    uint32_t transition_failures;                  /**< Failed transitions */
} nimcp_state_machine_t;

/* =============================================================================
 * Core State Machine Functions
 * ============================================================================= */

/**
 * @brief Initialize state machine
 * WHAT: Creates and initializes state machine in HEALTHY state
 * WHY:  Required before any state operations
 * HOW:  Allocates memory, sets initial state, clears history
 *
 * @return Initialized state machine, NULL on failure
 */
nimcp_state_machine_t* nimcp_state_machine_create(void);

/**
 * @brief Destroy state machine
 * WHAT: Frees state machine resources
 * WHY:  Prevents memory leaks
 * HOW:  Validates pointer, frees memory
 *
 * @param sm State machine to destroy
 */
void nimcp_state_machine_destroy(nimcp_state_machine_t* sm);

/**
 * @brief Attempt state transition
 * WHAT: Transitions state machine to new state with validation
 * WHY:  Ensures valid state transitions, maintains history
 * HOW:  Checks transition validity, runs guards, executes callbacks, records history
 *
 * @param sm State machine
 * @param new_state Target state
 * @param reason_code Application-defined reason for transition
 * @return Transition result code
 */
nimcp_transition_result_t nimcp_state_machine_transition(
    nimcp_state_machine_t* sm,
    nimcp_brain_state_t new_state,
    uint32_t reason_code
);

/**
 * @brief Get current state
 * WHAT: Returns current state of the machine
 * WHY:  Query current operational status
 * HOW:  Simple accessor
 *
 * @param sm State machine
 * @return Current state, NIMCP_STATE_FAILED if sm is NULL
 */
nimcp_brain_state_t nimcp_state_machine_get_state(const nimcp_state_machine_t* sm);

/* =============================================================================
 * Callback Registration
 * ============================================================================= */

/**
 * @brief Register state entry callback
 * WHAT: Sets callback to execute when entering a state
 * WHY:  Allows custom logic on state entry
 * HOW:  Stores callback pointer for specified state
 *
 * @param sm State machine
 * @param state State to attach callback to
 * @param callback Callback function
 * @return true on success, false on error
 */
bool nimcp_state_machine_set_entry_callback(
    nimcp_state_machine_t* sm,
    nimcp_brain_state_t state,
    nimcp_state_callback_t callback
);

/**
 * @brief Register state exit callback
 * WHAT: Sets callback to execute when exiting a state
 * WHY:  Allows cleanup or validation on state exit
 * HOW:  Stores callback pointer for specified state
 *
 * @param sm State machine
 * @param state State to attach callback to
 * @param callback Callback function
 * @return true on success, false on error
 */
bool nimcp_state_machine_set_exit_callback(
    nimcp_state_machine_t* sm,
    nimcp_brain_state_t state,
    nimcp_state_callback_t callback
);

/**
 * @brief Register transition guard
 * WHAT: Sets guard function to validate all transitions
 * WHY:  Allows custom transition validation logic
 * HOW:  Stores guard function pointer
 *
 * @param sm State machine
 * @param guard Guard function
 * @return true on success, false on error
 */
bool nimcp_state_machine_set_guard(
    nimcp_state_machine_t* sm,
    nimcp_state_guard_t guard
);

/**
 * @brief Set user data for callbacks
 * WHAT: Associates user context with state machine
 * WHY:  Callbacks need access to application state
 * HOW:  Stores pointer, passed to all callbacks
 *
 * @param sm State machine
 * @param user_data User context pointer
 */
void nimcp_state_machine_set_user_data(nimcp_state_machine_t* sm, void* user_data);

/* =============================================================================
 * State Validation
 * ============================================================================= */

/**
 * @brief Check if transition is valid
 * WHAT: Validates if transition is allowed by state machine rules
 * WHY:  Query transition validity without attempting it
 * HOW:  Checks transition matrix
 *
 * @param from_state Source state
 * @param to_state Target state
 * @return true if transition is valid, false otherwise
 */
bool nimcp_state_machine_is_valid_transition(
    nimcp_brain_state_t from_state,
    nimcp_brain_state_t to_state
);

/**
 * @brief Check if state is terminal
 * WHAT: Determines if state allows no further transitions
 * WHY:  Detect terminal states (SHUTDOWN)
 * HOW:  Checks state against terminal list
 *
 * @param state State to check
 * @return true if terminal, false otherwise
 */
bool nimcp_state_machine_is_terminal(nimcp_brain_state_t state);

/* =============================================================================
 * History and Statistics
 * ============================================================================= */

/**
 * @brief Get transition history
 * WHAT: Returns array of recent state transitions
 * WHY:  Debug, audit, analyze state behavior
 * HOW:  Returns circular buffer contents
 *
 * @param sm State machine
 * @param count Output: number of transitions returned
 * @return Array of transitions (newest first), NULL on error
 */
const nimcp_state_transition_t* nimcp_state_machine_get_history(
    const nimcp_state_machine_t* sm,
    uint32_t* count
);

/**
 * @brief Get time spent in current state
 * WHAT: Calculates duration in current state
 * WHY:  Monitor state stability
 * HOW:  Subtracts entry time from current time
 *
 * @param sm State machine
 * @return Seconds in current state, 0 on error
 */
uint64_t nimcp_state_machine_get_current_state_duration(const nimcp_state_machine_t* sm);

/**
 * @brief Get total time spent in a state
 * WHAT: Returns cumulative time in specified state
 * WHY:  Analyze state distribution over lifetime
 * HOW:  Returns accumulated duration counter
 *
 * @param sm State machine
 * @param state State to query
 * @return Total seconds in state, 0 on error
 */
uint64_t nimcp_state_machine_get_total_state_duration(
    const nimcp_state_machine_t* sm,
    nimcp_brain_state_t state
);

/**
 * @brief Get transition statistics
 * WHAT: Returns success/failure counts for transitions
 * WHY:  Monitor state machine health
 * HOW:  Returns internal counters
 *
 * @param sm State machine
 * @param attempts Output: total transition attempts
 * @param failures Output: failed transitions
 * @return true on success, false on error
 */
bool nimcp_state_machine_get_statistics(
    const nimcp_state_machine_t* sm,
    uint32_t* attempts,
    uint32_t* failures
);

/**
 * @brief Reset statistics
 * WHAT: Clears duration and transition counters
 * WHY:  Start fresh statistics collection
 * HOW:  Zeros all statistics, keeps current state
 *
 * @param sm State machine
 */
void nimcp_state_machine_reset_statistics(nimcp_state_machine_t* sm);

/* =============================================================================
 * Utility Functions
 * ============================================================================= */

/**
 * @brief Convert state to string
 * WHAT: Returns human-readable state name
 * WHY:  Logging, debugging, display
 * HOW:  Lookup table
 *
 * @param state State to convert
 * @return State name string
 */
const char* nimcp_state_to_string(nimcp_brain_state_t state);

/**
 * @brief Convert transition result to string
 * WHAT: Returns human-readable result description
 * WHY:  Error reporting, logging
 * HOW:  Lookup table
 *
 * @param result Transition result
 * @return Result description string
 */
const char* nimcp_transition_result_to_string(nimcp_transition_result_t result);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_FAULT_STATE_MACHINE_H */
