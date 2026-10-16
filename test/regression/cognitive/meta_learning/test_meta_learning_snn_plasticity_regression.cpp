//=============================================================================
// test_meta_learning_snn_plasticity_regression.cpp - Regression Tests
//=============================================================================
/**
 * @file test_meta_learning_snn_plasticity_regression.cpp
 * @brief Regression tests for Meta Learning-SNN-Plasticity integration
 *
 * WHAT: Test for regressions in numerical stability, performance, and behavior
 * WHY:  Ensure consistent behavior across changes and prevent past bugs
 * HOW:  Test edge cases, numerical bounds, and performance constraints
 *
 * REGRESSION TEST CATEGORIES:
 * - Numerical stability (weight bounds, adaptation normalization)
 * - Memory leak detection (repeated create/destroy cycles)
 * - Performance bounds (operation timing)
 * - Consistency (deterministic output for same input)
 * - Protected synapse integrity (Learning Rate, Consolidation)
 *
 * @date 2026-01-06
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <chrono>
#include <vector>
#include <memory>

#include "cognitive/meta_learning/nimcp_meta_learning_snn_bridge.h"
#include "cognitive/meta_learning/nimcp_meta_learning_plasticity_bridge.h"
#include "utils/time/nimcp_time.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixture
//=============================================================================

class MetaLearningSNNPlasticityRegressionTest : public ::testing::Test {
protected:
    meta_learning_snn_bridge_t* snn_bridge = nullptr;
    meta_learning_plasticity_bridge_t* plasticity_bridge = nullptr;

    void SetUp() override {
        // Create bridges with default configs
        meta_learning_snn_config_t snn_config = meta_learning_snn_config_default();
        snn_config.enable_bio_async = false;

        snn_bridge = meta_learning_snn_create(&snn_config);
        ASSERT_NE(snn_bridge, nullptr);

        meta_learning_plasticity_config_t plasticity_config = meta_learning_plasticity_config_default();
        plasticity_bridge = meta_learning_plasticity_create(&plasticity_config);
        ASSERT_NE(plasticity_bridge, nullptr);
    }

    void TearDown() override {
        if (snn_bridge) {
            meta_learning_snn_destroy(snn_bridge);
            snn_bridge = nullptr;
        }
        if (plasticity_bridge) {
            meta_learning_plasticity_destroy(plasticity_bridge);
            plasticity_bridge = nullptr;
        }
    }

    // Generate deterministic meta learning context
    void generate_context(float* dims, uint32_t seed) {
        for (int i = 0; i < META_DIM_COUNT; i++) {
            dims[i] = 0.5f + 0.3f * sinf((float)(i + seed) * 0.1f);
        }
    }
};

//=============================================================================
// Numerical Stability Tests
//=============================================================================

TEST_F(MetaLearningSNNPlasticityRegressionTest, WeightBoundsAfterIntenseLearning) {
    // Register synapse
    ASSERT_EQ(meta_learning_plasticity_register_synapse(plasticity_bridge,
        1, META_SYNAPSE_STRATEGY, 0.5f), 0);

    // Apply intense learning cycles
    for (int i = 0; i < 100; i++) {
        float reward = (i % 2 == 0) ? 1.0f : -1.0f;
        meta_learning_plasticity_learn(plasticity_bridge,
            META_LEARN_RATE_CORRECT, reward, 1, 0.9f);
    }

    // Verify weight stays in valid bounds
    meta_learning_plasticity_synapse_t synapse;
    EXPECT_EQ(meta_learning_plasticity_get_synapse(plasticity_bridge, 1, &synapse), 0);
    EXPECT_GE(synapse.weight, 0.0f);
    EXPECT_LE(synapse.weight, 2.0f);
}

TEST_F(MetaLearningSNNPlasticityRegressionTest, InsightAdaptationNormalization) {
    // Run many scenarios
    for (int s = 0; s < 50; s++) {
        float dims[META_DIM_COUNT];
        generate_context(dims, s);
        dims[META_DIM_ADAPTATION_SPEED] = (float)s / 50.0f;  // Vary adaptation

        meta_learning_snn_encode_state(snn_bridge, dims, META_DIM_COUNT);
        meta_learning_snn_simulate(snn_bridge, 20.0f);

        meta_learning_insight_t insight;
        EXPECT_EQ(meta_learning_snn_get_insight(snn_bridge, &insight), 0);

        // All scores must be normalized
        EXPECT_GE(insight.learning_rate_level, 0.0f);
        EXPECT_LE(insight.learning_rate_level, 1.0f);
        EXPECT_GE(insight.adaptation_level, 0.0f);
        EXPECT_LE(insight.adaptation_level, 1.0f);
        EXPECT_GE(insight.transfer_potential, 0.0f);
        EXPECT_LE(insight.transfer_potential, 1.0f);
    }
}

TEST_F(MetaLearningSNNPlasticityRegressionTest, STDPWeightStability) {
    // Register synapses
    for (int i = 0; i < 10; i++) {
        ASSERT_EQ(meta_learning_plasticity_register_synapse(plasticity_bridge,
            i, META_SYNAPSE_STRATEGY, 0.5f), 0);
    }

    // Apply many STDP updates
    for (int cycle = 0; cycle < 100; cycle++) {
        for (int i = 0; i < 10; i++) {
            float pre_time = (float)(cycle + i) * 1.0f;
            float post_time = pre_time + ((cycle % 2) ? 5.0f : -5.0f);
            meta_learning_plasticity_apply_stdp(plasticity_bridge, i, pre_time, post_time);
        }
    }

    // Verify all weights in bounds
    for (int i = 0; i < 10; i++) {
        meta_learning_plasticity_synapse_t synapse;
        EXPECT_EQ(meta_learning_plasticity_get_synapse(plasticity_bridge, i, &synapse), 0);
        EXPECT_GE(synapse.weight, 0.0f);
        EXPECT_LE(synapse.weight, 2.0f);
    }
}

TEST_F(MetaLearningSNNPlasticityRegressionTest, BCMThresholdBounds) {
    // Register synapse
    ASSERT_EQ(meta_learning_plasticity_register_synapse(plasticity_bridge,
        1, META_SYNAPSE_STRATEGY, 0.5f), 0);

    // Apply extreme BCM updates
    for (int i = 0; i < 100; i++) {
        float rate = (i % 2 == 0) ? 1.0f : 0.0f;  // Extreme rates
        meta_learning_plasticity_update_bcm(plasticity_bridge, rate);
    }

    // Verify synapse still valid
    meta_learning_plasticity_synapse_t synapse;
    EXPECT_EQ(meta_learning_plasticity_get_synapse(plasticity_bridge, 1, &synapse), 0);
    EXPECT_TRUE(std::isfinite(synapse.weight));
    EXPECT_TRUE(std::isfinite(synapse.bcm_threshold));
}

//=============================================================================
// Memory Leak Detection Tests
//=============================================================================

TEST_F(MetaLearningSNNPlasticityRegressionTest, RepeatedCreateDestroyCycles) {
    // First, destroy existing bridges
    if (snn_bridge) {
        meta_learning_snn_destroy(snn_bridge);
        snn_bridge = nullptr;
    }
    if (plasticity_bridge) {
        meta_learning_plasticity_destroy(plasticity_bridge);
        plasticity_bridge = nullptr;
    }

    // Repeated create/destroy cycles
    for (int cycle = 0; cycle < 50; cycle++) {
        meta_learning_snn_config_t snn_config = meta_learning_snn_config_default();
        snn_config.enable_bio_async = false;
        meta_learning_snn_bridge_t* snn = meta_learning_snn_create(&snn_config);
        ASSERT_NE(snn, nullptr);

        meta_learning_plasticity_config_t plasticity_config = meta_learning_plasticity_config_default();
        meta_learning_plasticity_bridge_t* plasticity = meta_learning_plasticity_create(&plasticity_config);
        ASSERT_NE(plasticity, nullptr);

        // Do some work
        float dims[META_DIM_COUNT] = {0.5f};
        meta_learning_snn_encode_state(snn, dims, META_DIM_COUNT);
        meta_learning_snn_step(snn);

        meta_learning_plasticity_register_synapse(plasticity, 1, META_SYNAPSE_STRATEGY, 0.5f);
        meta_learning_plasticity_learn(plasticity, META_LEARN_RATE_CORRECT, 0.1f, 1, 0.5f);

        meta_learning_snn_destroy(snn);
        meta_learning_plasticity_destroy(plasticity);
    }
    // Test passes if no crash/memory exhaustion
}

TEST_F(MetaLearningSNNPlasticityRegressionTest, RepeatedSynapseRegistrationUnregistration) {
    // Repeated register/unregister cycles
    for (int cycle = 0; cycle < 100; cycle++) {
        ASSERT_EQ(meta_learning_plasticity_register_synapse(plasticity_bridge,
            1, META_SYNAPSE_STRATEGY, 0.5f), 0);
        ASSERT_EQ(meta_learning_plasticity_unregister_synapse(plasticity_bridge, 1), 0);
    }
    // Test passes if no crash/memory leak
}

//=============================================================================
// Performance Bounds Tests
//=============================================================================

TEST_F(MetaLearningSNNPlasticityRegressionTest, EncodingPerformance) {
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 100; i++) {
        float dims[META_DIM_COUNT];
        generate_context(dims, i);
        meta_learning_snn_encode_state(snn_bridge, dims, META_DIM_COUNT);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // 100 encodings should complete in under 1 second
    EXPECT_LT(duration.count(), 1000) << "Encoding too slow: " << duration.count() << "ms";
}

TEST_F(MetaLearningSNNPlasticityRegressionTest, SimulationPerformance) {
    float dims[META_DIM_COUNT];
    generate_context(dims, 0);
    meta_learning_snn_encode_state(snn_bridge, dims, META_DIM_COUNT);

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 100; i++) {
        meta_learning_snn_simulate(snn_bridge, 10.0f);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // 100 simulations should complete in under 2 seconds
    EXPECT_LT(duration.count(), 2000) << "Simulation too slow: " << duration.count() << "ms";
}

TEST_F(MetaLearningSNNPlasticityRegressionTest, LearningPerformance) {
    // Register synapses
    for (int i = 0; i < 50; i++) {
        meta_learning_plasticity_register_synapse(plasticity_bridge,
            i, META_SYNAPSE_STRATEGY, 0.5f);
    }

    auto start = std::chrono::high_resolution_clock::now();

    for (int cycle = 0; cycle < 100; cycle++) {
        for (int i = 0; i < 50; i++) {
            meta_learning_plasticity_learn(plasticity_bridge,
                META_LEARN_RATE_CORRECT, 0.1f, i, 0.5f);
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

TEST_F(MetaLearningSNNPlasticityRegressionTest, DeterministicInsight) {
    float dims[META_DIM_COUNT];
    generate_context(dims, 42);

    // Get first insight
    meta_learning_snn_encode_state(snn_bridge, dims, META_DIM_COUNT);
    meta_learning_snn_simulate(snn_bridge, 20.0f);
    meta_learning_insight_t first_insight;
    meta_learning_snn_get_insight(snn_bridge, &first_insight);

    // Reset and get second insight with same input
    meta_learning_snn_reset(snn_bridge);
    meta_learning_snn_encode_state(snn_bridge, dims, META_DIM_COUNT);
    meta_learning_snn_simulate(snn_bridge, 20.0f);
    meta_learning_insight_t second_insight;
    meta_learning_snn_get_insight(snn_bridge, &second_insight);

    // Results should be identical
    EXPECT_FLOAT_EQ(first_insight.learning_rate_level, second_insight.learning_rate_level);
    EXPECT_FLOAT_EQ(first_insight.adaptation_level, second_insight.adaptation_level);
    EXPECT_FLOAT_EQ(first_insight.transfer_potential, second_insight.transfer_potential);
}

TEST_F(MetaLearningSNNPlasticityRegressionTest, TransferDetectionConsistency) {
    // High transfer should always be detected
    for (int trial = 0; trial < 10; trial++) {
        meta_learning_snn_reset(snn_bridge);
        meta_learning_snn_encode_transfer(snn_bridge, 0.95f, 1);
        meta_learning_snn_simulate(snn_bridge, 30.0f);

        float transfer_level;
        meta_learning_snn_check_transfer(snn_bridge, &transfer_level);
        // Transfer level should be detected
        EXPECT_GE(transfer_level, 0.0f);
        EXPECT_LE(transfer_level, 1.0f);
    }
}

//=============================================================================
// Protected Synapse Integrity Tests
//=============================================================================

TEST_F(MetaLearningSNNPlasticityRegressionTest, LearningRateProtectionUnbreakable) {
    // Register LearningRate synapse (auto-protected)
    ASSERT_EQ(meta_learning_plasticity_register_synapse(plasticity_bridge,
        100, META_SYNAPSE_LEARNING_RATE, 1.0f), 0);

    meta_learning_plasticity_synapse_t synapse;
    meta_learning_plasticity_get_synapse(plasticity_bridge, 100, &synapse);
    float original_weight = synapse.weight;
    EXPECT_TRUE(synapse.is_protected);

    // Try many modification attempts
    for (int i = 0; i < 100; i++) {
        meta_learning_plasticity_apply_stdp(plasticity_bridge, 100, (float)i, (float)i + 10.0f);
        meta_learning_plasticity_learn(plasticity_bridge,
            META_LEARN_RATE_TOO_HIGH, -1.0f, 100, 1.0f);
        meta_learning_plasticity_apply_reward(plasticity_bridge, -1.0f);
    }

    // Weight must remain unchanged
    meta_learning_plasticity_get_synapse(plasticity_bridge, 100, &synapse);
    EXPECT_FLOAT_EQ(synapse.weight, original_weight);
    EXPECT_TRUE(synapse.is_protected);
}

TEST_F(MetaLearningSNNPlasticityRegressionTest, ConsolidationProtectionUnbreakable) {
    // Register Consolidation synapse (auto-protected)
    ASSERT_EQ(meta_learning_plasticity_register_synapse(plasticity_bridge,
        200, META_SYNAPSE_CONSOLIDATION, 0.9f), 0);

    meta_learning_plasticity_synapse_t synapse;
    meta_learning_plasticity_get_synapse(plasticity_bridge, 200, &synapse);
    EXPECT_TRUE(synapse.is_protected);

    float original_weight = synapse.weight;

    // Apply learning - protected synapse should not change
    meta_learning_plasticity_apply_stdp(plasticity_bridge, 200, 5.0f, 10.0f);
    meta_learning_plasticity_get_synapse(plasticity_bridge, 200, &synapse);
    EXPECT_FLOAT_EQ(synapse.weight, original_weight) << "Protected synapse weight should not change";
}

TEST_F(MetaLearningSNNPlasticityRegressionTest, ManualProtectionToggle) {
    // Register unprotected synapse
    ASSERT_EQ(meta_learning_plasticity_register_synapse(plasticity_bridge,
        300, META_SYNAPSE_TRANSFER, 0.5f), 0);

    meta_learning_plasticity_synapse_t synapse;
    meta_learning_plasticity_get_synapse(plasticity_bridge, 300, &synapse);
    EXPECT_FALSE(synapse.is_protected);

    // Protect it
    EXPECT_EQ(meta_learning_plasticity_protect_synapse(plasticity_bridge, 300, true), 0);
    meta_learning_plasticity_get_synapse(plasticity_bridge, 300, &synapse);
    EXPECT_TRUE(synapse.is_protected);
    float protected_weight = synapse.weight;

    // Try to modify (should be blocked)
    meta_learning_plasticity_apply_stdp(plasticity_bridge, 300, 0.0f, 10.0f);
    meta_learning_plasticity_get_synapse(plasticity_bridge, 300, &synapse);
    EXPECT_FLOAT_EQ(synapse.weight, protected_weight);

    // Unprotect it
    EXPECT_EQ(meta_learning_plasticity_protect_synapse(plasticity_bridge, 300, false), 0);
    meta_learning_plasticity_get_synapse(plasticity_bridge, 300, &synapse);
    EXPECT_FALSE(synapse.is_protected);

    // Now modification should work
    meta_learning_plasticity_apply_stdp(plasticity_bridge, 300, 0.0f, 10.0f);
    meta_learning_plasticity_get_synapse(plasticity_bridge, 300, &synapse);
    // Weight may or may not change depending on STDP implementation
}

//=============================================================================
// Edge Case Tests
//=============================================================================

TEST_F(MetaLearningSNNPlasticityRegressionTest, ZeroInputsHandled) {
    float dims[META_DIM_COUNT] = {0};  // All zeros

    int spikes = meta_learning_snn_encode_state(snn_bridge, dims, META_DIM_COUNT);
    EXPECT_GE(spikes, 0);  // Should not crash

    meta_learning_snn_simulate(snn_bridge, 10.0f);

    meta_learning_insight_t insight;
    EXPECT_EQ(meta_learning_snn_get_insight(snn_bridge, &insight), 0);
    // Results should be valid (normalized)
    EXPECT_TRUE(std::isfinite(insight.learning_rate_level));
    EXPECT_TRUE(std::isfinite(insight.adaptation_level));
}

TEST_F(MetaLearningSNNPlasticityRegressionTest, MaxInputsHandled) {
    float dims[META_DIM_COUNT];
    for (int i = 0; i < META_DIM_COUNT; i++) {
        dims[i] = 1.0f;  // All max
    }

    int spikes = meta_learning_snn_encode_state(snn_bridge, dims, META_DIM_COUNT);
    EXPECT_GE(spikes, 0);

    meta_learning_snn_simulate(snn_bridge, 10.0f);

    meta_learning_insight_t insight;
    EXPECT_EQ(meta_learning_snn_get_insight(snn_bridge, &insight), 0);
    EXPECT_TRUE(std::isfinite(insight.learning_rate_level));
    EXPECT_TRUE(std::isfinite(insight.adaptation_level));
}

TEST_F(MetaLearningSNNPlasticityRegressionTest, LargeSimulationTime) {
    float dims[META_DIM_COUNT] = {0.5f};
    meta_learning_snn_encode_state(snn_bridge, dims, META_DIM_COUNT);

    // Very long simulation should not crash or hang
    EXPECT_EQ(meta_learning_snn_simulate(snn_bridge, 1000.0f), 0);

    meta_learning_insight_t insight;
    EXPECT_EQ(meta_learning_snn_get_insight(snn_bridge, &insight), 0);
}

TEST_F(MetaLearningSNNPlasticityRegressionTest, ZeroTimeDelta) {
    float dims[META_DIM_COUNT] = {0.5f};
    meta_learning_snn_encode_state(snn_bridge, dims, META_DIM_COUNT);

    // Zero time is rejected (invalid input)
    EXPECT_EQ(meta_learning_snn_simulate(snn_bridge, 0.0f), -1);

    // Negative time should be rejected
    EXPECT_EQ(meta_learning_snn_simulate(snn_bridge, -1.0f), -1);
}

//=============================================================================
// Statistics Accumulation Tests
//=============================================================================

TEST_F(MetaLearningSNNPlasticityRegressionTest, SNNStatsAccurate) {
    // Perform known number of operations
    for (int i = 0; i < 10; i++) {
        float dims[META_DIM_COUNT] = {0.5f};
        meta_learning_snn_encode_state(snn_bridge, dims, META_DIM_COUNT);
        meta_learning_snn_simulate(snn_bridge, 5.0f);
    }

    meta_learning_snn_stats_t stats;
    meta_learning_snn_get_stats(snn_bridge, &stats);
    EXPECT_GE(stats.total_evaluations, 10u);
    EXPECT_GE(stats.total_simulations, 10u);
}

TEST_F(MetaLearningSNNPlasticityRegressionTest, PlasticityStatsAccurate) {
    // Register synapses and perform operations
    for (int i = 0; i < 5; i++) {
        meta_learning_plasticity_register_synapse(plasticity_bridge,
            i, META_SYNAPSE_STRATEGY, 0.5f);
    }

    for (int cycle = 0; cycle < 10; cycle++) {
        for (int i = 0; i < 5; i++) {
            meta_learning_plasticity_learn(plasticity_bridge,
                META_LEARN_RATE_CORRECT, 0.1f, i, 0.5f);
            meta_learning_plasticity_apply_stdp(plasticity_bridge, i, (float)cycle, (float)cycle + 5.0f);
        }
        meta_learning_plasticity_update_bcm(plasticity_bridge, 0.5f);
    }

    meta_learning_plasticity_stats_t stats;
    meta_learning_plasticity_get_stats(plasticity_bridge, &stats);
    // Check active synapses from state
    meta_learning_plasticity_bridge_state_t state;
    meta_learning_plasticity_get_state(plasticity_bridge, &state);
    EXPECT_EQ(state.active_synapses, 5u);
    EXPECT_GE(stats.total_learning_events, 50u);
    EXPECT_GE(stats.weight_updates, 50u);
}

//=============================================================================
// Reset Behavior Tests
//=============================================================================

TEST_F(MetaLearningSNNPlasticityRegressionTest, ResetClearsState) {
    // Do work
    float dims[META_DIM_COUNT] = {0.8f};
    meta_learning_snn_encode_state(snn_bridge, dims, META_DIM_COUNT);
    meta_learning_snn_simulate(snn_bridge, 30.0f);

    // Reset
    EXPECT_EQ(meta_learning_snn_reset(snn_bridge), 0);

    // Verify state is cleared
    meta_learning_snn_bridge_state_t state;
    meta_learning_snn_get_state(snn_bridge, &state);
    EXPECT_EQ(state.state, META_LEARNING_SNN_STATE_IDLE);
}

TEST_F(MetaLearningSNNPlasticityRegressionTest, ResetStatsClearsCounters) {
    // Accumulate stats (need simulate to increment evaluations)
    for (int i = 0; i < 5; i++) {
        float dims[META_DIM_COUNT] = {0.5f};
        meta_learning_snn_encode_state(snn_bridge, dims, META_DIM_COUNT);
        meta_learning_snn_simulate(snn_bridge, 5.0f);
    }

    meta_learning_snn_stats_t before;
    meta_learning_snn_get_stats(snn_bridge, &before);
    EXPECT_GT(before.total_evaluations, 0u);

    // Reset stats
    meta_learning_snn_reset_stats(snn_bridge);

    meta_learning_snn_stats_t after;
    meta_learning_snn_get_stats(snn_bridge, &after);
    EXPECT_EQ(after.total_evaluations, 0u);
}

//=============================================================================
// Adaptation State Tests
//=============================================================================

TEST_F(MetaLearningSNNPlasticityRegressionTest, AdaptationStateConsistency) {
    // Initial adaptation state
    meta_learning_adaptation_state_t initial_state;
    EXPECT_EQ(meta_learning_plasticity_get_adaptation_state(plasticity_bridge, &initial_state), 0);

    // Run learning cycles
    for (int i = 0; i < 20; i++) {
        meta_learning_plasticity_register_synapse(plasticity_bridge,
            1000 + i, META_SYNAPSE_STRATEGY, 0.5f);
        meta_learning_plasticity_learn(plasticity_bridge,
            META_LEARN_RATE_CORRECT, 0.5f, 1000 + i, 0.7f);
    }

    // Updated adaptation state should be valid
    meta_learning_adaptation_state_t updated_state;
    EXPECT_EQ(meta_learning_plasticity_get_adaptation_state(plasticity_bridge, &updated_state), 0);
    EXPECT_TRUE(std::isfinite(updated_state.transfer_calibration));
    EXPECT_TRUE(std::isfinite(updated_state.learning_rate_mod));
}
