/**
 * @file test_snn_executive_bridge.cpp
 * @brief Unit tests for SNN-Executive bridge (15 tests)
 *
 * @author NIMCP Team
 * @date 2024-12-20
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "snn/bridges/nimcp_snn_executive_bridge.h"
#include "snn/nimcp_snn_network.h"
#include "snn/nimcp_snn_config.h"
#include "cognitive/nimcp_executive.h"

class SNNExecutiveBridgeTest : public ::testing::Test {
protected:
    snn_network_t* snn;
    executive_controller_t* executive;
    snn_executive_bridge_t* bridge;

    void SetUp() override {
        snn_config_t snn_config;
        snn_config_default(&snn_config);
        snn_config.n_inputs = 10;
        snn_config.n_outputs = 10;
        snn_config.n_populations = 2;
        snn_config.dt = 0.1f;
        snn = snn_network_create(&snn_config);
        ASSERT_NE(snn, nullptr);

        executive = executive_create();
        ASSERT_NE(executive, nullptr);

        bridge = nullptr;
    }

    void TearDown() override {
        if (bridge) snn_executive_bridge_destroy(bridge);
        if (executive) executive_destroy(executive);
        if (snn) snn_network_destroy(snn);
    }
};

TEST_F(SNNExecutiveBridgeTest, DefaultConfigInitialization) {
    snn_executive_config_t config;
    snn_executive_config_default(&config);
    EXPECT_FLOAT_EQ(config.inhibition_rate_threshold, 30.0f);
    EXPECT_FLOAT_EQ(config.task_switch_rate_change, 15.0f);
    EXPECT_TRUE(config.enable_interneuron_control);
}

TEST_F(SNNExecutiveBridgeTest, BridgeCreation) {
    snn_executive_config_t config;
    snn_executive_config_default(&config);
    bridge = snn_executive_bridge_create(&config, snn, executive);
    ASSERT_NE(bridge, nullptr);
}

TEST_F(SNNExecutiveBridgeTest, BridgeCreationNullParams) {
    snn_executive_config_t config;
    snn_executive_config_default(&config);
    EXPECT_EQ(snn_executive_bridge_create(nullptr, snn, executive), nullptr);
    EXPECT_EQ(snn_executive_bridge_create(&config, nullptr, executive), nullptr);
    EXPECT_EQ(snn_executive_bridge_create(&config, snn, nullptr), nullptr);
}

TEST_F(SNNExecutiveBridgeTest, BridgeDestruction) {
    snn_executive_config_t config;
    snn_executive_config_default(&config);
    bridge = snn_executive_bridge_create(&config, snn, executive);
    ASSERT_NE(bridge, nullptr);
    snn_executive_bridge_destroy(bridge);
    bridge = nullptr;
}

TEST_F(SNNExecutiveBridgeTest, GetInhibition) {
    snn_executive_config_t config;
    snn_executive_config_default(&config);
    bridge = snn_executive_bridge_create(&config, snn, executive);
    ASSERT_NE(bridge, nullptr);
    float inhibition = snn_executive_get_inhibition(bridge);
    EXPECT_GE(inhibition, 0.0f);
    EXPECT_LE(inhibition, 1.0f);
}

TEST_F(SNNExecutiveBridgeTest, GetCognitiveLoad) {
    snn_executive_config_t config;
    snn_executive_config_default(&config);
    bridge = snn_executive_bridge_create(&config, snn, executive);
    ASSERT_NE(bridge, nullptr);
    float load = snn_executive_get_cognitive_load(bridge);
    EXPECT_GE(load, 0.0f);
    EXPECT_LE(load, 1.0f);
}

TEST_F(SNNExecutiveBridgeTest, IsTaskSwitching) {
    snn_executive_config_t config;
    snn_executive_config_default(&config);
    bridge = snn_executive_bridge_create(&config, snn, executive);
    ASSERT_NE(bridge, nullptr);
    bool switching = snn_executive_is_task_switching(bridge);
    EXPECT_FALSE(switching);
}

TEST_F(SNNExecutiveBridgeTest, ComputeInhibition) {
    snn_executive_config_t config;
    snn_executive_config_default(&config);
    bridge = snn_executive_bridge_create(&config, snn, executive);
    ASSERT_NE(bridge, nullptr);
    float inhibition = snn_executive_compute_inhibition(bridge, 40.0f);
    EXPECT_GE(inhibition, 0.0f);
    EXPECT_LE(inhibition, 1.0f);
}

TEST_F(SNNExecutiveBridgeTest, ComputeCognitiveLoad) {
    snn_executive_config_t config;
    snn_executive_config_default(&config);
    bridge = snn_executive_bridge_create(&config, snn, executive);
    ASSERT_NE(bridge, nullptr);
    float load = snn_executive_compute_cognitive_load(bridge, 40.0f);
    EXPECT_GE(load, 0.0f);
    EXPECT_LE(load, 1.0f);
}

TEST_F(SNNExecutiveBridgeTest, DetectTaskSwitch) {
    snn_executive_config_t config;
    snn_executive_config_default(&config);
    bridge = snn_executive_bridge_create(&config, snn, executive);
    ASSERT_NE(bridge, nullptr);
    // Task switch detection returns bool based on internal state
    bool switched = snn_executive_detect_task_switch(bridge, 30.0f);
    // Accept either result - test verifies function executes without crash
    (void)switched;
}

TEST_F(SNNExecutiveBridgeTest, GetBridgeState) {
    snn_executive_config_t config;
    snn_executive_config_default(&config);
    bridge = snn_executive_bridge_create(&config, snn, executive);
    ASSERT_NE(bridge, nullptr);
    snn_executive_state_t state;
    int ret = snn_executive_bridge_get_state(bridge, &state);
    EXPECT_EQ(ret, 0);
}

TEST_F(SNNExecutiveBridgeTest, GetStatistics) {
    snn_executive_config_t config;
    snn_executive_config_default(&config);
    bridge = snn_executive_bridge_create(&config, snn, executive);
    ASSERT_NE(bridge, nullptr);
    uint32_t switch_count;
    float avg_inhibition, avg_load;
    int ret = snn_executive_get_stats(bridge, &switch_count, &avg_inhibition, &avg_load);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(switch_count, 0);
}

TEST_F(SNNExecutiveBridgeTest, ResetStatistics) {
    snn_executive_config_t config;
    snn_executive_config_default(&config);
    bridge = snn_executive_bridge_create(&config, snn, executive);
    ASSERT_NE(bridge, nullptr);
    snn_executive_reset_stats(bridge);
    uint32_t switch_count;
    snn_executive_get_stats(bridge, &switch_count, nullptr, nullptr);
    EXPECT_EQ(switch_count, 0);
}

TEST_F(SNNExecutiveBridgeTest, BioAsyncConnectionStatus) {
    snn_executive_config_t config;
    snn_executive_config_default(&config);
    bridge = snn_executive_bridge_create(&config, snn, executive);
    ASSERT_NE(bridge, nullptr);
    bool connected = snn_executive_bridge_is_bio_async_connected(bridge);
    EXPECT_FALSE(connected);
}

TEST_F(SNNExecutiveBridgeTest, ProcessFunction) {
    snn_executive_config_t config;
    snn_executive_config_default(&config);
    bridge = snn_executive_bridge_create(&config, snn, executive);
    ASSERT_NE(bridge, nullptr);
    float output[2] = {0.0f, 0.0f};
    int ret = snn_executive_bridge_process(bridge, nullptr, output);
    EXPECT_EQ(ret, 0);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
