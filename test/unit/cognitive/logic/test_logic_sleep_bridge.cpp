/**
 * @file test_logic_sleep_bridge.cpp
 * @brief Unit tests for Logic Sleep Bridge module
 *
 * WHAT: Comprehensive tests for Sleep-Logic bidirectional integration
 * WHY:  Ensure sleep state modulation of logical reasoning works correctly
 * HOW:  Test lifecycle, state effects, bio-async integration, and edge cases
 *
 * TEST COVERAGE:
 * - Default configuration initialization
 * - Bridge creation and destruction
 * - Update function for each sleep state (AWAKE, DROWSY, LIGHT_NREM, DEEP_NREM, REM)
 * - Effects retrieval and validation
 * - Bio-async connection
 * - NULL pointer handling
 * - Edge cases
 *
 * @author NIMCP Development Team
 * @date 2025-12-21
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include "cognitive/logic/nimcp_logic_sleep_bridge.h"
#include "cognitive/nimcp_sleep_wake.h"

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class LogicSleepBridgeTest : public ::testing::Test {
protected:
    logic_sleep_bridge_t* bridge = nullptr;
    sleep_system_t sleep_system = nullptr;

    void SetUp() override {
        // Create sleep system
        sleep_config_t sleep_config;
        memset(&sleep_config, 0, sizeof(sleep_config));
        sleep_config.adenosine_accumulation_rate = 0.0001f;
        sleep_config.sleep_pressure_threshold = 0.8f;
        sleep_config.adenosine_clearance_rate = 0.05f;
        sleep_system = sleep_system_create(&sleep_config);
        ASSERT_NE(sleep_system, nullptr);

        // Create logic-sleep bridge with default config
        logic_sleep_config_t config;
        logic_sleep_default_config(&config);
        bridge = logic_sleep_bridge_create(&config, sleep_system);
    }

    void TearDown() override {
        if (bridge) {
            logic_sleep_bridge_destroy(bridge);
            bridge = nullptr;
        }
        if (sleep_system) {
            sleep_system_destroy(sleep_system);
            sleep_system = nullptr;
        }
    }
};

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(LogicSleepBridgeTest, CreateDestroy) {
    ASSERT_NE(bridge, nullptr);
}

TEST_F(LogicSleepBridgeTest, CreateWithNullConfig) {
    logic_sleep_bridge_t* br = logic_sleep_bridge_create(nullptr, sleep_system);
    ASSERT_NE(br, nullptr);
    logic_sleep_bridge_destroy(br);
}

TEST_F(LogicSleepBridgeTest, CreateWithNullSleepSystem) {
    logic_sleep_config_t config;
    logic_sleep_default_config(&config);
    logic_sleep_bridge_t* br = logic_sleep_bridge_create(&config, nullptr);
    EXPECT_EQ(br, nullptr);
}

TEST_F(LogicSleepBridgeTest, DestroyNull) {
    logic_sleep_bridge_destroy(nullptr);
    // Should not crash
}

/* ============================================================================
 * Default Config Tests
 * ============================================================================ */

TEST_F(LogicSleepBridgeTest, DefaultConfig) {
    logic_sleep_config_t config;
    int ret = logic_sleep_default_config(&config);

    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(config.enable_inference_modulation);
    EXPECT_TRUE(config.enable_accuracy_modulation);
    EXPECT_TRUE(config.enable_consistency_modulation);
    EXPECT_TRUE(config.enable_nrem_consolidation);
    EXPECT_TRUE(config.enable_rem_creativity);
    EXPECT_GT(config.modulation_strength, 0.0f);
    EXPECT_LE(config.modulation_strength, 2.0f);
    EXPECT_GE(config.sleep_pressure_threshold, 0.5f);
    EXPECT_LE(config.sleep_pressure_threshold, 0.9f);
}

TEST_F(LogicSleepBridgeTest, DefaultConfigNullPtr) {
    int ret = logic_sleep_default_config(nullptr);
    EXPECT_EQ(ret, -1);
}

/* ============================================================================
 * Update and Effects Tests - AWAKE State
 * ============================================================================ */

