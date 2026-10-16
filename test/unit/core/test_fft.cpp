/**
 * @file test_fft.cpp
 * @brief Tests for FFT spectral analysis
 */

#include <gtest/gtest.h>
#include "utils/spectral/nimcp_fft.h"
#include <cmath>

//=============================================================================
// Utility Tests
//=============================================================================

TEST(FFTUtility, PowerOf2Check) {
    EXPECT_TRUE(fft_is_power_of_2(1));
    EXPECT_TRUE(fft_is_power_of_2(2));
    EXPECT_TRUE(fft_is_power_of_2(4));
    EXPECT_TRUE(fft_is_power_of_2(1024));

    EXPECT_FALSE(fft_is_power_of_2(0));
    EXPECT_FALSE(fft_is_power_of_2(3));
    EXPECT_FALSE(fft_is_power_of_2(1000));
}

TEST(FFTUtility, NextPowerOf2) {
    EXPECT_EQ(fft_next_power_of_2(0), 1);
    EXPECT_EQ(fft_next_power_of_2(1), 1);
    EXPECT_EQ(fft_next_power_of_2(2), 2);
    EXPECT_EQ(fft_next_power_of_2(3), 4);
    EXPECT_EQ(fft_next_power_of_2(1000), 1024);
    EXPECT_EQ(fft_next_power_of_2(1024), 1024);
    EXPECT_EQ(fft_next_power_of_2(1025), 2048);
}

TEST(FFTUtility, FrequencyConversion) {
    uint32_t fft_size = 1024;
    float sampling_rate = 1000.0f;  // 1000 Hz

    // Bin 0 = DC (0 Hz)
    EXPECT_FLOAT_EQ(fft_bin_to_frequency(0, fft_size, sampling_rate), 0.0f);

    // Bin 10 ≈ 9.77 Hz
    float freq_10 = fft_bin_to_frequency(10, fft_size, sampling_rate);
    EXPECT_NEAR(freq_10, 9.77f, 0.1f);

    // Nyquist = 500 Hz
    float nyquist = fft_bin_to_frequency(512, fft_size, sampling_rate);
    EXPECT_FLOAT_EQ(nyquist, 500.0f);

    // Round trip
    int32_t bin = fft_frequency_to_bin(freq_10, fft_size, sampling_rate);
    EXPECT_EQ(bin, 10);
}

//=============================================================================
// FFT Plan Tests
//=============================================================================

TEST(FFTPlan, CreateDestroy) {
    fft_plan_t* plan = fft_plan_create(1024, FFT_REAL);
    ASSERT_NE(plan, nullptr);

    EXPECT_EQ(fft_plan_get_size(plan), 1024);

    fft_plan_destroy(plan);
}

TEST(FFTPlan, InvalidSize) {
    // Non-power-of-2
    fft_plan_t* plan1 = fft_plan_create(1000, FFT_REAL);
    EXPECT_EQ(plan1, nullptr);

    // Too small
    fft_plan_t* plan2 = fft_plan_create(1, FFT_REAL);
    EXPECT_EQ(plan2, nullptr);

    // Too large
    fft_plan_t* plan3 = fft_plan_create(1000000, FFT_REAL);
    EXPECT_EQ(plan3, nullptr);
}

TEST(FFTPlan, WindowFunction) {
    fft_plan_t* plan = fft_plan_create(256, FFT_REAL);
    ASSERT_NE(plan, nullptr);

    EXPECT_TRUE(fft_plan_set_window(plan, FFT_WINDOW_HANN));
    EXPECT_TRUE(fft_plan_set_window(plan, FFT_WINDOW_HAMMING));
    EXPECT_TRUE(fft_plan_set_window(plan, FFT_WINDOW_BLACKMAN));
    EXPECT_TRUE(fft_plan_set_window(plan, FFT_WINDOW_NONE));

    fft_plan_destroy(plan);
}

//=============================================================================
// FFT Execution Tests
//=============================================================================

TEST(FFTExecution, RealFFTDC) {
    // Test DC signal (constant)
    uint32_t size = 256;
    float input[256];
    fft_complex_t output[129];  // size/2 + 1

    // DC signal: constant value
    for (uint32_t i = 0; i < size; i++) {
        input[i] = 1.0f;
    }

    fft_plan_t* plan = fft_plan_create(size, FFT_REAL);
    ASSERT_NE(plan, nullptr);

    bool success = fft_execute_real(plan, input, output);
    EXPECT_TRUE(success);

    // DC component should be large
    EXPECT_GT(fabsf(output[0].real), 100.0f);

    // Other bins should be near zero
    for (uint32_t i = 1; i < 10; i++) {
        EXPECT_LT(sqrtf(output[i].real * output[i].real +
                       output[i].imag * output[i].imag), 1.0f);
    }

    fft_plan_destroy(plan);
}

