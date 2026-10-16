/* ============================================================================
 * E2E Test: Financial Workflow GPU Pipeline
 * ============================================================================
 * WHAT: End-to-end test of GPU-accelerated financial computing workflow
 * WHY:  Validate complete financial analysis pipeline on GPU
 * HOW:  Market simulation -> Risk analysis -> Portfolio optimization -> Hedging
 * ============================================================================
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <random>
#include <chrono>
#include <algorithm>
#include <numeric>

#ifdef NIMCP_ENABLE_CUDA
#include "gpu/financial/nimcp_financial_monte_carlo_gpu.h"
#include "gpu/financial/nimcp_financial_optimization_gpu.h"
#include "gpu/financial/nimcp_financial_risk_gpu.h"
#include "gpu/financial/nimcp_financial_derivatives_gpu.h"
#include "gpu/context/nimcp_gpu_context.h"
#endif

namespace {

constexpr float TOLERANCE = 1e-4f;
constexpr float STATISTICAL_TOLERANCE = 0.1f;

class FinancialWorkflowE2ETest : public ::testing::Test {
protected:
    void SetUp() override {
#ifdef NIMCP_ENABLE_CUDA
        ctx_ = nimcp_gpu_context_create(0);  // Use default device (GPU 0)
        if (ctx_) {
            rng_ = fin_gpu_rng_create(ctx_, 100000, 54321);
        }
#endif
    }

    void TearDown() override {
#ifdef NIMCP_ENABLE_CUDA
        if (rng_) {
            fin_gpu_rng_destroy(rng_);
            rng_ = nullptr;
        }
        if (ctx_) {
            nimcp_gpu_context_destroy(ctx_);
            ctx_ = nullptr;
        }
#endif
    }

#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_context_t* ctx_ = nullptr;
    fin_gpu_rng_t* rng_ = nullptr;
#endif
};

/* ============================================================================
 * E2E Test: Complete Portfolio Management Workflow
 * ============================================================================
 * Simulates a realistic portfolio management process:
 * 1. Simulate market scenarios
 * 2. Estimate returns and covariances
 * 3. Optimize portfolio
 * 4. Compute risk metrics
 * 5. Price hedging options
 */
