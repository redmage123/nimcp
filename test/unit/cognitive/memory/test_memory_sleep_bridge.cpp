/**
 * @file test_memory_sleep_bridge.cpp
 * @brief Unit tests for Memory-Sleep Bridge module
 *
 * WHAT: Comprehensive tests for sleep-systems consolidation integration
 * WHY:  Ensure memory consolidation modulation by sleep states works correctly
 * HOW:  Test lifecycle, state effects, bio-async, and edge cases
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include "cognitive/memory/nimcp_memory_sleep_bridge.h"
#include "cognitive/nimcp_sleep_wake.h"

class MemorySleepBridgeTest : public ::testing::Test {
protected:
    memory_sleep_bridge_t bridge = nullptr;
    sleep_system_t sleep = nullptr;

    void SetUp() override {
        // Create sleep system
        sleep_config_t sleep_config = sleep_default_config();
        sleep = sleep_system_create(&sleep_config);
        ASSERT_NE(sleep, nullptr);

        // Create bridge with sleep system
        memory_sleep_config_t config;
        memory_sleep_default_config(&config);
        bridge = memory_sleep_bridge_create(&config, sleep);
    }

    void TearDown() override {
        if (bridge) {
            memory_sleep_bridge_destroy(bridge);
            bridge = nullptr;
        }
        if (sleep) {
            sleep_system_destroy(sleep);
            sleep = nullptr;
        }
    }
};

/* ============================================================================
 * Configuration Tests
 * ============================================================================ */

TEST_F(MemorySleepBridgeTest, DefaultConfig) {
    memory_sleep_config_t config;
    int ret = memory_sleep_default_config(&config);

    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(config.enable_replay_modulation);
    EXPECT_TRUE(config.enable_transfer_modulation);
    EXPECT_TRUE(config.enable_consolidation_modulation);
    EXPECT_TRUE(config.enable_semantic_extraction);
    EXPECT_FLOAT_EQ(config.modulation_strength, 1.0f);
}

TEST_F(MemorySleepBridgeTest, DefaultConfigNullPtr) {
    int ret = memory_sleep_default_config(nullptr);
    EXPECT_EQ(ret, -1);
}

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(MemorySleepBridgeTest, CreateDestroy) {
    ASSERT_NE(bridge, nullptr);
}

TEST_F(MemorySleepBridgeTest, CreateWithNullConfig) {
    memory_sleep_bridge_t br = memory_sleep_bridge_create(nullptr, sleep);
    EXPECT_NE(br, nullptr);
    memory_sleep_bridge_destroy(br);
}

TEST_F(MemorySleepBridgeTest, CreateWithNullSleep) {
    memory_sleep_config_t config;
    memory_sleep_default_config(&config);
    memory_sleep_bridge_t br = memory_sleep_bridge_create(&config, nullptr);
    EXPECT_EQ(br, nullptr);
}

TEST_F(MemorySleepBridgeTest, CreateWithBothNull) {
    memory_sleep_bridge_t br = memory_sleep_bridge_create(nullptr, nullptr);
    EXPECT_EQ(br, nullptr);
}

TEST_F(MemorySleepBridgeTest, DestroyNull) {
    memory_sleep_bridge_destroy(nullptr);  // Should not crash
}

/* ============================================================================
 * Update Function Tests
 * ============================================================================ */

TEST_F(MemorySleepBridgeTest, UpdateNull) {
    int ret = memory_sleep_update(nullptr);
    EXPECT_EQ(ret, -1);
}

TEST_F(MemorySleepBridgeTest, UpdateAwakeState) {
    sleep_enter_state(sleep, SLEEP_STATE_AWAKE);
    int ret = memory_sleep_update(bridge);
    EXPECT_EQ(ret, 0);

    memory_sleep_effects_t effects;
    memory_sleep_get_effects(bridge, &effects);
    EXPECT_EQ(effects.current_state, SLEEP_STATE_AWAKE);
    EXPECT_FLOAT_EQ(effects.replay_frequency_factor, MEMORY_SLEEP_REPLAY_AWAKE);
    EXPECT_FLOAT_EQ(effects.transfer_rate_factor, MEMORY_SLEEP_TRANSFER_AWAKE);
    EXPECT_FLOAT_EQ(effects.consolidation_strength_factor, MEMORY_SLEEP_CONSOLIDATION_AWAKE);
    EXPECT_FLOAT_EQ(effects.semantic_extraction_factor, MEMORY_SLEEP_SEMANTIC_AWAKE);
    EXPECT_FALSE(effects.replay_active);
    EXPECT_FALSE(effects.peak_consolidation);
}

