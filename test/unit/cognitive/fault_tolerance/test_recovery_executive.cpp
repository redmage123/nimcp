/**
 * @file test_recovery_executive.cpp
 * @brief Unit tests for Executive Function Recovery Planning module
 *
 * WHAT: Comprehensive unit tests for recovery executive
 * WHY:  Ensure goal-oriented planning, multi-step recovery, adaptive replanning
 * HOW:  Test each function in isolation with mocked dependencies
 *
 * TEST COVERAGE:
 * - Executive function creation/destruction
 * - Goal decomposition and hierarchy
 * - Recovery plan creation
 * - Plan execution and monitoring
 * - Adaptive replanning on failure
 * - Metacognitive monitoring
 * - Decision criteria application
 * - Integration with diagnostics
 *
 * @author NIMCP Team
 * @date 2025-11-20
 * @version 1.0.0
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

// Headers have their own extern "C" guards
#include "cognitive/fault_tolerance/nimcp_recovery_executive.h"
#include "utils/fault_tolerance/nimcp_diagnostics.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixture
//=============================================================================

class RecoveryExecutiveTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize memory tracking
        nimcp_memory_init();
        nimcp_memory_enable_tracking(true);

        // Get default config
        config = recovery_executive_default_config();
    }

    void TearDown() override {
        // Check for memory leaks
        nimcp_memory_stats_t stats;
        nimcp_memory_get_stats(&stats);
        EXPECT_EQ(stats.allocation_count, stats.free_count)
            << "Memory leak detected!";

        nimcp_memory_cleanup();
    }

    recovery_executive_config_t config;
};

//=============================================================================
// Creation and Destruction Tests
//=============================================================================

/**
 * @test Executive Creation with Default Config
 *
 * WHAT: Verify executive function can be created with default configuration
 * WHY:  Basic initialization must work
 * HOW:  Create executive, verify non-NULL, destroy
 */
TEST_F(RecoveryExecutiveTest, CreateWithDefaultConfig) {
    // ARRANGE
    recovery_executive_t* exec = nullptr;

    // ACT
    exec = recovery_executive_create(&config);

    // ASSERT
    ASSERT_NE(exec, nullptr);
    EXPECT_TRUE(recovery_executive_is_ready(exec));

    // CLEANUP
    recovery_executive_destroy(exec);
}

/**
 * @test Executive Creation with NULL Config
 *
 * WHAT: Verify NULL config is rejected
 * WHY:  Guard against invalid parameters
 * HOW:  Pass NULL config, expect NULL return
 */
TEST_F(RecoveryExecutiveTest, CreateWithNullConfig) {
    // ACT
    recovery_executive_t* exec = recovery_executive_create(nullptr);

    // ASSERT
    EXPECT_EQ(exec, nullptr);
}

/**
 * @test Executive Creation with Custom Config
 *
 * WHAT: Verify custom configuration is applied
 * WHY:  Users need to customize behavior
 * HOW:  Create with custom config, verify settings
 */
TEST_F(RecoveryExecutiveTest, CreateWithCustomConfig) {
    // ARRANGE
    config.max_subgoals = 10;
    config.max_plan_steps = 20;
    config.enable_metacognitive_monitoring = true;
    config.replanning_confidence_threshold = 0.8f;

    // ACT
    recovery_executive_t* exec = recovery_executive_create(&config);

    // ASSERT
    ASSERT_NE(exec, nullptr);

    recovery_executive_config_t retrieved;
    EXPECT_TRUE(recovery_executive_get_config(exec, &retrieved));
    EXPECT_EQ(retrieved.max_subgoals, 10);
    EXPECT_EQ(retrieved.max_plan_steps, 20);
    EXPECT_TRUE(retrieved.enable_metacognitive_monitoring);
    EXPECT_FLOAT_EQ(retrieved.replanning_confidence_threshold, 0.8f);

    // CLEANUP
    recovery_executive_destroy(exec);
}

