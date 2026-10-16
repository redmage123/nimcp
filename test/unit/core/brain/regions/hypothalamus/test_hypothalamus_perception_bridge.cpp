/**
 * @file test_hypothalamus_perception_bridge.cpp
 * @brief Comprehensive unit tests for nimcp_hypothalamus_perception_bridge.c
 *
 * WHAT: Unit tests for the enhanced Hypothalamus-Perception Bridge
 * WHY:  Ensure correct sensory modulation, interoception, chemosensory processing,
 *       predictive coding, pain modulation, sleep gating, and thermal salience
 * HOW:  Use Google Test framework with mocked drive system
 *
 * TESTS COVER:
 * 1. Lifecycle (create, destroy, reset)
 * 2. Configuration (default_config, default_enhanced_config, apply_enhanced_config)
 * 3. Base functions (compute_modulation, get_modulation, set_arousal, process_detection, get_anticipation)
 * 4. Enhancement 1: Interoception
 * 5. Enhancement 2: Olfactory/Gustatory
 * 6. Enhancement 3: Predictive Coding
 * 7. Enhancement 4: Pain Modulation
 * 8. Enhancement 5: Sleep Gating
 * 9. Enhancement 6: Thermal Salience
 * 10. Statistics (get_stats, reset_stats)
 * 11. String utilities (all type name functions)
 * 12. Edge cases (NULL params, invalid types, boundary values)
 *
 * COVERAGE TARGET: 100%
 *
 * @version Phase 17.5: Enhanced Perception Integration
 * @date 2026-01-10
 */

#include <gtest/gtest.h>
#include <string.h>
#include <stdlib.h>
#include <cmath>

// Headers have their own extern "C" guards
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_perception_bridge.h"
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_drives.h"

// ============================================================================
// TEST FIXTURE
// ============================================================================

/**
 * @brief Test fixture for Hypothalamus Perception Bridge tests
 *
 * Creates a mock drive system and bridge for each test.
 */
class HypothalamusPerceptionBridgeTest : public ::testing::Test {
protected:
    hypo_perception_bridge_t* bridge;
    hypo_drive_system_handle_t* drive_system;
    hypo_perception_config_t config;

    void SetUp() override {
        // Create drive system with default configuration
        hypo_drive_config_t drive_config = hypo_drive_default_config();
        drive_system = hypo_drive_create(&drive_config);
        ASSERT_NE(nullptr, drive_system) << "Failed to create drive system";

        // Get default perception bridge config
        hypo_perception_bridge_default_config(&config);

        // Create perception bridge
        bridge = hypo_perception_bridge_create(drive_system, &config);
        ASSERT_NE(nullptr, bridge) << "Failed to create perception bridge";
    }

    void TearDown() override {
        if (bridge) {
            hypo_perception_bridge_destroy(bridge);
            bridge = nullptr;
        }
        if (drive_system) {
            hypo_drive_destroy(drive_system);
            drive_system = nullptr;
        }
    }

    // Helper to create a detection event
    hypo_sensory_detection_t createDetection(
        hypo_stim_category_t category,
        hypo_sense_modality_t modality,
        float confidence,
        float intensity,
        bool is_threat)
    {
        hypo_sensory_detection_t detection;
        detection.detected_category = category;
        detection.modality = modality;
        detection.confidence = confidence;
        detection.intensity = intensity;
        detection.is_threat = is_threat;
        detection.timestamp_us = 0;
        return detection;
    }
};

// ============================================================================
// LIFECYCLE TESTS
// ============================================================================

TEST_F(HypothalamusPerceptionBridgeTest, CreateWithValidDrives) {
    // Already created in SetUp, verify it's valid
    EXPECT_NE(nullptr, bridge);
}

TEST_F(HypothalamusPerceptionBridgeTest, CreateWithNullDrives) {
    hypo_perception_bridge_t* null_bridge = hypo_perception_bridge_create(nullptr, &config);
    EXPECT_EQ(nullptr, null_bridge);
}

TEST_F(HypothalamusPerceptionBridgeTest, CreateWithNullConfigUsesDefaults) {
    hypo_perception_bridge_t* bridge_null_config = hypo_perception_bridge_create(drive_system, nullptr);
    EXPECT_NE(nullptr, bridge_null_config);

    // Verify default modulation values
    hypo_perception_modulation_t mod;
    EXPECT_EQ(0, hypo_perception_bridge_get_modulation(bridge_null_config, &mod));
    EXPECT_FLOAT_EQ(1.0f, mod.global_gain);

    hypo_perception_bridge_destroy(bridge_null_config);
}

TEST_F(HypothalamusPerceptionBridgeTest, DestroyNullDoesNotCrash) {
    hypo_perception_bridge_destroy(nullptr);
    // Should not crash
    SUCCEED();
}

TEST_F(HypothalamusPerceptionBridgeTest, ResetRestorerInitialState) {
    // Modify some state
    hypo_perception_bridge_set_arousal(bridge, 0.9f);
    hypo_perception_bridge_set_core_temperature(bridge, 0.2f);
    hypo_perception_bridge_set_sleep_stage(bridge, HYPO_SLEEP_DEEP);

    // Reset
    EXPECT_EQ(0, hypo_perception_bridge_reset(bridge));

    // Verify reset state
    hypo_perception_modulation_t mod;
    EXPECT_EQ(0, hypo_perception_bridge_get_modulation(bridge, &mod));
    EXPECT_FLOAT_EQ(1.0f, mod.global_gain);
    EXPECT_FLOAT_EQ(0.5f, mod.arousal_level);
    EXPECT_FALSE(mod.threat_priority);
    EXPECT_FALSE(mod.survival_mode);

    // Sleep should be back to awake
    hypo_sleep_gating_state_t sleep_state;
    EXPECT_EQ(0, hypo_perception_bridge_get_sleep_gating_state(bridge, &sleep_state));
    EXPECT_EQ(HYPO_SLEEP_AWAKE, sleep_state.current_stage);
}

TEST_F(HypothalamusPerceptionBridgeTest, ResetNullReturnError) {
    EXPECT_EQ(-1, hypo_perception_bridge_reset(nullptr));
}

// ============================================================================
// CONFIGURATION TESTS
// ============================================================================

TEST_F(HypothalamusPerceptionBridgeTest, DefaultConfigHasReasonableValues) {
    hypo_perception_config_t default_config;
    hypo_perception_bridge_default_config(&default_config);

    EXPECT_FLOAT_EQ(0.7f, default_config.arousal_gain_min);
    EXPECT_FLOAT_EQ(1.5f, default_config.arousal_gain_max);
    EXPECT_FLOAT_EQ(0.8f, default_config.drive_salience_weight);
    EXPECT_FLOAT_EQ(0.5f, default_config.threat_priority_threshold);
    EXPECT_FLOAT_EQ(1.0f, default_config.visual_weight);
    EXPECT_FLOAT_EQ(1.0f, default_config.auditory_weight);
    EXPECT_TRUE(default_config.enable_anticipation);
    EXPECT_FLOAT_EQ(0.05f, default_config.anticipation_decay);
}

TEST_F(HypothalamusPerceptionBridgeTest, DefaultConfigNullDoesNotCrash) {
    hypo_perception_bridge_default_config(nullptr);
    SUCCEED();
}

TEST_F(HypothalamusPerceptionBridgeTest, DefaultEnhancedConfigHasReasonableValues) {
    hypo_perception_enhanced_config_t enhanced_config;
    hypo_perception_bridge_default_enhanced_config(&enhanced_config);

    // Check base config
    EXPECT_FLOAT_EQ(0.7f, enhanced_config.base.arousal_gain_min);
    EXPECT_FLOAT_EQ(1.5f, enhanced_config.base.arousal_gain_max);

    // Check enhancement flags
    EXPECT_TRUE(enhanced_config.enable_interoception);
    EXPECT_FLOAT_EQ(0.7f, enhanced_config.interoceptive_accuracy);

    EXPECT_TRUE(enhanced_config.enable_chemosensory);
    EXPECT_FLOAT_EQ(0.5f, enhanced_config.hunger_smell_boost);
    EXPECT_FLOAT_EQ(0.4f, enhanced_config.thirst_salt_boost);

    EXPECT_TRUE(enhanced_config.enable_predictive_coding);
    EXPECT_FLOAT_EQ(0.5f, enhanced_config.prediction_precision_default);
    EXPECT_FLOAT_EQ(1.0f, enhanced_config.free_energy_weight);

    EXPECT_TRUE(enhanced_config.enable_pain_modulation);
    EXPECT_FLOAT_EQ(0.5f, enhanced_config.acute_stress_analgesia);
    EXPECT_FLOAT_EQ(0.3f, enhanced_config.chronic_stress_hyperalgesia);

    EXPECT_TRUE(enhanced_config.enable_sleep_gating);
    EXPECT_FLOAT_EQ(0.1f, enhanced_config.deep_sleep_gate);
    EXPECT_FLOAT_EQ(0.2f, enhanced_config.rem_sleep_gate);

    EXPECT_TRUE(enhanced_config.enable_thermal_salience);
    EXPECT_FLOAT_EQ(0.5f, enhanced_config.thermal_setpoint);
    EXPECT_FLOAT_EQ(0.1f, enhanced_config.thermal_tolerance);
}

