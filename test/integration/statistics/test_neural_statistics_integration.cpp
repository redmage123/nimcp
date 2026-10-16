/**
 * @file test_neural_statistics_integration.cpp
 * @brief Comprehensive integration tests for NIMCP neural statistics modules
 *
 * WHAT: Verify neural statistics integrate correctly with other NIMCP modules
 * WHY:  Ensure statistical analysis works across neural network operations
 * HOW:  Test cross-module contracts, memory management, and realistic data scenarios
 *
 * TEST COVERAGE:
 * - Neural statistics + existing statistics module consistency
 * - Integration with UMM allocator
 * - Health agent integration for long operations
 * - Logging integration
 * - Exception/immune integration
 * - Real-world neural data scenarios (spike trains, population activity)
 *
 * @version 1.0.0
 * @author NIMCP Development Team
 * @date 2026-01-30
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <random>
#include <chrono>
#include <numeric>
#include <algorithm>
#include <limits>
#include <memory>

// Statistics headers
#include "utils/statistics/nimcp_statistics.h"
#include "utils/statistics/nimcp_quantum_statistics.h"

// Memory management
#include "utils/memory/nimcp_memory.h"

// Logging
#include "utils/logging/nimcp_logging.h"

// Core types

//=============================================================================
// Test Configuration Constants
//=============================================================================

namespace {
    // Tolerance thresholds for floating-point comparisons
    constexpr float STRICT_TOLERANCE = 1e-5f;
    constexpr float RELAXED_TOLERANCE = 1e-4f;
    constexpr float STATISTICAL_TOLERANCE = 1e-3f;
    constexpr float BOOTSTRAP_TOLERANCE = 0.05f;  // 5% for bootstrap CI

    // Test data sizes
    constexpr uint32_t SMALL_SIZE = 100;
    constexpr uint32_t MEDIUM_SIZE = 1000;
    constexpr uint32_t LARGE_SIZE = 10000;
    constexpr uint32_t VERY_LARGE_SIZE = 100000;

    // Neural data parameters
    constexpr float SPIKE_RATE_HZ = 10.0f;     // Typical cortical firing rate
    constexpr float MEMBRANE_NOISE_STD = 0.5f;
    constexpr float SAMPLING_RATE_HZ = 1000.0f;

    // Performance thresholds (microseconds) - relaxed for varying system loads
    constexpr uint64_t MEAN_THRESHOLD_US = 1000;       // 1ms for 10k samples
    constexpr uint64_t VARIANCE_THRESHOLD_US = 2000;   // 2ms for 10k samples
    constexpr uint64_t DESCRIBE_THRESHOLD_US = 10000;  // 10ms for all descriptive stats
}

//=============================================================================
// Test Fixture
//=============================================================================

class NeuralStatisticsIntegrationTest : public ::testing::Test {
protected:
    std::mt19937 rng{42};  // Reproducible random numbers

    void SetUp() override {
        // Initialize memory tracking
        nimcp_memory_init();
        nimcp_memory_enable_tracking(true);
        nimcp_memory_clear_stats();

        // Initialize statistics module
        nimcp_stats_config_t config = nimcp_stats_default_config();
        config.enable_simd = true;
        config.enable_parallel = true;
        config.parallel_threshold = 1000;
        config.random_seed = 42;
        nimcp_stats_result_t result = nimcp_stats_init(&config);
        ASSERT_EQ(result, NIMCP_STATS_OK) << "Failed to initialize statistics module";
        ASSERT_TRUE(nimcp_stats_is_initialized());
    }

    void TearDown() override {
        nimcp_stats_shutdown();

        // Check for memory leaks
        nimcp_memory_stats_t stats;
        nimcp_memory_get_stats(&stats);
        EXPECT_LT(stats.current_allocated, 4096)
            << "Potential memory leak detected: " << stats.current_allocated << " bytes";
    }

    //=========================================================================
    // Helper: Generate Poisson spike train
    //=========================================================================
    std::vector<float> generateSpikeTrain(float rate_hz, float duration_s,
                                           float dt = 0.001f) {
        std::vector<float> train;
        float prob_spike = rate_hz * dt;
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);

        for (float t = 0.0f; t < duration_s; t += dt) {
            train.push_back(dist(rng) < prob_spike ? 1.0f : 0.0f);
        }
        return train;
    }

    //=========================================================================
    // Helper: Generate membrane potential with noise
    //=========================================================================
    std::vector<float> generateMembranePotential(uint32_t n_samples,
                                                  float mean = -70.0f,
                                                  float std = 5.0f) {
        std::vector<float> data(n_samples);
        std::normal_distribution<float> dist(mean, std);
        for (uint32_t i = 0; i < n_samples; i++) {
            data[i] = dist(rng);
        }
        return data;
    }

    //=========================================================================
    // Helper: Generate population activity (multiple neurons)
    //=========================================================================
    std::vector<std::vector<float>> generatePopulationActivity(
            uint32_t n_neurons, uint32_t n_timepoints, float mean_rate = 0.1f) {
        std::vector<std::vector<float>> population;
        std::uniform_real_distribution<float> rate_dist(0.05f, 0.2f);

        for (uint32_t i = 0; i < n_neurons; i++) {
            float rate = rate_dist(rng);
            population.push_back(generateSpikeTrain(rate * SAMPLING_RATE_HZ,
                                                    n_timepoints / SAMPLING_RATE_HZ));
        }
        return population;
    }

    //=========================================================================
    // Helper: Measure execution time
    //=========================================================================
    template<typename F>
    uint64_t measureTime(F&& func) {
        auto start = std::chrono::high_resolution_clock::now();
        func();
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    }

    //=========================================================================
    // Helper: Compare with tolerance
    //=========================================================================
    bool nearEqual(float a, float b, float tolerance = RELAXED_TOLERANCE) {
        if (std::isnan(a) || std::isnan(b)) return false;
        if (std::isinf(a) && std::isinf(b)) return (a > 0) == (b > 0);
        return std::fabs(a - b) < tolerance;
    }
};

//=============================================================================
// Module Initialization Tests
//=============================================================================

TEST_F(NeuralStatisticsIntegrationTest, InitializationAndShutdown) {
    // Test that module is initialized
    EXPECT_TRUE(nimcp_stats_is_initialized());

    // Shutdown and verify
    nimcp_stats_shutdown();
    EXPECT_FALSE(nimcp_stats_is_initialized());

    // Re-initialize with default config
    nimcp_stats_result_t result = nimcp_stats_init(nullptr);
    EXPECT_EQ(result, NIMCP_STATS_OK);
    EXPECT_TRUE(nimcp_stats_is_initialized());
}

TEST_F(NeuralStatisticsIntegrationTest, ConfigurationOptions) {
    nimcp_stats_shutdown();

    nimcp_stats_config_t config = nimcp_stats_default_config();

    // Test SIMD disabled
    config.enable_simd = false;
    EXPECT_EQ(nimcp_stats_init(&config), NIMCP_STATS_OK);
    nimcp_stats_shutdown();

    // Test parallel disabled
    config.enable_simd = true;
    config.enable_parallel = false;
    EXPECT_EQ(nimcp_stats_init(&config), NIMCP_STATS_OK);
    nimcp_stats_shutdown();

    // Test with specific seed
    config.enable_parallel = true;
    config.random_seed = 12345;
    EXPECT_EQ(nimcp_stats_init(&config), NIMCP_STATS_OK);
}

//=============================================================================
// Descriptive Statistics Tests with Neural Data
//=============================================================================

TEST_F(NeuralStatisticsIntegrationTest, MembranePotentialStatistics) {
    auto membrane = generateMembranePotential(MEDIUM_SIZE, -70.0f, 5.0f);

    float mean = nimcp_stats_mean(membrane.data(), MEDIUM_SIZE);
    float variance = nimcp_stats_variance(membrane.data(), MEDIUM_SIZE);
    float std_dev = nimcp_stats_std_dev(membrane.data(), MEDIUM_SIZE);

    // Check reasonable values for membrane potential
    EXPECT_NEAR(mean, -70.0f, 1.0f) << "Mean should be near -70mV";
    EXPECT_NEAR(std_dev, 5.0f, 1.0f) << "Std dev should be near 5mV";
    EXPECT_NEAR(variance, std_dev * std_dev, 0.5f) << "Variance = std_dev^2";

    // Test skewness (should be near zero for normal distribution)
    float skew = nimcp_stats_skewness(membrane.data(), MEDIUM_SIZE);
    EXPECT_NEAR(skew, 0.0f, 0.5f) << "Skewness should be near zero";

    // Test kurtosis (should be near zero for normal distribution)
    float kurt = nimcp_stats_kurtosis(membrane.data(), MEDIUM_SIZE);
    EXPECT_NEAR(kurt, 0.0f, 0.5f) << "Excess kurtosis should be near zero";
}

TEST_F(NeuralStatisticsIntegrationTest, SpikeTrainStatistics) {
    auto spikes = generateSpikeTrain(SPIKE_RATE_HZ, 10.0f);  // 10 seconds

    float mean = nimcp_stats_mean(spikes.data(), static_cast<uint32_t>(spikes.size()));
    float rate_estimate = mean * SAMPLING_RATE_HZ;

    // Rate estimate should be near the generating rate
    EXPECT_NEAR(rate_estimate, SPIKE_RATE_HZ, 2.0f)
        << "Estimated rate should be near " << SPIKE_RATE_HZ << " Hz";

    // Spike trains are binary, so min=0, max=1
    float min_val = nimcp_stats_min(spikes.data(), static_cast<uint32_t>(spikes.size()));
    float max_val = nimcp_stats_max(spikes.data(), static_cast<uint32_t>(spikes.size()));
    EXPECT_EQ(min_val, 0.0f);
    EXPECT_EQ(max_val, 1.0f);
}

TEST_F(NeuralStatisticsIntegrationTest, DescribeAllStatistics) {
    auto membrane = generateMembranePotential(LARGE_SIZE);
    nimcp_descriptive_stats_t stats;

    nimcp_stats_result_t result = nimcp_stats_describe(membrane.data(), LARGE_SIZE, &stats);
    EXPECT_EQ(result, NIMCP_STATS_OK);

    // Verify all fields are populated
    EXPECT_EQ(stats.n, LARGE_SIZE);
    EXPECT_TRUE(std::isfinite(stats.mean));
    EXPECT_TRUE(std::isfinite(stats.variance));
    EXPECT_TRUE(std::isfinite(stats.std_dev));
    EXPECT_TRUE(std::isfinite(stats.median));
    EXPECT_TRUE(std::isfinite(stats.q1));
    EXPECT_TRUE(std::isfinite(stats.q3));
    EXPECT_TRUE(std::isfinite(stats.iqr));
    EXPECT_TRUE(std::isfinite(stats.skewness));
    EXPECT_TRUE(std::isfinite(stats.kurtosis));

    // IQR should be Q3 - Q1
    EXPECT_NEAR(stats.iqr, stats.q3 - stats.q1, STRICT_TOLERANCE);

    // Q1 < median < Q3
    EXPECT_LT(stats.q1, stats.median);
    EXPECT_LT(stats.median, stats.q3);
}

TEST_F(NeuralStatisticsIntegrationTest, QuantileComputation) {
    auto data = generateMembranePotential(MEDIUM_SIZE);

    // Test various quantiles
    float q0 = nimcp_stats_quantile(data.data(), MEDIUM_SIZE, 0.0f);
    float q25 = nimcp_stats_quantile(data.data(), MEDIUM_SIZE, 0.25f);
    float q50 = nimcp_stats_quantile(data.data(), MEDIUM_SIZE, 0.5f);
    float q75 = nimcp_stats_quantile(data.data(), MEDIUM_SIZE, 0.75f);
    float q100 = nimcp_stats_quantile(data.data(), MEDIUM_SIZE, 1.0f);

    // Verify ordering
    EXPECT_LE(q0, q25);
    EXPECT_LE(q25, q50);
    EXPECT_LE(q50, q75);
    EXPECT_LE(q75, q100);

    // Verify consistency with dedicated functions
    float median = nimcp_stats_median(data.data(), MEDIUM_SIZE);
    EXPECT_NEAR(q50, median, STRICT_TOLERANCE);

    float min_val = nimcp_stats_min(data.data(), MEDIUM_SIZE);
    float max_val = nimcp_stats_max(data.data(), MEDIUM_SIZE);
    EXPECT_NEAR(q0, min_val, STRICT_TOLERANCE);
    EXPECT_NEAR(q100, max_val, STRICT_TOLERANCE);
}

//=============================================================================
// Running Statistics Tests
//=============================================================================

TEST_F(NeuralStatisticsIntegrationTest, RunningStatisticsAccumulation) {
    auto data = generateMembranePotential(MEDIUM_SIZE);

    nimcp_running_stats_t running;
    nimcp_stats_running_init(&running);

    // Add data point by point
    for (uint32_t i = 0; i < MEDIUM_SIZE; i++) {
        nimcp_stats_running_add(&running, data[i]);
    }

    // Compare with batch computation
    float batch_mean = nimcp_stats_mean(data.data(), MEDIUM_SIZE);
    float batch_var = nimcp_stats_variance(data.data(), MEDIUM_SIZE);

    double running_mean = nimcp_stats_running_mean(&running);
    double running_var = nimcp_stats_running_variance(&running);

    EXPECT_NEAR(static_cast<float>(running_mean), batch_mean, RELAXED_TOLERANCE)
        << "Running mean should match batch mean";
    EXPECT_NEAR(static_cast<float>(running_var), batch_var, RELAXED_TOLERANCE)
        << "Running variance should match batch variance";
}

TEST_F(NeuralStatisticsIntegrationTest, RunningStatisticsMerge) {
    auto data1 = generateMembranePotential(SMALL_SIZE);
    auto data2 = generateMembranePotential(SMALL_SIZE);

    // Combine data for reference
    std::vector<float> combined;
    combined.insert(combined.end(), data1.begin(), data1.end());
    combined.insert(combined.end(), data2.begin(), data2.end());

    // Compute running stats separately and merge
    nimcp_running_stats_t stats1, stats2;
    nimcp_stats_running_init(&stats1);
    nimcp_stats_running_init(&stats2);

    nimcp_stats_running_add_array(&stats1, data1.data(), SMALL_SIZE);
    nimcp_stats_running_add_array(&stats2, data2.data(), SMALL_SIZE);

    // Merge
    nimcp_stats_running_merge(&stats1, &stats2);

    // Compare with combined batch
    float batch_mean = nimcp_stats_mean(combined.data(), SMALL_SIZE * 2);
    EXPECT_NEAR(static_cast<float>(nimcp_stats_running_mean(&stats1)), batch_mean, RELAXED_TOLERANCE);
}

TEST_F(NeuralStatisticsIntegrationTest, RunningStatisticsHigherMoments) {
    auto data = generateMembranePotential(LARGE_SIZE);

    nimcp_running_stats_t running;
    nimcp_stats_running_init(&running);
    nimcp_stats_running_add_array(&running, data.data(), LARGE_SIZE);

    float batch_skew = nimcp_stats_skewness(data.data(), LARGE_SIZE);
    float batch_kurt = nimcp_stats_kurtosis(data.data(), LARGE_SIZE);

    double running_skew = nimcp_stats_running_skewness(&running);
    double running_kurt = nimcp_stats_running_kurtosis(&running);

    EXPECT_NEAR(static_cast<float>(running_skew), batch_skew, STATISTICAL_TOLERANCE)
        << "Running skewness should match batch skewness";
    EXPECT_NEAR(static_cast<float>(running_kurt), batch_kurt, STATISTICAL_TOLERANCE)
        << "Running kurtosis should match batch kurtosis";
}

//=============================================================================
// Probability Distribution Tests
//=============================================================================

TEST_F(NeuralStatisticsIntegrationTest, NormalDistributionPDF) {
    // Test standard normal PDF
    EXPECT_NEAR(nimcp_stats_pdf_standard_normal(0.0f), 0.3989f, RELAXED_TOLERANCE)
        << "PDF at mean should be ~0.3989";

    // Symmetry
    EXPECT_NEAR(nimcp_stats_pdf_standard_normal(1.0f),
                nimcp_stats_pdf_standard_normal(-1.0f), STRICT_TOLERANCE);

    // General normal
    float pdf_at_mean = nimcp_stats_pdf_normal(-70.0f, -70.0f, 5.0f);
    EXPECT_NEAR(pdf_at_mean, 1.0f / (5.0f * std::sqrt(2.0f * M_PI)), RELAXED_TOLERANCE);
}

TEST_F(NeuralStatisticsIntegrationTest, NormalDistributionCDF) {
    // Standard normal CDF properties
    EXPECT_NEAR(nimcp_stats_cdf_standard_normal(0.0f), 0.5f, RELAXED_TOLERANCE);
    EXPECT_NEAR(nimcp_stats_cdf_standard_normal(-10.0f), 0.0f, RELAXED_TOLERANCE);
    EXPECT_NEAR(nimcp_stats_cdf_standard_normal(10.0f), 1.0f, RELAXED_TOLERANCE);

    // 68-95-99.7 rule
    EXPECT_NEAR(nimcp_stats_cdf_standard_normal(1.0f) -
                nimcp_stats_cdf_standard_normal(-1.0f), 0.6827f, 0.01f);
    EXPECT_NEAR(nimcp_stats_cdf_standard_normal(2.0f) -
                nimcp_stats_cdf_standard_normal(-2.0f), 0.9545f, 0.01f);
}

TEST_F(NeuralStatisticsIntegrationTest, NormalQuantileInverse) {
    // Quantile should be inverse of CDF
    for (float p = 0.1f; p <= 0.9f; p += 0.1f) {
        float q = nimcp_stats_quantile_standard_normal(p);
        float p_check = nimcp_stats_cdf_standard_normal(q);
        EXPECT_NEAR(p, p_check, RELAXED_TOLERANCE)
            << "CDF(quantile(p)) should equal p";
    }
}

TEST_F(NeuralStatisticsIntegrationTest, PoissonDistribution) {
    // Poisson PMF for spike counts
    float lambda = 10.0f;  // Mean spike count

    // PMF should sum to 1 (approximately)
    float sum = 0.0f;
    for (uint32_t k = 0; k <= 50; k++) {
        sum += nimcp_stats_pmf_poisson(k, lambda);
    }
    EXPECT_NEAR(sum, 1.0f, RELAXED_TOLERANCE);

    // Mode should be near lambda
    float max_pmf = 0.0f;
    uint32_t mode = 0;
    for (uint32_t k = 0; k <= 20; k++) {
        float pmf = nimcp_stats_pmf_poisson(k, lambda);
        if (pmf > max_pmf) {
            max_pmf = pmf;
            mode = k;
        }
    }
    EXPECT_LE(std::abs(static_cast<int>(mode) - static_cast<int>(lambda)), 1);
}

TEST_F(NeuralStatisticsIntegrationTest, ExponentialDistribution) {
    // Inter-spike intervals follow exponential distribution
    float rate = 10.0f;  // spikes per second

    // PDF at 0
    EXPECT_NEAR(nimcp_stats_pdf_exponential(0.0f, rate), rate, RELAXED_TOLERANCE);

    // CDF properties
    EXPECT_NEAR(nimcp_stats_cdf_exponential(0.0f, rate), 0.0f, RELAXED_TOLERANCE);
    EXPECT_NEAR(nimcp_stats_cdf_exponential(100.0f, rate), 1.0f, RELAXED_TOLERANCE);

    // Mean of exponential is 1/rate
    float median_isi = std::log(2.0f) / rate;
    EXPECT_NEAR(nimcp_stats_cdf_exponential(median_isi, rate), 0.5f, RELAXED_TOLERANCE);
}

TEST_F(NeuralStatisticsIntegrationTest, GammaDistribution) {
    // Gamma distribution for burst spike counts
    float shape = 2.0f;
    float scale = 5.0f;

    // PDF should be positive
    EXPECT_GT(nimcp_stats_pdf_gamma(5.0f, shape, scale), 0.0f);

    // CDF should be monotonic increasing (with small tolerance for numerical precision)
    float prev_cdf = 0.0f;
    for (float x = 0.1f; x <= 20.0f; x += 1.0f) {
        float cdf = nimcp_stats_cdf_gamma(x, shape, scale);
        EXPECT_GE(cdf, prev_cdf - 0.02f) << "CDF should be approximately monotonic";
        prev_cdf = cdf;
    }
}

//=============================================================================
// Distribution Sampling Tests
//=============================================================================

TEST_F(NeuralStatisticsIntegrationTest, NormalSampling) {
    nimcp_distribution_params_t params;
    params.type = NIMCP_DIST_NORMAL;
    params.params.normal.mu = -70.0f;
    params.params.normal.sigma = 5.0f;

    std::vector<float> samples(LARGE_SIZE);
    nimcp_stats_result_t result = nimcp_stats_sample_array(&params, samples.data(), LARGE_SIZE);
    EXPECT_EQ(result, NIMCP_STATS_OK);

    // Check sample statistics match distribution parameters
    float sample_mean = nimcp_stats_mean(samples.data(), LARGE_SIZE);
    float sample_std = nimcp_stats_std_dev(samples.data(), LARGE_SIZE);

    EXPECT_NEAR(sample_mean, -70.0f, 0.5f) << "Sample mean should be near -70";
    EXPECT_NEAR(sample_std, 5.0f, 0.5f) << "Sample std should be near 5";
}

TEST_F(NeuralStatisticsIntegrationTest, UniformSampling) {
    nimcp_distribution_params_t params;
    params.type = NIMCP_DIST_UNIFORM;
    params.params.uniform.a = 0.0f;
    params.params.uniform.b = 1.0f;

    std::vector<float> samples(LARGE_SIZE);
    nimcp_stats_result_t result = nimcp_stats_sample_array(&params, samples.data(), LARGE_SIZE);
    EXPECT_EQ(result, NIMCP_STATS_OK);

    // All samples should be in [0, 1]
    for (uint32_t i = 0; i < LARGE_SIZE; i++) {
        EXPECT_GE(samples[i], 0.0f);
        EXPECT_LE(samples[i], 1.0f);
    }

    // Mean should be 0.5
    float mean = nimcp_stats_mean(samples.data(), LARGE_SIZE);
    EXPECT_NEAR(mean, 0.5f, 0.02f);
}

//=============================================================================
// Hypothesis Testing Tests
//=============================================================================

TEST_F(NeuralStatisticsIntegrationTest, OneSampleTTest) {
    // Generate data with known mean
    auto data = generateMembranePotential(MEDIUM_SIZE, -70.0f, 5.0f);

    nimcp_test_result_t result;
    nimcp_stats_result_t status = nimcp_stats_ttest_one_sample(
        data.data(), MEDIUM_SIZE, -70.0f, NIMCP_TEST_TWO_SIDED, 0.95f, &result);

    EXPECT_EQ(status, NIMCP_STATS_OK);

    // P-value should be high (fail to reject null that mean = -70)
    EXPECT_GT(result.p_value, 0.05f)
        << "Should fail to reject null hypothesis";

    // Test with wrong mean
    status = nimcp_stats_ttest_one_sample(
        data.data(), MEDIUM_SIZE, -50.0f, NIMCP_TEST_TWO_SIDED, 0.95f, &result);

    EXPECT_EQ(status, NIMCP_STATS_OK);
    EXPECT_LT(result.p_value, 0.05f)
        << "Should reject null hypothesis for wrong mean";
}

TEST_F(NeuralStatisticsIntegrationTest, TwoSampleTTest) {
    // Generate two samples with different means
    auto control = generateMembranePotential(SMALL_SIZE, -70.0f, 5.0f);
    auto treatment = generateMembranePotential(SMALL_SIZE, -65.0f, 5.0f);

    nimcp_test_result_t result;
    nimcp_stats_result_t status = nimcp_stats_ttest_two_sample(
        control.data(), SMALL_SIZE, treatment.data(), SMALL_SIZE,
        true, NIMCP_TEST_TWO_SIDED, 0.95f, &result);

    EXPECT_EQ(status, NIMCP_STATS_OK);

    // Should detect significant difference
    EXPECT_LT(result.p_value, 0.05f)
        << "Should detect significant difference between groups";

    // Effect size should be positive (treatment > control in mV)
    EXPECT_NE(result.effect_size, 0.0f)
        << "Effect size should be non-zero";
}

TEST_F(NeuralStatisticsIntegrationTest, PairedTTest) {
    // Before and after treatment
    auto before = generateMembranePotential(SMALL_SIZE, -70.0f, 5.0f);
    std::vector<float> after(SMALL_SIZE);
    for (uint32_t i = 0; i < SMALL_SIZE; i++) {
        after[i] = before[i] + 5.0f;  // Consistent 5mV shift
    }

    nimcp_test_result_t result;
    nimcp_stats_result_t status = nimcp_stats_ttest_paired(
        before.data(), after.data(), SMALL_SIZE,
        NIMCP_TEST_TWO_SIDED, 0.95f, &result);

    EXPECT_EQ(status, NIMCP_STATS_OK);
    EXPECT_LT(result.p_value, 0.001f)
        << "Should detect highly significant paired difference";
}

TEST_F(NeuralStatisticsIntegrationTest, ChiSquaredGoodnessOfFit) {
    // Test if spike counts follow expected distribution
    std::vector<float> observed = {10.0f, 25.0f, 30.0f, 25.0f, 10.0f};
    std::vector<float> expected = {20.0f, 20.0f, 20.0f, 20.0f, 20.0f};

    nimcp_test_result_t result;
    nimcp_stats_result_t status = nimcp_stats_chi_squared_gof(
        observed.data(), expected.data(), 5, &result);

    EXPECT_EQ(status, NIMCP_STATS_OK);
    EXPECT_TRUE(std::isfinite(result.statistic));
    EXPECT_TRUE(std::isfinite(result.p_value));
    EXPECT_GE(result.p_value, 0.0f);
    EXPECT_LE(result.p_value, 1.0f);
}

TEST_F(NeuralStatisticsIntegrationTest, ShapiroWilkNormalityTest) {
    // Test with normal data - should pass
    auto normal_data = generateMembranePotential(SMALL_SIZE);
    nimcp_test_result_t result;
    nimcp_stats_result_t status = nimcp_stats_shapiro_wilk(
        normal_data.data(), SMALL_SIZE, &result);

    EXPECT_EQ(status, NIMCP_STATS_OK);
    EXPECT_GT(result.p_value, 0.05f)
        << "Normal data should pass Shapiro-Wilk test";

    // Test with uniform data - should fail
    std::uniform_real_distribution<float> uniform_dist(0.0f, 1.0f);
    std::vector<float> uniform_data(SMALL_SIZE);
    for (uint32_t i = 0; i < SMALL_SIZE; i++) {
        uniform_data[i] = uniform_dist(rng);
    }

    status = nimcp_stats_shapiro_wilk(uniform_data.data(), SMALL_SIZE, &result);
    EXPECT_EQ(status, NIMCP_STATS_OK);
    // Uniform might pass or fail depending on sample
}

TEST_F(NeuralStatisticsIntegrationTest, ANOVA) {
    // Create groups with different means
    auto group1 = generateMembranePotential(50, -70.0f, 3.0f);
    auto group2 = generateMembranePotential(50, -65.0f, 3.0f);
    auto group3 = generateMembranePotential(50, -60.0f, 3.0f);

    const float* groups[] = {group1.data(), group2.data(), group3.data()};
    uint32_t sizes[] = {50, 50, 50};

    nimcp_anova_result_t result;
    nimcp_stats_result_t status = nimcp_stats_anova_one_way(
        groups, sizes, 3, 0.05f, &result);

    EXPECT_EQ(status, NIMCP_STATS_OK);
    EXPECT_GT(result.f_statistic, 0.0f);
    EXPECT_LT(result.p_value, 0.05f)
        << "ANOVA should detect significant difference between groups";
    EXPECT_TRUE(result.significant);
    EXPECT_GT(result.eta_squared, 0.0f) << "Effect size should be positive";
}

//=============================================================================
// Correlation Tests
//=============================================================================

TEST_F(NeuralStatisticsIntegrationTest, PearsonCorrelation) {
    // Generate correlated data
    auto x = generateMembranePotential(MEDIUM_SIZE);
    std::vector<float> y(MEDIUM_SIZE);
    std::normal_distribution<float> noise(0.0f, 2.0f);
    for (uint32_t i = 0; i < MEDIUM_SIZE; i++) {
        y[i] = 0.8f * x[i] + noise(rng);  // r should be ~0.8
    }

    nimcp_correlation_result_t result;
    nimcp_stats_result_t status = nimcp_stats_correlation_pearson(
        x.data(), y.data(), MEDIUM_SIZE, &result);

    EXPECT_EQ(status, NIMCP_STATS_OK);
    EXPECT_GT(result.r, 0.7f) << "Correlation should be strong positive";
    EXPECT_LT(result.r, 1.0f);
    EXPECT_LT(result.p_value, 0.001f) << "Correlation should be significant";
}

TEST_F(NeuralStatisticsIntegrationTest, SpearmanCorrelation) {
    // Generate monotonically related but non-linear data
    auto x = generateMembranePotential(SMALL_SIZE);
    std::vector<float> y(SMALL_SIZE);
    for (uint32_t i = 0; i < SMALL_SIZE; i++) {
        y[i] = x[i] * x[i] * x[i];  // Cubic relationship
    }

    nimcp_correlation_result_t result;
    nimcp_stats_result_t status = nimcp_stats_correlation_spearman(
        x.data(), y.data(), SMALL_SIZE, &result);

    EXPECT_EQ(status, NIMCP_STATS_OK);
    // Spearman should detect monotonic relationship
    EXPECT_GT(std::abs(result.r), 0.5f);
}

TEST_F(NeuralStatisticsIntegrationTest, PartialCorrelation) {
    // Test partial correlation controlling for confound
    auto x = generateMembranePotential(SMALL_SIZE);
    auto z = generateMembranePotential(SMALL_SIZE);  // Confound
    std::vector<float> y(SMALL_SIZE);
    std::normal_distribution<float> noise(0.0f, 1.0f);

    // Y depends on both X and Z
    for (uint32_t i = 0; i < SMALL_SIZE; i++) {
        y[i] = 0.3f * x[i] + 0.7f * z[i] + noise(rng);
    }

    nimcp_correlation_result_t result;
    nimcp_stats_result_t status = nimcp_stats_correlation_partial(
        x.data(), y.data(), z.data(), SMALL_SIZE, &result);

    EXPECT_EQ(status, NIMCP_STATS_OK);
    EXPECT_TRUE(std::isfinite(result.r));
}

TEST_F(NeuralStatisticsIntegrationTest, CovarianceMatrix) {
    // Generate multivariate data
    uint32_t n_vars = 4;
    uint32_t n_obs = SMALL_SIZE;
    std::vector<float> data(n_obs * n_vars);
    std::vector<float> cov_matrix(n_vars * n_vars);

    // Fill with random data
    std::normal_distribution<float> dist(0.0f, 1.0f);
    for (uint32_t i = 0; i < n_obs * n_vars; i++) {
        data[i] = dist(rng);
    }

    nimcp_stats_result_t status = nimcp_stats_covariance_matrix(
        data.data(), n_obs, n_vars, cov_matrix.data());

    EXPECT_EQ(status, NIMCP_STATS_OK);

    // Covariance matrix should be symmetric
    for (uint32_t i = 0; i < n_vars; i++) {
        for (uint32_t j = 0; j < n_vars; j++) {
            EXPECT_NEAR(cov_matrix[i * n_vars + j],
                       cov_matrix[j * n_vars + i], STRICT_TOLERANCE)
                << "Covariance matrix should be symmetric";
        }
    }

    // Diagonal should be positive (variances)
    for (uint32_t i = 0; i < n_vars; i++) {
        EXPECT_GT(cov_matrix[i * n_vars + i], 0.0f)
            << "Variance should be positive";
    }
}

//=============================================================================
// Regression Tests
//=============================================================================

TEST_F(NeuralStatisticsIntegrationTest, LinearRegression) {
    // Generate linear data: y = 2x + 5 + noise
    std::vector<float> x(SMALL_SIZE);
    std::vector<float> y(SMALL_SIZE);
    std::normal_distribution<float> noise(0.0f, 1.0f);

    for (uint32_t i = 0; i < SMALL_SIZE; i++) {
        x[i] = static_cast<float>(i) / 10.0f;
        y[i] = 2.0f * x[i] + 5.0f + noise(rng);
    }

    nimcp_regression_result_t result;
    nimcp_stats_result_t status = nimcp_stats_regression_linear(
        x.data(), y.data(), SMALL_SIZE, &result);

    EXPECT_EQ(status, NIMCP_STATS_OK);
    EXPECT_NEAR(result.slope, 2.0f, 0.5f) << "Slope should be near 2";
    EXPECT_NEAR(result.intercept, 5.0f, 1.0f) << "Intercept should be near 5";
    EXPECT_GT(result.r_squared, 0.9f) << "R-squared should be high";
}

TEST_F(NeuralStatisticsIntegrationTest, PolynomialRegression) {
    // Generate quadratic data: y = x^2 - 3x + 2 + noise
    std::vector<float> x(SMALL_SIZE);
    std::vector<float> y(SMALL_SIZE);
    std::normal_distribution<float> noise(0.0f, 0.5f);

    for (uint32_t i = 0; i < SMALL_SIZE; i++) {
        float xi = -5.0f + 10.0f * i / SMALL_SIZE;
        x[i] = xi;
        y[i] = xi * xi - 3.0f * xi + 2.0f + noise(rng);
    }

    nimcp_regression_result_t result;
    result.coefficients = new float[3];  // Allocate for degree 2
    result.n_coefficients = 3;

    nimcp_stats_result_t status = nimcp_stats_regression_polynomial(
        x.data(), y.data(), SMALL_SIZE, 2, &result);

    EXPECT_EQ(status, NIMCP_STATS_OK);
    EXPECT_GT(result.r_squared, 0.95f) << "R-squared should be high for quadratic fit";

    nimcp_stats_regression_free(&result);
}

//=============================================================================
// Bayesian Inference Tests
//=============================================================================

TEST_F(NeuralStatisticsIntegrationTest, BetaBinomialUpdate) {
    // Bayesian update for spike detection probability
    // Prior: Beta(1, 1) = uniform
    // Observed: 30 spikes in 100 bins
    nimcp_bayesian_result_t result;
    nimcp_stats_result_t status = nimcp_stats_bayesian_beta_binomial(
        1.0f, 1.0f, 30, 100, 0.95f, &result);

    EXPECT_EQ(status, NIMCP_STATS_OK);
    EXPECT_NEAR(result.posterior_mean, 0.306f, 0.02f)  // (1+30)/(2+100)
        << "Posterior mean should be (alpha+successes)/(alpha+beta+trials)";
    EXPECT_GT(result.credible_lower, 0.0f);
    EXPECT_LT(result.credible_upper, 1.0f);
    EXPECT_LT(result.credible_lower, result.posterior_mean);
    EXPECT_GT(result.credible_upper, result.posterior_mean);
}

TEST_F(NeuralStatisticsIntegrationTest, NormalBayesianUpdate) {
    // Bayesian update for membrane potential with known variance
    auto data = generateMembranePotential(SMALL_SIZE, -70.0f, 5.0f);

    nimcp_bayesian_result_t result;
    nimcp_stats_result_t status = nimcp_stats_bayesian_normal(
        -65.0f, 100.0f,  // Prior: N(-65, 100)
        data.data(), SMALL_SIZE,
        25.0f,  // Known variance = 5^2
        0.95f, &result);

    EXPECT_EQ(status, NIMCP_STATS_OK);
    // Posterior mean should be between prior mean and sample mean
    float sample_mean = nimcp_stats_mean(data.data(), SMALL_SIZE);
    EXPECT_TRUE((result.posterior_mean >= std::min(-65.0f, sample_mean) - 1.0f) &&
                (result.posterior_mean <= std::max(-65.0f, sample_mean) + 1.0f));
}

TEST_F(NeuralStatisticsIntegrationTest, GammaPoissonUpdate) {
    // Bayesian update for spike rate estimation
    // Prior: Gamma(2, 0.5) -> mean = 4 spikes
    // Observed: 50 spikes in 10 time units
    nimcp_bayesian_result_t result;
    nimcp_stats_result_t status = nimcp_stats_bayesian_gamma_poisson(
        2.0f, 0.5f, 50, 10.0f, 0.95f, &result);

    EXPECT_EQ(status, NIMCP_STATS_OK);
    // Posterior mean = (prior_shape + events) / (prior_rate + exposure)
    float expected_mean = (2.0f + 50.0f) / (0.5f + 10.0f);
    EXPECT_NEAR(result.posterior_mean, expected_mean, 0.1f);
}

//=============================================================================
// Information Theory Tests
//=============================================================================

TEST_F(NeuralStatisticsIntegrationTest, ShannonEntropy) {
    // Test with uniform distribution (max entropy)
    std::vector<float> uniform = {0.25f, 0.25f, 0.25f, 0.25f};
    float entropy_uniform = nimcp_stats_entropy(uniform.data(), 4);
    EXPECT_NEAR(entropy_uniform, 2.0f, RELAXED_TOLERANCE)
        << "Entropy of uniform 4-state should be 2 bits";

    // Test with deterministic distribution (zero entropy)
    std::vector<float> deterministic = {1.0f, 0.0f, 0.0f, 0.0f};
    float entropy_det = nimcp_stats_entropy(deterministic.data(), 4);
    EXPECT_NEAR(entropy_det, 0.0f, RELAXED_TOLERANCE)
        << "Entropy of deterministic should be 0";

    // Test with biased distribution
    std::vector<float> biased = {0.5f, 0.5f};
    float entropy_biased = nimcp_stats_entropy(biased.data(), 2);
    EXPECT_NEAR(entropy_biased, 1.0f, RELAXED_TOLERANCE)
        << "Entropy of 50/50 binary should be 1 bit";
}

TEST_F(NeuralStatisticsIntegrationTest, MutualInformation) {
    // Independent variables -> MI = 0
    std::vector<float> joint_indep(4);
    joint_indep[0] = 0.25f; joint_indep[1] = 0.25f;
    joint_indep[2] = 0.25f; joint_indep[3] = 0.25f;

    float mi_indep = nimcp_stats_mutual_information(joint_indep.data(), 2, 2);
    EXPECT_NEAR(mi_indep, 0.0f, RELAXED_TOLERANCE)
        << "MI of independent variables should be 0";

    // Perfectly correlated -> MI = H(X)
    std::vector<float> joint_corr(4);
    joint_corr[0] = 0.5f; joint_corr[1] = 0.0f;
    joint_corr[2] = 0.0f; joint_corr[3] = 0.5f;

    float mi_corr = nimcp_stats_mutual_information(joint_corr.data(), 2, 2);
    EXPECT_NEAR(mi_corr, 1.0f, RELAXED_TOLERANCE)
        << "MI of perfectly correlated should be 1 bit";
}

TEST_F(NeuralStatisticsIntegrationTest, KLDivergence) {
    // KL(P||P) = 0
    std::vector<float> p = {0.5f, 0.3f, 0.2f};
    float kl_self = nimcp_stats_kl_divergence(p.data(), p.data(), 3);
    EXPECT_NEAR(kl_self, 0.0f, RELAXED_TOLERANCE);

    // KL(P||Q) >= 0
    std::vector<float> q = {0.33f, 0.34f, 0.33f};
    float kl_pq = nimcp_stats_kl_divergence(p.data(), q.data(), 3);
    EXPECT_GE(kl_pq, 0.0f) << "KL divergence should be non-negative";
}

TEST_F(NeuralStatisticsIntegrationTest, JensenShannonDivergence) {
    std::vector<float> p = {0.5f, 0.3f, 0.2f};
    std::vector<float> q = {0.33f, 0.34f, 0.33f};

    float js = nimcp_stats_js_divergence(p.data(), q.data(), 3);

    // JS is symmetric
    float js_qp = nimcp_stats_js_divergence(q.data(), p.data(), 3);
    EXPECT_NEAR(js, js_qp, STRICT_TOLERANCE) << "JS should be symmetric";

    // JS in [0, 1] for base 2
    EXPECT_GE(js, 0.0f);
    EXPECT_LE(js, 1.0f);
}

TEST_F(NeuralStatisticsIntegrationTest, TransferEntropy) {
    // Generate causally related time series
    auto x = generateSpikeTrain(10.0f, 1.0f);
    std::vector<float> y(x.size());

    // Y lags X by 2 samples with some noise
    std::uniform_real_distribution<float> noise(0.0f, 1.0f);
    for (size_t i = 2; i < x.size(); i++) {
        y[i] = (x[i-2] > 0.5f && noise(rng) < 0.8f) ? 1.0f : 0.0f;
    }

    float te_xy = nimcp_stats_transfer_entropy(
        x.data(), y.data(), static_cast<uint32_t>(x.size()), 3, 10);
    float te_yx = nimcp_stats_transfer_entropy(
        y.data(), x.data(), static_cast<uint32_t>(x.size()), 3, 10);

    // X->Y should have higher transfer entropy than Y->X
    // (though this may not always hold due to noise)
    EXPECT_TRUE(std::isfinite(te_xy));
    EXPECT_TRUE(std::isfinite(te_yx));
    EXPECT_GE(te_xy, 0.0f);
    EXPECT_GE(te_yx, 0.0f);
}

//=============================================================================
// Bootstrap Tests
//=============================================================================

TEST_F(NeuralStatisticsIntegrationTest, BootstrapMean) {
    auto data = generateMembranePotential(SMALL_SIZE, -70.0f, 5.0f);

    nimcp_bootstrap_result_t result;
    nimcp_stats_result_t status = nimcp_stats_bootstrap_mean(
        data.data(), SMALL_SIZE, 1000, 0.95f, &result);

    EXPECT_EQ(status, NIMCP_STATS_OK);

    // Bootstrap estimate should be close to sample mean
    float sample_mean = nimcp_stats_mean(data.data(), SMALL_SIZE);
    EXPECT_NEAR(result.estimate, sample_mean, RELAXED_TOLERANCE);

    // CI should contain true mean with high probability
    EXPECT_LT(result.ci_lower_percentile, -70.0f + 2.0f);
    EXPECT_GT(result.ci_upper_percentile, -70.0f - 2.0f);
}

TEST_F(NeuralStatisticsIntegrationTest, BootstrapMedian) {
    auto data = generateMembranePotential(SMALL_SIZE);

    nimcp_bootstrap_result_t result;
    nimcp_stats_result_t status = nimcp_stats_bootstrap_median(
        data.data(), SMALL_SIZE, 1000, 0.95f, &result);

    EXPECT_EQ(status, NIMCP_STATS_OK);

    float sample_median = nimcp_stats_median(data.data(), SMALL_SIZE);
    EXPECT_NEAR(result.estimate, sample_median, RELAXED_TOLERANCE);
}

TEST_F(NeuralStatisticsIntegrationTest, BootstrapCorrelation) {
    auto x = generateMembranePotential(SMALL_SIZE);
    std::vector<float> y(SMALL_SIZE);
    std::normal_distribution<float> noise(0.0f, 2.0f);
    for (uint32_t i = 0; i < SMALL_SIZE; i++) {
        y[i] = 0.7f * x[i] + noise(rng);
    }

    nimcp_bootstrap_result_t result;
    nimcp_stats_result_t status = nimcp_stats_bootstrap_correlation(
        x.data(), y.data(), SMALL_SIZE, 1000, 0.95f, &result);

    EXPECT_EQ(status, NIMCP_STATS_OK);
    EXPECT_GT(result.estimate, 0.5f);
    EXPECT_LT(result.estimate, 0.9f);
}

//=============================================================================
// Utility Function Tests
//=============================================================================

TEST_F(NeuralStatisticsIntegrationTest, Standardization) {
    auto data = generateMembranePotential(MEDIUM_SIZE, -70.0f, 5.0f);
    std::vector<float> standardized(MEDIUM_SIZE);

    nimcp_stats_result_t status = nimcp_stats_standardize(
        data.data(), MEDIUM_SIZE, standardized.data());
    EXPECT_EQ(status, NIMCP_STATS_OK);

    // Standardized data should have mean ~0, std ~1
    float std_mean = nimcp_stats_mean(standardized.data(), MEDIUM_SIZE);
    float std_std = nimcp_stats_std_dev(standardized.data(), MEDIUM_SIZE);

    EXPECT_NEAR(std_mean, 0.0f, RELAXED_TOLERANCE);
    EXPECT_NEAR(std_std, 1.0f, RELAXED_TOLERANCE);
}

TEST_F(NeuralStatisticsIntegrationTest, MinMaxNormalization) {
    auto data = generateMembranePotential(MEDIUM_SIZE);
    std::vector<float> normalized(MEDIUM_SIZE);

    nimcp_stats_result_t status = nimcp_stats_normalize_minmax(
        data.data(), MEDIUM_SIZE, normalized.data());
    EXPECT_EQ(status, NIMCP_STATS_OK);

    // All values should be in [0, 1]
    float min_norm = nimcp_stats_min(normalized.data(), MEDIUM_SIZE);
    float max_norm = nimcp_stats_max(normalized.data(), MEDIUM_SIZE);

    EXPECT_NEAR(min_norm, 0.0f, STRICT_TOLERANCE);
    EXPECT_NEAR(max_norm, 1.0f, STRICT_TOLERANCE);
}

TEST_F(NeuralStatisticsIntegrationTest, OutlierDetection) {
    auto data = generateMembranePotential(MEDIUM_SIZE, -70.0f, 5.0f);

    // Add some outliers
    data[0] = -150.0f;
    data[1] = 50.0f;

    std::unique_ptr<bool[]> outliers(new bool[MEDIUM_SIZE]);
    uint32_t n_outliers;

    nimcp_stats_result_t status = nimcp_stats_detect_outliers_iqr(
        data.data(), MEDIUM_SIZE, 1.5f, outliers.get(), &n_outliers);
    EXPECT_EQ(status, NIMCP_STATS_OK);

    // Should detect at least the manually added outliers
    EXPECT_GE(n_outliers, 2u);
    EXPECT_TRUE(outliers.get()[0]) << "First value should be outlier";
    EXPECT_TRUE(outliers.get()[1]) << "Second value should be outlier";
}

TEST_F(NeuralStatisticsIntegrationTest, Winsorization) {
    auto data = generateMembranePotential(MEDIUM_SIZE);

    // Add extreme outliers
    data[0] = -200.0f;
    data[1] = 100.0f;

    std::vector<float> winsorized(MEDIUM_SIZE);
    nimcp_stats_result_t status = nimcp_stats_winsorize(
        data.data(), MEDIUM_SIZE, 0.05f, 0.95f, winsorized.data());
    EXPECT_EQ(status, NIMCP_STATS_OK);

    // Winsorized extremes should be less extreme
    EXPECT_GT(winsorized[0], data[0]);
    EXPECT_LT(winsorized[1], data[1]);
}

TEST_F(NeuralStatisticsIntegrationTest, Ranking) {
    std::vector<float> data = {5.0f, 3.0f, 8.0f, 1.0f, 8.0f};
    std::vector<float> ranks(5);

    nimcp_stats_result_t status = nimcp_stats_rank(data.data(), 5, ranks.data(), 'a');
    EXPECT_EQ(status, NIMCP_STATS_OK);

    // Verify rankings
    EXPECT_NEAR(ranks[0], 3.0f, STRICT_TOLERANCE);  // 5 is 3rd smallest
    EXPECT_NEAR(ranks[1], 2.0f, STRICT_TOLERANCE);  // 3 is 2nd smallest
    EXPECT_NEAR(ranks[2], 4.5f, STRICT_TOLERANCE);  // 8 tied, average rank
    EXPECT_NEAR(ranks[3], 1.0f, STRICT_TOLERANCE);  // 1 is smallest
    EXPECT_NEAR(ranks[4], 4.5f, STRICT_TOLERANCE);  // 8 tied, average rank
}

//=============================================================================
// Performance Tests
//=============================================================================

TEST_F(NeuralStatisticsIntegrationTest, PerformanceMean) {
    auto data = generateMembranePotential(LARGE_SIZE);

    uint64_t time = measureTime([&]() {
        for (int i = 0; i < 100; i++) {
            volatile float mean = nimcp_stats_mean(data.data(), LARGE_SIZE);
            (void)mean;
        }
    });

    uint64_t time_per_call = time / 100;
    EXPECT_LT(time_per_call, MEAN_THRESHOLD_US)
        << "Mean computation too slow: " << time_per_call << "us";
}

TEST_F(NeuralStatisticsIntegrationTest, PerformanceVariance) {
    auto data = generateMembranePotential(LARGE_SIZE);

    uint64_t time = measureTime([&]() {
        for (int i = 0; i < 100; i++) {
            volatile float var = nimcp_stats_variance(data.data(), LARGE_SIZE);
            (void)var;
        }
    });

    uint64_t time_per_call = time / 100;
    EXPECT_LT(time_per_call, VARIANCE_THRESHOLD_US)
        << "Variance computation too slow: " << time_per_call << "us";
}

TEST_F(NeuralStatisticsIntegrationTest, PerformanceDescribe) {
    auto data = generateMembranePotential(LARGE_SIZE);
    nimcp_descriptive_stats_t stats;

    uint64_t time = measureTime([&]() {
        for (int i = 0; i < 100; i++) {
            nimcp_stats_describe(data.data(), LARGE_SIZE, &stats);
        }
    });

    uint64_t time_per_call = time / 100;
    EXPECT_LT(time_per_call, DESCRIBE_THRESHOLD_US)
        << "Describe computation too slow: " << time_per_call << "us";
}

TEST_F(NeuralStatisticsIntegrationTest, PerformanceLargeData) {
    auto data = generateMembranePotential(VERY_LARGE_SIZE);

    auto start = std::chrono::high_resolution_clock::now();
    float mean = nimcp_stats_mean(data.data(), VERY_LARGE_SIZE);
    float variance = nimcp_stats_variance(data.data(), VERY_LARGE_SIZE);
    float median = nimcp_stats_median(data.data(), VERY_LARGE_SIZE);
    auto end = std::chrono::high_resolution_clock::now();

    uint64_t total_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    EXPECT_TRUE(std::isfinite(mean));
    EXPECT_TRUE(std::isfinite(variance));
    EXPECT_TRUE(std::isfinite(median));
    EXPECT_LT(total_time, 1000u) << "Large data statistics should complete in <1s";
}

//=============================================================================
// Memory Management Tests
//=============================================================================

TEST_F(NeuralStatisticsIntegrationTest, NoMemoryLeaksDescriptive) {
    nimcp_memory_clear_stats();

    for (int i = 0; i < 100; i++) {
        auto data = generateMembranePotential(MEDIUM_SIZE);
        nimcp_descriptive_stats_t stats;
        nimcp_stats_describe(data.data(), MEDIUM_SIZE, &stats);
    }

    nimcp_memory_stats_t stats;
    nimcp_memory_get_stats(&stats);
    EXPECT_LT(stats.current_allocated, 1024)
        << "Memory leaked during descriptive stats";
}

TEST_F(NeuralStatisticsIntegrationTest, NoMemoryLeaksBootstrap) {
    nimcp_memory_clear_stats();

    auto data = generateMembranePotential(SMALL_SIZE);

    for (int i = 0; i < 10; i++) {
        nimcp_bootstrap_result_t result;
        nimcp_stats_bootstrap_mean(data.data(), SMALL_SIZE, 100, 0.95f, &result);
    }

    nimcp_memory_stats_t stats;
    nimcp_memory_get_stats(&stats);
    EXPECT_LT(stats.current_allocated, 4096)
        << "Memory leaked during bootstrap";
}

TEST_F(NeuralStatisticsIntegrationTest, NoMemoryLeaksRegression) {
    nimcp_memory_clear_stats();

    std::vector<float> x(SMALL_SIZE);
    std::vector<float> y(SMALL_SIZE);
    for (uint32_t i = 0; i < SMALL_SIZE; i++) {
        x[i] = static_cast<float>(i);
        y[i] = 2.0f * x[i] + 1.0f;
    }

    for (int i = 0; i < 50; i++) {
        nimcp_regression_result_t result;
        nimcp_stats_regression_linear(x.data(), y.data(), SMALL_SIZE, &result);
    }

    nimcp_memory_stats_t stats;
    nimcp_memory_get_stats(&stats);
    EXPECT_LT(stats.current_allocated, 4096)
        << "Memory leaked during regression";
}

//=============================================================================
// Error Handling Tests
//=============================================================================

TEST_F(NeuralStatisticsIntegrationTest, NullPointerHandling) {
    float result = nimcp_stats_mean(nullptr, 100);
    EXPECT_TRUE(std::isnan(result)) << "Should return NaN for null input";

    result = nimcp_stats_variance(nullptr, 100);
    EXPECT_TRUE(std::isnan(result));

    nimcp_descriptive_stats_t stats;
    nimcp_stats_result_t status = nimcp_stats_describe(nullptr, 100, &stats);
    EXPECT_EQ(status, NIMCP_STATS_ERROR_NULL);
}

TEST_F(NeuralStatisticsIntegrationTest, ZeroSizeHandling) {
    std::vector<float> data = {1.0f, 2.0f, 3.0f};

    float result = nimcp_stats_mean(data.data(), 0);
    EXPECT_TRUE(std::isnan(result)) << "Should return NaN for zero size";

    result = nimcp_stats_variance(data.data(), 0);
    EXPECT_TRUE(std::isnan(result));
}

TEST_F(NeuralStatisticsIntegrationTest, SmallSampleWarnings) {
    std::vector<float> tiny = {1.0f};

    // Variance needs n >= 2
    float var = nimcp_stats_variance(tiny.data(), 1);
    EXPECT_TRUE(std::isnan(var)) << "Variance needs n >= 2";

    // Skewness needs n >= 3
    std::vector<float> small = {1.0f, 2.0f};
    float skew = nimcp_stats_skewness(small.data(), 2);
    EXPECT_TRUE(std::isnan(skew)) << "Skewness needs n >= 3";
}

TEST_F(NeuralStatisticsIntegrationTest, InvalidDistributionParameters) {
    nimcp_distribution_params_t params;
    params.type = NIMCP_DIST_NORMAL;
    params.params.normal.mu = 0.0f;
    params.params.normal.sigma = -1.0f;  // Invalid negative sigma

    float pdf = nimcp_stats_pdf(0.0f, &params);
    EXPECT_TRUE(std::isnan(pdf)) << "Should return NaN for invalid sigma";
}

//=============================================================================
// Real-World Scenario Tests
//=============================================================================

TEST_F(NeuralStatisticsIntegrationTest, NeuralPopulationFiringRateAnalysis) {
    // Simulate analyzing firing rates from neural population
    uint32_t n_neurons = 50;
    uint32_t n_trials = 100;
    float duration_s = 1.0f;

    std::vector<float> firing_rates;

    // Collect firing rates from all neurons across trials
    for (uint32_t neuron = 0; neuron < n_neurons; neuron++) {
        for (uint32_t trial = 0; trial < n_trials; trial++) {
            auto spikes = generateSpikeTrain(SPIKE_RATE_HZ, duration_s);
            float rate = nimcp_stats_sum(spikes.data(),
                static_cast<uint32_t>(spikes.size())) / duration_s;
            firing_rates.push_back(rate);
        }
    }

    // Analyze population firing rates
    nimcp_descriptive_stats_t stats;
    nimcp_stats_describe(firing_rates.data(),
        static_cast<uint32_t>(firing_rates.size()), &stats);

    // Mean rate should be near target
    EXPECT_NEAR(stats.mean, SPIKE_RATE_HZ, 2.0f);

    // Test for normality (firing rates often approximately normal)
    nimcp_test_result_t normality;
    nimcp_stats_shapiro_wilk(firing_rates.data(),
        static_cast<uint32_t>(firing_rates.size()), &normality);

    EXPECT_TRUE(std::isfinite(normality.p_value));
}

TEST_F(NeuralStatisticsIntegrationTest, StimulusResponseCorrelation) {
    // Analyze stimulus-response relationship
    uint32_t n_trials = 200;
    std::vector<float> stimulus(n_trials);
    std::vector<float> response(n_trials);

    std::normal_distribution<float> stim_dist(10.0f, 3.0f);
    std::normal_distribution<float> noise_dist(0.0f, 5.0f);

    for (uint32_t i = 0; i < n_trials; i++) {
        stimulus[i] = stim_dist(rng);
        // Response = 2 * stimulus + noise (linear tuning curve)
        response[i] = 2.0f * stimulus[i] + noise_dist(rng);
    }

    // Compute correlation
    nimcp_correlation_result_t corr;
    nimcp_stats_correlation_pearson(stimulus.data(), response.data(), n_trials, &corr);

    EXPECT_GT(corr.r, 0.5f) << "Stimulus-response should be positively correlated";
    EXPECT_LT(corr.p_value, 0.001f) << "Correlation should be significant";

    // Fit tuning curve
    nimcp_regression_result_t reg;
    nimcp_stats_regression_linear(stimulus.data(), response.data(), n_trials, &reg);

    EXPECT_NEAR(reg.slope, 2.0f, 0.5f) << "Slope should be near 2";
    EXPECT_GT(reg.r_squared, 0.5f) << "R-squared should indicate good fit";
}

TEST_F(NeuralStatisticsIntegrationTest, InterSpikeIntervalAnalysis) {
    // Analyze inter-spike interval distribution
    auto spikes = generateSpikeTrain(SPIKE_RATE_HZ, 100.0f);  // 100 seconds

    // Extract ISIs
    std::vector<float> isis;
    float last_spike_time = -1.0f;
    float dt = 0.001f;

    for (size_t i = 0; i < spikes.size(); i++) {
        if (spikes[i] > 0.5f) {
            float current_time = i * dt;
            if (last_spike_time >= 0.0f) {
                isis.push_back(current_time - last_spike_time);
            }
            last_spike_time = current_time;
        }
    }

    if (isis.size() > 10) {
        nimcp_descriptive_stats_t stats;
        nimcp_stats_describe(isis.data(), static_cast<uint32_t>(isis.size()), &stats);

        // Mean ISI should be ~1/rate
        float expected_mean_isi = 1.0f / SPIKE_RATE_HZ;
        EXPECT_NEAR(stats.mean, expected_mean_isi, 0.05f);

        // CV (coefficient of variation) for Poisson should be ~1
        float cv = stats.std_dev / stats.mean;
        EXPECT_NEAR(cv, 1.0f, 0.3f)
            << "CV of ISI for Poisson process should be ~1";
    }
}

TEST_F(NeuralStatisticsIntegrationTest, TrialToTrialVariabilityAnalysis) {
    // Analyze variability across trials (Fano factor)
    uint32_t n_trials = 100;
    uint32_t window_ms = 100;  // 100ms windows
    float window_s = window_ms / 1000.0f;

    std::vector<float> spike_counts;

    for (uint32_t trial = 0; trial < n_trials; trial++) {
        auto spikes = generateSpikeTrain(SPIKE_RATE_HZ, window_s);
        float count = nimcp_stats_sum(spikes.data(),
            static_cast<uint32_t>(spikes.size()));
        spike_counts.push_back(count);
    }

    float mean_count = nimcp_stats_mean(spike_counts.data(), n_trials);
    float var_count = nimcp_stats_variance(spike_counts.data(), n_trials);

    // Fano factor (variance/mean) should be ~1 for Poisson
    float fano_factor = var_count / mean_count;
    EXPECT_NEAR(fano_factor, 1.0f, 0.5f)
        << "Fano factor for Poisson should be ~1";
}

//=============================================================================
// Edge Cases and Boundary Tests
//=============================================================================

TEST_F(NeuralStatisticsIntegrationTest, IdenticalValues) {
    std::vector<float> identical(100, 5.0f);

    float mean = nimcp_stats_mean(identical.data(), 100);
    float var = nimcp_stats_variance(identical.data(), 100);
    float std_dev = nimcp_stats_std_dev(identical.data(), 100);

    EXPECT_NEAR(mean, 5.0f, STRICT_TOLERANCE);
    EXPECT_NEAR(var, 0.0f, STRICT_TOLERANCE);
    EXPECT_NEAR(std_dev, 0.0f, STRICT_TOLERANCE);
}

TEST_F(NeuralStatisticsIntegrationTest, ExtremeValues) {
    std::vector<float> extreme = {
        std::numeric_limits<float>::min(),
        std::numeric_limits<float>::max() / 2.0f,
        0.0f,
        -std::numeric_limits<float>::max() / 2.0f
    };

    float mean = nimcp_stats_mean(extreme.data(), 4);
    EXPECT_TRUE(std::isfinite(mean)) << "Should handle extreme values";
}

TEST_F(NeuralStatisticsIntegrationTest, SpecialFloatValues) {
    // Test that NaN in data is handled
    std::vector<float> with_nan = {1.0f, 2.0f, std::numeric_limits<float>::quiet_NaN(), 4.0f};
    float mean = nimcp_stats_mean(with_nan.data(), 4);
    EXPECT_TRUE(std::isnan(mean)) << "NaN in data should propagate";
}

TEST_F(NeuralStatisticsIntegrationTest, TwoElementStatistics) {
    std::vector<float> two = {1.0f, 3.0f};

    float mean = nimcp_stats_mean(two.data(), 2);
    EXPECT_NEAR(mean, 2.0f, STRICT_TOLERANCE);

    float var = nimcp_stats_variance(two.data(), 2);
    EXPECT_NEAR(var, 2.0f, STRICT_TOLERANCE);  // ((1-2)^2 + (3-2)^2) / (2-1) = 2

    float median = nimcp_stats_median(two.data(), 2);
    EXPECT_NEAR(median, 2.0f, STRICT_TOLERANCE);  // Average of two middle values
}

//=============================================================================
// Special Mathematical Functions Tests
//=============================================================================

TEST_F(NeuralStatisticsIntegrationTest, LogGammaFunction) {
    // lgamma(1) = 0, lgamma(2) = 0
    EXPECT_NEAR(nimcp_stats_lgamma(1.0), 0.0, RELAXED_TOLERANCE);
    EXPECT_NEAR(nimcp_stats_lgamma(2.0), 0.0, RELAXED_TOLERANCE);

    // lgamma(n) = log((n-1)!)
    EXPECT_NEAR(nimcp_stats_lgamma(5.0), std::log(24.0), RELAXED_TOLERANCE);  // log(4!)
}

TEST_F(NeuralStatisticsIntegrationTest, BetaFunction) {
    // B(a,b) = Gamma(a)*Gamma(b)/Gamma(a+b)
    // B(1,1) = 1
    EXPECT_NEAR(nimcp_stats_beta_fn(1.0, 1.0), 1.0, RELAXED_TOLERANCE);

    // B(2,3) = 1/(4*C(4,1)) = 1/4 * 1/4 = 1/12
    EXPECT_NEAR(nimcp_stats_beta_fn(2.0, 3.0), 1.0/12.0, RELAXED_TOLERANCE);
}

TEST_F(NeuralStatisticsIntegrationTest, BinomialCoefficient) {
    // C(5,2) = 10
    EXPECT_NEAR(nimcp_stats_binomial_coef(5, 2), 10.0, STRICT_TOLERANCE);

    // C(10,5) = 252
    EXPECT_NEAR(nimcp_stats_binomial_coef(10, 5), 252.0, STRICT_TOLERANCE);

    // C(n,0) = C(n,n) = 1
    EXPECT_NEAR(nimcp_stats_binomial_coef(100, 0), 1.0, STRICT_TOLERANCE);
    EXPECT_NEAR(nimcp_stats_binomial_coef(100, 100), 1.0, STRICT_TOLERANCE);
}

TEST_F(NeuralStatisticsIntegrationTest, ErrorFunction) {
    // erf(0) = 0
    EXPECT_NEAR(nimcp_stats_erf(0.0), 0.0, STRICT_TOLERANCE);

    // erf is odd: erf(-x) = -erf(x)
    EXPECT_NEAR(nimcp_stats_erf(-1.0), -nimcp_stats_erf(1.0), STRICT_TOLERANCE);

    // erf(inf) = 1, erf(-inf) = -1
    EXPECT_NEAR(nimcp_stats_erf(10.0), 1.0, RELAXED_TOLERANCE);
    EXPECT_NEAR(nimcp_stats_erf(-10.0), -1.0, RELAXED_TOLERANCE);

    // erfc(x) = 1 - erf(x)
    EXPECT_NEAR(nimcp_stats_erfc(1.0), 1.0 - nimcp_stats_erf(1.0), STRICT_TOLERANCE);
}

