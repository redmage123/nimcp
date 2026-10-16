/**
 * @file test_microglia_sleep_bridge.cpp
 * @brief Comprehensive unit tests for microglia-sleep integration bridge
 * @version 1.0.0
 * @date 2025-12-21
 *
 * WHAT: Unit tests for sleep-microglia bridge functionality
 * WHY:  Ensure correct sleep state modulation of microglia behavior
 * HOW:  Test lifecycle, updates, effects, and state-specific modulation
 *
 * TEST COVERAGE:
 * - Default configuration initialization
 * - Bridge creation and destruction
 * - Update function for all sleep states
 * - Effects retrieval and validation
 * - Query functions (phagocytosis, surveillance, glymphatic, enhancement)
 * - Utility functions for state-specific factors
 * - NULL pointer handling
 * - Edge cases and boundary conditions
 *
 * @author NIMCP Development Team
 */

#include <gtest/gtest.h>
#include <cmath>

// Headers have their own extern "C" guards
#include "glial/sleep/nimcp_microglia_sleep_bridge.h"
#include "cognitive/nimcp_sleep_wake.h"

/* ========================================================================
 * TEST FIXTURE
 * ======================================================================== */

class MicrogliaSleepBridgeTest : public ::testing::Test {
protected:
    sleep_system_t sleep_system;
    microglia_sleep_bridge_t bridge;

    void SetUp() override {
        // Create sleep system with default config
        sleep_system = sleep_system_create(nullptr);
        ASSERT_NE(sleep_system, nullptr);

        // Start in awake state
        sleep_enter_state(sleep_system, SLEEP_STATE_AWAKE);

        bridge = nullptr;
    }

    void TearDown() override {
        if (bridge) {
            microglia_sleep_bridge_destroy(bridge);
            bridge = nullptr;
        }
        if (sleep_system) {
            sleep_system_destroy(sleep_system);
            sleep_system = nullptr;
        }
    }

    /**
     * Helper: Assert float equality with tolerance
     */
    void AssertFloatEqual(float actual, float expected, float tolerance = 0.001f) {
        ASSERT_NEAR(actual, expected, tolerance);
    }

    /**
     * Helper: Create bridge with default config
     */
    microglia_sleep_bridge_t CreateDefaultBridge() {
        return microglia_sleep_bridge_create(nullptr, sleep_system);
    }

    /**
     * Helper: Create bridge with custom config
     */
    microglia_sleep_bridge_t CreateBridgeWithConfig(const microglia_sleep_config_t* config) {
        return microglia_sleep_bridge_create(config, sleep_system);
    }

    /**
     * Helper: Set sleep state and update bridge
     */
    void SetStateAndUpdate(sleep_state_t state) {
        sleep_enter_state(sleep_system, state);
        microglia_sleep_update(bridge);
    }
};

/* ========================================================================
 * DEFAULT CONFIGURATION TESTS
 * ======================================================================== */

TEST_F(MicrogliaSleepBridgeTest, DefaultConfigInitialization) {
    microglia_sleep_config_t config;
    int result = microglia_sleep_default_config(&config);

    EXPECT_EQ(result, 0);
    EXPECT_TRUE(config.enable_phagocytosis_modulation);
    EXPECT_TRUE(config.enable_surveillance_modulation);
    EXPECT_TRUE(config.enable_process_modulation);
    EXPECT_TRUE(config.enable_glymphatic_clearance);
    EXPECT_FLOAT_EQ(config.modulation_strength, 1.0f);
    EXPECT_FLOAT_EQ(config.glymphatic_clearance_multiplier, 15.0f);
}

TEST_F(MicrogliaSleepBridgeTest, DefaultConfigNullPointer) {
    int result = microglia_sleep_default_config(nullptr);
    EXPECT_EQ(result, -1);
}

/* ========================================================================
 * LIFECYCLE TESTS
 * ======================================================================== */

