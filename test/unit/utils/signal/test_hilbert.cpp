//=============================================================================
// test_hilbert.cpp - Hilbert Transform Unit Tests
//=============================================================================

#include <gtest/gtest.h>
#include "utils/signal/nimcp_hilbert.h"
#include "utils/math/nimcp_complex_math.h"
#include <cmath>
#include <vector>

class HilbertTest : public ::testing::Test {
protected:
    hilbert_transform_t* ht = nullptr;

    void SetUp() override {
        complex_math_init(nullptr);
        hilbert_config_t config = hilbert_default_config();
        ht = hilbert_create(&config);
        ASSERT_NE(ht, nullptr);
    }

    void TearDown() override {
        if (ht != nullptr) {
            hilbert_destroy(ht);
            ht = nullptr;
        }
        complex_math_cleanup();
    }

    // Generate test signal: pure sinusoid
    std::vector<float> generate_sine(float freq, float sample_rate, uint32_t n) {
        std::vector<float> signal(n);
        for (uint32_t i = 0; i < n; i++) {
            float t = static_cast<float>(i) / sample_rate;
            signal[i] = std::sin(2.0f * M_PI * freq * t);
        }
        return signal;
    }

    // Generate test signal: AM modulated
    std::vector<float> generate_am_signal(float carrier_freq, float mod_freq,
                                           float sample_rate, uint32_t n) {
        std::vector<float> signal(n);
        for (uint32_t i = 0; i < n; i++) {
            float t = static_cast<float>(i) / sample_rate;
            float amplitude = 1.0f + 0.5f * std::sin(2.0f * M_PI * mod_freq * t);
            signal[i] = amplitude * std::sin(2.0f * M_PI * carrier_freq * t);
        }
        return signal;
    }
};

//=============================================================================
// Configuration Tests
//=============================================================================

TEST_F(HilbertTest, DefaultConfig) {
    hilbert_config_t config = hilbert_default_config();
    EXPECT_TRUE(config.auto_pad_power_of_2);
    EXPECT_TRUE(config.enable_simd);
    EXPECT_EQ(config.max_signal_length, 4096u);
}

TEST_F(HilbertTest, ValidateConfig) {
    hilbert_config_t valid_config = hilbert_default_config();
    EXPECT_TRUE(hilbert_validate_config(&valid_config));

    hilbert_config_t invalid_config = hilbert_default_config();
    invalid_config.max_signal_length = 0;
    EXPECT_FALSE(hilbert_validate_config(&invalid_config));

    EXPECT_FALSE(hilbert_validate_config(nullptr));
}

//=============================================================================
// Basic Transform Tests
//=============================================================================

TEST_F(HilbertTest, PowerOf2Transform) {
    const uint32_t n = 1024;
    auto signal = generate_sine(10.0f, 1000.0f, n);
    std::vector<neural_phasor_t> analytic(n);

    EXPECT_TRUE(hilbert_apply(ht, signal.data(), analytic.data(), n));

    // Check that analytic signal has non-zero imaginary part
    bool has_imag = false;
    for (uint32_t i = 0; i < n; i++) {
        if (std::abs(analytic[i].imag) > 1e-6f) {
            has_imag = true;
            break;
        }
    }
    EXPECT_TRUE(has_imag);
}

TEST_F(HilbertTest, NonPowerOf2Transform) {
    const uint32_t n = 1000;  // Not a power of 2
    auto signal = generate_sine(10.0f, 1000.0f, n);
    std::vector<neural_phasor_t> analytic(n);

    EXPECT_TRUE(hilbert_apply(ht, signal.data(), analytic.data(), n));
}

TEST_F(HilbertTest, NullInputs) {
    const uint32_t n = 1024;
    auto signal = generate_sine(10.0f, 1000.0f, n);
    std::vector<neural_phasor_t> analytic(n);

    EXPECT_FALSE(hilbert_apply(nullptr, signal.data(), analytic.data(), n));
    EXPECT_FALSE(hilbert_apply(ht, nullptr, analytic.data(), n));
    EXPECT_FALSE(hilbert_apply(ht, signal.data(), nullptr, n));
    EXPECT_FALSE(hilbert_apply(ht, signal.data(), analytic.data(), 0));
}

//=============================================================================
// Amplitude Extraction Tests
//=============================================================================

TEST_F(HilbertTest, ExtractAmplitudeConstant) {
    const uint32_t n = 1024;
    const float freq = 10.0f;
    const float sample_rate = 1000.0f;

    auto signal = generate_sine(freq, sample_rate, n);
    std::vector<float> amplitude(n);

    EXPECT_TRUE(hilbert_extract_amplitude(ht, signal.data(), amplitude.data(), n));

    // For a pure sinusoid with amplitude 1, envelope should be close to 1
    float mean_amp = 0.0f;
    for (uint32_t i = 100; i < n - 100; i++) {  // Skip edges
        mean_amp += amplitude[i];
    }
    mean_amp /= (n - 200);

    EXPECT_NEAR(mean_amp, 1.0f, 0.1f);  // Should be close to 1.0
}

