/**
 * @file nimcp_financial_emotion_bridge.c
 * @brief Financial Emotion Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-29
 *
 * WHAT: Bridge for modeling and tracking emotional states during trading using
 *       Plutchik's 8 primary emotions plus financial-specific compound emotions.
 *
 * WHY:  Emotions significantly impact trading decisions and outcomes. This bridge
 *       enables real-time emotional state tracking, decision modulation, and
 *       bias detection.
 *
 * HOW:  Market events trigger emotional responses via appraisal theory. Primary
 *       emotions combine to form compound emotions. Emotional states modulate
 *       decision parameters.
 *
 * @author NIMCP Development Team
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdarg.h>

#include "cognitive/parietal/nimcp_financial_emotion_bridge.h"
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

#define LOG_MODULE "financial_emotion"

/* ============================================================================
 * Health Agent Integration (Phase 8: System-Wide Health Integration)
 * ============================================================================ */
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/math/nimcp_math_helpers.h"

/* Health agent: using pre-existing custom implementation */
static nimcp_health_agent_t* g_financial_emotion_bridge_health_agent = NULL;

BRIDGE_DEFINE_MESH_REGISTRATION(financial_emotion_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


/** @brief Send heartbeat from financial_emotion_bridge module */
static inline void fin_emotion_heartbeat(const char* operation, float progress) {
    if (g_financial_emotion_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_financial_emotion_bridge_health_agent, operation, progress);
    }
}

/** @brief Send heartbeat from financial_emotion_bridge module (instance-level) */
static inline void fin_emotion_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_financial_emotion_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_financial_emotion_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_financial_emotion_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}

/* ============================================================================
 * Thread-Local Error
 * ============================================================================ */

static _Thread_local char fin_emotion_last_error[NIMCP_ERROR_BUFFER_SIZE] = {0};

static void set_error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(fin_emotion_last_error, sizeof(fin_emotion_last_error), fmt, args);
    va_end(args);
}

/* ============================================================================
 * KG Wiring Integration
 * ============================================================================ */

#define KG_MSG_FIN_EMOTION_UPDATE       "FIN_EMOTION_UPDATE"
#define KG_MSG_FIN_EMOTION_MODULATE     "FIN_EMOTION_MODULATE"
#define KG_MSG_FIN_EMOTION_BIAS         "FIN_EMOTION_BIAS"
#define KG_MSG_FIN_EMOTION_ERROR        "FIN_EMOTION_ERROR"

/* ============================================================================
 * Internal Structure
 * ============================================================================ */

/**
 * @brief Financial emotion bridge structure
 */
struct financial_emotion_bridge {
    bridge_base_t base;                 /**< MUST be first: base bridge infrastructure */
    uint32_t magic;
    fin_emotion_bridge_state_t state;

    /* Configuration */
    fin_emotion_config_t config;

    /* Current emotional state */
    fin_emotion_state_t current_state;

    /* Timestamp of last update */
    uint64_t last_update_ms;

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
    fin_emotion_bridge_stats_t stats;
};

/* Security integration via bridge_base */
BRIDGE_DEFINE_SECURITY_SETTERS(financial_emotion_bridge)

/* ============================================================================
 * Static Name Tables
 * ============================================================================ */

static const char* primary_emotion_names[] = {
    "joy",
    "sadness",
    "anger",
    "fear",
    "surprise",
    "disgust",
    "trust",
    "anticipation"
};

static const char* compound_emotion_names[] = {
    "greed",
    "panic",
    "fomo",
    "euphoria",
    "anxiety",
    "regret"
};

static const char* dominant_emotion_names[] = {
    "neutral",
    "joy",
    "sadness",
    "anger",
    "fear",
    "surprise",
    "disgust",
    "trust",
    "anticipation",
    "greed",
    "panic",
    "fomo",
    "euphoria",
    "anxiety",
    "regret"
};

static const char* bias_names[] = {
    "none",
    "fomo",
    "panic_selling",
    "greed_overtrading",
    "loss_aversion",
    "overconfidence",
    "revenge_trading"
};

static const char* event_names[] = {
    "price_increase",
    "price_decrease",
    "volume_spike",
    "volatility_spike",
    "stop_loss_hit",
    "profit_target_hit",
    "news_positive",
    "news_negative",
    "regime_change",
    "missed_opportunity"
};

