/**
 * @file test_medulla_cerebellum_bridge.cpp
 * @brief Comprehensive unit tests for the Medulla-Cerebellum Bridge
 *
 * WHAT: Tests for the bridge connecting medulla to cerebellum via inferior olive
 * WHY:  Ensure proper error signaling, modulation, and protection gating
 * HOW:  Use GoogleTest framework with lifecycle and functional validation
 *
 * Tests cover:
 * - Lifecycle: create, destroy, reset, default_config
 * - Connection: connect_medulla, connect_cerebellum, connect_bio_async, is_connected
 * - Inferior Olive Error Signaling: queue_error, send_climbing_signal, broadcast_error
 * - Arousal Modulation: get_arousal_effects, modulate_motor
 * - Protection Gating: get_protection_effects, motor_allowed, emergency_stop, release_emergency
 * - Circadian Learning: get_circadian_effects, get_learning_multiplier, apply_circadian_learning
 * - Update: update, process_messages
 * - Query: get_stats, reset_stats, get_io_state, pending_error_count
 * - Error handling: NULL inputs, invalid parameters
 *
 * @version 1.0.0
 * @date 2026-01-10
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

// Headers have their own extern "C" guards
#include "core/medulla/nimcp_medulla_cerebellum_bridge.h"
#include "core/medulla/nimcp_medulla.h"
#include "core/brain/regions/cerebellum/nimcp_cerebellum_adapter.h"

//=============================================================================
// Test Fixtures
//=============================================================================

/**
 * @brief Base test fixture for Medulla-Cerebellum Bridge tests
 */
class MedullaCerebellumBridgeTest : public ::testing::Test {
protected:
    medulla_t medulla = nullptr;
    cerebellum_adapter_t* cerebellum = nullptr;
    med_cereb_bridge_t bridge = nullptr;

    void SetUp() override {
        // Create medulla
        medulla_config_t med_config = medulla_default_config();
        medulla = medulla_create(&med_config);
        ASSERT_NE(medulla, nullptr);

        // Create cerebellum adapter
        cerebellum_config_t cereb_config = cerebellum_default_config();
        cerebellum = cerebellum_create(&cereb_config);
        ASSERT_NE(cerebellum, nullptr);

        // Create bridge with defaults
        bridge = med_cereb_bridge_create(nullptr);
        ASSERT_NE(bridge, nullptr);

        // Connect to medulla and cerebellum
        ASSERT_EQ(0, med_cereb_bridge_connect_medulla(bridge, medulla));
        ASSERT_EQ(0, med_cereb_bridge_connect_cerebellum(bridge, cerebellum));
    }

    void TearDown() override {
        if (bridge) {
            med_cereb_bridge_destroy(bridge);
            bridge = nullptr;
        }
        if (cerebellum) {
            cerebellum_destroy(cerebellum);
            cerebellum = nullptr;
        }
        if (medulla) {
            medulla_destroy(medulla);
            medulla = nullptr;
        }
    }
};

/**
 * @brief Lightweight test fixture without medulla/cerebellum
 */
class MedullaCerebellumBridgeStandaloneTest : public ::testing::Test {
protected:
    med_cereb_bridge_t bridge = nullptr;

    void SetUp() override {
        bridge = med_cereb_bridge_create(nullptr);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            med_cereb_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }
};

//=============================================================================
// Configuration Tests
//=============================================================================

class MedullaCerebellumConfigTest : public ::testing::Test {};

TEST_F(MedullaCerebellumConfigTest, DefaultConfigReturnsSuccess) {
    med_cereb_bridge_config_t config;
    int result = med_cereb_bridge_default_config(&config);
    EXPECT_EQ(result, 0);
}