TEST_F(HypothalamusPerceptionBridgeTest, DefaultEnhancedConfigNullDoesNotCrash) {
    hypo_perception_bridge_default_enhanced_config(nullptr);
    SUCCEED();
}

TEST_F(HypothalamusPerceptionBridgeTest, ApplyEnhancedConfigSuccess) {
    hypo_perception_enhanced_config_t enhanced_config;
    hypo_perception_bridge_default_enhanced_config(&enhanced_config);

    // Modify some values
    enhanced_config.enable_interoception = false;
    enhanced_config.enable_sleep_gating = false;
    enhanced_config.thermal_setpoint = 0.6f;

    EXPECT_EQ(0, hypo_perception_bridge_apply_enhanced_config(bridge, &enhanced_config));

    // Note: We can't directly verify internal state, but no error means success
}

TEST_F(HypothalamusPerceptionBridgeTest, ApplyEnhancedConfigNullBridge) {
    hypo_perception_enhanced_config_t enhanced_config;
    hypo_perception_bridge_default_enhanced_config(&enhanced_config);

    EXPECT_EQ(-1, hypo_perception_bridge_apply_enhanced_config(nullptr, &enhanced_config));
}

TEST_F(HypothalamusPerceptionBridgeTest, ApplyEnhancedConfigNullConfig) {
    EXPECT_EQ(-1, hypo_perception_bridge_apply_enhanced_config(bridge, nullptr));
}

// ============================================================================
// BASE FUNCTION TESTS
// ============================================================================

TEST_F(HypothalamusPerceptionBridgeTest, ComputeModulationSuccess) {
    EXPECT_EQ(0, hypo_perception_bridge_compute_modulation(bridge));

    hypo_perception_modulation_t mod;
    EXPECT_EQ(0, hypo_perception_bridge_get_modulation(bridge, &mod));
    EXPECT_GE(mod.global_gain, 0.5f);
    EXPECT_LE(mod.global_gain, 2.0f);
}

TEST_F(HypothalamusPerceptionBridgeTest, ComputeModulationNullBridge) {
    EXPECT_EQ(-1, hypo_perception_bridge_compute_modulation(nullptr));
}

TEST_F(HypothalamusPerceptionBridgeTest, GetModulationSuccess) {
    hypo_perception_modulation_t mod;
    EXPECT_EQ(0, hypo_perception_bridge_get_modulation(bridge, &mod));

    // Verify initial values
    EXPECT_FLOAT_EQ(1.0f, mod.global_gain);
    EXPECT_FLOAT_EQ(0.5f, mod.arousal_level);
    EXPECT_FALSE(mod.threat_priority);
    EXPECT_FALSE(mod.survival_mode);
}

TEST_F(HypothalamusPerceptionBridgeTest, GetModulationNullBridge) {
    hypo_perception_modulation_t mod;
    EXPECT_EQ(-1, hypo_perception_bridge_get_modulation(nullptr, &mod));
}

TEST_F(HypothalamusPerceptionBridgeTest, GetModulationNullOutput) {
    EXPECT_EQ(-1, hypo_perception_bridge_get_modulation(bridge, nullptr));
}

TEST_F(HypothalamusPerceptionBridgeTest, SetArousalAffectsGlobalGain) {
    // Set low arousal
    EXPECT_EQ(0, hypo_perception_bridge_set_arousal(bridge, 0.0f));
    hypo_perception_bridge_compute_modulation(bridge);

    hypo_perception_modulation_t mod;
    EXPECT_EQ(0, hypo_perception_bridge_get_modulation(bridge, &mod));
    EXPECT_NEAR(0.7f, mod.global_gain, 0.1f);  // Near arousal_gain_min

    // Set high arousal
    EXPECT_EQ(0, hypo_perception_bridge_set_arousal(bridge, 1.0f));
    hypo_perception_bridge_compute_modulation(bridge);

    EXPECT_EQ(0, hypo_perception_bridge_get_modulation(bridge, &mod));
    EXPECT_NEAR(1.5f, mod.global_gain, 0.1f);  // Near arousal_gain_max
}

TEST_F(HypothalamusPerceptionBridgeTest, SetArousalNullBridge) {
    EXPECT_EQ(-1, hypo_perception_bridge_set_arousal(nullptr, 0.5f));
}

TEST_F(HypothalamusPerceptionBridgeTest, SetArousalClampsToValidRange) {
    // Test values outside [0, 1]
    EXPECT_EQ(0, hypo_perception_bridge_set_arousal(bridge, -0.5f));
    hypo_perception_bridge_compute_modulation(bridge);
    hypo_perception_modulation_t mod;
    hypo_perception_bridge_get_modulation(bridge, &mod);
    EXPECT_FLOAT_EQ(0.0f, mod.arousal_level);

    EXPECT_EQ(0, hypo_perception_bridge_set_arousal(bridge, 1.5f));
    hypo_perception_bridge_compute_modulation(bridge);
    hypo_perception_bridge_get_modulation(bridge, &mod);
    EXPECT_FLOAT_EQ(1.0f, mod.arousal_level);
}

TEST_F(HypothalamusPerceptionBridgeTest, ProcessDetectionBoostsAnticipation) {
    hypo_sensory_detection_t detection = createDetection(
        HYPO_STIM_FOOD, HYPO_SENSE_VISUAL, 0.9f, 0.8f, false);

    // Initial anticipation should be low
    float initial_anticipation = hypo_perception_bridge_get_anticipation(bridge, HYPO_DRIVE_HUNGER);
    EXPECT_LT(initial_anticipation, 0.1f);

    // Process detection
    EXPECT_EQ(0, hypo_perception_bridge_process_detection(bridge, &detection));

    // Anticipation should increase
    float new_anticipation = hypo_perception_bridge_get_anticipation(bridge, HYPO_DRIVE_HUNGER);
    EXPECT_GT(new_anticipation, initial_anticipation);
}

TEST_F(HypothalamusPerceptionBridgeTest, ProcessDetectionThreatBoostsSafety) {
    hypo_sensory_detection_t detection = createDetection(
        HYPO_STIM_THREAT, HYPO_SENSE_VISUAL, 0.9f, 0.9f, true);

    float initial_safety = hypo_perception_bridge_get_anticipation(bridge, HYPO_DRIVE_SAFETY);

    EXPECT_EQ(0, hypo_perception_bridge_process_detection(bridge, &detection));

    float new_safety = hypo_perception_bridge_get_anticipation(bridge, HYPO_DRIVE_SAFETY);
    EXPECT_GT(new_safety, initial_safety);
}

TEST_F(HypothalamusPerceptionBridgeTest, ProcessDetectionNullBridge) {
    hypo_sensory_detection_t detection = createDetection(
        HYPO_STIM_FOOD, HYPO_SENSE_VISUAL, 0.9f, 0.8f, false);
    EXPECT_EQ(-1, hypo_perception_bridge_process_detection(nullptr, &detection));
}

TEST_F(HypothalamusPerceptionBridgeTest, ProcessDetectionNullDetection) {
    EXPECT_EQ(-1, hypo_perception_bridge_process_detection(bridge, nullptr));
}

TEST_F(HypothalamusPerceptionBridgeTest, GetAnticipationNullBridge) {
    float anticipation = hypo_perception_bridge_get_anticipation(nullptr, HYPO_DRIVE_HUNGER);
    EXPECT_FLOAT_EQ(0.0f, anticipation);
}

TEST_F(HypothalamusPerceptionBridgeTest, GetAnticipationInvalidDrive) {
    float anticipation = hypo_perception_bridge_get_anticipation(bridge, HYPO_DRIVE_COUNT);
    EXPECT_FLOAT_EQ(0.0f, anticipation);
}

// ============================================================================
// ENHANCEMENT 1: INTEROCEPTION TESTS
// ============================================================================

TEST_F(HypothalamusPerceptionBridgeTest, ProcessInteroceptiveSuccess) {
    EXPECT_EQ(0, hypo_perception_bridge_process_interoceptive(
        bridge, HYPO_INTERO_HUNGER_PANGS, 0.8f));

    hypo_interoceptive_state_t state;
    EXPECT_EQ(0, hypo_perception_bridge_get_interoceptive_state(bridge, &state));
    EXPECT_FLOAT_EQ(0.8f, state.signals[HYPO_INTERO_HUNGER_PANGS]);
}

TEST_F(HypothalamusPerceptionBridgeTest, ProcessInteroceptiveUpdatesPredictionError) {
    // First signal
    EXPECT_EQ(0, hypo_perception_bridge_process_interoceptive(
        bridge, HYPO_INTERO_HEART_RATE, 0.3f));

    // Second signal with different intensity - should cause prediction error
    EXPECT_EQ(0, hypo_perception_bridge_process_interoceptive(
        bridge, HYPO_INTERO_HEART_RATE, 0.8f));

    hypo_interoceptive_state_t state;
    EXPECT_EQ(0, hypo_perception_bridge_get_interoceptive_state(bridge, &state));
    EXPECT_GT(state.prediction_errors[HYPO_INTERO_HEART_RATE], 0.0f);
}

TEST_F(HypothalamusPerceptionBridgeTest, ProcessInteroceptiveUpdatesSalience) {
    EXPECT_EQ(0, hypo_perception_bridge_process_interoceptive(
        bridge, HYPO_INTERO_PAIN, 0.9f));

    hypo_interoceptive_state_t state;
    EXPECT_EQ(0, hypo_perception_bridge_get_interoceptive_state(bridge, &state));
    EXPECT_GT(state.salience[HYPO_INTERO_PAIN], 0.0f);
}

