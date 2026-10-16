/**
 * @file test_portia_collective_bridge.cpp
 * @brief Unit tests for Portia-Collective Cognition Bridge
 *
 * WHAT: Tests for portia-collective integration bridge
 * WHY: Verify resource sharing and coordination works correctly
 * HOW: Test lifecycle, connection, load balancing, degradation coordination
 *
 * TEST COVERAGE:
 * - Configuration API (3 tests)
 * - Lifecycle API (5 tests)
 * - Connection API (4 tests)
 * - Update cycle (4 tests)
 * - Load balancing (6 tests)
 * - Degradation coordination (4 tests)
 * - Query API (5 tests)
 * - Statistics (3 tests)
 * - Bio-Async API (4 tests)
 * - Remote Tier Handling (3 tests)
 * - Instance State (3 tests)
 * - Boundary Value (4 tests)
 *
 * TOTAL: 48 tests
 *
 * @author NIMCP Development Team
 * @date 2025-12-31
 */

#include <gtest/gtest.h>
#include <cstring>

// Headers have their own extern "C" guards
#include "portia/nimcp_portia_collective_bridge.h"

class PortiaCollectiveBridgeTest : public ::testing::Test {
protected:
    portia_collective_bridge_t* bridge;
    portia_collective_config_t config;

    void SetUp() override {
        portia_collective_default_config(&config);
        config.enable_bio_async = false;
        bridge = portia_collective_create(&config, nullptr, nullptr);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            portia_collective_destroy(bridge);
            bridge = nullptr;
        }
    }
};

//=============================================================================
// Configuration API (3 tests)
//=============================================================================

TEST_F(PortiaCollectiveBridgeTest, DefaultConfigHasReasonableValues) {
    portia_collective_config_t cfg;
    portia_collective_default_config(&cfg);

    EXPECT_EQ(cfg.resource_strategy, COLLECTIVE_RESOURCE_COOPERATIVE);
    EXPECT_EQ(cfg.degradation_mode, DEGRADATION_COORD_COMPENSATING);
    EXPECT_GT(cfg.offload_threshold, 0.0f);
    EXPECT_LT(cfg.offload_threshold, 1.0f);
    EXPECT_GT(cfg.receive_threshold, 0.0f);
    EXPECT_LT(cfg.receive_threshold, 1.0f);
    EXPECT_GT(cfg.update_interval_ms, 0u);
}

TEST_F(PortiaCollectiveBridgeTest, DefaultConfigWithNullDoesNotCrash) {
    portia_collective_default_config(nullptr);
    // Should not crash
}

TEST_F(PortiaCollectiveBridgeTest, CreateWithCustomConfig) {
    portia_collective_config_t custom_config;
    portia_collective_default_config(&custom_config);
    custom_config.resource_strategy = COLLECTIVE_RESOURCE_PROACTIVE;
    custom_config.enable_bio_async = false;

    portia_collective_bridge_t* custom_bridge = portia_collective_create(
        &custom_config, nullptr, nullptr);
    ASSERT_NE(custom_bridge, nullptr);
    portia_collective_destroy(custom_bridge);
}

//=============================================================================
// Lifecycle API (5 tests)
//=============================================================================

TEST_F(PortiaCollectiveBridgeTest, CreateWithNullConfigUsesDefaults) {
    portia_collective_bridge_t* b = portia_collective_create(nullptr, nullptr, nullptr);
    ASSERT_NE(b, nullptr);
    portia_collective_destroy(b);
}

TEST_F(PortiaCollectiveBridgeTest, DestroyNullDoesNotCrash) {
    portia_collective_destroy(nullptr);
    // Should not crash
}

TEST_F(PortiaCollectiveBridgeTest, ResetSucceeds) {
    EXPECT_EQ(portia_collective_reset(bridge), 0);
}

TEST_F(PortiaCollectiveBridgeTest, ResetNullFails) {
    EXPECT_EQ(portia_collective_reset(nullptr), -1);
}

TEST_F(PortiaCollectiveBridgeTest, CreateDestroyMultipleTimes) {
    for (int i = 0; i < 5; ++i) {
        portia_collective_bridge_t* b = portia_collective_create(&config, nullptr, nullptr);
        ASSERT_NE(b, nullptr);
        portia_collective_destroy(b);
    }
}

//=============================================================================
// Connection API (4 tests)
//=============================================================================

TEST_F(PortiaCollectiveBridgeTest, ConnectPortiaWithNull) {
    EXPECT_EQ(portia_collective_connect_portia(bridge, nullptr), 0);
}

TEST_F(PortiaCollectiveBridgeTest, ConnectPortiaNullBridgeFails) {
    EXPECT_EQ(portia_collective_connect_portia(nullptr, nullptr), -1);
}

TEST_F(PortiaCollectiveBridgeTest, ConnectCollectiveWithNull) {
    EXPECT_EQ(portia_collective_connect_collective(bridge, nullptr), 0);
}

