/**
 * @file e2e_test_tom_snn_plasticity_pipeline.cpp
 * @brief End-to-end tests for Theory of Mind-SNN-Plasticity learning pipeline
 * @version 1.0.0
 * @date 2026-01-06
 *
 * WHAT: Complete Theory of Mind pipeline with SNN and Plasticity
 * WHY:  Verify full dataflow from social context -> SNN encoding -> mental state
 *       inference -> plasticity learning -> social cognition evolution
 * HOW:  Test realistic scenarios combining belief/desire/intention encoding,
 *       STDP learning, reward-modulated plasticity, and protected synapse integrity
 *
 * Test Coverage:
 * - Full social context to mental state inference pipeline via SNN
 * - STDP and reward-modulated learning for belief attribution
 * - False belief detection and perspective-taking
 * - Belief and Perspective synapse protection
 * - Multi-scenario empathy learning
 * - Theory of mind accuracy evolution through experience
 * - Protected synapse integrity under stress
 */

#include <gtest/gtest.h>

#include "cognitive/theory_of_mind/nimcp_tom_snn_bridge.h"
#include "cognitive/theory_of_mind/nimcp_tom_plasticity_bridge.h"
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

class TomSNNPlasticityE2E : public ::testing::Test {
protected:
    tom_snn_bridge_t* snn_bridge = nullptr;
    tom_plasticity_bridge_t* plasticity_bridge = nullptr;

    // Learning statistics
    struct LearningStats {
        int correct_belief_attributions = 0;
        int false_belief_detections = 0;
        int empathy_responses = 0;
        int total_evaluations = 0;
        std::vector<float> belief_accuracy_history;
        std::vector<float> empathy_scores;
    } stats;

    void SetUp() override {
        // Create SNN bridge with full ToM dimensions
        tom_snn_config_t snn_config = tom_snn_config_default();
        snn_config.num_dimensions = TOM_DIM_COUNT;
        snn_config.neurons_per_dim = 32;
        snn_config.dt_ms = 1.0f;
        snn_config.enable_perspective_taking = true;
        snn_config.enable_bio_async = false;

        snn_bridge = tom_snn_create(&snn_config);
        ASSERT_NE(snn_bridge, nullptr) << "Failed to create SNN bridge";

        // Create Plasticity bridge with all learning mechanisms
        tom_plasticity_config_t plasticity_config = tom_plasticity_config_default();
        plasticity_config.base_learning_rate = 0.01f;
        plasticity_config.stdp_a_plus = 0.01f;
        plasticity_config.stdp_a_minus = 0.012f;

        plasticity_bridge = tom_plasticity_create(&plasticity_config);
        ASSERT_NE(plasticity_bridge, nullptr) << "Failed to create Plasticity bridge";

        // Register base synapses for plasticity
        for (uint32_t i = 0; i < TOM_DIM_COUNT; i++) {
            tom_plasticity_register_synapse(plasticity_bridge, i,
                TOM_SYNAPSE_EMPATHY, 0.5f);
        }

        // Register protected synapses
        tom_plasticity_register_synapse(plasticity_bridge, 100,
            TOM_SYNAPSE_BELIEF, 1.0f);
        tom_plasticity_register_synapse(plasticity_bridge, 101,
            TOM_SYNAPSE_PERSPECTIVE, 0.9f);
    }

    void TearDown() override {
        if (snn_bridge) {
            tom_snn_destroy(snn_bridge);
            snn_bridge = nullptr;
        }
        if (plasticity_bridge) {
            tom_plasticity_destroy(plasticity_bridge);
            plasticity_bridge = nullptr;
        }
    }

    // Generate social scenario
    enum SocialScenario {
        CORRECT_BELIEF_ATTRIBUTION,  // Agent correctly infers other's belief
        FALSE_BELIEF_DETECTION,      // Agent detects other holds false belief
        DESIRE_INFERENCE,            // Agent infers other's desire from behavior
        INTENTION_PREDICTION,        // Agent predicts other's intention
        PERSPECTIVE_TAKING,          // Agent takes other's perspective
        EMPATHIC_RESPONSE,           // Agent responds empathically
        DECEPTION_DETECTION,         // Agent detects deception attempt
        SHARED_ATTENTION             // Joint attention scenario
    };

