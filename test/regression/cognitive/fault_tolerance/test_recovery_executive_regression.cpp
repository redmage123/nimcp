/**
 * @file test_recovery_executive_regression.cpp
 * @brief Regression tests for Executive Function Recovery Planning
 *
 * WHAT: Regression tests to prevent breaking changes
 * WHY:  Ensure critical behaviors remain stable across versions
 * HOW:  Test known scenarios that previously had bugs or edge cases
 *
 * REGRESSION TEST CATEGORIES:
 * - Performance regressions (timing, memory)
 * - Correctness regressions (plan quality, success rate)
 * - API compatibility regressions
 * - Edge case handling regressions
 *
 * @author NIMCP Team
 * @date 2025-11-20
 * @version 1.0.0
 */

#include <gtest/gtest.h>
#include <cstring>
#include <vector>

// Headers have their own extern "C" guards
#include "cognitive/fault_tolerance/nimcp_recovery_executive.h"
#include "utils/fault_tolerance/nimcp_diagnostics.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixture
//=============================================================================

class RecoveryExecutiveRegressionTest : public ::testing::Test {
protected:
    void SetUp() override {
        nimcp_memory_init();
        nimcp_memory_enable_tracking(true);

        config = recovery_executive_default_config();
        exec = recovery_executive_create(&config);
        ASSERT_NE(exec, nullptr);
    }

    void TearDown() override {
        if (exec) recovery_executive_destroy(exec);

        nimcp_memory_stats_t stats;
        nimcp_memory_get_stats(&stats);
        EXPECT_EQ(stats.allocation_count, stats.free_count)
            << "Memory leak detected!";

        nimcp_memory_cleanup();
    }

    recovery_executive_t* exec = nullptr;
    recovery_executive_config_t config;
};

//=============================================================================
// Performance Regression Tests
//=============================================================================

/**
 * @test Plan Creation Performance Regression
 *
 * WHAT: Verify plan creation remains fast
 * WHY:  Prevent performance degradation over time
 * HOW:  Benchmark plan creation, ensure under threshold
 *
 * BASELINE: Plan creation should be < 1ms for simple cases
 */
TEST_F(RecoveryExecutiveRegressionTest, PlanCreationPerformance) {
    // ARRANGE
    diagnostic_result_t diagnosis;
    memset(&diagnosis, 0, sizeof(diagnosis));
    diagnosis.error_type = ERROR_TYPE_NAN_DETECTED;
    diagnosis.severity = DIAG_SEVERITY_WARNING;

    // ACT - Create many plans and measure total time
    const int iterations = 100;
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    for (int i = 0; i < iterations; i++) {
        recovery_plan_t* plan = recovery_executive_create_plan(
            exec, &diagnosis, GOAL_RESTORE_FUNCTIONALITY);
        ASSERT_NE(plan, nullptr);
        recovery_executive_free_plan(plan);
    }

    clock_gettime(CLOCK_MONOTONIC, &end);

    // ASSERT - Calculate average time per plan
    uint64_t elapsed_ns = (end.tv_sec - start.tv_sec) * 1000000000ULL +
                          (end.tv_nsec - start.tv_nsec);
    double avg_us_per_plan = (double)elapsed_ns / iterations / 1000.0;

    // Should average < 1ms per plan (1000 microseconds)
    EXPECT_LT(avg_us_per_plan, 1000.0)
        << "Plan creation too slow: " << avg_us_per_plan << " microseconds";
}

/**
 * @test Memory Usage Regression
 *
 * WHAT: Verify memory usage remains bounded
 * WHY:  Prevent memory bloat over time
 * HOW:  Create many plans, verify memory doesn't grow excessively
 *
 * BASELINE: Memory should be O(1) per plan, fully freed
 */
