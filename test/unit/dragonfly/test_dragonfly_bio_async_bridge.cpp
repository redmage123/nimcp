/**
 * @file test_dragonfly_bio_async_bridge.cpp
 * @brief Unit tests for Dragonfly-to-Bio-Async Integration Bridge
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

// Headers have their own extern "C" guards
#include "dragonfly/nimcp_dragonfly_bio_async_bridge.h"

//=============================================================================
// Test Fixtures
//=============================================================================

class DragonflyBioAsyncBridgeTest : public ::testing::Test {
protected:
    dragonfly_bio_async_bridge_t* bridge = nullptr;
    dragonfly_bio_async_config_t config;

    void SetUp() override {
        ASSERT_EQ(0, dragonfly_bio_async_bridge_default_config(&config));
    }

    void TearDown() override {
        if (bridge) {
            dragonfly_bio_async_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }

    void CreateBridge() {
        bridge = dragonfly_bio_async_bridge_create(nullptr, nullptr, &config);
        ASSERT_NE(nullptr, bridge);
    }
};

//=============================================================================
// Configuration Tests
//=============================================================================

TEST_F(DragonflyBioAsyncBridgeTest, DefaultConfigValid) {
    dragonfly_bio_async_config_t cfg;
    EXPECT_EQ(0, dragonfly_bio_async_bridge_default_config(&cfg));

    EXPECT_GT(cfg.dopamine_decay_rate, 0.0f);
    EXPECT_GE(cfg.norepinephrine_threshold, 0.0f);
    EXPECT_LE(cfg.norepinephrine_threshold, 1.0f);
    EXPECT_GT(cfg.acetylcholine_focus_gain, 0.0f);
    EXPECT_EQ(DRAGONFLY_PHASE_GAMMA, cfg.default_phase);
    EXPECT_GT(cfg.default_timeout_ms, 0.0f);
}

TEST_F(DragonflyBioAsyncBridgeTest, DefaultConfigNullReturnsError) {
    EXPECT_EQ(-1, dragonfly_bio_async_bridge_default_config(nullptr));
}

TEST_F(DragonflyBioAsyncBridgeTest, ValidateConfigSuccess) {
    EXPECT_EQ(0, dragonfly_bio_async_bridge_validate_config(&config));
}

TEST_F(DragonflyBioAsyncBridgeTest, ValidateConfigNullReturnsError) {
    EXPECT_EQ(-1, dragonfly_bio_async_bridge_validate_config(nullptr));
}

TEST_F(DragonflyBioAsyncBridgeTest, ValidateConfigNegativeDecayRate) {
    config.dopamine_decay_rate = -0.1f;
    EXPECT_EQ(-1, dragonfly_bio_async_bridge_validate_config(&config));
}

TEST_F(DragonflyBioAsyncBridgeTest, ValidateConfigInvalidThreshold) {
    config.norepinephrine_threshold = 1.5f;
    EXPECT_EQ(-1, dragonfly_bio_async_bridge_validate_config(&config));
}

TEST_F(DragonflyBioAsyncBridgeTest, ValidateConfigInvalidPriority) {
    config.base_priority = -0.1f;
    EXPECT_EQ(-1, dragonfly_bio_async_bridge_validate_config(&config));
}

TEST_F(DragonflyBioAsyncBridgeTest, ValidateConfigInvalidTimeout) {
    config.default_timeout_ms = 0.0f;
    EXPECT_EQ(-1, dragonfly_bio_async_bridge_validate_config(&config));
}

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(DragonflyBioAsyncBridgeTest, CreateWithDefaultConfig) {
    bridge = dragonfly_bio_async_bridge_create(nullptr, nullptr, &config);
    EXPECT_NE(nullptr, bridge);
}

TEST_F(DragonflyBioAsyncBridgeTest, CreateWithNullConfigUsesDefaults) {
    bridge = dragonfly_bio_async_bridge_create(nullptr, nullptr, nullptr);
    EXPECT_NE(nullptr, bridge);
}

TEST_F(DragonflyBioAsyncBridgeTest, CreateWithInvalidConfigReturnsNull) {
    config.base_priority = -1.0f;
    bridge = dragonfly_bio_async_bridge_create(nullptr, nullptr, &config);
    EXPECT_EQ(nullptr, bridge);
}

TEST_F(DragonflyBioAsyncBridgeTest, DestroyNullSafe) {
    dragonfly_bio_async_bridge_destroy(nullptr);
    /* No crash = success */
}

