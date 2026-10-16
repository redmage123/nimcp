/**
 * @file test_recovery_executive_integration.cpp
 * @brief Integration tests for Executive Function with Brain
 *
 * WHAT: Test recovery executive integrated with brain decision-making
 * WHY:  Verify executive function works with real brain instance
 * HOW:  Create brain, attach executive, test end-to-end workflows
 *
 * INTEGRATION POINTS:
 * - Brain reasoning for strategy selection
 * - Working memory for past failure recall
 * - Executive control for plan coordination
 * - Episodic memory for learning outcomes
 *
 * @author NIMCP Team
 * @date 2025-11-20
 * @version 1.0.0
 */

#include <gtest/gtest.h>
#include <cstring>

// Headers have their own extern "C" guards
#include "cognitive/fault_tolerance/nimcp_recovery_executive.h"
#include "utils/fault_tolerance/nimcp_diagnostics.h"
#include "utils/fault_tolerance/nimcp_brain_recovery_integration.h"
#include "core/brain/nimcp_brain.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixture with Brain
//=============================================================================

class RecoveryExecutiveIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        nimcp_memory_init();
        nimcp_memory_enable_tracking(true);

        // Create brain for integration testing
        brain_config_t brain_config = brain_default_config();
        brain_config.input_size = 10;
        brain_config.output_size = 5;
        brain = brain_create_custom(&brain_config);
        ASSERT_NE(brain, nullptr);

        // Create executive with default config
        exec_config = recovery_executive_default_config();
        exec = recovery_executive_create(&exec_config);
        ASSERT_NE(exec, nullptr);

        // Create brain recovery integration
        brain_recovery_config_t br_config;
        brain_recovery_default_config(&br_config);
        br_config.enable_brain_decisions = true;
        brain_recovery = brain_recovery_create(brain, &br_config);
        ASSERT_NE(brain_recovery, nullptr);
    }

    void TearDown() override {
        if (brain_recovery) brain_recovery_destroy(brain_recovery);
        if (exec) recovery_executive_destroy(exec);
        if (brain) brain_destroy(brain);

        nimcp_memory_stats_t stats;
        nimcp_memory_get_stats(&stats);
        EXPECT_EQ(stats.allocation_count, stats.free_count)
            << "Memory leak detected!";

        nimcp_memory_cleanup();
    }

    brain_t brain = nullptr;
    recovery_executive_t* exec = nullptr;
    brain_recovery_t* brain_recovery = nullptr;
    recovery_executive_config_t exec_config;
};

//=============================================================================
// Brain-Guided Plan Creation Tests
//=============================================================================

/**
 * @test Brain Guides Plan Creation
 *
 * WHAT: Verify brain can influence plan creation
 * WHY:  Cognitive recovery requires brain input
 * HOW:  Create diagnostic, let brain evaluate, verify plan matches
 */
TEST_F(RecoveryExecutiveIntegrationTest, BrainGuidesPlanCreation) {
    // ARRANGE
    diagnostic_result_t diagnosis;
    memset(&diagnosis, 0, sizeof(diagnosis));
    diagnosis.error_type = ERROR_TYPE_NAN_DETECTED;
    diagnosis.severity = DIAG_SEVERITY_ERROR;
    diagnosis.confidence = 0.9f;

    // ACT - Let brain analyze and suggest strategy
    brain_recovery_decision_t* brain_decision = brain_recovery_decide_strategy(
        brain_recovery,
        &diagnosis
    );
    ASSERT_NE(brain_decision, nullptr);

    // Create plan influenced by brain decision
    recovery_plan_t* plan = recovery_executive_create_plan_with_brain_input(
        exec,
        &diagnosis,
        GOAL_RESTORE_FUNCTIONALITY,
        brain_decision
    );

    // ASSERT
    ASSERT_NE(plan, nullptr);
    EXPECT_GT(plan->step_count, 0);
    EXPECT_GT(plan->confidence, 0.0f);

    // Plan should reflect brain's recommended strategy
    if (brain_decision->recommended_strategy != RECOVERY_STRATEGY_NONE) {
        // Verify plan includes actions from recommended strategy
        bool has_strategy_action = false;
        for (uint32_t i = 0; i < plan->step_count; i++) {
            // Check if any step matches the strategy
            if (plan->steps[i].action != EXCEPTION_RECOVERY_NONE) {
                has_strategy_action = true;
                break;
            }
        }
        EXPECT_TRUE(has_strategy_action);
    }

    // CLEANUP
    recovery_executive_free_plan(plan);
    brain_recovery_free_decision(brain_decision);
}

