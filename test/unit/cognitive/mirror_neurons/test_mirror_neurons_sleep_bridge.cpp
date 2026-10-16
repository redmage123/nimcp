/**
 * @file test_mirror_neurons_sleep_bridge.cpp
 * @brief Unit tests for Mirror Neurons Sleep Bridge
 *
 * WHAT: Comprehensive tests for mirror neurons sleep integration
 * WHY:  Ensure mirror neuron activity is correctly modulated by sleep states
 * HOW:  Test lifecycle, sleep state effects, modulation factors, and bio-async
 *
 * Test Coverage:
 * - Default config initialization
 * - Bridge creation and destruction
 * - Update function for all sleep states (AWAKE, DROWSY, LIGHT_NREM, DEEP_NREM, REM)
 * - Effects retrieval and validation
 * - State-specific factor queries
 * - Replay detection
 * - Bio-async connection
 * - NULL pointer handling
 * - Edge cases and boundary conditions
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

// Headers have their own extern "C" guards
#include "cognitive/mirror_neurons/nimcp_mirror_neurons_sleep_bridge.h"
#include "cognitive/nimcp_sleep_wake.h"

// Test tolerance for floating point comparisons
#define FLOAT_EPSILON 1e-6f

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class MirrorNeuronsSleepBridgeTest : public ::testing::Test {
protected:
    sleep_system_t sleep = nullptr;
    mirror_neurons_sleep_bridge_t bridge = nullptr;

    void SetUp() override {
        // Create sleep system with default config
        sleep_config_t sleep_config = sleep_default_config();
        sleep = sleep_system_create(&sleep_config);
        ASSERT_NE(sleep, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            mirror_neurons_sleep_bridge_destroy(bridge);
            bridge = nullptr;
        }
        if (sleep) {
            sleep_system_destroy(sleep);
            sleep = nullptr;
        }
    }

    // Helper to compare floats
    bool floatEquals(float a, float b) {
        return std::fabs(a - b) < FLOAT_EPSILON;
    }
};

/* ============================================================================
 * Configuration Tests
 * ============================================================================ */

TEST_F(MirrorNeuronsSleepBridgeTest, DefaultConfig) {
    mirror_neurons_sleep_config_t config;
    int ret = mirror_neurons_sleep_default_config(&config);

    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(config.enable_activity_modulation);
    EXPECT_TRUE(config.enable_empathy_modulation);
    EXPECT_TRUE(config.enable_observation_modulation);
    EXPECT_TRUE(config.enable_replay_modulation);
    EXPECT_GT(config.modulation_strength, 0.0f);
    EXPECT_LE(config.modulation_strength, 1.0f);
}

TEST_F(MirrorNeuronsSleepBridgeTest, DefaultConfigNull) {
    int ret = mirror_neurons_sleep_default_config(nullptr);
    EXPECT_EQ(ret, -1);
}

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(MirrorNeuronsSleepBridgeTest, CreateWithDefaultConfig) {
    bridge = mirror_neurons_sleep_bridge_create(nullptr, sleep);
    ASSERT_NE(bridge, nullptr);
}

TEST_F(MirrorNeuronsSleepBridgeTest, CreateWithCustomConfig) {
    mirror_neurons_sleep_config_t config;
    mirror_neurons_sleep_default_config(&config);
    config.modulation_strength = 0.5f;
    config.enable_replay_modulation = false;

    bridge = mirror_neurons_sleep_bridge_create(&config, sleep);
    ASSERT_NE(bridge, nullptr);
}

TEST_F(MirrorNeuronsSleepBridgeTest, CreateNullSleep) {
    bridge = mirror_neurons_sleep_bridge_create(nullptr, nullptr);
    EXPECT_EQ(bridge, nullptr);
}

TEST_F(MirrorNeuronsSleepBridgeTest, DestroyNull) {
    // Should not crash
    mirror_neurons_sleep_bridge_destroy(nullptr);
}

