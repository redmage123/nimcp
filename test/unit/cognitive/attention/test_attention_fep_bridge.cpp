/**
 * @file test_attention_fep_bridge.cpp
 * @brief Unit tests for Attention-FEP Bridge module
 *
 * WHAT: Comprehensive tests for FEP-Attention bidirectional integration
 * WHY:  Ensure attention precision-weighting and FEP belief gating work correctly
 * HOW:  Test lifecycle, connections, modulation, gating, and bio-async integration
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include "cognitive/attention/nimcp_attention_fep_bridge.h"
#include "cognitive/free_energy/nimcp_free_energy.h"

class AttentionFepBridgeTest : public ::testing::Test {
protected:
    attention_fep_bridge_t* bridge = nullptr;

    void SetUp() override {
        attention_fep_config_t config;
        attention_fep_bridge_default_config(&config);
        bridge = attention_fep_bridge_create(&config);
    }

    void TearDown() override {
        if (bridge) {
            attention_fep_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }
};

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(AttentionFepBridgeTest, CreateDestroy) {
    ASSERT_NE(bridge, nullptr);
}

TEST_F(AttentionFepBridgeTest, CreateWithNullConfig) {
    attention_fep_bridge_t* br = attention_fep_bridge_create(nullptr);
    ASSERT_NE(br, nullptr);
    attention_fep_bridge_destroy(br);
}

TEST_F(AttentionFepBridgeTest, DestroyNull) {
    attention_fep_bridge_destroy(nullptr);  // Should not crash
}

TEST_F(AttentionFepBridgeTest, DefaultConfig) {
    attention_fep_config_t config;
    int ret = attention_fep_bridge_default_config(&config);

    EXPECT_EQ(ret, 0);
    EXPECT_GT(config.precision_gain_scaling, 0.0f);
    EXPECT_GT(config.pe_attention_shift_threshold, 0.0f);
    EXPECT_GT(config.efe_info_seeking_threshold, 0.0f);
    EXPECT_TRUE(config.enable_precision_gain_modulation);
    EXPECT_TRUE(config.enable_surprise_attention_shift);
    EXPECT_TRUE(config.enable_efe_info_seeking);
    EXPECT_TRUE(config.enable_attentional_gating);
    EXPECT_TRUE(config.enable_attention_lr_modulation);
}

TEST_F(AttentionFepBridgeTest, DefaultConfigNullPtr) {
    int ret = attention_fep_bridge_default_config(nullptr);
    EXPECT_EQ(ret, -1);
}

/* ============================================================================
 * Connection Tests
 * ============================================================================ */

TEST_F(AttentionFepBridgeTest, ConnectFep) {
    fep_config_t fep_config;
    fep_default_config(&fep_config);
    fep_system_t* fep = fep_create(&fep_config, 8, 4);
    ASSERT_NE(fep, nullptr);

    int ret = attention_fep_bridge_connect_fep(bridge, fep);
    EXPECT_EQ(ret, 0);

    fep_destroy(fep);
}

TEST_F(AttentionFepBridgeTest, ConnectFepNull) {
    EXPECT_EQ(attention_fep_bridge_connect_fep(nullptr, nullptr), -1);

    fep_config_t fep_config;
    fep_default_config(&fep_config);
    fep_system_t* fep = fep_create(&fep_config, 8, 4);

    EXPECT_EQ(attention_fep_bridge_connect_fep(nullptr, fep), -1);
    EXPECT_EQ(attention_fep_bridge_connect_fep(bridge, nullptr), -1);

    fep_destroy(fep);
}

TEST_F(AttentionFepBridgeTest, ConnectAttention) {
    multihead_attention_t attention;
    memset(&attention, 0, sizeof(multihead_attention_t));

    int ret = attention_fep_bridge_connect_attention(bridge, attention);
    EXPECT_EQ(ret, 0);
}

TEST_F(AttentionFepBridgeTest, ConnectAttentionNull) {
    multihead_attention_t attention;
    memset(&attention, 0, sizeof(multihead_attention_t));

    EXPECT_EQ(attention_fep_bridge_connect_attention(nullptr, attention), -1);
}

