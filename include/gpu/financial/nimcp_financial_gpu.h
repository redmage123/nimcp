//=============================================================================
// nimcp_financial_gpu.h - Main GPU Financial Acceleration API
//=============================================================================
/**
 * @file nimcp_financial_gpu.h
 * @brief GPU-accelerated financial computations for Monte Carlo, portfolio
 *        optimization, risk metrics, and derivatives pricing
 *
 * WHAT: GPU acceleration for computationally intensive financial operations
 * WHY:  33-100x speedup for Monte Carlo, portfolio optimization, and
 *       efficient frontier computation
 * HOW:  CUDA kernels with cuBLAS for matrix operations, cuRAND for RNG,
 *       parallel sorting for VaR/CVaR
 *
 * ARCHITECTURE:
 *
 *   +------------------------------------------------------------------+
 *   |                  GPU FINANCIAL ACCELERATION                       |
 *   |                                                                  |
 *   |  +----------------+  +------------------+  +------------------+  |
 *   |  | Monte Carlo    |  | Portfolio        |  | Risk Metrics     |  |
 *   |  | (100K paths,   |  | Optimization     |  | (VaR, CVaR,      |  |
 *   |  |  GBM/Heston)   |  | (cuBLAS)         |  |  GPU sorting)    |  |
 *   |  +----------------+  +------------------+  +------------------+  |
 *   |           |                  |                    |              |
 *   |  +----------------+  +------------------+  +------------------+  |
 *   |  | Efficient      |  | Binomial Tree    |  | Greeks           |  |
 *   |  | Frontier       |  | (American opts)  |  | Computation      |  |
 *   |  +----------------+  +------------------+  +------------------+  |
 *   +------------------------------------------------------------------+
 *
 * EXPECTED PERFORMANCE:
 *   - Monte Carlo (100K paths): 500ms CPU -> 15ms GPU (33x)
 *   - Portfolio Opt (256 assets): 200ms CPU -> 8ms GPU (25x)
 *   - Efficient Frontier (50 pts): 2s CPU -> 50ms GPU (40x)
 *
 * @author NIMCP Development Team
 * @date 2025
 */

#ifndef NIMCP_FINANCIAL_GPU_H
#define NIMCP_FINANCIAL_GPU_H

#include "common/nimcp_export.h"
#include "gpu/context/nimcp_gpu_context.h"
#include "cognitive/parietal/nimcp_financial_investment.h"
#include "cognitive/parietal/nimcp_financial_market.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

#define BIO_MODULE_FINANCIAL_GPU            0x03A0

/** Maximum Monte Carlo paths on GPU */
#define FIN_GPU_MAX_MONTE_CARLO_PATHS       1000000

/** Maximum assets for GPU portfolio optimization */
#define FIN_GPU_MAX_ASSETS                  512

/** Maximum efficient frontier points */
#define FIN_GPU_MAX_FRONTIER_POINTS         256

/** Maximum binomial tree steps */
#define FIN_GPU_MAX_TREE_STEPS              4096

/** Default CUDA block size for financial kernels */
#define FIN_GPU_BLOCK_SIZE                  256

//=============================================================================
// Error Codes
//=============================================================================

#define FIN_GPU_ERR_BASE                    33000

#define FIN_GPU_ERR_OK                      0
#define FIN_GPU_ERR_NULL_CTX                (FIN_GPU_ERR_BASE + 1)
#define FIN_GPU_ERR_NULL_INPUT              (FIN_GPU_ERR_BASE + 2)
#define FIN_GPU_ERR_NULL_OUTPUT             (FIN_GPU_ERR_BASE + 3)
#define FIN_GPU_ERR_ALLOC                   (FIN_GPU_ERR_BASE + 4)
#define FIN_GPU_ERR_CUDA                    (FIN_GPU_ERR_BASE + 5)
#define FIN_GPU_ERR_CUBLAS                  (FIN_GPU_ERR_BASE + 6)
#define FIN_GPU_ERR_CURAND                  (FIN_GPU_ERR_BASE + 7)
#define FIN_GPU_ERR_KERNEL_LAUNCH           (FIN_GPU_ERR_BASE + 8)
#define FIN_GPU_ERR_INVALID_PARAMS          (FIN_GPU_ERR_BASE + 9)
#define FIN_GPU_ERR_CONVERGENCE             (FIN_GPU_ERR_BASE + 10)
#define FIN_GPU_ERR_TOO_MANY_ASSETS         (FIN_GPU_ERR_BASE + 11)
#define FIN_GPU_ERR_TOO_MANY_PATHS          (FIN_GPU_ERR_BASE + 12)
#define FIN_GPU_ERR_RNG_NOT_INIT            (FIN_GPU_ERR_BASE + 13)
#define FIN_GPU_ERR_SINGULAR_MATRIX         (FIN_GPU_ERR_BASE + 14)

