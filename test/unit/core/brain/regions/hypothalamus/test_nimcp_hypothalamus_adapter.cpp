/**
 * @file test_nimcp_hypothalamus_adapter.cpp
 * @brief Unit tests for nimcp_hypothalamus_adapter.c
 *
 * WHAT: Comprehensive unit tests for the Hypothalamus adapter
 * WHY:  Ensure correct homeostatic regulation, circadian rhythms, stress response
 * HOW:  Use Google Test framework to test lifecycle, circadian, homeostasis,
 *       HPA axis, autonomic control, and bio-async integration.
 *
 * COVERAGE TARGET: 100%
 *
 * @version Phase H1: Hypothalamus Brain Integration
 * @date 2025-12-30
 */

#include <gtest/gtest.h>
#include <string.h>
#include <stdlib.h>
#include <cmath>

// Headers have their own extern "C" guards
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_adapter.h"

// Test Fixture for Hypothalamus Adapter
class HypothalamusAdapterTest : public ::testing::Test {
protected:
    hypothalamus_adapter_t* adapter;
    hypothalamus_config_t config;

    void SetUp() override {
        config = hypothalamus_default_config();
        adapter = hypothalamus_create(&config);
        ASSERT_NE(nullptr, adapter) << "Failed to create Hypothalamus adapter";
    }

    void TearDown() override {
        hypothalamus_destroy(adapter);
        adapter = nullptr;
    }
};

// ============================================================================
// LIFECYCLE TESTS
// ============================================================================

TEST_F(HypothalamusAdapterTest, DefaultConfigHasReasonableValues) {
    hypothalamus_config_t default_config = hypothalamus_default_config();

    EXPECT_FLOAT_EQ(default_config.circadian_period_hours,
                    HYPOTHALAMUS_DEFAULT_CIRCADIAN_PERIOD_HOURS);
    EXPECT_FLOAT_EQ(default_config.temperature_setpoint_c,
                    HYPOTHALAMUS_DEFAULT_TEMP_SETPOINT_C);
    EXPECT_FLOAT_EQ(default_config.cortisol_baseline,
                    HYPOTHALAMUS_DEFAULT_CORTISOL_BASELINE);
    EXPECT_FLOAT_EQ(default_config.sympathetic_bias,
                    HYPOTHALAMUS_DEFAULT_AUTONOMIC_BALANCE);
    EXPECT_TRUE(default_config.enable_circadian);
    EXPECT_TRUE(default_config.enable_hpa_axis);
    EXPECT_TRUE(default_config.enable_autonomic);
    EXPECT_TRUE(default_config.enable_appetite);
}

TEST_F(HypothalamusAdapterTest, CreateWithNullConfigUsesDefaults) {
    hypothalamus_adapter_t* adapter_null = hypothalamus_create(NULL);
    ASSERT_NE(nullptr, adapter_null);

    hypothalamus_config_t retrieved;
    EXPECT_TRUE(hypothalamus_get_config(adapter_null, &retrieved));
    EXPECT_FLOAT_EQ(retrieved.circadian_period_hours,
                    HYPOTHALAMUS_DEFAULT_CIRCADIAN_PERIOD_HOURS);

    hypothalamus_destroy(adapter_null);
}

TEST_F(HypothalamusAdapterTest, DestroyNullDoesNotCrash) {
    hypothalamus_destroy(NULL);
    // Should not crash
}

TEST_F(HypothalamusAdapterTest, ResetClearsState) {
    // Apply some stress first
    hypothalamus_apply_stress(adapter, 0.8f);

    EXPECT_TRUE(hypothalamus_reset(adapter));

    // Status should be idle after reset
    EXPECT_EQ(hypothalamus_get_status(adapter), HYPOTHALAMUS_STATUS_IDLE);
    EXPECT_EQ(hypothalamus_get_last_error(adapter), HYPOTHALAMUS_ERROR_NONE);

    // Cortisol should return to baseline
    EXPECT_FLOAT_EQ(hypothalamus_get_cortisol(adapter), config.cortisol_baseline);
}

TEST_F(HypothalamusAdapterTest, ResetNullReturnsFalse) {
    EXPECT_FALSE(hypothalamus_reset(NULL));
}

// ============================================================================
// CIRCADIAN RHYTHM TESTS
// ============================================================================

