/**
 * @file test_rand.cpp
 * @brief Comprehensive unit tests for nimcp_rand module
 *
 * WHAT: Tests for unified RNG module with thread-safe, context-based operations
 * WHY:  Verify correctness of RNG algorithms, thread safety, reproducibility
 * HOW:  Statistical tests, thread concurrency tests, seed determinism tests
 *
 * TESTS COVER:
 * 1. Global initialization/shutdown lifecycle
 * 2. Thread-local RNG functions (uniform, int, range, normal, pink)
 * 3. Seeding operations (seed, get_seed, entropy_seed)
 * 4. Context-based RNG (create, destroy, clone, sampling)
 * 5. Array/batch operations (uniform_array, normal_array, shuffle)
 * 6. Statistical validation (mean, variance, distribution properties)
 * 7. Thread safety (concurrent access to thread-local RNG)
 * 8. Reproducibility (same seed produces same sequence)
 *
 * @version 1.0.0
 * @date 2026-01-15
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <algorithm>
#include <numeric>
#include <thread>
#include <mutex>
#include <atomic>
#include <set>

extern "C" {
#include "utils/rng/nimcp_rand.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class RandTest : public ::testing::Test {
protected:
    void SetUp() override {
        /* Initialize RNG subsystem with default config */
        nimcp_rand_result_t result = nimcp_rand_init(NULL);
        ASSERT_EQ(result, NIMCP_RAND_OK) << "Failed to initialize nimcp_rand";
    }

    void TearDown() override {
        /* Shutdown RNG subsystem */
        nimcp_rand_shutdown();
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
// Initialization/Shutdown Tests
//=============================================================================

TEST_F(RandTest, InitShutdownLifecycle) {
    /**
     * WHAT: Verify init/shutdown lifecycle works correctly
     * WHY:  Subsystem must initialize and shutdown cleanly
     * HOW:  Check initialized state before/after operations
     */
    /* Already initialized in SetUp, verify it */
    EXPECT_TRUE(nimcp_rand_is_initialized());

    /* Shutdown */
    nimcp_rand_shutdown();
    EXPECT_FALSE(nimcp_rand_is_initialized());

    /* Re-initialize */
    nimcp_rand_result_t result = nimcp_rand_init(NULL);
    EXPECT_EQ(result, NIMCP_RAND_OK);
    EXPECT_TRUE(nimcp_rand_is_initialized());
}

TEST_F(RandTest, InitWithConfig) {
    /**
     * WHAT: Verify initialization with custom config
     * WHY:  Users should be able to configure backend and options
     * HOW:  Create config, initialize with it, verify behavior
     */
    nimcp_rand_shutdown();

    nimcp_rand_config_t config = nimcp_rand_default_config();
    config.default_backend = NIMCP_RAND_BACKEND_LCG;
    config.global_seed = 42;
    config.thread_local_seeding = true;

    nimcp_rand_result_t result = nimcp_rand_init(&config);
    EXPECT_EQ(result, NIMCP_RAND_OK);
    EXPECT_TRUE(nimcp_rand_is_initialized());
}

TEST_F(RandTest, DoubleInitNoOp) {
    /**
     * WHAT: Verify double initialization is safe
     * WHY:  Should not crash or corrupt state
     * HOW:  Initialize twice, verify still works
     */
    /* Already initialized in SetUp */
    nimcp_rand_result_t result = nimcp_rand_init(NULL);
    /* Second init should be OK or return appropriate error */
    EXPECT_TRUE(result == NIMCP_RAND_OK || result == NIMCP_RAND_ERROR_INIT);

    /* Should still work */
    float val = nimcp_rand_uniform();
    EXPECT_GE(val, 0.0f);
    EXPECT_LT(val, 1.0f);
}

//=============================================================================
// Thread-Local Uniform RNG Tests
//=============================================================================

TEST_F(RandTest, UniformProducesValuesInRange) {
    /**
     * WHAT: Verify uniform RNG produces values in [0, 1)
     * WHY:  Out-of-range values would break probability calculations
     * HOW:  Generate large sample, check all values are in range
     */
    const int num_samples = 100000;

    for (int i = 0; i < num_samples; i++) {
        float val = nimcp_rand_uniform();

        ASSERT_GE(val, 0.0f) << "Value " << val << " is below 0 at iteration " << i;
        ASSERT_LT(val, 1.0f) << "Value " << val << " is >= 1 at iteration " << i;
    }
}

TEST_F(RandTest, UniformMeanApproximatelyHalf) {
    /**
     * WHAT: Verify uniform RNG mean is approximately 0.5
     * WHY:  Uniform [0,1) distribution has theoretical mean of 0.5
     * HOW:  Generate samples, compute sample mean, verify within tolerance
     */
    const int num_samples = 100000;
    std::vector<float> samples(num_samples);

    for (int i = 0; i < num_samples; i++) {
        samples[i] = nimcp_rand_uniform();
    }

    double mean = compute_mean(samples);

    /* Expected mean = 0.5, tolerance ~0.01 for 100k samples */
    EXPECT_NEAR(mean, 0.5, 0.01) << "Uniform mean should be ~0.5";
}

TEST_F(RandTest, UniformVarianceApproximatelyOneTwelfth) {
    /**
     * WHAT: Verify uniform RNG variance is approximately 1/12
     * WHY:  Uniform [0,1) distribution has theoretical variance of 1/12 = 0.0833...
     * HOW:  Generate samples, compute sample variance, verify within tolerance
     */
    const int num_samples = 100000;
    std::vector<float> samples(num_samples);

    for (int i = 0; i < num_samples; i++) {
        samples[i] = nimcp_rand_uniform();
    }

    double mean = compute_mean(samples);
    double variance = compute_variance(samples, mean);

    /* Expected variance = 1/12 = 0.0833... */
    double expected_variance = 1.0 / 12.0;
    EXPECT_NEAR(variance, expected_variance, 0.005)
        << "Uniform variance should be ~1/12";
}

TEST_F(RandTest, UniformDoubleHigherPrecision) {
    /**
     * WHAT: Verify double precision uniform RNG
     * WHY:  Some applications need higher precision
     * HOW:  Generate samples, verify range and distinct values
     */
    const int num_samples = 10000;
    std::set<double> unique_values;

    for (int i = 0; i < num_samples; i++) {
        double val = nimcp_rand_uniform_double();

        ASSERT_GE(val, 0.0) << "Value " << val << " is below 0";
        ASSERT_LT(val, 1.0) << "Value " << val << " is >= 1";

        unique_values.insert(val);
    }

    /* Should have many unique values (not degenerate) */
    EXPECT_GT(unique_values.size(), static_cast<size_t>(num_samples * 0.99))
        << "Double RNG should produce mostly unique values";
}

//=============================================================================
// Thread-Local Integer RNG Tests
//=============================================================================

TEST_F(RandTest, IntProducesValuesInRange) {
    /**
     * WHAT: Verify integer RNG produces values in [0, max)
     * WHY:  Out-of-range indices would cause array overflows
     * HOW:  Generate samples, verify all in range
     */
    const int32_t max_val = 100;
    const int num_samples = 10000;

    for (int i = 0; i < num_samples; i++) {
        int32_t val = nimcp_rand_int(max_val);
        ASSERT_GE(val, 0) << "Value " << val << " is negative";
        ASSERT_LT(val, max_val) << "Value " << val << " >= max " << max_val;
    }
}

TEST_F(RandTest, IntZeroMaxReturnsZero) {
    /**
     * WHAT: Verify zero max handling
     * WHY:  Edge case - should return 0 gracefully
     * HOW:  Pass zero max, verify returns 0
     */
    int32_t result = nimcp_rand_int(0);
    EXPECT_EQ(result, 0);
}

TEST_F(RandTest, IntOneMaxReturnsZero) {
    /**
     * WHAT: Verify max=1 always returns 0
     * WHY:  [0, 1) only contains 0
     * HOW:  Generate samples, verify all zero
     */
    for (int i = 0; i < 100; i++) {
        EXPECT_EQ(nimcp_rand_int(1), 0);
    }
}

TEST_F(RandTest, RangeProducesValuesInRange) {
    /**
     * WHAT: Verify range RNG produces values in [min, max]
     * WHY:  Range is inclusive on both ends
     * HOW:  Generate samples, verify all in range
     */
    const int32_t min_val = 10;
    const int32_t max_val = 20;
    const int num_samples = 10000;

    for (int i = 0; i < num_samples; i++) {
        int32_t val = nimcp_rand_range(min_val, max_val);
        ASSERT_GE(val, min_val) << "Value " << val << " < min " << min_val;
        ASSERT_LE(val, max_val) << "Value " << val << " > max " << max_val;
    }
}

TEST_F(RandTest, RangeNegativeValues) {
    /**
     * WHAT: Verify range works with negative values
     * WHY:  Range should support any integer interval
     * HOW:  Test negative-to-negative and negative-to-positive ranges
     */
    /* Negative range */
    for (int i = 0; i < 1000; i++) {
        int32_t val = nimcp_rand_range(-10, -5);
        ASSERT_GE(val, -10);
        ASSERT_LE(val, -5);
    }

    /* Crossing zero */
    for (int i = 0; i < 1000; i++) {
        int32_t val = nimcp_rand_range(-5, 5);
        ASSERT_GE(val, -5);
        ASSERT_LE(val, 5);
    }
}

TEST_F(RandTest, RangeSingleValueReturnsValue) {
    /**
     * WHAT: Verify range with min==max returns that value
     * WHY:  Degenerate range should be handled correctly
     * HOW:  Set min=max, verify always returns that value
     */
    for (int i = 0; i < 100; i++) {
        EXPECT_EQ(nimcp_rand_range(42, 42), 42);
    }
}

TEST_F(RandTest, UintProducesValuesInRange) {
    /**
     * WHAT: Verify uint32_t RNG produces values in [0, max)
     * WHY:  Some APIs need unsigned integers
     * HOW:  Generate samples, verify all in range
     */
    const uint32_t max_val = 1000;
    const int num_samples = 10000;

    for (int i = 0; i < num_samples; i++) {
        uint32_t val = nimcp_rand_uint(max_val);
        ASSERT_LT(val, max_val) << "Value " << val << " >= max " << max_val;
    }
}

//=============================================================================
// Thread-Local Normal (Gaussian) RNG Tests
//=============================================================================

TEST_F(RandTest, NormalMeanApproximatesTarget) {
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
        samples[i] = nimcp_rand_normal(target_mean, target_stddev);
    }

    double sample_mean = compute_mean(samples);

    /* Allow tolerance based on standard error */
    double tolerance = 3.0 * target_stddev / std::sqrt(static_cast<double>(num_samples));
    EXPECT_NEAR(sample_mean, target_mean, tolerance)
        << "Sample mean should approximate target mean";
}