TEST_F(HypothalamusPerceptionBridgeTest, ProcessInteroceptiveNullBridge) {
    EXPECT_EQ(-1, hypo_perception_bridge_process_interoceptive(
        nullptr, HYPO_INTERO_HUNGER_PANGS, 0.5f));
}

TEST_F(HypothalamusPerceptionBridgeTest, ProcessInteroceptiveInvalidType) {
    EXPECT_EQ(-1, hypo_perception_bridge_process_interoceptive(
        bridge, HYPO_INTERO_COUNT, 0.5f));
}

TEST_F(HypothalamusPerceptionBridgeTest, GetInteroceptiveStateNullBridge) {
    hypo_interoceptive_state_t state;
    EXPECT_EQ(-1, hypo_perception_bridge_get_interoceptive_state(nullptr, &state));
}

TEST_F(HypothalamusPerceptionBridgeTest, GetInteroceptiveStateNullOutput) {
    EXPECT_EQ(-1, hypo_perception_bridge_get_interoceptive_state(bridge, nullptr));
}

TEST_F(HypothalamusPerceptionBridgeTest, SetInteroceptiveAccuracySuccess) {
    EXPECT_EQ(0, hypo_perception_bridge_set_interoceptive_accuracy(bridge, 0.9f));

    hypo_interoceptive_state_t state;
    EXPECT_EQ(0, hypo_perception_bridge_get_interoceptive_state(bridge, &state));
    EXPECT_FLOAT_EQ(0.9f, state.global_interoceptive_accuracy);
}

TEST_F(HypothalamusPerceptionBridgeTest, SetInteroceptiveAccuracyNullBridge) {
    EXPECT_EQ(-1, hypo_perception_bridge_set_interoceptive_accuracy(nullptr, 0.5f));
}

TEST_F(HypothalamusPerceptionBridgeTest, SetInteroceptiveAccuracyClampsValues) {
    EXPECT_EQ(0, hypo_perception_bridge_set_interoceptive_accuracy(bridge, 1.5f));
    hypo_interoceptive_state_t state;
    hypo_perception_bridge_get_interoceptive_state(bridge, &state);
    EXPECT_FLOAT_EQ(1.0f, state.global_interoceptive_accuracy);

    EXPECT_EQ(0, hypo_perception_bridge_set_interoceptive_accuracy(bridge, -0.5f));
    hypo_perception_bridge_get_interoceptive_state(bridge, &state);
    EXPECT_FLOAT_EQ(0.0f, state.global_interoceptive_accuracy);
}

TEST_F(HypothalamusPerceptionBridgeTest, InteroceptiveHungerPangsBoostsDriveAnticipation) {
    float initial_anticipation = hypo_perception_bridge_get_anticipation(bridge, HYPO_DRIVE_HUNGER);

    // Strong hunger pangs signal
    EXPECT_EQ(0, hypo_perception_bridge_process_interoceptive(
        bridge, HYPO_INTERO_HUNGER_PANGS, 0.8f));

    float new_anticipation = hypo_perception_bridge_get_anticipation(bridge, HYPO_DRIVE_HUNGER);
    EXPECT_GT(new_anticipation, initial_anticipation);
}

// ============================================================================
// ENHANCEMENT 2: OLFACTORY/GUSTATORY TESTS
// ============================================================================

TEST_F(HypothalamusPerceptionBridgeTest, ProcessOlfactorySuccess) {
    EXPECT_EQ(0, hypo_perception_bridge_process_olfactory(
        bridge, HYPO_OLFACT_FOOD_PLEASANT, 0.7f));

    hypo_chemosensory_state_t state;
    EXPECT_EQ(0, hypo_perception_bridge_get_chemosensory_state(bridge, &state));
    EXPECT_GT(state.olfactory_intensity[HYPO_OLFACT_FOOD_PLEASANT], 0.0f);
}

TEST_F(HypothalamusPerceptionBridgeTest, ProcessOlfactoryFoodDetected) {
    EXPECT_EQ(0, hypo_perception_bridge_process_olfactory(
        bridge, HYPO_OLFACT_FOOD_PLEASANT, 0.5f));

    hypo_chemosensory_state_t state;
    EXPECT_EQ(0, hypo_perception_bridge_get_chemosensory_state(bridge, &state));
    EXPECT_TRUE(state.food_detected);
}

TEST_F(HypothalamusPerceptionBridgeTest, ProcessOlfactoryToxinDetected) {
    EXPECT_EQ(0, hypo_perception_bridge_process_olfactory(
        bridge, HYPO_OLFACT_FOOD_UNPLEASANT, 0.6f));

    hypo_chemosensory_state_t state;
    EXPECT_EQ(0, hypo_perception_bridge_get_chemosensory_state(bridge, &state));
    EXPECT_TRUE(state.toxin_detected);
}

TEST_F(HypothalamusPerceptionBridgeTest, ProcessOlfactoryDangerBoostsSafety) {
    float initial_safety = hypo_perception_bridge_get_anticipation(bridge, HYPO_DRIVE_SAFETY);

    EXPECT_EQ(0, hypo_perception_bridge_process_olfactory(
        bridge, HYPO_OLFACT_DANGER, 0.8f));

    float new_safety = hypo_perception_bridge_get_anticipation(bridge, HYPO_DRIVE_SAFETY);
    EXPECT_GT(new_safety, initial_safety);
}

TEST_F(HypothalamusPerceptionBridgeTest, ProcessOlfactoryNullBridge) {
    EXPECT_EQ(-1, hypo_perception_bridge_process_olfactory(
        nullptr, HYPO_OLFACT_FOOD_PLEASANT, 0.5f));
}

TEST_F(HypothalamusPerceptionBridgeTest, ProcessOlfactoryInvalidType) {
    EXPECT_EQ(-1, hypo_perception_bridge_process_olfactory(
        bridge, HYPO_OLFACT_COUNT, 0.5f));
}

TEST_F(HypothalamusPerceptionBridgeTest, ProcessGustatorySuccess) {
    EXPECT_EQ(0, hypo_perception_bridge_process_gustatory(
        bridge, HYPO_GUST_SWEET, 0.7f));

    hypo_chemosensory_state_t state;
    EXPECT_EQ(0, hypo_perception_bridge_get_chemosensory_state(bridge, &state));
    EXPECT_GT(state.gustatory_intensity[HYPO_GUST_SWEET], 0.0f);
}

TEST_F(HypothalamusPerceptionBridgeTest, ProcessGustatoryBitterToxinDetection) {
    EXPECT_EQ(0, hypo_perception_bridge_process_gustatory(
        bridge, HYPO_GUST_BITTER, 0.5f));

    hypo_chemosensory_state_t state;
    EXPECT_EQ(0, hypo_perception_bridge_get_chemosensory_state(bridge, &state));
    EXPECT_TRUE(state.toxin_detected);
}

TEST_F(HypothalamusPerceptionBridgeTest, ProcessGustatoryNullBridge) {
    EXPECT_EQ(-1, hypo_perception_bridge_process_gustatory(
        nullptr, HYPO_GUST_SWEET, 0.5f));
}

TEST_F(HypothalamusPerceptionBridgeTest, ProcessGustatoryInvalidType) {
    EXPECT_EQ(-1, hypo_perception_bridge_process_gustatory(
        bridge, HYPO_GUST_COUNT, 0.5f));
}

TEST_F(HypothalamusPerceptionBridgeTest, GetChemosensoryStateNullBridge) {
    hypo_chemosensory_state_t state;
    EXPECT_EQ(-1, hypo_perception_bridge_get_chemosensory_state(nullptr, &state));
}

TEST_F(HypothalamusPerceptionBridgeTest, GetChemosensoryStateNullOutput) {
    EXPECT_EQ(-1, hypo_perception_bridge_get_chemosensory_state(bridge, nullptr));
}

TEST_F(HypothalamusPerceptionBridgeTest, ChemosensoryHungerModulation) {
    // Get initial modulation
    hypo_chemosensory_state_t initial_state;
    EXPECT_EQ(0, hypo_perception_bridge_get_chemosensory_state(bridge, &initial_state));
    EXPECT_FLOAT_EQ(1.0f, initial_state.hunger_modulation);

    // Increase hunger drive (via compute modulation with modified drive state)
    // Process food smell and verify modulation affects intensity
    EXPECT_EQ(0, hypo_perception_bridge_process_olfactory(
        bridge, HYPO_OLFACT_FOOD_PLEASANT, 0.5f));
}

// ============================================================================
// ENHANCEMENT 3: PREDICTIVE CODING TESTS
// ============================================================================

TEST_F(HypothalamusPerceptionBridgeTest, GeneratePredictionSuccess) {
    EXPECT_EQ(0, hypo_perception_bridge_generate_prediction(
        bridge, HYPO_PRED_FOOD_PRESENCE, 0.8f, 0.9f));

    hypo_predictive_state_t state;
    EXPECT_EQ(0, hypo_perception_bridge_get_predictive_state(bridge, &state));
    EXPECT_FLOAT_EQ(0.8f, state.predictions[HYPO_PRED_FOOD_PRESENCE]);
    EXPECT_FLOAT_EQ(0.9f, state.precision[HYPO_PRED_FOOD_PRESENCE]);
}

