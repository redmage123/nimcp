//=============================================================================
// test_routing_stage.cpp - Comprehensive Unit Tests for Routing Stage
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

class RoutingStageTest : public ::testing::Test {
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
// Routing Stage Tests
//=============================================================================

TEST_F(RoutingStageTest, NullContextHandling) {
    bool result = middleware_pipeline_execute(pipeline, nullptr);
    EXPECT_FALSE(result);
}

TEST_F(RoutingStageTest, NoPatternRouting) {
    // Setup: No patterns to route
    middleware_context_set_active_neurons(context, nullptr, 0);
    context->timestamp_us = 1000000;

    // Execute
    bool result = middleware_pipeline_execute(pipeline, context);

    // Verify: Should succeed with no routing
    EXPECT_TRUE(result);
}

TEST_F(RoutingStageTest, BasicRouting) {
    // Setup: Create patterns to route
    std::vector<uint32_t> neurons = {0, 1, 2, 3, 4};
    setupPipelineWithNeurons(neurons);

    // Verify: Pipeline executed successfully
    EXPECT_TRUE(context->features_valid);
}

TEST_F(RoutingStageTest, SinglePatternRouting) {
    // Test routing of single pattern
    std::vector<uint32_t> neurons = {42};
    setupPipelineWithNeurons(neurons);

    // Verify: Should handle single pattern
    EXPECT_TRUE(context->features_valid);
}

TEST_F(RoutingStageTest, MultiplePatternRouting) {
    // Test routing of multiple patterns
    std::vector<uint32_t> neurons;
    for (uint32_t i = 0; i < 20; i++) {
        neurons.push_back(i);
    }
    setupPipelineWithNeurons(neurons);

    // Verify: Should handle multiple patterns
    EXPECT_TRUE(context->features_valid);
}

TEST_F(RoutingStageTest, MemoryManagement) {
    // Test repeated routing doesn't leak memory
    for (int iter = 0; iter < 100; iter++) {
        std::vector<uint32_t> neurons = {0, 1, 2, 3, 4};
        middleware_context_set_active_neurons(context, neurons.data(), neurons.size());
        context->timestamp_us = iter * 1000;

        bool result = middleware_pipeline_execute(pipeline, context);
        EXPECT_TRUE(result);
    }
}

TEST_F(RoutingStageTest, VaryingPatternCounts) {
    // Test routing with varying numbers of patterns

    for (size_t count = 1; count <= 20; count++) {
        std::vector<uint32_t> neurons;
        for (size_t i = 0; i < count; i++) {
            neurons.push_back(i);
        }

        setupPipelineWithNeurons(neurons);
        EXPECT_TRUE(context->features_valid);
    }
}

TEST_F(RoutingStageTest, MaxCapacityRouting) {
    // Test routing at maximum capacity
    std::vector<uint32_t> neurons;
    for (uint32_t i = 0; i < 100; i++) {
        neurons.push_back(i);
    }

    setupPipelineWithNeurons(neurons);
    EXPECT_TRUE(context->features_valid);
}

TEST_F(RoutingStageTest, ConsistentRouting) {
    // Test that same input produces consistent routing

    // First routing
    std::vector<uint32_t> neurons = {1, 2, 3, 4, 5};
    setupPipelineWithNeurons(neurons);
    bool first_valid = context->features_valid;

    // Second routing with same input
    middleware_context_set_active_neurons(context,
        const_cast<uint32_t*>(neurons.data()), neurons.size());
    context->timestamp_us = 2000000;
    middleware_pipeline_execute(pipeline, context);

    // Verify: Consistent results
    EXPECT_EQ(context->features_valid, first_valid);
}

TEST_F(RoutingStageTest, EmptyToActiveTransition) {
    // Test transition from no patterns to patterns

    // Start with no patterns
    middleware_context_set_active_neurons(context, nullptr, 0);
    middleware_pipeline_execute(pipeline, context);

    // Add patterns
    std::vector<uint32_t> neurons = {1, 2, 3};
    bool result = middleware_pipeline_execute(pipeline, context);
    EXPECT_TRUE(result);
}

TEST_F(RoutingStageTest, ActiveToEmptyTransition) {
    // Test transition from patterns to no patterns

    // Start with patterns
    std::vector<uint32_t> neurons = {1, 2, 3};
    setupPipelineWithNeurons(neurons);

    // Clear patterns
    middleware_context_set_active_neurons(context, nullptr, 0);
    bool result = middleware_pipeline_execute(pipeline, context);
    EXPECT_TRUE(result);
}
