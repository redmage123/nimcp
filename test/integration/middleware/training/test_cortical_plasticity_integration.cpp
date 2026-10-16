/**
 * @file test_cortical_plasticity_integration.cpp
 * @brief Cross-bridge integration tests: Cortical-Training → Training-Plasticity
 *
 * WHAT: Tests bidirectional integration between cortical and plasticity bridges
 * WHY:  Verify cortical state (burst rate, prediction error) modulates plasticity
 * HOW:  Create both bridges, connect them, verify plasticity modulation
 *
 * TEST COVERAGE:
 * - Connection lifecycle (4 tests)
 * - Plasticity factor computation (6 tests)
 * - Integration loop (5 tests)
 *
 * TOTAL: 15 tests
 *
 * @author NIMCP Development Team
 * @date 2025-12-20
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

// Headers have their own extern "C" guards
#include "middleware/training/nimcp_cortical_training_bridge.h"
#include "middleware/training/nimcp_training_plasticity_bridge.h"
#include "utils/error/nimcp_error_codes.h"

class CorticalPlasticityIntegrationTest : public ::testing::Test {
protected:
    cortical_training_bridge_t* cortical_bridge;
    tpb_context_t* plasticity_ctx;
    cortical_training_config_t cortical_config;
    tpb_config_t plasticity_config;

    void SetUp() override {
        cortical_training_default_config(&cortical_config);
        cortical_config.enable_bio_async = false;
        cortical_bridge = cortical_training_create(&cortical_config);
        ASSERT_NE(cortical_bridge, nullptr);

        plasticity_config = tpb_config_default();
        plasticity_ctx = tpb_create(&plasticity_config);
        ASSERT_NE(plasticity_ctx, nullptr);
    }

    void TearDown() override {
        if (plasticity_ctx) {
            tpb_destroy(plasticity_ctx);
            plasticity_ctx = nullptr;
        }
        if (cortical_bridge) {
            cortical_training_destroy(cortical_bridge);
            cortical_bridge = nullptr;
        }
    }
};

//=============================================================================
// Connection Lifecycle (4 tests)
//=============================================================================

TEST_F(CorticalPlasticityIntegrationTest, ConnectCorticalToPlasticity) {
    EXPECT_EQ(cortical_training_start(cortical_bridge), 0);

    EXPECT_EQ(tpb_connect_cortical_training(plasticity_ctx, cortical_bridge), NIMCP_SUCCESS);
    EXPECT_TRUE(tpb_is_cortical_training_connected(plasticity_ctx));
}

TEST_F(CorticalPlasticityIntegrationTest, DisconnectCorticalFromPlasticity) {
    EXPECT_EQ(cortical_training_start(cortical_bridge), 0);

    EXPECT_EQ(tpb_connect_cortical_training(plasticity_ctx, cortical_bridge), NIMCP_SUCCESS);
    EXPECT_TRUE(tpb_is_cortical_training_connected(plasticity_ctx));

    EXPECT_EQ(tpb_connect_cortical_training(plasticity_ctx, nullptr), NIMCP_SUCCESS);
    EXPECT_FALSE(tpb_is_cortical_training_connected(plasticity_ctx));
}

TEST_F(CorticalPlasticityIntegrationTest, ConnectNullPlasticityReturnsError) {
    EXPECT_NE(tpb_connect_cortical_training(nullptr, cortical_bridge), NIMCP_SUCCESS);
}

TEST_F(CorticalPlasticityIntegrationTest, ReconnectCorticalBridge) {
    EXPECT_EQ(cortical_training_start(cortical_bridge), 0);

    EXPECT_EQ(tpb_connect_cortical_training(plasticity_ctx, cortical_bridge), NIMCP_SUCCESS);
    EXPECT_EQ(tpb_connect_cortical_training(plasticity_ctx, nullptr), NIMCP_SUCCESS);
    EXPECT_EQ(tpb_connect_cortical_training(plasticity_ctx, cortical_bridge), NIMCP_SUCCESS);
    EXPECT_TRUE(tpb_is_cortical_training_connected(plasticity_ctx));
}

//=============================================================================
// Plasticity Factor Computation (6 tests)
//=============================================================================

TEST_F(CorticalPlasticityIntegrationTest, NoConnectionReturnsDefaultFactor) {
    /* No cortical connected → factor = 1.0 (no modulation) */
    float factor = tpb_get_cortical_plasticity_factor(plasticity_ctx);
    EXPECT_FLOAT_EQ(factor, 1.0f);
}