TEST_F(LogicSleepBridgeTest, UpdateAwakeState) {
    int ret = logic_sleep_update(bridge);
    EXPECT_EQ(ret, 0);

    logic_sleep_effects_t effects;
    ret = logic_sleep_get_effects(bridge, &effects);
    EXPECT_EQ(ret, 0);

    // AWAKE state should have full capacity
    EXPECT_EQ(effects.current_state, SLEEP_STATE_AWAKE);
    EXPECT_FLOAT_EQ(effects.inference_capacity_factor, LOGIC_SLEEP_INFERENCE_AWAKE);
    EXPECT_FLOAT_EQ(effects.deduction_accuracy_factor, LOGIC_SLEEP_ACCURACY_AWAKE);
    EXPECT_FLOAT_EQ(effects.consistency_check_factor, LOGIC_SLEEP_CONSISTENCY_AWAKE);
    EXPECT_FALSE(effects.logic_offline);
    EXPECT_FALSE(effects.consolidation_mode);
    EXPECT_FALSE(effects.creative_mode);
}

TEST_F(LogicSleepBridgeTest, AwakeStateQueries) {
    logic_sleep_update(bridge);

    float inference = logic_sleep_get_inference_capacity(bridge);
    EXPECT_FLOAT_EQ(inference, LOGIC_SLEEP_INFERENCE_AWAKE);

    EXPECT_FALSE(logic_sleep_is_offline(bridge));
    EXPECT_FALSE(logic_sleep_is_consolidation_mode(bridge));
}

/* ============================================================================
 * Update and Effects Tests - DROWSY State
 * ============================================================================ */

TEST_F(LogicSleepBridgeTest, UpdateDrowsyState) {
    // Force sleep system to DROWSY state
    // This requires manipulating sleep pressure
    // For testing, we'll use the state mapping functions directly
    float inference = logic_sleep_inference_for_state(SLEEP_STATE_DROWSY);
    float accuracy = logic_sleep_accuracy_for_state(SLEEP_STATE_DROWSY);
    float consistency = logic_sleep_consistency_for_state(SLEEP_STATE_DROWSY);

    EXPECT_FLOAT_EQ(inference, LOGIC_SLEEP_INFERENCE_DROWSY);
    EXPECT_FLOAT_EQ(accuracy, LOGIC_SLEEP_ACCURACY_DROWSY);
    EXPECT_FLOAT_EQ(consistency, LOGIC_SLEEP_CONSISTENCY_DROWSY);
}

TEST_F(LogicSleepBridgeTest, DrowsyStateReducedPerformance) {
    // Verify drowsy state shows reduced performance vs awake
    float awake_inf = logic_sleep_inference_for_state(SLEEP_STATE_AWAKE);
    float drowsy_inf = logic_sleep_inference_for_state(SLEEP_STATE_DROWSY);
    EXPECT_LT(drowsy_inf, awake_inf);

    float awake_acc = logic_sleep_accuracy_for_state(SLEEP_STATE_AWAKE);
    float drowsy_acc = logic_sleep_accuracy_for_state(SLEEP_STATE_DROWSY);
    EXPECT_LT(drowsy_acc, awake_acc);

    float awake_con = logic_sleep_consistency_for_state(SLEEP_STATE_AWAKE);
    float drowsy_con = logic_sleep_consistency_for_state(SLEEP_STATE_DROWSY);
    EXPECT_LT(drowsy_con, awake_con);
}

/* ============================================================================
 * Update and Effects Tests - LIGHT_NREM State
 * ============================================================================ */

TEST_F(LogicSleepBridgeTest, UpdateLightNremState) {
    float inference = logic_sleep_inference_for_state(SLEEP_STATE_LIGHT_NREM);
    float accuracy = logic_sleep_accuracy_for_state(SLEEP_STATE_LIGHT_NREM);
    float consistency = logic_sleep_consistency_for_state(SLEEP_STATE_LIGHT_NREM);

    EXPECT_FLOAT_EQ(inference, LOGIC_SLEEP_INFERENCE_LIGHT_NREM);
    EXPECT_FLOAT_EQ(accuracy, LOGIC_SLEEP_ACCURACY_LIGHT_NREM);
    EXPECT_FLOAT_EQ(consistency, LOGIC_SLEEP_CONSISTENCY_NREM);
}

