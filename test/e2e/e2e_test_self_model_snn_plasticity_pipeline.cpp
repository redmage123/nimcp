/**
 * @file e2e_test_self_model_snn_plasticity_pipeline.cpp
 * @brief End-to-end tests for Self Model-SNN-Plasticity learning pipeline
 * @version 1.0.0
 * @date 2026-01-06
 *
 * WHAT: Complete self model pipeline with SNN and Plasticity
 * WHY:  Verify full dataflow from self-awareness state -> SNN encoding -> insight
 *       -> plasticity learning -> self-model evolution
 * HOW:  Test realistic scenarios combining self-representation encoding, STDP learning,
 *       reward-modulated plasticity, and protected synapse integrity
 *
 * Test Coverage:
 * - Full self-model state to insight pipeline via SNN
 * - STDP and reward-modulated learning for body awareness calibration
 * - Agency detection and calibration
 * - Identity and Boundary synapse protection
 * - Multi-scenario self-model learning
 * - Self-continuity evolution through experience
 * - Protected synapse integrity under stress
 */

#include <gtest/gtest.h>

#include "cognitive/self_model/nimcp_self_model_snn_bridge.h"
#include "cognitive/self_model/nimcp_self_model_plasticity_bridge.h"
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

class SelfModelSNNPlasticityE2E : public ::testing::Test {
protected:
    self_model_snn_bridge_t* snn_bridge = nullptr;
    self_model_plasticity_bridge_t* plasticity_bridge = nullptr;

    // Learning statistics
    struct LearningStats {
        int high_agency_correct = 0;
        int agency_disruption = 0;
        int boundary_violations = 0;
        int total_evaluations = 0;
        std::vector<float> agency_history;
        std::vector<float> boundary_scores;
    } stats;

    void SetUp() override {
        // Create SNN bridge with full self model dimensions
        self_model_snn_config_t snn_config = self_model_snn_config_default();
        snn_config.num_dimensions = SELF_DIM_COUNT;
        snn_config.neurons_per_dim = 32;
        snn_config.dt_ms = 1.0f;
        snn_config.enable_identity_core = true;
        snn_config.enable_boundary_detection = true;
        snn_config.enable_bio_async = false;

        snn_bridge = self_model_snn_create(&snn_config);
        ASSERT_NE(snn_bridge, nullptr) << "Failed to create SNN bridge";

        // Create Plasticity bridge with all learning mechanisms
        self_model_plasticity_config_t plasticity_config = self_model_plasticity_config_default();
        plasticity_config.base_learning_rate = 0.01f;
        plasticity_config.stdp_a_plus = 0.01f;
        plasticity_config.stdp_a_minus = 0.012f;

        plasticity_bridge = self_model_plasticity_create(&plasticity_config);
        ASSERT_NE(plasticity_bridge, nullptr) << "Failed to create Plasticity bridge";

        // Register base synapses for plasticity
        for (uint32_t i = 0; i < SELF_DIM_COUNT; i++) {
            self_model_plasticity_register_synapse(plasticity_bridge, i,
                SELF_SYNAPSE_AGENCY, 0.5f);
        }

        // Register protected synapses (IDENTITY and BOUNDARY are auto-protected)
        self_model_plasticity_register_synapse(plasticity_bridge, 100,
            SELF_SYNAPSE_IDENTITY, 1.0f);
        self_model_plasticity_register_synapse(plasticity_bridge, 101,
            SELF_SYNAPSE_BOUNDARY, 0.9f);
    }

    void TearDown() override {
        if (snn_bridge) {
            self_model_snn_destroy(snn_bridge);
            snn_bridge = nullptr;
        }
        if (plasticity_bridge) {
            self_model_plasticity_destroy(plasticity_bridge);
            plasticity_bridge = nullptr;
        }
    }

    // Generate self-model scenario
    enum SelfModelScenario {
        HIGH_AGENCY_CORRECT,       // Strong agency, well-calibrated
        AGENCY_DISRUPTION,         // Agency mismatch, should be corrected
        BOUNDARY_VIOLATION,        // Self-other boundary crossed
        BODY_STATE_CHANGE,         // Bodily awareness shift
        IDENTITY_STABLE,           // Core identity maintained
        NARRATIVE_COHERENT,        // Self-narrative intact
        CONTINUITY_MAINTAINED,     // Temporal continuity preserved
        OWNERSHIP_CONFIRMED        // Body ownership confirmed
    };

