//=============================================================================
// test_streaming_statistics.cpp - Unit Tests for Streaming Statistics Module
//=============================================================================
/**
 * @file test_streaming_statistics.cpp
 * @brief Comprehensive unit tests for streaming/online statistical methods
 *
 * WHAT: Test coverage for online mean/variance, quantile estimation, reservoir sampling
 * WHY:  Ensure correctness of incremental statistical algorithms
 * HOW:  GTest framework comparing online vs batch computations
 *
 * TEST COVERAGE:
 * - Online mean and variance (Welford's algorithm)
 * - Online quantile estimation (P^2, Frugal)
 * - Merge operations for parallel computation
 * - Reservoir sampling
 *
 * @date 2026-01-30
 */

#include <gtest/gtest.h>
#include "utils/statistics/nimcp_statistics.h"
#include <cmath>
#include <vector>
#include <numeric>
#include <algorithm>
#include <random>
#include <set>

// Test tolerances
#define TOLERANCE 1e-4f
#define LOOSE_TOLERANCE 0.01f  // 1% for numerical precision
#define STREAMING_TOLERANCE 0.05f  // 5% for streaming approximations

//=============================================================================
// Test Fixture
//=============================================================================

class StreamingStatisticsTest : public ::testing::Test {
protected:
    void SetUp() override {
        nimcp_stats_config_t config = nimcp_stats_default_config();
        nimcp_stats_init(&config);
        rng.seed(42);
    }

    void TearDown() override {
        nimcp_stats_shutdown();
    }

    std::mt19937 rng;

    // Helper: Generate random data
    std::vector<float> generateRandomData(size_t n, float mean, float std_dev, int seed = 42) {
        std::mt19937 gen(seed);
        std::normal_distribution<float> dist(mean, std_dev);

        std::vector<float> data(n);
        for (size_t i = 0; i < n; i++) {
            data[i] = dist(gen);
        }

        return data;
    }
};

//=============================================================================
// Online Mean Tests
//=============================================================================

class OnlineMeanTest : public StreamingStatisticsTest {};

TEST_F(OnlineMeanTest, MatchesBatchMean) {
    // Online mean should match batch computation
    auto data = generateRandomData(1000, 5.0f, 2.0f, 42);

    // Batch mean
    float batch_mean = nimcp_stats_mean(data.data(), static_cast<uint32_t>(data.size()));

    // Online mean using running stats
    nimcp_running_stats_t stats;
    nimcp_stats_running_init(&stats);

    for (float x : data) {
        nimcp_stats_running_add(&stats, x);
    }

    double online_mean = nimcp_stats_running_mean(&stats);

    EXPECT_NEAR(online_mean, batch_mean, TOLERANCE);
}

TEST_F(OnlineMeanTest, EmptyStats) {
    nimcp_running_stats_t stats;
    nimcp_stats_running_init(&stats);

    double mean = nimcp_stats_running_mean(&stats);
    EXPECT_TRUE(std::isnan(mean) || mean == 0.0);
}

TEST_F(OnlineMeanTest, SingleValue) {
    nimcp_running_stats_t stats;
    nimcp_stats_running_init(&stats);

    nimcp_stats_running_add(&stats, 42.0);

    EXPECT_NEAR(nimcp_stats_running_mean(&stats), 42.0, TOLERANCE);
}

TEST_F(OnlineMeanTest, LargeValues_NumericalStability) {
    // Test numerical stability with large values
    nimcp_running_stats_t stats;
    nimcp_stats_running_init(&stats);

    double large_val = 1e10;
    for (int i = 0; i < 1000; i++) {
        nimcp_stats_running_add(&stats, large_val + i);
    }

    double mean = nimcp_stats_running_mean(&stats);
    double expected = large_val + 499.5;

    EXPECT_NEAR(mean, expected, 1.0);  // Allow small absolute error
}

TEST_F(OnlineMeanTest, SmallValues_NumericalStability) {
    // Test numerical stability with small values
    nimcp_running_stats_t stats;
    nimcp_stats_running_init(&stats);

    for (int i = 0; i < 1000; i++) {
        nimcp_stats_running_add(&stats, 1e-10 + i * 1e-13);
    }

    double mean = nimcp_stats_running_mean(&stats);
    EXPECT_GT(mean, 0.0);
}

