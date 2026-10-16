//=============================================================================
// nimcp_financial_derivatives_gpu.h - GPU Derivatives Pricing
//=============================================================================
/**
 * @file nimcp_financial_derivatives_gpu.h
 * @brief GPU-accelerated derivatives pricing: binomial trees, Greeks
 *
 * WHAT: Parallel option pricing using binomial trees and finite difference
 * WHY:  Fast American option pricing, batch Greeks computation
 * HOW:  Level-parallel binomial tree, GPU finite difference for Greeks
 *
 * INSTRUMENTS:
 *   - European options (closed-form + GPU verification)
 *   - American options (binomial tree)
 *   - Bermudan options
 *   - Greeks (Delta, Gamma, Theta, Vega, Rho)
 *   - Implied volatility (Newton-Raphson on GPU)
 *
 * @author NIMCP Development Team
 * @date 2025
 */

#ifndef NIMCP_FINANCIAL_DERIVATIVES_GPU_H
#define NIMCP_FINANCIAL_DERIVATIVES_GPU_H

#include "gpu/financial/nimcp_financial_gpu.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Derivatives Types
//=============================================================================

/**
 * @brief Greeks selection flags
 */
typedef enum {
    FIN_GREEK_DELTA     = 0x01,             /**< First derivative w.r.t. spot */
    FIN_GREEK_GAMMA     = 0x02,             /**< Second derivative w.r.t. spot */
    FIN_GREEK_THETA     = 0x04,             /**< Derivative w.r.t. time */
    FIN_GREEK_VEGA      = 0x08,             /**< Derivative w.r.t. volatility */
    FIN_GREEK_RHO       = 0x10,             /**< Derivative w.r.t. rate */
    FIN_GREEK_VANNA     = 0x20,             /**< Cross derivative: d(Delta)/d(Vol) */
    FIN_GREEK_CHARM     = 0x40,             /**< Cross derivative: d(Delta)/d(Time) */
    FIN_GREEK_VOLGA     = 0x80,             /**< Second derivative w.r.t. vol */
    FIN_GREEK_ALL       = 0xFF              /**< All Greeks */
} fin_greek_flags_t;

/**
 * @brief Extended derivatives parameters
 */
typedef struct {
    fin_derivatives_gpu_params_t base;      /**< Base parameters */

    /* Greeks computation */
    uint32_t greek_flags;                   /**< Which Greeks to compute */
    float delta_bump;                       /**< Bump size for delta (e.g., 0.01) */
    float vol_bump;                         /**< Bump size for vega (e.g., 0.01) */
    float time_bump;                        /**< Time bump for theta (e.g., 1/252) */
    float rate_bump;                        /**< Rate bump for rho (e.g., 0.0001) */

    /* American option specifics */
    bool use_smooth_payoff;                 /**< Smooth early exercise boundary */
    float smoothing_width;                  /**< Smoothing width */

    /* Implied volatility */
    float iv_initial_guess;                 /**< Initial IV guess */
    float iv_tolerance;                     /**< Newton convergence tolerance */
    uint32_t iv_max_iterations;             /**< Max Newton iterations */

    /* Bermudan options */
    uint32_t* exercise_dates;               /**< Exercise date indices */
    uint32_t num_exercise_dates;            /**< Number of exercise dates */
} fin_deriv_extended_params_t;

/**
 * @brief Extended derivatives result
 */
typedef struct {
    fin_derivatives_gpu_result_t base;      /**< Base result */

    /* Additional Greeks */
    float vanna;                            /**< d(Delta)/d(Vol) */
    float charm;                            /**< d(Delta)/d(Time) */
    float volga;                            /**< d^2(Price)/d(Vol)^2 */
    float veta;                             /**< d(Vega)/d(Time) */
    float speed;                            /**< d(Gamma)/d(Spot) */
    float zomma;                            /**< d(Gamma)/d(Vol) */
    float color;                            /**< d(Gamma)/d(Time) */

    /* American option specifics */
    float* exercise_boundary;               /**< Early exercise boundary [num_steps] */
    uint32_t optimal_exercise_step;         /**< First step where exercise optimal */

    /* Tree diagnostics */
    float tree_convergence_error;           /**< Difference from finer tree */
} fin_deriv_extended_result_t;