TEST_F(MirrorNeuronsSleepBridgeTest, DestroyValid) {
    bridge = mirror_neurons_sleep_bridge_create(nullptr, sleep);
    ASSERT_NE(bridge, nullptr);

    mirror_neurons_sleep_bridge_destroy(bridge);
    bridge = nullptr; // Don't double-destroy in TearDown
}

/* ============================================================================
 * Update Tests - Sleep State Effects
 * ============================================================================ */

TEST_F(MirrorNeuronsSleepBridgeTest, UpdateAwakeState) {
    bridge = mirror_neurons_sleep_bridge_create(nullptr, sleep);
    ASSERT_NE(bridge, nullptr);

    // Set sleep to AWAKE state
    sleep_enter_state(sleep, SLEEP_STATE_AWAKE);

    int ret = mirror_neurons_sleep_update(bridge);
    EXPECT_EQ(ret, 0);

    mirror_neurons_sleep_effects_t effects;
    ret = mirror_neurons_sleep_get_effects(bridge, &effects);
    EXPECT_EQ(ret, 0);

    // Verify AWAKE state effects
    EXPECT_EQ(effects.current_state, SLEEP_STATE_AWAKE);
    EXPECT_TRUE(floatEquals(effects.mirroring_activity_factor, MIRROR_SLEEP_ACTIVITY_AWAKE));
    EXPECT_TRUE(floatEquals(effects.empathy_modulation_factor, MIRROR_SLEEP_EMPATHY_AWAKE));
    EXPECT_TRUE(floatEquals(effects.action_observation_factor, MIRROR_SLEEP_OBSERVATION_AWAKE));
    EXPECT_TRUE(floatEquals(effects.action_replay_factor, MIRROR_SLEEP_REPLAY_AWAKE));
    EXPECT_FALSE(effects.replay_active); // Replay threshold should be > 0.1
}

TEST_F(MirrorNeuronsSleepBridgeTest, UpdateDrowsyState) {
    bridge = mirror_neurons_sleep_bridge_create(nullptr, sleep);
    ASSERT_NE(bridge, nullptr);

    sleep_enter_state(sleep, SLEEP_STATE_DROWSY);

    int ret = mirror_neurons_sleep_update(bridge);
    EXPECT_EQ(ret, 0);

    mirror_neurons_sleep_effects_t effects;
    ret = mirror_neurons_sleep_get_effects(bridge, &effects);
    EXPECT_EQ(ret, 0);

    // Verify DROWSY state effects
    EXPECT_EQ(effects.current_state, SLEEP_STATE_DROWSY);
    EXPECT_TRUE(floatEquals(effects.mirroring_activity_factor, MIRROR_SLEEP_ACTIVITY_DROWSY));
    EXPECT_TRUE(floatEquals(effects.empathy_modulation_factor, MIRROR_SLEEP_EMPATHY_DROWSY));
    EXPECT_TRUE(floatEquals(effects.action_observation_factor, MIRROR_SLEEP_OBSERVATION_DROWSY));
    EXPECT_TRUE(floatEquals(effects.action_replay_factor, MIRROR_SLEEP_REPLAY_DROWSY));
    EXPECT_FALSE(effects.replay_active); // Replay threshold should be > 0.2
}

