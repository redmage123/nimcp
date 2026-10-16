/**
 * @file nimcp_financial_investment.c
 * @brief Core Financial Investment Module implementation
 *
 * Implements portfolio management, risk assessment, derivatives pricing,
 * asset valuation, portfolio optimization, factor analysis, and tax-loss
 * harvesting with fuzzy logic integration and biological modulation.
 */

#include "cognitive/parietal/nimcp_financial_investment.h"
#include "constants/nimcp_buffer_constants.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include <stdarg.h>

/* Fuzzy logic types for membership function evaluation */
#include "utils/fuzzy/nimcp_fuzzy_types.h"

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_learning_constants.h"
#include "constants/nimcp_threshold_constants.h"
#include "constants/nimcp_dimension_constants.h"
#include "constants/nimcp_math_constants.h"
#include "utils/math/nimcp_math_helpers.h"

/* Custom health agent: using pre-existing implementation from header */
static nimcp_health_agent_t* g_financial_investment_health_agent = NULL;

BRIDGE_DEFINE_MESH_REGISTRATION(financial_investment, MESH_ADAPTER_CATEGORY_COGNITIVE)

/* Forward declarations for immune/BBB validation stubs */
extern int brain_immune_validate_operation(brain_immune_system_t* immune,
                                            const char* operation,
                                            uint32_t severity);
extern int brain_immune_present_antigen(brain_immune_system_t* immune,
                                         int source,
                                         const uint8_t* epitope,
                                         size_t epitope_len,
                                         uint32_t severity,
                                         uint32_t source_node,
                                         uint32_t* antigen_id);
extern int bbb_validate_data(bbb_system_t bbb, const void* data,
                              size_t size, const char* context);

/* Stub declarations for subsystem integration globals */
static void* g_fin_investment_immune = NULL;
static void* g_fin_investment_bbb = NULL;


//=============================================================================
struct kg_wiring;
typedef struct kg_wiring kg_wiring_t;

/* KG message type defines for investment module */
#define KG_MSG_FIN_INV_REQUEST    "FIN_INV_REQUEST"
#define KG_MSG_FIN_INV_RESPONSE   "FIN_INV_RESPONSE"
#define KG_MSG_FIN_INV_ERROR      "FIN_INV_ERROR"
#define KG_MSG_FIN_INV_UPDATE     "FIN_INV_UPDATE"

/**
 * @brief Set module-level health agent for financial_investment heartbeats
 * @param agent Health agent (can be NULL to disable)
 */
void financial_investment_module_set_health_agent(nimcp_health_agent_t* agent) {
    g_financial_investment_health_agent = agent;
}

/** @brief Send heartbeat from financial_investment module */
static inline void financial_investment_heartbeat(const char* operation, float progress) {
    if (g_financial_investment_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_financial_investment_health_agent, operation, progress);
    }
}

/** @brief Send heartbeat from financial_investment module (instance-level) */
static inline void financial_investment_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_financial_investment_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_financial_investment_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_financial_investment_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}

//=============================================================================
// Constants
//=============================================================================

#define BIO_MODULE_ID  0x0393
#define EPSILON        NIMCP_EPSILON_ADAM
#define PI_VAL         NIMCP_PI_F

//=============================================================================
// Thread-local error string
//=============================================================================

static _Thread_local char fin_last_error[NIMCP_ERROR_BUFFER_SIZE] = {0};

static void set_error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(fin_last_error, sizeof(fin_last_error), fmt, args);
    va_end(args);
}

//=============================================================================
// Internal engine structure
//=============================================================================

struct financial_investment_eng {
    fin_config_t config;
    fin_stats_t stats;

    /* Biological modulation */
    float inflammation;
    float fatigue;

    /* Subsystem pointers */
    void* fuzzy_bridge;
    kg_wiring_t* kg_wiring;

    /* Immune/BBB instance pointers (Phase 9) */
    brain_immune_system_t* immune;
    bbb_system_t bbb;
    bool enable_immune_validation;
    bool enable_bbb_validation;

    /* Health agent and logger (Phase 8: Change Set 2/3) */
    nimcp_health_agent_t* health_agent;
    void* logger;

    /* Bio-async integration (Change Set 4) */
    bio_async_context_t* bio_async;
    bio_router_t* bio_router;
    bool async_enabled;

    /* Internal state */
    uint64_t total_ops;
    double total_processing_time_us;
};

//=============================================================================
// KG Wiring Helper (Change Set 1)
//=============================================================================

/**
 * @brief Publish a message through KG wiring
 * @param eng Investment engine instance
 * @param msg_type Message type string
 * @param payload Payload data
 * @param size Payload size in bytes
 * @return 0 on success
 */
static int investment_kg_publish(financial_investment_eng_t* eng, const char* msg_type,
                                  const void* payload, size_t size) {
    if (eng && eng->kg_wiring) {
        /* kg_wiring_publish would be called here */
        (void)msg_type; (void)payload; (void)size;
        return 0;
    }
    return 0;
}

//=============================================================================
// Instance-Level Heartbeat Helper (Phase 8: Change Set 2/3)
//=============================================================================

static inline void investment_heartbeat_instance(financial_investment_eng_t* eng,
                                                   const char* op, float progress) {
    if (eng && eng->health_agent) {
        /* nimcp_health_agent_heartbeat_ex would be called here */
        (void)op; (void)progress;
    }
}

//=============================================================================
// Logging Macros (Phase 8: Change Set 2/3)
//=============================================================================

#define FIN_INV_LOG_DEBUG(eng, fmt, ...) /* placeholder */
#define FIN_INV_LOG_INFO(eng, fmt, ...)  /* placeholder */
#define FIN_INV_LOG_WARN(eng, fmt, ...)  /* placeholder */
#define FIN_INV_LOG_ERROR(eng, fmt, ...) /* placeholder */

//=============================================================================
// Immune/BBB Validation Helpers
//=============================================================================

static int fin_investment_validate_subsystems(const char* operation) {
    if (g_fin_investment_immune) {
        int rc = brain_immune_validate_operation(g_fin_investment_immune, operation, 5);
        if (rc != 0) {
            set_error("financial_investment: immune validation failed for %s", operation);
            return FIN_ERR_SUBSYSTEM;
        }
    }
    if (g_fin_investment_bbb) {
        int rc = bbb_validate_data(g_fin_investment_bbb, NULL, 0, operation);
        if (rc != 0) {
            set_error("financial_investment: BBB validation failed for %s", operation);
            return FIN_ERR_SUBSYSTEM;
        }
    }
    return FIN_ERR_OK;
}

static int investment_validate_subsystems(financial_investment_eng_t* eng, const char* operation) {
    if (!eng) return FIN_ERR_NULL;

    if (eng->enable_bbb_validation && eng->bbb) {
        int rc = bbb_validate_data(eng->bbb, NULL, 0, operation);
        if (rc != 0) {
            set_error("BBB validation failed for %s", operation);
            NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_BBB_VALIDATION,
                "financial_investment: BBB validation failed for %s", operation);
            return FIN_ERR_VALIDATION;
        }
    }

    if (eng->enable_immune_validation && eng->immune) {
        int rc = brain_immune_validate_operation(eng->immune, operation, 5);
        if (rc != 0) {
            set_error("Immune validation failed for %s", operation);
            NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_BBB_VALIDATION,
                "financial_investment: immune validation failed for %s", operation);
            return FIN_ERR_VALIDATION;
        }
    }

    return FIN_ERR_OK;
}

static void investment_present_antigen(financial_investment_eng_t* eng,
                                        const char* anomaly, uint32_t severity) {
    if (eng && eng->immune) {
        uint8_t sig[64] = {0};
        snprintf((char*)sig, sizeof(sig), "fin_investment:%s", anomaly);
        uint32_t antigen_id = 0;
        brain_immune_present_antigen(eng->immune, 0, sig, strlen((char*)sig),
                                      severity, 0, &antigen_id);
    }
}

/**
 * @brief Approximation of the standard normal CDF
 */
static float norm_cdf(float x) {
    float k = 1.0f / (1.0f + 0.2316419f * fabsf(x));
    float k2 = k * k, k3 = k2 * k, k4 = k3 * k, k5 = k4 * k;
    float w = 0.319381530f * k - 0.356563782f * k2 + 1.781477937f * k3
              - 1.821255978f * k4 + 1.330274429f * k5;
    float phi = 0.3989422804f * expf(-0.5f * x * x);
    return x >= 0 ? 1.0f - phi * w : phi * w;
}

/**
 * @brief Standard normal PDF
 */
static float norm_pdf(float x) {
    return 0.3989422804f * expf(-0.5f * x * x);
}

/**
 * @brief Comparison function for qsort (ascending floats)
 */
static int float_compare_asc(const void* a, const void* b) {
    float fa = *(const float*)a;
    float fb = *(const float*)b;
    if (fa < fb) return -1;
    if (fa > fb) return 1;
    return 0;
}

/**
 * @brief Apply modulation factor based on inflammation and fatigue
 */
static float apply_modulation(const financial_investment_eng_t* eng, float value) {
    float factor = 1.0f - eng->inflammation * eng->config.inflammation_sensitivity * 0.2f
                        - eng->fatigue * eng->config.fatigue_sensitivity * 0.3f;
    return value * fmaxf(0.4f, factor);
}

/**
 * @brief Get effective iteration count adjusted for fatigue
 */
static uint32_t effective_iterations(const financial_investment_eng_t* eng) {
    float scale = 1.0f - eng->fatigue * eng->config.fatigue_sensitivity * 0.5f;
    scale = fmaxf(0.2f, scale);
    return (uint32_t)(eng->config.max_iterations * scale);
}

/**
 * @brief Get effective Monte Carlo paths adjusted for fatigue
 */
static uint32_t effective_mc_paths(const financial_investment_eng_t* eng) {
    float scale = 1.0f - eng->fatigue * eng->config.fatigue_sensitivity * 0.5f;
    scale = fmaxf(0.2f, scale);
    return (uint32_t)(eng->config.monte_carlo_paths * scale);
}

/**
 * @brief Get effective binomial steps adjusted for fatigue
 */
static uint32_t effective_binom_steps(const financial_investment_eng_t* eng) {
    float scale = 1.0f - eng->fatigue * eng->config.fatigue_sensitivity * 0.5f;
    scale = fmaxf(0.2f, scale);
    return (uint32_t)(eng->config.binomial_tree_steps * scale);
}

//=============================================================================
// Lifecycle API
//=============================================================================

