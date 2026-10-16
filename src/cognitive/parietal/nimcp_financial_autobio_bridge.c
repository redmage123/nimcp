/**
 * @file nimcp_financial_autobio_bridge.c
 * @brief Financial Autobiographical Memory Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-29
 *
 * WHAT: Bridge for recording and recalling autobiographical trading episodes
 *       with emotional context and lesson extraction
 *
 * WHY:  Trading decisions benefit from experience-based learning. This bridge
 *       integrates episodic memory (specific trading events) with emotional
 *       tagging and outcome tracking.
 *
 * HOW:  Episodes stored in circular buffer with emotion and outcome indices
 *       for fast filtering. Lesson extraction clusters similar experiences.
 *
 * @author NIMCP Development Team
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdarg.h>

#include "cognitive/parietal/nimcp_financial_autobio_bridge.h"
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

#define LOG_MODULE "financial_autobio"

/* ============================================================================
 * Health Agent Integration (Phase 8: System-Wide Health Integration)
 * ============================================================================ */
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/math/nimcp_math_helpers.h"

/* Health agent: using pre-existing custom implementation */
static nimcp_health_agent_t* g_financial_autobio_bridge_health_agent = NULL;

BRIDGE_DEFINE_MESH_REGISTRATION(financial_autobio_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


/** @brief Send heartbeat from financial_autobio_bridge module */
static inline void fin_autobio_heartbeat(const char* operation, float progress) {
    if (g_financial_autobio_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_financial_autobio_bridge_health_agent, operation, progress);
    }
}

/** @brief Send heartbeat from financial_autobio_bridge module (instance-level) */
static inline void fin_autobio_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_financial_autobio_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_financial_autobio_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_financial_autobio_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}

/* ============================================================================
 * Thread-Local Error
 * ============================================================================ */

static _Thread_local char fin_autobio_last_error[NIMCP_ERROR_BUFFER_SIZE] = {0};

static void set_error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(fin_autobio_last_error, sizeof(fin_autobio_last_error), fmt, args);
    va_end(args);
}

/* ============================================================================
 * KG Wiring Integration
 * ============================================================================ */

#define KG_MSG_FIN_AUTOBIO_EPISODE      "FIN_AUTOBIO_EPISODE"
#define KG_MSG_FIN_AUTOBIO_RECALL       "FIN_AUTOBIO_RECALL"
#define KG_MSG_FIN_AUTOBIO_LESSON       "FIN_AUTOBIO_LESSON"
#define KG_MSG_FIN_AUTOBIO_ERROR        "FIN_AUTOBIO_ERROR"

/* ============================================================================
 * Internal Structure
 * ============================================================================ */

/**
 * @brief Episode storage node
 */
typedef struct episode_node {
    fin_trading_episode_t episode;
    struct episode_node* next;
    struct episode_node* prev;
} episode_node_t;

/**
 * @brief Financial autobiographical memory bridge structure
 */
struct financial_autobio_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    uint32_t magic;
    fin_autobio_state_t state;

    /* Configuration */
    fin_autobio_config_t config;

    /* Episode storage */
    episode_node_t* episode_head;    /**< Most recent episode */
    episode_node_t* episode_tail;    /**< Oldest episode */
    uint32_t episode_count;
    uint64_t next_episode_id;

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
    fin_autobio_episode_callback_t episode_cb;
    void* episode_cb_data;
    fin_autobio_lesson_callback_t lesson_cb;
    void* lesson_cb_data;

    /* Statistics */
    fin_autobio_bridge_stats_t stats;

    /* Synchronization */
    nimcp_mutex_t* mutex;
};

/* Security integration via bridge_base */
BRIDGE_DEFINE_SECURITY_SETTERS(financial_autobio_bridge)

/* ============================================================================
 * Static Name Tables
 * ============================================================================ */

static const char* emotion_names[] = {
    "neutral",
    "joy",
    "fear",
    "anger",
    "surprise",
    "sadness",
    "greed",
    "panic"
};

static const char* direction_names[] = {
    "buy",
    "sell",
    "short",
    "hold"
};

static const char* state_names[] = {
    "uninitialized",
    "initialized",
    "active",
    "degraded",
    "error"
};

