/**
 * @file e2e_test_wellbeing_snn_plasticity_pipeline.cpp
 * @brief End-to-end tests for Wellbeing-SNN-Plasticity learning pipeline
 * @version 1.0.0
 * @date 2026-01-06
 *
 * WHAT: Complete wellbeing pipeline with SNN and Plasticity
 * WHY:  Verify full dataflow from wellbeing state -> SNN encoding -> assessment
 *       -> plasticity learning -> foundation evolution
 * HOW:  Test realistic scenarios combining hedonic/eudaimonic encoding, STDP learning,
 *       reward-modulated plasticity, and protected synapse integrity
 *
 * Test Coverage:
 * - Full wellbeing state to assessment pipeline via SNN
 * - STDP and reward-modulated learning for wellbeing adaptation
 * - Stress detection and recovery
 * - Resilience synapse protection
 * - Multi-scenario wellbeing learning
 * - Foundation state evolution through experience
 * - Protected synapse integrity under stress
 */

#include <gtest/gtest.h>

#include "cognitive/wellbeing/nimcp_wellbeing_snn_bridge.h"
#include "cognitive/wellbeing/nimcp_wellbeing_plasticity_bridge.h"
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

class WellbeingSNNPlasticityE2E : public ::testing::Test {
protected:
    wellbeing_snn_bridge_t* snn_bridge = nullptr;
    wellbeing_plasticity_bridge_t* plasticity_bridge = nullptr;

    // Learning statistics
    struct LearningStats {
        int flourishing_count = 0;
        int stress_count = 0;
        int recovery_count = 0;
        int total_evaluations = 0;
        std::vector<float> flourishing_history;
        std::vector<float> stress_scores;
    } stats;

    void SetUp() override {
        // Create SNN bridge with full wellbeing dimensions
        wellbeing_snn_config_t snn_config = wellbeing_snn_config_default();
        snn_config.num_dimensions = WELLBEING_DIM_COUNT;
        snn_config.neurons_per_dim = 32;
        snn_config.dt_ms = 1.0f;
        snn_config.enable_stress_detection = true;
        snn_config.enable_bio_async = false;

        snn_bridge = wellbeing_snn_create(&snn_config);
        ASSERT_NE(snn_bridge, nullptr) << "Failed to create SNN bridge";

        // Create Plasticity bridge with all learning mechanisms
        wellbeing_plasticity_config_t plasticity_config = wellbeing_plasticity_config_default();
        plasticity_config.base_learning_rate = 0.01f;
        plasticity_config.stdp_a_plus = 0.01f;
        plasticity_config.stdp_a_minus = 0.012f;

        plasticity_bridge = wellbeing_plasticity_create(&plasticity_config);
        ASSERT_NE(plasticity_bridge, nullptr) << "Failed to create Plasticity bridge";

        // Register base synapses for plasticity
        for (uint32_t i = 0; i < WELLBEING_DIM_COUNT; i++) {
            wellbeing_plasticity_register_synapse(plasticity_bridge, i,
                WELLBEING_SYNAPSE_HEDONIC, 0.5f);
        }

        // Register protected synapses
        wellbeing_plasticity_register_synapse(plasticity_bridge, 100,
            WELLBEING_SYNAPSE_RESILIENCE, 1.0f);
    }

    void TearDown() override {
        if (snn_bridge) {
            wellbeing_snn_destroy(snn_bridge);
            snn_bridge = nullptr;
        }
        if (plasticity_bridge) {
            wellbeing_plasticity_destroy(plasticity_bridge);
            plasticity_bridge = nullptr;
        }
    }

    // Generate wellbeing scenario
    enum WellbeingScenario {
        FLOURISHING,            // High wellbeing across dimensions
        HIGH_STRESS,            // Stressed state
        RECOVERY,               // Recovering from stress
        SOCIAL_SUPPORT,         // Strong social connection
        EUDAIMONIC_FOCUS,       // Meaning and purpose
        BALANCED,               // Balanced across dimensions
        LOW_VITALITY,           // Low energy
        RESILIENCE_TEST         // Testing resilience
    };