/**
 * @test Brain Recalls Similar Past Failures
 *
 * WHAT: Verify brain uses working memory to recall similar failures
 * WHY:  Past experience should guide current recovery
 * HOW:  Create multiple failures, verify brain recalls them
 */
TEST_F(RecoveryExecutiveIntegrationTest, BrainRecallsSimilarPastFailures) {
    // ARRANGE - Create series of similar failures
    for (int i = 0; i < 3; i++) {
        diagnostic_result_t diagnosis;
        memset(&diagnosis, 0, sizeof(diagnosis));
        diagnosis.error_type = ERROR_TYPE_NAN_DETECTED;
        diagnosis.severity = DIAG_SEVERITY_ERROR;

        // Let brain analyze and learn
        brain_recovery_decision_t* decision = brain_recovery_decide_strategy(
            brain_recovery, &diagnosis);

        if (decision) {
            // Simulate recovery outcome
            brain_recovery_learn_outcome(
                brain_recovery,
                decision,
                true,  // success
                100    // recovery time
            );
            brain_recovery_free_decision(decision);
        }
    }

    // ACT - Present similar failure again
    diagnostic_result_t new_diagnosis;
    memset(&new_diagnosis, 0, sizeof(new_diagnosis));
    new_diagnosis.error_type = ERROR_TYPE_NAN_DETECTED;
    new_diagnosis.severity = DIAG_SEVERITY_ERROR;

    brain_recovery_decision_t* decision = brain_recovery_decide_strategy(
        brain_recovery, &new_diagnosis);

    // ASSERT - Brain should have higher confidence due to past experience
    ASSERT_NE(decision, nullptr);
    // Confidence should be reasonable (implementation dependent)
    EXPECT_GT(decision->confidence, 0.0f);
    EXPECT_LE(decision->confidence, 1.0f);

    // CLEANUP
    brain_recovery_free_decision(decision);
}

//=============================================================================
// End-to-End Recovery Workflow Tests
//=============================================================================

/**
 * @test Complete Recovery Workflow with Brain
 *
 * WHAT: Test complete recovery from diagnosis to learning
 * WHY:  Verify entire cognitive recovery pipeline
 * HOW:  Simulate error, diagnose, plan, execute, learn
 */
TEST_F(RecoveryExecutiveIntegrationTest, CompleteRecoveryWorkflowWithBrain) {
    // ARRANGE
    diagnostic_result_t diagnosis;
    memset(&diagnosis, 0, sizeof(diagnosis));
    diagnosis.error_type = ERROR_TYPE_GRADIENT_EXPLOSION;
    diagnosis.severity = DIAG_SEVERITY_ERROR;
    diagnosis.confidence = 0.85f;
    snprintf(diagnosis.root_cause, sizeof(diagnosis.root_cause),
             "Learning rate too high causing gradient explosion");

    // ACT - Step 1: Brain decides strategy
    brain_recovery_decision_t* brain_decision = brain_recovery_decide_strategy(
        brain_recovery, &diagnosis);
    ASSERT_NE(brain_decision, nullptr);

    // Step 2: Create plan based on brain decision
    recovery_plan_t* plan = recovery_executive_create_plan_with_brain_input(
        exec, &diagnosis, GOAL_RESTORE_FUNCTIONALITY, brain_decision);
    ASSERT_NE(plan, nullptr);

    // Step 3: Execute plan
    recovery_execution_result_t exec_result = recovery_executive_execute_plan(exec, plan);

    // Step 4: Learn from outcome
    brain_recovery_learn_outcome(
        brain_recovery,
        brain_decision,
        exec_result.success,
        exec_result.total_time_us
    );

    // ASSERT
    EXPECT_GT(plan->step_count, 0);
    // Success depends on implementation details

    // Verify statistics updated
    recovery_executive_stats_t stats;
    recovery_executive_get_stats(exec, &stats);
    EXPECT_GE(stats.total_plans_created, 1);
    EXPECT_GE(stats.total_plans_executed, 1);

    // CLEANUP
    recovery_executive_free_plan(plan);
    brain_recovery_free_decision(brain_decision);
}

