//=============================================================================
// test_signal_filter.cpp - Unit Tests for Signal Filtering
//=============================================================================
/**
 * @file test_signal_filter.cpp
 * @brief Comprehensive unit tests for band-pass filtering (100% coverage)
 *
 * TEST COVERAGE:
 * - Configuration functions (default, bandpass, lowpass, highpass, validation)
 * - Filter lifecycle (create, destroy, reset)
 * - Filtering functions (apply, apply_envelope, get_response, get_coefficients)
 * - All filter types (lowpass, highpass, bandpass, bandstop)
 * - All window functions (rectangular, hamming, hann, blackman)
 * - Edge cases and error handling
 */

#include <gtest/gtest.h>
// Headers have their own extern "C" guards
    #include "utils/signal/nimcp_signal_filter.h"
#include <cmath>
#include <vector>

#define TOLERANCE 1e-3f

//=============================================================================
// Test Fixtures
//=============================================================================

class SignalFilterTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}

    // Helper: Generate sine wave
    std::vector<float> generate_sine(float freq, float sample_rate, uint32_t n) {
        std::vector<float> signal(n);
        for (uint32_t i = 0; i < n; i++) {
            signal[i] = sinf(2.0f * M_PI * freq * i / sample_rate);
        }
        return signal;
    }

    // Helper: Measure signal power
    float signal_power(const float* signal, uint32_t n) {
        float sum = 0.0f;
        for (uint32_t i = 0; i < n; i++) {
            sum += signal[i] * signal[i];
        }
        return sum / n;
    }
};

//=============================================================================
// Configuration Tests
//=============================================================================

TEST_F(SignalFilterTest, DefaultConfig) {
    signal_filter_config_t config = signal_filter_default_config();

    EXPECT_EQ(config.type, FILTER_BANDPASS);
    EXPECT_EQ(config.low_freq, 4.0f);
    EXPECT_EQ(config.high_freq, 8.0f);
    EXPECT_EQ(config.sample_rate, 1000.0f);
    EXPECT_EQ(config.order, 64u);
    EXPECT_EQ(config.window, WINDOW_HAMMING);
    EXPECT_TRUE(config.use_fft_convolution);
}

TEST_F(SignalFilterTest, BandpassConfig) {
    signal_filter_config_t config = signal_filter_bandpass_config(10.0f, 50.0f, 500.0f);

    EXPECT_EQ(config.type, FILTER_BANDPASS);
    EXPECT_EQ(config.low_freq, 10.0f);
    EXPECT_EQ(config.high_freq, 50.0f);
    EXPECT_EQ(config.sample_rate, 500.0f);
}

TEST_F(SignalFilterTest, LowpassConfig) {
    signal_filter_config_t config = signal_filter_lowpass_config(100.0f, 1000.0f);

    EXPECT_EQ(config.type, FILTER_LOWPASS);
    EXPECT_EQ(config.cutoff_freq, 100.0f);
    EXPECT_EQ(config.sample_rate, 1000.0f);
}

TEST_F(SignalFilterTest, HighpassConfig) {
    signal_filter_config_t config = signal_filter_highpass_config(50.0f, 500.0f);

    EXPECT_EQ(config.type, FILTER_HIGHPASS);
    EXPECT_EQ(config.cutoff_freq, 50.0f);
    EXPECT_EQ(config.sample_rate, 500.0f);
}

TEST_F(SignalFilterTest, ValidateConfigValid) {
    signal_filter_config_t config = signal_filter_default_config();
    EXPECT_TRUE(signal_filter_validate_config(&config));
}

TEST_F(SignalFilterTest, ValidateConfigNull) {
    EXPECT_FALSE(signal_filter_validate_config(nullptr));
}

TEST_F(SignalFilterTest, ValidateConfigInvalidSampleRate) {
    signal_filter_config_t config = signal_filter_default_config();
    config.sample_rate = 0.0f;
    EXPECT_FALSE(signal_filter_validate_config(&config));

    config.sample_rate = -100.0f;
    EXPECT_FALSE(signal_filter_validate_config(&config));
}