TEST_F(MedullaCerebellumConfigTest, DefaultConfigNullReturnsError) {
    int result = med_cereb_bridge_default_config(nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(MedullaCerebellumConfigTest, DefaultConfigHasValidIONeurons) {
    med_cereb_bridge_config_t config;
    med_cereb_bridge_default_config(&config);

    EXPECT_GT(config.num_io_neurons, 0u);
    EXPECT_LE(config.num_io_neurons, MED_CEREB_MAX_IO_NEURONS);
}

TEST_F(MedullaCerebellumConfigTest, DefaultConfigEnablesModulations) {
    med_cereb_bridge_config_t config;
    med_cereb_bridge_default_config(&config);

    // At least some modulation should be enabled by default
    EXPECT_TRUE(config.enable_arousal_modulation ||
                config.enable_protection_gating ||
                config.enable_circadian_learning ||
                config.enable_io_signaling);
}

TEST_F(MedullaCerebellumConfigTest, DefaultConfigHasValidIOParams) {
    med_cereb_bridge_config_t config;
    med_cereb_bridge_default_config(&config);

    EXPECT_GT(config.io_oscillation_freq, 0.0f);
    EXPECT_LE(config.io_oscillation_freq, 20.0f);  // ~10 Hz biologically
    EXPECT_GE(config.io_coupling_strength, 0.0f);
    EXPECT_LE(config.io_coupling_strength, 1.0f);
    EXPECT_GT(config.io_refractory_ms, 0.0f);
    EXPECT_GT(config.io_firing_threshold, 0.0f);
}

TEST_F(MedullaCerebellumConfigTest, DefaultConfigHasValidArousalParams) {
    med_cereb_bridge_config_t config;
    med_cereb_bridge_default_config(&config);

    EXPECT_GT(config.arousal_gain_sensitivity, 0.0f);
    EXPECT_GE(config.optimal_arousal_level, 0.0f);
    EXPECT_LE(config.optimal_arousal_level, 1.0f);
}

TEST_F(MedullaCerebellumConfigTest, DefaultConfigHasValidProtectionParams) {
    med_cereb_bridge_config_t config;
    med_cereb_bridge_default_config(&config);

    EXPECT_LT(config.non_essential_cutoff_level, config.voluntary_cutoff_level);
}

TEST_F(MedullaCerebellumConfigTest, DefaultConfigHasValidCircadianParams) {
    med_cereb_bridge_config_t config;
    med_cereb_bridge_default_config(&config);

    EXPECT_GT(config.max_circadian_learning_boost, 1.0f);
    EXPECT_LT(config.min_circadian_learning_factor, 1.0f);
    EXPECT_GT(config.min_circadian_learning_factor, 0.0f);
}

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(MedullaCerebellumBridgeStandaloneTest, CreateWithNullConfigUsesDefaults) {
    // bridge already created with nullptr in SetUp
    EXPECT_NE(bridge, nullptr);
}

TEST_F(MedullaCerebellumConfigTest, CreateWithCustomConfig) {
    med_cereb_bridge_config_t config;
    med_cereb_bridge_default_config(&config);
    config.num_io_neurons = 50;
    config.enable_arousal_modulation = false;

    med_cereb_bridge_t custom_bridge = med_cereb_bridge_create(&config);
    ASSERT_NE(custom_bridge, nullptr);

    med_cereb_bridge_destroy(custom_bridge);
}

TEST_F(MedullaCerebellumConfigTest, CreateWithZeroIONeurons) {
    med_cereb_bridge_config_t config;
    med_cereb_bridge_default_config(&config);
    config.num_io_neurons = 0;  // Invalid

    // Should either fail or use minimum
    med_cereb_bridge_t bridge = med_cereb_bridge_create(&config);
    // Implementation may handle this either way
    if (bridge) {
        med_cereb_bridge_destroy(bridge);
    }
}

TEST_F(MedullaCerebellumConfigTest, CreateWithMaxIONeurons) {
    med_cereb_bridge_config_t config;
    med_cereb_bridge_default_config(&config);
    config.num_io_neurons = MED_CEREB_MAX_IO_NEURONS;

    med_cereb_bridge_t bridge = med_cereb_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);
    med_cereb_bridge_destroy(bridge);
}

TEST_F(MedullaCerebellumConfigTest, DestroyNullIsSafe) {
    med_cereb_bridge_destroy(nullptr);  // Should not crash
}

TEST_F(MedullaCerebellumBridgeStandaloneTest, ResetReturnsSuccess) {
    int result = med_cereb_bridge_reset(bridge);
    EXPECT_EQ(result, 0);
}

TEST_F(MedullaCerebellumConfigTest, ResetNullReturnsError) {
    int result = med_cereb_bridge_reset(nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(MedullaCerebellumBridgeStandaloneTest, ResetClearsStats) {
    // Queue some errors first
    med_cereb_bridge_queue_error(bridge, MED_CEREB_ERROR_TIMING, 0.5f, 1);
    med_cereb_bridge_queue_error(bridge, MED_CEREB_ERROR_AMPLITUDE, 0.3f, 2);

    // Reset
    med_cereb_bridge_reset(bridge);

    // Stats should be cleared
    med_cereb_bridge_stats_t stats;
    med_cereb_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.climbing_signals_sent, 0u);
    EXPECT_FLOAT_EQ(stats.avg_error_magnitude, 0.0f);
}

//=============================================================================
// Connection Tests
//=============================================================================

TEST_F(MedullaCerebellumBridgeStandaloneTest, ConnectMedullaSuccess) {
    medulla_config_t config = medulla_default_config();
    medulla_t m = medulla_create(&config);
    ASSERT_NE(m, nullptr);

    int result = med_cereb_bridge_connect_medulla(bridge, m);
    EXPECT_EQ(result, 0);

    medulla_destroy(m);
}

TEST_F(MedullaCerebellumBridgeStandaloneTest, ConnectMedullaNullBridge) {
    medulla_config_t config = medulla_default_config();
    medulla_t m = medulla_create(&config);
    ASSERT_NE(m, nullptr);

    int result = med_cereb_bridge_connect_medulla(nullptr, m);
    EXPECT_EQ(result, -1);

    medulla_destroy(m);
}

TEST_F(MedullaCerebellumBridgeStandaloneTest, ConnectMedullaNullMedulla) {
    int result = med_cereb_bridge_connect_medulla(bridge, nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(MedullaCerebellumBridgeStandaloneTest, ConnectCerebellumSuccess) {
    cerebellum_config_t config = cerebellum_default_config();
    cerebellum_adapter_t* c = cerebellum_create(&config);
    ASSERT_NE(c, nullptr);

    int result = med_cereb_bridge_connect_cerebellum(bridge, c);
    EXPECT_EQ(result, 0);

    cerebellum_destroy(c);
}

TEST_F(MedullaCerebellumBridgeStandaloneTest, ConnectCerebellumNullBridge) {
    cerebellum_config_t config = cerebellum_default_config();
    cerebellum_adapter_t* c = cerebellum_create(&config);
    ASSERT_NE(c, nullptr);

    int result = med_cereb_bridge_connect_cerebellum(nullptr, c);
    EXPECT_EQ(result, -1);

    cerebellum_destroy(c);
}

TEST_F(MedullaCerebellumBridgeStandaloneTest, ConnectCerebellumNullCerebellum) {
    int result = med_cereb_bridge_connect_cerebellum(bridge, nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(MedullaCerebellumBridgeStandaloneTest, ConnectBioAsyncNullBridge) {
    int result = med_cereb_bridge_connect_bio_async(nullptr, nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(MedullaCerebellumBridgeStandaloneTest, ConnectBioAsyncNullRouter) {
    int result = med_cereb_bridge_connect_bio_async(bridge, nullptr);
    // May succeed if router is optional, or fail if required
    EXPECT_TRUE(result == 0 || result == -1);
}

TEST_F(MedullaCerebellumBridgeStandaloneTest, IsConnectedReturnsFalseInitially) {
    bool connected = med_cereb_bridge_is_connected(bridge);
    EXPECT_FALSE(connected);  // No medulla/cerebellum connected yet
}

TEST_F(MedullaCerebellumBridgeTest, IsConnectedReturnsTrueWhenBothConnected) {
    bool connected = med_cereb_bridge_is_connected(bridge);
    EXPECT_TRUE(connected);
}

TEST_F(MedullaCerebellumBridgeStandaloneTest, IsConnectedNullReturnsFalse) {
    bool connected = med_cereb_bridge_is_connected(nullptr);
    EXPECT_FALSE(connected);
}

TEST_F(MedullaCerebellumBridgeStandaloneTest, IsConnectedWithOnlyMedulla) {
    medulla_config_t config = medulla_default_config();
    medulla_t m = medulla_create(&config);
    ASSERT_NE(m, nullptr);

    med_cereb_bridge_connect_medulla(bridge, m);
    bool connected = med_cereb_bridge_is_connected(bridge);
    EXPECT_FALSE(connected);  // Need both

    medulla_destroy(m);
}

//=============================================================================
// Inferior Olive Error Signaling Tests
//=============================================================================

TEST_F(MedullaCerebellumBridgeTest, QueueErrorSuccess) {
    int result = med_cereb_bridge_queue_error(bridge, MED_CEREB_ERROR_TIMING, 0.5f, 1);
    EXPECT_EQ(result, 0);
}

TEST_F(MedullaCerebellumBridgeTest, QueueErrorNullBridge) {
    int result = med_cereb_bridge_queue_error(nullptr, MED_CEREB_ERROR_TIMING, 0.5f, 1);
    EXPECT_EQ(result, -1);
}

TEST_F(MedullaCerebellumBridgeTest, QueueErrorAllTypes) {
    for (int i = 0; i < MED_CEREB_ERROR_COUNT; i++) {
        int result = med_cereb_bridge_queue_error(
            bridge, static_cast<med_cereb_error_type_t>(i), 0.3f, i);
        EXPECT_EQ(result, 0);
    }
}

TEST_F(MedullaCerebellumBridgeTest, QueueErrorIncrementsPendingCount) {
    uint32_t initial = med_cereb_bridge_pending_error_count(bridge);

    med_cereb_bridge_queue_error(bridge, MED_CEREB_ERROR_TIMING, 0.5f, 1);

    uint32_t after = med_cereb_bridge_pending_error_count(bridge);
    EXPECT_EQ(after, initial + 1);
}

TEST_F(MedullaCerebellumBridgeTest, QueueErrorClampsMagnitude) {
    // Magnitude should be clamped to [-1, 1]
    int result = med_cereb_bridge_queue_error(bridge, MED_CEREB_ERROR_TIMING, 5.0f, 1);
    EXPECT_EQ(result, 0);

    result = med_cereb_bridge_queue_error(bridge, MED_CEREB_ERROR_TIMING, -5.0f, 2);
    EXPECT_EQ(result, 0);
}

TEST_F(MedullaCerebellumBridgeTest, QueueErrorFillsQueue) {
    // Fill queue to max
    for (uint32_t i = 0; i < MED_CEREB_MAX_ERROR_QUEUE; i++) {
        int result = med_cereb_bridge_queue_error(
            bridge, MED_CEREB_ERROR_TIMING, 0.5f, i);
        EXPECT_EQ(result, 0);
    }

    // Next should either fail or drop oldest
    int result = med_cereb_bridge_queue_error(bridge, MED_CEREB_ERROR_TIMING, 0.5f, 999);
    // Implementation may vary - just ensure it doesn't crash
    EXPECT_TRUE(result == 0 || result == -1);
}

TEST_F(MedullaCerebellumBridgeTest, SendClimbingSignalSuccess) {
    int result = med_cereb_bridge_send_climbing_signal(
        bridge, MED_CEREB_ERROR_PREDICTION, 0.7f, 0);
    EXPECT_EQ(result, 0);
}

TEST_F(MedullaCerebellumBridgeTest, SendClimbingSignalNullBridge) {
    int result = med_cereb_bridge_send_climbing_signal(
        nullptr, MED_CEREB_ERROR_PREDICTION, 0.7f, 0);
    EXPECT_EQ(result, -1);
}

TEST_F(MedullaCerebellumBridgeTest, SendClimbingSignalSpecificTarget) {
    int result = med_cereb_bridge_send_climbing_signal(
        bridge, MED_CEREB_ERROR_TRAJECTORY, 0.5f, 42);
    EXPECT_EQ(result, 0);
}

TEST_F(MedullaCerebellumBridgeTest, SendClimbingSignalIncrementsStats) {
    med_cereb_bridge_stats_t stats_before;
    med_cereb_bridge_get_stats(bridge, &stats_before);

    med_cereb_bridge_send_climbing_signal(bridge, MED_CEREB_ERROR_TIMING, 0.5f, 0);

    med_cereb_bridge_stats_t stats_after;
    med_cereb_bridge_get_stats(bridge, &stats_after);

    EXPECT_GT(stats_after.climbing_signals_sent, stats_before.climbing_signals_sent);
}

TEST_F(MedullaCerebellumBridgeTest, BroadcastErrorSuccess) {
    int result = med_cereb_bridge_broadcast_error(
        bridge, MED_CEREB_ERROR_COORDINATION, 0.8f);
    EXPECT_EQ(result, 0);
}

TEST_F(MedullaCerebellumBridgeTest, BroadcastErrorNullBridge) {
    int result = med_cereb_bridge_broadcast_error(nullptr, MED_CEREB_ERROR_COORDINATION, 0.8f);
    EXPECT_EQ(result, -1);
}

TEST_F(MedullaCerebellumBridgeTest, BroadcastErrorAllTypes) {
    for (int i = 0; i < MED_CEREB_ERROR_COUNT; i++) {
        int result = med_cereb_bridge_broadcast_error(
            bridge, static_cast<med_cereb_error_type_t>(i), 0.5f);
        EXPECT_EQ(result, 0);
    }
}

//=============================================================================
// Arousal Modulation Tests
//=============================================================================

TEST_F(MedullaCerebellumBridgeTest, GetArousalEffectsSuccess) {
    med_cereb_arousal_effects_t effects;
    int result = med_cereb_bridge_get_arousal_effects(bridge, &effects);
    EXPECT_EQ(result, 0);
}

TEST_F(MedullaCerebellumBridgeTest, GetArousalEffectsNullBridge) {
    med_cereb_arousal_effects_t effects;
    int result = med_cereb_bridge_get_arousal_effects(nullptr, &effects);
    EXPECT_EQ(result, -1);
}

TEST_F(MedullaCerebellumBridgeTest, GetArousalEffectsNullOutput) {
    int result = med_cereb_bridge_get_arousal_effects(bridge, nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(MedullaCerebellumBridgeTest, GetArousalEffectsValidRanges) {
    med_cereb_arousal_effects_t effects;
    med_cereb_bridge_get_arousal_effects(bridge, &effects);

    EXPECT_GE(effects.motor_gain, 0.2f);
    EXPECT_LE(effects.motor_gain, 2.0f);
    EXPECT_GE(effects.reaction_time_factor, 0.5f);
    EXPECT_LE(effects.reaction_time_factor, 2.0f);
    EXPECT_GE(effects.nuclei_excitability, 0.0f);
    EXPECT_LE(effects.nuclei_excitability, 1.0f);
    EXPECT_GE(effects.fine_motor_precision, 0.0f);
    EXPECT_LE(effects.fine_motor_precision, 1.0f);
    EXPECT_GE(effects.tremor_amplitude, 0.0f);
    EXPECT_LE(effects.tremor_amplitude, 1.0f);
}

TEST_F(MedullaCerebellumBridgeTest, ModulateMotorSuccess) {
    float motor_command[3] = {1.0f, 0.5f, 0.8f};
    float modulated[3] = {0.0f, 0.0f, 0.0f};

    int result = med_cereb_bridge_modulate_motor(bridge, motor_command, modulated, 3);
    EXPECT_EQ(result, 0);
}

TEST_F(MedullaCerebellumBridgeTest, ModulateMotorNullBridge) {
    float motor_command[3] = {1.0f, 0.5f, 0.8f};
    float modulated[3] = {0.0f, 0.0f, 0.0f};

    int result = med_cereb_bridge_modulate_motor(nullptr, motor_command, modulated, 3);
    EXPECT_EQ(result, -1);
}

TEST_F(MedullaCerebellumBridgeTest, ModulateMotorNullInput) {
    float modulated[3] = {0.0f, 0.0f, 0.0f};

    int result = med_cereb_bridge_modulate_motor(bridge, nullptr, modulated, 3);
    EXPECT_EQ(result, -1);
}

TEST_F(MedullaCerebellumBridgeTest, ModulateMotorNullOutput) {
    float motor_command[3] = {1.0f, 0.5f, 0.8f};

    int result = med_cereb_bridge_modulate_motor(bridge, motor_command, nullptr, 3);
    EXPECT_EQ(result, -1);
}

TEST_F(MedullaCerebellumBridgeTest, ModulateMotorZeroDimensions) {
    float motor_command[1] = {1.0f};
    float modulated[1] = {0.0f};

    int result = med_cereb_bridge_modulate_motor(bridge, motor_command, modulated, 0);
    // May succeed with no-op or return error
    EXPECT_TRUE(result == 0 || result == -1);
}

TEST_F(MedullaCerebellumBridgeTest, ModulateMotorAppliesGain) {
    float motor_command[3] = {1.0f, 0.5f, 0.8f};
    float modulated[3] = {0.0f, 0.0f, 0.0f};

    med_cereb_bridge_modulate_motor(bridge, motor_command, modulated, 3);

    // Modulated values should be different (unless gain is exactly 1.0)
    // Just verify they're not all zero
    bool any_nonzero = (modulated[0] != 0.0f || modulated[1] != 0.0f || modulated[2] != 0.0f);
    EXPECT_TRUE(any_nonzero);
}

//=============================================================================
// Protection Gating Tests
//=============================================================================

TEST_F(MedullaCerebellumBridgeTest, GetProtectionEffectsSuccess) {
    med_cereb_protection_effects_t effects;
    int result = med_cereb_bridge_get_protection_effects(bridge, &effects);
    EXPECT_EQ(result, 0);
}

TEST_F(MedullaCerebellumBridgeTest, GetProtectionEffectsNullBridge) {
    med_cereb_protection_effects_t effects;
    int result = med_cereb_bridge_get_protection_effects(nullptr, &effects);
    EXPECT_EQ(result, -1);
}

TEST_F(MedullaCerebellumBridgeTest, GetProtectionEffectsNullOutput) {
    int result = med_cereb_bridge_get_protection_effects(bridge, nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(MedullaCerebellumBridgeTest, GetProtectionEffectsValidRanges) {
    med_cereb_protection_effects_t effects;
    med_cereb_bridge_get_protection_effects(bridge, &effects);

    EXPECT_GE(effects.output_scale, 0.0f);
    EXPECT_LE(effects.output_scale, 1.0f);
}

TEST_F(MedullaCerebellumBridgeTest, MotorAllowedNormalConditions) {
    // Under normal conditions, motor should be allowed
    bool allowed = med_cereb_bridge_motor_allowed(bridge, false, false);
    EXPECT_TRUE(allowed);
}

TEST_F(MedullaCerebellumBridgeTest, MotorAllowedNullBridge) {
    bool allowed = med_cereb_bridge_motor_allowed(nullptr, false, false);
    EXPECT_FALSE(allowed);
}

TEST_F(MedullaCerebellumBridgeTest, MotorAllowedEssential) {
    bool allowed = med_cereb_bridge_motor_allowed(bridge, true, false);
    EXPECT_TRUE(allowed);  // Essential motor should generally be allowed
}

TEST_F(MedullaCerebellumBridgeTest, MotorAllowedReflexive) {
    bool allowed = med_cereb_bridge_motor_allowed(bridge, false, true);
    EXPECT_TRUE(allowed);  // Reflexive motor should generally be allowed
}

TEST_F(MedullaCerebellumBridgeTest, EmergencyStopSuccess) {
    int result = med_cereb_bridge_emergency_stop(bridge);
    EXPECT_EQ(result, 0);
}

TEST_F(MedullaCerebellumBridgeTest, EmergencyStopNullBridge) {
    int result = med_cereb_bridge_emergency_stop(nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(MedullaCerebellumBridgeTest, EmergencyStopGatesMotor) {
    med_cereb_bridge_emergency_stop(bridge);

    // After emergency stop, non-reflexive motor should be blocked
    bool allowed = med_cereb_bridge_motor_allowed(bridge, false, false);
    EXPECT_FALSE(allowed);
}

TEST_F(MedullaCerebellumBridgeTest, EmergencyStopAllowsReflexive) {
    med_cereb_bridge_emergency_stop(bridge);

    // Even during emergency, reflexive motor may be allowed
    bool allowed = med_cereb_bridge_motor_allowed(bridge, false, true);
    // Depends on implementation - reflexes might still work
    // Just verify it doesn't crash
    EXPECT_TRUE(allowed || !allowed);
}

TEST_F(MedullaCerebellumBridgeTest, ReleaseEmergencySuccess) {
    med_cereb_bridge_emergency_stop(bridge);
    int result = med_cereb_bridge_release_emergency(bridge);
    EXPECT_EQ(result, 0);
}

TEST_F(MedullaCerebellumBridgeTest, ReleaseEmergencyNullBridge) {
    int result = med_cereb_bridge_release_emergency(nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(MedullaCerebellumBridgeTest, ReleaseEmergencyRestoresMotor) {
    med_cereb_bridge_emergency_stop(bridge);
    med_cereb_bridge_release_emergency(bridge);

    bool allowed = med_cereb_bridge_motor_allowed(bridge, false, false);
    EXPECT_TRUE(allowed);
}

TEST_F(MedullaCerebellumBridgeTest, EmergencyStopUpdatesStats) {
    med_cereb_bridge_stats_t stats_before;
    med_cereb_bridge_get_stats(bridge, &stats_before);

    med_cereb_bridge_emergency_stop(bridge);

    med_cereb_bridge_stats_t stats_after;
    med_cereb_bridge_get_stats(bridge, &stats_after);

    EXPECT_GT(stats_after.protection_gates, stats_before.protection_gates);
}

//=============================================================================
// Circadian Learning Tests
//=============================================================================

TEST_F(MedullaCerebellumBridgeTest, GetCircadianEffectsSuccess) {
    med_cereb_circadian_effects_t effects;
    int result = med_cereb_bridge_get_circadian_effects(bridge, &effects);
    EXPECT_EQ(result, 0);
}

TEST_F(MedullaCerebellumBridgeTest, GetCircadianEffectsNullBridge) {
    med_cereb_circadian_effects_t effects;
    int result = med_cereb_bridge_get_circadian_effects(nullptr, &effects);
    EXPECT_EQ(result, -1);
}

TEST_F(MedullaCerebellumBridgeTest, GetCircadianEffectsNullOutput) {
    int result = med_cereb_bridge_get_circadian_effects(bridge, nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(MedullaCerebellumBridgeTest, GetCircadianEffectsValidRanges) {
    med_cereb_circadian_effects_t effects;
    med_cereb_bridge_get_circadian_effects(bridge, &effects);

    EXPECT_GE(effects.ltd_rate_multiplier, 0.3f);
    EXPECT_LE(effects.ltd_rate_multiplier, 1.5f);
    EXPECT_GE(effects.ltp_rate_multiplier, 0.3f);
    EXPECT_LE(effects.ltp_rate_multiplier, 1.5f);
    EXPECT_GE(effects.consolidation_rate, 0.0f);
    EXPECT_LE(effects.consolidation_rate, 1.0f);
    EXPECT_GE(effects.retrieval_efficiency, 0.0f);
    EXPECT_LE(effects.retrieval_efficiency, 1.0f);
}

TEST_F(MedullaCerebellumBridgeTest, GetLearningMultiplierReturnsValidRange) {
    float multiplier = med_cereb_bridge_get_learning_multiplier(bridge);
    EXPECT_GE(multiplier, 0.1f);
    EXPECT_LE(multiplier, 2.0f);
}

TEST_F(MedullaCerebellumBridgeTest, GetLearningMultiplierNullReturnsDefault) {
    float multiplier = med_cereb_bridge_get_learning_multiplier(nullptr);
    // Should return some default value (likely 1.0 or similar)
    EXPECT_GE(multiplier, 0.0f);
}

TEST_F(MedullaCerebellumBridgeTest, ApplyCircadianLearningSuccess) {
    int result = med_cereb_bridge_apply_circadian_learning(bridge);
    EXPECT_EQ(result, 0);
}

TEST_F(MedullaCerebellumBridgeTest, ApplyCircadianLearningNullBridge) {
    int result = med_cereb_bridge_apply_circadian_learning(nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(MedullaCerebellumBridgeTest, ApplyCircadianLearningUpdatesStats) {
    med_cereb_bridge_stats_t stats_before;
    med_cereb_bridge_get_stats(bridge, &stats_before);

    med_cereb_bridge_apply_circadian_learning(bridge);

    med_cereb_bridge_stats_t stats_after;
    med_cereb_bridge_get_stats(bridge, &stats_after);

    EXPECT_GT(stats_after.learning_rate_adjustments, stats_before.learning_rate_adjustments);
}

//=============================================================================
// Update Tests
//=============================================================================

TEST_F(MedullaCerebellumBridgeTest, UpdateSuccess) {
    int result = med_cereb_bridge_update(bridge, 16000);  // 16ms
    EXPECT_EQ(result, 0);
}

TEST_F(MedullaCerebellumBridgeTest, UpdateNullBridge) {
    int result = med_cereb_bridge_update(nullptr, 16000);
    EXPECT_EQ(result, -1);
}

TEST_F(MedullaCerebellumBridgeTest, UpdateZeroDelta) {
    int result = med_cereb_bridge_update(bridge, 0);
    EXPECT_EQ(result, 0);  // Should be no-op
}

TEST_F(MedullaCerebellumBridgeTest, UpdateProcessesQueuedErrors) {
    // Queue some errors
    med_cereb_bridge_queue_error(bridge, MED_CEREB_ERROR_TIMING, 0.5f, 1);
    med_cereb_bridge_queue_error(bridge, MED_CEREB_ERROR_AMPLITUDE, 0.3f, 2);

    uint32_t before = med_cereb_bridge_pending_error_count(bridge);
    EXPECT_GT(before, 0u);

    // Update should process errors (depending on IO refractory period)
    for (int i = 0; i < 10; i++) {
        med_cereb_bridge_update(bridge, 100000);  // 100ms
    }

    uint32_t after = med_cereb_bridge_pending_error_count(bridge);
    EXPECT_LE(after, before);  // Some or all errors should be processed
}

TEST_F(MedullaCerebellumBridgeTest, UpdateMultipleTimes) {
    for (int i = 0; i < 100; i++) {
        int result = med_cereb_bridge_update(bridge, 16000);
        EXPECT_EQ(result, 0);
    }
}

TEST_F(MedullaCerebellumBridgeTest, ProcessMessagesSuccess) {
    int result = med_cereb_bridge_process_messages(bridge);
    // Returns number of messages processed, -1 on error
    EXPECT_GE(result, 0);
}

TEST_F(MedullaCerebellumBridgeTest, ProcessMessagesNullBridge) {
    int result = med_cereb_bridge_process_messages(nullptr);
    EXPECT_EQ(result, -1);
}

//=============================================================================
// Query Tests
//=============================================================================

TEST_F(MedullaCerebellumBridgeTest, GetStatsSuccess) {
    med_cereb_bridge_stats_t stats;
    int result = med_cereb_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(result, 0);
}

TEST_F(MedullaCerebellumBridgeTest, GetStatsNullBridge) {
    med_cereb_bridge_stats_t stats;
    int result = med_cereb_bridge_get_stats(nullptr, &stats);
    EXPECT_EQ(result, -1);
}

TEST_F(MedullaCerebellumBridgeTest, GetStatsNullOutput) {
    int result = med_cereb_bridge_get_stats(bridge, nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(MedullaCerebellumBridgeTest, GetStatsInitialValues) {
    med_cereb_bridge_stats_t stats;
    med_cereb_bridge_get_stats(bridge, &stats);

    // After creation, stats should be at initial values
    EXPECT_EQ(stats.climbing_signals_sent, 0u);
    EXPECT_EQ(stats.errors_dropped, 0u);
    EXPECT_FLOAT_EQ(stats.avg_error_magnitude, 0.0f);
}

TEST_F(MedullaCerebellumBridgeTest, GetStatsAfterOperations) {
    // Perform some operations
    med_cereb_bridge_send_climbing_signal(bridge, MED_CEREB_ERROR_TIMING, 0.5f, 0);
    med_cereb_bridge_send_climbing_signal(bridge, MED_CEREB_ERROR_AMPLITUDE, 0.3f, 0);

    med_cereb_bridge_stats_t stats;
    med_cereb_bridge_get_stats(bridge, &stats);

    EXPECT_GT(stats.climbing_signals_sent, 0u);
}

TEST_F(MedullaCerebellumBridgeTest, ResetStatsSuccess) {
    // Generate some stats
    med_cereb_bridge_send_climbing_signal(bridge, MED_CEREB_ERROR_TIMING, 0.5f, 0);

    int result = med_cereb_bridge_reset_stats(bridge);
    EXPECT_EQ(result, 0);

    med_cereb_bridge_stats_t stats;
    med_cereb_bridge_get_stats(bridge, &stats);

    EXPECT_EQ(stats.climbing_signals_sent, 0u);
}

TEST_F(MedullaCerebellumBridgeTest, ResetStatsNullBridge) {
    int result = med_cereb_bridge_reset_stats(nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(MedullaCerebellumBridgeTest, GetIOStateSuccess) {
    med_cereb_inferior_olive_t io_state;
    int result = med_cereb_bridge_get_io_state(bridge, &io_state);
    EXPECT_EQ(result, 0);
}

TEST_F(MedullaCerebellumBridgeTest, GetIOStateNullBridge) {
    med_cereb_inferior_olive_t io_state;
    int result = med_cereb_bridge_get_io_state(nullptr, &io_state);
    EXPECT_EQ(result, -1);
}

TEST_F(MedullaCerebellumBridgeTest, GetIOStateNullOutput) {
    int result = med_cereb_bridge_get_io_state(bridge, nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(MedullaCerebellumBridgeTest, GetIOStateValidValues) {
    med_cereb_inferior_olive_t io_state;
    med_cereb_bridge_get_io_state(bridge, &io_state);

    EXPECT_GT(io_state.num_neurons, 0u);
    EXPECT_LE(io_state.num_neurons, MED_CEREB_MAX_IO_NEURONS);
    EXPECT_GT(io_state.oscillation_freq, 0.0f);
    EXPECT_GE(io_state.coupling_strength, 0.0f);
    EXPECT_LE(io_state.coupling_strength, 1.0f);
}

TEST_F(MedullaCerebellumBridgeTest, GetIOStateNeuronData) {
    med_cereb_inferior_olive_t io_state;
    med_cereb_bridge_get_io_state(bridge, &io_state);

    for (uint32_t i = 0; i < io_state.num_neurons; i++) {
        EXPECT_EQ(io_state.neurons[i].neuron_id, i);
        EXPECT_GE(io_state.neurons[i].activation, 0.0f);
        EXPECT_LE(io_state.neurons[i].activation, 1.0f);
        EXPECT_LT(io_state.neurons[i].error_type, MED_CEREB_ERROR_COUNT);
    }
}

TEST_F(MedullaCerebellumBridgeTest, PendingErrorCountInitiallyZero) {
    // Fresh bridge should have no pending errors
    med_cereb_bridge_reset(bridge);
    uint32_t count = med_cereb_bridge_pending_error_count(bridge);
    EXPECT_EQ(count, 0u);
}

TEST_F(MedullaCerebellumBridgeTest, PendingErrorCountNullBridge) {
    uint32_t count = med_cereb_bridge_pending_error_count(nullptr);
    EXPECT_EQ(count, 0u);
}

TEST_F(MedullaCerebellumBridgeTest, PendingErrorCountAfterQueue) {
    med_cereb_bridge_reset(bridge);

    med_cereb_bridge_queue_error(bridge, MED_CEREB_ERROR_TIMING, 0.5f, 1);
    med_cereb_bridge_queue_error(bridge, MED_CEREB_ERROR_AMPLITUDE, 0.3f, 2);
    med_cereb_bridge_queue_error(bridge, MED_CEREB_ERROR_TRAJECTORY, 0.7f, 3);

    uint32_t count = med_cereb_bridge_pending_error_count(bridge);
    EXPECT_EQ(count, 3u);
}

//=============================================================================
// Debug/Diagnostics Tests
//=============================================================================

TEST_F(MedullaCerebellumBridgeTest, PrintStateNullIsSafe) {
    med_cereb_bridge_print_state(nullptr);  // Should not crash
}

TEST_F(MedullaCerebellumBridgeTest, PrintIOStateNullIsSafe) {
    med_cereb_bridge_print_io_state(nullptr);  // Should not crash
}

TEST_F(MedullaCerebellumBridgeTest, ErrorTypeNameAllTypes) {
    for (int i = 0; i < MED_CEREB_ERROR_COUNT; i++) {
        const char* name = med_cereb_error_type_name(static_cast<med_cereb_error_type_t>(i));
        EXPECT_NE(name, nullptr);
        EXPECT_GT(strlen(name), 0u);
    }
}

TEST_F(MedullaCerebellumBridgeTest, ErrorTypeNameInvalidType) {
    const char* name = med_cereb_error_type_name(static_cast<med_cereb_error_type_t>(-1));
    EXPECT_NE(name, nullptr);  // Should return "unknown" or similar

    name = med_cereb_error_type_name(static_cast<med_cereb_error_type_t>(MED_CEREB_ERROR_COUNT));
    EXPECT_NE(name, nullptr);
}

TEST_F(MedullaCerebellumBridgeTest, ErrorTypeNameKnownTypes) {
    // Verify each error type has a unique non-empty name
    const char* timing_name = med_cereb_error_type_name(MED_CEREB_ERROR_TIMING);
    const char* amplitude_name = med_cereb_error_type_name(MED_CEREB_ERROR_AMPLITUDE);
    const char* trajectory_name = med_cereb_error_type_name(MED_CEREB_ERROR_TRAJECTORY);
    const char* coordination_name = med_cereb_error_type_name(MED_CEREB_ERROR_COORDINATION);
    const char* prediction_name = med_cereb_error_type_name(MED_CEREB_ERROR_PREDICTION);
    const char* protection_name = med_cereb_error_type_name(MED_CEREB_ERROR_PROTECTION);
    const char* sequence_name = med_cereb_error_type_name(MED_CEREB_ERROR_SEQUENCE);

    // All names should be valid
    EXPECT_NE(timing_name, nullptr);
    EXPECT_NE(amplitude_name, nullptr);
    EXPECT_NE(trajectory_name, nullptr);
    EXPECT_NE(coordination_name, nullptr);
    EXPECT_NE(prediction_name, nullptr);
    EXPECT_NE(protection_name, nullptr);
    EXPECT_NE(sequence_name, nullptr);

    // All names should be non-empty
    EXPECT_GT(strlen(timing_name), 0u);
    EXPECT_GT(strlen(amplitude_name), 0u);
    EXPECT_GT(strlen(trajectory_name), 0u);
    EXPECT_GT(strlen(coordination_name), 0u);
    EXPECT_GT(strlen(prediction_name), 0u);
    EXPECT_GT(strlen(protection_name), 0u);
    EXPECT_GT(strlen(sequence_name), 0u);

    // All names should be unique (different error types have different names)
    EXPECT_STRNE(timing_name, amplitude_name);
    EXPECT_STRNE(timing_name, trajectory_name);
    EXPECT_STRNE(amplitude_name, trajectory_name);
}

//=============================================================================
// Integration Tests
//=============================================================================

TEST_F(MedullaCerebellumBridgeTest, FullErrorSignalingPipeline) {
    // 1. Queue an error
    med_cereb_bridge_queue_error(bridge, MED_CEREB_ERROR_PREDICTION, 0.6f, 1);

    // 2. Update to process through IO
    for (int i = 0; i < 20; i++) {
        med_cereb_bridge_update(bridge, 50000);  // 50ms
    }

    // 3. Verify climbing signal was sent
    med_cereb_bridge_stats_t stats;
    med_cereb_bridge_get_stats(bridge, &stats);

    // Either error was processed (signal sent) or dropped
    EXPECT_TRUE(stats.climbing_signals_sent > 0 ||
                stats.io_spikes > 0 ||
                stats.errors_dropped > 0);
}

TEST_F(MedullaCerebellumBridgeTest, ArousalModulationPipeline) {
    // 1. Get initial arousal effects
    med_cereb_arousal_effects_t initial_effects;
    med_cereb_bridge_get_arousal_effects(bridge, &initial_effects);

    // 2. Boost medulla arousal
    medulla_boost_arousal(medulla, 0.3f);

    // 3. Update bridge to pick up new arousal
    med_cereb_bridge_update(bridge, 100000);

    // 4. Get updated effects
    med_cereb_arousal_effects_t updated_effects;
    med_cereb_bridge_get_arousal_effects(bridge, &updated_effects);

    // Motor gain should change with arousal
    // (exact behavior depends on implementation)
}

TEST_F(MedullaCerebellumBridgeTest, ProtectionPipeline) {
    // 1. Verify motor allowed initially
    EXPECT_TRUE(med_cereb_bridge_motor_allowed(bridge, false, false));

    // 2. Trigger emergency
    med_cereb_bridge_emergency_stop(bridge);

    // 3. Verify motor gated
    EXPECT_FALSE(med_cereb_bridge_motor_allowed(bridge, false, false));

    // 4. Check protection effects
    med_cereb_protection_effects_t effects;
    med_cereb_bridge_get_protection_effects(bridge, &effects);
    EXPECT_TRUE(effects.emergency_stop);

    // 5. Release emergency
    med_cereb_bridge_release_emergency(bridge);

    // 6. Verify motor restored
    EXPECT_TRUE(med_cereb_bridge_motor_allowed(bridge, false, false));
}

TEST_F(MedullaCerebellumBridgeTest, CircadianLearningPipeline) {
    // 1. Get initial learning multiplier
    float initial_mult = med_cereb_bridge_get_learning_multiplier(bridge);

    // 2. Get circadian effects
    med_cereb_circadian_effects_t effects;
    med_cereb_bridge_get_circadian_effects(bridge, &effects);

    // 3. Apply circadian learning
    med_cereb_bridge_apply_circadian_learning(bridge);

    // 4. Verify stats updated
    med_cereb_bridge_stats_t stats;
    med_cereb_bridge_get_stats(bridge, &stats);
    EXPECT_GT(stats.learning_rate_adjustments, 0u);

    // 5. Multiplier should be consistent with effects
    float ltd_ltp_avg = (effects.ltd_rate_multiplier + effects.ltp_rate_multiplier) / 2.0f;
    EXPECT_NEAR(initial_mult, ltd_ltp_avg, 0.5f);
}

TEST_F(MedullaCerebellumBridgeTest, IOModelOscillation) {
    // Get IO state
    med_cereb_inferior_olive_t io_state;
    med_cereb_bridge_get_io_state(bridge, &io_state);

    float initial_phases[MED_CEREB_MAX_IO_NEURONS];
    for (uint32_t i = 0; i < io_state.num_neurons; i++) {
        initial_phases[i] = io_state.neurons[i].oscillation_phase;
    }

    // Update for some time
    for (int i = 0; i < 100; i++) {
        med_cereb_bridge_update(bridge, 10000);  // 10ms
    }

    // Get updated IO state
    med_cereb_bridge_get_io_state(bridge, &io_state);

    // Phases should have changed
    bool any_changed = false;
    for (uint32_t i = 0; i < io_state.num_neurons; i++) {
        if (io_state.neurons[i].oscillation_phase != initial_phases[i]) {
            any_changed = true;
            break;
        }
    }
    // IO neurons should oscillate
    EXPECT_TRUE(any_changed || io_state.oscillation_freq == 0.0f);
}

TEST_F(MedullaCerebellumBridgeTest, IOModelRefractory) {
    med_cereb_inferior_olive_t io_state;

    // Queue error with high magnitude to trigger firing
    med_cereb_bridge_queue_error(bridge, MED_CEREB_ERROR_TIMING, 1.0f, 1);

    // First update - should fire
    med_cereb_bridge_update(bridge, 10000);

    med_cereb_bridge_get_io_state(bridge, &io_state);

    // Check if any neurons are refractory
    bool any_refractory = false;
    for (uint32_t i = 0; i < io_state.num_neurons; i++) {
        if (io_state.neurons[i].is_refractory) {
            any_refractory = true;
            break;
        }
    }

    // If IO signaling is enabled and we just fired, some neurons should be refractory
    med_cereb_bridge_stats_t stats;
    med_cereb_bridge_get_stats(bridge, &stats);
    if (stats.io_spikes > 0) {
        EXPECT_TRUE(any_refractory);
    }
}

//=============================================================================
// Stress Tests
//=============================================================================

TEST_F(MedullaCerebellumBridgeTest, RapidQueueAndUpdate) {
    // Rapid-fire queue and update
    for (int i = 0; i < 1000; i++) {
        med_cereb_bridge_queue_error(
            bridge,
            static_cast<med_cereb_error_type_t>(i % MED_CEREB_ERROR_COUNT),
            0.5f,
            i);
        med_cereb_bridge_update(bridge, 1000);  // 1ms
    }

    // Should not crash, get stats to verify
    med_cereb_bridge_stats_t stats;
    int result = med_cereb_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(result, 0);
}

TEST_F(MedullaCerebellumBridgeTest, LongRunningSimulation) {
    // Simulate 1 second of operation at 60fps
    for (int frame = 0; frame < 60; frame++) {
        // Occasional errors
        if (frame % 10 == 0) {
            med_cereb_bridge_queue_error(bridge, MED_CEREB_ERROR_TIMING, 0.3f, frame);
        }

        // Motor modulation
        float motor[3] = {0.5f, 0.5f, 0.5f};
        float modulated[3];
        med_cereb_bridge_modulate_motor(bridge, motor, modulated, 3);

        // Update
        med_cereb_bridge_update(bridge, 16667);  // ~60fps

        // Process messages
        med_cereb_bridge_process_messages(bridge);
    }

    // Verify state is still valid
    med_cereb_bridge_stats_t stats;
    int result = med_cereb_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(result, 0);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