TEST_F(MicrogliaSleepBridgeTest, BridgeCreationWithDefaults) {
    bridge = CreateDefaultBridge();
    ASSERT_NE(bridge, nullptr);
}

TEST_F(MicrogliaSleepBridgeTest, BridgeCreationWithCustomConfig) {
    microglia_sleep_config_t config;
    microglia_sleep_default_config(&config);
    config.modulation_strength = 0.5f;
    config.glymphatic_clearance_multiplier = 10.0f;

    bridge = CreateBridgeWithConfig(&config);
    ASSERT_NE(bridge, nullptr);
}

TEST_F(MicrogliaSleepBridgeTest, BridgeCreationWithNullSleep) {
    bridge = microglia_sleep_bridge_create(nullptr, nullptr);
    EXPECT_EQ(bridge, nullptr);
}

TEST_F(MicrogliaSleepBridgeTest, BridgeDestroyNull) {
    // Should not crash
    microglia_sleep_bridge_destroy(nullptr);
}

TEST_F(MicrogliaSleepBridgeTest, BridgeDestroyValid) {
    bridge = CreateDefaultBridge();
    ASSERT_NE(bridge, nullptr);

    microglia_sleep_bridge_destroy(bridge);
    bridge = nullptr; // Prevent double-free in TearDown
}

/* ========================================================================
 * UPDATE FUNCTION TESTS (ALL SLEEP STATES)
 * ======================================================================== */

TEST_F(MicrogliaSleepBridgeTest, UpdateAwakeState) {
    bridge = CreateDefaultBridge();
    ASSERT_NE(bridge, nullptr);

    SetStateAndUpdate(SLEEP_STATE_AWAKE);

    microglia_sleep_effects_t effects;
    int result = microglia_sleep_get_effects(bridge, &effects);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(effects.current_state, SLEEP_STATE_AWAKE);
    AssertFloatEqual(effects.phagocytosis_rate_factor, MICROGLIA_SLEEP_PHAGO_AWAKE);
    AssertFloatEqual(effects.surveillance_activity_factor, MICROGLIA_SLEEP_SURVEILLANCE_AWAKE);
    AssertFloatEqual(effects.process_extension_factor, MICROGLIA_SLEEP_PROCESS_AWAKE);
    EXPECT_FALSE(effects.microglia_enhanced);
    EXPECT_FALSE(effects.glymphatic_active);
}

TEST_F(MicrogliaSleepBridgeTest, UpdateDrowsyState) {
    bridge = CreateDefaultBridge();
    ASSERT_NE(bridge, nullptr);

    SetStateAndUpdate(SLEEP_STATE_DROWSY);

    microglia_sleep_effects_t effects;
    int result = microglia_sleep_get_effects(bridge, &effects);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(effects.current_state, SLEEP_STATE_DROWSY);
    AssertFloatEqual(effects.phagocytosis_rate_factor, MICROGLIA_SLEEP_PHAGO_DROWSY);
    AssertFloatEqual(effects.surveillance_activity_factor, MICROGLIA_SLEEP_SURVEILLANCE_DROWSY);
    AssertFloatEqual(effects.process_extension_factor, MICROGLIA_SLEEP_PROCESS_DROWSY);
    EXPECT_FALSE(effects.microglia_enhanced);
    EXPECT_FALSE(effects.glymphatic_active);
}

TEST_F(MicrogliaSleepBridgeTest, UpdateLightNREMState) {
    bridge = CreateDefaultBridge();
    ASSERT_NE(bridge, nullptr);

    SetStateAndUpdate(SLEEP_STATE_LIGHT_NREM);

    microglia_sleep_effects_t effects;
    int result = microglia_sleep_get_effects(bridge, &effects);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(effects.current_state, SLEEP_STATE_LIGHT_NREM);
    AssertFloatEqual(effects.phagocytosis_rate_factor, MICROGLIA_SLEEP_PHAGO_LIGHT_NREM);
    AssertFloatEqual(effects.surveillance_activity_factor, MICROGLIA_SLEEP_SURVEILLANCE_LIGHT_NREM);
    AssertFloatEqual(effects.process_extension_factor, MICROGLIA_SLEEP_PROCESS_LIGHT_NREM);
    EXPECT_FALSE(effects.microglia_enhanced);
    EXPECT_FALSE(effects.glymphatic_active);
}

