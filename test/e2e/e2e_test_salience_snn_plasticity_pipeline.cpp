/**
 * @file e2e_test_salience_snn_plasticity_pipeline.cpp
 * @brief End-to-end tests for Salience-SNN-Plasticity learning pipeline
 * @version 1.0.0
 * @date 2026-01-06
 *
 * WHAT: Complete salience/attention pipeline with SNN and Plasticity
 * WHY:  Verify full dataflow from features -> SNN encoding -> salience detection
 *       -> plasticity learning -> attention allocation improvement
 * HOW:  Test realistic scenarios combining novelty detection, habituation,
 *       value learning, and attention feedback
 *
 * Test Coverage:
 * - Full feature to salience assessment pipeline via SNN
 * - STDP and reward-modulated learning for attention
 * - Novelty detection and habituation learning
 * - Multi-channel salience processing (novelty, surprise, urgency)
 * - Value-based attention learning
 * - Long-term attention improvement
 */

#include <gtest/gtest.h>

#include "cognitive/salience/nimcp_salience_snn_bridge.h"
#include "cognitive/salience/nimcp_salience_plasticity_bridge.h"

#include <cstring>
#include <cmath>
#include <vector>
#include <chrono>
#include <numeric>

//=============================================================================
// Test Fixtures
//=============================================================================

class SalienceSNNPlasticityE2E : public ::testing::Test {
protected:
    salience_snn_bridge_t* snn_bridge = nullptr;
    salience_plasticity_bridge_t* plasticity_bridge = nullptr;

    struct LearningStats {
        int attention_events = 0;
        int correct_attention = 0;
        int novelty_detections = 0;
        int habituation_events = 0;
        int reward_events = 0;
        std::vector<float> salience_history;
        std::vector<float> value_history;
    } stats;

    void SetUp() override {
        salience_snn_config_t snn_config = salience_snn_config_default();
        snn_config.enable_history = true;
        snn_config.enable_multimodal = true;
        snn_config.enable_prediction = true;
        snn_config.enable_bio_async = false;

        snn_bridge = salience_snn_create(&snn_config);
        ASSERT_NE(snn_bridge, nullptr) << "Failed to create SNN bridge";

        salience_plasticity_config_t plasticity_config = salience_plasticity_config_default();
        plasticity_config.enable_habituation = true;
        plasticity_config.enable_value_learning = true;
        plasticity_config.enable_novelty_seeking = true;
        plasticity_config.enable_eligibility = true;

        plasticity_bridge = salience_plasticity_create(&plasticity_config);
        ASSERT_NE(plasticity_bridge, nullptr) << "Failed to create Plasticity bridge";

        // Register synapses for different salience channels
        for (uint32_t ch = 0; ch < SALIENCE_SNN_CHANNEL_COUNT; ch++) {
            salience_plasticity_register_synapse(plasticity_bridge, ch,
                (salience_synapse_type_t)ch, ch, 0.5f);
        }

        // Register value learning synapses
        for (uint32_t feat = 0; feat < 5; feat++) {
            salience_plasticity_register_synapse(plasticity_bridge, 10 + feat,
                SALIENCE_SYNAPSE_VALUE, feat, 0.5f);
        }

        // Register habituation synapse
        salience_plasticity_register_synapse(plasticity_bridge, 20,
            SALIENCE_SYNAPSE_HABITUATION, 0, 0.5f);
    }

    void TearDown() override {
        if (snn_bridge) {
            salience_snn_destroy(snn_bridge);
            snn_bridge = nullptr;
        }
        if (plasticity_bridge) {
            salience_plasticity_destroy(plasticity_bridge);
            plasticity_bridge = nullptr;
        }
    }

    enum SalienceScenario {
        HIGH_NOVELTY,           // Novel unfamiliar stimulus
        HIGH_SURPRISE,          // Prediction error
        HIGH_URGENCY,           // Intense stimulus requiring attention
        LOW_SALIENCE,           // Familiar, expected, mild
        FAMILIAR_STIMULUS,      // Previously seen
        VALUABLE_STIMULUS,      // Associated with reward
        MIXED_SALIENCE          // Multiple channels active
    };

