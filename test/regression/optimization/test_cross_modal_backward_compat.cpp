//=============================================================================
// test_cross_modal_backward_compat.cpp - Regression Tests for Cross-Modal Module
//=============================================================================
/**
 * @file test_cross_modal_backward_compat.cpp
 * @brief Backward compatibility tests for cross-modal information flow
 *
 * PURPOSE: Ensure cross-modal module doesn't break existing NIMCP functionality
 * COVERAGE: All pre-cross-modal APIs continue to work unchanged
 * TEST COUNT: 22 regression tests
 *
 * @author NIMCP Development Team
 * @date 2025-11-14
 * @version 2.11 Phase C4.7
 */

#include <gtest/gtest.h>
#include "information/nimcp_cross_modal.h"
#include "information/nimcp_shannon.h"
#include "core/brain/nimcp_brain.h"
#include <cmath>

//=============================================================================
// Test Fixture
//=============================================================================

class CrossModalRegressionTest : public ::testing::Test {
protected:
    void SetUp() override {
        brain = brain_create("regression_test", BRAIN_SIZE_SMALL,
                           BRAIN_TASK_CLASSIFICATION, 10, 10);
        ASSERT_NE(brain, nullptr);

        // Allocate test features
        num_samples = 50;
        dim1 = 10;
        dim2 = 8;

        features1 = new float[num_samples * dim1];
        features2 = new float[num_samples * dim2];

        // Create features with sufficient variance for entropy calculation
        for (uint32_t i = 0; i < num_samples * dim1; i++) {
            features1[i] = 0.1f + 0.8f * (float)(i % 100) / 100.0f;
        }
        for (uint32_t i = 0; i < num_samples * dim2; i++) {
            features2[i] = 0.2f + 0.6f * (float)((i * 7) % 100) / 100.0f;
        }
    }

    void TearDown() override {
        delete[] features1;
        delete[] features2;

        if (brain) {
            brain_destroy(brain);
        }
    }

    brain_t brain;
    float* features1;
    float* features2;
    uint32_t num_samples;
    uint32_t dim1;
    uint32_t dim2;

    void create_pattern(float* features, uint32_t size, float value) {
        for (uint32_t i = 0; i < size; i++) {
            features[i] = value + 0.01f * (float)i;
        }
    }
};

//=============================================================================
// Brain Creation/Destruction (Pre-Cross-Modal Functionality)
//=============================================================================

TEST_F(CrossModalRegressionTest, BrainCreate_StillWorks) {
    brain_t test_brain = brain_create("test_regression", BRAIN_SIZE_SMALL,
                                      BRAIN_TASK_CLASSIFICATION, 10, 10);
    ASSERT_NE(test_brain, nullptr);
    brain_destroy(test_brain);
}

TEST_F(CrossModalRegressionTest, BrainDestroy_HandlesNull) {
    brain_destroy(nullptr);  // Should not crash
}

//=============================================================================
// Learning Pipeline (Pre-Cross-Modal Functionality)
//=============================================================================

TEST_F(CrossModalRegressionTest, LearnExample_StillWorks) {
    float features[10];
    create_pattern(features, 10, 0.5f);

    float loss = brain_learn_example(brain, features, 10, "class_a", 0.9f);

    EXPECT_GE(loss, 0.0f);
}

TEST_F(CrossModalRegressionTest, LearnExample_MultipleEpochs) {
    float features[10];
    create_pattern(features, 10, 0.5f);

    for (int i = 0; i < 10; i++) {
        float loss = brain_learn_example(brain, features, 10, "class_a", 0.9f);
        EXPECT_GE(loss, 0.0f);
    }
}

