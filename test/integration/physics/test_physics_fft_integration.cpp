//=============================================================================
// test_physics_fft_integration.cpp - Physics FFT Integration Tests
//=============================================================================
/**
 * @file test_physics_fft_integration.cpp
 * @brief Integration tests for FFT-based spectral analysis in physics layer
 *
 * Tests Ephaptic FFT bridge for LFP band power computation.
 */

#include <gtest/gtest.h>
#include <cmath>

#include "utils/validation/nimcp_common.h"
#include "utils/spectral/nimcp_fft.h"
#include "physics/ephaptic/nimcp_ephaptic.h"
#include "physics/bridges/nimcp_ephaptic_fft_bridge.h"

//=============================================================================
// Test Fixture
//=============================================================================

class PhysicsFFTIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        nimcp_ephaptic_config_t eph_config = nimcp_ephaptic_default_config();
        ephaptic_initialized_ = (nimcp_ephaptic_init(&ephaptic_, &eph_config) == NIMCP_SUCCESS);
    }

    void TearDown() override {
        if (ephaptic_initialized_) nimcp_ephaptic_destroy(&ephaptic_);
    }

    nimcp_ephaptic_system_t ephaptic_;
    bool ephaptic_initialized_ = false;
};

//=============================================================================
// FFT Plan Tests
//=============================================================================

TEST_F(PhysicsFFTIntegrationTest, FFTPlanCreation) {
    fft_plan_t* plan = fft_plan_create(1024, FFT_REAL);
    ASSERT_NE(plan, nullptr);
    EXPECT_EQ(fft_plan_get_size(plan), 1024U);
    fft_plan_destroy(plan);
}

TEST_F(PhysicsFFTIntegrationTest, FFTPlanPowerOf2) {
    // Valid power of 2 sizes
    fft_plan_t* plan64 = fft_plan_create(64, FFT_REAL);
    fft_plan_t* plan256 = fft_plan_create(256, FFT_REAL);
    fft_plan_t* plan2048 = fft_plan_create(2048, FFT_REAL);

    EXPECT_NE(plan64, nullptr);
    EXPECT_NE(plan256, nullptr);
    EXPECT_NE(plan2048, nullptr);

    fft_plan_destroy(plan64);
    fft_plan_destroy(plan256);
    fft_plan_destroy(plan2048);
}

TEST_F(PhysicsFFTIntegrationTest, FFTWindow) {
    fft_plan_t* plan = fft_plan_create(1024, FFT_REAL);
    ASSERT_NE(plan, nullptr);

    EXPECT_TRUE(fft_plan_set_window(plan, FFT_WINDOW_HANN));
    EXPECT_TRUE(fft_plan_set_window(plan, FFT_WINDOW_HAMMING));
    EXPECT_TRUE(fft_plan_set_window(plan, FFT_WINDOW_BLACKMAN));

    fft_plan_destroy(plan);
}

//=============================================================================
// FFT Execution Tests
//=============================================================================

TEST_F(PhysicsFFTIntegrationTest, FFTRealExecution) {
    const uint32_t size = 1024;
    fft_plan_t* plan = fft_plan_create(size, FFT_REAL);
    ASSERT_NE(plan, nullptr);

    // Create test signal: DC + 10 Hz sine wave
    float* signal = new float[size];
    const float sampling_rate = 1000.0f;
    for (uint32_t i = 0; i < size; i++) {
        float t = i / sampling_rate;
        signal[i] = 1.0f + 0.5f * sinf(2.0f * M_PI * 10.0f * t);  // DC + 10 Hz
    }

    fft_complex_t* spectrum = new fft_complex_t[size / 2 + 1];
    EXPECT_TRUE(fft_execute_real(plan, signal, spectrum));

    // Check DC component (should be ~1.0 * size)
    EXPECT_GT(spectrum[0].real, 0.0f);

    // Check 10 Hz component (bin 10 at 1000 Hz sampling)
    int bin_10hz = 10;
    float mag_10hz = sqrtf(spectrum[bin_10hz].real * spectrum[bin_10hz].real +
                          spectrum[bin_10hz].imag * spectrum[bin_10hz].imag);
    EXPECT_GT(mag_10hz, 0.0f);

    delete[] signal;
    delete[] spectrum;
    fft_plan_destroy(plan);
}

TEST_F(PhysicsFFTIntegrationTest, FFTPowerSpectrum) {
    const uint32_t size = 1024;
    fft_plan_t* plan = fft_plan_create(size, FFT_REAL);
    ASSERT_NE(plan, nullptr);

    // Create test signal
    float* signal = new float[size];
    for (uint32_t i = 0; i < size; i++) {
        signal[i] = sinf(2.0f * M_PI * 50.0f * i / 1000.0f);  // 50 Hz
    }

    fft_complex_t* spectrum = new fft_complex_t[size / 2 + 1];
    float* power = new float[size / 2 + 1];

    fft_execute_real(plan, signal, spectrum);
    EXPECT_TRUE(fft_power_spectrum(spectrum, power, size / 2 + 1));

    // Check power is non-negative
    for (uint32_t i = 0; i < size / 2 + 1; i++) {
        EXPECT_GE(power[i], 0.0f);
    }

    delete[] signal;
    delete[] spectrum;
    delete[] power;
    fft_plan_destroy(plan);
}