static const char* state_names[] = {
    "uninitialized",
    "initialized",
    "active",
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
static int bridge_kg_publish(financial_emotion_bridge_t* bridge, const char* msg_type,
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
 * @brief Update compound emotions from primary emotions
 */
static void update_compound_emotions(fin_emotion_state_t* state) {
    /* Greed = joy + anticipation */
    state->greed = nimcp_clampf((state->joy + state->anticipation) / 2.0f, 0.0f, 1.0f);

    /* Panic = fear + surprise */
    state->panic = nimcp_clampf((state->fear + state->surprise) / 2.0f, 0.0f, 1.0f);

    /* FOMO = fear + anticipation (with emphasis on fear of missing) */
    state->fomo = nimcp_clampf((state->fear * 0.6f + state->anticipation * 0.4f), 0.0f, 1.0f);

    /* Euphoria = joy + surprise */
    state->euphoria = nimcp_clampf((state->joy + state->surprise) / 2.0f, 0.0f, 1.0f);

    /* Anxiety = fear + anticipation (worried anticipation) */
    state->anxiety = nimcp_clampf((state->fear * 0.7f + state->anticipation * 0.3f), 0.0f, 1.0f);

    /* Regret = sadness + anger */
    state->regret = nimcp_clampf((state->sadness + state->anger) / 2.0f, 0.0f, 1.0f);
}

/**
 * @brief Apply appraisal to generate emotional response to event
 */
static void apply_event_appraisal(
    fin_emotion_state_t* state,
    const fin_market_event_t* event,
    float sensitivity
) {
    float mag = fabsf(event->magnitude);
    float surprise = event->surprise_factor;
    float delta = mag * sensitivity;

    switch (event->event_type) {
        case FIN_MKT_EVENT_PRICE_INCREASE:
            if (event->magnitude > 0) {
                /* Long position: joy, trust increase */
                state->joy = nimcp_clampf(state->joy + delta * 0.5f, 0.0f, 1.0f);
                state->trust = nimcp_clampf(state->trust + delta * 0.3f, 0.0f, 1.0f);
                state->anticipation = nimcp_clampf(state->anticipation + delta * 0.2f, 0.0f, 1.0f);
            } else {
                /* Short position: fear, sadness increase */
                state->fear = nimcp_clampf(state->fear + delta * 0.4f, 0.0f, 1.0f);
                state->sadness = nimcp_clampf(state->sadness + delta * 0.3f, 0.0f, 1.0f);
            }
            state->surprise = nimcp_clampf(state->surprise + surprise * delta * 0.5f, 0.0f, 1.0f);
            break;

        case FIN_MKT_EVENT_PRICE_DECREASE:
            if (event->magnitude < 0) {
                /* Long position loss: fear, sadness, anger */
                state->fear = nimcp_clampf(state->fear + delta * 0.5f, 0.0f, 1.0f);
                state->sadness = nimcp_clampf(state->sadness + delta * 0.3f, 0.0f, 1.0f);
                state->anger = nimcp_clampf(state->anger + delta * 0.2f, 0.0f, 1.0f);
            } else {
                /* Short position gain: joy */
                state->joy = nimcp_clampf(state->joy + delta * 0.4f, 0.0f, 1.0f);
            }
            state->surprise = nimcp_clampf(state->surprise + surprise * delta * 0.5f, 0.0f, 1.0f);
            break;

        case FIN_MKT_EVENT_VOLUME_SPIKE:
            /* Increased anticipation and surprise */
            state->anticipation = nimcp_clampf(state->anticipation + delta * 0.4f, 0.0f, 1.0f);
            state->surprise = nimcp_clampf(state->surprise + surprise * delta * 0.6f, 0.0f, 1.0f);
            state->fear = nimcp_clampf(state->fear + delta * 0.2f, 0.0f, 1.0f);
            break;

        case FIN_MKT_EVENT_VOLATILITY_SPIKE:
            /* Fear and anticipation increase significantly */
            state->fear = nimcp_clampf(state->fear + delta * 0.5f, 0.0f, 1.0f);
            state->anticipation = nimcp_clampf(state->anticipation + delta * 0.3f, 0.0f, 1.0f);
            state->surprise = nimcp_clampf(state->surprise + surprise * delta * 0.4f, 0.0f, 1.0f);
            /* Trust decreases */
            state->trust = nimcp_clampf(state->trust - delta * 0.2f, 0.0f, 1.0f);
            break;

        case FIN_MKT_EVENT_STOP_LOSS_HIT:
            /* Strong negative emotions */
            state->sadness = nimcp_clampf(state->sadness + delta * 0.6f, 0.0f, 1.0f);
            state->anger = nimcp_clampf(state->anger + delta * 0.4f, 0.0f, 1.0f);
            state->fear = nimcp_clampf(state->fear + delta * 0.3f, 0.0f, 1.0f);
            state->disgust = nimcp_clampf(state->disgust + delta * 0.2f, 0.0f, 1.0f);
            /* Trust and joy decrease */
            state->trust = nimcp_clampf(state->trust - delta * 0.4f, 0.0f, 1.0f);
            state->joy = nimcp_clampf(state->joy - delta * 0.5f, 0.0f, 1.0f);
            break;

        case FIN_MKT_EVENT_PROFIT_TARGET_HIT:
            /* Strong positive emotions */
            state->joy = nimcp_clampf(state->joy + delta * 0.7f, 0.0f, 1.0f);
            state->trust = nimcp_clampf(state->trust + delta * 0.4f, 0.0f, 1.0f);
            state->anticipation = nimcp_clampf(state->anticipation + delta * 0.3f, 0.0f, 1.0f);
            /* Fear and sadness decrease */
            state->fear = nimcp_clampf(state->fear - delta * 0.3f, 0.0f, 1.0f);
            state->sadness = nimcp_clampf(state->sadness - delta * 0.3f, 0.0f, 1.0f);
            break;

        case FIN_MKT_EVENT_NEWS_POSITIVE:
            state->joy = nimcp_clampf(state->joy + delta * 0.4f, 0.0f, 1.0f);
            state->trust = nimcp_clampf(state->trust + delta * 0.3f, 0.0f, 1.0f);
            state->anticipation = nimcp_clampf(state->anticipation + delta * 0.4f, 0.0f, 1.0f);
            state->surprise = nimcp_clampf(state->surprise + surprise * delta * 0.5f, 0.0f, 1.0f);
            break;

        case FIN_MKT_EVENT_NEWS_NEGATIVE:
            state->fear = nimcp_clampf(state->fear + delta * 0.4f, 0.0f, 1.0f);
            state->sadness = nimcp_clampf(state->sadness + delta * 0.3f, 0.0f, 1.0f);
            state->anger = nimcp_clampf(state->anger + delta * 0.2f, 0.0f, 1.0f);
            state->surprise = nimcp_clampf(state->surprise + surprise * delta * 0.5f, 0.0f, 1.0f);
            state->trust = nimcp_clampf(state->trust - delta * 0.3f, 0.0f, 1.0f);
            break;

        case FIN_MKT_EVENT_REGIME_CHANGE:
            /* High surprise and anticipation, moderate fear */
            state->surprise = nimcp_clampf(state->surprise + delta * 0.7f, 0.0f, 1.0f);
            state->anticipation = nimcp_clampf(state->anticipation + delta * 0.5f, 0.0f, 1.0f);
            state->fear = nimcp_clampf(state->fear + delta * 0.3f, 0.0f, 1.0f);
            break;

        case FIN_MKT_EVENT_MISSED_OPPORTUNITY:
            /* FOMO trigger: fear, anticipation, regret components */
            state->fear = nimcp_clampf(state->fear + delta * 0.4f, 0.0f, 1.0f);
            state->anticipation = nimcp_clampf(state->anticipation + delta * 0.5f, 0.0f, 1.0f);
            state->sadness = nimcp_clampf(state->sadness + delta * 0.4f, 0.0f, 1.0f);
            state->anger = nimcp_clampf(state->anger + delta * 0.2f, 0.0f, 1.0f);
            break;

        default:
            break;
    }

    /* Update appraisal dimensions based on event */
    state->relevance = nimcp_clampf(state->relevance + mag * 0.3f, 0.0f, 1.0f);

    /* Certainty decreases with surprise, increases with expected outcomes */
    if (surprise > 0.5f) {
        state->certainty = nimcp_clampf(state->certainty - surprise * 0.2f, 0.0f, 1.0f);
    }

    /* Control decreases with volatility and unexpected events */
    if (event->event_type == FIN_MKT_EVENT_VOLATILITY_SPIKE ||
        event->event_type == FIN_MKT_EVENT_STOP_LOSS_HIT) {
        state->control = nimcp_clampf(state->control - delta * 0.3f, 0.0f, 1.0f);
    } else if (event->event_type == FIN_MKT_EVENT_PROFIT_TARGET_HIT) {
        state->control = nimcp_clampf(state->control + delta * 0.2f, 0.0f, 1.0f);
    }
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

int financial_emotion_bridge_default_config(fin_emotion_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");
        return -1;
    }

    fin_emotion_heartbeat("fin_emotion_default_config", 0.0f);

    memset(config, 0, sizeof(*config));

    /* Emotion dynamics */
    config->decay_rate = 0.05f;         /* 5% decay per second */
    config->sensitivity = 1.0f;         /* Normal sensitivity */
    config->baseline_mood = 0.5f;       /* Neutral baseline */

    /* Thresholds for bias detection */
    config->fomo_threshold = 0.6f;
    config->panic_threshold = 0.7f;
    config->greed_threshold = 0.65f;
    config->overconfidence_threshold = 0.7f;

    /* Decision modulation settings */
    config->max_risk_scale = 1.5f;
    config->min_risk_scale = 0.3f;
    config->urgency_threshold = 0.5f;
    config->enable_pause_suggestion = true;

    /* Integration settings */
    config->enable_immune_integration = true;
    config->enable_bbb_validation = true;
    config->enable_kg_messaging = true;
    config->enable_health_monitoring = true;

    /* Logging */
    config->verbose_logging = false;

    fin_emotion_heartbeat("fin_emotion_default_config", 1.0f);
    return 0;
}

financial_emotion_bridge_t* financial_emotion_bridge_create(
    const fin_emotion_config_t* config
) {
    fin_emotion_heartbeat("fin_emotion_create", 0.0f);

    financial_emotion_bridge_t* bridge = nimcp_calloc(1, sizeof(financial_emotion_bridge_t));
    if (!bridge) {
        set_error("Failed to allocate financial_emotion_bridge");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NO_MEMORY, "Failed to allocate financial_emotion_bridge");
        return NULL;
    }

    bridge->magic = FINANCIAL_EMOTION_BRIDGE_MAGIC;
    bridge->state = FIN_EMOTION_STATE_UNINITIALIZED;

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        financial_emotion_bridge_default_config(&bridge->config);
    }

    /* Initialize bridge base (creates mutex) */
    if (bridge_base_init(&bridge->base, BIO_MODULE_FINANCIAL_EMOTION, "financial_emotion") != 0) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "financial_emotion_bridge_create: validation failed");
        return NULL;
    }

    /* Initialize emotional state to baseline */
    memset(&bridge->current_state, 0, sizeof(bridge->current_state));
    bridge->current_state.certainty = 0.5f;
    bridge->current_state.control = 0.5f;
    bridge->current_state.relevance = 0.0f;

    bridge->last_update_ms = nimcp_time_get_ms();

    /* Initialize statistics */
    memset(&bridge->stats, 0, sizeof(bridge->stats));

    bridge->state = FIN_EMOTION_STATE_INITIALIZED;

    fin_emotion_heartbeat("fin_emotion_create", 1.0f);
    return bridge;
}

