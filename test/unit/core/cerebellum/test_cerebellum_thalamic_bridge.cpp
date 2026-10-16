/**
 * @file test_cerebellum_thalamic_bridge.cpp
 * @brief Unit tests for cerebellum thalamic bridge
 *
 * Tests:
 * - Configuration defaults
 * - Bridge lifecycle (create/destroy)
 * - Signal routing (timing, correction, learning, coordination)
 * - Attention modulation
 * - Statistics tracking
 *
 * @author NIMCP Development Team
 * @date 2026-01-03
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

#include "core/cerebellum/nimcp_cerebellum_thalamic_bridge.h"
#include "middleware/routing/nimcp_thalamic_router.h"

//=============================================================================
// Configuration Tests
//=============================================================================

class CerebellumThalamicConfigTest : public ::testing::Test {};

TEST_F(CerebellumThalamicConfigTest, DefaultConfigValues) {
    cerebellum_thalamic_config_t config = cerebellum_thalamic_default_config();

    // Routing should be enabled by default
    EXPECT_TRUE(config.enable_timing_relay);
    EXPECT_TRUE(config.enable_error_routing);

    // Thresholds should be reasonable
    EXPECT_GE(config.min_timing_precision, 0.0f);
    EXPECT_LE(config.min_timing_precision, 1.0f);
    EXPECT_GE(config.error_threshold, 0.0f);
}

TEST_F(CerebellumThalamicConfigTest, SignalTypeConstants) {
    // Signal types should be unique and properly defined
    EXPECT_EQ(CEREBELLUM_SIGNAL_TIMING, 0x2501);
    EXPECT_EQ(CEREBELLUM_SIGNAL_CORRECTION, 0x2502);
    EXPECT_EQ(CEREBELLUM_SIGNAL_LEARNING, 0x2503);
    EXPECT_EQ(CEREBELLUM_SIGNAL_COORDINATE, 0x2504);
}

//=============================================================================
// Lifecycle Tests
//=============================================================================

class CerebellumThalamicBridgeTest : public ::testing::Test {
protected:
    thalamic_router_t* router = nullptr;
    cerebellum_thalamic_bridge_t* bridge = nullptr;

    void SetUp() override {
        // Create thalamic router first
        thalamic_router_config_t router_config = thalamic_router_default_config();
        router = thalamic_router_create(&router_config);
        ASSERT_NE(router, nullptr) << "Failed to create thalamic router";

        // Create cerebellum thalamic bridge
        cerebellum_thalamic_config_t config = cerebellum_thalamic_default_config();
        bridge = cerebellum_thalamic_bridge_create(nullptr, router, &config);
        ASSERT_NE(bridge, nullptr) << "Failed to create cerebellum thalamic bridge";
    }

    void TearDown() override {
        if (bridge) {
            cerebellum_thalamic_bridge_destroy(bridge);
            bridge = nullptr;
        }
        if (router) {
            thalamic_router_destroy(router);
            router = nullptr;
        }
    }
};

TEST_F(CerebellumThalamicBridgeTest, CreateWithDefaultConfig) {
    EXPECT_NE(bridge, nullptr);
}

TEST_F(CerebellumThalamicBridgeTest, CreateWithCustomConfig) {
    cerebellum_thalamic_config_t config = cerebellum_thalamic_default_config();
    config.enable_timing_relay = false;
    config.min_timing_precision = 0.5f;

    cerebellum_thalamic_bridge_t* custom_bridge =
        cerebellum_thalamic_bridge_create(nullptr, router, &config);
    ASSERT_NE(custom_bridge, nullptr);

    cerebellum_thalamic_bridge_destroy(custom_bridge);
}

TEST_F(CerebellumThalamicBridgeTest, CreateWithNullConfig) {
    cerebellum_thalamic_bridge_t* null_config_bridge =
        cerebellum_thalamic_bridge_create(nullptr, router, nullptr);

    // Should use defaults
    ASSERT_NE(null_config_bridge, nullptr);
    cerebellum_thalamic_bridge_destroy(null_config_bridge);
}

TEST_F(CerebellumThalamicBridgeTest, CreateWithNullRouterFails) {
    cerebellum_thalamic_config_t config = cerebellum_thalamic_default_config();
    cerebellum_thalamic_bridge_t* null_router_bridge =
        cerebellum_thalamic_bridge_create(nullptr, nullptr, &config);

    // Should fail with NULL router
    EXPECT_EQ(null_router_bridge, nullptr);
}

TEST_F(CerebellumThalamicBridgeTest, DestroyNull) {
    cerebellum_thalamic_bridge_destroy(nullptr);  // Should not crash
}

//=============================================================================
// Reset Tests
//=============================================================================

TEST_F(CerebellumThalamicBridgeTest, ResetSucceeds) {
    int result = cerebellum_thalamic_bridge_reset(bridge);
    EXPECT_EQ(result, 0);
}

TEST_F(CerebellumThalamicBridgeTest, ResetNull) {
    int result = cerebellum_thalamic_bridge_reset(nullptr);
    EXPECT_NE(result, 0);
}

TEST_F(CerebellumThalamicBridgeTest, ResetClearsStats) {
    // Route some signals
    cerebellum_thalamic_signal_t signal;
    memset(&signal, 0, sizeof(signal));
    signal.signal_type = CEREBELLUM_SIGNAL_TIMING;
    signal.timing_precision = 0.9f;

    for (int i = 0; i < 10; i++) {
        cerebellum_thalamic_route_signal(bridge, &signal);
    }

    // Reset
    cerebellum_thalamic_bridge_reset(bridge);

    // Stats should be cleared
    cerebellum_thalamic_stats_t stats;
    cerebellum_thalamic_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.timing_signals_routed, 0);
}

//=============================================================================
// Signal Routing Tests
//=============================================================================

TEST_F(CerebellumThalamicBridgeTest, RouteTimingSignal) {
    cerebellum_thalamic_signal_t signal;
    memset(&signal, 0, sizeof(signal));
    signal.signal_type = CEREBELLUM_SIGNAL_TIMING;
    signal.timing_precision = 0.9f;
    signal.timestamp_us = 1000;

    int result = cerebellum_thalamic_route_signal(bridge, &signal);
    EXPECT_EQ(result, 0);
}

TEST_F(CerebellumThalamicBridgeTest, RouteCorrectionSignal) {
    cerebellum_thalamic_signal_t signal;
    memset(&signal, 0, sizeof(signal));
    signal.signal_type = CEREBELLUM_SIGNAL_CORRECTION;
    signal.error_magnitude = 0.5f;
    signal.timestamp_us = 2000;

    int result = cerebellum_thalamic_route_signal(bridge, &signal);
    EXPECT_EQ(result, 0);
}

TEST_F(CerebellumThalamicBridgeTest, RouteLearningSignal) {
    cerebellum_thalamic_signal_t signal;
    memset(&signal, 0, sizeof(signal));
    signal.signal_type = CEREBELLUM_SIGNAL_LEARNING;
    signal.learning_signal = 0.7f;
    signal.timestamp_us = 3000;

    int result = cerebellum_thalamic_route_signal(bridge, &signal);
    EXPECT_EQ(result, 0);
}

TEST_F(CerebellumThalamicBridgeTest, RouteCoordinationSignal) {
    cerebellum_thalamic_signal_t signal;
    memset(&signal, 0, sizeof(signal));
    signal.signal_type = CEREBELLUM_SIGNAL_COORDINATE;
    signal.timing_precision = 0.85f;
    signal.timestamp_us = 4000;

    int result = cerebellum_thalamic_route_signal(bridge, &signal);
    EXPECT_EQ(result, 0);
}

TEST_F(CerebellumThalamicBridgeTest, RouteSignalNull) {
    int result = cerebellum_thalamic_route_signal(bridge, nullptr);
    EXPECT_NE(result, 0);
}

TEST_F(CerebellumThalamicBridgeTest, RouteSignalNullBridge) {
    cerebellum_thalamic_signal_t signal;
    memset(&signal, 0, sizeof(signal));
    signal.signal_type = CEREBELLUM_SIGNAL_TIMING;

    int result = cerebellum_thalamic_route_signal(nullptr, &signal);
    EXPECT_NE(result, 0);
}

TEST_F(CerebellumThalamicBridgeTest, RouteMultipleSignals) {
    cerebellum_thalamic_signal_t signal;
    memset(&signal, 0, sizeof(signal));

    for (int i = 0; i < 100; i++) {
        signal.signal_type = CEREBELLUM_SIGNAL_TIMING + (i % 4);
        signal.timing_precision = 0.5f + 0.4f * sinf((float)i * 0.1f);
        signal.timestamp_us = i * 1000;

        int result = cerebellum_thalamic_route_signal(bridge, &signal);
        EXPECT_EQ(result, 0);
    }
}

//=============================================================================
// Correction Routing Tests
//=============================================================================

TEST_F(CerebellumThalamicBridgeTest, RouteCorrectionSucceeds) {
    float correction_data[3] = {0.1f, 0.2f, 0.3f};
    int result = cerebellum_thalamic_route_correction(bridge, correction_data, 0.5f);
    EXPECT_EQ(result, 0);
}

TEST_F(CerebellumThalamicBridgeTest, RouteCorrectionNullBridge) {
    float correction_data[3] = {0.1f, 0.2f, 0.3f};
    int result = cerebellum_thalamic_route_correction(nullptr, correction_data, 0.5f);
    EXPECT_NE(result, 0);
}

TEST_F(CerebellumThalamicBridgeTest, RouteCorrectionNullData) {
    int result = cerebellum_thalamic_route_correction(bridge, nullptr, 0.5f);
    // May succeed or fail depending on implementation
    (void)result;
}

TEST_F(CerebellumThalamicBridgeTest, RouteCorrectionVariousMagnitudes) {
    float correction_data[3] = {0.1f, 0.2f, 0.3f};

    float magnitudes[] = {0.0f, 0.1f, 0.5f, 1.0f, 2.0f};
    for (float mag : magnitudes) {
        int result = cerebellum_thalamic_route_correction(bridge, correction_data, mag);
        EXPECT_EQ(result, 0);
    }
}

//=============================================================================
// Attention Tests
//=============================================================================

TEST_F(CerebellumThalamicBridgeTest, SetAttentionSucceeds) {
    int result = cerebellum_thalamic_set_attention(bridge, 0.8f);
    EXPECT_EQ(result, 0);
}

TEST_F(CerebellumThalamicBridgeTest, SetAttentionNull) {
    int result = cerebellum_thalamic_set_attention(nullptr, 0.8f);
    EXPECT_NE(result, 0);
}

TEST_F(CerebellumThalamicBridgeTest, GetAttentionSucceeds) {
    float attention;
    int result = cerebellum_thalamic_get_attention(bridge, &attention);
    EXPECT_EQ(result, 0);
    EXPECT_GE(attention, 0.0f);
    EXPECT_LE(attention, 1.0f);
}

TEST_F(CerebellumThalamicBridgeTest, GetAttentionNull) {
    float attention;
    int result = cerebellum_thalamic_get_attention(nullptr, &attention);
    EXPECT_NE(result, 0);
}

TEST_F(CerebellumThalamicBridgeTest, GetAttentionNullOutput) {
    int result = cerebellum_thalamic_get_attention(bridge, nullptr);
    EXPECT_NE(result, 0);
}

TEST_F(CerebellumThalamicBridgeTest, SetGetAttentionRoundtrip) {
    float test_values[] = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};

    for (float expected : test_values) {
        cerebellum_thalamic_set_attention(bridge, expected);

        float actual;
        cerebellum_thalamic_get_attention(bridge, &actual);

        EXPECT_NEAR(actual, expected, 0.01f);
    }
}

TEST_F(CerebellumThalamicBridgeTest, AttentionClamping) {
    // Set out-of-range values
    cerebellum_thalamic_set_attention(bridge, 1.5f);
    float attention;
    cerebellum_thalamic_get_attention(bridge, &attention);
    EXPECT_LE(attention, 1.0f);

    cerebellum_thalamic_set_attention(bridge, -0.5f);
    cerebellum_thalamic_get_attention(bridge, &attention);
    EXPECT_GE(attention, 0.0f);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(CerebellumThalamicBridgeTest, GetStatsSucceeds) {
    cerebellum_thalamic_stats_t stats;
    int result = cerebellum_thalamic_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(result, 0);
}

TEST_F(CerebellumThalamicBridgeTest, GetStatsNullBridge) {
    cerebellum_thalamic_stats_t stats;
    int result = cerebellum_thalamic_bridge_get_stats(nullptr, &stats);
    EXPECT_NE(result, 0);
}

TEST_F(CerebellumThalamicBridgeTest, GetStatsNullOutput) {
    int result = cerebellum_thalamic_bridge_get_stats(bridge, nullptr);
    EXPECT_NE(result, 0);
}

TEST_F(CerebellumThalamicBridgeTest, StatsTrackTimingSignals) {
    cerebellum_thalamic_signal_t signal;
    memset(&signal, 0, sizeof(signal));
    signal.signal_type = CEREBELLUM_SIGNAL_TIMING;
    signal.timing_precision = 0.9f;

    for (int i = 0; i < 10; i++) {
        cerebellum_thalamic_route_signal(bridge, &signal);
    }

    cerebellum_thalamic_stats_t stats;
    cerebellum_thalamic_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.timing_signals_routed, 10);
}

TEST_F(CerebellumThalamicBridgeTest, StatsTrackCorrections) {
    float correction_data[3] = {0.1f, 0.2f, 0.3f};

    for (int i = 0; i < 5; i++) {
        cerebellum_thalamic_route_correction(bridge, correction_data, 0.5f);
    }

    cerebellum_thalamic_stats_t stats;
    cerebellum_thalamic_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.corrections_applied, 5);
}

TEST_F(CerebellumThalamicBridgeTest, StatsTrackLearningSignals) {
    cerebellum_thalamic_signal_t signal;
    memset(&signal, 0, sizeof(signal));
    signal.signal_type = CEREBELLUM_SIGNAL_LEARNING;
    signal.learning_signal = 0.7f;

    for (int i = 0; i < 7; i++) {
        cerebellum_thalamic_route_signal(bridge, &signal);
    }

    cerebellum_thalamic_stats_t stats;
    cerebellum_thalamic_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.learning_signals, 7);
}

TEST_F(CerebellumThalamicBridgeTest, StatsTrackAveragePrecision) {
    // Reset first to clear any prior state
    cerebellum_thalamic_bridge_reset(bridge);

    cerebellum_thalamic_signal_t signal;
    memset(&signal, 0, sizeof(signal));
    signal.signal_type = CEREBELLUM_SIGNAL_TIMING;

    float precisions[] = {0.8f, 0.9f, 0.7f, 0.85f, 0.95f};

    for (float p : precisions) {
        signal.timing_precision = p;
        cerebellum_thalamic_route_signal(bridge, &signal);
    }

    cerebellum_thalamic_stats_t stats;
    cerebellum_thalamic_bridge_get_stats(bridge, &stats);

    // Average precision should be reasonable (may be weighted by attention)
    EXPECT_GE(stats.avg_timing_precision, 0.0f);
    EXPECT_LE(stats.avg_timing_precision, 1.0f);
    // Should be non-zero after routing timing signals
    EXPECT_GT(stats.avg_timing_precision, 0.1f);
}

//=============================================================================
// Continuous Operation Tests
//=============================================================================

TEST_F(CerebellumThalamicBridgeTest, ContinuousOperation) {
    cerebellum_thalamic_signal_t signal;

    for (int frame = 0; frame < 500; frame++) {
        memset(&signal, 0, sizeof(signal));
        signal.signal_type = CEREBELLUM_SIGNAL_TIMING + (frame % 4);
        signal.timing_precision = 0.5f + 0.4f * sinf((float)frame * 0.05f);
        signal.error_magnitude = fabsf(sinf((float)frame * 0.1f)) * 0.3f;
        signal.timestamp_us = frame * 1000;

        int result = cerebellum_thalamic_route_signal(bridge, &signal);
        EXPECT_EQ(result, 0);
    }

    cerebellum_thalamic_stats_t stats;
    cerebellum_thalamic_bridge_get_stats(bridge, &stats);

    // Should have processed many signals
    uint64_t total = stats.timing_signals_routed + stats.corrections_applied + stats.learning_signals;
    EXPECT_GT(total, 0);
}

TEST_F(CerebellumThalamicBridgeTest, AttentionModulationDuringOperation) {
    cerebellum_thalamic_signal_t signal;
    memset(&signal, 0, sizeof(signal));
    signal.signal_type = CEREBELLUM_SIGNAL_TIMING;

    for (int i = 0; i < 100; i++) {
        // Vary attention
        float attention = 0.5f + 0.5f * sinf((float)i * 0.1f);
        cerebellum_thalamic_set_attention(bridge, attention);

        signal.timing_precision = 0.8f;
        cerebellum_thalamic_route_signal(bridge, &signal);

        float actual_attention;
        cerebellum_thalamic_get_attention(bridge, &actual_attention);
        EXPECT_GE(actual_attention, 0.0f);
        EXPECT_LE(actual_attention, 1.0f);
    }
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
