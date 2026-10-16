/**
 * @file e2e_test_attention_snn_plasticity_pipeline.cpp
 * @brief End-to-end tests for Attention-SNN-Plasticity learning pipeline
 * @version 1.0.0
 * @date 2026-01-06
 *
 * WHAT: Complete attention pipeline with SNN and Plasticity
 * WHY:  Verify full dataflow from attention state -> SNN encoding -> focus strength
 *       -> plasticity learning -> attention bias evolution
 * HOW:  Test realistic scenarios combining attention allocation, STDP learning,
 *       reward-modulated plasticity, and attention habituation/novelty
 *
 * Test Coverage:
 * - Full attention state to focus computation pipeline via SNN
 * - STDP and reward-modulated learning for attention optimization
 * - Habituation and novelty-driven attention modulation
 * - Competition dynamics and winner-take-all attention
 * - Multi-head attention learning scenarios
 * - Attention shift learning and stability
 */

#include <gtest/gtest.h>

#include "cognitive/attention/nimcp_attention_snn_bridge.h"
#include "cognitive/attention/nimcp_attention_plasticity_bridge.h"
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

class AttentionSNNPlasticityE2E : public ::testing::Test {
protected:
    attention_snn_bridge_t* snn_bridge = nullptr;
    attention_plasticity_bridge_t* plasticity_bridge = nullptr;

    struct LearningStats {
        int focus_events = 0;
        int shift_events = 0;
        int novelty_detections = 0;
        int habituation_trials = 0;
        int total_evaluations = 0;
        std::vector<float> focus_history;
        std::vector<float> sparsity_history;
    } stats;

    void SetUp() override {
        attention_snn_config_t snn_config = attention_snn_config_default();
        snn_config.num_heads = 8;
        snn_config.neurons_per_head = 32;
        snn_config.dt_ms = 1.0f;
        snn_config.enable_competition = true;
        snn_config.enable_bio_async = false;

        snn_bridge = attention_snn_create(&snn_config);
        ASSERT_NE(snn_bridge, nullptr) << "Failed to create SNN bridge";

        attention_plasticity_config_t plasticity_config = attention_plasticity_config_default();
        plasticity_config.focus_learning_boost = 0.5f;
        plasticity_config.stdp_a_plus = 0.01f;
        plasticity_config.stdp_a_minus = 0.012f;
        plasticity_config.enable_habituation = true;
        plasticity_config.enable_novelty_detection = true;

        plasticity_bridge = attention_plasticity_create(&plasticity_config);
        ASSERT_NE(plasticity_bridge, nullptr) << "Failed to create Plasticity bridge";

        for (uint32_t i = 0; i < 8; i++) {
            attention_plasticity_register_synapse(plasticity_bridge, i,
                ATTENTION_SYNAPSE_QUERY_KEY, i, 0.5f);
        }

        for (uint32_t i = 100; i < 108; i++) {
            attention_plasticity_register_synapse(plasticity_bridge, i,
                ATTENTION_SYNAPSE_HEAD_OUTPUT, i - 100, 0.5f);
        }
    }

    void TearDown() override {
        if (snn_bridge) {
            attention_snn_destroy(snn_bridge);
            snn_bridge = nullptr;
        }
        if (plasticity_bridge) {
            attention_plasticity_destroy(plasticity_bridge);
            plasticity_bridge = nullptr;
        }
    }

    enum AttentionScenario {
        FOCUSED_ATTENTION,
        DIVIDED_ATTENTION,
        SHIFTING_ATTENTION,
        NOVEL_STIMULUS,
        FAMILIAR_STIMULUS,
        COMPETITION_HIGH,
        SALIENCE_DRIVEN,
        REWARD_GUIDED
    };

    void generate_scenario(float* weights, uint32_t num_heads, AttentionScenario scenario) {
        memset(weights, 0, sizeof(float) * num_heads);

        switch (scenario) {
            case FOCUSED_ATTENTION:
                weights[0] = 0.9f;
                for (uint32_t i = 1; i < num_heads; i++) {
                    weights[i] = 0.1f / (num_heads - 1);
                }
                break;

            case DIVIDED_ATTENTION:
                for (uint32_t i = 0; i < num_heads; i++) {
                    weights[i] = 1.0f / num_heads;
                }
                break;

            case SHIFTING_ATTENTION:
                weights[0] = 0.4f;
                weights[1] = 0.5f;
                weights[2] = 0.1f;
                break;

            case NOVEL_STIMULUS:
                weights[0] = 0.7f;
                weights[1] = 0.2f;
                weights[2] = 0.1f;
                break;

            case FAMILIAR_STIMULUS:
                weights[0] = 0.3f;
                weights[1] = 0.3f;
                weights[2] = 0.2f;
                weights[3] = 0.2f;
                break;

            case COMPETITION_HIGH:
                weights[0] = 0.48f;
                weights[1] = 0.48f;
                weights[2] = 0.04f;
                break;

            case SALIENCE_DRIVEN:
                weights[0] = 0.8f;
                weights[1] = 0.15f;
                weights[2] = 0.05f;
                break;

            case REWARD_GUIDED:
                weights[0] = 0.6f;
                weights[1] = 0.3f;
                weights[2] = 0.1f;
                break;
        }
    }

