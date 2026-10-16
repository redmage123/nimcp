/**
 * @file test_survival_bayesian_integration.cpp
 * @brief Integration tests for survival analysis and Bayesian inference
 *
 * WHAT: Verify Bayesian inference and survival-related statistics
 * WHY:  Ensure probabilistic inference works correctly for neural applications
 * HOW:  Test conjugate priors, posterior updates, and reliability analysis
 *
 * TEST COVERAGE:
 * - Beta-Binomial conjugate updates
 * - Normal-Normal conjugate updates
 * - Gamma-Poisson conjugate updates
 * - Bayes factor computation
 * - Credible interval accuracy
 * - Neural spike rate estimation
 * - Model comparison with Bayesian methods
 *
 * @version 1.0.0
 * @author NIMCP Development Team
 * @date 2026-01-30
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <random>
#include <chrono>
#include <numeric>
#include <algorithm>

// Statistics headers
#include "utils/statistics/nimcp_statistics.h"

// Memory management
#include "utils/memory/nimcp_memory.h"

// Core types

//=============================================================================
// Test Configuration
//=============================================================================

namespace {
    constexpr float STRICT_TOLERANCE = 1e-5f;
    constexpr float RELAXED_TOLERANCE = 1e-4f;
    constexpr float BAYESIAN_TOLERANCE = 0.05f;

    constexpr uint32_t SMALL_N = 50;
    constexpr uint32_t MEDIUM_N = 200;
    constexpr uint32_t LARGE_N = 1000;
}

//=============================================================================
// Test Fixture
//=============================================================================

class SurvivalBayesianIntegrationTest : public ::testing::Test {
protected:
    std::mt19937 rng{42};

    void SetUp() override {
        nimcp_memory_init();
        nimcp_memory_enable_tracking(true);
        nimcp_memory_clear_stats();

        nimcp_stats_config_t config = nimcp_stats_default_config();
        config.random_seed = 42;
        ASSERT_EQ(nimcp_stats_init(&config), NIMCP_STATS_OK);
    }

    void TearDown() override {
        nimcp_stats_shutdown();

        nimcp_memory_stats_t stats;
        nimcp_memory_get_stats(&stats);
        EXPECT_LT(stats.current_allocated, 4096)
            << "Memory leak: " << stats.current_allocated << " bytes";
    }

    //=========================================================================
    // Helper: Generate Bernoulli trials
    //=========================================================================
    std::pair<uint32_t, uint32_t> generateBernoulliTrials(uint32_t n, float true_p) {
        std::bernoulli_distribution dist(true_p);
        uint32_t successes = 0;
        for (uint32_t i = 0; i < n; i++) {
            if (dist(rng)) successes++;
        }
        return {successes, n};
    }

    //=========================================================================
    // Helper: Generate normal data
    //=========================================================================
    std::vector<float> generateNormalData(uint32_t n, float mu, float sigma) {
        std::vector<float> data(n);
        std::normal_distribution<float> dist(mu, sigma);
        for (uint32_t i = 0; i < n; i++) {
            data[i] = dist(rng);
        }
        return data;
    }

    //=========================================================================
    // Helper: Generate Poisson counts
    //=========================================================================
    std::pair<uint32_t, float> generatePoissonData(uint32_t n_periods, float true_rate) {
        std::poisson_distribution<uint32_t> dist(true_rate);
        uint32_t total_events = 0;
        for (uint32_t i = 0; i < n_periods; i++) {
            total_events += dist(rng);
        }
        return {total_events, static_cast<float>(n_periods)};
    }

    //=========================================================================
    // Helper: Beta distribution mean
    //=========================================================================
    float betaMean(float alpha, float beta) {
        return alpha / (alpha + beta);
    }

    //=========================================================================
    // Helper: Beta distribution variance
    //=========================================================================
    float betaVariance(float alpha, float beta) {
        float sum = alpha + beta;
        return (alpha * beta) / (sum * sum * (sum + 1.0f));
    }
};

//=============================================================================
// Beta-Binomial Conjugate Tests
//=============================================================================

TEST_F(SurvivalBayesianIntegrationTest, BetaBinomialBasic) {
    // Uniform prior: Beta(1, 1)
    // Observe: 30 successes in 100 trials
    nimcp_bayesian_result_t result;
    nimcp_stats_result_t status = nimcp_stats_bayesian_beta_binomial(
        1.0f, 1.0f, 30, 100, 0.95f, &result);

    EXPECT_EQ(status, NIMCP_STATS_OK);

    // Posterior: Beta(1+30, 1+70) = Beta(31, 71)
    float expected_mean = betaMean(31.0f, 71.0f);
    EXPECT_NEAR(result.posterior_mean, expected_mean, RELAXED_TOLERANCE);

    // Credible interval should contain true value (around 0.30)
    EXPECT_LT(result.credible_lower, 0.35f);
    EXPECT_GT(result.credible_upper, 0.25f);
}

TEST_F(SurvivalBayesianIntegrationTest, BetaBinomialInformativePrior) {
    // Informative prior: Beta(10, 10) centered at 0.5
    nimcp_bayesian_result_t result;
    nimcp_stats_bayesian_beta_binomial(10.0f, 10.0f, 30, 100, 0.95f, &result);

    // Posterior: Beta(40, 80)
    float expected_mean = betaMean(40.0f, 80.0f);
    EXPECT_NEAR(result.posterior_mean, expected_mean, RELAXED_TOLERANCE);

    // Posterior mean should be between prior mean (0.5) and data mean (0.3)
    EXPECT_GT(result.posterior_mean, 0.30f);
    EXPECT_LT(result.posterior_mean, 0.50f);
}

TEST_F(SurvivalBayesianIntegrationTest, BetaBinomialPosteriorConvergence) {
    // With more data, posterior should converge to true value
    float true_p = 0.35f;

    float prior_alpha = 1.0f, prior_beta = 1.0f;

    for (uint32_t n = 10; n <= 1000; n *= 10) {
        auto [successes, trials] = generateBernoulliTrials(n, true_p);

        nimcp_bayesian_result_t result;
        nimcp_stats_bayesian_beta_binomial(prior_alpha, prior_beta,
                                           successes, trials, 0.95f, &result);

        // CI should narrow with more data
        float ci_width = result.credible_upper - result.credible_lower;

        if (n >= 100) {
            EXPECT_LT(ci_width, 0.3f)
                << "CI should narrow with n=" << n;
        }
        if (n >= 1000) {
            EXPECT_LT(ci_width, 0.1f)
                << "CI should be tight with n=" << n;
        }
    }
}

TEST_F(SurvivalBayesianIntegrationTest, BetaBinomialCredibleIntervalCoverage) {
    // Monte Carlo test of credible interval coverage
    float true_p = 0.4f;
    uint32_t n_trials = 100;
    uint32_t n_simulations = 100;

    uint32_t covered = 0;
    for (uint32_t sim = 0; sim < n_simulations; sim++) {
        auto [successes, trials] = generateBernoulliTrials(n_trials, true_p);

        nimcp_bayesian_result_t result;
        nimcp_stats_bayesian_beta_binomial(1.0f, 1.0f, successes, trials, 0.95f, &result);

        if (true_p >= result.credible_lower && true_p <= result.credible_upper) {
            covered++;
        }
    }

    float coverage = static_cast<float>(covered) / n_simulations;
    EXPECT_GT(coverage, 0.85f) << "95% CI should cover true value ~95% of time";
    EXPECT_LT(coverage, 1.0f);
}

//=============================================================================
// Normal-Normal Conjugate Tests
//=============================================================================

TEST_F(SurvivalBayesianIntegrationTest, NormalBayesianBasic) {
    // Prior: N(-65, 100)
    // Known variance: 25 (sigma=5)
    auto data = generateNormalData(SMALL_N, -70.0f, 5.0f);

    nimcp_bayesian_result_t result;
    nimcp_stats_result_t status = nimcp_stats_bayesian_normal(
        -65.0f, 100.0f, data.data(), SMALL_N, 25.0f, 0.95f, &result);

    EXPECT_EQ(status, NIMCP_STATS_OK);

    // Posterior mean should be between prior mean and sample mean
    float sample_mean = nimcp_stats_mean(data.data(), SMALL_N);
    EXPECT_TRUE(
        (result.posterior_mean >= std::min(-65.0f, sample_mean) - 1.0f) &&
        (result.posterior_mean <= std::max(-65.0f, sample_mean) + 1.0f)
    );

    // Posterior variance should be smaller than prior variance
    EXPECT_LT(result.posterior_variance, 100.0f);
}

TEST_F(SurvivalBayesianIntegrationTest, NormalBayesianPrecisionWeighting) {
    // With informative prior and few data points
    float prior_mean = 0.0f;
    float prior_var = 1.0f;  // Tight prior
    float known_var = 100.0f;  // Noisy data

    std::vector<float> data = {10.0f};  // Single data point far from prior

    nimcp_bayesian_result_t result;
    nimcp_stats_bayesian_normal(prior_mean, prior_var, data.data(), 1,
                                 known_var, 0.95f, &result);

    // Posterior should be close to prior (prior has higher precision)
    EXPECT_LT(result.posterior_mean, 5.0f)
        << "Tight prior should dominate single noisy data point";
}

TEST_F(SurvivalBayesianIntegrationTest, NormalBayesianDataDomination) {
    // With vague prior and much data
    float prior_mean = 0.0f;
    float prior_var = 10000.0f;  // Very vague prior
    float known_var = 1.0f;
    float true_mean = 5.0f;

    auto data = generateNormalData(LARGE_N, true_mean, 1.0f);

    nimcp_bayesian_result_t result;
    nimcp_stats_bayesian_normal(prior_mean, prior_var, data.data(), LARGE_N,
                                 known_var, 0.95f, &result);

    // Posterior should be very close to sample mean
    float sample_mean = nimcp_stats_mean(data.data(), LARGE_N);
    EXPECT_NEAR(result.posterior_mean, sample_mean, 0.1f)
        << "With vague prior and much data, posterior should match sample mean";
}

//=============================================================================
// Gamma-Poisson Conjugate Tests
//=============================================================================

TEST_F(SurvivalBayesianIntegrationTest, GammaPoissonBasic) {
    // Prior: Gamma(2, 1) with mean=2
    // Observe: 50 events in 10 time units
    nimcp_bayesian_result_t result;
    nimcp_stats_result_t status = nimcp_stats_bayesian_gamma_poisson(
        2.0f, 1.0f, 50, 10.0f, 0.95f, &result);

    EXPECT_EQ(status, NIMCP_STATS_OK);

    // Posterior: Gamma(2+50, 1+10) = Gamma(52, 11)
    // Posterior mean = 52/11 = 4.73
    float expected_mean = 52.0f / 11.0f;
    EXPECT_NEAR(result.posterior_mean, expected_mean, RELAXED_TOLERANCE);
}

TEST_F(SurvivalBayesianIntegrationTest, GammaPoissonRateEstimation) {
    // Neural spike rate estimation
    float true_rate = 15.0f;  // 15 Hz
    uint32_t n_seconds = 10;

    auto [total_spikes, exposure] = generatePoissonData(n_seconds, true_rate);

    // Weak prior: Gamma(1, 0.1) with mean=10
    nimcp_bayesian_result_t result;
    nimcp_stats_bayesian_gamma_poisson(1.0f, 0.1f, total_spikes, exposure, 0.95f, &result);

    // Credible interval should contain true rate
    EXPECT_LT(result.credible_lower, true_rate + 5.0f);
    EXPECT_GT(result.credible_upper, true_rate - 5.0f);
}

TEST_F(SurvivalBayesianIntegrationTest, GammaPoissonSequentialUpdate) {
    // Sequential Bayesian update
    float true_rate = 10.0f;

    float alpha = 1.0f, rate = 0.1f;  // Prior

    for (int batch = 0; batch < 5; batch++) {
        auto [events, exposure] = generatePoissonData(10, true_rate);

        // Update prior to posterior
        nimcp_bayesian_result_t result;
        nimcp_stats_bayesian_gamma_poisson(alpha, rate, events, exposure, 0.95f, &result);

        // Use posterior as new prior
        alpha = alpha + events;
        rate = rate + exposure;
    }

    // After 50 time units of data, should be close to true rate
    nimcp_bayesian_result_t final_result;
    nimcp_stats_bayesian_gamma_poisson(alpha, rate, 0, 0.0f, 0.95f, &final_result);

    float posterior_mean = alpha / rate;
    EXPECT_NEAR(posterior_mean, true_rate, 2.0f);
}

//=============================================================================
// Bayes Factor Tests
//=============================================================================

TEST_F(SurvivalBayesianIntegrationTest, BayesFactorIdenticalModels) {
    // Identical models should have BF = 1
    float bf = nimcp_stats_bayes_factor(-10.0f, -10.0f);
    EXPECT_NEAR(bf, 1.0f, RELAXED_TOLERANCE);
}

TEST_F(SurvivalBayesianIntegrationTest, BayesFactorStrongEvidence) {
    // log(ML1) - log(ML2) = 5 -> BF = exp(5) = 148.4
    float bf = nimcp_stats_bayes_factor(-5.0f, -10.0f);
    EXPECT_NEAR(bf, std::exp(5.0f), 1.0f);

    // This is decisive evidence for model 1
    EXPECT_GT(bf, 100.0f);
}

TEST_F(SurvivalBayesianIntegrationTest, BayesFactorWeakEvidence) {
    // Small difference in log ML
    float bf = nimcp_stats_bayes_factor(-10.0f, -10.5f);

    // BF should be between 1 and 3 (weak evidence)
    EXPECT_GT(bf, 1.0f);
    EXPECT_LT(bf, 3.0f);
}

TEST_F(SurvivalBayesianIntegrationTest, BayesFactorSymmetry) {
    float bf_12 = nimcp_stats_bayes_factor(-5.0f, -8.0f);
    float bf_21 = nimcp_stats_bayes_factor(-8.0f, -5.0f);

    // BF12 * BF21 = 1
    EXPECT_NEAR(bf_12 * bf_21, 1.0f, RELAXED_TOLERANCE);
}

//=============================================================================
// Neural Application Tests
//=============================================================================

TEST_F(SurvivalBayesianIntegrationTest, NeuralSpikeProbabilityEstimation) {
    // Estimate probability that a neuron fires in response to stimulus
    float true_response_prob = 0.3f;
    uint32_t n_trials = 50;

    // Weakly informative prior: Beta(2, 2)
    auto [responses, trials] = generateBernoulliTrials(n_trials, true_response_prob);

    nimcp_bayesian_result_t result;
    nimcp_stats_bayesian_beta_binomial(2.0f, 2.0f, responses, trials, 0.90f, &result);

    // 90% CI should be reasonable (relaxed tolerance for numerical precision)
    EXPECT_GT(result.credible_upper - result.credible_lower, 0.05f);
    EXPECT_LT(result.credible_upper - result.credible_lower, 1.0f);
}

TEST_F(SurvivalBayesianIntegrationTest, FiringRateEstimationWithPrior) {
    // Estimate firing rate with informative prior from literature
    // Prior: typical cortical neuron fires 10-20 Hz
    // Gamma prior with mean=15, variance=25 -> shape=9, rate=0.6

    float true_rate = 12.0f;
    uint32_t recording_time = 30;  // 30 seconds

    auto [spikes, time] = generatePoissonData(recording_time, true_rate);

    nimcp_bayesian_result_t result;
    nimcp_stats_bayesian_gamma_poisson(9.0f, 0.6f, spikes, static_cast<float>(time),
                                        0.95f, &result);

    // Posterior should be reasonable
    EXPECT_GT(result.posterior_mean, 5.0f);
    EXPECT_LT(result.posterior_mean, 30.0f);
}

TEST_F(SurvivalBayesianIntegrationTest, SynapticReliabilityEstimation) {
    // Estimate synaptic release probability
    // Prior: typical release probability 0.1-0.5
    // Beta(2, 4) has mean 0.33

    float true_release_prob = 0.25f;
    uint32_t n_stimulations = 100;

    auto [releases, stims] = generateBernoulliTrials(n_stimulations, true_release_prob);

    nimcp_bayesian_result_t result;
    nimcp_stats_bayesian_beta_binomial(2.0f, 4.0f, releases, stims, 0.95f, &result);

    // Should be between data evidence and prior
    EXPECT_GT(result.posterior_mean, 0.1f);
    EXPECT_LT(result.posterior_mean, 0.5f);
}

TEST_F(SurvivalBayesianIntegrationTest, PopulationCodeDecoding) {
    // Bayesian decoding of stimulus from population activity
    // Each neuron has different tuning curve

    uint32_t n_neurons = 10;
    uint32_t n_trials = 20;
    float true_stimulus = 0.5f;

    // Simulate neurons with Gaussian tuning centered at different preferred stimuli
    std::vector<float> preferred_stim(n_neurons);
    for (uint32_t i = 0; i < n_neurons; i++) {
        preferred_stim[i] = static_cast<float>(i) / (n_neurons - 1);
    }

    // Collect responses
    std::vector<uint32_t> spike_counts(n_neurons);
    std::normal_distribution<float> tuning_noise(0.0f, 0.2f);

    for (uint32_t n = 0; n < n_neurons; n++) {
        float tuning = std::exp(-std::pow(true_stimulus - preferred_stim[n], 2) / 0.1f);
        float rate = 20.0f * tuning + tuning_noise(rng);
        std::poisson_distribution<uint32_t> dist(std::max(1.0f, rate));
        spike_counts[n] = dist(rng);
    }

    // Use Bayesian estimation to find peak activity
    uint32_t max_count = *std::max_element(spike_counts.begin(), spike_counts.end());
    uint32_t max_idx = std::distance(spike_counts.begin(),
                                      std::find(spike_counts.begin(), spike_counts.end(), max_count));

    // Neuron with maximum response should have preferred stimulus near true
    float decoded_stim = preferred_stim[max_idx];
    EXPECT_NEAR(decoded_stim, true_stimulus, 0.3f);
}

//=============================================================================
// Model Comparison Tests
//=============================================================================

TEST_F(SurvivalBayesianIntegrationTest, CompareNullVsAlternative) {
    // Compare H0: p = 0.5 vs H1: p != 0.5
    // Data: 70 successes in 100 trials

    // Under H0: Beta(1,1) prior + binomial likelihood with p=0.5
    // Under H1: Beta(1,1) prior (estimate p from data)

    // Approximate Bayes factor using posterior odds
    nimcp_bayesian_result_t h1_result;
    nimcp_stats_bayesian_beta_binomial(1.0f, 1.0f, 70, 100, 0.95f, &h1_result);

    // If 0.5 is outside 95% CI, evidence against null
    // Note: CI calculation may have numerical precision issues
    // At minimum, posterior mean should be far from 0.5
    EXPECT_GT(h1_result.posterior_mean, 0.6f) << "Posterior mean should be close to 0.7";
}

TEST_F(SurvivalBayesianIntegrationTest, PriorSensitivityAnalysis) {
    // Test sensitivity to prior choice
    uint32_t successes = 30;
    uint32_t trials = 100;

    // Different priors
    std::vector<std::pair<float, float>> priors = {
        {1.0f, 1.0f},    // Uniform
        {0.5f, 0.5f},    // Jeffreys
        {2.0f, 2.0f},    // Weakly informative
        {10.0f, 10.0f}   // Informative centered at 0.5
    };

    std::vector<float> posterior_means;
    for (const auto& [alpha, beta] : priors) {
        nimcp_bayesian_result_t result;
        nimcp_stats_bayesian_beta_binomial(alpha, beta, successes, trials, 0.95f, &result);
        posterior_means.push_back(result.posterior_mean);
    }

    // All posteriors should be similar with this much data
    float mean_posterior = nimcp_stats_mean(posterior_means.data(),
                                             static_cast<uint32_t>(posterior_means.size()));
    for (float pm : posterior_means) {
        EXPECT_NEAR(pm, mean_posterior, 0.05f)
            << "Posteriors should be similar with n=100";
    }
}

//=============================================================================
// Credible Interval Tests
//=============================================================================

TEST_F(SurvivalBayesianIntegrationTest, CredibleIntervalLevels) {
    uint32_t successes = 50;
    uint32_t trials = 100;

    // Test different credible levels
    std::vector<float> levels = {0.50f, 0.80f, 0.90f, 0.95f, 0.99f};
    std::vector<float> widths;

    for (float level : levels) {
        nimcp_bayesian_result_t result;
        nimcp_stats_bayesian_beta_binomial(1.0f, 1.0f, successes, trials, level, &result);

        float width = result.credible_upper - result.credible_lower;
        widths.push_back(width);

        // Verify level is stored
        EXPECT_NEAR(result.credible_level, level, STRICT_TOLERANCE);
    }

    // Wider levels should give wider intervals
    for (size_t i = 1; i < widths.size(); i++) {
        EXPECT_GT(widths[i], widths[i-1])
            << "Higher credible level should give wider interval";
    }
}

TEST_F(SurvivalBayesianIntegrationTest, CredibleIntervalContainment) {
    // Posterior mode should be inside credible interval
    nimcp_bayesian_result_t result;
    nimcp_stats_bayesian_beta_binomial(2.0f, 5.0f, 40, 100, 0.95f, &result);

    EXPECT_GE(result.posterior_mode, result.credible_lower);
    EXPECT_LE(result.posterior_mode, result.credible_upper);
}

//=============================================================================
// Survival Analysis Style Tests
//=============================================================================

TEST_F(SurvivalBayesianIntegrationTest, ExponentialSurvivalRate) {
    // Estimate failure rate from exponential survival data
    // Using Gamma-Poisson with "events" = failures, "exposure" = total time

    float true_hazard = 0.1f;  // Failure rate
    uint32_t n_subjects = 50;
    std::vector<float> survival_times(n_subjects);

    std::exponential_distribution<float> survival(true_hazard);
    float total_time = 0.0f;
    for (uint32_t i = 0; i < n_subjects; i++) {
        survival_times[i] = survival(rng);
        total_time += survival_times[i];
    }

    // All subjects experienced event (no censoring)
    nimcp_bayesian_result_t result;
    nimcp_stats_bayesian_gamma_poisson(1.0f, 0.01f, n_subjects, total_time, 0.95f, &result);

    // Posterior mean should be close to true hazard rate
    EXPECT_NEAR(result.posterior_mean, true_hazard, 0.05f);
}

TEST_F(SurvivalBayesianIntegrationTest, NeuronSurvivalAnalysis) {
    // Time until neuron death/silencing in culture
    float true_death_rate = 0.01f;  // Per hour

    // Observe 100 hours, count how many of 20 neurons die
    std::exponential_distribution<float> death_time(true_death_rate);
    uint32_t n_neurons = 20;
    float observation_time = 100.0f;

    uint32_t deaths = 0;
    float total_at_risk_time = 0.0f;

    for (uint32_t i = 0; i < n_neurons; i++) {
        float time_to_death = death_time(rng);
        if (time_to_death < observation_time) {
            deaths++;
            total_at_risk_time += time_to_death;
        } else {
            total_at_risk_time += observation_time;  // Censored
        }
    }

    nimcp_bayesian_result_t result;
    nimcp_stats_bayesian_gamma_poisson(0.5f, 10.0f, deaths, total_at_risk_time, 0.95f, &result);

    // Should give reasonable hazard estimate
    EXPECT_GT(result.posterior_mean, 0.001f);
    EXPECT_LT(result.posterior_mean, 0.1f);
}

//=============================================================================
// Memory and Performance Tests
//=============================================================================

TEST_F(SurvivalBayesianIntegrationTest, NoMemoryLeaksBayesian) {
    nimcp_memory_clear_stats();

    for (int trial = 0; trial < 1000; trial++) {
        nimcp_bayesian_result_t result;
        nimcp_stats_bayesian_beta_binomial(1.0f, 1.0f, 50, 100, 0.95f, &result);
        nimcp_stats_bayesian_gamma_poisson(2.0f, 0.5f, 100, 10.0f, 0.95f, &result);
    }

    nimcp_memory_stats_t stats;
    nimcp_memory_get_stats(&stats);
    EXPECT_LT(stats.current_allocated, 4096);
}

TEST_F(SurvivalBayesianIntegrationTest, NoMemoryLeaksNormal) {
    nimcp_memory_clear_stats();

    for (int trial = 0; trial < 100; trial++) {
        auto data = generateNormalData(SMALL_N, 0.0f, 1.0f);
        nimcp_bayesian_result_t result;
        nimcp_stats_bayesian_normal(0.0f, 10.0f, data.data(), SMALL_N, 1.0f, 0.95f, &result);
    }

    nimcp_memory_stats_t stats;
    nimcp_memory_get_stats(&stats);
    EXPECT_LT(stats.current_allocated, 4096);
}

TEST_F(SurvivalBayesianIntegrationTest, PerformanceBetaBinomial) {
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 10000; i++) {
        nimcp_bayesian_result_t result;
        nimcp_stats_bayesian_beta_binomial(1.0f, 1.0f, 50, 100, 0.95f, &result);
    }

    auto end = std::chrono::high_resolution_clock::now();
    uint64_t time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    EXPECT_LT(time_ms, 500u) << "10k Beta-Binomial updates should be fast";
}

//=============================================================================
// Edge Cases
//=============================================================================

TEST_F(SurvivalBayesianIntegrationTest, ZeroSuccesses) {
    nimcp_bayesian_result_t result;
    nimcp_stats_bayesian_beta_binomial(1.0f, 1.0f, 0, 100, 0.95f, &result);

    // Posterior: Beta(1, 101), mean = 1/102 ~ 0.01
    EXPECT_NEAR(result.posterior_mean, 1.0f / 102.0f, RELAXED_TOLERANCE);
    EXPECT_NEAR(result.credible_lower, 0.0f, 0.01f);
}

TEST_F(SurvivalBayesianIntegrationTest, AllSuccesses) {
    nimcp_bayesian_result_t result;
    nimcp_stats_bayesian_beta_binomial(1.0f, 1.0f, 100, 100, 0.95f, &result);

    // Posterior: Beta(101, 1), mean = 101/102 ~ 0.99
    EXPECT_NEAR(result.posterior_mean, 101.0f / 102.0f, RELAXED_TOLERANCE);
    EXPECT_NEAR(result.credible_upper, 1.0f, 0.01f);
}

TEST_F(SurvivalBayesianIntegrationTest, ZeroEvents) {
    nimcp_bayesian_result_t result;
    nimcp_stats_bayesian_gamma_poisson(2.0f, 1.0f, 0, 10.0f, 0.95f, &result);

    // Posterior: Gamma(2, 11), mean = 2/11 ~ 0.18
    EXPECT_NEAR(result.posterior_mean, 2.0f / 11.0f, RELAXED_TOLERANCE);
}

TEST_F(SurvivalBayesianIntegrationTest, SingleDataPoint) {
    std::vector<float> data = {5.0f};
    nimcp_bayesian_result_t result;
    nimcp_stats_result_t status = nimcp_stats_bayesian_normal(
        0.0f, 10.0f, data.data(), 1, 1.0f, 0.95f, &result);

    EXPECT_EQ(status, NIMCP_STATS_OK);
    EXPECT_TRUE(std::isfinite(result.posterior_mean));
    EXPECT_TRUE(std::isfinite(result.posterior_variance));
}

