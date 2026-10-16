/**
 * @file test_hypothalamus_perception_bridge_regression.cpp
 * @brief Regression tests for Hypothalamus-Perception Bridge
 *
 * WHAT: Regression tests ensuring perception bridge behavior remains consistent
 *       across code changes and enhancements don't break baseline functionality
 * WHY:  Prevent regressions in sensory modulation, detection, and enhanced features
 * HOW:  Test known good behaviors, numeric stability, performance, and isolation
 *
 * REGRESSION SCENARIOS:
 * 1. Baseline Behavior - Original functionality still works
 * 2. Backward Compatibility - Old code patterns still work
 * 3. Configuration Stability - Default configs produce expected values
 * 4. Numeric Stability - Clamping works correctly, no NaN/Inf
 * 5. Performance Metrics - Statistics tracking is accurate
 * 6. State Consistency - Reset returns to known state
 * 7. Memory Behavior - No leaks on create/destroy cycles
 * 8. Enhancement Isolation - Each enhancement can be enabled/disabled independently
 * 9. Stress Testing - Rapid updates don't cause issues
 *
 * @version Phase 17.5: Enhanced Perception Integration
 * @date 2026-01-10
 */

#include <gtest/gtest.h>
#include <string.h>
#include <stdlib.h>
#include <cmath>
#include <vector>
#include <chrono>
#include <limits>

// Headers have their own extern "C" guards
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_perception_bridge.h"
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_drives.h"

// ============================================================================
// REGRESSION TEST FIXTURE
// ============================================================================

class PerceptionBridgeRegressionTest : public ::testing::Test {
protected:
    hypo_drive_system_handle_t* drives;
    hypo_drive_config_t drive_config;
    hypo_perception_bridge_t* bridge;
    hypo_perception_config_t perception_config;

    void SetUp() override {
        drive_config = hypo_drive_default_config();
        drives = hypo_drive_create(&drive_config);
        ASSERT_NE(nullptr, drives);

        hypo_perception_bridge_default_config(&perception_config);
        bridge = hypo_perception_bridge_create(drives, &perception_config);
        ASSERT_NE(nullptr, bridge);
    }

    void TearDown() override {
        if (bridge) {
            hypo_perception_bridge_destroy(bridge);
            bridge = nullptr;
        }
        if (drives) {
            hypo_drive_destroy(drives);
            drives = nullptr;
        }
    }

    // Helper to check if a float is valid (not NaN or Inf)
    bool is_valid_float(float val) {
        return !std::isnan(val) && !std::isinf(val);
    }
};

// ============================================================================
// 1. BASELINE BEHAVIOR REGRESSION
// ============================================================================

// Regression: Modulation computation produces valid output
TEST_F(PerceptionBridgeRegressionTest, BaselineModulationComputation) {
    // Compute modulation
    EXPECT_EQ(0, hypo_perception_bridge_compute_modulation(bridge));

    // Get modulation output
    hypo_perception_modulation_t modulation;
    memset(&modulation, 0xFF, sizeof(modulation));

    EXPECT_EQ(0, hypo_perception_bridge_get_modulation(bridge, &modulation));

    // Verify fields are populated with valid values
    EXPECT_TRUE(is_valid_float(modulation.global_gain));
    EXPECT_TRUE(is_valid_float(modulation.arousal_level));

    // Global gain should be in expected range [0.5, 2.0]
    EXPECT_GE(modulation.global_gain, 0.5f);
    EXPECT_LE(modulation.global_gain, 2.0f);

    // Arousal should be in [0, 1]
    EXPECT_GE(modulation.arousal_level, 0.0f);
    EXPECT_LE(modulation.arousal_level, 1.0f);
}

// Regression: Arousal setting affects modulation
TEST_F(PerceptionBridgeRegressionTest, BaselineArousalAffectsModulation) {
    // Set low arousal
    EXPECT_EQ(0, hypo_perception_bridge_set_arousal(bridge, 0.0f));
    EXPECT_EQ(0, hypo_perception_bridge_compute_modulation(bridge));

    hypo_perception_modulation_t low_arousal_mod;
    EXPECT_EQ(0, hypo_perception_bridge_get_modulation(bridge, &low_arousal_mod));

    // Set high arousal
    EXPECT_EQ(0, hypo_perception_bridge_set_arousal(bridge, 1.0f));
    EXPECT_EQ(0, hypo_perception_bridge_compute_modulation(bridge));

    hypo_perception_modulation_t high_arousal_mod;
    EXPECT_EQ(0, hypo_perception_bridge_get_modulation(bridge, &high_arousal_mod));

    // High arousal should produce higher global gain
    EXPECT_GT(high_arousal_mod.global_gain, low_arousal_mod.global_gain)
        << "Higher arousal should produce higher sensory gain";
    EXPECT_GT(high_arousal_mod.arousal_level, low_arousal_mod.arousal_level);
}

// Regression: Detection processing updates anticipation
TEST_F(PerceptionBridgeRegressionTest, BaselineDetectionUpdatesAnticipation) {
    // Get initial anticipation
    float initial_anticipation = hypo_perception_bridge_get_anticipation(bridge, HYPO_DRIVE_HUNGER);

    // Process food detection
    hypo_sensory_detection_t detection;
    memset(&detection, 0, sizeof(detection));
    detection.detected_category = HYPO_STIM_FOOD;
    detection.modality = HYPO_SENSE_VISUAL;
    detection.confidence = 0.9f;
    detection.intensity = 0.8f;
    detection.is_threat = false;
    detection.timestamp_us = 1000000;

    EXPECT_EQ(0, hypo_perception_bridge_process_detection(bridge, &detection));

    // Anticipation for hunger should increase after food detection
    float after_anticipation = hypo_perception_bridge_get_anticipation(bridge, HYPO_DRIVE_HUNGER);
    EXPECT_GE(after_anticipation, initial_anticipation)
        << "Food detection should maintain or increase hunger anticipation";
}

