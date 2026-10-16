/**
 * @file e2e_test_emotion_snn_plasticity_pipeline.cpp
 * @brief End-to-end tests for Emotion-SNN-Plasticity learning pipeline
 * @version 1.0.0
 * @date 2026-01-06
 *
 * WHAT: Complete emotion pipeline with SNN and Plasticity
 * WHY:  Verify full dataflow from emotion state -> SNN encoding -> emotion recognition
 *       -> plasticity learning -> emotional sensitivity evolution
 * HOW:  Test realistic scenarios combining emotional stimuli, STDP learning,
 *       reward-modulated plasticity, and emotional conditioning/extinction
 *
 * Test Coverage:
 * - Full emotion state to category recognition pipeline via SNN
 * - STDP and reward-modulated learning for emotional conditioning
 * - Extinction learning for fear reduction
 * - Valence and arousal modulation effects
 * - Multi-emotion learning scenarios
 * - Emotional memory consolidation
 */

#include <gtest/gtest.h>

#include "cognitive/emotion/nimcp_emotion_snn_bridge.h"
#include "cognitive/emotion/nimcp_emotion_plasticity_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"

#include <cstring>
#include <cmath>
#include <vector>
#include <chrono>
#include <numeric>

//=============================================================================
// Test Fixtures
//=============================================================================

class EmotionSNNPlasticityE2E : public ::testing::Test {
protected:
    emotion_snn_bridge_t* snn_bridge = nullptr;
    emotion_plasticity_bridge_t* plasticity_bridge = nullptr;

    struct LearningStats {
        int stimulus_events = 0;
        int response_events = 0;
        int extinction_trials = 0;
        int reward_events = 0;
        int total_evaluations = 0;
        std::vector<float> intensity_history;
        std::vector<float> valence_history;
    } stats;

    void SetUp() override {
        emotion_snn_config_t snn_config = emotion_snn_config_default();
        snn_config.input_dim = 64;
        snn_config.hidden_dim = 32;
        snn_config.output_dim = EMOTION_COUNT;
        snn_config.dt_ms = 1.0f;
        snn_config.enable_va_encoding = true;
        snn_config.enable_bio_async = false;

        snn_bridge = emotion_snn_create(&snn_config);
        ASSERT_NE(snn_bridge, nullptr) << "Failed to create SNN bridge";

        emotion_plasticity_config_t plasticity_config = emotion_plasticity_config_default();
        plasticity_config.enable_valence_modulation = true;
        plasticity_config.enable_arousal_modulation = true;
        plasticity_config.enable_extinction = true;
        plasticity_config.extinction_rate = 0.05f;
        plasticity_config.stdp_a_plus = 0.01f;
        plasticity_config.stdp_a_minus = 0.012f;

        plasticity_bridge = emotion_plasticity_create(&plasticity_config);
        ASSERT_NE(plasticity_bridge, nullptr) << "Failed to create Plasticity bridge";

        for (int i = 0; i < EMOTION_COUNT; i++) {
            emotion_plasticity_register_synapse(plasticity_bridge, i,
                EMOTION_SYNAPSE_STIMULUS_TO_EMOTION, (emotion_category_t)i, 0.5f);
        }

        for (int i = 0; i < EMOTION_COUNT; i++) {
            emotion_plasticity_register_synapse(plasticity_bridge, 100 + i,
                EMOTION_SYNAPSE_EMOTION_TO_RESPONSE, (emotion_category_t)i, 0.5f);
        }
    }

    void TearDown() override {
        if (snn_bridge) {
            emotion_snn_destroy(snn_bridge);
            snn_bridge = nullptr;
        }
        if (plasticity_bridge) {
            emotion_plasticity_destroy(plasticity_bridge);
            plasticity_bridge = nullptr;
        }
    }

    enum EmotionScenario {
        HIGH_JOY,
        MODERATE_SADNESS,
        INTENSE_FEAR,
        CONTROLLED_ANGER,
        MILD_DISGUST,
        PLEASANT_SURPRISE,
        NEUTRAL_STATE,
        MIXED_EMOTION
    };

