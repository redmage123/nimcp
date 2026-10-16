//=============================================================================
// nimcp_financial_optimization_gpu.h - GPU Portfolio Optimization
//=============================================================================
/**
 * @file nimcp_financial_optimization_gpu.h
 * @brief GPU-accelerated portfolio optimization using cuBLAS
 *
 * WHAT: Mean-variance, risk parity, max Sharpe optimization on GPU
 * WHY:  25x+ speedup for large portfolios (256+ assets)
 * HOW:  cuBLAS for matrix operations, parallel gradient computation
 *
 * ALGORITHMS:
 *   - Mean-Variance: Quadratic programming with linear constraints
 *   - Max Sharpe: Iterative optimization with Sharpe ratio objective
 *   - Risk Parity: Equal risk contribution via Newton-Raphson
 *   - Black-Litterman: Bayesian prior + investor views
 *
 * @author NIMCP Development Team
 * @date 2025
 */

#ifndef NIMCP_FINANCIAL_OPTIMIZATION_GPU_H
#define NIMCP_FINANCIAL_OPTIMIZATION_GPU_H

#include "gpu/financial/nimcp_financial_gpu.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Optimization Types
//=============================================================================

/**
 * @brief Constraint type for optimization
 */
typedef enum {
    FIN_OPT_CONSTRAINT_EQUALITY,            /**< Sum to 1.0 */
    FIN_OPT_CONSTRAINT_INEQUALITY_LE,       /**< Less than or equal */
    FIN_OPT_CONSTRAINT_INEQUALITY_GE,       /**< Greater than or equal */
    FIN_OPT_CONSTRAINT_BOUNDS,              /**< Box constraints */
    FIN_OPT_CONSTRAINT_TYPE_COUNT
} fin_opt_constraint_type_t;

/**
 * @brief Linear constraint for portfolio optimization
 *
 * Represents: A * w <= b (or == or >=)
 */
typedef struct {
    fin_opt_constraint_type_t type;         /**< Constraint type */
    float* coefficients;                    /**< Constraint coefficients [num_assets] */
    float bound;                            /**< RHS bound value */
} fin_opt_constraint_t;

/**
 * @brief Extended optimization parameters
 */
typedef struct {
    fin_optimization_gpu_params_t base;     /**< Base parameters */

    /* Additional constraints */
    fin_opt_constraint_t* constraints;      /**< Array of constraints */
    uint32_t num_constraints;               /**< Number of constraints */

    /* Sector constraints */
    uint32_t* sector_ids;                   /**< Sector ID per asset [num_assets] */
    float* sector_min_weights;              /**< Min weight per sector */
    float* sector_max_weights;              /**< Max weight per sector */
    uint32_t num_sectors;                   /**< Number of sectors */

    /* Transaction costs */
    float* current_weights;                 /**< Current portfolio weights */
    float transaction_cost;                 /**< Transaction cost (basis points) */
    bool minimize_turnover;                 /**< Include turnover in objective */

    /* Black-Litterman parameters */
    float* views_matrix;                    /**< P matrix: views [num_views x num_assets] */
    float* views_returns;                   /**< Q vector: expected excess returns [num_views] */
    float* views_confidence;                /**< Omega diagonal [num_views] */
    uint32_t num_views;                     /**< Number of investor views */
    float tau;                              /**< Scaling factor for prior covariance */
    float* market_weights;                  /**< Market cap weights for equilibrium */

    /* Robust optimization */
    bool use_robust;                        /**< Use robust optimization */
    float* return_uncertainty;              /**< Return uncertainty [num_assets] */
    float* cov_uncertainty;                 /**< Covariance uncertainty [n x n] */
    float uncertainty_budget;               /**< Uncertainty budget (kappa) */
} fin_opt_extended_params_t;

/**
 * @brief Extended optimization result
 */