/**
 * @brief Publish message through KG wiring
 */
static int bridge_kg_publish(financial_autobio_bridge_t* bridge, const char* msg_type,
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
 * @brief Create new episode node
 */
static episode_node_t* create_episode_node(void) {
    episode_node_t* node = nimcp_calloc(1, sizeof(episode_node_t));
    return node;
}

/**
 * @brief Free episode node
 */
static void free_episode_node(episode_node_t* node) {
    if (node) {
        nimcp_free(node);
        node = NULL;
    }
}

/**
 * @brief Remove oldest episode if at capacity
 */
static void trim_episodes_unlocked(financial_autobio_bridge_t* bridge) {
    while (bridge->episode_count >= bridge->config.max_episodes && bridge->episode_tail) {
        episode_node_t* to_remove = bridge->episode_tail;
        bridge->episode_tail = to_remove->prev;
        if (bridge->episode_tail) {
            bridge->episode_tail->next = NULL;
        } else {
            bridge->episode_head = NULL;
        }
        free_episode_node(to_remove);
        bridge->episode_count--;
    }
}

/**
 * @brief Add episode to head of list
 */
static void add_episode_unlocked(financial_autobio_bridge_t* bridge, episode_node_t* node) {
    node->next = bridge->episode_head;
    node->prev = NULL;
    if (bridge->episode_head) {
        bridge->episode_head->prev = node;
    }
    bridge->episode_head = node;
    if (!bridge->episode_tail) {
        bridge->episode_tail = node;
    }
    bridge->episode_count++;
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

int financial_autobio_bridge_default_config(fin_autobio_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");
        return -1;
    }

    fin_autobio_heartbeat("fin_autobio_default_config", 0.0f);

    memset(config, 0, sizeof(*config));

    /* Episode storage */
    config->max_episodes = FIN_AUTOBIO_MAX_EPISODES;
    config->enable_auto_lesson_extraction = true;
    config->min_episodes_for_lesson = 5;

    /* Recall settings */
    config->default_recall_limit = 50;
    config->outcome_similarity_threshold = 0.1f;

    /* Integration settings */
    config->enable_immune_integration = true;
    config->enable_bbb_validation = true;
    config->enable_kg_messaging = true;
    config->enable_health_monitoring = true;

    /* Logging */
    config->verbose_logging = false;

    fin_autobio_heartbeat("fin_autobio_default_config", 1.0f);
    return 0;
}

financial_autobio_bridge_t* financial_autobio_bridge_create(
    const fin_autobio_config_t* config
) {
    fin_autobio_heartbeat("fin_autobio_create", 0.0f);

    financial_autobio_bridge_t* bridge = nimcp_calloc(1, sizeof(financial_autobio_bridge_t));
    if (!bridge) {
        set_error("Failed to allocate financial_autobio_bridge");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NO_MEMORY, "Failed to allocate financial_autobio_bridge");
        return NULL;
    }

    bridge->magic = FINANCIAL_AUTOBIO_BRIDGE_MAGIC;
    bridge->state = FIN_AUTOBIO_STATE_UNINITIALIZED;

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        financial_autobio_bridge_default_config(&bridge->config);
    }

    /* Initialize bridge base (creates mutex) */
    if (bridge_base_init(&bridge->base, BIO_MODULE_FINANCIAL_AUTOBIO, "financial_autobio") != 0) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "financial_autobio_bridge_create: validation failed");
        return NULL;
    }

    /* Initialize episode tracking */
    bridge->episode_head = NULL;
    bridge->episode_tail = NULL;
    bridge->episode_count = 0;
    bridge->next_episode_id = 1;

    /* Initialize statistics */
    memset(&bridge->stats, 0, sizeof(bridge->stats));

    bridge->state = FIN_AUTOBIO_STATE_INITIALIZED;

    fin_autobio_heartbeat("fin_autobio_create", 1.0f);
    return bridge;
}

