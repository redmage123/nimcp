//=============================================================================
// nimcp_financial_curiosity_bridge.c - Financial Curiosity-Driven Hypothesis Bridge
//=============================================================================
/**
 * @file nimcp_financial_curiosity_bridge.c
 * @brief Implementation of curiosity-driven exploration for financial markets
 *
 * @author NIMCP Development Team
 * @date 2026-01-29
 */

/* POSIX feature test macros for clock_gettime */
#define _POSIX_C_SOURCE 199309L
#define _GNU_SOURCE

#include "cognitive/parietal/nimcp_financial_curiosity_bridge.h"
#include "constants/nimcp_buffer_constants.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/error/nimcp_error_codes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdarg.h>
#include <time.h>
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_constants.h"
#include "constants/nimcp_math_constants.h"
#include "utils/math/nimcp_math_helpers.h"

/* Health agent: using pre-existing custom implementation */
static nimcp_health_agent_t* g_fin_curiosity_health_agent = NULL;

#include "utils/bridge/nimcp_bridge_boilerplate.h"
BRIDGE_DEFINE_MESH_REGISTRATION(fin_curiosity, MESH_ADAPTER_CATEGORY_COGNITIVE)


//=============================================================================
struct kg_wiring;
typedef struct kg_wiring kg_wiring_t;

/* KG message type defines */
#define KG_MSG_FIN_CURIOSITY_REQUEST    "FIN_CURIOSITY_REQUEST"
#define KG_MSG_FIN_CURIOSITY_RESPONSE   "FIN_CURIOSITY_RESPONSE"
#define KG_MSG_FIN_CURIOSITY_HYPOTHESIS "FIN_CURIOSITY_HYPOTHESIS"
#define KG_MSG_FIN_CURIOSITY_SELECTION  "FIN_CURIOSITY_SELECTION"

//=============================================================================
// BBB Integration
//=============================================================================
extern int bbb_validate_data(bbb_system_t bbb, const void* data, size_t size,
                              const char* context);
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

//=============================================================================
// Thread-local Error
//=============================================================================

static _Thread_local char fin_curiosity_last_error[NIMCP_ERROR_BUFFER_SIZE] = {0};

static void set_error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(fin_curiosity_last_error, sizeof(fin_curiosity_last_error), fmt, args);
    va_end(args);
}

//=============================================================================
// Heartbeat Helpers
//=============================================================================

static inline void fin_curiosity_heartbeat(const char* operation, float progress) {
    if (g_fin_curiosity_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_fin_curiosity_health_agent, operation, progress);
    }
}

static inline void fin_curiosity_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_fin_curiosity_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_fin_curiosity_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_fin_curiosity_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}

//=============================================================================
// Internal Structure
//=============================================================================

struct financial_curiosity_bridge {
    fin_curiosity_config_t config;
    fin_curiosity_bridge_stats_t stats;

    /* Modulation state */
    float inflammation;
    float fatigue;
    float curiosity_boost;

    /* Exploration tracking */
    uint64_t total_explorations;
    uint32_t* hypothesis_counts;    /**< Per-type exploration counts */
    float* hypothesis_success_rates; /**< Per-type success rates */

    /* Random state for Thompson sampling */
    uint32_t rng_state;

    /* Subsystem pointers */
    brain_immune_system_t* immune;
    bbb_system_t bbb;
    kg_wiring_t* kg_wiring;
    nimcp_health_agent_t* health_agent;
    void* logger;
    void* security;
    void* ethics;
    const void* lgss;
    void* coordinator;
    void* bio_router;

    /* Security validation flags */
    bool enable_bbb_validation;
    bool enable_immune_validation;

    /* Operational state */
    fin_curiosity_op_state_t operational_state;
};

/* Simple xorshift32 PRNG */
static uint32_t xorshift32(uint32_t* state) {
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

static float random_uniform(uint32_t* state) {
    return (float)xorshift32(state) / (float)UINT32_MAX;
}

/* Box-Muller transform for normal distribution */
static float random_normal(uint32_t* state, float mean, float std) {
    float u1 = random_uniform(state);
    float u2 = random_uniform(state);
    if (u1 < 1e-10f) u1 = 1e-10f;
    float z = sqrtf(-2.0f * logf(u1)) * cosf(NIMCP_TWO_PI_F * u2);
    return mean + std * z;
}

/* Beta distribution sampling for Thompson sampling */
static float random_beta(uint32_t* state, float alpha, float beta_param) {
    /* Approximation using gamma distribution ratio */
    float x = 0.0f, y = 0.0f;

    /* Sample from Gamma(alpha) using acceptance-rejection */
    for (int i = 0; i < 10; i++) {
        float u = random_uniform(state);
        x += -logf(u + 1e-10f);
    }
    x *= alpha / 10.0f;

    for (int i = 0; i < 10; i++) {
        float u = random_uniform(state);
        y += -logf(u + 1e-10f);
    }
    y *= beta_param / 10.0f;

    if (x + y < 1e-10f) return 0.5f;
    return x / (x + y);
}

static float compute_mean(const float* values, uint32_t count) {
    if (count == 0) return 0.0f;
    float sum = 0.0f;
    for (uint32_t i = 0; i < count; i++) {
        sum += values[i];
    }
    return sum / (float)count;
}

static float compute_variance(const float* values, uint32_t count, float mean) {
    if (count <= 1) return 0.0f;
    float sum_sq = 0.0f;
    for (uint32_t i = 0; i < count; i++) {
        float diff = values[i] - mean;
        sum_sq += diff * diff;
    }
    return sum_sq / (float)(count - 1);
}

static uint64_t get_timestamp_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

//=============================================================================
// Instance-Level Validation Helper
//=============================================================================

static int fin_curiosity_validate_instance(financial_curiosity_bridge_t* bridge,
                                            const char* operation) {
    if (!bridge) return FIN_CURIOSITY_ERR_NULL;

    if (bridge->enable_bbb_validation && bridge->bbb) {
        int rc = bbb_validate_data(bridge->bbb, NULL, 0, operation);
        if (rc != 0) {
            set_error("BBB validation failed for %s", operation);
            bridge->stats.bbb_validations++;
            NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_BBB_VALIDATION,
                "fin_curiosity: BBB validation failed for %s", operation);
            return FIN_CURIOSITY_ERR_VALIDATION;
        }
        bridge->stats.bbb_validations++;
    }

    if (bridge->enable_immune_validation && bridge->immune) {
        int rc = brain_immune_validate_operation(bridge->immune, operation, 5);
        if (rc != 0) {
            set_error("Immune validation failed for %s", operation);
            bridge->stats.immune_checks++;
            NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_BBB_VALIDATION,
                "fin_curiosity: immune validation failed for %s", operation);
            return FIN_CURIOSITY_ERR_VALIDATION;
        }
        bridge->stats.immune_checks++;
    }

    return FIN_CURIOSITY_ERR_OK;
}

//=============================================================================
// Antigen Presentation Helper
//=============================================================================

