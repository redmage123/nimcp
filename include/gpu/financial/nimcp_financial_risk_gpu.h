//=============================================================================
// nimcp_financial_risk_gpu.h - GPU Risk Metrics Computation
//=============================================================================
/**
 * @file nimcp_financial_risk_gpu.h
 * @brief GPU-accelerated risk metrics: VaR, CVaR, drawdown, volatility
 *
 * WHAT: Parallel computation of risk metrics using GPU sorting and reduction
 * WHY:  Fast VaR/CVaR for large return datasets, batch risk computation
 * HOW:  Bitonic sort on GPU, parallel reduction for statistics
 *
 * METRICS:
 *   - Value at Risk (VaR) - Historical, Parametric, Monte Carlo
 *   - Conditional VaR (Expected Shortfall)
 *   - Maximum Drawdown
 *   - Volatility (rolling, EWMA, GARCH)
 *   - Risk-adjusted returns (Sharpe, Sortino, Calmar)
 *
 * @author NIMCP Development Team
 * @date 2025
 */

#ifndef NIMCP_FINANCIAL_RISK_GPU_H
#define NIMCP_FINANCIAL_RISK_GPU_H

#include "gpu/financial/nimcp_financial_gpu.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Risk Computation Types
//=============================================================================

/**
 * @brief VaR computation method
 */
typedef enum {
    FIN_VAR_METHOD_HISTORICAL,              /**< Historical simulation */
    FIN_VAR_METHOD_PARAMETRIC,              /**< Parametric (normal) */
    FIN_VAR_METHOD_CORNISH_FISHER,          /**< Cornish-Fisher expansion */
    FIN_VAR_METHOD_MONTE_CARLO,             /**< Monte Carlo simulation */
    FIN_VAR_METHOD_COUNT
} fin_var_method_t;

/* Note: fin_vol_method_t is a typedef alias to fin_volatility_type_t
   defined in nimcp_financial_gpu.h */

/**
 * @brief Extended risk parameters
 */
typedef struct {
    fin_risk_gpu_params_t base;             /**< Base parameters */

    /* VaR settings */
    fin_var_method_t var_method;            /**< VaR computation method */
    uint32_t var_lookback;                  /**< Historical lookback period */

    /* Volatility settings */
    fin_volatility_type_t vol_method;       /**< Volatility computation method */
    float ewma_lambda;                      /**< EWMA decay factor (0.94 typical) */
    uint32_t rolling_window;                /**< Rolling window size */

    /* Drawdown settings */
    bool compute_peak_to_trough;            /**< Compute peak-to-trough drawdown */
    bool compute_recovery_time;             /**< Compute time to recovery */

    /* Tail risk */
    bool compute_tail_risk;                 /**< Compute tail risk metrics */
    float tail_threshold;                   /**< Tail threshold (e.g., 5th percentile) */

    /* Stress testing */
    bool compute_stress_var;                /**< Compute stressed VaR */
    float stress_multiplier;                /**< Stress scenario multiplier */
} fin_risk_extended_params_t;

/**
 * @brief Extended risk result
 */
typedef struct {
    fin_risk_gpu_result_t base;             /**< Base result */

    /* VaR by method */
    float var_historical;                   /**< Historical VaR */
    float var_parametric;                   /**< Parametric VaR */
    float var_cornish_fisher;               /**< Cornish-Fisher VaR */

    /* Volatility details */
    float vol_ewma;                         /**< EWMA volatility */
    float vol_parkinson;                    /**< Parkinson volatility */
    float vol_realized;                     /**< Realized volatility */

    /* Drawdown details */
    float max_drawdown;                     /**< Maximum drawdown */
    float avg_drawdown;                     /**< Average drawdown */
    uint32_t max_drawdown_duration;         /**< Duration of max drawdown */
    uint32_t recovery_time;                 /**< Time to recovery */

    /* Tail risk */
    float expected_tail_loss;               /**< Expected loss in tail */
    float tail_index;                       /**< Tail index (from GPD) */
    float tail_probability;                 /**< Probability of extreme loss */

    /* Additional ratios */
    float calmar_ratio;                     /**< Return / max drawdown */
    float omega_ratio;                      /**< Upside / downside probability */
    float ulcer_index;                      /**< Ulcer performance index */

    /* Stress VaR */
    float stressed_var_95;                  /**< Stressed 95% VaR */
    float stressed_var_99;                  /**< Stressed 99% VaR */
} fin_risk_extended_result_t;

/**
 * @brief Rolling risk result (for time series)
 */
