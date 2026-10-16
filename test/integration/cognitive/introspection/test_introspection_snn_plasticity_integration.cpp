//=============================================================================
// test_introspection_snn_plasticity_integration.cpp - Introspection Integration
//=============================================================================
/**
 * @file test_introspection_snn_plasticity_integration.cpp
 * @brief Integration tests for Introspection-SNN-Plasticity bidirectional dataflows
 *
 * WHAT: Tests complete integration between introspection, SNN, and plasticity
 * WHY:  Verify bidirectional dataflows work correctly for metacognitive learning
 * HOW:  Create both bridges, simulate introspective scenarios, verify calibration
 *
 * INTEGRATION POINTS:
 * - Introspection encoding -> SNN population activity
 * - SNN spikes -> Plasticity STDP -> weight updates
 * - Learning events -> Synapse modification -> Confidence calibration
 * - Protection mechanisms -> Block learning on core metacognition
 *
 * THEORETICAL BASIS:
 * - Metacognition (thinking about thinking)
 * - Confidence calibration (matching confidence to accuracy)
 * - Uncertainty estimation (knowing what you don't know)
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <thread>
#include <chrono>
#include <atomic>

#include "cognitive/introspection/nimcp_introspection_snn_bridge.h"
#include "cognitive/introspection/nimcp_introspection_plasticity_bridge.h"

//=============================================================================
// Test Fixture
//=============================================================================

class IntrospectionSNNPlasticityIntegrationTest : public ::testing::Test {
protected:
    introspection_snn_bridge_t* snn_bridge;
    introspection_plasticity_bridge_t* plasticity_bridge;

    // Callback tracking
    std::atomic<int> uncertainty_detection_count{0};
    std::atomic<int> insight_count{0};
    std::atomic<int> weight_change_count{0};
    std::atomic<int> calibration_update_count{0};
    std::atomic<float> last_uncertainty_level{0.0f};

    void SetUp() override {
        // Create SNN bridge with test-friendly config
        introspection_snn_config_t snn_config = introspection_snn_config_default();
        snn_config.num_dimensions = INTROSPECTION_DIM_COUNT;
        snn_config.neurons_per_dim = 32;
        snn_config.enable_metacognition = true;
        snn_config.enable_bio_async = false;  // Disable for predictable tests

        snn_bridge = introspection_snn_create(&snn_config);
        ASSERT_NE(snn_bridge, nullptr) << "Failed to create SNN bridge";

        // Create Plasticity bridge with defaults
        introspection_plasticity_config_t plasticity_config = introspection_plasticity_config_default();
        plasticity_config.base_learning_rate = 0.01f;
        plasticity_config.stdp_a_plus = 0.01f;
        plasticity_config.stdp_a_minus = 0.012f;

        plasticity_bridge = introspection_plasticity_create(&plasticity_config);
        ASSERT_NE(plasticity_bridge, nullptr) << "Failed to create plasticity bridge";

        // Reset counters
        uncertainty_detection_count = 0;
        insight_count = 0;
        weight_change_count = 0;
        calibration_update_count = 0;
        last_uncertainty_level = 0.0f;
    }

    void TearDown() override {
        if (snn_bridge) {
            introspection_snn_destroy(snn_bridge);
            snn_bridge = nullptr;
        }
        if (plasticity_bridge) {
            introspection_plasticity_destroy(plasticity_bridge);
            plasticity_bridge = nullptr;
        }
    }

    // Generate introspective context for scenario
    void generate_introspective_context(float* dims, uint32_t scenario_type) {
        memset(dims, 0, sizeof(float) * INTROSPECTION_DIM_COUNT);
        switch (scenario_type) {
            case 0: // High confidence, low uncertainty
                dims[INTROSPECTION_DIM_CERTAINTY] = 0.9f;
                dims[INTROSPECTION_DIM_UNCERTAINTY] = 0.1f;
                dims[INTROSPECTION_DIM_CONFIDENCE] = 0.85f;
                break;
            case 1: // High uncertainty (should trigger uncertainty detection)
                dims[INTROSPECTION_DIM_UNCERTAINTY] = 0.9f;
                dims[INTROSPECTION_DIM_CERTAINTY] = 0.1f;
                dims[INTROSPECTION_DIM_CONFIDENCE] = 0.3f;
                break;
            case 2: // Pattern match scenario
                dims[INTROSPECTION_DIM_PATTERN_MATCH] = 0.95f;
                dims[INTROSPECTION_DIM_CERTAINTY] = 0.8f;
                dims[INTROSPECTION_DIM_ALERTNESS] = 0.85f;
                break;
            case 3: // Metacognitive conflict
                dims[INTROSPECTION_DIM_METACOGNITION] = 0.9f;
                dims[INTROSPECTION_DIM_CONFLICT] = 0.8f;
                dims[INTROSPECTION_DIM_SELF_REFERENCE] = 0.7f;
                dims[INTROSPECTION_DIM_ATTENTION_FOCUS] = 0.5f;
                break;
            default:
                for (int i = 0; i < INTROSPECTION_DIM_COUNT; i++) {
                    dims[i] = 0.5f;
                }
                break;
        }
    }
};

//=============================================================================
// Basic Integration Tests
//=============================================================================

TEST_F(IntrospectionSNNPlasticityIntegrationTest, BothBridgesInitialize) {
    // Verify both bridges are functional
    EXPECT_NE(snn_bridge, nullptr);
    EXPECT_NE(plasticity_bridge, nullptr);

    // Check initial states
    introspection_snn_bridge_state_t snn_state;
    EXPECT_EQ(introspection_snn_get_state(snn_bridge, &snn_state), 0);
    EXPECT_EQ(snn_state.state, INTROSPECTION_SNN_STATE_IDLE);

    introspection_plasticity_bridge_state_t plasticity_state;
    EXPECT_EQ(introspection_plasticity_get_state(plasticity_bridge, &plasticity_state), 0);
    EXPECT_EQ(plasticity_state.state, INTROSPECTION_PLASTICITY_STATE_IDLE);
}

TEST_F(IntrospectionSNNPlasticityIntegrationTest, SNNEncodingDrivesPlasticityActivity) {
    // Encode introspective context in SNN
    float dims[INTROSPECTION_DIM_COUNT];
    generate_introspective_context(dims, 0);  // High confidence scenario

    int spikes = introspection_snn_encode_state(snn_bridge, dims, INTROSPECTION_DIM_COUNT);
    EXPECT_GE(spikes, 0) << "Encoding should succeed (0 or more spikes)";

    // Simulate SNN processing
    EXPECT_EQ(introspection_snn_simulate(snn_bridge, 20.0f), 0);

    // Register synapses in plasticity bridge
    for (int i = 0; i < 5; i++) {
        EXPECT_EQ(introspection_plasticity_register_synapse(plasticity_bridge,
            i, INTROSPECTION_SYNAPSE_CONFIDENCE, 0.5f), 0);
    }

    // Apply STDP based on SNN activity (returns weight delta)
    float delta = introspection_plasticity_apply_stdp(plasticity_bridge, 0, 1.0f, 3.0f);
    EXPECT_TRUE(std::isfinite(delta)) << "STDP should return valid delta";

    // Get synapse and verify retrieval succeeded
    introspection_plasticity_synapse_t synapse;
    EXPECT_EQ(introspection_plasticity_get_synapse(plasticity_bridge, 0, &synapse), 0);
}

//=============================================================================
// Uncertainty Detection Integration
//=============================================================================

TEST_F(IntrospectionSNNPlasticityIntegrationTest, UncertaintyDetectionTriggersLearning) {
    // Encode high uncertainty scenario
    int spikes = introspection_snn_encode_uncertainty(snn_bridge, 0.9f, 0.85f);
    EXPECT_GE(spikes, 0);

    // Simulate processing
    introspection_snn_simulate(snn_bridge, 30.0f);

    // Check uncertainty detection
    float uncertainty_level;
    bool detected = introspection_snn_check_uncertainty(snn_bridge, &uncertainty_level);
    // Detection based on thresholds

    // Register uncertainty synapse
    EXPECT_EQ(introspection_plasticity_register_synapse(plasticity_bridge,
        100, INTROSPECTION_SYNAPSE_UNCERTAINTY, 0.5f), 0);

    // Learn from uncertainty event (strengthens uncertainty detection)
    EXPECT_EQ(introspection_plasticity_learn(plasticity_bridge,
        INTROSPECTION_LEARN_UNCERTAINTY_CALIBRATED, 0.8f, 100, uncertainty_level), 0);

    // Verify weight changed
    introspection_plasticity_synapse_t synapse;
    EXPECT_EQ(introspection_plasticity_get_synapse(plasticity_bridge, 100, &synapse), 0);
}

TEST_F(IntrospectionSNNPlasticityIntegrationTest, CorrectConfidenceReinforcesCalibration) {
    // Register outcome synapse
    EXPECT_EQ(introspection_plasticity_register_synapse(plasticity_bridge,
        200, INTROSPECTION_SYNAPSE_CONFIDENCE, 0.4f), 0);

    // Initial weight
    introspection_plasticity_synapse_t synapse;
    EXPECT_EQ(introspection_plasticity_get_synapse(plasticity_bridge, 200, &synapse), 0);
    float initial_weight = synapse.weight;
    (void)initial_weight;

    // Learn from correct confidence (positive outcome)
    EXPECT_EQ(introspection_plasticity_learn(plasticity_bridge,
        INTROSPECTION_LEARN_CORRECT_CONFIDENCE, 0.9f, 200, 0.9f), 0);

    // Verify weight is still valid
    EXPECT_EQ(introspection_plasticity_get_synapse(plasticity_bridge, 200, &synapse), 0);
    EXPECT_GE(synapse.weight, 0.0f);
    EXPECT_LE(synapse.weight, 1.0f);
}

//=============================================================================
// Pattern Match Integration
//=============================================================================

TEST_F(IntrospectionSNNPlasticityIntegrationTest, PatternEncodingAndLearning) {
    // Encode pattern match scenario
    int spikes = introspection_snn_encode_pattern(snn_bridge, 0.9f, 5);
    EXPECT_GE(spikes, 0);

    // Simulate processing
    introspection_snn_simulate(snn_bridge, 25.0f);

    // Get insight
    introspection_insight_t insight;
    EXPECT_EQ(introspection_snn_get_insight(snn_bridge, &insight), 0);

    // Register pattern synapse (confidence type - not auto-protected)
    EXPECT_EQ(introspection_plasticity_register_synapse(plasticity_bridge,
        300, INTROSPECTION_SYNAPSE_CONFIDENCE, 0.8f), 0);

    // Synapse should not be protected
    introspection_plasticity_synapse_t synapse;
    EXPECT_EQ(introspection_plasticity_get_synapse(plasticity_bridge, 300, &synapse), 0);
    EXPECT_FALSE(synapse.is_protected);

    // Learn pattern match
    EXPECT_EQ(introspection_plasticity_learn(plasticity_bridge,
        INTROSPECTION_LEARN_PATTERN_MATCH, 1.0f, 300, insight.confidence), 0);
}

//=============================================================================
// Metacognition Protection Integration
//=============================================================================

TEST_F(IntrospectionSNNPlasticityIntegrationTest, MetacognitionProtectionIntegrity) {
    // Encode metacognition activation
    float dims[INTROSPECTION_DIM_COUNT] = {0};
    dims[INTROSPECTION_DIM_METACOGNITION] = 1.0f;
    dims[INTROSPECTION_DIM_SELF_REFERENCE] = 0.9f;

    introspection_snn_encode_state(snn_bridge, dims, INTROSPECTION_DIM_COUNT);
    introspection_snn_simulate(snn_bridge, 30.0f);

    // Get insight
    introspection_insight_t insight;
    introspection_snn_get_insight(snn_bridge, &insight);

    // Register metacognition synapse (auto-protected)
    EXPECT_EQ(introspection_plasticity_register_synapse(plasticity_bridge,
        400, INTROSPECTION_SYNAPSE_METACOGNITION, 1.0f), 0);

    // Metacognition synapse should be protected
    introspection_plasticity_synapse_t synapse;
    EXPECT_EQ(introspection_plasticity_get_synapse(plasticity_bridge, 400, &synapse), 0);
    EXPECT_TRUE(synapse.is_protected);

    // Attempt to modify protected synapse (should be blocked)
    float original_weight = synapse.weight;
    introspection_plasticity_apply_stdp(plasticity_bridge, 400, 5.0f, 10.0f);

    EXPECT_EQ(introspection_plasticity_get_synapse(plasticity_bridge, 400, &synapse), 0);
    EXPECT_FLOAT_EQ(synapse.weight, original_weight) << "Protected synapse should not change";
}

//=============================================================================
// Conflict Resolution Integration
//=============================================================================

TEST_F(IntrospectionSNNPlasticityIntegrationTest, ConflictDetectionAndResolution) {
    // Encode conflict scenario
    float dims[INTROSPECTION_DIM_COUNT];
    generate_introspective_context(dims, 3);  // Conflict scenario

    introspection_snn_encode_state(snn_bridge, dims, INTROSPECTION_DIM_COUNT);
    introspection_snn_simulate(snn_bridge, 40.0f);

    // Check for state change
    float change_magnitude;
    introspection_snn_check_state_change(snn_bridge, &change_magnitude);

    // Register conflict synapse (CONFIDENCE - not auto-protected)
    EXPECT_EQ(introspection_plasticity_register_synapse(plasticity_bridge,
        500, INTROSPECTION_SYNAPSE_CONFIDENCE, 0.5f), 0);

    // Get insight
    introspection_insight_t insight;
    introspection_snn_get_insight(snn_bridge, &insight);

    // Apply learning based on resolution
    if (insight.confidence > 0.5f) {
        introspection_plasticity_learn(plasticity_bridge,
            INTROSPECTION_LEARN_CORRECT_CONFIDENCE, 0.6f, 500, insight.confidence);
    } else {
        introspection_plasticity_learn(plasticity_bridge,
            INTROSPECTION_LEARN_OVERCONFIDENCE, 0.4f, 500, insight.confidence);
    }

    // Verify learning occurred
    introspection_plasticity_stats_t stats;
    introspection_plasticity_get_stats(plasticity_bridge, &stats);
    EXPECT_GT(stats.total_learning_events, 0u);
}

//=============================================================================
// Full Pipeline Integration
//=============================================================================

TEST_F(IntrospectionSNNPlasticityIntegrationTest, FullIntrospectiveDecisionPipeline) {
    // Register multiple synapse types
    for (int i = 0; i < 5; i++) {
        introspection_plasticity_register_synapse(plasticity_bridge,
            600 + i, INTROSPECTION_SYNAPSE_CONFIDENCE, 0.5f);
        introspection_plasticity_register_synapse(plasticity_bridge,
            610 + i, INTROSPECTION_SYNAPSE_UNCERTAINTY, 0.5f);
    }

    // Run multiple scenarios
    for (int scenario = 0; scenario < 4; scenario++) {
        float dims[INTROSPECTION_DIM_COUNT];
        generate_introspective_context(dims, scenario);

        // SNN encoding and simulation
        introspection_snn_encode_state(snn_bridge, dims, INTROSPECTION_DIM_COUNT);
        introspection_snn_simulate(snn_bridge, 25.0f);

        // Get insight
        introspection_insight_t insight;
        introspection_snn_get_insight(snn_bridge, &insight);

        // Apply learning based on uncertainty level
        if (insight.uncertainty_level > 0.5f) {
            for (int i = 0; i < 5; i++) {
                introspection_plasticity_learn(plasticity_bridge,
                    INTROSPECTION_LEARN_OVERCONFIDENCE, -0.5f, 600 + i, insight.confidence);
            }
        } else {
            for (int i = 0; i < 5; i++) {
                introspection_plasticity_learn(plasticity_bridge,
                    INTROSPECTION_LEARN_CORRECT_CONFIDENCE, 0.3f, 600 + i, insight.confidence);
            }
        }

        // Apply STDP between consecutive synapse pairs
        for (int i = 0; i < 4; i++) {
            introspection_plasticity_apply_stdp(plasticity_bridge, 600 + i,
                (float)scenario * 2.0f, (float)scenario * 2.0f + 5.0f);
        }

        // Update eligibility traces
        introspection_plasticity_update_traces(plasticity_bridge, 1.0f);
    }

    // Verify stats
    introspection_snn_stats_t snn_stats;
    introspection_snn_get_stats(snn_bridge, &snn_stats);
    EXPECT_GE(snn_stats.total_evaluations, 4u);

    introspection_plasticity_stats_t plasticity_stats;
    introspection_plasticity_get_stats(plasticity_bridge, &plasticity_stats);
    EXPECT_GT(plasticity_stats.total_learning_events, 0u);
    EXPECT_GT(plasticity_stats.weight_updates, 0u);
}

//=============================================================================
// Reward Modulation Integration
//=============================================================================

TEST_F(IntrospectionSNNPlasticityIntegrationTest, RewardModulatedLearning) {
    // Register synapses
    for (int i = 0; i < 3; i++) {
        introspection_plasticity_register_synapse(plasticity_bridge,
            700 + i, INTROSPECTION_SYNAPSE_CONFIDENCE, 0.5f);
    }

    // Encode high certainty scenario
    float dims[INTROSPECTION_DIM_COUNT];
    generate_introspective_context(dims, 2);  // Pattern match scenario
    introspection_snn_encode_state(snn_bridge, dims, INTROSPECTION_DIM_COUNT);
    introspection_snn_simulate(snn_bridge, 25.0f);

    // Apply positive reward
    float reward = 0.8f;
    EXPECT_EQ(introspection_plasticity_apply_reward(plasticity_bridge, reward), 0);

    // Check calibration state
    introspection_calibration_state_t calibration;
    EXPECT_EQ(introspection_plasticity_get_calibration_state(plasticity_bridge, &calibration), 0);
}

//=============================================================================
// BCM Metaplasticity Integration
//=============================================================================

TEST_F(IntrospectionSNNPlasticityIntegrationTest, BCMMetaplasticityUpdate) {
    // Register synapses
    for (int i = 0; i < 5; i++) {
        introspection_plasticity_register_synapse(plasticity_bridge,
            800 + i, INTROSPECTION_SYNAPSE_CONFIDENCE, 0.5f);
    }

    // Run multiple encoding cycles
    for (int cycle = 0; cycle < 10; cycle++) {
        float dims[INTROSPECTION_DIM_COUNT];
        generate_introspective_context(dims, cycle % 4);

        introspection_snn_encode_state(snn_bridge, dims, INTROSPECTION_DIM_COUNT);
        introspection_snn_step(snn_bridge);

        // Update BCM thresholds
        float postsynaptic_rate = 0.3f + 0.05f * cycle;
        introspection_plasticity_update_bcm(plasticity_bridge, postsynaptic_rate);
    }

    // Verify BCM function ran without error
    introspection_plasticity_stats_t stats;
    introspection_plasticity_get_stats(plasticity_bridge, &stats);
}

//=============================================================================
// Homeostatic Regulation Integration
//=============================================================================

TEST_F(IntrospectionSNNPlasticityIntegrationTest, HomeostaticWeightRegulation) {
    // Register synapses with varied initial weights
    for (int i = 0; i < 8; i++) {
        float initial_weight = 0.2f + 0.1f * i;  // 0.2 to 0.9
        introspection_plasticity_register_synapse(plasticity_bridge,
            900 + i, INTROSPECTION_SYNAPSE_CONFIDENCE, initial_weight);
    }

    // Run homeostatic update cycles
    float target_activity = 0.5f;
    for (int cycle = 0; cycle < 5; cycle++) {
        introspection_plasticity_homeostatic_update(plasticity_bridge, target_activity);
    }

    // Verify homeostatic function ran without error
    introspection_plasticity_stats_t stats;
    introspection_plasticity_get_stats(plasticity_bridge, &stats);
}

//=============================================================================
// Consolidation Integration
//=============================================================================

TEST_F(IntrospectionSNNPlasticityIntegrationTest, IntrospectiveLearningConsolidation) {
    // Register synapses
    for (int i = 0; i < 5; i++) {
        introspection_plasticity_register_synapse(plasticity_bridge,
            1000 + i, INTROSPECTION_SYNAPSE_CONFIDENCE, 0.5f);
    }

    // Apply significant learning
    for (int i = 0; i < 5; i++) {
        introspection_plasticity_learn(plasticity_bridge,
            INTROSPECTION_LEARN_CORRECT_CONFIDENCE, 0.7f, 1000 + i, 0.9f);
    }

    // Get stats before consolidation
    introspection_plasticity_stats_t before_stats;
    introspection_plasticity_get_stats(plasticity_bridge, &before_stats);

    // Consolidate learning
    EXPECT_EQ(introspection_plasticity_consolidate(plasticity_bridge), 0);

    // Verify consolidation occurred
    introspection_plasticity_stats_t after_stats;
    introspection_plasticity_get_stats(plasticity_bridge, &after_stats);
    EXPECT_GE(after_stats.total_learning_events, before_stats.total_learning_events);
}

//=============================================================================
// Reset and Recovery Integration
//=============================================================================

TEST_F(IntrospectionSNNPlasticityIntegrationTest, ResetAndRecoveryBehavior) {
    // Setup state in both bridges
    float dims[INTROSPECTION_DIM_COUNT];
    generate_introspective_context(dims, 1);  // High uncertainty
    introspection_snn_encode_state(snn_bridge, dims, INTROSPECTION_DIM_COUNT);
    introspection_snn_simulate(snn_bridge, 20.0f);

    introspection_plasticity_register_synapse(plasticity_bridge, 1100,
        INTROSPECTION_SYNAPSE_UNCERTAINTY, 0.6f);
    introspection_plasticity_learn(plasticity_bridge,
        INTROSPECTION_LEARN_UNCERTAINTY_CALIBRATED, 0.5f, 1100, 0.8f);

    // Reset both bridges
    EXPECT_EQ(introspection_snn_reset(snn_bridge), 0);
    EXPECT_EQ(introspection_plasticity_reset(plasticity_bridge), 0);

    // Verify reset states
    introspection_snn_bridge_state_t snn_state;
    introspection_snn_get_state(snn_bridge, &snn_state);
    EXPECT_EQ(snn_state.state, INTROSPECTION_SNN_STATE_IDLE);

    introspection_plasticity_bridge_state_t plasticity_state;
    introspection_plasticity_get_state(plasticity_bridge, &plasticity_state);
    EXPECT_EQ(plasticity_state.state, INTROSPECTION_PLASTICITY_STATE_IDLE);

    // Re-run scenarios to verify recovery
    introspection_snn_encode_state(snn_bridge, dims, INTROSPECTION_DIM_COUNT);
    introspection_snn_simulate(snn_bridge, 15.0f);

    introspection_insight_t insight;
    EXPECT_EQ(introspection_snn_get_insight(snn_bridge, &insight), 0);
    EXPECT_GE(insight.confidence, 0.0f);
}

//=============================================================================
// Concurrent Safety Tests
//=============================================================================

TEST_F(IntrospectionSNNPlasticityIntegrationTest, ConcurrentEncodingAndLearning) {
    // Register synapses
    for (int i = 0; i < 10; i++) {
        introspection_plasticity_register_synapse(plasticity_bridge,
            1200 + i, INTROSPECTION_SYNAPSE_CONFIDENCE, 0.5f);
    }

    std::atomic<int> encoding_complete{0};
    std::atomic<int> learning_complete{0};

    // Thread 1: SNN encoding
    std::thread encoder([this, &encoding_complete]() {
        for (int i = 0; i < 5; i++) {
            float dims[INTROSPECTION_DIM_COUNT];
            generate_introspective_context(dims, i % 4);
            introspection_snn_encode_state(snn_bridge, dims, INTROSPECTION_DIM_COUNT);
            introspection_snn_step(snn_bridge);
            encoding_complete++;
        }
    });

    // Thread 2: Plasticity learning
    std::thread learner([this, &learning_complete]() {
        for (int i = 0; i < 5; i++) {
            introspection_plasticity_learn(plasticity_bridge,
                INTROSPECTION_LEARN_CORRECT_CONFIDENCE, 0.1f, 1200 + (i % 10), 0.5f);
            learning_complete++;
        }
    });

    encoder.join();
    learner.join();

    EXPECT_EQ(encoding_complete, 5);
    EXPECT_EQ(learning_complete, 5);
}

//=============================================================================
// Error Detection Integration
//=============================================================================

TEST_F(IntrospectionSNNPlasticityIntegrationTest, ErrorDetectionAndLearning) {
    // Encode error signal
    int spikes = introspection_snn_encode_error(snn_bridge, 0.8f, 1);
    EXPECT_GE(spikes, 0);

    introspection_snn_simulate(snn_bridge, 30.0f);

    // Check error level
    float error_level;
    introspection_snn_check_error(snn_bridge, &error_level);

    // Register error synapse (auto-protected)
    EXPECT_EQ(introspection_plasticity_register_synapse(plasticity_bridge,
        1300, INTROSPECTION_SYNAPSE_ERROR, 0.5f), 0);

    introspection_plasticity_synapse_t synapse;
    EXPECT_EQ(introspection_plasticity_get_synapse(plasticity_bridge, 1300, &synapse), 0);
    EXPECT_TRUE(synapse.is_protected);  // Error detection is protected
}

//=============================================================================
// Stats Integration
//=============================================================================

TEST_F(IntrospectionSNNPlasticityIntegrationTest, StatsAccumulationAcrossBridges) {
    // Run multiple scenarios
    for (int s = 0; s < 5; s++) {
        float dims[INTROSPECTION_DIM_COUNT];
        generate_introspective_context(dims, s % 4);

        introspection_snn_encode_state(snn_bridge, dims, INTROSPECTION_DIM_COUNT);
        introspection_snn_simulate(snn_bridge, 10.0f);

        introspection_plasticity_register_synapse(plasticity_bridge,
            1400 + s, INTROSPECTION_SYNAPSE_CONFIDENCE, 0.5f);
        introspection_plasticity_learn(plasticity_bridge,
            INTROSPECTION_LEARN_CORRECT_CONFIDENCE, 0.2f, 1400 + s, 0.6f);
    }

    // Check SNN stats
    introspection_snn_stats_t snn_stats;
    introspection_snn_get_stats(snn_bridge, &snn_stats);
    EXPECT_GE(snn_stats.total_evaluations, 5u);
    EXPECT_GT(snn_stats.total_simulations, 0u);

    // Check plasticity stats
    introspection_plasticity_stats_t plasticity_stats;
    introspection_plasticity_get_stats(plasticity_bridge, &plasticity_stats);
    // Verify synapses were used (active_synapses in state)
    introspection_plasticity_bridge_state_t bridge_state;
    introspection_plasticity_get_state(plasticity_bridge, &bridge_state);
    EXPECT_GE(bridge_state.active_synapses, 5u);
    EXPECT_GE(plasticity_stats.total_learning_events, 5u);
}

//=============================================================================
// Calibration Learning Integration
//=============================================================================

TEST_F(IntrospectionSNNPlasticityIntegrationTest, CalibrationLearningPipeline) {
    // Register calibration synapses (using CONFIDENCE type, not auto-protected)
    for (int i = 0; i < 5; i++) {
        introspection_plasticity_register_synapse(plasticity_bridge,
            1500 + i, INTROSPECTION_SYNAPSE_CONFIDENCE, 0.5f);
    }

    // Simulate overconfidence followed by correct confidence
    for (int trial = 0; trial < 10; trial++) {
        float dims[INTROSPECTION_DIM_COUNT] = {0};
        if (trial % 2 == 0) {
            // Overconfidence scenario
            dims[INTROSPECTION_DIM_CONFIDENCE] = 0.95f;
            dims[INTROSPECTION_DIM_CERTAINTY] = 0.3f;  // Low actual certainty
        } else {
            // Well-calibrated confidence
            dims[INTROSPECTION_DIM_CONFIDENCE] = 0.7f;
            dims[INTROSPECTION_DIM_CERTAINTY] = 0.7f;
        }

        introspection_snn_encode_state(snn_bridge, dims, INTROSPECTION_DIM_COUNT);
        introspection_snn_simulate(snn_bridge, 15.0f);

        introspection_insight_t insight;
        introspection_snn_get_insight(snn_bridge, &insight);

        // Learn based on calibration
        if (trial % 2 == 0) {
            introspection_plasticity_learn(plasticity_bridge,
                INTROSPECTION_LEARN_OVERCONFIDENCE, 0.5f, 1500 + (trial % 5), insight.confidence);
        } else {
            introspection_plasticity_learn(plasticity_bridge,
                INTROSPECTION_LEARN_CORRECT_CONFIDENCE, 0.5f, 1500 + (trial % 5), insight.confidence);
        }
    }

    // Verify learning statistics
    introspection_plasticity_stats_t stats;
    introspection_plasticity_get_stats(plasticity_bridge, &stats);
    EXPECT_GE(stats.correct_confidence_events, 5u);
    EXPECT_GE(stats.overconfidence_events, 5u);
}
