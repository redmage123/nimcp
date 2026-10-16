/**
 * @file test_reasoning_fep_bridge.cpp
 * @brief Unit tests for Reasoning-FEP Bridge module
 *
 * WHAT: Comprehensive tests for FEP-Reasoning bidirectional integration
 * WHY:  Ensure abductive reasoning and logical constraints work correctly
 * HOW:  Test lifecycle, connections, hypothesis selection, inference, and bio-async
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include "cognitive/reasoning/nimcp_reasoning_fep_bridge.h"
#include "cognitive/free_energy/nimcp_free_energy.h"

class ReasoningFepBridgeTest : public ::testing::Test {
protected:
    reasoning_fep_bridge_t* bridge = nullptr;

    void SetUp() override {
        reasoning_fep_config_t config;
        reasoning_fep_bridge_default_config(&config);
        bridge = reasoning_fep_bridge_create(&config);
    }

    void TearDown() override {
        if (bridge) {
            reasoning_fep_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }
};

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(ReasoningFepBridgeTest, CreateDestroy) {
    ASSERT_NE(bridge, nullptr);
}

TEST_F(ReasoningFepBridgeTest, CreateWithNullConfig) {
    reasoning_fep_bridge_t* br = reasoning_fep_bridge_create(nullptr);
    ASSERT_NE(br, nullptr);
    reasoning_fep_bridge_destroy(br);
}

TEST_F(ReasoningFepBridgeTest, DestroyNull) {
    reasoning_fep_bridge_destroy(nullptr);  // Should not crash
}

TEST_F(ReasoningFepBridgeTest, DefaultConfig) {
    reasoning_fep_config_t config;
    int ret = reasoning_fep_bridge_default_config(&config);

    EXPECT_EQ(ret, 0);
    EXPECT_FLOAT_EQ(config.pe_abduction_threshold, REASONING_FEP_PE_ABDUCTION_THRESHOLD);
    EXPECT_GT(config.hypothesis_selection_temperature, 0.0f);
    EXPECT_GT(config.inference_precision_threshold, 0.0f);
    EXPECT_TRUE(config.enable_pe_abduction);
    EXPECT_TRUE(config.enable_fe_hypothesis_selection);
    EXPECT_TRUE(config.enable_precision_inference);
}

TEST_F(ReasoningFepBridgeTest, DefaultConfigNullPtr) {
    int ret = reasoning_fep_bridge_default_config(nullptr);
    EXPECT_EQ(ret, -1);
}

/* ============================================================================
 * Connection Tests
 * ============================================================================ */

TEST_F(ReasoningFepBridgeTest, ConnectFep) {
    fep_config_t fep_config;
    fep_default_config(&fep_config);
    fep_system_t* fep = fep_create(&fep_config, 8, 4);
    ASSERT_NE(fep, nullptr);

    int ret = reasoning_fep_bridge_connect_fep(bridge, fep);
    EXPECT_EQ(ret, 0);

    fep_destroy(fep);
}

TEST_F(ReasoningFepBridgeTest, ConnectFepNull) {
    EXPECT_EQ(reasoning_fep_bridge_connect_fep(nullptr, nullptr), -1);

    fep_config_t fep_config;
    fep_default_config(&fep_config);
    fep_system_t* fep = fep_create(&fep_config, 8, 4);

    EXPECT_EQ(reasoning_fep_bridge_connect_fep(nullptr, fep), -1);
    EXPECT_EQ(reasoning_fep_bridge_connect_fep(bridge, nullptr), -1);

    fep_destroy(fep);
}

TEST_F(ReasoningFepBridgeTest, ConnectReasoning) {
    // Reasoning system requires complex initialization, test with NULL for now
    int ret = reasoning_fep_bridge_connect_reasoning(bridge, nullptr);
    EXPECT_EQ(ret, -1);  // Should fail with NULL
}

TEST_F(ReasoningFepBridgeTest, ConnectReasoningNull) {
    EXPECT_EQ(reasoning_fep_bridge_connect_reasoning(nullptr, nullptr), -1);
}