typedef struct {
    float* var_series;                      /**< Rolling VaR [num_points] */
    float* cvar_series;                     /**< Rolling CVaR [num_points] */
    float* vol_series;                      /**< Rolling volatility [num_points] */
    float* drawdown_series;                 /**< Rolling drawdown [num_points] */
    uint32_t num_points;                    /**< Number of output points */
} fin_risk_rolling_result_t;

/**
 * @brief Default extended risk parameters
 */
static inline fin_risk_extended_params_t fin_risk_extended_params_default(void)
{
    fin_risk_extended_params_t params = {
        .base = {
            .confidence_95 = 0.95f,
            .confidence_99 = 0.99f,
            .compute_var = true,
            .compute_cvar = true,
            .compute_drawdown = true,
            .compute_volatility = true,
            .risk_free_rate = 0.02f,
            .annualization_factor = 252
        },
        .var_method = FIN_VAR_METHOD_HISTORICAL,
        .var_lookback = 252,
        .vol_method = FIN_VOL_EWMA,
        .ewma_lambda = 0.94f,
        .rolling_window = 21,
        .compute_peak_to_trough = true,
        .compute_recovery_time = true,
        .compute_tail_risk = false,
        .tail_threshold = 0.05f,
        .compute_stress_var = false,
        .stress_multiplier = 1.5f
    };
    return params;
}

//=============================================================================
// Advanced Risk API
//=============================================================================

/**
 * @brief Compute rolling risk metrics on GPU
 *
 * Computes rolling VaR, CVaR, and volatility over a sliding window.
 *
 * @param ctx      GPU context
 * @param returns  Return data [num_returns]
 * @param num_returns Number of returns
 * @param window   Rolling window size
 * @param params   Risk parameters
 * @param result   Rolling result (var_series, cvar_series, vol_series allocated)
 * @return true on success
 */
NIMCP_EXPORT bool fin_risk_gpu_rolling(
    nimcp_gpu_context_t* ctx,
    const float* returns,
    uint32_t num_returns,
    uint32_t window,
    const fin_risk_gpu_params_t* params,
    fin_risk_rolling_result_t* result);

/**
 * @brief Compute extended risk metrics on GPU
 *
 * @param ctx      GPU context
 * @param returns  Return data [num_returns]
 * @param num_returns Number of returns
 * @param params   Extended parameters
 * @param result   Extended result
 * @return true on success
 */
NIMCP_EXPORT bool fin_risk_gpu_extended(
    nimcp_gpu_context_t* ctx,
    const float* returns,
    uint32_t num_returns,
    const fin_risk_extended_params_t* params,
    fin_risk_extended_result_t* result);

/**
 * @brief Compute extended rolling risk metrics on GPU
 *
 * Returns comprehensive rolling metrics including VaR, CVaR, volatility,
 * and drawdown series.
 *
 * @param ctx      GPU context
 * @param returns  Return data [num_returns]
 * @param num_returns Number of returns
 * @param window   Rolling window size
 * @param params   Base risk parameters
 * @param result   Rolling result (arrays allocated)
 * @return true on success
 *
 * @note For simpler rolling VaR/volatility, use fin_risk_gpu_rolling()
 *       from nimcp_financial_gpu.h
 */
NIMCP_EXPORT bool fin_risk_gpu_rolling_extended(
    nimcp_gpu_context_t* ctx,
    const float* returns,
    uint32_t num_returns,
    uint32_t window,
    const fin_risk_gpu_params_t* params,
    fin_risk_rolling_result_t* result);

/**
 * @brief Compute parametric VaR on GPU
 *
 * Assumes normal distribution.
 *
 * @param ctx        GPU context
 * @param returns    Return data [num_returns]
 * @param num_returns Number of returns
 * @param confidence Confidence level
 * @return Parametric VaR
 */
NIMCP_EXPORT float fin_risk_gpu_var_parametric(
    nimcp_gpu_context_t* ctx,
    const float* returns,
    uint32_t num_returns,
    float confidence);

/**
 * @brief Compute Cornish-Fisher VaR on GPU
 *
 * Adjusts for skewness and kurtosis.
 *
 * @param ctx        GPU context
 * @param returns    Return data [num_returns]
 * @param num_returns Number of returns
 * @param confidence Confidence level
 * @return Cornish-Fisher VaR
 */
NIMCP_EXPORT float fin_risk_gpu_var_cornish_fisher(
    nimcp_gpu_context_t* ctx,
    const float* returns,
    uint32_t num_returns,
    float confidence);

/**
 * @brief Compute maximum drawdown on GPU
 *
 * @param ctx        GPU context
 * @param prices     Price series [num_prices]
 * @param num_prices Number of prices
 * @param out_start  Output: drawdown start index
 * @param out_end    Output: drawdown end index
 * @return Maximum drawdown (negative value)
 */
