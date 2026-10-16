/**
 * @file test_wellbeing_snn_bridge.cpp
 * @brief Unit tests for Wellbeing SNN Bridge
 */

#include <gtest/gtest.h>
#include <cmath>

#include "cognitive/wellbeing/nimcp_wellbeing_snn_bridge.h"

class WellbeingSNNBridgeTest : public ::testing::Test {
protected:
    wellbeing_snn_bridge_t* bridge = nullptr;

    void SetUp() override {
        wellbeing_snn_config_t config = wellbeing_snn_config_default();
        config.enable_bio_async = false;
        bridge = wellbeing_snn_create(&config);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            wellbeing_snn_destroy(bridge);
            bridge = nullptr;
        }
    }
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(WellbeingSNNBridgeTest, CreateWithDefaults) {
    wellbeing_snn_bridge_t* test_bridge = wellbeing_snn_create(nullptr);
    ASSERT_NE(test_bridge, nullptr);
    wellbeing_snn_destroy(test_bridge);
}

TEST_F(WellbeingSNNBridgeTest, CreateWithConfig) {
    wellbeing_snn_config_t config = wellbeing_snn_config_default();
    config.num_dimensions = 8;
    config.neurons_per_dim = 16;
    wellbeing_snn_bridge_t* test_bridge = wellbeing_snn_create(&config);
    ASSERT_NE(test_bridge, nullptr);
    wellbeing_snn_destroy(test_bridge);
}

TEST_F(WellbeingSNNBridgeTest, CreateWithInvalidConfig) {
    wellbeing_snn_config_t config = wellbeing_snn_config_default();
    config.num_dimensions = 0;
    wellbeing_snn_bridge_t* test_bridge = wellbeing_snn_create(&config);
    EXPECT_EQ(test_bridge, nullptr);
}

TEST_F(WellbeingSNNBridgeTest, Reset) {
    EXPECT_EQ(wellbeing_snn_reset(bridge), 0);
}

TEST_F(WellbeingSNNBridgeTest, ResetNull) {
    EXPECT_EQ(wellbeing_snn_reset(nullptr), -1);
}

TEST_F(WellbeingSNNBridgeTest, DestroyNull) {
    wellbeing_snn_destroy(nullptr); // Should not crash
}

//=============================================================================
// Default Config Tests
//=============================================================================

TEST_F(WellbeingSNNBridgeTest, DefaultConfigValues) {
    wellbeing_snn_config_t config = wellbeing_snn_config_default();
    EXPECT_EQ(config.num_dimensions, WELLBEING_DIM_COUNT);
    EXPECT_EQ(config.neurons_per_dim, WELLBEING_SNN_NEURONS_PER_DIM);
    EXPECT_GT(config.dt_ms, 0.0f);
    EXPECT_TRUE(config.enable_stress_detection);
}

//=============================================================================
// Encoding Tests
//=============================================================================

TEST_F(WellbeingSNNBridgeTest, EncodeState) {
    float dims[WELLBEING_DIM_COUNT] = {0};
    dims[WELLBEING_DIM_HEDONIC] = 0.8f;
    dims[WELLBEING_DIM_EUDAIMONIC] = 0.7f;
    dims[WELLBEING_DIM_VITALITY] = 0.6f;

    int spike_count = wellbeing_snn_encode_state(bridge, dims, WELLBEING_DIM_COUNT);
    EXPECT_GE(spike_count, 0);
}

TEST_F(WellbeingSNNBridgeTest, EncodeStateNull) {
    EXPECT_EQ(wellbeing_snn_encode_state(nullptr, nullptr, 0), -1);
    EXPECT_EQ(wellbeing_snn_encode_state(bridge, nullptr, 0), -1);
}

TEST_F(WellbeingSNNBridgeTest, EncodeHedonic) {
    int spike_count = wellbeing_snn_encode_hedonic(bridge, 0.8f, 0.2f);
    EXPECT_GE(spike_count, 0);
}