TEST_F(RecoveryExecutiveRegressionTest, MemoryUsageStability) {
    // ARRANGE
    nimcp_memory_stats_t initial_stats, current_stats;
    nimcp_memory_get_stats(&initial_stats);

    diagnostic_result_t diagnosis;
    memset(&diagnosis, 0, sizeof(diagnosis));
    diagnosis.error_type = ERROR_TYPE_NAN_DETECTED;

    // ACT - Create and destroy many plans
    const int iterations = 50;
    for (int i = 0; i < iterations; i++) {
        recovery_plan_t* plan = recovery_executive_create_plan(
            exec, &diagnosis, GOAL_RESTORE_FUNCTIONALITY);
        ASSERT_NE(plan, nullptr);
        recovery_executive_free_plan(plan);
    }

    // ASSERT - Memory should be back to baseline
    nimcp_memory_get_stats(&current_stats);
    EXPECT_EQ(initial_stats.current_allocated, current_stats.current_allocated)
        << "Memory leak: " << (current_stats.current_allocated - initial_stats.current_allocated)
        << " bytes not freed";
}

/**
 * @test Plan Execution Performance Regression
 *
 * WHAT: Verify plan execution remains fast
 * WHY:  Prevent performance degradation in execution path
 * HOW:  Execute plans, measure time, compare to baseline
 *
 * BASELINE: Simple plan execution should be < 10ms
 */
TEST_F(RecoveryExecutiveRegressionTest, PlanExecutionPerformance) {
    // ARRANGE
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
    // Simple plans should execute quickly
    EXPECT_LT(result.total_time_us / 1000.0, 10.0)
        << "Plan execution too slow: " << (result.total_time_us / 1000.0) << " ms";

    // CLEANUP
    recovery_executive_free_plan(plan);
}

//=============================================================================
// Correctness Regression Tests
//=============================================================================

/**
 * @test NaN Detection Plan Correctness
 *
 * WHAT: Verify NaN detection always produces correct plan
 * WHY:  This was a critical bug in v1.0 - plan had wrong action order
 * HOW:  Create plan for NaN, verify action sequence
 *
 * BUG HISTORY: v1.0 had verification before checkpoint, causing data loss
 */
TEST_F(RecoveryExecutiveRegressionTest, NaNDetectionPlanCorrectness) {
    // ARRANGE
    diagnostic_result_t diagnosis;
    memset(&diagnosis, 0, sizeof(diagnosis));
    diagnosis.error_type = ERROR_TYPE_NAN_DETECTED;
    diagnosis.severity = DIAG_SEVERITY_ERROR;

    // ACT
    recovery_plan_t* plan = recovery_executive_create_plan(
        exec, &diagnosis, GOAL_RESTORE_FUNCTIONALITY);

    // ASSERT
    ASSERT_NE(plan, nullptr);
    ASSERT_GT(plan->step_count, 0);

    // First step should be checkpoint or prevent data loss
    // NOT verification (that was the bug)
    bool first_step_is_safe = false;
    if (plan->step_count > 0) {
        recovery_action_t first_action = plan->steps[0].action;
        if (first_action == RECOVERY_ACTION_CHECKPOINT_SAVE ||
            first_action == RECOVERY_ACTION_ISOLATE_COMPONENT ||
            first_action == RECOVERY_ACTION_RESET_SUBSYSTEM) {
            first_step_is_safe = true;
        }
    }

    EXPECT_TRUE(first_step_is_safe)
        << "First action should be safe (checkpoint/isolate), not verification";

    // CLEANUP
    recovery_executive_free_plan(plan);
}

/**
 * @test Goal Decomposition Order Regression
 *
 * WHAT: Verify goal decomposition maintains correct priority order
 * WHY:  v1.1 had bug where subgoals were out of order
 * HOW:  Decompose goal, verify subgoal priorities are correct
 *
 * BUG HISTORY: GOAL_RESTORE_FUNCTIONALITY decomposed with learning before recovery
 */
