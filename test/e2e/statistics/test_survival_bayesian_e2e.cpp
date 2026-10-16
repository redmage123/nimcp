//=============================================================================
// test_survival_bayesian_e2e.cpp - Survival Analysis and Bayesian E2E Tests
//=============================================================================
/**
 * @file test_survival_bayesian_e2e.cpp
 * @brief End-to-end tests for survival analysis and Bayesian inference workflows
 *
 * WHAT: Complete clinical trial and Bayesian analysis pipelines
 * WHY:  Verify statistics module handles time-to-event and probabilistic reasoning
 * HOW:  Kaplan-Meier, Cox models, Bayesian updates, posterior inference
 *
 * TEST SCENARIOS:
 * 1. Kaplan-Meier survival curve estimation
 * 2. Log-rank test for survival comparison
 * 3. Hazard rate estimation
 * 4. Cox proportional hazards analysis
 * 5. Bayesian A/B testing
 * 6. Conjugate prior updates (Beta-Binomial)
 * 7. Bayesian parameter estimation
 * 8. Posterior predictive checking
 * 9. Model comparison with Bayes factors
 * 10. Credible interval estimation
 * 11. Sequential Bayesian updating
 * 12. Hierarchical Bayesian analysis (simplified)
 * 13. Bayesian regression
 * 14. Survival time prediction
 * 15. Clinical trial interim analysis
 *
 * @date 2026-01-30
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <random>
#include <algorithm>
#include <numeric>
#include <chrono>

extern "C" {
#include "utils/statistics/nimcp_statistics.h"
}

// Test tolerances
#define TOLERANCE 1e-5f
#define LOOSE_TOLERANCE 1e-2f
#define VERY_LOOSE_TOLERANCE 0.1f

// Timing macros
#define START_TIMER() auto _start_time = std::chrono::high_resolution_clock::now()
#define STOP_TIMER_MS() std::chrono::duration<double, std::milli>( \
    std::chrono::high_resolution_clock::now() - _start_time).count()

//=============================================================================
// Test Fixture
//=============================================================================

class SurvivalBayesianE2ETest : public ::testing::Test {
protected:
    void SetUp() override {
        config = nimcp_stats_default_config();
        ASSERT_EQ(nimcp_stats_init(&config), NIMCP_STATS_OK);
        rng.seed(42);  // Reproducible tests
    }

    void TearDown() override {
        nimcp_stats_shutdown();
    }

    nimcp_stats_config_t config;
    std::mt19937 rng;

    // Survival data structure
    struct SurvivalData {
        std::vector<float> times;     // Time to event or censoring
        std::vector<bool> events;     // True if event occurred, false if censored
        std::vector<int> groups;      // Group assignment (for comparisons)
    };

    // Generate exponential survival times
    SurvivalData generate_exponential_survival(size_t n, float lambda,
                                               float censoring_rate = 0.2f) {
        SurvivalData data;
        data.times.resize(n);
        data.events.resize(n);
        data.groups.resize(n, 0);

        std::exponential_distribution<float> surv_dist(lambda);
        std::uniform_real_distribution<float> censor_dist(0.0f, 1.0f);

        float max_time = 10.0f / lambda;  // Study duration

        for (size_t i = 0; i < n; i++) {
            float event_time = surv_dist(rng);
            if (censor_dist(rng) < censoring_rate || event_time > max_time) {
                // Censored: observed time is uniformly distributed before max_time
                data.times[i] = std::min(event_time, max_time);
                data.events[i] = (event_time <= max_time && censor_dist(rng) >= censoring_rate);
            } else {
                data.times[i] = event_time;
                data.events[i] = true;
            }
        }
        return data;
    }

    // Generate two-group survival data with hazard ratio
    SurvivalData generate_two_group_survival(size_t n_per_group,
                                             float lambda_control,
                                             float hazard_ratio) {
        SurvivalData data;
        size_t n_total = 2 * n_per_group;
        data.times.resize(n_total);
        data.events.resize(n_total);
        data.groups.resize(n_total);

        std::exponential_distribution<float> control_dist(lambda_control);
        std::exponential_distribution<float> treat_dist(lambda_control * hazard_ratio);
        std::uniform_real_distribution<float> censor_dist(0.0f, 1.0f);

        float max_time = 10.0f / lambda_control;

        for (size_t i = 0; i < n_total; i++) {
            data.groups[i] = (i < n_per_group) ? 0 : 1;
            float event_time = (data.groups[i] == 0)
                ? control_dist(rng) : treat_dist(rng);

            if (event_time > max_time) {
                data.times[i] = max_time;
                data.events[i] = false;
            } else {
                data.times[i] = event_time;
                data.events[i] = (censor_dist(rng) > 0.1f);  // 10% random censoring
            }
        }
        return data;
    }

    // Generate normal samples
    std::vector<float> generate_normal(size_t n, float mean, float stddev) {
        std::normal_distribution<float> dist(mean, stddev);
        std::vector<float> data(n);
        for (auto& x : data) x = dist(rng);
        return data;
    }

    // Generate binomial trials
    std::pair<uint32_t, uint32_t> generate_binomial(uint32_t n, float p) {
        std::binomial_distribution<uint32_t> dist(n, p);
        uint32_t successes = dist(rng);
        return {successes, n};
    }
};

//=============================================================================
// E2E Test 1: Kaplan-Meier Survival Curve Estimation
//=============================================================================

TEST_F(SurvivalBayesianE2ETest, KaplanMeierSurvivalEstimation) {
    START_TIMER();

    // Generate exponential survival data (lambda = 0.1, median ≈ 6.93)
    const size_t n_patients = 200;
    const float true_lambda = 0.1f;
    auto data = generate_exponential_survival(n_patients, true_lambda, 0.2f);

    // Sort by time
    std::vector<size_t> order(n_patients);
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(),
              [&](size_t a, size_t b) { return data.times[a] < data.times[b]; });

    // Kaplan-Meier estimation
    std::vector<float> km_times;
    std::vector<float> km_survival;
    float survival = 1.0f;
    size_t at_risk = n_patients;

    km_times.push_back(0.0f);
    km_survival.push_back(1.0f);

    for (size_t i = 0; i < n_patients; i++) {
        size_t idx = order[i];
        if (data.events[idx]) {
            survival *= (1.0f - 1.0f / at_risk);
            km_times.push_back(data.times[idx]);
            km_survival.push_back(survival);
        }
        at_risk--;
    }

    // Compute median survival (time when S(t) = 0.5)
    float km_median = 0.0f;
    for (size_t i = 1; i < km_survival.size(); i++) {
        if (km_survival[i] <= 0.5f && km_survival[i-1] > 0.5f) {
            // Linear interpolation
            float w = (0.5f - km_survival[i]) / (km_survival[i-1] - km_survival[i]);
            km_median = w * km_times[i-1] + (1.0f - w) * km_times[i];
            break;
        }
    }

    // True median for exponential: ln(2) / lambda
    float true_median = std::log(2.0f) / true_lambda;
    EXPECT_NEAR(km_median, true_median, 2.0f)
        << "KM median should be close to theoretical median";

    // Survival at end should be < 1
    EXPECT_LT(km_survival.back(), 1.0f);
    EXPECT_GE(km_survival.back(), 0.0f);

    double elapsed = STOP_TIMER_MS();
    EXPECT_LT(elapsed, 60000.0);
    std::cout << "Kaplan-Meier estimation completed in " << elapsed << " ms\n";
    std::cout << "KM median: " << km_median << " (true: " << true_median << ")\n";
}

//=============================================================================
// E2E Test 2: Log-Rank Test for Survival Comparison
//=============================================================================

TEST_F(SurvivalBayesianE2ETest, LogRankSurvivalComparison) {
    START_TIMER();

    // Generate two groups with different hazard rates
    const size_t n_per_group = 150;
    const float hazard_ratio = 0.6f;  // Treatment is beneficial
    auto data = generate_two_group_survival(n_per_group, 0.1f, hazard_ratio);

    // Combine and sort by time
    size_t n_total = data.times.size();
    std::vector<size_t> order(n_total);
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(),
              [&](size_t a, size_t b) { return data.times[a] < data.times[b]; });

    // Log-rank test
    double O1 = 0, E1 = 0;  // Observed and expected events in group 1
    size_t n0_at_risk = n_per_group, n1_at_risk = n_per_group;
    double var = 0;

    for (size_t i = 0; i < n_total; i++) {
        size_t idx = order[i];
        size_t n_at_risk = n0_at_risk + n1_at_risk;

        if (data.events[idx] && n_at_risk > 1) {
            // At this time point
            double expected1 = (double)n1_at_risk / n_at_risk;
            if (data.groups[idx] == 1) {
                O1 += 1;
            }
            E1 += expected1;

            // Variance contribution
            var += expected1 * (1 - expected1) * (double)(n_at_risk - 1) / (n_at_risk);
        }

        // Update at-risk counts
        if (data.groups[idx] == 0) n0_at_risk--;
        else n1_at_risk--;
    }

    // Log-rank statistic
    double chi_sq = (O1 - E1) * (O1 - E1) / var;
    // Approximate p-value from chi-squared(1) distribution
    float p_value = 1.0f - nimcp_stats_cdf_chi_squared(static_cast<float>(chi_sq), 1.0f);

    // With HR=0.6, groups should be significantly different
    EXPECT_LT(p_value, 0.10f) << "Groups should show significant difference";
    EXPECT_NE(O1, E1) << "Observed should differ from expected";

    double elapsed = STOP_TIMER_MS();
    EXPECT_LT(elapsed, 60000.0);
    std::cout << "Log-rank test completed in " << elapsed << " ms\n";
    std::cout << "Chi-squared: " << chi_sq << ", p-value: " << p_value << "\n";
    std::cout << "Observed events in treatment: " << O1 << ", Expected: " << E1 << "\n";
}

//=============================================================================
// E2E Test 3: Hazard Rate Estimation
//=============================================================================

TEST_F(SurvivalBayesianE2ETest, HazardRateEstimation) {
    START_TIMER();

    // Generate constant hazard (exponential) data
    const size_t n_patients = 500;
    const float true_lambda = 0.2f;
    auto data = generate_exponential_survival(n_patients, true_lambda, 0.15f);

    // Nelson-Aalen cumulative hazard estimation
    std::vector<size_t> order(n_patients);
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(),
              [&](size_t a, size_t b) { return data.times[a] < data.times[b]; });

    std::vector<float> na_times;
    std::vector<float> na_cumhaz;
    float cumhaz = 0.0f;
    size_t at_risk = n_patients;

    na_times.push_back(0.0f);
    na_cumhaz.push_back(0.0f);

    for (size_t i = 0; i < n_patients; i++) {
        size_t idx = order[i];
        if (data.events[idx]) {
            cumhaz += 1.0f / at_risk;
            na_times.push_back(data.times[idx]);
            na_cumhaz.push_back(cumhaz);
        }
        at_risk--;
    }

    // For constant hazard, cumulative hazard H(t) = lambda * t
    // Estimate lambda by regression of H(t) vs t
    if (na_times.size() > 10) {
        nimcp_regression_result_t reg;
        ASSERT_EQ(nimcp_stats_regression_linear(
            na_times.data(), na_cumhaz.data(), na_times.size(), &reg),
            NIMCP_STATS_OK);

        // Slope should estimate lambda
        EXPECT_NEAR(reg.slope, true_lambda, 0.1f)
            << "Estimated hazard rate should be close to true rate";

        // Good fit for exponential survival
        EXPECT_GT(reg.r_squared, 0.8f) << "Good linear fit expected for constant hazard";
    }

    // Alternative: estimate from total events / total time at risk
    float total_events = 0, total_time = 0;
    for (size_t i = 0; i < n_patients; i++) {
        if (data.events[i]) total_events += 1;
        total_time += data.times[i];
    }
    float simple_lambda_est = total_events / total_time;

    EXPECT_NEAR(simple_lambda_est, true_lambda, 0.1f);

    double elapsed = STOP_TIMER_MS();
    EXPECT_LT(elapsed, 60000.0);
    std::cout << "Hazard rate estimation completed in " << elapsed << " ms\n";
    std::cout << "Simple estimate: " << simple_lambda_est << " (true: " << true_lambda << ")\n";
}

//=============================================================================
// E2E Test 4: Cox Proportional Hazards Analysis (Simplified)
//=============================================================================

TEST_F(SurvivalBayesianE2ETest, CoxProportionalHazardsSimplified) {
    START_TIMER();

    // Generate survival data with covariate effect
    const size_t n_patients = 300;
    const float baseline_lambda = 0.1f;
    const float true_beta = 0.5f;  // log(HR) for covariate

    std::vector<float> times(n_patients);
    std::vector<bool> events(n_patients);
    std::vector<float> covariate(n_patients);  // Binary covariate

    std::exponential_distribution<float> base_surv(baseline_lambda);
    std::uniform_real_distribution<float> uni(0.0f, 1.0f);

    for (size_t i = 0; i < n_patients; i++) {
        covariate[i] = (i < n_patients / 2) ? 0.0f : 1.0f;
        float lambda_i = baseline_lambda * std::exp(true_beta * covariate[i]);
        std::exponential_distribution<float> surv_i(lambda_i);
        times[i] = surv_i(rng);
        events[i] = (uni(rng) > 0.1f);  // 10% censoring
    }

    // Simple score test approximation for single binary covariate
    // Compare event rates between groups
    float events_0 = 0, time_0 = 0;
    float events_1 = 0, time_1 = 0;

    for (size_t i = 0; i < n_patients; i++) {
        if (covariate[i] < 0.5f) {
            if (events[i]) events_0++;
            time_0 += times[i];
        } else {
            if (events[i]) events_1++;
            time_1 += times[i];
        }
    }

    // Estimated hazard ratio
    float lambda_0 = events_0 / time_0;
    float lambda_1 = events_1 / time_1;
    float estimated_hr = lambda_1 / lambda_0;
    float estimated_beta = std::log(estimated_hr);

    // True HR = exp(0.5) ≈ 1.65
    float true_hr = std::exp(true_beta);
    EXPECT_NEAR(estimated_hr, true_hr, 0.5f)
        << "Estimated HR should be close to true HR";
    EXPECT_NEAR(estimated_beta, true_beta, 0.3f)
        << "Estimated log-HR should be close to true beta";

    double elapsed = STOP_TIMER_MS();
    EXPECT_LT(elapsed, 60000.0);
    std::cout << "Cox PH analysis completed in " << elapsed << " ms\n";
    std::cout << "Estimated HR: " << estimated_hr << " (true: " << true_hr << ")\n";
    std::cout << "Estimated beta: " << estimated_beta << " (true: " << true_beta << ")\n";
}

//=============================================================================
// E2E Test 5: Bayesian A/B Testing
//=============================================================================

TEST_F(SurvivalBayesianE2ETest, BayesianABTesting) {
    START_TIMER();

    // Simulate A/B test: Control vs Treatment
    const uint32_t n_control = 1000;
    const uint32_t n_treatment = 1000;
    const float p_control = 0.10f;    // 10% conversion
    const float p_treatment = 0.12f;  // 12% conversion

    auto [control_conversions, _1] = generate_binomial(n_control, p_control);
    auto [treat_conversions, _2] = generate_binomial(n_treatment, p_treatment);

    // Beta prior: Beta(1, 1) = Uniform
    float prior_alpha = 1.0f, prior_beta = 1.0f;

    // Posterior for control: Beta(alpha + conversions, beta + non-conversions)
    nimcp_bayesian_result_t control_post, treat_post;

    ASSERT_EQ(nimcp_stats_bayesian_beta_binomial(
        prior_alpha, prior_beta,
        control_conversions, n_control,
        0.95f, &control_post), NIMCP_STATS_OK);

    ASSERT_EQ(nimcp_stats_bayesian_beta_binomial(
        prior_alpha, prior_beta,
        treat_conversions, n_treatment,
        0.95f, &treat_post), NIMCP_STATS_OK);

    // Monte Carlo estimation of P(treatment > control)
    const size_t n_samples = 10000;
    size_t treatment_wins = 0;

    std::gamma_distribution<float> control_gamma_a(prior_alpha + control_conversions, 1.0f);
    std::gamma_distribution<float> control_gamma_b(prior_beta + n_control - control_conversions, 1.0f);
    std::gamma_distribution<float> treat_gamma_a(prior_alpha + treat_conversions, 1.0f);
    std::gamma_distribution<float> treat_gamma_b(prior_beta + n_treatment - treat_conversions, 1.0f);

    for (size_t i = 0; i < n_samples; i++) {
        // Sample from Beta by ratio of gammas
        float c_a = control_gamma_a(rng), c_b = control_gamma_b(rng);
        float t_a = treat_gamma_a(rng), t_b = treat_gamma_b(rng);
        float control_sample = c_a / (c_a + c_b);
        float treat_sample = t_a / (t_a + t_b);
        if (treat_sample > control_sample) treatment_wins++;
    }

    float prob_treatment_better = static_cast<float>(treatment_wins) / n_samples;

    // With 2% absolute lift, should see treatment better > 50% of time
    EXPECT_GT(prob_treatment_better, 0.5f)
        << "Treatment should likely be better than control";

    // Credible intervals should contain true values
    EXPECT_LT(control_post.credible_lower, p_control + 0.02f);
    EXPECT_GT(control_post.credible_upper, p_control - 0.02f);

    double elapsed = STOP_TIMER_MS();
    EXPECT_LT(elapsed, 60000.0);
    std::cout << "Bayesian A/B testing completed in " << elapsed << " ms\n";
    std::cout << "Control posterior mean: " << control_post.posterior_mean
              << ", Treatment: " << treat_post.posterior_mean << "\n";
    std::cout << "P(treatment > control): " << prob_treatment_better << "\n";
}

//=============================================================================
// E2E Test 6: Conjugate Prior Updates (Beta-Binomial)
//=============================================================================

TEST_F(SurvivalBayesianE2ETest, ConjugateBetaBinomialUpdates) {
    START_TIMER();

    // Sequential updating as data arrives
    const size_t n_batches = 10;
    const uint32_t batch_size = 50;
    const float true_p = 0.30f;

    // Start with weakly informative prior
    float alpha = 2.0f, beta = 5.0f;  // Prior mean ≈ 0.29

    std::vector<float> posterior_means;
    std::vector<float> credible_widths;

    for (size_t batch = 0; batch < n_batches; batch++) {
        auto [successes, trials] = generate_binomial(batch_size, true_p);

        nimcp_bayesian_result_t result;
        ASSERT_EQ(nimcp_stats_bayesian_beta_binomial(
            alpha, beta, successes, trials, 0.95f, &result), NIMCP_STATS_OK);

        posterior_means.push_back(result.posterior_mean);
        credible_widths.push_back(result.credible_upper - result.credible_lower);

        // Update prior for next batch
        alpha += successes;
        beta += (trials - successes);
    }

    // Posterior should converge to true value
    EXPECT_NEAR(posterior_means.back(), true_p, 0.05f);

    // Credible intervals should narrow over time
    EXPECT_LT(credible_widths.back(), credible_widths.front());

    // Final credible interval should contain true value
    nimcp_bayesian_result_t final_result;
    ASSERT_EQ(nimcp_stats_bayesian_beta_binomial(
        alpha, beta, 0, 0, 0.95f, &final_result), NIMCP_STATS_OK);

    EXPECT_LT(final_result.credible_lower, true_p);
    EXPECT_GT(final_result.credible_upper, true_p);

    double elapsed = STOP_TIMER_MS();
    EXPECT_LT(elapsed, 60000.0);
    std::cout << "Conjugate updates completed in " << elapsed << " ms\n";
    std::cout << "Final posterior mean: " << posterior_means.back()
              << " (true: " << true_p << ")\n";
    std::cout << "Final CI width: " << credible_widths.back() << "\n";
}

//=============================================================================
// E2E Test 7: Bayesian Parameter Estimation
//=============================================================================

TEST_F(SurvivalBayesianE2ETest, BayesianParameterEstimation) {
    START_TIMER();

    // Estimate mean of normal distribution with known variance
    const size_t n_samples = 100;
    const float true_mean = 5.0f;
    const float known_variance = 4.0f;  // known sigma^2

    auto data = generate_normal(n_samples, true_mean, std::sqrt(known_variance));

    // Normal prior: N(prior_mean, prior_var)
    float prior_mean = 0.0f;
    float prior_variance = 100.0f;  // Weakly informative

    nimcp_bayesian_result_t result;
    ASSERT_EQ(nimcp_stats_bayesian_normal(
        prior_mean, prior_variance,
        data.data(), n_samples,
        known_variance,
        0.95f, &result), NIMCP_STATS_OK);

    // Posterior mean should be close to sample mean
    float sample_mean = nimcp_stats_mean(data.data(), n_samples);
    EXPECT_NEAR(result.posterior_mean, sample_mean, 0.5f);

    // Posterior should be close to true mean
    EXPECT_NEAR(result.posterior_mean, true_mean, 1.0f);

    // Posterior variance should be smaller than prior
    EXPECT_LT(result.posterior_variance, prior_variance);

    // Credible interval should contain true value
    EXPECT_LT(result.credible_lower, true_mean);
    EXPECT_GT(result.credible_upper, true_mean);

    double elapsed = STOP_TIMER_MS();
    EXPECT_LT(elapsed, 60000.0);
    std::cout << "Bayesian parameter estimation completed in " << elapsed << " ms\n";
    std::cout << "Posterior mean: " << result.posterior_mean << " (true: " << true_mean << ")\n";
    std::cout << "95% CI: [" << result.credible_lower << ", " << result.credible_upper << "]\n";
}

//=============================================================================
// E2E Test 8: Posterior Predictive Checking
//=============================================================================

TEST_F(SurvivalBayesianE2ETest, PosteriorPredictiveChecking) {
    START_TIMER();

    // Fit model to data, then check if model generates similar data
    const size_t n_samples = 200;
    const float true_p = 0.4f;

    auto [successes, trials] = generate_binomial(n_samples, true_p);

    // Bayesian update
    nimcp_bayesian_result_t result;
    ASSERT_EQ(nimcp_stats_bayesian_beta_binomial(
        1.0f, 1.0f, successes, static_cast<uint32_t>(n_samples), 0.95f, &result),
        NIMCP_STATS_OK);

    // Generate posterior predictive samples
    const size_t n_pred_samples = 1000;
    std::vector<float> pred_proportions(n_pred_samples);

    float post_alpha = 1.0f + successes;
    float post_beta = 1.0f + n_samples - successes;

    std::gamma_distribution<float> gamma_a(post_alpha, 1.0f);
    std::gamma_distribution<float> gamma_b(post_beta, 1.0f);

    for (size_t i = 0; i < n_pred_samples; i++) {
        // Sample p from posterior
        float a = gamma_a(rng), b = gamma_b(rng);
        float p_sample = a / (a + b);
        // Sample data from binomial with this p
        std::binomial_distribution<uint32_t> binom(n_samples, p_sample);
        pred_proportions[i] = static_cast<float>(binom(rng)) / n_samples;
    }

    // Compute posterior predictive statistics
    float pred_mean = nimcp_stats_mean(pred_proportions.data(), n_pred_samples);
    float pred_std = nimcp_stats_std_dev(pred_proportions.data(), n_pred_samples);

    // Observed proportion
    float observed_prop = static_cast<float>(successes) / n_samples;

    // Observed should be within predictive distribution
    float pred_lower = nimcp_stats_quantile(pred_proportions.data(), n_pred_samples, 0.025f);
    float pred_upper = nimcp_stats_quantile(pred_proportions.data(), n_pred_samples, 0.975f);

    EXPECT_GT(observed_prop, pred_lower - 0.05f);
    EXPECT_LT(observed_prop, pred_upper + 0.05f);

    // Predictive mean should match posterior mean
    EXPECT_NEAR(pred_mean, result.posterior_mean, 0.05f);

    double elapsed = STOP_TIMER_MS();
    EXPECT_LT(elapsed, 60000.0);
    std::cout << "Posterior predictive checking completed in " << elapsed << " ms\n";
    std::cout << "Observed: " << observed_prop << ", Predictive mean: " << pred_mean << "\n";
    std::cout << "Predictive 95% interval: [" << pred_lower << ", " << pred_upper << "]\n";
}

//=============================================================================
// E2E Test 9: Model Comparison with Bayes Factors
//=============================================================================

TEST_F(SurvivalBayesianE2ETest, ModelComparisonBayesFactors) {
    START_TIMER();

    // Compare two models: coin is fair vs coin is biased
    const uint32_t n_flips = 100;
    const float true_p = 0.65f;  // Actually biased

    auto [heads, _] = generate_binomial(n_flips, true_p);

    // Model 1 (H0): p = 0.5 (fair coin)
    // Likelihood: C(n,k) * 0.5^n
    float log_ml_fair = std::log(nimcp_stats_binomial_coef(n_flips, heads))
                      + n_flips * std::log(0.5f);

    // Model 2 (H1): p ~ Beta(1,1), integrate over p
    // Marginal likelihood: C(n,k) * B(k+1, n-k+1) / B(1,1)
    // = C(n,k) * Gamma(k+1)*Gamma(n-k+1) / Gamma(n+2)
    double log_ml_biased = std::log(nimcp_stats_binomial_coef(n_flips, heads))
                         + nimcp_stats_lgamma(heads + 1)
                         + nimcp_stats_lgamma(n_flips - heads + 1)
                         - nimcp_stats_lgamma(n_flips + 2);

    // Bayes factor
    float bf_biased_vs_fair = nimcp_stats_bayes_factor(
        static_cast<float>(log_ml_biased), log_ml_fair);

    // With p=0.65 and 100 flips, biased model should be favored
    EXPECT_GT(bf_biased_vs_fair, 1.0f)
        << "Biased model should be favored over fair model";

    // Interpretation
    std::string evidence;
    if (bf_biased_vs_fair > 100) evidence = "decisive";
    else if (bf_biased_vs_fair > 30) evidence = "very strong";
    else if (bf_biased_vs_fair > 10) evidence = "strong";
    else if (bf_biased_vs_fair > 3) evidence = "moderate";
    else if (bf_biased_vs_fair > 1) evidence = "weak";
    else evidence = "against";

    double elapsed = STOP_TIMER_MS();
    EXPECT_LT(elapsed, 60000.0);
    std::cout << "Bayes factor analysis completed in " << elapsed << " ms\n";
    std::cout << "Heads: " << heads << "/" << n_flips << "\n";
    std::cout << "BF (biased vs fair): " << bf_biased_vs_fair
              << " (" << evidence << " evidence for biased)\n";
}

//=============================================================================
// E2E Test 10: Credible Interval Estimation
//=============================================================================

TEST_F(SurvivalBayesianE2ETest, CredibleIntervalEstimation) {
    START_TIMER();

    // Test different credible levels
    const size_t n_samples = 200;
    const float true_p = 0.35f;

    auto [successes, trials] = generate_binomial(n_samples, true_p);

    std::vector<float> credible_levels = {0.50f, 0.80f, 0.90f, 0.95f, 0.99f};
    std::vector<float> ci_widths;

    for (float level : credible_levels) {
        nimcp_bayesian_result_t result;
        ASSERT_EQ(nimcp_stats_bayesian_beta_binomial(
            1.0f, 1.0f, successes, static_cast<uint32_t>(n_samples), level, &result),
            NIMCP_STATS_OK);

        float width = result.credible_upper - result.credible_lower;
        ci_widths.push_back(width);

        // Verify credible level is stored
        EXPECT_NEAR(result.credible_level, level, 0.01f);
    }

    // Higher credible levels should have wider intervals
    for (size_t i = 1; i < ci_widths.size(); i++) {
        EXPECT_GT(ci_widths[i], ci_widths[i-1])
            << "Higher credible level should give wider interval";
    }

    // 95% CI should be wider than 50% CI by substantial margin
    EXPECT_GT(ci_widths[3], ci_widths[0] * 1.5f);

    double elapsed = STOP_TIMER_MS();
    EXPECT_LT(elapsed, 60000.0);
    std::cout << "Credible interval estimation completed in " << elapsed << " ms\n";
    for (size_t i = 0; i < credible_levels.size(); i++) {
        std::cout << (int)(credible_levels[i] * 100) << "% CI width: "
                  << ci_widths[i] << "\n";
    }
}

//=============================================================================
// E2E Test 11: Sequential Bayesian Updating
//=============================================================================

TEST_F(SurvivalBayesianE2ETest, SequentialBayesianUpdating) {
    START_TIMER();

    // Simulate online learning with sequential updates
    const size_t total_samples = 1000;
    const float true_rate = 5.0f;  // Poisson rate

    std::poisson_distribution<uint32_t> pois(true_rate);

    // Gamma prior for Poisson rate: Gamma(alpha, beta)
    // Prior mean = alpha/beta, prior variance = alpha/beta^2
    float alpha = 2.0f, beta = 1.0f;  // Prior mean = 2

    std::vector<float> posterior_means;
    std::vector<float> posterior_vars;
    float exposure = 0.0f;
    uint32_t total_events = 0;

    for (size_t i = 0; i < total_samples; i++) {
        uint32_t events = pois(rng);
        total_events += events;
        exposure += 1.0f;

        // Update: Gamma(alpha + events, beta + exposure)
        nimcp_bayesian_result_t result;
        ASSERT_EQ(nimcp_stats_bayesian_gamma_poisson(
            alpha, beta + exposure - 1.0f,  // Adjust for cumulative
            total_events, exposure,
            0.95f, &result), NIMCP_STATS_OK);

        if ((i + 1) % 100 == 0) {
            posterior_means.push_back(result.posterior_mean);
            posterior_vars.push_back(result.posterior_variance);
        }
    }

    // Posterior should converge to true rate
    EXPECT_NEAR(posterior_means.back(), true_rate, 0.5f);

    // Variance should decrease over time
    EXPECT_LT(posterior_vars.back(), posterior_vars.front());

    // Learning curve should show convergence
    nimcp_correlation_result_t convergence;
    std::vector<float> indices(posterior_means.size());
    std::iota(indices.begin(), indices.end(), 0.0f);

    // Distance from true should decrease
    std::vector<float> errors(posterior_means.size());
    for (size_t i = 0; i < posterior_means.size(); i++) {
        errors[i] = std::abs(posterior_means[i] - true_rate);
    }

    ASSERT_EQ(nimcp_stats_correlation_pearson(
        indices.data(), errors.data(), errors.size(), &convergence), NIMCP_STATS_OK);

    EXPECT_LT(convergence.r, 0.0f) << "Error should decrease over time (negative correlation)";

    double elapsed = STOP_TIMER_MS();
    EXPECT_LT(elapsed, 60000.0);
    std::cout << "Sequential Bayesian updating completed in " << elapsed << " ms\n";
    std::cout << "Final posterior mean: " << posterior_means.back()
              << " (true: " << true_rate << ")\n";
    std::cout << "Error-time correlation: " << convergence.r << "\n";
}

//=============================================================================
// E2E Test 12: Hierarchical Bayesian Analysis (Simplified)
//=============================================================================

TEST_F(SurvivalBayesianE2ETest, HierarchicalBayesianSimplified) {
    START_TIMER();

    // Multiple groups with common hyperprior
    const size_t n_groups = 5;
    const size_t samples_per_group = 50;

    // True group means drawn from hyperprior: N(10, 4)
    std::normal_distribution<float> hyper_mean(10.0f, 2.0f);
    const float within_group_std = 1.0f;

    std::vector<float> true_group_means(n_groups);
    std::vector<std::vector<float>> group_data(n_groups);
    std::vector<float> group_sample_means(n_groups);

    for (size_t g = 0; g < n_groups; g++) {
        true_group_means[g] = hyper_mean(rng);
        group_data[g] = generate_normal(samples_per_group, true_group_means[g], within_group_std);
        group_sample_means[g] = nimcp_stats_mean(group_data[g].data(), samples_per_group);
    }

    // Estimate hyperparameters from group means
    float hyper_mean_est = nimcp_stats_mean(group_sample_means.data(), n_groups);
    float hyper_var_est = nimcp_stats_variance(group_sample_means.data(), n_groups);

    // Shrinkage estimation: group means shrink toward grand mean
    std::vector<float> shrunk_estimates(n_groups);
    float shrinkage_factor = hyper_var_est / (hyper_var_est + within_group_std * within_group_std / samples_per_group);

    for (size_t g = 0; g < n_groups; g++) {
        shrunk_estimates[g] = shrinkage_factor * group_sample_means[g]
                            + (1.0f - shrinkage_factor) * hyper_mean_est;
    }

    // Shrunk estimates should be closer to true means on average
    float raw_mse = 0.0f, shrunk_mse = 0.0f;
    for (size_t g = 0; g < n_groups; g++) {
        raw_mse += std::pow(group_sample_means[g] - true_group_means[g], 2);
        shrunk_mse += std::pow(shrunk_estimates[g] - true_group_means[g], 2);
    }
    raw_mse /= n_groups;
    shrunk_mse /= n_groups;

    // With proper shrinkage, shrunk MSE should typically be lower
    // (James-Stein phenomenon)
    std::cout << "Raw MSE: " << raw_mse << ", Shrunk MSE: " << shrunk_mse << "\n";

    // Hyperparameter estimates should be reasonable
    EXPECT_NEAR(hyper_mean_est, 10.0f, 3.0f);
    EXPECT_GT(hyper_var_est, 0.0f);

    double elapsed = STOP_TIMER_MS();
    EXPECT_LT(elapsed, 60000.0);
    std::cout << "Hierarchical Bayesian analysis completed in " << elapsed << " ms\n";
    std::cout << "Estimated hyper mean: " << hyper_mean_est << " (true: 10.0)\n";
    std::cout << "Shrinkage factor: " << shrinkage_factor << "\n";
}

//=============================================================================
// E2E Test 13: Bayesian Regression
//=============================================================================

TEST_F(SurvivalBayesianE2ETest, BayesianRegression) {
    START_TIMER();

    // Simple linear regression with uncertainty
    const size_t n_samples = 100;
    const float true_slope = 2.0f;
    const float true_intercept = 1.0f;
    const float noise_std = 0.5f;

    std::vector<float> x(n_samples), y(n_samples);
    std::uniform_real_distribution<float> x_dist(0.0f, 5.0f);
    std::normal_distribution<float> noise(0.0f, noise_std);

    for (size_t i = 0; i < n_samples; i++) {
        x[i] = x_dist(rng);
        y[i] = true_intercept + true_slope * x[i] + noise(rng);
    }

    // Frequentist fit for comparison
    nimcp_regression_result_t freq_result;
    ASSERT_EQ(nimcp_stats_regression_linear(x.data(), y.data(), n_samples, &freq_result),
              NIMCP_STATS_OK);

    // Bootstrap for uncertainty (pseudo-Bayesian)
    const size_t n_bootstrap = 500;
    std::vector<float> boot_slopes(n_bootstrap);
    std::vector<float> boot_intercepts(n_bootstrap);

    for (size_t b = 0; b < n_bootstrap; b++) {
        std::vector<float> boot_x(n_samples), boot_y(n_samples);
        std::uniform_int_distribution<size_t> idx_dist(0, n_samples - 1);

        for (size_t i = 0; i < n_samples; i++) {
            size_t idx = idx_dist(rng);
            boot_x[i] = x[idx];
            boot_y[i] = y[idx];
        }

        nimcp_regression_result_t boot_reg;
        if (nimcp_stats_regression_linear(boot_x.data(), boot_y.data(),
                                          n_samples, &boot_reg) == NIMCP_STATS_OK) {
            boot_slopes[b] = boot_reg.slope;
            boot_intercepts[b] = boot_reg.intercept;
        }
    }

    // Posterior-like summaries
    float slope_mean = nimcp_stats_mean(boot_slopes.data(), n_bootstrap);
    float slope_ci_lower = nimcp_stats_quantile(boot_slopes.data(), n_bootstrap, 0.025f);
    float slope_ci_upper = nimcp_stats_quantile(boot_slopes.data(), n_bootstrap, 0.975f);

    // Point estimate should be close to truth
    EXPECT_NEAR(slope_mean, true_slope, 0.3f);

    // Credible interval should contain true value
    EXPECT_LT(slope_ci_lower, true_slope);
    EXPECT_GT(slope_ci_upper, true_slope);

    // Bootstrap mean should be close to frequentist estimate
    EXPECT_NEAR(slope_mean, freq_result.slope, 0.1f);

    double elapsed = STOP_TIMER_MS();
    EXPECT_LT(elapsed, 60000.0);
    std::cout << "Bayesian regression completed in " << elapsed << " ms\n";
    std::cout << "Slope: " << slope_mean << " (true: " << true_slope << ")\n";
    std::cout << "95% CI: [" << slope_ci_lower << ", " << slope_ci_upper << "]\n";
}

//=============================================================================
// E2E Test 14: Survival Time Prediction
//=============================================================================

TEST_F(SurvivalBayesianE2ETest, SurvivalTimePrediction) {
    START_TIMER();

    // Generate training data
    const size_t n_train = 300;
    const float true_lambda = 0.15f;
    auto train_data = generate_exponential_survival(n_train, true_lambda, 0.2f);

    // Estimate hazard rate from training data
    float total_events = 0, total_time = 0;
    for (size_t i = 0; i < n_train; i++) {
        if (train_data.events[i]) total_events++;
        total_time += train_data.times[i];
    }
    float estimated_lambda = total_events / total_time;

    // Bayesian estimate with Gamma prior
    nimcp_bayesian_result_t hazard_post;
    ASSERT_EQ(nimcp_stats_bayesian_gamma_poisson(
        1.0f, 1.0f,  // Weakly informative prior
        static_cast<uint32_t>(total_events), total_time,
        0.95f, &hazard_post), NIMCP_STATS_OK);

    // Predict survival probabilities at fixed times
    std::vector<float> pred_times = {1.0f, 2.0f, 5.0f, 10.0f, 20.0f};
    std::vector<float> pred_survival;
    std::vector<float> true_survival;

    for (float t : pred_times) {
        // S(t) = exp(-lambda * t)
        pred_survival.push_back(std::exp(-hazard_post.posterior_mean * t));
        true_survival.push_back(std::exp(-true_lambda * t));
    }

    // Predictions should be reasonable
    for (size_t i = 0; i < pred_times.size(); i++) {
        EXPECT_NEAR(pred_survival[i], true_survival[i], 0.15f)
            << "Survival prediction at t=" << pred_times[i] << " should be accurate";
    }

    // Generate test data to validate
    const size_t n_test = 100;
    auto test_data = generate_exponential_survival(n_test, true_lambda, 0.1f);

    // Compute calibration: actual survival fraction vs predicted
    for (float t : pred_times) {
        size_t survived = 0;
        for (size_t i = 0; i < n_test; i++) {
            if (test_data.times[i] > t || !test_data.events[i]) survived++;
        }
        float empirical_survival = static_cast<float>(survived) / n_test;
        float predicted = std::exp(-hazard_post.posterior_mean * t);

        // Allow for sampling variability
        EXPECT_NEAR(empirical_survival, predicted, 0.25f);
    }

    double elapsed = STOP_TIMER_MS();
    EXPECT_LT(elapsed, 60000.0);
    std::cout << "Survival prediction completed in " << elapsed << " ms\n";
    std::cout << "Estimated lambda: " << hazard_post.posterior_mean
              << " (true: " << true_lambda << ")\n";
}

//=============================================================================
// E2E Test 15: Clinical Trial Interim Analysis
//=============================================================================

TEST_F(SurvivalBayesianE2ETest, ClinicalTrialInterimAnalysis) {
    START_TIMER();

    // Simulate adaptive clinical trial with interim looks
    const size_t max_patients = 500;
    const size_t interim_intervals = 5;
    const size_t patients_per_interim = max_patients / interim_intervals;

    const float p_control = 0.30f;   // 30% response in control
    const float p_treatment = 0.40f; // 40% response in treatment (true effect)

    // Thresholds for early stopping
    const float efficacy_threshold = 0.95f;   // Stop if P(treatment > control) > 0.95
    const float futility_threshold = 0.20f;   // Stop if P(treatment > control) < 0.20

    uint32_t control_n = 0, control_resp = 0;
    uint32_t treat_n = 0, treat_resp = 0;

    bool stopped_early = false;
    std::string stop_reason;
    size_t stop_interim = 0;

    std::vector<float> prob_treatment_better_history;

    for (size_t interim = 0; interim < interim_intervals; interim++) {
        // Enroll more patients
        for (size_t i = 0; i < patients_per_interim / 2; i++) {
            // Control arm
            auto [c_resp, _1] = generate_binomial(1, p_control);
            control_n++;
            control_resp += c_resp;

            // Treatment arm
            auto [t_resp, _2] = generate_binomial(1, p_treatment);
            treat_n++;
            treat_resp += t_resp;
        }

        // Bayesian analysis
        nimcp_bayesian_result_t control_post, treat_post;
        nimcp_stats_bayesian_beta_binomial(1.0f, 1.0f, control_resp, control_n,
                                           0.95f, &control_post);
        nimcp_stats_bayesian_beta_binomial(1.0f, 1.0f, treat_resp, treat_n,
                                           0.95f, &treat_post);

        // Monte Carlo P(treatment > control)
        const size_t mc_samples = 5000;
        size_t treatment_wins = 0;

        std::gamma_distribution<float> ctrl_a(1.0f + control_resp, 1.0f);
        std::gamma_distribution<float> ctrl_b(1.0f + control_n - control_resp, 1.0f);
        std::gamma_distribution<float> trt_a(1.0f + treat_resp, 1.0f);
        std::gamma_distribution<float> trt_b(1.0f + treat_n - treat_resp, 1.0f);

        for (size_t s = 0; s < mc_samples; s++) {
            float ca = ctrl_a(rng), cb = ctrl_b(rng);
            float ta = trt_a(rng), tb = trt_b(rng);
            if (ta / (ta + tb) > ca / (ca + cb)) treatment_wins++;
        }

        float prob_better = static_cast<float>(treatment_wins) / mc_samples;
        prob_treatment_better_history.push_back(prob_better);

        // Check stopping rules
        if (prob_better > efficacy_threshold) {
            stopped_early = true;
            stop_reason = "efficacy";
            stop_interim = interim + 1;
            break;
        } else if (prob_better < futility_threshold) {
            stopped_early = true;
            stop_reason = "futility";
            stop_interim = interim + 1;
            break;
        }
    }

    // With 10% absolute difference, should likely show efficacy by end
    float final_prob = prob_treatment_better_history.back();
    EXPECT_GT(final_prob, 0.5f) << "Treatment should likely be better";

    // Probability should increase over time (with true effect)
    if (prob_treatment_better_history.size() > 1) {
        nimcp_correlation_result_t trend;
        std::vector<float> indices(prob_treatment_better_history.size());
        std::iota(indices.begin(), indices.end(), 0.0f);
        nimcp_stats_correlation_pearson(indices.data(),
                                        prob_treatment_better_history.data(),
                                        prob_treatment_better_history.size(), &trend);
        // With true effect, evidence should accumulate
        std::cout << "Evidence trend correlation: " << trend.r << "\n";
    }

    double elapsed = STOP_TIMER_MS();
    EXPECT_LT(elapsed, 60000.0);
    std::cout << "Clinical trial interim analysis completed in " << elapsed << " ms\n";
    std::cout << "Total enrolled: " << (control_n + treat_n) << " patients\n";
    std::cout << "Control: " << control_resp << "/" << control_n
              << ", Treatment: " << treat_resp << "/" << treat_n << "\n";
    if (stopped_early) {
        std::cout << "Trial stopped early for " << stop_reason
                  << " at interim " << stop_interim << "\n";
    }
    std::cout << "Final P(treatment > control): " << final_prob << "\n";
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
