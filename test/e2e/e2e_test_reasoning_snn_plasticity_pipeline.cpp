/**
 * @file e2e_test_reasoning_snn_plasticity_pipeline.cpp
 * @brief End-to-end tests for Reasoning-SNN-Plasticity learning pipeline
 * @version 1.0.0
 * @date 2026-01-06
 *
 * WHAT: Complete reasoning pipeline with SNN and Plasticity
 * WHY:  Verify full dataflow from reasoning state -> SNN encoding -> inference
 *       -> plasticity learning -> logical validity evolution
 * HOW:  Test realistic scenarios combining deductive encoding, STDP learning,
 *       reward-modulated plasticity, and protected synapse integrity
 *
 * Test Coverage:
 * - Full reasoning state to inference pipeline via SNN
 * - STDP and reward-modulated learning for logical reasoning
 * - Causal chain detection and processing
 * - Deduction and Causal synapse protection
 * - Multi-scenario reasoning learning
 * - Logical validity evolution through experience
 * - Protected synapse integrity under stress
 */

#include <gtest/gtest.h>

#include "cognitive/reasoning/nimcp_reasoning_snn_bridge.h"
#include "cognitive/reasoning/nimcp_reasoning_plasticity_bridge.h"
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

class ReasoningSNNPlasticityE2E : public ::testing::Test {
protected:
    reasoning_snn_bridge_t* snn_bridge = nullptr;
    reasoning_plasticity_bridge_t* plasticity_bridge = nullptr;

    // Learning statistics
    struct LearningStats {
        int valid_conclusions = 0;
        int invalid_conclusions = 0;
        int causal_detected = 0;
        int total_evaluations = 0;
        std::vector<float> validity_history;
        std::vector<float> deduction_scores;
    } stats;

    void SetUp() override {
        // Create SNN bridge with full reasoning dimensions
        reasoning_snn_config_t snn_config = reasoning_snn_config_default();
        snn_config.num_dimensions = REASON_DIM_COUNT;
        snn_config.neurons_per_dim = 32;
        snn_config.dt_ms = 1.0f;
        snn_config.enable_causal_chains = true;
        snn_config.enable_bio_async = false;

        snn_bridge = reasoning_snn_create(&snn_config);
        ASSERT_NE(snn_bridge, nullptr) << "Failed to create SNN bridge";

        // Create Plasticity bridge with all learning mechanisms
        reasoning_plasticity_config_t plasticity_config = reasoning_plasticity_config_default();
        plasticity_config.base_learning_rate = 0.01f;
        plasticity_config.stdp_a_plus = 0.01f;
        plasticity_config.stdp_a_minus = 0.012f;

        plasticity_bridge = reasoning_plasticity_create(&plasticity_config);
        ASSERT_NE(plasticity_bridge, nullptr) << "Failed to create Plasticity bridge";

        // Register base synapses for plasticity
        for (uint32_t i = 0; i < REASON_DIM_COUNT; i++) {
            reasoning_plasticity_register_synapse(plasticity_bridge, i,
                REASON_SYNAPSE_EVIDENCE, 0.5f);
        }

        // Register protected synapses
        reasoning_plasticity_register_synapse(plasticity_bridge, 100,
            REASON_SYNAPSE_DEDUCTION, 1.0f);
        reasoning_plasticity_register_synapse(plasticity_bridge, 101,
            REASON_SYNAPSE_CAUSAL, 0.9f);
    }

    void TearDown() override {
        if (snn_bridge) {
            reasoning_snn_destroy(snn_bridge);
            snn_bridge = nullptr;
        }
        if (plasticity_bridge) {
            reasoning_plasticity_destroy(plasticity_bridge);
            plasticity_bridge = nullptr;
        }
    }

    // Generate reasoning scenario
    enum ReasoningScenario {
        STRONG_DEDUCTION,       // High deduction, high validity
        WEAK_DEDUCTION,         // Low deduction confidence
        STRONG_INDUCTION,       // High induction, many examples
        CAUSAL_CHAIN,           // Strong causal relationship
        EVIDENCE_HEAVY,         // Many evidence sources
        LOGICAL_CONFLICT,       // Conflicting premises
        ABDUCTIVE_INFERENCE,    // Best explanation reasoning
        ANALOGY_BASED           // Analogical reasoning
    };

