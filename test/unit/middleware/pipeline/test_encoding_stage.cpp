//=============================================================================
// test_encoding_stage.cpp - Comprehensive Unit Tests for Encoding Stage
//=============================================================================

#include <gtest/gtest.h>
#include <vector>
#include <cmath>

// Headers have their own extern "C" guards
#include "middleware/pipeline/nimcp_middleware_pipeline.h"
#include "middleware/pipeline/nimcp_middleware_context.h"
#include "middleware/encoding/nimcp_rate_coding.h"
#include "core/brain/nimcp_brain.h"

//=============================================================================
// Test Fixture
//=============================================================================

class EncodingStageTest : public ::testing::Test {
protected:
    brain_t brain = nullptr;
    middleware_context_t* context = nullptr;
    middleware_pipeline_t pipeline = nullptr;

    void SetUp() override {
        // Create minimal brain for testing
        brain_config_t config;
        memset(&config, 0, sizeof(config));
        config.size = BRAIN_SIZE_MEDIUM;
        config.task = BRAIN_TASK_CLASSIFICATION;
        config.num_inputs = 100;
        config.num_outputs = 50;
        snprintf(config.task_name, sizeof(config.task_name), "pipeline_test");

        brain = brain_create_custom(&config);
        ASSERT_NE(brain, nullptr);

        // Create middleware context
        context = middleware_context_create(brain, 1000, 100, 100, 7);
        ASSERT_NE(context, nullptr);

        // Create pipeline
        pipeline = middleware_pipeline_create_default(brain, nullptr);
        ASSERT_NE(pipeline, nullptr);
    }

    void TearDown() override {
        if (pipeline) middleware_pipeline_destroy(pipeline);
        if (context) middleware_context_destroy(context);
        if (brain) brain_destroy(brain);
    }
};

//=============================================================================
// Encoding Stage Tests
//=============================================================================

TEST_F(EncodingStageTest, NullContextHandling) {
    // Test NULL context - should handle gracefully
    bool result = middleware_pipeline_execute(pipeline, nullptr);
    EXPECT_FALSE(result);
}

TEST_F(EncodingStageTest, EmptyNeuronList) {
    // Setup: No active neurons
    middleware_context_set_active_neurons(context, nullptr, 0);
    context->timestamp_us = 1000000;

    // Execute: Run pipeline
    bool result = middleware_pipeline_execute(pipeline, context);

    // Verify: Should succeed but features should be invalid
    EXPECT_TRUE(result);
    EXPECT_FALSE(context->features_valid);
    EXPECT_EQ(context->num_cached_features, 0);
}

TEST_F(EncodingStageTest, SingleActiveNeuron) {
    // Setup: Single active neuron
    uint32_t active_neurons[] = {42};
    middleware_context_set_active_neurons(context, active_neurons, 1);
    context->timestamp_us = 1000000;

    // Execute: Run pipeline
    bool result = middleware_pipeline_execute(pipeline, context);

    // Verify: Should encode successfully
    EXPECT_TRUE(result);
    EXPECT_TRUE(context->features_valid);
    EXPECT_GT(context->num_cached_features, 0);

    // Verify features are valid (after full pipeline including normalization)
    ASSERT_NE(context->cached_features, nullptr);
    EXPECT_TRUE(std::isfinite(context->cached_features[0]));
}

TEST_F(EncodingStageTest, MultipleActiveNeurons) {
    // Setup: Multiple active neurons
    uint32_t active_neurons[] = {0, 5, 10, 15, 20};
    middleware_context_set_active_neurons(context, active_neurons, 5);
    context->timestamp_us = 1000000;

    // Execute: Run pipeline
    bool result = middleware_pipeline_execute(pipeline, context);

    // Verify: All neurons encoded
    EXPECT_TRUE(result);
    EXPECT_TRUE(context->features_valid);
    EXPECT_EQ(context->num_cached_features, 5);

    // Verify all features are valid (after full pipeline including normalization)
    // Features will be z-scores, so check they're finite instead of [0,1] range
    for (uint32_t i = 0; i < context->num_cached_features; i++) {
        EXPECT_TRUE(std::isfinite(context->cached_features[i]));
    }
}

TEST_F(EncodingStageTest, MaxCapacityNeurons) {
    // Setup: Maximum capacity
    std::vector<uint32_t> active_neurons;
    for (uint32_t i = 0; i < 100; i++) {
        active_neurons.push_back(i);
    }

    middleware_context_set_active_neurons(context, active_neurons.data(), 100);
    context->timestamp_us = 1000000;

    // Execute: Run pipeline
    bool result = middleware_pipeline_execute(pipeline, context);

    // Verify: All encoded successfully
    EXPECT_TRUE(result);
    EXPECT_TRUE(context->features_valid);
    EXPECT_EQ(context->num_cached_features, 100);
}

TEST_F(EncodingStageTest, ConsistentEncoding) {
    // Test that same input produces same output
    uint32_t active_neurons[] = {1, 2, 3, 4, 5};

    // First encoding
    middleware_context_set_active_neurons(context, active_neurons, 5);
    context->timestamp_us = 1000000;
    middleware_pipeline_execute(pipeline, context);

    std::vector<float> first_encoding;
    for (uint32_t i = 0; i < context->num_cached_features; i++) {
        first_encoding.push_back(context->cached_features[i]);
    }

    // Second encoding with same input
    middleware_context_set_active_neurons(context, active_neurons, 5);
    context->timestamp_us = 2000000;
    middleware_pipeline_execute(pipeline, context);

    // Verify: Same encoding results
    EXPECT_EQ(context->num_cached_features, first_encoding.size());
    for (size_t i = 0; i < first_encoding.size(); i++) {
        EXPECT_FLOAT_EQ(context->cached_features[i], first_encoding[i]);
    }
}

TEST_F(EncodingStageTest, MemoryManagement) {
    // Test multiple encoding cycles don't leak memory
    for (int iter = 0; iter < 100; iter++) {
        uint32_t active_neurons[] = {0, 1, 2, 3, 4};
        middleware_context_set_active_neurons(context, active_neurons, 5);
        context->timestamp_us = iter * 1000;

        bool result = middleware_pipeline_execute(pipeline, context);
        EXPECT_TRUE(result);
    }

    // Verify final state is valid
    EXPECT_TRUE(context->features_valid);
}

TEST_F(EncodingStageTest, ZeroToNonZeroTransition) {
    // Test transition from zero to non-zero active neurons

    // Start with no neurons
    middleware_context_set_active_neurons(context, nullptr, 0);
    middleware_pipeline_execute(pipeline, context);
    EXPECT_FALSE(context->features_valid);

    // Add active neurons
    uint32_t active_neurons[] = {1, 2, 3};
    middleware_context_set_active_neurons(context, active_neurons, 3);
    bool result = middleware_pipeline_execute(pipeline, context);

    EXPECT_TRUE(result);
    EXPECT_TRUE(context->features_valid);
    EXPECT_EQ(context->num_cached_features, 3);
}

TEST_F(EncodingStageTest, NonZeroToZeroTransition) {
    // Test transition from active to no active neurons

    // Start with active neurons
    uint32_t active_neurons[] = {1, 2, 3};
    middleware_context_set_active_neurons(context, active_neurons, 3);
    middleware_pipeline_execute(pipeline, context);
    EXPECT_TRUE(context->features_valid);

    // Clear neurons
    middleware_context_set_active_neurons(context, nullptr, 0);
    bool result = middleware_pipeline_execute(pipeline, context);

    EXPECT_TRUE(result);
    EXPECT_FALSE(context->features_valid);
}