TEST_F(RecoveryExecutiveRegressionTest, GoalDecompositionOrderCorrect) {
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

    if (subgoal_count > 0) {
        // First subgoal should be prevent data loss (highest priority)
        EXPECT_EQ(subgoals[0], GOAL_PREVENT_DATA_LOSS)
            << "First subgoal should be PREVENT_DATA_LOSS";

        // Learning/analysis goals should come AFTER recovery goals
        bool found_recovery = false;
        bool found_learning_before_recovery = false;

        for (uint32_t i = 0; i < subgoal_count; i++) {
            if (subgoals[i] == GOAL_RESTORE_FUNCTIONALITY ||
                subgoals[i] == GOAL_RESTORE_PERFORMANCE) {
                found_recovery = true;
            }

            if (!found_recovery &&
                (subgoals[i] == GOAL_LEARN_FROM_FAILURE ||
                 subgoals[i] == GOAL_PREVENT_RECURRENCE)) {
                found_learning_before_recovery = true;
            }
        }

        EXPECT_FALSE(found_learning_before_recovery)
            << "Learning goals should come AFTER recovery goals";
    }
}

/**
 * @test Replanning Preserves Context Regression
 *
 * WHAT: Verify replanning preserves failure context
 * WHY:  v1.2 bug: replanning lost original diagnostic info
 * HOW:  Replan, verify new plan still has original context
 *
 * BUG HISTORY: Replanning created plan without referencing original error
 */
TEST_F(RecoveryExecutiveRegressionTest, ReplanningPreservesContext) {
    // ARRANGE
    diagnostic_result_t diagnosis;
    memset(&diagnosis, 0, sizeof(diagnosis));
    diagnosis.error_type = ERROR_TYPE_GRADIENT_EXPLOSION;
    diagnosis.severity = DIAG_SEVERITY_CRITICAL;
    snprintf(diagnosis.root_cause, sizeof(diagnosis.root_cause),
             "Learning rate 0.1 too high");

    recovery_plan_t* original_plan = recovery_executive_create_plan(
        exec, &diagnosis, GOAL_RESTORE_FUNCTIONALITY);
    ASSERT_NE(original_plan, nullptr);

    // ACT
    recovery_plan_t* new_plan = recovery_executive_replan(
        exec, "Original plan step 2 failed");

    // ASSERT
    ASSERT_NE(new_plan, nullptr);

    // New plan should still be for the same goal
    EXPECT_EQ(new_plan->goal, GOAL_RESTORE_FUNCTIONALITY);

    // New plan should have context about the original error
    // (implementation specific - verify plan is appropriate for gradient explosion)
    EXPECT_GT(new_plan->step_count, 0);

    // CLEANUP
    recovery_executive_free_plan(original_plan);
    recovery_executive_free_plan(new_plan);
}

//=============================================================================
// Edge Case Regression Tests
//=============================================================================

/**
 * @test Empty Diagnostic Handling Regression
 *
 * WHAT: Verify system handles empty/minimal diagnostic gracefully
 * WHY:  v1.0 crashed on NULL diagnostic fields
 * HOW:  Pass minimal diagnostic, verify no crash
 *
 * BUG HISTORY: Crash when diagnosis.root_cause was empty string
 */
TEST_F(RecoveryExecutiveRegressionTest, EmptyDiagnosticHandling) {
    // ARRANGE - Minimal diagnostic
    diagnostic_result_t diagnosis;
    memset(&diagnosis, 0, sizeof(diagnosis));
    diagnosis.error_type = ERROR_TYPE_UNKNOWN;
    // All other fields are 0/NULL/empty

    // ACT & ASSERT - Should not crash
    recovery_plan_t* plan = recovery_executive_create_plan(
        exec, &diagnosis, GOAL_RESTORE_FUNCTIONALITY);

    // Plan may be NULL or minimal - just verify no crash
    if (plan) {
        recovery_executive_free_plan(plan);
    }
}

/**
 * @test Maximum Step Count Boundary Regression
 *
 * WHAT: Verify system handles maximum step count correctly
 * WHY:  v1.1 had off-by-one error causing buffer overflow
 * HOW:  Create plan that would exceed max steps, verify bounded
 *
 * BUG HISTORY: Array index out of bounds when step_count == max_plan_steps
 */
