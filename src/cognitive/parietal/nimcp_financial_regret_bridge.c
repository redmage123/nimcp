/**
 * @file nimcp_financial_regret_bridge.c
 * @brief Financial Regret Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-29
 *
 * WHAT: Bridge for analyzing trading regret, computing counterfactual outcomes,
 *       and extracting actionable lessons from past decisions.
 *
 * WHY:  Trading decisions often result in regret when outcomes differ from
 *       expectations. By systematically analyzing regret and extracting lessons,
 *       we can improve future decision-making and reduce repeated mistakes.
 *
 * HOW:  Analyzes completed trades to compute regret magnitude by comparing
 *       actual actions to optimal hindsight actions. Generates counterfactual
 *       scenarios and extracts lessons via pattern recognition.
 *
 * @author NIMCP Development Team
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdarg.h>

#include "cognitive/parietal/nimcp_financial_regret_bridge.h"
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

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(fin_regret)

/* Stub heartbeat for migration compatibility */
static inline void fin_regret_heartbeat_global(const char* op, float progress) {
    (void)op; (void)progress;
}

BRIDGE_DEFINE_MESH_REGISTRATION(fin_regret, MESH_ADAPTER_CATEGORY_COGNITIVE)


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

static _Thread_local char fin_regret_last_error[NIMCP_ERROR_BUFFER_SIZE] = {0};

static void set_error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(fin_regret_last_error, sizeof(fin_regret_last_error), fmt, args);
    va_end(args);
}

const char* financial_regret_bridge_get_last_error(void) {
    return fin_regret_last_error;
}

/* ============================================================================
 * Internal Structure
 * ============================================================================ */

struct financial_regret_bridge {
    uint32_t magic;
    fin_regret_config_t config;
    fin_regret_bridge_state_t op_state;
    fin_regret_bridge_stats_t stats;

    /* Trade history (circular buffer) */
    fin_trade_record_t* history;
    uint32_t history_capacity;
    uint32_t history_count;
    uint32_t history_head;  /* Next write position */

    /* Lesson storage */
    fin_lesson_t* lessons;
    uint32_t lesson_capacity;
    uint32_t lesson_count;
    uint32_t next_lesson_id;

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

    /* Training state */
    bool training_active;
};

static inline float fabsf_safe(float v) {
    return v < 0.0f ? -v : v;
}

static inline float maxf(float a, float b) {
    return a > b ? a : b;
}

static inline float minf(float a, float b) {
    return a < b ? a : b;
}

/* ============================================================================
 * KG Wiring Message Types
 * ============================================================================ */

#define KG_MSG_FIN_REGRET_ANALYSIS     "FIN_REGRET_ANALYSIS"
#define KG_MSG_FIN_REGRET_COUNTERFACT  "FIN_REGRET_COUNTERFACTUAL"
#define KG_MSG_FIN_REGRET_LESSON       "FIN_REGRET_LESSON"