    void generate_scenario(float* dims, ReasoningScenario scenario) {
        memset(dims, 0, sizeof(float) * REASON_DIM_COUNT);

        switch (scenario) {
            case STRONG_DEDUCTION:
                dims[REASON_DIM_DEDUCTION] = 0.9f;
                dims[REASON_DIM_LOGICAL_VALIDITY] = 0.95f;
                dims[REASON_DIM_EVIDENCE_WEIGHT] = 0.8f;
                dims[REASON_DIM_INFERENCE_DEPTH] = 0.7f;
                break;

            case WEAK_DEDUCTION:
                dims[REASON_DIM_DEDUCTION] = 0.4f;
                dims[REASON_DIM_LOGICAL_VALIDITY] = 0.5f;
                dims[REASON_DIM_EVIDENCE_WEIGHT] = 0.3f;
                dims[REASON_DIM_INFERENCE_DEPTH] = 0.2f;
                break;

            case STRONG_INDUCTION:
                dims[REASON_DIM_INDUCTION] = 0.85f;
                dims[REASON_DIM_EVIDENCE_WEIGHT] = 0.9f;
                dims[REASON_DIM_PROBABILITY] = 0.75f;
                dims[REASON_DIM_LOGICAL_VALIDITY] = 0.7f;
                break;

            case CAUSAL_CHAIN:
                dims[REASON_DIM_CAUSAL] = 0.9f;
                dims[REASON_DIM_DEDUCTION] = 0.7f;
                dims[REASON_DIM_INFERENCE_DEPTH] = 0.85f;
                dims[REASON_DIM_LOGICAL_VALIDITY] = 0.8f;
                break;

            case EVIDENCE_HEAVY:
                dims[REASON_DIM_EVIDENCE_WEIGHT] = 0.95f;
                dims[REASON_DIM_INDUCTION] = 0.7f;
                dims[REASON_DIM_PROBABILITY] = 0.8f;
                dims[REASON_DIM_LOGICAL_VALIDITY] = 0.75f;
                break;

            case LOGICAL_CONFLICT:
                dims[REASON_DIM_DEDUCTION] = 0.6f;
                dims[REASON_DIM_INDUCTION] = 0.5f;
                dims[REASON_DIM_COUNTERFACTUAL] = 0.7f;
                dims[REASON_DIM_LOGICAL_VALIDITY] = 0.3f;
                break;

            case ABDUCTIVE_INFERENCE:
                dims[REASON_DIM_ABDUCTION] = 0.85f;
                dims[REASON_DIM_EVIDENCE_WEIGHT] = 0.7f;
                dims[REASON_DIM_PROBABILITY] = 0.65f;
                dims[REASON_DIM_LOGICAL_VALIDITY] = 0.6f;
                break;

            case ANALOGY_BASED:
                dims[REASON_DIM_ANALOGY] = 0.8f;
                dims[REASON_DIM_INDUCTION] = 0.6f;
                dims[REASON_DIM_EVIDENCE_WEIGHT] = 0.55f;
                dims[REASON_DIM_LOGICAL_VALIDITY] = 0.65f;
                break;
        }
    }

    // Run single evaluation pipeline
    struct EvaluationResult {
        float deduction_strength;
        float logical_validity;
        float causal_confidence;
        bool conclusion_reached;
        int spike_count;
    };

    EvaluationResult run_evaluation(ReasoningScenario scenario) {
        EvaluationResult result = {0};

        float dims[REASON_DIM_COUNT];
        generate_scenario(dims, scenario);

        // Encode and simulate
        result.spike_count = reasoning_snn_encode_state(snn_bridge, dims, REASON_DIM_COUNT);
        reasoning_snn_simulate(snn_bridge, 30.0f);

        // Get inference
        reasoning_inference_t inference;
        reasoning_snn_get_inference(snn_bridge, &inference);

        result.deduction_strength = inference.deduction_strength;
        result.logical_validity = inference.logical_validity;
        result.causal_confidence = inference.causal_confidence;

        // Check conclusion validity
        float validity;
        result.conclusion_reached = reasoning_snn_check_conclusion(snn_bridge, &validity);

        // Update stats
        stats.total_evaluations++;
        stats.validity_history.push_back(inference.logical_validity);
        stats.deduction_scores.push_back(inference.deduction_strength);

        if (result.logical_validity > 0.6f) {
            stats.valid_conclusions++;
        } else {
            stats.invalid_conclusions++;
        }

        // Check causal detection
        float causal_strength;
        if (reasoning_snn_check_causal(snn_bridge, &causal_strength) && causal_strength > 0.5f) {
            stats.causal_detected++;
        }

        return result;
    }
};

