/**
 * @file nimcp_prefrontal_adapter.h
 * @brief Brain adapter for Prefrontal Cortex integration
 *
 * WHAT: Unified adapter connecting Prefrontal Cortex sub-modules to the brain system
 * WHY:  Enable seamless integration with cognitive layers, executive functions, and decision-making
 * HOW:  Orchestrates executive function, working memory, and impulse control as a cohesive unit
 *
 * ARCHITECTURE:
 * - Wraps prefrontal sub-modules (executive, planning, inhibition)
 * - Provides high-level API for executive function pipeline
 * - Integrates with working memory for goal maintenance
 * - Connects to event bus for inter-module communication
 * - Supports training through reinforcement learning adapters
 *
 * BIOLOGICAL BASIS:
 * - Models Brodmann areas 9, 10, 11, 44, 45, 46, 47
 * - Dorsolateral PFC (BA9/46): Working memory, cognitive control
 * - Ventromedial PFC (BA10/11): Decision-making, value-based choice
 * - Orbitofrontal Cortex (BA11/47): Reward processing, impulse control
 * - Anterior Cingulate (BA32): Conflict monitoring, error detection
 *
 * EXECUTIVE FUNCTIONS:
 * - Goal Maintenance: Sustained representation of task objectives
 * - Planning: Multi-step action sequence generation
 * - Decision-Making: Value-based choice between alternatives
 * - Cognitive Flexibility: Task switching and rule adaptation
 * - Inhibitory Control: Suppression of inappropriate responses
 * - Working Memory Integration: Active maintenance of relevant information
 *
 * @version Phase PFC-1: Prefrontal Cortex Brain Integration
 * @date 2025-12-30
 */

#ifndef NIMCP_PREFRONTAL_ADAPTER_H
#define NIMCP_PREFRONTAL_ADAPTER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Bio-async communication system */
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

/* Logging system */
#include "utils/logging/nimcp_logging.h"

/* Unified memory system */
#include "utils/memory/nimcp_unified_memory.h"

/* Forward declaration for opaque adapter type */
typedef struct prefrontal_adapter prefrontal_adapter_t;

/*=============================================================================
 * CONFIGURATION
 *===========================================================================*/

/**
 * @brief Default configuration values
 */
#define PREFRONTAL_DEFAULT_MAX_GOALS              8
#define PREFRONTAL_DEFAULT_MAX_ACTIONS            64
#define PREFRONTAL_DEFAULT_WORKING_MEMORY_SLOTS   7
#define PREFRONTAL_DEFAULT_PLANNING_HORIZON       10
#define PREFRONTAL_DEFAULT_DECISION_TIMEOUT_MS    500.0f
#define PREFRONTAL_DEFAULT_INHIBITION_THRESHOLD   0.7f

/**
 * @brief Prefrontal cortex adapter configuration
 */
typedef struct {
    /* Capacity limits */
    uint32_t max_goals;                 /**< Maximum concurrent goals */
    uint32_t max_actions;               /**< Maximum actions in plan */
    uint32_t max_options;               /**< Maximum decision options to evaluate */

    /* Working memory integration */
    uint32_t working_memory_slots;      /**< Slots for goal maintenance */
    bool enable_working_memory;         /**< Enable WM integration */

    /* Planning parameters */
    uint32_t planning_horizon;          /**< Max steps to plan ahead */
    float planning_discount;            /**< Temporal discount factor (gamma) */
    bool enable_hierarchical_planning;  /**< Enable hierarchical goal decomposition */

    /* Decision-making parameters */
    float decision_timeout_ms;          /**< Max time for decision */
    float decision_threshold;           /**< Confidence threshold for action */
    bool enable_value_learning;         /**< Enable value function learning */

    /* Inhibitory control */
    float inhibition_threshold;         /**< Threshold for response inhibition */
    float impulse_decay_rate;           /**< Rate of impulse strength decay */
    bool enable_conflict_monitoring;    /**< Enable ACC conflict detection */

    /* Cognitive flexibility */
    float switch_cost;                  /**< Cost penalty for task switching */
    bool enable_rule_learning;          /**< Enable rule extraction from experience */

    /* Event system */
    bool enable_events;                 /**< Enable event bus integration */

    /* Training */
    bool enable_training;               /**< Enable learning capabilities */
    float learning_rate;                /**< Base learning rate for value learning */
    float exploration_rate;             /**< Epsilon for exploration vs exploitation */

    /* Bio-async communication */
    bool enable_bio_async;              /**< Enable bio-async messaging */
    nimcp_bio_channel_type_t default_channel; /**< Default neuromodulator channel */
} prefrontal_config_t;

