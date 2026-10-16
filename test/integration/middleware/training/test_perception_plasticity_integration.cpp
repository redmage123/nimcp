/**
 * @file test_perception_plasticity_integration.cpp
 * @brief Cross-bridge integration tests: Perception-Training → Training-Plasticity
 *
 * WHAT: Tests bidirectional integration between perception and plasticity bridges
 * WHY:  Verify perception state correctly modulates plasticity factors
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
#include "middleware/training/nimcp_perception_training_bridge.h"
#include "middleware/training/nimcp_training_plasticity_bridge.h"
#include "utils/error/nimcp_error_codes.h"

class PerceptionPlasticityIntegrationTest : public ::testing::Test {
protected:
    perception_training_bridge_t* perception_bridge;
    tpb_context_t* plasticity_ctx;
    perception_training_config_t perception_config;
    tpb_config_t plasticity_config;

    void SetUp() override {
        perception_training_default_config(&perception_config);
        perception_config.enable_bio_async = false;
        perception_bridge = perception_training_create(&perception_config);
        ASSERT_NE(perception_bridge, nullptr);

        plasticity_config = tpb_config_default();
        plasticity_ctx = tpb_create(&plasticity_config);
        ASSERT_NE(plasticity_ctx, nullptr);
    }

    void TearDown() override {
        if (plasticity_ctx) {
            tpb_destroy(plasticity_ctx);
            plasticity_ctx = nullptr;
        }
        if (perception_bridge) {
            perception_training_destroy(perception_bridge);
            perception_bridge = nullptr;
        }
    }
};

//=============================================================================
// Connection Lifecycle (4 tests)
//=============================================================================

TEST_F(PerceptionPlasticityIntegrationTest, ConnectPerceptionToPlasticity) {
    EXPECT_EQ(perception_training_start(perception_bridge), 0);

    EXPECT_EQ(tpb_connect_perception_training(plasticity_ctx, perception_bridge), NIMCP_SUCCESS);
    EXPECT_TRUE(tpb_is_perception_training_connected(plasticity_ctx));
}

TEST_F(PerceptionPlasticityIntegrationTest, DisconnectPerceptionFromPlasticity) {
    EXPECT_EQ(perception_training_start(perception_bridge), 0);

    EXPECT_EQ(tpb_connect_perception_training(plasticity_ctx, perception_bridge), NIMCP_SUCCESS);
    EXPECT_TRUE(tpb_is_perception_training_connected(plasticity_ctx));

    EXPECT_EQ(tpb_connect_perception_training(plasticity_ctx, nullptr), NIMCP_SUCCESS);
    EXPECT_FALSE(tpb_is_perception_training_connected(plasticity_ctx));
}

TEST_F(PerceptionPlasticityIntegrationTest, ConnectNullPlasticityReturnsError) {
    EXPECT_NE(tpb_connect_perception_training(nullptr, perception_bridge), NIMCP_SUCCESS);
}

TEST_F(PerceptionPlasticityIntegrationTest, ReconnectPerceptionBridge) {
    EXPECT_EQ(perception_training_start(perception_bridge), 0);

    EXPECT_EQ(tpb_connect_perception_training(plasticity_ctx, perception_bridge), NIMCP_SUCCESS);
    EXPECT_EQ(tpb_connect_perception_training(plasticity_ctx, nullptr), NIMCP_SUCCESS);
    EXPECT_EQ(tpb_connect_perception_training(plasticity_ctx, perception_bridge), NIMCP_SUCCESS);
    EXPECT_TRUE(tpb_is_perception_training_connected(plasticity_ctx));
}

//=============================================================================
// Plasticity Factor Computation (6 tests)
//=============================================================================

TEST_F(PerceptionPlasticityIntegrationTest, NoConnectionReturnsDefaultFactor) {
    /* No perception connected → factor = 1.0 (no modulation) */
    float factor = tpb_get_perception_plasticity_factor(plasticity_ctx);
    EXPECT_FLOAT_EQ(factor, 1.0f);
}

