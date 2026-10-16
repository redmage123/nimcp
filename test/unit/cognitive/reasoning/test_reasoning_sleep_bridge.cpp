/**
 * @file test_reasoning_sleep_bridge.cpp
 * @brief Unit tests for Reasoning-Sleep Bridge
 * @version 1.0.0
 * @date 2025-12-21
 *
 * WHAT: Comprehensive test suite for reasoning-sleep integration
 * WHY:  Validate sleep modulation of logical/creative reasoning, inference speed
 * HOW:  Test all sleep states, configuration, effects, bio-async, edge cases
 *
 * Test Coverage:
 * - Default config initialization
 * - Bridge creation and destruction
 * - Update function for each sleep state (AWAKE, DROWSY, LIGHT_NREM, DEEP_NREM, REM)
 * - Effects retrieval and validation
 * - Individual factor queries
 * - Offline/REM creative state checks
 * - Utility functions for state-specific factors
 * - NULL pointer handling
 * - Edge cases and configuration variations
 */

#include <gtest/gtest.h>
#include <cmath>

// Headers have their own extern "C" guards
#include "cognitive/reasoning/nimcp_reasoning_sleep_bridge.h"
#include "cognitive/nimcp_sleep_wake.h"

/* ============================================================================
 * TEST FIXTURE
 * ========================================================================== */

class ReasoningSleepBridgeTest : public ::testing::Test {
protected:
    sleep_system_t sleep_system;
    reasoning_sleep_bridge_t bridge;

    void SetUp() override {
        sleep_system = nullptr;
        bridge = nullptr;
    }

    void TearDown() override {
        if (bridge) {
            reasoning_sleep_bridge_destroy(bridge);
            bridge = nullptr;
        }
        if (sleep_system) {
            sleep_system_destroy(sleep_system);
            sleep_system = nullptr;
        }
    }

    // Helper: Create sleep system with defaults
    void CreateSleepSystem() {
        sleep_system = sleep_system_create(nullptr);
        ASSERT_NE(nullptr, sleep_system);
    }

    // Helper: Create bridge with defaults
    void CreateBridge() {
        CreateSleepSystem();
        bridge = reasoning_sleep_bridge_create(nullptr, sleep_system);
        ASSERT_NE(nullptr, bridge);
    }

    // Helper: Set sleep state and update bridge
    void SetSleepState(sleep_state_t state) {
        ASSERT_TRUE(sleep_enter_state(sleep_system, state));
        ASSERT_EQ(0, reasoning_sleep_update(bridge));
    }

    // Helper: Validate effects for a given state
    void ValidateEffectsForState(sleep_state_t state,
                                  float expected_logical,
                                  float expected_creative,
                                  float expected_speed,
                                  bool expected_offline,
                                  bool expected_rem_creative) {
        SetSleepState(state);

        reasoning_sleep_effects_t effects;
        ASSERT_EQ(0, reasoning_sleep_get_effects(bridge, &effects));

        EXPECT_FLOAT_EQ(expected_logical, effects.logical_reasoning_factor);
        EXPECT_FLOAT_EQ(expected_creative, effects.creative_reasoning_factor);
        EXPECT_FLOAT_EQ(expected_speed, effects.inference_speed_factor);
        EXPECT_EQ(state, effects.current_state);
        EXPECT_EQ(expected_offline, effects.reasoning_offline);
        EXPECT_EQ(expected_rem_creative, effects.rem_creativity_boost);

        // Validate individual queries match
        EXPECT_FLOAT_EQ(expected_logical, reasoning_sleep_get_logical_factor(bridge));
        EXPECT_FLOAT_EQ(expected_creative, reasoning_sleep_get_creative_factor(bridge));
        EXPECT_EQ(expected_offline, reasoning_sleep_is_offline(bridge));
        EXPECT_EQ(expected_rem_creative, reasoning_sleep_is_rem_creative(bridge));
    }
};

/* ============================================================================
 * CONFIGURATION TESTS
 * ========================================================================== */

TEST_F(ReasoningSleepBridgeTest, DefaultConfigInitialization) {
    reasoning_sleep_config_t config;
    int result = reasoning_sleep_default_config(&config);

    EXPECT_EQ(0, result);
    EXPECT_TRUE(config.enable_logical_modulation);
    EXPECT_TRUE(config.enable_creative_modulation);
    EXPECT_TRUE(config.enable_speed_modulation);
    EXPECT_TRUE(config.enable_working_memory_gating);
    EXPECT_FLOAT_EQ(1.0f, config.modulation_strength);
}

