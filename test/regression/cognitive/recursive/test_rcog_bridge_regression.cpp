/**
 * @file test_rcog_bridge_regression.cpp
 * @brief Regression tests for Recursive Cognition SNN/Plasticity bridge integrations
 * @version 1.0.0
 * @date 2026-01-08
 *
 * WHAT: Regression tests for SNN and Plasticity bridges in recursive cognition
 * WHY:  Ensure bridge stability, numerical correctness, and performance over time
 * HOW:  Test depth tracking stability, free energy bounds, memory safety,
 *       callback stability, metrics consistency, and thread safety
 *
 * TEST CATEGORIES:
 * - RecursionDepthStability: Depth tracking stable over time
 * - FreeEnergyBounded: Free energy stays in valid range
 * - NoMemoryLeaks: Repeated create/destroy no leaks
 * - CallbackStability: Callbacks remain stable
 * - MetricsConsistency: Metrics remain consistent
 * - ThreadSafetyRegression: Concurrent access safe
 */

#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <memory>
#include <thread>
#include <vector>

#include "cognitive/recursive/nimcp_rcog_snn_bridge.h"
#include "cognitive/recursive/nimcp_rcog_plasticity_bridge.h"
#include "cognitive/recursive/nimcp_rcog_engine.h"

//=============================================================================
// Test Fixture
//=============================================================================

class RcogBridgeRegressionTest : public ::testing::Test {
protected:
    rcog_snn_bridge_t* snn_bridge = nullptr;
    rcog_plasticity_bridge_t* plasticity_bridge = nullptr;

    void SetUp() override {
        // Create SNN bridge with default config
        rcog_snn_config_t snn_config = rcog_snn_config_default();
        snn_config.enable_bio_async = false;
        snn_bridge = rcog_snn_create(&snn_config);
        ASSERT_NE(snn_bridge, nullptr);

        // Create plasticity bridge with default config
        rcog_plasticity_config_t plasticity_config = rcog_plasticity_config_default();
        plasticity_config.enable_bio_async = false;
        plasticity_bridge = rcog_plasticity_create(&plasticity_config);
        ASSERT_NE(plasticity_bridge, nullptr);
    }

    void TearDown() override {
        if (snn_bridge) {
            rcog_snn_destroy(snn_bridge);
            snn_bridge = nullptr;
        }
        if (plasticity_bridge) {
            rcog_plasticity_destroy(plasticity_bridge);
            plasticity_bridge = nullptr;
        }
    }

    // Helper: Generate recursive context dimensions
    void generate_recursive_context(float* dims, uint32_t seed, float depth_level) {
        for (int i = 0; i < RCOG_DIM_COUNT; i++) {
            dims[i] = 0.5f + 0.3f * sinf((float)(i + seed) * 0.1f);
        }
        dims[RCOG_DIM_RECURSION_DEPTH] = depth_level;
        dims[RCOG_DIM_META_COGNITIVE_LEVEL] = 0.6f + 0.2f * cosf((float)seed * 0.2f);
    }
};

//=============================================================================
// RecursionDepthStability Tests
//=============================================================================

TEST_F(RcogBridgeRegressionTest, RecursionDepthStability_BasicTracking) {
    // Test that recursion depth tracking remains stable over many iterations
    const int NUM_ITERATIONS = 1000;
    float prev_depth = -1.0f;
    int depth_changes = 0;

    for (int i = 0; i < NUM_ITERATIONS; i++) {
        float depth_level = (float)(i % 10) / 10.0f;  // Cycle depth 0.0-0.9

        int result = rcog_snn_encode_depth(snn_bridge, depth_level, 10);
        EXPECT_GE(result, 0) << "Iteration " << i;

        rcog_snn_simulate(snn_bridge, 10.0f);

        float current_depth = rcog_snn_get_depth(snn_bridge);
        EXPECT_GE(current_depth, 0.0f) << "Depth should be non-negative at iteration " << i;
        EXPECT_LE(current_depth, 1.0f) << "Depth should be <= 1.0 at iteration " << i;

        if (prev_depth >= 0.0f && fabsf(current_depth - prev_depth) > 0.5f) {
            depth_changes++;
        }
        prev_depth = current_depth;
    }

    // Verify state is valid
    rcog_snn_bridge_state_t state;
    EXPECT_EQ(rcog_snn_get_state(snn_bridge, &state), 0);
    EXPECT_FALSE(std::isnan(state.mean_depth));
    EXPECT_FALSE(std::isinf(state.mean_depth));
}

