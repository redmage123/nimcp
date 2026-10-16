/**
 * @file test_snn_autobiographical_bridge.cpp
 * @brief Unit tests for SNN-Autobiographical bridge
 *
 * @author NIMCP Team
 * @date 2024-12-20
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "snn/bridges/nimcp_snn_autobiographical_bridge.h"
#include "snn/nimcp_snn_network.h"
#include "snn/nimcp_snn_config.h"

class SNNAutobiographicalBridgeTest : public ::testing::Test {
protected:
    snn_network_t* snn;
    snn_autobiographical_bridge_t* bridge;

    void SetUp() override {
        snn_config_t snn_config;
        snn_config_default(&snn_config);
        snn_config.n_inputs = 10;
        snn_config.n_outputs = 10;
        snn_config.n_populations = 3;
        snn_config.dt = 0.1f;
        snn = snn_network_create(&snn_config);
        ASSERT_NE(snn, nullptr);
        bridge = nullptr;
    }

    void TearDown() override {
        if (bridge) snn_autobiographical_bridge_destroy(bridge);
        if (snn) snn_network_destroy(snn);
    }
};

TEST_F(SNNAutobiographicalBridgeTest, DefaultConfigInitialization) {
    snn_autobiographical_config_t config;
    snn_autobiographical_config_default(&config);

    EXPECT_GT(config.encoding_threshold, 0.0f);
    EXPECT_GT(config.encoding_window_ms, 0.0f);
    EXPECT_GT(config.retrieval_cue_strength, 0.0f);
    EXPECT_GT(config.consolidation_rate, 0.0f);
    EXPECT_GT(config.update_interval_ms, 0.0f);
}

TEST_F(SNNAutobiographicalBridgeTest, BridgeCreation) {
    snn_autobiographical_config_t config;
    snn_autobiographical_config_default(&config);

    bridge = snn_autobiographical_bridge_create(&config, snn);
    ASSERT_NE(bridge, nullptr);
}

TEST_F(SNNAutobiographicalBridgeTest, BridgeCreationNullParams) {
    snn_autobiographical_config_t config;
    snn_autobiographical_config_default(&config);

    EXPECT_EQ(snn_autobiographical_bridge_create(nullptr, snn), nullptr);
    EXPECT_EQ(snn_autobiographical_bridge_create(&config, nullptr), nullptr);
}

TEST_F(SNNAutobiographicalBridgeTest, BridgeDestruction) {
    snn_autobiographical_config_t config;
    snn_autobiographical_config_default(&config);

    bridge = snn_autobiographical_bridge_create(&config, snn);
    ASSERT_NE(bridge, nullptr);

    snn_autobiographical_bridge_destroy(bridge);
    bridge = nullptr;
}

TEST_F(SNNAutobiographicalBridgeTest, BioAsyncConnectionStatus) {
    snn_autobiographical_config_t config;
    snn_autobiographical_config_default(&config);

    bridge = snn_autobiographical_bridge_create(&config, snn);
    ASSERT_NE(bridge, nullptr);

    EXPECT_FALSE(snn_autobiographical_bridge_is_bio_async_connected(bridge));
}

TEST_F(SNNAutobiographicalBridgeTest, BioAsyncConnect) {
    snn_autobiographical_config_t config;
    snn_autobiographical_config_default(&config);

    bridge = snn_autobiographical_bridge_create(&config, snn);
    ASSERT_NE(bridge, nullptr);

    int ret = snn_autobiographical_bridge_connect_bio_async(bridge);
    EXPECT_TRUE(ret <= 0);  // 0 = success, negative = router not available
}

TEST_F(SNNAutobiographicalBridgeTest, BioAsyncDisconnect) {
    snn_autobiographical_config_t config;
    snn_autobiographical_config_default(&config);

    bridge = snn_autobiographical_bridge_create(&config, snn);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(snn_autobiographical_bridge_disconnect_bio_async(bridge), 0);
}

TEST_F(SNNAutobiographicalBridgeTest, GetMemoryCount) {
    snn_autobiographical_config_t config;
    snn_autobiographical_config_default(&config);

    bridge = snn_autobiographical_bridge_create(&config, snn);
    ASSERT_NE(bridge, nullptr);

    uint32_t count = snn_autobiographical_get_memory_count(bridge);
    EXPECT_EQ(count, 0);
}

TEST_F(SNNAutobiographicalBridgeTest, GetEncodingStrength) {
    snn_autobiographical_config_t config;
    snn_autobiographical_config_default(&config);

    bridge = snn_autobiographical_bridge_create(&config, snn);
    ASSERT_NE(bridge, nullptr);

    float strength = snn_autobiographical_get_encoding_strength(bridge);
    EXPECT_GE(strength, 0.0f);
    EXPECT_LE(strength, 1.0f);
}

TEST_F(SNNAutobiographicalBridgeTest, GetConsolidationProgress) {
    snn_autobiographical_config_t config;
    snn_autobiographical_config_default(&config);

    bridge = snn_autobiographical_bridge_create(&config, snn);
    ASSERT_NE(bridge, nullptr);

    float progress = snn_autobiographical_get_consolidation_progress(bridge);
    EXPECT_GE(progress, 0.0f);
    EXPECT_LE(progress, 1.0f);
}

TEST_F(SNNAutobiographicalBridgeTest, GetRetrievalSuccessRate) {
    snn_autobiographical_config_t config;
    snn_autobiographical_config_default(&config);

    bridge = snn_autobiographical_bridge_create(&config, snn);
    ASSERT_NE(bridge, nullptr);

    float rate = snn_autobiographical_get_retrieval_success_rate(bridge);
    EXPECT_GE(rate, 0.0f);
    EXPECT_LE(rate, 1.0f);
}

TEST_F(SNNAutobiographicalBridgeTest, GetBridgeState) {
    snn_autobiographical_config_t config;
    snn_autobiographical_config_default(&config);

    bridge = snn_autobiographical_bridge_create(&config, snn);
    ASSERT_NE(bridge, nullptr);

    snn_autobiographical_state_t state;
    int ret = snn_autobiographical_bridge_get_state(bridge, &state);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(state.memory_count, 0);
}

TEST_F(SNNAutobiographicalBridgeTest, GetStatistics) {
    snn_autobiographical_config_t config;
    snn_autobiographical_config_default(&config);

    bridge = snn_autobiographical_bridge_create(&config, snn);
    ASSERT_NE(bridge, nullptr);

    uint32_t memory_count, encoding_count;
    float success_rate;
    int ret = snn_autobiographical_get_stats(bridge, &memory_count, &encoding_count, &success_rate);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(memory_count, 0);
    EXPECT_EQ(encoding_count, 0);
}

TEST_F(SNNAutobiographicalBridgeTest, ResetStatistics) {
    snn_autobiographical_config_t config;
    snn_autobiographical_config_default(&config);

    bridge = snn_autobiographical_bridge_create(&config, snn);
    ASSERT_NE(bridge, nullptr);

    snn_autobiographical_reset_stats(bridge);

    uint32_t memory_count;
    snn_autobiographical_get_stats(bridge, &memory_count, nullptr, nullptr);
    EXPECT_EQ(memory_count, 0);
}

TEST_F(SNNAutobiographicalBridgeTest, BridgeUpdate) {
    snn_autobiographical_config_t config;
    snn_autobiographical_config_default(&config);

    bridge = snn_autobiographical_bridge_create(&config, snn);
    ASSERT_NE(bridge, nullptr);

    int ret = snn_autobiographical_bridge_update(bridge, 1.0f);
    EXPECT_EQ(ret, 0);
}

TEST_F(SNNAutobiographicalBridgeTest, EncodeEpisode) {
    snn_autobiographical_config_t config;
    snn_autobiographical_config_default(&config);

    bridge = snn_autobiographical_bridge_create(&config, snn);
    ASSERT_NE(bridge, nullptr);

    uint32_t memory_id;
    int ret = snn_autobiographical_encode_episode(bridge, SNN_ENCODING_STRONG, 0.5f, &memory_id);
    EXPECT_EQ(ret, 0);
}

TEST_F(SNNAutobiographicalBridgeTest, ComputeEncodingStrength) {
    snn_autobiographical_config_t config;
    snn_autobiographical_config_default(&config);

    bridge = snn_autobiographical_bridge_create(&config, snn);
    ASSERT_NE(bridge, nullptr);

    float strength = snn_autobiographical_compute_encoding_strength(bridge);
    EXPECT_GE(strength, 0.0f);
    EXPECT_LE(strength, 1.0f);
}

TEST_F(SNNAutobiographicalBridgeTest, ConsolidateMemory) {
    snn_autobiographical_config_t config;
    snn_autobiographical_config_default(&config);

    bridge = snn_autobiographical_bridge_create(&config, snn);
    ASSERT_NE(bridge, nullptr);

    uint32_t memory_id;
    snn_autobiographical_encode_episode(bridge, SNN_ENCODING_STRONG, 0.5f, &memory_id);
    int ret = snn_autobiographical_consolidate_memory(bridge, memory_id);
    EXPECT_EQ(ret, 0);
}

TEST_F(SNNAutobiographicalBridgeTest, ReplayMemories) {
    snn_autobiographical_config_t config;
    snn_autobiographical_config_default(&config);
    config.replay_probability = 1.0f;

    bridge = snn_autobiographical_bridge_create(&config, snn);
    ASSERT_NE(bridge, nullptr);

    int ret = snn_autobiographical_replay_memories(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(SNNAutobiographicalBridgeTest, ClearMemories) {
    snn_autobiographical_config_t config;
    snn_autobiographical_config_default(&config);

    bridge = snn_autobiographical_bridge_create(&config, snn);
    ASSERT_NE(bridge, nullptr);

    uint32_t memory_id;
    snn_autobiographical_encode_episode(bridge, SNN_ENCODING_MODERATE, 0.0f, &memory_id);
    snn_autobiographical_clear_memories(bridge);

    uint32_t count = snn_autobiographical_get_memory_count(bridge);
    EXPECT_EQ(count, 0);
}

TEST_F(SNNAutobiographicalBridgeTest, NullBridgeReturnsDefaults) {
    EXPECT_EQ(snn_autobiographical_get_memory_count(nullptr), 0);
    EXPECT_FLOAT_EQ(snn_autobiographical_get_encoding_strength(nullptr), 0.0f);
    EXPECT_FLOAT_EQ(snn_autobiographical_get_consolidation_progress(nullptr), 0.0f);
    EXPECT_FLOAT_EQ(snn_autobiographical_get_retrieval_success_rate(nullptr), 0.0f);
    EXPECT_FALSE(snn_autobiographical_bridge_is_bio_async_connected(nullptr));
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
