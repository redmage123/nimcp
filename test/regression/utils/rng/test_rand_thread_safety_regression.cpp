/**
 * @file test_rand_thread_safety_regression.cpp
 * @brief Regression tests to prevent reintroduction of unsafe rand() usage
 *
 * WHAT: Thread safety regression tests for nimcp_rand module
 * WHY:  Ensure thread-safe RNG functions remain safe and correct
 * HOW:  Multi-threaded stress tests, statistical validation, determinism checks
 *
 * BUG HISTORY:
 * - Bug #1: Unsafe use of stdlib rand() in multithreaded code
 *   FIX: Replace with thread-local RNG state via nimcp_rand_*()
 * - Bug #2: Box-Muller cache corruption under concurrent access
 *   FIX: Thread-local Box-Muller cache per thread
 * - Bug #3: Statistics tracking not atomic
 *   FIX: Use atomic operations for stats counters
 *
 * REGRESSION FOCUS:
 * 1. Thread-local RNG must not have data races
 * 2. nimcp_rand_int() bounds must be respected under contention
 * 3. Box-Muller Gaussian output must be statistically correct
 * 4. Pink noise generation must be thread-safe
 * 5. Context-based RNG must produce deterministic sequences
 * 6. Statistics tracking must be atomic and accurate
 *
 * THREADSANITIZER COMPATIBILITY:
 * - Uses std::atomic for synchronization
 * - Uses std::thread (C++11 threads)
 * - Avoids data races by design
 *
 * @version 1.0.0
 * @date 2026-01-15
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <algorithm>
#include <numeric>
#include <atomic>
#include <thread>
#include <mutex>
#include <chrono>
#include <cstring>
#include <unordered_set>
#include <random>

extern "C" {
#include "utils/rng/nimcp_rand.h"
}

//=============================================================================
// Test Configuration
//=============================================================================

/** Number of threads for concurrent tests */
static constexpr uint32_t NUM_THREADS = 8;

/** Operations per thread for stress tests */
static constexpr uint32_t OPS_PER_THREAD = 10000;

/** Number of samples for statistical tests */
static constexpr uint32_t STAT_SAMPLES = 50000;

/** Tolerance for statistical tests */
static constexpr double STAT_TOLERANCE = 0.05;

//=============================================================================
// Test Utilities
//=============================================================================

/**
 * @brief Compute sample mean
 */
static double compute_mean(const std::vector<float>& samples) {
    if (samples.empty()) return 0.0;
    double sum = std::accumulate(samples.begin(), samples.end(), 0.0);
    return sum / static_cast<double>(samples.size());
}

/**
 * @brief Compute sample variance
 */
static double compute_variance(const std::vector<float>& samples, double mean) {
    if (samples.size() < 2) return 0.0;
    double sum_sq = 0.0;
    for (float s : samples) {
        double diff = static_cast<double>(s) - mean;
        sum_sq += diff * diff;
    }
    return sum_sq / static_cast<double>(samples.size() - 1);
}

/**
 * @brief Chi-squared test for uniformity
 * @return Test statistic (lower is better fit)
 */
static double chi_squared_uniformity(const std::vector<int32_t>& samples, int32_t max_val) {
    if (samples.empty() || max_val <= 0) return 999.0;

    /* Count occurrences in each bucket */
    std::vector<uint32_t> counts(static_cast<size_t>(max_val), 0);
    for (int32_t s : samples) {
        if (s >= 0 && s < max_val) {
            counts[static_cast<size_t>(s)]++;
        }
    }

    /* Expected count per bucket */
    double expected = static_cast<double>(samples.size()) / static_cast<double>(max_val);

    /* Compute chi-squared statistic */
    double chi_sq = 0.0;
    for (size_t i = 0; i < counts.size(); i++) {
        double diff = static_cast<double>(counts[i]) - expected;
        chi_sq += (diff * diff) / expected;
    }

    return chi_sq;
}

//=============================================================================
// Test Fixture
//=============================================================================

class RandThreadSafetyRegressionTest : public ::testing::Test {
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
};

//=============================================================================
// TEST 1: nimcp_rand_uniform() Thread Safety
//=============================================================================

/**
 * REGRESSION TEST: Uniform RNG must be thread-safe
 *
 * BUG: stdlib rand() is not thread-safe and causes data races
 * FIX: Use thread-local RNG state
 *
 * This test verifies no data races by running many threads concurrently
 * and checking that all outputs are valid (in [0,1)).
 */
