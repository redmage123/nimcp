/**
 * @file test_ethics_snn_bridge.cpp
 * @brief Unit tests for Ethics-SNN Bridge
 */

#include <gtest/gtest.h>
#include <cmath>

#include "cognitive/ethics/nimcp_ethics_snn_bridge.h"

class EthicsSNNBridgeTest : public ::testing::Test {
protected:
    ethics_snn_bridge_t* bridge = nullptr;

    void SetUp() override {
        bridge = ethics_snn_create(nullptr);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            ethics_snn_destroy(bridge);
            bridge = nullptr;
        }
    }
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(EthicsSNNBridgeTest, CreateDestroy) {
    EXPECT_NE(bridge, nullptr);
}

TEST_F(EthicsSNNBridgeTest, CreateWithConfig) {
    ethics_snn_config_t config = ethics_snn_config_default();
    config.num_dimensions = 8;
    config.neurons_per_dim = 16;

    ethics_snn_bridge_t* custom = ethics_snn_create(&config);
    ASSERT_NE(custom, nullptr);
    ethics_snn_destroy(custom);
}

TEST_F(EthicsSNNBridgeTest, DefaultConfigValues) {
    ethics_snn_config_t config = ethics_snn_config_default();

    EXPECT_EQ(config.num_dimensions, ETHICS_DIM_COUNT);
    EXPECT_EQ(config.neurons_per_dim, ETHICS_SNN_NEURONS_PER_DIM);
    EXPECT_GT(config.dt_ms, 0.0f);
    EXPECT_EQ(config.enable_asimov_populations, true);
}

TEST_F(EthicsSNNBridgeTest, Reset) {
    float dims[ETHICS_DIM_COUNT] = {0.5f};
    ethics_snn_encode_context(bridge, dims, ETHICS_DIM_COUNT);
    ethics_snn_simulate(bridge, 10.0f);

    int ret = ethics_snn_reset(bridge);
    EXPECT_EQ(ret, 0);

    ethics_snn_bridge_state_t state;
    ethics_snn_get_state(bridge, &state);
    EXPECT_EQ(state.state, ETHICS_SNN_STATE_IDLE);
}

//=============================================================================
// Encoding Tests
//=============================================================================

TEST_F(EthicsSNNBridgeTest, EncodeContext) {
    float dims[ETHICS_DIM_COUNT];
    for (int i = 0; i < ETHICS_DIM_COUNT; i++) {
        dims[i] = 0.5f;
    }

    int spikes = ethics_snn_encode_context(bridge, dims, ETHICS_DIM_COUNT);
    EXPECT_GE(spikes, 0);
}

TEST_F(EthicsSNNBridgeTest, EncodeHarm) {
    int spikes = ethics_snn_encode_harm(bridge, 0.8f, 0.9f);
    EXPECT_GE(spikes, 0);

    float harm_level;
    bool detected = ethics_snn_check_harm(bridge, &harm_level);
    EXPECT_TRUE(detected);
    EXPECT_GT(harm_level, 0.3f);
}

TEST_F(EthicsSNNBridgeTest, EncodeHarmLowLevel) {
    int spikes = ethics_snn_encode_harm(bridge, 0.1f, 0.1f);
    EXPECT_GE(spikes, 0);

    float harm_level;
    bool detected = ethics_snn_check_harm(bridge, &harm_level);
    EXPECT_FALSE(detected);
}

TEST_F(EthicsSNNBridgeTest, EncodeGoldenRule) {
    int spikes = ethics_snn_encode_golden_rule(bridge, 0.7f, 0.7f, 0.9f);
    EXPECT_GE(spikes, 0);
}

TEST_F(EthicsSNNBridgeTest, EncodeNullBridge) {
    float dims[4] = {0.5f, 0.5f, 0.5f, 0.5f};
    int ret = ethics_snn_encode_context(nullptr, dims, 4);
    EXPECT_EQ(ret, -1);
}

//=============================================================================
// Simulation Tests
//=============================================================================

TEST_F(EthicsSNNBridgeTest, Simulate) {
    float dims[ETHICS_DIM_COUNT] = {0.5f};
    ethics_snn_encode_context(bridge, dims, ETHICS_DIM_COUNT);

    int ret = ethics_snn_simulate(bridge, 10.0f);
    EXPECT_EQ(ret, 0);
}

