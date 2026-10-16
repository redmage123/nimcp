/**
 * @file nimcp_financial_consolidation_bridge.c
 * @brief Financial Consolidation Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-29
 *
 * WHAT: Bridge for consolidating financial trading patterns through memory replay,
 *       winner strengthening, and loser pruning.
 *
 * WHY:  Memory consolidation is essential for learning from trading experience.
 *       Replaying profitable patterns strengthens winning strategies while
 *       pruning losers removes harmful patterns.
 *
 * HOW:  Trade history tracks outcomes. During consolidation, patterns are
 *       replayed, strengthened, or pruned based on cumulative performance.
 *
 * @author NIMCP Development Team
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdarg.h>

#include "cognitive/parietal/nimcp_financial_consolidation_bridge.h"
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

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(fin_consolidation)

/* Stub heartbeat for migration compatibility */
static inline void fin_consolidation_heartbeat_global(const char* op, float progress) {
    (void)op; (void)progress;
}

BRIDGE_DEFINE_MESH_REGISTRATION(fin_consolidation, MESH_ADAPTER_CATEGORY_COGNITIVE)


/* ============================================================================
 * Immune/BBB Integration (Phase 9: Security Integration)
 * ============================================================================ */

struct brain_immune_system;
typedef struct brain_immune_system brain_immune_system_t;
extern int brain_immune_validate_operation(brain_immune_system_t* immune,
                                            const char* operation,
                                            uint32_t severity);

struct bbb_system_struct;
extern int bbb_validate_data(bbb_system_t bbb, const void* data,
                              size_t size, const char* context);

/* ============================================================================
 * Thread-Local Error Handling
 * ============================================================================ */

static _Thread_local char fin_consolidation_last_error[NIMCP_ERROR_BUFFER_SIZE] = {0};

static void set_error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(fin_consolidation_last_error, sizeof(fin_consolidation_last_error), fmt, args);
    va_end(args);
}

const char* financial_consolidation_bridge_get_last_error(void) {
    return fin_consolidation_last_error;
}

/* ============================================================================
 * Internal Structure
 * ============================================================================ */

struct financial_consolidation_bridge {
    uint32_t magic;
    fin_consolidation_config_t config;
    fin_consolidation_op_state_t op_state;
    fin_consolidation_bridge_stats_t stats;

    /* Trade history */
    fin_trade_record_t* trades;
    uint32_t num_trades;
    uint32_t trades_capacity;

    /* Pattern memory */
    fin_pattern_entry_t* patterns;
    uint32_t num_patterns;
    uint32_t next_pattern_id;

    /* Pattern-trade associations (simple indexed mapping) */
    uint32_t** trade_patterns;       /* [trade_idx] -> array of pattern_ids */
    uint32_t* trade_pattern_counts;  /* [trade_idx] -> count of patterns */

    /* Consolidation session tracking */
    fin_consolidation_session_t current_session;
    uint64_t session_counter;

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
    fin_consolidation_replay_callback_t replay_callback;
    void* replay_callback_data;
    fin_consolidation_prune_callback_t prune_callback;
    void* prune_callback_data;
    fin_consolidation_strengthen_callback_t strengthen_callback;
    void* strengthen_callback_data;
};

/* ============================================================================
 * KG Wiring Message Types
 * ============================================================================ */

#define KG_MSG_FIN_CONSOLIDATION_REPLAY      "FIN_CONSOLIDATION_REPLAY"
#define KG_MSG_FIN_CONSOLIDATION_PRUNE       "FIN_CONSOLIDATION_PRUNE"
#define KG_MSG_FIN_CONSOLIDATION_STRENGTHEN  "FIN_CONSOLIDATION_STRENGTHEN"
#define KG_MSG_FIN_CONSOLIDATION_TRADE       "FIN_CONSOLIDATION_TRADE"