TEST_F(CorticalPlasticityIntegrationTest, HighBurstRateStableBoostsPlasticity) {
    EXPECT_EQ(cortical_training_start(cortical_bridge), 0);
    EXPECT_EQ(tpb_connect_cortical_training(plasticity_ctx, cortical_bridge), NIMCP_SUCCESS);

    /* High burst rate + predictions stable → consolidation boost */
    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.burst_rate = 0.9f;
    effects.predictions_stable = true;
    effects.prediction_error_mag = 0.1f;
    effects.valid = true;
    EXPECT_EQ(cortical_training_set_effects_for_testing(cortical_bridge, &effects), 0);

    float factor = tpb_get_cortical_plasticity_factor(plasticity_ctx);
    /* base = 0.5 + 0.5 × 0.9 = 0.95, factor = 0.95 × 1.2 = 1.14 */
    EXPECT_GT(factor, 1.0f);
    EXPECT_LE(factor, 1.5f);
}

TEST_F(CorticalPlasticityIntegrationTest, LowBurstRateReducesPlasticity) {
    EXPECT_EQ(cortical_training_start(cortical_bridge), 0);
    EXPECT_EQ(tpb_connect_cortical_training(plasticity_ctx, cortical_bridge), NIMCP_SUCCESS);

    /* Low burst rate → reduced plasticity */
    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.burst_rate = 0.1f;
    effects.predictions_stable = false;
    effects.prediction_error_mag = 0.5f;
    effects.valid = true;
    EXPECT_EQ(cortical_training_set_effects_for_testing(cortical_bridge, &effects), 0);

    float factor = tpb_get_cortical_plasticity_factor(plasticity_ctx);
    /* base = 0.5 + 0.5 × 0.1 = 0.55, factor = 0.55 × 1.15 = 0.63 */
    EXPECT_LT(factor, 1.0f);
    EXPECT_GE(factor, 0.25f);
}

TEST_F(CorticalPlasticityIntegrationTest, HighPredictionErrorBoostsWhenUnstable) {
    EXPECT_EQ(cortical_training_start(cortical_bridge), 0);
    EXPECT_EQ(tpb_connect_cortical_training(plasticity_ctx, cortical_bridge), NIMCP_SUCCESS);

    /* Unstable + high prediction error → error-driven boost */
    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.burst_rate = 0.6f;
    effects.predictions_stable = false;
    effects.prediction_error_mag = 1.0f;  /* High error */
    effects.valid = true;
    EXPECT_EQ(cortical_training_set_effects_for_testing(cortical_bridge, &effects), 0);

    float factor = tpb_get_cortical_plasticity_factor(plasticity_ctx);
    /* base = 0.5 + 0.5 × 0.6 = 0.8, factor = 0.8 × (1.0 + 0.3 × 1.0) = 0.8 × 1.3 = 1.04 */
    EXPECT_GT(factor, 0.9f);
}

TEST_F(CorticalPlasticityIntegrationTest, DisconnectedReturnsDefaultFactor) {
    EXPECT_EQ(cortical_training_start(cortical_bridge), 0);
    EXPECT_EQ(tpb_connect_cortical_training(plasticity_ctx, cortical_bridge), NIMCP_SUCCESS);

    /* Set valid high effects */
    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.burst_rate = 0.9f;
    effects.predictions_stable = true;
    effects.valid = true;
    EXPECT_EQ(cortical_training_set_effects_for_testing(cortical_bridge, &effects), 0);

    float factor_connected = tpb_get_cortical_plasticity_factor(plasticity_ctx);
    EXPECT_GT(factor_connected, 1.0f);

    /* Disconnect → default factor = 1.0 */
    EXPECT_EQ(tpb_connect_cortical_training(plasticity_ctx, nullptr), NIMCP_SUCCESS);
    float factor = tpb_get_cortical_plasticity_factor(plasticity_ctx);
    EXPECT_FLOAT_EQ(factor, 1.0f);
}

TEST_F(CorticalPlasticityIntegrationTest, PlasticityFactorClampedToBounds) {
    EXPECT_EQ(cortical_training_start(cortical_bridge), 0);
    EXPECT_EQ(tpb_connect_cortical_training(plasticity_ctx, cortical_bridge), NIMCP_SUCCESS);

    /* Very low burst rate → clamped to 0.25 */
    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.burst_rate = 0.0f;
    effects.predictions_stable = false;
    effects.prediction_error_mag = 0.0f;
    effects.valid = true;
    EXPECT_EQ(cortical_training_set_effects_for_testing(cortical_bridge, &effects), 0);

    float factor = tpb_get_cortical_plasticity_factor(plasticity_ctx);
    EXPECT_GE(factor, 0.25f);
    EXPECT_LE(factor, 1.5f);
}

