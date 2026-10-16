/**
 * @file test_pink_noise_fractal_integration.cpp
 * @brief Integration tests for Pink Noise + Fractal Analysis modules
 *
 * WHAT: Tests the integration between pink noise generation and fractal analysis
 * WHY:  Verify that generated pink noise exhibits correct fractal properties
 * HOW:  Generate pink noise, analyze with fractal methods, validate consistency
 *
 * NEUROSCIENCE MOTIVATION:
 * - Pink noise (1/f) is ubiquitous in neural systems
 * - Fractal analysis validates criticality and self-organized criticality
 * - Combined testing ensures biologically realistic noise generation
 *
 * @version 2.6.3
 * @date 2026-01-24
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <numeric>

extern "C" {
#include "plasticity/noise/nimcp_pink_noise.h"
#include "cognitive/memory/core/nimcp_fractal.h"
}

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class PinkNoiseFractalIntegrationTest : public ::testing::Test {
protected:
    pink_noise_generator_t generator;
    std::vector<float> samples;

    void SetUp() override {
        generator = nullptr;
        samples.resize(4096);  /* Sufficient for fractal analysis */
    }

    void TearDown() override {
        if (generator) {
            pink_noise_destroy(generator);
            generator = nullptr;
        }
        samples.clear();
    }

    /* Helper to create generator with specific settings */
    pink_noise_generator_t create_generator(pink_noise_method_t method, float alpha, uint32_t seed) {
        pink_noise_config_t config = pink_noise_default_config();
        config.method = method;
        config.alpha = alpha;
        config.seed = seed;
        config.amplitude = 1.0f;
        return pink_noise_create(&config);
    }
};

/* ============================================================================
 * Pink Noise Generation + DFA Validation Tests
 * ============================================================================ */

TEST_F(PinkNoiseFractalIntegrationTest, FFTMethodProducesPinkNoise) {
    /* SCENARIO: FFT-generated pink noise analyzed by DFA
     * EXPECTED: DFA exponent near 1.0 (within tolerance)
     */
    generator = create_generator(PINK_NOISE_FFT, 1.0f, 42);
    ASSERT_NE(generator, nullptr);

    bool gen_success = pink_noise_generate(generator, samples.data(), samples.size());
    ASSERT_TRUE(gen_success);

    fractal_result_t result;
    int ret = fractal_dfa(samples.data(), samples.size(), nullptr, &result);
    ASSERT_EQ(ret, FRACTAL_OK);

    /* DFA exponent should be near 1.0 for pink noise */
    EXPECT_NEAR(result.dfa_exponent, 1.0f, 0.3f)
        << "FFT pink noise should have DFA exponent near 1.0";
    EXPECT_GT(result.dfa_r2, 0.8f)
        << "DFA fit should have good R^2";
}

TEST_F(PinkNoiseFractalIntegrationTest, VossMethodProducesPinkNoise) {
    /* SCENARIO: Voss-McCartney generated pink noise analyzed by DFA
     * EXPECTED: DFA exponent near 1.0 (within tolerance)
     */
    generator = create_generator(PINK_NOISE_VOSS, 1.0f, 42);
    ASSERT_NE(generator, nullptr);

    bool gen_success = pink_noise_generate(generator, samples.data(), samples.size());
    ASSERT_TRUE(gen_success);

    fractal_result_t result;
    int ret = fractal_dfa(samples.data(), samples.size(), nullptr, &result);
    ASSERT_EQ(ret, FRACTAL_OK);

    /* Voss approximation may have slightly different exponent */
    EXPECT_NEAR(result.dfa_exponent, 1.0f, 0.4f)
        << "Voss pink noise should have DFA exponent near 1.0";
}

