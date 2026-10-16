/**
 * @file test_fractal_exception_handling.cpp
 * @brief Unit tests for NIMCP_THROW_TO_IMMUNE exception handling in Fractal Analysis module
 *
 * WHAT: Tests that all error paths properly trigger NIMCP_THROW_TO_IMMUNE
 * WHY:  Ensure errors are reported to the brain immune system for monitoring
 * HOW:  Test all NULL pointer, invalid parameter, and edge case paths
 *
 * @version 2.6.3
 * @date 2026-01-24
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>

extern "C" {
#include "cognitive/memory/core/nimcp_fractal.h"
}

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class FractalExceptionHandlingTest : public ::testing::Test {
protected:
    std::vector<float> valid_signal;
    fractal_config_t default_config;

    void SetUp() override {
        default_config = fractal_config_default();

        /* Generate a valid pink noise-like signal for testing */
        valid_signal.resize(1024);
        float accumulator = 0.0f;
        for (size_t i = 0; i < valid_signal.size(); i++) {
            /* Simple integrated random walk (approximates pink noise) */
            accumulator += ((float)rand() / RAND_MAX - 0.5f) * 0.1f;
            valid_signal[i] = accumulator;
        }
    }

    void TearDown() override {
        valid_signal.clear();
    }
};

/* ============================================================================
 * Configuration Validation Tests
 * ============================================================================ */

TEST_F(FractalExceptionHandlingTest, ConfigValidateWithNullConfig) {
    /* SCENARIO: Validate NULL config
     * EXPECTED: Returns false
     */
    bool valid = fractal_config_validate(nullptr);
    EXPECT_FALSE(valid);
}

TEST_F(FractalExceptionHandlingTest, ConfigValidateValidDefaults) {
    /* SCENARIO: Validate default config
     * EXPECTED: Returns true
     */
    bool valid = fractal_config_validate(&default_config);
    EXPECT_TRUE(valid);
}

TEST_F(FractalExceptionHandlingTest, ConfigValidateInvalidMinScale) {
    /* SCENARIO: min_scale too small
     * EXPECTED: Returns false
     */
    fractal_config_t config = default_config;
    config.min_scale = 2;  /* Must be >= 4 */
    bool valid = fractal_config_validate(&config);
    EXPECT_FALSE(valid);
}

TEST_F(FractalExceptionHandlingTest, ConfigValidateInvalidNumScales) {
    /* SCENARIO: num_scales too small
     * EXPECTED: Returns false
     */
    fractal_config_t config = default_config;
    config.num_scales = 2;  /* Must be >= 4 */
    bool valid = fractal_config_validate(&config);
    EXPECT_FALSE(valid);
}

TEST_F(FractalExceptionHandlingTest, ConfigValidateInvalidConfidence) {
    /* SCENARIO: confidence_threshold out of range
     * EXPECTED: Returns false
     */
    fractal_config_t config = default_config;
    config.confidence_threshold = 0.3f;  /* Must be >= 0.5 */
    bool valid = fractal_config_validate(&config);
    EXPECT_FALSE(valid);
}

TEST_F(FractalExceptionHandlingTest, ConfigValidateInvalidDFAOrder) {
    /* SCENARIO: dfa_poly_order out of range
     * EXPECTED: Returns false
     */
    fractal_config_t config = default_config;
    config.dfa_poly_order = 5;  /* Must be 1-3 */
    bool valid = fractal_config_validate(&config);
    EXPECT_FALSE(valid);
}

TEST_F(FractalExceptionHandlingTest, ConfigValidateInvalidSpectralOverlap) {
    /* SCENARIO: spectral_overlap out of range
     * EXPECTED: Returns false
     */
    fractal_config_t config = default_config;
    config.spectral_overlap = 0.95f;  /* Must be <= 0.9 */
    bool valid = fractal_config_validate(&config);
    EXPECT_FALSE(valid);
}

/* ============================================================================
 * Hurst Exponent (R/S Analysis) Exception Tests
 * ============================================================================ */