//=============================================================================
// Integration Loop (5 tests)
//=============================================================================

TEST_F(CorticalPlasticityIntegrationTest, PlasticityFactorEvolvesWithCortical) {
    EXPECT_EQ(cortical_training_start(cortical_bridge), 0);
    EXPECT_EQ(tpb_connect_cortical_training(plasticity_ctx, cortical_bridge), NIMCP_SUCCESS);

    /* Simulate improving cortical state over training steps */
    for (int step = 0; step < 20; ++step) {
        cortical_training_effects_t effects;
        memset(&effects, 0, sizeof(effects));
        effects.burst_rate = 0.2f + 0.7f * (step / 20.0f);
        effects.predictions_stable = (step > 10);
        effects.prediction_error_mag = 0.8f - 0.7f * (step / 20.0f);
        effects.valid = true;
        EXPECT_EQ(cortical_training_set_effects_for_testing(cortical_bridge, &effects), 0);

        float factor = tpb_get_cortical_plasticity_factor(plasticity_ctx);
        EXPECT_GE(factor, 0.25f);
        EXPECT_LE(factor, 1.5f);
    }
}

TEST_F(CorticalPlasticityIntegrationTest, CombinedFactorWithCorticalOnly) {
    EXPECT_EQ(cortical_training_start(cortical_bridge), 0);
    EXPECT_EQ(tpb_connect_cortical_training(plasticity_ctx, cortical_bridge), NIMCP_SUCCESS);

    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.burst_rate = 0.8f;
    effects.predictions_stable = true;
    effects.valid = true;
    EXPECT_EQ(cortical_training_set_effects_for_testing(cortical_bridge, &effects), 0);

    float combined = tpb_get_combined_plasticity_factor(plasticity_ctx);
    /* With only cortical connected, combined = sqrt(1.0 × cortical) */
    float cortical = tpb_get_cortical_plasticity_factor(plasticity_ctx);
    EXPECT_FLOAT_EQ(combined, sqrtf(1.0f * cortical));
}

TEST_F(CorticalPlasticityIntegrationTest, TransitionFromUnstableToStable) {
    EXPECT_EQ(cortical_training_start(cortical_bridge), 0);
    EXPECT_EQ(tpb_connect_cortical_training(plasticity_ctx, cortical_bridge), NIMCP_SUCCESS);

    /* First: unstable */
    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.burst_rate = 0.6f;
    effects.predictions_stable = false;
    effects.prediction_error_mag = 0.5f;
    effects.valid = true;
    EXPECT_EQ(cortical_training_set_effects_for_testing(cortical_bridge, &effects), 0);

    float factor_unstable = tpb_get_cortical_plasticity_factor(plasticity_ctx);

    /* Second: stable (should get consolidation boost) */
    effects.predictions_stable = true;
    effects.prediction_error_mag = 0.1f;
    EXPECT_EQ(cortical_training_set_effects_for_testing(cortical_bridge, &effects), 0);

    float factor_stable = tpb_get_cortical_plasticity_factor(plasticity_ctx);
    /* Stable gets 1.2× boost vs unstable's error-based */
    EXPECT_GT(factor_stable, factor_unstable * 0.9f);
}

TEST_F(CorticalPlasticityIntegrationTest, FreeEnergyDoesNotAffectPlasticity) {
    EXPECT_EQ(cortical_training_start(cortical_bridge), 0);
    EXPECT_EQ(tpb_connect_cortical_training(plasticity_ctx, cortical_bridge), NIMCP_SUCCESS);

    /* Free energy is used for immune, not plasticity modulation */
    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.burst_rate = 0.7f;
    effects.predictions_stable = true;
    effects.free_energy = 50.0f;  /* Should not affect factor */
    effects.valid = true;
    EXPECT_EQ(cortical_training_set_effects_for_testing(cortical_bridge, &effects), 0);

    float factor1 = tpb_get_cortical_plasticity_factor(plasticity_ctx);

    effects.free_energy = 5.0f;
    EXPECT_EQ(cortical_training_set_effects_for_testing(cortical_bridge, &effects), 0);

    float factor2 = tpb_get_cortical_plasticity_factor(plasticity_ctx);
    EXPECT_FLOAT_EQ(factor1, factor2);  /* Same factor */
}

TEST_F(CorticalPlasticityIntegrationTest, NullContextReturnsDefault) {
    EXPECT_FLOAT_EQ(tpb_get_cortical_plasticity_factor(nullptr), 1.0f);
    EXPECT_FALSE(tpb_is_cortical_training_connected(nullptr));
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
