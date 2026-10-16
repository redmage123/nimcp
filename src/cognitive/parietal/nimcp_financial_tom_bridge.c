/**
 * @file nimcp_financial_tom_bridge.c
 * @brief Financial Theory of Mind Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-29
 *
 * WHAT: Bridge for modeling investor mental states, predicting actions,
 *       detecting false beliefs, and aggregating market sentiment
 *
 * WHY:  Financial markets are driven by human cognition. Understanding
 *       what investors believe, desire, and intend enables prediction
 *       and anticipation of market behavior.
 *
 * HOW:  Each investor is modeled using BDI (Belief-Desire-Intention)
 *       framework combined with archetype-specific heuristics.
 *
 * @author NIMCP Development Team
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdarg.h>

#include "cognitive/parietal/nimcp_financial_tom_bridge.h"
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

#define LOG_MODULE "financial_tom"

/* ============================================================================
 * Health Agent Integration (Phase 8: System-Wide Health Integration)
 * ============================================================================ */
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/math/nimcp_math_helpers.h"

/* Health agent: using pre-existing custom implementation */
static nimcp_health_agent_t* g_financial_tom_bridge_health_agent = NULL;

BRIDGE_DEFINE_MESH_REGISTRATION(financial_tom_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


/** @brief Send heartbeat from financial_tom_bridge module */
static inline void fin_tom_heartbeat(const char* operation, float progress) {
    if (g_financial_tom_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_financial_tom_bridge_health_agent, operation, progress);
    }
}

/** @brief Send heartbeat from financial_tom_bridge module (instance-level) */
static inline void fin_tom_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_financial_tom_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_financial_tom_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_financial_tom_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}

/* ============================================================================
 * Thread-Local Error
 * ============================================================================ */

static _Thread_local char fin_tom_last_error[NIMCP_ERROR_BUFFER_SIZE] = {0};

static void set_error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(fin_tom_last_error, sizeof(fin_tom_last_error), fmt, args);
    va_end(args);
}

/* ============================================================================
 * KG Wiring Integration
 * ============================================================================ */

#define KG_MSG_FIN_TOM_MODEL_CREATED    "FIN_TOM_MODEL_CREATED"
#define KG_MSG_FIN_TOM_ACTION_PREDICTED "FIN_TOM_ACTION_PREDICTED"
#define KG_MSG_FIN_TOM_FALSE_BELIEF     "FIN_TOM_FALSE_BELIEF"
#define KG_MSG_FIN_TOM_SENTIMENT        "FIN_TOM_SENTIMENT"
#define KG_MSG_FIN_TOM_ERROR            "FIN_TOM_ERROR"

/* ============================================================================
 * Internal Structures
 * ============================================================================ */

/**
 * @brief Investor model node for hash map storage
 */
typedef struct model_node {
    fin_investor_model_t model;
    struct model_node* next;
} model_node_t;

/**
 * @brief Simple hash map bucket count
 */
#define MODEL_HASH_BUCKETS 64

/**
 * @brief Financial theory of mind bridge structure
 */
struct financial_tom_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    uint32_t magic;
    fin_tom_state_t state;

    /* Configuration */
    fin_tom_config_t config;

    /* Investor model storage (hash map) */
    model_node_t* model_buckets[MODEL_HASH_BUCKETS];
    uint32_t model_count;

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

    /* Callbacks */
    fin_tom_model_callback_t model_cb;
    void* model_cb_data;
    fin_tom_false_belief_callback_t false_belief_cb;
    void* false_belief_cb_data;

    /* Statistics */
    fin_tom_bridge_stats_t stats;

    /* Synchronization - use base.mutex */
};

/* Security integration via bridge_base */
BRIDGE_DEFINE_SECURITY_SETTERS(financial_tom_bridge)

/* ============================================================================
 * Static Name Tables
 * ============================================================================ */

static const char* archetype_names[] = {
    "Graham",
    "Buffett",
    "Soros",
    "Lynch",
    "Templeton",
    "Dalio",
    "Simons",
    "Fisher",
    "Munger",
    "Livermore"
};

static const char* emotion_names[] = {
    "neutral",
    "fear",
    "greed",
    "panic",
    "euphoria",
    "frustration",
    "confidence",
    "doubt"
};

static const char* action_names[] = {
    "strong_buy",
    "buy",
    "hold",
    "reduce",
    "sell",
    "short",
    "cover"
};

static const char* false_belief_names[] = {
    "none",
    "overvaluation",
    "undervaluation",
    "trend_continuation",
    "trend_reversal",
    "information_edge",
    "skill_attribution",
    "consensus_validity"
};

static const char* state_names[] = {
    "uninitialized",
    "initialized",
    "active",
    "degraded",
    "error"
};

/**
 * @brief Simple hash function for investor IDs
 */
static uint32_t hash_investor_id(const char* id) {
    uint32_t hash = 5381;
    int c = 0;
    while ((c = *id++)) {
        hash = ((hash << 5) + hash) + (uint32_t)c;
    }
    return hash % MODEL_HASH_BUCKETS;
}

/**
 * @brief Publish message through KG wiring
 */