static void fin_curiosity_present_antigen(financial_curiosity_bridge_t* bridge,
                                           const char* anomaly, uint32_t severity) {
    if (bridge && bridge->immune) {
        uint8_t sig[64] = {0};
        snprintf((char*)sig, sizeof(sig), "fin_curiosity:%s", anomaly);
        uint32_t antigen_id = 0;
        brain_immune_present_antigen(bridge->immune, 0, sig, strlen((char*)sig),
                                      severity, 0, &antigen_id);
    }
}

//=============================================================================
// KG Publish Helper
//=============================================================================

static int fin_curiosity_kg_publish(financial_curiosity_bridge_t* bridge,
                                     const char* msg_type,
                                     const void* payload, size_t size) {
    if (bridge && bridge->kg_wiring) {
        /* kg_wiring_publish would be called here */
        (void)msg_type; (void)payload; (void)size;
        bridge->stats.kg_messages_sent++;
        return 0;
    }
    return 0;
}

//=============================================================================
// Lifecycle API
//=============================================================================

fin_curiosity_config_t financial_curiosity_bridge_default_config(void) {
    fin_curiosity_heartbeat("default_config", 0.0f);

    fin_curiosity_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));

    /* Hypothesis generation settings */
    cfg.max_hypotheses_per_cycle = 16;
    cfg.min_information_gain = 0.05f;
    cfg.max_exploration_cost = 0.9f;
    cfg.enable_all_hypothesis_types = true;
    cfg.enabled_types_mask = 0xFFFFFFFF;

    /* Selection strategy settings */
    cfg.strategy = FIN_SELECTION_UCB;
    cfg.exploration_coefficient = NIMCP_SQRT2_F;  /* sqrt(2) */
    cfg.epsilon = 0.1f;
    cfg.temperature = NIMCP_TEMPERATURE_DEFAULT;

    /* Modulation sensitivity */
    cfg.inflammation_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    cfg.fatigue_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    cfg.curiosity_boost = 1.0f;

    /* Security */
    cfg.enable_bbb_validation = false;
    cfg.enable_immune_validation = false;

    fin_curiosity_heartbeat("default_config", 1.0f);
    return cfg;
}

financial_curiosity_bridge_t* financial_curiosity_bridge_create(
    const fin_curiosity_config_t* config)
{
    fin_curiosity_heartbeat("create", 0.0f);

    financial_curiosity_bridge_t* bridge =
        (financial_curiosity_bridge_t*)nimcp_malloc(sizeof(financial_curiosity_bridge_t));
    if (!bridge) {
        set_error("Failed to allocate financial_curiosity_bridge_t");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NO_MEMORY,
            "Failed to allocate financial_curiosity_bridge_t");
        return NULL;
    }
    memset(bridge, 0, sizeof(*bridge));

    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = financial_curiosity_bridge_default_config();
    }

    /* Allocate tracking arrays */
    bridge->hypothesis_counts = (uint32_t*)nimcp_calloc(FIN_HYPOTHESIS_COUNT, sizeof(uint32_t));
    bridge->hypothesis_success_rates = (float*)nimcp_calloc(FIN_HYPOTHESIS_COUNT, sizeof(float));
    if (!bridge->hypothesis_counts || !bridge->hypothesis_success_rates) {
        set_error("Failed to allocate tracking arrays");
        if (bridge->hypothesis_counts) nimcp_free(bridge->hypothesis_counts);
        if (bridge->hypothesis_success_rates) nimcp_free(bridge->hypothesis_success_rates);
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "financial_curiosity_bridge_create: validation failed");
        return NULL;
    }

    /* Initialize success rates with prior */
    for (int i = 0; i < FIN_HYPOTHESIS_COUNT; i++) {
        bridge->hypothesis_success_rates[i] = 0.5f;
    }

    /* Initialize RNG state */
    bridge->rng_state = (uint32_t)time(NULL) ^ 0xDEADBEEF;

    /* Initialize state */
    bridge->operational_state = FIN_CURIOSITY_STATE_IDLE;
    bridge->inflammation = 0.0f;
    bridge->fatigue = 0.0f;
    bridge->curiosity_boost = bridge->config.curiosity_boost;

    /* Copy security flags */
    bridge->enable_bbb_validation = bridge->config.enable_bbb_validation;
    bridge->enable_immune_validation = bridge->config.enable_immune_validation;

    fin_curiosity_heartbeat("create", 1.0f);
    return bridge;
}

void financial_curiosity_bridge_destroy(financial_curiosity_bridge_t* bridge) {
    if (!bridge) return;
    fin_curiosity_heartbeat("destroy", 0.0f);

    if (bridge->hypothesis_counts) nimcp_free(bridge->hypothesis_counts);
    if (bridge->hypothesis_success_rates) nimcp_free(bridge->hypothesis_success_rates);
    nimcp_free(bridge);
    bridge = NULL;

    fin_curiosity_heartbeat("destroy", 1.0f);
}

fin_curiosity_op_state_t financial_curiosity_bridge_get_state(
    const financial_curiosity_bridge_t* bridge)
{
    if (!bridge) return FIN_CURIOSITY_STATE_UNINITIALIZED;
    return bridge->operational_state;
}

int financial_curiosity_bridge_reset(financial_curiosity_bridge_t* bridge) {
    if (!bridge) {
        set_error("NULL bridge in reset");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_curiosity_bridge_reset: bridge is NULL");
        return FIN_CURIOSITY_ERR_NULL;
    }

    fin_curiosity_heartbeat_instance(bridge->health_agent, "reset", 0.0f);

    /* Reset statistics */
    memset(&bridge->stats, 0, sizeof(bridge->stats));

    /* Reset tracking */
    bridge->total_explorations = 0;
    if (bridge->hypothesis_counts) {
        memset(bridge->hypothesis_counts, 0, FIN_HYPOTHESIS_COUNT * sizeof(uint32_t));
    }
    if (bridge->hypothesis_success_rates) {
        for (int i = 0; i < FIN_HYPOTHESIS_COUNT; i++) {
            bridge->hypothesis_success_rates[i] = 0.5f;
        }
    }

    /* Reset modulation */
    bridge->inflammation = 0.0f;
    bridge->fatigue = 0.0f;
    bridge->curiosity_boost = bridge->config.curiosity_boost;

    bridge->operational_state = FIN_CURIOSITY_STATE_IDLE;

    fin_curiosity_heartbeat_instance(bridge->health_agent, "reset", 1.0f);
    return FIN_CURIOSITY_ERR_OK;
}

//=============================================================================
// Subsystem Setters
//=============================================================================

int financial_curiosity_bridge_set_immune(financial_curiosity_bridge_t* bridge,
                                           void* immune) {
    if (!bridge) {
        set_error("NULL bridge");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_curiosity_bridge_set_immune: bridge is NULL");
        return FIN_CURIOSITY_ERR_NULL;
    }
    bridge->immune = (brain_immune_system_t*)immune;
    return FIN_CURIOSITY_ERR_OK;
}

int financial_curiosity_bridge_set_bbb(financial_curiosity_bridge_t* bridge,
                                        bbb_system_t bbb) {
    if (!bridge) {
        set_error("NULL bridge");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_curiosity_bridge_set_bbb: bridge is NULL");
        return FIN_CURIOSITY_ERR_NULL;
    }
    bridge->bbb = bbb;
    return FIN_CURIOSITY_ERR_OK;
}

