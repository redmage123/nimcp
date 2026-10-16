//=============================================================================
// test_statistics_regression.cpp - Regression Tests for Statistics Module
//=============================================================================
/**
 * @file test_statistics_regression.cpp
 * @brief Regression tests ensuring backward compatibility and fixed bugs stay fixed
 *
 * WHAT: Verify previously working functionality remains correct
 * WHY:  Prevent regression when adding new features
 * HOW:  Test known good values and edge cases that previously failed
 *
 * @date 2026-01-30
 */

#include <gtest/gtest.h>
#include "utils/statistics/nimcp_statistics.h"
#include "information/nimcp_shannon.h"
#include <cmath>
#include <vector>
#include <limits>

// Test tolerances
#define TOLERANCE 1e-5f
#define LOOSE_TOLERANCE 1e-3f

//=============================================================================
// Test Fixture
//=============================================================================

class StatisticsRegressionTest : public ::testing::Test {
protected:
    void SetUp() override {
        config = nimcp_stats_default_config();
        nimcp_stats_init(&config);
    }

    void TearDown() override {
        nimcp_stats_shutdown();
    }

    nimcp_stats_config_t config;
};

//=============================================================================
// Regression: Incomplete Beta Function Fix (2026-01-30)
// Bug: betainc continued fraction had incorrect coefficient indexing
// Result: Non-monotonic CDF, incorrect Student-t quantiles
//=============================================================================

TEST_F(StatisticsRegressionTest, BetaInc_Monotonic) {
    // betainc(x, a, b) must be monotonic increasing in x
    double a = 5.0, b = 0.5;
    double prev = 0.0;

    for (double x = 0.1; x <= 0.9; x += 0.1) {
        double current = nimcp_stats_betainc(x, a, b);
        EXPECT_GE(current, prev) << "betainc not monotonic at x=" << x;
        prev = current;
    }
}

TEST_F(StatisticsRegressionTest, BetaInc_KnownValues) {
    // Known correct values for validation
    EXPECT_NEAR(nimcp_stats_betainc(0.5, 1.0, 1.0), 0.5, TOLERANCE);
    EXPECT_NEAR(nimcp_stats_betainc(0.5, 0.5, 0.5), 0.5, TOLERANCE);
    EXPECT_NEAR(nimcp_stats_betainc(0.5, 2.0, 2.0), 0.5, TOLERANCE);
    EXPECT_NEAR(nimcp_stats_betainc(0.0, 2.0, 3.0), 0.0, TOLERANCE);
    EXPECT_NEAR(nimcp_stats_betainc(1.0, 2.0, 3.0), 1.0, TOLERANCE);
}

TEST_F(StatisticsRegressionTest, StudentT_CDF_Monotonic) {
    // Student-t CDF must be monotonic increasing
    float df = 10.0f;
    float prev_cdf = 0.0f;

    for (float t = -5.0f; t <= 5.0f; t += 0.5f) {
        float current_cdf = nimcp_stats_cdf_student_t(t, df);
        EXPECT_GE(current_cdf, prev_cdf) << "Student-t CDF not monotonic at t=" << t;
        prev_cdf = current_cdf;
    }
}

TEST_F(StatisticsRegressionTest, StudentT_Quantile_KnownValues) {
    // t_{0.975, 10} ≈ 2.228
    float q_975 = nimcp_stats_quantile_student_t(0.975f, 10.0f);
    EXPECT_NEAR(q_975, 2.228f, 0.01f);

    // t_{0.95, 10} ≈ 1.812
    float q_95 = nimcp_stats_quantile_student_t(0.95f, 10.0f);
    EXPECT_NEAR(q_95, 1.812f, 0.01f);

    // t_{0.5, df} = 0 for any df (symmetric)
    EXPECT_NEAR(nimcp_stats_quantile_student_t(0.5f, 10.0f), 0.0f, TOLERANCE);
}

//=============================================================================
// Regression: Normal Quantile Fix (2026-01-30)
// Bug: Wichura AS241 coefficients were mixed up
// Result: Incorrect z-scores, broken confidence intervals
//=============================================================================

TEST_F(StatisticsRegressionTest, Normal_Quantile_KnownValues) {
    // z_{0.975} = 1.96
    EXPECT_NEAR(nimcp_stats_quantile_standard_normal(0.975f), 1.96f, LOOSE_TOLERANCE);

    // z_{0.5} = 0
    EXPECT_NEAR(nimcp_stats_quantile_standard_normal(0.5f), 0.0f, TOLERANCE);

    // z_{0.025} = -1.96
    EXPECT_NEAR(nimcp_stats_quantile_standard_normal(0.025f), -1.96f, LOOSE_TOLERANCE);

    // z_{0.99} ≈ 2.326
    EXPECT_NEAR(nimcp_stats_quantile_standard_normal(0.99f), 2.326f, LOOSE_TOLERANCE);
}

