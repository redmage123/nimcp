/**
 * @file nimcp_financial_mental_health_bridge.c
 * @brief Financial Mental Health Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-29
 *
 * WHAT: Bridge for assessing mental health state during trading and determining
 *       when trading should be paused for trader wellbeing.
 *
 * WHY:  Trading can severely impact mental health. This bridge provides
 *       real-time monitoring and protection mechanisms.
 *
 * HOW:  Monitors multiple mental health indicators and computes overall
 *       judgment impairment. When impairment exceeds thresholds, trading
 *       is blocked and breaks are recommended.
 *
 * @author NIMCP Development Team
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdarg.h>

#include "cognitive/parietal/nimcp_financial_mental_health_bridge.h"
#include "constants/nimcp_buffer_constants.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/time/nimcp_time.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "security/nimcp_bbb_helpers.h"

/* ============================================================================
 * Constants
 * ============================================================================ */

#define LOG_MODULE "financial_mental_health"

/* ============================================================================
 * Health Agent Integration (Phase 8: System-Wide Health Integration)
 * ============================================================================ */
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/math/nimcp_math_helpers.h"

/* Health agent: using pre-existing custom implementation */
static nimcp_health_agent_t* g_financial_mental_health_bridge_health_agent = NULL;

BRIDGE_DEFINE_MESH_REGISTRATION(financial_mental_health_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


/** @brief Send heartbeat from financial_mental_health_bridge module */
static inline void fin_mh_heartbeat(const char* operation, float progress) {
    if (g_financial_mental_health_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_financial_mental_health_bridge_health_agent, operation, progress);
    }
}

/** @brief Send heartbeat from financial_mental_health_bridge module (instance-level) */
static inline void fin_mh_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_financial_mental_health_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_financial_mental_health_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_financial_mental_health_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}

/* ============================================================================
 * Thread-Local Error
 * ============================================================================ */

static _Thread_local char fin_mh_last_error[NIMCP_ERROR_BUFFER_SIZE] = {0};

static void set_error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(fin_mh_last_error, sizeof(fin_mh_last_error), fmt, args);
    va_end(args);
}

/* ============================================================================
 * KG Wiring Integration
 * ============================================================================ */

#define KG_MSG_FIN_MH_ASSESSMENT        "FIN_MENTAL_HEALTH_ASSESSMENT"
#define KG_MSG_FIN_MH_TRADING_ADVICE    "FIN_MENTAL_HEALTH_TRADING_ADVICE"
#define KG_MSG_FIN_MH_BREAK_RECOMMEND   "FIN_MENTAL_HEALTH_BREAK_RECOMMEND"
#define KG_MSG_FIN_MH_ALERT             "FIN_MENTAL_HEALTH_ALERT"
#define KG_MSG_FIN_MH_ERROR             "FIN_MENTAL_HEALTH_ERROR"

/* ============================================================================
 * Internal Structure
 * ============================================================================ */

/**
 * @brief Financial mental health bridge structure
 */
struct financial_mental_health_bridge {
    bridge_base_t base;                 /**< MUST be first: base bridge infrastructure */
    uint32_t magic;
    fin_mental_health_bridge_state_t state;

    /* Configuration */
    fin_mental_health_config_t config;

    /* Current mental health state */
    fin_mental_health_state_t current_state;

    /* Cached risk level */
    fin_mental_health_risk_t current_risk;

    /* Session tracking */
    uint32_t session_trades;
    uint32_t session_decisions;
    uint32_t session_duration_mins;
    uint32_t time_since_break_mins;
    uint32_t consecutive_losses;

    /* Timestamp of last update */
    uint64_t last_update_ms;
    uint64_t session_start_ms;
    uint64_t last_break_ms;

    /* Subsystem pointers */
    void* immune;
    void* bbb;
    void* health_agent;
    void* kg_wiring;
    void* logger;
    void* security;
    void* ethics;
    void* lgss;
    void* cycle;
    void* bio_router;

    /* Statistics */
    fin_mental_health_bridge_stats_t stats;
};

/* Security integration via bridge_base */
BRIDGE_DEFINE_SECURITY_SETTERS(financial_mental_health_bridge)

/* ============================================================================
 * Static Name Tables
 * ============================================================================ */

static const char* risk_names[] = {
    "low",
    "moderate",
    "elevated",
    "high",
    "critical"
};

static const char* break_names[] = {
    "none",
    "micro",
    "short",
    "medium",
    "long",
    "extended",
    "session_end"
};

static const char* advice_names[] = {
    "advised",
    "caution",
    "reduced",
    "not_advised",
    "blocked"
};

static const char* state_names[] = {
    "uninitialized",
    "initialized",
    "active",
    "monitoring",
    "alert",
    "degraded",
    "error"
};

static inline float maxf(float a, float b) {
    return (a > b) ? a : b;
}

static inline float minf(float a, float b) {
    return (a < b) ? a : b;
}

/**
 * @brief Publish message through KG wiring
 */
static int bridge_kg_publish(financial_mental_health_bridge_t* bridge, const char* msg_type,
                              const void* payload, size_t size) {
    if (bridge && bridge->kg_wiring && bridge->config.enable_kg_messaging) {
        bridge->stats.kg_messages_sent++;
        /* kg_wiring_publish would be called here */
        (void)msg_type; (void)payload; (void)size;
        return 0;
    }
    return 0;
}

/**
 * @brief Calculate combined impairment score
 */
static float calculate_impairment(
    const fin_mental_health_state_t* state,
    const fin_mental_health_config_t* config
) {
    float impairment = 0.0f;
    float total_weight = 0.0f;

    /* Weighted sum of all indicators */
    impairment += state->stress_level * config->stress_weight;
    total_weight += config->stress_weight;

    impairment += state->anxiety_level * config->anxiety_weight;
    total_weight += config->anxiety_weight;

    impairment += state->depression_risk * config->depression_weight;
    total_weight += config->depression_weight;

    impairment += state->cognitive_load * config->cognitive_weight;
    total_weight += config->cognitive_weight;

    impairment += state->decision_fatigue * config->fatigue_weight;
    total_weight += config->fatigue_weight;

    /* Normalize by total weight */
    if (total_weight > 0.0f) {
        impairment /= total_weight;
    }

    return nimcp_clampf(impairment, 0.0f, 1.0f);
}

/**
 * @brief Determine risk level from impairment score and state
 */
