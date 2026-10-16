/**
 * @file nimcp_financial_market.c
 * @brief Financial Market Analysis Module implementation
 *
 * Implements time series analysis (GARCH), technical indicators (SMA, EMA,
 * RSI, MACD, Bollinger), market sentiment with fuzzy multi-membership,
 * regime detection, scenario analysis, stress testing, and Monte Carlo
 * simulation.
 *
 * @author NIMCP Development Team
 * @date 2025-12-31
 */

#include "cognitive/parietal/nimcp_financial_market.h"
#include "constants/nimcp_buffer_constants.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdarg.h>

//=============================================================================
// Health Agent Integration (Phase 8: System-Wide Health Integration)
//=============================================================================
#include <stddef.h> /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_learning_constants.h"
#include "constants/nimcp_threshold_constants.h"
#include "constants/nimcp_math_constants.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE_MESH_ONLY(fin_mkt, MESH_ADAPTER_CATEGORY_COGNITIVE)


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

struct kg_wiring;
typedef struct kg_wiring kg_wiring_t;

/* KG message type defines for market module */
#define KG_MSG_FIN_MKT_REQUEST    "FIN_MKT_REQUEST"
#define KG_MSG_FIN_MKT_RESPONSE   "FIN_MKT_RESPONSE"
#define KG_MSG_FIN_MKT_ERROR      "FIN_MKT_ERROR"
#define KG_MSG_FIN_MKT_UPDATE     "FIN_MKT_UPDATE"

static brain_immune_system_t* g_fin_market_immune = NULL;
static bbb_system_t g_fin_market_bbb = NULL;

void financial_market_set_immune(brain_immune_system_t* immune) {
    g_fin_market_immune = immune;
}

void financial_market_set_bbb(bbb_system_t bbb) {
    g_fin_market_bbb = bbb;
}

//=============================================================================
// Thread-Local Error Handling
//=============================================================================

static _Thread_local char fin_mkt_last_error[NIMCP_ERROR_BUFFER_SIZE] = {0};

static void set_error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(fin_mkt_last_error, sizeof(fin_mkt_last_error), fmt, args);
    va_end(args);
}

//=============================================================================
// Immune/BBB Validation Helper (Global)
//=============================================================================
static int fin_market_validate_subsystems(const char* operation) {
    if (g_fin_market_immune) {
        int rc = brain_immune_validate_operation(g_fin_market_immune, operation, 5);
        if (rc != 0) {
            set_error("financial_market: immune validation failed for %s", operation);
            return -1;
        }
    }
    if (g_fin_market_bbb) {
        int rc = bbb_validate_data(g_fin_market_bbb, NULL, 0, operation);
        if (rc != 0) {
            set_error("financial_market: BBB validation failed for %s", operation);
            return -1;
        }
    }
    return 0;
}

//=============================================================================
// Constants
//=============================================================================

#define FIN_MKT_EPSILON         NIMCP_EPSILON_ADAM
#define FIN_MKT_PI              NIMCP_PI_F
#define FIN_MKT_LCG_A           1664525u
#define FIN_MKT_LCG_C           1013904223u
#define FIN_MKT_TRADING_DAYS    252

//=============================================================================
// Opaque Structure
//=============================================================================

struct financial_market_eng {
    fin_market_config_t config;
    fin_market_stats_t  stats;
    float inflammation;
    float fatigue;
    void*               fuzzy_bridge;
    kg_wiring_t*        kg_wiring;
    /* Per-engine security integration */
    brain_immune_system_t* immune;
    bbb_system_t           bbb;
    bool enable_immune_validation;
    bool enable_bbb_validation;
    /* Health agent and logger (Phase 8: Change Set 2/3) */
    nimcp_health_agent_t*  health_agent;
    void*                  logger;
    /* Bio-async integration (Change Set 4) */
    bio_async_context_t*   bio_async;
    bio_router_t*          bio_router;
    bool async_enabled;
};

//=============================================================================
// Instance-Level Heartbeat Helper (Phase 8: Change Set 2/3)
//=============================================================================

static inline void market_heartbeat_instance(financial_market_eng_t* eng,
                                              const char* op, float progress) {
    if (eng && eng->health_agent) {
        /* nimcp_health_agent_heartbeat_ex would be called here */
        (void)op; (void)progress;
    }
}

//=============================================================================
// Logging Macros (Phase 8: Change Set 2/3)
//=============================================================================

#define FIN_MKT_LOG_DEBUG(eng, fmt, ...) /* placeholder */
#define FIN_MKT_LOG_INFO(eng, fmt, ...)  /* placeholder */
#define FIN_MKT_LOG_WARN(eng, fmt, ...)  /* placeholder */
#define FIN_MKT_LOG_ERROR(eng, fmt, ...) /* placeholder */

/**
 * @brief Publish a message through KG wiring
 * @param eng Market engine instance
 * @param msg_type Message type string
 * @param payload Payload data
 * @param size Payload size in bytes
 * @return 0 on success
 */
static int market_kg_publish(financial_market_eng_t* eng, const char* msg_type,
                              const void* payload, size_t size) {
    if (eng && eng->kg_wiring) {
        /* kg_wiring_publish would be called here */
        (void)msg_type; (void)payload; (void)size;
        return 0;
    }
    return 0;
}

//=============================================================================
// Per-Engine Validation Helper
//=============================================================================
static int market_validate_subsystems(financial_market_eng_t* eng, const char* operation) {
    if (!eng) return FIN_MKT_ERR_NULL;

    if (eng->enable_bbb_validation && eng->bbb) {
        int rc = bbb_validate_data(eng->bbb, NULL, 0, operation);
        if (rc != 0) {
            set_error("BBB validation failed for %s", operation);
            NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_OPERATION_FAILED,
                "financial_market: BBB validation failed for %s", operation);
            return FIN_MKT_ERR_VALIDATION;
        }
    }

    if (eng->enable_immune_validation && eng->immune) {
        int rc = brain_immune_validate_operation(eng->immune, operation, 5);
        if (rc != 0) {
            set_error("Immune validation failed for %s", operation);
            NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_OPERATION_FAILED,
                "financial_market: immune validation failed for %s", operation);
            return FIN_MKT_ERR_VALIDATION;
        }
    }

    return FIN_MKT_ERR_OK;
}

//=============================================================================
// Antigen Presentation Helper
//=============================================================================
static void market_present_antigen(financial_market_eng_t* eng,
                                    const char* anomaly, uint32_t severity) {
    if (eng && eng->immune) {
        uint8_t sig[64] = {0};
        snprintf((char*)sig, sizeof(sig), "fin_market:%s", anomaly);
        uint32_t antigen_id = 0;
        brain_immune_present_antigen(eng->immune, 0, sig, strlen((char*)sig),
                                      severity, 0, &antigen_id);
    }
}

//=============================================================================
// Internal Helper: Fuzzy Membership Functions
//=============================================================================

/** Z-shaped membership function: full membership at x<=a, zero at x>=b */
static float fuzzy_z_shaped(float a, float b, float x) {
    if (x <= a) return 1.0f;
    if (x >= b) return 0.0f;
    float mid = (a + b) * 0.5f;
    float range = b - a;
    if (range < FIN_MKT_EPSILON) return 0.5f;
    if (x <= mid) {
        float t = (x - a) / range;
        return 1.0f - 2.0f * t * t;
    } else {
        float t = (x - b) / range;
        return 2.0f * t * t;
    }
}

/** S-shaped membership function: zero at x<=a, full membership at x>=b */
static float fuzzy_s_shaped(float a, float b, float x) {
    if (x <= a) return 0.0f;
    if (x >= b) return 1.0f;
    float mid = (a + b) * 0.5f;
    float range = b - a;
    if (range < FIN_MKT_EPSILON) return 0.5f;
    if (x <= mid) {
        float t = (x - a) / range;
        return 2.0f * t * t;
    } else {
        float t = (x - b) / range;
        return 1.0f - 2.0f * t * t;
    }
}

/** Triangular membership function: zero at x<=a or x>=c, peak at b */
static float fuzzy_triangular(float a, float b, float c, float x) {
    if (x <= a || x >= c) return 0.0f;
    if (x <= b) {
        float denom = b - a;
        if (denom < FIN_MKT_EPSILON) return 1.0f;
        return (x - a) / denom;
    } else {
        float denom = c - b;
        if (denom < FIN_MKT_EPSILON) return 1.0f;
        return (c - x) / denom;
    }
}

//=============================================================================
// Internal Helper: Simple LCG Random Number Generator
//=============================================================================

static _Thread_local uint32_t lcg_state = 42u;

static uint32_t lcg_next(void) {
    lcg_state = FIN_MKT_LCG_A * lcg_state + FIN_MKT_LCG_C;
    return lcg_state;
}

/** Uniform random in [0, 1) */
static float rand_uniform(void) {
    return (float)(lcg_next() & 0x7FFFFF) / (float)0x800000;
}