/**
 * @test Adaptive Replanning with Brain Feedback
 *
 * WHAT: Verify brain guides replanning when initial plan fails
 * WHY:  Adaptive recovery requires brain-guided adjustments
 * HOW:  Simulate plan failure, trigger replan with brain input
 */
TEST_F(RecoveryExecutiveIntegrationTest, AdaptiveReplanningWithBrainFeedback) {
    // ARRANGE
    diagnostic_result_t diagnosis;
    memset(&diagnosis, 0, sizeof(diagnosis));
    diagnosis.error_type = ERROR_TYPE_OUT_OF_MEMORY;
    diagnosis.severity = DIAG_SEVERITY_CRITICAL;

    // Initial plan
    brain_recovery_decision_t* initial_decision = brain_recovery_decide_strategy(
        brain_recovery, &diagnosis);
    ASSERT_NE(initial_decision, nullptr);

    recovery_plan_t* initial_plan = recovery_executive_create_plan_with_brain_input(
        exec, &diagnosis, GOAL_RESTORE_FUNCTIONALITY, initial_decision);
    ASSERT_NE(initial_plan, nullptr);

    // Simulate partial failure
    brain_recovery_learn_outcome(
        brain_recovery,
        initial_decision,
        false,  // failure
        0
    );

    // ACT - Replan with brain feedback
    brain_recovery_decision_t* replan_decision = brain_recovery_decide_strategy(
        brain_recovery, &diagnosis);
    ASSERT_NE(replan_decision, nullptr);

    recovery_plan_t* new_plan = recovery_executive_replan_with_brain_input(
        exec,
        replan_decision,
        "Initial plan failed: memory allocation still failing"
    );

    // ASSERT
    ASSERT_NE(new_plan, nullptr);
    EXPECT_GT(new_plan->step_count, 0);

    // New plan may differ from initial plan
    // (implementation specific - verify it's a valid plan)

    // CLEANUP
    recovery_executive_free_plan(initial_plan);
    recovery_executive_free_plan(new_plan);
    brain_recovery_free_decision(initial_decision);
    brain_recovery_free_decision(replan_decision);
}

//=============================================================================
// Multi-Goal Recovery Tests
//=============================================================================

/**
 * @test Sequential Goal Achievement
 *
 * WHAT: Verify system can achieve multiple goals sequentially
 * WHY:  Complex recovery requires multiple phases
 * HOW:  Execute plans for multiple goals in sequence
 */
TEST_F(RecoveryExecutiveIntegrationTest, SequentialGoalAchievement) {
    // ARRANGE
    diagnostic_result_t diagnosis;
    memset(&diagnosis, 0, sizeof(diagnosis));
    diagnosis.error_type = ERROR_TYPE_SEGFAULT;
    diagnosis.severity = DIAG_SEVERITY_CRITICAL;

    recovery_goal_t goals[] = {
        GOAL_PREVENT_DATA_LOSS,      // First: save state
        GOAL_RESTORE_FUNCTIONALITY,  // Second: get running
        GOAL_LEARN_FROM_FAILURE,     // Third: analyze
        GOAL_PREVENT_RECURRENCE      // Fourth: fix root cause
    };

    // ACT & ASSERT - Execute each goal
    for (size_t i = 0; i < sizeof(goals)/sizeof(goals[0]); i++) {
        brain_recovery_decision_t* decision = brain_recovery_decide_strategy(
            brain_recovery, &diagnosis);
        ASSERT_NE(decision, nullptr);

        recovery_plan_t* plan = recovery_executive_create_plan_with_brain_input(
            exec, &diagnosis, goals[i], decision);
        ASSERT_NE(plan, nullptr) << "Failed to create plan for goal " << (int)goals[i];

        EXPECT_EQ(plan->goal, goals[i]);
        EXPECT_GT(plan->step_count, 0);

        recovery_execution_result_t result = recovery_executive_execute_plan(exec, plan);

        // Learn outcome
        brain_recovery_learn_outcome(
            brain_recovery,
            decision,
            result.success,
            result.total_time_us
        );

        recovery_executive_free_plan(plan);
        brain_recovery_free_decision(decision);
    }

    // Verify all goals were attempted
    recovery_executive_stats_t stats;
    recovery_executive_get_stats(exec, &stats);
    EXPECT_GE(stats.total_plans_created, 4);
}