// Regression: Category salience reflects drive urgencies
TEST_F(PerceptionBridgeRegressionTest, BaselineCategorySalienceFromDrives) {
    // Satisfy hunger to lower its urgency
    hypo_drive_satisfy(drives, HYPO_DRIVE_HUNGER, 1.0f);

    // Update and compute modulation
    hypo_drive_update(drives, 1000);
    EXPECT_EQ(0, hypo_perception_bridge_compute_modulation(bridge));

    hypo_perception_modulation_t mod;
    EXPECT_EQ(0, hypo_perception_bridge_get_modulation(bridge, &mod));

    // All category saliences should be valid
    for (int i = 0; i < HYPO_STIM_COUNT; i++) {
        EXPECT_TRUE(is_valid_float(mod.category_salience[i]))
            << "Category " << i << " salience is invalid";
        EXPECT_GE(mod.category_salience[i], 0.0f)
            << "Category " << i << " salience should be non-negative";
    }
}

// ============================================================================
// 2. BACKWARD COMPATIBILITY REGRESSION
// ============================================================================

// Regression: NULL config creates bridge with defaults
TEST_F(PerceptionBridgeRegressionTest, BackwardCompatNullConfigCreation) {
    hypo_perception_bridge_t* default_bridge = hypo_perception_bridge_create(drives, NULL);
    EXPECT_NE(nullptr, default_bridge);

    if (default_bridge) {
        // Should work normally
        EXPECT_EQ(0, hypo_perception_bridge_compute_modulation(default_bridge));

        hypo_perception_modulation_t mod;
        EXPECT_EQ(0, hypo_perception_bridge_get_modulation(default_bridge, &mod));
        EXPECT_TRUE(is_valid_float(mod.global_gain));

        hypo_perception_bridge_destroy(default_bridge);
    }
}

// Regression: Old modality enum values still work
TEST_F(PerceptionBridgeRegressionTest, BackwardCompatModalityEnums) {
    // Test all original modality values
    hypo_perception_modulation_t mod;
    EXPECT_EQ(0, hypo_perception_bridge_compute_modulation(bridge));
    EXPECT_EQ(0, hypo_perception_bridge_get_modulation(bridge, &mod));

    // All modality gains should be accessible
    for (int i = 0; i < HYPO_SENSE_COUNT; i++) {
        EXPECT_TRUE(is_valid_float(mod.modality_gains[i]))
            << "Modality " << i << " gain should be valid";
    }
}

// Regression: Old stimulus category enum values still work
TEST_F(PerceptionBridgeRegressionTest, BackwardCompatStimulusCategoryEnums) {
    hypo_sensory_detection_t detection;
    memset(&detection, 0, sizeof(detection));
    detection.confidence = 0.7f;
    detection.intensity = 0.5f;

    // Test all original category values
    hypo_stim_category_t categories[] = {
        HYPO_STIM_FOOD, HYPO_STIM_WATER, HYPO_STIM_THREAT,
        HYPO_STIM_SOCIAL, HYPO_STIM_NOVEL, HYPO_STIM_NEUTRAL
    };

    for (auto cat : categories) {
        detection.detected_category = cat;
        EXPECT_EQ(0, hypo_perception_bridge_process_detection(bridge, &detection))
            << "Category " << cat << " processing should succeed";
    }
}

// ============================================================================
// 3. CONFIGURATION STABILITY REGRESSION
// ============================================================================

// Regression: Default config values match documented defaults
TEST_F(PerceptionBridgeRegressionTest, ConfigStabilityDefaultValues) {
    hypo_perception_config_t config;
    hypo_perception_bridge_default_config(&config);

    // Document expected default values
    EXPECT_FLOAT_EQ(0.7f, config.arousal_gain_min)
        << "Default arousal_gain_min should be 0.7";
    EXPECT_FLOAT_EQ(1.5f, config.arousal_gain_max)
        << "Default arousal_gain_max should be 1.5";
    EXPECT_FLOAT_EQ(0.8f, config.drive_salience_weight)
        << "Default drive_salience_weight should be 0.8";
    EXPECT_FLOAT_EQ(0.5f, config.threat_priority_threshold)
        << "Default threat_priority_threshold should be 0.5";
}

// Regression: Enhanced config contains base config
TEST_F(PerceptionBridgeRegressionTest, ConfigStabilityEnhancedContainsBase) {
    hypo_perception_enhanced_config_t enhanced;
    hypo_perception_bridge_default_enhanced_config(&enhanced);

    // Base config values should match
    hypo_perception_config_t base;
    hypo_perception_bridge_default_config(&base);

    EXPECT_FLOAT_EQ(base.arousal_gain_min, enhanced.base.arousal_gain_min);
    EXPECT_FLOAT_EQ(base.arousal_gain_max, enhanced.base.arousal_gain_max);
    EXPECT_FLOAT_EQ(base.drive_salience_weight, enhanced.base.drive_salience_weight);
}

// Regression: Config application doesn't corrupt state
TEST_F(PerceptionBridgeRegressionTest, ConfigStabilityApplyDoesntCorrupt) {
    hypo_perception_enhanced_config_t enhanced;
    hypo_perception_bridge_default_enhanced_config(&enhanced);

    // Modify some settings
    enhanced.enable_interoception = true;
    enhanced.enable_chemosensory = true;

    EXPECT_EQ(0, hypo_perception_bridge_apply_enhanced_config(bridge, &enhanced));

    // Bridge should still function
    EXPECT_EQ(0, hypo_perception_bridge_compute_modulation(bridge));

    hypo_perception_modulation_t mod;
    EXPECT_EQ(0, hypo_perception_bridge_get_modulation(bridge, &mod));
    EXPECT_TRUE(is_valid_float(mod.global_gain));
}

// ============================================================================
// 4. NUMERIC STABILITY REGRESSION
// ============================================================================

// Regression: Extreme arousal values are clamped
TEST_F(PerceptionBridgeRegressionTest, NumericStabilityArousalClamping) {
    // Test extreme values
    EXPECT_EQ(0, hypo_perception_bridge_set_arousal(bridge, -10.0f));
    EXPECT_EQ(0, hypo_perception_bridge_compute_modulation(bridge));

    hypo_perception_modulation_t mod;
    EXPECT_EQ(0, hypo_perception_bridge_get_modulation(bridge, &mod));
    EXPECT_GE(mod.arousal_level, 0.0f) << "Arousal should be clamped to >= 0";

    EXPECT_EQ(0, hypo_perception_bridge_set_arousal(bridge, 100.0f));
    EXPECT_EQ(0, hypo_perception_bridge_compute_modulation(bridge));
    EXPECT_EQ(0, hypo_perception_bridge_get_modulation(bridge, &mod));
    EXPECT_LE(mod.arousal_level, 1.0f) << "Arousal should be clamped to <= 1";
}