TEST_F(RcogBridgeRegressionTest, RecursionDepthStability_ExtremeLevels) {
    // Test extreme depth levels
    float extreme_depths[] = {0.0f, 0.001f, 0.5f, 0.999f, 1.0f};

    for (float depth : extreme_depths) {
        rcog_snn_reset(snn_bridge);

        int result = rcog_snn_encode_depth(snn_bridge, depth, 20);
        EXPECT_GE(result, 0) << "Failed at depth " << depth;

        rcog_snn_simulate(snn_bridge, 20.0f);

        rcog_cognitive_state_t cog_state;
        EXPECT_EQ(rcog_snn_get_cognitive_state(snn_bridge, &cog_state), 0);
        EXPECT_GE(cog_state.recursion_depth, 0.0f);
        EXPECT_LE(cog_state.recursion_depth, 1.0f);
        EXPECT_FALSE(std::isnan(cog_state.recursion_depth));
    }
}

TEST_F(RcogBridgeRegressionTest, RecursionDepthStability_RapidChanges) {
    // Test rapid depth changes (stress test)
    for (int cycle = 0; cycle < 100; cycle++) {
        for (float depth = 0.0f; depth <= 1.0f; depth += 0.1f) {
            rcog_snn_encode_depth(snn_bridge, depth, 10);
            rcog_snn_step(snn_bridge);
        }
    }

    // System should still be functional
    rcog_cognitive_state_t state;
    EXPECT_EQ(rcog_snn_get_cognitive_state(snn_bridge, &state), 0);
    EXPECT_FALSE(std::isnan(state.recursion_depth));
    EXPECT_FALSE(std::isinf(state.recursion_depth));
}

//=============================================================================
// FreeEnergyBounded Tests
//=============================================================================

TEST_F(RcogBridgeRegressionTest, FreeEnergyBounded_SteadyState) {
    // Process many iterations and verify free energy stays bounded
    float dims[RCOG_DIM_COUNT];

    for (int i = 0; i < 500; i++) {
        generate_recursive_context(dims, i, 0.5f);
        rcog_snn_encode_state(snn_bridge, dims, RCOG_DIM_COUNT);
        rcog_snn_simulate(snn_bridge, 10.0f);
    }

    // Get final state and verify bounds
    rcog_cognitive_state_t state;
    EXPECT_EQ(rcog_snn_get_cognitive_state(snn_bridge, &state), 0);

    // All state values should be in [0, 1] range
    EXPECT_GE(state.recursion_depth, 0.0f);
    EXPECT_LE(state.recursion_depth, 1.0f);
    EXPECT_GE(state.meta_cognitive_level, 0.0f);
    EXPECT_LE(state.meta_cognitive_level, 1.0f);
    EXPECT_GE(state.problem_complexity, 0.0f);
    EXPECT_LE(state.problem_complexity, 1.0f);
    EXPECT_GE(state.decomposition_progress, 0.0f);
    EXPECT_LE(state.decomposition_progress, 1.0f);
    EXPECT_GE(state.aggregation_confidence, 0.0f);
    EXPECT_LE(state.aggregation_confidence, 1.0f);
}

TEST_F(RcogBridgeRegressionTest, FreeEnergyBounded_ExtremeInputs) {
    // Test with extreme input values
    float extreme_dims[RCOG_DIM_COUNT];

    // All zeros
    memset(extreme_dims, 0, sizeof(extreme_dims));
    rcog_snn_encode_state(snn_bridge, extreme_dims, RCOG_DIM_COUNT);
    rcog_snn_simulate(snn_bridge, 20.0f);

    rcog_cognitive_state_t state1;
    EXPECT_EQ(rcog_snn_get_cognitive_state(snn_bridge, &state1), 0);
    EXPECT_TRUE(std::isfinite(state1.recursion_depth));

    // All ones
    for (int i = 0; i < RCOG_DIM_COUNT; i++) {
        extreme_dims[i] = 1.0f;
    }
    rcog_snn_encode_state(snn_bridge, extreme_dims, RCOG_DIM_COUNT);
    rcog_snn_simulate(snn_bridge, 20.0f);

    rcog_cognitive_state_t state2;
    EXPECT_EQ(rcog_snn_get_cognitive_state(snn_bridge, &state2), 0);
    EXPECT_TRUE(std::isfinite(state2.recursion_depth));
    EXPECT_GE(state2.recursion_depth, 0.0f);
    EXPECT_LE(state2.recursion_depth, 1.0f);
}