static int bridge_kg_publish(financial_tom_bridge_t* bridge, const char* msg_type,
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
 * @brief Find model node by investor ID (unlocked)
 */
static model_node_t* find_model_unlocked(financial_tom_bridge_t* bridge, const char* investor_id) {
    uint32_t bucket = hash_investor_id(investor_id);
    model_node_t* node = bridge->model_buckets[bucket];
    while (node) {
        if (strncmp(node->model.investor_id, investor_id, FIN_TOM_INVESTOR_ID_LEN) == 0) {
            return node;
        }
        node = node->next;
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "find_model_unlocked: validation failed");
    return NULL;
}

/**
 * @brief Initialize default belief values based on archetype
 */
static void init_archetype_beliefs(fin_investor_model_t* model) {
    /* Initialize beliefs based on archetype tendencies */
    memset(model->beliefs, 0, sizeof(model->beliefs));
    memset(model->desires, 0, sizeof(model->desires));
    memset(model->intentions, 0, sizeof(model->intentions));

    switch (model->archetype) {
        case FIN_TOM_ARCHETYPE_GRAHAM:
            model->beliefs[0] = 0.8f;   /* Value focus */
            model->beliefs[1] = 0.9f;   /* Margin of safety */
            model->desires[0] = 0.9f;   /* Capital preservation */
            break;
        case FIN_TOM_ARCHETYPE_BUFFETT:
            model->beliefs[0] = 0.7f;   /* Quality matters */
            model->beliefs[1] = 0.8f;   /* Moat importance */
            model->desires[0] = 0.8f;   /* Long-term growth */
            break;
        case FIN_TOM_ARCHETYPE_SOROS:
            model->beliefs[0] = 0.6f;   /* Markets are reflexive */
            model->beliefs[1] = 0.7f;   /* Trends persist */
            model->desires[0] = 0.9f;   /* Asymmetric returns */
            break;
        case FIN_TOM_ARCHETYPE_LYNCH:
            model->beliefs[0] = 0.7f;   /* Growth at reasonable price */
            model->beliefs[1] = 0.6f;   /* Know what you own */
            model->desires[0] = 0.7f;   /* Beat the market */
            break;
        case FIN_TOM_ARCHETYPE_TEMPLETON:
            model->beliefs[0] = -0.5f;  /* Contrarian view */
            model->beliefs[1] = 0.8f;   /* Maximum pessimism = opportunity */
            model->desires[0] = 0.8f;   /* Global diversification */
            break;
        case FIN_TOM_ARCHETYPE_DALIO:
            model->beliefs[0] = 0.5f;   /* Balanced risk */
            model->beliefs[1] = 0.7f;   /* Diversification works */
            model->desires[0] = 0.9f;   /* Risk parity */
            break;
        case FIN_TOM_ARCHETYPE_SIMONS:
            model->beliefs[0] = 0.3f;   /* Statistical edge exists */
            model->beliefs[1] = 0.9f;   /* Quantitative signals */
            model->desires[0] = 0.8f;   /* Systematic returns */
            break;
        case FIN_TOM_ARCHETYPE_FISHER:
            model->beliefs[0] = 0.8f;   /* Management quality */
            model->beliefs[1] = 0.7f;   /* R&D investment */
            model->desires[0] = 0.9f;   /* Long-term growth */
            break;
        case FIN_TOM_ARCHETYPE_MUNGER:
            model->beliefs[0] = 0.6f;   /* Mental models help */
            model->beliefs[1] = 0.8f;   /* Inversion works */
            model->desires[0] = 0.7f;   /* Avoid mistakes */
            break;
        case FIN_TOM_ARCHETYPE_LIVERMORE:
            model->beliefs[0] = 0.7f;   /* Momentum matters */
            model->beliefs[1] = 0.8f;   /* Pivotal points */
            model->desires[0] = 0.9f;   /* Capture big moves */
            break;
        default:
            break;
    }
}

/**
 * @brief Predict action based on model and context
 */
static void compute_action_prediction(
    const fin_investor_model_t* model,
    const fin_tom_market_context_t* context,
    fin_tom_action_prediction_t* prediction
) {
    /* Initialize prediction */
    memset(prediction, 0, sizeof(*prediction));
    prediction->action = FIN_TOM_ACTION_HOLD;
    prediction->probability = 0.5f;
    prediction->conviction = model->confidence;
    prediction->position_size_pct = 0.0f;
    prediction->prediction_timestamp_ms = nimcp_time_get_ms();

    /* Combine beliefs with market context to determine action */
    float buy_signal = 0.0f;
    float sell_signal = 0.0f;

    /* Factor in emotional state */
    float emotional_bias = 0.0f;
    switch (model->emotional_state) {
        case FIN_TOM_EMOTION_FEAR:
        case FIN_TOM_EMOTION_PANIC:
            emotional_bias = -0.3f;
            break;
        case FIN_TOM_EMOTION_GREED:
        case FIN_TOM_EMOTION_EUPHORIA:
            emotional_bias = 0.3f;
            break;
        case FIN_TOM_EMOTION_CONFIDENCE:
            emotional_bias = 0.1f;
            break;
        case FIN_TOM_EMOTION_DOUBT:
            emotional_bias = -0.1f;
            break;
        default:
            break;
    }

    /* Archetype-specific signal generation */
    switch (model->archetype) {
        case FIN_TOM_ARCHETYPE_GRAHAM:
        case FIN_TOM_ARCHETYPE_BUFFETT:
            /* Value investors: negative price change = opportunity */
            if (context->price_change_pct < -0.05f) {
                buy_signal += 0.4f * model->beliefs[0];
            }
            if (context->price_change_pct > 0.2f) {
                sell_signal += 0.3f;
            }
            break;

        case FIN_TOM_ARCHETYPE_SOROS:
        case FIN_TOM_ARCHETYPE_LIVERMORE:
            /* Momentum: follow trend */
            if (context->is_trending && context->price_change_pct > 0) {
                buy_signal += 0.5f * model->beliefs[1];
            }
            if (context->is_trending && context->price_change_pct < 0) {
                sell_signal += 0.5f * model->beliefs[1];
            }
            break;

        case FIN_TOM_ARCHETYPE_TEMPLETON:
            /* Contrarian: buy fear, sell greed */
            if (context->market_fear_greed < 0.3f) {
                buy_signal += 0.6f * model->beliefs[1];
            }
            if (context->market_fear_greed > 0.7f) {
                sell_signal += 0.6f * model->beliefs[1];
            }
            break;

        case FIN_TOM_ARCHETYPE_DALIO:
            /* Risk parity: rebalance on volatility */
            if (context->volatility > 0.3f) {
                sell_signal += 0.2f;  /* Reduce exposure */
            }
            break;

        case FIN_TOM_ARCHETYPE_SIMONS:
            /* Quantitative: statistical signals */
            buy_signal += 0.3f * model->beliefs[1];
            sell_signal += 0.3f * (1.0f - model->beliefs[1]);
            break;

        default:
            /* Lynch, Fisher, Munger: balanced approach */
            if (context->price_change_pct < -0.1f && context->volume_ratio > 1.2f) {
                buy_signal += 0.3f;
            }
            break;
    }

    /* Apply emotional bias */
    buy_signal += emotional_bias;
    sell_signal -= emotional_bias;

    /* Clamp signals */
    buy_signal = nimcp_clampf(buy_signal, 0.0f, 1.0f);
    sell_signal = nimcp_clampf(sell_signal, 0.0f, 1.0f);

    /* Determine action */
    if (buy_signal > 0.7f && buy_signal > sell_signal + 0.3f) {
        prediction->action = FIN_TOM_ACTION_STRONG_BUY;
        prediction->probability = buy_signal;
        prediction->position_size_pct = 0.1f * model->confidence;
    } else if (buy_signal > 0.5f && buy_signal > sell_signal) {
        prediction->action = FIN_TOM_ACTION_BUY;
        prediction->probability = buy_signal;
        prediction->position_size_pct = 0.05f * model->confidence;
    } else if (sell_signal > 0.7f && sell_signal > buy_signal + 0.3f) {
        prediction->action = FIN_TOM_ACTION_SELL;
        prediction->probability = sell_signal;
        prediction->position_size_pct = 0.1f * model->confidence;
    } else if (sell_signal > 0.5f && sell_signal > buy_signal) {
        prediction->action = FIN_TOM_ACTION_REDUCE;
        prediction->probability = sell_signal;
        prediction->position_size_pct = 0.05f * model->confidence;
    } else {
        prediction->action = FIN_TOM_ACTION_HOLD;
        prediction->probability = 0.5f;
    }

    /* Generate rationale */
    snprintf(prediction->rationale, sizeof(prediction->rationale),
             "%s archetype: buy_signal=%.2f, sell_signal=%.2f, emotion=%s",
             archetype_names[model->archetype],
             buy_signal, sell_signal,
             emotion_names[model->emotional_state]);
}

/**
 * @brief Detect false beliefs in a single model
 */
static void detect_model_false_beliefs(
    const fin_investor_model_t* model,
    const fin_tom_market_context_t* context,
    fin_tom_false_belief_t* beliefs,
    uint32_t max_beliefs,
    uint32_t* count
) {
    *count = 0;

    /* Check for overvaluation belief when market is declining */
    if (model->beliefs[0] > 0.7f && context->price_change_pct < -0.1f) {
        if (*count < max_beliefs) {
            fin_tom_false_belief_t* b = &beliefs[*count];
            b->type = FIN_TOM_FALSE_BELIEF_UNDERVALUATION;
            b->severity = 0.5f + (model->beliefs[0] - 0.5f);
            b->confidence = 0.6f;
            snprintf(b->description, sizeof(b->description),
                     "Model believes asset is undervalued despite declining price");
            snprintf(b->evidence, sizeof(b->evidence),
                     "Price change: %.1f%%, Belief strength: %.2f",
                     context->price_change_pct * 100.0f, model->beliefs[0]);
            strncpy(b->investor_id, model->investor_id, FIN_TOM_INVESTOR_ID_LEN - 1);
            (*count)++;
        }
    }

    /* Check for trend continuation belief in ranging market */
    if (model->beliefs[1] > 0.6f && context->is_ranging && !context->is_trending) {
        if (*count < max_beliefs) {
            fin_tom_false_belief_t* b = &beliefs[*count];
            b->type = FIN_TOM_FALSE_BELIEF_TREND_CONTINUATION;
            b->severity = 0.4f;
            b->confidence = 0.5f;
            snprintf(b->description, sizeof(b->description),
                     "Model believes trend will continue in ranging market");
            snprintf(b->evidence, sizeof(b->evidence),
                     "Market is_ranging=true, trend belief=%.2f", model->beliefs[1]);
            strncpy(b->investor_id, model->investor_id, FIN_TOM_INVESTOR_ID_LEN - 1);
            (*count)++;
        }
    }

    /* Check for overconfidence (skill attribution) */
    if (model->confidence > 0.9f && model->emotional_state == FIN_TOM_EMOTION_EUPHORIA) {
        if (*count < max_beliefs) {
            fin_tom_false_belief_t* b = &beliefs[*count];
            b->type = FIN_TOM_FALSE_BELIEF_SKILL_ATTRIBUTION;
            b->severity = 0.7f;
            b->confidence = 0.7f;
            snprintf(b->description, sizeof(b->description),
                     "Model shows overconfidence potentially attributing luck to skill");
            snprintf(b->evidence, sizeof(b->evidence),
                     "Confidence: %.2f, Emotion: euphoria", model->confidence);
            strncpy(b->investor_id, model->investor_id, FIN_TOM_INVESTOR_ID_LEN - 1);
            (*count)++;
        }
    }
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

int financial_tom_bridge_default_config(fin_tom_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");
        return -1;
    }

    fin_tom_heartbeat("fin_tom_default_config", 0.0f);

    memset(config, 0, sizeof(*config));

    /* Model settings */
    config->max_models = FIN_TOM_MAX_MODELS;
    config->enable_archetype_weighting = true;
    config->default_confidence = 0.5f;

    /* Prediction settings */
    config->action_threshold = 0.5f;
    config->enable_rationale_generation = true;

    /* False belief detection */
    config->false_belief_threshold = 0.3f;
    config->enable_cross_model_detection = true;

    /* Sentiment aggregation */
    config->weight_by_confidence = true;
    config->weight_by_archetype_track_record = false;

    /* Integration settings */
    config->enable_immune_integration = true;
    config->enable_bbb_validation = true;
    config->enable_kg_messaging = true;
    config->enable_health_monitoring = true;

    /* Logging */
    config->verbose_logging = false;

    fin_tom_heartbeat("fin_tom_default_config", 1.0f);
    return 0;
}

financial_tom_bridge_t* financial_tom_bridge_create(
    const fin_tom_config_t* config
) {
    fin_tom_heartbeat("fin_tom_create", 0.0f);

    financial_tom_bridge_t* bridge = nimcp_calloc(1, sizeof(financial_tom_bridge_t));
    if (!bridge) {
        set_error("Failed to allocate financial_tom_bridge");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NO_MEMORY, "Failed to allocate financial_tom_bridge");
        return NULL;
    }

    bridge->magic = FINANCIAL_TOM_BRIDGE_MAGIC;
    bridge->state = FIN_TOM_STATE_UNINITIALIZED;

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        financial_tom_bridge_default_config(&bridge->config);
    }

    /* Initialize bridge base (creates mutex) */
    if (bridge_base_init(&bridge->base, BIO_MODULE_FINANCIAL_TOM, "financial_tom") != 0) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "financial_tom_bridge_create: validation failed");
        return NULL;
    }

    /* Initialize model hash map */
    memset(bridge->model_buckets, 0, sizeof(bridge->model_buckets));
    bridge->model_count = 0;

    /* Initialize statistics */
    memset(&bridge->stats, 0, sizeof(bridge->stats));

    bridge->state = FIN_TOM_STATE_INITIALIZED;

    fin_tom_heartbeat("fin_tom_create", 1.0f);
    return bridge;
}

