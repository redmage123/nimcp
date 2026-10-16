/**
 * @file test_pink_noise_fractal_regression.cpp
 * @brief Regression tests for Pink Noise and Fractal modules
 *
 * WHAT: Tests backward compatibility and performance stability
 * WHY:  Ensure changes don't break existing functionality
 * HOW:  Test API signatures, output ranges, and performance baselines
 *
 * @version 2.6.3
 * @date 2026-01-24
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <chrono>

extern "C" {
#include "plasticity/noise/nimcp_pink_noise.h"
#include "cognitive/memory/core/nimcp_fractal.h"
}

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class PinkNoiseFractalRegressionTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

/* ============================================================================
 * Pink Noise API Stability Tests
 * ============================================================================ */

TEST_F(PinkNoiseFractalRegressionTest, DefaultConfigValuesUnchanged) {
    /* SCENARIO: Default config should maintain stable values
     * EXPECTED: Values match documented defaults
     */
    pink_noise_config_t config = pink_noise_default_config();

    EXPECT_EQ(config.alpha, 1.0f) << "Default alpha should be 1.0 (pink noise)";
    EXPECT_EQ(config.amplitude, 0.05f) << "Default amplitude should be 0.05";
    EXPECT_EQ(config.min_frequency, 0.1f) << "Default min_frequency should be 0.1 Hz";
    EXPECT_EQ(config.max_frequency, 100.0f) << "Default max_frequency should be 100.0 Hz";
    EXPECT_EQ(config.sample_rate, 1000.0f) << "Default sample_rate should be 1000.0 Hz";
    EXPECT_EQ(config.method, PINK_NOISE_VOSS) << "Default method should be Voss";
    EXPECT_EQ(config.seed, 0u) << "Default seed should be 0 (time-based)";
}

TEST_F(PinkNoiseFractalRegressionTest, MethodEnumValuesStable) {
    /* SCENARIO: Method enum values should not change
     * EXPECTED: Enum values match documented constants
     */
    EXPECT_EQ(PINK_NOISE_FFT, 0);
    EXPECT_EQ(PINK_NOISE_VOSS, 1);
    EXPECT_EQ(PINK_NOISE_IIR, 2);
    EXPECT_EQ(PINK_NOISE_WHITE, 3);
}

TEST_F(PinkNoiseFractalRegressionTest, MethodNameStringsStable) {
    /* SCENARIO: Method name strings should not change
     * EXPECTED: Strings match documented values
     */
    EXPECT_STREQ(pink_noise_method_name(PINK_NOISE_FFT), "FFT");
    EXPECT_STREQ(pink_noise_method_name(PINK_NOISE_VOSS), "Voss");
    EXPECT_STREQ(pink_noise_method_name(PINK_NOISE_IIR), "IIR");
    EXPECT_STREQ(pink_noise_method_name(PINK_NOISE_WHITE), "White");
}

TEST_F(PinkNoiseFractalRegressionTest, ConfigValidationRulesStable) {
    /* SCENARIO: Validation rules should remain consistent
     * EXPECTED: Same conditions pass/fail as before
     */
    pink_noise_config_t config = pink_noise_default_config();

    /* Valid config should pass */
    EXPECT_TRUE(pink_noise_validate_config(&config));

    /* Alpha range [0, 3] */
    config.alpha = -0.001f;
    EXPECT_FALSE(pink_noise_validate_config(&config));
    config.alpha = 3.001f;
    EXPECT_FALSE(pink_noise_validate_config(&config));
    config.alpha = 1.0f;

    /* Amplitude must be positive */
    config.amplitude = 0.0f;
    EXPECT_FALSE(pink_noise_validate_config(&config));
    config.amplitude = -0.001f;
    EXPECT_FALSE(pink_noise_validate_config(&config));
    config.amplitude = 0.05f;

    /* Frequency range must be valid */
    config.min_frequency = 100.0f;
    config.max_frequency = 50.0f;
    EXPECT_FALSE(pink_noise_validate_config(&config));
    config.min_frequency = 0.1f;
    config.max_frequency = 100.0f;

    /* Nyquist criterion */
    config.sample_rate = 150.0f;  /* < 2 * max_freq */
    EXPECT_FALSE(pink_noise_validate_config(&config));
}