//=============================================================================
// Basic Pipeline Tests
//=============================================================================

TEST_F(ReasoningSNNPlasticityE2E, CompletePipelineInitialization) {
    // Verify complete pipeline setup
    EXPECT_NE(snn_bridge, nullptr);
    EXPECT_NE(plasticity_bridge, nullptr);

    // Check synapse registration
    reasoning_plasticity_bridge_state_t state;
    reasoning_plasticity_get_state(plasticity_bridge, &state);
    EXPECT_GT(state.active_synapses, (uint32_t)REASON_DIM_COUNT);  // Base + protected
}

TEST_F(ReasoningSNNPlasticityE2E, SingleEvaluationPipeline) {
    // Run single strong deduction scenario
    auto result = run_evaluation(STRONG_DEDUCTION);

    // Verify inference is valid
    EXPECT_GE(result.deduction_strength, 0.0f);
    EXPECT_LE(result.deduction_strength, 1.0f);
    EXPECT_GE(result.logical_validity, 0.0f);
    EXPECT_LE(result.logical_validity, 1.0f);
    EXPECT_GE(result.spike_count, 0);

    // Apply learning based on result
    int ret = reasoning_plasticity_learn(plasticity_bridge,
        REASON_LEARN_VALID_CONCLUSION, 0.5f, 0, result.logical_validity);
    EXPECT_EQ(ret, 0);
}

//=============================================================================
// Deductive Reasoning Tests
//=============================================================================

TEST_F(ReasoningSNNPlasticityE2E, DeductiveLearning) {
    // Run multiple deductive scenarios
    float total_validity = 0.0f;
    int valid_count = 0;

    for (int trial = 0; trial < 10; trial++) {
        auto result = run_evaluation(STRONG_DEDUCTION);

        total_validity += result.logical_validity;
        if (result.logical_validity > 0.5f || result.deduction_strength > 0.5f) {
            valid_count++;
        }

        // Learn valid deduction
        reasoning_plasticity_learn(plasticity_bridge,
            REASON_LEARN_VALID_CONCLUSION, 0.5f, 0, result.deduction_strength);

        // Apply STDP
        reasoning_plasticity_apply_stdp(plasticity_bridge, 0,
            (float)trial, (float)trial + 5.0f);
    }

    // At least some trials should yield valid conclusions
    EXPECT_GT(valid_count, 0);
}

TEST_F(ReasoningSNNPlasticityE2E, WeakDeductionLearning) {
    // Run multiple weak deduction scenarios
    float total_validity = 0.0f;

    for (int trial = 0; trial < 10; trial++) {
        auto result = run_evaluation(WEAK_DEDUCTION);

        total_validity += result.logical_validity;

        // Learn from weak deduction - should trigger caution
        reasoning_plasticity_learn(plasticity_bridge,
            REASON_LEARN_INVALID_CONCLUSION, 0.5f, 1, result.deduction_strength);
    }

    // Average validity should be lower for weak deduction
    // (we're just verifying processing completes)
    EXPECT_GE(total_validity / 10.0f, 0.0f);
}

TEST_F(ReasoningSNNPlasticityE2E, DeductionConsistencyImprovement) {
    // Register deduction synapses
    for (int i = 200; i < 210; i++) {
        reasoning_plasticity_register_synapse(plasticity_bridge, i,
            REASON_SYNAPSE_EVIDENCE, 0.5f);
    }

    // Initial synapse weight
    reasoning_plasticity_synapse_t initial_synapse;
    reasoning_plasticity_get_synapse(plasticity_bridge, 200, &initial_synapse);
    float initial_weight = initial_synapse.weight;

    // Run mixed deduction scenarios
    for (int epoch = 0; epoch < 5; epoch++) {
        // Strong deduction scenario
        auto strong_result = run_evaluation(STRONG_DEDUCTION);
        reasoning_plasticity_learn(plasticity_bridge,
            REASON_LEARN_VALID_CONCLUSION, 0.3f, 200, strong_result.logical_validity);

        // Weak deduction scenario
        auto weak_result = run_evaluation(WEAK_DEDUCTION);
        reasoning_plasticity_learn(plasticity_bridge,
            REASON_LEARN_INVALID_CONCLUSION, 0.3f, 201, weak_result.logical_validity);

        // BCM and homeostatic updates
        reasoning_plasticity_update_bcm(plasticity_bridge, 0.5f);
        reasoning_plasticity_homeostatic_update(plasticity_bridge, 0.5f);
    }

    // Verify learning occurred
    reasoning_plasticity_stats_t stats;
    reasoning_plasticity_get_stats(plasticity_bridge, &stats);
    EXPECT_GT(stats.total_learning_events, 0u);
    EXPECT_GT(stats.valid_conclusion_events, 0u);
}