TEST_F(RandTest, NormalVarianceApproximatesTarget) {
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
        samples[i] = nimcp_rand_normal(target_mean, target_stddev);
    }

    double sample_mean = compute_mean(samples);
    double sample_variance = compute_variance(samples, sample_mean);
    double expected_variance = target_stddev * target_stddev;

    /* Variance converges slower than mean, use larger tolerance */
    double tolerance = 0.5;  /* About 5% for variance of 9 */
    EXPECT_NEAR(sample_variance, expected_variance, tolerance)
        << "Sample variance should approximate target variance";
}

TEST_F(RandTest, NormalSkewnessNearZero) {
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
        samples[i] = nimcp_rand_normal(mean, stddev);
    }

    double sample_mean = compute_mean(samples);
    double sample_stddev = std::sqrt(compute_variance(samples, sample_mean));
    double skewness = compute_skewness(samples, sample_mean, sample_stddev);

    /* Skewness should be near 0 for normal distribution */
    EXPECT_NEAR(skewness, 0.0, 0.05) << "Normal distribution should have near-zero skewness";
}

TEST_F(RandTest, NormalKurtosisNearZero) {
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
        samples[i] = nimcp_rand_normal(mean, stddev);
    }

    double sample_mean = compute_mean(samples);
    double sample_stddev = std::sqrt(compute_variance(samples, sample_mean));
    double kurtosis = compute_kurtosis(samples, sample_mean, sample_stddev);

    /* Excess kurtosis should be near 0 for normal distribution */
    EXPECT_NEAR(kurtosis, 0.0, 0.15) << "Normal distribution should have near-zero excess kurtosis";
}

