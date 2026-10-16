/**
 * @file test_cognitive_sleep_regression.cpp
 * @brief Regression tests for Cognitive-Sleep bridge stability
 * @version 1.0.0
 * @date 2025-12-17
 *
 * WHAT: Ensure cognitive sleep bridges maintain consistent behavior
 * WHY:  Prevent regressions in sleep-dependent cognitive modulation
 * HOW:  Test exact values, boundary conditions, and edge cases
 */

#include <gtest/gtest.h>
#include <cmath>

// Headers have their own extern "C" guards
#include "cognitive/attention/nimcp_attention_sleep_bridge.h"
#include "cognitive/working_memory/nimcp_working_memory_sleep_bridge.h"
#include "cognitive/executive/nimcp_executive_sleep_bridge.h"
#include "core/brain_oscillations/nimcp_oscillations_sleep_bridge.h"

/* ============================================================================
 * Regression Test Fixture
 * ============================================================================ */

class CognitiveSleepRegressionTest : public ::testing::Test {
protected:
    static constexpr float EPSILON = 1e-6f;
};

/* ============================================================================
 * Exact Value Regression Tests - Attention
 * ============================================================================ */

TEST_F(CognitiveSleepRegressionTest, Attention_Capacity_ExactValues) {
    /* Regression: Ensure attention capacity factors haven't changed */
    EXPECT_FLOAT_EQ(attention_sleep_capacity_for_state(SLEEP_STATE_AWAKE), 1.0f);
    EXPECT_FLOAT_EQ(attention_sleep_capacity_for_state(SLEEP_STATE_DROWSY), 0.6f);
    EXPECT_FLOAT_EQ(attention_sleep_capacity_for_state(SLEEP_STATE_LIGHT_NREM), 0.1f);
    EXPECT_FLOAT_EQ(attention_sleep_capacity_for_state(SLEEP_STATE_DEEP_NREM), 0.0f);
    EXPECT_FLOAT_EQ(attention_sleep_capacity_for_state(SLEEP_STATE_REM), 0.3f);
}

TEST_F(CognitiveSleepRegressionTest, Attention_Vigilance_ExactValues) {
    /* Regression: Ensure vigilance factors haven't changed */
    EXPECT_FLOAT_EQ(attention_sleep_vigilance_for_state(SLEEP_STATE_AWAKE), 1.0f);
    EXPECT_FLOAT_EQ(attention_sleep_vigilance_for_state(SLEEP_STATE_DROWSY), 0.5f);
    EXPECT_FLOAT_EQ(attention_sleep_vigilance_for_state(SLEEP_STATE_LIGHT_NREM), 0.0f);
    EXPECT_FLOAT_EQ(attention_sleep_vigilance_for_state(SLEEP_STATE_DEEP_NREM), 0.0f);
    EXPECT_FLOAT_EQ(attention_sleep_vigilance_for_state(SLEEP_STATE_REM), 0.2f);
}

/* ============================================================================
 * Exact Value Regression Tests - Working Memory
 * ============================================================================ */

TEST_F(CognitiveSleepRegressionTest, WM_Capacity_ExactValues) {
    /* Regression: Ensure WM capacity factors haven't changed */
    EXPECT_FLOAT_EQ(working_memory_sleep_capacity_for_state(SLEEP_STATE_AWAKE), 1.0f);
    EXPECT_FLOAT_EQ(working_memory_sleep_capacity_for_state(SLEEP_STATE_DROWSY), 0.7f);
    EXPECT_FLOAT_EQ(working_memory_sleep_capacity_for_state(SLEEP_STATE_LIGHT_NREM), 0.3f);
    EXPECT_FLOAT_EQ(working_memory_sleep_capacity_for_state(SLEEP_STATE_DEEP_NREM), 0.0f);
    EXPECT_FLOAT_EQ(working_memory_sleep_capacity_for_state(SLEEP_STATE_REM), 0.4f);
}