static fin_mental_health_risk_t determine_risk_level(
    const fin_mental_health_state_t* state,
    float impairment,
    const fin_mental_health_config_t* config
) {
    /* Check for critical indicators first */
    if (state->stress_level >= config->stress_block_threshold ||
        state->anxiety_level >= config->anxiety_block_threshold ||
        state->cognitive_load >= config->cognitive_load_block ||
        state->decision_fatigue >= config->decision_fatigue_block) {
        return FIN_MH_RISK_CRITICAL;
    }

    /* Check combined impairment */
    if (impairment >= config->impairment_block) {
        return FIN_MH_RISK_HIGH;
    }

    if (impairment >= config->impairment_warning) {
        return FIN_MH_RISK_ELEVATED;
    }

    /* Check for any warning-level indicators */
    if (state->stress_level >= config->stress_warning_threshold ||
        state->anxiety_level >= config->anxiety_warning_threshold ||
        state->depression_risk >= config->depression_warning_threshold ||
        state->cognitive_load >= config->cognitive_load_warning ||
        state->decision_fatigue >= config->decision_fatigue_warning) {
        return FIN_MH_RISK_MODERATE;
    }

    return FIN_MH_RISK_LOW;
}

/**
 * @brief Update state from input factors
 */
static void update_state_from_factors(
    fin_mental_health_state_t* state,
    const fin_health_input_factors_t* factors,
    const fin_mental_health_config_t* config
) {
    /* Calculate stress from trading activity */
    float loss_stress = 0.0f;
    if (factors->losses_today > 0 || factors->consecutive_losses > 0) {
        loss_stress = minf((float)factors->losses_today / 10.0f, 1.0f);
        loss_stress += minf((float)factors->consecutive_losses / 5.0f, 0.5f);
    }

    /* Session duration stress */
    float duration_stress = 0.0f;
    if (factors->session_duration_mins > config->max_session_duration_mins / 2) {
        duration_stress = (float)(factors->session_duration_mins - config->max_session_duration_mins / 2) /
                          (float)(config->max_session_duration_mins / 2);
        duration_stress = nimcp_clampf(duration_stress, 0.0f, 1.0f);
    }

    /* Time since break stress */
    float break_stress = 0.0f;
    if (factors->time_since_break_mins > config->max_time_without_break / 2) {
        break_stress = (float)(factors->time_since_break_mins - config->max_time_without_break / 2) /
                       (float)(config->max_time_without_break / 2);
        break_stress = nimcp_clampf(break_stress, 0.0f, 1.0f);
    }

    /* External stress contribution */
    float external_stress = factors->external_stress;

    /* Combine stress factors */
    state->stress_level = nimcp_clampf(
        state->stress_level * 0.7f +  /* Decay existing */
        (loss_stress + duration_stress + break_stress + external_stress) / 4.0f * 0.3f,
        0.0f, 1.0f
    );

    /* Anxiety increases with P&L volatility and losses */
    float pnl_anxiety = 0.0f;
    if (factors->pnl_today < 0) {
        pnl_anxiety = nimcp_clampf(-factors->pnl_today, 0.0f, 1.0f);
    }
    state->anxiety_level = nimcp_clampf(
        state->anxiety_level * 0.8f + pnl_anxiety * 0.2f,
        0.0f, 1.0f
    );

    /* Cognitive load from decision count */
    float decision_load = 0.0f;
    if (factors->decisions_today > config->max_decisions_per_session / 2) {
        decision_load = (float)(factors->decisions_today - config->max_decisions_per_session / 2) /
                        (float)(config->max_decisions_per_session / 2);
        decision_load = nimcp_clampf(decision_load, 0.0f, 1.0f);
    }
    state->cognitive_load = nimcp_clampf(
        state->cognitive_load * 0.6f + decision_load * 0.4f,
        0.0f, 1.0f
    );

    /* Decision fatigue from trade count */
    float trade_fatigue = 0.0f;
    if (factors->trades_today > config->max_trades_per_session / 2) {
        trade_fatigue = (float)(factors->trades_today - config->max_trades_per_session / 2) /
                        (float)(config->max_trades_per_session / 2);
        trade_fatigue = nimcp_clampf(trade_fatigue, 0.0f, 1.0f);
    }
    state->decision_fatigue = nimcp_clampf(
        state->decision_fatigue * 0.5f + trade_fatigue * 0.3f + duration_stress * 0.2f,
        0.0f, 1.0f
    );

    /* Depression risk from sustained losses and poor sleep */
    float sleep_factor = 1.0f - factors->sleep_quality;
    float wellness_factor = 1.0f - factors->physical_wellness;
    state->depression_risk = nimcp_clampf(
        state->depression_risk * 0.9f +
        (loss_stress * 0.3f + sleep_factor * 0.4f + wellness_factor * 0.3f) * 0.1f,
        0.0f, 1.0f
    );

    /* Biometric integration if available */
    if (factors->biometrics_available && config->enable_biometric_integration) {
        /* Low HRV indicates stress */
        float hrv_stress = 1.0f - factors->heart_rate_variability;
        state->stress_level = nimcp_clampf(
            state->stress_level * 0.7f + hrv_stress * 0.3f,
            0.0f, 1.0f
        );

        /* Cortisol estimate */
        state->stress_level = nimcp_clampf(
            state->stress_level * 0.8f + factors->cortisol_estimate * 0.2f,
            0.0f, 1.0f
        );
    }
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

int financial_mental_health_bridge_default_config(fin_mental_health_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");
        return -1;
    }

    fin_mh_heartbeat("fin_mh_default_config", 0.0f);

    memset(config, 0, sizeof(*config));

    /* Stress thresholds */
    config->stress_warning_threshold = 0.5f;
    config->stress_block_threshold = 0.8f;

    /* Anxiety thresholds */
    config->anxiety_warning_threshold = 0.5f;
    config->anxiety_block_threshold = 0.75f;

    /* Depression thresholds */
    config->depression_warning_threshold = 0.4f;

    /* Cognitive load thresholds */
    config->cognitive_load_warning = 0.6f;
    config->cognitive_load_block = 0.85f;

    /* Decision fatigue thresholds */
    config->decision_fatigue_warning = 0.5f;
    config->decision_fatigue_block = 0.8f;

    /* Combined impairment thresholds */
    config->impairment_warning = 0.5f;
    config->impairment_block = 0.75f;

    /* Trading limits */
    config->max_trades_per_session = 50;
    config->max_decisions_per_session = 100;
    config->max_session_duration_mins = 240;  /* 4 hours */
    config->max_time_without_break = 90;      /* 90 minutes */
    config->max_consecutive_losses = 5;

    /* Weight factors for impairment calculation */
    config->stress_weight = 0.25f;
    config->anxiety_weight = 0.20f;
    config->depression_weight = 0.15f;
    config->cognitive_weight = 0.20f;
    config->fatigue_weight = 0.20f;

    /* Recovery settings */
    config->recovery_rate = 0.02f;  /* 2% per minute during break */
    config->enable_mandatory_breaks = true;
    config->enable_biometric_integration = false;

    /* Integration settings */
    config->enable_immune_integration = true;
    config->enable_bbb_validation = true;
    config->enable_kg_messaging = true;
    config->enable_health_monitoring = true;

    /* Logging */
    config->verbose_logging = false;

    fin_mh_heartbeat("fin_mh_default_config", 1.0f);
    return 0;
}