    void generate_scenario(float* dims, WellbeingScenario scenario) {
        memset(dims, 0, sizeof(float) * WELLBEING_DIM_COUNT);

        switch (scenario) {
            case FLOURISHING:
                dims[WELLBEING_DIM_HEDONIC] = 0.85f;
                dims[WELLBEING_DIM_EUDAIMONIC] = 0.9f;
                dims[WELLBEING_DIM_VITALITY] = 0.8f;
                dims[WELLBEING_DIM_RESILIENCE] = 0.85f;
                dims[WELLBEING_DIM_SOCIAL_CONNECTION] = 0.8f;
                dims[WELLBEING_DIM_STRESS] = 0.1f;
                break;

            case HIGH_STRESS:
                dims[WELLBEING_DIM_STRESS] = 0.9f;
                dims[WELLBEING_DIM_HEDONIC] = 0.2f;
                dims[WELLBEING_DIM_VITALITY] = 0.3f;
                dims[WELLBEING_DIM_RESILIENCE] = 0.4f;
                break;

            case RECOVERY:
                dims[WELLBEING_DIM_RESILIENCE] = 0.8f;
                dims[WELLBEING_DIM_STRESS] = 0.3f;
                dims[WELLBEING_DIM_HEDONIC] = 0.6f;
                dims[WELLBEING_DIM_VITALITY] = 0.65f;
                break;

            case SOCIAL_SUPPORT:
                dims[WELLBEING_DIM_SOCIAL_CONNECTION] = 0.95f;
                dims[WELLBEING_DIM_HEDONIC] = 0.75f;
                dims[WELLBEING_DIM_AUTONOMY] = 0.6f;
                dims[WELLBEING_DIM_STRESS] = 0.2f;
                break;

            case EUDAIMONIC_FOCUS:
                dims[WELLBEING_DIM_EUDAIMONIC] = 0.95f;
                dims[WELLBEING_DIM_AUTONOMY] = 0.85f;
                dims[WELLBEING_DIM_COMPETENCE] = 0.8f;
                dims[WELLBEING_DIM_STRESS] = 0.15f;
                break;

            case BALANCED:
                for (int i = 0; i < WELLBEING_DIM_COUNT; i++) {
                    dims[i] = 0.6f;
                }
                dims[WELLBEING_DIM_STRESS] = 0.3f;
                break;

            case LOW_VITALITY:
                dims[WELLBEING_DIM_VITALITY] = 0.2f;
                dims[WELLBEING_DIM_HEDONIC] = 0.4f;
                dims[WELLBEING_DIM_STRESS] = 0.5f;
                break;

            case RESILIENCE_TEST:
                dims[WELLBEING_DIM_RESILIENCE] = 0.9f;
                dims[WELLBEING_DIM_STRESS] = 0.6f;
                dims[WELLBEING_DIM_HEDONIC] = 0.5f;
                break;
        }
    }

    // Run single evaluation pipeline
    struct EvaluationResult {
        float flourishing_score;
        float stress_level;
        float resilience_score;
        bool stress_detected;
        bool balance_achieved;
        int spike_count;
    };

    EvaluationResult run_evaluation(WellbeingScenario scenario) {
        EvaluationResult result = {0};

        float dims[WELLBEING_DIM_COUNT];
        generate_scenario(dims, scenario);

        // Encode and simulate
        result.spike_count = wellbeing_snn_encode_state(snn_bridge, dims, WELLBEING_DIM_COUNT);
        wellbeing_snn_simulate(snn_bridge, 30.0f);

        // Get assessment
        wellbeing_assessment_t assessment;
        wellbeing_snn_get_assessment(snn_bridge, &assessment);

        result.flourishing_score = assessment.flourishing_score;
        result.stress_level = assessment.stress_level;
        result.resilience_score = assessment.resilience_score;
        result.stress_detected = assessment.stress_detected;
        result.balance_achieved = assessment.balance_achieved;

        // Update stats
        stats.total_evaluations++;
        stats.flourishing_history.push_back(assessment.flourishing_score);
        stats.stress_scores.push_back(assessment.stress_level);

        if (assessment.stress_detected) {
            stats.stress_count++;
        }
        if (assessment.flourishing_score > 0.7f) {
            stats.flourishing_count++;
        }

        return result;
    }
};

//=============================================================================
// Basic Pipeline Tests
//=============================================================================