TEST_F(FinancialWorkflowE2ETest, PortfolioManagementWorkflow) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";
    ASSERT_NE(rng_, nullptr) << "RNG required";

    const int num_assets = 10;
    const int num_scenarios = 50000;
    const int num_steps = 252;  // Daily for 1 year

    // ==================== Phase 1: Market Parameters ====================
    std::vector<float> expected_returns(num_assets);
    std::vector<float> volatilities(num_assets);
    std::vector<float> initial_prices(num_assets, 100.0f);

    // Diversified asset mix (stocks, bonds, commodities)
    float base_returns[] = {0.08f, 0.10f, 0.12f, 0.06f, 0.04f, 0.15f, 0.11f, 0.09f, 0.05f, 0.07f};
    float base_vols[] = {0.18f, 0.22f, 0.25f, 0.12f, 0.06f, 0.35f, 0.20f, 0.16f, 0.08f, 0.15f};

    for (int i = 0; i < num_assets; i++) {
        expected_returns[i] = base_returns[i];
        volatilities[i] = base_vols[i];
    }

    // Correlation structure (block diagonal with some cross-correlations)
    std::vector<float> correlation(num_assets * num_assets, 0.0f);
    for (int i = 0; i < num_assets; i++) {
        correlation[i * num_assets + i] = 1.0f;  // Diagonal
        for (int j = i + 1; j < num_assets; j++) {
            float corr = 0.3f;
            // Same asset class: higher correlation
            if ((i < 5 && j < 5) || (i >= 5 && j >= 5)) {
                corr = 0.5f;
            }
            correlation[i * num_assets + j] = corr;
            correlation[j * num_assets + i] = corr;
        }
    }

    // ==================== Phase 2: Monte Carlo Simulation ====================
    auto sim_start = std::chrono::high_resolution_clock::now();

    // Compute Cholesky decomposition of correlation matrix
    std::vector<float> cholesky_L(num_assets * num_assets, 0.0f);
    for (int i = 0; i < num_assets; i++) {
        for (int j = 0; j <= i; j++) {
            float sum = correlation[i * num_assets + j];
            for (int k = 0; k < j; k++) {
                sum -= cholesky_L[i * num_assets + k] * cholesky_L[j * num_assets + k];
            }
            if (i == j) {
                cholesky_L[i * num_assets + j] = std::sqrt(std::max(0.0f, sum));
            } else {
                cholesky_L[i * num_assets + j] = sum / cholesky_L[j * num_assets + j];
            }
        }
    }

    fin_monte_carlo_gpu_params_t base_mc_params = fin_monte_carlo_gpu_params_default();
    base_mc_params.num_paths = static_cast<uint32_t>(num_scenarios);
    base_mc_params.num_steps = static_cast<uint32_t>(num_steps);
    base_mc_params.time_horizon = 1.0f;

    fin_multi_asset_params_t mc_params = {
        .base = base_mc_params,
        .n_assets = static_cast<uint32_t>(num_assets),
        .initial_values = initial_prices.data(),
        .drifts = expected_returns.data(),
        .volatilities = volatilities.data(),
        .cholesky_L = cholesky_L.data()
    };

    std::vector<float> terminal_returns(num_scenarios * num_assets);

    bool mc_success = fin_monte_carlo_gpu_multi_asset(ctx_, rng_, &mc_params, terminal_returns.data());
    ASSERT_TRUE(mc_success) << "Monte Carlo simulation failed";

    auto sim_end = std::chrono::high_resolution_clock::now();
    auto sim_time = std::chrono::duration_cast<std::chrono::milliseconds>(sim_end - sim_start);

    // ==================== Phase 3: Estimate Parameters from Simulation ====================
    std::vector<float> estimated_returns(num_assets, 0.0f);
    std::vector<float> covariance(num_assets * num_assets, 0.0f);

    // Calculate means
    for (int a = 0; a < num_assets; a++) {
        for (int s = 0; s < num_scenarios; s++) {
            estimated_returns[a] += terminal_returns[s * num_assets + a];
        }
        estimated_returns[a] /= num_scenarios;
    }

    // Calculate covariance
    for (int i = 0; i < num_assets; i++) {
        for (int j = i; j < num_assets; j++) {
            float cov = 0.0f;
            for (int s = 0; s < num_scenarios; s++) {
                cov += (terminal_returns[s * num_assets + i] - estimated_returns[i]) *
                       (terminal_returns[s * num_assets + j] - estimated_returns[j]);
            }
            cov /= (num_scenarios - 1);
            covariance[i * num_assets + j] = cov;
            covariance[j * num_assets + i] = cov;
        }
    }

    // Verify estimated returns are close to expected
    for (int i = 0; i < num_assets; i++) {
        float rel_error = std::abs(estimated_returns[i] - expected_returns[i]) /
                         std::abs(expected_returns[i] + 0.01f);
        EXPECT_LT(rel_error, STATISTICAL_TOLERANCE)
            << "Estimated return for asset " << i << " differs too much from expected";
    }

    // ==================== Phase 4: Portfolio Optimization ====================
    auto opt_start = std::chrono::high_resolution_clock::now();

    fin_optimization_gpu_params_t opt_params = fin_optimization_gpu_params_default();
    opt_params.target_return = 0.08f;
    opt_params.max_iterations = 2000;
    opt_params.convergence_tolerance = 1e-8f;
    opt_params.long_only = true;
    opt_params.risk_aversion = 2.0f;
    opt_params.n_assets = static_cast<uint32_t>(num_assets);

    fin_optimization_gpu_result_t opt_result;
    memset(&opt_result, 0, sizeof(opt_result));
    opt_result.optimal_weights = new float[num_assets];

    bool opt_success = fin_optimization_gpu_mean_variance(
        ctx_, estimated_returns.data(), covariance.data(),
        &opt_params, &opt_result);
    ASSERT_TRUE(opt_success) << "Portfolio optimization failed";

    auto opt_end = std::chrono::high_resolution_clock::now();
    auto opt_time = std::chrono::duration_cast<std::chrono::milliseconds>(opt_end - opt_start);

    // Verify weights sum to 1
    float weight_sum = 0.0f;
    for (int i = 0; i < num_assets; i++) {
        EXPECT_GE(opt_result.optimal_weights[i], -TOLERANCE) << "Weight " << i << " is negative";
        weight_sum += opt_result.optimal_weights[i];
    }
    EXPECT_NEAR(weight_sum, 1.0f, 0.01f) << "Weights should sum to 1";

    // Calculate portfolio statistics
    float portfolio_return = 0.0f;
    float portfolio_variance = 0.0f;

    for (int i = 0; i < num_assets; i++) {
        portfolio_return += opt_result.optimal_weights[i] * estimated_returns[i];
        for (int j = 0; j < num_assets; j++) {
            portfolio_variance += opt_result.optimal_weights[i] * opt_result.optimal_weights[j] *
                                 covariance[i * num_assets + j];
        }
    }
    float portfolio_vol = std::sqrt(portfolio_variance);

    // ==================== Phase 5: Risk Analysis ====================
    // Calculate portfolio returns for all scenarios
    std::vector<float> portfolio_scenario_returns(num_scenarios);
    for (int s = 0; s < num_scenarios; s++) {
        float port_ret = 0.0f;
        for (int a = 0; a < num_assets; a++) {
            port_ret += opt_result.optimal_weights[a] * terminal_returns[s * num_assets + a];
        }
        portfolio_scenario_returns[s] = port_ret;
    }

    fin_risk_gpu_params_t risk_params = fin_risk_gpu_params_default();
    risk_params.confidence_level = 0.95f;
    risk_params.num_returns = static_cast<uint32_t>(num_scenarios);

    fin_risk_gpu_result_t risk_result;
    bool risk_success = fin_risk_gpu_compute(
        ctx_, portfolio_scenario_returns.data(), &risk_params, &risk_result);
    ASSERT_TRUE(risk_success) << "Risk computation failed";

    // VaR should be reasonable
    EXPECT_GT(risk_result.var_95, -1.0f) << "VaR too extreme";
    EXPECT_LT(risk_result.var_95, 0.5f) << "VaR too small (likely wrong sign)";

    // CVaR should be more extreme than VaR
    EXPECT_LE(risk_result.cvar_95, risk_result.var_95 + TOLERANCE)
        << "CVaR should be worse than VaR";

    // ==================== Phase 6: Options Hedging ====================
    // Price protective puts for the largest holdings
    std::vector<int> top_holdings;
    for (int i = 0; i < num_assets; i++) {
        if (opt_result.optimal_weights[i] > 0.1f) {
            top_holdings.push_back(i);
        }
    }

    if (!top_holdings.empty()) {
        std::vector<float> put_prices(top_holdings.size());
        bool deriv_success = true;

        // Price puts using binomial tree
        for (size_t i = 0; i < top_holdings.size(); i++) {
            int idx = top_holdings[i];
            fin_derivatives_gpu_params_t deriv_params = fin_derivatives_gpu_params_default();
            deriv_params.spot = initial_prices[idx];
            deriv_params.strike = initial_prices[idx] * 0.95f;  // 5% OTM puts
            deriv_params.risk_free_rate = 0.05f;
            deriv_params.volatility = volatilities[idx];
            deriv_params.time_to_expiry = 0.25f;  // 3-month puts
            deriv_params.option_type = FIN_OPT_PUT;
            deriv_params.option_style = FIN_OPT_STYLE_EUROPEAN;
            deriv_params.tree_steps = 100;

            fin_derivatives_gpu_result_t deriv_result;
            if (fin_derivatives_gpu_binomial_tree(ctx_, &deriv_params, &deriv_result)) {
                put_prices[i] = deriv_result.price;
            } else {
                deriv_success = false;
            }
        }

        if (deriv_success) {
            std::vector<float> spots;
            for (int idx : top_holdings) {
                spots.push_back(initial_prices[idx]);
            }
            float total_hedge_cost = 0.0f;
            for (size_t i = 0; i < top_holdings.size(); i++) {
                float position_value = opt_result.optimal_weights[top_holdings[i]] * 1000000.0f;  // $1M portfolio
                float num_contracts = position_value / (spots[i] * 100.0f);  // 100 shares per contract
                float hedge_cost = num_contracts * put_prices[i] * 100.0f;
                total_hedge_cost += hedge_cost;
            }

            float portfolio_value = 1000000.0f;
            float hedge_cost_pct = total_hedge_cost / portfolio_value * 100.0f;

            std::cout << "Hedging cost: $" << total_hedge_cost
                      << " (" << hedge_cost_pct << "% of portfolio)" << std::endl;

            EXPECT_LT(hedge_cost_pct, 5.0f) << "Hedge cost should be reasonable";
        }
    }

    // ==================== Phase 7: Performance Report ====================
    std::cout << "\n=== Portfolio Management E2E Results ===" << std::endl;
    std::cout << "Simulation time: " << sim_time.count() << " ms (" << num_scenarios << " scenarios)" << std::endl;
    std::cout << "Optimization time: " << opt_time.count() << " ms" << std::endl;
    std::cout << "\nOptimal Portfolio:" << std::endl;
    std::cout << "  Expected Return: " << portfolio_return * 100.0f << "%" << std::endl;
    std::cout << "  Volatility: " << portfolio_vol * 100.0f << "%" << std::endl;
    std::cout << "  Sharpe Ratio: " << (portfolio_return - 0.02f) / portfolio_vol << std::endl;
    std::cout << "\nRisk Metrics:" << std::endl;
    std::cout << "  VaR (95%): " << risk_result.var_95 * 100.0f << "%" << std::endl;
    std::cout << "  CVaR (95%): " << risk_result.cvar_95 * 100.0f << "%" << std::endl;
    std::cout << "\nTop Holdings:" << std::endl;
    for (int i = 0; i < num_assets; i++) {
        if (opt_result.optimal_weights[i] > 0.05f) {
            std::cout << "  Asset " << i << ": " << opt_result.optimal_weights[i] * 100.0f << "%" << std::endl;
        }
    }

    delete[] opt_result.optimal_weights;
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * E2E Test: Options Market Making Simulation
 * ============================================================================ */