/** Standard normal via Box-Muller transform */
static float rand_normal(void) {
    float u1 = rand_uniform();
    float u2 = rand_uniform();
    if (u1 < FIN_MKT_EPSILON) u1 = FIN_MKT_EPSILON;
    return sqrtf(-2.0f * logf(u1)) * cosf(2.0f * FIN_MKT_PI * u2);
}

//=============================================================================
// Internal Helper: Comparison for qsort
//=============================================================================

static int compare_floats_asc(const void* a, const void* b) {
    float fa = *(const float*)a;
    float fb = *(const float*)b;
    if (fa < fb) return -1;
    if (fa > fb) return 1;
    return 0;
}

//=============================================================================
// Lifecycle
//=============================================================================

fin_market_config_t financial_market_default_config(void) {
    fin_market_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.sentiment_weight        = 0.3f;
    cfg.technical_weight        = 0.4f;
    cfg.fundamental_weight      = 0.3f;
    cfg.default_ma_period       = 20;
    cfg.garch_max_iterations    = 500;
    cfg.garch_convergence_tol   = 1e-6f;
    cfg.monte_carlo_default_paths = 10000;
    cfg.enable_regime_detection = true;
    cfg.inflammation_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    cfg.fatigue_sensitivity      = NIMCP_SENSITIVITY_DEFAULT;
    cfg.enable_fuzzy_logic       = false;
    cfg.fuzzy_bridge             = NULL;
    return cfg;
}

financial_market_eng_t* financial_market_create(void) {
    fin_market_config_t cfg = financial_market_default_config();
    return financial_market_create_custom(&cfg);
}

financial_market_eng_t* financial_market_create_custom(const fin_market_config_t* config) {
    if (!config) {
        set_error("financial_market_create_custom: NULL config");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_market_create_custom: config is NULL");
        return NULL;
    }
    financial_market_eng_t* mkt = (financial_market_eng_t*)nimcp_calloc(1, sizeof(financial_market_eng_t));
    if (!mkt) {
        set_error("financial_market_create_custom: allocation failed");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NO_MEMORY, "financial_market_create_custom: allocation failed");
        return NULL;
    }
    mkt->config       = *config;
    mkt->inflammation = 0.0f;
    mkt->fatigue      = 0.0f;
    mkt->fuzzy_bridge = config->fuzzy_bridge;
    mkt->kg_wiring    = NULL;
    memset(&mkt->stats, 0, sizeof(mkt->stats));
    /* Health agent and logger (Phase 8: Change Set 2/3) */
    mkt->health_agent = NULL;
    mkt->logger = NULL;
    /* Bio-async integration (Change Set 4) */
    mkt->bio_async = NULL;
    mkt->bio_router = NULL;
    mkt->async_enabled = false;

    fin_mkt_heartbeat("financial_market_create", 1.0f);
    return mkt;
}

void financial_market_destroy(financial_market_eng_t* mkt) {
    if (!mkt) return;
    fin_mkt_heartbeat("financial_market_destroy", 1.0f);
    nimcp_free(mkt);
    mkt = NULL;
}

//=============================================================================
// GARCH(p,q) Fitting
//=============================================================================

int financial_market_garch_fit(financial_market_eng_t* mkt,
                                const float* returns, uint32_t length,
                                uint32_t p, uint32_t q,
                                fin_garch_result_t* out_result) {
    if (!mkt || !returns || !out_result) {
        set_error("garch_fit: NULL argument");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_market_garch_fit: NULL argument");
        return -1;
    }
    if (p == 0 || q == 0 || p > FIN_MKT_MAX_GARCH_ORDER || q > FIN_MKT_MAX_GARCH_ORDER) {
        set_error("garch_fit: invalid order p=%u q=%u", p, q);
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_INVALID_PARAM, "financial_market_garch_fit: invalid order p=%u q=%u", p, q);
        return -1;
    }
    if (length < p + q + 10) {
        set_error("garch_fit: insufficient data length=%u for p=%u q=%u", length, p, q);
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_INVALID_PARAM, "financial_market_garch_fit: insufficient data length=%u", length);
        return -1;
    }
    /* Per-engine validation */
    int val_rc = market_validate_subsystems(mkt, "garch_fit");
    if (val_rc != FIN_MKT_ERR_OK) return val_rc;
    /* Global validation (legacy) */
    val_rc = fin_market_validate_subsystems("garch_fit");
    if (val_rc != 0) return val_rc;

    fin_mkt_heartbeat("garch_fit_start", 0.0f);
    market_heartbeat_instance(mkt, "garch_fit", 0.0f);  /* Phase 8: Change Set 2/3 */

    /* Compute sample variance for initialization */
    float mean = 0.0f;
    for (uint32_t i = 0; i < length; i++) mean += returns[i];
    mean /= (float)length;

    float sample_var = 0.0f;
    for (uint32_t i = 0; i < length; i++) {
        float d = returns[i] - mean;
        sample_var += d * d;
    }
    sample_var /= (float)(length - 1);
    if (sample_var < FIN_MKT_EPSILON) sample_var = FIN_MKT_EPSILON;

    /* Initialize GARCH parameters */
    float omega = sample_var * 0.05f;
    float alpha[FIN_MKT_MAX_GARCH_ORDER];
    float beta[FIN_MKT_MAX_GARCH_ORDER];
    memset(alpha, 0, sizeof(alpha));
    memset(beta, 0, sizeof(beta));

    for (uint32_t i = 0; i < q; i++) alpha[i] = 0.05f / (float)q;
    for (uint32_t i = 0; i < p; i++) beta[i]  = 0.85f / (float)p;

    /* Conditional variance series */
    float* sigma2 = (float*)nimcp_calloc(length, sizeof(float));
    if (!sigma2) {
        set_error("garch_fit: allocation failed for variance series");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NO_MEMORY, "financial_market_garch_fit: allocation failed for variance series");
        return -1;
    }

    /* Compute log-likelihood for current parameters */
    float best_ll = -1e30f;
    float best_omega = omega;
    float best_alpha[FIN_MKT_MAX_GARCH_ORDER];
    float best_beta[FIN_MKT_MAX_GARCH_ORDER];
    memcpy(best_alpha, alpha, sizeof(alpha));
    memcpy(best_beta, beta, sizeof(beta));

    uint32_t max_iter = mkt->config.garch_max_iterations;
    float tol = mkt->config.garch_convergence_tol;

    /* Inflammation increases volatility estimates */
    float inflammation_factor = 1.0f + mkt->inflammation * mkt->config.inflammation_sensitivity * 0.2f;

    bool converged = false;
    float prev_ll = -1e30f;

    for (uint32_t iter = 0; iter < max_iter; iter++) {
        /* Compute conditional variance series */
        for (uint32_t t = 0; t < length; t++) {
            sigma2[t] = omega * inflammation_factor;
            for (uint32_t j = 0; j < q; j++) {
                if (t > j) {
                    float r = returns[t - j - 1] - mean;
                    sigma2[t] += alpha[j] * r * r;
                }
            }
            for (uint32_t j = 0; j < p; j++) {
                if (t > j) {
                    sigma2[t] += beta[j] * sigma2[t - j - 1];
                }
            }
            if (sigma2[t] < FIN_MKT_EPSILON) sigma2[t] = FIN_MKT_EPSILON;
        }

        /* Compute log-likelihood: sum of -0.5*(log(2*pi) + log(sigma2[t]) + r^2/sigma2[t]) */
        float ll = 0.0f;
        uint32_t start = (p > q) ? p : q;
        for (uint32_t t = start; t < length; t++) {
            float r = returns[t] - mean;
            ll += -0.5f * (logf(2.0f * FIN_MKT_PI) + logf(sigma2[t]) + (r * r) / sigma2[t]);
        }

        if (ll > best_ll) {
            best_ll = ll;
            best_omega = omega;
            memcpy(best_alpha, alpha, sizeof(alpha));
            memcpy(best_beta, beta, sizeof(beta));
        }

        /* Check convergence */
        if (iter > 0 && fabsf(ll - prev_ll) < tol) {
            converged = true;
            /* If fuzzy enabled, use convergence degree assessment */
            if (mkt->config.enable_fuzzy_logic) {
                float change = fabsf(ll - prev_ll);
                float conv_degree = fuzzy_s_shaped(tol * 0.01f, tol, tol - change);
                (void)conv_degree; /* used conceptually for fuzzy convergence */
            }
            break;
        }
        prev_ll = ll;

        /* Simplex-like perturbation: try small random changes and keep improvements */
        float step_size = 0.001f * (1.0f / (1.0f + (float)iter * 0.01f));

        /* Perturb omega */
        float trial = omega + (rand_uniform() - 0.5f) * 2.0f * step_size * sample_var;
        if (trial > FIN_MKT_EPSILON) omega = trial;

        /* Perturb alpha */
        for (uint32_t j = 0; j < q; j++) {
            trial = alpha[j] + (rand_uniform() - 0.5f) * 2.0f * step_size;
            if (trial > 0.0f && trial < 1.0f) alpha[j] = trial;
        }

        /* Perturb beta */
        for (uint32_t j = 0; j < p; j++) {
            trial = beta[j] + (rand_uniform() - 0.5f) * 2.0f * step_size;
            if (trial > 0.0f && trial < 1.0f) beta[j] = trial;
        }

        /* Ensure stationarity: sum(alpha) + sum(beta) < 1 */
        float sum_ab = 0.0f;
        for (uint32_t j = 0; j < q; j++) sum_ab += alpha[j];
        for (uint32_t j = 0; j < p; j++) sum_ab += beta[j];
        if (sum_ab >= 0.999f) {
            float scale = 0.99f / sum_ab;
            for (uint32_t j = 0; j < q; j++) alpha[j] *= scale;
            for (uint32_t j = 0; j < p; j++) beta[j] *= scale;
        }

        if (iter % 50 == 0) {
            fin_mkt_heartbeat("garch_fit_iterate", (float)iter / (float)max_iter);
        }
    }

    /* Fill output with best parameters */
    out_result->omega = best_omega;
    memcpy(out_result->alpha, best_alpha, sizeof(best_alpha));
    memcpy(out_result->beta, best_beta, sizeof(best_beta));
    out_result->p = p;
    out_result->q = q;
    out_result->log_likelihood = best_ll;
    out_result->converged = converged;

    /* Present antigen if convergence failed */
    if (!converged) {
        market_present_antigen(mkt, "garch_non_convergence", 5);
    }

    /* Compute current variance from best params */
    float cv = best_omega * inflammation_factor;
    for (uint32_t j = 0; j < q; j++) {
        if (length > j + 1) {
            float r = returns[length - j - 1] - mean;
            cv += best_alpha[j] * r * r;
        }
    }
    for (uint32_t j = 0; j < p; j++) {
        if (length > j + 1) {
            cv += best_beta[j] * sigma2[length - j - 1];
        }
    }
    out_result->current_variance = cv;

    nimcp_free(sigma2);
    sigma2 = NULL;
    mkt->stats.garch_fits++;
    fin_mkt_heartbeat("garch_fit_done", 1.0f);
    market_heartbeat_instance(mkt, "garch_fit", 1.0f);  /* Phase 8: Change Set 2/3 */
    return 0;
}

