/**
 * @file nimcp_recovery_executive.c
 * @brief Executive Function for Recovery Planning - Implementation
 *
 * WHAT: Goal-oriented multi-step recovery planning with metacognitive monitoring
 * WHY:  Complex recoveries require strategic planning, adaptive replanning, self-assessment
 * HOW:  Implements executive control with goal decomposition, plan creation/execution, monitoring
 *
 * IMPLEMENTATION NOTES:
 * - Uses goal hierarchy for decomposition
 * - Plans are action sequences with timing and confidence
 * - Metacognitive monitoring tracks plan effectiveness
 * - Adaptive replanning when confidence drops
 *
 * @author NIMCP Team
 * @date 2025-11-20
 * @version 1.0.0
 */

#include "cognitive/fault_tolerance/nimcp_recovery_executive.h"
#include "utils/fault_tolerance/nimcp_brain_recovery_integration.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"

#include <string.h>
#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include <math.h>

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Internal executive function state
 *
 * WHAT: Complete executive state including goals, plans, monitoring
 * WHY:  Track all information needed for planning and replanning
 * HOW:  Opaque structure implementation
 */
struct recovery_executive_internal {
    // Configuration
    recovery_executive_config_t config;

    // Current state
    recovery_goal_t current_goal;
    recovery_goal_t subgoals[MAX_SUBGOALS];
    uint32_t subgoal_count;

    // Active plan
    recovery_plan_t* active_plan;
    uint32_t current_step;

    // Decision making
    decision_criteria_t criteria;

    // Metacognitive monitoring
    plan_monitoring_state_t monitoring;

    // Context for replanning
    diagnostic_result_t last_diagnosis;
    bool has_diagnosis;

    // Statistics
    recovery_executive_stats_t stats;

    // Plan ID counter
    uint64_t next_plan_id;

    // Initialized flag
    bool initialized;
};

//=============================================================================
// Helper Functions - Time
//=============================================================================

/**
 * @brief Get current time in microseconds
 *
 * WHAT: High-resolution timestamp
 * WHY:  Track execution time accurately
 * HOW:  Uses gettimeofday
 *
 * @return Current time in microseconds
 */
static uint64_t get_time_us(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000000ULL + (uint64_t)tv.tv_usec;
}

//=============================================================================
// Helper Functions - Goal Decomposition
//=============================================================================

/**
 * @brief Decompose RESTORE_FUNCTIONALITY goal
 *
 * WHAT: Break down functionality restoration into subgoals
 * WHY:  Multi-phase recovery process
 * HOW:  Ordered sequence: prevent loss → restore → verify → learn
 */
static void decompose_restore_functionality(
    recovery_goal_t* subgoals,
    uint32_t* count
) {
    subgoals[0] = GOAL_PREVENT_DATA_LOSS;    // First: save state
    subgoals[1] = GOAL_RESTORE_FUNCTIONALITY; // Then: get running
    *count = 2;
}

/**
 * @brief Decompose RESTORE_PERFORMANCE goal
 *
 * WHAT: Break down performance restoration
 * WHY:  Performance requires functionality first
 * HOW:  Functionality → Performance → Verify
 */
static void decompose_restore_performance(
    recovery_goal_t* subgoals,
    uint32_t* count
) {
    subgoals[0] = GOAL_RESTORE_FUNCTIONALITY; // Must work first
    subgoals[1] = GOAL_RESTORE_PERFORMANCE;   // Then optimize
    *count = 2;
}

/**
 * @brief Decompose PREVENT_RECURRENCE goal
 *
 * WHAT: Break down recurrence prevention
 * WHY:  Requires learning and fixing root cause
 * HOW:  Restore → Learn → Fix
 */
static void decompose_prevent_recurrence(
    recovery_goal_t* subgoals,
    uint32_t* count
) {
    subgoals[0] = GOAL_RESTORE_FUNCTIONALITY; // Get working
    subgoals[1] = GOAL_LEARN_FROM_FAILURE;    // Understand why
    subgoals[2] = GOAL_PREVENT_RECURRENCE;    // Fix root cause
    *count = 3;
}

//=============================================================================
// Helper Functions - Action Selection
//=============================================================================