//=============================================================================
// Brain Wave Band Tests
//=============================================================================

TEST_F(PhysicsFFTIntegrationTest, BandPowerComputation) {
    const uint32_t size = 1024;
    const float sampling_rate = 1000.0f;

    fft_plan_t* plan = fft_plan_create(size, FFT_REAL);
    ASSERT_NE(plan, nullptr);

    // Create alpha wave signal (10 Hz)
    float* signal = new float[size];
    for (uint32_t i = 0; i < size; i++) {
        signal[i] = sinf(2.0f * M_PI * 10.0f * i / sampling_rate);
    }

    fft_complex_t* spectrum = new fft_complex_t[size / 2 + 1];
    float* power = new float[size / 2 + 1];

    fft_execute_real(plan, signal, spectrum);
    fft_power_spectrum(spectrum, power, size / 2 + 1);

    // Compute alpha band power (8-13 Hz)
    float alpha_power = fft_band_power(power, size / 2 + 1, sampling_rate, 8.0f, 13.0f);
    float delta_power = fft_band_power(power, size / 2 + 1, sampling_rate, 1.0f, 4.0f);

    // Alpha should dominate
    EXPECT_GT(alpha_power, delta_power);

    delete[] signal;
    delete[] spectrum;
    delete[] power;
    fft_plan_destroy(plan);
}

TEST_F(PhysicsFFTIntegrationTest, DominantFrequency) {
    const uint32_t size = 1024;
    const float sampling_rate = 1000.0f;

    fft_plan_t* plan = fft_plan_create(size, FFT_REAL);
    ASSERT_NE(plan, nullptr);

    // Create 25 Hz signal (beta)
    float* signal = new float[size];
    for (uint32_t i = 0; i < size; i++) {
        signal[i] = sinf(2.0f * M_PI * 25.0f * i / sampling_rate);
    }

    fft_complex_t* spectrum = new fft_complex_t[size / 2 + 1];
    float* power = new float[size / 2 + 1];

    fft_execute_real(plan, signal, spectrum);
    fft_power_spectrum(spectrum, power, size / 2 + 1);

    float dominant = fft_dominant_frequency(power, size / 2 + 1, sampling_rate);
    EXPECT_NEAR(dominant, 25.0f, 2.0f);  // Should be close to 25 Hz

    delete[] signal;
    delete[] spectrum;
    delete[] power;
    fft_plan_destroy(plan);
}

//=============================================================================
// Ephaptic FFT Bridge Tests
//=============================================================================

TEST_F(PhysicsFFTIntegrationTest, EphapticFFTBridgeCreation) {
    ephaptic_fft_bridge_t* bridge = ephaptic_fft_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);
    ephaptic_fft_bridge_destroy(bridge);
}

TEST_F(PhysicsFFTIntegrationTest, EphapticFFTDefaultConfig) {
    ephaptic_fft_config_t config;
    EXPECT_EQ(ephaptic_fft_default_config(&config), 0);
    EXPECT_EQ(config.fft_size, EPHAPTIC_FFT_DEFAULT_SIZE);
    EXPECT_EQ(config.sampling_rate, EPHAPTIC_FFT_DEFAULT_SAMPLE_RATE);
}

TEST_F(PhysicsFFTIntegrationTest, EphapticFFTAddSamples) {
    ephaptic_fft_bridge_t* bridge = ephaptic_fft_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    // Add samples
    for (int i = 0; i < 100; i++) {
        float lfp = sinf(2.0f * M_PI * 10.0f * i / 1000.0f);
        EXPECT_EQ(ephaptic_fft_add_sample(bridge, lfp, (float)i), 0);
    }

    float level = ephaptic_fft_buffer_level(bridge);
    EXPECT_GT(level, 0.0f);

    ephaptic_fft_bridge_destroy(bridge);
}

TEST_F(PhysicsFFTIntegrationTest, EphapticFFTBufferReady) {
    ephaptic_fft_config_t config;
    ephaptic_fft_default_config(&config);
    config.fft_size = 256;  // Smaller for faster test

    ephaptic_fft_bridge_t* bridge = ephaptic_fft_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);

    // Not ready initially
    EXPECT_FALSE(ephaptic_fft_buffer_ready(bridge));

    // Fill buffer
    for (uint32_t i = 0; i < 256; i++) {
        float lfp = sinf(2.0f * M_PI * 10.0f * i / 1000.0f);
        ephaptic_fft_add_sample(bridge, lfp, (float)i);
    }

    // Now should be ready
    EXPECT_TRUE(ephaptic_fft_buffer_ready(bridge));

    ephaptic_fft_bridge_destroy(bridge);
}