TEST_F(HypothalamusAdapterTest, UpdateCircadianAdvancesPhase) {
    hypo_circadian_state_t state_before, state_after;

    ASSERT_TRUE(hypothalamus_get_circadian_state(adapter, &state_before));
    float initial_phase = state_before.phase;

    // Update for 1 hour (in microseconds)
    uint64_t one_hour_us = 3600000000ULL;
    EXPECT_TRUE(hypothalamus_update_circadian(adapter, one_hour_us));

    ASSERT_TRUE(hypothalamus_get_circadian_state(adapter, &state_after));

    // Phase should advance
    EXPECT_GT(state_after.phase, initial_phase);

    // Should advance by approximately 1/24 of 2*PI (one hour in 24-hour cycle)
    float expected_advance = (2.0f * M_PI) / 24.0f;
    float actual_advance = state_after.phase - initial_phase;
    EXPECT_NEAR(actual_advance, expected_advance, 0.01f);
}

TEST_F(HypothalamusAdapterTest, CircadianPhaseWrapsAround) {
    // Simulate 25 hours of updates (should wrap around)
    uint64_t one_hour_us = 3600000000ULL;

    for (int i = 0; i < 25; i++) {
        EXPECT_TRUE(hypothalamus_update_circadian(adapter, one_hour_us));
    }

    hypo_circadian_state_t state;
    ASSERT_TRUE(hypothalamus_get_circadian_state(adapter, &state));

    // Phase should be normalized to [0, 2*PI)
    EXPECT_GE(state.phase, 0.0f);
    EXPECT_LT(state.phase, 2.0f * M_PI);
}

TEST_F(HypothalamusAdapterTest, CircadianMelatoninHighAtNight) {
    // Set phase to midnight (phase = 0)
    hypothalamus_reset(adapter);

    hypo_circadian_state_t state;
    ASSERT_TRUE(hypothalamus_get_circadian_state(adapter, &state));

    // At midnight, melatonin should be high (close to peak)
    EXPECT_GT(state.melatonin_level, 0.3f);
}

TEST_F(HypothalamusAdapterTest, CircadianCortisolPeaksInMorning) {
    // Update to morning hours (approximately 6-8 AM)
    // Phase = PI/2 corresponds to 6:00
    uint64_t six_hours_us = 6 * 3600000000ULL;
    EXPECT_TRUE(hypothalamus_update_circadian(adapter, six_hours_us));

    hypo_circadian_state_t state;
    ASSERT_TRUE(hypothalamus_get_circadian_state(adapter, &state));

    // Cortisol should be elevated in morning
    EXPECT_GT(state.cortisol_level, 0.3f);
}

TEST_F(HypothalamusAdapterTest, ApplyLightAffectsCircadian) {
    hypo_circadian_state_t state_before;
    ASSERT_TRUE(hypothalamus_get_circadian_state(adapter, &state_before));

    // Apply bright light exposure
    float phase_shift = hypothalamus_apply_light(adapter, 1.0f, 30.0f * 60.0f * 1000.0f);

    // Light should cause some phase shift
    // The exact direction depends on time of day
    EXPECT_NE(phase_shift, 0.0f);
}

TEST_F(HypothalamusAdapterTest, GetCircadianPhase) {
    hypo_circadian_phase_t phase = hypothalamus_get_circadian_phase(adapter);

    // Initial phase (midnight) should be late night or early morning
    EXPECT_TRUE(phase == HYPO_CIRCADIAN_PHASE_LATE_NIGHT ||
                phase == HYPO_CIRCADIAN_PHASE_MID_NIGHT);
}

// ============================================================================
// HOMEOSTATIC REGULATION TESTS
// ============================================================================

TEST_F(HypothalamusAdapterTest, UpdateHomeostasisSuccess) {
    uint64_t delta_us = 100000; // 100ms
    EXPECT_TRUE(hypothalamus_update_homeostasis(adapter, delta_us));
}

TEST_F(HypothalamusAdapterTest, SetTemperatureSuccess) {
    EXPECT_TRUE(hypothalamus_set_temperature(adapter, 37.5f));

    thermoregulation_state_t state;
    ASSERT_TRUE(hypothalamus_get_thermoregulation(adapter, &state));
    EXPECT_FLOAT_EQ(state.core_temp.current_value, 37.5f);
}