fin_config_t financial_investment_default_config(void) {
    financial_investment_heartbeat("fin_default_config", 0.0f);

    fin_config_t config;
    memset(&config, 0, sizeof(config));
    config.risk_free_rate          = 0.05f;
    config.default_horizon_years   = 1.0f;
    config.convergence_tolerance   = 1e-6f;
    config.max_iterations          = NIMCP_MAX_ITERATIONS_DEFAULT;
    config.monte_carlo_paths       = 10000;
    config.binomial_tree_steps     = 100;
    config.min_weight              = 0.0f;
    config.max_weight              = 1.0f;
    config.rebalance_threshold     = 0.05f;
    config.enable_tax_optimization = false;
    config.enable_intuition        = false;
    config.transaction_cost_bps    = 10.0f;
    config.tax_rate_short          = 0.37f;
    config.tax_rate_long           = 0.20f;
    config.inflammation_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    config.fatigue_sensitivity      = NIMCP_SENSITIVITY_DEFAULT;
    config.enable_fuzzy_logic      = true;
    config.fuzzy_bridge            = NULL;
    return config;
}

financial_investment_eng_t* financial_investment_create(void) {
    financial_investment_heartbeat("fin_create", 0.0f);

    fin_config_t config = financial_investment_default_config();
    return financial_investment_create_custom(&config);
}

financial_investment_eng_t* financial_investment_create_custom(const fin_config_t* config) {
    if (!config) {
        set_error("NULL config");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_investment_create_custom: config is NULL");
        return NULL;
    }
    financial_investment_heartbeat("fin_create_custom", 0.0f);

    financial_investment_eng_t* eng = (financial_investment_eng_t*)nimcp_calloc(
        1, sizeof(financial_investment_eng_t));
    if (!eng) {
        set_error("Failed to allocate financial_investment_eng");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NO_MEMORY, "financial_investment_create_custom: allocation failed");
        return NULL;
    }
    eng->config = *config;
    memset(&eng->stats, 0, sizeof(fin_stats_t));
    eng->inflammation = 0.0f;
    eng->fatigue = 0.0f;
    eng->fuzzy_bridge = config->fuzzy_bridge;
    eng->immune = NULL;
    eng->bbb = NULL;
    eng->kg_wiring = NULL;
    eng->enable_immune_validation = true;
    eng->enable_bbb_validation = true;
    eng->health_agent = NULL;
    eng->logger = NULL;
    /* Bio-async integration (Change Set 4) */
    eng->bio_async = NULL;
    eng->bio_router = NULL;
    eng->async_enabled = false;
    eng->total_ops = 0;
    eng->total_processing_time_us = 0.0;
    return eng;
}

void financial_investment_destroy(financial_investment_eng_t* fin) {
    financial_investment_heartbeat("fin_destroy", 0.0f);
    if (fin) {
        nimcp_free(fin);
        fin = NULL;
    }
}

//=============================================================================
// Portfolio Management
//=============================================================================

int financial_investment_portfolio_create(financial_investment_eng_t* fin,
                                          fin_portfolio_t* portfolio)
{
    if (!fin || !portfolio) {
        set_error("NULL parameter in portfolio_create");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_investment_portfolio_create: NULL parameter");
        return FIN_ERR_NULL;
    }
    financial_investment_heartbeat("fin_portfolio_create", 0.0f);

    memset(portfolio, 0, sizeof(fin_portfolio_t));
    fin->stats.portfolio_analyses++;
    return FIN_ERR_OK;
}

int financial_investment_portfolio_add_asset(financial_investment_eng_t* fin,
                                             fin_portfolio_t* portfolio,
                                             const fin_asset_t* asset, float weight)
{
    if (!fin || !portfolio || !asset) {
        set_error("NULL parameter in portfolio_add_asset");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_investment_portfolio_add_asset: NULL parameter");
        return FIN_ERR_NULL;
    }
    int val_rc = fin_investment_validate_subsystems("portfolio_add");
    if (val_rc != FIN_ERR_OK) return val_rc;
    financial_investment_heartbeat("fin_portfolio_add_asset", 0.0f);

    if (portfolio->asset_count >= FIN_MAX_PORTFOLIO_SIZE) {
        set_error("Portfolio full: %u assets", portfolio->asset_count);
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_OUT_OF_RANGE, "financial_investment_portfolio_add_asset: Portfolio full");
        return FIN_ERR_PORTFOLIO_FULL;
    }
    if (weight < 0.0f || weight > 1.0f) {
        set_error("Invalid weight: %f", (double)weight);
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_INVALID_PARAM, "financial_investment_portfolio_add_asset: Invalid weight %f", (double)weight);
        return FIN_ERR_INVALID_WEIGHT;
    }

    uint32_t idx = portfolio->asset_count;
    portfolio->assets[idx] = *asset;
    portfolio->weights[idx] = weight;
    portfolio->asset_count++;
    fin->stats.portfolio_analyses++;
    return FIN_ERR_OK;
}

int financial_investment_portfolio_remove_asset(financial_investment_eng_t* fin,
                                                fin_portfolio_t* portfolio,
                                                uint32_t asset_id)
{
    if (!fin || !portfolio) {
        set_error("NULL parameter in portfolio_remove_asset");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_investment_portfolio_remove_asset: NULL parameter");
        return FIN_ERR_NULL;
    }
    int val_rc = fin_investment_validate_subsystems("portfolio_remove");
    if (val_rc != FIN_ERR_OK) return val_rc;
    financial_investment_heartbeat("fin_portfolio_remove_asset", 0.0f);

    for (uint32_t i = 0; i < portfolio->asset_count; i++) {
        if (portfolio->assets[i].asset_id == asset_id) {
            /* Swap-remove: replace with last element */
            uint32_t last = portfolio->asset_count - 1;
            if (i != last) {
                portfolio->assets[i] = portfolio->assets[last];
                portfolio->weights[i] = portfolio->weights[last];
            }
            portfolio->asset_count--;
            fin->stats.portfolio_analyses++;
            return FIN_ERR_OK;
        }
    }
    set_error("Asset ID %u not found", asset_id);
    NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NOT_FOUND, "financial_investment_portfolio_remove_asset: Asset ID %u not found", asset_id);
    return FIN_ERR_ASSET_NOT_FOUND;
}

int financial_investment_portfolio_rebalance(financial_investment_eng_t* fin,
                                             fin_portfolio_t* portfolio,
                                             const float* target_weights)
{
    if (!fin || !portfolio || !target_weights) {
        set_error("NULL parameter in portfolio_rebalance");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_investment_portfolio_rebalance: NULL parameter");
        return FIN_ERR_NULL;
    }
    int val_rc = fin_investment_validate_subsystems("portfolio_rebalance");
    if (val_rc != FIN_ERR_OK) return val_rc;
    int vrc = investment_validate_subsystems(fin, "portfolio_rebalance");
    if (vrc != FIN_ERR_OK) return vrc;
    financial_investment_heartbeat("fin_portfolio_rebalance", 0.0f);

    uint32_t n = portfolio->asset_count;
    if (n == 0) {
        return FIN_ERR_OK;
    }

    /* Copy target weights */
    float sum = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        sum += target_weights[i];
    }

    /* Normalize to sum=1 if needed */
    if (fabsf(sum) < EPSILON) {
        set_error("Target weights sum to zero");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_INVALID_PARAM, "financial_investment_portfolio_rebalance: Target weights sum to zero");
        return FIN_ERR_INVALID_WEIGHT;
    }

    float inv_sum = 1.0f / (fabsf(sum) > 1e-7f ? sum : 1e-7f);
    for (uint32_t i = 0; i < n; i++) {
        portfolio->weights[i] = target_weights[i] * inv_sum;
    }

    /* Fuzzy rebalance urgency if enabled */
    if (fin->config.enable_fuzzy_logic) {
        /* Compute maximum drift from target */
        float max_drift = 0.0f;
        for (uint32_t i = 0; i < n; i++) {
            float drift = fabsf(portfolio->weights[i] - target_weights[i] * inv_sum);
            if (drift > max_drift) max_drift = drift;
        }
        /* Use Gaussian MF to compute rebalance urgency: high urgency when drift is large */
        fuzzy_mf_t urgency_mf = fuzzy_mf_s_shaped(
            fin->config.rebalance_threshold * 0.5f,
            fin->config.rebalance_threshold * 2.0f);
        float urgency = fuzzy_mf_evaluate(&urgency_mf, max_drift);
        (void)urgency; /* Urgency used for logging/diagnostics */
    }

    fin->stats.portfolio_analyses++;
    return FIN_ERR_OK;
}

float financial_investment_portfolio_return(const financial_investment_eng_t* fin,
                                            const fin_portfolio_t* portfolio)
{
    if (!fin || !portfolio) {
        set_error("NULL parameter in portfolio_return");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_investment_portfolio_return: NULL parameter");
        return 0.0f;
    }
    financial_investment_heartbeat("fin_portfolio_return", 0.0f);

    float ret = 0.0f;
    for (uint32_t i = 0; i < portfolio->asset_count; i++) {
        ret += portfolio->weights[i] * portfolio->assets[i].expected_return;
    }
    return ret;
}

float financial_investment_portfolio_volatility(const financial_investment_eng_t* fin,
                                                const fin_portfolio_t* portfolio,
                                                const float* correlation_matrix)
{
    if (!fin || !portfolio || !correlation_matrix) {
        set_error("NULL parameter in portfolio_volatility");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_investment_portfolio_volatility: NULL parameter");
        return 0.0f;
    }
    financial_investment_heartbeat("fin_portfolio_volatility", 0.0f);

    uint32_t n = portfolio->asset_count;
    if (n == 0) return 0.0f;

    /* Compute w^T * Cov * w where Cov_ij = corr_ij * vol_i * vol_j */
    float variance = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        for (uint32_t j = 0; j < n; j++) {
            float cov_ij = correlation_matrix[i * n + j]
                         * portfolio->assets[i].volatility
                         * portfolio->assets[j].volatility;
            variance += portfolio->weights[i] * portfolio->weights[j] * cov_ij;
        }
    }

    if (variance < 0.0f) variance = 0.0f;
    return sqrtf(variance);
}

//=============================================================================
// Risk Assessment
//=============================================================================