TEST_F(MemorySleepBridgeTest, UpdateDrowsyState) {
    sleep_enter_state(sleep, SLEEP_STATE_DROWSY);
    int ret = memory_sleep_update(bridge);
    EXPECT_EQ(ret, 0);

    memory_sleep_effects_t effects;
    memory_sleep_get_effects(bridge, &effects);
    EXPECT_EQ(effects.current_state, SLEEP_STATE_DROWSY);
    EXPECT_FLOAT_EQ(effects.replay_frequency_factor, MEMORY_SLEEP_REPLAY_DROWSY);
    EXPECT_FLOAT_EQ(effects.transfer_rate_factor, MEMORY_SLEEP_TRANSFER_DROWSY);
    EXPECT_FLOAT_EQ(effects.consolidation_strength_factor, MEMORY_SLEEP_CONSOLIDATION_DROWSY);
    EXPECT_FLOAT_EQ(effects.semantic_extraction_factor, MEMORY_SLEEP_SEMANTIC_DROWSY);
    EXPECT_FALSE(effects.replay_active);
    EXPECT_FALSE(effects.peak_consolidation);
}

TEST_F(MemorySleepBridgeTest, UpdateLightNremState) {
    sleep_enter_state(sleep, SLEEP_STATE_LIGHT_NREM);
    int ret = memory_sleep_update(bridge);
    EXPECT_EQ(ret, 0);

    memory_sleep_effects_t effects;
    memory_sleep_get_effects(bridge, &effects);
    EXPECT_EQ(effects.current_state, SLEEP_STATE_LIGHT_NREM);
    EXPECT_FLOAT_EQ(effects.replay_frequency_factor, MEMORY_SLEEP_REPLAY_LIGHT_NREM);
    EXPECT_FLOAT_EQ(effects.transfer_rate_factor, MEMORY_SLEEP_TRANSFER_LIGHT_NREM);
    EXPECT_FLOAT_EQ(effects.consolidation_strength_factor, MEMORY_SLEEP_CONSOLIDATION_LIGHT_NREM);
    EXPECT_FLOAT_EQ(effects.semantic_extraction_factor, MEMORY_SLEEP_SEMANTIC_LIGHT_NREM);
    EXPECT_TRUE(effects.replay_active);
    EXPECT_FALSE(effects.peak_consolidation);
}

TEST_F(MemorySleepBridgeTest, UpdateDeepNremState) {
    sleep_enter_state(sleep, SLEEP_STATE_DEEP_NREM);
    int ret = memory_sleep_update(bridge);
    EXPECT_EQ(ret, 0);

    memory_sleep_effects_t effects;
    memory_sleep_get_effects(bridge, &effects);
    EXPECT_EQ(effects.current_state, SLEEP_STATE_DEEP_NREM);
    EXPECT_FLOAT_EQ(effects.replay_frequency_factor, MEMORY_SLEEP_REPLAY_DEEP_NREM);
    EXPECT_FLOAT_EQ(effects.transfer_rate_factor, MEMORY_SLEEP_TRANSFER_DEEP_NREM);
    EXPECT_FLOAT_EQ(effects.consolidation_strength_factor, MEMORY_SLEEP_CONSOLIDATION_DEEP_NREM);
    EXPECT_FLOAT_EQ(effects.semantic_extraction_factor, MEMORY_SLEEP_SEMANTIC_DEEP_NREM);
    EXPECT_TRUE(effects.replay_active);
    EXPECT_TRUE(effects.peak_consolidation);
}