//=============================================================================
// Forward Declarations
//=============================================================================

/** GPU RNG state handle (opaque) */
typedef struct fin_gpu_rng_s fin_gpu_rng_t;

//=============================================================================
// Monte Carlo Parameters and Results
//=============================================================================

/**
 * @brief Stochastic process type for Monte Carlo
 */
typedef enum {
    FIN_GPU_PROCESS_GBM,                    /**< Geometric Brownian Motion */
    FIN_GPU_PROCESS_HESTON,                 /**< Heston stochastic volatility */
    FIN_GPU_PROCESS_JUMP_DIFFUSION,         /**< Merton jump-diffusion */
    FIN_GPU_PROCESS_COUNT
} fin_gpu_process_type_t;

/**
 * @brief Monte Carlo simulation parameters
 */
typedef struct {
    fin_gpu_process_type_t process_type;    /**< Stochastic process model */
    float initial_value;                    /**< Starting asset value */
    float drift;                            /**< Annual drift (mu) */
    float volatility;                       /**< Annual volatility (sigma) */
    float risk_free_rate;                   /**< Risk-free rate */
    float horizon_years;                    /**< Simulation horizon (alias for time_horizon) */
    float time_horizon;                     /**< Simulation horizon in years */
    uint32_t num_paths;                     /**< Number of Monte Carlo paths */
    uint32_t num_steps;                     /**< Time steps per path */
    uint64_t seed;                          /**< RNG seed (0 for time-based) */

    /* Heston model parameters (if process_type == HESTON) */
    float vol_of_vol;                       /**< Vol-of-vol (kappa in Heston) */
    float mean_reversion;                   /**< Mean reversion speed */
    float long_term_variance;               /**< Long-term variance level */
    float correlation;                      /**< Correlation between asset and vol */
    float initial_variance;                 /**< Initial variance */

    /* Jump-diffusion parameters (if process_type == JUMP_DIFFUSION) */
    float jump_intensity;                   /**< Jump arrival rate (lambda) */
    float jump_mean;                        /**< Mean jump size */
    float jump_vol;                         /**< Jump size volatility */

    /* Options */
    bool antithetic;                        /**< Use antithetic variates */
    bool use_antithetic;                    /**< Use antithetic variates (alias) */
    bool control_variate;                   /**< Use control variates */
    bool store_paths;                       /**< Store full path data (memory intensive) */
} fin_monte_carlo_gpu_params_t;

/**
 * @brief Monte Carlo simulation result
 */
typedef struct {
    float mean_return;                      /**< Mean of simulated returns */
    float mean_value;                       /**< Mean terminal value (discounted) */
    float std_return;                       /**< Standard deviation of returns */
    float std_error;                        /**< Standard error of the estimate */
    float variance;                         /**< Variance of results */
    float var_95;                           /**< 95% Value at Risk */
    float var_99;                           /**< 99% Value at Risk */
    float cvar_95;                          /**< 95% Conditional VaR */
    float cvar_99;                          /**< 99% Conditional VaR */
    float probability_of_loss;              /**< P(return < 0) */
    float expected_shortfall;               /**< Expected loss given loss */
    float median_terminal;                  /**< Median terminal value */
    float min_terminal;                     /**< Minimum terminal value */
    float max_terminal;                     /**< Maximum terminal value */
    float skewness;                         /**< Skewness of returns */
    float kurtosis;                         /**< Excess kurtosis of returns */
    uint32_t paths_completed;               /**< Paths successfully simulated */
    uint32_t num_paths;                     /**< Total paths in result */
    float kernel_time_ms;                   /**< GPU kernel execution time */

    /* Optional: terminal values (if store_paths=false) */
    float* terminal_values;                 /**< Terminal values [num_paths] (device) */

    /* Optional: full paths (if store_paths=true) */
    float* path_data;                       /**< Full paths [num_paths x num_steps] (device) */
} fin_monte_carlo_gpu_result_t;

/**
 * @brief Default Monte Carlo parameters
 */