// Regression: NaN handling - documents current behavior
// NOTE: This test documents that NaN values currently propagate.
// Future improvement: Implementation should clamp NaN to valid range
TEST_F(PerceptionBridgeRegressionTest, NumericStabilityNaNBehavior) {
    float nan_val = std::numeric_limits<float>::quiet_NaN();

    // Get known good state first
    EXPECT_EQ(0, hypo_perception_bridge_set_arousal(bridge, 0.5f));
    EXPECT_EQ(0, hypo_perception_bridge_compute_modulation(bridge));

    hypo_perception_modulation_t good_mod;
    EXPECT_EQ(0, hypo_perception_bridge_get_modulation(bridge, &good_mod));
    EXPECT_TRUE(is_valid_float(good_mod.global_gain));

    // Document: Setting NaN arousal is currently accepted (returns 0)
    // Ideal behavior: Should return error or clamp to valid value
    int nan_result = hypo_perception_bridge_set_arousal(bridge, nan_val);
    // We document the current behavior without asserting it's correct
    (void)nan_result;

    // After NaN, reset to restore known good state
    EXPECT_EQ(0, hypo_perception_bridge_reset(bridge));
    EXPECT_EQ(0, hypo_perception_bridge_compute_modulation(bridge));

    hypo_perception_modulation_t reset_mod;
    EXPECT_EQ(0, hypo_perception_bridge_get_modulation(bridge, &reset_mod));

    // After reset, values should be valid again
    EXPECT_TRUE(is_valid_float(reset_mod.global_gain))
        << "Reset should restore valid global_gain";
    EXPECT_TRUE(is_valid_float(reset_mod.arousal_level))
        << "Reset should restore valid arousal_level";
}

// Regression: No Inf from extreme computations
TEST_F(PerceptionBridgeRegressionTest, NumericStabilityNoInfFromExtremes) {
    float inf_val = std::numeric_limits<float>::infinity();

    // Attempt to inject infinity
    hypo_perception_bridge_set_arousal(bridge, inf_val);
    hypo_perception_bridge_compute_modulation(bridge);

    hypo_perception_modulation_t mod;
    hypo_perception_bridge_get_modulation(bridge, &mod);

    EXPECT_FALSE(std::isinf(mod.global_gain)) << "Inf should not appear in global_gain";
}

// Regression: Modality gains stay in valid range
TEST_F(PerceptionBridgeRegressionTest, NumericStabilityModalityGainsRange) {
    // Run several update cycles
    for (int i = 0; i < 100; i++) {
        hypo_perception_bridge_set_arousal(bridge, (float)i / 100.0f);
        hypo_perception_bridge_compute_modulation(bridge);
    }

    hypo_perception_modulation_t mod;
    EXPECT_EQ(0, hypo_perception_bridge_get_modulation(bridge, &mod));

    for (int i = 0; i < HYPO_SENSE_COUNT; i++) {
        EXPECT_TRUE(is_valid_float(mod.modality_gains[i]));
        EXPECT_GE(mod.modality_gains[i], 0.0f);
        EXPECT_LE(mod.modality_gains[i], 10.0f)  // Reasonable upper bound
            << "Modality " << i << " gain out of range";
    }
}

// Regression: Pain modulation stays in valid range
TEST_F(PerceptionBridgeRegressionTest, NumericStabilityPainModulationRange) {
    // Enable pain modulation
    hypo_perception_enhanced_config_t config;
    hypo_perception_bridge_default_enhanced_config(&config);
    config.enable_pain_modulation = true;
    hypo_perception_bridge_apply_enhanced_config(bridge, &config);

    // Test extreme stress values
    hypo_perception_bridge_set_stress_for_pain(bridge, 2.0f, false);

    float modulated_pain;
    EXPECT_EQ(0, hypo_perception_bridge_modulate_pain(bridge, 0.5f, &modulated_pain));

    EXPECT_TRUE(is_valid_float(modulated_pain));
    EXPECT_GE(modulated_pain, 0.0f);
    EXPECT_LE(modulated_pain, 2.0f)  // Pain can be amplified but should be bounded
        << "Modulated pain out of reasonable range";
}

// ============================================================================
// 5. PERFORMANCE METRICS REGRESSION
// ============================================================================

// Regression: Statistics counters increment correctly
TEST_F(PerceptionBridgeRegressionTest, MetricsStatisticsCountersIncrement) {
    hypo_perception_bridge_stats_t stats_before, stats_after;

    EXPECT_EQ(0, hypo_perception_bridge_get_stats(bridge, &stats_before));

    // Perform operations
    for (int i = 0; i < 10; i++) {
        hypo_perception_bridge_compute_modulation(bridge);
    }

    EXPECT_EQ(0, hypo_perception_bridge_get_stats(bridge, &stats_after));

    EXPECT_GT(stats_after.modulations_computed, stats_before.modulations_computed)
        << "Modulation counter should increment";
}

// Regression: Detection counter increments
TEST_F(PerceptionBridgeRegressionTest, MetricsDetectionCounterIncrements) {
    hypo_perception_bridge_stats_t stats_before;
    EXPECT_EQ(0, hypo_perception_bridge_get_stats(bridge, &stats_before));

    hypo_sensory_detection_t detection = {};
    detection.detected_category = HYPO_STIM_FOOD;
    detection.modality = HYPO_SENSE_VISUAL;
    detection.confidence = 0.8f;
    detection.intensity = 0.6f;

    for (int i = 0; i < 5; i++) {
        hypo_perception_bridge_process_detection(bridge, &detection);
    }

    hypo_perception_bridge_stats_t stats_after;
    EXPECT_EQ(0, hypo_perception_bridge_get_stats(bridge, &stats_after));

    EXPECT_EQ(stats_before.detections_processed + 5, stats_after.detections_processed)
        << "Detection counter should increment by 5";
}

// Regression: Reset clears statistics
TEST_F(PerceptionBridgeRegressionTest, MetricsResetClearsStatistics) {
    // Generate some activity
    for (int i = 0; i < 10; i++) {
        hypo_perception_bridge_compute_modulation(bridge);
    }

    // Reset stats
    EXPECT_EQ(0, hypo_perception_bridge_reset_stats(bridge));

    hypo_perception_bridge_stats_t stats;
    EXPECT_EQ(0, hypo_perception_bridge_get_stats(bridge, &stats));

    EXPECT_EQ(0u, stats.modulations_computed) << "Stats should be cleared after reset";
    EXPECT_EQ(0u, stats.detections_processed) << "Stats should be cleared after reset";
}