NIMCP_EXPORT float fin_risk_gpu_max_drawdown(
    nimcp_gpu_context_t* ctx,
    const float* prices,
    uint32_t num_prices,
    uint32_t* out_start,
    uint32_t* out_end);

/**
 * @brief Compute EWMA volatility on GPU
 *
 * @param ctx        GPU context
 * @param returns    Return data [num_returns]
 * @param num_returns Number of returns
 * @param lambda     Decay factor (e.g., 0.94)
 * @param out_vol    Output volatility series [num_returns]
 * @return Final EWMA volatility
 */
NIMCP_EXPORT float fin_risk_gpu_ewma_volatility(
    nimcp_gpu_context_t* ctx,
    const float* returns,
    uint32_t num_returns,
    float lambda,
    float* out_vol);

/**
 * @brief Compute portfolio VaR using delta-normal method
 *
 * @param ctx        GPU context
 * @param weights    Portfolio weights [num_assets]
 * @param covariance Covariance matrix [num_assets x num_assets]
 * @param num_assets Number of assets
 * @param confidence Confidence level
 * @param horizon_days VaR horizon in days
 * @return Portfolio VaR
 */
NIMCP_EXPORT float fin_risk_gpu_portfolio_var_delta_normal(
    nimcp_gpu_context_t* ctx,
    const float* weights,
    const float* covariance,
    uint32_t num_assets,
    float confidence,
    uint32_t horizon_days);

//=============================================================================
// Batch Risk API
//=============================================================================

/**
 * @brief Batch risk computation for multiple portfolios
 *
 * @param ctx           GPU context
 * @param returns       Return data [num_portfolios x num_returns]
 * @param num_portfolios Number of portfolios
 * @param num_returns   Returns per portfolio
 * @param params        Risk parameters
 * @param results       Output results [num_portfolios]
 * @return true on success
 */
NIMCP_EXPORT bool fin_risk_gpu_batch(
    nimcp_gpu_context_t* ctx,
    const float* returns,
    uint32_t num_portfolios,
    uint32_t num_returns,
    const fin_risk_gpu_params_t* params,
    fin_risk_gpu_result_t* results);

/**
 * @brief Compute correlation matrix on GPU
 *
 * @param ctx        GPU context
 * @param returns    Return data [num_assets x num_returns]
 * @param num_assets Number of assets
 * @param num_returns Number of return observations
 * @param correlation Output correlation [num_assets x num_assets]
 * @return true on success
 */
NIMCP_EXPORT bool fin_risk_gpu_correlation_matrix(
    nimcp_gpu_context_t* ctx,
    const float* returns,
    uint32_t num_assets,
    uint32_t num_returns,
    float* correlation);

/**
 * @brief Compute covariance matrix on GPU
 *
 * @param ctx        GPU context
 * @param returns    Return data [num_assets x num_returns]
 * @param num_assets Number of assets
 * @param num_returns Number of return observations
 * @param covariance Output covariance [num_assets x num_assets]
 * @return true on success
 */
NIMCP_EXPORT bool fin_risk_gpu_covariance_matrix(
    nimcp_gpu_context_t* ctx,
    const float* returns,
    uint32_t num_assets,
    uint32_t num_returns,
    float* covariance);

//=============================================================================
// GPU Sorting Utilities
//=============================================================================

/**
 * @brief Sort array on GPU (in-place)
 *
 * Uses bitonic sort for GPU efficiency.
 *
 * @param ctx        GPU context
 * @param data       Data to sort [n] (device, modified in-place)
 * @param n          Number of elements
 * @param ascending  True for ascending, false for descending
 * @return true on success
 */
NIMCP_EXPORT bool fin_risk_gpu_sort(
    nimcp_gpu_context_t* ctx,
    float* data,
    uint32_t n,
    bool ascending);

/**
 * @brief Get percentile from sorted data on GPU
 *
 * @param ctx        GPU context
 * @param sorted     Sorted data [n] (device)
 * @param n          Number of elements
 * @param percentile Percentile (0-100)
 * @return Percentile value
 */
NIMCP_EXPORT float fin_risk_gpu_percentile(
    nimcp_gpu_context_t* ctx,
    const float* sorted,
    uint32_t n,
    float percentile);

/**
 * @brief Free rolling result resources
 */
NIMCP_EXPORT void fin_risk_rolling_result_free(
    fin_risk_rolling_result_t* result);

/**
 * @brief Free extended result resources
 */
NIMCP_EXPORT void fin_risk_extended_result_free(
    fin_risk_extended_result_t* result);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_FINANCIAL_RISK_GPU_H */
