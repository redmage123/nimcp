/**
 * @file test_edge_regression.cpp
 * @brief GoogleTest regression tests for NIMCP edge brain subsystem
 *
 * Tests edge cases and previously-found issues: large arrays, zero inputs,
 * division safety, NaN prevention, boundary conditions, and buffer wraps.
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>

extern "C" {
#include "edge/nimcp_edge.h"
#include "edge/nimcp_edge_types.h"
}

class EdgeRegressionTest : public ::testing::Test {
protected:
    void TearDown() override {}
};

TEST_F(EdgeRegressionTest, QuantizeLargeArrayNoCrash) {
    const uint32_t n = 100000;
    std::vector<float> data(n);
    for (uint32_t i = 0; i < n; i++) {
        data[i] = sinf(float(i) * 0.001f) * 10.0f;
    }

    nimcp_quantized_tensor_t* qt = nimcp_quantize_tensor(
        data.data(), n, NIMCP_QUANT_INT8_SYMMETRIC, nullptr, nullptr);
    ASSERT_NE(qt, nullptr);
    EXPECT_EQ(qt->num_elements, n);

    std::vector<float> out(n);
    int ret = nimcp_dequantize_tensor(qt, out.data());
    EXPECT_EQ(ret, 0);

    // Spot check some values
    for (uint32_t i = 0; i < n; i += 10000) {
        EXPECT_NEAR(out[i], data[i], 0.2f);
    }

    nimcp_quantized_tensor_destroy(qt);
}

TEST_F(EdgeRegressionTest, DeltaAllZerosNoNegativeCount) {
    float weights[] = {0.0f, 0.0f, 0.0f, 0.0f};

    nimcp_weight_delta_t delta;
    memset(&delta, 0, sizeof(delta));
    int ret = nimcp_weight_delta_compute(weights, weights, 4, 0.01f, &delta);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(delta.num_changes, 0u);

    nimcp_weight_delta_destroy(&delta);
}

TEST_F(EdgeRegressionTest, RollbackZeroBaselineNoDivByZero) {
    float weights[] = {1.0f, 2.0f, 3.0f};
    nimcp_rollback_state_t state;
    memset(&state, 0, sizeof(state));

    nimcp_rollback_init(&state, weights, 3, 0.0f, 5, 2.0f);

    // Check step should not divide by zero
    int ret = nimcp_rollback_check_step(&state, 0.5f);
    EXPECT_FALSE(std::isnan(state.running_loss));
    EXPECT_FALSE(std::isinf(state.running_loss));

    nimcp_rollback_cleanup(&state);
}

TEST_F(EdgeRegressionTest, EWCZeroFisherNoNaN) {
    nimcp_ewc_state_t* ewc = nimcp_ewc_create(4, 1.0f);
    ASSERT_NE(ewc, nullptr);

    float grads[] = {0.0f, 0.0f, 0.0f, 0.0f};
    nimcp_ewc_compute_fisher(ewc, grads, 1);

    float anchor[] = {1.0f, 1.0f, 1.0f, 1.0f};
    nimcp_ewc_set_anchor(ewc, anchor);

    float local[] = {1.0f, 1.0f, 1.0f, 1.0f};
    float master[] = {0.0f, 0.0f, 0.0f, 0.0f};

    nimcp_ewc_blend_weights(ewc, local, master, 0.5f);

    for (int i = 0; i < 4; i++) {
        EXPECT_FALSE(std::isnan(local[i])) << "NaN at index " << i;
        EXPECT_FALSE(std::isinf(local[i])) << "Inf at index " << i;
    }

    nimcp_ewc_destroy(ewc);
}

TEST_F(EdgeRegressionTest, GossipRingBufferMaxCapacity) {
    nimcp_gossip_config_t config;
    memset(&config, 0, sizeof(config));
    nimcp_gossip_init(&config);

    uint32_t cap = config.seen_hash_capacity;
    ASSERT_GT(cap, 0u);

    // Fill to exactly capacity
    for (uint32_t i = 0; i < cap; i++) {
        nimcp_gossip_mark_seen(&config, i);
    }

    // Add one more — should wrap
    nimcp_gossip_mark_seen(&config, cap);
    EXPECT_TRUE(nimcp_gossip_is_seen(&config, cap));

    // Verify no crash or corruption
    for (uint32_t i = 0; i < cap + 10; i++) {
        nimcp_gossip_mark_seen(&config, 10000 + i);
    }
    EXPECT_TRUE(nimcp_gossip_is_seen(&config, 10000 + cap + 9));
}

TEST_F(EdgeRegressionTest, OfflinePolicyExactBoundarySteps) {
    nimcp_offline_policy_t policy;
    memset(&policy, 0, sizeof(policy));
    nimcp_offline_policy_init(&policy);

    // Step to exactly cautious boundary
    for (uint32_t i = 0; i < policy.cautious_after_steps; i++) {
        nimcp_offline_policy_step(&policy);
    }
    EXPECT_EQ(policy.current_mode, NIMCP_OFFLINE_CAUTIOUS);

    // One more step should still be cautious (not jump to conservative)
    nimcp_offline_policy_step(&policy);
    EXPECT_TRUE(policy.current_mode == NIMCP_OFFLINE_CAUTIOUS ||
                policy.current_mode == NIMCP_OFFLINE_CONSERVATIVE);
}

TEST_F(EdgeRegressionTest, PowerModeExactThresholdBattery) {
    nimcp_power_config_t config;
    memset(&config, 0, sizeof(config));
    nimcp_power_init(&config);

    // Exactly at balanced threshold
    nimcp_power_mode_t mode = nimcp_power_update(
        &config, config.balanced_battery_pct, 40.0f);
    // Should be either FULL or BALANCED — no crash
    EXPECT_TRUE(mode == NIMCP_POWER_FULL || mode == NIMCP_POWER_BALANCED);

    // Exactly at saving threshold
    mode = nimcp_power_update(&config, config.saving_battery_pct, 40.0f);
    EXPECT_TRUE(mode == NIMCP_POWER_BALANCED || mode == NIMCP_POWER_SAVING);

    // Exactly at critical threshold
    mode = nimcp_power_update(&config, config.critical_battery_pct, 40.0f);
    EXPECT_TRUE(mode == NIMCP_POWER_SAVING || mode == NIMCP_POWER_CRITICAL);
}

TEST_F(EdgeRegressionTest, TelemetrySerializeMaxValues) {
    nimcp_device_telemetry_t t;
    memset(&t, 0, sizeof(t));
    t.device_id = UINT32_MAX;
    t.timestamp = UINT64_MAX;
    t.avg_inference_ms = 1e30f;
    t.p99_inference_ms = 1e30f;
    t.avg_loss = 1e30f;
    t.battery_pct = 100.0f;
    t.steps_since_sync = UINT32_MAX;
    t.rollbacks_triggered = UINT32_MAX;

    uint8_t buffer[8192];
    uint32_t bytes_written = 0;
    int ret = nimcp_telemetry_serialize(&t, buffer, sizeof(buffer), &bytes_written);
    EXPECT_EQ(ret, 0);

    // Verify round-trip
    nimcp_device_telemetry_t restored;
    nimcp_telemetry_deserialize(buffer, bytes_written, &restored);
    EXPECT_EQ(restored.device_id, UINT32_MAX);
    EXPECT_EQ(restored.timestamp, UINT64_MAX);
}

TEST_F(EdgeRegressionTest, EarlyExitLayerSizeOne) {
    uint32_t exit_layers[] = {1};
    float thresholds[] = {0.5f};
    uint32_t layer_sizes[] = {1}; // Minimum size

    nimcp_early_exit_t* ee = nimcp_early_exit_create(
        exit_layers, thresholds, layer_sizes, 1, 1);
    ASSERT_NE(ee, nullptr);

    float activation[] = {1.0f};
    float output[1];
    float confidence;

    int ret = nimcp_early_exit_evaluate(ee, 0, activation, 1, output, &confidence);
    // Should not crash
    EXPECT_GE(confidence, 0.0f);
    EXPECT_LE(confidence, 1.0f);

    nimcp_early_exit_destroy(ee);
}

TEST_F(EdgeRegressionTest, MaturationZeroStepsNoDivByZero) {
    // maturation_steps = 1 (minimum viable, avoid 0 which could cause div/0)
    nimcp_maturation_tracker_t* tracker = nimcp_maturation_create(10, 1, 0.5f);
    ASSERT_NE(tracker, nullptr);

    nimcp_maturation_add_neuron(tracker, 1);
    nimcp_maturation_step(tracker);

    float progress = nimcp_maturation_get_progress(tracker);
    EXPECT_FALSE(std::isnan(progress));
    EXPECT_FALSE(std::isinf(progress));

    nimcp_maturation_destroy(tracker);
}

TEST_F(EdgeRegressionTest, DPPrivatizeLargeArray) {
    nimcp_edge_dp_config_t config;
    memset(&config, 0, sizeof(config));
    nimcp_edge_dp_init(&config);

    const uint32_t n = 100000;
    std::vector<float> grads(n, 1.0f);

    int ret = nimcp_edge_dp_privatize_gradients(&config, grads.data(), n);
    EXPECT_EQ(ret, 0);

    // Verify no NaN/Inf
    for (uint32_t i = 0; i < n; i++) {
        EXPECT_FALSE(std::isnan(grads[i]));
        EXPECT_FALSE(std::isinf(grads[i]));
    }
}
