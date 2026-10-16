/**
 * @file test_hypothalamus_training_bridge.cpp
 * @brief Unit tests for nimcp_hypothalamus_training_bridge.c
 *
 * WHAT: Comprehensive unit tests for the Hypothalamus-Training Bridge
 * WHY:  Ensure correct drive-modulated learning, homeostatic regulation,
 *       training event processing, and modulation output work correctly
 * HOW:  Use Google Test framework to test lifecycle, configuration,
 *       connection management, training events, modulation, state, and statistics
 *
 * COVERAGE TARGET: 100%
 *
 * @version 1.0.0
 * @date 2026-01-10
 */

#include <gtest/gtest.h>
#include <string.h>
#include <stdlib.h>
#include <cmath>
#include <atomic>

extern "C" {
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_training_bridge.h"
}

// ============================================================================
// TEST FIXTURES
// ============================================================================

/**
 * @brief Main test fixture with pre-created bridge
 */
class HypothalamusTrainingBridgeTest : public ::testing::Test {
protected:
    hypo_training_bridge_t* bridge;
    hypo_training_bridge_config_t config;

    void SetUp() override {
        ASSERT_EQ(0, hypo_training_bridge_default_config(&config));
        bridge = hypo_training_bridge_create(&config, nullptr, nullptr);
        ASSERT_NE(nullptr, bridge) << "Failed to create Hypothalamus training bridge";
    }

    void TearDown() override {
        hypo_training_bridge_destroy(bridge);
        bridge = nullptr;
    }
};

/**
 * @brief Lifecycle test fixture without pre-created bridge
 */
class HypothalamusTrainingBridgeLifecycleTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// ============================================================================
// DEFAULT CONFIG TESTS
// ============================================================================

TEST_F(HypothalamusTrainingBridgeLifecycleTest, DefaultConfigHasReasonableValues) {
    hypo_training_bridge_config_t config;
    EXPECT_EQ(0, hypo_training_bridge_default_config(&config));

    // Connection configuration
    EXPECT_TRUE(config.auto_connect_orchestrator);
    EXPECT_TRUE(config.auto_connect_training_hub);
    EXPECT_FALSE(config.enable_bio_async);  // Disabled by default

    // Homeostatic configuration
    EXPECT_FLOAT_EQ(HYPO_TRAINING_DEFAULT_LOSS_SETPOINT, config.homeostatic_config.loss_setpoint);
    EXPECT_FLOAT_EQ(HYPO_TRAINING_DEFAULT_LOSS_TOLERANCE, config.homeostatic_config.loss_tolerance);
    EXPECT_GT(config.homeostatic_config.deviation_response_gain, 0.0f);
    EXPECT_TRUE(config.homeostatic_config.adaptive_setpoint);

    // Drive configuration - curiosity
    EXPECT_FLOAT_EQ(HYPO_TRAINING_DEFAULT_CURIOSITY_LR_MULT, config.drive_config.curiosity_lr_multiplier);
    EXPECT_GT(config.drive_config.curiosity_exploration_weight, 0.0f);

    // Drive configuration - safety
    EXPECT_FLOAT_EQ(HYPO_TRAINING_DEFAULT_SAFETY_LR_MULT, config.drive_config.safety_lr_reduction);
    EXPECT_GT(config.drive_config.safety_gradient_clip_mult, 0.0f);
    EXPECT_GT(config.drive_config.safety_divergence_threshold, 0.0f);

    // Drive configuration - competence
    EXPECT_GT(config.drive_config.competence_difficulty_weight, 0.0f);
    EXPECT_GT(config.drive_config.competence_mastery_threshold, 0.0f);

    // Drive configuration - fatigue
    EXPECT_GT(config.drive_config.fatigue_lr_decay, 0.0f);
    EXPECT_FLOAT_EQ(HYPO_TRAINING_DEFAULT_CONSOLIDATION_THRESHOLD,
                    config.drive_config.fatigue_consolidation_threshold);
    EXPECT_GT(config.drive_config.fatigue_max_epochs_before_rest, 0u);

    // Operational configuration
    EXPECT_TRUE(config.enable_consolidation);
    EXPECT_TRUE(config.enable_stress_response);
    EXPECT_TRUE(config.enable_reward_signals);
    EXPECT_FALSE(config.enable_logging);  // Disabled by default
    EXPECT_TRUE(config.enable_metrics);
}

TEST_F(HypothalamusTrainingBridgeLifecycleTest, DefaultConfigNullReturnsError) {
    EXPECT_NE(0, hypo_training_bridge_default_config(nullptr));
}

// ============================================================================
// LIFECYCLE TESTS
// ============================================================================

TEST_F(HypothalamusTrainingBridgeLifecycleTest, CreateWithDefaultConfig) {
    hypo_training_bridge_t* bridge = hypo_training_bridge_create(nullptr, nullptr, nullptr);
    ASSERT_NE(nullptr, bridge);

    hypo_training_state_t state;
    EXPECT_EQ(0, hypo_training_bridge_get_training_state(bridge, &state));
    EXPECT_EQ(HYPO_TRAIN_STATE_HEALTHY, state);

    hypo_training_bridge_destroy(bridge);
}

TEST_F(HypothalamusTrainingBridgeLifecycleTest, CreateWithCustomConfig) {
    hypo_training_bridge_config_t config;
    hypo_training_bridge_default_config(&config);

    // Customize config
    config.homeostatic_config.loss_setpoint = 0.3f;
    config.homeostatic_config.loss_tolerance = 0.05f;
    config.drive_config.curiosity_lr_multiplier = 2.0f;
    config.drive_config.safety_lr_reduction = 0.3f;
    config.enable_consolidation = false;

    hypo_training_bridge_t* bridge = hypo_training_bridge_create(&config, nullptr, nullptr);
    ASSERT_NE(nullptr, bridge);

    hypo_training_bridge_destroy(bridge);
}

TEST_F(HypothalamusTrainingBridgeLifecycleTest, DestroyNullDoesNotCrash) {
    hypo_training_bridge_destroy(nullptr);
    // Should not crash
}

TEST_F(HypothalamusTrainingBridgeTest, ResetClearsState) {
    // Process some training events to change state
    EXPECT_EQ(0, hypo_training_bridge_process_loss(bridge, 1, 1, 0.8f));
    EXPECT_EQ(0, hypo_training_bridge_process_epoch(bridge, 1, 0.8f));

    // Set a drive
    EXPECT_EQ(0, hypo_training_bridge_set_drive(bridge, 3, 0.5f));  // Fatigue drive

    // Reset
    EXPECT_EQ(0, hypo_training_bridge_reset(bridge));

    // Training state should be healthy
    hypo_training_state_t state;
    EXPECT_EQ(0, hypo_training_bridge_get_training_state(bridge, &state));
    EXPECT_EQ(HYPO_TRAIN_STATE_HEALTHY, state);

    // Drive state should be reset
    hypo_training_drive_state_t drive_state;
    EXPECT_EQ(0, hypo_training_bridge_get_drive_state(bridge, &drive_state));
    EXPECT_FLOAT_EQ(0.0f, drive_state.fatigue_level);
}

TEST_F(HypothalamusTrainingBridgeLifecycleTest, ResetNullReturnsError) {
    EXPECT_NE(0, hypo_training_bridge_reset(nullptr));
}

// ============================================================================
// CONNECTION MANAGEMENT TESTS
// ============================================================================

TEST_F(HypothalamusTrainingBridgeTest, IsConnectedInitiallyDisconnected) {
    bool orch_connected = true;
    bool hub_connected = true;

    EXPECT_EQ(0, hypo_training_bridge_is_connected(bridge, &orch_connected, &hub_connected));
    EXPECT_FALSE(orch_connected);
    EXPECT_FALSE(hub_connected);
}

TEST_F(HypothalamusTrainingBridgeTest, ConnectOrchestratorNullOrchestratorSucceeds) {
    // NULL orchestrator is allowed (marks as disconnected)
    EXPECT_EQ(0, hypo_training_bridge_connect_orchestrator(bridge, nullptr));
}

