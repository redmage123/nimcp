/**
 * @file test_meta_learning_snn_bridge.cpp
 * @brief Unit tests for Meta Learning SNN Bridge
 */

#include <gtest/gtest.h>
#include <cmath>

#include "cognitive/meta_learning/nimcp_meta_learning_snn_bridge.h"

class MetaLearningSNNBridgeTest : public ::testing::Test {
protected:
    meta_learning_snn_bridge_t* bridge = nullptr;

    void SetUp() override {
        meta_learning_snn_config_t config = meta_learning_snn_config_default();
        config.enable_bio_async = false;
        bridge = meta_learning_snn_create(&config);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            meta_learning_snn_destroy(bridge);
            bridge = nullptr;
        }
    }
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(MetaLearningSNNBridgeTest, CreateWithDefaults) {
    meta_learning_snn_bridge_t* test_bridge = meta_learning_snn_create(nullptr);
    ASSERT_NE(test_bridge, nullptr);
    meta_learning_snn_destroy(test_bridge);
}

TEST_F(MetaLearningSNNBridgeTest, CreateWithConfig) {
    meta_learning_snn_config_t config = meta_learning_snn_config_default();
    config.num_dimensions = 8;
    config.neurons_per_dim = 16;
    meta_learning_snn_bridge_t* test_bridge = meta_learning_snn_create(&config);
    ASSERT_NE(test_bridge, nullptr);
    meta_learning_snn_destroy(test_bridge);
}

TEST_F(MetaLearningSNNBridgeTest, CreateWithInvalidConfig) {
    meta_learning_snn_config_t config = meta_learning_snn_config_default();
    config.num_dimensions = 0;
    meta_learning_snn_bridge_t* test_bridge = meta_learning_snn_create(&config);
    EXPECT_EQ(test_bridge, nullptr);
}

TEST_F(MetaLearningSNNBridgeTest, Reset) {
    EXPECT_EQ(meta_learning_snn_reset(bridge), 0);
}

TEST_F(MetaLearningSNNBridgeTest, ResetNull) {
    EXPECT_EQ(meta_learning_snn_reset(nullptr), -1);
}

TEST_F(MetaLearningSNNBridgeTest, DestroyNull) {
    meta_learning_snn_destroy(nullptr); // Should not crash
}

//=============================================================================
// Default Config Tests
//=============================================================================

TEST_F(MetaLearningSNNBridgeTest, DefaultConfigValues) {
    meta_learning_snn_config_t config = meta_learning_snn_config_default();
    EXPECT_EQ(config.num_dimensions, META_DIM_COUNT);
    EXPECT_EQ(config.neurons_per_dim, META_LEARNING_SNN_NEURONS_PER_DIM);
    EXPECT_GT(config.dt_ms, 0.0f);
    EXPECT_TRUE(config.enable_curriculum);
}

//=============================================================================
// Encoding Tests
//=============================================================================

TEST_F(MetaLearningSNNBridgeTest, EncodeState) {
    float dims[META_DIM_COUNT] = {0};
    dims[META_DIM_LEARNING_RATE] = 0.8f;
    dims[META_DIM_ADAPTATION_SPEED] = 0.7f;

    int spike_count = meta_learning_snn_encode_state(bridge, dims, META_DIM_COUNT);
    EXPECT_GE(spike_count, 0);
}

TEST_F(MetaLearningSNNBridgeTest, EncodeStateNull) {
    EXPECT_EQ(meta_learning_snn_encode_state(nullptr, nullptr, 0), -1);
    EXPECT_EQ(meta_learning_snn_encode_state(bridge, nullptr, 0), -1);
}

TEST_F(MetaLearningSNNBridgeTest, EncodeLearningRate) {
    int spike_count = meta_learning_snn_encode_learning_rate(bridge, 0.3f, 0.5f);
    EXPECT_GE(spike_count, 0);
}

TEST_F(MetaLearningSNNBridgeTest, EncodeTaskSimilarity) {
    int spike_count = meta_learning_snn_encode_task_similarity(bridge, 0.9f, 5);
    EXPECT_GE(spike_count, 0);
}

TEST_F(MetaLearningSNNBridgeTest, EncodeTransfer) {
    int spike_count = meta_learning_snn_encode_transfer(bridge, 0.6f, 1);
    EXPECT_GE(spike_count, 0);
}

//=============================================================================
// Simulation Tests
//=============================================================================

TEST_F(MetaLearningSNNBridgeTest, SimulateBasic) {
    float dims[META_DIM_COUNT] = {0.5f};
    meta_learning_snn_encode_state(bridge, dims, 1);
    EXPECT_EQ(meta_learning_snn_simulate(bridge, 10.0f), 0);
}

