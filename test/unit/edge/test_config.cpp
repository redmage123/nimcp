/**
 * @file test_config.cpp
 * @brief GoogleTest unit tests for NIMCP edge default configurations
 *
 * Tests that all default configs return valid values, optimal neuron
 * calculation, subsystem mask computation, and device profile defaults.
 */

#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "edge/nimcp_edge.h"
#include "edge/nimcp_edge_types.h"
}

class ConfigTest : public ::testing::Test {
protected:
    void TearDown() override {}
};

TEST_F(ConfigTest, ResizeConfigDefaultValid) {
    nimcp_resize_config_t config = nimcp_resize_config_default();
    // target_neuron_count defaults to 0 (must be set by caller)
    EXPECT_EQ(config.target_neuron_count, 0u);
    EXPECT_GT(config.maturation_steps, 0u);
    EXPECT_GT(config.fan_in_target, 0u);
    EXPECT_GT(config.initial_weight_scale, 0.0f);
}

TEST_F(ConfigTest, DistillConfigDefaultValid) {
    nimcp_distill_config_t config = nimcp_distill_config_default();
    EXPECT_GT(config.target_neurons, 0u);
    EXPECT_GT(config.distillation_steps, 0u);
    EXPECT_GT(config.temperature, 0.0f);
}

TEST_F(ConfigTest, QuantizeConfigDefaultValid) {
    nimcp_quantize_config_t config = nimcp_quantize_config_default();
    // Should have a valid precision
    EXPECT_GE((int)config.weight_precision, (int)NIMCP_QUANT_NONE);
}

TEST_F(ConfigTest, FederatedConfigDefaultValid) {
    nimcp_federated_config_t config = nimcp_federated_config_default();
    EXPECT_GT(config.blend_ratio, 0.0f);
    EXPECT_LE(config.blend_ratio, 1.0f);
    EXPECT_GT(config.sync_interval_steps, 0u);
}

TEST_F(ConfigTest, PowerConfigDefaultValid) {
    nimcp_power_config_t config = nimcp_power_config_default();
    EXPECT_GT(config.balanced_battery_pct, 0.0f);
    EXPECT_GT(config.saving_battery_pct, 0.0f);
    EXPECT_GT(config.critical_battery_pct, 0.0f);
    EXPECT_GT(config.thermal_throttle_c, 0.0f);

    // Thresholds should be ordered
    EXPECT_GT(config.balanced_battery_pct, config.saving_battery_pct);
    EXPECT_GT(config.saving_battery_pct, config.critical_battery_pct);
}

TEST_F(ConfigTest, OfflinePolicyDefaultValid) {
    nimcp_offline_policy_t policy = nimcp_offline_policy_default();
    EXPECT_GT(policy.cautious_after_steps, 0u);
    EXPECT_GT(policy.conservative_after_steps, policy.cautious_after_steps);
    EXPECT_GT(policy.frozen_after_steps, policy.conservative_after_steps);
    EXPECT_GT(policy.confidence_decay_rate, 0.0f);
}

TEST_F(ConfigTest, GossipConfigDefaultValid) {
    nimcp_gossip_config_t config = nimcp_gossip_config_default();
    EXPECT_GT(config.gossip_blend_ratio, 0.0f);
    EXPECT_GT(config.max_ttl, 0u);
    EXPECT_GT(config.broadcast_loss_ratio, 0.0f);
}

TEST_F(ConfigTest, DPConfigDefaultValid) {
    nimcp_edge_dp_config_t config = nimcp_dp_config_default();
    EXPECT_GT(config.noise_scale, 0.0f);
    EXPECT_GT(config.gradient_clip_norm, 0.0f);
    EXPECT_GT(config.privacy_budget_epsilon, 0.0f);
}

TEST_F(ConfigTest, DeviceProfileDefaultValid) {
    nimcp_device_profile_t profile = nimcp_device_profile_default();
    EXPECT_GT(profile.ram_mb, 0u);
    EXPECT_GT(profile.cpu_cores, 0u);
}