TEST_F(RandTest, NormalZeroStddevReturnsMean) {
    /**
     * WHAT: Verify zero stddev produces constant mean
     * WHY:  Zero variance distribution is a point mass at mean
     * HOW:  Generate with stddev=0, verify all values equal mean
     */
    const float mean = 7.5f;

    for (int i = 0; i < 100; i++) {
        float val = nimcp_rand_normal(mean, 0.0f);
        EXPECT_EQ(val, mean) << "Zero stddev should return exactly mean";
    }
}

TEST_F(RandTest, NormalProducesFiniteValues) {
    /**
     * WHAT: Verify Box-Muller doesn't produce NaN or Inf
     * WHY:  Edge cases (u1 near 0) could cause log(0) issues
     * HOW:  Generate many samples, check all are finite
     */
    const int num_samples = 100000;

    for (int i = 0; i < num_samples; i++) {
        float val = nimcp_rand_normal(0.0f, 1.0f);
        ASSERT_TRUE(std::isfinite(val))
            << "Box-Muller produced non-finite value: " << val << " at iteration " << i;
    }
}

TEST_F(RandTest, NormalCoversTails) {
    /**
     * WHAT: Verify Box-Muller covers distribution tails
     * WHY:  Must sample entire distribution, not just center
     * HOW:  Generate many samples, verify range spans +/- 3 sigma
     */
    const float mean = 0.0f;
    const float stddev = 1.0f;
    const int num_samples = 100000;

    float min_val = std::numeric_limits<float>::max();
    float max_val = std::numeric_limits<float>::lowest();

    for (int i = 0; i < num_samples; i++) {
        float val = nimcp_rand_normal(mean, stddev);
        min_val = std::min(min_val, val);
        max_val = std::max(max_val, val);
    }

    /* With 100k samples, should see values beyond +/- 3 sigma */
    EXPECT_LT(min_val, -3.0f) << "Should see values below -3 sigma";
    EXPECT_GT(max_val, 3.0f) << "Should see values above +3 sigma";
}

//=============================================================================
// Thread-Local Exponential RNG Tests
//=============================================================================

TEST_F(RandTest, ExponentialPositiveValues) {
    /**
     * WHAT: Verify exponential RNG produces positive values
     * WHY:  Exponential distribution is defined only for positive values
     * HOW:  Generate samples, verify all positive
     */
    const int num_samples = 10000;

    for (int i = 0; i < num_samples; i++) {
        float val = nimcp_rand_exponential(1.0f);
        ASSERT_GT(val, 0.0f) << "Exponential should be positive";
    }
}

TEST_F(RandTest, ExponentialMeanApproximatesInverseRate) {
    /**
     * WHAT: Verify exponential mean is approximately 1/rate
     * WHY:  Exponential(rate) has mean = 1/rate
     * HOW:  Generate samples, compute mean, verify near 1/rate
     */
    const float rate = 2.0f;
    const float expected_mean = 1.0f / rate;
    const int num_samples = 100000;
    std::vector<float> samples(num_samples);

    for (int i = 0; i < num_samples; i++) {
        samples[i] = nimcp_rand_exponential(rate);
    }

    double sample_mean = compute_mean(samples);
    EXPECT_NEAR(sample_mean, expected_mean, 0.02) << "Exponential mean should be ~1/rate";
}

//=============================================================================
// Thread-Local Pink Noise Tests
//=============================================================================

TEST_F(RandTest, PinkNoiseInRange) {
    /**
     * WHAT: Verify pink noise is in [-1, 1] range
     * WHY:  Pink noise output should be normalized
     * HOW:  Generate samples, verify range
     */
    const int num_samples = 10000;

    for (int i = 0; i < num_samples; i++) {
        float val = nimcp_rand_pink();
        ASSERT_GE(val, -1.0f) << "Pink noise below -1";
        ASSERT_LE(val, 1.0f) << "Pink noise above 1";
    }
}

TEST_F(RandTest, PinkNoiseMeanNearZero) {
    /**
     * WHAT: Verify pink noise has near-zero mean
     * WHY:  Pink noise should be centered around zero
     * HOW:  Generate samples, compute mean, verify near zero
     */
    const int num_samples = 100000;
    std::vector<float> samples(num_samples);

    for (int i = 0; i < num_samples; i++) {
        samples[i] = nimcp_rand_pink();
    }

    double mean = compute_mean(samples);
    /* Pink noise is correlated by design, so mean can vary more than white noise */
    /* Using 0.15 tolerance to account for statistical variance */
    EXPECT_NEAR(mean, 0.0, 0.15) << "Pink noise mean should be near zero";
}

//=============================================================================
// Seeding Tests
//=============================================================================

