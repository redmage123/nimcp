/**
 * @file test_cross_modal.cpp
 * @brief Unit tests for Phase C4.7 Cross-Modal Information Flow
 */

#include <gtest/gtest.h>
#include "information/nimcp_cross_modal.h"
#include <cmath>
#include <cstring>

class CrossModalTest : public ::testing::Test {
protected:
    void SetUp() override {
        config = cross_modal_default_config();
        num_samples = 100;
        source_dim = 10;
        dest_dim = 8;

        source_features = new float[num_samples * source_dim];
        dest_features = new float[num_samples * dest_dim];

        for (uint32_t i = 0; i < num_samples * source_dim; i++) {
            source_features[i] = static_cast<float>(i % 100) / 100.0f;
        }
        for (uint32_t i = 0; i < num_samples * dest_dim; i++) {
            dest_features[i] = static_cast<float>(i % 80) / 80.0f;
        }
    }

    void TearDown() override {
        delete[] source_features;
        delete[] dest_features;
    }

    shannon_config_t config;
    float* source_features;
    float* dest_features;
    uint32_t num_samples;
    uint32_t source_dim;
    uint32_t dest_dim;
};

// Channel Analysis Tests
TEST_F(CrossModalTest, AnalyzeChannel_ValidInputs_ReturnsValidMetrics) {
    cross_modal_channel_t channel = cross_modal_analyze_channel(
        "visual", "audio", source_features, source_dim,
        dest_features, dest_dim, num_samples, &config);

    EXPECT_STREQ(channel.source_modality, "visual");
    EXPECT_STREQ(channel.dest_modality, "audio");
    EXPECT_GT(channel.source_entropy, 0.0f);
    EXPECT_GT(channel.dest_entropy, 0.0f);
    EXPECT_GE(channel.mutual_information, 0.0f);
    EXPECT_GE(channel.transfer_efficiency, 0.0f);
    EXPECT_LE(channel.transfer_efficiency, 1.0f);
    EXPECT_GT(channel.channel_capacity, 0.0f);
    EXPECT_EQ(channel.sample_count, num_samples);
}

TEST_F(CrossModalTest, AnalyzeChannel_NullSourceModality_ReturnsZero) {
    cross_modal_channel_t channel = cross_modal_analyze_channel(
        nullptr, "audio", source_features, source_dim,
        dest_features, dest_dim, num_samples, &config);
    EXPECT_EQ(channel.source_entropy, 0.0f);
}

TEST_F(CrossModalTest, AnalyzeChannel_NullDestModality_ReturnsZero) {
    cross_modal_channel_t channel = cross_modal_analyze_channel(
        "visual", nullptr, source_features, source_dim,
        dest_features, dest_dim, num_samples, &config);
    EXPECT_EQ(channel.source_entropy, 0.0f);
}

TEST_F(CrossModalTest, AnalyzeChannel_NullSourceFeatures_ReturnsZero) {
    cross_modal_channel_t channel = cross_modal_analyze_channel(
        "visual", "audio", nullptr, source_dim,
        dest_features, dest_dim, num_samples, &config);
    EXPECT_EQ(channel.source_entropy, 0.0f);
}

TEST_F(CrossModalTest, AnalyzeChannel_NullDestFeatures_ReturnsZero) {
    cross_modal_channel_t channel = cross_modal_analyze_channel(
        "visual", "audio", source_features, source_dim,
        nullptr, dest_dim, num_samples, &config);
    EXPECT_EQ(channel.source_entropy, 0.0f);
}

TEST_F(CrossModalTest, AnalyzeChannel_NullConfig_ReturnsZero) {
    cross_modal_channel_t channel = cross_modal_analyze_channel(
        "visual", "audio", source_features, source_dim,
        dest_features, dest_dim, num_samples, nullptr);
    EXPECT_EQ(channel.source_entropy, 0.0f);
}

TEST_F(CrossModalTest, AnalyzeChannel_ZeroSourceDim_ReturnsZero) {
    cross_modal_channel_t channel = cross_modal_analyze_channel(
        "visual", "audio", source_features, 0,
        dest_features, dest_dim, num_samples, &config);
    EXPECT_EQ(channel.source_entropy, 0.0f);
}

TEST_F(CrossModalTest, AnalyzeChannel_ZeroDestDim_ReturnsZero) {
    cross_modal_channel_t channel = cross_modal_analyze_channel(
        "visual", "audio", source_features, source_dim,
        dest_features, 0, num_samples, &config);
    EXPECT_EQ(channel.source_entropy, 0.0f);
}

TEST_F(CrossModalTest, AnalyzeChannel_ZeroSamples_ReturnsZero) {
    cross_modal_channel_t channel = cross_modal_analyze_channel(
        "visual", "audio", source_features, source_dim,
        dest_features, dest_dim, 0, &config);
    EXPECT_EQ(channel.source_entropy, 0.0f);
}

// Bottleneck Detection Tests
TEST_F(CrossModalTest, IsBottleneck_NullChannel_ReturnsFalse) {
    EXPECT_FALSE(cross_modal_is_bottleneck(nullptr, 0.5f));
}