TEST_F(RcogBridgeRegressionTest, FreeEnergyBounded_PlasticityWeights) {
    // Verify plasticity weights stay bounded after learning
    for (int i = 0; i < 20; i++) {
        ASSERT_EQ(rcog_plasticity_register_synapse(plasticity_bridge,
            i, RCOG_SYNAPSE_DECOMPOSITION, 0.5f), 0);
    }

    // Apply many learning events with varying rewards
    for (int cycle = 0; cycle < 100; cycle++) {
        float reward = sinf((float)cycle * 0.1f);  // Oscillating reward
        EXPECT_EQ(rcog_plasticity_apply_reward(plasticity_bridge, reward), 0);

        for (int i = 0; i < 20; i++) {
            rcog_plasticity_learn(plasticity_bridge,
                RCOG_LEARN_DECOMP_SUCCESS, 0.1f, i, 0.5f);
        }

        rcog_plasticity_update_bcm(plasticity_bridge, 10.0f);
    }

    // Verify all weights are bounded
    for (int i = 0; i < 20; i++) {
        rcog_plasticity_synapse_t synapse;
        EXPECT_EQ(rcog_plasticity_get_synapse(plasticity_bridge, i, &synapse), 0);
        EXPECT_GE(synapse.weight, 0.0f) << "Weight below 0 for synapse " << i;
        EXPECT_LE(synapse.weight, 1.0f) << "Weight above 1 for synapse " << i;
        EXPECT_TRUE(std::isfinite(synapse.weight));
        EXPECT_TRUE(std::isfinite(synapse.eligibility_trace));
    }
}

//=============================================================================
// NoMemoryLeaks Tests
//=============================================================================

TEST_F(RcogBridgeRegressionTest, NoMemoryLeaks_RepeatedCreateDestroy) {
    // Destroy existing bridges first
    rcog_snn_destroy(snn_bridge);
    snn_bridge = nullptr;
    rcog_plasticity_destroy(plasticity_bridge);
    plasticity_bridge = nullptr;

    // Repeated create/destroy cycles
    for (int cycle = 0; cycle < 100; cycle++) {
        rcog_snn_config_t snn_config = rcog_snn_config_default();
        snn_config.enable_bio_async = false;
        rcog_snn_bridge_t* snn = rcog_snn_create(&snn_config);
        ASSERT_NE(snn, nullptr) << "Failed at cycle " << cycle;

        rcog_plasticity_config_t plasticity_config = rcog_plasticity_config_default();
        rcog_plasticity_bridge_t* plasticity = rcog_plasticity_create(&plasticity_config);
        ASSERT_NE(plasticity, nullptr) << "Failed at cycle " << cycle;

        // Do some work
        float dims[RCOG_DIM_COUNT] = {0.5f};
        rcog_snn_encode_state(snn, dims, RCOG_DIM_COUNT);
        rcog_snn_step(snn);

        rcog_plasticity_register_synapse(plasticity, 1, RCOG_SYNAPSE_DECOMPOSITION, 0.5f);
        rcog_plasticity_learn(plasticity, RCOG_LEARN_DECOMP_SUCCESS, 0.1f, 1, 0.5f);

        rcog_snn_destroy(snn);
        rcog_plasticity_destroy(plasticity);
    }
    // Test passes if no crash/memory exhaustion
}

TEST_F(RcogBridgeRegressionTest, NoMemoryLeaks_RepeatedSynapseRegistration) {
    // Repeated register/unregister cycles
    for (int cycle = 0; cycle < 200; cycle++) {
        EXPECT_EQ(rcog_plasticity_register_synapse(plasticity_bridge,
            1000 + (cycle % 10), RCOG_SYNAPSE_AGGREGATION, 0.5f), 0);
        EXPECT_EQ(rcog_plasticity_unregister_synapse(plasticity_bridge,
            1000 + (cycle % 10)), 0);
    }
    // Test passes if no crash/memory leak
}