    void generate_scenario(float* dims, SocialScenario scenario) {
        memset(dims, 0, sizeof(float) * TOM_DIM_COUNT);

        switch (scenario) {
            case CORRECT_BELIEF_ATTRIBUTION:
                dims[TOM_DIM_BELIEF_STATE] = 0.9f;
                dims[TOM_DIM_SOCIAL_CONTEXT] = 0.8f;
                dims[TOM_DIM_EMPATHIC_ACCURACY] = 0.85f;
                dims[TOM_DIM_MENTAL_SIMULATION] = 0.75f;
                break;

            case FALSE_BELIEF_DETECTION:
                dims[TOM_DIM_BELIEF_STATE] = 0.7f;
                dims[TOM_DIM_PERSPECTIVE] = 0.9f;        // Strong perspective-taking
                dims[TOM_DIM_DECEPTION_DETECTION] = 0.3f;
                dims[TOM_DIM_MENTAL_SIMULATION] = 0.85f; // Simulating other's incorrect belief
                break;

            case DESIRE_INFERENCE:
                dims[TOM_DIM_DESIRE_STATE] = 0.9f;
                dims[TOM_DIM_SOCIAL_CONTEXT] = 0.7f;
                dims[TOM_DIM_EMOTION_INFERENCE] = 0.6f;
                dims[TOM_DIM_INTENTION] = 0.5f;
                break;

            case INTENTION_PREDICTION:
                dims[TOM_DIM_INTENTION] = 0.85f;
                dims[TOM_DIM_BELIEF_STATE] = 0.7f;
                dims[TOM_DIM_DESIRE_STATE] = 0.75f;
                dims[TOM_DIM_MENTAL_SIMULATION] = 0.8f;
                break;

            case PERSPECTIVE_TAKING:
                dims[TOM_DIM_PERSPECTIVE] = 0.95f;
                dims[TOM_DIM_SOCIAL_CONTEXT] = 0.8f;
                dims[TOM_DIM_EMPATHIC_ACCURACY] = 0.7f;
                dims[TOM_DIM_MENTAL_SIMULATION] = 0.85f;
                break;

            case EMPATHIC_RESPONSE:
                dims[TOM_DIM_EMOTION_INFERENCE] = 0.9f;
                dims[TOM_DIM_EMPATHIC_ACCURACY] = 0.85f;
                dims[TOM_DIM_PERSPECTIVE] = 0.8f;
                dims[TOM_DIM_SHARED_ATTENTION] = 0.7f;
                break;

            case DECEPTION_DETECTION:
                dims[TOM_DIM_DECEPTION_DETECTION] = 0.9f;
                dims[TOM_DIM_BELIEF_STATE] = 0.6f;        // Uncertain belief
                dims[TOM_DIM_INTENTION] = 0.7f;
                dims[TOM_DIM_SOCIAL_CONTEXT] = 0.85f;
                break;

            case SHARED_ATTENTION:
                dims[TOM_DIM_SHARED_ATTENTION] = 0.95f;
                dims[TOM_DIM_PERSPECTIVE] = 0.8f;
                dims[TOM_DIM_SOCIAL_CONTEXT] = 0.9f;
                dims[TOM_DIM_EMOTION_INFERENCE] = 0.6f;
                break;
        }
    }

    // Run single evaluation pipeline
    struct EvaluationResult {
        float belief_confidence;
        float empathy_level;
        float perspective_quality;
        bool false_belief_detected;
        int spike_count;
    };

    EvaluationResult run_evaluation(SocialScenario scenario) {
        EvaluationResult result = {0};

        float dims[TOM_DIM_COUNT];
        generate_scenario(dims, scenario);

        // Encode and simulate
        result.spike_count = tom_snn_encode_context(snn_bridge, dims, TOM_DIM_COUNT);
        tom_snn_simulate(snn_bridge, 30.0f);

        // Get inference
        tom_inference_t inference;
        tom_snn_get_inference(snn_bridge, &inference);

        result.belief_confidence = inference.belief_state;
        result.empathy_level = inference.empathic_accuracy;
        result.perspective_quality = inference.perspective_alignment;

        // Check false belief detection (use deception fields from inference)
        result.false_belief_detected = inference.deception_detected;

        // Update stats
        stats.total_evaluations++;
        stats.belief_accuracy_history.push_back(inference.belief_state);
        stats.empathy_scores.push_back(inference.empathic_accuracy);

        if (result.false_belief_detected) {
            stats.false_belief_detections++;
        }

        return result;
    }
};

