/**
 * @file nimcp_recovery_executive.h
 * @brief Executive Function for Recovery Planning - Goal-Oriented Multi-Step Recovery
 *
 * WHAT: High-level decision making and strategy orchestration for fault recovery
 * WHY:  Complex recoveries require multi-step planning, goal management, and adaptive replanning
 * HOW:  Implements executive function with goal decomposition, plan creation/execution, metacognitive monitoring
 *
 * COGNITIVE ARCHITECTURE:
 * ┌──────────────────────────────────────────────────────────────────┐
 * │                    EXECUTIVE FUNCTION                             │
 * │                                                                   │
 * │  ┌────────────────────────────────────────────────────────────┐ │
 * │  │  GOAL MANAGEMENT                                           │ │
 * │  │  - Current goal (restore functionality, performance, etc.) │ │
 * │  │  - Subgoal hierarchy (decomposed multi-level goals)        │ │
 * │  │  - Goal priorities and dependencies                        │ │
 * │  └────────────────────────────────────────────────────────────┘ │
 * │                           ↓                                       │
 * │  ┌────────────────────────────────────────────────────────────┐ │
 * │  │  PLANNING                                                  │ │
 * │  │  - Create multi-step recovery plans                        │ │
 * │  │  - Estimate time/cost per step                             │ │
 * │  │  - Assign confidence scores                                │ │
 * │  └────────────────────────────────────────────────────────────┘ │
 * │                           ↓                                       │
 * │  ┌────────────────────────────────────────────────────────────┐ │
 * │  │  DECISION MAKING                                           │ │
 * │  │  - Risk tolerance evaluation                               │ │
 * │  │  - Time constraint checking                                │ │
 * │  │  - Resource availability                                   │ │
 * │  └────────────────────────────────────────────────────────────┘ │
 * │                           ↓                                       │
 * │  ┌────────────────────────────────────────────────────────────┐ │
 * │  │  METACOGNITIVE MONITORING                                  │ │
 * │  │  - "Is this plan working?" self-assessment                 │ │
 * │  │  - Confidence tracking during execution                    │ │
 * │  │  - Adaptive replanning triggers                            │ │
 * │  └────────────────────────────────────────────────────────────┘ │
 * └──────────────────────────────────────────────────────────────────┘
 *
 * BIOLOGICAL BASIS:
 * - Prefrontal cortex executive control
 * - Dorsolateral PFC: Planning and working memory
 * - Ventromedial PFC: Risk/reward evaluation
 * - Anterior cingulate: Monitoring and error detection
 *
 * INTEGRATION POINTS:
 * - Brain reasoning: Strategy evaluation
 * - Working memory: Past failure recall
 * - Episodic memory: Learning from outcomes
 * - Diagnostic system: Failure analysis input
 *
 * @author NIMCP Team
 * @date 2025-11-20
 * @version 1.0.0
 */

#ifndef NIMCP_RECOVERY_EXECUTIVE_H
#define NIMCP_RECOVERY_EXECUTIVE_H

#include <stdint.h>
#include <stdbool.h>
#include "utils/fault_tolerance/nimcp_diagnostics.h"
#include "utils/fault_tolerance/nimcp_recovery.h"
#include "utils/fault_tolerance/nimcp_brain_recovery_integration.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations
//=============================================================================

typedef struct brain_struct* brain_t;
typedef struct brain_recovery_internal* brain_recovery_t;
// brain_recovery_decision_t is defined in nimcp_brain_recovery_integration.h

//=============================================================================
// Constants
//=============================================================================

#define MAX_SUBGOALS 10                    /**< Maximum subgoal decomposition depth */
#define MAX_PLAN_STEPS 20                  /**< Maximum steps in a single plan */
#define RECOVERY_EXECUTIVE_VERSION "1.0.0" /**< Module version */

//=============================================================================
// Recovery Goals
//=============================================================================

/**
 * @brief Recovery goal types for executive function
 *
 * WHAT: High-level objectives for recovery operations
 * WHY:  Different failures require different goal hierarchies
 * HOW:  Executive decomposes complex goals into achievable subgoals
 */