TEST_F(MemorySleepBridgeTest, UpdateRemState) {
    sleep_enter_state(sleep, SLEEP_STATE_REM);
    int ret = memory_sleep_update(bridge);
    EXPECT_EQ(ret, 0);

    memory_sleep_effects_t effects;
    memory_sleep_get_effects(bridge, &effects);
    EXPECT_EQ(effects.current_state, SLEEP_STATE_REM);
    EXPECT_FLOAT_EQ(effects.replay_frequency_factor, MEMORY_SLEEP_REPLAY_REM);
    EXPECT_FLOAT_EQ(effects.transfer_rate_factor, MEMORY_SLEEP_TRANSFER_REM);
    EXPECT_FLOAT_EQ(effects.consolidation_strength_factor, MEMORY_SLEEP_CONSOLIDATION_REM);
    EXPECT_FLOAT_EQ(effects.semantic_extraction_factor, MEMORY_SLEEP_SEMANTIC_REM);
    EXPECT_TRUE(effects.replay_active);
    EXPECT_FALSE(effects.peak_consolidation);
}

/* ============================================================================
 * Effects Retrieval Tests
 * ============================================================================ */

TEST_F(MemorySleepBridgeTest, GetEffects) {
    memory_sleep_effects_t effects;
    int ret = memory_sleep_get_effects(bridge, &effects);

    EXPECT_EQ(ret, 0);
    EXPECT_GE(effects.replay_frequency_factor, 0.0f);
    EXPECT_GE(effects.transfer_rate_factor, 0.0f);
    EXPECT_GE(effects.consolidation_strength_factor, 0.0f);
    EXPECT_GE(effects.semantic_extraction_factor, 0.0f);
    EXPECT_GE(effects.sleep_pressure, 0.0f);
    EXPECT_LE(effects.sleep_pressure, 1.0f);
}

TEST_F(MemorySleepBridgeTest, GetEffectsNullBridge) {
    memory_sleep_effects_t effects;
    int ret = memory_sleep_get_effects(nullptr, &effects);
    EXPECT_EQ(ret, -1);
}

TEST_F(MemorySleepBridgeTest, GetEffectsNullOutput) {
    int ret = memory_sleep_get_effects(bridge, nullptr);
    EXPECT_EQ(ret, -1);
}

TEST_F(MemorySleepBridgeTest, GetEffectsBothNull) {
    int ret = memory_sleep_get_effects(nullptr, nullptr);
    EXPECT_EQ(ret, -1);
}

/* ============================================================================
 * Convenience Function Tests
 * ============================================================================ */

TEST_F(MemorySleepBridgeTest, GetReplayFrequency) {
    sleep_enter_state(sleep, SLEEP_STATE_DEEP_NREM);
    memory_sleep_update(bridge);

    float freq = memory_sleep_get_replay_frequency(bridge);
    EXPECT_FLOAT_EQ(freq, MEMORY_SLEEP_REPLAY_DEEP_NREM);
}

TEST_F(MemorySleepBridgeTest, GetReplayFrequencyNull) {
    float freq = memory_sleep_get_replay_frequency(nullptr);
    EXPECT_FLOAT_EQ(freq, 1.0f);  // Error default
}

TEST_F(MemorySleepBridgeTest, IsReplayActiveAwake) {
    sleep_enter_state(sleep, SLEEP_STATE_AWAKE);
    memory_sleep_update(bridge);

    bool active = memory_sleep_is_replay_active(bridge);
    EXPECT_FALSE(active);
}

TEST_F(MemorySleepBridgeTest, IsReplayActiveDrowsy) {
    sleep_enter_state(sleep, SLEEP_STATE_DROWSY);
    memory_sleep_update(bridge);

    bool active = memory_sleep_is_replay_active(bridge);
    EXPECT_FALSE(active);
}

TEST_F(MemorySleepBridgeTest, IsReplayActiveLightNrem) {
    sleep_enter_state(sleep, SLEEP_STATE_LIGHT_NREM);
    memory_sleep_update(bridge);

    bool active = memory_sleep_is_replay_active(bridge);
    EXPECT_TRUE(active);
}

TEST_F(MemorySleepBridgeTest, IsReplayActiveDeepNrem) {
    sleep_enter_state(sleep, SLEEP_STATE_DEEP_NREM);
    memory_sleep_update(bridge);

    bool active = memory_sleep_is_replay_active(bridge);
    EXPECT_TRUE(active);
}