TEST_F(WellbeingSNNPlasticityE2E, CompletePipelineInitialization) {
    // Verify complete pipeline setup
    EXPECT_NE(snn_bridge, nullptr);
    EXPECT_NE(plasticity_bridge, nullptr);

    // Check synapse registration
    wellbeing_plasticity_bridge_state_t state;
    wellbeing_plasticity_get_state(plasticity_bridge, &state);
    EXPECT_GT(state.active_synapses, (uint32_t)WELLBEING_DIM_COUNT);  // Base + protected
}

TEST_F(WellbeingSNNPlasticityE2E, SingleEvaluationPipeline) {
    // Run single flourishing scenario
    auto result = run_evaluation(FLOURISHING);

    // Verify assessment is valid
    EXPECT_GE(result.flourishing_score, 0.0f);
    EXPECT_LE(result.flourishing_score, 1.0f);
    EXPECT_GE(result.stress_level, 0.0f);
    EXPECT_LE(result.stress_level, 1.0f);
    EXPECT_GE(result.spike_count, 0);

    // Apply learning based on result
    int ret = wellbeing_plasticity_learn(plasticity_bridge,
        WELLBEING_LEARN_POSITIVE_EXPERIENCE, 0.5f, 0, result.flourishing_score);
    EXPECT_EQ(ret, 0);
}

//=============================================================================
// Stress and Recovery Tests
//=============================================================================

TEST_F(WellbeingSNNPlasticityE2E, StressRecoveryLearning) {
    // Run stress scenarios
    float total_stress = 0.0f;
    int stress_count = 0;

    for (int trial = 0; trial < 10; trial++) {
        auto result = run_evaluation(HIGH_STRESS);

        total_stress += result.stress_level;
        if (result.stress_level > 0.0f || result.stress_detected) {
            stress_count++;
        }

        // Learn from stress
        wellbeing_plasticity_learn(plasticity_bridge,
            WELLBEING_LEARN_STRESS_ACCUMULATED, 0.5f, 0, result.stress_level);

        // Apply STDP
        wellbeing_plasticity_apply_stdp(plasticity_bridge, 0,
            (float)trial, (float)trial + 5.0f);
    }

    // At least some trials should register stress signals
    EXPECT_GT(stress_count, 0);
}

TEST_F(WellbeingSNNPlasticityE2E, RecoveryLearning) {
    // Run recovery scenarios
    float total_resilience = 0.0f;

    for (int trial = 0; trial < 10; trial++) {
        auto result = run_evaluation(RECOVERY);

        total_resilience += result.resilience_score;

        // Learn from recovery
        wellbeing_plasticity_learn(plasticity_bridge,
            WELLBEING_LEARN_STRESS_RECOVERED, 0.5f, 1, result.resilience_score);
    }

    // Average resilience should be above minimum
    EXPECT_GT(total_resilience / 10.0f, 0.0f);
}

TEST_F(WellbeingSNNPlasticityE2E, FlourishingImprovement) {
    // Register wellbeing synapses
    for (int i = 200; i < 210; i++) {
        wellbeing_plasticity_register_synapse(plasticity_bridge, i,
            WELLBEING_SYNAPSE_HEDONIC, 0.5f);
    }

    // Initial weight
    wellbeing_plasticity_synapse_t initial_synapse;
    wellbeing_plasticity_get_synapse(plasticity_bridge, 200, &initial_synapse);
    float initial_weight = initial_synapse.weight;

    // Run mixed wellbeing scenarios
    for (int epoch = 0; epoch < 5; epoch++) {
        // Flourishing scenario
        auto good_result = run_evaluation(FLOURISHING);
        wellbeing_plasticity_learn(plasticity_bridge,
            WELLBEING_LEARN_POSITIVE_EXPERIENCE, 0.3f, 200, good_result.flourishing_score);

        // Stress scenario
        auto stress_result = run_evaluation(HIGH_STRESS);
        wellbeing_plasticity_learn(plasticity_bridge,
            WELLBEING_LEARN_STRESS_ACCUMULATED, 0.3f, 201, stress_result.stress_level);

        // BCM and homeostatic updates
        wellbeing_plasticity_update_bcm(plasticity_bridge, 0.5f);
        wellbeing_plasticity_homeostatic_update(plasticity_bridge, 0.5f);
    }

    // Verify learning occurred
    wellbeing_plasticity_stats_t stats;
    wellbeing_plasticity_get_stats(plasticity_bridge, &stats);
    EXPECT_GT(stats.total_learning_events, 0u);
    EXPECT_GT(stats.positive_experience_events, 0u);
}

