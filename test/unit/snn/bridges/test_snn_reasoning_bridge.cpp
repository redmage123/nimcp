/**
 * @file test_snn_reasoning_bridge.cpp
 * @brief Unit tests for SNN-Reasoning bridge (15 tests)
 *
 * @author NIMCP Team
 * @date 2024-12-20
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "snn/bridges/nimcp_snn_reasoning_bridge.h"
#include "snn/nimcp_snn_network.h"
#include "snn/nimcp_snn_config.h"

class SNNReasoningBridgeTest : public ::testing::Test {
protected:
    snn_network_t* snn;
    reasoning_system_t reasoning;
    snn_reasoning_bridge_t* bridge;

    void SetUp() override {
        snn_config_t snn_config;
        snn_config_default(&snn_config);
        snn_config.n_inputs = 10;
        snn_config.n_outputs = 10;
        snn_config.n_populations = 2;
        snn_config.dt = 0.1f;
        snn = snn_network_create(&snn_config);
        ASSERT_NE(snn, nullptr);
        reasoning = nullptr;
        bridge = nullptr;
    }

    void TearDown() override {
        if (bridge) snn_reasoning_bridge_destroy(bridge);
        if (snn) snn_network_destroy(snn);
    }
};

TEST_F(SNNReasoningBridgeTest, DefaultConfigInitialization) {
    snn_reasoning_config_t config;
    snn_reasoning_config_default(&config);
    EXPECT_FLOAT_EQ(config.evidence_rate_min, 10.0f);
    EXPECT_FLOAT_EQ(config.evidence_rate_max, 100.0f);
    EXPECT_FLOAT_EQ(config.decision_threshold, 0.7f);
}

TEST_F(SNNReasoningBridgeTest, BridgeCreation) {
    snn_reasoning_config_t config;
    snn_reasoning_config_default(&config);
    bridge = snn_reasoning_bridge_create(&config, snn, reasoning);
    ASSERT_NE(bridge, nullptr);
}

TEST_F(SNNReasoningBridgeTest, BridgeCreationNullParams) {
    snn_reasoning_config_t config;
    snn_reasoning_config_default(&config);
    EXPECT_EQ(snn_reasoning_bridge_create(nullptr, snn, reasoning), nullptr);
    EXPECT_EQ(snn_reasoning_bridge_create(&config, nullptr, reasoning), nullptr);
}

TEST_F(SNNReasoningBridgeTest, BridgeDestruction) {
    snn_reasoning_config_t config;
    snn_reasoning_config_default(&config);
    bridge = snn_reasoning_bridge_create(&config, snn, reasoning);
    ASSERT_NE(bridge, nullptr);
    snn_reasoning_bridge_destroy(bridge);
    bridge = nullptr;
}

TEST_F(SNNReasoningBridgeTest, GetEvidence) {
    snn_reasoning_config_t config;
    snn_reasoning_config_default(&config);
    bridge = snn_reasoning_bridge_create(&config, snn, reasoning);
    ASSERT_NE(bridge, nullptr);
    float evidence = snn_reasoning_get_evidence(bridge);
    EXPECT_GE(evidence, 0.0f);
    EXPECT_LE(evidence, 1.0f);
}

TEST_F(SNNReasoningBridgeTest, GetConfidence) {
    snn_reasoning_config_t config;
    snn_reasoning_config_default(&config);
    bridge = snn_reasoning_bridge_create(&config, snn, reasoning);
    ASSERT_NE(bridge, nullptr);
    float confidence = snn_reasoning_get_confidence(bridge);
    EXPECT_GE(confidence, 0.0f);
    EXPECT_LE(confidence, 1.0f);
}

TEST_F(SNNReasoningBridgeTest, IsDecisionMade) {
    snn_reasoning_config_t config;
    snn_reasoning_config_default(&config);
    bridge = snn_reasoning_bridge_create(&config, snn, reasoning);
    ASSERT_NE(bridge, nullptr);
    bool decided = snn_reasoning_is_decision_made(bridge);
    EXPECT_FALSE(decided);
}

TEST_F(SNNReasoningBridgeTest, CheckDecisionThreshold) {
    snn_reasoning_config_t config;
    snn_reasoning_config_default(&config);
    bridge = snn_reasoning_bridge_create(&config, snn, reasoning);
    ASSERT_NE(bridge, nullptr);
    bool above_threshold = snn_reasoning_check_decision_threshold(bridge);
    EXPECT_FALSE(above_threshold);
}

TEST_F(SNNReasoningBridgeTest, AccumulateEvidence) {
    snn_reasoning_config_t config;
    snn_reasoning_config_default(&config);
    bridge = snn_reasoning_bridge_create(&config, snn, reasoning);
    ASSERT_NE(bridge, nullptr);
    float accumulated = snn_reasoning_accumulate_evidence(bridge, 50.0f);
    EXPECT_GE(accumulated, 0.0f);
    EXPECT_LE(accumulated, 1.0f);
}

TEST_F(SNNReasoningBridgeTest, GetBridgeState) {
    snn_reasoning_config_t config;
    snn_reasoning_config_default(&config);
    bridge = snn_reasoning_bridge_create(&config, snn, reasoning);
    ASSERT_NE(bridge, nullptr);
    snn_reasoning_state_t state;
    int ret = snn_reasoning_bridge_get_state(bridge, &state);
    EXPECT_EQ(ret, 0);
}

TEST_F(SNNReasoningBridgeTest, GetStatistics) {
    snn_reasoning_config_t config;
    snn_reasoning_config_default(&config);
    bridge = snn_reasoning_bridge_create(&config, snn, reasoning);
    ASSERT_NE(bridge, nullptr);
    uint32_t steps, decisions;
    float avg_conf;
    int ret = snn_reasoning_get_stats(bridge, &steps, &decisions, &avg_conf);
    EXPECT_EQ(ret, 0);
}

TEST_F(SNNReasoningBridgeTest, ResetStatistics) {
    snn_reasoning_config_t config;
    snn_reasoning_config_default(&config);
    bridge = snn_reasoning_bridge_create(&config, snn, reasoning);
    ASSERT_NE(bridge, nullptr);
    snn_reasoning_reset_stats(bridge);
    float evidence = snn_reasoning_get_evidence(bridge);
    EXPECT_FLOAT_EQ(evidence, 0.0f);
}

TEST_F(SNNReasoningBridgeTest, BioAsyncConnectionStatus) {
    snn_reasoning_config_t config;
    snn_reasoning_config_default(&config);
    bridge = snn_reasoning_bridge_create(&config, snn, reasoning);
    ASSERT_NE(bridge, nullptr);
    bool connected = snn_reasoning_bridge_is_bio_async_connected(bridge);
    EXPECT_FALSE(connected);
}

TEST_F(SNNReasoningBridgeTest, BioAsyncConnect) {
    snn_reasoning_config_t config;
    snn_reasoning_config_default(&config);
    bridge = snn_reasoning_bridge_create(&config, snn, reasoning);
    ASSERT_NE(bridge, nullptr);
    int ret = snn_reasoning_bridge_connect_bio_async(bridge);
    EXPECT_TRUE(ret <= 0);  // 0 = success, negative = router not available
}

TEST_F(SNNReasoningBridgeTest, ProcessFunction) {
    snn_reasoning_config_t config;
    snn_reasoning_config_default(&config);
    bridge = snn_reasoning_bridge_create(&config, snn, reasoning);
    ASSERT_NE(bridge, nullptr);
    float output[2] = {0.0f, 0.0f};
    int ret = snn_reasoning_bridge_process(bridge, nullptr, output);
    EXPECT_EQ(ret, 0);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
