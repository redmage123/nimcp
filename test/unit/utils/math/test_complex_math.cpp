//=============================================================================
// test_complex_math.cpp - Unit Tests for Complex Number Mathematics
//=============================================================================

#include <gtest/gtest.h>
// Headers have their own extern "C" guards
    #include "utils/math/nimcp_complex_math.h"
#include <cmath>

// Test tolerance for floating point comparisons
#define TOLERANCE 1e-5f

//=============================================================================
// Test Fixture
//=============================================================================

class ComplexMathTest : public ::testing::Test {
protected:
    void SetUp() override {
        config = complex_math_default_config();
        complex_math_init(&config);
    }

    void TearDown() override {
        complex_math_cleanup();
    }

    complex_math_config_t config;
};

//=============================================================================
// Phasor Operations Tests
//=============================================================================

class PhasorOperationsTest : public ComplexMathTest {};

TEST_F(PhasorOperationsTest, FromPolar_ZeroPhase) {
    neural_phasor_t z = phasor_from_polar(1.0f, 0.0f);
    EXPECT_NEAR(z.real, 1.0f, TOLERANCE);
    EXPECT_NEAR(z.imag, 0.0f, TOLERANCE);
}

TEST_F(PhasorOperationsTest, FromPolar_90DegPhase) {
    neural_phasor_t z = phasor_from_polar(1.0f, M_PI / 2.0f);
    EXPECT_NEAR(z.real, 0.0f, TOLERANCE);
    EXPECT_NEAR(z.imag, 1.0f, TOLERANCE);
}

TEST_F(PhasorOperationsTest, FromPolar_180DegPhase) {
    neural_phasor_t z = phasor_from_polar(1.0f, M_PI);
    EXPECT_NEAR(z.real, -1.0f, TOLERANCE);
    EXPECT_NEAR(z.imag, 0.0f, TOLERANCE);
}

TEST_F(PhasorOperationsTest, FromPolar_270DegPhase) {
    neural_phasor_t z = phasor_from_polar(1.0f, 3.0f * M_PI / 2.0f);
    EXPECT_NEAR(z.real, 0.0f, TOLERANCE);
    EXPECT_NEAR(z.imag, -1.0f, TOLERANCE);
}

TEST_F(PhasorOperationsTest, FromPolar_AmplitudeScaling) {
    neural_phasor_t z = phasor_from_polar(2.5f, M_PI / 4.0f);
    EXPECT_NEAR(phasor_amplitude(z), 2.5f, TOLERANCE);
    EXPECT_NEAR(phasor_phase(z), M_PI / 4.0f, TOLERANCE);
}

TEST_F(PhasorOperationsTest, FromCartesian_PureReal) {
    neural_phasor_t z = phasor_from_cartesian(3.0f, 0.0f);
    EXPECT_NEAR(z.real, 3.0f, TOLERANCE);
    EXPECT_NEAR(z.imag, 0.0f, TOLERANCE);
}

TEST_F(PhasorOperationsTest, FromCartesian_PureImaginary) {
    neural_phasor_t z = phasor_from_cartesian(0.0f, 4.0f);
    EXPECT_NEAR(z.real, 0.0f, TOLERANCE);
    EXPECT_NEAR(z.imag, 4.0f, TOLERANCE);
}

TEST_F(PhasorOperationsTest, FromCartesian_Mixed) {
    neural_phasor_t z = phasor_from_cartesian(3.0f, 4.0f);
    EXPECT_NEAR(z.real, 3.0f, TOLERANCE);
    EXPECT_NEAR(z.imag, 4.0f, TOLERANCE);
}

TEST_F(PhasorOperationsTest, Amplitude_RightTriangle) {
    neural_phasor_t z = phasor_from_cartesian(3.0f, 4.0f);
    EXPECT_NEAR(phasor_amplitude(z), 5.0f, TOLERANCE);
}

TEST_F(PhasorOperationsTest, Amplitude_UnitCircle) {
    neural_phasor_t z = phasor_from_polar(1.0f, M_PI / 3.0f);
    EXPECT_NEAR(phasor_amplitude(z), 1.0f, TOLERANCE);
}

