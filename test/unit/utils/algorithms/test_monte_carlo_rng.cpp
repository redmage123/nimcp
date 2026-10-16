/**
 * @file test_monte_carlo_rng.cpp
 * @brief Unit tests for Monte Carlo RNG functions
 *
 * WHAT: Tests for random number generation in Monte Carlo module
 * WHY:  Verify correctness of RNG algorithms for numerical simulations
 * HOW:  Statistical tests for distribution properties, edge cases
 *
 * TESTS COVER:
 * 1. Box-Muller Gaussian distribution correctness
 * 2. Uniform RNG produces values in [0, 1)
 * 3. Both Gaussian outputs are properly utilized (Box-Muller generates pairs)
 * 4. Statistical tests for mean/variance of generated distributions
 * 5. NULL safety for RNG functions
 * 6. Seed determinism and reproducibility
 *
 * @version 1.0.0
 * @date 2025-01-15
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <algorithm>
#include <numeric>

extern "C" {
#include "utils/algorithms/nimcp_monte_carlo.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class MonteCarloRNGTest : public ::testing::Test {
protected:
    uint32_t seed;

    void SetUp() override {
        seed = 12345u;  /* Fixed seed for reproducibility */
    }

    void TearDown() override {
        /* No cleanup needed */
    }

    /**
     * @brief Compute sample mean
     */
    double compute_mean(const std::vector<float>& samples) {
        if (samples.empty()) return 0.0;
        double sum = std::accumulate(samples.begin(), samples.end(), 0.0);
        return sum / static_cast<double>(samples.size());
    }

    /**
     * @brief Compute sample variance
     */
    double compute_variance(const std::vector<float>& samples, double mean) {
        if (samples.size() < 2) return 0.0;
        double sum_sq = 0.0;
        for (float s : samples) {
            double diff = static_cast<double>(s) - mean;
            sum_sq += diff * diff;
        }
        return sum_sq / static_cast<double>(samples.size() - 1);
    }

    /**
     * @brief Compute sample skewness
     */
    double compute_skewness(const std::vector<float>& samples, double mean, double stddev) {
        if (samples.size() < 3 || stddev == 0.0) return 0.0;
        double sum_cube = 0.0;
        for (float s : samples) {
            double diff = (static_cast<double>(s) - mean) / stddev;
            sum_cube += diff * diff * diff;
        }
        return sum_cube / static_cast<double>(samples.size());
    }

    /**
     * @brief Compute sample kurtosis (excess)
     */
    double compute_kurtosis(const std::vector<float>& samples, double mean, double stddev) {
        if (samples.size() < 4 || stddev == 0.0) return 0.0;
        double sum_fourth = 0.0;
        for (float s : samples) {
            double diff = (static_cast<double>(s) - mean) / stddev;
            double sq = diff * diff;
            sum_fourth += sq * sq;
        }
        return (sum_fourth / static_cast<double>(samples.size())) - 3.0;
    }
};

//=============================================================================
// Uniform RNG Tests
//=============================================================================

TEST_F(MonteCarloRNGTest, UniformRNGProducesValuesInCorrectRange) {
    /**
     * WHAT: Verify uniform RNG produces values in [0, 1)
     * WHY:  Out-of-range values would break probability calculations
     * HOW:  Generate large sample, check all values are in range
     */
    const int num_samples = 100000;

    for (int i = 0; i < num_samples; i++) {
        float val = mc_random_uniform(&seed);

        /* Must be >= 0 */
        ASSERT_GE(val, 0.0f) << "Value " << val << " is below 0 at iteration " << i;

        /* Must be < 1 (strict inequality) */
        ASSERT_LT(val, 1.0f) << "Value " << val << " is >= 1 at iteration " << i;
    }
}

TEST_F(MonteCarloRNGTest, UniformRNGMeanApproximatelyHalf) {
    /**
     * WHAT: Verify uniform RNG mean is approximately 0.5
     * WHY:  Uniform [0,1) distribution has theoretical mean of 0.5
     * HOW:  Generate samples, compute sample mean, verify within tolerance
     */
    const int num_samples = 100000;
    std::vector<float> samples(num_samples);

    for (int i = 0; i < num_samples; i++) {
        samples[i] = mc_random_uniform(&seed);
    }

    double mean = compute_mean(samples);

    /* Expected mean = 0.5, tolerance ~0.01 for 100k samples */
    EXPECT_NEAR(mean, 0.5, 0.01) << "Uniform mean should be ~0.5";
}