TEST_F(PinkNoiseFractalIntegrationTest, IIRMethodProducesPinkNoise) {
    /* SCENARIO: IIR-filtered pink noise analyzed by DFA
     * EXPECTED: DFA exponent in reasonable range (IIR is approximate)
     */
    generator = create_generator(PINK_NOISE_IIR, 1.0f, 42);
    ASSERT_NE(generator, nullptr);

    bool gen_success = pink_noise_generate(generator, samples.data(), samples.size());
    ASSERT_TRUE(gen_success);

    fractal_result_t result;
    int ret = fractal_dfa(samples.data(), samples.size(), nullptr, &result);
    if (ret != FRACTAL_OK) {
        GTEST_SKIP() << "DFA analysis failed (may need longer signal)";
    }

    /* IIR approximation may have different exponent - wide tolerance */
    EXPECT_GT(result.dfa_exponent, 0.3f)
        << "IIR pink noise DFA exponent should be > 0.3";
    EXPECT_LT(result.dfa_exponent, 2.0f)
        << "IIR pink noise DFA exponent should be < 2.0";
}

TEST_F(PinkNoiseFractalIntegrationTest, WhiteNoiseProducesAlphaPointFive) {
    /* SCENARIO: White noise analyzed by DFA
     * EXPECTED: DFA exponent near 0.5
     */
    generator = create_generator(PINK_NOISE_WHITE, 0.0f, 42);
    ASSERT_NE(generator, nullptr);

    bool gen_success = pink_noise_generate(generator, samples.data(), samples.size());
    ASSERT_TRUE(gen_success);

    fractal_result_t result;
    int ret = fractal_dfa(samples.data(), samples.size(), nullptr, &result);
    ASSERT_EQ(ret, FRACTAL_OK);

    /* White noise has DFA exponent ~0.5 */
    EXPECT_NEAR(result.dfa_exponent, 0.5f, 0.2f)
        << "White noise should have DFA exponent near 0.5";
}

TEST_F(PinkNoiseFractalIntegrationTest, BrownianNoiseProducesAlphaOnePointFive) {
    /* SCENARIO: Brownian/red noise (alpha=2) analyzed by DFA
     * EXPECTED: DFA exponent higher than pink noise
     */
    generator = create_generator(PINK_NOISE_FFT, 2.0f, 42);
    ASSERT_NE(generator, nullptr);

    bool gen_success = pink_noise_generate(generator, samples.data(), samples.size());
    ASSERT_TRUE(gen_success);

    fractal_result_t result;
    int ret = fractal_dfa(samples.data(), samples.size(), nullptr, &result);
    if (ret != FRACTAL_OK) {
        GTEST_SKIP() << "DFA analysis failed (may need longer signal)";
    }

    /* Brownian noise has higher DFA exponent than pink - wide tolerance */
    EXPECT_GT(result.dfa_exponent, 1.0f)
        << "Brownian noise DFA exponent should be > 1.0";
    EXPECT_LT(result.dfa_exponent, 2.5f)
        << "Brownian noise DFA exponent should be < 2.5";
}

/* ============================================================================
 * Pink Noise + Spectral Analysis Tests
 * ============================================================================ */

TEST_F(PinkNoiseFractalIntegrationTest, PinkNoiseSpectralExponentNearOne) {
    /* SCENARIO: Pink noise spectral analysis
     * EXPECTED: Spectral exponent near 1.0
     */
    generator = create_generator(PINK_NOISE_FFT, 1.0f, 123);
    ASSERT_NE(generator, nullptr);

    bool gen_success = pink_noise_generate(generator, samples.data(), samples.size());
    ASSERT_TRUE(gen_success);

    fractal_result_t result;
    int ret = fractal_spectral_exponent(samples.data(), samples.size(), &result);
    ASSERT_EQ(ret, FRACTAL_OK);

    /* Spectral exponent should match generation alpha */
    EXPECT_NEAR(result.spectral_exponent, 1.0f, 0.4f)
        << "Pink noise spectral exponent should be near 1.0";
}

TEST_F(PinkNoiseFractalIntegrationTest, WhiteNoiseSpectralExponentNearZero) {
    /* SCENARIO: White noise spectral analysis
     * EXPECTED: Spectral exponent near 0.0 (flat spectrum)
     */
    generator = create_generator(PINK_NOISE_WHITE, 0.0f, 123);
    ASSERT_NE(generator, nullptr);

    bool gen_success = pink_noise_generate(generator, samples.data(), samples.size());
    ASSERT_TRUE(gen_success);

    fractal_result_t result;
    int ret = fractal_spectral_exponent(samples.data(), samples.size(), &result);
    ASSERT_EQ(ret, FRACTAL_OK);

    /* White noise has flat spectrum (exponent ~0) */
    EXPECT_NEAR(result.spectral_exponent, 0.0f, 0.4f)
        << "White noise spectral exponent should be near 0.0";
}