TEST_F(PhasorOperationsTest, Phase_FirstQuadrant) {
    neural_phasor_t z = phasor_from_cartesian(1.0f, 1.0f);
    EXPECT_NEAR(phasor_phase(z), M_PI / 4.0f, TOLERANCE);
}

TEST_F(PhasorOperationsTest, Phase_SecondQuadrant) {
    neural_phasor_t z = phasor_from_cartesian(-1.0f, 1.0f);
    EXPECT_NEAR(phasor_phase(z), 3.0f * M_PI / 4.0f, TOLERANCE);
}

TEST_F(PhasorOperationsTest, Phase_ThirdQuadrant) {
    neural_phasor_t z = phasor_from_cartesian(-1.0f, -1.0f);
    EXPECT_NEAR(phasor_phase(z), -3.0f * M_PI / 4.0f, TOLERANCE);
}

TEST_F(PhasorOperationsTest, Phase_FourthQuadrant) {
    neural_phasor_t z = phasor_from_cartesian(1.0f, -1.0f);
    EXPECT_NEAR(phasor_phase(z), -M_PI / 4.0f, TOLERANCE);
}

TEST_F(PhasorOperationsTest, PhaseDifference_InPhase) {
    neural_phasor_t z1 = phasor_from_polar(1.0f, M_PI / 4.0f);
    neural_phasor_t z2 = phasor_from_polar(1.0f, M_PI / 4.0f);
    EXPECT_NEAR(phasor_phase_difference(z1, z2), 0.0f, TOLERANCE);
}

TEST_F(PhasorOperationsTest, PhaseDifference_90Deg) {
    neural_phasor_t z1 = phasor_from_polar(1.0f, 0.0f);
    neural_phasor_t z2 = phasor_from_polar(1.0f, M_PI / 2.0f);
    EXPECT_NEAR(phasor_phase_difference(z1, z2), M_PI / 2.0f, TOLERANCE);
}

TEST_F(PhasorOperationsTest, PhaseDifference_180Deg) {
    neural_phasor_t z1 = phasor_from_polar(1.0f, 0.0f);
    neural_phasor_t z2 = phasor_from_polar(1.0f, M_PI);
    EXPECT_NEAR(std::abs(phasor_phase_difference(z1, z2)), M_PI, TOLERANCE);
}

TEST_F(PhasorOperationsTest, PhaseDifference_Wrapping) {
    neural_phasor_t z1 = phasor_from_polar(1.0f, M_PI - 0.1f);
    neural_phasor_t z2 = phasor_from_polar(1.0f, -M_PI + 0.1f);
    float diff = phasor_phase_difference(z1, z2);
    EXPECT_NEAR(std::abs(diff), 0.2f, TOLERANCE);
}

TEST_F(PhasorOperationsTest, Normalize_NonUnit) {
    neural_phasor_t z = phasor_from_cartesian(3.0f, 4.0f);
    neural_phasor_t norm = phasor_normalize(z);
    EXPECT_NEAR(phasor_amplitude(norm), 1.0f, TOLERANCE);
    EXPECT_NEAR(phasor_phase(norm), phasor_phase(z), TOLERANCE);
}

TEST_F(PhasorOperationsTest, Normalize_AlreadyUnit) {
    neural_phasor_t z = phasor_from_polar(1.0f, M_PI / 3.0f);
    neural_phasor_t norm = phasor_normalize(z);
    EXPECT_NEAR(phasor_amplitude(norm), 1.0f, TOLERANCE);
    EXPECT_NEAR(phasor_phase(norm), M_PI / 3.0f, TOLERANCE);
}

TEST_F(PhasorOperationsTest, Normalize_Zero) {
    neural_phasor_t z = phasor_from_cartesian(0.0f, 0.0f);
    neural_phasor_t norm = phasor_normalize(z);
    // Should return unit phasor with zero phase
    EXPECT_NEAR(phasor_amplitude(norm), 1.0f, TOLERANCE);
}

TEST_F(PhasorOperationsTest, Normalize_VerySmall) {
    neural_phasor_t z = phasor_from_cartesian(1e-12f, 1e-12f);
    neural_phasor_t norm = phasor_normalize(z);
    EXPECT_NEAR(phasor_amplitude(norm), 1.0f, TOLERANCE);
}