TEST_F(SignalFilterTest, ValidateConfigInvalidOrder) {
    signal_filter_config_t config = signal_filter_default_config();
    config.order = 0;
    EXPECT_FALSE(signal_filter_validate_config(&config));

    config.order = 1025;  // Too large
    EXPECT_FALSE(signal_filter_validate_config(&config));

    config.order = 63;  // Odd
    EXPECT_FALSE(signal_filter_validate_config(&config));
}

TEST_F(SignalFilterTest, ValidateConfigInvalidFrequencies) {
    signal_filter_config_t config = signal_filter_lowpass_config(50.0f, 100.0f);

    // Cutoff above Nyquist
    config.cutoff_freq = 60.0f;  // Nyquist = 50 Hz
    EXPECT_FALSE(signal_filter_validate_config(&config));

    // Cutoff <= 0
    config.cutoff_freq = 0.0f;
    EXPECT_FALSE(signal_filter_validate_config(&config));
}

TEST_F(SignalFilterTest, ValidateConfigInvalidBandpass) {
    signal_filter_config_t config = signal_filter_bandpass_config(10.0f, 50.0f, 100.0f);

    // High freq above Nyquist
    config.high_freq = 60.0f;  // Nyquist = 50 Hz
    EXPECT_FALSE(signal_filter_validate_config(&config));

    // Low >= High
    config.high_freq = 10.0f;
    config.low_freq = 15.0f;
    EXPECT_FALSE(signal_filter_validate_config(&config));
}

//=============================================================================
// Filter Lifecycle Tests
//=============================================================================

TEST_F(SignalFilterTest, CreateDestroyLowpass) {
    signal_filter_config_t config = signal_filter_lowpass_config(50.0f, 1000.0f);
    signal_filter_t* filter = signal_filter_create(&config);

    ASSERT_NE(filter, nullptr);
    signal_filter_destroy(filter);
}

TEST_F(SignalFilterTest, CreateDestroyHighpass) {
    signal_filter_config_t config = signal_filter_highpass_config(100.0f, 1000.0f);
    signal_filter_t* filter = signal_filter_create(&config);

    ASSERT_NE(filter, nullptr);
    signal_filter_destroy(filter);
}

TEST_F(SignalFilterTest, CreateDestroyBandpass) {
    signal_filter_config_t config = signal_filter_bandpass_config(30.0f, 80.0f, 1000.0f);
    signal_filter_t* filter = signal_filter_create(&config);

    ASSERT_NE(filter, nullptr);
    signal_filter_destroy(filter);
}

TEST_F(SignalFilterTest, CreateDestroyBandstop) {
    signal_filter_config_t config = signal_filter_default_config();
    config.type = FILTER_BANDSTOP;
    config.low_freq = 45.0f;
    config.high_freq = 55.0f;
    signal_filter_t* filter = signal_filter_create(&config);

    ASSERT_NE(filter, nullptr);
    signal_filter_destroy(filter);
}

TEST_F(SignalFilterTest, CreateNullConfig) {
    signal_filter_t* filter = signal_filter_create(nullptr);
    EXPECT_EQ(filter, nullptr);
}

TEST_F(SignalFilterTest, CreateInvalidConfig) {
    signal_filter_config_t config = signal_filter_default_config();
    config.sample_rate = -1.0f;

    signal_filter_t* filter = signal_filter_create(&config);
    EXPECT_EQ(filter, nullptr);
}

TEST_F(SignalFilterTest, DestroyNull) {
    signal_filter_destroy(nullptr);  // Should not crash
}

TEST_F(SignalFilterTest, ResetFilter) {
    signal_filter_config_t config = signal_filter_lowpass_config(100.0f, 1000.0f);
    signal_filter_t* filter = signal_filter_create(&config);
    ASSERT_NE(filter, nullptr);

    // Apply some filtering to populate state
    std::vector<float> input = generate_sine(50.0f, 1000.0f, 100);
    std::vector<float> output(100);
    signal_filter_apply(filter, input.data(), output.data(), 100);

    // Reset should clear state
    EXPECT_TRUE(signal_filter_reset(filter));

    signal_filter_destroy(filter);
}

