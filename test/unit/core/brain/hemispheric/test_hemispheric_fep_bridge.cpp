//=============================================================================
// test_hemispheric_fep_bridge.cpp - Hemispheric FEP Bridge Unit Tests
//=============================================================================
/**
 * @file test_hemispheric_fep_bridge.cpp
 * @brief Unit tests for hemispheric-FEP bidirectional integration
 *
 * Tests:
 * - Lifecycle (creation, destruction)
 * - Precision asymmetry between hemispheres
 * - Free energy computation
 * - Learning rate modulation
 * - Cross-hemisphere prediction transfer
 * - Bio-async integration
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "core/brain/hemispheric/nimcp_hemispheric_fep_bridge.h"
#include "core/brain/hemispheric/nimcp_hemispheric_brain.h"

//=============================================================================
// Test Fixture
//=============================================================================

class HemisphericFepBridgeTest : public ::testing::Test {
protected:
    // Shared brain instance - created once for all tests (very expensive to create)
    static hemispheric_brain_t* shared_brain;

    hemispheric_fep_bridge_t* bridge = nullptr;
    hemispheric_brain_t* brain = nullptr;

    static void SetUpTestSuite() {
        hemispheric_brain_config_t brain_config = hemispheric_brain_default_config();
        brain_config.size = BRAIN_SIZE_TINY;
        brain_config.num_inputs = 8;
        brain_config.num_outputs = 4;
        brain_config.initial_tier = PLATFORM_TIER_CONSTRAINED;
        brain_config.enable_bio_async = false;
        brain_config.left_config.size = BRAIN_SIZE_TINY;
        brain_config.left_config.initial_tier = PLATFORM_TIER_CONSTRAINED;
        brain_config.left_config.enable_bio_async = false;
        brain_config.right_config.size = BRAIN_SIZE_TINY;
        brain_config.right_config.initial_tier = PLATFORM_TIER_CONSTRAINED;
        brain_config.right_config.enable_bio_async = false;
        shared_brain = hemispheric_brain_create(&brain_config);
    }

    static void TearDownTestSuite() {
        if (shared_brain) {
            hemispheric_brain_destroy(shared_brain);
            shared_brain = nullptr;
        }
    }

    void SetUp() override {
        brain = shared_brain;
        ASSERT_NE(brain, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            hemispheric_fep_destroy(bridge);
            bridge = nullptr;
        }
    }
};

hemispheric_brain_t* HemisphericFepBridgeTest::shared_brain = nullptr;

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(HemisphericFepBridgeTest, DefaultConfigValues) {
    hemispheric_fep_config_t config = hemispheric_fep_default_config();

    EXPECT_FLOAT_EQ(config.left_base_precision, HEMI_FEP_LEFT_BASE_PRECISION);
    EXPECT_FLOAT_EQ(config.right_base_precision, HEMI_FEP_RIGHT_BASE_PRECISION);
    EXPECT_FLOAT_EQ(config.left_prior_width, HEMI_FEP_LEFT_PRIOR_WIDTH);
    EXPECT_FLOAT_EQ(config.right_prior_width, HEMI_FEP_RIGHT_PRIOR_WIDTH);
    EXPECT_TRUE(config.enable_precision_modulation);
    EXPECT_TRUE(config.enable_learning_modulation);
    EXPECT_TRUE(config.enable_callosum_transfer);
    EXPECT_TRUE(config.enable_bio_async);
}

TEST_F(HemisphericFepBridgeTest, CreateWithDefaultConfig) {
    hemispheric_fep_config_t config = hemispheric_fep_default_config();
    bridge = hemispheric_fep_create(&config, brain, nullptr);

    ASSERT_NE(bridge, nullptr);
}

TEST_F(HemisphericFepBridgeTest, CreateWithNullConfig) {
    bridge = hemispheric_fep_create(nullptr, brain, nullptr);

    ASSERT_NE(bridge, nullptr);
}

TEST_F(HemisphericFepBridgeTest, CreateWithNullBrain) {
    hemispheric_fep_config_t config = hemispheric_fep_default_config();
    bridge = hemispheric_fep_create(&config, nullptr, nullptr);

    EXPECT_EQ(bridge, nullptr);
}

TEST_F(HemisphericFepBridgeTest, DestroyNullBridge) {
    // Should not crash
    hemispheric_fep_destroy(nullptr);
}

//=============================================================================
// Precision Asymmetry Tests
//=============================================================================

TEST_F(HemisphericFepBridgeTest, LeftHasHigherBasePrecision) {
    bridge = hemispheric_fep_create(nullptr, brain, nullptr);
    ASSERT_NE(bridge, nullptr);

    float left_precision = hemispheric_fep_get_precision(bridge, HEMISPHERE_LEFT);
    float right_precision = hemispheric_fep_get_precision(bridge, HEMISPHERE_RIGHT);

    // Left hemisphere should have higher precision (analytical)
    EXPECT_GT(left_precision, right_precision);
}

TEST_F(HemisphericFepBridgeTest, SetPrecisionValid) {
    bridge = hemispheric_fep_create(nullptr, brain, nullptr);
    ASSERT_NE(bridge, nullptr);

    int result = hemispheric_fep_set_precision(bridge, HEMISPHERE_LEFT, 0.75f);
    EXPECT_EQ(result, 0);

    float precision = hemispheric_fep_get_precision(bridge, HEMISPHERE_LEFT);
    EXPECT_FLOAT_EQ(precision, 0.75f);
}

TEST_F(HemisphericFepBridgeTest, SetPrecisionOutOfRange) {
    bridge = hemispheric_fep_create(nullptr, brain, nullptr);
    ASSERT_NE(bridge, nullptr);

    int result = hemispheric_fep_set_precision(bridge, HEMISPHERE_LEFT, 1.5f);
    EXPECT_NE(result, NIMCP_SUCCESS);

    result = hemispheric_fep_set_precision(bridge, HEMISPHERE_RIGHT, -0.1f);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

//=============================================================================
// Free Energy Tests
//=============================================================================

TEST_F(HemisphericFepBridgeTest, InitialFreeEnergyLow) {
    bridge = hemispheric_fep_create(nullptr, brain, nullptr);
    ASSERT_NE(bridge, nullptr);

    float total_fe = hemispheric_fep_get_total_free_energy(bridge);

    // Initial free energy should be low (no prediction errors)
    EXPECT_LT(total_fe, 1.0f);
}

TEST_F(HemisphericFepBridgeTest, PredictionErrorIncreasesFreeEnergy) {
    bridge = hemispheric_fep_create(nullptr, brain, nullptr);
    ASSERT_NE(bridge, nullptr);

    float initial_fe = hemispheric_fep_get_total_free_energy(bridge);

    // Inject prediction error
    hemispheric_fep_inject_prediction_error(bridge, HEMISPHERE_LEFT, 0.8f);

    float after_fe = hemispheric_fep_get_total_free_energy(bridge);

    // Free energy should increase with prediction error
    EXPECT_GT(after_fe, initial_fe);
}

TEST_F(HemisphericFepBridgeTest, ResetFreeEnergy) {
    bridge = hemispheric_fep_create(nullptr, brain, nullptr);
    ASSERT_NE(bridge, nullptr);

    // Inject error to raise FE
    hemispheric_fep_inject_prediction_error(bridge, HEMISPHERE_LEFT, 1.0f);
    hemispheric_fep_inject_prediction_error(bridge, HEMISPHERE_RIGHT, 1.0f);

    // Reset
    int result = hemispheric_fep_reset_free_energy(bridge);
    EXPECT_EQ(result, 0);

    float fe = hemispheric_fep_get_total_free_energy(bridge);
    EXPECT_LT(fe, 0.5f);
}

TEST_F(HemisphericFepBridgeTest, FreeEnergyMinimizationReducesFE) {
    bridge = hemispheric_fep_create(nullptr, brain, nullptr);
    ASSERT_NE(bridge, nullptr);

    // Inject prediction errors
    hemispheric_fep_inject_prediction_error(bridge, HEMISPHERE_LEFT, 0.5f);
    hemispheric_fep_inject_prediction_error(bridge, HEMISPHERE_RIGHT, 0.5f);

    float before_fe = hemispheric_fep_get_total_free_energy(bridge);

    // Run several minimization steps
    for (int i = 0; i < 10; i++) {
        hemispheric_fep_minimize_step(bridge, 0.1f);
    }

    float after_fe = hemispheric_fep_get_total_free_energy(bridge);

    // Free energy should decrease (minimization)
    EXPECT_LT(after_fe, before_fe);
}

//=============================================================================
// Learning Rate Modulation Tests
//=============================================================================

TEST_F(HemisphericFepBridgeTest, LearningRateIncreasesWithError) {
    bridge = hemispheric_fep_create(nullptr, brain, nullptr);
    ASSERT_NE(bridge, nullptr);

    hemisphere_fep_effects_t initial = hemispheric_fep_get_left_effects(bridge);
    float initial_lr = initial.learning_rate_factor;

    // Inject high prediction error
    hemispheric_fep_inject_prediction_error(bridge, HEMISPHERE_LEFT, 0.9f);

    hemisphere_fep_effects_t after = hemispheric_fep_get_left_effects(bridge);

    // Learning rate should increase with error (learn more when wrong)
    EXPECT_GT(after.learning_rate_factor, initial_lr);
}

TEST_F(HemisphericFepBridgeTest, LearningRateBounded) {
    bridge = hemispheric_fep_create(nullptr, brain, nullptr);
    ASSERT_NE(bridge, nullptr);

    // Inject extreme prediction error
    hemispheric_fep_inject_prediction_error(bridge, HEMISPHERE_LEFT, 100.0f);

    hemisphere_fep_effects_t effects = hemispheric_fep_get_left_effects(bridge);

    // Should be within configured bounds
    EXPECT_LE(effects.learning_rate_factor, HEMI_FEP_MAX_LEARNING_RATE);
    EXPECT_GE(effects.learning_rate_factor, HEMI_FEP_MIN_LEARNING_RATE);
}

//=============================================================================
// Callosum Transfer Tests
//=============================================================================

TEST_F(HemisphericFepBridgeTest, TransferAveragesPredictionErrors) {
    bridge = hemispheric_fep_create(nullptr, brain, nullptr);
    ASSERT_NE(bridge, nullptr);

    // Set asymmetric errors
    hemispheric_fep_inject_prediction_error(bridge, HEMISPHERE_LEFT, 0.8f);
    hemispheric_fep_inject_prediction_error(bridge, HEMISPHERE_RIGHT, 0.2f);

    hemisphere_fep_effects_t left_before = hemispheric_fep_get_left_effects(bridge);
    hemisphere_fep_effects_t right_before = hemispheric_fep_get_right_effects(bridge);

    // Trigger transfer
    int result = hemispheric_fep_trigger_transfer(bridge);
    EXPECT_EQ(result, 0);

    hemisphere_fep_effects_t left_after = hemispheric_fep_get_left_effects(bridge);
    hemisphere_fep_effects_t right_after = hemispheric_fep_get_right_effects(bridge);

    // Errors should converge
    EXPECT_NEAR(left_after.prediction_error, right_after.prediction_error, 0.01f);
}

TEST_F(HemisphericFepBridgeTest, TransferIncrementsStats) {
    bridge = hemispheric_fep_create(nullptr, brain, nullptr);
    ASSERT_NE(bridge, nullptr);

    hemispheric_fep_stats_t initial = hemispheric_fep_get_stats(bridge);
    EXPECT_EQ(initial.prediction_transfers, 0u);

    hemispheric_fep_trigger_transfer(bridge);
    hemispheric_fep_trigger_transfer(bridge);

    hemispheric_fep_stats_t after = hemispheric_fep_get_stats(bridge);
    EXPECT_EQ(after.prediction_transfers, 2u);
}

TEST_F(HemisphericFepBridgeTest, CallosumEffectsShowTransfer) {
    bridge = hemispheric_fep_create(nullptr, brain, nullptr);
    ASSERT_NE(bridge, nullptr);

    callosum_fep_effects_t effects = hemispheric_fep_get_callosum_effects(bridge);

    EXPECT_TRUE(effects.transfer_active);
    EXPECT_GT(effects.transfer_rate, 0.0f);
    EXPECT_GE(effects.consensus_strength, 0.0f);
    EXPECT_LE(effects.consensus_strength, 1.0f);
}

//=============================================================================
// Global State Tests
//=============================================================================

TEST_F(HemisphericFepBridgeTest, GlobalStateContributions) {
    bridge = hemispheric_fep_create(nullptr, brain, nullptr);
    ASSERT_NE(bridge, nullptr);

    // Force update
    hemispheric_fep_update(bridge);

    global_fep_state_t state = hemispheric_fep_get_global_state(bridge);

    // Contributions should sum to approximately 1.0
    float total_contrib = state.left_contribution + state.right_contribution +
                          state.integration_contribution;
    EXPECT_NEAR(total_contrib, 1.0f, 0.01f);
}

TEST_F(HemisphericFepBridgeTest, GlobalStateIsMinimizing) {
    bridge = hemispheric_fep_create(nullptr, brain, nullptr);
    ASSERT_NE(bridge, nullptr);

    hemispheric_fep_inject_prediction_error(bridge, HEMISPHERE_LEFT, 0.5f);

    global_fep_state_t state = hemispheric_fep_get_global_state(bridge);
    EXPECT_TRUE(state.is_minimizing);
}

//=============================================================================
// Update and Apply Tests
//=============================================================================

TEST_F(HemisphericFepBridgeTest, UpdateSucceeds) {
    bridge = hemispheric_fep_create(nullptr, brain, nullptr);
    ASSERT_NE(bridge, nullptr);

    int result = hemispheric_fep_update(bridge);
    EXPECT_EQ(result, 0);
}

TEST_F(HemisphericFepBridgeTest, ApplyModulationSucceeds) {
    bridge = hemispheric_fep_create(nullptr, brain, nullptr);
    ASSERT_NE(bridge, nullptr);

    int result = hemispheric_fep_apply_modulation(bridge);
    EXPECT_EQ(result, 0);
}

TEST_F(HemisphericFepBridgeTest, UpdateWithNullBridge) {
    int result = hemispheric_fep_update(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(HemisphericFepBridgeTest, ApplyWithNullBridge) {
    int result = hemispheric_fep_apply_modulation(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(HemisphericFepBridgeTest, StatsAccumulate) {
    bridge = hemispheric_fep_create(nullptr, brain, nullptr);
    ASSERT_NE(bridge, nullptr);

    hemispheric_fep_stats_t initial = hemispheric_fep_get_stats(bridge);
    EXPECT_EQ(initial.updates, 0u);

    hemispheric_fep_update(bridge);
    hemispheric_fep_update(bridge);
    hemispheric_fep_update(bridge);

    hemispheric_fep_stats_t after = hemispheric_fep_get_stats(bridge);
    EXPECT_EQ(after.updates, 3u);
}

TEST_F(HemisphericFepBridgeTest, StatsTrackMinimization) {
    bridge = hemispheric_fep_create(nullptr, brain, nullptr);
    ASSERT_NE(bridge, nullptr);

    hemispheric_fep_stats_t initial = hemispheric_fep_get_stats(bridge);
    EXPECT_EQ(initial.fe_minimization_steps, 0u);

    hemispheric_fep_minimize_step(bridge, 0.1f);
    hemispheric_fep_minimize_step(bridge, 0.1f);

    hemispheric_fep_stats_t after = hemispheric_fep_get_stats(bridge);
    EXPECT_EQ(after.fe_minimization_steps, 2u);
}

TEST_F(HemisphericFepBridgeTest, ResetStats) {
    bridge = hemispheric_fep_create(nullptr, brain, nullptr);
    ASSERT_NE(bridge, nullptr);

    // Accumulate stats
    hemispheric_fep_update(bridge);
    hemispheric_fep_trigger_transfer(bridge);

    // Reset
    hemispheric_fep_reset_stats(bridge);

    hemispheric_fep_stats_t stats = hemispheric_fep_get_stats(bridge);
    EXPECT_EQ(stats.updates, 0u);
    EXPECT_EQ(stats.prediction_transfers, 0u);
}

//=============================================================================
// Bio-async Tests
//=============================================================================

TEST_F(HemisphericFepBridgeTest, ConnectBioAsync) {
    bridge = hemispheric_fep_create(nullptr, brain, nullptr);
    ASSERT_NE(bridge, nullptr);

    int result = hemispheric_fep_connect_bio_async(bridge);
    EXPECT_EQ(result, 0);
}

TEST_F(HemisphericFepBridgeTest, DisconnectBioAsync) {
    bridge = hemispheric_fep_create(nullptr, brain, nullptr);
    ASSERT_NE(bridge, nullptr);

    hemispheric_fep_connect_bio_async(bridge);
    int result = hemispheric_fep_disconnect_bio_async(bridge);
    EXPECT_EQ(result, 0);
}

TEST_F(HemisphericFepBridgeTest, ConnectBioAsyncNull) {
    int result = hemispheric_fep_connect_bio_async(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

//=============================================================================
// Query with Null/Invalid Bridge Tests
//=============================================================================

TEST_F(HemisphericFepBridgeTest, GetLeftEffectsNullBridge) {
    hemisphere_fep_effects_t effects = hemispheric_fep_get_left_effects(nullptr);
    EXPECT_FLOAT_EQ(effects.precision, HEMI_FEP_LEFT_BASE_PRECISION);
}

TEST_F(HemisphericFepBridgeTest, GetRightEffectsNullBridge) {
    hemisphere_fep_effects_t effects = hemispheric_fep_get_right_effects(nullptr);
    EXPECT_FLOAT_EQ(effects.precision, HEMI_FEP_RIGHT_BASE_PRECISION);
}

TEST_F(HemisphericFepBridgeTest, GetCallosumEffectsNullBridge) {
    callosum_fep_effects_t effects = hemispheric_fep_get_callosum_effects(nullptr);
    EXPECT_FLOAT_EQ(effects.transfer_rate, HEMI_FEP_CALLOSUM_TRANSFER_RATE);
}

TEST_F(HemisphericFepBridgeTest, GetGlobalStateNullBridge) {
    global_fep_state_t state = hemispheric_fep_get_global_state(nullptr);
    float total = state.left_contribution + state.right_contribution + state.integration_contribution;
    EXPECT_NEAR(total, 1.0f, 0.01f);
}

TEST_F(HemisphericFepBridgeTest, GetPrecisionNullBridge) {
    float left = hemispheric_fep_get_precision(nullptr, HEMISPHERE_LEFT);
    float right = hemispheric_fep_get_precision(nullptr, HEMISPHERE_RIGHT);

    EXPECT_FLOAT_EQ(left, HEMI_FEP_LEFT_BASE_PRECISION);
    EXPECT_FLOAT_EQ(right, HEMI_FEP_RIGHT_BASE_PRECISION);
}

TEST_F(HemisphericFepBridgeTest, GetFreeEnergyNullBridge) {
    float fe = hemispheric_fep_get_free_energy(nullptr, HEMISPHERE_LEFT);
    EXPECT_FLOAT_EQ(fe, 0.0f);
}

TEST_F(HemisphericFepBridgeTest, GetTotalFreeEnergyNullBridge) {
    float fe = hemispheric_fep_get_total_free_energy(nullptr);
    EXPECT_FLOAT_EQ(fe, 0.0f);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
