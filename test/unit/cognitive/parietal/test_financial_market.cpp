/**
 * @file test_financial_market.cpp
 * @brief Unit tests for NIMCP Financial Market Analysis module
 *
 * Tests GARCH fitting, technical indicators, sentiment analysis,
 * regime detection (crisp and fuzzy), scenario analysis, Monte Carlo
 * simulation, modulation, and NULL safety.
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

#include "cognitive/parietal/nimcp_financial_market.h"

namespace {

//=============================================================================
// Test Fixture
//=============================================================================

class FinancialMarketTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        mkt = financial_market_create();
        ASSERT_NE(mkt, nullptr);
    }

    void TearDown() override
    {
        if (mkt) {
            financial_market_destroy(mkt);
            mkt = nullptr;
        }
    }

    /** Helper: build a synthetic price series */
    void make_price_series(fin_time_series_t* ts, uint32_t len)
    {
        memset(ts, 0, sizeof(*ts));
        ts->length = len;
        for (uint32_t i = 0; i < len; i++) {
            // Trending upward with some noise
            ts->prices[i] = 100.0f + 0.1f * (float)i +
                            2.0f * sinf((float)i * 0.3f);
            ts->volumes[i] = 1000.0f + 100.0f * (float)(i % 10);
            ts->timestamps[i] = 1000000ULL * i;
        }
        ts->open = ts->prices[0];
        ts->close = ts->prices[len - 1];
        ts->high = ts->close + 5.0f;
        ts->low = ts->open - 3.0f;
    }

    /** Helper: build a simple price array */
    void make_prices(float* prices, uint32_t len)
    {
        for (uint32_t i = 0; i < len; i++) {
            prices[i] = 100.0f + 0.05f * (float)i +
                        1.5f * sinf((float)i * 0.2f);
        }
    }

    financial_market_eng_t* mkt = nullptr;
};

//=============================================================================
// 1. Lifecycle Tests
//=============================================================================

TEST_F(FinancialMarketTest, CreateDefault)
{
    EXPECT_NE(mkt, nullptr);
}

TEST_F(FinancialMarketTest, CreateCustom)
{
    fin_market_config_t cfg = financial_market_default_config();
    cfg.garch_max_iterations = 500;
    financial_market_eng_t* custom = financial_market_create_custom(&cfg);
    ASSERT_NE(custom, nullptr);
    financial_market_destroy(custom);
}

TEST_F(FinancialMarketTest, DefaultConfig)
{
    fin_market_config_t cfg = financial_market_default_config();
    EXPECT_GT(cfg.default_ma_period, 0u);
    EXPECT_GT(cfg.garch_max_iterations, 0u);
    EXPECT_GT(cfg.garch_convergence_tol, 0.0f);
}

TEST_F(FinancialMarketTest, DestroyNullSafe)
{
    financial_market_destroy(nullptr);
    // Should not crash
}

//=============================================================================
// 2. GARCH Tests
//=============================================================================

TEST_F(FinancialMarketTest, GARCHFit)
{
    float returns[200];
    for (int i = 0; i < 200; i++) {
        returns[i] = 0.001f * (float)(i % 20 - 10);
    }

    fin_garch_result_t result{};
    int rc = financial_market_garch_fit(mkt, returns, 200, 1, 1, &result);
    EXPECT_EQ(rc, 0);
    EXPECT_GT(result.omega, 0.0f);
    EXPECT_GT(result.current_variance, 0.0f);
}

TEST_F(FinancialMarketTest, GARCHForecast)
{
    float returns[200];
    for (int i = 0; i < 200; i++) {
        returns[i] = 0.001f * (float)(i % 20 - 10);
    }

    fin_garch_result_t garch{};
    financial_market_garch_fit(mkt, returns, 200, 1, 1, &garch);

    float forecast = financial_market_garch_forecast(&garch, 5);
    EXPECT_GT(forecast, 0.0f);
}

//=============================================================================
// 3. Technical Indicators Tests
//=============================================================================