TEST_F(WellbeingSNNBridgeTest, EncodeEudaimonic) {
    int spike_count = wellbeing_snn_encode_eudaimonic(bridge, 0.9f, 0.8f, 0.7f);
    EXPECT_GE(spike_count, 0);
}

TEST_F(WellbeingSNNBridgeTest, EncodeStress) {
    int spike_count = wellbeing_snn_encode_stress(bridge, 0.6f, false);
    EXPECT_GE(spike_count, 0);
}

TEST_F(WellbeingSNNBridgeTest, EncodeChronicStress) {
    int spike_count = wellbeing_snn_encode_stress(bridge, 0.8f, true);
    EXPECT_GE(spike_count, 0);
}

TEST_F(WellbeingSNNBridgeTest, EncodeSocial) {
    int spike_count = wellbeing_snn_encode_social(bridge, 0.9f, 0.8f, 0.1f);
    EXPECT_GE(spike_count, 0);
}

//=============================================================================
// Simulation Tests
//=============================================================================

TEST_F(WellbeingSNNBridgeTest, SimulateBasic) {
    float dims[WELLBEING_DIM_COUNT] = {0.5f};
    wellbeing_snn_encode_state(bridge, dims, 1);
    EXPECT_EQ(wellbeing_snn_simulate(bridge, 10.0f), 0);
}

TEST_F(WellbeingSNNBridgeTest, SimulateNull) {
    EXPECT_EQ(wellbeing_snn_simulate(nullptr, 10.0f), -1);
}

TEST_F(WellbeingSNNBridgeTest, SimulateZeroDuration) {
    EXPECT_EQ(wellbeing_snn_simulate(bridge, 0.0f), -1);
}

TEST_F(WellbeingSNNBridgeTest, SimulateNegativeDuration) {
    EXPECT_EQ(wellbeing_snn_simulate(bridge, -10.0f), -1);
}

TEST_F(WellbeingSNNBridgeTest, Step) {
    float dims[WELLBEING_DIM_COUNT] = {0.5f};
    wellbeing_snn_encode_state(bridge, dims, 1);
    EXPECT_EQ(wellbeing_snn_step(bridge), 0);
}

TEST_F(WellbeingSNNBridgeTest, Forward) {
    float inputs[5] = {0.5f, 0.6f, 0.7f, 0.4f, 0.3f};
    int spike_count = wellbeing_snn_forward(bridge, inputs, 5);
    EXPECT_GE(spike_count, 0);
}

//=============================================================================
// Assessment Decoding Tests
//=============================================================================

TEST_F(WellbeingSNNBridgeTest, GetAssessment) {
    float dims[WELLBEING_DIM_COUNT] = {0};
    dims[WELLBEING_DIM_HEDONIC] = 0.9f;
    dims[WELLBEING_DIM_EUDAIMONIC] = 0.85f;
    dims[WELLBEING_DIM_VITALITY] = 0.8f;
    dims[WELLBEING_DIM_RESILIENCE] = 0.75f;
    wellbeing_snn_encode_state(bridge, dims, WELLBEING_DIM_COUNT);
    wellbeing_snn_simulate(bridge, 20.0f);

    wellbeing_assessment_t assessment;
    EXPECT_EQ(wellbeing_snn_get_assessment(bridge, &assessment), 0);
    EXPECT_GE(assessment.hedonic_tone, 0.0f);
    EXPECT_LE(assessment.hedonic_tone, 1.0f);
    EXPECT_GE(assessment.flourishing_score, 0.0f);
    EXPECT_LE(assessment.flourishing_score, 1.0f);
}

TEST_F(WellbeingSNNBridgeTest, GetAssessmentNull) {
    EXPECT_EQ(wellbeing_snn_get_assessment(nullptr, nullptr), -1);
    wellbeing_assessment_t assessment;
    EXPECT_EQ(wellbeing_snn_get_assessment(nullptr, &assessment), -1);
    EXPECT_EQ(wellbeing_snn_get_assessment(bridge, nullptr), -1);
}