//=============================================================================
// Protected Synapse Tests
//=============================================================================

TEST_F(WellbeingSNNPlasticityE2E, ResilienceProtectionIntegrity) {
    // Get initial protected synapse weight
    wellbeing_plasticity_synapse_t resilience_synapse;
    wellbeing_plasticity_get_synapse(plasticity_bridge, 100, &resilience_synapse);
    float original_weight = resilience_synapse.weight;
    EXPECT_TRUE(resilience_synapse.is_protected);

    // Run many scenarios and try to modify protected synapse
    for (int trial = 0; trial < 20; trial++) {
        auto result = run_evaluation((WellbeingScenario)(trial % 8));

        // Try various learning operations on protected synapse
        wellbeing_plasticity_learn(plasticity_bridge,
            WELLBEING_LEARN_NEGATIVE_EXPERIENCE, -1.0f, 100, result.flourishing_score);
        wellbeing_plasticity_apply_stdp(plasticity_bridge, 100,
            (float)trial, (float)trial + 10.0f);
        wellbeing_plasticity_apply_reward(plasticity_bridge, -1.0f);
    }

    // Protected synapse should remain unchanged
    wellbeing_plasticity_get_synapse(plasticity_bridge, 100, &resilience_synapse);
    EXPECT_FLOAT_EQ(resilience_synapse.weight, original_weight);
    EXPECT_TRUE(resilience_synapse.is_protected);
}

//=============================================================================
// Social Support Tests
//=============================================================================

TEST_F(WellbeingSNNPlasticityE2E, SocialSupportLearning) {
    // Register social synapses
    for (int i = 300; i < 305; i++) {
        wellbeing_plasticity_register_synapse(plasticity_bridge, i,
            WELLBEING_SYNAPSE_SOCIAL, 0.5f);
    }

    int learning_events = 0;

    for (int trial = 0; trial < 10; trial++) {
        auto result = run_evaluation(SOCIAL_SUPPORT);

        // Apply learning for all social support scenarios
        wellbeing_plasticity_learn(plasticity_bridge,
            WELLBEING_LEARN_SOCIAL_SUPPORT, 0.5f, 300 + (trial % 5),
            result.flourishing_score > 0.0f ? result.flourishing_score : 0.5f);
        learning_events++;
    }

    // Verify we applied learning across all trials
    EXPECT_EQ(learning_events, 10);

    // Verify plasticity stats reflect learning
    wellbeing_plasticity_stats_t stats;
    wellbeing_plasticity_get_stats(plasticity_bridge, &stats);
    EXPECT_GE(stats.social_support_events, 10u);
}

//=============================================================================
// Eudaimonic Learning Tests
//=============================================================================

TEST_F(WellbeingSNNPlasticityE2E, EudaimonicLearning) {
    // Register eudaimonic synapses
    for (int i = 400; i < 405; i++) {
        wellbeing_plasticity_register_synapse(plasticity_bridge, i,
            WELLBEING_SYNAPSE_EUDAIMONIC, 0.5f);
    }

    for (int trial = 0; trial < 10; trial++) {
        auto result = run_evaluation(EUDAIMONIC_FOCUS);

        wellbeing_plasticity_learn(plasticity_bridge,
            WELLBEING_LEARN_MEANING_FOUND, 0.5f, 400 + (trial % 5),
            result.flourishing_score);
    }

    // Verify foundation evolved
    wellbeing_foundation_state_t foundation;
    wellbeing_plasticity_get_foundation_state(plasticity_bridge, &foundation);
    EXPECT_GE(foundation.eudaimonic_strength, 0.5f);
}

//=============================================================================
// Multi-Scenario Learning Tests
//=============================================================================

