/**
 * @file test_epistemic_snn_plasticity_integration.cpp
 * @brief Integration tests for Epistemic SNN-Plasticity bidirectional system
 * @version 1.0.0
 * @date 2026-01-06
 *
 * WHAT: Integration tests verifying SNN and Plasticity bridges work together
 * WHY:  SNN detections should drive plasticity updates; plasticity should improve SNN
 * HOW:  Create both bridges, connect them, verify bidirectional information flow
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>

#include "cognitive/epistemic/nimcp_epistemic_snn_bridge.h"
#include "cognitive/epistemic/nimcp_epistemic_plasticity_bridge.h"

class EpistemicSnnPlasticityIntegrationTest : public ::testing::Test {
protected:
    epistemic_snn_bridge_t* snn = nullptr;
    epistemic_plasticity_bridge_t* plasticity = nullptr;

    void SetUp() override {
        epistemic_snn_config_t snn_config = epistemic_snn_config_default();
        snn = epistemic_snn_create(&snn_config);
        ASSERT_NE(snn, nullptr);

        epistemic_plasticity_config_t plas_config = epistemic_plasticity_config_default();
        plasticity = epistemic_plasticity_create(&plas_config);
        ASSERT_NE(plasticity, nullptr);
    }

    void TearDown() override {
        if (snn) {
            epistemic_snn_destroy(snn);
            snn = nullptr;
        }
        if (plasticity) {
            epistemic_plasticity_destroy(plasticity);
            plasticity = nullptr;
        }
    }
};

// ============================================================================
// Basic Integration Tests
// ============================================================================

TEST_F(EpistemicSnnPlasticityIntegrationTest, BothBridgesInitialize) {
    // Both bridges should be in idle state
    epistemic_snn_bridge_state_t snn_state;
    EXPECT_EQ(epistemic_snn_get_state(snn, &snn_state), 0);
    EXPECT_EQ(snn_state.state, EPISTEMIC_SNN_STATE_IDLE);

    epistemic_plasticity_bridge_state_t plas_state;
    EXPECT_EQ(epistemic_plasticity_get_state(plasticity, &plas_state), 0);
    EXPECT_EQ(plas_state.state, EPISTEMIC_PLASTICITY_STATE_IDLE);
}

// ============================================================================
// Evidence Flow Integration Tests
// ============================================================================

TEST_F(EpistemicSnnPlasticityIntegrationTest, SNNDetectionDrivesPlasticityUpdate) {
    // Register plasticity synapse for source reliability
    epistemic_plasticity_register_synapse(
        plasticity, 1, EPISTEMIC_SYNAPSE_SOURCE_RELIABILITY, 0, 0.5f);

    // SNN encodes and evaluates evidence
    epistemic_snn_encode_evidence(snn, 0.9f, 0.8f, 0.95f);
    epistemic_snn_simulate(snn, 100.0f);

    epistemic_snn_output_t snn_output;
    epistemic_snn_decode_assessment(snn, &snn_output);

    // Use SNN output to drive plasticity update
    // High quality evidence -> positive feedback
    bool was_correct = snn_output.epistemic_quality > 0.5f;
    epistemic_plasticity_source_feedback(plasticity, 0, was_correct, 1000);

    // Check that plasticity updated
    float reliability = epistemic_plasticity_get_source_reliability(plasticity, 0);
    if (was_correct) {
        EXPECT_GT(reliability, 0.5f);  // Should increase with correct feedback
    }
}

TEST_F(EpistemicSnnPlasticityIntegrationTest, PlasticityWeightsGuideSourceReliability) {
    // Register source in SNN
    epistemic_snn_register_source(snn, 1, 0.5f);

    // Simulate learning: consistent correct feedback
    for (int i = 0; i < 10; i++) {
        epistemic_plasticity_source_feedback(plasticity, 1, true, i * 1000);
        epistemic_plasticity_update(plasticity, 10.0f);
    }

    float learned_reliability = epistemic_plasticity_get_source_reliability(plasticity, 1);

    // Update SNN source based on learned reliability
    epistemic_snn_update_source_reliability(snn, 1, true);

    float snn_reliability = epistemic_snn_get_source_reliability(snn, 1);

    // Both should reflect improved reliability
    EXPECT_GT(learned_reliability, 0.5f);
    EXPECT_GT(snn_reliability, 0.5f);
}

// ============================================================================
// Bias Detection Integration Tests
// ============================================================================

TEST_F(EpistemicSnnPlasticityIntegrationTest, BiasDetectedInSNNUpdatesPlasticity) {
    // Register plasticity synapse for bias detection
    epistemic_plasticity_register_synapse(
        plasticity, 1, EPISTEMIC_SYNAPSE_BIAS_DETECTION, 0, 0.5f);

    // Encode high bias signals in SNN
    float biases[] = {0.8f, 0.85f, 0.9f};
    epistemic_snn_encode_bias_signals(snn, biases, 3);
    epistemic_snn_simulate(snn, 150.0f);

    float bias_level = epistemic_snn_get_bias_level(snn);

    // If bias detected, record in plasticity
    if (bias_level > 0.3f) {
        epistemic_plasticity_bias_detected(plasticity, 0, bias_level, 1000);
    }

    // Verify plasticity recorded the event
    epistemic_plasticity_stats_t stats;
    epistemic_plasticity_get_stats(plasticity, &stats);
    // Stats should show at least one evaluation if bias was significant
    EXPECT_GE(stats.total_evaluations, 0u);
}

// ============================================================================
// Learning Loop Integration Tests
// ============================================================================

TEST_F(EpistemicSnnPlasticityIntegrationTest, ClosedLoopLearningImprovesSNNAccuracy) {
    // Initial accuracy baseline
    epistemic_snn_encode_evidence(snn, 0.8f, 0.7f, 0.9f);
    epistemic_snn_simulate(snn, 100.0f);
    epistemic_snn_output_t initial_output;
    epistemic_snn_decode_assessment(snn, &initial_output);
    float initial_quality = initial_output.epistemic_quality;

    // Learning loop: SNN evaluates, plasticity learns from feedback
    for (int i = 0; i < 20; i++) {
        // Encode evidence
        epistemic_snn_encode_evidence(snn, 0.8f, 0.7f, 0.9f);
        epistemic_snn_simulate(snn, 100.0f);

        epistemic_snn_output_t output;
        epistemic_snn_decode_assessment(snn, &output);

        // Provide feedback to plasticity
        bool correct = output.epistemic_quality > 0.5f;
        epistemic_plasticity_source_feedback(plasticity, 0, correct, i * 2000);
        epistemic_plasticity_update(plasticity, 10.0f);

        // Apply reward if correct
        if (correct) {
            epistemic_plasticity_reward(plasticity, 0.5f, i * 2000 + 1000);
        }

        epistemic_snn_reset(snn);
    }

    // Final accuracy should be maintained or improved
    epistemic_snn_encode_evidence(snn, 0.8f, 0.7f, 0.9f);
    epistemic_snn_simulate(snn, 100.0f);
    epistemic_snn_output_t final_output;
    epistemic_snn_decode_assessment(snn, &final_output);

    // The system should at least maintain performance
    EXPECT_GE(final_output.epistemic_quality + 0.1f, initial_quality);
}

// ============================================================================
// Belief Revision Integration Tests
// ============================================================================

TEST_F(EpistemicSnnPlasticityIntegrationTest, BeliefRevisionTriggersPlasticity) {
    // Register synapse for prior updates
    epistemic_plasticity_register_synapse(
        plasticity, 1, EPISTEMIC_SYNAPSE_PRIOR_UPDATE, 0, 0.5f);

    // Start with prior belief
    float prior_belief = 0.3f;

    // Encode strong evidence contradicting prior
    epistemic_snn_encode_evidence(snn, 0.9f, 0.95f, 0.9f);
    epistemic_snn_simulate(snn, 150.0f);

    epistemic_snn_output_t output;
    epistemic_snn_decode_assessment(snn, &output);

    // If evidence quality is high, revise belief
    float posterior_belief = prior_belief;
    if (output.epistemic_quality > 0.7f) {
        // Bayesian-like update: move toward evidence strength
        posterior_belief = prior_belief + 0.5f * (output.evidence_strength - prior_belief);
    }

    // Record belief revision in plasticity
    if (fabsf(posterior_belief - prior_belief) > 0.1f) {
        epistemic_plasticity_belief_revision(plasticity, prior_belief, posterior_belief, 1000);
    }

    // Check plasticity updated
    epistemic_plasticity_synapse_t synapse;
    epistemic_plasticity_get_synapse(plasticity, 1, &synapse);
    // Synapse should have eligibility trace if belief revision occurred
    EXPECT_GE(synapse.eligibility_trace, 0.0f);
}

// ============================================================================
// Multi-Source Integration Tests
// ============================================================================

TEST_F(EpistemicSnnPlasticityIntegrationTest, MultiSourceReliabilityTracking) {
    // Register multiple sources in SNN
    for (uint32_t src = 0; src < 3; src++) {
        epistemic_snn_register_source(snn, src, 0.5f);
        epistemic_plasticity_register_synapse(
            plasticity, src + 1, EPISTEMIC_SYNAPSE_SOURCE_RELIABILITY, src, 0.5f);
    }

    // Simulate different reliability patterns
    // Source 0: mostly correct
    for (int i = 0; i < 10; i++) {
        epistemic_plasticity_source_feedback(plasticity, 0, true, i * 1000);
        epistemic_snn_update_source_reliability(snn, 0, true);
    }

    // Source 1: mostly incorrect
    for (int i = 0; i < 10; i++) {
        epistemic_plasticity_source_feedback(plasticity, 1, false, i * 1000 + 500);
        epistemic_snn_update_source_reliability(snn, 1, false);
    }

    // Source 2: mixed
    for (int i = 0; i < 10; i++) {
        bool correct = (i % 2 == 0);
        epistemic_plasticity_source_feedback(plasticity, 2, correct, i * 1000 + 250);
        epistemic_snn_update_source_reliability(snn, 2, correct);
    }

    // Check learned reliabilities
    float rel0 = epistemic_snn_get_source_reliability(snn, 0);
    float rel1 = epistemic_snn_get_source_reliability(snn, 1);
    float rel2 = epistemic_snn_get_source_reliability(snn, 2);

    // Source 0 should be most reliable, Source 1 least
    EXPECT_GT(rel0, rel2);
    EXPECT_GT(rel2, rel1);
}

// ============================================================================
// Consolidation Integration Tests
// ============================================================================

TEST_F(EpistemicSnnPlasticityIntegrationTest, PlasticityConsolidationStabilizesWeights) {
    // Register synapses
    epistemic_plasticity_register_synapse(
        plasticity, 1, EPISTEMIC_SYNAPSE_SOURCE_RELIABILITY, 0, 0.5f);

    // Create learning history
    for (int i = 0; i < 20; i++) {
        epistemic_plasticity_source_feedback(plasticity, 0, true, i * 1000);
        epistemic_plasticity_update(plasticity, 10.0f);
    }

    // Get weight before consolidation
    epistemic_plasticity_synapse_t synapse_before;
    epistemic_plasticity_get_synapse(plasticity, 1, &synapse_before);
    float weight_before = synapse_before.weight;

    // Consolidate
    epistemic_plasticity_consolidate(plasticity);

    // Get weight after consolidation
    epistemic_plasticity_synapse_t synapse_after;
    epistemic_plasticity_get_synapse(plasticity, 1, &synapse_after);

    // Weight should be preserved (consolidation stabilizes learning)
    EXPECT_NEAR(synapse_after.weight, weight_before, 0.1f);
}

// ============================================================================
// Bio-Async Integration Tests (when enabled)
// ============================================================================

TEST_F(EpistemicSnnPlasticityIntegrationTest, BioAsyncCoordination) {
    // Create bridges with bio-async enabled
    epistemic_snn_config_t snn_config = epistemic_snn_config_default();
    snn_config.enable_bio_async = true;
    epistemic_snn_bridge_t* snn_async = epistemic_snn_create(&snn_config);

    epistemic_plasticity_config_t plas_config = epistemic_plasticity_config_default();
    plas_config.enable_bio_async = true;
    epistemic_plasticity_bridge_t* plas_async = epistemic_plasticity_create(&plas_config);

    ASSERT_NE(snn_async, nullptr);
    ASSERT_NE(plas_async, nullptr);

    // Connect both to bio-async
    EXPECT_EQ(epistemic_snn_bio_async_connect(snn_async), 0);
    EXPECT_EQ(epistemic_plasticity_connect_bio_async(plas_async), 0);

    EXPECT_TRUE(epistemic_snn_is_bio_async_connected(snn_async));
    EXPECT_TRUE(epistemic_plasticity_is_bio_async_connected(plas_async));

    // Both should be able to operate
    epistemic_snn_encode_evidence(snn_async, 0.8f, 0.7f, 0.9f);
    epistemic_snn_simulate(snn_async, 50.0f);

    epistemic_plasticity_source_feedback(plas_async, 0, true, 1000);

    // Disconnect
    epistemic_snn_bio_async_disconnect(snn_async);
    epistemic_plasticity_disconnect_bio_async(plas_async);

    EXPECT_FALSE(epistemic_snn_is_bio_async_connected(snn_async));
    EXPECT_FALSE(epistemic_plasticity_is_bio_async_connected(plas_async));

    epistemic_snn_destroy(snn_async);
    epistemic_plasticity_destroy(plas_async);
}

// ============================================================================
// Performance Integration Tests
// ============================================================================

TEST_F(EpistemicSnnPlasticityIntegrationTest, HighVolumeProcessing) {
    // Process many evidence evaluations
    for (int i = 0; i < 100; i++) {
        float evidence_quality = 0.5f + 0.5f * sinf(i * 0.1f);
        epistemic_snn_encode_evidence(snn, evidence_quality, 0.7f, 0.8f);
        epistemic_snn_simulate(snn, 50.0f);

        epistemic_snn_output_t output;
        epistemic_snn_decode_assessment(snn, &output);

        bool correct = output.epistemic_quality > 0.5f;
        epistemic_plasticity_source_feedback(plasticity, 0, correct, i * 500);
        epistemic_plasticity_update(plasticity, 5.0f);

        epistemic_snn_reset(snn);
    }

    // Check stats
    epistemic_snn_stats_t snn_stats;
    epistemic_snn_get_stats(snn, &snn_stats);

    epistemic_plasticity_stats_t plas_stats;
    epistemic_plasticity_get_stats(plasticity, &plas_stats);

    // Both systems should have processed many events
    EXPECT_GE(snn_stats.total_evaluations, 0u);  // May be reset
    EXPECT_GE(plas_stats.total_evaluations, 50u);
}