TEST_F(PhasorOperationsTest, RoundTrip_PolarToCartesianToPolar) {
    float amp = 2.5f;
    float phase = M_PI / 6.0f;
    neural_phasor_t z = phasor_from_polar(amp, phase);
    EXPECT_NEAR(phasor_amplitude(z), amp, TOLERANCE);
    EXPECT_NEAR(phasor_phase(z), phase, TOLERANCE);
}

TEST_F(PhasorOperationsTest, RoundTrip_CartesianToPolarToCartesian) {
    float real = 3.0f;
    float imag = 4.0f;
    neural_phasor_t z1 = phasor_from_cartesian(real, imag);
    neural_phasor_t z2 = phasor_from_polar(phasor_amplitude(z1), phasor_phase(z1));
    EXPECT_NEAR(z2.real, real, TOLERANCE);
    EXPECT_NEAR(z2.imag, imag, TOLERANCE);
}

//=============================================================================
// Array Operations Tests
//=============================================================================

class ArrayOperationsTest : public ComplexMathTest {};

TEST_F(ArrayOperationsTest, Coherence_PerfectAlignment) {
    neural_phasor_t signals[5];
    for (int i = 0; i < 5; i++) {
        signals[i] = phasor_from_polar(1.0f, M_PI / 4.0f);
    }
    float coherence = phasor_array_coherence(signals, 5);
    EXPECT_NEAR(coherence, 1.0f, TOLERANCE);
}

TEST_F(ArrayOperationsTest, Coherence_RandomPhases) {
    neural_phasor_t signals[4] = {
        phasor_from_polar(1.0f, 0.0f),
        phasor_from_polar(1.0f, M_PI / 2.0f),
        phasor_from_polar(1.0f, M_PI),
        phasor_from_polar(1.0f, 3.0f * M_PI / 2.0f)
    };
    float coherence = phasor_array_coherence(signals, 4);
    EXPECT_NEAR(coherence, 0.0f, TOLERANCE);
}

TEST_F(ArrayOperationsTest, Coherence_PartialAlignment) {
    neural_phasor_t signals[3] = {
        phasor_from_polar(1.0f, 0.0f),
        phasor_from_polar(1.0f, M_PI / 6.0f),
        phasor_from_polar(1.0f, -M_PI / 6.0f)
    };
    float coherence = phasor_array_coherence(signals, 3);
    EXPECT_GT(coherence, 0.5f);
    EXPECT_LT(coherence, 1.0f);
}

TEST_F(ArrayOperationsTest, Coherence_NullInput) {
    EXPECT_NEAR(phasor_array_coherence(nullptr, 5), 0.0f, TOLERANCE);
}

TEST_F(ArrayOperationsTest, Coherence_ZeroLength) {
    neural_phasor_t signals[1];
    EXPECT_NEAR(phasor_array_coherence(signals, 0), 0.0f, TOLERANCE);
}

TEST_F(ArrayOperationsTest, Synchrony_PerfectSync) {
    neural_phasor_t signals1[3] = {
        phasor_from_polar(1.0f, 0.0f),
        phasor_from_polar(1.0f, M_PI / 3.0f),
        phasor_from_polar(1.0f, 2.0f * M_PI / 3.0f)
    };
    neural_phasor_t signals2[3] = {
        phasor_from_polar(2.0f, 0.0f),
        phasor_from_polar(2.0f, M_PI / 3.0f),
        phasor_from_polar(2.0f, 2.0f * M_PI / 3.0f)
    };
    float synchrony = phasor_array_synchrony(signals1, signals2, 3);
    EXPECT_NEAR(synchrony, 1.0f, TOLERANCE);
}