void financial_tom_bridge_destroy(financial_tom_bridge_t* bridge) {
    if (!bridge || bridge->magic != FINANCIAL_TOM_BRIDGE_MAGIC) {
        return;
    }

    fin_tom_heartbeat("fin_tom_destroy", 0.0f);

    /* Free all model nodes */
    nimcp_mutex_lock(bridge->base.mutex);
    for (uint32_t i = 0; i < MODEL_HASH_BUCKETS; i++) {
        model_node_t* node = bridge->model_buckets[i];
        while (node) {
            model_node_t* next = node->next;
            nimcp_free(node);
            node = NULL;
            node = next;
        }
        bridge->model_buckets[i] = NULL;
    }
    bridge->model_count = 0;
    nimcp_mutex_unlock(bridge->base.mutex);

    /* Cleanup base */
    bridge_base_cleanup(&bridge->base);

    bridge->magic = 0;
    nimcp_free(bridge);
    bridge = NULL;
}

int financial_tom_bridge_reset(financial_tom_bridge_t* bridge) {
    if (!bridge || bridge->magic != FINANCIAL_TOM_BRIDGE_MAGIC) {
        set_error("bridge is NULL or invalid");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_tom_bridge_reset: invalid bridge");
        return FIN_TOM_ERR_NULL;
    }

    fin_tom_heartbeat("fin_tom_reset", 0.0f);

    nimcp_mutex_lock(bridge->base.mutex);

    /* Free all model nodes */
    for (uint32_t i = 0; i < MODEL_HASH_BUCKETS; i++) {
        model_node_t* node = bridge->model_buckets[i];
        while (node) {
            model_node_t* next = node->next;
            nimcp_free(node);
            node = NULL;
            node = next;
        }
        bridge->model_buckets[i] = NULL;
    }
    bridge->model_count = 0;

    /* Reset stats */
    memset(&bridge->stats, 0, sizeof(bridge->stats));

    bridge->state = FIN_TOM_STATE_INITIALIZED;

    nimcp_mutex_unlock(bridge->base.mutex);

    fin_tom_heartbeat("fin_tom_reset", 1.0f);
    return FIN_TOM_ERR_OK;
}