typedef enum {
    RECOVERY_GOAL_QUICK_FIX = 0,           /**< Fast, tactical fix */
    RECOVERY_GOAL_FULL_RECOVERY,           /**< Complete restoration */
    RECOVERY_GOAL_DEGRADED_MODE,           /**< Continue with reduced capability */
    RECOVERY_GOAL_PREVENT_RECURRENCE,      /**< Fix root cause */
    RECOVERY_GOAL_DATA_PRESERVATION        /**< Save state before crash */
} recovery_goal_t;

//=============================================================================
// Recovery Executive Actions
//=============================================================================

/**
 * @brief Recovery executive action types
 *
 * WHAT: Specific actions the executive can orchestrate
 * WHY:  Multi-step plans require sequenced actions
 * HOW:  Each action maps to a recovery operation
 */
typedef enum {
    RECOVERY_EXEC_ACTION_NONE = 0,
    RECOVERY_EXEC_ACTION_CHECKPOINT_SAVE,
    RECOVERY_EXEC_ACTION_CHECKPOINT_RESTORE,
    RECOVERY_EXEC_ACTION_ANALYZE_DIAGNOSTIC,
    RECOVERY_EXEC_ACTION_RESET_SUBSYSTEM,
    RECOVERY_EXEC_ACTION_REDUCE_PRECISION,
    RECOVERY_EXEC_ACTION_REDUCE_LEARNING_RATE,
    RECOVERY_EXEC_ACTION_REDUCE_BATCH_SIZE,
    RECOVERY_EXEC_ACTION_CLEAR_CACHE,
    RECOVERY_EXEC_ACTION_ISOLATE_COMPONENT,
    RECOVERY_EXEC_ACTION_VERIFY_STATE,
    RECOVERY_EXEC_ACTION_RETRY_OPERATION,
    RECOVERY_EXEC_ACTION_FALLBACK_MODE,
    RECOVERY_EXEC_ACTION_GRACEFUL_SHUTDOWN,
    RECOVERY_EXEC_ACTION_CUSTOM
} recovery_exec_action_t;

/**
 * @brief Recovery goal redefinition for executive function
 *
 * WHAT: Map recovery_goal_t to executive-specific goals
 * WHY:  Executive uses different goal hierarchy
 * HOW:  Redefine with GOAL_ prefix for clarity
 */
typedef enum {
    GOAL_NONE = 0,
    GOAL_RESTORE_FUNCTIONALITY,
    GOAL_RESTORE_PERFORMANCE,
    GOAL_PREVENT_DATA_LOSS,
    GOAL_LEARN_FROM_FAILURE,
    GOAL_PREVENT_RECURRENCE
} recovery_goal_internal_t;

// Map recovery_goal_t to internal goals
#define GOAL_RESTORE_FUNCTIONALITY  RECOVERY_GOAL_FULL_RECOVERY
#define GOAL_RESTORE_PERFORMANCE    RECOVERY_GOAL_FULL_RECOVERY
#define GOAL_PREVENT_DATA_LOSS      RECOVERY_GOAL_DATA_PRESERVATION
#define GOAL_LEARN_FROM_FAILURE     RECOVERY_GOAL_PREVENT_RECURRENCE
#define GOAL_PREVENT_RECURRENCE     RECOVERY_GOAL_PREVENT_RECURRENCE
#define GOAL_NONE                   RECOVERY_GOAL_QUICK_FIX

//=============================================================================
// Data Structures
//=============================================================================

/**
 * @brief Recovery plan step - single action in recovery plan
 *
 * WHAT: One step in multi-step recovery sequence
 * WHY:  Plans decompose complex recovery into steps
 * HOW:  Action + metadata (timeout, confidence, description)
 *
 * NOTE: Uses recovery_plan_step_t to avoid conflict with executive's plan_step_t
 */
typedef struct {
    recovery_exec_action_t action;   /**< Action to execute */
    uint32_t timeout_ms;              /**< Maximum execution time */
    char description[128];            /**< Human-readable description */
    float expected_success_rate;     /**< Expected probability of success */
    bool is_critical;                /**< If true, plan fails if step fails */
} recovery_plan_step_t;

/**
 * @brief Decision criteria for plan creation
 *
 * WHAT: Constraints and preferences for recovery planning
 * WHY:  Different contexts require different recovery strategies
 * HOW:  Executive uses criteria to select actions
 */