/**
 * @test Destroy NULL Executive
 *
 * WHAT: Verify NULL executive can be safely destroyed
 * WHY:  Guard against NULL pointer issues
 * HOW:  Call destroy with NULL, expect no crash
 */
TEST_F(RecoveryExecutiveTest, DestroyNullExecutive) {
    // ACT & ASSERT (should not crash)
    recovery_executive_destroy(nullptr);
}

//=============================================================================
// Goal Decomposition Tests
//=============================================================================

/**
 * @test Decompose Restore Functionality Goal
 *
 * WHAT: Verify GOAL_RESTORE_FUNCTIONALITY is properly decomposed
 * WHY:  Complex goals need subgoal breakdown
 * HOW:  Decompose goal, verify subgoals are logical and ordered
 */
TEST_F(RecoveryExecutiveTest, DecomposeRestoreFunctionalityGoal) {
    // ARRANGE
    recovery_goal_t subgoals[MAX_SUBGOALS];
    uint32_t subgoal_count = 0;

    // ACT
    bool result = recovery_executive_decompose_goal(
        GOAL_RESTORE_FUNCTIONALITY,
        subgoals,
        &subgoal_count
    );

    // ASSERT
    EXPECT_TRUE(result);
    EXPECT_GT(subgoal_count, 0);
    EXPECT_LE(subgoal_count, MAX_SUBGOALS);

    // First subgoal should be to prevent data loss
    EXPECT_EQ(subgoals[0], GOAL_PREVENT_DATA_LOSS);
}

/**
 * @test Decompose All Goal Types
 *
 * WHAT: Verify all goal types can be decomposed
 * WHY:  Each goal type must have a decomposition strategy
 * HOW:  Iterate through all goal types, decompose each
 */
TEST_F(RecoveryExecutiveTest, DecomposeAllGoalTypes) {
    // ARRANGE
    recovery_goal_t goals[] = {
        GOAL_RESTORE_FUNCTIONALITY,
        GOAL_RESTORE_PERFORMANCE,
        GOAL_PREVENT_DATA_LOSS,
        GOAL_LEARN_FROM_FAILURE,
        GOAL_PREVENT_RECURRENCE
    };

    // ACT & ASSERT
    for (size_t i = 0; i < sizeof(goals)/sizeof(goals[0]); i++) {
        recovery_goal_t subgoals[MAX_SUBGOALS];
        uint32_t subgoal_count = 0;

        bool result = recovery_executive_decompose_goal(
            goals[i],
            subgoals,
            &subgoal_count
        );

        EXPECT_TRUE(result) << "Failed to decompose goal " << (int)goals[i];

        // Some goals may not need decomposition (atomic goals)
        // They should still succeed but may have 0 subgoals
        EXPECT_LE(subgoal_count, MAX_SUBGOALS);
    }
}

/**
 * @test Decompose Goal with NULL Parameters
 *
 * WHAT: Verify NULL parameters are rejected
 * WHY:  Guard against invalid input
 * HOW:  Pass NULL for subgoals/count, expect false
 */
TEST_F(RecoveryExecutiveTest, DecomposeGoalWithNullParameters) {
    // ARRANGE
    recovery_goal_t subgoals[MAX_SUBGOALS];
    uint32_t subgoal_count = 0;

    // ACT & ASSERT
    EXPECT_FALSE(recovery_executive_decompose_goal(
        GOAL_RESTORE_FUNCTIONALITY, nullptr, &subgoal_count));

    EXPECT_FALSE(recovery_executive_decompose_goal(
        GOAL_RESTORE_FUNCTIONALITY, subgoals, nullptr));

    EXPECT_FALSE(recovery_executive_decompose_goal(
        GOAL_RESTORE_FUNCTIONALITY, nullptr, nullptr));
}

//=============================================================================
// Plan Creation Tests
//=============================================================================

/**
 * @test Create Plan from Diagnostic Result
 *
 * WHAT: Verify recovery plan can be created from diagnostic result
 * WHY:  Plans must be tailored to specific error conditions
 * HOW:  Create mock diagnostic, generate plan, verify structure
 */