int financial_investment_assess_risk(financial_investment_eng_t* fin,
                                     const fin_portfolio_t* portfolio,
                                     const float* correlation_matrix,
                                     const float* returns_history,
                                     uint32_t history_length,
                                     fin_risk_metrics_t* out_metrics)
{
    if (!fin || !portfolio || !correlation_matrix || !returns_history || !out_metrics) {
        set_error("NULL parameter in assess_risk");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_investment_assess_risk: NULL parameter");
        return FIN_ERR_NULL;
    }
    if (history_length < 2) {
        set_error("History too short: %u", history_length);
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_INVALID_PARAM, "financial_investment_assess_risk: History too short %u", history_length);
        return FIN_ERR_HISTORY_TOO_SHORT;
    }
    int val_rc = fin_investment_validate_subsystems("assess_risk");
    if (val_rc != FIN_ERR_OK) return val_rc;
    int vrc = investment_validate_subsystems(fin, "assess_risk");
    if (vrc != FIN_ERR_OK) return vrc;
    financial_investment_heartbeat("fin_assess_risk", 0.0f);

    memset(out_metrics, 0, sizeof(fin_risk_metrics_t));
    uint32_t n = portfolio->asset_count;

    /* Portfolio returns from history (weighted sum) */
    float* port_returns = (float*)nimcp_calloc(history_length, sizeof(float));
    if (!port_returns) {
        set_error("Allocation failed for portfolio returns");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NO_MEMORY, "financial_investment_assess_risk: Allocation failed");
        return FIN_ERR_ALLOC;
    }

    /* Compute portfolio returns per period */
    for (uint32_t t = 0; t < history_length; t++) {
        float r = 0.0f;
        for (uint32_t i = 0; i < n && i < FIN_MAX_PORTFOLIO_SIZE; i++) {
            /* Assume returns_history is stored column-major: asset i, time t */
            r += portfolio->weights[i] * returns_history[i * history_length + t];
        }
        port_returns[t] = r;
    }

    /* Mean return */
    float mean_ret = 0.0f;
    for (uint32_t t = 0; t < history_length; t++) {
        mean_ret += port_returns[t];
    }
    mean_ret /= (float)history_length;

    /* Volatility (annualized, assuming daily returns -> *sqrt(252)) */
    float var_sum = 0.0f;
    float downside_sum = 0.0f;
    for (uint32_t t = 0; t < history_length; t++) {
        float diff = port_returns[t] - mean_ret;
        var_sum += diff * diff;
        if (port_returns[t] < 0.0f) {
            downside_sum += port_returns[t] * port_returns[t];
        }
    }
    float daily_vol = sqrtf(var_sum / (float)(history_length - 1));
    float annual_vol = daily_vol * sqrtf(252.0f);
    out_metrics->volatility_annual = annual_vol;

    /* Downside deviation */
    out_metrics->downside_deviation = sqrtf(downside_sum / (float)history_length) * sqrtf(252.0f);

    /* VaR and CVaR at 95% and 99% */
    out_metrics->var_95 = financial_investment_compute_var(fin, port_returns, history_length, 0.95f);
    out_metrics->var_99 = financial_investment_compute_var(fin, port_returns, history_length, 0.99f);
    out_metrics->cvar_95 = financial_investment_compute_cvar(fin, port_returns, history_length, 0.95f);
    out_metrics->cvar_99 = financial_investment_compute_cvar(fin, port_returns, history_length, 0.99f);

    /* Sharpe ratio */
    float daily_rf = fin->config.risk_free_rate / 252.0f;
    float excess_mean = mean_ret - daily_rf;
    out_metrics->sharpe_ratio = (daily_vol > EPSILON)
        ? (excess_mean / daily_vol) * sqrtf(252.0f)
        : 0.0f;

    /* Sortino ratio */
    float ds = out_metrics->downside_deviation;
    out_metrics->sortino_ratio = (ds > EPSILON)
        ? (mean_ret * 252.0f - fin->config.risk_free_rate) / ds
        : 0.0f;

    /* Max drawdown */
    /* Reconstruct cumulative returns for drawdown */
    float* cum_values = (float*)nimcp_calloc(history_length, sizeof(float));
    if (cum_values) {
        cum_values[0] = 1.0f + port_returns[0];
        for (uint32_t t = 1; t < history_length; t++) {
            cum_values[t] = cum_values[t - 1] * (1.0f + port_returns[t]);
        }
        out_metrics->max_drawdown = financial_investment_max_drawdown(cum_values, history_length);
        nimcp_free(cum_values);
        cum_values = NULL;
    }

    /* Present antigen for extreme drawdown (Phase 9) */
    if (out_metrics->max_drawdown > 0.20f) {
        investment_present_antigen(fin, "extreme_drawdown", 8);
    }

    /* Calmar ratio */
    out_metrics->calmar_ratio = (out_metrics->max_drawdown > EPSILON)
        ? (mean_ret * 252.0f) / out_metrics->max_drawdown
        : 0.0f;

    /* Treynor ratio - requires portfolio beta */
    /* Compute portfolio beta (weighted average of asset betas) */
    float port_beta = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        port_beta += portfolio->weights[i] * portfolio->assets[i].beta;
    }
    out_metrics->portfolio_beta = port_beta;

    out_metrics->treynor_ratio = (fabsf(port_beta) > EPSILON)
        ? (mean_ret * 252.0f - fin->config.risk_free_rate) / port_beta
        : 0.0f;

    /* Portfolio alpha (Jensen's alpha) */
    float market_return = mean_ret * 252.0f; /* Simplified: use portfolio mean as proxy */
    out_metrics->portfolio_alpha = mean_ret * 252.0f
        - fin->config.risk_free_rate
        - port_beta * (market_return - fin->config.risk_free_rate);

    /* Tracking error (simplified: std dev of active returns) */
    out_metrics->tracking_error = daily_vol * sqrtf(252.0f) * 0.5f;

    /* Information ratio */
    out_metrics->information_ratio = (out_metrics->tracking_error > EPSILON)
        ? out_metrics->portfolio_alpha / out_metrics->tracking_error
        : 0.0f;

    /* Herfindahl index (concentration) */
    float hhi = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        hhi += portfolio->weights[i] * portfolio->weights[i];
    }
    out_metrics->herfindahl_index = hhi;

    /* Diversification ratio: sum of weighted vols / portfolio vol */
    float weighted_vol_sum = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        weighted_vol_sum += portfolio->weights[i] * portfolio->assets[i].volatility;
    }
    float port_vol = financial_investment_portfolio_volatility(fin, portfolio, correlation_matrix);
    out_metrics->diversification_ratio = (port_vol > EPSILON)
        ? weighted_vol_sum / port_vol
        : 1.0f;

    /* Fuzzy risk grade and diversification quality */
    if (fin->config.enable_fuzzy_logic) {
        /* Fuzzy risk grade: composite of VaR, volatility, max_drawdown */
        fuzzy_mf_t risk_vol_mf = fuzzy_mf_s_shaped(0.10f, 0.40f);
        float vol_risk = fuzzy_mf_evaluate(&risk_vol_mf, annual_vol);

        fuzzy_mf_t risk_dd_mf = fuzzy_mf_s_shaped(0.05f, 0.30f);
        float dd_risk = fuzzy_mf_evaluate(&risk_dd_mf, out_metrics->max_drawdown);

        fuzzy_mf_t risk_var_mf = fuzzy_mf_s_shaped(0.01f, 0.10f);
        float var_risk = fuzzy_mf_evaluate(&risk_var_mf, fabsf(out_metrics->var_95));

        /* Inflammation increases perceived risk */
        float inflam_boost = 1.0f + fin->inflammation * fin->config.inflammation_sensitivity * 0.3f;
        out_metrics->fuzzy_risk_grade = nimcp_clamp01(
            (vol_risk * 0.4f + dd_risk * 0.35f + var_risk * 0.25f) * inflam_boost);

        /* Fuzzy diversification quality */
        fuzzy_mf_t div_mf = fuzzy_mf_gaussian(1.5f, 0.5f);
        float div_score = fuzzy_mf_evaluate(&div_mf, out_metrics->diversification_ratio);

        fuzzy_mf_t conc_mf = fuzzy_mf_z_shaped(0.1f, 0.5f);
        float conc_score = fuzzy_mf_evaluate(&conc_mf, hhi);

        out_metrics->fuzzy_diversification_quality = nimcp_clamp01(
            div_score * 0.6f + conc_score * 0.4f);
    }

    nimcp_free(port_returns);
    fin->stats.risk_assessments++;
    return FIN_ERR_OK;
}

float financial_investment_compute_var(financial_investment_eng_t* fin,
                                       const float* returns, uint32_t count,
                                       float confidence)
{
    if (!fin || !returns || count == 0) {
        set_error("Invalid parameters in compute_var");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_investment_compute_var: Invalid parameters");
        return 0.0f;
    }
    if (confidence <= 0.0f || confidence >= 1.0f) {
        set_error("Invalid confidence level: %f", (double)confidence);
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_INVALID_PARAM, "financial_investment_compute_var: Invalid confidence level");
        return 0.0f;
    }
    int val_rc = fin_investment_validate_subsystems("compute_var");
    if (val_rc != FIN_ERR_OK) return 0.0f;
    int vrc = investment_validate_subsystems(fin, "compute_var");
    if (vrc != FIN_ERR_OK) return 0.0f;
    financial_investment_heartbeat("fin_compute_var", 0.0f);

    /* Copy and sort returns ascending */
    float* sorted = (float*)nimcp_calloc(count, sizeof(float));
    if (!sorted) {
        set_error("Allocation failed in compute_var");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NO_MEMORY, "financial_investment_compute_var: Allocation failed");
        return 0.0f;
    }
    memcpy(sorted, returns, count * sizeof(float));
    qsort(sorted, count, sizeof(float), float_compare_asc);

    /* VaR at given confidence: the (1-confidence) percentile of losses */
    uint32_t index = (uint32_t)((1.0f - confidence) * (float)count);
    if (index >= count) index = count - 1;
    float var = -sorted[index]; /* Negative because VaR is a loss measure */

    /* Check for VaR exceedance: if worst actual loss exceeds VaR (Phase 9) */
    float worst_actual_loss = -sorted[0]; /* Most negative return = largest loss */
    if (worst_actual_loss > var) {
        investment_present_antigen(fin, "var_exceedance", 7);
    }

    nimcp_free(sorted);
    sorted = NULL;
    return var;
}

float financial_investment_compute_cvar(financial_investment_eng_t* fin,
                                        const float* returns, uint32_t count,
                                        float confidence)
{
    if (!fin || !returns || count == 0) {
        set_error("Invalid parameters in compute_cvar");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_investment_compute_cvar: Invalid parameters");
        return 0.0f;
    }
    if (confidence <= 0.0f || confidence >= 1.0f) {
        set_error("Invalid confidence level: %f", (double)confidence);
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_INVALID_PARAM, "financial_investment_compute_cvar: Invalid confidence level");
        return 0.0f;
    }
    financial_investment_heartbeat("fin_compute_cvar", 0.0f);

    /* Copy and sort returns ascending */
    float* sorted = (float*)nimcp_calloc(count, sizeof(float));
    if (!sorted) {
        set_error("Allocation failed in compute_cvar");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NO_MEMORY, "financial_investment_compute_cvar: Allocation failed");
        return 0.0f;
    }
    memcpy(sorted, returns, count * sizeof(float));
    qsort(sorted, count, sizeof(float), float_compare_asc);

    /* VaR index */
    uint32_t var_idx = (uint32_t)((1.0f - confidence) * (float)count);
    if (var_idx >= count) var_idx = count - 1;
    if (var_idx == 0) var_idx = 1;

    /* CVaR: average of returns below VaR */
    float cvar_sum = 0.0f;
    for (uint32_t i = 0; i < var_idx; i++) {
        cvar_sum += sorted[i];
    }
    float cvar = -cvar_sum / (float)var_idx;

    nimcp_free(sorted);
    sorted = NULL;
    return cvar;
}

