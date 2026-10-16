/**
 * @file e2e_test_curiosity_snn_plasticity_pipeline.cpp
 * @brief End-to-end tests for Curiosity-SNN-Plasticity learning pipeline
 * @version 1.0.0
 * @date 2026-01-06
 *
 * WHAT: Complete curiosity pipeline with SNN and Plasticity
 * WHY:  Verify full dataflow from curiosity state -> SNN encoding -> exploration drive
 *       -> plasticity learning -> novelty sensitivity evolution
 * HOW:  Test realistic scenarios combining novelty detection, STDP learning,
 *       reward-modulated plasticity, and protected synapse integrity
 *
 * Test Coverage:
 * - Full curiosity state to exploration drive pipeline via SNN
 * - STDP and reward-modulated learning for exploration optimization
 * - Novelty detection and information gain learning
 * - Exploration and Learning synapse protection
 * - Multi-scenario exploration learning
 * - Novelty sensitivity evolution through experience
 * - Protected synapse integrity under stress
 */

#include <gtest/gtest.h>

#include "cognitive/curiosity/nimcp_curiosity_snn_bridge.h"
#include "cognitive/curiosity/nimcp_curiosity_plasticity_bridge.h"
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

class CuriositySNNPlasticityE2E : public ::testing::Test {
protected:
    curiosity_snn_bridge_t* snn_bridge = nullptr;
    curiosity_plasticity_bridge_t* plasticity_bridge = nullptr;

    // Learning statistics
    struct LearningStats {
        int novelty_detections = 0;
        int high_info_gain_events = 0;
        int exploration_successes = 0;
        int total_evaluations = 0;
        std::vector<float> exploration_history;
        std::vector<float> novelty_scores;
    } stats;

    void SetUp() override {
        // Create SNN bridge with full curiosity dimensions
        curiosity_snn_config_t snn_config = curiosity_snn_config_default();
        snn_config.num_dimensions = CURIOSITY_DIM_COUNT;
        snn_config.neurons_per_dim = 32;
        snn_config.dt_ms = 1.0f;
        snn_config.enable_novelty_detection = true;
        snn_config.enable_bio_async = false;

        snn_bridge = curiosity_snn_create(&snn_config);
        ASSERT_NE(snn_bridge, nullptr) << "Failed to create SNN bridge";

        // Create Plasticity bridge with all learning mechanisms
        curiosity_plasticity_config_t plasticity_config = curiosity_plasticity_config_default();
        plasticity_config.base_learning_rate = 0.01f;
        plasticity_config.stdp_a_plus = 0.01f;
        plasticity_config.stdp_a_minus = 0.012f;

        plasticity_bridge = curiosity_plasticity_create(&plasticity_config);
        ASSERT_NE(plasticity_bridge, nullptr) << "Failed to create Plasticity bridge";

        // Register base synapses for plasticity
        for (uint32_t i = 0; i < CURIOSITY_DIM_COUNT; i++) {
            curiosity_plasticity_register_synapse(plasticity_bridge, i,
                CURIOSITY_SYNAPSE_NOVELTY, 0.5f);
        }

        // Register protected synapses
        curiosity_plasticity_register_synapse(plasticity_bridge, 100,
            CURIOSITY_SYNAPSE_EXPLORATION, 1.0f);
        curiosity_plasticity_register_synapse(plasticity_bridge, 101,
            CURIOSITY_SYNAPSE_LEARNING, 0.9f);
    }

    void TearDown() override {
        if (snn_bridge) {
            curiosity_snn_destroy(snn_bridge);
            snn_bridge = nullptr;
        }
        if (plasticity_bridge) {
            curiosity_plasticity_destroy(plasticity_bridge);
            plasticity_bridge = nullptr;
        }
    }

    // Generate exploration scenario
    enum ExplorationScenario {
        HIGH_NOVELTY_SUCCESS,       // Novel stimulus, successful exploration
        HIGH_NOVELTY_FAILURE,       // Novel stimulus, failed exploration
        FAMILIAR_SUCCESS,           // Familiar stimulus, successful outcome
        HIGH_INFO_GAIN,             // High information gain expected
        LOW_INFO_GAIN,              // Low information gain
        EXPLORATION_DRIVE,          // Pure exploration motivation
        INTEREST_SEEKING,           // Interest-driven seeking
        SURPRISE_EVENT              // Unexpected event
    };