TEST_F(SignalFilterTest, ResetNull) {
    EXPECT_FALSE(signal_filter_reset(nullptr));
}

//=============================================================================
// Lowpass Filter Tests
//=============================================================================

TEST_F(SignalFilterTest, LowpassPassband) {
    signal_filter_config_t config = signal_filter_lowpass_config(100.0f, 1000.0f);
    config.order = 128;  // Higher order for sharper cutoff
    signal_filter_t* filter = signal_filter_create(&config);
    ASSERT_NE(filter, nullptr);

    // Test passband frequency (should pass)
    uint32_t n = 1024;
    std::vector<float> input = generate_sine(50.0f, 1000.0f, n);
    std::vector<float> output(n);

    ASSERT_TRUE(signal_filter_apply(filter, input.data(), output.data(), n));

    // Measure power (skip transient at start)
    float power_in = signal_power(input.data() + 200, n - 200);
    float power_out = signal_power(output.data() + 200, n - 200);

    // Passband should have minimal attenuation
    EXPECT_GT(power_out / power_in, 0.7f);  // At least 70% power retained

    signal_filter_destroy(filter);
}

TEST_F(SignalFilterTest, LowpassStopband) {
    signal_filter_config_t config = signal_filter_lowpass_config(100.0f, 1000.0f);
    config.order = 128;
    signal_filter_t* filter = signal_filter_create(&config);
    ASSERT_NE(filter, nullptr);

    // Test stopband frequency (should reject)
    uint32_t n = 1024;
    std::vector<float> input = generate_sine(200.0f, 1000.0f, n);
    std::vector<float> output(n);

    ASSERT_TRUE(signal_filter_apply(filter, input.data(), output.data(), n));

    // Measure power (skip transient)
    float power_in = signal_power(input.data() + 200, n - 200);
    float power_out = signal_power(output.data() + 200, n - 200);

    // Stopband should have significant attenuation
    EXPECT_LT(power_out / power_in, 0.3f);  // Less than 30% power retained

    signal_filter_destroy(filter);
}

//=============================================================================
// Highpass Filter Tests
//=============================================================================

TEST_F(SignalFilterTest, HighpassPassband) {
    signal_filter_config_t config = signal_filter_highpass_config(100.0f, 1000.0f);
    config.order = 128;
    signal_filter_t* filter = signal_filter_create(&config);
    ASSERT_NE(filter, nullptr);

    // Test passband (high frequency should pass)
    uint32_t n = 1024;
    std::vector<float> input = generate_sine(200.0f, 1000.0f, n);
    std::vector<float> output(n);

    ASSERT_TRUE(signal_filter_apply(filter, input.data(), output.data(), n));

    float power_in = signal_power(input.data() + 200, n - 200);
    float power_out = signal_power(output.data() + 200, n - 200);

    EXPECT_GT(power_out / power_in, 0.7f);

    signal_filter_destroy(filter);
}

TEST_F(SignalFilterTest, HighpassStopband) {
    signal_filter_config_t config = signal_filter_highpass_config(100.0f, 1000.0f);
    config.order = 128;
    signal_filter_t* filter = signal_filter_create(&config);
    ASSERT_NE(filter, nullptr);

    // Test stopband (low frequency should reject)
    uint32_t n = 1024;
    std::vector<float> input = generate_sine(50.0f, 1000.0f, n);
    std::vector<float> output(n);

    ASSERT_TRUE(signal_filter_apply(filter, input.data(), output.data(), n));

    float power_in = signal_power(input.data() + 200, n - 200);
    float power_out = signal_power(output.data() + 200, n - 200);

    EXPECT_LT(power_out / power_in, 0.3f);

    signal_filter_destroy(filter);
}

//=============================================================================
// Bandpass Filter Tests
//=============================================================================

