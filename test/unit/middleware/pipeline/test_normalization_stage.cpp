//=============================================================================
// test_normalization_stage.cpp - Comprehensive Unit Tests for Normalization Stage
//=============================================================================

#include <gtest/gtest.h>
#include <vector>
#include <cmath>

// Headers have their own extern "C" guards
#include "middleware/pipeline/nimcp_middleware_pipeline.h"
#include "middleware/pipeline/nimcp_middleware_context.h"
#include "core/brain/nimcp_brain.h"

//=============================================================================
// Test Fixture
//=============================================================================

class NormalizationStageTest : public ::testing::Test {
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

    // Calculate mean of features
    float calculateMean() {
        if (context->num_cached_features == 0) return 0.0f;

        float sum = 0.0f;
        for (uint32_t i = 0; i < context->num_cached_features; i++) {
            sum += context->cached_features[i];
        }
        return sum / context->num_cached_features;
    }

    // Calculate standard deviation of features
    float calculateStdDev(float mean) {
        if (context->num_cached_features == 0) return 0.0f;

        float sum_sq = 0.0f;
        for (uint32_t i = 0; i < context->num_cached_features; i++) {
            float diff = context->cached_features[i] - mean;
            sum_sq += diff * diff;
        }
        return std::sqrt(sum_sq / context->num_cached_features);
    }
};

//=============================================================================
// Normalization Stage Tests
//=============================================================================

TEST_F(NormalizationStageTest, NullContextHandling) {
    bool result = middleware_pipeline_execute(pipeline, nullptr);
    EXPECT_FALSE(result);
}

TEST_F(NormalizationStageTest, EmptyFeatures) {
    // Setup: No features to normalize
    middleware_context_set_active_neurons(context, nullptr, 0);
    context->timestamp_us = 1000000;

    // Execute
    bool result = middleware_pipeline_execute(pipeline, context);

    // Verify: Should succeed with no normalization
    EXPECT_TRUE(result);
}

TEST_F(NormalizationStageTest, ZScoreNormalization) {
    // Test that z-score normalization produces mean ~0 and std ~1

    std::vector<uint32_t> neurons = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    setupPipelineWithNeurons(neurons);

    // Calculate statistics after normalization
    float mean = calculateMean();
    float std_dev = calculateStdDev(mean);

    // After z-score normalization:
    // - mean should be close to 0
    // - std deviation should be close to 1
    EXPECT_NEAR(mean, 0.0f, 0.1f);
    EXPECT_NEAR(std_dev, 1.0f, 0.2f);
}

TEST_F(NormalizationStageTest, SingleFeatureNormalization) {
    // Test normalization with single feature
    std::vector<uint32_t> neurons = {42};
    setupPipelineWithNeurons(neurons);

    // Should handle single feature without divide-by-zero
    EXPECT_TRUE(context->features_valid);
    ASSERT_NE(context->cached_features, nullptr);
}

TEST_F(NormalizationStageTest, MultipleFeatureNormalization) {
    // Test normalization with multiple features
    std::vector<uint32_t> neurons = {0, 5, 10, 15, 20};
    setupPipelineWithNeurons(neurons);

    EXPECT_TRUE(context->features_valid);
    EXPECT_EQ(context->num_cached_features, 5);

    // Verify all features are normalized (finite values)
    for (uint32_t i = 0; i < context->num_cached_features; i++) {
        EXPECT_TRUE(std::isfinite(context->cached_features[i]));
    }
}

TEST_F(NormalizationStageTest, MaxCapacityNormalization) {
    // Test normalization at maximum capacity
    std::vector<uint32_t> neurons;
    for (uint32_t i = 0; i < 100; i++) {
        neurons.push_back(i);
    }

    setupPipelineWithNeurons(neurons);

    EXPECT_TRUE(context->features_valid);
    EXPECT_EQ(context->num_cached_features, 100);

    // Verify normalization properties
    float mean = calculateMean();
    float std_dev = calculateStdDev(mean);
    EXPECT_NEAR(mean, 0.0f, 0.1f);
    EXPECT_NEAR(std_dev, 1.0f, 0.2f);
}

TEST_F(NormalizationStageTest, NumericalStability) {
    // Test that normalization handles edge cases without NaN/Inf

    // Test with very small variance
    std::vector<uint32_t> neurons = {1, 2, 3};
    setupPipelineWithNeurons(neurons);

    // Verify no NaN or Inf values
    for (uint32_t i = 0; i < context->num_cached_features; i++) {
        EXPECT_TRUE(std::isfinite(context->cached_features[i]));
        EXPECT_FALSE(std::isnan(context->cached_features[i]));
        EXPECT_FALSE(std::isinf(context->cached_features[i]));
    }
}

TEST_F(NormalizationStageTest, ConsistentNormalization) {
    // Test that same input produces consistent normalized output

    // First normalization
    std::vector<uint32_t> neurons = {1, 2, 3, 4, 5};
    setupPipelineWithNeurons(neurons);

    std::vector<float> first_normalized;
    for (uint32_t i = 0; i < context->num_cached_features; i++) {
        first_normalized.push_back(context->cached_features[i]);
    }

    // Second normalization with same input
    middleware_context_set_active_neurons(context,
        const_cast<uint32_t*>(neurons.data()), neurons.size());
    context->timestamp_us = 2000000;
    middleware_pipeline_execute(pipeline, context);

    // Verify: Same normalized values
    EXPECT_EQ(context->num_cached_features, first_normalized.size());
    for (size_t i = 0; i < first_normalized.size(); i++) {
        EXPECT_FLOAT_EQ(context->cached_features[i], first_normalized[i]);
    }
}

TEST_F(NormalizationStageTest, MemoryManagement) {
    // Test repeated normalization doesn't leak memory
    for (int iter = 0; iter < 100; iter++) {
        std::vector<uint32_t> neurons = {0, 1, 2, 3, 4};
        middleware_context_set_active_neurons(context, neurons.data(), neurons.size());
        context->timestamp_us = iter * 1000;

        bool result = middleware_pipeline_execute(pipeline, context);
        EXPECT_TRUE(result);
    }

    EXPECT_TRUE(context->features_valid);
}

TEST_F(NormalizationStageTest, VaryingInputSizes) {
    // Test normalization with varying input sizes

    for (size_t count = 2; count <= 20; count++) {
        std::vector<uint32_t> neurons;
        for (size_t i = 0; i < count; i++) {
            neurons.push_back(i);
        }

        setupPipelineWithNeurons(neurons);

        EXPECT_TRUE(context->features_valid);
        EXPECT_EQ(context->num_cached_features, count);

        // Verify normalization properties
        float mean = calculateMean();
        EXPECT_NEAR(mean, 0.0f, 0.1f);
    }
}

TEST_F(NormalizationStageTest, PreservesRelativeOrder) {
    // Test that normalization preserves relative ordering of features

    std::vector<uint32_t> neurons = {0, 1, 2, 3, 4};
    setupPipelineWithNeurons(neurons);

    // Encoding produces increasing values, so after normalization
    // they should maintain relative ordering (monotonically increasing)
    if (context->num_cached_features > 1) {
        for (uint32_t i = 1; i < context->num_cached_features; i++) {
            // Each successive feature should be >= previous (monotonic)
            EXPECT_GE(context->cached_features[i], context->cached_features[i-1]);
        }
    }
}
