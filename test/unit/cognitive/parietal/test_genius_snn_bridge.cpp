/**
 * @file test_genius_snn_bridge.cpp
 * @brief Unit tests for Mathematical Genius-SNN Bridge
 * @date 2026-01-24
 */

#include <gtest/gtest.h>

extern "C" {
#include "cognitive/parietal/nimcp_genius_snn_bridge.h"
}

class GeniusSNNBridgeTest : public ::testing::Test {
protected:
    genius_snn_config_t config;
    genius_snn_bridge_t* bridge = nullptr;

    void SetUp() override {
        config = genius_snn_config_default();
    }

    void TearDown() override {
        if (bridge) {
            genius_snn_destroy(bridge);
            bridge = nullptr;
        }
    }
};

/* ============================================================================
 * Configuration Tests
 * ============================================================================ */

TEST_F(GeniusSNNBridgeTest, DefaultConfigSetsReasonableDefaults) {
    genius_snn_config_t cfg = genius_snn_config_default();

    EXPECT_EQ(cfg.num_dimensions, (uint32_t)GENIUS_DIM_COUNT);
    EXPECT_EQ(cfg.neurons_per_dim, GENIUS_SNN_NEURONS_PER_CONCEPT);
    EXPECT_GT(cfg.hidden_dim, 0u);
    EXPECT_GT(cfg.dt_ms, 0.0f);
    EXPECT_GT(cfg.encoding_window_ms, 0.0f);
    EXPECT_EQ(cfg.encoding, GENIUS_SNN_ENCODE_POPULATION);
    EXPECT_EQ(cfg.decoding, GENIUS_SNN_DECODE_INTEGRATION);
    EXPECT_TRUE(cfg.enable_competition);
    EXPECT_TRUE(cfg.enable_insight_detection);
    EXPECT_TRUE(cfg.enable_gauss_circuits);
    EXPECT_TRUE(cfg.enable_newton_circuits);
    EXPECT_TRUE(cfg.enable_erdos_circuits);
}

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(GeniusSNNBridgeTest, CreateWithNullConfigUsesDefaults) {
    bridge = genius_snn_create(nullptr);
    ASSERT_NE(bridge, nullptr);
}

TEST_F(GeniusSNNBridgeTest, CreateWithValidConfigSucceeds) {
    bridge = genius_snn_create(&config);
    ASSERT_NE(bridge, nullptr);
}

TEST_F(GeniusSNNBridgeTest, CreateWithZeroDimensionsFails) {
    config.num_dimensions = 0;
    bridge = genius_snn_create(&config);
    EXPECT_EQ(bridge, nullptr);
}

TEST_F(GeniusSNNBridgeTest, CreateWithTooManyDimensionsFails) {
    config.num_dimensions = GENIUS_SNN_MAX_DIMENSIONS + 1;
    bridge = genius_snn_create(&config);
    EXPECT_EQ(bridge, nullptr);
}

TEST_F(GeniusSNNBridgeTest, DestroyNullIsSafe) {
    genius_snn_destroy(nullptr);
    // Should not crash
}

TEST_F(GeniusSNNBridgeTest, ResetSucceeds) {
    bridge = genius_snn_create(&config);
    ASSERT_NE(bridge, nullptr);

    int result = genius_snn_reset(bridge);
    EXPECT_EQ(result, 0);
}

TEST_F(GeniusSNNBridgeTest, ResetNullFails) {
    int result = genius_snn_reset(nullptr);
    EXPECT_EQ(result, -1);
}

/* ============================================================================
 * Encoding Tests
 * ============================================================================ */

TEST_F(GeniusSNNBridgeTest, EncodeStateSucceeds) {
    bridge = genius_snn_create(&config);
    ASSERT_NE(bridge, nullptr);

    float dimensions[GENIUS_DIM_COUNT] = {0.5f, 0.7f, 0.3f, 0.8f};
    int spikes = genius_snn_encode_state(bridge, dimensions, 4);
    EXPECT_GE(spikes, 0);
}

TEST_F(GeniusSNNBridgeTest, EncodeStateNullBridgeFails) {
    float dimensions[4] = {0.5f, 0.7f, 0.3f, 0.8f};
    int spikes = genius_snn_encode_state(nullptr, dimensions, 4);
    EXPECT_EQ(spikes, -1);
}

TEST_F(GeniusSNNBridgeTest, EncodeStateNullDimensionsFails) {
    bridge = genius_snn_create(&config);
    ASSERT_NE(bridge, nullptr);

    int spikes = genius_snn_encode_state(bridge, nullptr, 4);
    EXPECT_EQ(spikes, -1);
}

TEST_F(GeniusSNNBridgeTest, EncodePatternSucceeds) {
    bridge = genius_snn_create(&config);
    ASSERT_NE(bridge, nullptr);

    int result = genius_snn_encode_pattern(bridge, 0.8f, 1);
    EXPECT_EQ(result, 0);
}