float financial_investment_max_drawdown(const float* prices, uint32_t count) {
    if (!prices || count < 2) return 0.0f;

    float peak = prices[0];
    float max_dd = 0.0f;

    for (uint32_t i = 1; i < count; i++) {
        if (prices[i] > peak) {
            peak = prices[i];
        }
        if (peak > EPSILON) {
            float dd = (peak - prices[i]) / peak;
            if (dd > max_dd) max_dd = dd;
        }
    }
    return max_dd;
}

//=============================================================================
// Derivatives Pricing
//=============================================================================

float financial_investment_black_scholes(float spot, float strike,
                                         float rate, float vol,
                                         float time, fin_option_type_t type)
{
    if (spot <= 0.0f || strike <= 0.0f || vol <= 0.0f || time <= 0.0f) {
        return 0.0f;
    }

    float sqrt_t = sqrtf(time);
    float d1 = (logf(spot / strike) + (rate + 0.5f * vol * vol) * time)
               / (vol * sqrt_t);
    float d2 = d1 - vol * sqrt_t;

    float price = 0.0f;
    if (type == FIN_OPT_CALL) {
        price = spot * norm_cdf(d1) - strike * expf(-rate * time) * norm_cdf(d2);
    } else {
        price = strike * expf(-rate * time) * norm_cdf(-d2) - spot * norm_cdf(-d1);
    }
    return fmaxf(price, 0.0f);
}

int financial_investment_price_option(financial_investment_eng_t* fin,
                                      float spot, float strike,
                                      float rate, float volatility,
                                      float time_to_expiry,
                                      fin_option_type_t type,
                                      fin_pricing_model_t model,
                                      fin_option_result_t* out_result)
{
    if (!fin || !out_result) {
        set_error("NULL parameter in price_option");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_investment_price_option: NULL parameter");
        return FIN_ERR_NULL;
    }
    if (spot <= 0.0f || strike <= 0.0f || volatility <= 0.0f || time_to_expiry <= 0.0f) {
        set_error("Invalid option parameters: S=%.4f K=%.4f vol=%.4f T=%.4f",
                  (double)spot, (double)strike, (double)volatility, (double)time_to_expiry);
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_INVALID_PARAM, "financial_investment_price_option: Invalid option parameters");
        return FIN_ERR_INVALID_PARAMS;
    }
    int val_rc = fin_investment_validate_subsystems("price_option");
    if (val_rc != FIN_ERR_OK) return val_rc;
    financial_investment_heartbeat("fin_price_option", 0.0f);

    memset(out_result, 0, sizeof(fin_option_result_t));

    int rc = FIN_ERR_OK;

    switch (model) {
    case FIN_PRICING_BLACK_SCHOLES: {
        out_result->price = financial_investment_black_scholes(
            spot, strike, rate, volatility, time_to_expiry, type);

        /* Compute Greeks */
        float sqrt_t = sqrtf(time_to_expiry);
        float d1 = (logf(spot / strike) + (rate + 0.5f * volatility * volatility) * time_to_expiry)
                   / (volatility * sqrt_t);
        float d2 = d1 - volatility * sqrt_t;
        float nd1_pdf = norm_pdf(d1);
        float discount = expf(-rate * time_to_expiry);

        if (type == FIN_OPT_CALL) {
            out_result->delta = norm_cdf(d1);
            out_result->theta = (-spot * nd1_pdf * volatility / (2.0f * sqrt_t)
                                 - rate * strike * discount * norm_cdf(d2)) / 365.0f;
            out_result->rho = strike * time_to_expiry * discount * norm_cdf(d2) / 100.0f;
        } else {
            out_result->delta = norm_cdf(d1) - 1.0f;
            out_result->theta = (-spot * nd1_pdf * volatility / (2.0f * sqrt_t)
                                 + rate * strike * discount * norm_cdf(-d2)) / 365.0f;
            out_result->rho = -strike * time_to_expiry * discount * norm_cdf(-d2) / 100.0f;
        }
        out_result->gamma = nd1_pdf / (spot * volatility * sqrt_t);
        out_result->vega = spot * nd1_pdf * sqrt_t / 100.0f;

        /* Higher-order Greeks */
        out_result->charm = -nd1_pdf * (2.0f * rate * time_to_expiry - d2 * volatility * sqrt_t)
                           / (2.0f * time_to_expiry * volatility * sqrt_t);
        out_result->vanna = -nd1_pdf * d2 / volatility;

        out_result->implied_volatility = volatility;
        out_result->steps_used = 0;
        break;
    }
    case FIN_PRICING_BINOMIAL_TREE: {
        uint32_t steps = effective_binom_steps(fin);
        rc = financial_investment_binomial_tree(fin, spot, strike, rate, volatility,
                                               time_to_expiry, steps, type,
                                               FIN_OPT_STYLE_EUROPEAN, out_result);
        break;
    }
    case FIN_PRICING_MONTE_CARLO: {
        /* Monte Carlo pricing */
        uint32_t paths = effective_mc_paths(fin);
        float dt = time_to_expiry;
        float drift = (rate - 0.5f * volatility * volatility) * dt;
        float diffusion = volatility * sqrtf(dt);
        float payoff_sum = 0.0f;

        /* Simple pseudo-random MC using a basic LCG for reproducibility */
        uint64_t seed = 12345678901ULL;
        for (uint32_t p = 0; p < paths; p++) {
            /* Box-Muller for normal random */
            seed = seed * 6364136223846793005ULL + 1442695040888963407ULL;
            float u1 = (float)((seed >> 33) + 1) / (float)(1ULL << 31);
            seed = seed * 6364136223846793005ULL + 1442695040888963407ULL;
            float u2 = (float)((seed >> 33) + 1) / (float)(1ULL << 31);
            u1 = nimcp_clampf(u1, 1e-7f, 1.0f - 1e-7f);
            u2 = nimcp_clampf(u2, 1e-7f, 1.0f - 1e-7f);
            float z = sqrtf(-2.0f * logf(u1)) * cosf(2.0f * PI_VAL * u2);

            float st = spot * expf(drift + diffusion * z);

            float payoff = 0.0f;
            if (type == FIN_OPT_CALL) {
                payoff = fmaxf(st - strike, 0.0f);
            } else {
                payoff = fmaxf(strike - st, 0.0f);
            }
            payoff_sum += payoff;
        }

        out_result->price = expf(-rate * time_to_expiry) * payoff_sum / (float)paths;

        /* Approximate Greeks via finite differences */
        float ds = spot * 0.01f;
        float dv = 0.01f;

        /* Delta via bump-and-reprice (simplified) */
        float price_up = 0.0f, price_down = 0.0f;
        seed = 12345678901ULL;
        for (uint32_t p = 0; p < paths; p++) {
            seed = seed * 6364136223846793005ULL + 1442695040888963407ULL;
            float u1 = (float)((seed >> 33) + 1) / (float)(1ULL << 31);
            seed = seed * 6364136223846793005ULL + 1442695040888963407ULL;
            float u2 = (float)((seed >> 33) + 1) / (float)(1ULL << 31);
            u1 = nimcp_clampf(u1, 1e-7f, 1.0f - 1e-7f);
            u2 = nimcp_clampf(u2, 1e-7f, 1.0f - 1e-7f);
            float z = sqrtf(-2.0f * logf(u1)) * cosf(2.0f * PI_VAL * u2);

            float st_up = (spot + ds) * expf(drift + diffusion * z);
            float st_dn = (spot - ds) * expf(drift + diffusion * z);

            if (type == FIN_OPT_CALL) {
                price_up += fmaxf(st_up - strike, 0.0f);
                price_down += fmaxf(st_dn - strike, 0.0f);
            } else {
                price_up += fmaxf(strike - st_up, 0.0f);
                price_down += fmaxf(strike - st_dn, 0.0f);
            }
        }
        float disc = expf(-rate * time_to_expiry);
        price_up = disc * price_up / (float)paths;
        price_down = disc * price_down / (float)paths;

        out_result->delta = (price_up - price_down) / (2.0f * ds);
        out_result->gamma = (price_up - 2.0f * out_result->price + price_down) / (ds * ds);

        /* Vega via BS approximation */
        float sqrt_t = sqrtf(time_to_expiry);
        float d1 = (logf(spot / strike) + (rate + 0.5f * volatility * volatility) * time_to_expiry)
                   / (volatility * sqrt_t);
        out_result->vega = spot * norm_pdf(d1) * sqrt_t / 100.0f;
        out_result->theta = 0.0f; /* Simplified */
        out_result->rho = 0.0f;
        out_result->charm = 0.0f;
        out_result->vanna = 0.0f;
        out_result->implied_volatility = volatility;
        out_result->steps_used = paths;

        fin->stats.monte_carlo_runs++;
        break;
    }
    default:
        set_error("Unknown pricing model: %d", (int)model);
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_INVALID_PARAM, "financial_investment_price_option: Unknown pricing model %d", (int)model);
        rc = FIN_ERR_PRICING_FAILED;
        break;
    }

    /* Validate output for NaN/Inf - sanitize if invalid */
    if (rc == FIN_ERR_OK) {
        if (isnan(out_result->price) || isinf(out_result->price)) {
            out_result->price = 0.0f;
            rc = FIN_ERR_PRICING_FAILED;
        }
        /* Sanitize Greeks - replace NaN/Inf with 0 */
        if (isnan(out_result->delta) || isinf(out_result->delta)) out_result->delta = 0.0f;
        if (isnan(out_result->gamma) || isinf(out_result->gamma)) out_result->gamma = 0.0f;
        if (isnan(out_result->theta) || isinf(out_result->theta)) out_result->theta = 0.0f;
        if (isnan(out_result->vega) || isinf(out_result->vega)) out_result->vega = 0.0f;
        if (isnan(out_result->rho) || isinf(out_result->rho)) out_result->rho = 0.0f;
    }

    fin->stats.option_pricings++;
    return rc;
}