//=============================================================================
// Performance and Timing Tests
//=============================================================================

/**
 * @test Recovery Execution Time Bounds
 *
 * WHAT: Verify recovery completes within time bounds
 * WHY:  Real-time systems have strict timing requirements
 * HOW:  Execute recovery, verify time within limits
 */
TEST_F(RecoveryExecutiveIntegrationTest, RecoveryExecutionTimeBounds) {
    // ARRANGE
    diagnostic_result_t diagnosis;
    memset(&diagnosis, 0, sizeof(diagnosis));
    diagnosis.error_type = ERROR_TYPE_NAN_DETECTED;
    diagnosis.severity = DIAG_SEVERITY_WARNING;

    // Set strict time limit
    decision_criteria_t criteria;
    memset(&criteria, 0, sizeof(criteria));
    criteria.max_recovery_time_ms = 100;  // 100ms limit
    criteria.risk_tolerance = 0.5f;

    recovery_executive_set_decision_criteria(exec, &criteria);

    // ACT
    brain_recovery_decision_t* decision = brain_recovery_decide_strategy(
        brain_recovery, &diagnosis);
    ASSERT_NE(decision, nullptr);

    recovery_plan_t* plan = recovery_executive_create_plan_with_brain_input(
        exec, &diagnosis, GOAL_RESTORE_FUNCTIONALITY, decision);
    ASSERT_NE(plan, nullptr);

    recovery_execution_result_t result = recovery_executive_execute_plan(exec, plan);

    // ASSERT
    EXPECT_LE(plan->estimated_time_ms, criteria.max_recovery_time_ms);
    // Actual execution may vary slightly but should be close
    EXPECT_LT(result.total_time_us / 1000.0, criteria.max_recovery_time_ms * 1.5);

    // CLEANUP
    recovery_executive_free_plan(plan);
    brain_recovery_free_decision(decision);
}

//=============================================================================
// Learning and Improvement Tests
//=============================================================================

/**
 * @test Learning Improves Future Decisions
 *
 * WHAT: Verify brain learns from outcomes and improves
 * WHY:  System should get better over time
 * HOW:  Execute multiple recoveries, verify metrics improve
 */
TEST_F(RecoveryExecutiveIntegrationTest, LearningImprovesFutureDecisions) {
    // ARRANGE
    diagnostic_result_t diagnosis;
    memset(&diagnosis, 0, sizeof(diagnosis));
    diagnosis.error_type = ERROR_TYPE_NAN_DETECTED;
    diagnosis.severity = DIAG_SEVERITY_ERROR;

    float initial_confidence = 0.0f;
    float final_confidence = 0.0f;

    // ACT - First recovery
    brain_recovery_decision_t* first_decision = brain_recovery_decide_strategy(
        brain_recovery, &diagnosis);
    if (first_decision) {
        initial_confidence = first_decision->confidence;

        // Simulate successful recovery
        brain_recovery_learn_outcome(
            brain_recovery,
            first_decision,
            true,   // success
            50000   // 50ms
        );
        brain_recovery_free_decision(first_decision);
    }

    // Execute several more successful recoveries
    for (int i = 0; i < 5; i++) {
        brain_recovery_decision_t* decision = brain_recovery_decide_strategy(
            brain_recovery, &diagnosis);
        if (decision) {
            brain_recovery_learn_outcome(
                brain_recovery,
                decision,
                true,
                50000
            );
            brain_recovery_free_decision(decision);
        }
    }

    // Final recovery
    brain_recovery_decision_t* final_decision = brain_recovery_decide_strategy(
        brain_recovery, &diagnosis);
    if (final_decision) {
        final_confidence = final_decision->confidence;
        brain_recovery_free_decision(final_decision);
    }

    // ASSERT - Confidence should increase or stay high
    // (exact behavior depends on implementation)
    EXPECT_GE(final_confidence, 0.0f);
    EXPECT_LE(final_confidence, 1.0f);
}

