/**
 * @file test_epistemic_snn_plasticity_regression.cpp
 * @brief Regression tests for Epistemic SNN-Plasticity bridges
 * @version 1.0.0
 * @date 2026-01-06
 *
 * WHAT: Regression tests to ensure epistemic bridges maintain performance over time
 * WHY:  Prevent regressions in source reliability tracking, bias detection, belief update
 * HOW:  Test known baseline scenarios, boundary conditions, and performance thresholds
 */

#include <gtest/gtest.h>
#include <cmath>
#include <chrono>

#include "cognitive/epistemic/nimcp_epistemic_snn_bridge.h"
#include "cognitive/epistemic/nimcp_epistemic_plasticity_bridge.h"

class EpistemicRegressionTest : public ::testing::Test {
protected:
    epistemic_snn_bridge_t* snn = nullptr;
    epistemic_plasticity_bridge_t* plasticity = nullptr;

    void SetUp() override {
        epistemic_snn_config_t snn_config = epistemic_snn_config_default();
        snn_config.max_sources = EPISTEMIC_SNN_MAX_SOURCES;
        snn_config.neurons_per_dim = EPISTEMIC_SNN_NEURONS_PER_DIM;
        snn_config.dt_ms = 1.0f;
        snn_config.evidence_gain = 2.0f;
        snn_config.uncertainty_gain = 1.0f;
        snn_config.bias_detection_threshold = 0.5f;
        snn_config.encoding_type = EPISTEMIC_SNN_ENCODE_RATE;
        snn_config.enable_source_tracking = true;
        snn_config.enable_bias_detection = true;
        snn_config.enable_conspiracy_detection = false;
        snn_config.enable_bio_async = false;
        snn = epistemic_snn_create(&snn_config);
        ASSERT_NE(snn, nullptr);

        epistemic_plasticity_config_t plas_config = epistemic_plasticity_config_default();
        plas_config.stdp_ltp_window_ms = 20.0f;
        plas_config.stdp_ltd_window_ms = 20.0f;
        plas_config.stdp_a_plus = 0.01f;
        plas_config.stdp_a_minus = 0.0105f;
        plas_config.stdp_tau_plus = 20.0f;
        plas_config.stdp_tau_minus = 20.0f;
        plas_config.enable_source_learning = true;
        plas_config.source_correct_ltp = 0.02f;
        plas_config.source_incorrect_ltd = 0.015f;
        plas_config.enable_bcm = true;
        plas_config.bcm_threshold_tau = 1000.0f;
        plas_config.bcm_activity_tau = 100.0f;
        plas_config.enable_homeostatic = true;
        plas_config.target_epistemic_quality = 0.7f;
        plas_config.homeostatic_tau_ms = 5000.0f;
        plas_config.enable_eligibility = true;
        plas_config.eligibility_decay = 0.95f;
        plas_config.reward_modulation_gain = 1.0f;
        plas_config.weight_min = 0.0f;
        plas_config.weight_max = 1.0f;
        plas_config.initial_weight = 0.5f;
        plas_config.enable_bio_async = false;
        plasticity = epistemic_plasticity_create(&plas_config);
        ASSERT_NE(plasticity, nullptr);
    }

    void TearDown() override {
        if (snn) epistemic_snn_destroy(snn);
        if (plasticity) epistemic_plasticity_destroy(plasticity);
    }
};

// ============================================================================
// Baseline Behavior Regression Tests
// ============================================================================

TEST_F(EpistemicRegressionTest, HighQualityEvidenceBaseline) {
    // Known good scenario: high quality evidence should yield high epistemic quality
    epistemic_snn_encode_evidence(snn, 0.95f, 0.9f, 0.95f);
    epistemic_snn_simulate(snn, 100.0f);

    epistemic_snn_output_t output;
    epistemic_snn_decode_assessment(snn, &output);

    // BASELINE: High quality evidence should produce quality >= 0.5
    EXPECT_GE(output.epistemic_quality, 0.5f);
    EXPECT_GE(output.evidence_strength, 0.3f);
    EXPECT_FALSE(output.bias_detected);
}

TEST_F(EpistemicRegressionTest, LowQualityEvidenceBaseline) {
    // Known scenario: low quality evidence
    epistemic_snn_encode_evidence(snn, 0.2f, 0.3f, 0.1f);
    epistemic_snn_simulate(snn, 100.0f);

    epistemic_snn_output_t output;
    epistemic_snn_decode_assessment(snn, &output);

    // BASELINE: Low quality should produce lower epistemic quality
    EXPECT_LT(output.epistemic_quality, 0.8f);
}

TEST_F(EpistemicRegressionTest, SourceReliabilityLearningBaseline) {
    epistemic_plasticity_register_synapse(
        plasticity, 1, EPISTEMIC_SYNAPSE_SOURCE_RELIABILITY, 0, 0.5f);

    // 10 correct feedbacks should increase reliability
    for (int i = 0; i < 10; i++) {
        epistemic_plasticity_source_feedback(plasticity, 0, true, i * 1000);
        epistemic_plasticity_update(plasticity, 10.0f);
    }

    float reliability = epistemic_plasticity_get_source_reliability(plasticity, 0);

    // BASELINE: 10 correct feedbacks should yield reliability > 0.7
    EXPECT_GT(reliability, 0.7f);
}

// ============================================================================
// Boundary Condition Regression Tests
// ============================================================================