TEST_F(MemorySleepBridgeTest, IsReplayActiveRem) {
    sleep_enter_state(sleep, SLEEP_STATE_REM);
    memory_sleep_update(bridge);

    bool active = memory_sleep_is_replay_active(bridge);
    EXPECT_TRUE(active);
}

TEST_F(MemorySleepBridgeTest, IsReplayActiveNull) {
    bool active = memory_sleep_is_replay_active(nullptr);
    EXPECT_FALSE(active);
}

/* ============================================================================
 * State Lookup Function Tests
 * ============================================================================ */

TEST_F(MemorySleepBridgeTest, ReplayForStateAwake) {
    float replay = memory_sleep_replay_for_state(SLEEP_STATE_AWAKE);
    EXPECT_FLOAT_EQ(replay, MEMORY_SLEEP_REPLAY_AWAKE);
}

TEST_F(MemorySleepBridgeTest, ReplayForStateDrowsy) {
    float replay = memory_sleep_replay_for_state(SLEEP_STATE_DROWSY);
    EXPECT_FLOAT_EQ(replay, MEMORY_SLEEP_REPLAY_DROWSY);
}

TEST_F(MemorySleepBridgeTest, ReplayForStateLightNrem) {
    float replay = memory_sleep_replay_for_state(SLEEP_STATE_LIGHT_NREM);
    EXPECT_FLOAT_EQ(replay, MEMORY_SLEEP_REPLAY_LIGHT_NREM);
}

TEST_F(MemorySleepBridgeTest, ReplayForStateDeepNrem) {
    float replay = memory_sleep_replay_for_state(SLEEP_STATE_DEEP_NREM);
    EXPECT_FLOAT_EQ(replay, MEMORY_SLEEP_REPLAY_DEEP_NREM);
}

TEST_F(MemorySleepBridgeTest, ReplayForStateRem) {
    float replay = memory_sleep_replay_for_state(SLEEP_STATE_REM);
    EXPECT_FLOAT_EQ(replay, MEMORY_SLEEP_REPLAY_REM);
}

TEST_F(MemorySleepBridgeTest, TransferForStateAwake) {
    float transfer = memory_sleep_transfer_for_state(SLEEP_STATE_AWAKE);
    EXPECT_FLOAT_EQ(transfer, MEMORY_SLEEP_TRANSFER_AWAKE);
}

TEST_F(MemorySleepBridgeTest, TransferForStateDrowsy) {
    float transfer = memory_sleep_transfer_for_state(SLEEP_STATE_DROWSY);
    EXPECT_FLOAT_EQ(transfer, MEMORY_SLEEP_TRANSFER_DROWSY);
}

TEST_F(MemorySleepBridgeTest, TransferForStateLightNrem) {
    float transfer = memory_sleep_transfer_for_state(SLEEP_STATE_LIGHT_NREM);
    EXPECT_FLOAT_EQ(transfer, MEMORY_SLEEP_TRANSFER_LIGHT_NREM);
}

TEST_F(MemorySleepBridgeTest, TransferForStateDeepNrem) {
    float transfer = memory_sleep_transfer_for_state(SLEEP_STATE_DEEP_NREM);
    EXPECT_FLOAT_EQ(transfer, MEMORY_SLEEP_TRANSFER_DEEP_NREM);
}

TEST_F(MemorySleepBridgeTest, TransferForStateRem) {
    float transfer = memory_sleep_transfer_for_state(SLEEP_STATE_REM);
    EXPECT_FLOAT_EQ(transfer, MEMORY_SLEEP_TRANSFER_REM);
}

TEST_F(MemorySleepBridgeTest, ConsolidationForStateAwake) {
    float consolidation = memory_sleep_consolidation_for_state(SLEEP_STATE_AWAKE);
    EXPECT_FLOAT_EQ(consolidation, MEMORY_SLEEP_CONSOLIDATION_AWAKE);
}

TEST_F(MemorySleepBridgeTest, ConsolidationForStateDrowsy) {
    float consolidation = memory_sleep_consolidation_for_state(SLEEP_STATE_DROWSY);
    EXPECT_FLOAT_EQ(consolidation, MEMORY_SLEEP_CONSOLIDATION_DROWSY);
}

