//=============================================================================
// nimcp_financial_monte_carlo_gpu.h - GPU Monte Carlo Simulation
//=============================================================================
/**
 * @file nimcp_financial_monte_carlo_gpu.h
 * @brief Advanced GPU Monte Carlo simulation for financial modeling
 *
 * WHAT: GPU-accelerated Monte Carlo with multiple stochastic processes
 * WHY:  100K+ paths in milliseconds for VaR, option pricing, scenario analysis
 * HOW:  cuRAND for parallel RNG, specialized kernels per process type
 *
 * PROCESSES SUPPORTED:
 *   - Geometric Brownian Motion (GBM)
 *   - Heston stochastic volatility
 *   - Merton jump-diffusion
 *   - Correlated multi-asset
 *
 * @author NIMCP Development Team
 * @date 2025
 */

#ifndef NIMCP_FINANCIAL_MONTE_CARLO_GPU_H
#define NIMCP_FINANCIAL_MONTE_CARLO_GPU_H

#include "gpu/financial/nimcp_financial_gpu.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Advanced Monte Carlo Types
//=============================================================================

/**
 * @brief Variance reduction method
 */
typedef enum {
    FIN_MC_VAR_REDUCTION_NONE,              /**< No variance reduction */
    FIN_MC_VAR_REDUCTION_ANTITHETIC,        /**< Antithetic variates */
    FIN_MC_VAR_REDUCTION_CONTROL_VARIATE,   /**< Control variate */
    FIN_MC_VAR_REDUCTION_IMPORTANCE,        /**< Importance sampling */
    FIN_MC_VAR_REDUCTION_STRATIFIED,        /**< Stratified sampling */
    FIN_MC_VAR_REDUCTION_COUNT
} fin_mc_var_reduction_t;

/**
 * @brief Path-dependent option type
 */
typedef enum {
    FIN_MC_OPTION_EUROPEAN,                 /**< European (terminal only) */
    FIN_MC_OPTION_ASIAN_ARITHMETIC,         /**< Asian arithmetic average */
    FIN_MC_OPTION_ASIAN_GEOMETRIC,          /**< Asian geometric average */
    FIN_MC_OPTION_LOOKBACK_FIXED,           /**< Lookback with fixed strike */
    FIN_MC_OPTION_LOOKBACK_FLOATING,        /**< Lookback with floating strike */
    FIN_MC_OPTION_BARRIER_UP_IN,            /**< Up-and-in barrier */
    FIN_MC_OPTION_BARRIER_UP_OUT,           /**< Up-and-out barrier */
    FIN_MC_OPTION_BARRIER_DOWN_IN,          /**< Down-and-in barrier */
    FIN_MC_OPTION_BARRIER_DOWN_OUT,         /**< Down-and-out barrier */
    FIN_MC_OPTION_TYPE_COUNT
} fin_mc_option_type_t;

/**
 * @brief Extended Monte Carlo parameters for path-dependent options
 */
typedef struct {
    fin_monte_carlo_gpu_params_t base;      /**< Base MC parameters */

    /* Variance reduction */
    fin_mc_var_reduction_t var_reduction;   /**< Variance reduction method */
    float* control_variate_prices;          /**< Known prices for CV (device) */

    /* Path-dependent option */
    fin_mc_option_type_t option_type;       /**< Path-dependent option type */
    float barrier;                          /**< Barrier level (for barrier opts) */
    bool monitor_continuous;                /**< Continuous vs discrete monitoring */
    uint32_t monitoring_frequency;          /**< Monitoring freq (if discrete) */

    /* Multi-asset correlation */
    float* correlation_matrix;              /**< Correlation [n x n] (device) */
    float* cholesky_factor;                 /**< Cholesky decomposition (device) */
    uint32_t num_correlated_assets;         /**< Number of correlated assets */

    /* Greeks via finite difference */
    bool compute_delta;                     /**< Compute delta via bump */
    bool compute_gamma;                     /**< Compute gamma via bump */
    bool compute_vega;                      /**< Compute vega via bump */
    float bump_size;                        /**< Bump size for FD Greeks */
} fin_mc_extended_params_t;

/**
 * @brief Extended Monte Carlo result with Greeks
 */
typedef struct {
    fin_monte_carlo_gpu_result_t base;      /**< Base result */

    /* Path-dependent results */
    float option_price;                     /**< Option price */
    float option_std_error;                 /**< Monte Carlo standard error */

    /* Greeks */
    float delta;                            /**< Delta */
    float gamma;                            /**< Gamma */
    float vega;                             /**< Vega */
    float theta;                            /**< Theta (via time bump) */

    /* Barrier option specifics */
    float barrier_hit_probability;          /**< P(barrier hit) */

    /* Asian option specifics */
    float average_price;                    /**< Average price (Asian) */
} fin_mc_extended_result_t;

/**
 * @brief Default extended MC parameters
 */