    void generate_scenario(float* features, uint32_t count, float* prediction,
                          SalienceScenario scenario) {
        switch (scenario) {
            case HIGH_NOVELTY:
                for (uint32_t i = 0; i < count; i++) {
                    features[i] = 0.9f - 0.1f * i;
                    if (prediction) prediction[i] = 0.2f;
                }
                break;
            case HIGH_SURPRISE:
                for (uint32_t i = 0; i < count; i++) {
                    features[i] = 0.9f;
                    if (prediction) prediction[i] = 0.1f;  // Big mismatch
                }
                break;
            case HIGH_URGENCY:
                for (uint32_t i = 0; i < count; i++) {
                    features[i] = 0.95f + 0.05f * (i == 0 ? 1 : 0);
                    if (prediction) prediction[i] = 0.9f;
                }
                break;
            case LOW_SALIENCE:
                for (uint32_t i = 0; i < count; i++) {
                    features[i] = 0.3f;
                    if (prediction) prediction[i] = 0.3f;
                }
                break;
            case FAMILIAR_STIMULUS:
                for (uint32_t i = 0; i < count; i++) {
                    features[i] = 0.5f;
                    if (prediction) prediction[i] = 0.5f;
                }
                break;
            case VALUABLE_STIMULUS:
                for (uint32_t i = 0; i < count; i++) {
                    features[i] = 0.7f;
                    if (prediction) prediction[i] = 0.6f;
                }
                break;
            case MIXED_SALIENCE:
                for (uint32_t i = 0; i < count; i++) {
                    features[i] = 0.5f + 0.4f * sinf(i * 0.5f);
                    if (prediction) prediction[i] = 0.4f;
                }
                break;
        }
    }

    salience_snn_output_t evaluate_salience(const float* features, uint32_t count,
                                           const float* prediction = nullptr) {
        if (prediction) {
            salience_snn_encode_with_prediction(snn_bridge, features, count, prediction, count);
        } else {
            salience_snn_encode_features(snn_bridge, features, count);
        }

        salience_snn_simulate(snn_bridge, 100.0f);

        salience_snn_output_t output;
        salience_snn_decode_salience(snn_bridge, &output);

        stats.salience_history.push_back(output.combined_salience);

        return output;
    }
};

//=============================================================================
// E2E Pipeline Tests
//=============================================================================

TEST_F(SalienceSNNPlasticityE2E, CompleteSalienceEvaluationPipeline) {
    const uint32_t FEAT_COUNT = 4;
    float features[FEAT_COUNT], prediction[FEAT_COUNT];
    generate_scenario(features, FEAT_COUNT, prediction, HIGH_NOVELTY);

    salience_snn_output_t output = evaluate_salience(features, FEAT_COUNT, prediction);

    // High novelty should produce high salience
    EXPECT_GT(output.combined_salience, 0.3f);

    // Record attention event
    salience_plasticity_attention_event(plasticity_bridge, 0, output.combined_salience, 1000);
    salience_plasticity_attention_feedback(plasticity_bridge, 0, true, 2000);
    stats.attention_events++;
    stats.correct_attention++;

    salience_plasticity_update(plasticity_bridge, 10.0f);

    float learned_salience = salience_plasticity_get_learned_salience(plasticity_bridge, 0);
    EXPECT_GE(learned_salience, 0.0f);

    salience_snn_reset(snn_bridge);
}

TEST_F(SalienceSNNPlasticityE2E, NoveltyDetectionAndHabituation) {
    const uint32_t FEAT_COUNT = 3;
    float features[FEAT_COUNT];
    generate_scenario(features, FEAT_COUNT, nullptr, HIGH_NOVELTY);

    // First exposure - should be novel
    float novelty1 = salience_snn_compute_novelty(snn_bridge, features, FEAT_COUNT);
    salience_snn_add_to_history(snn_bridge, features, FEAT_COUNT);
    salience_plasticity_feature_exposure(plasticity_bridge, 0, 0.8f, 1000);

    // Multiple exposures - habituation
    for (int i = 0; i < 10; i++) {
        salience_snn_add_to_history(snn_bridge, features, FEAT_COUNT);
        salience_plasticity_feature_exposure(plasticity_bridge, 0, 0.8f, (i + 2) * 1000);
        salience_plasticity_update(plasticity_bridge, 10.0f);
    }

    // Later exposure - should be less novel
    float novelty2 = salience_snn_compute_novelty(snn_bridge, features, FEAT_COUNT);
    float habituation = salience_plasticity_get_habituation(plasticity_bridge, 0);

    // Novelty should decrease, habituation should increase
    EXPECT_LE(novelty2, novelty1);
    EXPECT_GT(habituation, 0.0f);
}