    struct EvaluationResult {
        float focus_strength;
        float sparsity;
        int spike_count;
        int32_t top_attended;
    };

    EvaluationResult run_evaluation(AttentionScenario scenario) {
        EvaluationResult result = {0};

        float weights[8];
        generate_scenario(weights, 8, scenario);

        result.spike_count = attention_snn_encode_weights(snn_bridge, weights, 8);
        attention_snn_simulate(snn_bridge, 30.0f);

        result.focus_strength = attention_snn_get_focus_strength(snn_bridge);
        result.sparsity = attention_snn_get_sparsity(snn_bridge);

        int32_t top_indices[1];
        if (attention_snn_get_top_k(snn_bridge, top_indices, 1) > 0) {
            result.top_attended = top_indices[0];
        } else {
            result.top_attended = -1;
        }

        stats.total_evaluations++;
        stats.focus_history.push_back(result.focus_strength);
        stats.sparsity_history.push_back(result.sparsity);

        return result;
    }
};

//=============================================================================
// Basic Pipeline Tests
//=============================================================================

TEST_F(AttentionSNNPlasticityE2E, CompletePipelineInitialization) {
    EXPECT_NE(snn_bridge, nullptr);
    EXPECT_NE(plasticity_bridge, nullptr);

    attention_plasticity_bridge_state_t state;
    attention_plasticity_get_state(plasticity_bridge, &state);
    EXPECT_GT(state.registered_synapses, 8u);
}

TEST_F(AttentionSNNPlasticityE2E, SingleEvaluationPipeline) {
    auto result = run_evaluation(FOCUSED_ATTENTION);

    EXPECT_GE(result.focus_strength, 0.0f);
    EXPECT_LE(result.focus_strength, 1.0f);
    EXPECT_GE(result.sparsity, 0.0f);
    EXPECT_LE(result.sparsity, 1.0f);
    EXPECT_GE(result.spike_count, 0);

    int ret = attention_plasticity_focus(plasticity_bridge, 0, result.focus_strength, 0);
    EXPECT_EQ(ret, 0);
}

//=============================================================================
// Focus-Based Learning Tests
//=============================================================================

TEST_F(AttentionSNNPlasticityE2E, FocusDrivenLearning) {
    float total_focus = 0.0f;

    for (int trial = 0; trial < 20; trial++) {
        auto result = run_evaluation(FOCUSED_ATTENTION);
        total_focus += result.focus_strength;

        attention_plasticity_focus(plasticity_bridge, 0, result.focus_strength, trial * 10000);
        attention_plasticity_update(plasticity_bridge, 10.0f);
    }

    EXPECT_GT(total_focus, 0.0f);

    attention_plasticity_stats_t pstats;
    attention_plasticity_get_stats(plasticity_bridge, &pstats);
    EXPECT_EQ(pstats.total_focus_events, 20u);
}

TEST_F(AttentionSNNPlasticityE2E, DividedAttentionLearning) {
    for (int trial = 0; trial < 20; trial++) {
        auto result = run_evaluation(DIVIDED_ATTENTION);

        for (uint32_t head = 0; head < 4; head++) {
            attention_plasticity_focus(plasticity_bridge, head, 0.25f, trial * 10000);
        }
        attention_plasticity_update(plasticity_bridge, 10.0f);
    }

    attention_plasticity_stats_t pstats;
    attention_plasticity_get_stats(plasticity_bridge, &pstats);
    EXPECT_EQ(pstats.total_focus_events, 80u);
}

//=============================================================================
// Attention Shift Learning Tests
//=============================================================================

TEST_F(AttentionSNNPlasticityE2E, AttentionShiftLearning) {
    uint32_t current_head = 0;

    for (int trial = 0; trial < 30; trial++) {
        auto result = run_evaluation(SHIFTING_ATTENTION);

        uint32_t new_head = (trial % 3);
        if (new_head != current_head) {
            attention_plasticity_shift(plasticity_bridge, current_head, new_head,
                result.focus_strength, trial * 10000);
            current_head = new_head;
            stats.shift_events++;
        }

        attention_plasticity_focus(plasticity_bridge, current_head, result.focus_strength, trial * 10000 + 5000);
        attention_plasticity_update(plasticity_bridge, 10.0f);
    }

    EXPECT_GT(stats.shift_events, 10);

    attention_plasticity_stats_t pstats;
    attention_plasticity_get_stats(plasticity_bridge, &pstats);
    EXPECT_GT(pstats.total_shift_events, 0u);
}