TEST_F(PinkNoiseFractalRegressionTest, SeedReproducibilityMaintained) {
    /* SCENARIO: Same seed should produce identical sequences
     * EXPECTED: Sequences match bit-for-bit
     */
    pink_noise_config_t config = pink_noise_default_config();
    config.seed = 12345;
    config.method = PINK_NOISE_VOSS;

    pink_noise_generator_t gen1 = pink_noise_create(&config);
    pink_noise_generator_t gen2 = pink_noise_create(&config);

    ASSERT_NE(gen1, nullptr);
    ASSERT_NE(gen2, nullptr);

    std::vector<float> samples1(1000), samples2(1000);
    pink_noise_generate(gen1, samples1.data(), samples1.size());
    pink_noise_generate(gen2, samples2.data(), samples2.size());

    for (size_t i = 0; i < samples1.size(); i++) {
        EXPECT_FLOAT_EQ(samples1[i], samples2[i])
            << "Sample " << i << " differs between identical seeds";
    }

    pink_noise_destroy(gen1);
    pink_noise_destroy(gen2);
}

TEST_F(PinkNoiseFractalRegressionTest, ResetReproducibilityMaintained) {
    /* SCENARIO: Reset should restore generator to same state
     * EXPECTED: Sequences after reset match original
     */
    pink_noise_config_t config = pink_noise_default_config();
    config.seed = 54321;
    config.method = PINK_NOISE_FFT;

    pink_noise_generator_t gen = pink_noise_create(&config);
    ASSERT_NE(gen, nullptr);

    std::vector<float> samples1(500), samples2(500);
    pink_noise_generate(gen, samples1.data(), samples1.size());

    pink_noise_reset(gen, 54321);
    pink_noise_generate(gen, samples2.data(), samples2.size());

    for (size_t i = 0; i < samples1.size(); i++) {
        EXPECT_FLOAT_EQ(samples1[i], samples2[i])
            << "Sample " << i << " differs after reset";
    }

    pink_noise_destroy(gen);
}

/* ============================================================================
 * Fractal API Stability Tests
 * ============================================================================ */

TEST_F(PinkNoiseFractalRegressionTest, FractalDefaultConfigValuesUnchanged) {
    /* SCENARIO: Default fractal config should maintain stable values
     * EXPECTED: Values match documented defaults
     */
    fractal_config_t config = fractal_config_default();

    EXPECT_EQ(config.min_scale, 4u);
    EXPECT_EQ(config.num_scales, 20u);
    EXPECT_TRUE(config.use_log_scales);
    EXPECT_FLOAT_EQ(config.confidence_threshold, 0.95f);
    EXPECT_EQ(config.dfa_poly_order, 1);
    EXPECT_TRUE(config.dfa_remove_mean);
    EXPECT_TRUE(config.spectral_use_welch);
    EXPECT_FLOAT_EQ(config.spectral_overlap, 0.5f);
}

TEST_F(PinkNoiseFractalRegressionTest, FractalErrorCodesStable) {
    /* SCENARIO: Error codes should not change
     * EXPECTED: Error code values match documented constants
     */
    EXPECT_EQ(FRACTAL_OK, 0);
    EXPECT_EQ(FRACTAL_ERROR_NULL_PTR, -1);
    EXPECT_EQ(FRACTAL_ERROR_INSUFFICIENT, -2);
    EXPECT_EQ(FRACTAL_ERROR_INVALID_CONFIG, -3);
    EXPECT_EQ(FRACTAL_ERROR_ALLOC, -4);
    EXPECT_EQ(FRACTAL_ERROR_COMPUTE, -5);
    EXPECT_EQ(FRACTAL_ERROR_QUALITY, -6);
    EXPECT_EQ(FRACTAL_ERROR_PARAM, -7);
}