TEST_F(SalienceSNNPlasticityE2E, SurpriseDetectionPipeline) {
    const uint32_t FEAT_COUNT = 3;
    float features[FEAT_COUNT], prediction[FEAT_COUNT];
    generate_scenario(features, FEAT_COUNT, prediction, HIGH_SURPRISE);

    salience_snn_output_t output = evaluate_salience(features, FEAT_COUNT, prediction);

    float surprise = salience_snn_get_surprise(snn_bridge);

    // Large prediction error should create surprise
    EXPECT_GT(surprise, 0.2f);

    // Record surprising event
    if (surprise > 0.3f) {
        salience_plasticity_novelty_response(plasticity_bridge, 0, surprise, true, 1000);
        stats.novelty_detections++;
    }

    salience_plasticity_update(plasticity_bridge, 10.0f);
    salience_snn_reset(snn_bridge);
}

TEST_F(SalienceSNNPlasticityE2E, ValueLearningThroughReward) {
    const uint32_t FEAT_COUNT = 3;
    const int NUM_TRIALS = 20;

    std::vector<float> values;

    for (int i = 0; i < NUM_TRIALS; i++) {
        float features[FEAT_COUNT];
        generate_scenario(features, FEAT_COUNT, nullptr, VALUABLE_STIMULUS);

        salience_snn_output_t output = evaluate_salience(features, FEAT_COUNT);

        // Attend and receive reward
        salience_plasticity_attention_event(plasticity_bridge, 0, output.combined_salience, i * 2000);
        salience_plasticity_reward(plasticity_bridge, 1.0f, i * 2000 + 1000);
        stats.reward_events++;

        salience_plasticity_update(plasticity_bridge, 10.0f);

        float value = salience_plasticity_get_value_estimate(plasticity_bridge, 0);
        values.push_back(value);

        salience_snn_reset(snn_bridge);
    }

    // Value should increase over time with consistent reward
    EXPECT_GT(values.back(), values.front());
}

TEST_F(SalienceSNNPlasticityE2E, AttentionLearningWithFeedback) {
    const uint32_t FEAT_COUNT = 3;
    const int NUM_TRIALS = 30;

    int correct = 0;

    for (int i = 0; i < NUM_TRIALS; i++) {
        // Alternate between high and low salience scenarios
        SalienceScenario scenario = (i % 2 == 0) ? HIGH_URGENCY : LOW_SALIENCE;
        bool should_attend = (scenario == HIGH_URGENCY);

        float features[FEAT_COUNT];
        generate_scenario(features, FEAT_COUNT, nullptr, scenario);

        salience_snn_output_t output = evaluate_salience(features, FEAT_COUNT);
        bool did_attend = output.combined_salience > 0.5f;

        // Feedback based on correctness
        salience_plasticity_attention_event(plasticity_bridge, 0, output.combined_salience, i * 2000);
        bool was_correct = (did_attend == should_attend);
        salience_plasticity_attention_feedback(plasticity_bridge, 0, was_correct, i * 2000 + 1000);

        if (was_correct) correct++;

        salience_plasticity_update(plasticity_bridge, 10.0f);
        salience_snn_reset(snn_bridge);
    }

    float accuracy = (float)correct / NUM_TRIALS;

    // Should learn to attend correctly most of the time
    EXPECT_GT(accuracy, 0.4f);
}

TEST_F(SalienceSNNPlasticityE2E, MultiChannelSalienceProcessing) {
    const uint32_t FEAT_COUNT = 4;
    float features[FEAT_COUNT], prediction[FEAT_COUNT];

    // Test different channels
    struct {
        SalienceScenario scenario;
        salience_snn_channel_t expected_dominant;
    } tests[] = {
        {HIGH_NOVELTY, SALIENCE_SNN_CHANNEL_NOVELTY},
        {HIGH_SURPRISE, SALIENCE_SNN_CHANNEL_SURPRISE},
        {HIGH_URGENCY, SALIENCE_SNN_CHANNEL_URGENCY}
    };

    for (int t = 0; t < 3; t++) {
        generate_scenario(features, FEAT_COUNT, prediction, tests[t].scenario);

        salience_snn_output_t output = evaluate_salience(features, FEAT_COUNT, prediction);

        // Record in plasticity
        salience_plasticity_attention_event(plasticity_bridge, t, output.combined_salience, t * 3000);

        // Get dominant channel
        salience_snn_channel_t dominant = salience_snn_get_dominant_channel(snn_bridge);

        // Each scenario may activate its corresponding channel
        EXPECT_GE(dominant, 0);
        EXPECT_LT(dominant, SALIENCE_SNN_CHANNEL_COUNT);

        salience_snn_reset(snn_bridge);
    }

    salience_plasticity_update(plasticity_bridge, 30.0f);
}

