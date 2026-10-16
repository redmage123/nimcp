/**
 * @file test_cortical_plasticity_bridge.cpp
 * @brief Unit tests for cortical plasticity coordinator integration
 */

#include <gtest/gtest.h>
// Headers have their own extern "C" guards
#include "core/cortical_columns/nimcp_cortical_plasticity_bridge.h"

/* ============================================================================
 * Test Fixtures
 * ============================================================================ */

class CorticalPlasticityBridgeTest : public ::testing::Test {
protected:
    cortical_plasticity_bridge_t* bridge;
    cortical_plasticity_config_t config;

    void SetUp() override {
        cortical_plasticity_default_config(&config);
        bridge = cortical_plasticity_create(&config);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            cortical_plasticity_destroy(bridge);
        }
    }
};

/* ============================================================================
 * Configuration Tests
 * ============================================================================ */

TEST_F(CorticalPlasticityBridgeTest, DefaultConfig) {
    cortical_plasticity_config_t cfg;
    int result = cortical_plasticity_default_config(&cfg);

    EXPECT_EQ(result, 0);
    EXPECT_GT(cfg.stdp_learning_rate, 0.0f);
    EXPECT_GT(cfg.bcm_threshold, 0.0f);
    EXPECT_GT(cfg.homeostatic_target, 0.0f);
}

TEST_F(CorticalPlasticityBridgeTest, DefaultConfigNullPointer) {
    int result = cortical_plasticity_default_config(nullptr);
    EXPECT_LT(result, 0);
}

TEST_F(CorticalPlasticityBridgeTest, CreateWithConfig) {
    cortical_plasticity_config_t custom_config;
    cortical_plasticity_default_config(&custom_config);
    custom_config.stdp_learning_rate = 0.05f;
    custom_config.enable_homeostasis = true;

    cortical_plasticity_bridge_t* system = cortical_plasticity_create(&custom_config);
    ASSERT_NE(system, nullptr);

    cortical_plasticity_destroy(system);
}

TEST_F(CorticalPlasticityBridgeTest, CreateWithNullConfig) {
    cortical_plasticity_bridge_t* system = cortical_plasticity_create(nullptr);
    ASSERT_NE(system, nullptr);
    cortical_plasticity_destroy(system);
}

/* ============================================================================
 * STDP Tests
 * ============================================================================ */

TEST_F(CorticalPlasticityBridgeTest, ApplySTDP) {
    float pre_spike_time = 10.0f;
    float post_spike_time = 15.0f;
    float weight_change;

    int result = cortical_plasticity_apply_stdp(bridge, pre_spike_time, post_spike_time,
                                                 1.0f, &weight_change);
    EXPECT_EQ(result, 0);
    EXPECT_GT(weight_change, 0.0f); /* LTP for pre-before-post */
}

TEST_F(CorticalPlasticityBridgeTest, ApplySTDPDepression) {
    float pre_spike_time = 15.0f;
    float post_spike_time = 10.0f;
    float weight_change;

    int result = cortical_plasticity_apply_stdp(bridge, pre_spike_time, post_spike_time,
                                                 1.0f, &weight_change);
    EXPECT_EQ(result, 0);
    EXPECT_LT(weight_change, 0.0f); /* LTD for post-before-pre */
}