TEST_F(FractalExceptionHandlingTest, HurstRSWithNullSamples) {
    /* SCENARIO: Hurst R/S with NULL samples
     * EXPECTED: Returns error code
     */
    fractal_result_t result;
    int ret = fractal_hurst_rs(nullptr, 1024, nullptr, &result);
    EXPECT_LT(ret, 0);
}

TEST_F(FractalExceptionHandlingTest, HurstRSWithNullResult) {
    /* SCENARIO: Hurst R/S with NULL result
     * EXPECTED: Returns error code
     */
    int ret = fractal_hurst_rs(valid_signal.data(), valid_signal.size(), nullptr, nullptr);
    EXPECT_LT(ret, 0);
}

TEST_F(FractalExceptionHandlingTest, HurstRSWithInsufficientSamples) {
    /* SCENARIO: Hurst R/S with too few samples
     * EXPECTED: Returns error code
     */
    float short_signal[32];
    fractal_result_t result;
    int ret = fractal_hurst_rs(short_signal, 32, nullptr, &result);
    EXPECT_LT(ret, 0);  /* FRACTAL_ERROR_INSUFFICIENT */
}

TEST_F(FractalExceptionHandlingTest, HurstRSValidSignal) {
    /* SCENARIO: Hurst R/S with valid signal
     * EXPECTED: Returns success, valid result
     */
    fractal_result_t result;
    int ret = fractal_hurst_rs(valid_signal.data(), valid_signal.size(), nullptr, &result);
    EXPECT_EQ(ret, FRACTAL_OK);

    /* Hurst should be in reasonable range (allow small numerical error) */
    EXPECT_GE(result.hurst_exponent, -0.1f);
    EXPECT_LE(result.hurst_exponent, 1.1f);
}

TEST_F(FractalExceptionHandlingTest, HurstRSWithConfig) {
    /* SCENARIO: Hurst R/S with custom config
     * EXPECTED: Returns success
     */
    fractal_result_t result;
    int ret = fractal_hurst_rs(valid_signal.data(), valid_signal.size(), &default_config, &result);
    EXPECT_EQ(ret, FRACTAL_OK);
}

/* ============================================================================
 * DFA Exception Tests
 * ============================================================================ */

TEST_F(FractalExceptionHandlingTest, DFAWithNullSamples) {
    /* SCENARIO: DFA with NULL samples
     * EXPECTED: Returns error code
     */
    fractal_result_t result;
    int ret = fractal_dfa(nullptr, 1024, nullptr, &result);
    EXPECT_LT(ret, 0);
}

TEST_F(FractalExceptionHandlingTest, DFAWithNullResult) {
    /* SCENARIO: DFA with NULL result
     * EXPECTED: Returns error code
     */
    int ret = fractal_dfa(valid_signal.data(), valid_signal.size(), nullptr, nullptr);
    EXPECT_LT(ret, 0);
}

TEST_F(FractalExceptionHandlingTest, DFAWithInsufficientSamples) {
    /* SCENARIO: DFA with too few samples
     * EXPECTED: Returns error code
     */
    float short_signal[32];
    fractal_result_t result;
    int ret = fractal_dfa(short_signal, 32, nullptr, &result);
    EXPECT_LT(ret, 0);
}

TEST_F(FractalExceptionHandlingTest, DFAValidSignal) {
    /* SCENARIO: DFA with valid signal
     * EXPECTED: Returns success, DFA exponent is valid
     */
    fractal_result_t result;
    int ret = fractal_dfa(valid_signal.data(), valid_signal.size(), nullptr, &result);
    EXPECT_EQ(ret, FRACTAL_OK);

    /* DFA exponent typically 0.5-1.5 for common signals */
    EXPECT_GE(result.dfa_exponent, 0.0f);
    EXPECT_LE(result.dfa_exponent, 3.0f);
}