TEST_F(RandTest, SeedDeterminism) {
    /**
     * WHAT: Verify same seed produces same sequence
     * WHY:  Reproducibility is essential for testing
     * HOW:  Generate sequences with same seed, verify identical
     */
    const uint64_t seed = 12345;
    const int num_samples = 100;

    /* Generate first sequence */
    nimcp_rand_seed(seed);
    std::vector<float> seq1(num_samples);
    for (int i = 0; i < num_samples; i++) {
        seq1[i] = nimcp_rand_uniform();
    }

    /* Reset seed and generate second sequence */
    nimcp_rand_seed(seed);
    std::vector<float> seq2(num_samples);
    for (int i = 0; i < num_samples; i++) {
        seq2[i] = nimcp_rand_uniform();
    }

    /* Verify sequences are identical */
    for (int i = 0; i < num_samples; i++) {
        EXPECT_EQ(seq1[i], seq2[i]) << "Sequences differ at index " << i;
    }
}

TEST_F(RandTest, DifferentSeedsDifferentSequences) {
    /**
     * WHAT: Verify different seeds produce different sequences
     * WHY:  Seeds should control RNG state independently
     * HOW:  Generate with different seeds, verify different values
     */
    nimcp_rand_seed(42);
    float val1 = nimcp_rand_uniform();

    nimcp_rand_seed(43);
    float val2 = nimcp_rand_uniform();

    EXPECT_NE(val1, val2) << "Different seeds should produce different values";
}

TEST_F(RandTest, GetSeedReturnsCurrentSeed) {
    /**
     * WHAT: Verify get_seed returns the current seed
     * WHY:  Users need to retrieve seed for reproducibility
     * HOW:  Set seed, verify get_seed returns it
     */
    const uint64_t seed = 98765;
    nimcp_rand_seed(seed);
    uint64_t retrieved = nimcp_rand_get_seed();
    EXPECT_EQ(retrieved, seed) << "get_seed should return current seed";
}

TEST_F(RandTest, EntropySeedNonZero) {
    /**
     * WHAT: Verify entropy seed is non-zero
     * WHY:  Zero seed would be poor initial state
     * HOW:  Call entropy_seed, verify non-zero
     */
    uint64_t seed = nimcp_rand_entropy_seed();
    EXPECT_NE(seed, 0u) << "Entropy seed should not be zero";
}

TEST_F(RandTest, EntropySeedVaries) {
    /**
     * WHAT: Verify entropy seed produces different values
     * WHY:  Entropy should be time/hardware dependent
     * HOW:  Call multiple times, verify variation
     */
    std::set<uint64_t> seeds;
    for (int i = 0; i < 10; i++) {
        seeds.insert(nimcp_rand_entropy_seed());
    }
    /* Should have multiple distinct values (timing dependent) */
    EXPECT_GT(seeds.size(), 1u) << "Entropy seed should vary";
}

//=============================================================================
// Context-Based RNG Tests
//=============================================================================

TEST_F(RandTest, ContextCreateDestroy) {
    /**
     * WHAT: Verify context creation and destruction
     * WHY:  Contexts must allocate and free correctly
     * HOW:  Create context, use it, destroy it
     */
    nimcp_rand_ctx_t* ctx = nimcp_rand_ctx_create(12345);
    ASSERT_NE(ctx, nullptr) << "Context creation failed";

    /* Use the context */
    float val = nimcp_rand_ctx_uniform(ctx);
    EXPECT_GE(val, 0.0f);
    EXPECT_LT(val, 1.0f);

    /* Destroy */
    nimcp_rand_ctx_destroy(ctx);
}

TEST_F(RandTest, ContextReproducibility) {
    /**
     * WHAT: Verify context produces reproducible sequences
     * WHY:  Contexts should enable reproducible experiments
     * HOW:  Create two contexts with same seed, verify identical sequences
     */
    const uint64_t seed = 42;
    const int num_samples = 100;

    /* Create first context and generate sequence */
    nimcp_rand_ctx_t* ctx1 = nimcp_rand_ctx_create(seed);
    ASSERT_NE(ctx1, nullptr);
    std::vector<float> seq1(num_samples);
    for (int i = 0; i < num_samples; i++) {
        seq1[i] = nimcp_rand_ctx_uniform(ctx1);
    }

    /* Create second context with same seed */
    nimcp_rand_ctx_t* ctx2 = nimcp_rand_ctx_create(seed);
    ASSERT_NE(ctx2, nullptr);
    std::vector<float> seq2(num_samples);
    for (int i = 0; i < num_samples; i++) {
        seq2[i] = nimcp_rand_ctx_uniform(ctx2);
    }

    /* Verify identical */
    for (int i = 0; i < num_samples; i++) {
        EXPECT_EQ(seq1[i], seq2[i]) << "Context sequences differ at index " << i;
    }

    nimcp_rand_ctx_destroy(ctx1);
    nimcp_rand_ctx_destroy(ctx2);
}

TEST_F(RandTest, ContextIndependence) {
    /**
     * WHAT: Verify contexts are independent
     * WHY:  One context's state should not affect another
     * HOW:  Create two contexts, verify they produce expected sequences
     */
    nimcp_rand_ctx_t* ctx1 = nimcp_rand_ctx_create(100);
    nimcp_rand_ctx_t* ctx2 = nimcp_rand_ctx_create(200);
    ASSERT_NE(ctx1, nullptr);
    ASSERT_NE(ctx2, nullptr);

    /* Generate from ctx1 */
    float val1_a = nimcp_rand_ctx_uniform(ctx1);

    /* Generate from ctx2 */
    float val2_a = nimcp_rand_ctx_uniform(ctx2);

    /* Generate more from ctx1 - should continue its sequence */
    float val1_b = nimcp_rand_ctx_uniform(ctx1);

    /* Verify ctx2 didn't affect ctx1 */
    nimcp_rand_ctx_t* ctx1_verify = nimcp_rand_ctx_create(100);
    float verify_a = nimcp_rand_ctx_uniform(ctx1_verify);
    float verify_b = nimcp_rand_ctx_uniform(ctx1_verify);

    EXPECT_EQ(val1_a, verify_a);
    EXPECT_EQ(val1_b, verify_b);

    nimcp_rand_ctx_destroy(ctx1);
    nimcp_rand_ctx_destroy(ctx2);
    nimcp_rand_ctx_destroy(ctx1_verify);
}