TEST_F(MonteCarloRNGTest, UniformRNGVarianceApproximatelyOneTwelfth) {
    /**
     * WHAT: Verify uniform RNG variance is approximately 1/12
     * WHY:  Uniform [0,1) distribution has theoretical variance of 1/12 = 0.0833...
     * HOW:  Generate samples, compute sample variance, verify within tolerance
     */
    const int num_samples = 100000;
    std::vector<float> samples(num_samples);

    for (int i = 0; i < num_samples; i++) {
        samples[i] = mc_random_uniform(&seed);
    }

    double mean = compute_mean(samples);
    double variance = compute_variance(samples, mean);

    /* Expected variance = 1/12 = 0.0833... */
    double expected_variance = 1.0 / 12.0;
    EXPECT_NEAR(variance, expected_variance, 0.005)
        << "Uniform variance should be ~1/12";
}

TEST_F(MonteCarloRNGTest, UniformRNGNullSeedReturnsZero) {
    /**
     * WHAT: Verify NULL seed handling
     * WHY:  Prevent crashes and undefined behavior
     * HOW:  Pass NULL seed, verify graceful handling
     */
    float result = mc_random_uniform(NULL);
    EXPECT_EQ(result, 0.0f) << "NULL seed should return 0";
}

TEST_F(MonteCarloRNGTest, UniformRNGDeterministicWithSameSeed) {
    /**
     * WHAT: Verify determinism with same seed
     * WHY:  Reproducibility is essential for testing and debugging
     * HOW:  Generate sequences with same seed, verify identical
     */
    uint32_t seed1 = 42;
    uint32_t seed2 = 42;

    for (int i = 0; i < 100; i++) {
        float val1 = mc_random_uniform(&seed1);
        float val2 = mc_random_uniform(&seed2);
        EXPECT_EQ(val1, val2) << "Same seed should produce same sequence";
    }
}

TEST_F(MonteCarloRNGTest, UniformRNGDifferentWithDifferentSeeds) {
    /**
     * WHAT: Verify different seeds produce different sequences
     * WHY:  Seeds should control RNG state independently
     * HOW:  Generate with different seeds, verify different values
     */
    uint32_t seed1 = 42;
    uint32_t seed2 = 43;

    float val1 = mc_random_uniform(&seed1);
    float val2 = mc_random_uniform(&seed2);

    /* Very unlikely to be equal with different seeds */
    EXPECT_NE(val1, val2) << "Different seeds should produce different values";
}

//=============================================================================
// Integer RNG Tests
//=============================================================================

TEST_F(MonteCarloRNGTest, IntRNGProducesValuesInRange) {
    /**
     * WHAT: Verify integer RNG produces values in [0, max)
     * WHY:  Out-of-range indices would cause array overflows
     * HOW:  Generate samples, verify all in range
     */
    const uint32_t max_val = 100;
    const int num_samples = 10000;

    for (int i = 0; i < num_samples; i++) {
        uint32_t val = mc_random_int(&seed, max_val);
        ASSERT_LT(val, max_val) << "Value " << val << " >= max " << max_val;
    }
}

TEST_F(MonteCarloRNGTest, IntRNGNullSeedReturnsZero) {
    /**
     * WHAT: Verify NULL seed handling for integer RNG
     * WHY:  Prevent crashes
     * HOW:  Pass NULL seed, verify returns 0
     */
    uint32_t result = mc_random_int(NULL, 100);
    EXPECT_EQ(result, 0u);
}

TEST_F(MonteCarloRNGTest, IntRNGZeroMaxReturnsZero) {
    /**
     * WHAT: Verify zero max handling
     * WHY:  Prevent division by zero
     * HOW:  Pass zero max, verify returns 0
     */
    uint32_t result = mc_random_int(&seed, 0);
    EXPECT_EQ(result, 0u);
}

//=============================================================================
// Gaussian (Normal) RNG Tests - Box-Muller
//=============================================================================

TEST_F(MonteCarloRNGTest, NormalRNGMeanApproximatesTarget) {
    /**
     * WHAT: Verify Gaussian RNG produces correct mean
     * WHY:  Box-Muller must correctly shift by mean parameter
     * HOW:  Generate samples, compute sample mean, verify near target
     */
    const float target_mean = 5.0f;
    const float target_stddev = 2.0f;
    const int num_samples = 100000;
    std::vector<float> samples(num_samples);

    for (int i = 0; i < num_samples; i++) {
        samples[i] = mc_random_normal(&seed, target_mean, target_stddev);
    }

    double sample_mean = compute_mean(samples);

    /* Allow tolerance based on standard error */
    double tolerance = 3.0 * target_stddev / std::sqrt(static_cast<double>(num_samples));
    EXPECT_NEAR(sample_mean, target_mean, tolerance)
        << "Sample mean should approximate target mean";
}