float financial_market_garch_forecast(const fin_garch_result_t* garch,
                                       uint32_t steps_ahead) {
    if (!garch || steps_ahead == 0) return 0.0f;

    /* Long-run variance: omega / (1 - sum(alpha) - sum(beta)) */
    float sum_ab = 0.0f;
    for (uint32_t j = 0; j < garch->q; j++) sum_ab += garch->alpha[j];
    for (uint32_t j = 0; j < garch->p; j++) sum_ab += garch->beta[j];

    float long_run_var = (sum_ab < 0.999f)
        ? garch->omega / (1.0f - sum_ab)
        : garch->current_variance;

    /* Forecast decays towards long-run variance */
    float forecast = long_run_var;
    if (steps_ahead == 1) {
        forecast = garch->current_variance;
    } else {
        /* h_t+k converges to long_run at rate (sum_ab)^k */
        float decay = powf(sum_ab, (float)steps_ahead);
        forecast = long_run_var + decay * (garch->current_variance - long_run_var);
    }

    return forecast;
}

//=============================================================================
// Technical Indicators: SMA
//=============================================================================

int financial_market_compute_sma(const float* prices, uint32_t length,
                                  uint32_t period, float* out_values) {
    if (!prices || !out_values) {
        set_error("compute_sma: NULL argument");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_market_compute_sma: NULL argument");
        return -1;
    }
    if (period == 0 || period > length) {
        set_error("compute_sma: invalid period=%u length=%u", period, length);
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_INVALID_PARAM, "financial_market_compute_sma: invalid period=%u length=%u", period, length);
        return -1;
    }

    /* Compute initial window sum */
    float sum = 0.0f;
    for (uint32_t i = 0; i < period; i++) sum += prices[i];
    out_values[0] = sum / (float)period;

    /* Slide window */
    uint32_t out_len = length - period + 1;
    for (uint32_t i = 1; i < out_len; i++) {
        sum += prices[i + period - 1] - prices[i - 1];
        out_values[i] = sum / (float)period;
    }

    return (int)out_len;
}

//=============================================================================
// Technical Indicators: EMA
//=============================================================================

int financial_market_compute_ema(const float* prices, uint32_t length,
                                  uint32_t period, float* out_values) {
    if (!prices || !out_values) {
        set_error("compute_ema: NULL argument");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_market_compute_ema: NULL argument");
        return -1;
    }
    if (period == 0 || period > length) {
        set_error("compute_ema: invalid period=%u length=%u", period, length);
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_INVALID_PARAM, "financial_market_compute_ema: invalid period=%u length=%u", period, length);
        return -1;
    }

    /* EMA multiplier */
    float mult = 2.0f / ((float)period + 1.0f);

    /* First EMA value = SMA of first 'period' prices */
    float sum = 0.0f;
    for (uint32_t i = 0; i < period; i++) sum += prices[i];
    out_values[0] = sum / (float)period;

    /* Subsequent values: EMA[i] = price[i+period-1]*mult + EMA[i-1]*(1-mult) */
    uint32_t out_len = length - period + 1;
    for (uint32_t i = 1; i < out_len; i++) {
        out_values[i] = prices[i + period - 1] * mult + out_values[i - 1] * (1.0f - mult);
    }

    return (int)out_len;
}

//=============================================================================
// Technical Indicators: RSI
//=============================================================================

float financial_market_compute_rsi(const float* prices, uint32_t length,
                                    uint32_t period) {
    if (!prices || period == 0 || length < period + 1) return 50.0f;

    /* Compute initial average gains and losses over 'period' */
    float avg_gain = 0.0f;
    float avg_loss = 0.0f;
    for (uint32_t i = 1; i <= period; i++) {
        float change = prices[i] - prices[i - 1];
        if (change > 0.0f) avg_gain += change;
        else               avg_loss += -change;
    }
    avg_gain /= (float)period;
    avg_loss /= (float)period;

    /* Smooth over remaining data using Wilder's smoothing */
    for (uint32_t i = period + 1; i < length; i++) {
        float change = prices[i] - prices[i - 1];
        float gain = (change > 0.0f) ? change : 0.0f;
        float loss = (change < 0.0f) ? -change : 0.0f;
        avg_gain = (avg_gain * (float)(period - 1) + gain) / (float)period;
        avg_loss = (avg_loss * (float)(period - 1) + loss) / (float)period;
    }

    if (avg_loss < FIN_MKT_EPSILON) return 100.0f;
    float rs = avg_gain / avg_loss;
    return 100.0f - 100.0f / (1.0f + rs);
}

//=============================================================================
// Technical Indicators: MACD
//=============================================================================

int financial_market_compute_macd(const float* prices, uint32_t length,
                                    uint32_t fast, uint32_t slow, uint32_t signal,
                                    float* out_macd, float* out_signal_line,
                                    float* out_histogram) {
    if (!prices || !out_macd || !out_signal_line || !out_histogram) {
        set_error("compute_macd: NULL argument");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_market_compute_macd: NULL argument");
        return -1;
    }
    if (fast == 0 || slow == 0 || signal == 0 || fast >= slow || slow > length) {
        set_error("compute_macd: invalid params fast=%u slow=%u signal=%u length=%u",
                  fast, slow, signal, length);
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_INVALID_PARAM, "financial_market_compute_macd: invalid params");
        return -1;
    }

    /* Compute fast and slow EMA */
    uint32_t ema_fast_len = length - fast + 1;
    uint32_t ema_slow_len = length - slow + 1;
    float* ema_fast = (float*)nimcp_calloc(ema_fast_len, sizeof(float));
    float* ema_slow = (float*)nimcp_calloc(ema_slow_len, sizeof(float));
    if (!ema_fast || !ema_slow) {
        nimcp_free(ema_fast);
        ema_fast = NULL;
        nimcp_free(ema_slow);
        ema_slow = NULL;
        set_error("compute_macd: allocation failed");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NO_MEMORY, "financial_market_compute_macd: allocation failed");
        return -1;
    }

    financial_market_compute_ema(prices, length, fast, ema_fast);
    financial_market_compute_ema(prices, length, slow, ema_slow);

    /* MACD line = EMA(fast) - EMA(slow), aligned to slow EMA indices */
    uint32_t offset = slow - fast; /* ema_fast is longer; align to ema_slow start */
    uint32_t macd_len = ema_slow_len;
    for (uint32_t i = 0; i < macd_len; i++) {
        out_macd[i] = ema_fast[i + offset] - ema_slow[i];
    }

    /* Signal line = EMA of MACD line */
    if (macd_len >= signal) {
        float sig_mult = 2.0f / ((float)signal + 1.0f);
        /* First signal = SMA of first 'signal' MACD values */
        float sig_sum = 0.0f;
        for (uint32_t i = 0; i < signal; i++) sig_sum += out_macd[i];
        out_signal_line[0] = sig_sum / (float)signal;

        uint32_t sig_len = macd_len - signal + 1;
        for (uint32_t i = 1; i < sig_len; i++) {
            out_signal_line[i] = out_macd[i + signal - 1] * sig_mult
                                 + out_signal_line[i - 1] * (1.0f - sig_mult);
        }

        /* Histogram = MACD - signal, aligned */
        for (uint32_t i = 0; i < sig_len; i++) {
            out_histogram[i] = out_macd[i + signal - 1] - out_signal_line[i];
        }
    }

    nimcp_free(ema_fast);
    ema_fast = NULL;
    nimcp_free(ema_slow);
    ema_slow = NULL;
    return (int)macd_len;
}