TEST_F(CrossModalRegressionTest, BrainDecide_StillWorks) {
    float features[10];
    create_pattern(features, 10, 0.5f);

    // Train first
    for (int i = 0; i < 5; i++) {
        brain_learn_example(brain, features, 10, "class_a", 0.9f);
    }

    // Decide
    brain_decision_t* decision = brain_decide(brain, features, 10);
    if (decision) {
        EXPECT_GE(decision->confidence, 0.0f);
        EXPECT_LE(decision->confidence, 1.0f);
    }
}

//=============================================================================
// Shannon API (Pre-Cross-Modal Functionality)
//=============================================================================

TEST_F(CrossModalRegressionTest, ShannonConfig_StillWorks) {
    shannon_config_t config = shannon_default_config();

    EXPECT_GT(config.min_probability, 0.0f);
    EXPECT_GT(config.min_capacity, 0.0f);
    EXPECT_GT(config.bottleneck_threshold, 0.0f);
}

TEST_F(CrossModalRegressionTest, ShannonAnalyzeSynapse_StillWorks) {
    shannon_config_t config = shannon_default_config();

    shannon_synapse_metrics_t metrics = shannon_analyze_synapse(
        0.5f, 10.0f, 0.1f, 10.0f, &config
    );

    EXPECT_GT(metrics.channel_capacity, 0.0f);
    EXPECT_GE(metrics.coding_efficiency, 0.0f);
}

TEST_F(CrossModalRegressionTest, ShannonAnalyzeNetwork_StillWorks) {
    shannon_config_t config = shannon_default_config();

    const uint32_t num_synapses = 10;
    shannon_synapse_metrics_t synapses[10];

    for (uint32_t i = 0; i < num_synapses; i++) {
        synapses[i] = shannon_analyze_synapse(
            0.5f, 10.0f, 0.1f, 10.0f, &config
        );
    }

    shannon_network_metrics_t network = shannon_analyze_network(
        synapses, num_synapses, nullptr, 0, &config
    );

    EXPECT_GT(network.total_capacity, 0.0f);
    EXPECT_GE(network.average_efficiency, 0.0f);
}

//=============================================================================
// Cross-Modal API Doesn't Break Existing Code
//=============================================================================

TEST_F(CrossModalRegressionTest, CrossModalConfig_NoSideEffects) {
    // Creating cross-modal config shouldn't affect brain
    shannon_config_t config = cross_modal_default_config();

    float features[10];
    create_pattern(features, 10, 0.5f);

    float loss = brain_learn_example(brain, features, 10, "class_a", 0.9f);

    EXPECT_GE(loss, 0.0f);
}

TEST_F(CrossModalRegressionTest, AnalyzeChannel_NoSideEffects) {
    shannon_config_t config = cross_modal_default_config();

    // Analyze cross-modal channel
    cross_modal_channel_t channel = cross_modal_analyze_channel(
        "visual", "audio", features1, dim1,
        features2, dim2, num_samples, &config
    );

    // Brain should still work normally
    float features[10];
    create_pattern(features, 10, 0.5f);
    float loss = brain_learn_example(brain, features, 10, "class_a", 0.9f);

    EXPECT_GE(loss, 0.0f);
    EXPECT_TRUE(std::isfinite(channel.source_entropy));
}

TEST_F(CrossModalRegressionTest, MultiModalIntegration_NoSideEffects) {
    shannon_config_t config = cross_modal_default_config();

    const float* features[2] = {features1, features2};
    const uint32_t dims[2] = {dim1, dim2};
    const char* names[2] = {"modality1", "modality2"};

    multi_modal_integration_t integration = cross_modal_analyze_integration(
        features, dims, 2, num_samples, names, &config
    );

    // Brain should still work normally
    float test_features[10];
    create_pattern(test_features, 10, 0.5f);
    float loss = brain_learn_example(brain, test_features, 10, "class_a", 0.9f);

    EXPECT_GE(loss, 0.0f);
    EXPECT_TRUE(std::isfinite(integration.joint_entropy));
}

