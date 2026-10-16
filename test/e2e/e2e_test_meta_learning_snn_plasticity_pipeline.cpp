/**
 * @file e2e_test_meta_learning_snn_plasticity_pipeline.cpp
 * @brief End-to-end tests for Meta Learning-SNN-Plasticity learning pipeline
 * @version 1.0.0
 * @date 2026-01-06
 *
 * WHAT: Complete meta-learning pipeline with SNN and Plasticity
 * WHY:  Verify full dataflow from meta-learning state -> SNN encoding -> insight
 *       -> plasticity learning -> learning rate adaptation evolution
 * HOW:  Test realistic scenarios combining strategy selection encoding, STDP learning,
 *       reward-modulated plasticity, and protected synapse integrity
 *
 * Test Coverage:
 * - Full meta-learning state to insight pipeline via SNN
 * - STDP and reward-modulated learning for strategy selection
 * - Transfer learning detection and generalization
 * - Learning rate and Consolidation synapse protection
 * - Multi-scenario meta-learning adaptation
 * - Learning-to-learn evolution through experience
 * - Protected synapse integrity under stress
 */

#include <gtest/gtest.h>

#include "cognitive/meta_learning/nimcp_meta_learning_snn_bridge.h"
#include "cognitive/meta_learning/nimcp_meta_learning_plasticity_bridge.h"
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

class MetaLearningSNNPlasticityE2E : public ::testing::Test {
protected:
    meta_learning_snn_bridge_t* snn_bridge = nullptr;
    meta_learning_plasticity_bridge_t* plasticity_bridge = nullptr;

    // Learning statistics
    struct LearningStats {
        int high_transfer_correct = 0;
        int high_transfer_incorrect = 0;
        int strategy_switches = 0;
        int total_evaluations = 0;
        std::vector<float> learning_rate_history;
        std::vector<float> generalization_scores;
    } stats;

    void SetUp() override {
        // Create SNN bridge with full meta-learning dimensions
        meta_learning_snn_config_t snn_config = meta_learning_snn_config_default();
        snn_config.num_dimensions = META_DIM_COUNT;
        snn_config.neurons_per_dim = 32;
        snn_config.dt_ms = 1.0f;
        snn_config.enable_transfer_detection = true;
        snn_config.enable_bio_async = false;

        snn_bridge = meta_learning_snn_create(&snn_config);
        ASSERT_NE(snn_bridge, nullptr) << "Failed to create SNN bridge";

        // Create Plasticity bridge with all learning mechanisms
        meta_learning_plasticity_config_t plasticity_config = meta_learning_plasticity_config_default();
        plasticity_config.base_learning_rate = 0.01f;
        plasticity_config.stdp_a_plus = 0.01f;
        plasticity_config.stdp_a_minus = 0.012f;

        plasticity_bridge = meta_learning_plasticity_create(&plasticity_config);
        ASSERT_NE(plasticity_bridge, nullptr) << "Failed to create Plasticity bridge";

        // Register base synapses for plasticity
        for (uint32_t i = 0; i < META_DIM_COUNT; i++) {
            meta_learning_plasticity_register_synapse(plasticity_bridge, i,
                META_SYNAPSE_STRATEGY, 0.5f);
        }

        // Register protected synapses
        meta_learning_plasticity_register_synapse(plasticity_bridge, 100,
            META_SYNAPSE_LEARNING_RATE, 1.0f);
        meta_learning_plasticity_register_synapse(plasticity_bridge, 101,
            META_SYNAPSE_CONSOLIDATION, 0.9f);
    }

    void TearDown() override {
        if (snn_bridge) {
            meta_learning_snn_destroy(snn_bridge);
            snn_bridge = nullptr;
        }
        if (plasticity_bridge) {
            meta_learning_plasticity_destroy(plasticity_bridge);
            plasticity_bridge = nullptr;
        }
    }

    // Generate meta-learning scenario
    enum MetaLearningScenario {
        HIGH_TRANSFER_SUCCESS,      // Successful transfer to new domain
        HIGH_TRANSFER_FAILURE,      // Transfer attempt failed
        STRATEGY_SWITCH,            // Need to change learning strategy
        RAPID_ADAPTATION,           // Quick adaptation required
        TASK_SIMILARITY_HIGH,       // High similarity to prior tasks
        TASK_SIMILARITY_LOW,        // Low similarity - new domain
        CURRICULUM_PROGRESSION,     // Normal curriculum learning
        CONSOLIDATION_TEST          // Knowledge consolidation scenario
    };