/*=============================================================================
 * STATUS AND STATE
 *===========================================================================*/

/**
 * @brief Processing status of the adapter
 */
typedef enum {
    PREFRONTAL_STATUS_IDLE = 0,         /**< Ready for input */
    PREFRONTAL_STATUS_GOAL_SELECTION,   /**< Selecting active goal */
    PREFRONTAL_STATUS_PLANNING,         /**< Generating action plan */
    PREFRONTAL_STATUS_DECISION,         /**< Evaluating decision options */
    PREFRONTAL_STATUS_INHIBITION,       /**< Checking impulse control */
    PREFRONTAL_STATUS_EXECUTING,        /**< Action execution in progress */
    PREFRONTAL_STATUS_MONITORING,       /**< Monitoring execution outcomes */
    PREFRONTAL_STATUS_ERROR             /**< Error state */
} prefrontal_status_t;

/**
 * @brief Error codes for prefrontal operations
 */
typedef enum {
    PREFRONTAL_ERROR_NONE = 0,
    PREFRONTAL_ERROR_INVALID_INPUT,
    PREFRONTAL_ERROR_GOAL_CONFLICT,
    PREFRONTAL_ERROR_PLANNING_FAILURE,
    PREFRONTAL_ERROR_DECISION_TIMEOUT,
    PREFRONTAL_ERROR_INHIBITION_TRIGGERED,
    PREFRONTAL_ERROR_WORKING_MEMORY_FULL,
    PREFRONTAL_ERROR_BUFFER_OVERFLOW,
    PREFRONTAL_ERROR_INTERNAL
} prefrontal_error_t;

/*=============================================================================
 * GOAL REPRESENTATION
 *===========================================================================*/

/**
 * @brief Goal priority level
 */
typedef enum {
    GOAL_PRIORITY_LOW = 0,
    GOAL_PRIORITY_NORMAL = 1,
    GOAL_PRIORITY_HIGH = 2,
    GOAL_PRIORITY_CRITICAL = 3
} goal_priority_t;

/**
 * @brief Goal state
 */
typedef enum {
    GOAL_STATE_INACTIVE = 0,
    GOAL_STATE_ACTIVE,
    GOAL_STATE_SUSPENDED,
    GOAL_STATE_ACHIEVED,
    GOAL_STATE_FAILED
} goal_state_t;

/**
 * @brief Goal representation
 */
typedef struct {
    uint32_t goal_id;                   /**< Unique goal identifier */
    char description[64];               /**< Goal description */
    goal_priority_t priority;           /**< Goal priority */
    goal_state_t state;                 /**< Current goal state */
    float value;                        /**< Expected value/reward */
    float urgency;                      /**< Time-sensitive urgency [0, 1] */
    float progress;                     /**< Completion progress [0, 1] */
    uint32_t parent_goal_id;            /**< Parent goal (0 if top-level) */
    uint64_t deadline_ms;               /**< Deadline timestamp (0 if none) */
} prefrontal_goal_t;

/*=============================================================================
 * ACTION REPRESENTATION
 *===========================================================================*/

/**
 * @brief Action type classification
 */
typedef enum {
    ACTION_TYPE_MOTOR = 0,              /**< Motor action (movement) */
    ACTION_TYPE_COGNITIVE,              /**< Cognitive action (thinking) */
    ACTION_TYPE_COMMUNICATION,          /**< Communication action (speech) */
    ACTION_TYPE_INTERNAL                /**< Internal action (attention shift) */
} action_type_t;