int financial_curiosity_bridge_enable_bbb_validation(
    financial_curiosity_bridge_t* bridge, bool enable) {
    if (!bridge) {
        set_error("NULL bridge");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_curiosity_bridge_enable_bbb_validation: bridge is NULL");
        return FIN_CURIOSITY_ERR_NULL;
    }
    bridge->enable_bbb_validation = enable;
    return FIN_CURIOSITY_ERR_OK;
}

int financial_curiosity_bridge_enable_immune_validation(
    financial_curiosity_bridge_t* bridge, bool enable) {
    if (!bridge) {
        set_error("NULL bridge");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_curiosity_bridge_enable_immune_validation: bridge is NULL");
        return FIN_CURIOSITY_ERR_NULL;
    }
    bridge->enable_immune_validation = enable;
    return FIN_CURIOSITY_ERR_OK;
}

int financial_curiosity_bridge_set_kg_wiring(financial_curiosity_bridge_t* bridge,
                                              void* kg) {
    if (!bridge) {
        set_error("NULL bridge");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_curiosity_bridge_set_kg_wiring: bridge is NULL");
        return FIN_CURIOSITY_ERR_NULL;
    }
    bridge->kg_wiring = (kg_wiring_t*)kg;
    return FIN_CURIOSITY_ERR_OK;
}

int financial_curiosity_bridge_set_health_agent(financial_curiosity_bridge_t* bridge,
                                                 void* health_agent) {
    if (!bridge) {
        set_error("NULL bridge");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_curiosity_bridge_set_health_agent: bridge is NULL");
        return FIN_CURIOSITY_ERR_NULL;
    }
    bridge->health_agent = (nimcp_health_agent_t*)health_agent;
    return FIN_CURIOSITY_ERR_OK;
}

int financial_curiosity_bridge_set_logger(financial_curiosity_bridge_t* bridge,
                                           void* logger) {
    if (!bridge) {
        set_error("NULL bridge");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_curiosity_bridge_set_logger: bridge is NULL");
        return FIN_CURIOSITY_ERR_NULL;
    }
    bridge->logger = logger;
    return FIN_CURIOSITY_ERR_OK;
}

int financial_curiosity_bridge_set_security(financial_curiosity_bridge_t* bridge,
                                             void* security) {
    if (!bridge) {
        set_error("NULL bridge");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_curiosity_bridge_set_security: bridge is NULL");
        return FIN_CURIOSITY_ERR_NULL;
    }
    bridge->security = security;
    return FIN_CURIOSITY_ERR_OK;
}

int financial_curiosity_bridge_set_ethics(financial_curiosity_bridge_t* bridge,
                                           ethics_engine_t ethics) {
    if (!bridge) {
        set_error("NULL bridge");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_curiosity_bridge_set_ethics: bridge is NULL");
        return FIN_CURIOSITY_ERR_NULL;
    }
    bridge->ethics = (void*)ethics;
    return FIN_CURIOSITY_ERR_OK;
}

int financial_curiosity_bridge_set_lgss(financial_curiosity_bridge_t* bridge,
                                         const void* lgss) {
    if (!bridge) {
        set_error("NULL bridge");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_curiosity_bridge_set_lgss: bridge is NULL");
        return FIN_CURIOSITY_ERR_NULL;
    }
    bridge->lgss = lgss;
    return FIN_CURIOSITY_ERR_OK;
}

int financial_curiosity_bridge_set_coordinator(financial_curiosity_bridge_t* bridge,
                                                brain_cycle_coordinator_t* coordinator) {
    if (!bridge) {
        set_error("NULL bridge");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_curiosity_bridge_set_coordinator: bridge is NULL");
        return FIN_CURIOSITY_ERR_NULL;
    }
    bridge->coordinator = (void*)coordinator;
    return FIN_CURIOSITY_ERR_OK;
}

int financial_curiosity_bridge_set_bio_router(financial_curiosity_bridge_t* bridge,
                                               void* bio_router) {
    if (!bridge) {
        set_error("NULL bridge");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_curiosity_bridge_set_bio_router: bridge is NULL");
        return FIN_CURIOSITY_ERR_NULL;
    }
    bridge->bio_router = bio_router;
    return FIN_CURIOSITY_ERR_OK;
}

//=============================================================================
// Hypothesis Generation Helpers
//=============================================================================

static void generate_trend_hypothesis(financial_curiosity_bridge_t* bridge,
                                       const fin_market_state_t* market,
                                       uint32_t asset_idx,
                                       fin_extended_candidate_t* candidate) {
    memset(candidate, 0, sizeof(*candidate));
    candidate->type = FIN_HYPOTHESIS_TREND;
    candidate->target_asset_idx = asset_idx;

    /* Compute simple trend metrics */
    float price = (market->prices && asset_idx < market->num_assets) ?
                   market->prices[asset_idx] : 100.0f;

    snprintf(candidate->base.hypothesis, sizeof(candidate->base.hypothesis),
             "Asset %u shows trend continuation (price=%.2f)", asset_idx, price);

    /* Information gain: higher for unclear trends */
    candidate->base.information_gain = 0.3f + random_uniform(&bridge->rng_state) * 0.4f;

    /* Exploration cost: moderate */
    candidate->base.exploration_cost = 0.2f + random_uniform(&bridge->rng_state) * 0.2f;

    /* Expected value: based on historical success */
    float success_rate = bridge->hypothesis_success_rates[FIN_HYPOTHESIS_TREND];
    candidate->base.expected_value = success_rate * 0.5f;

    candidate->confidence = 0.5f + random_uniform(&bridge->rng_state) * 0.3f;
    candidate->novelty = 0.3f;
    candidate->exploration_count = bridge->hypothesis_counts[FIN_HYPOTHESIS_TREND];
    candidate->prior_success_rate = success_rate;
}

static void generate_mean_reversion_hypothesis(financial_curiosity_bridge_t* bridge,
                                                const fin_market_state_t* market,
                                                uint32_t asset_idx,
                                                fin_extended_candidate_t* candidate) {
    memset(candidate, 0, sizeof(*candidate));
    candidate->type = FIN_HYPOTHESIS_MEAN_REVERSION;
    candidate->target_asset_idx = asset_idx;

    float price = (market->prices && asset_idx < market->num_assets) ?
                   market->prices[asset_idx] : 100.0f;

    snprintf(candidate->base.hypothesis, sizeof(candidate->base.hypothesis),
             "Asset %u will mean-revert (price=%.2f)", asset_idx, price);

    candidate->base.information_gain = 0.25f + random_uniform(&bridge->rng_state) * 0.35f;
    candidate->base.exploration_cost = 0.15f + random_uniform(&bridge->rng_state) * 0.2f;

    float success_rate = bridge->hypothesis_success_rates[FIN_HYPOTHESIS_MEAN_REVERSION];
    candidate->base.expected_value = success_rate * 0.4f;

    candidate->confidence = 0.4f + random_uniform(&bridge->rng_state) * 0.3f;
    candidate->novelty = 0.4f;
    candidate->exploration_count = bridge->hypothesis_counts[FIN_HYPOTHESIS_MEAN_REVERSION];
    candidate->prior_success_rate = success_rate;
}