/* ============================================================================
 * Subsystem Setters
 * ============================================================================ */

#define FIN_TOM_SETTER(name, field) \
    int financial_tom_bridge_set_##name(financial_tom_bridge_t* bridge, void* ptr) { \
        if (!bridge || bridge->magic != FINANCIAL_TOM_BRIDGE_MAGIC) { \
            set_error("bridge is NULL in set_" #name); \
            NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_tom_bridge_set_" #name ": bridge is NULL"); \
            return FIN_TOM_ERR_NULL; \
        } \
        nimcp_mutex_lock(bridge->base.mutex); \
        bridge->field = ptr; \
        nimcp_mutex_unlock(bridge->base.mutex); \
        return FIN_TOM_ERR_OK; \
    }

FIN_TOM_SETTER(immune,        immune)
FIN_TOM_SETTER(health_agent,  health_agent)
FIN_TOM_SETTER(kg_wiring,     kg_wiring)
FIN_TOM_SETTER(logger,        logger)
FIN_TOM_SETTER(security,      security)
FIN_TOM_SETTER(bio_router,    bio_router)

/* Security setters for bbb, ethics, lgss, coordinator handled by bridge_base */

/* ============================================================================
 * Investor Model Implementation
 * ============================================================================ */

int financial_tom_bridge_model_investor(
    financial_tom_bridge_t* bridge,
    const char* investor_id,
    fin_tom_archetype_t archetype,
    float initial_confidence,
    fin_investor_model_t* out_model
) {
    if (!bridge || bridge->magic != FINANCIAL_TOM_BRIDGE_MAGIC) {
        set_error("bridge is NULL or invalid");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "model_investor: invalid bridge");
        return FIN_TOM_ERR_NULL;
    }
    if (!investor_id || strlen(investor_id) == 0) {
        set_error("investor_id is NULL or empty");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_INVALID_PARAM, "model_investor: invalid investor_id");
        return FIN_TOM_ERR_INVALID_PARAM;
    }
    if (archetype >= FIN_TOM_ARCHETYPE_COUNT) {
        set_error("invalid archetype: %d", archetype);
        return FIN_TOM_ERR_ARCHETYPE;
    }

    fin_tom_heartbeat("fin_tom_model_investor", 0.0f);

    /* BBB validation */
    if (bridge->config.enable_bbb_validation && bridge->bbb) {
        BRIDGE_BBB_VALIDATE(bridge, investor_id, strlen(investor_id));
        bridge->stats.bbb_validations++;
    }

    /* Immune check */
    if (bridge->config.enable_immune_integration && bridge->immune) {
        bridge->stats.immune_checks++;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Check if model already exists */
    model_node_t* existing = find_model_unlocked(bridge, investor_id);
    if (existing) {
        /* Update existing model */
        existing->model.archetype = archetype;
        existing->model.confidence = nimcp_clampf(initial_confidence, 0.0f, 1.0f);
        init_archetype_beliefs(&existing->model);

        if (out_model) {
            *out_model = existing->model;
        }

        nimcp_mutex_unlock(bridge->base.mutex);

        /* Fire callback */
        if (bridge->model_cb) {
            bridge->model_cb(&existing->model, bridge->model_cb_data);
        }

        fin_tom_heartbeat("fin_tom_model_investor", 1.0f);
        return FIN_TOM_ERR_OK;
    }

    /* Check capacity */
    if (bridge->model_count >= bridge->config.max_models) {
        nimcp_mutex_unlock(bridge->base.mutex);
        set_error("Model capacity reached: %u", bridge->config.max_models);
        return FIN_TOM_ERR_CAPACITY;
    }

    /* Create new model node */
    model_node_t* node = nimcp_calloc(1, sizeof(model_node_t));
    if (!node) {
        nimcp_mutex_unlock(bridge->base.mutex);
        set_error("Failed to allocate model node");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NO_MEMORY, "model_investor: allocation failed");
        return FIN_TOM_ERR_NO_MEMORY;
    }

    /* Initialize model */
    strncpy(node->model.investor_id, investor_id, FIN_TOM_INVESTOR_ID_LEN - 1);
    node->model.investor_id[FIN_TOM_INVESTOR_ID_LEN - 1] = '\0';
    node->model.archetype = archetype;
    node->model.confidence = nimcp_clampf(initial_confidence, 0.0f, 1.0f);
    node->model.emotional_state = FIN_TOM_EMOTION_NEUTRAL;
    init_archetype_beliefs(&node->model);

    /* Insert into hash map */
    uint32_t bucket = hash_investor_id(investor_id);
    node->next = bridge->model_buckets[bucket];
    bridge->model_buckets[bucket] = node;
    bridge->model_count++;

    bridge->stats.models_created++;
    bridge->state = FIN_TOM_STATE_ACTIVE;

    if (out_model) {
        *out_model = node->model;
    }

    /* Fire callback */
    fin_tom_model_callback_t cb = bridge->model_cb;
    void* cb_data = bridge->model_cb_data;

    nimcp_mutex_unlock(bridge->base.mutex);

    if (cb) {
        cb(&node->model, cb_data);
    }

    /* KG notification */
    bridge_kg_publish(bridge, KG_MSG_FIN_TOM_MODEL_CREATED, &node->model, sizeof(node->model));

    fin_tom_heartbeat("fin_tom_model_investor", 1.0f);
    bridge->stats.health_heartbeats++;

    return FIN_TOM_ERR_OK;
}