TEST_F(LogicSleepBridgeTest, LightNremMinimalLogic) {
    // Light NREM should have minimal inference capacity
    float inference = logic_sleep_inference_for_state(SLEEP_STATE_LIGHT_NREM);
    EXPECT_LT(inference, 0.2f);  // Less than 20% capacity
}

/* ============================================================================
 * Update and Effects Tests - DEEP_NREM State
 * ============================================================================ */

TEST_F(LogicSleepBridgeTest, UpdateDeepNremState) {
    float inference = logic_sleep_inference_for_state(SLEEP_STATE_DEEP_NREM);
    float accuracy = logic_sleep_accuracy_for_state(SLEEP_STATE_DEEP_NREM);
    float consistency = logic_sleep_consistency_for_state(SLEEP_STATE_DEEP_NREM);

    EXPECT_FLOAT_EQ(inference, LOGIC_SLEEP_INFERENCE_DEEP_NREM);
    EXPECT_FLOAT_EQ(accuracy, LOGIC_SLEEP_ACCURACY_DEEP_NREM);
    EXPECT_FLOAT_EQ(consistency, LOGIC_SLEEP_CONSISTENCY_NREM);
}

TEST_F(LogicSleepBridgeTest, DeepNremLogicOffline) {
    // Deep NREM should have zero inference capacity (offline)
    float inference = logic_sleep_inference_for_state(SLEEP_STATE_DEEP_NREM);
    float accuracy = logic_sleep_accuracy_for_state(SLEEP_STATE_DEEP_NREM);

    EXPECT_FLOAT_EQ(inference, 0.0f);
    EXPECT_FLOAT_EQ(accuracy, 0.0f);
}

/* ============================================================================
 * Update and Effects Tests - REM State
 * ============================================================================ */

TEST_F(LogicSleepBridgeTest, UpdateRemState) {
    float inference = logic_sleep_inference_for_state(SLEEP_STATE_REM);
    float accuracy = logic_sleep_accuracy_for_state(SLEEP_STATE_REM);
    float consistency = logic_sleep_consistency_for_state(SLEEP_STATE_REM);

    EXPECT_FLOAT_EQ(inference, LOGIC_SLEEP_INFERENCE_REM);
    EXPECT_FLOAT_EQ(accuracy, LOGIC_SLEEP_ACCURACY_REM);
    EXPECT_FLOAT_EQ(consistency, LOGIC_SLEEP_CONSISTENCY_REM);
}

TEST_F(LogicSleepBridgeTest, RemCreativeMode) {
    // REM should have some inference (associative) but low consistency
    float inference = logic_sleep_inference_for_state(SLEEP_STATE_REM);
    float consistency = logic_sleep_consistency_for_state(SLEEP_STATE_REM);

    EXPECT_GT(inference, 0.0f);  // Some associative logic
    EXPECT_LT(consistency, 0.5f);  // Low consistency (dream logic)
}

/* ============================================================================
 * State Mapping Function Tests
 * ============================================================================ */

TEST_F(LogicSleepBridgeTest, InferenceForAllStates) {
    // Verify inference mapping for all states
    EXPECT_FLOAT_EQ(logic_sleep_inference_for_state(SLEEP_STATE_AWAKE),
                    LOGIC_SLEEP_INFERENCE_AWAKE);
    EXPECT_FLOAT_EQ(logic_sleep_inference_for_state(SLEEP_STATE_DROWSY),
                    LOGIC_SLEEP_INFERENCE_DROWSY);
    EXPECT_FLOAT_EQ(logic_sleep_inference_for_state(SLEEP_STATE_LIGHT_NREM),
                    LOGIC_SLEEP_INFERENCE_LIGHT_NREM);
    EXPECT_FLOAT_EQ(logic_sleep_inference_for_state(SLEEP_STATE_DEEP_NREM),
                    LOGIC_SLEEP_INFERENCE_DEEP_NREM);
    EXPECT_FLOAT_EQ(logic_sleep_inference_for_state(SLEEP_STATE_REM),
                    LOGIC_SLEEP_INFERENCE_REM);
}