TEST_F(HilbertTest, ExtractAmplitudeModulated) {
    const uint32_t n = 1024;
    const float carrier_freq = 40.0f;  // Gamma
    const float mod_freq = 6.0f;       // Theta modulation
    const float sample_rate = 1000.0f;

    auto signal = generate_am_signal(carrier_freq, mod_freq, sample_rate, n);
    std::vector<float> amplitude(n);

    EXPECT_TRUE(hilbert_extract_amplitude(ht, signal.data(), amplitude.data(), n));

    // Amplitude envelope should vary with modulation frequency
    // Find max and min in stable region
    float max_amp = 0.0f;
    float min_amp = 1e6f;
    for (uint32_t i = 100; i < n - 100; i++) {
        if (amplitude[i] > max_amp) max_amp = amplitude[i];
        if (amplitude[i] < min_amp) min_amp = amplitude[i];
    }

    // AM signal has amplitude 1 ± 0.5, so envelope should range ~[0.5, 1.5]
    EXPECT_GT(max_amp, 1.2f);
    EXPECT_LT(min_amp, 0.8f);
}

//=============================================================================
// Phase Extraction Tests
//=============================================================================

TEST_F(HilbertTest, ExtractPhase) {
    const uint32_t n = 1024;
    const float freq = 10.0f;
    const float sample_rate = 1000.0f;

    auto signal = generate_sine(freq, sample_rate, n);
    std::vector<float> phase(n);

    EXPECT_TRUE(hilbert_extract_phase(ht, signal.data(), phase.data(), n));

    // Phase should be in range [-π, π]
    for (uint32_t i = 0; i < n; i++) {
        EXPECT_GE(phase[i], -M_PI - 0.01f);
        EXPECT_LE(phase[i], M_PI + 0.01f);
    }

    // Phase should advance with time for a pure sinusoid
    bool phase_advances = false;
    for (uint32_t i = 1; i < n - 1; i++) {
        float phase_diff = phase[i] - phase[i-1];
        // Wrap to [-π, π]
        while (phase_diff > M_PI) phase_diff -= 2.0f * M_PI;
        while (phase_diff < -M_PI) phase_diff += 2.0f * M_PI;
        if (phase_diff > 0.01f) {
            phase_advances = true;
        }
    }
    EXPECT_TRUE(phase_advances);
}

TEST_F(HilbertTest, ExtractAmplitudeAndPhase) {
    const uint32_t n = 1024;
    auto signal = generate_sine(10.0f, 1000.0f, n);
    std::vector<float> amplitude(n);
    std::vector<float> phase(n);

    EXPECT_TRUE(hilbert_extract_amplitude_phase(ht, signal.data(),
                                                  amplitude.data(), phase.data(), n));

    // Both should be valid
    for (uint32_t i = 0; i < n; i++) {
        EXPECT_GT(amplitude[i], 0.0f);
        EXPECT_GE(phase[i], -M_PI - 0.01f);
        EXPECT_LE(phase[i], M_PI + 0.01f);
    }
}

//=============================================================================
// Batch Processing Tests
//=============================================================================

TEST_F(HilbertTest, BatchTransform) {
    const uint32_t n = 1024;
    const uint32_t num_channels = 4;

    std::vector<std::vector<float>> signals(num_channels);
    std::vector<std::vector<neural_phasor_t>> analytics(num_channels);
    std::vector<const float*> signal_ptrs(num_channels);
    std::vector<neural_phasor_t*> analytic_ptrs(num_channels);

    for (uint32_t ch = 0; ch < num_channels; ch++) {
        signals[ch] = generate_sine(5.0f + ch, 1000.0f, n);
        analytics[ch].resize(n);
        signal_ptrs[ch] = signals[ch].data();
        analytic_ptrs[ch] = analytics[ch].data();
    }

    EXPECT_TRUE(hilbert_apply_batch(ht, signal_ptrs.data(), analytic_ptrs.data(),
                                      n, num_channels));

    // Check that all channels processed
    for (uint32_t ch = 0; ch < num_channels; ch++) {
        bool has_imag = false;
        for (uint32_t i = 0; i < n; i++) {
            if (std::abs(analytics[ch][i].imag) > 1e-6f) {
                has_imag = true;
                break;
            }
        }
        EXPECT_TRUE(has_imag);
    }
}

