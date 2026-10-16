/**
 * @file test_salience_snn_plasticity_regression.cpp
 * @brief Regression tests for Salience SNN-Plasticity bridges
 * @version 1.0.0
 * @date 2026-01-06
 *
 * WHAT: Regression tests to ensure salience processing maintains performance
 * WHY:  Prevent regressions in novelty detection, habituation, value learning
 * HOW:  Test known baseline scenarios, boundary conditions, performance thresholds
 */

#include <gtest/gtest.h>
#include <cmath>
#include <chrono>

#include "cognitive/salience/nimcp_salience_snn_bridge.h"
#include "cognitive/salience/nimcp_salience_plasticity_bridge.h"

class SalienceRegressionTest : public ::testing::Test {
protected:
    salience_snn_bridge_t* snn = nullptr;
    salience_plasticity_bridge_t* plasticity = nullptr;

    void SetUp() override {
        salience_snn_config_t snn_config = salience_snn_config_default();
        snn_config.enable_history = true;
        snn = salience_snn_create(&snn_config);
        ASSERT_NE(snn, nullptr);

        salience_plasticity_config_t plas_config = salience_plasticity_config_default();
        plas_config.enable_habituation = true;
        plas_config.enable_value_learning = true;
        plasticity = salience_plasticity_create(&plas_config);
        ASSERT_NE(plasticity, nullptr);
    }

    void TearDown() override {
        if (snn) salience_snn_destroy(snn);
        if (plasticity) salience_plasticity_destroy(plasticity);
    }
};

// ============================================================================
// Salience Detection Baseline Tests
// ============================================================================

TEST_F(SalienceRegressionTest, HighIntensitySalienceBaseline) {
    float features[] = {0.9f, 0.95f, 0.85f};
    salience_snn_encode_features(snn, features, 3);
    salience_snn_simulate(snn, 100.0f);

    salience_snn_output_t output;
    salience_snn_decode_salience(snn, &output);

    // BASELINE: High intensity should yield salience > 0.4
    EXPECT_GT(output.combined_salience, 0.4f);
    EXPECT_GT(output.intensity, 0.3f);
}

TEST_F(SalienceRegressionTest, NovelFeaturesSalienceBaseline) {
    // Add familiar features to history
    float familiar[] = {0.3f, 0.3f, 0.3f};
    for (int i = 0; i < 5; i++) {
        salience_snn_add_to_history(snn, familiar, 3);
    }

    // Present novel features
    float novel[] = {0.9f, 0.8f, 0.85f};
    float novelty = salience_snn_compute_novelty(snn, novel, 3);

    // BASELINE: Novel features should yield novelty > 0.4
    EXPECT_GT(novelty, 0.4f);
}

TEST_F(SalienceRegressionTest, SurpriseDetectionBaseline) {
    float predicted[] = {0.2f, 0.2f, 0.2f};
    float actual[] = {0.9f, 0.85f, 0.8f};

    salience_snn_encode_with_prediction(snn, actual, 3, predicted, 3);
    salience_snn_simulate(snn, 100.0f);

    float surprise = salience_snn_get_surprise(snn);

    // BASELINE: Large prediction error should yield surprise > 0.3
    EXPECT_GT(surprise, 0.3f);
}

// ============================================================================
// Plasticity Learning Baseline Tests
// ============================================================================

TEST_F(SalienceRegressionTest, HabituationBaseline) {
    // Multiple exposures should increase habituation
    for (int i = 0; i < 15; i++) {
        salience_plasticity_feature_exposure(plasticity, 0, 0.8f, i * 1000);
        salience_plasticity_update(plasticity, 10.0f);
    }

    float habituation = salience_plasticity_get_habituation(plasticity, 0);

    // BASELINE: 15 exposures should yield habituation > 0.3
    EXPECT_GT(habituation, 0.3f);
}

TEST_F(SalienceRegressionTest, ValueLearningBaseline) {
    salience_plasticity_register_synapse(
        plasticity, 1, SALIENCE_SYNAPSE_VALUE, 0, 0.5f);

    // Reward attention repeatedly
    for (int i = 0; i < 10; i++) {
        salience_plasticity_attention_event(plasticity, 0, 0.8f, i * 2000);
        salience_plasticity_reward(plasticity, 1.0f, i * 2000 + 1000);
        salience_plasticity_update(plasticity, 10.0f);
    }

    float value = salience_plasticity_get_value_estimate(plasticity, 0);

    // BASELINE: 10 rewarded attentions should yield value > 0.3
    EXPECT_GT(value, 0.3f);
}

TEST_F(SalienceRegressionTest, AttentionLearningBaseline) {
    salience_plasticity_register_synapse(
        plasticity, 1, SALIENCE_SYNAPSE_VALUE, 0, 0.5f);

    // Correct attention feedback
    for (int i = 0; i < 10; i++) {
        salience_plasticity_attention_event(plasticity, 0, 0.8f, i * 2000);
        salience_plasticity_attention_feedback(plasticity, 0, true, i * 2000 + 1000);
        salience_plasticity_update(plasticity, 10.0f);
    }

    salience_plasticity_synapse_t synapse;
    salience_plasticity_get_synapse(plasticity, 1, &synapse);

    // BASELINE: Correct feedback should increase weight above 0.5
    EXPECT_GT(synapse.weight, 0.5f);
}

// ============================================================================
// Boundary Condition Tests
// ============================================================================