TEST_F(WellbeingSNNBridgeTest, GetActivations) {
    float activations[WELLBEING_DIM_COUNT];
    EXPECT_EQ(wellbeing_snn_get_activations(bridge, activations, WELLBEING_DIM_COUNT), 0);
}

//=============================================================================
// Detection Tests
//=============================================================================

TEST_F(WellbeingSNNBridgeTest, CheckStress) {
    float level;
    bool high_stress = wellbeing_snn_check_stress(bridge, &level);
    EXPECT_GE(level, 0.0f);
    EXPECT_LE(level, 1.0f);
}

TEST_F(WellbeingSNNBridgeTest, CheckFlourishing) {
    float level;
    wellbeing_snn_check_flourishing(bridge, &level);
    EXPECT_GE(level, 0.0f);
    EXPECT_LE(level, 1.0f);
}

TEST_F(WellbeingSNNBridgeTest, CheckBalance) {
    float score;
    wellbeing_snn_check_balance(bridge, &score);
    EXPECT_GE(score, 0.0f);
    EXPECT_LE(score, 1.0f);
}

TEST_F(WellbeingSNNBridgeTest, HighStressTriggersDetection) {
    wellbeing_snn_encode_stress(bridge, 0.9f, true);
    wellbeing_snn_simulate(bridge, 30.0f);

    float level;
    wellbeing_snn_check_stress(bridge, &level);
    // Stress should be elevated after encoding high stress
    EXPECT_GE(level, 0.0f);
}

//=============================================================================
// State Query Tests
//=============================================================================

TEST_F(WellbeingSNNBridgeTest, GetDimState) {
    wellbeing_dim_state_t state;
    EXPECT_EQ(wellbeing_snn_get_dim_state(bridge, 0, &state), 0);
}

TEST_F(WellbeingSNNBridgeTest, GetDimStateInvalidDim) {
    wellbeing_dim_state_t state;
    EXPECT_EQ(wellbeing_snn_get_dim_state(bridge, 100, &state), -1);
}

TEST_F(WellbeingSNNBridgeTest, GetBridgeState) {
    wellbeing_snn_bridge_state_t state;
    EXPECT_EQ(wellbeing_snn_get_state(bridge, &state), 0);
    EXPECT_EQ(state.state, WELLBEING_SNN_STATE_IDLE);
}

TEST_F(WellbeingSNNBridgeTest, GetStats) {
    wellbeing_snn_stats_t stats;
    EXPECT_EQ(wellbeing_snn_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.total_evaluations, 0u);
}

TEST_F(WellbeingSNNBridgeTest, ResetStats) {
    float dims[1] = {0.5f};
    wellbeing_snn_encode_state(bridge, dims, 1);
    wellbeing_snn_simulate(bridge, 10.0f);

    wellbeing_snn_stats_t stats;
    wellbeing_snn_get_stats(bridge, &stats);
    EXPECT_GT(stats.total_evaluations, 0u);

    EXPECT_EQ(wellbeing_snn_reset_stats(bridge), 0);
    wellbeing_snn_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_evaluations, 0u);
}

TEST_F(WellbeingSNNBridgeTest, GetFlourishing) {
    float flourishing = wellbeing_snn_get_flourishing(bridge);
    EXPECT_GE(flourishing, 0.0f);
    EXPECT_LE(flourishing, 1.0f);
}

TEST_F(WellbeingSNNBridgeTest, GetTotalActivity) {
    float activity = wellbeing_snn_get_total_activity(bridge);
    EXPECT_GE(activity, 0.0f);
}

//=============================================================================
// Callback Tests
//=============================================================================

static int stress_callback_count = 0;
static void test_stress_callback(wellbeing_snn_bridge_t*, float, uint64_t, void*) {
    stress_callback_count++;
}

TEST_F(WellbeingSNNBridgeTest, RegisterStressCallback) {
    EXPECT_EQ(wellbeing_snn_register_stress_callback(bridge, test_stress_callback, nullptr), 0);
}

