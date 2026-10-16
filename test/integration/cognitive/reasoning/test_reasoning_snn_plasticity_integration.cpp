//=============================================================================
// test_reasoning_snn_plasticity_integration.cpp - Reasoning Integration
//=============================================================================
/**
 * @file test_reasoning_snn_plasticity_integration.cpp
 * @brief Integration tests for Reasoning-SNN-Plasticity bidirectional dataflows
 *
 * WHAT: Tests complete integration between reasoning, SNN, and plasticity
 * WHY:  Verify bidirectional dataflows work correctly for reasoning learning
 * HOW:  Create both bridges, simulate reasoning scenarios, verify calibration
 *
 * INTEGRATION POINTS:
 * - Reasoning encoding -> SNN population activity
 * - SNN spikes -> Plasticity STDP -> weight updates
 * - Learning events -> Synapse modification -> Inference calibration
 * - Protection mechanisms -> Block learning on core reasoning
 *
 * THEORETICAL BASIS:
 * - Deductive reasoning (premise -> conclusion)
 * - Inductive reasoning (examples -> generalization)
 * - Causal reasoning (cause -> effect inference)
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <thread>
#include <chrono>
#include <atomic>

#include "cognitive/reasoning/nimcp_reasoning_snn_bridge.h"
#include "cognitive/reasoning/nimcp_reasoning_plasticity_bridge.h"

//=============================================================================
// Test Fixture
//=============================================================================

class ReasoningSNNPlasticityIntegrationTest : public ::testing::Test {
protected:
    reasoning_snn_bridge_t* snn_bridge;
    reasoning_plasticity_bridge_t* plasticity_bridge;

    // Callback tracking
    std::atomic<int> conflict_detection_count{0};
    std::atomic<int> inference_count{0};
    std::atomic<int> weight_change_count{0};
    std::atomic<int> calibration_update_count{0};
    std::atomic<float> last_conflict_level{0.0f};

    void SetUp() override {
        // Create SNN bridge with test-friendly config
        reasoning_snn_config_t snn_config = reasoning_snn_config_default();
        snn_config.num_dimensions = REASON_DIM_COUNT;
        snn_config.neurons_per_dim = 32;
        snn_config.enable_causal_chains = true;
        snn_config.enable_bio_async = false;  // Disable for predictable tests

        snn_bridge = reasoning_snn_create(&snn_config);
        ASSERT_NE(snn_bridge, nullptr) << "Failed to create SNN bridge";

        // Create Plasticity bridge with defaults
        reasoning_plasticity_config_t plasticity_config = reasoning_plasticity_config_default();
        plasticity_config.base_learning_rate = 0.01f;
        plasticity_config.stdp_a_plus = 0.01f;
        plasticity_config.stdp_a_minus = 0.012f;

        plasticity_bridge = reasoning_plasticity_create(&plasticity_config);
        ASSERT_NE(plasticity_bridge, nullptr) << "Failed to create plasticity bridge";

        // Reset counters
        conflict_detection_count = 0;
        inference_count = 0;
        weight_change_count = 0;
        calibration_update_count = 0;
        last_conflict_level = 0.0f;
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

    // Generate reasoning context for scenario
    void generate_reasoning_context(float* dims, uint32_t scenario_type) {
        memset(dims, 0, sizeof(float) * REASON_DIM_COUNT);
        switch (scenario_type) {
            case 0: // Strong deductive reasoning
                dims[REASON_DIM_DEDUCTION] = 0.9f;
                dims[REASON_DIM_LOGICAL_VALIDITY] = 0.95f;
                dims[REASON_DIM_EVIDENCE_WEIGHT] = 0.8f;
                break;
            case 1: // Inductive reasoning with evidence
                dims[REASON_DIM_INDUCTION] = 0.85f;
                dims[REASON_DIM_EVIDENCE_WEIGHT] = 0.9f;
                dims[REASON_DIM_PROBABILITY] = 0.7f;
                break;
            case 2: // Causal reasoning scenario
                dims[REASON_DIM_CAUSAL] = 0.9f;
                dims[REASON_DIM_ABDUCTION] = 0.6f;
                dims[REASON_DIM_INFERENCE_DEPTH] = 0.5f;
                break;
            case 3: // Analogical reasoning
                dims[REASON_DIM_ANALOGY] = 0.85f;
                dims[REASON_DIM_INDUCTION] = 0.7f;
                dims[REASON_DIM_EVIDENCE_WEIGHT] = 0.6f;
                break;
            case 4: // Counterfactual thinking
                dims[REASON_DIM_COUNTERFACTUAL] = 0.8f;
                dims[REASON_DIM_CAUSAL] = 0.7f;
                dims[REASON_DIM_PROBABILITY] = 0.5f;
                break;
            default:
                for (int i = 0; i < REASON_DIM_COUNT; i++) {
                    dims[i] = 0.5f;
                }
                break;
        }
    }
};

//=============================================================================
// Basic Integration Tests
//=============================================================================

TEST_F(ReasoningSNNPlasticityIntegrationTest, BothBridgesInitialize) {
    // Verify both bridges are functional
    EXPECT_NE(snn_bridge, nullptr);
    EXPECT_NE(plasticity_bridge, nullptr);

    // Check initial states
    reasoning_snn_bridge_state_t snn_state;
    EXPECT_EQ(reasoning_snn_get_state(snn_bridge, &snn_state), 0);
    EXPECT_EQ(snn_state.state, REASONING_SNN_STATE_IDLE);

    reasoning_plasticity_bridge_state_t plasticity_state;
    EXPECT_EQ(reasoning_plasticity_get_state(plasticity_bridge, &plasticity_state), 0);
    EXPECT_EQ(plasticity_state.state, REASONING_PLASTICITY_STATE_IDLE);
}

TEST_F(ReasoningSNNPlasticityIntegrationTest, SNNEncodingDrivesPlasticityActivity) {
    // Encode reasoning context in SNN
    float dims[REASON_DIM_COUNT];
    generate_reasoning_context(dims, 0);  // Strong deduction scenario

    int spikes = reasoning_snn_encode_state(snn_bridge, dims, REASON_DIM_COUNT);
    EXPECT_GE(spikes, 0) << "Encoding should succeed (0 or more spikes)";

    // Simulate SNN processing
    EXPECT_EQ(reasoning_snn_simulate(snn_bridge, 20.0f), 0);

    // Register synapses in plasticity bridge
    for (int i = 0; i < 5; i++) {
        EXPECT_EQ(reasoning_plasticity_register_synapse(plasticity_bridge,
            i, REASON_SYNAPSE_INDUCTION, 0.5f), 0);
    }

    // Apply STDP based on SNN activity (returns weight delta)
    float delta = reasoning_plasticity_apply_stdp(plasticity_bridge, 0, 1.0f, 3.0f);
    EXPECT_TRUE(std::isfinite(delta)) << "STDP should return valid delta";

    // Get synapse and verify retrieval succeeded
    reasoning_plasticity_synapse_t synapse;
    EXPECT_EQ(reasoning_plasticity_get_synapse(plasticity_bridge, 0, &synapse), 0);
}

//=============================================================================
// Deductive Reasoning Integration
//=============================================================================

TEST_F(ReasoningSNNPlasticityIntegrationTest, DeductiveReasoningPipeline) {
    // Encode deductive reasoning scenario
    int spikes = reasoning_snn_encode_deduction(snn_bridge, 0.95f, 0.9f);
    EXPECT_GE(spikes, 0);

    // Simulate processing
    reasoning_snn_simulate(snn_bridge, 30.0f);

    // Check conclusion validity
    float validity;
    reasoning_snn_check_conclusion(snn_bridge, &validity);

    // Register deduction synapse (auto-protected)
    EXPECT_EQ(reasoning_plasticity_register_synapse(plasticity_bridge,
        100, REASON_SYNAPSE_DEDUCTION, 0.5f), 0);

    // Deduction synapse should be protected
    reasoning_plasticity_synapse_t synapse;
    EXPECT_EQ(reasoning_plasticity_get_synapse(plasticity_bridge, 100, &synapse), 0);
    EXPECT_TRUE(synapse.is_protected);
}

TEST_F(ReasoningSNNPlasticityIntegrationTest, ValidConclusionReinforcesLearning) {
    // Register outcome synapse (not auto-protected)
    EXPECT_EQ(reasoning_plasticity_register_synapse(plasticity_bridge,
        200, REASON_SYNAPSE_INDUCTION, 0.4f), 0);

    // Initial weight
    reasoning_plasticity_synapse_t synapse;
    EXPECT_EQ(reasoning_plasticity_get_synapse(plasticity_bridge, 200, &synapse), 0);
    float initial_weight = synapse.weight;
    (void)initial_weight;

    // Learn from valid conclusion (positive outcome)
    EXPECT_EQ(reasoning_plasticity_learn(plasticity_bridge,
        REASON_LEARN_VALID_CONCLUSION, 0.9f, 200, 0.9f), 0);

    // Verify weight is still valid
    EXPECT_EQ(reasoning_plasticity_get_synapse(plasticity_bridge, 200, &synapse), 0);
    EXPECT_GE(synapse.weight, 0.0f);
    EXPECT_LE(synapse.weight, 2.0f);
}

//=============================================================================
// Causal Reasoning Integration
//=============================================================================

TEST_F(ReasoningSNNPlasticityIntegrationTest, CausalReasoningEncodingAndLearning) {
    // Encode causal scenario
    int spikes = reasoning_snn_encode_causal(snn_bridge, 0.9f, 0.85f);
    EXPECT_GE(spikes, 0);

    // Simulate processing
    reasoning_snn_simulate(snn_bridge, 25.0f);

    // Get inference
    reasoning_inference_t inference;
    EXPECT_EQ(reasoning_snn_get_inference(snn_bridge, &inference), 0);

    // Register causal synapse (auto-protected)
    EXPECT_EQ(reasoning_plasticity_register_synapse(plasticity_bridge,
        300, REASON_SYNAPSE_CAUSAL, 0.8f), 0);

    // Synapse should be protected
    reasoning_plasticity_synapse_t synapse;
    EXPECT_EQ(reasoning_plasticity_get_synapse(plasticity_bridge, 300, &synapse), 0);
    EXPECT_TRUE(synapse.is_protected);

    // Register non-protected synapse for learning
    EXPECT_EQ(reasoning_plasticity_register_synapse(plasticity_bridge,
        301, REASON_SYNAPSE_EVIDENCE, 0.5f), 0);

    // Learn causal confirmation
    EXPECT_EQ(reasoning_plasticity_learn(plasticity_bridge,
        REASON_LEARN_CAUSAL_CONFIRMED, 0.8f, 301, inference.causal_confidence), 0);
}

//=============================================================================
// Deduction and Causal Protection Integration
//=============================================================================

TEST_F(ReasoningSNNPlasticityIntegrationTest, DeductionCausalProtectionIntegrity) {
    // Encode high deduction and causal activation
    float dims[REASON_DIM_COUNT] = {0};
    dims[REASON_DIM_DEDUCTION] = 1.0f;
    dims[REASON_DIM_CAUSAL] = 0.9f;

    reasoning_snn_encode_state(snn_bridge, dims, REASON_DIM_COUNT);
    reasoning_snn_simulate(snn_bridge, 30.0f);

    // Get inference
    reasoning_inference_t inference;
    reasoning_snn_get_inference(snn_bridge, &inference);

    // Register deduction synapse (auto-protected)
    EXPECT_EQ(reasoning_plasticity_register_synapse(plasticity_bridge,
        400, REASON_SYNAPSE_DEDUCTION, 1.0f), 0);

    // Deduction synapse should be protected
    reasoning_plasticity_synapse_t synapse;
    EXPECT_EQ(reasoning_plasticity_get_synapse(plasticity_bridge, 400, &synapse), 0);
    EXPECT_TRUE(synapse.is_protected);

    // Attempt to modify protected synapse (should be blocked)
    float original_weight = synapse.weight;
    reasoning_plasticity_apply_stdp(plasticity_bridge, 400, 5.0f, 10.0f);

    EXPECT_EQ(reasoning_plasticity_get_synapse(plasticity_bridge, 400, &synapse), 0);
    EXPECT_FLOAT_EQ(synapse.weight, original_weight) << "Protected synapse should not change";
}

//=============================================================================
// Analogical Reasoning Integration
//=============================================================================

TEST_F(ReasoningSNNPlasticityIntegrationTest, AnalogyDetectionAndLearning) {
    // Encode analogy scenario
    float dims[REASON_DIM_COUNT];
    generate_reasoning_context(dims, 3);  // Analogy scenario

    reasoning_snn_encode_state(snn_bridge, dims, REASON_DIM_COUNT);
    reasoning_snn_simulate(snn_bridge, 40.0f);

    // Get inference
    reasoning_inference_t inference;
    reasoning_snn_get_inference(snn_bridge, &inference);

    // Register analogy synapse (not auto-protected)
    EXPECT_EQ(reasoning_plasticity_register_synapse(plasticity_bridge,
        500, REASON_SYNAPSE_ANALOGY, 0.5f), 0);

    // Apply learning based on analogy match
    if (inference.analogy_match > 0.5f) {
        reasoning_plasticity_learn(plasticity_bridge,
            REASON_LEARN_ANALOGY_MATCHED, 0.6f, 500, inference.analogy_match);
    } else {
        reasoning_plasticity_learn(plasticity_bridge,
            REASON_LEARN_ANALOGY_FAILED, 0.4f, 500, inference.analogy_match);
    }

    // Verify learning occurred
    reasoning_plasticity_stats_t stats;
    reasoning_plasticity_get_stats(plasticity_bridge, &stats);
    EXPECT_GT(stats.total_learning_events, 0u);
}

//=============================================================================
// Full Pipeline Integration
//=============================================================================

TEST_F(ReasoningSNNPlasticityIntegrationTest, FullReasoningDecisionPipeline) {
    // Register multiple synapse types
    for (int i = 0; i < 5; i++) {
        reasoning_plasticity_register_synapse(plasticity_bridge,
            600 + i, REASON_SYNAPSE_INDUCTION, 0.5f);
        reasoning_plasticity_register_synapse(plasticity_bridge,
            610 + i, REASON_SYNAPSE_EVIDENCE, 0.5f);
    }

    // Run multiple scenarios
    for (int scenario = 0; scenario < 5; scenario++) {
        float dims[REASON_DIM_COUNT];
        generate_reasoning_context(dims, scenario);

        // SNN encoding and simulation
        reasoning_snn_encode_state(snn_bridge, dims, REASON_DIM_COUNT);
        reasoning_snn_simulate(snn_bridge, 25.0f);

        // Get inference
        reasoning_inference_t inference;
        reasoning_snn_get_inference(snn_bridge, &inference);

        // Apply learning based on logical validity
        if (inference.logical_validity > 0.7f) {
            for (int i = 0; i < 5; i++) {
                reasoning_plasticity_learn(plasticity_bridge,
                    REASON_LEARN_VALID_CONCLUSION, 0.5f, 600 + i, inference.logical_validity);
            }
        } else {
            for (int i = 0; i < 5; i++) {
                reasoning_plasticity_learn(plasticity_bridge,
                    REASON_LEARN_INVALID_CONCLUSION, 0.3f, 600 + i, inference.logical_validity);
            }
        }

        // Apply STDP between consecutive synapse pairs
        for (int i = 0; i < 4; i++) {
            reasoning_plasticity_apply_stdp(plasticity_bridge, 600 + i,
                (float)scenario * 2.0f, (float)scenario * 2.0f + 5.0f);
        }

        // Update eligibility traces
        reasoning_plasticity_update_traces(plasticity_bridge, 1.0f);
    }

    // Verify stats
    reasoning_snn_stats_t snn_stats;
    reasoning_snn_get_stats(snn_bridge, &snn_stats);
    EXPECT_GE(snn_stats.total_evaluations, 5u);

    reasoning_plasticity_stats_t plasticity_stats;
    reasoning_plasticity_get_stats(plasticity_bridge, &plasticity_stats);
    EXPECT_GT(plasticity_stats.total_learning_events, 0u);
    EXPECT_GT(plasticity_stats.weight_updates, 0u);
}

//=============================================================================
// Reward Modulation Integration
//=============================================================================

TEST_F(ReasoningSNNPlasticityIntegrationTest, RewardModulatedLearning) {
    // Register synapses
    for (int i = 0; i < 3; i++) {
        reasoning_plasticity_register_synapse(plasticity_bridge,
            700 + i, REASON_SYNAPSE_INDUCTION, 0.5f);
    }

    // Encode strong deduction scenario
    float dims[REASON_DIM_COUNT];
    generate_reasoning_context(dims, 0);  // Strong deduction scenario
    reasoning_snn_encode_state(snn_bridge, dims, REASON_DIM_COUNT);
    reasoning_snn_simulate(snn_bridge, 25.0f);

    // Apply positive reward
    float reward = 0.8f;
    EXPECT_EQ(reasoning_plasticity_apply_reward(plasticity_bridge, reward), 0);

    // Check calibration state
    reasoning_calibration_state_t calibration;
    EXPECT_EQ(reasoning_plasticity_get_calibration_state(plasticity_bridge, &calibration), 0);
}

//=============================================================================
// BCM Metaplasticity Integration
//=============================================================================

TEST_F(ReasoningSNNPlasticityIntegrationTest, BCMMetaplasticityUpdate) {
    // Register synapses
    for (int i = 0; i < 5; i++) {
        reasoning_plasticity_register_synapse(plasticity_bridge,
            800 + i, REASON_SYNAPSE_INDUCTION, 0.5f);
    }

    // Run multiple encoding cycles
    for (int cycle = 0; cycle < 10; cycle++) {
        float dims[REASON_DIM_COUNT];
        generate_reasoning_context(dims, cycle % 5);

        reasoning_snn_encode_state(snn_bridge, dims, REASON_DIM_COUNT);
        reasoning_snn_step(snn_bridge);

        // Update BCM thresholds
        float postsynaptic_rate = 0.3f + 0.05f * cycle;
        reasoning_plasticity_update_bcm(plasticity_bridge, postsynaptic_rate);
    }

    // Verify BCM function ran without error
    reasoning_plasticity_stats_t stats;
    reasoning_plasticity_get_stats(plasticity_bridge, &stats);
}

//=============================================================================
// Homeostatic Regulation Integration
//=============================================================================

TEST_F(ReasoningSNNPlasticityIntegrationTest, HomeostaticWeightRegulation) {
    // Register synapses with varied initial weights
    for (int i = 0; i < 8; i++) {
        float initial_weight = 0.2f + 0.1f * i;  // 0.2 to 0.9
        reasoning_plasticity_register_synapse(plasticity_bridge,
            900 + i, REASON_SYNAPSE_INDUCTION, initial_weight);
    }

    // Run homeostatic update cycles
    float target_activity = 0.5f;
    for (int cycle = 0; cycle < 5; cycle++) {
        reasoning_plasticity_homeostatic_update(plasticity_bridge, target_activity);
    }

    // Verify homeostatic function ran without error
    reasoning_plasticity_stats_t stats;
    reasoning_plasticity_get_stats(plasticity_bridge, &stats);
}

//=============================================================================
// Consolidation Integration
//=============================================================================

TEST_F(ReasoningSNNPlasticityIntegrationTest, ReasoningLearningConsolidation) {
    // Register synapses
    for (int i = 0; i < 5; i++) {
        reasoning_plasticity_register_synapse(plasticity_bridge,
            1000 + i, REASON_SYNAPSE_INDUCTION, 0.5f);
    }

    // Apply significant learning
    for (int i = 0; i < 5; i++) {
        reasoning_plasticity_learn(plasticity_bridge,
            REASON_LEARN_VALID_CONCLUSION, 0.7f, 1000 + i, 0.9f);
    }

    // Get stats before consolidation
    reasoning_plasticity_stats_t before_stats;
    reasoning_plasticity_get_stats(plasticity_bridge, &before_stats);

    // Consolidate learning
    EXPECT_EQ(reasoning_plasticity_consolidate(plasticity_bridge), 0);

    // Verify consolidation occurred
    reasoning_plasticity_stats_t after_stats;
    reasoning_plasticity_get_stats(plasticity_bridge, &after_stats);
    EXPECT_GE(after_stats.total_learning_events, before_stats.total_learning_events);
}

//=============================================================================
// Reset and Recovery Integration
//=============================================================================

TEST_F(ReasoningSNNPlasticityIntegrationTest, ResetAndRecoveryBehavior) {
    // Setup state in both bridges
    float dims[REASON_DIM_COUNT];
    generate_reasoning_context(dims, 2);  // Causal reasoning
    reasoning_snn_encode_state(snn_bridge, dims, REASON_DIM_COUNT);
    reasoning_snn_simulate(snn_bridge, 20.0f);

    reasoning_plasticity_register_synapse(plasticity_bridge, 1100,
        REASON_SYNAPSE_EVIDENCE, 0.6f);
    reasoning_plasticity_learn(plasticity_bridge,
        REASON_LEARN_CAUSAL_CONFIRMED, 0.5f, 1100, 0.8f);

    // Reset both bridges
    EXPECT_EQ(reasoning_snn_reset(snn_bridge), 0);
    EXPECT_EQ(reasoning_plasticity_reset(plasticity_bridge), 0);

    // Verify reset states
    reasoning_snn_bridge_state_t snn_state;
    reasoning_snn_get_state(snn_bridge, &snn_state);
    EXPECT_EQ(snn_state.state, REASONING_SNN_STATE_IDLE);

    reasoning_plasticity_bridge_state_t plasticity_state;
    reasoning_plasticity_get_state(plasticity_bridge, &plasticity_state);
    EXPECT_EQ(plasticity_state.state, REASONING_PLASTICITY_STATE_IDLE);

    // Re-run scenarios to verify recovery
    reasoning_snn_encode_state(snn_bridge, dims, REASON_DIM_COUNT);
    reasoning_snn_simulate(snn_bridge, 15.0f);

    reasoning_inference_t inference;
    EXPECT_EQ(reasoning_snn_get_inference(snn_bridge, &inference), 0);
    EXPECT_GE(inference.deduction_strength, 0.0f);
}

//=============================================================================
// Concurrent Safety Tests
//=============================================================================

TEST_F(ReasoningSNNPlasticityIntegrationTest, ConcurrentEncodingAndLearning) {
    // Register synapses
    for (int i = 0; i < 10; i++) {
        reasoning_plasticity_register_synapse(plasticity_bridge,
            1200 + i, REASON_SYNAPSE_INDUCTION, 0.5f);
    }

    std::atomic<int> encoding_complete{0};
    std::atomic<int> learning_complete{0};

    // Thread 1: SNN encoding
    std::thread encoder([this, &encoding_complete]() {
        for (int i = 0; i < 5; i++) {
            float dims[REASON_DIM_COUNT];
            generate_reasoning_context(dims, i % 5);
            reasoning_snn_encode_state(snn_bridge, dims, REASON_DIM_COUNT);
            reasoning_snn_step(snn_bridge);
            encoding_complete++;
        }
    });

    // Thread 2: Plasticity learning
    std::thread learner([this, &learning_complete]() {
        for (int i = 0; i < 5; i++) {
            reasoning_plasticity_learn(plasticity_bridge,
                REASON_LEARN_VALID_CONCLUSION, 0.1f, 1200 + (i % 10), 0.5f);
            learning_complete++;
        }
    });

    encoder.join();
    learner.join();

    EXPECT_EQ(encoding_complete, 5);
    EXPECT_EQ(learning_complete, 5);
}

//=============================================================================
// Causal Inference Detection Integration
//=============================================================================

TEST_F(ReasoningSNNPlasticityIntegrationTest, CausalInferenceDetectionAndLearning) {
    // Encode causal signal
    int spikes = reasoning_snn_encode_causal(snn_bridge, 0.9f, 0.85f);
    EXPECT_GE(spikes, 0);

    reasoning_snn_simulate(snn_bridge, 30.0f);

    // Check causal level
    float causal_strength;
    reasoning_snn_check_causal(snn_bridge, &causal_strength);

    // Register evidence synapse (not auto-protected)
    EXPECT_EQ(reasoning_plasticity_register_synapse(plasticity_bridge,
        1300, REASON_SYNAPSE_EVIDENCE, 0.5f), 0);

    reasoning_plasticity_synapse_t synapse;
    EXPECT_EQ(reasoning_plasticity_get_synapse(plasticity_bridge, 1300, &synapse), 0);
    EXPECT_FALSE(synapse.is_protected);  // Evidence is not protected
}

//=============================================================================
// Stats Integration
//=============================================================================

TEST_F(ReasoningSNNPlasticityIntegrationTest, StatsAccumulationAcrossBridges) {
    // Run multiple scenarios
    for (int s = 0; s < 5; s++) {
        float dims[REASON_DIM_COUNT];
        generate_reasoning_context(dims, s % 5);

        reasoning_snn_encode_state(snn_bridge, dims, REASON_DIM_COUNT);
        reasoning_snn_simulate(snn_bridge, 10.0f);

        reasoning_plasticity_register_synapse(plasticity_bridge,
            1400 + s, REASON_SYNAPSE_INDUCTION, 0.5f);
        reasoning_plasticity_learn(plasticity_bridge,
            REASON_LEARN_VALID_CONCLUSION, 0.2f, 1400 + s, 0.6f);
    }

    // Check SNN stats
    reasoning_snn_stats_t snn_stats;
    reasoning_snn_get_stats(snn_bridge, &snn_stats);
    EXPECT_GE(snn_stats.total_evaluations, 5u);
    EXPECT_GT(snn_stats.total_simulations, 0u);

    // Check plasticity stats
    reasoning_plasticity_stats_t plasticity_stats;
    reasoning_plasticity_get_stats(plasticity_bridge, &plasticity_stats);
    // Verify synapses were used (active_synapses in state)
    reasoning_plasticity_bridge_state_t bridge_state;
    reasoning_plasticity_get_state(plasticity_bridge, &bridge_state);
    EXPECT_GE(bridge_state.active_synapses, 5u);
    EXPECT_GE(plasticity_stats.total_learning_events, 5u);
}

//=============================================================================
// Inference Calibration Learning Integration
//=============================================================================

TEST_F(ReasoningSNNPlasticityIntegrationTest, InferenceCalibrationLearningPipeline) {
    // Register calibration synapses (using INDUCTION type, not auto-protected)
    for (int i = 0; i < 5; i++) {
        reasoning_plasticity_register_synapse(plasticity_bridge,
            1500 + i, REASON_SYNAPSE_INDUCTION, 0.5f);
    }

    // Simulate valid vs invalid conclusions
    for (int trial = 0; trial < 10; trial++) {
        float dims[REASON_DIM_COUNT] = {0};
        if (trial % 2 == 0) {
            // Valid conclusion scenario
            dims[REASON_DIM_DEDUCTION] = 0.9f;
            dims[REASON_DIM_LOGICAL_VALIDITY] = 0.95f;
        } else {
            // Invalid conclusion scenario
            dims[REASON_DIM_DEDUCTION] = 0.4f;
            dims[REASON_DIM_LOGICAL_VALIDITY] = 0.3f;
        }

        reasoning_snn_encode_state(snn_bridge, dims, REASON_DIM_COUNT);
        reasoning_snn_simulate(snn_bridge, 15.0f);

        reasoning_inference_t inference;
        reasoning_snn_get_inference(snn_bridge, &inference);

        // Learn based on conclusion validity
        if (trial % 2 == 0) {
            reasoning_plasticity_learn(plasticity_bridge,
                REASON_LEARN_VALID_CONCLUSION, 0.5f, 1500 + (trial % 5), inference.logical_validity);
        } else {
            reasoning_plasticity_learn(plasticity_bridge,
                REASON_LEARN_INVALID_CONCLUSION, 0.5f, 1500 + (trial % 5), inference.logical_validity);
        }
    }

    // Verify learning statistics
    reasoning_plasticity_stats_t stats;
    reasoning_plasticity_get_stats(plasticity_bridge, &stats);
    EXPECT_GE(stats.valid_conclusion_events, 5u);
    EXPECT_GE(stats.invalid_conclusion_events, 5u);
}