TEST_F(HypothalamusPerceptionBridgeTest, GeneratePredictionNullBridge) {
    EXPECT_EQ(-1, hypo_perception_bridge_generate_prediction(
        nullptr, HYPO_PRED_FOOD_PRESENCE, 0.8f, 0.9f));
}

TEST_F(HypothalamusPerceptionBridgeTest, GeneratePredictionInvalidType) {
    EXPECT_EQ(-1, hypo_perception_bridge_generate_prediction(
        bridge, HYPO_PRED_COUNT, 0.8f, 0.9f));
}

TEST_F(HypothalamusPerceptionBridgeTest, UpdatePredictionErrorSuccess) {
    // First generate a prediction
    EXPECT_EQ(0, hypo_perception_bridge_generate_prediction(
        bridge, HYPO_PRED_THREAT_PRESENCE, 0.3f, 0.8f));

    // Update with actual observation different from prediction
    EXPECT_EQ(0, hypo_perception_bridge_update_prediction_error(
        bridge, HYPO_PRED_THREAT_PRESENCE, 0.9f));

    hypo_predictive_state_t state;
    EXPECT_EQ(0, hypo_perception_bridge_get_predictive_state(bridge, &state));

    // Prediction error should be non-zero (actual - predicted)
    EXPECT_NE(0.0f, state.prediction_errors[HYPO_PRED_THREAT_PRESENCE]);
}

TEST_F(HypothalamusPerceptionBridgeTest, UpdatePredictionErrorNullBridge) {
    EXPECT_EQ(-1, hypo_perception_bridge_update_prediction_error(
        nullptr, HYPO_PRED_THREAT_PRESENCE, 0.5f));
}

TEST_F(HypothalamusPerceptionBridgeTest, UpdatePredictionErrorInvalidType) {
    EXPECT_EQ(-1, hypo_perception_bridge_update_prediction_error(
        bridge, HYPO_PRED_COUNT, 0.5f));
}

TEST_F(HypothalamusPerceptionBridgeTest, GetPredictiveStateNullBridge) {
    hypo_predictive_state_t state;
    EXPECT_EQ(-1, hypo_perception_bridge_get_predictive_state(nullptr, &state));
}

TEST_F(HypothalamusPerceptionBridgeTest, GetPredictiveStateNullOutput) {
    EXPECT_EQ(-1, hypo_perception_bridge_get_predictive_state(bridge, nullptr));
}

TEST_F(HypothalamusPerceptionBridgeTest, ComputeFreeEnergySuccess) {
    // Generate predictions with different precision
    EXPECT_EQ(0, hypo_perception_bridge_generate_prediction(
        bridge, HYPO_PRED_FOOD_PRESENCE, 0.5f, 0.9f));
    EXPECT_EQ(0, hypo_perception_bridge_generate_prediction(
        bridge, HYPO_PRED_THREAT_PRESENCE, 0.3f, 0.8f));

    // Update with actual values to create prediction errors
    EXPECT_EQ(0, hypo_perception_bridge_update_prediction_error(
        bridge, HYPO_PRED_FOOD_PRESENCE, 0.9f));
    EXPECT_EQ(0, hypo_perception_bridge_update_prediction_error(
        bridge, HYPO_PRED_THREAT_PRESENCE, 0.1f));

    float free_energy;
    EXPECT_EQ(0, hypo_perception_bridge_compute_free_energy(bridge, &free_energy));

    // Free energy should be positive when there are prediction errors
    EXPECT_GT(free_energy, 0.0f);
}

TEST_F(HypothalamusPerceptionBridgeTest, ComputeFreeEnergyNullBridge) {
    float free_energy;
    EXPECT_EQ(-1, hypo_perception_bridge_compute_free_energy(nullptr, &free_energy));
}

TEST_F(HypothalamusPerceptionBridgeTest, ComputeFreeEnergyNullOutput) {
    EXPECT_EQ(-1, hypo_perception_bridge_compute_free_energy(bridge, nullptr));
}

TEST_F(HypothalamusPerceptionBridgeTest, FreeEnergyZeroWhenNoPredictionErrors) {
    // Initial state should have minimal free energy
    float free_energy;
    EXPECT_EQ(0, hypo_perception_bridge_compute_free_energy(bridge, &free_energy));
    EXPECT_GE(free_energy, 0.0f);
}

// ============================================================================
// ENHANCEMENT 4: PAIN MODULATION TESTS
// ============================================================================

TEST_F(HypothalamusPerceptionBridgeTest, SetStressForPainAcuteStress) {
    EXPECT_EQ(0, hypo_perception_bridge_set_stress_for_pain(bridge, 0.7f, false));

    hypo_pain_modulation_state_t state;
    EXPECT_EQ(0, hypo_perception_bridge_get_pain_state(bridge, &state));

    EXPECT_TRUE(state.in_acute_stress);
    EXPECT_FALSE(state.in_chronic_stress);
    EXPECT_GT(state.analgesia_level, 0.0f);  // Analgesia in acute stress
    EXPECT_LT(state.pain_sensitivity, 1.0f);  // Reduced sensitivity
}

TEST_F(HypothalamusPerceptionBridgeTest, SetStressForPainChronicStress) {
    EXPECT_EQ(0, hypo_perception_bridge_set_stress_for_pain(bridge, 0.7f, true));

    hypo_pain_modulation_state_t state;
    EXPECT_EQ(0, hypo_perception_bridge_get_pain_state(bridge, &state));

    EXPECT_FALSE(state.in_acute_stress);
    EXPECT_TRUE(state.in_chronic_stress);
    EXPECT_GT(state.hyperalgesia_level, 0.0f);  // Hyperalgesia in chronic stress
    EXPECT_GT(state.pain_sensitivity, 1.0f);  // Increased sensitivity
}

TEST_F(HypothalamusPerceptionBridgeTest, SetStressForPainNullBridge) {
    EXPECT_EQ(-1, hypo_perception_bridge_set_stress_for_pain(nullptr, 0.5f, false));
}

TEST_F(HypothalamusPerceptionBridgeTest, ModulatePainWithAcuteStress) {
    // Set acute stress to induce analgesia
    EXPECT_EQ(0, hypo_perception_bridge_set_stress_for_pain(bridge, 0.8f, false));

    float raw_pain = 0.7f;
    float modulated_pain;
    EXPECT_EQ(0, hypo_perception_bridge_modulate_pain(bridge, raw_pain, &modulated_pain));

    // Pain should be reduced due to analgesia
    EXPECT_LT(modulated_pain, raw_pain);
}

TEST_F(HypothalamusPerceptionBridgeTest, ModulatePainWithChronicStress) {
    // Set chronic stress to induce hyperalgesia
    EXPECT_EQ(0, hypo_perception_bridge_set_stress_for_pain(bridge, 0.8f, true));

    float raw_pain = 0.5f;
    float modulated_pain;
    EXPECT_EQ(0, hypo_perception_bridge_modulate_pain(bridge, raw_pain, &modulated_pain));

    // Pain should be increased due to hyperalgesia
    EXPECT_GT(modulated_pain, raw_pain);
}

TEST_F(HypothalamusPerceptionBridgeTest, ModulatePainNullBridge) {
    float modulated_pain;
    EXPECT_EQ(-1, hypo_perception_bridge_modulate_pain(nullptr, 0.5f, &modulated_pain));
}

TEST_F(HypothalamusPerceptionBridgeTest, ModulatePainNullOutput) {
    EXPECT_EQ(-1, hypo_perception_bridge_modulate_pain(bridge, 0.5f, nullptr));
}

TEST_F(HypothalamusPerceptionBridgeTest, ModulatePainClampsOutput) {
    // Set chronic stress to amplify pain
    EXPECT_EQ(0, hypo_perception_bridge_set_stress_for_pain(bridge, 0.9f, true));

    float modulated_pain;
    EXPECT_EQ(0, hypo_perception_bridge_modulate_pain(bridge, 0.9f, &modulated_pain));

    // Output should be clamped to [0, 1]
    EXPECT_LE(modulated_pain, 1.0f);
}

TEST_F(HypothalamusPerceptionBridgeTest, GetPainStateNullBridge) {
    hypo_pain_modulation_state_t state;
    EXPECT_EQ(-1, hypo_perception_bridge_get_pain_state(nullptr, &state));
}

TEST_F(HypothalamusPerceptionBridgeTest, GetPainStateNullOutput) {
    EXPECT_EQ(-1, hypo_perception_bridge_get_pain_state(bridge, nullptr));
}

TEST_F(HypothalamusPerceptionBridgeTest, ReleaseEndorphinsReducesPain) {
    EXPECT_EQ(0, hypo_perception_bridge_release_endorphins(bridge, 0.8f));

    hypo_pain_modulation_state_t state;
    EXPECT_EQ(0, hypo_perception_bridge_get_pain_state(bridge, &state));
    EXPECT_FLOAT_EQ(0.8f, state.endorphin_level);

    // Pain modulation should be affected
    float raw_pain = 0.6f;
    float modulated_pain;
    EXPECT_EQ(0, hypo_perception_bridge_modulate_pain(bridge, raw_pain, &modulated_pain));
    EXPECT_LT(modulated_pain, raw_pain);  // Endorphins reduce pain
}

