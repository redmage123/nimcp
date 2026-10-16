//=============================================================================
// nimcp_financial_investment.h - Core Financial Investment Module
//=============================================================================
/**
 * @file nimcp_financial_investment.h
 * @brief Portfolio management, risk assessment, derivatives pricing,
 *        asset valuation, optimization, and factor analysis
 *
 * @author NIMCP Development Team
 * @date 2025-12-31
 */

#ifndef NIMCP_FINANCIAL_INVESTMENT_H
#define NIMCP_FINANCIAL_INVESTMENT_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

#define BIO_MODULE_FINANCIAL_INVESTMENT  0x0393
#define FIN_MAX_ASSETS                  512
#define FIN_MAX_PORTFOLIO_SIZE          256
#define FIN_MAX_FACTORS                 32
#define FIN_MAX_SCENARIOS               64
#define FIN_MAX_GREEKS                  8
#define FIN_MAX_HISTORY_POINTS          4096
#define FIN_MAX_MONTE_CARLO_PATHS       100000
#define FIN_PRICE_PRECISION             1e-8

//=============================================================================
// Error Codes
//=============================================================================

#define FIN_ERROR_BASE                  32000
#define FIN_ERR_OK                      0
#define FIN_ERR_NULL                    (FIN_ERROR_BASE + 1)
#define FIN_ERR_PORTFOLIO_FULL          (FIN_ERROR_BASE + 2)
#define FIN_ERR_ASSET_NOT_FOUND         (FIN_ERROR_BASE + 3)
#define FIN_ERR_INVALID_WEIGHT          (FIN_ERROR_BASE + 4)
#define FIN_ERR_INVALID_PARAMS          (FIN_ERROR_BASE + 5)
#define FIN_ERR_CONVERGENCE             (FIN_ERROR_BASE + 6)
#define FIN_ERR_PRICING_FAILED          (FIN_ERROR_BASE + 7)
#define FIN_ERR_ALLOC                   (FIN_ERROR_BASE + 8)
#define FIN_ERR_HISTORY_TOO_SHORT       (FIN_ERROR_BASE + 9)
#define FIN_ERR_INVALID_CONFIDENCE      (FIN_ERROR_BASE + 10)
#define FIN_ERR_SUBSYSTEM               (FIN_ERROR_BASE + 11)
#define FIN_ERR_VALIDATION              (FIN_ERROR_BASE + 12)

//=============================================================================
// Enumerations
//=============================================================================

typedef enum {
    FIN_ASSET_EQUITY, FIN_ASSET_FIXED_INCOME, FIN_ASSET_COMMODITY,
    FIN_ASSET_CURRENCY, FIN_ASSET_DERIVATIVE_OPTION, FIN_ASSET_DERIVATIVE_FUTURE,
    FIN_ASSET_DERIVATIVE_SWAP, FIN_ASSET_CRYPTO, FIN_ASSET_REAL_ESTATE,
    FIN_ASSET_INDEX, FIN_ASSET_TYPE_COUNT
} fin_asset_type_t;

typedef enum { FIN_OPT_CALL, FIN_OPT_PUT } fin_option_type_t;

typedef enum {
    FIN_OPT_STYLE_EUROPEAN, FIN_OPT_STYLE_AMERICAN, FIN_OPT_STYLE_BERMUDAN
} fin_option_style_t;

typedef enum {
    FIN_ANALYSIS_RISK_ASSESSMENT, FIN_ANALYSIS_PORTFOLIO_OPTIMIZATION,
    FIN_ANALYSIS_DERIVATIVES_PRICING, FIN_ANALYSIS_ASSET_VALUATION,
    FIN_ANALYSIS_FACTOR_MODEL, FIN_ANALYSIS_SCENARIO,
    FIN_ANALYSIS_TAX_LOSS_HARVEST, FIN_ANALYSIS_TYPE_COUNT
} fin_analysis_type_t;