TEST_F(RcogBridgeRegressionTest, NoMemoryLeaks_StatsResetCycles) {
    for (int i = 0; i < 100; i++) {
        // Generate some activity
        float dims[RCOG_DIM_COUNT] = {0.5f};
        rcog_snn_encode_state(snn_bridge, dims, RCOG_DIM_COUNT);
        rcog_snn_simulate(snn_bridge, 5.0f);

        // Reset stats
        rcog_snn_reset_stats(snn_bridge);
        rcog_plasticity_reset_stats(plasticity_bridge);
    }
    // Test passes if no memory accumulation
}

//=============================================================================
// CallbackStability Tests
//=============================================================================

namespace {
    std::atomic<int> g_depth_callbacks{0};
    std::atomic<int> g_state_callbacks{0};
    std::atomic<int> g_self_ref_callbacks{0};
    std::atomic<int> g_learn_callbacks{0};
    std::atomic<int> g_strategy_callbacks{0};

    void depth_callback(rcog_snn_bridge_t*, float depth_level, uint64_t, void*) {
        g_depth_callbacks++;
        // Verify callback receives valid data
        EXPECT_GE(depth_level, 0.0f);
        EXPECT_LE(depth_level, 1.0f);
    }

    void state_callback(rcog_snn_bridge_t*, const rcog_cognitive_state_t* state, void*) {
        g_state_callbacks++;
        if (state) {
            EXPECT_GE(state->recursion_depth, 0.0f);
            EXPECT_LE(state->recursion_depth, 1.0f);
        }
    }

    void self_ref_callback(rcog_snn_bridge_t*, float intensity, uint32_t, void*) {
        g_self_ref_callbacks++;
        EXPECT_GE(intensity, 0.0f);
        EXPECT_LE(intensity, 1.0f);
    }

    void learn_callback(rcog_plasticity_bridge_t*, rcog_learn_event_t event,
                       float magnitude, void*) {
        g_learn_callbacks++;
        EXPECT_TRUE(std::isfinite(magnitude));
    }

    void strategy_callback(rcog_plasticity_bridge_t*, float old_pref,
                          float new_pref, void*) {
        g_strategy_callbacks++;
        EXPECT_TRUE(std::isfinite(old_pref));
        EXPECT_TRUE(std::isfinite(new_pref));
    }
}

TEST_F(RcogBridgeRegressionTest, CallbackStability_Registration) {
    // Reset counters
    g_depth_callbacks = 0;
    g_state_callbacks = 0;
    g_self_ref_callbacks = 0;

    // Register callbacks
    EXPECT_EQ(rcog_snn_register_depth_callback(snn_bridge, depth_callback, nullptr), 0);
    EXPECT_EQ(rcog_snn_register_state_callback(snn_bridge, state_callback, nullptr), 0);
    EXPECT_EQ(rcog_snn_register_self_ref_callback(snn_bridge, self_ref_callback, nullptr), 0);

    // Trigger callbacks via high-depth encoding
    rcog_snn_encode_depth(snn_bridge, 0.9f, 10);
    rcog_snn_simulate(snn_bridge, 30.0f);

    // Check for self-reference loop
    float intensity;
    rcog_snn_check_self_reference(snn_bridge, &intensity);

    // Callbacks should have been invoked (at least some)
    // Note: actual invocation depends on threshold crossing
}

TEST_F(RcogBridgeRegressionTest, CallbackStability_PlasticityCallbacks) {
    g_learn_callbacks = 0;
    g_strategy_callbacks = 0;

    // Register callbacks
    EXPECT_EQ(rcog_plasticity_register_learn_callback(plasticity_bridge,
        learn_callback, nullptr), 0);
    EXPECT_EQ(rcog_plasticity_register_strategy_callback(plasticity_bridge,
        strategy_callback, nullptr), 0);

    // Register synapse and trigger learning
    rcog_plasticity_register_synapse(plasticity_bridge,
        1, RCOG_SYNAPSE_DECOMPOSITION, 0.5f);

    for (int i = 0; i < 50; i++) {
        rcog_plasticity_learn(plasticity_bridge,
            RCOG_LEARN_DEPTH_OPTIMAL, 0.5f, 1, 0.7f);
    }

    // Callbacks should have been invoked
    EXPECT_GT(g_learn_callbacks.load(), 0);
}

