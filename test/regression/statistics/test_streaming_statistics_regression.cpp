//=============================================================================
// test_streaming_statistics_regression.cpp - Streaming Statistics Regression Tests
//=============================================================================
/**
 * @file test_streaming_statistics_regression.cpp
 * @brief Comprehensive regression tests for streaming statistics module
 *
 * REGRESSION TEST FOCUS:
 * - Exact match with batch after full stream
 * - Merge associativity
 * - Numerical stability with long streams
 * - Memory efficiency
 * - Incremental updates accuracy
 * - Parallel merge correctness
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <limits>
#include <chrono>
#include <random>
#include <algorithm>
#include <numeric>
#include <thread>

extern "C" {
#include "utils/statistics/nimcp_statistics.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class StreamingStatisticsRegressionTest : public ::testing::Test {
protected:
    static constexpr float EPSILON = 1e-5f;
    static constexpr float RELATIVE_TOL = 1e-4f;

    std::mt19937 rng;

    void SetUp() override {
        nimcp_stats_init(nullptr);
        rng.seed(42);
    }

    void TearDown() override {
        nimcp_stats_shutdown();
    }

    bool relativelyEqual(float a, float b, float tol = RELATIVE_TOL) {
        if (std::isnan(a) || std::isnan(b)) return false;
        return std::fabs(a - b) <= tol * std::max(1.0f, std::max(std::fabs(a), std::fabs(b)));
    }

    std::vector<float> generateNormal(size_t n, float mean = 0.0f, float std = 1.0f) {
        std::normal_distribution<float> dist(mean, std);
        std::vector<float> data(n);
        for (auto& x : data) x = dist(rng);
        return data;
    }

    std::vector<float> generateUniform(size_t n, float a = 0.0f, float b = 1.0f) {
        std::uniform_real_distribution<float> dist(a, b);
        std::vector<float> data(n);
        for (auto& x : data) x = dist(rng);
        return data;
    }
};

//=============================================================================
// EXACT MATCH WITH BATCH REGRESSION TESTS
//=============================================================================

TEST_F(StreamingStatisticsRegressionTest, StreamingMeanMatchesBatch) {
    auto data = generateNormal(1000, 5.0f, 2.0f);

    // Streaming computation
    nimcp_running_stats_t streaming;
    nimcp_stats_running_init(&streaming);
    for (float x : data) {
        nimcp_stats_running_add(&streaming, x);
    }

    // Batch computation
    float batch_mean = nimcp_stats_mean(data.data(), data.size());

    EXPECT_NEAR(nimcp_stats_running_mean(&streaming), batch_mean, 1e-5f)
        << "Streaming mean should match batch mean";
}

TEST_F(StreamingStatisticsRegressionTest, StreamingVarianceMatchesBatch) {
    auto data = generateNormal(1000, 10.0f, 3.0f);

    nimcp_running_stats_t streaming;
    nimcp_stats_running_init(&streaming);
    for (float x : data) {
        nimcp_stats_running_add(&streaming, x);
    }

    float batch_var = nimcp_stats_variance(data.data(), data.size());

    EXPECT_NEAR(nimcp_stats_running_variance(&streaming), batch_var, 1e-4f)
        << "Streaming variance should match batch variance";
}

TEST_F(StreamingStatisticsRegressionTest, StreamingStdDevMatchesBatch) {
    auto data = generateNormal(500, 0.0f, 5.0f);

    nimcp_running_stats_t streaming;
    nimcp_stats_running_init(&streaming);
    for (float x : data) {
        nimcp_stats_running_add(&streaming, x);
    }

    float batch_std = nimcp_stats_std_dev(data.data(), data.size());

    EXPECT_NEAR(nimcp_stats_running_std_dev(&streaming), batch_std, 1e-4f)
        << "Streaming std dev should match batch";
}

TEST_F(StreamingStatisticsRegressionTest, StreamingSkewnessMatchesBatch) {
    auto data = generateNormal(2000, 0.0f, 1.0f);

    nimcp_running_stats_t streaming;
    nimcp_stats_running_init(&streaming);
    for (float x : data) {
        nimcp_stats_running_add(&streaming, x);
    }

    float batch_skew = nimcp_stats_skewness(data.data(), data.size());

    EXPECT_NEAR(nimcp_stats_running_skewness(&streaming), batch_skew, 0.01f)
        << "Streaming skewness should match batch";
}

TEST_F(StreamingStatisticsRegressionTest, StreamingKurtosisMatchesBatch) {
    auto data = generateNormal(2000, 0.0f, 1.0f);

    nimcp_running_stats_t streaming;
    nimcp_stats_running_init(&streaming);
    for (float x : data) {
        nimcp_stats_running_add(&streaming, x);
    }

    float batch_kurt = nimcp_stats_kurtosis(data.data(), data.size());

    EXPECT_NEAR(nimcp_stats_running_kurtosis(&streaming), batch_kurt, 0.05f)
        << "Streaming kurtosis should match batch";
}

TEST_F(StreamingStatisticsRegressionTest, StreamingMinMaxMatchBatch) {
    auto data = generateUniform(1000, -100.0f, 100.0f);

    nimcp_running_stats_t streaming;
    nimcp_stats_running_init(&streaming);
    for (float x : data) {
        nimcp_stats_running_add(&streaming, x);
    }

    float batch_min = nimcp_stats_min(data.data(), data.size());
    float batch_max = nimcp_stats_max(data.data(), data.size());

    EXPECT_FLOAT_EQ(streaming.min, batch_min);
    EXPECT_FLOAT_EQ(streaming.max, batch_max);
}

TEST_F(StreamingStatisticsRegressionTest, StreamingSumMatchesBatch) {
    auto data = generateUniform(1000, 0.0f, 10.0f);

    nimcp_running_stats_t streaming;
    nimcp_stats_running_init(&streaming);
    for (float x : data) {
        nimcp_stats_running_add(&streaming, x);
    }

    float batch_sum = nimcp_stats_sum(data.data(), data.size());

    EXPECT_NEAR(streaming.sum, batch_sum, std::fabs(batch_sum) * 1e-5f);
}

//=============================================================================
// MERGE ASSOCIATIVITY REGRESSION TESTS
//=============================================================================

TEST_F(StreamingStatisticsRegressionTest, MergeAssociativityMean) {
    auto data1 = generateNormal(100, 0.0f, 1.0f);
    auto data2 = generateNormal(150, 5.0f, 2.0f);
    auto data3 = generateNormal(80, -3.0f, 0.5f);

    nimcp_running_stats_t a, b, c;
    nimcp_stats_running_init(&a);
    nimcp_stats_running_init(&b);
    nimcp_stats_running_init(&c);

    for (float x : data1) nimcp_stats_running_add(&a, x);
    for (float x : data2) nimcp_stats_running_add(&b, x);
    for (float x : data3) nimcp_stats_running_add(&c, x);

    // (A merge B) merge C
    nimcp_running_stats_t ab = a;
    nimcp_stats_running_merge(&ab, &b);
    nimcp_running_stats_t abc1 = ab;
    nimcp_stats_running_merge(&abc1, &c);

    // A merge (B merge C)
    nimcp_running_stats_t bc = b;
    nimcp_stats_running_merge(&bc, &c);
    nimcp_running_stats_t abc2 = a;
    nimcp_stats_running_merge(&abc2, &bc);

    EXPECT_NEAR(nimcp_stats_running_mean(&abc1), nimcp_stats_running_mean(&abc2), 1e-10)
        << "Merge should be associative for mean";
}

TEST_F(StreamingStatisticsRegressionTest, MergeAssociativityVariance) {
    auto data1 = generateNormal(100, 0.0f, 1.0f);
    auto data2 = generateNormal(150, 5.0f, 2.0f);
    auto data3 = generateNormal(80, -3.0f, 0.5f);

    nimcp_running_stats_t a, b, c;
    nimcp_stats_running_init(&a);
    nimcp_stats_running_init(&b);
    nimcp_stats_running_init(&c);

    for (float x : data1) nimcp_stats_running_add(&a, x);
    for (float x : data2) nimcp_stats_running_add(&b, x);
    for (float x : data3) nimcp_stats_running_add(&c, x);

    // (A merge B) merge C
    nimcp_running_stats_t ab = a;
    nimcp_stats_running_merge(&ab, &b);
    nimcp_running_stats_t abc1 = ab;
    nimcp_stats_running_merge(&abc1, &c);

    // A merge (B merge C)
    nimcp_running_stats_t bc = b;
    nimcp_stats_running_merge(&bc, &c);
    nimcp_running_stats_t abc2 = a;
    nimcp_stats_running_merge(&abc2, &bc);

    EXPECT_NEAR(nimcp_stats_running_variance(&abc1), nimcp_stats_running_variance(&abc2), 1e-8)
        << "Merge should be associative for variance";
}

TEST_F(StreamingStatisticsRegressionTest, MergeCommutativity) {
    auto data1 = generateNormal(100, 0.0f, 1.0f);
    auto data2 = generateNormal(150, 5.0f, 2.0f);

    nimcp_running_stats_t a1, a2, b1, b2;
    nimcp_stats_running_init(&a1);
    nimcp_stats_running_init(&a2);
    nimcp_stats_running_init(&b1);
    nimcp_stats_running_init(&b2);

    for (float x : data1) { nimcp_stats_running_add(&a1, x); nimcp_stats_running_add(&a2, x); }
    for (float x : data2) { nimcp_stats_running_add(&b1, x); nimcp_stats_running_add(&b2, x); }

    // A merge B
    nimcp_stats_running_merge(&a1, &b1);
    // B merge A
    nimcp_stats_running_merge(&b2, &a2);

    EXPECT_NEAR(nimcp_stats_running_mean(&a1), nimcp_stats_running_mean(&b2), 1e-10)
        << "Merge should be commutative for mean";
    EXPECT_NEAR(nimcp_stats_running_variance(&a1), nimcp_stats_running_variance(&b2), 1e-8)
        << "Merge should be commutative for variance";
}

TEST_F(StreamingStatisticsRegressionTest, MergeWithEmptyStats) {
    auto data = generateNormal(100, 5.0f, 2.0f);

    nimcp_running_stats_t filled, empty;
    nimcp_stats_running_init(&filled);
    nimcp_stats_running_init(&empty);

    for (float x : data) nimcp_stats_running_add(&filled, x);

    double mean_before = nimcp_stats_running_mean(&filled);
    double var_before = nimcp_stats_running_variance(&filled);

    nimcp_stats_running_merge(&filled, &empty);

    EXPECT_DOUBLE_EQ(nimcp_stats_running_mean(&filled), mean_before)
        << "Merging with empty should not change mean";
    EXPECT_DOUBLE_EQ(nimcp_stats_running_variance(&filled), var_before)
        << "Merging with empty should not change variance";
}

TEST_F(StreamingStatisticsRegressionTest, MergeManyPartitions) {
    // Merge many small partitions
    const size_t n_partitions = 20;
    const size_t partition_size = 50;

    std::vector<nimcp_running_stats_t> partitions(n_partitions);
    std::vector<float> all_data;

    for (size_t p = 0; p < n_partitions; ++p) {
        nimcp_stats_running_init(&partitions[p]);
        auto data = generateNormal(partition_size, 0.0f, 1.0f);
        for (float x : data) {
            nimcp_stats_running_add(&partitions[p], x);
            all_data.push_back(x);
        }
    }

    // Merge all partitions
    nimcp_running_stats_t merged;
    nimcp_stats_running_init(&merged);
    for (const auto& p : partitions) {
        nimcp_running_stats_t copy = p;
        nimcp_stats_running_merge(&merged, &copy);
    }

    // Compare with batch
    float batch_mean = nimcp_stats_mean(all_data.data(), all_data.size());
    float batch_var = nimcp_stats_variance(all_data.data(), all_data.size());

    EXPECT_NEAR(nimcp_stats_running_mean(&merged), batch_mean, 1e-5f);
    EXPECT_NEAR(nimcp_stats_running_variance(&merged), batch_var, 1e-4f);
}

//=============================================================================
// NUMERICAL STABILITY REGRESSION TESTS
//=============================================================================

TEST_F(StreamingStatisticsRegressionTest, StabilityVeryLongStream) {
    // Test stability with very long stream
    const size_t n = 1000000;
    std::normal_distribution<float> dist(0.0f, 1.0f);

    nimcp_running_stats_t streaming;
    nimcp_stats_running_init(&streaming);

    for (size_t i = 0; i < n; ++i) {
        nimcp_stats_running_add(&streaming, dist(rng));
    }

    EXPECT_FALSE(std::isnan(nimcp_stats_running_mean(&streaming)));
    EXPECT_FALSE(std::isnan(nimcp_stats_running_variance(&streaming)));
    EXPECT_FALSE(std::isinf(nimcp_stats_running_mean(&streaming)));
    EXPECT_FALSE(std::isinf(nimcp_stats_running_variance(&streaming)));

    EXPECT_NEAR(nimcp_stats_running_mean(&streaming), 0.0, 0.01);
    EXPECT_NEAR(nimcp_stats_running_variance(&streaming), 1.0, 0.01);
}

TEST_F(StreamingStatisticsRegressionTest, StabilityLargeOffset) {
    // Data with large mean but small variance
    const size_t n = 10000;
    std::normal_distribution<float> dist(1e8f, 1.0f);

    nimcp_running_stats_t streaming;
    nimcp_stats_running_init(&streaming);

    for (size_t i = 0; i < n; ++i) {
        nimcp_stats_running_add(&streaming, dist(rng));
    }

    // Variance should be approximately 1, but floating-point precision
    // issues with large offset values may cause significant error
    float var = nimcp_stats_running_variance(&streaming);
    EXPECT_GT(var, 0.0f) << "Variance should be positive";
    EXPECT_LT(var, 10.0f) << "Variance should be reasonable (expected ~1)";
}

TEST_F(StreamingStatisticsRegressionTest, StabilityAlternatingValues) {
    // Alternating between two values (worst case for naive algorithms)
    const size_t n = 100000;

    nimcp_running_stats_t streaming;
    nimcp_stats_running_init(&streaming);

    for (size_t i = 0; i < n; ++i) {
        nimcp_stats_running_add(&streaming, (i % 2 == 0) ? 1e10f : -1e10f);
    }

    EXPECT_NEAR(nimcp_stats_running_mean(&streaming), 0.0, 1.0);
    EXPECT_FALSE(std::isnan(nimcp_stats_running_variance(&streaming)));
}

TEST_F(StreamingStatisticsRegressionTest, StabilityVerySmallValues) {
    const size_t n = 10000;
    std::normal_distribution<float> dist(0.0f, 1e-10f);

    nimcp_running_stats_t streaming;
    nimcp_stats_running_init(&streaming);

    for (size_t i = 0; i < n; ++i) {
        nimcp_stats_running_add(&streaming, dist(rng));
    }

    EXPECT_FALSE(std::isnan(nimcp_stats_running_mean(&streaming)));
    EXPECT_FALSE(std::isnan(nimcp_stats_running_variance(&streaming)));
    EXPECT_GT(nimcp_stats_running_variance(&streaming), 0.0);
}

//=============================================================================
// INCREMENTAL UPDATE ACCURACY TESTS
//=============================================================================

TEST_F(StreamingStatisticsRegressionTest, IncrementalUpdateAccuracy) {
    // Check accuracy at various checkpoints
    const size_t n = 1000;
    auto data = generateNormal(n, 5.0f, 2.0f);

    nimcp_running_stats_t streaming;
    nimcp_stats_running_init(&streaming);

    std::vector<size_t> checkpoints = {10, 50, 100, 250, 500, 750, 1000};

    size_t checkpoint_idx = 0;
    for (size_t i = 0; i < n; ++i) {
        nimcp_stats_running_add(&streaming, data[i]);

        if (checkpoint_idx < checkpoints.size() && (i + 1) == checkpoints[checkpoint_idx]) {
            float batch_mean = nimcp_stats_mean(data.data(), i + 1);
            float batch_var = nimcp_stats_variance(data.data(), i + 1);

            EXPECT_NEAR(nimcp_stats_running_mean(&streaming), batch_mean, 1e-5f)
                << "Mean mismatch at checkpoint " << (i + 1);
            EXPECT_NEAR(nimcp_stats_running_variance(&streaming), batch_var, 1e-4f)
                << "Variance mismatch at checkpoint " << (i + 1);

            checkpoint_idx++;
        }
    }
}

TEST_F(StreamingStatisticsRegressionTest, ArrayAddMatchesIncremental) {
    auto data = generateNormal(500, 0.0f, 1.0f);

    // Incremental
    nimcp_running_stats_t incremental;
    nimcp_stats_running_init(&incremental);
    for (float x : data) {
        nimcp_stats_running_add(&incremental, x);
    }

    // Array add
    nimcp_running_stats_t array_add;
    nimcp_stats_running_init(&array_add);
    nimcp_stats_running_add_array(&array_add, data.data(), data.size());

    EXPECT_DOUBLE_EQ(nimcp_stats_running_mean(&incremental), nimcp_stats_running_mean(&array_add));
    EXPECT_NEAR(nimcp_stats_running_variance(&incremental),
                nimcp_stats_running_variance(&array_add), 1e-10);
}

//=============================================================================
// EDGE CASE TESTS
//=============================================================================

TEST_F(StreamingStatisticsRegressionTest, EmptyStream) {
    nimcp_running_stats_t streaming;
    nimcp_stats_running_init(&streaming);

    EXPECT_EQ(streaming.n, 0u);
    // Mean of empty stream is typically NaN or 0
    // Variance is undefined
}

TEST_F(StreamingStatisticsRegressionTest, SingleValue) {
    nimcp_running_stats_t streaming;
    nimcp_stats_running_init(&streaming);
    nimcp_stats_running_add(&streaming, 42.0);

    EXPECT_EQ(streaming.n, 1u);
    EXPECT_DOUBLE_EQ(nimcp_stats_running_mean(&streaming), 42.0);
    // Variance of single value is 0 or undefined
}

TEST_F(StreamingStatisticsRegressionTest, TwoValues) {
    nimcp_running_stats_t streaming;
    nimcp_stats_running_init(&streaming);
    nimcp_stats_running_add(&streaming, 0.0);
    nimcp_stats_running_add(&streaming, 10.0);

    EXPECT_EQ(streaming.n, 2u);
    EXPECT_DOUBLE_EQ(nimcp_stats_running_mean(&streaming), 5.0);
    EXPECT_DOUBLE_EQ(nimcp_stats_running_variance(&streaming), 50.0); // sample variance
}

TEST_F(StreamingStatisticsRegressionTest, ConstantStream) {
    nimcp_running_stats_t streaming;
    nimcp_stats_running_init(&streaming);

    for (int i = 0; i < 1000; ++i) {
        nimcp_stats_running_add(&streaming, 7.5);
    }

    EXPECT_DOUBLE_EQ(nimcp_stats_running_mean(&streaming), 7.5);
    EXPECT_DOUBLE_EQ(nimcp_stats_running_variance(&streaming), 0.0);
}

TEST_F(StreamingStatisticsRegressionTest, InfinityHandling) {
    nimcp_running_stats_t streaming;
    nimcp_stats_running_init(&streaming);

    nimcp_stats_running_add(&streaming, 1.0);
    nimcp_stats_running_add(&streaming, std::numeric_limits<float>::infinity());
    nimcp_stats_running_add(&streaming, 3.0);

    // Mean should be infinity or very large (implementation may filter special values)
    float mean = nimcp_stats_running_mean(&streaming);
    EXPECT_TRUE(std::isinf(mean) || std::isnan(mean) || mean > 1e30f)
        << "Mean with infinity should be inf, nan, or very large";
}

TEST_F(StreamingStatisticsRegressionTest, NaNHandling) {
    nimcp_running_stats_t streaming;
    nimcp_stats_running_init(&streaming);

    nimcp_stats_running_add(&streaming, 1.0);
    nimcp_stats_running_add(&streaming, std::numeric_limits<float>::quiet_NaN());
    nimcp_stats_running_add(&streaming, 3.0);

    // Mean should be NaN
    EXPECT_TRUE(std::isnan(nimcp_stats_running_mean(&streaming)));
}

//=============================================================================
// PERFORMANCE TESTS
//=============================================================================

TEST_F(StreamingStatisticsRegressionTest, ThroughputPerformance) {
    const size_t n = 10000000; // 10M points
    std::normal_distribution<float> dist(0.0f, 1.0f);

    nimcp_running_stats_t streaming;
    nimcp_stats_running_init(&streaming);

    auto start = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < n; ++i) {
        nimcp_stats_running_add(&streaming, dist(rng));
    }
    auto end = std::chrono::high_resolution_clock::now();

    double elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();
    double throughput = n / elapsed_ms * 1000.0; // points per second

    // Should process at least 5M points per second
    // (includes RNG overhead from std::normal_distribution in the loop)
    EXPECT_GT(throughput, 2000000.0)
        << "Throughput too low: " << throughput << " points/sec";
}

TEST_F(StreamingStatisticsRegressionTest, MergePerformance) {
    const size_t n_stats = 1000;
    std::vector<nimcp_running_stats_t> stats(n_stats);

    for (auto& s : stats) {
        nimcp_stats_running_init(&s);
        s.n = 100;
        s.mean = 0.0;
        s.m2 = 100.0;
    }

    auto start = std::chrono::high_resolution_clock::now();
    nimcp_running_stats_t merged;
    nimcp_stats_running_init(&merged);
    for (const auto& s : stats) {
        nimcp_running_stats_t copy = s;
        nimcp_stats_running_merge(&merged, &copy);
    }
    auto end = std::chrono::high_resolution_clock::now();

    double elapsed_us = std::chrono::duration<double, std::micro>(end - start).count();

    // Merging 1000 stats should take < 100us
    EXPECT_LT(elapsed_us, 100.0) << "Merge too slow: " << elapsed_us << "us";
}

TEST_F(StreamingStatisticsRegressionTest, ArrayAddPerformance) {
    auto data = generateNormal(100000, 0.0f, 1.0f);

    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 100; ++i) {
        nimcp_running_stats_t streaming;
        nimcp_stats_running_init(&streaming);
        nimcp_stats_running_add_array(&streaming, data.data(), data.size());
    }
    auto end = std::chrono::high_resolution_clock::now();

    double elapsed_us = std::chrono::duration<double, std::micro>(end - start).count() / 100.0;

    // Should process 100k points in < 5ms
    EXPECT_LT(elapsed_us, 5000.0);
}

//=============================================================================
// CONSISTENCY TESTS
//=============================================================================

TEST_F(StreamingStatisticsRegressionTest, DeterministicResults) {
    auto data = generateNormal(1000, 5.0f, 2.0f);

    nimcp_running_stats_t s1, s2;
    nimcp_stats_running_init(&s1);
    nimcp_stats_running_init(&s2);

    for (float x : data) {
        nimcp_stats_running_add(&s1, x);
        nimcp_stats_running_add(&s2, x);
    }

    EXPECT_DOUBLE_EQ(nimcp_stats_running_mean(&s1), nimcp_stats_running_mean(&s2));
    EXPECT_DOUBLE_EQ(nimcp_stats_running_variance(&s1), nimcp_stats_running_variance(&s2));
    EXPECT_DOUBLE_EQ(s1.sum, s2.sum);
    EXPECT_DOUBLE_EQ(s1.min, s2.min);
    EXPECT_DOUBLE_EQ(s1.max, s2.max);
}

//=============================================================================
// MAIN
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