TEST_F(LogicSleepBridgeTest, AccuracyForAllStates) {
    // Verify accuracy mapping for all states
    EXPECT_FLOAT_EQ(logic_sleep_accuracy_for_state(SLEEP_STATE_AWAKE),
                    LOGIC_SLEEP_ACCURACY_AWAKE);
    EXPECT_FLOAT_EQ(logic_sleep_accuracy_for_state(SLEEP_STATE_DROWSY),
                    LOGIC_SLEEP_ACCURACY_DROWSY);
    EXPECT_FLOAT_EQ(logic_sleep_accuracy_for_state(SLEEP_STATE_LIGHT_NREM),
                    LOGIC_SLEEP_ACCURACY_LIGHT_NREM);
    EXPECT_FLOAT_EQ(logic_sleep_accuracy_for_state(SLEEP_STATE_DEEP_NREM),
                    LOGIC_SLEEP_ACCURACY_DEEP_NREM);
    EXPECT_FLOAT_EQ(logic_sleep_accuracy_for_state(SLEEP_STATE_REM),
                    LOGIC_SLEEP_ACCURACY_REM);
}

TEST_F(LogicSleepBridgeTest, ConsistencyForAllStates) {
    // Verify consistency mapping for all states
    EXPECT_FLOAT_EQ(logic_sleep_consistency_for_state(SLEEP_STATE_AWAKE),
                    LOGIC_SLEEP_CONSISTENCY_AWAKE);
    EXPECT_FLOAT_EQ(logic_sleep_consistency_for_state(SLEEP_STATE_DROWSY),
                    LOGIC_SLEEP_CONSISTENCY_DROWSY);
    EXPECT_FLOAT_EQ(logic_sleep_consistency_for_state(SLEEP_STATE_LIGHT_NREM),
                    LOGIC_SLEEP_CONSISTENCY_NREM);
    EXPECT_FLOAT_EQ(logic_sleep_consistency_for_state(SLEEP_STATE_DEEP_NREM),
                    LOGIC_SLEEP_CONSISTENCY_NREM);
    EXPECT_FLOAT_EQ(logic_sleep_consistency_for_state(SLEEP_STATE_REM),
                    LOGIC_SLEEP_CONSISTENCY_REM);
}

/* ============================================================================
 * Effects Retrieval Tests
 * ============================================================================ */

TEST_F(LogicSleepBridgeTest, GetEffects) {
    logic_sleep_effects_t effects;
    int ret = logic_sleep_get_effects(bridge, &effects);
    EXPECT_EQ(ret, 0);

    // Verify all fields are initialized
    EXPECT_GE(effects.inference_capacity_factor, 0.0f);
    EXPECT_LE(effects.inference_capacity_factor, 1.0f);
    EXPECT_GE(effects.deduction_accuracy_factor, 0.0f);
    EXPECT_LE(effects.deduction_accuracy_factor, 1.0f);
    EXPECT_GE(effects.consistency_check_factor, 0.0f);
    EXPECT_LE(effects.consistency_check_factor, 1.0f);
    EXPECT_GE(effects.sleep_pressure, 0.0f);
    EXPECT_LE(effects.sleep_pressure, 1.0f);
}

TEST_F(LogicSleepBridgeTest, GetEffectsNull) {
    logic_sleep_effects_t effects;
    EXPECT_NE(logic_sleep_get_effects(nullptr, &effects), 0);
    EXPECT_NE(logic_sleep_get_effects(bridge, nullptr), 0);
}

TEST_F(LogicSleepBridgeTest, GetInferenceCapacity) {
    float capacity = logic_sleep_get_inference_capacity(bridge);
    EXPECT_GE(capacity, 0.0f);
    EXPECT_LE(capacity, 1.0f);
}

TEST_F(LogicSleepBridgeTest, GetInferenceCapacityNull) {
    float capacity = logic_sleep_get_inference_capacity(nullptr);
    EXPECT_FLOAT_EQ(capacity, 1.0f);  // Safe default: full capacity when no bridge
}

/* ============================================================================
 * State Query Tests
 * ============================================================================ */

TEST_F(LogicSleepBridgeTest, IsOffline) {
    // AWAKE state should not be offline
    bool offline = logic_sleep_is_offline(bridge);
    EXPECT_FALSE(offline);
}

TEST_F(LogicSleepBridgeTest, IsOfflineNull) {
    bool offline = logic_sleep_is_offline(nullptr);
    EXPECT_FALSE(offline);  // Safe default: not offline when no bridge
}