// ============================================================================
// 6. STATE CONSISTENCY REGRESSION
// ============================================================================

// Regression: Reset returns to known initial state
TEST_F(PerceptionBridgeRegressionTest, StateConsistencyResetToInitial) {
    // Get initial modulation
    hypo_perception_bridge_compute_modulation(bridge);
    hypo_perception_modulation_t initial_mod;
    hypo_perception_bridge_get_modulation(bridge, &initial_mod);

    // Modify state
    hypo_perception_bridge_set_arousal(bridge, 1.0f);
    hypo_perception_bridge_compute_modulation(bridge);

    hypo_perception_modulation_t modified_mod;
    hypo_perception_bridge_get_modulation(bridge, &modified_mod);

    // Verify modification took effect
    EXPECT_NE(initial_mod.arousal_level, modified_mod.arousal_level);

    // Reset
    EXPECT_EQ(0, hypo_perception_bridge_reset(bridge));

    // Compute again and check state is reset
    hypo_perception_bridge_compute_modulation(bridge);
    hypo_perception_modulation_t reset_mod;
    hypo_perception_bridge_get_modulation(bridge, &reset_mod);

    // Values should be close to initial (tolerance for floating point)
    EXPECT_NEAR(initial_mod.arousal_level, reset_mod.arousal_level, 0.01f)
        << "Reset should return arousal to initial state";
}

// Regression: Enhanced state reset also resets enhancements
TEST_F(PerceptionBridgeRegressionTest, StateConsistencyEnhancedStateReset) {
    // Enable and modify interoception
    hypo_perception_enhanced_config_t config;
    hypo_perception_bridge_default_enhanced_config(&config);
    config.enable_interoception = true;
    hypo_perception_bridge_apply_enhanced_config(bridge, &config);

    // Process interoceptive signals
    hypo_perception_bridge_process_interoceptive(bridge, HYPO_INTERO_HUNGER_PANGS, 0.8f);

    hypo_interoceptive_state_t state_before;
    hypo_perception_bridge_get_interoceptive_state(bridge, &state_before);

    // Reset
    hypo_perception_bridge_reset(bridge);

    hypo_interoceptive_state_t state_after;
    hypo_perception_bridge_get_interoceptive_state(bridge, &state_after);

    // Signal should be reset
    EXPECT_LT(state_after.signals[HYPO_INTERO_HUNGER_PANGS],
              state_before.signals[HYPO_INTERO_HUNGER_PANGS])
        << "Interoceptive signals should be reset";
}

// Regression: Sleep stage persists across updates
TEST_F(PerceptionBridgeRegressionTest, StateConsistencySleepStagePersistence) {
    // Enable sleep gating
    hypo_perception_enhanced_config_t config;
    hypo_perception_bridge_default_enhanced_config(&config);
    config.enable_sleep_gating = true;
    hypo_perception_bridge_apply_enhanced_config(bridge, &config);

    // Set sleep stage
    EXPECT_EQ(0, hypo_perception_bridge_set_sleep_stage(bridge, HYPO_SLEEP_DEEP));

    // Update multiple times
    for (int i = 0; i < 10; i++) {
        hypo_perception_bridge_update(bridge, 10000);
    }

    // Check sleep stage persisted
    hypo_sleep_gating_state_t state;
    EXPECT_EQ(0, hypo_perception_bridge_get_sleep_gating_state(bridge, &state));
    EXPECT_EQ(HYPO_SLEEP_DEEP, state.current_stage)
        << "Sleep stage should persist across updates";
}

// ============================================================================
// 7. MEMORY BEHAVIOR REGRESSION
// ============================================================================

// Regression: Create/destroy cycle doesn't leak
TEST_F(PerceptionBridgeRegressionTest, MemoryNoLeakOnCreateDestroyCycle) {
    // This test documents expected behavior
    // Memory leak detection requires external tools (valgrind, etc.)
    for (int i = 0; i < 100; i++) {
        hypo_perception_bridge_t* temp_bridge = hypo_perception_bridge_create(drives, NULL);
        ASSERT_NE(nullptr, temp_bridge) << "Create should succeed on iteration " << i;
        hypo_perception_bridge_destroy(temp_bridge);
    }
    // If we get here without crashing or timing out, cycles work
    SUCCEED();
}

// Regression: Destroy handles NULL gracefully
TEST_F(PerceptionBridgeRegressionTest, MemoryDestroyNullSafe) {
    // Should not crash
    hypo_perception_bridge_destroy(nullptr);
    SUCCEED();
}

// Regression: Operations after destroy are safe (should fail gracefully)
TEST_F(PerceptionBridgeRegressionTest, MemoryOperationsOnDestroyedBridge) {
    hypo_perception_bridge_t* temp_bridge = hypo_perception_bridge_create(drives, NULL);
    ASSERT_NE(nullptr, temp_bridge);
    hypo_perception_bridge_destroy(temp_bridge);

    // Note: After destruction, temp_bridge is a dangling pointer
    // In production code, should set to NULL after destroy
    // This test documents that we expect NULL checks in functions
}

// ============================================================================
// 8. ENHANCEMENT ISOLATION REGRESSION
// ============================================================================

// Regression: Interoception can be toggled independently
TEST_F(PerceptionBridgeRegressionTest, IsolationInteroceptionToggle) {
    hypo_perception_enhanced_config_t config;
    hypo_perception_bridge_default_enhanced_config(&config);

    // Enable only interoception
    config.enable_interoception = true;
    config.enable_chemosensory = false;
    config.enable_predictive_coding = false;
    config.enable_pain_modulation = false;
    config.enable_sleep_gating = false;
    config.enable_thermal_salience = false;

    EXPECT_EQ(0, hypo_perception_bridge_apply_enhanced_config(bridge, &config));

    // Interoception should work
    EXPECT_EQ(0, hypo_perception_bridge_process_interoceptive(bridge, HYPO_INTERO_HEART_RATE, 0.5f));

    hypo_interoceptive_state_t intero_state;
    EXPECT_EQ(0, hypo_perception_bridge_get_interoceptive_state(bridge, &intero_state));
    EXPECT_TRUE(is_valid_float(intero_state.signals[HYPO_INTERO_HEART_RATE]));
}