TEST_F(MirrorNeuronsSleepBridgeTest, UpdateLightNremState) {
    bridge = mirror_neurons_sleep_bridge_create(nullptr, sleep);
    ASSERT_NE(bridge, nullptr);

    sleep_enter_state(sleep, SLEEP_STATE_LIGHT_NREM);

    int ret = mirror_neurons_sleep_update(bridge);
    EXPECT_EQ(ret, 0);

    mirror_neurons_sleep_effects_t effects;
    ret = mirror_neurons_sleep_get_effects(bridge, &effects);
    EXPECT_EQ(ret, 0);

    // Verify LIGHT_NREM state effects
    EXPECT_EQ(effects.current_state, SLEEP_STATE_LIGHT_NREM);
    EXPECT_TRUE(floatEquals(effects.mirroring_activity_factor, MIRROR_SLEEP_ACTIVITY_LIGHT_NREM));
    EXPECT_TRUE(floatEquals(effects.empathy_modulation_factor, MIRROR_SLEEP_EMPATHY_LIGHT_NREM));
    EXPECT_TRUE(floatEquals(effects.action_observation_factor, MIRROR_SLEEP_OBSERVATION_LIGHT_NREM));
    EXPECT_TRUE(floatEquals(effects.action_replay_factor, MIRROR_SLEEP_REPLAY_LIGHT_NREM));
    EXPECT_TRUE(effects.replay_active); // Replay factor is 0.5
}

TEST_F(MirrorNeuronsSleepBridgeTest, UpdateDeepNremState) {
    bridge = mirror_neurons_sleep_bridge_create(nullptr, sleep);
    ASSERT_NE(bridge, nullptr);

    sleep_enter_state(sleep, SLEEP_STATE_DEEP_NREM);

    int ret = mirror_neurons_sleep_update(bridge);
    EXPECT_EQ(ret, 0);

    mirror_neurons_sleep_effects_t effects;
    ret = mirror_neurons_sleep_get_effects(bridge, &effects);
    EXPECT_EQ(ret, 0);

    // Verify DEEP_NREM state effects
    EXPECT_EQ(effects.current_state, SLEEP_STATE_DEEP_NREM);
    EXPECT_TRUE(floatEquals(effects.mirroring_activity_factor, MIRROR_SLEEP_ACTIVITY_DEEP_NREM));
    EXPECT_TRUE(floatEquals(effects.empathy_modulation_factor, MIRROR_SLEEP_EMPATHY_DEEP_NREM));
    EXPECT_TRUE(floatEquals(effects.action_observation_factor, MIRROR_SLEEP_OBSERVATION_DEEP_NREM));
    EXPECT_TRUE(floatEquals(effects.action_replay_factor, MIRROR_SLEEP_REPLAY_DEEP_NREM));
    EXPECT_TRUE(effects.replay_active); // Replay factor is 0.3
}

TEST_F(MirrorNeuronsSleepBridgeTest, UpdateRemState) {
    bridge = mirror_neurons_sleep_bridge_create(nullptr, sleep);
    ASSERT_NE(bridge, nullptr);

    sleep_enter_state(sleep, SLEEP_STATE_REM);

    int ret = mirror_neurons_sleep_update(bridge);
    EXPECT_EQ(ret, 0);

    mirror_neurons_sleep_effects_t effects;
    ret = mirror_neurons_sleep_get_effects(bridge, &effects);
    EXPECT_EQ(ret, 0);

    // Verify REM state effects
    EXPECT_EQ(effects.current_state, SLEEP_STATE_REM);
    EXPECT_TRUE(floatEquals(effects.mirroring_activity_factor, MIRROR_SLEEP_ACTIVITY_REM));
    EXPECT_TRUE(floatEquals(effects.empathy_modulation_factor, MIRROR_SLEEP_EMPATHY_REM));
    EXPECT_TRUE(floatEquals(effects.action_observation_factor, MIRROR_SLEEP_OBSERVATION_REM));
    EXPECT_TRUE(floatEquals(effects.action_replay_factor, MIRROR_SLEEP_REPLAY_REM));
    EXPECT_TRUE(effects.replay_active); // Replay factor is 1.0 (maximum)
}

TEST_F(MirrorNeuronsSleepBridgeTest, UpdateNullBridge) {
    int ret = mirror_neurons_sleep_update(nullptr);
    EXPECT_EQ(ret, -1);
}

/* ============================================================================
 * Effects Retrieval Tests
 * ============================================================================ */