int financial_investment_binomial_tree(financial_investment_eng_t* fin,
                                       float spot, float strike,
                                       float rate, float vol,
                                       float time, uint32_t steps,
                                       fin_option_type_t type,
                                       fin_option_style_t style,
                                       fin_option_result_t* out_result)
{
    if (!fin || !out_result) {
        set_error("NULL parameter in binomial_tree");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_investment_binomial_tree: NULL parameter");
        return FIN_ERR_NULL;
    }
    if (spot <= 0.0f || strike <= 0.0f || vol <= 0.0f || time <= 0.0f || steps == 0) {
        set_error("Invalid binomial parameters");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_INVALID_PARAM, "financial_investment_binomial_tree: Invalid binomial parameters");
        return FIN_ERR_INVALID_PARAMS;
    }
    financial_investment_heartbeat("fin_binomial_tree", 0.0f);

    memset(out_result, 0, sizeof(fin_option_result_t));

    float dt = time / (float)steps;
    float u = expf(vol * sqrtf(dt));         /* Up factor */
    float d = 1.0f / u;                      /* Down factor */
    float disc = expf(-rate * dt);            /* Discount factor per step */
    float p = (expf(rate * dt) - d) / (u - d); /* Risk-neutral probability */

    /* Allocate price tree at terminal nodes */
    float* prices = (float*)nimcp_calloc((steps + 1), sizeof(float));
    if (!prices) {
        set_error("Allocation failed in binomial_tree");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NO_MEMORY, "financial_investment_binomial_tree: Allocation failed");
        return FIN_ERR_ALLOC;
    }

    /* Terminal payoffs */
    for (uint32_t i = 0; i <= steps; i++) {
        float st = spot * powf(u, (float)(steps - i)) * powf(d, (float)i);
        if (type == FIN_OPT_CALL) {
            prices[i] = fmaxf(st - strike, 0.0f);
        } else {
            prices[i] = fmaxf(strike - st, 0.0f);
        }
    }

    /* Backward induction */
    for (int step = (int)steps - 1; step >= 0; step--) {
        for (int i = 0; i <= step; i++) {
            float continuation = disc * (p * prices[i] + (1.0f - p) * prices[i + 1]);

            /* For American options, check early exercise */
            if (style == FIN_OPT_STYLE_AMERICAN) {
                float st = spot * powf(u, (float)(step - i)) * powf(d, (float)i);
                float exercise = 0.0f;
                if (type == FIN_OPT_CALL) {
                    exercise = fmaxf(st - strike, 0.0f);
                } else {
                    exercise = fmaxf(strike - st, 0.0f);
                }
                prices[i] = fmaxf(continuation, exercise);
            } else {
                prices[i] = continuation;
            }
        }
    }

    out_result->price = prices[0];
    out_result->steps_used = steps;
    out_result->implied_volatility = vol;

    /* Approximate delta from first step */
    if (steps >= 1) {
        float su = spot * u;
        float sd = spot * d;
        float payoff_u, payoff_d;
        if (type == FIN_OPT_CALL) {
            payoff_u = financial_investment_black_scholes(su, strike, rate, vol,
                                                          time - dt, type);
            payoff_d = financial_investment_black_scholes(sd, strike, rate, vol,
                                                          time - dt, type);
        } else {
            payoff_u = financial_investment_black_scholes(su, strike, rate, vol,
                                                          time - dt, type);
            payoff_d = financial_investment_black_scholes(sd, strike, rate, vol,
                                                          time - dt, type);
        }
        out_result->delta = (payoff_u - payoff_d) / (su - sd);
    }

    /* Approximate other Greeks via BS as reference */
    float sqrt_t = sqrtf(time);
    float d1 = (logf(spot / strike) + (rate + 0.5f * vol * vol) * time) / (vol * sqrt_t);
    out_result->gamma = norm_pdf(d1) / (spot * vol * sqrt_t);
    out_result->vega = spot * norm_pdf(d1) * sqrt_t / 100.0f;
    out_result->theta = 0.0f;
    out_result->rho = 0.0f;
    out_result->charm = 0.0f;
    out_result->vanna = 0.0f;

    nimcp_free(prices);
    prices = NULL;
    fin->stats.option_pricings++;
    return FIN_ERR_OK;
}

float financial_investment_implied_vol(float market_price,
                                       float spot, float strike,
                                       float rate, float time,
                                       fin_option_type_t type)
{
    if (market_price <= 0.0f || spot <= 0.0f || strike <= 0.0f || time <= 0.0f) {
        return 0.0f;
    }

    /* Newton-Raphson iteration */
    float vol = 0.20f; /* Initial guess */
    float sqrt_t = sqrtf(time);

    for (int iter = 0; iter < 100; iter++) {
        float bs_price = financial_investment_black_scholes(spot, strike, rate, vol, time, type);
        float diff = bs_price - market_price;

        if (fabsf(diff) < FIN_PRICE_PRECISION) {
            return vol;
        }

        /* Vega = S * N'(d1) * sqrt(T) */
        float d1 = (logf(spot / strike) + (rate + 0.5f * vol * vol) * time)
                   / (vol * sqrt_t);
        float vega = spot * norm_pdf(d1) * sqrt_t;

        if (fabsf(vega) < EPSILON) {
            break;
        }

        vol -= diff / vega;
        vol = nimcp_clampf(vol, 0.001f, 5.0f);
    }

    return vol;
}

//=============================================================================
// Asset Valuation
//=============================================================================

int financial_investment_dcf_valuation(financial_investment_eng_t* fin,
                                       const float* cash_flows,
                                       uint32_t num_periods,
                                       float discount_rate,
                                       float terminal_growth,
                                       fin_valuation_result_t* out_result)
{
    if (!fin || !cash_flows || !out_result) {
        set_error("NULL parameter in dcf_valuation");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_investment_dcf_valuation: NULL parameter");
        return FIN_ERR_NULL;
    }
    if (num_periods == 0 || discount_rate <= terminal_growth) {
        set_error("Invalid DCF parameters: periods=%u, r=%.4f, g=%.4f",
                  num_periods, (double)discount_rate, (double)terminal_growth);
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_INVALID_PARAM, "financial_investment_dcf_valuation: Invalid DCF parameters");
        return FIN_ERR_INVALID_PARAMS;
    }
    financial_investment_heartbeat("fin_dcf_valuation", 0.0f);

    memset(out_result, 0, sizeof(fin_valuation_result_t));
    out_result->method = FIN_VALUATION_DCF;

    /* Sum PV of projected cash flows */
    float pv_sum = 0.0f;
    for (uint32_t t = 0; t < num_periods; t++) {
        float disc = powf(1.0f + discount_rate, (float)(t + 1));
        pv_sum += cash_flows[t] / disc;
    }

    /* Terminal value: CF_last * (1+g) / (r-g), discounted back */
    float last_cf = cash_flows[num_periods - 1];
    float terminal_value = last_cf * (1.0f + terminal_growth)
                         / (discount_rate - terminal_growth);
    float disc_terminal = powf(1.0f + discount_rate, (float)num_periods);
    float pv_terminal = terminal_value / disc_terminal;

    out_result->intrinsic_value = apply_modulation(fin, pv_sum + pv_terminal);

    /* Margin of safety: assume current price needs to be provided externally.
       For now, set a default confidence */
    out_result->confidence = 0.7f;
    out_result->margin_of_safety = 0.0f;
    out_result->upside_potential = 0.0f;

    fin->stats.valuations++;
    return FIN_ERR_OK;
}

int financial_investment_ddm_valuation(financial_investment_eng_t* fin,
                                       float current_dividend,
                                       float growth_rate,
                                       float required_return,
                                       fin_valuation_result_t* out_result)
{
    if (!fin || !out_result) {
        set_error("NULL parameter in ddm_valuation");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_investment_ddm_valuation: NULL parameter");
        return FIN_ERR_NULL;
    }
    if (required_return <= growth_rate) {
        set_error("DDM requires r > g: r=%.4f, g=%.4f",
                  (double)required_return, (double)growth_rate);
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_INVALID_PARAM, "financial_investment_ddm_valuation: DDM requires r > g");
        return FIN_ERR_INVALID_PARAMS;
    }
    financial_investment_heartbeat("fin_ddm_valuation", 0.0f);

    memset(out_result, 0, sizeof(fin_valuation_result_t));
    out_result->method = FIN_VALUATION_DDM;

    /* Gordon Growth Model: V = D * (1+g) / (r - g) */
    float value = current_dividend * (1.0f + growth_rate) / (required_return - growth_rate);
    out_result->intrinsic_value = apply_modulation(fin, value);
    out_result->confidence = 0.65f;
    out_result->margin_of_safety = 0.0f;
    out_result->upside_potential = 0.0f;

    fin->stats.valuations++;
    return FIN_ERR_OK;
}

int financial_investment_comparables(financial_investment_eng_t* fin,
                                     const float* peer_multiples,
                                     uint32_t peer_count,
                                     float target_metric,
                                     fin_valuation_result_t* out_result)
{
    if (!fin || !peer_multiples || !out_result) {
        set_error("NULL parameter in comparables");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_investment_comparables: NULL parameter");
        return FIN_ERR_NULL;
    }
    if (peer_count == 0) {
        set_error("No peer multiples provided");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_INVALID_PARAM, "financial_investment_comparables: No peer multiples provided");
        return FIN_ERR_INVALID_PARAMS;
    }
    financial_investment_heartbeat("fin_comparables", 0.0f);

    memset(out_result, 0, sizeof(fin_valuation_result_t));
    out_result->method = FIN_VALUATION_COMPARABLES;

    /* Find median of peer multiples */
    float* sorted = (float*)nimcp_calloc(peer_count, sizeof(float));
    if (!sorted) {
        set_error("Allocation failed in comparables");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NO_MEMORY, "financial_investment_comparables: Allocation failed");
        return FIN_ERR_ALLOC;
    }
    memcpy(sorted, peer_multiples, peer_count * sizeof(float));
    qsort(sorted, peer_count, sizeof(float), float_compare_asc);

    float median = 0.0f;
    if (peer_count % 2 == 0) {
        median = (sorted[peer_count / 2 - 1] + sorted[peer_count / 2]) * 0.5f;
    } else {
        median = sorted[peer_count / 2];
    }

    out_result->intrinsic_value = apply_modulation(fin, median * target_metric);

    /* Confidence decreases with higher dispersion */
    float mean = 0.0f;
    for (uint32_t i = 0; i < peer_count; i++) {
        mean += sorted[i];
    }
    mean /= (float)peer_count;

    float var = 0.0f;
    for (uint32_t i = 0; i < peer_count; i++) {
        float diff = sorted[i] - mean;
        var += diff * diff;
    }
    var /= (float)peer_count;
    float cv = (fabsf(mean) > EPSILON) ? sqrtf(var) / fabsf(mean) : 1.0f;

    out_result->confidence = nimcp_clamp01(1.0f - cv * 0.5f);
    out_result->margin_of_safety = 0.0f;
    out_result->upside_potential = 0.0f;

    nimcp_free(sorted);
    sorted = NULL;
    fin->stats.valuations++;
    return FIN_ERR_OK;
}

//=============================================================================
// Portfolio Optimization
//=============================================================================

/**
 * @brief Internal: Mean-variance optimization via gradient descent
 */