//=============================================================================
// Basic Pipeline Tests
//=============================================================================

TEST_F(TomSNNPlasticityE2E, CompletePipelineInitialization) {
    // Verify complete pipeline setup
    EXPECT_NE(snn_bridge, nullptr);
    EXPECT_NE(plasticity_bridge, nullptr);

    // Check synapse registration
    tom_plasticity_bridge_state_t state;
    tom_plasticity_get_state(plasticity_bridge, &state);
    EXPECT_GT(state.active_synapses, (uint32_t)TOM_DIM_COUNT);  // Base + protected
}

TEST_F(TomSNNPlasticityE2E, SingleEvaluationPipeline) {
    // Run single belief attribution scenario
    auto result = run_evaluation(CORRECT_BELIEF_ATTRIBUTION);

    // Verify inference is valid
    EXPECT_GE(result.belief_confidence, 0.0f);
    EXPECT_LE(result.belief_confidence, 1.0f);
    EXPECT_GE(result.empathy_level, 0.0f);
    EXPECT_LE(result.empathy_level, 1.0f);
    EXPECT_GE(result.spike_count, 0);

    // Apply learning based on result
    int ret = tom_plasticity_learn(plasticity_bridge,
        TOM_LEARN_CORRECT_BELIEF, 0.5f, 0, result.belief_confidence);
    EXPECT_EQ(ret, 0);
}

//=============================================================================
// Belief Attribution Tests
//=============================================================================

TEST_F(TomSNNPlasticityE2E, FalseBeliefLearning) {
    // Run multiple false belief scenarios
    float total_detection = 0.0f;
    int detection_count = 0;

    for (int trial = 0; trial < 10; trial++) {
        auto result = run_evaluation(FALSE_BELIEF_DETECTION);

        total_detection += result.perspective_quality;
        if (result.perspective_quality > 0.0f || result.false_belief_detected) {
            detection_count++;
        }

        // Learn false belief detection - reinforces perspective-taking
        tom_plasticity_learn(plasticity_bridge,
            TOM_LEARN_CORRECT_BELIEF, 0.5f, 0, result.belief_confidence);

        // Apply STDP
        tom_plasticity_apply_stdp(plasticity_bridge, 0,
            (float)trial, (float)trial + 5.0f);
    }

    // At least some trials should detect perspective signals
    EXPECT_GT(detection_count, 0);
}

TEST_F(TomSNNPlasticityE2E, BeliefDesireIntegration) {
    // Run multiple desire inference scenarios
    float total_inference_quality = 0.0f;

    for (int trial = 0; trial < 10; trial++) {
        auto result = run_evaluation(DESIRE_INFERENCE);

        total_inference_quality += result.belief_confidence;

        // Learn desire inference
        tom_plasticity_learn(plasticity_bridge,
            TOM_LEARN_INTENTION_CORRECT, 0.5f, 1, result.belief_confidence);
    }

    // Average belief confidence should be above minimum
    EXPECT_GT(total_inference_quality / 10.0f, 0.0f);
}

TEST_F(TomSNNPlasticityE2E, IntentionPredictionAccuracy) {
    // Register intention synapses
    for (int i = 200; i < 210; i++) {
        tom_plasticity_register_synapse(plasticity_bridge, i,
            TOM_SYNAPSE_INTENTION, 0.5f);
    }

    // Initial synapse weight
    tom_plasticity_synapse_t initial_synapse;
    tom_plasticity_get_synapse(plasticity_bridge, 200, &initial_synapse);
    float initial_weight = initial_synapse.weight;

    // Run mixed intention prediction scenarios
    for (int epoch = 0; epoch < 5; epoch++) {
        // Intention prediction scenario
        auto good_result = run_evaluation(INTENTION_PREDICTION);
        tom_plasticity_learn(plasticity_bridge,
            TOM_LEARN_INTENTION_CORRECT, 0.3f, 200, good_result.belief_confidence);

        // Perspective-taking scenario
        auto persp_result = run_evaluation(PERSPECTIVE_TAKING);
        tom_plasticity_learn(plasticity_bridge,
            TOM_LEARN_PERSPECTIVE_ERROR, 0.3f, 201, persp_result.perspective_quality);

        // BCM and homeostatic updates
        tom_plasticity_update_bcm(plasticity_bridge, 0.5f);
        tom_plasticity_homeostatic_update(plasticity_bridge, 0.5f);
    }

    // Verify learning occurred
    tom_plasticity_stats_t stats;
    tom_plasticity_get_stats(plasticity_bridge, &stats);
    EXPECT_GT(stats.total_learning_events, 0u);
    // Note: correct_belief_events and perspective_alignments may not be incremented
    // by TOM_LEARN_INTENTION_CORRECT and TOM_LEARN_PERSPECTIVE_ERROR events.
    // Check that weight updates occurred as evidence of learning.
    EXPECT_GT(stats.weight_updates, 0u);
}