static inline fin_monte_carlo_gpu_params_t fin_monte_carlo_gpu_params_default(void)
{
    fin_monte_carlo_gpu_params_t params = {
        .process_type = FIN_GPU_PROCESS_GBM,
        .initial_value = 100.0f,
        .drift = 0.05f,
        .volatility = 0.2f,
        .risk_free_rate = 0.02f,
        .horizon_years = 1.0f,
        .time_horizon = 1.0f,
        .num_paths = 100000,
        .num_steps = 252,
        .seed = 0,
        .vol_of_vol = 0.0f,
        .mean_reversion = 0.0f,
        .long_term_variance = 0.0f,
        .correlation = 0.0f,
        .initial_variance = 0.04f,
        .jump_intensity = 0.0f,
        .jump_mean = 0.0f,
        .jump_vol = 0.0f,
        .antithetic = true,
        .use_antithetic = true,
        .control_variate = false,
        .store_paths = false
    };
    return params;
}

//=============================================================================
// Monte Carlo Option Pricing Types
//=============================================================================

/**
 * @brief Monte Carlo option style for exotic options
 */
typedef enum {
    FIN_MC_OPT_EUROPEAN,                    /**< Standard European option */
    FIN_MC_OPT_ASIAN,                       /**< Asian (average price) option */
    FIN_MC_OPT_LOOKBACK,                    /**< Lookback (floating strike) option */
    FIN_MC_OPT_BARRIER,                     /**< Barrier (knock-in/out) option */
    FIN_MC_OPT_COUNT
} fin_mc_option_style_t;

/**
 * @brief Monte Carlo option pricing parameters
 */
typedef struct {
    fin_monte_carlo_gpu_params_t base;      /**< Base MC parameters */
    fin_option_type_t option_type;          /**< Call or Put */
    fin_mc_option_style_t option_style;     /**< Option style (Asian, Lookback, etc.) */
    float strike;                           /**< Strike price */
    float barrier;                          /**< Barrier level (for barrier options) */
    bool is_up_barrier;                     /**< true=up-and-out, false=down-and-out */
    float rebate;                           /**< Rebate if knocked out */
} fin_mc_option_params_t;

/**
 * @brief Monte Carlo option pricing result
 */
typedef struct {
    float price;                            /**< Option price */
    float std_error;                        /**< Standard error of price estimate */
    float delta;                            /**< Delta (dPrice/dSpot) */
    float gamma;                            /**< Gamma (d2Price/dSpot2) */
    float kernel_time_ms;                   /**< GPU kernel time */
} fin_mc_option_result_t;

/**
 * @brief Heston stochastic volatility model parameters
 */
typedef struct {
    fin_monte_carlo_gpu_params_t base;      /**< Base MC parameters */
    float initial_variance;                 /**< Initial variance v0 */
    float kappa;                            /**< Mean reversion speed */
    float theta;                            /**< Long-term variance level */
    float xi;                               /**< Volatility of volatility */
    float rho;                              /**< Correlation between asset and vol */
} fin_heston_params_t;

/**
 * @brief Multi-asset correlated simulation parameters
 */
typedef struct {
    fin_monte_carlo_gpu_params_t base;      /**< Base MC parameters */
    uint32_t n_assets;                      /**< Number of assets */
    const float* initial_values;            /**< Initial prices [n_assets] */
    const float* drifts;                    /**< Drift rates [n_assets] */
    const float* volatilities;              /**< Volatilities [n_assets] */
    const float* cholesky_L;                /**< Cholesky factor of correlation [n_assets x n_assets] */
} fin_multi_asset_params_t;

//=============================================================================
// Portfolio Optimization Parameters and Results
//=============================================================================

/**
 * @brief GPU portfolio optimization parameters
 */
typedef struct {
    fin_optimization_strategy_t strategy;   /**< Optimization strategy */
    float risk_free_rate;                   /**< Risk-free rate */
    float target_return;                    /**< Target return (for mean-variance) */
    float target_risk;                      /**< Target risk (for risk parity) */
    float min_weight;                       /**< Minimum weight per asset */
    float max_weight;                       /**< Maximum weight per asset */
    float convergence_tolerance;            /**< Convergence threshold */
    uint32_t max_iterations;                /**< Maximum optimization iterations */
    float regularization;                   /**< L2 regularization for stability */
    bool use_warm_start;                    /**< Use previous solution as start */
    bool long_only;                         /**< Constraint: weights >= 0 */
    uint32_t n_assets;                      /**< Number of assets in portfolio */
    float* lower_bounds;                    /**< Lower bounds per asset (device) */
    float* upper_bounds;                    /**< Upper bounds per asset (device) */
    float learning_rate;                    /**< Learning rate for gradient descent */
    float risk_aversion;                    /**< Risk aversion parameter */
} fin_optimization_gpu_params_t;

