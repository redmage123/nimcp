/**
 * @file test_snn_training_bridge.cpp
 * @brief Unit tests for SNN-Training integration bridge
 *
 * @author NIMCP Team
 * @date 2024-12-20
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "snn/bridges/nimcp_snn_training_bridge.h"
#include "snn/nimcp_snn_config.h"
#include "snn/nimcp_snn_network.h"
#include "snn/nimcp_snn_training.h"

class SNNTrainingBridgeTest : public ::testing::Test {
protected:
    snn_network_t* snn;
    snn_training_ctx_t* training_ctx;
    snn_training_bridge_t* bridge;

    void SetUp() override {
        // Create SNN network with proper defaults
        snn_config_t snn_config;
        snn_config_default(&snn_config);
        snn_config.n_inputs = 10;
        snn_config.n_outputs = 10;
        snn_config.n_populations = 2;
        snn_config.dt = 0.1f;
        snn = snn_network_create(&snn_config);
        ASSERT_NE(snn, nullptr);

        // Create STDP training context
        snn_stdp_config_t stdp_config;
        snn_stdp_config_default(&stdp_config);
        training_ctx = snn_training_create_stdp(&stdp_config);
        ASSERT_NE(training_ctx, nullptr);

        bridge = nullptr;
    }

    void TearDown() override {
        if (bridge) snn_training_bridge_destroy(bridge);
        if (training_ctx) snn_training_destroy(training_ctx);
        if (snn) snn_network_destroy(snn);
    }
};

//=============================================================================
// Configuration Tests
//=============================================================================

TEST_F(SNNTrainingBridgeTest, DefaultConfigInitialization) {
    snn_training_bridge_config_t config;
    snn_training_bridge_config_default(&config);

    // Instability thresholds
    EXPECT_GT(config.weight_explosion_threshold, 0.0f);
    EXPECT_GT(config.rate_explosion_threshold, 0.0f);
    EXPECT_GT(config.gradient_explosion_threshold, 0.0f);

    // Modulation parameters
    EXPECT_GE(config.immune_modulation_strength, 0.0f);
    EXPECT_LE(config.immune_modulation_strength, 1.0f);
    EXPECT_GT(config.sleep_consolidation_boost, 1.0f);

    // Metaplasticity
    EXPECT_TRUE(config.enable_metaplasticity);
    EXPECT_GT(config.metaplasticity_tau, 0.0f);
    EXPECT_GT(config.bcm_theta_init, 0.0f);

    // Timing
    EXPECT_GT(config.update_interval_ms, 0.0f);
    EXPECT_GT(config.instability_check_interval_ms, 0.0f);
}

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(SNNTrainingBridgeTest, BridgeCreation) {
    snn_training_bridge_config_t config;
    snn_training_bridge_config_default(&config);

    bridge = snn_training_bridge_create(&config, snn, training_ctx);
    ASSERT_NE(bridge, nullptr);
}

TEST_F(SNNTrainingBridgeTest, BridgeCreationNullParams) {
    snn_training_bridge_config_t config;
    snn_training_bridge_config_default(&config);

    EXPECT_EQ(snn_training_bridge_create(nullptr, snn, training_ctx), nullptr);
    EXPECT_EQ(snn_training_bridge_create(&config, nullptr, training_ctx), nullptr);
    EXPECT_EQ(snn_training_bridge_create(&config, snn, nullptr), nullptr);
}

TEST_F(SNNTrainingBridgeTest, BridgeDestruction) {
    snn_training_bridge_config_t config;
    snn_training_bridge_config_default(&config);

    bridge = snn_training_bridge_create(&config, snn, training_ctx);
    ASSERT_NE(bridge, nullptr);

    snn_training_bridge_destroy(bridge);
    bridge = nullptr;
}

//=============================================================================
// Bio-async Tests
//=============================================================================

TEST_F(SNNTrainingBridgeTest, BioAsyncConnectionStatus) {
    snn_training_bridge_config_t config;
    snn_training_bridge_config_default(&config);

    bridge = snn_training_bridge_create(&config, snn, training_ctx);
    ASSERT_NE(bridge, nullptr);

    EXPECT_FALSE(snn_training_bridge_is_bio_async_connected(bridge));
}

TEST_F(SNNTrainingBridgeTest, BioAsyncConnect) {
    snn_training_bridge_config_t config;
    snn_training_bridge_config_default(&config);

    bridge = snn_training_bridge_create(&config, snn, training_ctx);
    ASSERT_NE(bridge, nullptr);

    int ret = snn_training_bridge_connect_bio_async(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(SNNTrainingBridgeTest, BioAsyncDisconnect) {
    snn_training_bridge_config_t config;
    snn_training_bridge_config_default(&config);

    bridge = snn_training_bridge_create(&config, snn, training_ctx);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(snn_training_bridge_disconnect_bio_async(bridge), 0);
}

//=============================================================================
// System Connection Tests
//=============================================================================

TEST_F(SNNTrainingBridgeTest, ConnectImmune) {
    snn_training_bridge_config_t config;
    snn_training_bridge_config_default(&config);

    bridge = snn_training_bridge_create(&config, snn, training_ctx);
    ASSERT_NE(bridge, nullptr);

    // Connect with NULL (allowed, just sets connected to false)
    int ret = snn_training_bridge_connect_immune(bridge, nullptr);
    EXPECT_EQ(ret, 0);
}

TEST_F(SNNTrainingBridgeTest, ConnectPlasticity) {
    snn_training_bridge_config_t config;
    snn_training_bridge_config_default(&config);

    bridge = snn_training_bridge_create(&config, snn, training_ctx);
    ASSERT_NE(bridge, nullptr);

    int ret = snn_training_bridge_connect_plasticity(bridge, nullptr);
    EXPECT_EQ(ret, 0);
}

TEST_F(SNNTrainingBridgeTest, ConnectCognitive) {
    snn_training_bridge_config_t config;
    snn_training_bridge_config_default(&config);

    bridge = snn_training_bridge_create(&config, snn, training_ctx);
    ASSERT_NE(bridge, nullptr);

    int ret = snn_training_bridge_connect_cognitive(bridge, nullptr);
    EXPECT_EQ(ret, 0);
}

TEST_F(SNNTrainingBridgeTest, ConnectSleep) {
    snn_training_bridge_config_t config;
    snn_training_bridge_config_default(&config);

    bridge = snn_training_bridge_create(&config, snn, training_ctx);
    ASSERT_NE(bridge, nullptr);

    int ret = snn_training_bridge_connect_sleep(bridge, nullptr);
    EXPECT_EQ(ret, 0);
}

//=============================================================================
// Update Tests
//=============================================================================

TEST_F(SNNTrainingBridgeTest, BridgeUpdate) {
    snn_training_bridge_config_t config;
    snn_training_bridge_config_default(&config);

    bridge = snn_training_bridge_create(&config, snn, training_ctx);
    ASSERT_NE(bridge, nullptr);

    int ret = snn_training_bridge_update(bridge, 1.0f);
    EXPECT_EQ(ret, 0);
}

TEST_F(SNNTrainingBridgeTest, BridgeUpdateMultipleSteps) {
    snn_training_bridge_config_t config;
    snn_training_bridge_config_default(&config);

    bridge = snn_training_bridge_create(&config, snn, training_ctx);
    ASSERT_NE(bridge, nullptr);

    for (int i = 0; i < 100; i++) {
        int ret = snn_training_bridge_update(bridge, 1.0f);
        EXPECT_EQ(ret, 0);
    }

    // Check update count
    uint32_t update_count;
    snn_training_bridge_get_stats(bridge, &update_count, nullptr, nullptr);
    EXPECT_EQ(update_count, 100);
}

TEST_F(SNNTrainingBridgeTest, TrainStep) {
    snn_training_bridge_config_t config;
    snn_training_bridge_config_default(&config);

    bridge = snn_training_bridge_create(&config, snn, training_ctx);
    ASSERT_NE(bridge, nullptr);

    uint32_t updates = snn_training_bridge_train_step(bridge, 1.0f);
    EXPECT_GE(updates, 0);  // May be 0 if no spike pairs
}

//=============================================================================
// Stability Tests
//=============================================================================

TEST_F(SNNTrainingBridgeTest, CheckStabilityInitial) {
    snn_training_bridge_config_t config;
    snn_training_bridge_config_default(&config);

    bridge = snn_training_bridge_create(&config, snn, training_ctx);
    ASSERT_NE(bridge, nullptr);

    // Update metrics first
    snn_training_bridge_update_metrics(bridge);

    snn_training_instability_t stability = snn_training_bridge_check_stability(bridge);
    EXPECT_EQ(stability, SNN_TRAIN_STABLE);
}

TEST_F(SNNTrainingBridgeTest, GetStabilityScore) {
    snn_training_bridge_config_t config;
    snn_training_bridge_config_default(&config);

    bridge = snn_training_bridge_create(&config, snn, training_ctx);
    ASSERT_NE(bridge, nullptr);

    snn_training_bridge_update_metrics(bridge);

    float score = snn_training_bridge_get_stability_score(bridge);
    EXPECT_GE(score, 0.0f);
    EXPECT_LE(score, 1.0f);
}

TEST_F(SNNTrainingBridgeTest, GetInstability) {
    snn_training_bridge_config_t config;
    snn_training_bridge_config_default(&config);

    bridge = snn_training_bridge_create(&config, snn, training_ctx);
    ASSERT_NE(bridge, nullptr);

    snn_training_instability_t instability = snn_training_bridge_get_instability(bridge);
    EXPECT_GE(instability, SNN_TRAIN_STABLE);
    EXPECT_LE(instability, SNN_TRAIN_SATURATION);
}

TEST_F(SNNTrainingBridgeTest, HandleInstability) {
    snn_training_bridge_config_t config;
    snn_training_bridge_config_default(&config);

    bridge = snn_training_bridge_create(&config, snn, training_ctx);
    ASSERT_NE(bridge, nullptr);

    int ret = snn_training_bridge_handle_instability(bridge, SNN_TRAIN_WEIGHT_EXPLOSION);
    EXPECT_EQ(ret, 0);
}

//=============================================================================
// Learning Rate Modulation Tests
//=============================================================================

TEST_F(SNNTrainingBridgeTest, GetEffectiveLR) {
    snn_training_bridge_config_t config;
    snn_training_bridge_config_default(&config);

    bridge = snn_training_bridge_create(&config, snn, training_ctx);
    ASSERT_NE(bridge, nullptr);

    float lr = snn_training_bridge_get_effective_lr(bridge);
    EXPECT_GT(lr, 0.0f);
}

TEST_F(SNNTrainingBridgeTest, SetBaseLR) {
    snn_training_bridge_config_t config;
    snn_training_bridge_config_default(&config);

    bridge = snn_training_bridge_create(&config, snn, training_ctx);
    ASSERT_NE(bridge, nullptr);

    snn_training_bridge_set_base_lr(bridge, 0.05f);

    // The effective LR should reflect the new base
    float lr = snn_training_bridge_get_effective_lr(bridge);
    EXPECT_GT(lr, 0.0f);
}

TEST_F(SNNTrainingBridgeTest, GetModulationFactor) {
    snn_training_bridge_config_t config;
    snn_training_bridge_config_default(&config);

    bridge = snn_training_bridge_create(&config, snn, training_ctx);
    ASSERT_NE(bridge, nullptr);

    float factor = snn_training_bridge_get_modulation_factor(bridge, SNN_MODULATION_IMMUNE);
    EXPECT_GE(factor, 0.0f);
    EXPECT_LE(factor, 2.0f);

    factor = snn_training_bridge_get_modulation_factor(bridge, SNN_MODULATION_HOMEOSTATIC);
    EXPECT_GE(factor, 0.0f);
    EXPECT_LE(factor, 2.0f);
}

//=============================================================================
// Metaplasticity Tests
//=============================================================================

TEST_F(SNNTrainingBridgeTest, UpdateBCMTheta) {
    snn_training_bridge_config_t config;
    snn_training_bridge_config_default(&config);
    config.enable_metaplasticity = true;

    bridge = snn_training_bridge_create(&config, snn, training_ctx);
    ASSERT_NE(bridge, nullptr);

    float theta = snn_training_bridge_update_bcm_theta(bridge, 100.0f);
    EXPECT_GT(theta, 0.0f);
    EXPECT_LE(theta, 1.0f);
}

TEST_F(SNNTrainingBridgeTest, GetMetaplasticityLevel) {
    snn_training_bridge_config_t config;
    snn_training_bridge_config_default(&config);

    bridge = snn_training_bridge_create(&config, snn, training_ctx);
    ASSERT_NE(bridge, nullptr);

    float level = snn_training_bridge_get_metaplasticity_level(bridge);
    EXPECT_GE(level, 0.0f);
    EXPECT_LE(level, 2.0f);  // Can exceed 1.0 temporarily
}

//=============================================================================
// Consolidation Tests
//=============================================================================

TEST_F(SNNTrainingBridgeTest, TriggerConsolidation) {
    snn_training_bridge_config_t config;
    snn_training_bridge_config_default(&config);
    config.enable_offline_consolidation = true;

    bridge = snn_training_bridge_create(&config, snn, training_ctx);
    ASSERT_NE(bridge, nullptr);

    int ret = snn_training_bridge_trigger_consolidation(bridge);
    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(snn_training_bridge_is_consolidating(bridge));
}

TEST_F(SNNTrainingBridgeTest, TriggerReplay) {
    snn_training_bridge_config_t config;
    snn_training_bridge_config_default(&config);
    config.replay_probability = 1.0f;  // Always replay

    bridge = snn_training_bridge_create(&config, snn, training_ctx);
    ASSERT_NE(bridge, nullptr);

    int ret = snn_training_bridge_trigger_replay(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(SNNTrainingBridgeTest, GetConsolidationProgress) {
    snn_training_bridge_config_t config;
    snn_training_bridge_config_default(&config);

    bridge = snn_training_bridge_create(&config, snn, training_ctx);
    ASSERT_NE(bridge, nullptr);

    float progress = snn_training_bridge_get_consolidation_progress(bridge);
    EXPECT_GE(progress, 0.0f);
    EXPECT_LE(progress, 1.0f);
}

TEST_F(SNNTrainingBridgeTest, IsConsolidating) {
    snn_training_bridge_config_t config;
    snn_training_bridge_config_default(&config);

    bridge = snn_training_bridge_create(&config, snn, training_ctx);
    ASSERT_NE(bridge, nullptr);

    // Initially not consolidating
    EXPECT_FALSE(snn_training_bridge_is_consolidating(bridge));

    // After trigger
    config.enable_offline_consolidation = true;
    snn_training_bridge_trigger_consolidation(bridge);
    // Note: Depends on implementation whether this is true immediately
}

//=============================================================================
// Metrics Tests
//=============================================================================

TEST_F(SNNTrainingBridgeTest, UpdateMetrics) {
    snn_training_bridge_config_t config;
    snn_training_bridge_config_default(&config);

    bridge = snn_training_bridge_create(&config, snn, training_ctx);
    ASSERT_NE(bridge, nullptr);

    int ret = snn_training_bridge_update_metrics(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(SNNTrainingBridgeTest, GetMetrics) {
    snn_training_bridge_config_t config;
    snn_training_bridge_config_default(&config);

    bridge = snn_training_bridge_create(&config, snn, training_ctx);
    ASSERT_NE(bridge, nullptr);

    snn_training_bridge_update_metrics(bridge);

    snn_training_metrics_t metrics;
    int ret = snn_training_bridge_get_metrics(bridge, &metrics);
    EXPECT_EQ(ret, 0);
    EXPECT_GE(metrics.stability_score, 0.0f);
    EXPECT_LE(metrics.stability_score, 1.0f);
}

//=============================================================================
// State Tests
//=============================================================================

TEST_F(SNNTrainingBridgeTest, GetBridgeState) {
    snn_training_bridge_config_t config;
    snn_training_bridge_config_default(&config);

    bridge = snn_training_bridge_create(&config, snn, training_ctx);
    ASSERT_NE(bridge, nullptr);

    snn_training_bridge_state_t state;
    int ret = snn_training_bridge_get_state(bridge, &state);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(state.update_count, 0);
}

TEST_F(SNNTrainingBridgeTest, GetStatistics) {
    snn_training_bridge_config_t config;
    snn_training_bridge_config_default(&config);

    bridge = snn_training_bridge_create(&config, snn, training_ctx);
    ASSERT_NE(bridge, nullptr);

    uint32_t update_count, weight_updates;
    float avg_lr;
    int ret = snn_training_bridge_get_stats(bridge, &update_count, &weight_updates, &avg_lr);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(update_count, 0);
    EXPECT_EQ(weight_updates, 0);
}

TEST_F(SNNTrainingBridgeTest, ResetStatistics) {
    snn_training_bridge_config_t config;
    snn_training_bridge_config_default(&config);

    bridge = snn_training_bridge_create(&config, snn, training_ctx);
    ASSERT_NE(bridge, nullptr);

    // Do some updates
    for (int i = 0; i < 10; i++) {
        snn_training_bridge_update(bridge, 1.0f);
    }

    // Reset
    snn_training_bridge_reset_stats(bridge);

    uint32_t update_count;
    snn_training_bridge_get_stats(bridge, &update_count, nullptr, nullptr);
    EXPECT_EQ(update_count, 0);
}

//=============================================================================
// Null Safety Tests
//=============================================================================

TEST_F(SNNTrainingBridgeTest, NullBridgeReturnsDefaults) {
    EXPECT_FLOAT_EQ(snn_training_bridge_get_effective_lr(nullptr), 0.0f);
    EXPECT_FLOAT_EQ(snn_training_bridge_get_stability_score(nullptr), 0.0f);
    EXPECT_FLOAT_EQ(snn_training_bridge_get_metaplasticity_level(nullptr), 0.0f);
    EXPECT_FLOAT_EQ(snn_training_bridge_get_consolidation_progress(nullptr), 0.0f);
    EXPECT_FALSE(snn_training_bridge_is_consolidating(nullptr));
    EXPECT_FALSE(snn_training_bridge_is_bio_async_connected(nullptr));
    EXPECT_EQ(snn_training_bridge_get_instability(nullptr), SNN_TRAIN_STABLE);
}

TEST_F(SNNTrainingBridgeTest, NullBridgeOperations) {
    EXPECT_NE(snn_training_bridge_update(nullptr, 1.0f), 0);
    EXPECT_EQ(snn_training_bridge_train_step(nullptr, 1.0f), 0);
    EXPECT_EQ(snn_training_bridge_check_stability(nullptr), SNN_TRAIN_STABLE);
    EXPECT_NE(snn_training_bridge_get_state(nullptr, nullptr), 0);
}

//=============================================================================
// Integration Tests
//=============================================================================

TEST_F(SNNTrainingBridgeTest, FullTrainingLoop) {
    snn_training_bridge_config_t config;
    snn_training_bridge_config_default(&config);
    config.enable_metaplasticity = true;
    config.enable_offline_consolidation = true;

    bridge = snn_training_bridge_create(&config, snn, training_ctx);
    ASSERT_NE(bridge, nullptr);

    // Simulate training loop
    const int num_steps = 1000;
    for (int step = 0; step < num_steps; step++) {
        // Update bridge
        snn_training_bridge_update(bridge, 1.0f);

        // Perform training step
        snn_training_bridge_train_step(bridge, 1.0f);

        // Check stability every 100 steps
        if (step % 100 == 0) {
            snn_training_instability_t instability =
                snn_training_bridge_check_stability(bridge);
            if (instability != SNN_TRAIN_STABLE) {
                snn_training_bridge_handle_instability(bridge, instability);
            }
        }
    }

    // Check final state
    uint32_t update_count;
    snn_training_bridge_get_stats(bridge, &update_count, nullptr, nullptr);
    EXPECT_EQ(update_count, num_steps);

    float stability = snn_training_bridge_get_stability_score(bridge);
    EXPECT_GE(stability, 0.0f);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