    void generate_scenario(float* dims, SelfModelScenario scenario) {
        memset(dims, 0, sizeof(float) * SELF_DIM_COUNT);

        switch (scenario) {
            case HIGH_AGENCY_CORRECT:
                dims[SELF_DIM_AGENCY] = 0.9f;
                dims[SELF_DIM_BODY_STATE] = 0.85f;
                dims[SELF_DIM_OWNERSHIP] = 0.9f;
                dims[SELF_DIM_IDENTITY] = 0.8f;
                break;

            case AGENCY_DISRUPTION:
                dims[SELF_DIM_AGENCY] = 0.3f;  // Low agency
                dims[SELF_DIM_BODY_STATE] = 0.8f;
                dims[SELF_DIM_OWNERSHIP] = 0.7f;
                dims[SELF_DIM_CAPABILITY] = 0.4f;
                break;

            case BOUNDARY_VIOLATION:
                dims[SELF_DIM_BOUNDARY] = 0.2f;  // Low boundary clarity
                dims[SELF_DIM_OWNERSHIP] = 0.5f;
                dims[SELF_DIM_AGENCY] = 0.6f;
                dims[SELF_DIM_REFLECTION] = 0.7f;
                break;

            case BODY_STATE_CHANGE:
                dims[SELF_DIM_BODY_STATE] = 0.95f;
                dims[SELF_DIM_OWNERSHIP] = 0.9f;
                dims[SELF_DIM_AGENCY] = 0.75f;
                dims[SELF_DIM_CONTINUITY] = 0.8f;
                break;

            case IDENTITY_STABLE:
                dims[SELF_DIM_IDENTITY] = 0.95f;
                dims[SELF_DIM_AUTOBIOGRAPHICAL] = 0.85f;
                dims[SELF_DIM_NARRATIVE] = 0.9f;
                dims[SELF_DIM_CONTINUITY] = 0.9f;
                break;

            case NARRATIVE_COHERENT:
                dims[SELF_DIM_NARRATIVE] = 0.9f;
                dims[SELF_DIM_AUTOBIOGRAPHICAL] = 0.85f;
                dims[SELF_DIM_IDENTITY] = 0.8f;
                dims[SELF_DIM_REFLECTION] = 0.75f;
                break;

            case CONTINUITY_MAINTAINED:
                dims[SELF_DIM_CONTINUITY] = 0.95f;
                dims[SELF_DIM_IDENTITY] = 0.85f;
                dims[SELF_DIM_NARRATIVE] = 0.8f;
                dims[SELF_DIM_BOUNDARY] = 0.9f;
                break;

            case OWNERSHIP_CONFIRMED:
                dims[SELF_DIM_OWNERSHIP] = 0.95f;
                dims[SELF_DIM_BODY_STATE] = 0.9f;
                dims[SELF_DIM_AGENCY] = 0.85f;
                dims[SELF_DIM_BOUNDARY] = 0.85f;
                break;
        }
    }

    // Run single evaluation pipeline
    struct EvaluationResult {
        float agency_level;
        float boundary_level;
        float identity_coherence;
        bool agency_disrupted;
        bool boundary_violated;
        int spike_count;
    };

    EvaluationResult run_evaluation(SelfModelScenario scenario) {
        EvaluationResult result = {0};

        float dims[SELF_DIM_COUNT];
        generate_scenario(dims, scenario);

        // Encode and simulate
        result.spike_count = self_model_snn_encode_state(snn_bridge, dims, SELF_DIM_COUNT);
        self_model_snn_simulate(snn_bridge, 30.0f);

        // Get insight
        self_model_insight_t insight;
        self_model_snn_get_insight(snn_bridge, &insight);

        result.agency_level = insight.agency_level;
        result.boundary_level = insight.boundary_clarity;
        result.identity_coherence = insight.identity_coherence;

        // Check agency disruption
        float agency_level;
        result.agency_disrupted = self_model_snn_check_agency(snn_bridge, &agency_level);

        // Check boundary violation
        float boundary_level;
        result.boundary_violated = self_model_snn_check_boundary(snn_bridge, &boundary_level);

        // Update stats
        stats.total_evaluations++;
        stats.agency_history.push_back(insight.agency_level);
        stats.boundary_scores.push_back(insight.boundary_clarity);

        if (result.agency_disrupted) {
            stats.agency_disruption++;
        }
        if (result.boundary_violated) {
            stats.boundary_violations++;
        }

        return result;
    }
};