TEST_F(RandTest, ContextClone) {
    /**
     * WHAT: Verify context cloning preserves state
     * WHY:  Cloning enables forking RNG state
     * HOW:  Clone context, verify both produce same subsequent values
     */
    nimcp_rand_ctx_t* ctx1 = nimcp_rand_ctx_create(12345);
    ASSERT_NE(ctx1, nullptr);

    /* Advance ctx1 a bit */
    for (int i = 0; i < 10; i++) {
        nimcp_rand_ctx_uniform(ctx1);
    }

    /* Clone */
    nimcp_rand_ctx_t* ctx2 = nimcp_rand_ctx_clone(ctx1);
    ASSERT_NE(ctx2, nullptr);

    /* Both should produce same sequence from here */
    for (int i = 0; i < 100; i++) {
        float val1 = nimcp_rand_ctx_uniform(ctx1);
        float val2 = nimcp_rand_ctx_uniform(ctx2);
        EXPECT_EQ(val1, val2) << "Cloned contexts diverged at index " << i;
    }

    nimcp_rand_ctx_destroy(ctx1);
    nimcp_rand_ctx_destroy(ctx2);
}

TEST_F(RandTest, ContextReseed) {
    /**
     * WHAT: Verify context reseeding
     * WHY:  Users may need to reset context state
     * HOW:  Reseed context, verify it produces expected sequence
     */
    nimcp_rand_ctx_t* ctx = nimcp_rand_ctx_create(100);
    ASSERT_NE(ctx, nullptr);

    /* Generate some values */
    for (int i = 0; i < 10; i++) {
        nimcp_rand_ctx_uniform(ctx);
    }

    /* Reseed */
    nimcp_rand_ctx_seed(ctx, 100);

    /* Should match fresh context with same seed */
    nimcp_rand_ctx_t* fresh = nimcp_rand_ctx_create(100);
    ASSERT_NE(fresh, nullptr);

    for (int i = 0; i < 50; i++) {
        EXPECT_EQ(nimcp_rand_ctx_uniform(ctx), nimcp_rand_ctx_uniform(fresh))
            << "Reseeded context differs at index " << i;
    }

    nimcp_rand_ctx_destroy(ctx);
    nimcp_rand_ctx_destroy(fresh);
}

TEST_F(RandTest, ContextAllFunctions) {
    /**
     * WHAT: Verify all context sampling functions work
     * WHY:  Context API should mirror thread-local API
     * HOW:  Call each function, verify reasonable output
     */
    nimcp_rand_ctx_t* ctx = nimcp_rand_ctx_create(42);
    ASSERT_NE(ctx, nullptr);

    /* Uniform */
    float u = nimcp_rand_ctx_uniform(ctx);
    EXPECT_GE(u, 0.0f);
    EXPECT_LT(u, 1.0f);

    /* Uniform double */
    double ud = nimcp_rand_ctx_uniform_double(ctx);
    EXPECT_GE(ud, 0.0);
    EXPECT_LT(ud, 1.0);

    /* Int */
    int32_t i = nimcp_rand_ctx_int(ctx, 100);
    EXPECT_GE(i, 0);
    EXPECT_LT(i, 100);

    /* Range */
    int32_t r = nimcp_rand_ctx_range(ctx, -10, 10);
    EXPECT_GE(r, -10);
    EXPECT_LE(r, 10);

    /* Uint */
    uint32_t ui = nimcp_rand_ctx_uint(ctx, 1000);
    EXPECT_LT(ui, 1000u);

    /* Normal */
    float n = nimcp_rand_ctx_normal(ctx, 0.0f, 1.0f);
    EXPECT_TRUE(std::isfinite(n));

    /* Exponential */
    float e = nimcp_rand_ctx_exponential(ctx, 1.0f);
    EXPECT_GT(e, 0.0f);

    /* Pink */
    float p = nimcp_rand_ctx_pink(ctx);
    EXPECT_GE(p, -1.0f);
    EXPECT_LE(p, 1.0f);

    nimcp_rand_ctx_destroy(ctx);
}

TEST_F(RandTest, ContextWithBackend) {
    /**
     * WHAT: Verify context creation with specific backend
     * WHY:  Users may need to specify backend for quality/speed tradeoff
     * HOW:  Create context with backend, verify it works
     */
    nimcp_rand_ctx_t* ctx = nimcp_rand_ctx_create_with_backend(
        12345, NIMCP_RAND_BACKEND_LCG
    );
    ASSERT_NE(ctx, nullptr);

    /* Should work like normal */
    for (int i = 0; i < 100; i++) {
        float val = nimcp_rand_ctx_uniform(ctx);
        EXPECT_GE(val, 0.0f);
        EXPECT_LT(val, 1.0f);
    }

    nimcp_rand_ctx_destroy(ctx);
}

//=============================================================================
// Array/Batch Operation Tests
//=============================================================================

TEST_F(RandTest, UniformArrayFillsCorrectly) {
    /**
     * WHAT: Verify uniform array fills all elements
     * WHY:  Batch operations must fill entire array
     * HOW:  Fill array, verify all in range
     */
    const size_t n = 1000;
    std::vector<float> array(n, -1.0f);  /* Initialize to invalid value */

    nimcp_rand_uniform_array(array.data(), n);

    for (size_t i = 0; i < n; i++) {
        EXPECT_GE(array[i], 0.0f) << "Element " << i << " below range";
        EXPECT_LT(array[i], 1.0f) << "Element " << i << " above range";
    }
}