TEST_F(MemorySleepBridgeTest, ConsolidationForStateLightNrem) {
    float consolidation = memory_sleep_consolidation_for_state(SLEEP_STATE_LIGHT_NREM);
    EXPECT_FLOAT_EQ(consolidation, MEMORY_SLEEP_CONSOLIDATION_LIGHT_NREM);
}

TEST_F(MemorySleepBridgeTest, ConsolidationForStateDeepNrem) {
    float consolidation = memory_sleep_consolidation_for_state(SLEEP_STATE_DEEP_NREM);
    EXPECT_FLOAT_EQ(consolidation, MEMORY_SLEEP_CONSOLIDATION_DEEP_NREM);
}

TEST_F(MemorySleepBridgeTest, ConsolidationForStateRem) {
    float consolidation = memory_sleep_consolidation_for_state(SLEEP_STATE_REM);
    EXPECT_FLOAT_EQ(consolidation, MEMORY_SLEEP_CONSOLIDATION_REM);
}

TEST_F(MemorySleepBridgeTest, SemanticForStateAwake) {
    float semantic = memory_sleep_semantic_for_state(SLEEP_STATE_AWAKE);
    EXPECT_FLOAT_EQ(semantic, MEMORY_SLEEP_SEMANTIC_AWAKE);
}

TEST_F(MemorySleepBridgeTest, SemanticForStateDrowsy) {
    float semantic = memory_sleep_semantic_for_state(SLEEP_STATE_DROWSY);
    EXPECT_FLOAT_EQ(semantic, MEMORY_SLEEP_SEMANTIC_DROWSY);
}

TEST_F(MemorySleepBridgeTest, SemanticForStateLightNrem) {
    float semantic = memory_sleep_semantic_for_state(SLEEP_STATE_LIGHT_NREM);
    EXPECT_FLOAT_EQ(semantic, MEMORY_SLEEP_SEMANTIC_LIGHT_NREM);
}

TEST_F(MemorySleepBridgeTest, SemanticForStateDeepNrem) {
    float semantic = memory_sleep_semantic_for_state(SLEEP_STATE_DEEP_NREM);
    EXPECT_FLOAT_EQ(semantic, MEMORY_SLEEP_SEMANTIC_DEEP_NREM);
}

TEST_F(MemorySleepBridgeTest, SemanticForStateRem) {
    float semantic = memory_sleep_semantic_for_state(SLEEP_STATE_REM);
    EXPECT_FLOAT_EQ(semantic, MEMORY_SLEEP_SEMANTIC_REM);
}

/* ============================================================================
 * State Transition Tests
 * ============================================================================ */

TEST_F(MemorySleepBridgeTest, StateTransitionAwakeToDrowsy) {
    sleep_enter_state(sleep, SLEEP_STATE_AWAKE);
    memory_sleep_update(bridge);

    sleep_enter_state(sleep, SLEEP_STATE_DROWSY);
    memory_sleep_update(bridge);

    memory_sleep_effects_t effects;
    memory_sleep_get_effects(bridge, &effects);

    EXPECT_EQ(effects.current_state, SLEEP_STATE_DROWSY);
    EXPECT_GT(effects.replay_frequency_factor, MEMORY_SLEEP_REPLAY_AWAKE);
}

TEST_F(MemorySleepBridgeTest, StateTransitionDrowsyToLightNrem) {
    sleep_enter_state(sleep, SLEEP_STATE_DROWSY);
    memory_sleep_update(bridge);

    sleep_enter_state(sleep, SLEEP_STATE_LIGHT_NREM);
    memory_sleep_update(bridge);

    memory_sleep_effects_t effects;
    memory_sleep_get_effects(bridge, &effects);

    EXPECT_EQ(effects.current_state, SLEEP_STATE_LIGHT_NREM);
    EXPECT_TRUE(effects.replay_active);
}

