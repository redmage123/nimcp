/**
 * @file test_neuromod_attention_bridge.cpp
 * @brief Unit tests for Neuromodulatory-Attention Inter-Layer Bridge
 *
 * WHAT: Test suite for neuromod_attention_bridge
 * WHY:  Verify correct LC→attention gain, VTA→salience, 5-HT→patience modulation
 * HOW:  Unit tests for lifecycle, bottom-up modulation, top-down feedback, state
 *
 * @author NIMCP Development Team
 * @date 2026-01-12
 */

#include <gtest/gtest.h>
#include <cmath>

extern "C" {
#include "integration/inter/neuromod_attention/nimcp_neuromod_attention_bridge.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class NeuromodAttentionBridgeTest : public ::testing::Test {
protected:
    neuromod_attention_bridge_t* bridge = nullptr;

    void SetUp() override {
        neuromod_attention_config_t config = neuromod_attention_default_config();
        bridge = neuromod_attention_create(&config);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            neuromod_attention_destroy(bridge);
            bridge = nullptr;
        }
    }
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST(NeuromodAttentionCreateTest, CreateWithDefaultConfig) {
    neuromod_attention_bridge_t* br = neuromod_attention_create(nullptr);
    ASSERT_NE(br, nullptr);
    EXPECT_TRUE(neuromod_attention_is_connected(br));
    neuromod_attention_destroy(br);
}

TEST(NeuromodAttentionCreateTest, CreateWithCustomConfig) {
    neuromod_attention_config_t config = neuromod_attention_default_config();
    config.ne_gain_coupling = 0.9f;
    config.da_salience_coupling = 0.8f;
    config.ht_patience_coupling = 0.7f;
    config.enable_adaptive_gain = false;

    neuromod_attention_bridge_t* br = neuromod_attention_create(&config);
    ASSERT_NE(br, nullptr);
    neuromod_attention_destroy(br);
}

TEST(NeuromodAttentionCreateTest, DestroyNull) {
    neuromod_attention_destroy(nullptr);  /* Should not crash */
}

//=============================================================================
// NE-Attention Gain Tests
//=============================================================================

TEST_F(NeuromodAttentionBridgeTest, ApplyNEGainLow) {
    float gain_out;
    int ret = neuromod_attention_apply_ne_gain(bridge, 0.2f, &gain_out);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(gain_out, 0.0f);
    EXPECT_LE(gain_out, NE_ATTENTION_GAIN_MAX);
}

TEST_F(NeuromodAttentionBridgeTest, ApplyNEGainOptimal) {
    float gain_out;
    /* Optimal NE ~0.6 should give maximum gain with adaptive gain */
    int ret = neuromod_attention_apply_ne_gain(bridge, 0.6f, &gain_out);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(gain_out, NE_ATTENTION_GAIN_BASELINE);
}

TEST_F(NeuromodAttentionBridgeTest, ApplyNEGainHigh) {
    float gain_out;
    /* High NE should reduce gain (inverted-U) */
    int ret = neuromod_attention_apply_ne_gain(bridge, 1.0f, &gain_out);
    EXPECT_EQ(ret, 0);
    EXPECT_GE(gain_out, NE_ATTENTION_GAIN_BASELINE);
}

TEST_F(NeuromodAttentionBridgeTest, ApplyNEGainClamping) {
    float gain_out;
    /* Test clamping of out-of-range values */
    int ret = neuromod_attention_apply_ne_gain(bridge, -0.5f, &gain_out);
    EXPECT_EQ(ret, 0);
    EXPECT_GE(gain_out, 0.0f);

    ret = neuromod_attention_apply_ne_gain(bridge, 1.5f, &gain_out);
    EXPECT_EQ(ret, 0);
    EXPECT_LE(gain_out, NE_ATTENTION_GAIN_MAX);
}

TEST_F(NeuromodAttentionBridgeTest, ApplyNEGainNull) {
    int ret = neuromod_attention_apply_ne_gain(nullptr, 0.5f, nullptr);
    EXPECT_EQ(ret, -1);
}

//=============================================================================
// Phasic Shift Tests
//=============================================================================

TEST_F(NeuromodAttentionBridgeTest, PhasicShiftBelowThreshold) {
    bool triggered;
    int ret = neuromod_attention_apply_phasic_shift(bridge, 0.5f, &triggered);
    EXPECT_EQ(ret, 0);
    EXPECT_FALSE(triggered);
}

TEST_F(NeuromodAttentionBridgeTest, PhasicShiftAboveThreshold) {
    bool triggered;
    int ret = neuromod_attention_apply_phasic_shift(bridge, 0.8f, &triggered);
    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(triggered);
}

//=============================================================================
// DA-Salience Tests
//=============================================================================

TEST_F(NeuromodAttentionBridgeTest, ApplyDASalience) {
    float salience_out;
    int ret = neuromod_attention_apply_da_salience(bridge, 0.7f, &salience_out);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(salience_out, 0.0f);
    EXPECT_LE(salience_out, 1.0f);
}