TEST_F(ConfigTest, OptimalNeuronsHighRAM) {
    nimcp_device_profile_t device;
    memset(&device, 0, sizeof(device));
    device.ram_mb = 8192;
    device.cpu_cores = 8;
    device.cpu_gflops = 100.0f;
    device.target_inference_ms = 50.0f;

    uint32_t neurons_high = nimcp_compute_optimal_neurons(&device);

    device.ram_mb = 512;
    uint32_t neurons_low = nimcp_compute_optimal_neurons(&device);

    EXPECT_GT(neurons_high, neurons_low);
}

TEST_F(ConfigTest, OptimalNeuronsLowRAM) {
    nimcp_device_profile_t device;
    memset(&device, 0, sizeof(device));
    device.ram_mb = 64;
    device.cpu_cores = 1;
    device.cpu_gflops = 1.0f;
    device.target_inference_ms = 100.0f;

    uint32_t neurons = nimcp_compute_optimal_neurons(&device);
    EXPECT_GT(neurons, 0u);
    EXPECT_LT(neurons, 100000u);
}

TEST_F(ConfigTest, OptimalNeuronsRespectsInferenceLatency) {
    nimcp_device_profile_t fast, slow;
    memset(&fast, 0, sizeof(fast));
    memset(&slow, 0, sizeof(slow));

    fast.ram_mb = 4096;
    fast.cpu_cores = 8;
    fast.cpu_gflops = 100.0f;
    fast.target_inference_ms = 1.0f; // Very tight latency

    slow.ram_mb = 4096;
    slow.cpu_cores = 8;
    slow.cpu_gflops = 100.0f;
    slow.target_inference_ms = 1000.0f; // Relaxed latency

    uint32_t neurons_fast = nimcp_compute_optimal_neurons(&fast);
    uint32_t neurons_slow = nimcp_compute_optimal_neurons(&slow);

    EXPECT_LE(neurons_fast, neurons_slow);
}

TEST_F(ConfigTest, SubsystemMaskCamera) {
    nimcp_device_profile_t device;
    memset(&device, 0, sizeof(device));
    device.ram_mb = 1024;
    device.has_camera = true;

    uint32_t mask = nimcp_compute_subsystem_mask(&device);
    // Camera should enable visual cortex — mask should be non-zero
    EXPECT_GT(mask, 0u);
}

TEST_F(ConfigTest, SubsystemMaskMicrophone) {
    nimcp_device_profile_t device;
    memset(&device, 0, sizeof(device));
    device.ram_mb = 1024;
    device.has_microphone = true;

    uint32_t mask = nimcp_compute_subsystem_mask(&device);
    EXPECT_GT(mask, 0u);
}

TEST_F(ConfigTest, SubsystemMaskNoSensorsMinimal) {
    nimcp_device_profile_t device;
    memset(&device, 0, sizeof(device));
    device.ram_mb = 64;
    device.has_camera = false;
    device.has_microphone = false;
    device.has_imu = false;

    uint32_t mask_none = nimcp_compute_subsystem_mask(&device);

    device.has_camera = true;
    device.has_microphone = true;
    device.has_imu = true;
    uint32_t mask_all = nimcp_compute_subsystem_mask(&device);

    // No sensors should have fewer subsystems
    EXPECT_LE(mask_none, mask_all);
}

TEST_F(ConfigTest, SubsystemMaskCoordinatorRole) {
    nimcp_device_profile_t device;
    memset(&device, 0, sizeof(device));
    device.ram_mb = 2048;
    device.role = NIMCP_DEVICE_COORDINATOR;

    uint32_t mask = nimcp_compute_subsystem_mask(&device);
    // Coordinator should have executive control enabled
    EXPECT_GT(mask, 0u);
}

TEST_F(ConfigTest, DeviceProfileDefaultHasReasonableValues) {
    nimcp_device_profile_t profile = nimcp_device_profile_default();
    EXPECT_GT(profile.ram_mb, 0u);
    EXPECT_GT(profile.cpu_cores, 0u);
    EXPECT_GT(profile.target_inference_ms, 0.0f);
}
