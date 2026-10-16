//=============================================================================
// nimcp_financial_market.h - Financial Market Analysis Module
//=============================================================================
/**
 * @file nimcp_financial_market.h
 * @brief Time series analysis, technical indicators, sentiment, regime
 *        detection, scenario analysis, Monte Carlo simulation
 *
 * @author NIMCP Development Team
 * @date 2025-12-31
 */

#ifndef NIMCP_FINANCIAL_MARKET_H
#define NIMCP_FINANCIAL_MARKET_H

#include <stdint.h>
#include <stdbool.h>
#include "cognitive/parietal/nimcp_financial_investment.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

#define BIO_MODULE_FINANCIAL_MARKET      0x0394
#define FIN_MKT_MAX_INDICATORS          32
#define FIN_MKT_MAX_SERIES_LENGTH       8192
#define FIN_MKT_MAX_GARCH_ORDER         5
#define FIN_MKT_MAX_MA_PERIOD           200

//=============================================================================
// Error Codes
//=============================================================================

#define FIN_MKT_ERR_OK          0
#define FIN_MKT_ERR_NULL       -1
#define FIN_MKT_ERR_VALIDATION -2
#define FIN_MKT_ERR_PARAM      -3
#define FIN_MKT_ERR_MEMORY     -4

//=============================================================================
// Enumerations
//=============================================================================

typedef enum {
    FIN_MKT_BULL, FIN_MKT_BEAR, FIN_MKT_SIDEWAYS,
    FIN_MKT_HIGH_VOLATILITY, FIN_MKT_CRISIS, FIN_MKT_RECOVERY,
    FIN_MKT_CONDITION_COUNT
} fin_market_condition_t;

typedef enum {
    FIN_MKT_SENTIMENT_EXTREME_FEAR, FIN_MKT_SENTIMENT_FEAR,
    FIN_MKT_SENTIMENT_NEUTRAL, FIN_MKT_SENTIMENT_GREED,
    FIN_MKT_SENTIMENT_EXTREME_GREED, FIN_MKT_SENTIMENT_COUNT
} fin_sentiment_level_t;

typedef enum {
    FIN_MKT_IND_SMA, FIN_MKT_IND_EMA, FIN_MKT_IND_RSI,
    FIN_MKT_IND_MACD, FIN_MKT_IND_BOLLINGER, FIN_MKT_IND_ATR,
    FIN_MKT_IND_MOMENTUM, FIN_MKT_IND_VOLUME_WEIGHTED,
    FIN_MKT_IND_STOCHASTIC, FIN_MKT_IND_TYPE_COUNT
} fin_indicator_type_t;

typedef enum {
    FIN_MKT_SCENARIO_RECESSION, FIN_MKT_SCENARIO_INFLATION_SPIKE,
    FIN_MKT_SCENARIO_RATE_HIKE, FIN_MKT_SCENARIO_RATE_CUT,
    FIN_MKT_SCENARIO_MARKET_CRASH, FIN_MKT_SCENARIO_SECTOR_ROTATION,
    FIN_MKT_SCENARIO_BLACK_SWAN, FIN_MKT_SCENARIO_CUSTOM,
    FIN_MKT_SCENARIO_TYPE_COUNT
} fin_scenario_type_t;

//=============================================================================
// Data Structures
//=============================================================================

typedef struct {
    float prices[FIN_MKT_MAX_SERIES_LENGTH];
    float volumes[FIN_MKT_MAX_SERIES_LENGTH];
    uint64_t timestamps[FIN_MKT_MAX_SERIES_LENGTH];
    uint32_t length;
    float open, high, low, close;
} fin_time_series_t;

typedef struct {
    float omega;
    float alpha[FIN_MKT_MAX_GARCH_ORDER];
    float beta[FIN_MKT_MAX_GARCH_ORDER];
    uint32_t p, q;
    float current_variance;
    float log_likelihood;
    bool converged;
} fin_garch_result_t;

typedef struct {
    fin_indicator_type_t type;
    float values[FIN_MKT_MAX_SERIES_LENGTH];
    uint32_t length;
    float signal;
    float upper_band, lower_band;
    uint32_t period;
} fin_indicator_result_t;

typedef struct {
    fin_sentiment_level_t level;
    float fear_greed_index;
    float put_call_ratio;
    float vix_equivalent;
    float breadth;
    float momentum_score;
    float volume_ratio;
    float confidence;
} fin_sentiment_t;