static inline fin_mc_extended_params_t fin_mc_extended_params_default(void)
{
    fin_mc_extended_params_t params = {
        .base = {
            .process_type = FIN_GPU_PROCESS_GBM,
            .initial_value = 100.0f,
            .drift = 0.05f,
            .volatility = 0.2f,
            .risk_free_rate = 0.02f,
            .horizon_years = 1.0f,
            .num_paths = 100000,
            .num_steps = 252,
            .antithetic = true,
            .control_variate = false,
            .store_paths = false
        },
        .var_reduction = FIN_MC_VAR_REDUCTION_ANTITHETIC,
        .control_variate_prices = NULL,
        .option_type = FIN_MC_OPTION_EUROPEAN,
        .barrier = 0.0f,
        .monitor_continuous = true,
        .monitoring_frequency = 252,
        .correlation_matrix = NULL,
        .cholesky_factor = NULL,
        .num_correlated_assets = 0,
        .compute_delta = true,
        .compute_gamma = false,
        .compute_vega = true,
        .bump_size = 0.01f
    };
    return params;
}

//=============================================================================
// Advanced Monte Carlo API
//=============================================================================

/**
 * @brief Run Heston model Monte Carlo simulation
 *
 * @param ctx    GPU context
 * @param rng    GPU RNG state
 * @param params Extended parameters (with Heston params in base)
 * @param result Output result
 * @return true on success
 */
NIMCP_EXPORT bool fin_mc_gpu_heston(
    nimcp_gpu_context_t* ctx,
    fin_gpu_rng_t* rng,
    const fin_mc_extended_params_t* params,
    fin_mc_extended_result_t* result);

/**
 * @brief Run jump-diffusion Monte Carlo simulation
 *
 * @param ctx    GPU context
 * @param rng    GPU RNG state
 * @param params Extended parameters (with jump params in base)
 * @param result Output result
 * @return true on success
 */
NIMCP_EXPORT bool fin_mc_gpu_jump_diffusion(
    nimcp_gpu_context_t* ctx,
    fin_gpu_rng_t* rng,
    const fin_mc_extended_params_t* params,
    fin_mc_extended_result_t* result);

/**
 * @brief Price path-dependent option via Monte Carlo
 *
 * @param ctx    GPU context
 * @param rng    GPU RNG state
 * @param spot   Current spot price
 * @param strike Strike price
 * @param params Extended parameters (option_type specifies path dependence)
 * @param is_call True for call, false for put
 * @param result Output result
 * @return true on success
 */
NIMCP_EXPORT bool fin_mc_gpu_path_dependent_option(
    nimcp_gpu_context_t* ctx,
    fin_gpu_rng_t* rng,
    float spot,
    float strike,
    const fin_mc_extended_params_t* params,
    bool is_call,
    fin_mc_extended_result_t* result);

/**
 * @brief Run correlated multi-asset Monte Carlo
 *
 * @param ctx               GPU context
 * @param rng               GPU RNG state
 * @param initial_values    Starting prices [num_assets]
 * @param drifts            Drift per asset [num_assets]
 * @param volatilities      Volatility per asset [num_assets]
 * @param correlation       Correlation matrix [num_assets x num_assets]
 * @param num_assets        Number of correlated assets
 * @param params            Base parameters (process settings)
 * @param terminal_values   Output [num_paths x num_assets]
 * @return true on success
 */
NIMCP_EXPORT bool fin_mc_gpu_correlated_assets(
    nimcp_gpu_context_t* ctx,
    fin_gpu_rng_t* rng,
    const float* initial_values,
    const float* drifts,
    const float* volatilities,
    const float* correlation,
    uint32_t num_assets,
    const fin_monte_carlo_gpu_params_t* params,
    float* terminal_values);

/**
 * @brief Compute Cholesky decomposition on GPU
 *
 * For correlated asset simulation.
 *
 * @param ctx         GPU context
 * @param correlation Correlation matrix [n x n]
 * @param n           Matrix dimension
 * @param cholesky    Output Cholesky factor [n x n]
 * @return true on success
 */
NIMCP_EXPORT bool fin_mc_gpu_cholesky(
    nimcp_gpu_context_t* ctx,
    const float* correlation,
    uint32_t n,
    float* cholesky);

/**
 * @brief Free extended result resources
 */
NIMCP_EXPORT void fin_mc_extended_result_free(
    nimcp_gpu_context_t* ctx,
    fin_mc_extended_result_t* result);

//=============================================================================
// RNG Utilities
//=============================================================================

/**
 * @brief Generate uniform random numbers on GPU
 *
 * @param rng    GPU RNG state
 * @param output Output array [n] (device)
 * @param n      Number of random numbers
 * @return true on success
 */
NIMCP_EXPORT bool fin_gpu_rng_uniform(
    fin_gpu_rng_t* rng,
    float* output,
    uint32_t n);

/**
 * @brief Generate normal random numbers on GPU
 *
 * @param rng    GPU RNG state
 * @param output Output array [n] (device)
 * @param n      Number of random numbers
 * @return true on success
 */
NIMCP_EXPORT bool fin_gpu_rng_normal(
    fin_gpu_rng_t* rng,
    float* output,
    uint32_t n);

/**
 * @brief Generate correlated normal random numbers
 *
 * @param rng       GPU RNG state
 * @param cholesky  Cholesky factor [n x n] (device)
 * @param n         Number of correlated variables
 * @param num_sets  Number of random vector sets
 * @param output    Output [num_sets x n] (device)
 * @return true on success
 */
NIMCP_EXPORT bool fin_gpu_rng_correlated_normal(
    fin_gpu_rng_t* rng,
    const float* cholesky,
    uint32_t n,
    uint32_t num_sets,
    float* output);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_FINANCIAL_MONTE_CARLO_GPU_H */