TEST_F(RandThreadSafetyRegressionTest, UniformRNG_NoDataRaces) {
    std::atomic<uint32_t> ready_count{0};
    std::atomic<bool> start{false};
    std::atomic<uint64_t> invalid_count{0};
    std::atomic<uint64_t> total_samples{0};

    auto thread_func = [&](int thread_id) {
        /* Signal ready and wait for synchronized start */
        ready_count.fetch_add(1, std::memory_order_release);
        while (!start.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }

        /* Generate many uniform random numbers */
        uint64_t local_invalid = 0;
        for (uint32_t i = 0; i < OPS_PER_THREAD; i++) {
            float val = nimcp_rand_uniform();

            /* Check bounds - must be in [0, 1) */
            if (val < 0.0f || val >= 1.0f) {
                local_invalid++;
            }
        }

        invalid_count.fetch_add(local_invalid, std::memory_order_relaxed);
        total_samples.fetch_add(OPS_PER_THREAD, std::memory_order_relaxed);
    };

    /* Launch threads */
    std::vector<std::thread> threads;
    threads.reserve(NUM_THREADS);
    for (uint32_t i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back(thread_func, static_cast<int>(i));
    }

    /* Wait for all threads ready, then start */
    while (ready_count.load(std::memory_order_acquire) < NUM_THREADS) {
        std::this_thread::yield();
    }
    start.store(true, std::memory_order_release);

    /* Wait for completion */
    for (auto& t : threads) {
        t.join();
    }

    /* Verify results */
    EXPECT_EQ(invalid_count.load(), 0u)
        << "REGRESSION: " << invalid_count.load() << " invalid uniform values detected. "
        << "This may indicate a data race or incorrect implementation.";

    EXPECT_EQ(total_samples.load(), NUM_THREADS * OPS_PER_THREAD)
        << "Not all samples were generated";
}

/**
 * REGRESSION TEST: Uniform values must be strictly less than 1.0
 *
 * BUG: Incorrect divisor (UINT32_MAX instead of 1ULL << 32) can cause val >= 1.0
 * FIX: Divide by (1ULL << 32) = 4294967296
 */
TEST_F(RandThreadSafetyRegressionTest, UniformRNG_NeverReachesOne) {
    std::atomic<float> max_value{0.0f};
    std::atomic<uint64_t> ge_one_count{0};

    auto thread_func = [&](int thread_id) {
        float local_max = 0.0f;
        uint64_t local_ge_one = 0;

        for (uint32_t i = 0; i < OPS_PER_THREAD; i++) {
            float val = nimcp_rand_uniform();
            if (val > local_max) local_max = val;
            if (val >= 1.0f) local_ge_one++;
        }

        /* Atomically update global max */
        float old_max = max_value.load(std::memory_order_relaxed);
        while (local_max > old_max) {
            if (max_value.compare_exchange_weak(old_max, local_max,
                                                std::memory_order_relaxed)) {
                break;
            }
        }
        ge_one_count.fetch_add(local_ge_one, std::memory_order_relaxed);
    };

    std::vector<std::thread> threads;
    for (uint32_t i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back(thread_func, static_cast<int>(i));
    }
    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(ge_one_count.load(), 0u)
        << "REGRESSION: Uniform values >= 1.0 detected. "
        << "This indicates the divisor bug has been reintroduced.";

    float final_max = max_value.load();
    EXPECT_GT(final_max, 0.99f) << "Maximum value suspiciously low";
    EXPECT_LT(final_max, 1.0f) << "Maximum value reached 1.0";
}

//=============================================================================
// TEST 2: nimcp_rand_int() Bounds Under Contention
//=============================================================================

/**
 * REGRESSION TEST: Integer RNG bounds must be respected under contention
 *
 * BUG: Race conditions can cause out-of-bounds values
 * FIX: Thread-local state prevents races
 */
TEST_F(RandThreadSafetyRegressionTest, IntRNG_BoundsRespected) {
    const int32_t MAX_VAL = 100;
    std::atomic<uint64_t> out_of_bounds{0};
    std::atomic<uint64_t> total_ops{0};

    /* Use atomic histogram for thread-safe counting */
    std::vector<std::atomic<uint32_t>> histogram(MAX_VAL);
    for (auto& h : histogram) {
        h.store(0, std::memory_order_relaxed);
    }

    auto thread_func = [&](int thread_id) {
        uint64_t local_oob = 0;

        for (uint32_t i = 0; i < OPS_PER_THREAD; i++) {
            int32_t val = nimcp_rand_int(MAX_VAL);

            /* Check bounds: must be in [0, MAX_VAL) */
            if (val < 0 || val >= MAX_VAL) {
                local_oob++;
            } else {
                histogram[static_cast<size_t>(val)].fetch_add(1, std::memory_order_relaxed);
            }
        }

        out_of_bounds.fetch_add(local_oob, std::memory_order_relaxed);
        total_ops.fetch_add(OPS_PER_THREAD, std::memory_order_relaxed);
    };

    std::vector<std::thread> threads;
    for (uint32_t i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back(thread_func, static_cast<int>(i));
    }
    for (auto& t : threads) {
        t.join();
    }

    /* Verify no out-of-bounds */
    EXPECT_EQ(out_of_bounds.load(), 0u)
        << "REGRESSION: " << out_of_bounds.load() << " out-of-bounds values detected. "
        << "This indicates a thread safety issue in nimcp_rand_int().";

    /* Verify histogram sum matches total */
    uint64_t histogram_sum = 0;
    for (const auto& h : histogram) {
        histogram_sum += h.load(std::memory_order_relaxed);
    }
    EXPECT_EQ(histogram_sum, total_ops.load())
        << "Histogram sum mismatch - possible data race";

    /* Verify distribution is roughly uniform (chi-squared test) */
    double expected = static_cast<double>(histogram_sum) / static_cast<double>(MAX_VAL);
    double chi_sq = 0.0;
    for (const auto& h : histogram) {
        double diff = static_cast<double>(h.load(std::memory_order_relaxed)) - expected;
        chi_sq += (diff * diff) / expected;
    }

    /* Chi-squared critical value for df=99 at alpha=0.01 is ~135 */
    EXPECT_LT(chi_sq, 150.0)
        << "Distribution appears non-uniform (chi-squared = " << chi_sq << ")";
}