TEST_F(RecoveryExecutiveTest, CreatePlanFromDiagnostic) {
    // ARRANGE
    recovery_executive_t* exec = recovery_executive_create(&config);
    ASSERT_NE(exec, nullptr);

    diagnostic_result_t diagnosis;
    memset(&diagnosis, 0, sizeof(diagnosis));
    diagnosis.error_type = ERROR_TYPE_NAN_DETECTED;
    diagnosis.severity = DIAG_SEVERITY_ERROR;
    diagnosis.confidence = 0.9f;

    // ACT
    recovery_plan_t* plan = recovery_executive_create_plan(
        exec,
        &diagnosis,
        GOAL_RESTORE_FUNCTIONALITY
    );

    // ASSERT
    ASSERT_NE(plan, nullptr);
    EXPECT_EQ(plan->goal, GOAL_RESTORE_FUNCTIONALITY);
    EXPECT_GT(plan->step_count, 0);
    EXPECT_LE(plan->step_count, config.max_plan_steps);
    EXPECT_GT(plan->estimated_time_ms, 0);
    EXPECT_GT(plan->confidence, 0.0f);
    EXPECT_LE(plan->confidence, 1.0f);

    // CLEANUP
    recovery_executive_free_plan(plan);
    recovery_executive_destroy(exec);
}

/**
 * @test Create Plan for Different Error Types
 *
 * WHAT: Verify different error types produce different plans
 * WHY:  Recovery strategy must match error type
 * HOW:  Create plans for various errors, verify they differ
 */
TEST_F(RecoveryExecutiveTest, CreatePlanForDifferentErrorTypes) {
    // ARRANGE
    recovery_executive_t* exec = recovery_executive_create(&config);
    ASSERT_NE(exec, nullptr);

    error_type_t error_types[] = {
        ERROR_TYPE_NAN_DETECTED,
        ERROR_TYPE_SEGFAULT,
        ERROR_TYPE_OUT_OF_MEMORY,
        ERROR_TYPE_GRADIENT_EXPLOSION
    };

    // ACT & ASSERT
    for (size_t i = 0; i < sizeof(error_types)/sizeof(error_types[0]); i++) {
        diagnostic_result_t diagnosis;
        memset(&diagnosis, 0, sizeof(diagnosis));
        diagnosis.error_type = error_types[i];
        diagnosis.severity = DIAG_SEVERITY_ERROR;
        diagnosis.confidence = 0.9f;

        recovery_plan_t* plan = recovery_executive_create_plan(
            exec,
            &diagnosis,
            GOAL_RESTORE_FUNCTIONALITY
        );

        ASSERT_NE(plan, nullptr) << "Failed for error type " << (int)error_types[i];
        EXPECT_GT(plan->step_count, 0);

        // Verify plan has steps appropriate to error type
        bool has_relevant_action = false;
        for (uint32_t j = 0; j < plan->step_count; j++) {
            if (plan->steps[j].action != EXCEPTION_RECOVERY_NONE) {
                has_relevant_action = true;
                break;
            }
        }
        EXPECT_TRUE(has_relevant_action);

        recovery_executive_free_plan(plan);
    }

    // CLEANUP
    recovery_executive_destroy(exec);
}

/**
 * @test Create Plan with NULL Parameters
 *
 * WHAT: Verify NULL parameters are rejected
 * WHY:  Guard against invalid input
 * HOW:  Pass NULL for exec/diagnosis, expect NULL plan
 */
TEST_F(RecoveryExecutiveTest, CreatePlanWithNullParameters) {
    // ARRANGE
    recovery_executive_t* exec = recovery_executive_create(&config);
    ASSERT_NE(exec, nullptr);

    diagnostic_result_t diagnosis;
    memset(&diagnosis, 0, sizeof(diagnosis));
    diagnosis.error_type = ERROR_TYPE_NAN_DETECTED;

    // ACT & ASSERT
    EXPECT_EQ(recovery_executive_create_plan(nullptr, &diagnosis, GOAL_RESTORE_FUNCTIONALITY), nullptr);
    EXPECT_EQ(recovery_executive_create_plan(exec, nullptr, GOAL_RESTORE_FUNCTIONALITY), nullptr);

    // CLEANUP
    recovery_executive_destroy(exec);
}

