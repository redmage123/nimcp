/**
 * @file test_snn_emotional_tagging_bridge.cpp
 * @brief Unit tests for SNN-Emotional Tagging bridge
 *
 * @author NIMCP Team
 * @date 2024-12-20
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "snn/bridges/nimcp_snn_emotional_tagging_bridge.h"
#include "snn/nimcp_snn_network.h"
#include "snn/nimcp_snn_config.h"

class SNNEmotionalTaggingBridgeTest : public ::testing::Test {
protected:
    snn_network_t* snn;
    snn_emotional_tagging_bridge_t* bridge;

    void SetUp() override {
        snn_config_t snn_config;
        snn_config_default(&snn_config);
        snn_config.n_inputs = 10;
        snn_config.n_outputs = 10;
        snn_config.n_populations = 2;
        snn_config.dt = 0.1f;
        snn = snn_network_create(&snn_config);
        ASSERT_NE(snn, nullptr);
        bridge = nullptr;
    }

    void TearDown() override {
        if (bridge) snn_emotional_tagging_bridge_destroy(bridge);
        if (snn) snn_network_destroy(snn);
    }
};

TEST_F(SNNEmotionalTaggingBridgeTest, DefaultConfigInitialization) {
    snn_emotional_tagging_config_t config;
    snn_emotional_tagging_config_default(&config);

    EXPECT_GT(config.tagging_threshold, 0.0f);
    EXPECT_GT(config.emotional_decay_rate, 0.0f);
    EXPECT_GT(config.max_tag_intensity, 0.0f);
    EXPECT_GT(config.update_interval_ms, 0.0f);
}

TEST_F(SNNEmotionalTaggingBridgeTest, BridgeCreation) {
    snn_emotional_tagging_config_t config;
    snn_emotional_tagging_config_default(&config);

    bridge = snn_emotional_tagging_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);
}

TEST_F(SNNEmotionalTaggingBridgeTest, BridgeCreationNullParams) {
    snn_emotional_tagging_config_t config;
    snn_emotional_tagging_config_default(&config);

    EXPECT_EQ(snn_emotional_tagging_bridge_create(nullptr, snn, nullptr), nullptr);
    EXPECT_EQ(snn_emotional_tagging_bridge_create(&config, nullptr, nullptr), nullptr);
}

TEST_F(SNNEmotionalTaggingBridgeTest, BridgeDestruction) {
    snn_emotional_tagging_config_t config;
    snn_emotional_tagging_config_default(&config);

    bridge = snn_emotional_tagging_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    snn_emotional_tagging_bridge_destroy(bridge);
    bridge = nullptr;
}

TEST_F(SNNEmotionalTaggingBridgeTest, BioAsyncConnectionStatus) {
    snn_emotional_tagging_config_t config;
    snn_emotional_tagging_config_default(&config);

    bridge = snn_emotional_tagging_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    EXPECT_FALSE(snn_emotional_tagging_bridge_is_bio_async_connected(bridge));
}

TEST_F(SNNEmotionalTaggingBridgeTest, GetCurrentIntensity) {
    snn_emotional_tagging_config_t config;
    snn_emotional_tagging_config_default(&config);

    bridge = snn_emotional_tagging_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    float intensity = snn_emotional_tagging_get_current_intensity(bridge);
    EXPECT_GE(intensity, 0.0f);
    EXPECT_LE(intensity, 1.0f);
}

TEST_F(SNNEmotionalTaggingBridgeTest, GetTaggedCount) {
    snn_emotional_tagging_config_t config;
    snn_emotional_tagging_config_default(&config);

    bridge = snn_emotional_tagging_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    uint32_t count = snn_emotional_tagging_get_tagged_count(bridge);
    EXPECT_EQ(count, 0);
}

TEST_F(SNNEmotionalTaggingBridgeTest, GetActiveTags) {
    snn_emotional_tagging_config_t config;
    snn_emotional_tagging_config_default(&config);

    bridge = snn_emotional_tagging_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    uint32_t active = snn_emotional_tagging_get_active_tags(bridge);
    EXPECT_EQ(active, 0);
}

TEST_F(SNNEmotionalTaggingBridgeTest, GetBridgeState) {
    snn_emotional_tagging_config_t config;
    snn_emotional_tagging_config_default(&config);

    bridge = snn_emotional_tagging_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    snn_emotional_tagging_state_t state;
    int ret = snn_emotional_tagging_bridge_get_state(bridge, &state);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(state.tagged_events_count, 0);
}

TEST_F(SNNEmotionalTaggingBridgeTest, GetStatistics) {
    snn_emotional_tagging_config_t config;
    snn_emotional_tagging_config_default(&config);

    bridge = snn_emotional_tagging_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    uint32_t tagged_count, consolidated_count;
    float avg_intensity;
    int ret = snn_emotional_tagging_get_stats(bridge, &tagged_count, &avg_intensity, &consolidated_count);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(tagged_count, 0);
}

TEST_F(SNNEmotionalTaggingBridgeTest, ResetStatistics) {
    snn_emotional_tagging_config_t config;
    snn_emotional_tagging_config_default(&config);

    bridge = snn_emotional_tagging_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    snn_emotional_tagging_reset_stats(bridge);

    uint32_t tagged_count;
    snn_emotional_tagging_get_stats(bridge, &tagged_count, nullptr, nullptr);
    EXPECT_EQ(tagged_count, 0);
}

TEST_F(SNNEmotionalTaggingBridgeTest, BridgeUpdate) {
    snn_emotional_tagging_config_t config;
    snn_emotional_tagging_config_default(&config);

    bridge = snn_emotional_tagging_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    int ret = snn_emotional_tagging_bridge_update(bridge, 1.0f);
    EXPECT_EQ(ret, 0);
}

TEST_F(SNNEmotionalTaggingBridgeTest, DetectBurst) {
    snn_emotional_tagging_config_t config;
    snn_emotional_tagging_config_default(&config);

    bridge = snn_emotional_tagging_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    float burst_rate, synchrony;
    int ret = snn_emotional_tagging_detect_burst(bridge, &burst_rate, &synchrony);
    EXPECT_EQ(ret, 0);
}

TEST_F(SNNEmotionalTaggingBridgeTest, CreateTag) {
    snn_emotional_tagging_config_t config;
    snn_emotional_tagging_config_default(&config);

    bridge = snn_emotional_tagging_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    int ret = snn_emotional_tagging_create_tag(bridge, 1, 0.8f);
    EXPECT_EQ(ret, 0);
}

TEST_F(SNNEmotionalTaggingBridgeTest, DecayTags) {
    snn_emotional_tagging_config_t config;
    snn_emotional_tagging_config_default(&config);

    bridge = snn_emotional_tagging_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    int ret = snn_emotional_tagging_decay_tags(bridge, 10.0f);
    EXPECT_EQ(ret, 0);
}

TEST_F(SNNEmotionalTaggingBridgeTest, ConsolidateMemory) {
    snn_emotional_tagging_config_t config;
    snn_emotional_tagging_config_default(&config);

    bridge = snn_emotional_tagging_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    snn_emotional_tagging_create_tag(bridge, 1, 0.8f);
    int ret = snn_emotional_tagging_consolidate_memory(bridge, 1);
    EXPECT_EQ(ret, 0);
}

TEST_F(SNNEmotionalTaggingBridgeTest, EnhanceLtp) {
    snn_emotional_tagging_config_t config;
    snn_emotional_tagging_config_default(&config);

    bridge = snn_emotional_tagging_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    snn_emotional_tagging_create_tag(bridge, 1, 0.8f);
    float enhancement;
    int ret = snn_emotional_tagging_enhance_ltp(bridge, 1, &enhancement);
    EXPECT_EQ(ret, 0);
}

TEST_F(SNNEmotionalTaggingBridgeTest, NullBridgeReturnsDefaults) {
    EXPECT_FLOAT_EQ(snn_emotional_tagging_get_current_intensity(nullptr), 0.0f);
    EXPECT_EQ(snn_emotional_tagging_get_tagged_count(nullptr), 0);
    EXPECT_EQ(snn_emotional_tagging_get_active_tags(nullptr), 0);
    EXPECT_FALSE(snn_emotional_tagging_bridge_is_bio_async_connected(nullptr));
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