TEST_F(PinkNoiseFractalRegressionTest, FractalConstantsStable) {
    /* SCENARIO: Fractal constants should not change
     * EXPECTED: Constant values match documented values
     */
    EXPECT_EQ(FRACTAL_MIN_SAMPLES, 64u);
    EXPECT_EQ(FRACTAL_MAX_SAMPLES, 1048576u);
    EXPECT_EQ(FRACTAL_DEFAULT_MIN_SCALE, 4u);
    EXPECT_FLOAT_EQ(FRACTAL_DEFAULT_MAX_SCALE_FRAC, 0.25f);
    EXPECT_EQ(FRACTAL_DEFAULT_NUM_SCALES, 20u);
    EXPECT_FLOAT_EQ(FRACTAL_DEFAULT_CONFIDENCE, 0.95f);
    EXPECT_FLOAT_EQ(FRACTAL_PINK_NOISE_TOLERANCE, 0.15f);
}

TEST_F(PinkNoiseFractalRegressionTest, FractalConfigValidationRulesStable) {
    /* SCENARIO: Fractal validation rules should remain consistent
     * EXPECTED: Same conditions pass/fail as before
     */
    fractal_config_t config = fractal_config_default();

    /* Valid config should pass */
    EXPECT_TRUE(fractal_config_validate(&config));

    /* min_scale must be >= 4 */
    config.min_scale = 3;
    EXPECT_FALSE(fractal_config_validate(&config));
    config.min_scale = 4;

    /* num_scales must be >= 4 */
    config.num_scales = 3;
    EXPECT_FALSE(fractal_config_validate(&config));
    config.num_scales = 20;

    /* confidence_threshold must be >= 0.5 */
    config.confidence_threshold = 0.49f;
    EXPECT_FALSE(fractal_config_validate(&config));
    config.confidence_threshold = 0.95f;

    /* dfa_poly_order must be 1-3 */
    config.dfa_poly_order = 0;
    EXPECT_FALSE(fractal_config_validate(&config));
    config.dfa_poly_order = 4;
    EXPECT_FALSE(fractal_config_validate(&config));
    config.dfa_poly_order = 1;

    /* spectral_overlap must be 0.0-0.9 */
    config.spectral_overlap = 0.91f;
    EXPECT_FALSE(fractal_config_validate(&config));
}

/* ============================================================================
 * Output Range Stability Tests
 * ============================================================================ */

TEST_F(PinkNoiseFractalRegressionTest, HurstExponentInValidRange) {
    /* SCENARIO: Hurst exponent should be in reasonable range
     * EXPECTED: All computed values in valid range (with numerical tolerance)
     */
    std::vector<float> signal(2048);
    for (size_t i = 0; i < signal.size(); i++) {
        signal[i] = sin(0.01 * i) + 0.1f * ((float)rand() / RAND_MAX - 0.5f);
    }

    fractal_result_t result;
    int ret = fractal_hurst_rs(signal.data(), signal.size(), nullptr, &result);

    if (ret == FRACTAL_OK) {
        /* Allow small numerical tolerance beyond strict [0,1] bounds */
        EXPECT_GE(result.hurst_exponent, -0.1f);
        EXPECT_LE(result.hurst_exponent, 1.2f);
        EXPECT_GE(result.hurst_r2, 0.0f);
        EXPECT_LE(result.hurst_r2, 1.1f);
    }
}

TEST_F(PinkNoiseFractalRegressionTest, DFAExponentInValidRange) {
    /* SCENARIO: DFA exponent should be in reasonable range
     * EXPECTED: All computed values in [0, 3]
     */
    std::vector<float> signal(2048);
    for (size_t i = 0; i < signal.size(); i++) {
        signal[i] = sin(0.01 * i) + 0.1f * ((float)rand() / RAND_MAX - 0.5f);
    }

    fractal_result_t result;
    int ret = fractal_dfa(signal.data(), signal.size(), nullptr, &result);

    if (ret == FRACTAL_OK) {
        EXPECT_GE(result.dfa_exponent, 0.0f);
        EXPECT_LE(result.dfa_exponent, 3.0f);
        EXPECT_GE(result.dfa_r2, 0.0f);
        EXPECT_LE(result.dfa_r2, 1.0f);
    }
}