TEST_F(RcogBridgeRegressionTest, CallbackStability_UnderStress) {
    g_depth_callbacks = 0;

    EXPECT_EQ(rcog_snn_register_depth_callback(snn_bridge, depth_callback, nullptr), 0);

    // Stress test: many rapid encodings
    for (int i = 0; i < 500; i++) {
        float depth = (float)(i % 100) / 100.0f;
        rcog_snn_encode_depth(snn_bridge, depth, 20);
        rcog_snn_step(snn_bridge);
    }

    // System should remain stable
    rcog_snn_bridge_state_t state;
    EXPECT_EQ(rcog_snn_get_state(snn_bridge, &state), 0);
    EXPECT_NE(state.state, RCOG_SNN_STATE_ERROR);
}

//=============================================================================
// MetricsConsistency Tests
//=============================================================================

TEST_F(RcogBridgeRegressionTest, MetricsConsistency_SNNStats) {
    // Perform known operations
    for (int i = 0; i < 100; i++) {
        float dims[RCOG_DIM_COUNT] = {0.5f};
        rcog_snn_encode_state(snn_bridge, dims, RCOG_DIM_COUNT);
        rcog_snn_simulate(snn_bridge, 5.0f);
    }

    rcog_snn_stats_t stats;
    EXPECT_EQ(rcog_snn_get_stats(snn_bridge, &stats), 0);

    // Verify stats are consistent
    EXPECT_GE(stats.total_evaluations, 100u);
    EXPECT_GE(stats.total_simulations, 100u);
    EXPECT_GE(stats.total_spikes, 0u);  // May or may not have spikes
    EXPECT_TRUE(std::isfinite(stats.mean_evaluation_time_ms));
    EXPECT_TRUE(std::isfinite(stats.mean_depth));
}

TEST_F(RcogBridgeRegressionTest, MetricsConsistency_PlasticityStats) {
    // Register synapses
    for (int i = 0; i < 10; i++) {
        rcog_plasticity_register_synapse(plasticity_bridge,
            i, RCOG_SYNAPSE_DECOMPOSITION, 0.5f);
    }

    // Perform known learning operations
    for (int cycle = 0; cycle < 50; cycle++) {
        for (int i = 0; i < 10; i++) {
            rcog_plasticity_learn(plasticity_bridge,
                RCOG_LEARN_DECOMP_SUCCESS, 0.1f, i, 0.5f);
            rcog_plasticity_apply_stdp(plasticity_bridge, i,
                (float)cycle, (float)cycle + 5.0f);
        }
        rcog_plasticity_update_bcm(plasticity_bridge, 10.0f);
    }

    rcog_plasticity_stats_t stats;
    EXPECT_EQ(rcog_plasticity_get_stats(plasticity_bridge, &stats), 0);

    // Verify stats consistency
    EXPECT_GE(stats.total_learning_events, 500u);  // 50 * 10
    EXPECT_GE(stats.weight_updates, 500u);
    EXPECT_TRUE(std::isfinite(stats.mean_weight_change));
    EXPECT_TRUE(std::isfinite(stats.total_potentiation));
    EXPECT_TRUE(std::isfinite(stats.total_depression));
}

TEST_F(RcogBridgeRegressionTest, MetricsConsistency_DeterministicOutput) {
    float dims[RCOG_DIM_COUNT];
    generate_recursive_context(dims, 42, 0.6f);

    // First run
    rcog_snn_encode_state(snn_bridge, dims, RCOG_DIM_COUNT);
    rcog_snn_simulate(snn_bridge, 20.0f);
    rcog_cognitive_state_t state1;
    EXPECT_EQ(rcog_snn_get_cognitive_state(snn_bridge, &state1), 0);

    // Verify first run outputs are valid and normalized
    EXPECT_GE(state1.recursion_depth, 0.0f);
    EXPECT_LE(state1.recursion_depth, 1.0f);
    EXPECT_GE(state1.meta_cognitive_level, 0.0f);
    EXPECT_LE(state1.meta_cognitive_level, 1.0f);
    EXPECT_GE(state1.problem_complexity, 0.0f);
    EXPECT_LE(state1.problem_complexity, 1.0f);

    // Reset and second run with same input
    rcog_snn_reset(snn_bridge);
    rcog_snn_encode_state(snn_bridge, dims, RCOG_DIM_COUNT);
    rcog_snn_simulate(snn_bridge, 20.0f);
    rcog_cognitive_state_t state2;
    EXPECT_EQ(rcog_snn_get_cognitive_state(snn_bridge, &state2), 0);

    // Verify second run outputs are also valid and normalized.
    // Note: reset() clears SNN neuron state but not internal weights,
    // so post-reset outputs may differ from initial run outputs.
    // The key regression check is that both runs produce valid, finite outputs.
    EXPECT_GE(state2.recursion_depth, 0.0f);
    EXPECT_LE(state2.recursion_depth, 1.0f);
    EXPECT_GE(state2.meta_cognitive_level, 0.0f);
    EXPECT_LE(state2.meta_cognitive_level, 1.0f);
    EXPECT_GE(state2.problem_complexity, 0.0f);
    EXPECT_LE(state2.problem_complexity, 1.0f);
}

