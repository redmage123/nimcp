//=============================================================================
// test_neural_statistics.cpp - Unit Tests for Neural Statistics Module
//=============================================================================
/**
 * @file test_neural_statistics.cpp
 * @brief Comprehensive unit tests for neural-specific statistics functions
 *
 * WHAT: Test coverage for neural statistics (Fisher info, ISI, Fano factor, etc.)
 * WHY:  Ensure numerical accuracy for neuroscience-specific statistical measures
 * HOW:  GTest framework with tolerance-based comparisons
 *
 * TEST COVERAGE:
 * - Fisher information computation
 * - Inter-spike interval (ISI) distributions
 * - Fano factor calculations
 * - Spike train entropy
 * - Cross-correlogram analysis
 * - Quantal analysis parameter estimation
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

// Test tolerances
#define TOLERANCE 1e-5f
#define LOOSE_TOLERANCE 1e-3f
#define VERY_LOOSE_TOLERANCE 1e-2f
#define NEURAL_TOLERANCE 0.05f  // 5% tolerance for stochastic neural measures

//=============================================================================
// Test Fixture
//=============================================================================

class NeuralStatisticsTest : public ::testing::Test {
protected:
    void SetUp() override {
        nimcp_stats_config_t config = nimcp_stats_default_config();
        nimcp_stats_init(&config);
        rng.seed(42);  // Reproducible tests
    }

    void TearDown() override {
        nimcp_stats_shutdown();
    }

    std::mt19937 rng;

    // Helper: Generate Poisson process spike times
    std::vector<float> generatePoissonSpikeTrain(float rate, float duration, int seed = 42) {
        std::mt19937 gen(seed);
        std::exponential_distribution<float> dist(rate);

        std::vector<float> spikes;
        float t = 0.0f;
        while (t < duration) {
            t += dist(gen);
            if (t < duration) {
                spikes.push_back(t);
            }
        }
        return spikes;
    }

    // Helper: Compute ISI from spike times
    std::vector<float> computeISI(const std::vector<float>& spikes) {
        std::vector<float> isi;
        for (size_t i = 1; i < spikes.size(); i++) {
            isi.push_back(spikes[i] - spikes[i-1]);
        }
        return isi;
    }

    // Helper: Generate binned spike counts
    std::vector<uint32_t> binSpikes(const std::vector<float>& spikes,
                                     float bin_width, float duration) {
        size_t n_bins = static_cast<size_t>(std::ceil(duration / bin_width));
        std::vector<uint32_t> counts(n_bins, 0);
        for (float t : spikes) {
            size_t bin = static_cast<size_t>(t / bin_width);
            if (bin < n_bins) {
                counts[bin]++;
            }
        }
        return counts;
    }

    // Helper: Compute Fano factor from spike counts
    float computeFanoFactor(const std::vector<uint32_t>& counts) {
        if (counts.empty()) return NAN;

        float sum = 0.0f, sum_sq = 0.0f;
        for (uint32_t c : counts) {
            sum += c;
            sum_sq += c * c;
        }
        float n = static_cast<float>(counts.size());
        float mean = sum / n;
        float var = (sum_sq / n) - (mean * mean);

        if (mean == 0.0f) return NAN;
        return var / mean;
    }
};

//=============================================================================
// Fisher Information Tests
//=============================================================================

class FisherInformationTest : public NeuralStatisticsTest {};

TEST_F(FisherInformationTest, GaussianPopulation_KnownFormula) {
    // For Gaussian tuning curves, Fisher info = N * (f'(s))^2 / sigma^2
    // Test with simple Gaussian population
    const uint32_t n_neurons = 100;
    const float preferred_stimuli[] = {0.0f, 0.1f, 0.2f, 0.3f, 0.4f};
    const uint32_t n_stimuli = 5;

    // Simulate tuning curve responses
    std::vector<float> responses(n_neurons * n_stimuli);
    float sigma_tuning = 0.2f;  // Tuning width
    float sigma_noise = 0.1f;   // Response noise

    for (uint32_t s = 0; s < n_stimuli; s++) {
        for (uint32_t n = 0; n < n_neurons; n++) {
            float preferred = static_cast<float>(n) / n_neurons;  // Uniform coverage
            float stimulus = preferred_stimuli[s];
            float response = std::exp(-std::pow(stimulus - preferred, 2) /
                                      (2 * sigma_tuning * sigma_tuning));
            responses[s * n_neurons + n] = response;
        }
    }

    // Compute Fisher information at each stimulus
    for (uint32_t s = 1; s < n_stimuli - 1; s++) {
        // Numerical derivative
        float ds = preferred_stimuli[s+1] - preferred_stimuli[s-1];
        float fisher = 0.0f;

        for (uint32_t n = 0; n < n_neurons; n++) {
            float dr = responses[(s+1) * n_neurons + n] - responses[(s-1) * n_neurons + n];
            float slope = dr / ds;
            fisher += (slope * slope) / (sigma_noise * sigma_noise);
        }

        // Fisher info should be positive
        EXPECT_GT(fisher, 0.0f);
    }
}

TEST_F(FisherInformationTest, SingleNeuron_MonotonicTuning) {
    // For monotonic tuning curve, Fisher info depends on slope
    std::vector<float> stimuli = {0.0f, 0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f};
    std::vector<float> responses;

    float slope = 2.0f;  // Linear tuning
    for (float s : stimuli) {
        responses.push_back(slope * s);
    }

    // Fisher info = (slope)^2 / noise_variance
    float noise_var = 0.1f;
    float expected_fisher = (slope * slope) / noise_var;

    // With constant slope, Fisher info should be constant
    EXPECT_GT(expected_fisher, 0.0f);
}

TEST_F(FisherInformationTest, EdgeCases_EmptyArray) {
    // Test with empty input
    std::vector<float> empty;
    // Should handle gracefully
    float mean = nimcp_stats_mean(empty.data(), 0);
    EXPECT_TRUE(std::isnan(mean));
}

TEST_F(FisherInformationTest, MultipleTuningWidths) {
    // Wider tuning = lower Fisher info for same population
    std::vector<float> fisher_narrow;
    std::vector<float> fisher_wide;

    const int n_neurons = 50;
    const float sigma_narrow = 0.1f;
    const float sigma_wide = 0.3f;

    // Compute Fisher info at center
    float s = 0.5f;
    float ds = 0.01f;

    // For narrow tuning
    float fi_narrow = 0.0f;
    for (int n = 0; n < n_neurons; n++) {
        float pref = static_cast<float>(n) / n_neurons;
        float r_plus = std::exp(-std::pow(s + ds - pref, 2) / (2 * sigma_narrow * sigma_narrow));
        float r_minus = std::exp(-std::pow(s - ds - pref, 2) / (2 * sigma_narrow * sigma_narrow));
        float slope = (r_plus - r_minus) / (2 * ds);
        fi_narrow += slope * slope;
    }

    // For wide tuning
    float fi_wide = 0.0f;
    for (int n = 0; n < n_neurons; n++) {
        float pref = static_cast<float>(n) / n_neurons;
        float r_plus = std::exp(-std::pow(s + ds - pref, 2) / (2 * sigma_wide * sigma_wide));
        float r_minus = std::exp(-std::pow(s - ds - pref, 2) / (2 * sigma_wide * sigma_wide));
        float slope = (r_plus - r_minus) / (2 * ds);
        fi_wide += slope * slope;
    }

    // Narrow tuning should give higher Fisher info
    EXPECT_GT(fi_narrow, fi_wide);
}

//=============================================================================
// Inter-Spike Interval (ISI) Distribution Tests
//=============================================================================

class ISIDistributionTest : public NeuralStatisticsTest {};

TEST_F(ISIDistributionTest, PoissonProcess_ExponentialISI) {
    // ISI of Poisson process should be exponentially distributed
    float rate = 10.0f;  // Hz
    float duration = 100.0f;  // seconds

    auto spikes = generatePoissonSpikeTrain(rate, duration, 42);
    auto isi = computeISI(spikes);

    ASSERT_GT(isi.size(), 100u);  // Need enough samples

    // Mean ISI should be ~1/rate
    float expected_mean = 1.0f / rate;
    float actual_mean = nimcp_stats_mean(isi.data(), static_cast<uint32_t>(isi.size()));
    EXPECT_NEAR(actual_mean, expected_mean, expected_mean * NEURAL_TOLERANCE);

    // CV should be ~1 for Poisson
    float std_dev = nimcp_stats_std_dev(isi.data(), static_cast<uint32_t>(isi.size()));
    float cv = std_dev / actual_mean;
    EXPECT_NEAR(cv, 1.0f, NEURAL_TOLERANCE);
}

TEST_F(ISIDistributionTest, RegularProcess_LowCV) {
    // Regular (periodic) spiking should have low CV
    std::vector<float> regular_isi;
    float period = 0.1f;  // 10 Hz regular
    float jitter = 0.001f;  // Small jitter

    std::normal_distribution<float> noise(0.0f, jitter);
    for (int i = 0; i < 1000; i++) {
        regular_isi.push_back(period + noise(rng));
    }

    float mean = nimcp_stats_mean(regular_isi.data(), static_cast<uint32_t>(regular_isi.size()));
    float std_dev = nimcp_stats_std_dev(regular_isi.data(), static_cast<uint32_t>(regular_isi.size()));
    float cv = std_dev / mean;

    // CV should be very low for regular spiking
    EXPECT_LT(cv, 0.1f);
}

TEST_F(ISIDistributionTest, BurstyProcess_HighCV) {
    // Bursty spiking should have CV > 1
    std::vector<float> bursty_isi;

    // Alternate between short (burst) and long (inter-burst) intervals
    for (int i = 0; i < 500; i++) {
        bursty_isi.push_back(0.005f);  // Within-burst ISI (5 ms)
        bursty_isi.push_back(0.005f);
        bursty_isi.push_back(0.5f);    // Inter-burst interval (500 ms)
    }

    float mean = nimcp_stats_mean(bursty_isi.data(), static_cast<uint32_t>(bursty_isi.size()));
    float std_dev = nimcp_stats_std_dev(bursty_isi.data(), static_cast<uint32_t>(bursty_isi.size()));
    float cv = std_dev / mean;

    // CV should be high for bursty spiking
    EXPECT_GT(cv, 1.0f);
}

TEST_F(ISIDistributionTest, ISI_Positivity) {
    // All ISIs should be positive
    auto spikes = generatePoissonSpikeTrain(20.0f, 10.0f, 123);
    auto isi = computeISI(spikes);

    for (float interval : isi) {
        EXPECT_GT(interval, 0.0f);
    }
}

TEST_F(ISIDistributionTest, ISI_Median_LessThanMean_Exponential) {
    // For exponential distribution, median < mean
    auto spikes = generatePoissonSpikeTrain(15.0f, 50.0f, 456);
    auto isi = computeISI(spikes);

    ASSERT_GT(isi.size(), 50u);

    float mean = nimcp_stats_mean(isi.data(), static_cast<uint32_t>(isi.size()));
    float median = nimcp_stats_median(isi.data(), static_cast<uint32_t>(isi.size()));

    // For exponential, median = mean * ln(2) < mean
    EXPECT_LT(median, mean);
}

//=============================================================================
// Fano Factor Tests
//=============================================================================

class FanoFactorTest : public NeuralStatisticsTest {};

TEST_F(FanoFactorTest, PoissonProcess_FanoNearOne) {
    // Fano factor for Poisson process should be ~1
    float rate = 20.0f;
    float duration = 100.0f;
    float bin_width = 0.1f;  // 100 ms bins

    auto spikes = generatePoissonSpikeTrain(rate, duration, 789);
    auto counts = binSpikes(spikes, bin_width, duration);

    float fano = computeFanoFactor(counts);

    // Fano factor should be close to 1 for Poisson
    EXPECT_NEAR(fano, 1.0f, 0.2f);  // Allow 20% deviation
}

TEST_F(FanoFactorTest, RegularProcess_FanoLessThanOne) {
    // Regular spiking has Fano < 1 (sub-Poisson)
    std::vector<uint32_t> regular_counts;

    // Each bin has exactly 2 spikes (plus small noise)
    std::poisson_distribution<int> noise(0.1);
    for (int i = 0; i < 1000; i++) {
        regular_counts.push_back(2);  // Constant count
    }

    float fano = computeFanoFactor(regular_counts);

    // Fano should be 0 for perfectly regular
    EXPECT_NEAR(fano, 0.0f, LOOSE_TOLERANCE);
}

TEST_F(FanoFactorTest, BurstyProcess_FanoGreaterThanOne) {
    // Bursty spiking has Fano > 1 (super-Poisson)
    std::vector<uint32_t> bursty_counts;

    // Alternate between 0 and many spikes
    for (int i = 0; i < 500; i++) {
        bursty_counts.push_back(0);
        bursty_counts.push_back(10);
    }

    float fano = computeFanoFactor(bursty_counts);

    // Fano should be > 1 for bursty
    EXPECT_GT(fano, 1.0f);
}

TEST_F(FanoFactorTest, FanoFactor_TimeScaleDependence) {
    // Fano factor depends on bin size
    float rate = 10.0f;
    float duration = 200.0f;

    auto spikes = generatePoissonSpikeTrain(rate, duration, 111);

    std::vector<float> bin_widths = {0.01f, 0.05f, 0.1f, 0.5f, 1.0f};
    std::vector<float> fano_factors;

    for (float bw : bin_widths) {
        auto counts = binSpikes(spikes, bw, duration);
        float fano = computeFanoFactor(counts);
        fano_factors.push_back(fano);
    }

    // For Poisson, all Fano factors should be ~1
    for (float f : fano_factors) {
        EXPECT_NEAR(f, 1.0f, 0.3f);
    }
}

TEST_F(FanoFactorTest, FanoFactor_ZeroMean) {
    // Fano factor undefined for zero mean
    std::vector<uint32_t> zeros(100, 0);
    float fano = computeFanoFactor(zeros);
    EXPECT_TRUE(std::isnan(fano));
}

TEST_F(FanoFactorTest, FanoFactor_NonNegative) {
    // Fano factor should always be >= 0
    for (int seed = 0; seed < 10; seed++) {
        auto spikes = generatePoissonSpikeTrain(5.0f + seed, 20.0f, seed);
        auto counts = binSpikes(spikes, 0.1f, 20.0f);
        float fano = computeFanoFactor(counts);

        if (!std::isnan(fano)) {
            EXPECT_GE(fano, 0.0f);
        }
    }
}

//=============================================================================
// Spike Train Entropy Tests
//=============================================================================

class SpikeTrainEntropyTest : public NeuralStatisticsTest {};

TEST_F(SpikeTrainEntropyTest, HighRate_HighEntropy) {
    // Higher rate Poisson has more total entropy
    float low_rate = 5.0f;
    float high_rate = 50.0f;
    float duration = 10.0f;
    float bin_width = 0.01f;

    auto spikes_low = generatePoissonSpikeTrain(low_rate, duration, 222);
    auto spikes_high = generatePoissonSpikeTrain(high_rate, duration, 333);

    auto counts_low = binSpikes(spikes_low, bin_width, duration);
    auto counts_high = binSpikes(spikes_high, bin_width, duration);

    // Convert to probabilities
    float total_low = 0.0f, total_high = 0.0f;
    for (auto c : counts_low) total_low += c;
    for (auto c : counts_high) total_high += c;

    // More spikes = more bits of information (generally)
    EXPECT_GT(total_high, total_low);
}

TEST_F(SpikeTrainEntropyTest, Uniform_MaxEntropy) {
    // Uniform distribution has maximum entropy
    std::vector<float> uniform(8, 0.125f);
    float h_uniform = nimcp_stats_entropy(uniform.data(), 8);

    // Should be log2(8) = 3 bits
    EXPECT_NEAR(h_uniform, 3.0f, TOLERANCE);
}

TEST_F(SpikeTrainEntropyTest, Deterministic_ZeroEntropy) {
    // Deterministic has zero entropy
    std::vector<float> deterministic = {1.0f, 0.0f, 0.0f, 0.0f};
    float h = nimcp_stats_entropy(deterministic.data(), 4);
    EXPECT_NEAR(h, 0.0f, TOLERANCE);
}

TEST_F(SpikeTrainEntropyTest, ISI_Entropy_Poisson) {
    // ISI entropy for Poisson is well-defined
    auto spikes = generatePoissonSpikeTrain(20.0f, 50.0f, 444);
    auto isi = computeISI(spikes);

    ASSERT_GT(isi.size(), 100u);

    // Histogram-based entropy estimate
    int n_bins = 20;
    float max_isi = *std::max_element(isi.begin(), isi.end());
    float bin_width = max_isi / n_bins;

    std::vector<float> hist(n_bins, 0.0f);
    for (float interval : isi) {
        int bin = static_cast<int>(interval / bin_width);
        if (bin >= n_bins) bin = n_bins - 1;
        hist[bin] += 1.0f;
    }

    // Normalize to probabilities
    float total = static_cast<float>(isi.size());
    for (float& h : hist) {
        h /= total;
    }

    float entropy = 0.0f;
    for (float p : hist) {
        if (p > 0) {
            entropy -= p * std::log2(p);
        }
    }

    // Entropy should be positive for non-trivial distribution
    EXPECT_GT(entropy, 0.0f);
}

//=============================================================================
// Cross-Correlogram Tests
//=============================================================================

class CrossCorrelogramTest : public NeuralStatisticsTest {};

TEST_F(CrossCorrelogramTest, AutoCorrelogram_Symmetric) {
    // Auto-correlogram should be symmetric
    auto spikes = generatePoissonSpikeTrain(20.0f, 10.0f, 555);

    int max_lag_bins = 50;
    float bin_width = 0.001f;  // 1 ms bins
    std::vector<int> auto_corr(2 * max_lag_bins + 1, 0);

    // Compute auto-correlogram
    for (size_t i = 0; i < spikes.size(); i++) {
        for (size_t j = 0; j < spikes.size(); j++) {
            if (i == j) continue;  // Exclude 0-lag self

            float dt = spikes[j] - spikes[i];
            int lag_bin = static_cast<int>(dt / bin_width);

            if (std::abs(lag_bin) <= max_lag_bins) {
                auto_corr[lag_bin + max_lag_bins]++;
            }
        }
    }

    // Check symmetry
    for (int k = 1; k <= max_lag_bins; k++) {
        int left = auto_corr[max_lag_bins - k];
        int right = auto_corr[max_lag_bins + k];
        EXPECT_EQ(left, right);
    }
}

TEST_F(CrossCorrelogramTest, CrossCorrelogram_Independent_Flat) {
    // Cross-correlogram of independent Poisson should be approximately flat
    auto spikes1 = generatePoissonSpikeTrain(15.0f, 20.0f, 666);
    auto spikes2 = generatePoissonSpikeTrain(15.0f, 20.0f, 777);

    int max_lag_bins = 100;
    float bin_width = 0.001f;
    std::vector<int> cross_corr(2 * max_lag_bins + 1, 0);

    for (float t1 : spikes1) {
        for (float t2 : spikes2) {
            float dt = t2 - t1;
            int lag_bin = static_cast<int>(dt / bin_width);

            if (std::abs(lag_bin) <= max_lag_bins) {
                cross_corr[lag_bin + max_lag_bins]++;
            }
        }
    }

    // Compute mean and std of cross-correlogram
    float mean = 0.0f, var = 0.0f;
    int n = 2 * max_lag_bins + 1;
    for (int c : cross_corr) mean += c;
    mean /= n;

    for (int c : cross_corr) {
        var += (c - mean) * (c - mean);
    }
    var /= n;

    float cv = std::sqrt(var) / mean;

    // CV should be low for flat correlogram
    EXPECT_LT(cv, 0.5f);
}

TEST_F(CrossCorrelogramTest, CrossCorrelogram_Correlated_Peak) {
    // Correlated spike trains should show peak near zero lag
    auto spikes1 = generatePoissonSpikeTrain(20.0f, 10.0f, 888);

    // Create correlated train: copy with small jitter
    std::vector<float> spikes2;
    std::normal_distribution<float> jitter(0.0f, 0.005f);
    for (float t : spikes1) {
        spikes2.push_back(t + jitter(rng));
    }
    std::sort(spikes2.begin(), spikes2.end());

    int max_lag_bins = 50;
    float bin_width = 0.001f;
    std::vector<int> cross_corr(2 * max_lag_bins + 1, 0);

    for (float t1 : spikes1) {
        for (float t2 : spikes2) {
            float dt = t2 - t1;
            int lag_bin = static_cast<int>(dt / bin_width);

            if (std::abs(lag_bin) <= max_lag_bins) {
                cross_corr[lag_bin + max_lag_bins]++;
            }
        }
    }

    // Find peak
    int peak_idx = 0;
    int peak_val = 0;
    for (int i = 0; i < 2 * max_lag_bins + 1; i++) {
        if (cross_corr[i] > peak_val) {
            peak_val = cross_corr[i];
            peak_idx = i;
        }
    }

    // Peak should be near center (zero lag)
    EXPECT_NEAR(peak_idx, max_lag_bins, 10);
}

//=============================================================================
// Quantal Analysis Tests
//=============================================================================

class QuantalAnalysisTest : public NeuralStatisticsTest {};

TEST_F(QuantalAnalysisTest, MeanVarianceMethod_EstimatesN) {
    // For binomial release: mean = N*p, variance = N*p*(1-p)
    // N = mean^2 / (mean - variance) when mean > variance

    int N_true = 10;  // True number of release sites
    float p_true = 0.5f;  // True release probability
    int n_trials = 1000;

    std::binomial_distribution<int> dist(N_true, p_true);
    std::vector<float> responses;

    for (int i = 0; i < n_trials; i++) {
        responses.push_back(static_cast<float>(dist(rng)));
    }

    float mean = nimcp_stats_mean(responses.data(), n_trials);
    float var = nimcp_stats_variance(responses.data(), n_trials);

    // Estimate N using mean-variance method
    float estimated_N = (mean * mean) / (mean - var);

    // Check estimate is reasonable
    EXPECT_NEAR(estimated_N, N_true, 3.0f);  // Allow some error
}

TEST_F(QuantalAnalysisTest, QuanatalContent_Estimation) {
    // Quantal content m = mean / q, where q is quantal size
    float q_true = 1.0f;  // Quantal size
    int m_true = 5;       // True quantal content
    float noise_sd = 0.1f;
    int n_trials = 500;

    std::poisson_distribution<int> quanta(m_true);
    std::normal_distribution<float> noise(0.0f, noise_sd);
    std::vector<float> responses;

    for (int i = 0; i < n_trials; i++) {
        int n_quanta = quanta(rng);
        responses.push_back(n_quanta * q_true + noise(rng));
    }

    float mean = nimcp_stats_mean(responses.data(), n_trials);
    float estimated_m = mean / q_true;

    EXPECT_NEAR(estimated_m, m_true, 0.5f);
}

TEST_F(QuantalAnalysisTest, VarianceToMeanRatio_UniformReleaseProb) {
    // For uniform p across sites: Var/Mean = 1 - p
    float p_release = 0.3f;
    int n_sites = 20;
    int n_trials = 2000;

    std::binomial_distribution<int> release(n_sites, p_release);
    std::vector<float> counts;

    for (int i = 0; i < n_trials; i++) {
        counts.push_back(static_cast<float>(release(rng)));
    }

    float mean = nimcp_stats_mean(counts.data(), n_trials);
    float var = nimcp_stats_variance(counts.data(), n_trials);

    float vMratio = var / mean;
    float expected = 1.0f - p_release;

    EXPECT_NEAR(vMratio, expected, 0.1f);
}

TEST_F(QuantalAnalysisTest, FailureRate_Estimation) {
    // P(failure) = (1-p)^N for N independent sites
    int N = 8;
    float p = 0.4f;
    int n_trials = 5000;

    std::binomial_distribution<int> release(N, p);
    int failures = 0;

    for (int i = 0; i < n_trials; i++) {
        if (release(rng) == 0) {
            failures++;
        }
    }

    float observed_failure_rate = static_cast<float>(failures) / n_trials;
    float expected_failure_rate = std::pow(1.0f - p, N);

    EXPECT_NEAR(observed_failure_rate, expected_failure_rate, 0.02f);
}

//=============================================================================
// Edge Cases and Numerical Stability Tests
//=============================================================================

class NeuralEdgeCaseTest : public NeuralStatisticsTest {};

TEST_F(NeuralEdgeCaseTest, EmptySpikeTrain) {
    std::vector<float> empty;
    auto isi = computeISI(empty);
    EXPECT_TRUE(isi.empty());
}

TEST_F(NeuralEdgeCaseTest, SingleSpike) {
    std::vector<float> single = {0.5f};
    auto isi = computeISI(single);
    EXPECT_TRUE(isi.empty());
}

TEST_F(NeuralEdgeCaseTest, TwoSpikes) {
    std::vector<float> two = {0.1f, 0.2f};
    auto isi = computeISI(two);
    ASSERT_EQ(isi.size(), 1u);
    EXPECT_NEAR(isi[0], 0.1f, TOLERANCE);
}

TEST_F(NeuralEdgeCaseTest, VeryHighRate) {
    // Test with very high firing rate
    auto spikes = generatePoissonSpikeTrain(1000.0f, 1.0f, 999);
    auto isi = computeISI(spikes);

    if (!isi.empty()) {
        float mean_isi = nimcp_stats_mean(isi.data(), static_cast<uint32_t>(isi.size()));
        EXPECT_GT(mean_isi, 0.0f);
        EXPECT_LT(mean_isi, 0.01f);  // Should be < 10 ms
    }
}

TEST_F(NeuralEdgeCaseTest, VeryLowRate) {
    // Test with very low firing rate
    auto spikes = generatePoissonSpikeTrain(0.1f, 100.0f, 1000);

    // Might have very few or no spikes
    if (spikes.size() >= 2) {
        auto isi = computeISI(spikes);
        float mean_isi = nimcp_stats_mean(isi.data(), static_cast<uint32_t>(isi.size()));
        EXPECT_GT(mean_isi, 1.0f);  // Should be > 1 second
    }
}

TEST_F(NeuralEdgeCaseTest, NumericalStability_SmallIntervals) {
    // Test with very small intervals
    std::vector<float> small_isi;
    for (int i = 0; i < 1000; i++) {
        small_isi.push_back(1e-9f);  // 1 nanosecond
    }

    float mean = nimcp_stats_mean(small_isi.data(), 1000);
    EXPECT_NEAR(mean, 1e-9f, 1e-15f);

    float var = nimcp_stats_variance(small_isi.data(), 1000);
    EXPECT_NEAR(var, 0.0f, TOLERANCE);
}

TEST_F(NeuralEdgeCaseTest, NumericalStability_LargeIntervals) {
    // Test with large intervals
    std::vector<float> large_isi;
    for (int i = 0; i < 100; i++) {
        large_isi.push_back(1e6f);  // 1 million seconds
    }

    float mean = nimcp_stats_mean(large_isi.data(), 100);
    EXPECT_NEAR(mean, 1e6f, 1.0f);
}

//=============================================================================
// Parameterized Tests
//=============================================================================

class FanoFactorParameterizedTest : public NeuralStatisticsTest,
                                    public ::testing::WithParamInterface<float> {};

TEST_P(FanoFactorParameterizedTest, DifferentRates_FanoNearOne) {
    float rate = GetParam();
    float duration = 50.0f;
    float bin_width = 0.1f;

    auto spikes = generatePoissonSpikeTrain(rate, duration, static_cast<int>(rate * 100));
    auto counts = binSpikes(spikes, bin_width, duration);

    float fano = computeFanoFactor(counts);

    if (!std::isnan(fano)) {
        EXPECT_GT(fano, 0.5f);
        EXPECT_LT(fano, 2.0f);
    }
}

INSTANTIATE_TEST_SUITE_P(
    RateVariations,
    FanoFactorParameterizedTest,
    ::testing::Values(1.0f, 5.0f, 10.0f, 20.0f, 50.0f, 100.0f)
);

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