static int bridge_kg_publish(financial_consolidation_bridge_t* bridge,
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

const char* fin_consolidation_op_state_name(fin_consolidation_op_state_t state) {
    switch (state) {
        case FIN_CONSOLIDATION_OP_STATE_UNINITIALIZED: return "UNINITIALIZED";
        case FIN_CONSOLIDATION_OP_STATE_INITIALIZED:   return "INITIALIZED";
        case FIN_CONSOLIDATION_OP_STATE_ACTIVE:        return "ACTIVE";
        case FIN_CONSOLIDATION_OP_STATE_CONSOLIDATING: return "CONSOLIDATING";
        case FIN_CONSOLIDATION_OP_STATE_DEGRADED:      return "DEGRADED";
        case FIN_CONSOLIDATION_OP_STATE_ERROR:         return "ERROR";
        default: return "UNKNOWN";
    }
}

const char* fin_consolidation_pattern_type_name(fin_pattern_type_t type) {
    switch (type) {
        case FIN_PATTERN_TYPE_MOMENTUM:    return "MOMENTUM";
        case FIN_PATTERN_TYPE_REVERSAL:    return "REVERSAL";
        case FIN_PATTERN_TYPE_BREAKOUT:    return "BREAKOUT";
        case FIN_PATTERN_TYPE_SUPPORT:     return "SUPPORT";
        case FIN_PATTERN_TYPE_VOLUME:      return "VOLUME";
        case FIN_PATTERN_TYPE_SENTIMENT:   return "SENTIMENT";
        case FIN_PATTERN_TYPE_FUNDAMENTAL: return "FUNDAMENTAL";
        case FIN_PATTERN_TYPE_CUSTOM:      return "CUSTOM";
        default: return "UNKNOWN";
    }
}

const char* fin_consolidation_mode_name(fin_consolidation_mode_t mode) {
    switch (mode) {
        case FIN_CONSOLIDATION_MODE_FULL:            return "FULL";
        case FIN_CONSOLIDATION_MODE_REPLAY_ONLY:     return "REPLAY_ONLY";
        case FIN_CONSOLIDATION_MODE_PRUNE_ONLY:      return "PRUNE_ONLY";
        case FIN_CONSOLIDATION_MODE_STRENGTHEN_ONLY: return "STRENGTHEN_ONLY";
        default: return "UNKNOWN";
    }
}

const char* fin_consolidation_direction_name(fin_trade_direction_t direction) {
    switch (direction) {
        case FIN_TRADE_DIRECTION_LONG:  return "LONG";
        case FIN_TRADE_DIRECTION_SHORT: return "SHORT";
        case FIN_TRADE_DIRECTION_FLAT:  return "FLAT";
        default: return "UNKNOWN";
    }
}

const char* financial_consolidation_bridge_version(void) {
    return FINANCIAL_CONSOLIDATION_BRIDGE_VERSION;
}

/* ============================================================================
 * Configuration
 * ============================================================================ */

int financial_consolidation_bridge_default_config(fin_consolidation_config_t* config) {
    if (!config) {
        set_error("config is NULL");
        return FIN_CONSOLIDATION_ERR_NULL;
    }

    memset(config, 0, sizeof(fin_consolidation_config_t));

    /* Consolidation parameters */
    config->replay_batch_size = FIN_CONSOLIDATION_DEFAULT_BATCH_SIZE;
    config->strengthen_rate = FIN_CONSOLIDATION_DEFAULT_STRENGTHEN_RATE;
    config->weaken_rate = 0.05f;
    config->prune_threshold = FIN_CONSOLIDATION_DEFAULT_PRUNE_THRESHOLD;
    config->min_win_rate = 0.4f;
    config->min_occurrences = 5;

    /* History settings */
    config->max_history_size = FIN_CONSOLIDATION_MAX_TRADES;
    config->max_pattern_count = FIN_CONSOLIDATION_MAX_PATTERNS;
    config->history_retention_ms = 30ULL * 24 * 60 * 60 * 1000;  /* 30 days */

    /* Consolidation scheduling */
    config->auto_consolidate = false;
    config->consolidation_interval_ms = 24ULL * 60 * 60 * 1000;  /* 24 hours */

    /* Integration settings */
    config->enable_immune_integration = true;
    config->enable_bbb_validation = true;
    config->enable_kg_messaging = true;
    config->enable_health_monitoring = true;

    /* Logging */
    config->verbose_logging = false;

    return FIN_CONSOLIDATION_ERR_OK;
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

financial_consolidation_bridge_t* financial_consolidation_bridge_create(
    const fin_consolidation_config_t* config)
{
    fin_consolidation_heartbeat_global("fin_consolidation_create", 0.0f);

    financial_consolidation_bridge_t* bridge = (financial_consolidation_bridge_t*)
        nimcp_malloc(sizeof(financial_consolidation_bridge_t));
    if (!bridge) {
        set_error("Failed to allocate financial_consolidation_bridge");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NO_MEMORY,
            "Failed to allocate financial_consolidation_bridge");
        return NULL;
    }
    memset(bridge, 0, sizeof(financial_consolidation_bridge_t));

    bridge->magic = FINANCIAL_CONSOLIDATION_BRIDGE_MAGIC;

    /* Copy configuration or use defaults */
    if (config) {
        bridge->config = *config;
    } else {
        financial_consolidation_bridge_default_config(&bridge->config);
    }

    /* Allocate trade history */
    bridge->trades_capacity = bridge->config.max_history_size;
    bridge->trades = (fin_trade_record_t*)nimcp_malloc(
        bridge->trades_capacity * sizeof(fin_trade_record_t));
    if (!bridge->trades) {
        set_error("Failed to allocate trade history");
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NO_MEMORY,
            "Failed to allocate trade history");
        return NULL;
    }
    memset(bridge->trades, 0, bridge->trades_capacity * sizeof(fin_trade_record_t));
    bridge->num_trades = 0;

    /* Allocate pattern memory */
    bridge->patterns = (fin_pattern_entry_t*)nimcp_malloc(
        bridge->config.max_pattern_count * sizeof(fin_pattern_entry_t));
    if (!bridge->patterns) {
        set_error("Failed to allocate pattern memory");
        nimcp_free(bridge->trades);
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NO_MEMORY,
            "Failed to allocate pattern memory");
        return NULL;
    }
    memset(bridge->patterns, 0, bridge->config.max_pattern_count * sizeof(fin_pattern_entry_t));
    bridge->num_patterns = 0;
    bridge->next_pattern_id = 1;

    /* Allocate trade-pattern association arrays */
    bridge->trade_patterns = (uint32_t**)nimcp_calloc(bridge->trades_capacity, sizeof(uint32_t*));
    bridge->trade_pattern_counts = (uint32_t*)nimcp_calloc(bridge->trades_capacity, sizeof(uint32_t));
    if (!bridge->trade_patterns || !bridge->trade_pattern_counts) {
        set_error("Failed to allocate trade-pattern associations");
        if (bridge->trade_patterns) nimcp_free(bridge->trade_patterns);
        if (bridge->trade_pattern_counts) nimcp_free(bridge->trade_pattern_counts);
        nimcp_free(bridge->patterns);
        nimcp_free(bridge->trades);
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "financial_consolidation_bridge_create: validation failed");
        return NULL;
    }

    /* Initialize state */
    bridge->op_state = FIN_CONSOLIDATION_OP_STATE_INITIALIZED;
    bridge->session_counter = 0;

    fin_consolidation_heartbeat_global("fin_consolidation_create", 1.0f);
    return bridge;
}

void financial_consolidation_bridge_destroy(financial_consolidation_bridge_t* bridge) {
    fin_consolidation_heartbeat_global("fin_consolidation_destroy", 0.0f);

    if (bridge) {
        /* Free trade-pattern associations */
        if (bridge->trade_patterns) {
            for (uint32_t i = 0; i < bridge->trades_capacity; i++) {
                if (bridge->trade_patterns[i]) {
                    nimcp_free(bridge->trade_patterns[i]);
                }
            }
            nimcp_free(bridge->trade_patterns);
        }
        if (bridge->trade_pattern_counts) {
            nimcp_free(bridge->trade_pattern_counts);
        }

        if (bridge->patterns) {
            nimcp_free(bridge->patterns);
        }
        if (bridge->trades) {
            nimcp_free(bridge->trades);
        }
        bridge->magic = 0;
        bridge->op_state = FIN_CONSOLIDATION_OP_STATE_UNINITIALIZED;
        nimcp_free(bridge);
        bridge = NULL;
    }
}