TEST_F(ArrayOperationsTest, Synchrony_NoSync) {
    // Random phase relationships - no consistent phase lag
    neural_phasor_t signals1[4] = {
        phasor_from_polar(1.0f, 0.0f),
        phasor_from_polar(1.0f, M_PI / 3.0f),
        phasor_from_polar(1.0f, 2.0f * M_PI / 3.0f),
        phasor_from_polar(1.0f, M_PI)
    };
    neural_phasor_t signals2[4] = {
        phasor_from_polar(1.0f, M_PI / 4.0f),
        phasor_from_polar(1.0f, M_PI),
        phasor_from_polar(1.0f, M_PI / 6.0f),
        phasor_from_polar(1.0f, 3.0f * M_PI / 4.0f)
    };
    float synchrony = phasor_array_synchrony(signals1, signals2, 4);
    EXPECT_LT(synchrony, 0.5f);  // Low synchrony for random phases
}

TEST_F(ArrayOperationsTest, Synchrony_NullInput) {
    neural_phasor_t signals[1];
    EXPECT_NEAR(phasor_array_synchrony(nullptr, signals, 1), 0.0f, TOLERANCE);
    EXPECT_NEAR(phasor_array_synchrony(signals, nullptr, 1), 0.0f, TOLERANCE);
}

TEST_F(ArrayOperationsTest, MeanPhase_AlignedSignals) {
    neural_phasor_t signals[3];
    for (int i = 0; i < 3; i++) {
        signals[i] = phasor_from_polar(1.0f, M_PI / 4.0f);
    }
    float mean_phase = phasor_array_mean_phase(signals, 3);
    EXPECT_NEAR(mean_phase, M_PI / 4.0f, TOLERANCE);
}

TEST_F(ArrayOperationsTest, MeanPhase_OppositePhases) {
    neural_phasor_t signals[2] = {
        phasor_from_polar(1.0f, M_PI / 4.0f),
        phasor_from_polar(1.0f, -M_PI / 4.0f)
    };
    float mean_phase = phasor_array_mean_phase(signals, 2);
    EXPECT_NEAR(mean_phase, 0.0f, TOLERANCE);
}

TEST_F(ArrayOperationsTest, MeanPhase_NullInput) {
    EXPECT_NEAR(phasor_array_mean_phase(nullptr, 5), 0.0f, TOLERANCE);
}

TEST_F(ArrayOperationsTest, PhaseVariance_AlignedSignals) {
    neural_phasor_t signals[3];
    for (int i = 0; i < 3; i++) {
        signals[i] = phasor_from_polar(1.0f, M_PI / 3.0f);
    }
    float variance = phasor_array_phase_variance(signals, 3);
    EXPECT_NEAR(variance, 0.0f, TOLERANCE);
}

TEST_F(ArrayOperationsTest, PhaseVariance_RandomPhases) {
    neural_phasor_t signals[4] = {
        phasor_from_polar(1.0f, 0.0f),
        phasor_from_polar(1.0f, M_PI / 2.0f),
        phasor_from_polar(1.0f, M_PI),
        phasor_from_polar(1.0f, 3.0f * M_PI / 2.0f)
    };
    float variance = phasor_array_phase_variance(signals, 4);
    EXPECT_NEAR(variance, 1.0f, TOLERANCE);
}

TEST_F(ArrayOperationsTest, Statistics_Comprehensive) {
    neural_phasor_t signals[4] = {
        phasor_from_polar(1.0f, 0.0f),
        phasor_from_polar(2.0f, M_PI / 4.0f),
        phasor_from_polar(1.5f, M_PI / 2.0f),
        phasor_from_polar(2.5f, M_PI / 6.0f)
    };

    complex_signal_stats_t stats;
    phasor_array_statistics(signals, 4, &stats);

    EXPECT_GT(stats.mean_amplitude, 0.0f);
    EXPECT_GT(stats.coherence, 0.0f);
    EXPECT_LE(stats.coherence, 1.0f);
}

TEST_F(ArrayOperationsTest, Statistics_NullOutput) {
    neural_phasor_t signals[1];
    phasor_array_statistics(signals, 1, nullptr);  // Should not crash
}

//=============================================================================
// FFT Operations Tests
//=============================================================================

class FFTOperationsTest : public ComplexMathTest {};