int financial_tom_bridge_update_beliefs(
    financial_tom_bridge_t* bridge,
    const char* investor_id,
    const float* beliefs,
    uint32_t num_beliefs
) {
    if (!bridge || bridge->magic != FINANCIAL_TOM_BRIDGE_MAGIC) {
        set_error("bridge is NULL or invalid");
        return FIN_TOM_ERR_NULL;
    }
    if (!investor_id || !beliefs || num_beliefs == 0) {
        set_error("invalid parameters");
        return FIN_TOM_ERR_INVALID_PARAM;
    }

    fin_tom_heartbeat("fin_tom_update_beliefs", 0.0f);

    nimcp_mutex_lock(bridge->base.mutex);

    model_node_t* node = find_model_unlocked(bridge, investor_id);
    if (!node) {
        nimcp_mutex_unlock(bridge->base.mutex);
        set_error("investor not found: %s", investor_id);
        return FIN_TOM_ERR_NOT_FOUND;
    }

    uint32_t count = (num_beliefs < FIN_TOM_MAX_BELIEFS) ? num_beliefs : FIN_TOM_MAX_BELIEFS;
    for (uint32_t i = 0; i < count; i++) {
        node->model.beliefs[i] = nimcp_clampf(beliefs[i], -1.0f, 1.0f);
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    fin_tom_heartbeat("fin_tom_update_beliefs", 1.0f);
    return FIN_TOM_ERR_OK;
}

int financial_tom_bridge_update_desires(
    financial_tom_bridge_t* bridge,
    const char* investor_id,
    const float* desires,
    uint32_t num_desires
) {
    if (!bridge || bridge->magic != FINANCIAL_TOM_BRIDGE_MAGIC) {
        set_error("bridge is NULL or invalid");
        return FIN_TOM_ERR_NULL;
    }
    if (!investor_id || !desires || num_desires == 0) {
        set_error("invalid parameters");
        return FIN_TOM_ERR_INVALID_PARAM;
    }

    fin_tom_heartbeat("fin_tom_update_desires", 0.0f);

    nimcp_mutex_lock(bridge->base.mutex);

    model_node_t* node = find_model_unlocked(bridge, investor_id);
    if (!node) {
        nimcp_mutex_unlock(bridge->base.mutex);
        set_error("investor not found: %s", investor_id);
        return FIN_TOM_ERR_NOT_FOUND;
    }

    uint32_t count = (num_desires < FIN_TOM_MAX_DESIRES) ? num_desires : FIN_TOM_MAX_DESIRES;
    for (uint32_t i = 0; i < count; i++) {
        node->model.desires[i] = nimcp_clampf(desires[i], 0.0f, 1.0f);
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    fin_tom_heartbeat("fin_tom_update_desires", 1.0f);
    return FIN_TOM_ERR_OK;
}

int financial_tom_bridge_update_emotion(
    financial_tom_bridge_t* bridge,
    const char* investor_id,
    fin_tom_emotion_t emotion,
    float confidence
) {
    if (!bridge || bridge->magic != FINANCIAL_TOM_BRIDGE_MAGIC) {
        set_error("bridge is NULL or invalid");
        return FIN_TOM_ERR_NULL;
    }
    if (!investor_id) {
        set_error("investor_id is NULL");
        return FIN_TOM_ERR_INVALID_PARAM;
    }

    fin_tom_heartbeat("fin_tom_update_emotion", 0.0f);

    nimcp_mutex_lock(bridge->base.mutex);

    model_node_t* node = find_model_unlocked(bridge, investor_id);
    if (!node) {
        nimcp_mutex_unlock(bridge->base.mutex);
        set_error("investor not found: %s", investor_id);
        return FIN_TOM_ERR_NOT_FOUND;
    }

    node->model.emotional_state = (int)emotion;
    node->model.confidence = nimcp_clampf(confidence, 0.0f, 1.0f);

    nimcp_mutex_unlock(bridge->base.mutex);

    fin_tom_heartbeat("fin_tom_update_emotion", 1.0f);
    return FIN_TOM_ERR_OK;
}

int financial_tom_bridge_get_model(
    financial_tom_bridge_t* bridge,
    const char* investor_id,
    fin_investor_model_t* out_model
) {
    if (!bridge || bridge->magic != FINANCIAL_TOM_BRIDGE_MAGIC || !out_model) {
        set_error("NULL argument");
        return FIN_TOM_ERR_NULL;
    }
    if (!investor_id) {
        set_error("investor_id is NULL");
        return FIN_TOM_ERR_INVALID_PARAM;
    }

    fin_tom_heartbeat("fin_tom_get_model", 0.0f);

    nimcp_mutex_lock(bridge->base.mutex);

    model_node_t* node = find_model_unlocked(bridge, investor_id);
    if (!node) {
        nimcp_mutex_unlock(bridge->base.mutex);
        set_error("investor not found: %s", investor_id);
        return FIN_TOM_ERR_NOT_FOUND;
    }

    *out_model = node->model;

    nimcp_mutex_unlock(bridge->base.mutex);

    return FIN_TOM_ERR_OK;
}

int financial_tom_bridge_remove_model(
    financial_tom_bridge_t* bridge,
    const char* investor_id
) {
    if (!bridge || bridge->magic != FINANCIAL_TOM_BRIDGE_MAGIC) {
        set_error("bridge is NULL or invalid");
        return FIN_TOM_ERR_NULL;
    }
    if (!investor_id) {
        set_error("investor_id is NULL");
        return FIN_TOM_ERR_INVALID_PARAM;
    }

    fin_tom_heartbeat("fin_tom_remove_model", 0.0f);

    nimcp_mutex_lock(bridge->base.mutex);

    uint32_t bucket = hash_investor_id(investor_id);
    model_node_t* prev = NULL;
    model_node_t* node = bridge->model_buckets[bucket];

    while (node) {
        if (strncmp(node->model.investor_id, investor_id, FIN_TOM_INVESTOR_ID_LEN) == 0) {
            if (prev) {
                prev->next = node->next;
            } else {
                bridge->model_buckets[bucket] = node->next;
            }
            nimcp_free(node);
            node = NULL;
            bridge->model_count--;
            nimcp_mutex_unlock(bridge->base.mutex);
            return FIN_TOM_ERR_OK;
        }
        prev = node;
        node = node->next;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    set_error("investor not found: %s", investor_id);
    return FIN_TOM_ERR_NOT_FOUND;
}

/* ============================================================================
 * Action Prediction Implementation
 * ============================================================================ */

int financial_tom_bridge_predict_action(
    financial_tom_bridge_t* bridge,
    const char* investor_id,
    const fin_tom_market_context_t* context,
    fin_tom_action_prediction_t* out_prediction
) {
    if (!bridge || bridge->magic != FINANCIAL_TOM_BRIDGE_MAGIC) {
        set_error("bridge is NULL or invalid");
        return FIN_TOM_ERR_NULL;
    }
    if (!investor_id || !context || !out_prediction) {
        set_error("NULL argument");
        return FIN_TOM_ERR_INVALID_PARAM;
    }

    fin_tom_heartbeat("fin_tom_predict_action", 0.0f);

    /* BBB validation */
    if (bridge->config.enable_bbb_validation && bridge->bbb) {
        BRIDGE_BBB_VALIDATE(bridge, context, sizeof(*context));
        bridge->stats.bbb_validations++;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    model_node_t* node = find_model_unlocked(bridge, investor_id);
    if (!node) {
        nimcp_mutex_unlock(bridge->base.mutex);
        set_error("investor not found: %s", investor_id);
        return FIN_TOM_ERR_NOT_FOUND;
    }

    /* Compute prediction */
    compute_action_prediction(&node->model, context, out_prediction);

    bridge->stats.action_predictions++;

    nimcp_mutex_unlock(bridge->base.mutex);

    /* KG notification */
    bridge_kg_publish(bridge, KG_MSG_FIN_TOM_ACTION_PREDICTED, out_prediction, sizeof(*out_prediction));

    fin_tom_heartbeat("fin_tom_predict_action", 1.0f);
    bridge->stats.health_heartbeats++;

    return FIN_TOM_ERR_OK;
}

int financial_tom_bridge_predict_actions_batch(
    financial_tom_bridge_t* bridge,
    const char** investor_ids,
    uint32_t num_investors,
    const fin_tom_market_context_t* context,
    fin_tom_action_prediction_t* out_predictions,
    uint32_t max_predictions,
    uint32_t* out_count
) {
    if (!bridge || bridge->magic != FINANCIAL_TOM_BRIDGE_MAGIC) {
        set_error("bridge is NULL or invalid");
        return FIN_TOM_ERR_NULL;
    }
    if (!context || !out_predictions || !out_count) {
        set_error("NULL argument");
        return FIN_TOM_ERR_INVALID_PARAM;
    }

    fin_tom_heartbeat("fin_tom_predict_batch", 0.0f);

    *out_count = 0;

    nimcp_mutex_lock(bridge->base.mutex);

    if (investor_ids && num_investors > 0) {
        /* Predict for specified investors */
        for (uint32_t i = 0; i < num_investors && *out_count < max_predictions; i++) {
            model_node_t* node = find_model_unlocked(bridge, investor_ids[i]);
            if (node) {
                compute_action_prediction(&node->model, context, &out_predictions[*out_count]);
                (*out_count)++;
                bridge->stats.action_predictions++;
            }
        }
    } else {
        /* Predict for all investors */
        for (uint32_t bucket = 0; bucket < MODEL_HASH_BUCKETS && *out_count < max_predictions; bucket++) {
            model_node_t* node = bridge->model_buckets[bucket];
            while (node && *out_count < max_predictions) {
                compute_action_prediction(&node->model, context, &out_predictions[*out_count]);
                (*out_count)++;
                bridge->stats.action_predictions++;
                node = node->next;
            }
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    fin_tom_heartbeat("fin_tom_predict_batch", 1.0f);
    bridge->stats.health_heartbeats++;

    return FIN_TOM_ERR_OK;
}

/* ============================================================================
 * False Belief Detection Implementation
 * ============================================================================ */

int financial_tom_bridge_detect_false_belief(
    financial_tom_bridge_t* bridge,
    const char* investor_id,
    const fin_tom_market_context_t* context,
    fin_tom_false_belief_result_t* out_result
) {
    if (!bridge || bridge->magic != FINANCIAL_TOM_BRIDGE_MAGIC) {
        set_error("bridge is NULL or invalid");
        return FIN_TOM_ERR_NULL;
    }
    if (!investor_id || !context || !out_result) {
        set_error("NULL argument");
        return FIN_TOM_ERR_INVALID_PARAM;
    }

    fin_tom_heartbeat("fin_tom_detect_false_belief", 0.0f);

    memset(out_result, 0, sizeof(*out_result));

    nimcp_mutex_lock(bridge->base.mutex);

    model_node_t* node = find_model_unlocked(bridge, investor_id);
    if (!node) {
        nimcp_mutex_unlock(bridge->base.mutex);
        set_error("investor not found: %s", investor_id);
        return FIN_TOM_ERR_NOT_FOUND;
    }

    /* Allocate result array */
    out_result->beliefs = nimcp_calloc(FIN_TOM_MAX_FALSE_BELIEFS, sizeof(fin_tom_false_belief_t));
    if (!out_result->beliefs) {
        nimcp_mutex_unlock(bridge->base.mutex);
        set_error("Failed to allocate beliefs array");
        return FIN_TOM_ERR_NO_MEMORY;
    }

    /* Detect false beliefs */
    uint32_t count = 0;
    detect_model_false_beliefs(&node->model, context, out_result->beliefs, FIN_TOM_MAX_FALSE_BELIEFS, &count);
    out_result->count = count;
    out_result->total_analyzed = 1;

    bridge->stats.false_belief_detections += count;

    /* Fire callback for each detection */
    fin_tom_false_belief_callback_t cb = bridge->false_belief_cb;
    void* cb_data = bridge->false_belief_cb_data;

    nimcp_mutex_unlock(bridge->base.mutex);

    if (cb) {
        for (uint32_t i = 0; i < count; i++) {
            cb(&out_result->beliefs[i], cb_data);
        }
    }

    /* KG notification */
    if (count > 0) {
        bridge_kg_publish(bridge, KG_MSG_FIN_TOM_FALSE_BELIEF, out_result->beliefs, sizeof(fin_tom_false_belief_t) * count);
    }

    fin_tom_heartbeat("fin_tom_detect_false_belief", 1.0f);
    bridge->stats.health_heartbeats++;

    return FIN_TOM_ERR_OK;
}

int financial_tom_bridge_detect_false_beliefs_all(
    financial_tom_bridge_t* bridge,
    const fin_tom_market_context_t* context,
    fin_tom_false_belief_result_t* out_result
) {
    if (!bridge || bridge->magic != FINANCIAL_TOM_BRIDGE_MAGIC) {
        set_error("bridge is NULL or invalid");
        return FIN_TOM_ERR_NULL;
    }
    if (!context || !out_result) {
        set_error("NULL argument");
        return FIN_TOM_ERR_INVALID_PARAM;
    }

    fin_tom_heartbeat("fin_tom_detect_false_beliefs_all", 0.0f);

    memset(out_result, 0, sizeof(*out_result));

    nimcp_mutex_lock(bridge->base.mutex);

    /* Allocate result array */
    out_result->beliefs = nimcp_calloc(FIN_TOM_MAX_FALSE_BELIEFS, sizeof(fin_tom_false_belief_t));
    if (!out_result->beliefs) {
        nimcp_mutex_unlock(bridge->base.mutex);
        set_error("Failed to allocate beliefs array");
        return FIN_TOM_ERR_NO_MEMORY;
    }

    uint32_t total_count = 0;
    uint32_t models_analyzed = 0;

    /* Iterate all models */
    for (uint32_t bucket = 0; bucket < MODEL_HASH_BUCKETS; bucket++) {
        model_node_t* node = bridge->model_buckets[bucket];
        while (node) {
            uint32_t count = 0;
            uint32_t remaining = FIN_TOM_MAX_FALSE_BELIEFS - total_count;
            if (remaining > 0) {
                detect_model_false_beliefs(&node->model, context,
                                           &out_result->beliefs[total_count],
                                           remaining, &count);
                total_count += count;
                bridge->stats.false_belief_detections += count;
            }
            models_analyzed++;
            node = node->next;
        }
    }

    out_result->count = total_count;
    out_result->total_analyzed = models_analyzed;

    nimcp_mutex_unlock(bridge->base.mutex);

    fin_tom_heartbeat("fin_tom_detect_false_beliefs_all", 1.0f);
    bridge->stats.health_heartbeats++;

    return FIN_TOM_ERR_OK;
}

void financial_tom_bridge_free_false_belief_result(fin_tom_false_belief_result_t* result) {
    if (result && result->beliefs) {
        nimcp_free(result->beliefs);
        result->beliefs = NULL;
        result->count = 0;
        result->total_analyzed = 0;
    }
}

/* ============================================================================
 * Sentiment Aggregation Implementation
 * ============================================================================ */

int financial_tom_bridge_aggregate_sentiment(
    financial_tom_bridge_t* bridge,
    const char** investor_ids,
    uint32_t num_investors,
    fin_tom_sentiment_t* out_sentiment
) {
    if (!bridge || bridge->magic != FINANCIAL_TOM_BRIDGE_MAGIC) {
        set_error("bridge is NULL or invalid");
        return FIN_TOM_ERR_NULL;
    }
    if (!out_sentiment) {
        set_error("out_sentiment is NULL");
        return FIN_TOM_ERR_INVALID_PARAM;
    }

    fin_tom_heartbeat("fin_tom_aggregate_sentiment", 0.0f);

    memset(out_sentiment, 0, sizeof(*out_sentiment));
    out_sentiment->timestamp_ms = nimcp_time_get_ms();

    nimcp_mutex_lock(bridge->base.mutex);

    float bullish_sum = 0.0f;
    float bearish_sum = 0.0f;
    float neutral_sum = 0.0f;
    float conviction_sum = 0.0f;
    float fear_sum = 0.0f;
    float greed_sum = 0.0f;
    float weight_sum = 0.0f;
    uint32_t count = 0;

    /* Helper to process a single model */
    #define PROCESS_MODEL(node) do { \
        float weight = bridge->config.weight_by_confidence ? node->model.confidence : 1.0f; \
        /* Determine sentiment from beliefs and intentions */ \
        float sentiment_score = node->model.beliefs[0] * 0.5f + node->model.intentions[0] * 0.5f; \
        if (sentiment_score > 0.2f) { \
            bullish_sum += weight; \
        } else if (sentiment_score < -0.2f) { \
            bearish_sum += weight; \
        } else { \
            neutral_sum += weight; \
        } \
        conviction_sum += node->model.confidence * weight; \
        /* Factor in emotions */ \
        switch (node->model.emotional_state) { \
            case FIN_TOM_EMOTION_FEAR: \
            case FIN_TOM_EMOTION_PANIC: \
            case FIN_TOM_EMOTION_DOUBT: \
                fear_sum += weight; \
                break; \
            case FIN_TOM_EMOTION_GREED: \
            case FIN_TOM_EMOTION_EUPHORIA: \
            case FIN_TOM_EMOTION_CONFIDENCE: \
                greed_sum += weight; \
                break; \
            default: \
                break; \
        } \
        weight_sum += weight; \
        count++; \
    } while (0)

    if (investor_ids && num_investors > 0) {
        /* Process specified investors */
        for (uint32_t i = 0; i < num_investors; i++) {
            model_node_t* node = find_model_unlocked(bridge, investor_ids[i]);
            if (node) {
                PROCESS_MODEL(node);
            }
        }
    } else {
        /* Process all investors */
        for (uint32_t bucket = 0; bucket < MODEL_HASH_BUCKETS; bucket++) {
            model_node_t* node = bridge->model_buckets[bucket];
            while (node) {
                PROCESS_MODEL(node);
                node = node->next;
            }
        }
    }

    #undef PROCESS_MODEL

    /* Compute final sentiment */
    if (weight_sum > 0.0f) {
        out_sentiment->bullish_pct = bullish_sum / weight_sum;
        out_sentiment->bearish_pct = bearish_sum / weight_sum;
        out_sentiment->neutral_pct = neutral_sum / weight_sum;
        out_sentiment->conviction_avg = conviction_sum / weight_sum;
        out_sentiment->fear_level = fear_sum / weight_sum;
        out_sentiment->greed_level = greed_sum / weight_sum;

        /* Calculate consensus strength (inverse of dispersion) */
        float max_pct = out_sentiment->bullish_pct;
        if (out_sentiment->bearish_pct > max_pct) max_pct = out_sentiment->bearish_pct;
        if (out_sentiment->neutral_pct > max_pct) max_pct = out_sentiment->neutral_pct;
        out_sentiment->consensus_strength = max_pct;
    }
    out_sentiment->models_included = count;

    bridge->stats.sentiment_aggregations++;

    nimcp_mutex_unlock(bridge->base.mutex);

    /* KG notification */
    bridge_kg_publish(bridge, KG_MSG_FIN_TOM_SENTIMENT, out_sentiment, sizeof(*out_sentiment));

    fin_tom_heartbeat("fin_tom_aggregate_sentiment", 1.0f);
    bridge->stats.health_heartbeats++;

    return FIN_TOM_ERR_OK;
}

int financial_tom_bridge_sentiment_by_archetype(
    financial_tom_bridge_t* bridge,
    fin_tom_archetype_t archetype,
    fin_tom_sentiment_t* out_sentiment
) {
    if (!bridge || bridge->magic != FINANCIAL_TOM_BRIDGE_MAGIC) {
        set_error("bridge is NULL or invalid");
        return FIN_TOM_ERR_NULL;
    }
    if (!out_sentiment || archetype >= FIN_TOM_ARCHETYPE_COUNT) {
        set_error("invalid parameter");
        return FIN_TOM_ERR_INVALID_PARAM;
    }

    fin_tom_heartbeat("fin_tom_sentiment_by_archetype", 0.0f);

    memset(out_sentiment, 0, sizeof(*out_sentiment));
    out_sentiment->timestamp_ms = nimcp_time_get_ms();

    nimcp_mutex_lock(bridge->base.mutex);

    float bullish_sum = 0.0f;
    float bearish_sum = 0.0f;
    float neutral_sum = 0.0f;
    float weight_sum = 0.0f;
    uint32_t count = 0;

    for (uint32_t bucket = 0; bucket < MODEL_HASH_BUCKETS; bucket++) {
        model_node_t* node = bridge->model_buckets[bucket];
        while (node) {
            if (node->model.archetype == archetype) {
                float weight = bridge->config.weight_by_confidence ? node->model.confidence : 1.0f;
                float sentiment_score = node->model.beliefs[0] * 0.5f + node->model.intentions[0] * 0.5f;
                if (sentiment_score > 0.2f) {
                    bullish_sum += weight;
                } else if (sentiment_score < -0.2f) {
                    bearish_sum += weight;
                } else {
                    neutral_sum += weight;
                }
                weight_sum += weight;
                count++;
            }
            node = node->next;
        }
    }

    if (weight_sum > 0.0f) {
        out_sentiment->bullish_pct = bullish_sum / weight_sum;
        out_sentiment->bearish_pct = bearish_sum / weight_sum;
        out_sentiment->neutral_pct = neutral_sum / weight_sum;
    }
    out_sentiment->models_included = count;

    nimcp_mutex_unlock(bridge->base.mutex);

    fin_tom_heartbeat("fin_tom_sentiment_by_archetype", 1.0f);
    return FIN_TOM_ERR_OK;
}

/* ============================================================================
 * Callback Registration
 * ============================================================================ */

int financial_tom_bridge_set_model_callback(
    financial_tom_bridge_t* bridge,
    fin_tom_model_callback_t callback,
    void* user_data
) {
    if (!bridge || bridge->magic != FINANCIAL_TOM_BRIDGE_MAGIC) {
        set_error("bridge is NULL or invalid");
        return FIN_TOM_ERR_NULL;
    }

    fin_tom_heartbeat("fin_tom_set_model_cb", 0.0f);

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->model_cb = callback;
    bridge->model_cb_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return FIN_TOM_ERR_OK;
}

int financial_tom_bridge_set_false_belief_callback(
    financial_tom_bridge_t* bridge,
    fin_tom_false_belief_callback_t callback,
    void* user_data
) {
    if (!bridge || bridge->magic != FINANCIAL_TOM_BRIDGE_MAGIC) {
        set_error("bridge is NULL or invalid");
        return FIN_TOM_ERR_NULL;
    }

    fin_tom_heartbeat("fin_tom_set_false_belief_cb", 0.0f);

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->false_belief_cb = callback;
    bridge->false_belief_cb_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return FIN_TOM_ERR_OK;
}

/* ============================================================================
 * Query Implementation
 * ============================================================================ */

fin_tom_state_t financial_tom_bridge_get_state(
    const financial_tom_bridge_t* bridge
) {
    if (!bridge || bridge->magic != FINANCIAL_TOM_BRIDGE_MAGIC) {
        return FIN_TOM_STATE_ERROR;
    }
    return bridge->state;
}

int financial_tom_bridge_get_stats(
    const financial_tom_bridge_t* bridge,
    fin_tom_bridge_stats_t* stats
) {
    if (!bridge || bridge->magic != FINANCIAL_TOM_BRIDGE_MAGIC || !stats) {
        set_error("NULL argument");
        return FIN_TOM_ERR_NULL;
    }

    fin_tom_heartbeat("fin_tom_get_stats", 0.0f);

    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);

    return FIN_TOM_ERR_OK;
}

void financial_tom_bridge_reset_stats(financial_tom_bridge_t* bridge) {
    if (!bridge || bridge->magic != FINANCIAL_TOM_BRIDGE_MAGIC) {
        return;
    }

    fin_tom_heartbeat("fin_tom_reset_stats", 0.0f);

    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    nimcp_mutex_unlock(bridge->base.mutex);
}

uint32_t financial_tom_bridge_get_model_count(
    const financial_tom_bridge_t* bridge
) {
    if (!bridge || bridge->magic != FINANCIAL_TOM_BRIDGE_MAGIC) {
        return 0;
    }
    return bridge->model_count;
}

const char* financial_tom_bridge_get_last_error(void) {
    return fin_tom_last_error;
}

/* ============================================================================
 * Health Integration
 * ============================================================================ */

int financial_tom_bridge_heartbeat(
    financial_tom_bridge_t* bridge,
    const char* operation,
    float progress
) {
    if (!bridge || bridge->magic != FINANCIAL_TOM_BRIDGE_MAGIC) {
        return FIN_TOM_ERR_NULL;
    }

    /* Forward to global health agent */
    fin_tom_heartbeat(operation ? operation : "fin_tom_heartbeat", progress);

    /* Forward to instance-level health agent */
    if (bridge->health_agent) {
        nimcp_health_agent_heartbeat_ex(
            (nimcp_health_agent_t*)bridge->health_agent, operation, progress);
    }

    bridge->stats.health_heartbeats++;
    return FIN_TOM_ERR_OK;
}

/* ============================================================================
 * Training Hooks (B23 Upgrade Compatibility)
 * ============================================================================ */

int financial_tom_bridge_training_begin(financial_tom_bridge_t* bridge) {
    if (!bridge || bridge->magic != FINANCIAL_TOM_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "financial_tom_bridge_training_begin: NULL argument");
        return -1;
    }
    fin_tom_heartbeat_instance((nimcp_health_agent_t*)bridge->health_agent,
                                "financial_tom_bridge_training_begin", 0.0f);
    return 0;
}

int financial_tom_bridge_training_end(financial_tom_bridge_t* bridge) {
    if (!bridge || bridge->magic != FINANCIAL_TOM_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "financial_tom_bridge_training_end: NULL argument");
        return -1;
    }
    fin_tom_heartbeat_instance((nimcp_health_agent_t*)bridge->health_agent,
                                "financial_tom_bridge_training_end", 1.0f);
    return 0;
}

int financial_tom_bridge_training_step(financial_tom_bridge_t* bridge, float progress) {
    if (!bridge || bridge->magic != FINANCIAL_TOM_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "financial_tom_bridge_training_step: NULL argument");
        return -1;
    }

    /* Safety gates: ethics + LGSS pre-check */
    BRIDGE_ETHICS_GATE(bridge, "financial_tom_bridge_training_step");
    BRIDGE_LGSS_GATE(bridge, "financial_tom_bridge_training_step");

    fin_tom_heartbeat_instance((nimcp_health_agent_t*)bridge->health_agent,
                                "financial_tom_bridge_training_step", progress);

    /* Notify coordinator of update cycle completion */
    bridge_base_notify_coordinator_tick(&bridge->base, 0);
    return 0;
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

const char* fin_tom_archetype_name(fin_tom_archetype_t archetype) {
    if (archetype >= FIN_TOM_ARCHETYPE_COUNT) {
        return "unknown";
    }
    return archetype_names[archetype];
}

const char* fin_tom_emotion_name(fin_tom_emotion_t emotion) {
    if (emotion >= FIN_TOM_EMOTION_COUNT) {
        return "unknown";
    }
    return emotion_names[emotion];
}

const char* fin_tom_action_name(fin_tom_action_t action) {
    if (action >= FIN_TOM_ACTION_COUNT) {
        return "unknown";
    }
    return action_names[action];
}

const char* fin_tom_false_belief_name(fin_tom_false_belief_type_t type) {
    if (type >= FIN_TOM_FALSE_BELIEF_COUNT) {
        return "unknown";
    }
    return false_belief_names[type];
}

const char* fin_tom_state_name(fin_tom_state_t state) {
    if (state > FIN_TOM_STATE_ERROR) {
        return "unknown";
    }
    return state_names[state];
}

const char* financial_tom_bridge_version(void) {
    return FINANCIAL_TOM_BRIDGE_VERSION;
}