TEST_F(RandTest, UniformArrayStatistics) {
    /**
     * WHAT: Verify uniform array has correct statistics
     * WHY:  Batch must use same distribution as single calls
     * HOW:  Generate array, verify mean and variance
     */
    const size_t n = 100000;
    std::vector<float> array(n);

    nimcp_rand_uniform_array(array.data(), n);

    double mean = compute_mean(array);
    double variance = compute_variance(array, mean);

    EXPECT_NEAR(mean, 0.5, 0.01);
    EXPECT_NEAR(variance, 1.0/12.0, 0.005);
}

TEST_F(RandTest, NormalArrayFillsCorrectly) {
    /**
     * WHAT: Verify normal array fills with correct parameters
     * WHY:  Batch normal generation must respect mean/stddev
     * HOW:  Fill array, verify statistics
     */
    const size_t n = 100000;
    const float mean = 10.0f;
    const float stddev = 2.0f;
    std::vector<float> array(n);

    nimcp_rand_normal_array(array.data(), n, mean, stddev);

    double sample_mean = compute_mean(array);
    double sample_variance = compute_variance(array, sample_mean);

    EXPECT_NEAR(sample_mean, mean, 0.05);
    EXPECT_NEAR(sample_variance, stddev * stddev, 0.2);
}

TEST_F(RandTest, ShuffleU32Permutes) {
    /**
     * WHAT: Verify shuffle produces a permutation
     * WHY:  Shuffle must not lose or duplicate elements
     * HOW:  Shuffle array, verify all elements present
     */
    const size_t n = 100;
    std::vector<uint32_t> array(n);
    for (size_t i = 0; i < n; i++) {
        array[i] = static_cast<uint32_t>(i);
    }

    nimcp_rand_shuffle_u32(array.data(), n);

    /* Sort and verify all elements present */
    std::vector<uint32_t> sorted = array;
    std::sort(sorted.begin(), sorted.end());

    for (size_t i = 0; i < n; i++) {
        EXPECT_EQ(sorted[i], static_cast<uint32_t>(i)) << "Element " << i << " missing";
    }
}

TEST_F(RandTest, ShuffleU32ChangesOrder) {
    /**
     * WHAT: Verify shuffle actually changes order
     * WHY:  Shuffle should not leave array unchanged
     * HOW:  Shuffle, verify at least some elements moved
     */
    const size_t n = 100;
    std::vector<uint32_t> original(n);
    std::vector<uint32_t> array(n);
    for (size_t i = 0; i < n; i++) {
        original[i] = static_cast<uint32_t>(i);
        array[i] = static_cast<uint32_t>(i);
    }

    nimcp_rand_shuffle_u32(array.data(), n);

    int moved_count = 0;
    for (size_t i = 0; i < n; i++) {
        if (array[i] != original[i]) {
            moved_count++;
        }
    }

    /* Very unlikely all stay in place */
    EXPECT_GT(moved_count, 0) << "Shuffle should move at least some elements";
}

TEST_F(RandTest, ShuffleGenericWorks) {
    /**
     * WHAT: Verify generic shuffle works with different element sizes
     * WHY:  Generic shuffle must handle any element size
     * HOW:  Shuffle struct array, verify permutation
     */
    struct TestStruct {
        int id;
        float value;
    };

    const size_t n = 50;
    std::vector<TestStruct> array(n);
    for (size_t i = 0; i < n; i++) {
        array[i].id = static_cast<int>(i);
        array[i].value = static_cast<float>(i) * 1.5f;
    }

    nimcp_rand_shuffle(array.data(), n, sizeof(TestStruct));

    /* Verify all IDs still present */
    std::set<int> ids;
    for (size_t i = 0; i < n; i++) {
        ids.insert(array[i].id);
        /* Verify struct integrity */
        EXPECT_FLOAT_EQ(array[i].value, static_cast<float>(array[i].id) * 1.5f);
    }

    EXPECT_EQ(ids.size(), n) << "Not all elements present after shuffle";
}

TEST_F(RandTest, ShuffleSmallArrays) {
    /**
     * WHAT: Verify shuffle handles small arrays correctly
     * WHY:  Edge cases for n=0, n=1, n=2
     * HOW:  Test each case
     */
    /* n=0 - should not crash */
    nimcp_rand_shuffle_u32(nullptr, 0);

    /* n=1 - should be no-op */
    uint32_t single[] = {42};
    nimcp_rand_shuffle_u32(single, 1);
    EXPECT_EQ(single[0], 42u);

    /* n=2 - should swap or not */
    uint32_t pair[] = {1, 2};
    nimcp_rand_shuffle_u32(pair, 2);
    EXPECT_TRUE((pair[0] == 1 && pair[1] == 2) || (pair[0] == 2 && pair[1] == 1));
}

TEST_F(RandTest, ChoiceRespectsWeights) {
    /**
     * WHAT: Verify weighted choice respects probability weights
     * WHY:  Incorrect weighting would bias selection
     * HOW:  Use heavily skewed weights, verify distribution
     */
    float weights[] = {0.1f, 0.1f, 0.8f};
    uint32_t counts[3] = {0, 0, 0};
    const int num_samples = 10000;

    for (int i = 0; i < num_samples; i++) {
        uint32_t choice = nimcp_rand_choice(weights, 3);
        ASSERT_LT(choice, 3u);
        counts[choice]++;
    }

    /* Index 2 should be selected approximately 80% */
    double ratio = static_cast<double>(counts[2]) / num_samples;
    EXPECT_NEAR(ratio, 0.8, 0.05) << "Heavily weighted option should be selected ~80%";
}