TEST_F(HilbertTest, BatchAmplitude) {
    const uint32_t n = 1024;
    const uint32_t num_channels = 3;

    std::vector<std::vector<float>> signals(num_channels);
    std::vector<std::vector<float>> amplitudes(num_channels);
    std::vector<const float*> signal_ptrs(num_channels);
    std::vector<float*> amplitude_ptrs(num_channels);

    for (uint32_t ch = 0; ch < num_channels; ch++) {
        signals[ch] = generate_sine(10.0f * (ch + 1), 1000.0f, n);
        amplitudes[ch].resize(n);
        signal_ptrs[ch] = signals[ch].data();
        amplitude_ptrs[ch] = amplitudes[ch].data();
    }

    EXPECT_TRUE(hilbert_extract_amplitude_batch(ht, signal_ptrs.data(),
                                                  amplitude_ptrs.data(), n, num_channels));

    // All channels should have valid amplitudes
    for (uint32_t ch = 0; ch < num_channels; ch++) {
        for (uint32_t i = 0; i < n; i++) {
            EXPECT_GT(amplitudes[ch][i], 0.0f);
        }
    }
}

//=============================================================================
// Result Management Tests
//=============================================================================

TEST_F(HilbertTest, ResultCreateDestroy) {
    const uint32_t n = 512;

    hilbert_result_t* result = hilbert_result_create(n);
    ASSERT_NE(result, nullptr);
    EXPECT_NE(result->analytic, nullptr);
    EXPECT_NE(result->amplitude, nullptr);
    EXPECT_NE(result->phase, nullptr);
    EXPECT_EQ(result->length, n);
    EXPECT_TRUE(result->owns_memory);

    hilbert_result_destroy(result);
}

TEST_F(HilbertTest, ComputeFull) {
    const uint32_t n = 1024;
    auto signal = generate_sine(10.0f, 1000.0f, n);

    hilbert_result_t* result = hilbert_compute_full(ht, signal.data(), n);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->length, n);

    // Verify all components are valid
    for (uint32_t i = 0; i < n; i++) {
        EXPECT_GT(result->amplitude[i], 0.0f);
        EXPECT_GE(result->phase[i], -M_PI - 0.01f);
        EXPECT_LE(result->phase[i], M_PI + 0.01f);
    }

    hilbert_result_destroy(result);
}

//=============================================================================
// Instantaneous Frequency Tests
//=============================================================================

TEST_F(HilbertTest, InstantaneousFrequency) {
    const uint32_t n = 1024;
    const float true_freq = 10.0f;
    const float sample_rate = 1000.0f;

    auto signal = generate_sine(true_freq, sample_rate, n);
    std::vector<float> phase(n);
    std::vector<float> frequency(n);

    ASSERT_TRUE(hilbert_extract_phase(ht, signal.data(), phase.data(), n));
    ASSERT_TRUE(hilbert_instantaneous_frequency(phase.data(), frequency.data(),
                                                  n, sample_rate));

    // Compute mean frequency (skip edges)
    float mean_freq = 0.0f;
    for (uint32_t i = 100; i < n - 100; i++) {
        mean_freq += frequency[i];
    }
    mean_freq /= (n - 200);

    // Should be close to true frequency
    EXPECT_NEAR(mean_freq, true_freq, 1.0f);
}

//=============================================================================
// Edge Cases
//=============================================================================

TEST_F(HilbertTest, ZeroSignal) {
    const uint32_t n = 512;
    std::vector<float> signal(n, 0.0f);
    std::vector<neural_phasor_t> analytic(n);

    EXPECT_TRUE(hilbert_apply(ht, signal.data(), analytic.data(), n));

    // Analytic signal of zeros should be zeros
    for (uint32_t i = 0; i < n; i++) {
        EXPECT_NEAR(analytic[i].real, 0.0f, 1e-6f);
        EXPECT_NEAR(analytic[i].imag, 0.0f, 1e-6f);
    }
}

TEST_F(HilbertTest, LargeSignal) {
    const uint32_t n = 4096;  // Max size in default config
    auto signal = generate_sine(20.0f, 1000.0f, n);
    std::vector<neural_phasor_t> analytic(n);

    EXPECT_TRUE(hilbert_apply(ht, signal.data(), analytic.data(), n));
}

TEST_F(HilbertTest, SignalTooLarge) {
    const uint32_t n = 8192;  // Exceeds default max_signal_length
    auto signal = generate_sine(20.0f, 1000.0f, n);
    std::vector<neural_phasor_t> analytic(n);

    // Should fail because signal exceeds pre-allocated buffer
    EXPECT_FALSE(hilbert_apply(ht, signal.data(), analytic.data(), n));
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