//=============================================================================
// Technical Indicators: Bollinger Bands
//=============================================================================

int financial_market_compute_bollinger(const float* prices, uint32_t length,
                                        uint32_t period, float num_std,
                                        float* out_upper, float* out_middle,
                                        float* out_lower) {
    if (!prices || !out_upper || !out_middle || !out_lower) {
        set_error("compute_bollinger: NULL argument");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_market_compute_bollinger: NULL argument");
        return -1;
    }
    if (period == 0 || period > length) {
        set_error("compute_bollinger: invalid period=%u length=%u", period, length);
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_INVALID_PARAM, "financial_market_compute_bollinger: invalid period=%u length=%u", period, length);
        return -1;
    }

    uint32_t out_len = length - period + 1;

    /* Compute SMA for middle band */
    financial_market_compute_sma(prices, length, period, out_middle);

    /* Compute standard deviation for each window */
    for (uint32_t i = 0; i < out_len; i++) {
        float mean = out_middle[i];
        float var = 0.0f;
        for (uint32_t j = 0; j < period; j++) {
            float d = prices[i + j] - mean;
            var += d * d;
        }
        var /= (float)period;
        float sd = sqrtf(var);
        out_upper[i] = mean + num_std * sd;
        out_lower[i] = mean - num_std * sd;
    }

    return (int)out_len;
}

//=============================================================================
// Technical Indicators: Dispatcher
//=============================================================================

int financial_market_compute_indicator(financial_market_eng_t* mkt,
                                        const fin_time_series_t* series,
                                        fin_indicator_type_t type, uint32_t period,
                                        fin_indicator_result_t* out_result) {
    if (!mkt || !series || !out_result) {
        set_error("compute_indicator: NULL argument");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_market_compute_indicator: NULL argument");
        return -1;
    }
    if (series->length == 0) {
        set_error("compute_indicator: empty series");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_INVALID_PARAM, "financial_market_compute_indicator: empty series");
        return -1;
    }

    fin_mkt_heartbeat("compute_indicator_start", 0.0f);
    memset(out_result, 0, sizeof(*out_result));
    out_result->type = type;
    out_result->period = period;

    int result = 0;

    switch (type) {
    case FIN_MKT_IND_SMA: {
        int len = financial_market_compute_sma(series->prices, series->length,
                                               period, out_result->values);
        if (len < 0) { result = -1; break; }
        out_result->length = (uint32_t)len;
        if (len > 0) out_result->signal = out_result->values[len - 1];
        break;
    }
    case FIN_MKT_IND_EMA: {
        int len = financial_market_compute_ema(series->prices, series->length,
                                               period, out_result->values);
        if (len < 0) { result = -1; break; }
        out_result->length = (uint32_t)len;
        if (len > 0) out_result->signal = out_result->values[len - 1];
        break;
    }
    case FIN_MKT_IND_RSI: {
        float rsi = financial_market_compute_rsi(series->prices, series->length, period);
        out_result->values[0] = rsi;
        out_result->length = 1;
        out_result->signal = rsi;
        break;
    }
    case FIN_MKT_IND_MACD: {
        /* Default MACD parameters: fast=12, slow=26, signal=9 */
        uint32_t fast_p = 12, slow_p = 26, sig_p = 9;
        float macd_buf[FIN_MKT_MAX_SERIES_LENGTH];
        float sig_buf[FIN_MKT_MAX_SERIES_LENGTH];
        float hist_buf[FIN_MKT_MAX_SERIES_LENGTH];
        int len = financial_market_compute_macd(series->prices, series->length,
                                                 fast_p, slow_p, sig_p,
                                                 macd_buf, sig_buf, hist_buf);
        if (len < 0) { result = -1; break; }
        /* Store MACD values in output */
        uint32_t copy_len = (uint32_t)len;
        if (copy_len > FIN_MKT_MAX_SERIES_LENGTH) copy_len = FIN_MKT_MAX_SERIES_LENGTH;
        memcpy(out_result->values, macd_buf, copy_len * sizeof(float));
        out_result->length = copy_len;
        if (copy_len > 0) {
            out_result->signal = sig_buf[0]; /* latest signal line */
            out_result->upper_band = hist_buf[0]; /* histogram reuse */
        }
        break;
    }
    case FIN_MKT_IND_BOLLINGER: {
        float upper_buf[FIN_MKT_MAX_SERIES_LENGTH];
        float middle_buf[FIN_MKT_MAX_SERIES_LENGTH];
        float lower_buf[FIN_MKT_MAX_SERIES_LENGTH];
        float num_std = 2.0f; /* standard Bollinger uses 2 std devs */
        int len = financial_market_compute_bollinger(series->prices, series->length,
                                                      period, num_std,
                                                      upper_buf, middle_buf, lower_buf);
        if (len < 0) { result = -1; break; }
        uint32_t copy_len = (uint32_t)len;
        if (copy_len > FIN_MKT_MAX_SERIES_LENGTH) copy_len = FIN_MKT_MAX_SERIES_LENGTH;
        memcpy(out_result->values, middle_buf, copy_len * sizeof(float));
        out_result->length = copy_len;
        if (copy_len > 0) {
            out_result->signal = middle_buf[copy_len - 1];
            out_result->upper_band = upper_buf[copy_len - 1];
            out_result->lower_band = lower_buf[copy_len - 1];
        }
        break;
    }
    case FIN_MKT_IND_ATR: {
        /* Average True Range: use high-low as proxy when OHLC not fully available */
        if (series->length < period + 1) { result = -1; break; }
        /* Compute daily ranges from price changes as proxy */
        float tr_sum = 0.0f;
        for (uint32_t i = 1; i <= period; i++) {
            float tr = fabsf(series->prices[i] - series->prices[i - 1]);
            tr_sum += tr;
        }
        float atr = tr_sum / (float)period;
        /* Smooth with Wilder method */
        uint32_t out_idx = 0;
        out_result->values[out_idx++] = atr;
        for (uint32_t i = period + 1; i < series->length; i++) {
            float tr = fabsf(series->prices[i] - series->prices[i - 1]);
            atr = (atr * (float)(period - 1) + tr) / (float)period;
            if (out_idx < FIN_MKT_MAX_SERIES_LENGTH) {
                out_result->values[out_idx++] = atr;
            }
        }
        out_result->length = out_idx;
        out_result->signal = atr;
        break;
    }
    case FIN_MKT_IND_MOMENTUM: {
        /* Momentum = price[i] - price[i - period] */
        if (series->length <= period) { result = -1; break; }
        uint32_t out_idx = 0;
        for (uint32_t i = period; i < series->length; i++) {
            if (out_idx < FIN_MKT_MAX_SERIES_LENGTH) {
                out_result->values[out_idx++] = series->prices[i] - series->prices[i - period];
            }
        }
        out_result->length = out_idx;
        if (out_idx > 0) out_result->signal = out_result->values[out_idx - 1];
        break;
    }
    case FIN_MKT_IND_VOLUME_WEIGHTED: {
        /* Volume Weighted Average Price (simplified) */
        if (series->length == 0) { result = -1; break; }
        float cum_pv = 0.0f;
        float cum_vol = 0.0f;
        uint32_t out_idx = 0;
        for (uint32_t i = 0; i < series->length; i++) {
            float vol = (series->volumes[i] > 0.0f) ? series->volumes[i] : 1.0f;
            cum_pv += series->prices[i] * vol;
            cum_vol += vol;
            if (out_idx < FIN_MKT_MAX_SERIES_LENGTH) {
                out_result->values[out_idx++] = cum_pv / cum_vol;
            }
        }
        out_result->length = out_idx;
        if (out_idx > 0) out_result->signal = out_result->values[out_idx - 1];
        break;
    }
    case FIN_MKT_IND_STOCHASTIC: {
        /* %K = (close - low_n) / (high_n - low_n) * 100 */
        if (series->length < period) { result = -1; break; }
        uint32_t out_idx = 0;
        for (uint32_t i = period - 1; i < series->length; i++) {
            float hi = series->prices[i];
            float lo = series->prices[i];
            for (uint32_t j = 0; j < period; j++) {
                float p = series->prices[i - j];
                if (p > hi) hi = p;
                if (p < lo) lo = p;
            }
            float range = hi - lo;
            float pct_k = (range > FIN_MKT_EPSILON)
                          ? ((series->prices[i] - lo) / range) * 100.0f
                          : 50.0f;
            if (out_idx < FIN_MKT_MAX_SERIES_LENGTH) {
                out_result->values[out_idx++] = pct_k;
            }
        }
        out_result->length = out_idx;
        if (out_idx > 0) out_result->signal = out_result->values[out_idx - 1];
        break;
    }
    default:
        set_error("compute_indicator: unknown type %d", (int)type);
        result = -1;
        break;
    }

    mkt->stats.indicator_calculations++;
    fin_mkt_heartbeat("compute_indicator_done", 1.0f);
    return result;
}