void financial_emotion_bridge_destroy(financial_emotion_bridge_t* bridge) {
    if (!bridge || bridge->magic != FINANCIAL_EMOTION_BRIDGE_MAGIC) {
        return;
    }

    fin_emotion_heartbeat("fin_emotion_destroy", 0.0f);

    /* Cleanup base */
    bridge_base_cleanup(&bridge->base);

    bridge->magic = 0;
    nimcp_free(bridge);
    bridge = NULL;

    fin_emotion_heartbeat("fin_emotion_destroy", 1.0f);
}

int financial_emotion_bridge_reset(financial_emotion_bridge_t* bridge) {
    if (!bridge || bridge->magic != FINANCIAL_EMOTION_BRIDGE_MAGIC) {
        set_error("bridge is NULL or invalid");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_emotion_bridge_reset: invalid bridge");
        return FIN_EMOTION_ERR_NULL;
    }

    fin_emotion_heartbeat("fin_emotion_reset", 0.0f);

    nimcp_mutex_lock(bridge->base.mutex);

    /* Reset emotional state to baseline */
    memset(&bridge->current_state, 0, sizeof(bridge->current_state));
    bridge->current_state.certainty = 0.5f;
    bridge->current_state.control = 0.5f;
    bridge->current_state.relevance = 0.0f;

    bridge->last_update_ms = nimcp_time_get_ms();

    /* Reset stats */
    memset(&bridge->stats, 0, sizeof(bridge->stats));

    bridge->state = FIN_EMOTION_STATE_INITIALIZED;

    nimcp_mutex_unlock(bridge->base.mutex);

    fin_emotion_heartbeat("fin_emotion_reset", 1.0f);
    return FIN_EMOTION_ERR_OK;
}

