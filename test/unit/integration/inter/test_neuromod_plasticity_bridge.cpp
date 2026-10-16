/**
 * @file test_neuromod_plasticity_bridge.cpp
 * @brief Unit tests for Neuromodulatory-Plasticity Inter-Layer Bridge
 *
 * WHAT: Test suite for neuromod_plasticity_bridge
 * WHY:  Verify correct DA LTP gating, NE memory boost, 5-HT consolidation, eligibility traces
 * HOW:  Unit tests for lifecycle, modulation, eligibility, reward PE, feedback
 *
 * @author NIMCP Development Team
 * @date 2026-01-12
 */

#include <gtest/gtest.h>
#include <cmath>

extern "C" {
#include "integration/inter/neuromod_plasticity/nimcp_neuromod_plasticity_bridge.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class NeuromodPlasticityBridgeTest : public ::testing::Test {
protected:
    neuromod_plasticity_bridge_t* bridge = nullptr;

    void SetUp() override {
        neuromod_plasticity_config_t config = neuromod_plasticity_default_config();
        bridge = neuromod_plasticity_create(&config);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            neuromod_plasticity_destroy(bridge);
            bridge = nullptr;
        }
    }
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST(NeuromodPlasticityCreateTest, CreateWithDefaultConfig) {
    neuromod_plasticity_bridge_t* br = neuromod_plasticity_create(nullptr);
    ASSERT_NE(br, nullptr);
    EXPECT_TRUE(neuromod_plasticity_is_connected(br));
    neuromod_plasticity_destroy(br);
}

TEST(NeuromodPlasticityCreateTest, CreateWithCustomConfig) {
    neuromod_plasticity_config_t config = neuromod_plasticity_default_config();
    config.da_ltp_gate_strength = 0.9f;
    config.ne_memory_boost_coupling = 0.8f;
    config.ht_consolidation_coupling = 0.7f;
    config.enable_eligibility_traces = true;

    neuromod_plasticity_bridge_t* br = neuromod_plasticity_create(&config);
    ASSERT_NE(br, nullptr);
    neuromod_plasticity_destroy(br);
}

TEST(NeuromodPlasticityCreateTest, DestroyNull) {
    neuromod_plasticity_destroy(nullptr);
}

//=============================================================================
// DA LTP Gating Tests
//=============================================================================

TEST_F(NeuromodPlasticityBridgeTest, ApplyDAGatingBelowThreshold) {
    float ltp_gate;
    int ret = neuromod_plasticity_apply_da_gating(bridge, 0.1f, &ltp_gate);
    EXPECT_EQ(ret, 0);
    /* Below threshold, gate should be low */
    EXPECT_LT(ltp_gate, 0.3f);
}

TEST_F(NeuromodPlasticityBridgeTest, ApplyDAGatingAboveThreshold) {
    float ltp_gate;
    int ret = neuromod_plasticity_apply_da_gating(bridge, 0.8f, &ltp_gate);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(ltp_gate, 0.0f);
}

TEST_F(NeuromodPlasticityBridgeTest, DAGatingUpdatesMode) {
    neuromod_plasticity_apply_da_gating(bridge, 0.1f, nullptr);
    plasticity_mode_t mode = neuromod_plasticity_get_mode(bridge);
    EXPECT_EQ(mode, PLASTICITY_MODE_GATED);
}

//=============================================================================
// Reward Prediction Error Tests
//=============================================================================

TEST_F(NeuromodPlasticityBridgeTest, ApplyPositiveRPE) {
    float rpe;
    /* Reward higher than expected = positive RPE */
    int ret = neuromod_plasticity_apply_reward_pe(bridge, 0.8f, 0.3f, &rpe);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(rpe, 0.0f);
}

TEST_F(NeuromodPlasticityBridgeTest, ApplyNegativeRPE) {
    float rpe;
    /* Reward lower than expected = negative RPE */
    int ret = neuromod_plasticity_apply_reward_pe(bridge, 0.2f, 0.7f, &rpe);
    EXPECT_EQ(ret, 0);
    EXPECT_LT(rpe, 0.0f);
}

TEST_F(NeuromodPlasticityBridgeTest, ApplyZeroRPE) {
    float rpe;
    /* Reward equals expected = zero RPE */
    int ret = neuromod_plasticity_apply_reward_pe(bridge, 0.5f, 0.5f, &rpe);
    EXPECT_EQ(ret, 0);
    EXPECT_NEAR(rpe, 0.0f, 0.01f);
}