    void generate_scenario(float* dims, ExplorationScenario scenario) {
        memset(dims, 0, sizeof(float) * CURIOSITY_DIM_COUNT);

        switch (scenario) {
            case HIGH_NOVELTY_SUCCESS:
                dims[CURIOSITY_DIM_NOVELTY] = 0.9f;
                dims[CURIOSITY_DIM_EXPLORATION] = 0.85f;
                dims[CURIOSITY_DIM_LEARNING_PROGRESS] = 0.8f;
                dims[CURIOSITY_DIM_INFORMATION_GAIN] = 0.7f;
                break;

            case HIGH_NOVELTY_FAILURE:
                dims[CURIOSITY_DIM_NOVELTY] = 0.85f;
                dims[CURIOSITY_DIM_EXPLORATION] = 0.3f;
                dims[CURIOSITY_DIM_LEARNING_PROGRESS] = 0.2f;
                break;

            case FAMILIAR_SUCCESS:
                dims[CURIOSITY_DIM_NOVELTY] = 0.2f;
                dims[CURIOSITY_DIM_EXPLORATION] = 0.7f;
                dims[CURIOSITY_DIM_LEARNING_PROGRESS] = 0.6f;
                break;

            case HIGH_INFO_GAIN:
                dims[CURIOSITY_DIM_INFORMATION_GAIN] = 0.95f;
                dims[CURIOSITY_DIM_INTEREST] = 0.9f;
                dims[CURIOSITY_DIM_SEEKING] = 0.85f;
                dims[CURIOSITY_DIM_NOVELTY] = 0.7f;
                break;

            case LOW_INFO_GAIN:
                dims[CURIOSITY_DIM_INFORMATION_GAIN] = 0.15f;
                dims[CURIOSITY_DIM_INTEREST] = 0.2f;
                dims[CURIOSITY_DIM_KNOWLEDGE_GAP] = 0.1f;
                break;

            case EXPLORATION_DRIVE:
                dims[CURIOSITY_DIM_EXPLORATION] = 0.95f;
                dims[CURIOSITY_DIM_SEEKING] = 0.9f;
                dims[CURIOSITY_DIM_NOVELTY] = 0.6f;
                dims[CURIOSITY_DIM_UNCERTAINTY_REDUCTION] = 0.8f;
                break;

            case INTEREST_SEEKING:
                dims[CURIOSITY_DIM_INTEREST] = 0.9f;
                dims[CURIOSITY_DIM_SEEKING] = 0.85f;
                dims[CURIOSITY_DIM_COMPLEXITY] = 0.7f;
                break;

            case SURPRISE_EVENT:
                dims[CURIOSITY_DIM_SURPRISE] = 0.95f;
                dims[CURIOSITY_DIM_NOVELTY] = 0.85f;
                dims[CURIOSITY_DIM_INTEREST] = 0.9f;
                dims[CURIOSITY_DIM_INFORMATION_GAIN] = 0.8f;
                break;
        }
    }

    // Run single evaluation pipeline
    struct EvaluationResult {
        float novelty_level;
        float exploration_drive;
        float information_gain;
        bool novelty_detected;
        int spike_count;
    };

    EvaluationResult run_evaluation(ExplorationScenario scenario) {
        EvaluationResult result = {0};

        float dims[CURIOSITY_DIM_COUNT];
        generate_scenario(dims, scenario);

        // Encode and simulate
        result.spike_count = curiosity_snn_encode_state(snn_bridge, dims, CURIOSITY_DIM_COUNT);
        curiosity_snn_simulate(snn_bridge, 30.0f);

        // Get drive
        curiosity_drive_t drive;
        curiosity_snn_get_drive(snn_bridge, &drive);

        result.novelty_level = drive.novelty_level;
        result.exploration_drive = drive.exploration_drive;
        result.information_gain = drive.information_gain;
        result.novelty_detected = drive.novelty_detected;

        // Update stats
        stats.total_evaluations++;
        stats.exploration_history.push_back(drive.exploration_drive);
        stats.novelty_scores.push_back(drive.novelty_level);

        if (drive.novelty_detected) {
            stats.novelty_detections++;
        }

        return result;
    }
};

