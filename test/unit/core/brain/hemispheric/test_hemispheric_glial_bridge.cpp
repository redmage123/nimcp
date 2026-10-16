/**
 * @file test_hemispheric_glial_bridge.cpp
 * @brief Unit tests for hemispheric glial bridge integration
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "core/brain/hemispheric/nimcp_hemispheric_glial_bridge.h"
#include "core/brain/hemispheric/nimcp_hemispheric_brain.h"

class HemisphericGlialBridgeTest : public ::testing::Test {
protected:
    hemispheric_brain_t* brain = nullptr;
    hemispheric_glial_bridge_t* bridge = nullptr;

    void SetUp() override {
        // Create brain first
        hemispheric_brain_config_t brain_config = hemispheric_brain_default_config();
        brain = hemispheric_brain_create(&brain_config);
    }

    void TearDown() override {
        if (bridge) {
            hemispheric_glial_destroy(bridge);
            bridge = nullptr;
        }
        if (brain) {
            hemispheric_brain_destroy(brain);
            brain = nullptr;
        }
    }
};

//=============================================================================
// Configuration Tests
//=============================================================================

TEST(HemisphericGlialConfigTest, DefaultConfigHasValidValues) {
    hemispheric_glial_config_t config = hemispheric_glial_default_config();

    EXPECT_EQ(config.left_specialization, GLIAL_SPEC_LANGUAGE_DOMINANT);
    EXPECT_EQ(config.right_specialization, GLIAL_SPEC_SPATIAL_DOMINANT);

    EXPECT_GT(config.left_astrocyte_density_factor, 0.0f);
    EXPECT_GT(config.right_astrocyte_density_factor, 0.0f);

    EXPECT_GT(config.calcium_transfer_coefficient, 0.0f);
    EXPECT_LE(config.calcium_transfer_coefficient, 1.0f);

    EXPECT_TRUE(config.enable_cross_hemisphere_waves);
    EXPECT_TRUE(config.enable_bio_async);
}

TEST(HemisphericGlialConfigTest, DefaultMyelinFactorsAreOne) {
    hemispheric_glial_config_t config = hemispheric_glial_default_config();

    EXPECT_FLOAT_EQ(config.left_myelin_factor, 1.0f);
    EXPECT_FLOAT_EQ(config.right_myelin_factor, 1.0f);
}

TEST(HemisphericGlialConfigTest, LeftHemisphereHasHigherAstrocyteDensity) {
    hemispheric_glial_config_t config = hemispheric_glial_default_config();

    // Left hemisphere (language) should have higher astrocyte density
    EXPECT_GT(config.left_astrocyte_density_factor,
              config.right_astrocyte_density_factor);
}

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST(HemisphericGlialLifecycleTest, CreateWithNullBrainFails) {
    hemispheric_glial_config_t config = hemispheric_glial_default_config();
    hemispheric_glial_bridge_t* bridge = hemispheric_glial_create(&config, nullptr);

    EXPECT_EQ(bridge, nullptr);
}

TEST(HemisphericGlialLifecycleTest, CreateWithDefaultConfigSucceeds) {
    hemispheric_brain_config_t brain_config = hemispheric_brain_default_config();
    hemispheric_brain_t* brain = hemispheric_brain_create(&brain_config);
    if (!brain) GTEST_SKIP() << "Brain creation requires heavyweight initialization";

    hemispheric_glial_bridge_t* bridge = hemispheric_glial_create(nullptr, brain);

    EXPECT_NE(bridge, nullptr);

    hemispheric_glial_destroy(bridge);
    hemispheric_brain_destroy(brain);
}

TEST(HemisphericGlialLifecycleTest, CreateWithExplicitConfigSucceeds) {
    hemispheric_brain_config_t brain_config = hemispheric_brain_default_config();
    hemispheric_brain_t* brain = hemispheric_brain_create(&brain_config);
    if (!brain) GTEST_SKIP() << "Brain creation requires heavyweight initialization";

    hemispheric_glial_config_t config = hemispheric_glial_default_config();
    config.left_astrocyte_density_factor = 1.5f;
    config.enable_cross_hemisphere_waves = false;

    hemispheric_glial_bridge_t* bridge = hemispheric_glial_create(&config, brain);

    EXPECT_NE(bridge, nullptr);

    hemispheric_glial_destroy(bridge);
    hemispheric_brain_destroy(brain);
}

TEST(HemisphericGlialLifecycleTest, DestroyNullBridgeIsSafe) {
    hemispheric_glial_destroy(nullptr);
    // Should not crash
    SUCCEED();
}

//=============================================================================
// Update Tests
//=============================================================================

TEST(HemisphericGlialUpdateTest, UpdateNullBridgeFails) {
    int result = hemispheric_glial_update(nullptr, 0.001f);
    EXPECT_LT(result, 0);
}

TEST(HemisphericGlialUpdateTest, ApplyModulationNullBridgeFails) {
    int result = hemispheric_glial_apply_modulation(nullptr);
    EXPECT_LT(result, 0);
}

TEST(HemisphericGlialUpdateTest, UpdateWithValidBridgeSucceeds) {
    hemispheric_brain_config_t brain_config = hemispheric_brain_default_config();
    hemispheric_brain_t* brain = hemispheric_brain_create(&brain_config);
    if (!brain) GTEST_SKIP() << "Brain creation requires heavyweight initialization";

    hemispheric_glial_bridge_t* bridge = hemispheric_glial_create(nullptr, brain);
    ASSERT_NE(bridge, nullptr);

    int result = hemispheric_glial_update(bridge, 0.001f);
    EXPECT_EQ(result, 0);

    hemispheric_glial_destroy(bridge);
    hemispheric_brain_destroy(brain);
}

//=============================================================================
// Effects Tests
//=============================================================================

TEST(HemisphericGlialEffectsTest, InitialEffectsHaveDefaultValues) {
    hemispheric_brain_config_t brain_config = hemispheric_brain_default_config();
    hemispheric_brain_t* brain = hemispheric_brain_create(&brain_config);
    if (!brain) GTEST_SKIP() << "Brain creation requires heavyweight initialization";

    hemispheric_glial_bridge_t* bridge = hemispheric_glial_create(nullptr, brain);
    ASSERT_NE(bridge, nullptr);

    hemisphere_glial_effects_t left_effects =
        hemispheric_glial_get_effects(bridge, HEMISPHERE_LEFT);
    hemisphere_glial_effects_t right_effects =
        hemispheric_glial_get_effects(bridge, HEMISPHERE_RIGHT);

    // Initial modulation should be 1.0 (no change)
    EXPECT_FLOAT_EQ(left_effects.astrocyte_modulation, 1.0f);
    EXPECT_FLOAT_EQ(right_effects.astrocyte_modulation, 1.0f);

    // Initial calcium should be baseline
    EXPECT_GT(left_effects.avg_calcium_level, 0.0f);
    EXPECT_GT(right_effects.avg_calcium_level, 0.0f);

    // Metabolic support should be at max
    EXPECT_FLOAT_EQ(left_effects.metabolic_support, 1.0f);
    EXPECT_FLOAT_EQ(right_effects.metabolic_support, 1.0f);

    hemispheric_glial_destroy(bridge);
    hemispheric_brain_destroy(brain);
}

//=============================================================================
// Myelination Tests
//=============================================================================

TEST(HemisphericGlialMyelinTest, SetMyelinFactorNullBridgeFails) {
    int result = hemispheric_glial_set_myelin_factor(nullptr, HEMISPHERE_LEFT, 1.0f);
    EXPECT_LT(result, 0);
}

TEST(HemisphericGlialMyelinTest, SetMyelinFactorInvalidRangeFails) {
    hemispheric_brain_config_t brain_config = hemispheric_brain_default_config();
    hemispheric_brain_t* brain = hemispheric_brain_create(&brain_config);
    if (!brain) GTEST_SKIP() << "Brain creation requires heavyweight initialization";

    hemispheric_glial_bridge_t* bridge = hemispheric_glial_create(nullptr, brain);
    ASSERT_NE(bridge, nullptr);

    // Negative factor should fail
    int result = hemispheric_glial_set_myelin_factor(bridge, HEMISPHERE_LEFT, -0.5f);
    EXPECT_LT(result, 0);

    // Factor > 2.0 should fail
    result = hemispheric_glial_set_myelin_factor(bridge, HEMISPHERE_LEFT, 2.5f);
    EXPECT_LT(result, 0);

    hemispheric_glial_destroy(bridge);
    hemispheric_brain_destroy(brain);
}

TEST(HemisphericGlialMyelinTest, SetMyelinFactorValidRangeSucceeds) {
    hemispheric_brain_config_t brain_config = hemispheric_brain_default_config();
    hemispheric_brain_t* brain = hemispheric_brain_create(&brain_config);
    if (!brain) GTEST_SKIP() << "Brain creation requires heavyweight initialization";

    hemispheric_glial_bridge_t* bridge = hemispheric_glial_create(nullptr, brain);
    ASSERT_NE(bridge, nullptr);

    int result = hemispheric_glial_set_myelin_factor(bridge, HEMISPHERE_LEFT, 1.5f);
    EXPECT_EQ(result, 0);

    result = hemispheric_glial_set_myelin_factor(bridge, HEMISPHERE_RIGHT, 0.8f);
    EXPECT_EQ(result, 0);

    hemispheric_glial_destroy(bridge);
    hemispheric_brain_destroy(brain);
}

//=============================================================================
// Metabolic Tests
//=============================================================================

TEST(HemisphericGlialMetabolicTest, GetMetabolicSupportNullBridgeReturnsDefault) {
    float support = hemispheric_glial_get_metabolic_support(nullptr, HEMISPHERE_LEFT);
    EXPECT_FLOAT_EQ(support, 1.0f);
}

TEST(HemisphericGlialMetabolicTest, TransferMetabolicNullBridgeFails) {
    int result = hemispheric_glial_transfer_metabolic(nullptr, 0.1f);
    EXPECT_LT(result, 0);
}

TEST(HemisphericGlialMetabolicTest, TransferMetabolicUpdatesLevels) {
    hemispheric_brain_config_t brain_config = hemispheric_brain_default_config();
    hemispheric_brain_t* brain = hemispheric_brain_create(&brain_config);
    if (!brain) GTEST_SKIP() << "Brain creation requires heavyweight initialization";

    hemispheric_glial_bridge_t* bridge = hemispheric_glial_create(nullptr, brain);
    ASSERT_NE(bridge, nullptr);

    // First do a transfer from right to left (negative amount) to lower right's level
    // This allows room for right to increase in the subsequent test
    int pre_result = hemispheric_glial_transfer_metabolic(bridge, -0.3f);
    EXPECT_EQ(pre_result, 0);

    float initial_left = hemispheric_glial_get_metabolic_support(bridge, HEMISPHERE_LEFT);
    float initial_right = hemispheric_glial_get_metabolic_support(bridge, HEMISPHERE_RIGHT);

    // Transfer from left to right (positive amount)
    int result = hemispheric_glial_transfer_metabolic(bridge, 0.2f);
    EXPECT_EQ(result, 0);

    float final_left = hemispheric_glial_get_metabolic_support(bridge, HEMISPHERE_LEFT);
    float final_right = hemispheric_glial_get_metabolic_support(bridge, HEMISPHERE_RIGHT);

    // Left should have decreased, right should have increased
    EXPECT_LT(final_left, initial_left);
    EXPECT_GT(final_right, initial_right);

    hemispheric_glial_destroy(bridge);
    hemispheric_brain_destroy(brain);
}

//=============================================================================
// Cross-Hemisphere Wave Tests
//=============================================================================

TEST(HemisphericGlialWaveTest, TriggerCrossWaveNullBridgeFails) {
    int result = hemispheric_glial_trigger_cross_wave(nullptr, HEMISPHERE_LEFT, 1.0f);
    EXPECT_LT(result, 0);
}

TEST(HemisphericGlialWaveTest, TriggerCrossWaveUpdatesState) {
    hemispheric_brain_config_t brain_config = hemispheric_brain_default_config();
    hemispheric_brain_t* brain = hemispheric_brain_create(&brain_config);
    if (!brain) GTEST_SKIP() << "Brain creation requires heavyweight initialization";

    hemispheric_glial_config_t config = hemispheric_glial_default_config();
    config.enable_cross_hemisphere_waves = true;

    hemispheric_glial_bridge_t* bridge = hemispheric_glial_create(&config, brain);
    ASSERT_NE(bridge, nullptr);

    // Trigger wave from left hemisphere
    int result = hemispheric_glial_trigger_cross_wave(bridge, HEMISPHERE_LEFT, 2.0f);
    EXPECT_EQ(result, 0);

    // Check cross-state
    cross_hemisphere_glial_t state = hemispheric_glial_get_cross_state(bridge);
    EXPECT_TRUE(state.wave_propagating);
    EXPECT_EQ(state.wave_source, HEMISPHERE_LEFT);
    EXPECT_GT(state.calcium_transfer_rate, 0.0f);

    hemispheric_glial_destroy(bridge);
    hemispheric_brain_destroy(brain);
}

TEST(HemisphericGlialWaveTest, DisabledCrossWavesFails) {
    hemispheric_brain_config_t brain_config = hemispheric_brain_default_config();
    hemispheric_brain_t* brain = hemispheric_brain_create(&brain_config);
    if (!brain) GTEST_SKIP() << "Brain creation requires heavyweight initialization";

    hemispheric_glial_config_t config = hemispheric_glial_default_config();
    config.enable_cross_hemisphere_waves = false;

    hemispheric_glial_bridge_t* bridge = hemispheric_glial_create(&config, brain);
    ASSERT_NE(bridge, nullptr);

    // Should fail when cross-hemisphere waves are disabled
    int result = hemispheric_glial_trigger_cross_wave(bridge, HEMISPHERE_LEFT, 2.0f);
    EXPECT_LT(result, 0);

    hemispheric_glial_destroy(bridge);
    hemispheric_brain_destroy(brain);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST(HemisphericGlialStatsTest, InitialStatsAreZero) {
    hemispheric_brain_config_t brain_config = hemispheric_brain_default_config();
    hemispheric_brain_t* brain = hemispheric_brain_create(&brain_config);
    if (!brain) GTEST_SKIP() << "Brain creation requires heavyweight initialization";

    hemispheric_glial_bridge_t* bridge = hemispheric_glial_create(nullptr, brain);
    ASSERT_NE(bridge, nullptr);

    hemispheric_glial_stats_t stats = hemispheric_glial_get_stats(bridge);

    EXPECT_EQ(stats.glial_updates, 0u);
    EXPECT_EQ(stats.cross_hemisphere_waves, 0u);
    EXPECT_EQ(stats.pruning_events_left, 0u);
    EXPECT_EQ(stats.pruning_events_right, 0u);

    hemispheric_glial_destroy(bridge);
    hemispheric_brain_destroy(brain);
}

TEST(HemisphericGlialStatsTest, UpdateIncrementsCounter) {
    hemispheric_brain_config_t brain_config = hemispheric_brain_default_config();
    hemispheric_brain_t* brain = hemispheric_brain_create(&brain_config);
    if (!brain) GTEST_SKIP() << "Brain creation requires heavyweight initialization";

    hemispheric_glial_bridge_t* bridge = hemispheric_glial_create(nullptr, brain);
    ASSERT_NE(bridge, nullptr);

    // Perform multiple updates
    for (int i = 0; i < 5; i++) {
        hemispheric_glial_update(bridge, 0.001f);
    }

    hemispheric_glial_stats_t stats = hemispheric_glial_get_stats(bridge);
    EXPECT_EQ(stats.glial_updates, 5u);

    hemispheric_glial_destroy(bridge);
    hemispheric_brain_destroy(brain);
}

TEST(HemisphericGlialStatsTest, ResetClearsStats) {
    hemispheric_brain_config_t brain_config = hemispheric_brain_default_config();
    hemispheric_brain_t* brain = hemispheric_brain_create(&brain_config);
    if (!brain) GTEST_SKIP() << "Brain creation requires heavyweight initialization";

    hemispheric_glial_bridge_t* bridge = hemispheric_glial_create(nullptr, brain);
    ASSERT_NE(bridge, nullptr);

    // Perform updates
    hemispheric_glial_update(bridge, 0.001f);
    hemispheric_glial_update(bridge, 0.001f);

    // Reset stats
    hemispheric_glial_reset_stats(bridge);

    hemispheric_glial_stats_t stats = hemispheric_glial_get_stats(bridge);
    EXPECT_EQ(stats.glial_updates, 0u);

    hemispheric_glial_destroy(bridge);
    hemispheric_brain_destroy(brain);
}

//=============================================================================
// Connection Tests
//=============================================================================

TEST(HemisphericGlialConnectionTest, ConnectGlialNullBridgeFails) {
    int result = hemispheric_glial_connect_glial(nullptr, HEMISPHERE_LEFT, nullptr);
    EXPECT_LT(result, 0);
}

//=============================================================================
// Pruning Tests
//=============================================================================

TEST(HemisphericGlialPruningTest, ShouldPruneNullBridgeReturnsFalse) {
    bool prune = hemispheric_glial_should_prune(nullptr, HEMISPHERE_LEFT, 12345);
    EXPECT_FALSE(prune);
}

TEST(HemisphericGlialPruningTest, GetPruningRateNullBridgeReturnsZero) {
    float rate = hemispheric_glial_get_pruning_rate(nullptr, HEMISPHERE_LEFT);
    EXPECT_FLOAT_EQ(rate, 0.0f);
}

//=============================================================================
// Bio-async Tests
//=============================================================================

TEST(HemisphericGlialBioAsyncTest, ConnectBioAsyncNullBridgeFails) {
    int result = hemispheric_glial_connect_bio_async(nullptr);
    EXPECT_LT(result, 0);
}

TEST(HemisphericGlialBioAsyncTest, DisconnectBioAsyncNullBridgeFails) {
    int result = hemispheric_glial_disconnect_bio_async(nullptr);
    EXPECT_LT(result, 0);
}

//=============================================================================
// Astrocyte Calcium Tests
//=============================================================================

TEST(HemisphericGlialCalciumTest, GetAstrocyteCalciumNullBridgeReturnsZero) {
    float calcium = hemispheric_glial_get_astrocyte_calcium(nullptr, HEMISPHERE_LEFT, 0);
    EXPECT_FLOAT_EQ(calcium, 0.0f);
}

TEST(HemisphericGlialCalciumTest, GetAstrocyteCalciumWithoutGlialReturnsBaseline) {
    hemispheric_brain_config_t brain_config = hemispheric_brain_default_config();
    hemispheric_brain_t* brain = hemispheric_brain_create(&brain_config);
    if (!brain) GTEST_SKIP() << "Brain creation requires heavyweight initialization";

    hemispheric_glial_bridge_t* bridge = hemispheric_glial_create(nullptr, brain);
    ASSERT_NE(bridge, nullptr);

    // Without connected glial systems, should return baseline
    float calcium = hemispheric_glial_get_astrocyte_calcium(bridge, HEMISPHERE_LEFT, 0);
    EXPECT_FLOAT_EQ(calcium, ASTROCYTE_BASELINE_CALCIUM_UM);

    hemispheric_glial_destroy(bridge);
    hemispheric_brain_destroy(brain);
}

TEST(HemisphericGlialCalciumTest, StimulateAstrocyteNullBridgeFails) {
    int result = hemispheric_glial_stimulate_astrocyte(nullptr, HEMISPHERE_LEFT, 0, 5.0f);
    EXPECT_LT(result, 0);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