//=============================================================================
// NE Memory Boost Tests
//=============================================================================

TEST_F(NeuromodPlasticityBridgeTest, ApplyNEBoostLow) {
    float boost;
    int ret = neuromod_plasticity_apply_ne_boost(bridge, 0.3f, &boost);
    EXPECT_EQ(ret, 0);
    EXPECT_GE(boost, 1.0f);  /* Boost should be at least 1.0 */
}

TEST_F(NeuromodPlasticityBridgeTest, ApplyNEBoostHigh) {
    float boost;
    /* High NE should give significant memory boost */
    int ret = neuromod_plasticity_apply_ne_boost(bridge, 0.9f, &boost);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(boost, 1.0f);
}

TEST_F(NeuromodPlasticityBridgeTest, NEBoostUpdatesModeTooBoosted) {
    neuromod_plasticity_apply_ne_boost(bridge, 0.9f, nullptr);
    plasticity_mode_t mode = neuromod_plasticity_get_mode(bridge);
    EXPECT_EQ(mode, PLASTICITY_MODE_BOOSTED);
}

//=============================================================================
// 5-HT Consolidation Tests
//=============================================================================

TEST_F(NeuromodPlasticityBridgeTest, ApplyHTConsolidation) {
    float rate;
    int ret = neuromod_plasticity_apply_ht_consolidation(bridge, 0.6f, &rate);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(rate, 0.0f);
    EXPECT_LE(rate, 1.0f);
}

TEST_F(NeuromodPlasticityBridgeTest, HTConsolidationHighWithLowNE) {
    /* First set low NE */
    neuromod_plasticity_apply_ne_boost(bridge, 0.2f, nullptr);
    /* Then high 5-HT should trigger consolidation mode */
    neuromod_plasticity_apply_ht_consolidation(bridge, 0.9f, nullptr);
    plasticity_mode_t mode = neuromod_plasticity_get_mode(bridge);
    EXPECT_EQ(mode, PLASTICITY_MODE_CONSOLIDATING);
}

//=============================================================================
// Habenula Avoidance Tests
//=============================================================================

TEST_F(NeuromodPlasticityBridgeTest, ApplyHabAvoidanceLow) {
    float avoidance;
    int ret = neuromod_plasticity_apply_hab_avoidance(bridge, 0.3f, &avoidance);
    EXPECT_EQ(ret, 0);
    EXPECT_LT(avoidance, 0.3f);
}

TEST_F(NeuromodPlasticityBridgeTest, ApplyHabAvoidanceHigh) {
    float avoidance;
    int ret = neuromod_plasticity_apply_hab_avoidance(bridge, 0.9f, &avoidance);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(avoidance, 0.3f);
}

TEST_F(NeuromodPlasticityBridgeTest, HighHabSuppressesLearning) {
    neuromod_plasticity_apply_hab_avoidance(bridge, 0.9f, nullptr);
    plasticity_mode_t mode = neuromod_plasticity_get_mode(bridge);
    EXPECT_EQ(mode, PLASTICITY_MODE_SUPPRESSED);
}

//=============================================================================
// Eligibility Trace Tests
//=============================================================================

TEST_F(NeuromodPlasticityBridgeTest, SetEligibility) {
    int ret = neuromod_plasticity_set_eligibility(bridge, 0.8f);
    EXPECT_EQ(ret, 0);

    neuromod_plasticity_state_t state;
    neuromod_plasticity_get_state(bridge, &state);
    EXPECT_GT(state.eligibility_level, 0.5f);
}

TEST_F(NeuromodPlasticityBridgeTest, CaptureEligibility) {
    /* Set eligibility first */
    neuromod_plasticity_set_eligibility(bridge, 0.8f);

    /* Capture with DA signal */
    int ret = neuromod_plasticity_capture_eligibility(bridge, 0.7f);
    EXPECT_EQ(ret, 0);
}

TEST_F(NeuromodPlasticityBridgeTest, DecayEligibility) {
    neuromod_plasticity_set_eligibility(bridge, 0.8f);

    neuromod_plasticity_state_t state_before;
    neuromod_plasticity_get_state(bridge, &state_before);

    /* Decay over time */
    neuromod_plasticity_decay_eligibility(bridge, 100.0f);

    neuromod_plasticity_state_t state_after;
    neuromod_plasticity_get_state(bridge, &state_after);

    EXPECT_LT(state_after.eligibility_level, state_before.eligibility_level);
}

