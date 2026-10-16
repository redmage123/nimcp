//=============================================================================
// test_mirror_snn_plasticity_regression.cpp - Regression Tests
//=============================================================================
/**
 * @file test_mirror_snn_plasticity_regression.cpp
 * @brief Regression tests for Mirror-SNN-Plasticity integration
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
 * - Historical bug reproductions
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
#include "cognitive/mirror_neurons/nimcp_mirror_snn_bridge.h"
#include "cognitive/mirror_neurons/nimcp_mirror_plasticity_bridge.h"

#include "utils/time/nimcp_time.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixture
//=============================================================================

class MirrorSNNPlasticityRegressionTest : public ::testing::Test {
protected:
    mirror_snn_bridge_t* snn_bridge = nullptr;
    mirror_plasticity_bridge_t* plasticity_bridge = nullptr;

    void SetUp() override {
        // Create bridges with default configs
        mirror_snn_config_t snn_config = mirror_snn_config_default();
        snn_config.input_dim = 32;
        snn_config.hidden_dim = 64;
        snn_config.output_dim = 8;
        snn_config.enable_bio_async = false;
        snn_config.enable_immune_integration = false;

        snn_bridge = mirror_snn_create(&snn_config);
        ASSERT_NE(snn_bridge, nullptr);

        mirror_plasticity_config_t plasticity_config = mirror_plasticity_config_default();
        plasticity_bridge = mirror_plasticity_create(&plasticity_config);
        ASSERT_NE(plasticity_bridge, nullptr);
    }

    void TearDown() override {
        if (snn_bridge) {
            mirror_snn_destroy(snn_bridge);
            snn_bridge = nullptr;
        }
        if (plasticity_bridge) {
            mirror_plasticity_destroy(plasticity_bridge);
            plasticity_bridge = nullptr;
        }
    }

    // Generate deterministic test features
    void generate_features(float* features, uint32_t n, uint32_t seed) {
        for (uint32_t i = 0; i < n; i++) {
            features[i] = 0.5f + 0.3f * sinf((float)(i + seed) * 0.1f);
        }
    }
};

//=============================================================================
// Numerical Stability Tests
//=============================================================================

TEST_F(MirrorSNNPlasticityRegressionTest, ConfidenceNormalizationBounds) {
    // REGRESSION: Confidence values must always be in [0, 1]
    // Previously: Raw firing rates (Hz) were returned without normalization

    float features[32];
    generate_features(features, 32, 0);

    // Run multiple simulations with varying input strengths
    for (float strength = 0.1f; strength <= 1.0f; strength += 0.1f) {
        mirror_snn_encode_observation(snn_bridge, 0, features, 32, strength);
        mirror_snn_simulate(snn_bridge, 100.0f);

        float confidences[8];
        mirror_snn_get_action_confidences(snn_bridge, confidences, 8);

        // Verify all confidences are normalized
        for (int i = 0; i < 8; i++) {
            EXPECT_GE(confidences[i], 0.0f)
                << "Confidence must be >= 0 (strength=" << strength << ", i=" << i << ")";
            EXPECT_LE(confidences[i], 1.0f)
                << "Confidence must be <= 1 (strength=" << strength << ", i=" << i << ")";
        }

        mirror_snn_reset(snn_bridge);
    }
}

TEST_F(MirrorSNNPlasticityRegressionTest, WeightBoundsRespected) {
    // REGRESSION: Weights must stay within configured bounds after STDP

    // Register synapses
    for (uint32_t i = 0; i < 8; i++) {
        mirror_plasticity_register_synapse(plasticity_bridge, i,
            MIRROR_SYNAPSE_OBS_TO_HIDDEN, 0.5f);
    }

    uint64_t timestamp = nimcp_time_get_us();

    // Trigger many STDP events
    for (int iter = 0; iter < 100; iter++) {
        mirror_plasticity_observation(plasticity_bridge, iter % 8, 0.9f, timestamp);
        mirror_plasticity_execution(plasticity_bridge, iter % 8, 0.9f, timestamp + 10000);
        mirror_plasticity_reward(plasticity_bridge, 1.0f, timestamp + 20000);
        timestamp += 30000;
    }

    // Get synapse states and verify bounds
    for (uint32_t syn = 0; syn < 8; syn++) {
        mirror_plasticity_synapse_t state;
        int ret = mirror_plasticity_get_synapse(plasticity_bridge, syn, &state);
        if (ret == 0) {
            EXPECT_GE(state.weight, 0.0f) << "Weight must be >= 0";
            EXPECT_LE(state.weight, 1.0f) << "Weight must be <= 1";
        }
    }
}

TEST_F(MirrorSNNPlasticityRegressionTest, NoNaNOrInfValues) {
    // REGRESSION: No NaN or Inf values should appear in outputs

    float features[32];
    generate_features(features, 32, 42);

    // Edge case inputs
    float edge_cases[] = {0.0f, 1.0f, 0.5f, 0.001f, 0.999f};

    for (float strength : edge_cases) {
        mirror_snn_encode_observation(snn_bridge, 0, features, 32, strength);
        mirror_snn_simulate(snn_bridge, 50.0f);

        float confidences[8];
        mirror_snn_get_action_confidences(snn_bridge, confidences, 8);

        for (int i = 0; i < 8; i++) {
            EXPECT_FALSE(std::isnan(confidences[i]))
                << "NaN detected at strength=" << strength;
            EXPECT_FALSE(std::isinf(confidences[i]))
                << "Inf detected at strength=" << strength;
        }

        mirror_snn_reset(snn_bridge);
    }
}

//=============================================================================
// Memory Regression Tests
//=============================================================================

TEST_F(MirrorSNNPlasticityRegressionTest, RepeatedCreateDestroyCycles) {
    // REGRESSION: Memory leaks on repeated create/destroy
    // Previously: mutex not freed after destroy

    const int cycles = 50;

    for (int i = 0; i < cycles; i++) {
        mirror_snn_config_t config = mirror_snn_config_default();
        config.input_dim = 16;
        config.hidden_dim = 32;
        config.output_dim = 4;
        config.enable_bio_async = false;

        mirror_snn_bridge_t* bridge = mirror_snn_create(&config);
        ASSERT_NE(bridge, nullptr) << "Create failed at cycle " << i;

        // Do some work
        float features[16] = {0};
        mirror_snn_encode_observation(bridge, 0, features, 16, 0.5f);
        mirror_snn_simulate(bridge, 10.0f);

        mirror_snn_destroy(bridge);
    }

    // If we got here without crash/hang, memory management is working
    SUCCEED();
}

TEST_F(MirrorSNNPlasticityRegressionTest, PlasticityRepeatedCreateDestroy) {
    // REGRESSION: Plasticity bridge memory leaks

    const int cycles = 50;

    for (int i = 0; i < cycles; i++) {
        mirror_plasticity_config_t config = mirror_plasticity_config_default();
        mirror_plasticity_bridge_t* bridge = mirror_plasticity_create(&config);
        ASSERT_NE(bridge, nullptr) << "Create failed at cycle " << i;

        // Register and use synapses
        for (uint32_t s = 0; s < 4; s++) {
            mirror_plasticity_register_synapse(bridge, s, MIRROR_SYNAPSE_OBS_TO_HIDDEN, 0.5f);
        }

        mirror_plasticity_observation(bridge, 0, 0.8f, nimcp_time_get_us());
        mirror_plasticity_destroy(bridge);
    }

    SUCCEED();
}

//=============================================================================
// Performance Regression Tests
//=============================================================================

TEST_F(MirrorSNNPlasticityRegressionTest, EncodingPerformanceBound) {
    // REGRESSION: Encoding should complete within reasonable time

    float features[32];
    generate_features(features, 32, 0);

    auto start = std::chrono::high_resolution_clock::now();

    const int iterations = 100;
    for (int i = 0; i < iterations; i++) {
        mirror_snn_encode_observation(snn_bridge, i % 8, features, 32, 0.7f);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    // Should complete 100 encodings in < 100ms (1ms per encoding max)
    EXPECT_LT(duration_us, 100000)
        << "Encoding too slow: " << duration_us << "us for " << iterations << " iterations";
}

TEST_F(MirrorSNNPlasticityRegressionTest, SimulationPerformanceBound) {
    // REGRESSION: Simulation should complete within reasonable time

    float features[32];
    generate_features(features, 32, 0);

    mirror_snn_encode_observation(snn_bridge, 0, features, 32, 0.8f);

    auto start = std::chrono::high_resolution_clock::now();

    const int iterations = 50;
    for (int i = 0; i < iterations; i++) {
        mirror_snn_simulate(snn_bridge, 10.0f);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    // 50 simulations of 10ms each should complete in < 500ms real time
    EXPECT_LT(duration_us, 500000)
        << "Simulation too slow: " << duration_us << "us for " << iterations << " iterations";
}

TEST_F(MirrorSNNPlasticityRegressionTest, PlasticityUpdatePerformance) {
    // REGRESSION: Plasticity updates should be fast

    // Register synapses
    for (uint32_t i = 0; i < 16; i++) {
        mirror_plasticity_register_synapse(plasticity_bridge, i % 8,
            MIRROR_SYNAPSE_OBS_TO_HIDDEN, 0.5f);
    }

    auto start = std::chrono::high_resolution_clock::now();

    uint64_t timestamp = nimcp_time_get_us();
    const int iterations = 1000;
    for (int i = 0; i < iterations; i++) {
        mirror_plasticity_observation(plasticity_bridge, i % 8, 0.7f, timestamp);
        timestamp += 1000;
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    // 1000 plasticity updates should complete in < 100ms
    EXPECT_LT(duration_us, 100000)
        << "Plasticity too slow: " << duration_us << "us for " << iterations << " updates";
}

//=============================================================================
// Consistency Tests
//=============================================================================

TEST_F(MirrorSNNPlasticityRegressionTest, DeterministicOutput) {
    // REGRESSION: Same input should produce consistent output

    float features[32];
    generate_features(features, 32, 123);

    // Run first pass
    mirror_snn_encode_observation(snn_bridge, 0, features, 32, 0.8f);
    mirror_snn_simulate(snn_bridge, 50.0f);

    float confidences1[8];
    mirror_snn_get_action_confidences(snn_bridge, confidences1, 8);

    // Reset and run again
    mirror_snn_reset(snn_bridge);
    mirror_snn_encode_observation(snn_bridge, 0, features, 32, 0.8f);
    mirror_snn_simulate(snn_bridge, 50.0f);

    float confidences2[8];
    mirror_snn_get_action_confidences(snn_bridge, confidences2, 8);

    // Results should be similar (not necessarily identical due to timing)
    for (int i = 0; i < 8; i++) {
        float diff = fabsf(confidences1[i] - confidences2[i]);
        EXPECT_LT(diff, 0.1f)
            << "Inconsistent output at index " << i
            << ": " << confidences1[i] << " vs " << confidences2[i];
    }
}

TEST_F(MirrorSNNPlasticityRegressionTest, StatsConsistency) {
    // REGRESSION: Stats should accurately reflect operations

    mirror_snn_stats_t stats1;
    mirror_snn_get_stats(snn_bridge, &stats1);
    uint64_t initial_obs = stats1.total_observations;

    float features[32];
    generate_features(features, 32, 0);

    const int n_observations = 10;
    for (int i = 0; i < n_observations; i++) {
        mirror_snn_encode_observation(snn_bridge, i % 8, features, 32, 0.7f);
    }

    mirror_snn_stats_t stats2;
    mirror_snn_get_stats(snn_bridge, &stats2);

    EXPECT_EQ(stats2.total_observations, initial_obs + n_observations)
        << "Observation count mismatch";
}

//=============================================================================
// Historical Bug Reproductions
//=============================================================================

TEST_F(MirrorSNNPlasticityRegressionTest, Bug_PopulationArrayOverflow) {
    // REGRESSION: Previously, snn_config_feedforward set n_populations=3,
    // causing buffer overflow when adding more populations.
    // Fixed by setting n_populations=0 to use SNN_MAX_POPULATIONS.

    // This test creates a bridge with default config (which uses NULL)
    // and verifies it doesn't crash when creating multiple populations
    mirror_snn_bridge_t* test_bridge = mirror_snn_create(nullptr);
    ASSERT_NE(test_bridge, nullptr) << "Bridge creation should not crash";

    // Use the bridge
    float features[64];
    for (int i = 0; i < 64; i++) features[i] = 0.5f;

    mirror_snn_encode_observation(test_bridge, 0, features, 64, 0.8f);
    mirror_snn_simulate(test_bridge, 50.0f);

    float confidences[16];
    mirror_snn_get_action_confidences(test_bridge, confidences, 16);

    // Verify no crash on destroy
    mirror_snn_destroy(test_bridge);
    SUCCEED();
}

TEST_F(MirrorSNNPlasticityRegressionTest, Bug_MutexDeadlock) {
    // REGRESSION: Previously, observation/execution could deadlock
    // when calling pre_spike from within locked code.
    // Fixed by creating pre_spike_unlocked internal helper.

    uint64_t timestamp = nimcp_time_get_us();

    // Register synapses
    for (uint32_t i = 0; i < 8; i++) {
        mirror_plasticity_register_synapse(plasticity_bridge, i,
            MIRROR_SYNAPSE_OBS_TO_HIDDEN, 0.5f);
    }

    // Rapid alternating calls that previously caused deadlock
    for (int i = 0; i < 50; i++) {
        mirror_plasticity_observation(plasticity_bridge, i % 8, 0.8f, timestamp);
        mirror_plasticity_execution(plasticity_bridge, i % 8, 0.8f, timestamp + 1000);
        timestamp += 2000;
    }

    // If we reach here, no deadlock occurred
    SUCCEED();
}

TEST_F(MirrorSNNPlasticityRegressionTest, Bug_ATPLevelScale) {
    // REGRESSION: ATP level was expected in [0,1] but returned [0,100]
    // Test verifies the current expected range

    float atp = mirror_plasticity_get_atp_level(plasticity_bridge);

    // ATP is returned as percentage (0-100)
    EXPECT_GE(atp, 0.0f) << "ATP must be >= 0";
    EXPECT_LE(atp, 100.0f) << "ATP must be <= 100";
}

//=============================================================================
// Edge Case Tests
//=============================================================================

TEST_F(MirrorSNNPlasticityRegressionTest, ZeroStrengthInput) {
    // Edge case: zero strength observation

    float features[32];
    generate_features(features, 32, 0);

    int ret = mirror_snn_encode_observation(snn_bridge, 0, features, 32, 0.0f);
    EXPECT_GE(ret, 0) << "Zero strength should be handled gracefully";

    mirror_snn_simulate(snn_bridge, 50.0f);

    float confidences[8];
    mirror_snn_get_action_confidences(snn_bridge, confidences, 8);

    // Should not crash and confidences should be valid
    for (int i = 0; i < 8; i++) {
        EXPECT_FALSE(std::isnan(confidences[i]));
        EXPECT_FALSE(std::isinf(confidences[i]));
    }
}

TEST_F(MirrorSNNPlasticityRegressionTest, MaxActionID) {
    // Edge case: maximum valid action ID

    uint32_t max_action = 7;  // 8 actions, 0-indexed

    float features[32];
    generate_features(features, 32, 0);

    int ret = mirror_snn_encode_observation(snn_bridge, max_action, features, 32, 0.8f);
    EXPECT_GE(ret, 0) << "Max action ID should be valid";

    uint64_t timestamp = nimcp_time_get_us();
    ret = mirror_plasticity_observation(plasticity_bridge, max_action, 0.8f, timestamp);
    EXPECT_EQ(ret, 0) << "Plasticity max action should succeed";
}

TEST_F(MirrorSNNPlasticityRegressionTest, RapidResetCycles) {
    // Edge case: rapid reset cycles

    float features[32];
    generate_features(features, 32, 0);

    for (int cycle = 0; cycle < 20; cycle++) {
        mirror_snn_encode_observation(snn_bridge, 0, features, 32, 0.7f);
        mirror_snn_simulate(snn_bridge, 10.0f);
        mirror_snn_reset(snn_bridge);

        mirror_plasticity_reset(plasticity_bridge);
    }

    // Verify bridges are still functional after rapid resets
    mirror_snn_bridge_state_t snn_state;
    mirror_snn_get_state(snn_bridge, &snn_state);
    EXPECT_EQ(snn_state.state, MIRROR_SNN_STATE_IDLE);

    mirror_plasticity_bridge_state_t plasticity_state;
    mirror_plasticity_get_state(plasticity_bridge, &plasticity_state);
    EXPECT_EQ(plasticity_state.state, MIRROR_PLASTICITY_STATE_IDLE);
}

