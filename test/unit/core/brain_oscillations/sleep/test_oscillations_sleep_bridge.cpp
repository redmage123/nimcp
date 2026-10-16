/**
 * @file test_oscillations_sleep_bridge.cpp
 * @brief Unit tests for Oscillations-Sleep Integration Bridge
 * @version 1.0.0
 * @date 2025-12-17
 */

#include <gtest/gtest.h>
#include "core/brain_oscillations/nimcp_oscillations_sleep_bridge.h"

/* ============================================================================
 * Helper Function Tests
 * ============================================================================ */

class OscillationsSleepHelperTest : public ::testing::Test {};

/* Frequency Tests */
TEST_F(OscillationsSleepHelperTest, FrequencyAwake) {
    float freq = oscillations_sleep_freq_for_state(SLEEP_STATE_AWAKE);
    EXPECT_FLOAT_EQ(freq, OSC_SLEEP_FREQ_AWAKE);
    EXPECT_GT(freq, 20.0f) << "Awake should be Beta/Gamma range";
}

TEST_F(OscillationsSleepHelperTest, FrequencyDrowsy) {
    float freq = oscillations_sleep_freq_for_state(SLEEP_STATE_DROWSY);
    EXPECT_FLOAT_EQ(freq, OSC_SLEEP_FREQ_DROWSY);
    EXPECT_GT(freq, 8.0f) << "Drowsy should be Alpha range";
    EXPECT_LT(freq, 13.0f) << "Drowsy should be Alpha range";
}

TEST_F(OscillationsSleepHelperTest, FrequencyLightNREM) {
    float freq = oscillations_sleep_freq_for_state(SLEEP_STATE_LIGHT_NREM);
    EXPECT_FLOAT_EQ(freq, OSC_SLEEP_FREQ_LIGHT_NREM);
    EXPECT_GT(freq, 4.0f) << "Light NREM should be Theta range";
    EXPECT_LT(freq, 8.0f) << "Light NREM should be Theta range";
}

TEST_F(OscillationsSleepHelperTest, FrequencyDeepNREM) {
    float freq = oscillations_sleep_freq_for_state(SLEEP_STATE_DEEP_NREM);
    EXPECT_FLOAT_EQ(freq, OSC_SLEEP_FREQ_DEEP_NREM);
    EXPECT_LT(freq, 4.0f) << "Deep NREM should be Delta range";
}

TEST_F(OscillationsSleepHelperTest, FrequencyREM) {
    float freq = oscillations_sleep_freq_for_state(SLEEP_STATE_REM);
    EXPECT_FLOAT_EQ(freq, OSC_SLEEP_FREQ_REM);
    EXPECT_GT(freq, 4.0f) << "REM should be Theta range";
    EXPECT_LT(freq, 8.0f) << "REM should be Theta range";
}

/* Band Tests */
TEST_F(OscillationsSleepHelperTest, BandAwake) {
    oscillation_band_t band = oscillations_sleep_band_for_state(SLEEP_STATE_AWAKE);
    EXPECT_EQ(band, OSC_BAND_BETA) << "Awake should be Beta dominant";
}

TEST_F(OscillationsSleepHelperTest, BandDrowsy) {
    oscillation_band_t band = oscillations_sleep_band_for_state(SLEEP_STATE_DROWSY);
    EXPECT_EQ(band, OSC_BAND_ALPHA) << "Drowsy should be Alpha dominant";
}

TEST_F(OscillationsSleepHelperTest, BandLightNREM) {
    oscillation_band_t band = oscillations_sleep_band_for_state(SLEEP_STATE_LIGHT_NREM);
    EXPECT_EQ(band, OSC_BAND_THETA) << "Light NREM should be Theta dominant";
}

TEST_F(OscillationsSleepHelperTest, BandDeepNREM) {
    oscillation_band_t band = oscillations_sleep_band_for_state(SLEEP_STATE_DEEP_NREM);
    EXPECT_EQ(band, OSC_BAND_DELTA) << "Deep NREM should be Delta dominant";
}

TEST_F(OscillationsSleepHelperTest, BandREM) {
    oscillation_band_t band = oscillations_sleep_band_for_state(SLEEP_STATE_REM);
    EXPECT_EQ(band, OSC_BAND_THETA) << "REM should be Theta dominant";
}

/* Spindle Activity Tests */
TEST_F(OscillationsSleepHelperTest, SpindleAwake) {
    float spindle = oscillations_sleep_spindle_for_state(SLEEP_STATE_AWAKE);
    EXPECT_FLOAT_EQ(spindle, OSC_SLEEP_SPINDLE_AWAKE);
    EXPECT_FLOAT_EQ(spindle, 0.0f) << "No spindles when awake";
}

TEST_F(OscillationsSleepHelperTest, SpindleLightNREM) {
    float spindle = oscillations_sleep_spindle_for_state(SLEEP_STATE_LIGHT_NREM);
    EXPECT_FLOAT_EQ(spindle, OSC_SLEEP_SPINDLE_LIGHT);
    EXPECT_GT(spindle, 0.7f) << "Peak spindle activity in light NREM (Stage 2)";
}