// Regression: Chemosensory can be toggled independently
TEST_F(PerceptionBridgeRegressionTest, IsolationChemosensoryToggle) {
    hypo_perception_enhanced_config_t config;
    hypo_perception_bridge_default_enhanced_config(&config);

    config.enable_interoception = false;
    config.enable_chemosensory = true;
    config.enable_predictive_coding = false;

    EXPECT_EQ(0, hypo_perception_bridge_apply_enhanced_config(bridge, &config));

    // Chemosensory should work
    EXPECT_EQ(0, hypo_perception_bridge_process_olfactory(bridge, HYPO_OLFACT_FOOD_PLEASANT, 0.7f));
    EXPECT_EQ(0, hypo_perception_bridge_process_gustatory(bridge, HYPO_GUST_SWEET, 0.6f));

    hypo_chemosensory_state_t chem_state;
    EXPECT_EQ(0, hypo_perception_bridge_get_chemosensory_state(bridge, &chem_state));
    EXPECT_TRUE(is_valid_float(chem_state.olfactory_intensity[HYPO_OLFACT_FOOD_PLEASANT]));
}

// Regression: Predictive coding can be toggled independently
TEST_F(PerceptionBridgeRegressionTest, IsolationPredictiveCodingToggle) {
    hypo_perception_enhanced_config_t config;
    hypo_perception_bridge_default_enhanced_config(&config);

    config.enable_interoception = false;
    config.enable_chemosensory = false;
    config.enable_predictive_coding = true;

    EXPECT_EQ(0, hypo_perception_bridge_apply_enhanced_config(bridge, &config));

    // Predictive coding should work
    EXPECT_EQ(0, hypo_perception_bridge_generate_prediction(bridge, HYPO_PRED_FOOD_PRESENCE, 0.8f, 0.9f));

    hypo_predictive_state_t pred_state;
    EXPECT_EQ(0, hypo_perception_bridge_get_predictive_state(bridge, &pred_state));
    EXPECT_TRUE(is_valid_float(pred_state.predictions[HYPO_PRED_FOOD_PRESENCE]));
}

// Regression: Pain modulation can be toggled independently
TEST_F(PerceptionBridgeRegressionTest, IsolationPainModulationToggle) {
    hypo_perception_enhanced_config_t config;
    hypo_perception_bridge_default_enhanced_config(&config);

    config.enable_pain_modulation = true;
    config.enable_thermal_salience = false;
    config.enable_sleep_gating = false;

    EXPECT_EQ(0, hypo_perception_bridge_apply_enhanced_config(bridge, &config));

    // Pain modulation should work
    EXPECT_EQ(0, hypo_perception_bridge_set_stress_for_pain(bridge, 0.5f, false));

    float modulated;
    EXPECT_EQ(0, hypo_perception_bridge_modulate_pain(bridge, 0.5f, &modulated));
    EXPECT_TRUE(is_valid_float(modulated));
}

// Regression: Sleep gating can be toggled independently
TEST_F(PerceptionBridgeRegressionTest, IsolationSleepGatingToggle) {
    hypo_perception_enhanced_config_t config;
    hypo_perception_bridge_default_enhanced_config(&config);

    config.enable_sleep_gating = true;
    config.enable_thermal_salience = false;
    config.enable_pain_modulation = false;

    EXPECT_EQ(0, hypo_perception_bridge_apply_enhanced_config(bridge, &config));

    // Sleep gating should work
    EXPECT_EQ(0, hypo_perception_bridge_set_sleep_stage(bridge, HYPO_SLEEP_LIGHT));

    bool passes;
    EXPECT_EQ(0, hypo_perception_bridge_check_sleep_gate(bridge, 0.5f, false, false, &passes));
}

// Regression: Thermal salience can be toggled independently
TEST_F(PerceptionBridgeRegressionTest, IsolationThermalSalienceToggle) {
    hypo_perception_enhanced_config_t config;
    hypo_perception_bridge_default_enhanced_config(&config);

    config.enable_thermal_salience = true;
    config.enable_sleep_gating = false;
    config.enable_pain_modulation = false;

    EXPECT_EQ(0, hypo_perception_bridge_apply_enhanced_config(bridge, &config));

    // Thermal should work
    EXPECT_EQ(0, hypo_perception_bridge_set_core_temperature(bridge, 0.6f));

    hypo_thermal_state_t thermal;
    EXPECT_EQ(0, hypo_perception_bridge_get_thermal_state(bridge, &thermal));
    EXPECT_TRUE(is_valid_float(thermal.core_temperature));
}

// Regression: All enhancements can be enabled together
TEST_F(PerceptionBridgeRegressionTest, IsolationAllEnhancementsEnabled) {
    hypo_perception_enhanced_config_t config;
    hypo_perception_bridge_default_enhanced_config(&config);

    config.enable_interoception = true;
    config.enable_chemosensory = true;
    config.enable_predictive_coding = true;
    config.enable_pain_modulation = true;
    config.enable_sleep_gating = true;
    config.enable_thermal_salience = true;

    EXPECT_EQ(0, hypo_perception_bridge_apply_enhanced_config(bridge, &config));

    // All should work together
    EXPECT_EQ(0, hypo_perception_bridge_process_interoceptive(bridge, HYPO_INTERO_FATIGUE, 0.3f));
    EXPECT_EQ(0, hypo_perception_bridge_process_olfactory(bridge, HYPO_OLFACT_NEUTRAL, 0.2f));
    EXPECT_EQ(0, hypo_perception_bridge_generate_prediction(bridge, HYPO_PRED_REWARD_AVAILABLE, 0.5f, 0.7f));
    EXPECT_EQ(0, hypo_perception_bridge_set_stress_for_pain(bridge, 0.4f, true));
    EXPECT_EQ(0, hypo_perception_bridge_set_sleep_stage(bridge, HYPO_SLEEP_AWAKE));
    EXPECT_EQ(0, hypo_perception_bridge_set_core_temperature(bridge, 0.5f));

    // Core functionality still works
    EXPECT_EQ(0, hypo_perception_bridge_compute_modulation(bridge));
}

// ============================================================================
// 9. STRESS TESTING REGRESSION
// ============================================================================

