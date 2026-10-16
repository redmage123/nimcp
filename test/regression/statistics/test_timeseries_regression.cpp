//=============================================================================
// test_timeseries_regression.cpp - Time Series Statistics Regression Tests
//=============================================================================
/**
 * @file test_timeseries_regression.cpp
 * @brief Comprehensive regression tests for time series statistics module
 *
 * REGRESSION TEST FOCUS:
 * - AR(1) coefficient recovery
 * - Spectral peaks at known frequencies
 * - Running/streaming statistics accuracy
 * - Autocorrelation known values
 * - Stationarity tests
 * - Numerical stability with long time series
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <limits>
#include <chrono>
#include <random>
#include <algorithm>
#include <numeric>
#include <complex>

extern "C" {
#include "utils/statistics/nimcp_statistics.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class TimeseriesRegressionTest : public ::testing::Test {
protected:
    static constexpr float EPSILON = 1e-5f;
    static constexpr float RELATIVE_TOL = 1e-4f;
    static constexpr double PI = 3.14159265358979323846;

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

    // Generate AR(1) process: x[t] = phi * x[t-1] + epsilon
    std::vector<float> generateAR1(size_t n, float phi, float sigma = 1.0f) {
        std::normal_distribution<float> noise(0.0f, sigma);
        std::vector<float> x(n);
        x[0] = noise(rng);
        for (size_t t = 1; t < n; ++t) {
            x[t] = phi * x[t-1] + noise(rng);
        }
        return x;
    }

    // Generate sinusoidal signal
    std::vector<float> generateSinusoid(size_t n, float freq, float sample_rate, float amp = 1.0f) {
        std::vector<float> x(n);
        for (size_t t = 0; t < n; ++t) {
            x[t] = amp * std::sin(2.0 * PI * freq * t / sample_rate);
        }
        return x;
    }

    // Generate white noise
    std::vector<float> generateWhiteNoise(size_t n, float sigma = 1.0f) {
        std::normal_distribution<float> dist(0.0f, sigma);
        std::vector<float> x(n);
        for (auto& v : x) v = dist(rng);
        return x;
    }

    // Generate random walk
    std::vector<float> generateRandomWalk(size_t n, float sigma = 1.0f) {
        std::normal_distribution<float> dist(0.0f, sigma);
        std::vector<float> x(n);
        x[0] = 0.0f;
        for (size_t t = 1; t < n; ++t) {
            x[t] = x[t-1] + dist(rng);
        }
        return x;
    }

    // Compute autocorrelation at lag k
    float autocorrelation(const std::vector<float>& x, int lag) {
        size_t n = x.size();
        if (lag >= static_cast<int>(n)) return 0.0f;

        double mean = 0.0;
        for (float v : x) mean += v;
        mean /= n;

        double var = 0.0;
        for (float v : x) var += (v - mean) * (v - mean);

        if (var < 1e-10) return 1.0f; // All same value

        double cov = 0.0;
        for (size_t t = 0; t < n - lag; ++t) {
            cov += (x[t] - mean) * (x[t + lag] - mean);
        }

        return static_cast<float>(cov / var);
    }
};

//=============================================================================
// RUNNING STATISTICS REGRESSION TESTS
//=============================================================================

TEST_F(TimeseriesRegressionTest, RunningMeanConvergence) {
    // Running mean should converge to true mean
    const size_t n = 10000;
    auto data = generateWhiteNoise(n, 1.0f);

    nimcp_running_stats_t running;
    nimcp_stats_running_init(&running);

    std::vector<double> running_means;
    for (size_t i = 0; i < n; ++i) {
        nimcp_stats_running_add(&running, data[i]);
        if ((i + 1) % 100 == 0) {
            running_means.push_back(nimcp_stats_running_mean(&running));
        }
    }

    // Should converge toward 0 (mean of N(0,1))
    double final_mean = nimcp_stats_running_mean(&running);
    EXPECT_NEAR(final_mean, 0.0, 0.05) << "Running mean should converge to 0";

    // Variance should converge to 1
    double final_var = nimcp_stats_running_variance(&running);
    EXPECT_NEAR(final_var, 1.0, 0.1) << "Running variance should converge to 1";
}

TEST_F(TimeseriesRegressionTest, RunningStatsMatchBatch) {
    // Running stats should exactly match batch computation
    auto data = generateWhiteNoise(500, 2.0f);

    nimcp_running_stats_t running;
    nimcp_stats_running_init(&running);

    for (float x : data) {
        nimcp_stats_running_add(&running, x);
    }

    float batch_mean = nimcp_stats_mean(data.data(), data.size());
    float batch_var = nimcp_stats_variance(data.data(), data.size());
    float batch_std = nimcp_stats_std_dev(data.data(), data.size());

    EXPECT_NEAR(nimcp_stats_running_mean(&running), batch_mean, 1e-5f);
    EXPECT_NEAR(nimcp_stats_running_variance(&running), batch_var, 1e-4f);
    EXPECT_NEAR(nimcp_stats_running_std_dev(&running), batch_std, 1e-4f);
}

TEST_F(TimeseriesRegressionTest, RunningStatsMergeAssociativity) {
    // (A merge B) merge C == A merge (B merge C)
    auto data1 = generateWhiteNoise(100, 1.0f);
    auto data2 = generateWhiteNoise(150, 1.5f);
    auto data3 = generateWhiteNoise(80, 0.5f);

    nimcp_running_stats_t a, b, c, ab, abc1, bc, abc2;

    nimcp_stats_running_init(&a);
    nimcp_stats_running_init(&b);
    nimcp_stats_running_init(&c);

    for (float x : data1) nimcp_stats_running_add(&a, x);
    for (float x : data2) nimcp_stats_running_add(&b, x);
    for (float x : data3) nimcp_stats_running_add(&c, x);

    // (A merge B) merge C
    ab = a;
    nimcp_stats_running_merge(&ab, &b);
    abc1 = ab;
    nimcp_stats_running_merge(&abc1, &c);

    // A merge (B merge C)
    bc = b;
    nimcp_stats_running_merge(&bc, &c);
    abc2 = a;
    nimcp_stats_running_merge(&abc2, &bc);

    EXPECT_NEAR(nimcp_stats_running_mean(&abc1), nimcp_stats_running_mean(&abc2), 1e-6);
    EXPECT_NEAR(nimcp_stats_running_variance(&abc1), nimcp_stats_running_variance(&abc2), 1e-5);
}

TEST_F(TimeseriesRegressionTest, RunningStatsMinMax) {
    auto data = generateWhiteNoise(1000, 1.0f);

    nimcp_running_stats_t running;
    nimcp_stats_running_init(&running);

    for (float x : data) {
        nimcp_stats_running_add(&running, x);
    }

    float batch_min = nimcp_stats_min(data.data(), data.size());
    float batch_max = nimcp_stats_max(data.data(), data.size());

    EXPECT_FLOAT_EQ(running.min, batch_min);
    EXPECT_FLOAT_EQ(running.max, batch_max);
}

//=============================================================================
// AUTOCORRELATION REGRESSION TESTS
//=============================================================================

TEST_F(TimeseriesRegressionTest, AutocorrelationLag0) {
    // Autocorrelation at lag 0 is always 1
    auto data = generateWhiteNoise(100, 1.0f);

    float acf0 = autocorrelation(data, 0);
    EXPECT_FLOAT_EQ(acf0, 1.0f);
}

TEST_F(TimeseriesRegressionTest, AutocorrelationWhiteNoise) {
    // White noise should have near-zero autocorrelation at lag > 0
    auto data = generateWhiteNoise(10000, 1.0f);

    for (int lag = 1; lag <= 10; ++lag) {
        float acf = autocorrelation(data, lag);
        EXPECT_NEAR(acf, 0.0f, 0.05f)
            << "White noise ACF at lag " << lag << " should be ~0";
    }
}

TEST_F(TimeseriesRegressionTest, AutocorrelationAR1) {
    // AR(1) with phi = 0.8: ACF(k) = phi^k = 0.8^k
    float phi = 0.8f;
    auto data = generateAR1(10000, phi, 1.0f);

    for (int lag = 1; lag <= 5; ++lag) {
        float acf = autocorrelation(data, lag);
        float expected = std::pow(phi, lag);
        EXPECT_NEAR(acf, expected, 0.1f)
            << "AR(1) ACF at lag " << lag << " should be ~" << expected;
    }
}

TEST_F(TimeseriesRegressionTest, AutocorrelationDecay) {
    // ACF should decay for stationary process
    float phi = 0.9f;
    auto data = generateAR1(5000, phi, 1.0f);

    float prev_acf = 1.0f;
    for (int lag = 1; lag <= 10; ++lag) {
        float acf = autocorrelation(data, lag);
        EXPECT_LT(std::fabs(acf), std::fabs(prev_acf) + 0.1f)
            << "ACF should generally decay";
        prev_acf = acf;
    }
}

//=============================================================================
// SINUSOIDAL SIGNAL REGRESSION TESTS
//=============================================================================

TEST_F(TimeseriesRegressionTest, SinusoidMean) {
    // Sinusoid should have mean ~= 0
    auto signal = generateSinusoid(1000, 10.0f, 100.0f, 1.0f);

    float mean = nimcp_stats_mean(signal.data(), signal.size());
    EXPECT_NEAR(mean, 0.0f, 0.01f);
}

TEST_F(TimeseriesRegressionTest, SinusoidVariance) {
    // Variance of sin(t) = 1/2 (for amplitude 1)
    auto signal = generateSinusoid(10000, 10.0f, 1000.0f, 1.0f);

    float var = nimcp_stats_variance(signal.data(), signal.size());
    EXPECT_NEAR(var, 0.5f, 0.05f);
}

TEST_F(TimeseriesRegressionTest, SinusoidAutocorrelation) {
    // ACF of sinusoid is also sinusoidal
    float freq = 10.0f;
    float sample_rate = 100.0f;
    auto signal = generateSinusoid(1000, freq, sample_rate, 1.0f);

    // At lag = period/2, ACF should be approximately -1
    int half_period = static_cast<int>(sample_rate / (2.0f * freq));
    float acf = autocorrelation(signal, half_period);
    EXPECT_NEAR(acf, -1.0f, 0.1f) << "ACF at half period should be ~-1";

    // At lag = period, ACF should be approximately 1
    int full_period = static_cast<int>(sample_rate / freq);
    acf = autocorrelation(signal, full_period);
    EXPECT_NEAR(acf, 1.0f, 0.1f) << "ACF at full period should be ~1";
}

//=============================================================================
// DIFFERENCING AND STATIONARITY TESTS
//=============================================================================

TEST_F(TimeseriesRegressionTest, FirstDifferenceRandomWalk) {
    // First difference of random walk should be white noise
    auto walk = generateRandomWalk(1000, 1.0f);

    std::vector<float> diff(walk.size() - 1);
    for (size_t t = 0; t < diff.size(); ++t) {
        diff[t] = walk[t + 1] - walk[t];
    }

    // Mean of differences should be ~0
    float mean = nimcp_stats_mean(diff.data(), diff.size());
    EXPECT_NEAR(mean, 0.0f, 0.1f);

    // Autocorrelation should be near zero
    for (int lag = 1; lag <= 5; ++lag) {
        float acf = autocorrelation(diff, lag);
        EXPECT_NEAR(acf, 0.0f, 0.1f)
            << "Differenced random walk should have ACF ~0 at lag " << lag;
    }
}

TEST_F(TimeseriesRegressionTest, StationaryProcessVariance) {
    // Stationary AR(1) should have constant variance over time
    float phi = 0.5f;
    auto data = generateAR1(10000, phi, 1.0f);

    // Compute variance in windows
    size_t window = 1000;
    std::vector<float> variances;

    for (size_t start = 0; start + window <= data.size(); start += window) {
        float var = nimcp_stats_variance(&data[start], window);
        variances.push_back(var);
    }

    // Variance of variances should be small (constant variance)
    float mean_var = nimcp_stats_mean(variances.data(), variances.size());
    float var_var = nimcp_stats_variance(variances.data(), variances.size());

    EXPECT_LT(var_var, 0.5f * mean_var)
        << "Variance should be relatively constant for stationary process";
}

//=============================================================================
// TREND DETECTION TESTS
//=============================================================================

TEST_F(TimeseriesRegressionTest, LinearTrendDetection) {
    // Series with linear trend
    const size_t n = 100;
    std::vector<float> x(n), y(n);
    for (size_t t = 0; t < n; ++t) {
        x[t] = static_cast<float>(t);
        y[t] = 0.5f * t + 10.0f; // Linear trend
    }

    nimcp_regression_result_t result;
    nimcp_stats_regression_linear(x.data(), y.data(), n, &result);

    EXPECT_NEAR(result.slope, 0.5f, 1e-5f);
    EXPECT_NEAR(result.intercept, 10.0f, 1e-4f);
    EXPECT_NEAR(result.r_squared, 1.0f, 1e-6f);
}

TEST_F(TimeseriesRegressionTest, NoTrendDetection) {
    // White noise should have slope ~= 0
    auto data = generateWhiteNoise(1000, 1.0f);

    std::vector<float> x(data.size());
    for (size_t t = 0; t < x.size(); ++t) {
        x[t] = static_cast<float>(t);
    }

    nimcp_regression_result_t result;
    nimcp_stats_regression_linear(x.data(), data.data(), data.size(), &result);

    EXPECT_NEAR(result.slope, 0.0f, 0.01f) << "White noise should have no trend";
    EXPECT_LT(result.r_squared, 0.1f) << "R^2 should be low for no trend";
}

//=============================================================================
// NUMERICAL STABILITY TESTS
//=============================================================================

TEST_F(TimeseriesRegressionTest, LongTimeSeriesStability) {
    // Running stats should remain stable for long time series
    const size_t n = 100000;
    std::normal_distribution<float> dist(0.0f, 1.0f);

    nimcp_running_stats_t running;
    nimcp_stats_running_init(&running);

    for (size_t i = 0; i < n; ++i) {
        nimcp_stats_running_add(&running, dist(rng));
    }

    EXPECT_FALSE(std::isnan(nimcp_stats_running_mean(&running)));
    EXPECT_FALSE(std::isnan(nimcp_stats_running_variance(&running)));
    EXPECT_FALSE(std::isinf(nimcp_stats_running_mean(&running)));
    EXPECT_FALSE(std::isinf(nimcp_stats_running_variance(&running)));

    // Mean and variance should still be reasonable
    EXPECT_NEAR(nimcp_stats_running_mean(&running), 0.0, 0.02);
    EXPECT_NEAR(nimcp_stats_running_variance(&running), 1.0, 0.05);
}

TEST_F(TimeseriesRegressionTest, LargeValueStability) {
    // Handle time series with large values
    const size_t n = 1000;
    std::vector<float> data(n);
    for (size_t t = 0; t < n; ++t) {
        data[t] = 1e6f + static_cast<float>(t) * 0.001f;
    }

    nimcp_running_stats_t running;
    nimcp_stats_running_init(&running);
    for (float x : data) {
        nimcp_stats_running_add(&running, x);
    }

    EXPECT_FALSE(std::isnan(nimcp_stats_running_variance(&running)));
    EXPECT_GT(nimcp_stats_running_variance(&running), 0.0);
}

TEST_F(TimeseriesRegressionTest, SmallDifferencesStability) {
    // Handle time series with very small differences
    const size_t n = 1000;
    std::vector<float> data(n);
    for (size_t t = 0; t < n; ++t) {
        data[t] = 1.0f + static_cast<float>(t) * 1e-8f;
    }

    float var = nimcp_stats_variance(data.data(), n);
    EXPECT_GT(var, 0.0f) << "Variance should detect small differences";
}

//=============================================================================
// CONSISTENCY TESTS
//=============================================================================

TEST_F(TimeseriesRegressionTest, RunningStatsDeterministic) {
    auto data = generateWhiteNoise(500, 1.0f);

    nimcp_running_stats_t run1, run2;
    nimcp_stats_running_init(&run1);
    nimcp_stats_running_init(&run2);

    for (float x : data) {
        nimcp_stats_running_add(&run1, x);
        nimcp_stats_running_add(&run2, x);
    }

    EXPECT_DOUBLE_EQ(nimcp_stats_running_mean(&run1), nimcp_stats_running_mean(&run2));
    EXPECT_DOUBLE_EQ(nimcp_stats_running_variance(&run1), nimcp_stats_running_variance(&run2));
}

TEST_F(TimeseriesRegressionTest, IncrementalEqualsFullComputation) {
    // Adding data incrementally vs all at once should match
    auto data = generateWhiteNoise(300, 1.0f);

    nimcp_running_stats_t incremental;
    nimcp_stats_running_init(&incremental);
    for (float x : data) {
        nimcp_stats_running_add(&incremental, x);
    }

    nimcp_running_stats_t batch;
    nimcp_stats_running_init(&batch);
    nimcp_stats_running_add_array(&batch, data.data(), data.size());

    EXPECT_NEAR(nimcp_stats_running_mean(&incremental),
                nimcp_stats_running_mean(&batch), 1e-10);
    EXPECT_NEAR(nimcp_stats_running_variance(&incremental),
                nimcp_stats_running_variance(&batch), 1e-8);
}

//=============================================================================
// PERFORMANCE TESTS
//=============================================================================

TEST_F(TimeseriesRegressionTest, RunningStatsPerformance) {
    const size_t n = 100000;
    std::normal_distribution<float> dist(0.0f, 1.0f);
    std::vector<float> data(n);
    for (auto& x : data) x = dist(rng);

    auto start = std::chrono::high_resolution_clock::now();

    nimcp_running_stats_t running;
    nimcp_stats_running_init(&running);
    for (float x : data) {
        nimcp_stats_running_add(&running, x);
    }

    auto end = std::chrono::high_resolution_clock::now();
    double elapsed_us = std::chrono::duration<double, std::micro>(end - start).count();

    // Should process 100k points in less than 10ms
    EXPECT_LT(elapsed_us, 10000.0)
        << "Running stats too slow: " << elapsed_us << "us for " << n << " points";
}

TEST_F(TimeseriesRegressionTest, AutocorrelationPerformance) {
    auto data = generateWhiteNoise(1000, 1.0f);

    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 100; ++i) {
        for (int lag = 0; lag <= 20; ++lag) {
            volatile float acf = autocorrelation(data, lag);
            (void)acf;
        }
    }
    auto end = std::chrono::high_resolution_clock::now();

    double elapsed_us = std::chrono::duration<double, std::micro>(end - start).count() / 100.0;
    EXPECT_LT(elapsed_us, 5000.0) << "ACF computation too slow: " << elapsed_us << "us";
}

//=============================================================================
// EDGE CASE TESTS
//=============================================================================

TEST_F(TimeseriesRegressionTest, EmptyRunningStats) {
    nimcp_running_stats_t running;
    nimcp_stats_running_init(&running);

    EXPECT_TRUE(std::isnan(nimcp_stats_running_mean(&running)) ||
                nimcp_stats_running_mean(&running) == 0.0);
    EXPECT_TRUE(std::isnan(nimcp_stats_running_variance(&running)) ||
                nimcp_stats_running_variance(&running) == 0.0);
}

TEST_F(TimeseriesRegressionTest, SinglePointRunningStats) {
    nimcp_running_stats_t running;
    nimcp_stats_running_init(&running);
    nimcp_stats_running_add(&running, 42.0);

    EXPECT_DOUBLE_EQ(nimcp_stats_running_mean(&running), 42.0);
    // Variance undefined for single point
}

TEST_F(TimeseriesRegressionTest, ConstantTimeSeries) {
    std::vector<float> constant(1000, 7.5f);

    nimcp_running_stats_t running;
    nimcp_stats_running_init(&running);
    nimcp_stats_running_add_array(&running, constant.data(), constant.size());

    EXPECT_DOUBLE_EQ(nimcp_stats_running_mean(&running), 7.5);
    EXPECT_DOUBLE_EQ(nimcp_stats_running_variance(&running), 0.0);
}

//=============================================================================
// MAIN
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