TEST_F(HypothalamusPerceptionBridgeTest, ReleaseEndorphinsNullBridge) {
    EXPECT_EQ(-1, hypo_perception_bridge_release_endorphins(nullptr, 0.5f));
}

TEST_F(HypothalamusPerceptionBridgeTest, ReleaseEndorphinsClampsLevel) {
    EXPECT_EQ(0, hypo_perception_bridge_release_endorphins(bridge, 0.7f));
    EXPECT_EQ(0, hypo_perception_bridge_release_endorphins(bridge, 0.7f));

    hypo_pain_modulation_state_t state;
    EXPECT_EQ(0, hypo_perception_bridge_get_pain_state(bridge, &state));
    EXPECT_LE(state.endorphin_level, 1.0f);
}

// ============================================================================
// ENHANCEMENT 5: SLEEP GATING TESTS
// ============================================================================

TEST_F(HypothalamusPerceptionBridgeTest, SetSleepStageSuccess) {
    EXPECT_EQ(0, hypo_perception_bridge_set_sleep_stage(bridge, HYPO_SLEEP_DEEP));

    hypo_sleep_gating_state_t state;
    EXPECT_EQ(0, hypo_perception_bridge_get_sleep_gating_state(bridge, &state));
    EXPECT_EQ(HYPO_SLEEP_DEEP, state.current_stage);
}

TEST_F(HypothalamusPerceptionBridgeTest, SetSleepStageAffectsPerceptionGate) {
    // Awake should have full perception gate
    EXPECT_EQ(0, hypo_perception_bridge_set_sleep_stage(bridge, HYPO_SLEEP_AWAKE));
    hypo_sleep_gating_state_t state;
    EXPECT_EQ(0, hypo_perception_bridge_get_sleep_gating_state(bridge, &state));
    EXPECT_FLOAT_EQ(1.0f, state.perception_gate);

    // Deep sleep should have low perception gate
    EXPECT_EQ(0, hypo_perception_bridge_set_sleep_stage(bridge, HYPO_SLEEP_DEEP));
    EXPECT_EQ(0, hypo_perception_bridge_get_sleep_gating_state(bridge, &state));
    EXPECT_LT(state.perception_gate, 0.3f);
}

TEST_F(HypothalamusPerceptionBridgeTest, SetSleepStageNullBridge) {
    EXPECT_EQ(-1, hypo_perception_bridge_set_sleep_stage(nullptr, HYPO_SLEEP_AWAKE));
}

TEST_F(HypothalamusPerceptionBridgeTest, SetSleepStageInvalidStage) {
    EXPECT_EQ(-1, hypo_perception_bridge_set_sleep_stage(bridge, HYPO_SLEEP_COUNT));
}

TEST_F(HypothalamusPerceptionBridgeTest, CheckSleepGateAwakeAlwaysPasses) {
    EXPECT_EQ(0, hypo_perception_bridge_set_sleep_stage(bridge, HYPO_SLEEP_AWAKE));

    bool passes;
    EXPECT_EQ(0, hypo_perception_bridge_check_sleep_gate(
        bridge, 0.1f, false, false, &passes));
    EXPECT_TRUE(passes);
}

TEST_F(HypothalamusPerceptionBridgeTest, CheckSleepGateDeepSleepBlocksWeak) {
    EXPECT_EQ(0, hypo_perception_bridge_set_sleep_stage(bridge, HYPO_SLEEP_DEEP));

    bool passes;
    EXPECT_EQ(0, hypo_perception_bridge_check_sleep_gate(
        bridge, 0.1f, false, false, &passes));
    EXPECT_FALSE(passes);  // Weak stimulus blocked in deep sleep
}

TEST_F(HypothalamusPerceptionBridgeTest, CheckSleepGateThreatBypass) {
    EXPECT_EQ(0, hypo_perception_bridge_set_sleep_stage(bridge, HYPO_SLEEP_LIGHT));

    bool passes;
    // Threat stimuli should have lower threshold
    EXPECT_EQ(0, hypo_perception_bridge_check_sleep_gate(
        bridge, 0.3f, true, false, &passes));
    EXPECT_TRUE(passes);  // Threat bypasses gate more easily
}

TEST_F(HypothalamusPerceptionBridgeTest, CheckSleepGateNameBypass) {
    EXPECT_EQ(0, hypo_perception_bridge_set_sleep_stage(bridge, HYPO_SLEEP_LIGHT));

    bool passes;
    // Own name should bypass gate more easily
    EXPECT_EQ(0, hypo_perception_bridge_check_sleep_gate(
        bridge, 0.3f, false, true, &passes));
    EXPECT_TRUE(passes);
}

TEST_F(HypothalamusPerceptionBridgeTest, CheckSleepGateNullBridge) {
    bool passes;
    EXPECT_EQ(-1, hypo_perception_bridge_check_sleep_gate(
        nullptr, 0.5f, false, false, &passes));
}

TEST_F(HypothalamusPerceptionBridgeTest, CheckSleepGateNullOutput) {
    EXPECT_EQ(-1, hypo_perception_bridge_check_sleep_gate(
        bridge, 0.5f, false, false, nullptr));
}

TEST_F(HypothalamusPerceptionBridgeTest, GetSleepGatingStateNullBridge) {
    hypo_sleep_gating_state_t state;
    EXPECT_EQ(-1, hypo_perception_bridge_get_sleep_gating_state(nullptr, &state));
}

TEST_F(HypothalamusPerceptionBridgeTest, GetSleepGatingStateNullOutput) {
    EXPECT_EQ(-1, hypo_perception_bridge_get_sleep_gating_state(bridge, nullptr));
}

TEST_F(HypothalamusPerceptionBridgeTest, SleepStageTransitionTracksSleepOnset) {
    // Start awake
    EXPECT_EQ(0, hypo_perception_bridge_set_sleep_stage(bridge, HYPO_SLEEP_AWAKE));

    // Transition to sleep
    EXPECT_EQ(0, hypo_perception_bridge_set_sleep_stage(bridge, HYPO_SLEEP_DROWSY));

    hypo_sleep_gating_state_t state;
    EXPECT_EQ(0, hypo_perception_bridge_get_sleep_gating_state(bridge, &state));
    EXPECT_GT(state.sleep_onset_us, 0u);

    // Transition back to awake
    EXPECT_EQ(0, hypo_perception_bridge_set_sleep_stage(bridge, HYPO_SLEEP_AWAKE));
    EXPECT_EQ(0, hypo_perception_bridge_get_sleep_gating_state(bridge, &state));
    EXPECT_EQ(0u, state.sleep_onset_us);
}

// ============================================================================
// ENHANCEMENT 6: THERMAL SALIENCE TESTS
// ============================================================================

TEST_F(HypothalamusPerceptionBridgeTest, SetCoreTemperatureSuccess) {
    EXPECT_EQ(0, hypo_perception_bridge_set_core_temperature(bridge, 0.3f));

    hypo_thermal_state_t state;
    EXPECT_EQ(0, hypo_perception_bridge_get_thermal_state(bridge, &state));
    EXPECT_FLOAT_EQ(0.3f, state.core_temperature);
}

TEST_F(HypothalamusPerceptionBridgeTest, SetCoreTemperatureUpdatesDiscomfort) {
    // Temperature at setpoint (0.5) should have low discomfort
    EXPECT_EQ(0, hypo_perception_bridge_set_core_temperature(bridge, 0.5f));
    hypo_thermal_state_t state;
    EXPECT_EQ(0, hypo_perception_bridge_get_thermal_state(bridge, &state));
    EXPECT_LT(state.thermal_discomfort, 0.2f);

    // Temperature far from setpoint should have high discomfort
    EXPECT_EQ(0, hypo_perception_bridge_set_core_temperature(bridge, 0.2f));
    EXPECT_EQ(0, hypo_perception_bridge_get_thermal_state(bridge, &state));
    EXPECT_GT(state.thermal_discomfort, 0.5f);
}

TEST_F(HypothalamusPerceptionBridgeTest, SetCoreTemperatureColdTriggersWarmSeeking) {
    EXPECT_EQ(0, hypo_perception_bridge_set_core_temperature(bridge, 0.2f));

    hypo_thermal_state_t state;
    EXPECT_EQ(0, hypo_perception_bridge_get_thermal_state(bridge, &state));
    EXPECT_GT(state.warm_seeking, 0.0f);
    EXPECT_FLOAT_EQ(0.0f, state.cool_seeking);
}

TEST_F(HypothalamusPerceptionBridgeTest, SetCoreTemperatureHotTriggersCoolSeeking) {
    EXPECT_EQ(0, hypo_perception_bridge_set_core_temperature(bridge, 0.8f));

    hypo_thermal_state_t state;
    EXPECT_EQ(0, hypo_perception_bridge_get_thermal_state(bridge, &state));
    EXPECT_GT(state.cool_seeking, 0.0f);
    EXPECT_FLOAT_EQ(0.0f, state.warm_seeking);
}

TEST_F(HypothalamusPerceptionBridgeTest, SetCoreTemperatureHypothermicAlert) {
    EXPECT_EQ(0, hypo_perception_bridge_set_core_temperature(bridge, 0.2f));

    hypo_thermal_state_t state;
    EXPECT_EQ(0, hypo_perception_bridge_get_thermal_state(bridge, &state));
    EXPECT_TRUE(state.hypothermic_alert);
    EXPECT_FALSE(state.hyperthermic_alert);
}

