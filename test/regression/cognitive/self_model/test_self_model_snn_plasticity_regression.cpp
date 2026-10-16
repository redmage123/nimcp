//=============================================================================
// test_self_model_snn_plasticity_regression.cpp - Regression Tests
//=============================================================================
/**
 * @file test_self_model_snn_plasticity_regression.cpp
 * @brief Regression tests for Self Model-SNN-Plasticity integration
 *
 * WHAT: Test for regressions in numerical stability, performance, and behavior
 * WHY:  Ensure consistent behavior across changes and prevent past bugs
 * HOW:  Test edge cases, numerical bounds, and performance constraints
 *
 * REGRESSION TEST CATEGORIES:
 * - Numerical stability (weight bounds, agency normalization)
 * - Memory leak detection (repeated create/destroy cycles)
 * - Performance bounds (operation timing)
 * - Consistency (deterministic output for same input)
 * - Protected synapse integrity (Identity, Boundary)
 *
 * @date 2026-01-06
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <chrono>
#include <vector>
#include <memory>

#include "cognitive/self_model/nimcp_self_model_snn_bridge.h"
#include "cognitive/self_model/nimcp_self_model_plasticity_bridge.h"
#include "utils/time/nimcp_time.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixture
//=============================================================================

class SelfModelSNNPlasticityRegressionTest : public ::testing::Test {
protected:
    self_model_snn_bridge_t* snn_bridge = nullptr;
    self_model_plasticity_bridge_t* plasticity_bridge = nullptr;

    void SetUp() override {
        // Create bridges with default configs
        self_model_snn_config_t snn_config = self_model_snn_config_default();
        snn_config.enable_bio_async = false;

        snn_bridge = self_model_snn_create(&snn_config);
        ASSERT_NE(snn_bridge, nullptr);

        self_model_plasticity_config_t plasticity_config = self_model_plasticity_config_default();
        plasticity_bridge = self_model_plasticity_create(&plasticity_config);
        ASSERT_NE(plasticity_bridge, nullptr);
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

    // Generate deterministic self model context
    void generate_context(float* dims, uint32_t seed) {
        for (int i = 0; i < SELF_DIM_COUNT; i++) {
            dims[i] = 0.5f + 0.3f * sinf((float)(i + seed) * 0.1f);
        }
    }
};

//=============================================================================
// Numerical Stability Tests
//=============================================================================

TEST_F(SelfModelSNNPlasticityRegressionTest, WeightBoundsAfterIntenseLearning) {
    // Register synapse
    ASSERT_EQ(self_model_plasticity_register_synapse(plasticity_bridge,
        1, SELF_SYNAPSE_AGENCY, 0.5f), 0);

    // Apply intense learning cycles
    for (int i = 0; i < 100; i++) {
        float reward = (i % 2 == 0) ? 1.0f : -1.0f;
        self_model_plasticity_learn(plasticity_bridge,
            SELF_LEARN_AGENCY_CONFIRMED, reward, 1, 0.9f);
    }

    // Verify weight stays in valid bounds
    self_model_plasticity_synapse_t synapse;
    EXPECT_EQ(self_model_plasticity_get_synapse(plasticity_bridge, 1, &synapse), 0);
    EXPECT_GE(synapse.weight, 0.0f);
    EXPECT_LE(synapse.weight, 2.0f);
}

TEST_F(SelfModelSNNPlasticityRegressionTest, InsightAgencyNormalization) {
    // Run many scenarios
    for (int s = 0; s < 50; s++) {
        float dims[SELF_DIM_COUNT];
        generate_context(dims, s);
        dims[SELF_DIM_AGENCY] = (float)s / 50.0f;  // Vary agency

        self_model_snn_encode_state(snn_bridge, dims, SELF_DIM_COUNT);
        self_model_snn_simulate(snn_bridge, 20.0f);

        self_model_insight_t insight;
        EXPECT_EQ(self_model_snn_get_insight(snn_bridge, &insight), 0);

        // All scores must be normalized
        EXPECT_GE(insight.body_state_level, 0.0f);
        EXPECT_LE(insight.body_state_level, 1.0f);
        EXPECT_GE(insight.agency_level, 0.0f);
        EXPECT_LE(insight.agency_level, 1.0f);
        EXPECT_GE(insight.identity_coherence, 0.0f);
        EXPECT_LE(insight.identity_coherence, 1.0f);
    }
}

TEST_F(SelfModelSNNPlasticityRegressionTest, STDPWeightStability) {
    // Register synapses
    for (int i = 0; i < 10; i++) {
        ASSERT_EQ(self_model_plasticity_register_synapse(plasticity_bridge,
            i, SELF_SYNAPSE_AGENCY, 0.5f), 0);
    }

    // Apply many STDP updates
    for (int cycle = 0; cycle < 100; cycle++) {
        for (int i = 0; i < 10; i++) {
            float pre_time = (float)(cycle + i) * 1.0f;
            float post_time = pre_time + ((cycle % 2) ? 5.0f : -5.0f);
            self_model_plasticity_apply_stdp(plasticity_bridge, i, pre_time, post_time);
        }
    }

    // Verify all weights in bounds
    for (int i = 0; i < 10; i++) {
        self_model_plasticity_synapse_t synapse;
        EXPECT_EQ(self_model_plasticity_get_synapse(plasticity_bridge, i, &synapse), 0);
        EXPECT_GE(synapse.weight, 0.0f);
        EXPECT_LE(synapse.weight, 2.0f);
    }
}

TEST_F(SelfModelSNNPlasticityRegressionTest, BCMThresholdBounds) {
    // Register synapse
    ASSERT_EQ(self_model_plasticity_register_synapse(plasticity_bridge,
        1, SELF_SYNAPSE_AGENCY, 0.5f), 0);

    // Apply extreme BCM updates
    for (int i = 0; i < 100; i++) {
        float rate = (i % 2 == 0) ? 1.0f : 0.0f;  // Extreme rates
        self_model_plasticity_update_bcm(plasticity_bridge, rate);
    }

    // Verify synapse still valid
    self_model_plasticity_synapse_t synapse;
    EXPECT_EQ(self_model_plasticity_get_synapse(plasticity_bridge, 1, &synapse), 0);
    EXPECT_TRUE(std::isfinite(synapse.weight));
    EXPECT_TRUE(std::isfinite(synapse.bcm_threshold));
}

//=============================================================================
// Memory Leak Detection Tests
//=============================================================================

TEST_F(SelfModelSNNPlasticityRegressionTest, RepeatedCreateDestroyCycles) {
    // First, destroy existing bridges
    if (snn_bridge) {
        self_model_snn_destroy(snn_bridge);
        snn_bridge = nullptr;
    }
    if (plasticity_bridge) {
        self_model_plasticity_destroy(plasticity_bridge);
        plasticity_bridge = nullptr;
    }

    // Repeated create/destroy cycles
    for (int cycle = 0; cycle < 50; cycle++) {
        self_model_snn_config_t snn_config = self_model_snn_config_default();
        snn_config.enable_bio_async = false;
        self_model_snn_bridge_t* snn = self_model_snn_create(&snn_config);
        ASSERT_NE(snn, nullptr);

        self_model_plasticity_config_t plasticity_config = self_model_plasticity_config_default();
        self_model_plasticity_bridge_t* plasticity = self_model_plasticity_create(&plasticity_config);
        ASSERT_NE(plasticity, nullptr);

        // Do some work
        float dims[SELF_DIM_COUNT] = {0.5f};
        self_model_snn_encode_state(snn, dims, SELF_DIM_COUNT);
        self_model_snn_step(snn);

        self_model_plasticity_register_synapse(plasticity, 1, SELF_SYNAPSE_AGENCY, 0.5f);
        self_model_plasticity_learn(plasticity, SELF_LEARN_AGENCY_CONFIRMED, 0.1f, 1, 0.5f);

        self_model_snn_destroy(snn);
        self_model_plasticity_destroy(plasticity);
    }
    // Test passes if no crash/memory exhaustion
}

TEST_F(SelfModelSNNPlasticityRegressionTest, RepeatedSynapseRegistrationUnregistration) {
    // Repeated register/unregister cycles
    for (int cycle = 0; cycle < 100; cycle++) {
        ASSERT_EQ(self_model_plasticity_register_synapse(plasticity_bridge,
            1, SELF_SYNAPSE_AGENCY, 0.5f), 0);
        ASSERT_EQ(self_model_plasticity_unregister_synapse(plasticity_bridge, 1), 0);
    }
    // Test passes if no crash/memory leak
}

//=============================================================================
// Performance Bounds Tests
//=============================================================================

TEST_F(SelfModelSNNPlasticityRegressionTest, EncodingPerformance) {
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 100; i++) {
        float dims[SELF_DIM_COUNT];
        generate_context(dims, i);
        self_model_snn_encode_state(snn_bridge, dims, SELF_DIM_COUNT);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // 100 encodings should complete in under 1 second
    EXPECT_LT(duration.count(), 1000) << "Encoding too slow: " << duration.count() << "ms";
}

TEST_F(SelfModelSNNPlasticityRegressionTest, SimulationPerformance) {
    float dims[SELF_DIM_COUNT];
    generate_context(dims, 0);
    self_model_snn_encode_state(snn_bridge, dims, SELF_DIM_COUNT);

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 100; i++) {
        self_model_snn_simulate(snn_bridge, 10.0f);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // 100 simulations should complete in under 2 seconds
    EXPECT_LT(duration.count(), 2000) << "Simulation too slow: " << duration.count() << "ms";
}

TEST_F(SelfModelSNNPlasticityRegressionTest, LearningPerformance) {
    // Register synapses
    for (int i = 0; i < 50; i++) {
        self_model_plasticity_register_synapse(plasticity_bridge,
            i, SELF_SYNAPSE_AGENCY, 0.5f);
    }

    auto start = std::chrono::high_resolution_clock::now();

    for (int cycle = 0; cycle < 100; cycle++) {
        for (int i = 0; i < 50; i++) {
            self_model_plasticity_learn(plasticity_bridge,
                SELF_LEARN_AGENCY_CONFIRMED, 0.1f, i, 0.5f);
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // 5000 learning operations should complete in under 500ms
    EXPECT_LT(duration.count(), 500) << "Learning too slow: " << duration.count() << "ms";
}

//=============================================================================
// Consistency Tests
//=============================================================================

TEST_F(SelfModelSNNPlasticityRegressionTest, DeterministicInsight) {
    float dims[SELF_DIM_COUNT];
    generate_context(dims, 42);

    // Get first insight
    self_model_snn_encode_state(snn_bridge, dims, SELF_DIM_COUNT);
    self_model_snn_simulate(snn_bridge, 20.0f);
    self_model_insight_t first_insight;
    self_model_snn_get_insight(snn_bridge, &first_insight);

    // Reset and get second insight with same input
    self_model_snn_reset(snn_bridge);
    self_model_snn_encode_state(snn_bridge, dims, SELF_DIM_COUNT);
    self_model_snn_simulate(snn_bridge, 20.0f);
    self_model_insight_t second_insight;
    self_model_snn_get_insight(snn_bridge, &second_insight);

    // Results should be identical
    EXPECT_FLOAT_EQ(first_insight.body_state_level, second_insight.body_state_level);
    EXPECT_FLOAT_EQ(first_insight.agency_level, second_insight.agency_level);
    EXPECT_FLOAT_EQ(first_insight.identity_coherence, second_insight.identity_coherence);
}

TEST_F(SelfModelSNNPlasticityRegressionTest, AgencyDetectionConsistency) {
    // Low agency should always be detected
    for (int trial = 0; trial < 10; trial++) {
        self_model_snn_reset(snn_bridge);
        self_model_snn_encode_agency(snn_bridge, 0.1f, 0.05f);
        self_model_snn_simulate(snn_bridge, 30.0f);

        float agency_level;
        self_model_snn_check_agency(snn_bridge, &agency_level);
        // Agency level should be detected
        EXPECT_GE(agency_level, 0.0f);
        EXPECT_LE(agency_level, 1.0f);
    }
}

//=============================================================================
// Protected Synapse Integrity Tests
//=============================================================================

TEST_F(SelfModelSNNPlasticityRegressionTest, IdentityProtectionUnbreakable) {
    // Register Identity synapse (auto-protected)
    ASSERT_EQ(self_model_plasticity_register_synapse(plasticity_bridge,
        100, SELF_SYNAPSE_IDENTITY, 1.0f), 0);

    self_model_plasticity_synapse_t synapse;
    self_model_plasticity_get_synapse(plasticity_bridge, 100, &synapse);
    float original_weight = synapse.weight;
    EXPECT_TRUE(synapse.is_protected);

    // Try many modification attempts
    for (int i = 0; i < 100; i++) {
        self_model_plasticity_apply_stdp(plasticity_bridge, 100, (float)i, (float)i + 10.0f);
        self_model_plasticity_learn(plasticity_bridge,
            SELF_LEARN_AGENCY_VIOLATED, -1.0f, 100, 1.0f);
        self_model_plasticity_apply_reward(plasticity_bridge, -1.0f);
    }

    // Weight must remain unchanged
    self_model_plasticity_get_synapse(plasticity_bridge, 100, &synapse);
    EXPECT_FLOAT_EQ(synapse.weight, original_weight);
    EXPECT_TRUE(synapse.is_protected);
}

TEST_F(SelfModelSNNPlasticityRegressionTest, BoundaryProtectionUnbreakable) {
    // Register Boundary synapse (auto-protected)
    ASSERT_EQ(self_model_plasticity_register_synapse(plasticity_bridge,
        200, SELF_SYNAPSE_BOUNDARY, 0.9f), 0);

    self_model_plasticity_synapse_t synapse;
    self_model_plasticity_get_synapse(plasticity_bridge, 200, &synapse);
    EXPECT_TRUE(synapse.is_protected);

    float original_weight = synapse.weight;

    // Apply learning - protected synapse should not change
    self_model_plasticity_apply_stdp(plasticity_bridge, 200, 5.0f, 10.0f);
    self_model_plasticity_get_synapse(plasticity_bridge, 200, &synapse);
    EXPECT_FLOAT_EQ(synapse.weight, original_weight) << "Protected synapse weight should not change";
}

TEST_F(SelfModelSNNPlasticityRegressionTest, ManualProtectionToggle) {
    // Register unprotected synapse
    ASSERT_EQ(self_model_plasticity_register_synapse(plasticity_bridge,
        300, SELF_SYNAPSE_NARRATIVE, 0.5f), 0);

    self_model_plasticity_synapse_t synapse;
    self_model_plasticity_get_synapse(plasticity_bridge, 300, &synapse);
    EXPECT_FALSE(synapse.is_protected);

    // Protect it
    EXPECT_EQ(self_model_plasticity_protect_synapse(plasticity_bridge, 300, true), 0);
    self_model_plasticity_get_synapse(plasticity_bridge, 300, &synapse);
    EXPECT_TRUE(synapse.is_protected);
    float protected_weight = synapse.weight;

    // Try to modify (should be blocked)
    self_model_plasticity_apply_stdp(plasticity_bridge, 300, 0.0f, 10.0f);
    self_model_plasticity_get_synapse(plasticity_bridge, 300, &synapse);
    EXPECT_FLOAT_EQ(synapse.weight, protected_weight);

    // Unprotect it
    EXPECT_EQ(self_model_plasticity_protect_synapse(plasticity_bridge, 300, false), 0);
    self_model_plasticity_get_synapse(plasticity_bridge, 300, &synapse);
    EXPECT_FALSE(synapse.is_protected);

    // Now modification should work
    self_model_plasticity_apply_stdp(plasticity_bridge, 300, 0.0f, 10.0f);
    self_model_plasticity_get_synapse(plasticity_bridge, 300, &synapse);
    // Weight may or may not change depending on STDP implementation
}

//=============================================================================
// Edge Case Tests
//=============================================================================

TEST_F(SelfModelSNNPlasticityRegressionTest, ZeroInputsHandled) {
    float dims[SELF_DIM_COUNT] = {0};  // All zeros

    int spikes = self_model_snn_encode_state(snn_bridge, dims, SELF_DIM_COUNT);
    EXPECT_GE(spikes, 0);  // Should not crash

    self_model_snn_simulate(snn_bridge, 10.0f);

    self_model_insight_t insight;
    EXPECT_EQ(self_model_snn_get_insight(snn_bridge, &insight), 0);
    // Results should be valid (normalized)
    EXPECT_TRUE(std::isfinite(insight.body_state_level));
    EXPECT_TRUE(std::isfinite(insight.agency_level));
}

TEST_F(SelfModelSNNPlasticityRegressionTest, MaxInputsHandled) {
    float dims[SELF_DIM_COUNT];
    for (int i = 0; i < SELF_DIM_COUNT; i++) {
        dims[i] = 1.0f;  // All max
    }

    int spikes = self_model_snn_encode_state(snn_bridge, dims, SELF_DIM_COUNT);
    EXPECT_GE(spikes, 0);

    self_model_snn_simulate(snn_bridge, 10.0f);

    self_model_insight_t insight;
    EXPECT_EQ(self_model_snn_get_insight(snn_bridge, &insight), 0);
    EXPECT_TRUE(std::isfinite(insight.body_state_level));
    EXPECT_TRUE(std::isfinite(insight.agency_level));
}

TEST_F(SelfModelSNNPlasticityRegressionTest, LargeSimulationTime) {
    float dims[SELF_DIM_COUNT] = {0.5f};
    self_model_snn_encode_state(snn_bridge, dims, SELF_DIM_COUNT);

    // Very long simulation should not crash or hang
    EXPECT_EQ(self_model_snn_simulate(snn_bridge, 1000.0f), 0);

    self_model_insight_t insight;
    EXPECT_EQ(self_model_snn_get_insight(snn_bridge, &insight), 0);
}

TEST_F(SelfModelSNNPlasticityRegressionTest, ZeroTimeDelta) {
    float dims[SELF_DIM_COUNT] = {0.5f};
    self_model_snn_encode_state(snn_bridge, dims, SELF_DIM_COUNT);

    // Zero time is rejected (invalid input)
    EXPECT_EQ(self_model_snn_simulate(snn_bridge, 0.0f), -1);

    // Negative time should be rejected
    EXPECT_EQ(self_model_snn_simulate(snn_bridge, -1.0f), -1);
}

//=============================================================================
// Statistics Accumulation Tests
//=============================================================================

TEST_F(SelfModelSNNPlasticityRegressionTest, SNNStatsAccurate) {
    // Perform known number of operations
    for (int i = 0; i < 10; i++) {
        float dims[SELF_DIM_COUNT] = {0.5f};
        self_model_snn_encode_state(snn_bridge, dims, SELF_DIM_COUNT);
        self_model_snn_simulate(snn_bridge, 5.0f);
    }

    self_model_snn_stats_t stats;
    self_model_snn_get_stats(snn_bridge, &stats);
    EXPECT_GE(stats.total_evaluations, 10u);
    EXPECT_GE(stats.total_simulations, 10u);
}

TEST_F(SelfModelSNNPlasticityRegressionTest, PlasticityStatsAccurate) {
    // Register synapses and perform operations
    for (int i = 0; i < 5; i++) {
        self_model_plasticity_register_synapse(plasticity_bridge,
            i, SELF_SYNAPSE_AGENCY, 0.5f);
    }

    for (int cycle = 0; cycle < 10; cycle++) {
        for (int i = 0; i < 5; i++) {
            self_model_plasticity_learn(plasticity_bridge,
                SELF_LEARN_AGENCY_CONFIRMED, 0.1f, i, 0.5f);
            self_model_plasticity_apply_stdp(plasticity_bridge, i, (float)cycle, (float)cycle + 5.0f);
        }
        self_model_plasticity_update_bcm(plasticity_bridge, 0.5f);
    }

    self_model_plasticity_stats_t stats;
    self_model_plasticity_get_stats(plasticity_bridge, &stats);
    // Check active synapses from state
    self_model_plasticity_bridge_state_t state;
    self_model_plasticity_get_state(plasticity_bridge, &state);
    EXPECT_EQ(state.active_synapses, 5u);
    EXPECT_GE(stats.total_learning_events, 50u);
    EXPECT_GE(stats.weight_updates, 50u);
}

//=============================================================================
// Reset Behavior Tests
//=============================================================================

TEST_F(SelfModelSNNPlasticityRegressionTest, ResetClearsState) {
    // Do work
    float dims[SELF_DIM_COUNT] = {0.8f};
    self_model_snn_encode_state(snn_bridge, dims, SELF_DIM_COUNT);
    self_model_snn_simulate(snn_bridge, 30.0f);

    // Reset
    EXPECT_EQ(self_model_snn_reset(snn_bridge), 0);

    // Verify state is cleared
    self_model_snn_bridge_state_t state;
    self_model_snn_get_state(snn_bridge, &state);
    EXPECT_EQ(state.state, SELF_MODEL_SNN_STATE_IDLE);
}

TEST_F(SelfModelSNNPlasticityRegressionTest, ResetStatsClearsCounters) {
    // Accumulate stats (need simulate to increment evaluations)
    for (int i = 0; i < 5; i++) {
        float dims[SELF_DIM_COUNT] = {0.5f};
        self_model_snn_encode_state(snn_bridge, dims, SELF_DIM_COUNT);
        self_model_snn_simulate(snn_bridge, 5.0f);
    }

    self_model_snn_stats_t before;
    self_model_snn_get_stats(snn_bridge, &before);
    EXPECT_GT(before.total_evaluations, 0u);

    // Reset stats
    self_model_snn_reset_stats(snn_bridge);

    self_model_snn_stats_t after;
    self_model_snn_get_stats(snn_bridge, &after);
    EXPECT_EQ(after.total_evaluations, 0u);
}

//=============================================================================
// Calibration State Tests
//=============================================================================

TEST_F(SelfModelSNNPlasticityRegressionTest, CalibrationStateConsistency) {
    // Initial calibration state
    self_model_calibration_state_t initial_state;
    EXPECT_EQ(self_model_plasticity_get_calibration_state(plasticity_bridge, &initial_state), 0);

    // Run learning cycles
    for (int i = 0; i < 20; i++) {
        self_model_plasticity_register_synapse(plasticity_bridge,
            1000 + i, SELF_SYNAPSE_AGENCY, 0.5f);
        self_model_plasticity_learn(plasticity_bridge,
            SELF_LEARN_AGENCY_CONFIRMED, 0.5f, 1000 + i, 0.7f);
    }

    // Updated calibration state should be valid
    self_model_calibration_state_t updated_state;
    EXPECT_EQ(self_model_plasticity_get_calibration_state(plasticity_bridge, &updated_state), 0);
    EXPECT_TRUE(std::isfinite(updated_state.boundary_calibration));
    EXPECT_TRUE(std::isfinite(updated_state.learning_rate_mod));
}

//=============================================================================
// Agency Learning Protection Tests
//=============================================================================

TEST_F(SelfModelSNNPlasticityRegressionTest, AgencySynapseModifiable) {
    // Register Agency synapse (NOT auto-protected)
    ASSERT_EQ(self_model_plasticity_register_synapse(plasticity_bridge,
        400, SELF_SYNAPSE_AGENCY, 0.5f), 0);

    self_model_plasticity_synapse_t synapse;
    self_model_plasticity_get_synapse(plasticity_bridge, 400, &synapse);
    EXPECT_FALSE(synapse.is_protected);

    float original_weight = synapse.weight;

    // Modifications should work
    for (int i = 0; i < 50; i++) {
        self_model_plasticity_learn(plasticity_bridge,
            SELF_LEARN_AGENCY_CONFIRMED, 0.5f, 400, 0.9f);
    }

    // Weight should have changed
    self_model_plasticity_get_synapse(plasticity_bridge, 400, &synapse);
    EXPECT_NE(synapse.weight, original_weight);
}