TEST_F(GeniusSNNBridgeTest, EncodeProofStateSucceeds) {
    bridge = genius_snn_create(&config);
    ASSERT_NE(bridge, nullptr);

    int result = genius_snn_encode_proof_state(bridge, 0.5f, 0.7f, 10);
    EXPECT_EQ(result, 0);
}

TEST_F(GeniusSNNBridgeTest, EncodeModeSucceeds) {
    bridge = genius_snn_create(&config);
    ASSERT_NE(bridge, nullptr);

    int result = genius_snn_encode_mode(bridge, GENIUS_MODE_GAUSS, 0.9f);
    EXPECT_EQ(result, 0);

    result = genius_snn_encode_mode(bridge, GENIUS_MODE_NEWTON, 0.6f);
    EXPECT_EQ(result, 0);

    result = genius_snn_encode_mode(bridge, GENIUS_MODE_ERDOS, 0.4f);
    EXPECT_EQ(result, 0);
}

/* ============================================================================
 * Simulation Tests
 * ============================================================================ */

TEST_F(GeniusSNNBridgeTest, SimulateSucceeds) {
    bridge = genius_snn_create(&config);
    ASSERT_NE(bridge, nullptr);

    int result = genius_snn_simulate(bridge, 100.0f);
    EXPECT_EQ(result, 0);
}

TEST_F(GeniusSNNBridgeTest, SimulateNullFails) {
    int result = genius_snn_simulate(nullptr, 100.0f);
    EXPECT_EQ(result, -1);
}

TEST_F(GeniusSNNBridgeTest, SimulateNegativeDurationFails) {
    bridge = genius_snn_create(&config);
    ASSERT_NE(bridge, nullptr);

    int result = genius_snn_simulate(bridge, -10.0f);
    EXPECT_EQ(result, -1);
}

TEST_F(GeniusSNNBridgeTest, StepSucceeds) {
    bridge = genius_snn_create(&config);
    ASSERT_NE(bridge, nullptr);

    int result = genius_snn_step(bridge);
    EXPECT_EQ(result, 0);
}

/* ============================================================================
 * Decoding Tests
 * ============================================================================ */

TEST_F(GeniusSNNBridgeTest, GetInsightOutputSucceeds) {
    bridge = genius_snn_create(&config);
    ASSERT_NE(bridge, nullptr);

    // Encode some activity first
    genius_snn_encode_mode(bridge, GENIUS_MODE_GAUSS, 0.9f);
    genius_snn_simulate(bridge, 50.0f);

    genius_insight_output_t insight;
    int result = genius_snn_get_insight_output(bridge, &insight);
    EXPECT_EQ(result, 0);
    EXPECT_GE(insight.gauss_activity, 0.0f);
    EXPECT_LE(insight.gauss_activity, 1.0f);
}