TEST_F(CrossModalTest, IsBottleneck_HighEfficiency_ReturnsFalse) {
    cross_modal_channel_t channel;
    memset(&channel, 0, sizeof(channel));
    channel.transfer_efficiency = 0.8f;
    EXPECT_FALSE(cross_modal_is_bottleneck(&channel, 0.5f));
}

TEST_F(CrossModalTest, IsBottleneck_LowEfficiency_ReturnsTrue) {
    cross_modal_channel_t channel;
    memset(&channel, 0, sizeof(channel));
    channel.transfer_efficiency = 0.3f;
    EXPECT_TRUE(cross_modal_is_bottleneck(&channel, 0.5f));
}

// Multi-Modal Integration Tests
TEST_F(CrossModalTest, AnalyzeIntegration_TwoModalities_ValidMetrics) {
    const float* features[2] = {source_features, dest_features};
    const uint32_t dims[2] = {source_dim, dest_dim};
    const char* names[2] = {"visual", "audio"};

    multi_modal_integration_t integration = cross_modal_analyze_integration(
        features, dims, 2, num_samples, names, &config);

    EXPECT_EQ(integration.num_modalities, 2u);
    EXPECT_GT(integration.joint_entropy, 0.0f);
}

TEST_F(CrossModalTest, AnalyzeIntegration_NullFeatures_ReturnsZero) {
    const uint32_t dims[2] = {source_dim, dest_dim};
    const char* names[2] = {"visual", "audio"};
    multi_modal_integration_t integration = cross_modal_analyze_integration(
        nullptr, dims, 2, num_samples, names, &config);
    EXPECT_EQ(integration.num_modalities, 0u);
}

// Synergy Tests
TEST_F(CrossModalTest, ComputeSynergy_NullIntegration_ReturnsZero) {
    EXPECT_EQ(cross_modal_compute_synergy(nullptr), 0.0f);
}

TEST_F(CrossModalTest, ComputeSynergy_ValidIntegration_ReturnsValue) {
    const float* features[2] = {source_features, dest_features};
    const uint32_t dims[2] = {source_dim, dest_dim};
    const char* names[2] = {"visual", "audio"};
    multi_modal_integration_t integration = cross_modal_analyze_integration(
        features, dims, 2, num_samples, names, &config);
    float synergy = cross_modal_compute_synergy(&integration);
    EXPECT_TRUE(std::isfinite(synergy));
}

// Routing Graph Tests
TEST_F(CrossModalTest, CreateRoutingGraph_ValidInputs_ReturnsGraph) {
    const char* names[3] = {"visual", "audio", "speech"};
    cross_modal_routing_graph_t* graph = cross_modal_create_routing_graph(names, 3);
    ASSERT_NE(graph, nullptr);
    EXPECT_EQ(graph->num_modalities, 3u);
    cross_modal_destroy_routing_graph(graph);
}

TEST_F(CrossModalTest, CreateRoutingGraph_NullNames_ReturnsNull) {
    EXPECT_EQ(cross_modal_create_routing_graph(nullptr, 3), nullptr);
}

TEST_F(CrossModalTest, UpdateRoutingGraph_ValidChannel_ReturnsTrue) {
    const char* names[2] = {"visual", "audio"};
    cross_modal_routing_graph_t* graph = cross_modal_create_routing_graph(names, 2);
    ASSERT_NE(graph, nullptr);

    cross_modal_channel_t channel = cross_modal_analyze_channel(
        "visual", "audio", source_features, source_dim,
        dest_features, dest_dim, num_samples, &config);

    EXPECT_TRUE(cross_modal_update_routing_graph(graph, 0, 1, &channel));
    cross_modal_destroy_routing_graph(graph);
}

TEST_F(CrossModalTest, DetectBottlenecks_EmptyGraph_ReturnsZero) {
    const char* names[2] = {"visual", "audio"};
    cross_modal_routing_graph_t* graph = cross_modal_create_routing_graph(names, 2);
    ASSERT_NE(graph, nullptr);

    cross_modal_channel_t bottlenecks[10];
    uint32_t num_bottlenecks;
    EXPECT_TRUE(cross_modal_detect_bottlenecks(graph, 0.5f, bottlenecks, 10, &num_bottlenecks));
    EXPECT_EQ(num_bottlenecks, 0u);
    cross_modal_destroy_routing_graph(graph);
}

TEST_F(CrossModalTest, FindOptimalRoute_DirectPath_ReturnsCapacity) {
    const char* names[2] = {"visual", "audio"};
    cross_modal_routing_graph_t* graph = cross_modal_create_routing_graph(names, 2);
    ASSERT_NE(graph, nullptr);

    cross_modal_channel_t channel;
    memset(&channel, 0, sizeof(channel));
    channel.channel_capacity = 100.0f;
    cross_modal_update_routing_graph(graph, 0, 1, &channel);

    uint32_t path[10];
    uint32_t path_length;
    float capacity = cross_modal_find_optimal_route(graph, 0, 1, path, 10, &path_length);

    EXPECT_EQ(capacity, 100.0f);
    EXPECT_EQ(path_length, 2u);
    cross_modal_destroy_routing_graph(graph);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
