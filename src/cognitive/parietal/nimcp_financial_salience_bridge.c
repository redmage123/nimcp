/**
 * @file nimcp_financial_salience_bridge.c
 * @brief Financial Salience Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-29
 *
 * WHAT: Bridge for evaluating, filtering, and ranking financial market events
 *       based on salience (novelty, surprise, urgency, relevance).
 *
 * WHY:  Enables attention-based prioritization of market events in high-frequency
 *       trading environments where thousands of events occur per second.
 *
 * HOW:  Events are scored across four salience dimensions, combined using
 *       configurable weights, and filtered/ranked for priority processing.
 *
 * @author NIMCP Development Team
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdarg.h>

#include "cognitive/parietal/nimcp_financial_salience_bridge.h"
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

#define LOG_MODULE "financial_salience"

/** Maximum symbols to track for relevance */
#define MAX_TRACKED_SYMBOLS 256

/* ============================================================================
 * Health Agent Integration (Phase 8: System-Wide Health Integration)
 * ============================================================================ */
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/math/nimcp_math_helpers.h"

/* Health agent: using pre-existing custom implementation */
static nimcp_health_agent_t* g_financial_salience_bridge_health_agent = NULL;

#include "utils/bridge/nimcp_bridge_boilerplate.h"
BRIDGE_DEFINE_MESH_REGISTRATION(financial_salience_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


/** @brief Send heartbeat from financial_salience_bridge module */
static inline void fin_salience_heartbeat(const char* operation, float progress) {
    if (g_financial_salience_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_financial_salience_bridge_health_agent, operation, progress);
    }
}

/** @brief Send heartbeat from financial_salience_bridge module (instance-level) */
static inline void fin_salience_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_financial_salience_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_financial_salience_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_financial_salience_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}

/* ============================================================================
 * Thread-Local Error
 * ============================================================================ */

static _Thread_local char fin_salience_last_error[NIMCP_ERROR_BUFFER_SIZE] = {0};

static void set_error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(fin_salience_last_error, sizeof(fin_salience_last_error), fmt, args);
    va_end(args);
}

/* ============================================================================
 * KG Wiring Integration
 * ============================================================================ */

#define KG_MSG_FIN_SALIENCE_EVAL      "FIN_SALIENCE_EVAL"
#define KG_MSG_FIN_SALIENCE_FILTER    "FIN_SALIENCE_FILTER"
#define KG_MSG_FIN_SALIENCE_RANK      "FIN_SALIENCE_RANK"
#define KG_MSG_FIN_SALIENCE_ERROR     "FIN_SALIENCE_ERROR"

/* ============================================================================
 * Internal Structures
 * ============================================================================ */

/**
 * @brief Symbol relevance tracking entry
 */
typedef struct {
    char symbol[FIN_SALIENCE_MAX_SYMBOL];
    float relevance;
    bool active;
} symbol_relevance_entry_t;

/**
 * @brief Event history entry for novelty calculation
 */
typedef struct {
    int event_type;
    float magnitude;
    float volume_ratio;
    uint64_t timestamp_ms;
} event_history_entry_t;

/**
 * @brief Financial salience bridge structure
 */
struct financial_salience_bridge {
    bridge_base_t base;                 /**< MUST be first: base bridge infrastructure */
    uint32_t magic;
    fin_salience_bridge_state_t state;

    /* Configuration */
    fin_salience_config_t config;

    /* Normalized weights (sum to 1.0) */
    fin_salience_weights_t normalized_weights;

    /* Symbol relevance tracking */
    symbol_relevance_entry_t* symbol_relevance;
    size_t symbol_count;

    /* Event history for novelty (EMA) */
    event_history_entry_t* event_history;
    size_t history_count;
    size_t history_capacity;
    size_t history_index;

    /* Running EMA values for novelty */
    float ema_magnitude;
    float ema_volume_ratio;
    bool ema_initialized;

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
    fin_salience_bridge_stats_t stats;
};

/* Security integration via bridge_base */
BRIDGE_DEFINE_SECURITY_SETTERS(financial_salience_bridge)

/* ============================================================================
 * Static Name Tables
 * ============================================================================ */