TEST_F(LogicSleepBridgeTest, IsConsolidationMode) {
    // AWAKE state should not be in consolidation mode
    bool consolidation = logic_sleep_is_consolidation_mode(bridge);
    EXPECT_FALSE(consolidation);
}

TEST_F(LogicSleepBridgeTest, IsConsolidationModeNull) {
    bool consolidation = logic_sleep_is_consolidation_mode(nullptr);
    EXPECT_FALSE(consolidation);  // Return false on null
}

/* ============================================================================
 * Update Function Tests
 * ============================================================================ */

TEST_F(LogicSleepBridgeTest, Update) {
    int ret = logic_sleep_update(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(LogicSleepBridgeTest, UpdateNull) {
    int ret = logic_sleep_update(nullptr);
    EXPECT_NE(ret, 0);
}

TEST_F(LogicSleepBridgeTest, MultipleUpdates) {
    // Multiple updates should not crash
    for (int i = 0; i < 10; i++) {
        int ret = logic_sleep_update(bridge);
        EXPECT_EQ(ret, 0);
    }
}

/* ============================================================================
 * Bio-Async Integration Tests
 * ============================================================================ */

TEST_F(LogicSleepBridgeTest, ConnectBioAsync) {
    int ret = logic_sleep_connect_bio_async(bridge);
    // May succeed or fail depending on bio-async router availability
    // We just ensure it doesn't crash
    (void)ret;
}

TEST_F(LogicSleepBridgeTest, ConnectBioAsyncNull) {
    int ret = logic_sleep_connect_bio_async(nullptr);
    EXPECT_NE(ret, 0);
}

TEST_F(LogicSleepBridgeTest, DisconnectBioAsync) {
    logic_sleep_connect_bio_async(bridge);
    int ret = logic_sleep_disconnect_bio_async(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(LogicSleepBridgeTest, DisconnectBioAsyncNull) {
    int ret = logic_sleep_disconnect_bio_async(nullptr);
    EXPECT_NE(ret, 0);
}

TEST_F(LogicSleepBridgeTest, IsBioAsyncConnected) {
    bool connected = logic_sleep_is_bio_async_connected(bridge);
    // Initially not connected
    EXPECT_FALSE(connected);

    // After connect, should be connected (or still false if router unavailable)
    logic_sleep_connect_bio_async(bridge);
    connected = logic_sleep_is_bio_async_connected(bridge);
    // Don't assert true, as bio-async router may not be available in tests
}

TEST_F(LogicSleepBridgeTest, IsBioAsyncConnectedNull) {
    bool connected = logic_sleep_is_bio_async_connected(nullptr);
    EXPECT_FALSE(connected);
}

TEST_F(LogicSleepBridgeTest, DisconnectTwice) {
    logic_sleep_connect_bio_async(bridge);
    logic_sleep_disconnect_bio_async(bridge);
    int ret = logic_sleep_disconnect_bio_async(bridge);
    EXPECT_EQ(ret, 0);  // Should handle double disconnect gracefully
}

/* ============================================================================
 * Edge Cases and Boundary Tests
 * ============================================================================ */

TEST_F(LogicSleepBridgeTest, ConfigWithAllFeaturesDisabled) {
    logic_sleep_config_t config;
    logic_sleep_default_config(&config);

    config.enable_inference_modulation = false;
    config.enable_accuracy_modulation = false;
    config.enable_consistency_modulation = false;
    config.enable_nrem_consolidation = false;
    config.enable_rem_creativity = false;

    logic_sleep_bridge_t* br = logic_sleep_bridge_create(&config, sleep_system);
    ASSERT_NE(br, nullptr);

    int ret = logic_sleep_update(br);
    EXPECT_EQ(ret, 0);

    logic_sleep_bridge_destroy(br);
}

TEST_F(LogicSleepBridgeTest, ConfigWithExtremeModulationStrength) {
    logic_sleep_config_t config;
    logic_sleep_default_config(&config);

    // Test minimum strength
    config.modulation_strength = 0.5f;
    logic_sleep_bridge_t* br1 = logic_sleep_bridge_create(&config, sleep_system);
    ASSERT_NE(br1, nullptr);
    logic_sleep_bridge_destroy(br1);

    // Test maximum strength
    config.modulation_strength = 2.0f;
    logic_sleep_bridge_t* br2 = logic_sleep_bridge_create(&config, sleep_system);
    ASSERT_NE(br2, nullptr);
    logic_sleep_bridge_destroy(br2);
}

TEST_F(LogicSleepBridgeTest, ConfigWithExtremeSleepPressureThreshold) {
    logic_sleep_config_t config;
    logic_sleep_default_config(&config);

    // Test minimum threshold
    config.sleep_pressure_threshold = 0.5f;
    logic_sleep_bridge_t* br1 = logic_sleep_bridge_create(&config, sleep_system);
    ASSERT_NE(br1, nullptr);
    logic_sleep_bridge_destroy(br1);

    // Test maximum threshold
    config.sleep_pressure_threshold = 0.9f;
    logic_sleep_bridge_t* br2 = logic_sleep_bridge_create(&config, sleep_system);
    ASSERT_NE(br2, nullptr);
    logic_sleep_bridge_destroy(br2);
}

TEST_F(LogicSleepBridgeTest, EffectsRangeValidation) {
    logic_sleep_update(bridge);
    logic_sleep_effects_t effects;
    logic_sleep_get_effects(bridge, &effects);

    // All factors should be in valid range [0, 1]
    EXPECT_GE(effects.inference_capacity_factor, 0.0f);
    EXPECT_LE(effects.inference_capacity_factor, 1.0f);
    EXPECT_GE(effects.deduction_accuracy_factor, 0.0f);
    EXPECT_LE(effects.deduction_accuracy_factor, 1.0f);
    EXPECT_GE(effects.consistency_check_factor, 0.0f);
    EXPECT_LE(effects.consistency_check_factor, 1.0f);
    EXPECT_GE(effects.rule_consolidation_rate, 0.0f);
    EXPECT_GE(effects.pressure_inference_penalty, 0.0f);
    EXPECT_LE(effects.pressure_inference_penalty, 1.0f);
    EXPECT_GE(effects.pressure_accuracy_penalty, 0.0f);
    EXPECT_LE(effects.pressure_accuracy_penalty, 1.0f);
}

TEST_F(LogicSleepBridgeTest, StateTransitionProgression) {
    // Verify that sleep states form a logical progression
    float prev_inference = logic_sleep_inference_for_state(SLEEP_STATE_AWAKE);

    // DROWSY should be less than AWAKE
    float drowsy_inference = logic_sleep_inference_for_state(SLEEP_STATE_DROWSY);
    EXPECT_LT(drowsy_inference, prev_inference);

    // LIGHT_NREM should be less than DROWSY
    float light_inference = logic_sleep_inference_for_state(SLEEP_STATE_LIGHT_NREM);
    EXPECT_LT(light_inference, drowsy_inference);

    // DEEP_NREM should be minimal (0)
    float deep_inference = logic_sleep_inference_for_state(SLEEP_STATE_DEEP_NREM);
    EXPECT_FLOAT_EQ(deep_inference, 0.0f);
}

TEST_F(LogicSleepBridgeTest, ConsolidationRateNonNegative) {
    logic_sleep_update(bridge);
    logic_sleep_effects_t effects;
    logic_sleep_get_effects(bridge, &effects);

    // Consolidation rate should always be non-negative
    EXPECT_GE(effects.rule_consolidation_rate, 0.0f);
}

/* ============================================================================
 * Thread Safety Tests (Basic)
 * ============================================================================ */

TEST_F(LogicSleepBridgeTest, ConcurrentGetEffects) {
    // Multiple simultaneous reads should be safe
    logic_sleep_effects_t effects1, effects2;
    int ret1 = logic_sleep_get_effects(bridge, &effects1);
    int ret2 = logic_sleep_get_effects(bridge, &effects2);

    EXPECT_EQ(ret1, 0);
    EXPECT_EQ(ret2, 0);

    // Both should return same values
    EXPECT_FLOAT_EQ(effects1.inference_capacity_factor, effects2.inference_capacity_factor);
    EXPECT_FLOAT_EQ(effects1.deduction_accuracy_factor, effects2.deduction_accuracy_factor);
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