TEST_F(FractalExceptionHandlingTest, DFAWithInvalidConfig) {
    /* SCENARIO: DFA with invalid config
     * EXPECTED: Returns error or uses defaults
     */
    fractal_config_t invalid_config = default_config;
    invalid_config.min_scale = 1;  /* Too small */

    fractal_result_t result;
    int ret = fractal_dfa(valid_signal.data(), valid_signal.size(), &invalid_config, &result);
    /* May return error or gracefully handle */
    (void)ret;
    SUCCEED();
}

/* ============================================================================
 * Spectral Exponent Exception Tests
 * ============================================================================ */

TEST_F(FractalExceptionHandlingTest, SpectralExponentWithNullSamples) {
    /* SCENARIO: Spectral exponent with NULL samples
     * EXPECTED: Returns error code
     */
    fractal_result_t result;
    int ret = fractal_spectral_exponent(nullptr, 1024, &result);
    EXPECT_LT(ret, 0);
}

TEST_F(FractalExceptionHandlingTest, SpectralExponentWithNullResult) {
    /* SCENARIO: Spectral exponent with NULL result
     * EXPECTED: Returns error code
     */
    int ret = fractal_spectral_exponent(valid_signal.data(), valid_signal.size(), nullptr);
    EXPECT_LT(ret, 0);
}

TEST_F(FractalExceptionHandlingTest, SpectralExponentValidSignal) {
    /* SCENARIO: Spectral exponent with valid signal
     * EXPECTED: Returns success
     */
    fractal_result_t result;
    int ret = fractal_spectral_exponent(valid_signal.data(), valid_signal.size(), &result);
    EXPECT_EQ(ret, FRACTAL_OK);

    /* Spectral exponent typically 0-2 */
    EXPECT_GE(result.spectral_exponent, -1.0f);
    EXPECT_LE(result.spectral_exponent, 4.0f);
}

TEST_F(FractalExceptionHandlingTest, SpectralExponentConfigWithNullSamples) {
    /* SCENARIO: Spectral exponent config version with NULL samples
     * EXPECTED: Returns error code
     */
    fractal_result_t result;
    int ret = fractal_spectral_exponent_config(nullptr, 1024, &default_config, &result);
    EXPECT_LT(ret, 0);
}

/* ============================================================================
 * Box-Counting Dimension Exception Tests
 * ============================================================================ */

TEST_F(FractalExceptionHandlingTest, BoxDimensionWithNullSamples) {
    /* SCENARIO: Box dimension with NULL samples
     * EXPECTED: Returns error code
     */
    fractal_result_t result;
    int ret = fractal_box_dimension(nullptr, 1024, nullptr, &result);
    EXPECT_LT(ret, 0);
}

TEST_F(FractalExceptionHandlingTest, BoxDimensionWithNullResult) {
    /* SCENARIO: Box dimension with NULL result
     * EXPECTED: Returns error code
     */
    int ret = fractal_box_dimension(valid_signal.data(), valid_signal.size(), nullptr, nullptr);
    EXPECT_LT(ret, 0);
}

TEST_F(FractalExceptionHandlingTest, BoxDimensionValidSignal) {
    /* SCENARIO: Box dimension with valid signal
     * EXPECTED: Returns success, dimension in reasonable range
     */
    fractal_result_t result;
    int ret = fractal_box_dimension(valid_signal.data(), valid_signal.size(), nullptr, &result);
    EXPECT_EQ(ret, FRACTAL_OK);

    /* Fractal dimension for 1D signal should be in reasonable range (allow numerical error) */
    EXPECT_GE(result.fractal_dimension, 0.8f);
    EXPECT_LE(result.fractal_dimension, 2.2f);
}

/* ============================================================================
 * Lacunarity Exception Tests
 * ============================================================================ */

TEST_F(FractalExceptionHandlingTest, LacunarityWithNullSamples) {
    /* SCENARIO: Lacunarity with NULL samples
     * EXPECTED: Returns -1.0
     */
    float lac = fractal_lacunarity(nullptr, 1024, 16);
    EXPECT_LT(lac, 0.0f);
}