TEST_F(FinancialMarketTest, ComputeSMA)
{
    float prices[50];
    make_prices(prices, 50);
    float out[50]{};

    int rc = financial_market_compute_sma(prices, 50, 10, out);
    EXPECT_GT(rc, 0);  // Returns output length (41) on success
    // SMA should be close to the average price level
    EXPECT_GT(out[10], 90.0f);
    EXPECT_LT(out[10], 120.0f);
}

TEST_F(FinancialMarketTest, ComputeEMA)
{
    float prices[50];
    make_prices(prices, 50);
    float out[50]{};

    int rc = financial_market_compute_ema(prices, 50, 10, out);
    EXPECT_GT(rc, 0);  // Returns output length on success
    // EMA should track price closely
    EXPECT_GT(out[40], 90.0f);  // Use valid index (out_len = 41)
}

TEST_F(FinancialMarketTest, ComputeRSI)
{
    float prices[50];
    make_prices(prices, 50);

    float rsi = financial_market_compute_rsi(prices, 50, 14);
    // RSI should be in [0, 100]
    EXPECT_GE(rsi, 0.0f);
    EXPECT_LE(rsi, 100.0f);
}

TEST_F(FinancialMarketTest, RSIOverbought)
{
    // Monotonically rising prices -> RSI high
    float prices[50];
    for (int i = 0; i < 50; i++) prices[i] = 100.0f + (float)i * 2.0f;
    float rsi = financial_market_compute_rsi(prices, 50, 14);
    EXPECT_GT(rsi, 60.0f);
}

TEST_F(FinancialMarketTest, RSIOversold)
{
    // Monotonically falling prices -> RSI low
    float prices[50];
    for (int i = 0; i < 50; i++) prices[i] = 200.0f - (float)i * 2.0f;
    float rsi = financial_market_compute_rsi(prices, 50, 14);
    EXPECT_LT(rsi, 40.0f);
}

TEST_F(FinancialMarketTest, ComputeMACD)
{
    float prices[100];
    make_prices(prices, 100);

    float macd_line[100]{};
    float signal_line[100]{};
    float histogram[100]{};

    int rc = financial_market_compute_macd(prices, 100, 12, 26, 9,
        macd_line, signal_line, histogram);
    EXPECT_GT(rc, 0);  // Returns output length on success
}

TEST_F(FinancialMarketTest, ComputeBollinger)
{
    float prices[50];
    make_prices(prices, 50);

    float upper[50]{}, middle[50]{}, lower[50]{};
    int rc = financial_market_compute_bollinger(prices, 50, 20, 2.0f,
        upper, middle, lower);
    EXPECT_GT(rc, 0);  // Returns output length (31) on success
    // Upper > middle > lower for valid index (0 to out_len-1)
    EXPECT_GT(upper[20], middle[20]);
    EXPECT_GT(middle[20], lower[20]);
}

TEST_F(FinancialMarketTest, ComputeIndicatorGeneric)
{
    fin_time_series_t ts{};
    make_price_series(&ts, 100);

    fin_indicator_result_t result{};
    int rc = financial_market_compute_indicator(mkt, &ts,
        FIN_MKT_IND_RSI, 14, &result);
    EXPECT_EQ(rc, 0);
    EXPECT_EQ(result.type, FIN_MKT_IND_RSI);
}

//=============================================================================
// 4. Sentiment Tests
//=============================================================================

TEST_F(FinancialMarketTest, AnalyzeSentiment)
{
    fin_time_series_t price_ts{}, volume_ts{};
    make_price_series(&price_ts, 100);
    make_price_series(&volume_ts, 100);

    fin_sentiment_t sentiment{};
    int rc = financial_market_analyze_sentiment(mkt, &price_ts, &volume_ts,
        &sentiment);
    EXPECT_EQ(rc, 0);
    EXPECT_GE(sentiment.fear_greed_index, 0.0f);
    EXPECT_LE(sentiment.fear_greed_index, 100.0f);
}