//=============================================================================
// TEST 3: nimcp_rand_normal() Statistical Properties Under Contention
//=============================================================================

/**
 * REGRESSION TEST: Gaussian output must have correct statistics under contention
 *
 * BUG: Box-Muller cache shared between threads causes incorrect distribution
 * FIX: Thread-local Box-Muller cache
 */
TEST_F(RandThreadSafetyRegressionTest, NormalRNG_StatisticalProperties) {
    const float EXPECTED_MEAN = 0.0f;
    const float EXPECTED_STDDEV = 1.0f;
    const uint32_t SAMPLES_PER_THREAD = STAT_SAMPLES / NUM_THREADS;

    std::mutex samples_mutex;
    std::vector<float> all_samples;
    all_samples.reserve(STAT_SAMPLES);

    std::atomic<uint64_t> nan_count{0};
    std::atomic<uint64_t> inf_count{0};

    auto thread_func = [&](int thread_id) {
        std::vector<float> local_samples;
        local_samples.reserve(SAMPLES_PER_THREAD);
        uint64_t local_nan = 0;
        uint64_t local_inf = 0;

        for (uint32_t i = 0; i < SAMPLES_PER_THREAD; i++) {
            float val = nimcp_rand_normal(EXPECTED_MEAN, EXPECTED_STDDEV);

            if (std::isnan(val)) {
                local_nan++;
            } else if (std::isinf(val)) {
                local_inf++;
            } else {
                local_samples.push_back(val);
            }
        }

        /* Merge into global samples */
        {
            std::lock_guard<std::mutex> lock(samples_mutex);
            all_samples.insert(all_samples.end(),
                               local_samples.begin(), local_samples.end());
        }

        nan_count.fetch_add(local_nan, std::memory_order_relaxed);
        inf_count.fetch_add(local_inf, std::memory_order_relaxed);
    };

    std::vector<std::thread> threads;
    for (uint32_t i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back(thread_func, static_cast<int>(i));
    }
    for (auto& t : threads) {
        t.join();
    }

    /* Verify no NaN or Inf values */
    EXPECT_EQ(nan_count.load(), 0u)
        << "REGRESSION: Box-Muller produced NaN. This may indicate divisor bug.";
    EXPECT_EQ(inf_count.load(), 0u)
        << "REGRESSION: Box-Muller produced Inf. This may indicate log(0).";

    /* Verify mean is close to expected */
    double actual_mean = compute_mean(all_samples);
    double std_error = EXPECTED_STDDEV / std::sqrt(static_cast<double>(all_samples.size()));
    EXPECT_NEAR(actual_mean, EXPECTED_MEAN, 4.0 * std_error)
        << "REGRESSION: Gaussian mean is incorrect. "
        << "Actual: " << actual_mean << ", Expected: " << EXPECTED_MEAN;

    /* Verify stddev is close to expected */
    double actual_variance = compute_variance(all_samples, actual_mean);
    double actual_stddev = std::sqrt(actual_variance);
    EXPECT_NEAR(actual_stddev, EXPECTED_STDDEV, EXPECTED_STDDEV * STAT_TOLERANCE)
        << "REGRESSION: Gaussian stddev is incorrect. "
        << "Actual: " << actual_stddev << ", Expected: " << EXPECTED_STDDEV;
}

/**
 * REGRESSION TEST: Consecutive Gaussian samples must be independent
 *
 * BUG: Incorrect Box-Muller implementation can produce correlated pairs
 * FIX: Both Box-Muller outputs are independent Gaussians
 */
TEST_F(RandThreadSafetyRegressionTest, NormalRNG_IndependentSamples) {
    const uint32_t NUM_PAIRS = 10000;
    std::vector<float> first_vals, second_vals;
    first_vals.reserve(NUM_PAIRS);
    second_vals.reserve(NUM_PAIRS);

    for (uint32_t i = 0; i < NUM_PAIRS; i++) {
        first_vals.push_back(nimcp_rand_normal(0.0f, 1.0f));
        second_vals.push_back(nimcp_rand_normal(0.0f, 1.0f));
    }

    /* Compute correlation coefficient */
    double mean1 = compute_mean(first_vals);
    double mean2 = compute_mean(second_vals);

    double sum_xy = 0.0, sum_x2 = 0.0, sum_y2 = 0.0;
    for (size_t i = 0; i < NUM_PAIRS; i++) {
        double dx = first_vals[i] - mean1;
        double dy = second_vals[i] - mean2;
        sum_xy += dx * dy;
        sum_x2 += dx * dx;
        sum_y2 += dy * dy;
    }

    double correlation = sum_xy / std::sqrt(sum_x2 * sum_y2);

    /* Correlation should be close to 0 */
    EXPECT_NEAR(correlation, 0.0, 0.05)
        << "REGRESSION: Consecutive Gaussian samples are correlated. "
        << "Correlation = " << correlation;
}