/**
 * @brief GPU portfolio optimization result
 */
typedef struct {
    float* optimal_weights;                 /**< Optimal weights [num_assets] */
    float expected_return;                  /**< Portfolio expected return */
    float expected_volatility;              /**< Portfolio volatility */
    float portfolio_variance;               /**< Portfolio variance */
    float portfolio_volatility;             /**< Portfolio volatility (redundant for compat) */
    float sharpe_ratio;                     /**< Sharpe ratio */
    float objective_value;                  /**< Final objective value */
    uint32_t iterations;                    /**< Iterations to converge */
    uint32_t n_assets;                      /**< Number of assets */
    bool converged;                         /**< Convergence achieved */
    float kernel_time_ms;                   /**< GPU time */
} fin_optimization_gpu_result_t;

/**
 * @brief Default optimization parameters
 */
static inline fin_optimization_gpu_params_t fin_optimization_gpu_params_default(void)
{
    fin_optimization_gpu_params_t params = {
        .strategy = FIN_OPT_STRATEGY_MAX_SHARPE,
        .risk_free_rate = 0.02f,
        .target_return = 0.10f,
        .target_risk = 0.15f,
        .min_weight = 0.0f,
        .max_weight = 1.0f,
        .convergence_tolerance = 1e-6f,
        .max_iterations = 1000,
        .regularization = 1e-6f,
        .use_warm_start = false,
        .long_only = true,
        .n_assets = 0,
        .lower_bounds = NULL,
        .upper_bounds = NULL,
        .learning_rate = 0.01f,
        .risk_aversion = 1.0f
    };
    return params;
}

/**
 * @brief Efficient frontier result
 */
typedef struct {
    float* returns;                         /**< Returns for each frontier point */
    float* volatilities;                    /**< Volatilities for each frontier point */
    float* sharpe_ratios;                   /**< Sharpe ratios for each frontier point */
    float* weights;                         /**< Weights [num_points x num_assets] */
    uint32_t num_points;                    /**< Number of frontier points */
    float kernel_time_ms;                   /**< GPU kernel time */
} fin_efficient_frontier_result_t;

/**
 * @brief Risk parity optimization parameters
 */
typedef struct {
    uint32_t n_assets;                      /**< Number of assets */
    float target_risk;                      /**< Target portfolio risk */
    float convergence_tolerance;            /**< Convergence threshold */
    uint32_t max_iterations;                /**< Maximum iterations */
    float learning_rate;                    /**< Learning rate for optimization */
    float* initial_weights;                 /**< Initial guess (device) */
} fin_risk_parity_params_t;

//=============================================================================
// Risk Metrics Parameters and Results
//=============================================================================

/**
 * @brief Volatility estimation method
 */
typedef enum {
    FIN_VOL_SIMPLE,                         /**< Simple historical volatility */
    FIN_VOL_EWMA,                           /**< Exponentially weighted moving average */
    FIN_VOL_PARKINSON,                      /**< Parkinson (high-low) estimator */
    FIN_VOL_GARMAN_KLASS,                   /**< Garman-Klass (OHLC) estimator */
    FIN_VOL_YANG_ZHANG,                     /**< Yang-Zhang (OHLC + overnight) */
    FIN_VOL_COUNT
} fin_volatility_type_t;

/** Alias for backwards compatibility */
typedef fin_volatility_type_t fin_vol_method_t;

/**
 * @brief GPU risk metrics parameters
 */
typedef struct {
    float confidence_95;                    /**< Confidence for 95% VaR (0.95) */
    float confidence_99;                    /**< Confidence for 99% VaR (0.99) */
    float confidence_level;                 /**< General confidence level */
    bool compute_var;                       /**< Compute VaR */
    bool compute_cvar;                      /**< Compute CVaR */
    bool compute_drawdown;                  /**< Compute max drawdown */
    bool compute_volatility;                /**< Compute volatility metrics */
    float risk_free_rate;                   /**< For Sharpe ratio */
    uint32_t annualization_factor;          /**< Days per year (252) */
    uint32_t num_returns;                   /**< Number of return observations */
    fin_volatility_type_t vol_type;         /**< Volatility estimation method */
} fin_risk_gpu_params_t;