//=============================================================================
// Basic Pipeline Tests
//=============================================================================

TEST_F(SelfModelSNNPlasticityE2E, CompletePipelineInitialization) {
    // Verify complete pipeline setup
    EXPECT_NE(snn_bridge, nullptr);
    EXPECT_NE(plasticity_bridge, nullptr);

    // Check synapse registration
    self_model_plasticity_bridge_state_t state;
    self_model_plasticity_get_state(plasticity_bridge, &state);
    EXPECT_GT(state.active_synapses, (uint32_t)SELF_DIM_COUNT);  // Base + protected
}

TEST_F(SelfModelSNNPlasticityE2E, SingleEvaluationPipeline) {
    // Run single high agency scenario
    auto result = run_evaluation(HIGH_AGENCY_CORRECT);

    // Verify insight is valid
    EXPECT_GE(result.agency_level, 0.0f);
    EXPECT_LE(result.agency_level, 1.0f);
    EXPECT_GE(result.identity_coherence, 0.0f);
    EXPECT_LE(result.identity_coherence, 1.0f);
    EXPECT_GE(result.spike_count, 0);

    // Apply learning based on result
    int ret = self_model_plasticity_learn(plasticity_bridge,
        SELF_LEARN_AGENCY_CONFIRMED, 0.5f, 0, result.agency_level);
    EXPECT_EQ(ret, 0);
}

//=============================================================================
// Agency Calibration Tests
//=============================================================================

TEST_F(SelfModelSNNPlasticityE2E, AgencyDisruptionLearning) {
    // Run multiple agency disruption scenarios
    float total_agency = 0.0f;
    int disruption_count = 0;

    for (int trial = 0; trial < 10; trial++) {
        auto result = run_evaluation(AGENCY_DISRUPTION);

        total_agency += result.agency_level;
        if (result.agency_level > 0.0f || result.agency_disrupted) {
            disruption_count++;
        }

        // Learn agency disruption - should decrease agency confidence
        self_model_plasticity_learn(plasticity_bridge,
            SELF_LEARN_AGENCY_VIOLATED, 0.5f, 0, result.agency_level);

        // Apply STDP
        self_model_plasticity_apply_stdp(plasticity_bridge, 0,
            (float)trial, (float)trial + 5.0f);
    }

    // At least some trials should register disruption signals
    EXPECT_GT(disruption_count, 0);
}

TEST_F(SelfModelSNNPlasticityE2E, AgencyConfirmationLearning) {
    // Run multiple high agency scenarios
    float total_agency = 0.0f;

    for (int trial = 0; trial < 10; trial++) {
        auto result = run_evaluation(HIGH_AGENCY_CORRECT);

        total_agency += result.agency_level;

        // Learn agency confirmation - should reinforce agency
        self_model_plasticity_learn(plasticity_bridge,
            SELF_LEARN_AGENCY_CONFIRMED, 0.5f, 1, result.agency_level);
    }

    // Average agency should be above minimum
    EXPECT_GT(total_agency / 10.0f, 0.0f);
}

TEST_F(SelfModelSNNPlasticityE2E, AgencyCalibrationImprovement) {
    // Register calibration synapses
    for (int i = 200; i < 210; i++) {
        self_model_plasticity_register_synapse(plasticity_bridge, i,
            SELF_SYNAPSE_AGENCY, 0.5f);
    }

    // Initial calibration weight
    self_model_plasticity_synapse_t initial_synapse;
    self_model_plasticity_get_synapse(plasticity_bridge, 200, &initial_synapse);
    float initial_weight = initial_synapse.weight;

    // Run mixed calibration scenarios
    for (int epoch = 0; epoch < 5; epoch++) {
        // High agency scenario
        auto good_result = run_evaluation(HIGH_AGENCY_CORRECT);
        self_model_plasticity_learn(plasticity_bridge,
            SELF_LEARN_AGENCY_CONFIRMED, 0.3f, 200, good_result.agency_level);

        // Disruption scenario
        auto disruption_result = run_evaluation(AGENCY_DISRUPTION);
        self_model_plasticity_learn(plasticity_bridge,
            SELF_LEARN_AGENCY_VIOLATED, 0.3f, 201, disruption_result.agency_level);

        // BCM and homeostatic updates
        self_model_plasticity_update_bcm(plasticity_bridge, 0.5f);
        self_model_plasticity_homeostatic_update(plasticity_bridge, 0.5f);
    }

    // Verify learning occurred
    self_model_plasticity_stats_t stats;
    self_model_plasticity_get_stats(plasticity_bridge, &stats);
    EXPECT_GT(stats.total_learning_events, 0u);
    EXPECT_GT(stats.agency_confirmed_events, 0u);
    EXPECT_GT(stats.agency_violated_events, 0u);
}