static void generate_correlation_hypothesis(financial_curiosity_bridge_t* bridge,
                                             const fin_market_state_t* market,
                                             uint32_t asset_i,
                                             uint32_t asset_j,
                                             fin_extended_candidate_t* candidate) {
    memset(candidate, 0, sizeof(*candidate));
    candidate->type = FIN_HYPOTHESIS_CORRELATION;
    candidate->target_asset_idx = asset_i;  /* Primary asset */

    snprintf(candidate->base.hypothesis, sizeof(candidate->base.hypothesis),
             "Correlation change between asset %u and %u", asset_i, asset_j);

    /* Correlation hypotheses have higher information gain potential */
    candidate->base.information_gain = 0.4f + random_uniform(&bridge->rng_state) * 0.4f;
    candidate->base.exploration_cost = 0.3f + random_uniform(&bridge->rng_state) * 0.25f;

    float success_rate = bridge->hypothesis_success_rates[FIN_HYPOTHESIS_CORRELATION];
    candidate->base.expected_value = success_rate * 0.6f;

    candidate->confidence = 0.35f + random_uniform(&bridge->rng_state) * 0.35f;
    candidate->novelty = 0.6f;
    candidate->exploration_count = bridge->hypothesis_counts[FIN_HYPOTHESIS_CORRELATION];
    candidate->prior_success_rate = success_rate;

    (void)market;  /* Used for context in full implementation */
}

static void generate_regime_change_hypothesis(financial_curiosity_bridge_t* bridge,
                                               const fin_market_state_t* market,
                                               fin_extended_candidate_t* candidate) {
    memset(candidate, 0, sizeof(*candidate));
    candidate->type = FIN_HYPOTHESIS_REGIME_CHANGE;
    candidate->target_asset_idx = (uint32_t)-1;  /* Market-wide */

    snprintf(candidate->base.hypothesis, sizeof(candidate->base.hypothesis),
             "Market regime change detected at ts=%lu", (unsigned long)market->timestamp_ms);

    /* Regime changes have highest information gain but also highest cost */
    candidate->base.information_gain = 0.6f + random_uniform(&bridge->rng_state) * 0.3f;
    candidate->base.exploration_cost = 0.5f + random_uniform(&bridge->rng_state) * 0.3f;

    float success_rate = bridge->hypothesis_success_rates[FIN_HYPOTHESIS_REGIME_CHANGE];
    candidate->base.expected_value = success_rate * 0.8f;

    candidate->confidence = 0.3f + random_uniform(&bridge->rng_state) * 0.3f;
    candidate->novelty = 0.8f;
    candidate->exploration_count = bridge->hypothesis_counts[FIN_HYPOTHESIS_REGIME_CHANGE];
    candidate->prior_success_rate = success_rate;
}

static void generate_volatility_hypothesis(financial_curiosity_bridge_t* bridge,
                                            const fin_market_state_t* market,
                                            uint32_t asset_idx,
                                            fin_extended_candidate_t* candidate) {
    memset(candidate, 0, sizeof(*candidate));
    candidate->type = FIN_HYPOTHESIS_VOLATILITY;
    candidate->target_asset_idx = asset_idx;

    snprintf(candidate->base.hypothesis, sizeof(candidate->base.hypothesis),
             "Volatility breakout for asset %u", asset_idx);

    candidate->base.information_gain = 0.35f + random_uniform(&bridge->rng_state) * 0.35f;
    candidate->base.exploration_cost = 0.25f + random_uniform(&bridge->rng_state) * 0.2f;

    float success_rate = bridge->hypothesis_success_rates[FIN_HYPOTHESIS_VOLATILITY];
    candidate->base.expected_value = success_rate * 0.5f;

    candidate->confidence = 0.4f + random_uniform(&bridge->rng_state) * 0.3f;
    candidate->novelty = 0.5f;
    candidate->exploration_count = bridge->hypothesis_counts[FIN_HYPOTHESIS_VOLATILITY];
    candidate->prior_success_rate = success_rate;

    (void)market;
}

static void generate_anomaly_hypothesis(financial_curiosity_bridge_t* bridge,
                                         const fin_market_state_t* market,
                                         uint32_t asset_idx,
                                         fin_extended_candidate_t* candidate) {
    memset(candidate, 0, sizeof(*candidate));
    candidate->type = FIN_HYPOTHESIS_ANOMALY;
    candidate->target_asset_idx = asset_idx;

    snprintf(candidate->base.hypothesis, sizeof(candidate->base.hypothesis),
             "Statistical anomaly detected for asset %u", asset_idx);

    /* Anomalies have very high information gain but uncertain value */
    candidate->base.information_gain = 0.5f + random_uniform(&bridge->rng_state) * 0.4f;
    candidate->base.exploration_cost = 0.35f + random_uniform(&bridge->rng_state) * 0.3f;

    float success_rate = bridge->hypothesis_success_rates[FIN_HYPOTHESIS_ANOMALY];
    candidate->base.expected_value = success_rate * 0.7f;

    candidate->confidence = 0.25f + random_uniform(&bridge->rng_state) * 0.35f;
    candidate->novelty = 0.9f;
    candidate->exploration_count = bridge->hypothesis_counts[FIN_HYPOTHESIS_ANOMALY];
    candidate->prior_success_rate = success_rate;

    (void)market;
}

//=============================================================================
// Core Curiosity API: generate_hypotheses()
//=============================================================================

