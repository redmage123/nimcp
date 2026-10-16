/**
 * @file test_rcog_engine.cpp
 * @brief Unit tests for Recursive Cognition Engine
 *
 * WHAT: Comprehensive tests for RCOG engine functionality
 * WHY:  Engine is the main coordinator - must be thoroughly tested
 * HOW:  Unit tests for lifecycle, goal processing, subsystem access, modulation
 *
 * @author NIMCP Development Team
 * @date 2026-01-03
 */

#include <gtest/gtest.h>
#include <string.h>
#include <thread>
#include <chrono>

#include "cognitive/recursive/nimcp_rcog_types.h"
#include "cognitive/recursive/nimcp_rcog_engine.h"
#include "cognitive/recursive/nimcp_rcog_context_store.h"
#include "cognitive/recursive/nimcp_rcog_orchestrator.h"
#include "cognitive/recursive/nimcp_rcog_delegation_pool.h"
#include "cognitive/recursive/nimcp_rcog_tool_router.h"
#include "cognitive/recursive/nimcp_rcog_answer.h"

//=============================================================================
// Test Fixtures
//=============================================================================

/**
 * @brief Basic engine test fixture
 */
class RcogEngineTest : public ::testing::Test {
protected:
    rcog_engine_t* engine;

    void SetUp() override
    {
        engine = rcog_engine_create_default();
        ASSERT_NE(engine, nullptr);
    }

    void TearDown() override
    {
        if (engine) {
            rcog_engine_destroy(engine);
            engine = nullptr;
        }
    }
};

/**
 * @brief Initialized engine test fixture
 */
class RcogEngineInitializedTest : public ::testing::Test {
protected:
    rcog_engine_t* engine;

    void SetUp() override
    {
        engine = rcog_engine_create_default();
        ASSERT_NE(engine, nullptr);
        int result = rcog_engine_init(engine);
        ASSERT_EQ(result, 0);
    }

    void TearDown() override
    {
        if (engine) {
            rcog_engine_destroy(engine);
            engine = nullptr;
        }
    }
};

/**
 * @brief Started engine test fixture
 */
class RcogEngineStartedTest : public ::testing::Test {
protected:
    rcog_engine_t* engine;

    void SetUp() override
    {
        engine = rcog_engine_create_default();
        ASSERT_NE(engine, nullptr);
        int result = rcog_engine_init(engine);
        ASSERT_EQ(result, 0);
        result = rcog_engine_start(engine);
        ASSERT_EQ(result, 0);
    }