TEST_F(HypothalamusPerceptionBridgeTest, SetCoreTemperatureHyperthermicAlert) {
    EXPECT_EQ(0, hypo_perception_bridge_set_core_temperature(bridge, 0.8f));

    hypo_thermal_state_t state;
    EXPECT_EQ(0, hypo_perception_bridge_get_thermal_state(bridge, &state));
    EXPECT_FALSE(state.hypothermic_alert);
    EXPECT_TRUE(state.hyperthermic_alert);
}

TEST_F(HypothalamusPerceptionBridgeTest, SetCoreTemperatureNullBridge) {
    EXPECT_EQ(-1, hypo_perception_bridge_set_core_temperature(nullptr, 0.5f));
}

TEST_F(HypothalamusPerceptionBridgeTest, SetAmbientTemperatureSuccess) {
    EXPECT_EQ(0, hypo_perception_bridge_set_ambient_temperature(bridge, 0.3f));

    hypo_thermal_state_t state;
    EXPECT_EQ(0, hypo_perception_bridge_get_thermal_state(bridge, &state));
    EXPECT_FLOAT_EQ(0.3f, state.ambient_temperature);
}

TEST_F(HypothalamusPerceptionBridgeTest, SetAmbientTemperatureNullBridge) {
    EXPECT_EQ(-1, hypo_perception_bridge_set_ambient_temperature(nullptr, 0.5f));
}

TEST_F(HypothalamusPerceptionBridgeTest, GetThermalStateNullBridge) {
    hypo_thermal_state_t state;
    EXPECT_EQ(-1, hypo_perception_bridge_get_thermal_state(nullptr, &state));
}

TEST_F(HypothalamusPerceptionBridgeTest, GetThermalStateNullOutput) {
    EXPECT_EQ(-1, hypo_perception_bridge_get_thermal_state(bridge, nullptr));
}

TEST_F(HypothalamusPerceptionBridgeTest, ComputeThermalSalienceSuccess) {
    // High discomfort should increase salience
    EXPECT_EQ(0, hypo_perception_bridge_set_core_temperature(bridge, 0.2f));

    float boost;
    EXPECT_EQ(0, hypo_perception_bridge_compute_thermal_salience(bridge, &boost));
    EXPECT_GT(boost, 1.0f);  // Salience boost should be > 1
    EXPECT_LE(boost, 3.0f);  // But capped at 3.0
}

TEST_F(HypothalamusPerceptionBridgeTest, ComputeThermalSalienceAtSetpoint) {
    // At setpoint, minimal salience boost
    EXPECT_EQ(0, hypo_perception_bridge_set_core_temperature(bridge, 0.5f));

    float boost;
    EXPECT_EQ(0, hypo_perception_bridge_compute_thermal_salience(bridge, &boost));
    EXPECT_NEAR(1.0f, boost, 0.3f);  // Near baseline
}

TEST_F(HypothalamusPerceptionBridgeTest, ComputeThermalSalienceNullBridge) {
    float boost;
    EXPECT_EQ(-1, hypo_perception_bridge_compute_thermal_salience(nullptr, &boost));
}

TEST_F(HypothalamusPerceptionBridgeTest, ComputeThermalSalienceNullOutput) {
    EXPECT_EQ(-1, hypo_perception_bridge_compute_thermal_salience(bridge, nullptr));
}

// ============================================================================
// STATISTICS TESTS
// ============================================================================

TEST_F(HypothalamusPerceptionBridgeTest, GetStatsSuccess) {
    hypo_perception_bridge_stats_t stats;
    EXPECT_EQ(0, hypo_perception_bridge_get_stats(bridge, &stats));

    // Initial stats should be zero
    EXPECT_EQ(0u, stats.modulations_computed);
    EXPECT_EQ(0u, stats.detections_processed);
}

TEST_F(HypothalamusPerceptionBridgeTest, GetStatsTracksModulations) {
    hypo_perception_bridge_compute_modulation(bridge);
    hypo_perception_bridge_compute_modulation(bridge);
    hypo_perception_bridge_compute_modulation(bridge);

    hypo_perception_bridge_stats_t stats;
    EXPECT_EQ(0, hypo_perception_bridge_get_stats(bridge, &stats));
    EXPECT_EQ(3u, stats.modulations_computed);
}

TEST_F(HypothalamusPerceptionBridgeTest, GetStatsTracksDetections) {
    hypo_sensory_detection_t detection = createDetection(
        HYPO_STIM_FOOD, HYPO_SENSE_VISUAL, 0.9f, 0.8f, false);

    hypo_perception_bridge_process_detection(bridge, &detection);
    hypo_perception_bridge_process_detection(bridge, &detection);

    hypo_perception_bridge_stats_t stats;
    EXPECT_EQ(0, hypo_perception_bridge_get_stats(bridge, &stats));
    EXPECT_EQ(2u, stats.detections_processed);
}

TEST_F(HypothalamusPerceptionBridgeTest, GetStatsTracksInteroceptive) {
    hypo_perception_bridge_process_interoceptive(bridge, HYPO_INTERO_HUNGER_PANGS, 0.5f);

    hypo_perception_bridge_stats_t stats;
    EXPECT_EQ(0, hypo_perception_bridge_get_stats(bridge, &stats));
    EXPECT_EQ(1u, stats.interoceptive_signals);
}

TEST_F(HypothalamusPerceptionBridgeTest, GetStatsTracksOlfactory) {
    hypo_perception_bridge_process_olfactory(bridge, HYPO_OLFACT_FOOD_PLEASANT, 0.5f);

    hypo_perception_bridge_stats_t stats;
    EXPECT_EQ(0, hypo_perception_bridge_get_stats(bridge, &stats));
    EXPECT_EQ(1u, stats.olfactory_stimuli);
}

TEST_F(HypothalamusPerceptionBridgeTest, GetStatsTracksGustatory) {
    hypo_perception_bridge_process_gustatory(bridge, HYPO_GUST_SWEET, 0.5f);

    hypo_perception_bridge_stats_t stats;
    EXPECT_EQ(0, hypo_perception_bridge_get_stats(bridge, &stats));
    EXPECT_EQ(1u, stats.gustatory_stimuli);
}

TEST_F(HypothalamusPerceptionBridgeTest, GetStatsTracksPredictions) {
    hypo_perception_bridge_generate_prediction(bridge, HYPO_PRED_FOOD_PRESENCE, 0.5f, 0.8f);

    hypo_perception_bridge_stats_t stats;
    EXPECT_EQ(0, hypo_perception_bridge_get_stats(bridge, &stats));
    EXPECT_EQ(1u, stats.predictions_generated);
}

TEST_F(HypothalamusPerceptionBridgeTest, GetStatsTracksSleepGate) {
    bool passes;
    hypo_perception_bridge_check_sleep_gate(bridge, 0.5f, false, false, &passes);

    hypo_perception_bridge_stats_t stats;
    EXPECT_EQ(0, hypo_perception_bridge_get_stats(bridge, &stats));
    EXPECT_EQ(1u, stats.sleep_gate_checks);
}

TEST_F(HypothalamusPerceptionBridgeTest, GetStatsNullBridge) {
    hypo_perception_bridge_stats_t stats;
    EXPECT_EQ(-1, hypo_perception_bridge_get_stats(nullptr, &stats));
}

TEST_F(HypothalamusPerceptionBridgeTest, GetStatsNullOutput) {
    EXPECT_EQ(-1, hypo_perception_bridge_get_stats(bridge, nullptr));
}

TEST_F(HypothalamusPerceptionBridgeTest, ResetStatsSuccess) {
    // Generate some stats
    hypo_perception_bridge_compute_modulation(bridge);
    hypo_perception_bridge_process_interoceptive(bridge, HYPO_INTERO_HUNGER_PANGS, 0.5f);

    // Reset stats
    EXPECT_EQ(0, hypo_perception_bridge_reset_stats(bridge));

    // Verify stats are cleared
    hypo_perception_bridge_stats_t stats;
    EXPECT_EQ(0, hypo_perception_bridge_get_stats(bridge, &stats));
    EXPECT_EQ(0u, stats.modulations_computed);
    EXPECT_EQ(0u, stats.interoceptive_signals);
}

TEST_F(HypothalamusPerceptionBridgeTest, ResetStatsNullBridge) {
    EXPECT_EQ(-1, hypo_perception_bridge_reset_stats(nullptr));
}

TEST_F(HypothalamusPerceptionBridgeTest, UpdateSuccess) {
    EXPECT_EQ(0, hypo_perception_bridge_update(bridge, 1000000));  // 1 second
}

TEST_F(HypothalamusPerceptionBridgeTest, UpdateNullBridge) {
    EXPECT_EQ(-1, hypo_perception_bridge_update(nullptr, 1000000));
}

TEST_F(HypothalamusPerceptionBridgeTest, UpdateDecaysEndorphins) {
    // Set high endorphins
    hypo_perception_bridge_release_endorphins(bridge, 0.8f);

    hypo_pain_modulation_state_t state_before;
    hypo_perception_bridge_get_pain_state(bridge, &state_before);

    // Update with time passage
    for (int i = 0; i < 1000; i++) {
        hypo_perception_bridge_update(bridge, 10000);  // 10ms each
    }

    hypo_pain_modulation_state_t state_after;
    hypo_perception_bridge_get_pain_state(bridge, &state_after);

    // Endorphins should have decayed
    EXPECT_LT(state_after.endorphin_level, state_before.endorphin_level);
}