TEST_F(HypothalamusTrainingBridgeTest, ConnectOrchestratorNullBridgeFails) {
    int dummy_orch = 42;
    EXPECT_NE(0, hypo_training_bridge_connect_orchestrator(nullptr,
              (hypo_orchestrator_t)&dummy_orch));
}

TEST_F(HypothalamusTrainingBridgeTest, ConnectTrainingHubNullHubSucceeds) {
    // NULL hub is allowed (marks as disconnected)
    EXPECT_EQ(0, hypo_training_bridge_connect_training_hub(bridge, nullptr));
}

TEST_F(HypothalamusTrainingBridgeTest, ConnectTrainingHubNullBridgeFails) {
    int dummy_hub = 42;
    EXPECT_NE(0, hypo_training_bridge_connect_training_hub(nullptr,
              (training_integration_hub_t)&dummy_hub));
}

TEST_F(HypothalamusTrainingBridgeTest, DisconnectSuccess) {
    EXPECT_EQ(0, hypo_training_bridge_disconnect(bridge));
}

TEST_F(HypothalamusTrainingBridgeTest, DisconnectNullBridgeFails) {
    EXPECT_NE(0, hypo_training_bridge_disconnect(nullptr));
}

TEST_F(HypothalamusTrainingBridgeTest, IsConnectedNullBridgeFails) {
    bool orch_connected, hub_connected;
    EXPECT_NE(0, hypo_training_bridge_is_connected(nullptr, &orch_connected, &hub_connected));
}

TEST_F(HypothalamusTrainingBridgeTest, IsConnectedNullOutputsSucceeds) {
    // NULL outputs are allowed (skipped)
    bool orch_connected, hub_connected;
    EXPECT_EQ(0, hypo_training_bridge_is_connected(bridge, nullptr, &hub_connected));
    EXPECT_EQ(0, hypo_training_bridge_is_connected(bridge, &orch_connected, nullptr));
}

// ============================================================================
// TRAINING EVENT PROCESSING TESTS - LOSS
// ============================================================================

TEST_F(HypothalamusTrainingBridgeTest, ProcessLossSuccess) {
    EXPECT_EQ(0, hypo_training_bridge_process_loss(bridge, 1, 0, 0.5f));

    hypo_training_homeostatic_state_t state;
    EXPECT_EQ(0, hypo_training_bridge_get_homeostatic_state(bridge, &state));
    EXPECT_FLOAT_EQ(0.5f, state.current_loss);
}

TEST_F(HypothalamusTrainingBridgeTest, ProcessLossNullBridgeFails) {
    EXPECT_NE(0, hypo_training_bridge_process_loss(nullptr, 1, 0, 0.5f));
}

TEST_F(HypothalamusTrainingBridgeTest, ProcessLossUpdatesDeviation) {
    // Set a loss above setpoint
    EXPECT_EQ(0, hypo_training_bridge_process_loss(bridge, 1, 0, 0.8f));

    hypo_training_homeostatic_state_t state;
    EXPECT_EQ(0, hypo_training_bridge_get_homeostatic_state(bridge, &state));

    // Deviation should be positive (loss > setpoint)
    EXPECT_GT(state.deviation, 0.0f);
}

TEST_F(HypothalamusTrainingBridgeTest, ProcessLossBelowSetpointNegativeDeviation) {
    // Set a loss below setpoint
    EXPECT_EQ(0, hypo_training_bridge_process_loss(bridge, 1, 0, 0.2f));

    hypo_training_homeostatic_state_t state;
    EXPECT_EQ(0, hypo_training_bridge_get_homeostatic_state(bridge, &state));

    // Deviation should be negative (loss < setpoint)
    EXPECT_LT(state.deviation, 0.0f);
}

TEST_F(HypothalamusTrainingBridgeTest, ProcessLossDetectsImproving) {
    // Process decreasing losses
    EXPECT_EQ(0, hypo_training_bridge_process_loss(bridge, 1, 0, 0.8f));
    EXPECT_EQ(0, hypo_training_bridge_process_loss(bridge, 1, 1, 0.7f));
    EXPECT_EQ(0, hypo_training_bridge_process_loss(bridge, 1, 2, 0.6f));

    hypo_training_state_t state;
    EXPECT_EQ(0, hypo_training_bridge_get_training_state(bridge, &state));

    // State should indicate improvement
    EXPECT_TRUE(state == HYPO_TRAIN_STATE_IMPROVING || state == HYPO_TRAIN_STATE_HEALTHY);
}

TEST_F(HypothalamusTrainingBridgeTest, ProcessLossDetectsDiverging) {
    // Process increasing losses
    EXPECT_EQ(0, hypo_training_bridge_process_loss(bridge, 1, 0, 0.4f));
    EXPECT_EQ(0, hypo_training_bridge_process_loss(bridge, 1, 1, 0.6f));
    EXPECT_EQ(0, hypo_training_bridge_process_loss(bridge, 1, 2, 0.8f));
    EXPECT_EQ(0, hypo_training_bridge_process_loss(bridge, 1, 3, 1.0f));
    EXPECT_EQ(0, hypo_training_bridge_process_loss(bridge, 1, 4, 1.2f));

    hypo_training_state_t state;
    EXPECT_EQ(0, hypo_training_bridge_get_training_state(bridge, &state));

    // State should indicate divergence or at least not healthy
    EXPECT_TRUE(state == HYPO_TRAIN_STATE_DIVERGING ||
                state == HYPO_TRAIN_STATE_UNSTABLE ||
                state == HYPO_TRAIN_STATE_CRITICAL);
}

TEST_F(HypothalamusTrainingBridgeTest, ProcessLossHandlesNaN) {
    float nan_val = std::nan("");
    EXPECT_EQ(0, hypo_training_bridge_process_loss(bridge, 1, 0, nan_val));

    hypo_training_state_t state;
    EXPECT_EQ(0, hypo_training_bridge_get_training_state(bridge, &state));
    EXPECT_EQ(HYPO_TRAIN_STATE_CRITICAL, state);
}

TEST_F(HypothalamusTrainingBridgeTest, ProcessLossHandlesInf) {
    float inf_val = std::numeric_limits<float>::infinity();
    EXPECT_EQ(0, hypo_training_bridge_process_loss(bridge, 1, 0, inf_val));

    hypo_training_state_t state;
    EXPECT_EQ(0, hypo_training_bridge_get_training_state(bridge, &state));
    EXPECT_EQ(HYPO_TRAIN_STATE_CRITICAL, state);
}

// ============================================================================
// TRAINING EVENT PROCESSING TESTS - GRADIENT
// ============================================================================

TEST_F(HypothalamusTrainingBridgeTest, ProcessGradientSuccess) {
    EXPECT_EQ(0, hypo_training_bridge_process_gradient(bridge, 1.0f, false));
}

TEST_F(HypothalamusTrainingBridgeTest, ProcessGradientNullBridgeFails) {
    EXPECT_NE(0, hypo_training_bridge_process_gradient(nullptr, 1.0f, false));
}

TEST_F(HypothalamusTrainingBridgeTest, ProcessGradientWithClippingActivatesSafety) {
    // Process clipped gradients
    EXPECT_EQ(0, hypo_training_bridge_process_gradient(bridge, 100.0f, true));

    hypo_training_drive_state_t drive_state;
    EXPECT_EQ(0, hypo_training_bridge_get_drive_state(bridge, &drive_state));

    // Safety drive should be elevated
    EXPECT_GT(drive_state.safety_activation, 0.0f);
}

TEST_F(HypothalamusTrainingBridgeTest, ProcessGradientHighNormActivatesSafety) {
    // Process high gradient norm
    EXPECT_EQ(0, hypo_training_bridge_process_gradient(bridge, 1000.0f, false));

    hypo_training_drive_state_t drive_state;
    EXPECT_EQ(0, hypo_training_bridge_get_drive_state(bridge, &drive_state));

    // Safety drive should be elevated
    EXPECT_GT(drive_state.safety_activation, 0.0f);
}