TEST_F(WellbeingSNNBridgeTest, RegisterAssessmentCallback) {
    EXPECT_EQ(wellbeing_snn_register_assessment_callback(bridge, nullptr, nullptr), 0);
}

TEST_F(WellbeingSNNBridgeTest, RegisterBalanceCallback) {
    EXPECT_EQ(wellbeing_snn_register_balance_callback(bridge, nullptr, nullptr), 0);
}

//=============================================================================
// Bio-Async Tests
//=============================================================================

TEST_F(WellbeingSNNBridgeTest, BioAsyncNotEnabled) {
    EXPECT_EQ(wellbeing_snn_bio_async_connect(bridge), -1);
    EXPECT_FALSE(wellbeing_snn_is_bio_async_connected(bridge));
}

TEST_F(WellbeingSNNBridgeTest, BioAsyncDisconnectSafe) {
    EXPECT_EQ(wellbeing_snn_bio_async_disconnect(bridge), 0);
}

//=============================================================================
// Integration Workflow Tests
//=============================================================================

TEST_F(WellbeingSNNBridgeTest, FullWorkflow) {
    // Encode wellbeing context
    float dims[WELLBEING_DIM_COUNT] = {0};
    dims[WELLBEING_DIM_HEDONIC] = 0.8f;
    dims[WELLBEING_DIM_EUDAIMONIC] = 0.75f;
    dims[WELLBEING_DIM_VITALITY] = 0.9f;
    dims[WELLBEING_DIM_RESILIENCE] = 0.7f;
    dims[WELLBEING_DIM_SOCIAL_CONNECTION] = 0.85f;

    int spike_count = wellbeing_snn_encode_state(bridge, dims, WELLBEING_DIM_COUNT);
    EXPECT_GE(spike_count, 0);

    // Simulate
    EXPECT_EQ(wellbeing_snn_simulate(bridge, 30.0f), 0);

    // Get assessment
    wellbeing_assessment_t assessment;
    EXPECT_EQ(wellbeing_snn_get_assessment(bridge, &assessment), 0);

    // Verify all fields are valid
    EXPECT_GE(assessment.hedonic_tone, 0.0f);
    EXPECT_LE(assessment.hedonic_tone, 1.0f);
    EXPECT_GE(assessment.flourishing_score, 0.0f);
    EXPECT_LE(assessment.flourishing_score, 1.0f);
    EXPECT_GE(assessment.stress_level, 0.0f);
    EXPECT_LE(assessment.stress_level, 1.0f);
}

TEST_F(WellbeingSNNBridgeTest, MultipleEvaluations) {
    for (int i = 0; i < 10; i++) {
        float dims[WELLBEING_DIM_COUNT] = {0};
        dims[0] = (float)i / 10.0f;
        dims[1] = 1.0f - (float)i / 10.0f;

        wellbeing_snn_encode_state(bridge, dims, 2);
        wellbeing_snn_simulate(bridge, 10.0f);
    }

    wellbeing_snn_stats_t stats;
    wellbeing_snn_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_evaluations, 10u);
}

TEST_F(WellbeingSNNBridgeTest, StressRecoveryScenario) {
    // Initial stressed state
    wellbeing_snn_encode_stress(bridge, 0.9f, false);
    wellbeing_snn_simulate(bridge, 20.0f);

    float initial_stress;
    wellbeing_snn_check_stress(bridge, &initial_stress);

    // Recovery state
    float recovery_dims[WELLBEING_DIM_COUNT] = {0};
    recovery_dims[WELLBEING_DIM_HEDONIC] = 0.7f;
    recovery_dims[WELLBEING_DIM_VITALITY] = 0.6f;
    recovery_dims[WELLBEING_DIM_STRESS] = 0.2f;

    wellbeing_snn_encode_state(bridge, recovery_dims, WELLBEING_DIM_COUNT);
    wellbeing_snn_simulate(bridge, 30.0f);

    float final_stress;
    wellbeing_snn_check_stress(bridge, &final_stress);

    // Stress should be different after recovery encoding
    EXPECT_GE(final_stress, 0.0f);
}