TEST_F(MonteCarloRNGTest, NormalRNGVarianceApproximatesTarget) {
    /**
     * WHAT: Verify Gaussian RNG produces correct variance
     * WHY:  Box-Muller must correctly scale by stddev parameter
     * HOW:  Generate samples, compute sample variance, verify near target^2
     */
    const float target_mean = 0.0f;
    const float target_stddev = 3.0f;
    const int num_samples = 100000;
    std::vector<float> samples(num_samples);

    for (int i = 0; i < num_samples; i++) {
        samples[i] = mc_random_normal(&seed, target_mean, target_stddev);
    }

    double sample_mean = compute_mean(samples);
    double sample_variance = compute_variance(samples, sample_mean);
    double expected_variance = target_stddev * target_stddev;

    /* Variance converges slower than mean, use larger tolerance */
    double tolerance = 0.5;  /* About 5% for variance of 9 */
    EXPECT_NEAR(sample_variance, expected_variance, tolerance)
        << "Sample variance should approximate target variance";
}

TEST_F(MonteCarloRNGTest, NormalRNGSkewnessNearZero) {
    /**
     * WHAT: Verify Gaussian distribution has near-zero skewness
     * WHY:  Normal distribution is symmetric, skewness should be 0
     * HOW:  Generate samples, compute skewness, verify near zero
     */
    const float mean = 0.0f;
    const float stddev = 1.0f;
    const int num_samples = 100000;
    std::vector<float> samples(num_samples);

    for (int i = 0; i < num_samples; i++) {
        samples[i] = mc_random_normal(&seed, mean, stddev);
    }

    double sample_mean = compute_mean(samples);
    double sample_stddev = std::sqrt(compute_variance(samples, sample_mean));
    double skewness = compute_skewness(samples, sample_mean, sample_stddev);

    /* Skewness should be near 0 for normal distribution */
    EXPECT_NEAR(skewness, 0.0, 0.05) << "Normal distribution should have near-zero skewness";
}

TEST_F(MonteCarloRNGTest, NormalRNGKurtosisNearZero) {
    /**
     * WHAT: Verify Gaussian distribution has near-zero excess kurtosis
     * WHY:  Normal distribution has kurtosis 3, excess kurtosis 0
     * HOW:  Generate samples, compute excess kurtosis, verify near zero
     */
    const float mean = 0.0f;
    const float stddev = 1.0f;
    const int num_samples = 100000;
    std::vector<float> samples(num_samples);

    for (int i = 0; i < num_samples; i++) {
        samples[i] = mc_random_normal(&seed, mean, stddev);
    }

    double sample_mean = compute_mean(samples);
    double sample_stddev = std::sqrt(compute_variance(samples, sample_mean));
    double kurtosis = compute_kurtosis(samples, sample_mean, sample_stddev);

    /* Excess kurtosis should be near 0 for normal distribution */
    EXPECT_NEAR(kurtosis, 0.0, 0.15) << "Normal distribution should have near-zero excess kurtosis";
}

TEST_F(MonteCarloRNGTest, NormalRNGNullSeedReturnsMean) {
    /**
     * WHAT: Verify NULL seed handling
     * WHY:  Prevent crashes and undefined behavior
     * HOW:  Pass NULL seed, verify returns mean as fallback
     */
    float result = mc_random_normal(NULL, 5.0f, 1.0f);
    EXPECT_EQ(result, 5.0f) << "NULL seed should return mean";
}

TEST_F(MonteCarloRNGTest, NormalRNGZeroStddevReturnsMean) {
    /**
     * WHAT: Verify zero stddev produces constant mean
     * WHY:  Zero variance distribution is a point mass at mean
     * HOW:  Generate with stddev=0, verify all values equal mean
     */
    const float mean = 7.5f;

    for (int i = 0; i < 100; i++) {
        float val = mc_random_normal(&seed, mean, 0.0f);
        EXPECT_EQ(val, mean) << "Zero stddev should return exactly mean";
    }
}

TEST_F(MonteCarloRNGTest, BoxMullerProducesValidValues) {
    /**
     * WHAT: Verify Box-Muller doesn't produce NaN or Inf
     * WHY:  Edge cases (u1 near 0) could cause log(0) issues
     * HOW:  Generate many samples, check all are finite
     */
    const int num_samples = 100000;

    for (int i = 0; i < num_samples; i++) {
        float val = mc_random_normal(&seed, 0.0f, 1.0f);
        ASSERT_TRUE(std::isfinite(val))
            << "Box-Muller produced non-finite value: " << val << " at iteration " << i;
    }
}