TEST_F(PerceptionPlasticityIntegrationTest, HighConfidenceHighLRFactorBoostsPlasticity) {
    EXPECT_EQ(perception_training_start(perception_bridge), 0);
    EXPECT_EQ(tpb_connect_perception_training(plasticity_ctx, perception_bridge), NIMCP_SUCCESS);

    /* High visual confidence + high LR factor → enhanced plasticity */
    perception_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.visual_confidence = 0.9f;
    effects.lr_factor = 1.2f;
    effects.valid = true;
    EXPECT_EQ(perception_training_set_effects_for_testing(perception_bridge, &effects), 0);

    float factor = tpb_get_perception_plasticity_factor(plasticity_ctx);
    /* factor = 1.2 × (0.5 + 0.5 × 0.9) = 1.2 × 0.95 = 1.14, clamped to 1.14 */
    EXPECT_GT(factor, 1.0f);
    EXPECT_LE(factor, 1.5f);
}

TEST_F(PerceptionPlasticityIntegrationTest, LowConfidenceLowLRFactorReducesPlasticity) {
    EXPECT_EQ(perception_training_start(perception_bridge), 0);
    EXPECT_EQ(tpb_connect_perception_training(plasticity_ctx, perception_bridge), NIMCP_SUCCESS);

    /* Low visual confidence + low LR factor → reduced plasticity */
    perception_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.visual_confidence = 0.1f;
    effects.lr_factor = 0.4f;
    effects.valid = true;
    EXPECT_EQ(perception_training_set_effects_for_testing(perception_bridge, &effects), 0);

    float factor = tpb_get_perception_plasticity_factor(plasticity_ctx);
    /* factor = 0.4 × (0.5 + 0.5 × 0.1) = 0.4 × 0.55 = 0.22, clamped to 0.25 */
    EXPECT_GE(factor, 0.25f);
    EXPECT_LT(factor, 1.0f);
}

TEST_F(PerceptionPlasticityIntegrationTest, DisconnectedReturnsDefaultFactor) {
    EXPECT_EQ(perception_training_start(perception_bridge), 0);
    EXPECT_EQ(tpb_connect_perception_training(plasticity_ctx, perception_bridge), NIMCP_SUCCESS);

    /* Set valid high effects */
    perception_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.visual_confidence = 0.9f;
    effects.lr_factor = 1.2f;
    effects.valid = true;
    EXPECT_EQ(perception_training_set_effects_for_testing(perception_bridge, &effects), 0);

    float factor_connected = tpb_get_perception_plasticity_factor(plasticity_ctx);
    EXPECT_GT(factor_connected, 1.0f);

    /* Disconnect → default factor = 1.0 */
    EXPECT_EQ(tpb_connect_perception_training(plasticity_ctx, nullptr), NIMCP_SUCCESS);
    float factor = tpb_get_perception_plasticity_factor(plasticity_ctx);
    EXPECT_FLOAT_EQ(factor, 1.0f);
}

TEST_F(PerceptionPlasticityIntegrationTest, PlasticityFactorClampedToUpperBound) {
    EXPECT_EQ(perception_training_start(perception_bridge), 0);
    EXPECT_EQ(tpb_connect_perception_training(plasticity_ctx, perception_bridge), NIMCP_SUCCESS);

    /* Very high values → clamped to 1.5 */
    perception_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.visual_confidence = 1.0f;
    effects.lr_factor = 2.0f;  /* Would give 2.0 × 1.0 = 2.0 */
    effects.valid = true;
    EXPECT_EQ(perception_training_set_effects_for_testing(perception_bridge, &effects), 0);

    float factor = tpb_get_perception_plasticity_factor(plasticity_ctx);
    EXPECT_FLOAT_EQ(factor, 1.5f);  /* Clamped */
}

TEST_F(PerceptionPlasticityIntegrationTest, PlasticityFactorClampedToLowerBound) {
    EXPECT_EQ(perception_training_start(perception_bridge), 0);
    EXPECT_EQ(tpb_connect_perception_training(plasticity_ctx, perception_bridge), NIMCP_SUCCESS);

    /* Very low values → clamped to 0.25 */
    perception_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.visual_confidence = 0.0f;
    effects.lr_factor = 0.1f;  /* Would give 0.1 × 0.5 = 0.05 */
    effects.valid = true;
    EXPECT_EQ(perception_training_set_effects_for_testing(perception_bridge, &effects), 0);

    float factor = tpb_get_perception_plasticity_factor(plasticity_ctx);
    EXPECT_FLOAT_EQ(factor, 0.25f);  /* Clamped */
}

//=============================================================================
// Integration Loop (5 tests)
//=============================================================================