TEST_F(HypothalamusTrainingBridgeTest, ProcessGradientNormalDoesNotActivateSafety) {
    // Process normal gradient norm
    EXPECT_EQ(0, hypo_training_bridge_process_gradient(bridge, 0.5f, false));

    hypo_training_drive_state_t drive_state;
    EXPECT_EQ(0, hypo_training_bridge_get_drive_state(bridge, &drive_state));

    // Safety drive should be low
    EXPECT_LT(drive_state.safety_activation, 0.5f);
}

// ============================================================================
// TRAINING EVENT PROCESSING TESTS - EPOCH
// ============================================================================

TEST_F(HypothalamusTrainingBridgeTest, ProcessEpochSuccess) {
    EXPECT_EQ(0, hypo_training_bridge_process_epoch(bridge, 1, 0.5f));
}

TEST_F(HypothalamusTrainingBridgeTest, ProcessEpochNullBridgeFails) {
    EXPECT_NE(0, hypo_training_bridge_process_epoch(nullptr, 1, 0.5f));
}

TEST_F(HypothalamusTrainingBridgeTest, ProcessEpochAccumulatesFatigue) {
    // Process multiple epochs
    for (int i = 0; i < 10; i++) {
        EXPECT_EQ(0, hypo_training_bridge_process_epoch(bridge, i, 0.5f));
    }

    hypo_training_drive_state_t drive_state;
    EXPECT_EQ(0, hypo_training_bridge_get_drive_state(bridge, &drive_state));

    // Fatigue should be elevated
    EXPECT_GT(drive_state.fatigue_level, 0.0f);
}

TEST_F(HypothalamusTrainingBridgeTest, ProcessEpochUpdatesCompetence) {
    // Process epochs with improving loss
    for (int i = 0; i < 5; i++) {
        float loss = 0.8f - (i * 0.1f);
        EXPECT_EQ(0, hypo_training_bridge_process_epoch(bridge, i, loss));
    }

    hypo_training_drive_state_t drive_state;
    EXPECT_EQ(0, hypo_training_bridge_get_drive_state(bridge, &drive_state));

    // Competence should be elevated due to improvement
    EXPECT_GT(drive_state.competence_activation, 0.0f);
}

TEST_F(HypothalamusTrainingBridgeTest, ProcessEpochTracksBestLoss) {
    // Best loss is tracked via process_loss, not process_epoch
    EXPECT_EQ(0, hypo_training_bridge_process_loss(bridge, 0, 0, 0.5f));
    EXPECT_EQ(0, hypo_training_bridge_process_loss(bridge, 1, 0, 0.3f));
    EXPECT_EQ(0, hypo_training_bridge_process_loss(bridge, 2, 0, 0.4f));

    hypo_training_homeostatic_state_t state;
    EXPECT_EQ(0, hypo_training_bridge_get_homeostatic_state(bridge, &state));

    // Best loss should be 0.3
    EXPECT_FLOAT_EQ(0.3f, state.best_loss_seen);
}

// ============================================================================
// TRAINING EVENT PROCESSING TESTS - LEARNING RATE CHANGE
// ============================================================================

TEST_F(HypothalamusTrainingBridgeTest, ProcessLRChangeSuccess) {
    EXPECT_EQ(0, hypo_training_bridge_process_lr_change(bridge, 0.01f, 0.001f));
}

TEST_F(HypothalamusTrainingBridgeTest, ProcessLRChangeNullBridgeFails) {
    EXPECT_NE(0, hypo_training_bridge_process_lr_change(nullptr, 0.01f, 0.001f));
}

TEST_F(HypothalamusTrainingBridgeTest, ProcessLRIncreaseBoostsCuriosity) {
    // LR increase indicates exploration
    EXPECT_EQ(0, hypo_training_bridge_process_lr_change(bridge, 0.001f, 0.01f));

    hypo_training_drive_state_t drive_state;
    EXPECT_EQ(0, hypo_training_bridge_get_drive_state(bridge, &drive_state));

    // Curiosity should be elevated
    EXPECT_GT(drive_state.curiosity_activation, 0.0f);
    // Exploration tendency should be positive
    EXPECT_GT(drive_state.exploration_tendency, 0.0f);
}

TEST_F(HypothalamusTrainingBridgeTest, ProcessLRDecreaseBoostsSafety) {
    // LR decrease indicates exploitation/caution
    EXPECT_EQ(0, hypo_training_bridge_process_lr_change(bridge, 0.01f, 0.001f));

    hypo_training_drive_state_t drive_state;
    EXPECT_EQ(0, hypo_training_bridge_get_drive_state(bridge, &drive_state));

    // Safety should be elevated or exploration tendency negative
    EXPECT_TRUE(drive_state.safety_activation > 0.0f ||
                drive_state.exploration_tendency < 0.5f);
}

// ============================================================================
// MODULATION OUTPUT TESTS
// ============================================================================

TEST_F(HypothalamusTrainingBridgeTest, ComputeModulationSuccess) {
    hypo_training_modulation_t modulation;
    EXPECT_EQ(0, hypo_training_bridge_compute_modulation(bridge, &modulation));

    // Default modulation should have reasonable values
    EXPECT_GE(modulation.lr_multiplier, HYPO_TRAINING_MIN_PRECISION);
    EXPECT_LE(modulation.lr_multiplier, HYPO_TRAINING_MAX_PRECISION);
    EXPECT_GE(modulation.batch_size_multiplier, 0.5f);
    EXPECT_LE(modulation.batch_size_multiplier, 2.0f);
    EXPECT_GE(modulation.gradient_clip_multiplier, 0.5f);
    EXPECT_LE(modulation.gradient_clip_multiplier, 2.0f);
    EXPECT_GE(modulation.difficulty_adjustment, -1.0f);
    EXPECT_LE(modulation.difficulty_adjustment, 1.0f);
}

TEST_F(HypothalamusTrainingBridgeTest, ComputeModulationNullBridgeFails) {
    hypo_training_modulation_t modulation;
    EXPECT_NE(0, hypo_training_bridge_compute_modulation(nullptr, &modulation));
}

TEST_F(HypothalamusTrainingBridgeTest, ComputeModulationNullOutputFails) {
    EXPECT_NE(0, hypo_training_bridge_compute_modulation(bridge, nullptr));
}

TEST_F(HypothalamusTrainingBridgeTest, ComputeModulationReflectsDriveState) {
    // Set high curiosity drive
    EXPECT_EQ(0, hypo_training_bridge_set_drive(bridge, 0, 0.9f));

    hypo_training_modulation_t modulation;
    EXPECT_EQ(0, hypo_training_bridge_compute_modulation(bridge, &modulation));

    // LR multiplier should be elevated due to curiosity
    EXPECT_GT(modulation.lr_multiplier, 1.0f);
}

TEST_F(HypothalamusTrainingBridgeTest, ComputeModulationReflectsSafetyDrive) {
    // Set high safety drive
    EXPECT_EQ(0, hypo_training_bridge_set_drive(bridge, 1, 0.9f));

    hypo_training_modulation_t modulation;
    EXPECT_EQ(0, hypo_training_bridge_compute_modulation(bridge, &modulation));

    // LR multiplier should be reduced due to safety
    EXPECT_LT(modulation.lr_multiplier, 1.2f);
}

TEST_F(HypothalamusTrainingBridgeTest, GetLRMultiplierSuccess) {
    float lr_mult;
    EXPECT_EQ(0, hypo_training_bridge_get_lr_multiplier(bridge, &lr_mult));

    // Should be in valid range
    EXPECT_GE(lr_mult, HYPO_TRAINING_MIN_PRECISION);
    EXPECT_LE(lr_mult, HYPO_TRAINING_MAX_PRECISION);
}

TEST_F(HypothalamusTrainingBridgeTest, GetLRMultiplierNullBridgeFails) {
    float lr_mult;
    EXPECT_NE(0, hypo_training_bridge_get_lr_multiplier(nullptr, &lr_mult));
}

