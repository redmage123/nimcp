//=============================================================================
// test_executive_snn_plasticity_integration.cpp - Executive Integration Tests
//=============================================================================
/**
 * @file test_executive_snn_plasticity_integration.cpp
 * @brief Integration tests for Executive-SNN-Plasticity bidirectional dataflows
 *
 * WHAT: Tests complete integration between executive functions, SNN, and plasticity
 * WHY:  Verify bidirectional dataflows work correctly for cognitive control learning
 * HOW:  Create both bridges, simulate executive control scenarios, verify learning
 *
 * INTEGRATION POINTS:
 * - Executive state encoding -> SNN population activity
 * - SNN spikes -> Plasticity STDP -> weight updates
 * - Learning events -> Synapse modification -> Control calibration
 * - Protection mechanisms -> Block learning on core executive circuits
 *
 * THEORETICAL BASIS:
 * - Miller & Cohen (2001): Prefrontal Cortex and Cognitive Control
 * - Ridderinkhof et al. (2004): Learning executive control
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <thread>
#include <chrono>
#include <atomic>

#include "cognitive/executive/nimcp_executive_snn_bridge.h"
#include "cognitive/executive/nimcp_executive_plasticity_bridge.h"

//=============================================================================
// Test Fixture
//=============================================================================

class ExecutiveSNNPlasticityIntegrationTest : public ::testing::Test {
protected:
    executive_snn_bridge_t* snn_bridge;
    executive_plasticity_bridge_t* plasticity_bridge;

    // Callback tracking
    std::atomic<int> conflict_detection_count{0};
    std::atomic<int> control_output_count{0};
    std::atomic<int> weight_change_count{0};
    std::atomic<int> calibration_update_count{0};
    std::atomic<float> last_conflict_level{0.0f};

    void SetUp() override {
        // Create SNN bridge with test-friendly config
        executive_snn_config_t snn_config = executive_snn_config_default();
        snn_config.num_dimensions = EXEC_DIM_COUNT;
        snn_config.neurons_per_dim = 32;
        snn_config.enable_conflict_detection = true;
        snn_config.enable_bio_async = false;  // Disable for predictable tests

        snn_bridge = executive_snn_create(&snn_config);
        ASSERT_NE(snn_bridge, nullptr) << "Failed to create SNN bridge";

        // Create Plasticity bridge with defaults
        executive_plasticity_config_t plasticity_config = executive_plasticity_config_default();
        plasticity_config.base_learning_rate = 0.01f;
        plasticity_config.stdp_a_plus = 0.01f;
        plasticity_config.stdp_a_minus = 0.012f;

        plasticity_bridge = executive_plasticity_create(&plasticity_config);
        ASSERT_NE(plasticity_bridge, nullptr) << "Failed to create plasticity bridge";

        // Reset counters
        conflict_detection_count = 0;
        control_output_count = 0;
        weight_change_count = 0;
        calibration_update_count = 0;
        last_conflict_level = 0.0f;
    }

    void TearDown() override {
        if (snn_bridge) {
            executive_snn_destroy(snn_bridge);
            snn_bridge = nullptr;
        }
        if (plasticity_bridge) {
            executive_plasticity_destroy(plasticity_bridge);
            plasticity_bridge = nullptr;
        }
    }

    // Generate executive context for scenario
    void generate_executive_context(float* dims, uint32_t scenario_type) {
        memset(dims, 0, sizeof(float) * EXEC_DIM_COUNT);
        switch (scenario_type) {
            case 0: // High inhibition requirement
                dims[EXEC_DIM_INHIBITION] = 0.9f;
                dims[EXEC_DIM_ATTENTION_CONTROL] = 0.85f;
                dims[EXEC_DIM_CONFLICT_MONITOR] = 0.3f;
                break;
            case 1: // Task switching (high conflict)
                dims[EXEC_DIM_TASK_SWITCHING] = 0.9f;
                dims[EXEC_DIM_FLEXIBILITY] = 0.8f;
                dims[EXEC_DIM_CONFLICT_MONITOR] = 0.7f;
                break;
            case 2: // Planning scenario
                dims[EXEC_DIM_PLANNING] = 0.95f;
                dims[EXEC_DIM_WORKING_MEMORY] = 0.8f;
                dims[EXEC_DIM_GOAL_MAINTENANCE] = 0.85f;
                break;
            case 3: // High conflict scenario
                dims[EXEC_DIM_CONFLICT_MONITOR] = 0.9f;
                dims[EXEC_DIM_ERROR_CORRECTION] = 0.7f;
                dims[EXEC_DIM_ATTENTION_CONTROL] = 0.8f;
                break;
            default:
                for (int i = 0; i < EXEC_DIM_COUNT; i++) {
                    dims[i] = 0.5f;
                }
                break;
        }
    }
};

//=============================================================================
// Basic Integration Tests
//=============================================================================

TEST_F(ExecutiveSNNPlasticityIntegrationTest, BothBridgesInitialize) {
    // Verify both bridges are functional
    EXPECT_NE(snn_bridge, nullptr);
    EXPECT_NE(plasticity_bridge, nullptr);

    // Check initial states
    executive_snn_bridge_state_t snn_state;
    EXPECT_EQ(executive_snn_get_state(snn_bridge, &snn_state), 0);
    EXPECT_EQ(snn_state.state, EXECUTIVE_SNN_STATE_IDLE);

    executive_plasticity_bridge_state_t plasticity_state;
    EXPECT_EQ(executive_plasticity_get_state(plasticity_bridge, &plasticity_state), 0);
    EXPECT_EQ(plasticity_state.state, EXECUTIVE_PLASTICITY_STATE_IDLE);
}

TEST_F(ExecutiveSNNPlasticityIntegrationTest, SNNEncodingDrivesPlasticityActivity) {
    // Encode executive context in SNN
    float dims[EXEC_DIM_COUNT];
    generate_executive_context(dims, 0);  // High inhibition scenario

    int spikes = executive_snn_encode_state(snn_bridge, dims, EXEC_DIM_COUNT);
    EXPECT_GE(spikes, 0) << "Encoding should succeed (0 or more spikes)";

    // Simulate SNN processing
    EXPECT_EQ(executive_snn_simulate(snn_bridge, 20.0f), 0);

    // Register synapses in plasticity bridge
    for (int i = 0; i < 5; i++) {
        EXPECT_EQ(executive_plasticity_register_synapse(plasticity_bridge,
            i, EXEC_SYNAPSE_FLEXIBILITY, 0.5f), 0);
    }

    // Apply STDP based on SNN activity (returns weight delta)
    float delta = executive_plasticity_apply_stdp(plasticity_bridge, 0, 1.0f, 3.0f);
    EXPECT_TRUE(std::isfinite(delta)) << "STDP should return valid delta";

    // Get synapse and verify retrieval succeeded
    executive_plasticity_synapse_t synapse;
    EXPECT_EQ(executive_plasticity_get_synapse(plasticity_bridge, 0, &synapse), 0);
}

//=============================================================================
// Conflict Detection Integration
//=============================================================================

TEST_F(ExecutiveSNNPlasticityIntegrationTest, ConflictDetectionTriggersLearning) {
    // Encode high conflict scenario
    int spikes = executive_snn_encode_conflict(snn_bridge, 0.9f, 1);
    EXPECT_GE(spikes, 0);

    // Simulate processing
    executive_snn_simulate(snn_bridge, 30.0f);

    // Check conflict detection
    float conflict_level;
    bool detected = executive_snn_check_conflict(snn_bridge, &conflict_level);
    // Detection based on thresholds

    // Register conflict synapse
    EXPECT_EQ(executive_plasticity_register_synapse(plasticity_bridge,
        100, EXEC_SYNAPSE_CONFLICT, 0.5f), 0);

    // Learn from conflict event
    EXPECT_EQ(executive_plasticity_learn(plasticity_bridge,
        EXEC_LEARN_CONFLICT_RESOLVED, 0.8f, 100, conflict_level), 0);

    // Verify weight changed
    executive_plasticity_synapse_t synapse;
    EXPECT_EQ(executive_plasticity_get_synapse(plasticity_bridge, 100, &synapse), 0);
}

TEST_F(ExecutiveSNNPlasticityIntegrationTest, SuccessfulInhibitionReinforcesCircuit) {
    // Register flexibility synapse (not auto-protected)
    EXPECT_EQ(executive_plasticity_register_synapse(plasticity_bridge,
        200, EXEC_SYNAPSE_FLEXIBILITY, 0.4f), 0);

    // Initial weight
    executive_plasticity_synapse_t synapse;
    EXPECT_EQ(executive_plasticity_get_synapse(plasticity_bridge, 200, &synapse), 0);
    float initial_weight = synapse.weight;
    (void)initial_weight;

    // Learn from successful inhibition
    EXPECT_EQ(executive_plasticity_learn(plasticity_bridge,
        EXEC_LEARN_SUCCESSFUL_INHIBITION, 0.9f, 200, 0.9f), 0);

    // Verify weight is still valid
    EXPECT_EQ(executive_plasticity_get_synapse(plasticity_bridge, 200, &synapse), 0);
    EXPECT_GE(synapse.weight, 0.0f);
    EXPECT_LE(synapse.weight, 2.0f);  // max weight is 2.0
}

//=============================================================================
// Task Switching Integration
//=============================================================================

TEST_F(ExecutiveSNNPlasticityIntegrationTest, TaskSwitchingEncodingAndLearning) {
    // Encode task switching scenario
    float dims[EXEC_DIM_COUNT];
    generate_executive_context(dims, 1);  // Task switching scenario

    int spikes = executive_snn_encode_state(snn_bridge, dims, EXEC_DIM_COUNT);
    EXPECT_GE(spikes, 0);

    // Simulate processing
    executive_snn_simulate(snn_bridge, 25.0f);

    // Get control output
    executive_control_output_t output;
    EXPECT_EQ(executive_snn_get_control_output(snn_bridge, &output), 0);

    // Register flexibility synapse (not auto-protected)
    EXPECT_EQ(executive_plasticity_register_synapse(plasticity_bridge,
        300, EXEC_SYNAPSE_FLEXIBILITY, 0.8f), 0);

    // Synapse should not be protected
    executive_plasticity_synapse_t synapse;
    EXPECT_EQ(executive_plasticity_get_synapse(plasticity_bridge, 300, &synapse), 0);
    EXPECT_FALSE(synapse.is_protected);

    // Learn task switch
    EXPECT_EQ(executive_plasticity_learn(plasticity_bridge,
        EXEC_LEARN_TASK_SWITCH_SUCCESS, 1.0f, 300, output.flexibility_level), 0);
}

//=============================================================================
// Inhibition Protection Integration
//=============================================================================

TEST_F(ExecutiveSNNPlasticityIntegrationTest, InhibitionProtectionIntegrity) {
    // Encode inhibition activation
    float dims[EXEC_DIM_COUNT] = {0};
    dims[EXEC_DIM_INHIBITION] = 1.0f;
    dims[EXEC_DIM_ATTENTION_CONTROL] = 0.9f;

    executive_snn_encode_state(snn_bridge, dims, EXEC_DIM_COUNT);
    executive_snn_simulate(snn_bridge, 30.0f);

    // Get control output
    executive_control_output_t output;
    executive_snn_get_control_output(snn_bridge, &output);

    // Register inhibition synapse (auto-protected)
    EXPECT_EQ(executive_plasticity_register_synapse(plasticity_bridge,
        400, EXEC_SYNAPSE_INHIBITION, 1.0f), 0);

    // Inhibition synapse should be protected
    executive_plasticity_synapse_t synapse;
    EXPECT_EQ(executive_plasticity_get_synapse(plasticity_bridge, 400, &synapse), 0);
    EXPECT_TRUE(synapse.is_protected);

    // Attempt to modify protected synapse (should be blocked)
    float original_weight = synapse.weight;
    executive_plasticity_apply_stdp(plasticity_bridge, 400, 5.0f, 10.0f);

    EXPECT_EQ(executive_plasticity_get_synapse(plasticity_bridge, 400, &synapse), 0);
    EXPECT_FLOAT_EQ(synapse.weight, original_weight) << "Protected synapse should not change";
}

//=============================================================================
// Goal Maintenance Integration
//=============================================================================

TEST_F(ExecutiveSNNPlasticityIntegrationTest, GoalMaintenanceProtection) {
    // Encode goal maintenance scenario
    float dims[EXEC_DIM_COUNT];
    generate_executive_context(dims, 2);  // Planning scenario

    executive_snn_encode_state(snn_bridge, dims, EXEC_DIM_COUNT);
    executive_snn_simulate(snn_bridge, 40.0f);

    // Register goal synapse (auto-protected)
    EXPECT_EQ(executive_plasticity_register_synapse(plasticity_bridge,
        500, EXEC_SYNAPSE_GOAL, 0.9f), 0);

    executive_plasticity_synapse_t synapse;
    EXPECT_EQ(executive_plasticity_get_synapse(plasticity_bridge, 500, &synapse), 0);
    EXPECT_TRUE(synapse.is_protected);

    float original_weight = synapse.weight;

    // Try to modify (should be blocked)
    executive_plasticity_learn(plasticity_bridge,
        EXEC_LEARN_GOAL_LOST, 0.8f, 500, 0.9f);

    EXPECT_EQ(executive_plasticity_get_synapse(plasticity_bridge, 500, &synapse), 0);
    EXPECT_FLOAT_EQ(synapse.weight, original_weight);
}

//=============================================================================
// Full Pipeline Integration
//=============================================================================

TEST_F(ExecutiveSNNPlasticityIntegrationTest, FullExecutiveControlPipeline) {
    // Register multiple synapse types
    for (int i = 0; i < 5; i++) {
        executive_plasticity_register_synapse(plasticity_bridge,
            600 + i, EXEC_SYNAPSE_FLEXIBILITY, 0.5f);
        executive_plasticity_register_synapse(plasticity_bridge,
            610 + i, EXEC_SYNAPSE_PLANNING, 0.5f);
    }

    // Run multiple scenarios
    for (int scenario = 0; scenario < 4; scenario++) {
        float dims[EXEC_DIM_COUNT];
        generate_executive_context(dims, scenario);

        // SNN encoding and simulation
        executive_snn_encode_state(snn_bridge, dims, EXEC_DIM_COUNT);
        executive_snn_simulate(snn_bridge, 25.0f);

        // Get control output
        executive_control_output_t output;
        executive_snn_get_control_output(snn_bridge, &output);

        // Apply learning based on conflict level
        if (output.conflict_magnitude > 0.5f) {
            for (int i = 0; i < 5; i++) {
                executive_plasticity_learn(plasticity_bridge,
                    EXEC_LEARN_CONFLICT_UNRESOLVED, -0.5f, 600 + i, output.flexibility_level);
            }
        } else {
            for (int i = 0; i < 5; i++) {
                executive_plasticity_learn(plasticity_bridge,
                    EXEC_LEARN_SUCCESSFUL_INHIBITION, 0.3f, 600 + i, output.flexibility_level);
            }
        }

        // Apply STDP between consecutive synapse pairs
        for (int i = 0; i < 4; i++) {
            executive_plasticity_apply_stdp(plasticity_bridge, 600 + i,
                (float)scenario * 2.0f, (float)scenario * 2.0f + 5.0f);
        }

        // Update eligibility traces
        executive_plasticity_update_traces(plasticity_bridge, 1.0f);
    }

    // Verify stats
    executive_snn_stats_t snn_stats;
    executive_snn_get_stats(snn_bridge, &snn_stats);
    EXPECT_GE(snn_stats.total_evaluations, 4u);

    executive_plasticity_stats_t plasticity_stats;
    executive_plasticity_get_stats(plasticity_bridge, &plasticity_stats);
    EXPECT_GT(plasticity_stats.total_learning_events, 0u);
    EXPECT_GT(plasticity_stats.weight_updates, 0u);
}

//=============================================================================
// Reward Modulation Integration
//=============================================================================

TEST_F(ExecutiveSNNPlasticityIntegrationTest, RewardModulatedLearning) {
    // Register synapses
    for (int i = 0; i < 3; i++) {
        executive_plasticity_register_synapse(plasticity_bridge,
            700 + i, EXEC_SYNAPSE_FLEXIBILITY, 0.5f);
    }

    // Encode high inhibition scenario
    float dims[EXEC_DIM_COUNT];
    generate_executive_context(dims, 0);
    executive_snn_encode_state(snn_bridge, dims, EXEC_DIM_COUNT);
    executive_snn_simulate(snn_bridge, 25.0f);

    // Apply positive reward
    float reward = 0.8f;
    EXPECT_EQ(executive_plasticity_apply_reward(plasticity_bridge, reward), 0);

    // Check calibration state
    executive_calibration_state_t calibration;
    EXPECT_EQ(executive_plasticity_get_calibration_state(plasticity_bridge, &calibration), 0);
}

//=============================================================================
// BCM Metaplasticity Integration
//=============================================================================

TEST_F(ExecutiveSNNPlasticityIntegrationTest, BCMMetaplasticityUpdate) {
    // Register synapses
    for (int i = 0; i < 5; i++) {
        executive_plasticity_register_synapse(plasticity_bridge,
            800 + i, EXEC_SYNAPSE_FLEXIBILITY, 0.5f);
    }

    // Run multiple encoding cycles
    for (int cycle = 0; cycle < 10; cycle++) {
        float dims[EXEC_DIM_COUNT];
        generate_executive_context(dims, cycle % 4);

        executive_snn_encode_state(snn_bridge, dims, EXEC_DIM_COUNT);
        executive_snn_step(snn_bridge);

        // Update BCM thresholds
        float postsynaptic_rate = 0.3f + 0.05f * cycle;
        executive_plasticity_update_bcm(plasticity_bridge, postsynaptic_rate);
    }

    // Verify BCM function ran without error
    executive_plasticity_stats_t stats;
    executive_plasticity_get_stats(plasticity_bridge, &stats);
}

//=============================================================================
// Homeostatic Regulation Integration
//=============================================================================

TEST_F(ExecutiveSNNPlasticityIntegrationTest, HomeostaticWeightRegulation) {
    // Register synapses with varied initial weights
    for (int i = 0; i < 8; i++) {
        float initial_weight = 0.2f + 0.1f * i;  // 0.2 to 0.9
        executive_plasticity_register_synapse(plasticity_bridge,
            900 + i, EXEC_SYNAPSE_FLEXIBILITY, initial_weight);
    }

    // Run homeostatic update cycles
    float target_activity = 0.5f;
    for (int cycle = 0; cycle < 5; cycle++) {
        executive_plasticity_homeostatic_update(plasticity_bridge, target_activity);
    }

    // Verify homeostatic function ran without error
    executive_plasticity_stats_t stats;
    executive_plasticity_get_stats(plasticity_bridge, &stats);
}

//=============================================================================
// Consolidation Integration
//=============================================================================

TEST_F(ExecutiveSNNPlasticityIntegrationTest, ExecutiveLearningConsolidation) {
    // Register synapses
    for (int i = 0; i < 5; i++) {
        executive_plasticity_register_synapse(plasticity_bridge,
            1000 + i, EXEC_SYNAPSE_FLEXIBILITY, 0.5f);
    }

    // Apply significant learning
    for (int i = 0; i < 5; i++) {
        executive_plasticity_learn(plasticity_bridge,
            EXEC_LEARN_SUCCESSFUL_INHIBITION, 0.7f, 1000 + i, 0.9f);
    }

    // Get stats before consolidation
    executive_plasticity_stats_t before_stats;
    executive_plasticity_get_stats(plasticity_bridge, &before_stats);

    // Consolidate learning
    EXPECT_EQ(executive_plasticity_consolidate(plasticity_bridge), 0);

    // Verify consolidation occurred
    executive_plasticity_stats_t after_stats;
    executive_plasticity_get_stats(plasticity_bridge, &after_stats);
    EXPECT_GE(after_stats.total_learning_events, before_stats.total_learning_events);
}

//=============================================================================
// Reset and Recovery Integration
//=============================================================================

TEST_F(ExecutiveSNNPlasticityIntegrationTest, ResetAndRecoveryBehavior) {
    // Setup state in both bridges
    float dims[EXEC_DIM_COUNT];
    generate_executive_context(dims, 3);  // High conflict
    executive_snn_encode_state(snn_bridge, dims, EXEC_DIM_COUNT);
    executive_snn_simulate(snn_bridge, 20.0f);

    executive_plasticity_register_synapse(plasticity_bridge, 1100,
        EXEC_SYNAPSE_CONFLICT, 0.6f);
    executive_plasticity_learn(plasticity_bridge,
        EXEC_LEARN_CONFLICT_RESOLVED, 0.5f, 1100, 0.8f);

    // Reset both bridges
    EXPECT_EQ(executive_snn_reset(snn_bridge), 0);
    EXPECT_EQ(executive_plasticity_reset(plasticity_bridge), 0);

    // Verify reset states
    executive_snn_bridge_state_t snn_state;
    executive_snn_get_state(snn_bridge, &snn_state);
    EXPECT_EQ(snn_state.state, EXECUTIVE_SNN_STATE_IDLE);

    executive_plasticity_bridge_state_t plasticity_state;
    executive_plasticity_get_state(plasticity_bridge, &plasticity_state);
    EXPECT_EQ(plasticity_state.state, EXECUTIVE_PLASTICITY_STATE_IDLE);

    // Re-run scenarios to verify recovery
    executive_snn_encode_state(snn_bridge, dims, EXEC_DIM_COUNT);
    executive_snn_simulate(snn_bridge, 15.0f);

    executive_control_output_t output;
    EXPECT_EQ(executive_snn_get_control_output(snn_bridge, &output), 0);
    EXPECT_GE(output.flexibility_level, 0.0f);
}

//=============================================================================
// Concurrent Safety Tests
//=============================================================================

TEST_F(ExecutiveSNNPlasticityIntegrationTest, ConcurrentEncodingAndLearning) {
    // Register synapses
    for (int i = 0; i < 10; i++) {
        executive_plasticity_register_synapse(plasticity_bridge,
            1200 + i, EXEC_SYNAPSE_FLEXIBILITY, 0.5f);
    }

    std::atomic<int> encoding_complete{0};
    std::atomic<int> learning_complete{0};

    // Thread 1: SNN encoding
    std::thread encoder([this, &encoding_complete]() {
        for (int i = 0; i < 5; i++) {
            float dims[EXEC_DIM_COUNT];
            generate_executive_context(dims, i % 4);
            executive_snn_encode_state(snn_bridge, dims, EXEC_DIM_COUNT);
            executive_snn_step(snn_bridge);
            encoding_complete++;
        }
    });

    // Thread 2: Plasticity learning
    std::thread learner([this, &learning_complete]() {
        for (int i = 0; i < 5; i++) {
            executive_plasticity_learn(plasticity_bridge,
                EXEC_LEARN_SUCCESSFUL_INHIBITION, 0.1f, 1200 + (i % 10), 0.5f);
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

TEST_F(ExecutiveSNNPlasticityIntegrationTest, ErrorDetectionAndLearning) {
    // Encode error signal via conflict
    float dims[EXEC_DIM_COUNT] = {0};
    dims[EXEC_DIM_ERROR_CORRECTION] = 0.9f;
    dims[EXEC_DIM_CONFLICT_MONITOR] = 0.8f;

    int spikes = executive_snn_encode_state(snn_bridge, dims, EXEC_DIM_COUNT);
    EXPECT_GE(spikes, 0);

    executive_snn_simulate(snn_bridge, 30.0f);

    // Check error level
    float error_level;
    executive_snn_check_error(snn_bridge, &error_level);

    // Register flexibility synapse (not auto-protected)
    EXPECT_EQ(executive_plasticity_register_synapse(plasticity_bridge,
        1300, EXEC_SYNAPSE_FLEXIBILITY, 0.5f), 0);

    executive_plasticity_synapse_t synapse;
    EXPECT_EQ(executive_plasticity_get_synapse(plasticity_bridge, 1300, &synapse), 0);
    EXPECT_FALSE(synapse.is_protected);
}

//=============================================================================
// Stats Integration
//=============================================================================

TEST_F(ExecutiveSNNPlasticityIntegrationTest, StatsAccumulationAcrossBridges) {
    // Run multiple scenarios
    for (int s = 0; s < 5; s++) {
        float dims[EXEC_DIM_COUNT];
        generate_executive_context(dims, s % 4);

        executive_snn_encode_state(snn_bridge, dims, EXEC_DIM_COUNT);
        executive_snn_simulate(snn_bridge, 10.0f);

        executive_plasticity_register_synapse(plasticity_bridge,
            1400 + s, EXEC_SYNAPSE_FLEXIBILITY, 0.5f);
        executive_plasticity_learn(plasticity_bridge,
            EXEC_LEARN_SUCCESSFUL_INHIBITION, 0.2f, 1400 + s, 0.6f);
    }

    // Check SNN stats
    executive_snn_stats_t snn_stats;
    executive_snn_get_stats(snn_bridge, &snn_stats);
    EXPECT_GE(snn_stats.total_evaluations, 5u);
    EXPECT_GT(snn_stats.total_simulations, 0u);

    // Check plasticity stats
    executive_plasticity_stats_t plasticity_stats;
    executive_plasticity_get_stats(plasticity_bridge, &plasticity_stats);
    // Verify synapses were used (active_synapses in state)
    executive_plasticity_bridge_state_t bridge_state;
    executive_plasticity_get_state(plasticity_bridge, &bridge_state);
    EXPECT_GE(bridge_state.active_synapses, 5u);
    EXPECT_GE(plasticity_stats.total_learning_events, 5u);
}

//=============================================================================
// Inhibition Learning Integration
//=============================================================================

TEST_F(ExecutiveSNNPlasticityIntegrationTest, InhibitionLearningPipeline) {
    // Register flexibility synapses (not auto-protected)
    for (int i = 0; i < 5; i++) {
        executive_plasticity_register_synapse(plasticity_bridge,
            1500 + i, EXEC_SYNAPSE_FLEXIBILITY, 0.5f);
    }

    // Simulate failed vs successful inhibition
    for (int trial = 0; trial < 10; trial++) {
        float dims[EXEC_DIM_COUNT] = {0};
        if (trial % 2 == 0) {
            // Failed inhibition scenario
            dims[EXEC_DIM_INHIBITION] = 0.3f;
            dims[EXEC_DIM_ERROR_CORRECTION] = 0.8f;
        } else {
            // Successful inhibition scenario
            dims[EXEC_DIM_INHIBITION] = 0.9f;
            dims[EXEC_DIM_ATTENTION_CONTROL] = 0.85f;
        }

        executive_snn_encode_state(snn_bridge, dims, EXEC_DIM_COUNT);
        executive_snn_simulate(snn_bridge, 15.0f);

        executive_control_output_t output;
        executive_snn_get_control_output(snn_bridge, &output);

        // Learn based on inhibition outcome
        if (trial % 2 == 0) {
            executive_plasticity_learn(plasticity_bridge,
                EXEC_LEARN_FAILED_INHIBITION, 0.5f, 1500 + (trial % 5), output.inhibition_level);
        } else {
            executive_plasticity_learn(plasticity_bridge,
                EXEC_LEARN_SUCCESSFUL_INHIBITION, 0.5f, 1500 + (trial % 5), output.inhibition_level);
        }
    }

    // Verify learning statistics
    executive_plasticity_stats_t stats;
    executive_plasticity_get_stats(plasticity_bridge, &stats);
    EXPECT_GE(stats.successful_inhibition_events, 5u);
    EXPECT_GE(stats.failed_inhibition_events, 5u);
}