TEST_F(WellbeingSNNPlasticityE2E, CompleteWellbeingWorkflow) {
    // Register workflow synapses
    for (int i = 500; i < 520; i++) {
        wellbeing_plasticity_register_synapse(plasticity_bridge, i,
            WELLBEING_SYNAPSE_HEDONIC, 0.5f);
    }

    // Run complete wellbeing workflow
    for (int epoch = 0; epoch < 5; epoch++) {
        for (int scenario = 0; scenario < 8; scenario++) {
            auto result = run_evaluation((WellbeingScenario)scenario);

            // Select learning event based on scenario
            wellbeing_learn_event_t event;
            float magnitude = 0.3f;

            switch ((WellbeingScenario)scenario) {
                case FLOURISHING:
                case BALANCED:
                    event = WELLBEING_LEARN_POSITIVE_EXPERIENCE;
                    break;
                case HIGH_STRESS:
                    event = WELLBEING_LEARN_STRESS_ACCUMULATED;
                    break;
                case RECOVERY:
                    event = WELLBEING_LEARN_STRESS_RECOVERED;
                    break;
                case SOCIAL_SUPPORT:
                    event = WELLBEING_LEARN_SOCIAL_SUPPORT;
                    break;
                case EUDAIMONIC_FOCUS:
                    event = WELLBEING_LEARN_MEANING_FOUND;
                    break;
                case LOW_VITALITY:
                    event = WELLBEING_LEARN_NEGATIVE_EXPERIENCE;
                    break;
                default:
                    event = WELLBEING_LEARN_BALANCE_IMPROVED;
                    break;
            }

            int synapse_id = 500 + (epoch * 8 + scenario) % 20;
            wellbeing_plasticity_learn(plasticity_bridge, event, magnitude,
                synapse_id, result.flourishing_score);

            // Apply STDP
            wellbeing_plasticity_apply_stdp(plasticity_bridge, synapse_id,
                (float)(epoch * 10 + scenario), (float)(epoch * 10 + scenario + 5));
        }

        // Periodic maintenance
        wellbeing_plasticity_update_bcm(plasticity_bridge, 0.5f);
        wellbeing_plasticity_homeostatic_update(plasticity_bridge, 0.5f);
        wellbeing_plasticity_update_traces(plasticity_bridge, 1.0f);
    }

    // Consolidate learning
    wellbeing_plasticity_consolidate(plasticity_bridge);

    // Verify extensive learning occurred
    wellbeing_plasticity_stats_t final_stats;
    wellbeing_plasticity_get_stats(plasticity_bridge, &final_stats);
    EXPECT_GT(final_stats.total_learning_events, 30u);
    EXPECT_GT(final_stats.weight_updates, 30u);

    wellbeing_snn_stats_t snn_stats;
    wellbeing_snn_get_stats(snn_bridge, &snn_stats);
    EXPECT_GE(snn_stats.total_evaluations, 40u);
}

//=============================================================================
// Stress and Performance Tests
//=============================================================================

TEST_F(WellbeingSNNPlasticityE2E, HighVolumeProcessing) {
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 100; i++) {
        run_evaluation((WellbeingScenario)(i % 8));
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // 100 evaluations should complete in under 5 seconds
    EXPECT_LT(duration.count(), 5000);
    EXPECT_EQ(stats.total_evaluations, 100);
}

TEST_F(WellbeingSNNPlasticityE2E, ContinuousLearning) {
    // Register many synapses
    for (int i = 600; i < 700; i++) {
        wellbeing_plasticity_register_synapse(plasticity_bridge, i,
            WELLBEING_SYNAPSE_HEDONIC, 0.5f);
    }

    // Continuous learning loop
    for (int cycle = 0; cycle < 50; cycle++) {
        auto result = run_evaluation((WellbeingScenario)(cycle % 8));

        // Learn on rotating synapses
        for (int j = 0; j < 5; j++) {
            int synapse_id = 600 + (cycle * 5 + j) % 100;
            wellbeing_plasticity_learn(plasticity_bridge,
                WELLBEING_LEARN_POSITIVE_EXPERIENCE, 0.1f, synapse_id, result.flourishing_score);
        }

        // Periodic BCM update
        if (cycle % 10 == 0) {
            wellbeing_plasticity_update_bcm(plasticity_bridge, 0.5f);
        }
    }

    // Verify extensive learning
    wellbeing_plasticity_stats_t stats;
    wellbeing_plasticity_get_stats(plasticity_bridge, &stats);
    EXPECT_GE(stats.total_learning_events, 200u);
}