TEST_F(MirrorNeuronsSleepBridgeTest, GetEffectsValid) {
    bridge = mirror_neurons_sleep_bridge_create(nullptr, sleep);
    ASSERT_NE(bridge, nullptr);

    sleep_enter_state(sleep, SLEEP_STATE_AWAKE);
    mirror_neurons_sleep_update(bridge);

    mirror_neurons_sleep_effects_t effects;
    int ret = mirror_neurons_sleep_get_effects(bridge, &effects);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(effects.current_state, SLEEP_STATE_AWAKE);
    EXPECT_GE(effects.mirroring_activity_factor, 0.0f);
    EXPECT_LE(effects.mirroring_activity_factor, 1.0f);
    EXPECT_GE(effects.sleep_pressure, 0.0f);
    EXPECT_LE(effects.sleep_pressure, 1.0f);
}

TEST_F(MirrorNeuronsSleepBridgeTest, GetEffectsNullBridge) {
    mirror_neurons_sleep_effects_t effects;
    int ret = mirror_neurons_sleep_get_effects(nullptr, &effects);
    EXPECT_EQ(ret, -1);
}

TEST_F(MirrorNeuronsSleepBridgeTest, GetEffectsNullEffects) {
    bridge = mirror_neurons_sleep_bridge_create(nullptr, sleep);
    ASSERT_NE(bridge, nullptr);

    int ret = mirror_neurons_sleep_get_effects(bridge, nullptr);
    EXPECT_EQ(ret, -1);
}

TEST_F(MirrorNeuronsSleepBridgeTest, GetActivityValid) {
    bridge = mirror_neurons_sleep_bridge_create(nullptr, sleep);
    ASSERT_NE(bridge, nullptr);

    sleep_enter_state(sleep, SLEEP_STATE_AWAKE);
    mirror_neurons_sleep_update(bridge);

    float activity = mirror_neurons_sleep_get_activity(bridge);
    EXPECT_TRUE(floatEquals(activity, MIRROR_SLEEP_ACTIVITY_AWAKE));
}

TEST_F(MirrorNeuronsSleepBridgeTest, GetActivityNull) {
    float activity = mirror_neurons_sleep_get_activity(nullptr);
    EXPECT_TRUE(floatEquals(activity, 1.0f)); // Default value per spec
}

/* ============================================================================
 * Replay Detection Tests
 * ============================================================================ */

TEST_F(MirrorNeuronsSleepBridgeTest, ReplayActiveInRem) {
    bridge = mirror_neurons_sleep_bridge_create(nullptr, sleep);
    ASSERT_NE(bridge, nullptr);

    sleep_enter_state(sleep, SLEEP_STATE_REM);
    mirror_neurons_sleep_update(bridge);

    bool replay = mirror_neurons_sleep_is_replay_active(bridge);
    EXPECT_TRUE(replay); // REM has replay factor of 1.0
}

TEST_F(MirrorNeuronsSleepBridgeTest, ReplayActiveInLightNrem) {
    bridge = mirror_neurons_sleep_bridge_create(nullptr, sleep);
    ASSERT_NE(bridge, nullptr);

    sleep_enter_state(sleep, SLEEP_STATE_LIGHT_NREM);
    mirror_neurons_sleep_update(bridge);

    bool replay = mirror_neurons_sleep_is_replay_active(bridge);
    EXPECT_TRUE(replay); // Light NREM has replay factor of 0.5
}

TEST_F(MirrorNeuronsSleepBridgeTest, ReplayInactiveWhileAwake) {
    bridge = mirror_neurons_sleep_bridge_create(nullptr, sleep);
    ASSERT_NE(bridge, nullptr);

    sleep_enter_state(sleep, SLEEP_STATE_AWAKE);
    mirror_neurons_sleep_update(bridge);

    bool replay = mirror_neurons_sleep_is_replay_active(bridge);
    EXPECT_FALSE(replay); // Awake has low replay factor of 0.1
}