TEST_F(MonteCarloRNGTest, BoxMullerCoverageTest) {
    /**
     * WHAT: Verify Box-Muller uses both uniform inputs properly
     * WHY:  The transform uses sqrt(-2*log(u1)) * cos(2*pi*u2)
     *       Both u1 and u2 must be used correctly
     * HOW:  Test that distribution spans expected range (+-3 sigma commonly)
     */
    const float mean = 0.0f;
    const float stddev = 1.0f;
    const int num_samples = 100000;

    float min_val = std::numeric_limits<float>::max();
    float max_val = std::numeric_limits<float>::lowest();

    for (int i = 0; i < num_samples; i++) {
        float val = mc_random_normal(&seed, mean, stddev);
        min_val = std::min(min_val, val);
        max_val = std::max(max_val, val);
    }

    /* With 100k samples, we should see values beyond +-3 sigma */
    EXPECT_LT(min_val, -3.0f) << "Should see values below -3 sigma";
    EXPECT_GT(max_val, 3.0f) << "Should see values above +3 sigma";
}

//=============================================================================
// Weighted Choice Tests
//=============================================================================

TEST_F(MonteCarloRNGTest, WeightedChoiceRespectsWeights) {
    /**
     * WHAT: Verify weighted choice respects probability weights
     * WHY:  Incorrect weighting would bias selection
     * HOW:  Use heavily skewed weights, verify selection distribution
     */
    float weights[] = {0.1f, 0.1f, 0.8f};  /* Index 2 should be selected ~80% */
    uint32_t counts[3] = {0, 0, 0};
    const int num_samples = 10000;

    for (int i = 0; i < num_samples; i++) {
        uint32_t choice = mc_random_choice(&seed, weights, 3);
        ASSERT_LT(choice, 3u);
        counts[choice]++;
    }

    /* Index 2 should be selected approximately 80% */
    double ratio = static_cast<double>(counts[2]) / num_samples;
    EXPECT_NEAR(ratio, 0.8, 0.05) << "Heavily weighted option should be selected ~80%";
}

TEST_F(MonteCarloRNGTest, WeightedChoiceNullReturnsZero) {
    /**
     * WHAT: Verify NULL handling
     * WHY:  Prevent crashes
     * HOW:  Pass NULL, verify returns 0
     */
    float weights[] = {0.5f, 0.5f};

    EXPECT_EQ(mc_random_choice(NULL, weights, 2), 0u);
    EXPECT_EQ(mc_random_choice(&seed, NULL, 2), 0u);
    EXPECT_EQ(mc_random_choice(&seed, weights, 0), 0u);
}

TEST_F(MonteCarloRNGTest, WeightedChoiceZeroWeightsUniform) {
    /**
     * WHAT: Verify all-zero weights fall back to uniform
     * WHY:  Division by zero must be handled gracefully
     * HOW:  Use zero weights, verify roughly uniform distribution
     */
    float weights[] = {0.0f, 0.0f, 0.0f};
    uint32_t counts[3] = {0, 0, 0};
    const int num_samples = 3000;

    for (int i = 0; i < num_samples; i++) {
        uint32_t choice = mc_random_choice(&seed, weights, 3);
        ASSERT_LT(choice, 3u);
        counts[choice]++;
    }

    /* Should be roughly uniform (each ~33%) */
    for (int i = 0; i < 3; i++) {
        double ratio = static_cast<double>(counts[i]) / num_samples;
        EXPECT_NEAR(ratio, 0.333, 0.1) << "Zero weights should produce uniform distribution";
    }
}

//=============================================================================
// Statistical Utility Tests
//=============================================================================