//=============================================================================
// Top-Down Feedback Tests
//=============================================================================

TEST_F(NeuromodPlasticityBridgeTest, ReportSuccess) {
    float vta_trigger;
    int ret = neuromod_plasticity_report_success(bridge, 0.8f, &vta_trigger);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(vta_trigger, 0.0f);
}

TEST_F(NeuromodPlasticityBridgeTest, ReportNovelty) {
    float lc_trigger;
    int ret = neuromod_plasticity_report_novelty(bridge, 0.7f, &lc_trigger);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(lc_trigger, 0.0f);
}

TEST_F(NeuromodPlasticityBridgeTest, ReportConflict) {
    float ht_mod;
    int ret = neuromod_plasticity_report_conflict(bridge, 0.6f, &ht_mod);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(ht_mod, 0.0f);
}

TEST_F(NeuromodPlasticityBridgeTest, ReportPredictionMiss) {
    float hab_trigger;
    int ret = neuromod_plasticity_report_prediction_miss(bridge, 0.7f, &hab_trigger);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(hab_trigger, 0.0f);
}

//=============================================================================
// Unified Modulation Tests
//=============================================================================

TEST_F(NeuromodPlasticityBridgeTest, ComputeModulation) {
    neuromod_plasticity_state_t state;
    int ret = neuromod_plasticity_compute_modulation(bridge, 0.5f, 0.5f, 0.5f, 0.3f, &state);
    EXPECT_EQ(ret, 0);
    EXPECT_GE(state.ltp_gate_level, 0.0f);
    EXPECT_LE(state.ltp_gate_level, 1.0f);
    EXPECT_GE(state.learning_efficiency, 0.0f);
    EXPECT_LE(state.learning_efficiency, 1.0f);
}

//=============================================================================
// Mode and Diagnostics Tests
//=============================================================================

TEST_F(NeuromodPlasticityBridgeTest, ModeNameMapping) {
    EXPECT_STREQ(neuromod_plasticity_mode_name(PLASTICITY_MODE_NORMAL), "Normal");
    EXPECT_STREQ(neuromod_plasticity_mode_name(PLASTICITY_MODE_BOOSTED), "Boosted");
    EXPECT_STREQ(neuromod_plasticity_mode_name(PLASTICITY_MODE_GATED), "Gated");
    EXPECT_STREQ(neuromod_plasticity_mode_name(PLASTICITY_MODE_CONSOLIDATING), "Consolidating");
    EXPECT_STREQ(neuromod_plasticity_mode_name(PLASTICITY_MODE_SUPPRESSED), "Suppressed");
}

TEST_F(NeuromodPlasticityBridgeTest, GetEfficiency) {
    float efficiency = neuromod_plasticity_get_efficiency(bridge);
    EXPECT_GE(efficiency, 0.0f);
    EXPECT_LE(efficiency, 1.0f);
}

TEST_F(NeuromodPlasticityBridgeTest, GetCoherence) {
    float coherence = neuromod_plasticity_get_coherence(bridge);
    EXPECT_GE(coherence, 0.0f);
    EXPECT_LE(coherence, 1.0f);
}

TEST_F(NeuromodPlasticityBridgeTest, PrintSummary) {
    neuromod_plasticity_print_summary(bridge);
    neuromod_plasticity_print_summary(nullptr);
}

//=============================================================================
// Stats Tests
//=============================================================================

TEST_F(NeuromodPlasticityBridgeTest, GetStats) {
    neuromod_plasticity_apply_da_gating(bridge, 0.8f, nullptr);
    neuromod_plasticity_apply_reward_pe(bridge, 0.8f, 0.3f, nullptr);

    neuromod_plasticity_stats_t stats;
    int ret = neuromod_plasticity_get_stats(bridge, &stats);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(stats.ltp_gate_openings, 0u);
}

TEST_F(NeuromodPlasticityBridgeTest, ResetStats) {
    neuromod_plasticity_apply_da_gating(bridge, 0.8f, nullptr);

    int ret = neuromod_plasticity_reset_stats(bridge);
    EXPECT_EQ(ret, 0);

    neuromod_plasticity_stats_t stats;
    neuromod_plasticity_get_stats(bridge, &stats);
    EXPECT_EQ(stats.ltp_gate_openings, 0u);
}