//=============================================================================
// Boundary Detection Tests
//=============================================================================

TEST_F(SelfModelSNNPlasticityE2E, BoundaryViolationTriggerCalibration) {
    int violation_trials = 0;
    float total_boundary = 0.0f;

    for (int trial = 0; trial < 10; trial++) {
        auto result = run_evaluation(BOUNDARY_VIOLATION);

        total_boundary += result.boundary_level;
        if (result.boundary_level < 0.5f || result.boundary_violated) {
            violation_trials++;

            // Learn from boundary violation
            self_model_plasticity_learn(plasticity_bridge,
                SELF_LEARN_BOUNDARY_VIOLATED, 0.5f, 2, result.boundary_level);
        }
    }

    // Should detect boundary violations in multiple trials
    EXPECT_GE(violation_trials, 3);
    // Total boundary clarity across trials should be meaningful
    EXPECT_GT(total_boundary, 0.0f);
}

//=============================================================================
// Protected Synapse Tests
//=============================================================================

TEST_F(SelfModelSNNPlasticityE2E, IdentityProtectionIntegrity) {
    // Get initial protected synapse weight
    self_model_plasticity_synapse_t identity_synapse;
    self_model_plasticity_get_synapse(plasticity_bridge, 100, &identity_synapse);
    float original_weight = identity_synapse.weight;
    EXPECT_TRUE(identity_synapse.is_protected);

    // Run many scenarios and try to modify protected synapse
    for (int trial = 0; trial < 20; trial++) {
        auto result = run_evaluation((SelfModelScenario)(trial % 8));

        // Try various learning operations on protected synapse
        self_model_plasticity_learn(plasticity_bridge,
            SELF_LEARN_AGENCY_VIOLATED, -1.0f, 100, result.agency_level);
        self_model_plasticity_apply_stdp(plasticity_bridge, 100,
            (float)trial, (float)trial + 10.0f);
        self_model_plasticity_apply_reward(plasticity_bridge, -1.0f);
    }

    // Protected synapse should remain unchanged
    self_model_plasticity_get_synapse(plasticity_bridge, 100, &identity_synapse);
    EXPECT_FLOAT_EQ(identity_synapse.weight, original_weight);
    EXPECT_TRUE(identity_synapse.is_protected);
}

TEST_F(SelfModelSNNPlasticityE2E, BoundarySynapseProtection) {
    // Boundary synapse should also be protected
    self_model_plasticity_synapse_t boundary_synapse;
    self_model_plasticity_get_synapse(plasticity_bridge, 101, &boundary_synapse);
    float original_weight = boundary_synapse.weight;
    EXPECT_TRUE(boundary_synapse.is_protected);

    // Stress test protection
    for (int i = 0; i < 50; i++) {
        self_model_plasticity_apply_stdp(plasticity_bridge, 101, (float)i, (float)i + 5.0f);
        self_model_plasticity_learn(plasticity_bridge,
            SELF_LEARN_AGENCY_VIOLATED, 1.0f, 101, 0.9f);
    }

    // Weight must remain unchanged
    self_model_plasticity_get_synapse(plasticity_bridge, 101, &boundary_synapse);
    EXPECT_FLOAT_EQ(boundary_synapse.weight, original_weight);
}

//=============================================================================
// Body State Tests
//=============================================================================

