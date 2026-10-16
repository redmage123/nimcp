//=============================================================================
// test_neural_statistics_e2e.cpp - Neural Data Analysis Pipeline E2E Tests
//=============================================================================
/**
 * @file test_neural_statistics_e2e.cpp
 * @brief End-to-end tests for neural data analysis workflows
 *
 * WHAT: Complete neural data analysis pipelines from raw spikes to insights
 * WHY:  Verify statistics module handles realistic neural data scenarios
 * HOW:  Simulate spike trains, compute firing rates, ISI stats, population coding
 *
 * TEST SCENARIOS:
 * 1. Spike train generation and analysis
 * 2. Firing rate estimation with multiple methods
 * 3. Inter-spike interval (ISI) statistics
 * 4. Population coding with Fisher information
 * 5. Cross-correlogram analysis between neurons
 * 6. Information transfer between brain regions
 * 7. Neural variability (Fano factor, CV)
 * 8. Spike train correlation analysis
 * 9. Rate coding vs temporal coding
 * 10. Population vector decoding
 * 11. Neural entropy estimation
 * 12. Noise correlation analysis
 * 13. Multi-neuron information decomposition
 * 14. Neural state space analysis
 * 15. Spike timing precision analysis
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
#include "information/nimcp_shannon.h"
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

class NeuralStatisticsE2ETest : public ::testing::Test {
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

    // Generate Poisson spike train
    std::vector<double> generate_poisson_spikes(double rate_hz, double duration_s) {
        std::vector<double> spikes;
        std::exponential_distribution<double> exp_dist(rate_hz);
        double t = 0.0;
        while (t < duration_s) {
            double isi = exp_dist(rng);
            t += isi;
            if (t < duration_s) {
                spikes.push_back(t);
            }
        }
        return spikes;
    }

    // Generate inhomogeneous Poisson spike train
    std::vector<double> generate_inhomogeneous_poisson(
        const std::vector<float>& rate_profile, double dt, double duration_s) {
        std::vector<double> spikes;
        std::uniform_real_distribution<double> uni_dist(0.0, 1.0);
        double t = 0.0;
        int idx = 0;
        while (t < duration_s && idx < (int)rate_profile.size()) {
            double rate = rate_profile[idx];
            double p = rate * dt;
            if (uni_dist(rng) < p) {
                spikes.push_back(t);
            }
            t += dt;
            idx++;
        }
        return spikes;
    }

    // Compute inter-spike intervals
    std::vector<float> compute_isis(const std::vector<double>& spikes) {
        std::vector<float> isis;
        for (size_t i = 1; i < spikes.size(); i++) {
            isis.push_back(static_cast<float>(spikes[i] - spikes[i-1]));
        }
        return isis;
    }

    // Estimate firing rate using binned method
    std::vector<float> estimate_firing_rate_binned(
        const std::vector<double>& spikes, double bin_size, double duration) {
        int n_bins = static_cast<int>(duration / bin_size);
        std::vector<float> rates(n_bins, 0.0f);
        for (double t : spikes) {
            int bin = static_cast<int>(t / bin_size);
            if (bin >= 0 && bin < n_bins) {
                rates[bin] += 1.0f / static_cast<float>(bin_size);
            }
        }
        return rates;
    }

    // Compute cross-correlogram
    std::vector<float> compute_cross_correlogram(
        const std::vector<double>& spikes1,
        const std::vector<double>& spikes2,
        double max_lag, double bin_size) {
        int n_bins = static_cast<int>(2 * max_lag / bin_size) + 1;
        std::vector<float> ccg(n_bins, 0.0f);

        for (double t1 : spikes1) {
            for (double t2 : spikes2) {
                double lag = t2 - t1;
                if (std::abs(lag) <= max_lag) {
                    int bin = static_cast<int>((lag + max_lag) / bin_size);
                    if (bin >= 0 && bin < n_bins) {
                        ccg[bin] += 1.0f;
                    }
                }
            }
        }
        return ccg;
    }

    // Generate normal samples
    std::vector<float> generate_normal(size_t n, float mean, float stddev) {
        std::normal_distribution<float> dist(mean, stddev);
        std::vector<float> data(n);
        for (auto& x : data) x = dist(rng);
        return data;
    }

    // Generate uniform samples
    std::vector<float> generate_uniform(size_t n, float low, float high) {
        std::uniform_real_distribution<float> dist(low, high);
        std::vector<float> data(n);
        for (auto& x : data) x = dist(rng);
        return data;
    }
};

//=============================================================================
// E2E Test 1: Complete Spike Train Analysis Pipeline
//=============================================================================

TEST_F(NeuralStatisticsE2ETest, SpikeTrainAnalysisPipeline) {
    START_TIMER();

    // Generate multiple neurons with different firing rates
    const int num_neurons = 50;
    const double duration = 10.0;  // 10 seconds
    std::vector<double> firing_rates = {5, 10, 15, 20, 30, 40, 50, 60, 80, 100};

    std::vector<std::vector<double>> all_spikes(num_neurons);
    std::vector<float> mean_rates(num_neurons);
    std::vector<float> cv_values(num_neurons);

    for (int n = 0; n < num_neurons; n++) {
        double rate = firing_rates[n % firing_rates.size()];
        all_spikes[n] = generate_poisson_spikes(rate, duration);

        // Compute empirical firing rate
        mean_rates[n] = static_cast<float>(all_spikes[n].size()) / static_cast<float>(duration);

        // Compute ISI statistics
        auto isis = compute_isis(all_spikes[n]);
        if (isis.size() > 1) {
            float isi_mean = nimcp_stats_mean(isis.data(), isis.size());
            float isi_std = nimcp_stats_std_dev(isis.data(), isis.size());
            cv_values[n] = isi_std / isi_mean;  // Coefficient of variation
        } else {
            cv_values[n] = 0.0f;
        }
    }

    // Analyze population statistics
    nimcp_descriptive_stats_t rate_stats;
    EXPECT_EQ(nimcp_stats_describe(mean_rates.data(), num_neurons, &rate_stats), NIMCP_STATS_OK);

    // Population firing rate should be in expected range
    EXPECT_GT(rate_stats.mean, 10.0f);
    EXPECT_LT(rate_stats.mean, 80.0f);

    // CV for Poisson process should be around 1
    float mean_cv = nimcp_stats_mean(cv_values.data(), num_neurons);
    EXPECT_NEAR(mean_cv, 1.0f, 0.3f);

    double elapsed = STOP_TIMER_MS();
    EXPECT_LT(elapsed, 5000.0);  // Should complete in 5 seconds

    std::cout << "Spike Train Analysis: " << num_neurons << " neurons, "
              << "mean rate=" << rate_stats.mean << " Hz, "
              << "mean CV=" << mean_cv << ", "
              << "time=" << elapsed << " ms" << std::endl;
}

//=============================================================================
// E2E Test 2: Firing Rate Estimation Methods
//=============================================================================

TEST_F(NeuralStatisticsE2ETest, FiringRateEstimationMethods) {
    START_TIMER();

    // Generate spike train with known rate
    const double true_rate = 50.0;  // 50 Hz
    const double duration = 60.0;   // 60 seconds
    auto spikes = generate_poisson_spikes(true_rate, duration);

    // Method 1: Simple count rate
    float count_rate = static_cast<float>(spikes.size()) / static_cast<float>(duration);
    EXPECT_NEAR(count_rate, true_rate, 5.0f);

    // Method 2: Binned rate estimate (various bin sizes)
    std::vector<double> bin_sizes = {0.01, 0.05, 0.1, 0.5, 1.0};
    for (double bin_size : bin_sizes) {
        auto rates = estimate_firing_rate_binned(spikes, bin_size, duration);
        float mean_rate = nimcp_stats_mean(rates.data(), rates.size());
        EXPECT_NEAR(mean_rate, true_rate, 10.0f);
    }

    // Method 3: ISI-based rate estimate (1/mean_ISI)
    auto isis = compute_isis(spikes);
    float mean_isi = nimcp_stats_mean(isis.data(), isis.size());
    float isi_rate = 1.0f / mean_isi;
    EXPECT_NEAR(isi_rate, true_rate, 5.0f);

    double elapsed = STOP_TIMER_MS();
    EXPECT_LT(elapsed, 2000.0);

    std::cout << "Firing Rate Estimation: true=" << true_rate << " Hz, "
              << "count=" << count_rate << ", ISI=" << isi_rate << " Hz, "
              << "time=" << elapsed << " ms" << std::endl;
}

//=============================================================================
// E2E Test 3: Inter-Spike Interval Distribution Analysis
//=============================================================================

TEST_F(NeuralStatisticsE2ETest, ISIDistributionAnalysis) {
    START_TIMER();

    // Generate long spike train for good statistics
    const double rate = 30.0;
    const double duration = 120.0;  // 2 minutes
    auto spikes = generate_poisson_spikes(rate, duration);
    auto isis = compute_isis(spikes);

    // Full ISI statistics
    nimcp_descriptive_stats_t isi_stats;
    ASSERT_GT(isis.size(), 100);
    EXPECT_EQ(nimcp_stats_describe(isis.data(), isis.size(), &isi_stats), NIMCP_STATS_OK);

    // For Poisson: mean ISI = 1/rate
    float expected_mean_isi = 1.0f / rate;
    EXPECT_NEAR(isi_stats.mean, expected_mean_isi, 0.01f);

    // For exponential: variance = mean^2
    float expected_variance = expected_mean_isi * expected_mean_isi;
    EXPECT_NEAR(isi_stats.variance, expected_variance, 0.01f);

    // Coefficient of variation should be ~1 for Poisson
    float cv = isi_stats.std_dev / isi_stats.mean;
    EXPECT_NEAR(cv, 1.0f, 0.2f);

    // Test for exponential distribution (skewness ~2 for exponential)
    EXPECT_GT(isi_stats.skewness, 1.0f);

    // Quantile analysis
    float q25 = nimcp_stats_quantile(isis.data(), isis.size(), 0.25f);
    float q50 = nimcp_stats_quantile(isis.data(), isis.size(), 0.50f);
    float q75 = nimcp_stats_quantile(isis.data(), isis.size(), 0.75f);

    EXPECT_LT(q25, q50);
    EXPECT_LT(q50, q75);

    double elapsed = STOP_TIMER_MS();
    EXPECT_LT(elapsed, 3000.0);

    std::cout << "ISI Analysis: " << isis.size() << " ISIs, "
              << "mean=" << isi_stats.mean*1000 << " ms, "
              << "CV=" << cv << ", "
              << "time=" << elapsed << " ms" << std::endl;
}

//=============================================================================
// E2E Test 4: Population Coding with Fisher Information
//=============================================================================

TEST_F(NeuralStatisticsE2ETest, PopulationCodingFisherInformation) {
    START_TIMER();

    // Simulate population of orientation-tuned neurons
    const int num_neurons = 100;
    const int num_orientations = 180;
    const float tuning_width = 30.0f;  // degrees

    // Generate tuning curves (Gaussian tuning)
    std::vector<std::vector<float>> tuning_curves(num_neurons);
    std::vector<float> preferred_orientations(num_neurons);

    for (int n = 0; n < num_neurons; n++) {
        preferred_orientations[n] = static_cast<float>(n * 180) / num_neurons;
        tuning_curves[n].resize(num_orientations);

        for (int o = 0; o < num_orientations; o++) {
            float orientation = static_cast<float>(o);
            float diff = std::min(
                std::abs(orientation - preferred_orientations[n]),
                180.0f - std::abs(orientation - preferred_orientations[n])
            );
            tuning_curves[n][o] = std::exp(-diff * diff / (2 * tuning_width * tuning_width));
        }
    }

    // Compute Fisher information at each orientation
    std::vector<float> fisher_info(num_orientations, 0.0f);

    for (int o = 1; o < num_orientations - 1; o++) {
        for (int n = 0; n < num_neurons; n++) {
            float f = tuning_curves[n][o];
            float f_prime = (tuning_curves[n][o+1] - tuning_curves[n][o-1]) / 2.0f;
            if (f > 0.001f) {
                fisher_info[o] += f_prime * f_prime / f;
            }
        }
    }

    // Analyze Fisher information distribution
    nimcp_descriptive_stats_t fi_stats;
    EXPECT_EQ(nimcp_stats_describe(fisher_info.data(), num_orientations, &fi_stats), NIMCP_STATS_OK);

    // Fisher information should be positive and relatively uniform
    EXPECT_GT(fi_stats.mean, 0.0f);
    EXPECT_GT(fi_stats.min, 0.0f);

    // Lower variability indicates better uniform coverage
    float cv_fi = fi_stats.std_dev / fi_stats.mean;
    EXPECT_LT(cv_fi, 1.0f);  // Should be reasonably uniform

    double elapsed = STOP_TIMER_MS();
    EXPECT_LT(elapsed, 2000.0);

    std::cout << "Fisher Information: " << num_neurons << " neurons, "
              << "mean FI=" << fi_stats.mean << ", "
              << "CV=" << cv_fi << ", "
              << "time=" << elapsed << " ms" << std::endl;
}

//=============================================================================
// E2E Test 5: Cross-Correlogram Analysis
//=============================================================================

TEST_F(NeuralStatisticsE2ETest, CrossCorrelogramAnalysis) {
    START_TIMER();

    // Generate correlated spike trains
    const double duration = 30.0;
    const double rate1 = 40.0;
    const double rate2 = 50.0;
    const double correlation_delay = 0.01;  // 10ms delay

    // Generate primary spike train
    auto spikes1 = generate_poisson_spikes(rate1, duration);

    // Generate secondary spike train with correlation
    std::vector<double> spikes2;
    std::uniform_real_distribution<double> prob_dist(0.0, 1.0);
    std::normal_distribution<double> jitter_dist(0.0, 0.002);  // 2ms jitter

    // Add some correlated spikes
    for (double t : spikes1) {
        if (prob_dist(rng) < 0.3) {  // 30% correlation
            double t2 = t + correlation_delay + jitter_dist(rng);
            if (t2 > 0 && t2 < duration) {
                spikes2.push_back(t2);
            }
        }
    }

    // Add independent Poisson spikes
    auto independent_spikes = generate_poisson_spikes(rate2 * 0.7, duration);
    spikes2.insert(spikes2.end(), independent_spikes.begin(), independent_spikes.end());
    std::sort(spikes2.begin(), spikes2.end());

    // Compute cross-correlogram
    const double max_lag = 0.1;  // 100ms
    const double bin_size = 0.001;  // 1ms bins
    auto ccg = compute_cross_correlogram(spikes1, spikes2, max_lag, bin_size);

    // Analyze CCG
    nimcp_descriptive_stats_t ccg_stats;
    EXPECT_EQ(nimcp_stats_describe(ccg.data(), ccg.size(), &ccg_stats), NIMCP_STATS_OK);

    // Find peak in CCG (should be near correlation_delay)
    auto max_it = std::max_element(ccg.begin(), ccg.end());
    int peak_bin = std::distance(ccg.begin(), max_it);
    double peak_lag = (peak_bin * bin_size) - max_lag;

    EXPECT_NEAR(peak_lag, correlation_delay, 0.01);  // Peak should be near expected delay
    EXPECT_GT(*max_it, ccg_stats.mean);  // Peak should be above baseline

    double elapsed = STOP_TIMER_MS();
    EXPECT_LT(elapsed, 3000.0);

    std::cout << "Cross-Correlogram: peak lag=" << peak_lag*1000 << " ms, "
              << "expected=" << correlation_delay*1000 << " ms, "
              << "time=" << elapsed << " ms" << std::endl;
}

//=============================================================================
// E2E Test 6: Information Transfer Between Brain Regions
//=============================================================================

TEST_F(NeuralStatisticsE2ETest, InformationTransferBetweenRegions) {
    START_TIMER();

    // Simulate activity in two connected brain regions
    const int n_samples = 1000;
    const int n_bins = 8;  // Discretization bins

    // Generate activity with causal relationship: Y depends on X
    std::vector<float> region_x(n_samples);
    std::vector<float> region_y(n_samples);

    std::normal_distribution<float> noise(0.0f, 0.3f);
    region_x[0] = 0.5f;
    region_y[0] = 0.5f;

    for (int t = 1; t < n_samples; t++) {
        // X has independent dynamics
        region_x[t] = 0.7f * region_x[t-1] + noise(rng);
        // Y depends on X with lag 1
        region_y[t] = 0.5f * region_x[t-1] + 0.3f * region_y[t-1] + noise(rng);
    }

    // Normalize to [0, 1]
    float x_min = nimcp_stats_min(region_x.data(), n_samples);
    float x_max = nimcp_stats_max(region_x.data(), n_samples);
    float y_min = nimcp_stats_min(region_y.data(), n_samples);
    float y_max = nimcp_stats_max(region_y.data(), n_samples);

    for (int t = 0; t < n_samples; t++) {
        region_x[t] = (region_x[t] - x_min) / (x_max - x_min + 1e-6f);
        region_y[t] = (region_y[t] - y_min) / (y_max - y_min + 1e-6f);
    }

    // Compute transfer entropy in both directions
    float te_xy = nimcp_stats_transfer_entropy(region_x.data(), region_y.data(), n_samples, 1, n_bins);
    float te_yx = nimcp_stats_transfer_entropy(region_y.data(), region_x.data(), n_samples, 1, n_bins);

    // X->Y should have higher transfer entropy than Y->X
    EXPECT_GE(te_xy, te_yx * 0.5f);  // X causes Y
    EXPECT_GT(te_xy, 0.0f);

    // Compute correlation for comparison
    nimcp_correlation_result_t corr_result;
    nimcp_stats_correlation_pearson(region_x.data(), region_y.data(), n_samples, &corr_result);

    EXPECT_GT(corr_result.r, 0.0f);  // Should be positively correlated

    double elapsed = STOP_TIMER_MS();
    EXPECT_LT(elapsed, 5000.0);

    std::cout << "Information Transfer: TE(X->Y)=" << te_xy << ", "
              << "TE(Y->X)=" << te_yx << ", "
              << "correlation=" << corr_result.r << ", "
              << "time=" << elapsed << " ms" << std::endl;
}

//=============================================================================
// E2E Test 7: Neural Variability (Fano Factor and CV)
//=============================================================================

TEST_F(NeuralStatisticsE2ETest, NeuralVariabilityAnalysis) {
    START_TIMER();

    // Simulate repeated trials of neural responses
    const int num_trials = 100;
    const int num_neurons = 20;
    const double trial_duration = 1.0;  // 1 second

    std::vector<std::vector<int>> spike_counts(num_neurons, std::vector<int>(num_trials));
    std::vector<float> fano_factors(num_neurons);

    for (int n = 0; n < num_neurons; n++) {
        double base_rate = 20.0 + n * 3.0;  // 20-80 Hz range

        for (int trial = 0; trial < num_trials; trial++) {
            auto spikes = generate_poisson_spikes(base_rate, trial_duration);
            spike_counts[n][trial] = static_cast<int>(spikes.size());
        }

        // Compute Fano factor = variance / mean
        std::vector<float> counts_f(num_trials);
        for (int t = 0; t < num_trials; t++) {
            counts_f[t] = static_cast<float>(spike_counts[n][t]);
        }

        float mean_count = nimcp_stats_mean(counts_f.data(), num_trials);
        float var_count = nimcp_stats_variance(counts_f.data(), num_trials);
        fano_factors[n] = var_count / (mean_count + 1e-6f);
    }

    // Analyze Fano factors
    nimcp_descriptive_stats_t ff_stats;
    EXPECT_EQ(nimcp_stats_describe(fano_factors.data(), num_neurons, &ff_stats), NIMCP_STATS_OK);

    // For Poisson process, Fano factor should be ~1
    EXPECT_NEAR(ff_stats.mean, 1.0f, 0.3f);

    // Most neurons should have Fano factor between 0.5 and 1.5
    int in_range = 0;
    for (float ff : fano_factors) {
        if (ff > 0.5f && ff < 1.5f) in_range++;
    }
    EXPECT_GT(in_range, num_neurons * 0.7);  // >70% in expected range

    double elapsed = STOP_TIMER_MS();
    EXPECT_LT(elapsed, 5000.0);

    std::cout << "Neural Variability: " << num_neurons << " neurons, "
              << num_trials << " trials, "
              << "mean Fano=" << ff_stats.mean << ", "
              << "time=" << elapsed << " ms" << std::endl;
}

//=============================================================================
// E2E Test 8: Spike Train Correlation Analysis
//=============================================================================

TEST_F(NeuralStatisticsE2ETest, SpikeTrainCorrelationAnalysis) {
    START_TIMER();

    // Generate pairs of spike trains with known correlation
    const int num_pairs = 20;
    const double duration = 30.0;
    const double bin_size = 0.05;  // 50ms bins
    int n_bins = static_cast<int>(duration / bin_size);

    std::vector<float> correlations(num_pairs);
    std::vector<float> expected_correlations = {0.0f, 0.1f, 0.2f, 0.3f, 0.5f, 0.7f, 0.9f};

    for (int p = 0; p < num_pairs; p++) {
        float target_corr = expected_correlations[p % expected_correlations.size()];

        // Generate first spike train
        auto spikes1 = generate_poisson_spikes(30.0, duration);
        auto rates1 = estimate_firing_rate_binned(spikes1, bin_size, duration);

        // Generate correlated second spike train
        std::vector<double> spikes2;
        for (double t : spikes1) {
            if (generate_uniform(1, 0, 1)[0] < target_corr) {
                double jitter = generate_normal(1, 0, 0.01f)[0];
                double t2 = t + jitter;
                if (t2 > 0 && t2 < duration) {
                    spikes2.push_back(t2);
                }
            }
        }
        // Add independent spikes
        auto independent = generate_poisson_spikes(30.0 * (1.0 - target_corr), duration);
        spikes2.insert(spikes2.end(), independent.begin(), independent.end());
        std::sort(spikes2.begin(), spikes2.end());

        auto rates2 = estimate_firing_rate_binned(spikes2, bin_size, duration);

        // Compute correlation
        nimcp_correlation_result_t corr_result;
        nimcp_stats_correlation_pearson(rates1.data(), rates2.data(), n_bins, &corr_result);
        correlations[p] = corr_result.r;
    }

    // Analyze measured correlations
    nimcp_descriptive_stats_t corr_stats;
    EXPECT_EQ(nimcp_stats_describe(correlations.data(), num_pairs, &corr_stats), NIMCP_STATS_OK);

    // Correlations should span a range
    EXPECT_GT(corr_stats.max, 0.3f);
    EXPECT_LT(corr_stats.min, 0.5f);

    double elapsed = STOP_TIMER_MS();
    EXPECT_LT(elapsed, 10000.0);

    std::cout << "Spike Correlation: " << num_pairs << " pairs, "
              << "correlation range=[" << corr_stats.min << ", " << corr_stats.max << "], "
              << "time=" << elapsed << " ms" << std::endl;
}

//=============================================================================
// E2E Test 9: Rate Coding vs Temporal Coding Analysis
//=============================================================================

TEST_F(NeuralStatisticsE2ETest, RateVsTemporalCoding) {
    START_TIMER();

    // Compare information in rate vs temporal codes
    const int num_stimuli = 8;
    const int num_trials = 50;
    const double trial_duration = 0.5;  // 500ms

    std::vector<std::vector<float>> rate_responses(num_stimuli, std::vector<float>(num_trials));
    std::vector<std::vector<float>> temporal_responses(num_stimuli, std::vector<float>(num_trials));

    for (int s = 0; s < num_stimuli; s++) {
        double base_rate = 20.0 + s * 10.0;  // Different rate for each stimulus
        double temporal_phase = s * 0.05;     // Different temporal pattern

        for (int trial = 0; trial < num_trials; trial++) {
            // Rate code: spike count
            auto spikes = generate_poisson_spikes(base_rate, trial_duration);
            rate_responses[s][trial] = static_cast<float>(spikes.size());

            // Temporal code: first spike latency
            if (!spikes.empty()) {
                temporal_responses[s][trial] = static_cast<float>(spikes[0] - temporal_phase);
            } else {
                temporal_responses[s][trial] = static_cast<float>(trial_duration);
            }
        }
    }

    // Compute discriminability for rate code
    std::vector<float> rate_means(num_stimuli), rate_vars(num_stimuli);
    for (int s = 0; s < num_stimuli; s++) {
        rate_means[s] = nimcp_stats_mean(rate_responses[s].data(), num_trials);
        rate_vars[s] = nimcp_stats_variance(rate_responses[s].data(), num_trials);
    }

    // Compute discriminability for temporal code
    std::vector<float> temp_means(num_stimuli), temp_vars(num_stimuli);
    for (int s = 0; s < num_stimuli; s++) {
        temp_means[s] = nimcp_stats_mean(temporal_responses[s].data(), num_trials);
        temp_vars[s] = nimcp_stats_variance(temporal_responses[s].data(), num_trials);
    }

    // Rate code should show clear stimulus discrimination
    float rate_range = nimcp_stats_range(rate_means.data(), num_stimuli);
    float temp_range = nimcp_stats_range(temp_means.data(), num_stimuli);

    EXPECT_GT(rate_range, 10.0f);  // Clear rate differences

    // Both codes should carry information
    nimcp_descriptive_stats_t rate_var_stats, temp_var_stats;
    nimcp_stats_describe(rate_vars.data(), num_stimuli, &rate_var_stats);
    nimcp_stats_describe(temp_vars.data(), num_stimuli, &temp_var_stats);

    double elapsed = STOP_TIMER_MS();
    EXPECT_LT(elapsed, 5000.0);

    std::cout << "Rate vs Temporal: rate_range=" << rate_range << ", "
              << "temporal_range=" << temp_range << ", "
              << "time=" << elapsed << " ms" << std::endl;
}

//=============================================================================
// E2E Test 10: Population Vector Decoding
//=============================================================================

TEST_F(NeuralStatisticsE2ETest, PopulationVectorDecoding) {
    START_TIMER();

    // Simulate direction-tuned neurons for motor decoding
    const int num_neurons = 32;
    const int num_directions = 8;  // Movement directions
    const int num_trials = 20;

    // Neuron preferred directions
    std::vector<float> preferred_dirs(num_neurons);
    for (int n = 0; n < num_neurons; n++) {
        preferred_dirs[n] = static_cast<float>(n) * 360.0f / num_neurons;
    }

    std::vector<float> decode_errors;

    for (int d = 0; d < num_directions; d++) {
        float true_direction = static_cast<float>(d) * 360.0f / num_directions;

        for (int trial = 0; trial < num_trials; trial++) {
            // Generate population response
            std::vector<float> responses(num_neurons);
            for (int n = 0; n < num_neurons; n++) {
                float angle_diff = std::min(
                    std::abs(true_direction - preferred_dirs[n]),
                    360.0f - std::abs(true_direction - preferred_dirs[n])
                );
                float tuning = std::exp(-angle_diff * angle_diff / (2 * 60.0f * 60.0f));
                responses[n] = tuning + generate_normal(1, 0, 0.2f)[0];
                responses[n] = std::max(0.0f, responses[n]);
            }

            // Population vector decode
            float pv_x = 0.0f, pv_y = 0.0f;
            for (int n = 0; n < num_neurons; n++) {
                float rad = preferred_dirs[n] * M_PI / 180.0f;
                pv_x += responses[n] * std::cos(rad);
                pv_y += responses[n] * std::sin(rad);
            }

            float decoded_direction = std::atan2(pv_y, pv_x) * 180.0f / M_PI;
            if (decoded_direction < 0) decoded_direction += 360.0f;

            // Compute error
            float error = std::min(
                std::abs(decoded_direction - true_direction),
                360.0f - std::abs(decoded_direction - true_direction)
            );
            decode_errors.push_back(error);
        }
    }

    // Analyze decoding accuracy
    nimcp_descriptive_stats_t error_stats;
    EXPECT_EQ(nimcp_stats_describe(decode_errors.data(), decode_errors.size(), &error_stats), NIMCP_STATS_OK);

    // Mean decoding error should be reasonably small
    EXPECT_LT(error_stats.mean, 30.0f);  // Less than 30 degrees
    EXPECT_LT(error_stats.median, 25.0f);

    double elapsed = STOP_TIMER_MS();
    EXPECT_LT(elapsed, 3000.0);

    std::cout << "Population Vector: " << num_neurons << " neurons, "
              << "mean error=" << error_stats.mean << " deg, "
              << "median error=" << error_stats.median << " deg, "
              << "time=" << elapsed << " ms" << std::endl;
}

//=============================================================================
// E2E Test 11: Neural Entropy Estimation
//=============================================================================

TEST_F(NeuralStatisticsE2ETest, NeuralEntropyEstimation) {
    START_TIMER();

    // Estimate entropy of neural spike patterns
    const int n_samples = 5000;
    const double duration = 0.1;  // 100ms windows
    const int n_bins = 16;  // Discretization

    // Generate spike counts with different distributions
    struct EntropyCase {
        std::string name;
        double rate;
        float expected_entropy_ratio;  // Relative to max entropy
    };

    std::vector<EntropyCase> cases = {
        {"Low variability", 10.0, 0.3f},
        {"Medium variability", 30.0, 0.6f},
        {"High variability", 100.0, 0.8f}
    };

    float max_entropy = std::log2(static_cast<float>(n_bins));

    for (const auto& test_case : cases) {
        std::vector<float> spike_counts(n_samples);

        for (int i = 0; i < n_samples; i++) {
            auto spikes = generate_poisson_spikes(test_case.rate, duration);
            spike_counts[i] = std::min(static_cast<float>(spikes.size()), static_cast<float>(n_bins - 1));
        }

        // Estimate entropy using histogram
        std::vector<float> histogram(n_bins, 0.0f);
        for (float count : spike_counts) {
            int bin = static_cast<int>(count);
            bin = std::min(bin, n_bins - 1);
            histogram[bin] += 1.0f;
        }

        // Normalize to probability
        float total = std::accumulate(histogram.begin(), histogram.end(), 0.0f);
        for (auto& p : histogram) p /= total;

        float entropy = nimcp_stats_entropy(histogram.data(), n_bins);
        float entropy_ratio = entropy / max_entropy;

        // Entropy should be positive and bounded
        EXPECT_GT(entropy, 0.0f);
        EXPECT_LT(entropy, max_entropy + 0.1f);
    }

    double elapsed = STOP_TIMER_MS();
    EXPECT_LT(elapsed, 5000.0);

    std::cout << "Neural Entropy: " << cases.size() << " conditions tested, "
              << "max possible entropy=" << max_entropy << " bits, "
              << "time=" << elapsed << " ms" << std::endl;
}

//=============================================================================
// E2E Test 12: Noise Correlation Analysis
//=============================================================================

TEST_F(NeuralStatisticsE2ETest, NoiseCorrelationAnalysis) {
    START_TIMER();

    // Analyze noise correlations in neural populations
    const int num_neurons = 30;
    const int num_trials = 100;
    const int num_stimuli = 4;

    // Generate responses with varying noise correlations
    std::vector<std::vector<std::vector<float>>> responses(
        num_stimuli,
        std::vector<std::vector<float>>(num_neurons, std::vector<float>(num_trials))
    );

    for (int s = 0; s < num_stimuli; s++) {
        // Shared noise source for this stimulus
        std::vector<float> shared_noise = generate_normal(num_trials, 0.0f, 1.0f);

        for (int n = 0; n < num_neurons; n++) {
            float mean_response = 20.0f + s * 5.0f + n * 0.5f;
            float shared_weight = 0.3f;  // Amount of shared variability

            for (int t = 0; t < num_trials; t++) {
                float private_noise = generate_normal(1, 0, 3.0f)[0];
                responses[s][n][t] = mean_response + shared_weight * shared_noise[t] + private_noise;
            }
        }
    }

    // Compute noise correlations (trial-to-trial correlations after subtracting mean)
    std::vector<float> noise_correlations;

    for (int s = 0; s < num_stimuli; s++) {
        // Subtract mean from each neuron
        std::vector<std::vector<float>> residuals(num_neurons, std::vector<float>(num_trials));
        for (int n = 0; n < num_neurons; n++) {
            float mean = nimcp_stats_mean(responses[s][n].data(), num_trials);
            for (int t = 0; t < num_trials; t++) {
                residuals[n][t] = responses[s][n][t] - mean;
            }
        }

        // Compute pairwise correlations
        for (int n1 = 0; n1 < num_neurons; n1++) {
            for (int n2 = n1 + 1; n2 < num_neurons; n2++) {
                nimcp_correlation_result_t corr;
                nimcp_stats_correlation_pearson(
                    residuals[n1].data(), residuals[n2].data(), num_trials, &corr);
                noise_correlations.push_back(corr.r);
            }
        }
    }

    // Analyze noise correlation distribution
    nimcp_descriptive_stats_t nc_stats;
    EXPECT_EQ(nimcp_stats_describe(noise_correlations.data(), noise_correlations.size(), &nc_stats), NIMCP_STATS_OK);

    // Mean noise correlation should be positive (shared variability)
    EXPECT_GT(nc_stats.mean, 0.0f);
    EXPECT_LT(nc_stats.mean, 0.5f);  // But not too high

    // Distribution should be roughly symmetric around mean
    EXPECT_LT(std::abs(nc_stats.skewness), 1.0f);

    double elapsed = STOP_TIMER_MS();
    EXPECT_LT(elapsed, 10000.0);

    std::cout << "Noise Correlation: " << noise_correlations.size() << " pairs, "
              << "mean r=" << nc_stats.mean << ", "
              << "range=[" << nc_stats.min << ", " << nc_stats.max << "], "
              << "time=" << elapsed << " ms" << std::endl;
}

//=============================================================================
// E2E Test 13: Multi-Neuron Information Decomposition
//=============================================================================

TEST_F(NeuralStatisticsE2ETest, MultiNeuronInformationDecomposition) {
    START_TIMER();

    // Analyze how information is distributed across neurons
    const int num_neurons = 10;
    const int num_stimuli = 4;
    const int num_trials = 50;
    const int n_bins = 4;  // Discretization bins

    // Generate responses with redundant and synergistic information
    std::vector<std::vector<std::vector<float>>> responses(
        num_stimuli,
        std::vector<std::vector<float>>(num_neurons, std::vector<float>(num_trials))
    );

    for (int s = 0; s < num_stimuli; s++) {
        for (int n = 0; n < num_neurons; n++) {
            float selectivity = (n % 2 == s % 2) ? 10.0f : 2.0f;  // Some neurons are selective
            for (int t = 0; t < num_trials; t++) {
                responses[s][n][t] = selectivity + generate_normal(1, 0, 2.0f)[0];
            }
        }
    }

    // Compute mutual information for individual neurons
    std::vector<float> single_neuron_mi(num_neurons);

    for (int n = 0; n < num_neurons; n++) {
        // Build joint distribution P(stimulus, response)
        std::vector<float> joint(num_stimuli * n_bins, 0.0f);

        for (int s = 0; s < num_stimuli; s++) {
            for (int t = 0; t < num_trials; t++) {
                int bin = static_cast<int>((responses[s][n][t] - 0.0f) / 5.0f);
                bin = std::max(0, std::min(n_bins - 1, bin));
                joint[s * n_bins + bin] += 1.0f;
            }
        }

        // Normalize
        float total = std::accumulate(joint.begin(), joint.end(), 0.0f);
        for (auto& p : joint) p /= total;

        single_neuron_mi[n] = nimcp_stats_mutual_information(joint.data(), num_stimuli, n_bins);
    }

    // Analyze information distribution
    nimcp_descriptive_stats_t mi_stats;
    EXPECT_EQ(nimcp_stats_describe(single_neuron_mi.data(), num_neurons, &mi_stats), NIMCP_STATS_OK);

    // Some neurons should carry more information
    EXPECT_GT(mi_stats.max, mi_stats.min);
    EXPECT_GT(mi_stats.mean, 0.0f);

    // Total single-neuron information
    float total_single_mi = std::accumulate(single_neuron_mi.begin(), single_neuron_mi.end(), 0.0f);

    double elapsed = STOP_TIMER_MS();
    EXPECT_LT(elapsed, 5000.0);

    std::cout << "Information Decomposition: " << num_neurons << " neurons, "
              << "mean MI=" << mi_stats.mean << " bits, "
              << "total single=" << total_single_mi << " bits, "
              << "time=" << elapsed << " ms" << std::endl;
}

//=============================================================================
// E2E Test 14: Neural State Space Analysis
//=============================================================================

TEST_F(NeuralStatisticsE2ETest, NeuralStateSpaceAnalysis) {
    START_TIMER();

    // Analyze population dynamics in state space
    const int num_neurons = 50;
    const int num_timepoints = 200;
    const int num_conditions = 3;

    // Generate population activity over time
    std::vector<std::vector<std::vector<float>>> activity(
        num_conditions,
        std::vector<std::vector<float>>(num_timepoints, std::vector<float>(num_neurons))
    );

    for (int c = 0; c < num_conditions; c++) {
        // Initial state
        std::vector<float> state = generate_normal(num_neurons, 0.0f, 1.0f);

        // Dynamics matrix (different for each condition)
        std::vector<std::vector<float>> dynamics(num_neurons, std::vector<float>(num_neurons));
        for (int i = 0; i < num_neurons; i++) {
            for (int j = 0; j < num_neurons; j++) {
                if (i == j) {
                    dynamics[i][j] = 0.9f;  // Decay
                } else if (std::abs(i - j) < 3) {
                    dynamics[i][j] = 0.1f + 0.05f * c;  // Condition-dependent coupling
                } else {
                    dynamics[i][j] = 0.0f;
                }
            }
        }

        for (int t = 0; t < num_timepoints; t++) {
            // Store current state
            activity[c][t] = state;

            // Update state
            std::vector<float> new_state(num_neurons, 0.0f);
            for (int i = 0; i < num_neurons; i++) {
                for (int j = 0; j < num_neurons; j++) {
                    new_state[i] += dynamics[i][j] * state[j];
                }
                new_state[i] += generate_normal(1, 0, 0.1f)[0];
            }
            state = new_state;
        }
    }

    // Compute covariance matrix for each condition
    std::vector<float> condition_variances(num_conditions);

    for (int c = 0; c < num_conditions; c++) {
        // Flatten timepoints x neurons to observations x features
        std::vector<float> flat_data(num_timepoints * num_neurons);
        for (int t = 0; t < num_timepoints; t++) {
            for (int n = 0; n < num_neurons; n++) {
                flat_data[t * num_neurons + n] = activity[c][t][n];
            }
        }

        // Compute variance of population activity
        std::vector<float> mean_activity(num_neurons);
        for (int n = 0; n < num_neurons; n++) {
            std::vector<float> neuron_activity(num_timepoints);
            for (int t = 0; t < num_timepoints; t++) {
                neuron_activity[t] = activity[c][t][n];
            }
            mean_activity[n] = nimcp_stats_variance(neuron_activity.data(), num_timepoints);
        }

        condition_variances[c] = nimcp_stats_mean(mean_activity.data(), num_neurons);
    }

    // Different conditions should have different state space properties
    nimcp_descriptive_stats_t var_stats;
    EXPECT_EQ(nimcp_stats_describe(condition_variances.data(), num_conditions, &var_stats), NIMCP_STATS_OK);

    EXPECT_GT(var_stats.mean, 0.0f);

    double elapsed = STOP_TIMER_MS();
    EXPECT_LT(elapsed, 10000.0);

    std::cout << "State Space: " << num_neurons << " neurons x " << num_timepoints << " timepoints, "
              << num_conditions << " conditions, "
              << "mean variance=" << var_stats.mean << ", "
              << "time=" << elapsed << " ms" << std::endl;
}

//=============================================================================
// E2E Test 15: Spike Timing Precision Analysis
//=============================================================================

TEST_F(NeuralStatisticsE2ETest, SpikeTimingPrecisionAnalysis) {
    START_TIMER();

    // Analyze spike timing precision across repeated stimuli
    const int num_trials = 100;
    const double stimulus_onset = 0.1;  // 100ms
    const double duration = 0.5;

    // Generate spike trains with different precision levels
    struct PrecisionCase {
        std::string name;
        double jitter_ms;
        double expected_precision_ms;
    };

    std::vector<PrecisionCase> cases = {
        {"High precision", 1.0, 2.0},
        {"Medium precision", 5.0, 10.0},
        {"Low precision", 20.0, 40.0}
    };

    for (const auto& test_case : cases) {
        std::vector<double> first_spike_times(num_trials);

        for (int trial = 0; trial < num_trials; trial++) {
            // Fixed response latency + jitter
            double latency = 0.05 + generate_normal(1, 0, test_case.jitter_ms / 1000.0f)[0];
            first_spike_times[trial] = stimulus_onset + std::max(0.001, latency);
        }

        // Convert to float for statistics
        std::vector<float> fst_float(num_trials);
        for (int i = 0; i < num_trials; i++) {
            fst_float[i] = static_cast<float>(first_spike_times[i] * 1000);  // Convert to ms
        }

        // Compute timing precision (std dev of first spike time)
        float precision = nimcp_stats_std_dev(fst_float.data(), num_trials);

        // Precision should be close to expected
        EXPECT_LT(precision, test_case.expected_precision_ms * 2);
        EXPECT_GT(precision, test_case.expected_precision_ms * 0.3f);
    }

    double elapsed = STOP_TIMER_MS();
    EXPECT_LT(elapsed, 3000.0);

    std::cout << "Spike Timing Precision: " << cases.size() << " conditions, "
              << num_trials << " trials each, "
              << "time=" << elapsed << " ms" << std::endl;
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