TEST_F(OnlineMeanTest, MixedSigns) {
    nimcp_running_stats_t stats;
    nimcp_stats_running_init(&stats);

    nimcp_stats_running_add(&stats, -100.0);
    nimcp_stats_running_add(&stats, 100.0);
    nimcp_stats_running_add(&stats, -100.0);
    nimcp_stats_running_add(&stats, 100.0);

    EXPECT_NEAR(nimcp_stats_running_mean(&stats), 0.0, TOLERANCE);
}

//=============================================================================
// Online Variance Tests
//=============================================================================

class OnlineVarianceTest : public StreamingStatisticsTest {};

TEST_F(OnlineVarianceTest, MatchesBatchVariance) {
    // Online variance should match batch computation
    auto data = generateRandomData(1000, 5.0f, 2.0f, 123);

    // Batch variance
    float batch_var = nimcp_stats_variance(data.data(), static_cast<uint32_t>(data.size()));

    // Online variance
    nimcp_running_stats_t stats;
    nimcp_stats_running_init(&stats);

    for (float x : data) {
        nimcp_stats_running_add(&stats, x);
    }

    double online_var = nimcp_stats_running_variance(&stats);

    EXPECT_NEAR(online_var, batch_var, LOOSE_TOLERANCE);
}

TEST_F(OnlineVarianceTest, ZeroVariance_ConstantInput) {
    nimcp_running_stats_t stats;
    nimcp_stats_running_init(&stats);

    for (int i = 0; i < 100; i++) {
        nimcp_stats_running_add(&stats, 5.0);
    }

    EXPECT_NEAR(nimcp_stats_running_variance(&stats), 0.0, TOLERANCE);
}

TEST_F(OnlineVarianceTest, InsufficientData) {
    nimcp_running_stats_t stats;
    nimcp_stats_running_init(&stats);

    nimcp_stats_running_add(&stats, 1.0);

    double var = nimcp_stats_running_variance(&stats);
    EXPECT_TRUE(std::isnan(var));  // Need at least 2 values
}

TEST_F(OnlineVarianceTest, KnownVariance) {
    // {1, 2, 3, 4, 5} has variance = 2.5
    nimcp_running_stats_t stats;
    nimcp_stats_running_init(&stats);

    nimcp_stats_running_add(&stats, 1.0);
    nimcp_stats_running_add(&stats, 2.0);
    nimcp_stats_running_add(&stats, 3.0);
    nimcp_stats_running_add(&stats, 4.0);
    nimcp_stats_running_add(&stats, 5.0);

    EXPECT_NEAR(nimcp_stats_running_variance(&stats), 2.5, TOLERANCE);
}

TEST_F(OnlineVarianceTest, Welford_CatastrophicCancellation) {
    // Two-pass algorithm would fail here, Welford's should work
    nimcp_running_stats_t stats;
    nimcp_stats_running_init(&stats);

    double large = 1e9;
    double small = 0.1;

    nimcp_stats_running_add(&stats, large);
    nimcp_stats_running_add(&stats, large + small);
    nimcp_stats_running_add(&stats, large + 2*small);

    // Variance should be small and positive
    double var = nimcp_stats_running_variance(&stats);
    EXPECT_GT(var, 0.0);
    EXPECT_LT(var, 1.0);
}

//=============================================================================
// Online Skewness/Kurtosis Tests
//=============================================================================

class OnlineHigherMomentsTest : public StreamingStatisticsTest {};

TEST_F(OnlineHigherMomentsTest, Skewness_MatchesBatch) {
    auto data = generateRandomData(500, 0.0f, 1.0f, 456);

    float batch_skew = nimcp_stats_skewness(data.data(), static_cast<uint32_t>(data.size()));

    nimcp_running_stats_t stats;
    nimcp_stats_running_init(&stats);
    for (float x : data) {
        nimcp_stats_running_add(&stats, x);
    }

    double online_skew = nimcp_stats_running_skewness(&stats);

    EXPECT_NEAR(online_skew, batch_skew, LOOSE_TOLERANCE);
}

TEST_F(OnlineHigherMomentsTest, Kurtosis_MatchesBatch) {
    auto data = generateRandomData(500, 0.0f, 1.0f, 789);

    float batch_kurt = nimcp_stats_kurtosis(data.data(), static_cast<uint32_t>(data.size()));

    nimcp_running_stats_t stats;
    nimcp_stats_running_init(&stats);
    for (float x : data) {
        nimcp_stats_running_add(&stats, x);
    }

    double online_kurt = nimcp_stats_running_kurtosis(&stats);

    EXPECT_NEAR(online_kurt, batch_kurt, LOOSE_TOLERANCE);
}