TEST_F(GeniusSNNBridgeTest, GetInsightOutputNullFails) {
    int result = genius_snn_get_insight_output(nullptr, nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(GeniusSNNBridgeTest, GetActivationsSucceeds) {
    bridge = genius_snn_create(&config);
    ASSERT_NE(bridge, nullptr);

    float activations[GENIUS_DIM_COUNT];
    int result = genius_snn_get_activations(bridge, activations, GENIUS_DIM_COUNT);
    EXPECT_EQ(result, 0);
}

TEST_F(GeniusSNNBridgeTest, CheckInsightReturnsBoolean) {
    bridge = genius_snn_create(&config);
    ASSERT_NE(bridge, nullptr);

    float level;
    bool detected = genius_snn_check_insight(bridge, &level);
    // Initial state should not have insight
    EXPECT_FALSE(detected);
}

TEST_F(GeniusSNNBridgeTest, RecommendModeReturnsValidMode) {
    bridge = genius_snn_create(&config);
    ASSERT_NE(bridge, nullptr);

    // Activate Gauss mode
    genius_snn_encode_mode(bridge, GENIUS_MODE_GAUSS, 0.9f);

    float confidence;
    genius_mode_t mode = genius_snn_recommend_mode(bridge, &confidence);
    EXPECT_EQ(mode, GENIUS_MODE_GAUSS);
    EXPECT_GT(confidence, 0.0f);
}

/* ============================================================================
 * State Query Tests
 * ============================================================================ */

TEST_F(GeniusSNNBridgeTest, GetDimStateSucceeds) {
    bridge = genius_snn_create(&config);
    ASSERT_NE(bridge, nullptr);

    genius_dim_state_t state;
    int result = genius_snn_get_dim_state(bridge, 0, &state);
    EXPECT_EQ(result, 0);
}

TEST_F(GeniusSNNBridgeTest, GetDimStateInvalidDimFails) {
    bridge = genius_snn_create(&config);
    ASSERT_NE(bridge, nullptr);

    genius_dim_state_t state;
    int result = genius_snn_get_dim_state(bridge, GENIUS_SNN_MAX_DIMENSIONS, &state);
    EXPECT_EQ(result, -1);
}

TEST_F(GeniusSNNBridgeTest, GetStateSucceeds) {
    bridge = genius_snn_create(&config);
    ASSERT_NE(bridge, nullptr);

    genius_snn_bridge_state_t state;
    int result = genius_snn_get_state(bridge, &state);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(state.state, GENIUS_SNN_STATE_IDLE);
}

TEST_F(GeniusSNNBridgeTest, GetStatsSucceeds) {
    bridge = genius_snn_create(&config);
    ASSERT_NE(bridge, nullptr);

    genius_snn_stats_t stats;
    int result = genius_snn_get_stats(bridge, &stats);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(stats.total_evaluations, 0u);
}

TEST_F(GeniusSNNBridgeTest, ResetStatsSucceeds) {
    bridge = genius_snn_create(&config);
    ASSERT_NE(bridge, nullptr);

    // Generate some stats
    float dims[4] = {0.5f, 0.5f, 0.5f, 0.5f};
    genius_snn_encode_state(bridge, dims, 4);

    int result = genius_snn_reset_stats(bridge);
    EXPECT_EQ(result, 0);

    genius_snn_stats_t stats;
    genius_snn_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_evaluations, 0u);
}

/* ============================================================================
 * Callback Tests
 * ============================================================================ */

static bool insight_callback_called = false;
static void test_insight_callback(genius_snn_bridge_t*, float, genius_mode_t, void*) {
    insight_callback_called = true;
}

TEST_F(GeniusSNNBridgeTest, RegisterInsightCallbackSucceeds) {
    bridge = genius_snn_create(&config);
    ASSERT_NE(bridge, nullptr);

    insight_callback_called = false;
    int result = genius_snn_register_insight_callback(bridge, test_insight_callback, nullptr);
    EXPECT_EQ(result, 0);
}

TEST_F(GeniusSNNBridgeTest, RegisterModeCallbackSucceeds) {
    bridge = genius_snn_create(&config);
    ASSERT_NE(bridge, nullptr);

    int result = genius_snn_register_mode_callback(bridge, nullptr, nullptr);
    EXPECT_EQ(result, 0);
}

/* ============================================================================
 * Bio-Async Tests
 * ============================================================================ */

TEST_F(GeniusSNNBridgeTest, BioAsyncInitiallyDisconnected) {
    bridge = genius_snn_create(&config);
    ASSERT_NE(bridge, nullptr);

    EXPECT_FALSE(genius_snn_is_bio_async_connected(bridge));
}

TEST_F(GeniusSNNBridgeTest, BioAsyncConnectSucceeds) {
    bridge = genius_snn_create(&config);
    ASSERT_NE(bridge, nullptr);

    int result = genius_snn_bio_async_connect(bridge);
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(genius_snn_is_bio_async_connected(bridge));
}

TEST_F(GeniusSNNBridgeTest, BioAsyncDisconnectSucceeds) {
    bridge = genius_snn_create(&config);
    ASSERT_NE(bridge, nullptr);

    genius_snn_bio_async_connect(bridge);
    int result = genius_snn_bio_async_disconnect(bridge);
    EXPECT_EQ(result, 0);
    EXPECT_FALSE(genius_snn_is_bio_async_connected(bridge));
}

/* ============================================================================
 * Integration Tests
 * ============================================================================ */

TEST_F(GeniusSNNBridgeTest, FullWorkflowSucceeds) {
    bridge = genius_snn_create(&config);
    ASSERT_NE(bridge, nullptr);

    // 1. Encode mathematical state
    float dims[GENIUS_DIM_COUNT] = {0.8f, 0.6f, 0.7f, 0.5f, 0.9f, 0.4f};
    int spikes = genius_snn_encode_state(bridge, dims, 6);
    EXPECT_GE(spikes, 0);

    // 2. Encode mode activations
    genius_snn_encode_mode(bridge, GENIUS_MODE_GAUSS, 0.8f);
    genius_snn_encode_mode(bridge, GENIUS_MODE_NEWTON, 0.3f);

    // 3. Simulate processing
    int result = genius_snn_simulate(bridge, 100.0f);
    EXPECT_EQ(result, 0);

    // 4. Get insight output
    genius_insight_output_t insight;
    result = genius_snn_get_insight_output(bridge, &insight);
    EXPECT_EQ(result, 0);
    EXPECT_GT(insight.gauss_activity, insight.newton_activity);

    // 5. Check state
    genius_snn_bridge_state_t state;
    genius_snn_get_state(bridge, &state);
    EXPECT_GT(state.total_activity, 0.0f);

    // 6. Get stats
    genius_snn_stats_t stats;
    genius_snn_get_stats(bridge, &stats);
    EXPECT_GT(stats.total_evaluations, 0u);
    EXPECT_GT(stats.total_simulations, 0u);
}
