//=============================================================================
// test_extraction_stage.cpp - Comprehensive Unit Tests for Extraction Stage
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

class ExtractionStageTest : public ::testing::Test {
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

    // Helper to run encoding stage first
    void setupEncodedFeatures(const std::vector<uint32_t>& neurons) {
        middleware_context_set_active_neurons(context,
            const_cast<uint32_t*>(neurons.data()), neurons.size());
        context->timestamp_us = 1000000;
        middleware_pipeline_execute(pipeline, context);
    }
};

//=============================================================================
// Extraction Stage Tests
//=============================================================================

TEST_F(ExtractionStageTest, NullContextHandling) {
    bool result = middleware_pipeline_execute(pipeline, nullptr);
    EXPECT_FALSE(result);
}

TEST_F(ExtractionStageTest, EmptyFeatureList) {
    // Setup: No features to extract from
    middleware_context_set_active_neurons(context, nullptr, 0);
    context->timestamp_us = 1000000;

    // Execute
    bool result = middleware_pipeline_execute(pipeline, context);

    // Verify: Should succeed but no features extracted
    EXPECT_TRUE(result);
}

TEST_F(ExtractionStageTest, SingleFeatureExtraction) {
    // Setup: Single neuron produces single feature
    std::vector<uint32_t> neurons = {42};
    setupEncodedFeatures(neurons);

    // Verify: Feature extraction succeeded
    EXPECT_TRUE(context->features_valid);
    EXPECT_GT(context->num_cached_features, 0);
}

TEST_F(ExtractionStageTest, MultipleFeatureExtraction) {
    // Setup: Multiple neurons
    std::vector<uint32_t> neurons = {0, 5, 10, 15, 20};
    setupEncodedFeatures(neurons);

    // Verify: All features extracted
    EXPECT_TRUE(context->features_valid);
    EXPECT_EQ(context->num_cached_features, 5);
}

TEST_F(ExtractionStageTest, StatisticalPropertiesPreserved) {
    // Test that extraction preserves statistical properties

    // Setup: Known pattern of neurons
    std::vector<uint32_t> neurons = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    setupEncodedFeatures(neurons);

    // Verify: Features are valid
    EXPECT_TRUE(context->features_valid);
    EXPECT_GT(context->num_cached_features, 0);

    // Calculate basic statistics
    float sum = 0.0f;
    for (uint32_t i = 0; i < context->num_cached_features; i++) {
        sum += context->cached_features[i];
    }
    float mean = sum / context->num_cached_features;

    // Mean should be within reasonable bounds
    EXPECT_GE(mean, 0.0f);
    EXPECT_LE(mean, 1.0f);
}

TEST_F(ExtractionStageTest, MaxCapacityExtraction) {
    // Test extraction at maximum capacity
    std::vector<uint32_t> neurons;
    for (uint32_t i = 0; i < 100; i++) {
        neurons.push_back(i);
    }

    setupEncodedFeatures(neurons);

    EXPECT_TRUE(context->features_valid);
    EXPECT_EQ(context->num_cached_features, 100);
}

TEST_F(ExtractionStageTest, ConsistentExtraction) {
    // Test that same input produces consistent extraction

    // First extraction
    std::vector<uint32_t> neurons = {1, 2, 3, 4, 5};
    setupEncodedFeatures(neurons);

    std::vector<float> first_features;
    for (uint32_t i = 0; i < context->num_cached_features; i++) {
        first_features.push_back(context->cached_features[i]);
    }

    // Reset and extract again
    middleware_context_set_active_neurons(context,
        const_cast<uint32_t*>(neurons.data()), neurons.size());
    context->timestamp_us = 2000000;
    middleware_pipeline_execute(pipeline, context);

    // Verify: Consistent results
    EXPECT_EQ(context->num_cached_features, first_features.size());
    for (size_t i = 0; i < first_features.size(); i++) {
        EXPECT_FLOAT_EQ(context->cached_features[i], first_features[i]);
    }
}

TEST_F(ExtractionStageTest, MemoryManagement) {
    // Test repeated extraction doesn't leak memory
    for (int iter = 0; iter < 100; iter++) {
        std::vector<uint32_t> neurons = {0, 1, 2, 3, 4};
        middleware_context_set_active_neurons(context, neurons.data(), neurons.size());
        context->timestamp_us = iter * 1000;

        bool result = middleware_pipeline_execute(pipeline, context);
        EXPECT_TRUE(result);
    }

    EXPECT_TRUE(context->features_valid);
}

TEST_F(ExtractionStageTest, VaryingInputSizes) {
    // Test extraction with varying input sizes

    for (size_t count = 1; count <= 20; count++) {
        std::vector<uint32_t> neurons;
        for (size_t i = 0; i < count; i++) {
            neurons.push_back(i);
        }

        setupEncodedFeatures(neurons);

        EXPECT_TRUE(context->features_valid);
        EXPECT_EQ(context->num_cached_features, count);
    }
}