/**
 * @brief Select actions for NaN/Inf errors
 *
 * WHAT: Choose recovery actions for numerical instability
 * WHY:  NaN/Inf require specific numerical recovery strategies
 * HOW:  Checkpoint → Reset → Reduce precision → Verify
 */
static uint32_t select_actions_for_nan(
    plan_step_t* steps,
    uint32_t max_steps,
    const decision_criteria_t* criteria
) {
    uint32_t count = 0;

    // Step 1: Save state (prevent data loss)
    if (count < max_steps) {
        steps[count].action = RECOVERY_EXEC_ACTION_CHECKPOINT_SAVE;
        steps[count].timeout_ms = 100;
        snprintf(steps[count].description, sizeof(steps[count].description),
                "Save current state before recovery");
        steps[count].expected_success_rate = 0.95f;
        steps[count].is_critical = true;
        count++;
    }

    // Step 2: Reset subsystem (clear NaN state)
    if (count < max_steps) {
        steps[count].action = RECOVERY_EXEC_ACTION_RESET_SUBSYSTEM;
        steps[count].timeout_ms = 50;
        snprintf(steps[count].description, sizeof(steps[count].description),
                "Reset numerical subsystem to clear NaN values");
        steps[count].expected_success_rate = 0.85f;
        steps[count].is_critical = true;
        count++;
    }

    // Step 3: Reduce precision (prevent recurrence)
    if (count < max_steps && criteria->allow_performance_degradation) {
        steps[count].action = RECOVERY_EXEC_ACTION_REDUCE_PRECISION;
        steps[count].timeout_ms = 10;
        snprintf(steps[count].description, sizeof(steps[count].description),
                "Switch to higher precision to prevent NaN");
        steps[count].expected_success_rate = 0.90f;
        steps[count].is_critical = false;
        count++;
    }

    // Step 4: Verify (ensure recovery successful)
    if (count < max_steps && criteria->require_verification) {
        steps[count].action = RECOVERY_EXEC_ACTION_VERIFY_STATE;
        steps[count].timeout_ms = 50;
        snprintf(steps[count].description, sizeof(steps[count].description),
                "Verify system state is numerically stable");
        steps[count].expected_success_rate = 0.95f;
        steps[count].is_critical = false;
        count++;
    }

    return count;
}

/**
 * @brief Select actions for memory errors
 *
 * WHAT: Choose recovery actions for memory-related failures
 * WHY:  Memory errors require different strategy than numerical errors
 * HOW:  Isolate → Clear cache → Verify → Checkpoint
 */
static uint32_t select_actions_for_memory_error(
    plan_step_t* steps,
    uint32_t max_steps,
    const decision_criteria_t* criteria
) {
    uint32_t count = 0;

    // Step 1: Isolate faulty component
    if (count < max_steps) {
        steps[count].action = RECOVERY_EXEC_ACTION_ISOLATE_COMPONENT;
        steps[count].timeout_ms = 20;
        snprintf(steps[count].description, sizeof(steps[count].description),
                "Isolate potentially corrupted memory region");
        steps[count].expected_success_rate = 0.80f;
        steps[count].is_critical = true;
        count++;
    }

    // Step 2: Clear caches
    if (count < max_steps) {
        steps[count].action = RECOVERY_EXEC_ACTION_CLEAR_CACHE;
        steps[count].timeout_ms = 30;
        snprintf(steps[count].description, sizeof(steps[count].description),
                "Clear memory caches to remove corrupted data");
        steps[count].expected_success_rate = 0.90f;
        steps[count].is_critical = false;
        count++;
    }

    // Step 3: Restore from checkpoint if available
    if (count < max_steps && !criteria->allow_data_loss) {
        steps[count].action = RECOVERY_EXEC_ACTION_CHECKPOINT_RESTORE;
        steps[count].timeout_ms = 200;
        snprintf(steps[count].description, sizeof(steps[count].description),
                "Restore from last known good checkpoint");
        steps[count].expected_success_rate = 0.75f;
        steps[count].is_critical = true;
        count++;
    }

    // Step 4: Verify
    if (count < max_steps && criteria->require_verification) {
        steps[count].action = RECOVERY_EXEC_ACTION_VERIFY_STATE;
        steps[count].timeout_ms = 50;
        snprintf(steps[count].description, sizeof(steps[count].description),
                "Verify memory integrity");
        steps[count].expected_success_rate = 0.95f;
        steps[count].is_critical = false;
        count++;
    }

    return count;
}