/**
 * @brief Action representation
 */
typedef struct {
    uint32_t action_id;                 /**< Unique action identifier */
    action_type_t type;                 /**< Action type */
    char description[64];               /**< Action description */
    float expected_value;               /**< Expected value of action */
    float cost;                         /**< Effort/resource cost [0, 1] */
    float confidence;                   /**< Confidence in success [0, 1] */
    uint32_t goal_id;                   /**< Associated goal */
    float parameters[8];                /**< Action parameters */
    uint32_t param_count;               /**< Number of parameters */
} prefrontal_action_t;

/*=============================================================================
 * DECISION STRUCTURES
 *===========================================================================*/

/**
 * @brief Decision option
 */
typedef struct {
    prefrontal_action_t action;         /**< Action to take */
    float expected_utility;             /**< Expected utility (value - cost) */
    float probability;                  /**< Estimated success probability */
    float risk;                         /**< Associated risk [0, 1] */
    float desirability;                 /**< Overall desirability score */
} decision_option_t;

/**
 * @brief Decision result
 */
typedef struct {
    decision_option_t* selected_option; /**< Selected action */
    uint32_t options_evaluated;         /**< Number of options considered */
    float decision_confidence;          /**< Confidence in decision [0, 1] */
    float decision_time_ms;             /**< Time taken to decide */
    bool was_inhibited;                 /**< Whether initial impulse was inhibited */
    uint32_t conflict_level;            /**< Level of conflict (0-3) */
} decision_result_t;

/*=============================================================================
 * PLANNING STRUCTURES
 *===========================================================================*/

/**
 * @brief Action plan
 */
typedef struct {
    prefrontal_action_t* actions;       /**< Sequence of planned actions */
    uint32_t action_count;              /**< Number of actions in plan */
    uint32_t current_step;              /**< Current execution step */
    float expected_total_value;         /**< Expected value of completing plan */
    float total_cost;                   /**< Total resource cost */
    float plan_confidence;              /**< Confidence in plan success */
    uint32_t goal_id;                   /**< Goal this plan serves */
} action_plan_t;

/*=============================================================================
 * INHIBITION STRUCTURES
 *===========================================================================*/

/**
 * @brief Impulse record for inhibition
 */
typedef struct {
    prefrontal_action_t impulse_action; /**< Impulsive action to inhibit */
    float impulse_strength;             /**< Strength of impulse [0, 1] */
    float inhibition_strength;          /**< Applied inhibition [0, 1] */
    bool was_suppressed;                /**< Whether impulse was suppressed */
    char suppression_reason[64];        /**< Reason for suppression */
} impulse_record_t;

/*=============================================================================
 * STATISTICS
 *===========================================================================*/

/**
 * @brief Adapter statistics
 */
typedef struct {
    /* Goal tracking */
    uint64_t goals_activated;           /**< Total goals activated */
    uint64_t goals_achieved;            /**< Goals successfully achieved */
    uint64_t goals_failed;              /**< Goals that failed */
    uint64_t goals_suspended;           /**< Goals temporarily suspended */

    /* Decision tracking */
    uint64_t decisions_made;            /**< Total decisions */
    float avg_decision_time_ms;         /**< Average decision latency */
    float avg_decision_confidence;      /**< Average decision confidence */
    uint64_t decision_timeouts;         /**< Decisions that timed out */

    /* Planning tracking */
    uint64_t plans_generated;           /**< Total plans created */
    uint64_t plans_completed;           /**< Plans executed to completion */
    uint64_t plans_revised;             /**< Plans that required revision */
    float avg_plan_length;              /**< Average plan step count */

    /* Inhibition tracking */
    uint64_t impulses_detected;         /**< Total impulses detected */
    uint64_t impulses_suppressed;       /**< Impulses successfully suppressed */
    uint64_t inhibition_failures;       /**< Failed inhibitions */
    float avg_inhibition_strength;      /**< Average inhibition applied */

    /* Task switching */
    uint64_t task_switches;             /**< Total task switches */
    float avg_switch_cost_ms;           /**< Average switch cost in ms */

    /* Training */
    uint64_t training_iterations;       /**< Training updates */
    float value_learning_loss;          /**< Current value learning loss */
} prefrontal_stats_t;

