//=============================================================================
// test_signal_filter_regression.cpp - Signal Filter Backward Compat Tests
//=============================================================================
/**
 * @file test_signal_filter_regression.cpp
 * @brief Regression tests for signal filter backward compatibility
 *
 * WHAT: Verify filter integration doesn't break existing functionality
 * WHY:  Ensure DFT method continues to work and API remains stable
 * HOW:  Test known scenarios with expected results
 *
 * TEST COVERAGE:
 * - DFT method backward compatibility (use_phasor_detection=false)
 * - API stability (function signatures, return values)
 * - Known good values don't regress
 */

#include <gtest/gtest.h>
// Headers have their own extern "C" guards
    #include "middleware/patterns/nimcp_oscillation_detector.h"
#include <cmath>
#include <vector>

#define M_PI 3.14159265358979323846

//=============================================================================
// Test Fixture
//=============================================================================

class SignalFilterRegressionTest : public ::testing::Test {
protected:
    void SetUp() override {
        config = oscillation_detector_default_config();
        config.use_phasor_detection = false;  // DFT method for regression
        detector = oscillation_detector_create(&config);
        ASSERT_NE(detector, nullptr);
    }

    void TearDown() override {
        if (detector) {
            oscillation_detector_destroy(detector);
        }
    }

    std::vector<float> generate_tone(uint32_t n, float freq) {
        std::vector<float> signal(n);
        const float sample_rate = config.sample_rate_hz;
        for (uint32_t i = 0; i < n; i++) {
            float t = (float)i / sample_rate;
            signal[i] = sinf(2.0f * M_PI * freq * t);
        }
        return signal;
    }

    oscillation_detector_config_t config;
    oscillation_detector_t* detector;
};

//=============================================================================
// Backward Compatibility Tests
//=============================================================================

TEST_F(SignalFilterRegressionTest, DFT_Method_StillWorks) {
    // Verify DFT method (use_phasor_detection=false) still functions
    auto signal = generate_tone(1024, 40.0f);

    for (size_t i = 0; i < signal.size(); i++) {
        ASSERT_TRUE(oscillation_detector_add_sample(detector, signal[i], (double)i));
    }

    oscillation_result_t result;
    ASSERT_TRUE(oscillation_detector_detect(detector, &result));

    // Gamma should be dominant for 40Hz signal
    EXPECT_EQ(result.dominant_band, OSC_BAND_GAMMA);
    EXPECT_GT(result.bands[OSC_BAND_GAMMA].power, 0.5f);
}

TEST_F(SignalFilterRegressionTest, API_Stability_DefaultConfig) {
    // Verify default config hasn't changed
    oscillation_detector_config_t default_config = oscillation_detector_default_config();

    EXPECT_EQ(default_config.sample_rate_hz, 1000.0f);
    EXPECT_EQ(default_config.window_size, 1024u);
    EXPECT_TRUE(default_config.use_phasor_detection);  // Default should be true
    // Note: enable_pac and enable_plv are opt-in features
}

TEST_F(SignalFilterRegressionTest, API_Stability_CreateDestroy) {
    // Verify create/destroy API still works
    oscillation_detector_t* temp_detector = oscillation_detector_create(&config);
    ASSERT_NE(temp_detector, nullptr);
    oscillation_detector_destroy(temp_detector);
    // No crash = success
}

TEST_F(SignalFilterRegressionTest, API_Stability_AddSample) {
    // Verify add_sample API unchanged
    EXPECT_TRUE(oscillation_detector_add_sample(detector, 1.0f, 0.0));
    EXPECT_TRUE(oscillation_detector_add_sample(detector, -1.0f, 1.0));
}

TEST_F(SignalFilterRegressionTest, API_Stability_Detect) {
    // Verify detect API unchanged
    auto signal = generate_tone(1024, 10.0f);
    for (size_t i = 0; i < signal.size(); i++) {
        oscillation_detector_add_sample(detector, signal[i], (double)i);
    }

    oscillation_result_t result;
    EXPECT_TRUE(oscillation_detector_detect(detector, &result));

    // Verify result structure unchanged
    EXPECT_GE(result.dominant_band, OSC_BAND_DELTA);
    EXPECT_LE(result.dominant_band, OSC_BAND_GAMMA);
    EXPECT_GE(result.total_power, 0.0f);
}

TEST_F(SignalFilterRegressionTest, KnownGood_GammaBand) {
    // Verify known good result for 40Hz signal
    auto signal = generate_tone(1024, 40.0f);

    for (size_t i = 0; i < signal.size(); i++) {
        oscillation_detector_add_sample(detector, signal[i], (double)i);
    }

    oscillation_result_t result;
    ASSERT_TRUE(oscillation_detector_detect(detector, &result));

    // Known good values from before filter integration
    EXPECT_EQ(result.dominant_band, OSC_BAND_GAMMA);
    EXPECT_GT(result.bands[OSC_BAND_GAMMA].power, 0.9f);
    EXPECT_LT(result.bands[OSC_BAND_DELTA].power, 0.1f);
    EXPECT_LT(result.bands[OSC_BAND_THETA].power, 0.1f);
    EXPECT_LT(result.bands[OSC_BAND_ALPHA].power, 0.1f);
}

TEST_F(SignalFilterRegressionTest, KnownGood_AlphaBand) {
    // Verify known good result for 10Hz signal
    auto signal = generate_tone(1024, 10.0f);

    for (size_t i = 0; i < signal.size(); i++) {
        oscillation_detector_add_sample(detector, signal[i], (double)i);
    }

    oscillation_result_t result;
    ASSERT_TRUE(oscillation_detector_detect(detector, &result));

    // Known good values from before filter integration
    EXPECT_EQ(result.dominant_band, OSC_BAND_ALPHA);
    EXPECT_GT(result.bands[OSC_BAND_ALPHA].power, 0.9f);
}

TEST_F(SignalFilterRegressionTest, KnownGood_BetaBand) {
    // Verify known good result for 20Hz signal
    auto signal = generate_tone(1024, 20.0f);

    for (size_t i = 0; i < signal.size(); i++) {
        oscillation_detector_add_sample(detector, signal[i], (double)i);
    }

    oscillation_result_t result;
    ASSERT_TRUE(oscillation_detector_detect(detector, &result));

    // Known good values
    EXPECT_EQ(result.dominant_band, OSC_BAND_BETA);
    EXPECT_GT(result.bands[OSC_BAND_BETA].power, 0.9f);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
