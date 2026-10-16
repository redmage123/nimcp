/**
 * @file nimcp_financial_motivation_bridge.c
 * @brief Financial Motivation Bridge Implementation - Nucleus Accumbens Integration
 * @version 1.0.0
 * @date 2026-01-29
 *
 * WHAT: Bridge for modeling motivational signals in financial decision-making,
 *       separating wanting (incentive salience) from liking (hedonic value)
 *       following the nucleus accumbens dopaminergic model.
 *
 * WHY:  Financial decisions are driven by both rational analysis and
 *       motivational/emotional impulses. By modeling the nucleus accumbens
 *       reward circuitry, we can detect irrational FOMO and recommend
 *       rational overrides when emotional wanting exceeds computed value.
 *
 * HOW:  The implementation computes motivation signals from opportunity
 *       features, detects FOMO by comparing wanting to rational baselines,
 *       and recommends overrides based on configurable thresholds.
 *
 * @author NIMCP Development Team
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdarg.h>

#include "cognitive/parietal/nimcp_financial_motivation_bridge.h"
#include "constants/nimcp_buffer_constants.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/memory/nimcp_memory.h"

/* ============================================================================
 * Health Agent Integration (Phase 8: System-Wide Health Integration)
 * ============================================================================ */
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(fin_motivation)

/* Stub heartbeat for migration compatibility */
static inline void fin_motivation_heartbeat_global(const char* op, float progress) {
    (void)op; (void)progress;
}

BRIDGE_DEFINE_MESH_REGISTRATION(fin_motivation, MESH_ADAPTER_CATEGORY_COGNITIVE)


/* ============================================================================
 * Thread-Local Error Handling
 * ============================================================================ */

static _Thread_local char fin_motivation_last_error[NIMCP_ERROR_BUFFER_SIZE] = {0};

static void set_error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(fin_motivation_last_error, sizeof(fin_motivation_last_error), fmt, args);
    va_end(args);
}

const char* financial_motivation_bridge_get_last_error(void) {
    return fin_motivation_last_error;
}

/* ============================================================================
 * Internal Structure
 * ============================================================================ */

struct financial_motivation_bridge {
    uint32_t magic;
    fin_motivation_config_t config;
    fin_motivation_state_t state;
    fin_motivation_bridge_stats_t stats;

    /* Subsystem pointers */
    void* immune;
    void* bbb;
    void* health_agent;
    void* kg_wiring;
    void* logger;
    void* security;
    void* ethics;
    const void* lgss;
    void* coordinator;
    void* bio_router;

    /* Callbacks */
    fin_motivation_fomo_callback_t fomo_callback;
    void* fomo_callback_data;
    fin_motivation_override_callback_t override_callback;
    void* override_callback_data;

    /* Learning state */
    float baseline_wanting;         /**< Baseline wanting level */
    float recent_prediction_error;  /**< Most recent prediction error */
    float cumulative_prediction_error; /**< Running average PE */
    uint32_t outcome_count;         /**< Number of outcomes processed */
};

static inline float sigmoidf(float x) {
    return 1.0f / (1.0f + expf(-x));
}

/* ============================================================================
 * KG Wiring Message Types
 * ============================================================================ */

#define KG_MSG_FIN_MOTIVATION_EVAL      "FIN_MOTIVATION_EVAL"
#define KG_MSG_FIN_MOTIVATION_FOMO      "FIN_MOTIVATION_FOMO"
#define KG_MSG_FIN_MOTIVATION_OVERRIDE  "FIN_MOTIVATION_OVERRIDE"
#define KG_MSG_FIN_MOTIVATION_OUTCOME   "FIN_MOTIVATION_OUTCOME"