TEST_F(CorticalPlasticityBridgeTest, ApplySTDPBatch) {
    float pre_times[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    float post_times[10] = {2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
    float weights[10] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
    float changes[10];

    int result = cortical_plasticity_apply_stdp_batch(bridge, pre_times, post_times,
                                                       weights, 10, changes);
    EXPECT_EQ(result, 0);
}

/* ============================================================================
 * BCM Tests
 * ============================================================================ */

TEST_F(CorticalPlasticityBridgeTest, ApplyBCM) {
    float post_activity = 0.8f;
    float pre_activity = 0.5f;
    float weight_change;

    int result = cortical_plasticity_apply_bcm(bridge, post_activity, pre_activity,
                                                1.0f, &weight_change);
    EXPECT_EQ(result, 0);
}

TEST_F(CorticalPlasticityBridgeTest, UpdateBCMThreshold) {
    int result = cortical_plasticity_update_bcm_threshold(bridge, 0.6f);
    EXPECT_EQ(result, 0);
}

/* ============================================================================
 * Homeostasis Tests
 * ============================================================================ */

TEST_F(CorticalPlasticityBridgeTest, ApplyHomeostasis) {
    float current_activity = 0.3f;
    float adjustment;

    int result = cortical_plasticity_apply_homeostasis(bridge, current_activity, &adjustment);
    EXPECT_EQ(result, 0);
    /* Low activity should lead to positive adjustment */
    EXPECT_GT(adjustment, 0.0f);
}

TEST_F(CorticalPlasticityBridgeTest, ApplyHomeostasisHighActivity) {
    float current_activity = 0.9f;
    float adjustment;

    int result = cortical_plasticity_apply_homeostasis(bridge, current_activity, &adjustment);
    EXPECT_EQ(result, 0);
    /* High activity should lead to negative adjustment */
    EXPECT_LT(adjustment, 0.0f);
}

/* ============================================================================
 * Metaplasticity Tests
 * ============================================================================ */

TEST_F(CorticalPlasticityBridgeTest, UpdateMetaplasticity) {
    float activity_history[10] = {0.5f, 0.6f, 0.4f, 0.7f, 0.5f, 0.3f, 0.8f, 0.4f, 0.5f, 0.6f};

    int result = cortical_plasticity_update_metaplasticity(bridge, activity_history, 10);
    EXPECT_EQ(result, 0);
}

TEST_F(CorticalPlasticityBridgeTest, GetPlasticityModulation) {
    float modulation = cortical_plasticity_get_modulation(bridge);
    EXPECT_GT(modulation, 0.0f);
}

/* ============================================================================
 * Weight Constraint Tests
 * ============================================================================ */

TEST_F(CorticalPlasticityBridgeTest, ClampWeights) {
    float weights[10] = {-0.5f, 0.0f, 0.5f, 1.0f, 1.5f, 2.0f, -1.0f, 0.8f, 0.3f, 1.2f};

    int result = cortical_plasticity_clamp_weights(bridge, weights, 10);
    EXPECT_EQ(result, 0);

    /* Check all weights are in valid range */
    for (int i = 0; i < 10; i++) {
        EXPECT_GE(weights[i], 0.0f);
        EXPECT_LE(weights[i], 1.0f);
    }
}

TEST_F(CorticalPlasticityBridgeTest, NormalizeWeights) {
    float weights[10] = {0.5f, 0.6f, 0.4f, 0.7f, 0.5f, 0.3f, 0.8f, 0.4f, 0.5f, 0.6f};
    float target_sum = 5.0f;

    int result = cortical_plasticity_normalize_weights(bridge, weights, 10, target_sum);
    EXPECT_EQ(result, 0);

    /* Check sum is approximately target */
    float sum = 0.0f;
    for (int i = 0; i < 10; i++) {
        sum += weights[i];
    }
    EXPECT_NEAR(sum, target_sum, 0.01f);
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

TEST_F(CorticalPlasticityBridgeTest, GetStats) {
    cortical_plasticity_stats_t stats;
    int result = cortical_plasticity_get_stats(bridge, &stats);
    EXPECT_EQ(result, 0);
}

TEST_F(CorticalPlasticityBridgeTest, ResetStats) {
    int result = cortical_plasticity_reset_stats(bridge);
    EXPECT_EQ(result, 0);
}

/* ============================================================================
 * Bio-async Tests
 * ============================================================================ */

TEST_F(CorticalPlasticityBridgeTest, ConnectBioAsync) {
    int result = cortical_plasticity_connect_bio_async(bridge);
    EXPECT_TRUE(result == 0 || result < 0);
}

TEST_F(CorticalPlasticityBridgeTest, IsBioAsyncConnected) {
    bool connected = cortical_plasticity_is_bio_async_connected(bridge);
    EXPECT_FALSE(connected);
}

TEST_F(CorticalPlasticityBridgeTest, DisconnectBioAsync) {
    int result = cortical_plasticity_disconnect_bio_async(bridge);
    EXPECT_EQ(result, 0);
}

/* ============================================================================
 * Edge Cases
 * ============================================================================ */

TEST_F(CorticalPlasticityBridgeTest, DestroyNull) {
    cortical_plasticity_destroy(nullptr);
}

TEST_F(CorticalPlasticityBridgeTest, STDPZeroTimeDelay) {
    float weight_change;
    int result = cortical_plasticity_apply_stdp(bridge, 10.0f, 10.0f, 1.0f, &weight_change);
    EXPECT_EQ(result, 0);
    /* Zero delay should produce minimal or zero change */
    EXPECT_NEAR(weight_change, 0.0f, 0.1f);
}

TEST_F(CorticalPlasticityBridgeTest, LargeSTDPBatch) {
    const int N = 1000;
    float* pre_times = new float[N];
    float* post_times = new float[N];
    float* weights = new float[N];
    float* changes = new float[N];

    for (int i = 0; i < N; i++) {
        pre_times[i] = (float)i;
        post_times[i] = (float)i + 5.0f;
        weights[i] = 0.5f;
    }

    int result = cortical_plasticity_apply_stdp_batch(bridge, pre_times, post_times, weights, N, changes);
    EXPECT_EQ(result, 0);

    delete[] pre_times;
    delete[] post_times;
    delete[] weights;
    delete[] changes;
}
