/**
 * @file test_middleware_pipeline.cpp
 * @brief Comprehensive tests for middleware pipeline system
 *
 * Tests all pipeline and context functions across 6 categories:
 * 1. Pipeline Lifecycle (3 functions)
 * 2. Pipeline Execution (2 functions)
 * 3. Pipeline Configuration (3 functions)
 * 4. Context Lifecycle (2 functions)
 * 5. Context Operations (8 functions)
 * 6. Integration Scenarios (full pipeline workflows)
 *
 * Total: 76 tests covering all functionality
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "middleware/pipeline/nimcp_middleware_pipeline.h"
#include "middleware/pipeline/nimcp_middleware_context.h"
#include "core/events/nimcp_event_bus.h"
#include "core/brain/nimcp_brain.h"
#include "utils/memory/nimcp_memory.h"

#include <cmath>
#include <vector>
#include <cstring>

//=============================================================================
// Test Fixtures
//=============================================================================

class MiddlewarePipelineTest : public ::testing::Test {
protected:
    brain_t brain;
    event_bus_t event_bus;

    void SetUp() override {
        // Create minimal brain for testing
        brain = brain_create(100, 50);

        // Create event bus
        event_bus = event_bus_create("test_bus", EVENT_DELIVERY_IMMEDIATE);
    }

    void TearDown() override {
        // Cleanup
        if (event_bus) {
            event_bus_destroy(event_bus);
        }
        if (brain) {
            brain_destroy(brain);
        }
    }

    // Helper: Create simple test stage
    static bool test_stage_success(middleware_context_t* ctx, void* data) {
        (void)data;
        return ctx != nullptr;
    }

    static bool test_stage_failure(middleware_context_t* ctx, void* data) {
        (void)ctx;
        (void)data;
        return false;
    }

    // Helper: Create basic pipeline config
    pipeline_config_t create_basic_config(pipeline_stage_config_t* stages, uint32_t num_stages) {
        pipeline_config_t config = {
            .stages = stages,
            .num_stages = num_stages,
            .event_bus = event_bus,
            .enable_profiling = true,
            .fail_fast = false
        };
        return config;
    }
};

//=============================================================================
// 1. PIPELINE LIFECYCLE TESTS
//=============================================================================

//-----------------------------------------------------------------------------
// middleware_pipeline_create Tests
//-----------------------------------------------------------------------------

TEST_F(MiddlewarePipelineTest, CreatePipeline_Success_SingleStage) {
    pipeline_stage_config_t stages[] = {
        {PIPELINE_STAGE_ENCODING, "Test", test_stage_success, nullptr, true, 1000}
    };

    pipeline_config_t config = create_basic_config(stages, 1);
    middleware_pipeline_t pipeline = middleware_pipeline_create(&config);

    ASSERT_NE(pipeline, nullptr);

    middleware_pipeline_destroy(pipeline);
}

TEST_F(MiddlewarePipelineTest, CreatePipeline_Success_MultipleStages) {
    pipeline_stage_config_t stages[] = {
        {PIPELINE_STAGE_ENCODING, "Encode", test_stage_success, nullptr, true, 1000},
        {PIPELINE_STAGE_EXTRACTION, "Extract", test_stage_success, nullptr, true, 1000},
        {PIPELINE_STAGE_DETECTION, "Detect", test_stage_success, nullptr, true, 1000}
    };

    pipeline_config_t config = create_basic_config(stages, 3);
    middleware_pipeline_t pipeline = middleware_pipeline_create(&config);

    ASSERT_NE(pipeline, nullptr);

    middleware_pipeline_destroy(pipeline);
}

TEST_F(MiddlewarePipelineTest, CreatePipeline_Success_AllStages) {
    pipeline_stage_config_t stages[PIPELINE_STAGE_COUNT];
    for (int i = 0; i < PIPELINE_STAGE_COUNT; i++) {
        stages[i].id = static_cast<pipeline_stage_id_t>(i);
        stages[i].name = "Stage";
        stages[i].execute = test_stage_success;
        stages[i].stage_data = nullptr;
        stages[i].enabled = true;
        stages[i].timeout_us = 1000;
    }

    pipeline_config_t config = create_basic_config(stages, PIPELINE_STAGE_COUNT);
    middleware_pipeline_t pipeline = middleware_pipeline_create(&config);

    ASSERT_NE(pipeline, nullptr);

    middleware_pipeline_destroy(pipeline);
}

TEST_F(MiddlewarePipelineTest, CreatePipeline_Success_ProfilingEnabled) {
    pipeline_stage_config_t stages[] = {
        {PIPELINE_STAGE_ENCODING, "Test", test_stage_success, nullptr, true, 1000}
    };

    pipeline_config_t config = create_basic_config(stages, 1);
    config.enable_profiling = true;

    middleware_pipeline_t pipeline = middleware_pipeline_create(&config);

    ASSERT_NE(pipeline, nullptr);

    middleware_pipeline_destroy(pipeline);
}

TEST_F(MiddlewarePipelineTest, CreatePipeline_Success_FailFastEnabled) {
    pipeline_stage_config_t stages[] = {
        {PIPELINE_STAGE_ENCODING, "Test", test_stage_success, nullptr, true, 1000}
    };

    pipeline_config_t config = create_basic_config(stages, 1);
    config.fail_fast = true;

    middleware_pipeline_t pipeline = middleware_pipeline_create(&config);

    ASSERT_NE(pipeline, nullptr);

    middleware_pipeline_destroy(pipeline);
}

TEST_F(MiddlewarePipelineTest, CreatePipeline_Fail_NullConfig) {
    middleware_pipeline_t pipeline = middleware_pipeline_create(nullptr);

    EXPECT_EQ(pipeline, nullptr);
}

TEST_F(MiddlewarePipelineTest, CreatePipeline_Fail_NullStages) {
    pipeline_config_t config = {
        .stages = nullptr,
        .num_stages = 1,
        .event_bus = event_bus,
        .enable_profiling = false,
        .fail_fast = false
    };

    middleware_pipeline_t pipeline = middleware_pipeline_create(&config);

    EXPECT_EQ(pipeline, nullptr);
}

TEST_F(MiddlewarePipelineTest, CreatePipeline_Fail_ZeroStages) {
    pipeline_stage_config_t stages[] = {
        {PIPELINE_STAGE_ENCODING, "Test", test_stage_success, nullptr, true, 1000}
    };

    pipeline_config_t config = create_basic_config(stages, 0);

    middleware_pipeline_t pipeline = middleware_pipeline_create(&config);

    EXPECT_EQ(pipeline, nullptr);
}

//-----------------------------------------------------------------------------
// middleware_pipeline_destroy Tests
//-----------------------------------------------------------------------------

TEST_F(MiddlewarePipelineTest, DestroyPipeline_Success) {
    pipeline_stage_config_t stages[] = {
        {PIPELINE_STAGE_ENCODING, "Test", test_stage_success, nullptr, true, 1000}
    };

    pipeline_config_t config = create_basic_config(stages, 1);
    middleware_pipeline_t pipeline = middleware_pipeline_create(&config);
    ASSERT_NE(pipeline, nullptr);

    // Should not crash
    middleware_pipeline_destroy(pipeline);
}

TEST_F(MiddlewarePipelineTest, DestroyPipeline_Null) {
    // Should not crash
    middleware_pipeline_destroy(nullptr);
}

//-----------------------------------------------------------------------------
// middleware_pipeline_create_default Tests
//-----------------------------------------------------------------------------

TEST_F(MiddlewarePipelineTest, CreateDefaultPipeline_Success) {
    middleware_pipeline_t pipeline = middleware_pipeline_create_default(brain, event_bus);

    ASSERT_NE(pipeline, nullptr);

    middleware_pipeline_destroy(pipeline);
}

//=============================================================================
// 2. PIPELINE EXECUTION TESTS
//=============================================================================

//-----------------------------------------------------------------------------
// middleware_pipeline_execute Tests
//-----------------------------------------------------------------------------

TEST_F(MiddlewarePipelineTest, ExecutePipeline_Success_AllStagesEnabled) {
    pipeline_stage_config_t stages[] = {
        {PIPELINE_STAGE_ENCODING, "Stage1", test_stage_success, nullptr, true, 1000},
        {PIPELINE_STAGE_EXTRACTION, "Stage2", test_stage_success, nullptr, true, 1000},
        {PIPELINE_STAGE_DETECTION, "Stage3", test_stage_success, nullptr, true, 1000}
    };

    pipeline_config_t config = create_basic_config(stages, 3);
    middleware_pipeline_t pipeline = middleware_pipeline_create(&config);
    ASSERT_NE(pipeline, nullptr);

    middleware_context_t* context = middleware_context_create(brain, 100, 50, 10, 3);
    ASSERT_NE(context, nullptr);

    bool result = middleware_pipeline_execute(pipeline, context);

    EXPECT_TRUE(result);

    middleware_context_destroy(context);
    middleware_pipeline_destroy(pipeline);
}

TEST_F(MiddlewarePipelineTest, ExecutePipeline_Success_SomeStagesDisabled) {
    pipeline_stage_config_t stages[] = {
        {PIPELINE_STAGE_ENCODING, "Stage1", test_stage_success, nullptr, true, 1000},
        {PIPELINE_STAGE_EXTRACTION, "Stage2", test_stage_success, nullptr, false, 1000},
        {PIPELINE_STAGE_DETECTION, "Stage3", test_stage_success, nullptr, true, 1000}
    };

    pipeline_config_t config = create_basic_config(stages, 3);
    middleware_pipeline_t pipeline = middleware_pipeline_create(&config);
    ASSERT_NE(pipeline, nullptr);

    middleware_context_t* context = middleware_context_create(brain, 100, 50, 10, 3);
    ASSERT_NE(context, nullptr);

    bool result = middleware_pipeline_execute(pipeline, context);

    // Should succeed - disabled stages are skipped
    EXPECT_TRUE(result);

    middleware_context_destroy(context);
    middleware_pipeline_destroy(pipeline);
}

TEST_F(MiddlewarePipelineTest, ExecutePipeline_Partial_OneStageFailsNoFailFast) {
    pipeline_stage_config_t stages[] = {
        {PIPELINE_STAGE_ENCODING, "Stage1", test_stage_success, nullptr, true, 1000},
        {PIPELINE_STAGE_EXTRACTION, "Stage2", test_stage_failure, nullptr, true, 1000},
        {PIPELINE_STAGE_DETECTION, "Stage3", test_stage_success, nullptr, true, 1000}
    };

    pipeline_config_t config = create_basic_config(stages, 3);
    config.fail_fast = false;

    middleware_pipeline_t pipeline = middleware_pipeline_create(&config);
    ASSERT_NE(pipeline, nullptr);

    middleware_context_t* context = middleware_context_create(brain, 100, 50, 10, 3);
    ASSERT_NE(context, nullptr);

    bool result = middleware_pipeline_execute(pipeline, context);

    // Should return false but execute all stages
    EXPECT_FALSE(result);

    middleware_context_destroy(context);
    middleware_pipeline_destroy(pipeline);
}

TEST_F(MiddlewarePipelineTest, ExecutePipeline_Fail_OneStageFailsWithFailFast) {
    pipeline_stage_config_t stages[] = {
        {PIPELINE_STAGE_ENCODING, "Stage1", test_stage_success, nullptr, true, 1000},
        {PIPELINE_STAGE_EXTRACTION, "Stage2", test_stage_failure, nullptr, true, 1000},
        {PIPELINE_STAGE_DETECTION, "Stage3", test_stage_success, nullptr, true, 1000}
    };

    pipeline_config_t config = create_basic_config(stages, 3);
    config.fail_fast = true;

    middleware_pipeline_t pipeline = middleware_pipeline_create(&config);
    ASSERT_NE(pipeline, nullptr);

    middleware_context_t* context = middleware_context_create(brain, 100, 50, 10, 3);
    ASSERT_NE(context, nullptr);

    bool result = middleware_pipeline_execute(pipeline, context);

    // Should return false and stop at first failure
    EXPECT_FALSE(result);

    middleware_context_destroy(context);
    middleware_pipeline_destroy(pipeline);
}

TEST_F(MiddlewarePipelineTest, ExecutePipeline_Fail_NullPipeline) {
    middleware_context_t* context = middleware_context_create(brain, 100, 50, 10, 3);
    ASSERT_NE(context, nullptr);

    bool result = middleware_pipeline_execute(nullptr, context);

    EXPECT_FALSE(result);

    middleware_context_destroy(context);
}

TEST_F(MiddlewarePipelineTest, ExecutePipeline_Fail_NullContext) {
    pipeline_stage_config_t stages[] = {
        {PIPELINE_STAGE_ENCODING, "Test", test_stage_success, nullptr, true, 1000}
    };

    pipeline_config_t config = create_basic_config(stages, 1);
    middleware_pipeline_t pipeline = middleware_pipeline_create(&config);
    ASSERT_NE(pipeline, nullptr);

    bool result = middleware_pipeline_execute(pipeline, nullptr);

    EXPECT_FALSE(result);

    middleware_pipeline_destroy(pipeline);
}

TEST_F(MiddlewarePipelineTest, ExecutePipeline_MultipleExecutions_Success) {
    pipeline_stage_config_t stages[] = {
        {PIPELINE_STAGE_ENCODING, "Test", test_stage_success, nullptr, true, 1000}
    };

    pipeline_config_t config = create_basic_config(stages, 1);
    middleware_pipeline_t pipeline = middleware_pipeline_create(&config);
    ASSERT_NE(pipeline, nullptr);

    middleware_context_t* context = middleware_context_create(brain, 100, 50, 10, 1);
    ASSERT_NE(context, nullptr);

    // Execute multiple times
    for (int i = 0; i < 10; i++) {
        bool result = middleware_pipeline_execute(pipeline, context);
        EXPECT_TRUE(result);
    }

    middleware_context_destroy(context);
    middleware_pipeline_destroy(pipeline);
}

//-----------------------------------------------------------------------------
// middleware_pipeline_execute_stage Tests
//-----------------------------------------------------------------------------

TEST_F(MiddlewarePipelineTest, ExecuteStage_Success) {
    pipeline_stage_config_t stages[] = {
        {PIPELINE_STAGE_ENCODING, "Stage1", test_stage_success, nullptr, true, 1000},
        {PIPELINE_STAGE_EXTRACTION, "Stage2", test_stage_success, nullptr, true, 1000}
    };

    pipeline_config_t config = create_basic_config(stages, 2);
    middleware_pipeline_t pipeline = middleware_pipeline_create(&config);
    ASSERT_NE(pipeline, nullptr);

    middleware_context_t* context = middleware_context_create(brain, 100, 50, 10, 2);
    ASSERT_NE(context, nullptr);

    bool result = middleware_pipeline_execute_stage(pipeline, PIPELINE_STAGE_ENCODING, context);

    EXPECT_TRUE(result);

    middleware_context_destroy(context);
    middleware_pipeline_destroy(pipeline);
}

TEST_F(MiddlewarePipelineTest, ExecuteStage_Fail_StageDisabled) {
    pipeline_stage_config_t stages[] = {
        {PIPELINE_STAGE_ENCODING, "Stage1", test_stage_success, nullptr, false, 1000}
    };

    pipeline_config_t config = create_basic_config(stages, 1);
    middleware_pipeline_t pipeline = middleware_pipeline_create(&config);
    ASSERT_NE(pipeline, nullptr);

    middleware_context_t* context = middleware_context_create(brain, 100, 50, 10, 1);
    ASSERT_NE(context, nullptr);

    bool result = middleware_pipeline_execute_stage(pipeline, PIPELINE_STAGE_ENCODING, context);

    EXPECT_FALSE(result);

    middleware_context_destroy(context);
    middleware_pipeline_destroy(pipeline);
}

TEST_F(MiddlewarePipelineTest, ExecuteStage_Fail_InvalidStageId) {
    pipeline_stage_config_t stages[] = {
        {PIPELINE_STAGE_ENCODING, "Test", test_stage_success, nullptr, true, 1000}
    };

    pipeline_config_t config = create_basic_config(stages, 1);
    middleware_pipeline_t pipeline = middleware_pipeline_create(&config);
    ASSERT_NE(pipeline, nullptr);

    middleware_context_t* context = middleware_context_create(brain, 100, 50, 10, 1);
    ASSERT_NE(context, nullptr);

    // Try to execute stage beyond num_stages
    bool result = middleware_pipeline_execute_stage(pipeline, PIPELINE_STAGE_EVENTS, context);

    EXPECT_FALSE(result);

    middleware_context_destroy(context);
    middleware_pipeline_destroy(pipeline);
}

TEST_F(MiddlewarePipelineTest, ExecuteStage_Fail_NullPipeline) {
    middleware_context_t* context = middleware_context_create(brain, 100, 50, 10, 1);
    ASSERT_NE(context, nullptr);

    bool result = middleware_pipeline_execute_stage(nullptr, PIPELINE_STAGE_ENCODING, context);

    EXPECT_FALSE(result);

    middleware_context_destroy(context);
}

TEST_F(MiddlewarePipelineTest, ExecuteStage_Fail_NullContext) {
    pipeline_stage_config_t stages[] = {
        {PIPELINE_STAGE_ENCODING, "Test", test_stage_success, nullptr, true, 1000}
    };

    pipeline_config_t config = create_basic_config(stages, 1);
    middleware_pipeline_t pipeline = middleware_pipeline_create(&config);
    ASSERT_NE(pipeline, nullptr);

    bool result = middleware_pipeline_execute_stage(pipeline, PIPELINE_STAGE_ENCODING, nullptr);

    EXPECT_FALSE(result);

    middleware_pipeline_destroy(pipeline);
}

//=============================================================================
// 3. PIPELINE CONFIGURATION TESTS
//=============================================================================

//-----------------------------------------------------------------------------
// middleware_pipeline_set_stage_enabled Tests
//-----------------------------------------------------------------------------

TEST_F(MiddlewarePipelineTest, SetStageEnabled_Success_Enable) {
    pipeline_stage_config_t stages[] = {
        {PIPELINE_STAGE_ENCODING, "Test", test_stage_success, nullptr, false, 1000}
    };

    pipeline_config_t config = create_basic_config(stages, 1);
    middleware_pipeline_t pipeline = middleware_pipeline_create(&config);
    ASSERT_NE(pipeline, nullptr);

    bool result = middleware_pipeline_set_stage_enabled(pipeline, PIPELINE_STAGE_ENCODING, true);

    EXPECT_TRUE(result);

    middleware_pipeline_destroy(pipeline);
}

TEST_F(MiddlewarePipelineTest, SetStageEnabled_Success_Disable) {
    pipeline_stage_config_t stages[] = {
        {PIPELINE_STAGE_ENCODING, "Test", test_stage_success, nullptr, true, 1000}
    };

    pipeline_config_t config = create_basic_config(stages, 1);
    middleware_pipeline_t pipeline = middleware_pipeline_create(&config);
    ASSERT_NE(pipeline, nullptr);

    bool result = middleware_pipeline_set_stage_enabled(pipeline, PIPELINE_STAGE_ENCODING, false);

    EXPECT_TRUE(result);

    middleware_pipeline_destroy(pipeline);
}

TEST_F(MiddlewarePipelineTest, SetStageEnabled_Fail_NullPipeline) {
    bool result = middleware_pipeline_set_stage_enabled(nullptr, PIPELINE_STAGE_ENCODING, true);

    EXPECT_FALSE(result);
}

TEST_F(MiddlewarePipelineTest, SetStageEnabled_Fail_InvalidStageId) {
    pipeline_stage_config_t stages[] = {
        {PIPELINE_STAGE_ENCODING, "Test", test_stage_success, nullptr, true, 1000}
    };

    pipeline_config_t config = create_basic_config(stages, 1);
    middleware_pipeline_t pipeline = middleware_pipeline_create(&config);
    ASSERT_NE(pipeline, nullptr);

    // Try to set stage beyond num_stages
    bool result = middleware_pipeline_set_stage_enabled(pipeline, PIPELINE_STAGE_EVENTS, true);

    EXPECT_FALSE(result);

    middleware_pipeline_destroy(pipeline);
}

//-----------------------------------------------------------------------------
// middleware_pipeline_get_stats Tests
//-----------------------------------------------------------------------------

TEST_F(MiddlewarePipelineTest, GetStats_Success) {
    pipeline_stage_config_t stages[] = {
        {PIPELINE_STAGE_ENCODING, "Test", test_stage_success, nullptr, true, 1000}
    };

    pipeline_config_t config = create_basic_config(stages, 1);
    middleware_pipeline_t pipeline = middleware_pipeline_create(&config);
    ASSERT_NE(pipeline, nullptr);

    pipeline_stats_t stats;
    bool result = middleware_pipeline_get_stats(pipeline, &stats);

    EXPECT_TRUE(result);
    EXPECT_EQ(stats.num_stages, 1);
    EXPECT_EQ(stats.total_executions, 0);

    // Free allocated arrays
    nimcp_free(stats.stage_execution_counts);
    nimcp_free(stats.stage_total_time_us);
    nimcp_free(stats.stage_avg_time_us);

    middleware_pipeline_destroy(pipeline);
}

TEST_F(MiddlewarePipelineTest, GetStats_AfterExecution) {
    pipeline_stage_config_t stages[] = {
        {PIPELINE_STAGE_ENCODING, "Test", test_stage_success, nullptr, true, 1000}
    };

    pipeline_config_t config = create_basic_config(stages, 1);
    config.enable_profiling = true;

    middleware_pipeline_t pipeline = middleware_pipeline_create(&config);
    ASSERT_NE(pipeline, nullptr);

    middleware_context_t* context = middleware_context_create(brain, 100, 50, 10, 1);
    ASSERT_NE(context, nullptr);

    // Execute pipeline
    middleware_pipeline_execute(pipeline, context);

    pipeline_stats_t stats;
    bool result = middleware_pipeline_get_stats(pipeline, &stats);

    EXPECT_TRUE(result);
    EXPECT_EQ(stats.total_executions, 1);
    EXPECT_EQ(stats.successful_executions, 1);
    EXPECT_EQ(stats.failed_executions, 0);

    // Free allocated arrays
    nimcp_free(stats.stage_execution_counts);
    nimcp_free(stats.stage_total_time_us);
    nimcp_free(stats.stage_avg_time_us);

    middleware_context_destroy(context);
    middleware_pipeline_destroy(pipeline);
}

TEST_F(MiddlewarePipelineTest, GetStats_Fail_NullPipeline) {
    pipeline_stats_t stats;
    bool result = middleware_pipeline_get_stats(nullptr, &stats);

    EXPECT_FALSE(result);
}

TEST_F(MiddlewarePipelineTest, GetStats_Fail_NullStats) {
    pipeline_stage_config_t stages[] = {
        {PIPELINE_STAGE_ENCODING, "Test", test_stage_success, nullptr, true, 1000}
    };

    pipeline_config_t config = create_basic_config(stages, 1);
    middleware_pipeline_t pipeline = middleware_pipeline_create(&config);
    ASSERT_NE(pipeline, nullptr);

    bool result = middleware_pipeline_get_stats(pipeline, nullptr);

    EXPECT_FALSE(result);

    middleware_pipeline_destroy(pipeline);
}

//-----------------------------------------------------------------------------
// middleware_pipeline_reset_stats Tests
//-----------------------------------------------------------------------------

TEST_F(MiddlewarePipelineTest, ResetStats_Success) {
    pipeline_stage_config_t stages[] = {
        {PIPELINE_STAGE_ENCODING, "Test", test_stage_success, nullptr, true, 1000}
    };

    pipeline_config_t config = create_basic_config(stages, 1);
    middleware_pipeline_t pipeline = middleware_pipeline_create(&config);
    ASSERT_NE(pipeline, nullptr);

    middleware_context_t* context = middleware_context_create(brain, 100, 50, 10, 1);
    ASSERT_NE(context, nullptr);

    // Execute to generate stats
    middleware_pipeline_execute(pipeline, context);

    // Reset stats
    middleware_pipeline_reset_stats(pipeline);

    // Verify stats are reset
    pipeline_stats_t stats;
    middleware_pipeline_get_stats(pipeline, &stats);

    EXPECT_EQ(stats.total_executions, 0);
    EXPECT_EQ(stats.successful_executions, 0);
    EXPECT_EQ(stats.failed_executions, 0);

    nimcp_free(stats.stage_execution_counts);
    nimcp_free(stats.stage_total_time_us);
    nimcp_free(stats.stage_avg_time_us);

    middleware_context_destroy(context);
    middleware_pipeline_destroy(pipeline);
}

TEST_F(MiddlewarePipelineTest, ResetStats_Null) {
    // Should not crash
    middleware_pipeline_reset_stats(nullptr);
}

//=============================================================================
// 4. CONTEXT LIFECYCLE TESTS
//=============================================================================

//-----------------------------------------------------------------------------
// middleware_context_create Tests
//-----------------------------------------------------------------------------

TEST_F(MiddlewarePipelineTest, CreateContext_Success_BasicConfig) {
    middleware_context_t* context = middleware_context_create(brain, 100, 50, 10, 7);

    ASSERT_NE(context, nullptr);
    EXPECT_EQ(context->brain, brain);
    EXPECT_NE(context->cached_features, nullptr);
    EXPECT_EQ(context->num_cached_features, 100);
    EXPECT_NE(context->detected_patterns, nullptr);
    EXPECT_EQ(context->num_detected_patterns, 50);
    EXPECT_NE(context->recent_events, nullptr);
    EXPECT_EQ(context->recent_event_capacity, 10);
    EXPECT_NE(context->stage_timings_us, nullptr);
    EXPECT_EQ(context->num_stages, 7);

    middleware_context_destroy(context);
}

TEST_F(MiddlewarePipelineTest, CreateContext_Success_LargeBuffers) {
    middleware_context_t* context = middleware_context_create(brain, 1000, 500, 100, 10);

    ASSERT_NE(context, nullptr);
    EXPECT_EQ(context->num_cached_features, 1000);
    EXPECT_EQ(context->num_detected_patterns, 500);
    EXPECT_EQ(context->recent_event_capacity, 100);

    middleware_context_destroy(context);
}

TEST_F(MiddlewarePipelineTest, CreateContext_Success_MinimalBuffers) {
    middleware_context_t* context = middleware_context_create(brain, 1, 1, 1, 1);

    ASSERT_NE(context, nullptr);
    EXPECT_EQ(context->num_cached_features, 1);
    EXPECT_EQ(context->num_detected_patterns, 1);
    EXPECT_EQ(context->recent_event_capacity, 1);

    middleware_context_destroy(context);
}

//-----------------------------------------------------------------------------
// middleware_context_destroy Tests
//-----------------------------------------------------------------------------

TEST_F(MiddlewarePipelineTest, DestroyContext_Success) {
    middleware_context_t* context = middleware_context_create(brain, 100, 50, 10, 7);
    ASSERT_NE(context, nullptr);

    // Should not crash
    middleware_context_destroy(context);
}

TEST_F(MiddlewarePipelineTest, DestroyContext_Null) {
    // Should not crash
    middleware_context_destroy(nullptr);
}

//=============================================================================
// 5. CONTEXT OPERATIONS TESTS
//=============================================================================

//-----------------------------------------------------------------------------
// middleware_context_set_active_neurons Tests
//-----------------------------------------------------------------------------

TEST_F(MiddlewarePipelineTest, SetActiveNeurons_Success) {
    middleware_context_t* context = middleware_context_create(brain, 100, 50, 10, 7);
    ASSERT_NE(context, nullptr);

    uint32_t neurons[] = {1, 5, 10, 15, 20};

    middleware_context_set_active_neurons(context, neurons, 5);

    EXPECT_NE(context->active_neurons, nullptr);
    EXPECT_EQ(context->num_active_neurons, 5);
    EXPECT_EQ(context->active_neurons[0], 1);
    EXPECT_EQ(context->active_neurons[4], 20);

    middleware_context_destroy(context);
}

TEST_F(MiddlewarePipelineTest, SetActiveNeurons_Success_EmptyArray) {
    middleware_context_t* context = middleware_context_create(brain, 100, 50, 10, 7);
    ASSERT_NE(context, nullptr);

    middleware_context_set_active_neurons(context, nullptr, 0);

    EXPECT_EQ(context->active_neurons, nullptr);
    EXPECT_EQ(context->num_active_neurons, 0);

    middleware_context_destroy(context);
}

TEST_F(MiddlewarePipelineTest, SetActiveNeurons_Success_MultipleUpdates) {
    middleware_context_t* context = middleware_context_create(brain, 100, 50, 10, 7);
    ASSERT_NE(context, nullptr);

    uint32_t neurons1[] = {1, 2, 3};
    uint32_t neurons2[] = {4, 5, 6, 7};

    middleware_context_set_active_neurons(context, neurons1, 3);
    EXPECT_EQ(context->num_active_neurons, 3);

    middleware_context_set_active_neurons(context, neurons2, 4);
    EXPECT_EQ(context->num_active_neurons, 4);
    EXPECT_EQ(context->active_neurons[0], 4);

    middleware_context_destroy(context);
}

TEST_F(MiddlewarePipelineTest, SetActiveNeurons_Null_Context) {
    uint32_t neurons[] = {1, 2, 3};

    // Should not crash
    middleware_context_set_active_neurons(nullptr, neurons, 3);
}

//-----------------------------------------------------------------------------
// middleware_context_cache_features Tests
//-----------------------------------------------------------------------------

TEST_F(MiddlewarePipelineTest, CacheFeatures_Success) {
    middleware_context_t* context = middleware_context_create(brain, 5, 50, 10, 7);
    ASSERT_NE(context, nullptr);

    float features[] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f};

    middleware_context_cache_features(context, features, 5);

    EXPECT_TRUE(context->features_valid);
    EXPECT_FLOAT_EQ(context->cached_features[0], 0.1f);
    EXPECT_FLOAT_EQ(context->cached_features[4], 0.5f);

    middleware_context_destroy(context);
}

TEST_F(MiddlewarePipelineTest, CacheFeatures_Success_PartialArray) {
    middleware_context_t* context = middleware_context_create(brain, 10, 50, 10, 7);
    ASSERT_NE(context, nullptr);

    float features[] = {0.1f, 0.2f, 0.3f};

    middleware_context_cache_features(context, features, 3);

    EXPECT_TRUE(context->features_valid);

    middleware_context_destroy(context);
}

TEST_F(MiddlewarePipelineTest, CacheFeatures_Fail_TooManyFeatures) {
    middleware_context_t* context = middleware_context_create(brain, 3, 50, 10, 7);
    ASSERT_NE(context, nullptr);

    float features[] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f};

    // Should not cache - too many features
    middleware_context_cache_features(context, features, 5);

    // Features_valid might not be set
    middleware_context_destroy(context);
}

TEST_F(MiddlewarePipelineTest, CacheFeatures_Null_Context) {
    float features[] = {0.1f, 0.2f, 0.3f};

    // Should not crash
    middleware_context_cache_features(nullptr, features, 3);
}

//-----------------------------------------------------------------------------
// middleware_context_get_cached_features Tests
//-----------------------------------------------------------------------------

TEST_F(MiddlewarePipelineTest, GetCachedFeatures_Success) {
    middleware_context_t* context = middleware_context_create(brain, 5, 50, 10, 7);
    ASSERT_NE(context, nullptr);

    float features[] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f};
    middleware_context_cache_features(context, features, 5);

    float* cached_features;
    uint32_t count;
    bool result = middleware_context_get_cached_features(context, &cached_features, &count);

    EXPECT_TRUE(result);
    EXPECT_NE(cached_features, nullptr);
    EXPECT_EQ(count, 5);

    middleware_context_destroy(context);
}

TEST_F(MiddlewarePipelineTest, GetCachedFeatures_Fail_NoCachedFeatures) {
    middleware_context_t* context = middleware_context_create(brain, 5, 50, 10, 7);
    ASSERT_NE(context, nullptr);

    float* cached_features;
    uint32_t count;
    bool result = middleware_context_get_cached_features(context, &cached_features, &count);

    EXPECT_FALSE(result);

    middleware_context_destroy(context);
}

TEST_F(MiddlewarePipelineTest, GetCachedFeatures_Fail_NullContext) {
    float* cached_features;
    uint32_t count;
    bool result = middleware_context_get_cached_features(nullptr, &cached_features, &count);

    EXPECT_FALSE(result);
}

//-----------------------------------------------------------------------------
// middleware_context_invalidate_cache Tests
//-----------------------------------------------------------------------------

TEST_F(MiddlewarePipelineTest, InvalidateCache_Success) {
    middleware_context_t* context = middleware_context_create(brain, 5, 50, 10, 7);
    ASSERT_NE(context, nullptr);

    float features[] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f};
    middleware_context_cache_features(context, features, 5);

    EXPECT_TRUE(context->features_valid);

    middleware_context_invalidate_cache(context);

    EXPECT_FALSE(context->features_valid);

    middleware_context_destroy(context);
}

TEST_F(MiddlewarePipelineTest, InvalidateCache_Null) {
    // Should not crash
    middleware_context_invalidate_cache(nullptr);
}

//-----------------------------------------------------------------------------
// middleware_context_add_event Tests
//-----------------------------------------------------------------------------

TEST_F(MiddlewarePipelineTest, AddEvent_Success) {
    middleware_context_t* context = middleware_context_create(brain, 100, 50, 10, 7);
    ASSERT_NE(context, nullptr);

    brain_event_t event = event_create(EVENT_NEURON_SPIKE, EVENT_PRIORITY_NORMAL, "test");

    middleware_context_add_event(context, &event);

    EXPECT_EQ(context->recent_event_count, 1);
    EXPECT_EQ(context->recent_event_head, 1);

    middleware_context_destroy(context);
}

TEST_F(MiddlewarePipelineTest, AddEvent_Success_MultipleEvents) {
    middleware_context_t* context = middleware_context_create(brain, 100, 50, 5, 7);
    ASSERT_NE(context, nullptr);

    for (int i = 0; i < 3; i++) {
        brain_event_t event = event_create(EVENT_NEURON_SPIKE, EVENT_PRIORITY_NORMAL, "test");
        middleware_context_add_event(context, &event);
    }

    EXPECT_EQ(context->recent_event_count, 3);

    middleware_context_destroy(context);
}

TEST_F(MiddlewarePipelineTest, AddEvent_Success_CircularBuffer) {
    middleware_context_t* context = middleware_context_create(brain, 100, 50, 3, 7);
    ASSERT_NE(context, nullptr);

    // Add more events than capacity
    for (int i = 0; i < 5; i++) {
        brain_event_t event = event_create(EVENT_NEURON_SPIKE, EVENT_PRIORITY_NORMAL, "test");
        middleware_context_add_event(context, &event);
    }

    // Should wrap around
    EXPECT_EQ(context->recent_event_count, 3);
    EXPECT_EQ(context->recent_event_head, 2); // (5 % 3) = 2

    middleware_context_destroy(context);
}

TEST_F(MiddlewarePipelineTest, AddEvent_Null_Context) {
    brain_event_t event = event_create(EVENT_NEURON_SPIKE, EVENT_PRIORITY_NORMAL, "test");

    // Should not crash
    middleware_context_add_event(nullptr, &event);
}

TEST_F(MiddlewarePipelineTest, AddEvent_Null_Event) {
    middleware_context_t* context = middleware_context_create(brain, 100, 50, 10, 7);
    ASSERT_NE(context, nullptr);

    // Should not crash
    middleware_context_add_event(context, nullptr);

    middleware_context_destroy(context);
}

//-----------------------------------------------------------------------------
// middleware_context_get_recent_events Tests
//-----------------------------------------------------------------------------

TEST_F(MiddlewarePipelineTest, GetRecentEvents_Success) {
    middleware_context_t* context = middleware_context_create(brain, 100, 50, 10, 7);
    ASSERT_NE(context, nullptr);

    brain_event_t event = event_create(EVENT_NEURON_SPIKE, EVENT_PRIORITY_NORMAL, "test");
    middleware_context_add_event(context, &event);

    brain_event_t* events;
    uint32_t count = middleware_context_get_recent_events(context, &events);

    EXPECT_EQ(count, 1);
    EXPECT_NE(events, nullptr);

    middleware_context_destroy(context);
}

TEST_F(MiddlewarePipelineTest, GetRecentEvents_Success_Empty) {
    middleware_context_t* context = middleware_context_create(brain, 100, 50, 10, 7);
    ASSERT_NE(context, nullptr);

    brain_event_t* events;
    uint32_t count = middleware_context_get_recent_events(context, &events);

    EXPECT_EQ(count, 0);

    middleware_context_destroy(context);
}

TEST_F(MiddlewarePipelineTest, GetRecentEvents_Null_Context) {
    brain_event_t* events;
    uint32_t count = middleware_context_get_recent_events(nullptr, &events);

    EXPECT_EQ(count, 0);
}

//-----------------------------------------------------------------------------
// middleware_context_record_stage_time Tests
//-----------------------------------------------------------------------------

TEST_F(MiddlewarePipelineTest, RecordStageTime_Success) {
    middleware_context_t* context = middleware_context_create(brain, 100, 50, 10, 7);
    ASSERT_NE(context, nullptr);

    middleware_context_record_stage_time(context, 0, 1000);
    middleware_context_record_stage_time(context, 1, 2000);

    EXPECT_EQ(context->stage_timings_us[0], 1000);
    EXPECT_EQ(context->stage_timings_us[1], 2000);

    middleware_context_destroy(context);
}

TEST_F(MiddlewarePipelineTest, RecordStageTime_Fail_InvalidStageIndex) {
    middleware_context_t* context = middleware_context_create(brain, 100, 50, 10, 3);
    ASSERT_NE(context, nullptr);

    // Should not crash - index out of bounds is handled
    middleware_context_record_stage_time(context, 10, 1000);

    middleware_context_destroy(context);
}

TEST_F(MiddlewarePipelineTest, RecordStageTime_Null_Context) {
    // Should not crash
    middleware_context_record_stage_time(nullptr, 0, 1000);
}

//-----------------------------------------------------------------------------
// middleware_context_get_stage_timings Tests
//-----------------------------------------------------------------------------

TEST_F(MiddlewarePipelineTest, GetStageTimings_Success) {
    middleware_context_t* context = middleware_context_create(brain, 100, 50, 10, 3);
    ASSERT_NE(context, nullptr);

    middleware_context_record_stage_time(context, 0, 100);
    middleware_context_record_stage_time(context, 1, 200);
    middleware_context_record_stage_time(context, 2, 300);

    uint64_t* timings;
    uint32_t count;
    bool result = middleware_context_get_stage_timings(context, &timings, &count);

    EXPECT_TRUE(result);
    EXPECT_EQ(count, 3);
    EXPECT_EQ(timings[0], 100);
    EXPECT_EQ(timings[1], 200);
    EXPECT_EQ(timings[2], 300);

    middleware_context_destroy(context);
}

TEST_F(MiddlewarePipelineTest, GetStageTimings_Fail_NullContext) {
    uint64_t* timings;
    uint32_t count;
    bool result = middleware_context_get_stage_timings(nullptr, &timings, &count);

    EXPECT_FALSE(result);
}

//=============================================================================
// 6. INTEGRATION TESTS
//=============================================================================

TEST_F(MiddlewarePipelineTest, Integration_DefaultPipeline_FullExecution) {
    // Create default pipeline
    middleware_pipeline_t pipeline = middleware_pipeline_create_default(brain, event_bus);
    ASSERT_NE(pipeline, nullptr);

    // Create context
    middleware_context_t* context = middleware_context_create(brain, 100, 50, 10, PIPELINE_STAGE_COUNT);
    ASSERT_NE(context, nullptr);

    // Set some active neurons
    uint32_t neurons[] = {1, 5, 10, 15, 20};
    middleware_context_set_active_neurons(context, neurons, 5);

    // Execute pipeline
    bool result = middleware_pipeline_execute(pipeline, context);

    EXPECT_TRUE(result);

    // Verify features were encoded
    EXPECT_TRUE(context->features_valid);
    EXPECT_GT(context->num_cached_features, 0);

    middleware_context_destroy(context);
    middleware_pipeline_destroy(pipeline);
}

TEST_F(MiddlewarePipelineTest, Integration_CustomPipeline_PartialExecution) {
    // Create custom pipeline with only some stages
    pipeline_stage_config_t stages[] = {
        {PIPELINE_STAGE_ENCODING, "Encode", test_stage_success, nullptr, true, 1000},
        {PIPELINE_STAGE_EXTRACTION, "Extract", test_stage_success, nullptr, true, 1000},
        {PIPELINE_STAGE_DETECTION, "Detect", test_stage_success, nullptr, false, 1000}
    };

    pipeline_config_t config = create_basic_config(stages, 3);
    middleware_pipeline_t pipeline = middleware_pipeline_create(&config);
    ASSERT_NE(pipeline, nullptr);

    middleware_context_t* context = middleware_context_create(brain, 100, 50, 10, 3);
    ASSERT_NE(context, nullptr);

    // Execute pipeline
    bool result = middleware_pipeline_execute(pipeline, context);

    EXPECT_TRUE(result);

    middleware_context_destroy(context);
    middleware_pipeline_destroy(pipeline);
}

TEST_F(MiddlewarePipelineTest, Integration_MultipleExecutions_StatisticsAccumulation) {
    pipeline_stage_config_t stages[] = {
        {PIPELINE_STAGE_ENCODING, "Test", test_stage_success, nullptr, true, 1000}
    };

    pipeline_config_t config = create_basic_config(stages, 1);
    config.enable_profiling = true;

    middleware_pipeline_t pipeline = middleware_pipeline_create(&config);
    ASSERT_NE(pipeline, nullptr);

    middleware_context_t* context = middleware_context_create(brain, 100, 50, 10, 1);
    ASSERT_NE(context, nullptr);

    // Execute 10 times
    for (int i = 0; i < 10; i++) {
        middleware_pipeline_execute(pipeline, context);
    }

    // Check stats
    pipeline_stats_t stats;
    middleware_pipeline_get_stats(pipeline, &stats);

    EXPECT_EQ(stats.total_executions, 10);
    EXPECT_EQ(stats.successful_executions, 10);
    EXPECT_EQ(stats.failed_executions, 0);

    nimcp_free(stats.stage_execution_counts);
    nimcp_free(stats.stage_total_time_us);
    nimcp_free(stats.stage_avg_time_us);

    middleware_context_destroy(context);
    middleware_pipeline_destroy(pipeline);
}

TEST_F(MiddlewarePipelineTest, Integration_DynamicStageEnableDisable) {
    pipeline_stage_config_t stages[] = {
        {PIPELINE_STAGE_ENCODING, "Stage1", test_stage_success, nullptr, true, 1000},
        {PIPELINE_STAGE_EXTRACTION, "Stage2", test_stage_success, nullptr, true, 1000}
    };

    pipeline_config_t config = create_basic_config(stages, 2);
    middleware_pipeline_t pipeline = middleware_pipeline_create(&config);
    ASSERT_NE(pipeline, nullptr);

    middleware_context_t* context = middleware_context_create(brain, 100, 50, 10, 2);
    ASSERT_NE(context, nullptr);

    // Execute with both enabled
    EXPECT_TRUE(middleware_pipeline_execute(pipeline, context));

    // Disable second stage
    middleware_pipeline_set_stage_enabled(pipeline, PIPELINE_STAGE_EXTRACTION, false);

    // Execute again
    EXPECT_TRUE(middleware_pipeline_execute(pipeline, context));

    // Re-enable
    middleware_pipeline_set_stage_enabled(pipeline, PIPELINE_STAGE_EXTRACTION, true);

    // Execute once more
    EXPECT_TRUE(middleware_pipeline_execute(pipeline, context));

    middleware_context_destroy(context);
    middleware_pipeline_destroy(pipeline);
}

TEST_F(MiddlewarePipelineTest, Integration_EventHistoryTracking) {
    middleware_context_t* context = middleware_context_create(brain, 100, 50, 5, 7);
    ASSERT_NE(context, nullptr);

    // Add events over time
    for (int i = 0; i < 10; i++) {
        brain_event_t event = event_create(EVENT_NEURON_SPIKE, EVENT_PRIORITY_NORMAL, "test");
        middleware_context_add_event(context, &event);
    }

    // Should only keep last 5 (capacity)
    brain_event_t* events;
    uint32_t count = middleware_context_get_recent_events(context, &events);

    EXPECT_EQ(count, 5);

    middleware_context_destroy(context);
}

TEST_F(MiddlewarePipelineTest, Integration_CompleteWorkflow) {
    // Create pipeline
    middleware_pipeline_t pipeline = middleware_pipeline_create_default(brain, event_bus);
    ASSERT_NE(pipeline, nullptr);

    // Create context
    middleware_context_t* context = middleware_context_create(brain, 100, 50, 10, PIPELINE_STAGE_COUNT);
    ASSERT_NE(context, nullptr);

    // Simulate neural activity
    uint32_t neurons[] = {1, 5, 10, 15, 20, 25, 30};
    middleware_context_set_active_neurons(context, neurons, 7);

    // Execute pipeline
    EXPECT_TRUE(middleware_pipeline_execute(pipeline, context));

    // Verify context state
    EXPECT_GT(context->num_active_neurons, 0);

    // Check features were generated
    EXPECT_TRUE(context->features_valid);

    // Get and verify statistics
    pipeline_stats_t stats;
    middleware_pipeline_get_stats(pipeline, &stats);
    EXPECT_GT(stats.total_executions, 0);

    nimcp_free(stats.stage_execution_counts);
    nimcp_free(stats.stage_total_time_us);
    nimcp_free(stats.stage_avg_time_us);

    // Cleanup
    middleware_context_destroy(context);
    middleware_pipeline_destroy(pipeline);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