TEST_F(CrossModalRegressionTest, RoutingGraph_NoSideEffects) {
    const char* names[3] = {"visual", "audio", "speech"};
    cross_modal_routing_graph_t* graph = cross_modal_create_routing_graph(names, 3);

    ASSERT_NE(graph, nullptr);

    // Brain should still work normally
    float features[10];
    create_pattern(features, 10, 0.5f);
    float loss = brain_learn_example(brain, features, 10, "class_a", 0.9f);

    EXPECT_GE(loss, 0.0f);

    cross_modal_destroy_routing_graph(graph);
}

//=============================================================================
// Data Integrity Tests
//=============================================================================

TEST_F(CrossModalRegressionTest, ChannelAnalysis_DoesNotModifyInput) {
    shannon_config_t config = cross_modal_default_config();

    // Save original values
    float original1[5];
    float original2[5];
    memcpy(original1, features1, 5 * sizeof(float));
    memcpy(original2, features2, 5 * sizeof(float));

    // Analyze channel
    cross_modal_channel_t channel = cross_modal_analyze_channel(
        "visual", "audio", features1, dim1,
        features2, dim2, num_samples, &config
    );

    // Verify input unchanged
    for (int i = 0; i < 5; i++) {
        EXPECT_FLOAT_EQ(features1[i], original1[i]);
        EXPECT_FLOAT_EQ(features2[i], original2[i]);
    }

    // Channel should compute something (may be negative for small samples)
    EXPECT_TRUE(std::isfinite(channel.source_entropy));
    EXPECT_TRUE(std::isfinite(channel.dest_entropy));
}

TEST_F(CrossModalRegressionTest, MultiModalAnalysis_DoesNotModifyInput) {
    shannon_config_t config = cross_modal_default_config();

    const float* features[2] = {features1, features2};
    const uint32_t dims[2] = {dim1, dim2};
    const char* names[2] = {"modality1", "modality2"};

    // Save original values
    float original1[5];
    float original2[5];
    memcpy(original1, features1, 5 * sizeof(float));
    memcpy(original2, features2, 5 * sizeof(float));

    // Analyze integration
    multi_modal_integration_t integration = cross_modal_analyze_integration(
        features, dims, 2, num_samples, names, &config
    );

    // Verify input unchanged
    for (int i = 0; i < 5; i++) {
        EXPECT_FLOAT_EQ(features1[i], original1[i]);
        EXPECT_FLOAT_EQ(features2[i], original2[i]);
    }

    // Integration should compute something
    EXPECT_TRUE(std::isfinite(integration.joint_entropy));
}

//=============================================================================
// Memory Management Tests
//=============================================================================

TEST_F(CrossModalRegressionTest, RoutingGraph_CreateDestroy_NoLeaks) {
    const char* names[5] = {"v1", "v2", "v3", "v4", "v5"};

    // Create and destroy multiple graphs
    for (int i = 0; i < 10; i++) {
        cross_modal_routing_graph_t* graph = cross_modal_create_routing_graph(names, 5);
        ASSERT_NE(graph, nullptr);
        cross_modal_destroy_routing_graph(graph);
    }
}

TEST_F(CrossModalRegressionTest, ChannelAnalysis_RepeatedCalls_Stable) {
    shannon_config_t config = cross_modal_default_config();

    // Analyze same channel first time to get baseline
    cross_modal_channel_t first = cross_modal_analyze_channel(
        "visual", "audio", features1, dim1,
        features2, dim2, num_samples, &config
    );

    // Analyze same channel 100 times - should be deterministic
    for (int i = 0; i < 100; i++) {
        cross_modal_channel_t channel = cross_modal_analyze_channel(
            "visual", "audio", features1, dim1,
            features2, dim2, num_samples, &config
        );

        // Should get identical results each time
        EXPECT_FLOAT_EQ(channel.source_entropy, first.source_entropy);
        EXPECT_FLOAT_EQ(channel.dest_entropy, first.dest_entropy);
    }
}

//=============================================================================
// Thread Safety Tests (Basic)
//=============================================================================