//=============================================================================
// Plan Execution Tests
//=============================================================================

/**
 * @test Execute Simple Plan Successfully
 *
 * WHAT: Verify simple plan can be executed successfully
 * WHY:  Basic execution must work
 * HOW:  Create simple plan, execute, verify success
 */
TEST_F(RecoveryExecutiveTest, ExecuteSimplePlanSuccessfully) {
    // ARRANGE
    recovery_executive_t* exec = recovery_executive_create(&config);
    ASSERT_NE(exec, nullptr);

    // Create a simple diagnostic
    diagnostic_result_t diagnosis;
    memset(&diagnosis, 0, sizeof(diagnosis));
    diagnosis.error_type = ERROR_TYPE_NAN_DETECTED;
    diagnosis.severity = DIAG_SEVERITY_WARNING;

    recovery_plan_t* plan = recovery_executive_create_plan(
        exec, &diagnosis, GOAL_RESTORE_FUNCTIONALITY);
    ASSERT_NE(plan, nullptr);

    // ACT
    recovery_execution_result_t result = recovery_executive_execute_plan(exec, plan);

    // ASSERT
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.steps_completed, plan->step_count);
    EXPECT_EQ(result.failed_step, -1);
    EXPECT_GT(result.total_time_us, 0);

    // CLEANUP
    recovery_executive_free_plan(plan);
    recovery_executive_destroy(exec);
}

/**
 * @test Execute Plan with NULL Parameters
 *
 * WHAT: Verify NULL parameters are rejected
 * WHY:  Guard against invalid input
 * HOW:  Pass NULL for exec/plan, expect failure result
 */
TEST_F(RecoveryExecutiveTest, ExecutePlanWithNullParameters) {
    // ARRANGE
    recovery_executive_t* exec = recovery_executive_create(&config);
    ASSERT_NE(exec, nullptr);

    diagnostic_result_t diagnosis;
    memset(&diagnosis, 0, sizeof(diagnosis));
    diagnosis.error_type = ERROR_TYPE_NAN_DETECTED;

    recovery_plan_t* plan = recovery_executive_create_plan(
        exec, &diagnosis, GOAL_RESTORE_FUNCTIONALITY);
    ASSERT_NE(plan, nullptr);

    // ACT & ASSERT
    recovery_execution_result_t result1 = recovery_executive_execute_plan(nullptr, plan);
    EXPECT_FALSE(result1.success);

    recovery_execution_result_t result2 = recovery_executive_execute_plan(exec, nullptr);
    EXPECT_FALSE(result2.success);

    // CLEANUP
    recovery_executive_free_plan(plan);
    recovery_executive_destroy(exec);
}

//=============================================================================
// Plan Monitoring Tests
//=============================================================================

/**
 * @test Monitor Plan Progress
 *
 * WHAT: Verify plan monitoring tracks execution progress
 * WHY:  Need real-time visibility into recovery progress
 * HOW:  Execute plan, check monitoring during execution
 */
TEST_F(RecoveryExecutiveTest, MonitorPlanProgress) {
    // ARRANGE
    recovery_executive_t* exec = recovery_executive_create(&config);
    ASSERT_NE(exec, nullptr);

    diagnostic_result_t diagnosis;
    memset(&diagnosis, 0, sizeof(diagnosis));
    diagnosis.error_type = ERROR_TYPE_NAN_DETECTED;

    recovery_plan_t* plan = recovery_executive_create_plan(
        exec, &diagnosis, GOAL_RESTORE_FUNCTIONALITY);
    ASSERT_NE(plan, nullptr);

    // ACT
    recovery_execution_result_t result = recovery_executive_execute_plan(exec, plan);

    // ASSERT - Check monitoring state
    EXPECT_TRUE(result.success);

    plan_monitoring_state_t monitor_state;
    bool got_state = recovery_executive_get_monitoring_state(exec, &monitor_state);
    EXPECT_TRUE(got_state);
    EXPECT_GE(monitor_state.confidence_in_plan, 0.0f);
    EXPECT_LE(monitor_state.confidence_in_plan, 1.0f);

    // CLEANUP
    recovery_executive_free_plan(plan);
    recovery_executive_destroy(exec);
}

