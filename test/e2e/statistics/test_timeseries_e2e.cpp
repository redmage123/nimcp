//=============================================================================
// test_timeseries_e2e.cpp - Financial Time Series Analysis E2E Tests
//=============================================================================
/**
 * @file test_timeseries_e2e.cpp
 * @brief End-to-end tests for time series and financial data analysis
 *
 * WHAT: Complete time series analysis pipelines from raw data to forecasts
 * WHY:  Verify statistics module handles temporal data and financial scenarios
 * HOW:  Test ARIMA, volatility, change detection, forecasting workflows
 *
 * TEST SCENARIOS:
 * 1. Returns and volatility computation
 * 2. ARIMA model fitting (simplified)
 * 3. Change point detection
 * 4. Regime switching analysis
 * 5. Risk analysis (VaR, ES)
 * 6. Rolling statistics computation
 * 7. Autocorrelation analysis
 * 8. Seasonality detection
 * 9. Trend extraction
 * 10. Cointegration testing
 * 11. GARCH volatility modeling
 * 12. Portfolio statistics
 * 13. Drawdown analysis
 * 14. Moving average crossovers
 * 15. Mean reversion testing
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

class TimeSeriesE2ETest : public ::testing::Test {
protected:
    void SetUp() override {
        config = nimcp_stats_default_config();
        ASSERT_EQ(nimcp_stats_init(&config), NIMCP_STATS_OK);
        rng.seed(42);
    }

    void TearDown() override {
        nimcp_stats_shutdown();
    }

    nimcp_stats_config_t config;
    std::mt19937 rng;

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

    // Generate random walk (stock prices)
    std::vector<float> generate_random_walk(int n, float start, float drift, float volatility) {
        std::vector<float> prices(n);
        prices[0] = start;
        for (int i = 1; i < n; i++) {
            float ret = drift + volatility * generate_normal(1, 0.0f, 1.0f)[0];
            prices[i] = prices[i-1] * std::exp(ret);
        }
        return prices;
    }

    // Generate AR(1) process
    std::vector<float> generate_ar1(int n, float phi, float sigma) {
        std::vector<float> x(n);
        x[0] = 0.0f;
        for (int i = 1; i < n; i++) {
            x[i] = phi * x[i-1] + sigma * generate_normal(1, 0.0f, 1.0f)[0];
        }
        return x;
    }

    // Compute log returns
    std::vector<float> compute_returns(const std::vector<float>& prices) {
        std::vector<float> returns(prices.size() - 1);
        for (size_t i = 0; i < returns.size(); i++) {
            returns[i] = std::log(prices[i+1] / prices[i]);
        }
        return returns;
    }

    // Compute rolling mean
    std::vector<float> rolling_mean(const std::vector<float>& data, int window) {
        std::vector<float> result(data.size() - window + 1);
        for (size_t i = 0; i <= data.size() - window; i++) {
            float sum = 0.0f;
            for (int j = 0; j < window; j++) {
                sum += data[i + j];
            }
            result[i] = sum / window;
        }
        return result;
    }

    // Compute rolling std
    std::vector<float> rolling_std(const std::vector<float>& data, int window) {
        std::vector<float> result(data.size() - window + 1);
        for (size_t i = 0; i <= data.size() - window; i++) {
            std::vector<float> window_data(data.begin() + i, data.begin() + i + window);
            result[i] = nimcp_stats_std_dev(window_data.data(), window);
        }
        return result;
    }

    // Compute autocorrelation at lag k
    float autocorrelation(const std::vector<float>& data, int lag) {
        int n = data.size();
        if (lag >= n) return 0.0f;

        float mean = nimcp_stats_mean(data.data(), n);
        float var = nimcp_stats_variance(data.data(), n);

        float cov = 0.0f;
        for (int i = 0; i < n - lag; i++) {
            cov += (data[i] - mean) * (data[i + lag] - mean);
        }
        cov /= (n - lag);

        return cov / (var + 1e-10f);
    }
};

//=============================================================================
// E2E Test 1: Returns and Volatility Computation
//=============================================================================

TEST_F(TimeSeriesE2ETest, ReturnsAndVolatilityComputation) {
    START_TIMER();

    // Simulate 5 years of daily prices
    const int n_days = 252 * 5;  // ~5 years
    const float annual_drift = 0.08f;   // 8% annual return
    const float annual_vol = 0.20f;     // 20% annual volatility
    const float daily_drift = annual_drift / 252;
    const float daily_vol = annual_vol / std::sqrt(252.0f);

    auto prices = generate_random_walk(n_days, 100.0f, daily_drift, daily_vol);
    auto returns = compute_returns(prices);

    // Analyze returns
    nimcp_descriptive_stats_t return_stats;
    EXPECT_EQ(nimcp_stats_describe(returns.data(), returns.size(), &return_stats), NIMCP_STATS_OK);

    // Mean daily return should be close to daily_drift
    EXPECT_NEAR(return_stats.mean, daily_drift, daily_drift * 2);

    // Daily volatility should be close to daily_vol
    EXPECT_NEAR(return_stats.std_dev, daily_vol, daily_vol * 0.5f);

    // Annualized volatility
    float annualized_vol = return_stats.std_dev * std::sqrt(252.0f);
    EXPECT_NEAR(annualized_vol, annual_vol, annual_vol * 0.3f);

    // Returns should be roughly normal (low skew and kurtosis)
    EXPECT_LT(std::abs(return_stats.skewness), 1.0f);

    double elapsed = STOP_TIMER_MS();
    EXPECT_LT(elapsed, 3000.0);

    std::cout << "Returns Analysis: " << n_days << " days, "
              << "mean return=" << return_stats.mean * 252 * 100 << "% annual, "
              << "volatility=" << annualized_vol * 100 << "% annual, "
              << "time=" << elapsed << " ms" << std::endl;
}

//=============================================================================
// E2E Test 2: ARIMA Model Fitting (Simplified AR(1))
//=============================================================================

TEST_F(TimeSeriesE2ETest, ARIMAModelFitting) {
    START_TIMER();

    // Generate AR(1) process with known parameter
    const int n = 1000;
    const float true_phi = 0.7f;
    const float sigma = 1.0f;

    auto ar1_data = generate_ar1(n, true_phi, sigma);

    // Estimate AR(1) parameter using Yule-Walker
    float rho1 = autocorrelation(ar1_data, 1);
    float estimated_phi = rho1;

    // Estimate should be close to true value
    EXPECT_NEAR(estimated_phi, true_phi, 0.15f);

    // Check lag-2 autocorrelation (should be ~phi^2)
    float rho2 = autocorrelation(ar1_data, 2);
    EXPECT_NEAR(rho2, true_phi * true_phi, 0.2f);

    // Residual analysis
    std::vector<float> residuals(n - 1);
    for (int i = 1; i < n; i++) {
        residuals[i-1] = ar1_data[i] - estimated_phi * ar1_data[i-1];
    }

    float residual_autocorr = autocorrelation(residuals, 1);
    // Residuals should have low autocorrelation
    EXPECT_LT(std::abs(residual_autocorr), 0.15f);

    double elapsed = STOP_TIMER_MS();
    EXPECT_LT(elapsed, 3000.0);

    std::cout << "AR(1) Fitting: true phi=" << true_phi
              << ", estimated phi=" << estimated_phi
              << ", rho2=" << rho2
              << ", time=" << elapsed << " ms" << std::endl;
}

//=============================================================================
// E2E Test 3: Change Point Detection
//=============================================================================

TEST_F(TimeSeriesE2ETest, ChangePointDetection) {
    START_TIMER();

    // Generate data with change point
    const int n1 = 500;  // Before change
    const int n2 = 500;  // After change
    const int n_total = n1 + n2;

    std::vector<float> data(n_total);

    // First segment: mean=0, std=1
    for (int i = 0; i < n1; i++) {
        data[i] = generate_normal(1, 0.0f, 1.0f)[0];
    }

    // Second segment: mean=2, std=1 (mean shift)
    for (int i = n1; i < n_total; i++) {
        data[i] = generate_normal(1, 2.0f, 1.0f)[0];
    }

    // CUSUM-like detection
    float cumsum = 0.0f;
    float overall_mean = nimcp_stats_mean(data.data(), n_total);
    std::vector<float> cusum_values(n_total);

    for (int i = 0; i < n_total; i++) {
        cumsum += data[i] - overall_mean;
        cusum_values[i] = cumsum;
    }

    // Find max deviation
    float max_cusum = *std::max_element(cusum_values.begin(), cusum_values.end());
    float min_cusum = *std::min_element(cusum_values.begin(), cusum_values.end());
    int max_idx = std::distance(cusum_values.begin(),
                                std::max_element(cusum_values.begin(), cusum_values.end()));
    int min_idx = std::distance(cusum_values.begin(),
                                std::min_element(cusum_values.begin(), cusum_values.end()));

    // Change point should be detected near true location
    int detected_change = std::abs(max_cusum) > std::abs(min_cusum) ? max_idx : min_idx;
    EXPECT_NEAR(detected_change, n1, 100);  // Within 100 samples

    // Two-sample t-test at detected change point
    std::vector<float> segment1(data.begin(), data.begin() + detected_change);
    std::vector<float> segment2(data.begin() + detected_change, data.end());

    if (!segment1.empty() && !segment2.empty()) {
        nimcp_test_result_t result;
        nimcp_stats_ttest_two_sample(
            segment1.data(), segment1.size(),
            segment2.data(), segment2.size(),
            true, NIMCP_TEST_TWO_SIDED, 0.95f, &result);

        // Should detect significant difference
        EXPECT_LT(result.p_value, 0.05f);
    }

    double elapsed = STOP_TIMER_MS();
    EXPECT_LT(elapsed, 3000.0);

    std::cout << "Change Point: true=" << n1 << ", detected=" << detected_change
              << ", error=" << std::abs(detected_change - n1)
              << ", time=" << elapsed << " ms" << std::endl;
}

//=============================================================================
// E2E Test 4: Regime Switching Analysis
//=============================================================================

TEST_F(TimeSeriesE2ETest, RegimeSwitchingAnalysis) {
    START_TIMER();

    // Generate data with two regimes (low and high volatility)
    const int n = 1000;
    const int regime_length = 100;

    std::vector<float> data(n);
    std::vector<int> true_regimes(n);

    for (int i = 0; i < n; i++) {
        // Switch regime periodically
        int regime = (i / regime_length) % 2;
        true_regimes[i] = regime;

        float vol = (regime == 0) ? 0.5f : 2.0f;
        data[i] = generate_normal(1, 0.0f, vol)[0];
    }

    // Detect regimes using rolling volatility
    const int window = 50;
    auto rolling_vol = rolling_std(data, window);

    // Classify regimes based on volatility threshold
    float median_vol = nimcp_stats_median(rolling_vol.data(), rolling_vol.size());
    std::vector<int> detected_regimes(rolling_vol.size());

    for (size_t i = 0; i < rolling_vol.size(); i++) {
        detected_regimes[i] = rolling_vol[i] > median_vol ? 1 : 0;
    }

    // Compute regime statistics
    std::vector<float> low_vol_data, high_vol_data;
    for (int i = window / 2; i < n - window / 2; i++) {
        int detected = detected_regimes[i - window / 2];
        if (detected == 0) {
            low_vol_data.push_back(data[i]);
        } else {
            high_vol_data.push_back(data[i]);
        }
    }

    float vol_low = nimcp_stats_std_dev(low_vol_data.data(), low_vol_data.size());
    float vol_high = nimcp_stats_std_dev(high_vol_data.data(), high_vol_data.size());

    // High volatility regime should have higher std
    EXPECT_GT(vol_high, vol_low);

    double elapsed = STOP_TIMER_MS();
    EXPECT_LT(elapsed, 3000.0);

    std::cout << "Regime Switching: vol_low=" << vol_low
              << ", vol_high=" << vol_high
              << ", time=" << elapsed << " ms" << std::endl;
}

//=============================================================================
// E2E Test 5: Risk Analysis (VaR and Expected Shortfall)
//=============================================================================

TEST_F(TimeSeriesE2ETest, RiskAnalysisVaRES) {
    START_TIMER();

    // Generate return series
    const int n = 1000;
    const float mean_return = 0.0005f;  // 0.05% daily
    const float vol = 0.02f;  // 2% daily

    auto returns = generate_normal(n, mean_return, vol);

    // Historical VaR at 95% and 99%
    float var_95 = nimcp_stats_quantile(returns.data(), n, 0.05f);  // 5th percentile
    float var_99 = nimcp_stats_quantile(returns.data(), n, 0.01f);  // 1st percentile

    // VaR should be negative (losses)
    EXPECT_LT(var_95, 0.0f);
    EXPECT_LT(var_99, 0.0f);

    // 99% VaR should be more extreme than 95% VaR
    EXPECT_LT(var_99, var_95);

    // Expected Shortfall (average of returns below VaR)
    std::vector<float> tail_losses;
    for (float r : returns) {
        if (r <= var_95) {
            tail_losses.push_back(r);
        }
    }

    float es_95 = nimcp_stats_mean(tail_losses.data(), tail_losses.size());

    // ES should be more extreme than VaR
    EXPECT_LT(es_95, var_95);

    // Parametric VaR (assuming normal)
    float parametric_var_95 = mean_return + vol * nimcp_stats_quantile_standard_normal(0.05f);

    // Historical and parametric should be similar
    EXPECT_NEAR(var_95, parametric_var_95, vol);

    double elapsed = STOP_TIMER_MS();
    EXPECT_LT(elapsed, 2000.0);

    std::cout << "Risk Analysis: VaR95=" << var_95 * 100 << "%, "
              << "VaR99=" << var_99 * 100 << "%, "
              << "ES95=" << es_95 * 100 << "%, "
              << "time=" << elapsed << " ms" << std::endl;
}

//=============================================================================
// E2E Test 6: Rolling Statistics Computation
//=============================================================================

TEST_F(TimeSeriesE2ETest, RollingStatisticsComputation) {
    START_TIMER();

    // Generate price series
    const int n = 500;
    auto prices = generate_random_walk(n, 100.0f, 0.0003f, 0.015f);

    // Compute various rolling statistics
    const int window = 20;

    auto roll_mean = rolling_mean(prices, window);
    auto roll_vol = rolling_std(prices, window);

    // Rolling quantiles
    std::vector<float> roll_median(n - window + 1);
    std::vector<float> roll_q75(n - window + 1);

    for (int i = 0; i <= n - window; i++) {
        std::vector<float> window_data(prices.begin() + i, prices.begin() + i + window);
        roll_median[i] = nimcp_stats_median(window_data.data(), window);
        roll_q75[i] = nimcp_stats_quantile(window_data.data(), window, 0.75f);
    }

    // Verify relationships
    // Median should be between mean and Q75 (generally)
    nimcp_descriptive_stats_t mean_stats, median_stats;
    nimcp_stats_describe(roll_mean.data(), roll_mean.size(), &mean_stats);
    nimcp_stats_describe(roll_median.data(), roll_median.size(), &median_stats);

    // Rolling volatility should be positive
    for (float v : roll_vol) {
        EXPECT_GE(v, 0.0f);
    }

    // Analyze volatility clustering
    float vol_autocorr = autocorrelation(roll_vol, 1);
    // Volatility should show persistence
    EXPECT_GT(vol_autocorr, 0.5f);

    double elapsed = STOP_TIMER_MS();
    EXPECT_LT(elapsed, 3000.0);

    std::cout << "Rolling Stats: window=" << window
              << ", vol autocorr=" << vol_autocorr
              << ", time=" << elapsed << " ms" << std::endl;
}

//=============================================================================
// E2E Test 7: Autocorrelation Analysis
//=============================================================================

TEST_F(TimeSeriesE2ETest, AutocorrelationAnalysis) {
    START_TIMER();

    // Generate different processes and analyze ACF
    const int n = 500;
    const int max_lag = 20;

    // AR(1) with phi=0.8
    auto ar1 = generate_ar1(n, 0.8f, 1.0f);

    // White noise
    auto white_noise = generate_normal(n, 0.0f, 1.0f);

    // Random walk (integrated)
    std::vector<float> random_walk(n);
    random_walk[0] = 0.0f;
    for (int i = 1; i < n; i++) {
        random_walk[i] = random_walk[i-1] + generate_normal(1, 0.0f, 1.0f)[0];
    }

    // Compute ACF
    std::vector<float> acf_ar1(max_lag), acf_white(max_lag), acf_rw(max_lag);

    for (int lag = 1; lag <= max_lag; lag++) {
        acf_ar1[lag-1] = autocorrelation(ar1, lag);
        acf_white[lag-1] = autocorrelation(white_noise, lag);
        acf_rw[lag-1] = autocorrelation(random_walk, lag);
    }

    // AR(1): ACF should decay exponentially
    EXPECT_GT(acf_ar1[0], 0.6f);  // Lag 1 should be near phi
    EXPECT_GT(acf_ar1[0], acf_ar1[4]);  // Should decay

    // White noise: ACF should be near zero
    for (int i = 0; i < max_lag; i++) {
        EXPECT_LT(std::abs(acf_white[i]), 0.2f);
    }

    // Random walk: ACF stays high
    EXPECT_GT(acf_rw[max_lag - 1], 0.8f);

    double elapsed = STOP_TIMER_MS();
    EXPECT_LT(elapsed, 2000.0);

    std::cout << "ACF Analysis: AR1(1)=" << acf_ar1[0]
              << ", white(1)=" << acf_white[0]
              << ", RW(" << max_lag << ")=" << acf_rw[max_lag-1]
              << ", time=" << elapsed << " ms" << std::endl;
}

//=============================================================================
// E2E Test 8: Seasonality Detection
//=============================================================================

TEST_F(TimeSeriesE2ETest, SeasonalityDetection) {
    START_TIMER();

    // Generate data with known seasonality
    const int n = 500;
    const int period = 20;

    std::vector<float> seasonal_data(n);
    for (int i = 0; i < n; i++) {
        // Seasonal component
        float seasonal = std::sin(2 * M_PI * i / period);
        // Trend
        float trend = 0.01f * i;
        // Noise
        float noise = generate_normal(1, 0.0f, 0.5f)[0];

        seasonal_data[i] = seasonal + trend + noise;
    }

    // Detect seasonality using autocorrelation
    std::vector<float> acf(period * 2);
    for (int lag = 1; lag <= period * 2; lag++) {
        acf[lag-1] = autocorrelation(seasonal_data, lag);
    }

    // Find local maxima in ACF (should be at multiples of period)
    std::vector<int> peaks;
    for (int i = 1; i < (int)acf.size() - 1; i++) {
        if (acf[i] > acf[i-1] && acf[i] > acf[i+1] && acf[i] > 0.3f) {
            peaks.push_back(i + 1);  // Lag = index + 1
        }
    }

    // Should find peak near the true period
    bool found_period = false;
    for (int peak : peaks) {
        if (std::abs(peak - period) <= 2) {
            found_period = true;
            break;
        }
    }

    EXPECT_TRUE(found_period);

    double elapsed = STOP_TIMER_MS();
    EXPECT_LT(elapsed, 2000.0);

    std::cout << "Seasonality: true period=" << period
              << ", detected peaks=";
    for (int p : peaks) std::cout << p << " ";
    std::cout << ", time=" << elapsed << " ms" << std::endl;
}

//=============================================================================
// E2E Test 9: Trend Extraction
//=============================================================================

TEST_F(TimeSeriesE2ETest, TrendExtraction) {
    START_TIMER();

    // Generate data with linear trend
    const int n = 300;
    const float true_slope = 0.5f;
    const float true_intercept = 10.0f;

    std::vector<float> data(n), x_values(n);
    for (int i = 0; i < n; i++) {
        x_values[i] = static_cast<float>(i);
        data[i] = true_intercept + true_slope * i + generate_normal(1, 0.0f, 5.0f)[0];
    }

    // Linear regression for trend
    nimcp_regression_result_t reg_result;
    EXPECT_EQ(nimcp_stats_regression_linear(x_values.data(), data.data(), n, &reg_result), NIMCP_STATS_OK);

    // Estimates should be close to true values
    EXPECT_NEAR(reg_result.slope, true_slope, 0.1f);
    EXPECT_NEAR(reg_result.intercept, true_intercept, 5.0f);

    // R-squared should be high (strong linear trend)
    EXPECT_GT(reg_result.r_squared, 0.8f);

    // Compute detrended data
    std::vector<float> detrended(n);
    for (int i = 0; i < n; i++) {
        float trend = reg_result.intercept + reg_result.slope * i;
        detrended[i] = data[i] - trend;
    }

    // Detrended data should have no trend
    nimcp_regression_result_t detrend_reg;
    nimcp_stats_regression_linear(x_values.data(), detrended.data(), n, &detrend_reg);
    EXPECT_NEAR(detrend_reg.slope, 0.0f, 0.05f);

    nimcp_stats_regression_free(&reg_result);
    nimcp_stats_regression_free(&detrend_reg);

    double elapsed = STOP_TIMER_MS();
    EXPECT_LT(elapsed, 2000.0);

    std::cout << "Trend Extraction: true slope=" << true_slope
              << ", estimated=" << reg_result.slope
              << ", R2=" << reg_result.r_squared
              << ", time=" << elapsed << " ms" << std::endl;
}

//=============================================================================
// E2E Test 10: Cointegration Testing (Simplified)
//=============================================================================

TEST_F(TimeSeriesE2ETest, CointegrationTesting) {
    START_TIMER();

    // Generate cointegrated pair
    const int n = 500;

    // Common stochastic trend
    std::vector<float> trend(n);
    trend[0] = 0.0f;
    for (int i = 1; i < n; i++) {
        trend[i] = trend[i-1] + generate_normal(1, 0.0f, 1.0f)[0];
    }

    // Cointegrated series
    std::vector<float> y1(n), y2(n);
    const float beta = 1.5f;  // Cointegration coefficient

    for (int i = 0; i < n; i++) {
        y1[i] = trend[i] + generate_normal(1, 0.0f, 0.5f)[0];
        y2[i] = beta * trend[i] + generate_normal(1, 0.0f, 0.5f)[0];
    }

    // Estimate cointegration coefficient via regression
    nimcp_regression_result_t reg;
    nimcp_stats_regression_linear(y1.data(), y2.data(), n, &reg);

    EXPECT_NEAR(reg.slope, beta, 0.3f);

    // Compute spread (should be stationary)
    std::vector<float> spread(n);
    for (int i = 0; i < n; i++) {
        spread[i] = y2[i] - reg.slope * y1[i] - reg.intercept;
    }

    // Spread should have lower variance than original series
    float var_spread = nimcp_stats_variance(spread.data(), n);
    float var_y1 = nimcp_stats_variance(y1.data(), n);
    float var_y2 = nimcp_stats_variance(y2.data(), n);

    EXPECT_LT(var_spread, var_y1);
    EXPECT_LT(var_spread, var_y2);

    // Spread should have low autocorrelation (mean-reverting)
    float spread_acf1 = autocorrelation(spread, 1);
    float y1_acf1 = autocorrelation(y1, 1);

    EXPECT_LT(std::abs(spread_acf1), y1_acf1);

    nimcp_stats_regression_free(&reg);

    double elapsed = STOP_TIMER_MS();
    EXPECT_LT(elapsed, 3000.0);

    std::cout << "Cointegration: beta=" << beta
              << ", estimated=" << reg.slope
              << ", spread_var=" << var_spread
              << ", time=" << elapsed << " ms" << std::endl;
}

//=============================================================================
// E2E Test 11: GARCH Volatility Modeling (Simplified)
//=============================================================================

TEST_F(TimeSeriesE2ETest, GARCHVolatilityModeling) {
    START_TIMER();

    // Generate GARCH(1,1)-like process
    const int n = 1000;
    const float omega = 0.01f;
    const float alpha = 0.1f;
    const float beta = 0.85f;

    std::vector<float> returns(n);
    std::vector<float> sigma_sq(n);

    sigma_sq[0] = omega / (1 - alpha - beta);  // Unconditional variance
    returns[0] = std::sqrt(sigma_sq[0]) * generate_normal(1, 0.0f, 1.0f)[0];

    for (int i = 1; i < n; i++) {
        sigma_sq[i] = omega + alpha * returns[i-1] * returns[i-1] + beta * sigma_sq[i-1];
        returns[i] = std::sqrt(sigma_sq[i]) * generate_normal(1, 0.0f, 1.0f)[0];
    }

    // Analyze squared returns (proxy for variance)
    std::vector<float> sq_returns(n);
    for (int i = 0; i < n; i++) {
        sq_returns[i] = returns[i] * returns[i];
    }

    // Squared returns should have positive autocorrelation (volatility clustering)
    float sq_acf1 = autocorrelation(sq_returns, 1);
    EXPECT_GT(sq_acf1, 0.1f);  // Volatility persistence

    // Returns should have low autocorrelation
    float ret_acf1 = autocorrelation(returns, 1);
    EXPECT_LT(std::abs(ret_acf1), 0.1f);

    // Returns should have excess kurtosis (fat tails)
    float kurtosis = nimcp_stats_kurtosis(returns.data(), n);
    EXPECT_GT(kurtosis, 0.0f);

    double elapsed = STOP_TIMER_MS();
    EXPECT_LT(elapsed, 2000.0);

    std::cout << "GARCH: sq_returns ACF(1)=" << sq_acf1
              << ", returns ACF(1)=" << ret_acf1
              << ", kurtosis=" << kurtosis
              << ", time=" << elapsed << " ms" << std::endl;
}

//=============================================================================
// E2E Test 12: Portfolio Statistics
//=============================================================================

TEST_F(TimeSeriesE2ETest, PortfolioStatistics) {
    START_TIMER();

    // Generate returns for multiple assets
    const int n = 252;  // 1 year
    const int n_assets = 5;

    std::vector<std::vector<float>> asset_returns(n_assets);
    std::vector<float> expected_returns = {0.08f, 0.12f, 0.06f, 0.15f, 0.10f};  // Annual
    std::vector<float> volatilities = {0.15f, 0.25f, 0.10f, 0.30f, 0.20f};  // Annual

    for (int a = 0; a < n_assets; a++) {
        float daily_ret = expected_returns[a] / 252;
        float daily_vol = volatilities[a] / std::sqrt(252.0f);
        asset_returns[a] = generate_normal(n, daily_ret, daily_vol);
    }

    // Equal-weight portfolio
    std::vector<float> weights(n_assets, 1.0f / n_assets);

    std::vector<float> portfolio_returns(n, 0.0f);
    for (int t = 0; t < n; t++) {
        for (int a = 0; a < n_assets; a++) {
            portfolio_returns[t] += weights[a] * asset_returns[a][t];
        }
    }

    // Portfolio statistics
    float port_mean = nimcp_stats_mean(portfolio_returns.data(), n);
    float port_vol = nimcp_stats_std_dev(portfolio_returns.data(), n);

    // Annualize
    float annual_return = port_mean * 252;
    float annual_vol = port_vol * std::sqrt(252.0f);

    // Sharpe ratio (assuming rf=0)
    float sharpe = annual_return / (annual_vol + 1e-6f);

    // Portfolio volatility should be lower than average individual (diversification)
    float avg_ind_vol = 0.0f;
    for (float v : volatilities) avg_ind_vol += v;
    avg_ind_vol /= n_assets;

    EXPECT_LT(annual_vol, avg_ind_vol);

    // Compute correlation matrix
    std::vector<float> corr_matrix(n_assets * n_assets);
    for (int i = 0; i < n_assets; i++) {
        for (int j = 0; j < n_assets; j++) {
            if (i == j) {
                corr_matrix[i * n_assets + j] = 1.0f;
            } else {
                nimcp_correlation_result_t corr;
                nimcp_stats_correlation_pearson(
                    asset_returns[i].data(), asset_returns[j].data(), n, &corr);
                corr_matrix[i * n_assets + j] = corr.r;
            }
        }
    }

    double elapsed = STOP_TIMER_MS();
    EXPECT_LT(elapsed, 3000.0);

    std::cout << "Portfolio: annual return=" << annual_return * 100 << "%, "
              << "annual vol=" << annual_vol * 100 << "%, "
              << "Sharpe=" << sharpe
              << ", time=" << elapsed << " ms" << std::endl;
}

//=============================================================================
// E2E Test 13: Drawdown Analysis
//=============================================================================

TEST_F(TimeSeriesE2ETest, DrawdownAnalysis) {
    START_TIMER();

    // Generate equity curve
    const int n = 500;
    auto prices = generate_random_walk(n, 100.0f, 0.0002f, 0.015f);

    // Compute drawdowns
    std::vector<float> drawdowns(n);
    float running_max = prices[0];

    for (int i = 0; i < n; i++) {
        running_max = std::max(running_max, prices[i]);
        drawdowns[i] = (prices[i] - running_max) / running_max;
    }

    // Maximum drawdown
    float max_drawdown = *std::min_element(drawdowns.begin(), drawdowns.end());

    // Average drawdown
    float avg_drawdown = nimcp_stats_mean(drawdowns.data(), n);

    // Drawdown duration analysis
    int max_duration = 0;
    int current_duration = 0;
    for (int i = 0; i < n; i++) {
        if (drawdowns[i] < 0) {
            current_duration++;
            max_duration = std::max(max_duration, current_duration);
        } else {
            current_duration = 0;
        }
    }

    // Verify drawdown properties
    EXPECT_LE(max_drawdown, 0.0f);
    EXPECT_LE(avg_drawdown, 0.0f);
    EXPECT_GE(max_drawdown, -1.0f);  // Can't lose more than 100%

    double elapsed = STOP_TIMER_MS();
    EXPECT_LT(elapsed, 2000.0);

    std::cout << "Drawdown: max=" << max_drawdown * 100 << "%, "
              << "avg=" << avg_drawdown * 100 << "%, "
              << "max duration=" << max_duration << " periods, "
              << "time=" << elapsed << " ms" << std::endl;
}

//=============================================================================
// E2E Test 14: Moving Average Crossovers
//=============================================================================

TEST_F(TimeSeriesE2ETest, MovingAverageCrossovers) {
    START_TIMER();

    // Generate trending price series
    const int n = 300;
    std::vector<float> prices(n);
    prices[0] = 100.0f;

    // Add trend with mean reversion
    for (int i = 1; i < n; i++) {
        float trend = (i < n/2) ? 0.002f : -0.001f;  // Up then down
        prices[i] = prices[i-1] * (1 + trend + 0.01f * generate_normal(1, 0, 1)[0]);
    }

    // Compute moving averages
    const int short_window = 10;
    const int long_window = 30;

    auto ma_short = rolling_mean(prices, short_window);
    auto ma_long = rolling_mean(prices, long_window);

    // Align to same length
    int offset = long_window - short_window;
    std::vector<float> ma_short_aligned(ma_short.begin() + offset, ma_short.end());

    // Count crossovers
    int bullish_cross = 0;
    int bearish_cross = 0;

    for (size_t i = 1; i < ma_long.size(); i++) {
        bool prev_above = ma_short_aligned[i-1] > ma_long[i-1];
        bool curr_above = ma_short_aligned[i] > ma_long[i];

        if (!prev_above && curr_above) bullish_cross++;
        if (prev_above && !curr_above) bearish_cross++;
    }

    // Should have some crossovers due to trend changes
    EXPECT_GT(bullish_cross + bearish_cross, 0);

    // Backtest simple strategy
    float position = 0.0f;  // 0 = out, 1 = in
    float capital = 10000.0f;

    for (size_t i = 1; i < ma_long.size(); i++) {
        int price_idx = long_window - 1 + i;

        bool prev_above = ma_short_aligned[i-1] > ma_long[i-1];
        bool curr_above = ma_short_aligned[i] > ma_long[i];

        if (!prev_above && curr_above) position = 1.0f;  // Buy
        if (prev_above && !curr_above) position = 0.0f;  // Sell

        if (position > 0 && price_idx < n - 1) {
            capital *= prices[price_idx + 1] / prices[price_idx];
        }
    }

    double elapsed = STOP_TIMER_MS();
    EXPECT_LT(elapsed, 2000.0);

    std::cout << "MA Crossover: bullish=" << bullish_cross
              << ", bearish=" << bearish_cross
              << ", final capital=" << capital
              << ", time=" << elapsed << " ms" << std::endl;
}

//=============================================================================
// E2E Test 15: Mean Reversion Testing
//=============================================================================

TEST_F(TimeSeriesE2ETest, MeanReversionTesting) {
    START_TIMER();

    // Generate mean-reverting process (Ornstein-Uhlenbeck)
    const int n = 500;
    const float theta = 0.1f;  // Mean reversion speed
    const float mu = 10.0f;    // Long-term mean
    const float sigma = 0.5f;  // Volatility

    std::vector<float> ou_process(n);
    ou_process[0] = mu;

    for (int i = 1; i < n; i++) {
        ou_process[i] = ou_process[i-1] + theta * (mu - ou_process[i-1]) +
                        sigma * generate_normal(1, 0.0f, 1.0f)[0];
    }

    // Test for mean reversion using ADF-like approach
    // Regress x_t - x_{t-1} on x_{t-1}
    std::vector<float> diff(n - 1);
    std::vector<float> lagged(n - 1);

    for (int i = 1; i < n; i++) {
        diff[i-1] = ou_process[i] - ou_process[i-1];
        lagged[i-1] = ou_process[i-1];
    }

    nimcp_regression_result_t reg;
    nimcp_stats_regression_linear(lagged.data(), diff.data(), n - 1, &reg);

    // Coefficient should be negative for mean reversion
    EXPECT_LT(reg.slope, 0.0f);

    // Estimated theta from regression
    float estimated_theta = -reg.slope;
    EXPECT_NEAR(estimated_theta, theta, theta);

    // Half-life of mean reversion
    float half_life = std::log(2) / (estimated_theta + 1e-6f);
    EXPECT_GT(half_life, 0.0f);

    // Process should stay near mean
    float mean_process = nimcp_stats_mean(ou_process.data(), n);
    EXPECT_NEAR(mean_process, mu, 2.0f);

    nimcp_stats_regression_free(&reg);

    double elapsed = STOP_TIMER_MS();
    EXPECT_LT(elapsed, 2000.0);

    std::cout << "Mean Reversion: true theta=" << theta
              << ", estimated=" << estimated_theta
              << ", half-life=" << half_life
              << ", process mean=" << mean_process
              << ", time=" << elapsed << " ms" << std::endl;
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