TEST_F(MetaLearningSNNBridgeTest, SimulateNull) {
    EXPECT_EQ(meta_learning_snn_simulate(nullptr, 10.0f), -1);
}

TEST_F(MetaLearningSNNBridgeTest, SimulateZeroDuration) {
    EXPECT_EQ(meta_learning_snn_simulate(bridge, 0.0f), -1);
}

TEST_F(MetaLearningSNNBridgeTest, SimulateNegativeDuration) {
    EXPECT_EQ(meta_learning_snn_simulate(bridge, -10.0f), -1);
}

TEST_F(MetaLearningSNNBridgeTest, Step) {
    float dims[META_DIM_COUNT] = {0.5f};
    meta_learning_snn_encode_state(bridge, dims, 1);
    EXPECT_EQ(meta_learning_snn_step(bridge), 0);
}

TEST_F(MetaLearningSNNBridgeTest, Forward) {
    float inputs[5] = {0.5f, 0.6f, 0.7f, 0.4f, 0.3f};
    int spike_count = meta_learning_snn_forward(bridge, inputs, 5);
    EXPECT_GE(spike_count, 0);
}

//=============================================================================
// Insight Decoding Tests
//=============================================================================

TEST_F(MetaLearningSNNBridgeTest, GetInsight) {
    float dims[META_DIM_COUNT] = {0};
    dims[META_DIM_LEARNING_RATE] = 0.9f;
    dims[META_DIM_ADAPTATION_SPEED] = 0.85f;
    meta_learning_snn_encode_state(bridge, dims, META_DIM_COUNT);
    meta_learning_snn_simulate(bridge, 20.0f);

    meta_learning_insight_t insight;
    EXPECT_EQ(meta_learning_snn_get_insight(bridge, &insight), 0);
    EXPECT_GE(insight.learning_rate_level, 0.0f);
    EXPECT_LE(insight.learning_rate_level, 1.0f);
    EXPECT_GE(insight.adaptation_level, 0.0f);
    EXPECT_LE(insight.adaptation_level, 1.0f);
}

TEST_F(MetaLearningSNNBridgeTest, GetInsightNull) {
    EXPECT_EQ(meta_learning_snn_get_insight(nullptr, nullptr), -1);
    meta_learning_insight_t insight;
    EXPECT_EQ(meta_learning_snn_get_insight(nullptr, &insight), -1);
    EXPECT_EQ(meta_learning_snn_get_insight(bridge, nullptr), -1);
}

TEST_F(MetaLearningSNNBridgeTest, GetActivations) {
    float activations[META_DIM_COUNT];
    EXPECT_EQ(meta_learning_snn_get_activations(bridge, activations, META_DIM_COUNT), 0);
}

//=============================================================================
// Detection Tests
//=============================================================================

TEST_F(MetaLearningSNNBridgeTest, CheckAdaptation) {
    float level;
    bool high_adaptation = meta_learning_snn_check_adaptation(bridge, &level);
    EXPECT_GE(level, 0.0f);
    EXPECT_LE(level, 1.0f);
}

TEST_F(MetaLearningSNNBridgeTest, CheckTransfer) {
    meta_learning_snn_encode_transfer(bridge, 0.8f, 1);
    meta_learning_snn_simulate(bridge, 10.0f);

    float level;
    meta_learning_snn_check_transfer(bridge, &level);
    EXPECT_GE(level, 0.0f);
}

TEST_F(MetaLearningSNNBridgeTest, CheckStateChange) {
    float dims1[META_DIM_COUNT] = {0.2f};
    float dims2[META_DIM_COUNT] = {0.9f};

    meta_learning_snn_encode_state(bridge, dims1, 1);
    meta_learning_snn_simulate(bridge, 10.0f);

    meta_learning_snn_encode_state(bridge, dims2, 1);
    meta_learning_snn_simulate(bridge, 10.0f);

    float magnitude;
    meta_learning_snn_check_state_change(bridge, &magnitude);
}

//=============================================================================
// State Query Tests
//=============================================================================

TEST_F(MetaLearningSNNBridgeTest, GetDimState) {
    meta_learning_dim_state_t state;
    EXPECT_EQ(meta_learning_snn_get_dim_state(bridge, 0, &state), 0);
}

TEST_F(MetaLearningSNNBridgeTest, GetDimStateInvalidDim) {
    meta_learning_dim_state_t state;
    EXPECT_EQ(meta_learning_snn_get_dim_state(bridge, 100, &state), -1);
}

TEST_F(MetaLearningSNNBridgeTest, GetBridgeState) {
    meta_learning_snn_bridge_state_t state;
    EXPECT_EQ(meta_learning_snn_get_state(bridge, &state), 0);
    EXPECT_EQ(state.state, META_LEARNING_SNN_STATE_IDLE);
}

