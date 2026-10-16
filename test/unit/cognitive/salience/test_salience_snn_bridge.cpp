/**
 * @file test_salience_snn_bridge.cpp
 * @brief Unit tests for Salience-SNN Integration Bridge
 * @version 1.0.0
 * @date 2026-01-06
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

#include "cognitive/salience/nimcp_salience_snn_bridge.h"

class SalienceSnnBridgeTest : public ::testing::Test {
protected:
    salience_snn_bridge_t* bridge = nullptr;

    void SetUp() override {
        salience_snn_config_t config = salience_snn_config_default();
        bridge = salience_snn_create(&config);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            salience_snn_destroy(bridge);
            bridge = nullptr;
        }
    }
};

// ============================================================================
// Lifecycle Tests
// ============================================================================

TEST_F(SalienceSnnBridgeTest, CreateWithNullConfig) {
    salience_snn_bridge_t* b = salience_snn_create(nullptr);
    EXPECT_EQ(b, nullptr);
}

TEST_F(SalienceSnnBridgeTest, DefaultConfigValid) {
    salience_snn_config_t config = salience_snn_config_default();
    EXPECT_EQ(config.max_features, SALIENCE_SNN_MAX_FEATURES);
    EXPECT_EQ(config.neurons_per_dim, SALIENCE_SNN_NEURONS_PER_DIM);
    EXPECT_GT(config.dt_ms, 0.0f);
    EXPECT_GT(config.novelty_threshold, 0.0f);
    EXPECT_GT(config.surprise_threshold, 0.0f);
}

TEST_F(SalienceSnnBridgeTest, ResetSucceeds) {
    int result = salience_snn_reset(bridge);
    EXPECT_EQ(result, 0);
}

TEST_F(SalienceSnnBridgeTest, ResetNullBridge) {
    int result = salience_snn_reset(nullptr);
    EXPECT_EQ(result, -1);
}

// ============================================================================
// Encoding Tests
// ============================================================================

TEST_F(SalienceSnnBridgeTest, EncodeFeaturesBasic) {
    float features[] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f};
    int result = salience_snn_encode_features(bridge, features, 5);
    EXPECT_EQ(result, 0);
}

TEST_F(SalienceSnnBridgeTest, EncodeFeaturesNullFeatures) {
    int result = salience_snn_encode_features(bridge, nullptr, 5);
    EXPECT_EQ(result, -1);
}

TEST_F(SalienceSnnBridgeTest, EncodeFeaturesZeroCount) {
    float features[] = {0.1f};
    int result = salience_snn_encode_features(bridge, features, 0);
    EXPECT_EQ(result, 0);  // Zero count should be valid (no-op)
}

TEST_F(SalienceSnnBridgeTest, EncodeWithPrediction) {
    float features[] = {0.1f, 0.2f, 0.3f};
    float prediction[] = {0.15f, 0.25f, 0.35f};
    int result = salience_snn_encode_with_prediction(
        bridge, features, 3, prediction, 3);
    EXPECT_EQ(result, 0);
}

TEST_F(SalienceSnnBridgeTest, EncodeWithPredictionMismatch) {
    float features[] = {0.1f, 0.2f, 0.3f};
    float prediction[] = {0.5f, 0.5f, 0.5f};
    // Large mismatch should create surprise
    int result = salience_snn_encode_with_prediction(
        bridge, features, 3, prediction, 3);
    EXPECT_EQ(result, 0);
}

TEST_F(SalienceSnnBridgeTest, EncodeTemporal) {
    float features[] = {0.1f, 0.2f, 0.3f};
    int result = salience_snn_encode_temporal(bridge, features, 3, 1000000);
    EXPECT_EQ(result, 0);
}

// ============================================================================
// Simulation Tests
// ============================================================================

TEST_F(SalienceSnnBridgeTest, SimulateBasic) {
    float features[] = {0.5f, 0.6f, 0.7f};
    salience_snn_encode_features(bridge, features, 3);
    int result = salience_snn_simulate(bridge, 100.0f);
    EXPECT_EQ(result, 0);
}

TEST_F(SalienceSnnBridgeTest, SimulateNullBridge) {
    int result = salience_snn_simulate(nullptr, 100.0f);
    EXPECT_EQ(result, -1);
}

TEST_F(SalienceSnnBridgeTest, StepBasic) {
    float features[] = {0.5f, 0.6f, 0.7f};
    salience_snn_encode_features(bridge, features, 3);
    int result = salience_snn_step(bridge);
    EXPECT_EQ(result, 0);
}

TEST_F(SalienceSnnBridgeTest, ForwardBasic) {
    float inputs[] = {0.1f, 0.2f, 0.3f, 0.4f};
    int result = salience_snn_forward(bridge, inputs, 4);
    EXPECT_EQ(result, 0);
}

// ============================================================================
// Decoding Tests
// ============================================================================

TEST_F(SalienceSnnBridgeTest, DecodeSalience) {
    float features[] = {0.8f, 0.9f, 0.7f};
    salience_snn_encode_features(bridge, features, 3);
    salience_snn_simulate(bridge, 100.0f);

    salience_snn_output_t output;
    int result = salience_snn_decode_salience(bridge, &output);
    EXPECT_EQ(result, 0);
    EXPECT_GE(output.combined_salience, 0.0f);
    EXPECT_LE(output.combined_salience, 1.0f);
}

TEST_F(SalienceSnnBridgeTest, DecodeSalienceNullOutput) {
    int result = salience_snn_decode_salience(bridge, nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(SalienceSnnBridgeTest, GetCombinedSalience) {
    float features[] = {0.8f, 0.9f, 0.7f};
    salience_snn_encode_features(bridge, features, 3);
    salience_snn_simulate(bridge, 100.0f);

    float salience = salience_snn_get_combined_salience(bridge);
    EXPECT_GE(salience, 0.0f);
}

TEST_F(SalienceSnnBridgeTest, GetNovelty) {
    float novelty = salience_snn_get_novelty(bridge);
    EXPECT_GE(novelty, 0.0f);
}

TEST_F(SalienceSnnBridgeTest, GetSurprise) {
    float surprise = salience_snn_get_surprise(bridge);
    EXPECT_GE(surprise, 0.0f);
}

TEST_F(SalienceSnnBridgeTest, GetUrgency) {
    float urgency = salience_snn_get_urgency(bridge);
    EXPECT_GE(urgency, 0.0f);
}

TEST_F(SalienceSnnBridgeTest, GetDominantChannel) {
    salience_snn_channel_t channel = salience_snn_get_dominant_channel(bridge);
    EXPECT_GE(channel, SALIENCE_SNN_CHANNEL_NOVELTY);
    EXPECT_LT(channel, SALIENCE_SNN_CHANNEL_COUNT);
}

// ============================================================================
// History Tests
// ============================================================================

TEST_F(SalienceSnnBridgeTest, AddToHistory) {
    float features[] = {0.1f, 0.2f, 0.3f};
    int result = salience_snn_add_to_history(bridge, features, 3);
    EXPECT_EQ(result, 0);
}

TEST_F(SalienceSnnBridgeTest, ClearHistory) {
    float features[] = {0.1f, 0.2f, 0.3f};
    salience_snn_add_to_history(bridge, features, 3);

    int result = salience_snn_clear_history(bridge);
    EXPECT_EQ(result, 0);
}

TEST_F(SalienceSnnBridgeTest, ComputeNovelty) {
    // Add some history
    float old_features[] = {0.1f, 0.2f, 0.3f};
    salience_snn_add_to_history(bridge, old_features, 3);

    // Compute novelty for new features
    float new_features[] = {0.9f, 0.8f, 0.7f};
    float novelty = salience_snn_compute_novelty(bridge, new_features, 3);
    EXPECT_GE(novelty, 0.0f);
}

TEST_F(SalienceSnnBridgeTest, NoveltyDecreasesWithFamiliarity) {
    salience_snn_config_t config = salience_snn_config_default();
    config.enable_history = true;
    salience_snn_bridge_t* b = salience_snn_create(&config);
    ASSERT_NE(b, nullptr);

    float features[] = {0.5f, 0.5f, 0.5f};

    // First exposure should have high novelty
    float novelty1 = salience_snn_compute_novelty(b, features, 3);
    salience_snn_add_to_history(b, features, 3);

    // Second exposure should have lower novelty
    float novelty2 = salience_snn_compute_novelty(b, features, 3);

    EXPECT_LE(novelty2, novelty1);

    salience_snn_destroy(b);
}

// ============================================================================
// State Query Tests
// ============================================================================

TEST_F(SalienceSnnBridgeTest, GetChannelState) {
    float features[] = {0.8f, 0.9f, 0.7f};
    salience_snn_encode_features(bridge, features, 3);
    salience_snn_simulate(bridge, 100.0f);

    salience_channel_state_t state;
    int result = salience_snn_get_channel_state(
        bridge, SALIENCE_SNN_CHANNEL_NOVELTY, &state);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(state.channel, SALIENCE_SNN_CHANNEL_NOVELTY);
}

TEST_F(SalienceSnnBridgeTest, GetChannelStateInvalidChannel) {
    salience_channel_state_t state;
    int result = salience_snn_get_channel_state(
        bridge, (salience_snn_channel_t)100, &state);
    EXPECT_EQ(result, -1);
}

TEST_F(SalienceSnnBridgeTest, GetState) {
    salience_snn_bridge_state_t state;
    int result = salience_snn_get_state(bridge, &state);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(state.state, SALIENCE_SNN_STATE_IDLE);
}

TEST_F(SalienceSnnBridgeTest, GetStats) {
    salience_snn_stats_t stats;
    int result = salience_snn_get_stats(bridge, &stats);
    EXPECT_EQ(result, 0);
}

TEST_F(SalienceSnnBridgeTest, ResetStats) {
    float features[] = {0.8f, 0.9f, 0.7f};
    salience_snn_encode_features(bridge, features, 3);
    salience_snn_simulate(bridge, 100.0f);

    int result = salience_snn_reset_stats(bridge);
    EXPECT_EQ(result, 0);

    salience_snn_stats_t stats;
    salience_snn_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_evaluations, 0u);
}

// ============================================================================
// Callback Tests
// ============================================================================

static bool spike_callback_called = false;
static void test_spike_callback(salience_snn_bridge_t*, salience_snn_channel_t, float, void*) {
    spike_callback_called = true;
}

TEST_F(SalienceSnnBridgeTest, RegisterSpikeCallback) {
    spike_callback_called = false;
    int result = salience_snn_register_spike_callback(bridge, test_spike_callback, nullptr);
    EXPECT_EQ(result, 0);
}

static bool threshold_callback_called = false;
static void test_threshold_callback(salience_snn_bridge_t*, salience_snn_channel_t, float, void*) {
    threshold_callback_called = true;
}

TEST_F(SalienceSnnBridgeTest, RegisterThresholdCallback) {
    threshold_callback_called = false;
    int result = salience_snn_register_threshold_callback(bridge, test_threshold_callback, nullptr);
    EXPECT_EQ(result, 0);
}

// ============================================================================
// Weight and Threshold Configuration Tests
// ============================================================================

TEST_F(SalienceSnnBridgeTest, SetWeights) {
    int result = salience_snn_set_weights(bridge, 0.4f, 0.3f, 0.3f);
    EXPECT_EQ(result, 0);
}

TEST_F(SalienceSnnBridgeTest, SetThresholds) {
    int result = salience_snn_set_thresholds(bridge, 0.6f, 0.5f, 0.7f);
    EXPECT_EQ(result, 0);
}

// ============================================================================
// Bio-Async Tests
// ============================================================================

TEST_F(SalienceSnnBridgeTest, BioAsyncNotConnectedByDefault) {
    bool connected = salience_snn_is_bio_async_connected(bridge);
    EXPECT_FALSE(connected);
}

TEST_F(SalienceSnnBridgeTest, BioAsyncConnectWithEnable) {
    salience_snn_config_t config = salience_snn_config_default();
    config.enable_bio_async = true;
    salience_snn_bridge_t* b = salience_snn_create(&config);
    ASSERT_NE(b, nullptr);

    int result = salience_snn_bio_async_connect(b);
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(salience_snn_is_bio_async_connected(b));

    salience_snn_bio_async_disconnect(b);
    EXPECT_FALSE(salience_snn_is_bio_async_connected(b));

    salience_snn_destroy(b);
}

// ============================================================================
// Salience Detection Scenarios
// ============================================================================

TEST_F(SalienceSnnBridgeTest, HighNoveltyScenario) {
    salience_snn_config_t config = salience_snn_config_default();
    config.enable_history = true;
    salience_snn_bridge_t* b = salience_snn_create(&config);
    ASSERT_NE(b, nullptr);

    // Add familiar patterns to history
    float familiar[] = {0.1f, 0.1f, 0.1f};
    for (int i = 0; i < 5; i++) {
        salience_snn_add_to_history(b, familiar, 3);
    }

    // Present novel pattern
    float novel[] = {0.9f, 0.9f, 0.9f};
    salience_snn_encode_features(b, novel, 3);
    salience_snn_simulate(b, 200.0f);

    float novelty = salience_snn_get_novelty(b);
    EXPECT_GT(novelty, 0.0f);

    salience_snn_destroy(b);
}

TEST_F(SalienceSnnBridgeTest, HighSurpriseScenario) {
    // Encode features with prediction that doesn't match
    float features[] = {0.9f, 0.9f, 0.9f};
    float prediction[] = {0.1f, 0.1f, 0.1f};
    salience_snn_encode_with_prediction(bridge, features, 3, prediction, 3);
    salience_snn_simulate(bridge, 200.0f);

    float surprise = salience_snn_get_surprise(bridge);
    EXPECT_GT(surprise, 0.0f);
}

TEST_F(SalienceSnnBridgeTest, HighIntensityScenario) {
    // High intensity features
    float features[] = {0.95f, 0.98f, 0.99f};
    salience_snn_encode_features(bridge, features, 3);
    salience_snn_simulate(bridge, 200.0f);

    salience_snn_output_t output;
    salience_snn_decode_salience(bridge, &output);

    EXPECT_GT(output.intensity, 0.3f);
}

TEST_F(SalienceSnnBridgeTest, MultiChannelSalience) {
    salience_snn_config_t config = salience_snn_config_default();
    config.enable_multimodal = true;
    salience_snn_bridge_t* b = salience_snn_create(&config);
    ASSERT_NE(b, nullptr);

    // Encode multiple feature sets
    float features[] = {0.7f, 0.8f, 0.9f, 0.6f};
    salience_snn_encode_features(b, features, 4);
    salience_snn_simulate(b, 200.0f);

    salience_snn_output_t output;
    salience_snn_decode_salience(b, &output);

    EXPECT_GE(output.combined_salience, 0.0f);
    EXPECT_GE(output.confidence, 0.0f);

    salience_snn_destroy(b);
}
