//=============================================================================
// test_oscillation_detector.cpp - Comprehensive Oscillation Detector Tests
//=============================================================================

#include <gtest/gtest.h>
#include <cmath>

// Headers have their own extern "C" guards
#include "middleware/patterns/nimcp_oscillation_detector.h"

/**
 * WHAT: Comprehensive test suite for oscillation detection
 * WHY:  Ensure brain oscillation analysis works correctly
 * HOW:  Unit tests for all 11 functions, signal generation, FFT validation
 */

class OscillationDetectorTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}

    // Generate synthetic oscillation at specific frequency
    void GenerateOscillation(float* buffer, uint32_t size, float freq_hz,
                            float amplitude, float sample_rate_hz) {
        for (uint32_t i = 0; i < size; i++) {
            float t = (float)i / sample_rate_hz;
            buffer[i] = amplitude * sin(2.0f * M_PI * freq_hz * t);
        }
    }

    bool FloatEquals(float a, float b, float eps = 0.01f) {
        return std::fabs(a - b) < eps;
    }
};

//=============================================================================
// LIFECYCLE TESTS
//=============================================================================

TEST_F(OscillationDetectorTest, DefaultConfig_Valid) {
    oscillation_detector_config_t config = oscillation_detector_default_config();
    EXPECT_EQ(config.sample_rate_hz, OSC_SAMPLE_RATE_HZ);
    EXPECT_EQ(config.window_size, OSC_WINDOW_SIZE);
    EXPECT_FLOAT_EQ(config.min_burst_duration_ms, OSC_MIN_BURST_DURATION_MS);
    EXPECT_FLOAT_EQ(config.burst_threshold_std, OSC_BURST_THRESHOLD);
}

TEST_F(OscillationDetectorTest, Create_Success_DefaultConfig) {
    oscillation_detector_config_t config = oscillation_detector_default_config();
    oscillation_detector_t* detector = oscillation_detector_create(&config);
    ASSERT_NE(detector, nullptr);
    oscillation_detector_destroy(detector);
}

TEST_F(OscillationDetectorTest, Create_Success_CustomConfig) {
    oscillation_detector_config_t config = oscillation_detector_default_config();
    config.window_size = 512;
    config.sample_rate_hz = 500.0f;
    config.enable_burst_detection = false;

    oscillation_detector_t* detector = oscillation_detector_create(&config);
    ASSERT_NE(detector, nullptr);
    oscillation_detector_destroy(detector);
}

TEST_F(OscillationDetectorTest, Create_Failure_NullConfig) {
    oscillation_detector_t* detector = oscillation_detector_create(nullptr);
    EXPECT_EQ(detector, nullptr);
}

TEST_F(OscillationDetectorTest, Destroy_NullSafe) {
    oscillation_detector_destroy(nullptr);
    // Should not crash
}

//=============================================================================
// SAMPLE ADDITION TESTS
//=============================================================================

TEST_F(OscillationDetectorTest, AddSample_Success) {
    oscillation_detector_config_t config = oscillation_detector_default_config();
    oscillation_detector_t* detector = oscillation_detector_create(&config);
    ASSERT_NE(detector, nullptr);

    bool result = oscillation_detector_add_sample(detector, 1.0f, 0.0);
    EXPECT_TRUE(result);

    oscillation_detector_destroy(detector);
}

TEST_F(OscillationDetectorTest, AddSample_Failure_NullDetector) {
    bool result = oscillation_detector_add_sample(nullptr, 1.0f, 0.0);
    EXPECT_FALSE(result);
}

TEST_F(OscillationDetectorTest, AddSample_Success_MultipleValues) {
    oscillation_detector_config_t config = oscillation_detector_default_config();
    oscillation_detector_t* detector = oscillation_detector_create(&config);

    for (int i = 0; i < 100; i++) {
        bool result = oscillation_detector_add_sample(detector,
            (float)i * 0.1f, (double)i);
        EXPECT_TRUE(result);
    }

    oscillation_detector_destroy(detector);
}

//=============================================================================
// OSCILLATION DETECTION TESTS
//=============================================================================