TEST_F(RandTest, SampleWithoutReplacement) {
    /**
     * WHAT: Verify sample produces k unique indices
     * WHY:  Sample without replacement must not repeat
     * HOW:  Sample, verify all indices unique and in range
     */
    const uint32_t n = 100;
    const uint32_t k = 20;
    std::vector<uint32_t> out(k);

    nimcp_rand_result_t result = nimcp_rand_sample(n, k, out.data());
    EXPECT_EQ(result, NIMCP_RAND_OK);

    /* Verify all in range */
    for (uint32_t i = 0; i < k; i++) {
        EXPECT_LT(out[i], n) << "Index " << out[i] << " out of range";
    }

    /* Verify all unique */
    std::set<uint32_t> unique(out.begin(), out.end());
    EXPECT_EQ(unique.size(), k) << "Sample should produce unique indices";
}

TEST_F(RandTest, SampleKEqualsN) {
    /**
     * WHAT: Verify sample k=n returns all indices
     * WHY:  Edge case - should be a permutation
     * HOW:  Sample all, verify all indices present
     */
    const uint32_t n = 50;
    std::vector<uint32_t> out(n);

    nimcp_rand_result_t result = nimcp_rand_sample(n, n, out.data());
    EXPECT_EQ(result, NIMCP_RAND_OK);

    std::set<uint32_t> unique(out.begin(), out.end());
    EXPECT_EQ(unique.size(), n);
}

//=============================================================================
// Random Bytes Tests
//=============================================================================

TEST_F(RandTest, BytesFillsBuffer) {
    /**
     * WHAT: Verify bytes fills entire buffer
     * WHY:  Cryptographic operations need full buffers
     * HOW:  Fill buffer, verify non-trivial content
     */
    const size_t len = 256;
    std::vector<uint8_t> buffer(len, 0);

    nimcp_rand_result_t result = nimcp_rand_bytes(buffer.data(), len);
    EXPECT_EQ(result, NIMCP_RAND_OK);

    /* Check not all zeros (extremely unlikely) */
    int nonzero = 0;
    for (size_t i = 0; i < len; i++) {
        if (buffer[i] != 0) nonzero++;
    }
    EXPECT_GT(nonzero, 0) << "Buffer should have non-zero bytes";
}

TEST_F(RandTest, BytesDistribution) {
    /**
     * WHAT: Verify bytes are approximately uniform
     * WHY:  Each byte should have equal probability
     * HOW:  Generate many bytes, check distribution
     */
    const size_t len = 100000;
    std::vector<uint8_t> buffer(len);
    uint32_t counts[256] = {0};

    nimcp_rand_result_t result = nimcp_rand_bytes(buffer.data(), len);
    EXPECT_EQ(result, NIMCP_RAND_OK);

    for (size_t i = 0; i < len; i++) {
        counts[buffer[i]]++;
    }

    /* Expected count per byte: len/256 */
    double expected = static_cast<double>(len) / 256.0;

    /* Chi-squared test (simplified) */
    double chi_sq = 0.0;
    for (int i = 0; i < 256; i++) {
        double diff = counts[i] - expected;
        chi_sq += (diff * diff) / expected;
    }

    /* Critical value for df=255, alpha=0.01 is ~310 */
    EXPECT_LT(chi_sq, 350.0) << "Byte distribution appears non-uniform";
}

//=============================================================================
// Thread Safety Tests
//=============================================================================