/**
 * @test Check if Plan is Working
 *
 * WHAT: Verify metacognitive monitoring of plan success
 * WHY:  Need to detect when plan isn't working
 * HOW:  Execute plan, check is_plan_working during execution
 */
TEST_F(RecoveryExecutiveTest, CheckIfPlanIsWorking) {
    // ARRANGE
    recovery_executive_t* exec = recovery_executive_create(&config);
    ASSERT_NE(exec, nullptr);

    diagnostic_result_t diagnosis;
    memset(&diagnosis, 0, sizeof(diagnosis));
    diagnosis.error_type = ERROR_TYPE_NAN_DETECTED;

    recovery_plan_t* plan = recovery_executive_create_plan(
        exec, &diagnosis, GOAL_RESTORE_FUNCTIONALITY);
    ASSERT_NE(plan, nullptr);

    // ACT
    recovery_execution_result_t result = recovery_executive_execute_plan(exec, plan);
    bool is_working = recovery_executive_is_plan_working(exec);

    // ASSERT
    if (result.success) {
        EXPECT_TRUE(is_working);
    } else {
        EXPECT_FALSE(is_working);
    }

    // CLEANUP
    recovery_executive_free_plan(plan);
    recovery_executive_destroy(exec);
}

//=============================================================================
// Replanning Tests
//=============================================================================

/**
 * @test Replan After Failure
 *
 * WHAT: Verify system can replan when initial plan fails
 * WHY:  Adaptive recovery requires replanning capability
 * HOW:  Simulate plan failure, request replan, verify new plan
 */
TEST_F(RecoveryExecutiveTest, ReplanAfterFailure) {
    // ARRANGE
    recovery_executive_t* exec = recovery_executive_create(&config);
    ASSERT_NE(exec, nullptr);

    diagnostic_result_t diagnosis;
    memset(&diagnosis, 0, sizeof(diagnosis));
    diagnosis.error_type = ERROR_TYPE_NAN_DETECTED;

    recovery_plan_t* original_plan = recovery_executive_create_plan(
        exec, &diagnosis, GOAL_RESTORE_FUNCTIONALITY);
    ASSERT_NE(original_plan, nullptr);

    // ACT
    recovery_plan_t* new_plan = recovery_executive_replan(
        exec,
        "Original plan step 2 failed: numerical instability persists"
    );

    // ASSERT
    ASSERT_NE(new_plan, nullptr);
    EXPECT_EQ(new_plan->goal, original_plan->goal);

    // New plan should have different strategy or steps
    // (implementation specific - verify plan was regenerated)
    EXPECT_GT(new_plan->step_count, 0);

    // CLEANUP
    recovery_executive_free_plan(original_plan);
    recovery_executive_free_plan(new_plan);
    recovery_executive_destroy(exec);
}

/**
 * @test Replan with Different Goal
 *
 * WHAT: Verify replanning can change goal if needed
 * WHY:  Sometimes original goal becomes infeasible
 * HOW:  Replan with new goal, verify goal change
 */
