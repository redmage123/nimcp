/**
 * @file test_self_model_snn_bridge.cpp
 * @brief Unit tests for Self Model SNN Bridge
 */

#include <gtest/gtest.h>
#include <cmath>

#include "cognitive/self_model/nimcp_self_model_snn_bridge.h"

class SelfModelSNNBridgeTest : public ::testing::Test {
protected:
    self_model_snn_bridge_t* bridge = nullptr;

    void SetUp() override {
        self_model_snn_config_t config = self_model_snn_config_default();
        config.enable_bio_async = false;
        bridge = self_model_snn_create(&config);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            self_model_snn_destroy(bridge);
            bridge = nullptr;
        }
    }
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(SelfModelSNNBridgeTest, CreateWithDefaults) {
    self_model_snn_bridge_t* test_bridge = self_model_snn_create(nullptr);
    ASSERT_NE(test_bridge, nullptr);
    self_model_snn_destroy(test_bridge);
}

TEST_F(SelfModelSNNBridgeTest, CreateWithConfig) {
    self_model_snn_config_t config = self_model_snn_config_default();
    config.num_dimensions = 8;
    config.neurons_per_dim = 16;
    self_model_snn_bridge_t* test_bridge = self_model_snn_create(&config);
    ASSERT_NE(test_bridge, nullptr);
    self_model_snn_destroy(test_bridge);
}

TEST_F(SelfModelSNNBridgeTest, CreateWithInvalidConfig) {
    self_model_snn_config_t config = self_model_snn_config_default();
    config.num_dimensions = 0;
    self_model_snn_bridge_t* test_bridge = self_model_snn_create(&config);
    EXPECT_EQ(test_bridge, nullptr);
}

TEST_F(SelfModelSNNBridgeTest, Reset) {
    EXPECT_EQ(self_model_snn_reset(bridge), 0);
}

TEST_F(SelfModelSNNBridgeTest, ResetNull) {
    EXPECT_EQ(self_model_snn_reset(nullptr), -1);
}

TEST_F(SelfModelSNNBridgeTest, DestroyNull) {
    self_model_snn_destroy(nullptr); // Should not crash
}

//=============================================================================
// Default Config Tests
//=============================================================================

TEST_F(SelfModelSNNBridgeTest, DefaultConfigValues) {
    self_model_snn_config_t config = self_model_snn_config_default();
    EXPECT_EQ(config.num_dimensions, SELF_DIM_COUNT);
    EXPECT_EQ(config.neurons_per_dim, SELF_MODEL_SNN_NEURONS_PER_DIM);
    EXPECT_GT(config.dt_ms, 0.0f);
    EXPECT_TRUE(config.enable_identity_core);
    EXPECT_TRUE(config.enable_boundary_detection);
}

//=============================================================================
// Encoding Tests
//=============================================================================

TEST_F(SelfModelSNNBridgeTest, EncodeState) {
    float dims[SELF_DIM_COUNT] = {0};
    dims[SELF_DIM_BODY_STATE] = 0.8f;
    dims[SELF_DIM_AGENCY] = 0.7f;
    dims[SELF_DIM_IDENTITY] = 0.9f;

    int spike_count = self_model_snn_encode_state(bridge, dims, SELF_DIM_COUNT);
    EXPECT_GE(spike_count, 0);
}

TEST_F(SelfModelSNNBridgeTest, EncodeStateNull) {
    EXPECT_EQ(self_model_snn_encode_state(nullptr, nullptr, 0), -1);
    EXPECT_EQ(self_model_snn_encode_state(bridge, nullptr, 0), -1);
}

TEST_F(SelfModelSNNBridgeTest, EncodeBodyState) {
    int spike_count = self_model_snn_encode_body_state(bridge, 0.7f, 0.8f);
    EXPECT_GE(spike_count, 0);
}

TEST_F(SelfModelSNNBridgeTest, EncodeAgency) {
    int spike_count = self_model_snn_encode_agency(bridge, 0.9f, 0.85f);
    EXPECT_GE(spike_count, 0);
}

TEST_F(SelfModelSNNBridgeTest, EncodeBoundary) {
    int spike_count = self_model_snn_encode_boundary(bridge, 0.6f, 1);
    EXPECT_GE(spike_count, 0);
}

TEST_F(SelfModelSNNBridgeTest, EncodeAgencyLow) {
    // Test agency disruption detection
    int spike_count = self_model_snn_encode_agency(bridge, 0.3f, 0.2f); // Below threshold
    EXPECT_GE(spike_count, 0);
}

//=============================================================================
// Simulation Tests
//=============================================================================

TEST_F(SelfModelSNNBridgeTest, SimulateBasic) {
    float dims[SELF_DIM_COUNT] = {0.5f};
    self_model_snn_encode_state(bridge, dims, 1);
    EXPECT_EQ(self_model_snn_simulate(bridge, 10.0f), 0);
}

TEST_F(SelfModelSNNBridgeTest, SimulateNull) {
    EXPECT_EQ(self_model_snn_simulate(nullptr, 10.0f), -1);
}