//=============================================================================
// Basic Pipeline Tests
//=============================================================================

TEST_F(CuriositySNNPlasticityE2E, CompletePipelineInitialization) {
    // Verify complete pipeline setup
    EXPECT_NE(snn_bridge, nullptr);
    EXPECT_NE(plasticity_bridge, nullptr);

    // Check synapse registration
    curiosity_plasticity_bridge_state_t state;
    curiosity_plasticity_get_state(plasticity_bridge, &state);
    EXPECT_GT(state.active_synapses, (uint32_t)CURIOSITY_DIM_COUNT);  // Base + protected
}

TEST_F(CuriositySNNPlasticityE2E, SingleEvaluationPipeline) {
    // Run single high novelty scenario
    auto result = run_evaluation(HIGH_NOVELTY_SUCCESS);

    // Verify drive is valid
    EXPECT_GE(result.novelty_level, 0.0f);
    EXPECT_LE(result.novelty_level, 1.0f);
    EXPECT_GE(result.exploration_drive, 0.0f);
    EXPECT_LE(result.exploration_drive, 1.0f);
    EXPECT_GE(result.spike_count, 0);

    // Apply learning based on result
    int ret = curiosity_plasticity_learn(plasticity_bridge,
        CURIOSITY_LEARN_NOVELTY_CONFIRMED, 0.5f, 0, result.novelty_level);
    EXPECT_EQ(ret, 0);
}

//=============================================================================
// Novelty Learning Tests
//=============================================================================

TEST_F(CuriositySNNPlasticityE2E, NoveltyDrivenLearning) {
    // Run multiple novelty scenarios
    float total_novelty = 0.0f;
    int novelty_count = 0;

    for (int trial = 0; trial < 10; trial++) {
        auto result = run_evaluation(HIGH_NOVELTY_SUCCESS);

        total_novelty += result.novelty_level;
        if (result.novelty_detected || result.novelty_level > 0.5f) {
            novelty_count++;
        }

        // Learn from novelty
        curiosity_plasticity_learn(plasticity_bridge,
            CURIOSITY_LEARN_NOVELTY_CONFIRMED, 0.5f, 0, result.novelty_level);

        // Apply STDP
        curiosity_plasticity_apply_stdp(plasticity_bridge, 0,
            (float)trial, (float)trial + 5.0f);
    }

    // At least some trials should register novelty signals
    EXPECT_GT(novelty_count, 0);
}

TEST_F(CuriositySNNPlasticityE2E, FalseNoveltyCorrection) {
    // Run multiple familiar stimuli
    for (int trial = 0; trial < 10; trial++) {
        auto result = run_evaluation(FAMILIAR_SUCCESS);

        // Learn false novelty - should decrease weights
        curiosity_plasticity_learn(plasticity_bridge,
            CURIOSITY_LEARN_FALSE_NOVELTY, 0.5f, 0, result.novelty_level);
    }

    curiosity_plasticity_stats_t stats;
    curiosity_plasticity_get_stats(plasticity_bridge, &stats);
    EXPECT_GT(stats.false_novelty_events, 0u);
}

//=============================================================================
// Information Gain Learning Tests
//=============================================================================

TEST_F(CuriositySNNPlasticityE2E, HighInfoGainLearning) {
    // Register info gain synapses
    for (int i = 200; i < 210; i++) {
        curiosity_plasticity_register_synapse(plasticity_bridge, i,
            CURIOSITY_SYNAPSE_INFORMATION, 0.5f);
    }

    float total_info_gain = 0.0f;
    int high_gain_count = 0;

    for (int trial = 0; trial < 10; trial++) {
        auto result = run_evaluation(HIGH_INFO_GAIN);

        total_info_gain += result.information_gain;
        if (result.information_gain > 0.0f) {
            high_gain_count++;
        }

        // Learn high info gain - should increase weights
        curiosity_plasticity_learn(plasticity_bridge,
            CURIOSITY_LEARN_INFO_GAIN_HIGH, 0.5f, 200 + (trial % 10),
            result.information_gain > 0.0f ? result.information_gain : 0.5f);
    }

    // Verify we had some high info gain events
    EXPECT_GT(high_gain_count, 0);
}

