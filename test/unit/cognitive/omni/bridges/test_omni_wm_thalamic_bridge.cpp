/**
 * @file test_omni_wm_thalamic_bridge.cpp
 * @brief Comprehensive unit tests for World Model Thalamic Bridge
 *
 * WHAT: Tests for WM-Thalamus integration bridge
 * WHY:  Verify attention gating, prediction biasing, and TRN inhibition
 * HOW:  GTest-based tests for lifecycle, connection, sensory gating,
 *       attention biasing, TRN inhibition, and prediction integration
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>

extern "C" {
#include "cognitive/omni/bridges/nimcp_omni_wm_thalamic_bridge.h"
#include "utils/error/nimcp_error_codes.h"
}

// =============================================================================
// Constants and Helpers
// =============================================================================

static constexpr float FLOAT_TOLERANCE = 1e-5f;
static constexpr float DEFAULT_DT = 0.016f;
static constexpr uint32_t TEST_INPUT_DIM = 64;
static constexpr uint32_t TEST_ATTENTION_DIM = 32;

static bool float_equals(float a, float b, float tol = FLOAT_TOLERANCE)
{
    return std::fabs(a - b) < tol;
}

static bool float_in_range(float val, float min_val, float max_val)
{
    return val >= min_val && val <= max_val;
}

// =============================================================================
// Test Fixture
// =============================================================================

class OmniWmThalamicBridgeTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        bridge_ = omni_wm_thalamic_bridge_create(nullptr);
    }

    void TearDown() override
    {
        if (bridge_) {
            omni_wm_thalamic_bridge_destroy(bridge_);
            bridge_ = nullptr;
        }
    }

    // Helper to create test input
    std::vector<float> create_test_input(uint32_t dim, float base_value)
    {
        std::vector<float> input(dim);
        for (uint32_t i = 0; i < dim; i++) {
            input[i] = base_value + (float)i * 0.01f;
        }
        return input;
    }

    omni_wm_thalamic_bridge_t* bridge_ = nullptr;
};

// =============================================================================
// 1. Lifecycle Tests
// =============================================================================

TEST_F(OmniWmThalamicBridgeTest, CreateWithNullConfig)
{
    ASSERT_NE(bridge_, nullptr);
}

TEST_F(OmniWmThalamicBridgeTest, CreateWithDefaultConfig)
{
    omni_wm_thalamic_bridge_config_t config;
    ASSERT_EQ(omni_wm_thalamic_bridge_default_config(&config), NIMCP_SUCCESS);

    omni_wm_thalamic_bridge_t* bridge = omni_wm_thalamic_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);
    omni_wm_thalamic_bridge_destroy(bridge);
}

TEST_F(OmniWmThalamicBridgeTest, CreateWithCustomConfig)
{
    omni_wm_thalamic_bridge_config_t config;
    ASSERT_EQ(omni_wm_thalamic_bridge_default_config(&config), NIMCP_SUCCESS);

    // Customize
    config.enable_modulation = true;
    config.sensitivity = 1.5f;
    config.enable_sensory_gating = true;
    config.attention_baseline = 0.6f;
    config.min_attention_threshold = 0.3f;
    config.enable_prediction_biasing = true;
    config.enable_trn_inhibition = true;
    config.trn_inhibition_strength = 0.5f;

    omni_wm_thalamic_bridge_t* bridge = omni_wm_thalamic_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);

    EXPECT_FLOAT_EQ(bridge->config.sensitivity, 1.5f);
    EXPECT_FLOAT_EQ(bridge->config.attention_baseline, 0.6f);
    EXPECT_FLOAT_EQ(bridge->config.trn_inhibition_strength, 0.5f);

    omni_wm_thalamic_bridge_destroy(bridge);
}

TEST_F(OmniWmThalamicBridgeTest, DestroyNullSafe)
{
    omni_wm_thalamic_bridge_destroy(nullptr);
}

TEST_F(OmniWmThalamicBridgeTest, DestroyValidBridge)
{
    omni_wm_thalamic_bridge_t* bridge = omni_wm_thalamic_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);
    omni_wm_thalamic_bridge_destroy(bridge);
}

TEST_F(OmniWmThalamicBridgeTest, ResetNullFails)
{
    nimcp_error_t result = omni_wm_thalamic_bridge_reset(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmThalamicBridgeTest, ResetValidBridge)
{
    nimcp_error_t result = omni_wm_thalamic_bridge_reset(bridge_);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

// =============================================================================
// 2. Default Config Tests
// =============================================================================

TEST_F(OmniWmThalamicBridgeTest, DefaultConfigNullFails)
{
    nimcp_error_t result = omni_wm_thalamic_bridge_default_config(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmThalamicBridgeTest, DefaultConfigSetsReasonableValues)
{
    omni_wm_thalamic_bridge_config_t config;
    ASSERT_EQ(omni_wm_thalamic_bridge_default_config(&config), NIMCP_SUCCESS);

    EXPECT_TRUE(float_in_range(config.sensitivity, 0.5f, 2.0f));
    EXPECT_TRUE(float_in_range(config.attention_baseline, 0.0f, 1.0f));
    EXPECT_TRUE(float_in_range(config.min_attention_threshold, 0.0f, 1.0f));
    EXPECT_TRUE(float_in_range(config.trn_inhibition_strength, 0.0f, 1.0f));
    EXPECT_TRUE(float_in_range(config.pulvinar_attention_gain, 0.5f, 2.0f));
}

TEST_F(OmniWmThalamicBridgeTest, ValidateConfigNullFails)
{
    nimcp_error_t result = omni_wm_thalamic_bridge_validate_config(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmThalamicBridgeTest, ValidateConfigDefaultSucceeds)
{
    omni_wm_thalamic_bridge_config_t config;
    ASSERT_EQ(omni_wm_thalamic_bridge_default_config(&config), NIMCP_SUCCESS);
    EXPECT_EQ(omni_wm_thalamic_bridge_validate_config(&config), NIMCP_SUCCESS);
}

// =============================================================================
// 3. Connection Tests
// =============================================================================

TEST_F(OmniWmThalamicBridgeTest, ConnectNullBridgeFails)
{
    nimcp_error_t result = omni_wm_thalamic_bridge_connect(
        nullptr, nullptr, nullptr, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmThalamicBridgeTest, IsConnectedBeforeConnect)
{
    EXPECT_FALSE(omni_wm_thalamic_bridge_is_connected(bridge_));
}

TEST_F(OmniWmThalamicBridgeTest, IsConnectedNullReturnsFalse)
{
    EXPECT_FALSE(omni_wm_thalamic_bridge_is_connected(nullptr));
}

TEST_F(OmniWmThalamicBridgeTest, ConnectWorldModelNullBridgeFails)
{
    nimcp_error_t result = omni_wm_thalamic_bridge_connect_world_model(nullptr, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmThalamicBridgeTest, ConnectThalamusNullBridgeFails)
{
    nimcp_error_t result = omni_wm_thalamic_bridge_connect_thalamus(nullptr, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmThalamicBridgeTest, ConnectRouterNullBridgeFails)
{
    nimcp_error_t result = omni_wm_thalamic_bridge_connect_router(nullptr, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

// =============================================================================
// 4. Update Tests
// =============================================================================

TEST_F(OmniWmThalamicBridgeTest, UpdateNullBridgeFails)
{
    nimcp_error_t result = omni_wm_thalamic_bridge_update(nullptr, DEFAULT_DT);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmThalamicBridgeTest, UpdateUnconnectedBridge)
{
    nimcp_error_t result = omni_wm_thalamic_bridge_update(bridge_, DEFAULT_DT);
    (void)result;  // May succeed or fail, but should not crash
}

TEST_F(OmniWmThalamicBridgeTest, SetArousalNullBridgeFails)
{
    nimcp_error_t result = omni_wm_thalamic_bridge_set_arousal(nullptr, 0.5f);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmThalamicBridgeTest, SetArousalValidRange)
{
    EXPECT_EQ(omni_wm_thalamic_bridge_set_arousal(bridge_, 0.0f), NIMCP_SUCCESS);
    EXPECT_EQ(omni_wm_thalamic_bridge_set_arousal(bridge_, 0.5f), NIMCP_SUCCESS);
    EXPECT_EQ(omni_wm_thalamic_bridge_set_arousal(bridge_, 1.0f), NIMCP_SUCCESS);
}

// =============================================================================
// 5. Sensory Gating Tests
// =============================================================================

TEST_F(OmniWmThalamicBridgeTest, GateInputNullBridgeFails)
{
    std::vector<float> input = create_test_input(TEST_INPUT_DIM, 0.5f);
    std::vector<float> output(TEST_INPUT_DIM);
    float attention_applied;

    nimcp_error_t result = omni_wm_thalamic_bridge_gate_input(
        nullptr, WM_THAL_NUCLEUS_LGN, input.data(), TEST_INPUT_DIM,
        output.data(), TEST_INPUT_DIM, &attention_applied);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmThalamicBridgeTest, GateInputNullInputFails)
{
    std::vector<float> output(TEST_INPUT_DIM);
    float attention_applied;

    nimcp_error_t result = omni_wm_thalamic_bridge_gate_input(
        bridge_, WM_THAL_NUCLEUS_LGN, nullptr, TEST_INPUT_DIM,
        output.data(), TEST_INPUT_DIM, &attention_applied);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmThalamicBridgeTest, GateInputNullOutputFails)
{
    std::vector<float> input = create_test_input(TEST_INPUT_DIM, 0.5f);
    float attention_applied;

    nimcp_error_t result = omni_wm_thalamic_bridge_gate_input(
        bridge_, WM_THAL_NUCLEUS_LGN, input.data(), TEST_INPUT_DIM,
        nullptr, TEST_INPUT_DIM, &attention_applied);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmThalamicBridgeTest, GateVisualNullBridgeFails)
{
    std::vector<float> input = create_test_input(TEST_INPUT_DIM, 0.5f);
    std::vector<float> output(TEST_INPUT_DIM);

    nimcp_error_t result = omni_wm_thalamic_bridge_gate_visual(
        nullptr, input.data(), TEST_INPUT_DIM, output.data(), TEST_INPUT_DIM);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmThalamicBridgeTest, GateAuditoryNullBridgeFails)
{
    std::vector<float> input = create_test_input(TEST_INPUT_DIM, 0.5f);
    std::vector<float> output(TEST_INPUT_DIM);

    nimcp_error_t result = omni_wm_thalamic_bridge_gate_auditory(
        nullptr, input.data(), TEST_INPUT_DIM, output.data(), TEST_INPUT_DIM);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmThalamicBridgeTest, GateMotorNullBridgeFails)
{
    std::vector<float> input = create_test_input(TEST_INPUT_DIM, 0.5f);
    std::vector<float> output(TEST_INPUT_DIM);

    nimcp_error_t result = omni_wm_thalamic_bridge_gate_motor(
        nullptr, input.data(), TEST_INPUT_DIM, output.data(), TEST_INPUT_DIM);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmThalamicBridgeTest, GateExecutiveNullBridgeFails)
{
    std::vector<float> input = create_test_input(TEST_INPUT_DIM, 0.5f);
    std::vector<float> output(TEST_INPUT_DIM);

    nimcp_error_t result = omni_wm_thalamic_bridge_gate_executive(
        nullptr, input.data(), TEST_INPUT_DIM, output.data(), TEST_INPUT_DIM);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmThalamicBridgeTest, GateInputAllNuclei)
{
    std::vector<float> input = create_test_input(TEST_INPUT_DIM, 0.5f);
    std::vector<float> output(TEST_INPUT_DIM);
    float attention_applied;

    for (int nucleus = 0; nucleus < WM_THAL_NUCLEUS_COUNT; nucleus++) {
        if (nucleus == WM_THAL_NUCLEUS_TRN) continue;  // TRN is inhibitory, skip

        nimcp_error_t result = omni_wm_thalamic_bridge_gate_input(
            bridge_, (wm_thal_nucleus_type_t)nucleus, input.data(), TEST_INPUT_DIM,
            output.data(), TEST_INPUT_DIM, &attention_applied);
        // Should handle even unconnected gracefully
        (void)result;
    }
}

// =============================================================================
// 6. Attention Biasing Tests
// =============================================================================

TEST_F(OmniWmThalamicBridgeTest, SetAttentionBiasNullBridgeFails)
{
    std::vector<float> bias = create_test_input(TEST_ATTENTION_DIM, 0.5f);

    nimcp_error_t result = omni_wm_thalamic_bridge_set_attention_bias(
        nullptr, bias.data(), TEST_ATTENTION_DIM, 0.8f);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmThalamicBridgeTest, SetAttentionBiasNullBiasFails)
{
    nimcp_error_t result = omni_wm_thalamic_bridge_set_attention_bias(
        bridge_, nullptr, TEST_ATTENTION_DIM, 0.8f);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmThalamicBridgeTest, SetNucleusAttentionNullBridgeFails)
{
    nimcp_error_t result = omni_wm_thalamic_bridge_set_nucleus_attention(
        nullptr, WM_THAL_NUCLEUS_LGN, 0.7f);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmThalamicBridgeTest, SetNucleusAttentionValidParams)
{
    nimcp_error_t result = omni_wm_thalamic_bridge_set_nucleus_attention(
        bridge_, WM_THAL_NUCLEUS_LGN, 0.7f);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmThalamicBridgeTest, SetNucleusAttentionBoundary)
{
    EXPECT_EQ(omni_wm_thalamic_bridge_set_nucleus_attention(
        bridge_, WM_THAL_NUCLEUS_LGN, 0.0f), NIMCP_SUCCESS);
    EXPECT_EQ(omni_wm_thalamic_bridge_set_nucleus_attention(
        bridge_, WM_THAL_NUCLEUS_LGN, 1.0f), NIMCP_SUCCESS);
}

TEST_F(OmniWmThalamicBridgeTest, GetNucleusAttentionNullReturnsNegative)
{
    float attention = omni_wm_thalamic_bridge_get_nucleus_attention(
        nullptr, WM_THAL_NUCLEUS_LGN);
    EXPECT_LT(attention, 0.0f);
}

TEST_F(OmniWmThalamicBridgeTest, SetAndGetNucleusAttention)
{
    omni_wm_thalamic_bridge_set_nucleus_attention(bridge_, WM_THAL_NUCLEUS_MGN, 0.8f);
    float attention = omni_wm_thalamic_bridge_get_nucleus_attention(
        bridge_, WM_THAL_NUCLEUS_MGN);
    EXPECT_TRUE(float_in_range(attention, 0.0f, 1.0f));
}

TEST_F(OmniWmThalamicBridgeTest, SetPulvinarAttentionNullBridgeFails)
{
    std::vector<float> weights = create_test_input(TEST_ATTENTION_DIM, 0.5f);

    nimcp_error_t result = omni_wm_thalamic_bridge_set_pulvinar_attention(
        nullptr, weights.data(), TEST_ATTENTION_DIM);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmThalamicBridgeTest, SetPulvinarAttentionNullWeightsFails)
{
    nimcp_error_t result = omni_wm_thalamic_bridge_set_pulvinar_attention(
        bridge_, nullptr, TEST_ATTENTION_DIM);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmThalamicBridgeTest, PredictSalienceNullBridgeFails)
{
    std::vector<float> salience(TEST_ATTENTION_DIM);

    nimcp_error_t result = omni_wm_thalamic_bridge_predict_salience(
        nullptr, salience.data(), TEST_ATTENTION_DIM);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmThalamicBridgeTest, PredictSalienceNullOutputFails)
{
    nimcp_error_t result = omni_wm_thalamic_bridge_predict_salience(
        bridge_, nullptr, TEST_ATTENTION_DIM);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

// =============================================================================
// 7. TRN Inhibition Tests
// =============================================================================

TEST_F(OmniWmThalamicBridgeTest, ApplyTrnInhibitionNullBridgeFails)
{
    nimcp_error_t result = omni_wm_thalamic_bridge_apply_trn_inhibition(
        nullptr, WM_THAL_NUCLEUS_LGN, 0.5f);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmThalamicBridgeTest, ApplyTrnInhibitionValidParams)
{
    nimcp_error_t result = omni_wm_thalamic_bridge_apply_trn_inhibition(
        bridge_, WM_THAL_NUCLEUS_LGN, 0.5f);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmThalamicBridgeTest, ApplyTrnInhibitionBoundary)
{
    EXPECT_EQ(omni_wm_thalamic_bridge_apply_trn_inhibition(
        bridge_, WM_THAL_NUCLEUS_LGN, 0.0f), NIMCP_SUCCESS);
    EXPECT_EQ(omni_wm_thalamic_bridge_apply_trn_inhibition(
        bridge_, WM_THAL_NUCLEUS_LGN, 1.0f), NIMCP_SUCCESS);
}

TEST_F(OmniWmThalamicBridgeTest, ApplySelectiveInhibitionNullBridgeFails)
{
    std::vector<float> inhibition_map = create_test_input(TEST_INPUT_DIM, 0.3f);

    nimcp_error_t result = omni_wm_thalamic_bridge_apply_selective_inhibition(
        nullptr, inhibition_map.data(), TEST_INPUT_DIM);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmThalamicBridgeTest, ApplySelectiveInhibitionNullMapFails)
{
    nimcp_error_t result = omni_wm_thalamic_bridge_apply_selective_inhibition(
        bridge_, nullptr, TEST_INPUT_DIM);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmThalamicBridgeTest, ReleaseTrnInhibitionNullBridgeFails)
{
    nimcp_error_t result = omni_wm_thalamic_bridge_release_trn_inhibition(
        nullptr, WM_THAL_NUCLEUS_LGN);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmThalamicBridgeTest, ReleaseTrnInhibitionValidParams)
{
    // First apply inhibition
    omni_wm_thalamic_bridge_apply_trn_inhibition(bridge_, WM_THAL_NUCLEUS_LGN, 0.8f);

    // Then release
    nimcp_error_t result = omni_wm_thalamic_bridge_release_trn_inhibition(
        bridge_, WM_THAL_NUCLEUS_LGN);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmThalamicBridgeTest, ModulateTrnFromConfidenceNullBridgeFails)
{
    nimcp_error_t result = omni_wm_thalamic_bridge_modulate_trn_from_confidence(
        nullptr, 0.8f);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmThalamicBridgeTest, ModulateTrnFromConfidenceValidRange)
{
    EXPECT_EQ(omni_wm_thalamic_bridge_modulate_trn_from_confidence(bridge_, 0.0f), NIMCP_SUCCESS);
    EXPECT_EQ(omni_wm_thalamic_bridge_modulate_trn_from_confidence(bridge_, 0.5f), NIMCP_SUCCESS);
    EXPECT_EQ(omni_wm_thalamic_bridge_modulate_trn_from_confidence(bridge_, 1.0f), NIMCP_SUCCESS);
}

// =============================================================================
// 8. Prediction Integration Tests
// =============================================================================

TEST_F(OmniWmThalamicBridgeTest, PredictionErrorFeedbackNullBridgeFails)
{
    std::vector<float> errors = create_test_input(TEST_INPUT_DIM, 0.1f);

    nimcp_error_t result = omni_wm_thalamic_bridge_prediction_error_feedback(
        nullptr, errors.data(), TEST_INPUT_DIM, 0.15f);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmThalamicBridgeTest, PredictionErrorFeedbackNullErrorsFails)
{
    nimcp_error_t result = omni_wm_thalamic_bridge_prediction_error_feedback(
        bridge_, nullptr, TEST_INPUT_DIM, 0.15f);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmThalamicBridgeTest, UpdateFromGatedInputNullBridgeFails)
{
    std::vector<float> input = create_test_input(TEST_INPUT_DIM, 0.5f);

    nimcp_error_t result = omni_wm_thalamic_bridge_update_from_gated_input(
        nullptr, input.data(), TEST_INPUT_DIM);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmThalamicBridgeTest, UpdateFromGatedInputNullInputFails)
{
    nimcp_error_t result = omni_wm_thalamic_bridge_update_from_gated_input(
        bridge_, nullptr, TEST_INPUT_DIM);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

// =============================================================================
// 9. Query API Tests
// =============================================================================

TEST_F(OmniWmThalamicBridgeTest, GetThalamicEffectsNullReturnsNull)
{
    const thalamus_to_omni_wm_effects_t* effects =
        omni_wm_thalamic_bridge_get_thalamic_effects(nullptr);
    EXPECT_EQ(effects, nullptr);
}

TEST_F(OmniWmThalamicBridgeTest, GetThalamicEffectsValid)
{
    const thalamus_to_omni_wm_effects_t* effects =
        omni_wm_thalamic_bridge_get_thalamic_effects(bridge_);
    EXPECT_NE(effects, nullptr);
}

TEST_F(OmniWmThalamicBridgeTest, GetWmEffectsNullReturnsNull)
{
    const omni_wm_to_thalamus_effects_t* effects =
        omni_wm_thalamic_bridge_get_wm_effects(nullptr);
    EXPECT_EQ(effects, nullptr);
}

TEST_F(OmniWmThalamicBridgeTest, GetWmEffectsValid)
{
    const omni_wm_to_thalamus_effects_t* effects =
        omni_wm_thalamic_bridge_get_wm_effects(bridge_);
    EXPECT_NE(effects, nullptr);
}

// =============================================================================
// 10. Statistics Tests
// =============================================================================

TEST_F(OmniWmThalamicBridgeTest, GetStatsNullBridgeFails)
{
    omni_wm_thalamic_bridge_stats_t stats;
    nimcp_error_t result = omni_wm_thalamic_bridge_get_stats(nullptr, &stats);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmThalamicBridgeTest, GetStatsNullOutputFails)
{
    nimcp_error_t result = omni_wm_thalamic_bridge_get_stats(bridge_, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmThalamicBridgeTest, GetStatsValid)
{
    omni_wm_thalamic_bridge_stats_t stats;
    nimcp_error_t result = omni_wm_thalamic_bridge_get_stats(bridge_, &stats);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmThalamicBridgeTest, ResetStatsNullBridgeFails)
{
    nimcp_error_t result = omni_wm_thalamic_bridge_reset_stats(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmThalamicBridgeTest, ResetStatsValid)
{
    nimcp_error_t result = omni_wm_thalamic_bridge_reset_stats(bridge_);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmThalamicBridgeTest, StatsIncrementOnUpdate)
{
    omni_wm_thalamic_bridge_stats_t stats_before, stats_after;

    omni_wm_thalamic_bridge_get_stats(bridge_, &stats_before);

    for (int i = 0; i < 5; i++) {
        omni_wm_thalamic_bridge_update(bridge_, DEFAULT_DT);
    }

    omni_wm_thalamic_bridge_get_stats(bridge_, &stats_after);
    EXPECT_GT(stats_after.total_updates, stats_before.total_updates);
}

// =============================================================================
// 11. Bio-Async Tests
// =============================================================================

TEST_F(OmniWmThalamicBridgeTest, ConnectBioAsyncNullFails)
{
    nimcp_error_t result = omni_wm_thalamic_bridge_connect_bio_async(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmThalamicBridgeTest, DisconnectBioAsyncNullFails)
{
    nimcp_error_t result = omni_wm_thalamic_bridge_disconnect_bio_async(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmThalamicBridgeTest, IsBioAsyncConnectedNullReturnsFalse)
{
    EXPECT_FALSE(omni_wm_thalamic_bridge_is_bio_async_connected(nullptr));
}

TEST_F(OmniWmThalamicBridgeTest, IsBioAsyncConnectedInitially)
{
    EXPECT_FALSE(omni_wm_thalamic_bridge_is_bio_async_connected(bridge_));
}

// =============================================================================
// 12. Utility Function Tests
// =============================================================================

TEST_F(OmniWmThalamicBridgeTest, NucleusTypeToStringValid)
{
    for (int type = 0; type < WM_THAL_NUCLEUS_COUNT; type++) {
        const char* name = wm_thal_nucleus_type_to_string((wm_thal_nucleus_type_t)type);
        EXPECT_NE(name, nullptr);
        EXPECT_GT(strlen(name), 0u);
    }
}

TEST_F(OmniWmThalamicBridgeTest, NucleusTypeToStringInvalid)
{
    const char* name = wm_thal_nucleus_type_to_string((wm_thal_nucleus_type_t)999);
    EXPECT_NE(name, nullptr);  // Should return "Unknown" or similar
}

// =============================================================================
// 13. Edge Case Tests
// =============================================================================

TEST_F(OmniWmThalamicBridgeTest, MultipleResetCalls)
{
    for (int i = 0; i < 5; i++) {
        nimcp_error_t result = omni_wm_thalamic_bridge_reset(bridge_);
        EXPECT_EQ(result, NIMCP_SUCCESS);
    }
}

TEST_F(OmniWmThalamicBridgeTest, RapidAttentionChanges)
{
    for (int i = 0; i < 100; i++) {
        float attention = (float)i / 100.0f;
        omni_wm_thalamic_bridge_set_nucleus_attention(
            bridge_, WM_THAL_NUCLEUS_PULVINAR, attention);
    }
}

TEST_F(OmniWmThalamicBridgeTest, AllNucleiAttention)
{
    for (int nucleus = 0; nucleus < WM_THAL_NUCLEUS_COUNT; nucleus++) {
        nimcp_error_t result = omni_wm_thalamic_bridge_set_nucleus_attention(
            bridge_, (wm_thal_nucleus_type_t)nucleus, 0.5f);
        EXPECT_EQ(result, NIMCP_SUCCESS);

        float attention = omni_wm_thalamic_bridge_get_nucleus_attention(
            bridge_, (wm_thal_nucleus_type_t)nucleus);
        EXPECT_TRUE(float_in_range(attention, 0.0f, 1.0f));
    }
}

TEST_F(OmniWmThalamicBridgeTest, InhibitionThenRelease)
{
    // Apply inhibition to all nuclei
    for (int nucleus = 0; nucleus < WM_THAL_NUCLEUS_COUNT; nucleus++) {
        omni_wm_thalamic_bridge_apply_trn_inhibition(
            bridge_, (wm_thal_nucleus_type_t)nucleus, 0.8f);
    }

    // Release all
    for (int nucleus = 0; nucleus < WM_THAL_NUCLEUS_COUNT; nucleus++) {
        nimcp_error_t result = omni_wm_thalamic_bridge_release_trn_inhibition(
            bridge_, (wm_thal_nucleus_type_t)nucleus);
        EXPECT_EQ(result, NIMCP_SUCCESS);
    }
}

// =============================================================================
// 14. Memory Safety Tests
// =============================================================================

TEST_F(OmniWmThalamicBridgeTest, CreateDestroyMultiple)
{
    for (int i = 0; i < 10; i++) {
        omni_wm_thalamic_bridge_t* bridge = omni_wm_thalamic_bridge_create(nullptr);
        ASSERT_NE(bridge, nullptr);
        omni_wm_thalamic_bridge_destroy(bridge);
    }
}

TEST_F(OmniWmThalamicBridgeTest, UseAfterReset)
{
    omni_wm_thalamic_bridge_reset(bridge_);

    EXPECT_EQ(omni_wm_thalamic_bridge_set_arousal(bridge_, 0.5f), NIMCP_SUCCESS);
    EXPECT_EQ(omni_wm_thalamic_bridge_set_nucleus_attention(
        bridge_, WM_THAL_NUCLEUS_LGN, 0.7f), NIMCP_SUCCESS);
}

TEST_F(OmniWmThalamicBridgeTest, ConfigIntegrity)
{
    omni_wm_thalamic_bridge_config_t config;
    omni_wm_thalamic_bridge_default_config(&config);
    config.attention_baseline = 0.75f;
    config.trn_inhibition_strength = 0.4f;

    omni_wm_thalamic_bridge_t* bridge = omni_wm_thalamic_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);

    // Modify original
    config.attention_baseline = 0.25f;

    // Bridge should have original value
    EXPECT_FLOAT_EQ(bridge->config.attention_baseline, 0.75f);

    omni_wm_thalamic_bridge_destroy(bridge);
}