TEST_F(MirrorNeuronsSleepBridgeTest, ReplayNull) {
    bool replay = mirror_neurons_sleep_is_replay_active(nullptr);
    EXPECT_FALSE(replay);
}

/* ============================================================================
 * State-Specific Factor Query Tests
 * ============================================================================ */

TEST_F(MirrorNeuronsSleepBridgeTest, ActivityFactorForAllStates) {
    EXPECT_TRUE(floatEquals(
        mirror_neurons_sleep_activity_for_state(SLEEP_STATE_AWAKE),
        MIRROR_SLEEP_ACTIVITY_AWAKE));
    EXPECT_TRUE(floatEquals(
        mirror_neurons_sleep_activity_for_state(SLEEP_STATE_DROWSY),
        MIRROR_SLEEP_ACTIVITY_DROWSY));
    EXPECT_TRUE(floatEquals(
        mirror_neurons_sleep_activity_for_state(SLEEP_STATE_LIGHT_NREM),
        MIRROR_SLEEP_ACTIVITY_LIGHT_NREM));
    EXPECT_TRUE(floatEquals(
        mirror_neurons_sleep_activity_for_state(SLEEP_STATE_DEEP_NREM),
        MIRROR_SLEEP_ACTIVITY_DEEP_NREM));
    EXPECT_TRUE(floatEquals(
        mirror_neurons_sleep_activity_for_state(SLEEP_STATE_REM),
        MIRROR_SLEEP_ACTIVITY_REM));
}

TEST_F(MirrorNeuronsSleepBridgeTest, EmpathyFactorForAllStates) {
    EXPECT_TRUE(floatEquals(
        mirror_neurons_sleep_empathy_for_state(SLEEP_STATE_AWAKE),
        MIRROR_SLEEP_EMPATHY_AWAKE));
    EXPECT_TRUE(floatEquals(
        mirror_neurons_sleep_empathy_for_state(SLEEP_STATE_DROWSY),
        MIRROR_SLEEP_EMPATHY_DROWSY));
    EXPECT_TRUE(floatEquals(
        mirror_neurons_sleep_empathy_for_state(SLEEP_STATE_LIGHT_NREM),
        MIRROR_SLEEP_EMPATHY_LIGHT_NREM));
    EXPECT_TRUE(floatEquals(
        mirror_neurons_sleep_empathy_for_state(SLEEP_STATE_DEEP_NREM),
        MIRROR_SLEEP_EMPATHY_DEEP_NREM));
    EXPECT_TRUE(floatEquals(
        mirror_neurons_sleep_empathy_for_state(SLEEP_STATE_REM),
        MIRROR_SLEEP_EMPATHY_REM));
}

TEST_F(MirrorNeuronsSleepBridgeTest, ObservationFactorForAllStates) {
    EXPECT_TRUE(floatEquals(
        mirror_neurons_sleep_observation_for_state(SLEEP_STATE_AWAKE),
        MIRROR_SLEEP_OBSERVATION_AWAKE));
    EXPECT_TRUE(floatEquals(
        mirror_neurons_sleep_observation_for_state(SLEEP_STATE_DROWSY),
        MIRROR_SLEEP_OBSERVATION_DROWSY));
    EXPECT_TRUE(floatEquals(
        mirror_neurons_sleep_observation_for_state(SLEEP_STATE_LIGHT_NREM),
        MIRROR_SLEEP_OBSERVATION_LIGHT_NREM));
    EXPECT_TRUE(floatEquals(
        mirror_neurons_sleep_observation_for_state(SLEEP_STATE_DEEP_NREM),
        MIRROR_SLEEP_OBSERVATION_DEEP_NREM));
    EXPECT_TRUE(floatEquals(
        mirror_neurons_sleep_observation_for_state(SLEEP_STATE_REM),
        MIRROR_SLEEP_OBSERVATION_REM));
}