//=============================================================================
// TEST 4: Box-Muller Cache Thread Safety
//=============================================================================

/**
 * REGRESSION TEST: Box-Muller cache must not cause issues with concurrent threads
 *
 * BUG: Shared cache between threads causes race conditions
 * FIX: Thread-local cache ensures each thread has its own state
 *
 * Test strategy: Run many threads generating Gaussians simultaneously.
 * If cache is shared, we'll see statistical anomalies or crashes.
 */
TEST_F(RandThreadSafetyRegressionTest, BoxMullerCache_ThreadSafety) {
    std::atomic<uint32_t> ready_count{0};
    std::atomic<bool> start{false};
    std::atomic<uint64_t> anomaly_count{0};

    auto thread_func = [&](int thread_id) {
        ready_count.fetch_add(1, std::memory_order_release);
        while (!start.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }

        /* Generate pairs of Gaussians and check they're reasonable */
        uint64_t local_anomalies = 0;
        for (uint32_t i = 0; i < OPS_PER_THREAD; i++) {
            float g1 = nimcp_rand_normal(0.0f, 1.0f);
            float g2 = nimcp_rand_normal(0.0f, 1.0f);

            /* Check for anomalies (values way outside expected range) */
            /* For N(0,1), 99.99% of values should be within [-4, 4] */
            if (std::isnan(g1) || std::isnan(g2) ||
                std::isinf(g1) || std::isinf(g2) ||
                std::abs(g1) > 10.0f || std::abs(g2) > 10.0f) {
                local_anomalies++;
            }
        }

        anomaly_count.fetch_add(local_anomalies, std::memory_order_relaxed);
    };

    std::vector<std::thread> threads;
    for (uint32_t i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back(thread_func, static_cast<int>(i));
    }

    while (ready_count.load(std::memory_order_acquire) < NUM_THREADS) {
        std::this_thread::yield();
    }
    start.store(true, std::memory_order_release);

    for (auto& t : threads) {
        t.join();
    }

    /* Allow some statistical outliers (|Z| > 10 happens ~1e-23 times) */
    /* But if we see many, the cache is broken */
    uint64_t total_samples = NUM_THREADS * OPS_PER_THREAD * 2;
    double anomaly_rate = static_cast<double>(anomaly_count.load()) /
                          static_cast<double>(total_samples);

    EXPECT_LT(anomaly_rate, 0.0001)
        << "REGRESSION: Too many anomalous Gaussian values (" << anomaly_count.load()
        << " of " << total_samples << "). This may indicate Box-Muller cache corruption.";
}

//=============================================================================
// TEST 5: Pink Noise Thread Safety
//=============================================================================

/**
 * REGRESSION TEST: Pink noise generation must be thread-safe
 *
 * BUG: Shared Voss-McCartney state causes data races
 * FIX: Thread-local pink noise generators
 */
TEST_F(RandThreadSafetyRegressionTest, PinkNoise_ThreadSafety) {
    std::atomic<uint64_t> out_of_range{0};
    std::atomic<uint64_t> total_samples{0};

    auto thread_func = [&](int thread_id) {
        uint64_t local_oor = 0;

        for (uint32_t i = 0; i < OPS_PER_THREAD; i++) {
            float val = nimcp_rand_pink();

            /* Pink noise should be in [-1, 1] range */
            if (std::isnan(val) || std::isinf(val) ||
                val < -2.0f || val > 2.0f) {  /* Allow some margin */
                local_oor++;
            }
        }

        out_of_range.fetch_add(local_oor, std::memory_order_relaxed);
        total_samples.fetch_add(OPS_PER_THREAD, std::memory_order_relaxed);
    };

    std::vector<std::thread> threads;
    for (uint32_t i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back(thread_func, static_cast<int>(i));
    }
    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(out_of_range.load(), 0u)
        << "REGRESSION: " << out_of_range.load()
        << " pink noise values out of expected range. "
        << "This may indicate thread safety issues.";
}

/**
 * REGRESSION TEST: Pink noise should have correct spectral characteristics
 *
 * Tests that mean is close to 0 and variance is reasonable.
 */