static int bridge_kg_publish(financial_motivation_bridge_t* bridge,
                              const char* msg_type,
                              const void* payload,
                              size_t size) {
    if (bridge && bridge->kg_wiring && bridge->config.enable_kg_messaging) {
        bridge->stats.kg_messages_sent++;
        /* kg_wiring_publish would be called here */
        (void)msg_type; (void)payload; (void)size;
        return 0;
    }
    return 0;
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

const char* fin_motivation_state_name(fin_motivation_state_t state) {
    switch (state) {
        case FIN_MOTIVATION_STATE_UNINITIALIZED: return "UNINITIALIZED";
        case FIN_MOTIVATION_STATE_INITIALIZED:   return "INITIALIZED";
        case FIN_MOTIVATION_STATE_ACTIVE:        return "ACTIVE";
        case FIN_MOTIVATION_STATE_DEGRADED:      return "DEGRADED";
        case FIN_MOTIVATION_STATE_ERROR:         return "ERROR";
        default: return "UNKNOWN";
    }
}

const char* fin_motivation_override_name(fin_override_level_t level) {
    switch (level) {
        case FIN_OVERRIDE_NONE:    return "NONE";
        case FIN_OVERRIDE_CAUTION: return "CAUTION";
        case FIN_OVERRIDE_REVIEW:  return "REVIEW";
        case FIN_OVERRIDE_BLOCK:   return "BLOCK";
        default: return "UNKNOWN";
    }
}

const char* fin_motivation_fomo_name(fin_fomo_level_t level) {
    switch (level) {
        case FIN_FOMO_NONE:     return "NONE";
        case FIN_FOMO_MILD:     return "MILD";
        case FIN_FOMO_MODERATE: return "MODERATE";
        case FIN_FOMO_STRONG:   return "STRONG";
        case FIN_FOMO_EXTREME:  return "EXTREME";
        default: return "UNKNOWN";
    }
}

const char* financial_motivation_bridge_version(void) {
    return FINANCIAL_MOTIVATION_BRIDGE_VERSION;
}

/* ============================================================================
 * Configuration
 * ============================================================================ */

int financial_motivation_bridge_default_config(fin_motivation_config_t* config) {
    if (!config) {
        set_error("config is NULL");
        return FIN_MOTIVATION_ERR_NULL;
    }

    memset(config, 0, sizeof(fin_motivation_config_t));

    /* Motivation signal parameters */
    config->wanting_sensitivity = 1.0f;
    config->novelty_weight = 0.3f;
    config->urgency_weight = 0.4f;
    config->risk_aversion = 1.0f;

    /* FOMO detection thresholds */
    config->fomo_mild_threshold = 0.2f;
    config->fomo_moderate_threshold = 0.4f;
    config->fomo_strong_threshold = 0.6f;
    config->fomo_extreme_threshold = 0.8f;

    /* Override parameters */
    config->override_caution_gap = 0.3f;
    config->override_review_gap = 0.5f;
    config->override_block_gap = 0.7f;
    config->caution_delay_ms = 5000;
    config->review_delay_ms = 30000;

    /* Learning parameters */
    config->learning_rate = 0.1f;
    config->discount_factor = 0.95f;

    /* Integration settings */
    config->enable_immune_integration = true;
    config->enable_bbb_validation = true;
    config->enable_kg_messaging = true;
    config->enable_health_monitoring = true;

    /* Logging */
    config->verbose_logging = false;

    return FIN_MOTIVATION_ERR_OK;
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

financial_motivation_bridge_t* financial_motivation_bridge_create(
    const fin_motivation_config_t* config)
{
    fin_motivation_heartbeat_global("fin_motivation_create", 0.0f);

    financial_motivation_bridge_t* bridge = (financial_motivation_bridge_t*)
        nimcp_malloc(sizeof(financial_motivation_bridge_t));
    if (!bridge) {
        set_error("Failed to allocate financial_motivation_bridge");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NO_MEMORY,
            "Failed to allocate financial_motivation_bridge");
        return NULL;
    }
    memset(bridge, 0, sizeof(financial_motivation_bridge_t));

    bridge->magic = FINANCIAL_MOTIVATION_BRIDGE_MAGIC;

    /* Copy configuration or use defaults */
    if (config) {
        bridge->config = *config;
    } else {
        financial_motivation_bridge_default_config(&bridge->config);
    }

    /* Initialize learning state */
    bridge->baseline_wanting = 0.5f;
    bridge->recent_prediction_error = 0.0f;
    bridge->cumulative_prediction_error = 0.0f;
    bridge->outcome_count = 0;

    bridge->state = FIN_MOTIVATION_STATE_INITIALIZED;

    fin_motivation_heartbeat_global("fin_motivation_create", 1.0f);
    return bridge;
}

void financial_motivation_bridge_destroy(financial_motivation_bridge_t* bridge) {
    fin_motivation_heartbeat_global("fin_motivation_destroy", 0.0f);

    if (bridge) {
        bridge->magic = 0;
        bridge->state = FIN_MOTIVATION_STATE_UNINITIALIZED;
        nimcp_free(bridge);
        bridge = NULL;
    }
}

int financial_motivation_bridge_reset(financial_motivation_bridge_t* bridge) {
    if (!bridge) {
        set_error("bridge is NULL");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_motivation_bridge_reset: bridge is NULL");
        return FIN_MOTIVATION_ERR_NULL;
    }

    if (bridge->magic != FINANCIAL_MOTIVATION_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_INVALID_STATE,
            "financial_motivation_bridge_reset: invalid magic");
        return FIN_MOTIVATION_ERR_STATE;
    }

    fin_motivation_heartbeat_global("fin_motivation_reset", 0.0f);

    /* Reset stats */
    memset(&bridge->stats, 0, sizeof(fin_motivation_bridge_stats_t));

    /* Reset learning state */
    bridge->baseline_wanting = 0.5f;
    bridge->recent_prediction_error = 0.0f;
    bridge->cumulative_prediction_error = 0.0f;
    bridge->outcome_count = 0;

    bridge->state = FIN_MOTIVATION_STATE_INITIALIZED;

    fin_motivation_heartbeat_global("fin_motivation_reset", 1.0f);
    return FIN_MOTIVATION_ERR_OK;
}