/**
 * @brief Option chain for batch pricing
 */
typedef struct {
    float* strikes;                         /**< Strike prices [num_options] */
    float* expiries;                        /**< Times to expiry [num_options] */
    fin_option_type_t* types;               /**< Call/Put [num_options] */
    fin_option_style_t* styles;             /**< Exercise style [num_options] */
    uint32_t num_options;                   /**< Number of options */
    float spot;                             /**< Current spot price */
    float rate;                             /**< Risk-free rate */
    float dividend;                         /**< Dividend yield */
} fin_option_chain_t;

/**
 * @brief Option chain results
 */
typedef struct {
    float* prices;                          /**< Option prices [num_options] */
    float* deltas;                          /**< Deltas [num_options] */
    float* gammas;                          /**< Gammas [num_options] */
    float* thetas;                          /**< Thetas [num_options] */
    float* vegas;                           /**< Vegas [num_options] */
    float* implied_vols;                    /**< Implied vols [num_options] (if market prices given) */
    uint32_t num_options;                   /**< Number of options */
} fin_option_chain_result_t;

/**
 * @brief Default extended derivatives parameters
 */
static inline fin_deriv_extended_params_t fin_deriv_extended_params_default(void)
{
    fin_deriv_extended_params_t params = {
        .base = {
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
        },
        .greek_flags = FIN_GREEK_DELTA | FIN_GREEK_GAMMA | FIN_GREEK_THETA |
                       FIN_GREEK_VEGA | FIN_GREEK_RHO,
        .delta_bump = 0.01f,
        .vol_bump = 0.01f,
        .time_bump = 1.0f / 252.0f,
        .rate_bump = 0.0001f,
        .use_smooth_payoff = false,
        .smoothing_width = 0.01f,
        .iv_initial_guess = 0.2f,
        .iv_tolerance = 1e-6f,
        .iv_max_iterations = 100,
        .exercise_dates = NULL,
        .num_exercise_dates = 0
    };
    return params;
}

//=============================================================================
// Advanced Pricing API
//=============================================================================

/**
 * @brief Price option with extended Greeks on GPU
 *
 * @param ctx    GPU context
 * @param params Extended parameters
 * @param result Extended result
 * @return true on success
 */
NIMCP_EXPORT bool fin_deriv_gpu_price_extended(
    nimcp_gpu_context_t* ctx,
    const fin_deriv_extended_params_t* params,
    fin_deriv_extended_result_t* result);

/**
 * @brief Price American option with exercise boundary
 *
 * @param ctx    GPU context
 * @param params Extended parameters (style must be American)
 * @param result Extended result (includes exercise_boundary)
 * @return true on success
 */
NIMCP_EXPORT bool fin_deriv_gpu_american_with_boundary(
    nimcp_gpu_context_t* ctx,
    const fin_deriv_extended_params_t* params,
    fin_deriv_extended_result_t* result);

/**
 * @brief Price Bermudan option on GPU
 *
 * @param ctx    GPU context
 * @param params Extended params (must set exercise_dates)
 * @param result Extended result
 * @return true on success
 */
NIMCP_EXPORT bool fin_deriv_gpu_bermudan(
    nimcp_gpu_context_t* ctx,
    const fin_deriv_extended_params_t* params,
    fin_deriv_extended_result_t* result);

/**
 * @brief Price option chain on GPU
 *
 * Prices multiple options with same underlying.
 *
 * @param ctx        GPU context
 * @param chain      Option chain specification
 * @param volatility Volatility (or volatilities if smile)
 * @param result     Chain results
 * @return true on success
 */
NIMCP_EXPORT bool fin_deriv_gpu_price_chain(
    nimcp_gpu_context_t* ctx,
    const fin_option_chain_t* chain,
    float volatility,
    fin_option_chain_result_t* result);

/**
 * @brief Compute implied volatility surface on GPU
 *
 * @param ctx            GPU context
 * @param chain          Option chain specification
 * @param market_prices  Market prices [num_options]
 * @param implied_vols   Output implied vols [num_options]
 * @return true on success
 */