typedef struct {
    float risk_tolerance;            /**< Risk tolerance [0.0 = conservative, 1.0 = aggressive] */
    uint32_t max_recovery_time_ms;   /**< Maximum allowed recovery time */
    uint32_t max_steps;              /**< Maximum steps in plan */
    bool allow_data_loss;            /**< Whether data loss is acceptable */
    bool allow_performance_degradation; /**< Whether performance degradation is acceptable */
    bool require_verification;       /**< Whether to verify after recovery */
    float min_confidence_threshold;  /**< Minimum confidence to execute plan */
} decision_criteria_t;

/**
 * @brief Plan monitoring state - metacognitive tracking
 *
 * WHAT: Current assessment of plan effectiveness
 * WHY:  Track whether plan is working ("Am I succeeding?")
 * HOW:  Update confidence and success/failure counts during execution
 */
typedef struct {
    float confidence_in_plan;        /**< Current confidence plan will succeed */
    bool plan_working;               /**< Is plan succeeding? */
    uint32_t steps_succeeded;        /**< Count of successful steps */
    uint32_t steps_failed;           /**< Count of failed steps */
    float progress_rate;             /**< Progress rate [0.0, 1.0] */
} plan_monitoring_state_t;

/**
 * @brief Recovery plan - multi-step action sequence
 *
 * WHAT: Complete recovery strategy with steps and metadata
 * WHY:  Complex recoveries need orchestrated sequences
 * HOW:  Sequence of steps + goal + confidence + rationale
 */
typedef struct {
    uint64_t plan_id;                /**< Unique plan identifier */
    recovery_goal_t goal;            /**< Recovery goal this plan achieves */
    recovery_plan_step_t steps[MAX_PLAN_STEPS]; /**< Action sequence */
    uint32_t step_count;             /**< Number of steps */
    float confidence;                /**< Estimated success probability */
    uint32_t estimated_time_ms;      /**< Estimated execution time */
    char rationale[256];             /**< Why this plan was chosen */
    uint64_t creation_time_us;       /**< When plan was created */
} recovery_plan_t;

/**
 * @brief Recovery executive configuration
 *
 * WHAT: Parameters controlling executive behavior
 * WHY:  Allow tuning for different scenarios
 * HOW:  Passed to executive_create()
 */
typedef struct {
    uint32_t max_subgoals;           /**< Maximum subgoal depth */
    uint32_t max_plan_steps;         /**< Maximum steps per plan */
    bool enable_metacognitive_monitoring; /**< Enable plan monitoring */
    float replanning_confidence_threshold; /**< When to trigger replanning */
    decision_criteria_t default_criteria;  /**< Default decision criteria */
} recovery_executive_config_t;

/**
 * @brief Recovery executive statistics
 *
 * WHAT: Performance and usage metrics
 * WHY:  Monitor executive effectiveness
 * HOW:  Updated during operation
 */
typedef struct {
    uint64_t total_plans_created;    /**< Total plans generated */
    uint64_t total_plans_executed;   /**< Total plans executed */
    uint64_t successful_plans;       /**< Plans that succeeded */
    uint64_t failed_plans;           /**< Plans that failed */
    uint64_t replanning_events;      /**< Times replanning was triggered */
    float average_plan_confidence;   /**< Average plan confidence */
    uint64_t total_execution_time_us; /**< Total time spent executing */
} recovery_executive_stats_t;

/**
 * @brief Plan execution result
 *
 * WHAT: Outcome of executing a recovery plan
 * WHY:  Report success/failure and diagnostic info
 * HOW:  Contains success flag, timing, and failure details
 */
typedef struct {
    bool success;                    /**< Did plan succeed? */
    uint32_t steps_completed;        /**< Number of steps completed */
    int32_t failed_step;             /**< Index of failed step (-1 if none) */
    char failure_reason[256];        /**< Why plan failed */
    uint32_t total_time_us;          /**< Total execution time */
} recovery_execution_result_t;

//=============================================================================
// Opaque Types
//=============================================================================

/**
 * @brief Recovery executive opaque handle
 *
 * WHAT: Opaque handle to executive function state
 * WHY:  Encapsulation - hide implementation details
 * HOW:  Typedef of internal structure
 */
typedef struct recovery_executive_internal recovery_executive_t;

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Get default executive configuration
 *
 * WHAT: Returns sensible defaults for executive config
 * WHY:  Easy initialization for common cases
 * HOW:  Static configuration with typical values
 *
 * @return Default configuration
 */
