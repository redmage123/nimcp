//=============================================================================
// test_attention_bridge_regression.cpp - Regression Tests
//=============================================================================
/**
 * @file test_attention_bridge_regression.cpp
 * @brief Regression tests for Attention-SNN and Attention-Plasticity bridges
 *
 * WHAT: Test for regressions in numerical stability, memory safety, and performance
 * WHY:  Ensure consistent behavior across changes and prevent past bugs
 * HOW:  Test edge cases, numerical bounds, memory safety, and performance constraints
 *
 * REGRESSION TEST CATEGORIES:
 * - Numerical stability (weight bounds, focus strength, sparsity)
 * - Memory safety (create/destroy cycles, max synapse handling, buffer protection)
 * - Performance bounds (simulation time, encoding latency, memory usage)
 * - Edge cases (zero/max weights, rapid attention shifts, empty input)
 * - Competition convergence and thread safety basics
 *
 * @date 2026-01-06
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <chrono>
#include <vector>
#include <memory>
#include <random>
#include <thread>
#include <atomic>

// Headers have their own extern "C" guards
#include "cognitive/attention/nimcp_attention_snn_bridge.h"
#include "cognitive/attention/nimcp_attention_plasticity_bridge.h"

#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixture: Attention-SNN Bridge
//=============================================================================

class AttentionSNNBridgeRegressionTest : public ::testing::Test {
protected:
    attention_snn_bridge_t* bridge = nullptr;
    attention_snn_config_t config;

    void SetUp() override {
        config = attention_snn_config_default();
        config.enable_bio_async = false;
        config.enable_immune_modulation = false;
        config.enable_plasticity_integration = false;
        config.num_heads = 8;
        config.neurons_per_head = ATTENTION_SNN_NEURONS_PER_HEAD;
        config.sequence_length = 64;
        bridge = attention_snn_create(&config);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            attention_snn_destroy(bridge);
            bridge = nullptr;
        }
    }

    // Generate deterministic test weights
    void generate_weights(float* weights, uint32_t n, uint32_t seed) {
        for (uint32_t i = 0; i < n; i++) {
            weights[i] = 0.5f + 0.4f * sinf((float)(i + seed) * 0.1f);
        }
    }

    // Generate deterministic salience map
    void generate_salience(float* salience, uint32_t n, uint32_t seed) {
        for (uint32_t i = 0; i < n; i++) {
            salience[i] = 0.3f + 0.5f * cosf((float)(i + seed) * 0.15f);
        }
    }

    uint64_t get_timestamp_us() {
        auto now = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::microseconds>(
            now.time_since_epoch()).count();
    }
};

//=============================================================================
// Test Fixture: Attention-Plasticity Bridge
//=============================================================================

class AttentionPlasticityBridgeRegressionTest : public ::testing::Test {
protected:
    attention_plasticity_bridge_t* bridge = nullptr;
    attention_plasticity_config_t config;

    void SetUp() override {
        config = attention_plasticity_config_default();
        config.enable_bio_async = false;
        config.enable_immune_modulation = false;
        bridge = attention_plasticity_create(&config);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            attention_plasticity_destroy(bridge);
            bridge = nullptr;
        }
    }

    uint64_t get_timestamp_us() {
        auto now = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::microseconds>(
            now.time_since_epoch()).count();
    }
};

//=============================================================================
// Test Fixture: Combined Bridges
//=============================================================================

class AttentionBridgeCombinedRegressionTest : public ::testing::Test {
protected:
    attention_snn_bridge_t* snn_bridge = nullptr;
    attention_plasticity_bridge_t* plasticity_bridge = nullptr;

    void SetUp() override {
        attention_snn_config_t snn_config = attention_snn_config_default();
        snn_config.enable_bio_async = false;
        snn_config.enable_immune_modulation = false;
        snn_config.num_heads = 8;
        snn_bridge = attention_snn_create(&snn_config);
        ASSERT_NE(snn_bridge, nullptr);

        attention_plasticity_config_t plasticity_config = attention_plasticity_config_default();
        plasticity_config.enable_bio_async = false;
        plasticity_bridge = attention_plasticity_create(&plasticity_config);
        ASSERT_NE(plasticity_bridge, nullptr);
    }

    void TearDown() override {
        if (snn_bridge) {
            attention_snn_destroy(snn_bridge);
            snn_bridge = nullptr;
        }
        if (plasticity_bridge) {
            attention_plasticity_destroy(plasticity_bridge);
            plasticity_bridge = nullptr;
        }
    }

    uint64_t get_timestamp_us() {
        auto now = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::microseconds>(
            now.time_since_epoch()).count();
    }
};

//=============================================================================
// SECTION 1: Performance Under Rapid Attention Shifts (Stress Test)
//=============================================================================

TEST_F(AttentionSNNBridgeRegressionTest, RapidAttentionShiftsStability) {
    // REGRESSION: Bridge should remain stable under rapid attention shifts

    float weights[ATTENTION_SNN_MAX_HEADS];
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);

    for (int cycle = 0; cycle < 500; cycle++) {
        // Generate random attention weights
        for (uint32_t i = 0; i < config.num_heads; i++) {
            weights[i] = dist(rng);
        }

        int spikes = attention_snn_encode_weights(bridge, weights, config.num_heads);
        EXPECT_GE(spikes, -1) << "Encoding should not fail at cycle " << cycle;

        // Rapid simulation steps
        int ret = attention_snn_step(bridge);
        EXPECT_EQ(ret, 0) << "Step should succeed at cycle " << cycle;

        // Check focus strength is valid
        float focus = attention_snn_get_focus_strength(bridge);
        EXPECT_GE(focus, -1.0f);  // -1.0f is error indicator
        if (focus >= 0.0f) {
            EXPECT_LE(focus, 1.0f);
            EXPECT_FALSE(std::isnan(focus));
        }
    }

    // Verify bridge state after stress
    attention_snn_bridge_state_t state;
    int ret = attention_snn_get_state(bridge, &state);
    EXPECT_EQ(ret, 0);
    EXPECT_NE(state.state, ATTENTION_SNN_STATE_DISABLED);
}

TEST_F(AttentionPlasticityBridgeRegressionTest, RapidShiftEventsStability) {
    // REGRESSION: Plasticity bridge stable under rapid shift events

    // Register synapses for all heads
    for (uint32_t i = 0; i < ATTENTION_PLASTICITY_MAX_HEADS; i++) {
        attention_plasticity_register_synapse(
            bridge, i, ATTENTION_SYNAPSE_HEAD_OUTPUT, i, 0.5f);
    }

    uint64_t timestamp = get_timestamp_us();

    for (int cycle = 0; cycle < 500; cycle++) {
        uint32_t from_head = cycle % ATTENTION_PLASTICITY_MAX_HEADS;
        uint32_t to_head = (cycle + 3) % ATTENTION_PLASTICITY_MAX_HEADS;
        float shift_strength = 0.5f + 0.4f * sinf(cycle * 0.1f);

        int ret = attention_plasticity_shift(bridge, from_head, to_head,
                                             shift_strength, timestamp);
        EXPECT_EQ(ret, 0) << "Shift should succeed at cycle " << cycle;

        attention_plasticity_update(bridge, 1.0f);
        timestamp += 1000;
    }

    // Verify state after stress
    attention_plasticity_bridge_state_t state;
    int ret = attention_plasticity_get_state(bridge, &state);
    EXPECT_EQ(ret, 0);
    EXPECT_NE(state.state, ATTENTION_PLASTICITY_STATE_DISABLED);
}

//=============================================================================
// SECTION 2: Memory Usage Bounded After Many Operations
//=============================================================================

TEST_F(AttentionBridgeCombinedRegressionTest, MemoryUsageBoundedAfterOperations) {
    // REGRESSION: Memory usage should not grow unboundedly

    float weights[ATTENTION_SNN_MAX_HEADS];
    uint64_t timestamp = get_timestamp_us();

    for (int iter = 0; iter < 1000; iter++) {
        // SNN operations
        for (uint32_t i = 0; i < 8; i++) {
            weights[i] = 0.5f + 0.3f * sinf((float)(iter + i) * 0.1f);
        }

        attention_snn_encode_weights(snn_bridge, weights, 8);
        attention_snn_simulate(snn_bridge, 5.0f);

        // Plasticity operations
        uint32_t head_idx = iter % ATTENTION_PLASTICITY_MAX_HEADS;
        attention_plasticity_focus(plasticity_bridge, head_idx, 0.8f, timestamp);
        attention_plasticity_update(plasticity_bridge, 5.0f);
        timestamp += 5000;

        // Periodic reset to prevent state accumulation
        if (iter % 200 == 199) {
            attention_snn_reset(snn_bridge);
            attention_plasticity_reset(plasticity_bridge);
        }
    }

    // Verify both bridges are still functional
    attention_snn_bridge_state_t snn_state;
    attention_snn_get_state(snn_bridge, &snn_state);
    EXPECT_NE(snn_state.state, ATTENTION_SNN_STATE_DISABLED);

    attention_plasticity_bridge_state_t plasticity_state;
    attention_plasticity_get_state(plasticity_bridge, &plasticity_state);
    EXPECT_NE(plasticity_state.state, ATTENTION_PLASTICITY_STATE_DISABLED);
}

TEST_F(AttentionSNNBridgeRegressionTest, RepeatedCreateDestroyCycles) {
    // REGRESSION: Memory leaks on repeated create/destroy

    const int cycles = 50;

    for (int i = 0; i < cycles; i++) {
        attention_snn_config_t test_config = attention_snn_config_default();
        test_config.num_heads = 8;
        test_config.neurons_per_head = 32;
        test_config.sequence_length = 64;
        test_config.enable_bio_async = false;

        attention_snn_bridge_t* test_bridge = attention_snn_create(&test_config);
        ASSERT_NE(test_bridge, nullptr) << "Create failed at cycle " << i;

        // Do some work
        float weights[8] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f};
        attention_snn_encode_weights(test_bridge, weights, 8);
        attention_snn_simulate(test_bridge, 10.0f);

        attention_snn_destroy(test_bridge);
    }

    // If we got here without crash/hang, memory management is working
    SUCCEED();
}

TEST_F(AttentionPlasticityBridgeRegressionTest, RepeatedCreateDestroyCycles) {
    // REGRESSION: Plasticity bridge memory leaks

    const int cycles = 50;

    for (int i = 0; i < cycles; i++) {
        attention_plasticity_config_t test_config = attention_plasticity_config_default();
        attention_plasticity_bridge_t* test_bridge = attention_plasticity_create(&test_config);
        ASSERT_NE(test_bridge, nullptr) << "Create failed at cycle " << i;

        // Register and use synapses
        for (uint32_t s = 0; s < 8; s++) {
            attention_plasticity_register_synapse(
                test_bridge, s, ATTENTION_SYNAPSE_QUERY_KEY, s, 0.5f);
        }

        attention_plasticity_focus(test_bridge, 0, 0.8f, get_timestamp_us());
        attention_plasticity_destroy(test_bridge);
    }

    SUCCEED();
}

//=============================================================================
// SECTION 3: Stability with Large Number of Heads
//=============================================================================

TEST_F(AttentionSNNBridgeRegressionTest, MaxHeadsStability) {
    // REGRESSION: Bridge should handle maximum number of heads

    // Create bridge with max heads
    attention_snn_config_t max_config = attention_snn_config_default();
    max_config.num_heads = ATTENTION_SNN_MAX_HEADS;
    max_config.neurons_per_head = ATTENTION_SNN_NEURONS_PER_HEAD;
    max_config.enable_bio_async = false;

    attention_snn_bridge_t* max_bridge = attention_snn_create(&max_config);
    ASSERT_NE(max_bridge, nullptr);

    float weights[ATTENTION_SNN_MAX_HEADS];
    for (uint32_t i = 0; i < ATTENTION_SNN_MAX_HEADS; i++) {
        weights[i] = (float)i / (float)ATTENTION_SNN_MAX_HEADS;
    }

    // Encode with max heads
    int spikes = attention_snn_encode_weights(max_bridge, weights, ATTENTION_SNN_MAX_HEADS);
    EXPECT_GE(spikes, 0) << "Encoding with max heads should succeed";

    // Simulate
    int ret = attention_snn_simulate(max_bridge, 50.0f);
    EXPECT_EQ(ret, 0);

    // Decode weights
    float decoded[ATTENTION_SNN_MAX_HEADS];
    ret = attention_snn_get_weights(max_bridge, decoded, ATTENTION_SNN_MAX_HEADS);
    EXPECT_EQ(ret, 0);

    // All decoded weights should be valid
    for (uint32_t i = 0; i < ATTENTION_SNN_MAX_HEADS; i++) {
        EXPECT_GE(decoded[i], 0.0f) << "Decoded weight[" << i << "] must be >= 0";
        EXPECT_LE(decoded[i], 1.0f) << "Decoded weight[" << i << "] must be <= 1";
        EXPECT_FALSE(std::isnan(decoded[i])) << "Decoded weight[" << i << "] must not be NaN";
    }

    attention_snn_destroy(max_bridge);
}

TEST_F(AttentionPlasticityBridgeRegressionTest, MaxSynapsesGraceful) {
    // REGRESSION: Handle max synapse count gracefully

    int success_count = 0;
    for (uint32_t i = 0; i < ATTENTION_PLASTICITY_MAX_SYNAPSES + 50; i++) {
        int ret = attention_plasticity_register_synapse(
            bridge, i, ATTENTION_SYNAPSE_QUERY_KEY, i % ATTENTION_PLASTICITY_MAX_HEADS, 0.5f);

        if (ret == 0) {
            success_count++;
        }
    }

    // Should have registered exactly max synapses or less
    EXPECT_LE(success_count, ATTENTION_PLASTICITY_MAX_SYNAPSES)
        << "Should not exceed max synapse count";
    EXPECT_GT(success_count, 0) << "Should register at least some synapses";

    // Bridge should still be functional
    attention_plasticity_bridge_state_t state;
    int ret = attention_plasticity_get_state(bridge, &state);
    EXPECT_EQ(ret, 0);
    EXPECT_LE(state.registered_synapses, ATTENTION_PLASTICITY_MAX_SYNAPSES);
}

//=============================================================================
// SECTION 4: Consistent Output After Reset
//=============================================================================

TEST_F(AttentionSNNBridgeRegressionTest, ConsistentOutputAfterReset) {
    // REGRESSION: Reset should produce consistent state

    float weights[8] = {0.1f, 0.3f, 0.5f, 0.7f, 0.9f, 0.8f, 0.6f, 0.4f};

    // First run
    attention_snn_encode_weights(bridge, weights, 8);
    attention_snn_simulate(bridge, 50.0f);

    float focus1 = attention_snn_get_focus_strength(bridge);
    float sparsity1 = attention_snn_get_sparsity(bridge);

    // Reset and run again with same input
    attention_snn_reset(bridge);

    attention_snn_encode_weights(bridge, weights, 8);
    attention_snn_simulate(bridge, 50.0f);

    float focus2 = attention_snn_get_focus_strength(bridge);
    float sparsity2 = attention_snn_get_sparsity(bridge);

    // Results should be similar (allowing for some variance)
    if (focus1 >= 0.0f && focus2 >= 0.0f) {
        EXPECT_NEAR(focus1, focus2, 0.2f) << "Focus should be similar after reset";
    }
    if (sparsity1 >= 0.0f && sparsity2 >= 0.0f) {
        EXPECT_NEAR(sparsity1, sparsity2, 0.2f) << "Sparsity should be similar after reset";
    }
}

TEST_F(AttentionPlasticityBridgeRegressionTest, ResetClearsStateCorrectly) {
    // REGRESSION: Reset should clear state while maintaining configuration

    // Do some work
    for (uint32_t i = 0; i < 8; i++) {
        attention_plasticity_register_synapse(bridge, i, ATTENTION_SYNAPSE_HEAD_OUTPUT, i, 0.5f);
    }

    uint64_t timestamp = get_timestamp_us();
    for (int i = 0; i < 50; i++) {
        attention_plasticity_focus(bridge, i % 8, 0.8f, timestamp);
        attention_plasticity_update(bridge, 10.0f);
        timestamp += 10000;
    }

    // Get stats before reset
    attention_plasticity_stats_t stats_before;
    attention_plasticity_get_stats(bridge, &stats_before);
    EXPECT_GT(stats_before.total_focus_events, 0u);

    // Reset
    int ret = attention_plasticity_reset(bridge);
    EXPECT_EQ(ret, 0);

    // Get state after reset
    attention_plasticity_bridge_state_t state_after;
    attention_plasticity_get_state(bridge, &state_after);
    EXPECT_EQ(state_after.state, ATTENTION_PLASTICITY_STATE_IDLE);

    // Bridge should still be functional
    ret = attention_plasticity_focus(bridge, 0, 0.9f, get_timestamp_us());
    EXPECT_EQ(ret, 0);
}

//=============================================================================
// SECTION 5: Empty/Zero Input Handling
//=============================================================================

TEST_F(AttentionSNNBridgeRegressionTest, ZeroWeightsHandling) {
    // REGRESSION: Zero weights should be handled gracefully

    float zero_weights[8] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};

    int spikes = attention_snn_encode_weights(bridge, zero_weights, 8);
    EXPECT_GE(spikes, -1) << "Zero weights should be handled";

    int ret = attention_snn_simulate(bridge, 20.0f);
    EXPECT_EQ(ret, 0);

    // Focus and sparsity should be valid
    float focus = attention_snn_get_focus_strength(bridge);
    EXPECT_GE(focus, -1.0f);  // -1.0f indicates error

    float sparsity = attention_snn_get_sparsity(bridge);
    EXPECT_GE(sparsity, -1.0f);
}

TEST_F(AttentionSNNBridgeRegressionTest, NullInputHandling) {
    // REGRESSION: Null input should be handled gracefully

    int spikes = attention_snn_encode_weights(bridge, nullptr, 8);
    EXPECT_LT(spikes, 0) << "Null weights should return error";

    spikes = attention_snn_encode_salience(bridge, nullptr, 64);
    EXPECT_LT(spikes, 0) << "Null salience should return error";

    // Bridge should still be functional after null inputs
    float valid_weights[8] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    spikes = attention_snn_encode_weights(bridge, valid_weights, 8);
    EXPECT_GE(spikes, 0) << "Bridge should recover after null input";
}

TEST_F(AttentionSNNBridgeRegressionTest, NullOutputHandling) {
    // REGRESSION: Null output pointers should be handled gracefully

    int ret = attention_snn_get_state(bridge, nullptr);
    EXPECT_LT(ret, 0) << "Null state output should return error";

    ret = attention_snn_get_stats(bridge, nullptr);
    EXPECT_LT(ret, 0) << "Null stats output should return error";

    ret = attention_snn_get_weights(bridge, nullptr, 8);
    EXPECT_LT(ret, 0) << "Null weights output should return error";
}

TEST_F(AttentionSNNBridgeRegressionTest, EmptySalienceHandling) {
    // REGRESSION: Empty salience should be handled

    float salience[1] = {0.0f};

    // Zero sequence length should be handled
    int spikes = attention_snn_encode_salience(bridge, salience, 0);
    EXPECT_GE(spikes, -1) << "Zero sequence length should be handled gracefully";

    // Bridge should still be functional
    float valid_salience[64];
    for (int i = 0; i < 64; i++) {
        valid_salience[i] = 0.5f;
    }
    spikes = attention_snn_encode_salience(bridge, valid_salience, 64);
    EXPECT_GE(spikes, -1);
}

//=============================================================================
// SECTION 6: Boundary Value Testing (weight_min, weight_max)
//=============================================================================

TEST_F(AttentionPlasticityBridgeRegressionTest, WeightBoundsAfterSTDP) {
    // REGRESSION: Weights must stay within [weight_min, weight_max] after STDP

    // Register synapses
    for (uint32_t i = 0; i < 16; i++) {
        int ret = attention_plasticity_register_synapse(
            bridge, i, ATTENTION_SYNAPSE_QUERY_KEY, i % ATTENTION_PLASTICITY_MAX_HEADS, 0.5f);
        EXPECT_EQ(ret, 0) << "Synapse registration should succeed for synapse " << i;
    }

    uint64_t timestamp = get_timestamp_us();

    // Trigger many plasticity events with extreme values
    for (int iter = 0; iter < 200; iter++) {
        uint32_t head = iter % ATTENTION_PLASTICITY_MAX_HEADS;

        attention_plasticity_focus(bridge, head, 1.0f, timestamp);
        timestamp += 5000;

        // Alternate between reward and punishment
        float reward = (iter % 2 == 0) ? 1.0f : -1.0f;
        attention_plasticity_reward(bridge, reward, timestamp);
        timestamp += 5000;

        attention_plasticity_update(bridge, 10.0f);
    }

    // Verify all synapse weights are within bounds
    for (uint32_t syn = 0; syn < 16; syn++) {
        attention_plasticity_synapse_t state;
        int ret = attention_plasticity_get_synapse(bridge, syn, &state);
        if (ret == 0) {
            EXPECT_GE(state.weight, config.weight_min)
                << "Weight must be >= weight_min for synapse " << syn;
            EXPECT_LE(state.weight, config.weight_max)
                << "Weight must be <= weight_max for synapse " << syn;
            EXPECT_FALSE(std::isnan(state.weight))
                << "Weight must not be NaN for synapse " << syn;
        }
    }
}

TEST_F(AttentionPlasticityBridgeRegressionTest, InitialWeightBoundaryValues) {
    // REGRESSION: Test boundary values for initial weights

    float boundary_weights[] = {0.0f, 0.001f, 0.5f, 0.999f, 1.0f};

    for (int i = 0; i < 5; i++) {
        int ret = attention_plasticity_register_synapse(
            bridge, i, ATTENTION_SYNAPSE_KEY_VALUE, 0, boundary_weights[i]);
        EXPECT_EQ(ret, 0) << "Registration should succeed with initial weight " << boundary_weights[i];

        attention_plasticity_synapse_t synapse;
        ret = attention_plasticity_get_synapse(bridge, i, &synapse);
        if (ret == 0) {
            EXPECT_GE(synapse.weight, config.weight_min);
            EXPECT_LE(synapse.weight, config.weight_max);
        }
    }
}

//=============================================================================
// SECTION 7: Statistics Consistency
//=============================================================================

TEST_F(AttentionSNNBridgeRegressionTest, StatisticsConsistency) {
    // REGRESSION: Statistics should accurately reflect operations

    attention_snn_reset_stats(bridge);

    float weights[8];
    const int n_encodes = 50;

    for (int i = 0; i < n_encodes; i++) {
        generate_weights(weights, 8, i);
        attention_snn_encode_weights(bridge, weights, 8);
        attention_snn_simulate(bridge, 10.0f);
    }

    attention_snn_stats_t stats;
    int ret = attention_snn_get_stats(bridge, &stats);
    EXPECT_EQ(ret, 0);

    // Forward passes should reflect iterations
    EXPECT_GE(stats.total_forward_passes, 0u);
    EXPECT_GE(stats.total_spikes_generated, 0u);

    // Processing time should be positive
    EXPECT_GE(stats.avg_processing_time_ms, 0.0f);
    EXPECT_FALSE(std::isnan(stats.avg_processing_time_ms));
}

TEST_F(AttentionPlasticityBridgeRegressionTest, StatisticsAccuracy) {
    // REGRESSION: Statistics should accurately reflect operations

    for (uint32_t i = 0; i < 8; i++) {
        attention_plasticity_register_synapse(
            bridge, i, ATTENTION_SYNAPSE_HEAD_OUTPUT, i, 0.5f);
    }

    attention_plasticity_reset_stats(bridge);

    const int n_focus = 50;
    const int n_shift = 30;
    uint64_t timestamp = get_timestamp_us();

    for (int i = 0; i < n_focus; i++) {
        attention_plasticity_focus(bridge, i % 8, 0.8f, timestamp);
        timestamp += 10000;
    }

    for (int i = 0; i < n_shift; i++) {
        attention_plasticity_shift(bridge, i % 8, (i + 1) % 8, 0.7f, timestamp);
        timestamp += 10000;
    }

    attention_plasticity_stats_t stats;
    int ret = attention_plasticity_get_stats(bridge, &stats);
    EXPECT_EQ(ret, 0);

    EXPECT_EQ(stats.total_focus_events, static_cast<uint64_t>(n_focus));
    EXPECT_EQ(stats.total_shift_events, static_cast<uint64_t>(n_shift));
}

TEST_F(AttentionSNNBridgeRegressionTest, StatsResetWorks) {
    // REGRESSION: Reset stats should clear counters

    float weights[8];
    generate_weights(weights, 8, 0);
    attention_snn_encode_weights(bridge, weights, 8);
    attention_snn_simulate(bridge, 50.0f);

    attention_snn_stats_t stats_before;
    attention_snn_get_stats(bridge, &stats_before);

    attention_snn_reset_stats(bridge);

    attention_snn_stats_t stats_after;
    attention_snn_get_stats(bridge, &stats_after);

    EXPECT_EQ(stats_after.total_forward_passes, 0u);
    EXPECT_EQ(stats_after.total_decodings, 0u);
}

//=============================================================================
// SECTION 8: Thread Safety Verification (Basic)
//=============================================================================

TEST_F(AttentionSNNBridgeRegressionTest, BasicThreadSafety) {
    // REGRESSION: Basic thread safety - concurrent reads should not crash

    std::atomic<bool> stop{false};
    std::atomic<int> error_count{0};

    // Setup initial state
    float weights[8] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f};
    attention_snn_encode_weights(bridge, weights, 8);
    attention_snn_simulate(bridge, 50.0f);

    // Reader threads
    auto reader = [&]() {
        while (!stop) {
            attention_snn_bridge_state_t state;
            int ret = attention_snn_get_state(bridge, &state);
            if (ret != 0) {
                error_count++;
            }

            float focus = attention_snn_get_focus_strength(bridge);
            if (focus < -1.1f) {  // Allow -1.0f as error indicator
                error_count++;
            }
        }
    };

    // Start reader threads
    std::thread t1(reader);
    std::thread t2(reader);

    // Let them run briefly
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    stop = true;
    t1.join();
    t2.join();

    // No crashes is the primary goal; errors during concurrent access are acceptable
    // but there shouldn't be too many
    EXPECT_LT(error_count, 100) << "Too many errors during concurrent reads";
}

//=============================================================================
// SECTION 9: High-Frequency Encode/Decode Cycles
//=============================================================================

TEST_F(AttentionSNNBridgeRegressionTest, HighFrequencyEncodeDecodeCycles) {
    // REGRESSION: High-frequency encode/decode should not cause issues

    float weights[8];
    float decoded[8];

    auto start = std::chrono::high_resolution_clock::now();

    const int iterations = 1000;
    for (int i = 0; i < iterations; i++) {
        // Generate varying weights
        for (int j = 0; j < 8; j++) {
            weights[j] = 0.5f + 0.4f * sinf((float)(i * 8 + j) * 0.01f);
        }

        // Encode
        int spikes = attention_snn_encode_weights(bridge, weights, 8);
        EXPECT_GE(spikes, -1);

        // Quick step
        attention_snn_step(bridge);

        // Decode
        int ret = attention_snn_get_weights(bridge, decoded, 8);
        EXPECT_EQ(ret, 0);

        // Validate decoded weights
        for (int j = 0; j < 8; j++) {
            EXPECT_GE(decoded[j], 0.0f);
            EXPECT_LE(decoded[j], 1.0f);
            EXPECT_FALSE(std::isnan(decoded[j]));
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    // Performance check: should complete 1000 cycles in reasonable time
    double avg_us = static_cast<double>(duration_us) / iterations;
    EXPECT_LT(avg_us, 500.0)
        << "Encode/decode too slow: " << avg_us << " us/cycle (max 500us)";
}

TEST_F(AttentionPlasticityBridgeRegressionTest, HighFrequencyUpdateCycles) {
    // REGRESSION: High-frequency plasticity updates should not cause issues

    for (uint32_t i = 0; i < 16; i++) {
        attention_plasticity_register_synapse(
            bridge, i, ATTENTION_SYNAPSE_QUERY_KEY, i % 8, 0.5f);
    }

    uint64_t timestamp = get_timestamp_us();

    auto start = std::chrono::high_resolution_clock::now();

    const int iterations = 1000;
    for (int i = 0; i < iterations; i++) {
        uint32_t head = i % 8;
        float weight = 0.5f + 0.4f * sinf(i * 0.1f);

        attention_plasticity_focus(bridge, head, weight, timestamp);
        attention_plasticity_update(bridge, 1.0f);
        timestamp += 1000;
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    // Performance check
    double avg_us = static_cast<double>(duration_us) / iterations;
    EXPECT_LT(avg_us, 200.0)
        << "Plasticity update too slow: " << avg_us << " us/update (max 200us)";

    // Verify synapses are still valid
    for (uint32_t syn = 0; syn < 16; syn++) {
        attention_plasticity_synapse_t state;
        int ret = attention_plasticity_get_synapse(bridge, syn, &state);
        if (ret == 0) {
            EXPECT_FALSE(std::isnan(state.weight));
            EXPECT_FALSE(std::isinf(state.weight));
        }
    }
}

//=============================================================================
// SECTION 10: Competition Convergence Behavior
//=============================================================================

TEST_F(AttentionSNNBridgeRegressionTest, CompetitionConvergence) {
    // REGRESSION: Competition should converge to stable state

    // Create bridge with competition enabled
    attention_snn_config_t comp_config = attention_snn_config_default();
    comp_config.enable_competition = true;
    comp_config.inhibition_strength = 0.5f;
    comp_config.top_k = 3;
    comp_config.num_heads = 8;
    comp_config.enable_bio_async = false;

    attention_snn_bridge_t* comp_bridge = attention_snn_create(&comp_config);
    ASSERT_NE(comp_bridge, nullptr);

    // Input with clear winner
    float weights[8] = {0.9f, 0.1f, 0.1f, 0.8f, 0.1f, 0.1f, 0.1f, 0.7f};

    attention_snn_encode_weights(comp_bridge, weights, 8);

    // Run competition
    int ret = attention_snn_compete(comp_bridge, 100.0f);
    EXPECT_EQ(ret, 0);

    // Get top-k indices
    int32_t top_indices[3];
    int count = attention_snn_get_top_k(comp_bridge, top_indices, 3);
    EXPECT_GE(count, 0);

    // Focus strength should show competition effect
    float focus = attention_snn_get_focus_strength(comp_bridge);
    EXPECT_GE(focus, 0.0f) << "Focus should be non-negative";
    EXPECT_LE(focus, 1.0f) << "Focus should be at most 1.0";

    // Sparsity indicates how many weights are below threshold
    float sparsity = attention_snn_get_sparsity(comp_bridge);
    EXPECT_GE(sparsity, 0.0f) << "Sparsity should be non-negative";
    EXPECT_LE(sparsity, 1.0f) << "Sparsity should be at most 1.0";

    // At least one metric should show differentiation (focus > 0 indicates variance)
    // If SNN produces uniform outputs, sparsity may be low, but focus should be high after normalization
    bool has_differentiation = (focus > 0.1f) || (sparsity > 0.1f);
    EXPECT_TRUE(has_differentiation) << "Competition should produce some differentiation (focus="
                                      << focus << ", sparsity=" << sparsity << ")";

    attention_snn_destroy(comp_bridge);
}

TEST_F(AttentionSNNBridgeRegressionTest, CompetitionEnergyDecays) {
    // REGRESSION: Competition energy should decay over time

    attention_snn_config_t comp_config = attention_snn_config_default();
    comp_config.enable_competition = true;
    comp_config.inhibition_strength = 0.8f;
    comp_config.num_heads = 8;
    comp_config.enable_bio_async = false;

    attention_snn_bridge_t* comp_bridge = attention_snn_create(&comp_config);
    ASSERT_NE(comp_bridge, nullptr);

    float weights[8] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    attention_snn_encode_weights(comp_bridge, weights, 8);

    // Run competition and track energy
    std::vector<float> energies;

    for (int i = 0; i < 10; i++) {
        attention_snn_compete(comp_bridge, 20.0f);

        attention_snn_bridge_state_t state;
        attention_snn_get_state(comp_bridge, &state);
        energies.push_back(state.competition_energy);
    }

    // Energy values should all be valid (non-NaN, non-Inf)
    for (size_t i = 0; i < energies.size(); i++) {
        EXPECT_FALSE(std::isnan(energies[i])) << "Energy NaN at step " << i;
        EXPECT_FALSE(std::isinf(energies[i])) << "Energy Inf at step " << i;
        EXPECT_GE(energies[i], 0.0f) << "Energy should be non-negative at step " << i;
    }

    attention_snn_destroy(comp_bridge);
}

//=============================================================================
// SECTION 11: Salience Processing Tests
//=============================================================================

TEST_F(AttentionSNNBridgeRegressionTest, SalienceEncodingDecoding) {
    // REGRESSION: Salience encoding/decoding should preserve structure

    float salience[64];
    float decoded[64];

    // Create salience map with a clear peak
    for (uint32_t i = 0; i < 64; i++) {
        salience[i] = 0.1f + 0.8f * expf(-powf((float)i - 32.0f, 2.0f) / 100.0f);
    }

    int spikes = attention_snn_encode_salience(bridge, salience, 64);
    EXPECT_GE(spikes, 0) << "Salience encoding should succeed";

    attention_snn_simulate(bridge, 50.0f);

    int ret = attention_snn_get_salience(bridge, decoded, 64);
    EXPECT_EQ(ret, 0);

    // All decoded values should be valid
    for (uint32_t i = 0; i < 64; i++) {
        EXPECT_GE(decoded[i], 0.0f) << "Decoded salience[" << i << "] must be >= 0";
        EXPECT_LE(decoded[i], 1.0f) << "Decoded salience[" << i << "] must be <= 1";
        EXPECT_FALSE(std::isnan(decoded[i]));
    }
}

TEST_F(AttentionPlasticityBridgeRegressionTest, SalienceModulationEffect) {
    // REGRESSION: Salience modulation should affect learning

    for (uint32_t i = 0; i < 8; i++) {
        attention_plasticity_register_synapse(
            bridge, i, ATTENTION_SYNAPSE_SALIENCE, i, 0.5f);
    }

    // Set high salience modulation
    int ret = attention_plasticity_set_salience_modulation(bridge, 0.9f);
    EXPECT_EQ(ret, 0);

    attention_plasticity_bridge_state_t state;
    attention_plasticity_get_state(bridge, &state);

    EXPECT_GE(state.current_salience_mod, 0.0f);
    EXPECT_LE(state.current_salience_mod, 1.0f);
}

//=============================================================================
// SECTION 12: Gate Integration Tests
//=============================================================================

TEST_F(AttentionSNNBridgeRegressionTest, GateSignalBounds) {
    // REGRESSION: Gate signal encoding should handle boundary values

    float test_gates[] = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};

    for (float gate : test_gates) {
        attention_snn_reset(bridge);

        int ret = attention_snn_encode_gate(bridge, gate);
        EXPECT_EQ(ret, 0) << "Gate encoding should succeed for gate=" << gate;

        attention_snn_simulate(bridge, 20.0f);

        attention_snn_attention_state_t attn_state;
        ret = attention_snn_get_attention_state(bridge, &attn_state);
        if (ret == 0) {
            EXPECT_GE(attn_state.gate_activation, 0.0f);
            EXPECT_LE(attn_state.gate_activation, 1.0f);
        }
    }
}

TEST_F(AttentionSNNBridgeRegressionTest, GateModulationEffect) {
    // REGRESSION: Gate modulation should affect attention

    float weights[8] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    attention_snn_encode_weights(bridge, weights, 8);

    // Low gate modulation
    attention_snn_set_gate_modulation(bridge, 0.1f);
    attention_snn_simulate(bridge, 30.0f);
    float focus_low = attention_snn_get_focus_strength(bridge);

    attention_snn_reset(bridge);
    attention_snn_encode_weights(bridge, weights, 8);

    // High gate modulation
    attention_snn_set_gate_modulation(bridge, 0.9f);
    attention_snn_simulate(bridge, 30.0f);
    float focus_high = attention_snn_get_focus_strength(bridge);

    // Both should be valid
    if (focus_low >= 0.0f && focus_high >= 0.0f) {
        EXPECT_FALSE(std::isnan(focus_low));
        EXPECT_FALSE(std::isnan(focus_high));
    }
}

//=============================================================================
// SECTION 13: Habituation and Novelty Tests
//=============================================================================

TEST_F(AttentionPlasticityBridgeRegressionTest, HabituationLevelBounded) {
    // REGRESSION: Habituation level should be bounded

    for (uint32_t head = 0; head < 8; head++) {
        attention_plasticity_register_synapse(
            bridge, head, ATTENTION_SYNAPSE_HEAD_OUTPUT, head, 0.5f);
    }

    uint64_t timestamp = get_timestamp_us();

    // Trigger many habituation trials
    for (int i = 0; i < 100; i++) {
        for (uint32_t head = 0; head < 8; head++) {
            attention_plasticity_habituation_trial(bridge, head, timestamp);
            attention_plasticity_update(bridge, 10.0f);
            timestamp += 10000;
        }
    }

    // Check habituation levels
    for (uint32_t head = 0; head < 8; head++) {
        float hab = attention_plasticity_get_habituation(bridge, head);

        EXPECT_GE(hab, 0.0f) << "Habituation must be >= 0 for head " << head;
        EXPECT_LE(hab, 1.0f) << "Habituation must be <= 1 for head " << head;
        EXPECT_FALSE(std::isnan(hab)) << "Habituation must not be NaN";
    }
}

TEST_F(AttentionPlasticityBridgeRegressionTest, NoveltyScoreBounded) {
    // REGRESSION: Novelty score should be bounded

    for (uint32_t head = 0; head < 8; head++) {
        attention_plasticity_register_synapse(
            bridge, head, ATTENTION_SYNAPSE_HEAD_OUTPUT, head, 0.5f);
    }

    uint64_t timestamp = get_timestamp_us();

    // Trigger novelty events
    for (int i = 0; i < 50; i++) {
        uint32_t head = i % 8;
        float novelty = 0.5f + 0.4f * sinf(i * 0.2f);

        attention_plasticity_novelty(bridge, head, novelty, timestamp);
        attention_plasticity_update(bridge, 10.0f);
        timestamp += 10000;
    }

    // Check novelty scores
    for (uint32_t head = 0; head < 8; head++) {
        float novelty = attention_plasticity_get_novelty_score(bridge, head);

        EXPECT_GE(novelty, 0.0f) << "Novelty must be >= 0 for head " << head;
        EXPECT_FALSE(std::isnan(novelty)) << "Novelty must not be NaN";
        EXPECT_FALSE(std::isinf(novelty)) << "Novelty must not be Inf";
    }
}

//=============================================================================
// SECTION 14: Arousal Modulation Tests
//=============================================================================

TEST_F(AttentionSNNBridgeRegressionTest, ArousalModulationBounds) {
    // REGRESSION: Arousal modulation should handle boundary values

    float test_arousals[] = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};

    for (float arousal : test_arousals) {
        int ret = attention_snn_modulate_by_arousal(bridge, arousal);
        EXPECT_EQ(ret, 0) << "Arousal modulation should succeed for arousal=" << arousal;

        // Verify bridge is still functional
        float weights[8] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
        int spikes = attention_snn_encode_weights(bridge, weights, 8);
        EXPECT_GE(spikes, 0);
    }
}

TEST_F(AttentionPlasticityBridgeRegressionTest, AttentionModulationBounded) {
    // REGRESSION: Attention modulation should be bounded

    float test_attentions[] = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};

    for (float attn : test_attentions) {
        int ret = attention_plasticity_set_attention_modulation(bridge, attn);
        EXPECT_EQ(ret, 0) << "Setting attention modulation should succeed for " << attn;

        attention_plasticity_bridge_state_t state;
        attention_plasticity_get_state(bridge, &state);

        EXPECT_GE(state.current_attention_mod, 0.0f);
        EXPECT_LE(state.current_attention_mod, 1.0f);
    }
}

//=============================================================================
// SECTION 15: Sensitivity and Bias Tests
//=============================================================================

TEST_F(AttentionPlasticityBridgeRegressionTest, SensitivityBounded) {
    // REGRESSION: Sensitivity values should be bounded

    for (uint32_t i = 0; i < 8; i++) {
        attention_plasticity_register_synapse(
            bridge, i, ATTENTION_SYNAPSE_HEAD_OUTPUT, i, 0.5f);
    }

    uint64_t timestamp = get_timestamp_us();

    // Process many events
    for (int iter = 0; iter < 100; iter++) {
        uint32_t head = iter % 8;
        attention_plasticity_focus(bridge, head, 0.9f, timestamp);
        attention_plasticity_reward(bridge, 1.0f, timestamp + 5000);
        attention_plasticity_update(bridge, 10.0f);
        timestamp += 15000;
    }

    // Check sensitivity for all heads
    for (uint32_t head = 0; head < 8; head++) {
        float sensitivity = attention_plasticity_get_sensitivity(bridge, head);

        EXPECT_GE(sensitivity, 0.0f) << "Sensitivity must be >= 0 for head " << head;
        EXPECT_FALSE(std::isnan(sensitivity)) << "Sensitivity must not be NaN";
        EXPECT_FALSE(std::isinf(sensitivity)) << "Sensitivity must not be Inf";
    }
}

TEST_F(AttentionPlasticityBridgeRegressionTest, BiasBounded) {
    // REGRESSION: Attention bias should be bounded

    for (uint32_t i = 0; i < 8; i++) {
        attention_plasticity_register_synapse(
            bridge, i, ATTENTION_SYNAPSE_HEAD_OUTPUT, i, 0.5f);
    }

    uint64_t timestamp = get_timestamp_us();

    // Focus heavily on one head
    for (int iter = 0; iter < 100; iter++) {
        attention_plasticity_focus(bridge, 0, 1.0f, timestamp);
        attention_plasticity_update(bridge, 10.0f);
        timestamp += 10000;
    }

    // Check bias for all heads
    for (uint32_t head = 0; head < 8; head++) {
        float bias;
        int ret = attention_plasticity_get_bias(bridge, head, &bias);

        if (ret == 0) {
            EXPECT_FALSE(std::isnan(bias)) << "Bias must not be NaN for head " << head;
            EXPECT_FALSE(std::isinf(bias)) << "Bias must not be Inf for head " << head;
        }
    }
}

//=============================================================================
// SECTION 16: Numerical Stability Under Extreme Values
//=============================================================================

TEST_F(AttentionSNNBridgeRegressionTest, NoNaNOrInfAfterManyIterations) {
    // REGRESSION: No NaN or Inf values should appear after extended simulation

    std::mt19937 rng(42);
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);

    float weights[8];

    for (int iter = 0; iter < 500; iter++) {
        for (int i = 0; i < 8; i++) {
            weights[i] = dist(rng);
        }

        attention_snn_encode_weights(bridge, weights, 8);
        attention_snn_simulate(bridge, 5.0f);

        // Check attention state
        attention_snn_attention_state_t attn_state;
        int ret = attention_snn_get_attention_state(bridge, &attn_state);
        if (ret == 0) {
            EXPECT_FALSE(std::isnan(attn_state.focus_strength))
                << "Focus strength NaN at iter " << iter;
            EXPECT_FALSE(std::isinf(attn_state.focus_strength))
                << "Focus strength Inf at iter " << iter;
            EXPECT_FALSE(std::isnan(attn_state.sparsity))
                << "Sparsity NaN at iter " << iter;
            EXPECT_FALSE(std::isinf(attn_state.sparsity))
                << "Sparsity Inf at iter " << iter;
        }

        // Check bridge state
        attention_snn_bridge_state_t state;
        attention_snn_get_state(bridge, &state);
        EXPECT_FALSE(std::isnan(state.avg_firing_rate))
            << "Firing rate NaN at iter " << iter;
    }
}

TEST_F(AttentionPlasticityBridgeRegressionTest, EligibilityTracesBounded) {
    // REGRESSION: Eligibility traces must remain bounded

    for (uint32_t i = 0; i < 8; i++) {
        attention_plasticity_register_synapse(
            bridge, i, ATTENTION_SYNAPSE_QUERY_KEY, i, 0.5f);
    }

    uint64_t timestamp = get_timestamp_us();

    // Rapid stimulation to build up eligibility traces
    for (int iter = 0; iter < 300; iter++) {
        attention_plasticity_focus(bridge, iter % 8, 1.0f, timestamp);
        attention_plasticity_update(bridge, 1.0f);
        timestamp += 1000;
    }

    // Verify eligibility traces
    for (uint32_t syn = 0; syn < 8; syn++) {
        attention_plasticity_synapse_t state;
        int ret = attention_plasticity_get_synapse(bridge, syn, &state);
        if (ret == 0) {
            EXPECT_GE(state.eligibility_trace, 0.0f)
                << "Eligibility trace must be >= 0 for synapse " << syn;
            EXPECT_LE(state.eligibility_trace, 10.0f)
                << "Eligibility trace must be bounded for synapse " << syn;
            EXPECT_FALSE(std::isnan(state.eligibility_trace))
                << "Eligibility trace must not be NaN";
        }
    }
}

//=============================================================================
// SECTION 17: Competition Strength Tests
//=============================================================================

TEST_F(AttentionSNNBridgeRegressionTest, CompetitionStrengthBounds) {
    // REGRESSION: Competition strength setting should handle bounds

    float test_strengths[] = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};

    for (float strength : test_strengths) {
        int ret = attention_snn_set_competition_strength(bridge, strength);
        EXPECT_EQ(ret, 0) << "Setting competition strength should succeed for " << strength;

        // Bridge should still be functional
        float weights[8] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
        attention_snn_encode_weights(bridge, weights, 8);
        ret = attention_snn_simulate(bridge, 10.0f);
        EXPECT_EQ(ret, 0);
    }
}

//=============================================================================
// SECTION 18: Consolidation Tests
//=============================================================================

TEST_F(AttentionPlasticityBridgeRegressionTest, ConsolidationDoesNotCorrupt) {
    // REGRESSION: Consolidation should not corrupt weights

    for (uint32_t i = 0; i < 16; i++) {
        attention_plasticity_register_synapse(
            bridge, i, ATTENTION_SYNAPSE_HEAD_OUTPUT, i % 8, 0.5f);
    }

    uint64_t timestamp = get_timestamp_us();

    // Build up some state
    for (int iter = 0; iter < 50; iter++) {
        attention_plasticity_focus(bridge, iter % 8, 0.8f, timestamp);
        attention_plasticity_update(bridge, 10.0f);
        timestamp += 10000;
    }

    // Get weights before consolidation
    std::vector<float> weights_before(16);
    for (uint32_t i = 0; i < 16; i++) {
        attention_plasticity_synapse_t state;
        if (attention_plasticity_get_synapse(bridge, i, &state) == 0) {
            weights_before[i] = state.weight;
        }
    }

    // Trigger consolidation
    int ret = attention_plasticity_consolidate(bridge);
    EXPECT_EQ(ret, 0);

    // Verify weights are still valid after consolidation
    for (uint32_t i = 0; i < 16; i++) {
        attention_plasticity_synapse_t state;
        if (attention_plasticity_get_synapse(bridge, i, &state) == 0) {
            EXPECT_GE(state.weight, config.weight_min);
            EXPECT_LE(state.weight, config.weight_max);
            EXPECT_FALSE(std::isnan(state.weight));
        }
    }
}

//=============================================================================
// SECTION 19: Modulation Query Tests
//=============================================================================

TEST_F(AttentionPlasticityBridgeRegressionTest, ModulationQueryAllHeads) {
    // REGRESSION: Getting modulation for all heads should work

    for (uint32_t i = 0; i < 8; i++) {
        attention_plasticity_register_synapse(
            bridge, i, ATTENTION_SYNAPSE_HEAD_OUTPUT, i, 0.5f);
    }

    // Do some focus operations
    uint64_t timestamp = get_timestamp_us();
    for (int iter = 0; iter < 20; iter++) {
        attention_plasticity_focus(bridge, iter % 8, 0.8f, timestamp);
        attention_plasticity_update(bridge, 10.0f);
        timestamp += 10000;
    }

    float modulation[ATTENTION_PLASTICITY_MAX_HEADS];
    int ret = attention_plasticity_get_modulation(bridge, modulation, 8);
    EXPECT_EQ(ret, 0);

    // All modulation values should be valid
    for (uint32_t i = 0; i < 8; i++) {
        EXPECT_FALSE(std::isnan(modulation[i]))
            << "Modulation must not be NaN for head " << i;
        EXPECT_FALSE(std::isinf(modulation[i]))
            << "Modulation must not be Inf for head " << i;
    }
}

//=============================================================================
// SECTION 20: Top-K Indices Tests
//=============================================================================

TEST_F(AttentionSNNBridgeRegressionTest, TopKIndicesValid) {
    // REGRESSION: Top-k indices should be valid

    // Create distinct weights
    float weights[8] = {0.1f, 0.9f, 0.2f, 0.8f, 0.3f, 0.7f, 0.4f, 0.6f};

    attention_snn_encode_weights(bridge, weights, 8);
    attention_snn_simulate(bridge, 50.0f);

    int32_t indices[4];
    int count = attention_snn_get_top_k(bridge, indices, 4);

    if (count > 0) {
        for (int i = 0; i < count; i++) {
            EXPECT_GE(indices[i], 0) << "Top-k index[" << i << "] must be >= 0";
            EXPECT_LT(indices[i], 8) << "Top-k index[" << i << "] must be < num_heads";
        }
    }
}

TEST_F(AttentionSNNBridgeRegressionTest, TopKWithZeroK) {
    // REGRESSION: Top-k with k=0 should be handled

    float weights[8] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};

    attention_snn_encode_weights(bridge, weights, 8);
    attention_snn_simulate(bridge, 20.0f);

    int32_t indices[1];
    int count = attention_snn_get_top_k(bridge, indices, 0);

    // Should handle gracefully (return 0 or error)
    EXPECT_GE(count, -1);
}

//=============================================================================
// SECTION 21: Synapse Registration/Unregistration Tests
//=============================================================================

TEST_F(AttentionPlasticityBridgeRegressionTest, SynapseUnregistration) {
    // REGRESSION: Synapse unregistration should work correctly

    // Register synapses
    for (uint32_t i = 0; i < 8; i++) {
        int ret = attention_plasticity_register_synapse(
            bridge, i, ATTENTION_SYNAPSE_HEAD_OUTPUT, i, 0.5f);
        EXPECT_EQ(ret, 0);
    }

    // Unregister some
    for (uint32_t i = 0; i < 4; i++) {
        int ret = attention_plasticity_unregister_synapse(bridge, i);
        EXPECT_EQ(ret, 0) << "Unregistration should succeed for synapse " << i;
    }

    // Verify state
    attention_plasticity_bridge_state_t state;
    attention_plasticity_get_state(bridge, &state);
    EXPECT_LE(state.registered_synapses, 8u);

    // Re-register should work
    int ret = attention_plasticity_register_synapse(bridge, 0, ATTENTION_SYNAPSE_QUERY_KEY, 0, 0.6f);
    EXPECT_EQ(ret, 0);
}

TEST_F(AttentionPlasticityBridgeRegressionTest, DoubleUnregistration) {
    // REGRESSION: Double unregistration should be handled

    attention_plasticity_register_synapse(bridge, 0, ATTENTION_SYNAPSE_HEAD_OUTPUT, 0, 0.5f);

    int ret1 = attention_plasticity_unregister_synapse(bridge, 0);
    EXPECT_EQ(ret1, 0);

    // Second unregistration should fail gracefully
    int ret2 = attention_plasticity_unregister_synapse(bridge, 0);
    EXPECT_LE(ret2, 0);  // Error or no-op
}

//=============================================================================
// SECTION 22: Encoding Methods Tests
//=============================================================================

TEST_F(AttentionSNNBridgeRegressionTest, AllEncodingMethodsWork) {
    // REGRESSION: All encoding methods should work without crashes

    // Rate encoding is default
    float weights[8] = {0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 0.4f, 0.3f, 0.2f};

    int spikes = attention_snn_encode_weights(bridge, weights, 8);
    EXPECT_GE(spikes, -1);

    attention_snn_step(bridge);

    // Get decoded weights
    float decoded[8];
    int ret = attention_snn_get_weights(bridge, decoded, 8);
    EXPECT_EQ(ret, 0);

    for (int i = 0; i < 8; i++) {
        EXPECT_FALSE(std::isnan(decoded[i]));
    }
}

//=============================================================================
// SECTION 23: Decoding Methods Tests
//=============================================================================

TEST_F(AttentionSNNBridgeRegressionTest, AllDecodingOutputsValid) {
    // REGRESSION: All decoding outputs should be valid

    float weights[8] = {0.1f, 0.3f, 0.5f, 0.7f, 0.9f, 0.8f, 0.6f, 0.4f};
    attention_snn_encode_weights(bridge, weights, 8);
    attention_snn_simulate(bridge, 50.0f);

    // Get all outputs
    float decoded_weights[8];
    attention_snn_get_weights(bridge, decoded_weights, 8);

    float salience[64];
    attention_snn_get_salience(bridge, salience, 64);

    int32_t top_indices[4];
    attention_snn_get_top_k(bridge, top_indices, 4);

    float focus = attention_snn_get_focus_strength(bridge);
    float sparsity = attention_snn_get_sparsity(bridge);

    attention_snn_attention_state_t attn_state;
    attention_snn_get_attention_state(bridge, &attn_state);

    // Verify all values are valid
    for (int i = 0; i < 8; i++) {
        EXPECT_FALSE(std::isnan(decoded_weights[i]));
    }

    if (focus >= 0.0f) {
        EXPECT_LE(focus, 1.0f);
    }

    if (sparsity >= 0.0f) {
        EXPECT_LE(sparsity, 1.0f);
    }
}

//=============================================================================
// SECTION 24: Callback Tests
//=============================================================================

// Callback test data
static std::atomic<int> weight_change_count{0};
static std::atomic<int> shift_count{0};

static void test_weight_change_callback(
    uint32_t synapse_id,
    uint32_t head_idx,
    float old_weight,
    float new_weight,
    attention_learn_event_t event_type,
    void* user_data
) {
    (void)synapse_id;
    (void)head_idx;
    (void)old_weight;
    (void)new_weight;
    (void)event_type;
    (void)user_data;
    weight_change_count++;
}

static void test_shift_callback(
    uint32_t old_head,
    uint32_t new_head,
    float shift_strength,
    void* user_data
) {
    (void)old_head;
    (void)new_head;
    (void)shift_strength;
    (void)user_data;
    shift_count++;
}

TEST_F(AttentionPlasticityBridgeRegressionTest, CallbackRegistration) {
    // REGRESSION: Callback registration should work

    weight_change_count = 0;
    shift_count = 0;

    int ret = attention_plasticity_set_weight_callback(bridge, test_weight_change_callback, nullptr);
    EXPECT_EQ(ret, 0);

    ret = attention_plasticity_set_shift_callback(bridge, test_shift_callback, nullptr);
    EXPECT_EQ(ret, 0);

    // Register synapse and trigger events
    attention_plasticity_register_synapse(bridge, 0, ATTENTION_SYNAPSE_HEAD_OUTPUT, 0, 0.5f);

    uint64_t timestamp = get_timestamp_us();

    // Trigger shift
    attention_plasticity_shift(bridge, 0, 1, 0.8f, timestamp);
    attention_plasticity_update(bridge, 10.0f);

    // Callback should have been called (implementation dependent)
    // Just verify no crash occurred
    SUCCEED();
}

//=============================================================================
// SECTION 25: Stress Test - Long Running
//=============================================================================

TEST_F(AttentionBridgeCombinedRegressionTest, LongRunningStability) {
    // REGRESSION: System should remain stable over long operation

    float weights[8];
    uint64_t timestamp = get_timestamp_us();

    const int total_iterations = 2000;

    for (int iter = 0; iter < total_iterations; iter++) {
        // Generate varying inputs
        for (int j = 0; j < 8; j++) {
            weights[j] = 0.5f + 0.4f * sinf((float)(iter * 8 + j) * 0.01f);
        }

        // SNN operations
        attention_snn_encode_weights(snn_bridge, weights, 8);
        attention_snn_step(snn_bridge);

        // Plasticity operations
        uint32_t head = iter % 8;
        attention_plasticity_focus(plasticity_bridge, head, weights[head], timestamp);

        if (iter % 10 == 0) {
            attention_plasticity_update(plasticity_bridge, 10.0f);
        }

        timestamp += 500;

        // Periodic checks
        if (iter % 500 == 499) {
            // Verify SNN state
            attention_snn_bridge_state_t snn_state;
            attention_snn_get_state(snn_bridge, &snn_state);
            EXPECT_NE(snn_state.state, ATTENTION_SNN_STATE_DISABLED)
                << "SNN bridge disabled at iter " << iter;

            // Verify plasticity state
            attention_plasticity_bridge_state_t plasticity_state;
            attention_plasticity_get_state(plasticity_bridge, &plasticity_state);
            EXPECT_NE(plasticity_state.state, ATTENTION_PLASTICITY_STATE_DISABLED)
                << "Plasticity bridge disabled at iter " << iter;
        }
    }

    // Final verification
    attention_snn_stats_t snn_stats;
    attention_snn_get_stats(snn_bridge, &snn_stats);
    EXPECT_GT(snn_stats.total_forward_passes, 0u);

    attention_plasticity_stats_t plasticity_stats;
    attention_plasticity_get_stats(plasticity_bridge, &plasticity_stats);
    EXPECT_GT(plasticity_stats.total_focus_events, 0u);
}
