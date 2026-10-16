/**
 * @file test_edge_integration.cpp
 * @brief GoogleTest integration tests for NIMCP edge brain subsystem
 *
 * Tests cross-subsystem interactions: quantize+dequantize on real arrays,
 * rollback+check flows, EWC+federated blend, gossip+seen hash,
 * maturation lifecycle, OTA lifecycle, and device config pipelines.
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>
#include <algorithm>

extern "C" {
#include "edge/nimcp_edge.h"
#include "edge/nimcp_edge_types.h"
}

class EdgeIntegrationTest : public ::testing::Test {
protected:
    void TearDown() override {}
};

/* ---------- Full Lifecycle ---------- */

TEST_F(EdgeIntegrationTest, EdgeContextLifecycle) {
    nimcp_device_profile_t profile = nimcp_device_profile_default();
    nimcp_edge_ctx_t* ctx = nimcp_edge_ctx_create(&profile);
    ASSERT_NE(ctx, nullptr);

    // Record some inferences
    for (int i = 0; i < 50; i++) {
        nimcp_edge_record_inference(ctx, 5.0f + i * 0.1f, 0.1f - i * 0.001f);
    }
    EXPECT_EQ(ctx->total_steps, 50u);

    // Collect telemetry
    nimcp_device_telemetry_t telemetry;
    memset(&telemetry, 0, sizeof(telemetry));
    nimcp_telemetry_collect(ctx, &telemetry);

    EXPECT_GT(telemetry.avg_inference_ms, 0.0f);

    nimcp_edge_ctx_destroy(ctx);
}

/* ---------- Quantize Round-trip Real Arrays ---------- */

TEST_F(EdgeIntegrationTest, QuantizeRoundTrip4096Elements) {
    const uint32_t n = 4096;
    std::vector<float> data(n);
    for (uint32_t i = 0; i < n; i++) {
        data[i] = sinf(float(i) * 0.01f);
    }

    nimcp_quantized_tensor_t* qt = nimcp_quantize_tensor(
        data.data(), n, NIMCP_QUANT_INT8_SYMMETRIC, nullptr, nullptr);
    ASSERT_NE(qt, nullptr);

    std::vector<float> out(n);
    nimcp_dequantize_tensor(qt, out.data());

    float max_err = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        float err = std::fabs(out[i] - data[i]);
        if (err > max_err) max_err = err;
    }
    EXPECT_LT(max_err, 0.02f);

    nimcp_quantized_tensor_destroy(qt);
}

/* ---------- Rollback Integration ---------- */

TEST_F(EdgeIntegrationTest, RollbackFullFlow) {
    const uint32_t n = 100;
    std::vector<float> weights(n, 0.5f);

    nimcp_rollback_state_t state;
    memset(&state, 0, sizeof(state));
    nimcp_rollback_init(&state, weights.data(), n, 0.5f, 10, 2.0f);

    // Good steps
    for (int i = 0; i < 5; i++) {
        nimcp_rollback_check_step(&state, 0.3f);
    }
    EXPECT_FALSE(state.rollback_triggered);

    // Bad step
    nimcp_rollback_check_step(&state, 10.0f);

    if (state.rollback_triggered) {
        std::vector<float> modified(n, 99.0f);
        nimcp_rollback_execute(&state, modified.data());

        for (uint32_t i = 0; i < n; i++) {
            EXPECT_FLOAT_EQ(modified[i], 0.5f);
        }
    }

    nimcp_rollback_cleanup(&state);
}

/* ---------- Early Exit + Power ---------- */

TEST_F(EdgeIntegrationTest, EarlyExitWithPowerMode) {
    uint32_t exit_layers[] = {1, 3};
    float thresholds[] = {0.9f, 0.5f};
    uint32_t layer_sizes[] = {8, 8};

    nimcp_early_exit_t* ee = nimcp_early_exit_create(
        exit_layers, thresholds, layer_sizes, 2, 4);
    ASSERT_NE(ee, nullptr);

    nimcp_power_config_t power;
    memset(&power, 0, sizeof(power));
    nimcp_power_init(&power);

    // Update power mode to saving
    nimcp_power_update(&power, 30.0f, 40.0f);

    // In saving mode, we might adjust thresholds
    // Just verify both subsystems work together without crash
    float activation[] = {1.0f, 0.5f, 0.2f, 0.1f, 0.0f, 0.0f, 0.0f, 0.0f};
    float output[4];
    float confidence;

    nimcp_early_exit_evaluate(ee, 0, activation, 8, output, &confidence);
    EXPECT_GE(confidence, 0.0f);
    EXPECT_LE(confidence, 1.0f);

    nimcp_early_exit_destroy(ee);
}