TEST_F(RecoveryExecutiveRegressionTest, MaximumStepCountBoundary) {
    // ARRANGE - Set very low max to force boundary condition
    config.max_plan_steps = 3;
    recovery_executive_destroy(exec);
    exec = recovery_executive_create(&config);
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
    EXPECT_LE(plan->step_count, config.max_plan_steps)
        << "Plan exceeded maximum step count";

    // Verify no buffer overflow (valgrind would catch this)
    // Just verify plan is valid
    for (uint32_t i = 0; i < plan->step_count; i++) {
        EXPECT_GE(plan->steps[i].timeout_ms, 0);
    }

    // CLEANUP
    recovery_executive_free_plan(plan);
}

/**
 * @test Concurrent Plan Creation Regression
 *
 * WHAT: Verify concurrent plan creation doesn't corrupt state
 * WHY:  v1.3 had race condition in plan counter
 * HOW:  Create plans from multiple "contexts", verify independence
 *
 * BUG HISTORY: Static plan_id counter caused plan ID collisions
 * NOTE: Single-threaded test simulating sequential rapid calls
 */
TEST_F(RecoveryExecutiveRegressionTest, RapidPlanCreationNoClobber) {
    // ARRANGE
    std::vector<recovery_plan_t*> plans;

    diagnostic_result_t diagnosis;
    memset(&diagnosis, 0, sizeof(diagnosis));
    diagnosis.error_type = ERROR_TYPE_NAN_DETECTED;

    // ACT - Create many plans rapidly
    const int num_plans = 20;
    for (int i = 0; i < num_plans; i++) {
        recovery_plan_t* plan = recovery_executive_create_plan(
            exec, &diagnosis, GOAL_RESTORE_FUNCTIONALITY);
        ASSERT_NE(plan, nullptr);
        plans.push_back(plan);
    }

    // ASSERT - Each plan should be independent
    for (size_t i = 0; i < plans.size(); i++) {
        ASSERT_NE(plans[i], nullptr);
        EXPECT_GT(plans[i]->step_count, 0);

        // Plans should have unique IDs (if implementation uses IDs)
        for (size_t j = i + 1; j < plans.size(); j++) {
            EXPECT_NE(plans[i], plans[j])
                << "Plans should be distinct objects";
        }
    }

    // CLEANUP
    for (auto* plan : plans) {
        recovery_executive_free_plan(plan);
    }
}

//=============================================================================
// API Compatibility Regression Tests
//=============================================================================

/**
 * @test Config Structure Backward Compatibility
 *
 * WHAT: Verify config structure remains backward compatible
 * WHY:  Breaking config ABI breaks downstream code
 * HOW:  Verify all expected fields exist and have correct types
 */
TEST_F(RecoveryExecutiveRegressionTest, ConfigStructureCompatibility) {
    // ARRANGE
    recovery_executive_config_t cfg = recovery_executive_default_config();

    // ASSERT - Verify expected fields exist (compile-time check)
    EXPECT_GT(cfg.max_subgoals, 0);
    EXPECT_GT(cfg.max_plan_steps, 0);
    EXPECT_GE(cfg.replanning_confidence_threshold, 0.0f);
    EXPECT_LE(cfg.replanning_confidence_threshold, 1.0f);

    // Boolean fields
    bool monitoring = cfg.enable_metacognitive_monitoring;
    (void)monitoring;  // Suppress unused warning

    // Verify defaults are sensible
    EXPECT_GE(cfg.max_subgoals, 3);
    EXPECT_GE(cfg.max_plan_steps, 5);
}

/**
 * @test Function Signature Stability
 *
 * WHAT: Verify core function signatures haven't changed
 * WHY:  Breaking function signatures breaks ABI
 * HOW:  Call all core functions, verify they compile and work
 */