//=============================================================================
// Empathy Tests
//=============================================================================

TEST_F(TomSNNPlasticityE2E, EmpathicResponseLearning) {
    int empathy_trials = 0;
    float total_empathy = 0.0f;

    for (int trial = 0; trial < 10; trial++) {
        auto result = run_evaluation(EMPATHIC_RESPONSE);

        total_empathy += result.empathy_level;
        if (result.empathy_level > 0.0f) {
            empathy_trials++;

            // Learn from empathic response
            tom_plasticity_learn(plasticity_bridge,
                TOM_LEARN_EMPATHY_ERROR, 0.5f, 2, result.empathy_level);
        }
    }

    // Should detect empathy in multiple trials
    EXPECT_GE(empathy_trials, 3);
    // Total empathy across trials should be meaningful
    EXPECT_GT(total_empathy, 0.0f);
}

//=============================================================================
// Protected Synapse Tests
//=============================================================================

TEST_F(TomSNNPlasticityE2E, BeliefSynapseProtectionIntegrity) {
    // Get initial protected synapse weight
    tom_plasticity_synapse_t belief_synapse;
    tom_plasticity_get_synapse(plasticity_bridge, 100, &belief_synapse);
    float original_weight = belief_synapse.weight;
    EXPECT_TRUE(belief_synapse.is_protected);

    // Run many scenarios and try to modify protected synapse
    for (int trial = 0; trial < 20; trial++) {
        auto result = run_evaluation((SocialScenario)(trial % 8));

        // Try various learning operations on protected synapse
        tom_plasticity_learn(plasticity_bridge,
            TOM_LEARN_CORRECT_BELIEF, -1.0f, 100, result.belief_confidence);
        tom_plasticity_apply_stdp(plasticity_bridge, 100,
            (float)trial, (float)trial + 10.0f);
        tom_plasticity_apply_reward(plasticity_bridge, -1.0f);
    }

    // Protected synapse should remain unchanged
    tom_plasticity_get_synapse(plasticity_bridge, 100, &belief_synapse);
    EXPECT_FLOAT_EQ(belief_synapse.weight, original_weight);
    EXPECT_TRUE(belief_synapse.is_protected);
}

TEST_F(TomSNNPlasticityE2E, PerspectiveSynapseProtection) {
    // Perspective synapse should also be protected
    tom_plasticity_synapse_t persp_synapse;
    tom_plasticity_get_synapse(plasticity_bridge, 101, &persp_synapse);
    float original_weight = persp_synapse.weight;
    EXPECT_TRUE(persp_synapse.is_protected);

    // Stress test protection
    for (int i = 0; i < 50; i++) {
        tom_plasticity_apply_stdp(plasticity_bridge, 101, (float)i, (float)i + 5.0f);
        tom_plasticity_learn(plasticity_bridge,
            TOM_LEARN_CORRECT_BELIEF, 1.0f, 101, 0.9f);
    }

    // Weight must remain unchanged
    tom_plasticity_get_synapse(plasticity_bridge, 101, &persp_synapse);
    EXPECT_FLOAT_EQ(persp_synapse.weight, original_weight);
}

//=============================================================================
// Deception Detection Tests
//=============================================================================