/**
 * @brief GPU risk metrics result
 */
typedef struct {
    float var_95;                           /**< 95% VaR */
    float var_99;                           /**< 99% VaR */
    float var;                              /**< VaR at confidence_level */
    float var_parametric;                   /**< Parametric VaR estimate */
    float cvar_95;                          /**< 95% CVaR */
    float cvar_99;                          /**< 99% CVaR */
    float cvar;                             /**< CVaR at confidence_level */
    float volatility;                       /**< Computed volatility */
    float volatility_daily;                 /**< Daily volatility */
    float volatility_annual;                /**< Annualized volatility */
    float mean_return;                      /**< Mean return */
    float sharpe_ratio;                     /**< Sharpe ratio */
    float sortino_ratio;                    /**< Sortino ratio */
    float max_drawdown;                     /**< Maximum drawdown */
    float downside_deviation;               /**< Downside deviation */
    float kernel_time_ms;                   /**< GPU time */
} fin_risk_gpu_result_t;

/**
 * @brief Default risk parameters
 */
static inline fin_risk_gpu_params_t fin_risk_gpu_params_default(void)
{
    fin_risk_gpu_params_t params = {
        .confidence_95 = 0.95f,
        .confidence_99 = 0.99f,
        .confidence_level = 0.95f,
        .compute_var = true,
        .compute_cvar = true,
        .compute_drawdown = true,
        .compute_volatility = true,
        .risk_free_rate = 0.02f,
        .annualization_factor = 252,
        .num_returns = 0,
        .vol_type = FIN_VOL_SIMPLE
    };
    return params;
}

//=============================================================================
// Derivatives Pricing Parameters and Results
//=============================================================================

/**
 * @brief GPU derivatives pricing parameters
 */
typedef struct {
    fin_option_type_t option_type;          /**< Call or Put */
    fin_option_style_t option_style;        /**< European, American, Bermudan */
    float spot;                             /**< Current spot price */
    float strike;                           /**< Strike price */
    float risk_free_rate;                   /**< Risk-free rate */
    float volatility;                       /**< Volatility (sigma) */
    float time_to_expiry;                   /**< Time to expiry in years */
    float dividend_yield;                   /**< Continuous dividend yield */
    uint32_t tree_steps;                    /**< Binomial tree steps */
    uint32_t mc_paths;                      /**< Monte Carlo paths (for MC pricing) */
    bool compute_greeks;                    /**< Compute Greeks */
} fin_derivatives_gpu_params_t;

/**
 * @brief GPU derivatives pricing result
 */
typedef struct {
    float price;                            /**< Option price */
    float delta;                            /**< Delta */
    float gamma;                            /**< Gamma */
    float theta;                            /**< Theta */
    float vega;                             /**< Vega */
    float rho;                              /**< Rho */
    float implied_volatility;               /**< Implied vol (if market price given) */
    float early_exercise_premium;           /**< American - European difference */
    uint32_t steps_used;                    /**< Tree steps actually used */
    float kernel_time_ms;                   /**< GPU time */
} fin_derivatives_gpu_result_t;

/**
 * @brief Default derivatives parameters
 */
static inline fin_derivatives_gpu_params_t fin_derivatives_gpu_params_default(void)
{
    fin_derivatives_gpu_params_t params = {
        .option_type = FIN_OPT_CALL,
        .option_style = FIN_OPT_STYLE_EUROPEAN,
        .spot = 100.0f,
        .strike = 100.0f,
        .risk_free_rate = 0.02f,
        .volatility = 0.2f,
        .time_to_expiry = 1.0f,
        .dividend_yield = 0.0f,
        .tree_steps = 1000,
        .mc_paths = 100000,
        .compute_greeks = true
    };
    return params;
}

//=============================================================================
// Statistics
//=============================================================================

/**
 * @brief GPU financial operation statistics
 */
typedef struct {
    uint64_t monte_carlo_runs;              /**< Total MC simulations */
    uint64_t total_mc_paths;                /**< Total paths simulated */
    uint64_t portfolio_optimizations;       /**< Total optimizations */
    uint64_t risk_calculations;             /**< Total risk metric calculations */
    uint64_t derivatives_pricings;          /**< Total option pricings */
    uint64_t frontier_computations;         /**< Total efficient frontiers */
    float total_kernel_time_ms;             /**< Total GPU time */
    size_t peak_memory_bytes;               /**< Peak GPU memory usage */
} fin_gpu_stats_t;