/*=============================================================================
 * CALLBACK TYPES
 *===========================================================================*/

/**
 * @brief Callback for goal state changes
 */
typedef void (*prefrontal_goal_callback_t)(
    const prefrontal_goal_t* goal,
    goal_state_t old_state,
    goal_state_t new_state,
    void* user_data
);

/**
 * @brief Callback for decision events
 */
typedef void (*prefrontal_decision_callback_t)(
    const decision_result_t* result,
    void* user_data
);

/**
 * @brief Callback for inhibition events
 */
typedef void (*prefrontal_inhibition_callback_t)(
    const impulse_record_t* record,
    void* user_data
);

/**
 * @brief Callback for action events
 */
typedef void (*prefrontal_action_callback_t)(
    const prefrontal_action_t* action,
    bool started,  /* true = started, false = completed */
    float outcome, /* outcome value (only valid if !started) */
    void* user_data
);

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

/**
 * @brief Get default configuration
 *
 * WHAT: Returns default configuration for prefrontal cortex adapter
 * WHY:  Provide sensible defaults for common use cases
 * HOW:  Initialize all fields with biologically-motivated values
 *
 * @return Default configuration structure
 */
prefrontal_config_t prefrontal_default_config(void);

/**
 * @brief Create prefrontal cortex adapter
 *
 * WHAT: Allocate and initialize the adapter with all sub-modules
 * WHY:  Central point for executive function initialization
 * HOW:  Create planning, decision, inhibition modules; initialize state
 *
 * @param config Configuration (NULL for defaults)
 * @return New adapter instance, or NULL on failure
 */
prefrontal_adapter_t* prefrontal_create(const prefrontal_config_t* config);

/**
 * @brief Destroy prefrontal cortex adapter
 *
 * WHAT: Free all resources associated with the adapter
 * WHY:  Prevent memory leaks
 * HOW:  Destroy sub-modules, free buffers and goal structures
 *
 * @param adapter Adapter to destroy
 */
void prefrontal_destroy(prefrontal_adapter_t* adapter);

/**
 * @brief Reset adapter state
 *
 * WHAT: Clear buffers and reset to idle state
 * WHY:  Prepare for new task without full reinitialization
 * HOW:  Reset all sub-modules, clear goals and plans
 *
 * @param adapter Adapter instance
 * @return true on success, false on failure
 */
bool prefrontal_reset(prefrontal_adapter_t* adapter);

/*=============================================================================
 * GOAL MANAGEMENT
 *===========================================================================*/

/**
 * @brief Activate a new goal
 *
 * WHAT: Add a goal to active goal set
 * WHY:  Drive behavior toward objective achievement
 * HOW:  Add to goal hierarchy, update working memory
 *
 * @param adapter Adapter instance
 * @param goal Goal to activate
 * @return Goal ID on success, 0 on failure
 */
uint32_t prefrontal_activate_goal(prefrontal_adapter_t* adapter,
                                   const prefrontal_goal_t* goal);

/**
 * @brief Deactivate a goal
 *
 * WHAT: Remove goal from active set
 * WHY:  Goal achieved, failed, or no longer relevant
 * HOW:  Update state, release resources
 *
 * @param adapter Adapter instance
 * @param goal_id Goal to deactivate
 * @param new_state Final state (ACHIEVED, FAILED, INACTIVE)
 * @return true on success
 */
bool prefrontal_deactivate_goal(prefrontal_adapter_t* adapter,
                                 uint32_t goal_id,
                                 goal_state_t new_state);