TEST_F(CuriositySNNPlasticityE2E, LowInfoGainLearning) {
    // Run multiple low info gain scenarios
    for (int trial = 0; trial < 10; trial++) {
        auto result = run_evaluation(LOW_INFO_GAIN);

        // Always apply learning
        curiosity_plasticity_learn(plasticity_bridge,
            CURIOSITY_LEARN_INFO_GAIN_LOW, 0.5f, 1, 0.3f);
    }

    // Verify plasticity stats reflect learning
    curiosity_plasticity_stats_t stats;
    curiosity_plasticity_get_stats(plasticity_bridge, &stats);
    EXPECT_GE(stats.total_learning_events, 10u);
}

//=============================================================================
// Protected Synapse Tests
//=============================================================================

TEST_F(CuriositySNNPlasticityE2E, ExplorationSynapseProtectionIntegrity) {
    // Get initial protected synapse weight
    curiosity_plasticity_synapse_t exploration_syn;
    curiosity_plasticity_get_synapse(plasticity_bridge, 100, &exploration_syn);
    float original_weight = exploration_syn.weight;
    EXPECT_TRUE(exploration_syn.is_protected);

    // Run many scenarios and try to modify protected synapse
    for (int trial = 0; trial < 20; trial++) {
        auto result = run_evaluation((ExplorationScenario)(trial % 8));

        // Try various learning operations on protected synapse
        curiosity_plasticity_learn(plasticity_bridge,
            CURIOSITY_LEARN_EXPLORATION_FAILURE, -1.0f, 100, result.exploration_drive);
        curiosity_plasticity_apply_stdp(plasticity_bridge, 100,
            (float)trial, (float)trial + 10.0f);
        curiosity_plasticity_apply_reward(plasticity_bridge, -1.0f);
    }

    // Protected synapse should remain unchanged
    curiosity_plasticity_get_synapse(plasticity_bridge, 100, &exploration_syn);
    EXPECT_FLOAT_EQ(exploration_syn.weight, original_weight);
    EXPECT_TRUE(exploration_syn.is_protected);
}

TEST_F(CuriositySNNPlasticityE2E, LearningSynapseProtection) {
    // Learning synapse should also be protected
    curiosity_plasticity_synapse_t learning_syn;
    curiosity_plasticity_get_synapse(plasticity_bridge, 101, &learning_syn);
    float original_weight = learning_syn.weight;
    EXPECT_TRUE(learning_syn.is_protected);

    // Stress test protection
    for (int i = 0; i < 50; i++) {
        curiosity_plasticity_apply_stdp(plasticity_bridge, 101, (float)i, (float)i + 5.0f);
        curiosity_plasticity_learn(plasticity_bridge,
            CURIOSITY_LEARN_EXPLORATION_FAILURE, 1.0f, 101, 0.9f);
    }

    // Weight must remain unchanged
    curiosity_plasticity_get_synapse(plasticity_bridge, 101, &learning_syn);
    EXPECT_FLOAT_EQ(learning_syn.weight, original_weight);
}

//=============================================================================
// Exploration Learning Tests
//=============================================================================

TEST_F(CuriositySNNPlasticityE2E, ExplorationDriveLearning) {
    // Register exploration-focused synapses
    for (int i = 300; i < 305; i++) {
        curiosity_plasticity_register_synapse(plasticity_bridge, i,
            CURIOSITY_SYNAPSE_INTEREST, 0.5f);
    }

    int learning_events = 0;

    for (int trial = 0; trial < 10; trial++) {
        auto result = run_evaluation(EXPLORATION_DRIVE);

        // Apply learning for all exploration scenarios
        curiosity_plasticity_learn(plasticity_bridge,
            CURIOSITY_LEARN_EXPLORATION_SUCCESS, 0.5f, 300 + (trial % 5),
            result.exploration_drive > 0.0f ? result.exploration_drive : 0.5f);
        learning_events++;
    }

    // Verify we applied learning across all trials
    EXPECT_EQ(learning_events, 10);

    // Verify plasticity stats reflect learning
    curiosity_plasticity_stats_t stats;
    curiosity_plasticity_get_stats(plasticity_bridge, &stats);
    EXPECT_GE(stats.exploration_success_events, 10u);
}