TEST_F(RandThreadSafetyRegressionTest, PinkNoise_StatisticalProperties) {
    std::vector<float> samples;
    samples.reserve(STAT_SAMPLES);

    for (uint32_t i = 0; i < STAT_SAMPLES; i++) {
        samples.push_back(nimcp_rand_pink());
    }

    double mean = compute_mean(samples);
    double variance = compute_variance(samples, mean);

    /* Pink noise should have mean close to 0 */
    /* Note: Pink noise is correlated by design, so mean can vary more than white noise */
    /* Threshold of 0.15 catches real bugs while allowing statistical variance */
    EXPECT_NEAR(mean, 0.0, 0.15)
        << "Pink noise mean is too far from 0: " << mean;

    /* Variance should be reasonable (not too high or low) */
    EXPECT_GT(variance, 0.01) << "Pink noise variance too low: " << variance;
    EXPECT_LT(variance, 1.0) << "Pink noise variance too high: " << variance;
}

//=============================================================================
// TEST 6: Deterministic Sequences with Fixed Seeds (Reproducibility)
//=============================================================================

/**
 * REGRESSION TEST: Same seed must produce identical sequences
 *
 * BUG: Thread-local state not properly seeded
 * FIX: nimcp_rand_seed() properly initializes thread-local state
 */
TEST_F(RandThreadSafetyRegressionTest, Reproducibility_SameSeedSameSequence) {
    const uint64_t TEST_SEED = 12345;
    const uint32_t SEQUENCE_LENGTH = 1000;

    /* Generate first sequence */
    nimcp_rand_seed(TEST_SEED);
    std::vector<float> seq1;
    seq1.reserve(SEQUENCE_LENGTH);
    for (uint32_t i = 0; i < SEQUENCE_LENGTH; i++) {
        seq1.push_back(nimcp_rand_uniform());
    }

    /* Reseed and generate second sequence */
    nimcp_rand_seed(TEST_SEED);
    std::vector<float> seq2;
    seq2.reserve(SEQUENCE_LENGTH);
    for (uint32_t i = 0; i < SEQUENCE_LENGTH; i++) {
        seq2.push_back(nimcp_rand_uniform());
    }

    /* Sequences must be identical */
    ASSERT_EQ(seq1.size(), seq2.size());
    for (uint32_t i = 0; i < SEQUENCE_LENGTH; i++) {
        EXPECT_EQ(seq1[i], seq2[i])
            << "REGRESSION: Sequences differ at index " << i
            << ". Same seed must produce same sequence.";
    }
}

/**
 * REGRESSION TEST: Context-based RNG produces deterministic sequences
 *
 * Verifies that nimcp_rand_ctx_* functions are deterministic.
 */
TEST_F(RandThreadSafetyRegressionTest, Reproducibility_ContextDeterminism) {
    const uint64_t TEST_SEED = 67890;
    const uint32_t SEQUENCE_LENGTH = 1000;

    /* Create first context and generate sequence */
    nimcp_rand_ctx_t* ctx1 = nimcp_rand_ctx_create(TEST_SEED);
    ASSERT_NE(ctx1, nullptr) << "Failed to create context";

    std::vector<float> seq1;
    seq1.reserve(SEQUENCE_LENGTH);
    for (uint32_t i = 0; i < SEQUENCE_LENGTH; i++) {
        seq1.push_back(nimcp_rand_ctx_uniform(ctx1));
    }

    nimcp_rand_ctx_destroy(ctx1);

    /* Create second context with same seed */
    nimcp_rand_ctx_t* ctx2 = nimcp_rand_ctx_create(TEST_SEED);
    ASSERT_NE(ctx2, nullptr) << "Failed to create context";

    std::vector<float> seq2;
    seq2.reserve(SEQUENCE_LENGTH);
    for (uint32_t i = 0; i < SEQUENCE_LENGTH; i++) {
        seq2.push_back(nimcp_rand_ctx_uniform(ctx2));
    }

    nimcp_rand_ctx_destroy(ctx2);

    /* Sequences must be identical */
    ASSERT_EQ(seq1.size(), seq2.size());
    for (uint32_t i = 0; i < SEQUENCE_LENGTH; i++) {
        EXPECT_EQ(seq1[i], seq2[i])
            << "REGRESSION: Context sequences differ at index " << i;
    }
}

/**
 * REGRESSION TEST: Different seeds produce different sequences
 */
TEST_F(RandThreadSafetyRegressionTest, Reproducibility_DifferentSeedsDifferentSequences) {
    const uint32_t SEQUENCE_LENGTH = 100;

    nimcp_rand_ctx_t* ctx1 = nimcp_rand_ctx_create(11111);
    nimcp_rand_ctx_t* ctx2 = nimcp_rand_ctx_create(22222);
    ASSERT_NE(ctx1, nullptr);
    ASSERT_NE(ctx2, nullptr);

    uint32_t differences = 0;
    for (uint32_t i = 0; i < SEQUENCE_LENGTH; i++) {
        float v1 = nimcp_rand_ctx_uniform(ctx1);
        float v2 = nimcp_rand_ctx_uniform(ctx2);
        if (v1 != v2) differences++;
    }

    nimcp_rand_ctx_destroy(ctx1);
    nimcp_rand_ctx_destroy(ctx2);

    /* Most values should be different */
    EXPECT_GT(differences, SEQUENCE_LENGTH / 2)
        << "Different seeds should produce different sequences";
}

//=============================================================================
// TEST 7: Statistics Tracking Atomicity
//=============================================================================