TEST_F(SignalFilterTest, BandpassPassband) {
    signal_filter_config_t config = signal_filter_bandpass_config(30.0f, 80.0f, 1000.0f);
    config.order = 128;
    signal_filter_t* filter = signal_filter_create(&config);
    ASSERT_NE(filter, nullptr);

    // Test passband (middle frequency should pass)
    uint32_t n = 1024;
    std::vector<float> input = generate_sine(50.0f, 1000.0f, n);  // Center of band
    std::vector<float> output(n);

    ASSERT_TRUE(signal_filter_apply(filter, input.data(), output.data(), n));

    float power_in = signal_power(input.data() + 200, n - 200);
    float power_out = signal_power(output.data() + 200, n - 200);

    EXPECT_GT(power_out / power_in, 0.6f);  // Band-pass has more loss than single-band

    signal_filter_destroy(filter);
}

TEST_F(SignalFilterTest, BandpassStopbandLow) {
    signal_filter_config_t config = signal_filter_bandpass_config(30.0f, 80.0f, 1000.0f);
    config.order = 128;
    signal_filter_t* filter = signal_filter_create(&config);
    ASSERT_NE(filter, nullptr);

    // Test low stopband
    uint32_t n = 1024;
    std::vector<float> input = generate_sine(10.0f, 1000.0f, n);
    std::vector<float> output(n);

    ASSERT_TRUE(signal_filter_apply(filter, input.data(), output.data(), n));

    float power_in = signal_power(input.data() + 200, n - 200);
    float power_out = signal_power(output.data() + 200, n - 200);

    EXPECT_LT(power_out / power_in, 0.3f);

    signal_filter_destroy(filter);
}

TEST_F(SignalFilterTest, BandpassStopbandHigh) {
    signal_filter_config_t config = signal_filter_bandpass_config(30.0f, 80.0f, 1000.0f);
    config.order = 128;
    signal_filter_t* filter = signal_filter_create(&config);
    ASSERT_NE(filter, nullptr);

    // Test high stopband
    uint32_t n = 1024;
    std::vector<float> input = generate_sine(150.0f, 1000.0f, n);
    std::vector<float> output(n);

    ASSERT_TRUE(signal_filter_apply(filter, input.data(), output.data(), n));

    float power_in = signal_power(input.data() + 200, n - 200);
    float power_out = signal_power(output.data() + 200, n - 200);

    EXPECT_LT(power_out / power_in, 0.3f);

    signal_filter_destroy(filter);
}

//=============================================================================
// Window Function Tests
//=============================================================================

TEST_F(SignalFilterTest, WindowFunctionRectangular) {
    signal_filter_config_t config = signal_filter_default_config();
    config.window = WINDOW_RECTANGULAR;
    signal_filter_t* filter = signal_filter_create(&config);
    ASSERT_NE(filter, nullptr);
    signal_filter_destroy(filter);
}

TEST_F(SignalFilterTest, WindowFunctionHamming) {
    signal_filter_config_t config = signal_filter_default_config();
    config.window = WINDOW_HAMMING;
    signal_filter_t* filter = signal_filter_create(&config);
    ASSERT_NE(filter, nullptr);
    signal_filter_destroy(filter);
}

TEST_F(SignalFilterTest, WindowFunctionHann) {
    signal_filter_config_t config = signal_filter_default_config();
    config.window = WINDOW_HANN;
    signal_filter_t* filter = signal_filter_create(&config);
    ASSERT_NE(filter, nullptr);
    signal_filter_destroy(filter);
}

TEST_F(SignalFilterTest, WindowFunctionBlackman) {
    signal_filter_config_t config = signal_filter_default_config();
    config.window = WINDOW_BLACKMAN;
    signal_filter_t* filter = signal_filter_create(&config);
    ASSERT_NE(filter, nullptr);
    signal_filter_destroy(filter);
}

//=============================================================================
// Utility Function Tests
//=============================================================================