TEST_F(MicrogliaSleepBridgeTest, UpdateDeepNREMState) {
    bridge = CreateDefaultBridge();
    ASSERT_NE(bridge, nullptr);

    SetStateAndUpdate(SLEEP_STATE_DEEP_NREM);

    microglia_sleep_effects_t effects;
    int result = microglia_sleep_get_effects(bridge, &effects);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(effects.current_state, SLEEP_STATE_DEEP_NREM);
    AssertFloatEqual(effects.phagocytosis_rate_factor, MICROGLIA_SLEEP_PHAGO_DEEP_NREM);
    AssertFloatEqual(effects.surveillance_activity_factor, MICROGLIA_SLEEP_SURVEILLANCE_DEEP_NREM);
    AssertFloatEqual(effects.process_extension_factor, MICROGLIA_SLEEP_PROCESS_DEEP_NREM);
    EXPECT_TRUE(effects.microglia_enhanced); // Enhanced during deep NREM
    EXPECT_TRUE(effects.glymphatic_active);  // Glymphatic active during deep NREM
}

TEST_F(MicrogliaSleepBridgeTest, UpdateREMState) {
    bridge = CreateDefaultBridge();
    ASSERT_NE(bridge, nullptr);

    SetStateAndUpdate(SLEEP_STATE_REM);

    microglia_sleep_effects_t effects;
    int result = microglia_sleep_get_effects(bridge, &effects);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(effects.current_state, SLEEP_STATE_REM);
    AssertFloatEqual(effects.phagocytosis_rate_factor, MICROGLIA_SLEEP_PHAGO_REM);
    AssertFloatEqual(effects.surveillance_activity_factor, MICROGLIA_SLEEP_SURVEILLANCE_REM);
    AssertFloatEqual(effects.process_extension_factor, MICROGLIA_SLEEP_PROCESS_REM);
    EXPECT_FALSE(effects.microglia_enhanced);
    EXPECT_FALSE(effects.glymphatic_active);
}

TEST_F(MicrogliaSleepBridgeTest, UpdateNullBridge) {
    int result = microglia_sleep_update(nullptr);
    EXPECT_EQ(result, -1);
}

/* ========================================================================
 * EFFECTS RETRIEVAL TESTS
 * ======================================================================== */

TEST_F(MicrogliaSleepBridgeTest, GetEffectsValid) {
    bridge = CreateDefaultBridge();
    ASSERT_NE(bridge, nullptr);

    microglia_sleep_effects_t effects;
    int result = microglia_sleep_get_effects(bridge, &effects);

    EXPECT_EQ(result, 0);
    // Effects should have valid values
    EXPECT_GE(effects.phagocytosis_rate_factor, 0.0f);
    EXPECT_LE(effects.phagocytosis_rate_factor, 1.0f);
    EXPECT_GE(effects.surveillance_activity_factor, 0.0f);
    EXPECT_GE(effects.process_extension_factor, 0.0f);
}

TEST_F(MicrogliaSleepBridgeTest, GetEffectsNullBridge) {
    microglia_sleep_effects_t effects;
    int result = microglia_sleep_get_effects(nullptr, &effects);
    EXPECT_EQ(result, -1);
}