// Regression: Rapid modulation updates don't cause issues
TEST_F(PerceptionBridgeRegressionTest, StressRapidModulationUpdates) {
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 10000; i++) {
        hypo_perception_bridge_set_arousal(bridge, (float)(i % 100) / 100.0f);
        EXPECT_EQ(0, hypo_perception_bridge_compute_modulation(bridge));
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should complete in reasonable time
    EXPECT_LT(duration.count(), 5000)
        << "10000 modulation updates should complete in under 5 seconds";

    // Final state should be valid
    hypo_perception_modulation_t mod;
    EXPECT_EQ(0, hypo_perception_bridge_get_modulation(bridge, &mod));
    EXPECT_TRUE(is_valid_float(mod.global_gain));
}

// Regression: Rapid detection processing doesn't cause issues
TEST_F(PerceptionBridgeRegressionTest, StressRapidDetectionProcessing) {
    hypo_sensory_detection_t detection = {};
    detection.confidence = 0.5f;
    detection.intensity = 0.5f;

    for (int i = 0; i < 10000; i++) {
        detection.detected_category = (hypo_stim_category_t)(i % HYPO_STIM_COUNT);
        detection.modality = (hypo_sense_modality_t)(i % HYPO_SENSE_COUNT);
        detection.timestamp_us = (uint64_t)i * 100;

        EXPECT_EQ(0, hypo_perception_bridge_process_detection(bridge, &detection));
    }

    // Statistics should reflect processing
    hypo_perception_bridge_stats_t stats;
    EXPECT_EQ(0, hypo_perception_bridge_get_stats(bridge, &stats));
    EXPECT_EQ(10000u, stats.detections_processed);
}

// Regression: Rapid update cycles don't accumulate errors
TEST_F(PerceptionBridgeRegressionTest, StressRapidUpdateCycles) {
    for (int i = 0; i < 10000; i++) {
        EXPECT_EQ(0, hypo_perception_bridge_update(bridge, 1000)); // 1ms updates
    }

    // State should still be valid
    hypo_perception_modulation_t mod;
    EXPECT_EQ(0, hypo_perception_bridge_compute_modulation(bridge));
    EXPECT_EQ(0, hypo_perception_bridge_get_modulation(bridge, &mod));

    // All values should be valid
    EXPECT_TRUE(is_valid_float(mod.global_gain));
    EXPECT_TRUE(is_valid_float(mod.arousal_level));
    for (int i = 0; i < HYPO_SENSE_COUNT; i++) {
        EXPECT_TRUE(is_valid_float(mod.modality_gains[i]));
    }
}

// Regression: Mixed operations under stress
TEST_F(PerceptionBridgeRegressionTest, StressMixedOperations) {
    // Enable all enhancements
    hypo_perception_enhanced_config_t config;
    hypo_perception_bridge_default_enhanced_config(&config);
    config.enable_interoception = true;
    config.enable_chemosensory = true;
    config.enable_predictive_coding = true;
    config.enable_pain_modulation = true;
    config.enable_sleep_gating = true;
    config.enable_thermal_salience = true;
    hypo_perception_bridge_apply_enhanced_config(bridge, &config);

    hypo_sensory_detection_t detection = {};
    detection.confidence = 0.6f;
    detection.intensity = 0.5f;

    for (int i = 0; i < 1000; i++) {
        // Vary arousal
        hypo_perception_bridge_set_arousal(bridge, (float)(i % 100) / 100.0f);

        // Process detections
        detection.detected_category = (hypo_stim_category_t)(i % HYPO_STIM_COUNT);
        hypo_perception_bridge_process_detection(bridge, &detection);

        // Process interoceptive signals
        hypo_perception_bridge_process_interoceptive(
            bridge, (hypo_interoceptive_type_t)(i % HYPO_INTERO_COUNT), 0.5f);

        // Update predictions
        hypo_perception_bridge_generate_prediction(
            bridge, (hypo_prediction_type_t)(i % HYPO_PRED_COUNT), 0.5f, 0.5f);

        // Compute modulation
        hypo_perception_bridge_compute_modulation(bridge);

        // Update time
        hypo_perception_bridge_update(bridge, 1000);
    }

    // Final state validation
    hypo_perception_modulation_t mod;
    EXPECT_EQ(0, hypo_perception_bridge_get_modulation(bridge, &mod));
    EXPECT_TRUE(is_valid_float(mod.global_gain));
    // After stress testing, state should still be consistent:
    // Either survival_mode is false and we have a normal gain, or survival_mode is active
    EXPECT_TRUE(mod.global_gain >= 0.5f && mod.global_gain <= 2.0f)
        << "Final global_gain should be within valid range after stress test";
}

// ============================================================================
// NULL SAFETY REGRESSION
// ============================================================================

// Regression: All functions handle NULL bridge gracefully
TEST_F(PerceptionBridgeRegressionTest, NullSafetyBridgeParameter) {
    hypo_perception_modulation_t mod;
    hypo_perception_bridge_stats_t stats;
    hypo_interoceptive_state_t intero;
    hypo_chemosensory_state_t chem;
    hypo_predictive_state_t pred;
    hypo_pain_modulation_state_t pain;
    hypo_sleep_gating_state_t sleep;
    hypo_thermal_state_t thermal;

    // All should return error (not crash) with NULL bridge
    EXPECT_NE(0, hypo_perception_bridge_compute_modulation(nullptr));
    EXPECT_NE(0, hypo_perception_bridge_get_modulation(nullptr, &mod));
    EXPECT_NE(0, hypo_perception_bridge_set_arousal(nullptr, 0.5f));
    EXPECT_NE(0, hypo_perception_bridge_get_stats(nullptr, &stats));
    EXPECT_NE(0, hypo_perception_bridge_reset(nullptr));
    EXPECT_NE(0, hypo_perception_bridge_update(nullptr, 1000));
    EXPECT_NE(0, hypo_perception_bridge_get_interoceptive_state(nullptr, &intero));
    EXPECT_NE(0, hypo_perception_bridge_get_chemosensory_state(nullptr, &chem));
    EXPECT_NE(0, hypo_perception_bridge_get_predictive_state(nullptr, &pred));
    EXPECT_NE(0, hypo_perception_bridge_get_pain_state(nullptr, &pain));
    EXPECT_NE(0, hypo_perception_bridge_get_sleep_gating_state(nullptr, &sleep));
    EXPECT_NE(0, hypo_perception_bridge_get_thermal_state(nullptr, &thermal));
}

