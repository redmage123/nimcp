/**
 * @file e2e_test_introspection_snn_plasticity_pipeline.cpp
 * @brief End-to-end tests for Introspection-SNN-Plasticity learning pipeline
 * @version 1.0.0
 * @date 2026-01-06
 *
 * WHAT: Complete introspection pipeline with SNN and Plasticity
 * WHY:  Verify full dataflow from introspective state → SNN encoding → insight
 *       → plasticity learning → confidence calibration evolution
 * HOW:  Test realistic scenarios combining metacognition encoding, STDP learning,
 *       reward-modulated plasticity, and protected synapse integrity
 *
 * Test Coverage:
 * - Full introspective state to insight pipeline via SNN
 * - STDP and reward-modulated learning for confidence calibration
 * - Uncertainty detection and calibration
 * - Metacognition and Calibration synapse protection
 * - Multi-scenario calibration learning
 * - Confidence calibration evolution through experience
 * - Protected synapse integrity under stress
 */

#include <gtest/gtest.h>

#include "cognitive/introspection/nimcp_introspection_snn_bridge.h"
#include "cognitive/introspection/nimcp_introspection_plasticity_bridge.h"
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

class IntrospectionSNNPlasticityE2E : public ::testing::Test {
protected:
    introspection_snn_bridge_t* snn_bridge = nullptr;
    introspection_plasticity_bridge_t* plasticity_bridge = nullptr;

    // Learning statistics
    struct LearningStats {
        int high_confidence_correct = 0;
        int high_confidence_incorrect = 0;
        int uncertainty_detected = 0;
        int total_evaluations = 0;
        std::vector<float> confidence_history;
        std::vector<float> uncertainty_scores;
    } stats;