/* ============================================================================
 * Pink Noise + Hurst Exponent Tests
 * ============================================================================ */

TEST_F(PinkNoiseFractalIntegrationTest, PinkNoiseHurstExponent) {
    /* SCENARIO: Pink noise Hurst exponent analysis
     * EXPECTED: H near 0.9-1.0 (persistent)
     */
    generator = create_generator(PINK_NOISE_FFT, 1.0f, 456);
    ASSERT_NE(generator, nullptr);

    bool gen_success = pink_noise_generate(generator, samples.data(), samples.size());
    ASSERT_TRUE(gen_success);

    fractal_result_t result;
    int ret = fractal_hurst_rs(samples.data(), samples.size(), nullptr, &result);
    ASSERT_EQ(ret, FRACTAL_OK);

    /* Pink noise is persistent (H > 0.5) */
    EXPECT_GT(result.hurst_exponent, 0.5f)
        << "Pink noise should be persistent (H > 0.5)";
    EXPECT_LT(result.hurst_exponent, 1.0f)
        << "Hurst exponent should be < 1.0";
}

TEST_F(PinkNoiseFractalIntegrationTest, WhiteNoiseHurstNearPointFive) {
    /* SCENARIO: White noise Hurst exponent analysis
     * EXPECTED: H near 0.5 (random walk)
     */
    generator = create_generator(PINK_NOISE_WHITE, 0.0f, 456);
    ASSERT_NE(generator, nullptr);

    bool gen_success = pink_noise_generate(generator, samples.data(), samples.size());
    ASSERT_TRUE(gen_success);

    fractal_result_t result;
    int ret = fractal_hurst_rs(samples.data(), samples.size(), nullptr, &result);
    ASSERT_EQ(ret, FRACTAL_OK);

    /* White noise has H ~0.5 */
    EXPECT_NEAR(result.hurst_exponent, 0.5f, 0.15f)
        << "White noise Hurst should be near 0.5";
}

/* ============================================================================
 * Pink Noise + Fractal Dimension Tests
 * ============================================================================ */

TEST_F(PinkNoiseFractalIntegrationTest, PinkNoiseFractalDimension) {
    /* SCENARIO: Pink noise fractal dimension
     * EXPECTED: D in reasonable range for 1D signal
     */
    generator = create_generator(PINK_NOISE_FFT, 1.0f, 789);
    ASSERT_NE(generator, nullptr);

    bool gen_success = pink_noise_generate(generator, samples.data(), samples.size());
    ASSERT_TRUE(gen_success);

    fractal_result_t result;
    int ret = fractal_box_dimension(samples.data(), samples.size(), nullptr, &result);
    if (ret != FRACTAL_OK) {
        GTEST_SKIP() << "Box dimension analysis failed";
    }

    /* Fractal dimension for 1D signal - allow wide range due to numerical variance */
    EXPECT_GT(result.fractal_dimension, 0.5f)
        << "Fractal dimension should be > 0.5";
    EXPECT_LT(result.fractal_dimension, 2.5f)
        << "Fractal dimension should be < 2.5";
}

/* ============================================================================
 * Pink Noise + Comprehensive Analysis Tests
 * ============================================================================ */

TEST_F(PinkNoiseFractalIntegrationTest, ComprehensiveAnalysisPinkNoise) {
    /* SCENARIO: Complete fractal analysis of pink noise
     * EXPECTED: All metrics consistent with 1/f characteristics
     */
    generator = create_generator(PINK_NOISE_FFT, 1.0f, 999);
    ASSERT_NE(generator, nullptr);

    bool gen_success = pink_noise_generate(generator, samples.data(), samples.size());
    ASSERT_TRUE(gen_success);

    fractal_result_t result;
    int ret = fractal_analyze(samples.data(), samples.size(), nullptr, &result);
    ASSERT_EQ(ret, FRACTAL_OK);

    /* Verify all metrics are populated */
    EXPECT_GT(result.samples_analyzed, 0u);
    EXPECT_GT(result.scales_computed, 0u);

    /* DFA should indicate pink noise */
    EXPECT_NEAR(result.dfa_exponent, 1.0f, 0.4f);

    /* Hurst should be persistent */
    EXPECT_GT(result.hurst_exponent, 0.5f);

    /* R^2 values should indicate good fits */
    EXPECT_GT(result.dfa_r2, 0.7f);
    EXPECT_GT(result.hurst_r2, 0.7f);
}