//=============================================================================
// RNG Lifecycle
//=============================================================================

/**
 * @brief Create GPU RNG state
 *
 * @param ctx  GPU context
 * @param n    Number of parallel generators (typically num_paths)
 * @param seed Initial seed (0 for time-based)
 * @return RNG handle on success, NULL on failure
 */
NIMCP_EXPORT fin_gpu_rng_t* fin_gpu_rng_create(
    nimcp_gpu_context_t* ctx,
    uint32_t n,
    uint64_t seed);

/**
 * @brief Destroy GPU RNG state
 */
NIMCP_EXPORT void fin_gpu_rng_destroy(fin_gpu_rng_t* rng);

/**
 * @brief Reseed GPU RNG
 */
NIMCP_EXPORT bool fin_gpu_rng_reseed(fin_gpu_rng_t* rng, uint64_t seed);

//=============================================================================
// Monte Carlo API
//=============================================================================

/**
 * @brief Run Monte Carlo simulation on GPU
 *
 * @param ctx    GPU context
 * @param rng    GPU RNG state
 * @param params Simulation parameters
 * @param result Output result (terminal_values populated if not store_paths)
 * @return true on success
 *
 * EXAMPLE:
 *   fin_gpu_rng_t* rng = fin_gpu_rng_create(ctx, 100000, 42);
 *   fin_monte_carlo_gpu_params_t params = fin_monte_carlo_gpu_params_default();
 *   params.num_paths = 100000;
 *   params.volatility = 0.25f;
 *   fin_monte_carlo_gpu_result_t result = {0};
 *   bool ok = fin_monte_carlo_gpu_simulate(ctx, rng, &params, &result);
 */
NIMCP_EXPORT bool fin_monte_carlo_gpu_simulate(
    nimcp_gpu_context_t* ctx,
    fin_gpu_rng_t* rng,
    const fin_monte_carlo_gpu_params_t* params,
    fin_monte_carlo_gpu_result_t* result);

/**
 * @brief Run portfolio Monte Carlo simulation
 *
 * Simulates correlated asset returns for a portfolio.
 *
 * @param ctx               GPU context
 * @param rng               GPU RNG state
 * @param weights           Portfolio weights [num_assets]
 * @param expected_returns  Expected returns [num_assets]
 * @param covariance        Covariance matrix [num_assets x num_assets]
 * @param num_assets        Number of assets
 * @param params            Simulation parameters
 * @param result            Output result
 * @return true on success
 */
NIMCP_EXPORT bool fin_monte_carlo_gpu_portfolio(
    nimcp_gpu_context_t* ctx,
    fin_gpu_rng_t* rng,
    const float* weights,
    const float* expected_returns,
    const float* covariance,
    uint32_t num_assets,
    const fin_monte_carlo_gpu_params_t* params,
    fin_monte_carlo_gpu_result_t* result);

/**
 * @brief Free Monte Carlo result resources
 */
NIMCP_EXPORT void fin_monte_carlo_gpu_result_free(
    nimcp_gpu_context_t* ctx,
    fin_monte_carlo_gpu_result_t* result);

/**
 * @brief Price path-dependent options using Monte Carlo
 *
 * Supports Asian, Lookback, and Barrier options.
 *
 * @param ctx    GPU context
 * @param rng    GPU RNG state
 * @param params Option pricing parameters
 * @param result Output result
 * @return true on success
 */
NIMCP_EXPORT bool fin_monte_carlo_gpu_option_price(
    nimcp_gpu_context_t* ctx,
    fin_gpu_rng_t* rng,
    const fin_mc_option_params_t* params,
    fin_mc_option_result_t* result);

/**
 * @brief Run Heston stochastic volatility simulation
 *
 * @param ctx    GPU context
 * @param rng    GPU RNG state
 * @param params Heston model parameters
 * @param result Output result
 * @return true on success
 */
NIMCP_EXPORT bool fin_monte_carlo_gpu_heston(
    nimcp_gpu_context_t* ctx,
    fin_gpu_rng_t* rng,
    const fin_heston_params_t* params,
    fin_monte_carlo_gpu_result_t* result);

/**
 * @brief Run multi-asset correlated simulation
 *
 * @param ctx             GPU context
 * @param rng             GPU RNG state
 * @param params          Multi-asset parameters
 * @param terminal_values Output terminal values [num_paths x n_assets]
 * @return true on success
 */