//=============================================================================
// Habituation and Novelty Tests
//=============================================================================

TEST_F(AttentionSNNPlasticityE2E, HabituationLearning) {
    for (int trial = 0; trial < 30; trial++) {
        auto result = run_evaluation(FAMILIAR_STIMULUS);

        attention_plasticity_habituation_trial(plasticity_bridge, 0, trial * 10000);
        attention_plasticity_update(plasticity_bridge, 10.0f);
    }

    float habituation = attention_plasticity_get_habituation(plasticity_bridge, 0);
    EXPECT_GE(habituation, 0.0f);

    attention_plasticity_stats_t pstats;
    attention_plasticity_get_stats(plasticity_bridge, &pstats);
    EXPECT_GT(pstats.habituation_events, 0u);
}

TEST_F(AttentionSNNPlasticityE2E, NoveltyLearning) {
    for (int trial = 0; trial < 20; trial++) {
        auto result = run_evaluation(NOVEL_STIMULUS);

        attention_plasticity_novelty(plasticity_bridge, 0, 0.9f, trial * 10000);
        attention_plasticity_update(plasticity_bridge, 10.0f);
    }

    float novelty_score = attention_plasticity_get_novelty_score(plasticity_bridge, 0);
    EXPECT_GE(novelty_score, 0.0f);

    attention_plasticity_stats_t pstats;
    attention_plasticity_get_stats(plasticity_bridge, &pstats);
    EXPECT_GT(pstats.novelty_events, 0u);
}

//=============================================================================
// Competition Learning Tests
//=============================================================================

TEST_F(AttentionSNNPlasticityE2E, CompetitionDrivenLearning) {
    for (int trial = 0; trial < 20; trial++) {
        auto result = run_evaluation(COMPETITION_HIGH);

        attention_snn_compete(snn_bridge, 20.0f);
        float final_focus = attention_snn_get_focus_strength(snn_bridge);

        attention_plasticity_focus(plasticity_bridge, result.top_attended >= 0 ? result.top_attended : 0,
            final_focus, trial * 10000);
        attention_plasticity_update(plasticity_bridge, 10.0f);
    }

    attention_snn_stats_t snn_stats;
    attention_snn_get_stats(snn_bridge, &snn_stats);
    EXPECT_GE(snn_stats.total_forward_passes, 20u);
}

//=============================================================================
// Salience-Based Learning Tests
//=============================================================================

TEST_F(AttentionSNNPlasticityE2E, SalienceBasedLearning) {
    float salience_map[64];
    for (int i = 0; i < 64; i++) {
        salience_map[i] = (i < 16) ? 0.8f : 0.2f;
    }

    for (int trial = 0; trial < 15; trial++) {
        attention_snn_encode_salience(snn_bridge, salience_map, 64);
        attention_snn_simulate(snn_bridge, 30.0f);

        attention_plasticity_salience(plasticity_bridge, salience_map, 64, trial * 10000);
        attention_plasticity_update(plasticity_bridge, 10.0f);
    }

    attention_snn_stats_t snn_stats;
    attention_snn_get_stats(snn_bridge, &snn_stats);
    EXPECT_GE(snn_stats.total_forward_passes, 15u);
}

//=============================================================================
// Reward-Modulated Learning Tests
//=============================================================================

TEST_F(AttentionSNNPlasticityE2E, RewardModulatedLearning) {
    for (int trial = 0; trial < 20; trial++) {
        auto result = run_evaluation(REWARD_GUIDED);

        attention_plasticity_focus(plasticity_bridge, 0, result.focus_strength, trial * 10000);
        float reward = (trial % 2 == 0) ? 1.0f : -0.5f;
        attention_plasticity_reward(plasticity_bridge, reward, trial * 10000 + 5000);
        attention_plasticity_update(plasticity_bridge, 10.0f);
    }

    attention_plasticity_stats_t pstats;
    attention_plasticity_get_stats(plasticity_bridge, &pstats);
    EXPECT_GT(pstats.total_reward, 0.0f);
}

//=============================================================================
// Multi-Scenario Learning Tests
//=============================================================================