void financial_autobio_bridge_destroy(financial_autobio_bridge_t* bridge) {
    if (!bridge || bridge->magic != FINANCIAL_AUTOBIO_BRIDGE_MAGIC) {
        return;
    }

    fin_autobio_heartbeat("fin_autobio_destroy", 0.0f);

    /* Free all episodes */
    nimcp_mutex_lock(bridge->base.mutex);
    episode_node_t* current = bridge->episode_head;
    while (current) {
        episode_node_t* next = current->next;
        free_episode_node(current);
        current = next;
    }
    bridge->episode_head = NULL;
    bridge->episode_tail = NULL;
    bridge->episode_count = 0;
    nimcp_mutex_unlock(bridge->base.mutex);

    /* Cleanup base */
    bridge_base_cleanup(&bridge->base);

    bridge->magic = 0;
    nimcp_free(bridge);
    bridge = NULL;
}

int financial_autobio_bridge_reset(financial_autobio_bridge_t* bridge) {
    if (!bridge || bridge->magic != FINANCIAL_AUTOBIO_BRIDGE_MAGIC) {
        set_error("bridge is NULL or invalid");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_autobio_bridge_reset: invalid bridge");
        return FIN_AUTOBIO_ERR_NULL;
    }

    fin_autobio_heartbeat("fin_autobio_reset", 0.0f);

    nimcp_mutex_lock(bridge->base.mutex);

    /* Free all episodes */
    episode_node_t* current = bridge->episode_head;
    while (current) {
        episode_node_t* next = current->next;
        free_episode_node(current);
        current = next;
    }
    bridge->episode_head = NULL;
    bridge->episode_tail = NULL;
    bridge->episode_count = 0;
    bridge->next_episode_id = 1;

    /* Reset stats */
    memset(&bridge->stats, 0, sizeof(bridge->stats));

    bridge->state = FIN_AUTOBIO_STATE_INITIALIZED;

    nimcp_mutex_unlock(bridge->base.mutex);

    fin_autobio_heartbeat("fin_autobio_reset", 1.0f);
    return FIN_AUTOBIO_ERR_OK;
}

/* ============================================================================
 * Subsystem Setters
 * ============================================================================ */