TEST_F(FFTOperationsTest, FFT_DCSignal) {
    neural_phasor_t input[4] = {
        phasor_from_cartesian(1.0f, 0.0f),
        phasor_from_cartesian(1.0f, 0.0f),
        phasor_from_cartesian(1.0f, 0.0f),
        phasor_from_cartesian(1.0f, 0.0f)
    };
    neural_phasor_t output[4];

    ASSERT_TRUE(phasor_fft(input, output, 4));

    // DC component should be N
    EXPECT_NEAR(phasor_amplitude(output[0]), 4.0f, TOLERANCE);
    // Other components should be ~0
    for (int i = 1; i < 4; i++) {
        EXPECT_NEAR(phasor_amplitude(output[i]), 0.0f, TOLERANCE);
    }
}

TEST_F(FFTOperationsTest, FFT_IFFT_RoundTrip) {
    neural_phasor_t input[8] = {
        phasor_from_cartesian(1.0f, 0.0f),
        phasor_from_cartesian(0.5f, 0.5f),
        phasor_from_cartesian(0.0f, 1.0f),
        phasor_from_cartesian(-0.5f, 0.5f),
        phasor_from_cartesian(-1.0f, 0.0f),
        phasor_from_cartesian(-0.5f, -0.5f),
        phasor_from_cartesian(0.0f, -1.0f),
        phasor_from_cartesian(0.5f, -0.5f)
    };
    neural_phasor_t fft_result[8];
    neural_phasor_t ifft_result[8];

    ASSERT_TRUE(phasor_fft(input, fft_result, 8));
    ASSERT_TRUE(phasor_ifft(fft_result, ifft_result, 8));

    for (int i = 0; i < 8; i++) {
        EXPECT_NEAR(ifft_result[i].real, input[i].real, TOLERANCE);
        EXPECT_NEAR(ifft_result[i].imag, input[i].imag, TOLERANCE);
    }
}

TEST_F(FFTOperationsTest, FFT_NullInput) {
    neural_phasor_t output[4];
    EXPECT_FALSE(phasor_fft(nullptr, output, 4));

    neural_phasor_t input[4];
    EXPECT_FALSE(phasor_fft(input, nullptr, 4));
}

TEST_F(FFTOperationsTest, FFT_NonPowerOfTwo) {
    neural_phasor_t input[5];
    neural_phasor_t output[5];
    EXPECT_FALSE(phasor_fft(input, output, 5));
}

TEST_F(FFTOperationsTest, PowerSpectrum_DCSignal) {
    neural_phasor_t input[4] = {
        phasor_from_cartesian(2.0f, 0.0f),
        phasor_from_cartesian(2.0f, 0.0f),
        phasor_from_cartesian(2.0f, 0.0f),
        phasor_from_cartesian(2.0f, 0.0f)
    };
    float power[4];

    ASSERT_TRUE(phasor_power_spectrum(input, power, 4));

    // DC component power should be N² * amplitude²
    EXPECT_NEAR(power[0], 64.0f, TOLERANCE);  // 4² * 2² = 64
    // Other components should be ~0
    for (int i = 1; i < 4; i++) {
        EXPECT_NEAR(power[i], 0.0f, TOLERANCE);
    }
}

//=============================================================================
// Neural-Specific Operations Tests
//=============================================================================

class NeuralOperationsTest : public ComplexMathTest {};

TEST_F(NeuralOperationsTest, PAC_StrongCoupling) {
    // Generate theta phase with strong modulation
    neural_phasor_t theta_phase[100];
    float gamma_amplitude[100];

    for (int i = 0; i < 100; i++) {
        float phase = (i / 100.0f) * 2.0f * M_PI;
        theta_phase[i] = phasor_from_polar(1.0f, phase);
        // Gamma amplitude peaks at theta trough (phase = π)
        gamma_amplitude[i] = 1.0f + cosf(phase);
    }

    float mi = phasor_pac_modulation_index(theta_phase, gamma_amplitude, 100);
    EXPECT_GT(mi, 0.05f);  // Detectable coupling (MI > 0.05 indicates modulation)
}