TEST_F(EpistemicRegressionTest, ZeroEvidenceBoundary) {
    epistemic_snn_encode_evidence(snn, 0.0f, 0.0f, 0.0f);
    epistemic_snn_simulate(snn, 100.0f);

    epistemic_snn_output_t output;
    int result = epistemic_snn_decode_assessment(snn, &output);

    EXPECT_EQ(result, 0);  // Should not crash
    EXPECT_GE(output.epistemic_quality, 0.0f);
    EXPECT_LE(output.epistemic_quality, 1.0f);
}

TEST_F(EpistemicRegressionTest, MaxEvidenceBoundary) {
    epistemic_snn_encode_evidence(snn, 1.0f, 1.0f, 1.0f);
    epistemic_snn_simulate(snn, 100.0f);

    epistemic_snn_output_t output;
    int result = epistemic_snn_decode_assessment(snn, &output);

    EXPECT_EQ(result, 0);
    EXPECT_GE(output.epistemic_quality, 0.5f);  // Max evidence should be quality
}

TEST_F(EpistemicRegressionTest, WeightBoundaryMin) {
    epistemic_plasticity_register_synapse(
        plasticity, 1, EPISTEMIC_SYNAPSE_SOURCE_RELIABILITY, 0, 0.1f);

    // Many negative feedbacks
    for (int i = 0; i < 50; i++) {
        epistemic_plasticity_source_feedback(plasticity, 0, false, i * 1000);
        epistemic_plasticity_update(plasticity, 10.0f);
    }

    epistemic_plasticity_synapse_t synapse;
    epistemic_plasticity_get_synapse(plasticity, 1, &synapse);

    // Weight should be at or near minimum but not negative
    EXPECT_GE(synapse.weight, 0.0f);
}

TEST_F(EpistemicRegressionTest, WeightBoundaryMax) {
    epistemic_plasticity_register_synapse(
        plasticity, 1, EPISTEMIC_SYNAPSE_SOURCE_RELIABILITY, 0, 0.9f);

    // Many positive feedbacks
    for (int i = 0; i < 50; i++) {
        epistemic_plasticity_source_feedback(plasticity, 0, true, i * 1000);
        epistemic_plasticity_reward(plasticity, 1.0f, i * 1000 + 500);
        epistemic_plasticity_update(plasticity, 10.0f);
    }

    epistemic_plasticity_synapse_t synapse;
    epistemic_plasticity_get_synapse(plasticity, 1, &synapse);

    // Weight should be at or near maximum but not exceed 1.0
    EXPECT_LE(synapse.weight, 1.0f);
}

// ============================================================================
// Performance Regression Tests
// ============================================================================

TEST_F(EpistemicRegressionTest, SNNSimulationPerformance) {
    // Performance baseline: 100ms simulation should complete in < 100ms wall time
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 100; i++) {
        epistemic_snn_encode_evidence(snn, 0.8f, 0.7f, 0.9f);
        epistemic_snn_simulate(snn, 100.0f);
        epistemic_snn_reset(snn);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    // BASELINE: 100 iterations should complete in < 5000ms
    EXPECT_LT(duration, 5000);
}

TEST_F(EpistemicRegressionTest, PlasticityUpdatePerformance) {
    epistemic_plasticity_register_synapse(
        plasticity, 1, EPISTEMIC_SYNAPSE_SOURCE_RELIABILITY, 0, 0.5f);

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 1000; i++) {
        epistemic_plasticity_source_feedback(plasticity, 0, i % 2 == 0, i * 100);
        epistemic_plasticity_update(plasticity, 1.0f);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    // BASELINE: 1000 updates should complete in < 1000ms
    EXPECT_LT(duration, 1000);
}

// ============================================================================
// Known Issue Regression Tests
// ============================================================================

TEST_F(EpistemicRegressionTest, NoSourceLeakage) {
    // Ensure sources don't leak between evaluations
    epistemic_snn_register_source(snn, 1, 0.9f);
    epistemic_snn_register_source(snn, 2, 0.3f);

    // Update source 1
    epistemic_snn_update_source_reliability(snn, 1, true);

    // Source 2 should be unchanged
    float rel2 = epistemic_snn_get_source_reliability(snn, 2);
    EXPECT_LT(rel2, 0.5f);  // Should still be low
}

TEST_F(EpistemicRegressionTest, ResetClearsState) {
    epistemic_snn_encode_evidence(snn, 0.9f, 0.9f, 0.9f);
    epistemic_snn_simulate(snn, 100.0f);

    epistemic_snn_reset(snn);

    epistemic_snn_bridge_state_t state;
    epistemic_snn_get_state(snn, &state);

    EXPECT_EQ(state.state, EPISTEMIC_SNN_STATE_IDLE);
}

// ============================================================================
// Consistency Regression Tests
// ============================================================================

TEST_F(EpistemicRegressionTest, DeterministicOutput) {
    // Same input should produce consistent output
    float quality1, quality2;

    epistemic_snn_encode_evidence(snn, 0.8f, 0.7f, 0.9f);
    epistemic_snn_simulate(snn, 100.0f);
    quality1 = epistemic_snn_get_epistemic_quality(snn);

    epistemic_snn_reset(snn);

    epistemic_snn_encode_evidence(snn, 0.8f, 0.7f, 0.9f);
    epistemic_snn_simulate(snn, 100.0f);
    quality2 = epistemic_snn_get_epistemic_quality(snn);

    // Should be approximately equal (some stochasticity allowed)
    EXPECT_NEAR(quality1, quality2, 0.1f);
}