TEST_F(HypothalamusTrainingBridgeTest, GetLRMultiplierNullOutputFails) {
    EXPECT_NE(0, hypo_training_bridge_get_lr_multiplier(bridge, nullptr));
}

TEST_F(HypothalamusTrainingBridgeTest, GetDifficultyAdjustmentSuccess) {
    float difficulty;
    EXPECT_EQ(0, hypo_training_bridge_get_difficulty_adjustment(bridge, &difficulty));

    // Should be in valid range
    EXPECT_GE(difficulty, -1.0f);
    EXPECT_LE(difficulty, 1.0f);
}

TEST_F(HypothalamusTrainingBridgeTest, GetDifficultyAdjustmentNullBridgeFails) {
    float difficulty;
    EXPECT_NE(0, hypo_training_bridge_get_difficulty_adjustment(nullptr, &difficulty));
}

TEST_F(HypothalamusTrainingBridgeTest, GetDifficultyAdjustmentNullOutputFails) {
    EXPECT_NE(0, hypo_training_bridge_get_difficulty_adjustment(bridge, nullptr));
}

TEST_F(HypothalamusTrainingBridgeTest, CheckConsolidationSuccess) {
    hypo_consolidation_type_t type;
    EXPECT_EQ(0, hypo_training_bridge_check_consolidation(bridge, &type));

    // Initially should be no consolidation needed
    EXPECT_EQ(HYPO_CONSOL_NONE, type);
}

TEST_F(HypothalamusTrainingBridgeTest, CheckConsolidationNullBridgeFails) {
    hypo_consolidation_type_t type;
    EXPECT_NE(0, hypo_training_bridge_check_consolidation(nullptr, &type));
}

TEST_F(HypothalamusTrainingBridgeTest, CheckConsolidationNullOutputFails) {
    EXPECT_NE(0, hypo_training_bridge_check_consolidation(bridge, nullptr));
}

TEST_F(HypothalamusTrainingBridgeTest, CheckConsolidationAfterHighFatigue) {
    // Set high fatigue
    EXPECT_EQ(0, hypo_training_bridge_set_drive(bridge, 3, 0.9f));

    hypo_consolidation_type_t type;
    EXPECT_EQ(0, hypo_training_bridge_check_consolidation(bridge, &type));

    // Should recommend consolidation
    EXPECT_NE(HYPO_CONSOL_NONE, type);
}

// ============================================================================
// HOMEOSTATIC STATE TESTS
// ============================================================================

TEST_F(HypothalamusTrainingBridgeTest, GetHomeostaticStateSuccess) {
    hypo_training_homeostatic_state_t state;
    EXPECT_EQ(0, hypo_training_bridge_get_homeostatic_state(bridge, &state));

    // Initial state should have default setpoint
    EXPECT_FLOAT_EQ(HYPO_TRAINING_DEFAULT_LOSS_SETPOINT, state.loss_setpoint);
    EXPECT_EQ(HYPO_TRAIN_STATE_HEALTHY, state.state);
}

TEST_F(HypothalamusTrainingBridgeTest, GetHomeostaticStateNullBridgeFails) {
    hypo_training_homeostatic_state_t state;
    EXPECT_NE(0, hypo_training_bridge_get_homeostatic_state(nullptr, &state));
}

TEST_F(HypothalamusTrainingBridgeTest, GetHomeostaticStateNullOutputFails) {
    EXPECT_NE(0, hypo_training_bridge_get_homeostatic_state(bridge, nullptr));
}

TEST_F(HypothalamusTrainingBridgeTest, GetTrainingStateSuccess) {
    hypo_training_state_t state;
    EXPECT_EQ(0, hypo_training_bridge_get_training_state(bridge, &state));

    // Initially should be healthy
    EXPECT_EQ(HYPO_TRAIN_STATE_HEALTHY, state);
}

TEST_F(HypothalamusTrainingBridgeTest, GetTrainingStateNullBridgeFails) {
    hypo_training_state_t state;
    EXPECT_NE(0, hypo_training_bridge_get_training_state(nullptr, &state));
}

TEST_F(HypothalamusTrainingBridgeTest, GetTrainingStateNullOutputFails) {
    EXPECT_NE(0, hypo_training_bridge_get_training_state(bridge, nullptr));
}

TEST_F(HypothalamusTrainingBridgeTest, SetLossSetpointSuccess) {
    EXPECT_EQ(0, hypo_training_bridge_set_loss_setpoint(bridge, 0.3f));

    hypo_training_homeostatic_state_t state;
    EXPECT_EQ(0, hypo_training_bridge_get_homeostatic_state(bridge, &state));
    EXPECT_FLOAT_EQ(0.3f, state.loss_setpoint);
}

TEST_F(HypothalamusTrainingBridgeTest, SetLossSetpointNullBridgeFails) {
    EXPECT_NE(0, hypo_training_bridge_set_loss_setpoint(nullptr, 0.3f));
}

TEST_F(HypothalamusTrainingBridgeTest, SetLossSetpointNegativeClampsToMin) {
    // Negative values are clamped to min_setpoint
    EXPECT_EQ(0, hypo_training_bridge_set_loss_setpoint(bridge, -0.1f));

    hypo_training_homeostatic_state_t state;
    EXPECT_EQ(0, hypo_training_bridge_get_homeostatic_state(bridge, &state));
    EXPECT_GE(state.loss_setpoint, 0.0f);  // Should be clamped to min
}

TEST_F(HypothalamusTrainingBridgeTest, SetLossSetpointZeroSucceeds) {
    // Zero setpoint might be valid for perfect fit scenarios
    EXPECT_EQ(0, hypo_training_bridge_set_loss_setpoint(bridge, 0.0f));
}

// ============================================================================
// DRIVE STATE TESTS
// ============================================================================

TEST_F(HypothalamusTrainingBridgeTest, GetDriveStateSuccess) {
    hypo_training_drive_state_t drive_state;
    EXPECT_EQ(0, hypo_training_bridge_get_drive_state(bridge, &drive_state));

    // Drives are initialized with sensible defaults (not zero)
    EXPECT_FLOAT_EQ(0.5f, drive_state.curiosity_activation);
    EXPECT_FLOAT_EQ(0.2f, drive_state.safety_activation);
    EXPECT_FLOAT_EQ(0.3f, drive_state.competence_activation);
    EXPECT_FLOAT_EQ(0.0f, drive_state.fatigue_level);
    EXPECT_FLOAT_EQ(0.5f, drive_state.autonomy_activation);

    // Derived states
    EXPECT_FLOAT_EQ(1.0f - drive_state.fatigue_level, drive_state.learning_readiness);
}

TEST_F(HypothalamusTrainingBridgeTest, GetDriveStateNullBridgeFails) {
    hypo_training_drive_state_t drive_state;
    EXPECT_NE(0, hypo_training_bridge_get_drive_state(nullptr, &drive_state));
}

TEST_F(HypothalamusTrainingBridgeTest, GetDriveStateNullOutputFails) {
    EXPECT_NE(0, hypo_training_bridge_get_drive_state(bridge, nullptr));
}

TEST_F(HypothalamusTrainingBridgeTest, SetDriveCuriositySuccess) {
    EXPECT_EQ(0, hypo_training_bridge_set_drive(bridge, 0, 0.7f));

    hypo_training_drive_state_t drive_state;
    EXPECT_EQ(0, hypo_training_bridge_get_drive_state(bridge, &drive_state));
    EXPECT_FLOAT_EQ(0.7f, drive_state.curiosity_activation);
}

TEST_F(HypothalamusTrainingBridgeTest, SetDriveSafetySuccess) {
    EXPECT_EQ(0, hypo_training_bridge_set_drive(bridge, 1, 0.6f));

    hypo_training_drive_state_t drive_state;
    EXPECT_EQ(0, hypo_training_bridge_get_drive_state(bridge, &drive_state));
    EXPECT_FLOAT_EQ(0.6f, drive_state.safety_activation);
}