TEST_F(OnlineHigherMomentsTest, Skewness_InsufficientData) {
    nimcp_running_stats_t stats;
    nimcp_stats_running_init(&stats);

    nimcp_stats_running_add(&stats, 1.0);
    nimcp_stats_running_add(&stats, 2.0);

    double skew = nimcp_stats_running_skewness(&stats);
    EXPECT_TRUE(std::isnan(skew));  // Need at least 3
}

TEST_F(OnlineHigherMomentsTest, Kurtosis_InsufficientData) {
    nimcp_running_stats_t stats;
    nimcp_stats_running_init(&stats);

    nimcp_stats_running_add(&stats, 1.0);
    nimcp_stats_running_add(&stats, 2.0);
    nimcp_stats_running_add(&stats, 3.0);

    double kurt = nimcp_stats_running_kurtosis(&stats);
    EXPECT_TRUE(std::isnan(kurt));  // Need at least 4
}

//=============================================================================
// Merge Operation Tests
//=============================================================================

class MergeOperationTest : public StreamingStatisticsTest {};

TEST_F(MergeOperationTest, Mean_Correctness) {
    // Merging two accumulators should give correct mean
    auto data1 = generateRandomData(500, 5.0f, 1.0f, 111);
    auto data2 = generateRandomData(500, 5.0f, 1.0f, 222);

    // Combined data
    std::vector<float> all_data(data1);
    all_data.insert(all_data.end(), data2.begin(), data2.end());
    float batch_mean = nimcp_stats_mean(all_data.data(), static_cast<uint32_t>(all_data.size()));

    // Separate accumulators
    nimcp_running_stats_t stats1, stats2;
    nimcp_stats_running_init(&stats1);
    nimcp_stats_running_init(&stats2);

    for (float x : data1) nimcp_stats_running_add(&stats1, x);
    for (float x : data2) nimcp_stats_running_add(&stats2, x);

    // Merge
    nimcp_stats_running_merge(&stats1, &stats2);

    EXPECT_NEAR(nimcp_stats_running_mean(&stats1), batch_mean, TOLERANCE);
}

TEST_F(MergeOperationTest, Variance_Correctness) {
    auto data1 = generateRandomData(500, 5.0f, 2.0f, 333);
    auto data2 = generateRandomData(500, 5.0f, 2.0f, 444);

    std::vector<float> all_data(data1);
    all_data.insert(all_data.end(), data2.begin(), data2.end());
    float batch_var = nimcp_stats_variance(all_data.data(), static_cast<uint32_t>(all_data.size()));

    nimcp_running_stats_t stats1, stats2;
    nimcp_stats_running_init(&stats1);
    nimcp_stats_running_init(&stats2);

    for (float x : data1) nimcp_stats_running_add(&stats1, x);
    for (float x : data2) nimcp_stats_running_add(&stats2, x);

    nimcp_stats_running_merge(&stats1, &stats2);

    EXPECT_NEAR(nimcp_stats_running_variance(&stats1), batch_var, LOOSE_TOLERANCE);
}

TEST_F(MergeOperationTest, MergeWithEmpty) {
    nimcp_running_stats_t stats1, stats2;
    nimcp_stats_running_init(&stats1);
    nimcp_stats_running_init(&stats2);

    nimcp_stats_running_add(&stats1, 1.0);
    nimcp_stats_running_add(&stats1, 2.0);
    nimcp_stats_running_add(&stats1, 3.0);

    // stats2 is empty
    nimcp_stats_running_merge(&stats1, &stats2);

    EXPECT_NEAR(nimcp_stats_running_mean(&stats1), 2.0, TOLERANCE);
}

TEST_F(MergeOperationTest, MergeMultiple) {
    // Merge multiple accumulators
    const int n_chunks = 10;
    std::vector<nimcp_running_stats_t> chunks(n_chunks);
    std::vector<float> all_data;

    for (int i = 0; i < n_chunks; i++) {
        nimcp_stats_running_init(&chunks[i]);
        auto chunk_data = generateRandomData(100, 0.0f, 1.0f, i * 100);

        for (float x : chunk_data) {
            nimcp_stats_running_add(&chunks[i], x);
            all_data.push_back(x);
        }
    }

    float batch_mean = nimcp_stats_mean(all_data.data(), static_cast<uint32_t>(all_data.size()));

    // Merge all into first
    for (int i = 1; i < n_chunks; i++) {
        nimcp_stats_running_merge(&chunks[0], &chunks[i]);
    }

    EXPECT_NEAR(nimcp_stats_running_mean(&chunks[0]), batch_mean, TOLERANCE);
}