typedef enum {
    FIN_OPT_STRATEGY_MEAN_VARIANCE, FIN_OPT_STRATEGY_MIN_VARIANCE,
    FIN_OPT_STRATEGY_MAX_SHARPE, FIN_OPT_STRATEGY_RISK_PARITY,
    FIN_OPT_STRATEGY_BLACK_LITTERMAN, FIN_OPT_STRATEGY_EQUAL_WEIGHT,
    FIN_OPT_STRATEGY_COUNT
} fin_optimization_strategy_t;

typedef enum {
    FIN_VALUATION_DCF, FIN_VALUATION_DDM, FIN_VALUATION_COMPARABLES,
    FIN_VALUATION_BOOK_VALUE, FIN_VALUATION_EARNINGS_MULTIPLE,
    FIN_VALUATION_TYPE_COUNT
} fin_valuation_type_t;

typedef enum {
    FIN_PRICING_BLACK_SCHOLES, FIN_PRICING_BINOMIAL_TREE,
    FIN_PRICING_MONTE_CARLO, FIN_PRICING_TYPE_COUNT
} fin_pricing_model_t;

//=============================================================================
// Core Data Structures
//=============================================================================

typedef struct {
    uint32_t asset_id;
    fin_asset_type_t type;
    char symbol[32];
    float current_price;
    float expected_return;
    float volatility;
    float dividend_yield;
    float beta;
    float market_cap;
    uint32_t sector_id;
} fin_asset_t;

typedef struct {
    uint32_t asset_count;
    fin_asset_t assets[FIN_MAX_PORTFOLIO_SIZE];
    float weights[FIN_MAX_PORTFOLIO_SIZE];
    float total_value;
    float cash_position;
    uint64_t last_rebalance_us;
} fin_portfolio_t;

typedef struct {
    float var_95;
    float var_99;
    float cvar_95;
    float cvar_99;
    float sharpe_ratio;
    float sortino_ratio;
    float calmar_ratio;
    float treynor_ratio;
    float information_ratio;
    float max_drawdown;
    float portfolio_beta;
    float portfolio_alpha;
    float tracking_error;
    float volatility_annual;
    float downside_deviation;
    float herfindahl_index;
    float diversification_ratio;
    /* Fuzzy integration */
    float fuzzy_risk_grade;              /**< Fuzzy composite risk [0,1] */
    float fuzzy_diversification_quality; /**< Fuzzy diversification [0,1] */
} fin_risk_metrics_t;

typedef struct {
    float price;
    float delta;
    float gamma;
    float theta;
    float vega;
    float rho;
    float charm;
    float vanna;
    float implied_volatility;
    uint32_t steps_used;
} fin_option_result_t;

typedef struct {
    fin_valuation_type_t method;
    float intrinsic_value;
    float margin_of_safety;
    float upside_potential;
    float confidence;
} fin_valuation_result_t;

typedef struct {
    fin_optimization_strategy_t strategy;
    float optimal_weights[FIN_MAX_PORTFOLIO_SIZE];
    float expected_return;
    float expected_volatility;
    float expected_sharpe;
    uint32_t iterations;
    bool converged;
    float objective_value;
    float convergence_degree;            /**< Fuzzy convergence [0,1] */
} fin_optimization_result_t;

typedef struct {
    float factor_loadings[FIN_MAX_FACTORS];
    float factor_returns[FIN_MAX_FACTORS];
    float residual_return;
    float r_squared;
    uint32_t factor_count;
} fin_factor_result_t;

//=============================================================================
// Configuration & Statistics
//=============================================================================

typedef struct {
    float risk_free_rate;
    float default_horizon_years;
    float convergence_tolerance;
    uint32_t max_iterations;
    uint32_t monte_carlo_paths;
    uint32_t binomial_tree_steps;
    float min_weight;
    float max_weight;
    float rebalance_threshold;
    bool enable_tax_optimization;
    bool enable_intuition;
    float transaction_cost_bps;
    float tax_rate_short;
    float tax_rate_long;
    float inflammation_sensitivity;
    float fatigue_sensitivity;
    bool enable_fuzzy_logic;
    void* fuzzy_bridge;
} fin_config_t;