//=============================================================================
// Market Sentiment Analysis
//=============================================================================

int financial_market_analyze_sentiment(financial_market_eng_t* mkt,
                                        const fin_time_series_t* price_series,
                                        const fin_time_series_t* volume_series,
                                        fin_sentiment_t* out_sentiment) {
    if (!mkt || !price_series || !out_sentiment) {
        set_error("analyze_sentiment: NULL argument");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_market_analyze_sentiment: NULL argument");
        return -1;
    }
    if (price_series->length < 20) {
        set_error("analyze_sentiment: insufficient price data length=%u", price_series->length);
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_INVALID_PARAM, "financial_market_analyze_sentiment: insufficient price data length=%u", price_series->length);
        return -1;
    }
    /* Per-engine validation */
    int val_rc = market_validate_subsystems(mkt, "analyze_sentiment");
    if (val_rc != FIN_MKT_ERR_OK) return val_rc;

    fin_mkt_heartbeat("analyze_sentiment_start", 0.0f);
    memset(out_sentiment, 0, sizeof(*out_sentiment));

    uint32_t len = price_series->length;
    const float* prices = price_series->prices;

    /* RSI contribution (period=14) */
    float rsi = financial_market_compute_rsi(prices, len, 14);
    float rsi_contrib = rsi; /* Already 0-100 scale */

    /* Price momentum: compare current to SMA(50) if possible, else SMA(20) */
    uint32_t mom_period = (len >= 50) ? 50 : 20;
    float sma_buf[FIN_MKT_MAX_SERIES_LENGTH];
    int sma_len = financial_market_compute_sma(prices, len, mom_period, sma_buf);
    float momentum_contrib = 50.0f;
    if (sma_len > 0) {
        float latest_sma = sma_buf[sma_len - 1];
        if (latest_sma > FIN_MKT_EPSILON) {
            float mom_ratio = prices[len - 1] / latest_sma;
            momentum_contrib = nimcp_clampf((mom_ratio - 0.9f) / 0.2f * 100.0f, 0.0f, 100.0f);
        }
    }

    /* Volume ratio contribution */
    float volume_contrib = 50.0f;
    if (volume_series && volume_series->length >= 20) {
        float recent_vol = 0.0f;
        float older_vol = 0.0f;
        uint32_t vlen = volume_series->length;
        for (uint32_t i = vlen - 5; i < vlen; i++) recent_vol += volume_series->volumes[i];
        for (uint32_t i = vlen - 20; i < vlen - 5; i++) older_vol += volume_series->volumes[i];
        recent_vol /= 5.0f;
        older_vol /= 15.0f;
        if (older_vol > FIN_MKT_EPSILON) {
            float vratio = recent_vol / older_vol;
            volume_contrib = nimcp_clampf(vratio * 50.0f, 0.0f, 100.0f);
        }
    }

    /* Volatility contribution: higher volatility = more fear */
    float vol_contrib = 50.0f;
    if (len >= 21) {  /* Need len >= 21 to safely access prices[i-1] when i starts at len-20 */
        float sum_r2 = 0.0f;
        for (uint32_t i = len - 20; i < len; i++) {
            if (prices[i - 1] > FIN_MKT_EPSILON) {  /* Guard against division by zero */
                float r = (prices[i] - prices[i - 1]) / prices[i - 1];
                sum_r2 += r * r;
            }
        }
        float realized_vol = sqrtf(sum_r2 / 20.0f) * sqrtf((float)FIN_MKT_TRADING_DAYS);
        /* Higher vol -> lower score (more fear). Typical vol range 0.1 to 0.5 */
        vol_contrib = nimcp_clampf((0.5f - realized_vol) / 0.4f * 100.0f, 0.0f, 100.0f);
        out_sentiment->vix_equivalent = realized_vol * 100.0f;
    }

    /* Composite fear-greed index */
    float fear_greed = rsi_contrib * 0.3f + momentum_contrib * 0.3f
                       + volume_contrib * 0.2f + vol_contrib * 0.2f;

    /* Apply inflammation: increases fear */
    fear_greed -= mkt->inflammation * mkt->config.inflammation_sensitivity * 10.0f;
    fear_greed = nimcp_clampf(fear_greed, 0.0f, 100.0f);

    out_sentiment->fear_greed_index = fear_greed;
    out_sentiment->momentum_score = (momentum_contrib - 50.0f) / 50.0f;
    out_sentiment->volume_ratio = volume_contrib / 50.0f;
    out_sentiment->breadth = 0.5f; /* placeholder: would need breadth data */
    out_sentiment->put_call_ratio = 1.0f; /* placeholder */

    /* Classify sentiment level */
    if (fear_greed < 15.0f)      out_sentiment->level = FIN_MKT_SENTIMENT_EXTREME_FEAR;
    else if (fear_greed < 35.0f) out_sentiment->level = FIN_MKT_SENTIMENT_FEAR;
    else if (fear_greed < 65.0f) out_sentiment->level = FIN_MKT_SENTIMENT_NEUTRAL;
    else if (fear_greed < 85.0f) out_sentiment->level = FIN_MKT_SENTIMENT_GREED;
    else                         out_sentiment->level = FIN_MKT_SENTIMENT_EXTREME_GREED;

    /* Confidence based on agreement among components */
    float spread = fabsf(rsi_contrib - momentum_contrib)
                   + fabsf(rsi_contrib - vol_contrib)
                   + fabsf(momentum_contrib - vol_contrib);
    out_sentiment->confidence = nimcp_clampf(1.0f - spread / 300.0f, 0.2f, 1.0f);

    mkt->stats.sentiment_analyses++;
    fin_mkt_heartbeat("analyze_sentiment_done", 1.0f);
    return 0;
}

//=============================================================================
// Fuzzy Sentiment Analysis
//=============================================================================

int financial_market_analyze_sentiment_fuzzy(financial_market_eng_t* mkt,
                                              const fin_time_series_t* price_series,
                                              const fin_time_series_t* volume_series,
                                              fin_fuzzy_sentiment_t* out_sentiment) {
    if (!mkt || !price_series || !out_sentiment) {
        set_error("analyze_sentiment_fuzzy: NULL argument");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_market_analyze_sentiment_fuzzy: NULL argument");
        return -1;
    }

    fin_mkt_heartbeat("analyze_sentiment_fuzzy_start", 0.0f);

    /* Compute the crisp fear-greed index using the same logic as analyze_sentiment */
    fin_sentiment_t crisp;
    int rc = financial_market_analyze_sentiment(mkt, price_series, volume_series, &crisp);
    if (rc != 0) return rc;

    /* Decrement stat since analyze_sentiment already counted it */
    if (mkt->stats.sentiment_analyses > 0) mkt->stats.sentiment_analyses--;

    float fg = crisp.fear_greed_index; /* 0-100 scale */

    /* Apply 5 fuzzy membership functions */
    out_sentiment->extreme_fear_degree = fuzzy_z_shaped(10.0f, 25.0f, fg);
    out_sentiment->fear_degree         = fuzzy_triangular(15.0f, 30.0f, 45.0f, fg);
    out_sentiment->neutral_degree      = fuzzy_triangular(35.0f, 50.0f, 65.0f, fg);
    out_sentiment->greed_degree        = fuzzy_triangular(55.0f, 70.0f, 85.0f, fg);
    out_sentiment->extreme_greed_degree = fuzzy_s_shaped(75.0f, 90.0f, fg);

    /* Determine dominant */
    float degrees[FIN_MKT_SENTIMENT_COUNT] = {
        out_sentiment->extreme_fear_degree,
        out_sentiment->fear_degree,
        out_sentiment->neutral_degree,
        out_sentiment->greed_degree,
        out_sentiment->extreme_greed_degree
    };
    fin_sentiment_level_t best = FIN_MKT_SENTIMENT_NEUTRAL;
    float best_deg = -1.0f;
    for (int i = 0; i < FIN_MKT_SENTIMENT_COUNT; i++) {
        if (degrees[i] > best_deg) {
            best_deg = degrees[i];
            best = (fin_sentiment_level_t)i;
        }
    }
    out_sentiment->dominant = best;

    mkt->stats.sentiment_analyses++;
    fin_mkt_heartbeat("analyze_sentiment_fuzzy_done", 1.0f);
    return 0;
}