TEST_F(PerceptionPlasticityIntegrationTest, PlasticityFactorEvolvesWithPerception) {
    EXPECT_EQ(perception_training_start(perception_bridge), 0);
    EXPECT_EQ(tpb_connect_perception_training(plasticity_ctx, perception_bridge), NIMCP_SUCCESS);

    /* Simulate improving perception over training steps */
    for (int step = 0; step < 20; ++step) {
        perception_training_effects_t effects;
        memset(&effects, 0, sizeof(effects));
        effects.visual_confidence = 0.3f + 0.6f * (step / 20.0f);
        effects.lr_factor = 0.5f + 0.5f * (step / 20.0f);
        effects.valid = true;
        EXPECT_EQ(perception_training_set_effects_for_testing(perception_bridge, &effects), 0);

        float factor = tpb_get_perception_plasticity_factor(plasticity_ctx);
        EXPECT_GE(factor, 0.25f);
        EXPECT_LE(factor, 1.5f);

        /* Factor should generally increase as perception improves */
        if (step > 10) {
            EXPECT_GT(factor, 0.5f);
        }
    }
}

TEST_F(PerceptionPlasticityIntegrationTest, DisconnectDuringLoopResetsFactor) {
    EXPECT_EQ(perception_training_start(perception_bridge), 0);
    EXPECT_EQ(tpb_connect_perception_training(plasticity_ctx, perception_bridge), NIMCP_SUCCESS);

    /* Set high perception */
    perception_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.visual_confidence = 0.9f;
    effects.lr_factor = 1.0f;
    effects.valid = true;
    EXPECT_EQ(perception_training_set_effects_for_testing(perception_bridge, &effects), 0);

    float factor_before = tpb_get_perception_plasticity_factor(plasticity_ctx);
    EXPECT_GT(factor_before, 0.5f);

    /* Disconnect */
    EXPECT_EQ(tpb_connect_perception_training(plasticity_ctx, nullptr), NIMCP_SUCCESS);

    float factor_after = tpb_get_perception_plasticity_factor(plasticity_ctx);
    EXPECT_FLOAT_EQ(factor_after, 1.0f);  /* Default after disconnect */
}

TEST_F(PerceptionPlasticityIntegrationTest, CombinedFactorWithPerceptionOnly) {
    EXPECT_EQ(perception_training_start(perception_bridge), 0);
    EXPECT_EQ(tpb_connect_perception_training(plasticity_ctx, perception_bridge), NIMCP_SUCCESS);

    perception_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.visual_confidence = 0.8f;
    effects.lr_factor = 1.0f;
    effects.valid = true;
    EXPECT_EQ(perception_training_set_effects_for_testing(perception_bridge, &effects), 0);

    float combined = tpb_get_combined_plasticity_factor(plasticity_ctx);
    /* With only perception connected, combined = sqrt(perception × 1.0) */
    float perception = tpb_get_perception_plasticity_factor(plasticity_ctx);
    EXPECT_FLOAT_EQ(combined, sqrtf(perception * 1.0f));
}

TEST_F(PerceptionPlasticityIntegrationTest, MultipleEffectUpdates) {
    EXPECT_EQ(perception_training_start(perception_bridge), 0);
    EXPECT_EQ(tpb_connect_perception_training(plasticity_ctx, perception_bridge), NIMCP_SUCCESS);

    /* First: low quality */
    perception_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.visual_confidence = 0.2f;
    effects.lr_factor = 0.5f;
    effects.valid = true;
    EXPECT_EQ(perception_training_set_effects_for_testing(perception_bridge, &effects), 0);

    float factor1 = tpb_get_perception_plasticity_factor(plasticity_ctx);
    EXPECT_LT(factor1, 1.0f);

    /* Second: high quality */
    effects.visual_confidence = 0.9f;
    effects.lr_factor = 1.2f;
    EXPECT_EQ(perception_training_set_effects_for_testing(perception_bridge, &effects), 0);

    float factor2 = tpb_get_perception_plasticity_factor(plasticity_ctx);
    EXPECT_GT(factor2, factor1);
}

TEST_F(PerceptionPlasticityIntegrationTest, NullContextReturnsDefault) {
    EXPECT_FLOAT_EQ(tpb_get_perception_plasticity_factor(nullptr), 1.0f);
    EXPECT_FLOAT_EQ(tpb_get_combined_plasticity_factor(nullptr), 1.0f);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