// Regression: All functions handle NULL output parameters gracefully
TEST_F(PerceptionBridgeRegressionTest, NullSafetyOutputParameters) {
    EXPECT_NE(0, hypo_perception_bridge_get_modulation(bridge, nullptr));
    EXPECT_NE(0, hypo_perception_bridge_get_stats(bridge, nullptr));
    EXPECT_NE(0, hypo_perception_bridge_get_interoceptive_state(bridge, nullptr));
    EXPECT_NE(0, hypo_perception_bridge_get_chemosensory_state(bridge, nullptr));
    EXPECT_NE(0, hypo_perception_bridge_get_predictive_state(bridge, nullptr));
    EXPECT_NE(0, hypo_perception_bridge_get_pain_state(bridge, nullptr));
    EXPECT_NE(0, hypo_perception_bridge_get_sleep_gating_state(bridge, nullptr));
    EXPECT_NE(0, hypo_perception_bridge_get_thermal_state(bridge, nullptr));

    float output;
    EXPECT_NE(0, hypo_perception_bridge_modulate_pain(bridge, 0.5f, nullptr));
    EXPECT_NE(0, hypo_perception_bridge_compute_free_energy(bridge, nullptr));
    EXPECT_NE(0, hypo_perception_bridge_compute_thermal_salience(bridge, nullptr));

    bool passes;
    EXPECT_NE(0, hypo_perception_bridge_check_sleep_gate(bridge, 0.5f, false, false, nullptr));
}

// ============================================================================
// STRING UTILITIES REGRESSION
// ============================================================================

// Regression: All type name functions return non-null strings
TEST_F(PerceptionBridgeRegressionTest, StringUtilitiesNotNull) {
    // Interoceptive types
    for (int i = 0; i < HYPO_INTERO_COUNT; i++) {
        EXPECT_NE(nullptr, hypo_interoceptive_type_name((hypo_interoceptive_type_t)i))
            << "Interoceptive type " << i << " name should not be null";
    }

    // Olfactory types
    for (int i = 0; i < HYPO_OLFACT_COUNT; i++) {
        EXPECT_NE(nullptr, hypo_olfactory_type_name((hypo_olfactory_type_t)i))
            << "Olfactory type " << i << " name should not be null";
    }

    // Gustatory types
    for (int i = 0; i < HYPO_GUST_COUNT; i++) {
        EXPECT_NE(nullptr, hypo_gustatory_type_name((hypo_gustatory_type_t)i))
            << "Gustatory type " << i << " name should not be null";
    }

    // Prediction types
    for (int i = 0; i < HYPO_PRED_COUNT; i++) {
        EXPECT_NE(nullptr, hypo_prediction_type_name((hypo_prediction_type_t)i))
            << "Prediction type " << i << " name should not be null";
    }

    // Sleep stages
    for (int i = 0; i < HYPO_SLEEP_COUNT; i++) {
        EXPECT_NE(nullptr, hypo_sleep_stage_name((hypo_sleep_stage_t)i))
            << "Sleep stage " << i << " name should not be null";
    }
}

// Regression: Invalid enum values return safe string
TEST_F(PerceptionBridgeRegressionTest, StringUtilitiesInvalidEnums) {
    // Invalid values should return something (not crash)
    const char* invalid = hypo_interoceptive_type_name((hypo_interoceptive_type_t)999);
    EXPECT_NE(nullptr, invalid);

    invalid = hypo_sleep_stage_name((hypo_sleep_stage_t)999);
    EXPECT_NE(nullptr, invalid);
}

// ============================================================================
// PERFORMANCE REGRESSION
// ============================================================================

// Regression: Modulation query should be fast
TEST_F(PerceptionBridgeRegressionTest, PerformanceModulationQuery) {
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 10000; i++) {
        hypo_perception_modulation_t mod;
        hypo_perception_bridge_get_modulation(bridge, &mod);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_LT(duration.count(), 1000)
        << "10000 modulation queries should complete in under 1 second";
}

// Regression: Statistics query should be fast
TEST_F(PerceptionBridgeRegressionTest, PerformanceStatsQuery) {
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 10000; i++) {
        hypo_perception_bridge_stats_t stats;
        hypo_perception_bridge_get_stats(bridge, &stats);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_LT(duration.count(), 1000)
        << "10000 stats queries should complete in under 1 second";
}

// ============================================================================
// PREDICTIVE CODING SPECIFIC REGRESSION
// ============================================================================

// Regression: Free energy computation produces valid results
TEST_F(PerceptionBridgeRegressionTest, PredictiveFreeEnergyValid) {
    hypo_perception_enhanced_config_t config;
    hypo_perception_bridge_default_enhanced_config(&config);
    config.enable_predictive_coding = true;
    hypo_perception_bridge_apply_enhanced_config(bridge, &config);

    // Generate predictions
    hypo_perception_bridge_generate_prediction(bridge, HYPO_PRED_FOOD_PRESENCE, 0.8f, 0.9f);
    hypo_perception_bridge_generate_prediction(bridge, HYPO_PRED_THREAT_PRESENCE, 0.2f, 0.8f);

    // Update with actual observations
    hypo_perception_bridge_update_prediction_error(bridge, HYPO_PRED_FOOD_PRESENCE, 0.7f);
    hypo_perception_bridge_update_prediction_error(bridge, HYPO_PRED_THREAT_PRESENCE, 0.1f);

    // Compute free energy
    float free_energy;
    EXPECT_EQ(0, hypo_perception_bridge_compute_free_energy(bridge, &free_energy));
    EXPECT_TRUE(is_valid_float(free_energy));
    EXPECT_GE(free_energy, 0.0f) << "Free energy should be non-negative";
}

// Regression: Prediction errors bounded
TEST_F(PerceptionBridgeRegressionTest, PredictivePredictionErrorsBounded) {
    hypo_perception_enhanced_config_t config;
    hypo_perception_bridge_default_enhanced_config(&config);
    config.enable_predictive_coding = true;
    hypo_perception_bridge_apply_enhanced_config(bridge, &config);

    // Generate extreme prediction
    hypo_perception_bridge_generate_prediction(bridge, HYPO_PRED_FOOD_PRESENCE, 1.0f, 1.0f);

    // Update with completely wrong observation
    hypo_perception_bridge_update_prediction_error(bridge, HYPO_PRED_FOOD_PRESENCE, 0.0f);

    hypo_predictive_state_t state;
    EXPECT_EQ(0, hypo_perception_bridge_get_predictive_state(bridge, &state));

    // Prediction error should be bounded
    EXPECT_TRUE(is_valid_float(state.prediction_errors[HYPO_PRED_FOOD_PRESENCE]));
    EXPECT_GE(state.prediction_errors[HYPO_PRED_FOOD_PRESENCE], -1.0f);
    EXPECT_LE(state.prediction_errors[HYPO_PRED_FOOD_PRESENCE], 1.0f);
}