TEST(FFTExecution, RealFFTSineWave) {
    // Test sine wave at known frequency
    uint32_t size = 1024;
    float sampling_rate = 1000.0f;  // 1000 Hz
    float test_freq = 50.0f;         // 50 Hz sine wave

    float* input = new float[size];
    fft_complex_t* output = new fft_complex_t[size/2 + 1];

    // Generate 50 Hz sine wave
    for (uint32_t i = 0; i < size; i++) {
        float t = i / sampling_rate;
        input[i] = sinf(2.0f * M_PI * test_freq * t);
    }

    fft_plan_t* plan = fft_plan_create(size, FFT_REAL);
    ASSERT_NE(plan, nullptr);

    bool success = fft_execute_real(plan, input, output);
    EXPECT_TRUE(success);

    // Compute power spectrum
    float* power = new float[size/2 + 1];
    fft_power_spectrum(output, power, size/2 + 1);

    // Find dominant frequency
    float dominant = fft_dominant_frequency(power, size/2 + 1, sampling_rate);

    // Should be near 50 Hz
    EXPECT_NEAR(dominant, test_freq, 2.0f);

    delete[] input;
    delete[] output;
    delete[] power;
    fft_plan_destroy(plan);
}

//=============================================================================
// Power Spectrum Tests
//=============================================================================

TEST(PowerSpectrum, Magnitude) {
    fft_complex_t spectrum[5] = {
        {3.0f, 4.0f},   // magnitude = 5
        {0.0f, 1.0f},   // magnitude = 1
        {1.0f, 0.0f},   // magnitude = 1
        {5.0f, 12.0f},  // magnitude = 13
        {0.0f, 0.0f}    // magnitude = 0
    };

    float magnitude[5];
    bool success = fft_magnitude_spectrum(spectrum, magnitude, 5);
    EXPECT_TRUE(success);

    EXPECT_FLOAT_EQ(magnitude[0], 5.0f);
    EXPECT_FLOAT_EQ(magnitude[1], 1.0f);
    EXPECT_FLOAT_EQ(magnitude[2], 1.0f);
    EXPECT_FLOAT_EQ(magnitude[3], 13.0f);
    EXPECT_FLOAT_EQ(magnitude[4], 0.0f);
}

TEST(PowerSpectrum, Power) {
    fft_complex_t spectrum[3] = {
        {3.0f, 4.0f},   // power = 25
        {1.0f, 1.0f},   // power = 2
        {2.0f, 0.0f}    // power = 4
    };

    float power[3];
    bool success = fft_power_spectrum(spectrum, power, 3);
    EXPECT_TRUE(success);

    EXPECT_FLOAT_EQ(power[0], 25.0f);
    EXPECT_FLOAT_EQ(power[1], 2.0f);
    EXPECT_FLOAT_EQ(power[2], 4.0f);
}

TEST(PowerSpectrum, PowerDB) {
    fft_complex_t spectrum[2] = {
        {10.0f, 0.0f},   // power = 100, dB = 20
        {1.0f, 0.0f}     // power = 1, dB = 0
    };

    float psd_db[2];
    bool success = fft_power_spectrum_db(spectrum, psd_db, 2);
    EXPECT_TRUE(success);

    EXPECT_NEAR(psd_db[0], 20.0f, 0.1f);
    EXPECT_NEAR(psd_db[1], 0.0f, 0.1f);
}

//=============================================================================
// Brain Wave Tests
//=============================================================================

TEST(BrainWave, BandPower) {
    // Create spectrum with known peaks in different bands
    uint32_t size = 513;  // 1024/2 + 1
    float power[513] = {0};
    float sampling_rate = 1000.0f;

    // Add power in theta band (4-8 Hz)
    // Bins 4-7 (approximately 3.9-6.8 Hz, clearly in theta range)
    for (int i = 4; i <= 7; i++) {
        power[i] = 10.0f;
    }

    float theta_power = fft_brain_wave_power(power, size, sampling_rate,
                                             BRAIN_WAVE_THETA);
    EXPECT_GT(theta_power, 35.0f);  // Should have significant power (4 bins * 10)

    float alpha_power = fft_brain_wave_power(power, size, sampling_rate,
                                             BRAIN_WAVE_ALPHA);
    EXPECT_LT(alpha_power, 5.0f);   // Should have little power
}

//=============================================================================
// Validation Tests
//=============================================================================

TEST(FFTValidation, NullInputs) {
    fft_plan_t* plan = fft_plan_create(256, FFT_REAL);
    ASSERT_NE(plan, nullptr);

    float input[256];
    fft_complex_t output[129];

    EXPECT_FALSE(fft_execute_real(nullptr, input, output));
    EXPECT_FALSE(fft_execute_real(plan, nullptr, output));
    EXPECT_FALSE(fft_execute_real(plan, input, nullptr));

    fft_plan_destroy(plan);
}

TEST(FFTValidation, WindowApplication) {
    float signal[256];
    for (uint32_t i = 0; i < 256; i++) {
        signal[i] = 1.0f;
    }

    bool success = fft_apply_window(signal, 256, FFT_WINDOW_HANN);
    EXPECT_TRUE(success);

    // Hann window should taper at edges
    EXPECT_LT(signal[0], 0.1f);
    EXPECT_LT(signal[255], 0.1f);

    // Peak near center
    EXPECT_GT(signal[128], 0.9f);
}