/**
 * REGRESSION TEST: Statistics tracking must be atomic and accurate
 *
 * BUG: Non-atomic statistics counters cause lost updates
 * FIX: Use atomic operations for all statistics
 */
TEST_F(RandThreadSafetyRegressionTest, Statistics_AtomicAndAccurate) {
    /* Reset statistics */
    nimcp_rand_reset_stats();

    std::atomic<uint32_t> ready_count{0};
    std::atomic<bool> start{false};

    const uint32_t UNIFORM_OPS = 1000;
    const uint32_t NORMAL_OPS = 500;
    const uint32_t INT_OPS = 800;

    auto thread_func = [&](int thread_id) {
        ready_count.fetch_add(1, std::memory_order_release);
        while (!start.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }

        /* Generate different types of random numbers */
        for (uint32_t i = 0; i < UNIFORM_OPS; i++) {
            nimcp_rand_uniform();
        }
        for (uint32_t i = 0; i < NORMAL_OPS; i++) {
            nimcp_rand_normal(0.0f, 1.0f);
        }
        for (uint32_t i = 0; i < INT_OPS; i++) {
            nimcp_rand_int(100);
        }
    };

    std::vector<std::thread> threads;
    for (uint32_t i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back(thread_func, static_cast<int>(i));
    }

    while (ready_count.load(std::memory_order_acquire) < NUM_THREADS) {
        std::this_thread::yield();
    }
    start.store(true, std::memory_order_release);

    for (auto& t : threads) {
        t.join();
    }

    /* Get statistics */
    nimcp_rand_stats_t stats;
    nimcp_rand_get_stats(&stats);

    /* Verify counts match expected */
    uint64_t expected_uniform = NUM_THREADS * UNIFORM_OPS;
    uint64_t expected_normal = NUM_THREADS * NORMAL_OPS;
    uint64_t expected_int = NUM_THREADS * INT_OPS;

    /* Use >= because internal operations (Box-Muller, thread init) make extra calls */
    /* The key test is that counts are >= expected (atomicity preserved) */
    EXPECT_GE(stats.uniform_calls, expected_uniform)
        << "REGRESSION: Uniform call count too low. Expected >= " << expected_uniform
        << ", got " << stats.uniform_calls << ". Statistics may not be atomic.";

    EXPECT_GE(stats.normal_calls, expected_normal)
        << "REGRESSION: Normal call count too low. Expected >= " << expected_normal
        << ", got " << stats.normal_calls << ". Statistics may not be atomic.";

    EXPECT_GE(stats.int_calls, expected_int)
        << "REGRESSION: Int call count too low. Expected >= " << expected_int
        << ", got " << stats.int_calls << ". Statistics may not be atomic.";
}

//=============================================================================
// TEST 8: Context Create/Destroy Thread Safety
//=============================================================================

/**
 * REGRESSION TEST: Context creation and destruction must be thread-safe
 *
 * Tests that concurrent context lifecycle operations don't cause races.
 */
TEST_F(RandThreadSafetyRegressionTest, Context_CreateDestroyThreadSafe) {
    std::atomic<uint64_t> success_count{0};
    std::atomic<uint64_t> failure_count{0};

    auto thread_func = [&](int thread_id) {
        for (uint32_t i = 0; i < 100; i++) {
            /* Create context with unique seed per thread/iteration */
            uint64_t seed = static_cast<uint64_t>(thread_id) * 1000 + i;
            nimcp_rand_ctx_t* ctx = nimcp_rand_ctx_create(seed);

            if (ctx == nullptr) {
                failure_count.fetch_add(1, std::memory_order_relaxed);
                continue;
            }

            /* Use context */
            for (int j = 0; j < 100; j++) {
                nimcp_rand_ctx_uniform(ctx);
            }

            /* Destroy context */
            nimcp_rand_ctx_destroy(ctx);
            success_count.fetch_add(1, std::memory_order_relaxed);
        }
    };

    std::vector<std::thread> threads;
    for (uint32_t i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back(thread_func, static_cast<int>(i));
    }
    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(failure_count.load(), 0u)
        << "REGRESSION: " << failure_count.load()
        << " context creation failures. May indicate thread safety issue.";

    EXPECT_EQ(success_count.load(), NUM_THREADS * 100)
        << "Not all context operations completed";
}

//=============================================================================
// TEST 9: nimcp_rand_range() Bounds Under Contention
//=============================================================================

/**
 * REGRESSION TEST: Range RNG bounds must be respected
 */
TEST_F(RandThreadSafetyRegressionTest, RangeRNG_BoundsRespected) {
    const int32_t MIN_VAL = -50;
    const int32_t MAX_VAL = 50;
    std::atomic<uint64_t> out_of_bounds{0};

    auto thread_func = [&](int thread_id) {
        uint64_t local_oob = 0;

        for (uint32_t i = 0; i < OPS_PER_THREAD; i++) {
            int32_t val = nimcp_rand_range(MIN_VAL, MAX_VAL);

            /* Check bounds: must be in [MIN_VAL, MAX_VAL] (inclusive) */
            if (val < MIN_VAL || val > MAX_VAL) {
                local_oob++;
            }
        }

        out_of_bounds.fetch_add(local_oob, std::memory_order_relaxed);
    };

    std::vector<std::thread> threads;
    for (uint32_t i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back(thread_func, static_cast<int>(i));
    }
    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(out_of_bounds.load(), 0u)
        << "REGRESSION: " << out_of_bounds.load()
        << " out-of-bounds range values detected.";
}

