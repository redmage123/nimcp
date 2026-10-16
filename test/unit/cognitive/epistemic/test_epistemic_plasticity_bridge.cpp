/**
 * @file test_epistemic_plasticity_bridge.cpp
 * @brief Unit tests for Epistemic-Plasticity Integration Bridge
 * @version 1.0.0
 * @date 2026-01-06
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

#include "cognitive/epistemic/nimcp_epistemic_plasticity_bridge.h"

class EpistemicPlasticityBridgeTest : public ::testing::Test {
protected:
    epistemic_plasticity_bridge_t* bridge = nullptr;

    void SetUp() override {
        epistemic_plasticity_config_t config = epistemic_plasticity_config_default();
        bridge = epistemic_plasticity_create(&config);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            epistemic_plasticity_destroy(bridge);
            bridge = nullptr;
        }
    }
};

// ============================================================================
// Lifecycle Tests
// ============================================================================

TEST_F(EpistemicPlasticityBridgeTest, CreateWithNullConfig) {
    epistemic_plasticity_bridge_t* b = epistemic_plasticity_create(nullptr);
    EXPECT_EQ(b, nullptr);
}

TEST_F(EpistemicPlasticityBridgeTest, DefaultConfigValid) {
    epistemic_plasticity_config_t config = epistemic_plasticity_config_default();
    EXPECT_GT(config.stdp_ltp_window_ms, 0.0f);
    EXPECT_GT(config.stdp_a_plus, 0.0f);
    EXPECT_GT(config.stdp_a_minus, 0.0f);
    EXPECT_TRUE(config.enable_source_learning);
}

TEST_F(EpistemicPlasticityBridgeTest, ResetSucceeds) {
    int result = epistemic_plasticity_reset(bridge);
    EXPECT_EQ(result, 0);
}

TEST_F(EpistemicPlasticityBridgeTest, ResetNullBridge) {
    int result = epistemic_plasticity_reset(nullptr);
    EXPECT_EQ(result, -1);
}

// ============================================================================
// Synapse Management Tests
// ============================================================================

TEST_F(EpistemicPlasticityBridgeTest, RegisterSynapse) {
    int result = epistemic_plasticity_register_synapse(
        bridge, 1, EPISTEMIC_SYNAPSE_EVIDENCE_INTEGRATION, 0, 0.5f);
    EXPECT_EQ(result, 0);
}

TEST_F(EpistemicPlasticityBridgeTest, RegisterSynapseDuplicate) {
    epistemic_plasticity_register_synapse(bridge, 1, EPISTEMIC_SYNAPSE_EVIDENCE_INTEGRATION, 0, 0.5f);
    int result = epistemic_plasticity_register_synapse(
        bridge, 1, EPISTEMIC_SYNAPSE_SOURCE_RELIABILITY, 1, 0.6f);
    EXPECT_EQ(result, -1);
}

TEST_F(EpistemicPlasticityBridgeTest, UnregisterSynapse) {
    epistemic_plasticity_register_synapse(bridge, 1, EPISTEMIC_SYNAPSE_EVIDENCE_INTEGRATION, 0, 0.5f);
    int result = epistemic_plasticity_unregister_synapse(bridge, 1);
    EXPECT_EQ(result, 0);
}

TEST_F(EpistemicPlasticityBridgeTest, UnregisterNonexistentSynapse) {
    int result = epistemic_plasticity_unregister_synapse(bridge, 999);
    EXPECT_EQ(result, -1);
}

TEST_F(EpistemicPlasticityBridgeTest, GetSynapse) {
    epistemic_plasticity_register_synapse(bridge, 1, EPISTEMIC_SYNAPSE_EVIDENCE_INTEGRATION, 0, 0.5f);

    epistemic_plasticity_synapse_t synapse;
    int result = epistemic_plasticity_get_synapse(bridge, 1, &synapse);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(synapse.synapse_id, 1u);
    EXPECT_EQ(synapse.type, EPISTEMIC_SYNAPSE_EVIDENCE_INTEGRATION);
    EXPECT_FLOAT_EQ(synapse.weight, 0.5f);
}

TEST_F(EpistemicPlasticityBridgeTest, GetNonexistentSynapse) {
    epistemic_plasticity_synapse_t synapse;
    int result = epistemic_plasticity_get_synapse(bridge, 999, &synapse);
    EXPECT_EQ(result, -1);
}

// ============================================================================
// Event Recording Tests
// ============================================================================

TEST_F(EpistemicPlasticityBridgeTest, EvidenceUpdate) {
    epistemic_plasticity_register_synapse(bridge, 1, EPISTEMIC_SYNAPSE_EVIDENCE_INTEGRATION, 0, 0.5f);

    int result = epistemic_plasticity_evidence_update(bridge, 0, 0.8f, 1000);
    EXPECT_EQ(result, 0);
}

TEST_F(EpistemicPlasticityBridgeTest, SourceFeedbackCorrect) {
    epistemic_plasticity_register_synapse(bridge, 1, EPISTEMIC_SYNAPSE_SOURCE_RELIABILITY, 0, 0.5f);

    int result = epistemic_plasticity_source_feedback(bridge, 0, true, 1000);
    EXPECT_EQ(result, 0);

    // Check that source reliability increased
    float reliability = epistemic_plasticity_get_source_reliability(bridge, 0);
    EXPECT_GT(reliability, 0.5f);
}

TEST_F(EpistemicPlasticityBridgeTest, SourceFeedbackIncorrect) {
    epistemic_plasticity_register_synapse(bridge, 1, EPISTEMIC_SYNAPSE_SOURCE_RELIABILITY, 0, 0.5f);

    // First get baseline
    epistemic_plasticity_source_feedback(bridge, 0, true, 1000);
    float initial = epistemic_plasticity_get_source_reliability(bridge, 0);

    // Now incorrect feedback
    epistemic_plasticity_source_feedback(bridge, 0, false, 2000);
    float after = epistemic_plasticity_get_source_reliability(bridge, 0);

    EXPECT_LT(after, initial);
}

TEST_F(EpistemicPlasticityBridgeTest, BiasDetected) {
    epistemic_plasticity_register_synapse(bridge, 1, EPISTEMIC_SYNAPSE_BIAS_DETECTION, 0, 0.5f);

    int result = epistemic_plasticity_bias_detected(bridge, 0, 0.8f, 1000);
    EXPECT_EQ(result, 0);
}

TEST_F(EpistemicPlasticityBridgeTest, BeliefRevision) {
    epistemic_plasticity_register_synapse(bridge, 1, EPISTEMIC_SYNAPSE_PRIOR_UPDATE, 0, 0.5f);

    int result = epistemic_plasticity_belief_revision(bridge, 0.3f, 0.8f, 1000);
    EXPECT_EQ(result, 0);
}

TEST_F(EpistemicPlasticityBridgeTest, Reward) {
    epistemic_plasticity_register_synapse(bridge, 1, EPISTEMIC_SYNAPSE_EVIDENCE_INTEGRATION, 0, 0.5f);

    // Create eligibility trace
    epistemic_plasticity_evidence_update(bridge, 0, 0.8f, 1000);

    int result = epistemic_plasticity_reward(bridge, 1.0f, 2000);
    EXPECT_EQ(result, 0);
}

// ============================================================================
// Update Tests
// ============================================================================

TEST_F(EpistemicPlasticityBridgeTest, UpdateBasic) {
    int result = epistemic_plasticity_update(bridge, 10.0f);
    EXPECT_EQ(result, 0);
}

TEST_F(EpistemicPlasticityBridgeTest, UpdateDecaysEligibility) {
    epistemic_plasticity_register_synapse(bridge, 1, EPISTEMIC_SYNAPSE_EVIDENCE_INTEGRATION, 0, 0.5f);
    epistemic_plasticity_evidence_update(bridge, 0, 0.8f, 1000);

    epistemic_plasticity_synapse_t synapse_before;
    epistemic_plasticity_get_synapse(bridge, 1, &synapse_before);
    float elig_before = synapse_before.eligibility_trace;

    // Update for significant time
    epistemic_plasticity_update(bridge, 100.0f);

    epistemic_plasticity_synapse_t synapse_after;
    epistemic_plasticity_get_synapse(bridge, 1, &synapse_after);

    EXPECT_LT(synapse_after.eligibility_trace, elig_before);
}

TEST_F(EpistemicPlasticityBridgeTest, Consolidate) {
    epistemic_plasticity_register_synapse(bridge, 1, EPISTEMIC_SYNAPSE_SOURCE_RELIABILITY, 0, 0.5f);

    // Create history of correct feedback
    for (int i = 0; i < 15; i++) {
        epistemic_plasticity_source_feedback(bridge, 0, true, i * 1000);
    }

    int result = epistemic_plasticity_consolidate(bridge);
    EXPECT_EQ(result, 0);
}

// ============================================================================
// Query Tests
// ============================================================================

TEST_F(EpistemicPlasticityBridgeTest, GetSourceReliability) {
    epistemic_plasticity_source_feedback(bridge, 0, true, 1000);

    float reliability = epistemic_plasticity_get_source_reliability(bridge, 0);
    EXPECT_GE(reliability, 0.0f);
    EXPECT_LE(reliability, 1.0f);
}

TEST_F(EpistemicPlasticityBridgeTest, GetSourceReliabilityUnknown) {
    float reliability = epistemic_plasticity_get_source_reliability(bridge, 999);
    EXPECT_FLOAT_EQ(reliability, 0.5f);
}

TEST_F(EpistemicPlasticityBridgeTest, GetEvidenceWeight) {
    epistemic_plasticity_register_synapse(bridge, 1, EPISTEMIC_SYNAPSE_EVIDENCE_INTEGRATION, 0, 0.7f);

    float weight = epistemic_plasticity_get_evidence_weight(bridge, 0);
    EXPECT_FLOAT_EQ(weight, 0.7f);
}

TEST_F(EpistemicPlasticityBridgeTest, GetBiasSensitivity) {
    epistemic_plasticity_register_synapse(bridge, 1, EPISTEMIC_SYNAPSE_BIAS_DETECTION, 0, 0.6f);

    float sensitivity = epistemic_plasticity_get_bias_sensitivity(bridge, 0);
    EXPECT_FLOAT_EQ(sensitivity, 0.6f);
}

TEST_F(EpistemicPlasticityBridgeTest, GetSourceLearning) {
    epistemic_plasticity_source_feedback(bridge, 0, true, 1000);
    epistemic_plasticity_source_feedback(bridge, 0, true, 2000);
    epistemic_plasticity_source_feedback(bridge, 0, false, 3000);

    epistemic_source_learning_t learning;
    int result = epistemic_plasticity_get_source_learning(bridge, 0, &learning);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(learning.total_evaluations, 3u);
    EXPECT_EQ(learning.correct_evaluations, 2u);
}

// ============================================================================
// State and Stats Tests
// ============================================================================

TEST_F(EpistemicPlasticityBridgeTest, GetState) {
    epistemic_plasticity_bridge_state_t state;
    int result = epistemic_plasticity_get_state(bridge, &state);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(state.state, EPISTEMIC_PLASTICITY_STATE_IDLE);
}

TEST_F(EpistemicPlasticityBridgeTest, GetStats) {
    epistemic_plasticity_stats_t stats;
    int result = epistemic_plasticity_get_stats(bridge, &stats);
    EXPECT_EQ(result, 0);
}

TEST_F(EpistemicPlasticityBridgeTest, ResetStats) {
    epistemic_plasticity_evidence_update(bridge, 0, 0.8f, 1000);

    epistemic_plasticity_reset_stats(bridge);

    epistemic_plasticity_stats_t stats;
    epistemic_plasticity_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_evaluations, 0u);
}

// ============================================================================
// Callback Tests
// ============================================================================

static bool weight_callback_called = false;
static void test_weight_callback(uint32_t, uint32_t, float, float, epistemic_learn_event_t, void*) {
    weight_callback_called = true;
}

TEST_F(EpistemicPlasticityBridgeTest, SetWeightCallback) {
    weight_callback_called = false;
    int result = epistemic_plasticity_set_weight_callback(bridge, test_weight_callback, nullptr);
    EXPECT_EQ(result, 0);

    // Trigger callback
    epistemic_plasticity_register_synapse(bridge, 1, EPISTEMIC_SYNAPSE_SOURCE_RELIABILITY, 0, 0.5f);
    epistemic_plasticity_source_feedback(bridge, 0, true, 1000);

    EXPECT_TRUE(weight_callback_called);
}

static bool source_callback_called = false;
static void test_source_callback(uint32_t, float, float, void*) {
    source_callback_called = true;
}

TEST_F(EpistemicPlasticityBridgeTest, SetSourceCallback) {
    source_callback_called = false;
    int result = epistemic_plasticity_set_source_callback(bridge, test_source_callback, nullptr);
    EXPECT_EQ(result, 0);

    // Trigger callback
    epistemic_plasticity_source_feedback(bridge, 0, true, 1000);

    EXPECT_TRUE(source_callback_called);
}

// ============================================================================
// Bio-Async Tests
// ============================================================================

TEST_F(EpistemicPlasticityBridgeTest, BioAsyncNotConnectedByDefault) {
    bool connected = epistemic_plasticity_is_bio_async_connected(bridge);
    EXPECT_FALSE(connected);
}

TEST_F(EpistemicPlasticityBridgeTest, BioAsyncConnectWithEnable) {
    epistemic_plasticity_config_t config = epistemic_plasticity_config_default();
    config.enable_bio_async = true;
    epistemic_plasticity_bridge_t* b = epistemic_plasticity_create(&config);
    ASSERT_NE(b, nullptr);

    int result = epistemic_plasticity_connect_bio_async(b);
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(epistemic_plasticity_is_bio_async_connected(b));

    epistemic_plasticity_disconnect_bio_async(b);
    EXPECT_FALSE(epistemic_plasticity_is_bio_async_connected(b));

    epistemic_plasticity_destroy(b);
}

// ============================================================================
// Learning Scenarios
// ============================================================================

TEST_F(EpistemicPlasticityBridgeTest, SourceLearningOverTime) {
    epistemic_plasticity_register_synapse(bridge, 1, EPISTEMIC_SYNAPSE_SOURCE_RELIABILITY, 0, 0.5f);

    // Simulate reliable source
    for (int i = 0; i < 10; i++) {
        epistemic_plasticity_source_feedback(bridge, 0, true, i * 1000);
        epistemic_plasticity_update(bridge, 10.0f);
    }

    float reliability = epistemic_plasticity_get_source_reliability(bridge, 0);
    EXPECT_GT(reliability, 0.8f);  // Should be high after consistent correct feedback
}

TEST_F(EpistemicPlasticityBridgeTest, UnreliableSourceLearning) {
    epistemic_plasticity_register_synapse(bridge, 1, EPISTEMIC_SYNAPSE_SOURCE_RELIABILITY, 0, 0.5f);

    // Simulate unreliable source (mostly incorrect)
    for (int i = 0; i < 10; i++) {
        bool correct = (i % 5 == 0);  // Only 20% correct
        epistemic_plasticity_source_feedback(bridge, 0, correct, i * 1000);
        epistemic_plasticity_update(bridge, 10.0f);
    }

    float reliability = epistemic_plasticity_get_source_reliability(bridge, 0);
    EXPECT_LT(reliability, 0.5f);  // Should be low after mostly incorrect feedback
}

TEST_F(EpistemicPlasticityBridgeTest, RewardModulatedLearning) {
    epistemic_plasticity_register_synapse(bridge, 1, EPISTEMIC_SYNAPSE_EVIDENCE_INTEGRATION, 0, 0.5f);

    // Create eligibility and reward
    epistemic_plasticity_evidence_update(bridge, 0, 0.9f, 1000);
    epistemic_plasticity_reward(bridge, 1.0f, 2000);

    epistemic_plasticity_synapse_t synapse;
    epistemic_plasticity_get_synapse(bridge, 1, &synapse);

    EXPECT_GT(synapse.weight, 0.5f);  // Weight should increase with positive reward
}