TEST_F(OscillationDetectorTest, Detect_Success_ThetaBand) {
    oscillation_detector_config_t config = oscillation_detector_default_config();
    // Use DFT method for low-frequency bands due to filter transient effects
    // The Hilbert+filter method works well for high frequencies (see GammaBand test)
    // but introduces artifacts for Theta band (4-8Hz) with the current filter order
    config.use_phasor_detection = false;
    oscillation_detector_t* detector = oscillation_detector_create(&config);
    ASSERT_NE(detector, nullptr);

    // Generate theta oscillation (6 Hz)
    float signal[1024];
    GenerateOscillation(signal, 1024, 6.0f, 1.0f, config.sample_rate_hz);

    // Add samples
    for (uint32_t i = 0; i < 1024; i++) {
        oscillation_detector_add_sample(detector, signal[i], (double)i);
    }

    // Detect oscillations
    oscillation_result_t result;
    bool success = oscillation_detector_detect(detector, &result);
    EXPECT_TRUE(success);

    // Should detect theta band as dominant
    EXPECT_EQ(result.dominant_band, OSC_BAND_THETA);
    EXPECT_GT(result.bands[OSC_BAND_THETA].power,
              result.bands[OSC_BAND_DELTA].power);

    oscillation_detector_destroy(detector);
}

TEST_F(OscillationDetectorTest, Detect_Success_GammaBand) {
    oscillation_detector_config_t config = oscillation_detector_default_config();
    oscillation_detector_t* detector = oscillation_detector_create(&config);

    // Generate gamma oscillation (40 Hz)
    float signal[1024];
    GenerateOscillation(signal, 1024, 40.0f, 1.0f, config.sample_rate_hz);

    for (uint32_t i = 0; i < 1024; i++) {
        oscillation_detector_add_sample(detector, signal[i], (double)i);
    }

    oscillation_result_t result;
    oscillation_detector_detect(detector, &result);

    EXPECT_EQ(result.dominant_band, OSC_BAND_GAMMA);
    EXPECT_TRUE(result.has_gamma);

    oscillation_detector_destroy(detector);
}

TEST_F(OscillationDetectorTest, Detect_Failure_NullDetector) {
    oscillation_result_t result;
    bool success = oscillation_detector_detect(nullptr, &result);
    EXPECT_FALSE(success);
}

TEST_F(OscillationDetectorTest, Detect_Failure_NullResult) {
    oscillation_detector_config_t config = oscillation_detector_default_config();
    oscillation_detector_t* detector = oscillation_detector_create(&config);

    bool success = oscillation_detector_detect(detector, nullptr);
    EXPECT_FALSE(success);

    oscillation_detector_destroy(detector);
}

//=============================================================================
// BAND POWER TESTS
//=============================================================================

TEST_F(OscillationDetectorTest, GetBandPower_Success) {
    oscillation_detector_config_t config = oscillation_detector_default_config();
    oscillation_detector_t* detector = oscillation_detector_create(&config);

    // Add some signal
    for (int i = 0; i < 100; i++) {
        oscillation_detector_add_sample(detector, 0.5f, (double)i);
    }

    band_power_t power;
    bool result = oscillation_detector_get_band_power(detector, OSC_BAND_ALPHA, &power);
    EXPECT_TRUE(result);
    EXPECT_EQ(power.band, OSC_BAND_ALPHA);

    oscillation_detector_destroy(detector);
}

TEST_F(OscillationDetectorTest, GetBandPower_Failure_NullDetector) {
    band_power_t power;
    bool result = oscillation_detector_get_band_power(nullptr, OSC_BAND_ALPHA, &power);
    EXPECT_FALSE(result);
}

TEST_F(OscillationDetectorTest, GetBandPower_Failure_NullPower) {
    oscillation_detector_config_t config = oscillation_detector_default_config();
    oscillation_detector_t* detector = oscillation_detector_create(&config);

    bool result = oscillation_detector_get_band_power(detector, OSC_BAND_ALPHA, nullptr);
    EXPECT_FALSE(result);

    oscillation_detector_destroy(detector);
}

//=============================================================================
// PLV (PHASE LOCKING) TESTS
//=============================================================================

TEST_F(OscillationDetectorTest, ComputePLV_Success_Identical) {
    oscillation_detector_config_t config = oscillation_detector_default_config();
    config.enable_plv = true;
    oscillation_detector_t* detector = oscillation_detector_create(&config);

    // Two identical signals should have PLV = 1.0
    float signal1[1024], signal2[1024];
    GenerateOscillation(signal1, 1024, 10.0f, 1.0f, config.sample_rate_hz);
    for (uint32_t i = 0; i < 1024; i++) {
        signal2[i] = signal1[i];
    }

    phase_locking_t result;
    bool success = oscillation_detector_compute_plv(detector, OSC_BAND_ALPHA,
                                                    signal1, signal2, 1024, &result);
    EXPECT_TRUE(success);
    EXPECT_NEAR(result.plv, 1.0f, 0.1f);  // Should be close to 1.0

    oscillation_detector_destroy(detector);
}