financial_mental_health_bridge_t* financial_mental_health_bridge_create(
    const fin_mental_health_config_t* config
) {
    fin_mh_heartbeat("fin_mh_create", 0.0f);

    financial_mental_health_bridge_t* bridge = nimcp_calloc(1, sizeof(financial_mental_health_bridge_t));
    if (!bridge) {
        set_error("Failed to allocate financial_mental_health_bridge");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NO_MEMORY, "Failed to allocate financial_mental_health_bridge");
        return NULL;
    }

    bridge->magic = FINANCIAL_MENTAL_HEALTH_BRIDGE_MAGIC;
    bridge->state = FIN_MH_STATE_UNINITIALIZED;

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        financial_mental_health_bridge_default_config(&bridge->config);
    }

    /* Initialize bridge base (creates mutex) */
    if (bridge_base_init(&bridge->base, BIO_MODULE_FINANCIAL_MENTAL_HEALTH, "financial_mental_health") != 0) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "financial_mental_health_bridge_create: validation failed");
        return NULL;
    }

    /* Initialize mental health state to healthy baseline */
    memset(&bridge->current_state, 0, sizeof(bridge->current_state));
    bridge->current_risk = FIN_MH_RISK_LOW;

    /* Initialize session tracking */
    bridge->session_trades = 0;
    bridge->session_decisions = 0;
    bridge->session_duration_mins = 0;
    bridge->time_since_break_mins = 0;
    bridge->consecutive_losses = 0;

    uint64_t now = nimcp_time_get_ms();
    bridge->last_update_ms = now;
    bridge->session_start_ms = now;
    bridge->last_break_ms = now;

    /* Initialize statistics */
    memset(&bridge->stats, 0, sizeof(bridge->stats));

    bridge->state = FIN_MH_STATE_INITIALIZED;

    fin_mh_heartbeat("fin_mh_create", 1.0f);
    return bridge;
}

void financial_mental_health_bridge_destroy(financial_mental_health_bridge_t* bridge) {
    if (!bridge || bridge->magic != FINANCIAL_MENTAL_HEALTH_BRIDGE_MAGIC) {
        return;
    }

    fin_mh_heartbeat("fin_mh_destroy", 0.0f);

    /* Cleanup base */
    bridge_base_cleanup(&bridge->base);

    bridge->magic = 0;
    nimcp_free(bridge);
    bridge = NULL;

    fin_mh_heartbeat("fin_mh_destroy", 1.0f);
}

int financial_mental_health_bridge_reset(financial_mental_health_bridge_t* bridge) {
    if (!bridge || bridge->magic != FINANCIAL_MENTAL_HEALTH_BRIDGE_MAGIC) {
        set_error("bridge is NULL or invalid");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_mental_health_bridge_reset: invalid bridge");
        return FIN_MENTAL_HEALTH_ERR_NULL;
    }

    fin_mh_heartbeat("fin_mh_reset", 0.0f);

    nimcp_mutex_lock(bridge->base.mutex);

    /* Reset mental health state */
    memset(&bridge->current_state, 0, sizeof(bridge->current_state));
    bridge->current_risk = FIN_MH_RISK_LOW;

    /* Reset session tracking */
    bridge->session_trades = 0;
    bridge->session_decisions = 0;
    bridge->session_duration_mins = 0;
    bridge->time_since_break_mins = 0;
    bridge->consecutive_losses = 0;

    uint64_t now = nimcp_time_get_ms();
    bridge->last_update_ms = now;
    bridge->session_start_ms = now;
    bridge->last_break_ms = now;

    /* Reset stats */
    memset(&bridge->stats, 0, sizeof(bridge->stats));

    bridge->state = FIN_MH_STATE_INITIALIZED;

    nimcp_mutex_unlock(bridge->base.mutex);

    fin_mh_heartbeat("fin_mh_reset", 1.0f);
    return FIN_MENTAL_HEALTH_ERR_OK;
}

/* ============================================================================
 * Subsystem Setters
 * ============================================================================ */