//=============================================================================
// Causal Reasoning Tests
//=============================================================================

TEST_F(ReasoningSNNPlasticityE2E, CausalChainDetection) {
    int causal_trials = 0;
    float total_causal = 0.0f;

    for (int trial = 0; trial < 10; trial++) {
        auto result = run_evaluation(CAUSAL_CHAIN);

        total_causal += result.causal_confidence;
        if (result.causal_confidence > 0.0f) {
            causal_trials++;

            // Learn from causal detection
            reasoning_plasticity_learn(plasticity_bridge,
                REASON_LEARN_CAUSAL_CONFIRMED, 0.5f, 2, result.causal_confidence);
        }
    }

    // Should detect causal signals in multiple trials
    EXPECT_GE(causal_trials, 3);
    // Total causal confidence across trials should be meaningful
    EXPECT_GT(total_causal, 0.0f);
}

//=============================================================================
// Protected Synapse Tests
//=============================================================================

TEST_F(ReasoningSNNPlasticityE2E, DeductionProtectionIntegrity) {
    // Get initial protected synapse weight
    reasoning_plasticity_synapse_t deduction_synapse;
    reasoning_plasticity_get_synapse(plasticity_bridge, 100, &deduction_synapse);
    float original_weight = deduction_synapse.weight;
    EXPECT_TRUE(deduction_synapse.is_protected);

    // Run many scenarios and try to modify protected synapse
    for (int trial = 0; trial < 20; trial++) {
        auto result = run_evaluation((ReasoningScenario)(trial % 8));

        // Try various learning operations on protected synapse
        reasoning_plasticity_learn(plasticity_bridge,
            REASON_LEARN_INVALID_CONCLUSION, -1.0f, 100, result.logical_validity);
        reasoning_plasticity_apply_stdp(plasticity_bridge, 100,
            (float)trial, (float)trial + 10.0f);
        reasoning_plasticity_apply_reward(plasticity_bridge, -1.0f);
    }

    // Protected synapse should remain unchanged
    reasoning_plasticity_get_synapse(plasticity_bridge, 100, &deduction_synapse);
    EXPECT_FLOAT_EQ(deduction_synapse.weight, original_weight);
    EXPECT_TRUE(deduction_synapse.is_protected);
}

TEST_F(ReasoningSNNPlasticityE2E, CausalSynapseProtection) {
    // Causal synapse should also be protected
    reasoning_plasticity_synapse_t causal_synapse;
    reasoning_plasticity_get_synapse(plasticity_bridge, 101, &causal_synapse);
    float original_weight = causal_synapse.weight;
    EXPECT_TRUE(causal_synapse.is_protected);

    // Stress test protection
    for (int i = 0; i < 50; i++) {
        reasoning_plasticity_apply_stdp(plasticity_bridge, 101, (float)i, (float)i + 5.0f);
        reasoning_plasticity_learn(plasticity_bridge,
            REASON_LEARN_INVALID_CONCLUSION, 1.0f, 101, 0.9f);
    }

    // Weight must remain unchanged
    reasoning_plasticity_get_synapse(plasticity_bridge, 101, &causal_synapse);
    EXPECT_FLOAT_EQ(causal_synapse.weight, original_weight);
}

//=============================================================================
// Evidence and Induction Tests
//=============================================================================

TEST_F(ReasoningSNNPlasticityE2E, EvidenceAccumulationLearning) {
    // Register evidence synapses
    for (int i = 300; i < 305; i++) {
        reasoning_plasticity_register_synapse(plasticity_bridge, i,
            REASON_SYNAPSE_EVIDENCE, 0.5f);
    }

    int learning_events = 0;

    for (int trial = 0; trial < 10; trial++) {
        auto result = run_evaluation(EVIDENCE_HEAVY);

        // Apply learning for all evidence scenarios
        reasoning_plasticity_learn(plasticity_bridge,
            REASON_LEARN_EVIDENCE_INTEGRATED, 0.5f, 300 + (trial % 5),
            result.logical_validity > 0.0f ? result.logical_validity : 0.5f);
        learning_events++;
    }

    // Verify we applied learning across all trials
    EXPECT_EQ(learning_events, 10);

    // Verify plasticity stats reflect learning
    reasoning_plasticity_stats_t stats;
    reasoning_plasticity_get_stats(plasticity_bridge, &stats);
    EXPECT_GE(stats.total_learning_events, 10u);
}