//=============================================================================
// Market Regime Detection
//=============================================================================

/** Track previous regime for detecting shifts */
static _Thread_local fin_market_condition_t prev_regime = FIN_MKT_SIDEWAYS;

fin_market_condition_t financial_market_detect_regime(financial_market_eng_t* mkt,
                                                       const fin_time_series_t* series) {
    if (!mkt || !series || series->length < 20) {
        return FIN_MKT_SIDEWAYS;
    }
    /* Per-engine validation */
    int val_rc = market_validate_subsystems(mkt, "detect_regime");
    if (val_rc != FIN_MKT_ERR_OK) return FIN_MKT_SIDEWAYS;
    /* Global validation (legacy) */
    val_rc = fin_market_validate_subsystems("detect_regime");
    if (val_rc != 0) return FIN_MKT_SIDEWAYS;

    fin_mkt_heartbeat("detect_regime_start", 0.0f);

    uint32_t len = series->length;
    const float* prices = series->prices;

    /* Trend: ratio of short SMA to long SMA */
    float short_sma_buf[FIN_MKT_MAX_SERIES_LENGTH];
    float long_sma_buf[FIN_MKT_MAX_SERIES_LENGTH];
    uint32_t short_period = 20;
    uint32_t long_period = (len >= 50) ? 50 : len / 2;
    if (long_period <= short_period) long_period = short_period + 1;
    if (long_period > len) long_period = len;

    int sl = financial_market_compute_sma(prices, len, short_period, short_sma_buf);
    int ll = financial_market_compute_sma(prices, len, long_period, long_sma_buf);

    float trend_ratio = 1.0f;
    if (sl > 0 && ll > 0) {
        float latest_short = short_sma_buf[sl - 1];
        float latest_long = long_sma_buf[ll - 1];
        if (latest_long > FIN_MKT_EPSILON) {
            trend_ratio = latest_short / latest_long;
        }
    }

    /* Volatility: annualized realized vol from last 20 days */
    float sum_r2 = 0.0f;
    uint32_t start = (len > 20) ? len - 20 : 1;
    for (uint32_t i = start; i < len; i++) {
        if (prices[i - 1] > FIN_MKT_EPSILON) {
            float r = (prices[i] - prices[i - 1]) / prices[i - 1];
            sum_r2 += r * r;
        }
    }
    float vol = sqrtf(sum_r2 / (float)(len - start)) * sqrtf((float)FIN_MKT_TRADING_DAYS);

    /* Drawdown from peak */
    float peak = prices[0];
    float max_dd = 0.0f;
    for (uint32_t i = 1; i < len; i++) {
        if (prices[i] > peak) peak = prices[i];
        float dd = (peak - prices[i]) / peak;
        if (dd > max_dd) max_dd = dd;
    }

    /* Inflammation increases perceived volatility */
    vol *= (1.0f + mkt->inflammation * mkt->config.inflammation_sensitivity * 0.15f);

    /* Classification thresholds */
    fin_market_condition_t condition;
    if (max_dd > 0.20f && vol > 0.35f) {
        condition = FIN_MKT_CRISIS;
    } else if (vol > 0.30f) {
        condition = FIN_MKT_HIGH_VOLATILITY;
    } else if (trend_ratio > 1.03f) {
        condition = FIN_MKT_BULL;
    } else if (trend_ratio < 0.97f) {
        condition = FIN_MKT_BEAR;
    } else if (max_dd > 0.10f && trend_ratio > 0.99f) {
        condition = FIN_MKT_RECOVERY;
    } else {
        condition = FIN_MKT_SIDEWAYS;
    }

    /* Present antigen if regime has shifted */
    if (condition != prev_regime) {
        market_present_antigen(mkt, "regime_shift", 4);
        prev_regime = condition;
    }

    fin_mkt_heartbeat("detect_regime_done", 1.0f);
    return condition;
}

//=============================================================================
// Fuzzy Regime Detection
//=============================================================================

int financial_market_detect_regime_fuzzy(financial_market_eng_t* mkt,
                                          const fin_time_series_t* series,
                                          fin_fuzzy_market_condition_t* out_condition) {
    if (!mkt || !series || !out_condition) {
        set_error("detect_regime_fuzzy: NULL argument");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_market_detect_regime_fuzzy: NULL argument");
        return -1;
    }
    if (series->length < 20) {
        set_error("detect_regime_fuzzy: insufficient data length=%u", series->length);
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_INVALID_PARAM, "financial_market_detect_regime_fuzzy: insufficient data length=%u", series->length);
        return -1;
    }

    fin_mkt_heartbeat("detect_regime_fuzzy_start", 0.0f);
    memset(out_condition, 0, sizeof(*out_condition));

    uint32_t len = series->length;
    const float* prices = series->prices;

    /* Trend strength: ratio short SMA / long SMA mapped to 0-100 scale */
    float short_sma_buf[FIN_MKT_MAX_SERIES_LENGTH];
    float long_sma_buf[FIN_MKT_MAX_SERIES_LENGTH];
    uint32_t short_period = 20;
    uint32_t long_period = (len >= 50) ? 50 : len / 2;
    if (long_period <= short_period) long_period = short_period + 1;
    if (long_period > len) long_period = len;

    int sl = financial_market_compute_sma(prices, len, short_period, short_sma_buf);
    int ll = financial_market_compute_sma(prices, len, long_period, long_sma_buf);

    float trend_strength = 50.0f;
    if (sl > 0 && ll > 0) {  /* Both SMAs must be valid */
        int sl_len = sl;
        int offset = sl_len - ll;
        if (offset >= 0) {
            float s = short_sma_buf[sl_len - 1];
            float l = long_sma_buf[ll - 1];
            if (l > FIN_MKT_EPSILON) {
                /* ratio of 1.0 = 50, range ~0.9-1.1 mapped to 0-100 */
                trend_strength = nimcp_clampf(((s / l) - 0.9f) / 0.2f * 100.0f, 0.0f, 100.0f);
            }
        }
    }

    /* Volatility: annualized */
    float sum_r2 = 0.0f;
    uint32_t start = (len > 20) ? len - 20 : 1;
    uint32_t count = (len > start) ? len - start : 1;  /* Guard against count == 0 */
    for (uint32_t i = start; i < len; i++) {
        if (prices[i - 1] > FIN_MKT_EPSILON) {
            float r = (prices[i] - prices[i - 1]) / prices[i - 1];
            sum_r2 += r * r;
        }
    }
    float vol = sqrtf(sum_r2 / (float)count) * sqrtf((float)FIN_MKT_TRADING_DAYS);
    vol *= (1.0f + mkt->inflammation * mkt->config.inflammation_sensitivity * 0.15f);
    /* Map vol: 0.1=low(10), 0.3=medium(50), 0.5+=high(100) */
    float vol_score = nimcp_clampf((vol - 0.1f) / 0.4f * 100.0f, 0.0f, 100.0f);

    /* Momentum: recent returns */
    float momentum_score = 50.0f;
    if (len >= 10 && prices[len - 10] > FIN_MKT_EPSILON) {
        float mom = (prices[len - 1] - prices[len - 10]) / prices[len - 10];
        momentum_score = nimcp_clampf((mom + 0.1f) / 0.2f * 100.0f, 0.0f, 100.0f);
    }

    /* Composite index combining trend, vol, and momentum */
    float composite = trend_strength * 0.4f + (100.0f - vol_score) * 0.3f + momentum_score * 0.3f;

    /* Apply 6 membership functions for market conditions */
    /* Bull: high composite (strong trend, low vol, positive momentum) */
    out_condition->bull_degree    = fuzzy_s_shaped(55.0f, 75.0f, composite);
    /* Bear: low composite */
    out_condition->bear_degree    = fuzzy_z_shaped(25.0f, 45.0f, composite);
    /* Sideways: middle range, low volatility */
    out_condition->sideways_degree = fuzzy_triangular(35.0f, 50.0f, 65.0f, composite)
                                     * fuzzy_z_shaped(20.0f, 50.0f, vol_score);
    /* High volatility: driven by vol_score */
    out_condition->high_vol_degree = fuzzy_s_shaped(40.0f, 70.0f, vol_score);
    /* Crisis: high vol + bear trend */
    out_condition->crisis_degree  = fuzzy_s_shaped(70.0f, 90.0f, vol_score)
                                    * fuzzy_z_shaped(20.0f, 35.0f, composite);
    /* Recovery: rising from bear territory */
    out_condition->recovery_degree = fuzzy_triangular(30.0f, 45.0f, 60.0f, composite)
                                     * fuzzy_s_shaped(40.0f, 65.0f, momentum_score);

    /* Determine dominant condition */
    float degrees[FIN_MKT_CONDITION_COUNT] = {
        out_condition->bull_degree,
        out_condition->bear_degree,
        out_condition->sideways_degree,
        out_condition->high_vol_degree,
        out_condition->crisis_degree,
        out_condition->recovery_degree
    };
    fin_market_condition_t best = FIN_MKT_SIDEWAYS;
    float best_deg = -1.0f;
    for (int i = 0; i < FIN_MKT_CONDITION_COUNT; i++) {
        if (degrees[i] > best_deg) {
            best_deg = degrees[i];
            best = (fin_market_condition_t)i;
        }
    }
    out_condition->dominant = best;

    fin_mkt_heartbeat("detect_regime_fuzzy_done", 1.0f);
    return 0;
}

