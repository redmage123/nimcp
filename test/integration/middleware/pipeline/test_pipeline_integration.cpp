//=============================================================================
// test_pipeline_integration.cpp - End-to-End Pipeline Integration Tests
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

class PipelineIntegrationTest : public ::testing::Test {
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
        snprintf(config.task_name, sizeof(config.task_name), "pipeline_integration");

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

    // Helper: Execute pipeline with neuron activity
    bool executePipeline(const std::vector<uint32_t>& neurons, uint64_t timestamp) {
        middleware_context_set_active_neurons(context,
            const_cast<uint32_t*>(neurons.data()), neurons.size());
        context->timestamp_us = timestamp;
        return middleware_pipeline_execute(pipeline, context);
    }
};

//=============================================================================
// END-TO-END INTEGRATION TESTS
//=============================================================================

TEST_F(PipelineIntegrationTest, CompleteDataFlow) {
    // Test: Data flows correctly through all 7 stages

    // Setup: Create realistic neuron activity pattern
    std::vector<uint32_t> neurons = {0, 5, 10, 15, 20, 25, 30};

    // Execute: Run through entire pipeline
    bool result = executePipeline(neurons, 1000000);

    // Verify: Pipeline executed successfully
    EXPECT_TRUE(result);

    // Verify: Encoding stage produced valid features
    EXPECT_TRUE(context->features_valid);
    EXPECT_EQ(context->num_cached_features, neurons.size());
    ASSERT_NE(context->cached_features, nullptr);

    // Verify: Features are normalized (finite z-scores)
    for (uint32_t i = 0; i < context->num_cached_features; i++) {
        EXPECT_TRUE(std::isfinite(context->cached_features[i]));
    }

    // Verify: Detection stage found patterns
    // (After normalization, about half should be above mean)
    EXPECT_GT(context->num_detected_patterns, 0);
    EXPECT_LE(context->num_detected_patterns, context->num_cached_features);
}

TEST_F(PipelineIntegrationTest, MultipleSequentialExecutions) {
    // Test: Pipeline handles multiple sequential executions correctly

    std::vector<uint32_t> pattern1 = {0, 1, 2, 3, 4};
    std::vector<uint32_t> pattern2 = {10, 11, 12, 13, 14};
    std::vector<uint32_t> pattern3 = {20, 21, 22, 23, 24};

    // Execute: Run pipeline multiple times
    EXPECT_TRUE(executePipeline(pattern1, 1000000));
    uint32_t patterns1 = context->num_detected_patterns;

    EXPECT_TRUE(executePipeline(pattern2, 2000000));
    uint32_t patterns2 = context->num_detected_patterns;

    EXPECT_TRUE(executePipeline(pattern3, 3000000));
    uint32_t patterns3 = context->num_detected_patterns;

    // Verify: Each execution produced consistent results
    EXPECT_EQ(patterns1, patterns2);
    EXPECT_EQ(patterns2, patterns3);
}

TEST_F(PipelineIntegrationTest, VaryingActivityLevels) {
    // Test: Pipeline handles varying levels of neural activity

    // Low activity
    std::vector<uint32_t> low_activity = {42};
    EXPECT_TRUE(executePipeline(low_activity, 1000000));
    uint32_t low_patterns = context->num_detected_patterns;

    // Medium activity
    std::vector<uint32_t> medium_activity = {0, 5, 10, 15, 20};
    EXPECT_TRUE(executePipeline(medium_activity, 2000000));
    uint32_t medium_patterns = context->num_detected_patterns;

    // High activity
    std::vector<uint32_t> high_activity;
    for (uint32_t i = 0; i < 50; i++) high_activity.push_back(i);
    EXPECT_TRUE(executePipeline(high_activity, 3000000));
    uint32_t high_patterns = context->num_detected_patterns;

    // Verify: Higher activity should detect more patterns
    EXPECT_LE(low_patterns, medium_patterns);
    EXPECT_LE(medium_patterns, high_patterns);
}

TEST_F(PipelineIntegrationTest, EmptyToActiveTransitions) {
    // Test: Pipeline handles transitions between empty and active states

    // Start with no activity
    std::vector<uint32_t> no_neurons;
    EXPECT_TRUE(executePipeline(no_neurons, 1000000));
    EXPECT_EQ(context->num_detected_patterns, 0);

    // Transition to active
    std::vector<uint32_t> active_neurons = {0, 1, 2, 3, 4};
    EXPECT_TRUE(executePipeline(active_neurons, 2000000));
    EXPECT_GT(context->num_detected_patterns, 0);

    // Back to no activity
    EXPECT_TRUE(executePipeline(no_neurons, 3000000));
    EXPECT_EQ(context->num_detected_patterns, 0);

    // Active again
    EXPECT_TRUE(executePipeline(active_neurons, 4000000));
    EXPECT_GT(context->num_detected_patterns, 0);
}