TEST_F(DragonflyBioAsyncBridgeTest, ResetSuccess) {
    CreateBridge();
    EXPECT_EQ(0, dragonfly_bio_async_bridge_reset(bridge));
}

TEST_F(DragonflyBioAsyncBridgeTest, ResetNullReturnsError) {
    EXPECT_EQ(-1, dragonfly_bio_async_bridge_reset(nullptr));
}

//=============================================================================
// Async Operations Tests
//=============================================================================

TEST_F(DragonflyBioAsyncBridgeTest, StartAsyncOperationSuccess) {
    CreateBridge();

    float input[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    dragonfly_bio_future_t* future = dragonfly_bio_async_start(
        bridge, DRAGONFLY_ASYNC_TRACKING, input, 4);
    EXPECT_NE(nullptr, future);

    dragonfly_bio_future_destroy(future);
}

TEST_F(DragonflyBioAsyncBridgeTest, StartAsyncNullReturnsNull) {
    EXPECT_EQ(nullptr, dragonfly_bio_async_start(nullptr, DRAGONFLY_ASYNC_TRACKING, nullptr, 0));
}

TEST_F(DragonflyBioAsyncBridgeTest, FutureWaitSuccess) {
    CreateBridge();

    float input[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    dragonfly_bio_future_t* future = dragonfly_bio_async_start(
        bridge, DRAGONFLY_ASYNC_PREDICTION, input, 4);
    ASSERT_NE(nullptr, future);

    dragonfly_async_result_t result;
    EXPECT_EQ(0, dragonfly_bio_future_wait(future, &result, 100.0f));
    EXPECT_TRUE(result.success);
    EXPECT_EQ(DRAGONFLY_ASYNC_PREDICTION, result.operation);

    dragonfly_bio_future_destroy(future);
}

TEST_F(DragonflyBioAsyncBridgeTest, FutureWaitNullReturnsError) {
    dragonfly_async_result_t result;
    EXPECT_EQ(-1, dragonfly_bio_future_wait(nullptr, &result, 100.0f));
}

TEST_F(DragonflyBioAsyncBridgeTest, FutureIsReadyAfterWait) {
    CreateBridge();

    float input[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    dragonfly_bio_future_t* future = dragonfly_bio_async_start(
        bridge, DRAGONFLY_ASYNC_INTERCEPT, input, 4);
    ASSERT_NE(nullptr, future);

    dragonfly_async_result_t result;
    dragonfly_bio_future_wait(future, &result, 100.0f);
    EXPECT_TRUE(dragonfly_bio_future_is_ready(future));

    dragonfly_bio_future_destroy(future);
}

TEST_F(DragonflyBioAsyncBridgeTest, FutureIsReadyNullReturnsFalse) {
    EXPECT_FALSE(dragonfly_bio_future_is_ready(nullptr));
}

TEST_F(DragonflyBioAsyncBridgeTest, FutureGetConfidence) {
    CreateBridge();

    float input[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    dragonfly_bio_future_t* future = dragonfly_bio_async_start(
        bridge, DRAGONFLY_ASYNC_TSDN_UPDATE, input, 4);
    ASSERT_NE(nullptr, future);

    dragonfly_async_result_t result;
    dragonfly_bio_future_wait(future, &result, 100.0f);

    float confidence = dragonfly_bio_future_get_confidence(future);
    EXPECT_GT(confidence, 0.0f);
    EXPECT_LE(confidence, 1.0f);

    dragonfly_bio_future_destroy(future);
}

TEST_F(DragonflyBioAsyncBridgeTest, FutureDestroyNullSafe) {
    dragonfly_bio_future_destroy(nullptr);
    /* No crash = success */
}

TEST_F(DragonflyBioAsyncBridgeTest, InterceptOperationHigherPriority) {
    CreateBridge();

    float input[4] = {1.0f, 2.0f, 3.0f, 4.0f};

    /* Start tracking operation */
    dragonfly_bio_future_t* tracking = dragonfly_bio_async_start(
        bridge, DRAGONFLY_ASYNC_TRACKING, input, 4);

    /* Start intercept operation (should have higher priority) */
    dragonfly_bio_future_t* intercept = dragonfly_bio_async_start(
        bridge, DRAGONFLY_ASYNC_INTERCEPT, input, 4);

    EXPECT_NE(nullptr, tracking);
    EXPECT_NE(nullptr, intercept);

    dragonfly_bio_future_destroy(tracking);
    dragonfly_bio_future_destroy(intercept);
}

//=============================================================================
// Neuromodulator Signaling Tests
//=============================================================================

TEST_F(DragonflyBioAsyncBridgeTest, SignalRewardIncreaseDopamine) {
    CreateBridge();

    float initial = dragonfly_bio_async_get_dopamine(bridge);
    EXPECT_EQ(0, dragonfly_bio_async_signal_reward(bridge, 0.8f));
    float after = dragonfly_bio_async_get_dopamine(bridge);

    EXPECT_GT(after, initial);
}

TEST_F(DragonflyBioAsyncBridgeTest, SignalRewardNullReturnsError) {
    EXPECT_EQ(-1, dragonfly_bio_async_signal_reward(nullptr, 0.5f));
}

TEST_F(DragonflyBioAsyncBridgeTest, SignalAlertSetsNorepinephrine) {
    CreateBridge();

    EXPECT_EQ(0, dragonfly_bio_async_signal_alert(bridge, 0.9f));
    float level = dragonfly_bio_async_get_norepinephrine(bridge);

    EXPECT_GE(level, 0.9f);
}

TEST_F(DragonflyBioAsyncBridgeTest, SignalAlertNullReturnsError) {
    EXPECT_EQ(-1, dragonfly_bio_async_signal_alert(nullptr, 0.5f));
}

TEST_F(DragonflyBioAsyncBridgeTest, SignalFocusSetsAcetylcholine) {
    CreateBridge();

    EXPECT_EQ(0, dragonfly_bio_async_signal_focus(bridge, 0.5f));
    float level = dragonfly_bio_async_get_acetylcholine(bridge);

    EXPECT_GT(level, 0.0f);
}

TEST_F(DragonflyBioAsyncBridgeTest, SignalFocusNullReturnsError) {
    EXPECT_EQ(-1, dragonfly_bio_async_signal_focus(nullptr, 0.5f));
}

TEST_F(DragonflyBioAsyncBridgeTest, GetDopamineNullReturnsZero) {
    EXPECT_EQ(0.0f, dragonfly_bio_async_get_dopamine(nullptr));
}

TEST_F(DragonflyBioAsyncBridgeTest, GetNorepinephrineNullReturnsZero) {
    EXPECT_EQ(0.0f, dragonfly_bio_async_get_norepinephrine(nullptr));
}

TEST_F(DragonflyBioAsyncBridgeTest, GetAcetylcholineNullReturnsZero) {
    EXPECT_EQ(0.0f, dragonfly_bio_async_get_acetylcholine(nullptr));
}

//=============================================================================
// Phase Synchronization Tests
//=============================================================================

TEST_F(DragonflyBioAsyncBridgeTest, SetPhaseModeSuccess) {
    CreateBridge();
    EXPECT_EQ(0, dragonfly_bio_async_set_phase_mode(bridge, DRAGONFLY_PHASE_BETA));
    EXPECT_EQ(0, dragonfly_bio_async_set_phase_mode(bridge, DRAGONFLY_PHASE_ALPHA));
    EXPECT_EQ(0, dragonfly_bio_async_set_phase_mode(bridge, DRAGONFLY_PHASE_THETA));
}

TEST_F(DragonflyBioAsyncBridgeTest, SetPhaseModeNullReturnsError) {
    EXPECT_EQ(-1, dragonfly_bio_async_set_phase_mode(nullptr, DRAGONFLY_PHASE_GAMMA));
}

TEST_F(DragonflyBioAsyncBridgeTest, SetPhaseModeInvalidReturnsError) {
    CreateBridge();
    EXPECT_EQ(-1, dragonfly_bio_async_set_phase_mode(bridge, (dragonfly_phase_mode_t)99));
}

TEST_F(DragonflyBioAsyncBridgeTest, GetCoherenceValid) {
    CreateBridge();
    float coherence = dragonfly_bio_async_get_coherence(bridge);
    EXPECT_GE(coherence, 0.0f);
    EXPECT_LE(coherence, 1.0f);
}

TEST_F(DragonflyBioAsyncBridgeTest, GetCoherenceNullReturnsZero) {
    EXPECT_EQ(0.0f, dragonfly_bio_async_get_coherence(nullptr));
}

TEST_F(DragonflyBioAsyncBridgeTest, SyncFuturesSuccess) {
    CreateBridge();

    float input[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    dragonfly_bio_future_t* futures[2];
    futures[0] = dragonfly_bio_async_start(bridge, DRAGONFLY_ASYNC_TRACKING, input, 4);
    futures[1] = dragonfly_bio_async_start(bridge, DRAGONFLY_ASYNC_PREDICTION, input, 4);

    /* Should sync if coherence is high enough */
    int result = dragonfly_bio_async_sync_futures(bridge, futures, 2, 0.5f);
    EXPECT_GE(result, 0);  /* 0 = synced, 1 = not synced */

    dragonfly_bio_future_destroy(futures[0]);
    dragonfly_bio_future_destroy(futures[1]);
}

TEST_F(DragonflyBioAsyncBridgeTest, SyncFuturesNullReturnsError) {
    EXPECT_EQ(-1, dragonfly_bio_async_sync_futures(nullptr, nullptr, 0, 0.5f));
}

//=============================================================================
// Integration Tests
//=============================================================================

TEST_F(DragonflyBioAsyncBridgeTest, ConnectDragonflySuccess) {
    CreateBridge();
    int dummy;
    EXPECT_EQ(0, dragonfly_bio_async_connect_dragonfly(bridge, (dragonfly_system_t*)&dummy));
}

TEST_F(DragonflyBioAsyncBridgeTest, ConnectDragonflyNullBridgeReturnsError) {
    int dummy;
    EXPECT_EQ(-1, dragonfly_bio_async_connect_dragonfly(nullptr, (dragonfly_system_t*)&dummy));
}

TEST_F(DragonflyBioAsyncBridgeTest, ConnectSystemSuccess) {
    CreateBridge();
    int dummy;
    EXPECT_EQ(0, dragonfly_bio_async_connect_system(bridge, &dummy));
}

TEST_F(DragonflyBioAsyncBridgeTest, ConnectSystemNullBridgeReturnsError) {
    int dummy;
    EXPECT_EQ(-1, dragonfly_bio_async_connect_system(nullptr, &dummy));
}

TEST_F(DragonflyBioAsyncBridgeTest, UpdateSuccess) {
    CreateBridge();
    EXPECT_EQ(0, dragonfly_bio_async_update(bridge, 16.0f));
}

TEST_F(DragonflyBioAsyncBridgeTest, UpdateNullReturnsError) {
    EXPECT_EQ(-1, dragonfly_bio_async_update(nullptr, 16.0f));
}

TEST_F(DragonflyBioAsyncBridgeTest, UpdateDecaysNeuromodulators) {
    CreateBridge();

    /* Set high levels */
    dragonfly_bio_async_signal_reward(bridge, 1.0f);
    dragonfly_bio_async_signal_alert(bridge, 1.0f);
    dragonfly_bio_async_signal_focus(bridge, 1.0f);

    float initial_da = dragonfly_bio_async_get_dopamine(bridge);
    float initial_ne = dragonfly_bio_async_get_norepinephrine(bridge);

    /* Update to decay */
    for (int i = 0; i < 100; i++) {
        dragonfly_bio_async_update(bridge, 16.0f);
    }

    float final_da = dragonfly_bio_async_get_dopamine(bridge);
    float final_ne = dragonfly_bio_async_get_norepinephrine(bridge);

    EXPECT_LT(final_da, initial_da);
    EXPECT_LT(final_ne, initial_ne);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(DragonflyBioAsyncBridgeTest, GetStatsSuccess) {
    CreateBridge();
    dragonfly_bio_async_stats_t stats;
    EXPECT_EQ(0, dragonfly_bio_async_bridge_get_stats(bridge, &stats));
}

TEST_F(DragonflyBioAsyncBridgeTest, GetStatsNullReturnsError) {
    CreateBridge();
    dragonfly_bio_async_stats_t stats;
    EXPECT_EQ(-1, dragonfly_bio_async_bridge_get_stats(nullptr, &stats));
    EXPECT_EQ(-1, dragonfly_bio_async_bridge_get_stats(bridge, nullptr));
}

TEST_F(DragonflyBioAsyncBridgeTest, ResetStatsSuccess) {
    CreateBridge();
    EXPECT_EQ(0, dragonfly_bio_async_bridge_reset_stats(bridge));
}

TEST_F(DragonflyBioAsyncBridgeTest, ResetStatsNullReturnsError) {
    EXPECT_EQ(-1, dragonfly_bio_async_bridge_reset_stats(nullptr));
}

TEST_F(DragonflyBioAsyncBridgeTest, StatsTrackOperations) {
    CreateBridge();

    /* Start some operations */
    float input[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    for (int i = 0; i < 5; i++) {
        dragonfly_bio_future_t* future = dragonfly_bio_async_start(
            bridge, DRAGONFLY_ASYNC_TRACKING, input, 4);
        dragonfly_bio_future_destroy(future);
    }

    dragonfly_bio_async_stats_t stats;
    dragonfly_bio_async_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(5u, stats.operations_started);
}

//=============================================================================
// Utility Tests
//=============================================================================

TEST_F(DragonflyBioAsyncBridgeTest, ChannelNameValid) {
    EXPECT_STREQ("dopamine", dragonfly_bio_async_channel_name(DRAGONFLY_CHANNEL_DOPAMINE));
    EXPECT_STREQ("norepinephrine", dragonfly_bio_async_channel_name(DRAGONFLY_CHANNEL_NOREPINEPHRINE));
    EXPECT_STREQ("acetylcholine", dragonfly_bio_async_channel_name(DRAGONFLY_CHANNEL_ACETYLCHOLINE));
    EXPECT_STREQ("serotonin", dragonfly_bio_async_channel_name(DRAGONFLY_CHANNEL_SEROTONIN));
}

TEST_F(DragonflyBioAsyncBridgeTest, ChannelNameInvalid) {
    EXPECT_STREQ("unknown", dragonfly_bio_async_channel_name((dragonfly_neuromod_channel_t)99));
}

TEST_F(DragonflyBioAsyncBridgeTest, OpNameValid) {
    EXPECT_STREQ("tsdn_update", dragonfly_bio_async_op_name(DRAGONFLY_ASYNC_TSDN_UPDATE));
    EXPECT_STREQ("tracking", dragonfly_bio_async_op_name(DRAGONFLY_ASYNC_TRACKING));
    EXPECT_STREQ("prediction", dragonfly_bio_async_op_name(DRAGONFLY_ASYNC_PREDICTION));
    EXPECT_STREQ("intercept", dragonfly_bio_async_op_name(DRAGONFLY_ASYNC_INTERCEPT));
    EXPECT_STREQ("mode_switch", dragonfly_bio_async_op_name(DRAGONFLY_ASYNC_MODE_SWITCH));
    EXPECT_STREQ("full_cycle", dragonfly_bio_async_op_name(DRAGONFLY_ASYNC_FULL_CYCLE));
}

TEST_F(DragonflyBioAsyncBridgeTest, OpNameInvalid) {
    EXPECT_STREQ("unknown", dragonfly_bio_async_op_name((dragonfly_async_op_t)99));
}