typedef struct {
    uint64_t portfolio_analyses;
    uint64_t risk_assessments;
    uint64_t option_pricings;
    uint64_t valuations;
    uint64_t optimizations;
    uint64_t factor_analyses;
    uint64_t scenario_runs;
    uint64_t monte_carlo_runs;
    float avg_processing_time_us;
    uint64_t async_operations;  /**< Bio-async operations count (Change Set 4) */
} fin_stats_t;

//=============================================================================
// Opaque Handle
//=============================================================================

typedef struct financial_investment_eng financial_investment_eng_t;

//=============================================================================
// Lifecycle API
//=============================================================================

financial_investment_eng_t* financial_investment_create(void);
financial_investment_eng_t* financial_investment_create_custom(const fin_config_t* config);
void financial_investment_destroy(financial_investment_eng_t* fin);
fin_config_t financial_investment_default_config(void);

//=============================================================================
// Portfolio Management
//=============================================================================

int financial_investment_portfolio_create(financial_investment_eng_t* fin,
                                          fin_portfolio_t* portfolio);
int financial_investment_portfolio_add_asset(financial_investment_eng_t* fin,
                                             fin_portfolio_t* portfolio,
                                             const fin_asset_t* asset, float weight);
int financial_investment_portfolio_remove_asset(financial_investment_eng_t* fin,
                                                fin_portfolio_t* portfolio,
                                                uint32_t asset_id);
int financial_investment_portfolio_rebalance(financial_investment_eng_t* fin,
                                             fin_portfolio_t* portfolio,
                                             const float* target_weights);
float financial_investment_portfolio_return(const financial_investment_eng_t* fin,
                                            const fin_portfolio_t* portfolio);
float financial_investment_portfolio_volatility(const financial_investment_eng_t* fin,
                                                const fin_portfolio_t* portfolio,
                                                const float* correlation_matrix);

//=============================================================================
// Risk Assessment
//=============================================================================

int financial_investment_assess_risk(financial_investment_eng_t* fin,
                                     const fin_portfolio_t* portfolio,
                                     const float* correlation_matrix,
                                     const float* returns_history,
                                     uint32_t history_length,
                                     fin_risk_metrics_t* out_metrics);
float financial_investment_compute_var(financial_investment_eng_t* fin,
                                       const float* returns, uint32_t count,
                                       float confidence);
float financial_investment_compute_cvar(financial_investment_eng_t* fin,
                                        const float* returns, uint32_t count,
                                        float confidence);
float financial_investment_max_drawdown(const float* prices, uint32_t count);

//=============================================================================
// Derivatives Pricing
//=============================================================================

int financial_investment_price_option(financial_investment_eng_t* fin,
                                      float spot, float strike,
                                      float rate, float volatility,
                                      float time_to_expiry,
                                      fin_option_type_t type,
                                      fin_pricing_model_t model,
                                      fin_option_result_t* out_result);
float financial_investment_black_scholes(float spot, float strike,
                                         float rate, float vol,
                                         float time, fin_option_type_t type);
int financial_investment_binomial_tree(financial_investment_eng_t* fin,
                                       float spot, float strike,
                                       float rate, float vol,
                                       float time, uint32_t steps,
                                       fin_option_type_t type,
                                       fin_option_style_t style,
                                       fin_option_result_t* out_result);
float financial_investment_implied_vol(float market_price,
                                       float spot, float strike,
                                       float rate, float time,
                                       fin_option_type_t type);

//=============================================================================
// Asset Valuation
//=============================================================================

int financial_investment_dcf_valuation(financial_investment_eng_t* fin,
                                       const float* cash_flows,
                                       uint32_t num_periods,
                                       float discount_rate,
                                       float terminal_growth,
                                       fin_valuation_result_t* out_result);
int financial_investment_ddm_valuation(financial_investment_eng_t* fin,
                                       float current_dividend,
                                       float growth_rate,
                                       float required_return,
                                       fin_valuation_result_t* out_result);
int financial_investment_comparables(financial_investment_eng_t* fin,
                                     const float* peer_multiples,
                                     uint32_t peer_count,
                                     float target_metric,
                                     fin_valuation_result_t* out_result);