TEST_F(EthicsSNNBridgeTest, Step) {
    float dims[ETHICS_DIM_COUNT] = {0.5f};
    ethics_snn_encode_context(bridge, dims, ETHICS_DIM_COUNT);

    int ret = ethics_snn_step(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(EthicsSNNBridgeTest, Forward) {
    float inputs[128];
    for (int i = 0; i < 128; i++) {
        inputs[i] = 0.5f;
    }

    int spikes = ethics_snn_forward(bridge, inputs, 128);
    EXPECT_GE(spikes, 0);
}

//=============================================================================
// Decoding Tests
//=============================================================================

TEST_F(EthicsSNNBridgeTest, GetJudgment) {
    float dims[ETHICS_DIM_COUNT];
    for (int i = 0; i < ETHICS_DIM_COUNT; i++) {
        dims[i] = 0.5f;
    }

    ethics_snn_encode_context(bridge, dims, ETHICS_DIM_COUNT);
    ethics_snn_simulate(bridge, 20.0f);

    ethics_judgment_t judgment;
    int ret = ethics_snn_get_judgment(bridge, &judgment);
    EXPECT_EQ(ret, 0);

    EXPECT_GE(judgment.allow_score, 0.0f);
    EXPECT_LE(judgment.allow_score, 1.0f);
    EXPECT_GE(judgment.block_score, 0.0f);
    EXPECT_LE(judgment.block_score, 1.0f);
}

TEST_F(EthicsSNNBridgeTest, GetActivations) {
    float dims[ETHICS_DIM_COUNT];
    for (int i = 0; i < ETHICS_DIM_COUNT; i++) {
        dims[i] = (float)i / (float)ETHICS_DIM_COUNT;
    }

    ethics_snn_encode_context(bridge, dims, ETHICS_DIM_COUNT);

    float activations[ETHICS_DIM_COUNT];
    int ret = ethics_snn_get_activations(bridge, activations, ETHICS_DIM_COUNT);
    EXPECT_EQ(ret, 0);

    for (int i = 0; i < ETHICS_DIM_COUNT; i++) {
        EXPECT_GE(activations[i], 0.0f);
        EXPECT_LE(activations[i], 1.0f);
    }
}

TEST_F(EthicsSNNBridgeTest, CheckConflict) {
    float conflict_level;
    bool detected = ethics_snn_check_conflict(bridge, &conflict_level);

    // Initial state should have no conflict
    EXPECT_GE(conflict_level, 0.0f);
}

//=============================================================================
// State Query Tests
//=============================================================================

TEST_F(EthicsSNNBridgeTest, GetDimState) {
    float dims[ETHICS_DIM_COUNT] = {0.8f};
    ethics_snn_encode_context(bridge, dims, ETHICS_DIM_COUNT);

    ethics_dim_state_t state;
    int ret = ethics_snn_get_dim_state(bridge, ETHICS_DIM_HARM, &state);
    EXPECT_EQ(ret, 0);
    EXPECT_GE(state.activation, 0.0f);
}

TEST_F(EthicsSNNBridgeTest, GetBridgeState) {
    ethics_snn_bridge_state_t state;
    int ret = ethics_snn_get_state(bridge, &state);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(state.state, ETHICS_SNN_STATE_IDLE);
}

TEST_F(EthicsSNNBridgeTest, GetStats) {
    ethics_snn_stats_t stats;
    int ret = ethics_snn_get_stats(bridge, &stats);
    EXPECT_EQ(ret, 0);
}

TEST_F(EthicsSNNBridgeTest, ResetStats) {
    float dims[ETHICS_DIM_COUNT] = {0.5f};
    ethics_snn_encode_context(bridge, dims, ETHICS_DIM_COUNT);

    int ret = ethics_snn_reset_stats(bridge);
    EXPECT_EQ(ret, 0);

    ethics_snn_stats_t stats;
    ethics_snn_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_evaluations, 0u);
}

TEST_F(EthicsSNNBridgeTest, GetConfidence) {
    float conf = ethics_snn_get_confidence(bridge);
    EXPECT_GE(conf, 0.0f);
    EXPECT_LE(conf, 1.0f);
}

TEST_F(EthicsSNNBridgeTest, GetTotalActivity) {
    float dims[ETHICS_DIM_COUNT];
    for (int i = 0; i < ETHICS_DIM_COUNT; i++) {
        dims[i] = 0.5f;
    }
    ethics_snn_encode_context(bridge, dims, ETHICS_DIM_COUNT);

    float activity = ethics_snn_get_total_activity(bridge);
    EXPECT_GE(activity, 0.0f);
}

//=============================================================================
// Callback Tests
//=============================================================================

static int harm_callback_count = 0;
static void harm_callback(ethics_snn_bridge_t*, float, uint64_t, void*) {
    harm_callback_count++;
}

TEST_F(EthicsSNNBridgeTest, RegisterHarmCallback) {
    harm_callback_count = 0;
    int ret = ethics_snn_register_harm_callback(bridge, harm_callback, nullptr);
    EXPECT_EQ(ret, 0);

    ethics_snn_encode_harm(bridge, 0.9f, 0.9f);
    EXPECT_GT(harm_callback_count, 0);
}

TEST_F(EthicsSNNBridgeTest, RegisterJudgmentCallback) {
    int ret = ethics_snn_register_judgment_callback(bridge, nullptr, nullptr);
    EXPECT_EQ(ret, 0);
}

TEST_F(EthicsSNNBridgeTest, RegisterConflictCallback) {
    int ret = ethics_snn_register_conflict_callback(bridge, nullptr, nullptr);
    EXPECT_EQ(ret, 0);
}

//=============================================================================
// Bio-Async Tests
//=============================================================================

TEST_F(EthicsSNNBridgeTest, BioAsyncNotConnectedInitially) {
    bool connected = ethics_snn_is_bio_async_connected(bridge);
    EXPECT_FALSE(connected);
}

TEST_F(EthicsSNNBridgeTest, BioAsyncConnectWithDisabledConfig) {
    // Default config has bio_async disabled
    int ret = ethics_snn_bio_async_connect(bridge);
    EXPECT_EQ(ret, -1);
}

TEST_F(EthicsSNNBridgeTest, BioAsyncDisconnect) {
    int ret = ethics_snn_bio_async_disconnect(bridge);
    EXPECT_EQ(ret, 0);
}

//=============================================================================
// Integration Tests
//=============================================================================

TEST_F(EthicsSNNBridgeTest, FullEthicalEvaluationPipeline) {
    // Encode a potentially harmful action
    ethics_snn_encode_harm(bridge, 0.7f, 0.8f);

    // Encode context
    float dims[ETHICS_DIM_COUNT] = {0};
    dims[ETHICS_DIM_HARM] = 0.7f;
    dims[ETHICS_DIM_FAIRNESS] = 0.3f;
    dims[ETHICS_DIM_EMPATHY] = 0.6f;
    ethics_snn_encode_context(bridge, dims, ETHICS_DIM_COUNT);

    // Simulate processing
    ethics_snn_simulate(bridge, 50.0f);

    // Get judgment
    ethics_judgment_t judgment;
    int ret = ethics_snn_get_judgment(bridge, &judgment);
    EXPECT_EQ(ret, 0);

    // High harm should result in higher block score
    EXPECT_TRUE(judgment.harm_detected);
}

TEST_F(EthicsSNNBridgeTest, GoldenRuleActivation) {
    // Encode Golden Rule scenario
    ethics_snn_encode_golden_rule(bridge, 0.8f, 0.8f, 0.9f);

    // Get judgment
    ethics_judgment_t judgment;
    ethics_snn_get_judgment(bridge, &judgment);

    // High alignment should have high Golden Rule activation
    EXPECT_GT(judgment.golden_rule_activation, 0.5f);
}

TEST_F(EthicsSNNBridgeTest, AsimovFirstLawPriority) {
    float dims[ETHICS_DIM_COUNT] = {0};
    dims[ETHICS_DIM_ASIMOV_FIRST] = 1.0f;
    ethics_snn_encode_context(bridge, dims, ETHICS_DIM_COUNT);

    ethics_snn_simulate(bridge, 30.0f);

    ethics_judgment_t judgment;
    ethics_snn_get_judgment(bridge, &judgment);

    EXPECT_GT(judgment.first_law_activation, 0.5f);
}

TEST_F(EthicsSNNBridgeTest, StatsAccumulation) {
    for (int i = 0; i < 10; i++) {
        float dims[ETHICS_DIM_COUNT] = {0.5f};
        ethics_snn_encode_context(bridge, dims, ETHICS_DIM_COUNT);
        ethics_snn_simulate(bridge, 5.0f);
    }

    ethics_snn_stats_t stats;
    ethics_snn_get_stats(bridge, &stats);
    EXPECT_GE(stats.total_evaluations, 10u);
    EXPECT_GT(stats.total_simulations, 0u);
}

//=============================================================================
// Null Pointer Tests
//=============================================================================

TEST_F(EthicsSNNBridgeTest, NullBridgeHandling) {
    EXPECT_EQ(ethics_snn_reset(nullptr), -1);
    EXPECT_EQ(ethics_snn_simulate(nullptr, 10.0f), -1);
    EXPECT_EQ(ethics_snn_step(nullptr), -1);
    EXPECT_FALSE(ethics_snn_is_bio_async_connected(nullptr));
    EXPECT_LT(ethics_snn_get_confidence(nullptr), 0.0f);
    EXPECT_LT(ethics_snn_get_total_activity(nullptr), 0.0f);
}