recovery_executive_config_t recovery_executive_default_config(void);

/**
 * @brief Create recovery executive
 *
 * WHAT: Initialize executive function system
 * WHY:  Required before any planning operations
 * HOW:  Allocates structure, initializes state
 *
 * @param config Configuration parameters (NULL = use defaults)
 * @return Executive handle or NULL on failure
 */
recovery_executive_t* recovery_executive_create(
    const recovery_executive_config_t* config
);

/**
 * @brief Destroy recovery executive
 *
 * WHAT: Free all executive resources
 * WHY:  Prevent memory leaks
 * HOW:  Frees plans and structure
 *
 * @param exec Executive to destroy (NULL-safe)
 */
void recovery_executive_destroy(recovery_executive_t* exec);

/**
 * @brief Check if executive is ready
 *
 * WHAT: Verify executive is initialized and operational
 * WHY:  Safety check before use
 *
 * @param exec Executive handle
 * @return true if ready, false otherwise
 */
bool recovery_executive_is_ready(const recovery_executive_t* exec);

//=============================================================================
// Goal Decomposition Functions
//=============================================================================

/**
 * @brief Decompose high-level goal into subgoals
 *
 * WHAT: Break complex goal into achievable subgoals
 * WHY:  Multi-step planning requires goal hierarchy
 * HOW:  Apply decomposition rules per goal type
 *
 * @param goal High-level recovery goal
 * @param subgoals Output array for subgoals
 * @param subgoal_count Output count of subgoals
 * @return true on success, false on error
 */
bool recovery_executive_decompose_goal(
    recovery_goal_t goal,
    recovery_goal_t* subgoals,
    uint32_t* subgoal_count
);

//=============================================================================
// Plan Creation Functions
//=============================================================================

/**
 * @brief Create recovery plan for diagnosis and goal
 *
 * WHAT: Generate multi-step plan to achieve recovery goal
 * WHY:  Complex recoveries need orchestrated strategies
 * HOW:  Analyzes diagnosis → Selects actions → Estimates confidence
 *
 * @param exec Executive handle
 * @param diagnosis Diagnostic result describing failure
 * @param goal Recovery goal to achieve
 * @return Plan handle or NULL on failure
 */
recovery_plan_t* recovery_executive_create_plan(
    recovery_executive_t* exec,
    const diagnostic_result_t* diagnosis,
    recovery_goal_t goal
);

/**
 * @brief Create plan with brain reasoning input
 *
 * WHAT: Generate plan augmented with brain strategic reasoning
 * WHY:  Brain can provide higher-level strategy guidance
 * HOW:  Combines diagnosis + brain decision → Enhanced plan
 *
 * @param exec Executive handle
 * @param diagnosis Diagnostic result
 * @param goal Recovery goal
 * @param brain_decision Brain's strategic recommendation (can be NULL)
 * @return Plan handle or NULL on failure
 */
recovery_plan_t* recovery_executive_create_plan_with_brain_input(
    recovery_executive_t* exec,
    const diagnostic_result_t* diagnosis,
    recovery_goal_t goal,
    const brain_recovery_decision_t* brain_decision
);

/**
 * @brief Free recovery plan
 *
 * WHAT: Release plan memory
 * WHY:  Prevent memory leaks
 *
 * @param plan Plan to free (NULL-safe)
 */
void recovery_executive_free_plan(recovery_plan_t* plan);

//=============================================================================
// Plan Execution Functions
//=============================================================================

/**
 * @brief Execute recovery plan
 *
 * WHAT: Execute all steps in plan sequentially
 * WHY:  Actuate the recovery strategy
 * HOW:  Iterate steps → Execute → Monitor → Report
 *
 * @param exec Executive handle
 * @param plan Plan to execute
 * @return Execution result with success/failure info
 */
recovery_execution_result_t recovery_executive_execute_plan(
    recovery_executive_t* exec,
    const recovery_plan_t* plan
);

//=============================================================================
// Plan Monitoring Functions
//=============================================================================

/**
 * @brief Check if current plan is working
 *
 * WHAT: Metacognitive assessment of plan effectiveness
 * WHY:  Know when to replan ("Is this working?")
 * HOW:  Analyzes step success pattern and confidence
 *
 * @param exec Executive handle
 * @return true if plan appears to be working
 */
