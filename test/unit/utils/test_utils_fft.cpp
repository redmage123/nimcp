/**
 * @file test_utils_fft.cpp
 * @brief Comprehensive unit tests for Fast Fourier Transform utilities
 *
 * WHAT: 100% test coverage for nimcp_fft.c (FFT operations)
 * WHY:  FFT is critical for spectral analysis of neural signals
 * HOW:  Test all operations, edge cases, numerical accuracy, round-trip transforms
 *
 * TEST COVERAGE:
 * 1. fft_plan_create() - plan creation and validation
 * 2. fft_plan_destroy() - proper cleanup
 * 3. fft_plan_set_window() - window function setting
 * 4. fft_execute_real() - real-to-complex FFT
 * 5. fft_execute_inverse_real() - inverse FFT
 * 6. Round-trip transforms (forward + inverse = identity)
 * 7. Known signals (DC, sine waves, impulse)
 * 8. fft_power_spectrum() - power spectrum calculation
 * 9. fft_magnitude_spectrum() - magnitude spectrum
 * 10. fft_phase_spectrum() - phase spectrum
 * 11. fft_dominant_frequency() - peak frequency detection
 * 12. fft_band_power() - frequency band power
 * 13. fft_brain_wave_power() - neuroscience band analysis
 * 14. Utility functions (power of 2, bin conversion)
 * 15. Edge cases (NULL, size 0, size 1, non-power-of-2)
 * 16. Different FFT sizes (2, 4, 8, 16, 64, 1024)
 *
 * @version Unit Testing Framework v1.0
 * @date 2025-11-13
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>

    #include "utils/spectral/nimcp_fft.h"

//=============================================================================
// Test Fixture
//=============================================================================

class FFTTest : public ::testing::Test {
protected:
    static constexpr float EPSILON = 1e-4f;  // Looser tolerance for FFT
    static constexpr float M_PI_F = 3.14159265358979323846f;

    bool FloatEqual(float a, float b) {
        return std::abs(a - b) < EPSILON;
    }

    bool ComplexEqual(const fft_complex_t& a, const fft_complex_t& b) {
        return FloatEqual(a.real, b.real) && FloatEqual(a.imag, b.imag);
    }

    // Helper: Generate sine wave
    void GenerateSine(float* signal, uint32_t size, float frequency, float sampling_rate) {
        for (uint32_t i = 0; i < size; i++) {
            float t = (float)i / sampling_rate;
            signal[i] = sinf(2.0f * M_PI_F * frequency * t);
        }
    }

    // Helper: Generate DC signal
    void GenerateDC(float* signal, uint32_t size, float value) {
        for (uint32_t i = 0; i < size; i++) {
            signal[i] = value;
        }
    }

    // Helper: Generate impulse
    void GenerateImpulse(float* signal, uint32_t size, uint32_t position) {
        for (uint32_t i = 0; i < size; i++) {
            signal[i] = (i == position) ? 1.0f : 0.0f;
        }
    }
};

//=============================================================================
// Unit Test 1: Utility - Is power of 2
//=============================================================================

TEST_F(FFTTest, IsPowerOf2_ValidCases) {
    // WHAT: Verify power-of-2 detection
    // WHY:  FFT requires power-of-2 sizes

    EXPECT_TRUE(fft_is_power_of_2(1));
    EXPECT_TRUE(fft_is_power_of_2(2));
    EXPECT_TRUE(fft_is_power_of_2(4));
    EXPECT_TRUE(fft_is_power_of_2(8));
    EXPECT_TRUE(fft_is_power_of_2(16));
    EXPECT_TRUE(fft_is_power_of_2(1024));
    EXPECT_TRUE(fft_is_power_of_2(65536));
    SUCCEED() << "Power-of-2 detection works for valid cases";
}

TEST_F(FFTTest, IsPowerOf2_InvalidCases) {
    // WHAT: Verify non-power-of-2 rejection
    // WHY:  Must reject invalid sizes

    EXPECT_FALSE(fft_is_power_of_2(0));
    EXPECT_FALSE(fft_is_power_of_2(3));
    EXPECT_FALSE(fft_is_power_of_2(5));
    EXPECT_FALSE(fft_is_power_of_2(100));
    EXPECT_FALSE(fft_is_power_of_2(1000));
    SUCCEED() << "Power-of-2 detection rejects invalid cases";
}

//=============================================================================
// Unit Test 2: Utility - Next power of 2
//=============================================================================

TEST_F(FFTTest, NextPowerOf2_Basic) {
    // WHAT: Verify next-power-of-2 calculation
    // WHY:  Used to pad signals to FFT-friendly sizes

    EXPECT_EQ(fft_next_power_of_2(1), 1u);
    EXPECT_EQ(fft_next_power_of_2(2), 2u);
    EXPECT_EQ(fft_next_power_of_2(3), 4u);
    EXPECT_EQ(fft_next_power_of_2(5), 8u);
    EXPECT_EQ(fft_next_power_of_2(100), 128u);
    EXPECT_EQ(fft_next_power_of_2(1000), 1024u);
    EXPECT_EQ(fft_next_power_of_2(1024), 1024u);
    SUCCEED() << "Next power-of-2 calculation works";
}

//=============================================================================
// Unit Test 3: Plan creation - valid sizes
//=============================================================================

TEST_F(FFTTest, PlanCreate_ValidSizes) {
    // WHAT: Verify plan creation for valid sizes
    // WHY:  Plan is required for all FFT operations

    std::vector<uint32_t> valid_sizes = {2, 4, 8, 16, 32, 64, 128, 256, 512, 1024};

    for (uint32_t size : valid_sizes) {
        fft_plan_t* plan = fft_plan_create(size, FFT_REAL);
        ASSERT_NE(plan, nullptr) << "Failed to create plan for size " << size;
        EXPECT_EQ(fft_plan_get_size(plan), size);
        fft_plan_destroy(plan);
    }

    SUCCEED() << "Plan creation works for all valid sizes";
}

//=============================================================================
// Unit Test 4: Plan creation - invalid sizes
//=============================================================================

TEST_F(FFTTest, PlanCreate_InvalidSizes) {
    // WHAT: Verify plan creation fails for invalid sizes
    // WHY:  Must reject non-power-of-2 and out-of-range sizes

    EXPECT_EQ(fft_plan_create(0, FFT_REAL), nullptr);
    EXPECT_EQ(fft_plan_create(1, FFT_REAL), nullptr);  // Too small
    EXPECT_EQ(fft_plan_create(3, FFT_REAL), nullptr);  // Not power of 2
    EXPECT_EQ(fft_plan_create(100, FFT_REAL), nullptr);  // Not power of 2
    EXPECT_EQ(fft_plan_create(100000, FFT_REAL), nullptr);  // Too large
    SUCCEED() << "Plan creation rejects invalid sizes";
}

//=============================================================================
// Unit Test 5: Plan destruction - NULL safety
//=============================================================================

TEST_F(FFTTest, PlanDestroy_NullSafe) {
    // WHAT: Verify destroy handles NULL safely
    // WHY:  Prevent crashes on NULL pointers

    fft_plan_destroy(nullptr);  // Should not crash
    SUCCEED() << "Plan destruction is NULL-safe";
}

//=============================================================================
// Unit Test 6: Window function setting
//=============================================================================

TEST_F(FFTTest, WindowFunction_AllTypes) {
    // WHAT: Verify window function can be set
    // WHY:  Windows reduce spectral leakage

    fft_plan_t* plan = fft_plan_create(64, FFT_REAL);
    ASSERT_NE(plan, nullptr);

    EXPECT_TRUE(fft_plan_set_window(plan, FFT_WINDOW_NONE));
    EXPECT_TRUE(fft_plan_set_window(plan, FFT_WINDOW_HANN));
    EXPECT_TRUE(fft_plan_set_window(plan, FFT_WINDOW_HAMMING));
    EXPECT_TRUE(fft_plan_set_window(plan, FFT_WINDOW_BLACKMAN));

    fft_plan_destroy(plan);
    SUCCEED() << "All window functions can be set";
}

//=============================================================================
// Unit Test 7: DC signal (constant value)
//=============================================================================

TEST_F(FFTTest, RealFFT_DCSignal) {
    // WHAT: Verify FFT of DC signal (constant value)
    // WHY:  DC should appear only in bin 0
    // HOW:  FFT([1,1,1,1,...]) = [N, 0, 0, 0,...]

    const uint32_t size = 16;
    fft_plan_t* plan = fft_plan_create(size, FFT_REAL);
    ASSERT_NE(plan, nullptr);

    float signal[size];
    GenerateDC(signal, size, 1.0f);

    fft_complex_t spectrum[size/2 + 1];
    ASSERT_TRUE(fft_execute_real(plan, signal, spectrum));

    // DC component should be N
    EXPECT_TRUE(FloatEqual(spectrum[0].real, (float)size));
    EXPECT_TRUE(FloatEqual(spectrum[0].imag, 0.0f));

    // All other components should be ~0
    for (uint32_t i = 1; i < size/2 + 1; i++) {
        EXPECT_TRUE(FloatEqual(spectrum[i].real, 0.0f)) << "Non-zero at bin " << i;
        EXPECT_TRUE(FloatEqual(spectrum[i].imag, 0.0f)) << "Non-zero at bin " << i;
    }

    fft_plan_destroy(plan);
    SUCCEED() << "DC signal FFT is correct";
}

//=============================================================================
// Unit Test 8: Impulse signal
//=============================================================================

TEST_F(FFTTest, RealFFT_ImpulseSignal) {
    // WHAT: Verify FFT of impulse (delta function)
    // WHY:  Impulse has flat spectrum (all frequencies equal)
    // HOW:  FFT([1,0,0,0,...]) = [1,1,1,1,...]

    const uint32_t size = 16;
    fft_plan_t* plan = fft_plan_create(size, FFT_REAL);
    ASSERT_NE(plan, nullptr);

    float signal[size];
    GenerateImpulse(signal, size, 0);

    fft_complex_t spectrum[size/2 + 1];
    ASSERT_TRUE(fft_execute_real(plan, signal, spectrum));

    // All magnitudes should be ~1
    for (uint32_t i = 0; i < size/2 + 1; i++) {
        float magnitude = sqrtf(spectrum[i].real * spectrum[i].real +
                                spectrum[i].imag * spectrum[i].imag);
        EXPECT_TRUE(FloatEqual(magnitude, 1.0f)) << "Magnitude at bin " << i;
    }

    fft_plan_destroy(plan);
    SUCCEED() << "Impulse signal FFT is correct";
}

//=============================================================================
// Unit Test 9: Sine wave - single frequency
//=============================================================================

TEST_F(FFTTest, RealFFT_SineWave) {
    // WHAT: Verify FFT of pure sine wave
    // WHY:  Should have peak at known frequency
    // HOW:  Generate 10 Hz sine at 100 Hz sampling rate

    const uint32_t size = 64;
    const float sampling_rate = 100.0f;
    const float frequency = 10.0f;

    fft_plan_t* plan = fft_plan_create(size, FFT_REAL);
    ASSERT_NE(plan, nullptr);

    float signal[size];
    GenerateSine(signal, size, frequency, sampling_rate);

    fft_complex_t spectrum[size/2 + 1];
    ASSERT_TRUE(fft_execute_real(plan, signal, spectrum));

    // Find expected bin
    int32_t expected_bin = fft_frequency_to_bin(frequency, size, sampling_rate);
    ASSERT_GE(expected_bin, 0);

    // Power should be highest at expected bin
    float max_power = 0.0f;
    uint32_t max_bin = 0;

    for (uint32_t i = 1; i < size/2 + 1; i++) {  // Skip DC
        float power = spectrum[i].real * spectrum[i].real +
                      spectrum[i].imag * spectrum[i].imag;
        if (power > max_power) {
            max_power = power;
            max_bin = i;
        }
    }

    EXPECT_EQ(max_bin, (uint32_t)expected_bin) << "Peak at wrong frequency";

    fft_plan_destroy(plan);
    SUCCEED() << "Sine wave FFT shows correct peak frequency";
}

//=============================================================================
// Unit Test 10: Inverse FFT - round trip
//=============================================================================

TEST_F(FFTTest, InverseFFT_RoundTrip) {
    // WHAT: Verify forward + inverse = identity
    // WHY:  FFT and IFFT should be perfect inverses
    // HOW:  FFT(signal) → IFFT(spectrum) = signal

    const uint32_t size = 32;

    // Create plans
    fft_plan_t* plan_fwd = fft_plan_create(size, FFT_REAL);
    fft_plan_t* plan_inv = fft_plan_create(size, FFT_INVERSE);
    ASSERT_NE(plan_fwd, nullptr);
    ASSERT_NE(plan_inv, nullptr);

    // Original signal (arbitrary)
    float original[size];
    for (uint32_t i = 0; i < size; i++) {
        original[i] = sinf(i * 0.5f) + cosf(i * 0.3f);
    }

    // Forward FFT
    fft_complex_t spectrum[size/2 + 1];
    ASSERT_TRUE(fft_execute_real(plan_fwd, original, spectrum));

    // Inverse FFT
    float reconstructed[size];
    ASSERT_TRUE(fft_execute_inverse_real(plan_inv, spectrum, reconstructed));

    // Compare
    for (uint32_t i = 0; i < size; i++) {
        EXPECT_TRUE(FloatEqual(original[i], reconstructed[i]))
            << "Mismatch at index " << i << ": " << original[i] << " vs " << reconstructed[i];
    }

    fft_plan_destroy(plan_fwd);
    fft_plan_destroy(plan_inv);
    SUCCEED() << "Round-trip FFT preserves signal";
}

//=============================================================================
// Unit Test 11: Power spectrum
//=============================================================================

TEST_F(FFTTest, PowerSpectrum_Calculation) {
    // WHAT: Verify power spectrum calculation
    // WHY:  Power = real^2 + imag^2
    // HOW:  Manual calculation vs function

    const uint32_t size = 8;
    fft_complex_t spectrum[size] = {
        {3.0f, 4.0f},   // |.|^2 = 9 + 16 = 25
        {1.0f, 0.0f},   // |.|^2 = 1
        {0.0f, 1.0f},   // |.|^2 = 1
        {2.0f, 2.0f},   // |.|^2 = 4 + 4 = 8
        {0.0f, 0.0f},   // |.|^2 = 0
        {-1.0f, 1.0f},  // |.|^2 = 1 + 1 = 2
        {5.0f, 0.0f},   // |.|^2 = 25
        {0.0f, -3.0f}   // |.|^2 = 9
    };

    float expected_power[] = {25.0f, 1.0f, 1.0f, 8.0f, 0.0f, 2.0f, 25.0f, 9.0f};
    float power[size];

    ASSERT_TRUE(fft_power_spectrum(spectrum, power, size));

    for (uint32_t i = 0; i < size; i++) {
        EXPECT_TRUE(FloatEqual(power[i], expected_power[i]))
            << "Power mismatch at bin " << i;
    }

    SUCCEED() << "Power spectrum calculation is correct";
}

//=============================================================================
// Unit Test 12: Magnitude spectrum
//=============================================================================

TEST_F(FFTTest, MagnitudeSpectrum_Calculation) {
    // WHAT: Verify magnitude spectrum calculation
    // WHY:  Magnitude = sqrt(real^2 + imag^2)
    // HOW:  Known complex values

    const uint32_t size = 4;
    fft_complex_t spectrum[size] = {
        {3.0f, 4.0f},   // |.| = 5
        {0.0f, 0.0f},   // |.| = 0
        {1.0f, 0.0f},   // |.| = 1
        {0.0f, -1.0f}   // |.| = 1
    };

    float expected_mag[] = {5.0f, 0.0f, 1.0f, 1.0f};
    float magnitude[size];

    ASSERT_TRUE(fft_magnitude_spectrum(spectrum, magnitude, size));

    for (uint32_t i = 0; i < size; i++) {
        EXPECT_TRUE(FloatEqual(magnitude[i], expected_mag[i]))
            << "Magnitude mismatch at bin " << i;
    }

    SUCCEED() << "Magnitude spectrum calculation is correct";
}

//=============================================================================
// Unit Test 13: Phase spectrum
//=============================================================================

TEST_F(FFTTest, PhaseSpectrum_Calculation) {
    // WHAT: Verify phase spectrum calculation
    // WHY:  Phase = atan2(imag, real)
    // HOW:  Known complex values with known phases

    const uint32_t size = 4;
    fft_complex_t spectrum[size] = {
        {1.0f, 0.0f},       // phase = 0
        {0.0f, 1.0f},       // phase = π/2
        {-1.0f, 0.0f},      // phase = π
        {1.0f, 1.0f}        // phase = π/4
    };

    float expected_phase[] = {0.0f, M_PI_F/2.0f, M_PI_F, M_PI_F/4.0f};
    float phase[size];

    ASSERT_TRUE(fft_phase_spectrum(spectrum, phase, size));

    for (uint32_t i = 0; i < size; i++) {
        EXPECT_TRUE(FloatEqual(phase[i], expected_phase[i]))
            << "Phase mismatch at bin " << i;
    }

    SUCCEED() << "Phase spectrum calculation is correct";
}

//=============================================================================
// Unit Test 14: Dominant frequency detection
//=============================================================================

TEST_F(FFTTest, DominantFrequency_Detection) {
    // WHAT: Verify dominant frequency detection
    // WHY:  Find peak in power spectrum
    // HOW:  Power spectrum with known peak

    const uint32_t size = 8;
    const float sampling_rate = 100.0f;

    float power[size] = {0.5f, 1.0f, 5.0f, 2.0f, 1.5f, 0.8f, 0.3f, 0.1f};

    float dominant_freq = fft_dominant_frequency(power, size, sampling_rate);

    // Peak is at bin 2
    // Frequency = bin * (sampling_rate / (size * 2))
    //           = 2 * (100 / 16) = 12.5 Hz
    float expected_freq = 2.0f * sampling_rate / (size * 2);

    EXPECT_TRUE(FloatEqual(dominant_freq, expected_freq))
        << "Expected " << expected_freq << " Hz, got " << dominant_freq << " Hz";

    SUCCEED() << "Dominant frequency detection works";
}

//=============================================================================
// Unit Test 15: Band power calculation
//=============================================================================

TEST_F(FFTTest, BandPower_Calculation) {
    // WHAT: Verify frequency band power calculation
    // WHY:  Measure power in specific frequency ranges
    // HOW:  Known power spectrum, sum specific bins

    const uint32_t size = 16;  // 16 bins = 32-point FFT
    const float sampling_rate = 100.0f;  // 100 Hz sampling
    // Bin resolution: 100/32 = 3.125 Hz per bin

    float power[size];
    for (uint32_t i = 0; i < size; i++) {
        power[i] = 1.0f;  // Uniform power
    }

    // Test band: 10-20 Hz
    // Bins: 10/3.125 = 3.2 (bin 3) to 20/3.125 = 6.4 (bin 6)
    // Sum of bins 3,4,5,6 = 4.0
    float band_power = fft_band_power(power, size, sampling_rate, 10.0f, 20.0f);

    EXPECT_GT(band_power, 3.0f);  // Should be ~4
    EXPECT_LT(band_power, 5.0f);

    SUCCEED() << "Band power calculation works";
}

//=============================================================================
// Unit Test 16: Brain wave band power
//=============================================================================

TEST_F(FFTTest, BrainWavePower_AllBands) {
    // WHAT: Verify brain wave band power calculation
    // WHY:  Neuroscience-specific frequency bands
    // HOW:  Test all standard bands

    const uint32_t size = 256;  // 512-point FFT
    const float sampling_rate = 250.0f;  // 250 Hz (typical EEG)

    float power[size];
    for (uint32_t i = 0; i < size; i++) {
        power[i] = 1.0f;
    }

    // All bands should return non-zero power
    EXPECT_GT(fft_brain_wave_power(power, size, sampling_rate, BRAIN_WAVE_DELTA), 0.0f);
    EXPECT_GT(fft_brain_wave_power(power, size, sampling_rate, BRAIN_WAVE_THETA), 0.0f);
    EXPECT_GT(fft_brain_wave_power(power, size, sampling_rate, BRAIN_WAVE_ALPHA), 0.0f);
    EXPECT_GT(fft_brain_wave_power(power, size, sampling_rate, BRAIN_WAVE_BETA), 0.0f);
    EXPECT_GT(fft_brain_wave_power(power, size, sampling_rate, BRAIN_WAVE_GAMMA), 0.0f);

    SUCCEED() << "Brain wave band power calculation works for all bands";
}

//=============================================================================
// Unit Test 17: Frequency to bin conversion
//=============================================================================

TEST_F(FFTTest, FrequencyToBin_Conversion) {
    // WHAT: Verify frequency-to-bin conversion
    // WHY:  Map Hz to array indices
    // HOW:  Known conversions

    const uint32_t fft_size = 64;
    const float sampling_rate = 128.0f;

    // Bin resolution = 128/64 = 2 Hz per bin
    EXPECT_EQ(fft_frequency_to_bin(0.0f, fft_size, sampling_rate), 0);
    EXPECT_EQ(fft_frequency_to_bin(2.0f, fft_size, sampling_rate), 1);
    EXPECT_EQ(fft_frequency_to_bin(4.0f, fft_size, sampling_rate), 2);
    EXPECT_EQ(fft_frequency_to_bin(10.0f, fft_size, sampling_rate), 5);

    // Out of range
    EXPECT_EQ(fft_frequency_to_bin(-1.0f, fft_size, sampling_rate), -1);
    EXPECT_EQ(fft_frequency_to_bin(200.0f, fft_size, sampling_rate), -1);

    SUCCEED() << "Frequency-to-bin conversion works";
}

//=============================================================================
// Unit Test 18: Bin to frequency conversion
//=============================================================================

TEST_F(FFTTest, BinToFrequency_Conversion) {
    // WHAT: Verify bin-to-frequency conversion
    // WHY:  Label frequency axes in plots
    // HOW:  Known conversions

    const uint32_t fft_size = 64;
    const float sampling_rate = 128.0f;

    // Bin resolution = 128/64 = 2 Hz per bin
    EXPECT_TRUE(FloatEqual(fft_bin_to_frequency(0, fft_size, sampling_rate), 0.0f));
    EXPECT_TRUE(FloatEqual(fft_bin_to_frequency(1, fft_size, sampling_rate), 2.0f));
    EXPECT_TRUE(FloatEqual(fft_bin_to_frequency(5, fft_size, sampling_rate), 10.0f));
    EXPECT_TRUE(FloatEqual(fft_bin_to_frequency(32, fft_size, sampling_rate), 64.0f));

    SUCCEED() << "Bin-to-frequency conversion works";
}

//=============================================================================
// Unit Test 19: Edge case - NULL inputs
//=============================================================================

TEST_F(FFTTest, EdgeCase_NullInputs) {
    // WHAT: Verify NULL input handling
    // WHY:  Prevent crashes
    // HOW:  Pass NULL to all functions

    fft_plan_t* plan = fft_plan_create(16, FFT_REAL);
    ASSERT_NE(plan, nullptr);

    float signal[16];
    fft_complex_t spectrum[9];
    float power[9];

    // NULL plan
    EXPECT_FALSE(fft_execute_real(nullptr, signal, spectrum));
    EXPECT_FALSE(fft_plan_set_window(nullptr, FFT_WINDOW_HANN));
    EXPECT_EQ(fft_plan_get_size(nullptr), 0u);

    // NULL data
    EXPECT_FALSE(fft_execute_real(plan, nullptr, spectrum));
    EXPECT_FALSE(fft_execute_real(plan, signal, nullptr));
    EXPECT_FALSE(fft_power_spectrum(nullptr, power, 9));
    EXPECT_FALSE(fft_power_spectrum(spectrum, nullptr, 9));

    fft_plan_destroy(plan);
    SUCCEED() << "NULL input handling is robust";
}

//=============================================================================
// Unit Test 20: Edge case - size zero
//=============================================================================

TEST_F(FFTTest, EdgeCase_ZeroSize) {
    // WHAT: Verify zero-size handling
    // WHY:  Prevent invalid operations
    // HOW:  Pass size=0 to functions

    fft_complex_t spectrum[1];
    float power[1];

    EXPECT_FALSE(fft_power_spectrum(spectrum, power, 0));
    EXPECT_EQ(fft_dominant_frequency(power, 0, 100.0f), 0.0f);
    EXPECT_EQ(fft_band_power(power, 0, 100.0f, 10.0f, 20.0f), 0.0f);

    SUCCEED() << "Zero-size handling is robust";
}

//=============================================================================
// Unit Test 21: Different FFT sizes
//=============================================================================

TEST_F(FFTTest, DifferentSizes_AllWork) {
    // WHAT: Verify FFT works for all supported sizes
    // WHY:  Test scalability and correctness
    // HOW:  Test sizes from 2 to 1024

    std::vector<uint32_t> sizes = {2, 4, 8, 16, 32, 64, 128, 256, 512, 1024};

    for (uint32_t size : sizes) {
        fft_plan_t* plan_fwd = fft_plan_create(size, FFT_REAL);
        fft_plan_t* plan_inv = fft_plan_create(size, FFT_INVERSE);
        ASSERT_NE(plan_fwd, nullptr) << "Failed for size " << size;
        ASSERT_NE(plan_inv, nullptr) << "Failed for size " << size;

        // Test DC signal
        std::vector<float> signal(size, 1.0f);
        std::vector<fft_complex_t> spectrum(size/2 + 1);
        std::vector<float> reconstructed(size);

        ASSERT_TRUE(fft_execute_real(plan_fwd, signal.data(), spectrum.data()))
            << "Forward FFT failed for size " << size;

        ASSERT_TRUE(fft_execute_inverse_real(plan_inv, spectrum.data(), reconstructed.data()))
            << "Inverse FFT failed for size " << size;

        // Check reconstruction
        for (uint32_t i = 0; i < size; i++) {
            EXPECT_TRUE(FloatEqual(signal[i], reconstructed[i]))
                << "Reconstruction error at size " << size << ", index " << i;
        }

        fft_plan_destroy(plan_fwd);
        fft_plan_destroy(plan_inv);
    }

    SUCCEED() << "FFT works correctly for all supported sizes";
}

//=============================================================================
// Unit Test 22: Complex FFT execution
//=============================================================================

TEST_F(FFTTest, ComplexFFT_Execution) {
    // WHAT: Verify complex-to-complex FFT
    // WHY:  Test general complex case
    // HOW:  Complex input with known properties

    const uint32_t size = 16;
    fft_plan_t* plan = fft_plan_create(size, FFT_COMPLEX);
    ASSERT_NE(plan, nullptr);

    // Create complex input (real = 1, imag = 0)
    fft_complex_t input[size];
    fft_complex_t output[size];
    for (uint32_t i = 0; i < size; i++) {
        input[i].real = 1.0f;
        input[i].imag = 0.0f;
    }

    ASSERT_TRUE(fft_execute_complex(plan, input, output));

    // DC component should be N
    EXPECT_TRUE(FloatEqual(output[0].real, (float)size));
    EXPECT_TRUE(FloatEqual(output[0].imag, 0.0f));

    fft_plan_destroy(plan);
    SUCCEED() << "Complex FFT execution works";
}

//=============================================================================
// Unit Test 23: Power spectrum in dB
//=============================================================================

TEST_F(FFTTest, PowerSpectrumDB_Calculation) {
    // WHAT: Verify dB power spectrum calculation
    // WHY:  Logarithmic scale for wide dynamic range
    // HOW:  Known power values → known dB values

    const uint32_t size = 4;
    fft_complex_t spectrum[size] = {
        {10.0f, 0.0f},   // power = 100, dB = 20
        {1.0f, 0.0f},    // power = 1, dB = 0
        {0.1f, 0.0f},    // power = 0.01, dB = -20
        {3.0f, 4.0f}     // power = 25, dB = ~14
    };

    float psd_db[size];
    ASSERT_TRUE(fft_power_spectrum_db(spectrum, psd_db, size));

    EXPECT_TRUE(FloatEqual(psd_db[0], 20.0f));
    EXPECT_TRUE(FloatEqual(psd_db[1], 0.0f));
    EXPECT_TRUE(FloatEqual(psd_db[2], -20.0f));
    EXPECT_TRUE(FloatEqual(psd_db[3], 10.0f * log10f(25.0f)));

    SUCCEED() << "Power spectrum dB calculation is correct";
}

//=============================================================================
// SUMMARY
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