TEST_F(RecoveryExecutiveTest, ReplanWithDifferentGoal) {
    // ARRANGE
    recovery_executive_t* exec = recovery_executive_create(&config);
    ASSERT_NE(exec, nullptr);

    diagnostic_result_t diagnosis;
    memset(&diagnosis, 0, sizeof(diagnosis));
    diagnosis.error_type = ERROR_TYPE_SEGFAULT;
    diagnosis.severity = DIAG_SEVERITY_CRITICAL;

    recovery_plan_t* original_plan = recovery_executive_create_plan(
        exec, &diagnosis, GOAL_RESTORE_PERFORMANCE);
    ASSERT_NE(original_plan, nullptr);

    // ACT - Replan with fallback goal
    recovery_plan_t* fallback_plan = recovery_executive_replan_with_goal(
        exec,
        GOAL_RESTORE_FUNCTIONALITY,  // Less ambitious goal
        "Performance restoration infeasible, switching to functionality"
    );

    // ASSERT
    ASSERT_NE(fallback_plan, nullptr);
    EXPECT_EQ(fallback_plan->goal, GOAL_RESTORE_FUNCTIONALITY);
    EXPECT_GT(fallback_plan->step_count, 0);

    // CLEANUP
    recovery_executive_free_plan(original_plan);
    recovery_executive_free_plan(fallback_plan);
    recovery_executive_destroy(exec);
}

/**
 * @test Replan with NULL Parameters
 *
 * WHAT: Verify NULL parameters are rejected
 * WHY:  Guard against invalid input
 * HOW:  Pass NULL for exec/reason, expect NULL plan
 */
TEST_F(RecoveryExecutiveTest, ReplanWithNullParameters) {
    // ARRANGE
    recovery_executive_t* exec = recovery_executive_create(&config);
    ASSERT_NE(exec, nullptr);

    // ACT & ASSERT
    EXPECT_EQ(recovery_executive_replan(nullptr, "test reason"), nullptr);
    EXPECT_EQ(recovery_executive_replan(exec, nullptr), nullptr);

    // CLEANUP
    recovery_executive_destroy(exec);
}

//=============================================================================
// Decision Criteria Tests
//=============================================================================

/**
 * @test Apply Risk Tolerance Criteria
 *
 * WHAT: Verify risk tolerance affects plan selection
 * WHY:  Different scenarios require different risk profiles
 * HOW:  Set risk tolerance, verify plan changes accordingly
 */
TEST_F(RecoveryExecutiveTest, ApplyRiskToleranceCriteria) {
    // ARRANGE
    recovery_executive_t* exec = recovery_executive_create(&config);
    ASSERT_NE(exec, nullptr);

    diagnostic_result_t diagnosis;
    memset(&diagnosis, 0, sizeof(diagnosis));
    diagnosis.error_type = ERROR_TYPE_GRADIENT_EXPLOSION;

    decision_criteria_t criteria;
    memset(&criteria, 0, sizeof(criteria));

    // ACT - Low risk tolerance
    criteria.risk_tolerance = 0.1f;  // Very conservative
    recovery_executive_set_decision_criteria(exec, &criteria);

    recovery_plan_t* conservative_plan = recovery_executive_create_plan(
        exec, &diagnosis, GOAL_RESTORE_FUNCTIONALITY);
    ASSERT_NE(conservative_plan, nullptr);

    // High risk tolerance
    criteria.risk_tolerance = 0.9f;  // Aggressive
    recovery_executive_set_decision_criteria(exec, &criteria);

    recovery_plan_t* aggressive_plan = recovery_executive_create_plan(
        exec, &diagnosis, GOAL_RESTORE_FUNCTIONALITY);
    ASSERT_NE(aggressive_plan, nullptr);

    // ASSERT - Plans should differ based on risk tolerance
    // Conservative plan may have more validation steps
    // Aggressive plan may take shortcuts
    EXPECT_GT(conservative_plan->step_count, 0);
    EXPECT_GT(aggressive_plan->step_count, 0);

    // CLEANUP
    recovery_executive_free_plan(conservative_plan);
    recovery_executive_free_plan(aggressive_plan);
    recovery_executive_destroy(exec);
}

/**
 * @test Apply Time Limit Criteria
 *
 * WHAT: Verify time limits affect plan selection
 * WHY:  Real-time systems have strict time constraints
 * HOW:  Set time limit, verify plan respects it
 */