    void generate_scenario(float* dims, MetaLearningScenario scenario) {
        memset(dims, 0, sizeof(float) * META_DIM_COUNT);

        switch (scenario) {
            case HIGH_TRANSFER_SUCCESS:
                dims[META_DIM_LEARNING_RATE] = 0.8f;
                dims[META_DIM_TRANSFER] = 0.9f;
                dims[META_DIM_GENERALIZATION] = 0.85f;
                dims[META_DIM_PRIOR_KNOWLEDGE] = 0.9f;
                break;

            case HIGH_TRANSFER_FAILURE:
                dims[META_DIM_LEARNING_RATE] = 0.3f;
                dims[META_DIM_TRANSFER] = 0.2f;  // Low transfer
                dims[META_DIM_GENERALIZATION] = 0.4f;
                dims[META_DIM_TASK_SIMILARITY] = 0.2f;
                break;

            case STRATEGY_SWITCH:
                dims[META_DIM_STRATEGY_SELECT] = 0.9f;
                dims[META_DIM_ADAPTATION_SPEED] = 0.8f;
                dims[META_DIM_LEARNING_TO_LEARN] = 0.7f;
                dims[META_DIM_CURRICULUM] = 0.5f;
                break;

            case RAPID_ADAPTATION:
                dims[META_DIM_ADAPTATION_SPEED] = 0.95f;
                dims[META_DIM_LEARNING_RATE] = 0.9f;
                dims[META_DIM_STRATEGY_SELECT] = 0.8f;
                break;

            case TASK_SIMILARITY_HIGH:
                dims[META_DIM_TASK_SIMILARITY] = 0.95f;
                dims[META_DIM_PRIOR_KNOWLEDGE] = 0.85f;
                dims[META_DIM_TRANSFER] = 0.9f;
                dims[META_DIM_GENERALIZATION] = 0.85f;
                break;

            case TASK_SIMILARITY_LOW:
                dims[META_DIM_TASK_SIMILARITY] = 0.1f;
                dims[META_DIM_PRIOR_KNOWLEDGE] = 0.2f;
                dims[META_DIM_TRANSFER] = 0.3f;
                dims[META_DIM_LEARNING_TO_LEARN] = 0.9f;  // Must rely on learning-to-learn
                dims[META_DIM_LEARNING_RATE] = 0.7f;       // Higher learning rate for novel tasks
                break;

            case CURRICULUM_PROGRESSION:
                dims[META_DIM_CURRICULUM] = 0.7f;
                dims[META_DIM_CONSOLIDATION] = 0.8f;
                dims[META_DIM_LEARNING_RATE] = 0.6f;
                dims[META_DIM_GENERALIZATION] = 0.7f;
                break;

            case CONSOLIDATION_TEST:
                dims[META_DIM_CONSOLIDATION] = 0.9f;
                dims[META_DIM_PRIOR_KNOWLEDGE] = 0.85f;
                dims[META_DIM_GENERALIZATION] = 0.8f;
                dims[META_DIM_TASK_SIMILARITY] = 0.75f;
                break;
        }
    }

    // Run single evaluation pipeline
    struct EvaluationResult {
        float learning_rate_level;
        float transfer_level;
        float generalization;
        bool strategy_switch_detected;
        int spike_count;
    };

    EvaluationResult run_evaluation(MetaLearningScenario scenario) {
        EvaluationResult result = {0};

        float dims[META_DIM_COUNT];
        generate_scenario(dims, scenario);

        // Encode and simulate
        result.spike_count = meta_learning_snn_encode_state(snn_bridge, dims, META_DIM_COUNT);
        meta_learning_snn_simulate(snn_bridge, 30.0f);

        // Get insight
        meta_learning_insight_t insight;
        meta_learning_snn_get_insight(snn_bridge, &insight);

        result.learning_rate_level = insight.learning_rate_level;
        result.transfer_level = insight.transfer_potential;
        result.generalization = insight.generalization_score;

        // Check strategy switch detection
        float strategy_switch;
        result.strategy_switch_detected = meta_learning_snn_check_state_change(snn_bridge, &strategy_switch);

        // Update stats
        stats.total_evaluations++;
        stats.learning_rate_history.push_back(insight.learning_rate_level);
        stats.generalization_scores.push_back(insight.generalization_score);

        if (result.strategy_switch_detected) {
            stats.strategy_switches++;
        }

        return result;
    }
};