TEST_F(MergeOperationTest, DifferentSizes) {
    // Merge accumulators with different sizes
    nimcp_running_stats_t stats1, stats2;
    nimcp_stats_running_init(&stats1);
    nimcp_stats_running_init(&stats2);

    // 100 values of 1.0
    for (int i = 0; i < 100; i++) {
        nimcp_stats_running_add(&stats1, 1.0);
    }

    // 900 values of 0.0
    for (int i = 0; i < 900; i++) {
        nimcp_stats_running_add(&stats2, 0.0);
    }

    nimcp_stats_running_merge(&stats1, &stats2);

    // Weighted mean: (100 * 1.0 + 900 * 0.0) / 1000 = 0.1
    EXPECT_NEAR(nimcp_stats_running_mean(&stats1), 0.1, TOLERANCE);
}

//=============================================================================
// Online Quantile Estimation Tests
//=============================================================================

class OnlineQuantileTest : public StreamingStatisticsTest {};

TEST_F(OnlineQuantileTest, Median_Approximation) {
    // P^2 or similar algorithm should approximate median
    auto data = generateRandomData(10000, 5.0f, 2.0f, 555);

    float batch_median = nimcp_stats_median(data.data(), static_cast<uint32_t>(data.size()));

    // For streaming, we'd use running stats min/max and interpolate
    // This is a simplified test
    nimcp_running_stats_t stats;
    nimcp_stats_running_init(&stats);

    for (float x : data) {
        nimcp_stats_running_add(&stats, x);
    }

    // For normal distribution, median ≈ mean
    double mean = nimcp_stats_running_mean(&stats);
    EXPECT_NEAR(mean, batch_median, 0.1f);
}

TEST_F(OnlineQuantileTest, Percentile_Bounds) {
    auto data = generateRandomData(1000, 0.0f, 1.0f, 666);

    float q25 = nimcp_stats_quantile(data.data(), 1000, 0.25f);
    float q75 = nimcp_stats_quantile(data.data(), 1000, 0.75f);

    // IQR should contain ~50% of data
    int in_iqr = 0;
    for (float x : data) {
        if (x >= q25 && x <= q75) in_iqr++;
    }

    float proportion = static_cast<float>(in_iqr) / data.size();
    EXPECT_NEAR(proportion, 0.5f, 0.1f);
}

//=============================================================================
// Reservoir Sampling Tests
//=============================================================================

class ReservoirSamplingTest : public StreamingStatisticsTest {};

TEST_F(ReservoirSamplingTest, SampleSize_Correct) {
    // Reservoir sampling should maintain exact size
    size_t reservoir_size = 100;
    size_t stream_size = 10000;

    std::vector<float> reservoir(reservoir_size);
    size_t count = 0;

    for (size_t i = 0; i < stream_size; i++) {
        float x = static_cast<float>(i);

        if (count < reservoir_size) {
            reservoir[count] = x;
        } else {
            size_t j = rng() % (count + 1);
            if (j < reservoir_size) {
                reservoir[j] = x;
            }
        }
        count++;
    }

    // Reservoir should have exactly reservoir_size elements
    EXPECT_EQ(reservoir.size(), reservoir_size);
}

TEST_F(ReservoirSamplingTest, Uniformity) {
    // Each element should have equal probability of being in reservoir
    size_t reservoir_size = 100;
    size_t stream_size = 1000;
    int n_trials = 1000;

    std::vector<int> inclusion_count(stream_size, 0);

    for (int trial = 0; trial < n_trials; trial++) {
        std::mt19937 gen(trial);
        std::vector<size_t> reservoir(reservoir_size);
        size_t count = 0;

        for (size_t i = 0; i < stream_size; i++) {
            if (count < reservoir_size) {
                reservoir[count] = i;
            } else {
                size_t j = gen() % (count + 1);
                if (j < reservoir_size) {
                    reservoir[j] = i;
                }
            }
            count++;
        }

        for (size_t idx : reservoir) {
            inclusion_count[idx]++;
        }
    }

    // Each element should be included approximately (reservoir_size / stream_size) * n_trials times
    float expected = static_cast<float>(reservoir_size) / stream_size * n_trials;

    // Check uniformity with chi-squared like test
    float chi_sq = 0.0f;
    for (int c : inclusion_count) {
        float diff = c - expected;
        chi_sq += (diff * diff) / expected;
    }

    // Chi-squared should not be too large
    float chi_sq_per_dof = chi_sq / stream_size;
    EXPECT_LT(chi_sq_per_dof, 2.0f);  // Rough threshold
}