TEST_F(ReasoningSleepBridgeTest, DefaultConfigNullPointer) {
    int result = reasoning_sleep_default_config(nullptr);
    EXPECT_NE(0, result);
}

TEST_F(ReasoningSleepBridgeTest, CustomConfiguration) {
    reasoning_sleep_config_t config;
    reasoning_sleep_default_config(&config);

    // Customize
    config.enable_logical_modulation = false;
    config.enable_creative_modulation = true;
    config.enable_speed_modulation = false;
    config.modulation_strength = 0.5f;

    CreateSleepSystem();
    bridge = reasoning_sleep_bridge_create(&config, sleep_system);
    ASSERT_NE(nullptr, bridge);

    // Effects should respect configuration
    SetSleepState(SLEEP_STATE_REM);
    reasoning_sleep_effects_t effects;
    ASSERT_EQ(0, reasoning_sleep_get_effects(bridge, &effects));

    // With modulation_strength = 0.5 and logical disabled (stays at 1.0),
    // while creative is enabled in REM (base 1.5), creative should be enhanced
    // creative = 1.5 * 0.5 + 0.5 = 1.25, logical = 1.0
    EXPECT_LT(effects.logical_reasoning_factor, effects.creative_reasoning_factor);
}

/* ============================================================================
 * LIFECYCLE TESTS
 * ========================================================================== */

TEST_F(ReasoningSleepBridgeTest, CreateWithDefaults) {
    CreateSleepSystem();
    bridge = reasoning_sleep_bridge_create(nullptr, sleep_system);

    ASSERT_NE(nullptr, bridge);

    // Should start in AWAKE state with full capacity
    reasoning_sleep_effects_t effects;
    ASSERT_EQ(0, reasoning_sleep_get_effects(bridge, &effects));

    EXPECT_FLOAT_EQ(1.0f, effects.logical_reasoning_factor);
    EXPECT_FLOAT_EQ(1.0f, effects.creative_reasoning_factor);
    EXPECT_FLOAT_EQ(1.0f, effects.inference_speed_factor);
    EXPECT_EQ(SLEEP_STATE_AWAKE, effects.current_state);
    EXPECT_FALSE(effects.reasoning_offline);
    EXPECT_FALSE(effects.rem_creativity_boost);
}

TEST_F(ReasoningSleepBridgeTest, CreateWithCustomConfig) {
    reasoning_sleep_config_t config;
    reasoning_sleep_default_config(&config);
    config.modulation_strength = 0.5f;

    CreateSleepSystem();
    bridge = reasoning_sleep_bridge_create(&config, sleep_system);

    ASSERT_NE(nullptr, bridge);
}

TEST_F(ReasoningSleepBridgeTest, CreateNullSleepSystem) {
    bridge = reasoning_sleep_bridge_create(nullptr, nullptr);
    EXPECT_EQ(nullptr, bridge);
}

TEST_F(ReasoningSleepBridgeTest, DestroyNullBridge) {
    // Should not crash
    reasoning_sleep_bridge_destroy(nullptr);
}

TEST_F(ReasoningSleepBridgeTest, DestroyValidBridge) {
    CreateBridge();

    reasoning_sleep_bridge_destroy(bridge);
    bridge = nullptr;  // Prevent double-free in TearDown
}

/* ============================================================================
 * UPDATE TESTS - EACH SLEEP STATE
 * ========================================================================== */

TEST_F(ReasoningSleepBridgeTest, AwakeState) {
    CreateBridge();

    ValidateEffectsForState(
        SLEEP_STATE_AWAKE,
        REASONING_SLEEP_LOGICAL_AWAKE,    // 1.0
        REASONING_SLEEP_CREATIVE_AWAKE,   // 1.0
        REASONING_SLEEP_SPEED_AWAKE,      // 1.0
        false,  // not offline
        false   // no REM boost
    );
}

TEST_F(ReasoningSleepBridgeTest, DrowsyState) {
    CreateBridge();

    ValidateEffectsForState(
        SLEEP_STATE_DROWSY,
        REASONING_SLEEP_LOGICAL_DROWSY,   // 0.7
        REASONING_SLEEP_CREATIVE_DROWSY,  // 1.1
        REASONING_SLEEP_SPEED_DROWSY,     // 0.6
        false,  // not offline
        false   // no REM boost
    );
}