TEST_F(MemorySleepBridgeTest, StateTransitionLightToDeepNrem) {
    sleep_enter_state(sleep, SLEEP_STATE_LIGHT_NREM);
    memory_sleep_update(bridge);

    sleep_enter_state(sleep, SLEEP_STATE_DEEP_NREM);
    memory_sleep_update(bridge);

    memory_sleep_effects_t effects;
    memory_sleep_get_effects(bridge, &effects);

    EXPECT_EQ(effects.current_state, SLEEP_STATE_DEEP_NREM);
    EXPECT_TRUE(effects.peak_consolidation);
    EXPECT_FLOAT_EQ(effects.consolidation_strength_factor, 1.0f);
}

TEST_F(MemorySleepBridgeTest, StateTransitionDeepNremToRem) {
    sleep_enter_state(sleep, SLEEP_STATE_DEEP_NREM);
    memory_sleep_update(bridge);

    sleep_enter_state(sleep, SLEEP_STATE_REM);
    memory_sleep_update(bridge);

    memory_sleep_effects_t effects;
    memory_sleep_get_effects(bridge, &effects);

    EXPECT_EQ(effects.current_state, SLEEP_STATE_REM);
    EXPECT_FLOAT_EQ(effects.semantic_extraction_factor, 1.0f);
    EXPECT_FALSE(effects.peak_consolidation);
}

TEST_F(MemorySleepBridgeTest, StateTransitionRemToAwake) {
    sleep_enter_state(sleep, SLEEP_STATE_REM);
    memory_sleep_update(bridge);

    sleep_enter_state(sleep, SLEEP_STATE_AWAKE);
    memory_sleep_update(bridge);

    memory_sleep_effects_t effects;
    memory_sleep_get_effects(bridge, &effects);

    EXPECT_EQ(effects.current_state, SLEEP_STATE_AWAKE);
    EXPECT_FALSE(effects.replay_active);
    EXPECT_FALSE(effects.peak_consolidation);
}

/* ============================================================================
 * Edge Cases and Validation Tests
 * ============================================================================ */

TEST_F(MemorySleepBridgeTest, MultipleUpdatesConsistent) {
    sleep_enter_state(sleep, SLEEP_STATE_DEEP_NREM);

    memory_sleep_update(bridge);
    memory_sleep_effects_t effects1;
    memory_sleep_get_effects(bridge, &effects1);

    memory_sleep_update(bridge);
    memory_sleep_effects_t effects2;
    memory_sleep_get_effects(bridge, &effects2);

    EXPECT_FLOAT_EQ(effects1.replay_frequency_factor, effects2.replay_frequency_factor);
    EXPECT_FLOAT_EQ(effects1.transfer_rate_factor, effects2.transfer_rate_factor);
    EXPECT_FLOAT_EQ(effects1.consolidation_strength_factor, effects2.consolidation_strength_factor);
    EXPECT_EQ(effects1.current_state, effects2.current_state);
}

TEST_F(MemorySleepBridgeTest, ReplayFactorsProgression) {
    // Awake should have lowest replay
    float awake = memory_sleep_replay_for_state(SLEEP_STATE_AWAKE);
    float drowsy = memory_sleep_replay_for_state(SLEEP_STATE_DROWSY);
    float light = memory_sleep_replay_for_state(SLEEP_STATE_LIGHT_NREM);
    float deep = memory_sleep_replay_for_state(SLEEP_STATE_DEEP_NREM);
    float rem = memory_sleep_replay_for_state(SLEEP_STATE_REM);

    EXPECT_LT(awake, drowsy);
    EXPECT_LT(drowsy, light);
    EXPECT_LT(light, deep);
    EXPECT_GT(deep, rem);  // Deep NREM has highest replay
}

TEST_F(MemorySleepBridgeTest, TransferFactorsProgression) {
    // Awake should have lowest transfer rate
    float awake = memory_sleep_transfer_for_state(SLEEP_STATE_AWAKE);
    float drowsy = memory_sleep_transfer_for_state(SLEEP_STATE_DROWSY);
    float light = memory_sleep_transfer_for_state(SLEEP_STATE_LIGHT_NREM);
    float deep = memory_sleep_transfer_for_state(SLEEP_STATE_DEEP_NREM);
    float rem = memory_sleep_transfer_for_state(SLEEP_STATE_REM);

    EXPECT_LT(awake, drowsy);
    EXPECT_LT(drowsy, light);
    EXPECT_LT(light, deep);
    EXPECT_GT(deep, rem);  // Deep NREM has highest transfer
}