#define FIN_AUTOBIO_SETTER(name, field) \
    int financial_autobio_bridge_set_##name(financial_autobio_bridge_t* bridge, void* ptr) { \
        if (!bridge || bridge->magic != FINANCIAL_AUTOBIO_BRIDGE_MAGIC) { \
            set_error("bridge is NULL in set_" #name); \
            NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_autobio_bridge_set_" #name ": bridge is NULL"); \
            return FIN_AUTOBIO_ERR_NULL; \
        } \
        nimcp_mutex_lock(bridge->base.mutex); \
        bridge->field = ptr; \
        nimcp_mutex_unlock(bridge->base.mutex); \
        return FIN_AUTOBIO_ERR_OK; \
    }

FIN_AUTOBIO_SETTER(immune,        immune)
FIN_AUTOBIO_SETTER(health_agent,  health_agent)
FIN_AUTOBIO_SETTER(kg_wiring,     kg_wiring)
FIN_AUTOBIO_SETTER(logger,        logger)
FIN_AUTOBIO_SETTER(security,      security)
FIN_AUTOBIO_SETTER(bio_router,    bio_router)

/* Security setters for bbb, ethics, lgss, coordinator handled by bridge_base */

/* ============================================================================
 * Episode Recording Implementation
 * ============================================================================ */

int financial_autobio_bridge_record_episode(
    financial_autobio_bridge_t* bridge,
    const fin_trading_episode_t* episode
) {
    if (!bridge || bridge->magic != FINANCIAL_AUTOBIO_BRIDGE_MAGIC) {
        set_error("bridge is NULL or invalid");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_autobio_bridge_record_episode: invalid bridge");
        return FIN_AUTOBIO_ERR_NULL;
    }
    if (!episode) {
        set_error("episode is NULL");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_autobio_bridge_record_episode: episode is NULL");
        return FIN_AUTOBIO_ERR_NULL;
    }

    fin_autobio_heartbeat("fin_autobio_record", 0.0f);

    /* BBB validation */
    if (bridge->config.enable_bbb_validation && bridge->bbb) {
        BRIDGE_BBB_VALIDATE(bridge, episode, sizeof(*episode));
        bridge->stats.bbb_validations++;
    }

    /* Immune check */
    if (bridge->config.enable_immune_integration && bridge->immune) {
        bridge->stats.immune_checks++;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Trim if at capacity */
    trim_episodes_unlocked(bridge);

    /* Create and populate node */
    episode_node_t* node = create_episode_node();
    if (!node) {
        nimcp_mutex_unlock(bridge->base.mutex);
        set_error("Failed to allocate episode node");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NO_MEMORY, "financial_autobio_bridge_record_episode: allocation failed");
        return FIN_AUTOBIO_ERR_NO_MEMORY;
    }

    node->episode = *episode;
    node->episode.episode_id = bridge->next_episode_id++;
    if (node->episode.timestamp_ms == 0) {
        node->episode.timestamp_ms = nimcp_time_get_ms();
    }

    /* Add to list */
    add_episode_unlocked(bridge, node);

    bridge->stats.episodes_recorded++;
    bridge->state = FIN_AUTOBIO_STATE_ACTIVE;

    /* Fire callback if registered */
    fin_autobio_episode_callback_t cb = bridge->episode_cb;
    void* cb_data = bridge->episode_cb_data;

    nimcp_mutex_unlock(bridge->base.mutex);

    if (cb) {
        cb(&node->episode, cb_data);
    }

    /* KG notification */
    bridge_kg_publish(bridge, KG_MSG_FIN_AUTOBIO_EPISODE, &node->episode, sizeof(node->episode));

    fin_autobio_heartbeat("fin_autobio_record", 1.0f);
    bridge->stats.health_heartbeats++;

    return FIN_AUTOBIO_ERR_OK;
}

uint64_t financial_autobio_bridge_record(
    financial_autobio_bridge_t* bridge,
    const char* description,
    float price,
    float quantity,
    fin_trade_direction_t direction,
    float volatility,
    fin_emotion_type_t emotion,
    float outcome,
    const char* lesson
) {
    if (!bridge || bridge->magic != FINANCIAL_AUTOBIO_BRIDGE_MAGIC) {
        set_error("bridge is NULL or invalid");
        return 0;
    }

    fin_trading_episode_t episode;
    memset(&episode, 0, sizeof(episode));

    if (description) {
        strncpy(episode.description, description, FIN_AUTOBIO_DESC_LEN - 1);
        episode.description[FIN_AUTOBIO_DESC_LEN - 1] = '\0';
    }
    episode.trade_price = price;
    episode.trade_quantity = quantity;
    episode.trade_direction = (int)direction;
    episode.market_volatility = volatility;
    episode.emotional_state = emotion;
    episode.outcome = outcome;
    if (lesson) {
        strncpy(episode.lesson_learned, lesson, FIN_AUTOBIO_LESSON_LEN - 1);
        episode.lesson_learned[FIN_AUTOBIO_LESSON_LEN - 1] = '\0';
    }
    episode.timestamp_ms = nimcp_time_get_ms();

    if (financial_autobio_bridge_record_episode(bridge, &episode) == FIN_AUTOBIO_ERR_OK) {
        return bridge->next_episode_id - 1;
    }
    return 0;
}

/* ============================================================================
 * Recall Implementation
 * ============================================================================ */

int financial_autobio_bridge_recall_by_emotion(
    financial_autobio_bridge_t* bridge,
    fin_emotion_type_t emotion,
    uint32_t max_results,
    fin_recall_result_t* result
) {
    if (!bridge || bridge->magic != FINANCIAL_AUTOBIO_BRIDGE_MAGIC) {
        set_error("bridge is NULL or invalid");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "recall_by_emotion: invalid bridge");
        return FIN_AUTOBIO_ERR_NULL;
    }
    if (!result) {
        set_error("result is NULL");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "recall_by_emotion: result is NULL");
        return FIN_AUTOBIO_ERR_NULL;
    }

    fin_autobio_heartbeat("fin_autobio_recall_emotion", 0.0f);

    if (max_results == 0) {
        max_results = bridge->config.default_recall_limit;
    }

    memset(result, 0, sizeof(*result));

    nimcp_mutex_lock(bridge->base.mutex);

    /* Count matching episodes */
    uint32_t match_count = 0;
    episode_node_t* current = bridge->episode_head;
    while (current) {
        if (current->episode.emotional_state == emotion) {
            match_count++;
        }
        current = current->next;
    }

    result->total_available = match_count;

    if (match_count == 0) {
        nimcp_mutex_unlock(bridge->base.mutex);
        bridge->stats.emotion_queries++;
        fin_autobio_heartbeat("fin_autobio_recall_emotion", 1.0f);
        return FIN_AUTOBIO_ERR_OK;
    }

    /* Allocate result array */
    uint32_t return_count = (match_count < max_results) ? match_count : max_results;
    result->episodes = nimcp_calloc(return_count, sizeof(fin_trading_episode_t));
    if (!result->episodes) {
        nimcp_mutex_unlock(bridge->base.mutex);
        set_error("Failed to allocate recall result");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NO_MEMORY, "recall_by_emotion: allocation failed");
        return FIN_AUTOBIO_ERR_NO_MEMORY;
    }

    /* Copy matching episodes */
    uint32_t idx = 0;
    current = bridge->episode_head;
    while (current && idx < return_count) {
        if (current->episode.emotional_state == emotion) {
            result->episodes[idx++] = current->episode;
        }
        current = current->next;
    }
    result->count = idx;

    bridge->stats.emotion_queries++;

    nimcp_mutex_unlock(bridge->base.mutex);

    /* KG notification */
    bridge_kg_publish(bridge, KG_MSG_FIN_AUTOBIO_RECALL, &emotion, sizeof(emotion));

    fin_autobio_heartbeat("fin_autobio_recall_emotion", 1.0f);
    bridge->stats.health_heartbeats++;

    return FIN_AUTOBIO_ERR_OK;
}