static const char* event_type_names[] = {
    "price_change",
    "volume_spike",
    "volatility_change",
    "order_imbalance",
    "news",
    "earnings",
    "dividend",
    "split",
    "halt",
    "circuit_breaker",
    "regime_change",
    "custom"
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
 * @brief Normalize weights to sum to 1.0
 */
static void normalize_weights(const fin_salience_weights_t* input, fin_salience_weights_t* output) {
    float sum = input->novelty_weight + input->surprise_weight +
                input->urgency_weight + input->relevance_weight;

    if (sum <= 0.0f) {
        /* Default to equal weights */
        output->novelty_weight = 0.25f;
        output->surprise_weight = 0.25f;
        output->urgency_weight = 0.25f;
        output->relevance_weight = 0.25f;
    } else {
        output->novelty_weight = input->novelty_weight / sum;
        output->surprise_weight = input->surprise_weight / sum;
        output->urgency_weight = input->urgency_weight / sum;
        output->relevance_weight = input->relevance_weight / sum;
    }
}

/**
 * @brief Publish message through KG wiring
 */
static int bridge_kg_publish(financial_salience_bridge_t* bridge, const char* msg_type,
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
 * @brief Find symbol relevance entry
 */
static symbol_relevance_entry_t* find_symbol_entry(financial_salience_bridge_t* bridge, const char* symbol) {
    if (!bridge->symbol_relevance || !symbol) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_symbol_entry: required parameter is NULL (bridge->symbol_relevance, symbol)");
        return NULL;
    }

    for (size_t i = 0; i < bridge->symbol_count; i++) {
        if (bridge->symbol_relevance[i].active &&
            strncmp(bridge->symbol_relevance[i].symbol, symbol, FIN_SALIENCE_MAX_SYMBOL - 1) == 0) {
            return &bridge->symbol_relevance[i];
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_symbol_entry: required parameter is NULL (bridge->symbol_relevance, symbol)");
    return NULL;
}

/**
 * @brief Get relevance for a symbol
 */
static float get_symbol_relevance(financial_salience_bridge_t* bridge, const char* symbol) {
    symbol_relevance_entry_t* entry = find_symbol_entry(bridge, symbol);
    return entry ? entry->relevance : 0.0f;
}

/**
 * @brief Update EMA for novelty calculation
 */
static void update_ema(financial_salience_bridge_t* bridge, const fin_market_event_t* event) {
    float alpha = bridge->config.ema_alpha;

    if (!bridge->ema_initialized) {
        bridge->ema_magnitude = fabsf(event->magnitude);
        bridge->ema_volume_ratio = event->volume_ratio;
        bridge->ema_initialized = true;
    } else {
        bridge->ema_magnitude = alpha * fabsf(event->magnitude) + (1.0f - alpha) * bridge->ema_magnitude;
        bridge->ema_volume_ratio = alpha * event->volume_ratio + (1.0f - alpha) * bridge->ema_volume_ratio;
    }

    /* Add to history */
    if (bridge->event_history && bridge->history_capacity > 0) {
        bridge->event_history[bridge->history_index].event_type = event->event_type;
        bridge->event_history[bridge->history_index].magnitude = event->magnitude;
        bridge->event_history[bridge->history_index].volume_ratio = event->volume_ratio;
        bridge->event_history[bridge->history_index].timestamp_ms = event->timestamp_ms;

        bridge->history_index = (bridge->history_index + 1) % bridge->history_capacity;
        if (bridge->history_count < bridge->history_capacity) {
            bridge->history_count++;
        }
    }
}

/**
 * @brief Calculate novelty score (deviation from EMA)
 */
static float calculate_novelty(financial_salience_bridge_t* bridge, const fin_market_event_t* event) {
    if (!bridge->ema_initialized) {
        return 0.5f;  /* Unknown baseline = moderate novelty */
    }

    /* Calculate deviation from running average */
    float mag_dev = fabsf(fabsf(event->magnitude) - bridge->ema_magnitude);
    float vol_dev = fabsf(event->volume_ratio - bridge->ema_volume_ratio);

    /* Normalize deviations (assuming typical deviations are small) */
    float novelty = nimcp_clampf((mag_dev * 2.0f + vol_dev * 0.5f) / 2.5f, 0.0f, 1.0f);

    return novelty;
}

/**
 * @brief Calculate surprise score (prediction error)
 */
static float calculate_surprise(const fin_market_event_t* event) {
    /* Surprise based on event type and magnitude */
    float base_surprise = 0.0f;

    switch (event->event_type) {
        case FIN_SAL_EVENT_CIRCUIT_BREAKER:
        case FIN_SAL_EVENT_HALT:
            base_surprise = 0.9f;  /* Very surprising events */
            break;
        case FIN_SAL_EVENT_REGIME_CHANGE:
        case FIN_SAL_EVENT_EARNINGS:
            base_surprise = 0.6f;
            break;
        case FIN_SAL_EVENT_NEWS:
        case FIN_SAL_EVENT_VOLUME_SPIKE:
            base_surprise = 0.4f;
            break;
        case FIN_SAL_EVENT_PRICE_CHANGE:
        case FIN_SAL_EVENT_VOLATILITY_CHANGE:
            base_surprise = 0.2f;
            break;
        default:
            base_surprise = 0.3f;
            break;
    }

    /* Modulate by magnitude */
    float magnitude_factor = minf(fabsf(event->magnitude) * 2.0f, 1.0f);
    float volume_factor = nimcp_clampf((event->volume_ratio - 1.0f) / 3.0f, 0.0f, 1.0f);

    float surprise = base_surprise + (1.0f - base_surprise) * 0.5f * (magnitude_factor + volume_factor);

    return nimcp_clampf(surprise, 0.0f, 1.0f);
}

/**
 * @brief Calculate urgency score (time sensitivity)
 */
static float calculate_urgency(const fin_market_event_t* event, uint64_t current_time_ms) {
    /* Urgency based on event type */
    float base_urgency = 0.0f;

    switch (event->event_type) {
        case FIN_SAL_EVENT_CIRCUIT_BREAKER:
        case FIN_SAL_EVENT_HALT:
            base_urgency = 1.0f;  /* Maximum urgency */
            break;
        case FIN_SAL_EVENT_ORDER_IMBALANCE:
            base_urgency = 0.8f;
            break;
        case FIN_SAL_EVENT_PRICE_CHANGE:
        case FIN_SAL_EVENT_VOLUME_SPIKE:
            base_urgency = 0.6f;
            break;
        case FIN_SAL_EVENT_NEWS:
        case FIN_SAL_EVENT_EARNINGS:
            base_urgency = 0.7f;
            break;
        case FIN_SAL_EVENT_REGIME_CHANGE:
            base_urgency = 0.5f;
            break;
        default:
            base_urgency = 0.3f;
            break;
    }

    /* Decay urgency based on event age */
    if (event->timestamp_ms > 0 && current_time_ms > event->timestamp_ms) {
        uint64_t age_ms = current_time_ms - event->timestamp_ms;
        float decay = expf(-((float)age_ms / 10000.0f));  /* ~10s half-life */
        base_urgency *= decay;
    }

    /* Modulate by price change magnitude (larger moves = more urgent) */
    float price_factor = nimcp_clampf(fabsf(event->price_change_pct) / 5.0f, 0.0f, 1.0f);
    float urgency = base_urgency + (1.0f - base_urgency) * 0.3f * price_factor;

    return nimcp_clampf(urgency, 0.0f, 1.0f);
}

/**
 * @brief Calculate relevance score
 */
static float calculate_relevance(financial_salience_bridge_t* bridge, const fin_market_event_t* event) {
    /* Base relevance from symbol tracking */
    float symbol_relevance = get_symbol_relevance(bridge, event->symbol);

    /* Modulate by event magnitude (larger events are more relevant even if not tracked) */
    float magnitude_relevance = nimcp_clampf(fabsf(event->magnitude), 0.0f, 0.5f);

    /* Combine symbol tracking with magnitude */
    float relevance = maxf(symbol_relevance, magnitude_relevance);

    return nimcp_clampf(relevance, 0.0f, 1.0f);
}

/**
 * @brief Comparison function for qsort (descending by combined score)
 */
static int compare_scored_events_desc(const void* a, const void* b) {
    const fin_scored_event_t* ea = (const fin_scored_event_t*)a;
    const fin_scored_event_t* eb = (const fin_scored_event_t*)b;

    if (eb->score.combined > ea->score.combined) return 1;
    if (eb->score.combined < ea->score.combined) return -1;
    return 0;
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

int financial_salience_bridge_default_config(fin_salience_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");
        return -1;
    }

    fin_salience_heartbeat("fin_salience_default_config", 0.0f);

    memset(config, 0, sizeof(*config));

    /* Salience weights (will be normalized) */
    config->weights.novelty_weight = 0.25f;
    config->weights.surprise_weight = 0.25f;
    config->weights.urgency_weight = 0.30f;
    config->weights.relevance_weight = 0.20f;

    /* Thresholds */
    config->attention_threshold = 0.3f;
    config->high_priority_threshold = 0.7f;

    /* Novelty calculation */
    config->ema_alpha = 0.1f;
    config->history_size = FIN_SALIENCE_HISTORY_SIZE;

    /* Urgency parameters */
    config->base_urgency_decay = 0.1f;

    /* Integration settings */
    config->enable_immune_integration = true;
    config->enable_bbb_validation = true;
    config->enable_kg_messaging = true;
    config->enable_health_monitoring = true;

    /* Logging */
    config->verbose_logging = false;

    fin_salience_heartbeat("fin_salience_default_config", 1.0f);
    return 0;
}

financial_salience_bridge_t* financial_salience_bridge_create(
    const fin_salience_config_t* config
) {
    fin_salience_heartbeat("fin_salience_create", 0.0f);

    financial_salience_bridge_t* bridge = nimcp_calloc(1, sizeof(financial_salience_bridge_t));
    if (!bridge) {
        set_error("Failed to allocate financial_salience_bridge");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NO_MEMORY, "Failed to allocate financial_salience_bridge");
        return NULL;
    }

    bridge->magic = FINANCIAL_SALIENCE_BRIDGE_MAGIC;
    bridge->state = FIN_SALIENCE_STATE_UNINITIALIZED;

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        financial_salience_bridge_default_config(&bridge->config);
    }

    /* Normalize weights */
    normalize_weights(&bridge->config.weights, &bridge->normalized_weights);

    /* Initialize bridge base (creates mutex) */
    if (bridge_base_init(&bridge->base, BIO_MODULE_FINANCIAL_SALIENCE, "financial_salience") != 0) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "financial_salience_bridge_create: validation failed");
        return NULL;
    }

    /* Allocate symbol relevance tracking */
    bridge->symbol_relevance = nimcp_calloc(MAX_TRACKED_SYMBOLS, sizeof(symbol_relevance_entry_t));
    if (!bridge->symbol_relevance) {
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        bridge = NULL;
        set_error("Failed to allocate symbol_relevance array");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "financial_salience_bridge_create: bridge->symbol_relevance is NULL");
        return NULL;
    }
    bridge->symbol_count = MAX_TRACKED_SYMBOLS;

    /* Allocate event history */
    bridge->history_capacity = bridge->config.history_size;
    if (bridge->history_capacity > 0) {
        bridge->event_history = nimcp_calloc(bridge->history_capacity, sizeof(event_history_entry_t));
        if (!bridge->event_history) {
            nimcp_free(bridge->symbol_relevance);
            bridge_base_cleanup(&bridge->base);
            nimcp_free(bridge);
            bridge = NULL;
            set_error("Failed to allocate event_history array");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "financial_salience_bridge_create: bridge->event_history is NULL");
            return NULL;
        }
    }
    bridge->history_count = 0;
    bridge->history_index = 0;

    /* Initialize EMA */
    bridge->ema_initialized = false;
    bridge->ema_magnitude = 0.0f;
    bridge->ema_volume_ratio = 1.0f;

    bridge->last_update_ms = nimcp_time_get_ms();

    /* Initialize statistics */
    memset(&bridge->stats, 0, sizeof(bridge->stats));

    bridge->state = FIN_SALIENCE_STATE_INITIALIZED;

    fin_salience_heartbeat("fin_salience_create", 1.0f);
    return bridge;
}