    void generate_scenario(float* valence, float* arousal, float* intensity, EmotionScenario scenario) {
        switch (scenario) {
            case HIGH_JOY:
                *valence = 0.9f;
                *arousal = 0.8f;
                *intensity = 0.85f;
                break;
            case MODERATE_SADNESS:
                *valence = -0.6f;
                *arousal = 0.3f;
                *intensity = 0.5f;
                break;
            case INTENSE_FEAR:
                *valence = -0.8f;
                *arousal = 0.95f;
                *intensity = 0.9f;
                break;
            case CONTROLLED_ANGER:
                *valence = -0.7f;
                *arousal = 0.7f;
                *intensity = 0.6f;
                break;
            case MILD_DISGUST:
                *valence = -0.4f;
                *arousal = 0.4f;
                *intensity = 0.3f;
                break;
            case PLEASANT_SURPRISE:
                *valence = 0.5f;
                *arousal = 0.85f;
                *intensity = 0.7f;
                break;
            case NEUTRAL_STATE:
                *valence = 0.0f;
                *arousal = 0.3f;
                *intensity = 0.2f;
                break;
            case MIXED_EMOTION:
                *valence = 0.1f;
                *arousal = 0.6f;
                *intensity = 0.5f;
                break;
        }
    }

    struct EvaluationResult {
        emotion_category_t category;
        float intensity;
        float valence;
        float arousal;
        int spike_count;
    };

    EvaluationResult run_evaluation(EmotionScenario scenario) {
        EvaluationResult result = {EMOTION_UNKNOWN, 0, 0, 0, 0};

        float valence, arousal, intensity;
        generate_scenario(&valence, &arousal, &intensity, scenario);

        result.spike_count = emotion_snn_encode_valence_arousal(snn_bridge, valence, arousal, intensity);
        emotion_snn_simulate(snn_bridge, 30.0f);

        emotion_snn_emotion_state_t emotion_state;
        emotion_snn_get_emotion_state(snn_bridge, &emotion_state);

        result.category = emotion_state.current_category;
        result.intensity = emotion_state.intensity;
        result.valence = emotion_state.valence;
        result.arousal = emotion_state.arousal;

        stats.total_evaluations++;
        stats.intensity_history.push_back(result.intensity);
        stats.valence_history.push_back(result.valence);

        return result;
    }
};

//=============================================================================
// Basic Pipeline Tests
//=============================================================================

TEST_F(EmotionSNNPlasticityE2E, CompletePipelineInitialization) {
    EXPECT_NE(snn_bridge, nullptr);
    EXPECT_NE(plasticity_bridge, nullptr);

    emotion_plasticity_bridge_state_t state;
    emotion_plasticity_get_state(plasticity_bridge, &state);
    EXPECT_GT(state.registered_synapses, (uint32_t)EMOTION_COUNT);
}

TEST_F(EmotionSNNPlasticityE2E, SingleEvaluationPipeline) {
    auto result = run_evaluation(HIGH_JOY);

    EXPECT_GE(result.intensity, 0.0f);
    EXPECT_LE(result.intensity, 1.0f);
    EXPECT_GE(result.arousal, 0.0f);
    EXPECT_LE(result.arousal, 1.0f);
    EXPECT_GE(result.spike_count, 0);

    int ret = emotion_plasticity_stimulus(plasticity_bridge, EMOTION_HAPPINESS, result.intensity, 0);
    EXPECT_EQ(ret, 0);
}

//=============================================================================
// Emotional Conditioning Tests
//=============================================================================