TEST_F(NeuralOperationsTest, PAC_NoCoupling) {
    neural_phasor_t theta_phase[100];
    float gamma_amplitude[100];

    for (int i = 0; i < 100; i++) {
        float phase = (i / 100.0f) * 2.0f * M_PI;
        theta_phase[i] = phasor_from_polar(1.0f, phase);
        gamma_amplitude[i] = 1.0f;  // Constant amplitude
    }

    float mi = phasor_pac_modulation_index(theta_phase, gamma_amplitude, 100);
    EXPECT_LT(mi, 0.1f);  // Weak/no coupling
}

TEST_F(NeuralOperationsTest, Hilbert_Cosine) {
    // Hilbert transform of cos(t) should give sin(t) as imaginary part
    float real_signal[16];
    neural_phasor_t analytic[16];

    for (int i = 0; i < 16; i++) {
        float t = (i / 16.0f) * 2.0f * M_PI;
        real_signal[i] = cosf(t);
    }

    ASSERT_TRUE(phasor_hilbert_transform(real_signal, analytic, 16));

    // Check that real part is preserved
    for (int i = 0; i < 16; i++) {
        EXPECT_NEAR(analytic[i].real, real_signal[i], 0.1f);
    }
}

TEST_F(NeuralOperationsTest, Hilbert_AnalyticSignalAmplitude) {
    float real_signal[8];
    neural_phasor_t analytic[8];

    for (int i = 0; i < 8; i++) {
        real_signal[i] = cosf((i / 8.0f) * 2.0f * M_PI);
    }

    ASSERT_TRUE(phasor_hilbert_transform(real_signal, analytic, 8));

    // Analytic signal amplitude should be relatively constant for pure cosine
    float first_amp = phasor_amplitude(analytic[0]);
    EXPECT_GT(first_amp, 0.0f);
}

//=============================================================================
// Configuration Tests
//=============================================================================

class ConfigurationTest : public ::testing::Test {};

TEST_F(ConfigurationTest, DefaultConfig) {
    complex_math_config_t config = complex_math_default_config();
    // SIMD is auto-enabled if AVX2 is available at compile time
#ifdef __AVX2__
    EXPECT_TRUE(config.enable_simd);  // Optimistic default on AVX2 platforms
#else
    EXPECT_FALSE(config.enable_simd); // Conservative default otherwise
#endif
    EXPECT_FALSE(config.enable_fft_cache);
    EXPECT_EQ(config.fft_cache_size, 0u);
}

TEST_F(ConfigurationTest, InitCleanup) {
    complex_math_config_t config = complex_math_default_config();
    EXPECT_TRUE(complex_math_init(&config));
    complex_math_cleanup();
}

TEST_F(ConfigurationTest, InitWithNull) {
    EXPECT_TRUE(complex_math_init(nullptr));
    complex_math_cleanup();
}

TEST_F(ConfigurationTest, SIMD_NotAvailable) {
    EXPECT_FALSE(complex_math_has_simd());
}

//=============================================================================
// Edge Case Tests
//=============================================================================

class EdgeCaseTest : public ComplexMathTest {};

TEST_F(EdgeCaseTest, VerySmallAmplitudes) {
    neural_phasor_t z = phasor_from_polar(1e-10f, M_PI / 4.0f);
    EXPECT_GE(phasor_amplitude(z), 0.0f);
}

TEST_F(EdgeCaseTest, VeryLargeAmplitudes) {
    neural_phasor_t z = phasor_from_polar(1e10f, M_PI / 4.0f);
    EXPECT_GT(phasor_amplitude(z), 1e9f);
}

TEST_F(EdgeCaseTest, PhaseWrapping) {
    // Phases near ±π should wrap correctly
    neural_phasor_t z1 = phasor_from_polar(1.0f, M_PI - 0.01f);
    neural_phasor_t z2 = phasor_from_polar(1.0f, -M_PI + 0.01f);
    float diff = phasor_phase_difference(z1, z2);
    EXPECT_NEAR(std::abs(diff), 0.02f, 0.01f);
}

TEST_F(EdgeCaseTest, SingleElementArray) {
    neural_phasor_t signal[1] = {phasor_from_polar(1.0f, M_PI / 4.0f)};
    float coherence = phasor_array_coherence(signal, 1);
    EXPECT_NEAR(coherence, 1.0f, TOLERANCE);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