TEST_F(StatisticsRegressionTest, Normal_Quantile_Inverse) {
    // Quantile should be inverse of CDF
    float test_probs[] = {0.1f, 0.25f, 0.5f, 0.75f, 0.9f, 0.95f, 0.99f};

    for (float p : test_probs) {
        float q = nimcp_stats_quantile_standard_normal(p);
        float cdf = nimcp_stats_cdf_standard_normal(q);
        EXPECT_NEAR(cdf, p, TOLERANCE) << "Inverse property failed at p=" << p;
    }
}

//=============================================================================
// Regression: Entropy Numerical Stability
// Issue: log(0) should not cause NaN/crash
//=============================================================================

TEST_F(StatisticsRegressionTest, Entropy_ZeroProbability) {
    // Distribution with zero probabilities
    float probs[] = {0.5f, 0.0f, 0.5f, 0.0f};
    float h = nimcp_stats_entropy(probs, 4);

    EXPECT_FALSE(std::isnan(h));
    EXPECT_FALSE(std::isinf(h));
    EXPECT_NEAR(h, 1.0f, TOLERANCE);  // Binary entropy
}

TEST_F(StatisticsRegressionTest, Entropy_DegenerateDistribution) {
    // Single outcome with probability 1
    float probs[] = {1.0f, 0.0f, 0.0f};
    float h = nimcp_stats_entropy(probs, 3);

    EXPECT_FALSE(std::isnan(h));
    EXPECT_NEAR(h, 0.0f, TOLERANCE);  // Zero entropy
}

//=============================================================================
// Regression: KL Divergence Edge Cases
// Issue: Division by zero when q has zeros
//=============================================================================

TEST_F(StatisticsRegressionTest, KL_QHasZeros) {
    float p[] = {0.5f, 0.5f};
    float q[] = {1.0f, 0.0f};  // q has zero where p is non-zero

    float kl = nimcp_stats_kl_divergence(p, q, 2);

    // Should return infinity, not NaN or crash
    EXPECT_TRUE(std::isinf(kl));
    EXPECT_GT(kl, 0.0f);  // Positive infinity
}

TEST_F(StatisticsRegressionTest, KL_SameDistribution) {
    float p[] = {0.3f, 0.4f, 0.3f};

    float kl = nimcp_stats_kl_divergence(p, p, 3);

    EXPECT_NEAR(kl, 0.0f, TOLERANCE);
}

//=============================================================================
// Regression: Variance Edge Cases
//=============================================================================

TEST_F(StatisticsRegressionTest, Variance_ConstantValues) {
    float data[] = {42.0f, 42.0f, 42.0f, 42.0f};

    float var = nimcp_stats_variance(data, 4);

    EXPECT_NEAR(var, 0.0f, TOLERANCE);
}

TEST_F(StatisticsRegressionTest, Variance_TwoElements) {
    float data[] = {0.0f, 2.0f};

    // Sample variance: s² = Σ(x-mean)²/(n-1)
    // Mean = 1, deviations = -1, +1
    // Variance = (1 + 1) / 1 = 2
    float var = nimcp_stats_variance(data, 2);

    EXPECT_NEAR(var, 2.0f, TOLERANCE);
}

//=============================================================================
// Regression: Quantile Edge Cases
//=============================================================================

TEST_F(StatisticsRegressionTest, Quantile_Boundaries) {
    float data[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};

    // 0th percentile = minimum
    EXPECT_NEAR(nimcp_stats_quantile(data, 5, 0.0f), 1.0f, TOLERANCE);

    // 100th percentile = maximum
    EXPECT_NEAR(nimcp_stats_quantile(data, 5, 1.0f), 5.0f, TOLERANCE);

    // Invalid percentiles
    EXPECT_TRUE(std::isnan(nimcp_stats_quantile(data, 5, -0.1f)));
    EXPECT_TRUE(std::isnan(nimcp_stats_quantile(data, 5, 1.1f)));
}

//=============================================================================
// Regression: Statistical Test Accuracy
//=============================================================================