static int bridge_kg_publish(financial_regret_bridge_t* bridge,
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

const char* fin_regret_action_name(fin_action_type_t action) {
    switch (action) {
        case FIN_ACTION_NONE:     return "NONE";
        case FIN_ACTION_BUY:      return "BUY";
        case FIN_ACTION_SELL:     return "SELL";
        case FIN_ACTION_HOLD:     return "HOLD";
        case FIN_ACTION_EXIT:     return "EXIT";
        case FIN_ACTION_INCREASE: return "INCREASE";
        case FIN_ACTION_DECREASE: return "DECREASE";
        default: return "UNKNOWN";
    }
}

const char* fin_regret_type_name(fin_regret_type_t type) {
    switch (type) {
        case FIN_REGRET_COMMISSION: return "COMMISSION";
        case FIN_REGRET_OMISSION:   return "OMISSION";
        case FIN_REGRET_TIMING:     return "TIMING";
        case FIN_REGRET_SIZING:     return "SIZING";
        case FIN_REGRET_EXIT:       return "EXIT";
        default: return "UNKNOWN";
    }
}

const char* fin_regret_state_name(fin_regret_bridge_state_t state) {
    switch (state) {
        case FIN_REGRET_STATE_UNINITIALIZED: return "UNINITIALIZED";
        case FIN_REGRET_STATE_INITIALIZED:   return "INITIALIZED";
        case FIN_REGRET_STATE_ACTIVE:        return "ACTIVE";
        case FIN_REGRET_STATE_ANALYZING:     return "ANALYZING";
        case FIN_REGRET_STATE_DEGRADED:      return "DEGRADED";
        case FIN_REGRET_STATE_ERROR:         return "ERROR";
        default: return "UNKNOWN";
    }
}

const char* financial_regret_bridge_version(void) {
    return FINANCIAL_REGRET_BRIDGE_VERSION;
}

/* ============================================================================
 * Configuration
 * ============================================================================ */

int financial_regret_bridge_default_config(fin_regret_config_t* config) {
    if (!config) {
        set_error("config is NULL");
        return FIN_REGRET_ERR_NULL;
    }

    memset(config, 0, sizeof(fin_regret_config_t));

    /* Analysis parameters */
    config->min_trades_for_analysis = 5;
    config->analysis_window_size = 64;
    config->regret_threshold = 0.2f;
    config->lesson_confidence_threshold = 0.6f;

    /* Counterfactual parameters */
    config->max_counterfactuals = FIN_REGRET_MAX_COUNTERFACTUALS;
    config->counterfactual_probability_threshold = 0.1f;

    /* Lesson extraction parameters */
    config->min_pattern_occurrences = 3;
    config->pattern_similarity_threshold = 0.7f;

    /* Integration settings */
    config->enable_immune_integration = true;
    config->enable_bbb_validation = true;
    config->enable_kg_messaging = true;
    config->enable_health_monitoring = true;

    /* Emotional weighting */
    config->enable_emotional_weighting = true;
    config->emotional_decay_rate = 0.1f;

    /* Logging */
    config->verbose_logging = false;

    return FIN_REGRET_ERR_OK;
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

financial_regret_bridge_t* financial_regret_bridge_create(
    const fin_regret_config_t* config)
{
    fin_regret_heartbeat_global("fin_regret_create", 0.0f);

    financial_regret_bridge_t* bridge = (financial_regret_bridge_t*)
        nimcp_malloc(sizeof(financial_regret_bridge_t));
    if (!bridge) {
        set_error("Failed to allocate financial_regret_bridge");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NO_MEMORY,
            "Failed to allocate financial_regret_bridge");
        return NULL;
    }
    memset(bridge, 0, sizeof(financial_regret_bridge_t));

    bridge->magic = FINANCIAL_REGRET_BRIDGE_MAGIC;

    /* Copy configuration or use defaults */
    if (config) {
        bridge->config = *config;
    } else {
        financial_regret_bridge_default_config(&bridge->config);
    }

    /* Allocate trade history */
    bridge->history_capacity = FIN_REGRET_MAX_HISTORY;
    bridge->history = (fin_trade_record_t*)
        nimcp_calloc(bridge->history_capacity, sizeof(fin_trade_record_t));
    if (!bridge->history) {
        set_error("Failed to allocate trade history");
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "financial_regret_bridge_create: bridge->history is NULL");
        return NULL;
    }

    /* Allocate lesson storage */
    bridge->lesson_capacity = FIN_REGRET_MAX_LESSONS;
    bridge->lessons = (fin_lesson_t*)
        nimcp_calloc(bridge->lesson_capacity, sizeof(fin_lesson_t));
    if (!bridge->lessons) {
        set_error("Failed to allocate lesson storage");
        nimcp_free(bridge->history);
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "financial_regret_bridge_create: bridge->lessons is NULL");
        return NULL;
    }

    bridge->history_count = 0;
    bridge->history_head = 0;
    bridge->lesson_count = 0;
    bridge->next_lesson_id = 1;
    bridge->training_active = false;

    bridge->op_state = FIN_REGRET_STATE_INITIALIZED;

    fin_regret_heartbeat_global("fin_regret_create", 1.0f);
    return bridge;
}

void financial_regret_bridge_destroy(financial_regret_bridge_t* bridge) {
    fin_regret_heartbeat_global("fin_regret_destroy", 0.0f);

    if (bridge) {
        if (bridge->history) {
            nimcp_free(bridge->history);
        }
        if (bridge->lessons) {
            nimcp_free(bridge->lessons);
        }
        bridge->magic = 0;
        bridge->op_state = FIN_REGRET_STATE_UNINITIALIZED;
        nimcp_free(bridge);
        bridge = NULL;
    }
}

int financial_regret_bridge_reset(financial_regret_bridge_t* bridge) {
    if (!bridge) {
        set_error("bridge is NULL");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_regret_bridge_reset: bridge is NULL");
        return FIN_REGRET_ERR_NULL;
    }

    if (bridge->magic != FINANCIAL_REGRET_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic");
        return FIN_REGRET_ERR_STATE;
    }

    fin_regret_heartbeat_global("fin_regret_reset", 0.0f);

    /* Reset stats */
    memset(&bridge->stats, 0, sizeof(fin_regret_bridge_stats_t));

    /* Reset history */
    if (bridge->history) {
        memset(bridge->history, 0,
               bridge->history_capacity * sizeof(fin_trade_record_t));
    }
    bridge->history_count = 0;
    bridge->history_head = 0;

    /* Reset lessons */
    if (bridge->lessons) {
        memset(bridge->lessons, 0,
               bridge->lesson_capacity * sizeof(fin_lesson_t));
    }
    bridge->lesson_count = 0;
    bridge->next_lesson_id = 1;

    bridge->training_active = false;
    bridge->op_state = FIN_REGRET_STATE_INITIALIZED;

    fin_regret_heartbeat_global("fin_regret_reset", 1.0f);
    return FIN_REGRET_ERR_OK;
}

/* ============================================================================
 * Subsystem Setters
 * ============================================================================ */