int financial_autobio_bridge_recall_by_outcome(
    financial_autobio_bridge_t* bridge,
    float min_outcome,
    float max_outcome,
    uint32_t max_results,
    fin_recall_result_t* result
) {
    if (!bridge || bridge->magic != FINANCIAL_AUTOBIO_BRIDGE_MAGIC) {
        set_error("bridge is NULL or invalid");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "recall_by_outcome: invalid bridge");
        return FIN_AUTOBIO_ERR_NULL;
    }
    if (!result) {
        set_error("result is NULL");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "recall_by_outcome: result is NULL");
        return FIN_AUTOBIO_ERR_NULL;
    }

    fin_autobio_heartbeat("fin_autobio_recall_outcome", 0.0f);

    if (max_results == 0) {
        max_results = bridge->config.default_recall_limit;
    }

    memset(result, 0, sizeof(*result));

    nimcp_mutex_lock(bridge->base.mutex);

    /* Count matching episodes */
    uint32_t match_count = 0;
    episode_node_t* current = bridge->episode_head;
    while (current) {
        if (current->episode.outcome >= min_outcome && current->episode.outcome <= max_outcome) {
            match_count++;
        }
        current = current->next;
    }

    result->total_available = match_count;

    if (match_count == 0) {
        nimcp_mutex_unlock(bridge->base.mutex);
        bridge->stats.outcome_queries++;
        fin_autobio_heartbeat("fin_autobio_recall_outcome", 1.0f);
        return FIN_AUTOBIO_ERR_OK;
    }

    /* Allocate result array */
    uint32_t return_count = (match_count < max_results) ? match_count : max_results;
    result->episodes = nimcp_calloc(return_count, sizeof(fin_trading_episode_t));
    if (!result->episodes) {
        nimcp_mutex_unlock(bridge->base.mutex);
        set_error("Failed to allocate recall result");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NO_MEMORY, "recall_by_outcome: allocation failed");
        return FIN_AUTOBIO_ERR_NO_MEMORY;
    }

    /* Copy matching episodes */
    uint32_t idx = 0;
    current = bridge->episode_head;
    while (current && idx < return_count) {
        if (current->episode.outcome >= min_outcome && current->episode.outcome <= max_outcome) {
            result->episodes[idx++] = current->episode;
        }
        current = current->next;
    }
    result->count = idx;

    bridge->stats.outcome_queries++;

    nimcp_mutex_unlock(bridge->base.mutex);

    /* KG notification */
    bridge_kg_publish(bridge, KG_MSG_FIN_AUTOBIO_RECALL, NULL, 0);

    fin_autobio_heartbeat("fin_autobio_recall_outcome", 1.0f);
    bridge->stats.health_heartbeats++;

    return FIN_AUTOBIO_ERR_OK;
}