int financial_curiosity_bridge_generate_hypotheses(
    financial_curiosity_bridge_t* bridge,
    const fin_market_state_t* market_state,
    fin_hypothesis_result_t* result)
{
    if (!bridge) {
        set_error("NULL bridge");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_curiosity_bridge_generate_hypotheses: bridge is NULL");
        return FIN_CURIOSITY_ERR_NULL;
    }
    if (!market_state) {
        set_error("NULL market_state");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_curiosity_bridge_generate_hypotheses: market_state is NULL");
        return FIN_CURIOSITY_ERR_NULL;
    }
    if (!result) {
        set_error("NULL result");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_curiosity_bridge_generate_hypotheses: result is NULL");
        return FIN_CURIOSITY_ERR_NULL;
    }

    /* Validate subsystems */
    int val_rc = fin_curiosity_validate_instance(bridge, "generate_hypotheses");
    if (val_rc != FIN_CURIOSITY_ERR_OK) return val_rc;

    fin_curiosity_heartbeat_instance(bridge->health_agent, "generate_hypotheses", 0.0f);
    bridge->stats.health_heartbeats++;

    uint64_t start_time = get_timestamp_us();
    bridge->operational_state = FIN_CURIOSITY_STATE_GENERATING;

    /* Health modulation affects hypothesis generation quality */
    float health_mod = 1.0f
        - bridge->inflammation * bridge->config.inflammation_sensitivity * 0.2f
        - bridge->fatigue * bridge->config.fatigue_sensitivity * 0.15f;
    if (health_mod < 0.3f) health_mod = 0.3f;

    /* Curiosity boost affects exploration tendency */
    float curiosity_mod = bridge->curiosity_boost * health_mod;

    /* Initialize result */
    result->num_candidates = 0;
    result->total_information_gain = 0.0f;
    result->avg_exploration_cost = 0.0f;

    if (!result->candidates || result->max_candidates == 0) {
        set_error("Result candidates not allocated");
        bridge->operational_state = FIN_CURIOSITY_STATE_ERROR;
        return FIN_CURIOSITY_ERR_INVALID_PARAM;
    }

    uint32_t max_hyps = bridge->config.max_hypotheses_per_cycle;
    if (max_hyps > result->max_candidates) {
        max_hyps = result->max_candidates;
    }

    fin_curiosity_heartbeat_instance(bridge->health_agent, "generate_hypotheses", 0.2f);

    /* Generate diverse hypotheses */
    uint32_t count = 0;
    uint32_t num_assets = market_state->num_assets > 0 ? market_state->num_assets : 1;

    /* 1. Trend hypotheses for first few assets */
    for (uint32_t i = 0; i < num_assets && count < max_hyps; i++) {
        if (random_uniform(&bridge->rng_state) < 0.5f * curiosity_mod) {
            generate_trend_hypothesis(bridge, market_state, i, &result->candidates[count]);
            count++;
        }
    }

    fin_curiosity_heartbeat_instance(bridge->health_agent, "generate_hypotheses", 0.4f);

    /* 2. Mean reversion hypotheses */
    for (uint32_t i = 0; i < num_assets && count < max_hyps; i++) {
        if (random_uniform(&bridge->rng_state) < 0.4f * curiosity_mod) {
            generate_mean_reversion_hypothesis(bridge, market_state, i, &result->candidates[count]);
            count++;
        }
    }

    /* 3. Correlation hypotheses (pairs) */
    for (uint32_t i = 0; i < num_assets && count < max_hyps; i++) {
        for (uint32_t j = i + 1; j < num_assets && count < max_hyps; j++) {
            if (random_uniform(&bridge->rng_state) < 0.2f * curiosity_mod) {
                generate_correlation_hypothesis(bridge, market_state, i, j, &result->candidates[count]);
                count++;
            }
        }
    }

    fin_curiosity_heartbeat_instance(bridge->health_agent, "generate_hypotheses", 0.6f);

    /* 4. Regime change hypothesis (market-wide) */
    if (count < max_hyps && random_uniform(&bridge->rng_state) < 0.3f * curiosity_mod) {
        generate_regime_change_hypothesis(bridge, market_state, &result->candidates[count]);
        count++;
    }

    /* 5. Volatility hypotheses */
    for (uint32_t i = 0; i < num_assets && count < max_hyps; i++) {
        if (random_uniform(&bridge->rng_state) < 0.35f * curiosity_mod) {
            generate_volatility_hypothesis(bridge, market_state, i, &result->candidates[count]);
            count++;
        }
    }

    /* 6. Anomaly hypotheses */
    for (uint32_t i = 0; i < num_assets && count < max_hyps; i++) {
        if (random_uniform(&bridge->rng_state) < 0.15f * curiosity_mod) {
            generate_anomaly_hypothesis(bridge, market_state, i, &result->candidates[count]);
            count++;
        }
    }

    fin_curiosity_heartbeat_instance(bridge->health_agent, "generate_hypotheses", 0.8f);

    /* Filter by minimum information gain */
    uint32_t filtered_count = 0;
    for (uint32_t i = 0; i < count; i++) {
        if (result->candidates[i].base.information_gain >= bridge->config.min_information_gain &&
            result->candidates[i].base.exploration_cost <= bridge->config.max_exploration_cost) {
            if (filtered_count != i) {
                result->candidates[filtered_count] = result->candidates[i];
            }
            result->total_information_gain += result->candidates[filtered_count].base.information_gain;
            result->avg_exploration_cost += result->candidates[filtered_count].base.exploration_cost;
            filtered_count++;
        }
    }

    result->num_candidates = filtered_count;
    if (filtered_count > 0) {
        result->avg_exploration_cost /= (float)filtered_count;
    }

    result->generation_time_us = get_timestamp_us() - start_time;

    bridge->stats.hypotheses_generated += filtered_count;
    bridge->operational_state = FIN_CURIOSITY_STATE_IDLE;

    /* Publish to KG */
    fin_curiosity_kg_publish(bridge, KG_MSG_FIN_CURIOSITY_HYPOTHESIS,
                              result, sizeof(*result));

    fin_curiosity_heartbeat_instance(bridge->health_agent, "generate_hypotheses", 1.0f);
    bridge->stats.health_heartbeats++;

    return FIN_CURIOSITY_ERR_OK;
}

//=============================================================================
// Core Curiosity API: select_exploration()
//=============================================================================