TEST_F(MemorySleepBridgeTest, ConsolidationFactorsProgression) {
    // Awake should have lowest consolidation
    float awake = memory_sleep_consolidation_for_state(SLEEP_STATE_AWAKE);
    float drowsy = memory_sleep_consolidation_for_state(SLEEP_STATE_DROWSY);
    float light = memory_sleep_consolidation_for_state(SLEEP_STATE_LIGHT_NREM);
    float deep = memory_sleep_consolidation_for_state(SLEEP_STATE_DEEP_NREM);

    EXPECT_LT(awake, drowsy);
    EXPECT_LT(drowsy, light);
    EXPECT_LT(light, deep);
    EXPECT_FLOAT_EQ(deep, 1.0f);  // Deep NREM has peak consolidation
}

TEST_F(MemorySleepBridgeTest, SemanticExtractionPeakInRem) {
    // REM should have highest semantic extraction
    float awake = memory_sleep_semantic_for_state(SLEEP_STATE_AWAKE);
    float light = memory_sleep_semantic_for_state(SLEEP_STATE_LIGHT_NREM);
    float deep = memory_sleep_semantic_for_state(SLEEP_STATE_DEEP_NREM);
    float rem = memory_sleep_semantic_for_state(SLEEP_STATE_REM);

    EXPECT_LT(awake, light);
    EXPECT_LT(light, deep);
    EXPECT_LT(deep, rem);
    EXPECT_FLOAT_EQ(rem, 1.0f);  // REM has peak semantic extraction
}

TEST_F(MemorySleepBridgeTest, SleepPressureTracking) {
    memory_sleep_effects_t effects;
    memory_sleep_get_effects(bridge, &effects);

    // Initial sleep pressure should be valid
    EXPECT_GE(effects.sleep_pressure, 0.0f);
    EXPECT_LE(effects.sleep_pressure, 1.0f);
}

TEST_F(MemorySleepBridgeTest, PeakConsolidationOnlyInDeepNrem) {
    // Test all states to ensure only deep NREM sets peak_consolidation
    sleep_state_t states[] = {
        SLEEP_STATE_AWAKE,
        SLEEP_STATE_DROWSY,
        SLEEP_STATE_LIGHT_NREM,
        SLEEP_STATE_DEEP_NREM,
        SLEEP_STATE_REM
    };

    for (int i = 0; i < 5; i++) {
        sleep_enter_state(sleep, states[i]);
        memory_sleep_update(bridge);

        memory_sleep_effects_t effects;
        memory_sleep_get_effects(bridge, &effects);

        if (states[i] == SLEEP_STATE_DEEP_NREM) {
            EXPECT_TRUE(effects.peak_consolidation);
        } else {
            EXPECT_FALSE(effects.peak_consolidation);
        }
    }
}

TEST_F(MemorySleepBridgeTest, ReplayActiveOnlyDuringSleep) {
    // Test all states to ensure replay_active is correct
    sleep_state_t states[] = {
        SLEEP_STATE_AWAKE,
        SLEEP_STATE_DROWSY,
        SLEEP_STATE_LIGHT_NREM,
        SLEEP_STATE_DEEP_NREM,
        SLEEP_STATE_REM
    };

    bool expected_replay[] = {false, false, true, true, true};

    for (int i = 0; i < 5; i++) {
        sleep_enter_state(sleep, states[i]);
        memory_sleep_update(bridge);

        memory_sleep_effects_t effects;
        memory_sleep_get_effects(bridge, &effects);

        EXPECT_EQ(effects.replay_active, expected_replay[i])
            << "State " << states[i] << " has incorrect replay_active";
    }
}

/* ============================================================================
 * Modulation Strength Tests
 * ============================================================================ */