static int optimize_mean_variance(const financial_investment_eng_t* fin,
                                   const fin_portfolio_t* portfolio,
                                   const float* expected_returns,
                                   const float* covariance_matrix,
                                   float risk_aversion,
                                   fin_optimization_result_t* out_result)
{
    uint32_t n = portfolio->asset_count;
    uint32_t max_iter = effective_iterations(fin);
    float tol = fin->config.convergence_tolerance;
    float lr = NIMCP_LEARNING_RATE_DEFAULT;

    /* Initialize weights equally */
    for (uint32_t i = 0; i < n; i++) {
        out_result->optimal_weights[i] = 1.0f / (float)n;
    }

    float prev_obj = -FLT_MAX;
    out_result->converged = false;

    for (uint32_t iter = 0; iter < max_iter; iter++) {
        /* Compute gradient of utility = E[r] - 0.5 * lambda * w^T Cov w */
        /* dU/dw_i = mu_i - lambda * sum_j(Cov_ij * w_j) */
        float obj = 0.0f;
        float port_ret = 0.0f;
        float port_var = 0.0f;

        for (uint32_t i = 0; i < n; i++) {
            port_ret += out_result->optimal_weights[i] * expected_returns[i];
        }

        for (uint32_t i = 0; i < n; i++) {
            for (uint32_t j = 0; j < n; j++) {
                port_var += out_result->optimal_weights[i]
                          * out_result->optimal_weights[j]
                          * covariance_matrix[i * n + j];
            }
        }

        obj = port_ret - 0.5f * risk_aversion * port_var;

        /* Check convergence */
        if (iter > 0 && fabsf(obj - prev_obj) < tol) {
            out_result->converged = true;
            out_result->iterations = iter;
            break;
        }
        prev_obj = obj;

        /* Gradient step */
        float sum_w = 0.0f;
        for (uint32_t i = 0; i < n; i++) {
            float grad = expected_returns[i];
            for (uint32_t j = 0; j < n; j++) {
                grad -= risk_aversion * covariance_matrix[i * n + j]
                      * out_result->optimal_weights[j];
            }
            out_result->optimal_weights[i] += lr * grad;
            /* Clamp weights */
            out_result->optimal_weights[i] = nimcp_clampf(
                out_result->optimal_weights[i],
                fin->config.min_weight,
                fin->config.max_weight);
            sum_w += out_result->optimal_weights[i];
        }

        /* Normalize to sum = 1 */
        if (fabsf(sum_w) > EPSILON) {
            for (uint32_t i = 0; i < n; i++) {
                out_result->optimal_weights[i] /= sum_w;
            }
        }

        out_result->iterations = iter + 1;
    }

    /* Compute final metrics */
    out_result->expected_return = 0.0f;
    out_result->expected_volatility = 0.0f;
    float final_var = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        out_result->expected_return += out_result->optimal_weights[i] * expected_returns[i];
        for (uint32_t j = 0; j < n; j++) {
            final_var += out_result->optimal_weights[i]
                       * out_result->optimal_weights[j]
                       * covariance_matrix[i * n + j];
        }
    }
    out_result->expected_volatility = sqrtf(fmaxf(final_var, 0.0f));
    out_result->expected_sharpe = (out_result->expected_volatility > EPSILON)
        ? (out_result->expected_return - fin->config.risk_free_rate) / out_result->expected_volatility
        : 0.0f;
    out_result->objective_value = out_result->expected_return
                                - 0.5f * risk_aversion * final_var;

    return FIN_ERR_OK;
}

/**
 * @brief Internal: Risk-parity optimization - equalize risk contributions
 */
static int optimize_risk_parity(const financial_investment_eng_t* fin,
                                 const fin_portfolio_t* portfolio,
                                 const float* covariance_matrix,
                                 fin_optimization_result_t* out_result)
{
    uint32_t n = portfolio->asset_count;
    uint32_t max_iter = effective_iterations(fin);
    float tol = fin->config.convergence_tolerance;

    /* Initialize weights equally */
    for (uint32_t i = 0; i < n; i++) {
        out_result->optimal_weights[i] = 1.0f / (float)n;
    }

    out_result->converged = false;

    for (uint32_t iter = 0; iter < max_iter; iter++) {
        /* Compute portfolio variance */
        float port_var = 0.0f;
        for (uint32_t i = 0; i < n; i++) {
            for (uint32_t j = 0; j < n; j++) {
                port_var += out_result->optimal_weights[i]
                          * out_result->optimal_weights[j]
                          * covariance_matrix[i * n + j];
            }
        }
        float port_vol = sqrtf(fmaxf(port_var, EPSILON));

        /* Compute marginal risk contributions */
        float target_rc = port_vol / (float)n;
        float max_diff = 0.0f;

        for (uint32_t i = 0; i < n; i++) {
            /* Marginal contribution: w_i * (Cov * w)_i / port_vol */
            float cov_w_i = 0.0f;
            for (uint32_t j = 0; j < n; j++) {
                cov_w_i += covariance_matrix[i * n + j] * out_result->optimal_weights[j];
            }
            float rc_i = out_result->optimal_weights[i] * cov_w_i / port_vol;
            float diff = rc_i - target_rc;
            max_diff = fmaxf(max_diff, fabsf(diff));

            /* Adjust weight: decrease if risk contribution too high */
            float adj = 1.0f - 0.5f * diff / (target_rc + EPSILON);
            out_result->optimal_weights[i] *= nimcp_clampf(adj, 0.5f, 2.0f);
            out_result->optimal_weights[i] = nimcp_clampf(
                out_result->optimal_weights[i],
                fin->config.min_weight + EPSILON,
                fin->config.max_weight);
        }

        /* Normalize */
        float sum_w = 0.0f;
        for (uint32_t i = 0; i < n; i++) sum_w += out_result->optimal_weights[i];
        if (fabsf(sum_w) > EPSILON) {
            for (uint32_t i = 0; i < n; i++) out_result->optimal_weights[i] /= sum_w;
        }

        if (max_diff < tol) {
            out_result->converged = true;
            out_result->iterations = iter;
            break;
        }
        out_result->iterations = iter + 1;
    }

    return FIN_ERR_OK;
}

int financial_investment_optimize(financial_investment_eng_t* fin,
                                  const fin_portfolio_t* portfolio,
                                  const float* expected_returns,
                                  const float* covariance_matrix,
                                  fin_optimization_strategy_t strategy,
                                  fin_optimization_result_t* out_result)
{
    if (!fin || !portfolio || !expected_returns || !covariance_matrix || !out_result) {
        set_error("NULL parameter in optimize");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_investment_optimize: NULL parameter");
        return FIN_ERR_NULL;
    }
    int val_rc = fin_investment_validate_subsystems("optimize");
    if (val_rc != FIN_ERR_OK) return val_rc;
    int vrc = investment_validate_subsystems(fin, "optimize");
    if (vrc != FIN_ERR_OK) return vrc;
    financial_investment_heartbeat("fin_optimize", 0.0f);

    memset(out_result, 0, sizeof(fin_optimization_result_t));
    out_result->strategy = strategy;
    uint32_t n = portfolio->asset_count;
    int rc = FIN_ERR_OK;

    switch (strategy) {
    case FIN_OPT_STRATEGY_MEAN_VARIANCE: {
        /* Standard mean-variance with moderate risk aversion */
        rc = optimize_mean_variance(fin, portfolio, expected_returns,
                                    covariance_matrix, 2.0f, out_result);
        break;
    }
    case FIN_OPT_STRATEGY_MIN_VARIANCE: {
        /* Minimize variance only: high risk aversion */
        rc = optimize_mean_variance(fin, portfolio, expected_returns,
                                    covariance_matrix, 1000.0f, out_result);
        break;
    }
    case FIN_OPT_STRATEGY_MAX_SHARPE: {
        /* Maximize Sharpe: sweep risk aversions, pick best Sharpe */
        float best_sharpe = -FLT_MAX;
        fin_optimization_result_t trial;
        memset(&trial, 0, sizeof(trial));

        float lambdas[] = {0.1f, 0.5f, 1.0f, 2.0f, 5.0f, 10.0f, 20.0f, 50.0f};
        int num_lambdas = (int)(sizeof(lambdas) / sizeof(lambdas[0]));

        for (int l = 0; l < num_lambdas; l++) {
            memset(&trial, 0, sizeof(trial));
            optimize_mean_variance(fin, portfolio, expected_returns,
                                   covariance_matrix, lambdas[l], &trial);

            float sharpe = trial.expected_sharpe;
            if (sharpe > best_sharpe) {
                best_sharpe = sharpe;
                memcpy(out_result->optimal_weights, trial.optimal_weights,
                       n * sizeof(float));
                out_result->expected_return = trial.expected_return;
                out_result->expected_volatility = trial.expected_volatility;
                out_result->expected_sharpe = trial.expected_sharpe;
                out_result->iterations = trial.iterations;
                out_result->converged = trial.converged;
                out_result->objective_value = trial.objective_value;
            }
        }
        break;
    }
    case FIN_OPT_STRATEGY_RISK_PARITY: {
        rc = optimize_risk_parity(fin, portfolio, covariance_matrix, out_result);

        /* Compute return and vol for risk-parity weights */
        out_result->expected_return = 0.0f;
        float final_var = 0.0f;
        for (uint32_t i = 0; i < n; i++) {
            out_result->expected_return += out_result->optimal_weights[i] * expected_returns[i];
            for (uint32_t j = 0; j < n; j++) {
                final_var += out_result->optimal_weights[i]
                           * out_result->optimal_weights[j]
                           * covariance_matrix[i * n + j];
            }
        }
        out_result->expected_volatility = sqrtf(fmaxf(final_var, 0.0f));
        out_result->expected_sharpe = (out_result->expected_volatility > EPSILON)
            ? (out_result->expected_return - fin->config.risk_free_rate) / out_result->expected_volatility
            : 0.0f;
        break;
    }
    case FIN_OPT_STRATEGY_BLACK_LITTERMAN: {
        /* Simplified Black-Litterman: combine equilibrium with views */
        /* Use market-cap weights as prior, blend with equal-weight views */
        float total_cap = 0.0f;
        for (uint32_t i = 0; i < n; i++) {
            total_cap += portfolio->assets[i].market_cap;
        }

        if (total_cap > EPSILON) {
            for (uint32_t i = 0; i < n; i++) {
                float mkt_w = portfolio->assets[i].market_cap / total_cap;
                float eq_w = 1.0f / (float)n;
                /* Blend: tau / (tau + 1) * views + 1/(tau+1) * prior */
                float tau = 0.05f;
                out_result->optimal_weights[i] = (tau * eq_w + mkt_w) / (1.0f + tau);
            }
        } else {
            /* Fallback to equal weight */
            for (uint32_t i = 0; i < n; i++) {
                out_result->optimal_weights[i] = 1.0f / (float)n;
            }
        }

        /* Normalize */
        float sum_w = 0.0f;
        for (uint32_t i = 0; i < n; i++) sum_w += out_result->optimal_weights[i];
        if (fabsf(sum_w) > EPSILON) {
            for (uint32_t i = 0; i < n; i++) out_result->optimal_weights[i] /= sum_w;
        }

        out_result->converged = true;
        out_result->iterations = 1;

        /* Compute return and vol */
        out_result->expected_return = 0.0f;
        float bvar = 0.0f;
        for (uint32_t i = 0; i < n; i++) {
            out_result->expected_return += out_result->optimal_weights[i] * expected_returns[i];
            for (uint32_t j = 0; j < n; j++) {
                bvar += out_result->optimal_weights[i]
                      * out_result->optimal_weights[j]
                      * covariance_matrix[i * n + j];
            }
        }
        out_result->expected_volatility = sqrtf(fmaxf(bvar, 0.0f));
        out_result->expected_sharpe = (out_result->expected_volatility > EPSILON)
            ? (out_result->expected_return - fin->config.risk_free_rate) / out_result->expected_volatility
            : 0.0f;
        break;
    }
    case FIN_OPT_STRATEGY_EQUAL_WEIGHT: {
        for (uint32_t i = 0; i < n; i++) {
            out_result->optimal_weights[i] = 1.0f / (float)n;
        }
        out_result->converged = true;
        out_result->iterations = 0;

        /* Compute return and vol */
        out_result->expected_return = 0.0f;
        float eq_var = 0.0f;
        for (uint32_t i = 0; i < n; i++) {
            out_result->expected_return += out_result->optimal_weights[i] * expected_returns[i];
            for (uint32_t j = 0; j < n; j++) {
                eq_var += out_result->optimal_weights[i]
                        * out_result->optimal_weights[j]
                        * covariance_matrix[i * n + j];
            }
        }
        out_result->expected_volatility = sqrtf(fmaxf(eq_var, 0.0f));
        out_result->expected_sharpe = (out_result->expected_volatility > EPSILON)
            ? (out_result->expected_return - fin->config.risk_free_rate) / out_result->expected_volatility
            : 0.0f;
        break;
    }
    default:
        set_error("Unknown optimization strategy: %d", (int)strategy);
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_INVALID_PARAM, "financial_investment_optimize: Unknown optimization strategy %d", (int)strategy);
        return FIN_ERR_INVALID_PARAMS;
    }

    /* Fuzzy convergence detection if enabled */
    if (fin->config.enable_fuzzy_logic && out_result->iterations > 0) {
        /* Convergence degree based on iteration count relative to max */
        float progress = (float)out_result->iterations / (float)fin->config.max_iterations;
        fuzzy_mf_t conv_mf = fuzzy_mf_gaussian(0.0f, 0.3f);
        float conv_degree = fuzzy_mf_evaluate(&conv_mf, progress);
        /* Faster convergence = higher convergence degree */
        out_result->convergence_degree = nimcp_clamp01(conv_degree);
    } else if (out_result->converged) {
        out_result->convergence_degree = 1.0f;
    }

    /* Present antigen for optimization divergence (Phase 9) */
    if (out_result->iterations >= fin->config.max_iterations) {
        investment_present_antigen(fin, "optimization_divergence", 5);
    }

    /* Instance heartbeat at end (Phase 8: Change Set 2/3) */
    investment_heartbeat_instance((financial_investment_eng_t*)fin, "fin_optimize", 1.0f);

    fin->stats.optimizations++;
    return rc;
}