void financial_salience_bridge_destroy(financial_salience_bridge_t* bridge) {
    if (!bridge || bridge->magic != FINANCIAL_SALIENCE_BRIDGE_MAGIC) {
        return;
    }

    fin_salience_heartbeat("fin_salience_destroy", 0.0f);

    /* Free event history */
    if (bridge->event_history) {
        nimcp_free(bridge->event_history);
        bridge->event_history = NULL;
    }

    /* Free symbol relevance */
    if (bridge->symbol_relevance) {
        nimcp_free(bridge->symbol_relevance);
        bridge->symbol_relevance = NULL;
    }

    /* Cleanup base */
    bridge_base_cleanup(&bridge->base);

    bridge->magic = 0;
    nimcp_free(bridge);
    bridge = NULL;

    fin_salience_heartbeat("fin_salience_destroy", 1.0f);
}

int financial_salience_bridge_reset(financial_salience_bridge_t* bridge) {
    if (!bridge || bridge->magic != FINANCIAL_SALIENCE_BRIDGE_MAGIC) {
        set_error("bridge is NULL or invalid");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_salience_bridge_reset: invalid bridge");
        return FIN_SALIENCE_ERR_NULL;
    }

    fin_salience_heartbeat("fin_salience_reset", 0.0f);

    nimcp_mutex_lock(bridge->base.mutex);

    /* Reset EMA */
    bridge->ema_initialized = false;
    bridge->ema_magnitude = 0.0f;
    bridge->ema_volume_ratio = 1.0f;

    /* Reset event history */
    bridge->history_count = 0;
    bridge->history_index = 0;
    if (bridge->event_history) {
        memset(bridge->event_history, 0, bridge->history_capacity * sizeof(event_history_entry_t));
    }

    /* Reset symbol relevance */
    if (bridge->symbol_relevance) {
        memset(bridge->symbol_relevance, 0, bridge->symbol_count * sizeof(symbol_relevance_entry_t));
    }

    bridge->last_update_ms = nimcp_time_get_ms();

    /* Reset stats */
    memset(&bridge->stats, 0, sizeof(bridge->stats));

    bridge->state = FIN_SALIENCE_STATE_INITIALIZED;

    nimcp_mutex_unlock(bridge->base.mutex);

    fin_salience_heartbeat("fin_salience_reset", 1.0f);
    return FIN_SALIENCE_ERR_OK;
}

