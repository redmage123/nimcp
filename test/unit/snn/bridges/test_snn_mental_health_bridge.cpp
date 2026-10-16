/**
 * @file test_snn_mental_health_bridge.cpp
 * @brief Unit tests for SNN-Mental Health bridge
 *
 * @author NIMCP Team
 * @date 2024-12-20
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "snn/bridges/nimcp_snn_mental_health_bridge.h"
#include "snn/nimcp_snn_network.h"
#include "snn/nimcp_snn_config.h"

class SNNMentalHealthBridgeTest : public ::testing::Test {
protected:
    snn_network_t* snn;
    snn_mental_health_bridge_t* bridge;

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
        if (bridge) snn_mental_health_bridge_destroy(bridge);
        if (snn) snn_network_destroy(snn);
    }
};

TEST_F(SNNMentalHealthBridgeTest, DefaultConfigInitialization) {
    snn_mental_health_config_t config;
    snn_mental_health_config_default(&config);

    EXPECT_GT(config.stability_threshold, 0.0f);
    EXPECT_GT(config.stability_window_ms, 0.0f);
    EXPECT_GT(config.risk_detection_sensitivity, 0.0f);
    EXPECT_GT(config.update_interval_ms, 0.0f);
}

TEST_F(SNNMentalHealthBridgeTest, BridgeCreation) {
    snn_mental_health_config_t config;
    snn_mental_health_config_default(&config);

    bridge = snn_mental_health_bridge_create(&config, snn);
    ASSERT_NE(bridge, nullptr);
}

TEST_F(SNNMentalHealthBridgeTest, BridgeCreationNullParams) {
    snn_mental_health_config_t config;
    snn_mental_health_config_default(&config);

    EXPECT_EQ(snn_mental_health_bridge_create(nullptr, snn), nullptr);
    EXPECT_EQ(snn_mental_health_bridge_create(&config, nullptr), nullptr);
}

TEST_F(SNNMentalHealthBridgeTest, BridgeDestruction) {
    snn_mental_health_config_t config;
    snn_mental_health_config_default(&config);

    bridge = snn_mental_health_bridge_create(&config, snn);
    ASSERT_NE(bridge, nullptr);

    snn_mental_health_bridge_destroy(bridge);
    bridge = nullptr;
}

TEST_F(SNNMentalHealthBridgeTest, BioAsyncConnectionStatus) {
    snn_mental_health_config_t config;
    snn_mental_health_config_default(&config);

    bridge = snn_mental_health_bridge_create(&config, snn);
    ASSERT_NE(bridge, nullptr);

    EXPECT_FALSE(snn_mental_health_bridge_is_bio_async_connected(bridge));
}

TEST_F(SNNMentalHealthBridgeTest, BioAsyncConnect) {
    snn_mental_health_config_t config;
    snn_mental_health_config_default(&config);

    bridge = snn_mental_health_bridge_create(&config, snn);
    ASSERT_NE(bridge, nullptr);

    int ret = snn_mental_health_bridge_connect_bio_async(bridge);
    EXPECT_TRUE(ret <= 0);  // 0 = success, negative = router not available
}

TEST_F(SNNMentalHealthBridgeTest, BioAsyncDisconnect) {
    snn_mental_health_config_t config;
    snn_mental_health_config_default(&config);

    bridge = snn_mental_health_bridge_create(&config, snn);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(snn_mental_health_bridge_disconnect_bio_async(bridge), 0);
}

TEST_F(SNNMentalHealthBridgeTest, GetStability) {
    snn_mental_health_config_t config;
    snn_mental_health_config_default(&config);

    bridge = snn_mental_health_bridge_create(&config, snn);
    ASSERT_NE(bridge, nullptr);

    float stability = snn_mental_health_get_stability(bridge);
    EXPECT_GE(stability, 0.0f);
    EXPECT_LE(stability, 1.0f);
}

TEST_F(SNNMentalHealthBridgeTest, GetRiskLevel) {
    snn_mental_health_config_t config;
    snn_mental_health_config_default(&config);

    bridge = snn_mental_health_bridge_create(&config, snn);
    ASSERT_NE(bridge, nullptr);

    snn_mental_health_risk_t risk = snn_mental_health_get_risk_level(bridge);
    EXPECT_EQ(risk, SNN_MENTAL_HEALTH_RISK_NONE);
}

TEST_F(SNNMentalHealthBridgeTest, GetRiskScore) {
    snn_mental_health_config_t config;
    snn_mental_health_config_default(&config);

    bridge = snn_mental_health_bridge_create(&config, snn);
    ASSERT_NE(bridge, nullptr);

    float score = snn_mental_health_get_risk_score(bridge);
    EXPECT_GE(score, 0.0f);
    EXPECT_LE(score, 1.0f);
}

TEST_F(SNNMentalHealthBridgeTest, GetDysregulationType) {
    snn_mental_health_config_t config;
    snn_mental_health_config_default(&config);

    bridge = snn_mental_health_bridge_create(&config, snn);
    ASSERT_NE(bridge, nullptr);

    snn_dysregulation_type_t type = snn_mental_health_get_dysregulation_type(bridge);
    EXPECT_EQ(type, SNN_DYSREGULATION_NONE);
}

TEST_F(SNNMentalHealthBridgeTest, IsInterventionActive) {
    snn_mental_health_config_t config;
    snn_mental_health_config_default(&config);

    bridge = snn_mental_health_bridge_create(&config, snn);
    ASSERT_NE(bridge, nullptr);

    bool active = snn_mental_health_is_intervention_active(bridge);
    EXPECT_FALSE(active);
}

TEST_F(SNNMentalHealthBridgeTest, GetBridgeState) {
    snn_mental_health_config_t config;
    snn_mental_health_config_default(&config);

    bridge = snn_mental_health_bridge_create(&config, snn);
    ASSERT_NE(bridge, nullptr);

    snn_mental_health_state_t state;
    int ret = snn_mental_health_bridge_get_state(bridge, &state);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(state.intervention_count, 0);
}

TEST_F(SNNMentalHealthBridgeTest, GetStatistics) {
    snn_mental_health_config_t config;
    snn_mental_health_config_default(&config);

    bridge = snn_mental_health_bridge_create(&config, snn);
    ASSERT_NE(bridge, nullptr);

    uint32_t intervention_count, dysregulation_count;
    float avg_stability;
    int ret = snn_mental_health_get_stats(bridge, &intervention_count, &dysregulation_count, &avg_stability);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(intervention_count, 0);
    EXPECT_EQ(dysregulation_count, 0);
}

TEST_F(SNNMentalHealthBridgeTest, ResetStatistics) {
    snn_mental_health_config_t config;
    snn_mental_health_config_default(&config);

    bridge = snn_mental_health_bridge_create(&config, snn);
    ASSERT_NE(bridge, nullptr);

    snn_mental_health_reset_stats(bridge);

    uint32_t intervention_count;
    snn_mental_health_get_stats(bridge, &intervention_count, nullptr, nullptr);
    EXPECT_EQ(intervention_count, 0);
}

TEST_F(SNNMentalHealthBridgeTest, BridgeUpdate) {
    snn_mental_health_config_t config;
    snn_mental_health_config_default(&config);

    bridge = snn_mental_health_bridge_create(&config, snn);
    ASSERT_NE(bridge, nullptr);

    int ret = snn_mental_health_bridge_update(bridge, 1.0f);
    EXPECT_EQ(ret, 0);
}

TEST_F(SNNMentalHealthBridgeTest, ComputeStability) {
    snn_mental_health_config_t config;
    snn_mental_health_config_default(&config);

    bridge = snn_mental_health_bridge_create(&config, snn);
    ASSERT_NE(bridge, nullptr);

    float stability = snn_mental_health_compute_stability(bridge);
    EXPECT_GE(stability, 0.0f);
    EXPECT_LE(stability, 1.0f);
}

TEST_F(SNNMentalHealthBridgeTest, DetectDysregulation) {
    snn_mental_health_config_t config;
    snn_mental_health_config_default(&config);

    bridge = snn_mental_health_bridge_create(&config, snn);
    ASSERT_NE(bridge, nullptr);

    snn_dysregulation_type_t type = snn_mental_health_detect_dysregulation(bridge);
    EXPECT_GE(type, SNN_DYSREGULATION_NONE);
    EXPECT_LE(type, SNN_DYSREGULATION_INSTABILITY);
}

TEST_F(SNNMentalHealthBridgeTest, AssessRisk) {
    snn_mental_health_config_t config;
    snn_mental_health_config_default(&config);

    bridge = snn_mental_health_bridge_create(&config, snn);
    ASSERT_NE(bridge, nullptr);

    snn_mental_health_risk_t risk = snn_mental_health_assess_risk(bridge);
    EXPECT_GE(risk, SNN_MENTAL_HEALTH_RISK_NONE);
    EXPECT_LE(risk, SNN_MENTAL_HEALTH_RISK_CRITICAL);
}

TEST_F(SNNMentalHealthBridgeTest, ComputeRiskScore) {
    snn_mental_health_config_t config;
    snn_mental_health_config_default(&config);

    bridge = snn_mental_health_bridge_create(&config, snn);
    ASSERT_NE(bridge, nullptr);

    float score = snn_mental_health_compute_risk_score(bridge);
    EXPECT_GE(score, 0.0f);
    EXPECT_LE(score, 1.0f);
}

TEST_F(SNNMentalHealthBridgeTest, TriggerIntervention) {
    snn_mental_health_config_t config;
    snn_mental_health_config_default(&config);
    config.enable_auto_intervention = true;

    bridge = snn_mental_health_bridge_create(&config, snn);
    ASSERT_NE(bridge, nullptr);

    int ret = snn_mental_health_trigger_intervention(bridge, SNN_DYSREGULATION_HYPERACTIVITY);
    EXPECT_EQ(ret, 0);
}

TEST_F(SNNMentalHealthBridgeTest, StopIntervention) {
    snn_mental_health_config_t config;
    snn_mental_health_config_default(&config);

    bridge = snn_mental_health_bridge_create(&config, snn);
    ASSERT_NE(bridge, nullptr);

    int ret = snn_mental_health_stop_intervention(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(SNNMentalHealthBridgeTest, NullBridgeReturnsDefaults) {
    EXPECT_FLOAT_EQ(snn_mental_health_get_stability(nullptr), 0.0f);
    EXPECT_EQ(snn_mental_health_get_risk_level(nullptr), SNN_MENTAL_HEALTH_RISK_NONE);
    EXPECT_FLOAT_EQ(snn_mental_health_get_risk_score(nullptr), 0.0f);
    EXPECT_EQ(snn_mental_health_get_dysregulation_type(nullptr), SNN_DYSREGULATION_NONE);
    EXPECT_FALSE(snn_mental_health_is_intervention_active(nullptr));
    EXPECT_FALSE(snn_mental_health_bridge_is_bio_async_connected(nullptr));
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