/**
 * @brief Select actions for gradient errors
 *
 * WHAT: Choose recovery actions for gradient explosion/vanishing
 * WHY:  Gradient errors need learning rate adjustment
 * HOW:  Checkpoint → Reduce learning rate → Reduce batch size → Verify
 */
static uint32_t select_actions_for_gradient_error(
    plan_step_t* steps,
    uint32_t max_steps,
    const decision_criteria_t* criteria
) {
    uint32_t count = 0;

    // Step 1: Checkpoint (before parameter changes)
    if (count < max_steps) {
        steps[count].action = RECOVERY_EXEC_ACTION_CHECKPOINT_SAVE;
        steps[count].timeout_ms = 100;
        snprintf(steps[count].description, sizeof(steps[count].description),
                "Save state before parameter adjustments");
        steps[count].expected_success_rate = 0.95f;
        steps[count].is_critical = true;
        count++;
    }

    // Step 2: Reduce learning rate
    if (count < max_steps) {
        steps[count].action = RECOVERY_EXEC_ACTION_REDUCE_LEARNING_RATE;
        steps[count].timeout_ms = 10;
        snprintf(steps[count].description, sizeof(steps[count].description),
                "Reduce learning rate to stabilize gradients");
        steps[count].expected_success_rate = 0.85f;
        steps[count].is_critical = true;
        count++;
    }

    // Step 3: Reduce batch size (if aggressive)
    if (count < max_steps && criteria->risk_tolerance > 0.3f) {
        steps[count].action = RECOVERY_EXEC_ACTION_REDUCE_BATCH_SIZE;
        steps[count].timeout_ms = 10;
        snprintf(steps[count].description, sizeof(steps[count].description),
                "Reduce batch size for more stable gradients");
        steps[count].expected_success_rate = 0.80f;
        steps[count].is_critical = false;
        count++;
    }

    // Step 4: Verify
    if (count < max_steps && criteria->require_verification) {
        steps[count].action = RECOVERY_EXEC_ACTION_VERIFY_STATE;
        steps[count].timeout_ms = 50;
        snprintf(steps[count].description, sizeof(steps[count].description),
                "Verify gradient stability");
        steps[count].expected_success_rate = 0.90f;
        steps[count].is_critical = false;
        count++;
    }

    return count;
}

/**
 * @brief Select actions for critical errors (SEGFAULT, etc.)
 *
 * WHAT: Choose recovery actions for critical crashes
 * WHY:  Critical errors require cautious recovery
 * HOW:  Analyze → Checkpoint restore or graceful shutdown
 */
static uint32_t select_actions_for_critical_error(
    plan_step_t* steps,
    uint32_t max_steps,
    const decision_criteria_t* criteria
) {
    uint32_t count = 0;

    // Step 1: Analyze diagnostic
    if (count < max_steps) {
        steps[count].action = RECOVERY_EXEC_ACTION_ANALYZE_DIAGNOSTIC;
        steps[count].timeout_ms = 50;
        snprintf(steps[count].description, sizeof(steps[count].description),
                "Analyze crash context and stack trace");
        steps[count].expected_success_rate = 0.95f;
        steps[count].is_critical = false;
        count++;
    }

    // Step 2: Attempt checkpoint restore
    if (count < max_steps && !criteria->allow_data_loss) {
        steps[count].action = RECOVERY_EXEC_ACTION_CHECKPOINT_RESTORE;
        steps[count].timeout_ms = 200;
        snprintf(steps[count].description, sizeof(steps[count].description),
                "Restore from checkpoint before crash");
        steps[count].expected_success_rate = 0.70f;
        steps[count].is_critical = true;
        count++;
    }

    // Step 3: If recovery impossible, graceful shutdown
    if (count < max_steps) {
        steps[count].action = RECOVERY_EXEC_ACTION_GRACEFUL_SHUTDOWN;
        steps[count].timeout_ms = 500;
        snprintf(steps[count].description, sizeof(steps[count].description),
                "Graceful shutdown to preserve data");
        steps[count].expected_success_rate = 0.99f;
        steps[count].is_critical = true;
        count++;
    }

    return count;
}