int financial_curiosity_bridge_select_exploration(
    financial_curiosity_bridge_t* bridge,
    const fin_extended_candidate_t* candidates,
    uint32_t num_candidates,
    fin_selection_result_t* selection)
{
    if (!bridge) {
        set_error("NULL bridge");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_curiosity_bridge_select_exploration: bridge is NULL");
        return FIN_CURIOSITY_ERR_NULL;
    }
    if (!candidates || num_candidates == 0) {
        set_error("No candidates to select from");
        return FIN_CURIOSITY_ERR_NO_CANDIDATES;
    }
    if (!selection) {
        set_error("NULL selection");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_curiosity_bridge_select_exploration: selection is NULL");
        return FIN_CURIOSITY_ERR_NULL;
    }

    /* Validate subsystems */
    int val_rc = fin_curiosity_validate_instance(bridge, "select_exploration");
    if (val_rc != FIN_CURIOSITY_ERR_OK) return val_rc;

    fin_curiosity_heartbeat_instance(bridge->health_agent, "select_exploration", 0.0f);
    bridge->stats.health_heartbeats++;

    bridge->operational_state = FIN_CURIOSITY_STATE_SELECTING;

    /* Health modulation affects selection strategy */
    float health_mod = 1.0f
        - bridge->inflammation * bridge->config.inflammation_sensitivity * 0.15f
        - bridge->fatigue * bridge->config.fatigue_sensitivity * 0.1f;
    if (health_mod < 0.3f) health_mod = 0.3f;

    uint32_t selected_idx = 0;
    float best_score = -1e30f;

    switch (bridge->config.strategy) {
        case FIN_SELECTION_UCB: {
            /* Upper Confidence Bound selection */
            float c = bridge->config.exploration_coefficient * health_mod;
            uint64_t N = bridge->total_explorations + 1;

            for (uint32_t i = 0; i < num_candidates; i++) {
                float exploitation = candidates[i].base.expected_value;
                uint32_t n_i = candidates[i].exploration_count + 1;
                float exploration = c * sqrtf(2.0f * logf((float)N) / (float)n_i);
                float score = exploitation + exploration;

                if (score > best_score) {
                    best_score = score;
                    selected_idx = i;
                    selection->exploitation_score = exploitation;
                    selection->exploration_bonus = exploration;
                }
            }
            snprintf(selection->rationale, sizeof(selection->rationale),
                     "UCB: exploitation=%.3f + exploration=%.3f",
                     selection->exploitation_score, selection->exploration_bonus);
            break;
        }

        case FIN_SELECTION_THOMPSON: {
            /* Thompson Sampling: sample from posterior distributions */
            for (uint32_t i = 0; i < num_candidates; i++) {
                float alpha = candidates[i].exploration_count *
                              candidates[i].prior_success_rate + 1.0f;
                float beta_param = candidates[i].exploration_count *
                                   (1.0f - candidates[i].prior_success_rate) + 1.0f;
                float sample = random_beta(&bridge->rng_state, alpha, beta_param);
                float score = sample * candidates[i].base.expected_value;

                if (score > best_score) {
                    best_score = score;
                    selected_idx = i;
                    selection->exploitation_score = candidates[i].base.expected_value;
                    selection->exploration_bonus = sample - candidates[i].prior_success_rate;
                }
            }
            snprintf(selection->rationale, sizeof(selection->rationale),
                     "Thompson: sampled score=%.3f", best_score);
            break;
        }

        case FIN_SELECTION_GREEDY: {
            /* Pure greedy: highest expected value */
            for (uint32_t i = 0; i < num_candidates; i++) {
                float score = candidates[i].base.expected_value;
                if (score > best_score) {
                    best_score = score;
                    selected_idx = i;
                }
            }
            selection->exploitation_score = best_score;
            selection->exploration_bonus = 0.0f;
            snprintf(selection->rationale, sizeof(selection->rationale),
                     "Greedy: max expected value=%.3f", best_score);
            break;
        }

        case FIN_SELECTION_EPSILON_GREEDY: {
            /* Epsilon-greedy: random with probability epsilon */
            float epsilon = bridge->config.epsilon / health_mod;  /* More random under stress */
            if (random_uniform(&bridge->rng_state) < epsilon) {
                /* Random selection */
                selected_idx = xorshift32(&bridge->rng_state) % num_candidates;
                selection->exploitation_score = candidates[selected_idx].base.expected_value;
                selection->exploration_bonus = epsilon;
                snprintf(selection->rationale, sizeof(selection->rationale),
                         "Epsilon-greedy: random explore (eps=%.2f)", epsilon);
            } else {
                /* Greedy selection */
                for (uint32_t i = 0; i < num_candidates; i++) {
                    float score = candidates[i].base.expected_value;
                    if (score > best_score) {
                        best_score = score;
                        selected_idx = i;
                    }
                }
                selection->exploitation_score = best_score;
                selection->exploration_bonus = 0.0f;
                snprintf(selection->rationale, sizeof(selection->rationale),
                         "Epsilon-greedy: exploit (value=%.3f)", best_score);
            }
            break;
        }

        case FIN_SELECTION_SOFTMAX: {
            /* Softmax/Boltzmann selection */
            float temperature = bridge->config.temperature * (2.0f - health_mod);
            float* probs = (float*)nimcp_calloc(num_candidates, sizeof(float));
            if (!probs) {
                bridge->operational_state = FIN_CURIOSITY_STATE_ERROR;
                return FIN_CURIOSITY_ERR_NO_MEMORY;
            }

            float max_val = candidates[0].base.expected_value;
            for (uint32_t i = 1; i < num_candidates; i++) {
                if (candidates[i].base.expected_value > max_val) {
                    max_val = candidates[i].base.expected_value;
                }
            }

            float sum_exp = 0.0f;
            for (uint32_t i = 0; i < num_candidates; i++) {
                probs[i] = expf((candidates[i].base.expected_value - max_val) / temperature);
                sum_exp += probs[i];
            }
            for (uint32_t i = 0; i < num_candidates; i++) {
                probs[i] /= sum_exp;
            }

            /* Sample from distribution */
            float r = random_uniform(&bridge->rng_state);
            float cumulative = 0.0f;
            for (uint32_t i = 0; i < num_candidates; i++) {
                cumulative += probs[i];
                if (r <= cumulative) {
                    selected_idx = i;
                    break;
                }
            }

            selection->exploitation_score = candidates[selected_idx].base.expected_value;
            selection->exploration_bonus = probs[selected_idx];
            snprintf(selection->rationale, sizeof(selection->rationale),
                     "Softmax: prob=%.3f, temp=%.2f", probs[selected_idx], temperature);

            nimcp_free(probs);
            probs = NULL;
            break;
        }

        default:
            /* Fallback to greedy */
            for (uint32_t i = 0; i < num_candidates; i++) {
                float score = candidates[i].base.expected_value;
                if (score > best_score) {
                    best_score = score;
                    selected_idx = i;
                }
            }
            selection->exploitation_score = best_score;
            selection->exploration_bonus = 0.0f;
            snprintf(selection->rationale, sizeof(selection->rationale),
                     "Fallback greedy: value=%.3f", best_score);
            break;
    }

    /* Populate selection result */
    selection->selected = candidates[selected_idx];
    selection->selected_index = selected_idx;
    selection->selection_score = selection->exploitation_score + selection->exploration_bonus;

    bridge->stats.explorations_selected++;
    bridge->total_explorations++;
    bridge->hypothesis_counts[selection->selected.type]++;

    bridge->operational_state = FIN_CURIOSITY_STATE_IDLE;

    /* Publish to KG */
    fin_curiosity_kg_publish(bridge, KG_MSG_FIN_CURIOSITY_SELECTION,
                              selection, sizeof(*selection));

    fin_curiosity_heartbeat_instance(bridge->health_agent, "select_exploration", 1.0f);
    bridge->stats.health_heartbeats++;

    return FIN_CURIOSITY_ERR_OK;
}

//=============================================================================
// Convenience API: explore()
//=============================================================================

int financial_curiosity_bridge_explore(
    financial_curiosity_bridge_t* bridge,
    const fin_market_state_t* market_state,
    fin_selection_result_t* selection)
{
    if (!bridge || !market_state || !selection) {
        set_error("NULL parameter");
        return FIN_CURIOSITY_ERR_NULL;
    }

    /* Allocate temporary result */
    fin_hypothesis_result_t* result = financial_curiosity_result_create(
        bridge->config.max_hypotheses_per_cycle);
    if (!result) {
        set_error("Failed to allocate result");
        return FIN_CURIOSITY_ERR_NO_MEMORY;
    }

    /* Generate hypotheses */
    int rc = financial_curiosity_bridge_generate_hypotheses(bridge, market_state, result);
    if (rc != FIN_CURIOSITY_ERR_OK) {
        financial_curiosity_result_destroy(result);
        return rc;
    }

    if (result->num_candidates == 0) {
        financial_curiosity_result_destroy(result);
        set_error("No hypotheses generated");
        return FIN_CURIOSITY_ERR_NO_CANDIDATES;
    }

    /* Select best hypothesis */
    rc = financial_curiosity_bridge_select_exploration(
        bridge, result->candidates, result->num_candidates, selection);

    financial_curiosity_result_destroy(result);
    return rc;
}

//=============================================================================
// Update Outcome
//=============================================================================