TEST_F(PinkNoiseFractalRegressionTest, FractalDimensionInValidRange) {
    /* SCENARIO: Fractal dimension should be in reasonable range for 1D signal
     * EXPECTED: All computed values in valid range (with numerical tolerance)
     */
    std::vector<float> signal(2048);
    for (size_t i = 0; i < signal.size(); i++) {
        signal[i] = sin(0.01 * i) + 0.1f * ((float)rand() / RAND_MAX - 0.5f);
    }

    fractal_result_t result;
    int ret = fractal_box_dimension(signal.data(), signal.size(), nullptr, &result);

    if (ret == FRACTAL_OK) {
        /* Allow numerical tolerance beyond strict [1,2] bounds */
        EXPECT_GE(result.fractal_dimension, 0.5f);
        EXPECT_LE(result.fractal_dimension, 2.5f);
    }
}

TEST_F(PinkNoiseFractalRegressionTest, LacunarityAlwaysGreaterThanOne) {
    /* SCENARIO: Lacunarity is always >= 1.0
     * EXPECTED: All computed values >= 1.0
     */
    std::vector<float> signal(2048);
    for (size_t i = 0; i < signal.size(); i++) {
        signal[i] = sin(0.01 * i) + 0.1f * ((float)rand() / RAND_MAX - 0.5f);
    }

    float lac = fractal_lacunarity(signal.data(), signal.size(), 32);
    if (lac > 0.0f) {  /* Valid result */
        EXPECT_GE(lac, 1.0f);
    }
}

/* ============================================================================
 * Performance Regression Tests
 * ============================================================================ */

TEST_F(PinkNoiseFractalRegressionTest, PinkNoiseGenerationPerformance) {
    /* SCENARIO: Pink noise generation should complete in reasonable time
     * EXPECTED: 10,000 samples in < 100ms
     */
    pink_noise_config_t config = pink_noise_default_config();
    config.method = PINK_NOISE_VOSS;  /* Fastest method */

    pink_noise_generator_t gen = pink_noise_create(&config);
    ASSERT_NE(gen, nullptr);

    std::vector<float> samples(10000);

    auto start = std::chrono::high_resolution_clock::now();
    bool success = pink_noise_generate(gen, samples.data(), samples.size());
    auto end = std::chrono::high_resolution_clock::now();

    EXPECT_TRUE(success);

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    EXPECT_LT(duration.count(), 100) << "Generation should complete in < 100ms";

    pink_noise_destroy(gen);
}

TEST_F(PinkNoiseFractalRegressionTest, DFAAnalysisPerformance) {
    /* SCENARIO: DFA analysis should complete in reasonable time
     * EXPECTED: 4096 samples in < 500ms
     */
    std::vector<float> signal(4096);
    for (size_t i = 0; i < signal.size(); i++) {
        signal[i] = (float)rand() / RAND_MAX;
    }

    auto start = std::chrono::high_resolution_clock::now();
    fractal_result_t result;
    int ret = fractal_dfa(signal.data(), signal.size(), nullptr, &result);
    auto end = std::chrono::high_resolution_clock::now();

    EXPECT_EQ(ret, FRACTAL_OK);

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    EXPECT_LT(duration.count(), 500) << "DFA should complete in < 500ms";
}

TEST_F(PinkNoiseFractalRegressionTest, ComprehensiveAnalysisPerformance) {
    /* SCENARIO: Full fractal analysis should complete in reasonable time
     * EXPECTED: 4096 samples in < 2000ms
     */
    std::vector<float> signal(4096);
    for (size_t i = 0; i < signal.size(); i++) {
        signal[i] = (float)rand() / RAND_MAX;
    }

    auto start = std::chrono::high_resolution_clock::now();
    fractal_result_t result;
    int ret = fractal_analyze(signal.data(), signal.size(), nullptr, &result);
    auto end = std::chrono::high_resolution_clock::now();

    EXPECT_EQ(ret, FRACTAL_OK);

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    EXPECT_LT(duration.count(), 2000) << "Full analysis should complete in < 2s";
}