TEST_F(CognitiveSleepRegressionTest, WM_Decay_ExactValues) {
    /* Regression: Ensure WM decay factors haven't changed */
    EXPECT_FLOAT_EQ(working_memory_sleep_decay_for_state(SLEEP_STATE_AWAKE), 1.0f);
    EXPECT_FLOAT_EQ(working_memory_sleep_decay_for_state(SLEEP_STATE_DROWSY), 1.5f);
    EXPECT_FLOAT_EQ(working_memory_sleep_decay_for_state(SLEEP_STATE_LIGHT_NREM), 2.0f);
    EXPECT_FLOAT_EQ(working_memory_sleep_decay_for_state(SLEEP_STATE_DEEP_NREM), 0.0f);
    EXPECT_FLOAT_EQ(working_memory_sleep_decay_for_state(SLEEP_STATE_REM), 1.2f);
}

/* ============================================================================
 * Exact Value Regression Tests - Executive
 * ============================================================================ */

TEST_F(CognitiveSleepRegressionTest, Executive_Inhibition_ExactValues) {
    /* Regression: Ensure inhibition factors haven't changed */
    EXPECT_FLOAT_EQ(executive_sleep_inhibition_for_state(SLEEP_STATE_AWAKE), 1.0f);
    EXPECT_FLOAT_EQ(executive_sleep_inhibition_for_state(SLEEP_STATE_DROWSY), 0.6f);
    EXPECT_FLOAT_EQ(executive_sleep_inhibition_for_state(SLEEP_STATE_LIGHT_NREM), 0.1f);
    EXPECT_FLOAT_EQ(executive_sleep_inhibition_for_state(SLEEP_STATE_DEEP_NREM), 0.0f);
    EXPECT_FLOAT_EQ(executive_sleep_inhibition_for_state(SLEEP_STATE_REM), 0.3f);
}

TEST_F(CognitiveSleepRegressionTest, Executive_Flexibility_ExactValues) {
    /* Regression: Ensure flexibility factors haven't changed */
    EXPECT_FLOAT_EQ(executive_sleep_flexibility_for_state(SLEEP_STATE_AWAKE), 1.0f);
    EXPECT_FLOAT_EQ(executive_sleep_flexibility_for_state(SLEEP_STATE_DROWSY), 0.5f);
    EXPECT_FLOAT_EQ(executive_sleep_flexibility_for_state(SLEEP_STATE_LIGHT_NREM), 0.0f);
    EXPECT_FLOAT_EQ(executive_sleep_flexibility_for_state(SLEEP_STATE_DEEP_NREM), 0.0f);
    EXPECT_FLOAT_EQ(executive_sleep_flexibility_for_state(SLEEP_STATE_REM), 0.4f);
}

TEST_F(CognitiveSleepRegressionTest, Executive_SwitchCost_ExactValues) {
    /* Regression: Ensure switch cost factors haven't changed */
    EXPECT_FLOAT_EQ(executive_sleep_switch_cost_for_state(SLEEP_STATE_AWAKE), 1.0f);
    EXPECT_FLOAT_EQ(executive_sleep_switch_cost_for_state(SLEEP_STATE_DROWSY), 1.5f);
    EXPECT_FLOAT_EQ(executive_sleep_switch_cost_for_state(SLEEP_STATE_LIGHT_NREM), 10.0f);
    EXPECT_FLOAT_EQ(executive_sleep_switch_cost_for_state(SLEEP_STATE_DEEP_NREM), 10.0f);
    EXPECT_FLOAT_EQ(executive_sleep_switch_cost_for_state(SLEEP_STATE_REM), 2.0f);
}

/* ============================================================================
 * Exact Value Regression Tests - Oscillations
 * ============================================================================ */

TEST_F(CognitiveSleepRegressionTest, Oscillations_Frequency_ExactValues) {
    /* Regression: Ensure frequency values haven't changed */
    EXPECT_FLOAT_EQ(oscillations_sleep_freq_for_state(SLEEP_STATE_AWAKE), 25.0f);
    EXPECT_FLOAT_EQ(oscillations_sleep_freq_for_state(SLEEP_STATE_DROWSY), 10.0f);
    EXPECT_FLOAT_EQ(oscillations_sleep_freq_for_state(SLEEP_STATE_LIGHT_NREM), 6.0f);
    EXPECT_FLOAT_EQ(oscillations_sleep_freq_for_state(SLEEP_STATE_DEEP_NREM), 2.0f);
    EXPECT_FLOAT_EQ(oscillations_sleep_freq_for_state(SLEEP_STATE_REM), 6.0f);
}