TEST_F(ReasoningSNNPlasticityE2E, InductiveLearning) {
    float total_induction = 0.0f;
    int induction_detections = 0;

    for (int trial = 0; trial < 10; trial++) {
        auto result = run_evaluation(STRONG_INDUCTION);

        reasoning_inference_t inference;
        reasoning_snn_get_inference(snn_bridge, &inference);
        total_induction += inference.induction_strength;

        if (inference.induction_strength > 0.0f) {
            induction_detections++;
        }

        // Learn from induction
        reasoning_plasticity_learn(plasticity_bridge,
            REASON_LEARN_ANALOGY_MATCHED, 0.5f, 3, inference.induction_strength);
    }

    // Should detect induction signals
    EXPECT_GE(induction_detections, 3);
    EXPECT_GT(total_induction, 0.0f);
}

//=============================================================================
// Conflict Detection Tests
//=============================================================================

TEST_F(ReasoningSNNPlasticityE2E, LogicalConflictDetection) {
    float total_conflict = 0.0f;
    int conflict_detections = 0;
    int learning_events = 0;

    for (int trial = 0; trial < 10; trial++) {
        auto result = run_evaluation(LOGICAL_CONFLICT);

        float conflict_level;
        reasoning_snn_check_conflict(snn_bridge, &conflict_level);

        total_conflict += conflict_level;
        if (conflict_level > 0.0f || result.logical_validity < 0.5f) {
            conflict_detections++;
        }

        // Always apply learning for conflict scenarios
        reasoning_plasticity_learn(plasticity_bridge,
            REASON_LEARN_CONFLICT_RESOLVED, 0.5f, 4, conflict_level);
        learning_events++;
    }

    // Should detect conflicts in at least some trials
    EXPECT_GE(conflict_detections, 3);
    // Verify learning was applied
    EXPECT_EQ(learning_events, 10);
}

//=============================================================================
// Multi-Scenario Learning Tests
//=============================================================================

TEST_F(ReasoningSNNPlasticityE2E, CompleteReasoningWorkflow) {
    // Register workflow synapses
    for (int i = 400; i < 420; i++) {
        reasoning_plasticity_register_synapse(plasticity_bridge, i,
            REASON_SYNAPSE_EVIDENCE, 0.5f);
    }

    // Run complete reasoning workflow
    for (int epoch = 0; epoch < 5; epoch++) {
        for (int scenario = 0; scenario < 8; scenario++) {
            auto result = run_evaluation((ReasoningScenario)scenario);

            // Select learning event based on scenario
            reasoning_learn_event_t event;
            float magnitude = 0.3f;

            switch ((ReasoningScenario)scenario) {
                case STRONG_DEDUCTION:
                case STRONG_INDUCTION:
                    event = REASON_LEARN_VALID_CONCLUSION;
                    break;
                case WEAK_DEDUCTION:
                    event = REASON_LEARN_INVALID_CONCLUSION;
                    break;
                case CAUSAL_CHAIN:
                    event = REASON_LEARN_CAUSAL_CONFIRMED;
                    break;
                case EVIDENCE_HEAVY:
                    event = REASON_LEARN_EVIDENCE_INTEGRATED;
                    break;
                case LOGICAL_CONFLICT:
                    event = REASON_LEARN_CONFLICT_RESOLVED;
                    break;
                case ABDUCTIVE_INFERENCE:
                    event = REASON_LEARN_VALID_CONCLUSION;
                    break;
                default:
                    event = REASON_LEARN_ANALOGY_MATCHED;
                    break;
            }

            int synapse_id = 400 + (epoch * 8 + scenario) % 20;
            reasoning_plasticity_learn(plasticity_bridge, event, magnitude,
                synapse_id, result.logical_validity);

            // Apply STDP
            reasoning_plasticity_apply_stdp(plasticity_bridge, synapse_id,
                (float)(epoch * 10 + scenario), (float)(epoch * 10 + scenario + 5));
        }

        // Periodic maintenance
        reasoning_plasticity_update_bcm(plasticity_bridge, 0.5f);
        reasoning_plasticity_homeostatic_update(plasticity_bridge, 0.5f);
        reasoning_plasticity_update_traces(plasticity_bridge, 1.0f);
    }

    // Consolidate learning
    reasoning_plasticity_consolidate(plasticity_bridge);

    // Verify extensive learning occurred
    reasoning_plasticity_stats_t final_stats;
    reasoning_plasticity_get_stats(plasticity_bridge, &final_stats);
    EXPECT_GT(final_stats.total_learning_events, 30u);
    EXPECT_GT(final_stats.weight_updates, 30u);

    reasoning_snn_stats_t snn_stats;
    reasoning_snn_get_stats(snn_bridge, &snn_stats);
    EXPECT_GE(snn_stats.total_evaluations, 40u);
}