TEST_F(RandTest, ThreadLocalSafety) {
    /**
     * WHAT: Verify thread-local RNG is thread-safe
     * WHY:  Multiple threads calling nimcp_rand_uniform concurrently must not crash
     * HOW:  Launch multiple threads, each generating random numbers
     */
    const int num_threads = 8;
    const int samples_per_thread = 10000;
    std::atomic<int> valid_count{0};
    std::atomic<int> invalid_count{0};

    auto thread_func = [&]() {
        for (int i = 0; i < samples_per_thread; i++) {
            float val = nimcp_rand_uniform();
            if (val >= 0.0f && val < 1.0f) {
                valid_count++;
            } else {
                invalid_count++;
            }
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; i++) {
        threads.emplace_back(thread_func);
    }

    for (auto& t : threads) {
        t.join();
    }

    int total = num_threads * samples_per_thread;
    EXPECT_EQ(valid_count.load(), total) << "All samples should be valid";
    EXPECT_EQ(invalid_count.load(), 0) << "No samples should be invalid";
}

TEST_F(RandTest, ThreadIsolation) {
    /**
     * WHAT: Verify threads have isolated RNG state
     * WHY:  Thread-local storage should prevent interference
     * HOW:  Seed threads identically, verify they can produce same sequences
     */
    const int num_samples = 100;
    std::mutex results_mutex;
    std::vector<std::vector<float>> thread_results;

    auto thread_func = [&](uint64_t seed) {
        nimcp_rand_seed(seed);
        std::vector<float> samples(num_samples);
        for (int i = 0; i < num_samples; i++) {
            samples[i] = nimcp_rand_uniform();
        }
        std::lock_guard<std::mutex> lock(results_mutex);
        thread_results.push_back(samples);
    };

    /* Launch 4 threads with same seed */
    const uint64_t common_seed = 12345;
    std::vector<std::thread> threads;
    for (int i = 0; i < 4; i++) {
        threads.emplace_back(thread_func, common_seed);
    }

    for (auto& t : threads) {
        t.join();
    }

    /* All threads should produce identical sequences (if properly isolated) */
    ASSERT_EQ(thread_results.size(), 4u);
    for (size_t t = 1; t < thread_results.size(); t++) {
        for (int i = 0; i < num_samples; i++) {
            EXPECT_EQ(thread_results[0][i], thread_results[t][i])
                << "Thread " << t << " differs at index " << i;
        }
    }
}

TEST_F(RandTest, ConcurrentContextAccess) {
    /**
     * WHAT: Verify contexts can be used from different threads safely
     * WHY:  Each context has its own state
     * HOW:  Create contexts per thread, verify independent operation
     */
    const int num_threads = 4;
    const int samples_per_thread = 1000;
    std::atomic<bool> all_valid{true};

    auto thread_func = [&](int thread_id) {
        /* Each thread gets its own context */
        nimcp_rand_ctx_t* ctx = nimcp_rand_ctx_create(static_cast<uint64_t>(thread_id));
        if (!ctx) {
            all_valid = false;
            return;
        }

        for (int i = 0; i < samples_per_thread; i++) {
            float val = nimcp_rand_ctx_uniform(ctx);
            if (val < 0.0f || val >= 1.0f) {
                all_valid = false;
            }
        }

        nimcp_rand_ctx_destroy(ctx);
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; i++) {
        threads.emplace_back(thread_func, i);
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_TRUE(all_valid.load()) << "All context operations should be valid";
}

//=============================================================================
// Statistics and Self-Test
//=============================================================================

TEST_F(RandTest, GetStatsWorks) {
    /**
     * WHAT: Verify statistics retrieval
     * WHY:  Users need to monitor RNG usage
     * HOW:  Generate samples, verify stats updated
     */
    nimcp_rand_reset_stats();

    /* Generate some samples */
    for (int i = 0; i < 100; i++) {
        nimcp_rand_uniform();
    }
    for (int i = 0; i < 50; i++) {
        nimcp_rand_normal(0.0f, 1.0f);
    }

    nimcp_rand_stats_t stats;
    nimcp_rand_get_stats(&stats);

    EXPECT_GE(stats.uniform_calls, 100u) << "Uniform calls should be tracked";
    EXPECT_GE(stats.normal_calls, 50u) << "Normal calls should be tracked";
}

TEST_F(RandTest, ResetStatsClearsCounters) {
    /**
     * WHAT: Verify stats reset
     * WHY:  Users may need to reset counters
     * HOW:  Generate samples, reset, verify cleared
     */
    for (int i = 0; i < 100; i++) {
        nimcp_rand_uniform();
    }

    nimcp_rand_reset_stats();

    nimcp_rand_stats_t stats;
    nimcp_rand_get_stats(&stats);

    EXPECT_EQ(stats.uniform_calls, 0u) << "Stats should be reset";
}

TEST_F(RandTest, BackendName) {
    /**
     * WHAT: Verify backend name retrieval
     * WHY:  Debugging and logging need backend info
     * HOW:  Get names for each backend type
     */
    const char* lcg_name = nimcp_rand_backend_name(NIMCP_RAND_BACKEND_LCG);
    EXPECT_NE(lcg_name, nullptr);
    EXPECT_GT(strlen(lcg_name), 0u);

    const char* pcg_name = nimcp_rand_backend_name(NIMCP_RAND_BACKEND_PCG);
    EXPECT_NE(pcg_name, nullptr);
}

TEST_F(RandTest, SelfTestPasses) {
    /**
     * WHAT: Verify built-in self-test passes
     * WHY:  Self-test validates RNG quality
     * HOW:  Run self-test, verify OK result
     */
    nimcp_rand_result_t result = nimcp_rand_self_test();
    EXPECT_EQ(result, NIMCP_RAND_OK) << "RNG self-test should pass";
}

//=============================================================================
// Edge Cases and Error Handling
//=============================================================================

TEST_F(RandTest, NullContextHandling) {
    /**
     * WHAT: Verify NULL context handling
     * WHY:  API should handle NULL gracefully
     * HOW:  Pass NULL to context functions
     */
    /* These should not crash, may return default values */
    nimcp_rand_ctx_destroy(nullptr);  /* Should be no-op */

    /* Clone of NULL should return NULL */
    nimcp_rand_ctx_t* clone = nimcp_rand_ctx_clone(nullptr);
    EXPECT_EQ(clone, nullptr);
}

TEST_F(RandTest, ZeroLengthArrays) {
    /**
     * WHAT: Verify zero-length array operations
     * WHY:  Edge case that should be handled gracefully
     * HOW:  Call with n=0
     */
    /* These should not crash */
    nimcp_rand_uniform_array(nullptr, 0);
    nimcp_rand_normal_array(nullptr, 0, 0.0f, 1.0f);
    nimcp_rand_shuffle_u32(nullptr, 0);
    nimcp_rand_shuffle(nullptr, 0, sizeof(int));
}

TEST_F(RandTest, ContextBytesNullBuffer) {
    /**
     * WHAT: Verify NULL buffer handling for bytes
     * WHY:  Should return error, not crash
     * HOW:  Pass NULL buffer to bytes functions
     */
    nimcp_rand_result_t result = nimcp_rand_bytes(nullptr, 100);
    EXPECT_EQ(result, NIMCP_RAND_ERROR_NULL);

    nimcp_rand_ctx_t* ctx = nimcp_rand_ctx_create(42);
    ASSERT_NE(ctx, nullptr);

    result = nimcp_rand_ctx_bytes(ctx, nullptr, 100);
    EXPECT_EQ(result, NIMCP_RAND_ERROR_NULL);

    nimcp_rand_ctx_destroy(ctx);
}

TEST_F(RandTest, SampleInvalidK) {
    /**
     * WHAT: Verify sample handles k > n
     * WHY:  Cannot sample more than population
     * HOW:  Request k > n, verify error
     */
    uint32_t out[10];
    nimcp_rand_result_t result = nimcp_rand_sample(5, 10, out);
    EXPECT_EQ(result, NIMCP_RAND_ERROR_INVALID);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