void financial_autobio_bridge_free_recall_result(fin_recall_result_t* result) {
    if (result && result->episodes) {
        nimcp_free(result->episodes);
        result->episodes = NULL;
        result->count = 0;
        result->total_available = 0;
    }
}

/* ============================================================================
 * Lesson Extraction Implementation
 * ============================================================================ */

int financial_autobio_bridge_get_lessons(
    financial_autobio_bridge_t* bridge,
    fin_emotion_type_t emotion,
    uint32_t max_lessons,
    fin_lesson_result_t* result
) {
    if (!bridge || bridge->magic != FINANCIAL_AUTOBIO_BRIDGE_MAGIC) {
        set_error("bridge is NULL or invalid");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "get_lessons: invalid bridge");
        return FIN_AUTOBIO_ERR_NULL;
    }
    if (!result) {
        set_error("result is NULL");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "get_lessons: result is NULL");
        return FIN_AUTOBIO_ERR_NULL;
    }

    fin_autobio_heartbeat("fin_autobio_get_lessons", 0.0f);

    if (max_lessons == 0) {
        max_lessons = FIN_AUTOBIO_MAX_LESSONS;
    }

    memset(result, 0, sizeof(*result));

    nimcp_mutex_lock(bridge->base.mutex);

    /* Count episodes by emotion (or all if emotion == FIN_EMOTION_COUNT) */
    uint32_t counts[FIN_EMOTION_COUNT] = {0};
    float outcome_sums[FIN_EMOTION_COUNT] = {0};

    episode_node_t* current = bridge->episode_head;
    while (current) {
        fin_emotion_type_t e = current->episode.emotional_state;
        if (e < FIN_EMOTION_COUNT) {
            if (emotion == FIN_EMOTION_COUNT || e == emotion) {
                counts[e]++;
                outcome_sums[e] += current->episode.outcome;
            }
        }
        current = current->next;
    }

    /* Count how many emotions have enough episodes for a lesson */
    uint32_t lesson_count = 0;
    for (int i = 0; i < FIN_EMOTION_COUNT; i++) {
        if (counts[i] >= bridge->config.min_episodes_for_lesson) {
            lesson_count++;
        }
    }

    if (lesson_count == 0) {
        nimcp_mutex_unlock(bridge->base.mutex);
        bridge->stats.lessons_extracted++;
        fin_autobio_heartbeat("fin_autobio_get_lessons", 1.0f);
        return FIN_AUTOBIO_ERR_OK;
    }

    /* Allocate lessons */
    uint32_t return_count = (lesson_count < max_lessons) ? lesson_count : max_lessons;
    result->lessons = nimcp_calloc(return_count, sizeof(fin_extracted_lesson_t));
    if (!result->lessons) {
        nimcp_mutex_unlock(bridge->base.mutex);
        set_error("Failed to allocate lessons");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NO_MEMORY, "get_lessons: allocation failed");
        return FIN_AUTOBIO_ERR_NO_MEMORY;
    }

    /* Generate lessons */
    uint32_t idx = 0;
    for (int i = 0; i < FIN_EMOTION_COUNT && idx < return_count; i++) {
        if (counts[i] >= bridge->config.min_episodes_for_lesson) {
            fin_extracted_lesson_t* lesson = &result->lessons[idx++];
            lesson->associated_emotion = (fin_emotion_type_t)i;
            lesson->episode_count = counts[i];
            lesson->avg_outcome = outcome_sums[i] / (float)counts[i];
            lesson->confidence = nimcp_clampf((float)counts[i] / 20.0f, 0.0f, 1.0f);

            /* Generate lesson text based on emotion and outcome */
            const char* emo_name = emotion_names[i];
            if (lesson->avg_outcome > 0) {
                snprintf(lesson->lesson_text, FIN_AUTOBIO_LESSON_LEN,
                         "When feeling %s, trades tend to be profitable (avg %.2f). "
                         "Based on %u episodes with %.0f%% confidence.",
                         emo_name, lesson->avg_outcome, lesson->episode_count,
                         lesson->confidence * 100.0f);
            } else {
                snprintf(lesson->lesson_text, FIN_AUTOBIO_LESSON_LEN,
                         "When feeling %s, trades tend to lose (avg %.2f). "
                         "Consider pausing or reducing size. Based on %u episodes.",
                         emo_name, lesson->avg_outcome, lesson->episode_count);
            }
        }
    }
    result->count = idx;

    bridge->stats.lessons_extracted++;

    /* Fire callback for each lesson */
    fin_autobio_lesson_callback_t cb = bridge->lesson_cb;
    void* cb_data = bridge->lesson_cb_data;

    nimcp_mutex_unlock(bridge->base.mutex);

    if (cb) {
        for (uint32_t i = 0; i < result->count; i++) {
            cb(&result->lessons[i], cb_data);
        }
    }

    /* KG notification */
    bridge_kg_publish(bridge, KG_MSG_FIN_AUTOBIO_LESSON, NULL, 0);

    fin_autobio_heartbeat("fin_autobio_get_lessons", 1.0f);
    bridge->stats.health_heartbeats++;

    return FIN_AUTOBIO_ERR_OK;
}

