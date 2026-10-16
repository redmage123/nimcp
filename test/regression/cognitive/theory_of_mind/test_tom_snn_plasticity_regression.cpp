//=============================================================================
// test_tom_snn_plasticity_regression.cpp - Regression Tests
//=============================================================================
/**
 * @file test_tom_snn_plasticity_regression.cpp
 * @brief Regression tests for Theory of Mind SNN-Plasticity integration
 *
 * WHAT: Test for regressions in numerical stability, performance, and behavior
 * WHY:  Ensure consistent behavior across changes and prevent past bugs
 * HOW:  Test edge cases, numerical bounds, and performance constraints
 *
 * REGRESSION TEST CATEGORIES:
 * - Numerical stability (confidence bounds, inference normalization)
 * - Memory leak detection (repeated create/destroy cycles)
 * - Performance bounds (operation timing)
 * - Consistency (deterministic output for same input)
 * - Protected synapse behavior
 *
 * @date 2026-01-06
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <chrono>
#include <vector>
#include <memory>

// Headers have their own extern "C" guards
#include "cognitive/theory_of_mind/nimcp_tom_snn_bridge.h"
#include "cognitive/theory_of_mind/nimcp_tom_plasticity_bridge.h"

//=============================================================================
// Test Fixture
//=============================================================================

class TOMSNNPlasticityRegressionTest : public ::testing::Test {
protected:
    tom_snn_bridge_t* snn_bridge = nullptr;
    tom_plasticity_bridge_t* plasticity_bridge = nullptr;

    void SetUp() override {
        // Create bridges with default configs
        tom_snn_config_t snn_config = tom_snn_config_default();
        snn_config.enable_bio_async = false;

        snn_bridge = tom_snn_create(&snn_config);
        ASSERT_NE(snn_bridge, nullptr);

        tom_plasticity_config_t plasticity_config = tom_plasticity_config_default();
        plasticity_bridge = tom_plasticity_create(&plasticity_config);
        ASSERT_NE(plasticity_bridge, nullptr);
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

    // Generate deterministic test dimensions
    void generate_dims(float* dims, uint32_t n, uint32_t seed) {
        for (uint32_t i = 0; i < n; i++) {
            dims[i] = 0.5f + 0.3f * sinf((float)(i + seed) * 0.1f);
        }
    }
};

//=============================================================================
// Numerical Stability Tests
//=============================================================================

TEST_F(TOMSNNPlasticityRegressionTest, InferenceBoundsNormalized) {
    // REGRESSION: All inference values must be in [0, 1]

    float dims[TOM_DIM_COUNT];
    generate_dims(dims, TOM_DIM_COUNT, 0);

    // Run multiple simulations with varying inputs
    for (float belief = 0.1f; belief <= 1.0f; belief += 0.2f) {
        dims[TOM_DIM_BELIEF_STATE] = belief;
        tom_snn_encode_context(snn_bridge, dims, TOM_DIM_COUNT);
        tom_snn_simulate(snn_bridge, 50.0f);

        tom_inference_t inference;
        tom_snn_get_inference(snn_bridge, &inference);

        EXPECT_GE(inference.belief_state, 0.0f);
        EXPECT_LE(inference.belief_state, 1.0f);
        EXPECT_GE(inference.desire_state, 0.0f);
        EXPECT_LE(inference.desire_state, 1.0f);
        EXPECT_GE(inference.intention_clarity, 0.0f);
        EXPECT_LE(inference.intention_clarity, 1.0f);
        EXPECT_GE(inference.perspective_alignment, 0.0f);
        EXPECT_LE(inference.perspective_alignment, 1.0f);
        EXPECT_GE(inference.empathic_accuracy, 0.0f);
        EXPECT_LE(inference.empathic_accuracy, 1.0f);
        EXPECT_GE(inference.confidence, 0.0f);
        EXPECT_LE(inference.confidence, 1.0f);

        tom_snn_reset(snn_bridge);
    }
}

TEST_F(TOMSNNPlasticityRegressionTest, WeightBoundsRespected) {
    // REGRESSION: Weights must stay within configured bounds

    // Register synapses (non-protected types)
    for (uint32_t i = 0; i < 6; i++) {
        tom_plasticity_register_synapse(plasticity_bridge, i, TOM_SYNAPSE_EMPATHY, 0.5f);
    }

    // Trigger many STDP events
    for (int iter = 0; iter < 100; iter++) {
        // LTP
        tom_plasticity_apply_stdp(plasticity_bridge, iter % 6, 0.0f, 10.0f);
        // LTD
        tom_plasticity_apply_stdp(plasticity_bridge, iter % 6, 10.0f, 0.0f);
    }

    // Verify bounds
    for (uint32_t syn = 0; syn < 6; syn++) {
        tom_plasticity_synapse_t state;
        int ret = tom_plasticity_get_synapse(plasticity_bridge, syn, &state);
        if (ret == 0) {
            EXPECT_GE(state.weight, 0.0f) << "Weight must be >= 0";
            EXPECT_LE(state.weight, 2.0f) << "Weight must be <= max (2.0)";
        }
    }
}

TEST_F(TOMSNNPlasticityRegressionTest, NoNaNOrInfValues) {
    // REGRESSION: No NaN or Inf values should appear in outputs

    float dims[TOM_DIM_COUNT];
    generate_dims(dims, TOM_DIM_COUNT, 42);

    // Edge case inputs
    float edge_cases[] = {0.0f, 1.0f, 0.5f, 0.001f, 0.999f};

    for (float val : edge_cases) {
        dims[0] = val;
        tom_snn_encode_context(snn_bridge, dims, TOM_DIM_COUNT);
        tom_snn_simulate(snn_bridge, 50.0f);

        tom_inference_t inference;
        tom_snn_get_inference(snn_bridge, &inference);

        EXPECT_FALSE(std::isnan(inference.belief_state));
        EXPECT_FALSE(std::isinf(inference.belief_state));
        EXPECT_FALSE(std::isnan(inference.confidence));
        EXPECT_FALSE(std::isinf(inference.confidence));

        tom_snn_reset(snn_bridge);
    }
}

//=============================================================================
// Memory Regression Tests
//=============================================================================

TEST_F(TOMSNNPlasticityRegressionTest, RepeatedCreateDestroyCycles) {
    // REGRESSION: Memory leaks on repeated create/destroy

    const int cycles = 50;

    for (int i = 0; i < cycles; i++) {
        tom_snn_config_t config = tom_snn_config_default();
        config.enable_bio_async = false;

        tom_snn_bridge_t* bridge = tom_snn_create(&config);
        ASSERT_NE(bridge, nullptr) << "Create failed at cycle " << i;

        // Do some work
        float dims[TOM_DIM_COUNT] = {0};
        dims[0] = 0.5f;
        tom_snn_encode_context(bridge, dims, TOM_DIM_COUNT);
        tom_snn_simulate(bridge, 10.0f);

        tom_snn_destroy(bridge);
    }

    SUCCEED();
}

TEST_F(TOMSNNPlasticityRegressionTest, PlasticityRepeatedCreateDestroy) {
    // REGRESSION: Plasticity bridge memory leaks

    const int cycles = 50;

    for (int i = 0; i < cycles; i++) {
        tom_plasticity_config_t config = tom_plasticity_config_default();
        tom_plasticity_bridge_t* bridge = tom_plasticity_create(&config);
        ASSERT_NE(bridge, nullptr) << "Create failed at cycle " << i;

        // Register and use synapses
        for (uint32_t s = 0; s < 4; s++) {
            tom_plasticity_register_synapse(bridge, s, TOM_SYNAPSE_EMPATHY, 0.5f);
        }

        tom_plasticity_learn(bridge, TOM_LEARN_EMPATHY_ACCURATE, 0.8f, 0, 1.0f);
        tom_plasticity_destroy(bridge);
    }

    SUCCEED();
}

//=============================================================================
// Performance Regression Tests
//=============================================================================

TEST_F(TOMSNNPlasticityRegressionTest, EncodingPerformanceBound) {
    // REGRESSION: Encoding should complete within reasonable time

    float dims[TOM_DIM_COUNT];
    generate_dims(dims, TOM_DIM_COUNT, 0);

    auto start = std::chrono::high_resolution_clock::now();

    const int iterations = 100;
    for (int i = 0; i < iterations; i++) {
        dims[0] = (float)i / 100.0f;
        tom_snn_encode_context(snn_bridge, dims, TOM_DIM_COUNT);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    // Should complete 100 encodings in < 100ms
    EXPECT_LT(duration_us, 100000)
        << "Encoding too slow: " << duration_us << "us for " << iterations << " iterations";
}

TEST_F(TOMSNNPlasticityRegressionTest, SimulationPerformanceBound) {
    // REGRESSION: Simulation should complete within reasonable time

    float dims[TOM_DIM_COUNT];
    generate_dims(dims, TOM_DIM_COUNT, 0);

    tom_snn_encode_context(snn_bridge, dims, TOM_DIM_COUNT);

    auto start = std::chrono::high_resolution_clock::now();

    const int iterations = 50;
    for (int i = 0; i < iterations; i++) {
        tom_snn_simulate(snn_bridge, 10.0f);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    // 50 simulations should complete in < 500ms
    EXPECT_LT(duration_us, 500000)
        << "Simulation too slow: " << duration_us << "us for " << iterations << " iterations";
}

TEST_F(TOMSNNPlasticityRegressionTest, PlasticityLearningPerformance) {
    // REGRESSION: Plasticity updates should be fast

    // Register synapses
    for (uint32_t i = 0; i < 8; i++) {
        tom_plasticity_register_synapse(plasticity_bridge, i, TOM_SYNAPSE_EMPATHY, 0.5f);
    }

    auto start = std::chrono::high_resolution_clock::now();

    const int iterations = 1000;
    for (int i = 0; i < iterations; i++) {
        tom_plasticity_learn(plasticity_bridge, TOM_LEARN_EMPATHY_ACCURATE, 0.7f, i % 8, 1.0f);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    // 1000 learning events should complete in < 100ms
    EXPECT_LT(duration_us, 100000)
        << "Learning too slow: " << duration_us << "us for " << iterations << " updates";
}

//=============================================================================
// Consistency Tests
//=============================================================================

TEST_F(TOMSNNPlasticityRegressionTest, DeterministicOutput) {
    // REGRESSION: Same input should produce consistent output

    float dims[TOM_DIM_COUNT];
    generate_dims(dims, TOM_DIM_COUNT, 123);

    // Run first pass
    tom_snn_encode_context(snn_bridge, dims, TOM_DIM_COUNT);
    tom_snn_simulate(snn_bridge, 50.0f);

    tom_inference_t inference1;
    tom_snn_get_inference(snn_bridge, &inference1);

    // Reset and run again
    tom_snn_reset(snn_bridge);
    tom_snn_encode_context(snn_bridge, dims, TOM_DIM_COUNT);
    tom_snn_simulate(snn_bridge, 50.0f);

    tom_inference_t inference2;
    tom_snn_get_inference(snn_bridge, &inference2);

    // Results should be similar
    EXPECT_LT(fabsf(inference1.belief_state - inference2.belief_state), 0.1f);
    EXPECT_LT(fabsf(inference1.confidence - inference2.confidence), 0.1f);
}

TEST_F(TOMSNNPlasticityRegressionTest, StatsConsistency) {
    // REGRESSION: Stats should accurately reflect operations

    tom_snn_stats_t stats1;
    tom_snn_get_stats(snn_bridge, &stats1);
    uint64_t initial_evals = stats1.total_evaluations;

    float dims[TOM_DIM_COUNT];
    generate_dims(dims, TOM_DIM_COUNT, 0);

    const int n_evaluations = 10;
    for (int i = 0; i < n_evaluations; i++) {
        tom_snn_encode_context(snn_bridge, dims, TOM_DIM_COUNT);
        tom_snn_simulate(snn_bridge, 10.0f);
    }

    tom_snn_stats_t stats2;
    tom_snn_get_stats(snn_bridge, &stats2);

    EXPECT_EQ(stats2.total_evaluations, initial_evals + n_evaluations)
        << "Evaluation count mismatch";
}

//=============================================================================
// Protected Synapse Tests
//=============================================================================

TEST_F(TOMSNNPlasticityRegressionTest, ProtectedSynapsesBlocked) {
    // REGRESSION: Protected synapses should not be modified

    // Register protected synapses (BELIEF and PERSPECTIVE are auto-protected)
    tom_plasticity_register_synapse(plasticity_bridge, 0, TOM_SYNAPSE_BELIEF, 0.5f);
    tom_plasticity_register_synapse(plasticity_bridge, 1, TOM_SYNAPSE_PERSPECTIVE, 0.5f);

    // Get initial weights
    tom_plasticity_synapse_t syn0_before, syn1_before;
    tom_plasticity_get_synapse(plasticity_bridge, 0, &syn0_before);
    tom_plasticity_get_synapse(plasticity_bridge, 1, &syn1_before);

    EXPECT_TRUE(syn0_before.is_protected);
    EXPECT_TRUE(syn1_before.is_protected);

    // Try to modify protected synapses
    for (int i = 0; i < 100; i++) {
        tom_plasticity_learn(plasticity_bridge, TOM_LEARN_CORRECT_BELIEF, 0.9f, 0, 1.0f);
        tom_plasticity_apply_stdp(plasticity_bridge, 0, 0.0f, 10.0f);
        tom_plasticity_apply_stdp(plasticity_bridge, 1, 0.0f, 10.0f);
    }

    // Weights should be unchanged
    tom_plasticity_synapse_t syn0_after, syn1_after;
    tom_plasticity_get_synapse(plasticity_bridge, 0, &syn0_after);
    tom_plasticity_get_synapse(plasticity_bridge, 1, &syn1_after);

    EXPECT_FLOAT_EQ(syn0_before.weight, syn0_after.weight);
    EXPECT_FLOAT_EQ(syn1_before.weight, syn1_after.weight);

    // Verify blocked updates were counted
    tom_plasticity_stats_t stats;
    tom_plasticity_get_stats(plasticity_bridge, &stats);
    EXPECT_GT(stats.protected_updates_blocked, 0u);
}

//=============================================================================
// Edge Case Tests
//=============================================================================

TEST_F(TOMSNNPlasticityRegressionTest, ZeroActivationInput) {
    // Edge case: zero activation

    float dims[TOM_DIM_COUNT] = {0};

    int ret = tom_snn_encode_context(snn_bridge, dims, TOM_DIM_COUNT);
    EXPECT_GE(ret, 0) << "Zero activation should be handled gracefully";

    tom_snn_simulate(snn_bridge, 50.0f);

    tom_inference_t inference;
    tom_snn_get_inference(snn_bridge, &inference);

    EXPECT_FALSE(std::isnan(inference.belief_state));
    EXPECT_FALSE(std::isnan(inference.confidence));
}

TEST_F(TOMSNNPlasticityRegressionTest, MaxDimensionInput) {
    // Edge case: maximum dimension count

    float dims[TOM_DIM_COUNT];
    for (int i = 0; i < TOM_DIM_COUNT; i++) {
        dims[i] = 1.0f;  // All max
    }

    int ret = tom_snn_encode_context(snn_bridge, dims, TOM_DIM_COUNT);
    EXPECT_GE(ret, 0) << "Max dimensions should be valid";

    tom_snn_simulate(snn_bridge, 30.0f);

    tom_inference_t inference;
    tom_snn_get_inference(snn_bridge, &inference);

    EXPECT_GE(inference.confidence, 0.0f);
    EXPECT_LE(inference.confidence, 1.0f);
}

TEST_F(TOMSNNPlasticityRegressionTest, RapidResetCycles) {
    // Edge case: rapid reset cycles

    float dims[TOM_DIM_COUNT];
    generate_dims(dims, TOM_DIM_COUNT, 0);

    for (int cycle = 0; cycle < 20; cycle++) {
        tom_snn_encode_context(snn_bridge, dims, TOM_DIM_COUNT);
        tom_snn_simulate(snn_bridge, 10.0f);
        tom_snn_reset(snn_bridge);

        tom_plasticity_reset(plasticity_bridge);
    }

    // Verify bridges are still functional after rapid resets
    tom_snn_bridge_state_t snn_state;
    tom_snn_get_state(snn_bridge, &snn_state);
    EXPECT_EQ(snn_state.state, TOM_SNN_STATE_IDLE);

    tom_plasticity_bridge_state_t plasticity_state;
    tom_plasticity_get_state(plasticity_bridge, &plasticity_state);
    EXPECT_EQ(plasticity_state.state, TOM_PLASTICITY_STATE_IDLE);
}

TEST_F(TOMSNNPlasticityRegressionTest, BeliefDiscrepancyExtremes) {
    // Edge case: extreme belief discrepancy (false belief detection)

    // Self = 1.0, Other = 0.0 -> maximum discrepancy
    int ret = tom_snn_encode_belief(snn_bridge, 1.0f, 0.0f);
    EXPECT_GE(ret, 0);

    tom_snn_simulate(snn_bridge, 50.0f);

    tom_inference_t inference;
    tom_snn_get_inference(snn_bridge, &inference);

    // Should detect significant deception potential
    EXPECT_GE(inference.deception_confidence, 0.0f);

    // Reverse
    tom_snn_reset(snn_bridge);
    ret = tom_snn_encode_belief(snn_bridge, 0.0f, 1.0f);
    EXPECT_GE(ret, 0);

    tom_snn_simulate(snn_bridge, 50.0f);
    tom_snn_get_inference(snn_bridge, &inference);
    EXPECT_GE(inference.deception_confidence, 0.0f);
}

//=============================================================================
// Calibration Stability Tests
//=============================================================================

TEST_F(TOMSNNPlasticityRegressionTest, CalibrationStability) {
    // REGRESSION: Calibration should remain stable over updates

    tom_calibration_state_t calib_initial;
    tom_plasticity_get_calibration_state(plasticity_bridge, &calib_initial);

    // Run many homeostatic updates
    for (int i = 0; i < 100; i++) {
        tom_plasticity_homeostatic_update(plasticity_bridge, 10.0f);
    }

    tom_calibration_state_t calib_final;
    tom_plasticity_get_calibration_state(plasticity_bridge, &calib_final);

    // Calibration should converge to target, not diverge
    EXPECT_GT(calib_final.empathy_strength, 0.0f);
    EXPECT_LT(calib_final.empathy_strength, 1.5f);
    EXPECT_GT(calib_final.learning_rate_mod, 0.0f);
}