/* ============================================================================
 * Subsystem Setters
 * ============================================================================ */

#define FIN_EMOTION_SETTER(name, field) \
    int financial_emotion_bridge_set_##name(financial_emotion_bridge_t* bridge, void* ptr) { \
        if (!bridge || bridge->magic != FINANCIAL_EMOTION_BRIDGE_MAGIC) { \
            set_error("bridge is NULL in set_" #name); \
            NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_emotion_bridge_set_" #name ": bridge is NULL"); \
            return FIN_EMOTION_ERR_NULL; \
        } \
        nimcp_mutex_lock(bridge->base.mutex); \
        bridge->field = ptr; \
        nimcp_mutex_unlock(bridge->base.mutex); \
        return FIN_EMOTION_ERR_OK; \
    }

FIN_EMOTION_SETTER(immune,        immune)
FIN_EMOTION_SETTER(health_agent,  health_agent)
FIN_EMOTION_SETTER(kg_wiring,     kg_wiring)
FIN_EMOTION_SETTER(logger,        logger)
FIN_EMOTION_SETTER(security,      security)
FIN_EMOTION_SETTER(bio_router,    bio_router)

/* Security setters for bbb, ethics, lgss, coordinator handled by bridge_base */

/* ============================================================================
 * Core Emotion API Implementation
 * ============================================================================ */

int financial_emotion_bridge_update(
    financial_emotion_bridge_t* bridge,
    const fin_market_event_t* event
) {
    if (!bridge || bridge->magic != FINANCIAL_EMOTION_BRIDGE_MAGIC) {
        set_error("bridge is NULL or invalid");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_emotion_bridge_update: invalid bridge");
        return FIN_EMOTION_ERR_NULL;
    }
    if (!event) {
        set_error("event is NULL");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_emotion_bridge_update: event is NULL");
        return FIN_EMOTION_ERR_NULL;
    }

    fin_emotion_heartbeat("fin_emotion_update", 0.0f);

    /* BBB validation */
    if (bridge->config.enable_bbb_validation && bridge->bbb) {
        BRIDGE_BBB_VALIDATE(bridge, event, sizeof(*event));
        bridge->stats.bbb_validations++;
    }

    /* Immune check */
    if (bridge->config.enable_immune_integration && bridge->immune) {
        bridge->stats.immune_checks++;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Apply time-based decay since last update */
    uint64_t now = nimcp_time_get_ms();
    uint64_t elapsed = now - bridge->last_update_ms;
    if (elapsed > 0 && bridge->config.decay_rate > 0.0f) {
        float decay = 1.0f - bridge->config.decay_rate * (float)elapsed / 1000.0f;
        decay = nimcp_clampf(decay, 0.0f, 1.0f);

        fin_emotion_state_t* s = &bridge->current_state;
        s->joy *= decay;
        s->sadness *= decay;
        s->anger *= decay;
        s->fear *= decay;
        s->surprise *= decay;
        s->disgust *= decay;
        s->trust = bridge->config.baseline_mood + (s->trust - bridge->config.baseline_mood) * decay;
        s->anticipation *= decay;
    }

    /* Apply event appraisal */
    apply_event_appraisal(&bridge->current_state, event, bridge->config.sensitivity);

    /* Update compound emotions */
    update_compound_emotions(&bridge->current_state);

    bridge->last_update_ms = now;
    bridge->stats.updates++;
    bridge->state = FIN_EMOTION_STATE_ACTIVE;

    nimcp_mutex_unlock(bridge->base.mutex);

    /* KG notification */
    bridge_kg_publish(bridge, KG_MSG_FIN_EMOTION_UPDATE, event, sizeof(*event));

    fin_emotion_heartbeat("fin_emotion_update", 1.0f);
    bridge->stats.health_heartbeats++;

    return FIN_EMOTION_ERR_OK;
}

int financial_emotion_bridge_get_state(
    const financial_emotion_bridge_t* bridge,
    fin_emotion_state_t* state
) {
    if (!bridge || bridge->magic != FINANCIAL_EMOTION_BRIDGE_MAGIC || !state) {
        set_error("NULL argument in get_state");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "get_state: NULL argument");
        return FIN_EMOTION_ERR_NULL;
    }

    fin_emotion_heartbeat("fin_emotion_get_state", 0.0f);

    nimcp_mutex_lock(bridge->base.mutex);
    *state = bridge->current_state;
    nimcp_mutex_unlock(bridge->base.mutex);

    return FIN_EMOTION_ERR_OK;
}

int financial_emotion_bridge_get_dominant(
    const financial_emotion_bridge_t* bridge,
    fin_dominant_result_t* result
) {
    if (!bridge || bridge->magic != FINANCIAL_EMOTION_BRIDGE_MAGIC || !result) {
        set_error("NULL argument in get_dominant");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "get_dominant: NULL argument");
        return FIN_EMOTION_ERR_NULL;
    }

    fin_emotion_heartbeat("fin_emotion_get_dominant", 0.0f);

    nimcp_mutex_lock(bridge->base.mutex);
    const fin_emotion_state_t* s = &bridge->current_state;

    /* Find max among primary emotions */
    float primary_values[FIN_PRIMARY_COUNT] = {
        s->joy, s->sadness, s->anger, s->fear,
        s->surprise, s->disgust, s->trust, s->anticipation
    };

    float max_primary = 0.0f;
    int max_primary_idx = 0;
    for (int i = 0; i < FIN_PRIMARY_COUNT; i++) {
        if (primary_values[i] > max_primary) {
            max_primary = primary_values[i];
            max_primary_idx = i;
        }
    }

    /* Find max among compound emotions */
    float compound_values[FIN_COMPOUND_COUNT] = {
        s->greed, s->panic, s->fomo, s->euphoria, s->anxiety, s->regret
    };

    float max_compound = 0.0f;
    int max_compound_idx = 0;
    for (int i = 0; i < FIN_COMPOUND_COUNT; i++) {
        if (compound_values[i] > max_compound) {
            max_compound = compound_values[i];
            max_compound_idx = i;
        }
    }

    /* Determine which is dominant */
    if (max_primary < 0.1f && max_compound < 0.1f) {
        /* No significant emotion */
        result->dominant = FIN_DOMINANT_NEUTRAL;
        result->intensity = 0.0f;
        result->is_compound = false;
    } else if (max_compound > max_primary) {
        /* Compound emotion dominates */
        result->is_compound = true;
        result->intensity = max_compound;
        /* Map compound index to dominant enum (compounds start at FIN_DOMINANT_GREED = 9) */
        result->dominant = (fin_dominant_emotion_t)(FIN_DOMINANT_GREED + max_compound_idx);
    } else {
        /* Primary emotion dominates */
        result->is_compound = false;
        result->intensity = max_primary;
        /* Map primary index to dominant enum (primaries start at FIN_DOMINANT_JOY = 1) */
        result->dominant = (fin_dominant_emotion_t)(FIN_DOMINANT_JOY + max_primary_idx);
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return FIN_EMOTION_ERR_OK;
}

int financial_emotion_bridge_decay(
    financial_emotion_bridge_t* bridge,
    uint64_t elapsed_ms
) {
    if (!bridge || bridge->magic != FINANCIAL_EMOTION_BRIDGE_MAGIC) {
        set_error("bridge is NULL or invalid");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_emotion_bridge_decay: invalid bridge");
        return FIN_EMOTION_ERR_NULL;
    }

    if (elapsed_ms == 0 || bridge->config.decay_rate <= 0.0f) {
        return FIN_EMOTION_ERR_OK;
    }

    fin_emotion_heartbeat("fin_emotion_decay", 0.0f);

    nimcp_mutex_lock(bridge->base.mutex);

    float decay = 1.0f - bridge->config.decay_rate * (float)elapsed_ms / 1000.0f;
    decay = nimcp_clampf(decay, 0.0f, 1.0f);

    fin_emotion_state_t* s = &bridge->current_state;

    /* Decay primary emotions toward zero (except trust toward baseline) */
    s->joy *= decay;
    s->sadness *= decay;
    s->anger *= decay;
    s->fear *= decay;
    s->surprise *= decay;
    s->disgust *= decay;
    s->trust = bridge->config.baseline_mood + (s->trust - bridge->config.baseline_mood) * decay;
    s->anticipation *= decay;

    /* Decay appraisal dimensions toward neutral */
    s->certainty = 0.5f + (s->certainty - 0.5f) * decay;
    s->control = 0.5f + (s->control - 0.5f) * decay;
    s->relevance *= decay;

    /* Update compound emotions */
    update_compound_emotions(s);

    bridge->last_update_ms = nimcp_time_get_ms();

    nimcp_mutex_unlock(bridge->base.mutex);

    fin_emotion_heartbeat("fin_emotion_decay", 1.0f);

    return FIN_EMOTION_ERR_OK;
}

/* ============================================================================
 * Decision Modulation Implementation
 * ============================================================================ */

int financial_emotion_bridge_modulate_decision(
    const financial_emotion_bridge_t* bridge,
    fin_decision_modulation_t* modulation
) {
    if (!bridge || bridge->magic != FINANCIAL_EMOTION_BRIDGE_MAGIC || !modulation) {
        set_error("NULL argument in modulate_decision");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "modulate_decision: NULL argument");
        return FIN_EMOTION_ERR_NULL;
    }

    fin_emotion_heartbeat("fin_emotion_modulate", 0.0f);

    nimcp_mutex_lock(bridge->base.mutex);
    const fin_emotion_state_t* s = &bridge->current_state;

    memset(modulation, 0, sizeof(*modulation));

    /* Start with neutral modulation */
    modulation->risk_tolerance_scale = 1.0f;
    modulation->position_size_scale = 1.0f;
    modulation->stop_loss_scale = 1.0f;
    modulation->take_profit_scale = 1.0f;
    modulation->urgency_dampening = 0.0f;
    modulation->suggest_pause = false;

    /* Calculate overall emotional intensity */
    float total_negative = s->fear + s->anger + s->sadness + s->panic + s->anxiety + s->regret;
    float total_positive = s->joy + s->trust + s->euphoria + s->anticipation;
    float arousal = total_negative + total_positive + s->surprise;

    /* Fear/Panic: Reduce risk tolerance */
    if (s->fear > 0.3f || s->panic > 0.3f) {
        float fear_factor = maxf(s->fear, s->panic);
        modulation->risk_tolerance_scale *= (1.0f - fear_factor * 0.5f);
        modulation->position_size_scale *= (1.0f - fear_factor * 0.4f);
        modulation->stop_loss_scale *= (1.0f + fear_factor * 0.3f);  /* Wider stops when fearful to avoid whipsaws */
    }

    /* Greed/Euphoria: Reduce position size, tighten stops */
    if (s->greed > 0.4f || s->euphoria > 0.4f) {
        float greed_factor = maxf(s->greed, s->euphoria);
        modulation->position_size_scale *= (1.0f - greed_factor * 0.3f);
        modulation->stop_loss_scale *= (1.0f - greed_factor * 0.2f);  /* Tighter stops when greedy */
        modulation->take_profit_scale *= (1.0f - greed_factor * 0.2f); /* Earlier profit taking */
    }

    /* FOMO: Significantly reduce position size, increase urgency dampening */
    if (s->fomo > 0.4f) {
        modulation->position_size_scale *= (1.0f - s->fomo * 0.5f);
        modulation->urgency_dampening = maxf(modulation->urgency_dampening, s->fomo * 0.7f);
    }

    /* Anger/Regret (revenge trading risk): Reduce risk, suggest pause */
    if (s->anger > 0.4f || s->regret > 0.4f) {
        float revenge_factor = maxf(s->anger, s->regret);
        modulation->risk_tolerance_scale *= (1.0f - revenge_factor * 0.4f);
        modulation->position_size_scale *= (1.0f - revenge_factor * 0.5f);
        modulation->urgency_dampening = maxf(modulation->urgency_dampening, revenge_factor * 0.6f);
    }

    /* High overall arousal: Dampen urgency */
    if (arousal > 2.0f) {
        modulation->urgency_dampening = maxf(modulation->urgency_dampening, (arousal - 2.0f) * 0.3f);
    }

    /* Clamp all values to configured limits */
    modulation->risk_tolerance_scale = nimcp_clampf(modulation->risk_tolerance_scale,
                                               bridge->config.min_risk_scale,
                                               bridge->config.max_risk_scale);
    modulation->position_size_scale = nimcp_clampf(modulation->position_size_scale, 0.1f, 2.0f);
    modulation->stop_loss_scale = nimcp_clampf(modulation->stop_loss_scale, 0.5f, 2.0f);
    modulation->take_profit_scale = nimcp_clampf(modulation->take_profit_scale, 0.5f, 2.0f);
    modulation->urgency_dampening = nimcp_clampf(modulation->urgency_dampening, 0.0f, 1.0f);

    /* Determine if pause should be suggested */
    if (bridge->config.enable_pause_suggestion) {
        if (modulation->urgency_dampening > bridge->config.urgency_threshold ||
            s->panic > bridge->config.panic_threshold ||
            (s->anger + s->regret) > 1.0f) {
            modulation->suggest_pause = true;
        }
    }

    /* Generate reason string */
    if (modulation->suggest_pause) {
        if (s->panic > bridge->config.panic_threshold) {
            snprintf(modulation->reason, sizeof(modulation->reason),
                     "High panic (%.0f%%) detected. Consider stepping away.", s->panic * 100.0f);
        } else if (s->anger > 0.5f || s->regret > 0.5f) {
            snprintf(modulation->reason, sizeof(modulation->reason),
                     "Revenge trading risk detected. Pause recommended.");
        } else {
            snprintf(modulation->reason, sizeof(modulation->reason),
                     "Emotional state elevated. Slow down decision-making.");
        }
    } else if (modulation->risk_tolerance_scale < 0.7f) {
        snprintf(modulation->reason, sizeof(modulation->reason),
                 "Risk reduced due to elevated %s.", s->fear > s->greed ? "fear" : "euphoria");
    } else if (modulation->position_size_scale < 0.7f) {
        snprintf(modulation->reason, sizeof(modulation->reason),
                 "Position size reduced due to FOMO/greed indicators.");
    } else {
        snprintf(modulation->reason, sizeof(modulation->reason),
                 "Emotional state within normal range.");
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Update stats */
    ((financial_emotion_bridge_t*)bridge)->stats.modulations++;

    /* KG notification */
    bridge_kg_publish((financial_emotion_bridge_t*)bridge, KG_MSG_FIN_EMOTION_MODULATE, modulation, sizeof(*modulation));

    fin_emotion_heartbeat("fin_emotion_modulate", 1.0f);
    ((financial_emotion_bridge_t*)bridge)->stats.health_heartbeats++;

    return FIN_EMOTION_ERR_OK;
}

/* ============================================================================
 * Bias Detection Implementation
 * ============================================================================ */

int financial_emotion_bridge_detect_bias(
    const financial_emotion_bridge_t* bridge,
    fin_bias_detection_t* detection
) {
    if (!bridge || bridge->magic != FINANCIAL_EMOTION_BRIDGE_MAGIC || !detection) {
        set_error("NULL argument in detect_bias");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "detect_bias: NULL argument");
        return FIN_EMOTION_ERR_NULL;
    }

    fin_emotion_heartbeat("fin_emotion_detect_bias", 0.0f);

    nimcp_mutex_lock(bridge->base.mutex);
    const fin_emotion_state_t* s = &bridge->current_state;
    const fin_emotion_config_t* cfg = &bridge->config;

    memset(detection, 0, sizeof(*detection));
    detection->bias = FIN_BIAS_NONE;
    detection->severity = 0.0f;
    detection->confidence = 0.0f;
    detection->action_recommended = false;

    /* Check for various biases in priority order */

    /* 1. Panic Selling: High fear + panic, low control */
    if (s->panic > cfg->panic_threshold || (s->fear > cfg->panic_threshold && s->control < 0.3f)) {
        float severity = maxf(s->panic, s->fear);
        if (severity > detection->severity) {
            detection->bias = FIN_BIAS_PANIC_SELLING;
            detection->severity = severity;
            detection->confidence = nimcp_clampf(severity + (1.0f - s->control) * 0.3f, 0.0f, 1.0f);
            detection->action_recommended = true;
            snprintf(detection->description, sizeof(detection->description),
                     "Panic selling risk detected (panic=%.0f%%, fear=%.0f%%, control=%.0f%%). "
                     "Avoid impulsive sell decisions. Consider waiting 24h before acting.",
                     s->panic * 100.0f, s->fear * 100.0f, s->control * 100.0f);
        }
    }

    /* 2. FOMO: High fomo, often after missed opportunity */
    if (s->fomo > cfg->fomo_threshold) {
        if (s->fomo > detection->severity) {
            detection->bias = FIN_BIAS_FOMO;
            detection->severity = s->fomo;
            detection->confidence = nimcp_clampf(s->fomo + s->anticipation * 0.2f, 0.0f, 1.0f);
            detection->action_recommended = true;
            snprintf(detection->description, sizeof(detection->description),
                     "FOMO detected (%.0f%%). Fear of missing out can lead to poor entry timing. "
                     "Wait for better setup or reduce position size.",
                     s->fomo * 100.0f);
        }
    }

    /* 3. Greed/Overtrading: High greed with high anticipation */
    if (s->greed > cfg->greed_threshold && s->anticipation > 0.5f) {
        float severity = (s->greed + s->anticipation) / 2.0f;
        if (severity > detection->severity) {
            detection->bias = FIN_BIAS_GREED_OVERTRADING;
            detection->severity = severity;
            detection->confidence = nimcp_clampf(severity, 0.0f, 1.0f);
            detection->action_recommended = true;
            snprintf(detection->description, sizeof(detection->description),
                     "Greed-driven overtrading risk (greed=%.0f%%, anticipation=%.0f%%). "
                     "Stick to trading plan. Consider smaller positions.",
                     s->greed * 100.0f, s->anticipation * 100.0f);
        }
    }

    /* 4. Loss Aversion: High fear without actual panic, reluctance to exit losers */
    if (s->fear > 0.5f && s->panic < 0.3f && s->sadness > 0.3f) {
        float severity = (s->fear + s->sadness) / 2.0f;
        if (severity > detection->severity) {
            detection->bias = FIN_BIAS_LOSS_AVERSION;
            detection->severity = severity;
            detection->confidence = nimcp_clampf(severity, 0.0f, 1.0f);
            detection->action_recommended = s->fear > 0.6f;
            snprintf(detection->description, sizeof(detection->description),
                     "Loss aversion detected (fear=%.0f%%, sadness=%.0f%%). "
                     "May be holding losers too long. Review positions objectively.",
                     s->fear * 100.0f, s->sadness * 100.0f);
        }
    }

    /* 5. Overconfidence: High trust + joy + euphoria */
    float overconfidence = (s->trust + s->joy + s->euphoria) / 3.0f;
    if (overconfidence > cfg->overconfidence_threshold) {
        if (overconfidence > detection->severity) {
            detection->bias = FIN_BIAS_OVERCONFIDENCE;
            detection->severity = overconfidence;
            detection->confidence = nimcp_clampf(overconfidence, 0.0f, 1.0f);
            detection->action_recommended = overconfidence > 0.8f;
            snprintf(detection->description, sizeof(detection->description),
                     "Overconfidence detected (trust=%.0f%%, joy=%.0f%%, euphoria=%.0f%%). "
                     "Review position sizing. Consider taking partial profits.",
                     s->trust * 100.0f, s->joy * 100.0f, s->euphoria * 100.0f);
        }
    }

    /* 6. Revenge Trading: High anger + regret after loss */
    if (s->anger > 0.5f && s->regret > 0.4f) {
        float severity = (s->anger + s->regret) / 2.0f;
        if (severity > detection->severity) {
            detection->bias = FIN_BIAS_REVENGE_TRADING;
            detection->severity = severity;
            detection->confidence = nimcp_clampf(severity + 0.1f, 0.0f, 1.0f);
            detection->action_recommended = true;
            snprintf(detection->description, sizeof(detection->description),
                     "Revenge trading risk (anger=%.0f%%, regret=%.0f%%). "
                     "STOP trading for today. Process the loss before continuing.",
                     s->anger * 100.0f, s->regret * 100.0f);
        }
    }

    /* If no bias detected */
    if (detection->bias == FIN_BIAS_NONE) {
        detection->confidence = 0.8f;  /* Confident there's no bias */
        snprintf(detection->description, sizeof(detection->description),
                 "No significant emotional bias detected. Emotional state is balanced.");
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Update stats */
    ((financial_emotion_bridge_t*)bridge)->stats.bias_detections++;

    /* KG notification if bias detected */
    if (detection->bias != FIN_BIAS_NONE) {
        bridge_kg_publish((financial_emotion_bridge_t*)bridge, KG_MSG_FIN_EMOTION_BIAS, detection, sizeof(*detection));
    }

    fin_emotion_heartbeat("fin_emotion_detect_bias", 1.0f);
    ((financial_emotion_bridge_t*)bridge)->stats.health_heartbeats++;

    return FIN_EMOTION_ERR_OK;
}

/* ============================================================================
 * Query Implementation
 * ============================================================================ */

fin_emotion_bridge_state_t financial_emotion_bridge_get_bridge_state(
    const financial_emotion_bridge_t* bridge
) {
    if (!bridge || bridge->magic != FINANCIAL_EMOTION_BRIDGE_MAGIC) {
        return FIN_EMOTION_STATE_ERROR;
    }
    fin_emotion_heartbeat("fin_emotion_get_bridge_state", 0.0f);
    return bridge->state;
}

int financial_emotion_bridge_get_stats(
    const financial_emotion_bridge_t* bridge,
    fin_emotion_bridge_stats_t* stats
) {
    if (!bridge || bridge->magic != FINANCIAL_EMOTION_BRIDGE_MAGIC || !stats) {
        set_error("NULL argument in get_stats");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "get_stats: NULL argument");
        return FIN_EMOTION_ERR_NULL;
    }

    fin_emotion_heartbeat("fin_emotion_get_stats", 0.0f);

    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);

    return FIN_EMOTION_ERR_OK;
}