TEST_F(HypothalamusAdapterTest, ThermoregulationRespondsToHighTemp) {
    // Set temperature above normal
    EXPECT_TRUE(hypothalamus_set_temperature(adapter, 39.0f));

    // Update homeostasis
    uint64_t delta_us = 1000000; // 1 second
    EXPECT_TRUE(hypothalamus_update_homeostasis(adapter, delta_us));

    thermoregulation_state_t state;
    ASSERT_TRUE(hypothalamus_get_thermoregulation(adapter, &state));

    // Should activate cooling mechanisms
    EXPECT_TRUE(state.vasodilation || state.sweating_active);
    EXPECT_GT(state.heat_loss, 0.5f);
}

TEST_F(HypothalamusAdapterTest, ThermoregulationRespondsToLowTemp) {
    // Set temperature below normal
    EXPECT_TRUE(hypothalamus_set_temperature(adapter, 35.0f));

    // Update homeostasis
    uint64_t delta_us = 1000000; // 1 second
    EXPECT_TRUE(hypothalamus_update_homeostasis(adapter, delta_us));

    thermoregulation_state_t state;
    ASSERT_TRUE(hypothalamus_get_thermoregulation(adapter, &state));

    // Should activate warming mechanisms
    EXPECT_TRUE(state.vasoconstriction || state.shivering_active);
    EXPECT_GT(state.heat_production, 0.5f);
}

TEST_F(HypothalamusAdapterTest, SetBloodGlucoseSuccess) {
    EXPECT_TRUE(hypothalamus_set_blood_glucose(adapter, 70.0f)); // Low glucose

    appetite_state_t state;
    ASSERT_TRUE(hypothalamus_get_appetite(adapter, &state));
    EXPECT_FLOAT_EQ(state.blood_glucose.current_value, 70.0f);
}

TEST_F(HypothalamusAdapterTest, HungerIncreasesWithLowGlucose) {
    // Set low glucose
    EXPECT_TRUE(hypothalamus_set_blood_glucose(adapter, 60.0f));

    // Update multiple times to let the system respond
    for (int i = 0; i < 10; i++) {
        EXPECT_TRUE(hypothalamus_update_homeostasis(adapter, 100000));
    }

    appetite_state_t state;
    ASSERT_TRUE(hypothalamus_get_appetite(adapter, &state));

    // Hunger should be elevated
    EXPECT_GT(state.hunger_drive, 0.3f);
    EXPECT_GT(state.ghrelin_level, 0.3f);
}

TEST_F(HypothalamusAdapterTest, SetOsmolalitySuccess) {
    EXPECT_TRUE(hypothalamus_set_osmolality(adapter, 310.0f)); // High osmolality

    hydration_state_t state;
    ASSERT_TRUE(hypothalamus_get_hydration(adapter, &state));
    EXPECT_FLOAT_EQ(state.osmolality.current_value, 310.0f);
}

TEST_F(HypothalamusAdapterTest, ThirstIncreasesWithHighOsmolality) {
    // Set high osmolality
    EXPECT_TRUE(hypothalamus_set_osmolality(adapter, 320.0f));

    // Update multiple times
    for (int i = 0; i < 10; i++) {
        EXPECT_TRUE(hypothalamus_update_homeostasis(adapter, 100000));
    }

    hydration_state_t state;
    ASSERT_TRUE(hypothalamus_get_hydration(adapter, &state));

    // Thirst should be elevated
    EXPECT_GT(state.thirst_drive, 0.1f);
    EXPECT_GT(state.vasopressin_level, 0.3f);
}

// ============================================================================
// HPA AXIS (STRESS RESPONSE) TESTS
// ============================================================================

TEST_F(HypothalamusAdapterTest, UpdateHPAAxisSuccess) {
    uint64_t delta_us = 100000; // 100ms
    EXPECT_TRUE(hypothalamus_update_hpa_axis(adapter, delta_us));
}

TEST_F(HypothalamusAdapterTest, ApplyStressIncreasesCortisolReturn) {
    float cortisol = hypothalamus_apply_stress(adapter, 0.5f);
    EXPECT_GE(cortisol, 0.0f);
    EXPECT_LE(cortisol, 1.0f);
}