TEST_F(CognitiveSleepRegressionTest, Oscillations_Band_ExactValues) {
    /* Regression: Ensure band assignments haven't changed */
    EXPECT_EQ(oscillations_sleep_band_for_state(SLEEP_STATE_AWAKE), OSC_BAND_BETA);
    EXPECT_EQ(oscillations_sleep_band_for_state(SLEEP_STATE_DROWSY), OSC_BAND_ALPHA);
    EXPECT_EQ(oscillations_sleep_band_for_state(SLEEP_STATE_LIGHT_NREM), OSC_BAND_THETA);
    EXPECT_EQ(oscillations_sleep_band_for_state(SLEEP_STATE_DEEP_NREM), OSC_BAND_DELTA);
    EXPECT_EQ(oscillations_sleep_band_for_state(SLEEP_STATE_REM), OSC_BAND_THETA);
}

TEST_F(CognitiveSleepRegressionTest, Oscillations_Spindle_ExactValues) {
    /* Regression: Ensure spindle values haven't changed */
    EXPECT_FLOAT_EQ(oscillations_sleep_spindle_for_state(SLEEP_STATE_AWAKE), 0.0f);
    EXPECT_FLOAT_EQ(oscillations_sleep_spindle_for_state(SLEEP_STATE_DROWSY), 0.1f);
    EXPECT_FLOAT_EQ(oscillations_sleep_spindle_for_state(SLEEP_STATE_LIGHT_NREM), 0.8f);
    EXPECT_FLOAT_EQ(oscillations_sleep_spindle_for_state(SLEEP_STATE_DEEP_NREM), 0.4f);
    EXPECT_FLOAT_EQ(oscillations_sleep_spindle_for_state(SLEEP_STATE_REM), 0.0f);
}

/* ============================================================================
 * Boundary Condition Tests
 * ============================================================================ */

TEST_F(CognitiveSleepRegressionTest, InvalidState_DefaultsToAwake) {
    /* Regression: Invalid states should return awake defaults */
    sleep_state_t invalid = static_cast<sleep_state_t>(99);

    EXPECT_FLOAT_EQ(attention_sleep_capacity_for_state(invalid), ATTN_SLEEP_CAPACITY_AWAKE);
    EXPECT_FLOAT_EQ(working_memory_sleep_capacity_for_state(invalid), WM_SLEEP_CAPACITY_AWAKE);
    EXPECT_FLOAT_EQ(executive_sleep_inhibition_for_state(invalid), EXEC_SLEEP_INHIBITION_AWAKE);
    EXPECT_FLOAT_EQ(oscillations_sleep_freq_for_state(invalid), OSC_SLEEP_FREQ_AWAKE);
}

TEST_F(CognitiveSleepRegressionTest, DefaultConfig_NullCheck) {
    /* Regression: NULL config should return error */
    EXPECT_EQ(attention_sleep_default_config(nullptr), -1);
    EXPECT_EQ(working_memory_sleep_default_config(nullptr), -1);
    EXPECT_EQ(executive_sleep_default_config(nullptr), -1);
    EXPECT_EQ(oscillations_sleep_default_config(nullptr), -1);
}