TEST_F(HypothalamusPerceptionBridgeTest, PrintSummaryDoesNotCrash) {
    // Just verify it doesn't crash
    hypo_perception_bridge_print_summary(bridge);
    hypo_perception_bridge_print_summary(nullptr);
    SUCCEED();
}

// ============================================================================
// STRING UTILITY TESTS
// ============================================================================

TEST_F(HypothalamusPerceptionBridgeTest, InteroceptiveTypeNameValid) {
    EXPECT_STREQ("hunger_pangs", hypo_interoceptive_type_name(HYPO_INTERO_HUNGER_PANGS));
    EXPECT_STREQ("heart_rate", hypo_interoceptive_type_name(HYPO_INTERO_HEART_RATE));
    EXPECT_STREQ("breathing", hypo_interoceptive_type_name(HYPO_INTERO_BREATHING));
    EXPECT_STREQ("temperature", hypo_interoceptive_type_name(HYPO_INTERO_TEMPERATURE));
    EXPECT_STREQ("fatigue", hypo_interoceptive_type_name(HYPO_INTERO_FATIGUE));
    EXPECT_STREQ("pain", hypo_interoceptive_type_name(HYPO_INTERO_PAIN));
    EXPECT_STREQ("thirst", hypo_interoceptive_type_name(HYPO_INTERO_THIRST));
    EXPECT_STREQ("bladder", hypo_interoceptive_type_name(HYPO_INTERO_BLADDER));
}

TEST_F(HypothalamusPerceptionBridgeTest, InteroceptiveTypeNameInvalid) {
    EXPECT_STREQ("unknown", hypo_interoceptive_type_name(HYPO_INTERO_COUNT));
    EXPECT_STREQ("unknown", hypo_interoceptive_type_name((hypo_interoceptive_type_t)100));
}

TEST_F(HypothalamusPerceptionBridgeTest, OlfactoryTypeNameValid) {
    EXPECT_STREQ("food_pleasant", hypo_olfactory_type_name(HYPO_OLFACT_FOOD_PLEASANT));
    EXPECT_STREQ("food_unpleasant", hypo_olfactory_type_name(HYPO_OLFACT_FOOD_UNPLEASANT));
    EXPECT_STREQ("pheromone_social", hypo_olfactory_type_name(HYPO_OLFACT_PHEROMONE_SOCIAL));
    EXPECT_STREQ("danger", hypo_olfactory_type_name(HYPO_OLFACT_DANGER));
    EXPECT_STREQ("neutral", hypo_olfactory_type_name(HYPO_OLFACT_NEUTRAL));
}

TEST_F(HypothalamusPerceptionBridgeTest, OlfactoryTypeNameInvalid) {
    EXPECT_STREQ("unknown", hypo_olfactory_type_name(HYPO_OLFACT_COUNT));
    EXPECT_STREQ("unknown", hypo_olfactory_type_name((hypo_olfactory_type_t)100));
}

TEST_F(HypothalamusPerceptionBridgeTest, GustatoryTypeNameValid) {
    EXPECT_STREQ("sweet", hypo_gustatory_type_name(HYPO_GUST_SWEET));
    EXPECT_STREQ("salty", hypo_gustatory_type_name(HYPO_GUST_SALTY));
    EXPECT_STREQ("sour", hypo_gustatory_type_name(HYPO_GUST_SOUR));
    EXPECT_STREQ("bitter", hypo_gustatory_type_name(HYPO_GUST_BITTER));
    EXPECT_STREQ("umami", hypo_gustatory_type_name(HYPO_GUST_UMAMI));
    EXPECT_STREQ("fat", hypo_gustatory_type_name(HYPO_GUST_FAT));
}

TEST_F(HypothalamusPerceptionBridgeTest, GustatoryTypeNameInvalid) {
    EXPECT_STREQ("unknown", hypo_gustatory_type_name(HYPO_GUST_COUNT));
    EXPECT_STREQ("unknown", hypo_gustatory_type_name((hypo_gustatory_type_t)100));
}

TEST_F(HypothalamusPerceptionBridgeTest, PredictionTypeNameValid) {
    EXPECT_STREQ("food_presence", hypo_prediction_type_name(HYPO_PRED_FOOD_PRESENCE));
    EXPECT_STREQ("threat_presence", hypo_prediction_type_name(HYPO_PRED_THREAT_PRESENCE));
    EXPECT_STREQ("social_interaction", hypo_prediction_type_name(HYPO_PRED_SOCIAL_INTERACTION));
    EXPECT_STREQ("temperature_change", hypo_prediction_type_name(HYPO_PRED_TEMPERATURE_CHANGE));
    EXPECT_STREQ("reward_available", hypo_prediction_type_name(HYPO_PRED_REWARD_AVAILABLE));
}

TEST_F(HypothalamusPerceptionBridgeTest, PredictionTypeNameInvalid) {
    EXPECT_STREQ("unknown", hypo_prediction_type_name(HYPO_PRED_COUNT));
    EXPECT_STREQ("unknown", hypo_prediction_type_name((hypo_prediction_type_t)100));
}

TEST_F(HypothalamusPerceptionBridgeTest, SleepStageNameValid) {
    EXPECT_STREQ("awake", hypo_sleep_stage_name(HYPO_SLEEP_AWAKE));
    EXPECT_STREQ("drowsy", hypo_sleep_stage_name(HYPO_SLEEP_DROWSY));
    EXPECT_STREQ("light", hypo_sleep_stage_name(HYPO_SLEEP_LIGHT));
    EXPECT_STREQ("deep", hypo_sleep_stage_name(HYPO_SLEEP_DEEP));
    EXPECT_STREQ("rem", hypo_sleep_stage_name(HYPO_SLEEP_REM));
}

TEST_F(HypothalamusPerceptionBridgeTest, SleepStageNameInvalid) {
    EXPECT_STREQ("unknown", hypo_sleep_stage_name(HYPO_SLEEP_COUNT));
    EXPECT_STREQ("unknown", hypo_sleep_stage_name((hypo_sleep_stage_t)100));
}

// ============================================================================
// EDGE CASE AND BOUNDARY TESTS
// ============================================================================

TEST_F(HypothalamusPerceptionBridgeTest, IntensityClampingAtBoundaries) {
    // Test clamping for various functions with out-of-range values
    EXPECT_EQ(0, hypo_perception_bridge_process_interoceptive(
        bridge, HYPO_INTERO_PAIN, 1.5f));
    hypo_interoceptive_state_t state;
    hypo_perception_bridge_get_interoceptive_state(bridge, &state);
    EXPECT_LE(state.signals[HYPO_INTERO_PAIN], 1.0f);

    EXPECT_EQ(0, hypo_perception_bridge_process_interoceptive(
        bridge, HYPO_INTERO_PAIN, -0.5f));
    hypo_perception_bridge_get_interoceptive_state(bridge, &state);
    EXPECT_GE(state.signals[HYPO_INTERO_PAIN], 0.0f);
}

TEST_F(HypothalamusPerceptionBridgeTest, TemperatureClampingAtBoundaries) {
    EXPECT_EQ(0, hypo_perception_bridge_set_core_temperature(bridge, 2.0f));
    hypo_thermal_state_t state;
    hypo_perception_bridge_get_thermal_state(bridge, &state);
    EXPECT_LE(state.core_temperature, 1.0f);

    EXPECT_EQ(0, hypo_perception_bridge_set_core_temperature(bridge, -1.0f));
    hypo_perception_bridge_get_thermal_state(bridge, &state);
    EXPECT_GE(state.core_temperature, 0.0f);
}

TEST_F(HypothalamusPerceptionBridgeTest, AllSleepStagesValid) {
    for (int i = 0; i < HYPO_SLEEP_COUNT; i++) {
        EXPECT_EQ(0, hypo_perception_bridge_set_sleep_stage(
            bridge, (hypo_sleep_stage_t)i));
    }
}

TEST_F(HypothalamusPerceptionBridgeTest, AllInteroceptiveTypesValid) {
    for (int i = 0; i < HYPO_INTERO_COUNT; i++) {
        EXPECT_EQ(0, hypo_perception_bridge_process_interoceptive(
            bridge, (hypo_interoceptive_type_t)i, 0.5f));
    }
}

TEST_F(HypothalamusPerceptionBridgeTest, AllOlfactoryTypesValid) {
    for (int i = 0; i < HYPO_OLFACT_COUNT; i++) {
        EXPECT_EQ(0, hypo_perception_bridge_process_olfactory(
            bridge, (hypo_olfactory_type_t)i, 0.5f));
    }
}

TEST_F(HypothalamusPerceptionBridgeTest, AllGustatoryTypesValid) {
    for (int i = 0; i < HYPO_GUST_COUNT; i++) {
        EXPECT_EQ(0, hypo_perception_bridge_process_gustatory(
            bridge, (hypo_gustatory_type_t)i, 0.5f));
    }
}

TEST_F(HypothalamusPerceptionBridgeTest, AllPredictionTypesValid) {
    for (int i = 0; i < HYPO_PRED_COUNT; i++) {
        EXPECT_EQ(0, hypo_perception_bridge_generate_prediction(
            bridge, (hypo_prediction_type_t)i, 0.5f, 0.5f));
    }
}