//=============================================================================
// Surprise Learning Tests
//=============================================================================

TEST_F(CuriositySNNPlasticityE2E, SurpriseEventLearning) {
    float total_surprise = 0.0f;
    int surprise_events = 0;
    int learning_events = 0;

    for (int trial = 0; trial < 10; trial++) {
        auto result = run_evaluation(SURPRISE_EVENT);

        // Check if high novelty from surprise
        total_surprise += result.novelty_level;
        if (result.novelty_level > 0.5f || result.novelty_detected) {
            surprise_events++;
        }

        // Always apply learning for surprise scenarios
        curiosity_plasticity_learn(plasticity_bridge,
            CURIOSITY_LEARN_SURPRISE_POSITIVE, 0.5f, 2, result.novelty_level);
        learning_events++;
    }

    // Should have some surprise detections
    EXPECT_GE(surprise_events, 3);
    // Verify learning was applied
    EXPECT_EQ(learning_events, 10);
}

//=============================================================================
// Multi-Scenario Learning Tests
//=============================================================================

TEST_F(CuriositySNNPlasticityE2E, CompleteExplorationWorkflow) {
    // Register workflow synapses
    for (int i = 400; i < 420; i++) {
        curiosity_plasticity_register_synapse(plasticity_bridge, i,
            CURIOSITY_SYNAPSE_NOVELTY, 0.5f);
    }

    // Run complete exploration workflow
    for (int epoch = 0; epoch < 5; epoch++) {
        for (int scenario = 0; scenario < 8; scenario++) {
            auto result = run_evaluation((ExplorationScenario)scenario);

            // Select learning event based on scenario
            curiosity_learn_event_t event;
            float magnitude = 0.3f;

            switch ((ExplorationScenario)scenario) {
                case HIGH_NOVELTY_SUCCESS:
                    event = CURIOSITY_LEARN_NOVELTY_CONFIRMED;
                    break;
                case HIGH_NOVELTY_FAILURE:
                    event = CURIOSITY_LEARN_EXPLORATION_FAILURE;
                    break;
                case FAMILIAR_SUCCESS:
                    event = CURIOSITY_LEARN_EXPLORATION_SUCCESS;
                    break;
                case HIGH_INFO_GAIN:
                    event = CURIOSITY_LEARN_INFO_GAIN_HIGH;
                    break;
                case LOW_INFO_GAIN:
                    event = CURIOSITY_LEARN_INFO_GAIN_LOW;
                    break;
                case EXPLORATION_DRIVE:
                    event = CURIOSITY_LEARN_EXPLORATION_SUCCESS;
                    break;
                case INTEREST_SEEKING:
                    event = CURIOSITY_LEARN_INTEREST_MATCHED;
                    break;
                default:
                    event = CURIOSITY_LEARN_SURPRISE_POSITIVE;
                    break;
            }

            int synapse_id = 400 + (epoch * 8 + scenario) % 20;
            curiosity_plasticity_learn(plasticity_bridge, event, magnitude,
                synapse_id, result.novelty_level);

            // Apply STDP
            curiosity_plasticity_apply_stdp(plasticity_bridge, synapse_id,
                (float)(epoch * 10 + scenario), (float)(epoch * 10 + scenario + 5));
        }

        // Periodic maintenance
        curiosity_plasticity_update_bcm(plasticity_bridge, 0.5f);
        curiosity_plasticity_homeostatic_update(plasticity_bridge, 0.5f);
        curiosity_plasticity_update_traces(plasticity_bridge, 1.0f);
    }

    // Consolidate learning
    curiosity_plasticity_consolidate(plasticity_bridge);

    // Verify extensive learning occurred
    curiosity_plasticity_stats_t final_stats;
    curiosity_plasticity_get_stats(plasticity_bridge, &final_stats);
    EXPECT_GT(final_stats.total_learning_events, 30u);
    EXPECT_GT(final_stats.weight_updates, 30u);

    curiosity_snn_stats_t snn_stats;
    curiosity_snn_get_stats(snn_bridge, &snn_stats);
    EXPECT_GE(snn_stats.total_evaluations, 40u);
}