    void TearDown() override
    {
        if (engine) {
            rcog_engine_stop(engine, 1000);
            rcog_engine_destroy(engine);
            engine = nullptr;
        }
    }
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST(RcogEngineLifecycleTest, DefaultConfig)
{
    rcog_engine_config_t config = rcog_engine_default_config();

    EXPECT_GT(config.max_recursion_depth, 0u);
    EXPECT_GT(config.max_parallel_subtasks, 0u);
    EXPECT_GT(config.max_concurrent_goals, 0u);
    EXPECT_GT(config.default_timeout_ms, 0u);
    EXPECT_GT(config.confidence_threshold, 0.0f);
    EXPECT_LE(config.confidence_threshold, 1.0f);
    EXPECT_GT(config.max_refinement_steps, 0u);
    EXPECT_TRUE(config.enable_early_termination);
}

TEST(RcogEngineLifecycleTest, CreateDefault)
{
    rcog_engine_t* engine = rcog_engine_create_default();
    ASSERT_NE(engine, nullptr);

    EXPECT_EQ(rcog_engine_get_state(engine), RCOG_ENGINE_UNINITIALIZED);

    rcog_engine_destroy(engine);
}

TEST(RcogEngineLifecycleTest, CreateWithConfig)
{
    rcog_engine_config_t config = rcog_engine_default_config();
    config.max_recursion_depth = 5;
    config.max_parallel_subtasks = 8;
    config.confidence_threshold = 0.8f;

    rcog_engine_t* engine = rcog_engine_create(&config);
    ASSERT_NE(engine, nullptr);

    EXPECT_EQ(rcog_engine_get_state(engine), RCOG_ENGINE_UNINITIALIZED);

    rcog_engine_destroy(engine);
}

TEST(RcogEngineLifecycleTest, DestroyNull)
{
    // Should not crash
    rcog_engine_destroy(nullptr);
}

TEST(RcogEngineLifecycleTest, GetStateNull)
{
    EXPECT_EQ(rcog_engine_get_state(nullptr), RCOG_ENGINE_UNINITIALIZED);
}

TEST_F(RcogEngineTest, InitEngine)
{
    EXPECT_EQ(rcog_engine_get_state(engine), RCOG_ENGINE_UNINITIALIZED);

    int result = rcog_engine_init(engine);
    EXPECT_EQ(result, 0);

    EXPECT_EQ(rcog_engine_get_state(engine), RCOG_ENGINE_READY);
}

TEST_F(RcogEngineTest, InitEngineNull)
{
    int result = rcog_engine_init(nullptr);
    EXPECT_EQ(result, RCOG_ERROR_NULL_POINTER);
}

TEST_F(RcogEngineTest, DoubleInit)
{
    int result = rcog_engine_init(engine);
    EXPECT_EQ(result, 0);

    // Second init should fail
    result = rcog_engine_init(engine);
    EXPECT_EQ(result, RCOG_ERROR_ALREADY_INITIALIZED);
}

TEST_F(RcogEngineInitializedTest, StartEngine)
{
    int result = rcog_engine_start(engine);
    EXPECT_EQ(result, 0);

    EXPECT_EQ(rcog_engine_get_state(engine), RCOG_ENGINE_READY);
}

TEST_F(RcogEngineInitializedTest, StopEngine)
{
    int result = rcog_engine_start(engine);
    EXPECT_EQ(result, 0);

    result = rcog_engine_stop(engine, 1000);
    EXPECT_EQ(result, 0);

    EXPECT_EQ(rcog_engine_get_state(engine), RCOG_ENGINE_STOPPED);
}

TEST(RcogEngineLifecycleTest, StartWithoutInit)
{
    rcog_engine_t* engine = rcog_engine_create_default();
    ASSERT_NE(engine, nullptr);

    int result = rcog_engine_start(engine);
    EXPECT_EQ(result, RCOG_ERROR_NOT_INITIALIZED);

    rcog_engine_destroy(engine);
}

//=============================================================================
// Subsystem Access Tests
//=============================================================================

TEST_F(RcogEngineInitializedTest, GetContextStore)
{
    rcog_context_store_t* store = rcog_engine_get_context_store(engine);
    EXPECT_NE(store, nullptr);
}

TEST_F(RcogEngineInitializedTest, GetOrchestrator)
{
    rcog_orchestrator_t* orch = rcog_engine_get_orchestrator(engine);
    EXPECT_NE(orch, nullptr);
}

TEST_F(RcogEngineInitializedTest, GetDelegationPool)
{
    rcog_delegation_pool_t* pool = rcog_engine_get_delegation_pool(engine);
    EXPECT_NE(pool, nullptr);
}

TEST_F(RcogEngineInitializedTest, GetToolRouter)
{
    rcog_tool_router_t* router = rcog_engine_get_tool_router(engine);
    EXPECT_NE(router, nullptr);
}

TEST_F(RcogEngineInitializedTest, GetAnswerRefiner)
{
    rcog_answer_refiner_t* refiner = rcog_engine_get_answer_refiner(engine);
    EXPECT_NE(refiner, nullptr);
}

TEST(RcogEngineSubsystemTest, GetSubsystemsNull)
{
    EXPECT_EQ(rcog_engine_get_context_store(nullptr), nullptr);
    EXPECT_EQ(rcog_engine_get_orchestrator(nullptr), nullptr);
    EXPECT_EQ(rcog_engine_get_delegation_pool(nullptr), nullptr);
    EXPECT_EQ(rcog_engine_get_tool_router(nullptr), nullptr);
    EXPECT_EQ(rcog_engine_get_answer_refiner(nullptr), nullptr);
}

//=============================================================================
// Context Management Tests
//=============================================================================

TEST_F(RcogEngineInitializedTest, SetGetContext)
{
    const char* data = "test context data";
    int result = rcog_engine_set_context(engine, "test_var", data,
                                         strlen(data) + 1, RCOG_DTYPE_TEXT);
    EXPECT_EQ(result, 0);

    rcog_query_result_t query_result;
    result = rcog_engine_get_context(engine, "test_var", &query_result);
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(query_result.found);
    EXPECT_EQ(query_result.dtype, RCOG_DTYPE_TEXT);
}

TEST_F(RcogEngineInitializedTest, ClearContext)
{
    const char* data = "test data";
    rcog_engine_set_context(engine, "var1", data, strlen(data) + 1, RCOG_DTYPE_TEXT);

    int result = rcog_engine_clear_context(engine, "var1");
    EXPECT_EQ(result, 0);

    // After clearing, the context should not be found
    rcog_query_result_t query_result;
    result = rcog_engine_get_context(engine, "var1", &query_result);
    EXPECT_EQ(result, (int)RCOG_ERROR_CONTEXT_NOT_FOUND);
}

TEST_F(RcogEngineInitializedTest, ClearAllContext)
{
    const char* data = "test data";
    rcog_engine_set_context(engine, "var1", data, strlen(data) + 1, RCOG_DTYPE_TEXT);
    rcog_engine_set_context(engine, "var2", data, strlen(data) + 1, RCOG_DTYPE_TEXT);

    int result = rcog_engine_clear_all_context(engine);
    EXPECT_EQ(result, 0);

    // After clearing all, variables should not be found
    rcog_query_result_t query_result;
    result = rcog_engine_get_context(engine, "var1", &query_result);
    EXPECT_EQ(result, (int)RCOG_ERROR_CONTEXT_NOT_FOUND);

    result = rcog_engine_get_context(engine, "var2", &query_result);
    EXPECT_EQ(result, (int)RCOG_ERROR_CONTEXT_NOT_FOUND);
}

TEST(RcogEngineContextTest, ContextNullEngine)
{
    const char* data = "test";
    int result = rcog_engine_set_context(nullptr, "var", data, 5, RCOG_DTYPE_TEXT);
    EXPECT_EQ(result, RCOG_ERROR_NULL_POINTER);

    rcog_query_result_t query_result;
    result = rcog_engine_get_context(nullptr, "var", &query_result);
    EXPECT_EQ(result, RCOG_ERROR_NULL_POINTER);
}

//=============================================================================
// Tool Management Tests
//=============================================================================

static rcog_error_t dummy_tool_handler(
    const void* input,
    size_t input_size,
    void* tool_context,
    void** output,
    size_t* output_size)
{
    (void)input;
    (void)input_size;
    (void)tool_context;
    *output = nullptr;
    *output_size = 0;
    return RCOG_OK;
}

TEST_F(RcogEngineInitializedTest, RegisterTool)
{
    int result = rcog_engine_register_tool(engine, "test_tool",
                                           dummy_tool_handler,
                                           RCOG_TIER_L1_REASONING,
                                           nullptr);
    EXPECT_EQ(result, 0);
}

TEST_F(RcogEngineInitializedTest, UnregisterTool)
{
    rcog_engine_register_tool(engine, "test_tool",
                              dummy_tool_handler,
                              RCOG_TIER_L1_REASONING,
                              nullptr);

    int result = rcog_engine_unregister_tool(engine, "test_tool");
    EXPECT_EQ(result, 0);
}

TEST_F(RcogEngineInitializedTest, ListTools)
{
    rcog_engine_register_tool(engine, "tool_a", dummy_tool_handler,
                              RCOG_TIER_L1_REASONING, nullptr);
    rcog_engine_register_tool(engine, "tool_b", dummy_tool_handler,
                              RCOG_TIER_L1_REASONING, nullptr);

    char tools[10][64];
    char* tool_ptrs[10];
    for (int i = 0; i < 10; i++) {
        tool_ptrs[i] = tools[i];
    }

    size_t count = 0;
    int result = rcog_engine_list_tools(engine, RCOG_TIER_L1_REASONING,
                                        (char(*)[64])tools, 10, &count);
    EXPECT_EQ(result, 0);
    EXPECT_GE(count, 2u);  // At least our two tools, plus builtins
}

//=============================================================================
// Immune Modulation Tests
//=============================================================================

TEST_F(RcogEngineInitializedTest, ApplyImmuneModulation)
{
    rcog_immune_modulation_t mod;
    memset(&mod, 0, sizeof(mod));
    mod.capacity_multiplier = 0.5f;
    mod.max_depth_multiplier = 0.75f;
    mod.parallelism_multiplier = 0.8f;
    mod.timeout_multiplier = 1.5f;
    mod.enable_degraded_mode = false;

    int result = rcog_engine_apply_immune_modulation(engine, &mod);
    EXPECT_EQ(result, 0);

    rcog_immune_modulation_t retrieved;
    result = rcog_engine_get_immune_modulation(engine, &retrieved);
    EXPECT_EQ(result, 0);
    EXPECT_FLOAT_EQ(retrieved.capacity_multiplier, 0.5f);
    EXPECT_FLOAT_EQ(retrieved.max_depth_multiplier, 0.75f);
}

TEST_F(RcogEngineInitializedTest, EnterDegradedMode)
{
    int result = rcog_engine_enter_degraded_mode(engine);
    EXPECT_EQ(result, 0);

    EXPECT_EQ(rcog_engine_get_state(engine), RCOG_ENGINE_DEGRADED);
}

TEST_F(RcogEngineInitializedTest, ExitDegradedMode)
{
    rcog_engine_enter_degraded_mode(engine);

    int result = rcog_engine_exit_degraded_mode(engine);
    EXPECT_EQ(result, 0);

    EXPECT_EQ(rcog_engine_get_state(engine), RCOG_ENGINE_READY);
}

TEST(RcogEngineModulationTest, ModulationNullParams)
{
    rcog_engine_t* engine = rcog_engine_create_default();
    rcog_engine_init(engine);

    int result = rcog_engine_apply_immune_modulation(nullptr, nullptr);
    EXPECT_EQ(result, RCOG_ERROR_NULL_POINTER);

    rcog_immune_modulation_t mod;
    result = rcog_engine_apply_immune_modulation(nullptr, &mod);
    EXPECT_EQ(result, RCOG_ERROR_NULL_POINTER);

    result = rcog_engine_apply_immune_modulation(engine, nullptr);
    EXPECT_EQ(result, RCOG_ERROR_NULL_POINTER);

    rcog_engine_destroy(engine);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(RcogEngineInitializedTest, GetStats)
{
    rcog_engine_stats_t stats;
    int result = rcog_engine_get_stats(engine, &stats);
    EXPECT_EQ(result, 0);

    EXPECT_EQ(stats.goals_submitted, 0u);
    EXPECT_EQ(stats.goals_completed, 0u);
    EXPECT_EQ(stats.state, RCOG_ENGINE_READY);
}

TEST_F(RcogEngineInitializedTest, ResetStats)
{
    rcog_engine_reset_stats(engine);

    rcog_engine_stats_t stats;
    rcog_engine_get_stats(engine, &stats);
    EXPECT_EQ(stats.goals_submitted, 0u);
}

TEST(RcogEngineStatsTest, StatsNullParams)
{
    rcog_engine_stats_t stats;
    int result = rcog_engine_get_stats(nullptr, &stats);
    EXPECT_EQ(result, RCOG_ERROR_NULL_POINTER);

    rcog_engine_t* engine = rcog_engine_create_default();
    result = rcog_engine_get_stats(engine, nullptr);
    EXPECT_EQ(result, RCOG_ERROR_NULL_POINTER);

    rcog_engine_destroy(engine);
}

//=============================================================================
// Utility Tests
//=============================================================================

TEST(RcogEngineUtilityTest, DefaultRequest)
{
    rcog_goal_t goal = rcog_engine_create_goal("What is 2+2?", RCOG_GOAL_QUESTION_ANSWERING);
    rcog_process_request_t request = rcog_engine_default_request(&goal);

    EXPECT_EQ(request.mode, RCOG_MODE_SYNC);
    EXPECT_EQ(request.timeout_ms, 0u);  // Use engine default
    EXPECT_FALSE(request.skip_decomposition);
    EXPECT_FALSE(request.force_local);
}

TEST(RcogEngineUtilityTest, CreateGoal)
{
    rcog_goal_t goal = rcog_engine_create_goal("Test query", RCOG_GOAL_PLANNING);

    EXPECT_EQ(goal.type, RCOG_GOAL_PLANNING);
    EXPECT_STREQ(goal.query, "Test query");
    EXPECT_FLOAT_EQ(goal.priority, 0.5f);
}

TEST(RcogEngineUtilityTest, StateName)
{
    EXPECT_STREQ(rcog_engine_state_name(RCOG_ENGINE_UNINITIALIZED), "uninitialized");
    EXPECT_STREQ(rcog_engine_state_name(RCOG_ENGINE_INITIALIZING), "initializing");
    EXPECT_STREQ(rcog_engine_state_name(RCOG_ENGINE_READY), "ready");
    EXPECT_STREQ(rcog_engine_state_name(RCOG_ENGINE_PROCESSING), "processing");
    EXPECT_STREQ(rcog_engine_state_name(RCOG_ENGINE_PAUSED), "paused");
    EXPECT_STREQ(rcog_engine_state_name(RCOG_ENGINE_DEGRADED), "degraded");
    EXPECT_STREQ(rcog_engine_state_name(RCOG_ENGINE_SHUTTING_DOWN), "shutting_down");
    EXPECT_STREQ(rcog_engine_state_name(RCOG_ENGINE_STOPPED), "stopped");
}

TEST_F(RcogEngineInitializedTest, IsReady)
{
    EXPECT_TRUE(rcog_engine_is_ready(engine));
}

TEST_F(RcogEngineInitializedTest, HasCapacity)
{
    EXPECT_TRUE(rcog_engine_has_capacity(engine));
}

TEST(RcogEngineUtilityTest, IsReadyNull)
{
    EXPECT_FALSE(rcog_engine_is_ready(nullptr));
}

TEST(RcogEngineUtilityTest, HasCapacityNull)
{
    EXPECT_FALSE(rcog_engine_has_capacity(nullptr));
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