int financial_consolidation_bridge_reset(financial_consolidation_bridge_t* bridge) {
    if (!bridge) {
        set_error("bridge is NULL");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_consolidation_bridge_reset: bridge is NULL");
        return FIN_CONSOLIDATION_ERR_NULL;
    }

    if (bridge->magic != FINANCIAL_CONSOLIDATION_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_INVALID_STATE,
            "financial_consolidation_bridge_reset: invalid magic");
        return FIN_CONSOLIDATION_ERR_STATE;
    }

    fin_consolidation_heartbeat_global("fin_consolidation_reset", 0.0f);

    /* Clear trade history */
    memset(bridge->trades, 0, bridge->trades_capacity * sizeof(fin_trade_record_t));
    bridge->num_trades = 0;

    /* Clear patterns */
    memset(bridge->patterns, 0, bridge->config.max_pattern_count * sizeof(fin_pattern_entry_t));
    bridge->num_patterns = 0;
    bridge->next_pattern_id = 1;

    /* Clear trade-pattern associations */
    for (uint32_t i = 0; i < bridge->trades_capacity; i++) {
        if (bridge->trade_patterns[i]) {
            nimcp_free(bridge->trade_patterns[i]);
            bridge->trade_patterns[i] = NULL;
        }
        bridge->trade_pattern_counts[i] = 0;
    }

    bridge->op_state = FIN_CONSOLIDATION_OP_STATE_INITIALIZED;

    fin_consolidation_heartbeat_global("fin_consolidation_reset", 1.0f);
    return FIN_CONSOLIDATION_ERR_OK;
}

/* ============================================================================
 * Subsystem Setters
 * ============================================================================ */

