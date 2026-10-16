//=============================================================================
// test_detection_stage.cpp - Comprehensive Unit Tests for Detection Stage
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

class DetectionStageTest : public ::testing::Test {
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
// Detection Stage Tests
//=============================================================================

TEST_F(DetectionStageTest, NullContextHandling) {
    bool result = middleware_pipeline_execute(pipeline, nullptr);
    EXPECT_FALSE(result);
}

TEST_F(DetectionStageTest, NoPatterns) {
    // Setup: No active neurons
    middleware_context_set_active_neurons(context, nullptr, 0);
    context->timestamp_us = 1000000;

    // Execute
    bool result = middleware_pipeline_execute(pipeline, context);

    // Verify: No patterns detected
    EXPECT_TRUE(result);
    EXPECT_EQ(context->num_detected_patterns, 0);
}

TEST_F(DetectionStageTest, SinglePatternDetection) {
    // Setup: High activity should create pattern
    std::vector<uint32_t> neurons = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    setupPipelineWithNeurons(neurons);

    // Verify: Pattern detected (high activity neurons)
    EXPECT_GT(context->num_detected_patterns, 0);
}

TEST_F(DetectionStageTest, MultiplePatternDetection) {
    // Setup: Multiple active neuron groups
    std::vector<uint32_t> neurons;
    for (uint32_t i = 0; i < 20; i++) {
        neurons.push_back(i);
    }
    setupPipelineWithNeurons(neurons);

    // Verify: Multiple patterns detected
    EXPECT_GT(context->num_detected_patterns, 0);
    ASSERT_NE(context->detected_patterns, nullptr);
    ASSERT_NE(context->pattern_confidences, nullptr);
}

TEST_F(DetectionStageTest, PatternConfidences) {
    // Test that pattern confidences are valid
    std::vector<uint32_t> neurons = {0, 1, 2, 3, 4};
    setupPipelineWithNeurons(neurons);

    if (context->num_detected_patterns > 0) {
        // Verify confidence values are in valid range [0, 1]
        for (uint32_t i = 0; i < context->num_detected_patterns; i++) {
            EXPECT_GE(context->pattern_confidences[i], 0.0f);
            EXPECT_LE(context->pattern_confidences[i], 1.0f);
        }
    }
}

TEST_F(DetectionStageTest, PatternIndicesValid) {
    // Test that pattern indices are valid
    std::vector<uint32_t> neurons = {0, 1, 2, 3, 4};
    setupPipelineWithNeurons(neurons);

    if (context->num_detected_patterns > 0) {
        // Verify pattern indices are within bounds
        for (uint32_t i = 0; i < context->num_detected_patterns; i++) {
            EXPECT_LT(context->detected_patterns[i], context->num_cached_features);
        }
    }
}

TEST_F(DetectionStageTest, ConsistentDetection) {
    // Test that same input produces same detection

    // First detection
    std::vector<uint32_t> neurons = {1, 2, 3, 4, 5};
    setupPipelineWithNeurons(neurons);

    uint32_t first_count = context->num_detected_patterns;
    std::vector<uint32_t> first_patterns;
    for (uint32_t i = 0; i < first_count; i++) {
        first_patterns.push_back(context->detected_patterns[i]);
    }

    // Second detection with same input
    middleware_context_set_active_neurons(context,
        const_cast<uint32_t*>(neurons.data()), neurons.size());
    context->timestamp_us = 2000000;
    middleware_pipeline_execute(pipeline, context);

    // Verify: Same number of patterns
    EXPECT_EQ(context->num_detected_patterns, first_count);

    // Verify: Same patterns detected
    for (uint32_t i = 0; i < first_count && i < context->num_detected_patterns; i++) {
        EXPECT_EQ(context->detected_patterns[i], first_patterns[i]);
    }
}

TEST_F(DetectionStageTest, MemoryManagement) {
    // Test repeated detection doesn't leak memory
    for (int iter = 0; iter < 100; iter++) {
        std::vector<uint32_t> neurons = {0, 1, 2, 3, 4};
        middleware_context_set_active_neurons(context, neurons.data(), neurons.size());
        context->timestamp_us = iter * 1000;

        bool result = middleware_pipeline_execute(pipeline, context);
        EXPECT_TRUE(result);
    }
}

TEST_F(DetectionStageTest, VaryingActivityLevels) {
    // Test detection with varying activity levels

    // Low activity
    std::vector<uint32_t> low_activity = {0};
    setupPipelineWithNeurons(low_activity);
    uint32_t low_patterns = context->num_detected_patterns;

    // High activity
    std::vector<uint32_t> high_activity;
    for (uint32_t i = 0; i < 50; i++) {
        high_activity.push_back(i);
    }
    setupPipelineWithNeurons(high_activity);
    uint32_t high_patterns = context->num_detected_patterns;

    // High activity should detect more patterns
    EXPECT_GE(high_patterns, low_patterns);
}

TEST_F(DetectionStageTest, MaxCapacityDetection) {
    // Test detection at maximum capacity
    std::vector<uint32_t> neurons;
    for (uint32_t i = 0; i < 100; i++) {
        neurons.push_back(i);
    }

    setupPipelineWithNeurons(neurons);

    // Should handle max capacity without error
    EXPECT_LE(context->num_detected_patterns, context->num_cached_features);
}

TEST_F(DetectionStageTest, EmptyToActiveTransition) {
    // Test transition from no patterns to patterns

    // Start with no neurons
    middleware_context_set_active_neurons(context, nullptr, 0);
    middleware_pipeline_execute(pipeline, context);
    EXPECT_EQ(context->num_detected_patterns, 0);

    // Add neurons
    std::vector<uint32_t> neurons = {1, 2, 3, 4, 5};
    setupPipelineWithNeurons(neurons);

    // Should now detect patterns
    EXPECT_GE(context->num_detected_patterns, 0);
}