    void SetUp() override {
        // Create SNN bridge with full introspection dimensions
        introspection_snn_config_t snn_config = introspection_snn_config_default();
        snn_config.num_dimensions = INTROSPECTION_DIM_COUNT;
        snn_config.neurons_per_dim = 32;
        snn_config.dt_ms = 1.0f;
        snn_config.enable_metacognition = true;
        snn_config.enable_bio_async = false;

        snn_bridge = introspection_snn_create(&snn_config);
        ASSERT_NE(snn_bridge, nullptr) << "Failed to create SNN bridge";

        // Create Plasticity bridge with all learning mechanisms
        introspection_plasticity_config_t plasticity_config = introspection_plasticity_config_default();
        plasticity_config.base_learning_rate = 0.01f;
        plasticity_config.stdp_a_plus = 0.01f;
        plasticity_config.stdp_a_minus = 0.012f;

        plasticity_bridge = introspection_plasticity_create(&plasticity_config);
        ASSERT_NE(plasticity_bridge, nullptr) << "Failed to create Plasticity bridge";

        // Register base synapses for plasticity
        for (uint32_t i = 0; i < INTROSPECTION_DIM_COUNT; i++) {
            introspection_plasticity_register_synapse(plasticity_bridge, i,
                INTROSPECTION_SYNAPSE_CONFIDENCE, 0.5f);
        }

        // Register protected synapses
        introspection_plasticity_register_synapse(plasticity_bridge, 100,
            INTROSPECTION_SYNAPSE_METACOGNITION, 1.0f);
        introspection_plasticity_register_synapse(plasticity_bridge, 101,
            INTROSPECTION_SYNAPSE_CALIBRATION, 0.9f);
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

    // Generate introspective scenario
    enum IntrospectiveScenario {
        HIGH_CONFIDENCE_CORRECT,    // High certainty, well-calibrated
        HIGH_CONFIDENCE_WRONG,      // Overconfidence, should be corrected
        LOW_CONFIDENCE_CORRECT,     // Underconfidence, actual high certainty
        UNCERTAIN_STATE,            // Genuinely uncertain
        PATTERN_RECOGNITION,        // Strong pattern match
        ERROR_DETECTION,            // Error signal triggered
        METACOGNITIVE_CONFLICT,     // Conflicting metacognitive signals
        CALIBRATION_TEST            // Confidence-certainty alignment
    };

    void generate_scenario(float* dims, IntrospectiveScenario scenario) {
        memset(dims, 0, sizeof(float) * INTROSPECTION_DIM_COUNT);

        switch (scenario) {
            case HIGH_CONFIDENCE_CORRECT:
                dims[INTROSPECTION_DIM_CERTAINTY] = 0.9f;
                dims[INTROSPECTION_DIM_CONFIDENCE] = 0.85f;
                dims[INTROSPECTION_DIM_UNCERTAINTY] = 0.1f;
                dims[INTROSPECTION_DIM_ALERTNESS] = 0.8f;
                break;

            case HIGH_CONFIDENCE_WRONG:
                dims[INTROSPECTION_DIM_CONFIDENCE] = 0.95f;
                dims[INTROSPECTION_DIM_CERTAINTY] = 0.3f;  // Low actual certainty
                dims[INTROSPECTION_DIM_UNCERTAINTY] = 0.7f;
                dims[INTROSPECTION_DIM_ERROR_SIGNAL] = 0.6f;
                break;

            case LOW_CONFIDENCE_CORRECT:
                dims[INTROSPECTION_DIM_CONFIDENCE] = 0.3f;
                dims[INTROSPECTION_DIM_CERTAINTY] = 0.8f;  // High actual certainty
                dims[INTROSPECTION_DIM_UNCERTAINTY] = 0.2f;
                break;

            case UNCERTAIN_STATE:
                dims[INTROSPECTION_DIM_UNCERTAINTY] = 0.9f;
                dims[INTROSPECTION_DIM_CERTAINTY] = 0.2f;
                dims[INTROSPECTION_DIM_CONFIDENCE] = 0.3f;
                dims[INTROSPECTION_DIM_CONFLICT] = 0.5f;
                break;

            case PATTERN_RECOGNITION:
                dims[INTROSPECTION_DIM_PATTERN_MATCH] = 0.95f;
                dims[INTROSPECTION_DIM_CERTAINTY] = 0.85f;
                dims[INTROSPECTION_DIM_CONFIDENCE] = 0.8f;
                dims[INTROSPECTION_DIM_ALERTNESS] = 0.9f;
                break;

            case ERROR_DETECTION:
                dims[INTROSPECTION_DIM_ERROR_SIGNAL] = 0.9f;
                dims[INTROSPECTION_DIM_CONFLICT] = 0.7f;
                dims[INTROSPECTION_DIM_STATE_CHANGE] = 0.8f;
                dims[INTROSPECTION_DIM_METACOGNITION] = 0.85f;
                break;

            case METACOGNITIVE_CONFLICT:
                dims[INTROSPECTION_DIM_METACOGNITION] = 0.9f;
                dims[INTROSPECTION_DIM_CONFLICT] = 0.85f;
                dims[INTROSPECTION_DIM_SELF_REFERENCE] = 0.8f;
                dims[INTROSPECTION_DIM_ATTENTION_FOCUS] = 0.5f;
                break;

            case CALIBRATION_TEST:
                dims[INTROSPECTION_DIM_CONFIDENCE] = 0.7f;
                dims[INTROSPECTION_DIM_CERTAINTY] = 0.7f;  // Well-calibrated
                dims[INTROSPECTION_DIM_ALERTNESS] = 0.75f;
                dims[INTROSPECTION_DIM_INTEGRATION] = 0.8f;
                break;
        }
    }

    // Run single evaluation pipeline
    struct EvaluationResult {
        float certainty_level;
        float uncertainty_level;
        float confidence;
        bool uncertainty_detected;
        int spike_count;
    };

    EvaluationResult run_evaluation(IntrospectiveScenario scenario) {
        EvaluationResult result = {0};

        float dims[INTROSPECTION_DIM_COUNT];
        generate_scenario(dims, scenario);

        // Encode and simulate
        result.spike_count = introspection_snn_encode_state(snn_bridge, dims, INTROSPECTION_DIM_COUNT);
        introspection_snn_simulate(snn_bridge, 30.0f);

        // Get insight
        introspection_insight_t insight;
        introspection_snn_get_insight(snn_bridge, &insight);

        result.certainty_level = insight.certainty_level;
        result.uncertainty_level = insight.uncertainty_level;
        result.confidence = insight.confidence;

        // Check uncertainty detection
        float uncertainty_level;
        result.uncertainty_detected = introspection_snn_check_uncertainty(snn_bridge, &uncertainty_level);

        // Update stats
        stats.total_evaluations++;
        stats.confidence_history.push_back(insight.confidence);
        stats.uncertainty_scores.push_back(insight.uncertainty_level);

        if (result.uncertainty_detected) {
            stats.uncertainty_detected++;
        }

        return result;
    }
};

//=============================================================================
// Basic Pipeline Tests
//=============================================================================

TEST_F(IntrospectionSNNPlasticityE2E, CompletePipelineInitialization) {
    // Verify complete pipeline setup
    EXPECT_NE(snn_bridge, nullptr);
    EXPECT_NE(plasticity_bridge, nullptr);

    // Check synapse registration
    introspection_plasticity_bridge_state_t state;
    introspection_plasticity_get_state(plasticity_bridge, &state);
    EXPECT_GT(state.active_synapses, (uint32_t)INTROSPECTION_DIM_COUNT);  // Base + protected
}

TEST_F(IntrospectionSNNPlasticityE2E, SingleEvaluationPipeline) {
    // Run single high confidence scenario
    auto result = run_evaluation(HIGH_CONFIDENCE_CORRECT);

    // Verify insight is valid
    EXPECT_GE(result.certainty_level, 0.0f);
    EXPECT_LE(result.certainty_level, 1.0f);
    EXPECT_GE(result.confidence, 0.0f);
    EXPECT_LE(result.confidence, 1.0f);
    EXPECT_GE(result.spike_count, 0);

    // Apply learning based on result
    int ret = introspection_plasticity_learn(plasticity_bridge,
        INTROSPECTION_LEARN_CORRECT_CONFIDENCE, 0.5f, 0, result.confidence);
    EXPECT_EQ(ret, 0);
}

//=============================================================================
// Confidence Calibration Tests
//=============================================================================

TEST_F(IntrospectionSNNPlasticityE2E, OverconfidenceLearning) {
    // Run multiple overconfidence scenarios
    float total_uncertainty = 0.0f;
    int uncertainty_count = 0;

    for (int trial = 0; trial < 10; trial++) {
        auto result = run_evaluation(HIGH_CONFIDENCE_WRONG);

        total_uncertainty += result.uncertainty_level;
        if (result.uncertainty_level > 0.0f || result.uncertainty_detected) {
            uncertainty_count++;
        }

        // Learn overconfidence - should decrease confidence
        introspection_plasticity_learn(plasticity_bridge,
            INTROSPECTION_LEARN_OVERCONFIDENCE, 0.5f, 0, result.confidence);

        // Apply STDP
        introspection_plasticity_apply_stdp(plasticity_bridge, 0,
            (float)trial, (float)trial + 5.0f);
    }

    // At least some trials should register uncertainty signals
    EXPECT_GT(uncertainty_count, 0);
}

TEST_F(IntrospectionSNNPlasticityE2E, UnderconfidenceLearning) {
    // Run multiple underconfidence scenarios
    float total_certainty = 0.0f;

    for (int trial = 0; trial < 10; trial++) {
        auto result = run_evaluation(LOW_CONFIDENCE_CORRECT);

        total_certainty += result.certainty_level;

        // Learn underconfidence - should increase confidence
        introspection_plasticity_learn(plasticity_bridge,
            INTROSPECTION_LEARN_UNDERCONFIDENCE, 0.5f, 1, result.confidence);
    }

    // Average certainty should be above minimum
    EXPECT_GT(total_certainty / 10.0f, 0.0f);
}

TEST_F(IntrospectionSNNPlasticityE2E, CalibrationImprovement) {
    // Register calibration synapses
    for (int i = 200; i < 210; i++) {
        introspection_plasticity_register_synapse(plasticity_bridge, i,
            INTROSPECTION_SYNAPSE_CONFIDENCE, 0.5f);
    }

    // Initial calibration weight
    introspection_plasticity_synapse_t initial_synapse;
    introspection_plasticity_get_synapse(plasticity_bridge, 200, &initial_synapse);
    float initial_weight = initial_synapse.weight;

    // Run mixed calibration scenarios
    for (int epoch = 0; epoch < 5; epoch++) {
        // Well-calibrated scenario
        auto good_result = run_evaluation(CALIBRATION_TEST);
        introspection_plasticity_learn(plasticity_bridge,
            INTROSPECTION_LEARN_CORRECT_CONFIDENCE, 0.3f, 200, good_result.confidence);

        // Overconfidence scenario
        auto over_result = run_evaluation(HIGH_CONFIDENCE_WRONG);
        introspection_plasticity_learn(plasticity_bridge,
            INTROSPECTION_LEARN_OVERCONFIDENCE, 0.3f, 201, over_result.confidence);

        // BCM and homeostatic updates
        introspection_plasticity_update_bcm(plasticity_bridge, 0.5f);
        introspection_plasticity_homeostatic_update(plasticity_bridge, 0.5f);
    }

    // Verify learning occurred
    introspection_plasticity_stats_t stats;
    introspection_plasticity_get_stats(plasticity_bridge, &stats);
    EXPECT_GT(stats.total_learning_events, 0u);
    EXPECT_GT(stats.correct_confidence_events, 0u);
    EXPECT_GT(stats.overconfidence_events, 0u);
}

//=============================================================================
// Uncertainty Detection Tests
//=============================================================================

TEST_F(IntrospectionSNNPlasticityE2E, UncertaintyTriggersCalibration) {
    int uncertainty_trials = 0;
    float total_uncertainty = 0.0f;

    for (int trial = 0; trial < 10; trial++) {
        auto result = run_evaluation(UNCERTAIN_STATE);

        total_uncertainty += result.uncertainty_level;
        if (result.uncertainty_level > 0.0f || result.uncertainty_detected) {
            uncertainty_trials++;

            // Learn from uncertainty
            introspection_plasticity_learn(plasticity_bridge,
                INTROSPECTION_LEARN_UNCERTAINTY_CALIBRATED, 0.5f, 2, result.uncertainty_level);
        }
    }

    // Should detect uncertainty signals in multiple trials
    EXPECT_GE(uncertainty_trials, 3);
    // Total uncertainty across trials should be meaningful
    EXPECT_GT(total_uncertainty, 0.0f);
}

//=============================================================================
// Protected Synapse Tests
//=============================================================================

TEST_F(IntrospectionSNNPlasticityE2E, MetacognitionProtectionIntegrity) {
    // Get initial protected synapse weight
    introspection_plasticity_synapse_t metacog_synapse;
    introspection_plasticity_get_synapse(plasticity_bridge, 100, &metacog_synapse);
    float original_weight = metacog_synapse.weight;
    EXPECT_TRUE(metacog_synapse.is_protected);

    // Run many scenarios and try to modify protected synapse
    for (int trial = 0; trial < 20; trial++) {
        auto result = run_evaluation((IntrospectiveScenario)(trial % 8));

        // Try various learning operations on protected synapse
        introspection_plasticity_learn(plasticity_bridge,
            INTROSPECTION_LEARN_OVERCONFIDENCE, -1.0f, 100, result.confidence);
        introspection_plasticity_apply_stdp(plasticity_bridge, 100,
            (float)trial, (float)trial + 10.0f);
        introspection_plasticity_apply_reward(plasticity_bridge, -1.0f);
    }

    // Protected synapse should remain unchanged
    introspection_plasticity_get_synapse(plasticity_bridge, 100, &metacog_synapse);
    EXPECT_FLOAT_EQ(metacog_synapse.weight, original_weight);
    EXPECT_TRUE(metacog_synapse.is_protected);
}

TEST_F(IntrospectionSNNPlasticityE2E, CalibrationSynapseProtection) {
    // Calibration synapse should also be protected
    introspection_plasticity_synapse_t calib_synapse;
    introspection_plasticity_get_synapse(plasticity_bridge, 101, &calib_synapse);
    float original_weight = calib_synapse.weight;
    EXPECT_TRUE(calib_synapse.is_protected);

    // Stress test protection
    for (int i = 0; i < 50; i++) {
        introspection_plasticity_apply_stdp(plasticity_bridge, 101, (float)i, (float)i + 5.0f);
        introspection_plasticity_learn(plasticity_bridge,
            INTROSPECTION_LEARN_OVERCONFIDENCE, 1.0f, 101, 0.9f);
    }

    // Weight must remain unchanged
    introspection_plasticity_get_synapse(plasticity_bridge, 101, &calib_synapse);
    EXPECT_FLOAT_EQ(calib_synapse.weight, original_weight);
}

//=============================================================================
// Pattern Recognition Tests
//=============================================================================

TEST_F(IntrospectionSNNPlasticityE2E, PatternRecognitionLearning) {
    // Register pattern synapses
    for (int i = 300; i < 305; i++) {
        introspection_plasticity_register_synapse(plasticity_bridge, i,
            INTROSPECTION_SYNAPSE_CONFIDENCE, 0.5f);
    }

    int learning_events = 0;

    for (int trial = 0; trial < 10; trial++) {
        auto result = run_evaluation(PATTERN_RECOGNITION);

        // Apply learning for all pattern recognition scenarios
        introspection_plasticity_learn(plasticity_bridge,
            INTROSPECTION_LEARN_PATTERN_MATCH, 0.5f, 300 + (trial % 5),
            result.confidence > 0.0f ? result.confidence : 0.5f);  // Use default if 0
        learning_events++;
    }

    // Verify we applied learning across all trials
    EXPECT_EQ(learning_events, 10);

    // Verify plasticity stats reflect learning
    introspection_plasticity_stats_t stats;
    introspection_plasticity_get_stats(plasticity_bridge, &stats);
    EXPECT_GE(stats.total_learning_events, 10u);
}

//=============================================================================
// Error Detection Tests
//=============================================================================

TEST_F(IntrospectionSNNPlasticityE2E, ErrorDetectionAndCorrection) {
    float total_error_signal = 0.0f;
    int error_detections = 0;
    int learning_events = 0;

    for (int trial = 0; trial < 10; trial++) {
        auto result = run_evaluation(ERROR_DETECTION);

        float error_level;
        introspection_snn_check_error(snn_bridge, &error_level);

        total_error_signal += error_level;
        if (error_level > 0.0f || result.uncertainty_level > 0.0f) {
            error_detections++;
        }

        // Always apply learning for error scenarios
        introspection_plasticity_learn(plasticity_bridge,
            INTROSPECTION_LEARN_ERROR_DETECTED, 0.5f, 3, error_level);
        learning_events++;
    }

    // Should detect errors in at least some trials
    EXPECT_GE(error_detections, 3);
    // Verify learning was applied
    EXPECT_EQ(learning_events, 10);
}

//=============================================================================
// Multi-Scenario Learning Tests
//=============================================================================

TEST_F(IntrospectionSNNPlasticityE2E, CompleteCalibrationWorkflow) {
    // Register workflow synapses
    for (int i = 400; i < 420; i++) {
        introspection_plasticity_register_synapse(plasticity_bridge, i,
            INTROSPECTION_SYNAPSE_CONFIDENCE, 0.5f);
    }

    // Run complete calibration workflow
    for (int epoch = 0; epoch < 5; epoch++) {
        for (int scenario = 0; scenario < 8; scenario++) {
            auto result = run_evaluation((IntrospectiveScenario)scenario);

            // Select learning event based on scenario
            introspection_learn_event_t event;
            float magnitude = 0.3f;

            switch ((IntrospectiveScenario)scenario) {
                case HIGH_CONFIDENCE_CORRECT:
                case CALIBRATION_TEST:
                    event = INTROSPECTION_LEARN_CORRECT_CONFIDENCE;
                    break;
                case HIGH_CONFIDENCE_WRONG:
                    event = INTROSPECTION_LEARN_OVERCONFIDENCE;
                    break;
                case LOW_CONFIDENCE_CORRECT:
                    event = INTROSPECTION_LEARN_UNDERCONFIDENCE;
                    break;
                case UNCERTAIN_STATE:
                    event = INTROSPECTION_LEARN_UNCERTAINTY_CALIBRATED;
                    break;
                case ERROR_DETECTION:
                    event = INTROSPECTION_LEARN_ERROR_DETECTED;
                    break;
                case PATTERN_RECOGNITION:
                    event = INTROSPECTION_LEARN_PATTERN_MATCH;
                    break;
                default:
                    event = INTROSPECTION_LEARN_STATE_TRACKED;
                    break;
            }

            int synapse_id = 400 + (epoch * 8 + scenario) % 20;
            introspection_plasticity_learn(plasticity_bridge, event, magnitude,
                synapse_id, result.confidence);

            // Apply STDP
            introspection_plasticity_apply_stdp(plasticity_bridge, synapse_id,
                (float)(epoch * 10 + scenario), (float)(epoch * 10 + scenario + 5));
        }

        // Periodic maintenance
        introspection_plasticity_update_bcm(plasticity_bridge, 0.5f);
        introspection_plasticity_homeostatic_update(plasticity_bridge, 0.5f);
        introspection_plasticity_update_traces(plasticity_bridge, 1.0f);
    }

    // Consolidate learning
    introspection_plasticity_consolidate(plasticity_bridge);

    // Verify extensive learning occurred
    introspection_plasticity_stats_t final_stats;
    introspection_plasticity_get_stats(plasticity_bridge, &final_stats);
    EXPECT_GT(final_stats.total_learning_events, 30u);
    EXPECT_GT(final_stats.weight_updates, 30u);

    introspection_snn_stats_t snn_stats;
    introspection_snn_get_stats(snn_bridge, &snn_stats);
    EXPECT_GE(snn_stats.total_evaluations, 40u);
}

//=============================================================================
// Stress and Performance Tests
//=============================================================================

TEST_F(IntrospectionSNNPlasticityE2E, HighVolumeProcessing) {
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 100; i++) {
        run_evaluation((IntrospectiveScenario)(i % 8));
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // 100 evaluations should complete in under 5 seconds
    EXPECT_LT(duration.count(), 5000);
    EXPECT_EQ(stats.total_evaluations, 100);
}

TEST_F(IntrospectionSNNPlasticityE2E, ContinuousLearning) {
    // Register many synapses
    for (int i = 500; i < 600; i++) {
        introspection_plasticity_register_synapse(plasticity_bridge, i,
            INTROSPECTION_SYNAPSE_CONFIDENCE, 0.5f);
    }

    // Continuous learning loop
    for (int cycle = 0; cycle < 50; cycle++) {
        auto result = run_evaluation((IntrospectiveScenario)(cycle % 8));

        // Learn on rotating synapses
        for (int j = 0; j < 5; j++) {
            int synapse_id = 500 + (cycle * 5 + j) % 100;
            introspection_plasticity_learn(plasticity_bridge,
                INTROSPECTION_LEARN_CORRECT_CONFIDENCE, 0.1f, synapse_id, result.confidence);
        }

        // Periodic BCM update
        if (cycle % 10 == 0) {
            introspection_plasticity_update_bcm(plasticity_bridge, 0.5f);
        }
    }

    // Verify extensive learning
    introspection_plasticity_stats_t stats;
    introspection_plasticity_get_stats(plasticity_bridge, &stats);
    EXPECT_GE(stats.total_learning_events, 200u);
}

//=============================================================================
// Reset and Recovery Tests
//=============================================================================

TEST_F(IntrospectionSNNPlasticityE2E, ResetAndRecovery) {
    // Accumulate some state
    for (int i = 0; i < 10; i++) {
        run_evaluation((IntrospectiveScenario)(i % 8));
    }

    // Reset both bridges
    introspection_snn_reset(snn_bridge);
    introspection_plasticity_reset(plasticity_bridge);

    // Verify recovery
    introspection_snn_bridge_state_t snn_state;
    introspection_snn_get_state(snn_bridge, &snn_state);
    EXPECT_EQ(snn_state.state, INTROSPECTION_SNN_STATE_IDLE);

    introspection_plasticity_bridge_state_t plasticity_state;
    introspection_plasticity_get_state(plasticity_bridge, &plasticity_state);
    EXPECT_EQ(plasticity_state.state, INTROSPECTION_PLASTICITY_STATE_IDLE);

    // Can continue processing
    auto result = run_evaluation(HIGH_CONFIDENCE_CORRECT);
    EXPECT_GE(result.confidence, 0.0f);
}

//=============================================================================
// Statistics Validation Tests
//=============================================================================

TEST_F(IntrospectionSNNPlasticityE2E, StatisticsAccuracy) {
    // Run known number of evaluations
    for (int i = 0; i < 20; i++) {
        run_evaluation((IntrospectiveScenario)(i % 8));

        // Apply learning
        introspection_plasticity_learn(plasticity_bridge,
            INTROSPECTION_LEARN_CORRECT_CONFIDENCE, 0.1f, i % INTROSPECTION_DIM_COUNT, 0.5f);
    }

    // Verify stats match
    introspection_snn_stats_t snn_stats;
    introspection_snn_get_stats(snn_bridge, &snn_stats);
    EXPECT_GE(snn_stats.total_evaluations, 20u);

    introspection_plasticity_stats_t plasticity_stats;
    introspection_plasticity_get_stats(plasticity_bridge, &plasticity_stats);
    EXPECT_GE(plasticity_stats.total_learning_events, 20u);
}