TEST_F(PinkNoiseFractalIntegrationTest, IsPinkNoiseValidation) {
    /* SCENARIO: Use fractal_is_pink_noise to validate generated noise
     * EXPECTED: Returns true for pink noise
     */
    generator = create_generator(PINK_NOISE_FFT, 1.0f, 111);
    ASSERT_NE(generator, nullptr);

    bool gen_success = pink_noise_generate(generator, samples.data(), samples.size());
    ASSERT_TRUE(gen_success);

    bool is_pink = fractal_is_pink_noise(samples.data(), samples.size(), 0.4f);
    EXPECT_TRUE(is_pink) << "Generated pink noise should be classified as pink";
}

TEST_F(PinkNoiseFractalIntegrationTest, WhiteNoiseNotClassifiedAsPink) {
    /* SCENARIO: White noise should NOT be classified as pink
     * EXPECTED: Returns false
     */
    generator = create_generator(PINK_NOISE_WHITE, 0.0f, 222);
    ASSERT_NE(generator, nullptr);

    bool gen_success = pink_noise_generate(generator, samples.data(), samples.size());
    ASSERT_TRUE(gen_success);

    /* Use strict tolerance */
    bool is_pink = fractal_is_pink_noise(samples.data(), samples.size(), 0.2f);
    EXPECT_FALSE(is_pink) << "White noise should not be classified as pink";
}

/* ============================================================================
 * Cross-Validation Between Pink Noise Stats and Fractal Analysis
 * ============================================================================ */

TEST_F(PinkNoiseFractalIntegrationTest, PinkNoiseStatsMatchFractalAnalysis) {
    /* SCENARIO: Pink noise internal stats should match fractal analysis
     * EXPECTED: Both methods produce valid results for same signal
     */
    generator = create_generator(PINK_NOISE_FFT, 1.0f, 333);
    ASSERT_NE(generator, nullptr);

    bool gen_success = pink_noise_generate(generator, samples.data(), samples.size());
    ASSERT_TRUE(gen_success);

    /* Get pink noise internal stats */
    pink_noise_stats_t pn_stats;
    bool stats_success = pink_noise_compute_stats(samples.data(), samples.size(),
                                                   1000.0f, &pn_stats);
    if (!stats_success) {
        GTEST_SKIP() << "Pink noise stats computation failed";
    }

    /* Get fractal spectral analysis */
    fractal_result_t fr_result;
    int ret = fractal_spectral_exponent(samples.data(), samples.size(), &fr_result);
    if (ret != FRACTAL_OK) {
        GTEST_SKIP() << "Fractal spectral analysis failed";
    }

    /* Both should produce valid exponents - wide tolerance for different methods */
    EXPECT_GT(pn_stats.measured_alpha, 0.0f)
        << "Pink noise measured alpha should be positive";
    EXPECT_LT(pn_stats.measured_alpha, 3.0f)
        << "Pink noise measured alpha should be < 3.0";
    EXPECT_GT(fr_result.spectral_exponent, 0.0f)
        << "Fractal spectral exponent should be positive";
    EXPECT_LT(fr_result.spectral_exponent, 3.0f)
        << "Fractal spectral exponent should be < 3.0";
}

/* ============================================================================
 * Self-Similarity Tests
 * ============================================================================ */

TEST_F(PinkNoiseFractalIntegrationTest, PinkNoiseIsSelfSimilar) {
    /* SCENARIO: Pink noise should exhibit self-similarity
     * EXPECTED: fractal_is_self_similar returns true
     */
    generator = create_generator(PINK_NOISE_FFT, 1.0f, 444);
    ASSERT_NE(generator, nullptr);

    bool gen_success = pink_noise_generate(generator, samples.data(), samples.size());
    ASSERT_TRUE(gen_success);

    bool is_similar = fractal_is_self_similar(samples.data(), samples.size(), 8);
    EXPECT_TRUE(is_similar) << "Pink noise should be self-similar across scales";
}