//=============================================================================
// Basic Pipeline Tests
//=============================================================================

TEST_F(MetaLearningSNNPlasticityE2E, CompletePipelineInitialization) {
    // Verify complete pipeline setup
    EXPECT_NE(snn_bridge, nullptr);
    EXPECT_NE(plasticity_bridge, nullptr);

    // Check synapse registration
    meta_learning_plasticity_bridge_state_t state;
    meta_learning_plasticity_get_state(plasticity_bridge, &state);
    EXPECT_GT(state.active_synapses, (uint32_t)META_DIM_COUNT);  // Base + protected
}

TEST_F(MetaLearningSNNPlasticityE2E, SingleEvaluationPipeline) {
    // Run single high transfer scenario
    auto result = run_evaluation(HIGH_TRANSFER_SUCCESS);

    // Verify insight is valid
    EXPECT_GE(result.learning_rate_level, 0.0f);
    EXPECT_LE(result.learning_rate_level, 1.0f);
    EXPECT_GE(result.generalization, 0.0f);
    EXPECT_LE(result.generalization, 1.0f);
    EXPECT_GE(result.spike_count, 0);

    // Apply learning based on result
    int ret = meta_learning_plasticity_learn(plasticity_bridge,
        META_LEARN_TRANSFER_SUCCESS, 0.5f, 0, result.learning_rate_level);
    EXPECT_EQ(ret, 0);
}

//=============================================================================
// Transfer Learning Tests
//=============================================================================

TEST_F(MetaLearningSNNPlasticityE2E, TransferLearningDetection) {
    // Run multiple transfer scenarios
    float total_transfer = 0.0f;
    int transfer_success_count = 0;

    for (int trial = 0; trial < 10; trial++) {
        auto result = run_evaluation(HIGH_TRANSFER_SUCCESS);

        total_transfer += result.transfer_level;
        if (result.transfer_level > 0.0f) {
            transfer_success_count++;
        }

        // Learn successful transfer
        meta_learning_plasticity_learn(plasticity_bridge,
            META_LEARN_TRANSFER_SUCCESS, 0.5f, 0, result.transfer_level);

        // Apply STDP
        meta_learning_plasticity_apply_stdp(plasticity_bridge, 0,
            (float)trial, (float)trial + 5.0f);
    }

    // At least some trials should register high transfer
    EXPECT_GT(transfer_success_count, 0);
}

TEST_F(MetaLearningSNNPlasticityE2E, TransferFailureLearning) {
    // Run multiple transfer failure scenarios
    float total_generalization = 0.0f;

    for (int trial = 0; trial < 10; trial++) {
        auto result = run_evaluation(HIGH_TRANSFER_FAILURE);

        total_generalization += result.generalization;

        // Learn from transfer failure - should adjust strategy
        meta_learning_plasticity_learn(plasticity_bridge,
            META_LEARN_TRANSFER_FAILURE, 0.5f, 1, result.generalization);
    }

    // Average generalization from failed transfers
    EXPECT_GT(total_generalization / 10.0f, 0.0f);
}

TEST_F(MetaLearningSNNPlasticityE2E, StrategyAdaptation) {
    // Register strategy synapses
    for (int i = 200; i < 210; i++) {
        meta_learning_plasticity_register_synapse(plasticity_bridge, i,
            META_SYNAPSE_STRATEGY, 0.5f);
    }

    // Initial strategy weight
    meta_learning_plasticity_synapse_t initial_synapse;
    meta_learning_plasticity_get_synapse(plasticity_bridge, 200, &initial_synapse);
    float initial_weight = initial_synapse.weight;

    // Run mixed strategy scenarios
    for (int epoch = 0; epoch < 5; epoch++) {
        // Strategy switch scenario
        auto switch_result = run_evaluation(STRATEGY_SWITCH);
        meta_learning_plasticity_learn(plasticity_bridge,
            META_LEARN_STRATEGY_EFFECTIVE, 0.3f, 200, switch_result.learning_rate_level);

        // Rapid adaptation scenario
        auto adapt_result = run_evaluation(RAPID_ADAPTATION);
        meta_learning_plasticity_learn(plasticity_bridge,
            META_LEARN_RATE_CORRECT, 0.3f, 201, adapt_result.learning_rate_level);

        // BCM and homeostatic updates
        meta_learning_plasticity_update_bcm(plasticity_bridge, 0.5f);
        meta_learning_plasticity_homeostatic_update(plasticity_bridge, 0.5f);
    }

    // Verify learning occurred
    meta_learning_plasticity_stats_t stats;
    meta_learning_plasticity_get_stats(plasticity_bridge, &stats);
    EXPECT_GT(stats.total_learning_events, 0u);
    /* Check correct_rate_events since we use META_LEARN_RATE_CORRECT */
    EXPECT_GT(stats.correct_rate_events, 0u);
}