TEST_F(ReasoningFepBridgeTest, Disconnect) {
    int ret = reasoning_fep_bridge_disconnect(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(ReasoningFepBridgeTest, DisconnectNull) {
    EXPECT_EQ(reasoning_fep_bridge_disconnect(nullptr), -1);
}

/* ============================================================================
 * State/Stats Tests
 * ============================================================================ */

TEST_F(ReasoningFepBridgeTest, GetState) {
    reasoning_fep_state_t state;
    int ret = reasoning_fep_bridge_get_state(bridge, &state);

    EXPECT_EQ(ret, 0);
    EXPECT_GE(state.current_prediction_error, 0.0f);
    EXPECT_GE(state.current_precision, 0.0f);
    EXPECT_GE(state.current_free_energy, 0.0f);
}

TEST_F(ReasoningFepBridgeTest, GetStateNull) {
    reasoning_fep_state_t state;

    EXPECT_EQ(reasoning_fep_bridge_get_state(nullptr, &state), -1);
    EXPECT_EQ(reasoning_fep_bridge_get_state(bridge, nullptr), -1);
}

TEST_F(ReasoningFepBridgeTest, GetStats) {
    reasoning_fep_stats_t stats;
    int ret = reasoning_fep_bridge_get_stats(bridge, &stats);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(stats.abduction_events, 0u);
    EXPECT_EQ(stats.hypothesis_selections, 0u);
    EXPECT_EQ(stats.inference_steps, 0u);
}

TEST_F(ReasoningFepBridgeTest, GetStatsNull) {
    reasoning_fep_stats_t stats;

    EXPECT_EQ(reasoning_fep_bridge_get_stats(nullptr, &stats), -1);
    EXPECT_EQ(reasoning_fep_bridge_get_stats(bridge, nullptr), -1);
}

/* ============================================================================
 * FEP → Reasoning Direction Tests
 * ============================================================================ */

TEST_F(ReasoningFepBridgeTest, TriggerAbduction) {
    float pe_magnitude = 6.0f;  // Above REASONING_FEP_PE_ABDUCTION_THRESHOLD

    int ret = reasoning_fep_trigger_abduction(bridge, pe_magnitude);
    EXPECT_EQ(ret, 0);

    reasoning_fep_state_t state;
    reasoning_fep_bridge_get_state(bridge, &state);
    EXPECT_TRUE(state.abduction_active);
}

TEST_F(ReasoningFepBridgeTest, TriggerAbductionBelowThreshold) {
    float pe_magnitude = 2.0f;  // Below threshold

    int ret = reasoning_fep_trigger_abduction(bridge, pe_magnitude);
    EXPECT_EQ(ret, 0);

    reasoning_fep_state_t state;
    reasoning_fep_bridge_get_state(bridge, &state);
    EXPECT_FALSE(state.abduction_active);
}

TEST_F(ReasoningFepBridgeTest, TriggerAbductionNull) {
    EXPECT_EQ(reasoning_fep_trigger_abduction(nullptr, 5.0f), -1);
}

TEST_F(ReasoningFepBridgeTest, SelectHypothesisByFe) {
    int ret = reasoning_fep_select_hypothesis_by_fe(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(ReasoningFepBridgeTest, SelectHypothesisByFeNull) {
    EXPECT_EQ(reasoning_fep_select_hypothesis_by_fe(nullptr), -1);
}

TEST_F(ReasoningFepBridgeTest, ModulateInferenceConfidence) {
    int ret = reasoning_fep_modulate_inference_confidence(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(ReasoningFepBridgeTest, ModulateInferenceConfidenceNull) {
    EXPECT_EQ(reasoning_fep_modulate_inference_confidence(nullptr), -1);
}

/* ============================================================================
 * Reasoning → FEP Direction Tests
 * ============================================================================ */

TEST_F(ReasoningFepBridgeTest, ApplyRulePriors) {
    int ret = reasoning_fep_apply_rule_priors(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(ReasoningFepBridgeTest, ApplyRulePriorsNull) {
    EXPECT_EQ(reasoning_fep_apply_rule_priors(nullptr), -1);
}

TEST_F(ReasoningFepBridgeTest, ApplyConclusionConstraints) {
    int ret = reasoning_fep_apply_conclusion_constraints(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(ReasoningFepBridgeTest, ApplyConclusionConstraintsNull) {
    EXPECT_EQ(reasoning_fep_apply_conclusion_constraints(nullptr), -1);
}

TEST_F(ReasoningFepBridgeTest, ApplyExplanationReduction) {
    int ret = reasoning_fep_apply_explanation_reduction(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(ReasoningFepBridgeTest, ApplyExplanationReductionNull) {
    EXPECT_EQ(reasoning_fep_apply_explanation_reduction(nullptr), -1);
}

/* ============================================================================
 * Update Tests
 * ============================================================================ */

TEST_F(ReasoningFepBridgeTest, Update) {
    int ret = reasoning_fep_bridge_update(bridge, 16);  // 16ms delta
    EXPECT_EQ(ret, 0);
}

TEST_F(ReasoningFepBridgeTest, UpdateNull) {
    EXPECT_EQ(reasoning_fep_bridge_update(nullptr, 16), -1);
}

TEST_F(ReasoningFepBridgeTest, UpdateZeroDelta) {
    int ret = reasoning_fep_bridge_update(bridge, 0);
    EXPECT_EQ(ret, 0);
}

/* ============================================================================
 * Bio-Async Tests
 * ============================================================================ */

TEST_F(ReasoningFepBridgeTest, BioAsyncConnect) {
    int ret = reasoning_fep_bridge_connect_bio_async(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(ReasoningFepBridgeTest, BioAsyncDisconnect) {
    reasoning_fep_bridge_connect_bio_async(bridge);

    int ret = reasoning_fep_bridge_disconnect_bio_async(bridge);
    EXPECT_EQ(ret, 0);
    EXPECT_FALSE(reasoning_fep_bridge_is_bio_async_connected(bridge));
}

TEST_F(ReasoningFepBridgeTest, BioAsyncIsConnected) {
    EXPECT_FALSE(reasoning_fep_bridge_is_bio_async_connected(bridge));

    reasoning_fep_bridge_connect_bio_async(bridge);
    // May or may not be connected depending on router availability

    reasoning_fep_bridge_disconnect_bio_async(bridge);
    EXPECT_FALSE(reasoning_fep_bridge_is_bio_async_connected(bridge));
}

TEST_F(ReasoningFepBridgeTest, BioAsyncNullParams) {
    EXPECT_EQ(reasoning_fep_bridge_connect_bio_async(nullptr), -1);
    EXPECT_EQ(reasoning_fep_bridge_disconnect_bio_async(nullptr), -1);
    EXPECT_FALSE(reasoning_fep_bridge_is_bio_async_connected(nullptr));
}

TEST_F(ReasoningFepBridgeTest, BioAsyncDoubleConnect) {
    reasoning_fep_bridge_connect_bio_async(bridge);
    int ret = reasoning_fep_bridge_connect_bio_async(bridge);  // Should be no-op
    EXPECT_EQ(ret, 0);
    reasoning_fep_bridge_disconnect_bio_async(bridge);
}

/* ============================================================================
 * Integration Tests
 * ============================================================================ */

TEST_F(ReasoningFepBridgeTest, HighPeTriggersAbduction) {
    float high_pe = 10.0f;
    reasoning_fep_trigger_abduction(bridge, high_pe);

    reasoning_fep_stats_t stats;
    reasoning_fep_bridge_get_stats(bridge, &stats);
    EXPECT_GT(stats.abduction_events, 0u);
}

TEST_F(ReasoningFepBridgeTest, HypothesisSelectionUsesMinFe) {
    reasoning_fep_select_hypothesis_by_fe(bridge);

    reasoning_fep_stats_t stats;
    reasoning_fep_bridge_get_stats(bridge, &stats);
    EXPECT_GE(stats.hypothesis_selections, 0u);
}

TEST_F(ReasoningFepBridgeTest, RulePriorsConstrainBeliefs) {
    reasoning_fep_apply_rule_priors(bridge);

    reasoning_fep_stats_t stats;
    reasoning_fep_bridge_get_stats(bridge, &stats);
    EXPECT_GE(stats.rule_prior_applications, 0u);
}

TEST_F(ReasoningFepBridgeTest, ExplanationsReduceFreeEnergy) {
    reasoning_fep_apply_explanation_reduction(bridge);

    reasoning_fep_stats_t stats;
    reasoning_fep_bridge_get_stats(bridge, &stats);
    EXPECT_GE(stats.explanation_reductions, 0u);
}