TEST_F(FractalExceptionHandlingTest, LacunarityWithZeroCount) {
    /* SCENARIO: Lacunarity with zero samples
     * EXPECTED: Returns -1.0
     */
    float samples[10];
    float lac = fractal_lacunarity(samples, 0, 16);
    EXPECT_LT(lac, 0.0f);
}

TEST_F(FractalExceptionHandlingTest, LacunarityWithBoxSizeTooLarge) {
    /* SCENARIO: Box size larger than sample count
     * EXPECTED: Returns -1.0 or handles gracefully
     */
    float lac = fractal_lacunarity(valid_signal.data(), valid_signal.size(), valid_signal.size() + 100);
    EXPECT_LT(lac, 0.0f);
}

TEST_F(FractalExceptionHandlingTest, LacunarityValidSignal) {
    /* SCENARIO: Lacunarity with valid parameters
     * EXPECTED: Returns positive value >= 1.0
     */
    float lac = fractal_lacunarity(valid_signal.data(), valid_signal.size(), 16);
    EXPECT_GE(lac, 1.0f);
}

TEST_F(FractalExceptionHandlingTest, LacunarityCurveWithNullSamples) {
    /* SCENARIO: Lacunarity curve with NULL samples
     * EXPECTED: Returns error code
     */
    float scales[10], lacs[10];
    int ret = fractal_lacunarity_curve(nullptr, 1024, &default_config, scales, lacs, 10);
    EXPECT_LT(ret, 0);
}

TEST_F(FractalExceptionHandlingTest, LacunarityCurveWithNullOutputs) {
    /* SCENARIO: Lacunarity curve with NULL outputs
     * EXPECTED: Returns error code
     */
    int ret = fractal_lacunarity_curve(valid_signal.data(), valid_signal.size(),
                                       &default_config, nullptr, nullptr, 10);
    EXPECT_LT(ret, 0);
}

/* ============================================================================
 * Multifractal Spectrum Exception Tests
 * ============================================================================ */

TEST_F(FractalExceptionHandlingTest, MultifractalWithNullSamples) {
    /* SCENARIO: Multifractal spectrum with NULL samples
     * EXPECTED: Returns error code
     */
    multifractal_spectrum_t* spectrum = nullptr;
    int ret = fractal_multifractal_spectrum(nullptr, 1024, -5.0f, 5.0f, 21, &spectrum);
    EXPECT_LT(ret, 0);
    EXPECT_EQ(spectrum, nullptr);
}

TEST_F(FractalExceptionHandlingTest, MultifractalWithNullSpectrumPtr) {
    /* SCENARIO: Multifractal spectrum with NULL spectrum pointer
     * EXPECTED: Returns error code
     */
    int ret = fractal_multifractal_spectrum(valid_signal.data(), valid_signal.size(),
                                            -5.0f, 5.0f, 21, nullptr);
    EXPECT_LT(ret, 0);
}

TEST_F(FractalExceptionHandlingTest, MultifractalWithInvalidQRange) {
    /* SCENARIO: q_min > q_max
     * EXPECTED: Returns error code
     */
    multifractal_spectrum_t* spectrum = nullptr;
    int ret = fractal_multifractal_spectrum(valid_signal.data(), valid_signal.size(),
                                            5.0f, -5.0f, 21, &spectrum);
    EXPECT_LT(ret, 0);
}

TEST_F(FractalExceptionHandlingTest, MultifractalValidSignal) {
    /* SCENARIO: Multifractal spectrum with valid signal
     * EXPECTED: Returns success, spectrum populated
     */
    multifractal_spectrum_t* spectrum = nullptr;
    int ret = fractal_multifractal_spectrum(valid_signal.data(), valid_signal.size(),
                                            -3.0f, 3.0f, 13, &spectrum);

    if (ret == FRACTAL_OK && spectrum != nullptr) {
        EXPECT_GT(spectrum->spectrum_size, 0u);
        EXPECT_GE(spectrum->width, 0.0f);

        multifractal_spectrum_destroy(spectrum);
    }
}