TEST_F(ReasoningSleepBridgeTest, LightNREMState) {
    CreateBridge();

    ValidateEffectsForState(
        SLEEP_STATE_LIGHT_NREM,
        REASONING_SLEEP_LOGICAL_LIGHT_NREM,   // 0.3
        REASONING_SLEEP_CREATIVE_LIGHT_NREM,  // 0.8
        REASONING_SLEEP_SPEED_LIGHT_NREM,     // 0.2
        true,   // offline
        false   // no REM boost
    );
}

TEST_F(ReasoningSleepBridgeTest, DeepNREMState) {
    CreateBridge();

    ValidateEffectsForState(
        SLEEP_STATE_DEEP_NREM,
        REASONING_SLEEP_LOGICAL_DEEP_NREM,   // 0.1
        REASONING_SLEEP_CREATIVE_DEEP_NREM,  // 0.5
        REASONING_SLEEP_SPEED_DEEP_NREM,     // 0.1
        true,   // offline
        false   // no REM boost
    );
}

TEST_F(ReasoningSleepBridgeTest, REMState) {
    CreateBridge();

    ValidateEffectsForState(
        SLEEP_STATE_REM,
        REASONING_SLEEP_LOGICAL_REM,      // 0.4
        REASONING_SLEEP_CREATIVE_REM,     // 1.5
        REASONING_SLEEP_SPEED_REM,        // 0.8
        false,  // not offline (REM is active)
        true    // REM creativity boost
    );
}

TEST_F(ReasoningSleepBridgeTest, StateTransitions) {
    CreateBridge();

    // Cycle through all states
    sleep_state_t states[] = {
        SLEEP_STATE_AWAKE,
        SLEEP_STATE_DROWSY,
        SLEEP_STATE_LIGHT_NREM,
        SLEEP_STATE_DEEP_NREM,
        SLEEP_STATE_REM,
        SLEEP_STATE_AWAKE
    };

    for (auto state : states) {
        SetSleepState(state);

        reasoning_sleep_effects_t effects;
        ASSERT_EQ(0, reasoning_sleep_get_effects(bridge, &effects));
        EXPECT_EQ(state, effects.current_state);
    }
}

TEST_F(ReasoningSleepBridgeTest, UpdateNullBridge) {
    int result = reasoning_sleep_update(nullptr);
    EXPECT_NE(0, result);
}

/* ============================================================================
 * EFFECTS RETRIEVAL TESTS
 * ========================================================================== */

TEST_F(ReasoningSleepBridgeTest, GetEffectsValid) {
    CreateBridge();
    SetSleepState(SLEEP_STATE_DROWSY);

    reasoning_sleep_effects_t effects;
    int result = reasoning_sleep_get_effects(bridge, &effects);

    EXPECT_EQ(0, result);
    EXPECT_EQ(SLEEP_STATE_DROWSY, effects.current_state);
    EXPECT_FLOAT_EQ(REASONING_SLEEP_LOGICAL_DROWSY, effects.logical_reasoning_factor);
}

TEST_F(ReasoningSleepBridgeTest, GetEffectsNullBridge) {
    reasoning_sleep_effects_t effects;
    int result = reasoning_sleep_get_effects(nullptr, &effects);
    EXPECT_NE(0, result);
}

TEST_F(ReasoningSleepBridgeTest, GetEffectsNullOutput) {
    CreateBridge();
    int result = reasoning_sleep_get_effects(bridge, nullptr);
    EXPECT_NE(0, result);
}

TEST_F(ReasoningSleepBridgeTest, GetEffectsNullBoth) {
    int result = reasoning_sleep_get_effects(nullptr, nullptr);
    EXPECT_NE(0, result);
}

/* ============================================================================
 * INDIVIDUAL FACTOR QUERY TESTS
 * ========================================================================== */