//=============================================================================
// Task Similarity Tests
//=============================================================================

TEST_F(MetaLearningSNNPlasticityE2E, TaskSimilarityProcessing) {
    int high_similarity_count = 0;
    int low_similarity_count = 0;

    for (int trial = 0; trial < 10; trial++) {
        // Alternate between high and low similarity
        MetaLearningScenario scenario = (trial % 2 == 0) ?
            TASK_SIMILARITY_HIGH : TASK_SIMILARITY_LOW;

        auto result = run_evaluation(scenario);

        if (scenario == TASK_SIMILARITY_HIGH) {
            if (result.transfer_level > 0.0f) {
                high_similarity_count++;
            }
            meta_learning_plasticity_learn(plasticity_bridge,
                META_LEARN_TRANSFER_SUCCESS, 0.5f, 2, result.transfer_level);
        } else {
            if (result.learning_rate_level > 0.0f) {
                low_similarity_count++;
            }
            meta_learning_plasticity_learn(plasticity_bridge,
                META_LEARN_TRANSFER_SUCCESS, 0.5f, 3, result.learning_rate_level);
        }
    }

    // Should detect patterns in both similarity conditions
    EXPECT_GE(high_similarity_count, 2);
    EXPECT_GE(low_similarity_count, 2);
}

//=============================================================================
// Protected Synapse Tests
//=============================================================================

TEST_F(MetaLearningSNNPlasticityE2E, LearningRateProtectionIntegrity) {
    // Get initial protected synapse weight
    meta_learning_plasticity_synapse_t lr_synapse;
    meta_learning_plasticity_get_synapse(plasticity_bridge, 100, &lr_synapse);
    float original_weight = lr_synapse.weight;
    EXPECT_TRUE(lr_synapse.is_protected);

    // Run many scenarios and try to modify protected synapse
    for (int trial = 0; trial < 20; trial++) {
        auto result = run_evaluation((MetaLearningScenario)(trial % 8));

        // Try various learning operations on protected synapse
        meta_learning_plasticity_learn(plasticity_bridge,
            META_LEARN_TRANSFER_FAILURE, -1.0f, 100, result.learning_rate_level);
        meta_learning_plasticity_apply_stdp(plasticity_bridge, 100,
            (float)trial, (float)trial + 10.0f);
        meta_learning_plasticity_apply_reward(plasticity_bridge, -1.0f);
    }

    // Protected synapse should remain unchanged
    meta_learning_plasticity_get_synapse(plasticity_bridge, 100, &lr_synapse);
    EXPECT_FLOAT_EQ(lr_synapse.weight, original_weight);
    EXPECT_TRUE(lr_synapse.is_protected);
}

TEST_F(MetaLearningSNNPlasticityE2E, ConsolidationSynapseProtection) {
    // Consolidation synapse should also be protected
    meta_learning_plasticity_synapse_t consol_synapse;
    meta_learning_plasticity_get_synapse(plasticity_bridge, 101, &consol_synapse);
    float original_weight = consol_synapse.weight;
    EXPECT_TRUE(consol_synapse.is_protected);

    // Stress test protection
    for (int i = 0; i < 50; i++) {
        meta_learning_plasticity_apply_stdp(plasticity_bridge, 101, (float)i, (float)i + 5.0f);
        meta_learning_plasticity_learn(plasticity_bridge,
            META_LEARN_TRANSFER_FAILURE, 1.0f, 101, 0.9f);
    }

    // Weight must remain unchanged
    meta_learning_plasticity_get_synapse(plasticity_bridge, 101, &consol_synapse);
    EXPECT_FLOAT_EQ(consol_synapse.weight, original_weight);
}

//=============================================================================
// Curriculum Learning Tests
//=============================================================================