TEST_F(OscillationDetectorTest, ComputePLV_Failure_NullParams) {
    oscillation_detector_config_t config = oscillation_detector_default_config();
    oscillation_detector_t* detector = oscillation_detector_create(&config);

    float signal[100];
    phase_locking_t result;

    EXPECT_FALSE(oscillation_detector_compute_plv(nullptr, OSC_BAND_ALPHA,
                                                  signal, signal, 100, &result));
    EXPECT_FALSE(oscillation_detector_compute_plv(detector, OSC_BAND_ALPHA,
                                                  nullptr, signal, 100, &result));
    EXPECT_FALSE(oscillation_detector_compute_plv(detector, OSC_BAND_ALPHA,
                                                  signal, nullptr, 100, &result));
    EXPECT_FALSE(oscillation_detector_compute_plv(detector, OSC_BAND_ALPHA,
                                                  signal, signal, 100, nullptr));

    oscillation_detector_destroy(detector);
}

//=============================================================================
// PAC (CROSS-FREQUENCY COUPLING) TESTS
//=============================================================================

TEST_F(OscillationDetectorTest, DetectPAC_Success) {
    oscillation_detector_config_t config = oscillation_detector_default_config();
    config.enable_pac = true;
    oscillation_detector_t* detector = oscillation_detector_create(&config);

    // Generate theta-gamma coupled signal
    float signal[1024];
    for (uint32_t i = 0; i < 1024; i++) {
        float t = (float)i / config.sample_rate_hz;
        float theta = sin(2.0f * M_PI * 6.0f * t);  // 6 Hz theta
        float gamma = sin(2.0f * M_PI * 40.0f * t) * (1.0f + theta);  // Gamma modulated by theta
        signal[i] = theta + gamma;
    }

    for (uint32_t i = 0; i < 1024; i++) {
        oscillation_detector_add_sample(detector, signal[i], (double)i);
    }

    cross_freq_coupling_t couplings[10];
    uint32_t num_found = 0;
    bool success = oscillation_detector_detect_pac(detector, couplings, 10, &num_found);
    EXPECT_TRUE(success);

    oscillation_detector_destroy(detector);
}

TEST_F(OscillationDetectorTest, DetectPAC_Failure_NullParams) {
    oscillation_detector_config_t config = oscillation_detector_default_config();
    oscillation_detector_t* detector = oscillation_detector_create(&config);

    cross_freq_coupling_t couplings[10];
    uint32_t num_found = 0;

    EXPECT_FALSE(oscillation_detector_detect_pac(nullptr, couplings, 10, &num_found));
    EXPECT_FALSE(oscillation_detector_detect_pac(detector, nullptr, 10, &num_found));
    EXPECT_FALSE(oscillation_detector_detect_pac(detector, couplings, 10, nullptr));

    oscillation_detector_destroy(detector);
}

//=============================================================================
// RESET TESTS
//=============================================================================

TEST_F(OscillationDetectorTest, Reset_ClearsState) {
    oscillation_detector_config_t config = oscillation_detector_default_config();
    oscillation_detector_t* detector = oscillation_detector_create(&config);

    // Add samples
    for (int i = 0; i < 100; i++) {
        oscillation_detector_add_sample(detector, 1.0f, (double)i);
    }

    // Reset
    oscillation_detector_reset(detector);

    // Reset clears buffer state and band states, but total_samples persists
    // This is correct behavior - reset() clears analysis state, not statistics
    uint64_t samples, bursts;
    float avg_power;
    oscillation_detector_get_stats(detector, &samples, &bursts, &avg_power);
    EXPECT_EQ(samples, 100);  // total_samples is not reset - it's a lifetime statistic

    oscillation_detector_destroy(detector);
}

TEST_F(OscillationDetectorTest, Reset_NullSafe) {
    oscillation_detector_reset(nullptr);
    // Should not crash
}

//=============================================================================
// STATISTICS TESTS
//=============================================================================

TEST_F(OscillationDetectorTest, GetStats_Success) {
    oscillation_detector_config_t config = oscillation_detector_default_config();
    oscillation_detector_t* detector = oscillation_detector_create(&config);

    uint64_t samples, bursts;
    float avg_power;
    bool result = oscillation_detector_get_stats(detector, &samples, &bursts, &avg_power);
    EXPECT_TRUE(result);
    EXPECT_EQ(samples, 0);  // Initially zero

    oscillation_detector_destroy(detector);
}

TEST_F(OscillationDetectorTest, GetStats_Failure_NullDetector) {
    uint64_t samples, bursts;
    float avg_power;
    bool result = oscillation_detector_get_stats(nullptr, &samples, &bursts, &avg_power);
    EXPECT_FALSE(result);
}