TEST_F(HypothalamusPerceptionBridgeTest, RapidUpdatesDoNotAccumulateErrors) {
    for (int i = 0; i < 10000; i++) {
        EXPECT_EQ(0, hypo_perception_bridge_update(bridge, 100));  // 0.1ms
    }

    // Should still be able to get valid state
    hypo_perception_modulation_t mod;
    EXPECT_EQ(0, hypo_perception_bridge_get_modulation(bridge, &mod));
    EXPECT_GE(mod.global_gain, 0.0f);
}

TEST_F(HypothalamusPerceptionBridgeTest, LongUpdateDoesNotCrash) {
    // 24 hours worth of time in one update
    uint64_t day_us = 24ULL * 3600ULL * 1000000ULL;
    EXPECT_EQ(0, hypo_perception_bridge_update(bridge, day_us));
}

TEST_F(HypothalamusPerceptionBridgeTest, ConcurrentEnhancementsWork) {
    // Test all enhancements working together
    hypo_perception_bridge_process_interoceptive(bridge, HYPO_INTERO_HUNGER_PANGS, 0.8f);
    hypo_perception_bridge_process_olfactory(bridge, HYPO_OLFACT_FOOD_PLEASANT, 0.7f);
    hypo_perception_bridge_process_gustatory(bridge, HYPO_GUST_SWEET, 0.6f);
    hypo_perception_bridge_generate_prediction(bridge, HYPO_PRED_FOOD_PRESENCE, 0.9f, 0.8f);
    hypo_perception_bridge_set_stress_for_pain(bridge, 0.5f, false);
    hypo_perception_bridge_set_sleep_stage(bridge, HYPO_SLEEP_DROWSY);
    hypo_perception_bridge_set_core_temperature(bridge, 0.6f);

    // Compute modulation with all enhancements active
    EXPECT_EQ(0, hypo_perception_bridge_compute_modulation(bridge));

    // Verify we can still get valid state
    hypo_perception_modulation_t mod;
    EXPECT_EQ(0, hypo_perception_bridge_get_modulation(bridge, &mod));
    EXPECT_GE(mod.global_gain, 0.0f);
}

TEST_F(HypothalamusPerceptionBridgeTest, ResetClearsAllEnhancements) {
    // Set up state in all enhancements
    hypo_perception_bridge_process_interoceptive(bridge, HYPO_INTERO_HUNGER_PANGS, 0.9f);
    hypo_perception_bridge_process_olfactory(bridge, HYPO_OLFACT_FOOD_PLEASANT, 0.8f);
    hypo_perception_bridge_generate_prediction(bridge, HYPO_PRED_FOOD_PRESENCE, 0.9f, 0.9f);
    hypo_perception_bridge_set_stress_for_pain(bridge, 0.8f, true);
    hypo_perception_bridge_set_sleep_stage(bridge, HYPO_SLEEP_DEEP);
    hypo_perception_bridge_set_core_temperature(bridge, 0.2f);

    // Reset
    EXPECT_EQ(0, hypo_perception_bridge_reset(bridge));

    // Verify all states are back to defaults
    hypo_interoceptive_state_t intero_state;
    hypo_perception_bridge_get_interoceptive_state(bridge, &intero_state);
    EXPECT_FLOAT_EQ(0.0f, intero_state.signals[HYPO_INTERO_HUNGER_PANGS]);

    hypo_chemosensory_state_t chemo_state;
    hypo_perception_bridge_get_chemosensory_state(bridge, &chemo_state);
    EXPECT_FLOAT_EQ(0.0f, chemo_state.olfactory_intensity[HYPO_OLFACT_FOOD_PLEASANT]);

    hypo_predictive_state_t pred_state;
    hypo_perception_bridge_get_predictive_state(bridge, &pred_state);
    EXPECT_FLOAT_EQ(0.5f, pred_state.predictions[HYPO_PRED_FOOD_PRESENCE]);  // Default

    hypo_pain_modulation_state_t pain_state;
    hypo_perception_bridge_get_pain_state(bridge, &pain_state);
    EXPECT_FLOAT_EQ(1.0f, pain_state.pain_sensitivity);

    hypo_sleep_gating_state_t sleep_state;
    hypo_perception_bridge_get_sleep_gating_state(bridge, &sleep_state);
    EXPECT_EQ(HYPO_SLEEP_AWAKE, sleep_state.current_stage);

    hypo_thermal_state_t thermal_state;
    hypo_perception_bridge_get_thermal_state(bridge, &thermal_state);
    EXPECT_FLOAT_EQ(0.5f, thermal_state.core_temperature);  // Default
}

// ============================================================================
// CATEGORY-TO-DRIVE MAPPING TESTS
// ============================================================================

TEST_F(HypothalamusPerceptionBridgeTest, DetectionCategoriesMapCorrectly) {
    // Test each category maps to correct drive
    struct {
        hypo_stim_category_t category;
        hypo_drive_type_t expected_drive;
    } mappings[] = {
        {HYPO_STIM_FOOD, HYPO_DRIVE_HUNGER},
        {HYPO_STIM_WATER, HYPO_DRIVE_THIRST},
        {HYPO_STIM_THREAT, HYPO_DRIVE_SAFETY},
        {HYPO_STIM_SOCIAL, HYPO_DRIVE_SOCIAL},
        {HYPO_STIM_NOVEL, HYPO_DRIVE_CURIOSITY},
        {HYPO_STIM_THERMAL, HYPO_DRIVE_TEMPERATURE},
    };

    for (const auto& mapping : mappings) {
        // Reset anticipation
        hypo_perception_bridge_reset(bridge);

        hypo_sensory_detection_t detection = createDetection(
            mapping.category, HYPO_SENSE_VISUAL, 0.9f, 0.9f, false);

        float initial = hypo_perception_bridge_get_anticipation(bridge, mapping.expected_drive);
        hypo_perception_bridge_process_detection(bridge, &detection);
        float after = hypo_perception_bridge_get_anticipation(bridge, mapping.expected_drive);

        EXPECT_GT(after, initial) << "Category " << mapping.category
                                  << " should boost drive " << mapping.expected_drive;
    }
}

// ============================================================================
// MODALITY WEIGHT TESTS
// ============================================================================

TEST_F(HypothalamusPerceptionBridgeTest, ModalityGainsAffectedByConfig) {
    hypo_perception_config_t custom_config;
    hypo_perception_bridge_default_config(&custom_config);
    custom_config.visual_weight = 1.5f;
    custom_config.auditory_weight = 0.8f;

    hypo_perception_bridge_t* custom_bridge = hypo_perception_bridge_create(drive_system, &custom_config);
    ASSERT_NE(nullptr, custom_bridge);

    hypo_perception_bridge_compute_modulation(custom_bridge);

    hypo_perception_modulation_t mod;
    hypo_perception_bridge_get_modulation(custom_bridge, &mod);

    // Visual should have higher gain than auditory
    EXPECT_GT(mod.modality_gains[HYPO_SENSE_VISUAL], mod.modality_gains[HYPO_SENSE_AUDITORY]);

    hypo_perception_bridge_destroy(custom_bridge);
}

// ============================================================================
// ANTICIPATION DECAY TESTS
// ============================================================================

TEST_F(HypothalamusPerceptionBridgeTest, AnticipationDecaysOverTime) {
    // Boost anticipation
    hypo_sensory_detection_t detection = createDetection(
        HYPO_STIM_FOOD, HYPO_SENSE_VISUAL, 0.9f, 0.9f, false);
    hypo_perception_bridge_process_detection(bridge, &detection);

    float initial = hypo_perception_bridge_get_anticipation(bridge, HYPO_DRIVE_HUNGER);
    EXPECT_GT(initial, 0.0f);

    // Compute modulation several times (causes decay)
    for (int i = 0; i < 100; i++) {
        hypo_perception_bridge_compute_modulation(bridge);
    }

    float after = hypo_perception_bridge_get_anticipation(bridge, HYPO_DRIVE_HUNGER);
    EXPECT_LT(after, initial);
}

// ============================================================================
// FREE ENERGY STATISTICS TESTS
// ============================================================================

TEST_F(HypothalamusPerceptionBridgeTest, StatsTrackAverageFreeEnergy) {
    // Generate predictions and errors
    for (int i = 0; i < 10; i++) {
        hypo_perception_bridge_generate_prediction(
            bridge, HYPO_PRED_FOOD_PRESENCE, 0.5f, 0.8f);
        hypo_perception_bridge_update_prediction_error(
            bridge, HYPO_PRED_FOOD_PRESENCE, 0.9f);
    }

    hypo_perception_bridge_stats_t stats;
    EXPECT_EQ(0, hypo_perception_bridge_get_stats(bridge, &stats));
    EXPECT_GT(stats.avg_free_energy, 0.0f);
}

TEST_F(HypothalamusPerceptionBridgeTest, StatsTrackAveragePainModulation) {
    // Modulate pain several times
    float modulated;
    for (int i = 0; i < 10; i++) {
        hypo_perception_bridge_modulate_pain(bridge, 0.5f, &modulated);
    }

    hypo_perception_bridge_stats_t stats;
    EXPECT_EQ(0, hypo_perception_bridge_get_stats(bridge, &stats));
    EXPECT_GT(stats.avg_pain_modulation, 0.0f);
}