/**
 * @brief Select actions based on error type
 *
 * WHAT: Choose appropriate action sequence for error type
 * WHY:  Different errors need different recovery strategies
 * HOW:  Dispatches to specialized action selectors
 */
static uint32_t select_actions_for_error(
    error_type_t error_type,
    plan_step_t* steps,
    uint32_t max_steps,
    const decision_criteria_t* criteria
) {
    // Select based on error type
    switch (error_type) {
        case ERROR_TYPE_NAN_DETECTED:
        case ERROR_TYPE_INF_DETECTED:
        case ERROR_TYPE_NUMERICAL_OVERFLOW:
        case ERROR_TYPE_NUMERICAL_UNDERFLOW:
            return select_actions_for_nan(steps, max_steps, criteria);

        case ERROR_TYPE_OUT_OF_MEMORY:
        case ERROR_TYPE_BUFFER_OVERFLOW:
        case ERROR_TYPE_NULL_POINTER:
        case ERROR_TYPE_HEAP_CORRUPTION:
            return select_actions_for_memory_error(steps, max_steps, criteria);

        case ERROR_TYPE_GRADIENT_EXPLOSION:
        case ERROR_TYPE_GRADIENT_VANISHING:
            return select_actions_for_gradient_error(steps, max_steps, criteria);

        case ERROR_TYPE_SEGFAULT:
        case ERROR_TYPE_BUS_ERROR:
        case ERROR_TYPE_ILLEGAL_INSTRUCTION:
        case ERROR_TYPE_ABORT:
            return select_actions_for_critical_error(steps, max_steps, criteria);

        default:
            // Generic recovery: checkpoint + verify
            if (max_steps > 0) {
                steps[0].action = RECOVERY_EXEC_ACTION_CHECKPOINT_SAVE;
                steps[0].timeout_ms = 100;
                snprintf(steps[0].description, sizeof(steps[0].description),
                        "Generic recovery: save state");
                steps[0].expected_success_rate = 0.80f;
                steps[0].is_critical = true;
                return 1;
            }
            return 0;
    }
}

//=============================================================================
// Helper Functions - Plan Confidence
//=============================================================================

/**
 * @brief Calculate plan confidence
 *
 * WHAT: Estimate probability of plan success
 * WHY:  Need to know if plan is likely to work
 * HOW:  Combines step success rates and diagnostic confidence
 */
static float calculate_plan_confidence(
    const plan_step_t* steps,
    uint32_t step_count,
    float diagnostic_confidence
) {
    if (step_count == 0) {
        return 0.0f;
    }

    // Multiply step success probabilities (independence assumption)
    float combined_success = 1.0f;
    for (uint32_t i = 0; i < step_count; i++) {
        combined_success *= steps[i].expected_success_rate;
    }

    // Weight by diagnostic confidence
    float confidence = combined_success * diagnostic_confidence;

    // Clamp to [0, 1]
    if (confidence < 0.0f) confidence = 0.0f;
    if (confidence > 1.0f) confidence = 1.0f;

    return confidence;
}

/**
 * @brief Estimate plan execution time
 *
 * WHAT: Sum timeouts for all steps
 * WHY:  Need to know if plan fits within time budget
 * HOW:  Add step timeouts
 */