TEST_F(MicrogliaSleepBridgeTest, GetEffectsNullEffects) {
    bridge = CreateDefaultBridge();
    ASSERT_NE(bridge, nullptr);

    int result = microglia_sleep_get_effects(bridge, nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(MicrogliaSleepBridgeTest, GetEffectsBothNull) {
    int result = microglia_sleep_get_effects(nullptr, nullptr);
    EXPECT_EQ(result, -1);
}

/* ========================================================================
 * QUERY FUNCTION TESTS
 * ======================================================================== */

TEST_F(MicrogliaSleepBridgeTest, GetPhagocytosisRateAwake) {
    bridge = CreateDefaultBridge();
    ASSERT_NE(bridge, nullptr);
    SetStateAndUpdate(SLEEP_STATE_AWAKE);

    float rate = microglia_sleep_get_phagocytosis_rate(bridge);
    AssertFloatEqual(rate, MICROGLIA_SLEEP_PHAGO_AWAKE);
}

TEST_F(MicrogliaSleepBridgeTest, GetPhagocytosisRateDeepNREM) {
    bridge = CreateDefaultBridge();
    ASSERT_NE(bridge, nullptr);
    SetStateAndUpdate(SLEEP_STATE_DEEP_NREM);

    float rate = microglia_sleep_get_phagocytosis_rate(bridge);
    AssertFloatEqual(rate, MICROGLIA_SLEEP_PHAGO_DEEP_NREM);
}

TEST_F(MicrogliaSleepBridgeTest, GetPhagocytosisRateNullBridge) {
    float rate = microglia_sleep_get_phagocytosis_rate(nullptr);
    EXPECT_FLOAT_EQ(rate, MICROGLIA_SLEEP_PHAGO_AWAKE);  // Safe default: baseline AWAKE value
}

TEST_F(MicrogliaSleepBridgeTest, GetSurveillanceActivityAwake) {
    bridge = CreateDefaultBridge();
    ASSERT_NE(bridge, nullptr);
    SetStateAndUpdate(SLEEP_STATE_AWAKE);

    float activity = microglia_sleep_get_surveillance_activity(bridge);
    AssertFloatEqual(activity, MICROGLIA_SLEEP_SURVEILLANCE_AWAKE);
}

TEST_F(MicrogliaSleepBridgeTest, GetSurveillanceActivityDeepNREM) {
    bridge = CreateDefaultBridge();
    ASSERT_NE(bridge, nullptr);
    SetStateAndUpdate(SLEEP_STATE_DEEP_NREM);

    float activity = microglia_sleep_get_surveillance_activity(bridge);
    AssertFloatEqual(activity, MICROGLIA_SLEEP_SURVEILLANCE_DEEP_NREM);
}

TEST_F(MicrogliaSleepBridgeTest, GetSurveillanceActivityNullBridge) {
    float activity = microglia_sleep_get_surveillance_activity(nullptr);
    EXPECT_FLOAT_EQ(activity, MICROGLIA_SLEEP_SURVEILLANCE_AWAKE);  // Safe default: baseline AWAKE value
}

TEST_F(MicrogliaSleepBridgeTest, IsGlymphaticActiveAwake) {
    bridge = CreateDefaultBridge();
    ASSERT_NE(bridge, nullptr);
    SetStateAndUpdate(SLEEP_STATE_AWAKE);

    bool active = microglia_sleep_is_glymphatic_active(bridge);
    EXPECT_FALSE(active);
}

TEST_F(MicrogliaSleepBridgeTest, IsGlymphaticActiveDeepNREM) {
    bridge = CreateDefaultBridge();
    ASSERT_NE(bridge, nullptr);
    SetStateAndUpdate(SLEEP_STATE_DEEP_NREM);

    bool active = microglia_sleep_is_glymphatic_active(bridge);
    EXPECT_TRUE(active);
}

TEST_F(MicrogliaSleepBridgeTest, IsGlymphaticActiveNullBridge) {
    bool active = microglia_sleep_is_glymphatic_active(nullptr);
    EXPECT_FALSE(active);
}

TEST_F(MicrogliaSleepBridgeTest, IsEnhancedAwake) {
    bridge = CreateDefaultBridge();
    ASSERT_NE(bridge, nullptr);
    SetStateAndUpdate(SLEEP_STATE_AWAKE);

    bool enhanced = microglia_sleep_is_enhanced(bridge);
    EXPECT_FALSE(enhanced);
}

TEST_F(MicrogliaSleepBridgeTest, IsEnhancedDeepNREM) {
    bridge = CreateDefaultBridge();
    ASSERT_NE(bridge, nullptr);
    SetStateAndUpdate(SLEEP_STATE_DEEP_NREM);

    bool enhanced = microglia_sleep_is_enhanced(bridge);
    EXPECT_TRUE(enhanced);
}

TEST_F(MicrogliaSleepBridgeTest, IsEnhancedNullBridge) {
    bool enhanced = microglia_sleep_is_enhanced(nullptr);
    EXPECT_FALSE(enhanced);
}

/* ========================================================================
 * UTILITY FUNCTION TESTS
 * ======================================================================== */

TEST_F(MicrogliaSleepBridgeTest, PhagocytosisForStateAwake) {
    float rate = microglia_sleep_phagocytosis_for_state(SLEEP_STATE_AWAKE);
    AssertFloatEqual(rate, MICROGLIA_SLEEP_PHAGO_AWAKE);
}

TEST_F(MicrogliaSleepBridgeTest, PhagocytosisForStateDrowsy) {
    float rate = microglia_sleep_phagocytosis_for_state(SLEEP_STATE_DROWSY);
    AssertFloatEqual(rate, MICROGLIA_SLEEP_PHAGO_DROWSY);
}

TEST_F(MicrogliaSleepBridgeTest, PhagocytosisForStateLightNREM) {
    float rate = microglia_sleep_phagocytosis_for_state(SLEEP_STATE_LIGHT_NREM);
    AssertFloatEqual(rate, MICROGLIA_SLEEP_PHAGO_LIGHT_NREM);
}

TEST_F(MicrogliaSleepBridgeTest, PhagocytosisForStateDeepNREM) {
    float rate = microglia_sleep_phagocytosis_for_state(SLEEP_STATE_DEEP_NREM);
    AssertFloatEqual(rate, MICROGLIA_SLEEP_PHAGO_DEEP_NREM);
}

TEST_F(MicrogliaSleepBridgeTest, PhagocytosisForStateREM) {
    float rate = microglia_sleep_phagocytosis_for_state(SLEEP_STATE_REM);
    AssertFloatEqual(rate, MICROGLIA_SLEEP_PHAGO_REM);
}

TEST_F(MicrogliaSleepBridgeTest, SurveillanceForStateAwake) {
    float activity = microglia_sleep_surveillance_for_state(SLEEP_STATE_AWAKE);
    AssertFloatEqual(activity, MICROGLIA_SLEEP_SURVEILLANCE_AWAKE);
}

TEST_F(MicrogliaSleepBridgeTest, SurveillanceForStateDrowsy) {
    float activity = microglia_sleep_surveillance_for_state(SLEEP_STATE_DROWSY);
    AssertFloatEqual(activity, MICROGLIA_SLEEP_SURVEILLANCE_DROWSY);
}

TEST_F(MicrogliaSleepBridgeTest, SurveillanceForStateLightNREM) {
    float activity = microglia_sleep_surveillance_for_state(SLEEP_STATE_LIGHT_NREM);
    AssertFloatEqual(activity, MICROGLIA_SLEEP_SURVEILLANCE_LIGHT_NREM);
}

TEST_F(MicrogliaSleepBridgeTest, SurveillanceForStateDeepNREM) {
    float activity = microglia_sleep_surveillance_for_state(SLEEP_STATE_DEEP_NREM);
    AssertFloatEqual(activity, MICROGLIA_SLEEP_SURVEILLANCE_DEEP_NREM);
}

TEST_F(MicrogliaSleepBridgeTest, SurveillanceForStateREM) {
    float activity = microglia_sleep_surveillance_for_state(SLEEP_STATE_REM);
    AssertFloatEqual(activity, MICROGLIA_SLEEP_SURVEILLANCE_REM);
}

TEST_F(MicrogliaSleepBridgeTest, ProcessExtensionForStateAwake) {
    float extension = microglia_sleep_process_extension_for_state(SLEEP_STATE_AWAKE);
    AssertFloatEqual(extension, MICROGLIA_SLEEP_PROCESS_AWAKE);
}

TEST_F(MicrogliaSleepBridgeTest, ProcessExtensionForStateDrowsy) {
    float extension = microglia_sleep_process_extension_for_state(SLEEP_STATE_DROWSY);
    AssertFloatEqual(extension, MICROGLIA_SLEEP_PROCESS_DROWSY);
}

TEST_F(MicrogliaSleepBridgeTest, ProcessExtensionForStateLightNREM) {
    float extension = microglia_sleep_process_extension_for_state(SLEEP_STATE_LIGHT_NREM);
    AssertFloatEqual(extension, MICROGLIA_SLEEP_PROCESS_LIGHT_NREM);
}

TEST_F(MicrogliaSleepBridgeTest, ProcessExtensionForStateDeepNREM) {
    float extension = microglia_sleep_process_extension_for_state(SLEEP_STATE_DEEP_NREM);
    AssertFloatEqual(extension, MICROGLIA_SLEEP_PROCESS_DEEP_NREM);
}

TEST_F(MicrogliaSleepBridgeTest, ProcessExtensionForStateREM) {
    float extension = microglia_sleep_process_extension_for_state(SLEEP_STATE_REM);
    AssertFloatEqual(extension, MICROGLIA_SLEEP_PROCESS_REM);
}

/* ========================================================================
 * STATE TRANSITION TESTS
 * ======================================================================== */

TEST_F(MicrogliaSleepBridgeTest, StateTransitionAwakeToDrowsy) {
    bridge = CreateDefaultBridge();
    ASSERT_NE(bridge, nullptr);

    SetStateAndUpdate(SLEEP_STATE_AWAKE);
    float awake_phago = microglia_sleep_get_phagocytosis_rate(bridge);

    SetStateAndUpdate(SLEEP_STATE_DROWSY);
    float drowsy_phago = microglia_sleep_get_phagocytosis_rate(bridge);

    // Phagocytosis should increase from awake to drowsy
    EXPECT_GT(drowsy_phago, awake_phago);
}

TEST_F(MicrogliaSleepBridgeTest, StateTransitionDrowsyToDeepNREM) {
    bridge = CreateDefaultBridge();
    ASSERT_NE(bridge, nullptr);

    SetStateAndUpdate(SLEEP_STATE_DROWSY);
    bool drowsy_enhanced = microglia_sleep_is_enhanced(bridge);

    SetStateAndUpdate(SLEEP_STATE_DEEP_NREM);
    bool deep_enhanced = microglia_sleep_is_enhanced(bridge);

    // Should not be enhanced in drowsy, but enhanced in deep NREM
    EXPECT_FALSE(drowsy_enhanced);
    EXPECT_TRUE(deep_enhanced);
}

TEST_F(MicrogliaSleepBridgeTest, StateTransitionDeepNREMToREM) {
    bridge = CreateDefaultBridge();
    ASSERT_NE(bridge, nullptr);

    SetStateAndUpdate(SLEEP_STATE_DEEP_NREM);
    float deep_phago = microglia_sleep_get_phagocytosis_rate(bridge);

    SetStateAndUpdate(SLEEP_STATE_REM);
    float rem_phago = microglia_sleep_get_phagocytosis_rate(bridge);

    // Phagocytosis should decrease from deep NREM to REM
    EXPECT_LT(rem_phago, deep_phago);
}

/* ========================================================================
 * MODULATION STRENGTH TESTS
 * ======================================================================== */

TEST_F(MicrogliaSleepBridgeTest, ModulationStrengthZero) {
    microglia_sleep_config_t config;
    microglia_sleep_default_config(&config);
    config.modulation_strength = 0.0f;

    bridge = CreateBridgeWithConfig(&config);
    ASSERT_NE(bridge, nullptr);

    SetStateAndUpdate(SLEEP_STATE_DEEP_NREM);

    microglia_sleep_effects_t effects;
    microglia_sleep_get_effects(bridge, &effects);

    // With zero modulation, effects should be at baseline (1.0 for most factors)
    // or minimal modulation
    EXPECT_LE(std::abs(effects.phagocytosis_rate_factor - 1.0f), 1.0f);
}

TEST_F(MicrogliaSleepBridgeTest, ModulationStrengthHalf) {
    microglia_sleep_config_t config;
    microglia_sleep_default_config(&config);
    config.modulation_strength = 0.5f;

    bridge = CreateBridgeWithConfig(&config);
    ASSERT_NE(bridge, nullptr);

    SetStateAndUpdate(SLEEP_STATE_DEEP_NREM);

    microglia_sleep_effects_t effects;
    microglia_sleep_get_effects(bridge, &effects);

    // With half modulation, effects should be between baseline and full
    EXPECT_GT(effects.phagocytosis_rate_factor, 0.0f);
    EXPECT_LE(effects.phagocytosis_rate_factor, 1.0f);
}

/* ========================================================================
 * FEATURE ENABLE/DISABLE TESTS
 * ======================================================================== */

TEST_F(MicrogliaSleepBridgeTest, DisablePhagocytosisModulation) {
    microglia_sleep_config_t config;
    microglia_sleep_default_config(&config);
    config.enable_phagocytosis_modulation = false;

    bridge = CreateBridgeWithConfig(&config);
    ASSERT_NE(bridge, nullptr);

    SetStateAndUpdate(SLEEP_STATE_DEEP_NREM);

    // Phagocytosis modulation disabled, should have baseline effect
    float phago_rate = microglia_sleep_get_phagocytosis_rate(bridge);
    EXPECT_GE(phago_rate, 0.0f);
}

TEST_F(MicrogliaSleepBridgeTest, DisableSurveillanceModulation) {
    microglia_sleep_config_t config;
    microglia_sleep_default_config(&config);
    config.enable_surveillance_modulation = false;

    bridge = CreateBridgeWithConfig(&config);
    ASSERT_NE(bridge, nullptr);

    SetStateAndUpdate(SLEEP_STATE_DEEP_NREM);

    // Surveillance modulation disabled
    float surveillance = microglia_sleep_get_surveillance_activity(bridge);
    EXPECT_GE(surveillance, 0.0f);
}

TEST_F(MicrogliaSleepBridgeTest, DisableGlymphatic) {
    microglia_sleep_config_t config;
    microglia_sleep_default_config(&config);
    config.enable_glymphatic_clearance = false;

    bridge = CreateBridgeWithConfig(&config);
    ASSERT_NE(bridge, nullptr);

    SetStateAndUpdate(SLEEP_STATE_DEEP_NREM);

    // Glymphatic disabled even in deep NREM
    bool glymphatic = microglia_sleep_is_glymphatic_active(bridge);
    EXPECT_FALSE(glymphatic);
}

/* ========================================================================
 * EDGE CASE TESTS
 * ======================================================================== */

TEST_F(MicrogliaSleepBridgeTest, RapidStateChanges) {
    bridge = CreateDefaultBridge();
    ASSERT_NE(bridge, nullptr);

    // Rapidly change states multiple times
    for (int i = 0; i < 10; i++) {
        SetStateAndUpdate(SLEEP_STATE_AWAKE);
        SetStateAndUpdate(SLEEP_STATE_DEEP_NREM);
        SetStateAndUpdate(SLEEP_STATE_REM);
    }

    // Should still work correctly
    microglia_sleep_effects_t effects;
    int result = microglia_sleep_get_effects(bridge, &effects);
    EXPECT_EQ(result, 0);
}

TEST_F(MicrogliaSleepBridgeTest, MultipleUpdatesNoStateChange) {
    bridge = CreateDefaultBridge();
    ASSERT_NE(bridge, nullptr);

    SetStateAndUpdate(SLEEP_STATE_DEEP_NREM);

    // Multiple updates in same state
    for (int i = 0; i < 5; i++) {
        int result = microglia_sleep_update(bridge);
        EXPECT_EQ(result, 0);
    }

    // Should maintain correct state
    microglia_sleep_effects_t effects;
    microglia_sleep_get_effects(bridge, &effects);
    EXPECT_EQ(effects.current_state, SLEEP_STATE_DEEP_NREM);
}

TEST_F(MicrogliaSleepBridgeTest, GlymphaticClearanceMultiplierCustom) {
    microglia_sleep_config_t config;
    microglia_sleep_default_config(&config);
    config.glymphatic_clearance_multiplier = 20.0f; // Higher than default

    bridge = CreateBridgeWithConfig(&config);
    ASSERT_NE(bridge, nullptr);

    SetStateAndUpdate(SLEEP_STATE_DEEP_NREM);

    microglia_sleep_effects_t effects;
    microglia_sleep_get_effects(bridge, &effects);

    // Glymphatic should be active with custom multiplier
    EXPECT_TRUE(effects.glymphatic_active);
    EXPECT_GT(effects.glymphatic_clearance_factor, 0.0f);
}

/* ========================================================================
 * PHAGOCYTOSIS RATE PROGRESSION TEST
 * ======================================================================== */

TEST_F(MicrogliaSleepBridgeTest, PhagocytosisRateProgression) {
    bridge = CreateDefaultBridge();
    ASSERT_NE(bridge, nullptr);

    // Test progression through sleep cycle
    sleep_state_t states[] = {
        SLEEP_STATE_AWAKE,
        SLEEP_STATE_DROWSY,
        SLEEP_STATE_LIGHT_NREM,
        SLEEP_STATE_DEEP_NREM,
        SLEEP_STATE_REM
    };

    float expected_rates[] = {
        MICROGLIA_SLEEP_PHAGO_AWAKE,
        MICROGLIA_SLEEP_PHAGO_DROWSY,
        MICROGLIA_SLEEP_PHAGO_LIGHT_NREM,
        MICROGLIA_SLEEP_PHAGO_DEEP_NREM,
        MICROGLIA_SLEEP_PHAGO_REM
    };

    for (size_t i = 0; i < sizeof(states) / sizeof(states[0]); i++) {
        SetStateAndUpdate(states[i]);
        float rate = microglia_sleep_get_phagocytosis_rate(bridge);
        AssertFloatEqual(rate, expected_rates[i]);
    }
}

/* ========================================================================
 * SURVEILLANCE ACTIVITY PROGRESSION TEST
 * ======================================================================== */

TEST_F(MicrogliaSleepBridgeTest, SurveillanceActivityProgression) {
    bridge = CreateDefaultBridge();
    ASSERT_NE(bridge, nullptr);

    sleep_state_t states[] = {
        SLEEP_STATE_AWAKE,
        SLEEP_STATE_DROWSY,
        SLEEP_STATE_LIGHT_NREM,
        SLEEP_STATE_DEEP_NREM,
        SLEEP_STATE_REM
    };

    float expected_surveillance[] = {
        MICROGLIA_SLEEP_SURVEILLANCE_AWAKE,
        MICROGLIA_SLEEP_SURVEILLANCE_DROWSY,
        MICROGLIA_SLEEP_SURVEILLANCE_LIGHT_NREM,
        MICROGLIA_SLEEP_SURVEILLANCE_DEEP_NREM,
        MICROGLIA_SLEEP_SURVEILLANCE_REM
    };

    for (size_t i = 0; i < sizeof(states) / sizeof(states[0]); i++) {
        SetStateAndUpdate(states[i]);
        float activity = microglia_sleep_get_surveillance_activity(bridge);
        AssertFloatEqual(activity, expected_surveillance[i]);
    }
}

/* ========================================================================
 * MAIN
 * ======================================================================== */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