/* ============================================================================
 * Subsystem Setters
 * ============================================================================ */

#define FIN_SALIENCE_SETTER(name, field) \
    int financial_salience_bridge_set_##name(financial_salience_bridge_t* bridge, void* ptr) { \
        if (!bridge || bridge->magic != FINANCIAL_SALIENCE_BRIDGE_MAGIC) { \
            set_error("bridge is NULL in set_" #name); \
            NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_salience_bridge_set_" #name ": bridge is NULL"); \
            return FIN_SALIENCE_ERR_NULL; \
        } \
        nimcp_mutex_lock(bridge->base.mutex); \
        bridge->field = ptr; \
        nimcp_mutex_unlock(bridge->base.mutex); \
        return FIN_SALIENCE_ERR_OK; \
    }

FIN_SALIENCE_SETTER(immune,        immune)
FIN_SALIENCE_SETTER(health_agent,  health_agent)
FIN_SALIENCE_SETTER(kg_wiring,     kg_wiring)
FIN_SALIENCE_SETTER(logger,        logger)
FIN_SALIENCE_SETTER(security,      security)
FIN_SALIENCE_SETTER(bio_router,    bio_router)

/* Security setters for bbb, ethics, lgss, coordinator handled by bridge_base */

/* ============================================================================
 * Core Salience API Implementation
 * ============================================================================ */

int financial_salience_bridge_evaluate(
    financial_salience_bridge_t* bridge,
    const fin_market_event_t* event,
    fin_salience_score_t* score
) {
    if (!bridge || bridge->magic != FINANCIAL_SALIENCE_BRIDGE_MAGIC) {
        set_error("bridge is NULL or invalid");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_salience_bridge_evaluate: invalid bridge");
        return FIN_SALIENCE_ERR_NULL;
    }
    if (!event || !score) {
        set_error("event or score is NULL");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_salience_bridge_evaluate: NULL argument");
        return FIN_SALIENCE_ERR_NULL;
    }

    fin_salience_heartbeat("fin_salience_evaluate", 0.0f);

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

    uint64_t current_time = nimcp_time_get_ms();

    /* Calculate individual salience dimensions */
    score->novelty = calculate_novelty(bridge, event);
    score->surprise = calculate_surprise(event);
    score->urgency = calculate_urgency(event, current_time);
    score->relevance = calculate_relevance(bridge, event);

    /* Combine using normalized weights */
    score->combined = bridge->normalized_weights.novelty_weight * score->novelty +
                      bridge->normalized_weights.surprise_weight * score->surprise +
                      bridge->normalized_weights.urgency_weight * score->urgency +
                      bridge->normalized_weights.relevance_weight * score->relevance;

    score->combined = nimcp_clampf(score->combined, 0.0f, 1.0f);

    /* Update EMA for future novelty calculations */
    update_ema(bridge, event);

    bridge->last_update_ms = current_time;
    bridge->stats.evaluations++;
    bridge->state = FIN_SALIENCE_STATE_ACTIVE;

    nimcp_mutex_unlock(bridge->base.mutex);

    /* KG notification */
    bridge_kg_publish(bridge, KG_MSG_FIN_SALIENCE_EVAL, score, sizeof(*score));

    fin_salience_heartbeat("fin_salience_evaluate", 1.0f);
    bridge->stats.health_heartbeats++;

    return FIN_SALIENCE_ERR_OK;
}

int financial_salience_bridge_filter(
    financial_salience_bridge_t* bridge,
    const fin_market_event_t* events,
    size_t event_count,
    fin_scored_event_t* output,
    size_t* output_count,
    float threshold
) {
    if (!bridge || bridge->magic != FINANCIAL_SALIENCE_BRIDGE_MAGIC) {
        set_error("bridge is NULL or invalid");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_salience_bridge_filter: invalid bridge");
        return FIN_SALIENCE_ERR_NULL;
    }
    if (!events || !output || !output_count) {
        set_error("NULL argument in filter");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_salience_bridge_filter: NULL argument");
        return FIN_SALIENCE_ERR_NULL;
    }
    if (event_count > FIN_SALIENCE_MAX_BATCH) {
        set_error("event_count exceeds FIN_SALIENCE_MAX_BATCH");
        return FIN_SALIENCE_ERR_CAPACITY;
    }

    fin_salience_heartbeat("fin_salience_filter", 0.0f);

    /* Use config threshold if none specified */
    float effective_threshold = (threshold > 0.0f) ? threshold : bridge->config.attention_threshold;

    *output_count = 0;

    for (size_t i = 0; i < event_count; i++) {
        fin_salience_score_t score;
        int rc = financial_salience_bridge_evaluate(bridge, &events[i], &score);
        if (rc != FIN_SALIENCE_ERR_OK) {
            continue;  /* Skip events that fail evaluation */
        }

        if (score.combined >= effective_threshold) {
            output[*output_count].event = events[i];
            output[*output_count].score = score;
            (*output_count)++;
        }
    }

    bridge->stats.filters_applied++;

    /* KG notification */
    bridge_kg_publish(bridge, KG_MSG_FIN_SALIENCE_FILTER, NULL, 0);

    fin_salience_heartbeat("fin_salience_filter", 1.0f);
    bridge->stats.health_heartbeats++;

    return FIN_SALIENCE_ERR_OK;
}

int financial_salience_bridge_rank(
    financial_salience_bridge_t* bridge,
    const fin_market_event_t* events,
    size_t event_count,
    fin_scored_event_t* output,
    size_t top_k,
    size_t* output_count
) {
    if (!bridge || bridge->magic != FINANCIAL_SALIENCE_BRIDGE_MAGIC) {
        set_error("bridge is NULL or invalid");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_salience_bridge_rank: invalid bridge");
        return FIN_SALIENCE_ERR_NULL;
    }
    if (!events || !output || !output_count) {
        set_error("NULL argument in rank");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_salience_bridge_rank: NULL argument");
        return FIN_SALIENCE_ERR_NULL;
    }
    if (event_count > FIN_SALIENCE_MAX_BATCH) {
        set_error("event_count exceeds FIN_SALIENCE_MAX_BATCH");
        return FIN_SALIENCE_ERR_CAPACITY;
    }

    fin_salience_heartbeat("fin_salience_rank", 0.0f);

    /* Score all events */
    size_t scored_count = 0;
    for (size_t i = 0; i < event_count; i++) {
        fin_salience_score_t score;
        int rc = financial_salience_bridge_evaluate(bridge, &events[i], &score);
        if (rc != FIN_SALIENCE_ERR_OK) {
            continue;
        }

        output[scored_count].event = events[i];
        output[scored_count].score = score;
        scored_count++;
    }

    /* Sort by combined score (descending) */
    if (scored_count > 1) {
        qsort(output, scored_count, sizeof(fin_scored_event_t), compare_scored_events_desc);
    }

    /* Limit to top_k if specified */
    if (top_k > 0 && scored_count > top_k) {
        *output_count = top_k;
    } else {
        *output_count = scored_count;
    }

    bridge->stats.rankings++;

    /* KG notification */
    bridge_kg_publish(bridge, KG_MSG_FIN_SALIENCE_RANK, NULL, 0);

    fin_salience_heartbeat("fin_salience_rank", 1.0f);
    bridge->stats.health_heartbeats++;

    return FIN_SALIENCE_ERR_OK;
}

int financial_salience_bridge_set_symbol_relevance(
    financial_salience_bridge_t* bridge,
    const char* symbol,
    float relevance
) {
    if (!bridge || bridge->magic != FINANCIAL_SALIENCE_BRIDGE_MAGIC) {
        set_error("bridge is NULL or invalid");
        return FIN_SALIENCE_ERR_NULL;
    }
    if (!symbol) {
        set_error("symbol is NULL");
        return FIN_SALIENCE_ERR_NULL;
    }

    fin_salience_heartbeat("fin_salience_set_relevance", 0.0f);

    nimcp_mutex_lock(bridge->base.mutex);

    /* Find existing entry or create new */
    symbol_relevance_entry_t* entry = find_symbol_entry(bridge, symbol);
    if (entry) {
        entry->relevance = nimcp_clampf(relevance, 0.0f, 1.0f);
        if (relevance <= 0.0f) {
            entry->active = false;  /* Remove if relevance is zero */
        }
    } else if (relevance > 0.0f) {
        /* Find free slot */
        for (size_t i = 0; i < bridge->symbol_count; i++) {
            if (!bridge->symbol_relevance[i].active) {
                strncpy(bridge->symbol_relevance[i].symbol, symbol, FIN_SALIENCE_MAX_SYMBOL - 1);
                bridge->symbol_relevance[i].symbol[FIN_SALIENCE_MAX_SYMBOL - 1] = '\0';
                bridge->symbol_relevance[i].relevance = nimcp_clampf(relevance, 0.0f, 1.0f);
                bridge->symbol_relevance[i].active = true;
                break;
            }
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    fin_salience_heartbeat("fin_salience_set_relevance", 1.0f);
    return FIN_SALIENCE_ERR_OK;
}

int financial_salience_bridge_clear_relevance(financial_salience_bridge_t* bridge) {
    if (!bridge || bridge->magic != FINANCIAL_SALIENCE_BRIDGE_MAGIC) {
        set_error("bridge is NULL or invalid");
        return FIN_SALIENCE_ERR_NULL;
    }

    fin_salience_heartbeat("fin_salience_clear_relevance", 0.0f);

    nimcp_mutex_lock(bridge->base.mutex);

    if (bridge->symbol_relevance) {
        memset(bridge->symbol_relevance, 0, bridge->symbol_count * sizeof(symbol_relevance_entry_t));
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    fin_salience_heartbeat("fin_salience_clear_relevance", 1.0f);
    return FIN_SALIENCE_ERR_OK;
}

/* ============================================================================
 * Query Implementation
 * ============================================================================ */

fin_salience_bridge_state_t financial_salience_bridge_get_state(
    const financial_salience_bridge_t* bridge
) {
    if (!bridge || bridge->magic != FINANCIAL_SALIENCE_BRIDGE_MAGIC) {
        return FIN_SALIENCE_STATE_ERROR;
    }
    fin_salience_heartbeat("fin_salience_get_state", 0.0f);
    return bridge->state;
}

int financial_salience_bridge_get_stats(
    const financial_salience_bridge_t* bridge,
    fin_salience_bridge_stats_t* stats
) {
    if (!bridge || bridge->magic != FINANCIAL_SALIENCE_BRIDGE_MAGIC || !stats) {
        set_error("NULL argument in get_stats");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "get_stats: NULL argument");
        return FIN_SALIENCE_ERR_NULL;
    }

    fin_salience_heartbeat("fin_salience_get_stats", 0.0f);

    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);

    return FIN_SALIENCE_ERR_OK;
}

void financial_salience_bridge_reset_stats(financial_salience_bridge_t* bridge) {
    if (!bridge || bridge->magic != FINANCIAL_SALIENCE_BRIDGE_MAGIC) {
        return;
    }

    fin_salience_heartbeat("fin_salience_reset_stats", 0.0f);

    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    nimcp_mutex_unlock(bridge->base.mutex);
}

const char* financial_salience_bridge_get_last_error(void) {
    return fin_salience_last_error;
}

int financial_salience_bridge_get_weights(
    const financial_salience_bridge_t* bridge,
    fin_salience_weights_t* weights
) {
    if (!bridge || bridge->magic != FINANCIAL_SALIENCE_BRIDGE_MAGIC || !weights) {
        set_error("NULL argument in get_weights");
        return FIN_SALIENCE_ERR_NULL;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    *weights = bridge->normalized_weights;
    nimcp_mutex_unlock(bridge->base.mutex);

    return FIN_SALIENCE_ERR_OK;
}

int financial_salience_bridge_set_weights(
    financial_salience_bridge_t* bridge,
    const fin_salience_weights_t* weights
) {
    if (!bridge || bridge->magic != FINANCIAL_SALIENCE_BRIDGE_MAGIC || !weights) {
        set_error("NULL argument in set_weights");
        return FIN_SALIENCE_ERR_NULL;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->config.weights = *weights;
    normalize_weights(weights, &bridge->normalized_weights);
    nimcp_mutex_unlock(bridge->base.mutex);

    return FIN_SALIENCE_ERR_OK;
}

/* ============================================================================
 * Health Integration
 * ============================================================================ */

int financial_salience_bridge_heartbeat(
    financial_salience_bridge_t* bridge,
    const char* operation,
    float progress
) {
    if (!bridge || bridge->magic != FINANCIAL_SALIENCE_BRIDGE_MAGIC) {
        return FIN_SALIENCE_ERR_NULL;
    }

    /* Forward to global health agent */
    fin_salience_heartbeat(operation ? operation : "fin_salience_heartbeat", progress);

    /* Forward to instance-level health agent */
    if (bridge->health_agent) {
        nimcp_health_agent_heartbeat_ex(
            (nimcp_health_agent_t*)bridge->health_agent, operation, progress);
    }

    bridge->stats.health_heartbeats++;
    return FIN_SALIENCE_ERR_OK;
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

const char* fin_salience_event_name(fin_salience_event_type_t event_type) {
    if (event_type >= FIN_SAL_EVENT_COUNT) {
        return "unknown";
    }
    return event_type_names[event_type];
}

const char* fin_salience_state_name(fin_salience_bridge_state_t state) {
    if (state > FIN_SALIENCE_STATE_ERROR) {
        return "unknown";
    }
    return state_names[state];
}

const char* financial_salience_bridge_version(void) {
    return FINANCIAL_SALIENCE_BRIDGE_VERSION;
}

/* ============================================================================
 * Training Integration (B23 Upgrade Compatibility)
 * ============================================================================ */

int financial_salience_bridge_training_begin(financial_salience_bridge_t* bridge) {
    if (!bridge || bridge->magic != FINANCIAL_SALIENCE_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "financial_salience_bridge_training_begin: NULL argument");
        return -1;
    }
    fin_salience_heartbeat_instance((nimcp_health_agent_t*)bridge->health_agent,
                                     "financial_salience_bridge_training_begin", 0.0f);
    return 0;
}

int financial_salience_bridge_training_end(financial_salience_bridge_t* bridge) {
    if (!bridge || bridge->magic != FINANCIAL_SALIENCE_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "financial_salience_bridge_training_end: NULL argument");
        return -1;
    }
    fin_salience_heartbeat_instance((nimcp_health_agent_t*)bridge->health_agent,
                                     "financial_salience_bridge_training_end", 1.0f);
    return 0;
}

int financial_salience_bridge_training_step(financial_salience_bridge_t* bridge, float progress) {
    if (!bridge || bridge->magic != FINANCIAL_SALIENCE_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "financial_salience_bridge_training_step: NULL argument");
        return -1;
    }

    /* Safety gates: ethics + LGSS pre-check */
    BRIDGE_ETHICS_GATE(bridge, "financial_salience_bridge_training_step");
    BRIDGE_LGSS_GATE(bridge, "financial_salience_bridge_training_step");

    fin_salience_heartbeat_instance((nimcp_health_agent_t*)bridge->health_agent,
                                     "financial_salience_bridge_training_step", progress);

    /* Notify coordinator of update cycle completion */
    bridge_base_notify_coordinator_tick(&bridge->base, 0);
    return 0;
}