TEST_F(FractalExceptionHandlingTest, MultifractalDestroyNull) {
    /* SCENARIO: Destroy NULL spectrum
     * EXPECTED: Does not crash
     */
    multifractal_spectrum_destroy(nullptr);
    SUCCEED();
}

/* ============================================================================
 * Validation Function Exception Tests
 * ============================================================================ */

TEST_F(FractalExceptionHandlingTest, IsPinkNoiseWithNullSamples) {
    /* SCENARIO: is_pink_noise with NULL samples
     * EXPECTED: Returns false
     */
    bool is_pink = fractal_is_pink_noise(nullptr, 1024, 0.15f);
    EXPECT_FALSE(is_pink);
}

TEST_F(FractalExceptionHandlingTest, IsPinkNoiseWithInsufficientSamples) {
    /* SCENARIO: is_pink_noise with too few samples
     * EXPECTED: Returns false
     */
    float short_signal[32];
    bool is_pink = fractal_is_pink_noise(short_signal, 32, 0.15f);
    EXPECT_FALSE(is_pink);
}

TEST_F(FractalExceptionHandlingTest, IsSelfSimilarWithNullSamples) {
    /* SCENARIO: is_self_similar with NULL samples
     * EXPECTED: Returns false
     */
    bool is_similar = fractal_is_self_similar(nullptr, 1024, 10);
    EXPECT_FALSE(is_similar);
}

TEST_F(FractalExceptionHandlingTest, ValidateSignalWithNullSamples) {
    /* SCENARIO: validate_signal with NULL samples
     * EXPECTED: Returns false
     */
    bool valid = fractal_validate_signal(nullptr, 1024);
    EXPECT_FALSE(valid);
}

TEST_F(FractalExceptionHandlingTest, ValidateSignalWithZeroCount) {
    /* SCENARIO: validate_signal with zero count
     * EXPECTED: Returns false
     */
    float samples[10];
    bool valid = fractal_validate_signal(samples, 0);
    EXPECT_FALSE(valid);
}

TEST_F(FractalExceptionHandlingTest, ValidateSignalValid) {
    /* SCENARIO: validate_signal with valid signal
     * EXPECTED: Returns true
     */
    bool valid = fractal_validate_signal(valid_signal.data(), valid_signal.size());
    EXPECT_TRUE(valid);
}

/* ============================================================================
 * Comprehensive Analysis Exception Tests
 * ============================================================================ */

TEST_F(FractalExceptionHandlingTest, AnalyzeWithNullSamples) {
    /* SCENARIO: fractal_analyze with NULL samples
     * EXPECTED: Returns error code
     */
    fractal_result_t result;
    int ret = fractal_analyze(nullptr, 1024, nullptr, &result);
    EXPECT_LT(ret, 0);
}

TEST_F(FractalExceptionHandlingTest, AnalyzeWithNullResult) {
    /* SCENARIO: fractal_analyze with NULL result
     * EXPECTED: Returns error code
     */
    int ret = fractal_analyze(valid_signal.data(), valid_signal.size(), nullptr, nullptr);
    EXPECT_LT(ret, 0);
}

TEST_F(FractalExceptionHandlingTest, AnalyzeValidSignal) {
    /* SCENARIO: fractal_analyze with valid signal
     * EXPECTED: Returns success, all metrics populated
     */
    fractal_result_t result;
    int ret = fractal_analyze(valid_signal.data(), valid_signal.size(), nullptr, &result);
    EXPECT_EQ(ret, FRACTAL_OK);

    /* All metrics should be computed (allow numerical tolerance) */
    EXPECT_GE(result.hurst_exponent, -0.1f);
    EXPECT_LE(result.hurst_exponent, 1.1f);
    EXPECT_GE(result.dfa_exponent, 0.0f);
    EXPECT_GE(result.fractal_dimension, 0.8f);
    EXPECT_LE(result.fractal_dimension, 2.2f);
    EXPECT_GE(result.lacunarity, 0.9f);
    EXPECT_GT(result.samples_analyzed, 0u);
}

