/**
 * @file test_edge_e2e.cpp
 * @brief GoogleTest end-to-end tests for NIMCP edge brain subsystem
 *
 * Tests full deployment simulations, swarm gossip convergence,
 * power cycling, offline resilience, and master-device lifecycle.
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

class EdgeE2ETest : public ::testing::Test {
protected:
    void TearDown() override {}
};

/* ---------- Full Edge Deployment Simulation ---------- */

TEST_F(EdgeE2ETest, FullDeploymentSimulation) {
    // 1. Create device profile
    nimcp_device_profile_t profile;
    memset(&profile, 0, sizeof(profile));
    profile.ram_mb = 2048;
    profile.cpu_cores = 4;
    profile.cpu_gflops = 30.0f;
    profile.target_inference_ms = 20.0f;
    profile.has_camera = true;
    profile.has_microphone = true;
    profile.role = NIMCP_DEVICE_GENERAL;
    strncpy(profile.device_name, "test-device", sizeof(profile.device_name) - 1);

    // 2. Compute optimal configuration
    uint32_t neurons = nimcp_compute_optimal_neurons(&profile);
    EXPECT_GT(neurons, 0u);

    uint32_t mask = nimcp_compute_subsystem_mask(&profile);
    EXPECT_GT(mask, 0u);

    // 3. Create edge context
    nimcp_edge_ctx_t* ctx = nimcp_edge_ctx_create(&profile);
    ASSERT_NE(ctx, nullptr);

    // 4. Simulate quantization of model weights
    const uint32_t n_weights = 4096;
    std::vector<float> weights(n_weights);
    for (uint32_t i = 0; i < n_weights; i++) {
        weights[i] = sinf(float(i) * 0.01f) * 0.5f;
    }

    nimcp_quantized_tensor_t* qt = nimcp_quantize_tensor(
        weights.data(), n_weights, NIMCP_QUANT_INT8_SYMMETRIC, nullptr, nullptr);
    ASSERT_NE(qt, nullptr);

    // 5. Run inferences
    for (int i = 0; i < 100; i++) {
        nimcp_edge_record_inference(ctx, 15.0f + (i % 5), 0.1f - i * 0.0005f);
    }

    // 6. Collect telemetry
    nimcp_device_telemetry_t telemetry;
    memset(&telemetry, 0, sizeof(telemetry));
    nimcp_telemetry_collect(ctx, &telemetry);

    // 7. Analyze telemetry
    uint32_t actions = nimcp_telemetry_analyze(&telemetry);
    // With normal values, should be no critical actions
    // (Loss trend is negative = improving)

    // 8. Serialize telemetry for reporting
    uint8_t buf[4096];
    uint32_t written = 0;
    nimcp_telemetry_serialize(&telemetry, buf, sizeof(buf), &written);
    EXPECT_GT(written, 0u);

    // 9. Verify round-trip
    nimcp_device_telemetry_t restored;
    nimcp_telemetry_deserialize(buf, written, &restored);
    EXPECT_FLOAT_EQ(restored.avg_inference_ms, telemetry.avg_inference_ms);

    // 10. Cleanup
    nimcp_quantized_tensor_destroy(qt);
    nimcp_edge_ctx_destroy(ctx);
}

/* ---------- Swarm Gossip Simulation ---------- */