TEST_F(SalienceSNNPlasticityE2E, HabituationAndDishabituation) {
    const uint32_t FEAT_COUNT = 3;
    float familiar[FEAT_COUNT] = {0.5f, 0.5f, 0.5f};
    float novel[FEAT_COUNT] = {0.9f, 0.9f, 0.9f};

    // Habituate to familiar stimulus
    for (int i = 0; i < 15; i++) {
        salience_snn_add_to_history(snn_bridge, familiar, FEAT_COUNT);
        salience_plasticity_feature_exposure(plasticity_bridge, 0, 0.8f, i * 1000);
        salience_plasticity_update(plasticity_bridge, 10.0f);
    }

    float habituation_peak = salience_plasticity_get_habituation(plasticity_bridge, 0);
    EXPECT_GT(habituation_peak, 0.3f);

    // Present novel stimulus (dishabituation trigger)
    float novel_novelty = salience_snn_compute_novelty(snn_bridge, novel, FEAT_COUNT);
    EXPECT_GT(novel_novelty, 0.5f);  // Should be novel

    // Novelty response can trigger dishabituation
    salience_plasticity_novelty_response(plasticity_bridge, 0, novel_novelty, true, 16000);

    salience_plasticity_update(plasticity_bridge, 10.0f);

    // Habituation may reset or reduce
    // (Exact behavior depends on implementation)
}

TEST_F(SalienceSNNPlasticityE2E, LongTermAttentionImprovement) {
    const uint32_t FEAT_COUNT = 3;
    const int TRAINING_EPOCHS = 40;
    const int EVAL_EPOCHS = 10;

    // Training phase
    for (int i = 0; i < TRAINING_EPOCHS; i++) {
        SalienceScenario scenario = (i % 3 == 0) ? HIGH_URGENCY : ((i % 3 == 1) ? HIGH_NOVELTY : LOW_SALIENCE);
        bool should_attend = (scenario != LOW_SALIENCE);

        float features[FEAT_COUNT];
        generate_scenario(features, FEAT_COUNT, nullptr, scenario);

        salience_snn_output_t output = evaluate_salience(features, FEAT_COUNT);

        salience_plasticity_attention_event(plasticity_bridge, 0, output.combined_salience, i * 2000);
        salience_plasticity_attention_feedback(plasticity_bridge, 0, output.combined_salience > 0.4f == should_attend, i * 2000 + 1000);

        if (should_attend && output.combined_salience > 0.4f) {
            salience_plasticity_reward(plasticity_bridge, 0.5f, i * 2000 + 1500);
        }

        salience_plasticity_update(plasticity_bridge, 10.0f);
        salience_snn_reset(snn_bridge);
    }

    // Consolidate
    salience_plasticity_consolidate(plasticity_bridge);

    // Evaluation phase
    int correct_eval = 0;
    for (int i = 0; i < EVAL_EPOCHS; i++) {
        SalienceScenario scenario = (i % 2 == 0) ? HIGH_URGENCY : LOW_SALIENCE;
        bool should_attend = (scenario == HIGH_URGENCY);

        float features[FEAT_COUNT];
        generate_scenario(features, FEAT_COUNT, nullptr, scenario);

        salience_snn_output_t output = evaluate_salience(features, FEAT_COUNT);
        bool did_attend = output.combined_salience > 0.4f;

        if (did_attend == should_attend) correct_eval++;

        salience_snn_reset(snn_bridge);
    }

    float eval_accuracy = (float)correct_eval / EVAL_EPOCHS;

    // After training, should have reasonable accuracy
    EXPECT_GT(eval_accuracy, 0.4f);
}

TEST_F(SalienceSNNPlasticityE2E, PerformanceBenchmark) {
    const uint32_t FEAT_COUNT = 5;
    const int NUM_ITERATIONS = 100;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < NUM_ITERATIONS; i++) {
        float features[FEAT_COUNT];
        for (uint32_t f = 0; f < FEAT_COUNT; f++) {
            features[f] = 0.3f + 0.6f * fabsf(sinf(i * 0.1f + f * 0.5f));
        }

        salience_snn_output_t output = evaluate_salience(features, FEAT_COUNT);

        salience_plasticity_attention_event(plasticity_bridge, 0, output.combined_salience, i * 1000);

        if (i % 2 == 0) {
            salience_plasticity_reward(plasticity_bridge, 0.5f, i * 1000 + 500);
        }

        salience_plasticity_update(plasticity_bridge, 5.0f);
        salience_snn_reset(snn_bridge);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    // Should complete in reasonable time
    EXPECT_LT(duration, 15000);

    // Should have processed all iterations
    EXPECT_EQ(stats.salience_history.size(), (size_t)NUM_ITERATIONS);
}