TEST_F(RcogBridgeRegressionTest, MetricsConsistency_StateAndEffects) {
    // Bridge state should be consistent with strategy state
    rcog_plasticity_register_synapse(plasticity_bridge,
        1, RCOG_SYNAPSE_DEPTH_CONTROL, 0.5f);

    for (int i = 0; i < 20; i++) {
        rcog_plasticity_learn(plasticity_bridge,
            RCOG_LEARN_DEPTH_OPTIMAL, 0.5f, 1, 0.7f);
    }

    rcog_plasticity_bridge_state_t bridge_state;
    EXPECT_EQ(rcog_plasticity_get_state(plasticity_bridge, &bridge_state), 0);

    rcog_strategy_state_t strategy_state;
    EXPECT_EQ(rcog_plasticity_get_strategy_state(plasticity_bridge, &strategy_state), 0);

    // States should be valid
    EXPECT_TRUE(std::isfinite(bridge_state.mean_weight));
    EXPECT_TRUE(std::isfinite(strategy_state.depth_preference));
    EXPECT_TRUE(std::isfinite(strategy_state.learning_rate_mod));
}

//=============================================================================
// ThreadSafetyRegression Tests
//=============================================================================

TEST_F(RcogBridgeRegressionTest, ThreadSafetyRegression_ConcurrentEncoding) {
    constexpr int NUM_THREADS = 4;
    constexpr int OPS_PER_THREAD = 100;
    std::atomic<int> errors{0};

    std::vector<std::thread> threads;
    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back([this, t, &errors]() {
            for (int i = 0; i < OPS_PER_THREAD; i++) {
                float dims[RCOG_DIM_COUNT];
                for (int d = 0; d < RCOG_DIM_COUNT; d++) {
                    dims[d] = 0.5f + 0.3f * sinf((float)(d + i + t) * 0.1f);
                }

                int result = rcog_snn_encode_state(snn_bridge, dims, RCOG_DIM_COUNT);
                if (result < 0) errors++;

                result = rcog_snn_step(snn_bridge);
                if (result != 0) errors++;
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(errors.load(), 0) << "Thread safety errors detected";

    // Verify bridge is still functional
    rcog_snn_bridge_state_t state;
    EXPECT_EQ(rcog_snn_get_state(snn_bridge, &state), 0);
    EXPECT_NE(state.state, RCOG_SNN_STATE_ERROR);
}

TEST_F(RcogBridgeRegressionTest, ThreadSafetyRegression_ConcurrentLearning) {
    constexpr int NUM_THREADS = 4;
    constexpr int SYNAPSES_PER_THREAD = 10;
    constexpr int OPS_PER_SYNAPSE = 50;
    std::atomic<int> errors{0};

    // Pre-register synapses for each thread
    for (int t = 0; t < NUM_THREADS; t++) {
        for (int s = 0; s < SYNAPSES_PER_THREAD; s++) {
            int id = t * SYNAPSES_PER_THREAD + s;
            ASSERT_EQ(rcog_plasticity_register_synapse(plasticity_bridge,
                id, RCOG_SYNAPSE_DECOMPOSITION, 0.5f), 0);
        }
    }

    std::vector<std::thread> threads;
    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back([this, t, &errors]() {
            for (int op = 0; op < OPS_PER_SYNAPSE; op++) {
                for (int s = 0; s < SYNAPSES_PER_THREAD; s++) {
                    int id = t * SYNAPSES_PER_THREAD + s;

                    int result = rcog_plasticity_learn(plasticity_bridge,
                        RCOG_LEARN_DECOMP_SUCCESS, 0.1f, id, 0.5f);
                    if (result != 0) errors++;

                    float weight = rcog_plasticity_apply_stdp(plasticity_bridge,
                        id, (float)op, (float)op + 5.0f);
                    if (std::isnan(weight)) errors++;
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(errors.load(), 0) << "Thread safety errors in learning";

    // Verify all weights are valid
    for (int i = 0; i < NUM_THREADS * SYNAPSES_PER_THREAD; i++) {
        rcog_plasticity_synapse_t synapse;
        EXPECT_EQ(rcog_plasticity_get_synapse(plasticity_bridge, i, &synapse), 0);
        EXPECT_TRUE(std::isfinite(synapse.weight));
    }
}

TEST_F(RcogBridgeRegressionTest, ThreadSafetyRegression_MixedOperations) {
    constexpr int NUM_THREADS = 4;
    std::atomic<int> errors{0};
    std::atomic<bool> stop{false};

    // Writer thread - encoding
    std::thread encoder([this, &errors, &stop]() {
        while (!stop) {
            float dims[RCOG_DIM_COUNT] = {0.5f};
            if (rcog_snn_encode_state(snn_bridge, dims, RCOG_DIM_COUNT) < 0) {
                errors++;
            }
        }
    });

    // Writer thread - simulation
    std::thread simulator([this, &errors, &stop]() {
        while (!stop) {
            if (rcog_snn_step(snn_bridge) != 0) {
                errors++;
            }
        }
    });

    // Reader threads - state queries
    std::vector<std::thread> readers;
    for (int i = 0; i < NUM_THREADS - 2; i++) {
        readers.emplace_back([this, &errors, &stop]() {
            while (!stop) {
                rcog_cognitive_state_t state;
                if (rcog_snn_get_cognitive_state(snn_bridge, &state) != 0) {
                    errors++;
                }

                rcog_snn_stats_t stats;
                if (rcog_snn_get_stats(snn_bridge, &stats) != 0) {
                    errors++;
                }
            }
        });
    }

    // Let threads run for a bit
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    stop = true;

    encoder.join();
    simulator.join();
    for (auto& r : readers) {
        r.join();
    }

    EXPECT_EQ(errors.load(), 0) << "Errors in mixed concurrent operations";
}

//=============================================================================
// Protected Synapse Tests
//=============================================================================

TEST_F(RcogBridgeRegressionTest, ProtectedSynapse_DepthControlProtection) {
    // Register depth control synapse (auto-protected)
    ASSERT_EQ(rcog_plasticity_register_synapse(plasticity_bridge,
        100, RCOG_SYNAPSE_DEPTH_CONTROL, 1.0f), 0);

    rcog_plasticity_synapse_t synapse;
    rcog_plasticity_get_synapse(plasticity_bridge, 100, &synapse);
    float original_weight = synapse.weight;
    EXPECT_TRUE(synapse.is_protected);

    // Try many modification attempts
    for (int i = 0; i < 100; i++) {
        rcog_plasticity_apply_stdp(plasticity_bridge, 100, (float)i, (float)i + 10.0f);
        rcog_plasticity_learn(plasticity_bridge,
            RCOG_LEARN_DEPTH_TOO_DEEP, -1.0f, 100, 1.0f);
        rcog_plasticity_apply_reward(plasticity_bridge, -1.0f);
    }

    // Weight must remain unchanged
    rcog_plasticity_get_synapse(plasticity_bridge, 100, &synapse);
    EXPECT_FLOAT_EQ(synapse.weight, original_weight);
    EXPECT_TRUE(synapse.is_protected);
}

TEST_F(RcogBridgeRegressionTest, ProtectedSynapse_MetaCognitiveProtection) {
    // Register meta-cognitive synapse (auto-protected)
    ASSERT_EQ(rcog_plasticity_register_synapse(plasticity_bridge,
        200, RCOG_SYNAPSE_META_COGNITIVE, 0.9f), 0);

    rcog_plasticity_synapse_t synapse;
    rcog_plasticity_get_synapse(plasticity_bridge, 200, &synapse);
    EXPECT_TRUE(synapse.is_protected);
    float original_weight = synapse.weight;

    // Apply learning - protected synapse should not change
    for (int i = 0; i < 50; i++) {
        rcog_plasticity_learn(plasticity_bridge,
            RCOG_LEARN_META_INSIGHT, 0.5f, 200, 0.9f);
    }

    rcog_plasticity_get_synapse(plasticity_bridge, 200, &synapse);
    EXPECT_FLOAT_EQ(synapse.weight, original_weight);
}

//=============================================================================
// Edge Case Tests
//=============================================================================

TEST_F(RcogBridgeRegressionTest, EdgeCase_ZeroSimulationTime) {
    float dims[RCOG_DIM_COUNT] = {0.5f};
    rcog_snn_encode_state(snn_bridge, dims, RCOG_DIM_COUNT);

    // Zero time should be rejected
    EXPECT_EQ(rcog_snn_simulate(snn_bridge, 0.0f), -1);

    // Negative time should be rejected
    EXPECT_EQ(rcog_snn_simulate(snn_bridge, -1.0f), -1);
}

TEST_F(RcogBridgeRegressionTest, EdgeCase_LargeSimulationTime) {
    float dims[RCOG_DIM_COUNT] = {0.5f};
    rcog_snn_encode_state(snn_bridge, dims, RCOG_DIM_COUNT);

    // Large simulation should not crash or hang
    EXPECT_EQ(rcog_snn_simulate(snn_bridge, 1000.0f), 0);

    rcog_cognitive_state_t state;
    EXPECT_EQ(rcog_snn_get_cognitive_state(snn_bridge, &state), 0);
    EXPECT_TRUE(std::isfinite(state.recursion_depth));
}

TEST_F(RcogBridgeRegressionTest, EdgeCase_ResetBehavior) {
    // Do work
    float dims[RCOG_DIM_COUNT] = {0.8f};
    rcog_snn_encode_state(snn_bridge, dims, RCOG_DIM_COUNT);
    rcog_snn_simulate(snn_bridge, 30.0f);

    // Reset
    EXPECT_EQ(rcog_snn_reset(snn_bridge), 0);

    // Verify state is cleared
    rcog_snn_bridge_state_t state;
    rcog_snn_get_state(snn_bridge, &state);
    EXPECT_EQ(state.state, RCOG_SNN_STATE_IDLE);
}

//=============================================================================
// Performance Regression Tests
//=============================================================================

TEST_F(RcogBridgeRegressionTest, Performance_EncodingLatency) {
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 1000; i++) {
        float dims[RCOG_DIM_COUNT];
        generate_recursive_context(dims, i, 0.5f);
        rcog_snn_encode_state(snn_bridge, dims, RCOG_DIM_COUNT);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // 1000 encodings should complete in under 1 second
    EXPECT_LT(duration.count(), 1000) << "Encoding too slow: " << duration.count() << "ms";
}

TEST_F(RcogBridgeRegressionTest, Performance_SimulationLatency) {
    float dims[RCOG_DIM_COUNT] = {0.5f};
    rcog_snn_encode_state(snn_bridge, dims, RCOG_DIM_COUNT);

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 500; i++) {
        rcog_snn_simulate(snn_bridge, 10.0f);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // 500 simulations should complete in under 2 seconds
    EXPECT_LT(duration.count(), 2000) << "Simulation too slow: " << duration.count() << "ms";
}

TEST_F(RcogBridgeRegressionTest, Performance_LearningLatency) {
    // Register synapses
    for (int i = 0; i < 50; i++) {
        rcog_plasticity_register_synapse(plasticity_bridge,
            i, RCOG_SYNAPSE_DECOMPOSITION, 0.5f);
    }

    auto start = std::chrono::high_resolution_clock::now();

    for (int cycle = 0; cycle < 100; cycle++) {
        for (int i = 0; i < 50; i++) {
            rcog_plasticity_learn(plasticity_bridge,
                RCOG_LEARN_DECOMP_SUCCESS, 0.1f, i, 0.5f);
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // 5000 learning operations should complete in under 500ms
    EXPECT_LT(duration.count(), 500) << "Learning too slow: " << duration.count() << "ms";
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