TEST_F(HypothalamusTrainingBridgeTest, SetDriveCompetenceSuccess) {
    EXPECT_EQ(0, hypo_training_bridge_set_drive(bridge, 2, 0.5f));

    hypo_training_drive_state_t drive_state;
    EXPECT_EQ(0, hypo_training_bridge_get_drive_state(bridge, &drive_state));
    EXPECT_FLOAT_EQ(0.5f, drive_state.competence_activation);
}

TEST_F(HypothalamusTrainingBridgeTest, SetDriveFatigueSuccess) {
    EXPECT_EQ(0, hypo_training_bridge_set_drive(bridge, 3, 0.4f));

    hypo_training_drive_state_t drive_state;
    EXPECT_EQ(0, hypo_training_bridge_get_drive_state(bridge, &drive_state));
    EXPECT_FLOAT_EQ(0.4f, drive_state.fatigue_level);

    // Learning readiness should reflect fatigue
    EXPECT_FLOAT_EQ(0.6f, drive_state.learning_readiness);
}

TEST_F(HypothalamusTrainingBridgeTest, SetDriveAutonomySuccess) {
    EXPECT_EQ(0, hypo_training_bridge_set_drive(bridge, 4, 0.8f));

    hypo_training_drive_state_t drive_state;
    EXPECT_EQ(0, hypo_training_bridge_get_drive_state(bridge, &drive_state));
    EXPECT_FLOAT_EQ(0.8f, drive_state.autonomy_activation);
}

TEST_F(HypothalamusTrainingBridgeTest, SetDriveNullBridgeFails) {
    EXPECT_NE(0, hypo_training_bridge_set_drive(nullptr, 0, 0.5f));
}

TEST_F(HypothalamusTrainingBridgeTest, SetDriveInvalidTypeFails) {
    EXPECT_NE(0, hypo_training_bridge_set_drive(bridge, 5, 0.5f));
    EXPECT_NE(0, hypo_training_bridge_set_drive(bridge, 100, 0.5f));
}

TEST_F(HypothalamusTrainingBridgeTest, SetDriveClampsToBounds) {
    // Set drive above 1.0
    EXPECT_EQ(0, hypo_training_bridge_set_drive(bridge, 0, 1.5f));

    hypo_training_drive_state_t drive_state;
    EXPECT_EQ(0, hypo_training_bridge_get_drive_state(bridge, &drive_state));
    EXPECT_LE(drive_state.curiosity_activation, 1.0f);

    // Set drive below 0.0
    EXPECT_EQ(0, hypo_training_bridge_set_drive(bridge, 0, -0.5f));

    EXPECT_EQ(0, hypo_training_bridge_get_drive_state(bridge, &drive_state));
    EXPECT_GE(drive_state.curiosity_activation, 0.0f);
}

TEST_F(HypothalamusTrainingBridgeTest, ResetFatigueSuccess) {
    // Set fatigue
    EXPECT_EQ(0, hypo_training_bridge_set_drive(bridge, 3, 0.8f));

    // Reset fatigue
    EXPECT_EQ(0, hypo_training_bridge_reset_fatigue(bridge));

    hypo_training_drive_state_t drive_state;
    EXPECT_EQ(0, hypo_training_bridge_get_drive_state(bridge, &drive_state));
    EXPECT_FLOAT_EQ(0.0f, drive_state.fatigue_level);
    EXPECT_FLOAT_EQ(1.0f, drive_state.learning_readiness);
}

TEST_F(HypothalamusTrainingBridgeTest, ResetFatigueNullBridgeFails) {
    EXPECT_NE(0, hypo_training_bridge_reset_fatigue(nullptr));
}

// ============================================================================
// STATISTICS TESTS
// ============================================================================

TEST_F(HypothalamusTrainingBridgeTest, GetStatsSuccess) {
    hypo_training_bridge_stats_t stats;
    EXPECT_EQ(0, hypo_training_bridge_get_stats(bridge, &stats));

    // Initial stats should be zero
    EXPECT_EQ(0u, stats.training_events_received);
    EXPECT_EQ(0u, stats.modulations_published);
    EXPECT_EQ(0u, stats.drive_updates);
    EXPECT_EQ(0u, stats.lr_modulations);
    EXPECT_EQ(0u, stats.safety_interventions);
    EXPECT_EQ(0u, stats.consolidation_phases);
    EXPECT_GE(stats.uptime_us, 0u);
}

TEST_F(HypothalamusTrainingBridgeTest, GetStatsNullBridgeFails) {
    hypo_training_bridge_stats_t stats;
    EXPECT_NE(0, hypo_training_bridge_get_stats(nullptr, &stats));
}

TEST_F(HypothalamusTrainingBridgeTest, GetStatsNullOutputFails) {
    EXPECT_NE(0, hypo_training_bridge_get_stats(bridge, nullptr));
}

TEST_F(HypothalamusTrainingBridgeTest, GetStatsAfterEvents) {
    // Process some events
    EXPECT_EQ(0, hypo_training_bridge_process_loss(bridge, 1, 0, 0.5f));
    EXPECT_EQ(0, hypo_training_bridge_process_gradient(bridge, 1.0f, false));
    EXPECT_EQ(0, hypo_training_bridge_process_epoch(bridge, 1, 0.5f));

    hypo_training_bridge_stats_t stats;
    EXPECT_EQ(0, hypo_training_bridge_get_stats(bridge, &stats));

    // Should have counted events
    EXPECT_GE(stats.training_events_received, 3u);
}

TEST_F(HypothalamusTrainingBridgeTest, ResetStatsSuccess) {
    // Process some events
    EXPECT_EQ(0, hypo_training_bridge_process_loss(bridge, 1, 0, 0.5f));
    EXPECT_EQ(0, hypo_training_bridge_process_epoch(bridge, 1, 0.5f));

    // Reset stats
    EXPECT_EQ(0, hypo_training_bridge_reset_stats(bridge));

    hypo_training_bridge_stats_t stats;
    EXPECT_EQ(0, hypo_training_bridge_get_stats(bridge, &stats));

    // Event counts should be reset
    EXPECT_EQ(0u, stats.training_events_received);
    EXPECT_EQ(0u, stats.modulations_published);
}

TEST_F(HypothalamusTrainingBridgeTest, ResetStatsNullBridgeFails) {
    EXPECT_NE(0, hypo_training_bridge_reset_stats(nullptr));
}

// ============================================================================
// UTILITY FUNCTION TESTS
// ============================================================================

TEST_F(HypothalamusTrainingBridgeLifecycleTest, TrainingStateNameReturnsValidStrings) {
    EXPECT_STREQ("HEALTHY", hypo_training_state_name(HYPO_TRAIN_STATE_HEALTHY));
    EXPECT_STREQ("IMPROVING", hypo_training_state_name(HYPO_TRAIN_STATE_IMPROVING));
    EXPECT_STREQ("PLATEAU", hypo_training_state_name(HYPO_TRAIN_STATE_PLATEAU));
    EXPECT_STREQ("DIVERGING", hypo_training_state_name(HYPO_TRAIN_STATE_DIVERGING));
    EXPECT_STREQ("UNSTABLE", hypo_training_state_name(HYPO_TRAIN_STATE_UNSTABLE));
    EXPECT_STREQ("CRITICAL", hypo_training_state_name(HYPO_TRAIN_STATE_CRITICAL));
}

TEST_F(HypothalamusTrainingBridgeLifecycleTest, TrainingStateNameInvalidReturnsInvalid) {
    const char* name = hypo_training_state_name((hypo_training_state_t)100);
    EXPECT_NE(nullptr, name);
    EXPECT_TRUE(strlen(name) > 0);
}