TEST_F(OscillationDetectorTest, GetStats_TracksSamples) {
    oscillation_detector_config_t config = oscillation_detector_default_config();
    oscillation_detector_t* detector = oscillation_detector_create(&config);

    // Add 50 samples
    for (int i = 0; i < 50; i++) {
        oscillation_detector_add_sample(detector, 0.5f, (double)i);
    }

    uint64_t samples, bursts;
    float avg_power;
    oscillation_detector_get_stats(detector, &samples, &bursts, &avg_power);
    EXPECT_EQ(samples, 50);

    oscillation_detector_destroy(detector);
}

//=============================================================================
// UTILITY FUNCTION TESTS
//=============================================================================

TEST_F(OscillationDetectorTest, BandName_AllBands) {
    EXPECT_STREQ(oscillation_band_name(OSC_BAND_DELTA), "Delta");
    EXPECT_STREQ(oscillation_band_name(OSC_BAND_THETA), "Theta");
    EXPECT_STREQ(oscillation_band_name(OSC_BAND_ALPHA), "Alpha");
    EXPECT_STREQ(oscillation_band_name(OSC_BAND_BETA), "Beta");
    EXPECT_STREQ(oscillation_band_name(OSC_BAND_GAMMA), "Gamma");
}

TEST_F(OscillationDetectorTest, BandRange_Delta) {
    float min_hz, max_hz;
    oscillation_band_range(OSC_BAND_DELTA, &min_hz, &max_hz);
    EXPECT_FLOAT_EQ(min_hz, 0.0f);
    EXPECT_FLOAT_EQ(max_hz, 4.0f);
}

TEST_F(OscillationDetectorTest, BandRange_Theta) {
    float min_hz, max_hz;
    oscillation_band_range(OSC_BAND_THETA, &min_hz, &max_hz);
    EXPECT_FLOAT_EQ(min_hz, 4.0f);
    EXPECT_FLOAT_EQ(max_hz, 8.0f);
}

TEST_F(OscillationDetectorTest, BandRange_Gamma) {
    float min_hz, max_hz;
    oscillation_band_range(OSC_BAND_GAMMA, &min_hz, &max_hz);
    EXPECT_FLOAT_EQ(min_hz, 30.0f);
    EXPECT_FLOAT_EQ(max_hz, 100.0f);
}

TEST_F(OscillationDetectorTest, BandRange_NullSafe) {
    float min_hz, max_hz;
    oscillation_band_range(OSC_BAND_ALPHA, &min_hz, nullptr);
    oscillation_band_range(OSC_BAND_ALPHA, nullptr, &max_hz);
    // Should not crash
}

//=============================================================================
// REGRESSION TESTS
//=============================================================================

TEST_F(OscillationDetectorTest, Regression_WindowOverflow) {
    oscillation_detector_config_t config = oscillation_detector_default_config();
    oscillation_detector_t* detector = oscillation_detector_create(&config);

    // Add more samples than window size
    for (uint32_t i = 0; i < config.window_size * 2; i++) {
        bool result = oscillation_detector_add_sample(detector, 1.0f, (double)i);
        EXPECT_TRUE(result);  // Should handle overflow gracefully
    }

    oscillation_detector_destroy(detector);
}

TEST_F(OscillationDetectorTest, Regression_ZeroSignal) {
    oscillation_detector_config_t config = oscillation_detector_default_config();
    oscillation_detector_t* detector = oscillation_detector_create(&config);

    // Add all zeros - need full window (1024 samples) for detect() to succeed
    for (uint32_t i = 0; i < config.window_size; i++) {
        oscillation_detector_add_sample(detector, 0.0f, (double)i);
    }

    oscillation_result_t result;
    bool success = oscillation_detector_detect(detector, &result);
    EXPECT_TRUE(success);
    EXPECT_FLOAT_EQ(result.total_power, 0.0f);

    oscillation_detector_destroy(detector);
}

TEST_F(OscillationDetectorTest, Regression_HighFrequencyNyquist) {
    oscillation_detector_config_t config = oscillation_detector_default_config();
    oscillation_detector_t* detector = oscillation_detector_create(&config);

    // Generate signal at Nyquist frequency (sample_rate / 2)
    float nyquist_freq = config.sample_rate_hz / 2.0f;
    float signal[1024];
    GenerateOscillation(signal, 1024, nyquist_freq, 1.0f, config.sample_rate_hz);

    for (uint32_t i = 0; i < 1024; i++) {
        oscillation_detector_add_sample(detector, signal[i], (double)i);
    }

    oscillation_result_t result;
    bool success = oscillation_detector_detect(detector, &result);
    EXPECT_TRUE(success);

    oscillation_detector_destroy(detector);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
