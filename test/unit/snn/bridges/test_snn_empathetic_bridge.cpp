/**
 * @file test_snn_empathetic_bridge.cpp
 * @brief Unit tests for SNN-Empathetic bridge
 *
 * @author NIMCP Team
 * @date 2024-12-20
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "snn/bridges/nimcp_snn_empathetic_bridge.h"
#include "snn/nimcp_snn_network.h"
#include "snn/nimcp_snn_config.h"

class SNNEmpatheticBridgeTest : public ::testing::Test {
protected:
    snn_network_t* snn;
    snn_empathetic_bridge_t* bridge;

    void SetUp() override {
        snn_config_t snn_config;
        snn_config_default(&snn_config);
        snn_config.n_inputs = 10;
        snn_config.n_outputs = 10;
        snn_config.n_populations = 4;  // mirror, sts, ipl, acc
        snn_config.dt = 0.1f;
        snn = snn_network_create(&snn_config);
        ASSERT_NE(snn, nullptr);
        bridge = nullptr;
    }

    void TearDown() override {
        if (bridge) snn_empathetic_bridge_destroy(bridge);
        if (snn) snn_network_destroy(snn);
    }
};

TEST_F(SNNEmpatheticBridgeTest, DefaultConfigInitialization) {
    snn_empathetic_config_t config;
    snn_empathetic_config_default(&config);

    EXPECT_GT(config.mirror_activation_threshold, 0.0f);
    EXPECT_GT(config.empathy_gain, 0.0f);
    EXPECT_GT(config.resonance_decay_rate, 0.0f);
    EXPECT_GT(config.action_observation_weight, 0.0f);
}

TEST_F(SNNEmpatheticBridgeTest, BridgeCreation) {
    snn_empathetic_config_t config;
    snn_empathetic_config_default(&config);

    bridge = snn_empathetic_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);
}

TEST_F(SNNEmpatheticBridgeTest, BridgeCreationNullParams) {
    snn_empathetic_config_t config;
    snn_empathetic_config_default(&config);

    EXPECT_EQ(snn_empathetic_bridge_create(nullptr, snn, nullptr), nullptr);
    EXPECT_EQ(snn_empathetic_bridge_create(&config, nullptr, nullptr), nullptr);
}

TEST_F(SNNEmpatheticBridgeTest, BridgeDestruction) {
    snn_empathetic_config_t config;
    snn_empathetic_config_default(&config);

    bridge = snn_empathetic_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    snn_empathetic_bridge_destroy(bridge);
    bridge = nullptr;
}

TEST_F(SNNEmpatheticBridgeTest, BioAsyncConnectionStatus) {
    snn_empathetic_config_t config;
    snn_empathetic_config_default(&config);

    bridge = snn_empathetic_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    EXPECT_FALSE(snn_empathetic_bridge_is_bio_async_connected(bridge));
}

TEST_F(SNNEmpatheticBridgeTest, BioAsyncConnect) {
    snn_empathetic_config_t config;
    snn_empathetic_config_default(&config);

    bridge = snn_empathetic_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    int ret = snn_empathetic_bridge_connect_bio_async(bridge);
    EXPECT_TRUE(ret <= 0);  // 0 = success, negative = router not available
}

TEST_F(SNNEmpatheticBridgeTest, BioAsyncDisconnect) {
    snn_empathetic_config_t config;
    snn_empathetic_config_default(&config);

    bridge = snn_empathetic_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(snn_empathetic_bridge_disconnect_bio_async(bridge), 0);
}

TEST_F(SNNEmpatheticBridgeTest, GetMirrorActivation) {
    snn_empathetic_config_t config;
    snn_empathetic_config_default(&config);

    bridge = snn_empathetic_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    float activation = snn_empathetic_get_mirror_activation(bridge);
    EXPECT_GE(activation, 0.0f);
    EXPECT_LE(activation, 1.0f);
}

TEST_F(SNNEmpatheticBridgeTest, GetResponseCount) {
    snn_empathetic_config_t config;
    snn_empathetic_config_default(&config);

    bridge = snn_empathetic_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    uint32_t count = snn_empathetic_get_response_count(bridge);
    EXPECT_EQ(count, 0);  // Initially zero
}

TEST_F(SNNEmpatheticBridgeTest, GetResonanceCoherence) {
    snn_empathetic_config_t config;
    snn_empathetic_config_default(&config);

    bridge = snn_empathetic_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    float coherence = snn_empathetic_get_resonance_coherence(bridge);
    EXPECT_GE(coherence, 0.0f);
    EXPECT_LE(coherence, 1.0f);
}

TEST_F(SNNEmpatheticBridgeTest, GetBridgeState) {
    snn_empathetic_config_t config;
    snn_empathetic_config_default(&config);

    bridge = snn_empathetic_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    snn_empathetic_state_t state;
    int ret = snn_empathetic_bridge_get_state(bridge, &state);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(state.empathy_response_count, 0);
}

TEST_F(SNNEmpatheticBridgeTest, GetStatistics) {
    snn_empathetic_config_t config;
    snn_empathetic_config_default(&config);

    bridge = snn_empathetic_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    uint32_t response_count;
    float avg_activation, avg_resonance;
    int ret = snn_empathetic_get_stats(bridge, &response_count, &avg_activation, &avg_resonance);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(response_count, 0);
}

TEST_F(SNNEmpatheticBridgeTest, ResetStatistics) {
    snn_empathetic_config_t config;
    snn_empathetic_config_default(&config);

    bridge = snn_empathetic_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    snn_empathetic_reset_stats(bridge);

    uint32_t response_count;
    snn_empathetic_get_stats(bridge, &response_count, nullptr, nullptr);
    EXPECT_EQ(response_count, 0);
}

TEST_F(SNNEmpatheticBridgeTest, BridgeUpdate) {
    snn_empathetic_config_t config;
    snn_empathetic_config_default(&config);

    bridge = snn_empathetic_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    int ret = snn_empathetic_bridge_update(bridge, 1.0f);
    EXPECT_EQ(ret, 0);
}

TEST_F(SNNEmpatheticBridgeTest, ObserveAction) {
    snn_empathetic_config_t config;
    snn_empathetic_config_default(&config);

    bridge = snn_empathetic_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    float spikes[] = {1.0f, 0.0f, 1.0f, 1.0f, 0.0f};
    float observation_rate;
    int ret = snn_empathetic_observe_action(bridge, spikes, 5, &observation_rate);
    EXPECT_EQ(ret, 0);
}

TEST_F(SNNEmpatheticBridgeTest, TriggerMirrorResponse) {
    snn_empathetic_config_t config;
    snn_empathetic_config_default(&config);

    bridge = snn_empathetic_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    snn_empathy_response_t response;
    int ret = snn_empathetic_trigger_mirror_response(bridge, 50.0f, &response);
    EXPECT_EQ(ret, 0);
}

TEST_F(SNNEmpatheticBridgeTest, ComputeResonance) {
    snn_empathetic_config_t config;
    snn_empathetic_config_default(&config);

    bridge = snn_empathetic_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    float coherence;
    int ret = snn_empathetic_compute_resonance(bridge, 50.0f, 45.0f, &coherence);
    EXPECT_EQ(ret, 0);
    EXPECT_GE(coherence, 0.0f);
}

TEST_F(SNNEmpatheticBridgeTest, RecognizeAction) {
    snn_empathetic_config_t config;
    snn_empathetic_config_default(&config);

    bridge = snn_empathetic_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    float pattern[] = {1.0f, 1.0f, 0.0f, 1.0f, 0.0f};
    bool recognized;
    int ret = snn_empathetic_recognize_action(bridge, pattern, 5, &recognized);
    EXPECT_EQ(ret, 0);
}

TEST_F(SNNEmpatheticBridgeTest, DecayActivation) {
    snn_empathetic_config_t config;
    snn_empathetic_config_default(&config);

    bridge = snn_empathetic_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    int ret = snn_empathetic_decay_activation(bridge, 10.0f);
    EXPECT_EQ(ret, 0);
}

TEST_F(SNNEmpatheticBridgeTest, EmotionalContagion) {
    snn_empathetic_config_t config;
    snn_empathetic_config_default(&config);
    config.enable_emotional_contagion = true;

    bridge = snn_empathetic_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    float mirrored_emotion;
    int ret = snn_empathetic_emotional_contagion(bridge, 0.8f, &mirrored_emotion);
    EXPECT_EQ(ret, 0);
    EXPECT_GE(mirrored_emotion, 0.0f);
}

TEST_F(SNNEmpatheticBridgeTest, NullBridgeReturnsDefaults) {
    EXPECT_FLOAT_EQ(snn_empathetic_get_mirror_activation(nullptr), 0.0f);
    EXPECT_EQ(snn_empathetic_get_response_count(nullptr), 0);
    EXPECT_FLOAT_EQ(snn_empathetic_get_resonance_coherence(nullptr), 0.0f);
    EXPECT_FALSE(snn_empathetic_bridge_is_bio_async_connected(nullptr));
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
