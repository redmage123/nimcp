/**
 * @file test_snn_wellbeing_bridge.cpp
 * @brief Unit tests for SNN-Wellbeing bridge
 *
 * @author NIMCP Team
 * @date 2024-12-20
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "snn/bridges/nimcp_snn_wellbeing_bridge.h"
#include "snn/nimcp_snn_network.h"
#include "snn/nimcp_snn_config.h"
#include "snn/nimcp_snn_types.h"

class SNNWellbeingBridgeTest : public ::testing::Test {
protected:
    snn_network_t* snn;
    snn_wellbeing_bridge_t* bridge;

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
        if (bridge) snn_wellbeing_bridge_destroy(bridge);
        if (snn) snn_network_destroy(snn);
    }
};

TEST_F(SNNWellbeingBridgeTest, DefaultConfigInitialization) {
    snn_wellbeing_config_t config;
    snn_wellbeing_config_default(&config);

    EXPECT_GT(config.homeostasis_setpoint, 0.0f);
    EXPECT_GT(config.allostatic_load_threshold, 0.0f);
    EXPECT_GT(config.recovery_rate, 0.0f);
    EXPECT_GT(config.update_interval_ms, 0.0f);
}

TEST_F(SNNWellbeingBridgeTest, BridgeCreation) {
    snn_wellbeing_config_t config;
    snn_wellbeing_config_default(&config);

    bridge = snn_wellbeing_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);
}

TEST_F(SNNWellbeingBridgeTest, BridgeCreationNullParams) {
    snn_wellbeing_config_t config;
    snn_wellbeing_config_default(&config);

    EXPECT_EQ(snn_wellbeing_bridge_create(nullptr, snn, nullptr), nullptr);
    EXPECT_EQ(snn_wellbeing_bridge_create(&config, nullptr, nullptr), nullptr);
}

TEST_F(SNNWellbeingBridgeTest, BridgeDestruction) {
    snn_wellbeing_config_t config;
    snn_wellbeing_config_default(&config);

    bridge = snn_wellbeing_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    snn_wellbeing_bridge_destroy(bridge);
    bridge = nullptr;
}

TEST_F(SNNWellbeingBridgeTest, BioAsyncConnectionStatus) {
    snn_wellbeing_config_t config;
    snn_wellbeing_config_default(&config);

    bridge = snn_wellbeing_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    EXPECT_FALSE(snn_wellbeing_bridge_is_bio_async_connected(bridge));
}

TEST_F(SNNWellbeingBridgeTest, BioAsyncConnect) {
    snn_wellbeing_config_t config;
    snn_wellbeing_config_default(&config);

    bridge = snn_wellbeing_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    int ret = snn_wellbeing_bridge_connect_bio_async(bridge);
    // Returns 0 on success, or error if bio-async not available
    EXPECT_TRUE(ret == 0 || ret == SNN_ERROR_NULL_POINTER || ret == SNN_ERROR_OPERATION_FAILED);
}

TEST_F(SNNWellbeingBridgeTest, BioAsyncDisconnect) {
    snn_wellbeing_config_t config;
    snn_wellbeing_config_default(&config);

    bridge = snn_wellbeing_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(snn_wellbeing_bridge_disconnect_bio_async(bridge), 0);
}

TEST_F(SNNWellbeingBridgeTest, GetIndex) {
    snn_wellbeing_config_t config;
    snn_wellbeing_config_default(&config);

    bridge = snn_wellbeing_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    float index = snn_wellbeing_get_index(bridge);
    EXPECT_GE(index, 0.0f);
    EXPECT_LE(index, 1.0f);
}

TEST_F(SNNWellbeingBridgeTest, GetAllostaticLoad) {
    snn_wellbeing_config_t config;
    snn_wellbeing_config_default(&config);

    bridge = snn_wellbeing_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    float load = snn_wellbeing_get_allostatic_load(bridge);
    EXPECT_GE(load, 0.0f);
}

TEST_F(SNNWellbeingBridgeTest, GetRegulationEvents) {
    snn_wellbeing_config_t config;
    snn_wellbeing_config_default(&config);

    bridge = snn_wellbeing_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    uint32_t events = snn_wellbeing_get_regulation_events(bridge);
    EXPECT_EQ(events, 0);
}

TEST_F(SNNWellbeingBridgeTest, IsRegulating) {
    snn_wellbeing_config_t config;
    snn_wellbeing_config_default(&config);

    bridge = snn_wellbeing_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    bool regulating = snn_wellbeing_is_regulating(bridge);
    EXPECT_FALSE(regulating);
}

TEST_F(SNNWellbeingBridgeTest, IsOverloaded) {
    snn_wellbeing_config_t config;
    snn_wellbeing_config_default(&config);

    bridge = snn_wellbeing_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    bool overloaded = snn_wellbeing_is_overloaded(bridge);
    EXPECT_FALSE(overloaded);
}

TEST_F(SNNWellbeingBridgeTest, GetBridgeState) {
    snn_wellbeing_config_t config;
    snn_wellbeing_config_default(&config);

    bridge = snn_wellbeing_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    snn_wellbeing_state_t state;
    int ret = snn_wellbeing_bridge_get_state(bridge, &state);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(state.regulation_events, 0);
}

TEST_F(SNNWellbeingBridgeTest, GetStatistics) {
    snn_wellbeing_config_t config;
    snn_wellbeing_config_default(&config);

    bridge = snn_wellbeing_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    uint32_t regulation_events, overload_events;
    float avg_load;
    int ret = snn_wellbeing_get_stats(bridge, &regulation_events, &overload_events, &avg_load);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(regulation_events, 0);
    EXPECT_EQ(overload_events, 0);
}

TEST_F(SNNWellbeingBridgeTest, ResetStatistics) {
    snn_wellbeing_config_t config;
    snn_wellbeing_config_default(&config);

    bridge = snn_wellbeing_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    snn_wellbeing_reset_stats(bridge);

    uint32_t regulation_events;
    snn_wellbeing_get_stats(bridge, &regulation_events, nullptr, nullptr);
    EXPECT_EQ(regulation_events, 0);
}

TEST_F(SNNWellbeingBridgeTest, BridgeUpdate) {
    snn_wellbeing_config_t config;
    snn_wellbeing_config_default(&config);

    bridge = snn_wellbeing_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    int ret = snn_wellbeing_bridge_update(bridge, 1.0f);
    EXPECT_EQ(ret, 0);
}

TEST_F(SNNWellbeingBridgeTest, EncodeWellbeing) {
    snn_wellbeing_config_t config;
    snn_wellbeing_config_default(&config);

    bridge = snn_wellbeing_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    int ret = snn_wellbeing_bridge_encode_wellbeing(bridge, 0.8f);
    // Returns 0 on success, or -5 (SNN_ERROR_INVALID_STATE) if no population configured
    EXPECT_TRUE(ret == 0 || ret == -5);
}

TEST_F(SNNWellbeingBridgeTest, TriggerRegulation) {
    snn_wellbeing_config_t config;
    snn_wellbeing_config_default(&config);

    bridge = snn_wellbeing_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    int ret = snn_wellbeing_bridge_trigger_regulation(bridge);
    // Returns 0 on success, or -5 (SNN_ERROR_INVALID_STATE) if no population configured
    EXPECT_TRUE(ret == 0 || ret == -5);
}

TEST_F(SNNWellbeingBridgeTest, ApplyRecovery) {
    snn_wellbeing_config_t config;
    snn_wellbeing_config_default(&config);

    bridge = snn_wellbeing_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    int ret = snn_wellbeing_bridge_apply_recovery(bridge, 10.0f);
    EXPECT_EQ(ret, 0);
}

TEST_F(SNNWellbeingBridgeTest, ComputeWellbeingIndex) {
    snn_wellbeing_config_t config;
    snn_wellbeing_config_default(&config);

    bridge = snn_wellbeing_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    float index = snn_wellbeing_compute_wellbeing_index(bridge, 50.0f, 0.2f);
    EXPECT_GE(index, 0.0f);
    EXPECT_LE(index, 1.0f);
}

TEST_F(SNNWellbeingBridgeTest, ComputeAllostaticLoad) {
    snn_wellbeing_config_t config;
    snn_wellbeing_config_default(&config);

    bridge = snn_wellbeing_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    float load = snn_wellbeing_compute_allostatic_load(bridge, 60.0f, 0.1f, 1.0f);
    EXPECT_GE(load, 0.0f);
}

TEST_F(SNNWellbeingBridgeTest, NullBridgeReturnsDefaults) {
    EXPECT_FLOAT_EQ(snn_wellbeing_get_index(nullptr), 0.0f);
    EXPECT_FLOAT_EQ(snn_wellbeing_get_allostatic_load(nullptr), 0.0f);
    EXPECT_EQ(snn_wellbeing_get_regulation_events(nullptr), 0);
    EXPECT_FALSE(snn_wellbeing_is_regulating(nullptr));
    EXPECT_FALSE(snn_wellbeing_is_overloaded(nullptr));
    EXPECT_FALSE(snn_wellbeing_bridge_is_bio_async_connected(nullptr));
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