/* ---------- Offline + Sync ---------- */

TEST_F(EdgeIntegrationTest, OfflineDegradeAndRecover) {
    nimcp_offline_policy_t policy;
    memset(&policy, 0, sizeof(policy));
    nimcp_offline_policy_init(&policy);

    // Go offline
    for (uint32_t i = 0; i < policy.conservative_after_steps; i++) {
        nimcp_offline_policy_step(&policy);
    }
    EXPECT_EQ(policy.current_mode, NIMCP_OFFLINE_CONSERVATIVE);
    float low_conf = nimcp_offline_get_confidence(&policy);

    // Sync — recover
    nimcp_offline_policy_on_sync(&policy);
    EXPECT_EQ(policy.current_mode, NIMCP_OFFLINE_NORMAL);
    float high_conf = nimcp_offline_get_confidence(&policy);

    EXPECT_GT(high_conf, low_conf);
}

/* ---------- EWC + Federated Blend ---------- */

TEST_F(EdgeIntegrationTest, EWCProtectsWeightsInFederatedBlend) {
    const uint32_t np = 8;
    nimcp_ewc_state_t* ewc = nimcp_ewc_create(np, 10.0f);
    ASSERT_NE(ewc, nullptr);

    // Make weights 0-3 important, 4-7 unimportant
    float gradients[8] = {10.0f, 10.0f, 10.0f, 10.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    nimcp_ewc_compute_fisher(ewc, gradients, 1);

    float anchor[8] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
    nimcp_ewc_set_anchor(ewc, anchor);

    float device[8] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
    float master[8] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};

    nimcp_federated_blend(device, master, np, 0.5f, ewc);

    // Protected weights (0-3) should stay closer to 1.0
    float avg_protected = (device[0] + device[1] + device[2] + device[3]) / 4.0f;
    float avg_unprotected = (device[4] + device[5] + device[6] + device[7]) / 4.0f;

    EXPECT_GT(avg_protected, avg_unprotected);

    nimcp_ewc_destroy(ewc);
}

/* ---------- Gossip + Seen Hash ---------- */

TEST_F(EdgeIntegrationTest, GossipCreateApplyReapply) {
    nimcp_gossip_config_t config;
    memset(&config, 0, sizeof(config));
    nimcp_gossip_init(&config);

    float old_w[] = {1.0f, 2.0f, 3.0f};
    float new_w[] = {1.5f, 2.5f, 3.5f};

    nimcp_gossip_update_t* update = nimcp_gossip_create_update(
        1, old_w, new_w, 3, 2.0f, 1.0f);
    ASSERT_NE(update, nullptr);

    float local[] = {1.0f, 2.0f, 3.0f};
    nimcp_gossip_apply_update(local, update, &config);

    // Mark as seen
    nimcp_gossip_mark_seen(&config, update->experience_hash);

    // Re-apply — should be skipped
    float local2[] = {1.0f, 2.0f, 3.0f};
    nimcp_gossip_apply_update(local2, update, &config);

    // local2 should be unchanged (skipped)
    EXPECT_FLOAT_EQ(local2[0], 1.0f);
    EXPECT_FLOAT_EQ(local2[1], 2.0f);
    EXPECT_FLOAT_EQ(local2[2], 3.0f);

    nimcp_gossip_update_destroy(update);
}

/* ---------- Delta Compute + Apply ---------- */