/* ============================================================================
 * Subsystem Setters
 * ============================================================================ */

#define FIN_MOTIVATION_SETTER(name, field) \
    int financial_motivation_bridge_set_##name( \
        financial_motivation_bridge_t* bridge, void* ptr) { \
        if (!bridge) { \
            set_error("bridge is NULL in set_" #name); \
            NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, \
                "financial_motivation_bridge_set_" #name ": bridge is NULL"); \
            return FIN_MOTIVATION_ERR_NULL; \
        } \
        if (bridge->magic != FINANCIAL_MOTIVATION_BRIDGE_MAGIC) { \
            set_error("Invalid bridge magic in set_" #name); \
            return FIN_MOTIVATION_ERR_STATE; \
        } \
        bridge->field = ptr; \
        return FIN_MOTIVATION_ERR_OK; \
    }

FIN_MOTIVATION_SETTER(immune, immune)
FIN_MOTIVATION_SETTER(health_agent, health_agent)
FIN_MOTIVATION_SETTER(kg_wiring, kg_wiring)
FIN_MOTIVATION_SETTER(logger, logger)
FIN_MOTIVATION_SETTER(security, security)
FIN_MOTIVATION_SETTER(bio_router, bio_router)

int financial_motivation_bridge_set_coordinator(
    financial_motivation_bridge_t* bridge,
    brain_cycle_coordinator_t* coordinator)
{
    if (!bridge) {
        set_error("bridge is NULL in set_coordinator");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_motivation_bridge_set_coordinator: bridge is NULL");
        return FIN_MOTIVATION_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_MOTIVATION_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic in set_coordinator");
        return FIN_MOTIVATION_ERR_STATE;
    }
    bridge->coordinator = (void*)coordinator;
    return FIN_MOTIVATION_ERR_OK;
}

int financial_motivation_bridge_set_bbb(
    financial_motivation_bridge_t* bridge,
    bbb_system_t bbb)
{
    if (!bridge) {
        set_error("bridge is NULL in set_bbb");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_motivation_bridge_set_bbb: bridge is NULL");
        return FIN_MOTIVATION_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_MOTIVATION_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic in set_bbb");
        return FIN_MOTIVATION_ERR_STATE;
    }
    bridge->bbb = (void*)bbb;
    return FIN_MOTIVATION_ERR_OK;
}

int financial_motivation_bridge_set_ethics(
    financial_motivation_bridge_t* bridge,
    ethics_engine_t ethics)
{
    if (!bridge) {
        set_error("bridge is NULL in set_ethics");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_motivation_bridge_set_ethics: bridge is NULL");
        return FIN_MOTIVATION_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_MOTIVATION_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic in set_ethics");
        return FIN_MOTIVATION_ERR_STATE;
    }
    bridge->ethics = (void*)ethics;
    return FIN_MOTIVATION_ERR_OK;
}

int financial_motivation_bridge_set_lgss(
    financial_motivation_bridge_t* bridge,
    const void* lgss)
{
    if (!bridge) {
        set_error("bridge is NULL in set_lgss");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_motivation_bridge_set_lgss: bridge is NULL");
        return FIN_MOTIVATION_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_MOTIVATION_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic in set_lgss");
        return FIN_MOTIVATION_ERR_STATE;
    }
    bridge->lgss = lgss;
    return FIN_MOTIVATION_ERR_OK;
}

/* ============================================================================
 * Callback Registration
 * ============================================================================ */

int financial_motivation_bridge_set_fomo_callback(
    financial_motivation_bridge_t* bridge,
    fin_motivation_fomo_callback_t callback,
    void* user_data)
{
    if (!bridge) {
        set_error("bridge is NULL in set_fomo_callback");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_motivation_bridge_set_fomo_callback: bridge is NULL");
        return FIN_MOTIVATION_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_MOTIVATION_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic in set_fomo_callback");
        return FIN_MOTIVATION_ERR_STATE;
    }
    bridge->fomo_callback = callback;
    bridge->fomo_callback_data = user_data;
    return FIN_MOTIVATION_ERR_OK;
}

int financial_motivation_bridge_set_override_callback(
    financial_motivation_bridge_t* bridge,
    fin_motivation_override_callback_t callback,
    void* user_data)
{
    if (!bridge) {
        set_error("bridge is NULL in set_override_callback");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_motivation_bridge_set_override_callback: bridge is NULL");
        return FIN_MOTIVATION_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_MOTIVATION_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic in set_override_callback");
        return FIN_MOTIVATION_ERR_STATE;
    }
    bridge->override_callback = callback;
    bridge->override_callback_data = user_data;
    return FIN_MOTIVATION_ERR_OK;
}

/* ============================================================================
 * Core API - Motivation Evaluation
 * ============================================================================ */

int financial_motivation_bridge_evaluate(
    financial_motivation_bridge_t* bridge,
    const fin_opportunity_t* opportunity,
    fin_motivation_signal_t* out_signal)
{
    if (!bridge) {
        set_error("bridge is NULL");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_motivation_bridge_evaluate: bridge is NULL");
        return FIN_MOTIVATION_ERR_NULL;
    }
    if (!opportunity || !out_signal) {
        set_error("opportunity or out_signal is NULL");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_motivation_bridge_evaluate: NULL argument");
        return FIN_MOTIVATION_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_MOTIVATION_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic");
        return FIN_MOTIVATION_ERR_STATE;
    }

    fin_motivation_heartbeat_global("fin_motivation_evaluate", 0.0f);

    /* BBB validation if enabled */
    if (bridge->config.enable_bbb_validation && bridge->bbb) {
        bridge->stats.bbb_validations++;
        /* bbb_validate_data would be called here */
    }

    /* Immune check if enabled */
    if (bridge->config.enable_immune_integration && bridge->immune) {
        bridge->stats.immune_checks++;
        /* immune_check would be called here */
    }

    /* Compute WANTING (incentive salience)
     * Based on:
     * - Expected return (dopaminergic reward prediction)
     * - Novelty (mesolimbic novelty detection)
     * - Urgency (temporal discounting / FOMO pressure)
     * - Inverse of risk (approach tendency)
     */
    float return_signal = sigmoidf(opportunity->expected_return * 10.0f) * 2.0f - 1.0f;
    float novelty_signal = opportunity->novelty;
    float urgency_signal = opportunity->urgency;
    float risk_avoidance = 1.0f - opportunity->risk_level * bridge->config.risk_aversion;
    risk_avoidance = nimcp_clampf(risk_avoidance, 0.0f, 1.0f);

    float wanting = bridge->config.wanting_sensitivity * (
        0.4f * return_signal +
        bridge->config.novelty_weight * novelty_signal +
        bridge->config.urgency_weight * urgency_signal
    ) * risk_avoidance;

    /* Adjust for baseline (adaptation level) */
    wanting = wanting - bridge->baseline_wanting + 0.5f;
    wanting = nimcp_clampf(wanting, -1.0f, 1.0f);

    /* Compute LIKING (hedonic value)
     * This is the anticipated hedonic experience, not actual consumption
     * Based on expected return and risk-adjusted confidence
     */
    float liking = return_signal * risk_avoidance;
    liking = nimcp_clampf(liking, -1.0f, 1.0f);

    /* Compute LEARNING (prediction error signal)
     * Use recent prediction error as the learning signal
     * In real usage, this updates when outcomes are received
     */
    float learning = bridge->recent_prediction_error;
    learning = nimcp_clampf(learning, -1.0f, 1.0f);

    out_signal->wanting = wanting;
    out_signal->liking = liking;
    out_signal->learning = learning;

    /* Update bridge state */
    bridge->state = FIN_MOTIVATION_STATE_ACTIVE;
    bridge->stats.evaluations++;

    /* KG messaging */
    bridge_kg_publish(bridge, KG_MSG_FIN_MOTIVATION_EVAL, out_signal, sizeof(*out_signal));

    fin_motivation_heartbeat_global("fin_motivation_evaluate", 1.0f);
    return FIN_MOTIVATION_ERR_OK;
}

int financial_motivation_bridge_detect_fomo(
    financial_motivation_bridge_t* bridge,
    const fin_opportunity_t* opportunity,
    const fin_motivation_signal_t* signal,
    fin_fomo_result_t* out_fomo)
{
    if (!bridge) {
        set_error("bridge is NULL");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_motivation_bridge_detect_fomo: bridge is NULL");
        return FIN_MOTIVATION_ERR_NULL;
    }
    if (!opportunity || !signal || !out_fomo) {
        set_error("NULL argument");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_motivation_bridge_detect_fomo: NULL argument");
        return FIN_MOTIVATION_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_MOTIVATION_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic");
        return FIN_MOTIVATION_ERR_STATE;
    }

    fin_motivation_heartbeat_global("fin_motivation_detect_fomo", 0.0f);

    memset(out_fomo, 0, sizeof(fin_fomo_result_t));

    /* Compute rational baseline for wanting
     * Based purely on expected return adjusted for risk
     */
    float rational_wanting = opportunity->expected_return /
        (1.0f + opportunity->risk_level * bridge->config.risk_aversion);
    rational_wanting = sigmoidf(rational_wanting * 5.0f);

    /* FOMO = wanting exceeds rational baseline */
    float wanting_excess = signal->wanting - rational_wanting;
    wanting_excess = nimcp_clampf(wanting_excess, -1.0f, 1.0f);

    /* Component biases */
    float urgency_bias = opportunity->urgency * bridge->config.urgency_weight;
    float novelty_bias = opportunity->novelty * bridge->config.novelty_weight;

    /* Estimate herd signal (social influence)
     * In a full system this would come from market data
     * Here we use urgency as a proxy
     */
    float herd_signal = opportunity->urgency * 0.5f;

    out_fomo->wanting_excess = wanting_excess;
    out_fomo->urgency_bias = urgency_bias;
    out_fomo->novelty_bias = novelty_bias;
    out_fomo->herd_signal = herd_signal;

    /* Determine FOMO level */
    if (wanting_excess >= bridge->config.fomo_extreme_threshold) {
        out_fomo->level = FIN_FOMO_EXTREME;
        snprintf(out_fomo->trigger, FIN_MOTIVATION_DESC_LEN,
                 "Extreme urgency bias (%.2f)", wanting_excess);
    } else if (wanting_excess >= bridge->config.fomo_strong_threshold) {
        out_fomo->level = FIN_FOMO_STRONG;
        snprintf(out_fomo->trigger, FIN_MOTIVATION_DESC_LEN,
                 "Strong novelty/urgency drive (%.2f)", wanting_excess);
    } else if (wanting_excess >= bridge->config.fomo_moderate_threshold) {
        out_fomo->level = FIN_FOMO_MODERATE;
        snprintf(out_fomo->trigger, FIN_MOTIVATION_DESC_LEN,
                 "Moderate FOMO detected (%.2f)", wanting_excess);
    } else if (wanting_excess >= bridge->config.fomo_mild_threshold) {
        out_fomo->level = FIN_FOMO_MILD;
        snprintf(out_fomo->trigger, FIN_MOTIVATION_DESC_LEN,
                 "Mild urgency bias (%.2f)", wanting_excess);
    } else {
        out_fomo->level = FIN_FOMO_NONE;
        snprintf(out_fomo->trigger, FIN_MOTIVATION_DESC_LEN,
                 "No significant FOMO");
    }

    /* Update stats */
    if (out_fomo->level > FIN_FOMO_NONE) {
        bridge->stats.fomo_detections++;

        /* Fire callback if registered */
        if (bridge->fomo_callback) {
            bridge->fomo_callback(opportunity, out_fomo, bridge->fomo_callback_data);
        }

        /* KG messaging */
        bridge_kg_publish(bridge, KG_MSG_FIN_MOTIVATION_FOMO, out_fomo, sizeof(*out_fomo));
    }

    fin_motivation_heartbeat_global("fin_motivation_detect_fomo", 1.0f);
    return FIN_MOTIVATION_ERR_OK;
}

int financial_motivation_bridge_rational_value(
    financial_motivation_bridge_t* bridge,
    const fin_opportunity_t* opportunity,
    fin_rational_value_t* out_rational)
{
    if (!bridge) {
        set_error("bridge is NULL");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_motivation_bridge_rational_value: bridge is NULL");
        return FIN_MOTIVATION_ERR_NULL;
    }
    if (!opportunity || !out_rational) {
        set_error("NULL argument");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_motivation_bridge_rational_value: NULL argument");
        return FIN_MOTIVATION_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_MOTIVATION_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic");
        return FIN_MOTIVATION_ERR_STATE;
    }

    fin_motivation_heartbeat_global("fin_motivation_rational", 0.0f);

    memset(out_rational, 0, sizeof(fin_rational_value_t));

    /* Risk-adjusted expected value
     * Simple Sharpe-like ratio: return / (1 + risk_aversion * risk)
     */
    float risk_penalty = 1.0f + bridge->config.risk_aversion * opportunity->risk_level;
    float expected_value = opportunity->expected_return / risk_penalty;

    /* Opportunity cost estimate
     * Assume baseline opportunity cost is ~2% annualized
     * Adjust for urgency (higher urgency = lower opportunity cost)
     */
    float opportunity_cost = 0.02f * (1.0f - opportunity->urgency * 0.5f);

    /* Kelly fraction for optimal position sizing
     * Simplified: f = (p * b - q) / b
     * where p = win probability, q = loss probability, b = win/loss ratio
     * Use expected return and risk to estimate
     */
    float win_prob = nimcp_clampf(0.5f + opportunity->expected_return * 2.0f, 0.1f, 0.9f);
    float loss_prob = 1.0f - win_prob;
    float win_loss_ratio = 1.0f / (opportunity->risk_level + 0.1f);
    float kelly = (win_prob * win_loss_ratio - loss_prob) / (fabsf(win_loss_ratio) > 1e-7f ? win_loss_ratio : 1e-7f);
    kelly = nimcp_clampf(kelly, 0.0f, 0.25f);  /* Cap at 25% */

    /* Confidence based on information quality
     * Lower novelty = more information = higher confidence
     */
    float confidence = nimcp_clampf(1.0f - opportunity->novelty * 0.5f, 0.3f, 0.95f);

    out_rational->expected_value = expected_value;
    out_rational->opportunity_cost = opportunity_cost;
    out_rational->kelly_fraction = kelly;
    out_rational->confidence = confidence;

    snprintf(out_rational->rationale, FIN_MOTIVATION_DESC_LEN,
             "EV=%.4f, Kelly=%.2f%%, Conf=%.2f",
             expected_value, kelly * 100.0f, confidence);

    fin_motivation_heartbeat_global("fin_motivation_rational", 1.0f);
    return FIN_MOTIVATION_ERR_OK;
}

int financial_motivation_bridge_should_override(
    financial_motivation_bridge_t* bridge,
    const fin_motivation_signal_t* signal,
    const fin_rational_value_t* rational,
    fin_override_result_t* out_override)
{
    if (!bridge) {
        set_error("bridge is NULL");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_motivation_bridge_should_override: bridge is NULL");
        return FIN_MOTIVATION_ERR_NULL;
    }
    if (!signal || !rational || !out_override) {
        set_error("NULL argument");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_motivation_bridge_should_override: NULL argument");
        return FIN_MOTIVATION_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_MOTIVATION_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic");
        return FIN_MOTIVATION_ERR_STATE;
    }

    fin_motivation_heartbeat_global("fin_motivation_override", 0.0f);

    memset(out_override, 0, sizeof(fin_override_result_t));

    /* Normalize rational expected value to wanting scale [-1, 1] */
    float rational_normalized = sigmoidf(rational->expected_value * 10.0f) * 2.0f - 1.0f;

    /* Gap between emotional wanting and rational value */
    float gap = signal->wanting - rational_normalized;

    out_override->wanting_rational_gap = gap;
    out_override->override_confidence = rational->confidence;

    /* Determine override level based on gap */
    if (gap >= bridge->config.override_block_gap) {
        out_override->level = FIN_OVERRIDE_BLOCK;
        out_override->delay_ms = 0;  /* Blocked, no delay */
        snprintf(out_override->reason, FIN_MOTIVATION_DESC_LEN,
                 "BLOCK: Extreme emotion-rational gap (%.2f)", gap);
    } else if (gap >= bridge->config.override_review_gap) {
        out_override->level = FIN_OVERRIDE_REVIEW;
        out_override->delay_ms = bridge->config.review_delay_ms;
        snprintf(out_override->reason, FIN_MOTIVATION_DESC_LEN,
                 "REVIEW: Significant gap requires review (%.2f)", gap);
    } else if (gap >= bridge->config.override_caution_gap) {
        out_override->level = FIN_OVERRIDE_CAUTION;
        out_override->delay_ms = bridge->config.caution_delay_ms;
        snprintf(out_override->reason, FIN_MOTIVATION_DESC_LEN,
                 "CAUTION: Add delay before execution (%.2f)", gap);
    } else {
        out_override->level = FIN_OVERRIDE_NONE;
        out_override->delay_ms = 0;
        snprintf(out_override->reason, FIN_MOTIVATION_DESC_LEN,
                 "OK: Emotion-rational alignment acceptable");
    }

    /* Update stats */
    if (out_override->level > FIN_OVERRIDE_NONE) {
        bridge->stats.rational_overrides++;

        /* Fire callback if registered */
        if (bridge->override_callback) {
            fin_opportunity_t dummy_opp = {0};  /* Would need actual opp */
            bridge->override_callback(&dummy_opp, out_override, bridge->override_callback_data);
        }

        /* KG messaging */
        bridge_kg_publish(bridge, KG_MSG_FIN_MOTIVATION_OVERRIDE,
                          out_override, sizeof(*out_override));
    }

    fin_motivation_heartbeat_global("fin_motivation_override", 1.0f);
    return FIN_MOTIVATION_ERR_OK;
}

/* ============================================================================
 * Learning API
 * ============================================================================ */

int financial_motivation_bridge_process_outcome(
    financial_motivation_bridge_t* bridge,
    const fin_outcome_feedback_t* feedback)
{
    if (!bridge) {
        set_error("bridge is NULL");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_motivation_bridge_process_outcome: bridge is NULL");
        return FIN_MOTIVATION_ERR_NULL;
    }
    if (!feedback) {
        set_error("feedback is NULL");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_motivation_bridge_process_outcome: feedback is NULL");
        return FIN_MOTIVATION_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_MOTIVATION_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic");
        return FIN_MOTIVATION_ERR_STATE;
    }

    fin_motivation_heartbeat_global("fin_motivation_outcome", 0.0f);

    /* Compute prediction error
     * PE = actual_outcome - expected_outcome
     * We use satisfaction as a proxy for subjective outcome evaluation
     */
    float expected = bridge->baseline_wanting;  /* Simplified */
    float actual = feedback->satisfaction;
    float prediction_error = actual - expected;

    /* Update recent prediction error */
    bridge->recent_prediction_error = prediction_error;

    /* Update cumulative (running average) */
    bridge->outcome_count++;
    float n = (float)bridge->outcome_count;
    bridge->cumulative_prediction_error =
        bridge->cumulative_prediction_error * ((n - 1.0f) / (fabsf(n) > 1e-7f ? n : 1e-7f)) +
        prediction_error / (fabsf(n) > 1e-7f ? n : 1e-7f);

    /* Update baseline wanting (adaptation)
     * Baseline moves toward recent outcomes
     */
    bridge->baseline_wanting +=
        bridge->config.learning_rate * prediction_error;
    bridge->baseline_wanting = nimcp_clampf(bridge->baseline_wanting, 0.0f, 1.0f);

    /* KG messaging */
    bridge_kg_publish(bridge, KG_MSG_FIN_MOTIVATION_OUTCOME,
                      feedback, sizeof(*feedback));

    fin_motivation_heartbeat_global("fin_motivation_outcome", 1.0f);
    return FIN_MOTIVATION_ERR_OK;
}

int financial_motivation_bridge_get_prediction_error(
    financial_motivation_bridge_t* bridge,
    float* out_prediction_error)
{
    if (!bridge) {
        set_error("bridge is NULL");
        return FIN_MOTIVATION_ERR_NULL;
    }
    if (!out_prediction_error) {
        set_error("out_prediction_error is NULL");
        return FIN_MOTIVATION_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_MOTIVATION_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic");
        return FIN_MOTIVATION_ERR_STATE;
    }

    *out_prediction_error = bridge->recent_prediction_error;
    return FIN_MOTIVATION_ERR_OK;
}

/* ============================================================================
 * Query API
 * ============================================================================ */

fin_motivation_state_t financial_motivation_bridge_get_state(
    const financial_motivation_bridge_t* bridge)
{
    if (!bridge || bridge->magic != FINANCIAL_MOTIVATION_BRIDGE_MAGIC) {
        return FIN_MOTIVATION_STATE_UNINITIALIZED;
    }
    return bridge->state;
}

int financial_motivation_bridge_get_stats(
    const financial_motivation_bridge_t* bridge,
    fin_motivation_bridge_stats_t* stats)
{
    if (!bridge) {
        set_error("bridge is NULL");
        return FIN_MOTIVATION_ERR_NULL;
    }
    if (!stats) {
        set_error("stats is NULL");
        return FIN_MOTIVATION_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_MOTIVATION_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic");
        return FIN_MOTIVATION_ERR_STATE;
    }

    *stats = bridge->stats;
    return FIN_MOTIVATION_ERR_OK;
}

void financial_motivation_bridge_reset_stats(financial_motivation_bridge_t* bridge) {
    if (bridge && bridge->magic == FINANCIAL_MOTIVATION_BRIDGE_MAGIC) {
        memset(&bridge->stats, 0, sizeof(fin_motivation_bridge_stats_t));
    }
}

/* ============================================================================
 * Health Integration
 * ============================================================================ */

int financial_motivation_bridge_heartbeat(
    financial_motivation_bridge_t* bridge,
    const char* operation,
    float progress)
{
    if (!bridge) {
        return FIN_MOTIVATION_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_MOTIVATION_BRIDGE_MAGIC) {
        return FIN_MOTIVATION_ERR_STATE;
    }

    /* Forward to global health agent */
    fin_motivation_heartbeat_global(operation ? operation : "fin_motivation_heartbeat", progress);

    /* Forward to instance-level health agent */
    if (bridge->health_agent) {
        nimcp_health_agent_heartbeat_ex(
            (nimcp_health_agent_t*)bridge->health_agent, operation, progress);
    }

    bridge->stats.health_heartbeats++;
    return FIN_MOTIVATION_ERR_OK;
}