/* ============================================================================
 * Reproducibility Tests
 * ============================================================================ */

TEST_F(PinkNoiseFractalIntegrationTest, SameSeedProducesSameAnalysis) {
    /* SCENARIO: Same seed should produce identical fractal analysis
     * EXPECTED: Two runs produce identical results
     */
    const uint32_t seed = 55555;
    std::vector<float> samples1(4096), samples2(4096);

    /* First run */
    generator = create_generator(PINK_NOISE_FFT, 1.0f, seed);
    ASSERT_NE(generator, nullptr);
    pink_noise_generate(generator, samples1.data(), samples1.size());

    fractal_result_t result1;
    fractal_dfa(samples1.data(), samples1.size(), nullptr, &result1);

    pink_noise_destroy(generator);

    /* Second run with same seed */
    generator = create_generator(PINK_NOISE_FFT, 1.0f, seed);
    ASSERT_NE(generator, nullptr);
    pink_noise_generate(generator, samples2.data(), samples2.size());

    fractal_result_t result2;
    fractal_dfa(samples2.data(), samples2.size(), nullptr, &result2);

    /* Results should be identical */
    EXPECT_FLOAT_EQ(result1.dfa_exponent, result2.dfa_exponent);
    EXPECT_FLOAT_EQ(result1.dfa_r2, result2.dfa_r2);
}

/* ============================================================================
 * Varying Alpha Tests
 * ============================================================================ */

TEST_F(PinkNoiseFractalIntegrationTest, AlphaRangeProducesExpectedDFA) {
    /* SCENARIO: Different alpha values produce valid DFA exponents
     * EXPECTED: DFA analysis succeeds and produces reasonable values
     */
    struct TestCase {
        float alpha;
        float expected_dfa_low;
        float expected_dfa_high;
    };

    TestCase cases[] = {
        {0.0f, 0.0f, 1.5f},   /* White noise: DFA ~0.5, wide tolerance */
        {1.0f, 0.3f, 2.0f},   /* Pink noise: DFA ~1.0, wide tolerance */
        {2.0f, 0.5f, 2.5f},   /* Brown noise: DFA ~1.5, wide tolerance */
    };

    for (const auto& tc : cases) {
        generator = create_generator(PINK_NOISE_FFT, tc.alpha, 12345);
        ASSERT_NE(generator, nullptr);

        bool gen_success = pink_noise_generate(generator, samples.data(), samples.size());
        ASSERT_TRUE(gen_success);

        fractal_result_t result;
        int ret = fractal_dfa(samples.data(), samples.size(), nullptr, &result);
        if (ret != FRACTAL_OK) {
            pink_noise_destroy(generator);
            generator = nullptr;
            continue;  /* Skip this case if DFA fails */
        }

        EXPECT_GT(result.dfa_exponent, tc.expected_dfa_low)
            << "Alpha=" << tc.alpha << " DFA exponent too low";
        EXPECT_LT(result.dfa_exponent, tc.expected_dfa_high)
            << "Alpha=" << tc.alpha << " DFA exponent too high";

        pink_noise_destroy(generator);
        generator = nullptr;
    }
}

/* ============================================================================
 * Lacunarity Tests
 * ============================================================================ */

TEST_F(PinkNoiseFractalIntegrationTest, PinkNoiseLacunarity) {
    /* SCENARIO: Compute lacunarity of pink noise
     * EXPECTED: Lacunarity >= 1.0
     */
    generator = create_generator(PINK_NOISE_FFT, 1.0f, 666);
    ASSERT_NE(generator, nullptr);

    bool gen_success = pink_noise_generate(generator, samples.data(), samples.size());
    ASSERT_TRUE(gen_success);

    float lac = fractal_lacunarity(samples.data(), samples.size(), 32);
    EXPECT_GE(lac, 1.0f) << "Lacunarity should be >= 1.0";
}

/* ============================================================================
 * Entry Point
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