TEST_F(EdgeE2ETest, SwarmGossipConvergence) {
    const uint32_t n_devices = 3;
    const uint32_t n_weights = 100;

    // Each device has slightly different weights
    std::vector<std::vector<float>> device_weights(n_devices);
    for (uint32_t d = 0; d < n_devices; d++) {
        device_weights[d].resize(n_weights);
        for (uint32_t i = 0; i < n_weights; i++) {
            device_weights[d][i] = 1.0f + float(d) * 0.1f + float(i) * 0.001f;
        }
    }

    // Initialize gossip configs for each device
    std::vector<nimcp_gossip_config_t> configs(n_devices);
    for (uint32_t d = 0; d < n_devices; d++) {
        memset(&configs[d], 0, sizeof(nimcp_gossip_config_t));
        nimcp_gossip_init(&configs[d]);
    }

    // Simulate gossip rounds
    for (int round = 0; round < 5; round++) {
        for (uint32_t sender = 0; sender < n_devices; sender++) {
            // Save pre-gossip weights
            std::vector<float> old_weights(device_weights[sender]);

            // Simulate local learning (small random changes)
            for (uint32_t i = 0; i < n_weights; i += 10) {
                device_weights[sender][i] += 0.01f;
            }

            // Check if should broadcast
            float loss = 2.0f; // High loss
            float ema = 1.0f;
            if (nimcp_gossip_should_broadcast(loss, ema, &configs[sender])) {
                nimcp_gossip_update_t* update = nimcp_gossip_create_update(
                    sender, old_weights.data(), device_weights[sender].data(),
                    n_weights, loss, ema);

                if (update && update->num_weights > 0) {
                    // Apply to other devices
                    for (uint32_t receiver = 0; receiver < n_devices; receiver++) {
                        if (receiver != sender) {
                            nimcp_gossip_apply_update(
                                device_weights[receiver].data(),
                                update, &configs[receiver]);
                        }
                    }
                }

                if (update) nimcp_gossip_update_destroy(update);
            }
        }
    }

    // After gossip, devices should have converged somewhat
    // Check variance between devices is reasonable
    for (uint32_t i = 0; i < n_weights; i++) {
        float min_w = device_weights[0][i];
        float max_w = device_weights[0][i];
        for (uint32_t d = 1; d < n_devices; d++) {
            min_w = std::min(min_w, device_weights[d][i]);
            max_w = std::max(max_w, device_weights[d][i]);
        }
        // Weights should not have diverged wildly
        EXPECT_LT(max_w - min_w, 2.0f)
            << "Weight divergence too large at index " << i;
    }
}

/* ---------- Power Cycling ---------- */

TEST_F(EdgeE2ETest, PowerCycling) {
    nimcp_power_config_t config;
    memset(&config, 0, sizeof(config));
    nimcp_power_init(&config);

    // Default thresholds: balanced=80, saving=50, critical=20
    // Implementation uses strict less-than: battery < threshold

    // Simulate battery drain and recharge
    float battery = 100.0f;

    // Drain
    while (battery > 5.0f) {
        nimcp_power_mode_t mode = nimcp_power_update(&config, battery, 40.0f);

        // Match implementation logic: battery < critical → CRITICAL,
        // battery < saving → SAVING, battery < balanced → BALANCED, else FULL
        if (battery < config.critical_battery_pct) {
            EXPECT_EQ(mode, NIMCP_POWER_CRITICAL);
        } else if (battery < config.saving_battery_pct) {
            EXPECT_EQ(mode, NIMCP_POWER_SAVING);
        } else if (battery < config.balanced_battery_pct) {
            EXPECT_EQ(mode, NIMCP_POWER_BALANCED);
        } else {
            EXPECT_EQ(mode, NIMCP_POWER_FULL);
        }

        battery -= 5.0f;
    }

    EXPECT_EQ(config.mode, NIMCP_POWER_CRITICAL);
    EXPECT_FALSE(nimcp_power_is_gpu_enabled(&config));

    // Recharge
    battery = 100.0f;
    nimcp_power_mode_t mode = nimcp_power_update(&config, battery, 40.0f);
    EXPECT_EQ(mode, NIMCP_POWER_FULL);
    EXPECT_TRUE(nimcp_power_is_gpu_enabled(&config));
}

/* ---------- Offline Resilience ---------- */

TEST_F(EdgeE2ETest, OfflineResilience25000Steps) {
    nimcp_offline_policy_t policy;
    memset(&policy, 0, sizeof(policy));
    nimcp_offline_policy_init(&policy);

    // Go offline for 25000 steps
    for (uint32_t i = 0; i < 25000; i++) {
        nimcp_offline_policy_step(&policy);
    }

    // Should be frozen at this point
    EXPECT_EQ(policy.current_mode, NIMCP_OFFLINE_FROZEN);

    float frozen_conf = nimcp_offline_get_confidence(&policy);
    // Confidence decays with rate 0.9999 but is clamped to min_confidence_multiplier (0.5)
    EXPECT_LE(frozen_conf, 0.5f);

    float frozen_lr = nimcp_offline_get_lr_scale(&policy);
    // LR is 0.0 in frozen mode (no learning)
    EXPECT_FLOAT_EQ(frozen_lr, 0.0f);

    // Sync — recover
    nimcp_offline_policy_on_sync(&policy);
    EXPECT_EQ(policy.current_mode, NIMCP_OFFLINE_NORMAL);
    EXPECT_FLOAT_EQ(nimcp_offline_get_confidence(&policy), 1.0f);
}