TEST_F(MirrorNeuronsSleepBridgeTest, ReplayFactorForAllStates) {
    EXPECT_TRUE(floatEquals(
        mirror_neurons_sleep_replay_for_state(SLEEP_STATE_AWAKE),
        MIRROR_SLEEP_REPLAY_AWAKE));
    EXPECT_TRUE(floatEquals(
        mirror_neurons_sleep_replay_for_state(SLEEP_STATE_DROWSY),
        MIRROR_SLEEP_REPLAY_DROWSY));
    EXPECT_TRUE(floatEquals(
        mirror_neurons_sleep_replay_for_state(SLEEP_STATE_LIGHT_NREM),
        MIRROR_SLEEP_REPLAY_LIGHT_NREM));
    EXPECT_TRUE(floatEquals(
        mirror_neurons_sleep_replay_for_state(SLEEP_STATE_DEEP_NREM),
        MIRROR_SLEEP_REPLAY_DEEP_NREM));
    EXPECT_TRUE(floatEquals(
        mirror_neurons_sleep_replay_for_state(SLEEP_STATE_REM),
        MIRROR_SLEEP_REPLAY_REM));
}

/* ============================================================================
 * Sleep State Transition Tests
 * ============================================================================ */

TEST_F(MirrorNeuronsSleepBridgeTest, StateTransitionAwakeToDrowsy) {
    bridge = mirror_neurons_sleep_bridge_create(nullptr, sleep);
    ASSERT_NE(bridge, nullptr);

    sleep_enter_state(sleep, SLEEP_STATE_AWAKE);
    mirror_neurons_sleep_update(bridge);

    float awake_activity = mirror_neurons_sleep_get_activity(bridge);

    sleep_enter_state(sleep, SLEEP_STATE_DROWSY);
    mirror_neurons_sleep_update(bridge);

    float drowsy_activity = mirror_neurons_sleep_get_activity(bridge);

    // Activity should decrease when transitioning to drowsy
    EXPECT_LT(drowsy_activity, awake_activity);
    EXPECT_TRUE(floatEquals(drowsy_activity, MIRROR_SLEEP_ACTIVITY_DROWSY));
}

TEST_F(MirrorNeuronsSleepBridgeTest, StateTransitionDeepNremToRem) {
    bridge = mirror_neurons_sleep_bridge_create(nullptr, sleep);
    ASSERT_NE(bridge, nullptr);

    sleep_enter_state(sleep, SLEEP_STATE_DEEP_NREM);
    mirror_neurons_sleep_update(bridge);

    mirror_neurons_sleep_effects_t effects_deep;
    mirror_neurons_sleep_get_effects(bridge, &effects_deep);

    sleep_enter_state(sleep, SLEEP_STATE_REM);
    mirror_neurons_sleep_update(bridge);

    mirror_neurons_sleep_effects_t effects_rem;
    mirror_neurons_sleep_get_effects(bridge, &effects_rem);

    // REM should have higher activity and replay than deep NREM
    EXPECT_GT(effects_rem.mirroring_activity_factor, effects_deep.mirroring_activity_factor);
    EXPECT_GT(effects_rem.action_replay_factor, effects_deep.action_replay_factor);
    EXPECT_GT(effects_rem.empathy_modulation_factor, effects_deep.empathy_modulation_factor);
}

/* ============================================================================
 * Modulation Strength Tests
 * ============================================================================ */

