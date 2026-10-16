/**
 * @file test_neuromod_wm_bridge.cpp
 * @brief Unit tests for Neuromodulatory-Working Memory Inter-Layer Bridge
 *
 * WHAT: Test suite for neuromod_wm_bridge
 * WHY:  Verify correct DA→WM gain (inverted-U), NE→flexibility, 5-HT→delay
 * HOW:  Unit tests for lifecycle, bottom-up modulation, top-down feedback, state
 *
 * @author NIMCP Development Team
 * @date 2026-01-12
 */

#include <gtest/gtest.h>
#include <cmath>

extern "C" {
#include "integration/inter/neuromod_wm/nimcp_neuromod_wm_bridge.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class NeuromodWMBridgeTest : public ::testing::Test {
protected:
    neuromod_wm_bridge_t* bridge = nullptr;

    void SetUp() override {
        neuromod_wm_config_t config = neuromod_wm_default_config();
        bridge = neuromod_wm_create(&config);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            neuromod_wm_destroy(bridge);
            bridge = nullptr;
        }
    }
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST(NeuromodWMCreateTest, CreateWithDefaultConfig) {
    neuromod_wm_bridge_t* br = neuromod_wm_create(nullptr);
    ASSERT_NE(br, nullptr);
    EXPECT_TRUE(neuromod_wm_is_connected(br));
    neuromod_wm_destroy(br);
}

TEST(NeuromodWMCreateTest, CreateWithCustomConfig) {
    neuromod_wm_config_t config = neuromod_wm_default_config();
    config.da_wm_gain_coupling = 0.9f;
    config.ne_flexibility_coupling = 0.8f;
    config.ht_delay_coupling = 0.7f;
    config.enable_inverted_u = false;

    neuromod_wm_bridge_t* br = neuromod_wm_create(&config);
    ASSERT_NE(br, nullptr);
    neuromod_wm_destroy(br);
}

TEST(NeuromodWMCreateTest, DestroyNull) {
    neuromod_wm_destroy(nullptr);  /* Should not crash */
}

//=============================================================================
// DA-WM Gain Tests (Inverted-U)
//=============================================================================

TEST_F(NeuromodWMBridgeTest, ApplyDAGainLow) {
    float gain_out;
    int ret = neuromod_wm_apply_da_gain(bridge, 0.1f, &gain_out);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(gain_out, 0.0f);
    EXPECT_LT(gain_out, DA_WM_GAIN_MAX);
}

TEST_F(NeuromodWMBridgeTest, ApplyDAGainOptimal) {
    float gain_out;
    /* Optimal DA ~0.5 should give near-maximum gain */
    int ret = neuromod_wm_apply_da_gain(bridge, DA_OPTIMAL_LEVEL, &gain_out);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(gain_out, DA_WM_GAIN_BASELINE);
}

TEST_F(NeuromodWMBridgeTest, ApplyDAGainHigh) {
    float gain_out;
    /* High DA should reduce gain from optimal (inverted-U) */
    int ret = neuromod_wm_apply_da_gain(bridge, 1.0f, &gain_out);
    EXPECT_EQ(ret, 0);
    EXPECT_GE(gain_out, DA_WM_GAIN_BASELINE);
}

TEST_F(NeuromodWMBridgeTest, InvertedUCurve) {
    float gain_low, gain_optimal, gain_high;

    neuromod_wm_apply_da_gain(bridge, 0.2f, &gain_low);
    neuromod_wm_apply_da_gain(bridge, DA_OPTIMAL_LEVEL, &gain_optimal);
    neuromod_wm_apply_da_gain(bridge, 0.9f, &gain_high);

    /* Optimal should be higher than extremes */
    EXPECT_GT(gain_optimal, gain_low);
    EXPECT_GT(gain_optimal, gain_high);
}

//=============================================================================
// D1/D2 Balance Tests
//=============================================================================

TEST_F(NeuromodWMBridgeTest, ApplyD1Stability) {
    float stability_out;
    int ret = neuromod_wm_apply_d1_stability(bridge, 0.7f, &stability_out);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(stability_out, 0.0f);
}

TEST_F(NeuromodWMBridgeTest, ApplyD2Flexibility) {
    float flexibility_out;
    int ret = neuromod_wm_apply_d2_flexibility(bridge, 0.6f, &flexibility_out);
    EXPECT_EQ(ret, 0);
    EXPECT_GE(flexibility_out, 0.0f);
}

//=============================================================================
// NE-Flexibility Tests
//=============================================================================

TEST_F(NeuromodWMBridgeTest, ApplyNEFlexibility) {
    float flexibility_out;
    int ret = neuromod_wm_apply_ne_flexibility(bridge, 0.7f, &flexibility_out);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(flexibility_out, 0.0f);
    EXPECT_LE(flexibility_out, 1.0f);
}