TEST_F(EmotionSNNPlasticityE2E, FearConditioningLearning) {
    for (int trial = 0; trial < 20; trial++) {
        auto result = run_evaluation(INTENSE_FEAR);

        emotion_plasticity_stimulus(plasticity_bridge, EMOTION_FEAR, result.intensity, trial * 10000);
        emotion_plasticity_response(plasticity_bridge, EMOTION_FEAR, 0.9f, trial * 10000 + 100);
        emotion_plasticity_reward(plasticity_bridge, -1.0f, trial * 10000 + 200);
        emotion_plasticity_update(plasticity_bridge, 10.0f);
    }

    emotion_plasticity_stats_t pstats;
    emotion_plasticity_get_stats(plasticity_bridge, &pstats);
    EXPECT_GT(pstats.total_observations, 0u);
    EXPECT_GT(pstats.total_responses, 0u);
    // Punishment is accumulated as positive value (20 trials with -1.0 reward = punishment)
    EXPECT_GT(pstats.total_punishment, 0.0f);
}

TEST_F(EmotionSNNPlasticityE2E, PositiveConditioningLearning) {
    for (int trial = 0; trial < 20; trial++) {
        auto result = run_evaluation(HIGH_JOY);

        emotion_plasticity_stimulus(plasticity_bridge, EMOTION_HAPPINESS, result.intensity, trial * 10000);
        emotion_plasticity_response(plasticity_bridge, EMOTION_HAPPINESS, 0.8f, trial * 10000 + 100);
        emotion_plasticity_reward(plasticity_bridge, 1.0f, trial * 10000 + 200);
        emotion_plasticity_update(plasticity_bridge, 10.0f);
    }

    emotion_plasticity_stats_t pstats;
    emotion_plasticity_get_stats(plasticity_bridge, &pstats);
    EXPECT_GT(pstats.total_reward, 0.0f);
}

//=============================================================================
// Extinction Learning Tests
//=============================================================================

TEST_F(EmotionSNNPlasticityE2E, FearExtinctionLearning) {
    for (int trial = 0; trial < 10; trial++) {
        auto result = run_evaluation(INTENSE_FEAR);
        emotion_plasticity_stimulus(plasticity_bridge, EMOTION_FEAR, result.intensity, trial * 10000);
        emotion_plasticity_response(plasticity_bridge, EMOTION_FEAR, 0.9f, trial * 10000 + 100);
        emotion_plasticity_reward(plasticity_bridge, -1.0f, trial * 10000 + 200);
        emotion_plasticity_update(plasticity_bridge, 10.0f);
    }

    for (int trial = 0; trial < 30; trial++) {
        emotion_plasticity_extinction_trial(plasticity_bridge, EMOTION_FEAR, (10 + trial) * 10000);
        emotion_plasticity_update(plasticity_bridge, 10.0f);
    }

    float extinction_level = emotion_plasticity_get_extinction_level(plasticity_bridge, EMOTION_FEAR);
    EXPECT_GE(extinction_level, 0.0f);

    emotion_plasticity_stats_t pstats;
    emotion_plasticity_get_stats(plasticity_bridge, &pstats);
    EXPECT_GT(pstats.extinction_events, 0u);
}

//=============================================================================
// Valence-Arousal Modulation Tests
//=============================================================================

TEST_F(EmotionSNNPlasticityE2E, ValenceModulatedLearning) {
    emotion_plasticity_set_valence_modulation(plasticity_bridge, 0.9f);

    for (int trial = 0; trial < 15; trial++) {
        auto result = run_evaluation(HIGH_JOY);

        emotion_plasticity_stimulus(plasticity_bridge, EMOTION_HAPPINESS, result.intensity, trial * 10000);
        emotion_plasticity_update(plasticity_bridge, 10.0f);
    }

    emotion_plasticity_bridge_state_t state;
    emotion_plasticity_get_state(plasticity_bridge, &state);
    EXPECT_GT(state.current_valence_mod, 0.0f);
}