TEST_F(MirrorNeuronsSleepBridgeTest, ModulationStrengthEffect) {
    mirror_neurons_sleep_config_t config;
    mirror_neurons_sleep_default_config(&config);

    // Test with half strength
    config.modulation_strength = 0.5f;
    bridge = mirror_neurons_sleep_bridge_create(&config, sleep);
    ASSERT_NE(bridge, nullptr);

    sleep_enter_state(sleep, SLEEP_STATE_DEEP_NREM);
    mirror_neurons_sleep_update(bridge);

    mirror_neurons_sleep_effects_t effects;
    mirror_neurons_sleep_get_effects(bridge, &effects);

    // With 50% modulation strength, effects should be between baseline (1.0) and full effect
    // Formula: 1.0 - modulation_strength * (1.0 - base_factor)
    float expected = 1.0f - 0.5f * (1.0f - MIRROR_SLEEP_ACTIVITY_DEEP_NREM);
    EXPECT_GT(effects.mirroring_activity_factor, MIRROR_SLEEP_ACTIVITY_DEEP_NREM);
    EXPECT_LT(effects.mirroring_activity_factor, 1.0f);
}

TEST_F(MirrorNeuronsSleepBridgeTest, ModulationDisabled) {
    mirror_neurons_sleep_config_t config;
    mirror_neurons_sleep_default_config(&config);

    // Disable all modulations
    config.enable_activity_modulation = false;
    config.enable_empathy_modulation = false;
    config.enable_observation_modulation = false;
    config.enable_replay_modulation = false;

    bridge = mirror_neurons_sleep_bridge_create(&config, sleep);
    ASSERT_NE(bridge, nullptr);

    sleep_enter_state(sleep, SLEEP_STATE_DEEP_NREM);
    mirror_neurons_sleep_update(bridge);

    mirror_neurons_sleep_effects_t effects;
    mirror_neurons_sleep_get_effects(bridge, &effects);

    // With modulations disabled, factors should be at baseline (1.0)
    // Exception: action_replay_factor = 0.1f when disabled (no replay without modulation)
    EXPECT_TRUE(floatEquals(effects.mirroring_activity_factor, 1.0f));
    EXPECT_TRUE(floatEquals(effects.empathy_modulation_factor, 1.0f));
    EXPECT_TRUE(floatEquals(effects.action_observation_factor, 1.0f));
    EXPECT_TRUE(floatEquals(effects.action_replay_factor, 0.1f));  // Minimal replay when disabled
}

/* ============================================================================
 * Sleep Pressure Tests
 * ============================================================================ */

TEST_F(MirrorNeuronsSleepBridgeTest, SleepPressureTracking) {
    bridge = mirror_neurons_sleep_bridge_create(nullptr, sleep);
    ASSERT_NE(bridge, nullptr);

    // Accumulate some sleep pressure
    sleep_accumulate_pressure(sleep, 1000);

    sleep_enter_state(sleep, SLEEP_STATE_AWAKE);
    mirror_neurons_sleep_update(bridge);

    mirror_neurons_sleep_effects_t effects;
    mirror_neurons_sleep_get_effects(bridge, &effects);

    EXPECT_GT(effects.sleep_pressure, 0.0f);
    EXPECT_LE(effects.sleep_pressure, 1.0f);
}

/* ============================================================================
 * Motor Suppression Tests
 * ============================================================================ */

TEST_F(MirrorNeuronsSleepBridgeTest, MotorSuppressionDuringSleep) {
    bridge = mirror_neurons_sleep_bridge_create(nullptr, sleep);
    ASSERT_NE(bridge, nullptr);

    // Awake: no motor suppression
    sleep_enter_state(sleep, SLEEP_STATE_AWAKE);
    mirror_neurons_sleep_update(bridge);

    mirror_neurons_sleep_effects_t effects_awake;
    mirror_neurons_sleep_get_effects(bridge, &effects_awake);

    // REM: motor suppression should be high
    sleep_enter_state(sleep, SLEEP_STATE_REM);
    mirror_neurons_sleep_update(bridge);

    mirror_neurons_sleep_effects_t effects_rem;
    mirror_neurons_sleep_get_effects(bridge, &effects_rem);

    // REM should have higher motor suppression than awake
    EXPECT_GT(effects_rem.motor_suppression_factor, effects_awake.motor_suppression_factor);
}

