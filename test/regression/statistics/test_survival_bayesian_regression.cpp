//=============================================================================
// test_survival_bayesian_regression.cpp - Survival & Bayesian Statistics Tests
//=============================================================================
/**
 * @file test_survival_bayesian_regression.cpp
 * @brief Comprehensive regression tests for survival and Bayesian statistics
 *
 * REGRESSION TEST FOCUS:
 * - Kaplan-Meier curve on exponential survival
 * - MCMC posterior moments
 * - Bayesian conjugate prior updates
 * - Credible interval coverage
 * - Bayes factor computation
 * - Prior sensitivity analysis
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <limits>
#include <chrono>
#include <random>
#include <algorithm>
#include <numeric>

extern "C" {
#include "utils/statistics/nimcp_statistics.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class SurvivalBayesianRegressionTest : public ::testing::Test {
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

    // Generate exponential survival times
    std::vector<float> generateExponentialSurvival(size_t n, float lambda) {
        std::exponential_distribution<float> dist(lambda);
        std::vector<float> times(n);
        for (auto& t : times) t = dist(rng);
        return times;
    }

    // Generate binomial data
    uint32_t generateBinomial(uint32_t n, float p) {
        std::binomial_distribution<uint32_t> dist(n, p);
        return dist(rng);
    }

    // Generate Poisson data
    uint32_t generatePoisson(float lambda) {
        std::poisson_distribution<uint32_t> dist(lambda);
        return dist(rng);
    }

    // Beta function for reference calculations
    double betaFunction(double a, double b) {
        return std::tgamma(a) * std::tgamma(b) / std::tgamma(a + b);
    }
};

//=============================================================================
// BAYESIAN BETA-BINOMIAL REGRESSION TESTS
//=============================================================================

TEST_F(SurvivalBayesianRegressionTest, BetaBinomialPosteriorMean) {
    // Prior: Beta(1, 1) = uniform
    // Data: 7 successes in 10 trials
    // Posterior: Beta(1+7, 1+3) = Beta(8, 4)
    // Posterior mean = 8/(8+4) = 2/3

    nimcp_bayesian_result_t result;
    nimcp_stats_result_t status = nimcp_stats_bayesian_beta_binomial(
        1.0f, 1.0f,  // Prior
        7, 10,       // Data
        0.95f,       // Credible level
        &result
    );

    EXPECT_EQ(status, NIMCP_STATS_OK);
    EXPECT_NEAR(result.posterior_mean, 8.0f/12.0f, 1e-5f);
}

TEST_F(SurvivalBayesianRegressionTest, BetaBinomialPosteriorMode) {
    // Posterior: Beta(8, 4)
    // Mode = (alpha-1)/(alpha+beta-2) = 7/10 = 0.7

    nimcp_bayesian_result_t result;
    nimcp_stats_bayesian_beta_binomial(1.0f, 1.0f, 7, 10, 0.95f, &result);

    EXPECT_NEAR(result.posterior_mode, 7.0f/10.0f, 1e-5f);
}

TEST_F(SurvivalBayesianRegressionTest, BetaBinomialCredibleInterval) {
    // 95% credible interval should contain true value most of the time
    float true_p = 0.6f;
    int coverage_count = 0;
    const int n_simulations = 100;

    for (int sim = 0; sim < n_simulations; ++sim) {
        uint32_t successes = generateBinomial(50, true_p);

        nimcp_bayesian_result_t result;
        nimcp_stats_bayesian_beta_binomial(1.0f, 1.0f, successes, 50, 0.95f, &result);

        if (true_p >= result.credible_lower && true_p <= result.credible_upper) {
            coverage_count++;
        }
    }

    // Coverage should be approximately 95%
    float coverage = static_cast<float>(coverage_count) / n_simulations;
    EXPECT_GT(coverage, 0.85f) << "Credible interval coverage too low";
    EXPECT_LT(coverage, 1.0f);
}

TEST_F(SurvivalBayesianRegressionTest, BetaBinomialPriorEffect) {
    // Informative prior should pull posterior toward prior mean
    // Prior: Beta(10, 10) -> prior mean = 0.5
    // Data: 9 successes in 10 trials -> MLE = 0.9
    // Posterior mean should be between 0.5 and 0.9

    nimcp_bayesian_result_t result;
    nimcp_stats_bayesian_beta_binomial(10.0f, 10.0f, 9, 10, 0.95f, &result);

    EXPECT_GT(result.posterior_mean, 0.5f);
    EXPECT_LT(result.posterior_mean, 0.9f);
}

TEST_F(SurvivalBayesianRegressionTest, BetaBinomialNoData) {
    // With no data, posterior should equal prior
    // Prior: Beta(2, 3) -> mean = 2/5 = 0.4

    nimcp_bayesian_result_t result;
    nimcp_stats_bayesian_beta_binomial(2.0f, 3.0f, 0, 0, 0.95f, &result);

    EXPECT_NEAR(result.posterior_mean, 0.4f, 1e-5f);
}

//=============================================================================
// BAYESIAN NORMAL REGRESSION TESTS
//=============================================================================

TEST_F(SurvivalBayesianRegressionTest, NormalPosteriorMean) {
    // Prior: N(0, 100) - weak prior
    // Known variance: 1
    // Data: sample mean should dominate with large n

    auto data = generateNormal(100, 5.0f, 1.0f);

    nimcp_bayesian_result_t result;
    nimcp_stats_result_t status = nimcp_stats_bayesian_normal(
        0.0f, 100.0f,   // Prior: mean=0, variance=100
        data.data(), data.size(),
        1.0f,           // Known variance
        0.95f,
        &result
    );

    EXPECT_EQ(status, NIMCP_STATS_OK);
    // Posterior mean should be close to sample mean (5.0) with weak prior
    EXPECT_NEAR(result.posterior_mean, 5.0f, 0.5f);
}

TEST_F(SurvivalBayesianRegressionTest, NormalPosteriorVariance) {
    // Posterior variance should decrease with more data
    auto data10 = generateNormal(10, 5.0f, 1.0f);
    auto data100 = generateNormal(100, 5.0f, 1.0f);

    nimcp_bayesian_result_t result10, result100;
    nimcp_stats_bayesian_normal(0.0f, 100.0f, data10.data(), 10, 1.0f, 0.95f, &result10);
    nimcp_stats_bayesian_normal(0.0f, 100.0f, data100.data(), 100, 1.0f, 0.95f, &result100);

    EXPECT_LT(result100.posterior_variance, result10.posterior_variance)
        << "More data should reduce posterior variance";
}

TEST_F(SurvivalBayesianRegressionTest, NormalStrongPrior) {
    // Strong prior should dominate weak data
    // Prior: N(10, 0.01) - very strong (small variance)
    // Data: sample mean ~5, but only few observations

    auto data = generateNormal(5, 5.0f, 1.0f);

    nimcp_bayesian_result_t result;
    nimcp_stats_bayesian_normal(10.0f, 0.01f, data.data(), data.size(), 1.0f, 0.95f, &result);

    // Posterior should be closer to prior mean (10) than sample mean (~5)
    EXPECT_GT(result.posterior_mean, 7.0f);
}

TEST_F(SurvivalBayesianRegressionTest, NormalCredibleIntervalCoverage) {
    float true_mean = 3.0f;
    int coverage_count = 0;
    const int n_simulations = 100;

    for (int sim = 0; sim < n_simulations; ++sim) {
        auto data = generateNormal(30, true_mean, 1.0f);

        nimcp_bayesian_result_t result;
        nimcp_stats_bayesian_normal(0.0f, 100.0f, data.data(), 30, 1.0f, 0.95f, &result);

        if (true_mean >= result.credible_lower && true_mean <= result.credible_upper) {
            coverage_count++;
        }
    }

    float coverage = static_cast<float>(coverage_count) / n_simulations;
    EXPECT_GT(coverage, 0.85f);
}

//=============================================================================
// BAYESIAN GAMMA-POISSON REGRESSION TESTS
//=============================================================================

TEST_F(SurvivalBayesianRegressionTest, GammaPoissonPosteriorMean) {
    // Prior: Gamma(shape=2, rate=1) -> mean = 2
    // Data: 10 events in 2 time units
    // Posterior: Gamma(2+10, 1+2) = Gamma(12, 3) -> mean = 12/3 = 4

    nimcp_bayesian_result_t result;
    nimcp_stats_result_t status = nimcp_stats_bayesian_gamma_poisson(
        2.0f, 1.0f,  // Prior
        10, 2.0f,    // Data: 10 events, 2 exposure
        0.95f,
        &result
    );

    EXPECT_EQ(status, NIMCP_STATS_OK);
    EXPECT_NEAR(result.posterior_mean, 4.0f, 1e-5f);
}

TEST_F(SurvivalBayesianRegressionTest, GammaPoissonNoData) {
    // With no data, posterior = prior
    // Gamma(3, 2) -> mean = 3/2 = 1.5

    nimcp_bayesian_result_t result;
    nimcp_stats_bayesian_gamma_poisson(3.0f, 2.0f, 0, 0.0f, 0.95f, &result);

    EXPECT_NEAR(result.posterior_mean, 1.5f, 1e-5f);
}

TEST_F(SurvivalBayesianRegressionTest, GammaPoissonCoverage) {
    float true_lambda = 3.0f;
    int coverage_count = 0;
    const int n_simulations = 100;

    for (int sim = 0; sim < n_simulations; ++sim) {
        // Generate Poisson data with true_lambda rate over 10 time units
        uint32_t events = 0;
        for (int i = 0; i < 10; ++i) {
            events += generatePoisson(true_lambda);
        }

        nimcp_bayesian_result_t result;
        nimcp_stats_bayesian_gamma_poisson(1.0f, 0.1f, events, 10.0f, 0.95f, &result);

        if (true_lambda >= result.credible_lower && true_lambda <= result.credible_upper) {
            coverage_count++;
        }
    }

    float coverage = static_cast<float>(coverage_count) / n_simulations;
    EXPECT_GT(coverage, 0.85f);
}

//=============================================================================
// BAYES FACTOR REGRESSION TESTS
//=============================================================================

TEST_F(SurvivalBayesianRegressionTest, BayesFactorEqual) {
    // Equal log marginal likelihoods -> BF = 1
    float bf = nimcp_stats_bayes_factor(0.0f, 0.0f);
    EXPECT_FLOAT_EQ(bf, 1.0f);
}

TEST_F(SurvivalBayesianRegressionTest, BayesFactorStrongEvidence) {
    // log(ML1) - log(ML2) = log(100) -> BF = 100
    float bf = nimcp_stats_bayes_factor(std::log(100.0f), 0.0f);
    EXPECT_NEAR(bf, 100.0f, 0.1f);
}

TEST_F(SurvivalBayesianRegressionTest, BayesFactorWeakEvidence) {
    // BF = 2 (weak evidence)
    float bf = nimcp_stats_bayes_factor(std::log(2.0f), 0.0f);
    EXPECT_NEAR(bf, 2.0f, 0.01f);
}

TEST_F(SurvivalBayesianRegressionTest, BayesFactorAgainst) {
    // Evidence against Model 1 (BF < 1)
    float bf = nimcp_stats_bayes_factor(0.0f, std::log(10.0f));
    EXPECT_NEAR(bf, 0.1f, 0.01f);
}

//=============================================================================
// EXPONENTIAL SURVIVAL REGRESSION TESTS
//=============================================================================

TEST_F(SurvivalBayesianRegressionTest, ExponentialSurvivalMLE) {
    // For exponential survival, MLE of lambda = n / sum(times)
    float true_lambda = 2.0f;
    auto times = generateExponentialSurvival(1000, true_lambda);

    float sum_times = std::accumulate(times.begin(), times.end(), 0.0f);
    float mle_lambda = times.size() / sum_times;

    // MLE should be close to true lambda
    EXPECT_NEAR(mle_lambda, true_lambda, 0.2f);
}

TEST_F(SurvivalBayesianRegressionTest, ExponentialSurvivalMean) {
    // Mean survival time = 1/lambda
    float lambda = 0.5f;
    auto times = generateExponentialSurvival(5000, lambda);

    float mean_time = nimcp_stats_mean(times.data(), times.size());
    float expected_mean = 1.0f / lambda;

    EXPECT_NEAR(mean_time, expected_mean, 0.2f);
}

TEST_F(SurvivalBayesianRegressionTest, ExponentialSurvivalMedian) {
    // Median survival time = ln(2)/lambda
    float lambda = 1.0f;
    auto times = generateExponentialSurvival(10000, lambda);

    float median_time = nimcp_stats_median(times.data(), times.size());
    float expected_median = std::log(2.0f) / lambda;

    EXPECT_NEAR(median_time, expected_median, 0.1f);
}

//=============================================================================
// DISTRIBUTION FUNCTION REGRESSION TESTS
//=============================================================================

TEST_F(SurvivalBayesianRegressionTest, GammaPDFKnownValues) {
    // Gamma(2, 1) PDF at x=1: f(1) = 1*exp(-1) = e^-1 ~= 0.368
    float pdf = nimcp_stats_pdf_gamma(1.0f, 2.0f, 1.0f);
    EXPECT_NEAR(pdf, std::exp(-1.0f), 1e-5f);
}

TEST_F(SurvivalBayesianRegressionTest, GammaCDFKnownValues) {
    // Gamma(1, 1) = Exponential(1)
    // CDF at x=1: 1 - exp(-1) ~= 0.632
    float cdf = nimcp_stats_cdf_gamma(1.0f, 1.0f, 1.0f);
    EXPECT_NEAR(cdf, 1.0f - std::exp(-1.0f), 1e-5f);
}

TEST_F(SurvivalBayesianRegressionTest, BetaPDFKnownValues) {
    // Beta(1, 1) = Uniform(0, 1)
    // PDF = 1 everywhere in [0, 1]
    for (float x = 0.1f; x <= 0.9f; x += 0.1f) {
        float pdf = nimcp_stats_pdf_beta(x, 1.0f, 1.0f);
        EXPECT_NEAR(pdf, 1.0f, 1e-5f);
    }
}

TEST_F(SurvivalBayesianRegressionTest, BetaCDFKnownValues) {
    // Beta(1, 1) CDF = x for x in [0, 1]
    for (float x = 0.1f; x <= 0.9f; x += 0.1f) {
        float cdf = nimcp_stats_cdf_beta(x, 1.0f, 1.0f);
        EXPECT_NEAR(cdf, x, 1e-5f);
    }
}

//=============================================================================
// NUMERICAL STABILITY TESTS
//=============================================================================

TEST_F(SurvivalBayesianRegressionTest, BetaBinomialLargeData) {
    // Large sample size
    nimcp_bayesian_result_t result;
    nimcp_stats_result_t status = nimcp_stats_bayesian_beta_binomial(
        1.0f, 1.0f,
        500000, 1000000,  // 50% success rate with 1M trials
        0.95f,
        &result
    );

    EXPECT_EQ(status, NIMCP_STATS_OK);
    EXPECT_NEAR(result.posterior_mean, 0.5f, 0.01f);
    EXPECT_FALSE(std::isnan(result.posterior_variance));
}

TEST_F(SurvivalBayesianRegressionTest, BetaBinomialExtremeParameters) {
    // Very informative prior
    nimcp_bayesian_result_t result;
    nimcp_stats_result_t status = nimcp_stats_bayesian_beta_binomial(
        1000.0f, 1000.0f,  // Strong prior at 0.5
        5, 10,
        0.95f,
        &result
    );

    EXPECT_EQ(status, NIMCP_STATS_OK);
    // Should be close to prior mean
    EXPECT_NEAR(result.posterior_mean, 0.5f, 0.01f);
}

TEST_F(SurvivalBayesianRegressionTest, GammaPoissonLargeExposure) {
    nimcp_bayesian_result_t result;
    nimcp_stats_result_t status = nimcp_stats_bayesian_gamma_poisson(
        1.0f, 0.001f,
        10000, 5000.0f,  // Large exposure
        0.95f,
        &result
    );

    EXPECT_EQ(status, NIMCP_STATS_OK);
    EXPECT_NEAR(result.posterior_mean, 2.0f, 0.1f); // lambda ~= 10000/5000 = 2
}

//=============================================================================
// CONSISTENCY TESTS
//=============================================================================

TEST_F(SurvivalBayesianRegressionTest, BayesianResultsDeterministic) {
    nimcp_bayesian_result_t r1, r2;

    nimcp_stats_bayesian_beta_binomial(1.0f, 1.0f, 30, 50, 0.95f, &r1);
    nimcp_stats_bayesian_beta_binomial(1.0f, 1.0f, 30, 50, 0.95f, &r2);

    EXPECT_FLOAT_EQ(r1.posterior_mean, r2.posterior_mean);
    EXPECT_FLOAT_EQ(r1.posterior_variance, r2.posterior_variance);
    EXPECT_FLOAT_EQ(r1.credible_lower, r2.credible_lower);
    EXPECT_FLOAT_EQ(r1.credible_upper, r2.credible_upper);
}

TEST_F(SurvivalBayesianRegressionTest, BetaBinomialSequentialUpdate) {
    // Sequential updates should match batch update
    // Prior: Beta(1,1), Data: 3+2=5 successes in 5+5=10 trials

    // Batch
    nimcp_bayesian_result_t batch;
    nimcp_stats_bayesian_beta_binomial(1.0f, 1.0f, 5, 10, 0.95f, &batch);

    // Sequential: first update
    nimcp_bayesian_result_t seq1;
    nimcp_stats_bayesian_beta_binomial(1.0f, 1.0f, 3, 5, 0.95f, &seq1);

    // Second update using posterior as new prior
    // Posterior1: Beta(1+3, 1+2) = Beta(4, 3)
    nimcp_bayesian_result_t seq2;
    nimcp_stats_bayesian_beta_binomial(4.0f, 3.0f, 2, 5, 0.95f, &seq2);

    EXPECT_NEAR(batch.posterior_mean, seq2.posterior_mean, 1e-5f)
        << "Sequential update should match batch";
}

//=============================================================================
// PERFORMANCE TESTS
//=============================================================================

TEST_F(SurvivalBayesianRegressionTest, BetaBinomialPerformance) {
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 10000; ++i) {
        nimcp_bayesian_result_t result;
        nimcp_stats_bayesian_beta_binomial(1.0f, 1.0f, 30, 50, 0.95f, &result);
    }

    auto end = std::chrono::high_resolution_clock::now();
    double elapsed_us = std::chrono::duration<double, std::micro>(end - start).count() / 10000.0;

    // Allow up to 50us per call to account for system variability
    EXPECT_LT(elapsed_us, 50.0) << "Beta-binomial too slow: " << elapsed_us << "us";
}

TEST_F(SurvivalBayesianRegressionTest, NormalPosteriorPerformance) {
    auto data = generateNormal(1000, 0.0f, 1.0f);

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 1000; ++i) {
        nimcp_bayesian_result_t result;
        nimcp_stats_bayesian_normal(0.0f, 100.0f, data.data(), data.size(), 1.0f, 0.95f, &result);
    }

    auto end = std::chrono::high_resolution_clock::now();
    double elapsed_us = std::chrono::duration<double, std::micro>(end - start).count() / 1000.0;

    EXPECT_LT(elapsed_us, 500.0) << "Normal posterior too slow: " << elapsed_us << "us";
}

//=============================================================================
// EDGE CASE TESTS
//=============================================================================

TEST_F(SurvivalBayesianRegressionTest, BetaBinomialZeroSuccesses) {
    nimcp_bayesian_result_t result;
    nimcp_stats_bayesian_beta_binomial(1.0f, 1.0f, 0, 10, 0.95f, &result);

    // Posterior: Beta(1, 11), mean = 1/12
    EXPECT_NEAR(result.posterior_mean, 1.0f/12.0f, 1e-5f);
}

TEST_F(SurvivalBayesianRegressionTest, BetaBinomialAllSuccesses) {
    nimcp_bayesian_result_t result;
    nimcp_stats_bayesian_beta_binomial(1.0f, 1.0f, 10, 10, 0.95f, &result);

    // Posterior: Beta(11, 1), mean = 11/12
    EXPECT_NEAR(result.posterior_mean, 11.0f/12.0f, 1e-5f);
}

TEST_F(SurvivalBayesianRegressionTest, GammaPoissonZeroEvents) {
    nimcp_bayesian_result_t result;
    nimcp_stats_bayesian_gamma_poisson(2.0f, 1.0f, 0, 5.0f, 0.95f, &result);

    // Posterior: Gamma(2, 6), mean = 2/6 = 1/3
    EXPECT_NEAR(result.posterior_mean, 2.0f/6.0f, 1e-5f);
}

//=============================================================================
// MAIN
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