//=============================================================================
// Scenario Analysis
//=============================================================================

int financial_market_run_scenario(financial_market_eng_t* mkt,
                                   const fin_portfolio_t* portfolio,
                                   const fin_scenario_t* scenario,
                                   fin_scenario_result_t* out_result) {
    if (!mkt || !portfolio || !scenario || !out_result) {
        set_error("run_scenario: NULL argument");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_market_run_scenario: NULL argument");
        return -1;
    }
    if (portfolio->asset_count == 0) {
        set_error("run_scenario: empty portfolio");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_INVALID_PARAM, "financial_market_run_scenario: empty portfolio");
        return -1;
    }
    int val_rc = fin_market_validate_subsystems("run_scenario");
    if (val_rc != 0) return val_rc;

    fin_mkt_heartbeat("run_scenario_start", 0.0f);
    memset(out_result, 0, sizeof(*out_result));

    float total_pnl = 0.0f;
    float worst_ret = 1e30f;
    float best_ret = -1e30f;
    float total_value = portfolio->total_value;
    if (total_value < FIN_MKT_EPSILON) total_value = 1.0f;

    for (uint32_t i = 0; i < portfolio->asset_count; i++) {
        const fin_asset_t* asset = &portfolio->assets[i];
        float weight = portfolio->weights[i];
        float asset_value = total_value * weight;
        float asset_return = 0.0f;

        /* Apply shocks based on asset type */
        switch (asset->type) {
        case FIN_ASSET_EQUITY:
        case FIN_ASSET_INDEX:
            asset_return = scenario->equity_shock;
            /* Volatility shock amplifies equity move */
            asset_return *= (1.0f + scenario->vol_shock * 0.5f);
            /* Beta-adjust */
            asset_return *= asset->beta;
            break;
        case FIN_ASSET_FIXED_INCOME:
            /* Rate shock: duration-based price change (simplified) */
            /* Approx: price change = -duration * rate_change */
            {
                float rate_change = scenario->rate_shock_bps / 10000.0f;
                float duration_approx = 5.0f; /* default duration */
                asset_return = -duration_approx * rate_change;
                /* Credit spread shock */
                asset_return -= scenario->credit_spread_shock_bps / 10000.0f * 3.0f;
            }
            break;
        case FIN_ASSET_COMMODITY:
            asset_return = scenario->commodity_shock;
            break;
        case FIN_ASSET_CURRENCY:
            asset_return = scenario->fx_shock;
            break;
        case FIN_ASSET_CRYPTO:
            /* Crypto: higher beta to equity, plus vol shock */
            asset_return = scenario->equity_shock * 2.0f + scenario->vol_shock * 0.3f;
            break;
        case FIN_ASSET_DERIVATIVE_OPTION:
        case FIN_ASSET_DERIVATIVE_FUTURE:
        case FIN_ASSET_DERIVATIVE_SWAP:
            /* Derivatives: combination of equity and vol shocks */
            asset_return = scenario->equity_shock * asset->beta
                           + scenario->vol_shock * 0.5f;
            break;
        case FIN_ASSET_REAL_ESTATE:
            asset_return = scenario->equity_shock * 0.6f
                           - scenario->rate_shock_bps / 10000.0f * 3.0f;
            break;
        default:
            asset_return = scenario->equity_shock;
            break;
        }

        /* Inflammation amplifies negative shocks */
        if (asset_return < 0.0f) {
            asset_return *= (1.0f + mkt->inflammation * mkt->config.inflammation_sensitivity * 0.1f);
        }

        float pnl = asset_value * asset_return;
        total_pnl += pnl;

        if (asset_return < worst_ret) worst_ret = asset_return;
        if (asset_return > best_ret)  best_ret = asset_return;
    }

    out_result->portfolio_pnl = total_pnl;
    out_result->portfolio_return = total_pnl / total_value;
    out_result->worst_asset_return = worst_ret;
    out_result->best_asset_return = best_ret;

    /* Max drawdown during scenario (simplified: single-period shock) */
    float drawdown = (out_result->portfolio_return < 0.0f)
                     ? -out_result->portfolio_return : 0.0f;
    out_result->max_drawdown = drawdown;

    /* Recovery time estimate: assume 8% annual recovery rate */
    float recovery_rate = 0.08f / (float)FIN_MKT_TRADING_DAYS;
    if (drawdown > FIN_MKT_EPSILON && recovery_rate > FIN_MKT_EPSILON) {
        out_result->recovery_time_days = drawdown / recovery_rate;
    } else {
        out_result->recovery_time_days = 0.0f;
    }

    /* VaR breach check (simplified: if loss exceeds 5% of portfolio) */
    out_result->var_breach = (out_result->portfolio_return < -0.05f)
                             ? -out_result->portfolio_return - 0.05f : 0.0f;

    /* Margin call if loss exceeds 25% */
    out_result->margin_call_triggered = (out_result->portfolio_return < -0.25f);

    mkt->stats.scenario_analyses++;
    fin_mkt_heartbeat("run_scenario_done", 1.0f);
    return 0;
}

//=============================================================================
// Stress Test
//=============================================================================

int financial_market_stress_test(financial_market_eng_t* mkt,
                                  const fin_portfolio_t* portfolio,
                                  const fin_scenario_t* scenarios,
                                  uint32_t num_scenarios,
                                  fin_scenario_result_t* out_results) {
    if (!mkt || !portfolio || !scenarios || !out_results) {
        set_error("stress_test: NULL argument");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_market_stress_test: NULL argument");
        return -1;
    }
    if (num_scenarios == 0) {
        set_error("stress_test: zero scenarios");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_INVALID_PARAM, "financial_market_stress_test: zero scenarios");
        return -1;
    }
    int val_rc = fin_market_validate_subsystems("stress_test");
    if (val_rc != 0) return val_rc;

    fin_mkt_heartbeat("stress_test_start", 0.0f);

    for (uint32_t i = 0; i < num_scenarios; i++) {
        int rc = financial_market_run_scenario(mkt, portfolio, &scenarios[i], &out_results[i]);
        if (rc != 0) {
            set_error("stress_test: scenario %u failed", i);
            NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_OPERATION_FAILED, "financial_market_stress_test: scenario %u failed", i);
            return -1;
        }

        if (i % 5 == 0) {
            fin_mkt_heartbeat("stress_test_progress",
                              (float)(i + 1) / (float)num_scenarios);
        }
    }

    fin_mkt_heartbeat("stress_test_done", 1.0f);
    return 0;
}

//=============================================================================
// Monte Carlo Simulation
//=============================================================================

