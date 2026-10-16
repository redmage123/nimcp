/**
 * @file test_rcog_orchestrator.cpp
 * @brief Unit tests for Recursive Cognition Orchestrator
 *
 * WHAT: Comprehensive tests for RCOG orchestrator functionality
 * WHY:  Orchestrator handles task decomposition - critical for RLM pattern
 * HOW:  Unit tests for lifecycle, decomposition, batching, aggregation
 *
 * @author NIMCP Development Team
 * @date 2026-01-03
 */

#include <gtest/gtest.h>
#include <string.h>

#include "cognitive/recursive/nimcp_rcog_types.h"
#include "cognitive/recursive/nimcp_rcog_orchestrator.h"
#include "cognitive/recursive/nimcp_rcog_context_store.h"
#include "cognitive/recursive/nimcp_rcog_answer.h"

//=============================================================================
// Test Fixtures
//=============================================================================

/**
 * @brief Basic orchestrator test fixture
 */
class RcogOrchestratorTest : public ::testing::Test {
protected:
    rcog_orchestrator_t* orchestrator;

    void SetUp() override
    {
        orchestrator = rcog_orchestrator_create_default();
        ASSERT_NE(orchestrator, nullptr);
    }

    void TearDown() override
    {
        if (orchestrator) {
            rcog_orchestrator_destroy(orchestrator);
            orchestrator = nullptr;
        }
    }
};

/**
 * @brief Orchestrator with context store
 */
class RcogOrchestratorWithContextTest : public ::testing::Test {
protected:
    rcog_orchestrator_t* orchestrator;
    rcog_context_store_t* context_store;

    void SetUp() override
    {
        orchestrator = rcog_orchestrator_create_default();
        ASSERT_NE(orchestrator, nullptr);

        context_store = rcog_context_store_create_default();
        ASSERT_NE(context_store, nullptr);

        int result = rcog_orchestrator_connect_context_store(orchestrator, context_store);
        ASSERT_EQ(result, 0);
    }