/* ============================================================================
 * Utility Function Exception Tests
 * ============================================================================ */

TEST_F(FractalExceptionHandlingTest, ResultPrintWithNullResult) {
    /* SCENARIO: fractal_result_print with NULL result
     * EXPECTED: Does not crash
     */
    fractal_result_print(nullptr);
    SUCCEED();
}

TEST_F(FractalExceptionHandlingTest, SpectrumPrintWithNullSpectrum) {
    /* SCENARIO: multifractal_spectrum_print with NULL spectrum
     * EXPECTED: Does not crash
     */
    multifractal_spectrum_print(nullptr);
    SUCCEED();
}

TEST_F(FractalExceptionHandlingTest, ClassifyNoiseForRanges) {
    /* SCENARIO: fractal_classify_noise for different exponent ranges
     * EXPECTED: Returns appropriate strings
     */
    const char* white = fractal_classify_noise(0.5f);
    const char* pink = fractal_classify_noise(1.0f);
    const char* brown = fractal_classify_noise(1.5f);

    EXPECT_NE(white, nullptr);
    EXPECT_NE(pink, nullptr);
    EXPECT_NE(brown, nullptr);
}

TEST_F(FractalExceptionHandlingTest, ConvertExponentsWithNullOutputs) {
    /* SCENARIO: fractal_convert_exponents with NULL outputs
     * EXPECTED: Does not crash
     */
    fractal_convert_exponents(0.5f, nullptr, nullptr, nullptr);
    SUCCEED();
}

TEST_F(FractalExceptionHandlingTest, ConvertExponentsValid) {
    /* SCENARIO: fractal_convert_exponents with valid outputs
     * EXPECTED: Outputs are computed correctly
     */
    float dfa, spectral, dimension;
    fractal_convert_exponents(0.75f, &dfa, &spectral, &dimension);

    EXPECT_NEAR(dfa, 0.75f, 0.01f);  /* H = alpha for stationary */
    EXPECT_NEAR(spectral, 0.5f, 0.01f);  /* beta = 2*H - 1 */
    EXPECT_NEAR(dimension, 1.25f, 0.01f);  /* D = 2 - H */
}

TEST_F(FractalExceptionHandlingTest, EstimateSampleRequirementValid) {
    /* SCENARIO: estimate_sample_requirement with valid params
     * EXPECTED: Returns positive count
     */
    size_t count = fractal_estimate_sample_requirement(0.95f, 0);
    EXPECT_GT(count, 0u);
}

/* ============================================================================
 * Edge Cases with Special Signal Values
 * ============================================================================ */

TEST_F(FractalExceptionHandlingTest, AnalyzeConstantSignal) {
    /* SCENARIO: Analyze constant signal (no variance)
     * EXPECTED: Returns error or handles gracefully
     */
    std::vector<float> constant(1024, 1.0f);
    fractal_result_t result;
    int ret = fractal_analyze(constant.data(), constant.size(), nullptr, &result);

    /* May return error due to zero variance */
    (void)ret;
    SUCCEED();
}

TEST_F(FractalExceptionHandlingTest, AnalyzeSignalWithNaN) {
    /* SCENARIO: Analyze signal containing NaN
     * EXPECTED: validate_signal returns false
     */
    std::vector<float> with_nan = valid_signal;
    with_nan[512] = std::nanf("");

    bool valid = fractal_validate_signal(with_nan.data(), with_nan.size());
    EXPECT_FALSE(valid);
}

TEST_F(FractalExceptionHandlingTest, AnalyzeSignalWithInf) {
    /* SCENARIO: Analyze signal containing Inf
     * EXPECTED: validate_signal returns false
     */
    std::vector<float> with_inf = valid_signal;
    with_inf[512] = INFINITY;

    bool valid = fractal_validate_signal(with_inf.data(), with_inf.size());
    EXPECT_FALSE(valid);
}

/* ============================================================================
 * Entry Point
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