TEST_F(ReasoningSleepBridgeTest, GetLogicalFactorValid) {
    CreateBridge();

    // Test each state
    SetSleepState(SLEEP_STATE_AWAKE);
    EXPECT_FLOAT_EQ(REASONING_SLEEP_LOGICAL_AWAKE,
                    reasoning_sleep_get_logical_factor(bridge));

    SetSleepState(SLEEP_STATE_DROWSY);
    EXPECT_FLOAT_EQ(REASONING_SLEEP_LOGICAL_DROWSY,
                    reasoning_sleep_get_logical_factor(bridge));

    SetSleepState(SLEEP_STATE_DEEP_NREM);
    EXPECT_FLOAT_EQ(REASONING_SLEEP_LOGICAL_DEEP_NREM,
                    reasoning_sleep_get_logical_factor(bridge));
}

TEST_F(ReasoningSleepBridgeTest, GetLogicalFactorNullBridge) {
    float result = reasoning_sleep_get_logical_factor(nullptr);
    EXPECT_FLOAT_EQ(1.0f, result);  // Default on error
}

TEST_F(ReasoningSleepBridgeTest, GetCreativeFactorValid) {
    CreateBridge();

    // Test states with different creative factors
    SetSleepState(SLEEP_STATE_AWAKE);
    EXPECT_FLOAT_EQ(REASONING_SLEEP_CREATIVE_AWAKE,
                    reasoning_sleep_get_creative_factor(bridge));

    SetSleepState(SLEEP_STATE_DROWSY);
    EXPECT_FLOAT_EQ(REASONING_SLEEP_CREATIVE_DROWSY,
                    reasoning_sleep_get_creative_factor(bridge));

    SetSleepState(SLEEP_STATE_REM);
    EXPECT_FLOAT_EQ(REASONING_SLEEP_CREATIVE_REM,
                    reasoning_sleep_get_creative_factor(bridge));
}

TEST_F(ReasoningSleepBridgeTest, GetCreativeFactorNullBridge) {
    float result = reasoning_sleep_get_creative_factor(nullptr);
    EXPECT_FLOAT_EQ(1.0f, result);  // Default on error
}

/* ============================================================================
 * STATE CHECK TESTS
 * ========================================================================== */

TEST_F(ReasoningSleepBridgeTest, IsOfflineAwake) {
    CreateBridge();
    SetSleepState(SLEEP_STATE_AWAKE);
    EXPECT_FALSE(reasoning_sleep_is_offline(bridge));
}

TEST_F(ReasoningSleepBridgeTest, IsOfflineDrowsy) {
    CreateBridge();
    SetSleepState(SLEEP_STATE_DROWSY);
    EXPECT_FALSE(reasoning_sleep_is_offline(bridge));
}

TEST_F(ReasoningSleepBridgeTest, IsOfflineLightNREM) {
    CreateBridge();
    SetSleepState(SLEEP_STATE_LIGHT_NREM);
    EXPECT_TRUE(reasoning_sleep_is_offline(bridge));
}

TEST_F(ReasoningSleepBridgeTest, IsOfflineDeepNREM) {
    CreateBridge();
    SetSleepState(SLEEP_STATE_DEEP_NREM);
    EXPECT_TRUE(reasoning_sleep_is_offline(bridge));
}

TEST_F(ReasoningSleepBridgeTest, IsOfflineREM) {
    CreateBridge();
    SetSleepState(SLEEP_STATE_REM);
    EXPECT_FALSE(reasoning_sleep_is_offline(bridge));  // REM is active
}

TEST_F(ReasoningSleepBridgeTest, IsOfflineNullBridge) {
    EXPECT_FALSE(reasoning_sleep_is_offline(nullptr));
}

TEST_F(ReasoningSleepBridgeTest, IsRemCreativeAwake) {
    CreateBridge();
    SetSleepState(SLEEP_STATE_AWAKE);
    EXPECT_FALSE(reasoning_sleep_is_rem_creative(bridge));
}

TEST_F(ReasoningSleepBridgeTest, IsRemCreativeNREM) {
    CreateBridge();
    SetSleepState(SLEEP_STATE_DEEP_NREM);
    EXPECT_FALSE(reasoning_sleep_is_rem_creative(bridge));
}

TEST_F(ReasoningSleepBridgeTest, IsRemCreativeREM) {
    CreateBridge();
    SetSleepState(SLEEP_STATE_REM);
    EXPECT_TRUE(reasoning_sleep_is_rem_creative(bridge));
}

TEST_F(ReasoningSleepBridgeTest, IsRemCreativeNullBridge) {
    EXPECT_FALSE(reasoning_sleep_is_rem_creative(nullptr));
}