/* ============================================================================
 * Edge Cases and Boundary Conditions
 * ============================================================================ */

TEST_F(MirrorNeuronsSleepBridgeTest, MultipleUpdates) {
    bridge = mirror_neurons_sleep_bridge_create(nullptr, sleep);
    ASSERT_NE(bridge, nullptr);

    sleep_enter_state(sleep, SLEEP_STATE_AWAKE);

    // Multiple updates should not cause issues
    for (int i = 0; i < 10; i++) {
        int ret = mirror_neurons_sleep_update(bridge);
        EXPECT_EQ(ret, 0);
    }

    mirror_neurons_sleep_effects_t effects;
    int ret = mirror_neurons_sleep_get_effects(bridge, &effects);
    EXPECT_EQ(ret, 0);
}

TEST_F(MirrorNeuronsSleepBridgeTest, RapidStateChanges) {
    bridge = mirror_neurons_sleep_bridge_create(nullptr, sleep);
    ASSERT_NE(bridge, nullptr);

    sleep_state_t states[] = {
        SLEEP_STATE_AWAKE,
        SLEEP_STATE_DROWSY,
        SLEEP_STATE_LIGHT_NREM,
        SLEEP_STATE_DEEP_NREM,
        SLEEP_STATE_REM,
        SLEEP_STATE_AWAKE
    };

    for (size_t i = 0; i < sizeof(states) / sizeof(states[0]); i++) {
        sleep_enter_state(sleep, states[i]);
        int ret = mirror_neurons_sleep_update(bridge);
        EXPECT_EQ(ret, 0);

        mirror_neurons_sleep_effects_t effects;
        ret = mirror_neurons_sleep_get_effects(bridge, &effects);
        EXPECT_EQ(ret, 0);
        EXPECT_EQ(effects.current_state, states[i]);
    }
}

TEST_F(MirrorNeuronsSleepBridgeTest, ZeroModulationStrength) {
    mirror_neurons_sleep_config_t config;
    mirror_neurons_sleep_default_config(&config);
    config.modulation_strength = 0.0f;

    bridge = mirror_neurons_sleep_bridge_create(&config, sleep);
    ASSERT_NE(bridge, nullptr);

    sleep_enter_state(sleep, SLEEP_STATE_DEEP_NREM);
    mirror_neurons_sleep_update(bridge);

    mirror_neurons_sleep_effects_t effects;
    mirror_neurons_sleep_get_effects(bridge, &effects);

    // With zero modulation strength, all factors should be 1.0
    EXPECT_TRUE(floatEquals(effects.mirroring_activity_factor, 1.0f));
    EXPECT_TRUE(floatEquals(effects.empathy_modulation_factor, 1.0f));
    EXPECT_TRUE(floatEquals(effects.action_observation_factor, 1.0f));
}

TEST_F(MirrorNeuronsSleepBridgeTest, MaxModulationStrength) {
    mirror_neurons_sleep_config_t config;
    mirror_neurons_sleep_default_config(&config);
    config.modulation_strength = 1.0f;

    bridge = mirror_neurons_sleep_bridge_create(&config, sleep);
    ASSERT_NE(bridge, nullptr);

    sleep_enter_state(sleep, SLEEP_STATE_DEEP_NREM);
    mirror_neurons_sleep_update(bridge);

    mirror_neurons_sleep_effects_t effects;
    mirror_neurons_sleep_get_effects(bridge, &effects);

    // With full modulation strength, factors should match constants exactly
    EXPECT_TRUE(floatEquals(effects.mirroring_activity_factor, MIRROR_SLEEP_ACTIVITY_DEEP_NREM));
    EXPECT_TRUE(floatEquals(effects.empathy_modulation_factor, MIRROR_SLEEP_EMPATHY_DEEP_NREM));
    EXPECT_TRUE(floatEquals(effects.action_observation_factor, MIRROR_SLEEP_OBSERVATION_DEEP_NREM));
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
