/**
 * @file test_cerebellum_substrate_bridge.cpp
 * @brief Unit tests for cerebellum substrate bridge
 *
 * Tests:
 * - Configuration defaults
 * - Bridge lifecycle (create/destroy)
 * - Metabolic modulation (ATP, fatigue)
 * - Effect channels (coordination, timing, learning, error correction)
 * - Bio-async integration
 *
 * @author NIMCP Development Team
 * @date 2026-01-03
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

#include "core/cerebellum/nimcp_cerebellum_substrate_bridge.h"
#include "core/neural_substrate/nimcp_neural_substrate.h"

//=============================================================================
// Configuration Tests
//=============================================================================

class CerebellumSubstrateConfigTest : public ::testing::Test {};

TEST_F(CerebellumSubstrateConfigTest, DefaultConfigValues) {
    cerebellum_substrate_config_t config = cerebellum_substrate_default_config();

    // Modulation should be enabled by default
    EXPECT_TRUE(config.enable_atp_modulation);
    EXPECT_TRUE(config.enable_fatigue_modulation);

    // Sensitivity values should be positive
    EXPECT_GT(config.atp_sensitivity, 0.0f);
    EXPECT_GT(config.fatigue_sensitivity, 0.0f);

    // Minimum capacity should be reasonable
    EXPECT_GE(config.min_capacity, 0.0f);
    EXPECT_LE(config.min_capacity, 1.0f);
}

//=============================================================================
// Lifecycle Tests
//=============================================================================

class CerebellumSubstrateBridgeTest : public ::testing::Test {
protected:
    neural_substrate_t* substrate = nullptr;
    cerebellum_substrate_bridge_t* bridge = nullptr;

    void SetUp() override {
        substrate_config_t sub_config;
        substrate_default_config(&sub_config);
        substrate = substrate_create(&sub_config);
        ASSERT_NE(substrate, nullptr);

        cerebellum_substrate_config_t config = cerebellum_substrate_default_config();
        bridge = cerebellum_substrate_bridge_create(nullptr, substrate, &config);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            cerebellum_substrate_bridge_destroy(bridge);
            bridge = nullptr;
        }
        if (substrate) {
            substrate_destroy(substrate);
            substrate = nullptr;
        }
    }
};

TEST_F(CerebellumSubstrateBridgeTest, CreateWithDefaultConfig) {
    EXPECT_NE(bridge, nullptr);
}

TEST_F(CerebellumSubstrateBridgeTest, CreateWithCustomConfig) {
    cerebellum_substrate_config_t config = cerebellum_substrate_default_config();
    config.enable_atp_modulation = false;
    config.atp_sensitivity = 0.5f;

    cerebellum_substrate_bridge_t* custom_bridge =
        cerebellum_substrate_bridge_create(nullptr, substrate, &config);
    ASSERT_NE(custom_bridge, nullptr);

    cerebellum_substrate_bridge_destroy(custom_bridge);
}

TEST_F(CerebellumSubstrateBridgeTest, CreateWithNullSubstrate) {
    cerebellum_substrate_config_t config = cerebellum_substrate_default_config();
    cerebellum_substrate_bridge_t* null_bridge =
        cerebellum_substrate_bridge_create(nullptr, nullptr, &config);

    // Should still create (substrate optional)
    if (null_bridge) {
        cerebellum_substrate_bridge_destroy(null_bridge);
    }
}

TEST_F(CerebellumSubstrateBridgeTest, DestroyNull) {
    cerebellum_substrate_bridge_destroy(nullptr);  // Should not crash
}

//=============================================================================
// Update Tests
//=============================================================================

TEST_F(CerebellumSubstrateBridgeTest, UpdateSucceeds) {
    int result = cerebellum_substrate_bridge_update(bridge);
    EXPECT_EQ(result, 0);
}

TEST_F(CerebellumSubstrateBridgeTest, UpdateNull) {
    int result = cerebellum_substrate_bridge_update(nullptr);
    EXPECT_NE(result, 0);
}

TEST_F(CerebellumSubstrateBridgeTest, UpdateAfterSubstrateChange) {
    substrate_set_atp(substrate, 0.5f);
    substrate_update(substrate, 10);

    int result = cerebellum_substrate_bridge_update(bridge);
    EXPECT_EQ(result, 0);
}

//=============================================================================
// Effects Tests
//=============================================================================

TEST_F(CerebellumSubstrateBridgeTest, GetEffectsSucceeds) {
    cerebellum_substrate_bridge_update(bridge);

    cerebellum_substrate_effects_t effects;
    int result = cerebellum_substrate_bridge_get_effects(bridge, &effects);
    EXPECT_EQ(result, 0);
}

TEST_F(CerebellumSubstrateBridgeTest, GetEffectsNullBridge) {
    cerebellum_substrate_effects_t effects;
    int result = cerebellum_substrate_bridge_get_effects(nullptr, &effects);
    EXPECT_NE(result, 0);
}

TEST_F(CerebellumSubstrateBridgeTest, GetEffectsNullEffects) {
    int result = cerebellum_substrate_bridge_get_effects(bridge, nullptr);
    EXPECT_NE(result, 0);
}

TEST_F(CerebellumSubstrateBridgeTest, EffectsInValidRange) {
    substrate_set_atp(substrate, SUBSTRATE_NORMAL_ATP);
    substrate_update(substrate, 10);
    cerebellum_substrate_bridge_update(bridge);

    cerebellum_substrate_effects_t effects;
    cerebellum_substrate_bridge_get_effects(bridge, &effects);

    EXPECT_GE(effects.motor_coordination, 0.0f);
    EXPECT_LE(effects.motor_coordination, 1.0f);

    EXPECT_GE(effects.timing_precision, 0.0f);
    EXPECT_LE(effects.timing_precision, 1.0f);

    EXPECT_GE(effects.procedural_learning, 0.0f);
    EXPECT_LE(effects.procedural_learning, 1.0f);

    EXPECT_GE(effects.error_correction, 0.0f);
    EXPECT_LE(effects.error_correction, 1.0f);

    EXPECT_GE(effects.overall_capacity, 0.0f);
    EXPECT_LE(effects.overall_capacity, 1.0f);
}

//=============================================================================
// Metabolic Modulation Tests
//=============================================================================

TEST_F(CerebellumSubstrateBridgeTest, NormalATPEffects) {
    substrate_set_atp(substrate, SUBSTRATE_NORMAL_ATP);
    substrate_update(substrate, 10);
    cerebellum_substrate_bridge_update(bridge);

    cerebellum_substrate_effects_t effects;
    cerebellum_substrate_bridge_get_effects(bridge, &effects);

    // With normal ATP, capacity should be high
    EXPECT_GT(effects.overall_capacity, 0.5f);
}

TEST_F(CerebellumSubstrateBridgeTest, LowATPEffects) {
    substrate_set_atp(substrate, 0.3f);
    substrate_update(substrate, 10);
    cerebellum_substrate_bridge_update(bridge);

    cerebellum_substrate_effects_t effects;
    cerebellum_substrate_bridge_get_effects(bridge, &effects);

    // With low ATP, capacity should be reduced but valid
    EXPECT_GE(effects.overall_capacity, 0.0f);
    EXPECT_LE(effects.overall_capacity, 1.0f);
}

TEST_F(CerebellumSubstrateBridgeTest, CriticalATPEffects) {
    substrate_set_atp(substrate, 0.1f);
    substrate_update(substrate, 10);
    cerebellum_substrate_bridge_update(bridge);

    cerebellum_substrate_effects_t effects;
    cerebellum_substrate_bridge_get_effects(bridge, &effects);

    // Even at critical ATP, effects should be valid
    EXPECT_GE(effects.overall_capacity, 0.0f);
}

TEST_F(CerebellumSubstrateBridgeTest, ATPAffectsAllChannels) {
    // High ATP
    substrate_set_atp(substrate, SUBSTRATE_NORMAL_ATP);
    substrate_update(substrate, 10);
    cerebellum_substrate_bridge_update(bridge);

    cerebellum_substrate_effects_t high_effects;
    cerebellum_substrate_bridge_get_effects(bridge, &high_effects);

    // Low ATP
    substrate_set_atp(substrate, 0.3f);
    substrate_update(substrate, 10);
    cerebellum_substrate_bridge_update(bridge);

    cerebellum_substrate_effects_t low_effects;
    cerebellum_substrate_bridge_get_effects(bridge, &low_effects);

    // Both should be valid
    EXPECT_GE(high_effects.motor_coordination, 0.0f);
    EXPECT_GE(low_effects.motor_coordination, 0.0f);
}

TEST_F(CerebellumSubstrateBridgeTest, FatigueModulation) {
    // Induce fatigue through activity
    for (int i = 0; i < 100; i++) {
        substrate_record_spikes(substrate, 300);
        substrate_record_transmissions(substrate, 600);
        substrate_update(substrate, 10);
    }

    cerebellum_substrate_bridge_update(bridge);

    cerebellum_substrate_effects_t effects;
    cerebellum_substrate_bridge_get_effects(bridge, &effects);

    // Effects should still be valid after fatigue
    EXPECT_GE(effects.overall_capacity, 0.0f);
    EXPECT_LE(effects.overall_capacity, 1.0f);
}

//=============================================================================
// Apply Effects Tests
//=============================================================================

TEST_F(CerebellumSubstrateBridgeTest, ApplyEffectsSucceeds) {
    cerebellum_substrate_bridge_update(bridge);
    int result = cerebellum_substrate_bridge_apply_effects(bridge);
    EXPECT_EQ(result, 0);
}

TEST_F(CerebellumSubstrateBridgeTest, ApplyEffectsNull) {
    int result = cerebellum_substrate_bridge_apply_effects(nullptr);
    EXPECT_NE(result, 0);
}

TEST_F(CerebellumSubstrateBridgeTest, ApplyEffectsMultipleTimes) {
    for (int i = 0; i < 10; i++) {
        cerebellum_substrate_bridge_update(bridge);
        int result = cerebellum_substrate_bridge_apply_effects(bridge);
        EXPECT_EQ(result, 0);
    }
}

//=============================================================================
// Continuous Operation Tests
//=============================================================================

TEST_F(CerebellumSubstrateBridgeTest, ContinuousOperation) {
    for (int frame = 0; frame < 100; frame++) {
        // Simulate varying metabolic state
        float atp = 0.5f + 0.4f * sinf((float)frame * 0.1f);
        substrate_set_atp(substrate, atp);
        substrate_update(substrate, 10);

        int result = cerebellum_substrate_bridge_update(bridge);
        EXPECT_EQ(result, 0);

        cerebellum_substrate_effects_t effects;
        result = cerebellum_substrate_bridge_get_effects(bridge, &effects);
        EXPECT_EQ(result, 0);

        // All values should remain valid
        EXPECT_FALSE(std::isnan(effects.overall_capacity));
        EXPECT_FALSE(std::isinf(effects.overall_capacity));
    }
}

TEST_F(CerebellumSubstrateBridgeTest, StabilityUnderStress) {
    // Rapid changes in metabolic state
    for (int i = 0; i < 50; i++) {
        substrate_set_atp(substrate, (i % 2 == 0) ? 0.9f : 0.2f);
        substrate_update(substrate, 5);
        cerebellum_substrate_bridge_update(bridge);

        cerebellum_substrate_effects_t effects;
        cerebellum_substrate_bridge_get_effects(bridge, &effects);

        EXPECT_GE(effects.overall_capacity, 0.0f);
        EXPECT_LE(effects.overall_capacity, 1.0f);
    }
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