TEST_F(SignalFilterTest, GetCoefficients) {
    signal_filter_config_t config = signal_filter_lowpass_config(100.0f, 1000.0f);
    config.order = 64;
    signal_filter_t* filter = signal_filter_create(&config);
    ASSERT_NE(filter, nullptr);

    float coeffs[128];
    uint32_t num_coeffs = 0;

    ASSERT_TRUE(signal_filter_get_coefficients(filter, coeffs, 128, &num_coeffs));
    EXPECT_EQ(num_coeffs, 65u);  // order + 1

    signal_filter_destroy(filter);
}

TEST_F(SignalFilterTest, GetCoefficientsNull) {
    signal_filter_config_t config = signal_filter_default_config();
    signal_filter_t* filter = signal_filter_create(&config);
    ASSERT_NE(filter, nullptr);

    float coeffs[128];
    uint32_t num_coeffs = 0;

    EXPECT_FALSE(signal_filter_get_coefficients(nullptr, coeffs, 128, &num_coeffs));
    EXPECT_FALSE(signal_filter_get_coefficients(filter, nullptr, 128, &num_coeffs));
    EXPECT_FALSE(signal_filter_get_coefficients(filter, coeffs, 128, nullptr));

    signal_filter_destroy(filter);
}

TEST_F(SignalFilterTest, GetFrequencyResponse) {
    signal_filter_config_t config = signal_filter_lowpass_config(100.0f, 1000.0f);
    signal_filter_t* filter = signal_filter_create(&config);
    ASSERT_NE(filter, nullptr);

    float freqs[] = {10.0f, 50.0f, 100.0f, 200.0f, 300.0f};
    float response[5];

    ASSERT_TRUE(signal_filter_get_response(filter, freqs, response, 5));

    // Low frequencies should have higher response than high frequencies
    EXPECT_GT(response[0], response[3]);  // 10 Hz > 200 Hz
    EXPECT_GT(response[1], response[4]);  // 50 Hz > 300 Hz

    signal_filter_destroy(filter);
}

TEST_F(SignalFilterTest, GetFrequencyResponseNull) {
    signal_filter_config_t config = signal_filter_default_config();
    signal_filter_t* filter = signal_filter_create(&config);
    ASSERT_NE(filter, nullptr);

    float freqs[] = {10.0f};
    float response[1];

    EXPECT_FALSE(signal_filter_get_response(nullptr, freqs, response, 1));
    EXPECT_FALSE(signal_filter_get_response(filter, nullptr, response, 1));
    EXPECT_FALSE(signal_filter_get_response(filter, freqs, nullptr, 1));
    EXPECT_FALSE(signal_filter_get_response(filter, freqs, response, 0));

    signal_filter_destroy(filter);
}

TEST_F(SignalFilterTest, GetDelay) {
    signal_filter_config_t config = signal_filter_default_config();
    config.order = 64;
    signal_filter_t* filter = signal_filter_create(&config);
    ASSERT_NE(filter, nullptr);

    uint32_t delay = signal_filter_get_delay(filter);
    EXPECT_EQ(delay, 32u);  // order / 2 for linear phase FIR

    signal_filter_destroy(filter);
}

TEST_F(SignalFilterTest, GetDelayNull) {
    uint32_t delay = signal_filter_get_delay(nullptr);
    EXPECT_EQ(delay, 0u);
}

//=============================================================================
// Apply Function Tests
//=============================================================================

TEST_F(SignalFilterTest, ApplyNull) {
    signal_filter_config_t config = signal_filter_default_config();
    signal_filter_t* filter = signal_filter_create(&config);
    ASSERT_NE(filter, nullptr);

    std::vector<float> input(100, 0.0f);
    std::vector<float> output(100);

    EXPECT_FALSE(signal_filter_apply(nullptr, input.data(), output.data(), 100));
    EXPECT_FALSE(signal_filter_apply(filter, nullptr, output.data(), 100));
    EXPECT_FALSE(signal_filter_apply(filter, input.data(), nullptr, 100));
    EXPECT_FALSE(signal_filter_apply(filter, input.data(), output.data(), 0));

    signal_filter_destroy(filter);
}

