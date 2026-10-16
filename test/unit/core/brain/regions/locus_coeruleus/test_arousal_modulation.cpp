/**
 * @file test_arousal_modulation.cpp
 * @brief Unit tests for Arousal Modulation system
 * @date 2026-01-11
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

extern "C" {
#include "core/brain/regions/locus_coeruleus/nimcp_arousal_modulation.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class ArousalModulationTest : public ::testing::Test {
protected:
    nimcp_arousal_system_t system;

    void SetUp() override {
        memset(&system, 0, sizeof(system));
        int err = nimcp_arousal_init(&system, nullptr);
        ASSERT_EQ(err, 0);
    }

    void TearDown() override {
        nimcp_arousal_shutdown(&system);
    }
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(ArousalModulationTest, InitializesSuccessfully) {
    EXPECT_TRUE(system.initialized);
}

TEST_F(ArousalModulationTest, InitNullReturnsError) {
    int err = nimcp_arousal_init(nullptr, nullptr);
    EXPECT_EQ(err, -1);
}

TEST_F(ArousalModulationTest, ShutdownClearsState) {
    int err = nimcp_arousal_shutdown(&system);
    EXPECT_EQ(err, 0);
    EXPECT_FALSE(system.initialized);
}

TEST_F(ArousalModulationTest, ResetRestoresDefaults) {
    system.dimensions.arousal = 0.9f;

    int err = nimcp_arousal_reset(&system);
    EXPECT_EQ(err, 0);
    EXPECT_FLOAT_EQ(system.dimensions.arousal, 0.5f);
}

TEST_F(ArousalModulationTest, CustomConfigApplied) {
    nimcp_arousal_shutdown(&system);

    nimcp_arousal_config_t config = nimcp_arousal_default_config();
    config.optimal_ne = 50.0f;
    config.curve_type = AROUSAL_CURVE_SIGMOID;

    int err = nimcp_arousal_init(&system, &config);
    EXPECT_EQ(err, 0);
    EXPECT_FLOAT_EQ(system.optimal_ne, 50.0f);
    EXPECT_EQ(system.curve_type, AROUSAL_CURVE_SIGMOID);
}

//=============================================================================
// Update Tests
//=============================================================================

TEST_F(ArousalModulationTest, UpdateSucceeds) {
    int err = nimcp_arousal_update(&system, 30.0f, 10.0f);
    EXPECT_EQ(err, 0);
}

TEST_F(ArousalModulationTest, UpdateInvalidDtReturnsError) {
    int err = nimcp_arousal_update(&system, 30.0f, -1.0f);
    EXPECT_EQ(err, -1);
}

TEST_F(ArousalModulationTest, LowNEReducesArousal) {
    /* Start at baseline */
    float initial_arousal = system.dimensions.arousal;

    /* Very low NE */
    for (int i = 0; i < 100; i++) {
        nimcp_arousal_update(&system, 1.0f, 10.0f);
    }

    EXPECT_LT(system.dimensions.arousal, initial_arousal);
}

TEST_F(ArousalModulationTest, OptimalNEIncreasesArousal) {
    float optimal = system.optimal_ne;

    for (int i = 0; i < 100; i++) {
        nimcp_arousal_update(&system, optimal, 10.0f);
    }

    EXPECT_GT(system.dimensions.arousal, 0.4f);
}

TEST_F(ArousalModulationTest, VeryHighNECanReduceArousal) {
    /* First reach good arousal with optimal NE */
    for (int i = 0; i < 50; i++) {
        nimcp_arousal_update(&system, system.optimal_ne, 10.0f);
    }

    float mid_arousal = system.dimensions.arousal;

    /* Very high NE should not improve further (inverted-U) */
    for (int i = 0; i < 100; i++) {
        nimcp_arousal_update(&system, system.optimal_ne * 10.0f, 10.0f);
    }

    /* Arousal should plateau or decrease at extreme NE */
    EXPECT_LE(system.dimensions.arousal, mid_arousal + 0.2f);
}

//=============================================================================
// State Classification Tests
//=============================================================================