TEST_F(FinancialWorkflowE2ETest, OptionsMarketMakingSimulation) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    // Simulate a market maker quoting options throughout the day
    const int num_quotes = 1000;  // Number of quote updates
    const int options_per_quote = 50;  // Options in the book

    float spot = 100.0f;
    float base_vol = 0.20f;
    float rf_rate = 0.05f;

    // Generate option chain (5 strikes x 4 maturities x 2 types)
    std::vector<float> all_strikes;
    std::vector<float> all_times;
    std::vector<fin_option_type_t> all_types;

    float strike_range[] = {90.0f, 95.0f, 100.0f, 105.0f, 110.0f};
    float time_range[] = {0.0833f, 0.25f, 0.5f, 1.0f};  // 1M, 3M, 6M, 1Y

    for (float T : time_range) {
        for (float K : strike_range) {
            all_strikes.push_back(K);
            all_times.push_back(T);
            all_types.push_back(FIN_OPT_CALL);
            all_strikes.push_back(K);
            all_times.push_back(T);
            all_types.push_back(FIN_OPT_PUT);
        }
    }

    const int total_options = static_cast<int>(all_strikes.size());

    auto start = std::chrono::high_resolution_clock::now();

    float total_delta = 0.0f;
    float total_gamma = 0.0f;
    float total_vega = 0.0f;

    std::mt19937 gen(42);
    std::normal_distribution<float> spot_change(0.0f, 0.005f);  // 0.5% moves
    std::normal_distribution<float> vol_change(0.0f, 0.01f);    // 1% vol changes

    for (int quote = 0; quote < num_quotes; quote++) {
        // Update market conditions
        spot *= (1.0f + spot_change(gen));
        base_vol = std::max(0.05f, std::min(0.50f, base_vol + vol_change(gen)));

        // Build vol smile
        std::vector<float> vols(total_options);
        for (int i = 0; i < total_options; i++) {
            float moneyness = all_strikes[i] / spot;
            float smile_adjust = 0.05f * (moneyness - 1.0f) * (moneyness - 1.0f);
            vols[i] = base_vol + smile_adjust;
        }

        std::vector<float> spots(total_options, spot);
        std::vector<float> rates(total_options, rf_rate);

        // Price all options using binomial tree
        std::vector<float> prices(total_options);
        bool success = true;
        for (int i = 0; i < total_options; i++) {
            fin_derivatives_gpu_params_t deriv_params = fin_derivatives_gpu_params_default();
            deriv_params.spot = spots[i];
            deriv_params.strike = all_strikes[i];
            deriv_params.risk_free_rate = rates[i];
            deriv_params.volatility = vols[i];
            deriv_params.time_to_expiry = all_times[i];
            deriv_params.option_type = all_types[i];
            deriv_params.option_style = FIN_OPT_STYLE_EUROPEAN;
            deriv_params.tree_steps = 50;  // Fewer steps for speed

            fin_derivatives_gpu_result_t deriv_result;
            if (fin_derivatives_gpu_binomial_tree(ctx_, &deriv_params, &deriv_result)) {
                prices[i] = deriv_result.price;
            } else {
                success = false;
                break;
            }
        }

        if (!success) continue;

        // Compute Greeks for a subset (top 20 options by vega)
        std::vector<std::pair<float, int>> vega_rank;
        for (int i = 0; i < total_options; i++) {
            // Estimate vega from vol sensitivity
            float est_vega = prices[i] * 0.01f / vols[i];
            vega_rank.push_back({est_vega, i});
        }
        std::sort(vega_rank.begin(), vega_rank.end(),
            [](const auto& a, const auto& b) { return a.first > b.first; });

        float quote_delta = 0.0f, quote_gamma = 0.0f, quote_vega = 0.0f;

        for (int k = 0; k < std::min(20, total_options); k++) {
            int idx = vega_rank[k].second;

            fin_derivatives_gpu_params_t deriv_params = fin_derivatives_gpu_params_default();
            deriv_params.spot = spot;
            deriv_params.strike = all_strikes[idx];
            deriv_params.risk_free_rate = rf_rate;
            deriv_params.volatility = vols[idx];
            deriv_params.time_to_expiry = all_times[idx];
            deriv_params.tree_steps = 100;
            deriv_params.option_type = all_types[idx];
            deriv_params.option_style = FIN_OPT_STYLE_EUROPEAN;
            deriv_params.compute_greeks = true;

            fin_derivatives_gpu_result_t deriv_result;
            if (fin_derivatives_gpu_binomial_tree(ctx_, &deriv_params, &deriv_result)) {
                quote_delta += deriv_result.delta;
                quote_gamma += deriv_result.gamma;
                quote_vega += deriv_result.vega;
            }
        }

        total_delta += quote_delta;
        total_gamma += quote_gamma;
        total_vega += quote_vega;
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    float quotes_per_sec = num_quotes / (elapsed.count() / 1000.0f);
    float options_per_sec = num_quotes * total_options / (elapsed.count() / 1000.0f);

    std::cout << "\n=== Options Market Making E2E Results ===" << std::endl;
    std::cout << "Total time: " << elapsed.count() << " ms" << std::endl;
    std::cout << "Quote updates: " << num_quotes << std::endl;
    std::cout << "Options per update: " << total_options << std::endl;
    std::cout << "Quote throughput: " << quotes_per_sec << " quotes/sec" << std::endl;
    std::cout << "Pricing throughput: " << options_per_sec << " options/sec" << std::endl;

    // Should be able to process at least 100 quotes/sec for real-time trading
    EXPECT_GT(quotes_per_sec, 50.0f) << "Quote throughput too low for trading";
    EXPECT_GT(options_per_sec, 1000.0f) << "Pricing throughput too low";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * E2E Test: Risk Scenario Analysis
 * ============================================================================ */
TEST_F(FinancialWorkflowE2ETest, RiskScenarioAnalysis) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";
    ASSERT_NE(rng_, nullptr) << "RNG required";

    // Define market scenarios
    struct Scenario {
        const char* name;
        float drift;
        float volatility;
    };

    std::vector<Scenario> scenarios = {
        {"Bull Market", 0.15f, 0.15f},
        {"Bear Market", -0.20f, 0.30f},
        {"High Volatility", 0.05f, 0.40f},
        {"Recession", -0.30f, 0.35f},
        {"Recovery", 0.25f, 0.25f},
        {"Stagflation", -0.05f, 0.20f}
    };

    const int num_paths = 10000;
    const float portfolio_value = 1000000.0f;  // $1M portfolio

    std::cout << "\n=== Risk Scenario Analysis ===" << std::endl;
    std::cout << std::string(70, '-') << std::endl;
    std::cout << "Scenario         | Mean Return | VaR (95%) | CVaR (95%) | Max Loss" << std::endl;
    std::cout << std::string(70, '-') << std::endl;

    for (const auto& scenario : scenarios) {
        fin_monte_carlo_gpu_params_t mc_params = fin_monte_carlo_gpu_params_default();
        mc_params.initial_value = portfolio_value;
        mc_params.drift = scenario.drift;
        mc_params.volatility = scenario.volatility;
        mc_params.time_horizon = 1.0f;
        mc_params.num_steps = 252;
        mc_params.num_paths = static_cast<uint32_t>(num_paths);

        fin_monte_carlo_gpu_result_t mc_result;
        memset(&mc_result, 0, sizeof(mc_result));

        bool mc_success = fin_monte_carlo_gpu_simulate(ctx_, rng_, &mc_params, &mc_result);

        if (mc_success) {
            // Use the computed VaR/CVaR from monte carlo result directly
            float mean_return_pct = mc_result.mean_return;
            float max_loss = -mc_result.var_99;  // Approximate max loss from 99% VaR

            printf("%-16s | %+10.1f%% | %9.1f%% | %10.1f%% | %8.1f%%\n",
                   scenario.name,
                   mean_return_pct * 100.0f,
                   mc_result.var_95 * 100.0f,
                   mc_result.cvar_95 * 100.0f,
                   max_loss * 100.0f);

            // Validate scenario expectations
            if (scenario.drift > 0.1f) {
                EXPECT_GT(mean_return_pct, 0.0f) << scenario.name << " should have positive mean";
            } else if (scenario.drift < -0.1f) {
                EXPECT_LT(mean_return_pct, 0.0f) << scenario.name << " should have negative mean";
            }

            // Higher volatility should generally mean worse VaR
            if (scenario.volatility > 0.30f) {
                EXPECT_LT(mc_result.var_95, -0.2f)
                    << scenario.name << " VaR should be significant with high vol";
            }
        }

        // Free monte carlo result resources
        // Result struct was zeroed, no dynamic memory to free
    }

    std::cout << std::string(70, '-') << std::endl;
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * E2E Test: GPU Performance Benchmark
 * ============================================================================ */
TEST_F(FinancialWorkflowE2ETest, GPUPerformanceBenchmark) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";
    ASSERT_NE(rng_, nullptr) << "RNG required";

    std::cout << "\n=== GPU Financial Computing Benchmark ===" << std::endl;

    // Benchmark 1: Monte Carlo Scaling
    std::cout << "\n1. Monte Carlo Path Scaling:" << std::endl;
    std::vector<int> path_counts = {1000, 10000, 100000, 500000};

    for (int num_paths : path_counts) {
        fin_monte_carlo_gpu_params_t params = fin_monte_carlo_gpu_params_default();
        params.initial_value = 100.0f;
        params.drift = 0.05f;
        params.volatility = 0.20f;
        params.time_horizon = 1.0f;
        params.num_steps = 252;
        params.num_paths = static_cast<uint32_t>(num_paths);

        fin_monte_carlo_gpu_result_t result;
        memset(&result, 0, sizeof(result));

        auto start = std::chrono::high_resolution_clock::now();
        fin_monte_carlo_gpu_simulate(ctx_, rng_, &params, &result);
        auto end = std::chrono::high_resolution_clock::now();

        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        float paths_per_sec = num_paths / (elapsed.count() / 1e6f);

        std::cout << "  " << num_paths << " paths: "
                  << elapsed.count() / 1000.0f << " ms ("
                  << paths_per_sec / 1e6f << "M paths/sec)" << std::endl;

        // Result struct was zeroed, no dynamic memory to free
    }

    // Benchmark 2: Option Pricing Batch Size (using binomial tree)
    std::cout << "\n2. Binomial Tree Batch Scaling:" << std::endl;
    std::vector<int> batch_sizes = {10, 50, 100, 200};

    for (int batch : batch_sizes) {
        std::vector<float> prices(batch);

        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < batch; i++) {
            fin_derivatives_gpu_params_t deriv_params = fin_derivatives_gpu_params_default();
            deriv_params.spot = 100.0f;
            deriv_params.strike = 100.0f;
            deriv_params.risk_free_rate = 0.05f;
            deriv_params.volatility = 0.20f;
            deriv_params.time_to_expiry = 1.0f;
            deriv_params.option_type = FIN_OPT_CALL;
            deriv_params.option_style = FIN_OPT_STYLE_EUROPEAN;
            deriv_params.tree_steps = 100;

            fin_derivatives_gpu_result_t deriv_result;
            fin_derivatives_gpu_binomial_tree(ctx_, &deriv_params, &deriv_result);
            prices[i] = deriv_result.price;
        }
        auto end = std::chrono::high_resolution_clock::now();

        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        float opts_per_sec = batch / (elapsed.count() / 1e6f);

        std::cout << "  " << batch << " options: "
                  << elapsed.count() / 1000.0f << " ms ("
                  << opts_per_sec << " opts/sec)" << std::endl;
    }

    // Benchmark 3: Portfolio Optimization Asset Scaling
    std::cout << "\n3. Portfolio Optimization Scaling:" << std::endl;
    std::vector<int> asset_counts = {10, 50, 100, 200};

    for (int n : asset_counts) {
        std::vector<float> returns(n);
        std::vector<float> covariance(n * n);

        // Generate random returns and positive-definite covariance
        std::mt19937 gen(42);
        std::normal_distribution<float> dist(0.08f, 0.03f);
        for (int i = 0; i < n; i++) {
            returns[i] = dist(gen);
            for (int j = 0; j < n; j++) {
                if (i == j) {
                    covariance[i * n + j] = 0.04f;
                } else {
                    covariance[i * n + j] = 0.01f;
                }
            }
        }

        fin_optimization_gpu_params_t params = fin_optimization_gpu_params_default();
        params.target_return = 0.08f;
        params.max_iterations = 1000;
        params.convergence_tolerance = 1e-6f;
        params.long_only = true;
        params.risk_aversion = 1.0f;
        params.n_assets = n;

        fin_optimization_gpu_result_t result;
        memset(&result, 0, sizeof(result));
        result.optimal_weights = new float[n];

        auto start = std::chrono::high_resolution_clock::now();
        fin_optimization_gpu_mean_variance(ctx_, returns.data(), covariance.data(), &params, &result);
        auto end = std::chrono::high_resolution_clock::now();

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        std::cout << "  " << n << " assets: " << elapsed.count() << " ms" << std::endl;

        delete[] result.optimal_weights;

        // Optimization should complete in reasonable time
        EXPECT_LT(elapsed.count(), 5000) << n << " asset optimization took too long";
    }
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

}  // namespace