TEST_F(EdgeIntegrationTest, DeltaComputeApplyVerify) {
    const uint32_t n = 1000;
    std::vector<float> old_w(n), new_w(n);
    for (uint32_t i = 0; i < n; i++) {
        old_w[i] = float(i) * 0.01f;
        new_w[i] = old_w[i] + (i % 3 == 0 ? 1.0f : 0.0f);
    }

    nimcp_weight_delta_t delta;
    memset(&delta, 0, sizeof(delta));
    nimcp_weight_delta_compute(old_w.data(), new_w.data(), n, 0.01f, &delta);

    EXPECT_GT(delta.num_changes, 0u);
    EXPECT_LE(delta.num_changes, n);

    // Apply to old weights
    std::vector<float> target(old_w);
    nimcp_weight_delta_apply(target.data(), &delta);

    for (uint32_t i = 0; i < n; i++) {
        EXPECT_NEAR(target[i], new_w[i], 0.01f);
    }

    nimcp_weight_delta_destroy(&delta);
}

/* ---------- OTA Full Lifecycle ---------- */

TEST_F(EdgeIntegrationTest, OTAFullLifecycle) {
    nimcp_ota_state_t state;
    memset(&state, 0, sizeof(state));
    nimcp_ota_init(&state);

    float staged[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    uint8_t checksum[32] = {0};
    nimcp_ota_stage_weights(&state, staged, 5, checksum);
    EXPECT_EQ(state.stage, NIMCP_OTA_VALIDATING);

    // Skip validation, force ready
    state.stage = NIMCP_OTA_READY_TO_SWAP;

    bool safe = nimcp_ota_is_safe_to_swap(0.0f, false, false, 80.0f);
    EXPECT_TRUE(safe);

    float active[] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    nimcp_ota_swap(&state, active);

    for (int i = 0; i < 5; i++) {
        EXPECT_FLOAT_EQ(active[i], staged[i]);
    }

    nimcp_ota_cleanup(&state);
}

/* ---------- Maturation Full Lifecycle ---------- */

TEST_F(EdgeIntegrationTest, MaturationFullLifecycle100Neurons) {
    // Full lifecycle: PROGENITOR(100) + IMMATURE(400) + INTEGRATING(500) = 1000
    nimcp_maturation_tracker_t* tracker = nimcp_maturation_create(100, 1000, 0.5f);
    ASSERT_NE(tracker, nullptr);

    for (uint32_t i = 0; i < 100; i++) {
        nimcp_maturation_add_neuron(tracker, i);
    }

    for (int step = 0; step < 1100; step++) {
        nimcp_maturation_step(tracker);
    }

    float progress = nimcp_maturation_get_progress(tracker);
    EXPECT_NEAR(progress, 1.0f, 0.01f);

    // All neurons should be mature
    for (uint32_t i = 0; i < 100; i++) {
        float scale = nimcp_maturation_get_output_scale(tracker, i);
        EXPECT_NEAR(scale, 1.0f, 0.01f);
    }

    nimcp_maturation_destroy(tracker);
}

/* ---------- Power Mode Transitions ---------- */

TEST_F(EdgeIntegrationTest, PowerModeTransitionsAllStates) {
    nimcp_power_config_t config;
    memset(&config, 0, sizeof(config));
    nimcp_power_init(&config);

    struct { float battery; nimcp_power_mode_t expected; } tests[] = {
        {100.0f, NIMCP_POWER_FULL},
        {70.0f, NIMCP_POWER_BALANCED},
        {30.0f, NIMCP_POWER_SAVING},
        {10.0f, NIMCP_POWER_CRITICAL},
        {100.0f, NIMCP_POWER_FULL}, // recovery
    };

    for (auto& t : tests) {
        nimcp_power_mode_t mode = nimcp_power_update(&config, t.battery, 40.0f);
        EXPECT_EQ(mode, t.expected)
            << "At battery " << t.battery << "%";
    }
}

/* ---------- Telemetry Analysis Multiple Conditions ---------- */

TEST_F(EdgeIntegrationTest, TelemetryAnalysisMultipleConditions) {
    nimcp_device_telemetry_t t;
    memset(&t, 0, sizeof(t));
    t.avg_loss = 0.8f;
    t.loss_trend = 0.2f;       // > 0.1 threshold
    t.anomaly_rate = 25.0f;    // > 20.0 threshold (percentage)
    t.battery_pct = 5.0f;      // < 10.0 threshold (strict less than)
    t.rollbacks_triggered = 10; // > 2 threshold

    uint32_t actions = nimcp_telemetry_analyze(&t);

    // Multiple actions should be triggered
    EXPECT_TRUE(actions & NIMCP_TELEMETRY_ACTION_REDISTILL);
    EXPECT_TRUE(actions & NIMCP_TELEMETRY_ACTION_ALERT_ANOMALY);
    EXPECT_TRUE(actions & NIMCP_TELEMETRY_ACTION_POWER_SAVE);
    EXPECT_TRUE(actions & NIMCP_TELEMETRY_ACTION_STOP_UPDATES);
}

/* ---------- Version Compatibility Matrix ---------- */

TEST_F(EdgeIntegrationTest, VersionCompatibilityMatrix) {
    uint32_t layers_a[] = {128, 256, 128};
    uint32_t layers_b[] = {256, 512, 256};

    struct {
        uint32_t maj1, min1, maj2, min2;
        uint32_t* l1; uint32_t n1;
        uint32_t* l2; uint32_t n2;
        bool arch_compat;
    } cases[] = {
        {1, 0, 1, 0, layers_a, 3, layers_a, 3, true},   // Same
        {1, 0, 1, 5, layers_a, 3, layers_a, 3, true},   // Minor diff
        {1, 0, 2, 0, layers_a, 3, layers_a, 3, false},  // Major diff
        {1, 0, 1, 0, layers_a, 3, layers_b, 3, false},  // Diff arch
    };

    for (int i = 0; i < 4; i++) {
        nimcp_model_version_t v1 = nimcp_version_create(
            cases[i].maj1, cases[i].min1, 0, cases[i].l1, cases[i].n1);
        nimcp_model_version_t v2 = nimcp_version_create(
            cases[i].maj2, cases[i].min2, 0, cases[i].l2, cases[i].n2);

        nimcp_compatibility_result_t result;
        nimcp_version_check_compatibility(&v1, &v2, &result);

        EXPECT_EQ(result.architecturally_compatible, cases[i].arch_compat)
            << "Case " << i;
    }
}

/* ---------- Config Defaults No Crash ---------- */

TEST_F(EdgeIntegrationTest, AllConfigDefaultsNoCrash) {
    nimcp_resize_config_t r = nimcp_resize_config_default();
    nimcp_distill_config_t d = nimcp_distill_config_default();
    nimcp_quantize_config_t q = nimcp_quantize_config_default();
    nimcp_federated_config_t f = nimcp_federated_config_default();
    nimcp_power_config_t p = nimcp_power_config_default();
    nimcp_offline_policy_t o = nimcp_offline_policy_default();
    nimcp_gossip_config_t g = nimcp_gossip_config_default();
    nimcp_edge_dp_config_t dp = nimcp_dp_config_default();
    nimcp_device_profile_t dev = nimcp_device_profile_default();

    // Just verify no crash and some basic sanity
    // target_neuron_count defaults to 0 (must be set by caller)
    EXPECT_EQ(r.target_neuron_count, 0u);
    EXPECT_GT(d.target_neurons, 0u);
    EXPECT_GT(f.blend_ratio, 0.0f);
    EXPECT_GT(p.thermal_throttle_c, 0.0f);
    EXPECT_GT(o.frozen_after_steps, 0u);
    EXPECT_GT(g.max_ttl, 0u);
    EXPECT_GT(dp.privacy_budget_epsilon, 0.0f);
    EXPECT_GT(dev.ram_mb, 0u);
    (void)q;
}

/* ---------- Device Profile Pipeline ---------- */

TEST_F(EdgeIntegrationTest, DeviceProfileToNeuronsToMask) {
    nimcp_device_profile_t device;
    memset(&device, 0, sizeof(device));
    device.ram_mb = 2048;
    device.cpu_cores = 4;
    device.cpu_gflops = 50.0f;
    device.target_inference_ms = 20.0f;
    device.has_camera = true;
    device.has_microphone = true;
    device.role = NIMCP_DEVICE_COORDINATOR;

    uint32_t neurons = nimcp_compute_optimal_neurons(&device);
    EXPECT_GT(neurons, 0u);

    uint32_t mask = nimcp_compute_subsystem_mask(&device);
    EXPECT_GT(mask, 0u);
}