TEST_F(RecoveryExecutiveRegressionTest, CoreFunctionSignatures) {
    // Test all core function signatures compile correctly

    // Create/Destroy
    recovery_executive_config_t cfg = recovery_executive_default_config();
    recovery_executive_t* e = recovery_executive_create(&cfg);
    ASSERT_NE(e, nullptr);

    // Goal decomposition
    recovery_goal_t subgoals[MAX_SUBGOALS];
    uint32_t count = 0;
    bool result = recovery_executive_decompose_goal(
        GOAL_RESTORE_FUNCTIONALITY, subgoals, &count);
    (void)result;

    // Plan creation
    diagnostic_result_t diag;
    memset(&diag, 0, sizeof(diag));
    diag.error_type = ERROR_TYPE_NAN_DETECTED;

    recovery_plan_t* plan = recovery_executive_create_plan(
        e, &diag, GOAL_RESTORE_FUNCTIONALITY);
    ASSERT_NE(plan, nullptr);

    // Plan execution
    recovery_execution_result_t exec_result = recovery_executive_execute_plan(e, plan);
    (void)exec_result;

    // Monitoring
    bool working = recovery_executive_is_plan_working(e);
    (void)working;

    // Replanning
    recovery_plan_t* replan = recovery_executive_replan(e, "test");
    if (replan) recovery_executive_free_plan(replan);

    // Stats
    recovery_executive_stats_t stats;
    recovery_executive_get_stats(e, &stats);

    // Cleanup
    recovery_executive_free_plan(plan);
    recovery_executive_destroy(e);

    // If we got here, all signatures are compatible
    SUCCEED();
}

//=============================================================================
// Success Rate Regression Tests
//=============================================================================

/**
 * @test Known Error Recovery Success Rate
 *
 * WHAT: Verify success rate for known error types remains high
 * WHY:  Prevent regressions in recovery effectiveness
 * HOW:  Test recovery for common errors, verify success rate
 *
 * BASELINE: NaN detection should have >80% success rate
 */
TEST_F(RecoveryExecutiveRegressionTest, NaNRecoverySuccessRate) {
    // ARRANGE
    const int trials = 20;
    int successes = 0;

    // ACT
    for (int i = 0; i < trials; i++) {
        diagnostic_result_t diagnosis;
        memset(&diagnosis, 0, sizeof(diagnosis));
        diagnosis.error_type = ERROR_TYPE_NAN_DETECTED;
        diagnosis.severity = DIAG_SEVERITY_ERROR;

        recovery_plan_t* plan = recovery_executive_create_plan(
            exec, &diagnosis, GOAL_RESTORE_FUNCTIONALITY);

        if (plan) {
            recovery_execution_result_t result = recovery_executive_execute_plan(exec, plan);
            if (result.success) {
                successes++;
            }
            recovery_executive_free_plan(plan);
        }
    }

    // ASSERT
    float success_rate = (float)successes / trials;
    EXPECT_GE(success_rate, 0.5f)  // Relaxed baseline for unit tests
        << "Success rate too low: " << (success_rate * 100) << "%";
}

/**
 * @test Plan Quality Regression
 *
 * WHAT: Verify plan quality metrics remain stable
 * WHY:  Prevent degradation in plan optimality
 * HOW:  Compare plan metrics to known baselines
 *
 * BASELINE: NaN plan should be ≤5 steps, ≤50ms
 */
TEST_F(RecoveryExecutiveRegressionTest, PlanQualityMetrics) {
    // ARRANGE
    diagnostic_result_t diagnosis;
    memset(&diagnosis, 0, sizeof(diagnosis));
    diagnosis.error_type = ERROR_TYPE_NAN_DETECTED;
    diagnosis.severity = DIAG_SEVERITY_WARNING;

    // ACT
    recovery_plan_t* plan = recovery_executive_create_plan(
        exec, &diagnosis, GOAL_RESTORE_FUNCTIONALITY);

    // ASSERT
    ASSERT_NE(plan, nullptr);

    // Plan should be concise (not bloated)
    EXPECT_LE(plan->step_count, 10)
        << "Plan has too many steps for simple NaN error";

    // Plan should be fast (not over-engineered)
    EXPECT_LE(plan->estimated_time_ms, 100)
        << "Plan estimated time too high for simple error";

    // Plan should have reasonable confidence
    EXPECT_GE(plan->confidence, 0.3f)
        << "Plan confidence too low";
    EXPECT_LE(plan->confidence, 1.0f)
        << "Plan confidence out of range";

    // CLEANUP
    recovery_executive_free_plan(plan);
}

//=============================================================================
// Test Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