typedef struct {
    fin_optimization_gpu_result_t base;     /**< Base result */

    /* Risk attribution */
    float* marginal_risk;                   /**< Marginal risk contribution [num_assets] */
    float* risk_contribution;               /**< Risk contribution [num_assets] */
    float concentration_ratio;              /**< Risk concentration (Herfindahl) */

    /* Black-Litterman outputs */
    float* posterior_returns;               /**< BL posterior returns [num_assets] */
    float* posterior_covariance;            /**< BL posterior covariance [n x n] */

    /* Transaction analysis */
    float turnover;                         /**< Portfolio turnover */
    float transaction_costs;                /**< Total transaction costs */

    /* Constraint analysis */
    float* constraint_slackness;            /**< Slack per constraint */
    bool* active_constraints;               /**< Which constraints are active */
} fin_opt_extended_result_t;

/**
 * @brief Default extended optimization parameters
 */
static inline fin_opt_extended_params_t fin_opt_extended_params_default(void)
{
    fin_opt_extended_params_t params = {
        .base = {
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
            .long_only = true
        },
        .constraints = NULL,
        .num_constraints = 0,
        .sector_ids = NULL,
        .sector_min_weights = NULL,
        .sector_max_weights = NULL,
        .num_sectors = 0,
        .current_weights = NULL,
        .transaction_cost = 0.0f,
        .minimize_turnover = false,
        .views_matrix = NULL,
        .views_returns = NULL,
        .views_confidence = NULL,
        .num_views = 0,
        .tau = 0.05f,
        .market_weights = NULL,
        .use_robust = false,
        .return_uncertainty = NULL,
        .cov_uncertainty = NULL,
        .uncertainty_budget = 0.0f
    };
    return params;
}

//=============================================================================
// Advanced Optimization API
//=============================================================================

/**
 * @brief Run constrained portfolio optimization on GPU
 *
 * @param ctx               GPU context
 * @param expected_returns  Expected returns [num_assets]
 * @param covariance_matrix Covariance matrix [num_assets x num_assets]
 * @param num_assets        Number of assets
 * @param params            Extended parameters
 * @param result            Extended result
 * @return true on success
 */
NIMCP_EXPORT bool fin_opt_gpu_constrained(
    nimcp_gpu_context_t* ctx,
    const float* expected_returns,
    const float* covariance_matrix,
    uint32_t num_assets,
    const fin_opt_extended_params_t* params,
    fin_opt_extended_result_t* result);

/**
 * @brief Run Black-Litterman optimization on GPU
 *
 * @param ctx               GPU context
 * @param market_covariance Market covariance [num_assets x num_assets]
 * @param num_assets        Number of assets
 * @param params            Extended params (must have BL fields set)
 * @param result            Extended result (includes posterior)
 * @return true on success
 */
NIMCP_EXPORT bool fin_opt_gpu_black_litterman(
    nimcp_gpu_context_t* ctx,
    const float* market_covariance,
    uint32_t num_assets,
    const fin_opt_extended_params_t* params,
    fin_opt_extended_result_t* result);

/**
 * @brief Run risk parity optimization on GPU
 *
 * @param ctx               GPU context
 * @param covariance_matrix Covariance matrix [num_assets x num_assets]
 * @param num_assets        Number of assets
 * @param target_risk_contrib Target risk contribution per asset (NULL = equal)
 * @param params            Base parameters
 * @param result            Output result
 * @return true on success
 */
NIMCP_EXPORT bool fin_opt_gpu_risk_parity(
    nimcp_gpu_context_t* ctx,
    const float* covariance_matrix,
    uint32_t num_assets,
    const float* target_risk_contrib,
    const fin_optimization_gpu_params_t* params,
    fin_optimization_gpu_result_t* result);

/**
 * @brief Run robust optimization on GPU
 *
 * @param ctx               GPU context
 * @param expected_returns  Expected returns [num_assets]
 * @param covariance_matrix Covariance matrix [num_assets x num_assets]
 * @param num_assets        Number of assets
 * @param params            Extended params (must have robust fields set)
 * @param result            Extended result
 * @return true on success
 */