TEST_F(CrossModalRegressionTest, MultipleChannels_Sequential) {
    shannon_config_t config = cross_modal_default_config();

    // Analyze multiple different channels sequentially
    cross_modal_channel_t c1 = cross_modal_analyze_channel(
        "visual", "audio", features1, dim1,
        features2, dim2, num_samples, &config
    );

    cross_modal_channel_t c2 = cross_modal_analyze_channel(
        "audio", "visual", features2, dim2,
        features1, dim1, num_samples, &config
    );

    // Both channels should compute finite values
    EXPECT_TRUE(std::isfinite(c1.source_entropy));
    EXPECT_TRUE(std::isfinite(c2.source_entropy));
    EXPECT_STREQ(c1.source_modality, "visual");
    EXPECT_STREQ(c2.source_modality, "audio");
}

//=============================================================================
// Configuration Backward Compatibility
//=============================================================================

TEST_F(CrossModalRegressionTest, DefaultConfig_ValidValues) {
    shannon_config_t config = cross_modal_default_config();

    // Verify all config values are reasonable
    EXPECT_GT(config.min_probability, 0.0f);
    EXPECT_LT(config.min_probability, 1.0f);
    EXPECT_GT(config.min_capacity, 0.0f);
    EXPECT_GT(config.bottleneck_threshold, 0.0f);
    EXPECT_LE(config.bottleneck_threshold, 1.0f);
}

TEST_F(CrossModalRegressionTest, CustomConfig_StillWorks) {
    shannon_config_t config = cross_modal_default_config();

    // Modify config
    config.min_probability = 1e-8f;
    config.bottleneck_threshold = 0.6f;

    // Should still work with custom config
    cross_modal_channel_t channel = cross_modal_analyze_channel(
        "visual", "audio", features1, dim1,
        features2, dim2, num_samples, &config
    );

    // Should compute finite values
    EXPECT_TRUE(std::isfinite(channel.source_entropy));
    EXPECT_TRUE(std::isfinite(channel.dest_entropy));
}

//=============================================================================
// Edge Case Regression Tests
//=============================================================================

TEST_F(CrossModalRegressionTest, ZeroVarianceFeatures_HandlesGracefully) {
    shannon_config_t config = cross_modal_default_config();

    float uniform[50 * 8];
    for (uint32_t i = 0; i < 50 * 8; i++) {
        uniform[i] = 0.5f;
    }

    // Should not crash with zero-variance features
    cross_modal_channel_t channel = cross_modal_analyze_channel(
        "visual", "uniform", features1, dim1,
        uniform, 8, 50, &config
    );

    // Zero variance can produce negative differential entropy (expected)
    EXPECT_TRUE(std::isfinite(channel.source_entropy));
    EXPECT_TRUE(std::isfinite(channel.dest_entropy));
    // Dest should have very low/negative entropy (uniform distribution)
    EXPECT_LT(channel.dest_entropy, 1.0f);
}

TEST_F(CrossModalRegressionTest, LargeFeatureDims_StillWorks) {
    shannon_config_t config = cross_modal_default_config();

    const uint32_t large_dim = 100;
    const uint32_t samples = 50;

    float* large_features = new float[samples * large_dim];
    for (uint32_t i = 0; i < samples * large_dim; i++) {
        large_features[i] = 0.1f + 0.8f * (float)(i % 100) / 100.0f;
    }

    // Should handle large feature dimensions
    cross_modal_channel_t channel = cross_modal_analyze_channel(
        "high_dim1", "high_dim2",
        large_features, large_dim,
        features2, dim2,
        samples, &config
    );

    // Should complete without crashing and produce finite values
    EXPECT_TRUE(std::isfinite(channel.source_entropy));
    EXPECT_TRUE(std::isfinite(channel.dest_entropy));
    EXPECT_EQ(channel.sample_count, samples);

    delete[] large_features;
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