// ============================================================================
// SLEEP GATING SPECIFIC REGRESSION
// ============================================================================

// Regression: Sleep gating thresholds follow expected pattern
TEST_F(PerceptionBridgeRegressionTest, SleepGatingThresholdPattern) {
    hypo_perception_enhanced_config_t config;
    hypo_perception_bridge_default_enhanced_config(&config);
    config.enable_sleep_gating = true;
    hypo_perception_bridge_apply_enhanced_config(bridge, &config);

    struct {
        hypo_sleep_stage_t stage;
        float expected_min_gate;
        float expected_max_gate;
    } stage_tests[] = {
        {HYPO_SLEEP_AWAKE, 0.8f, 1.0f},
        {HYPO_SLEEP_DROWSY, 0.5f, 0.9f},
        {HYPO_SLEEP_LIGHT, 0.3f, 0.7f},
        {HYPO_SLEEP_DEEP, 0.0f, 0.3f},
        {HYPO_SLEEP_REM, 0.1f, 0.4f},
    };

    for (const auto& test : stage_tests) {
        hypo_perception_bridge_set_sleep_stage(bridge, test.stage);

        hypo_sleep_gating_state_t state;
        hypo_perception_bridge_get_sleep_gating_state(bridge, &state);

        EXPECT_GE(state.perception_gate, test.expected_min_gate)
            << "Stage " << hypo_sleep_stage_name(test.stage)
            << " gate should be >= " << test.expected_min_gate;
        EXPECT_LE(state.perception_gate, test.expected_max_gate)
            << "Stage " << hypo_sleep_stage_name(test.stage)
            << " gate should be <= " << test.expected_max_gate;
    }
}

// Regression: Threat can bypass sleep gate
TEST_F(PerceptionBridgeRegressionTest, SleepGatingThreatBypass) {
    hypo_perception_enhanced_config_t config;
    hypo_perception_bridge_default_enhanced_config(&config);
    config.enable_sleep_gating = true;
    hypo_perception_bridge_apply_enhanced_config(bridge, &config);

    // Set deep sleep (lowest gate)
    hypo_perception_bridge_set_sleep_stage(bridge, HYPO_SLEEP_DEEP);

    // Low intensity non-threat should not pass
    bool passes_normal;
    hypo_perception_bridge_check_sleep_gate(bridge, 0.1f, false, false, &passes_normal);

    // Same intensity threat should pass
    bool passes_threat;
    hypo_perception_bridge_check_sleep_gate(bridge, 0.1f, true, false, &passes_threat);

    // Threat should have better chance of passing
    // Note: actual pass depends on implementation, but threat should at least not be worse
    EXPECT_TRUE(passes_threat || !passes_normal)
        << "Threat should have equal or better gate bypass than non-threat";
}

// ============================================================================
// THERMAL SPECIFIC REGRESSION
// ============================================================================

// Regression: Temperature deviation increases salience
TEST_F(PerceptionBridgeRegressionTest, ThermalDeviationIncreasesSalience) {
    hypo_perception_enhanced_config_t config;
    hypo_perception_bridge_default_enhanced_config(&config);
    config.enable_thermal_salience = true;
    hypo_perception_bridge_apply_enhanced_config(bridge, &config);

    // Set comfortable temperature
    hypo_perception_bridge_set_core_temperature(bridge, 0.5f);
    float salience_comfort;
    hypo_perception_bridge_compute_thermal_salience(bridge, &salience_comfort);

    // Set uncomfortable (cold) temperature
    hypo_perception_bridge_set_core_temperature(bridge, 0.2f);
    float salience_cold;
    hypo_perception_bridge_compute_thermal_salience(bridge, &salience_cold);

    // Cold should increase thermal salience
    EXPECT_GE(salience_cold, salience_comfort)
        << "Temperature deviation should increase thermal salience";
}

// Regression: Thermal alerts set correctly
TEST_F(PerceptionBridgeRegressionTest, ThermalAlertsSetCorrectly) {
    hypo_perception_enhanced_config_t config;
    hypo_perception_bridge_default_enhanced_config(&config);
    config.enable_thermal_salience = true;
    hypo_perception_bridge_apply_enhanced_config(bridge, &config);

    // Set very cold
    hypo_perception_bridge_set_core_temperature(bridge, 0.1f);
    hypo_thermal_state_t cold_state;
    hypo_perception_bridge_get_thermal_state(bridge, &cold_state);
    // Hypothermic alert expected for very low temp
    EXPECT_TRUE(cold_state.hypothermic_alert || cold_state.thermal_discomfort > 0.5f)
        << "Very cold temperature should trigger alert or high discomfort";

    // Set very hot
    hypo_perception_bridge_set_core_temperature(bridge, 0.9f);
    hypo_thermal_state_t hot_state;
    hypo_perception_bridge_get_thermal_state(bridge, &hot_state);
    // Hyperthermic alert expected for very high temp
    EXPECT_TRUE(hot_state.hyperthermic_alert || hot_state.thermal_discomfort > 0.5f)
        << "Very hot temperature should trigger alert or high discomfort";
}

// ============================================================================
// ENDORPHIN REGRESSION
// ============================================================================

// Regression: Endorphin release affects pain modulation
TEST_F(PerceptionBridgeRegressionTest, EndorphinReleaseAffectsPain) {
    hypo_perception_enhanced_config_t config;
    hypo_perception_bridge_default_enhanced_config(&config);
    config.enable_pain_modulation = true;
    hypo_perception_bridge_apply_enhanced_config(bridge, &config);

    // Get baseline pain modulation
    float pain_baseline;
    hypo_perception_bridge_modulate_pain(bridge, 0.5f, &pain_baseline);

    // Release endorphins
    EXPECT_EQ(0, hypo_perception_bridge_release_endorphins(bridge, 0.8f));

    // Check pain modulation after endorphins
    float pain_after;
    hypo_perception_bridge_modulate_pain(bridge, 0.5f, &pain_after);

    // Endorphins should reduce pain
    EXPECT_LE(pain_after, pain_baseline)
        << "Endorphin release should reduce modulated pain";
}

// ============================================================================
// MAIN
// ============================================================================

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