int financial_investment_efficient_frontier(financial_investment_eng_t* fin,
                                            const float* expected_returns,
                                            const float* covariance_matrix,
                                            uint32_t asset_count,
                                            float* out_frontier_returns,
                                            float* out_frontier_vols,
                                            uint32_t frontier_points)
{
    if (!fin || !expected_returns || !covariance_matrix
        || !out_frontier_returns || !out_frontier_vols) {
        set_error("NULL parameter in efficient_frontier");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_investment_efficient_frontier: NULL parameter");
        return FIN_ERR_NULL;
    }
    if (asset_count == 0 || frontier_points == 0) {
        set_error("Invalid frontier parameters");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_INVALID_PARAM, "financial_investment_efficient_frontier: Invalid frontier parameters");
        return FIN_ERR_INVALID_PARAMS;
    }
    financial_investment_heartbeat("fin_efficient_frontier", 0.0f);

    /* Find min and max possible returns */
    float min_ret = FLT_MAX, max_ret = -FLT_MAX;
    for (uint32_t i = 0; i < asset_count; i++) {
        if (expected_returns[i] < min_ret) min_ret = expected_returns[i];
        if (expected_returns[i] > max_ret) max_ret = expected_returns[i];
    }

    /* Create a synthetic portfolio for optimization */
    fin_portfolio_t temp_port;
    memset(&temp_port, 0, sizeof(temp_port));
    temp_port.asset_count = asset_count;
    for (uint32_t i = 0; i < asset_count; i++) {
        temp_port.assets[i].asset_id = i;
        temp_port.assets[i].expected_return = expected_returns[i];
        temp_port.assets[i].volatility = sqrtf(covariance_matrix[i * asset_count + i]);
        temp_port.weights[i] = 1.0f / (float)asset_count;
    }

    /* Sweep target returns across the frontier */
    float step = (max_ret - min_ret) / (float)(frontier_points > 1 ? frontier_points - 1 : 1);

    for (uint32_t p = 0; p < frontier_points; p++) {
        float target_ret = min_ret + step * (float)p;
        out_frontier_returns[p] = target_ret;

        /* Solve min-variance subject to target return constraint */
        /* Use mean-variance with high risk aversion and return constraint via penalty */
        fin_optimization_result_t opt;
        memset(&opt, 0, sizeof(opt));

        /* Approximate by sweeping risk aversion */
        float best_vol = FLT_MAX;
        float lambdas[] = {0.5f, 1.0f, 2.0f, 5.0f, 10.0f, 50.0f, 200.0f};
        int num_l = (int)(sizeof(lambdas) / sizeof(lambdas[0]));

        for (int l = 0; l < num_l; l++) {
            fin_optimization_result_t trial;
            memset(&trial, 0, sizeof(trial));
            optimize_mean_variance(fin, &temp_port, expected_returns,
                                   covariance_matrix, lambdas[l], &trial);

            if (fabsf(trial.expected_return - target_ret) < step * 0.5f
                && trial.expected_volatility < best_vol) {
                best_vol = trial.expected_volatility;
            }
        }

        out_frontier_vols[p] = (best_vol < FLT_MAX) ? best_vol : 0.0f;
    }

    fin->stats.optimizations++;
    return FIN_ERR_OK;
}

//=============================================================================
// Factor Analysis
//=============================================================================

int financial_investment_factor_analysis(financial_investment_eng_t* fin,
                                         const float* asset_returns,
                                         const float* factor_returns,
                                         uint32_t num_observations,
                                         uint32_t num_factors,
                                         fin_factor_result_t* out_result)
{
    if (!fin || !asset_returns || !factor_returns || !out_result) {
        set_error("NULL parameter in factor_analysis");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_investment_factor_analysis: NULL parameter");
        return FIN_ERR_NULL;
    }
    if (num_observations < 2 || num_factors == 0 || num_factors > FIN_MAX_FACTORS) {
        set_error("Invalid factor analysis parameters: obs=%u, factors=%u",
                  num_observations, num_factors);
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_INVALID_PARAM, "financial_investment_factor_analysis: Invalid factor analysis parameters");
        return FIN_ERR_INVALID_PARAMS;
    }
    financial_investment_heartbeat("fin_factor_analysis", 0.0f);

    memset(out_result, 0, sizeof(fin_factor_result_t));
    out_result->factor_count = num_factors;

    uint32_t T = num_observations;
    uint32_t K = num_factors;

    /* OLS regression: beta = (X^T X)^-1 X^T y
     * Simplified single-asset version.
     * X is T x K factor matrix, y is T x 1 asset returns.
     */

    /* Compute X^T X (K x K) */
    float* xtx = (float*)nimcp_calloc(K * K, sizeof(float));
    float* xty = (float*)nimcp_calloc(K, sizeof(float));
    if (!xtx || !xty) {
        nimcp_free(xtx);
        xtx = NULL;
        nimcp_free(xty);
        xty = NULL;
        set_error("Allocation failed in factor_analysis");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NO_MEMORY, "financial_investment_factor_analysis: Allocation failed");
        return FIN_ERR_ALLOC;
    }

    /* Compute factor means for de-meaning */
    float* factor_means = (float*)nimcp_calloc(K, sizeof(float));
    float asset_mean = 0.0f;
    if (!factor_means) {
        nimcp_free(xtx); nimcp_free(xty);
        xtx = NULL;
        set_error("Allocation failed in factor_analysis");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NO_MEMORY, "financial_investment_factor_analysis: Allocation failed for factor_means");
        return FIN_ERR_ALLOC;
    }

    for (uint32_t k = 0; k < K; k++) {
        float sum = 0.0f;
        for (uint32_t t = 0; t < T; t++) {
            sum += factor_returns[k * T + t];
        }
        factor_means[k] = sum / (float)T;
    }
    for (uint32_t t = 0; t < T; t++) {
        asset_mean += asset_returns[t];
    }
    asset_mean /= (float)T;

    /* X^T X */
    for (uint32_t i = 0; i < K; i++) {
        for (uint32_t j = 0; j < K; j++) {
            float sum = 0.0f;
            for (uint32_t t = 0; t < T; t++) {
                float xi = factor_returns[i * T + t] - factor_means[i];
                float xj = factor_returns[j * T + t] - factor_means[j];
                sum += xi * xj;
            }
            xtx[i * K + j] = sum;
        }
    }

    /* X^T y */
    for (uint32_t k = 0; k < K; k++) {
        float sum = 0.0f;
        for (uint32_t t = 0; t < T; t++) {
            float xk = factor_returns[k * T + t] - factor_means[k];
            float yt = asset_returns[t] - asset_mean;
            sum += xk * yt;
        }
        xty[k] = sum;
    }

    /* Solve (X^T X) beta = X^T y via simplified approach */
    /* For single factor (K=1), direct solve; for multi-factor, use Gauss elimination */
    if (K == 1) {
        if (fabsf(xtx[0]) > EPSILON) {
            out_result->factor_loadings[0] = xty[0] / xtx[0];
        }
    } else {
        /* Gaussian elimination with partial pivoting */
        float* aug = (float*)nimcp_calloc(K * (K + 1), sizeof(float));
        if (!aug) {
            nimcp_free(xtx); nimcp_free(xty); nimcp_free(factor_means);
            xtx = NULL;
            set_error("Allocation failed in factor_analysis");
            NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NO_MEMORY, "financial_investment_factor_analysis: Allocation failed for augmented matrix");
            return FIN_ERR_ALLOC;
        }

        /* Build augmented matrix [X^T X | X^T y] */
        for (uint32_t i = 0; i < K; i++) {
            for (uint32_t j = 0; j < K; j++) {
                aug[i * (K + 1) + j] = xtx[i * K + j];
            }
            aug[i * (K + 1) + K] = xty[i];
        }

        /* Forward elimination */
        for (uint32_t col = 0; col < K; col++) {
            /* Partial pivot */
            uint32_t max_row = col;
            float max_val = fabsf(aug[col * (K + 1) + col]);
            for (uint32_t row = col + 1; row < K; row++) {
                float val = fabsf(aug[row * (K + 1) + col]);
                if (val > max_val) {
                    max_val = val;
                    max_row = row;
                }
            }
            if (max_row != col) {
                for (uint32_t j = 0; j <= K; j++) {
                    float tmp = aug[col * (K + 1) + j];
                    aug[col * (K + 1) + j] = aug[max_row * (K + 1) + j];
                    aug[max_row * (K + 1) + j] = tmp;
                }
            }

            float pivot = aug[col * (K + 1) + col];
            if (fabsf(pivot) < EPSILON) continue;

            for (uint32_t row = col + 1; row < K; row++) {
                float factor = aug[row * (K + 1) + col] / pivot;
                for (uint32_t j = col; j <= K; j++) {
                    aug[row * (K + 1) + j] -= factor * aug[col * (K + 1) + j];
                }
            }
        }

        /* Back substitution */
        for (int i = (int)K - 1; i >= 0; i--) {
            float sum = aug[i * (K + 1) + K];
            for (uint32_t j = (uint32_t)i + 1; j < K; j++) {
                sum -= aug[i * (K + 1) + j] * out_result->factor_loadings[j];
            }
            float pivot = aug[i * (K + 1) + i];
            if (fabsf(pivot) > EPSILON) {
                out_result->factor_loadings[i] = sum / pivot;
            }
        }

        nimcp_free(aug);
        aug = NULL;
    }

    /* Compute residual return (alpha) */
    out_result->residual_return = asset_mean;
    for (uint32_t k = 0; k < K; k++) {
        out_result->residual_return -= out_result->factor_loadings[k] * factor_means[k];
    }

    /* Compute R-squared */
    float ss_total = 0.0f;
    float ss_residual = 0.0f;
    for (uint32_t t = 0; t < T; t++) {
        float predicted = out_result->residual_return;
        for (uint32_t k = 0; k < K; k++) {
            predicted += out_result->factor_loadings[k] * factor_returns[k * T + t];
        }
        float residual = asset_returns[t] - predicted;
        ss_residual += residual * residual;
        float diff = asset_returns[t] - asset_mean;
        ss_total += diff * diff;
    }
    out_result->r_squared = (ss_total > EPSILON) ? 1.0f - ss_residual / (fabsf(ss_total) > 1e-7f ? ss_total : 1e-7f) : 0.0f;
    out_result->r_squared = nimcp_clamp01(out_result->r_squared);

    /* Store factor returns (means) */
    for (uint32_t k = 0; k < K; k++) {
        out_result->factor_returns[k] = factor_means[k];
    }

    nimcp_free(xtx);
    xtx = NULL;
    nimcp_free(xty);
    xty = NULL;
    nimcp_free(factor_means);
    factor_means = NULL;
    fin->stats.factor_analyses++;
    return FIN_ERR_OK;
}