TEST_F(MetaLearningSNNBridgeTest, GetStats) {
    meta_learning_snn_stats_t stats;
    EXPECT_EQ(meta_learning_snn_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.total_evaluations, 0u);
}

TEST_F(MetaLearningSNNBridgeTest, ResetStats) {
    float dims[1] = {0.5f};
    meta_learning_snn_encode_state(bridge, dims, 1);
    meta_learning_snn_simulate(bridge, 10.0f);

    meta_learning_snn_stats_t stats;
    meta_learning_snn_get_stats(bridge, &stats);
    EXPECT_GT(stats.total_evaluations, 0u);

    EXPECT_EQ(meta_learning_snn_reset_stats(bridge), 0);
    meta_learning_snn_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_evaluations, 0u);
}

TEST_F(MetaLearningSNNBridgeTest, GetAdaptation) {
    float adaptation = meta_learning_snn_get_adaptation(bridge);
    EXPECT_GE(adaptation, 0.0f);
    EXPECT_LE(adaptation, 1.0f);
}

TEST_F(MetaLearningSNNBridgeTest, GetTotalActivity) {
    float activity = meta_learning_snn_get_total_activity(bridge);
    EXPECT_GE(activity, 0.0f);
}

//=============================================================================
// Callback Tests
//=============================================================================

static int adaptation_callback_count = 0;
static void test_adaptation_callback(meta_learning_snn_bridge_t*, float, uint64_t, void*) {
    adaptation_callback_count++;
}

TEST_F(MetaLearningSNNBridgeTest, RegisterAdaptationCallback) {
    EXPECT_EQ(meta_learning_snn_register_adaptation_callback(bridge, test_adaptation_callback, nullptr), 0);
}

TEST_F(MetaLearningSNNBridgeTest, RegisterInsightCallback) {
    EXPECT_EQ(meta_learning_snn_register_insight_callback(bridge, nullptr, nullptr), 0);
}

TEST_F(MetaLearningSNNBridgeTest, RegisterTransferCallback) {
    EXPECT_EQ(meta_learning_snn_register_transfer_callback(bridge, nullptr, nullptr), 0);
}

//=============================================================================
// Bio-Async Tests
//=============================================================================

TEST_F(MetaLearningSNNBridgeTest, BioAsyncNotEnabled) {
    EXPECT_EQ(meta_learning_snn_bio_async_connect(bridge), -1);
    EXPECT_FALSE(meta_learning_snn_is_bio_async_connected(bridge));
}

TEST_F(MetaLearningSNNBridgeTest, BioAsyncDisconnectSafe) {
    EXPECT_EQ(meta_learning_snn_bio_async_disconnect(bridge), 0);
}

//=============================================================================
// Integration Workflow Tests
//=============================================================================

TEST_F(MetaLearningSNNBridgeTest, FullWorkflow) {
    // Encode meta learning context
    float dims[META_DIM_COUNT] = {0};
    dims[META_DIM_LEARNING_RATE] = 0.8f;
    dims[META_DIM_STRATEGY_SELECT] = 0.2f;
    dims[META_DIM_TRANSFER] = 0.75f;
    dims[META_DIM_GENERALIZATION] = 0.9f;
    dims[META_DIM_LEARNING_TO_LEARN] = 0.6f;

    int spike_count = meta_learning_snn_encode_state(bridge, dims, META_DIM_COUNT);
    EXPECT_GE(spike_count, 0);

    // Simulate
    EXPECT_EQ(meta_learning_snn_simulate(bridge, 30.0f), 0);

    // Get insight
    meta_learning_insight_t insight;
    EXPECT_EQ(meta_learning_snn_get_insight(bridge, &insight), 0);

    // Verify all fields are valid
    EXPECT_GE(insight.learning_rate_level, 0.0f);
    EXPECT_LE(insight.learning_rate_level, 1.0f);
    EXPECT_GE(insight.adaptation_level, 0.0f);
    EXPECT_LE(insight.adaptation_level, 1.0f);
    EXPECT_GE(insight.transfer_potential, 0.0f);
    EXPECT_LE(insight.transfer_potential, 1.0f);
}

TEST_F(MetaLearningSNNBridgeTest, MultipleEvaluations) {
    for (int i = 0; i < 10; i++) {
        float dims[META_DIM_COUNT] = {0};
        dims[0] = (float)i / 10.0f;
        dims[1] = 1.0f - (float)i / 10.0f;

        meta_learning_snn_encode_state(bridge, dims, 2);
        meta_learning_snn_simulate(bridge, 10.0f);
    }

    meta_learning_snn_stats_t stats;
    meta_learning_snn_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_evaluations, 10u);
}