//=============================================================================
// Portfolio Optimization
//=============================================================================

int financial_investment_optimize(financial_investment_eng_t* fin,
                                  const fin_portfolio_t* portfolio,
                                  const float* expected_returns,
                                  const float* covariance_matrix,
                                  fin_optimization_strategy_t strategy,
                                  fin_optimization_result_t* out_result);
int financial_investment_efficient_frontier(financial_investment_eng_t* fin,
                                            const float* expected_returns,
                                            const float* covariance_matrix,
                                            uint32_t asset_count,
                                            float* out_frontier_returns,
                                            float* out_frontier_vols,
                                            uint32_t frontier_points);

//=============================================================================
// Factor Analysis
//=============================================================================

int financial_investment_factor_analysis(financial_investment_eng_t* fin,
                                         const float* asset_returns,
                                         const float* factor_returns,
                                         uint32_t num_observations,
                                         uint32_t num_factors,
                                         fin_factor_result_t* out_result);

//=============================================================================
// Tax-Loss Harvesting
//=============================================================================

int financial_investment_tax_loss_harvest(financial_investment_eng_t* fin,
                                          fin_portfolio_t* portfolio,
                                          const float* cost_basis,
                                          float* out_tax_savings);

//=============================================================================
// Modulation & Statistics
//=============================================================================

int financial_investment_set_inflammation(financial_investment_eng_t* fin, float level);
int financial_investment_set_fatigue(financial_investment_eng_t* fin, float level);
int financial_investment_get_stats(const financial_investment_eng_t* fin, fin_stats_t* stats);
void financial_investment_reset_stats(financial_investment_eng_t* fin);
const char* financial_investment_get_last_error(void);
void financial_investment_free_optimization(fin_optimization_result_t* result);
void financial_investment_free_factor(fin_factor_result_t* result);

//=============================================================================
// Instance-Level Immune/BBB Integration (Phase 9)
//=============================================================================

/* Forward declarations for immune/bbb types */
struct brain_immune_system;
typedef struct brain_immune_system brain_immune_system_t;
struct bbb_system_struct;
typedef struct bbb_system_struct* bbb_system_t;

int financial_investment_set_instance_immune(financial_investment_eng_t* fin, brain_immune_system_t* immune);
int financial_investment_set_instance_bbb(financial_investment_eng_t* fin, bbb_system_t bbb);
int financial_investment_enable_immune_validation(financial_investment_eng_t* fin, bool enable);
int financial_investment_enable_bbb_validation(financial_investment_eng_t* fin, bool enable);

//=============================================================================
// KG Wiring Integration (Change Set 1)
//=============================================================================

/* Forward declaration for KG wiring */
struct kg_wiring;
typedef struct kg_wiring kg_wiring_t;

/** Set KG wiring for financial investment engine */
int financial_investment_set_kg_wiring(financial_investment_eng_t* fin, kg_wiring_t* kg);

//=============================================================================
// Bio-Async Integration (Change Set 4)
//=============================================================================

/* Forward declarations for bio-async types (guarded to avoid redefinition) */
#ifndef NIMCP_BIO_ASYNC_FWD_DECL
#define NIMCP_BIO_ASYNC_FWD_DECL
struct bio_async_context;
typedef struct bio_async_context bio_async_context_t;
struct bio_router_struct;
typedef struct bio_router_struct* bio_router_t;
#endif

/** Set bio-async context for asynchronous processing */
int financial_investment_set_bio_async(financial_investment_eng_t* fin, bio_async_context_t* ctx);
/** Set bio-router for message routing */
int financial_investment_set_bio_router(financial_investment_eng_t* fin, bio_router_t* router);

//=============================================================================
// Health Agent and Logger Integration (Phase 8: Change Set 2/3)
//=============================================================================

/** Set health agent for instance-level heartbeats */
int financial_investment_set_health_agent(financial_investment_eng_t* fin, void* agent);
/** Set logger for instance-level logging */
int financial_investment_set_logger(financial_investment_eng_t* fin, void* logger);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_FINANCIAL_INVESTMENT_H */