/* ============================================================================
 * UTILITY FUNCTION TESTS
 * ========================================================================== */

TEST_F(ReasoningSleepBridgeTest, LogicalForStateAllStates) {
    EXPECT_FLOAT_EQ(REASONING_SLEEP_LOGICAL_AWAKE,
                    reasoning_sleep_logical_for_state(SLEEP_STATE_AWAKE));
    EXPECT_FLOAT_EQ(REASONING_SLEEP_LOGICAL_DROWSY,
                    reasoning_sleep_logical_for_state(SLEEP_STATE_DROWSY));
    EXPECT_FLOAT_EQ(REASONING_SLEEP_LOGICAL_LIGHT_NREM,
                    reasoning_sleep_logical_for_state(SLEEP_STATE_LIGHT_NREM));
    EXPECT_FLOAT_EQ(REASONING_SLEEP_LOGICAL_DEEP_NREM,
                    reasoning_sleep_logical_for_state(SLEEP_STATE_DEEP_NREM));
    EXPECT_FLOAT_EQ(REASONING_SLEEP_LOGICAL_REM,
                    reasoning_sleep_logical_for_state(SLEEP_STATE_REM));
}

TEST_F(ReasoningSleepBridgeTest, CreativeForStateAllStates) {
    EXPECT_FLOAT_EQ(REASONING_SLEEP_CREATIVE_AWAKE,
                    reasoning_sleep_creative_for_state(SLEEP_STATE_AWAKE));
    EXPECT_FLOAT_EQ(REASONING_SLEEP_CREATIVE_DROWSY,
                    reasoning_sleep_creative_for_state(SLEEP_STATE_DROWSY));
    EXPECT_FLOAT_EQ(REASONING_SLEEP_CREATIVE_LIGHT_NREM,
                    reasoning_sleep_creative_for_state(SLEEP_STATE_LIGHT_NREM));
    EXPECT_FLOAT_EQ(REASONING_SLEEP_CREATIVE_DEEP_NREM,
                    reasoning_sleep_creative_for_state(SLEEP_STATE_DEEP_NREM));
    EXPECT_FLOAT_EQ(REASONING_SLEEP_CREATIVE_REM,
                    reasoning_sleep_creative_for_state(SLEEP_STATE_REM));
}

TEST_F(ReasoningSleepBridgeTest, SpeedForStateAllStates) {
    EXPECT_FLOAT_EQ(REASONING_SLEEP_SPEED_AWAKE,
                    reasoning_sleep_speed_for_state(SLEEP_STATE_AWAKE));
    EXPECT_FLOAT_EQ(REASONING_SLEEP_SPEED_DROWSY,
                    reasoning_sleep_speed_for_state(SLEEP_STATE_DROWSY));
    EXPECT_FLOAT_EQ(REASONING_SLEEP_SPEED_LIGHT_NREM,
                    reasoning_sleep_speed_for_state(SLEEP_STATE_LIGHT_NREM));
    EXPECT_FLOAT_EQ(REASONING_SLEEP_SPEED_DEEP_NREM,
                    reasoning_sleep_speed_for_state(SLEEP_STATE_DEEP_NREM));
    EXPECT_FLOAT_EQ(REASONING_SLEEP_SPEED_REM,
                    reasoning_sleep_speed_for_state(SLEEP_STATE_REM));
}

/* ============================================================================
 * WORKING MEMORY ACCESS TESTS
 * ========================================================================== */

TEST_F(ReasoningSleepBridgeTest, WorkingMemoryAccessAwake) {
    CreateBridge();
    SetSleepState(SLEEP_STATE_AWAKE);

    reasoning_sleep_effects_t effects;
    ASSERT_EQ(0, reasoning_sleep_get_effects(bridge, &effects));

    // Full access when awake
    EXPECT_FLOAT_EQ(1.0f, effects.working_memory_access_factor);
}

TEST_F(ReasoningSleepBridgeTest, WorkingMemoryAccessDrowsy) {
    CreateBridge();
    SetSleepState(SLEEP_STATE_DROWSY);

    reasoning_sleep_effects_t effects;
    ASSERT_EQ(0, reasoning_sleep_get_effects(bridge, &effects));

    // Reduced but non-zero when drowsy
    EXPECT_GT(effects.working_memory_access_factor, 0.0f);
    EXPECT_LT(effects.working_memory_access_factor, 1.0f);
}