//=============================================================================
// Tax-Loss Harvesting
//=============================================================================

int financial_investment_tax_loss_harvest(financial_investment_eng_t* fin,
                                          fin_portfolio_t* portfolio,
                                          const float* cost_basis,
                                          float* out_tax_savings)
{
    if (!fin || !portfolio || !cost_basis || !out_tax_savings) {
        set_error("NULL parameter in tax_loss_harvest");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_investment_tax_loss_harvest: NULL parameter");
        return FIN_ERR_NULL;
    }
    int val_rc = fin_investment_validate_subsystems("tax_loss_harvest");
    if (val_rc != FIN_ERR_OK) return val_rc;
    financial_investment_heartbeat("fin_tax_loss_harvest", 0.0f);

    float total_savings = 0.0f;
    uint32_t n = portfolio->asset_count;

    for (uint32_t i = 0; i < n; i++) {
        float current_value = portfolio->assets[i].current_price;
        float basis = cost_basis[i];

        if (current_value < basis) {
            /* Position is at a loss */
            float loss = basis - current_value;

            /* Apply appropriate tax rate (simplified: use short-term rate
               if holding period < 1 year, else long-term) */
            /* Without explicit holding period info, use a blended rate */
            float short_savings = loss * fin->config.tax_rate_short;
            float long_savings = loss * fin->config.tax_rate_long;

            /* Blend: assume 60% short-term, 40% long-term */
            float savings = 0.6f * short_savings + 0.4f * long_savings;

            /* Account for transaction costs */
            float tx_cost = current_value * fin->config.transaction_cost_bps / 10000.0f;
            savings -= tx_cost;

            if (savings > 0.0f) {
                total_savings += savings;
            }
        }
    }

    *out_tax_savings = apply_modulation(fin, total_savings);
    fin->stats.portfolio_analyses++;
    return FIN_ERR_OK;
}

//=============================================================================
// Modulation & Statistics
//=============================================================================

int financial_investment_set_inflammation(financial_investment_eng_t* fin, float level) {
    if (!fin) {
        set_error("NULL engine in set_inflammation");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_investment_set_inflammation: fin is NULL");
        return FIN_ERR_NULL;
    }
    financial_investment_heartbeat("fin_set_inflammation", 0.0f);
    fin->inflammation = nimcp_clamp01(level);
    return FIN_ERR_OK;
}

int financial_investment_set_fatigue(financial_investment_eng_t* fin, float level) {
    if (!fin) {
        set_error("NULL engine in set_fatigue");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_investment_set_fatigue: fin is NULL");
        return FIN_ERR_NULL;
    }
    financial_investment_heartbeat("fin_set_fatigue", 0.0f);
    fin->fatigue = nimcp_clamp01(level);
    return FIN_ERR_OK;
}

int financial_investment_set_instance_immune(financial_investment_eng_t* fin, brain_immune_system_t* immune) {
    if (!fin) {
        set_error("NULL engine in set_instance_immune");
        return FIN_ERR_NULL;
    }
    fin->immune = immune;
    return FIN_ERR_OK;
}

int financial_investment_set_instance_bbb(financial_investment_eng_t* fin, bbb_system_t bbb) {
    if (!fin) {
        set_error("NULL engine in set_instance_bbb");
        return FIN_ERR_NULL;
    }
    fin->bbb = bbb;
    return FIN_ERR_OK;
}

int financial_investment_enable_immune_validation(financial_investment_eng_t* fin, bool enable) {
    if (!fin) {
        set_error("NULL engine in enable_immune_validation");
        return FIN_ERR_NULL;
    }
    fin->enable_immune_validation = enable;
    return FIN_ERR_OK;
}

int financial_investment_enable_bbb_validation(financial_investment_eng_t* fin, bool enable) {
    if (!fin) {
        set_error("NULL engine in enable_bbb_validation");
        return FIN_ERR_NULL;
    }
    fin->enable_bbb_validation = enable;
    return FIN_ERR_OK;
}

//=============================================================================
// KG Wiring Setter (Change Set 1)
//=============================================================================

int financial_investment_set_kg_wiring(financial_investment_eng_t* fin, kg_wiring_t* kg) {
    if (!fin) {
        set_error("set_kg_wiring: NULL eng");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_investment_set_kg_wiring: NULL eng");
        return FIN_ERR_NULL;
    }
    fin->kg_wiring = kg;
    return FIN_ERR_OK;
}

//=============================================================================
// Bio-Async Integration Setters (Change Set 4)
//=============================================================================

int financial_investment_set_bio_async(financial_investment_eng_t* eng, bio_async_context_t* ctx) {
    if (!eng) {
        set_error("set_bio_async: NULL eng");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_investment_set_bio_async: NULL eng");
        return FIN_ERR_NULL;
    }
    eng->bio_async = ctx;
    eng->async_enabled = (ctx != NULL);
    return FIN_ERR_OK;
}

int financial_investment_set_bio_router(financial_investment_eng_t* eng, bio_router_t* router) {
    if (!eng) {
        set_error("set_bio_router: NULL eng");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_investment_set_bio_router: NULL eng");
        return FIN_ERR_NULL;
    }
    eng->bio_router = router;
    return FIN_ERR_OK;
}

int financial_investment_get_stats(const financial_investment_eng_t* fin, fin_stats_t* stats) {
    if (!fin || !stats) {
        set_error("NULL parameter in get_stats");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_investment_get_stats: NULL parameter");
        return FIN_ERR_NULL;
    }
    financial_investment_heartbeat("fin_get_stats", 0.0f);
    *stats = fin->stats;
    return FIN_ERR_OK;
}

void financial_investment_reset_stats(financial_investment_eng_t* fin) {
    if (!fin) return;
    financial_investment_heartbeat("fin_reset_stats", 0.0f);
    memset(&fin->stats, 0, sizeof(fin_stats_t));
    fin->total_ops = 0;
    fin->total_processing_time_us = 0.0;
}

const char* financial_investment_get_last_error(void) {
    return fin_last_error;
}

void financial_investment_free_optimization(fin_optimization_result_t* result) {
    /* Currently optimization results are stack/struct based with no heap
       allocations, but this function exists for future extensibility. */
    if (result) {
        memset(result, 0, sizeof(fin_optimization_result_t));
    }
}

void financial_investment_free_factor(fin_factor_result_t* result) {
    /* Currently factor results are stack/struct based with no heap
       allocations, but this function exists for future extensibility. */
    if (result) {
        memset(result, 0, sizeof(fin_factor_result_t));
    }
}

//=============================================================================
// Health Agent and Logger Setters (Phase 8: Change Set 2/3)
//=============================================================================

int financial_investment_set_health_agent(financial_investment_eng_t* fin, void* agent) {
    if (!fin) {
        set_error("set_health_agent: NULL eng");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_investment_set_health_agent: NULL eng");
        return FIN_ERR_NULL;
    }
    fin->health_agent = (nimcp_health_agent_t*)agent;
    return FIN_ERR_OK;
}

int financial_investment_set_logger(financial_investment_eng_t* fin, void* logger) {
    if (!fin) {
        set_error("set_logger: NULL eng");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_investment_set_logger: NULL eng");
        return FIN_ERR_NULL;
    }
    fin->logger = logger;
    return FIN_ERR_OK;
}