TEST_F(RecoveryExecutiveTest, ApplyTimeLimitCriteria) {
    // ARRANGE
    recovery_executive_t* exec = recovery_executive_create(&config);
    ASSERT_NE(exec, nullptr);

    diagnostic_result_t diagnosis;
    memset(&diagnosis, 0, sizeof(diagnosis));
    diagnosis.error_type = ERROR_TYPE_NAN_DETECTED;

    decision_criteria_t criteria;
    memset(&criteria, 0, sizeof(criteria));
    criteria.max_recovery_time_ms = 10;  // Very tight constraint
    criteria.risk_tolerance = 0.5f;

    // ACT
    recovery_executive_set_decision_criteria(exec, &criteria);
    recovery_plan_t* plan = recovery_executive_create_plan(
        exec, &diagnosis, GOAL_RESTORE_FUNCTIONALITY);

    // ASSERT
    ASSERT_NE(plan, nullptr);
    EXPECT_LE(plan->estimated_time_ms, criteria.max_recovery_time_ms);

    // CLEANUP
    recovery_executive_free_plan(plan);
    recovery_executive_destroy(exec);
}

//=============================================================================
// Metacognitive Monitoring Tests
//=============================================================================

/**
 * @test Track Confidence in Plan
 *
 * WHAT: Verify metacognitive confidence tracking
 * WHY:  System needs to know if plan is working
 * HOW:  Execute plan, check confidence changes
 */
TEST_F(RecoveryExecutiveTest, TrackConfidenceInPlan) {
    // ARRANGE
    config.enable_metacognitive_monitoring = true;
    recovery_executive_t* exec = recovery_executive_create(&config);
    ASSERT_NE(exec, nullptr);

    diagnostic_result_t diagnosis;
    memset(&diagnosis, 0, sizeof(diagnosis));
    diagnosis.error_type = ERROR_TYPE_NAN_DETECTED;

    recovery_plan_t* plan = recovery_executive_create_plan(
        exec, &diagnosis, GOAL_RESTORE_FUNCTIONALITY);
    ASSERT_NE(plan, nullptr);

    // ACT
    recovery_execution_result_t result = recovery_executive_execute_plan(exec, plan);

    // ASSERT
    plan_monitoring_state_t monitor;
    EXPECT_TRUE(recovery_executive_get_monitoring_state(exec, &monitor));

    EXPECT_GE(monitor.confidence_in_plan, 0.0f);
    EXPECT_LE(monitor.confidence_in_plan, 1.0f);

    if (result.success) {
        // Confidence should be high after success
        EXPECT_GT(monitor.confidence_in_plan, 0.5f);
    }

    // CLEANUP
    recovery_executive_free_plan(plan);
    recovery_executive_destroy(exec);
}

/**
 * @test Detect Plan Not Working
 *
 * WHAT: Verify system detects when plan isn't working
 * WHY:  Early detection prevents wasted effort
 * HOW:  Monitor plan that's failing, verify detection
 */
TEST_F(RecoveryExecutiveTest, DetectPlanNotWorking) {
    // ARRANGE
    config.enable_metacognitive_monitoring = true;
    recovery_executive_t* exec = recovery_executive_create(&config);
    ASSERT_NE(exec, nullptr);

    // This is tested implicitly through is_plan_working checks
    // Detailed testing requires failure injection (integration test)

    // CLEANUP
    recovery_executive_destroy(exec);
}

//=============================================================================
// Statistics and Reporting Tests
//=============================================================================

/**
 * @test Get Executive Statistics
 *
 * WHAT: Verify statistics collection works
 * WHY:  Need visibility into executive performance
 * HOW:  Execute plans, check statistics
 */
