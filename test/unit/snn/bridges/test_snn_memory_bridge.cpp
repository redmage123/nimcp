/**
 * @file test_snn_memory_bridge.cpp
 * @brief Unit tests for SNN-Memory bridge (15 tests)
 *
 * @author NIMCP Team
 * @date 2024-12-20
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "snn/bridges/nimcp_snn_memory_bridge.h"
#include "snn/nimcp_snn_network.h"
#include "snn/nimcp_snn_config.h"
#include "cognitive/nimcp_working_memory.h"

class SNNMemoryBridgeTest : public ::testing::Test {
protected:
    snn_network_t* snn;
    working_memory_t* wm;
    snn_memory_bridge_t* bridge;

    void SetUp() override {
        snn_config_t snn_config;
        snn_config_default(&snn_config);
        snn_config.n_inputs = 10;
        snn_config.n_outputs = 10;
        snn_config.n_populations = 2;
        snn_config.dt = 0.1f;
        snn = snn_network_create(&snn_config);
        ASSERT_NE(snn, nullptr);

        wm = working_memory_create();
        ASSERT_NE(wm, nullptr);

        bridge = nullptr;
    }

    void TearDown() override {
        if (bridge) snn_memory_bridge_destroy(bridge);
        if (wm) working_memory_destroy(wm);
        if (snn) snn_network_destroy(snn);
    }
};

TEST_F(SNNMemoryBridgeTest, DefaultConfigInitialization) {
    snn_memory_config_t config;
    snn_memory_config_default(&config);
    EXPECT_FLOAT_EQ(config.encoding_threshold_rate, 20.0f);
    EXPECT_EQ(config.max_memory_items, 7);
    EXPECT_TRUE(config.use_population_code);
}

TEST_F(SNNMemoryBridgeTest, BridgeCreation) {
    snn_memory_config_t config;
    snn_memory_config_default(&config);
    bridge = snn_memory_bridge_create(&config, snn, wm);
    ASSERT_NE(bridge, nullptr);
}

TEST_F(SNNMemoryBridgeTest, BridgeCreationNullParams) {
    snn_memory_config_t config;
    snn_memory_config_default(&config);
    EXPECT_EQ(snn_memory_bridge_create(nullptr, snn, wm), nullptr);
    EXPECT_EQ(snn_memory_bridge_create(&config, nullptr, wm), nullptr);
    EXPECT_EQ(snn_memory_bridge_create(&config, snn, nullptr), nullptr);
}

TEST_F(SNNMemoryBridgeTest, BridgeDestruction) {
    snn_memory_config_t config;
    snn_memory_config_default(&config);
    bridge = snn_memory_bridge_create(&config, snn, wm);
    ASSERT_NE(bridge, nullptr);
    snn_memory_bridge_destroy(bridge);
    bridge = nullptr;
}

TEST_F(SNNMemoryBridgeTest, GetNumItems) {
    snn_memory_config_t config;
    snn_memory_config_default(&config);
    bridge = snn_memory_bridge_create(&config, snn, wm);
    ASSERT_NE(bridge, nullptr);
    uint32_t num = snn_memory_get_num_items(bridge);
    EXPECT_EQ(num, 0);
}

TEST_F(SNNMemoryBridgeTest, GetBridgeState) {
    snn_memory_config_t config;
    snn_memory_config_default(&config);
    bridge = snn_memory_bridge_create(&config, snn, wm);
    ASSERT_NE(bridge, nullptr);
    snn_memory_state_t state;
    int ret = snn_memory_bridge_get_state(bridge, &state);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(state.num_encoded_items, 0);
}

TEST_F(SNNMemoryBridgeTest, GetPersistenceStrength) {
    snn_memory_config_t config;
    snn_memory_config_default(&config);
    bridge = snn_memory_bridge_create(&config, snn, wm);
    ASSERT_NE(bridge, nullptr);
    float strength = snn_memory_get_persistence_strength(bridge, 0);
    EXPECT_GE(strength, 0.0f);
    EXPECT_LE(strength, 1.0f);
}

TEST_F(SNNMemoryBridgeTest, HasPersistentActivity) {
    snn_memory_config_t config;
    snn_memory_config_default(&config);
    bridge = snn_memory_bridge_create(&config, snn, wm);
    ASSERT_NE(bridge, nullptr);
    bool persistent = snn_memory_has_persistent_activity(bridge, 0);
    EXPECT_FALSE(persistent);
}

TEST_F(SNNMemoryBridgeTest, GetStatistics) {
    snn_memory_config_t config;
    snn_memory_config_default(&config);
    bridge = snn_memory_bridge_create(&config, snn, wm);
    ASSERT_NE(bridge, nullptr);
    uint32_t encodings, retrievals, evictions;
    int ret = snn_memory_get_stats(bridge, &encodings, &retrievals, &evictions);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(encodings, 0);
    EXPECT_EQ(retrievals, 0);
    EXPECT_EQ(evictions, 0);
}

TEST_F(SNNMemoryBridgeTest, ResetStatistics) {
    snn_memory_config_t config;
    snn_memory_config_default(&config);
    bridge = snn_memory_bridge_create(&config, snn, wm);
    ASSERT_NE(bridge, nullptr);
    snn_memory_reset_stats(bridge);
    uint32_t encodings;
    snn_memory_get_stats(bridge, &encodings, nullptr, nullptr);
    EXPECT_EQ(encodings, 0);
}

TEST_F(SNNMemoryBridgeTest, BioAsyncConnectionStatus) {
    snn_memory_config_t config;
    snn_memory_config_default(&config);
    bridge = snn_memory_bridge_create(&config, snn, wm);
    ASSERT_NE(bridge, nullptr);
    bool connected = snn_memory_bridge_is_bio_async_connected(bridge);
    EXPECT_FALSE(connected);
}

TEST_F(SNNMemoryBridgeTest, BioAsyncConnect) {
    snn_memory_config_t config;
    snn_memory_config_default(&config);
    bridge = snn_memory_bridge_create(&config, snn, wm);
    ASSERT_NE(bridge, nullptr);
    int ret = snn_memory_bridge_connect_bio_async(bridge);
    EXPECT_TRUE(ret <= 0);  // 0 = success, negative = router not available
}

TEST_F(SNNMemoryBridgeTest, BioAsyncDisconnect) {
    snn_memory_config_t config;
    snn_memory_config_default(&config);
    bridge = snn_memory_bridge_create(&config, snn, wm);
    ASSERT_NE(bridge, nullptr);
    int ret = snn_memory_bridge_disconnect_bio_async(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(SNNMemoryBridgeTest, UpdateFunction) {
    snn_memory_config_t config;
    snn_memory_config_default(&config);
    bridge = snn_memory_bridge_create(&config, snn, wm);
    ASSERT_NE(bridge, nullptr);
    int ret = snn_memory_bridge_update(bridge, 50.0f);
    EXPECT_EQ(ret, 0);
}

TEST_F(SNNMemoryBridgeTest, ProcessFunction) {
    snn_memory_config_t config;
    snn_memory_config_default(&config);
    bridge = snn_memory_bridge_create(&config, snn, wm);
    ASSERT_NE(bridge, nullptr);
    float output[2] = {0.0f, 0.0f};
    int ret = snn_memory_bridge_process(bridge, nullptr, output);
    EXPECT_EQ(ret, 0);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