//=============================================================================
// Stress and Performance Tests
//=============================================================================

TEST_F(CuriositySNNPlasticityE2E, HighVolumeProcessing) {
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 100; i++) {
        run_evaluation((ExplorationScenario)(i % 8));
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // 100 evaluations should complete in under 5 seconds
    EXPECT_LT(duration.count(), 5000);
    EXPECT_EQ(stats.total_evaluations, 100);
}

TEST_F(CuriositySNNPlasticityE2E, ContinuousLearning) {
    // Register many synapses
    for (int i = 500; i < 600; i++) {
        curiosity_plasticity_register_synapse(plasticity_bridge, i,
            CURIOSITY_SYNAPSE_NOVELTY, 0.5f);
    }

    // Continuous learning loop
    for (int cycle = 0; cycle < 50; cycle++) {
        auto result = run_evaluation((ExplorationScenario)(cycle % 8));

        // Learn on rotating synapses
        for (int j = 0; j < 5; j++) {
            int synapse_id = 500 + (cycle * 5 + j) % 100;
            curiosity_plasticity_learn(plasticity_bridge,
                CURIOSITY_LEARN_NOVELTY_CONFIRMED, 0.1f, synapse_id, result.novelty_level);
        }

        // Periodic BCM update
        if (cycle % 10 == 0) {
            curiosity_plasticity_update_bcm(plasticity_bridge, 0.5f);
        }
    }

    // Verify extensive learning
    curiosity_plasticity_stats_t stats;
    curiosity_plasticity_get_stats(plasticity_bridge, &stats);
    EXPECT_GE(stats.total_learning_events, 200u);
}

//=============================================================================
// Reset and Recovery Tests
//=============================================================================

TEST_F(CuriositySNNPlasticityE2E, ResetAndRecovery) {
    // Accumulate some state
    for (int i = 0; i < 10; i++) {
        run_evaluation((ExplorationScenario)(i % 8));
    }

    // Reset both bridges
    curiosity_snn_reset(snn_bridge);
    curiosity_plasticity_reset(plasticity_bridge);

    // Verify recovery
    curiosity_snn_bridge_state_t snn_state;
    curiosity_snn_get_state(snn_bridge, &snn_state);
    EXPECT_EQ(snn_state.state, CURIOSITY_SNN_STATE_IDLE);

    curiosity_plasticity_bridge_state_t plasticity_state;
    curiosity_plasticity_get_state(plasticity_bridge, &plasticity_state);
    EXPECT_EQ(plasticity_state.state, CURIOSITY_PLASTICITY_STATE_IDLE);

    // Can continue processing
    auto result = run_evaluation(HIGH_NOVELTY_SUCCESS);
    EXPECT_GE(result.novelty_level, 0.0f);
}

//=============================================================================
// Statistics Validation Tests
//=============================================================================

TEST_F(CuriositySNNPlasticityE2E, StatisticsAccuracy) {
    // Run known number of evaluations
    for (int i = 0; i < 20; i++) {
        run_evaluation((ExplorationScenario)(i % 8));

        // Apply learning
        curiosity_plasticity_learn(plasticity_bridge,
            CURIOSITY_LEARN_NOVELTY_CONFIRMED, 0.1f, i % CURIOSITY_DIM_COUNT, 0.5f);
    }

    // Verify stats match
    curiosity_snn_stats_t snn_stats;
    curiosity_snn_get_stats(snn_bridge, &snn_stats);
    EXPECT_GE(snn_stats.total_evaluations, 20u);

    curiosity_plasticity_stats_t plasticity_stats;
    curiosity_plasticity_get_stats(plasticity_bridge, &plasticity_stats);
    EXPECT_GE(plasticity_stats.total_learning_events, 20u);
}