TEST_F(CognitiveSleepRegressionTest, DefaultConfig_ValidValues) {
    /* Regression: Default configs should have valid values */
    attention_sleep_config_t attn_cfg;
    working_memory_sleep_config_t wm_cfg;
    executive_sleep_config_t exec_cfg;
    oscillations_sleep_config_t osc_cfg;

    ASSERT_EQ(attention_sleep_default_config(&attn_cfg), 0);
    ASSERT_EQ(working_memory_sleep_default_config(&wm_cfg), 0);
    ASSERT_EQ(executive_sleep_default_config(&exec_cfg), 0);
    ASSERT_EQ(oscillations_sleep_default_config(&osc_cfg), 0);

    /* All modulation strengths should be in [0, 1] */
    EXPECT_GE(attn_cfg.modulation_strength, 0.0f);
    EXPECT_LE(attn_cfg.modulation_strength, 1.0f);
    EXPECT_GE(wm_cfg.modulation_strength, 0.0f);
    EXPECT_LE(wm_cfg.modulation_strength, 1.0f);
    EXPECT_GE(exec_cfg.modulation_strength, 0.0f);
    EXPECT_LE(exec_cfg.modulation_strength, 1.0f);
    EXPECT_GE(osc_cfg.modulation_strength, 0.0f);
    EXPECT_LE(osc_cfg.modulation_strength, 1.0f);
}

/* ============================================================================
 * Value Range Tests
 * ============================================================================ */

TEST_F(CognitiveSleepRegressionTest, AllFactors_InValidRange) {
    /* Regression: All factors should be in valid ranges */
    for (int state = SLEEP_STATE_AWAKE; state <= SLEEP_STATE_REM; state++) {
        sleep_state_t s = static_cast<sleep_state_t>(state);

        /* Capacity factors should be in [0, 1] */
        EXPECT_GE(attention_sleep_capacity_for_state(s), 0.0f);
        EXPECT_LE(attention_sleep_capacity_for_state(s), 1.0f);
        EXPECT_GE(working_memory_sleep_capacity_for_state(s), 0.0f);
        EXPECT_LE(working_memory_sleep_capacity_for_state(s), 1.0f);

        /* Inhibition and flexibility should be in [0, 1] */
        EXPECT_GE(executive_sleep_inhibition_for_state(s), 0.0f);
        EXPECT_LE(executive_sleep_inhibition_for_state(s), 1.0f);
        EXPECT_GE(executive_sleep_flexibility_for_state(s), 0.0f);
        EXPECT_LE(executive_sleep_flexibility_for_state(s), 1.0f);

        /* Switch cost should be >= 1 */
        EXPECT_GE(executive_sleep_switch_cost_for_state(s), 1.0f);

        /* Spindle activity should be in [0, 1] */
        EXPECT_GE(oscillations_sleep_spindle_for_state(s), 0.0f);
        EXPECT_LE(oscillations_sleep_spindle_for_state(s), 1.0f);

        /* Frequency should be positive */
        EXPECT_GT(oscillations_sleep_freq_for_state(s), 0.0f);
    }
}

/* ============================================================================
 * Monotonicity Tests
 * ============================================================================ */

TEST_F(CognitiveSleepRegressionTest, CognitiveCapacity_MonotonicDecline) {
    /* Regression: Cognitive capacity should decline as sleep deepens */
    float attn_awake = attention_sleep_capacity_for_state(SLEEP_STATE_AWAKE);
    float attn_drowsy = attention_sleep_capacity_for_state(SLEEP_STATE_DROWSY);
    float attn_light = attention_sleep_capacity_for_state(SLEEP_STATE_LIGHT_NREM);
    float attn_deep = attention_sleep_capacity_for_state(SLEEP_STATE_DEEP_NREM);

    EXPECT_GE(attn_awake, attn_drowsy);
    EXPECT_GE(attn_drowsy, attn_light);
    EXPECT_GE(attn_light, attn_deep);
}

TEST_F(CognitiveSleepRegressionTest, Frequency_MonotonicDecline) {
    /* Regression: Frequency should decline from awake to deep NREM */
    float freq_awake = oscillations_sleep_freq_for_state(SLEEP_STATE_AWAKE);
    float freq_drowsy = oscillations_sleep_freq_for_state(SLEEP_STATE_DROWSY);
    float freq_light = oscillations_sleep_freq_for_state(SLEEP_STATE_LIGHT_NREM);
    float freq_deep = oscillations_sleep_freq_for_state(SLEEP_STATE_DEEP_NREM);

    EXPECT_GT(freq_awake, freq_drowsy);
    EXPECT_GT(freq_drowsy, freq_light);
    EXPECT_GT(freq_light, freq_deep);
}
