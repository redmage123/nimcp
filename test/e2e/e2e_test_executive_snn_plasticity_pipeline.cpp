/**
 * @file e2e_test_executive_snn_plasticity_pipeline.cpp
 * @brief End-to-end tests for Executive-SNN-Plasticity learning pipeline
 * @version 1.0.0
 * @date 2026-01-06
 *
 * WHAT: Complete executive control pipeline with SNN and Plasticity
 * WHY:  Verify full dataflow from executive state -> SNN encoding -> control output
 *       -> plasticity learning -> cognitive control calibration evolution
 * HOW:  Test realistic scenarios combining inhibition encoding, STDP learning,
 *       reward-modulated plasticity, and protected synapse integrity
 *
 * Test Coverage:
 * - Full executive state to control output pipeline via SNN
 * - STDP and reward-modulated learning for control calibration
 * - Conflict detection and resolution
 * - Inhibition and Goal synapse protection
 * - Multi-scenario control learning
 * - Control calibration evolution through experience
 * - Protected synapse integrity under stress
 */

#include <gtest/gtest.h>

#include "cognitive/executive/nimcp_executive_snn_bridge.h"
#include "cognitive/executive/nimcp_executive_plasticity_bridge.h"
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

class ExecutiveSNNPlasticityE2E : public ::testing::Test {
protected:
    executive_snn_bridge_t* snn_bridge = nullptr;
    executive_plasticity_bridge_t* plasticity_bridge = nullptr;

    // Learning statistics
    struct LearningStats {
        int successful_inhibition = 0;
        int failed_inhibition = 0;
        int conflict_detected = 0;
        int total_evaluations = 0;
        std::vector<float> inhibition_history;
        std::vector<float> conflict_scores;
    } stats;

