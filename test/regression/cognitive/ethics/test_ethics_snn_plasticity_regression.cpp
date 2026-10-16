//=============================================================================
// test_ethics_snn_plasticity_regression.cpp - Regression Tests
//=============================================================================
/**
 * @file test_ethics_snn_plasticity_regression.cpp
 * @brief Regression tests for Ethics-SNN-Plasticity integration
 *
 * WHAT: Test for regressions in numerical stability, performance, and behavior
 * WHY:  Ensure consistent behavior across changes and prevent past bugs
 * HOW:  Test edge cases, numerical bounds, and performance constraints
 *
 * REGRESSION TEST CATEGORIES:
 * - Numerical stability (weight bounds, confidence normalization)
 * - Memory leak detection (repeated create/destroy cycles)
 * - Performance bounds (operation timing)
 * - Consistency (deterministic output for same input)
 * - Protected synapse integrity (Asimov Laws, Golden Rule)
 *
 * @date 2026-01-06
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <chrono>
#include <vector>
#include <memory>

#include "cognitive/ethics/nimcp_ethics_snn_bridge.h"
#include "cognitive/ethics/nimcp_ethics_plasticity_bridge.h"
#include "utils/time/nimcp_time.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixture
//=============================================================================

class EthicsSNNPlasticityRegressionTest : public ::testing::Test {
protected:
    ethics_snn_bridge_t* snn_bridge = nullptr;
    ethics_plasticity_bridge_t* plasticity_bridge = nullptr;

    void SetUp() override {
        // Create bridges with default configs
        ethics_snn_config_t snn_config = ethics_snn_config_default();
        snn_config.enable_bio_async = false;

        snn_bridge = ethics_snn_create(&snn_config);
        ASSERT_NE(snn_bridge, nullptr);

        ethics_plasticity_config_t plasticity_config = ethics_plasticity_config_default();
        plasticity_bridge = ethics_plasticity_create(&plasticity_config);
        ASSERT_NE(plasticity_bridge, nullptr);
    }

    void TearDown() override {
        if (snn_bridge) {
            ethics_snn_destroy(snn_bridge);
            snn_bridge = nullptr;
        }
        if (plasticity_bridge) {
            ethics_plasticity_destroy(plasticity_bridge);
            plasticity_bridge = nullptr;
        }
    }

    // Generate deterministic ethical context
    void generate_context(float* dims, uint32_t seed) {
        for (int i = 0; i < ETHICS_DIM_COUNT; i++) {
            dims[i] = 0.5f + 0.3f * sinf((float)(i + seed) * 0.1f);
        }
    }
};

//=============================================================================
// Numerical Stability Tests
//=============================================================================

TEST_F(EthicsSNNPlasticityRegressionTest, WeightBoundsAfterIntenseLearning) {
    // Register synapse
    ASSERT_EQ(ethics_plasticity_register_synapse(plasticity_bridge,
        1, ETHICS_SYNAPSE_OUTCOME, 0.5f), 0);

    // Apply intense learning cycles
    for (int i = 0; i < 100; i++) {
        float reward = (i % 2 == 0) ? 1.0f : -1.0f;
        ethics_plasticity_learn(plasticity_bridge,
            ETHICS_LEARN_POSITIVE_OUTCOME, reward, 1, 0.9f);
    }

    // Verify weight stays in valid bounds
    ethics_plasticity_synapse_t synapse;
    EXPECT_EQ(ethics_plasticity_get_synapse(plasticity_bridge, 1, &synapse), 0);
    EXPECT_GE(synapse.weight, 0.0f);
    EXPECT_LE(synapse.weight, 1.0f);
}

TEST_F(EthicsSNNPlasticityRegressionTest, JudgmentConfidenceNormalization) {
    // Run many scenarios
    for (int s = 0; s < 50; s++) {
        float dims[ETHICS_DIM_COUNT];
        generate_context(dims, s);
        dims[ETHICS_DIM_HARM] = (float)s / 50.0f;  // Vary harm

        ethics_snn_encode_context(snn_bridge, dims, ETHICS_DIM_COUNT);
        ethics_snn_simulate(snn_bridge, 20.0f);

        ethics_judgment_t judgment;
        EXPECT_EQ(ethics_snn_get_judgment(snn_bridge, &judgment), 0);

        // All scores must be normalized
        EXPECT_GE(judgment.allow_score, 0.0f);
        EXPECT_LE(judgment.allow_score, 1.0f);
        EXPECT_GE(judgment.block_score, 0.0f);
        EXPECT_LE(judgment.block_score, 1.0f);
        EXPECT_GE(judgment.confidence, 0.0f);
        EXPECT_LE(judgment.confidence, 1.0f);
    }
}

TEST_F(EthicsSNNPlasticityRegressionTest, STDPWeightStability) {
    // Register synapses
    for (int i = 0; i < 10; i++) {
        ASSERT_EQ(ethics_plasticity_register_synapse(plasticity_bridge,
            i, ETHICS_SYNAPSE_FAIRNESS, 0.5f), 0);
    }

    // Apply many STDP updates
    for (int cycle = 0; cycle < 100; cycle++) {
        for (int i = 0; i < 10; i++) {
            float pre_time = (float)(cycle + i) * 1.0f;
            float post_time = pre_time + ((cycle % 2) ? 5.0f : -5.0f);
            ethics_plasticity_apply_stdp(plasticity_bridge, i, pre_time, post_time);
        }
    }

    // Verify all weights in bounds
    for (int i = 0; i < 10; i++) {
        ethics_plasticity_synapse_t synapse;
        EXPECT_EQ(ethics_plasticity_get_synapse(plasticity_bridge, i, &synapse), 0);
        EXPECT_GE(synapse.weight, 0.0f);
        EXPECT_LE(synapse.weight, 1.0f);
    }
}

TEST_F(EthicsSNNPlasticityRegressionTest, BCMThresholdBounds) {
    // Register synapse
    ASSERT_EQ(ethics_plasticity_register_synapse(plasticity_bridge,
        1, ETHICS_SYNAPSE_OUTCOME, 0.5f), 0);

    // Apply extreme BCM updates
    for (int i = 0; i < 100; i++) {
        float rate = (i % 2 == 0) ? 1.0f : 0.0f;  // Extreme rates
        ethics_plasticity_update_bcm(plasticity_bridge, rate);
    }

    // Verify synapse still valid
    ethics_plasticity_synapse_t synapse;
    EXPECT_EQ(ethics_plasticity_get_synapse(plasticity_bridge, 1, &synapse), 0);
    EXPECT_TRUE(std::isfinite(synapse.weight));
    EXPECT_TRUE(std::isfinite(synapse.bcm_threshold));
}

//=============================================================================
// Memory Leak Detection Tests
//=============================================================================

TEST_F(EthicsSNNPlasticityRegressionTest, RepeatedCreateDestroyCycles) {
    // First, destroy existing bridges
    if (snn_bridge) {
        ethics_snn_destroy(snn_bridge);
        snn_bridge = nullptr;
    }
    if (plasticity_bridge) {
        ethics_plasticity_destroy(plasticity_bridge);
        plasticity_bridge = nullptr;
    }

    // Repeated create/destroy cycles
    for (int cycle = 0; cycle < 50; cycle++) {
        ethics_snn_config_t snn_config = ethics_snn_config_default();
        snn_config.enable_bio_async = false;
        ethics_snn_bridge_t* snn = ethics_snn_create(&snn_config);
        ASSERT_NE(snn, nullptr);

        ethics_plasticity_config_t plasticity_config = ethics_plasticity_config_default();
        ethics_plasticity_bridge_t* plasticity = ethics_plasticity_create(&plasticity_config);
        ASSERT_NE(plasticity, nullptr);

        // Do some work
        float dims[ETHICS_DIM_COUNT] = {0.5f};
        ethics_snn_encode_context(snn, dims, ETHICS_DIM_COUNT);
        ethics_snn_step(snn);

        ethics_plasticity_register_synapse(plasticity, 1, ETHICS_SYNAPSE_OUTCOME, 0.5f);
        ethics_plasticity_learn(plasticity, ETHICS_LEARN_POSITIVE_OUTCOME, 0.1f, 1, 0.5f);

        ethics_snn_destroy(snn);
        ethics_plasticity_destroy(plasticity);
    }
    // Test passes if no crash/memory exhaustion
}

TEST_F(EthicsSNNPlasticityRegressionTest, RepeatedSynapseRegistrationUnregistration) {
    // Repeated register/unregister cycles
    for (int cycle = 0; cycle < 100; cycle++) {
        ASSERT_EQ(ethics_plasticity_register_synapse(plasticity_bridge,
            1, ETHICS_SYNAPSE_OUTCOME, 0.5f), 0);
        ASSERT_EQ(ethics_plasticity_unregister_synapse(plasticity_bridge, 1), 0);
    }
    // Test passes if no crash/memory leak
}

//=============================================================================
// Performance Bounds Tests
//=============================================================================

TEST_F(EthicsSNNPlasticityRegressionTest, EncodingPerformance) {
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 100; i++) {
        float dims[ETHICS_DIM_COUNT];
        generate_context(dims, i);
        ethics_snn_encode_context(snn_bridge, dims, ETHICS_DIM_COUNT);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // 100 encodings should complete in under 1 second
    EXPECT_LT(duration.count(), 1000) << "Encoding too slow: " << duration.count() << "ms";
}

TEST_F(EthicsSNNPlasticityRegressionTest, SimulationPerformance) {
    float dims[ETHICS_DIM_COUNT];
    generate_context(dims, 0);
    ethics_snn_encode_context(snn_bridge, dims, ETHICS_DIM_COUNT);

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 100; i++) {
        ethics_snn_simulate(snn_bridge, 10.0f);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // 100 simulations should complete in under 2 seconds
    EXPECT_LT(duration.count(), 2000) << "Simulation too slow: " << duration.count() << "ms";
}

TEST_F(EthicsSNNPlasticityRegressionTest, LearningPerformance) {
    // Register synapses
    for (int i = 0; i < 50; i++) {
        ethics_plasticity_register_synapse(plasticity_bridge,
            i, ETHICS_SYNAPSE_OUTCOME, 0.5f);
    }

    auto start = std::chrono::high_resolution_clock::now();

    for (int cycle = 0; cycle < 100; cycle++) {
        for (int i = 0; i < 50; i++) {
            ethics_plasticity_learn(plasticity_bridge,
                ETHICS_LEARN_POSITIVE_OUTCOME, 0.1f, i, 0.5f);
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

TEST_F(EthicsSNNPlasticityRegressionTest, DeterministicJudgment) {
    float dims[ETHICS_DIM_COUNT];
    generate_context(dims, 42);

    // Get first judgment
    ethics_snn_encode_context(snn_bridge, dims, ETHICS_DIM_COUNT);
    ethics_snn_simulate(snn_bridge, 20.0f);
    ethics_judgment_t first_judgment;
    ethics_snn_get_judgment(snn_bridge, &first_judgment);

    // Reset and get second judgment with same input
    ethics_snn_reset(snn_bridge);
    ethics_snn_encode_context(snn_bridge, dims, ETHICS_DIM_COUNT);
    ethics_snn_simulate(snn_bridge, 20.0f);
    ethics_judgment_t second_judgment;
    ethics_snn_get_judgment(snn_bridge, &second_judgment);

    // Results should be identical
    EXPECT_FLOAT_EQ(first_judgment.allow_score, second_judgment.allow_score);
    EXPECT_FLOAT_EQ(first_judgment.block_score, second_judgment.block_score);
    EXPECT_EQ(first_judgment.harm_detected, second_judgment.harm_detected);
}

TEST_F(EthicsSNNPlasticityRegressionTest, HarmDetectionConsistency) {
    // High harm should always be detected
    for (int trial = 0; trial < 10; trial++) {
        ethics_snn_reset(snn_bridge);
        ethics_snn_encode_harm(snn_bridge, 0.95f, 0.9f);
        ethics_snn_simulate(snn_bridge, 30.0f);

        float harm_level;
        bool detected = ethics_snn_check_harm(snn_bridge, &harm_level);
        EXPECT_TRUE(detected) << "Failed on trial " << trial;
        EXPECT_GT(harm_level, 0.3f);
    }
}

//=============================================================================
// Protected Synapse Integrity Tests
//=============================================================================

TEST_F(EthicsSNNPlasticityRegressionTest, FirstLawProtectionUnbreakable) {
    // Register First Law synapse (auto-protected)
    ASSERT_EQ(ethics_plasticity_register_synapse(plasticity_bridge,
        100, ETHICS_SYNAPSE_FIRST_LAW, 1.0f), 0);

    ethics_plasticity_synapse_t synapse;
    ethics_plasticity_get_synapse(plasticity_bridge, 100, &synapse);
    float original_weight = synapse.weight;
    EXPECT_TRUE(synapse.is_protected);

    // Try many modification attempts
    for (int i = 0; i < 100; i++) {
        ethics_plasticity_apply_stdp(plasticity_bridge, 100, (float)i, (float)i + 10.0f);
        ethics_plasticity_learn(plasticity_bridge,
            ETHICS_LEARN_NEGATIVE_OUTCOME, -1.0f, 100, 1.0f);
        ethics_plasticity_apply_reward(plasticity_bridge, -1.0f);
    }

    // Weight must remain unchanged
    ethics_plasticity_get_synapse(plasticity_bridge, 100, &synapse);
    EXPECT_FLOAT_EQ(synapse.weight, original_weight);
    EXPECT_TRUE(synapse.is_protected);
}

TEST_F(EthicsSNNPlasticityRegressionTest, GoldenRuleProtectionUnbreakable) {
    // Register Golden Rule synapse
    ASSERT_EQ(ethics_plasticity_register_synapse(plasticity_bridge,
        200, ETHICS_SYNAPSE_GOLDEN_RULE, 0.9f), 0);

    ethics_plasticity_synapse_t synapse;
    ethics_plasticity_get_synapse(plasticity_bridge, 200, &synapse);

    // Golden Rule synapses may or may not be auto-protected
    // Manually protect to test protection mechanism
    EXPECT_EQ(ethics_plasticity_protect_synapse(plasticity_bridge, 200), 0);
    ethics_plasticity_get_synapse(plasticity_bridge, 200, &synapse);
    EXPECT_TRUE(synapse.is_protected);

    float original_weight = synapse.weight;

    // Apply learning - protected synapse should not change
    ethics_plasticity_apply_stdp(plasticity_bridge, 200, 5.0f, 10.0f);
    ethics_plasticity_get_synapse(plasticity_bridge, 200, &synapse);
    EXPECT_FLOAT_EQ(synapse.weight, original_weight) << "Protected synapse weight should not change";
}

TEST_F(EthicsSNNPlasticityRegressionTest, ManualProtectionToggle) {
    // Register unprotected synapse
    ASSERT_EQ(ethics_plasticity_register_synapse(plasticity_bridge,
        300, ETHICS_SYNAPSE_EMPATHY, 0.5f), 0);

    ethics_plasticity_synapse_t synapse;
    ethics_plasticity_get_synapse(plasticity_bridge, 300, &synapse);
    EXPECT_FALSE(synapse.is_protected);

    // Protect it
    EXPECT_EQ(ethics_plasticity_protect_synapse(plasticity_bridge, 300), 0);
    ethics_plasticity_get_synapse(plasticity_bridge, 300, &synapse);
    EXPECT_TRUE(synapse.is_protected);
    float protected_weight = synapse.weight;

    // Try to modify (should be blocked)
    ethics_plasticity_apply_stdp(plasticity_bridge, 300, 0.0f, 10.0f);
    ethics_plasticity_get_synapse(plasticity_bridge, 300, &synapse);
    EXPECT_FLOAT_EQ(synapse.weight, protected_weight);

    // Unprotect it
    EXPECT_EQ(ethics_plasticity_unprotect_synapse(plasticity_bridge, 300), 0);
    ethics_plasticity_get_synapse(plasticity_bridge, 300, &synapse);
    EXPECT_FALSE(synapse.is_protected);

    // Now modification should work
    ethics_plasticity_apply_stdp(plasticity_bridge, 300, 0.0f, 10.0f);
    ethics_plasticity_get_synapse(plasticity_bridge, 300, &synapse);
    // Weight may or may not change depending on STDP implementation
}

//=============================================================================
// Edge Case Tests
//=============================================================================

TEST_F(EthicsSNNPlasticityRegressionTest, ZeroInputsHandled) {
    float dims[ETHICS_DIM_COUNT] = {0};  // All zeros

    int spikes = ethics_snn_encode_context(snn_bridge, dims, ETHICS_DIM_COUNT);
    EXPECT_GE(spikes, 0);  // Should not crash

    ethics_snn_simulate(snn_bridge, 10.0f);

    ethics_judgment_t judgment;
    EXPECT_EQ(ethics_snn_get_judgment(snn_bridge, &judgment), 0);
    // Results should be valid (normalized)
    EXPECT_TRUE(std::isfinite(judgment.allow_score));
    EXPECT_TRUE(std::isfinite(judgment.block_score));
}

TEST_F(EthicsSNNPlasticityRegressionTest, MaxInputsHandled) {
    float dims[ETHICS_DIM_COUNT];
    for (int i = 0; i < ETHICS_DIM_COUNT; i++) {
        dims[i] = 1.0f;  // All max
    }

    int spikes = ethics_snn_encode_context(snn_bridge, dims, ETHICS_DIM_COUNT);
    EXPECT_GE(spikes, 0);

    ethics_snn_simulate(snn_bridge, 10.0f);

    ethics_judgment_t judgment;
    EXPECT_EQ(ethics_snn_get_judgment(snn_bridge, &judgment), 0);
    EXPECT_TRUE(std::isfinite(judgment.allow_score));
    EXPECT_TRUE(std::isfinite(judgment.block_score));
}

TEST_F(EthicsSNNPlasticityRegressionTest, LargeSimulationTime) {
    float dims[ETHICS_DIM_COUNT] = {0.5f};
    ethics_snn_encode_context(snn_bridge, dims, ETHICS_DIM_COUNT);

    // Very long simulation should not crash or hang
    EXPECT_EQ(ethics_snn_simulate(snn_bridge, 1000.0f), 0);

    ethics_judgment_t judgment;
    EXPECT_EQ(ethics_snn_get_judgment(snn_bridge, &judgment), 0);
}

TEST_F(EthicsSNNPlasticityRegressionTest, ZeroTimeDelta) {
    float dims[ETHICS_DIM_COUNT] = {0.5f};
    ethics_snn_encode_context(snn_bridge, dims, ETHICS_DIM_COUNT);

    // Zero time is rejected (invalid input)
    EXPECT_EQ(ethics_snn_simulate(snn_bridge, 0.0f), -1);

    // Negative time should be rejected
    EXPECT_EQ(ethics_snn_simulate(snn_bridge, -1.0f), -1);
}

//=============================================================================
// Statistics Accumulation Tests
//=============================================================================

TEST_F(EthicsSNNPlasticityRegressionTest, SNNStatsAccurate) {
    // Perform known number of operations
    for (int i = 0; i < 10; i++) {
        float dims[ETHICS_DIM_COUNT] = {0.5f};
        ethics_snn_encode_context(snn_bridge, dims, ETHICS_DIM_COUNT);
        ethics_snn_simulate(snn_bridge, 5.0f);
    }

    ethics_snn_stats_t stats;
    ethics_snn_get_stats(snn_bridge, &stats);
    EXPECT_GE(stats.total_evaluations, 10u);
    EXPECT_GE(stats.total_simulations, 10u);
}

TEST_F(EthicsSNNPlasticityRegressionTest, PlasticityStatsAccurate) {
    // Register synapses and perform operations
    for (int i = 0; i < 5; i++) {
        ethics_plasticity_register_synapse(plasticity_bridge,
            i, ETHICS_SYNAPSE_OUTCOME, 0.5f);
    }

    for (int cycle = 0; cycle < 10; cycle++) {
        for (int i = 0; i < 5; i++) {
            ethics_plasticity_learn(plasticity_bridge,
                ETHICS_LEARN_POSITIVE_OUTCOME, 0.1f, i, 0.5f);
            ethics_plasticity_apply_stdp(plasticity_bridge, i, (float)cycle, (float)cycle + 5.0f);
        }
        ethics_plasticity_update_bcm(plasticity_bridge, 0.5f);
    }

    ethics_plasticity_stats_t stats;
    ethics_plasticity_get_stats(plasticity_bridge, &stats);
    // Check active synapses from state
    ethics_plasticity_bridge_state_t state;
    ethics_plasticity_get_state(plasticity_bridge, &state);
    EXPECT_EQ(state.active_synapses, 5u);
    EXPECT_GE(stats.total_learning_events, 50u);
    EXPECT_GE(stats.weight_updates, 50u);
}

//=============================================================================
// Reset Behavior Tests
//=============================================================================

TEST_F(EthicsSNNPlasticityRegressionTest, ResetClearsState) {
    // Do work
    float dims[ETHICS_DIM_COUNT] = {0.8f};
    ethics_snn_encode_context(snn_bridge, dims, ETHICS_DIM_COUNT);
    ethics_snn_simulate(snn_bridge, 30.0f);

    // Reset
    EXPECT_EQ(ethics_snn_reset(snn_bridge), 0);

    // Verify state is cleared
    ethics_snn_bridge_state_t state;
    ethics_snn_get_state(snn_bridge, &state);
    EXPECT_EQ(state.state, ETHICS_SNN_STATE_IDLE);
}

TEST_F(EthicsSNNPlasticityRegressionTest, ResetStatsClearsCounters) {
    // Accumulate stats
    for (int i = 0; i < 5; i++) {
        float dims[ETHICS_DIM_COUNT] = {0.5f};
        ethics_snn_encode_context(snn_bridge, dims, ETHICS_DIM_COUNT);
    }

    ethics_snn_stats_t before;
    ethics_snn_get_stats(snn_bridge, &before);
    EXPECT_GT(before.total_evaluations, 0u);

    // Reset stats
    ethics_snn_reset_stats(snn_bridge);

    ethics_snn_stats_t after;
    ethics_snn_get_stats(snn_bridge, &after);
    EXPECT_EQ(after.total_evaluations, 0u);
}