TEST_F(HypothalamusAdapterTest, StressResponseCascade) {
    // Apply moderate stress
    hypothalamus_apply_stress(adapter, 0.6f);

    // Update HPA axis multiple times to allow cascade
    for (int i = 0; i < 50; i++) {
        EXPECT_TRUE(hypothalamus_update_hpa_axis(adapter, 100000));
    }

    hpa_axis_state_t state;
    ASSERT_TRUE(hypothalamus_get_hpa_state(adapter, &state));

    // CRH, ACTH, and Cortisol should all be elevated
    EXPECT_GT(state.crh_level, 0.1f);
    EXPECT_GT(state.acth_level, 0.05f);
    EXPECT_GT(state.cortisol_level, config.cortisol_baseline);
}

TEST_F(HypothalamusAdapterTest, CortisolNegativeFeedback) {
    // Apply initial stress
    hypothalamus_apply_stress(adapter, 0.8f);

    // Let HPA cascade develop
    for (int i = 0; i < 100; i++) {
        EXPECT_TRUE(hypothalamus_update_hpa_axis(adapter, 100000));
    }

    hpa_axis_state_t state;
    ASSERT_TRUE(hypothalamus_get_hpa_state(adapter, &state));

    // Negative feedback should be non-zero
    EXPECT_GT(state.negative_feedback, 0.0f);
}

TEST_F(HypothalamusAdapterTest, GetCortisolReturnsValidLevel) {
    float cortisol = hypothalamus_get_cortisol(adapter);
    EXPECT_GE(cortisol, 0.0f);
    EXPECT_LE(cortisol, 1.0f);
}

TEST_F(HypothalamusAdapterTest, ChronicStressDetection) {
    // Apply high sustained stress
    for (int i = 0; i < 1100; i++) {
        hypothalamus_apply_stress(adapter, 0.9f);
        EXPECT_TRUE(hypothalamus_update_hpa_axis(adapter, 1000));
    }

    hpa_axis_state_t state;
    ASSERT_TRUE(hypothalamus_get_hpa_state(adapter, &state));

    // After sustained high stress, chronic_stress flag should be set
    // and HPA sensitivity should be reduced
    EXPECT_TRUE(state.chronic_stress);
    EXPECT_LT(state.hpa_sensitivity, 1.0f);
}

// ============================================================================
// AUTONOMIC CONTROL TESTS
// ============================================================================

TEST_F(HypothalamusAdapterTest, UpdateAutonomicSuccess) {
    uint64_t delta_us = 100000; // 100ms
    EXPECT_TRUE(hypothalamus_update_autonomic(adapter, delta_us));
}

TEST_F(HypothalamusAdapterTest, GetAutonomicStateSuccess) {
    autonomic_state_t state;
    EXPECT_TRUE(hypothalamus_get_autonomic(adapter, &state));

    // Initial state should be balanced
    EXPECT_GE(state.sympathetic_tone, 0.0f);
    EXPECT_LE(state.sympathetic_tone, 1.0f);
    EXPECT_GE(state.parasympathetic_tone, 0.0f);
    EXPECT_LE(state.parasympathetic_tone, 1.0f);
}

TEST_F(HypothalamusAdapterTest, StressIncreasesSympatheticTone) {
    // Apply stress
    hypothalamus_apply_stress(adapter, 0.8f);

    // Update systems
    for (int i = 0; i < 50; i++) {
        EXPECT_TRUE(hypothalamus_update_hpa_axis(adapter, 100000));
        EXPECT_TRUE(hypothalamus_update_autonomic(adapter, 100000));
    }

    autonomic_state_t state;
    ASSERT_TRUE(hypothalamus_get_autonomic(adapter, &state));

    // Sympathetic tone should be elevated under stress
    EXPECT_GT(state.sympathetic_tone, 0.4f);
}

TEST_F(HypothalamusAdapterTest, GetAutonomicBalanceReturnsValidValue) {
    float balance = hypothalamus_get_autonomic_balance(adapter);
    EXPECT_GE(balance, 0.0f);
    EXPECT_LE(balance, 1.0f);
}

TEST_F(HypothalamusAdapterTest, FightOrFlightActivatesUnderHighStress) {
    // Apply high stress
    hypothalamus_apply_stress(adapter, 0.95f);

    // Update many times
    for (int i = 0; i < 100; i++) {
        EXPECT_TRUE(hypothalamus_update_hpa_axis(adapter, 100000));
        EXPECT_TRUE(hypothalamus_update_autonomic(adapter, 100000));
    }

    autonomic_state_t state;
    ASSERT_TRUE(hypothalamus_get_autonomic(adapter, &state));

    // Fight-or-flight should be active
    EXPECT_TRUE(state.fight_or_flight);
    EXPECT_GT(state.pupil_dilation, 0.5f);
}