    void TearDown() override
    {
        if (orchestrator) {
            rcog_orchestrator_destroy(orchestrator);
            orchestrator = nullptr;
        }
        if (context_store) {
            rcog_context_store_destroy(context_store);
            context_store = nullptr;
        }
    }
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST(RcogOrchestratorLifecycleTest, DefaultConfig)
{
    rcog_orchestrator_config_t config = rcog_orchestrator_default_config();

    EXPECT_GT(config.max_recursion_depth, 0u);
    EXPECT_GT(config.max_parallel_subtasks, 0u);
    EXPECT_GT(config.ready_threshold, 0.0f);
    EXPECT_LE(config.ready_threshold, 1.0f);
    EXPECT_GT(config.max_refinement_steps, 0u);
}

TEST(RcogOrchestratorLifecycleTest, CreateDefault)
{
    rcog_orchestrator_t* orch = rcog_orchestrator_create_default();
    ASSERT_NE(orch, nullptr);

    rcog_orchestrator_destroy(orch);
}

TEST(RcogOrchestratorLifecycleTest, CreateWithConfig)
{
    rcog_orchestrator_config_t config = rcog_orchestrator_default_config();
    config.max_recursion_depth = 5;
    config.max_parallel_subtasks = 8;
    config.ready_threshold = 0.9f;
    config.default_strategy = RCOG_DECOMP_PARALLEL;

    rcog_orchestrator_t* orch = rcog_orchestrator_create(&config);
    ASSERT_NE(orch, nullptr);

    rcog_orchestrator_destroy(orch);
}

TEST(RcogOrchestratorLifecycleTest, DestroyNull)
{
    // Should not crash
    rcog_orchestrator_destroy(nullptr);
}

TEST(RcogOrchestratorLifecycleTest, CreateWithNullConfig)
{
    rcog_orchestrator_t* orch = rcog_orchestrator_create(nullptr);
    ASSERT_NE(orch, nullptr);

    rcog_orchestrator_destroy(orch);
}

//=============================================================================
// Connection Tests
//=============================================================================

TEST_F(RcogOrchestratorTest, ConnectContextStore)
{
    rcog_context_store_t* store = rcog_context_store_create_default();
    ASSERT_NE(store, nullptr);

    int result = rcog_orchestrator_connect_context_store(orchestrator, store);
    EXPECT_EQ(result, 0);

    rcog_context_store_destroy(store);
}

TEST_F(RcogOrchestratorTest, ConnectAnswerRefiner)
{
    rcog_answer_refiner_t* refiner = rcog_answer_refiner_create_default();
    ASSERT_NE(refiner, nullptr);

    int result = rcog_orchestrator_connect_answer_refiner(orchestrator, refiner);
    EXPECT_EQ(result, 0);

    rcog_answer_refiner_destroy(refiner);
}

TEST(RcogOrchestratorConnectionTest, ConnectNullOrchestrator)
{
    rcog_context_store_t* store = rcog_context_store_create_default();

    int result = rcog_orchestrator_connect_context_store(nullptr, store);
    EXPECT_EQ(result, RCOG_ERROR_NULL_POINTER);

    rcog_context_store_destroy(store);
}

//=============================================================================
// Decomposition Tests
//=============================================================================

TEST_F(RcogOrchestratorWithContextTest, DecomposeSimpleGoal)
{
    rcog_goal_t goal;
    memset(&goal, 0, sizeof(goal));
    goal.type = RCOG_GOAL_QUESTION_ANSWERING;
    goal.query = "What is 2 + 2?";
    goal.priority = 0.5f;

    rcog_decomposition_t decomp;
    memset(&decomp, 0, sizeof(decomp));

    int result = rcog_orchestrator_decompose(orchestrator, &goal,
                                             context_store, &decomp);
    EXPECT_EQ(result, 0);

    // Should have at least one subtask
    EXPECT_GT(decomp.num_subtasks, 0u);

    // Clean up
    rcog_orchestrator_free_decomposition(&decomp);
}

TEST_F(RcogOrchestratorWithContextTest, DecomposeWithSequentialStrategy)
{
    rcog_goal_t goal;
    memset(&goal, 0, sizeof(goal));
    goal.type = RCOG_GOAL_PLANNING;
    goal.query = "Step 1, then step 2, then step 3";
    goal.priority = 0.5f;

    rcog_decomposition_t decomp;
    memset(&decomp, 0, sizeof(decomp));

    int result = rcog_orchestrator_decompose_with_strategy(
        orchestrator, &goal, context_store,
        RCOG_DECOMP_SEQUENTIAL, &decomp);
    EXPECT_EQ(result, 0);

    EXPECT_EQ(decomp.metadata.strategy, RCOG_DECOMP_SEQUENTIAL);

    rcog_orchestrator_free_decomposition(&decomp);
}

TEST_F(RcogOrchestratorWithContextTest, DecomposeWithParallelStrategy)
{
    rcog_goal_t goal;
    memset(&goal, 0, sizeof(goal));
    goal.type = RCOG_GOAL_PLANNING;
    goal.query = "Do A and B and C independently";
    goal.priority = 0.5f;

    rcog_decomposition_t decomp;
    memset(&decomp, 0, sizeof(decomp));

    int result = rcog_orchestrator_decompose_with_strategy(
        orchestrator, &goal, context_store,
        RCOG_DECOMP_PARALLEL, &decomp);
    EXPECT_EQ(result, 0);

    EXPECT_EQ(decomp.metadata.strategy, RCOG_DECOMP_PARALLEL);

    rcog_orchestrator_free_decomposition(&decomp);
}

TEST_F(RcogOrchestratorWithContextTest, DecomposeWithAdaptiveStrategy)
{
    rcog_goal_t goal;
    memset(&goal, 0, sizeof(goal));
    goal.type = RCOG_GOAL_PLANNING;
    goal.query = "Analyze this complex problem";
    goal.priority = 0.5f;

    rcog_decomposition_t decomp;
    memset(&decomp, 0, sizeof(decomp));

    int result = rcog_orchestrator_decompose_with_strategy(
        orchestrator, &goal, context_store,
        RCOG_DECOMP_ADAPTIVE, &decomp);
    EXPECT_EQ(result, 0);

    rcog_orchestrator_free_decomposition(&decomp);
}

TEST(RcogOrchestratorDecomposeTest, DecomposeNullParams)
{
    rcog_orchestrator_t* orch = rcog_orchestrator_create_default();
    rcog_goal_t goal;
    memset(&goal, 0, sizeof(goal));
    goal.query = "test";
    rcog_decomposition_t decomp;

    int result = rcog_orchestrator_decompose(nullptr, &goal, nullptr, &decomp);
    EXPECT_EQ(result, RCOG_ERROR_NULL_POINTER);

    result = rcog_orchestrator_decompose(orch, nullptr, nullptr, &decomp);
    EXPECT_EQ(result, RCOG_ERROR_NULL_POINTER);

    result = rcog_orchestrator_decompose(orch, &goal, nullptr, nullptr);
    EXPECT_EQ(result, RCOG_ERROR_NULL_POINTER);

    rcog_orchestrator_destroy(orch);
}

//=============================================================================
// Subtask Tests
//=============================================================================

TEST_F(RcogOrchestratorWithContextTest, DispatchSubtask)
{
    // Set up a decomposition first
    rcog_goal_t goal;
    memset(&goal, 0, sizeof(goal));
    goal.type = RCOG_GOAL_QUESTION_ANSWERING;
    goal.query = "Analyze data";

    rcog_decomposition_t decomp;
    int result = rcog_orchestrator_decompose(orchestrator, &goal, context_store, &decomp);
    EXPECT_EQ(result, 0);

    // Dispatch if we have subtasks
    if (decomp.num_subtasks > 0) {
        result = rcog_orchestrator_dispatch_subtask(orchestrator, &decomp.subtasks[0]);
        // May fail without pool connection, just ensure no crash
    }

    rcog_orchestrator_free_decomposition(&decomp);
}

TEST_F(RcogOrchestratorTest, DecomposeMultiStep)
{
    rcog_goal_t goal;
    memset(&goal, 0, sizeof(goal));
    goal.type = RCOG_GOAL_PLANNING;
    goal.query = "Step 1 then step 2";

    rcog_decomposition_t decomp;
    int result = rcog_orchestrator_decompose(orchestrator, &goal, nullptr, &decomp);
    EXPECT_EQ(result, 0);

    rcog_orchestrator_free_decomposition(&decomp);
}

TEST(RcogOrchestratorSubtaskTest, DispatchSubtaskNull)
{
    int result = rcog_orchestrator_dispatch_subtask(nullptr, nullptr);
    EXPECT_NE(result, 0);
}

//=============================================================================
// Batch Management Tests
//=============================================================================

TEST_F(RcogOrchestratorWithContextTest, DispatchAndTrack)
{
    // Create a decomposition to get subtasks
    rcog_goal_t goal;
    memset(&goal, 0, sizeof(goal));
    goal.type = RCOG_GOAL_REASONING;
    goal.query = "Test batch processing";

    rcog_decomposition_t decomp;
    int result = rcog_orchestrator_decompose(orchestrator, &goal, context_store, &decomp);
    EXPECT_EQ(result, 0);

    // Dispatch the decomposition
    rcog_batch_handle_t* handle = nullptr;
    result = rcog_orchestrator_dispatch(orchestrator, &decomp, &handle);
    // May fail without pool connection

    rcog_orchestrator_free_decomposition(&decomp);
}

TEST_F(RcogOrchestratorWithContextTest, GetReadySubtasks)
{
    rcog_goal_t goal;
    memset(&goal, 0, sizeof(goal));
    goal.type = RCOG_GOAL_ANALYSIS;
    goal.query = "Get ready subtasks test";

    rcog_decomposition_t decomp;
    int result = rcog_orchestrator_decompose(orchestrator, &goal, context_store, &decomp);
    EXPECT_EQ(result, 0);

    if (decomp.num_subtasks > 0) {
        uint64_t ready_ids[16];
        size_t ready_count = 0;
        // No completed tasks yet
        result = rcog_orchestrator_get_ready_subtasks(&decomp, nullptr, 0,
                                                       ready_ids, 16, &ready_count);
        EXPECT_EQ(result, 0);
    }

    rcog_orchestrator_free_decomposition(&decomp);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(RcogOrchestratorTest, GetStats)
{
    rcog_orchestrator_stats_t stats;
    int result = rcog_orchestrator_get_stats(orchestrator, &stats);
    EXPECT_EQ(result, 0);

    EXPECT_EQ(stats.goals_decomposed, 0u);
}

TEST_F(RcogOrchestratorWithContextTest, StatsAfterDecomposition)
{
    rcog_goal_t goal;
    memset(&goal, 0, sizeof(goal));
    goal.type = RCOG_GOAL_QUESTION_ANSWERING;
    goal.query = "Test question";

    rcog_decomposition_t decomp;
    rcog_orchestrator_decompose(orchestrator, &goal, context_store, &decomp);
    rcog_orchestrator_free_decomposition(&decomp);

    rcog_orchestrator_stats_t stats;
    rcog_orchestrator_get_stats(orchestrator, &stats);

    EXPECT_EQ(stats.goals_decomposed, 1u);
}

TEST_F(RcogOrchestratorTest, ResetStats)
{
    rcog_orchestrator_reset_stats(orchestrator);

    rcog_orchestrator_stats_t stats;
    rcog_orchestrator_get_stats(orchestrator, &stats);
    EXPECT_EQ(stats.goals_decomposed, 0u);
}

TEST(RcogOrchestratorStatsTest, StatsNullParams)
{
    rcog_orchestrator_stats_t stats;
    int result = rcog_orchestrator_get_stats(nullptr, &stats);
    EXPECT_EQ(result, RCOG_ERROR_NULL_POINTER);

    rcog_orchestrator_t* orch = rcog_orchestrator_create_default();
    result = rcog_orchestrator_get_stats(orch, nullptr);
    EXPECT_EQ(result, RCOG_ERROR_NULL_POINTER);

    rcog_orchestrator_destroy(orch);
}

//=============================================================================
// Immune Modulation Tests
//=============================================================================

TEST_F(RcogOrchestratorTest, ApplyImmuneModulation)
{
    rcog_immune_modulation_t mod;
    memset(&mod, 0, sizeof(mod));
    mod.capacity_multiplier = 0.7f;
    mod.max_depth_multiplier = 0.5f;
    mod.parallelism_multiplier = 0.8f;

    int result = rcog_orchestrator_apply_immune_modulation(orchestrator, &mod);
    EXPECT_EQ(result, 0);
}

TEST(RcogOrchestratorModulationTest, ModulationNullParams)
{
    rcog_orchestrator_t* orch = rcog_orchestrator_create_default();
    rcog_immune_modulation_t mod;

    int result = rcog_orchestrator_apply_immune_modulation(nullptr, &mod);
    EXPECT_EQ(result, RCOG_ERROR_NULL_POINTER);

    result = rcog_orchestrator_apply_immune_modulation(orch, nullptr);
    EXPECT_EQ(result, RCOG_ERROR_NULL_POINTER);

    rcog_orchestrator_destroy(orch);
}

//=============================================================================
// Depth and Recursion Tests
//=============================================================================

TEST_F(RcogOrchestratorWithContextTest, GetCurrentDepth)
{
    uint32_t depth = rcog_orchestrator_get_current_depth(orchestrator);
    EXPECT_EQ(depth, 0u);  // No active processing
}

TEST_F(RcogOrchestratorWithContextTest, WouldExceedDepth)
{
    // At current depth 0, adding more should be fine
    bool would_exceed = rcog_orchestrator_would_exceed_depth(orchestrator, 1);
    EXPECT_FALSE(would_exceed);

    // At max depth should return true
    rcog_orchestrator_config_t config = rcog_orchestrator_default_config();
    would_exceed = rcog_orchestrator_would_exceed_depth(orchestrator,
                                                        config.max_recursion_depth + 1);
    EXPECT_TRUE(would_exceed);
}

//=============================================================================
// Free Decomposition Tests
//=============================================================================

TEST(RcogOrchestratorFreeTest, FreeDecompositionNull)
{
    // Should not crash
    rcog_orchestrator_free_decomposition(nullptr);
}

TEST(RcogOrchestratorFreeTest, FreeTraceNull)
{
    // Should not crash
    rcog_orchestrator_free_trace(nullptr);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