NIMCP_EXPORT bool fin_opt_gpu_robust(
    nimcp_gpu_context_t* ctx,
    const float* expected_returns,
    const float* covariance_matrix,
    uint32_t num_assets,
    const fin_opt_extended_params_t* params,
    fin_opt_extended_result_t* result);

/**
 * @brief Compute risk contributions on GPU
 *
 * @param ctx               GPU context
 * @param weights           Portfolio weights [num_assets]
 * @param covariance_matrix Covariance matrix [num_assets x num_assets]
 * @param num_assets        Number of assets
 * @param out_marginal      Output marginal risk [num_assets]
 * @param out_contribution  Output risk contribution [num_assets]
 * @return true on success
 */
NIMCP_EXPORT bool fin_opt_gpu_risk_contribution(
    nimcp_gpu_context_t* ctx,
    const float* weights,
    const float* covariance_matrix,
    uint32_t num_assets,
    float* out_marginal,
    float* out_contribution);

/**
 * @brief Compute efficient frontier with constraints on GPU
 *
 * @param ctx               GPU context
 * @param expected_returns  Expected returns [num_assets]
 * @param covariance_matrix Covariance matrix [num_assets x num_assets]
 * @param num_assets        Number of assets
 * @param num_points        Number of frontier points
 * @param params            Extended parameters (constraints applied)
 * @param out_returns       Output returns [num_points]
 * @param out_volatilities  Output volatilities [num_points]
 * @param out_weights       Output weights [num_points x num_assets]
 * @param out_sharpes       Output Sharpe ratios [num_points]
 * @return true on success
 */
NIMCP_EXPORT bool fin_opt_gpu_efficient_frontier_constrained(
    nimcp_gpu_context_t* ctx,
    const float* expected_returns,
    const float* covariance_matrix,
    uint32_t num_assets,
    uint32_t num_points,
    const fin_opt_extended_params_t* params,
    float* out_returns,
    float* out_volatilities,
    float* out_weights,
    float* out_sharpes);

//=============================================================================
// Matrix Utilities (GPU)
//=============================================================================

/**
 * @brief Compute portfolio variance on GPU: w' * Cov * w
 *
 * @param ctx         GPU context
 * @param weights     Weights [num_assets]
 * @param covariance  Covariance [num_assets x num_assets]
 * @param num_assets  Number of assets
 * @return Portfolio variance
 */
NIMCP_EXPORT float fin_opt_gpu_portfolio_variance(
    nimcp_gpu_context_t* ctx,
    const float* weights,
    const float* covariance,
    uint32_t num_assets);

/**
 * @brief Compute portfolio return on GPU: w' * returns
 *
 * @param ctx        GPU context
 * @param weights    Weights [num_assets]
 * @param returns    Expected returns [num_assets]
 * @param num_assets Number of assets
 * @return Portfolio return
 */
NIMCP_EXPORT float fin_opt_gpu_portfolio_return(
    nimcp_gpu_context_t* ctx,
    const float* weights,
    const float* returns,
    uint32_t num_assets);

/**
 * @brief Invert covariance matrix on GPU
 *
 * @param ctx               GPU context
 * @param covariance_matrix Input covariance [n x n]
 * @param n                 Matrix dimension
 * @param inverse           Output inverse [n x n]
 * @return true on success (false if singular)
 */
NIMCP_EXPORT bool fin_opt_gpu_invert_covariance(
    nimcp_gpu_context_t* ctx,
    const float* covariance_matrix,
    uint32_t n,
    float* inverse);

/**
 * @brief Free extended result resources
 */
NIMCP_EXPORT void fin_opt_extended_result_free(
    fin_opt_extended_result_t* result);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_FINANCIAL_OPTIMIZATION_GPU_H */