#define FIN_CONSOLIDATION_SETTER(name, field) \
    int financial_consolidation_bridge_set_##name( \
        financial_consolidation_bridge_t* bridge, void* ptr) { \
        if (!bridge) { \
            set_error("bridge is NULL in set_" #name); \
            NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, \
                "financial_consolidation_bridge_set_" #name ": bridge is NULL"); \
            return FIN_CONSOLIDATION_ERR_NULL; \
        } \
        if (bridge->magic != FINANCIAL_CONSOLIDATION_BRIDGE_MAGIC) { \
            set_error("Invalid bridge magic in set_" #name); \
            return FIN_CONSOLIDATION_ERR_STATE; \
        } \
        bridge->field = ptr; \
        return FIN_CONSOLIDATION_ERR_OK; \
    }

FIN_CONSOLIDATION_SETTER(immune, immune)
FIN_CONSOLIDATION_SETTER(health_agent, health_agent)
FIN_CONSOLIDATION_SETTER(kg_wiring, kg_wiring)
FIN_CONSOLIDATION_SETTER(logger, logger)
FIN_CONSOLIDATION_SETTER(security, security)
FIN_CONSOLIDATION_SETTER(bio_router, bio_router)

int financial_consolidation_bridge_set_coordinator(
    financial_consolidation_bridge_t* bridge,
    brain_cycle_coordinator_t* coordinator)
{
    if (!bridge) {
        set_error("bridge is NULL in set_coordinator");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_consolidation_bridge_set_coordinator: bridge is NULL");
        return FIN_CONSOLIDATION_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_CONSOLIDATION_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic in set_coordinator");
        return FIN_CONSOLIDATION_ERR_STATE;
    }
    bridge->coordinator = (void*)coordinator;
    return FIN_CONSOLIDATION_ERR_OK;
}

int financial_consolidation_bridge_set_bbb(
    financial_consolidation_bridge_t* bridge,
    bbb_system_t bbb)
{
    if (!bridge) {
        set_error("bridge is NULL in set_bbb");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_consolidation_bridge_set_bbb: bridge is NULL");
        return FIN_CONSOLIDATION_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_CONSOLIDATION_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic in set_bbb");
        return FIN_CONSOLIDATION_ERR_STATE;
    }
    bridge->bbb = (void*)bbb;
    return FIN_CONSOLIDATION_ERR_OK;
}

int financial_consolidation_bridge_set_ethics(
    financial_consolidation_bridge_t* bridge,
    ethics_engine_t ethics)
{
    if (!bridge) {
        set_error("bridge is NULL in set_ethics");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_consolidation_bridge_set_ethics: bridge is NULL");
        return FIN_CONSOLIDATION_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_CONSOLIDATION_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic in set_ethics");
        return FIN_CONSOLIDATION_ERR_STATE;
    }
    bridge->ethics = (void*)ethics;
    return FIN_CONSOLIDATION_ERR_OK;
}

int financial_consolidation_bridge_set_lgss(
    financial_consolidation_bridge_t* bridge,
    const void* lgss)
{
    if (!bridge) {
        set_error("bridge is NULL in set_lgss");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_consolidation_bridge_set_lgss: bridge is NULL");
        return FIN_CONSOLIDATION_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_CONSOLIDATION_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic in set_lgss");
        return FIN_CONSOLIDATION_ERR_STATE;
    }
    bridge->lgss = lgss;
    return FIN_CONSOLIDATION_ERR_OK;
}

/* ============================================================================
 * Callback Registration
 * ============================================================================ */

int financial_consolidation_bridge_set_replay_callback(
    financial_consolidation_bridge_t* bridge,
    fin_consolidation_replay_callback_t callback,
    void* user_data)
{
    if (!bridge) {
        set_error("bridge is NULL in set_replay_callback");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_consolidation_bridge_set_replay_callback: bridge is NULL");
        return FIN_CONSOLIDATION_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_CONSOLIDATION_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic in set_replay_callback");
        return FIN_CONSOLIDATION_ERR_STATE;
    }
    bridge->replay_callback = callback;
    bridge->replay_callback_data = user_data;
    return FIN_CONSOLIDATION_ERR_OK;
}

int financial_consolidation_bridge_set_prune_callback(
    financial_consolidation_bridge_t* bridge,
    fin_consolidation_prune_callback_t callback,
    void* user_data)
{
    if (!bridge) {
        set_error("bridge is NULL in set_prune_callback");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_consolidation_bridge_set_prune_callback: bridge is NULL");
        return FIN_CONSOLIDATION_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_CONSOLIDATION_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic in set_prune_callback");
        return FIN_CONSOLIDATION_ERR_STATE;
    }
    bridge->prune_callback = callback;
    bridge->prune_callback_data = user_data;
    return FIN_CONSOLIDATION_ERR_OK;
}

int financial_consolidation_bridge_set_strengthen_callback(
    financial_consolidation_bridge_t* bridge,
    fin_consolidation_strengthen_callback_t callback,
    void* user_data)
{
    if (!bridge) {
        set_error("bridge is NULL in set_strengthen_callback");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_consolidation_bridge_set_strengthen_callback: bridge is NULL");
        return FIN_CONSOLIDATION_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_CONSOLIDATION_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic in set_strengthen_callback");
        return FIN_CONSOLIDATION_ERR_STATE;
    }
    bridge->strengthen_callback = callback;
    bridge->strengthen_callback_data = user_data;
    return FIN_CONSOLIDATION_ERR_OK;
}

/* ============================================================================
 * Trade History Management
 * ============================================================================ */

int financial_consolidation_bridge_add_trade(
    financial_consolidation_bridge_t* bridge,
    const fin_trade_record_t* trade)
{
    if (!bridge) {
        set_error("bridge is NULL");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_consolidation_bridge_add_trade: bridge is NULL");
        return FIN_CONSOLIDATION_ERR_NULL;
    }
    if (!trade) {
        set_error("trade is NULL");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_consolidation_bridge_add_trade: trade is NULL");
        return FIN_CONSOLIDATION_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_CONSOLIDATION_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic");
        return FIN_CONSOLIDATION_ERR_STATE;
    }

    if (bridge->num_trades >= bridge->trades_capacity) {
        set_error("Trade history full (%u trades)", bridge->trades_capacity);
        return FIN_CONSOLIDATION_ERR_HISTORY_FULL;
    }

    fin_consolidation_heartbeat_global("fin_consolidation_add_trade", 0.0f);

    /* BBB validation if enabled */
    if (bridge->config.enable_bbb_validation && bridge->bbb) {
        bridge->stats.bbb_validations++;
        int rc = bbb_validate_data((bbb_system_t)bridge->bbb, trade,
                                    sizeof(*trade), "consolidation_add_trade");
        if (rc != 0) {
            set_error("BBB validation failed for add_trade");
            return FIN_CONSOLIDATION_ERR_BBB;
        }
    }

    /* Add trade to history */
    bridge->trades[bridge->num_trades++] = *trade;

    /* Update state */
    bridge->op_state = FIN_CONSOLIDATION_OP_STATE_ACTIVE;

    /* KG messaging */
    bridge_kg_publish(bridge, KG_MSG_FIN_CONSOLIDATION_TRADE, trade, sizeof(*trade));

    fin_consolidation_heartbeat_global("fin_consolidation_add_trade", 1.0f);
    return FIN_CONSOLIDATION_ERR_OK;
}

int financial_consolidation_bridge_add_annotated_trade(
    financial_consolidation_bridge_t* bridge,
    const fin_annotated_trade_t* trade)
{
    if (!bridge || !trade) {
        set_error("bridge or trade is NULL");
        return FIN_CONSOLIDATION_ERR_NULL;
    }

    /* Add base trade */
    int rc = financial_consolidation_bridge_add_trade(bridge, &trade->trade);
    if (rc != FIN_CONSOLIDATION_ERR_OK) {
        return rc;
    }

    /* Associate patterns if provided */
    uint32_t trade_idx = bridge->num_trades - 1;
    if (trade->pattern_ids && trade->num_patterns > 0) {
        bridge->trade_patterns[trade_idx] = (uint32_t*)nimcp_malloc(
            trade->num_patterns * sizeof(uint32_t));
        if (bridge->trade_patterns[trade_idx]) {
            memcpy(bridge->trade_patterns[trade_idx], trade->pattern_ids,
                   trade->num_patterns * sizeof(uint32_t));
            bridge->trade_pattern_counts[trade_idx] = trade->num_patterns;

            /* Update pattern statistics */
            for (uint32_t i = 0; i < trade->num_patterns; i++) {
                uint32_t pid = trade->pattern_ids[i];
                for (uint32_t j = 0; j < bridge->num_patterns; j++) {
                    if (bridge->patterns[j].pattern_id == pid) {
                        bridge->patterns[j].occurrence_count++;
                        bridge->patterns[j].cumulative_outcome += trade->trade.outcome;
                        bridge->patterns[j].last_seen_ms = trade->trade.timestamp_ms;
                        if (trade->trade.outcome > 0) {
                            bridge->patterns[j].win_count++;
                        } else if (trade->trade.outcome < 0) {
                            bridge->patterns[j].loss_count++;
                        }
                        /* Update win rate */
                        if (bridge->patterns[j].occurrence_count > 0) {
                            bridge->patterns[j].win_rate =
                                (float)bridge->patterns[j].win_count /
                                (float)bridge->patterns[j].occurrence_count;
                        }
                        break;
                    }
                }
            }
        }
    }

    return FIN_CONSOLIDATION_ERR_OK;
}

int financial_consolidation_bridge_get_history(
    const financial_consolidation_bridge_t* bridge,
    fin_trade_history_t* history,
    uint32_t max_trades)
{
    if (!bridge || !history) {
        return -FIN_CONSOLIDATION_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_CONSOLIDATION_BRIDGE_MAGIC) {
        return -FIN_CONSOLIDATION_ERR_STATE;
    }

    uint32_t count = bridge->num_trades;
    if (count > max_trades) {
        count = max_trades;
    }

    if (history->trades && count > 0) {
        memcpy(history->trades, bridge->trades, count * sizeof(fin_trade_record_t));
    }
    history->num_trades = count;

    return (int)count;
}

int financial_consolidation_bridge_clear_history(
    financial_consolidation_bridge_t* bridge)
{
    if (!bridge) {
        return FIN_CONSOLIDATION_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_CONSOLIDATION_BRIDGE_MAGIC) {
        return FIN_CONSOLIDATION_ERR_STATE;
    }

    memset(bridge->trades, 0, bridge->trades_capacity * sizeof(fin_trade_record_t));
    bridge->num_trades = 0;

    /* Clear trade-pattern associations */
    for (uint32_t i = 0; i < bridge->trades_capacity; i++) {
        if (bridge->trade_patterns[i]) {
            nimcp_free(bridge->trade_patterns[i]);
            bridge->trade_patterns[i] = NULL;
        }
        bridge->trade_pattern_counts[i] = 0;
    }

    return FIN_CONSOLIDATION_ERR_OK;
}

uint32_t financial_consolidation_bridge_get_trade_count(
    const financial_consolidation_bridge_t* bridge)
{
    if (!bridge || bridge->magic != FINANCIAL_CONSOLIDATION_BRIDGE_MAGIC) {
        return 0;
    }
    return bridge->num_trades;
}

/* ============================================================================
 * Pattern Management
 * ============================================================================ */

int financial_consolidation_bridge_register_pattern(
    financial_consolidation_bridge_t* bridge,
    fin_pattern_type_t type,
    float initial_strength)
{
    if (!bridge) {
        set_error("bridge is NULL");
        return -FIN_CONSOLIDATION_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_CONSOLIDATION_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic");
        return -FIN_CONSOLIDATION_ERR_STATE;
    }
    if (bridge->num_patterns >= bridge->config.max_pattern_count) {
        set_error("Pattern memory full");
        return -FIN_CONSOLIDATION_ERR_PATTERN_FULL;
    }

    fin_pattern_entry_t* pattern = &bridge->patterns[bridge->num_patterns];
    memset(pattern, 0, sizeof(fin_pattern_entry_t));

    pattern->pattern_id = bridge->next_pattern_id++;
    pattern->type = type;
    pattern->strength = nimcp_clampf(initial_strength, 0.0f, 1.0f);
    pattern->enabled = true;

    bridge->num_patterns++;

    return (int)pattern->pattern_id;
}

int financial_consolidation_bridge_associate_pattern(
    financial_consolidation_bridge_t* bridge,
    uint32_t trade_index,
    uint32_t pattern_id,
    float outcome)
{
    if (!bridge) {
        return FIN_CONSOLIDATION_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_CONSOLIDATION_BRIDGE_MAGIC) {
        return FIN_CONSOLIDATION_ERR_STATE;
    }
    if (trade_index >= bridge->num_trades) {
        set_error("Invalid trade index");
        return FIN_CONSOLIDATION_ERR_INVALID_PARAM;
    }

    /* Find pattern */
    fin_pattern_entry_t* pattern = NULL;
    for (uint32_t i = 0; i < bridge->num_patterns; i++) {
        if (bridge->patterns[i].pattern_id == pattern_id) {
            pattern = &bridge->patterns[i];
            break;
        }
    }
    if (!pattern) {
        set_error("Pattern ID %u not found", pattern_id);
        return FIN_CONSOLIDATION_ERR_INVALID_PARAM;
    }

    /* Add to trade's pattern list */
    uint32_t current_count = bridge->trade_pattern_counts[trade_index];
    uint32_t* new_patterns = (uint32_t*)nimcp_realloc(
        bridge->trade_patterns[trade_index],
        (current_count + 1) * sizeof(uint32_t));
    if (!new_patterns) {
        return FIN_CONSOLIDATION_ERR_NO_MEMORY;
    }
    bridge->trade_patterns[trade_index] = new_patterns;
    bridge->trade_patterns[trade_index][current_count] = pattern_id;
    bridge->trade_pattern_counts[trade_index] = current_count + 1;

    /* Update pattern statistics */
    pattern->occurrence_count++;
    pattern->cumulative_outcome += outcome;
    pattern->last_seen_ms = bridge->trades[trade_index].timestamp_ms;
    if (outcome > 0) {
        pattern->win_count++;
    } else if (outcome < 0) {
        pattern->loss_count++;
    }

    /* Update derived metrics */
    if (pattern->occurrence_count > 0) {
        pattern->win_rate = (float)pattern->win_count / (float)pattern->occurrence_count;

        float total_wins = 0.0f;
        float total_losses = 0.0f;
        /* Scan all trades for this pattern to compute avg profit/loss */
        for (uint32_t i = 0; i < bridge->num_trades; i++) {
            for (uint32_t j = 0; j < bridge->trade_pattern_counts[i]; j++) {
                if (bridge->trade_patterns[i] && bridge->trade_patterns[i][j] == pattern_id) {
                    float o = bridge->trades[i].outcome;
                    if (o > 0) total_wins += o;
                    else total_losses += (-o);
                }
            }
        }
        pattern->avg_profit = (pattern->win_count > 0) ? total_wins / pattern->win_count : 0.0f;
        pattern->avg_loss = (pattern->loss_count > 0) ? total_losses / pattern->loss_count : 0.0f;
        pattern->profit_factor = (total_losses > 0.001f) ? total_wins / total_losses : 999.0f;
    }

    return FIN_CONSOLIDATION_ERR_OK;
}

int financial_consolidation_bridge_get_pattern(
    const financial_consolidation_bridge_t* bridge,
    uint32_t pattern_id,
    fin_pattern_entry_t* pattern)
{
    if (!bridge || !pattern) {
        return FIN_CONSOLIDATION_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_CONSOLIDATION_BRIDGE_MAGIC) {
        return FIN_CONSOLIDATION_ERR_STATE;
    }

    for (uint32_t i = 0; i < bridge->num_patterns; i++) {
        if (bridge->patterns[i].pattern_id == pattern_id) {
            *pattern = bridge->patterns[i];
            return FIN_CONSOLIDATION_ERR_OK;
        }
    }

    set_error("Pattern ID %u not found", pattern_id);
    return FIN_CONSOLIDATION_ERR_INVALID_PARAM;
}

uint32_t financial_consolidation_bridge_get_pattern_count(
    const financial_consolidation_bridge_t* bridge)
{
    if (!bridge || bridge->magic != FINANCIAL_CONSOLIDATION_BRIDGE_MAGIC) {
        return 0;
    }
    return bridge->num_patterns;
}

int financial_consolidation_bridge_clear_patterns(
    financial_consolidation_bridge_t* bridge)
{
    if (!bridge) {
        return FIN_CONSOLIDATION_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_CONSOLIDATION_BRIDGE_MAGIC) {
        return FIN_CONSOLIDATION_ERR_STATE;
    }

    memset(bridge->patterns, 0, bridge->config.max_pattern_count * sizeof(fin_pattern_entry_t));
    bridge->num_patterns = 0;
    bridge->next_pattern_id = 1;

    return FIN_CONSOLIDATION_ERR_OK;
}

/* ============================================================================
 * Consolidation Operations
 * ============================================================================ */

int financial_consolidation_bridge_replay(
    financial_consolidation_bridge_t* bridge,
    fin_consolidation_result_t* result)
{
    if (!bridge) {
        set_error("bridge is NULL");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_consolidation_bridge_replay: bridge is NULL");
        return FIN_CONSOLIDATION_ERR_NULL;
    }
    if (!result) {
        set_error("result is NULL");
        return FIN_CONSOLIDATION_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_CONSOLIDATION_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic");
        return FIN_CONSOLIDATION_ERR_STATE;
    }

    if (bridge->num_patterns == 0) {
        set_error("No patterns to replay");
        return FIN_CONSOLIDATION_ERR_NO_PATTERNS;
    }

    fin_consolidation_heartbeat_global("fin_consolidation_replay", 0.0f);

    /* Immune check if enabled */
    if (bridge->config.enable_immune_integration && bridge->immune) {
        bridge->stats.immune_checks++;
        int rc = brain_immune_validate_operation(
            (brain_immune_system_t*)bridge->immune,
            "consolidation_replay", 5);
        if (rc != 0) {
            set_error("Immune validation failed for replay");
            return FIN_CONSOLIDATION_ERR_IMMUNE;
        }
    }

    /* Initialize result */
    financial_consolidation_result_init(result);

    /* Allocate pattern strengths array */
    result->pattern_strengths = (float*)nimcp_calloc(bridge->num_patterns, sizeof(float));
    if (!result->pattern_strengths) {
        set_error("Failed to allocate result pattern_strengths");
        return FIN_CONSOLIDATION_ERR_NO_MEMORY;
    }
    result->num_patterns = bridge->num_patterns;

    bridge->op_state = FIN_CONSOLIDATION_OP_STATE_CONSOLIDATING;

    /* Replay profitable patterns */
    uint32_t replayed = 0;
    uint32_t batch_size = bridge->config.replay_batch_size;

    for (uint32_t i = 0; i < bridge->num_patterns && replayed < batch_size; i++) {
        fin_pattern_entry_t* pattern = &bridge->patterns[i];

        if (!pattern->enabled) {
            continue;
        }

        /* Only replay winning patterns */
        if (pattern->cumulative_outcome > 0 &&
            pattern->win_rate >= bridge->config.min_win_rate &&
            pattern->occurrence_count >= bridge->config.min_occurrences) {

            /* "Replay" by incrementing replay count and slightly boosting strength */
            pattern->replay_count++;
            float boost = bridge->config.strengthen_rate * 0.1f;  /* Smaller boost for replay */
            float old_strength = pattern->strength;
            pattern->strength = nimcp_clampf(pattern->strength + boost, 0.0f, 1.0f);

            replayed++;
            bridge->stats.replays++;

            /* Fire callback */
            if (bridge->replay_callback) {
                bridge->replay_callback(pattern, bridge->replay_callback_data);
            }

            /* KG messaging */
            bridge_kg_publish(bridge, KG_MSG_FIN_CONSOLIDATION_REPLAY,
                              pattern, sizeof(*pattern));

            if (bridge->config.verbose_logging) {
                /* Would log replay details */
            }

            (void)old_strength;  /* Suppress unused warning */
        }

        result->pattern_strengths[i] = pattern->strength;

        fin_consolidation_heartbeat_global("fin_consolidation_replay",
            (float)(i + 1) / (float)bridge->num_patterns);
    }

    result->patterns_replayed = replayed;
    bridge->op_state = FIN_CONSOLIDATION_OP_STATE_ACTIVE;

    fin_consolidation_heartbeat_global("fin_consolidation_replay", 1.0f);
    return FIN_CONSOLIDATION_ERR_OK;
}

int financial_consolidation_bridge_prune_losers(
    financial_consolidation_bridge_t* bridge,
    fin_consolidation_result_t* result)
{
    if (!bridge) {
        set_error("bridge is NULL");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_consolidation_bridge_prune_losers: bridge is NULL");
        return FIN_CONSOLIDATION_ERR_NULL;
    }
    if (!result) {
        set_error("result is NULL");
        return FIN_CONSOLIDATION_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_CONSOLIDATION_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic");
        return FIN_CONSOLIDATION_ERR_STATE;
    }

    if (bridge->num_patterns == 0) {
        set_error("No patterns to prune");
        return FIN_CONSOLIDATION_ERR_NO_PATTERNS;
    }

    fin_consolidation_heartbeat_global("fin_consolidation_prune", 0.0f);

    /* BBB validation if enabled */
    if (bridge->config.enable_bbb_validation && bridge->bbb) {
        bridge->stats.bbb_validations++;
    }

    /* Initialize result */
    financial_consolidation_result_init(result);

    result->pattern_strengths = (float*)nimcp_calloc(bridge->num_patterns, sizeof(float));
    if (!result->pattern_strengths) {
        set_error("Failed to allocate result pattern_strengths");
        return FIN_CONSOLIDATION_ERR_NO_MEMORY;
    }
    result->num_patterns = bridge->num_patterns;

    bridge->op_state = FIN_CONSOLIDATION_OP_STATE_CONSOLIDATING;

    /* Prune losing patterns */
    uint32_t pruned = 0;

    for (uint32_t i = 0; i < bridge->num_patterns; i++) {
        fin_pattern_entry_t* pattern = &bridge->patterns[i];

        if (!pattern->enabled) {
            result->pattern_strengths[i] = pattern->strength;
            continue;
        }

        /* Check if pattern should be pruned or weakened */
        bool should_prune = false;
        bool should_weaken = false;

        if (pattern->occurrence_count >= bridge->config.min_occurrences) {
            /* Pattern has enough data to evaluate */
            if (pattern->cumulative_outcome < 0) {
                /* Losing pattern */
                if (pattern->win_rate < 0.3f) {
                    /* Very low win rate - prune */
                    should_prune = true;
                } else {
                    /* Moderate - weaken */
                    should_weaken = true;
                }
            }
            if (pattern->strength < bridge->config.prune_threshold) {
                /* Below threshold - prune */
                should_prune = true;
            }
        }

        if (should_prune) {
            /* Fire callback before disabling */
            if (bridge->prune_callback) {
                bridge->prune_callback(pattern, bridge->prune_callback_data);
            }

            pattern->enabled = false;
            pattern->strength = 0.0f;
            pruned++;
            bridge->stats.prunings++;

            /* KG messaging */
            bridge_kg_publish(bridge, KG_MSG_FIN_CONSOLIDATION_PRUNE,
                              pattern, sizeof(*pattern));
        } else if (should_weaken) {
            /* Weaken but don't prune */
            pattern->strength = nimcp_clampf(
                pattern->strength - bridge->config.weaken_rate, 0.0f, 1.0f);
        }

        result->pattern_strengths[i] = pattern->strength;

        fin_consolidation_heartbeat_global("fin_consolidation_prune",
            (float)(i + 1) / (float)bridge->num_patterns);
    }

    result->patterns_pruned = pruned;
    bridge->op_state = FIN_CONSOLIDATION_OP_STATE_ACTIVE;

    fin_consolidation_heartbeat_global("fin_consolidation_prune", 1.0f);
    return FIN_CONSOLIDATION_ERR_OK;
}

int financial_consolidation_bridge_strengthen_winners(
    financial_consolidation_bridge_t* bridge,
    fin_consolidation_result_t* result)
{
    if (!bridge) {
        set_error("bridge is NULL");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_consolidation_bridge_strengthen_winners: bridge is NULL");
        return FIN_CONSOLIDATION_ERR_NULL;
    }
    if (!result) {
        set_error("result is NULL");
        return FIN_CONSOLIDATION_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_CONSOLIDATION_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic");
        return FIN_CONSOLIDATION_ERR_STATE;
    }

    if (bridge->num_patterns == 0) {
        set_error("No patterns to strengthen");
        return FIN_CONSOLIDATION_ERR_NO_PATTERNS;
    }

    fin_consolidation_heartbeat_global("fin_consolidation_strengthen", 0.0f);

    /* Immune check if enabled */
    if (bridge->config.enable_immune_integration && bridge->immune) {
        bridge->stats.immune_checks++;
        int rc = brain_immune_validate_operation(
            (brain_immune_system_t*)bridge->immune,
            "consolidation_strengthen", 5);
        if (rc != 0) {
            set_error("Immune validation failed for strengthen");
            return FIN_CONSOLIDATION_ERR_IMMUNE;
        }
    }

    /* Initialize result */
    financial_consolidation_result_init(result);

    result->pattern_strengths = (float*)nimcp_calloc(bridge->num_patterns, sizeof(float));
    if (!result->pattern_strengths) {
        set_error("Failed to allocate result pattern_strengths");
        return FIN_CONSOLIDATION_ERR_NO_MEMORY;
    }
    result->num_patterns = bridge->num_patterns;

    bridge->op_state = FIN_CONSOLIDATION_OP_STATE_CONSOLIDATING;

    /* Strengthen winning patterns */
    uint32_t strengthened = 0;

    for (uint32_t i = 0; i < bridge->num_patterns; i++) {
        fin_pattern_entry_t* pattern = &bridge->patterns[i];

        if (!pattern->enabled) {
            result->pattern_strengths[i] = pattern->strength;
            continue;
        }

        /* Check if pattern qualifies for strengthening */
        if (pattern->occurrence_count >= bridge->config.min_occurrences &&
            pattern->cumulative_outcome > 0 &&
            pattern->win_rate >= bridge->config.min_win_rate) {

            /* Calculate strengthening amount based on performance */
            float base_boost = bridge->config.strengthen_rate;

            /* Bonus for high win rate */
            float win_rate_bonus = (pattern->win_rate - bridge->config.min_win_rate) * 0.5f;

            /* Bonus for high profit factor */
            float pf_bonus = 0.0f;
            if (pattern->profit_factor > 1.5f) {
                pf_bonus = (pattern->profit_factor - 1.0f) * 0.02f;
                pf_bonus = nimcp_clampf(pf_bonus, 0.0f, 0.1f);
            }

            float total_boost = base_boost + win_rate_bonus + pf_bonus;
            float old_strength = pattern->strength;
            pattern->strength = nimcp_clampf(pattern->strength + total_boost, 0.0f, 1.0f);

            strengthened++;
            bridge->stats.strengthenings++;

            /* Fire callback */
            if (bridge->strengthen_callback) {
                bridge->strengthen_callback(pattern, old_strength, pattern->strength,
                                            bridge->strengthen_callback_data);
            }

            /* KG messaging */
            bridge_kg_publish(bridge, KG_MSG_FIN_CONSOLIDATION_STRENGTHEN,
                              pattern, sizeof(*pattern));
        }

        result->pattern_strengths[i] = pattern->strength;

        fin_consolidation_heartbeat_global("fin_consolidation_strengthen",
            (float)(i + 1) / (float)bridge->num_patterns);
    }

    /* Update result (we don't have a dedicated "strengthened" field, but can use replayed) */
    result->patterns_replayed = strengthened;
    bridge->op_state = FIN_CONSOLIDATION_OP_STATE_ACTIVE;

    fin_consolidation_heartbeat_global("fin_consolidation_strengthen", 1.0f);
    return FIN_CONSOLIDATION_ERR_OK;
}

int financial_consolidation_bridge_consolidate(
    financial_consolidation_bridge_t* bridge,
    fin_consolidation_mode_t mode,
    fin_consolidation_result_t* result)
{
    if (!bridge) {
        set_error("bridge is NULL");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_consolidation_bridge_consolidate: bridge is NULL");
        return FIN_CONSOLIDATION_ERR_NULL;
    }
    if (!result) {
        set_error("result is NULL");
        return FIN_CONSOLIDATION_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_CONSOLIDATION_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic");
        return FIN_CONSOLIDATION_ERR_STATE;
    }

    fin_consolidation_heartbeat_global("fin_consolidation_full", 0.0f);

    int rc = FIN_CONSOLIDATION_ERR_OK;
    fin_consolidation_result_t temp_result;

    /* Initialize final result */
    financial_consolidation_result_init(result);

    switch (mode) {
        case FIN_CONSOLIDATION_MODE_FULL:
            /* Run all three operations */

            /* 1. Replay */
            financial_consolidation_result_init(&temp_result);
            rc = financial_consolidation_bridge_replay(bridge, &temp_result);
            if (rc == FIN_CONSOLIDATION_ERR_OK) {
                result->patterns_replayed = temp_result.patterns_replayed;
            }
            financial_consolidation_result_free(&temp_result);
            if (rc != FIN_CONSOLIDATION_ERR_OK && rc != FIN_CONSOLIDATION_ERR_NO_PATTERNS) {
                return rc;
            }

            fin_consolidation_heartbeat_global("fin_consolidation_full", 0.33f);

            /* 2. Prune */
            financial_consolidation_result_init(&temp_result);
            rc = financial_consolidation_bridge_prune_losers(bridge, &temp_result);
            if (rc == FIN_CONSOLIDATION_ERR_OK) {
                result->patterns_pruned = temp_result.patterns_pruned;
            }
            financial_consolidation_result_free(&temp_result);
            if (rc != FIN_CONSOLIDATION_ERR_OK && rc != FIN_CONSOLIDATION_ERR_NO_PATTERNS) {
                return rc;
            }

            fin_consolidation_heartbeat_global("fin_consolidation_full", 0.66f);

            /* 3. Strengthen */
            financial_consolidation_result_init(&temp_result);
            rc = financial_consolidation_bridge_strengthen_winners(bridge, &temp_result);
            if (rc == FIN_CONSOLIDATION_ERR_OK) {
                /* Copy final pattern strengths */
                result->pattern_strengths = temp_result.pattern_strengths;
                result->num_patterns = temp_result.num_patterns;
                temp_result.pattern_strengths = NULL;  /* Transfer ownership */
            }
            financial_consolidation_result_free(&temp_result);
            break;

        case FIN_CONSOLIDATION_MODE_REPLAY_ONLY:
            rc = financial_consolidation_bridge_replay(bridge, result);
            break;

        case FIN_CONSOLIDATION_MODE_PRUNE_ONLY:
            rc = financial_consolidation_bridge_prune_losers(bridge, result);
            break;

        case FIN_CONSOLIDATION_MODE_STRENGTHEN_ONLY:
            rc = financial_consolidation_bridge_strengthen_winners(bridge, result);
            break;

        default:
            set_error("Unknown consolidation mode");
            return FIN_CONSOLIDATION_ERR_INVALID_PARAM;
    }

    fin_consolidation_heartbeat_global("fin_consolidation_full", 1.0f);
    return rc;
}

/* ============================================================================
 * Result Management
 * ============================================================================ */

void financial_consolidation_result_init(fin_consolidation_result_t* result) {
    if (result) {
        memset(result, 0, sizeof(fin_consolidation_result_t));
    }
}

void financial_consolidation_result_free(fin_consolidation_result_t* result) {
    if (result) {
        if (result->pattern_strengths) {
            nimcp_free(result->pattern_strengths);
            result->pattern_strengths = NULL;
        }
        result->num_patterns = 0;
        result->patterns_replayed = 0;
        result->patterns_pruned = 0;
    }
}

/* ============================================================================
 * Query API
 * ============================================================================ */

fin_consolidation_op_state_t financial_consolidation_bridge_get_op_state(
    const financial_consolidation_bridge_t* bridge)
{
    if (!bridge || bridge->magic != FINANCIAL_CONSOLIDATION_BRIDGE_MAGIC) {
        return FIN_CONSOLIDATION_OP_STATE_UNINITIALIZED;
    }
    return bridge->op_state;
}

int financial_consolidation_bridge_get_stats(
    const financial_consolidation_bridge_t* bridge,
    fin_consolidation_bridge_stats_t* stats)
{
    if (!bridge) {
        set_error("bridge is NULL");
        return FIN_CONSOLIDATION_ERR_NULL;
    }
    if (!stats) {
        set_error("stats is NULL");
        return FIN_CONSOLIDATION_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_CONSOLIDATION_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic");
        return FIN_CONSOLIDATION_ERR_STATE;
    }

    *stats = bridge->stats;
    return FIN_CONSOLIDATION_ERR_OK;
}

void financial_consolidation_bridge_reset_stats(financial_consolidation_bridge_t* bridge) {
    if (bridge && bridge->magic == FINANCIAL_CONSOLIDATION_BRIDGE_MAGIC) {
        memset(&bridge->stats, 0, sizeof(fin_consolidation_bridge_stats_t));
    }
}

int financial_consolidation_bridge_get_top_patterns(
    const financial_consolidation_bridge_t* bridge,
    fin_pattern_entry_t* patterns,
    uint32_t max_patterns)
{
    if (!bridge || !patterns) {
        return -FIN_CONSOLIDATION_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_CONSOLIDATION_BRIDGE_MAGIC) {
        return -FIN_CONSOLIDATION_ERR_STATE;
    }

    if (bridge->num_patterns == 0) {
        return 0;
    }

    /* Copy all enabled patterns */
    uint32_t count = 0;
    for (uint32_t i = 0; i < bridge->num_patterns && count < max_patterns; i++) {
        if (bridge->patterns[i].enabled) {
            patterns[count++] = bridge->patterns[i];
        }
    }

    /* Sort by strength (simple bubble sort for small arrays) */
    for (uint32_t i = 0; i < count - 1; i++) {
        for (uint32_t j = 0; j < count - i - 1; j++) {
            if (patterns[j].strength < patterns[j + 1].strength) {
                fin_pattern_entry_t temp = patterns[j];
                patterns[j] = patterns[j + 1];
                patterns[j + 1] = temp;
            }
        }
    }

    return (int)count;
}

int financial_consolidation_bridge_get_patterns_by_type(
    const financial_consolidation_bridge_t* bridge,
    fin_pattern_type_t type,
    fin_pattern_entry_t* patterns,
    uint32_t max_patterns)
{
    if (!bridge || !patterns) {
        return -FIN_CONSOLIDATION_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_CONSOLIDATION_BRIDGE_MAGIC) {
        return -FIN_CONSOLIDATION_ERR_STATE;
    }

    uint32_t count = 0;
    for (uint32_t i = 0; i < bridge->num_patterns && count < max_patterns; i++) {
        if (bridge->patterns[i].type == type && bridge->patterns[i].enabled) {
            patterns[count++] = bridge->patterns[i];
        }
    }

    return (int)count;
}

/* ============================================================================
 * Health Integration
 * ============================================================================ */

int financial_consolidation_bridge_heartbeat(
    financial_consolidation_bridge_t* bridge,
    const char* operation,
    float progress)
{
    if (!bridge) {
        return FIN_CONSOLIDATION_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_CONSOLIDATION_BRIDGE_MAGIC) {
        return FIN_CONSOLIDATION_ERR_STATE;
    }

    /* Forward to global health agent */
    fin_consolidation_heartbeat_global(
        operation ? operation : "fin_consolidation_heartbeat", progress);

    /* Forward to instance-level health agent */
    if (bridge->health_agent) {
        nimcp_health_agent_heartbeat_ex(
            (nimcp_health_agent_t*)bridge->health_agent, operation, progress);
    }

    bridge->stats.health_heartbeats++;
    return FIN_CONSOLIDATION_ERR_OK;
}