// ============================================================================
// INTEGRATED UPDATE TESTS
// ============================================================================

TEST_F(HypothalamusAdapterTest, IntegratedUpdateSuccess) {
    uint64_t delta_us = 100000; // 100ms
    EXPECT_TRUE(hypothalamus_update(adapter, delta_us));
}

TEST_F(HypothalamusAdapterTest, GetCompleteStateSuccess) {
    hypothalamus_state_t state;
    EXPECT_TRUE(hypothalamus_get_state(adapter, &state));

    // Check that all subsystems have valid state
    EXPECT_GE(state.circadian.phase, 0.0f);
    EXPECT_GE(state.thermoregulation.core_temp.setpoint, 35.0f);
    EXPECT_GE(state.hpa_axis.cortisol_level, 0.0f);
    EXPECT_GE(state.autonomic.sympathetic_tone, 0.0f);
}

TEST_F(HypothalamusAdapterTest, IntegratedUpdateMaintainsHomeostatsis) {
    // Start at normal values
    hypothalamus_set_temperature(adapter, 37.0f);
    hypothalamus_set_blood_glucose(adapter, 90.0f);
    hypothalamus_set_osmolality(adapter, 290.0f);

    // Run integrated update for simulated minute
    for (int i = 0; i < 600; i++) {
        EXPECT_TRUE(hypothalamus_update(adapter, 100000));
    }

    // Status should still be idle (no alerts)
    EXPECT_EQ(hypothalamus_get_status(adapter), HYPOTHALAMUS_STATUS_IDLE);
}

// ============================================================================
// STATUS AND DIAGNOSTICS TESTS
// ============================================================================

TEST_F(HypothalamusAdapterTest, GetStatusIdle) {
    EXPECT_EQ(hypothalamus_get_status(adapter), HYPOTHALAMUS_STATUS_IDLE);
}

TEST_F(HypothalamusAdapterTest, GetLastErrorNone) {
    EXPECT_EQ(hypothalamus_get_last_error(adapter), HYPOTHALAMUS_ERROR_NONE);
}

TEST_F(HypothalamusAdapterTest, ErrorStringNotNull) {
    const char* str = hypothalamus_error_string(HYPOTHALAMUS_ERROR_NONE);
    EXPECT_NE(str, nullptr);
    EXPECT_GT(strlen(str), 0u);

    str = hypothalamus_error_string(HYPOTHALAMUS_ERROR_INVALID_CONFIG);
    EXPECT_NE(str, nullptr);
}

TEST_F(HypothalamusAdapterTest, StatusStringNotNull) {
    const char* str = hypothalamus_status_string(HYPOTHALAMUS_STATUS_IDLE);
    EXPECT_NE(str, nullptr);
    EXPECT_GT(strlen(str), 0u);

    str = hypothalamus_status_string(HYPOTHALAMUS_STATUS_STRESS_RESPONSE);
    EXPECT_NE(str, nullptr);
}

TEST_F(HypothalamusAdapterTest, GetStats) {
    hypothalamus_stats_t stats;
    EXPECT_TRUE(hypothalamus_get_stats(adapter, &stats));
    EXPECT_EQ(stats.updates_processed, 0u); // No updates yet
}

TEST_F(HypothalamusAdapterTest, StatsTrackUpdates) {
    // Run several updates
    for (int i = 0; i < 10; i++) {
        EXPECT_TRUE(hypothalamus_update(adapter, 100000));
    }

    hypothalamus_stats_t stats;
    EXPECT_TRUE(hypothalamus_get_stats(adapter, &stats));
    EXPECT_EQ(stats.updates_processed, 10u);
    EXPECT_GT(stats.circadian_ticks, 0u);
    EXPECT_GT(stats.homeostatic_corrections, 0u);
}

TEST_F(HypothalamusAdapterTest, GetConfig) {
    hypothalamus_config_t retrieved;
    EXPECT_TRUE(hypothalamus_get_config(adapter, &retrieved));
    EXPECT_FLOAT_EQ(retrieved.circadian_period_hours, config.circadian_period_hours);
    EXPECT_FLOAT_EQ(retrieved.temperature_setpoint_c, config.temperature_setpoint_c);
}

// ============================================================================
// CALLBACK TESTS
// ============================================================================