static uint32_t estimate_plan_time(
    const plan_step_t* steps,
    uint32_t step_count
) {
    uint32_t total_time = 0;
    for (uint32_t i = 0; i < step_count; i++) {
        total_time += steps[i].timeout_ms;
    }
    return total_time;
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

recovery_executive_config_t recovery_executive_default_config(void) {
    recovery_executive_config_t config;

    config.max_subgoals = 5;
    config.max_plan_steps = 20;
    config.enable_metacognitive_monitoring = true;
    config.replanning_confidence_threshold = 0.3f;

    // Default decision criteria
    config.default_criteria.risk_tolerance = 0.5f;
    config.default_criteria.max_recovery_time_ms = 1000;
    config.default_criteria.max_steps = 20;
    config.default_criteria.allow_data_loss = false;
    config.default_criteria.allow_performance_degradation = true;
    config.default_criteria.require_verification = true;
    config.default_criteria.min_confidence_threshold = 0.3f;

    return config;
}

recovery_executive_t* recovery_executive_create(
    const recovery_executive_config_t* config
) {
    // GUARD: NULL config check
    if (!config) {
        LOG_ERROR("NULL config in recovery_executive_create");
        return NULL;
    }

    // WHAT: Allocate executive structure
    // WHY:  Need memory for executive state
    // HOW:  Use nimcp_malloc for tracking
    recovery_executive_t* exec = (recovery_executive_t*)nimcp_malloc(
        sizeof(recovery_executive_t));
    if (!exec) {
        LOG_ERROR("Failed to allocate recovery executive");
        return NULL;
    }

    // Initialize to zero
    memset(exec, 0, sizeof(recovery_executive_t));

    // Copy configuration
    exec->config = *config;

    // Initialize decision criteria to defaults
    exec->criteria = config->default_criteria;

    // Initialize current state
    exec->current_goal = GOAL_NONE;
    exec->subgoal_count = 0;
    exec->active_plan = NULL;
    exec->current_step = 0;

    // Initialize monitoring
    exec->monitoring.confidence_in_plan = 1.0f;
    exec->monitoring.plan_working = true;
    exec->monitoring.steps_succeeded = 0;
    exec->monitoring.steps_failed = 0;
    exec->monitoring.progress_rate = 0.0f;

    // No diagnosis yet
    exec->has_diagnosis = false;

    // Statistics start at zero
    memset(&exec->stats, 0, sizeof(recovery_executive_stats_t));

    // Plan ID counter
    exec->next_plan_id = 1;

    // Mark as initialized
    exec->initialized = true;

    LOG_INFO("Recovery executive created successfully");

    return exec;
}

void recovery_executive_destroy(recovery_executive_t* exec) {
    // GUARD: NULL check
    if (!exec) {
        return;
    }

    // Free active plan if any
    if (exec->active_plan) {
        nimcp_free(exec->active_plan);
    }

    // Free executive structure
    nimcp_free(exec);

    LOG_INFO("Recovery executive destroyed");
}

bool recovery_executive_is_ready(const recovery_executive_t* exec) {
    if (!exec) {
        return false;
    }

    return exec->initialized;
}

//=============================================================================
// Goal Decomposition Functions
//=============================================================================

bool recovery_executive_decompose_goal(
    recovery_goal_t goal,
    recovery_goal_t* subgoals,
    uint32_t* subgoal_count
) {
    // GUARD: Parameter validation
    if (!subgoals || !subgoal_count) {
        LOG_ERROR("NULL parameters in recovery_executive_decompose_goal");
        return false;
    }

    // Initialize count
    *subgoal_count = 0;

    // WHAT: Decompose based on goal type
    // WHY:  Different goals have different decomposition strategies
    // HOW:  Switch on goal type, delegate to specialized functions
    switch (goal) {
        case RECOVERY_GOAL_FULL_RECOVERY:
            decompose_restore_functionality(subgoals, subgoal_count);
            break;

        case RECOVERY_GOAL_DEGRADED_MODE:
            // Degraded mode - single atomic goal
            subgoals[0] = goal;
            *subgoal_count = 1;
            break;

        case RECOVERY_GOAL_PREVENT_RECURRENCE:
            decompose_prevent_recurrence(subgoals, subgoal_count);
            break;

        case RECOVERY_GOAL_DATA_PRESERVATION:
            // Atomic goal - no decomposition needed
            subgoals[0] = goal;
            *subgoal_count = 1;
            break;

        case RECOVERY_GOAL_QUICK_FIX:
            *subgoal_count = 0;
            break;

        default:
            LOG_WARNING("Unknown goal type in decompose_goal: %d", (int)goal);
            return false;
    }

    return true;
}

//=============================================================================
// Plan Creation Functions
//=============================================================================

recovery_plan_t* recovery_executive_create_plan(
    recovery_executive_t* exec,
    const diagnostic_result_t* diagnosis,
    recovery_goal_t goal
) {
    return recovery_executive_create_plan_with_brain_input(
        exec, diagnosis, goal, NULL);
}

recovery_plan_t* recovery_executive_create_plan_with_brain_input(
    recovery_executive_t* exec,
    const diagnostic_result_t* diagnosis,
    recovery_goal_t goal,
    const brain_recovery_decision_t* brain_decision
) {
    // GUARD: Parameter validation
    if (!exec) {
        LOG_ERROR("NULL exec in create_plan");
        return NULL;
    }

    if (!diagnosis) {
        LOG_ERROR("NULL diagnosis in create_plan");
        return NULL;
    }

    // WHAT: Allocate plan structure
    recovery_plan_t* plan = (recovery_plan_t*)nimcp_malloc(sizeof(recovery_plan_t));
    if (!plan) {
        LOG_ERROR("Failed to allocate recovery plan");
        return NULL;
    }

    // Initialize plan
    memset(plan, 0, sizeof(recovery_plan_t));

    // Set goal
    plan->goal = goal;

    // Assign unique plan ID
    plan->plan_id = exec->next_plan_id++;

    // Record creation time
    plan->creation_time_us = get_time_us();

    // WHAT: Select actions based on error type and goal
    // WHY:  Different errors and goals require different action sequences
    // HOW:  Delegate to action selection functions
    uint32_t step_count = select_actions_for_error(
        diagnosis->error_type,
        plan->steps,
        exec->config.max_plan_steps,
        &exec->criteria
    );

    plan->step_count = step_count;

    // Calculate confidence
    float diag_confidence = diagnosis->confidence > 0.0f ? diagnosis->confidence : 0.5f;
    plan->confidence = calculate_plan_confidence(
        plan->steps,
        step_count,
        diag_confidence
    );

    // Estimate time
    plan->estimated_time_ms = estimate_plan_time(plan->steps, step_count);

    // Generate rationale
    snprintf(plan->rationale, sizeof(plan->rationale),
             "Plan for %s error with goal %s (%u steps, %.0f%% confidence)",
             diagnostics_get_error_type_name(diagnosis->error_type),
             recovery_executive_get_goal_name(goal),
             step_count,
             plan->confidence * 100.0f);

    // Store diagnosis for potential replanning
    exec->last_diagnosis = *diagnosis;
    exec->has_diagnosis = true;

    // Update statistics
    exec->stats.total_plans_created++;
    exec->stats.average_plan_confidence =
        (exec->stats.average_plan_confidence * (exec->stats.total_plans_created - 1) +
         plan->confidence) / exec->stats.total_plans_created;

    LOG_INFO("Created recovery plan ID %lu: %u steps, %.2f confidence",
             (unsigned long)plan->plan_id, step_count, plan->confidence);

    return plan;
}

void recovery_executive_free_plan(recovery_plan_t* plan) {
    if (plan) {
        nimcp_free(plan);
    }
}

//=============================================================================
// Plan Execution Functions
//=============================================================================

recovery_execution_result_t recovery_executive_execute_plan(
    recovery_executive_t* exec,
    const recovery_plan_t* plan
) {
    recovery_execution_result_t result;
    memset(&result, 0, sizeof(result));

    // GUARD: Parameter validation
    if (!exec) {
        LOG_ERROR("NULL exec in execute_plan");
        result.success = false;
        snprintf(result.failure_reason, sizeof(result.failure_reason),
                "NULL executive handle");
        return result;
    }

    if (!plan) {
        LOG_ERROR("NULL plan in execute_plan");
        result.success = false;
        snprintf(result.failure_reason, sizeof(result.failure_reason),
                "NULL plan");
        return result;
    }

    // Start timing
    uint64_t start_time = get_time_us();

    // Initialize execution state
    exec->active_plan = (recovery_plan_t*)plan;  // Cast away const for tracking
    exec->current_step = 0;

    // Reset monitoring
    exec->monitoring.confidence_in_plan = plan->confidence;
    exec->monitoring.plan_working = true;
    exec->monitoring.steps_succeeded = 0;
    exec->monitoring.steps_failed = 0;

    // WHAT: Execute each step in sequence
    // WHY:  Plans are ordered sequences of actions
    // HOW:  Iterate through steps, execute each, update monitoring
    result.success = true;
    result.failed_step = -1;

    for (uint32_t i = 0; i < plan->step_count; i++) {
        exec->current_step = i;

        LOG_DEBUG("Executing plan step %u: %s",
                 i, plan->steps[i].description);

        // SIMULATION: In real implementation, would execute actual action
        // For now, simulate success based on expected success rate
        bool step_success = (plan->steps[i].expected_success_rate > 0.5f);

        if (step_success) {
            exec->monitoring.steps_succeeded++;
            result.steps_completed++;
        } else {
            exec->monitoring.steps_failed++;

            // If critical step failed, abort plan
            if (plan->steps[i].is_critical) {
                result.success = false;
                result.failed_step = (int32_t)i;
                snprintf(result.failure_reason, sizeof(result.failure_reason),
                        "Critical step %u failed: %s",
                        i, plan->steps[i].description);

                LOG_ERROR("Plan execution failed at critical step %u", i);
                break;
            }
        }

        // Update metacognitive monitoring
        if (exec->config.enable_metacognitive_monitoring) {
            float progress = (float)(i + 1) / plan->step_count;
            exec->monitoring.progress_rate = progress;

            // Update confidence based on success/failure pattern
            if (exec->monitoring.steps_failed > exec->monitoring.steps_succeeded) {
                exec->monitoring.confidence_in_plan *= 0.8f;  // Reduce confidence
                exec->monitoring.plan_working = false;
            } else {
                exec->monitoring.confidence_in_plan = fminf(
                    exec->monitoring.confidence_in_plan * 1.1f, 1.0f);
            }
        }
    }

    // Stop timing
    uint64_t end_time = get_time_us();
    result.total_time_us = (uint32_t)(end_time - start_time);

    // Update statistics
    exec->stats.total_plans_executed++;
    exec->stats.total_execution_time_us += result.total_time_us;

    if (result.success) {
        exec->stats.successful_plans++;
        LOG_INFO("Plan executed successfully in %u us", result.total_time_us);
    } else {
        exec->stats.failed_plans++;
        LOG_WARNING("Plan execution failed: %s", result.failure_reason);
    }

    return result;
}

//=============================================================================
// Plan Monitoring Functions
//=============================================================================

bool recovery_executive_is_plan_working(const recovery_executive_t* exec) {
    if (!exec) {
        return false;
    }

    if (!exec->config.enable_metacognitive_monitoring) {
        // If monitoring disabled, assume plan is working
        return true;
    }

    return exec->monitoring.plan_working;
}

bool recovery_executive_get_monitoring_state(
    const recovery_executive_t* exec,
    plan_monitoring_state_t* state
) {
    if (!exec || !state) {
        return false;
    }

    *state = exec->monitoring;
    return true;
}

//=============================================================================
// Replanning Functions
//=============================================================================

recovery_plan_t* recovery_executive_replan(
    recovery_executive_t* exec,
    const char* reason
) {
    // GUARD: Parameter validation
    if (!exec) {
        LOG_ERROR("NULL exec in replan");
        return NULL;
    }

    if (!reason) {
        LOG_ERROR("NULL reason in replan");
        return NULL;
    }

    // Must have stored diagnosis for replanning
    if (!exec->has_diagnosis) {
        LOG_ERROR("No stored diagnosis for replanning");
        return NULL;
    }

    LOG_INFO("Replanning: %s", reason);

    // Update statistics
    exec->stats.replanning_events++;

    // Create new plan with same goal and diagnosis
    recovery_plan_t* new_plan = recovery_executive_create_plan(
        exec,
        &exec->last_diagnosis,
        exec->current_goal
    );

    return new_plan;
}

recovery_plan_t* recovery_executive_replan_with_goal(
    recovery_executive_t* exec,
    recovery_goal_t new_goal,
    const char* reason
) {
    if (!exec) {
        return NULL;
    }

    LOG_INFO("Replanning with new goal %s: %s",
             recovery_executive_get_goal_name(new_goal), reason);

    // Update current goal
    exec->current_goal = new_goal;

    // Replan with new goal
    return recovery_executive_replan(exec, reason);
}

recovery_plan_t* recovery_executive_replan_with_brain_input(
    recovery_executive_t* exec,
    const brain_recovery_decision_t* brain_decision,
    const char* reason
) {
    if (!exec || !brain_decision || !reason) {
        return NULL;
    }

    if (!exec->has_diagnosis) {
        LOG_ERROR("No stored diagnosis for replanning");
        return NULL;
    }

    LOG_INFO("Replanning with brain input: %s", reason);

    exec->stats.replanning_events++;

    // Create plan with brain input
    return recovery_executive_create_plan_with_brain_input(
        exec,
        &exec->last_diagnosis,
        exec->current_goal,
        brain_decision
    );
}

//=============================================================================
// Decision Criteria Functions
//=============================================================================

bool recovery_executive_set_decision_criteria(
    recovery_executive_t* exec,
    const decision_criteria_t* criteria
) {
    if (!exec || !criteria) {
        return false;
    }

    exec->criteria = *criteria;
    return true;
}

bool recovery_executive_get_decision_criteria(
    const recovery_executive_t* exec,
    decision_criteria_t* criteria
) {
    if (!exec || !criteria) {
        return false;
    }

    *criteria = exec->criteria;
    return true;
}

//=============================================================================
// Configuration Functions
//=============================================================================

bool recovery_executive_get_config(
    const recovery_executive_t* exec,
    recovery_executive_config_t* config
) {
    if (!exec || !config) {
        return false;
    }

    *config = exec->config;
    return true;
}

bool recovery_executive_update_config(
    recovery_executive_t* exec,
    const recovery_executive_config_t* config
) {
    if (!exec || !config) {
        return false;
    }

    exec->config = *config;
    return true;
}

//=============================================================================
// Statistics Functions
//=============================================================================

bool recovery_executive_get_stats(
    const recovery_executive_t* exec,
    recovery_executive_stats_t* stats
) {
    if (!exec || !stats) {
        return false;
    }

    *stats = exec->stats;
    return true;
}

void recovery_executive_reset_stats(recovery_executive_t* exec) {
    if (!exec) {
        return;
    }

    memset(&exec->stats, 0, sizeof(recovery_executive_stats_t));
}

//=============================================================================
// Utility Functions
//=============================================================================

const char* recovery_executive_get_goal_name(recovery_goal_t goal) {
    switch (goal) {
        case RECOVERY_GOAL_QUICK_FIX:           return "QUICK_FIX";
        case RECOVERY_GOAL_FULL_RECOVERY:       return "FULL_RECOVERY";
        case RECOVERY_GOAL_DEGRADED_MODE:       return "DEGRADED_MODE";
        case RECOVERY_GOAL_PREVENT_RECURRENCE:  return "PREVENT_RECURRENCE";
        case RECOVERY_GOAL_DATA_PRESERVATION:   return "DATA_PRESERVATION";
        default:                                return "UNKNOWN";
    }
}

const char* recovery_executive_get_action_name(recovery_exec_action_t action) {
    switch (action) {
        case RECOVERY_EXEC_ACTION_NONE:                 return "NONE";
        case RECOVERY_EXEC_ACTION_CHECKPOINT_SAVE:      return "CHECKPOINT_SAVE";
        case RECOVERY_EXEC_ACTION_CHECKPOINT_RESTORE:   return "CHECKPOINT_RESTORE";
        case RECOVERY_EXEC_ACTION_ANALYZE_DIAGNOSTIC:   return "ANALYZE_DIAGNOSTIC";
        case RECOVERY_EXEC_ACTION_RESET_SUBSYSTEM:      return "RESET_SUBSYSTEM";
        case RECOVERY_EXEC_ACTION_REDUCE_PRECISION:     return "REDUCE_PRECISION";
        case RECOVERY_EXEC_ACTION_REDUCE_LEARNING_RATE: return "REDUCE_LEARNING_RATE";
        case RECOVERY_EXEC_ACTION_REDUCE_BATCH_SIZE:    return "REDUCE_BATCH_SIZE";
        case RECOVERY_EXEC_ACTION_CLEAR_CACHE:          return "CLEAR_CACHE";
        case RECOVERY_EXEC_ACTION_ISOLATE_COMPONENT:    return "ISOLATE_COMPONENT";
        case RECOVERY_EXEC_ACTION_VERIFY_STATE:         return "VERIFY_STATE";
        case RECOVERY_EXEC_ACTION_RETRY_OPERATION:      return "RETRY_OPERATION";
        case RECOVERY_EXEC_ACTION_FALLBACK_MODE:        return "FALLBACK_MODE";
        case RECOVERY_EXEC_ACTION_GRACEFUL_SHUTDOWN:    return "GRACEFUL_SHUTDOWN";
        case RECOVERY_EXEC_ACTION_CUSTOM:               return "CUSTOM";
        default:                                        return "UNKNOWN";
    }
}