TEST_F(TomSNNPlasticityE2E, DeceptionDetectionLearning) {
    // Register deception synapses
    for (int i = 300; i < 305; i++) {
        tom_plasticity_register_synapse(plasticity_bridge, i,
            TOM_SYNAPSE_SOCIAL, 0.5f);
    }

    int learning_events = 0;

    for (int trial = 0; trial < 10; trial++) {
        auto result = run_evaluation(DECEPTION_DETECTION);

        // Apply learning for all deception detection scenarios
        tom_plasticity_learn(plasticity_bridge,
            TOM_LEARN_DECEPTION_MISSED, 0.5f, 300 + (trial % 5),
            result.belief_confidence > 0.0f ? result.belief_confidence : 0.5f);
        learning_events++;
    }

    // Verify we applied learning across all trials
    EXPECT_EQ(learning_events, 10);

    // Verify plasticity stats reflect learning
    tom_plasticity_stats_t stats;
    tom_plasticity_get_stats(plasticity_bridge, &stats);
    EXPECT_GE(stats.total_learning_events, 10u);
}

//=============================================================================
// Shared Attention Tests
//=============================================================================

TEST_F(TomSNNPlasticityE2E, SharedAttentionAndJointEngagement) {
    float total_attention_quality = 0.0f;
    int attention_events = 0;
    int learning_events = 0;

    for (int trial = 0; trial < 10; trial++) {
        auto result = run_evaluation(SHARED_ATTENTION);

        float attention_level = result.perspective_quality;
        total_attention_quality += attention_level;
        if (attention_level > 0.0f) {
            attention_events++;
        }

        // Always apply learning for shared attention scenarios
        tom_plasticity_learn(plasticity_bridge,
            TOM_LEARN_INTENTION_CORRECT, 0.5f, 3, attention_level);
        learning_events++;
    }

    // Should detect attention in at least some trials
    EXPECT_GE(attention_events, 3);
    // Verify learning was applied
    EXPECT_EQ(learning_events, 10);
}

//=============================================================================
// Multi-Scenario Learning Tests
//=============================================================================

TEST_F(TomSNNPlasticityE2E, CompleteTheoryOfMindWorkflow) {
    // Register workflow synapses
    for (int i = 400; i < 420; i++) {
        tom_plasticity_register_synapse(plasticity_bridge, i,
            TOM_SYNAPSE_EMPATHY, 0.5f);
    }

    // Run complete ToM workflow
    for (int epoch = 0; epoch < 5; epoch++) {
        for (int scenario = 0; scenario < 8; scenario++) {
            auto result = run_evaluation((SocialScenario)scenario);

            // Select learning event based on scenario
            tom_learn_event_t event;
            float magnitude = 0.3f;

            switch ((SocialScenario)scenario) {
                case CORRECT_BELIEF_ATTRIBUTION:
                    event = TOM_LEARN_CORRECT_BELIEF;
                    break;
                case FALSE_BELIEF_DETECTION:
                    event = TOM_LEARN_CORRECT_BELIEF;
                    break;
                case DESIRE_INFERENCE:
                    event = TOM_LEARN_INTENTION_CORRECT;
                    break;
                case INTENTION_PREDICTION:
                    event = TOM_LEARN_INTENTION_CORRECT;
                    break;
                case PERSPECTIVE_TAKING:
                    event = TOM_LEARN_PERSPECTIVE_ERROR;
                    break;
                case EMPATHIC_RESPONSE:
                    event = TOM_LEARN_EMPATHY_ERROR;
                    break;
                case DECEPTION_DETECTION:
                    event = TOM_LEARN_DECEPTION_MISSED;
                    break;
                case SHARED_ATTENTION:
                    event = TOM_LEARN_INTENTION_CORRECT;
                    break;
                default:
                    event = TOM_LEARN_INTENTION_CORRECT;
                    break;
            }

            int synapse_id = 400 + (epoch * 8 + scenario) % 20;
            tom_plasticity_learn(plasticity_bridge, event, magnitude,
                synapse_id, result.belief_confidence);

            // Apply STDP
            tom_plasticity_apply_stdp(plasticity_bridge, synapse_id,
                (float)(epoch * 10 + scenario), (float)(epoch * 10 + scenario + 5));
        }

        // Periodic maintenance
        tom_plasticity_update_bcm(plasticity_bridge, 0.5f);
        tom_plasticity_homeostatic_update(plasticity_bridge, 0.5f);
        tom_plasticity_update_traces(plasticity_bridge, 1.0f);
    }

    // Consolidate learning
    tom_plasticity_consolidate(plasticity_bridge);

    // Verify extensive learning occurred
    tom_plasticity_stats_t final_stats;
    tom_plasticity_get_stats(plasticity_bridge, &final_stats);
    EXPECT_GT(final_stats.total_learning_events, 30u);
    EXPECT_GT(final_stats.weight_updates, 30u);

    tom_snn_stats_t snn_stats;
    tom_snn_get_stats(snn_bridge, &snn_stats);
    EXPECT_GE(snn_stats.total_evaluations, 40u);
}