/**
 * @brief Get active goals
 *
 * WHAT: Retrieve currently active goals
 * WHY:  Monitor goal state, plan coordination
 * HOW:  Copy active goals to output buffer
 *
 * @param adapter Adapter instance
 * @param goals Output buffer (must be pre-allocated)
 * @param count Input: buffer capacity; Output: actual count
 * @return true on success
 */
bool prefrontal_get_active_goals(const prefrontal_adapter_t* adapter,
                                  prefrontal_goal_t* goals,
                                  uint32_t* count);

/**
 * @brief Update goal progress
 *
 * WHAT: Update completion progress of a goal
 * WHY:  Track progress toward objectives
 * HOW:  Update progress field, trigger callbacks if needed
 *
 * @param adapter Adapter instance
 * @param goal_id Goal to update
 * @param progress New progress value [0, 1]
 * @return true on success
 */
bool prefrontal_update_goal_progress(prefrontal_adapter_t* adapter,
                                      uint32_t goal_id,
                                      float progress);

/*=============================================================================
 * PLANNING
 *===========================================================================*/

/**
 * @brief Generate action plan for goal
 *
 * WHAT: Create multi-step action plan
 * WHY:  Achieve complex goals through sequential actions
 * HOW:  Hierarchical task decomposition, forward search
 *
 * @param adapter Adapter instance
 * @param goal_id Goal to plan for
 * @param plan Output plan structure
 * @return true on success, false on planning failure
 */
bool prefrontal_generate_plan(prefrontal_adapter_t* adapter,
                               uint32_t goal_id,
                               action_plan_t* plan);

/**
 * @brief Get next action from plan
 *
 * WHAT: Retrieve next action to execute
 * WHY:  Sequential plan execution
 * HOW:  Return current step, advance pointer
 *
 * @param adapter Adapter instance
 * @param goal_id Goal whose plan to execute
 * @param action Output action
 * @return true if action available, false if plan complete
 */
bool prefrontal_get_next_action(prefrontal_adapter_t* adapter,
                                 uint32_t goal_id,
                                 prefrontal_action_t* action);

/**
 * @brief Report action outcome
 *
 * WHAT: Report result of executed action
 * WHY:  Update plan, learn from outcomes
 * HOW:  Update progress, possibly revise plan
 *
 * @param adapter Adapter instance
 * @param action_id Action that was executed
 * @param success Whether action succeeded
 * @param outcome Outcome value (reward/penalty)
 * @return true on success
 */
bool prefrontal_report_action_outcome(prefrontal_adapter_t* adapter,
                                       uint32_t action_id,
                                       bool success,
                                       float outcome);

/*=============================================================================
 * DECISION-MAKING
 *===========================================================================*/

/**
 * @brief Evaluate decision options
 *
 * WHAT: Evaluate multiple action options and select best
 * WHY:  Value-based decision-making
 * HOW:  Compute expected utility, apply risk preferences
 *
 * @param adapter Adapter instance
 * @param options Array of options to evaluate
 * @param num_options Number of options
 * @param result Output decision result
 * @return true on success, false on timeout or error
 */
bool prefrontal_evaluate_options(prefrontal_adapter_t* adapter,
                                  const decision_option_t* options,
                                  uint32_t num_options,
                                  decision_result_t* result);

/**
 * @brief Make quick decision (reflexive)
 *
 * WHAT: Fast, heuristic-based decision
 * WHY:  Time-critical situations requiring quick response
 * HOW:  Use cached value estimates, skip deep evaluation
 *
 * @param adapter Adapter instance
 * @param options Array of options
 * @param num_options Number of options
 * @param selected Output: index of selected option
 * @return true on success
 */
bool prefrontal_quick_decision(prefrontal_adapter_t* adapter,
                                const decision_option_t* options,
                                uint32_t num_options,
                                uint32_t* selected);

/**
 * @brief Get current decision state
 *
 * WHAT: Get information about ongoing decision process
 * WHY:  Monitor decision progress, detect conflicts
 * HOW:  Return internal decision state
 *
 * @param adapter Adapter instance
 * @param conflict_level Output: current conflict level (0-3)
 * @param dominant_option Output: currently leading option index
 * @return true if decision in progress, false if idle
 */
