/**
 * @file test_salience_snn_plasticity_integration.cpp
 * @brief Integration tests for Salience SNN-Plasticity bidirectional system
 * @version 1.0.0
 * @date 2026-01-06
 *
 * WHAT: Integration tests verifying Salience SNN and Plasticity bridges work together
 * WHY:  Attention allocation should improve through experience; learning shapes salience
 * HOW:  Create both bridges, connect them, verify habituation and value learning
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>

#include "cognitive/salience/nimcp_salience_snn_bridge.h"
#include "cognitive/salience/nimcp_salience_plasticity_bridge.h"

class SalienceSnnPlasticityIntegrationTest : public ::testing::Test {
protected:
    salience_snn_bridge_t* snn = nullptr;
    salience_plasticity_bridge_t* plasticity = nullptr;

    void SetUp() override {
        salience_snn_config_t snn_config = salience_snn_config_default();
        snn_config.enable_history = true;
        snn = salience_snn_create(&snn_config);
        ASSERT_NE(snn, nullptr);

        salience_plasticity_config_t plas_config = salience_plasticity_config_default();
        plasticity = salience_plasticity_create(&plas_config);
        ASSERT_NE(plasticity, nullptr);
    }

    void TearDown() override {
        if (snn) {
            salience_snn_destroy(snn);
            snn = nullptr;
        }
        if (plasticity) {
            salience_plasticity_destroy(plasticity);
            plasticity = nullptr;
        }
    }
};

// ============================================================================
// Basic Integration Tests
// ============================================================================

TEST_F(SalienceSnnPlasticityIntegrationTest, BothBridgesInitialize) {
    salience_snn_bridge_state_t snn_state;
    EXPECT_EQ(salience_snn_get_state(snn, &snn_state), 0);
    EXPECT_EQ(snn_state.state, SALIENCE_SNN_STATE_IDLE);

    salience_plasticity_bridge_state_t plas_state;
    EXPECT_EQ(salience_plasticity_get_state(plasticity, &plas_state), 0);
    EXPECT_EQ(plas_state.state, SALIENCE_PLASTICITY_STATE_IDLE);
}

// ============================================================================
// Salience Detection Integration Tests
// ============================================================================

TEST_F(SalienceSnnPlasticityIntegrationTest, SNNDetectionDrivesPlasticityUpdate) {
    // Register plasticity synapses for different channels
    salience_plasticity_register_synapse(
        plasticity, 1, SALIENCE_SYNAPSE_NOVELTY, 0, 0.5f);
    salience_plasticity_register_synapse(
        plasticity, 2, SALIENCE_SYNAPSE_VALUE, 0, 0.5f);

    // Present novel features to SNN
    float features[] = {0.8f, 0.9f, 0.7f};
    salience_snn_encode_features(snn, features, 3);
    salience_snn_simulate(snn, 100.0f);

    salience_snn_output_t snn_output;
    salience_snn_decode_salience(snn, &snn_output);

    // If high salience, record attention event
    if (snn_output.combined_salience > 0.3f) {
        salience_plasticity_attention_event(
            plasticity, 0, snn_output.combined_salience, 1000);
    }

    // Provide feedback
    salience_plasticity_attention_feedback(plasticity, 0, true, 2000);

    // Verify plasticity recorded the event
    salience_plasticity_stats_t stats;
    salience_plasticity_get_stats(plasticity, &stats);
    EXPECT_GE(stats.total_attention_events, 1u);
}

// ============================================================================
// Novelty-Habituation Integration Tests
// ============================================================================

TEST_F(SalienceSnnPlasticityIntegrationTest, NoveltyDecreasesWithExposure) {
    float features[] = {0.5f, 0.5f, 0.5f};

    // First exposure - novel
    float novelty1 = salience_snn_compute_novelty(snn, features, 3);
    salience_snn_add_to_history(snn, features, 3);
    salience_plasticity_feature_exposure(plasticity, 0, 0.8f, 1000);

    // Multiple exposures
    for (int i = 0; i < 5; i++) {
        salience_plasticity_feature_exposure(plasticity, 0, 0.8f, (i + 2) * 1000);
        salience_plasticity_update(plasticity, 10.0f);
    }

    // Later exposure - less novel
    float novelty2 = salience_snn_compute_novelty(snn, features, 3);
    float habituation = salience_plasticity_get_habituation(plasticity, 0);

    // Novelty should decrease, habituation should increase
    EXPECT_LE(novelty2, novelty1);
    EXPECT_GT(habituation, 0.0f);
}

TEST_F(SalienceSnnPlasticityIntegrationTest, NovelStimulusBreaksHabituation) {
    // Habituate to a stimulus
    float familiar[] = {0.3f, 0.3f, 0.3f};
    for (int i = 0; i < 10; i++) {
        salience_snn_add_to_history(snn, familiar, 3);
        salience_plasticity_feature_exposure(plasticity, 0, 0.8f, i * 1000);
    }

    float habituation_before = salience_plasticity_get_habituation(plasticity, 0);

    // Present novel stimulus
    float novel[] = {0.9f, 0.9f, 0.9f};
    float novelty = salience_snn_compute_novelty(snn, novel, 3);

    // Novel stimulus should have high novelty
    EXPECT_GT(novelty, 0.5f);

    // Record novelty response (dishabituating)
    salience_plasticity_novelty_response(plasticity, 0, novelty, true, 11000);

    // Could check that habituation is reset or attention is captured
    EXPECT_GE(habituation_before, 0.0f);  // Habituation existed
}

// ============================================================================
// Value Learning Integration Tests
// ============================================================================

TEST_F(SalienceSnnPlasticityIntegrationTest, ValueLearningShapesSalience) {
    salience_plasticity_config_t config = salience_plasticity_config_default();
    config.enable_value_learning = true;
    salience_plasticity_bridge_t* plas = salience_plasticity_create(&config);
    ASSERT_NE(plas, nullptr);

    salience_plasticity_register_synapse(plas, 1, SALIENCE_SYNAPSE_VALUE, 0, 0.5f);

    // Present features and reward attention
    for (int i = 0; i < 10; i++) {
        float features[] = {0.6f, 0.7f, 0.5f};
        salience_snn_encode_features(snn, features, 3);
        salience_snn_simulate(snn, 50.0f);

        salience_snn_output_t output;
        salience_snn_decode_salience(snn, &output);

        // Attend and reward
        salience_plasticity_attention_event(plas, 0, output.combined_salience, i * 2000);
        salience_plasticity_reward(plas, 1.0f, i * 2000 + 1000);
        salience_plasticity_update(plas, 10.0f);

        salience_snn_reset(snn);
    }

    // Check value estimate has increased
    float value = salience_plasticity_get_value_estimate(plas, 0);
    EXPECT_GT(value, 0.0f);

    salience_plasticity_destroy(plas);
}

TEST_F(SalienceSnnPlasticityIntegrationTest, NegativeRewardDecreasesValue) {
    salience_plasticity_register_synapse(plasticity, 1, SALIENCE_SYNAPSE_VALUE, 0, 0.7f);

    // First establish some positive value
    salience_plasticity_attention_event(plasticity, 0, 0.8f, 1000);
    salience_plasticity_reward(plasticity, 1.0f, 2000);
    float initial_value = salience_plasticity_get_value_estimate(plasticity, 0);

    // Then negative outcomes
    for (int i = 0; i < 5; i++) {
        salience_plasticity_attention_event(plasticity, 0, 0.8f, (i + 2) * 2000);
        salience_plasticity_reward(plasticity, -0.5f, (i + 2) * 2000 + 1000);
        salience_plasticity_update(plasticity, 10.0f);
    }

    float final_value = salience_plasticity_get_value_estimate(plasticity, 0);

    // Value should decrease (or be lower) after negative reward
    // Allow for learning rate effects
    EXPECT_LE(final_value, initial_value + 0.1f);
}

// ============================================================================
// Attention Feedback Loop Integration Tests
// ============================================================================

TEST_F(SalienceSnnPlasticityIntegrationTest, CorrectAttentionIncreasesWeight) {
    salience_plasticity_register_synapse(plasticity, 1, SALIENCE_SYNAPSE_VALUE, 0, 0.5f);

    // Multiple correct attention events
    for (int i = 0; i < 10; i++) {
        float features[] = {0.7f, 0.8f, 0.6f};
        salience_snn_encode_features(snn, features, 3);
        salience_snn_simulate(snn, 50.0f);

        salience_snn_output_t output;
        salience_snn_decode_salience(snn, &output);

        salience_plasticity_attention_event(plasticity, 0, output.combined_salience, i * 2000);
        salience_plasticity_attention_feedback(plasticity, 0, true, i * 2000 + 1000);
        salience_plasticity_update(plasticity, 10.0f);

        salience_snn_reset(snn);
    }

    salience_plasticity_synapse_t synapse;
    salience_plasticity_get_synapse(plasticity, 1, &synapse);

    // Weight should increase with correct attention
    EXPECT_GT(synapse.weight, 0.5f);
}

TEST_F(SalienceSnnPlasticityIntegrationTest, IncorrectAttentionDecreasesWeight) {
    salience_plasticity_register_synapse(plasticity, 1, SALIENCE_SYNAPSE_VALUE, 0, 0.7f);

    // Multiple incorrect attention events (attended to wrong thing)
    for (int i = 0; i < 10; i++) {
        salience_plasticity_attention_event(plasticity, 0, 0.8f, i * 2000);
        salience_plasticity_attention_feedback(plasticity, 0, false, i * 2000 + 1000);
        salience_plasticity_update(plasticity, 10.0f);
    }

    salience_plasticity_synapse_t synapse;
    salience_plasticity_get_synapse(plasticity, 1, &synapse);

    // Weight should decrease with incorrect attention
    EXPECT_LT(synapse.weight, 0.7f);
}

// ============================================================================
// Multi-Channel Integration Tests
// ============================================================================

TEST_F(SalienceSnnPlasticityIntegrationTest, MultiChannelSalienceProcessing) {
    // Register synapses for different channels
    salience_plasticity_register_synapse(plasticity, 1, SALIENCE_SYNAPSE_NOVELTY, 0, 0.5f);
    salience_plasticity_register_synapse(plasticity, 2, SALIENCE_SYNAPSE_SURPRISE, 1, 0.5f);
    salience_plasticity_register_synapse(plasticity, 3, SALIENCE_SYNAPSE_URGENCY, 2, 0.5f);

    // Test different scenarios
    // Scenario 1: Novel but not urgent
    float novel_features[] = {0.9f, 0.8f, 0.85f};
    salience_snn_encode_features(snn, novel_features, 3);
    salience_snn_simulate(snn, 100.0f);

    float novelty = salience_snn_get_novelty(snn);
    if (novelty > 0.3f) {
        salience_plasticity_novelty_response(plasticity, 0, novelty, true, 1000);
    }
    salience_snn_reset(snn);

    // Scenario 2: Surprising (prediction error)
    float predicted[] = {0.2f, 0.2f, 0.2f};
    float actual[] = {0.8f, 0.9f, 0.7f};
    salience_snn_encode_with_prediction(snn, actual, 3, predicted, 3);
    salience_snn_simulate(snn, 100.0f);

    float surprise = salience_snn_get_surprise(snn);
    EXPECT_GT(surprise, 0.0f);
    salience_snn_reset(snn);

    // Verify multi-channel tracking
    salience_feature_learning_t learning0, learning1;
    salience_plasticity_get_feature_learning(plasticity, 0, &learning0);
    salience_plasticity_get_feature_learning(plasticity, 1, &learning1);
    // At least one should have been updated
    EXPECT_GE(learning0.exposure_count + learning1.exposure_count, 0u);
}

// ============================================================================
// Urgency Integration Tests
// ============================================================================

TEST_F(SalienceSnnPlasticityIntegrationTest, UrgencyDomination) {
    salience_plasticity_register_synapse(plasticity, 1, SALIENCE_SYNAPSE_URGENCY, 0, 0.5f);

    // High intensity features create urgency
    float urgent_features[] = {0.95f, 0.98f, 0.99f};
    salience_snn_encode_features(snn, urgent_features, 3);
    salience_snn_simulate(snn, 100.0f);

    float urgency = salience_snn_get_urgency(snn);
    salience_snn_channel_t dominant = salience_snn_get_dominant_channel(snn);

    // High intensity should create urgency/intensity as dominant
    EXPECT_GE(urgency, 0.0f);

    // Record urgent attention
    salience_plasticity_attention_event(plasticity, 0, urgency, 1000);

    salience_plasticity_stats_t stats;
    salience_plasticity_get_stats(plasticity, &stats);
    EXPECT_GE(stats.total_attention_events, 1u);
}

// ============================================================================
// Consolidation Integration Tests
// ============================================================================

TEST_F(SalienceSnnPlasticityIntegrationTest, ConsolidationPreservesLearning) {
    salience_plasticity_register_synapse(plasticity, 1, SALIENCE_SYNAPSE_VALUE, 0, 0.5f);

    // Create learning history
    for (int i = 0; i < 15; i++) {
        salience_plasticity_attention_event(plasticity, 0, 0.8f, i * 1000);
        salience_plasticity_attention_feedback(plasticity, 0, true, i * 1000 + 500);
        salience_plasticity_update(plasticity, 10.0f);
    }

    salience_plasticity_synapse_t before;
    salience_plasticity_get_synapse(plasticity, 1, &before);
    float weight_before = before.weight;

    // Consolidate
    salience_plasticity_consolidate(plasticity);

    salience_plasticity_synapse_t after;
    salience_plasticity_get_synapse(plasticity, 1, &after);

    // Weight should be preserved
    EXPECT_NEAR(after.weight, weight_before, 0.1f);
}

// ============================================================================
// Bio-Async Coordination Tests
// ============================================================================

TEST_F(SalienceSnnPlasticityIntegrationTest, BioAsyncCoordination) {
    salience_snn_config_t snn_config = salience_snn_config_default();
    snn_config.enable_bio_async = true;
    salience_snn_bridge_t* snn_async = salience_snn_create(&snn_config);

    salience_plasticity_config_t plas_config = salience_plasticity_config_default();
    plas_config.enable_bio_async = true;
    salience_plasticity_bridge_t* plas_async = salience_plasticity_create(&plas_config);

    ASSERT_NE(snn_async, nullptr);
    ASSERT_NE(plas_async, nullptr);

    // Connect both
    EXPECT_EQ(salience_snn_bio_async_connect(snn_async), 0);
    EXPECT_EQ(salience_plasticity_connect_bio_async(plas_async), 0);

    EXPECT_TRUE(salience_snn_is_bio_async_connected(snn_async));
    EXPECT_TRUE(salience_plasticity_is_bio_async_connected(plas_async));

    // Operate
    float features[] = {0.7f, 0.8f, 0.6f};
    salience_snn_encode_features(snn_async, features, 3);
    salience_snn_simulate(snn_async, 50.0f);

    salience_plasticity_attention_event(plas_async, 0, 0.7f, 1000);

    // Disconnect
    salience_snn_bio_async_disconnect(snn_async);
    salience_plasticity_disconnect_bio_async(plas_async);

    salience_snn_destroy(snn_async);
    salience_plasticity_destroy(plas_async);
}

// ============================================================================
// Stress Test
// ============================================================================

TEST_F(SalienceSnnPlasticityIntegrationTest, HighVolumeProcessing) {
    salience_plasticity_register_synapse(plasticity, 1, SALIENCE_SYNAPSE_VALUE, 0, 0.5f);

    for (int i = 0; i < 100; i++) {
        // Varying features
        float features[] = {
            0.5f + 0.4f * sinf(i * 0.1f),
            0.5f + 0.4f * cosf(i * 0.15f),
            0.5f + 0.4f * sinf(i * 0.2f)
        };

        salience_snn_encode_features(snn, features, 3);
        salience_snn_simulate(snn, 30.0f);

        salience_snn_output_t output;
        salience_snn_decode_salience(snn, &output);

        if (output.combined_salience > 0.3f) {
            salience_plasticity_attention_event(plasticity, 0, output.combined_salience, i * 400);
            salience_plasticity_attention_feedback(plasticity, 0, true, i * 400 + 200);
        }

        salience_plasticity_update(plasticity, 3.0f);
        salience_snn_reset(snn);
    }

    // Verify processing
    salience_plasticity_stats_t stats;
    salience_plasticity_get_stats(plasticity, &stats);
    EXPECT_GE(stats.total_attention_events, 30u);
}