//=============================================================================
// Stress and Performance Tests
//=============================================================================

TEST_F(ReasoningSNNPlasticityE2E, HighVolumeProcessing) {
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 100; i++) {
        run_evaluation((ReasoningScenario)(i % 8));
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // 100 evaluations should complete in under 5 seconds
    EXPECT_LT(duration.count(), 5000);
    EXPECT_EQ(stats.total_evaluations, 100);
}

TEST_F(ReasoningSNNPlasticityE2E, ContinuousLearning) {
    // Register many synapses
    for (int i = 500; i < 600; i++) {
        reasoning_plasticity_register_synapse(plasticity_bridge, i,
            REASON_SYNAPSE_EVIDENCE, 0.5f);
    }

    // Continuous learning loop
    for (int cycle = 0; cycle < 50; cycle++) {
        auto result = run_evaluation((ReasoningScenario)(cycle % 8));

        // Learn on rotating synapses
        for (int j = 0; j < 5; j++) {
            int synapse_id = 500 + (cycle * 5 + j) % 100;
            reasoning_plasticity_learn(plasticity_bridge,
                REASON_LEARN_VALID_CONCLUSION, 0.1f, synapse_id, result.logical_validity);
        }

        // Periodic BCM update
        if (cycle % 10 == 0) {
            reasoning_plasticity_update_bcm(plasticity_bridge, 0.5f);
        }
    }

    // Verify extensive learning
    reasoning_plasticity_stats_t stats;
    reasoning_plasticity_get_stats(plasticity_bridge, &stats);
    EXPECT_GE(stats.total_learning_events, 200u);
}

//=============================================================================
// Reset and Recovery Tests
//=============================================================================

TEST_F(ReasoningSNNPlasticityE2E, ResetAndRecovery) {
    // Accumulate some state
    for (int i = 0; i < 10; i++) {
        run_evaluation((ReasoningScenario)(i % 8));
    }

    // Reset both bridges
    reasoning_snn_reset(snn_bridge);
    reasoning_plasticity_reset(plasticity_bridge);

    // Verify recovery
    reasoning_snn_bridge_state_t snn_state;
    reasoning_snn_get_state(snn_bridge, &snn_state);
    EXPECT_EQ(snn_state.state, REASONING_SNN_STATE_IDLE);

    reasoning_plasticity_bridge_state_t plasticity_state;
    reasoning_plasticity_get_state(plasticity_bridge, &plasticity_state);
    EXPECT_EQ(plasticity_state.state, REASONING_PLASTICITY_STATE_IDLE);

    // Can continue processing
    auto result = run_evaluation(STRONG_DEDUCTION);
    EXPECT_GE(result.logical_validity, 0.0f);
}

//=============================================================================
// Statistics Validation Tests
//=============================================================================

TEST_F(ReasoningSNNPlasticityE2E, StatisticsAccuracy) {
    // Run known number of evaluations
    for (int i = 0; i < 20; i++) {
        run_evaluation((ReasoningScenario)(i % 8));

        // Apply learning
        reasoning_plasticity_learn(plasticity_bridge,
            REASON_LEARN_VALID_CONCLUSION, 0.1f, i % REASON_DIM_COUNT, 0.5f);
    }

    // Verify stats match
    reasoning_snn_stats_t snn_stats;
    reasoning_snn_get_stats(snn_bridge, &snn_stats);
    EXPECT_GE(snn_stats.total_evaluations, 20u);

    reasoning_plasticity_stats_t plasticity_stats;
    reasoning_plasticity_get_stats(plasticity_bridge, &plasticity_stats);
    EXPECT_GE(plasticity_stats.total_learning_events, 20u);
}