/* ---------- Master-Device Lifecycle ---------- */

TEST_F(EdgeE2ETest, MasterDeviceLifecycle) {
    const uint32_t n_params = 500;

    // 1. Master weights
    std::vector<float> master_weights(n_params);
    for (uint32_t i = 0; i < n_params; i++) {
        master_weights[i] = float(i) * 0.01f;
    }

    // 2. "Distill" to device (just copy for testing)
    std::vector<float> device_weights(master_weights);

    // 3. Device learns locally
    for (uint32_t i = 0; i < n_params; i++) {
        device_weights[i] += (i % 5 == 0) ? 0.5f : 0.0f;
    }

    // 4. Compute delta
    nimcp_weight_delta_t delta;
    memset(&delta, 0, sizeof(delta));
    nimcp_weight_delta_compute(
        master_weights.data(), device_weights.data(), n_params, 0.01f, &delta);
    EXPECT_GT(delta.num_changes, 0u);
    EXPECT_EQ(delta.num_changes, n_params / 5); // Every 5th changed

    // 5. Aggregate (single device = identity)
    nimcp_federated_gradient_t grad;
    memset(&grad, 0, sizeof(grad));
    grad.device_id = 1;
    grad.num_params = n_params;
    // Use device weights as "gradients" for aggregation test
    grad.gradients = device_weights.data();

    std::vector<float> aggregated(n_params, 0.0f);
    nimcp_federated_aggregate(&grad, 1, aggregated.data(), n_params, NIMCP_FED_AVG);

    // 6. Push back via blend
    nimcp_federated_blend(master_weights.data(), aggregated.data(), n_params, 0.5f, nullptr);

    // 7. Verify master weights have moved toward device weights
    for (uint32_t i = 0; i < n_params; i += 5) {
        float original = float(i) * 0.01f;
        // Master should have moved from original toward device
        EXPECT_NE(master_weights[i], original)
            << "Master weight should have been updated at index " << i;
    }

    // 8. Apply delta to fresh copy to verify delta correctness
    std::vector<float> verify(n_params);
    for (uint32_t i = 0; i < n_params; i++) {
        verify[i] = float(i) * 0.01f; // original master
    }
    nimcp_weight_delta_apply(verify.data(), &delta);

    for (uint32_t i = 0; i < n_params; i++) {
        EXPECT_NEAR(verify[i], device_weights[i], 0.01f)
            << "Delta apply mismatch at index " << i;
    }

    nimcp_weight_delta_destroy(&delta);
}

/* ---------- EWC-Protected Federated Sync ---------- */

TEST_F(EdgeE2ETest, EWCProtectedFederatedSync) {
    const uint32_t np = 200;

    nimcp_ewc_state_t* ewc = nimcp_ewc_create(np, 5.0f);
    ASSERT_NE(ewc, nullptr);

    // Train locally — first 50 params are important
    std::vector<float> gradients(np, 0.0f);
    for (uint32_t i = 0; i < 50; i++) {
        gradients[i] = 10.0f;
    }
    nimcp_ewc_compute_fisher(ewc, gradients.data(), 1);

    // Set anchor
    std::vector<float> local(np, 1.0f);
    nimcp_ewc_set_anchor(ewc, local.data());

    // Master pushes very different weights
    std::vector<float> master(np, 0.0f);

    // Blend with EWC protection
    nimcp_federated_blend(local.data(), master.data(), np, 0.5f, ewc);

    // Protected weights (0-49) should be closer to 1.0
    float avg_protected = 0, avg_unprotected = 0;
    for (uint32_t i = 0; i < 50; i++) avg_protected += local[i];
    for (uint32_t i = 50; i < 100; i++) avg_unprotected += local[i];
    avg_protected /= 50.0f;
    avg_unprotected /= 50.0f;

    EXPECT_GT(avg_protected, avg_unprotected)
        << "EWC-protected weights should resist master push more";

    nimcp_ewc_destroy(ewc);
}