TEST_F(OscillationsSleepHelperTest, SpindleDeepNREM) {
    float spindle = oscillations_sleep_spindle_for_state(SLEEP_STATE_DEEP_NREM);
    EXPECT_FLOAT_EQ(spindle, OSC_SLEEP_SPINDLE_DEEP);
    EXPECT_GT(spindle, 0.0f) << "Some spindles in deep NREM";
}

TEST_F(OscillationsSleepHelperTest, SpindleREM) {
    float spindle = oscillations_sleep_spindle_for_state(SLEEP_STATE_REM);
    EXPECT_FLOAT_EQ(spindle, OSC_SLEEP_SPINDLE_REM);
    EXPECT_FLOAT_EQ(spindle, 0.0f) << "No spindles during REM";
}

/* ============================================================================
 * Default Configuration Tests
 * ============================================================================ */

TEST_F(OscillationsSleepHelperTest, DefaultConfigValid) {
    oscillations_sleep_config_t config;
    ASSERT_EQ(oscillations_sleep_default_config(&config), 0);

    EXPECT_TRUE(config.enable_frequency_modulation);
    EXPECT_TRUE(config.enable_power_modulation);
    EXPECT_TRUE(config.enable_spindle_generation);
    EXPECT_GT(config.modulation_strength, 0.0f);
}

TEST_F(OscillationsSleepHelperTest, DefaultConfigNullReturnsError) {
    EXPECT_EQ(oscillations_sleep_default_config(nullptr), -1);
}

/* ============================================================================
 * Biological Validity Tests
 * ============================================================================ */

TEST_F(OscillationsSleepHelperTest, FrequencyDecreasesDuringSleep) {
    /* Dominant frequency should decrease as sleep deepens */
    float freq_awake = oscillations_sleep_freq_for_state(SLEEP_STATE_AWAKE);
    float freq_drowsy = oscillations_sleep_freq_for_state(SLEEP_STATE_DROWSY);
    float freq_light = oscillations_sleep_freq_for_state(SLEEP_STATE_LIGHT_NREM);
    float freq_deep = oscillations_sleep_freq_for_state(SLEEP_STATE_DEEP_NREM);

    EXPECT_GT(freq_awake, freq_drowsy);
    EXPECT_GT(freq_drowsy, freq_light);
    EXPECT_GT(freq_light, freq_deep);
}

TEST_F(OscillationsSleepHelperTest, SleepSpindlesInNREM) {
    /* Sleep spindles should be maximal in light NREM (Stage 2) */
    float spindle_light = oscillations_sleep_spindle_for_state(SLEEP_STATE_LIGHT_NREM);
    float spindle_deep = oscillations_sleep_spindle_for_state(SLEEP_STATE_DEEP_NREM);

    EXPECT_GT(spindle_light, spindle_deep) << "Peak spindles in light NREM";
}

TEST_F(OscillationsSleepHelperTest, NoSpindlesInREMOrWake) {
    /* Spindles are NREM-specific phenomena */
    float spindle_awake = oscillations_sleep_spindle_for_state(SLEEP_STATE_AWAKE);
    float spindle_rem = oscillations_sleep_spindle_for_state(SLEEP_STATE_REM);

    EXPECT_FLOAT_EQ(spindle_awake, 0.0f);
    EXPECT_FLOAT_EQ(spindle_rem, 0.0f);
}

TEST_F(OscillationsSleepHelperTest, AlphaInDrowsyState) {
    /* Drowsy state is characterized by alpha waves (relaxed, eyes closed) */
    oscillation_band_t band = oscillations_sleep_band_for_state(SLEEP_STATE_DROWSY);
    EXPECT_EQ(band, OSC_BAND_ALPHA);
}

TEST_F(OscillationsSleepHelperTest, DeltaDefinesSWS) {
    /* Delta waves define slow wave sleep (deep NREM) */
    oscillation_band_t band = oscillations_sleep_band_for_state(SLEEP_STATE_DEEP_NREM);
    float freq = oscillations_sleep_freq_for_state(SLEEP_STATE_DEEP_NREM);

    EXPECT_EQ(band, OSC_BAND_DELTA);
    EXPECT_GE(freq, 0.5f) << "Delta: 0.5-4 Hz";
    EXPECT_LE(freq, 4.0f) << "Delta: 0.5-4 Hz";
}

TEST_F(OscillationsSleepHelperTest, REMDesynchronized) {
    /* REM sleep is characterized by desynchronized EEG similar to waking theta */
    oscillation_band_t band_rem = oscillations_sleep_band_for_state(SLEEP_STATE_REM);
    oscillation_band_t band_light = oscillations_sleep_band_for_state(SLEEP_STATE_LIGHT_NREM);

    /* Both light NREM and REM have theta, but REM is more "desynchronized" */
    EXPECT_EQ(band_rem, OSC_BAND_THETA);
    EXPECT_EQ(band_light, OSC_BAND_THETA);
}

TEST_F(OscillationsSleepHelperTest, MemoryConsolidationMarkers) {
    /* Sleep spindles and sharp-wave ripples are memory consolidation markers */
    float spindle_light = oscillations_sleep_spindle_for_state(SLEEP_STATE_LIGHT_NREM);

    EXPECT_GT(spindle_light, 0.5f) << "Spindles are memory consolidation markers";
}