TEST_F(PhysicsFFTIntegrationTest, EphapticFFTComputeBandPower) {
    ephaptic_fft_config_t config;
    ephaptic_fft_default_config(&config);
    config.fft_size = 256;

    ephaptic_fft_bridge_t* bridge = ephaptic_fft_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);

    // Fill buffer with alpha wave (10 Hz)
    for (uint32_t i = 0; i < 256; i++) {
        float lfp = sinf(2.0f * M_PI * 10.0f * i / config.sampling_rate);
        ephaptic_fft_add_sample(bridge, lfp, (float)i);
    }

    ephaptic_fft_result_t result;
    int status = ephaptic_fft_compute_band_power(bridge, &result);
    EXPECT_EQ(status, 0);

    if (status == 0) {
        // Alpha band (index 2) should have most power
        EXPECT_GT(result.total_power, 0.0f);
        EXPECT_TRUE(result.buffer_full);
    }

    ephaptic_fft_bridge_destroy(bridge);
}

TEST_F(PhysicsFFTIntegrationTest, EphapticFFTReset) {
    ephaptic_fft_config_t config;
    ephaptic_fft_default_config(&config);
    config.fft_size = 128;

    ephaptic_fft_bridge_t* bridge = ephaptic_fft_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);

    // Fill buffer
    for (uint32_t i = 0; i < 128; i++) {
        ephaptic_fft_add_sample(bridge, 1.0f, (float)i);
    }
    EXPECT_TRUE(ephaptic_fft_buffer_ready(bridge));

    // Reset
    EXPECT_EQ(ephaptic_fft_bridge_reset(bridge), 0);
    EXPECT_FALSE(ephaptic_fft_buffer_ready(bridge));

    ephaptic_fft_bridge_destroy(bridge);
}

TEST_F(PhysicsFFTIntegrationTest, EphapticFFTGetConfig) {
    ephaptic_fft_config_t in_config;
    ephaptic_fft_default_config(&in_config);
    in_config.fft_size = 512;
    in_config.sampling_rate = 500.0f;

    ephaptic_fft_bridge_t* bridge = ephaptic_fft_bridge_create(&in_config);
    ASSERT_NE(bridge, nullptr);

    ephaptic_fft_config_t out_config;
    EXPECT_EQ(ephaptic_fft_get_config(bridge, &out_config), 0);
    EXPECT_EQ(out_config.fft_size, 512U);
    EXPECT_FLOAT_EQ(out_config.sampling_rate, 500.0f);

    ephaptic_fft_bridge_destroy(bridge);
}

TEST_F(PhysicsFFTIntegrationTest, EphapticFFTNyquist) {
    ephaptic_fft_config_t config;
    ephaptic_fft_default_config(&config);
    config.sampling_rate = 1000.0f;

    ephaptic_fft_bridge_t* bridge = ephaptic_fft_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);

    float nyquist = ephaptic_fft_get_nyquist(bridge);
    EXPECT_FLOAT_EQ(nyquist, 500.0f);  // 1000/2

    ephaptic_fft_bridge_destroy(bridge);
}

TEST_F(PhysicsFFTIntegrationTest, EphapticFFTNumBins) {
    ephaptic_fft_config_t config;
    ephaptic_fft_default_config(&config);
    config.fft_size = 1024;

    ephaptic_fft_bridge_t* bridge = ephaptic_fft_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);

    uint32_t bins = ephaptic_fft_get_num_bins(bridge);
    EXPECT_EQ(bins, 513U);  // 1024/2 + 1

    ephaptic_fft_bridge_destroy(bridge);
}

//=============================================================================
// Utility Tests
//=============================================================================

TEST_F(PhysicsFFTIntegrationTest, NextPowerOf2) {
    EXPECT_EQ(fft_next_power_of_2(1000), 1024U);
    EXPECT_EQ(fft_next_power_of_2(1024), 1024U);
    EXPECT_EQ(fft_next_power_of_2(1025), 2048U);
    EXPECT_EQ(fft_next_power_of_2(100), 128U);
}

TEST_F(PhysicsFFTIntegrationTest, IsPowerOf2) {
    EXPECT_TRUE(fft_is_power_of_2(1));
    EXPECT_TRUE(fft_is_power_of_2(2));
    EXPECT_TRUE(fft_is_power_of_2(1024));
    EXPECT_TRUE(fft_is_power_of_2(65536));
    EXPECT_FALSE(fft_is_power_of_2(0));
    EXPECT_FALSE(fft_is_power_of_2(1000));
    EXPECT_FALSE(fft_is_power_of_2(1023));
}

TEST_F(PhysicsFFTIntegrationTest, FrequencyBinConversion) {
    float freq = fft_bin_to_frequency(10, 1024, 1000.0f);
    EXPECT_NEAR(freq, 9.765625f, 0.001f);

    int32_t bin = fft_frequency_to_bin(10.0f, 1024, 1000.0f);
    EXPECT_EQ(bin, 10);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