TEST_F(HypothalamusTrainingBridgeLifecycleTest, ConsolidationTypeNameReturnsValidStrings) {
    EXPECT_STREQ("NONE", hypo_consolidation_type_name(HYPO_CONSOL_NONE));
    EXPECT_STREQ("MINI_REST", hypo_consolidation_type_name(HYPO_CONSOL_MINI_REST));
    EXPECT_STREQ("CHECKPOINT", hypo_consolidation_type_name(HYPO_CONSOL_CHECKPOINT));
    EXPECT_STREQ("REPLAY", hypo_consolidation_type_name(HYPO_CONSOL_REPLAY));
    EXPECT_STREQ("FULL_REST", hypo_consolidation_type_name(HYPO_CONSOL_FULL_REST));
}

TEST_F(HypothalamusTrainingBridgeLifecycleTest, ConsolidationTypeNameInvalidReturnsInvalid) {
    const char* name = hypo_consolidation_type_name((hypo_consolidation_type_t)100);
    EXPECT_NE(nullptr, name);
    EXPECT_TRUE(strlen(name) > 0);
}

TEST_F(HypothalamusTrainingBridgeLifecycleTest, ModulationNameReturnsValidStrings) {
    EXPECT_STREQ("LEARNING_RATE", hypo_training_modulation_name(HYPO_TRAIN_MOD_LEARNING_RATE));
    EXPECT_STREQ("BATCH_SIZE", hypo_training_modulation_name(HYPO_TRAIN_MOD_BATCH_SIZE));
    EXPECT_STREQ("GRADIENT_CLIP", hypo_training_modulation_name(HYPO_TRAIN_MOD_GRADIENT_CLIP));
    EXPECT_STREQ("CURRICULUM_DIFFICULTY", hypo_training_modulation_name(HYPO_TRAIN_MOD_CURRICULUM_DIFF));
    EXPECT_STREQ("SAMPLE_PRIORITY", hypo_training_modulation_name(HYPO_TRAIN_MOD_SAMPLE_PRIORITY));
    EXPECT_STREQ("CHECKPOINT_FREQUENCY", hypo_training_modulation_name(HYPO_TRAIN_MOD_CHECKPOINT_FREQ));
    EXPECT_STREQ("MULTI_TASK_WEIGHT", hypo_training_modulation_name(HYPO_TRAIN_MOD_MULTI_TASK_WEIGHT));
    EXPECT_STREQ("REPLAY_PRIORITY", hypo_training_modulation_name(HYPO_TRAIN_MOD_REPLAY_PRIORITY));
}

TEST_F(HypothalamusTrainingBridgeLifecycleTest, ModulationNameInvalidReturnsInvalid) {
    const char* name = hypo_training_modulation_name(HYPO_TRAIN_MOD_COUNT);
    EXPECT_NE(nullptr, name);

    name = hypo_training_modulation_name((hypo_training_modulation_type_t)100);
    EXPECT_NE(nullptr, name);
    EXPECT_TRUE(strlen(name) > 0);
}

TEST_F(HypothalamusTrainingBridgeTest, PrintSummaryDoesNotCrash) {
    // Process some events to create interesting state
    EXPECT_EQ(0, hypo_training_bridge_process_loss(bridge, 1, 0, 0.5f));
    EXPECT_EQ(0, hypo_training_bridge_process_epoch(bridge, 1, 0.5f));
    EXPECT_EQ(0, hypo_training_bridge_set_drive(bridge, 0, 0.6f));

    // Should not crash
    hypo_training_bridge_print_summary(bridge);
}

TEST_F(HypothalamusTrainingBridgeTest, PrintSummaryNullDoesNotCrash) {
    hypo_training_bridge_print_summary(nullptr);
    // Should not crash
}

TEST_F(HypothalamusTrainingBridgeTest, PrintStatsDoesNotCrash) {
    hypo_training_bridge_stats_t stats;
    EXPECT_EQ(0, hypo_training_bridge_get_stats(bridge, &stats));

    // Should not crash
    hypo_training_bridge_print_stats(&stats);
}

TEST_F(HypothalamusTrainingBridgeTest, PrintStatsNullDoesNotCrash) {
    hypo_training_bridge_print_stats(nullptr);
    // Should not crash
}

// ============================================================================
// INTEGRATION SCENARIO TESTS
// ============================================================================

TEST_F(HypothalamusTrainingBridgeTest, ScenarioNormalTraining) {
    // Simulate normal training with decreasing loss
    for (int epoch = 0; epoch < 10; epoch++) {
        float avg_loss = 0.8f - (epoch * 0.05f);

        for (int batch = 0; batch < 5; batch++) {
            float batch_loss = avg_loss + ((float)(batch - 2) * 0.02f);
            EXPECT_EQ(0, hypo_training_bridge_process_loss(bridge, epoch, batch, batch_loss));
            EXPECT_EQ(0, hypo_training_bridge_process_gradient(bridge, 0.5f + batch * 0.1f, false));
        }

        EXPECT_EQ(0, hypo_training_bridge_process_epoch(bridge, epoch, avg_loss));
    }

    // Check final state
    hypo_training_state_t state;
    EXPECT_EQ(0, hypo_training_bridge_get_training_state(bridge, &state));
    EXPECT_TRUE(state == HYPO_TRAIN_STATE_HEALTHY || state == HYPO_TRAIN_STATE_IMPROVING);

    // Check modulation is reasonable
    float lr_mult;
    EXPECT_EQ(0, hypo_training_bridge_get_lr_multiplier(bridge, &lr_mult));
    EXPECT_GT(lr_mult, 0.5f);
    EXPECT_LT(lr_mult, 2.0f);
}

TEST_F(HypothalamusTrainingBridgeTest, ScenarioUnstableTraining) {
    // Simulate unstable training with high variance
    for (int batch = 0; batch < 20; batch++) {
        float loss = 0.5f + 0.3f * sinf(batch * 2.0f);
        EXPECT_EQ(0, hypo_training_bridge_process_loss(bridge, 0, batch, loss));
        EXPECT_EQ(0, hypo_training_bridge_process_gradient(bridge, 10.0f + batch * 5.0f, batch % 2 == 0));
    }

    // Safety drive should be elevated
    hypo_training_drive_state_t drive_state;
    EXPECT_EQ(0, hypo_training_bridge_get_drive_state(bridge, &drive_state));
    EXPECT_GT(drive_state.safety_activation, 0.3f);
}

TEST_F(HypothalamusTrainingBridgeTest, ScenarioFatigueAndConsolidation) {
    // Simulate many epochs to accumulate fatigue
    for (int epoch = 0; epoch < 50; epoch++) {
        EXPECT_EQ(0, hypo_training_bridge_process_epoch(bridge, epoch, 0.5f));
    }

    // Check fatigue is elevated
    hypo_training_drive_state_t drive_state;
    EXPECT_EQ(0, hypo_training_bridge_get_drive_state(bridge, &drive_state));
    EXPECT_GT(drive_state.fatigue_level, 0.5f);

    // Check consolidation is recommended
    hypo_consolidation_type_t consol_type;
    EXPECT_EQ(0, hypo_training_bridge_check_consolidation(bridge, &consol_type));
    EXPECT_NE(HYPO_CONSOL_NONE, consol_type);

    // Reset fatigue (simulating consolidation)
    EXPECT_EQ(0, hypo_training_bridge_reset_fatigue(bridge));

    // Verify fatigue is reset
    EXPECT_EQ(0, hypo_training_bridge_get_drive_state(bridge, &drive_state));
    EXPECT_FLOAT_EQ(0.0f, drive_state.fatigue_level);

    // No consolidation needed now
    EXPECT_EQ(0, hypo_training_bridge_check_consolidation(bridge, &consol_type));
    EXPECT_EQ(HYPO_CONSOL_NONE, consol_type);
}