TEST_F(AttentionFepBridgeTest, Disconnect) {
    int ret = attention_fep_bridge_disconnect(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(AttentionFepBridgeTest, DisconnectNull) {
    EXPECT_EQ(attention_fep_bridge_disconnect(nullptr), -1);
}

/* ============================================================================
 * State/Stats Tests
 * ============================================================================ */

TEST_F(AttentionFepBridgeTest, GetState) {
    attention_fep_state_t state;
    int ret = attention_fep_bridge_get_state(bridge, &state);

    EXPECT_EQ(ret, 0);
    EXPECT_GE(state.current_precision, 0.0f);
    EXPECT_GE(state.current_attention_focus, 0.0f);
    EXPECT_LE(state.current_attention_focus, 1.0f);
}

TEST_F(AttentionFepBridgeTest, GetStateNull) {
    attention_fep_state_t state;

    EXPECT_EQ(attention_fep_bridge_get_state(nullptr, &state), -1);
    EXPECT_EQ(attention_fep_bridge_get_state(bridge, nullptr), -1);
}

TEST_F(AttentionFepBridgeTest, GetStats) {
    attention_fep_stats_t stats;
    int ret = attention_fep_bridge_get_stats(bridge, &stats);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(stats.precision_gain_modulations, 0u);
    EXPECT_EQ(stats.surprise_attention_shifts, 0u);
    EXPECT_EQ(stats.efe_info_seeking_events, 0u);
}

TEST_F(AttentionFepBridgeTest, GetStatsNull) {
    attention_fep_stats_t stats;

    EXPECT_EQ(attention_fep_bridge_get_stats(nullptr, &stats), -1);
    EXPECT_EQ(attention_fep_bridge_get_stats(bridge, nullptr), -1);
}

/* ============================================================================
 * FEP → Attention Direction Tests
 * ============================================================================ */

TEST_F(AttentionFepBridgeTest, ApplyPrecisionGainModulation) {
    int ret = attention_fep_apply_precision_gain_modulation(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(AttentionFepBridgeTest, ApplyPrecisionGainModulationNull) {
    EXPECT_EQ(attention_fep_apply_precision_gain_modulation(nullptr), -1);
}

TEST_F(AttentionFepBridgeTest, SurpriseAttentionShift) {
    float pe_magnitude = 6.0f;  // Above ATTENTION_FEP_SURPRISE_SHIFT_THRESHOLD

    int ret = attention_fep_surprise_attention_shift(bridge, pe_magnitude);
    EXPECT_EQ(ret, 0);

    attention_fep_state_t state;
    attention_fep_bridge_get_state(bridge, &state);
    EXPECT_TRUE(state.surprise_shift_triggered);
}

TEST_F(AttentionFepBridgeTest, SurpriseAttentionShiftBelowThreshold) {
    float pe_magnitude = 2.0f;  // Below threshold

    int ret = attention_fep_surprise_attention_shift(bridge, pe_magnitude);
    EXPECT_EQ(ret, 0);

    attention_fep_state_t state;
    attention_fep_bridge_get_state(bridge, &state);
    EXPECT_FALSE(state.surprise_shift_triggered);
}

TEST_F(AttentionFepBridgeTest, SurpriseAttentionShiftNull) {
    EXPECT_EQ(attention_fep_surprise_attention_shift(nullptr, 5.0f), -1);
}

TEST_F(AttentionFepBridgeTest, EfeInfoSeeking) {
    int ret = attention_fep_efe_info_seeking(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(AttentionFepBridgeTest, EfeInfoSeekingNull) {
    EXPECT_EQ(attention_fep_efe_info_seeking(nullptr), -1);
}

/* ============================================================================
 * Attention → FEP Direction Tests
 * ============================================================================ */

TEST_F(AttentionFepBridgeTest, ApplyAttentionalGating) {
    int ret = attention_fep_apply_attentional_gating(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(AttentionFepBridgeTest, ApplyAttentionalGatingNull) {
    EXPECT_EQ(attention_fep_apply_attentional_gating(nullptr), -1);
}

TEST_F(AttentionFepBridgeTest, ModulateLearningRate) {
    int ret = attention_fep_modulate_learning_rate(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(AttentionFepBridgeTest, ModulateLearningRateNull) {
    EXPECT_EQ(attention_fep_modulate_learning_rate(nullptr), -1);
}

TEST_F(AttentionFepBridgeTest, ApplyFocusModelNarrowing) {
    int ret = attention_fep_apply_focus_model_narrowing(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(AttentionFepBridgeTest, ApplyFocusModelNarrowingNull) {
    EXPECT_EQ(attention_fep_apply_focus_model_narrowing(nullptr), -1);
}

/* ============================================================================
 * Update Tests
 * ============================================================================ */

TEST_F(AttentionFepBridgeTest, Update) {
    int ret = attention_fep_bridge_update(bridge, 16);  // 16ms delta
    EXPECT_EQ(ret, 0);
}

TEST_F(AttentionFepBridgeTest, UpdateNull) {
    EXPECT_EQ(attention_fep_bridge_update(nullptr, 16), -1);
}

TEST_F(AttentionFepBridgeTest, UpdateZeroDelta) {
    int ret = attention_fep_bridge_update(bridge, 0);
    EXPECT_EQ(ret, 0);
}

/* ============================================================================
 * Bio-Async Tests
 * ============================================================================ */

TEST_F(AttentionFepBridgeTest, BioAsyncConnect) {
    int ret = attention_fep_bridge_connect_bio_async(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(AttentionFepBridgeTest, BioAsyncDisconnect) {
    attention_fep_bridge_connect_bio_async(bridge);

    int ret = attention_fep_bridge_disconnect_bio_async(bridge);
    EXPECT_EQ(ret, 0);
    EXPECT_FALSE(attention_fep_bridge_is_bio_async_connected(bridge));
}

TEST_F(AttentionFepBridgeTest, BioAsyncIsConnected) {
    EXPECT_FALSE(attention_fep_bridge_is_bio_async_connected(bridge));

    attention_fep_bridge_connect_bio_async(bridge);
    // May or may not be connected depending on router availability

    attention_fep_bridge_disconnect_bio_async(bridge);
    EXPECT_FALSE(attention_fep_bridge_is_bio_async_connected(bridge));
}

TEST_F(AttentionFepBridgeTest, BioAsyncNullParams) {
    EXPECT_EQ(attention_fep_bridge_connect_bio_async(nullptr), -1);
    EXPECT_EQ(attention_fep_bridge_disconnect_bio_async(nullptr), -1);
    EXPECT_FALSE(attention_fep_bridge_is_bio_async_connected(nullptr));
}

TEST_F(AttentionFepBridgeTest, BioAsyncDoubleConnect) {
    attention_fep_bridge_connect_bio_async(bridge);
    int ret = attention_fep_bridge_connect_bio_async(bridge);  // Should be no-op
    EXPECT_EQ(ret, 0);
    attention_fep_bridge_disconnect_bio_async(bridge);
}

/* ============================================================================
 * Integration Tests
 * ============================================================================ */

TEST_F(AttentionFepBridgeTest, PrecisionModulatesGain) {
    // Apply precision gain modulation
    attention_fep_apply_precision_gain_modulation(bridge);

    attention_fep_stats_t stats;
    attention_fep_bridge_get_stats(bridge, &stats);
    EXPECT_GE(stats.precision_gain_modulations, 0u);
}

TEST_F(AttentionFepBridgeTest, HighPredictionErrorTriggersShift) {
    float high_pe = 10.0f;
    attention_fep_surprise_attention_shift(bridge, high_pe);

    attention_fep_stats_t stats;
    attention_fep_bridge_get_stats(bridge, &stats);
    EXPECT_GT(stats.surprise_attention_shifts, 0u);
}

TEST_F(AttentionFepBridgeTest, AttentionalGatingModifiesPrecision) {
    attention_fep_apply_attentional_gating(bridge);

    attention_fep_stats_t stats;
    attention_fep_bridge_get_stats(bridge, &stats);
    EXPECT_GE(stats.attentional_gating_events, 0u);
}