NIMCP_EXPORT bool fin_monte_carlo_gpu_multi_asset(
    nimcp_gpu_context_t* ctx,
    fin_gpu_rng_t* rng,
    const fin_multi_asset_params_t* params,
    float* terminal_values);

/**
 * @brief Get last Monte Carlo GPU error message
 */
NIMCP_EXPORT const char* fin_monte_carlo_gpu_get_last_error(void);

//=============================================================================
// Portfolio Optimization API
//=============================================================================

/**
 * @brief Run mean-variance portfolio optimization on GPU
 *
 * Uses cuBLAS for matrix operations.
 *
 * @param ctx               GPU context
 * @param expected_returns  Expected returns [params->n_assets]
 * @param covariance_matrix Covariance matrix [n_assets x n_assets]
 * @param params            Optimization parameters (includes n_assets)
 * @param result            Output result
 * @return true on success
 */
NIMCP_EXPORT bool fin_optimization_gpu_mean_variance(
    nimcp_gpu_context_t* ctx,
    const float* expected_returns,
    const float* covariance_matrix,
    const fin_optimization_gpu_params_t* params,
    fin_optimization_gpu_result_t* result);

/**
 * @brief Compute efficient frontier on GPU
 *
 * Computes multiple optimal portfolios along the efficient frontier.
 *
 * @param ctx               GPU context
 * @param expected_returns  Expected returns [params->n_assets]
 * @param covariance_matrix Covariance matrix [n_assets x n_assets]
 * @param num_points        Number of frontier points to compute
 * @param params            Optimization parameters (includes n_assets)
 * @param result            Output result with frontier data
 * @return true on success
 */
NIMCP_EXPORT bool fin_optimization_gpu_efficient_frontier(
    nimcp_gpu_context_t* ctx,
    const float* expected_returns,
    const float* covariance_matrix,
    const fin_optimization_gpu_params_t* params,
    uint32_t num_points,
    fin_efficient_frontier_result_t* result);

/**
 * @brief Free optimization result resources
 */
NIMCP_EXPORT void fin_optimization_gpu_result_free(
    fin_optimization_gpu_result_t* result);

//=============================================================================
// Risk Metrics API
//=============================================================================

/**
 * @brief Compute risk metrics from returns on GPU
 *
 * Uses GPU sorting for VaR/CVaR computation.
 *
 * @param ctx      GPU context
 * @param returns  Return data [params->num_returns]
 * @param params   Risk parameters (includes num_returns)
 * @param result   Output result
 * @return true on success
 */
NIMCP_EXPORT bool fin_risk_gpu_compute(
    nimcp_gpu_context_t* ctx,
    const float* returns,
    const fin_risk_gpu_params_t* params,
    fin_risk_gpu_result_t* result);

/**
 * @brief Compute volatility from price series on GPU
 *
 * @param ctx        GPU context
 * @param prices     Price data [n]
 * @param n          Number of price observations
 * @param method     Volatility estimation method
 * @param volatility Output volatility
 * @return true on success
 */
NIMCP_EXPORT bool fin_risk_gpu_volatility(
    nimcp_gpu_context_t* ctx,
    const float* prices,
    uint32_t n,
    fin_vol_method_t method,
    float* volatility);

/**
 * @brief Compute volatility from OHLC data on GPU
 *
 * For Parkinson, Garman-Klass, and Yang-Zhang estimators.
 *
 * @param ctx          GPU context
 * @param open_prices  Open prices [n]
 * @param high_prices  High prices [n]
 * @param low_prices   Low prices [n]
 * @param close_prices Close prices [n]
 * @param n            Number of observations
 * @param method       Volatility estimation method
 * @param volatility   Output volatility
 * @return true on success
 */
NIMCP_EXPORT bool fin_risk_gpu_volatility_ohlc(
    nimcp_gpu_context_t* ctx,
    const float* open_prices,
    const float* high_prices,
    const float* low_prices,
    const float* close_prices,
    uint32_t n,
    fin_vol_method_t method,
    float* volatility);

/**
 * @brief Get last risk GPU error message
 */
NIMCP_EXPORT const char* fin_risk_gpu_get_last_error(void);

/**
 * @brief Compute VaR using GPU parallel sorting
 *
 * @param ctx        GPU context
 * @param returns    Return data [num_returns] (device pointer)
 * @param num_returns Number of returns
 * @param confidence Confidence level (0.95 or 0.99)
 * @return VaR value
 */