void financial_emotion_bridge_reset_stats(financial_emotion_bridge_t* bridge) {
    if (!bridge || bridge->magic != FINANCIAL_EMOTION_BRIDGE_MAGIC) {
        return;
    }

    fin_emotion_heartbeat("fin_emotion_reset_stats", 0.0f);

    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    nimcp_mutex_unlock(bridge->base.mutex);
}

const char* financial_emotion_bridge_get_last_error(void) {
    return fin_emotion_last_error;
}

/* ============================================================================
 * Health Integration
 * ============================================================================ */

int financial_emotion_bridge_heartbeat(
    financial_emotion_bridge_t* bridge,
    const char* operation,
    float progress
) {
    if (!bridge || bridge->magic != FINANCIAL_EMOTION_BRIDGE_MAGIC) {
        return FIN_EMOTION_ERR_NULL;
    }

    /* Forward to global health agent */
    fin_emotion_heartbeat(operation ? operation : "fin_emotion_heartbeat", progress);

    /* Forward to instance-level health agent */
    if (bridge->health_agent) {
        nimcp_health_agent_heartbeat_ex(
            (nimcp_health_agent_t*)bridge->health_agent, operation, progress);
    }

    bridge->stats.health_heartbeats++;
    return FIN_EMOTION_ERR_OK;
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

const char* fin_emotion_primary_name(fin_primary_emotion_t emotion) {
    if (emotion >= FIN_PRIMARY_COUNT) {
        return "unknown";
    }
    return primary_emotion_names[emotion];
}

const char* fin_emotion_compound_name(fin_compound_emotion_t emotion) {
    if (emotion >= FIN_COMPOUND_COUNT) {
        return "unknown";
    }
    return compound_emotion_names[emotion];
}

const char* fin_emotion_dominant_name(fin_dominant_emotion_t emotion) {
    if (emotion >= FIN_DOMINANT_COUNT) {
        return "unknown";
    }
    return dominant_emotion_names[emotion];
}

const char* fin_emotion_bias_name(fin_emotional_bias_t bias) {
    if (bias >= FIN_BIAS_COUNT) {
        return "unknown";
    }
    return bias_names[bias];
}

const char* fin_emotion_event_name(fin_market_event_type_t event) {
    if (event >= FIN_MKT_EVENT_COUNT) {
        return "unknown";
    }
    return event_names[event];
}

const char* fin_emotion_state_name(fin_emotion_bridge_state_t state) {
    if (state > FIN_EMOTION_STATE_ERROR) {
        return "unknown";
    }
    return state_names[state];
}

const char* financial_emotion_bridge_version(void) {
    return FINANCIAL_EMOTION_BRIDGE_VERSION;
}

/* ============================================================================
 * Training Integration (B23 Upgrade Compatibility)
 * ============================================================================ */

int financial_emotion_bridge_training_begin(financial_emotion_bridge_t* bridge) {
    if (!bridge || bridge->magic != FINANCIAL_EMOTION_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "financial_emotion_bridge_training_begin: NULL argument");
        return -1;
    }
    fin_emotion_heartbeat_instance((nimcp_health_agent_t*)bridge->health_agent,
                                    "financial_emotion_bridge_training_begin", 0.0f);
    return 0;
}

int financial_emotion_bridge_training_end(financial_emotion_bridge_t* bridge) {
    if (!bridge || bridge->magic != FINANCIAL_EMOTION_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "financial_emotion_bridge_training_end: NULL argument");
        return -1;
    }
    fin_emotion_heartbeat_instance((nimcp_health_agent_t*)bridge->health_agent,
                                    "financial_emotion_bridge_training_end", 1.0f);
    return 0;
}

int financial_emotion_bridge_training_step(financial_emotion_bridge_t* bridge, float progress) {
    if (!bridge || bridge->magic != FINANCIAL_EMOTION_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "financial_emotion_bridge_training_step: NULL argument");
        return -1;
    }

    /* Safety gates: ethics + LGSS pre-check */
    BRIDGE_ETHICS_GATE(bridge, "financial_emotion_bridge_training_step");
    BRIDGE_LGSS_GATE(bridge, "financial_emotion_bridge_training_step");

    fin_emotion_heartbeat_instance((nimcp_health_agent_t*)bridge->health_agent,
                                    "financial_emotion_bridge_training_step", progress);

    /* Notify coordinator of update cycle completion */
    bridge_base_notify_coordinator_tick(&bridge->base, 0);
    return 0;
}