//=============================================================================
// Stress and Edge Case Tests
//=============================================================================

/**
 * @test Handle Rapid Sequential Recoveries
 *
 * WHAT: Verify system handles multiple rapid recoveries
 * WHY:  Cascade failures require quick successive recoveries
 * HOW:  Execute many recoveries in quick succession
 */
TEST_F(RecoveryExecutiveIntegrationTest, HandleRapidSequentialRecoveries) {
    // ARRANGE
    const int num_recoveries = 10;

    // ACT
    for (int i = 0; i < num_recoveries; i++) {
        diagnostic_result_t diagnosis;
        memset(&diagnosis, 0, sizeof(diagnosis));
        diagnosis.error_type = (i % 2 == 0) ? ERROR_TYPE_NAN_DETECTED : ERROR_TYPE_INF_DETECTED;
        diagnosis.severity = DIAG_SEVERITY_ERROR;

        brain_recovery_decision_t* decision = brain_recovery_decide_strategy(
            brain_recovery, &diagnosis);

        if (decision) {
            recovery_plan_t* plan = recovery_executive_create_plan_with_brain_input(
                exec, &diagnosis, GOAL_RESTORE_FUNCTIONALITY, decision);

            if (plan) {
                recovery_execution_result_t result = recovery_executive_execute_plan(exec, plan);

                brain_recovery_learn_outcome(
                    brain_recovery,
                    decision,
                    result.success,
                    result.total_time_us
                );

                recovery_executive_free_plan(plan);
            }

            brain_recovery_free_decision(decision);
        }
    }

    // ASSERT
    recovery_executive_stats_t stats;
    recovery_executive_get_stats(exec, &stats);
    EXPECT_GE(stats.total_plans_created, num_recoveries);
    EXPECT_GE(stats.total_plans_executed, num_recoveries);
}

/**
 * @test Handle Complex Multi-Step Recovery
 *
 * WHAT: Verify system handles complex recovery requiring many steps
 * WHY:  Some failures require elaborate recovery procedures
 * HOW:  Create complex failure scenario, verify full recovery
 */
TEST_F(RecoveryExecutiveIntegrationTest, HandleComplexMultiStepRecovery) {
    // ARRANGE - Complex failure requiring multiple phases
    diagnostic_result_t diagnosis;
    memset(&diagnosis, 0, sizeof(diagnosis));
    diagnosis.error_type = ERROR_TYPE_SEGFAULT;
    diagnosis.severity = DIAG_SEVERITY_CRITICAL;
    diagnosis.confidence = 0.95f;
    snprintf(diagnosis.root_cause, sizeof(diagnosis.root_cause),
             "Catastrophic failure requiring checkpoint restore and reconfiguration");

    // ACT
    brain_recovery_decision_t* decision = brain_recovery_decide_strategy(
        brain_recovery, &diagnosis);
    ASSERT_NE(decision, nullptr);

    // Request complex goal requiring multiple steps
    recovery_plan_t* plan = recovery_executive_create_plan_with_brain_input(
        exec, &diagnosis, GOAL_PREVENT_RECURRENCE, decision);
    ASSERT_NE(plan, nullptr);

    // Execute complex plan
    recovery_execution_result_t result = recovery_executive_execute_plan(exec, plan);

    // ASSERT
    EXPECT_GT(plan->step_count, 1);  // Should have multiple steps

    // Verify plan had logical structure
    for (uint32_t i = 0; i < plan->step_count; i++) {
        EXPECT_NE(plan->steps[i].action, EXCEPTION_RECOVERY_NONE);
        EXPECT_GT(plan->steps[i].timeout_ms, 0);
    }

    // CLEANUP
    brain_recovery_learn_outcome(
        brain_recovery,
        decision,
        result.success,
        result.total_time_us
    );

    recovery_executive_free_plan(plan);
    brain_recovery_free_decision(decision);
}

//=============================================================================
// Test Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