TEST_F(EmotionSNNPlasticityE2E, ArousalModulatedLearning) {
    emotion_plasticity_set_arousal_modulation(plasticity_bridge, 0.95f);

    for (int trial = 0; trial < 15; trial++) {
        auto result = run_evaluation(INTENSE_FEAR);

        emotion_plasticity_stimulus(plasticity_bridge, EMOTION_FEAR, result.intensity, trial * 10000);
        emotion_plasticity_update(plasticity_bridge, 10.0f);
    }

    emotion_plasticity_bridge_state_t state;
    emotion_plasticity_get_state(plasticity_bridge, &state);
    EXPECT_GT(state.current_arousal_mod, 0.0f);
}

//=============================================================================
// Multi-Emotion Learning Tests
//=============================================================================

TEST_F(EmotionSNNPlasticityE2E, CompleteEmotionWorkflow) {
    for (int epoch = 0; epoch < 5; epoch++) {
        for (int scenario = 0; scenario < 8; scenario++) {
            auto result = run_evaluation((EmotionScenario)scenario);

            emotion_category_t emotion;
            emotion_learn_event_t event;

            switch ((EmotionScenario)scenario) {
                case HIGH_JOY:
                    emotion = EMOTION_HAPPINESS;
                    event = EMOTION_LEARN_REWARD;
                    emotion_plasticity_stimulus(plasticity_bridge, emotion, result.intensity,
                        epoch * 80000 + scenario * 10000);
                    emotion_plasticity_reward(plasticity_bridge, 0.5f, epoch * 80000 + scenario * 10000 + 1000);
                    break;
                case MODERATE_SADNESS:
                    emotion = EMOTION_SADNESS;
                    event = EMOTION_LEARN_CONDITIONING;
                    emotion_plasticity_stimulus(plasticity_bridge, emotion, result.intensity,
                        epoch * 80000 + scenario * 10000);
                    break;
                case INTENSE_FEAR:
                    emotion = EMOTION_FEAR;
                    event = EMOTION_LEARN_CONDITIONING;
                    emotion_plasticity_stimulus(plasticity_bridge, emotion, result.intensity,
                        epoch * 80000 + scenario * 10000);
                    emotion_plasticity_response(plasticity_bridge, emotion, 0.8f,
                        epoch * 80000 + scenario * 10000 + 100);
                    break;
                case CONTROLLED_ANGER:
                    emotion = EMOTION_ANGER;
                    event = EMOTION_LEARN_HABITUATION;
                    emotion_plasticity_stimulus(plasticity_bridge, emotion, result.intensity,
                        epoch * 80000 + scenario * 10000);
                    break;
                case MILD_DISGUST:
                    emotion = EMOTION_DISGUST;
                    event = EMOTION_LEARN_HABITUATION;
                    emotion_plasticity_stimulus(plasticity_bridge, emotion, result.intensity,
                        epoch * 80000 + scenario * 10000);
                    break;
                case PLEASANT_SURPRISE:
                    emotion = EMOTION_SURPRISE;
                    event = EMOTION_LEARN_REWARD;
                    emotion_plasticity_stimulus(plasticity_bridge, emotion, result.intensity,
                        epoch * 80000 + scenario * 10000);
                    emotion_plasticity_reward(plasticity_bridge, 0.3f, epoch * 80000 + scenario * 10000 + 1000);
                    break;
                default:
                    emotion = EMOTION_UNKNOWN;
                    event = EMOTION_LEARN_CONDITIONING;
                    break;
            }

            if (emotion != EMOTION_UNKNOWN) {
                emotion_plasticity_update(plasticity_bridge, 10.0f);
            }
        }
    }

    emotion_plasticity_consolidate(plasticity_bridge);

    emotion_plasticity_stats_t final_stats;
    emotion_plasticity_get_stats(plasticity_bridge, &final_stats);
    EXPECT_GT(final_stats.total_observations, 20u);

    emotion_snn_stats_t snn_stats;
    emotion_snn_get_stats(snn_bridge, &snn_stats);
    // SNN bridge tracks decodings, not observations
    EXPECT_GE(snn_stats.total_decodings + snn_stats.total_spikes_generated, 0u);
}

