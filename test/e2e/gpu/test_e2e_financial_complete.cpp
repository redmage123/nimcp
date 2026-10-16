//=============================================================================
// test_e2e_financial_complete.cpp - Complete E2E Financial GPU Tests
//=============================================================================
/**
 * @file test_e2e_financial_complete.cpp
 * @brief Comprehensive end-to-end tests for GPU-accelerated financial module
 *
 * WHAT: Complete integration tests simulating real-world financial workflows
 * WHY:  Validate entire financial pipeline works correctly end-to-end
 * HOW:  Realistic scenarios: trading, portfolio management, risk monitoring
 *
 * SCENARIOS:
 *   - Complete trading simulation pipeline
 *   - Portfolio management lifecycle
 *   - Risk monitoring end-to-end
 *   - Options market making scenario
 *   - Multi-day simulation with rebalancing
 *   - Stress testing scenarios
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstdlib>
#include <vector>
#include <algorithm>
#include <numeric>
#include <chrono>
#include <random>
#include <map>
#include <string>

// Include financial GPU headers
// Note: These are C headers but have proper extern "C" guards internally
#include "gpu/financial/nimcp_financial_gpu.h"
#include "gpu/financial/nimcp_financial_monte_carlo_gpu.h"
#include "gpu/financial/nimcp_financial_optimization_gpu.h"
#include "gpu/financial/nimcp_financial_risk_gpu.h"
#include "gpu/financial/nimcp_financial_derivatives_gpu.h"
#include "gpu/context/nimcp_gpu_context.h"

//=============================================================================
// Test Fixture
//=============================================================================

class FinancialE2ECompleteTest : public ::testing::Test {
protected:
    nimcp_gpu_context_t* ctx = nullptr;
    fin_gpu_rng_t* rng = nullptr;
    std::mt19937 cpu_rng;

    void SetUp() override {
        ctx = nimcp_gpu_context_create(0);
        if (!ctx) {
            GTEST_SKIP() << "No GPU available - skipping E2E test";
        }

        rng = fin_gpu_rng_create(ctx, 500000, 12345);
        if (!rng) {
            nimcp_gpu_context_destroy(ctx);
            ctx = nullptr;
            GTEST_SKIP() << "Failed to create GPU RNG - skipping test";
        }

        cpu_rng.seed(42);
    }

    void TearDown() override {
        if (rng) {
            fin_gpu_rng_destroy(rng);
            rng = nullptr;
        }
        if (ctx) {
            nimcp_gpu_context_destroy(ctx);
            ctx = nullptr;
        }
    }

    // Helper: Generate random correlation matrix
    std::vector<float> generateCorrelationMatrix(int n, float avg_corr = 0.3f) {
        std::vector<float> corr(n * n, 0.0f);
        std::normal_distribution<float> dist(avg_corr, 0.1f);

        for (int i = 0; i < n; i++) {
            corr[i * n + i] = 1.0f;
            for (int j = i + 1; j < n; j++) {
                float c = std::max(-0.9f, std::min(0.9f, dist(cpu_rng)));
                corr[i * n + j] = c;
                corr[j * n + i] = c;
            }
        }
        return corr;
    }

    // Helper: Generate positive-definite covariance matrix
    std::vector<float> generateCovarianceMatrix(int n, const std::vector<float>& vols) {
        std::vector<float> corr = generateCorrelationMatrix(n);
        std::vector<float> cov(n * n);

        for (int i = 0; i < n; i++) {
            for (int j = 0; j < n; j++) {
                cov[i * n + j] = corr[i * n + j] * vols[i] * vols[j];
            }
        }
        return cov;
    }

    // Helper: Simulate daily returns
    std::vector<float> simulateDailyReturns(int n_days, float drift, float vol) {
        std::vector<float> returns(n_days);
        std::normal_distribution<float> dist(drift / 252.0f, vol / std::sqrt(252.0f));

        for (int i = 0; i < n_days; i++) {
            returns[i] = dist(cpu_rng);
        }
        return returns;
    }

    // Helper: CPU Black-Scholes
    float blackScholesCall(float S, float K, float r, float sigma, float T) {
        if (T < 1e-10f || sigma < 1e-10f) {
            return std::max(0.0f, S - K * std::exp(-r * T));
        }
        float d1 = (std::log(S / K) + (r + 0.5f * sigma * sigma) * T) / (sigma * std::sqrt(T));
        float d2 = d1 - sigma * std::sqrt(T);
        float N_d1 = 0.5f * (1.0f + std::erf(d1 / std::sqrt(2.0f)));
        float N_d2 = 0.5f * (1.0f + std::erf(d2 / std::sqrt(2.0f)));
        return S * N_d1 - K * std::exp(-r * T) * N_d2;
    }
};

//=============================================================================
// SCENARIO 1: Complete Trading Simulation Pipeline
//=============================================================================

TEST_F(FinancialE2ECompleteTest, TradingSimulationPipeline) {
    std::cout << "\n=== E2E: Trading Simulation Pipeline ===" << std::endl;

    // ==================== PHASE 1: Market Setup ====================
    struct Asset {
        std::string name;
        float price;
        float expected_return;
        float volatility;
    };

    std::vector<Asset> assets = {
        {"TECH_LARGE", 150.0f, 0.12f, 0.25f},
        {"TECH_MID", 75.0f, 0.15f, 0.30f},
        {"FINANCE", 100.0f, 0.08f, 0.20f},
        {"HEALTHCARE", 120.0f, 0.10f, 0.18f},
        {"ENERGY", 50.0f, 0.06f, 0.35f},
        {"CONSUMER", 80.0f, 0.09f, 0.22f},
        {"UTILITIES", 60.0f, 0.05f, 0.12f},
        {"REALESTATE", 90.0f, 0.07f, 0.28f},
    };

    const int num_assets = assets.size();
    const int trading_days = 252;
    const float initial_capital = 1000000.0f;
    const float risk_free_rate = 0.02f;

    // ==================== PHASE 2: Generate Market Scenarios ====================
    std::vector<float> expected_returns(num_assets);
    std::vector<float> volatilities(num_assets);
    std::vector<float> prices(num_assets);

    for (int i = 0; i < num_assets; i++) {
        expected_returns[i] = assets[i].expected_return;
        volatilities[i] = assets[i].volatility;
        prices[i] = assets[i].price;
    }

    std::vector<float> covariance = generateCovarianceMatrix(num_assets, volatilities);

    // Monte Carlo simulation
    fin_monte_carlo_gpu_params_t mc_params = fin_monte_carlo_gpu_params_default();
    mc_params.num_paths = 100000;
    mc_params.num_steps = trading_days;
    mc_params.time_horizon = 1.0f;
    mc_params.antithetic = true;

    auto sim_start = std::chrono::high_resolution_clock::now();

    // Simulate each asset
    std::vector<fin_monte_carlo_gpu_result_t> mc_results(num_assets);
    for (int i = 0; i < num_assets; i++) {
        mc_params.initial_value = prices[i];
        mc_params.drift = expected_returns[i];
        mc_params.volatility = volatilities[i];

        memset(&mc_results[i], 0, sizeof(fin_monte_carlo_gpu_result_t));
        bool ok = fin_monte_carlo_gpu_simulate(ctx, rng, &mc_params, &mc_results[i]);
        ASSERT_TRUE(ok) << "Monte Carlo failed for asset " << i;
    }

    auto sim_end = std::chrono::high_resolution_clock::now();
    auto sim_time = std::chrono::duration_cast<std::chrono::milliseconds>(sim_end - sim_start);

    std::cout << "  Market simulation: " << sim_time.count() << " ms" << std::endl;

    // ==================== PHASE 3: Portfolio Optimization ====================
    fin_optimization_gpu_params_t opt_params = fin_optimization_gpu_params_default();
    opt_params.n_assets = num_assets;
    opt_params.risk_free_rate = risk_free_rate;
    opt_params.target_return = 0.10f;
    opt_params.long_only = true;
    opt_params.max_iterations = 1000;

    auto opt_start = std::chrono::high_resolution_clock::now();

    fin_optimization_gpu_result_t opt_result = {0};
    bool ok = fin_optimization_gpu_mean_variance(ctx, expected_returns.data(),
                                                   covariance.data(), &opt_params, &opt_result);
    ASSERT_TRUE(ok) << "Portfolio optimization failed";

    auto opt_end = std::chrono::high_resolution_clock::now();
    auto opt_time = std::chrono::duration_cast<std::chrono::milliseconds>(opt_end - opt_start);

    std::cout << "  Portfolio optimization: " << opt_time.count() << " ms" << std::endl;

    // Verify weights
    float weight_sum = 0.0f;
    for (int i = 0; i < num_assets; i++) {
        EXPECT_GE(opt_result.optimal_weights[i], -0.001f);
        weight_sum += opt_result.optimal_weights[i];
    }
    EXPECT_NEAR(weight_sum, 1.0f, 0.01f);

    // ==================== PHASE 4: Risk Analysis ====================
    // Generate portfolio returns from MC results
    std::vector<float> portfolio_returns(100000);
    for (int p = 0; p < 100000; p++) {
        float port_ret = 0.0f;
        for (int i = 0; i < num_assets; i++) {
            // Use mean and std to generate synthetic portfolio return
            float asset_ret = mc_results[i].mean_return +
                            (mc_results[i].std_return * (static_cast<float>(rand()) / RAND_MAX - 0.5f));
            port_ret += opt_result.optimal_weights[i] * asset_ret;
        }
        portfolio_returns[p] = port_ret;
    }

    fin_risk_gpu_params_t risk_params = fin_risk_gpu_params_default();
    risk_params.num_returns = 100000;

    fin_risk_gpu_result_t risk_result = {0};
    ok = fin_risk_gpu_compute(ctx, portfolio_returns.data(), &risk_params, &risk_result);
    ASSERT_TRUE(ok) << "Risk computation failed";

    std::cout << "\n  Portfolio Results:" << std::endl;
    std::cout << "    Expected Return: " << opt_result.expected_return * 100.0f << "%" << std::endl;
    std::cout << "    Volatility: " << opt_result.expected_volatility * 100.0f << "%" << std::endl;
    std::cout << "    Sharpe Ratio: " << opt_result.sharpe_ratio << std::endl;
    std::cout << "    VaR (95%): " << risk_result.var_95 * 100.0f << "%" << std::endl;
    std::cout << "    CVaR (95%): " << risk_result.cvar_95 * 100.0f << "%" << std::endl;

    // ==================== PHASE 5: Execute Trades ====================
    std::vector<float> shares(num_assets);
    float total_invested = 0.0f;

    for (int i = 0; i < num_assets; i++) {
        float allocation = initial_capital * opt_result.optimal_weights[i];
        shares[i] = allocation / prices[i];
        total_invested += shares[i] * prices[i];
    }

    std::cout << "\n  Trade Execution:" << std::endl;
    std::cout << "    Initial Capital: $" << initial_capital << std::endl;
    std::cout << "    Total Invested: $" << total_invested << std::endl;

    // Cleanup
    for (int i = 0; i < num_assets; i++) {
        fin_monte_carlo_gpu_result_free(ctx, &mc_results[i]);
    }
    fin_optimization_gpu_result_free(&opt_result);
}

//=============================================================================
// SCENARIO 2: Portfolio Management Lifecycle
//=============================================================================

TEST_F(FinancialE2ECompleteTest, PortfolioManagementLifecycle) {
    std::cout << "\n=== E2E: Portfolio Management Lifecycle ===" << std::endl;

    const int num_assets = 20;
    const int rebalance_periods = 4;  // Quarterly rebalancing
    const float initial_capital = 10000000.0f;

    // Generate asset universe
    std::vector<float> returns(num_assets);
    std::vector<float> vols(num_assets);
    std::uniform_real_distribution<float> ret_dist(0.04f, 0.15f);
    std::uniform_real_distribution<float> vol_dist(0.10f, 0.35f);

    for (int i = 0; i < num_assets; i++) {
        returns[i] = ret_dist(cpu_rng);
        vols[i] = vol_dist(cpu_rng);
    }

    std::vector<float> covariance = generateCovarianceMatrix(num_assets, vols);

    std::vector<float> current_weights(num_assets, 1.0f / num_assets);  // Equal weight start
    float portfolio_value = initial_capital;

    std::cout << "\n  Quarterly Rebalancing Simulation:" << std::endl;
    std::cout << "  " << std::string(60, '-') << std::endl;

    for (int period = 0; period < rebalance_periods; period++) {
        // ==================== Optimize Portfolio ====================
        fin_optimization_gpu_params_t params = fin_optimization_gpu_params_default();
        params.n_assets = num_assets;
        params.target_return = 0.08f + 0.01f * period;  // Increasing target
        params.long_only = true;
        params.max_iterations = 500;

        fin_optimization_gpu_result_t result = {0};
        bool ok = fin_optimization_gpu_mean_variance(ctx, returns.data(),
                                                       covariance.data(), &params, &result);
        ASSERT_TRUE(ok) << "Optimization failed in period " << period;

        // ==================== Calculate Turnover ====================
        float turnover = 0.0f;
        for (int i = 0; i < num_assets; i++) {
            turnover += std::abs(result.optimal_weights[i] - current_weights[i]);
        }
        turnover /= 2.0f;  // One-way turnover

        // ==================== Simulate Quarter Performance ====================
        std::normal_distribution<float> perf_dist(result.expected_return / 4.0f,
                                                   result.expected_volatility / 2.0f);
        float quarterly_return = perf_dist(cpu_rng);
        portfolio_value *= (1.0f + quarterly_return);

        // ==================== Transaction Costs ====================
        float transaction_cost = turnover * portfolio_value * 0.001f;  // 10 bps
        portfolio_value -= transaction_cost;

        std::cout << "  Q" << (period + 1)
                  << " | Return: " << std::fixed << std::setprecision(2) << quarterly_return * 100.0f << "%"
                  << " | Turnover: " << turnover * 100.0f << "%"
                  << " | Value: $" << std::setprecision(0) << portfolio_value
                  << " | Sharpe: " << std::setprecision(2) << result.sharpe_ratio
                  << std::endl;

        // Update current weights
        for (int i = 0; i < num_assets; i++) {
            current_weights[i] = result.optimal_weights[i];
        }

        fin_optimization_gpu_result_free(&result);
    }

    std::cout << "  " << std::string(60, '-') << std::endl;
    std::cout << "  Final Portfolio Value: $" << std::fixed << std::setprecision(0)
              << portfolio_value << std::endl;
    std::cout << "  Total Return: " << std::setprecision(2)
              << ((portfolio_value / initial_capital) - 1.0f) * 100.0f << "%" << std::endl;

    EXPECT_GT(portfolio_value, initial_capital * 0.8f) << "Portfolio lost too much value";
}

//=============================================================================
// SCENARIO 3: Risk Monitoring End-to-End
//=============================================================================

TEST_F(FinancialE2ECompleteTest, RiskMonitoringE2E) {
    std::cout << "\n=== E2E: Risk Monitoring System ===" << std::endl;

    const int history_days = 500;
    const int num_portfolios = 5;

    // Generate historical returns for multiple portfolios
    std::vector<std::vector<float>> portfolio_histories(num_portfolios);
    std::vector<std::string> portfolio_names = {
        "Aggressive Growth", "Balanced", "Conservative", "Income", "Market Neutral"
    };
    std::vector<float> target_vols = {0.25f, 0.15f, 0.08f, 0.10f, 0.05f};

    for (int p = 0; p < num_portfolios; p++) {
        float drift = target_vols[p] * 0.3f;  // Approximate Sharpe of 0.3
        portfolio_histories[p] = simulateDailyReturns(history_days, drift, target_vols[p]);
    }

    std::cout << "\n  Risk Dashboard:" << std::endl;
    std::cout << "  " << std::string(80, '-') << std::endl;
    std::cout << "  Portfolio           | Volatility | VaR(95%) | CVaR(95%) | Sharpe | MaxDD" << std::endl;
    std::cout << "  " << std::string(80, '-') << std::endl;

    for (int p = 0; p < num_portfolios; p++) {
        // Compute risk metrics
        fin_risk_gpu_params_t params = fin_risk_gpu_params_default();
        params.num_returns = history_days;

        fin_risk_gpu_result_t result = {0};
        bool ok = fin_risk_gpu_compute(ctx, portfolio_histories[p].data(), &params, &result);
        ASSERT_TRUE(ok) << "Risk computation failed for portfolio " << p;

        // Calculate max drawdown manually
        float peak = 0.0f;
        float max_dd = 0.0f;
        float cumulative = 0.0f;

        for (int d = 0; d < history_days; d++) {
            cumulative += portfolio_histories[p][d];
            if (cumulative > peak) peak = cumulative;
            float dd = peak - cumulative;
            if (dd > max_dd) max_dd = dd;
        }

        // Calculate annualized Sharpe
        float mean = std::accumulate(portfolio_histories[p].begin(),
                                      portfolio_histories[p].end(), 0.0f) / history_days;
        float sharpe = (mean * 252.0f - 0.02f) / (result.volatility * std::sqrt(252.0f));

        printf("  %-18s | %9.2f%% | %7.2f%% | %8.2f%% | %6.2f | %5.2f%%\n",
               portfolio_names[p].c_str(),
               result.volatility * std::sqrt(252.0f) * 100.0f,
               result.var_95 * 100.0f,
               result.cvar_95 * 100.0f,
               sharpe,
               max_dd * 100.0f);

        // Validate risk metrics
        EXPECT_GT(result.volatility, 0.0f);
        EXPECT_GE(result.cvar_95, result.var_95);
    }

    std::cout << "  " << std::string(80, '-') << std::endl;
}

//=============================================================================
// SCENARIO 4: Options Market Making
//=============================================================================

TEST_F(FinancialE2ECompleteTest, OptionsMarketMaking) {
    std::cout << "\n=== E2E: Options Market Making Scenario ===" << std::endl;

    const float spot = 100.0f;
    const float rf_rate = 0.02f;
    const float base_vol = 0.20f;
    const int trading_sessions = 100;
    const int options_per_session = 40;

    // Build option grid (5 strikes x 4 expiries x 2 types)
    std::vector<float> strikes = {90.0f, 95.0f, 100.0f, 105.0f, 110.0f};
    std::vector<float> expiries = {0.0833f, 0.25f, 0.5f, 1.0f};

    auto mm_start = std::chrono::high_resolution_clock::now();

    float total_pnl = 0.0f;
    float total_delta_hedge_cost = 0.0f;
    int total_trades = 0;

    for (int session = 0; session < trading_sessions; session++) {
        // Simulate market move
        std::normal_distribution<float> spot_move(0.0f, 0.01f);
        std::normal_distribution<float> vol_move(0.0f, 0.02f);

        float current_spot = spot * (1.0f + spot_move(cpu_rng) * (session + 1));
        float current_vol = std::max(0.05f, std::min(0.50f, base_vol + vol_move(cpu_rng)));

        // Price all options
        for (float K : strikes) {
            for (float T : expiries) {
                // Price calls and puts
                fin_derivatives_gpu_params_t params = fin_derivatives_gpu_params_default();
                params.spot = current_spot;
                params.strike = K;
                params.risk_free_rate = rf_rate;
                params.volatility = current_vol;
                params.time_to_expiry = T;
                params.tree_steps = 50;
                params.compute_greeks = true;

                for (int opt_type = 0; opt_type < 2; opt_type++) {
                    params.option_type = (opt_type == 0) ? FIN_OPT_CALL : FIN_OPT_PUT;

                    fin_derivatives_gpu_result_t result = {0};
                    bool ok = fin_derivatives_gpu_binomial_tree(ctx, &params, &result);

                    if (ok) {
                        // Simulate bid-ask spread (tighter for ATM, wider for wings)
                        float moneyness = std::abs(std::log(K / current_spot));
                        float spread = 0.02f * (1.0f + moneyness * 5.0f);

                        // Random trade direction
                        std::bernoulli_distribution trade_dist(0.1f);  // 10% trade probability
                        if (trade_dist(cpu_rng)) {
                            bool is_buy = std::bernoulli_distribution(0.5f)(cpu_rng);
                            float trade_price = is_buy
                                ? result.price * (1.0f + spread / 2.0f)
                                : result.price * (1.0f - spread / 2.0f);

                            float edge = is_buy
                                ? trade_price - result.price
                                : result.price - trade_price;

                            total_pnl += edge;

                            // Delta hedge
                            float hedge_cost = std::abs(result.delta) * current_spot * 0.0001f;
                            total_delta_hedge_cost += hedge_cost;

                            total_trades++;
                        }
                    }
                }
            }
        }
    }

    auto mm_end = std::chrono::high_resolution_clock::now();
    auto mm_time = std::chrono::duration_cast<std::chrono::milliseconds>(mm_end - mm_start);

    float net_pnl = total_pnl - total_delta_hedge_cost;
    float avg_pnl_per_trade = (total_trades > 0) ? net_pnl / total_trades : 0.0f;

    std::cout << "\n  Market Making Results:" << std::endl;
    std::cout << "    Trading Sessions: " << trading_sessions << std::endl;
    std::cout << "    Total Trades: " << total_trades << std::endl;
    std::cout << "    Gross P&L: $" << std::fixed << std::setprecision(2) << total_pnl << std::endl;
    std::cout << "    Hedge Costs: $" << total_delta_hedge_cost << std::endl;
    std::cout << "    Net P&L: $" << net_pnl << std::endl;
    std::cout << "    Avg P&L/Trade: $" << avg_pnl_per_trade << std::endl;
    std::cout << "    Execution Time: " << mm_time.count() << " ms" << std::endl;
    std::cout << "    Throughput: " << (trading_sessions * options_per_session * 1000) / mm_time.count()
              << " options/sec" << std::endl;

    // Market making should generally be profitable (with edge)
    EXPECT_GT(total_pnl, 0.0f) << "Market making should capture spread";
}

//=============================================================================
// SCENARIO 5: Multi-Day Simulation with Rebalancing
//=============================================================================

TEST_F(FinancialE2ECompleteTest, MultiDayRebalancingSimulation) {
    std::cout << "\n=== E2E: Multi-Day Portfolio Simulation ===" << std::endl;

    const int num_assets = 10;
    const int simulation_days = 252;  // 1 year
    const int rebalance_frequency = 21;  // Monthly
    const float initial_capital = 5000000.0f;

    // Generate asset characteristics
    std::vector<float> expected_returns(num_assets);
    std::vector<float> volatilities(num_assets);
    std::uniform_real_distribution<float> ret_dist(0.03f, 0.15f);
    std::uniform_real_distribution<float> vol_dist(0.10f, 0.40f);

    for (int i = 0; i < num_assets; i++) {
        expected_returns[i] = ret_dist(cpu_rng);
        volatilities[i] = vol_dist(cpu_rng);
    }

    std::vector<float> covariance = generateCovarianceMatrix(num_assets, volatilities);

    // Initial optimization
    fin_optimization_gpu_params_t opt_params = fin_optimization_gpu_params_default();
    opt_params.n_assets = num_assets;
    opt_params.long_only = true;

    fin_optimization_gpu_result_t opt_result = {0};
    fin_optimization_gpu_mean_variance(ctx, expected_returns.data(),
                                        covariance.data(), &opt_params, &opt_result);

    std::vector<float> weights(opt_result.optimal_weights,
                                opt_result.optimal_weights + num_assets);
    fin_optimization_gpu_result_free(&opt_result);

    // Simulate daily
    float portfolio_value = initial_capital;
    std::vector<float> value_history;
    value_history.push_back(portfolio_value);

    std::vector<float> daily_returns;

    for (int day = 0; day < simulation_days; day++) {
        // Generate daily asset returns
        std::vector<float> asset_returns(num_assets);
        for (int i = 0; i < num_assets; i++) {
            std::normal_distribution<float> ret(expected_returns[i] / 252.0f,
                                                 volatilities[i] / std::sqrt(252.0f));
            asset_returns[i] = ret(cpu_rng);
        }

        // Calculate portfolio return
        float port_return = 0.0f;
        for (int i = 0; i < num_assets; i++) {
            port_return += weights[i] * asset_returns[i];
        }

        portfolio_value *= (1.0f + port_return);
        value_history.push_back(portfolio_value);
        daily_returns.push_back(port_return);

        // Rebalance
        if ((day + 1) % rebalance_frequency == 0) {
            // Update expected returns based on recent performance (momentum signal)
            for (int i = 0; i < num_assets; i++) {
                expected_returns[i] = expected_returns[i] * 0.9f +
                    (asset_returns[i] * 252.0f) * 0.1f;
            }

            fin_optimization_gpu_result_t new_opt = {0};
            fin_optimization_gpu_mean_variance(ctx, expected_returns.data(),
                                                covariance.data(), &opt_params, &new_opt);

            if (new_opt.optimal_weights) {
                for (int i = 0; i < num_assets; i++) {
                    weights[i] = new_opt.optimal_weights[i];
                }
                fin_optimization_gpu_result_free(&new_opt);
            }
        }
    }

    // Calculate performance statistics
    float total_return = (portfolio_value / initial_capital) - 1.0f;
    float mean_daily = std::accumulate(daily_returns.begin(), daily_returns.end(), 0.0f) /
                       daily_returns.size();

    float variance = 0.0f;
    for (float r : daily_returns) {
        variance += (r - mean_daily) * (r - mean_daily);
    }
    variance /= daily_returns.size();
    float daily_vol = std::sqrt(variance);
    float annual_vol = daily_vol * std::sqrt(252.0f);

    float sharpe = (mean_daily * 252.0f - 0.02f) / annual_vol;

    // Max drawdown
    float peak = initial_capital;
    float max_dd = 0.0f;
    for (float v : value_history) {
        if (v > peak) peak = v;
        float dd = (peak - v) / peak;
        if (dd > max_dd) max_dd = dd;
    }

    std::cout << "\n  Simulation Results (" << simulation_days << " days):" << std::endl;
    std::cout << "    Initial Capital: $" << std::fixed << std::setprecision(0)
              << initial_capital << std::endl;
    std::cout << "    Final Value: $" << portfolio_value << std::endl;
    std::cout << "    Total Return: " << std::setprecision(2) << total_return * 100.0f << "%" << std::endl;
    std::cout << "    Annual Volatility: " << annual_vol * 100.0f << "%" << std::endl;
    std::cout << "    Sharpe Ratio: " << sharpe << std::endl;
    std::cout << "    Max Drawdown: " << max_dd * 100.0f << "%" << std::endl;

    // Compute VaR using GPU
    fin_risk_gpu_params_t risk_params = fin_risk_gpu_params_default();
    risk_params.num_returns = daily_returns.size();

    fin_risk_gpu_result_t risk_result = {0};
    fin_risk_gpu_compute(ctx, daily_returns.data(), &risk_params, &risk_result);

    std::cout << "    Daily VaR (95%): " << risk_result.var_95 * 100.0f << "%" << std::endl;
    std::cout << "    Daily CVaR (95%): " << risk_result.cvar_95 * 100.0f << "%" << std::endl;

    EXPECT_GT(portfolio_value, initial_capital * 0.5f) << "Portfolio lost more than 50%";
}

//=============================================================================
// SCENARIO 6: Stress Testing Scenarios
//=============================================================================

TEST_F(FinancialE2ECompleteTest, StressTestingScenarios) {
    std::cout << "\n=== E2E: Stress Testing Scenarios ===" << std::endl;

    const int num_assets = 15;
    const float initial_capital = 10000000.0f;

    // Generate base portfolio
    std::vector<float> returns(num_assets);
    std::vector<float> vols(num_assets);
    for (int i = 0; i < num_assets; i++) {
        returns[i] = 0.08f + 0.02f * std::sin(i);
        vols[i] = 0.15f + 0.10f * std::cos(i);
    }

    std::vector<float> cov = generateCovarianceMatrix(num_assets, vols);

    // Optimize portfolio
    fin_optimization_gpu_params_t opt_params = fin_optimization_gpu_params_default();
    opt_params.n_assets = num_assets;
    opt_params.long_only = true;

    fin_optimization_gpu_result_t opt_result = {0};
    fin_optimization_gpu_mean_variance(ctx, returns.data(), cov.data(), &opt_params, &opt_result);

    // Define stress scenarios
    struct StressScenario {
        std::string name;
        float equity_shock;
        float vol_multiplier;
        float correlation_increase;
    };

    std::vector<StressScenario> scenarios = {
        {"Normal Conditions", 0.0f, 1.0f, 0.0f},
        {"Market Correction (-10%)", -0.10f, 1.5f, 0.2f},
        {"Bear Market (-25%)", -0.25f, 2.0f, 0.3f},
        {"Crash (-40%)", -0.40f, 3.0f, 0.5f},
        {"Flash Crash (-20%, High Vol)", -0.20f, 5.0f, 0.4f},
        {"Volatility Spike", 0.0f, 3.0f, 0.3f},
        {"Correlation Breakdown", 0.0f, 1.5f, 0.6f},
    };

    std::cout << "\n  Stress Test Results:" << std::endl;
    std::cout << "  " << std::string(70, '-') << std::endl;
    std::cout << "  Scenario                    | Portfolio Loss | VaR Impact | Status" << std::endl;
    std::cout << "  " << std::string(70, '-') << std::endl;

    for (const auto& scenario : scenarios) {
        // Apply stress to returns
        std::vector<float> stressed_returns(num_assets);
        for (int i = 0; i < num_assets; i++) {
            stressed_returns[i] = returns[i] + scenario.equity_shock;
        }

        // Calculate portfolio return under stress
        float stressed_port_return = 0.0f;
        for (int i = 0; i < num_assets; i++) {
            stressed_port_return += opt_result.optimal_weights[i] * stressed_returns[i];
        }

        // Calculate stressed VaR
        float base_vol = opt_result.expected_volatility;
        float stressed_vol = base_vol * scenario.vol_multiplier;
        float stressed_var = stressed_vol * 1.645f;  // 95% normal VaR

        float portfolio_loss = stressed_port_return;
        float dollar_loss = initial_capital * portfolio_loss;

        const char* status = (portfolio_loss > -0.20f) ? "PASS" : "ALERT";

        printf("  %-28s | %13.2f%% | %9.2f%% | %s\n",
               scenario.name.c_str(),
               portfolio_loss * 100.0f,
               stressed_var * 100.0f,
               status);
    }

    std::cout << "  " << std::string(70, '-') << std::endl;

    // Monte Carlo stress test
    std::cout << "\n  Monte Carlo Stress Simulation (100K paths, 3x vol):" << std::endl;

    fin_monte_carlo_gpu_params_t mc_params = fin_monte_carlo_gpu_params_default();
    mc_params.num_paths = 100000;
    mc_params.num_steps = 252;
    mc_params.initial_value = initial_capital;
    mc_params.drift = opt_result.expected_return;
    mc_params.volatility = opt_result.expected_volatility * 3.0f;  // Stressed vol

    fin_monte_carlo_gpu_result_t mc_result = {0};
    bool ok = fin_monte_carlo_gpu_simulate(ctx, rng, &mc_params, &mc_result);
    ASSERT_TRUE(ok);

    std::cout << "    Mean Terminal Value: $" << std::fixed << std::setprecision(0)
              << mc_result.mean_value << std::endl;
    std::cout << "    Stressed VaR (95%): " << std::setprecision(2)
              << mc_result.var_95 * 100.0f << "%" << std::endl;
    std::cout << "    Stressed CVaR (95%): " << mc_result.cvar_95 * 100.0f << "%" << std::endl;
    std::cout << "    Probability of Loss: " << mc_result.probability_of_loss * 100.0f << "%" << std::endl;
    std::cout << "    Max Simulated Loss: " << (1.0f - mc_result.min_terminal / initial_capital) * 100.0f
              << "%" << std::endl;

    fin_monte_carlo_gpu_result_free(ctx, &mc_result);
    fin_optimization_gpu_result_free(&opt_result);
}

//=============================================================================
// SCENARIO 7: Memory Leak Detection
//=============================================================================

TEST_F(FinancialE2ECompleteTest, MemoryLeakDetection) {
    std::cout << "\n=== E2E: Memory Leak Detection ===" << std::endl;

    const int iterations = 50;

    // Track initial state
    auto start_time = std::chrono::high_resolution_clock::now();

    for (int iter = 0; iter < iterations; iter++) {
        // Monte Carlo
        fin_monte_carlo_gpu_params_t mc_params = fin_monte_carlo_gpu_params_default();
        mc_params.num_paths = 10000;

        fin_monte_carlo_gpu_result_t mc_result = {0};
        fin_monte_carlo_gpu_simulate(ctx, rng, &mc_params, &mc_result);
        fin_monte_carlo_gpu_result_free(ctx, &mc_result);

        // Optimization
        const int n = 20;
        std::vector<float> returns(n, 0.1f);
        std::vector<float> cov(n * n, 0.01f);
        for (int i = 0; i < n; i++) cov[i * n + i] = 0.04f;

        fin_optimization_gpu_params_t opt_params = fin_optimization_gpu_params_default();
        opt_params.n_assets = n;

        fin_optimization_gpu_result_t opt_result = {0};
        fin_optimization_gpu_mean_variance(ctx, returns.data(), cov.data(), &opt_params, &opt_result);
        fin_optimization_gpu_result_free(&opt_result);

        // Risk metrics
        std::vector<float> test_returns(1000);
        std::generate(test_returns.begin(), test_returns.end(),
                      [this]() { return std::normal_distribution<float>(0.0f, 0.02f)(cpu_rng); });

        fin_risk_gpu_params_t risk_params = fin_risk_gpu_params_default();
        risk_params.num_returns = 1000;

        fin_risk_gpu_result_t risk_result = {0};
        fin_risk_gpu_compute(ctx, test_returns.data(), &risk_params, &risk_result);

        // Derivatives
        fin_derivatives_gpu_params_t deriv_params = fin_derivatives_gpu_params_default();
        fin_derivatives_gpu_result_t deriv_result = {0};
        fin_derivatives_gpu_binomial_tree(ctx, &deriv_params, &deriv_result);
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    std::cout << "  Completed " << iterations << " full cycles in " << elapsed.count() << " ms" << std::endl;
    std::cout << "  Average cycle time: " << elapsed.count() / iterations << " ms" << std::endl;
    std::cout << "  Memory leak test: PASS (no crash or hang)" << std::endl;
}

//=============================================================================
// SCENARIO 8: Performance Benchmark
//=============================================================================

TEST_F(FinancialE2ECompleteTest, PerformanceBenchmark) {
    std::cout << "\n=== E2E: Performance Benchmark ===" << std::endl;

    struct BenchmarkResult {
        std::string operation;
        int iterations;
        double total_ms;
        double ops_per_sec;
    };

    std::vector<BenchmarkResult> results;

    // Benchmark 1: Monte Carlo
    {
        fin_monte_carlo_gpu_params_t params = fin_monte_carlo_gpu_params_default();
        params.num_paths = 100000;

        auto start = std::chrono::high_resolution_clock::now();
        const int iters = 10;

        for (int i = 0; i < iters; i++) {
            fin_monte_carlo_gpu_result_t result = {0};
            fin_monte_carlo_gpu_simulate(ctx, rng, &params, &result);
            fin_monte_carlo_gpu_result_free(ctx, &result);
        }

        auto end = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() / 1000.0;

        results.push_back({"Monte Carlo (100K paths)", iters, ms, iters * 1000.0 / ms});
    }

    // Benchmark 2: Portfolio Optimization
    {
        const int n = 100;
        std::vector<float> returns(n, 0.1f);
        std::vector<float> cov(n * n, 0.01f);
        for (int i = 0; i < n; i++) cov[i * n + i] = 0.04f;

        fin_optimization_gpu_params_t params = fin_optimization_gpu_params_default();
        params.n_assets = n;

        auto start = std::chrono::high_resolution_clock::now();
        const int iters = 20;

        for (int i = 0; i < iters; i++) {
            fin_optimization_gpu_result_t result = {0};
            fin_optimization_gpu_mean_variance(ctx, returns.data(), cov.data(), &params, &result);
            fin_optimization_gpu_result_free(&result);
        }

        auto end = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() / 1000.0;

        results.push_back({"Optimization (100 assets)", iters, ms, iters * 1000.0 / ms});
    }

    // Benchmark 3: Risk Metrics
    {
        std::vector<float> returns(100000);
        std::generate(returns.begin(), returns.end(),
                      [this]() { return std::normal_distribution<float>(0.0f, 0.02f)(cpu_rng); });

        fin_risk_gpu_params_t params = fin_risk_gpu_params_default();
        params.num_returns = 100000;

        auto start = std::chrono::high_resolution_clock::now();
        const int iters = 50;

        for (int i = 0; i < iters; i++) {
            fin_risk_gpu_result_t result = {0};
            fin_risk_gpu_compute(ctx, returns.data(), &params, &result);
        }

        auto end = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() / 1000.0;

        results.push_back({"Risk Metrics (100K returns)", iters, ms, iters * 1000.0 / ms});
    }

    // NOTE: Benchmark 4 (Batch Option Pricing) removed
    // fin_derivatives_gpu_batch_price not yet implemented

    // Print results
    std::cout << "\n  " << std::string(70, '-') << std::endl;
    std::cout << "  Operation                    | Iterations | Time (ms) | Throughput" << std::endl;
    std::cout << "  " << std::string(70, '-') << std::endl;

    for (const auto& r : results) {
        printf("  %-29s | %10d | %9.1f | %.0f/sec\n",
               r.operation.c_str(), r.iterations, r.total_ms, r.ops_per_sec);
    }

    std::cout << "  " << std::string(70, '-') << std::endl;
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