NIMCP_EXPORT float fin_risk_gpu_var(
    nimcp_gpu_context_t* ctx,
    const float* returns,
    uint32_t num_returns,
    float confidence);

/**
 * @brief Compute CVaR using GPU parallel sorting
 *
 * @param ctx        GPU context
 * @param returns    Return data [num_returns] (device pointer)
 * @param num_returns Number of returns
 * @param confidence Confidence level
 * @return CVaR value
 */
NIMCP_EXPORT float fin_risk_gpu_cvar(
    nimcp_gpu_context_t* ctx,
    const float* returns,
    uint32_t num_returns,
    float confidence);

/**
 * @brief Batch VaR computation for multiple portfolios
 *
 * @param ctx           GPU context
 * @param returns       Return data [num_portfolios x num_returns]
 * @param num_portfolios Number of portfolios
 * @param num_returns   Returns per portfolio
 * @param confidence    Confidence level
 * @param out_var       Output VaR [num_portfolios]
 * @return true on success
 */
NIMCP_EXPORT bool fin_risk_gpu_var_batch(
    nimcp_gpu_context_t* ctx,
    const float* returns,
    uint32_t num_portfolios,
    uint32_t num_returns,
    float confidence,
    float* out_var);

//=============================================================================
// Derivatives Pricing API
//=============================================================================

/**
 * @brief Price option using GPU binomial tree
 *
 * @param ctx    GPU context
 * @param params Pricing parameters
 * @param result Output result
 * @return true on success
 */
NIMCP_EXPORT bool fin_derivatives_gpu_binomial_tree(
    nimcp_gpu_context_t* ctx,
    const fin_derivatives_gpu_params_t* params,
    fin_derivatives_gpu_result_t* result);

/**
 * @brief Price option using GPU Monte Carlo
 *
 * @param ctx    GPU context
 * @param rng    GPU RNG state
 * @param params Pricing parameters
 * @param result Output result
 * @return true on success
 */
NIMCP_EXPORT bool fin_derivatives_gpu_monte_carlo(
    nimcp_gpu_context_t* ctx,
    fin_gpu_rng_t* rng,
    const fin_derivatives_gpu_params_t* params,
    fin_derivatives_gpu_result_t* result);

/**
 * @brief Batch option pricing on GPU
 *
 * Price multiple options with same underlying.
 *
 * @param ctx        GPU context
 * @param strikes    Strike prices [num_options]
 * @param expiries   Times to expiry [num_options]
 * @param types      Option types [num_options]
 * @param num_options Number of options
 * @param spot       Spot price
 * @param rate       Risk-free rate
 * @param volatility Volatility
 * @param prices     Output prices [num_options]
 * @param deltas     Output deltas [num_options] (optional)
 * @return true on success
 */
NIMCP_EXPORT bool fin_derivatives_gpu_batch_price(
    nimcp_gpu_context_t* ctx,
    const float* strikes,
    const float* expiries,
    const fin_option_type_t* types,
    uint32_t num_options,
    float spot,
    float rate,
    float volatility,
    float* prices,
    float* deltas);

/**
 * @brief Compute implied volatility using GPU
 *
 * @param ctx          GPU context
 * @param market_price Observed market price
 * @param params       Option parameters (volatility field ignored)
 * @return Implied volatility
 */
NIMCP_EXPORT float fin_derivatives_gpu_implied_vol(
    nimcp_gpu_context_t* ctx,
    float market_price,
    const fin_derivatives_gpu_params_t* params);

//=============================================================================
// Statistics and Utilities
//=============================================================================

/**
 * @brief Get GPU financial statistics
 */
NIMCP_EXPORT int fin_gpu_get_stats(fin_gpu_stats_t* stats);

/**
 * @brief Reset GPU financial statistics
 */
NIMCP_EXPORT void fin_gpu_reset_stats(void);

/**
 * @brief Get last GPU financial error message
 */
NIMCP_EXPORT const char* fin_gpu_get_last_error(void);

/**
 * @brief Check if GPU financial acceleration is available
 */
NIMCP_EXPORT bool fin_gpu_is_available(void);

/**
 * @brief Get recommended Monte Carlo path count for current GPU
 */
NIMCP_EXPORT uint32_t fin_gpu_recommended_mc_paths(nimcp_gpu_context_t* ctx);

/**
 * @brief Get maximum supported assets for optimization
 */
NIMCP_EXPORT uint32_t fin_gpu_max_optimization_assets(nimcp_gpu_context_t* ctx);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_FINANCIAL_GPU_H */
