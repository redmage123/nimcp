/**
 * @file test_thalamic_router_fep_bridge.cpp
 * @brief Unit tests for Thalamic Router - FEP Bridge
 * @version 1.0.0
 * @date 2025-12-12
 */

#include <gtest/gtest.h>
#include "middleware/routing/nimcp_thalamic_router_fep_bridge.h"

class ThalamicRouterFepBridgeTest : public ::testing::Test {
protected:
    thalamic_router_fep_bridge_t* bridge = nullptr;

    void SetUp() override {
        thalamic_router_fep_config_t config;
        thalamic_router_fep_bridge_default_config(&config);
        bridge = thalamic_router_fep_bridge_create(&config);
    }

    void TearDown() override {
        if (bridge) {
            thalamic_router_fep_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }
};

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(ThalamicRouterFepBridgeTest, DefaultConfigInitialization) {
    thalamic_router_fep_config_t config;
    int result = thalamic_router_fep_bridge_default_config(&config);

    EXPECT_EQ(result, 0);
    EXPECT_TRUE(config.enable_precision_gain);
    EXPECT_TRUE(config.enable_prediction_routing);
    EXPECT_TRUE(config.enable_pe_priority_boost);
    EXPECT_TRUE(config.enable_synchrony_confidence);
    EXPECT_GT(config.gain_sensitivity, 0.0f);
    EXPECT_GT(config.priority_sensitivity, 0.0f);
}

TEST_F(ThalamicRouterFepBridgeTest, DefaultConfigNullPointer) {
    int result = thalamic_router_fep_bridge_default_config(nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(ThalamicRouterFepBridgeTest, BridgeCreation) {
    ASSERT_NE(bridge, nullptr);
}

TEST_F(ThalamicRouterFepBridgeTest, BridgeCreationWithNullConfig) {
    thalamic_router_fep_bridge_t* bridge_null = thalamic_router_fep_bridge_create(nullptr);
    EXPECT_NE(bridge_null, nullptr);
    thalamic_router_fep_bridge_destroy(bridge_null);
}

TEST_F(ThalamicRouterFepBridgeTest, BridgeDestroyNullSafe) {
    thalamic_router_fep_bridge_destroy(nullptr);
    // Should not crash
    SUCCEED();
}

/* ============================================================================
 * Connection Tests
 * ============================================================================ */

TEST_F(ThalamicRouterFepBridgeTest, ConnectRouterSuccess) {
    thalamic_router_t router = {};
    int result = thalamic_router_fep_bridge_connect_router(bridge, &router);
    EXPECT_EQ(result, 0);
}

TEST_F(ThalamicRouterFepBridgeTest, ConnectRouterNullBridge) {
    thalamic_router_t router = {};
    int result = thalamic_router_fep_bridge_connect_router(nullptr, &router);
    EXPECT_EQ(result, -1);
}

TEST_F(ThalamicRouterFepBridgeTest, ConnectFepSuccess) {
    fep_system_t fep = {};
    int result = thalamic_router_fep_bridge_connect_fep(bridge, &fep);
    EXPECT_EQ(result, 0);
}

TEST_F(ThalamicRouterFepBridgeTest, ConnectFepNullBridge) {
    fep_system_t fep = {};
    int result = thalamic_router_fep_bridge_connect_fep(nullptr, &fep);
    EXPECT_EQ(result, -1);
}

TEST_F(ThalamicRouterFepBridgeTest, DisconnectSuccess) {
    int result = thalamic_router_fep_bridge_disconnect(bridge);
    EXPECT_EQ(result, 0);
}

TEST_F(ThalamicRouterFepBridgeTest, DisconnectNullBridge) {
    int result = thalamic_router_fep_bridge_disconnect(nullptr);
    EXPECT_EQ(result, -1);
}

/* ============================================================================
 * FEP → Router Direction Tests
 * ============================================================================ */

TEST_F(ThalamicRouterFepBridgeTest, ApplyPrecisionGainHigh) {
    int result = thalamic_router_fep_apply_precision_gain(bridge, 0.9f);
    EXPECT_EQ(result, 0);
}

TEST_F(ThalamicRouterFepBridgeTest, ApplyPrecisionGainLow) {
    int result = thalamic_router_fep_apply_precision_gain(bridge, 0.2f);
    EXPECT_EQ(result, 0);
}

TEST_F(ThalamicRouterFepBridgeTest, ApplyPrecisionGainNullBridge) {
    int result = thalamic_router_fep_apply_precision_gain(nullptr, 0.5f);
    EXPECT_EQ(result, -1);
}

TEST_F(ThalamicRouterFepBridgeTest, RoutePrediction) {
    int result = thalamic_router_fep_route_prediction(bridge, 0.7f);
    EXPECT_EQ(result, 0);
}

TEST_F(ThalamicRouterFepBridgeTest, RoutePredictionNullBridge) {
    int result = thalamic_router_fep_route_prediction(nullptr, 0.5f);
    EXPECT_EQ(result, -1);
}

TEST_F(ThalamicRouterFepBridgeTest, BoostPePriorityHigh) {
    int result = thalamic_router_fep_boost_pe_priority(bridge, 6.0f);
    EXPECT_EQ(result, 0);
}

TEST_F(ThalamicRouterFepBridgeTest, BoostPePriorityLow) {
    int result = thalamic_router_fep_boost_pe_priority(bridge, 2.0f);
    EXPECT_EQ(result, 0);
}

TEST_F(ThalamicRouterFepBridgeTest, BoostPePriorityNullBridge) {
    int result = thalamic_router_fep_boost_pe_priority(nullptr, 5.0f);
    EXPECT_EQ(result, -1);
}

/* ============================================================================
 * Router → FEP Direction Tests
 * ============================================================================ */

TEST_F(ThalamicRouterFepBridgeTest, ReportRoutedSignal) {
    routed_signal_t signal = {};
    signal.signal_strength = 0.8f;
    int result = thalamic_router_fep_report_routed_signal(bridge, &signal);
    EXPECT_EQ(result, 0);
}

TEST_F(ThalamicRouterFepBridgeTest, ReportRoutedSignalNullBridge) {
    routed_signal_t signal = {};
    int result = thalamic_router_fep_report_routed_signal(nullptr, &signal);
    EXPECT_EQ(result, -1);
}

TEST_F(ThalamicRouterFepBridgeTest, ReportRoutedSignalNullSignal) {
    int result = thalamic_router_fep_report_routed_signal(bridge, nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(ThalamicRouterFepBridgeTest, UpdateConfidenceFromRoutingSynchronous) {
    int result = thalamic_router_fep_update_confidence_from_routing(bridge, 0.9f);
    EXPECT_EQ(result, 0);
}

TEST_F(ThalamicRouterFepBridgeTest, UpdateConfidenceFromRoutingDesynchronized) {
    int result = thalamic_router_fep_update_confidence_from_routing(bridge, 0.2f);
    EXPECT_EQ(result, 0);
}

TEST_F(ThalamicRouterFepBridgeTest, UpdateConfidenceFromRoutingNullBridge) {
    int result = thalamic_router_fep_update_confidence_from_routing(nullptr, 0.5f);
    EXPECT_EQ(result, -1);
}

/* ============================================================================
 * Update Cycle Tests
 * ============================================================================ */

TEST_F(ThalamicRouterFepBridgeTest, BridgeUpdate) {
    int result = thalamic_router_fep_bridge_update(bridge, 100);
    EXPECT_EQ(result, 0);
}

TEST_F(ThalamicRouterFepBridgeTest, BridgeUpdateNullBridge) {
    int result = thalamic_router_fep_bridge_update(nullptr, 100);
    EXPECT_EQ(result, -1);
}

/* ============================================================================
 * State/Stats Tests
 * ============================================================================ */

TEST_F(ThalamicRouterFepBridgeTest, GetState) {
    thalamic_router_fep_state_t state;
    int result = thalamic_router_fep_bridge_get_state(bridge, &state);
    EXPECT_EQ(result, 0);
}

TEST_F(ThalamicRouterFepBridgeTest, GetStateNullBridge) {
    thalamic_router_fep_state_t state;
    int result = thalamic_router_fep_bridge_get_state(nullptr, &state);
    EXPECT_EQ(result, -1);
}

TEST_F(ThalamicRouterFepBridgeTest, GetStateNullOutput) {
    int result = thalamic_router_fep_bridge_get_state(bridge, nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(ThalamicRouterFepBridgeTest, GetStats) {
    thalamic_router_fep_stats_t stats;
    int result = thalamic_router_fep_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(result, 0);
}

TEST_F(ThalamicRouterFepBridgeTest, GetStatsNullBridge) {
    thalamic_router_fep_stats_t stats;
    int result = thalamic_router_fep_bridge_get_stats(nullptr, &stats);
    EXPECT_EQ(result, -1);
}

TEST_F(ThalamicRouterFepBridgeTest, GetStatsNullOutput) {
    thalamic_router_fep_stats_t stats;
    int result = thalamic_router_fep_bridge_get_stats(bridge, nullptr);
    EXPECT_EQ(result, -1);
}

/* ============================================================================
 * Bio-Async Tests
 * ============================================================================ */

TEST_F(ThalamicRouterFepBridgeTest, ConnectBioAsync) {
    int result = thalamic_router_fep_bridge_connect_bio_async(bridge);
    EXPECT_EQ(result, 0);
}

TEST_F(ThalamicRouterFepBridgeTest, ConnectBioAsyncNullBridge) {
    int result = thalamic_router_fep_bridge_connect_bio_async(nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(ThalamicRouterFepBridgeTest, DisconnectBioAsync) {
    thalamic_router_fep_bridge_connect_bio_async(bridge);
    int result = thalamic_router_fep_bridge_disconnect_bio_async(bridge);
    EXPECT_EQ(result, 0);
}

TEST_F(ThalamicRouterFepBridgeTest, DisconnectBioAsyncNullBridge) {
    int result = thalamic_router_fep_bridge_disconnect_bio_async(nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(ThalamicRouterFepBridgeTest, IsBioAsyncConnected) {
    bool connected = thalamic_router_fep_bridge_is_bio_async_connected(bridge);
    EXPECT_FALSE(connected);

    thalamic_router_fep_bridge_connect_bio_async(bridge);
    connected = thalamic_router_fep_bridge_is_bio_async_connected(bridge);
    // May or may not be true depending on bio-async availability
}

TEST_F(ThalamicRouterFepBridgeTest, IsBioAsyncConnectedNullBridge) {
    bool connected = thalamic_router_fep_bridge_is_bio_async_connected(nullptr);
    EXPECT_FALSE(connected);
}

/* ============================================================================
 * Integration Tests
 * ============================================================================ */

TEST_F(ThalamicRouterFepBridgeTest, HighPrecisionHighPriorityWorkflow) {
    // Apply high precision
    int result1 = thalamic_router_fep_apply_precision_gain(bridge, 0.9f);
    EXPECT_EQ(result1, 0);

    // Boost PE priority
    int result2 = thalamic_router_fep_boost_pe_priority(bridge, 6.0f);
    EXPECT_EQ(result2, 0);

    // Update
    int result3 = thalamic_router_fep_bridge_update(bridge, 100);
    EXPECT_EQ(result3, 0);
}

TEST_F(ThalamicRouterFepBridgeTest, LowPrecisionLowPriorityWorkflow) {
    // Apply low precision
    int result1 = thalamic_router_fep_apply_precision_gain(bridge, 0.2f);
    EXPECT_EQ(result1, 0);

    // Low PE
    int result2 = thalamic_router_fep_boost_pe_priority(bridge, 2.0f);
    EXPECT_EQ(result2, 0);

    // Update
    int result3 = thalamic_router_fep_bridge_update(bridge, 100);
    EXPECT_EQ(result3, 0);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