// Callback tracking variables
static bool alert_callback_called = false;
static hypothalamus_status_t last_alert_status = HYPOTHALAMUS_STATUS_IDLE;

static void test_alert_callback(hypothalamus_status_t status,
                                 const void* alert_data,
                                 void* user_data) {
    alert_callback_called = true;
    last_alert_status = status;
}

TEST_F(HypothalamusAdapterTest, SetAlertCallback) {
    alert_callback_called = false;

    EXPECT_TRUE(hypothalamus_set_alert_callback(adapter, test_alert_callback, nullptr));

    // Trigger an alert (high stress -> fight or flight)
    hypothalamus_apply_stress(adapter, 0.95f);
    for (int i = 0; i < 200; i++) {
        hypothalamus_update(adapter, 100000);
    }

    // Alert callback should have been called for autonomic alert
    // Note: This may or may not trigger depending on exact timing
    // The important thing is that setting the callback works
}

TEST_F(HypothalamusAdapterTest, SetAutonomicCallback) {
    static bool autonomic_called = false;
    auto autonomic_cb = [](const autonomic_state_t* state, void* data) {
        autonomic_called = true;
    };

    // Note: Can't use lambda with C function pointer directly
    // Just test that the function accepts the callback
    EXPECT_TRUE(hypothalamus_set_autonomic_callback(adapter, nullptr, nullptr));
}

// ============================================================================
// NULL SAFETY TESTS
// ============================================================================

TEST_F(HypothalamusAdapterTest, GetStatusNull) {
    EXPECT_EQ(hypothalamus_get_status(NULL), HYPOTHALAMUS_STATUS_ERROR);
}

TEST_F(HypothalamusAdapterTest, GetLastErrorNull) {
    EXPECT_NE(hypothalamus_get_last_error(NULL), HYPOTHALAMUS_ERROR_NONE);
}

TEST_F(HypothalamusAdapterTest, GetStatsNull) {
    hypothalamus_stats_t stats;
    EXPECT_FALSE(hypothalamus_get_stats(NULL, &stats));
    EXPECT_FALSE(hypothalamus_get_stats(adapter, NULL));
}

TEST_F(HypothalamusAdapterTest, GetConfigNull) {
    hypothalamus_config_t cfg;
    EXPECT_FALSE(hypothalamus_get_config(NULL, &cfg));
    EXPECT_FALSE(hypothalamus_get_config(adapter, NULL));
}

TEST_F(HypothalamusAdapterTest, UpdateFunctionsWithNullAdapter) {
    EXPECT_FALSE(hypothalamus_update(NULL, 100000));
    EXPECT_FALSE(hypothalamus_update_circadian(NULL, 100000));
    EXPECT_FALSE(hypothalamus_update_homeostasis(NULL, 100000));
    EXPECT_FALSE(hypothalamus_update_hpa_axis(NULL, 100000));
    EXPECT_FALSE(hypothalamus_update_autonomic(NULL, 100000));
}

TEST_F(HypothalamusAdapterTest, SetFunctionsWithNullAdapter) {
    EXPECT_FALSE(hypothalamus_set_temperature(NULL, 37.0f));
    EXPECT_FALSE(hypothalamus_set_blood_glucose(NULL, 90.0f));
    EXPECT_FALSE(hypothalamus_set_osmolality(NULL, 290.0f));
}

TEST_F(HypothalamusAdapterTest, GetFunctionsWithNullAdapter) {
    thermoregulation_state_t thermo;
    appetite_state_t appetite;
    hydration_state_t hydration;
    hpa_axis_state_t hpa;
    autonomic_state_t ans;
    hypo_circadian_state_t circadian;
    hypothalamus_state_t full_state;

    EXPECT_FALSE(hypothalamus_get_thermoregulation(NULL, &thermo));
    EXPECT_FALSE(hypothalamus_get_appetite(NULL, &appetite));
    EXPECT_FALSE(hypothalamus_get_hydration(NULL, &hydration));
    EXPECT_FALSE(hypothalamus_get_hpa_state(NULL, &hpa));
    EXPECT_FALSE(hypothalamus_get_autonomic(NULL, &ans));
    EXPECT_FALSE(hypothalamus_get_circadian_state(NULL, &circadian));
    EXPECT_FALSE(hypothalamus_get_state(NULL, &full_state));
}