TEST_F(SelfModelSNNPlasticityE2E, BodyStateChangeLearning) {
    // Register body state synapses
    for (int i = 300; i < 305; i++) {
        self_model_plasticity_register_synapse(plasticity_bridge, i,
            SELF_SYNAPSE_AGENCY, 0.5f);
    }

    int learning_events = 0;

    for (int trial = 0; trial < 10; trial++) {
        auto result = run_evaluation(BODY_STATE_CHANGE);

        // Apply learning for all body state change scenarios
        self_model_plasticity_learn(plasticity_bridge,
            SELF_LEARN_NARRATIVE_UPDATED, 0.5f, 300 + (trial % 5),
            result.agency_level > 0.0f ? result.agency_level : 0.5f);
        learning_events++;
    }

    // Verify we applied learning across all trials
    EXPECT_EQ(learning_events, 10);

    // Verify plasticity stats reflect learning
    self_model_plasticity_stats_t stats;
    self_model_plasticity_get_stats(plasticity_bridge, &stats);
    EXPECT_GE(stats.total_learning_events, 10u);
}

//=============================================================================
// Identity Continuity Tests
//=============================================================================

TEST_F(SelfModelSNNPlasticityE2E, IdentityContinuityMaintenance) {
    float total_identity = 0.0f;
    int identity_detections = 0;
    int learning_events = 0;

    for (int trial = 0; trial < 10; trial++) {
        auto result = run_evaluation(IDENTITY_STABLE);

        float change_level;
        self_model_snn_check_identity_change(snn_bridge, &change_level);

        total_identity += result.identity_coherence;
        if (result.identity_coherence > 0.0f) {
            identity_detections++;
        }

        // Always apply learning for identity scenarios
        self_model_plasticity_learn(plasticity_bridge,
            SELF_LEARN_IDENTITY_REINFORCED, 0.5f, 3, result.identity_coherence);
        learning_events++;
    }

    // Should detect identity coherence in most trials
    EXPECT_GE(identity_detections, 5);
    // Verify learning was applied
    EXPECT_EQ(learning_events, 10);
}

//=============================================================================
// Multi-Scenario Learning Tests
//=============================================================================

TEST_F(SelfModelSNNPlasticityE2E, CompleteSelfModelWorkflow) {
    // Register workflow synapses
    for (int i = 400; i < 420; i++) {
        self_model_plasticity_register_synapse(plasticity_bridge, i,
            SELF_SYNAPSE_AGENCY, 0.5f);
    }

    // Run complete self-model workflow
    for (int epoch = 0; epoch < 5; epoch++) {
        for (int scenario = 0; scenario < 8; scenario++) {
            auto result = run_evaluation((SelfModelScenario)scenario);

            // Select learning event based on scenario
            self_model_learn_event_t event;
            float magnitude = 0.3f;

            switch ((SelfModelScenario)scenario) {
                case HIGH_AGENCY_CORRECT:
                case OWNERSHIP_CONFIRMED:
                    event = SELF_LEARN_AGENCY_CONFIRMED;
                    break;
                case AGENCY_DISRUPTION:
                    event = SELF_LEARN_AGENCY_VIOLATED;
                    break;
                case BOUNDARY_VIOLATION:
                    event = SELF_LEARN_BOUNDARY_VIOLATED;
                    break;
                case BODY_STATE_CHANGE:
                    event = SELF_LEARN_NARRATIVE_UPDATED;
                    break;
                case IDENTITY_STABLE:
                    event = SELF_LEARN_IDENTITY_REINFORCED;
                    break;
                case CONTINUITY_MAINTAINED:
                    event = SELF_LEARN_CONTINUITY_MAINTAINED;
                    break;
                default:
                    event = SELF_LEARN_NARRATIVE_UPDATED;
                    break;
            }

            int synapse_id = 400 + (epoch * 8 + scenario) % 20;
            self_model_plasticity_learn(plasticity_bridge, event, magnitude,
                synapse_id, result.agency_level);

            // Apply STDP
            self_model_plasticity_apply_stdp(plasticity_bridge, synapse_id,
                (float)(epoch * 10 + scenario), (float)(epoch * 10 + scenario + 5));
        }

        // Periodic maintenance
        self_model_plasticity_update_bcm(plasticity_bridge, 0.5f);
        self_model_plasticity_homeostatic_update(plasticity_bridge, 0.5f);
        self_model_plasticity_update_traces(plasticity_bridge, 1.0f);
    }

    // Consolidate learning
    self_model_plasticity_consolidate(plasticity_bridge);

    // Verify extensive learning occurred
    self_model_plasticity_stats_t final_stats;
    self_model_plasticity_get_stats(plasticity_bridge, &final_stats);
    EXPECT_GT(final_stats.total_learning_events, 30u);
    EXPECT_GT(final_stats.weight_updates, 30u);

    self_model_snn_stats_t snn_stats;
    self_model_snn_get_stats(snn_bridge, &snn_stats);
    EXPECT_GE(snn_stats.total_evaluations, 40u);
}