int financial_curiosity_bridge_update_outcome(
    financial_curiosity_bridge_t* bridge,
    const fin_extended_candidate_t* hypothesis,
    float outcome_value,
    bool was_confirmed)
{
    if (!bridge) {
        set_error("NULL bridge");
        return FIN_CURIOSITY_ERR_NULL;
    }
    if (!hypothesis) {
        set_error("NULL hypothesis");
        return FIN_CURIOSITY_ERR_NULL;
    }

    fin_curiosity_heartbeat_instance(bridge->health_agent, "update_outcome", 0.0f);
    bridge->stats.health_heartbeats++;

    /* Update success rate for this hypothesis type */
    fin_hypothesis_type_t type = hypothesis->type;
    if (type > FIN_HYPOTHESIS_NONE && type < FIN_HYPOTHESIS_COUNT) {
        uint32_t count = bridge->hypothesis_counts[type];
        float old_rate = bridge->hypothesis_success_rates[type];

        /* Exponential moving average update */
        float alpha = 1.0f / (float)(count + 1);
        float new_rate = old_rate * (1.0f - alpha) + (was_confirmed ? 1.0f : 0.0f) * alpha;
        bridge->hypothesis_success_rates[type] = new_rate;
    }

    (void)outcome_value;  /* Used for value estimation in full implementation */

    fin_curiosity_heartbeat_instance(bridge->health_agent, "update_outcome", 1.0f);
    bridge->stats.health_heartbeats++;

    return FIN_CURIOSITY_ERR_OK;
}

//=============================================================================
// Advanced Generation API
//=============================================================================

int financial_curiosity_bridge_generate_typed(
    financial_curiosity_bridge_t* bridge,
    const fin_market_state_t* market_state,
    fin_hypothesis_type_t type,
    fin_extended_candidate_t* candidates,
    uint32_t max_candidates,
    uint32_t* num_generated)
{
    if (!bridge || !market_state || !candidates || !num_generated) {
        set_error("NULL parameter");
        return FIN_CURIOSITY_ERR_NULL;
    }

    int val_rc = fin_curiosity_validate_instance(bridge, "generate_typed");
    if (val_rc != FIN_CURIOSITY_ERR_OK) return val_rc;

    fin_curiosity_heartbeat_instance(bridge->health_agent, "generate_typed", 0.0f);
    bridge->stats.health_heartbeats++;

    *num_generated = 0;
    uint32_t num_assets = market_state->num_assets > 0 ? market_state->num_assets : 1;

    switch (type) {
        case FIN_HYPOTHESIS_TREND:
            for (uint32_t i = 0; i < num_assets && *num_generated < max_candidates; i++) {
                generate_trend_hypothesis(bridge, market_state, i, &candidates[*num_generated]);
                (*num_generated)++;
            }
            break;

        case FIN_HYPOTHESIS_MEAN_REVERSION:
            for (uint32_t i = 0; i < num_assets && *num_generated < max_candidates; i++) {
                generate_mean_reversion_hypothesis(bridge, market_state, i, &candidates[*num_generated]);
                (*num_generated)++;
            }
            break;

        case FIN_HYPOTHESIS_CORRELATION:
            for (uint32_t i = 0; i < num_assets && *num_generated < max_candidates; i++) {
                for (uint32_t j = i + 1; j < num_assets && *num_generated < max_candidates; j++) {
                    generate_correlation_hypothesis(bridge, market_state, i, j, &candidates[*num_generated]);
                    (*num_generated)++;
                }
            }
            break;

        case FIN_HYPOTHESIS_REGIME_CHANGE:
            if (*num_generated < max_candidates) {
                generate_regime_change_hypothesis(bridge, market_state, &candidates[*num_generated]);
                (*num_generated)++;
            }
            break;

        case FIN_HYPOTHESIS_VOLATILITY:
            for (uint32_t i = 0; i < num_assets && *num_generated < max_candidates; i++) {
                generate_volatility_hypothesis(bridge, market_state, i, &candidates[*num_generated]);
                (*num_generated)++;
            }
            break;

        case FIN_HYPOTHESIS_ANOMALY:
            for (uint32_t i = 0; i < num_assets && *num_generated < max_candidates; i++) {
                generate_anomaly_hypothesis(bridge, market_state, i, &candidates[*num_generated]);
                (*num_generated)++;
            }
            break;

        default:
            set_error("Unsupported hypothesis type: %d", type);
            return FIN_CURIOSITY_ERR_INVALID_PARAM;
    }

    bridge->stats.hypotheses_generated += *num_generated;

    fin_curiosity_heartbeat_instance(bridge->health_agent, "generate_typed", 1.0f);
    bridge->stats.health_heartbeats++;

    return FIN_CURIOSITY_ERR_OK;
}

int financial_curiosity_bridge_generate_correlation_hypothesis(
    financial_curiosity_bridge_t* bridge,
    const fin_market_state_t* market_state,
    uint32_t asset_i,
    uint32_t asset_j,
    fin_extended_candidate_t* candidate)
{
    if (!bridge || !market_state || !candidate) {
        set_error("NULL parameter");
        return FIN_CURIOSITY_ERR_NULL;
    }

    int val_rc = fin_curiosity_validate_instance(bridge, "generate_correlation_hypothesis");
    if (val_rc != FIN_CURIOSITY_ERR_OK) return val_rc;

    generate_correlation_hypothesis(bridge, market_state, asset_i, asset_j, candidate);
    bridge->stats.hypotheses_generated++;

    return FIN_CURIOSITY_ERR_OK;
}

//=============================================================================
// Utility Functions
//=============================================================================

fin_market_state_t* financial_curiosity_market_state_create(uint32_t num_assets) {
    if (num_assets == 0 || num_assets > FIN_CURIOSITY_MAX_ASSETS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "financial_curiosity_market_state_create: num_assets is zero");
        return NULL;
    }

    fin_market_state_t* state = (fin_market_state_t*)nimcp_malloc(sizeof(fin_market_state_t));
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "financial_curiosity_market_state_create: state is NULL");
        return NULL;
    }

    memset(state, 0, sizeof(*state));
    state->num_assets = num_assets;

    state->prices = (float*)nimcp_calloc(num_assets, sizeof(float));
    state->volumes = (float*)nimcp_calloc(num_assets, sizeof(float));

    if (!state->prices || !state->volumes) {
        financial_curiosity_market_state_destroy(state);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "financial_curiosity_market_state_create: required parameter is NULL (state->prices, state->volumes)");
        return NULL;
    }

    return state;
}

void financial_curiosity_market_state_destroy(fin_market_state_t* state) {
    if (!state) return;
    if (state->prices) nimcp_free(state->prices);
    if (state->volumes) nimcp_free(state->volumes);
    nimcp_free(state);
    state = NULL;
}

fin_hypothesis_result_t* financial_curiosity_result_create(uint32_t max_candidates) {
    if (max_candidates == 0 || max_candidates > FIN_CURIOSITY_MAX_CANDIDATES) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "financial_curiosity_result_create: max_candidates is zero");
        return NULL;
    }

    fin_hypothesis_result_t* result = (fin_hypothesis_result_t*)nimcp_malloc(sizeof(fin_hypothesis_result_t));
    if (!result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "financial_curiosity_result_create: result is NULL");
        return NULL;
    }

    memset(result, 0, sizeof(*result));
    result->max_candidates = max_candidates;

    result->candidates = (fin_extended_candidate_t*)nimcp_calloc(max_candidates,
                                                            sizeof(fin_extended_candidate_t));
    if (!result->candidates) {
        nimcp_free(result);
        result = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "financial_curiosity_result_create: result->candidates is NULL");
        return NULL;
    }

    return result;
}