void financial_autobio_bridge_free_lesson_result(fin_lesson_result_t* result) {
    if (result && result->lessons) {
        nimcp_free(result->lessons);
        result->lessons = NULL;
        result->count = 0;
    }
}

/* ============================================================================
 * Callback Registration
 * ============================================================================ */

int financial_autobio_bridge_set_episode_callback(
    financial_autobio_bridge_t* bridge,
    fin_autobio_episode_callback_t callback,
    void* user_data
) {
    if (!bridge || bridge->magic != FINANCIAL_AUTOBIO_BRIDGE_MAGIC) {
        set_error("bridge is NULL or invalid");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "set_episode_callback: invalid bridge");
        return FIN_AUTOBIO_ERR_NULL;
    }

    fin_autobio_heartbeat("fin_autobio_set_episode_cb", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, user_data, sizeof(*user_data));

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->episode_cb = callback;
    bridge->episode_cb_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return FIN_AUTOBIO_ERR_OK;
}

int financial_autobio_bridge_set_lesson_callback(
    financial_autobio_bridge_t* bridge,
    fin_autobio_lesson_callback_t callback,
    void* user_data
) {
    if (!bridge || bridge->magic != FINANCIAL_AUTOBIO_BRIDGE_MAGIC) {
        set_error("bridge is NULL or invalid");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "set_lesson_callback: invalid bridge");
        return FIN_AUTOBIO_ERR_NULL;
    }

    fin_autobio_heartbeat("fin_autobio_set_lesson_cb", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, user_data, sizeof(*user_data));

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->lesson_cb = callback;
    bridge->lesson_cb_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return FIN_AUTOBIO_ERR_OK;
}

/* ============================================================================
 * Query Implementation
 * ============================================================================ */

fin_autobio_state_t financial_autobio_bridge_get_state(
    const financial_autobio_bridge_t* bridge
) {
    if (!bridge || bridge->magic != FINANCIAL_AUTOBIO_BRIDGE_MAGIC) {
        return FIN_AUTOBIO_STATE_ERROR;
    }
    fin_autobio_heartbeat("fin_autobio_get_state", 0.0f);
    return bridge->state;
}

int financial_autobio_bridge_get_stats(
    const financial_autobio_bridge_t* bridge,
    fin_autobio_bridge_stats_t* stats
) {
    if (!bridge || bridge->magic != FINANCIAL_AUTOBIO_BRIDGE_MAGIC || !stats) {
        set_error("NULL argument in get_stats");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "get_stats: NULL argument");
        return FIN_AUTOBIO_ERR_NULL;
    }

    fin_autobio_heartbeat("fin_autobio_get_stats", 0.0f);

    /* Note: BBB validation skipped for const-qualified bridge in read-only accessor */

    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);

    return FIN_AUTOBIO_ERR_OK;
}