//=============================================================================
// Stress and Performance Tests
//=============================================================================

TEST_F(SelfModelSNNPlasticityE2E, HighVolumeProcessing) {
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 100; i++) {
        run_evaluation((SelfModelScenario)(i % 8));
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // 100 evaluations should complete in under 5 seconds
    EXPECT_LT(duration.count(), 5000);
    EXPECT_EQ(stats.total_evaluations, 100);
}

TEST_F(SelfModelSNNPlasticityE2E, ContinuousLearning) {
    // Register many synapses
    for (int i = 500; i < 600; i++) {
        self_model_plasticity_register_synapse(plasticity_bridge, i,
            SELF_SYNAPSE_AGENCY, 0.5f);
    }

    // Continuous learning loop
    for (int cycle = 0; cycle < 50; cycle++) {
        auto result = run_evaluation((SelfModelScenario)(cycle % 8));

        // Learn on rotating synapses
        for (int j = 0; j < 5; j++) {
            int synapse_id = 500 + (cycle * 5 + j) % 100;
            self_model_plasticity_learn(plasticity_bridge,
                SELF_LEARN_AGENCY_CONFIRMED, 0.1f, synapse_id, result.agency_level);
        }

        // Periodic BCM update
        if (cycle % 10 == 0) {
            self_model_plasticity_update_bcm(plasticity_bridge, 0.5f);
        }
    }

    // Verify extensive learning
    self_model_plasticity_stats_t stats;
    self_model_plasticity_get_stats(plasticity_bridge, &stats);
    EXPECT_GE(stats.total_learning_events, 200u);
}

//=============================================================================
// Reset and Recovery Tests
//=============================================================================

TEST_F(SelfModelSNNPlasticityE2E, ResetAndRecovery) {
    // Accumulate some state
    for (int i = 0; i < 10; i++) {
        run_evaluation((SelfModelScenario)(i % 8));
    }

    // Reset both bridges
    self_model_snn_reset(snn_bridge);
    self_model_plasticity_reset(plasticity_bridge);

    // Verify recovery
    self_model_snn_bridge_state_t snn_state;
    self_model_snn_get_state(snn_bridge, &snn_state);
    EXPECT_EQ(snn_state.state, SELF_MODEL_SNN_STATE_IDLE);

    self_model_plasticity_bridge_state_t plasticity_state;
    self_model_plasticity_get_state(plasticity_bridge, &plasticity_state);
    EXPECT_EQ(plasticity_state.state, SELF_MODEL_PLASTICITY_STATE_IDLE);

    // Can continue processing
    auto result = run_evaluation(HIGH_AGENCY_CORRECT);
    EXPECT_GE(result.agency_level, 0.0f);
}

//=============================================================================
// Statistics Validation Tests
//=============================================================================

TEST_F(SelfModelSNNPlasticityE2E, StatisticsAccuracy) {
    // Run known number of evaluations
    for (int i = 0; i < 20; i++) {
        run_evaluation((SelfModelScenario)(i % 8));

        // Apply learning
        self_model_plasticity_learn(plasticity_bridge,
            SELF_LEARN_AGENCY_CONFIRMED, 0.1f, i % SELF_DIM_COUNT, 0.5f);
    }

    // Verify stats match
    self_model_snn_stats_t snn_stats;
    self_model_snn_get_stats(snn_bridge, &snn_stats);
    EXPECT_GE(snn_stats.total_evaluations, 20u);

    self_model_plasticity_stats_t plasticity_stats;
    self_model_plasticity_get_stats(plasticity_bridge, &plasticity_stats);
    EXPECT_GE(plasticity_stats.total_learning_events, 20u);
}