bool prefrontal_get_decision_state(const prefrontal_adapter_t* adapter,
                                    uint32_t* conflict_level,
                                    uint32_t* dominant_option);

/*=============================================================================
 * INHIBITORY CONTROL
 *===========================================================================*/

/**
 * @brief Check if action should be inhibited
 *
 * WHAT: Evaluate whether impulsive action should be suppressed
 * WHY:  Prevent inappropriate responses
 * HOW:  Check against goals, rules, and context
 *
 * @param adapter Adapter instance
 * @param action Action to evaluate
 * @param record Output inhibition record
 * @return true if action should be inhibited
 */
bool prefrontal_check_inhibition(prefrontal_adapter_t* adapter,
                                  const prefrontal_action_t* action,
                                  impulse_record_t* record);

/**
 * @brief Apply impulse suppression
 *
 * WHAT: Actively suppress an impulsive response
 * WHY:  Override prepotent responses
 * HOW:  Apply inhibitory control, record outcome
 *
 * @param adapter Adapter instance
 * @param action Action to suppress
 * @param reason Reason for suppression
 * @return true if suppression successful
 */
bool prefrontal_suppress_impulse(prefrontal_adapter_t* adapter,
                                  const prefrontal_action_t* action,
                                  const char* reason);

/**
 * @brief Get impulse control statistics
 *
 * WHAT: Get statistics on inhibitory control
 * WHY:  Monitor self-control capacity
 * HOW:  Return aggregated inhibition metrics
 *
 * @param adapter Adapter instance
 * @param success_rate Output: proportion of successful inhibitions
 * @param fatigue_level Output: current inhibitory fatigue [0, 1]
 * @return true on success
 */
bool prefrontal_get_inhibition_stats(const prefrontal_adapter_t* adapter,
                                      float* success_rate,
                                      float* fatigue_level);

/*=============================================================================
 * WORKING MEMORY INTEGRATION
 *===========================================================================*/

/**
 * @brief Push item to working memory
 *
 * WHAT: Add item to prefrontal working memory buffer
 * WHY:  Maintain goal-relevant information
 * HOW:  Add to WM with goal association
 *
 * @param adapter Adapter instance
 * @param item_id Item identifier
 * @param priority Priority [0, 1]
 * @param goal_id Associated goal (0 for none)
 * @return true on success
 */
bool prefrontal_wm_push(prefrontal_adapter_t* adapter,
                         uint32_t item_id,
                         float priority,
                         uint32_t goal_id);

/**
 * @brief Update working memory item
 *
 * WHAT: Refresh or update WM item
 * WHY:  Prevent decay, update information
 * HOW:  Reset decay timer, update content
 *
 * @param adapter Adapter instance
 * @param item_id Item to update
 * @param new_priority New priority (or -1 to keep current)
 * @return true on success
 */
bool prefrontal_wm_update(prefrontal_adapter_t* adapter,
                           uint32_t item_id,
                           float new_priority);

/**
 * @brief Get working memory contents
 *
 * WHAT: Retrieve current WM items
 * WHY:  Inspect active representations
 * HOW:  Copy buffer contents
 *
 * @param adapter Adapter instance
 * @param item_ids Output buffer for IDs
 * @param priorities Output buffer for priorities (can be NULL)
 * @param count Input: buffer capacity; Output: actual count
 * @return true on success
 */
bool prefrontal_wm_get_contents(const prefrontal_adapter_t* adapter,
                                 uint32_t* item_ids,
                                 float* priorities,
                                 uint32_t* count);

/*=============================================================================
 * COGNITIVE FLEXIBILITY
 *===========================================================================*/

/**
 * @brief Switch to new task/rule set
 *
 * WHAT: Switch cognitive set to new task demands
 * WHY:  Adapt to changing requirements
 * HOW:  Update active rules, clear irrelevant WM, pay switch cost
 *
 * @param adapter Adapter instance
 * @param new_task_id New task identifier
 * @param switch_cost_out Output: actual switch cost incurred
 * @return true on success
 */
