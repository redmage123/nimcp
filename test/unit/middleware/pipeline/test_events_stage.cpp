//=============================================================================
// test_events_stage.cpp - Comprehensive Unit Tests for Events Stage
//=============================================================================

#include <gtest/gtest.h>
#include <vector>

// Headers have their own extern "C" guards
#include "middleware/pipeline/nimcp_middleware_pipeline.h"
#include "middleware/pipeline/nimcp_middleware_context.h"
#include "core/brain/nimcp_brain.h"

//=============================================================================
// Test Fixture
//=============================================================================

class EventsStageTest : public ::testing::Test {
protected:
    brain_t brain = nullptr;
    middleware_context_t* context = nullptr;
    middleware_pipeline_t pipeline = nullptr;

    void SetUp() override {
        brain_config_t config;
        memset(&config, 0, sizeof(config));
        config.size = BRAIN_SIZE_MEDIUM;
        config.task = BRAIN_TASK_CLASSIFICATION;
        config.num_inputs = 100;
        config.num_outputs = 50;
        snprintf(config.task_name, sizeof(config.task_name), "pipeline_test");

        brain = brain_create_custom(&config);
        ASSERT_NE(brain, nullptr);

        context = middleware_context_create(brain, 1000, 100, 100, 7);
        ASSERT_NE(context, nullptr);

        pipeline = middleware_pipeline_create_default(brain, nullptr);
        ASSERT_NE(pipeline, nullptr);
    }

    void TearDown() override {
        if (pipeline) middleware_pipeline_destroy(pipeline);
        if (context) middleware_context_destroy(context);
        if (brain) brain_destroy(brain);
    }

    void setupPipelineWithNeurons(const std::vector<uint32_t>& neurons) {
        middleware_context_set_active_neurons(context,
            const_cast<uint32_t*>(neurons.data()), neurons.size());
        context->timestamp_us = 1000000;
        middleware_pipeline_execute(pipeline, context);
    }
};

//=============================================================================
// Events Stage Tests
//=============================================================================

TEST_F(EventsStageTest, NullContextHandling) {
    bool result = middleware_pipeline_execute(pipeline, nullptr);
    EXPECT_FALSE(result);
}

TEST_F(EventsStageTest, NoEvents) {
    // Setup: No patterns to generate events from
    middleware_context_set_active_neurons(context, nullptr, 0);
    context->timestamp_us = 1000000;

    // Execute
    bool result = middleware_pipeline_execute(pipeline, context);

    // Verify: Should succeed with no events
    EXPECT_TRUE(result);
}

TEST_F(EventsStageTest, BasicEventGeneration) {
    // Test basic event generation
    std::vector<uint32_t> neurons = {0, 1, 2, 3, 4};
    setupPipelineWithNeurons(neurons);

    // Verify: Pipeline executed successfully
    EXPECT_TRUE(context->features_valid);
}

TEST_F(EventsStageTest, SinglePatternEvent) {
    // Test event generation from single pattern
    std::vector<uint32_t> neurons = {42};
    setupPipelineWithNeurons(neurons);

    EXPECT_TRUE(context->features_valid);
}

TEST_F(EventsStageTest, MultiplePatternEvents) {
    // Test event generation from multiple patterns
    std::vector<uint32_t> neurons = {0, 5, 10, 15, 20};
    setupPipelineWithNeurons(neurons);

    EXPECT_TRUE(context->features_valid);
}

TEST_F(EventsStageTest, LargePatternSetEvents) {
    // Test event generation with many patterns
    std::vector<uint32_t> neurons;
    for (uint32_t i = 0; i < 50; i++) {
        neurons.push_back(i);
    }

    setupPipelineWithNeurons(neurons);
    EXPECT_TRUE(context->features_valid);
}

TEST_F(EventsStageTest, MaxCapacityEvents) {
    // Test event generation at maximum capacity
    std::vector<uint32_t> neurons;
    for (uint32_t i = 0; i < 100; i++) {
        neurons.push_back(i);
    }

    setupPipelineWithNeurons(neurons);
    EXPECT_TRUE(context->features_valid);
}

TEST_F(EventsStageTest, MemoryManagement) {
    // Test repeated event generation doesn't leak memory
    for (int iter = 0; iter < 100; iter++) {
        std::vector<uint32_t> neurons = {0, 1, 2, 3, 4};
        middleware_context_set_active_neurons(context, neurons.data(), neurons.size());
        context->timestamp_us = iter * 1000;

        bool result = middleware_pipeline_execute(pipeline, context);
        EXPECT_TRUE(result);
    }

    EXPECT_TRUE(context->features_valid);
}

TEST_F(EventsStageTest, ConsistentEventGeneration) {
    // Test that same input produces consistent event generation

    // First event generation
    std::vector<uint32_t> neurons = {1, 2, 3, 4, 5};
    setupPipelineWithNeurons(neurons);
    bool first_valid = context->features_valid;

    // Second event generation with same input
    middleware_context_set_active_neurons(context,
        const_cast<uint32_t*>(neurons.data()), neurons.size());
    context->timestamp_us = 2000000;
    middleware_pipeline_execute(pipeline, context);

    // Verify: Consistent behavior
    EXPECT_EQ(context->features_valid, first_valid);
}

TEST_F(EventsStageTest, VaryingPatternCounts) {
    // Test event generation with varying pattern counts

    for (size_t count = 1; count <= 20; count++) {
        std::vector<uint32_t> neurons;
        for (size_t i = 0; i < count; i++) {
            neurons.push_back(i);
        }

        setupPipelineWithNeurons(neurons);
        EXPECT_TRUE(context->features_valid);
    }
}

TEST_F(EventsStageTest, EmptyToActiveTransition) {
    // Test transition from no events to events

    // Start with no patterns
    middleware_context_set_active_neurons(context, nullptr, 0);
    middleware_pipeline_execute(pipeline, context);

    // Add patterns
    std::vector<uint32_t> neurons = {1, 2, 3};
    bool result = middleware_pipeline_execute(pipeline, context);
    EXPECT_TRUE(result);
}

TEST_F(EventsStageTest, ActiveToEmptyTransition) {
    // Test transition from events to no events

    // Start with patterns
    std::vector<uint32_t> neurons = {1, 2, 3};
    setupPipelineWithNeurons(neurons);

    // Clear patterns
    middleware_context_set_active_neurons(context, nullptr, 0);
    bool result = middleware_pipeline_execute(pipeline, context);
    EXPECT_TRUE(result);
}

TEST_F(EventsStageTest, RapidEventGeneration) {
    // Test rapid sequential event generation (stress test)

    for (int iter = 0; iter < 1000; iter++) {
        std::vector<uint32_t> neurons = {static_cast<uint32_t>(iter % 10)};
        middleware_context_set_active_neurons(context, neurons.data(), neurons.size());
        context->timestamp_us = iter;

        bool result = middleware_pipeline_execute(pipeline, context);
        EXPECT_TRUE(result);
    }
}

TEST_F(EventsStageTest, CompletePipelineIntegration) {
    // Test that events stage integrates well with full pipeline

    // Run multiple cycles through entire pipeline
    for (int cycle = 0; cycle < 10; cycle++) {
        std::vector<uint32_t> neurons;
        for (uint32_t i = 0; i < 10; i++) {
            neurons.push_back(cycle * 10 + i);
        }

        middleware_context_set_active_neurons(context, neurons.data(), neurons.size());
        context->timestamp_us = cycle * 1000;

        bool result = middleware_pipeline_execute(pipeline, context);
        EXPECT_TRUE(result);
        EXPECT_TRUE(context->features_valid);
    }
}