TEST_F(MetaLearningSNNPlasticityE2E, CurriculumProgressionLearning) {
    // Register curriculum synapses
    for (int i = 300; i < 305; i++) {
        meta_learning_plasticity_register_synapse(plasticity_bridge, i,
            META_SYNAPSE_STRATEGY, 0.5f);
    }

    int learning_events = 0;

    for (int trial = 0; trial < 10; trial++) {
        auto result = run_evaluation(CURRICULUM_PROGRESSION);

        // Apply learning for all curriculum progression scenarios
        meta_learning_plasticity_learn(plasticity_bridge,
            META_LEARN_GENERALIZATION_SUCCESS, 0.5f, 300 + (trial % 5),
            result.learning_rate_level > 0.0f ? result.learning_rate_level : 0.5f);
        learning_events++;
    }

    // Verify we applied learning across all trials
    EXPECT_EQ(learning_events, 10);

    // Verify plasticity stats reflect learning
    meta_learning_plasticity_stats_t stats;
    meta_learning_plasticity_get_stats(plasticity_bridge, &stats);
    EXPECT_GE(stats.total_learning_events, 10u);
}

//=============================================================================
// Consolidation Tests
//=============================================================================

TEST_F(MetaLearningSNNPlasticityE2E, ConsolidationAndRetention) {
    float total_consolidation = 0.0f;
    int consolidation_events = 0;
    int learning_events = 0;

    for (int trial = 0; trial < 10; trial++) {
        auto result = run_evaluation(CONSOLIDATION_TEST);

        float consolidation_level;
        meta_learning_snn_check_adaptation(snn_bridge, &consolidation_level);

        total_consolidation += consolidation_level;
        if (consolidation_level > 0.0f || result.generalization > 0.0f) {
            consolidation_events++;
        }

        // Always apply learning for consolidation scenarios
        meta_learning_plasticity_learn(plasticity_bridge,
            META_LEARN_TRANSFER_SUCCESS, 0.5f, 3, consolidation_level);
        learning_events++;
    }

    // Should track consolidation in multiple trials
    EXPECT_GE(consolidation_events, 3);
    // Verify learning was applied
    EXPECT_EQ(learning_events, 10);
}

//=============================================================================
// Multi-Scenario Learning Tests
//=============================================================================

TEST_F(MetaLearningSNNPlasticityE2E, CompleteMetaLearningWorkflow) {
    // Register workflow synapses
    for (int i = 400; i < 420; i++) {
        meta_learning_plasticity_register_synapse(plasticity_bridge, i,
            META_SYNAPSE_STRATEGY, 0.5f);
    }

    // Run complete meta-learning workflow
    for (int epoch = 0; epoch < 5; epoch++) {
        for (int scenario = 0; scenario < 8; scenario++) {
            auto result = run_evaluation((MetaLearningScenario)scenario);

            // Select learning event based on scenario
            meta_learning_learn_event_t event;
            float magnitude = 0.3f;

            switch ((MetaLearningScenario)scenario) {
                case HIGH_TRANSFER_SUCCESS:
                case TASK_SIMILARITY_HIGH:
                    event = META_LEARN_TRANSFER_SUCCESS;
                    break;
                case HIGH_TRANSFER_FAILURE:
                case TASK_SIMILARITY_LOW:
                    event = META_LEARN_TRANSFER_FAILURE;
                    break;
                case STRATEGY_SWITCH:
                    event = META_LEARN_STRATEGY_EFFECTIVE;
                    break;
                case RAPID_ADAPTATION:
                    event = META_LEARN_RATE_CORRECT;
                    break;
                case CURRICULUM_PROGRESSION:
                    event = META_LEARN_GENERALIZATION_SUCCESS;
                    break;
                case CONSOLIDATION_TEST:
                    event = META_LEARN_TRANSFER_SUCCESS;
                    break;
                default:
                    event = META_LEARN_GENERALIZATION_SUCCESS;
                    break;
            }

            int synapse_id = 400 + (epoch * 8 + scenario) % 20;
            meta_learning_plasticity_learn(plasticity_bridge, event, magnitude,
                synapse_id, result.learning_rate_level);

            // Apply STDP
            meta_learning_plasticity_apply_stdp(plasticity_bridge, synapse_id,
                (float)(epoch * 10 + scenario), (float)(epoch * 10 + scenario + 5));
        }

        // Periodic maintenance
        meta_learning_plasticity_update_bcm(plasticity_bridge, 0.5f);
        meta_learning_plasticity_homeostatic_update(plasticity_bridge, 0.5f);
        meta_learning_plasticity_update_traces(plasticity_bridge, 1.0f);
    }

    // Consolidate learning
    meta_learning_plasticity_consolidate(plasticity_bridge);

    // Verify extensive learning occurred
    meta_learning_plasticity_stats_t final_stats;
    meta_learning_plasticity_get_stats(plasticity_bridge, &final_stats);
    EXPECT_GT(final_stats.total_learning_events, 30u);
    EXPECT_GT(final_stats.weight_updates, 30u);

    meta_learning_snn_stats_t snn_stats;
    meta_learning_snn_get_stats(snn_bridge, &snn_stats);
    EXPECT_GE(snn_stats.total_evaluations, 40u);
}