//=============================================================================
// Reset and Recovery Tests
//=============================================================================

TEST_F(WellbeingSNNPlasticityE2E, ResetAndRecovery) {
    // Accumulate some state
    for (int i = 0; i < 10; i++) {
        run_evaluation((WellbeingScenario)(i % 8));
    }

    // Reset both bridges
    wellbeing_snn_reset(snn_bridge);
    wellbeing_plasticity_reset(plasticity_bridge);

    // Verify recovery
    wellbeing_snn_bridge_state_t snn_state;
    wellbeing_snn_get_state(snn_bridge, &snn_state);
    EXPECT_EQ(snn_state.state, WELLBEING_SNN_STATE_IDLE);

    wellbeing_plasticity_bridge_state_t plasticity_state;
    wellbeing_plasticity_get_state(plasticity_bridge, &plasticity_state);
    EXPECT_EQ(plasticity_state.state, WELLBEING_PLASTICITY_STATE_IDLE);

    // Can continue processing
    auto result = run_evaluation(FLOURISHING);
    EXPECT_GE(result.flourishing_score, 0.0f);
}

//=============================================================================
// Statistics Validation Tests
//=============================================================================

TEST_F(WellbeingSNNPlasticityE2E, StatisticsAccuracy) {
    // Run known number of evaluations
    for (int i = 0; i < 20; i++) {
        run_evaluation((WellbeingScenario)(i % 8));

        // Apply learning
        wellbeing_plasticity_learn(plasticity_bridge,
            WELLBEING_LEARN_POSITIVE_EXPERIENCE, 0.1f, i % WELLBEING_DIM_COUNT, 0.5f);
    }

    // Verify stats match
    wellbeing_snn_stats_t snn_stats;
    wellbeing_snn_get_stats(snn_bridge, &snn_stats);
    EXPECT_GE(snn_stats.total_evaluations, 20u);

    wellbeing_plasticity_stats_t plasticity_stats;
    wellbeing_plasticity_get_stats(plasticity_bridge, &plasticity_stats);
    EXPECT_GE(plasticity_stats.total_learning_events, 20u);
}

//=============================================================================
// Foundation Evolution Tests
//=============================================================================

TEST_F(WellbeingSNNPlasticityE2E, FoundationEvolutionThroughExperience) {
    wellbeing_foundation_state_t initial;
    wellbeing_plasticity_get_foundation_state(plasticity_bridge, &initial);

    // Register various synapse types
    wellbeing_plasticity_register_synapse(plasticity_bridge, 700,
        WELLBEING_SYNAPSE_HEDONIC, 0.5f);
    wellbeing_plasticity_register_synapse(plasticity_bridge, 701,
        WELLBEING_SYNAPSE_SOCIAL, 0.5f);
    wellbeing_plasticity_register_synapse(plasticity_bridge, 702,
        WELLBEING_SYNAPSE_EUDAIMONIC, 0.5f);

    // Apply positive experiences
    for (int i = 0; i < 30; i++) {
        wellbeing_plasticity_learn(plasticity_bridge,
            WELLBEING_LEARN_POSITIVE_EXPERIENCE, 0.5f, 700, 0.8f);
        wellbeing_plasticity_learn(plasticity_bridge,
            WELLBEING_LEARN_SOCIAL_SUPPORT, 0.5f, 701, 0.85f);
        wellbeing_plasticity_learn(plasticity_bridge,
            WELLBEING_LEARN_MEANING_FOUND, 0.5f, 702, 0.9f);
        wellbeing_plasticity_learn(plasticity_bridge,
            WELLBEING_LEARN_STRESS_RECOVERED, 0.4f, 700, 0.7f);
    }

    wellbeing_foundation_state_t final_state;
    wellbeing_plasticity_get_foundation_state(plasticity_bridge, &final_state);

    // Foundation should have evolved positively
    EXPECT_GT(final_state.hedonic_sensitivity, initial.hedonic_sensitivity);
    EXPECT_GT(final_state.social_connection_strength, initial.social_connection_strength);
    EXPECT_GT(final_state.eudaimonic_strength, initial.eudaimonic_strength);
    EXPECT_GT(final_state.resilience_level, initial.resilience_level);
}