int financial_market_monte_carlo(financial_market_eng_t* mkt,
                                   const fin_portfolio_t* portfolio,
                                   float drift, float volatility,
                                   float horizon_years, uint32_t num_paths,
                                   fin_monte_carlo_result_t* out_result) {
    if (!mkt || !portfolio || !out_result) {
        set_error("monte_carlo: NULL argument");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_market_monte_carlo: NULL argument");
        return -1;
    }
    if (num_paths == 0) {
        num_paths = mkt->config.monte_carlo_default_paths;
    }
    if (horizon_years <= 0.0f) {
        set_error("monte_carlo: invalid horizon=%.4f", (double)horizon_years);
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_INVALID_PARAM, "financial_market_monte_carlo: invalid horizon");
        return -1;
    }
    /* Per-engine validation */
    int val_rc = market_validate_subsystems(mkt, "monte_carlo");
    if (val_rc != FIN_MKT_ERR_OK) return val_rc;
    /* Global validation (legacy) */
    val_rc = fin_market_validate_subsystems("monte_carlo");
    if (val_rc != 0) return val_rc;

    fin_mkt_heartbeat("monte_carlo_start", 0.0f);
    market_heartbeat_instance(mkt, "monte_carlo", 0.0f);  /* Phase 8: Change Set 2/3 */
    memset(out_result, 0, sizeof(*out_result));

    /* Fatigue reduces computation: fewer paths */
    float fatigue_factor = 1.0f - mkt->fatigue * mkt->config.fatigue_sensitivity * 0.5f;
    if (fatigue_factor < 0.1f) fatigue_factor = 0.1f;
    uint32_t effective_paths = (uint32_t)((float)num_paths * fatigue_factor);
    if (effective_paths < 100) effective_paths = 100;

    /* Inflammation increases volatility estimate */
    float adj_vol = volatility * (1.0f + mkt->inflammation * mkt->config.inflammation_sensitivity * 0.2f);

    /* Track average volatility for spike detection */
    static _Thread_local float avg_volatility = 0.2f; /* typical market vol */

    /* GBM parameters */
    uint32_t steps = (uint32_t)(horizon_years * (float)FIN_MKT_TRADING_DAYS);
    if (steps == 0) steps = 1;
    float dt = horizon_years / (float)steps;
    float sqrt_dt = sqrtf(dt);
    float drift_adj = (drift - 0.5f * adj_vol * adj_vol) * dt;

    float initial_value = portfolio->total_value;
    if (initial_value < FIN_MKT_EPSILON) initial_value = 1.0f;

    /* Allocate terminal values array */
    float* terminal_values = (float*)nimcp_calloc(effective_paths, sizeof(float));
    float* path_returns = (float*)nimcp_calloc(effective_paths, sizeof(float));
    if (!terminal_values || !path_returns) {
        nimcp_free(terminal_values);
        terminal_values = NULL;
        nimcp_free(path_returns);
        set_error("monte_carlo: allocation failed");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NO_MEMORY, "financial_market_monte_carlo: allocation failed");
        return -1;
    }

    /* Seed LCG with a mix of portfolio value and paths */
    lcg_state = (uint32_t)(initial_value * 1000.0f) ^ (num_paths * 7919u);

    /* Simulate paths */
    float sum_returns = 0.0f;
    float sum_returns2 = 0.0f;
    uint32_t loss_count = 0;

    for (uint32_t p = 0; p < effective_paths; p++) {
        float S = initial_value;
        for (uint32_t t = 0; t < steps; t++) {
            float Z = rand_normal();
            S *= expf(drift_adj + adj_vol * sqrt_dt * Z);
        }
        terminal_values[p] = S;
        float ret = (S - initial_value) / initial_value;
        path_returns[p] = ret;
        sum_returns += ret;
        sum_returns2 += ret * ret;
        if (ret < 0.0f) loss_count++;

        if (p % 1000 == 0) {
            fin_mkt_heartbeat("monte_carlo_simulate",
                              (float)(p + 1) / (float)effective_paths);
        }
    }

    /* Compute statistics */
    float mean_ret = sum_returns / (float)effective_paths;
    float var_ret = (sum_returns2 / (float)effective_paths) - mean_ret * mean_ret;
    float std_ret = sqrtf(var_ret > 0.0f ? var_ret : 0.0f);

    out_result->paths_mean_return = mean_ret;
    out_result->paths_std_return = std_ret;
    out_result->probability_of_loss = (float)loss_count / (float)effective_paths;
    out_result->paths_completed = effective_paths;

    /* Sort returns for VaR/CVaR */
    qsort(path_returns, effective_paths, sizeof(float), compare_floats_asc);

    /* VaR 95%: 5th percentile of returns */
    uint32_t var_idx = (uint32_t)(0.05f * (float)effective_paths);
    if (var_idx >= effective_paths) var_idx = effective_paths - 1;
    out_result->paths_var_95 = -path_returns[var_idx]; /* positive = loss */

    /* CVaR 95%: average of returns below VaR */
    float cvar_sum = 0.0f;
    uint32_t cvar_count = var_idx + 1;
    for (uint32_t i = 0; i < cvar_count; i++) {
        cvar_sum += path_returns[i];
    }
    out_result->paths_cvar_95 = (cvar_count > 0)
        ? -(cvar_sum / (float)cvar_count) : out_result->paths_var_95;
    out_result->expected_shortfall = out_result->paths_cvar_95;

    /* Median terminal value */
    qsort(terminal_values, effective_paths, sizeof(float), compare_floats_asc);
    out_result->median_terminal_value = terminal_values[effective_paths / 2];

    /* Processing time placeholder (no high-res timer used) */
    out_result->processing_time_us = (float)effective_paths * 0.1f;

    /* Detect volatility spike: if current volatility > 3x average */
    if (adj_vol > 3.0f * avg_volatility) {
        market_present_antigen(mkt, "volatility_spike", 7);
    }
    /* Update rolling average volatility (exponential smoothing) */
    avg_volatility = 0.95f * avg_volatility + 0.05f * adj_vol;

    nimcp_free(terminal_values);
    terminal_values = NULL;
    nimcp_free(path_returns);

    mkt->stats.monte_carlo_simulations++;
    fin_mkt_heartbeat("monte_carlo_done", 1.0f);
    market_heartbeat_instance(mkt, "monte_carlo", 1.0f);  /* Phase 8: Change Set 2/3 */
    return 0;
}

//=============================================================================
// Modulation & Statistics
//=============================================================================

int financial_market_set_inflammation(financial_market_eng_t* mkt, float level) {
    if (!mkt) {
        set_error("set_inflammation: NULL engine");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_market_set_inflammation: mkt is NULL");
        return -1;
    }
    mkt->inflammation = nimcp_clampf(level, 0.0f, 1.0f);
    return 0;
}

int financial_market_set_fatigue(financial_market_eng_t* mkt, float level) {
    if (!mkt) {
        set_error("set_fatigue: NULL engine");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_market_set_fatigue: mkt is NULL");
        return -1;
    }
    mkt->fatigue = nimcp_clampf(level, 0.0f, 1.0f);
    return 0;
}

int financial_market_get_stats(const financial_market_eng_t* mkt, fin_market_stats_t* stats) {
    if (!mkt || !stats) {
        set_error("get_stats: NULL argument");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_market_get_stats: NULL argument");
        return -1;
    }
    *stats = mkt->stats;
    return 0;
}

void financial_market_reset_stats(financial_market_eng_t* mkt) {
    if (!mkt) return;
    memset(&mkt->stats, 0, sizeof(mkt->stats));
}

const char* financial_market_get_last_error(void) {
    return fin_mkt_last_error;
}

//=============================================================================
// Per-Engine Security Integration Setters
//=============================================================================

void financial_market_eng_set_immune(financial_market_eng_t* mkt, brain_immune_system_t* immune) {
    if (mkt) {
        mkt->immune = immune;
    }
}

void financial_market_eng_set_bbb(financial_market_eng_t* mkt, bbb_system_t bbb) {
    if (mkt) {
        mkt->bbb = bbb;
    }
}

void financial_market_eng_enable_bbb_validation(financial_market_eng_t* mkt, bool enable) {
    if (mkt) {
        mkt->enable_bbb_validation = enable;
    }
}

void financial_market_eng_enable_immune_validation(financial_market_eng_t* mkt, bool enable) {
    if (mkt) {
        mkt->enable_immune_validation = enable;
    }
}

//=============================================================================
// Bio-Async Integration Setters (Change Set 4)
//=============================================================================

int financial_market_set_bio_async(financial_market_eng_t* mkt, bio_async_context_t* ctx) {
    if (!mkt) {
        set_error("set_bio_async: NULL mkt");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_market_set_bio_async: NULL mkt");
        return FIN_MKT_ERR_NULL;
    }
    mkt->bio_async = ctx;
    mkt->async_enabled = (ctx != NULL);
    return FIN_MKT_ERR_OK;
}

int financial_market_set_bio_router(financial_market_eng_t* mkt, bio_router_t* router) {
    if (!mkt) {
        set_error("set_bio_router: NULL mkt");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_market_set_bio_router: NULL mkt");
        return FIN_MKT_ERR_NULL;
    }
    mkt->bio_router = router;
    return FIN_MKT_ERR_OK;
}

//=============================================================================
// KG Wiring Setter (Change Set 1)
//=============================================================================

int financial_market_set_kg_wiring(financial_market_eng_t* mkt, kg_wiring_t* kg) {
    if (!mkt) {
        set_error("set_kg_wiring: NULL mkt");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_market_set_kg_wiring: NULL mkt");
        return FIN_MKT_ERR_NULL;
    }
    mkt->kg_wiring = kg;
    return FIN_MKT_ERR_OK;
}

//=============================================================================
// Health Agent and Logger Setters (Phase 8: Change Set 2/3)
//=============================================================================

int financial_market_set_health_agent(financial_market_eng_t* mkt, void* agent) {
    if (!mkt) {
        set_error("set_health_agent: NULL mkt");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_market_set_health_agent: NULL mkt");
        return FIN_MKT_ERR_NULL;
    }
    mkt->health_agent = (nimcp_health_agent_t*)agent;
    return FIN_MKT_ERR_OK;
}

int financial_market_set_logger(financial_market_eng_t* mkt, void* logger) {
    if (!mkt) {
        set_error("set_logger: NULL mkt");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_market_set_logger: NULL mkt");
        return FIN_MKT_ERR_NULL;
    }
    mkt->logger = logger;
    return FIN_MKT_ERR_OK;
}