//=============================================================================
// TEST 10: Array Operations Thread Safety
//=============================================================================

/**
 * REGRESSION TEST: Array fill operations must be thread-safe
 */
TEST_F(RandThreadSafetyRegressionTest, ArrayOperations_ThreadSafe) {
    const size_t ARRAY_SIZE = 1000;

    auto thread_func = [&](int thread_id) {
        std::vector<float> uniform_array(ARRAY_SIZE);
        std::vector<float> normal_array(ARRAY_SIZE);

        /* Fill arrays */
        nimcp_rand_uniform_array(uniform_array.data(), ARRAY_SIZE);
        nimcp_rand_normal_array(normal_array.data(), ARRAY_SIZE, 0.0f, 1.0f);

        /* Verify all values are valid */
        for (size_t i = 0; i < ARRAY_SIZE; i++) {
            EXPECT_GE(uniform_array[i], 0.0f);
            EXPECT_LT(uniform_array[i], 1.0f);
            EXPECT_FALSE(std::isnan(normal_array[i]));
            EXPECT_FALSE(std::isinf(normal_array[i]));
        }
    };

    std::vector<std::thread> threads;
    for (uint32_t i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back(thread_func, static_cast<int>(i));
    }
    for (auto& t : threads) {
        t.join();
    }
}

//=============================================================================
// TEST 11: Shuffle Thread Safety
//=============================================================================

/**
 * REGRESSION TEST: Fisher-Yates shuffle must be thread-safe
 */
TEST_F(RandThreadSafetyRegressionTest, Shuffle_ThreadSafe) {
    auto thread_func = [&](int thread_id) {
        const size_t ARRAY_SIZE = 100;
        std::vector<uint32_t> array(ARRAY_SIZE);

        /* Initialize array with 0, 1, 2, ..., 99 */
        for (size_t i = 0; i < ARRAY_SIZE; i++) {
            array[i] = static_cast<uint32_t>(i);
        }

        /* Shuffle */
        nimcp_rand_shuffle_u32(array.data(), ARRAY_SIZE);

        /* Verify it's a permutation (all values still present) */
        std::vector<bool> seen(ARRAY_SIZE, false);
        for (size_t i = 0; i < ARRAY_SIZE; i++) {
            ASSERT_LT(array[i], static_cast<uint32_t>(ARRAY_SIZE))
                << "Shuffle produced out-of-range value";
            EXPECT_FALSE(seen[array[i]])
                << "Shuffle produced duplicate value at index " << i;
            seen[array[i]] = true;
        }

        /* Verify all values present */
        for (size_t i = 0; i < ARRAY_SIZE; i++) {
            EXPECT_TRUE(seen[i]) << "Value " << i << " missing after shuffle";
        }
    };

    std::vector<std::thread> threads;
    for (uint32_t i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back(thread_func, static_cast<int>(i));
    }
    for (auto& t : threads) {
        t.join();
    }
}

//=============================================================================
// TEST 12: Exponential Distribution Thread Safety
//=============================================================================

/**
 * REGRESSION TEST: Exponential RNG must be thread-safe
 */
TEST_F(RandThreadSafetyRegressionTest, ExponentialRNG_ThreadSafety) {
    const float RATE = 2.0f;  /* Expected mean = 1/RATE = 0.5 */
    std::mutex samples_mutex;
    std::vector<float> all_samples;
    const uint32_t SAMPLES_PER_THREAD = STAT_SAMPLES / NUM_THREADS;

    std::atomic<uint64_t> invalid_count{0};

    auto thread_func = [&](int thread_id) {
        std::vector<float> local_samples;
        local_samples.reserve(SAMPLES_PER_THREAD);
        uint64_t local_invalid = 0;

        for (uint32_t i = 0; i < SAMPLES_PER_THREAD; i++) {
            float val = nimcp_rand_exponential(RATE);

            /* Exponential values must be >= 0 */
            if (std::isnan(val) || std::isinf(val) || val < 0.0f) {
                local_invalid++;
            } else {
                local_samples.push_back(val);
            }
        }

        {
            std::lock_guard<std::mutex> lock(samples_mutex);
            all_samples.insert(all_samples.end(),
                               local_samples.begin(), local_samples.end());
        }
        invalid_count.fetch_add(local_invalid, std::memory_order_relaxed);
    };

    std::vector<std::thread> threads;
    for (uint32_t i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back(thread_func, static_cast<int>(i));
    }
    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(invalid_count.load(), 0u)
        << "REGRESSION: " << invalid_count.load()
        << " invalid exponential values detected.";

    /* Verify mean is close to 1/RATE */
    double actual_mean = compute_mean(all_samples);
    double expected_mean = 1.0 / static_cast<double>(RATE);

    EXPECT_NEAR(actual_mean, expected_mean, expected_mean * STAT_TOLERANCE)
        << "Exponential mean incorrect. Actual: " << actual_mean
        << ", Expected: " << expected_mean;
}