#define FIN_MH_SETTER(name, field) \
    int financial_mental_health_bridge_set_##name(financial_mental_health_bridge_t* bridge, void* ptr) { \
        if (!bridge || bridge->magic != FINANCIAL_MENTAL_HEALTH_BRIDGE_MAGIC) { \
            set_error("bridge is NULL in set_" #name); \
            NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_mental_health_bridge_set_" #name ": bridge is NULL"); \
            return FIN_MENTAL_HEALTH_ERR_NULL; \
        } \
        nimcp_mutex_lock(bridge->base.mutex); \
        bridge->field = ptr; \
        nimcp_mutex_unlock(bridge->base.mutex); \
        return FIN_MENTAL_HEALTH_ERR_OK; \
    }

FIN_MH_SETTER(immune,        immune)
FIN_MH_SETTER(health_agent,  health_agent)
FIN_MH_SETTER(kg_wiring,     kg_wiring)
FIN_MH_SETTER(logger,        logger)
FIN_MH_SETTER(security,      security)
FIN_MH_SETTER(bio_router,    bio_router)

/* Security setters for bbb, ethics, lgss, coordinator handled by bridge_base macros above */

/* ============================================================================
 * Core Mental Health API Implementation
 * ============================================================================ */

int financial_mental_health_bridge_assess(
    financial_mental_health_bridge_t* bridge,
    const fin_health_input_factors_t* factors,
    fin_mental_health_assessment_t* assessment
) {
    if (!bridge || bridge->magic != FINANCIAL_MENTAL_HEALTH_BRIDGE_MAGIC) {
        set_error("bridge is NULL or invalid");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_mental_health_bridge_assess: invalid bridge");
        return FIN_MENTAL_HEALTH_ERR_NULL;
    }
    if (!assessment) {
        set_error("assessment is NULL");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_mental_health_bridge_assess: assessment is NULL");
        return FIN_MENTAL_HEALTH_ERR_NULL;
    }

    fin_mh_heartbeat("fin_mh_assess", 0.0f);

    /* BBB validation */
    if (bridge->config.enable_bbb_validation && bridge->bbb && factors) {
        BRIDGE_BBB_VALIDATE(bridge, factors, sizeof(*factors));
        bridge->stats.bbb_validations++;
    }

    /* Immune check */
    if (bridge->config.enable_immune_integration && bridge->immune) {
        bridge->stats.immune_checks++;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Update from factors if provided */
    if (factors) {
        update_state_from_factors(&bridge->current_state, factors, &bridge->config);

        /* Update session tracking */
        bridge->session_trades = factors->trades_today;
        bridge->session_decisions = factors->decisions_today;
        bridge->session_duration_mins = factors->session_duration_mins;
        bridge->time_since_break_mins = factors->time_since_break_mins;
        bridge->consecutive_losses = factors->consecutive_losses;
    }

    /* Calculate impairment */
    float impairment = calculate_impairment(&bridge->current_state, &bridge->config);

    /* Determine judgment impairment flag */
    bridge->current_state.judgment_impaired = (impairment >= bridge->config.impairment_warning);

    /* Determine risk level */
    bridge->current_risk = determine_risk_level(&bridge->current_state, impairment, &bridge->config);

    /* Build assessment result */
    assessment->state = bridge->current_state;
    assessment->risk_level = bridge->current_risk;
    assessment->impairment_score = impairment;
    assessment->wellbeing_score = 1.0f - impairment;
    assessment->timestamp_ms = nimcp_time_get_ms();

    /* Generate summary */
    switch (bridge->current_risk) {
        case FIN_MH_RISK_LOW:
            snprintf(assessment->summary, sizeof(assessment->summary),
                     "Mental health state is good. Wellbeing score: %.0f%%.",
                     assessment->wellbeing_score * 100.0f);
            break;
        case FIN_MH_RISK_MODERATE:
            snprintf(assessment->summary, sizeof(assessment->summary),
                     "Moderate stress detected (%.0f%%). Consider monitoring your state.",
                     bridge->current_state.stress_level * 100.0f);
            break;
        case FIN_MH_RISK_ELEVATED:
            snprintf(assessment->summary, sizeof(assessment->summary),
                     "Elevated impairment (%.0f%%). Trading performance may be affected.",
                     impairment * 100.0f);
            break;
        case FIN_MH_RISK_HIGH:
            snprintf(assessment->summary, sizeof(assessment->summary),
                     "High impairment (%.0f%%). Trading not recommended. Take a break.",
                     impairment * 100.0f);
            break;
        case FIN_MH_RISK_CRITICAL:
            snprintf(assessment->summary, sizeof(assessment->summary),
                     "Critical state detected. STOP trading. Stress=%.0f%%, Anxiety=%.0f%%.",
                     bridge->current_state.stress_level * 100.0f,
                     bridge->current_state.anxiety_level * 100.0f);
            break;
        default:
            snprintf(assessment->summary, sizeof(assessment->summary), "Unknown state.");
            break;
    }

    bridge->last_update_ms = assessment->timestamp_ms;
    bridge->stats.assessments++;

    /* Update bridge state */
    if (bridge->current_risk >= FIN_MH_RISK_HIGH) {
        bridge->state = FIN_MH_STATE_ALERT;
    } else if (bridge->current_risk >= FIN_MH_RISK_MODERATE) {
        bridge->state = FIN_MH_STATE_MONITORING;
    } else {
        bridge->state = FIN_MH_STATE_ACTIVE;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    /* KG notification */
    bridge_kg_publish(bridge, KG_MSG_FIN_MH_ASSESSMENT, assessment, sizeof(*assessment));

    fin_mh_heartbeat("fin_mh_assess", 1.0f);
    bridge->stats.health_heartbeats++;

    return FIN_MENTAL_HEALTH_ERR_OK;
}

int financial_mental_health_bridge_should_trade(
    financial_mental_health_bridge_t* bridge,
    fin_trading_advisability_t* advisability
) {
    if (!bridge || bridge->magic != FINANCIAL_MENTAL_HEALTH_BRIDGE_MAGIC) {
        set_error("bridge is NULL or invalid");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_mental_health_bridge_should_trade: invalid bridge");
        return FIN_MENTAL_HEALTH_ERR_NULL;
    }
    if (!advisability) {
        set_error("advisability is NULL");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_mental_health_bridge_should_trade: advisability is NULL");
        return FIN_MENTAL_HEALTH_ERR_NULL;
    }

    fin_mh_heartbeat("fin_mh_should_trade", 0.0f);

    nimcp_mutex_lock(bridge->base.mutex);

    memset(advisability, 0, sizeof(*advisability));
    advisability->max_position_scale = 1.0f;
    advisability->max_risk_scale = 1.0f;

    const fin_mental_health_config_t* cfg = &bridge->config;
    const fin_mental_health_state_t* s = &bridge->current_state;

    /* Check session limits first */
    bool session_limit_exceeded = false;
    if (bridge->session_trades >= cfg->max_trades_per_session) {
        session_limit_exceeded = true;
        snprintf(advisability->reason, sizeof(advisability->reason),
                 "Max trades per session reached (%u). Take a break.",
                 cfg->max_trades_per_session);
    } else if (bridge->session_duration_mins >= cfg->max_session_duration_mins) {
        session_limit_exceeded = true;
        snprintf(advisability->reason, sizeof(advisability->reason),
                 "Max session duration reached (%u mins). End session.",
                 cfg->max_session_duration_mins);
    } else if (cfg->enable_mandatory_breaks &&
               bridge->time_since_break_mins >= cfg->max_time_without_break) {
        session_limit_exceeded = true;
        snprintf(advisability->reason, sizeof(advisability->reason),
                 "Max time without break reached (%u mins). Take a break.",
                 cfg->max_time_without_break);
    } else if (bridge->consecutive_losses >= cfg->max_consecutive_losses) {
        session_limit_exceeded = true;
        bridge->stats.trading_blocked++;
        snprintf(advisability->reason, sizeof(advisability->reason),
                 "Consecutive loss limit reached (%u). Mandatory break required.",
                 cfg->max_consecutive_losses);
    }

    if (session_limit_exceeded) {
        advisability->advice = FIN_TRADE_BLOCKED;
        advisability->should_trade = false;
        advisability->confidence = 0.95f;
        advisability->max_position_scale = 0.0f;
        advisability->max_risk_scale = 0.0f;
        bridge->stats.trading_blocked++;
        nimcp_mutex_unlock(bridge->base.mutex);
        bridge_kg_publish(bridge, KG_MSG_FIN_MH_TRADING_ADVICE, advisability, sizeof(*advisability));
        fin_mh_heartbeat("fin_mh_should_trade", 1.0f);
        return FIN_MENTAL_HEALTH_ERR_OK;
    }

    /* Check mental health state */
    float impairment = calculate_impairment(s, cfg);

    switch (bridge->current_risk) {
        case FIN_MH_RISK_LOW:
            advisability->advice = FIN_TRADE_ADVISED;
            advisability->should_trade = true;
            advisability->confidence = 0.9f;
            advisability->max_position_scale = 1.0f;
            advisability->max_risk_scale = 1.0f;
            snprintf(advisability->reason, sizeof(advisability->reason),
                     "Mental health state is good. Trading is advisable.");
            break;

        case FIN_MH_RISK_MODERATE:
            advisability->advice = FIN_TRADE_CAUTION;
            advisability->should_trade = true;
            advisability->confidence = 0.75f;
            advisability->max_position_scale = 0.8f;
            advisability->max_risk_scale = 0.8f;
            snprintf(advisability->reason, sizeof(advisability->reason),
                     "Moderate stress (%.0f%%). Trade with caution, consider smaller positions.",
                     s->stress_level * 100.0f);
            break;

        case FIN_MH_RISK_ELEVATED:
            advisability->advice = FIN_TRADE_REDUCED;
            advisability->should_trade = true;
            advisability->confidence = 0.7f;
            advisability->max_position_scale = 0.5f;
            advisability->max_risk_scale = 0.5f;
            snprintf(advisability->reason, sizeof(advisability->reason),
                     "Elevated impairment (%.0f%%). Significantly reduce position sizes.",
                     impairment * 100.0f);
            break;

        case FIN_MH_RISK_HIGH:
            advisability->advice = FIN_TRADE_NOT_ADVISED;
            advisability->should_trade = false;
            advisability->confidence = 0.85f;
            advisability->max_position_scale = 0.0f;
            advisability->max_risk_scale = 0.0f;
            snprintf(advisability->reason, sizeof(advisability->reason),
                     "High impairment (%.0f%%). Trading is not advisable. Take a break.",
                     impairment * 100.0f);
            bridge->stats.trading_blocked++;
            break;

        case FIN_MH_RISK_CRITICAL:
            advisability->advice = FIN_TRADE_BLOCKED;
            advisability->should_trade = false;
            advisability->confidence = 0.95f;
            advisability->max_position_scale = 0.0f;
            advisability->max_risk_scale = 0.0f;
            snprintf(advisability->reason, sizeof(advisability->reason),
                     "CRITICAL: Stress=%.0f%%, Anxiety=%.0f%%. STOP trading immediately.",
                     s->stress_level * 100.0f, s->anxiety_level * 100.0f);
            bridge->stats.trading_blocked++;
            break;

        default:
            advisability->advice = FIN_TRADE_NOT_ADVISED;
            advisability->should_trade = false;
            advisability->confidence = 0.5f;
            snprintf(advisability->reason, sizeof(advisability->reason),
                     "Unknown state. Exercise caution.");
            break;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    /* KG notification */
    bridge_kg_publish(bridge, KG_MSG_FIN_MH_TRADING_ADVICE, advisability, sizeof(*advisability));

    fin_mh_heartbeat("fin_mh_should_trade", 1.0f);
    bridge->stats.health_heartbeats++;

    return FIN_MENTAL_HEALTH_ERR_OK;
}

int financial_mental_health_bridge_recommend_break(
    financial_mental_health_bridge_t* bridge,
    fin_break_recommendation_t* recommendation
) {
    if (!bridge || bridge->magic != FINANCIAL_MENTAL_HEALTH_BRIDGE_MAGIC) {
        set_error("bridge is NULL or invalid");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_mental_health_bridge_recommend_break: invalid bridge");
        return FIN_MENTAL_HEALTH_ERR_NULL;
    }
    if (!recommendation) {
        set_error("recommendation is NULL");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_mental_health_bridge_recommend_break: recommendation is NULL");
        return FIN_MENTAL_HEALTH_ERR_NULL;
    }

    fin_mh_heartbeat("fin_mh_recommend_break", 0.0f);

    nimcp_mutex_lock(bridge->base.mutex);

    memset(recommendation, 0, sizeof(*recommendation));

    const fin_mental_health_config_t* cfg = &bridge->config;
    const fin_mental_health_state_t* s = &bridge->current_state;
    float impairment = calculate_impairment(s, cfg);

    /* Determine break type based on risk level and impairment */
    switch (bridge->current_risk) {
        case FIN_MH_RISK_LOW:
            /* Check if time-based break is needed */
            if (bridge->time_since_break_mins >= cfg->max_time_without_break) {
                recommendation->break_type = FIN_BREAK_SHORT;
                recommendation->duration_minutes = 15;
                recommendation->urgency = 0.4f;
                recommendation->mandatory = cfg->enable_mandatory_breaks;
                snprintf(recommendation->message, sizeof(recommendation->message),
                         "Scheduled break recommended after %u minutes of trading.",
                         bridge->time_since_break_mins);
            } else {
                recommendation->break_type = FIN_BREAK_NONE;
                recommendation->duration_minutes = 0;
                recommendation->urgency = 0.0f;
                recommendation->mandatory = false;
                snprintf(recommendation->message, sizeof(recommendation->message),
                         "No break needed. Wellbeing is good.");
            }
            break;

        case FIN_MH_RISK_MODERATE:
            recommendation->break_type = FIN_BREAK_MICRO;
            recommendation->duration_minutes = 5;
            recommendation->urgency = 0.4f;
            recommendation->mandatory = false;
            snprintf(recommendation->message, sizeof(recommendation->message),
                     "Consider a 5-minute break. Stress level: %.0f%%.",
                     s->stress_level * 100.0f);
            snprintf(recommendation->activities, sizeof(recommendation->activities),
                     "Deep breathing, stretch, look away from screens.");
            break;

        case FIN_MH_RISK_ELEVATED:
            recommendation->break_type = FIN_BREAK_SHORT;
            recommendation->duration_minutes = 15;
            recommendation->urgency = 0.6f;
            recommendation->mandatory = false;
            snprintf(recommendation->message, sizeof(recommendation->message),
                     "A 15-minute break is recommended. Impairment: %.0f%%.",
                     impairment * 100.0f);
            snprintf(recommendation->activities, sizeof(recommendation->activities),
                     "Walk around, hydrate, light snack, mindfulness exercise.");
            break;

        case FIN_MH_RISK_HIGH:
            recommendation->break_type = FIN_BREAK_MEDIUM;
            recommendation->duration_minutes = 30;
            recommendation->urgency = 0.8f;
            recommendation->mandatory = true;
            snprintf(recommendation->message, sizeof(recommendation->message),
                     "A 30-minute break is strongly recommended. Impairment: %.0f%%.",
                     impairment * 100.0f);
            snprintf(recommendation->activities, sizeof(recommendation->activities),
                     "Physical activity, meal break, meditation, contact a friend.");
            break;

        case FIN_MH_RISK_CRITICAL:
            /* Check severity to determine if session should end */
            if (impairment >= 0.9f || s->stress_level >= 0.9f) {
                recommendation->break_type = FIN_BREAK_SESSION_END;
                recommendation->duration_minutes = 120;  /* Minimum 2 hours */
                recommendation->urgency = 1.0f;
                recommendation->mandatory = true;
                snprintf(recommendation->message, sizeof(recommendation->message),
                         "END TRADING SESSION. Critical mental state detected.");
                snprintf(recommendation->activities, sizeof(recommendation->activities),
                         "Rest, sleep if needed, physical exercise, seek support if distressed.");
            } else {
                recommendation->break_type = FIN_BREAK_EXTENDED;
                recommendation->duration_minutes = 60;
                recommendation->urgency = 0.95f;
                recommendation->mandatory = true;
                snprintf(recommendation->message, sizeof(recommendation->message),
                         "Extended 1-hour break REQUIRED. Stress=%.0f%%, Anxiety=%.0f%%.",
                         s->stress_level * 100.0f, s->anxiety_level * 100.0f);
                snprintf(recommendation->activities, sizeof(recommendation->activities),
                         "Leave trading area, exercise, eat well, review trading journal.");
            }
            break;

        default:
            recommendation->break_type = FIN_BREAK_SHORT;
            recommendation->duration_minutes = 15;
            recommendation->urgency = 0.5f;
            recommendation->mandatory = false;
            snprintf(recommendation->message, sizeof(recommendation->message),
                     "Break recommended due to uncertain state.");
            break;
    }

    /* Check for consecutive loss override */
    if (bridge->consecutive_losses >= cfg->max_consecutive_losses) {
        recommendation->break_type = FIN_BREAK_MEDIUM;
        recommendation->duration_minutes = maxf(recommendation->duration_minutes, 30);
        recommendation->urgency = maxf(recommendation->urgency, 0.85f);
        recommendation->mandatory = true;
        snprintf(recommendation->message, sizeof(recommendation->message),
                 "MANDATORY break after %u consecutive losses. Clear your mind.",
                 bridge->consecutive_losses);
    }

    bridge->stats.breaks_recommended++;

    nimcp_mutex_unlock(bridge->base.mutex);

    /* KG notification */
    bridge_kg_publish(bridge, KG_MSG_FIN_MH_BREAK_RECOMMEND, recommendation, sizeof(*recommendation));

    fin_mh_heartbeat("fin_mh_recommend_break", 1.0f);
    bridge->stats.health_heartbeats++;

    return FIN_MENTAL_HEALTH_ERR_OK;
}

/* ============================================================================
 * State Update API Implementation
 * ============================================================================ */

int financial_mental_health_bridge_update_state(
    financial_mental_health_bridge_t* bridge,
    const fin_mental_health_state_t* state
) {
    if (!bridge || bridge->magic != FINANCIAL_MENTAL_HEALTH_BRIDGE_MAGIC) {
        set_error("bridge is NULL or invalid");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_mental_health_bridge_update_state: invalid bridge");
        return FIN_MENTAL_HEALTH_ERR_NULL;
    }
    if (!state) {
        set_error("state is NULL");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_mental_health_bridge_update_state: state is NULL");
        return FIN_MENTAL_HEALTH_ERR_NULL;
    }

    fin_mh_heartbeat("fin_mh_update_state", 0.0f);

    /* BBB validation */
    if (bridge->config.enable_bbb_validation && bridge->bbb) {
        BRIDGE_BBB_VALIDATE(bridge, state, sizeof(*state));
        bridge->stats.bbb_validations++;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->current_state = *state;

    /* Recalculate risk level */
    float impairment = calculate_impairment(&bridge->current_state, &bridge->config);
    bridge->current_state.judgment_impaired = (impairment >= bridge->config.impairment_warning);
    bridge->current_risk = determine_risk_level(&bridge->current_state, impairment, &bridge->config);

    bridge->last_update_ms = nimcp_time_get_ms();

    /* Update bridge state */
    if (bridge->current_risk >= FIN_MH_RISK_HIGH) {
        bridge->state = FIN_MH_STATE_ALERT;
    } else if (bridge->current_risk >= FIN_MH_RISK_MODERATE) {
        bridge->state = FIN_MH_STATE_MONITORING;
    } else {
        bridge->state = FIN_MH_STATE_ACTIVE;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    fin_mh_heartbeat("fin_mh_update_state", 1.0f);

    return FIN_MENTAL_HEALTH_ERR_OK;
}

int financial_mental_health_bridge_update_from_factors(
    financial_mental_health_bridge_t* bridge,
    const fin_health_input_factors_t* factors
) {
    if (!bridge || bridge->magic != FINANCIAL_MENTAL_HEALTH_BRIDGE_MAGIC) {
        set_error("bridge is NULL or invalid");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_mental_health_bridge_update_from_factors: invalid bridge");
        return FIN_MENTAL_HEALTH_ERR_NULL;
    }
    if (!factors) {
        set_error("factors is NULL");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_mental_health_bridge_update_from_factors: factors is NULL");
        return FIN_MENTAL_HEALTH_ERR_NULL;
    }

    fin_mh_heartbeat("fin_mh_update_from_factors", 0.0f);

    /* BBB validation */
    if (bridge->config.enable_bbb_validation && bridge->bbb) {
        BRIDGE_BBB_VALIDATE(bridge, factors, sizeof(*factors));
        bridge->stats.bbb_validations++;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    update_state_from_factors(&bridge->current_state, factors, &bridge->config);

    /* Update session tracking */
    bridge->session_trades = factors->trades_today;
    bridge->session_decisions = factors->decisions_today;
    bridge->session_duration_mins = factors->session_duration_mins;
    bridge->time_since_break_mins = factors->time_since_break_mins;
    bridge->consecutive_losses = factors->consecutive_losses;

    /* Recalculate risk level */
    float impairment = calculate_impairment(&bridge->current_state, &bridge->config);
    bridge->current_state.judgment_impaired = (impairment >= bridge->config.impairment_warning);
    bridge->current_risk = determine_risk_level(&bridge->current_state, impairment, &bridge->config);

    bridge->last_update_ms = nimcp_time_get_ms();

    nimcp_mutex_unlock(bridge->base.mutex);

    fin_mh_heartbeat("fin_mh_update_from_factors", 1.0f);

    return FIN_MENTAL_HEALTH_ERR_OK;
}

int financial_mental_health_bridge_record_break(
    financial_mental_health_bridge_t* bridge,
    uint32_t duration_minutes
) {
    if (!bridge || bridge->magic != FINANCIAL_MENTAL_HEALTH_BRIDGE_MAGIC) {
        set_error("bridge is NULL or invalid");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_mental_health_bridge_record_break: invalid bridge");
        return FIN_MENTAL_HEALTH_ERR_NULL;
    }

    fin_mh_heartbeat("fin_mh_record_break", 0.0f);

    nimcp_mutex_lock(bridge->base.mutex);

    /* Apply recovery based on break duration */
    float recovery_amount = bridge->config.recovery_rate * (float)duration_minutes;

    fin_mental_health_state_t* s = &bridge->current_state;
    s->stress_level = nimcp_clampf(s->stress_level - recovery_amount, 0.0f, 1.0f);
    s->anxiety_level = nimcp_clampf(s->anxiety_level - recovery_amount * 0.8f, 0.0f, 1.0f);
    s->cognitive_load = nimcp_clampf(s->cognitive_load - recovery_amount * 1.2f, 0.0f, 1.0f);
    s->decision_fatigue = nimcp_clampf(s->decision_fatigue - recovery_amount * 1.5f, 0.0f, 1.0f);
    s->depression_risk = nimcp_clampf(s->depression_risk - recovery_amount * 0.5f, 0.0f, 1.0f);

    /* Reset consecutive losses counter after break */
    bridge->consecutive_losses = 0;
    bridge->time_since_break_mins = 0;
    bridge->last_break_ms = nimcp_time_get_ms();

    /* Recalculate risk level */
    float impairment = calculate_impairment(s, &bridge->config);
    s->judgment_impaired = (impairment >= bridge->config.impairment_warning);
    bridge->current_risk = determine_risk_level(s, impairment, &bridge->config);

    nimcp_mutex_unlock(bridge->base.mutex);

    fin_mh_heartbeat("fin_mh_record_break", 1.0f);

    return FIN_MENTAL_HEALTH_ERR_OK;
}

int financial_mental_health_bridge_apply_recovery(
    financial_mental_health_bridge_t* bridge,
    uint64_t elapsed_ms
) {
    if (!bridge || bridge->magic != FINANCIAL_MENTAL_HEALTH_BRIDGE_MAGIC) {
        set_error("bridge is NULL or invalid");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_mental_health_bridge_apply_recovery: invalid bridge");
        return FIN_MENTAL_HEALTH_ERR_NULL;
    }

    if (elapsed_ms == 0) {
        return FIN_MENTAL_HEALTH_ERR_OK;
    }

    fin_mh_heartbeat("fin_mh_apply_recovery", 0.0f);

    nimcp_mutex_lock(bridge->base.mutex);

    /* Natural recovery is slower than break recovery */
    float elapsed_mins = (float)elapsed_ms / 60000.0f;
    float natural_recovery = bridge->config.recovery_rate * 0.1f * elapsed_mins;

    fin_mental_health_state_t* s = &bridge->current_state;

    /* Only apply recovery if not actively stressed */
    if (s->stress_level > 0.0f) {
        s->stress_level = nimcp_clampf(s->stress_level - natural_recovery * 0.5f, 0.0f, 1.0f);
    }
    if (s->anxiety_level > 0.0f) {
        s->anxiety_level = nimcp_clampf(s->anxiety_level - natural_recovery * 0.3f, 0.0f, 1.0f);
    }
    if (s->cognitive_load > 0.0f) {
        s->cognitive_load = nimcp_clampf(s->cognitive_load - natural_recovery * 0.4f, 0.0f, 1.0f);
    }
    if (s->decision_fatigue > 0.0f) {
        s->decision_fatigue = nimcp_clampf(s->decision_fatigue - natural_recovery * 0.3f, 0.0f, 1.0f);
    }

    /* Update time tracking */
    bridge->time_since_break_mins += (uint32_t)(elapsed_mins);
    bridge->session_duration_mins += (uint32_t)(elapsed_mins);

    /* Recalculate risk level */
    float impairment = calculate_impairment(s, &bridge->config);
    s->judgment_impaired = (impairment >= bridge->config.impairment_warning);
    bridge->current_risk = determine_risk_level(s, impairment, &bridge->config);

    bridge->last_update_ms = nimcp_time_get_ms();

    nimcp_mutex_unlock(bridge->base.mutex);

    fin_mh_heartbeat("fin_mh_apply_recovery", 1.0f);

    return FIN_MENTAL_HEALTH_ERR_OK;
}

/* ============================================================================
 * Query Implementation
 * ============================================================================ */

int financial_mental_health_bridge_get_state(
    const financial_mental_health_bridge_t* bridge,
    fin_mental_health_state_t* state
) {
    if (!bridge || bridge->magic != FINANCIAL_MENTAL_HEALTH_BRIDGE_MAGIC || !state) {
        set_error("NULL argument in get_state");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "get_state: NULL argument");
        return FIN_MENTAL_HEALTH_ERR_NULL;
    }

    fin_mh_heartbeat("fin_mh_get_state", 0.0f);

    nimcp_mutex_lock(bridge->base.mutex);
    *state = bridge->current_state;
    nimcp_mutex_unlock(bridge->base.mutex);

    return FIN_MENTAL_HEALTH_ERR_OK;
}

fin_mental_health_risk_t financial_mental_health_bridge_get_risk_level(
    const financial_mental_health_bridge_t* bridge
) {
    if (!bridge || bridge->magic != FINANCIAL_MENTAL_HEALTH_BRIDGE_MAGIC) {
        return FIN_MH_RISK_CRITICAL;  /* Fail safe */
    }
    fin_mh_heartbeat("fin_mh_get_risk_level", 0.0f);
    return bridge->current_risk;
}

fin_mental_health_bridge_state_t financial_mental_health_bridge_get_bridge_state(
    const financial_mental_health_bridge_t* bridge
) {
    if (!bridge || bridge->magic != FINANCIAL_MENTAL_HEALTH_BRIDGE_MAGIC) {
        return FIN_MH_STATE_ERROR;
    }
    fin_mh_heartbeat("fin_mh_get_bridge_state", 0.0f);
    return bridge->state;
}

int financial_mental_health_bridge_get_stats(
    const financial_mental_health_bridge_t* bridge,
    fin_mental_health_bridge_stats_t* stats
) {
    if (!bridge || bridge->magic != FINANCIAL_MENTAL_HEALTH_BRIDGE_MAGIC || !stats) {
        set_error("NULL argument in get_stats");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "get_stats: NULL argument");
        return FIN_MENTAL_HEALTH_ERR_NULL;
    }

    fin_mh_heartbeat("fin_mh_get_stats", 0.0f);

    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);

    return FIN_MENTAL_HEALTH_ERR_OK;
}

void financial_mental_health_bridge_reset_stats(financial_mental_health_bridge_t* bridge) {
    if (!bridge || bridge->magic != FINANCIAL_MENTAL_HEALTH_BRIDGE_MAGIC) {
        return;
    }

    fin_mh_heartbeat("fin_mh_reset_stats", 0.0f);

    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    nimcp_mutex_unlock(bridge->base.mutex);
}

const char* financial_mental_health_bridge_get_last_error(void) {
    return fin_mh_last_error;
}

/* ============================================================================
 * Health Integration
 * ============================================================================ */

int financial_mental_health_bridge_heartbeat(
    financial_mental_health_bridge_t* bridge,
    const char* operation,
    float progress
) {
    if (!bridge || bridge->magic != FINANCIAL_MENTAL_HEALTH_BRIDGE_MAGIC) {
        return FIN_MENTAL_HEALTH_ERR_NULL;
    }

    /* Forward to global health agent */
    fin_mh_heartbeat(operation ? operation : "fin_mh_heartbeat", progress);

    /* Forward to instance-level health agent */
    if (bridge->health_agent) {
        nimcp_health_agent_heartbeat_ex(
            (nimcp_health_agent_t*)bridge->health_agent, operation, progress);
    }

    bridge->stats.health_heartbeats++;
    return FIN_MENTAL_HEALTH_ERR_OK;
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

const char* fin_mental_health_risk_name(fin_mental_health_risk_t risk) {
    if (risk >= FIN_MH_RISK_COUNT) {
        return "unknown";
    }
    return risk_names[risk];
}

const char* fin_mental_health_break_name(fin_break_type_t break_type) {
    if (break_type >= FIN_BREAK_COUNT) {
        return "unknown";
    }
    return break_names[break_type];
}

const char* fin_mental_health_advice_name(fin_trading_advice_t advice) {
    if (advice >= FIN_TRADE_ADVICE_COUNT) {
        return "unknown";
    }
    return advice_names[advice];
}

const char* fin_mental_health_state_name(fin_mental_health_bridge_state_t state) {
    if (state > FIN_MH_STATE_ERROR) {
        return "unknown";
    }
    return state_names[state];
}

const char* financial_mental_health_bridge_version(void) {
    return FINANCIAL_MENTAL_HEALTH_BRIDGE_VERSION;
}

/* ============================================================================
 * Training Integration (B23 Upgrade Compatibility)
 * ============================================================================ */

int financial_mental_health_bridge_training_begin(financial_mental_health_bridge_t* bridge) {
    if (!bridge || bridge->magic != FINANCIAL_MENTAL_HEALTH_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "financial_mental_health_bridge_training_begin: NULL argument");
        return -1;
    }
    fin_mh_heartbeat_instance((nimcp_health_agent_t*)bridge->health_agent,
                               "financial_mental_health_bridge_training_begin", 0.0f);
    return 0;
}

int financial_mental_health_bridge_training_end(financial_mental_health_bridge_t* bridge) {
    if (!bridge || bridge->magic != FINANCIAL_MENTAL_HEALTH_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "financial_mental_health_bridge_training_end: NULL argument");
        return -1;
    }
    fin_mh_heartbeat_instance((nimcp_health_agent_t*)bridge->health_agent,
                               "financial_mental_health_bridge_training_end", 1.0f);
    return 0;
}

int financial_mental_health_bridge_training_step(financial_mental_health_bridge_t* bridge, float progress) {
    if (!bridge || bridge->magic != FINANCIAL_MENTAL_HEALTH_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "financial_mental_health_bridge_training_step: NULL argument");
        return -1;
    }

    /* Safety gates: ethics + LGSS pre-check */
    BRIDGE_ETHICS_GATE(bridge, "financial_mental_health_bridge_training_step");
    BRIDGE_LGSS_GATE(bridge, "financial_mental_health_bridge_training_step");

    fin_mh_heartbeat_instance((nimcp_health_agent_t*)bridge->health_agent,
                               "financial_mental_health_bridge_training_step", progress);

    /* Notify coordinator of update cycle completion */
    bridge_base_notify_coordinator_tick(&bridge->base, 0);
    return 0;
}