TEST_F(SelfModelSNNBridgeTest, SimulateZeroDuration) {
    EXPECT_EQ(self_model_snn_simulate(bridge, 0.0f), -1);
}

TEST_F(SelfModelSNNBridgeTest, SimulateNegativeDuration) {
    EXPECT_EQ(self_model_snn_simulate(bridge, -10.0f), -1);
}

TEST_F(SelfModelSNNBridgeTest, Step) {
    float dims[SELF_DIM_COUNT] = {0.5f};
    self_model_snn_encode_state(bridge, dims, 1);
    EXPECT_EQ(self_model_snn_step(bridge), 0);
}

TEST_F(SelfModelSNNBridgeTest, Forward) {
    float inputs[5] = {0.5f, 0.6f, 0.7f, 0.4f, 0.3f};
    int spike_count = self_model_snn_forward(bridge, inputs, 5);
    EXPECT_GE(spike_count, 0);
}

//=============================================================================
// Insight Decoding Tests
//=============================================================================

TEST_F(SelfModelSNNBridgeTest, GetInsight) {
    float dims[SELF_DIM_COUNT] = {0};
    dims[SELF_DIM_BODY_STATE] = 0.9f;
    dims[SELF_DIM_AGENCY] = 0.85f;
    dims[SELF_DIM_IDENTITY] = 0.8f;
    self_model_snn_encode_state(bridge, dims, SELF_DIM_COUNT);
    self_model_snn_simulate(bridge, 20.0f);

    self_model_insight_t insight;
    EXPECT_EQ(self_model_snn_get_insight(bridge, &insight), 0);
    EXPECT_GE(insight.body_state_level, 0.0f);
    EXPECT_LE(insight.body_state_level, 1.0f);
    EXPECT_GE(insight.agency_level, 0.0f);
    EXPECT_LE(insight.agency_level, 1.0f);
    EXPECT_GE(insight.identity_coherence, 0.0f);
    EXPECT_LE(insight.identity_coherence, 1.0f);
}

TEST_F(SelfModelSNNBridgeTest, GetInsightNull) {
    EXPECT_EQ(self_model_snn_get_insight(nullptr, nullptr), -1);
    self_model_insight_t insight;
    EXPECT_EQ(self_model_snn_get_insight(nullptr, &insight), -1);
    EXPECT_EQ(self_model_snn_get_insight(bridge, nullptr), -1);
}

TEST_F(SelfModelSNNBridgeTest, GetActivations) {
    float activations[SELF_DIM_COUNT];
    EXPECT_EQ(self_model_snn_get_activations(bridge, activations, SELF_DIM_COUNT), 0);
}

//=============================================================================
// Detection Tests
//=============================================================================

TEST_F(SelfModelSNNBridgeTest, CheckBoundary) {
    float level;
    bool boundary_violated = self_model_snn_check_boundary(bridge, &level);
    EXPECT_GE(level, 0.0f);
    EXPECT_LE(level, 1.0f);
    (void)boundary_violated; // Suppress unused warning
}

TEST_F(SelfModelSNNBridgeTest, CheckAgency) {
    float level;
    bool agency_disrupted = self_model_snn_check_agency(bridge, &level);
    EXPECT_GE(level, 0.0f);
    (void)agency_disrupted; // Suppress unused warning
}

TEST_F(SelfModelSNNBridgeTest, CheckIdentityChange) {
    float dims1[SELF_DIM_COUNT] = {0.2f};
    float dims2[SELF_DIM_COUNT] = {0.9f};

    self_model_snn_encode_state(bridge, dims1, 1);
    self_model_snn_simulate(bridge, 10.0f);

    self_model_snn_encode_state(bridge, dims2, 1);
    self_model_snn_simulate(bridge, 10.0f);

    float magnitude;
    self_model_snn_check_identity_change(bridge, &magnitude);
}

//=============================================================================
// State Query Tests
//=============================================================================

TEST_F(SelfModelSNNBridgeTest, GetDimState) {
    self_model_dim_state_t state;
    EXPECT_EQ(self_model_snn_get_dim_state(bridge, 0, &state), 0);
}

TEST_F(SelfModelSNNBridgeTest, GetDimStateInvalidDim) {
    self_model_dim_state_t state;
    EXPECT_EQ(self_model_snn_get_dim_state(bridge, 100, &state), -1);
}

TEST_F(SelfModelSNNBridgeTest, GetBridgeState) {
    self_model_snn_bridge_state_t state;
    EXPECT_EQ(self_model_snn_get_state(bridge, &state), 0);
    EXPECT_EQ(state.state, SELF_MODEL_SNN_STATE_IDLE);
}

TEST_F(SelfModelSNNBridgeTest, GetStats) {
    self_model_snn_stats_t stats;
    EXPECT_EQ(self_model_snn_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.total_evaluations, 0u);
}