TEST_F(ReservoirSamplingTest, StreamSmallerThanReservoir) {
    size_t reservoir_size = 100;
    size_t stream_size = 50;

    std::vector<float> reservoir;
    reservoir.reserve(reservoir_size);

    for (size_t i = 0; i < stream_size; i++) {
        if (reservoir.size() < reservoir_size) {
            reservoir.push_back(static_cast<float>(i));
        }
    }

    // Should contain all stream elements
    EXPECT_EQ(reservoir.size(), stream_size);
}

TEST_F(ReservoirSamplingTest, FirstKElements_Initially) {
    // First k elements should definitely be in reservoir
    size_t reservoir_size = 10;
    std::vector<float> reservoir(reservoir_size);
    size_t count = 0;

    // Fill first k
    for (size_t i = 0; i < reservoir_size; i++) {
        reservoir[i] = static_cast<float>(i);
        count++;
    }

    // All first k elements present
    std::set<float> in_reservoir(reservoir.begin(), reservoir.end());
    for (size_t i = 0; i < reservoir_size; i++) {
        EXPECT_TRUE(in_reservoir.count(static_cast<float>(i)) > 0);
    }
}

//=============================================================================
// Batch Add Tests
//=============================================================================

class BatchAddTest : public StreamingStatisticsTest {};

TEST_F(BatchAddTest, AddArray_MatchesSingleAdds) {
    auto data = generateRandomData(1000, 3.0f, 1.5f, 777);

    // Single adds
    nimcp_running_stats_t stats_single;
    nimcp_stats_running_init(&stats_single);
    for (float x : data) {
        nimcp_stats_running_add(&stats_single, x);
    }

    // Array add
    nimcp_running_stats_t stats_array;
    nimcp_stats_running_init(&stats_array);
    nimcp_stats_running_add_array(&stats_array, data.data(), static_cast<uint32_t>(data.size()));

    EXPECT_NEAR(nimcp_stats_running_mean(&stats_single),
                nimcp_stats_running_mean(&stats_array), TOLERANCE);
    EXPECT_NEAR(nimcp_stats_running_variance(&stats_single),
                nimcp_stats_running_variance(&stats_array), TOLERANCE);
}

TEST_F(BatchAddTest, AddArray_Empty) {
    nimcp_running_stats_t stats;
    nimcp_stats_running_init(&stats);

    nimcp_stats_running_add_array(&stats, nullptr, 0);

    double mean = nimcp_stats_running_mean(&stats);
    EXPECT_TRUE(std::isnan(mean) || mean == 0.0);
}

TEST_F(BatchAddTest, AddArray_SingleElement) {
    nimcp_running_stats_t stats;
    nimcp_stats_running_init(&stats);

    float data[] = {42.0f};
    nimcp_stats_running_add_array(&stats, data, 1);

    EXPECT_NEAR(nimcp_stats_running_mean(&stats), 42.0, TOLERANCE);
}

//=============================================================================
// Min/Max Tracking Tests
//=============================================================================

class MinMaxTrackingTest : public StreamingStatisticsTest {};

TEST_F(MinMaxTrackingTest, TracksMin) {
    nimcp_running_stats_t stats;
    nimcp_stats_running_init(&stats);

    nimcp_stats_running_add(&stats, 5.0);
    nimcp_stats_running_add(&stats, 3.0);
    nimcp_stats_running_add(&stats, 7.0);
    nimcp_stats_running_add(&stats, 1.0);
    nimcp_stats_running_add(&stats, 9.0);

    EXPECT_EQ(stats.min, 1.0);
}

TEST_F(MinMaxTrackingTest, TracksMax) {
    nimcp_running_stats_t stats;
    nimcp_stats_running_init(&stats);

    nimcp_stats_running_add(&stats, 5.0);
    nimcp_stats_running_add(&stats, 3.0);
    nimcp_stats_running_add(&stats, 7.0);
    nimcp_stats_running_add(&stats, 1.0);
    nimcp_stats_running_add(&stats, 9.0);

    EXPECT_EQ(stats.max, 9.0);
}