TEST_F(MonteCarloRNGTest, MCMeanCorrect) {
    /**
     * WHAT: Verify mc_mean computes correct mean
     * WHY:  Core statistical function must be accurate
     * HOW:  Test with known values
     */
    float values[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    float mean = mc_mean(values, 5);
    EXPECT_FLOAT_EQ(mean, 3.0f);
}

TEST_F(MonteCarloRNGTest, MCMeanNullReturnsZero) {
    /**
     * WHAT: Verify NULL handling
     */
    EXPECT_EQ(mc_mean(NULL, 5), 0.0f);
}

TEST_F(MonteCarloRNGTest, MCMeanZeroCountReturnsZero) {
    /**
     * WHAT: Verify zero count handling
     */
    float values[] = {1.0f};
    EXPECT_EQ(mc_mean(values, 0), 0.0f);
}

TEST_F(MonteCarloRNGTest, MCVarianceCorrect) {
    /**
     * WHAT: Verify mc_variance computes correct sample variance
     * WHY:  Sample variance uses n-1 denominator
     * HOW:  Test with known values
     */
    float values[] = {2.0f, 4.0f, 4.0f, 4.0f, 5.0f, 5.0f, 7.0f, 9.0f};
    float mean = mc_mean(values, 8);  /* mean = 5 */
    float variance = mc_variance(values, 8, mean);

    /* Sample variance = sum((x-mean)^2) / (n-1) */
    /* = (9+1+1+1+0+0+4+16) / 7 = 32/7 = 4.571... */
    EXPECT_NEAR(variance, 32.0f / 7.0f, 0.001f);
}

TEST_F(MonteCarloRNGTest, MCVarianceNullReturnsZero) {
    /**
     * WHAT: Verify NULL handling
     */
    EXPECT_EQ(mc_variance(NULL, 5, 0.0f), 0.0f);
}

TEST_F(MonteCarloRNGTest, MCVarianceLessThanTwoReturnsZero) {
    /**
     * WHAT: Verify n<2 returns zero
     * WHY:  Sample variance undefined for n<2
     */
    float values[] = {1.0f};
    EXPECT_EQ(mc_variance(values, 1, 1.0f), 0.0f);
}

TEST_F(MonteCarloRNGTest, MCStdErrorCorrect) {
    /**
     * WHAT: Verify mc_std_error computes correct standard error
     * WHY:  SE = sqrt(variance / n)
     * HOW:  Test with known values
     */
    float se = mc_std_error(16.0f, 100);  /* sqrt(16/100) = sqrt(0.16) = 0.4 */
    EXPECT_FLOAT_EQ(se, 0.4f);
}

TEST_F(MonteCarloRNGTest, MCStdErrorZeroNReturnsZero) {
    /**
     * WHAT: Verify n=0 returns zero
     */
    EXPECT_EQ(mc_std_error(1.0f, 0), 0.0f);
}

//=============================================================================
// Shuffle Tests
//=============================================================================

TEST_F(MonteCarloRNGTest, ShuffleU32Permutes) {
    /**
     * WHAT: Verify shuffle produces a permutation
     * WHY:  Shuffle must not lose or duplicate elements
     * HOW:  Shuffle array, verify all elements present
     */
    uint32_t array[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    uint32_t n = 10;

    mc_shuffle_u32(array, n, &seed);

    /* Sort and verify all elements present */
    std::vector<uint32_t> sorted(array, array + n);
    std::sort(sorted.begin(), sorted.end());

    for (uint32_t i = 0; i < n; i++) {
        EXPECT_EQ(sorted[i], i) << "Element " << i << " missing after shuffle";
    }
}

TEST_F(MonteCarloRNGTest, ShuffleU32ChangesOrder) {
    /**
     * WHAT: Verify shuffle actually changes order
     * WHY:  Shuffle should not leave array unchanged
     * HOW:  Shuffle, compare to original (statistically unlikely to match)
     */
    uint32_t original[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    uint32_t array[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};

    mc_shuffle_u32(array, 10, &seed);

    /* Check if any elements moved (very unlikely all stay in place) */
    int moved_count = 0;
    for (int i = 0; i < 10; i++) {
        if (array[i] != original[i]) {
            moved_count++;
        }
    }

    EXPECT_GT(moved_count, 0) << "Shuffle should move at least some elements";
}

TEST_F(MonteCarloRNGTest, ShuffleNullSafe) {
    /**
     * WHAT: Verify NULL handling
     */
    uint32_t array[] = {1, 2, 3};
    mc_shuffle_u32(NULL, 3, &seed);  /* Should not crash */
    mc_shuffle_u32(array, 3, NULL);  /* Should not crash */
}

TEST_F(MonteCarloRNGTest, ShuffleTooSmallNoop) {
    /**
     * WHAT: Verify n<2 is no-op
     */
    uint32_t array[] = {42};
    mc_shuffle_u32(array, 1, &seed);
    EXPECT_EQ(array[0], 42u);
}

//=============================================================================
// Seed From Time Tests
//=============================================================================

TEST_F(MonteCarloRNGTest, SeedFromTimeNonZero) {
    /**
     * WHAT: Verify seed from time is non-zero
     * WHY:  Zero seed would be a poor initial state
     * HOW:  Call multiple times, verify non-zero
     */
    uint32_t seed1 = mc_seed_from_time();
    EXPECT_NE(seed1, 0u) << "Seed from time should not be zero";
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