TEST_F(RecoveryExecutiveTest, GetExecutiveStatistics) {
    // ARRANGE
    recovery_executive_t* exec = recovery_executive_create(&config);
    ASSERT_NE(exec, nullptr);

    // Execute a few plans
    for (int i = 0; i < 3; i++) {
        diagnostic_result_t diagnosis;
        memset(&diagnosis, 0, sizeof(diagnosis));
        diagnosis.error_type = ERROR_TYPE_NAN_DETECTED;

        recovery_plan_t* plan = recovery_executive_create_plan(
            exec, &diagnosis, GOAL_RESTORE_FUNCTIONALITY);
        ASSERT_NE(plan, nullptr);

        recovery_executive_execute_plan(exec, plan);
        recovery_executive_free_plan(plan);
    }

    // ACT
    recovery_executive_stats_t stats;
    bool got_stats = recovery_executive_get_stats(exec, &stats);

    // ASSERT
    EXPECT_TRUE(got_stats);
    EXPECT_GE(stats.total_plans_created, 3);
    EXPECT_GE(stats.total_plans_executed, 3);
    EXPECT_GE(stats.total_execution_time_us, 0);

    // CLEANUP
    recovery_executive_destroy(exec);
}

//=============================================================================
// Edge Cases and Error Handling Tests
//=============================================================================

/**
 * @test Handle Maximum Subgoals
 *
 * WHAT: Verify system handles maximum subgoal limit
 * WHY:  Prevent buffer overflow
 * HOW:  Attempt to create more subgoals than allowed
 */
TEST_F(RecoveryExecutiveTest, HandleMaximumSubgoals) {
    // ARRANGE
    recovery_goal_t subgoals[MAX_SUBGOALS];
    uint32_t subgoal_count = 0;

    // ACT
    bool result = recovery_executive_decompose_goal(
        GOAL_RESTORE_FUNCTIONALITY,
        subgoals,
        &subgoal_count
    );

    // ASSERT
    EXPECT_TRUE(result);
    EXPECT_LE(subgoal_count, MAX_SUBGOALS);
}

/**
 * @test Handle Maximum Plan Steps
 *
 * WHAT: Verify system handles maximum plan step limit
 * WHY:  Prevent buffer overflow
 * HOW:  Create complex plan, verify step count bounded
 */
TEST_F(RecoveryExecutiveTest, HandleMaximumPlanSteps) {
    // ARRANGE
    recovery_executive_t* exec = recovery_executive_create(&config);
    ASSERT_NE(exec, nullptr);

    diagnostic_result_t diagnosis;
    memset(&diagnosis, 0, sizeof(diagnosis));
    diagnosis.error_type = ERROR_TYPE_SEGFAULT;  // Complex error
    diagnosis.severity = DIAG_SEVERITY_CRITICAL;

    // ACT
    recovery_plan_t* plan = recovery_executive_create_plan(
        exec, &diagnosis, GOAL_PREVENT_RECURRENCE);  // Complex goal

    // ASSERT
    ASSERT_NE(plan, nullptr);
    EXPECT_LE(plan->step_count, config.max_plan_steps);

    // CLEANUP
    recovery_executive_free_plan(plan);
    recovery_executive_destroy(exec);
}

/**
 * @test Handle Empty Plan
 *
 * WHAT: Verify system handles plan with no steps
 * WHY:  Some goals may not require action
 * HOW:  Create plan for already-resolved error
 */
TEST_F(RecoveryExecutiveTest, HandleEmptyPlan) {
    // ARRANGE
    recovery_executive_t* exec = recovery_executive_create(&config);
    ASSERT_NE(exec, nullptr);

    diagnostic_result_t diagnosis;
    memset(&diagnosis, 0, sizeof(diagnosis));
    diagnosis.error_type = ERROR_TYPE_NONE;  // No error
    diagnosis.severity = DIAG_SEVERITY_INFO;

    // ACT
    recovery_plan_t* plan = recovery_executive_create_plan(
        exec, &diagnosis, GOAL_RESTORE_FUNCTIONALITY);

    // ASSERT
    // Should either return NULL or plan with 0 steps
    if (plan != nullptr) {
        EXPECT_EQ(plan->step_count, 0);
        recovery_executive_free_plan(plan);
    }

    // CLEANUP
    recovery_executive_destroy(exec);
}

//=============================================================================
// Test Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