bool prefrontal_task_switch(prefrontal_adapter_t* adapter,
                             uint32_t new_task_id,
                             float* switch_cost_out);

/**
 * @brief Learn new rule from experience
 *
 * WHAT: Extract rule from observed contingencies
 * WHY:  Acquire new task knowledge
 * HOW:  Identify regularities, form rule representation
 *
 * @param adapter Adapter instance
 * @param context Context features (input)
 * @param context_size Size of context vector
 * @param action Action taken
 * @param outcome Outcome received
 * @return true if new rule learned
 */
bool prefrontal_learn_rule(prefrontal_adapter_t* adapter,
                            const float* context,
                            uint32_t context_size,
                            uint32_t action,
                            float outcome);

/*=============================================================================
 * CALLBACK REGISTRATION
 *===========================================================================*/

/**
 * @brief Set goal state change callback
 */
bool prefrontal_set_goal_callback(prefrontal_adapter_t* adapter,
                                   prefrontal_goal_callback_t callback,
                                   void* user_data);

/**
 * @brief Set decision callback
 */
bool prefrontal_set_decision_callback(prefrontal_adapter_t* adapter,
                                       prefrontal_decision_callback_t callback,
                                       void* user_data);

/**
 * @brief Set inhibition callback
 */
bool prefrontal_set_inhibition_callback(prefrontal_adapter_t* adapter,
                                         prefrontal_inhibition_callback_t callback,
                                         void* user_data);

/**
 * @brief Set action callback
 */
bool prefrontal_set_action_callback(prefrontal_adapter_t* adapter,
                                     prefrontal_action_callback_t callback,
                                     void* user_data);

/*=============================================================================
 * STATUS AND DIAGNOSTICS
 *===========================================================================*/

/**
 * @brief Get current processing status
 */
prefrontal_status_t prefrontal_get_status(const prefrontal_adapter_t* adapter);

/**
 * @brief Get last error code
 */
prefrontal_error_t prefrontal_get_last_error(const prefrontal_adapter_t* adapter);

/**
 * @brief Get error description string
 */
const char* prefrontal_error_string(prefrontal_error_t error);

/**
 * @brief Get status description string
 */
const char* prefrontal_status_string(prefrontal_status_t status);

/**
 * @brief Get adapter statistics
 */
bool prefrontal_get_stats(const prefrontal_adapter_t* adapter,
                           prefrontal_stats_t* stats);

/**
 * @brief Get adapter configuration
 */
bool prefrontal_get_config(const prefrontal_adapter_t* adapter,
                            prefrontal_config_t* config);

/*=============================================================================
 * BIO-ASYNC COMMUNICATION
 *===========================================================================*/

/**
 * @brief Get bio-async module context
 *
 * @param adapter Adapter instance
 * @return Bio-async module context, or NULL if not enabled
 */
bio_module_context_t prefrontal_get_bio_context(prefrontal_adapter_t* adapter);

/**
 * @brief Process pending bio-async messages
 *
 * @param adapter Adapter instance
 * @param max_messages Maximum messages to process (0 = all)
 * @return Number of messages processed
 */
uint32_t prefrontal_process_bio_messages(prefrontal_adapter_t* adapter,
                                          uint32_t max_messages);

/**
 * @brief Request goal evaluation asynchronously
 *
 * @param adapter Adapter instance
 * @param goal Goal to evaluate
 * @return Future for evaluation result, or NULL on failure
 */
nimcp_bio_future_t prefrontal_request_goal_eval_async(
    prefrontal_adapter_t* adapter,
    const prefrontal_goal_t* goal
);

/**
 * @brief Broadcast decision made
 *
 * @param adapter Adapter instance
 * @param result Decision result to broadcast
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t prefrontal_broadcast_decision(
    prefrontal_adapter_t* adapter,
    const decision_result_t* result
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PREFRONTAL_ADAPTER_H */