TEST_F(HypothalamusTrainingBridgeTest, ScenarioLRScheduling) {
    // Simulate warmup phase with increasing LR
    EXPECT_EQ(0, hypo_training_bridge_process_lr_change(bridge, 0.0001f, 0.001f));
    EXPECT_EQ(0, hypo_training_bridge_process_lr_change(bridge, 0.001f, 0.01f));

    hypo_training_drive_state_t drive_state;
    EXPECT_EQ(0, hypo_training_bridge_get_drive_state(bridge, &drive_state));
    // Curiosity should be elevated during warmup
    EXPECT_GT(drive_state.curiosity_activation, 0.2f);

    // Simulate decay phase with decreasing LR
    EXPECT_EQ(0, hypo_training_bridge_process_lr_change(bridge, 0.01f, 0.005f));
    EXPECT_EQ(0, hypo_training_bridge_process_lr_change(bridge, 0.005f, 0.001f));

    EXPECT_EQ(0, hypo_training_bridge_get_drive_state(bridge, &drive_state));
    // Exploration tendency should decrease
    EXPECT_LT(drive_state.exploration_tendency, 0.8f);
}

TEST_F(HypothalamusTrainingBridgeTest, ScenarioAdaptiveSetpoint) {
    // Start with high setpoint
    EXPECT_EQ(0, hypo_training_bridge_set_loss_setpoint(bridge, 0.8f));

    // Process improving losses
    EXPECT_EQ(0, hypo_training_bridge_process_loss(bridge, 0, 0, 0.7f));
    EXPECT_EQ(0, hypo_training_bridge_process_loss(bridge, 0, 1, 0.5f));
    EXPECT_EQ(0, hypo_training_bridge_process_loss(bridge, 0, 2, 0.3f));

    // Reduce setpoint as training improves
    EXPECT_EQ(0, hypo_training_bridge_set_loss_setpoint(bridge, 0.4f));

    hypo_training_homeostatic_state_t homeo_state;
    EXPECT_EQ(0, hypo_training_bridge_get_homeostatic_state(bridge, &homeo_state));
    EXPECT_FLOAT_EQ(0.4f, homeo_state.loss_setpoint);

    // Loss below new setpoint should show negative deviation
    EXPECT_EQ(0, hypo_training_bridge_process_loss(bridge, 0, 3, 0.2f));
    EXPECT_EQ(0, hypo_training_bridge_get_homeostatic_state(bridge, &homeo_state));
    EXPECT_LT(homeo_state.deviation, 0.0f);
}

// ============================================================================
// BOUNDARY CONDITION TESTS
// ============================================================================

TEST_F(HypothalamusTrainingBridgeTest, ProcessLossWithZeroValue) {
    EXPECT_EQ(0, hypo_training_bridge_process_loss(bridge, 0, 0, 0.0f));

    hypo_training_homeostatic_state_t state;
    EXPECT_EQ(0, hypo_training_bridge_get_homeostatic_state(bridge, &state));
    EXPECT_FLOAT_EQ(0.0f, state.current_loss);
}

TEST_F(HypothalamusTrainingBridgeTest, ProcessLossWithVeryHighValue) {
    EXPECT_EQ(0, hypo_training_bridge_process_loss(bridge, 0, 0, 1000000.0f));

    hypo_training_state_t state;
    EXPECT_EQ(0, hypo_training_bridge_get_training_state(bridge, &state));
    EXPECT_EQ(HYPO_TRAIN_STATE_CRITICAL, state);
}

TEST_F(HypothalamusTrainingBridgeTest, ProcessGradientWithZeroNorm) {
    EXPECT_EQ(0, hypo_training_bridge_process_gradient(bridge, 0.0f, false));
}

TEST_F(HypothalamusTrainingBridgeTest, ProcessGradientWithVeryHighNorm) {
    EXPECT_EQ(0, hypo_training_bridge_process_gradient(bridge, 1e10f, true));

    hypo_training_drive_state_t drive_state;
    EXPECT_EQ(0, hypo_training_bridge_get_drive_state(bridge, &drive_state));
    // Safety should increase from baseline (0.2 + 0.2 = 0.4)
    EXPECT_GT(drive_state.safety_activation, 0.3f);
}

TEST_F(HypothalamusTrainingBridgeTest, ProcessLRChangeWithZeroOldLR) {
    EXPECT_EQ(0, hypo_training_bridge_process_lr_change(bridge, 0.0f, 0.01f));
}

TEST_F(HypothalamusTrainingBridgeTest, ProcessLRChangeWithZeroNewLR) {
    EXPECT_EQ(0, hypo_training_bridge_process_lr_change(bridge, 0.01f, 0.0f));
}

TEST_F(HypothalamusTrainingBridgeTest, ProcessEpochWithHighEpochNumber) {
    EXPECT_EQ(0, hypo_training_bridge_process_epoch(bridge, 1000000, 0.5f));
}

TEST_F(HypothalamusTrainingBridgeTest, AllDrivesSetToOne) {
    // Set all drives to maximum
    for (uint32_t i = 0; i < 5; i++) {
        EXPECT_EQ(0, hypo_training_bridge_set_drive(bridge, i, 1.0f));
    }

    hypo_training_drive_state_t drive_state;
    EXPECT_EQ(0, hypo_training_bridge_get_drive_state(bridge, &drive_state));

    EXPECT_FLOAT_EQ(1.0f, drive_state.curiosity_activation);
    EXPECT_FLOAT_EQ(1.0f, drive_state.safety_activation);
    EXPECT_FLOAT_EQ(1.0f, drive_state.competence_activation);
    EXPECT_FLOAT_EQ(1.0f, drive_state.fatigue_level);
    EXPECT_FLOAT_EQ(1.0f, drive_state.autonomy_activation);
    EXPECT_FLOAT_EQ(0.0f, drive_state.learning_readiness);  // 1 - fatigue
}

TEST_F(HypothalamusTrainingBridgeTest, AllDrivesSetToZero) {
    // First set drives to non-zero
    for (uint32_t i = 0; i < 5; i++) {
        EXPECT_EQ(0, hypo_training_bridge_set_drive(bridge, i, 0.5f));
    }

    // Then set all to zero
    for (uint32_t i = 0; i < 5; i++) {
        EXPECT_EQ(0, hypo_training_bridge_set_drive(bridge, i, 0.0f));
    }

    hypo_training_drive_state_t drive_state;
    EXPECT_EQ(0, hypo_training_bridge_get_drive_state(bridge, &drive_state));

    EXPECT_FLOAT_EQ(0.0f, drive_state.curiosity_activation);
    EXPECT_FLOAT_EQ(0.0f, drive_state.safety_activation);
    EXPECT_FLOAT_EQ(0.0f, drive_state.competence_activation);
    EXPECT_FLOAT_EQ(0.0f, drive_state.fatigue_level);
    EXPECT_FLOAT_EQ(0.0f, drive_state.autonomy_activation);
    EXPECT_FLOAT_EQ(1.0f, drive_state.learning_readiness);
}

// ============================================================================
// MODULATION RECOMMENDATION TESTS
// ============================================================================

TEST_F(HypothalamusTrainingBridgeTest, ModulationRecommendsEarlyStoppingOnCritical) {
    // Create critical state
    float inf_val = std::numeric_limits<float>::infinity();
    EXPECT_EQ(0, hypo_training_bridge_process_loss(bridge, 0, 0, inf_val));

    hypo_training_modulation_t modulation;
    EXPECT_EQ(0, hypo_training_bridge_compute_modulation(bridge, &modulation));

    // Should recommend early stopping or checkpoint
    EXPECT_TRUE(modulation.recommend_early_stopping || modulation.recommend_checkpoint);
}

TEST_F(HypothalamusTrainingBridgeTest, ModulationRecommendsCheckpointOnImprovement) {
    // Process improving losses
    EXPECT_EQ(0, hypo_training_bridge_process_loss(bridge, 0, 0, 0.8f));
    EXPECT_EQ(0, hypo_training_bridge_process_epoch(bridge, 0, 0.8f));
    EXPECT_EQ(0, hypo_training_bridge_process_loss(bridge, 1, 0, 0.5f));
    EXPECT_EQ(0, hypo_training_bridge_process_epoch(bridge, 1, 0.5f));
    EXPECT_EQ(0, hypo_training_bridge_process_loss(bridge, 2, 0, 0.3f));
    EXPECT_EQ(0, hypo_training_bridge_process_epoch(bridge, 2, 0.3f));

    hypo_training_modulation_t modulation;
    EXPECT_EQ(0, hypo_training_bridge_compute_modulation(bridge, &modulation));

    // Checkpoint urgency should be elevated
    EXPECT_GT(modulation.checkpoint_urgency, 0.0f);
}