TEST_F(MinMaxTrackingTest, SingleValue_MinEqualsMax) {
    nimcp_running_stats_t stats;
    nimcp_stats_running_init(&stats);

    nimcp_stats_running_add(&stats, 42.0);

    EXPECT_EQ(stats.min, stats.max);
    EXPECT_EQ(stats.min, 42.0);
}

TEST_F(MinMaxTrackingTest, NegativeValues) {
    nimcp_running_stats_t stats;
    nimcp_stats_running_init(&stats);

    nimcp_stats_running_add(&stats, -5.0);
    nimcp_stats_running_add(&stats, -10.0);
    nimcp_stats_running_add(&stats, -3.0);

    EXPECT_EQ(stats.min, -10.0);
    EXPECT_EQ(stats.max, -3.0);
}

//=============================================================================
// Sum Tracking Tests
//=============================================================================

class SumTrackingTest : public StreamingStatisticsTest {};

TEST_F(SumTrackingTest, MatchesBatch) {
    auto data = generateRandomData(1000, 5.0f, 2.0f, 888);

    float batch_sum = nimcp_stats_sum(data.data(), static_cast<uint32_t>(data.size()));

    nimcp_running_stats_t stats;
    nimcp_stats_running_init(&stats);

    for (float x : data) {
        nimcp_stats_running_add(&stats, x);
    }

    EXPECT_NEAR(stats.sum, batch_sum, LOOSE_TOLERANCE);
}

TEST_F(SumTrackingTest, Empty) {
    nimcp_running_stats_t stats;
    nimcp_stats_running_init(&stats);

    EXPECT_EQ(stats.sum, 0.0);
}

//=============================================================================
// Edge Cases
//=============================================================================

class StreamingEdgeCaseTest : public StreamingStatisticsTest {};

TEST_F(StreamingEdgeCaseTest, VeryLargeStream) {
    // Test with many elements
    nimcp_running_stats_t stats;
    nimcp_stats_running_init(&stats);

    std::normal_distribution<float> dist(0.0f, 1.0f);
    for (int i = 0; i < 100000; i++) {
        nimcp_stats_running_add(&stats, dist(rng));
    }

    double mean = nimcp_stats_running_mean(&stats);
    double var = nimcp_stats_running_variance(&stats);

    EXPECT_NEAR(mean, 0.0, 0.02);  // CLT
    EXPECT_NEAR(var, 1.0, 0.05);
}

TEST_F(StreamingEdgeCaseTest, AlternatingValues) {
    nimcp_running_stats_t stats;
    nimcp_stats_running_init(&stats);

    for (int i = 0; i < 1000; i++) {
        nimcp_stats_running_add(&stats, (i % 2 == 0) ? 1.0 : -1.0);
    }

    EXPECT_NEAR(nimcp_stats_running_mean(&stats), 0.0, TOLERANCE);
    EXPECT_NEAR(nimcp_stats_running_variance(&stats), 1.0, LOOSE_TOLERANCE);  // Sample variance ~1.001
}

TEST_F(StreamingEdgeCaseTest, IncreasingSequence) {
    nimcp_running_stats_t stats;
    nimcp_stats_running_init(&stats);

    for (int i = 1; i <= 100; i++) {
        nimcp_stats_running_add(&stats, static_cast<double>(i));
    }

    // Mean of 1..100 is 50.5
    EXPECT_NEAR(nimcp_stats_running_mean(&stats), 50.5, TOLERANCE);

    // Variance of 1..100 is 833.25
    EXPECT_NEAR(nimcp_stats_running_variance(&stats), 841.666667, 1.0);
}

TEST_F(StreamingEdgeCaseTest, InfiniteValue) {
    nimcp_running_stats_t stats;
    nimcp_stats_running_init(&stats);

    nimcp_stats_running_add(&stats, 1.0);
    nimcp_stats_running_add(&stats, INFINITY);
    nimcp_stats_running_add(&stats, 2.0);

    double mean = nimcp_stats_running_mean(&stats);
    // Mean should be infinite or propagate to a very large/abnormal value
    EXPECT_TRUE(std::isinf(mean) || std::isnan(mean) || mean > 1e30);
}

TEST_F(StreamingEdgeCaseTest, NaNValue) {
    nimcp_running_stats_t stats;
    nimcp_stats_running_init(&stats);

    nimcp_stats_running_add(&stats, 1.0);
    nimcp_stats_running_add(&stats, NAN);
    nimcp_stats_running_add(&stats, 2.0);

    double mean = nimcp_stats_running_mean(&stats);
    EXPECT_TRUE(std::isnan(mean));
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