NIMCP_EXPORT bool fin_deriv_gpu_implied_vol_surface(
    nimcp_gpu_context_t* ctx,
    const fin_option_chain_t* chain,
    const float* market_prices,
    float* implied_vols);

//=============================================================================
// Greeks Computation API
//=============================================================================

/**
 * @brief Compute single Greek on GPU
 *
 * @param ctx      GPU context
 * @param params   Base parameters
 * @param greek    Which Greek to compute
 * @param bump     Bump size for finite difference
 * @return Greek value
 */
NIMCP_EXPORT float fin_deriv_gpu_greek(
    nimcp_gpu_context_t* ctx,
    const fin_derivatives_gpu_params_t* params,
    fin_greek_flags_t greek,
    float bump);

/**
 * @brief Compute Greeks for option chain
 *
 * @param ctx        GPU context
 * @param chain      Option chain
 * @param volatility Volatility
 * @param greek_flags Which Greeks to compute
 * @param result     Chain results (only requested Greeks filled)
 * @return true on success
 */
NIMCP_EXPORT bool fin_deriv_gpu_greeks_chain(
    nimcp_gpu_context_t* ctx,
    const fin_option_chain_t* chain,
    float volatility,
    uint32_t greek_flags,
    fin_option_chain_result_t* result);

//=============================================================================
// Implied Volatility API
//=============================================================================

/**
 * @brief Compute implied volatility on GPU
 *
 * Uses Newton-Raphson with GPU acceleration.
 *
 * @param ctx          GPU context
 * @param market_price Observed market price
 * @param params       Option parameters
 * @return Implied volatility (or NaN if not found)
 */
NIMCP_EXPORT float fin_deriv_gpu_implied_vol(
    nimcp_gpu_context_t* ctx,
    float market_price,
    const fin_derivatives_gpu_params_t* params);

/**
 * @brief Batch implied volatility computation
 *
 * @param ctx           GPU context
 * @param market_prices Market prices [num_options]
 * @param chain         Option chain specification
 * @param implied_vols  Output implied vols [num_options]
 * @return true on success
 */
NIMCP_EXPORT bool fin_deriv_gpu_implied_vol_batch(
    nimcp_gpu_context_t* ctx,
    const float* market_prices,
    const fin_option_chain_t* chain,
    float* implied_vols);

//=============================================================================
// Black-Scholes GPU Utilities
//=============================================================================

/**
 * @brief Compute Black-Scholes price on GPU
 *
 * @param ctx   GPU context
 * @param spot  Spot price
 * @param strike Strike price
 * @param rate  Risk-free rate
 * @param vol   Volatility
 * @param time  Time to expiry
 * @param type  Call or Put
 * @return Option price
 */
NIMCP_EXPORT float fin_deriv_gpu_black_scholes(
    nimcp_gpu_context_t* ctx,
    float spot, float strike, float rate, float vol, float time,
    fin_option_type_t type);

/**
 * @brief Batch Black-Scholes on GPU
 *
 * @param ctx     GPU context
 * @param spots   Spot prices [n]
 * @param strikes Strike prices [n]
 * @param rates   Risk-free rates [n]
 * @param vols    Volatilities [n]
 * @param times   Times to expiry [n]
 * @param types   Option types [n]
 * @param n       Number of options
 * @param prices  Output prices [n]
 * @return true on success
 */
NIMCP_EXPORT bool fin_deriv_gpu_black_scholes_batch(
    nimcp_gpu_context_t* ctx,
    const float* spots,
    const float* strikes,
    const float* rates,
    const float* vols,
    const float* times,
    const fin_option_type_t* types,
    uint32_t n,
    float* prices);

/**
 * @brief Compute cumulative normal distribution on GPU
 *
 * @param x Input value
 * @return N(x)
 */
NIMCP_EXPORT float fin_deriv_gpu_norm_cdf(float x);

//=============================================================================
// Cleanup
//=============================================================================

/**
 * @brief Free extended result resources
 */
NIMCP_EXPORT void fin_deriv_extended_result_free(
    fin_deriv_extended_result_t* result);

/**
 * @brief Free option chain result resources
 */
NIMCP_EXPORT void fin_option_chain_result_free(
    fin_option_chain_result_t* result);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_FINANCIAL_DERIVATIVES_GPU_H */