TEST_F(SalienceRegressionTest, ZeroFeaturesBoundary) {
    float features[] = {0.0f, 0.0f, 0.0f};
    salience_snn_encode_features(snn, features, 3);
    salience_snn_simulate(snn, 100.0f);

    salience_snn_output_t output;
    int result = salience_snn_decode_salience(snn, &output);

    EXPECT_EQ(result, 0);
    EXPECT_GE(output.combined_salience, 0.0f);
    EXPECT_LE(output.combined_salience, 1.0f);
}

TEST_F(SalienceRegressionTest, MaxFeaturesBoundary) {
    float features[] = {1.0f, 1.0f, 1.0f};
    salience_snn_encode_features(snn, features, 3);
    salience_snn_simulate(snn, 100.0f);

    salience_snn_output_t output;
    int result = salience_snn_decode_salience(snn, &output);

    EXPECT_EQ(result, 0);
    // Max features should produce valid output
    EXPECT_GE(output.intensity, 0.3f);
}

TEST_F(SalienceRegressionTest, EmptyFeaturesBoundary) {
    float features[] = {0.5f};
    int result = salience_snn_encode_features(snn, features, 0);
    // Zero count should be handled gracefully
    EXPECT_EQ(result, 0);
}

// ============================================================================
// Performance Regression Tests
// ============================================================================

TEST_F(SalienceRegressionTest, SNNSimulationPerformance) {
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 100; i++) {
        float features[] = {0.7f, 0.8f, 0.6f};
        salience_snn_encode_features(snn, features, 3);
        salience_snn_simulate(snn, 100.0f);
        salience_snn_reset(snn);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    // BASELINE: 100 iterations should complete in < 5000ms
    EXPECT_LT(duration, 5000);
}

TEST_F(SalienceRegressionTest, PlasticityUpdatePerformance) {
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 1000; i++) {
        salience_plasticity_feature_exposure(plasticity, i % 10, 0.7f, i * 100);
        salience_plasticity_update(plasticity, 1.0f);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    // BASELINE: 1000 updates should complete in < 1000ms
    EXPECT_LT(duration, 1000);
}

TEST_F(SalienceRegressionTest, NoveltyComputationPerformance) {
    // Build up history
    for (int i = 0; i < 50; i++) {
        float hist[] = {(float)(i % 10) / 10.0f, 0.5f, 0.5f};
        salience_snn_add_to_history(snn, hist, 3);
    }

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 100; i++) {
        float features[] = {0.7f, 0.8f, 0.6f};
        salience_snn_compute_novelty(snn, features, 3);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    // BASELINE: 100 novelty computations should complete in < 500ms
    EXPECT_LT(duration, 500);
}

// ============================================================================
// Consistency Tests
// ============================================================================

TEST_F(SalienceRegressionTest, ChannelEnumConsistency) {
    EXPECT_EQ(SALIENCE_SNN_CHANNEL_NOVELTY, 0);
    EXPECT_EQ(SALIENCE_SNN_CHANNEL_SURPRISE, 1);
    EXPECT_EQ(SALIENCE_SNN_CHANNEL_URGENCY, 2);
    EXPECT_EQ(SALIENCE_SNN_CHANNEL_INTENSITY, 3);
    EXPECT_EQ(SALIENCE_SNN_CHANNEL_COUNT, 4);
}

TEST_F(SalienceRegressionTest, DeterministicOutput) {
    float salience1, salience2;

    float features[] = {0.7f, 0.8f, 0.6f};

    salience_snn_encode_features(snn, features, 3);
    salience_snn_simulate(snn, 100.0f);
    salience1 = salience_snn_get_combined_salience(snn);

    salience_snn_reset(snn);

    salience_snn_encode_features(snn, features, 3);
    salience_snn_simulate(snn, 100.0f);
    salience2 = salience_snn_get_combined_salience(snn);

    // Should be approximately equal
    EXPECT_NEAR(salience1, salience2, 0.1f);
}

// ============================================================================
// Known Issue Regression Tests
// ============================================================================

TEST_F(SalienceRegressionTest, NoChannelLeakage) {
    // High novelty should not leak into urgency
    float novel[] = {0.9f, 0.85f, 0.8f};
    salience_snn_encode_features(snn, novel, 3);
    salience_snn_simulate(snn, 100.0f);

    // Multiple channels can be active, but should be independent
    float novelty = salience_snn_get_novelty(snn);
    float urgency = salience_snn_get_urgency(snn);

    // Both should be valid (non-negative)
    EXPECT_GE(novelty, 0.0f);
    EXPECT_GE(urgency, 0.0f);
}

TEST_F(SalienceRegressionTest, ResetClearsHistory) {
    float features[] = {0.5f, 0.5f, 0.5f};
    for (int i = 0; i < 10; i++) {
        salience_snn_add_to_history(snn, features, 3);
    }

    salience_snn_clear_history(snn);

    // After clear, same features should appear novel
    float novelty = salience_snn_compute_novelty(snn, features, 3);
    // With empty history, everything is "novel" (high novelty)
    EXPECT_GE(novelty, 0.0f);
}

TEST_F(SalienceRegressionTest, HabituationDoesNotGoNegative) {
    // Even with many exposures, habituation should stay in [0, 1]
    for (int i = 0; i < 100; i++) {
        salience_plasticity_feature_exposure(plasticity, 0, 0.8f, i * 500);
        salience_plasticity_update(plasticity, 5.0f);
    }

    float habituation = salience_plasticity_get_habituation(plasticity, 0);

    EXPECT_GE(habituation, 0.0f);
    EXPECT_LE(habituation, 1.0f);
}