TEST_F(PortiaCollectiveBridgeTest, ConnectCollectiveNullBridgeFails) {
    EXPECT_EQ(portia_collective_connect_collective(nullptr, nullptr), -1);
}

//=============================================================================
// Update Cycle (4 tests)
//=============================================================================

TEST_F(PortiaCollectiveBridgeTest, UpdateSucceeds) {
    EXPECT_EQ(portia_collective_update(bridge, 100), 0);
}

TEST_F(PortiaCollectiveBridgeTest, UpdateNullFails) {
    EXPECT_EQ(portia_collective_update(nullptr, 100), -1);
}

TEST_F(PortiaCollectiveBridgeTest, UpdateMultipleTimesSucceeds) {
    for (int i = 0; i < 10; ++i) {
        EXPECT_EQ(portia_collective_update(bridge, 50), 0);
    }
}

TEST_F(PortiaCollectiveBridgeTest, BroadcastTierSucceeds) {
    EXPECT_EQ(portia_collective_broadcast_tier(bridge, 2), 0);

    portia_collective_stats_t stats;
    EXPECT_EQ(portia_collective_get_stats(bridge, &stats), 0);
    EXPECT_GE(stats.tier_broadcasts, 1u);
}

//=============================================================================
// Load Balancing (6 tests)
//=============================================================================

TEST_F(PortiaCollectiveBridgeTest, CanReceiveWithLowLoad) {
    // Default load is 0.0, should be able to receive
    EXPECT_TRUE(portia_collective_can_receive(bridge));
}

TEST_F(PortiaCollectiveBridgeTest, CanReceiveNullReturnsFalse) {
    EXPECT_FALSE(portia_collective_can_receive(nullptr));
}

TEST_F(PortiaCollectiveBridgeTest, RequestOffloadNoTargets) {
    // No other instances, should fail to find target
    uint32_t target = 0;
    EXPECT_EQ(portia_collective_request_offload(bridge, 0.5f, &target), -1);
}

TEST_F(PortiaCollectiveBridgeTest, RequestOffloadNullBridgeFails) {
    uint32_t target = 0;
    EXPECT_EQ(portia_collective_request_offload(nullptr, 0.5f, &target), -1);
}

TEST_F(PortiaCollectiveBridgeTest, RequestOffloadNullTargetFails) {
    EXPECT_EQ(portia_collective_request_offload(bridge, 0.5f, nullptr), -1);
}

TEST_F(PortiaCollectiveBridgeTest, RebalanceAsLeader) {
    // Bridge is initially leader
    EXPECT_TRUE(portia_collective_is_leader(bridge));
    int result = portia_collective_rebalance(bridge);
    EXPECT_GE(result, 0);  // 0 or more tasks redistributed
}

//=============================================================================
// Degradation Coordination (4 tests)
//=============================================================================

TEST_F(PortiaCollectiveBridgeTest, CoordinateDegradationSucceeds) {
    EXPECT_EQ(portia_collective_coordinate_degradation(bridge, true), 0);
    EXPECT_EQ(portia_collective_coordinate_degradation(bridge, false), 0);
}

TEST_F(PortiaCollectiveBridgeTest, CoordinateDegradationNullFails) {
    EXPECT_EQ(portia_collective_coordinate_degradation(nullptr, true), -1);
}

TEST_F(PortiaCollectiveBridgeTest, RequestCompensationSucceeds) {
    // Must be degraded to request compensation
    portia_collective_coordinate_degradation(bridge, true);
    EXPECT_EQ(portia_collective_request_compensation(bridge), 0);
}

TEST_F(PortiaCollectiveBridgeTest, RequestCompensationNullFails) {
    EXPECT_EQ(portia_collective_request_compensation(nullptr), -1);
}

//=============================================================================
// Query API (5 tests)
//=============================================================================

TEST_F(PortiaCollectiveBridgeTest, GetSummarySucceeds) {
    // Update to register local instance
    portia_collective_update(bridge, 100);

    collective_resource_summary_t summary;
    EXPECT_EQ(portia_collective_get_summary(bridge, &summary), 0);
    EXPECT_GE(summary.total_instances, 1u);  // At least local instance
}

TEST_F(PortiaCollectiveBridgeTest, GetSummaryNullBridgeFails) {
    collective_resource_summary_t summary;
    EXPECT_EQ(portia_collective_get_summary(nullptr, &summary), -1);
}

TEST_F(PortiaCollectiveBridgeTest, GetSummaryNullOutputFails) {
    EXPECT_EQ(portia_collective_get_summary(bridge, nullptr), -1);
}

TEST_F(PortiaCollectiveBridgeTest, GetLocalIdReturnsNonZero) {
    uint32_t id = portia_collective_get_local_id(bridge);
    EXPECT_NE(id, 0u);
}

