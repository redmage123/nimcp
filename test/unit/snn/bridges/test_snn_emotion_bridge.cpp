/**
 * @file test_snn_emotion_bridge.cpp
 * @brief Unit tests for SNN-Emotion bridge
 *
 * @author NIMCP Team
 * @date 2024-12-20
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "snn/bridges/nimcp_snn_emotion_bridge.h"
#include "snn/nimcp_snn_network.h"
#include "snn/nimcp_snn_config.h"

class SNNEmotionBridgeTest : public ::testing::Test {
protected:
    snn_network_t* snn;
    snn_emotion_bridge_t* bridge;

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
        if (bridge) {
            snn_emotion_bridge_destroy(bridge);
        }
        if (snn) {
            snn_network_destroy(snn);
        }
    }
};

// Test 1: Default configuration initialization
TEST_F(SNNEmotionBridgeTest, DefaultConfigInitialization) {
    snn_emotion_config_t config;
    snn_emotion_config_default(&config);

    EXPECT_EQ(config.arousal_rate_min, 5.0f);
    EXPECT_EQ(config.arousal_rate_max, 100.0f);
    EXPECT_FLOAT_EQ(config.valence_threshold, 0.1f);
    EXPECT_FLOAT_EQ(config.arousal_boost_factor, 1.5f);
    EXPECT_FLOAT_EQ(config.valence_weight_scaling, 0.2f);
    EXPECT_TRUE(config.modulate_excitability);
    EXPECT_TRUE(config.enable_theta_sync);
    EXPECT_EQ(config.theta_frequency, 6.0f);
}

// Test 2: Bridge creation with valid parameters
TEST_F(SNNEmotionBridgeTest, BridgeCreation) {
    snn_emotion_config_t config;
    snn_emotion_config_default(&config);

    bridge = snn_emotion_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);
}

// Test 3: Bridge creation with NULL config fails
TEST_F(SNNEmotionBridgeTest, BridgeCreationNullConfig) {
    EXPECT_EQ(snn_emotion_bridge_create(nullptr, snn, nullptr), nullptr);
}

// Test 4: Bridge creation with NULL SNN fails
TEST_F(SNNEmotionBridgeTest, BridgeCreationNullSNN) {
    snn_emotion_config_t config;
    snn_emotion_config_default(&config);
    EXPECT_EQ(snn_emotion_bridge_create(&config, nullptr, nullptr), nullptr);
}

// Test 5: Bridge destruction with NULL is safe
TEST_F(SNNEmotionBridgeTest, BridgeDestructionNull) {
    snn_emotion_bridge_destroy(nullptr);  // Should not crash
}

// Test 6: Bridge destruction
TEST_F(SNNEmotionBridgeTest, BridgeDestruction) {
    snn_emotion_config_t config;
    snn_emotion_config_default(&config);

    bridge = snn_emotion_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    snn_emotion_bridge_destroy(bridge);
    bridge = nullptr;
}

// Test 7: Bio-async connection status initially false
TEST_F(SNNEmotionBridgeTest, BioAsyncConnectionStatus) {
    snn_emotion_config_t config;
    snn_emotion_config_default(&config);

    bridge = snn_emotion_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    bool connected = snn_emotion_bridge_is_bio_async_connected(bridge);
    EXPECT_FALSE(connected);
}

// Test 8: Bio-async connect attempt
TEST_F(SNNEmotionBridgeTest, BioAsyncConnect) {
    snn_emotion_config_t config;
    snn_emotion_config_default(&config);

    bridge = snn_emotion_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    int ret = snn_emotion_bridge_connect_bio_async(bridge);
    // May fail if router unavailable - any non-crash is valid
    (void)ret;
}

// Test 9: Bio-async disconnect
TEST_F(SNNEmotionBridgeTest, BioAsyncDisconnect) {
    snn_emotion_config_t config;
    snn_emotion_config_default(&config);

    bridge = snn_emotion_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    int ret = snn_emotion_bridge_disconnect_bio_async(bridge);
    EXPECT_EQ(ret, 0);
}

// Test 10: Get decoded valence
TEST_F(SNNEmotionBridgeTest, GetDecodedValence) {
    snn_emotion_config_t config;
    snn_emotion_config_default(&config);

    bridge = snn_emotion_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    float valence = snn_emotion_get_decoded_valence(bridge);
    EXPECT_GE(valence, -1.0f);
    EXPECT_LE(valence, 1.0f);
}

// Test 11: Get decoded arousal
TEST_F(SNNEmotionBridgeTest, GetDecodedArousal) {
    snn_emotion_config_t config;
    snn_emotion_config_default(&config);

    bridge = snn_emotion_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    float arousal = snn_emotion_get_decoded_arousal(bridge);
    EXPECT_GE(arousal, 0.0f);
    EXPECT_LE(arousal, 1.0f);
}

// Test 12: Check theta synchronization status
TEST_F(SNNEmotionBridgeTest, CheckThetaSynchronization) {
    snn_emotion_config_t config;
    snn_emotion_config_default(&config);

    bridge = snn_emotion_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    bool synced = snn_emotion_is_theta_synchronized(bridge);
    EXPECT_FALSE(synced);  // Initially not synchronized
}

// Test 13: Get bridge state
TEST_F(SNNEmotionBridgeTest, GetBridgeState) {
    snn_emotion_config_t config;
    snn_emotion_config_default(&config);

    bridge = snn_emotion_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    snn_emotion_state_t state;
    int ret = snn_emotion_bridge_get_state(bridge, &state);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(state.sync_count, 0);
}

// Test 14: Get state with NULL output fails
TEST_F(SNNEmotionBridgeTest, GetBridgeStateNullOutput) {
    snn_emotion_config_t config;
    snn_emotion_config_default(&config);

    bridge = snn_emotion_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    int ret = snn_emotion_bridge_get_state(bridge, nullptr);
    EXPECT_NE(ret, 0);
}

// Test 15: Get statistics
TEST_F(SNNEmotionBridgeTest, GetStatistics) {
    snn_emotion_config_t config;
    snn_emotion_config_default(&config);

    bridge = snn_emotion_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    uint32_t sync_count;
    float avg_valence, avg_arousal;
    int ret = snn_emotion_get_stats(bridge, &sync_count, &avg_valence, &avg_arousal);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(sync_count, 0);
    EXPECT_FLOAT_EQ(avg_valence, 0.0f);
    EXPECT_FLOAT_EQ(avg_arousal, 0.0f);
}

// Test 16: Reset statistics
TEST_F(SNNEmotionBridgeTest, ResetStatistics) {
    snn_emotion_config_t config;
    snn_emotion_config_default(&config);

    bridge = snn_emotion_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    snn_emotion_reset_stats(bridge);

    uint32_t sync_count;
    int ret = snn_emotion_get_stats(bridge, &sync_count, nullptr, nullptr);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(sync_count, 0);
}

// Test 17: Bridge update
TEST_F(SNNEmotionBridgeTest, BridgeUpdate) {
    snn_emotion_config_t config;
    snn_emotion_config_default(&config);

    bridge = snn_emotion_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    int ret = snn_emotion_bridge_update(bridge, 1.0f);
    EXPECT_EQ(ret, 0);
}

// Test 18: Bridge update with NULL fails
TEST_F(SNNEmotionBridgeTest, BridgeUpdateNull) {
    int ret = snn_emotion_bridge_update(nullptr, 1.0f);
    EXPECT_NE(ret, 0);
}

// Test 19: Decode from spikes
TEST_F(SNNEmotionBridgeTest, DecodeFromSpikes) {
    snn_emotion_config_t config;
    snn_emotion_config_default(&config);

    bridge = snn_emotion_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    float valence, arousal;
    int ret = snn_emotion_decode_from_spikes(bridge, &valence, &arousal);
    EXPECT_EQ(ret, 0);
    EXPECT_GE(valence, -1.0f);
    EXPECT_LE(valence, 1.0f);
    EXPECT_GE(arousal, 0.0f);
    EXPECT_LE(arousal, 1.0f);
}

// Test 20: Decode with NULL outputs
TEST_F(SNNEmotionBridgeTest, DecodeFromSpikesNullOutputs) {
    snn_emotion_config_t config;
    snn_emotion_config_default(&config);

    bridge = snn_emotion_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    int ret = snn_emotion_decode_from_spikes(bridge, nullptr, nullptr);
    EXPECT_EQ(ret, 0);  // Should succeed, just not write outputs
}

// Test 21: Detect theta oscillation
TEST_F(SNNEmotionBridgeTest, DetectTheta) {
    snn_emotion_config_t config;
    snn_emotion_config_default(&config);

    bridge = snn_emotion_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    snn_theta_state_t theta;
    int ret = snn_emotion_detect_theta(bridge, &theta);
    EXPECT_EQ(ret, 0);
    EXPECT_FLOAT_EQ(theta.frequency, 6.0f);
}

// Test 22: Modulate populations
TEST_F(SNNEmotionBridgeTest, ModulatePopulations) {
    snn_emotion_config_t config;
    snn_emotion_config_default(&config);

    bridge = snn_emotion_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    int ret = snn_emotion_modulate_populations(bridge);
    EXPECT_EQ(ret, 0);
}

// Test 23: Encode to spikes
TEST_F(SNNEmotionBridgeTest, EncodeToSpikes) {
    snn_emotion_config_t config;
    snn_emotion_config_default(&config);

    bridge = snn_emotion_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    int ret = snn_emotion_encode_to_spikes(bridge, 0.5f, 0.7f);
    EXPECT_EQ(ret, 0);
}

// Test 24: Multiple updates accumulate sync count
TEST_F(SNNEmotionBridgeTest, MultipleUpdatesAccumulateSyncCount) {
    snn_emotion_config_t config;
    snn_emotion_config_default(&config);
    config.update_interval_ms = 10.0f;

    bridge = snn_emotion_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    // Update multiple times to exceed interval
    for (int i = 0; i < 20; i++) {
        snn_emotion_bridge_update(bridge, 1.0f);
    }

    uint32_t sync_count;
    snn_emotion_get_stats(bridge, &sync_count, nullptr, nullptr);
    EXPECT_GT(sync_count, 0);
}

// Test 25: Null bridge returns safe defaults
TEST_F(SNNEmotionBridgeTest, NullBridgeReturnsDefaults) {
    EXPECT_FLOAT_EQ(snn_emotion_get_decoded_valence(nullptr), 0.0f);
    EXPECT_FLOAT_EQ(snn_emotion_get_decoded_arousal(nullptr), 0.0f);
    EXPECT_FALSE(snn_emotion_is_theta_synchronized(nullptr));
    EXPECT_FALSE(snn_emotion_bridge_is_bio_async_connected(nullptr));
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