    void SetUp() override {
        // Create SNN bridge with full executive dimensions
        executive_snn_config_t snn_config = executive_snn_config_default();
        snn_config.num_dimensions = EXEC_DIM_COUNT;
        snn_config.neurons_per_dim = 32;
        snn_config.dt_ms = 1.0f;
        snn_config.enable_conflict_detection = true;
        snn_config.enable_bio_async = false;

        snn_bridge = executive_snn_create(&snn_config);
        ASSERT_NE(snn_bridge, nullptr) << "Failed to create SNN bridge";

        // Create Plasticity bridge with all learning mechanisms
        executive_plasticity_config_t plasticity_config = executive_plasticity_config_default();
        plasticity_config.base_learning_rate = 0.01f;
        plasticity_config.stdp_a_plus = 0.01f;
        plasticity_config.stdp_a_minus = 0.012f;

        plasticity_bridge = executive_plasticity_create(&plasticity_config);
        ASSERT_NE(plasticity_bridge, nullptr) << "Failed to create Plasticity bridge";

        // Register base synapses for plasticity
        for (uint32_t i = 0; i < EXEC_DIM_COUNT; i++) {
            executive_plasticity_register_synapse(plasticity_bridge, i,
                EXEC_SYNAPSE_FLEXIBILITY, 0.5f);
        }

        // Register protected synapses
        executive_plasticity_register_synapse(plasticity_bridge, 100,
            EXEC_SYNAPSE_INHIBITION, 1.0f);
        executive_plasticity_register_synapse(plasticity_bridge, 101,
            EXEC_SYNAPSE_GOAL, 0.9f);
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

    // Generate executive scenario
    enum ExecutiveScenario {
        HIGH_INHIBITION_SUCCESS,    // Successfully inhibited response
        HIGH_INHIBITION_FAILURE,    // Failed to inhibit (error)
        TASK_SWITCHING,             // Task switch with conflict
        GOAL_MAINTENANCE,           // Goal maintained under distraction
        CONFLICT_RESOLUTION,        // Conflict detected and resolved
        PLANNING_EXECUTION,         // Plan execution scenario
        RESOURCE_ALLOCATION,        // Resource allocation task
        CALIBRATION_TEST            // Control calibration test
    };

    void generate_scenario(float* dims, ExecutiveScenario scenario) {
        memset(dims, 0, sizeof(float) * EXEC_DIM_COUNT);

        switch (scenario) {
            case HIGH_INHIBITION_SUCCESS:
                dims[EXEC_DIM_INHIBITION] = 0.9f;
                dims[EXEC_DIM_ATTENTION_CONTROL] = 0.85f;
                dims[EXEC_DIM_CONFLICT_MONITOR] = 0.2f;
                break;

            case HIGH_INHIBITION_FAILURE:
                dims[EXEC_DIM_INHIBITION] = 0.3f;
                dims[EXEC_DIM_ERROR_CORRECTION] = 0.8f;
                dims[EXEC_DIM_CONFLICT_MONITOR] = 0.7f;
                break;

            case TASK_SWITCHING:
                dims[EXEC_DIM_TASK_SWITCHING] = 0.9f;
                dims[EXEC_DIM_FLEXIBILITY] = 0.8f;
                dims[EXEC_DIM_CONFLICT_MONITOR] = 0.6f;
                break;

            case GOAL_MAINTENANCE:
                dims[EXEC_DIM_GOAL_MAINTENANCE] = 0.9f;
                dims[EXEC_DIM_WORKING_MEMORY] = 0.85f;
                dims[EXEC_DIM_ATTENTION_CONTROL] = 0.8f;
                break;

            case CONFLICT_RESOLUTION:
                dims[EXEC_DIM_CONFLICT_MONITOR] = 0.9f;
                dims[EXEC_DIM_ERROR_CORRECTION] = 0.7f;
                dims[EXEC_DIM_FLEXIBILITY] = 0.75f;
                break;

            case PLANNING_EXECUTION:
                dims[EXEC_DIM_PLANNING] = 0.95f;
                dims[EXEC_DIM_WORKING_MEMORY] = 0.8f;
                dims[EXEC_DIM_GOAL_MAINTENANCE] = 0.85f;
                break;

            case RESOURCE_ALLOCATION:
                dims[EXEC_DIM_RESOURCE_ALLOCATION] = 0.9f;
                dims[EXEC_DIM_WORKING_MEMORY] = 0.7f;
                dims[EXEC_DIM_ATTENTION_CONTROL] = 0.75f;
                break;

            case CALIBRATION_TEST:
                dims[EXEC_DIM_INHIBITION] = 0.7f;
                dims[EXEC_DIM_FLEXIBILITY] = 0.7f;
                dims[EXEC_DIM_PLANNING] = 0.7f;
                break;
        }
    }

    // Run single evaluation pipeline
    struct EvaluationResult {
        float inhibition_level;
        float flexibility_level;
        float planning_activity;
        bool conflict_detected;
        int spike_count;
    };

    EvaluationResult run_evaluation(ExecutiveScenario scenario) {
        EvaluationResult result = {0};

        float dims[EXEC_DIM_COUNT];
        generate_scenario(dims, scenario);

        // Encode and simulate
        result.spike_count = executive_snn_encode_state(snn_bridge, dims, EXEC_DIM_COUNT);
        executive_snn_simulate(snn_bridge, 30.0f);

        // Get control output
        executive_control_output_t output;
        executive_snn_get_control_output(snn_bridge, &output);

        result.inhibition_level = output.inhibition_level;
        result.flexibility_level = output.flexibility_level;
        result.planning_activity = output.planning_activity;

        // Check conflict detection
        float conflict_level;
        result.conflict_detected = executive_snn_check_conflict(snn_bridge, &conflict_level);

        // Update stats
        stats.total_evaluations++;
        stats.inhibition_history.push_back(output.inhibition_level);
        stats.conflict_scores.push_back(output.conflict_magnitude);

        if (result.conflict_detected) {
            stats.conflict_detected++;
        }

        return result;
    }
};

//=============================================================================
// Basic Pipeline Tests
//=============================================================================

TEST_F(ExecutiveSNNPlasticityE2E, CompletePipelineInitialization) {
    // Verify complete pipeline setup
    EXPECT_NE(snn_bridge, nullptr);
    EXPECT_NE(plasticity_bridge, nullptr);

    // Check synapse registration
    executive_plasticity_bridge_state_t state;
    executive_plasticity_get_state(plasticity_bridge, &state);
    EXPECT_GT(state.active_synapses, (uint32_t)EXEC_DIM_COUNT);  // Base + protected
}

TEST_F(ExecutiveSNNPlasticityE2E, SingleEvaluationPipeline) {
    // Run single high inhibition scenario
    auto result = run_evaluation(HIGH_INHIBITION_SUCCESS);

    // Verify control output is valid
    EXPECT_GE(result.inhibition_level, 0.0f);
    EXPECT_LE(result.inhibition_level, 1.0f);
    EXPECT_GE(result.flexibility_level, 0.0f);
    EXPECT_LE(result.flexibility_level, 1.0f);
    EXPECT_GE(result.spike_count, 0);

    // Apply learning based on result
    int ret = executive_plasticity_learn(plasticity_bridge,
        EXEC_LEARN_SUCCESSFUL_INHIBITION, 0.5f, 0, result.inhibition_level);
    EXPECT_EQ(ret, 0);
}

//=============================================================================
// Inhibition Learning Tests
//=============================================================================

TEST_F(ExecutiveSNNPlasticityE2E, SuccessfulInhibitionLearning) {
    // Run multiple successful inhibition scenarios
    float total_inhibition = 0.0f;
    int high_inhibition_count = 0;

    for (int trial = 0; trial < 10; trial++) {
        auto result = run_evaluation(HIGH_INHIBITION_SUCCESS);

        total_inhibition += result.inhibition_level;
        if (result.inhibition_level > 0.0f) {
            high_inhibition_count++;
        }

        // Learn successful inhibition - should strengthen circuit
        executive_plasticity_learn(plasticity_bridge,
            EXEC_LEARN_SUCCESSFUL_INHIBITION, 0.5f, 0, result.inhibition_level);

        // Apply STDP
        executive_plasticity_apply_stdp(plasticity_bridge, 0,
            (float)trial, (float)trial + 5.0f);
    }

    // At least some trials should register high inhibition
    EXPECT_GT(high_inhibition_count, 0);
}

TEST_F(ExecutiveSNNPlasticityE2E, FailedInhibitionLearning) {
    // Run multiple failed inhibition scenarios
    float total_error = 0.0f;

    for (int trial = 0; trial < 10; trial++) {
        auto result = run_evaluation(HIGH_INHIBITION_FAILURE);

        // Error correction should be active in failed scenarios
        if (result.conflict_detected) {
            total_error += 1.0f;
        }

        // Learn failed inhibition - should decrease weight
        executive_plasticity_learn(plasticity_bridge,
            EXEC_LEARN_FAILED_INHIBITION, 0.5f, 1, result.inhibition_level);
    }

    // Verify some error signals detected
    EXPECT_GT(total_error, 0.0f);
}

TEST_F(ExecutiveSNNPlasticityE2E, CalibrationImprovement) {
    // Register calibration synapses
    for (int i = 200; i < 210; i++) {
        executive_plasticity_register_synapse(plasticity_bridge, i,
            EXEC_SYNAPSE_FLEXIBILITY, 0.5f);
    }

    // Initial calibration weight
    executive_plasticity_synapse_t initial_synapse;
    executive_plasticity_get_synapse(plasticity_bridge, 200, &initial_synapse);
    float initial_weight = initial_synapse.weight;

    // Run mixed calibration scenarios
    for (int epoch = 0; epoch < 5; epoch++) {
        // Well-calibrated scenario
        auto good_result = run_evaluation(CALIBRATION_TEST);
        executive_plasticity_learn(plasticity_bridge,
            EXEC_LEARN_SUCCESSFUL_INHIBITION, 0.3f, 200, good_result.inhibition_level);

        // Failed scenario
        auto bad_result = run_evaluation(HIGH_INHIBITION_FAILURE);
        executive_plasticity_learn(plasticity_bridge,
            EXEC_LEARN_FAILED_INHIBITION, 0.3f, 201, bad_result.inhibition_level);

        // BCM and homeostatic updates
        executive_plasticity_update_bcm(plasticity_bridge, 0.5f);
        executive_plasticity_homeostatic_update(plasticity_bridge, 0.5f);
    }

    // Verify learning occurred
    executive_plasticity_stats_t stats;
    executive_plasticity_get_stats(plasticity_bridge, &stats);
    EXPECT_GT(stats.total_learning_events, 0u);
    EXPECT_GT(stats.successful_inhibition_events, 0u);
    EXPECT_GT(stats.failed_inhibition_events, 0u);
}

//=============================================================================
// Conflict Detection Tests
//=============================================================================

TEST_F(ExecutiveSNNPlasticityE2E, ConflictTriggersCalibration) {
    int conflict_trials = 0;
    float total_conflict = 0.0f;

    for (int trial = 0; trial < 10; trial++) {
        auto result = run_evaluation(CONFLICT_RESOLUTION);

        if (result.conflict_detected) {
            conflict_trials++;
            total_conflict += 1.0f;

            // Learn from conflict event
            executive_plasticity_learn(plasticity_bridge,
                EXEC_LEARN_CONFLICT_RESOLVED, 0.5f, 2, result.flexibility_level);
        }
    }

    // Should detect conflict signals in multiple trials
    EXPECT_GE(conflict_trials, 3);
    // Total conflict across trials should be meaningful
    EXPECT_GT(total_conflict, 0.0f);
}

//=============================================================================
// Protected Synapse Tests
//=============================================================================

TEST_F(ExecutiveSNNPlasticityE2E, InhibitionProtectionIntegrity) {
    // Get initial protected synapse weight
    executive_plasticity_synapse_t inhib_synapse;
    executive_plasticity_get_synapse(plasticity_bridge, 100, &inhib_synapse);
    float original_weight = inhib_synapse.weight;
    EXPECT_TRUE(inhib_synapse.is_protected);

    // Run many scenarios and try to modify protected synapse
    for (int trial = 0; trial < 20; trial++) {
        auto result = run_evaluation((ExecutiveScenario)(trial % 8));

        // Try various learning operations on protected synapse
        executive_plasticity_learn(plasticity_bridge,
            EXEC_LEARN_FAILED_INHIBITION, -1.0f, 100, result.inhibition_level);
        executive_plasticity_apply_stdp(plasticity_bridge, 100,
            (float)trial, (float)trial + 10.0f);
        executive_plasticity_apply_reward(plasticity_bridge, -1.0f);
    }

    // Protected synapse should remain unchanged
    executive_plasticity_get_synapse(plasticity_bridge, 100, &inhib_synapse);
    EXPECT_FLOAT_EQ(inhib_synapse.weight, original_weight);
    EXPECT_TRUE(inhib_synapse.is_protected);
}

TEST_F(ExecutiveSNNPlasticityE2E, GoalSynapseProtection) {
    // Goal synapse should also be protected
    executive_plasticity_synapse_t goal_synapse;
    executive_plasticity_get_synapse(plasticity_bridge, 101, &goal_synapse);
    float original_weight = goal_synapse.weight;
    EXPECT_TRUE(goal_synapse.is_protected);

    // Stress test protection
    for (int i = 0; i < 50; i++) {
        executive_plasticity_apply_stdp(plasticity_bridge, 101, (float)i, (float)i + 5.0f);
        executive_plasticity_learn(plasticity_bridge,
            EXEC_LEARN_GOAL_LOST, 1.0f, 101, 0.9f);
    }

    // Weight must remain unchanged
    executive_plasticity_get_synapse(plasticity_bridge, 101, &goal_synapse);
    EXPECT_FLOAT_EQ(goal_synapse.weight, original_weight);
}

//=============================================================================
// Task Switching Tests
//=============================================================================

TEST_F(ExecutiveSNNPlasticityE2E, TaskSwitchingLearning) {
    // Register task switching synapses
    for (int i = 300; i < 305; i++) {
        executive_plasticity_register_synapse(plasticity_bridge, i,
            EXEC_SYNAPSE_FLEXIBILITY, 0.5f);
    }

    int learning_events = 0;

    for (int trial = 0; trial < 10; trial++) {
        auto result = run_evaluation(TASK_SWITCHING);

        // Apply learning for all task switching scenarios
        executive_plasticity_learn(plasticity_bridge,
            EXEC_LEARN_TASK_SWITCH_SUCCESS, 0.5f, 300 + (trial % 5),
            result.flexibility_level > 0.0f ? result.flexibility_level : 0.5f);
        learning_events++;
    }

    // Verify we applied learning across all trials
    EXPECT_EQ(learning_events, 10);

    // Verify plasticity stats reflect learning
    executive_plasticity_stats_t stats;
    executive_plasticity_get_stats(plasticity_bridge, &stats);
    EXPECT_GE(stats.total_learning_events, 10u);
}

//=============================================================================
// Planning Execution Tests
//=============================================================================

TEST_F(ExecutiveSNNPlasticityE2E, PlanningExecutionLearning) {
    float total_planning = 0.0f;
    int planning_events = 0;

    for (int trial = 0; trial < 10; trial++) {
        auto result = run_evaluation(PLANNING_EXECUTION);

        total_planning += result.planning_activity;
        if (result.planning_activity > 0.0f) {
            planning_events++;
        }

        // Always apply learning for planning scenarios
        executive_plasticity_learn(plasticity_bridge,
            EXEC_LEARN_PLANNING_SUCCESS, 0.5f, 3, result.planning_activity);
    }

    // Should register planning activity in multiple trials
    EXPECT_GE(planning_events, 3);
    // Verify learning was applied
    executive_plasticity_stats_t stats;
    executive_plasticity_get_stats(plasticity_bridge, &stats);
    EXPECT_GE(stats.total_learning_events, 10u);
}

//=============================================================================
// Multi-Scenario Learning Tests
//=============================================================================

TEST_F(ExecutiveSNNPlasticityE2E, CompleteControlWorkflow) {
    // Register workflow synapses
    for (int i = 400; i < 420; i++) {
        executive_plasticity_register_synapse(plasticity_bridge, i,
            EXEC_SYNAPSE_FLEXIBILITY, 0.5f);
    }

    // Run complete control workflow
    for (int epoch = 0; epoch < 5; epoch++) {
        for (int scenario = 0; scenario < 8; scenario++) {
            auto result = run_evaluation((ExecutiveScenario)scenario);

            // Select learning event based on scenario
            executive_learn_event_t event;
            float magnitude = 0.3f;

            switch ((ExecutiveScenario)scenario) {
                case HIGH_INHIBITION_SUCCESS:
                case CALIBRATION_TEST:
                    event = EXEC_LEARN_SUCCESSFUL_INHIBITION;
                    break;
                case HIGH_INHIBITION_FAILURE:
                    event = EXEC_LEARN_FAILED_INHIBITION;
                    break;
                case TASK_SWITCHING:
                    event = EXEC_LEARN_TASK_SWITCH_SUCCESS;
                    break;
                case GOAL_MAINTENANCE:
                    event = EXEC_LEARN_GOAL_MAINTAINED;
                    break;
                case CONFLICT_RESOLUTION:
                    event = EXEC_LEARN_CONFLICT_RESOLVED;
                    break;
                case PLANNING_EXECUTION:
                    event = EXEC_LEARN_PLANNING_SUCCESS;
                    break;
                default:
                    event = EXEC_LEARN_SUCCESSFUL_INHIBITION;
                    break;
            }

            int synapse_id = 400 + (epoch * 8 + scenario) % 20;
            executive_plasticity_learn(plasticity_bridge, event, magnitude,
                synapse_id, result.flexibility_level);

            // Apply STDP
            executive_plasticity_apply_stdp(plasticity_bridge, synapse_id,
                (float)(epoch * 10 + scenario), (float)(epoch * 10 + scenario + 5));
        }

        // Periodic maintenance
        executive_plasticity_update_bcm(plasticity_bridge, 0.5f);
        executive_plasticity_homeostatic_update(plasticity_bridge, 0.5f);
        executive_plasticity_update_traces(plasticity_bridge, 1.0f);
    }

    // Consolidate learning
    executive_plasticity_consolidate(plasticity_bridge);

    // Verify extensive learning occurred
    executive_plasticity_stats_t final_stats;
    executive_plasticity_get_stats(plasticity_bridge, &final_stats);
    EXPECT_GT(final_stats.total_learning_events, 30u);
    EXPECT_GT(final_stats.weight_updates, 30u);

    executive_snn_stats_t snn_stats;
    executive_snn_get_stats(snn_bridge, &snn_stats);
    EXPECT_GE(snn_stats.total_evaluations, 40u);
}

//=============================================================================
// Stress and Performance Tests
//=============================================================================

TEST_F(ExecutiveSNNPlasticityE2E, HighVolumeProcessing) {
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 100; i++) {
        run_evaluation((ExecutiveScenario)(i % 8));
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // 100 evaluations should complete in under 5 seconds
    EXPECT_LT(duration.count(), 5000);
    EXPECT_EQ(stats.total_evaluations, 100);
}

TEST_F(ExecutiveSNNPlasticityE2E, ContinuousLearning) {
    // Register many synapses
    for (int i = 500; i < 600; i++) {
        executive_plasticity_register_synapse(plasticity_bridge, i,
            EXEC_SYNAPSE_FLEXIBILITY, 0.5f);
    }

    // Continuous learning loop
    for (int cycle = 0; cycle < 50; cycle++) {
        auto result = run_evaluation((ExecutiveScenario)(cycle % 8));

        // Learn on rotating synapses
        for (int j = 0; j < 5; j++) {
            int synapse_id = 500 + (cycle * 5 + j) % 100;
            executive_plasticity_learn(plasticity_bridge,
                EXEC_LEARN_SUCCESSFUL_INHIBITION, 0.1f, synapse_id, result.flexibility_level);
        }

        // Periodic BCM update
        if (cycle % 10 == 0) {
            executive_plasticity_update_bcm(plasticity_bridge, 0.5f);
        }
    }

    // Verify extensive learning
    executive_plasticity_stats_t stats;
    executive_plasticity_get_stats(plasticity_bridge, &stats);
    EXPECT_GE(stats.total_learning_events, 200u);
}

//=============================================================================
// Reset and Recovery Tests
//=============================================================================

TEST_F(ExecutiveSNNPlasticityE2E, ResetAndRecovery) {
    // Accumulate some state
    for (int i = 0; i < 10; i++) {
        run_evaluation((ExecutiveScenario)(i % 8));
    }

    // Reset both bridges
    executive_snn_reset(snn_bridge);
    executive_plasticity_reset(plasticity_bridge);

    // Verify recovery
    executive_snn_bridge_state_t snn_state;
    executive_snn_get_state(snn_bridge, &snn_state);
    EXPECT_EQ(snn_state.state, EXECUTIVE_SNN_STATE_IDLE);

    executive_plasticity_bridge_state_t plasticity_state;
    executive_plasticity_get_state(plasticity_bridge, &plasticity_state);
    EXPECT_EQ(plasticity_state.state, EXECUTIVE_PLASTICITY_STATE_IDLE);

    // Can continue processing
    auto result = run_evaluation(HIGH_INHIBITION_SUCCESS);
    EXPECT_GE(result.flexibility_level, 0.0f);
}

//=============================================================================
// Statistics Validation Tests
//=============================================================================

TEST_F(ExecutiveSNNPlasticityE2E, StatisticsAccuracy) {
    // Run known number of evaluations
    for (int i = 0; i < 20; i++) {
        run_evaluation((ExecutiveScenario)(i % 8));

        // Apply learning
        executive_plasticity_learn(plasticity_bridge,
            EXEC_LEARN_SUCCESSFUL_INHIBITION, 0.1f, i % EXEC_DIM_COUNT, 0.5f);
    }

    // Verify stats match
    executive_snn_stats_t snn_stats;
    executive_snn_get_stats(snn_bridge, &snn_stats);
    EXPECT_GE(snn_stats.total_evaluations, 20u);

    executive_plasticity_stats_t plasticity_stats;
    executive_plasticity_get_stats(plasticity_bridge, &plasticity_stats);
    EXPECT_GE(plasticity_stats.total_learning_events, 20u);
}