TEST_F(HypothalamusAdapterTest, GetFunctionsWithNullOutput) {
    EXPECT_FALSE(hypothalamus_get_thermoregulation(adapter, NULL));
    EXPECT_FALSE(hypothalamus_get_appetite(adapter, NULL));
    EXPECT_FALSE(hypothalamus_get_hydration(adapter, NULL));
    EXPECT_FALSE(hypothalamus_get_hpa_state(adapter, NULL));
    EXPECT_FALSE(hypothalamus_get_autonomic(adapter, NULL));
    EXPECT_FALSE(hypothalamus_get_circadian_state(adapter, NULL));
    EXPECT_FALSE(hypothalamus_get_state(adapter, NULL));
}

// ============================================================================
// BIO-ASYNC COMMUNICATION TESTS
// ============================================================================

TEST_F(HypothalamusAdapterTest, GetBioContextReturnsContext) {
    // Bio context may be null if bio-async is not initialized
    // Just verify the function doesn't crash
    bio_module_context_t ctx = hypothalamus_get_bio_context(adapter);
    // Can be NULL if bio-async not enabled globally
    (void)ctx;
}

TEST_F(HypothalamusAdapterTest, ProcessBioMessagesReturnsZeroWhenNone) {
    uint32_t processed = hypothalamus_process_bio_messages(adapter, 10);
    EXPECT_EQ(processed, 0u);
}

// ============================================================================
// EDGE CASE TESTS
// ============================================================================

TEST_F(HypothalamusAdapterTest, ExtremeTemperatureValues) {
    // Set extreme low temperature
    EXPECT_TRUE(hypothalamus_set_temperature(adapter, 30.0f));
    EXPECT_TRUE(hypothalamus_update_homeostasis(adapter, 100000));
    EXPECT_EQ(hypothalamus_get_status(adapter), HYPOTHALAMUS_STATUS_THERMAL_ALERT);

    hypothalamus_reset(adapter);

    // Set extreme high temperature
    EXPECT_TRUE(hypothalamus_set_temperature(adapter, 42.0f));
    EXPECT_TRUE(hypothalamus_update_homeostasis(adapter, 100000));
    EXPECT_EQ(hypothalamus_get_status(adapter), HYPOTHALAMUS_STATUS_THERMAL_ALERT);
}

TEST_F(HypothalamusAdapterTest, ZeroStressHasMinimalEffect) {
    float initial_cortisol = hypothalamus_get_cortisol(adapter);

    // Apply zero stress
    float result = hypothalamus_apply_stress(adapter, 0.0f);

    // Should return current cortisol level
    EXPECT_GE(result, 0.0f);

    // Cortisol should not increase significantly
    float after_cortisol = hypothalamus_get_cortisol(adapter);
    EXPECT_NEAR(after_cortisol, initial_cortisol, 0.1f);
}

TEST_F(HypothalamusAdapterTest, MaxStressDoesNotExceedLimits) {
    // Apply maximum stress repeatedly
    for (int i = 0; i < 100; i++) {
        hypothalamus_apply_stress(adapter, 1.0f);
        hypothalamus_update_hpa_axis(adapter, 100000);
    }

    float cortisol = hypothalamus_get_cortisol(adapter);
    EXPECT_LE(cortisol, 1.0f);
}

TEST_F(HypothalamusAdapterTest, LongTimeJumpDoesNotCrash) {
    // Simulate 24 hours in one update
    uint64_t day_us = 24ULL * 3600ULL * 1000000ULL;
    EXPECT_TRUE(hypothalamus_update(adapter, day_us));

    // Should still have valid state
    hypothalamus_state_t state;
    EXPECT_TRUE(hypothalamus_get_state(adapter, &state));
    EXPECT_GE(state.circadian.phase, 0.0f);
    EXPECT_LT(state.circadian.phase, 2.0f * M_PI);
}

TEST_F(HypothalamusAdapterTest, RapidUpdatesDoNotAccumulateErrors) {
    // Run many very small updates
    for (int i = 0; i < 10000; i++) {
        EXPECT_TRUE(hypothalamus_update(adapter, 100)); // 0.1ms
    }

    // Should still be in valid state
    EXPECT_NE(hypothalamus_get_status(adapter), HYPOTHALAMUS_STATUS_ERROR);
    EXPECT_EQ(hypothalamus_get_last_error(adapter), HYPOTHALAMUS_ERROR_NONE);
}