TEST_F(FinancialMarketTest, AnalyzeSentimentFuzzy)
{
    fin_time_series_t price_ts{}, volume_ts{};
    make_price_series(&price_ts, 100);
    make_price_series(&volume_ts, 100);

    fin_fuzzy_sentiment_t fuzzy{};
    int rc = financial_market_analyze_sentiment_fuzzy(mkt, &price_ts,
        &volume_ts, &fuzzy);
    EXPECT_EQ(rc, 0);
    // All degrees should be in [0,1]
    EXPECT_GE(fuzzy.extreme_fear_degree, 0.0f);
    EXPECT_LE(fuzzy.extreme_fear_degree, 1.0f);
    EXPECT_GE(fuzzy.greed_degree, 0.0f);
    EXPECT_LE(fuzzy.greed_degree, 1.0f);
    // Degrees should sum reasonably (allowing overlap)
    float sum = fuzzy.extreme_fear_degree + fuzzy.fear_degree +
                fuzzy.neutral_degree + fuzzy.greed_degree +
                fuzzy.extreme_greed_degree;
    EXPECT_GT(sum, 0.0f);
}

//=============================================================================
// 5. Regime Detection Tests
//=============================================================================

TEST_F(FinancialMarketTest, DetectRegimeCrisp)
{
    fin_time_series_t ts{};
    make_price_series(&ts, 100);

    fin_market_condition_t regime = financial_market_detect_regime(mkt, &ts);
    EXPECT_GE((int)regime, 0);
    EXPECT_LT((int)regime, (int)FIN_MKT_CONDITION_COUNT);
}

TEST_F(FinancialMarketTest, DetectRegimeFuzzy)
{
    fin_time_series_t ts{};
    make_price_series(&ts, 100);

    fin_fuzzy_market_condition_t condition{};
    int rc = financial_market_detect_regime_fuzzy(mkt, &ts, &condition);
    EXPECT_EQ(rc, 0);
    // Each degree in [0,1]
    EXPECT_GE(condition.bull_degree, 0.0f);
    EXPECT_LE(condition.bull_degree, 1.0f);
    EXPECT_GE(condition.bear_degree, 0.0f);
    EXPECT_LE(condition.bear_degree, 1.0f);
    EXPECT_GE(condition.sideways_degree, 0.0f);
    EXPECT_LE(condition.sideways_degree, 1.0f);
}

TEST_F(FinancialMarketTest, FuzzyRegimeDegreesSum)
{
    fin_time_series_t ts{};
    make_price_series(&ts, 100);

    fin_fuzzy_market_condition_t condition{};
    financial_market_detect_regime_fuzzy(mkt, &ts, &condition);

    float sum = condition.bull_degree + condition.bear_degree +
                condition.sideways_degree + condition.high_vol_degree +
                condition.crisis_degree + condition.recovery_degree;
    // Sum should be positive (at least one regime active)
    EXPECT_GT(sum, 0.0f);
}

//=============================================================================
// 6. Scenario & Monte Carlo Tests
//=============================================================================

TEST_F(FinancialMarketTest, RunScenario)
{
    fin_portfolio_t p{};
    memset(&p, 0, sizeof(p));
    p.asset_count = 1;
    p.assets[0].asset_id = 1;
    p.assets[0].type = FIN_ASSET_EQUITY;
    p.assets[0].current_price = 100.0f;
    p.weights[0] = 1.0f;
    p.total_value = 10000.0f;

    fin_scenario_t scenario{};
    scenario.type = FIN_MKT_SCENARIO_RECESSION;
    scenario.equity_shock = -0.30f;
    scenario.vol_shock = 0.50f;
    scenario.duration_years = 1.0f;

    fin_scenario_result_t result{};
    int rc = financial_market_run_scenario(mkt, &p, &scenario, &result);
    EXPECT_EQ(rc, 0);
    // Recession scenario should result in negative returns (pnl can be 0 if no price change)
    EXPECT_LE(result.portfolio_return, 0.0f);
}

TEST_F(FinancialMarketTest, StressTest)
{
    fin_portfolio_t p{};
    memset(&p, 0, sizeof(p));
    p.asset_count = 1;
    p.assets[0].current_price = 100.0f;
    p.weights[0] = 1.0f;
    p.total_value = 10000.0f;

    fin_scenario_t scenarios[2]{};
    scenarios[0].type = FIN_MKT_SCENARIO_RECESSION;
    scenarios[0].equity_shock = -0.30f;
    scenarios[1].type = FIN_MKT_SCENARIO_MARKET_CRASH;
    scenarios[1].equity_shock = -0.50f;

    fin_scenario_result_t results[2]{};
    int rc = financial_market_stress_test(mkt, &p, scenarios, 2, results);
    EXPECT_EQ(rc, 0);
    // Crash should be worse than recession
    EXPECT_LE(results[1].portfolio_pnl, results[0].portfolio_pnl);
}