bool recovery_executive_is_plan_working(const recovery_executive_t* exec);

/**
 * @brief Get current monitoring state
 *
 * WHAT: Retrieve metacognitive monitoring metrics
 * WHY:  External components can use monitoring info
 *
 * @param exec Executive handle
 * @param state Output monitoring state
 * @return true on success, false if NULL parameters
 */
bool recovery_executive_get_monitoring_state(
    const recovery_executive_t* exec,
    plan_monitoring_state_t* state
);

//=============================================================================
// Replanning Functions
//=============================================================================

/**
 * @brief Create new plan (replan)
 *
 * WHAT: Generate alternative plan when current fails
 * WHY:  Adaptive recovery - try different strategy
 * HOW:  Uses stored diagnosis + current goal → New plan
 *
 * @param exec Executive handle
 * @param reason Why replanning (for logging)
 * @return New plan or NULL on failure
 */
recovery_plan_t* recovery_executive_replan(
    recovery_executive_t* exec,
    const char* reason
);

/**
 * @brief Replan with new goal
 *
 * WHAT: Change goal and create new plan
 * WHY:  Sometimes need to lower ambitions
 * HOW:  Updates goal → Creates new plan
 *
 * @param exec Executive handle
 * @param new_goal New recovery goal
 * @param reason Why changing goal
 * @return New plan or NULL on failure
 */
recovery_plan_t* recovery_executive_replan_with_goal(
    recovery_executive_t* exec,
    recovery_goal_t new_goal,
    const char* reason
);

/**
 * @brief Replan with brain input
 *
 * WHAT: Generate new plan with brain reasoning
 * WHY:  Brain may suggest alternative strategy
 *
 * @param exec Executive handle
 * @param brain_decision Brain's recommendation
 * @param reason Why replanning
 * @return New plan or NULL on failure
 */
recovery_plan_t* recovery_executive_replan_with_brain_input(
    recovery_executive_t* exec,
    const brain_recovery_decision_t* brain_decision,
    const char* reason
);

//=============================================================================
// Decision Criteria Functions
//=============================================================================

/**
 * @brief Set decision criteria
 *
 * WHAT: Update constraints for plan creation
 * WHY:  Adapt to changing context (time pressure, etc.)
 *
 * @param exec Executive handle
 * @param criteria New criteria
 * @return true on success, false if NULL
 */
bool recovery_executive_set_decision_criteria(
    recovery_executive_t* exec,
    const decision_criteria_t* criteria
);

/**
 * @brief Get current decision criteria
 *
 * @param exec Executive handle
 * @param criteria Output criteria
 * @return true on success, false if NULL
 */
bool recovery_executive_get_decision_criteria(
    const recovery_executive_t* exec,
    decision_criteria_t* criteria
);

//=============================================================================
// Configuration Functions
//=============================================================================

/**
 * @brief Get current configuration
 *
 * @param exec Executive handle
 * @param config Output configuration
 * @return true on success, false if NULL
 */
bool recovery_executive_get_config(
    const recovery_executive_t* exec,
    recovery_executive_config_t* config
);

/**
 * @brief Update configuration
 *
 * @param exec Executive handle
 * @param config New configuration
 * @return true on success, false if NULL
 */
bool recovery_executive_update_config(
    recovery_executive_t* exec,
    const recovery_executive_config_t* config
);

//=============================================================================
// Statistics Functions
//=============================================================================

/**
 * @brief Get executive statistics
 *
 * @param exec Executive handle
 * @param stats Output statistics
 * @return true on success, false if NULL
 */
bool recovery_executive_get_stats(
    const recovery_executive_t* exec,
    recovery_executive_stats_t* stats
);

/**
 * @brief Reset statistics counters
 *
 * @param exec Executive handle
 */
void recovery_executive_reset_stats(recovery_executive_t* exec);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get human-readable goal name
 *
 * @param goal Goal enumeration value
 * @return Goal name string
 */
const char* recovery_executive_get_goal_name(recovery_goal_t goal);

/**
 * @brief Get human-readable action name
 *
 * @param action Action enumeration value
 * @return Action name string
 */
const char* recovery_executive_get_action_name(recovery_exec_action_t action);

#ifdef __cplusplus
}
#endif

#endif  // NIMCP_RECOVERY_EXECUTIVE_H