#define FIN_REGRET_SETTER(name, field) \
    int financial_regret_bridge_set_##name( \
        financial_regret_bridge_t* bridge, void* ptr) { \
        if (!bridge) { \
            set_error("bridge is NULL in set_" #name); \
            NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, \
                "financial_regret_bridge_set_" #name ": bridge is NULL"); \
            return FIN_REGRET_ERR_NULL; \
        } \
        if (bridge->magic != FINANCIAL_REGRET_BRIDGE_MAGIC) { \
            set_error("Invalid bridge magic in set_" #name); \
            return FIN_REGRET_ERR_STATE; \
        } \
        bridge->field = ptr; \
        return FIN_REGRET_ERR_OK; \
    }

FIN_REGRET_SETTER(immune, immune)
FIN_REGRET_SETTER(health_agent, health_agent)
FIN_REGRET_SETTER(kg_wiring, kg_wiring)
FIN_REGRET_SETTER(logger, logger)
FIN_REGRET_SETTER(security, security)
FIN_REGRET_SETTER(bio_router, bio_router)

int financial_regret_bridge_set_coordinator(
    financial_regret_bridge_t* bridge,
    brain_cycle_coordinator_t* coordinator)
{
    if (!bridge) {
        set_error("bridge is NULL in set_coordinator");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_regret_bridge_set_coordinator: bridge is NULL");
        return FIN_REGRET_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_REGRET_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic in set_coordinator");
        return FIN_REGRET_ERR_STATE;
    }
    bridge->coordinator = (void*)coordinator;
    return FIN_REGRET_ERR_OK;
}

int financial_regret_bridge_set_bbb(
    financial_regret_bridge_t* bridge,
    bbb_system_t bbb)
{
    if (!bridge) {
        set_error("bridge is NULL in set_bbb");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_regret_bridge_set_bbb: bridge is NULL");
        return FIN_REGRET_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_REGRET_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic in set_bbb");
        return FIN_REGRET_ERR_STATE;
    }
    bridge->bbb = (void*)bbb;
    return FIN_REGRET_ERR_OK;
}

int financial_regret_bridge_set_ethics(
    financial_regret_bridge_t* bridge,
    ethics_engine_t ethics)
{
    if (!bridge) {
        set_error("bridge is NULL in set_ethics");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_regret_bridge_set_ethics: bridge is NULL");
        return FIN_REGRET_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_REGRET_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic in set_ethics");
        return FIN_REGRET_ERR_STATE;
    }
    bridge->ethics = (void*)ethics;
    return FIN_REGRET_ERR_OK;
}

int financial_regret_bridge_set_lgss(
    financial_regret_bridge_t* bridge,
    const void* lgss)
{
    if (!bridge) {
        set_error("bridge is NULL in set_lgss");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_regret_bridge_set_lgss: bridge is NULL");
        return FIN_REGRET_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_REGRET_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic in set_lgss");
        return FIN_REGRET_ERR_STATE;
    }
    bridge->lgss = lgss;
    return FIN_REGRET_ERR_OK;
}

/* ============================================================================
 * Trade History API
 * ============================================================================ */

int financial_regret_bridge_record_trade(
    financial_regret_bridge_t* bridge,
    const fin_trade_record_t* record)
{
    if (!bridge) {
        set_error("bridge is NULL");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_regret_bridge_record_trade: bridge is NULL");
        return FIN_REGRET_ERR_NULL;
    }
    if (!record) {
        set_error("record is NULL");
        return FIN_REGRET_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_REGRET_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic");
        return FIN_REGRET_ERR_STATE;
    }

    fin_regret_heartbeat_global("fin_regret_record", 0.0f);

    /* BBB validation if enabled */
    if (bridge->config.enable_bbb_validation && bridge->bbb) {
        bridge->stats.bbb_validations++;
        int rc = bbb_validate_data((bbb_system_t)bridge->bbb, record,
                                    sizeof(*record), "regret_record");
        if (rc != 0) {
            set_error("BBB validation failed for regret_record");
            return FIN_REGRET_ERR_BBB;
        }
    }

    /* Immune check if enabled */
    if (bridge->config.enable_immune_integration && bridge->immune) {
        bridge->stats.immune_checks++;
        int rc = brain_immune_validate_operation(
            (brain_immune_system_t*)bridge->immune,
            "regret_record", 3);
        if (rc != 0) {
            set_error("Immune validation failed for regret_record");
            return FIN_REGRET_ERR_IMMUNE;
        }
    }

    /* Store trade in circular buffer */
    bridge->history[bridge->history_head] = *record;
    bridge->history_head = (bridge->history_head + 1) % bridge->history_capacity;
    if (bridge->history_count < bridge->history_capacity) {
        bridge->history_count++;
    }

    bridge->op_state = FIN_REGRET_STATE_ACTIVE;

    fin_regret_heartbeat_global("fin_regret_record", 1.0f);
    return FIN_REGRET_ERR_OK;
}

uint32_t financial_regret_bridge_get_trade_count(
    const financial_regret_bridge_t* bridge)
{
    if (!bridge || bridge->magic != FINANCIAL_REGRET_BRIDGE_MAGIC) {
        return 0;
    }
    return bridge->history_count;
}

int financial_regret_bridge_clear_history(
    financial_regret_bridge_t* bridge)
{
    if (!bridge) {
        set_error("bridge is NULL");
        return FIN_REGRET_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_REGRET_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic");
        return FIN_REGRET_ERR_STATE;
    }

    if (bridge->history) {
        memset(bridge->history, 0,
               bridge->history_capacity * sizeof(fin_trade_record_t));
    }
    bridge->history_count = 0;
    bridge->history_head = 0;

    return FIN_REGRET_ERR_OK;
}

/* ============================================================================
 * Internal Regret Analysis Helpers
 * ============================================================================ */

/**
 * @brief Determine regret type from trade characteristics
 */
static fin_regret_type_t determine_regret_type(
    const fin_trade_t* trade,
    const fin_action_t* action_taken,
    float optimal_outcome)
{
    float actual = trade->outcome;
    float diff = optimal_outcome - actual;

    /* If action was taken and resulted in loss */
    if (action_taken->type == FIN_ACTION_BUY ||
        action_taken->type == FIN_ACTION_SELL) {
        if (actual < 0.0f && optimal_outcome > 0.0f) {
            /* Should not have entered - commission regret */
            return FIN_REGRET_COMMISSION;
        }
    }

    /* If no action was taken and missed opportunity */
    if (action_taken->type == FIN_ACTION_NONE ||
        action_taken->type == FIN_ACTION_HOLD) {
        if (optimal_outcome > 0.5f) {
            /* Should have entered - omission regret */
            return FIN_REGRET_OMISSION;
        }
    }

    /* Check for sizing regret */
    if (fabsf_safe(action_taken->magnitude) > 0.0f) {
        float size_optimal = fabsf_safe(trade->outcome) > 0.0f ?
            1.0f : 0.0f;  /* Simplified */
        if (fabsf_safe(action_taken->magnitude - size_optimal) > 0.3f) {
            return FIN_REGRET_SIZING;
        }
    }

    /* Check for exit regret - if max favorable was much higher */
    if (diff > 0.3f) {
        return FIN_REGRET_EXIT;
    }

    /* Default to timing if positive trade but could have been better */
    if (diff > 0.1f) {
        return FIN_REGRET_TIMING;
    }

    return FIN_REGRET_COMMISSION;  /* Default */
}

/**
 * @brief Compute optimal action in hindsight
 */
static void compute_best_action(
    const fin_trade_t* trade,
    const fin_action_t* action_taken,
    fin_action_t* best_action)
{
    memset(best_action, 0, sizeof(fin_action_t));

    /* Simple hindsight optimal:
     * - If outcome was positive, best action was to increase
     * - If outcome was negative, best action was opposite */

    if (trade->outcome > 0.2f) {
        /* Good trade - optimal was to size up */
        best_action->type = action_taken->type;
        best_action->magnitude = action_taken->magnitude * 1.5f;
    } else if (trade->outcome < -0.2f) {
        /* Bad trade - optimal was to avoid or reverse */
        if (action_taken->type == FIN_ACTION_BUY) {
            best_action->type = FIN_ACTION_SELL;
        } else if (action_taken->type == FIN_ACTION_SELL) {
            best_action->type = FIN_ACTION_BUY;
        } else {
            best_action->type = FIN_ACTION_NONE;
        }
        best_action->magnitude = 0.0f;
    } else {
        /* Neutral - optimal was to reduce size */
        best_action->type = action_taken->type;
        best_action->magnitude = action_taken->magnitude * 0.5f;
    }
}

/**
 * @brief Generate counterfactual description
 */
static void generate_counterfactual_description(
    const fin_trade_t* trade,
    const fin_action_t* action_taken,
    const fin_action_t* best_action,
    float regret_magnitude,
    char* buffer,
    size_t buffer_size)
{
    const char* action_str = fin_regret_action_name((fin_action_type_t)action_taken->type);
    const char* best_str = fin_regret_action_name((fin_action_type_t)best_action->type);

    if (best_action->type != action_taken->type) {
        snprintf(buffer, buffer_size,
                 "If %s instead of %s at %.2f, outcome would have improved by %.1f%%",
                 best_str, action_str, trade->price, regret_magnitude * 100.0f);
    } else if (fabsf_safe(best_action->magnitude - action_taken->magnitude) > 0.1f) {
        snprintf(buffer, buffer_size,
                 "If %s with size %.2f instead of %.2f, outcome would have improved by %.1f%%",
                 action_str, best_action->magnitude, action_taken->magnitude,
                 regret_magnitude * 100.0f);
    } else {
        snprintf(buffer, buffer_size,
                 "Optimal action was similar to taken action; regret is %.1f%%",
                 regret_magnitude * 100.0f);
    }
}

/**
 * @brief Generate lesson text based on regret type
 */
static void generate_lesson_text(
    fin_regret_type_t regret_type,
    float regret_magnitude,
    const fin_trade_t* trade,
    char* buffer,
    size_t buffer_size)
{
    switch (regret_type) {
        case FIN_REGRET_COMMISSION:
            snprintf(buffer, buffer_size,
                     "Wait for better confirmation before entering; entry at %.2f "
                     "was premature (%.0f%% regret)",
                     trade->price, regret_magnitude * 100.0f);
            break;
        case FIN_REGRET_OMISSION:
            snprintf(buffer, buffer_size,
                     "Be more decisive on clear signals; hesitation cost %.0f%% opportunity",
                     regret_magnitude * 100.0f);
            break;
        case FIN_REGRET_TIMING:
            snprintf(buffer, buffer_size,
                     "Improve entry/exit timing; current timing costs %.0f%% "
                     "vs optimal execution",
                     regret_magnitude * 100.0f);
            break;
        case FIN_REGRET_SIZING:
            snprintf(buffer, buffer_size,
                     "Adjust position sizing based on conviction; wrong size cost %.0f%%",
                     regret_magnitude * 100.0f);
            break;
        case FIN_REGRET_EXIT:
            snprintf(buffer, buffer_size,
                     "Hold winners longer or use trailing stops; early exit cost %.0f%%",
                     regret_magnitude * 100.0f);
            break;
        default:
            snprintf(buffer, buffer_size,
                     "Review decision process; regret magnitude %.0f%%",
                     regret_magnitude * 100.0f);
            break;
    }
}

/* ============================================================================
 * Core API - Regret Analysis
 * ============================================================================ */

int financial_regret_bridge_analyze(
    financial_regret_bridge_t* bridge,
    const fin_trade_t* trade,
    const fin_action_t* action_taken,
    fin_regret_analysis_t* analysis)
{
    if (!bridge) {
        set_error("bridge is NULL");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_regret_bridge_analyze: bridge is NULL");
        return FIN_REGRET_ERR_NULL;
    }
    if (!trade || !action_taken || !analysis) {
        set_error("trade, action_taken, or analysis is NULL");
        return FIN_REGRET_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_REGRET_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic");
        return FIN_REGRET_ERR_STATE;
    }

    fin_regret_heartbeat_global("fin_regret_analyze", 0.0f);
    bridge->op_state = FIN_REGRET_STATE_ANALYZING;

    /* BBB validation if enabled */
    if (bridge->config.enable_bbb_validation && bridge->bbb) {
        bridge->stats.bbb_validations++;
    }

    /* Immune check if enabled */
    if (bridge->config.enable_immune_integration && bridge->immune) {
        bridge->stats.immune_checks++;
    }

    memset(analysis, 0, sizeof(fin_regret_analysis_t));
    analysis->trade = *trade;
    analysis->action_taken = *action_taken;

    bridge->stats.analyses++;

    /* Compute best hindsight action */
    compute_best_action(trade, action_taken, &analysis->best_action);

    /* Compute regret magnitude as difference between actual and optimal */
    /* Optimal outcome approximation: direction * max(0, -outcome) for bad trades
     * or direction * 1.5 * outcome for good trades that could have been bigger */
    float optimal_outcome = 0.0f;
    if (trade->outcome < 0.0f) {
        /* Bad trade: optimal was to not enter (0) or reverse */
        optimal_outcome = -trade->outcome;  /* What we would have made going other way */
    } else {
        /* Good trade: optimal was to hold longer or size up */
        optimal_outcome = trade->outcome * 1.3f;  /* Assume 30% more was possible */
    }

    analysis->regret_magnitude = nimcp_clampf(
        fabsf_safe(optimal_outcome - trade->outcome) / maxf(fabsf_safe(optimal_outcome), 0.01f),
        0.0f, 1.0f);

    /* Generate counterfactual description */
    generate_counterfactual_description(
        trade, action_taken, &analysis->best_action,
        analysis->regret_magnitude,
        analysis->counterfactual, sizeof(analysis->counterfactual));

    /* Generate lesson */
    fin_regret_type_t regret_type = determine_regret_type(
        trade, action_taken, optimal_outcome);
    generate_lesson_text(
        regret_type, analysis->regret_magnitude, trade,
        analysis->lesson, sizeof(analysis->lesson));

    /* KG messaging */
    bridge_kg_publish(bridge, KG_MSG_FIN_REGRET_ANALYSIS,
                      analysis, sizeof(*analysis));

    bridge->op_state = FIN_REGRET_STATE_ACTIVE;
    fin_regret_heartbeat_global("fin_regret_analyze", 1.0f);
    return FIN_REGRET_ERR_OK;
}

int financial_regret_bridge_analyze_full(
    financial_regret_bridge_t* bridge,
    const fin_trade_record_t* record,
    fin_regret_assessment_t* assessment)
{
    if (!bridge) {
        set_error("bridge is NULL");
        return FIN_REGRET_ERR_NULL;
    }
    if (!record || !assessment) {
        set_error("record or assessment is NULL");
        return FIN_REGRET_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_REGRET_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic");
        return FIN_REGRET_ERR_STATE;
    }

    fin_regret_heartbeat_global("fin_regret_analyze_full", 0.0f);

    memset(assessment, 0, sizeof(fin_regret_assessment_t));

    /* Create action from record */
    fin_action_t action_taken;
    action_taken.type = (record->trade.direction > 0) ? FIN_ACTION_BUY :
                               (record->trade.direction < 0) ? FIN_ACTION_SELL :
                               FIN_ACTION_HOLD;
    action_taken.magnitude = record->trade.quantity;

    /* Perform basic analysis */
    int rc = financial_regret_bridge_analyze(
        bridge, &record->trade, &action_taken, &assessment->analysis);
    if (rc != FIN_REGRET_ERR_OK) {
        return rc;
    }

    /* Determine regret type using extended info */
    float optimal_outcome = (record->trade.outcome < 0.0f) ?
        -record->trade.outcome : record->trade.outcome * 1.3f;
    assessment->regret_type = determine_regret_type(
        &record->trade, &action_taken, optimal_outcome);

    /* Compute emotional intensity based on magnitude and type */
    float base_intensity = assessment->analysis.regret_magnitude;
    if (record->max_favorable > record->exit_price && record->trade.direction > 0) {
        /* Had a winner, let it become a loser - high emotional intensity */
        base_intensity *= 1.5f;
    }
    assessment->emotional_intensity = nimcp_clampf(base_intensity, 0.0f, 1.0f);

    /* Generate counterfactual scenarios */
    rc = financial_regret_bridge_generate_counterfactuals(
        bridge, record, assessment->counterfactuals,
        FIN_REGRET_MAX_COUNTERFACTUALS, &assessment->counterfactual_count);
    /* Continue even if counterfactual generation fails */

    /* Generate summary */
    snprintf(assessment->summary, FIN_REGRET_DESC_LEN,
             "%s regret (%.0f%% magnitude, %.0f%% emotional intensity) on %s trade",
             fin_regret_type_name(assessment->regret_type),
             assessment->analysis.regret_magnitude * 100.0f,
             assessment->emotional_intensity * 100.0f,
             record->symbol);

    fin_regret_heartbeat_global("fin_regret_analyze_full", 1.0f);
    return FIN_REGRET_ERR_OK;
}

/* ============================================================================
 * Core API - Counterfactual Analysis
 * ============================================================================ */

int financial_regret_bridge_counterfactual(
    financial_regret_bridge_t* bridge,
    const fin_trade_t* trade,
    const fin_action_t* alternative_action,
    float* hypothetical_outcome)
{
    if (!bridge) {
        set_error("bridge is NULL");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_regret_bridge_counterfactual: bridge is NULL");
        return FIN_REGRET_ERR_NULL;
    }
    if (!trade || !alternative_action || !hypothetical_outcome) {
        set_error("trade, alternative_action, or hypothetical_outcome is NULL");
        return FIN_REGRET_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_REGRET_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic");
        return FIN_REGRET_ERR_STATE;
    }

    fin_regret_heartbeat_global("fin_regret_counterfactual", 0.0f);

    bridge->stats.counterfactuals++;

    /* Compute hypothetical outcome based on alternative action */
    float actual_outcome = trade->outcome;
    float hypo = 0.0f;

    switch ((fin_action_type_t)alternative_action->type) {
        case FIN_ACTION_NONE:
            /* Did nothing - outcome would be 0 */
            hypo = 0.0f;
            break;

        case FIN_ACTION_BUY:
            /* If original was short or neutral, reverse */
            if (trade->direction <= 0) {
                hypo = -actual_outcome;  /* Reverse of original */
            } else {
                /* Size adjustment */
                float size_ratio = alternative_action->magnitude /
                    maxf(trade->quantity, 0.001f);
                hypo = actual_outcome * size_ratio;
            }
            break;

        case FIN_ACTION_SELL:
            /* If original was long or neutral, reverse */
            if (trade->direction >= 0) {
                hypo = -actual_outcome;
            } else {
                float size_ratio = alternative_action->magnitude /
                    maxf(trade->quantity, 0.001f);
                hypo = actual_outcome * size_ratio;
            }
            break;

        case FIN_ACTION_HOLD:
            /* Assume would have recovered or continued trend */
            if (actual_outcome < 0.0f) {
                /* Loss - assume 50% recovery if held */
                hypo = actual_outcome * 0.5f;
            } else {
                /* Profit - assume 30% more if held */
                hypo = actual_outcome * 1.3f;
            }
            break;

        case FIN_ACTION_EXIT:
            /* Earlier exit - assume captured 70% of move */
            hypo = actual_outcome * 0.7f;
            if (actual_outcome < 0.0f) {
                /* But limited loss */
                hypo = maxf(hypo, actual_outcome * 0.5f);
            }
            break;

        case FIN_ACTION_INCREASE:
            hypo = actual_outcome * (1.0f + alternative_action->magnitude * 0.5f);
            break;

        case FIN_ACTION_DECREASE:
            hypo = actual_outcome * (1.0f - alternative_action->magnitude * 0.5f);
            break;

        default:
            hypo = actual_outcome;
            break;
    }

    *hypothetical_outcome = nimcp_clampf(hypo, -2.0f, 2.0f);

    /* KG messaging */
    bridge_kg_publish(bridge, KG_MSG_FIN_REGRET_COUNTERFACT,
                      hypothetical_outcome, sizeof(float));

    fin_regret_heartbeat_global("fin_regret_counterfactual", 1.0f);
    return FIN_REGRET_ERR_OK;
}

int financial_regret_bridge_generate_counterfactuals(
    financial_regret_bridge_t* bridge,
    const fin_trade_record_t* trade,
    fin_counterfactual_scenario_t* scenarios,
    uint32_t max_scenarios,
    uint32_t* num_scenarios)
{
    if (!bridge) {
        set_error("bridge is NULL");
        return FIN_REGRET_ERR_NULL;
    }
    if (!trade || !scenarios || !num_scenarios) {
        set_error("trade, scenarios, or num_scenarios is NULL");
        return FIN_REGRET_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_REGRET_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic");
        return FIN_REGRET_ERR_STATE;
    }

    fin_regret_heartbeat_global("fin_regret_gen_cf", 0.0f);

    *num_scenarios = 0;
    uint32_t count = 0;

    /* Generate scenarios for each alternative action type */
    fin_action_type_t alternatives[] = {
        FIN_ACTION_NONE,
        FIN_ACTION_HOLD,
        FIN_ACTION_EXIT,
        FIN_ACTION_INCREASE,
        FIN_ACTION_DECREASE
    };

    for (size_t i = 0; i < sizeof(alternatives)/sizeof(alternatives[0]) && count < max_scenarios; i++) {
        fin_action_t alt_action;
        alt_action.type = alternatives[i];
        alt_action.magnitude = 0.5f;  /* Default magnitude */

        float hypo = 0.0f;
        int rc = financial_regret_bridge_counterfactual(
            bridge, &trade->trade, &alt_action, &hypo);
        if (rc != FIN_REGRET_ERR_OK) {
            continue;
        }

        /* Only include scenarios that differ meaningfully */
        float diff = hypo - trade->trade.outcome;
        if (fabsf_safe(diff) < bridge->config.counterfactual_probability_threshold) {
            continue;
        }

        fin_counterfactual_scenario_t* scenario = &scenarios[count];
        scenario->alternative_action = alt_action;
        scenario->hypothetical_outcome = hypo;
        scenario->outcome_difference = diff;
        scenario->probability = 0.5f;  /* Simplified probability */

        snprintf(scenario->description, FIN_REGRET_DESC_LEN,
                 "If %s: outcome %.2f (vs %.2f actual), diff %.2f",
                 fin_regret_action_name(alternatives[i]),
                 hypo, trade->trade.outcome, diff);

        count++;
    }

    *num_scenarios = count;

    fin_regret_heartbeat_global("fin_regret_gen_cf", 1.0f);
    return FIN_REGRET_ERR_OK;
}

/* ============================================================================
 * Core API - Lesson Extraction
 * ============================================================================ */

int financial_regret_bridge_extract_lesson(
    financial_regret_bridge_t* bridge,
    const fin_regret_analysis_t* analysis,
    fin_lesson_t* lesson)
{
    if (!bridge) {
        set_error("bridge is NULL");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_regret_bridge_extract_lesson: bridge is NULL");
        return FIN_REGRET_ERR_NULL;
    }
    if (!analysis || !lesson) {
        set_error("analysis or lesson is NULL");
        return FIN_REGRET_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_REGRET_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic");
        return FIN_REGRET_ERR_STATE;
    }

    fin_regret_heartbeat_global("fin_regret_lesson", 0.0f);

    /* Check if regret is significant enough for lesson */
    if (analysis->regret_magnitude < bridge->config.regret_threshold) {
        set_error("Regret magnitude too low for lesson extraction");
        return FIN_REGRET_ERR_VALIDATION;
    }

    memset(lesson, 0, sizeof(fin_lesson_t));

    /* Determine regret type */
    float optimal_outcome = (analysis->trade.outcome < 0.0f) ?
        -analysis->trade.outcome : analysis->trade.outcome * 1.3f;
    fin_regret_type_t regret_type = determine_regret_type(
        &analysis->trade, &analysis->action_taken, optimal_outcome);

    /* Check if similar lesson already exists */
    for (uint32_t i = 0; i < bridge->lesson_count; i++) {
        if (bridge->lessons[i].regret_type == regret_type) {
            /* Update existing lesson */
            bridge->lessons[i].occurrence_count++;
            bridge->lessons[i].last_seen_ms = analysis->trade.timestamp_ms;
            bridge->lessons[i].confidence = minf(
                bridge->lessons[i].confidence * 1.1f, 1.0f);

            *lesson = bridge->lessons[i];
            bridge->stats.lessons_extracted++;

            fin_regret_heartbeat_global("fin_regret_lesson", 1.0f);
            return FIN_REGRET_ERR_OK;
        }
    }

    /* Create new lesson */
    if (bridge->lesson_count >= bridge->lesson_capacity) {
        set_error("Lesson capacity reached");
        return FIN_REGRET_ERR_CAPACITY;
    }

    fin_lesson_t* new_lesson = &bridge->lessons[bridge->lesson_count];
    new_lesson->id = bridge->next_lesson_id++;
    new_lesson->regret_type = regret_type;
    new_lesson->occurrence_count = 1;
    new_lesson->confidence = bridge->config.lesson_confidence_threshold;
    new_lesson->first_seen_ms = analysis->trade.timestamp_ms;
    new_lesson->last_seen_ms = analysis->trade.timestamp_ms;

    /* Generate pattern description */
    snprintf(new_lesson->pattern, sizeof(new_lesson->pattern),
             "%s regret pattern at %.0f%% magnitude",
             fin_regret_type_name(regret_type),
             analysis->regret_magnitude * 100.0f);

    /* Copy lesson text from analysis */
    strncpy(new_lesson->lesson, analysis->lesson, sizeof(new_lesson->lesson) - 1);

    /* Generate action item */
    switch (regret_type) {
        case FIN_REGRET_COMMISSION:
            snprintf(new_lesson->action_item, sizeof(new_lesson->action_item),
                     "Add confirmation step before entry");
            break;
        case FIN_REGRET_OMISSION:
            snprintf(new_lesson->action_item, sizeof(new_lesson->action_item),
                     "Set alerts for clear setups; reduce hesitation");
            break;
        case FIN_REGRET_TIMING:
            snprintf(new_lesson->action_item, sizeof(new_lesson->action_item),
                     "Use limit orders; study optimal entry timing");
            break;
        case FIN_REGRET_SIZING:
            snprintf(new_lesson->action_item, sizeof(new_lesson->action_item),
                     "Review position sizing rules; adjust for conviction");
            break;
        case FIN_REGRET_EXIT:
            snprintf(new_lesson->action_item, sizeof(new_lesson->action_item),
                     "Implement trailing stops; review exit rules");
            break;
        default:
            snprintf(new_lesson->action_item, sizeof(new_lesson->action_item),
                     "Review and improve decision process");
            break;
    }

    bridge->lesson_count++;
    *lesson = *new_lesson;
    bridge->stats.lessons_extracted++;

    /* KG messaging */
    bridge_kg_publish(bridge, KG_MSG_FIN_REGRET_LESSON,
                      lesson, sizeof(*lesson));

    fin_regret_heartbeat_global("fin_regret_lesson", 1.0f);
    return FIN_REGRET_ERR_OK;
}

int financial_regret_bridge_get_lessons(
    financial_regret_bridge_t* bridge,
    fin_lesson_t* lessons,
    uint32_t max_lessons,
    uint32_t* num_lessons)
{
    if (!bridge) {
        set_error("bridge is NULL");
        return FIN_REGRET_ERR_NULL;
    }
    if (!lessons || !num_lessons) {
        set_error("lessons or num_lessons is NULL");
        return FIN_REGRET_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_REGRET_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic");
        return FIN_REGRET_ERR_STATE;
    }

    uint32_t count = bridge->lesson_count < max_lessons ?
                     bridge->lesson_count : max_lessons;

    memcpy(lessons, bridge->lessons, count * sizeof(fin_lesson_t));
    *num_lessons = count;

    return FIN_REGRET_ERR_OK;
}

uint32_t financial_regret_bridge_get_lesson_count(
    const financial_regret_bridge_t* bridge)
{
    if (!bridge || bridge->magic != FINANCIAL_REGRET_BRIDGE_MAGIC) {
        return 0;
    }
    return bridge->lesson_count;
}

int financial_regret_bridge_find_lessons_by_type(
    financial_regret_bridge_t* bridge,
    fin_regret_type_t regret_type,
    fin_lesson_t* lessons,
    uint32_t max_lessons,
    uint32_t* num_lessons)
{
    if (!bridge) {
        set_error("bridge is NULL");
        return FIN_REGRET_ERR_NULL;
    }
    if (!lessons || !num_lessons) {
        set_error("lessons or num_lessons is NULL");
        return FIN_REGRET_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_REGRET_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic");
        return FIN_REGRET_ERR_STATE;
    }

    *num_lessons = 0;

    for (uint32_t i = 0; i < bridge->lesson_count && *num_lessons < max_lessons; i++) {
        if (bridge->lessons[i].regret_type == regret_type) {
            lessons[*num_lessons] = bridge->lessons[i];
            (*num_lessons)++;
        }
    }

    return FIN_REGRET_ERR_OK;
}

/* ============================================================================
 * Query API
 * ============================================================================ */

fin_regret_bridge_state_t financial_regret_bridge_get_state(
    const financial_regret_bridge_t* bridge)
{
    if (!bridge || bridge->magic != FINANCIAL_REGRET_BRIDGE_MAGIC) {
        return FIN_REGRET_STATE_UNINITIALIZED;
    }
    return bridge->op_state;
}

int financial_regret_bridge_get_stats(
    const financial_regret_bridge_t* bridge,
    fin_regret_bridge_stats_t* stats)
{
    if (!bridge) {
        set_error("bridge is NULL");
        return FIN_REGRET_ERR_NULL;
    }
    if (!stats) {
        set_error("stats is NULL");
        return FIN_REGRET_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_REGRET_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic");
        return FIN_REGRET_ERR_STATE;
    }

    *stats = bridge->stats;
    return FIN_REGRET_ERR_OK;
}

void financial_regret_bridge_reset_stats(
    financial_regret_bridge_t* bridge)
{
    if (bridge && bridge->magic == FINANCIAL_REGRET_BRIDGE_MAGIC) {
        memset(&bridge->stats, 0, sizeof(fin_regret_bridge_stats_t));
    }
}

/* ============================================================================
 * Health Integration
 * ============================================================================ */

int financial_regret_bridge_heartbeat(
    financial_regret_bridge_t* bridge,
    const char* operation,
    float progress)
{
    if (!bridge) {
        return FIN_REGRET_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_REGRET_BRIDGE_MAGIC) {
        return FIN_REGRET_ERR_STATE;
    }

    /* Forward to global health agent */
    fin_regret_heartbeat_global(
        operation ? operation : "fin_regret_heartbeat", progress);

    /* Forward to instance-level health agent */
    if (bridge->health_agent) {
        nimcp_health_agent_heartbeat_ex(
            (nimcp_health_agent_t*)bridge->health_agent, operation, progress);
    }

    bridge->stats.health_heartbeats++;
    return FIN_REGRET_ERR_OK;
}

/* ============================================================================
 * Training Integration
 * ============================================================================ */

int financial_regret_bridge_training_begin(
    financial_regret_bridge_t* bridge)
{
    if (!bridge) {
        set_error("bridge is NULL");
        return FIN_REGRET_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_REGRET_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic");
        return FIN_REGRET_ERR_STATE;
    }

    bridge->training_active = true;
    fin_regret_heartbeat_global("fin_regret_train_begin", 0.0f);
    return FIN_REGRET_ERR_OK;
}

int financial_regret_bridge_training_end(
    financial_regret_bridge_t* bridge)
{
    if (!bridge) {
        set_error("bridge is NULL");
        return FIN_REGRET_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_REGRET_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic");
        return FIN_REGRET_ERR_STATE;
    }

    bridge->training_active = false;
    fin_regret_heartbeat_global("fin_regret_train_end", 1.0f);
    return FIN_REGRET_ERR_OK;
}

int financial_regret_bridge_training_step(
    financial_regret_bridge_t* bridge,
    float progress)
{
    if (!bridge) {
        set_error("bridge is NULL");
        return FIN_REGRET_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_REGRET_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic");
        return FIN_REGRET_ERR_STATE;
    }

    fin_regret_heartbeat_global("fin_regret_train_step", progress);
    return FIN_REGRET_ERR_OK;
}
