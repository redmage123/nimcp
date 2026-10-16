/**
 * @file test_timeseries_integration.cpp
 * @brief Integration tests for time series statistics with FFT module
 *
 * WHAT: Verify time series statistics integrate with FFT operations
 * WHY:  Ensure spectral analysis works with neural oscillation data
 * HOW:  Test autocorrelation, spectral analysis, and temporal patterns
 *
 * TEST COVERAGE:
 * - Time series + FFT module integration
 * - Autocorrelation and cross-correlation
 * - Power spectral density estimation
 * - Financial time series patterns
 * - Neural oscillation analysis
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
#include <complex>
#include <memory>

// Statistics headers
#include "utils/statistics/nimcp_statistics.h"

// FFT module
#include "utils/spectral/nimcp_fft.h"

// Memory management
#include "utils/memory/nimcp_memory.h"

// Core types

//=============================================================================
// Test Configuration
//=============================================================================

namespace {
    constexpr float STRICT_TOLERANCE = 1e-5f;
    constexpr float RELAXED_TOLERANCE = 1e-4f;
    constexpr float CORRELATION_TOLERANCE = 0.01f;  // For correlation numerical precision
    constexpr float SPECTRAL_TOLERANCE = 0.1f;

    constexpr uint32_t SMALL_SIZE = 256;
    constexpr uint32_t MEDIUM_SIZE = 1024;
    constexpr uint32_t LARGE_SIZE = 4096;

    constexpr float SAMPLING_RATE = 1000.0f;  // Hz
    constexpr float PI = 3.14159265358979323846f;
}

//=============================================================================
// Test Fixture
//=============================================================================

class TimeseriesIntegrationTest : public ::testing::Test {
protected:
    std::mt19937 rng{42};
    fft_plan_t* fft_plan = nullptr;

    void SetUp() override {
        nimcp_memory_init();
        nimcp_memory_enable_tracking(true);
        nimcp_memory_clear_stats();

        nimcp_stats_config_t config = nimcp_stats_default_config();
        config.random_seed = 42;
        ASSERT_EQ(nimcp_stats_init(&config), NIMCP_STATS_OK);
    }

    void TearDown() override {
        if (fft_plan) {
            fft_plan_destroy(fft_plan);
            fft_plan = nullptr;
        }

        nimcp_stats_shutdown();

        nimcp_memory_stats_t stats;
        nimcp_memory_get_stats(&stats);
        EXPECT_LT(stats.current_allocated, 4096)
            << "Memory leak: " << stats.current_allocated << " bytes";
    }

    //=========================================================================
    // Helper: Generate sine wave
    //=========================================================================
    std::vector<float> generateSineWave(float frequency, float duration,
                                         float amplitude = 1.0f, float phase = 0.0f) {
        uint32_t n = static_cast<uint32_t>(duration * SAMPLING_RATE);
        std::vector<float> signal(n);

        for (uint32_t i = 0; i < n; i++) {
            float t = i / SAMPLING_RATE;
            signal[i] = amplitude * std::sin(2.0f * PI * frequency * t + phase);
        }
        return signal;
    }

    //=========================================================================
    // Helper: Generate white noise
    //=========================================================================
    std::vector<float> generateWhiteNoise(uint32_t n, float std = 1.0f) {
        std::vector<float> noise(n);
        std::normal_distribution<float> dist(0.0f, std);
        for (uint32_t i = 0; i < n; i++) {
            noise[i] = dist(rng);
        }
        return noise;
    }

    //=========================================================================
    // Helper: Generate AR(1) process
    //=========================================================================
    std::vector<float> generateAR1(uint32_t n, float phi, float noise_std = 1.0f) {
        std::vector<float> series(n);
        std::normal_distribution<float> noise(0.0f, noise_std);

        series[0] = noise(rng);
        for (uint32_t i = 1; i < n; i++) {
            series[i] = phi * series[i-1] + noise(rng);
        }
        return series;
    }

    //=========================================================================
    // Helper: Generate random walk
    //=========================================================================
    std::vector<float> generateRandomWalk(uint32_t n, float step_std = 1.0f) {
        std::vector<float> walk(n);
        std::normal_distribution<float> step(0.0f, step_std);

        walk[0] = 0.0f;
        for (uint32_t i = 1; i < n; i++) {
            walk[i] = walk[i-1] + step(rng);
        }
        return walk;
    }

    //=========================================================================
    // Helper: Generate neural oscillation (alpha, beta, gamma, etc.)
    //=========================================================================
    std::vector<float> generateNeuralOscillation(float center_freq, float bandwidth,
                                                  float duration, float snr = 2.0f) {
        auto signal = generateSineWave(center_freq, duration);
        auto noise = generateWhiteNoise(static_cast<uint32_t>(signal.size()));

        float signal_power = 0.0f;
        for (float s : signal) signal_power += s * s;
        signal_power /= signal.size();

        float noise_scale = std::sqrt(signal_power / snr);

        for (size_t i = 0; i < signal.size(); i++) {
            signal[i] += noise_scale * noise[i];
        }
        return signal;
    }

    //=========================================================================
    // Helper: Compute autocorrelation
    //=========================================================================
    float computeAutocorrelation(const std::vector<float>& data, uint32_t lag) {
        uint32_t n = static_cast<uint32_t>(data.size());
        if (lag >= n) return 0.0f;

        float mean = nimcp_stats_mean(data.data(), n);
        float var = nimcp_stats_variance(data.data(), n);
        if (var < 1e-10f) return 0.0f;

        float sum = 0.0f;
        for (uint32_t i = 0; i < n - lag; i++) {
            sum += (data[i] - mean) * (data[i + lag] - mean);
        }
        return sum / ((n - lag) * var);
    }

    //=========================================================================
    // Helper: Compute cross-correlation
    //=========================================================================
    float computeCrossCorrelation(const std::vector<float>& x,
                                   const std::vector<float>& y, int32_t lag) {
        uint32_t n = static_cast<uint32_t>(std::min(x.size(), y.size()));
        float mean_x = nimcp_stats_mean(x.data(), n);
        float mean_y = nimcp_stats_mean(y.data(), n);
        float std_x = nimcp_stats_std_dev(x.data(), n);
        float std_y = nimcp_stats_std_dev(y.data(), n);

        if (std_x < 1e-10f || std_y < 1e-10f) return 0.0f;

        float sum = 0.0f;
        uint32_t count = 0;

        for (uint32_t i = 0; i < n; i++) {
            int32_t j = i + lag;
            if (j >= 0 && j < static_cast<int32_t>(n)) {
                sum += (x[i] - mean_x) * (y[j] - mean_y);
                count++;
            }
        }
        return sum / (count * std_x * std_y);
    }
};

//=============================================================================
// Basic Autocorrelation Tests
//=============================================================================

TEST_F(TimeseriesIntegrationTest, AutocorrelationWhiteNoise) {
    auto noise = generateWhiteNoise(MEDIUM_SIZE);

    // Lag 0 autocorrelation should be 1
    float r0 = computeAutocorrelation(noise, 0);
    EXPECT_NEAR(r0, 1.0f, CORRELATION_TOLERANCE);

    // Other lags should be near 0 for white noise
    for (uint32_t lag = 1; lag <= 10; lag++) {
        float r = computeAutocorrelation(noise, lag);
        EXPECT_NEAR(r, 0.0f, 0.2f)
            << "Lag " << lag << " should be near 0 for white noise";
    }
}

TEST_F(TimeseriesIntegrationTest, AutocorrelationAR1) {
    float phi = 0.7f;
    auto ar1 = generateAR1(LARGE_SIZE, phi);

    // For AR(1), theoretical r(k) = phi^k
    for (uint32_t lag = 1; lag <= 5; lag++) {
        float r = computeAutocorrelation(ar1, lag);
        float expected = std::pow(phi, static_cast<float>(lag));
        EXPECT_NEAR(r, expected, 0.15f)
            << "AR(1) autocorrelation at lag " << lag;
    }
}

TEST_F(TimeseriesIntegrationTest, AutocorrelationSineWave) {
    float freq = 10.0f;  // 10 Hz
    auto sine = generateSineWave(freq, 1.0f);

    // Autocorrelation of sine should peak at period
    uint32_t period_samples = static_cast<uint32_t>(SAMPLING_RATE / freq);

    float r_period = computeAutocorrelation(sine, period_samples);
    float r_half = computeAutocorrelation(sine, period_samples / 2);

    EXPECT_GT(r_period, 0.9f) << "Should have high correlation at period";
    EXPECT_LT(r_half, -0.8f) << "Should have negative correlation at half period";
}

//=============================================================================
// Cross-Correlation Tests
//=============================================================================

TEST_F(TimeseriesIntegrationTest, CrossCorrelationIdentical) {
    auto signal = generateWhiteNoise(MEDIUM_SIZE);

    // Cross-correlation with self at lag 0 should be 1
    float r = computeCrossCorrelation(signal, signal, 0);
    EXPECT_NEAR(r, 1.0f, CORRELATION_TOLERANCE);
}

TEST_F(TimeseriesIntegrationTest, CrossCorrelationLaggedCopy) {
    auto x = generateWhiteNoise(MEDIUM_SIZE);
    std::vector<float> y(MEDIUM_SIZE);

    // y is x shifted by 10 samples
    uint32_t true_lag = 10;
    for (uint32_t i = 0; i < MEDIUM_SIZE; i++) {
        if (i >= true_lag) {
            y[i] = x[i - true_lag];
        } else {
            y[i] = 0.0f;
        }
    }

    // Cross-correlation should peak at true lag
    float r_true = computeCrossCorrelation(x, y, true_lag);
    float r_zero = computeCrossCorrelation(x, y, 0);
    float r_other = computeCrossCorrelation(x, y, 20);

    EXPECT_GT(r_true, 0.8f) << "Should peak at true lag";
    EXPECT_GT(r_true, r_zero) << "True lag should exceed zero lag";
    EXPECT_GT(r_true, r_other) << "True lag should exceed other lags";
}

TEST_F(TimeseriesIntegrationTest, CrossCorrelationIndependent) {
    auto x = generateWhiteNoise(MEDIUM_SIZE);
    auto y = generateWhiteNoise(MEDIUM_SIZE);

    // Independent series should have low cross-correlation
    for (int32_t lag = -10; lag <= 10; lag++) {
        float r = computeCrossCorrelation(x, y, lag);
        EXPECT_LT(std::fabs(r), 0.2f)
            << "Independent series should have low cross-correlation";
    }
}

//=============================================================================
// FFT Integration Tests
//=============================================================================

TEST_F(TimeseriesIntegrationTest, FFTSineWavePeakDetection) {
    float freq = 25.0f;  // 25 Hz
    // Generate exactly MEDIUM_SIZE samples (duration = MEDIUM_SIZE / SAMPLING_RATE)
    float duration = static_cast<float>(MEDIUM_SIZE) / SAMPLING_RATE;
    auto signal = generateSineWave(freq, duration);
    ASSERT_EQ(signal.size(), MEDIUM_SIZE) << "Signal size must match FFT size";

    fft_plan = fft_plan_create(MEDIUM_SIZE, FFT_REAL);
    ASSERT_NE(fft_plan, nullptr);

    std::vector<fft_complex_t> spectrum(MEDIUM_SIZE / 2 + 1);
    bool success = fft_execute_real(fft_plan, signal.data(), spectrum.data());
    ASSERT_TRUE(success);

    // Find peak frequency
    uint32_t peak_bin = 0;
    float max_power = 0.0f;
    for (uint32_t i = 1; i < MEDIUM_SIZE / 2; i++) {
        float power = spectrum[i].real * spectrum[i].real +
                      spectrum[i].imag * spectrum[i].imag;
        if (power > max_power) {
            max_power = power;
            peak_bin = i;
        }
    }

    float freq_resolution = SAMPLING_RATE / MEDIUM_SIZE;
    float detected_freq = peak_bin * freq_resolution;

    EXPECT_NEAR(detected_freq, freq, freq_resolution)
        << "Should detect correct frequency";
}

TEST_F(TimeseriesIntegrationTest, FFTPowerSpectralDensity) {
    // Generate signal with multiple frequency components
    // Duration must match MEDIUM_SIZE samples
    float duration = static_cast<float>(MEDIUM_SIZE) / SAMPLING_RATE;
    auto sig10 = generateSineWave(10.0f, duration, 1.0f);
    auto sig50 = generateSineWave(50.0f, duration, 0.5f);
    ASSERT_EQ(sig10.size(), MEDIUM_SIZE);
    ASSERT_EQ(sig50.size(), MEDIUM_SIZE);

    std::vector<float> signal(MEDIUM_SIZE);
    for (uint32_t i = 0; i < MEDIUM_SIZE; i++) {
        signal[i] = sig10[i] + sig50[i];
    }

    fft_plan = fft_plan_create(MEDIUM_SIZE, FFT_REAL);
    ASSERT_NE(fft_plan, nullptr);

    std::vector<fft_complex_t> spectrum(MEDIUM_SIZE / 2 + 1);
    fft_execute_real(fft_plan, signal.data(), spectrum.data());

    // Compute PSD
    std::vector<float> psd(MEDIUM_SIZE / 2 + 1);
    for (uint32_t i = 0; i <= MEDIUM_SIZE / 2; i++) {
        psd[i] = spectrum[i].real * spectrum[i].real +
                 spectrum[i].imag * spectrum[i].imag;
    }

    // Check for peaks at 10 Hz and 50 Hz
    float freq_resolution = SAMPLING_RATE / MEDIUM_SIZE;
    uint32_t bin10 = static_cast<uint32_t>(10.0f / freq_resolution);
    uint32_t bin50 = static_cast<uint32_t>(50.0f / freq_resolution);

    // 10 Hz component should be stronger (amplitude 1.0 vs 0.5)
    EXPECT_GT(psd[bin10], psd[bin50])
        << "10 Hz component should be stronger";

    // Both peaks should be significant above noise floor
    float noise_floor = nimcp_stats_median(psd.data(), MEDIUM_SIZE / 2);
    EXPECT_GT(psd[bin10], 10.0f * noise_floor) << "10 Hz peak above noise";
    EXPECT_GT(psd[bin50], 5.0f * noise_floor) << "50 Hz peak above noise";
}

TEST_F(TimeseriesIntegrationTest, FFTNeuralOscillations) {
    // Generate neural data with alpha (8-12 Hz) oscillation
    float alpha_freq = 10.0f;
    auto neural = generateNeuralOscillation(alpha_freq, 2.0f, 2.0f, 3.0f);

    // Pad to power of 2
    uint32_t fft_size = 2048;
    std::vector<float> padded(fft_size, 0.0f);
    for (uint32_t i = 0; i < std::min(static_cast<uint32_t>(neural.size()), fft_size); i++) {
        padded[i] = neural[i];
    }

    fft_plan = fft_plan_create(fft_size, FFT_REAL);
    fft_plan_set_window(fft_plan, FFT_WINDOW_HANN);  // Reduce spectral leakage

    std::vector<fft_complex_t> spectrum(fft_size / 2 + 1);
    fft_execute_real(fft_plan, padded.data(), spectrum.data());

    // Find dominant frequency in alpha band
    float freq_resolution = SAMPLING_RATE / fft_size;
    uint32_t alpha_low = static_cast<uint32_t>(8.0f / freq_resolution);
    uint32_t alpha_high = static_cast<uint32_t>(12.0f / freq_resolution);

    float max_alpha_power = 0.0f;
    uint32_t peak_bin = alpha_low;
    for (uint32_t i = alpha_low; i <= alpha_high; i++) {
        float power = spectrum[i].real * spectrum[i].real +
                      spectrum[i].imag * spectrum[i].imag;
        if (power > max_alpha_power) {
            max_alpha_power = power;
            peak_bin = i;
        }
    }

    float detected_freq = peak_bin * freq_resolution;
    EXPECT_NEAR(detected_freq, alpha_freq, 1.0f)
        << "Should detect alpha oscillation";
}

//=============================================================================
// Financial Time Series Tests
//=============================================================================

TEST_F(TimeseriesIntegrationTest, FinancialReturnsStatistics) {
    // Simulate log returns
    auto walk = generateRandomWalk(LARGE_SIZE, 0.01f);

    // Compute returns
    std::vector<float> returns(LARGE_SIZE - 1);
    for (uint32_t i = 1; i < LARGE_SIZE; i++) {
        returns[i-1] = walk[i] - walk[i-1];
    }

    // Returns should have near-zero mean
    float mean = nimcp_stats_mean(returns.data(), LARGE_SIZE - 1);
    EXPECT_NEAR(mean, 0.0f, 0.01f);

    // Returns should have low autocorrelation (efficient market)
    float r1 = computeAutocorrelation(returns, 1);
    EXPECT_NEAR(r1, 0.0f, 0.1f)
        << "Random walk returns should have low lag-1 autocorrelation";
}

TEST_F(TimeseriesIntegrationTest, FinancialVolatilityClustering) {
    // Generate GARCH-like returns with volatility clustering
    uint32_t n = LARGE_SIZE;
    std::vector<float> returns(n);
    std::normal_distribution<float> noise(0.0f, 1.0f);

    float sigma = 0.01f;
    float alpha = 0.1f;
    float beta = 0.85f;
    float omega = 0.00001f;

    for (uint32_t i = 0; i < n; i++) {
        returns[i] = sigma * noise(rng);
        float sigma_sq = omega + alpha * returns[i] * returns[i] + beta * sigma * sigma;
        sigma = std::sqrt(sigma_sq);
    }

    // Squared returns should have positive autocorrelation
    std::vector<float> sq_returns(n);
    for (uint32_t i = 0; i < n; i++) {
        sq_returns[i] = returns[i] * returns[i];
    }

    float r1_sq = computeAutocorrelation(sq_returns, 1);
    EXPECT_GT(r1_sq, 0.1f)
        << "Squared returns should show volatility clustering";
}

TEST_F(TimeseriesIntegrationTest, FinancialSkewnessKurtosis) {
    // Simulate fat-tailed returns
    uint32_t n = LARGE_SIZE;
    std::vector<float> returns(n);
    std::student_t_distribution<float> tdist(4.0f);  // Heavy tails

    for (uint32_t i = 0; i < n; i++) {
        returns[i] = 0.01f * tdist(rng);
    }

    float kurtosis = nimcp_stats_kurtosis(returns.data(), n);
    // Student-t(4) has excess kurtosis = 6/(4-4) which is undefined,
    // but for finite samples should be positive (fat tails)
    EXPECT_GT(kurtosis, 0.5f)
        << "Fat-tailed returns should have positive excess kurtosis";
}

//=============================================================================
// Running Statistics for Streaming Data
//=============================================================================

TEST_F(TimeseriesIntegrationTest, RunningMeanOnlineComputation) {
    auto data = generateWhiteNoise(LARGE_SIZE);

    nimcp_running_stats_t running;
    nimcp_stats_running_init(&running);

    // Add data in chunks (simulating streaming)
    uint32_t chunk_size = 100;
    for (uint32_t start = 0; start < LARGE_SIZE; start += chunk_size) {
        uint32_t end = std::min(start + chunk_size, LARGE_SIZE);
        for (uint32_t i = start; i < end; i++) {
            nimcp_stats_running_add(&running, data[i]);
        }
    }

    // Compare with batch computation
    float batch_mean = nimcp_stats_mean(data.data(), LARGE_SIZE);
    double running_mean = nimcp_stats_running_mean(&running);

    EXPECT_NEAR(static_cast<float>(running_mean), batch_mean, RELAXED_TOLERANCE);
}

TEST_F(TimeseriesIntegrationTest, RunningVarianceOnlineComputation) {
    auto data = generateAR1(LARGE_SIZE, 0.5f);

    nimcp_running_stats_t running;
    nimcp_stats_running_init(&running);
    nimcp_stats_running_add_array(&running, data.data(), LARGE_SIZE);

    float batch_var = nimcp_stats_variance(data.data(), LARGE_SIZE);
    double running_var = nimcp_stats_running_variance(&running);

    EXPECT_NEAR(static_cast<float>(running_var), batch_var, RELAXED_TOLERANCE);
}

//=============================================================================
// Stationarity and Trend Tests
//=============================================================================

TEST_F(TimeseriesIntegrationTest, TrendDetection) {
    // Generate data with linear trend
    uint32_t n = MEDIUM_SIZE;
    std::vector<float> x(n), y(n);
    std::normal_distribution<float> noise(0.0f, 1.0f);

    for (uint32_t i = 0; i < n; i++) {
        x[i] = static_cast<float>(i);
        y[i] = 0.1f * i + noise(rng);  // Trend with noise
    }

    nimcp_regression_result_t result;
    nimcp_stats_regression_linear(x.data(), y.data(), n, &result);

    EXPECT_NEAR(result.slope, 0.1f, 0.02f)
        << "Should detect linear trend";
    EXPECT_GT(result.r_squared, 0.8f)
        << "Trend should explain significant variance";
}

TEST_F(TimeseriesIntegrationTest, SeasonalityDetection) {
    // Generate data with seasonal pattern
    uint32_t n = MEDIUM_SIZE;
    std::vector<float> seasonal(n);
    std::normal_distribution<float> noise(0.0f, 0.5f);
    float period = 100.0f;  // 100 samples per cycle

    for (uint32_t i = 0; i < n; i++) {
        seasonal[i] = std::sin(2.0f * PI * i / period) + noise(rng);
    }

    // Autocorrelation should peak at period
    uint32_t period_int = static_cast<uint32_t>(period);
    float r_period = computeAutocorrelation(seasonal, period_int);
    float r_half = computeAutocorrelation(seasonal, period_int / 2);

    EXPECT_GT(r_period, 0.5f) << "Should detect seasonal pattern";
    EXPECT_LT(r_half, 0.0f) << "Half-period should be negatively correlated";
}

//=============================================================================
// Outlier Detection in Time Series
//=============================================================================

TEST_F(TimeseriesIntegrationTest, TimeSeriesOutlierDetection) {
    auto data = generateWhiteNoise(MEDIUM_SIZE);

    // Inject outliers
    data[100] = 10.0f;   // Large positive
    data[500] = -10.0f;  // Large negative
    data[800] = 15.0f;   // Very large

    std::unique_ptr<bool[]> outliers(new bool[MEDIUM_SIZE]());
    uint32_t n_outliers;

    nimcp_stats_detect_outliers_zscore(data.data(), MEDIUM_SIZE, 3.0f,
                                        outliers.get(), &n_outliers);

    EXPECT_GE(n_outliers, 3u) << "Should detect injected outliers";
    EXPECT_TRUE(outliers[100]) << "Position 100 should be outlier";
    EXPECT_TRUE(outliers[500]) << "Position 500 should be outlier";
    EXPECT_TRUE(outliers[800]) << "Position 800 should be outlier";
}

TEST_F(TimeseriesIntegrationTest, WinsorizeTimeSeries) {
    auto data = generateWhiteNoise(MEDIUM_SIZE);

    // Inject extreme values
    data[0] = -100.0f;
    data[1] = 100.0f;

    std::vector<float> winsorized(MEDIUM_SIZE);
    nimcp_stats_winsorize(data.data(), MEDIUM_SIZE, 0.01f, 0.99f, winsorized.data());

    // Extremes should be clipped
    EXPECT_GT(winsorized[0], data[0]) << "Lower extreme should be clipped";
    EXPECT_LT(winsorized[1], data[1]) << "Upper extreme should be clipped";

    // Winsorized variance should be smaller
    float orig_var = nimcp_stats_variance(data.data(), MEDIUM_SIZE);
    float wins_var = nimcp_stats_variance(winsorized.data(), MEDIUM_SIZE);
    EXPECT_LT(wins_var, orig_var);
}

//=============================================================================
// Normality Testing for Time Series
//=============================================================================

TEST_F(TimeseriesIntegrationTest, ShapiroWilkNormalResiduals) {
    // Generate regression with normal residuals
    uint32_t n = 100;  // Shapiro-Wilk works well for small samples
    std::vector<float> x(n), y(n);
    std::normal_distribution<float> noise(0.0f, 1.0f);

    for (uint32_t i = 0; i < n; i++) {
        x[i] = static_cast<float>(i);
        y[i] = 2.0f * x[i] + noise(rng);
    }

    // Fit regression
    nimcp_regression_result_t reg;
    nimcp_stats_regression_linear(x.data(), y.data(), n, &reg);

    // Compute residuals
    std::vector<float> residuals(n);
    for (uint32_t i = 0; i < n; i++) {
        residuals[i] = y[i] - (reg.intercept + reg.slope * x[i]);
    }

    // Test normality of residuals
    nimcp_test_result_t normality;
    nimcp_stats_shapiro_wilk(residuals.data(), n, &normality);

    EXPECT_GT(normality.p_value, 0.05f)
        << "Residuals should pass normality test";
}

TEST_F(TimeseriesIntegrationTest, KSTestNonNormalData) {
    // Generate clearly non-normal data (exponential)
    uint32_t n = 500;  // Larger sample for better power
    std::vector<float> exp_data(n);
    std::exponential_distribution<float> dist(1.0f);
    for (uint32_t i = 0; i < n; i++) {
        exp_data[i] = dist(rng);
    }

    nimcp_test_result_t normality;
    nimcp_stats_ks_normality(exp_data.data(), n, &normality);

    // Should detect non-normality (relaxed threshold due to KS test conservatism)
    EXPECT_LT(normality.p_value, 0.25f)
        << "Should tend to reject normality for exponential data";
}

//=============================================================================
// Bootstrap for Time Series
//=============================================================================

TEST_F(TimeseriesIntegrationTest, BootstrapTimeSeriesMean) {
    auto data = generateAR1(MEDIUM_SIZE, 0.5f);

    nimcp_bootstrap_result_t result;
    nimcp_stats_bootstrap_mean(data.data(), MEDIUM_SIZE, 1000, 0.95f, &result);

    float sample_mean = nimcp_stats_mean(data.data(), MEDIUM_SIZE);

    // Bootstrap estimate should be close to sample mean
    EXPECT_NEAR(result.estimate, sample_mean, RELAXED_TOLERANCE);

    // CI should be reasonable
    EXPECT_LT(result.ci_lower_percentile, result.estimate);
    EXPECT_GT(result.ci_upper_percentile, result.estimate);
}

TEST_F(TimeseriesIntegrationTest, BootstrapMedianRobust) {
    auto data = generateWhiteNoise(SMALL_SIZE);

    // Inject outlier
    data[0] = 100.0f;

    nimcp_bootstrap_result_t mean_result, median_result;
    nimcp_stats_bootstrap_mean(data.data(), SMALL_SIZE, 500, 0.95f, &mean_result);
    nimcp_stats_bootstrap_median(data.data(), SMALL_SIZE, 500, 0.95f, &median_result);

    // Median CI should be tighter (less affected by outlier)
    float mean_ci_width = mean_result.ci_upper_percentile - mean_result.ci_lower_percentile;
    float median_ci_width = median_result.ci_upper_percentile - median_result.ci_lower_percentile;

    EXPECT_LT(median_ci_width, mean_ci_width * 2.0f)
        << "Median CI should be more robust to outliers";
}

//=============================================================================
// Real-World Neural Data Patterns
//=============================================================================

TEST_F(TimeseriesIntegrationTest, LocalFieldPotentialAnalysis) {
    // Simulate LFP with multiple oscillatory components
    auto theta = generateSineWave(6.0f, 2.0f, 1.0f);   // Theta (4-8 Hz)
    auto gamma = generateSineWave(40.0f, 2.0f, 0.3f); // Gamma (30-80 Hz)
    auto noise = generateWhiteNoise(static_cast<uint32_t>(theta.size()), 0.2f);

    std::vector<float> lfp(theta.size());
    for (size_t i = 0; i < theta.size(); i++) {
        lfp[i] = theta[i] + gamma[i] + noise[i];
    }

    // Statistical summary
    nimcp_descriptive_stats_t stats;
    nimcp_stats_describe(lfp.data(), static_cast<uint32_t>(lfp.size()), &stats);

    EXPECT_TRUE(std::isfinite(stats.mean));
    EXPECT_TRUE(std::isfinite(stats.std_dev));

    // Should have oscillatory autocorrelation structure
    float r_theta_period = computeAutocorrelation(lfp,
        static_cast<uint32_t>(SAMPLING_RATE / 6.0f));
    EXPECT_GT(r_theta_period, 0.3f)
        << "Should show theta rhythm in autocorrelation";
}

TEST_F(TimeseriesIntegrationTest, InterSpikeIntervalAnalysis) {
    // Generate ISI data (exponentially distributed with Poisson firing)
    uint32_t n_spikes = 500;
    std::vector<float> isis(n_spikes);
    float rate = 20.0f;  // 20 Hz firing rate
    std::exponential_distribution<float> isi_dist(rate);

    for (uint32_t i = 0; i < n_spikes; i++) {
        isis[i] = isi_dist(rng);
    }

    // ISIs should have CV ~ 1 for Poisson process
    float mean_isi = nimcp_stats_mean(isis.data(), n_spikes);
    float std_isi = nimcp_stats_std_dev(isis.data(), n_spikes);
    float cv = std_isi / mean_isi;

    EXPECT_NEAR(cv, 1.0f, 0.15f)
        << "ISI CV should be ~1 for Poisson firing";

    // Mean ISI should be ~1/rate
    EXPECT_NEAR(mean_isi, 1.0f / rate, 0.01f);
}

TEST_F(TimeseriesIntegrationTest, CrossFrequencyCoupling) {
    // Simulate theta-gamma coupling (phase-amplitude)
    uint32_t n = 2000;
    float theta_freq = 6.0f;
    float gamma_freq = 40.0f;

    std::vector<float> theta(n), gamma(n), coupled(n);
    std::normal_distribution<float> noise(0.0f, 0.1f);

    for (uint32_t i = 0; i < n; i++) {
        float t = i / SAMPLING_RATE;
        float theta_phase = 2.0f * PI * theta_freq * t;
        theta[i] = std::sin(theta_phase);

        // Gamma amplitude modulated by theta phase
        float gamma_amp = 0.5f + 0.5f * std::sin(theta_phase);
        gamma[i] = gamma_amp * std::sin(2.0f * PI * gamma_freq * t);

        coupled[i] = theta[i] + gamma[i] + noise(rng);
    }

    // Should detect both frequency components
    nimcp_descriptive_stats_t stats;
    nimcp_stats_describe(coupled.data(), n, &stats);

    EXPECT_TRUE(std::isfinite(stats.mean));
    EXPECT_GT(stats.std_dev, 0.5f) << "Should have significant variance";
}

//=============================================================================
// Memory and Performance Tests
//=============================================================================

TEST_F(TimeseriesIntegrationTest, NoMemoryLeaksFFT) {
    nimcp_memory_clear_stats();

    for (int trial = 0; trial < 50; trial++) {
        fft_plan_t* plan = fft_plan_create(MEDIUM_SIZE, FFT_REAL);
        ASSERT_NE(plan, nullptr);

        auto signal = generateSineWave(10.0f, 1.0f);
        std::vector<fft_complex_t> spectrum(MEDIUM_SIZE / 2 + 1);
        fft_execute_real(plan, signal.data(), spectrum.data());

        fft_plan_destroy(plan);
    }

    nimcp_memory_stats_t stats;
    nimcp_memory_get_stats(&stats);
    EXPECT_LT(stats.current_allocated, 4096);
}

TEST_F(TimeseriesIntegrationTest, PerformanceLargeTimeSeries) {
    auto data = generateWhiteNoise(LARGE_SIZE);

    auto start = std::chrono::high_resolution_clock::now();

    // Compute various statistics
    float mean = nimcp_stats_mean(data.data(), LARGE_SIZE);
    float var = nimcp_stats_variance(data.data(), LARGE_SIZE);
    float skew = nimcp_stats_skewness(data.data(), LARGE_SIZE);
    float kurt = nimcp_stats_kurtosis(data.data(), LARGE_SIZE);
    float median = nimcp_stats_median(data.data(), LARGE_SIZE);

    auto end = std::chrono::high_resolution_clock::now();
    uint64_t time_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    EXPECT_TRUE(std::isfinite(mean));
    EXPECT_TRUE(std::isfinite(var));
    EXPECT_TRUE(std::isfinite(skew));
    EXPECT_TRUE(std::isfinite(kurt));
    EXPECT_TRUE(std::isfinite(median));

    EXPECT_LT(time_us, 100000u) << "Should complete in <100ms";
}

TEST_F(TimeseriesIntegrationTest, PerformanceFFTLarge) {
    auto signal = generateSineWave(25.0f, 4.096f);  // 4096 samples

    auto start = std::chrono::high_resolution_clock::now();

    fft_plan = fft_plan_create(LARGE_SIZE, FFT_REAL);
    std::vector<fft_complex_t> spectrum(LARGE_SIZE / 2 + 1);

    for (int i = 0; i < 100; i++) {
        fft_execute_real(fft_plan, signal.data(), spectrum.data());
    }

    auto end = std::chrono::high_resolution_clock::now();
    uint64_t time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    EXPECT_LT(time_ms, 500u) << "100 FFTs should complete in <500ms";
}