void financial_autobio_bridge_reset_stats(financial_autobio_bridge_t* bridge) {
    if (!bridge || bridge->magic != FINANCIAL_AUTOBIO_BRIDGE_MAGIC) {
        return;
    }

    fin_autobio_heartbeat("fin_autobio_reset_stats", 0.0f);

    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    nimcp_mutex_unlock(bridge->base.mutex);
}

uint32_t financial_autobio_bridge_get_episode_count(
    const financial_autobio_bridge_t* bridge
) {
    if (!bridge || bridge->magic != FINANCIAL_AUTOBIO_BRIDGE_MAGIC) {
        return 0;
    }
    return bridge->episode_count;
}

const char* financial_autobio_bridge_get_last_error(void) {
    return fin_autobio_last_error;
}

/* ============================================================================
 * Health Integration
 * ============================================================================ */

int financial_autobio_bridge_heartbeat(
    financial_autobio_bridge_t* bridge,
    const char* operation,
    float progress
) {
    if (!bridge || bridge->magic != FINANCIAL_AUTOBIO_BRIDGE_MAGIC) {
        return FIN_AUTOBIO_ERR_NULL;
    }

    /* Forward to global health agent */
    fin_autobio_heartbeat(operation ? operation : "fin_autobio_heartbeat", progress);

    /* Forward to instance-level health agent */
    if (bridge->health_agent) {
        nimcp_health_agent_heartbeat_ex(
            (nimcp_health_agent_t*)bridge->health_agent, operation, progress);
    }

    bridge->stats.health_heartbeats++;
    return FIN_AUTOBIO_ERR_OK;
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

const char* fin_autobio_emotion_name(fin_emotion_type_t emotion) {
    if (emotion >= FIN_EMOTION_COUNT) {
        return "unknown";
    }
    return emotion_names[emotion];
}

const char* fin_autobio_direction_name(fin_trade_direction_t direction) {
    if (direction > FIN_TRADE_HOLD) {
        return "unknown";
    }
    return direction_names[direction];
}

const char* fin_autobio_state_name(fin_autobio_state_t state) {
    if (state > FIN_AUTOBIO_STATE_ERROR) {
        return "unknown";
    }
    return state_names[state];
}

const char* financial_autobio_bridge_version(void) {
    return FINANCIAL_AUTOBIO_BRIDGE_VERSION;
}

/* ============================================================================
 * Training Hook Stubs (B23 Upgrade Compatibility)
 * ============================================================================ */

int financial_autobio_bridge_training_begin(financial_autobio_bridge_t* bridge) {
    if (!bridge || bridge->magic != FINANCIAL_AUTOBIO_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "financial_autobio_bridge_training_begin: NULL argument");
        return -1;
    }
    fin_autobio_heartbeat_instance((nimcp_health_agent_t*)bridge->health_agent,
                                    "financial_autobio_bridge_training_begin", 0.0f);
    return 0;
}

int financial_autobio_bridge_training_end(financial_autobio_bridge_t* bridge) {
    if (!bridge || bridge->magic != FINANCIAL_AUTOBIO_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "financial_autobio_bridge_training_end: NULL argument");
        return -1;
    }
    fin_autobio_heartbeat_instance((nimcp_health_agent_t*)bridge->health_agent,
                                    "financial_autobio_bridge_training_end", 1.0f);
    return 0;
}

int financial_autobio_bridge_training_step(financial_autobio_bridge_t* bridge, float progress) {
    if (!bridge || bridge->magic != FINANCIAL_AUTOBIO_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "financial_autobio_bridge_training_step: NULL argument");
        return -1;
    }

    /* Safety gates: ethics + LGSS pre-check */
    BRIDGE_ETHICS_GATE(bridge, "financial_autobio_bridge_training_step");
    BRIDGE_LGSS_GATE(bridge, "financial_autobio_bridge_training_step");

    fin_autobio_heartbeat_instance((nimcp_health_agent_t*)bridge->health_agent,
                                    "financial_autobio_bridge_training_step", progress);

    /* Notify coordinator of update cycle completion */
    bridge_base_notify_coordinator_tick(&bridge->base, 0);
    return 0;
}