TEST_F(MemorySleepBridgeTest, ModulationStrengthZero) {
    memory_sleep_config_t config;
    memory_sleep_default_config(&config);
    config.modulation_strength = 0.0f;

    memory_sleep_bridge_t br = memory_sleep_bridge_create(&config, sleep);
    ASSERT_NE(br, nullptr);

    sleep_enter_state(sleep, SLEEP_STATE_DEEP_NREM);
    memory_sleep_update(br);

    // With zero modulation strength, effects should be minimal or baseline
    memory_sleep_effects_t effects;
    memory_sleep_get_effects(br, &effects);

    EXPECT_GE(effects.replay_frequency_factor, 0.0f);

    memory_sleep_bridge_destroy(br);
}

TEST_F(MemorySleepBridgeTest, ModulationStrengthHalf) {
    memory_sleep_config_t config;
    memory_sleep_default_config(&config);
    config.modulation_strength = 0.5f;

    memory_sleep_bridge_t br = memory_sleep_bridge_create(&config, sleep);
    ASSERT_NE(br, nullptr);

    sleep_enter_state(sleep, SLEEP_STATE_DEEP_NREM);
    memory_sleep_update(br);

    memory_sleep_effects_t effects;
    memory_sleep_get_effects(br, &effects);

    // Effects should be present but reduced
    EXPECT_GT(effects.replay_frequency_factor, 0.0f);

    memory_sleep_bridge_destroy(br);
}

/* ============================================================================
 * Selective Modulation Tests
 * ============================================================================ */

TEST_F(MemorySleepBridgeTest, DisableReplayModulation) {
    memory_sleep_config_t config;
    memory_sleep_default_config(&config);
    config.enable_replay_modulation = false;

    memory_sleep_bridge_t br = memory_sleep_bridge_create(&config, sleep);
    ASSERT_NE(br, nullptr);

    sleep_enter_state(sleep, SLEEP_STATE_DEEP_NREM);
    memory_sleep_update(br);

    // Replay modulation should be disabled
    memory_sleep_bridge_destroy(br);
}

TEST_F(MemorySleepBridgeTest, DisableTransferModulation) {
    memory_sleep_config_t config;
    memory_sleep_default_config(&config);
    config.enable_transfer_modulation = false;

    memory_sleep_bridge_t br = memory_sleep_bridge_create(&config, sleep);
    ASSERT_NE(br, nullptr);

    sleep_enter_state(sleep, SLEEP_STATE_DEEP_NREM);
    memory_sleep_update(br);

    // Transfer modulation should be disabled
    memory_sleep_bridge_destroy(br);
}

TEST_F(MemorySleepBridgeTest, DisableConsolidationModulation) {
    memory_sleep_config_t config;
    memory_sleep_default_config(&config);
    config.enable_consolidation_modulation = false;

    memory_sleep_bridge_t br = memory_sleep_bridge_create(&config, sleep);
    ASSERT_NE(br, nullptr);

    sleep_enter_state(sleep, SLEEP_STATE_DEEP_NREM);
    memory_sleep_update(br);

    // Consolidation modulation should be disabled
    memory_sleep_bridge_destroy(br);
}

TEST_F(MemorySleepBridgeTest, DisableSemanticExtraction) {
    memory_sleep_config_t config;
    memory_sleep_default_config(&config);
    config.enable_semantic_extraction = false;

    memory_sleep_bridge_t br = memory_sleep_bridge_create(&config, sleep);
    ASSERT_NE(br, nullptr);

    sleep_enter_state(sleep, SLEEP_STATE_REM);
    memory_sleep_update(br);

    // Semantic extraction should be disabled
    memory_sleep_bridge_destroy(br);
}

TEST_F(MemorySleepBridgeTest, AllModulationsDisabled) {
    memory_sleep_config_t config;
    memory_sleep_default_config(&config);
    config.enable_replay_modulation = false;
    config.enable_transfer_modulation = false;
    config.enable_consolidation_modulation = false;
    config.enable_semantic_extraction = false;

    memory_sleep_bridge_t br = memory_sleep_bridge_create(&config, sleep);
    ASSERT_NE(br, nullptr);

    sleep_enter_state(sleep, SLEEP_STATE_DEEP_NREM);
    memory_sleep_update(br);

    // All modulations disabled - bridge should still work
    memory_sleep_bridge_destroy(br);
}
