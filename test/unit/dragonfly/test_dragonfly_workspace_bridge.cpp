/**
 * @file test_dragonfly_workspace_bridge.cpp
 * @brief Unit tests for Dragonfly-to-Global Workspace Integration Bridge
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

// Headers have their own extern "C" guards
#include "dragonfly/nimcp_dragonfly_workspace_bridge.h"

//=============================================================================
// Test Fixtures
//=============================================================================

class DragonflyWorkspaceBridgeTest : public ::testing::Test {
protected:
    dragonfly_workspace_bridge_t* bridge = nullptr;
    dragonfly_ws_config_t config;

    void SetUp() override {
        ASSERT_EQ(0, dragonfly_ws_bridge_default_config(&config));
    }

    void TearDown() override {
        if (bridge) {
            dragonfly_ws_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }

    void CreateBridge() {
        bridge = dragonfly_ws_bridge_create(nullptr, nullptr, &config);
        ASSERT_NE(nullptr, bridge);
    }
};

//=============================================================================
// Configuration Tests
//=============================================================================

TEST_F(DragonflyWorkspaceBridgeTest, DefaultConfigValid) {
    dragonfly_ws_config_t cfg;
    EXPECT_EQ(0, dragonfly_ws_bridge_default_config(&cfg));

    EXPECT_GE(cfg.ignition_threshold, 0.0f);
    EXPECT_LE(cfg.ignition_threshold, 1.0f);
    EXPECT_GE(cfg.base_salience, 0.0f);
    EXPECT_LE(cfg.base_salience, 1.0f);
    EXPECT_GE(cfg.target_detection_salience, 0.0f);
    EXPECT_LE(cfg.target_detection_salience, 1.0f);
    EXPECT_EQ(WS_SUBSCRIBE_ALL, cfg.subscribe_mode);
}

TEST_F(DragonflyWorkspaceBridgeTest, DefaultConfigNullReturnsError) {
    EXPECT_EQ(-1, dragonfly_ws_bridge_default_config(nullptr));
}

TEST_F(DragonflyWorkspaceBridgeTest, ValidateConfigSuccess) {
    EXPECT_EQ(0, dragonfly_ws_bridge_validate_config(&config));
}

TEST_F(DragonflyWorkspaceBridgeTest, ValidateConfigNullReturnsError) {
    EXPECT_EQ(-1, dragonfly_ws_bridge_validate_config(nullptr));
}

TEST_F(DragonflyWorkspaceBridgeTest, ValidateConfigInvalidThreshold) {
    config.ignition_threshold = 1.5f;
    EXPECT_EQ(-1, dragonfly_ws_bridge_validate_config(&config));
}

TEST_F(DragonflyWorkspaceBridgeTest, ValidateConfigInvalidSalience) {
    config.base_salience = -0.1f;
    EXPECT_EQ(-1, dragonfly_ws_bridge_validate_config(&config));
}

TEST_F(DragonflyWorkspaceBridgeTest, ValidateConfigNegativeDecay) {
    config.broadcast_decay_rate = -0.1f;
    EXPECT_EQ(-1, dragonfly_ws_bridge_validate_config(&config));
}

TEST_F(DragonflyWorkspaceBridgeTest, ValidateConfigInvalidContextWeight) {
    config.context_influence_weight = 1.5f;
    EXPECT_EQ(-1, dragonfly_ws_bridge_validate_config(&config));
}

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(DragonflyWorkspaceBridgeTest, CreateWithDefaultConfig) {
    bridge = dragonfly_ws_bridge_create(nullptr, nullptr, &config);
    EXPECT_NE(nullptr, bridge);
}

TEST_F(DragonflyWorkspaceBridgeTest, CreateWithNullConfigUsesDefaults) {
    bridge = dragonfly_ws_bridge_create(nullptr, nullptr, nullptr);
    EXPECT_NE(nullptr, bridge);
}

TEST_F(DragonflyWorkspaceBridgeTest, CreateWithInvalidConfigReturnsNull) {
    config.ignition_threshold = -1.0f;
    bridge = dragonfly_ws_bridge_create(nullptr, nullptr, &config);
    EXPECT_EQ(nullptr, bridge);
}

TEST_F(DragonflyWorkspaceBridgeTest, DestroyNullSafe) {
    dragonfly_ws_bridge_destroy(nullptr);
    /* No crash = success */
}