TEST_F(ReasoningSleepBridgeTest, WorkingMemoryAccessNREM) {
    CreateBridge();
    SetSleepState(SLEEP_STATE_DEEP_NREM);

    reasoning_sleep_effects_t effects;
    ASSERT_EQ(0, reasoning_sleep_get_effects(bridge, &effects));

    // Minimal access during NREM
    EXPECT_LE(effects.working_memory_access_factor, 0.3f);
}

TEST_F(ReasoningSleepBridgeTest, WorkingMemoryAccessREM) {
    CreateBridge();
    SetSleepState(SLEEP_STATE_REM);

    reasoning_sleep_effects_t effects;
    ASSERT_EQ(0, reasoning_sleep_get_effects(bridge, &effects));

    // Some access during REM (for dream generation)
    EXPECT_GT(effects.working_memory_access_factor, 0.0f);
}

/* ============================================================================
 * SLEEP PRESSURE INTEGRATION TESTS
 * ========================================================================== */

TEST_F(ReasoningSleepBridgeTest, SleepPressureTracking) {
    CreateBridge();

    // Accumulate sleep pressure
    sleep_accumulate_pressure(sleep_system, 1000);

    // Update bridge
    ASSERT_EQ(0, reasoning_sleep_update(bridge));

    reasoning_sleep_effects_t effects;
    ASSERT_EQ(0, reasoning_sleep_get_effects(bridge, &effects));

    // Sleep pressure should be captured
    EXPECT_GE(effects.sleep_pressure, 0.0f);
    EXPECT_LE(effects.sleep_pressure, 1.0f);
}

TEST_F(ReasoningSleepBridgeTest, HighSleepPressureAwake) {
    CreateBridge();

    // Build up high sleep pressure
    sleep_accumulate_pressure(sleep_system, 10000);
    ASSERT_EQ(0, reasoning_sleep_update(bridge));

    reasoning_sleep_effects_t effects;
    ASSERT_EQ(0, reasoning_sleep_get_effects(bridge, &effects));

    // Even when awake, high pressure is captured
    EXPECT_GT(effects.sleep_pressure, 0.5f);
}

/* ============================================================================
 * EDGE CASE TESTS
 * ========================================================================== */

TEST_F(ReasoningSleepBridgeTest, RapidStateChanges) {
    CreateBridge();

    // Rapidly cycle through states
    sleep_state_t states[] = {
        SLEEP_STATE_AWAKE, SLEEP_STATE_REM, SLEEP_STATE_AWAKE,
        SLEEP_STATE_DROWSY, SLEEP_STATE_DEEP_NREM, SLEEP_STATE_AWAKE
    };

    for (int i = 0; i < 10; i++) {
        for (auto state : states) {
            SetSleepState(state);

            reasoning_sleep_effects_t effects;
            ASSERT_EQ(0, reasoning_sleep_get_effects(bridge, &effects));
            EXPECT_EQ(state, effects.current_state);
        }
    }
}

TEST_F(ReasoningSleepBridgeTest, ModulationStrengthZero) {
    reasoning_sleep_config_t config;
    reasoning_sleep_default_config(&config);
    config.modulation_strength = 0.0f;

    CreateSleepSystem();
    bridge = reasoning_sleep_bridge_create(&config, sleep_system);
    ASSERT_NE(nullptr, bridge);

    // With zero modulation strength, effects should be minimal
    SetSleepState(SLEEP_STATE_DEEP_NREM);

    reasoning_sleep_effects_t effects;
    ASSERT_EQ(0, reasoning_sleep_get_effects(bridge, &effects));

    // Factors should be closer to baseline (less modulation)
    EXPECT_GT(effects.logical_reasoning_factor,
              REASONING_SLEEP_LOGICAL_DEEP_NREM * 0.5f);
}