TEST_F(HypothalamusTrainingBridgeTest, ModulationRecommendsLRReductionOnInstability) {
    // Set safety drive high to trigger LR reduction recommendation
    EXPECT_EQ(0, hypo_training_bridge_set_drive(bridge, 1, 0.7f));  // Safety drive

    hypo_training_modulation_t modulation;
    EXPECT_EQ(0, hypo_training_bridge_compute_modulation(bridge, &modulation));

    // Should recommend LR reduction when safety is high
    EXPECT_TRUE(modulation.recommend_lr_reduction || modulation.lr_multiplier < 1.0f);
}

// ============================================================================
// PLATEAU DETECTION TESTS
// ============================================================================

TEST_F(HypothalamusTrainingBridgeTest, DetectsPlateauState) {
    // Process constant losses (plateau)
    for (int i = 0; i < 20; i++) {
        EXPECT_EQ(0, hypo_training_bridge_process_loss(bridge, 0, i, 0.5f));
    }
    for (int epoch = 0; epoch < 10; epoch++) {
        EXPECT_EQ(0, hypo_training_bridge_process_epoch(bridge, epoch, 0.5f));
    }

    hypo_training_homeostatic_state_t homeo_state;
    EXPECT_EQ(0, hypo_training_bridge_get_homeostatic_state(bridge, &homeo_state));

    // Loss variance should be low
    EXPECT_LT(homeo_state.loss_variance, 0.01f);

    hypo_training_state_t state;
    EXPECT_EQ(0, hypo_training_bridge_get_training_state(bridge, &state));

    // Should be plateau or healthy (if loss matches setpoint)
    EXPECT_TRUE(state == HYPO_TRAIN_STATE_PLATEAU || state == HYPO_TRAIN_STATE_HEALTHY);
}

// ============================================================================
// EPOCHS SINCE IMPROVEMENT TRACKING
// ============================================================================

TEST_F(HypothalamusTrainingBridgeTest, TracksEpochsSinceImprovement) {
    // Initial best loss - tracked via process_loss
    EXPECT_EQ(0, hypo_training_bridge_process_loss(bridge, 0, 0, 0.5f));
    EXPECT_EQ(0, hypo_training_bridge_process_epoch(bridge, 0, 0.5f));

    // Process worse epochs (loss >= best_loss_seen)
    EXPECT_EQ(0, hypo_training_bridge_process_loss(bridge, 1, 0, 0.6f));
    EXPECT_EQ(0, hypo_training_bridge_process_epoch(bridge, 1, 0.6f));
    EXPECT_EQ(0, hypo_training_bridge_process_loss(bridge, 2, 0, 0.6f));
    EXPECT_EQ(0, hypo_training_bridge_process_epoch(bridge, 2, 0.6f));
    EXPECT_EQ(0, hypo_training_bridge_process_loss(bridge, 3, 0, 0.55f));
    EXPECT_EQ(0, hypo_training_bridge_process_epoch(bridge, 3, 0.55f));

    hypo_training_homeostatic_state_t state;
    EXPECT_EQ(0, hypo_training_bridge_get_homeostatic_state(bridge, &state));

    // Should have counted epochs since improvement (3 epochs without new best)
    EXPECT_GT(state.epochs_since_improvement, 0u);

    // Now process improvement (new best loss)
    EXPECT_EQ(0, hypo_training_bridge_process_loss(bridge, 4, 0, 0.4f));

    EXPECT_EQ(0, hypo_training_bridge_get_homeostatic_state(bridge, &state));
    // Counter should reset on new best
    EXPECT_EQ(0u, state.epochs_since_improvement);
}

// ============================================================================
// EXPLORATION TENDENCY TESTS
// ============================================================================

TEST_F(HypothalamusTrainingBridgeTest, ExplorationTendencyBalance) {
    // Set curiosity high, safety low
    EXPECT_EQ(0, hypo_training_bridge_set_drive(bridge, 0, 0.8f));  // Curiosity
    EXPECT_EQ(0, hypo_training_bridge_set_drive(bridge, 1, 0.2f));  // Safety

    hypo_training_drive_state_t drive_state;
    EXPECT_EQ(0, hypo_training_bridge_get_drive_state(bridge, &drive_state));

    // Exploration tendency should be positive
    EXPECT_GT(drive_state.exploration_tendency, 0.0f);

    // Set curiosity low, safety high
    EXPECT_EQ(0, hypo_training_bridge_set_drive(bridge, 0, 0.2f));  // Curiosity
    EXPECT_EQ(0, hypo_training_bridge_set_drive(bridge, 1, 0.8f));  // Safety

    EXPECT_EQ(0, hypo_training_bridge_get_drive_state(bridge, &drive_state));

    // Exploration tendency should be negative
    EXPECT_LT(drive_state.exploration_tendency, 0.0f);
}

// ============================================================================
// DIFFICULTY READINESS TESTS
// ============================================================================

TEST_F(HypothalamusTrainingBridgeTest, DifficultyReadinessReflectsCompetence) {
    // Set high competence
    EXPECT_EQ(0, hypo_training_bridge_set_drive(bridge, 2, 0.9f));

    hypo_training_drive_state_t drive_state;
    EXPECT_EQ(0, hypo_training_bridge_get_drive_state(bridge, &drive_state));

    // Difficulty readiness should be elevated
    EXPECT_GT(drive_state.difficulty_readiness, 0.5f);

    // Set low competence
    EXPECT_EQ(0, hypo_training_bridge_set_drive(bridge, 2, 0.1f));

    EXPECT_EQ(0, hypo_training_bridge_get_drive_state(bridge, &drive_state));

    // Difficulty readiness should be low
    EXPECT_LT(drive_state.difficulty_readiness, 0.5f);
}

// ============================================================================
// MULTIPLE RESET CYCLES
// ============================================================================

TEST_F(HypothalamusTrainingBridgeTest, MultipleResetCycles) {
    for (int cycle = 0; cycle < 5; cycle++) {
        // Process some events
        EXPECT_EQ(0, hypo_training_bridge_process_loss(bridge, cycle, 0, 0.5f));
        EXPECT_EQ(0, hypo_training_bridge_set_drive(bridge, 3, 0.5f));

        // Reset
        EXPECT_EQ(0, hypo_training_bridge_reset(bridge));

        // Verify clean state
        hypo_training_state_t state;
        EXPECT_EQ(0, hypo_training_bridge_get_training_state(bridge, &state));
        EXPECT_EQ(HYPO_TRAIN_STATE_HEALTHY, state);
    }
}

// ============================================================================
// STATS ACCUMULATION ACROSS EVENTS
// ============================================================================

TEST_F(HypothalamusTrainingBridgeTest, StatsAccumulateCorrectly) {
    hypo_training_bridge_stats_t stats_before, stats_after;

    EXPECT_EQ(0, hypo_training_bridge_get_stats(bridge, &stats_before));

    // Process various events
    EXPECT_EQ(0, hypo_training_bridge_process_loss(bridge, 0, 0, 0.5f));
    EXPECT_EQ(0, hypo_training_bridge_process_loss(bridge, 0, 1, 0.5f));
    EXPECT_EQ(0, hypo_training_bridge_process_gradient(bridge, 1.0f, false));
    EXPECT_EQ(0, hypo_training_bridge_process_epoch(bridge, 0, 0.5f));
    EXPECT_EQ(0, hypo_training_bridge_process_lr_change(bridge, 0.01f, 0.001f));

    EXPECT_EQ(0, hypo_training_bridge_get_stats(bridge, &stats_after));

    // Event count should have increased
    EXPECT_GT(stats_after.training_events_received, stats_before.training_events_received);
}