TEST_F(ArousalModulationTest, LowArousalIsDrowsy) {
    system.dimensions.arousal = 0.15f;
    nimcp_arousal_update(&system, 5.0f, 1.0f);

    nimcp_arousal_state_t state = nimcp_arousal_get_state(&system);
    EXPECT_TRUE(state == AROUSAL_STATE_DROWSY || state == AROUSAL_STATE_SLEEP);
}

TEST_F(ArousalModulationTest, ModerateArousalIsAlert) {
    for (int i = 0; i < 100; i++) {
        nimcp_arousal_update(&system, system.optimal_ne, 10.0f);
    }

    nimcp_arousal_state_t state = nimcp_arousal_get_state(&system);
    EXPECT_TRUE(state == AROUSAL_STATE_ALERT || state == AROUSAL_STATE_RELAXED ||
                state == AROUSAL_STATE_VIGILANT);
}

TEST_F(ArousalModulationTest, HighArousalIsHyperaroused) {
    system.dimensions.arousal = 0.9f;
    nimcp_arousal_update(&system, 100.0f, 1.0f);

    nimcp_arousal_state_t state = nimcp_arousal_get_state(&system);
    EXPECT_TRUE(state == AROUSAL_STATE_HYPERAROUSED || state == AROUSAL_STATE_STRESSED);
}

//=============================================================================
// Dimension Tests
//=============================================================================

TEST_F(ArousalModulationTest, GetDimensionsSucceeds) {
    nimcp_arousal_dimensions_t dims;
    int err = nimcp_arousal_get_dimensions(&system, &dims);

    EXPECT_EQ(err, 0);
    EXPECT_GE(dims.arousal, 0.0f);
    EXPECT_LE(dims.arousal, 1.0f);
}

TEST_F(ArousalModulationTest, AllDimensionsInRange) {
    nimcp_arousal_update(&system, 30.0f, 100.0f);

    nimcp_arousal_dimensions_t dims;
    nimcp_arousal_get_dimensions(&system, &dims);

    EXPECT_GE(dims.arousal, 0.0f);
    EXPECT_LE(dims.arousal, 1.0f);
    EXPECT_GE(dims.alertness, 0.0f);
    EXPECT_LE(dims.alertness, 1.0f);
    EXPECT_GE(dims.vigilance, 0.0f);
    EXPECT_LE(dims.vigilance, 1.0f);
    EXPECT_GE(dims.activation, 0.0f);
    EXPECT_LE(dims.activation, 1.0f);
}

TEST_F(ArousalModulationTest, AlertnessFollowsArousal) {
    float low_alertness, high_alertness;

    nimcp_arousal_update(&system, 1.0f, 100.0f);
    nimcp_arousal_dimensions_t dims;
    nimcp_arousal_get_dimensions(&system, &dims);
    low_alertness = dims.alertness;

    nimcp_arousal_reset(&system);
    nimcp_arousal_update(&system, 50.0f, 100.0f);
    nimcp_arousal_get_dimensions(&system, &dims);
    high_alertness = dims.alertness;

    EXPECT_GT(high_alertness, low_alertness);
}

//=============================================================================
// Gain Modulation Tests
//=============================================================================

TEST_F(ArousalModulationTest, GetGainSucceeds) {
    nimcp_gain_modulation_t gain;
    int err = nimcp_arousal_get_gain(&system, &gain);

    EXPECT_EQ(err, 0);
    EXPECT_GT(gain.signal_gain, 0.0f);
}

TEST_F(ArousalModulationTest, GainInValidRange) {
    nimcp_arousal_update(&system, 30.0f, 100.0f);

    nimcp_gain_modulation_t gain;
    nimcp_arousal_get_gain(&system, &gain);

    EXPECT_GE(gain.signal_gain, AROUSAL_MIN_GAIN);
    EXPECT_LE(gain.signal_gain, AROUSAL_MAX_GAIN);
}

TEST_F(ArousalModulationTest, NoiseSuppressionInRange) {
    nimcp_arousal_update(&system, 30.0f, 100.0f);

    nimcp_gain_modulation_t gain;
    nimcp_arousal_get_gain(&system, &gain);

    EXPECT_GE(gain.noise_suppression, 0.0f);
    EXPECT_LE(gain.noise_suppression, 1.0f);
}