TEST_F(PipelineIntegrationTest, TemporalConsistency) {
    // Test: Pipeline produces temporally consistent results

    std::vector<uint32_t> neurons = {5, 10, 15, 20, 25};

    // Execute at different timestamps
    EXPECT_TRUE(executePipeline(neurons, 1000000));
    std::vector<float> features1(context->cached_features,
        context->cached_features + context->num_cached_features);

    EXPECT_TRUE(executePipeline(neurons, 5000000));
    std::vector<float> features2(context->cached_features,
        context->cached_features + context->num_cached_features);

    // Verify: Same input produces same normalized output
    ASSERT_EQ(features1.size(), features2.size());
    for (size_t i = 0; i < features1.size(); i++) {
        EXPECT_FLOAT_EQ(features1[i], features2[i]);
    }
}

TEST_F(PipelineIntegrationTest, MaxCapacityHandling) {
    // Test: Pipeline handles maximum capacity without errors

    std::vector<uint32_t> max_neurons;
    for (uint32_t i = 0; i < 100; i++) {
        max_neurons.push_back(i);
    }

    // Execute: Run with maximum neuron count
    EXPECT_TRUE(executePipeline(max_neurons, 1000000));

    // Verify: All stages completed successfully
    EXPECT_TRUE(context->features_valid);
    EXPECT_EQ(context->num_cached_features, 100);
    EXPECT_LE(context->num_detected_patterns, 100);

    // Verify: No memory corruption
    for (uint32_t i = 0; i < context->num_cached_features; i++) {
        EXPECT_TRUE(std::isfinite(context->cached_features[i]));
    }
}

TEST_F(PipelineIntegrationTest, ErrorRecovery) {
    // Test: Pipeline recovers from edge cases gracefully

    // Execute with NULL neurons pointer but 0 count (valid edge case)
    EXPECT_TRUE(executePipeline({}, 1000000));

    // Execute with valid data after edge case
    std::vector<uint32_t> neurons = {0, 1, 2, 3, 4};
    EXPECT_TRUE(executePipeline(neurons, 2000000));
    EXPECT_TRUE(context->features_valid);
    EXPECT_GT(context->num_detected_patterns, 0);
}

TEST_F(PipelineIntegrationTest, NormalizationIntegrity) {
    // Test: Normalization stage maintains statistical properties

    std::vector<uint32_t> neurons;
    for (uint32_t i = 0; i < 20; i++) {
        neurons.push_back(i);
    }

    EXPECT_TRUE(executePipeline(neurons, 1000000));

    // Calculate mean and std deviation of normalized features
    double sum = 0.0;
    for (uint32_t i = 0; i < context->num_cached_features; i++) {
        sum += context->cached_features[i];
    }
    double mean = sum / context->num_cached_features;

    double sum_sq = 0.0;
    for (uint32_t i = 0; i < context->num_cached_features; i++) {
        double diff = context->cached_features[i] - mean;
        sum_sq += diff * diff;
    }
    double std_dev = std::sqrt(sum_sq / context->num_cached_features);

    // Verify: Z-score normalization properties (mean ~0, std ~1)
    EXPECT_NEAR(mean, 0.0, 0.1);
    EXPECT_NEAR(std_dev, 1.0, 0.2);
}

TEST_F(PipelineIntegrationTest, DetectionAccuracy) {
    // Test: Detection stage accurately identifies patterns

    std::vector<uint32_t> neurons = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    EXPECT_TRUE(executePipeline(neurons, 1000000));

    // Verify: Pattern indices are valid
    for (uint32_t i = 0; i < context->num_detected_patterns; i++) {
        EXPECT_LT(context->detected_patterns[i], context->num_cached_features);
    }

    // Verify: Pattern confidences correspond to feature values
    for (uint32_t i = 0; i < context->num_detected_patterns; i++) {
        uint32_t pattern_idx = context->detected_patterns[i];
        float confidence = context->pattern_confidences[i];

        // Confidence should match the feature value
        EXPECT_FLOAT_EQ(confidence, context->cached_features[pattern_idx]);

        // Detected patterns should be above mean (positive z-scores)
        EXPECT_GT(confidence, 0.0f);
    }
}

TEST_F(PipelineIntegrationTest, MemoryStability) {
    // Test: Pipeline doesn't leak memory over many iterations

    std::vector<uint32_t> neurons = {0, 5, 10, 15, 20};

    // Execute many iterations
    for (int i = 0; i < 1000; i++) {
        EXPECT_TRUE(executePipeline(neurons, i * 1000));
    }

    // Verify: Final execution still works correctly
    EXPECT_TRUE(context->features_valid);
    EXPECT_EQ(context->num_cached_features, neurons.size());
    EXPECT_GT(context->num_detected_patterns, 0);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