TEST_F(NeuromodAttentionBridgeTest, ApplyDASalienceDisabled) {
    neuromod_attention_config_t config = neuromod_attention_default_config();
    config.enable_salience_filtering = false;
    neuromod_attention_bridge_t* br = neuromod_attention_create(&config);
    ASSERT_NE(br, nullptr);

    float salience_out;
    int ret = neuromod_attention_apply_da_salience(br, 0.8f, &salience_out);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(salience_out, 0.0f);

    neuromod_attention_destroy(br);
}

//=============================================================================
// 5-HT Patience Tests
//=============================================================================

TEST_F(NeuromodAttentionBridgeTest, ApplyHTPatience) {
    float patience_out;
    int ret = neuromod_attention_apply_ht_patience(bridge, 0.6f, &patience_out);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(patience_out, 0.0f);
    EXPECT_LE(patience_out, 1.0f);
}

//=============================================================================
// Habenula Aversion Tests
//=============================================================================

TEST_F(NeuromodAttentionBridgeTest, ApplyHabAversionBelowThreshold) {
    float withdrawal_out;
    int ret = neuromod_attention_apply_hab_aversion(bridge, 0.3f, &withdrawal_out);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(withdrawal_out, 0.0f);
}

TEST_F(NeuromodAttentionBridgeTest, ApplyHabAversionAboveThreshold) {
    float withdrawal_out;
    int ret = neuromod_attention_apply_hab_aversion(bridge, 0.8f, &withdrawal_out);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(withdrawal_out, 0.0f);
}

//=============================================================================
// Top-Down Feedback Tests
//=============================================================================

TEST_F(NeuromodAttentionBridgeTest, ReportNovelty) {
    float lc_trigger;
    int ret = neuromod_attention_report_novelty(bridge, 0.7f, &lc_trigger);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(lc_trigger, 0.0f);
}

TEST_F(NeuromodAttentionBridgeTest, ReportRewardFeature) {
    float vta_trigger;
    int ret = neuromod_attention_report_reward_feature(bridge, 0.6f, &vta_trigger);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(vta_trigger, 0.0f);
}

TEST_F(NeuromodAttentionBridgeTest, ReportConflict) {
    float arousal_demand;
    int ret = neuromod_attention_report_conflict(bridge, 0.5f, &arousal_demand);
    EXPECT_EQ(ret, 0);
    EXPECT_GE(arousal_demand, 0.0f);
}

//=============================================================================
// Unified Modulation Tests
//=============================================================================

TEST_F(NeuromodAttentionBridgeTest, ComputeModulation) {
    neuromod_attention_state_t state;
    int ret = neuromod_attention_compute_modulation(bridge, 0.5f, 0.5f, 0.5f, 0.3f, &state);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(state.attention_gain, 0.0f);
    EXPECT_GE(state.bridge_coherence, 0.0f);
    EXPECT_LE(state.bridge_coherence, 1.0f);
}

TEST_F(NeuromodAttentionBridgeTest, ComputeModulationCoherenceReduction) {
    /* High vigilance + high aversion should reduce coherence */
    neuromod_attention_state_t state;
    int ret = neuromod_attention_compute_modulation(bridge, 0.9f, 0.3f, 0.3f, 0.9f, &state);
    EXPECT_EQ(ret, 0);
    EXPECT_LT(state.bridge_coherence, 1.0f);
}

//=============================================================================
// Update and State Tests
//=============================================================================

TEST_F(NeuromodAttentionBridgeTest, Update) {
    int ret = neuromod_attention_update(bridge, 10.0f);
    EXPECT_EQ(ret, 0);
}

TEST_F(NeuromodAttentionBridgeTest, GetState) {
    neuromod_attention_state_t state;
    int ret = neuromod_attention_get_state(bridge, &state);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(state.attention_gain, 0.0f);
}

TEST_F(NeuromodAttentionBridgeTest, GetStats) {
    /* Perform some operations */
    neuromod_attention_apply_ne_gain(bridge, 0.5f, nullptr);
    neuromod_attention_report_novelty(bridge, 0.8f, nullptr);

    neuromod_attention_stats_t stats;
    int ret = neuromod_attention_get_stats(bridge, &stats);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(stats.gain_modulations, 0u);
}

TEST_F(NeuromodAttentionBridgeTest, ResetStats) {
    neuromod_attention_apply_ne_gain(bridge, 0.5f, nullptr);

    int ret = neuromod_attention_reset_stats(bridge);
    EXPECT_EQ(ret, 0);

    neuromod_attention_stats_t stats;
    neuromod_attention_get_stats(bridge, &stats);
    EXPECT_EQ(stats.gain_modulations, 0u);
}

//=============================================================================
// Diagnostics Tests
//=============================================================================

TEST_F(NeuromodAttentionBridgeTest, IsConnected) {
    EXPECT_TRUE(neuromod_attention_is_connected(bridge));
    EXPECT_FALSE(neuromod_attention_is_connected(nullptr));
}

TEST_F(NeuromodAttentionBridgeTest, GetCoherence) {
    float coherence = neuromod_attention_get_coherence(bridge);
    EXPECT_GE(coherence, 0.0f);
    EXPECT_LE(coherence, 1.0f);
}

TEST_F(NeuromodAttentionBridgeTest, PrintSummary) {
    /* Should not crash */
    neuromod_attention_print_summary(bridge);
    neuromod_attention_print_summary(nullptr);
}