TEST_F(SelfModelSNNBridgeTest, ResetStats) {
    float dims[1] = {0.5f};
    self_model_snn_encode_state(bridge, dims, 1);
    self_model_snn_simulate(bridge, 10.0f);

    self_model_snn_stats_t stats;
    self_model_snn_get_stats(bridge, &stats);
    EXPECT_GT(stats.total_evaluations, 0u);

    EXPECT_EQ(self_model_snn_reset_stats(bridge), 0);
    self_model_snn_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_evaluations, 0u);
}

TEST_F(SelfModelSNNBridgeTest, GetAgency) {
    float agency = self_model_snn_get_agency(bridge);
    EXPECT_GE(agency, 0.0f);
    EXPECT_LE(agency, 1.0f);
}

TEST_F(SelfModelSNNBridgeTest, GetTotalActivity) {
    float activity = self_model_snn_get_total_activity(bridge);
    EXPECT_GE(activity, 0.0f);
}

//=============================================================================
// Callback Tests
//=============================================================================

static int boundary_callback_count = 0;
static void test_boundary_callback(self_model_snn_bridge_t*, float, uint64_t, void*) {
    boundary_callback_count++;
}

TEST_F(SelfModelSNNBridgeTest, RegisterBoundaryCallback) {
    EXPECT_EQ(self_model_snn_register_boundary_callback(bridge, test_boundary_callback, nullptr), 0);
}

TEST_F(SelfModelSNNBridgeTest, RegisterInsightCallback) {
    EXPECT_EQ(self_model_snn_register_insight_callback(bridge, nullptr, nullptr), 0);
}

TEST_F(SelfModelSNNBridgeTest, RegisterAgencyCallback) {
    EXPECT_EQ(self_model_snn_register_agency_callback(bridge, nullptr, nullptr), 0);
}

//=============================================================================
// Bio-Async Tests
//=============================================================================

TEST_F(SelfModelSNNBridgeTest, BioAsyncNotEnabled) {
    EXPECT_EQ(self_model_snn_bio_async_connect(bridge), -1);
    EXPECT_FALSE(self_model_snn_is_bio_async_connected(bridge));
}

TEST_F(SelfModelSNNBridgeTest, BioAsyncDisconnectSafe) {
    EXPECT_EQ(self_model_snn_bio_async_disconnect(bridge), 0);
}

//=============================================================================
// Integration Workflow Tests
//=============================================================================

TEST_F(SelfModelSNNBridgeTest, FullWorkflow) {
    // Encode self model context
    float dims[SELF_DIM_COUNT] = {0};
    dims[SELF_DIM_BODY_STATE] = 0.8f;
    dims[SELF_DIM_AGENCY] = 0.85f;
    dims[SELF_DIM_OWNERSHIP] = 0.9f;
    dims[SELF_DIM_IDENTITY] = 0.95f;
    dims[SELF_DIM_BOUNDARY] = 0.7f;
    dims[SELF_DIM_CONTINUITY] = 0.8f;

    int spike_count = self_model_snn_encode_state(bridge, dims, SELF_DIM_COUNT);
    EXPECT_GE(spike_count, 0);

    // Simulate
    EXPECT_EQ(self_model_snn_simulate(bridge, 30.0f), 0);

    // Get insight
    self_model_insight_t insight;
    EXPECT_EQ(self_model_snn_get_insight(bridge, &insight), 0);

    // Verify all fields are valid
    EXPECT_GE(insight.body_state_level, 0.0f);
    EXPECT_LE(insight.body_state_level, 1.0f);
    EXPECT_GE(insight.agency_level, 0.0f);
    EXPECT_LE(insight.agency_level, 1.0f);
    EXPECT_GE(insight.identity_coherence, 0.0f);
    EXPECT_LE(insight.identity_coherence, 1.0f);
    EXPECT_GE(insight.boundary_clarity, 0.0f);
    EXPECT_LE(insight.boundary_clarity, 1.0f);
}

TEST_F(SelfModelSNNBridgeTest, MultipleEvaluations) {
    for (int i = 0; i < 10; i++) {
        float dims[SELF_DIM_COUNT] = {0};
        dims[0] = (float)i / 10.0f;
        dims[1] = 1.0f - (float)i / 10.0f;

        self_model_snn_encode_state(bridge, dims, 2);
        self_model_snn_simulate(bridge, 10.0f);
    }

    self_model_snn_stats_t stats;
    self_model_snn_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_evaluations, 10u);
}

TEST_F(SelfModelSNNBridgeTest, BodyStateProcessing) {
    // Test interoceptive and proprioceptive body state
    self_model_snn_encode_body_state(bridge, 0.8f, 0.9f);
    self_model_snn_simulate(bridge, 20.0f);

    self_model_insight_t insight;
    self_model_snn_get_insight(bridge, &insight);
    EXPECT_GE(insight.body_state_level, 0.0f);
}

TEST_F(SelfModelSNNBridgeTest, AgencyProcessing) {
    // Test agency and efference copy match
    self_model_snn_encode_agency(bridge, 0.95f, 0.9f);
    self_model_snn_simulate(bridge, 20.0f);

    self_model_insight_t insight;
    self_model_snn_get_insight(bridge, &insight);
    EXPECT_GE(insight.agency_level, 0.0f);
}