//=============================================================================
// Performance Tests
//=============================================================================

TEST_F(ArousalModulationTest, GetPerformanceSucceeds) {
    nimcp_arousal_performance_t perf;
    int err = nimcp_arousal_get_performance(&system, &perf);

    EXPECT_EQ(err, 0);
    EXPECT_GE(perf.cognitive_efficiency, 0.0f);
}

TEST_F(ArousalModulationTest, OptimalNEGivesGoodPerformance) {
    /* Update with optimal NE */
    for (int i = 0; i < 50; i++) {
        nimcp_arousal_update(&system, system.optimal_ne, 10.0f);
    }

    nimcp_arousal_performance_t perf;
    nimcp_arousal_get_performance(&system, &perf);

    EXPECT_GT(perf.cognitive_efficiency, 0.5f);
}

TEST_F(ArousalModulationTest, FatigueAccumulates) {
    /* Run for long time */
    for (int i = 0; i < 1000; i++) {
        nimcp_arousal_update(&system, 50.0f, 10.0f);
    }

    nimcp_arousal_performance_t perf;
    nimcp_arousal_get_performance(&system, &perf);

    EXPECT_GT(perf.fatigue_level, 0.0f);
}

//=============================================================================
// Circadian Tests
//=============================================================================

TEST_F(ArousalModulationTest, CircadianApplySucceeds) {
    int err = nimcp_arousal_apply_circadian(&system, 10.0f);
    EXPECT_EQ(err, 0);
}

TEST_F(ArousalModulationTest, CircadianAffectsArousal) {
    /* Morning peak */
    nimcp_arousal_apply_circadian(&system, 10.0f);
    float morning_drive = system.circadian_drive;

    /* Night low */
    nimcp_arousal_apply_circadian(&system, 3.0f);
    float night_drive = system.circadian_drive;

    EXPECT_GT(morning_drive, night_drive);
}

//=============================================================================
// NE-Performance Curve Tests
//=============================================================================

TEST_F(ArousalModulationTest, InvertedUCurveWorks) {
    float optimal = 30.0f;

    float low_perf = nimcp_arousal_ne_to_performance(5.0f, optimal, AROUSAL_CURVE_INVERTED_U);
    float mid_perf = nimcp_arousal_ne_to_performance(30.0f, optimal, AROUSAL_CURVE_INVERTED_U);
    float high_perf = nimcp_arousal_ne_to_performance(100.0f, optimal, AROUSAL_CURVE_INVERTED_U);

    /* Optimal should be best */
    EXPECT_GT(mid_perf, low_perf);
    EXPECT_GT(mid_perf, high_perf);
}

TEST_F(ArousalModulationTest, SigmoidCurveWorks) {
    float optimal = 30.0f;

    float low_perf = nimcp_arousal_ne_to_performance(5.0f, optimal, AROUSAL_CURVE_SIGMOID);
    float high_perf = nimcp_arousal_ne_to_performance(60.0f, optimal, AROUSAL_CURVE_SIGMOID);

    EXPECT_LT(low_perf, high_perf);
}

TEST_F(ArousalModulationTest, AllCurvesProduceValidOutput) {
    float optimal = 30.0f;

    for (int c = 0; c < AROUSAL_CURVE_COUNT; c++) {
        float perf = nimcp_arousal_ne_to_performance(30.0f, optimal, (nimcp_arousal_curve_t)c);
        EXPECT_GE(perf, 0.0f);
        EXPECT_LE(perf, 1.0f);
    }
}

//=============================================================================
// Target Setting Tests
//=============================================================================

TEST_F(ArousalModulationTest, SetTargetWorks) {
    int err = nimcp_arousal_set_target(&system, 0.8f);
    EXPECT_EQ(err, 0);
    EXPECT_FLOAT_EQ(system.target_arousal, 0.8f);
}

TEST_F(ArousalModulationTest, SetTargetClamps) {
    nimcp_arousal_set_target(&system, 1.5f);
    EXPECT_LE(system.target_arousal, 1.0f);

    nimcp_arousal_set_target(&system, -0.5f);
    EXPECT_GE(system.target_arousal, 0.0f);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