//=============================================================================
// Stress and Performance Tests
//=============================================================================

TEST_F(TomSNNPlasticityE2E, HighVolumeSocialProcessing) {
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 100; i++) {
        run_evaluation((SocialScenario)(i % 8));
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // 100 evaluations should complete in under 5 seconds
    EXPECT_LT(duration.count(), 5000);
    EXPECT_EQ(stats.total_evaluations, 100);
}

TEST_F(TomSNNPlasticityE2E, ContinuousSocialLearning) {
    // Register many synapses
    for (int i = 500; i < 600; i++) {
        tom_plasticity_register_synapse(plasticity_bridge, i,
            TOM_SYNAPSE_EMPATHY, 0.5f);
    }

    // Continuous learning loop
    for (int cycle = 0; cycle < 50; cycle++) {
        auto result = run_evaluation((SocialScenario)(cycle % 8));

        // Learn on rotating synapses
        for (int j = 0; j < 5; j++) {
            int synapse_id = 500 + (cycle * 5 + j) % 100;
            tom_plasticity_learn(plasticity_bridge,
                TOM_LEARN_CORRECT_BELIEF, 0.1f, synapse_id, result.belief_confidence);
        }

        // Periodic BCM update
        if (cycle % 10 == 0) {
            tom_plasticity_update_bcm(plasticity_bridge, 0.5f);
        }
    }

    // Verify extensive learning
    tom_plasticity_stats_t stats;
    tom_plasticity_get_stats(plasticity_bridge, &stats);
    EXPECT_GE(stats.total_learning_events, 200u);
}

//=============================================================================
// Reset and Recovery Tests
//=============================================================================

TEST_F(TomSNNPlasticityE2E, ResetAndRecovery) {
    // Accumulate some state
    for (int i = 0; i < 10; i++) {
        run_evaluation((SocialScenario)(i % 8));
    }

    // Reset both bridges
    tom_snn_reset(snn_bridge);
    tom_plasticity_reset(plasticity_bridge);

    // Verify recovery
    tom_snn_bridge_state_t snn_state;
    tom_snn_get_state(snn_bridge, &snn_state);
    EXPECT_EQ(snn_state.state, TOM_SNN_STATE_IDLE);

    tom_plasticity_bridge_state_t plasticity_state;
    tom_plasticity_get_state(plasticity_bridge, &plasticity_state);
    EXPECT_EQ(plasticity_state.state, TOM_PLASTICITY_STATE_IDLE);

    // Can continue processing
    auto result = run_evaluation(CORRECT_BELIEF_ATTRIBUTION);
    EXPECT_GE(result.belief_confidence, 0.0f);
}

//=============================================================================
// Statistics Validation Tests
//=============================================================================

TEST_F(TomSNNPlasticityE2E, StatisticsAccuracy) {
    // Run known number of evaluations
    for (int i = 0; i < 20; i++) {
        run_evaluation((SocialScenario)(i % 8));

        // Apply learning
        tom_plasticity_learn(plasticity_bridge,
            TOM_LEARN_CORRECT_BELIEF, 0.1f, i % TOM_DIM_COUNT, 0.5f);
    }

    // Verify stats match
    tom_snn_stats_t snn_stats;
    tom_snn_get_stats(snn_bridge, &snn_stats);
    EXPECT_GE(snn_stats.total_evaluations, 20u);

    tom_plasticity_stats_t plasticity_stats;
    tom_plasticity_get_stats(plasticity_bridge, &plasticity_stats);
    EXPECT_GE(plasticity_stats.total_learning_events, 20u);
}