TEST_F(ReasoningSleepBridgeTest, AllModulationsDisabled) {
    reasoning_sleep_config_t config;
    reasoning_sleep_default_config(&config);
    config.enable_logical_modulation = false;
    config.enable_creative_modulation = false;
    config.enable_speed_modulation = false;
    config.enable_working_memory_gating = false;

    CreateSleepSystem();
    bridge = reasoning_sleep_bridge_create(&config, sleep_system);
    ASSERT_NE(nullptr, bridge);

    SetSleepState(SLEEP_STATE_DEEP_NREM);

    reasoning_sleep_effects_t effects;
    ASSERT_EQ(0, reasoning_sleep_get_effects(bridge, &effects));

    // With all modulations disabled, factors should be near baseline
    EXPECT_FLOAT_EQ(1.0f, effects.logical_reasoning_factor);
    EXPECT_FLOAT_EQ(1.0f, effects.creative_reasoning_factor);
    EXPECT_FLOAT_EQ(1.0f, effects.inference_speed_factor);
    EXPECT_FLOAT_EQ(1.0f, effects.working_memory_access_factor);
}

TEST_F(ReasoningSleepBridgeTest, SelectiveModulation) {
    reasoning_sleep_config_t config;
    reasoning_sleep_default_config(&config);
    config.enable_logical_modulation = true;
    config.enable_creative_modulation = false;
    config.enable_speed_modulation = false;
    config.enable_working_memory_gating = false;

    CreateSleepSystem();
    bridge = reasoning_sleep_bridge_create(&config, sleep_system);
    ASSERT_NE(nullptr, bridge);

    SetSleepState(SLEEP_STATE_DEEP_NREM);

    reasoning_sleep_effects_t effects;
    ASSERT_EQ(0, reasoning_sleep_get_effects(bridge, &effects));

    // Only logical should be modulated
    EXPECT_FLOAT_EQ(REASONING_SLEEP_LOGICAL_DEEP_NREM,
                    effects.logical_reasoning_factor);
    EXPECT_FLOAT_EQ(1.0f, effects.creative_reasoning_factor);
    EXPECT_FLOAT_EQ(1.0f, effects.inference_speed_factor);
    EXPECT_FLOAT_EQ(1.0f, effects.working_memory_access_factor);
}

/* ============================================================================
 * BIOLOGICAL CONSISTENCY TESTS
 * ========================================================================== */

TEST_F(ReasoningSleepBridgeTest, CreativeReasoningEnhancedInREM) {
    CreateBridge();

    // Get awake baseline
    SetSleepState(SLEEP_STATE_AWAKE);
    float awake_creative = reasoning_sleep_get_creative_factor(bridge);

    // Get REM creative
    SetSleepState(SLEEP_STATE_REM);
    float rem_creative = reasoning_sleep_get_creative_factor(bridge);

    // REM should enhance creative reasoning
    EXPECT_GT(rem_creative, awake_creative);
}

TEST_F(ReasoningSleepBridgeTest, LogicalReasoningImpairedInREM) {
    CreateBridge();

    // Get awake baseline
    SetSleepState(SLEEP_STATE_AWAKE);
    float awake_logical = reasoning_sleep_get_logical_factor(bridge);

    // Get REM logical
    SetSleepState(SLEEP_STATE_REM);
    float rem_logical = reasoning_sleep_get_logical_factor(bridge);

    // REM should impair logical reasoning (dream logic)
    EXPECT_LT(rem_logical, awake_logical);
}

TEST_F(ReasoningSleepBridgeTest, DeepNREMMinimalActivity) {
    CreateBridge();
    SetSleepState(SLEEP_STATE_DEEP_NREM);

    reasoning_sleep_effects_t effects;
    ASSERT_EQ(0, reasoning_sleep_get_effects(bridge, &effects));

    // Deep NREM should have minimal reasoning activity
    EXPECT_LE(effects.logical_reasoning_factor, 0.2f);
    EXPECT_LE(effects.creative_reasoning_factor, 0.6f);
    EXPECT_LE(effects.inference_speed_factor, 0.2f);
    EXPECT_TRUE(effects.reasoning_offline);
}

TEST_F(ReasoningSleepBridgeTest, DrowsySlightlyEnhancesCreativity) {
    CreateBridge();

    // Get awake baseline
    SetSleepState(SLEEP_STATE_AWAKE);
    float awake_creative = reasoning_sleep_get_creative_factor(bridge);

    // Get drowsy creative
    SetSleepState(SLEEP_STATE_DROWSY);
    float drowsy_creative = reasoning_sleep_get_creative_factor(bridge);

    // Drowsy should slightly enhance creativity (relaxed constraints)
    EXPECT_GE(drowsy_creative, awake_creative);
}

/* ============================================================================
 * MAIN
 * ========================================================================== */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