//=============================================================================
// TEST 13: nimcp_rand_bytes() Thread Safety
//=============================================================================

/**
 * REGRESSION TEST: Byte generation must be thread-safe
 */
TEST_F(RandThreadSafetyRegressionTest, BytesRNG_ThreadSafety) {
    const size_t BUFFER_SIZE = 256;
    std::atomic<uint64_t> failure_count{0};

    auto thread_func = [&](int thread_id) {
        uint8_t buffer[BUFFER_SIZE];
        memset(buffer, 0, BUFFER_SIZE);

        for (uint32_t i = 0; i < 100; i++) {
            nimcp_rand_result_t result = nimcp_rand_bytes(buffer, BUFFER_SIZE);
            if (result != NIMCP_RAND_OK) {
                failure_count.fetch_add(1, std::memory_order_relaxed);
            }
        }
    };

    std::vector<std::thread> threads;
    for (uint32_t i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back(thread_func, static_cast<int>(i));
    }
    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(failure_count.load(), 0u)
        << "REGRESSION: " << failure_count.load()
        << " nimcp_rand_bytes() failures detected.";
}

//=============================================================================
// TEST 14: Weighted Choice Thread Safety
//=============================================================================

/**
 * REGRESSION TEST: Weighted choice must be thread-safe
 */
TEST_F(RandThreadSafetyRegressionTest, WeightedChoice_ThreadSafety) {
    const uint32_t NUM_CHOICES = 5;
    const float weights[NUM_CHOICES] = {0.1f, 0.2f, 0.3f, 0.2f, 0.2f};

    std::atomic<uint64_t> out_of_bounds{0};
    std::vector<std::atomic<uint64_t>> choice_counts(NUM_CHOICES);
    for (auto& c : choice_counts) {
        c.store(0, std::memory_order_relaxed);
    }

    auto thread_func = [&](int thread_id) {
        uint64_t local_oob = 0;

        for (uint32_t i = 0; i < OPS_PER_THREAD; i++) {
            uint32_t choice = nimcp_rand_choice(weights, NUM_CHOICES);
            if (choice >= NUM_CHOICES) {
                local_oob++;
            } else {
                choice_counts[choice].fetch_add(1, std::memory_order_relaxed);
            }
        }

        out_of_bounds.fetch_add(local_oob, std::memory_order_relaxed);
    };

    std::vector<std::thread> threads;
    for (uint32_t i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back(thread_func, static_cast<int>(i));
    }
    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(out_of_bounds.load(), 0u)
        << "REGRESSION: " << out_of_bounds.load()
        << " out-of-bounds weighted choices detected.";

    /* Verify distribution roughly matches weights */
    uint64_t total = NUM_THREADS * OPS_PER_THREAD;
    for (uint32_t i = 0; i < NUM_CHOICES; i++) {
        double expected_ratio = static_cast<double>(weights[i]);
        double actual_ratio = static_cast<double>(choice_counts[i].load()) /
                              static_cast<double>(total);

        EXPECT_NEAR(actual_ratio, expected_ratio, expected_ratio * 0.15)
            << "Weighted choice " << i << " has unexpected frequency. "
            << "Expected ratio: " << expected_ratio
            << ", Actual: " << actual_ratio;
    }
}

//=============================================================================
// TEST 15: Sample Without Replacement Thread Safety
//=============================================================================

/**
 * REGRESSION TEST: Sampling without replacement must be thread-safe
 */
TEST_F(RandThreadSafetyRegressionTest, SampleWithoutReplacement_ThreadSafety) {
    const uint32_t POPULATION = 100;
    const uint32_t SAMPLE_SIZE = 20;

    auto thread_func = [&](int thread_id) {
        std::vector<uint32_t> sample(SAMPLE_SIZE);

        for (uint32_t iter = 0; iter < 100; iter++) {
            nimcp_rand_result_t result = nimcp_rand_sample(POPULATION, SAMPLE_SIZE,
                                                           sample.data());
            ASSERT_EQ(result, NIMCP_RAND_OK) << "Sample failed";

            /* Verify all values are in range */
            for (uint32_t i = 0; i < SAMPLE_SIZE; i++) {
                ASSERT_LT(sample[i], POPULATION)
                    << "Sample value out of range";
            }

            /* Verify no duplicates */
            std::unordered_set<uint32_t> unique_vals(sample.begin(), sample.end());
            EXPECT_EQ(unique_vals.size(), SAMPLE_SIZE)
                << "Sample contains duplicates";
        }
    };

    std::vector<std::thread> threads;
    for (uint32_t i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back(thread_func, static_cast<int>(i));
    }
    for (auto& t : threads) {
        t.join();
    }
}

//=============================================================================
// MAIN
//=============================================================================

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