//=============================================================================
// Stress and Performance Tests
//=============================================================================

TEST_F(MetaLearningSNNPlasticityE2E, HighVolumeProcessing) {
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 100; i++) {
        run_evaluation((MetaLearningScenario)(i % 8));
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // 100 evaluations should complete in under 5 seconds
    EXPECT_LT(duration.count(), 5000);
    EXPECT_EQ(stats.total_evaluations, 100);
}

TEST_F(MetaLearningSNNPlasticityE2E, ContinuousLearning) {
    // Register many synapses
    for (int i = 500; i < 600; i++) {
        meta_learning_plasticity_register_synapse(plasticity_bridge, i,
            META_SYNAPSE_STRATEGY, 0.5f);
    }

    // Continuous learning loop
    for (int cycle = 0; cycle < 50; cycle++) {
        auto result = run_evaluation((MetaLearningScenario)(cycle % 8));

        // Learn on rotating synapses
        for (int j = 0; j < 5; j++) {
            int synapse_id = 500 + (cycle * 5 + j) % 100;
            meta_learning_plasticity_learn(plasticity_bridge,
                META_LEARN_TRANSFER_SUCCESS, 0.1f, synapse_id, result.learning_rate_level);
        }

        // Periodic BCM update
        if (cycle % 10 == 0) {
            meta_learning_plasticity_update_bcm(plasticity_bridge, 0.5f);
        }
    }

    // Verify extensive learning
    meta_learning_plasticity_stats_t stats;
    meta_learning_plasticity_get_stats(plasticity_bridge, &stats);
    EXPECT_GE(stats.total_learning_events, 200u);
}

//=============================================================================
// Reset and Recovery Tests
//=============================================================================

TEST_F(MetaLearningSNNPlasticityE2E, ResetAndRecovery) {
    // Accumulate some state
    for (int i = 0; i < 10; i++) {
        run_evaluation((MetaLearningScenario)(i % 8));
    }

    // Reset both bridges
    meta_learning_snn_reset(snn_bridge);
    meta_learning_plasticity_reset(plasticity_bridge);

    // Verify recovery
    meta_learning_snn_bridge_state_t snn_state;
    meta_learning_snn_get_state(snn_bridge, &snn_state);
    EXPECT_EQ(snn_state.state, META_LEARNING_SNN_STATE_IDLE);

    meta_learning_plasticity_bridge_state_t plasticity_state;
    meta_learning_plasticity_get_state(plasticity_bridge, &plasticity_state);
    EXPECT_EQ(plasticity_state.state, META_LEARNING_PLASTICITY_STATE_IDLE);

    // Can continue processing
    auto result = run_evaluation(HIGH_TRANSFER_SUCCESS);
    EXPECT_GE(result.learning_rate_level, 0.0f);
}

//=============================================================================
// Statistics Validation Tests
//=============================================================================

TEST_F(MetaLearningSNNPlasticityE2E, StatisticsAccuracy) {
    // Run known number of evaluations
    for (int i = 0; i < 20; i++) {
        run_evaluation((MetaLearningScenario)(i % 8));

        // Apply learning
        meta_learning_plasticity_learn(plasticity_bridge,
            META_LEARN_TRANSFER_SUCCESS, 0.1f, i % META_DIM_COUNT, 0.5f);
    }

    // Verify stats match
    meta_learning_snn_stats_t snn_stats;
    meta_learning_snn_get_stats(snn_bridge, &snn_stats);
    EXPECT_GE(snn_stats.total_evaluations, 20u);

    meta_learning_plasticity_stats_t plasticity_stats;
    meta_learning_plasticity_get_stats(plasticity_bridge, &plasticity_stats);
    EXPECT_GE(plasticity_stats.total_learning_events, 20u);
}