TEST_F(DragonflyWorkspaceBridgeTest, ResetSuccess) {
    CreateBridge();
    EXPECT_EQ(0, dragonfly_ws_bridge_reset(bridge));
}

TEST_F(DragonflyWorkspaceBridgeTest, ResetNullReturnsError) {
    EXPECT_EQ(-1, dragonfly_ws_bridge_reset(nullptr));
}

//=============================================================================
// Competition and Broadcast Tests
//=============================================================================

TEST_F(DragonflyWorkspaceBridgeTest, CompeteHighSalienceWins) {
    CreateBridge();

    float content[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    dragonfly_ws_compete_result_t result = dragonfly_ws_compete(
        bridge,
        WS_CONTENT_TARGET_POSITION,
        content,
        4,
        0.9f  /* High salience */
    );

    EXPECT_EQ(WS_COMPETE_WON, result);
}

TEST_F(DragonflyWorkspaceBridgeTest, CompeteLowSalienceIgnored) {
    CreateBridge();

    float content[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    dragonfly_ws_compete_result_t result = dragonfly_ws_compete(
        bridge,
        WS_CONTENT_TARGET_POSITION,
        content,
        4,
        0.1f  /* Very low salience, below ignition threshold */
    );

    EXPECT_EQ(WS_COMPETE_IGNORED, result);
}

TEST_F(DragonflyWorkspaceBridgeTest, CompeteNullReturnsIgnored) {
    float content[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    dragonfly_ws_compete_result_t result = dragonfly_ws_compete(
        nullptr,
        WS_CONTENT_TARGET_POSITION,
        content,
        4,
        0.8f
    );

    EXPECT_EQ(WS_COMPETE_IGNORED, result);
}

TEST_F(DragonflyWorkspaceBridgeTest, BroadcastTargetSuccess) {
    CreateBridge();

    EXPECT_EQ(0, dragonfly_ws_broadcast_target(bridge, 1.0f, 2.0f, 3.0f, 0.1f, 0.2f, 0.3f));
}

TEST_F(DragonflyWorkspaceBridgeTest, BroadcastTargetNullReturnsError) {
    EXPECT_EQ(-1, dragonfly_ws_broadcast_target(nullptr, 1.0f, 2.0f, 3.0f, 0.1f, 0.2f, 0.3f));
}

TEST_F(DragonflyWorkspaceBridgeTest, BroadcastInterceptSuccess) {
    CreateBridge();

    EXPECT_EQ(0, dragonfly_ws_broadcast_intercept(bridge, 5.0f, 5.0f, 5.0f, 100.0f));
}

TEST_F(DragonflyWorkspaceBridgeTest, BroadcastInterceptNullReturnsError) {
    EXPECT_EQ(-1, dragonfly_ws_broadcast_intercept(nullptr, 5.0f, 5.0f, 5.0f, 100.0f));
}

TEST_F(DragonflyWorkspaceBridgeTest, BroadcastAlertSuccess) {
    CreateBridge();

    EXPECT_EQ(0, dragonfly_ws_broadcast_alert(bridge, 0.9f));
}

TEST_F(DragonflyWorkspaceBridgeTest, BroadcastAlertNullReturnsError) {
    EXPECT_EQ(-1, dragonfly_ws_broadcast_alert(nullptr, 0.9f));
}

//=============================================================================
// Subscription and Reception Tests
//=============================================================================

TEST_F(DragonflyWorkspaceBridgeTest, SubscribeSuccess) {
    CreateBridge();
    EXPECT_EQ(0, dragonfly_ws_subscribe(bridge, WS_SUBSCRIBE_MOTOR));
    EXPECT_EQ(0, dragonfly_ws_subscribe(bridge, WS_SUBSCRIBE_VISUAL));
    EXPECT_EQ(0, dragonfly_ws_subscribe(bridge, WS_SUBSCRIBE_EXECUTIVE));
}

TEST_F(DragonflyWorkspaceBridgeTest, SubscribeNullReturnsError) {
    EXPECT_EQ(-1, dragonfly_ws_subscribe(nullptr, WS_SUBSCRIBE_ALL));
}

TEST_F(DragonflyWorkspaceBridgeTest, SubscribeInvalidModeReturnsError) {
    CreateBridge();
    EXPECT_EQ(-1, dragonfly_ws_subscribe(bridge, (dragonfly_ws_subscribe_mode_t)99));
}

TEST_F(DragonflyWorkspaceBridgeTest, UnsubscribeSuccess) {
    CreateBridge();
    dragonfly_ws_subscribe(bridge, WS_SUBSCRIBE_MOTOR);
    EXPECT_EQ(0, dragonfly_ws_unsubscribe(bridge, WS_SUBSCRIBE_MOTOR));
}

TEST_F(DragonflyWorkspaceBridgeTest, UnsubscribeNullReturnsError) {
    EXPECT_EQ(-1, dragonfly_ws_unsubscribe(nullptr, WS_SUBSCRIBE_ALL));
}

TEST_F(DragonflyWorkspaceBridgeTest, HasBroadcastInitiallyFalse) {
    CreateBridge();
    EXPECT_FALSE(dragonfly_ws_has_broadcast(bridge));
}

TEST_F(DragonflyWorkspaceBridgeTest, HasBroadcastNullReturnsFalse) {
    EXPECT_FALSE(dragonfly_ws_has_broadcast(nullptr));
}

TEST_F(DragonflyWorkspaceBridgeTest, ReceiveNoDataReturnsOne) {
    CreateBridge();
    dragonfly_ws_received_t received;
    EXPECT_EQ(1, dragonfly_ws_receive(bridge, &received));
}

TEST_F(DragonflyWorkspaceBridgeTest, ReceiveNullReturnsError) {
    CreateBridge();
    dragonfly_ws_received_t received;
    EXPECT_EQ(-1, dragonfly_ws_receive(nullptr, &received));
    EXPECT_EQ(-1, dragonfly_ws_receive(bridge, nullptr));
}

//=============================================================================
// Context Integration Tests
//=============================================================================

TEST_F(DragonflyWorkspaceBridgeTest, GetContextSuccess) {
    CreateBridge();

    float context[32];
    int dim = dragonfly_ws_get_context(bridge, context, 32);
    EXPECT_GE(dim, 0);
}

TEST_F(DragonflyWorkspaceBridgeTest, GetContextNullReturnsError) {
    CreateBridge();
    float context[32];
    EXPECT_EQ(-1, dragonfly_ws_get_context(nullptr, context, 32));
    EXPECT_EQ(-1, dragonfly_ws_get_context(bridge, nullptr, 32));
}

TEST_F(DragonflyWorkspaceBridgeTest, ApplyContextSuccess) {
    CreateBridge();
    EXPECT_EQ(0, dragonfly_ws_apply_context(bridge));
}

TEST_F(DragonflyWorkspaceBridgeTest, ApplyContextNullReturnsError) {
    EXPECT_EQ(-1, dragonfly_ws_apply_context(nullptr));
}

TEST_F(DragonflyWorkspaceBridgeTest, GetContextRelevanceValid) {
    CreateBridge();
    float relevance = dragonfly_ws_get_context_relevance(bridge);
    EXPECT_GE(relevance, 0.0f);
    EXPECT_LE(relevance, 1.0f);
}

TEST_F(DragonflyWorkspaceBridgeTest, GetContextRelevanceNullReturnsZero) {
    EXPECT_EQ(0.0f, dragonfly_ws_get_context_relevance(nullptr));
}

//=============================================================================
// Integration Tests
//=============================================================================

TEST_F(DragonflyWorkspaceBridgeTest, ConnectDragonflySuccess) {
    CreateBridge();
    int dummy;
    EXPECT_EQ(0, dragonfly_ws_connect_dragonfly(bridge, (dragonfly_system_t*)&dummy));
}

TEST_F(DragonflyWorkspaceBridgeTest, ConnectDragonflyNullBridgeReturnsError) {
    int dummy;
    EXPECT_EQ(-1, dragonfly_ws_connect_dragonfly(nullptr, (dragonfly_system_t*)&dummy));
}

TEST_F(DragonflyWorkspaceBridgeTest, ConnectWorkspaceSuccess) {
    CreateBridge();
    int dummy;
    EXPECT_EQ(0, dragonfly_ws_connect_workspace(bridge, &dummy));
}

TEST_F(DragonflyWorkspaceBridgeTest, ConnectWorkspaceNullBridgeReturnsError) {
    int dummy;
    EXPECT_EQ(-1, dragonfly_ws_connect_workspace(nullptr, &dummy));
}

TEST_F(DragonflyWorkspaceBridgeTest, UpdateSuccess) {
    CreateBridge();
    EXPECT_EQ(0, dragonfly_ws_update(bridge, 16.0f));
}

TEST_F(DragonflyWorkspaceBridgeTest, UpdateNullReturnsError) {
    EXPECT_EQ(-1, dragonfly_ws_update(nullptr, 16.0f));
}

TEST_F(DragonflyWorkspaceBridgeTest, UpdateDecaysBroadcast) {
    CreateBridge();

    /* Create a broadcast */
    dragonfly_ws_broadcast_target(bridge, 1.0f, 2.0f, 3.0f, 0.1f, 0.2f, 0.3f);

    /* Run one update to trigger initial occupancy computation */
    dragonfly_ws_update(bridge, 1.0f);

    dragonfly_ws_stats_t initial_stats;
    dragonfly_ws_bridge_get_stats(bridge, &initial_stats);

    /* Update many times with large dt to force decay */
    for (int i = 0; i < 100; i++) {
        dragonfly_ws_update(bridge, 100.0f);
    }

    dragonfly_ws_stats_t final_stats;
    dragonfly_ws_bridge_get_stats(bridge, &final_stats);

    /* Workspace occupancy should have decreased after decay */
    EXPECT_LE(final_stats.workspace_occupancy_pct, initial_stats.workspace_occupancy_pct);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(DragonflyWorkspaceBridgeTest, GetStatsSuccess) {
    CreateBridge();
    dragonfly_ws_stats_t stats;
    EXPECT_EQ(0, dragonfly_ws_bridge_get_stats(bridge, &stats));
}

TEST_F(DragonflyWorkspaceBridgeTest, GetStatsNullReturnsError) {
    CreateBridge();
    dragonfly_ws_stats_t stats;
    EXPECT_EQ(-1, dragonfly_ws_bridge_get_stats(nullptr, &stats));
    EXPECT_EQ(-1, dragonfly_ws_bridge_get_stats(bridge, nullptr));
}

TEST_F(DragonflyWorkspaceBridgeTest, ResetStatsSuccess) {
    CreateBridge();
    EXPECT_EQ(0, dragonfly_ws_bridge_reset_stats(bridge));
}

TEST_F(DragonflyWorkspaceBridgeTest, ResetStatsNullReturnsError) {
    EXPECT_EQ(-1, dragonfly_ws_bridge_reset_stats(nullptr));
}

TEST_F(DragonflyWorkspaceBridgeTest, StatsTrackCompetitions) {
    CreateBridge();

    float content[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    for (int i = 0; i < 5; i++) {
        dragonfly_ws_compete(bridge, WS_CONTENT_TARGET_POSITION, content, 4, 0.8f);
    }

    dragonfly_ws_stats_t stats;
    dragonfly_ws_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(5u, stats.competitions_entered);
}

TEST_F(DragonflyWorkspaceBridgeTest, StatsTrackBroadcasts) {
    CreateBridge();

    for (int i = 0; i < 3; i++) {
        dragonfly_ws_broadcast_target(bridge, 1.0f, 2.0f, 3.0f, 0.1f, 0.2f, 0.3f);
    }

    dragonfly_ws_stats_t stats;
    dragonfly_ws_bridge_get_stats(bridge, &stats);
    EXPECT_GE(stats.broadcasts_sent, 3u);
}

//=============================================================================
// Utility Tests
//=============================================================================

TEST_F(DragonflyWorkspaceBridgeTest, ContentNameValid) {
    EXPECT_STREQ("target_position", dragonfly_ws_content_name(WS_CONTENT_TARGET_POSITION));
    EXPECT_STREQ("target_velocity", dragonfly_ws_content_name(WS_CONTENT_TARGET_VELOCITY));
    EXPECT_STREQ("predicted_trajectory", dragonfly_ws_content_name(WS_CONTENT_PREDICTED_TRAJECTORY));
    EXPECT_STREQ("intercept_point", dragonfly_ws_content_name(WS_CONTENT_INTERCEPT_POINT));
    EXPECT_STREQ("tsdn_activation", dragonfly_ws_content_name(WS_CONTENT_TSDN_ACTIVATION));
    EXPECT_STREQ("mode_state", dragonfly_ws_content_name(WS_CONTENT_MODE_STATE));
    EXPECT_STREQ("alert_signal", dragonfly_ws_content_name(WS_CONTENT_ALERT_SIGNAL));
}

TEST_F(DragonflyWorkspaceBridgeTest, ContentNameInvalid) {
    EXPECT_STREQ("unknown", dragonfly_ws_content_name((dragonfly_ws_content_t)99));
}

TEST_F(DragonflyWorkspaceBridgeTest, ResultNameValid) {
    EXPECT_STREQ("won", dragonfly_ws_result_name(WS_COMPETE_WON));
    EXPECT_STREQ("lost", dragonfly_ws_result_name(WS_COMPETE_LOST));
    EXPECT_STREQ("queued", dragonfly_ws_result_name(WS_COMPETE_QUEUED));
    EXPECT_STREQ("ignored", dragonfly_ws_result_name(WS_COMPETE_IGNORED));
}

TEST_F(DragonflyWorkspaceBridgeTest, ResultNameInvalid) {
    EXPECT_STREQ("unknown", dragonfly_ws_result_name((dragonfly_ws_compete_result_t)99));
}

//=============================================================================
// Integration Scenario Tests
//=============================================================================

TEST_F(DragonflyWorkspaceBridgeTest, FullCompetitionCycle) {
    CreateBridge();

    /* Target detection with high salience */
    dragonfly_ws_compete_result_t result1 = dragonfly_ws_compete(
        bridge,
        WS_CONTENT_TARGET_POSITION,
        nullptr,
        0,
        0.95f
    );
    EXPECT_EQ(WS_COMPETE_WON, result1);

    /* Velocity update with lower salience */
    dragonfly_ws_compete_result_t result2 = dragonfly_ws_compete(
        bridge,
        WS_CONTENT_TARGET_VELOCITY,
        nullptr,
        0,
        0.6f
    );
    /* Should still compete, might be queued */
    EXPECT_NE(WS_COMPETE_IGNORED, result2);

    /* Alert with high salience */
    dragonfly_ws_compete_result_t result3 = dragonfly_ws_compete(
        bridge,
        WS_CONTENT_ALERT_SIGNAL,
        nullptr,
        0,
        0.99f
    );
    EXPECT_EQ(WS_COMPETE_WON, result3);

    /* Check stats */
    dragonfly_ws_stats_t stats;
    dragonfly_ws_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(3u, stats.competitions_entered);
    EXPECT_GE(stats.competitions_won, 2u);
}

TEST_F(DragonflyWorkspaceBridgeTest, SubscriptionManagement) {
    CreateBridge();

    /* Subscribe to multiple modes */
    dragonfly_ws_subscribe(bridge, WS_SUBSCRIBE_MOTOR);
    dragonfly_ws_subscribe(bridge, WS_SUBSCRIBE_VISUAL);
    dragonfly_ws_subscribe(bridge, WS_SUBSCRIBE_EXECUTIVE);

    /* Unsubscribe from one */
    dragonfly_ws_unsubscribe(bridge, WS_SUBSCRIBE_MOTOR);

    /* Bridge should still be functional */
    EXPECT_EQ(0, dragonfly_ws_broadcast_target(bridge, 1.0f, 2.0f, 3.0f, 0.1f, 0.2f, 0.3f));
}