/** Fuzzy multi-membership market condition */
typedef struct {
    float bull_degree;
    float bear_degree;
    float sideways_degree;
    float high_vol_degree;
    float crisis_degree;
    float recovery_degree;
    fin_market_condition_t dominant;
} fin_fuzzy_market_condition_t;

/** Fuzzy multi-membership sentiment */
typedef struct {
    float extreme_fear_degree;
    float fear_degree;
    float neutral_degree;
    float greed_degree;
    float extreme_greed_degree;
    fin_sentiment_level_t dominant;
} fin_fuzzy_sentiment_t;

typedef struct {
    fin_scenario_type_t type;
    char description[128];
    float equity_shock;
    float rate_shock_bps;
    float vol_shock;
    float credit_spread_shock_bps;
    float fx_shock;
    float commodity_shock;
    float duration_years;
} fin_scenario_t;

typedef struct {
    float portfolio_pnl;
    float portfolio_return;
    float worst_asset_return;
    float best_asset_return;
    float max_drawdown;
    float recovery_time_days;
    float var_breach;
    bool margin_call_triggered;
} fin_scenario_result_t;

typedef struct {
    float paths_mean_return;
    float paths_std_return;
    float paths_var_95;
    float paths_cvar_95;
    float probability_of_loss;
    float expected_shortfall;
    float median_terminal_value;
    uint32_t paths_completed;
    float processing_time_us;
} fin_monte_carlo_result_t;

typedef struct {
    float sentiment_weight;
    float technical_weight;
    float fundamental_weight;
    uint32_t default_ma_period;
    uint32_t garch_max_iterations;
    float garch_convergence_tol;
    uint32_t monte_carlo_default_paths;
    bool enable_regime_detection;
    float inflammation_sensitivity;
    float fatigue_sensitivity;
    bool enable_fuzzy_logic;
    void* fuzzy_bridge;
} fin_market_config_t;

typedef struct {
    uint64_t garch_fits;
    uint64_t indicator_calculations;
    uint64_t sentiment_analyses;
    uint64_t scenario_analyses;
    uint64_t monte_carlo_simulations;
    float avg_processing_time_us;
    uint64_t async_operations;  /**< Bio-async operations count (Change Set 4) */
} fin_market_stats_t;

//=============================================================================
// Forward Declarations for Security Integration
//=============================================================================

struct brain_immune_system;
typedef struct brain_immune_system brain_immune_system_t;
struct bbb_system_struct;
typedef struct bbb_system_struct* bbb_system_t;

//=============================================================================
// Forward Declarations for Bio-Async Integration (Change Set 4)
//=============================================================================

/* Forward declarations for bio-async types (guarded to avoid redefinition) */
#ifndef NIMCP_BIO_ASYNC_FWD_DECL
#define NIMCP_BIO_ASYNC_FWD_DECL
struct bio_async_context;
typedef struct bio_async_context bio_async_context_t;
struct bio_router_struct;
typedef struct bio_router_struct* bio_router_t;
#endif

//=============================================================================
// Opaque Handle
//=============================================================================

typedef struct financial_market_eng financial_market_eng_t;

//=============================================================================
// Lifecycle
//=============================================================================

financial_market_eng_t* financial_market_create(void);
financial_market_eng_t* financial_market_create_custom(const fin_market_config_t* config);
void financial_market_destroy(financial_market_eng_t* mkt);
fin_market_config_t financial_market_default_config(void);

//=============================================================================
// Time Series Analysis
//=============================================================================

int financial_market_garch_fit(financial_market_eng_t* mkt,
                                const float* returns, uint32_t length,
                                uint32_t p, uint32_t q,
                                fin_garch_result_t* out_result);
float financial_market_garch_forecast(const fin_garch_result_t* garch,
                                      uint32_t steps_ahead);

//=============================================================================
// Technical Indicators
//=============================================================================

int financial_market_compute_indicator(financial_market_eng_t* mkt,
                                       const fin_time_series_t* series,
                                       fin_indicator_type_t type, uint32_t period,
                                       fin_indicator_result_t* out_result);
int financial_market_compute_sma(const float* prices, uint32_t length,
                                 uint32_t period, float* out_values);
int financial_market_compute_ema(const float* prices, uint32_t length,
                                 uint32_t period, float* out_values);
float financial_market_compute_rsi(const float* prices, uint32_t length,
                                   uint32_t period);