TEST_F(FinancialMarketTest, MonteCarlo)
{
    fin_portfolio_t p{};
    memset(&p, 0, sizeof(p));
    p.asset_count = 1;
    p.assets[0].current_price = 100.0f;
    p.weights[0] = 1.0f;
    p.total_value = 10000.0f;

    fin_monte_carlo_result_t result{};
    int rc = financial_market_monte_carlo(mkt, &p,
        0.08f, 0.20f, 1.0f, 1000, &result);
    EXPECT_EQ(rc, 0);
    EXPECT_GT(result.paths_completed, 0u);
    EXPECT_GT(result.paths_std_return, 0.0f);
    EXPECT_GE(result.probability_of_loss, 0.0f);
    EXPECT_LE(result.probability_of_loss, 1.0f);
}

//=============================================================================
// 7. Modulation Tests
//=============================================================================

TEST_F(FinancialMarketTest, SetInflammation)
{
    int rc = financial_market_set_inflammation(mkt, 0.5f);
    EXPECT_EQ(rc, 0);
}

TEST_F(FinancialMarketTest, SetFatigue)
{
    int rc = financial_market_set_fatigue(mkt, 0.3f);
    EXPECT_EQ(rc, 0);
}

//=============================================================================
// 8. Statistics Tests
//=============================================================================

TEST_F(FinancialMarketTest, GetStats)
{
    fin_market_stats_t stats{};
    int rc = financial_market_get_stats(mkt, &stats);
    EXPECT_EQ(rc, 0);
}

TEST_F(FinancialMarketTest, ResetStats)
{
    financial_market_reset_stats(mkt);
    fin_market_stats_t stats{};
    financial_market_get_stats(mkt, &stats);
    EXPECT_EQ(stats.garch_fits, 0u);
}

TEST_F(FinancialMarketTest, GetLastError)
{
    const char* err = financial_market_get_last_error();
    EXPECT_NE(err, nullptr);
}

//=============================================================================
// 9. NULL Safety Tests
//=============================================================================

TEST_F(FinancialMarketTest, NullGARCHFit)
{
    int rc = financial_market_garch_fit(nullptr, nullptr, 0, 1, 1, nullptr);
    EXPECT_NE(rc, 0);
}

TEST_F(FinancialMarketTest, NullSMA)
{
    int rc = financial_market_compute_sma(nullptr, 0, 10, nullptr);
    EXPECT_NE(rc, 0);
}

TEST_F(FinancialMarketTest, NullAnalyzeSentiment)
{
    int rc = financial_market_analyze_sentiment(nullptr, nullptr, nullptr, nullptr);
    EXPECT_NE(rc, 0);
}

TEST_F(FinancialMarketTest, NullDetectRegimeFuzzy)
{
    int rc = financial_market_detect_regime_fuzzy(nullptr, nullptr, nullptr);
    EXPECT_NE(rc, 0);
}

TEST_F(FinancialMarketTest, NullRunScenario)
{
    int rc = financial_market_run_scenario(nullptr, nullptr, nullptr, nullptr);
    EXPECT_NE(rc, 0);
}

TEST_F(FinancialMarketTest, NullMonteCarlo)
{
    int rc = financial_market_monte_carlo(nullptr, nullptr, 0, 0, 0, 0, nullptr);
    EXPECT_NE(rc, 0);
}

TEST_F(FinancialMarketTest, NullSetInflammation)
{
    int rc = financial_market_set_inflammation(nullptr, 0.5f);
    EXPECT_NE(rc, 0);
}

TEST_F(FinancialMarketTest, NullGetStats)
{
    int rc = financial_market_get_stats(nullptr, nullptr);
    EXPECT_NE(rc, 0);
}

} // namespace