TEST_F(NeuromodWMBridgeTest, ApplyNEResetBelowThreshold) {
    bool triggered;
    int ret = neuromod_wm_apply_ne_reset(bridge, 0.5f, &triggered);
    EXPECT_EQ(ret, 0);
    EXPECT_FALSE(triggered);
}

TEST_F(NeuromodWMBridgeTest, ApplyNEResetAboveThreshold) {
    bool triggered;
    int ret = neuromod_wm_apply_ne_reset(bridge, 0.9f, &triggered);
    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(triggered);
}

//=============================================================================
// 5-HT Delay Tolerance Tests
//=============================================================================

TEST_F(NeuromodWMBridgeTest, ApplyHTDelay) {
    float delay_out;
    int ret = neuromod_wm_apply_ht_delay(bridge, 0.6f, &delay_out);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(delay_out, 0.0f);
    EXPECT_LE(delay_out, 1.0f);
}

//=============================================================================
// Top-Down Feedback Tests
//=============================================================================

TEST_F(NeuromodWMBridgeTest, ReportLoad) {
    float da_demand;
    int ret = neuromod_wm_report_load(bridge, 0.7f, &da_demand);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(da_demand, 0.0f);
}

TEST_F(NeuromodWMBridgeTest, ReportSwitchNeed) {
    float lc_trigger;
    int ret = neuromod_wm_report_switch_need(bridge, 0.8f, &lc_trigger);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(lc_trigger, 0.0f);
}

TEST_F(NeuromodWMBridgeTest, ReportOverflow) {
    float stress;
    int ret = neuromod_wm_report_overflow(bridge, 0.9f, &stress);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(stress, 0.0f);
}

//=============================================================================
// Unified Modulation Tests
//=============================================================================

TEST_F(NeuromodWMBridgeTest, ComputeModulation) {
    neuromod_wm_state_t state;
    int ret = neuromod_wm_compute_modulation(bridge, 0.5f, 0.5f, 0.5f, &state);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(state.wm_gain, 0.0f);
    EXPECT_GE(state.bridge_coherence, 0.0f);
    EXPECT_LE(state.bridge_coherence, 1.0f);
}

TEST_F(NeuromodWMBridgeTest, ComputeModulationCoherenceReduction) {
    /* High load + low DA should reduce coherence */
    neuromod_wm_report_load(bridge, 0.8f, nullptr);

    neuromod_wm_state_t state;
    int ret = neuromod_wm_compute_modulation(bridge, 0.2f, 0.5f, 0.5f, &state);
    EXPECT_EQ(ret, 0);
    EXPECT_LT(state.bridge_coherence, 1.0f);
}

//=============================================================================
// Update and State Tests
//=============================================================================

TEST_F(NeuromodWMBridgeTest, Update) {
    int ret = neuromod_wm_update(bridge, 10.0f);
    EXPECT_EQ(ret, 0);
}

TEST_F(NeuromodWMBridgeTest, GetState) {
    neuromod_wm_state_t state;
    int ret = neuromod_wm_get_state(bridge, &state);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(state.wm_gain, 0.0f);
}

TEST_F(NeuromodWMBridgeTest, GetStats) {
    neuromod_wm_apply_da_gain(bridge, 0.5f, nullptr);
    neuromod_wm_report_load(bridge, 0.6f, nullptr);

    neuromod_wm_stats_t stats;
    int ret = neuromod_wm_get_stats(bridge, &stats);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(stats.gain_modulations, 0u);
}

TEST_F(NeuromodWMBridgeTest, ResetStats) {
    neuromod_wm_apply_da_gain(bridge, 0.5f, nullptr);

    int ret = neuromod_wm_reset_stats(bridge);
    EXPECT_EQ(ret, 0);

    neuromod_wm_stats_t stats;
    neuromod_wm_get_stats(bridge, &stats);
    EXPECT_EQ(stats.gain_modulations, 0u);
}

//=============================================================================
// Diagnostics Tests
//=============================================================================

TEST_F(NeuromodWMBridgeTest, IsConnected) {
    EXPECT_TRUE(neuromod_wm_is_connected(bridge));
    EXPECT_FALSE(neuromod_wm_is_connected(nullptr));
}

TEST_F(NeuromodWMBridgeTest, GetCoherence) {
    float coherence = neuromod_wm_get_coherence(bridge);
    EXPECT_GE(coherence, 0.0f);
    EXPECT_LE(coherence, 1.0f);
}

TEST_F(NeuromodWMBridgeTest, PrintSummary) {
    neuromod_wm_print_summary(bridge);
    neuromod_wm_print_summary(nullptr);
}