int financial_market_compute_macd(const float* prices, uint32_t length,
                                   uint32_t fast, uint32_t slow, uint32_t signal,
                                   float* out_macd, float* out_signal_line,
                                   float* out_histogram);
int financial_market_compute_bollinger(const float* prices, uint32_t length,
                                       uint32_t period, float num_std,
                                       float* out_upper, float* out_middle,
                                       float* out_lower);

//=============================================================================
// Market Sentiment & Regime
//=============================================================================

int financial_market_analyze_sentiment(financial_market_eng_t* mkt,
                                       const fin_time_series_t* price_series,
                                       const fin_time_series_t* volume_series,
                                       fin_sentiment_t* out_sentiment);
fin_market_condition_t financial_market_detect_regime(financial_market_eng_t* mkt,
                                                      const fin_time_series_t* series);

/** Fuzzy regime detection with multi-membership */
int financial_market_detect_regime_fuzzy(financial_market_eng_t* mkt,
                                         const fin_time_series_t* series,
                                         fin_fuzzy_market_condition_t* out_condition);

/** Fuzzy sentiment with multi-membership */
int financial_market_analyze_sentiment_fuzzy(financial_market_eng_t* mkt,
                                              const fin_time_series_t* price_series,
                                              const fin_time_series_t* volume_series,
                                              fin_fuzzy_sentiment_t* out_sentiment);

//=============================================================================
// Scenario Analysis & Monte Carlo
//=============================================================================

int financial_market_run_scenario(financial_market_eng_t* mkt,
                                  const fin_portfolio_t* portfolio,
                                  const fin_scenario_t* scenario,
                                  fin_scenario_result_t* out_result);
int financial_market_stress_test(financial_market_eng_t* mkt,
                                 const fin_portfolio_t* portfolio,
                                 const fin_scenario_t* scenarios,
                                 uint32_t num_scenarios,
                                 fin_scenario_result_t* out_results);
int financial_market_monte_carlo(financial_market_eng_t* mkt,
                                  const fin_portfolio_t* portfolio,
                                  float drift, float volatility,
                                  float horizon_years, uint32_t num_paths,
                                  fin_monte_carlo_result_t* out_result);

//=============================================================================
// Modulation & Statistics
//=============================================================================

int financial_market_set_inflammation(financial_market_eng_t* mkt, float level);
int financial_market_set_fatigue(financial_market_eng_t* mkt, float level);
int financial_market_get_stats(const financial_market_eng_t* mkt, fin_market_stats_t* stats);
void financial_market_reset_stats(financial_market_eng_t* mkt);
const char* financial_market_get_last_error(void);

//=============================================================================
// Security Integration (Per-Engine)
//=============================================================================

/** Set immune system for per-engine validation */
void financial_market_eng_set_immune(financial_market_eng_t* mkt, brain_immune_system_t* immune);
/** Set BBB system for per-engine validation */
void financial_market_eng_set_bbb(financial_market_eng_t* mkt, bbb_system_t bbb);
/** Enable/disable BBB validation on engine */
void financial_market_eng_enable_bbb_validation(financial_market_eng_t* mkt, bool enable);
/** Enable/disable immune validation on engine */
void financial_market_eng_enable_immune_validation(financial_market_eng_t* mkt, bool enable);

//=============================================================================
// Bio-Async Integration (Change Set 4)
//=============================================================================

/** Set bio-async context for asynchronous processing */
int financial_market_set_bio_async(financial_market_eng_t* mkt, bio_async_context_t* ctx);
/** Set bio-router for message routing */
int financial_market_set_bio_router(financial_market_eng_t* mkt, bio_router_t* router);

//=============================================================================
// KG Wiring Integration (Change Set 1)
//=============================================================================

/* Forward declaration for KG wiring */
struct kg_wiring;
typedef struct kg_wiring kg_wiring_t;

/** Set KG wiring for financial market engine */
int financial_market_set_kg_wiring(financial_market_eng_t* mkt, kg_wiring_t* kg);

//=============================================================================
// Health Agent and Logger Integration (Phase 8: Change Set 2/3)
//=============================================================================

/** Set health agent for instance-level heartbeats */
int financial_market_set_health_agent(financial_market_eng_t* mkt, void* agent);
/** Set logger for instance-level logging */
int financial_market_set_logger(financial_market_eng_t* mkt, void* logger);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_FINANCIAL_MARKET_H */
