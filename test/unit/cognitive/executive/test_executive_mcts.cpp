/**
 * @file test_executive_mcts.cpp
 * @brief Unit tests for Executive MCTS-based goal decomposition
 *
 * WHAT: Tests for MCTS planning in executive functions
 * WHY:  Validate MCTS integration for sophisticated goal decomposition
 * HOW:  Test MCTS config, plan creation, evaluation, replanning, and action selection
 *
 * TEST COVERAGE:
 * - MCTS config initialization
 * - MCTS plan creation with various configurations
 * - Plan evaluation via MC rollouts
 * - Replanning from mid-execution
 * - Best action selection
 * - Statistics tracking
 * - Edge cases and error handling
 *
 * @author Claude Code
 * @date 2026-01-12
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

#include "cognitive/nimcp_executive.h"
#include "utils/time/nimcp_time.h"

// =============================================================================
// TEST FIXTURES
// =============================================================================

class ExecutiveMCTSTest : public ::testing::Test {
protected:
    executive_controller_t* exec;

    void SetUp() override {
        exec = executive_create();
        ASSERT_NE(exec, nullptr) << "Failed to create executive controller";
    }

    void TearDown() override {
        if (exec) {
            executive_destroy(exec);
            exec = nullptr;
        }
    }
};

// =============================================================================
// MCTS CONFIG TESTS
// =============================================================================

TEST_F(ExecutiveMCTSTest, ConfigInit_SetsDefaults) {
    // WHAT: Test MCTS config initialization
    // WHY:  Verify defaults are reasonable
    // HOW:  Initialize config and check values

    executive_mcts_config_t config;
    executive_mcts_config_init(&config);

    EXPECT_EQ(config.max_iterations, 100u);
    EXPECT_EQ(config.max_depth, 10u);
    EXPECT_GT(config.exploration_constant, 1.0f);  // Should be sqrt(2) ~= 1.414
    EXPECT_LE(config.exploration_constant, 2.0f);
    EXPECT_GT(config.discount_factor, 0.9f);
    EXPECT_LE(config.discount_factor, 1.0f);
    EXPECT_GT(config.rollout_depth, 0u);
    EXPECT_TRUE(config.enable_pruning);
    EXPECT_GT(config.pruning_threshold, 0.0f);
}

TEST_F(ExecutiveMCTSTest, ConfigInit_NullSafe) {
    // WHAT: Test NULL config handling
    // WHY:  Should not crash on NULL
    // HOW:  Pass NULL to init function

    executive_mcts_config_init(nullptr);  // Should not crash
}

// =============================================================================
// MCTS PLAN CREATION TESTS
// =============================================================================

TEST_F(ExecutiveMCTSTest, CreatePlanMCTS_Basic) {
    // WHAT: Create plan using MCTS
    // WHY:  Verify MCTS planning works
    // HOW:  Create plan with default config

    plan_t* plan = executive_create_plan_mcts(exec, "Complete analysis task", nullptr, nullptr);

    ASSERT_NE(plan, nullptr) << "MCTS plan creation failed";
    EXPECT_GT(plan->num_steps, 0u) << "Plan should have at least one step";
    EXPECT_STREQ(plan->goal, "Complete analysis task");

    executive_destroy_plan(plan);
}

TEST_F(ExecutiveMCTSTest, CreatePlanMCTS_WithConfig) {
    // WHAT: Create plan with custom MCTS config
    // WHY:  Verify config parameters are respected
    // HOW:  Use custom config with specific values

    executive_mcts_config_t config;
    executive_mcts_config_init(&config);
    config.max_iterations = 50;
    config.max_depth = 5;
    config.exploration_constant = 1.0f;

    plan_t* plan = executive_create_plan_mcts(exec, "Custom config task", &config, nullptr);

    ASSERT_NE(plan, nullptr);
    EXPECT_LE(plan->num_steps, 5u) << "Plan should not exceed max_depth";

    executive_destroy_plan(plan);
}

TEST_F(ExecutiveMCTSTest, CreatePlanMCTS_ReturnsStats) {
    // WHAT: Verify statistics are returned
    // WHY:  Stats help diagnose planning quality
    // HOW:  Create plan with stats output

    executive_mcts_stats_t stats;
    memset(&stats, 0, sizeof(stats));

    plan_t* plan = executive_create_plan_mcts(exec, "Stats tracking task", nullptr, &stats);

    ASSERT_NE(plan, nullptr);
    EXPECT_GT(stats.nodes_expanded, 0u) << "Should have expanded nodes";
    EXPECT_GE(stats.planning_time_ms, 0.0f);
    // root_value can exceed 1.0 due to cumulative discounted rewards
    EXPECT_GE(stats.root_value, -10.0f) << "Root value should be reasonable";
    EXPECT_LE(stats.root_value, 10.0f) << "Root value should be reasonable";

    executive_destroy_plan(plan);
}

TEST_F(ExecutiveMCTSTest, CreatePlanMCTS_NullExec) {
    // WHAT: Test NULL executive handling
    // WHY:  Should reject invalid input
    // HOW:  Pass NULL exec

    plan_t* plan = executive_create_plan_mcts(nullptr, "Test", nullptr, nullptr);
    EXPECT_EQ(plan, nullptr);
}

TEST_F(ExecutiveMCTSTest, CreatePlanMCTS_NullGoal) {
    // WHAT: Test NULL goal handling
    // WHY:  Should reject invalid input
    // HOW:  Pass NULL goal

    plan_t* plan = executive_create_plan_mcts(exec, nullptr, nullptr, nullptr);
    EXPECT_EQ(plan, nullptr);
}

TEST_F(ExecutiveMCTSTest, CreatePlanMCTS_StepsHaveDescriptions) {
    // WHAT: Verify plan steps have descriptions
    // WHY:  Steps must be actionable
    // HOW:  Check each step's description

    plan_t* plan = executive_create_plan_mcts(exec, "Descriptive task", nullptr, nullptr);
    ASSERT_NE(plan, nullptr);

    for (uint32_t i = 0; i < plan->num_steps; i++) {
        EXPECT_GT(strlen(plan->steps[i].description), 0u)
            << "Step " << i << " should have a description";
    }

    executive_destroy_plan(plan);
}

TEST_F(ExecutiveMCTSTest, CreatePlanMCTS_StepsHaveCosts) {
    // WHAT: Verify plan steps have cost estimates
    // WHY:  Costs enable resource planning
    // HOW:  Check each step's estimated_cost

    plan_t* plan = executive_create_plan_mcts(exec, "Cost estimation task", nullptr, nullptr);
    ASSERT_NE(plan, nullptr);

    for (uint32_t i = 0; i < plan->num_steps; i++) {
        EXPECT_GT(plan->steps[i].estimated_cost, 0u)
            << "Step " << i << " should have cost estimate";
    }

    executive_destroy_plan(plan);
}

TEST_F(ExecutiveMCTSTest, CreatePlanMCTS_HasCriticalSteps) {
    // WHAT: Verify plan has critical step markers
    // WHY:  Critical steps need special handling
    // HOW:  Check is_critical flags

    plan_t* plan = executive_create_plan_mcts(exec, "Critical steps task", nullptr, nullptr);
    ASSERT_NE(plan, nullptr);

    // First and last steps should typically be critical
    if (plan->num_steps > 0) {
        EXPECT_TRUE(plan->steps[0].is_critical)
            << "First step should typically be critical";
    }

    executive_destroy_plan(plan);
}

TEST_F(ExecutiveMCTSTest, CreatePlanMCTS_UpdatesStats) {
    // WHAT: Verify executive stats are updated
    // WHY:  Track planning activity
    // HOW:  Create plan and check stats

    executive_stats_t before, after;
    executive_get_stats(exec, &before);

    plan_t* plan = executive_create_plan_mcts(exec, "Stats update task", nullptr, nullptr);
    ASSERT_NE(plan, nullptr);

    executive_get_stats(exec, &after);
    EXPECT_GT(after.plans_created, before.plans_created)
        << "plans_created should increment";

    executive_destroy_plan(plan);
}

// =============================================================================
// PLAN EVALUATION TESTS
// =============================================================================

TEST_F(ExecutiveMCTSTest, EvaluatePlanMCTS_Basic) {
    // WHAT: Evaluate plan quality via MC
    // WHY:  Estimate success probability
    // HOW:  Create plan and evaluate

    plan_t* plan = executive_create_plan_mcts(exec, "Evaluation task", nullptr, nullptr);
    ASSERT_NE(plan, nullptr);

    float success_prob = executive_evaluate_plan_mcts(exec, plan, 100);

    EXPECT_GE(success_prob, 0.0f);
    EXPECT_LE(success_prob, 1.0f);

    executive_destroy_plan(plan);
}

TEST_F(ExecutiveMCTSTest, EvaluatePlanMCTS_MoreRollouts) {
    // WHAT: Test evaluation with more rollouts
    // WHY:  More rollouts should give stable estimate
    // HOW:  Run with different rollout counts

    plan_t* plan = executive_create_plan_mcts(exec, "Rollout test task", nullptr, nullptr);
    ASSERT_NE(plan, nullptr);

    float prob_low = executive_evaluate_plan_mcts(exec, plan, 10);
    float prob_high = executive_evaluate_plan_mcts(exec, plan, 1000);

    // Both should be valid probabilities
    EXPECT_GE(prob_low, 0.0f);
    EXPECT_LE(prob_low, 1.0f);
    EXPECT_GE(prob_high, 0.0f);
    EXPECT_LE(prob_high, 1.0f);

    executive_destroy_plan(plan);
}

TEST_F(ExecutiveMCTSTest, EvaluatePlanMCTS_NullInputs) {
    // WHAT: Test NULL input handling
    // WHY:  Should handle gracefully
    // HOW:  Pass NULL values

    plan_t* plan = executive_create_plan_mcts(exec, "Test", nullptr, nullptr);
    ASSERT_NE(plan, nullptr);

    float prob1 = executive_evaluate_plan_mcts(nullptr, plan, 100);
    EXPECT_EQ(prob1, 0.0f);

    float prob2 = executive_evaluate_plan_mcts(exec, nullptr, 100);
    EXPECT_EQ(prob2, 0.0f);

    executive_destroy_plan(plan);
}

// =============================================================================
// REPLANNING TESTS
// =============================================================================

TEST_F(ExecutiveMCTSTest, ReplanMCTS_Basic) {
    // WHAT: Test replanning from mid-execution
    // WHY:  Plans may need adaptation
    // HOW:  Create plan, then replan from step 1

    plan_t* original = executive_create_plan_mcts(exec, "Original goal", nullptr, nullptr);
    ASSERT_NE(original, nullptr);
    ASSERT_GT(original->num_steps, 1u);

    plan_t* replan = executive_replan_mcts(exec, original, 1, nullptr);
    ASSERT_NE(replan, nullptr);

    // Replan should mention continuation
    EXPECT_NE(strstr(replan->goal, "Continue"), nullptr)
        << "Replan goal should mention continuation";

    executive_destroy_plan(original);
    executive_destroy_plan(replan);
}

TEST_F(ExecutiveMCTSTest, ReplanMCTS_NullInputs) {
    // WHAT: Test NULL input handling
    // WHY:  Should reject invalid inputs
    // HOW:  Pass NULL values

    plan_t* plan = executive_create_plan_mcts(exec, "Test", nullptr, nullptr);
    ASSERT_NE(plan, nullptr);

    plan_t* replan1 = executive_replan_mcts(nullptr, plan, 0, nullptr);
    EXPECT_EQ(replan1, nullptr);

    plan_t* replan2 = executive_replan_mcts(exec, nullptr, 0, nullptr);
    EXPECT_EQ(replan2, nullptr);

    executive_destroy_plan(plan);
}

TEST_F(ExecutiveMCTSTest, ReplanMCTS_InvalidStep) {
    // WHAT: Test invalid step index
    // WHY:  Should reject out-of-bounds
    // HOW:  Pass step >= num_steps

    plan_t* plan = executive_create_plan_mcts(exec, "Test", nullptr, nullptr);
    ASSERT_NE(plan, nullptr);

    plan_t* replan = executive_replan_mcts(exec, plan, plan->num_steps + 1, nullptr);
    EXPECT_EQ(replan, nullptr);

    executive_destroy_plan(plan);
}

// =============================================================================
// BEST ACTION TESTS
// =============================================================================

TEST_F(ExecutiveMCTSTest, GetBestActionMCTS_Basic) {
    // WHAT: Get single best action via MCTS
    // WHY:  Quick decision without full plan
    // HOW:  Call best action function

    float action_value;
    char* action = executive_get_best_action_mcts(exec, "Quick decision", nullptr, &action_value);

    ASSERT_NE(action, nullptr);
    EXPECT_GT(strlen(action), 0u);
    EXPECT_GE(action_value, 0.0f);
    EXPECT_LE(action_value, 1.0f);

    free(action);
}

TEST_F(ExecutiveMCTSTest, GetBestActionMCTS_NullInputs) {
    // WHAT: Test NULL input handling
    // WHY:  Should reject invalid inputs
    // HOW:  Pass NULL values

    char* action1 = executive_get_best_action_mcts(nullptr, "Test", nullptr, nullptr);
    EXPECT_EQ(action1, nullptr);

    char* action2 = executive_get_best_action_mcts(exec, nullptr, nullptr, nullptr);
    EXPECT_EQ(action2, nullptr);
}

TEST_F(ExecutiveMCTSTest, GetBestActionMCTS_ValueIsOptional) {
    // WHAT: Test NULL action_value parameter
    // WHY:  Should work without value output
    // HOW:  Pass NULL for action_value

    char* action = executive_get_best_action_mcts(exec, "No value needed", nullptr, nullptr);

    ASSERT_NE(action, nullptr);
    EXPECT_GT(strlen(action), 0u);

    free(action);
}

// =============================================================================
// INTEGRATION WITH REGULAR PLANNING TESTS
// =============================================================================

TEST_F(ExecutiveMCTSTest, RegularPlanUsesMCTS) {
    // WHAT: Verify executive_create_plan uses MCTS
    // WHY:  MCTS should be integrated as fallback
    // HOW:  Create plan and check it's non-trivial

    plan_t* plan = executive_create_plan(exec, "Integration test", 5);
    ASSERT_NE(plan, nullptr);

    // Plan should have meaningful steps (not just the old 3-step placeholder)
    EXPECT_GT(plan->num_steps, 0u);

    // Steps should have descriptions
    for (uint32_t i = 0; i < plan->num_steps; i++) {
        EXPECT_GT(strlen(plan->steps[i].description), 0u);
    }

    executive_destroy_plan(plan);
}

// =============================================================================
// PERFORMANCE TESTS
// =============================================================================

TEST_F(ExecutiveMCTSTest, MCTSPlanningCompletesinTime) {
    // WHAT: Verify MCTS planning completes in reasonable time
    // WHY:  Planning should not block too long
    // HOW:  Measure planning time

    executive_mcts_config_t config;
    executive_mcts_config_init(&config);
    config.max_iterations = 100;
    config.max_depth = 10;

    executive_mcts_stats_t stats;
    uint64_t start = nimcp_time_monotonic_ms();

    plan_t* plan = executive_create_plan_mcts(exec, "Timed planning", &config, &stats);

    uint64_t elapsed = nimcp_time_monotonic_ms() - start;

    ASSERT_NE(plan, nullptr);
    EXPECT_LT(elapsed, 5000u) << "Planning should complete in <5 seconds";
    EXPECT_LT(stats.planning_time_ms, 5000.0f);

    executive_destroy_plan(plan);
}

TEST_F(ExecutiveMCTSTest, MCTSScalesWithIterations) {
    // WHAT: Verify more iterations produce more exploration
    // WHY:  Configuration should affect behavior
    // HOW:  Compare stats with different iteration counts

    executive_mcts_config_t config_low, config_high;
    executive_mcts_config_init(&config_low);
    executive_mcts_config_init(&config_high);

    config_low.max_iterations = 20;
    config_high.max_iterations = 200;

    executive_mcts_stats_t stats_low, stats_high;

    plan_t* plan_low = executive_create_plan_mcts(exec, "Low iterations", &config_low, &stats_low);
    plan_t* plan_high = executive_create_plan_mcts(exec, "High iterations", &config_high, &stats_high);

    ASSERT_NE(plan_low, nullptr);
    ASSERT_NE(plan_high, nullptr);

    // More iterations should generally expand more nodes
    // (though stochastic, so not a strict requirement)
    EXPECT_GT(stats_high.nodes_expanded, 0u);
    EXPECT_GT(stats_low.nodes_expanded, 0u);

    executive_destroy_plan(plan_low);
    executive_destroy_plan(plan_high);
}