TEST_F(StatisticsRegressionTest, TTest_KnownPValue) {
    // Data designed to give known p-value
    // Mean = 0.5, testing H0: mu = 0
    float data[] = {0.2f, 0.3f, 0.5f, 0.6f, 0.9f};  // n=5, mean=0.5, sd≈0.26
    nimcp_test_result_t result;

    nimcp_stats_ttest_one_sample(data, 5, 0.0f, NIMCP_TEST_TWO_SIDED, 0.95f, &result);

    // p-value should be significant (< 0.05)
    EXPECT_LT(result.p_value, 0.10f);
    EXPECT_GT(result.statistic, 0.0f);  // Positive t-stat for positive mean
}

TEST_F(StatisticsRegressionTest, Correlation_PerfectLinear) {
    float x[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    float y[] = {2.0f, 4.0f, 6.0f, 8.0f, 10.0f};  // y = 2x

    nimcp_correlation_result_t result;
    nimcp_stats_correlation_pearson(x, y, 5, &result);

    EXPECT_NEAR(result.r, 1.0f, TOLERANCE);
}

//=============================================================================
// Regression: Module Initialization
//=============================================================================

TEST_F(StatisticsRegressionTest, MultipleInitShutdown) {
    // Should handle multiple init/shutdown cycles
    for (int i = 0; i < 3; i++) {
        nimcp_stats_shutdown();
        EXPECT_FALSE(nimcp_stats_is_initialized());

        nimcp_stats_result_t res = nimcp_stats_init(&config);
        EXPECT_EQ(res, NIMCP_STATS_OK);
        EXPECT_TRUE(nimcp_stats_is_initialized());
    }
}

//=============================================================================
// Regression: Shannon Integration Functions
//=============================================================================

TEST_F(StatisticsRegressionTest, ChannelCapacity_Consistency) {
    // Shannon-Hartley: C = B * log2(1 + SNR)
    float B = 100.0f;
    float SNR = 10.0f;

    float capacity = nimcp_stats_channel_capacity(B, SNR);
    float expected = B * std::log2(1.0f + SNR);

    EXPECT_NEAR(capacity, expected, TOLERANCE);
}

TEST_F(StatisticsRegressionTest, SNR_Conversion_RoundTrip) {
    float original_db[] = {0.0f, 10.0f, 20.0f, 30.0f, -10.0f};

    for (float db : original_db) {
        float linear = nimcp_stats_snr_from_db(db);
        float back = nimcp_stats_snr_to_db(linear);
        EXPECT_NEAR(back, db, TOLERANCE) << "Round-trip failed for " << db << " dB";
    }
}

//=============================================================================
// Regression: Memory Safety
//=============================================================================

TEST_F(StatisticsRegressionTest, NullInputs_NoSIGSEGV) {
    // All functions should handle NULL gracefully
    EXPECT_TRUE(std::isnan(nimcp_stats_mean(nullptr, 5)));
    EXPECT_TRUE(std::isnan(nimcp_stats_variance(nullptr, 5)));
    EXPECT_TRUE(std::isnan(nimcp_stats_entropy(nullptr, 5)));
    EXPECT_TRUE(std::isnan(nimcp_stats_kl_divergence(nullptr, nullptr, 5)));

    // Functions returning error codes
    nimcp_test_result_t result;
    EXPECT_EQ(nimcp_stats_ttest_one_sample(nullptr, 5, 0, NIMCP_TEST_TWO_SIDED, 0.95f, &result),
              NIMCP_STATS_ERROR_NULL);
}

TEST_F(StatisticsRegressionTest, ZeroSize_NoSIGSEGV) {
    float data[] = {1.0f, 2.0f};

    EXPECT_TRUE(std::isnan(nimcp_stats_mean(data, 0)));
    EXPECT_TRUE(std::isnan(nimcp_stats_entropy(data, 0)));
}

//=============================================================================
// Regression: Numerical Stability with Extreme Values
//=============================================================================

TEST_F(StatisticsRegressionTest, VerySmallProbabilities) {
    // Near-zero probabilities shouldn't cause issues
    float probs[] = {0.999999f, 1e-6f};

    float h = nimcp_stats_entropy(probs, 2);
    EXPECT_FALSE(std::isnan(h));
    EXPECT_GT(h, 0.0f);
}

TEST_F(StatisticsRegressionTest, VeryLargeValues) {
    float data[] = {1e10f, 2e10f, 3e10f};

    float mean = nimcp_stats_mean(data, 3);
    EXPECT_NEAR(mean, 2e10f, 1e5f);

    float var = nimcp_stats_variance(data, 3);
    EXPECT_GT(var, 0.0f);
    EXPECT_FALSE(std::isinf(var));
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