TEST_F(SignalFilterTest, ApplyInPlace) {
    signal_filter_config_t config = signal_filter_lowpass_config(100.0f, 1000.0f);
    signal_filter_t* filter = signal_filter_create(&config);
    ASSERT_NE(filter, nullptr);

    std::vector<float> signal = generate_sine(50.0f, 1000.0f, 512);

    // In-place filtering
    ASSERT_TRUE(signal_filter_apply(filter, signal.data(), signal.data(), 512));

    signal_filter_destroy(filter);
}

TEST_F(SignalFilterTest, ApplyEnvelope) {
    signal_filter_config_t config = signal_filter_bandpass_config(30.0f, 80.0f, 1000.0f);
    signal_filter_t* filter = signal_filter_create(&config);
    ASSERT_NE(filter, nullptr);

    uint32_t n = 512;
    std::vector<float> input = generate_sine(50.0f, 1000.0f, n);
    std::vector<float> envelope(n);

    ASSERT_TRUE(signal_filter_apply_envelope(filter, input.data(), envelope.data(), n));

    // Envelope should be non-negative
    for (uint32_t i = 0; i < n; i++) {
        EXPECT_GE(envelope[i], 0.0f);
    }

    signal_filter_destroy(filter);
}

TEST_F(SignalFilterTest, ApplyEnvelopeNull) {
    signal_filter_config_t config = signal_filter_default_config();
    signal_filter_t* filter = signal_filter_create(&config);
    ASSERT_NE(filter, nullptr);

    std::vector<float> input(100, 0.0f);
    std::vector<float> output(100);

    EXPECT_FALSE(signal_filter_apply_envelope(nullptr, input.data(), output.data(), 100));
    EXPECT_FALSE(signal_filter_apply_envelope(filter, nullptr, output.data(), 100));
    EXPECT_FALSE(signal_filter_apply_envelope(filter, input.data(), nullptr, 100));

    signal_filter_destroy(filter);
}

//=============================================================================
// Edge Case Tests
//=============================================================================

TEST_F(SignalFilterTest, SmallSignal) {
    signal_filter_config_t config = signal_filter_default_config();
    signal_filter_t* filter = signal_filter_create(&config);
    ASSERT_NE(filter, nullptr);

    // Signal smaller than filter order
    std::vector<float> input(32, 1.0f);
    std::vector<float> output(32);

    ASSERT_TRUE(signal_filter_apply(filter, input.data(), output.data(), 32));

    signal_filter_destroy(filter);
}

TEST_F(SignalFilterTest, DCSignal) {
    signal_filter_config_t config = signal_filter_highpass_config(10.0f, 1000.0f);
    config.order = 128;
    signal_filter_t* filter = signal_filter_create(&config);
    ASSERT_NE(filter, nullptr);

    // DC signal (0 Hz) should be blocked by highpass
    uint32_t n = 1024;
    std::vector<float> input(n, 1.0f);  // DC = 1.0
    std::vector<float> output(n);

    ASSERT_TRUE(signal_filter_apply(filter, input.data(), output.data(), n));

    // Output should trend toward zero (DC blocked)
    float avg = 0.0f;
    for (uint32_t i = 200; i < n; i++) {  // Skip transient
        avg += fabsf(output[i]);
    }
    avg /= (n - 200);

    EXPECT_LT(avg, 0.1f);  // Should be near zero

    signal_filter_destroy(filter);
}

TEST_F(SignalFilterTest, StreamingConsistency) {
    signal_filter_config_t config = signal_filter_lowpass_config(50.0f, 1000.0f);
    signal_filter_t* filter = signal_filter_create(&config);
    ASSERT_NE(filter, nullptr);

    // Process signal in chunks
    uint32_t chunk_size = 64;
    uint32_t num_chunks = 10;

    for (uint32_t i = 0; i < num_chunks; i++) {
        std::vector<float> input = generate_sine(30.0f, 1000.0f, chunk_size);
        std::vector<float> output(chunk_size);

        ASSERT_TRUE(signal_filter_apply(filter, input.data(), output.data(), chunk_size));
    }

    signal_filter_destroy(filter);
}