TEST_F(AttentionSNNPlasticityE2E, CompleteAttentionWorkflow) {
    for (int epoch = 0; epoch < 5; epoch++) {
        for (int scenario = 0; scenario < 8; scenario++) {
            auto result = run_evaluation((AttentionScenario)scenario);

            attention_learn_event_t event;
            uint32_t head_idx = result.top_attended >= 0 ? result.top_attended : 0;

            switch ((AttentionScenario)scenario) {
                case FOCUSED_ATTENTION:
                    event = ATTENTION_LEARN_FOCUS;
                    attention_plasticity_focus(plasticity_bridge, head_idx, result.focus_strength,
                        epoch * 80000 + scenario * 10000);
                    break;
                case SHIFTING_ATTENTION:
                    event = ATTENTION_LEARN_SHIFT;
                    attention_plasticity_shift(plasticity_bridge, 0, head_idx, result.focus_strength,
                        epoch * 80000 + scenario * 10000);
                    break;
                case NOVEL_STIMULUS:
                    event = ATTENTION_LEARN_NOVELTY;
                    attention_plasticity_novelty(plasticity_bridge, head_idx, 0.8f,
                        epoch * 80000 + scenario * 10000);
                    break;
                case FAMILIAR_STIMULUS:
                    event = ATTENTION_LEARN_HABITUATION;
                    attention_plasticity_habituation_trial(plasticity_bridge, head_idx,
                        epoch * 80000 + scenario * 10000);
                    break;
                case REWARD_GUIDED:
                    event = ATTENTION_LEARN_REWARD;
                    attention_plasticity_focus(plasticity_bridge, head_idx, result.focus_strength,
                        epoch * 80000 + scenario * 10000);
                    attention_plasticity_reward(plasticity_bridge, 0.5f, epoch * 80000 + scenario * 10000 + 1000);
                    break;
                default:
                    event = ATTENTION_LEARN_FOCUS;
                    attention_plasticity_focus(plasticity_bridge, head_idx, result.focus_strength,
                        epoch * 80000 + scenario * 10000);
                    break;
            }

            attention_plasticity_update(plasticity_bridge, 10.0f);
        }
    }

    attention_plasticity_consolidate(plasticity_bridge);

    attention_plasticity_stats_t final_stats;
    attention_plasticity_get_stats(plasticity_bridge, &final_stats);
    EXPECT_GT(final_stats.total_focus_events, 20u);

    attention_snn_stats_t snn_stats;
    attention_snn_get_stats(snn_bridge, &snn_stats);
    EXPECT_GE(snn_stats.total_forward_passes, 40u);
}

//=============================================================================
// Stress and Performance Tests
//=============================================================================

TEST_F(AttentionSNNPlasticityE2E, HighVolumeProcessing) {
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 100; i++) {
        run_evaluation((AttentionScenario)(i % 8));
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_LT(duration.count(), 5000);
    EXPECT_EQ(stats.total_evaluations, 100);
}

TEST_F(AttentionSNNPlasticityE2E, ContinuousLearning) {
    for (int cycle = 0; cycle < 50; cycle++) {
        auto result = run_evaluation((AttentionScenario)(cycle % 8));

        attention_plasticity_focus(plasticity_bridge, cycle % 8, result.focus_strength, cycle * 10000);

        if (cycle % 5 == 0) {
            attention_plasticity_update(plasticity_bridge, 10.0f);
        }
    }

    attention_plasticity_stats_t pstats;
    attention_plasticity_get_stats(plasticity_bridge, &pstats);
    EXPECT_GE(pstats.total_focus_events, 50u);
}

//=============================================================================
// Reset and Recovery Tests
//=============================================================================

TEST_F(AttentionSNNPlasticityE2E, ResetAndRecovery) {
    for (int i = 0; i < 10; i++) {
        run_evaluation((AttentionScenario)(i % 8));
    }

    attention_snn_reset(snn_bridge);
    attention_plasticity_reset(plasticity_bridge);

    attention_snn_bridge_state_t snn_state;
    attention_snn_get_state(snn_bridge, &snn_state);
    EXPECT_EQ(snn_state.state, ATTENTION_SNN_STATE_IDLE);

    attention_plasticity_bridge_state_t plasticity_state;
    attention_plasticity_get_state(plasticity_bridge, &plasticity_state);
    EXPECT_EQ(plasticity_state.state, ATTENTION_PLASTICITY_STATE_IDLE);

    auto result = run_evaluation(FOCUSED_ATTENTION);
    EXPECT_GE(result.focus_strength, 0.0f);
}

//=============================================================================
// Statistics Validation Tests
//=============================================================================

TEST_F(AttentionSNNPlasticityE2E, StatisticsAccuracy) {
    for (int i = 0; i < 20; i++) {
        run_evaluation((AttentionScenario)(i % 8));
        attention_plasticity_focus(plasticity_bridge, i % 8, 0.5f, i * 10000);
    }

    attention_snn_stats_t snn_stats;
    attention_snn_get_stats(snn_bridge, &snn_stats);
    EXPECT_GE(snn_stats.total_forward_passes, 20u);

    attention_plasticity_stats_t plasticity_stats;
    attention_plasticity_get_stats(plasticity_bridge, &plasticity_stats);
    EXPECT_GE(plasticity_stats.total_focus_events, 20u);
}