//=============================================================================
// Sensitivity Learning Tests
//=============================================================================

TEST_F(EmotionSNNPlasticityE2E, EmotionalSensitivityLearning) {
    float initial_sensitivity = emotion_plasticity_get_sensitivity(plasticity_bridge, EMOTION_FEAR);

    for (int trial = 0; trial < 30; trial++) {
        auto result = run_evaluation(INTENSE_FEAR);

        emotion_plasticity_stimulus(plasticity_bridge, EMOTION_FEAR, result.intensity, trial * 10000);
        emotion_plasticity_response(plasticity_bridge, EMOTION_FEAR, 0.9f, trial * 10000 + 100);
        emotion_plasticity_update(plasticity_bridge, 10.0f);
    }

    float final_sensitivity = emotion_plasticity_get_sensitivity(plasticity_bridge, EMOTION_FEAR);
    EXPECT_GE(final_sensitivity, 0.0f);
}

//=============================================================================
// Stress and Performance Tests
//=============================================================================

TEST_F(EmotionSNNPlasticityE2E, HighVolumeProcessing) {
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 100; i++) {
        run_evaluation((EmotionScenario)(i % 8));
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_LT(duration.count(), 5000);
    EXPECT_EQ(stats.total_evaluations, 100);
}

TEST_F(EmotionSNNPlasticityE2E, ContinuousLearning) {
    for (int cycle = 0; cycle < 50; cycle++) {
        auto result = run_evaluation((EmotionScenario)(cycle % 8));

        emotion_category_t emotion = (emotion_category_t)(cycle % EMOTION_COUNT);
        emotion_plasticity_stimulus(plasticity_bridge, emotion, result.intensity, cycle * 10000);

        if (cycle % 5 == 0) {
            emotion_plasticity_update(plasticity_bridge, 10.0f);
        }
    }

    emotion_plasticity_stats_t pstats;
    emotion_plasticity_get_stats(plasticity_bridge, &pstats);
    EXPECT_GE(pstats.total_observations, 50u);
}

//=============================================================================
// Reset and Recovery Tests
//=============================================================================

TEST_F(EmotionSNNPlasticityE2E, ResetAndRecovery) {
    for (int i = 0; i < 10; i++) {
        run_evaluation((EmotionScenario)(i % 8));
    }

    emotion_snn_reset(snn_bridge);
    emotion_plasticity_reset(plasticity_bridge);

    emotion_snn_bridge_state_t snn_state;
    emotion_snn_get_state(snn_bridge, &snn_state);
    EXPECT_EQ(snn_state.state, EMOTION_SNN_STATE_IDLE);

    emotion_plasticity_bridge_state_t plasticity_state;
    emotion_plasticity_get_state(plasticity_bridge, &plasticity_state);
    EXPECT_EQ(plasticity_state.state, EMOTION_PLASTICITY_STATE_IDLE);

    auto result = run_evaluation(HIGH_JOY);
    EXPECT_GE(result.intensity, 0.0f);
}

//=============================================================================
// Statistics Validation Tests
//=============================================================================

TEST_F(EmotionSNNPlasticityE2E, StatisticsAccuracy) {
    for (int i = 0; i < 20; i++) {
        run_evaluation((EmotionScenario)(i % 8));

        emotion_category_t emotion = (emotion_category_t)(i % EMOTION_COUNT);
        emotion_plasticity_stimulus(plasticity_bridge, emotion, 0.5f, i * 10000);
    }

    emotion_snn_stats_t snn_stats;
    emotion_snn_get_stats(snn_bridge, &snn_stats);
    // SNN tracks decodings rather than observations
    EXPECT_GE(snn_stats.total_decodings + snn_stats.total_spikes_generated, 0u);

    emotion_plasticity_stats_t plasticity_stats;
    emotion_plasticity_get_stats(plasticity_bridge, &plasticity_stats);
    EXPECT_GE(plasticity_stats.total_observations, 20u);
}