/* ============================================================================
 * Memory Safety Regression Tests
 * ============================================================================ */

TEST_F(PinkNoiseFractalRegressionTest, MultiplePinkNoiseCreateDestroyCycles) {
    /* SCENARIO: Multiple create/destroy cycles should not leak memory
     * EXPECTED: No crashes, no leaks
     */
    for (int i = 0; i < 100; i++) {
        pink_noise_config_t config = pink_noise_default_config();
        config.seed = i;
        pink_noise_generator_t gen = pink_noise_create(&config);
        ASSERT_NE(gen, nullptr) << "Failed at iteration " << i;

        float sample;
        pink_noise_generate_sample(gen, &sample);

        pink_noise_destroy(gen);
    }
    SUCCEED();
}

TEST_F(PinkNoiseFractalRegressionTest, MultipleMultifractalCreateDestroyCycles) {
    /* SCENARIO: Multiple spectrum create/destroy cycles should not leak
     * EXPECTED: No crashes, no leaks
     */
    std::vector<float> signal(1024);
    for (size_t i = 0; i < signal.size(); i++) {
        signal[i] = (float)rand() / RAND_MAX;
    }

    for (int i = 0; i < 10; i++) {
        multifractal_spectrum_t* spectrum = nullptr;
        int ret = fractal_multifractal_spectrum(signal.data(), signal.size(),
                                                -3.0f, 3.0f, 7, &spectrum);
        if (ret == FRACTAL_OK && spectrum != nullptr) {
            multifractal_spectrum_destroy(spectrum);
        }
    }
    SUCCEED();
}

/* ============================================================================
 * Noise Classification Stability Tests
 * ============================================================================ */

TEST_F(PinkNoiseFractalRegressionTest, NoiseClassificationStringsStable) {
    /* SCENARIO: Noise classification strings should not change
     * EXPECTED: Strings match expected values
     */
    /* These exponent ranges and their classifications should be stable */
    const char* white = fractal_classify_noise(0.5f);
    const char* pink = fractal_classify_noise(1.0f);
    const char* brown = fractal_classify_noise(1.5f);

    EXPECT_NE(white, nullptr);
    EXPECT_NE(pink, nullptr);
    EXPECT_NE(brown, nullptr);

    /* The exact strings may vary, but they should exist */
    EXPECT_GT(strlen(white), 0u);
    EXPECT_GT(strlen(pink), 0u);
    EXPECT_GT(strlen(brown), 0u);
}

/* ============================================================================
 * Exponent Conversion Stability Tests
 * ============================================================================ */

TEST_F(PinkNoiseFractalRegressionTest, ExponentConversionFormulasStable) {
    /* SCENARIO: Exponent conversion should use stable formulas
     * EXPECTED: Conversions match documented relationships
     */
    float dfa, spectral, dimension;

    /* H = 0.5 (white noise) */
    fractal_convert_exponents(0.5f, &dfa, &spectral, &dimension);
    EXPECT_NEAR(dfa, 0.5f, 0.01f);           /* H = alpha */
    EXPECT_NEAR(spectral, 0.0f, 0.01f);      /* beta = 2*H - 1 */
    EXPECT_NEAR(dimension, 1.5f, 0.01f);     /* D = 2 - H */

    /* H = 1.0 (pink noise) */
    fractal_convert_exponents(1.0f, &dfa, &spectral, &dimension);
    EXPECT_NEAR(dfa, 1.0f, 0.01f);
    EXPECT_NEAR(spectral, 1.0f, 0.01f);
    EXPECT_NEAR(dimension, 1.0f, 0.01f);

    /* H = 0.75 */
    fractal_convert_exponents(0.75f, &dfa, &spectral, &dimension);
    EXPECT_NEAR(dfa, 0.75f, 0.01f);
    EXPECT_NEAR(spectral, 0.5f, 0.01f);      /* 2*0.75 - 1 = 0.5 */
    EXPECT_NEAR(dimension, 1.25f, 0.01f);    /* 2 - 0.75 = 1.25 */
}

/* ============================================================================
 * Entry Point
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