void financial_curiosity_result_destroy(fin_hypothesis_result_t* result) {
    if (!result) return;
    if (result->candidates) nimcp_free(result->candidates);
    nimcp_free(result);
    result = NULL;
}

int financial_curiosity_bridge_compute_information_gain(
    financial_curiosity_bridge_t* bridge,
    const fin_extended_candidate_t* hypothesis,
    const fin_market_state_t* market_state,
    float* information_gain)
{
    if (!bridge || !hypothesis || !information_gain) {
        set_error("NULL parameter");
        return FIN_CURIOSITY_ERR_NULL;
    }

    fin_curiosity_heartbeat_instance(bridge->health_agent, "compute_information_gain", 0.0f);

    /*
     * Information gain estimation:
     * IG(H) = H(Y) - H(Y | H tested)
     *
     * We use the hypothesis properties as a proxy:
     * - Novelty increases potential IG
     * - Prior success rate decreases IG (already known)
     * - Low confidence increases IG (more uncertainty to resolve)
     */

    float novelty_factor = hypothesis->novelty;
    float uncertainty_factor = 1.0f - hypothesis->confidence;
    float unknown_factor = 1.0f - hypothesis->prior_success_rate * hypothesis->prior_success_rate;

    float ig = 0.3f * novelty_factor + 0.4f * uncertainty_factor + 0.3f * unknown_factor;
    ig = nimcp_clampf(ig, 0.0f, 1.0f);

    /* Modulate by curiosity boost */
    ig *= bridge->curiosity_boost;
    ig = nimcp_clampf(ig, 0.0f, 1.0f);

    *information_gain = ig;

    (void)market_state;  /* Used for context-dependent IG in full implementation */

    fin_curiosity_heartbeat_instance(bridge->health_agent, "compute_information_gain", 1.0f);

    return FIN_CURIOSITY_ERR_OK;
}

//=============================================================================
// Modulation API
//=============================================================================

int financial_curiosity_bridge_set_inflammation(
    financial_curiosity_bridge_t* bridge, float level)
{
    if (!bridge) {
        set_error("NULL bridge");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_curiosity_bridge_set_inflammation: bridge is NULL");
        return FIN_CURIOSITY_ERR_NULL;
    }
    bridge->inflammation = nimcp_clampf(level, 0.0f, 1.0f);
    return FIN_CURIOSITY_ERR_OK;
}

int financial_curiosity_bridge_set_fatigue(
    financial_curiosity_bridge_t* bridge, float level)
{
    if (!bridge) {
        set_error("NULL bridge");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_curiosity_bridge_set_fatigue: bridge is NULL");
        return FIN_CURIOSITY_ERR_NULL;
    }
    bridge->fatigue = nimcp_clampf(level, 0.0f, 1.0f);
    return FIN_CURIOSITY_ERR_OK;
}

int financial_curiosity_bridge_set_curiosity_boost(
    financial_curiosity_bridge_t* bridge, float boost)
{
    if (!bridge) {
        set_error("NULL bridge");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_curiosity_bridge_set_curiosity_boost: bridge is NULL");
        return FIN_CURIOSITY_ERR_NULL;
    }
    bridge->curiosity_boost = nimcp_clampf(boost, 0.5f, 2.0f);
    return FIN_CURIOSITY_ERR_OK;
}

//=============================================================================
// Statistics
//=============================================================================

int financial_curiosity_bridge_get_stats(
    const financial_curiosity_bridge_t* bridge,
    fin_curiosity_bridge_stats_t* stats)
{
    if (!bridge) {
        set_error("NULL bridge");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_curiosity_bridge_get_stats: bridge is NULL");
        return FIN_CURIOSITY_ERR_NULL;
    }
    if (!stats) {
        set_error("NULL stats");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_curiosity_bridge_get_stats: stats is NULL");
        return FIN_CURIOSITY_ERR_NULL;
    }
    *stats = bridge->stats;
    return FIN_CURIOSITY_ERR_OK;
}

void financial_curiosity_bridge_reset_stats(financial_curiosity_bridge_t* bridge) {
    if (!bridge) return;
    memset(&bridge->stats, 0, sizeof(bridge->stats));
}

const char* financial_curiosity_bridge_get_last_error(void) {
    return fin_curiosity_last_error;
}

//=============================================================================
// String Conversion Utilities
//=============================================================================

const char* fin_curiosity_state_name(fin_curiosity_op_state_t state) {
    switch (state) {
        case FIN_CURIOSITY_STATE_UNINITIALIZED: return "UNINITIALIZED";
        case FIN_CURIOSITY_STATE_IDLE: return "IDLE";
        case FIN_CURIOSITY_STATE_GENERATING: return "GENERATING";
        case FIN_CURIOSITY_STATE_SELECTING: return "SELECTING";
        case FIN_CURIOSITY_STATE_EXPLORING: return "EXPLORING";
        case FIN_CURIOSITY_STATE_ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

const char* fin_curiosity_hypothesis_type_name(fin_hypothesis_type_t type) {
    switch (type) {
        case FIN_HYPOTHESIS_NONE: return "NONE";
        case FIN_HYPOTHESIS_TREND: return "TREND";
        case FIN_HYPOTHESIS_MEAN_REVERSION: return "MEAN_REVERSION";
        case FIN_HYPOTHESIS_MOMENTUM: return "MOMENTUM";
        case FIN_HYPOTHESIS_CORRELATION: return "CORRELATION";
        case FIN_HYPOTHESIS_REGIME_CHANGE: return "REGIME_CHANGE";
        case FIN_HYPOTHESIS_VOLATILITY: return "VOLATILITY";
        case FIN_HYPOTHESIS_SEASONALITY: return "SEASONALITY";
        case FIN_HYPOTHESIS_ANOMALY: return "ANOMALY";
        case FIN_HYPOTHESIS_LIQUIDITY: return "LIQUIDITY";
        case FIN_HYPOTHESIS_SENTIMENT: return "SENTIMENT";
        case FIN_HYPOTHESIS_CUSTOM: return "CUSTOM";
        default: return "UNKNOWN";
    }
}

const char* fin_curiosity_selection_strategy_name(fin_selection_strategy_t strategy) {
    switch (strategy) {
        case FIN_SELECTION_UCB: return "UCB";
        case FIN_SELECTION_THOMPSON: return "THOMPSON";
        case FIN_SELECTION_GREEDY: return "GREEDY";
        case FIN_SELECTION_EPSILON_GREEDY: return "EPSILON_GREEDY";
        case FIN_SELECTION_SOFTMAX: return "SOFTMAX";
        default: return "UNKNOWN";
    }
}

const char* financial_curiosity_bridge_version(void) {
    return "1.0.0";
}