TEST_F(PortiaCollectiveBridgeTest, IsLeaderReturnsTrue) {
    // Initially the only instance, should be leader
    EXPECT_TRUE(portia_collective_is_leader(bridge));
}

//=============================================================================
// Statistics (3 tests)
//=============================================================================

TEST_F(PortiaCollectiveBridgeTest, GetStatsSucceeds) {
    portia_collective_stats_t stats;
    EXPECT_EQ(portia_collective_get_stats(bridge, &stats), 0);
}

TEST_F(PortiaCollectiveBridgeTest, GetStatsNullFails) {
    portia_collective_stats_t stats;
    EXPECT_EQ(portia_collective_get_stats(nullptr, &stats), -1);
    EXPECT_EQ(portia_collective_get_stats(bridge, nullptr), -1);
}

TEST_F(PortiaCollectiveBridgeTest, ResetStatsSucceeds) {
    portia_collective_broadcast_tier(bridge, 2);

    portia_collective_stats_t stats;
    portia_collective_get_stats(bridge, &stats);
    EXPECT_GE(stats.tier_broadcasts, 1u);

    portia_collective_reset_stats(bridge);
    portia_collective_get_stats(bridge, &stats);
    EXPECT_EQ(stats.tier_broadcasts, 0u);
}

//=============================================================================
// Bio-Async API Tests (4 tests)
//=============================================================================

TEST_F(PortiaCollectiveBridgeTest, ConnectBioAsyncSucceeds) {
    EXPECT_EQ(portia_collective_connect_bio_async(bridge), 0);
}

TEST_F(PortiaCollectiveBridgeTest, ConnectBioAsyncNullFails) {
    EXPECT_EQ(portia_collective_connect_bio_async(nullptr), -1);
}

TEST_F(PortiaCollectiveBridgeTest, DisconnectBioAsyncSucceeds) {
    portia_collective_connect_bio_async(bridge);
    EXPECT_EQ(portia_collective_disconnect_bio_async(bridge), 0);
}

TEST_F(PortiaCollectiveBridgeTest, DisconnectBioAsyncNullFails) {
    EXPECT_EQ(portia_collective_disconnect_bio_async(nullptr), -1);
}

//=============================================================================
// Remote Tier Handling Tests (3 tests)
//=============================================================================

TEST_F(PortiaCollectiveBridgeTest, HandleRemoteTierSucceeds) {
    EXPECT_EQ(portia_collective_handle_remote_tier(bridge, 12345, 2), 0);
}

TEST_F(PortiaCollectiveBridgeTest, HandleRemoteTierNullFails) {
    EXPECT_EQ(portia_collective_handle_remote_tier(nullptr, 12345, 2), -1);
}

TEST_F(PortiaCollectiveBridgeTest, HandleRemoteTierIgnoresSelf) {
    uint32_t local_id = portia_collective_get_local_id(bridge);
    EXPECT_EQ(portia_collective_handle_remote_tier(bridge, local_id, 2), 0);
}

//=============================================================================
// Instance State Tests (3 tests)
//=============================================================================

TEST_F(PortiaCollectiveBridgeTest, GetInstanceStateSucceeds) {
    portia_collective_update(bridge, 100);
    uint32_t local_id = portia_collective_get_local_id(bridge);
    collective_instance_state_t state;
    EXPECT_EQ(portia_collective_get_instance_state(bridge, local_id, &state), 0);
    EXPECT_EQ(state.instance_id, local_id);
}

TEST_F(PortiaCollectiveBridgeTest, GetInstanceStateNotFound) {
    collective_instance_state_t state;
    EXPECT_EQ(portia_collective_get_instance_state(bridge, 99999, &state), -1);
}

TEST_F(PortiaCollectiveBridgeTest, GetInstanceStateNullFails) {
    collective_instance_state_t state;
    EXPECT_EQ(portia_collective_get_instance_state(nullptr, 1, &state), -1);
    EXPECT_EQ(portia_collective_get_instance_state(bridge, 1, nullptr), -1);
}

//=============================================================================
// Boundary Value Tests (4 tests)
//=============================================================================

TEST_F(PortiaCollectiveBridgeTest, BroadcastTierZero) {
    EXPECT_EQ(portia_collective_broadcast_tier(bridge, 0), 0);
}

TEST_F(PortiaCollectiveBridgeTest, BroadcastTierMax) {
    EXPECT_EQ(portia_collective_broadcast_tier(bridge, 3), 0);
}

TEST_F(PortiaCollectiveBridgeTest, RequestOffloadZeroComplexity) {
    uint32_t target = 0;
    // No other instances, should fail
    EXPECT_EQ(portia_collective_request_offload(bridge, 0.0f, &target), -1);
}

TEST_F(PortiaCollectiveBridgeTest, RequestOffloadMaxComplexity) {
    uint32_t target = 0;
    EXPECT_EQ(portia_collective_request_offload(bridge, 1.0f, &target), -1);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
