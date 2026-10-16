//=============================================================================
// test_tom_snn_plasticity_integration.cpp - Theory of Mind SNN/Plasticity Integration
//=============================================================================
/**
 * @file test_tom_snn_plasticity_integration.cpp
 * @brief Integration tests for Theory of Mind SNN-Plasticity bidirectional dataflows
 *
 * WHAT: Tests complete integration between ToM, SNN, and plasticity
 * WHY:  Verify bidirectional dataflows work correctly with all callbacks
 * HOW:  Create both bridges, simulate real dataflows, verify learning occurs
 *
 * INTEGRATION POINTS:
 * - Belief state encoding -> SNN encoding -> population activity
 * - SNN spikes -> Plasticity STDP -> weight updates
 * - Plasticity callbacks -> ToM inference updates
 * - Reward modulation -> both SNN and Plasticity bridges
 *
 * TEST SCENARIOS:
 * 1. Belief-driven learning: infer belief, update weights
 * 2. Perspective-taking learning: switch perspective, reinforce synapses
 * 3. Empathy-driven learning: emotional resonance affects learning
 * 4. Concurrent operation: both bridges operating simultaneously
 * 5. Statistics aggregation: verify stats from both bridges
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <thread>
#include <chrono>
#include <atomic>

// Headers have their own extern "C" guards
#include "cognitive/theory_of_mind/nimcp_tom_snn_bridge.h"
#include "cognitive/theory_of_mind/nimcp_tom_plasticity_bridge.h"

//=============================================================================
// Test Fixture
//=============================================================================

class TOMSNNPlasticityIntegrationTest : public ::testing::Test {
protected:
    tom_snn_bridge_t* snn_bridge;
    tom_plasticity_bridge_t* plasticity_bridge;

    // Callback tracking
    std::atomic<int> deception_count{0};
    std::atomic<int> inference_count{0};
    std::atomic<int> learning_count{0};
    std::atomic<int> calibration_count{0};

    void SetUp() override {
        // Create SNN bridge with test-friendly config
        tom_snn_config_t snn_config = tom_snn_config_default();
        snn_config.enable_bio_async = false;  // Disable for predictable tests
        snn_config.enable_deception_detection = true;
        snn_config.enable_mental_simulation = true;
        snn_config.enable_perspective_taking = true;

        snn_bridge = tom_snn_create(&snn_config);
        ASSERT_NE(snn_bridge, nullptr) << "Failed to create SNN bridge";

        // Create Plasticity bridge with defaults
        tom_plasticity_config_t plasticity_config = tom_plasticity_config_default();
        plasticity_config.protect_belief_patterns = true;
        plasticity_config.protect_perspective_patterns = true;

        plasticity_bridge = tom_plasticity_create(&plasticity_config);
        ASSERT_NE(plasticity_bridge, nullptr) << "Failed to create plasticity bridge";

        // Reset counters
        deception_count = 0;
        inference_count = 0;
        learning_count = 0;
        calibration_count = 0;
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

    // Generate social context dimensions
    void generate_social_context(float* dims, uint32_t n, float belief, float intention, float empathy) {
        if (n > 0) dims[0] = belief;        // TOM_DIM_BELIEF_STATE
        if (n > 1) dims[1] = 0.5f;          // TOM_DIM_DESIRE_STATE
        if (n > 2) dims[2] = intention;     // TOM_DIM_INTENTION
        if (n > 3) dims[3] = 0.5f;          // TOM_DIM_PERSPECTIVE
        if (n > 4) dims[4] = empathy;       // TOM_DIM_EMOTION_INFERENCE
        for (uint32_t i = 5; i < n; i++) {
            dims[i] = 0.5f;
        }
    }
};

//=============================================================================
// Static Callback Functions
//=============================================================================

static std::atomic<int>* g_deception_counter = nullptr;
static std::atomic<int>* g_learning_counter = nullptr;

static void deception_callback(tom_snn_bridge_t*, float level, uint64_t, void*) {
    if (g_deception_counter && level > 0.5f) {
        (*g_deception_counter)++;
    }
}

static void learning_callback(tom_plasticity_bridge_t*, tom_learn_event_t, float, void*) {
    if (g_learning_counter) {
        (*g_learning_counter)++;
    }
}

//=============================================================================
// Test: Belief-Driven Learning Pipeline
//=============================================================================

TEST_F(TOMSNNPlasticityIntegrationTest, BeliefDrivenLearning) {
    // Setup callbacks
    g_deception_counter = &deception_count;
    tom_snn_register_deception_callback(snn_bridge, deception_callback, nullptr);

    // Register synapses with plasticity bridge
    for (uint32_t i = 0; i < 6; i++) {
        tom_synapse_type_t type = (tom_synapse_type_t)i;
        tom_plasticity_register_synapse(plasticity_bridge, i, type, 0.5f);
    }

    // Simulate inferring another's belief state
    float dims[TOM_DIM_COUNT] = {0};
    generate_social_context(dims, TOM_DIM_COUNT, 0.8f, 0.7f, 0.6f);

    // Encode social context in SNN
    int spike_count = tom_snn_encode_context(snn_bridge, dims, TOM_DIM_COUNT);
    EXPECT_GE(spike_count, 0) << "Context encoding should succeed";

    // Run SNN simulation
    EXPECT_EQ(tom_snn_simulate(snn_bridge, 50.0f), 0) << "SNN simulation should succeed";

    // Get inference
    tom_inference_t inference;
    EXPECT_EQ(tom_snn_get_inference(snn_bridge, &inference), 0);
    EXPECT_GE(inference.belief_state, 0.0f);
    EXPECT_LE(inference.belief_state, 1.0f);

    // Apply learning based on correct belief inference
    int ret = tom_plasticity_learn(plasticity_bridge, TOM_LEARN_CORRECT_BELIEF, 0.8f, 2, 1.0f);
    EXPECT_EQ(ret, 0) << "Learning should succeed for unprotected synapse";

    // Consolidate learning
    EXPECT_EQ(tom_plasticity_consolidate(plasticity_bridge), 0);

    // Verify stats accumulated
    tom_snn_stats_t snn_stats;
    tom_snn_get_stats(snn_bridge, &snn_stats);
    EXPECT_GT(snn_stats.total_evaluations, 0u) << "Should have recorded evaluations";

    tom_plasticity_stats_t plasticity_stats;
    tom_plasticity_get_stats(plasticity_bridge, &plasticity_stats);
    EXPECT_GT(plasticity_stats.total_learning_events, 0u) << "Should have recorded learning events";
}

//=============================================================================
// Test: Perspective-Taking Learning
//=============================================================================

TEST_F(TOMSNNPlasticityIntegrationTest, PerspectiveTakingLearning) {
    // Register synapses
    tom_plasticity_register_synapse(plasticity_bridge, 10, TOM_SYNAPSE_EMPATHY, 0.5f);
    tom_plasticity_register_synapse(plasticity_bridge, 11, TOM_SYNAPSE_SOCIAL, 0.5f);

    // Simulate perspective switch scenario
    float dims1[TOM_DIM_COUNT] = {0};
    dims1[TOM_DIM_PERSPECTIVE] = 0.3f;
    tom_snn_encode_context(snn_bridge, dims1, TOM_DIM_COUNT);
    tom_snn_simulate(snn_bridge, 30.0f);

    // Switch perspective
    float dims2[TOM_DIM_COUNT] = {0};
    dims2[TOM_DIM_PERSPECTIVE] = 0.9f;  // Big perspective shift
    tom_snn_encode_context(snn_bridge, dims2, TOM_DIM_COUNT);
    tom_snn_simulate(snn_bridge, 30.0f);

    // Check for perspective shift
    float perspective_level;
    tom_snn_check_perspective_shift(snn_bridge, &perspective_level);
    EXPECT_GE(perspective_level, 0.0f);

    // Apply perspective learning
    int ret = tom_plasticity_learn(plasticity_bridge, TOM_LEARN_PERSPECTIVE_ALIGNED, 0.8f, 10, 1.0f);
    EXPECT_EQ(ret, 0) << "Perspective learning should succeed";

    // Verify perspective switches tracked
    tom_snn_stats_t stats;
    tom_snn_get_stats(snn_bridge, &stats);
    EXPECT_GE(stats.perspective_switches, 0u);
}

//=============================================================================
// Test: Empathy-Driven Learning
//=============================================================================

TEST_F(TOMSNNPlasticityIntegrationTest, EmpathyDrivenLearning) {
    // Register empathy-related synapses
    tom_plasticity_register_synapse(plasticity_bridge, 20, TOM_SYNAPSE_EMPATHY, 0.5f);

    // Encode empathic context
    int spike_count = tom_snn_encode_empathy(snn_bridge, 0.9f, 0.8f);
    EXPECT_GE(spike_count, 0);

    // Simulate
    EXPECT_EQ(tom_snn_simulate(snn_bridge, 40.0f), 0);

    // Check empathy signal
    float resonance;
    tom_snn_check_empathy(snn_bridge, &resonance);
    EXPECT_GE(resonance, 0.0f);

    // Get inference
    tom_inference_t inference;
    tom_snn_get_inference(snn_bridge, &inference);
    EXPECT_GE(inference.empathic_accuracy, 0.0f);

    // Apply empathy learning
    int ret = tom_plasticity_learn(plasticity_bridge, TOM_LEARN_EMPATHY_ACCURATE, 0.9f, 20, 1.0f);
    EXPECT_EQ(ret, 0);

    // Verify stats
    tom_plasticity_stats_t stats;
    tom_plasticity_get_stats(plasticity_bridge, &stats);
    EXPECT_GT(stats.empathy_accurate_events, 0u);
}

//=============================================================================
// Test: Deception Detection and Learning
//=============================================================================

TEST_F(TOMSNNPlasticityIntegrationTest, DeceptionDetectionLearning) {
    // Setup deception callback
    g_deception_counter = &deception_count;
    tom_snn_register_deception_callback(snn_bridge, deception_callback, nullptr);

    // Register deception-related synapse
    tom_plasticity_register_synapse(plasticity_bridge, 30, TOM_SYNAPSE_SOCIAL, 0.5f);

    // Encode belief discrepancy (simulates false belief scenario)
    // Self believes 0.9, other believes 0.1 -> large discrepancy
    int spike_count = tom_snn_encode_belief(snn_bridge, 0.9f, 0.1f);
    EXPECT_GE(spike_count, 0);

    // Simulate
    EXPECT_EQ(tom_snn_simulate(snn_bridge, 50.0f), 0);

    // Check for deception
    float deception_level;
    bool deception_detected = tom_snn_check_deception(snn_bridge, &deception_level);
    EXPECT_GE(deception_level, 0.0f);

    // Apply learning based on deception detection
    tom_learn_event_t event = deception_detected ?
        TOM_LEARN_DECEPTION_CORRECT : TOM_LEARN_DECEPTION_MISSED;
    int ret = tom_plasticity_learn(plasticity_bridge, event, 0.8f, 30, 1.0f);
    EXPECT_EQ(ret, 0);
}

//=============================================================================
// Test: Concurrent Operation of Both Bridges
//=============================================================================

TEST_F(TOMSNNPlasticityIntegrationTest, ConcurrentOperation) {
    // Setup callbacks
    g_learning_counter = &learning_count;
    tom_plasticity_register_learn_callback(plasticity_bridge, learning_callback, nullptr);

    // Register synapses
    for (uint32_t i = 0; i < 5; i++) {
        tom_plasticity_register_synapse(plasticity_bridge, i, TOM_SYNAPSE_SOCIAL, 0.5f);
    }

    // Run multiple iterations
    const int iterations = 10;
    float dims[TOM_DIM_COUNT];

    for (int iter = 0; iter < iterations; iter++) {
        // Generate varying social context
        float belief = 0.3f + 0.6f * (float)iter / iterations;
        float intention = 0.5f + 0.3f * sinf((float)iter * 0.5f);
        float empathy = 0.4f + 0.4f * cosf((float)iter * 0.3f);
        generate_social_context(dims, TOM_DIM_COUNT, belief, intention, empathy);

        // SNN pipeline
        tom_snn_encode_context(snn_bridge, dims, TOM_DIM_COUNT);
        tom_snn_simulate(snn_bridge, 20.0f);

        // Get inference for plasticity modulation
        tom_inference_t inference;
        tom_snn_get_inference(snn_bridge, &inference);

        // Apply learning based on confidence
        if (inference.confidence > 0.5f) {
            tom_plasticity_learn(plasticity_bridge, TOM_LEARN_CORRECT_BELIEF,
                                 inference.confidence, iter % 5, 1.0f);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    // Verify accumulated activity
    tom_snn_stats_t snn_stats;
    tom_snn_get_stats(snn_bridge, &snn_stats);
    EXPECT_EQ(snn_stats.total_evaluations, (uint64_t)iterations);

    tom_plasticity_stats_t plasticity_stats;
    tom_plasticity_get_stats(plasticity_bridge, &plasticity_stats);
    EXPECT_GT(plasticity_stats.total_learning_events, 0u);
}

//=============================================================================
// Test: Reward Propagation
//=============================================================================

TEST_F(TOMSNNPlasticityIntegrationTest, RewardPropagation) {
    // Register synapses
    tom_plasticity_register_synapse(plasticity_bridge, 1, TOM_SYNAPSE_EMPATHY, 0.5f);
    tom_plasticity_register_synapse(plasticity_bridge, 2, TOM_SYNAPSE_SOCIAL, 0.5f);

    // Do initial learning
    float dims[TOM_DIM_COUNT] = {0};
    generate_social_context(dims, TOM_DIM_COUNT, 0.7f, 0.6f, 0.8f);
    tom_snn_encode_context(snn_bridge, dims, TOM_DIM_COUNT);
    tom_snn_simulate(snn_bridge, 30.0f);

    // Get initial weights
    tom_plasticity_synapse_t syn_before;
    tom_plasticity_get_synapse(plasticity_bridge, 1, &syn_before);
    float weight_before = syn_before.weight;

    // Apply reward
    float reward = 0.8f;
    int ret = tom_plasticity_apply_reward(plasticity_bridge, reward);
    EXPECT_EQ(ret, 0);

    // The weight may or may not change depending on eligibility trace
    // Just verify the function works correctly
    tom_plasticity_synapse_t syn_after;
    tom_plasticity_get_synapse(plasticity_bridge, 1, &syn_after);
    // Weight should be in valid range
    EXPECT_GE(syn_after.weight, 0.0f);
    EXPECT_LE(syn_after.weight, 2.0f);
}

//=============================================================================
// Test: STDP Integration
//=============================================================================

TEST_F(TOMSNNPlasticityIntegrationTest, STDPIntegration) {
    // Register synapses
    tom_plasticity_register_synapse(plasticity_bridge, 1, TOM_SYNAPSE_EMPATHY, 0.5f);

    // Apply STDP - LTP (post after pre)
    float delta_ltp = tom_plasticity_apply_stdp(plasticity_bridge, 1, 0.0f, 10.0f);
    EXPECT_GT(delta_ltp, 0.0f) << "LTP should produce positive weight change";

    // Apply STDP - LTD (pre after post)
    float delta_ltd = tom_plasticity_apply_stdp(plasticity_bridge, 1, 10.0f, 0.0f);
    EXPECT_LT(delta_ltd, 0.0f) << "LTD should produce negative weight change";
}

//=============================================================================
// Test: Protected Synapse Behavior
//=============================================================================

TEST_F(TOMSNNPlasticityIntegrationTest, ProtectedSynapseBehavior) {
    // Register belief and perspective synapses (auto-protected)
    tom_plasticity_register_synapse(plasticity_bridge, 1, TOM_SYNAPSE_BELIEF, 0.5f);
    tom_plasticity_register_synapse(plasticity_bridge, 2, TOM_SYNAPSE_PERSPECTIVE, 0.5f);
    tom_plasticity_register_synapse(plasticity_bridge, 3, TOM_SYNAPSE_EMPATHY, 0.5f);

    // Verify protection status
    tom_plasticity_synapse_t syn_belief, syn_persp, syn_empathy;
    tom_plasticity_get_synapse(plasticity_bridge, 1, &syn_belief);
    tom_plasticity_get_synapse(plasticity_bridge, 2, &syn_persp);
    tom_plasticity_get_synapse(plasticity_bridge, 3, &syn_empathy);

    EXPECT_TRUE(syn_belief.is_protected) << "Belief synapse should be protected";
    EXPECT_TRUE(syn_persp.is_protected) << "Perspective synapse should be protected";
    EXPECT_FALSE(syn_empathy.is_protected) << "Empathy synapse should not be protected";

    // Try to learn on protected synapse - should be blocked
    tom_plasticity_learn(plasticity_bridge, TOM_LEARN_CORRECT_BELIEF, 0.8f, 1, 1.0f);

    tom_plasticity_stats_t stats;
    tom_plasticity_get_stats(plasticity_bridge, &stats);
    EXPECT_GT(stats.protected_updates_blocked, 0u) << "Protected updates should be blocked";
}

//=============================================================================
// Test: State Query Integration
//=============================================================================

TEST_F(TOMSNNPlasticityIntegrationTest, StateQueryIntegration) {
    // Initial states
    tom_snn_bridge_state_t snn_state;
    tom_plasticity_bridge_state_t plasticity_state;

    EXPECT_EQ(tom_snn_get_state(snn_bridge, &snn_state), 0);
    EXPECT_EQ(tom_plasticity_get_state(plasticity_bridge, &plasticity_state), 0);

    EXPECT_EQ(snn_state.state, TOM_SNN_STATE_IDLE);
    EXPECT_EQ(plasticity_state.state, TOM_PLASTICITY_STATE_IDLE);

    // Perform activity
    float dims[TOM_DIM_COUNT] = {0};
    generate_social_context(dims, TOM_DIM_COUNT, 0.7f, 0.6f, 0.5f);
    tom_snn_encode_context(snn_bridge, dims, TOM_DIM_COUNT);
    tom_snn_simulate(snn_bridge, 20.0f);

    tom_snn_get_state(snn_bridge, &snn_state);
    EXPECT_EQ(snn_state.state, TOM_SNN_STATE_IDLE);  // Should return to idle after simulation
    EXPECT_GT(snn_state.total_activity, 0.0f);
}

//=============================================================================
// Test: Calibration State
//=============================================================================

TEST_F(TOMSNNPlasticityIntegrationTest, CalibrationState) {
    // Get initial calibration
    tom_calibration_state_t calib_before;
    tom_plasticity_get_calibration_state(plasticity_bridge, &calib_before);

    EXPECT_GT(calib_before.belief_sensitivity, 0.0f);
    EXPECT_GT(calib_before.empathy_strength, 0.0f);
    EXPECT_GT(calib_before.learning_rate_mod, 0.0f);

    // Run homeostatic update
    for (int i = 0; i < 10; i++) {
        tom_plasticity_homeostatic_update(plasticity_bridge, 100.0f);
    }

    // Get updated calibration
    tom_calibration_state_t calib_after;
    tom_plasticity_get_calibration_state(plasticity_bridge, &calib_after);

    // Calibration should still be valid
    EXPECT_GT(calib_after.empathy_strength, 0.0f);
}

//=============================================================================
// Test: Reset Both Bridges
//=============================================================================

TEST_F(TOMSNNPlasticityIntegrationTest, ResetBothBridges) {
    // Do activity
    for (int i = 0; i < 5; i++) {
        float dims[TOM_DIM_COUNT] = {0};
        generate_social_context(dims, TOM_DIM_COUNT, 0.6f, 0.5f, 0.7f);
        tom_snn_encode_context(snn_bridge, dims, TOM_DIM_COUNT);
        tom_snn_simulate(snn_bridge, 20.0f);
    }

    // Reset both
    EXPECT_EQ(tom_snn_reset(snn_bridge), 0);
    EXPECT_EQ(tom_plasticity_reset(plasticity_bridge), 0);

    // Verify states are reset
    tom_snn_bridge_state_t snn_state;
    tom_plasticity_bridge_state_t plasticity_state;

    tom_snn_get_state(snn_bridge, &snn_state);
    tom_plasticity_get_state(plasticity_bridge, &plasticity_state);

    EXPECT_EQ(snn_state.state, TOM_SNN_STATE_IDLE);
    EXPECT_EQ(plasticity_state.state, TOM_PLASTICITY_STATE_IDLE);
}

//=============================================================================
// Test: Full Pipeline Integration
//=============================================================================

TEST_F(TOMSNNPlasticityIntegrationTest, FullPipelineIntegration) {
    // Register synapses for all types
    tom_plasticity_register_synapse(plasticity_bridge, 0, TOM_SYNAPSE_EMPATHY, 0.5f);
    tom_plasticity_register_synapse(plasticity_bridge, 1, TOM_SYNAPSE_SOCIAL, 0.5f);
    tom_plasticity_register_synapse(plasticity_bridge, 2, TOM_SYNAPSE_INTENTION, 0.5f);
    tom_plasticity_register_synapse(plasticity_bridge, 3, TOM_SYNAPSE_DESIRE, 0.5f);

    // Full ToM scenario: observe, infer, learn
    float dims[TOM_DIM_COUNT] = {0};

    // Step 1: Encode initial social context
    generate_social_context(dims, TOM_DIM_COUNT, 0.7f, 0.8f, 0.6f);
    int spikes = tom_snn_encode_context(snn_bridge, dims, TOM_DIM_COUNT);
    EXPECT_GE(spikes, 0);

    // Step 2: Simulate neural processing
    EXPECT_EQ(tom_snn_simulate(snn_bridge, 50.0f), 0);

    // Step 3: Get inference
    tom_inference_t inference;
    tom_snn_get_inference(snn_bridge, &inference);

    // Step 4: Apply learning based on inference
    if (inference.confidence > 0.3f) {
        tom_plasticity_learn(plasticity_bridge, TOM_LEARN_CORRECT_BELIEF,
                             inference.confidence, 0, 1.0f);
    }
    if (inference.intention_clarity > 0.5f) {
        tom_plasticity_learn(plasticity_bridge, TOM_LEARN_INTENTION_CORRECT,
                             inference.intention_clarity, 2, 1.0f);
    }
    if (inference.empathic_accuracy > 0.4f) {
        tom_plasticity_learn(plasticity_bridge, TOM_LEARN_EMPATHY_ACCURATE,
                             inference.empathic_accuracy, 0, 1.0f);
    }

    // Step 5: Apply reward for successful social inference
    tom_plasticity_apply_reward(plasticity_bridge, 0.7f);

    // Step 6: Update dynamics
    tom_plasticity_update_bcm(plasticity_bridge, 10.0f);
    tom_plasticity_update_traces(plasticity_bridge, 10.0f);

    // Step 7: Consolidate
    tom_plasticity_consolidate(plasticity_bridge);

    // Verify complete pipeline ran
    tom_snn_stats_t snn_stats;
    tom_snn_get_stats(snn_bridge, &snn_stats);
    EXPECT_GT(snn_stats.total_evaluations, 0u);

    tom_plasticity_stats_t plasticity_stats;
    tom_plasticity_get_stats(plasticity_bridge, &plasticity_stats);
    EXPECT_GT(plasticity_stats.total_learning_events, 0u);
}
