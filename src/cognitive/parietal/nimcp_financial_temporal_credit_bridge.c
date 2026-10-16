/**
 * @file nimcp_financial_temporal_credit_bridge.c
 * @brief Financial Temporal Credit Assignment Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-29
 *
 * WHAT: Bridge for temporal credit assignment in financial decision-making,
 *       attributing outcomes to past decisions through eligibility traces.
 *
 * WHY:  Delayed rewards/punishments in trading require propagating credit
 *       back to the decisions that caused them. TD(lambda) and related
 *       methods solve the credit assignment problem.
 *
 * HOW:  Maintains decision history with eligibility traces. When outcomes
 *       are observed, credit is assigned proportionally to eligibility
 *       and temporal proximity.
 *
 * @author NIMCP Development Team
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdarg.h>
#include <time.h>

#include "cognitive/parietal/nimcp_financial_temporal_credit_bridge.h"
#include "constants/nimcp_buffer_constants.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/memory/nimcp_memory.h"

/* ============================================================================
 * Health Agent Integration (Phase 8: System-Wide Health Integration)
 * ============================================================================ */
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(fin_temporal_credit)

/* Stub heartbeat for migration compatibility */
static inline void fin_temporal_credit_heartbeat_global(const char* op, float progress) {
    (void)op; (void)progress;
}
#include "utils/bridge/nimcp_bridge_boilerplate.h"
BRIDGE_DEFINE_MESH_REGISTRATION(fin_temporal_credit, MESH_ADAPTER_CATEGORY_COGNITIVE)


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

static _Thread_local char fin_temporal_credit_last_error[NIMCP_ERROR_BUFFER_SIZE] = {0};

static void set_error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(fin_temporal_credit_last_error, sizeof(fin_temporal_credit_last_error), fmt, args);
    va_end(args);
}

const char* financial_temporal_credit_bridge_get_last_error(void) {
    return fin_temporal_credit_last_error;
}

/* ============================================================================
 * Internal Structure
 * ============================================================================ */

struct financial_temporal_credit_bridge {
    uint32_t magic;
    fin_temporal_credit_config_t config;
    fin_temporal_credit_op_state_t op_state;
    fin_temporal_credit_bridge_stats_t stats;

    /* Decision history */
    fin_decision_t* decisions;
    uint32_t num_decisions;
    uint32_t decisions_capacity;

    /* Extended decision storage (optional) */
    fin_extended_decision_t* extended_decisions;
    uint32_t num_extended;
    uint32_t extended_capacity;

    /* Eligibility traces (parallel to decisions) */
    float* eligibility_traces;

    /* Last trace update time */
    uint64_t last_trace_update_ms;

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
    fin_credit_assigned_callback_t credit_callback;
    void* credit_callback_data;
    fin_eligibility_decayed_callback_t decay_callback;
    void* decay_callback_data;
    fin_custom_credit_fn_t custom_credit_fn;
    void* custom_credit_fn_data;
};

static inline uint64_t get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
}

/* ============================================================================
 * KG Wiring Message Types
 * ============================================================================ */

#define KG_MSG_FIN_TEMPORAL_CREDIT_ASSIGN     "FIN_TEMPORAL_CREDIT_ASSIGN"
#define KG_MSG_FIN_TEMPORAL_CREDIT_DECAY      "FIN_TEMPORAL_CREDIT_DECAY"
#define KG_MSG_FIN_TEMPORAL_CREDIT_DECISION   "FIN_TEMPORAL_CREDIT_DECISION"

static int bridge_kg_publish(financial_temporal_credit_bridge_t* bridge,
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

const char* fin_temporal_credit_op_state_name(fin_temporal_credit_op_state_t state) {
    switch (state) {
        case FIN_TEMPORAL_CREDIT_OP_STATE_UNINITIALIZED: return "UNINITIALIZED";
        case FIN_TEMPORAL_CREDIT_OP_STATE_INITIALIZED:   return "INITIALIZED";
        case FIN_TEMPORAL_CREDIT_OP_STATE_ACTIVE:        return "ACTIVE";
        case FIN_TEMPORAL_CREDIT_OP_STATE_ASSIGNING:     return "ASSIGNING";
        case FIN_TEMPORAL_CREDIT_OP_STATE_DEGRADED:      return "DEGRADED";
        case FIN_TEMPORAL_CREDIT_OP_STATE_ERROR:         return "ERROR";
        default: return "UNKNOWN";
    }
}

const char* fin_temporal_credit_decision_type_name(fin_decision_type_t type) {
    switch (type) {
        case FIN_DECISION_TYPE_BUY:               return "BUY";
        case FIN_DECISION_TYPE_SELL:              return "SELL";
        case FIN_DECISION_TYPE_HOLD:              return "HOLD";
        case FIN_DECISION_TYPE_EXIT_LONG:         return "EXIT_LONG";
        case FIN_DECISION_TYPE_EXIT_SHORT:        return "EXIT_SHORT";
        case FIN_DECISION_TYPE_INCREASE_POSITION: return "INCREASE_POSITION";
        case FIN_DECISION_TYPE_DECREASE_POSITION: return "DECREASE_POSITION";
        case FIN_DECISION_TYPE_STOP_LOSS:         return "STOP_LOSS";
        case FIN_DECISION_TYPE_TAKE_PROFIT:       return "TAKE_PROFIT";
        case FIN_DECISION_TYPE_REBALANCE:         return "REBALANCE";
        case FIN_DECISION_TYPE_HEDGE:             return "HEDGE";
        case FIN_DECISION_TYPE_CUSTOM:            return "CUSTOM";
        default: return "UNKNOWN";
    }
}

const char* fin_temporal_credit_method_name(fin_credit_method_t method) {
    switch (method) {
        case FIN_CREDIT_METHOD_TD_LAMBDA:      return "TD_LAMBDA";
        case FIN_CREDIT_METHOD_MONTE_CARLO:    return "MONTE_CARLO";
        case FIN_CREDIT_METHOD_N_STEP:         return "N_STEP";
        case FIN_CREDIT_METHOD_GAE:            return "GAE";
        case FIN_CREDIT_METHOD_REWARD_SHAPING: return "REWARD_SHAPING";
        case FIN_CREDIT_METHOD_CUSTOM:         return "CUSTOM";
        default: return "UNKNOWN";
    }
}

const char* fin_temporal_credit_trace_replace_name(fin_trace_replacement_t trace_replace) {
    switch (trace_replace) {
        case FIN_TRACE_REPLACE_ACCUMULATING: return "ACCUMULATING";
        case FIN_TRACE_REPLACE_REPLACING:    return "REPLACING";
        case FIN_TRACE_REPLACE_DUTCH:        return "DUTCH";
        default: return "UNKNOWN";
    }
}

const char* financial_temporal_credit_bridge_version(void) {
    return FINANCIAL_TEMPORAL_CREDIT_BRIDGE_VERSION;
}

/* ============================================================================
 * Configuration
 * ============================================================================ */

int financial_temporal_credit_bridge_default_config(fin_temporal_credit_config_t* config) {
    if (!config) {
        set_error("config is NULL");
        return FIN_TEMPORAL_CREDIT_ERR_NULL;
    }

    memset(config, 0, sizeof(fin_temporal_credit_config_t));

    /* TD(lambda) parameters */
    config->lambda = FIN_TEMPORAL_CREDIT_DEFAULT_LAMBDA;
    config->gamma = FIN_TEMPORAL_CREDIT_DEFAULT_GAMMA;
    config->learning_rate = FIN_TEMPORAL_CREDIT_DEFAULT_LR;
    config->trace_threshold = FIN_TEMPORAL_CREDIT_DEFAULT_TRACE_THRESHOLD;

    /* Credit method settings */
    config->method = FIN_CREDIT_METHOD_TD_LAMBDA;
    config->trace_replace = FIN_TRACE_REPLACE_ACCUMULATING;
    config->n_step = 5;

    /* History settings */
    config->max_history_size = FIN_TEMPORAL_CREDIT_MAX_DECISIONS;
    config->history_retention_ms = 24ULL * 60 * 60 * 1000;  /* 24 hours */
    config->auto_prune_history = true;

    /* Causality settings */
    config->enable_causal_filtering = false;
    config->causal_threshold = 0.1f;
    config->use_temporal_window = true;
    config->temporal_window_ms = 4ULL * 60 * 60 * 1000;  /* 4 hours default */

    /* Integration settings */
    config->enable_immune_integration = true;
    config->enable_bbb_validation = true;
    config->enable_kg_messaging = true;
    config->enable_health_monitoring = true;

    /* Logging */
    config->verbose_logging = false;

    return FIN_TEMPORAL_CREDIT_ERR_OK;
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

financial_temporal_credit_bridge_t* financial_temporal_credit_bridge_create(
    const fin_temporal_credit_config_t* config)
{
    fin_temporal_credit_heartbeat_global("fin_temporal_credit_create", 0.0f);

    financial_temporal_credit_bridge_t* bridge = (financial_temporal_credit_bridge_t*)
        nimcp_malloc(sizeof(financial_temporal_credit_bridge_t));
    if (!bridge) {
        set_error("Failed to allocate financial_temporal_credit_bridge");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NO_MEMORY,
            "Failed to allocate financial_temporal_credit_bridge");
        return NULL;
    }
    memset(bridge, 0, sizeof(financial_temporal_credit_bridge_t));

    bridge->magic = FINANCIAL_TEMPORAL_CREDIT_BRIDGE_MAGIC;

    /* Copy configuration or use defaults */
    if (config) {
        bridge->config = *config;
    } else {
        financial_temporal_credit_bridge_default_config(&bridge->config);
    }

    /* Allocate decision history */
    bridge->decisions_capacity = bridge->config.max_history_size;
    bridge->decisions = (fin_decision_t*)nimcp_malloc(
        bridge->decisions_capacity * sizeof(fin_decision_t));
    if (!bridge->decisions) {
        set_error("Failed to allocate decision history");
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NO_MEMORY,
            "Failed to allocate decision history");
        return NULL;
    }
    memset(bridge->decisions, 0, bridge->decisions_capacity * sizeof(fin_decision_t));
    bridge->num_decisions = 0;

    /* Allocate eligibility traces */
    bridge->eligibility_traces = (float*)nimcp_malloc(
        bridge->decisions_capacity * sizeof(float));
    if (!bridge->eligibility_traces) {
        set_error("Failed to allocate eligibility traces");
        nimcp_free(bridge->decisions);
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NO_MEMORY,
            "Failed to allocate eligibility traces");
        return NULL;
    }
    memset(bridge->eligibility_traces, 0, bridge->decisions_capacity * sizeof(float));

    /* Initialize state */
    bridge->op_state = FIN_TEMPORAL_CREDIT_OP_STATE_INITIALIZED;
    bridge->last_trace_update_ms = get_time_ms();

    fin_temporal_credit_heartbeat_global("fin_temporal_credit_create", 1.0f);
    return bridge;
}

void financial_temporal_credit_bridge_destroy(financial_temporal_credit_bridge_t* bridge) {
    fin_temporal_credit_heartbeat_global("fin_temporal_credit_destroy", 0.0f);

    if (bridge) {
        if (bridge->eligibility_traces) {
            nimcp_free(bridge->eligibility_traces);
        }
        if (bridge->decisions) {
            nimcp_free(bridge->decisions);
        }
        if (bridge->extended_decisions) {
            /* Free pattern_ids arrays in extended decisions */
            for (uint32_t i = 0; i < bridge->num_extended; i++) {
                if (bridge->extended_decisions[i].pattern_ids) {
                    nimcp_free(bridge->extended_decisions[i].pattern_ids);
                }
            }
            nimcp_free(bridge->extended_decisions);
        }
        bridge->magic = 0;
        bridge->op_state = FIN_TEMPORAL_CREDIT_OP_STATE_UNINITIALIZED;
        nimcp_free(bridge);
        bridge = NULL;
    }
}

int financial_temporal_credit_bridge_reset(financial_temporal_credit_bridge_t* bridge) {
    if (!bridge) {
        set_error("bridge is NULL");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_temporal_credit_bridge_reset: bridge is NULL");
        return FIN_TEMPORAL_CREDIT_ERR_NULL;
    }

    if (bridge->magic != FINANCIAL_TEMPORAL_CREDIT_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_INVALID_STATE,
            "financial_temporal_credit_bridge_reset: invalid magic");
        return FIN_TEMPORAL_CREDIT_ERR_STATE;
    }

    fin_temporal_credit_heartbeat_global("fin_temporal_credit_reset", 0.0f);

    /* Clear decision history */
    memset(bridge->decisions, 0, bridge->decisions_capacity * sizeof(fin_decision_t));
    bridge->num_decisions = 0;

    /* Clear eligibility traces */
    memset(bridge->eligibility_traces, 0, bridge->decisions_capacity * sizeof(float));

    /* Reset last update time */
    bridge->last_trace_update_ms = get_time_ms();

    bridge->op_state = FIN_TEMPORAL_CREDIT_OP_STATE_INITIALIZED;

    fin_temporal_credit_heartbeat_global("fin_temporal_credit_reset", 1.0f);
    return FIN_TEMPORAL_CREDIT_ERR_OK;
}

/* ============================================================================
 * Subsystem Setters
 * ============================================================================ */

#define FIN_TEMPORAL_CREDIT_SETTER(name, field) \
    int financial_temporal_credit_bridge_set_##name( \
        financial_temporal_credit_bridge_t* bridge, void* ptr) { \
        if (!bridge) { \
            set_error("bridge is NULL in set_" #name); \
            NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, \
                "financial_temporal_credit_bridge_set_" #name ": bridge is NULL"); \
            return FIN_TEMPORAL_CREDIT_ERR_NULL; \
        } \
        if (bridge->magic != FINANCIAL_TEMPORAL_CREDIT_BRIDGE_MAGIC) { \
            set_error("Invalid bridge magic in set_" #name); \
            return FIN_TEMPORAL_CREDIT_ERR_STATE; \
        } \
        bridge->field = ptr; \
        return FIN_TEMPORAL_CREDIT_ERR_OK; \
    }

FIN_TEMPORAL_CREDIT_SETTER(immune, immune)
FIN_TEMPORAL_CREDIT_SETTER(health_agent, health_agent)
FIN_TEMPORAL_CREDIT_SETTER(kg_wiring, kg_wiring)
FIN_TEMPORAL_CREDIT_SETTER(logger, logger)
FIN_TEMPORAL_CREDIT_SETTER(security, security)
FIN_TEMPORAL_CREDIT_SETTER(bio_router, bio_router)

int financial_temporal_credit_bridge_set_coordinator(
    financial_temporal_credit_bridge_t* bridge,
    brain_cycle_coordinator_t* coordinator)
{
    if (!bridge) {
        set_error("bridge is NULL in set_coordinator");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_temporal_credit_bridge_set_coordinator: bridge is NULL");
        return FIN_TEMPORAL_CREDIT_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_TEMPORAL_CREDIT_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic in set_coordinator");
        return FIN_TEMPORAL_CREDIT_ERR_STATE;
    }
    bridge->coordinator = (void*)coordinator;
    return FIN_TEMPORAL_CREDIT_ERR_OK;
}

int financial_temporal_credit_bridge_set_bbb(
    financial_temporal_credit_bridge_t* bridge,
    bbb_system_t bbb)
{
    if (!bridge) {
        set_error("bridge is NULL in set_bbb");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_temporal_credit_bridge_set_bbb: bridge is NULL");
        return FIN_TEMPORAL_CREDIT_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_TEMPORAL_CREDIT_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic in set_bbb");
        return FIN_TEMPORAL_CREDIT_ERR_STATE;
    }
    bridge->bbb = (void*)bbb;
    return FIN_TEMPORAL_CREDIT_ERR_OK;
}

int financial_temporal_credit_bridge_set_ethics(
    financial_temporal_credit_bridge_t* bridge,
    ethics_engine_t ethics)
{
    if (!bridge) {
        set_error("bridge is NULL in set_ethics");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_temporal_credit_bridge_set_ethics: bridge is NULL");
        return FIN_TEMPORAL_CREDIT_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_TEMPORAL_CREDIT_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic in set_ethics");
        return FIN_TEMPORAL_CREDIT_ERR_STATE;
    }
    bridge->ethics = (void*)ethics;
    return FIN_TEMPORAL_CREDIT_ERR_OK;
}

int financial_temporal_credit_bridge_set_lgss(
    financial_temporal_credit_bridge_t* bridge,
    const void* lgss)
{
    if (!bridge) {
        set_error("bridge is NULL in set_lgss");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_temporal_credit_bridge_set_lgss: bridge is NULL");
        return FIN_TEMPORAL_CREDIT_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_TEMPORAL_CREDIT_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic in set_lgss");
        return FIN_TEMPORAL_CREDIT_ERR_STATE;
    }
    bridge->lgss = lgss;
    return FIN_TEMPORAL_CREDIT_ERR_OK;
}

/* ============================================================================
 * Callback Registration
 * ============================================================================ */

int financial_temporal_credit_bridge_set_credit_callback(
    financial_temporal_credit_bridge_t* bridge,
    fin_credit_assigned_callback_t callback,
    void* user_data)
{
    if (!bridge) {
        set_error("bridge is NULL in set_credit_callback");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_temporal_credit_bridge_set_credit_callback: bridge is NULL");
        return FIN_TEMPORAL_CREDIT_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_TEMPORAL_CREDIT_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic in set_credit_callback");
        return FIN_TEMPORAL_CREDIT_ERR_STATE;
    }
    bridge->credit_callback = callback;
    bridge->credit_callback_data = user_data;
    return FIN_TEMPORAL_CREDIT_ERR_OK;
}

int financial_temporal_credit_bridge_set_decay_callback(
    financial_temporal_credit_bridge_t* bridge,
    fin_eligibility_decayed_callback_t callback,
    void* user_data)
{
    if (!bridge) {
        set_error("bridge is NULL in set_decay_callback");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_temporal_credit_bridge_set_decay_callback: bridge is NULL");
        return FIN_TEMPORAL_CREDIT_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_TEMPORAL_CREDIT_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic in set_decay_callback");
        return FIN_TEMPORAL_CREDIT_ERR_STATE;
    }
    bridge->decay_callback = callback;
    bridge->decay_callback_data = user_data;
    return FIN_TEMPORAL_CREDIT_ERR_OK;
}

int financial_temporal_credit_bridge_set_custom_credit_fn(
    financial_temporal_credit_bridge_t* bridge,
    fin_custom_credit_fn_t fn,
    void* user_data)
{
    if (!bridge) {
        set_error("bridge is NULL in set_custom_credit_fn");
        return FIN_TEMPORAL_CREDIT_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_TEMPORAL_CREDIT_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic in set_custom_credit_fn");
        return FIN_TEMPORAL_CREDIT_ERR_STATE;
    }
    bridge->custom_credit_fn = fn;
    bridge->custom_credit_fn_data = user_data;
    return FIN_TEMPORAL_CREDIT_ERR_OK;
}

/* ============================================================================
 * Decision History Management
 * ============================================================================ */

int financial_temporal_credit_bridge_record_decision(
    financial_temporal_credit_bridge_t* bridge,
    const fin_decision_t* decision)
{
    if (!bridge) {
        set_error("bridge is NULL");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_temporal_credit_bridge_record_decision: bridge is NULL");
        return FIN_TEMPORAL_CREDIT_ERR_NULL;
    }
    if (!decision) {
        set_error("decision is NULL");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_temporal_credit_bridge_record_decision: decision is NULL");
        return FIN_TEMPORAL_CREDIT_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_TEMPORAL_CREDIT_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic");
        return FIN_TEMPORAL_CREDIT_ERR_STATE;
    }

    /* Auto-prune if enabled and at capacity */
    if (bridge->num_decisions >= bridge->decisions_capacity) {
        if (bridge->config.auto_prune_history) {
            /* Remove oldest half of history */
            uint32_t keep = bridge->decisions_capacity / 2;
            uint32_t remove = bridge->num_decisions - keep;
            memmove(bridge->decisions, &bridge->decisions[remove],
                    keep * sizeof(fin_decision_t));
            memmove(bridge->eligibility_traces, &bridge->eligibility_traces[remove],
                    keep * sizeof(float));
            bridge->num_decisions = keep;
        } else {
            set_error("Decision history full (%u decisions)", bridge->decisions_capacity);
            return FIN_TEMPORAL_CREDIT_ERR_HISTORY_FULL;
        }
    }

    fin_temporal_credit_heartbeat_global("fin_temporal_credit_record", 0.0f);

    /* BBB validation if enabled */
    if (bridge->config.enable_bbb_validation && bridge->bbb) {
        bridge->stats.bbb_validations++;
        int rc = bbb_validate_data((bbb_system_t)bridge->bbb, decision,
                                    sizeof(*decision), "temporal_credit_record_decision");
        if (rc != 0) {
            set_error("BBB validation failed for record_decision");
            return FIN_TEMPORAL_CREDIT_ERR_BBB;
        }
    }

    /* Add decision to history */
    bridge->decisions[bridge->num_decisions] = *decision;

    /* Initialize eligibility trace to initial eligibility or 1.0 */
    bridge->eligibility_traces[bridge->num_decisions] =
        (decision->eligibility > 0.0f) ? decision->eligibility : 1.0f;

    bridge->num_decisions++;

    /* Update state */
    bridge->op_state = FIN_TEMPORAL_CREDIT_OP_STATE_ACTIVE;

    /* KG messaging */
    bridge_kg_publish(bridge, KG_MSG_FIN_TEMPORAL_CREDIT_DECISION,
                      decision, sizeof(*decision));

    fin_temporal_credit_heartbeat_global("fin_temporal_credit_record", 1.0f);
    return FIN_TEMPORAL_CREDIT_ERR_OK;
}

int financial_temporal_credit_bridge_record_extended_decision(
    financial_temporal_credit_bridge_t* bridge,
    const fin_extended_decision_t* decision)
{
    if (!bridge || !decision) {
        set_error("bridge or decision is NULL");
        return FIN_TEMPORAL_CREDIT_ERR_NULL;
    }

    /* Record base decision */
    int rc = financial_temporal_credit_bridge_record_decision(bridge, &decision->decision);
    if (rc != FIN_TEMPORAL_CREDIT_ERR_OK) {
        return rc;
    }

    /* Store extended info if space available */
    if (bridge->extended_decisions == NULL) {
        bridge->extended_capacity = bridge->decisions_capacity;
        bridge->extended_decisions = (fin_extended_decision_t*)nimcp_calloc(
            bridge->extended_capacity, sizeof(fin_extended_decision_t));
        if (!bridge->extended_decisions) {
            return FIN_TEMPORAL_CREDIT_ERR_OK;  /* Extended storage is optional */
        }
    }

    if (bridge->num_extended < bridge->extended_capacity) {
        bridge->extended_decisions[bridge->num_extended] = *decision;
        /* Deep copy pattern_ids if present */
        if (decision->pattern_ids && decision->num_patterns > 0) {
            bridge->extended_decisions[bridge->num_extended].pattern_ids =
                (uint32_t*)nimcp_calloc(decision->num_patterns, sizeof(uint32_t));
            if (bridge->extended_decisions[bridge->num_extended].pattern_ids) {
                memcpy(bridge->extended_decisions[bridge->num_extended].pattern_ids,
                       decision->pattern_ids,
                       decision->num_patterns * sizeof(uint32_t));
            }
        }
        bridge->num_extended++;
    }

    return FIN_TEMPORAL_CREDIT_ERR_OK;
}

int financial_temporal_credit_bridge_get_history(
    const financial_temporal_credit_bridge_t* bridge,
    fin_decision_history_t* history,
    uint32_t max_decisions)
{
    if (!bridge || !history) {
        return -FIN_TEMPORAL_CREDIT_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_TEMPORAL_CREDIT_BRIDGE_MAGIC) {
        return -FIN_TEMPORAL_CREDIT_ERR_STATE;
    }

    uint32_t count = bridge->num_decisions;
    if (count > max_decisions) {
        count = max_decisions;
    }

    if (history->decisions && count > 0) {
        memcpy(history->decisions, bridge->decisions, count * sizeof(fin_decision_t));
        /* Update eligibility values in returned decisions */
        for (uint32_t i = 0; i < count; i++) {
            history->decisions[i].eligibility = bridge->eligibility_traces[i];
        }
    }
    history->num_decisions = count;

    return (int)count;
}

int financial_temporal_credit_bridge_clear_history(
    financial_temporal_credit_bridge_t* bridge)
{
    if (!bridge) {
        return FIN_TEMPORAL_CREDIT_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_TEMPORAL_CREDIT_BRIDGE_MAGIC) {
        return FIN_TEMPORAL_CREDIT_ERR_STATE;
    }

    memset(bridge->decisions, 0, bridge->decisions_capacity * sizeof(fin_decision_t));
    memset(bridge->eligibility_traces, 0, bridge->decisions_capacity * sizeof(float));
    bridge->num_decisions = 0;

    return FIN_TEMPORAL_CREDIT_ERR_OK;
}

uint32_t financial_temporal_credit_bridge_get_decision_count(
    const financial_temporal_credit_bridge_t* bridge)
{
    if (!bridge || bridge->magic != FINANCIAL_TEMPORAL_CREDIT_BRIDGE_MAGIC) {
        return 0;
    }
    return bridge->num_decisions;
}

uint32_t financial_temporal_credit_bridge_prune_history(
    financial_temporal_credit_bridge_t* bridge,
    uint64_t older_than_ms)
{
    if (!bridge || bridge->magic != FINANCIAL_TEMPORAL_CREDIT_BRIDGE_MAGIC) {
        return 0;
    }

    uint32_t pruned = 0;
    uint32_t write_idx = 0;

    for (uint32_t i = 0; i < bridge->num_decisions; i++) {
        if (bridge->decisions[i].timestamp_ms >= older_than_ms) {
            /* Keep this decision */
            if (write_idx != i) {
                bridge->decisions[write_idx] = bridge->decisions[i];
                bridge->eligibility_traces[write_idx] = bridge->eligibility_traces[i];
            }
            write_idx++;
        } else {
            pruned++;
        }
    }

    bridge->num_decisions = write_idx;
    return pruned;
}

/* ============================================================================
 * Credit Assignment
 * ============================================================================ */

int financial_temporal_credit_bridge_assign(
    financial_temporal_credit_bridge_t* bridge,
    float outcome,
    uint64_t outcome_time_ms,
    fin_credit_assignment_result_t* result)
{
    if (!bridge) {
        set_error("bridge is NULL");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_temporal_credit_bridge_assign: bridge is NULL");
        return FIN_TEMPORAL_CREDIT_ERR_NULL;
    }
    if (!result) {
        set_error("result is NULL");
        return FIN_TEMPORAL_CREDIT_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_TEMPORAL_CREDIT_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic");
        return FIN_TEMPORAL_CREDIT_ERR_STATE;
    }

    if (bridge->num_decisions == 0) {
        set_error("No decisions to assign credit to");
        return FIN_TEMPORAL_CREDIT_ERR_NO_DECISIONS;
    }

    fin_temporal_credit_heartbeat_global("fin_temporal_credit_assign", 0.0f);

    /* Immune check if enabled */
    if (bridge->config.enable_immune_integration && bridge->immune) {
        bridge->stats.immune_checks++;
        int rc = brain_immune_validate_operation(
            (brain_immune_system_t*)bridge->immune,
            "temporal_credit_assign", 5);
        if (rc != 0) {
            set_error("Immune validation failed for assign");
            return FIN_TEMPORAL_CREDIT_ERR_IMMUNE;
        }
    }

    /* Initialize result */
    financial_temporal_credit_result_init(result);

    /* Allocate result array */
    result->results = (fin_credit_result_t*)nimcp_malloc(
        bridge->num_decisions * sizeof(fin_credit_result_t));
    if (!result->results) {
        set_error("Failed to allocate credit results");
        return FIN_TEMPORAL_CREDIT_ERR_NO_MEMORY;
    }
    memset(result->results, 0, bridge->num_decisions * sizeof(fin_credit_result_t));
    result->num_results = bridge->num_decisions;
    result->outcome_value = outcome;
    result->outcome_timestamp_ms = outcome_time_ms;

    bridge->op_state = FIN_TEMPORAL_CREDIT_OP_STATE_ASSIGNING;

    /* Compute credit for each decision */
    float total_credit = 0.0f;
    float total_eligibility = 0.0f;

    for (uint32_t i = 0; i < bridge->num_decisions; i++) {
        fin_decision_t* decision = &bridge->decisions[i];
        fin_credit_result_t* cr = &result->results[i];

        cr->decision_index = i;

        /* Check temporal window */
        if (bridge->config.use_temporal_window) {
            if (decision->timestamp_ms < outcome_time_ms - bridge->config.temporal_window_ms) {
                /* Decision is outside temporal window, no credit */
                cr->credit = 0.0f;
                cr->eligibility_at_outcome = 0.0f;
                cr->temporal_discount = 0.0f;
                cr->causal_weight = 0.0f;
                continue;
            }
        }

        /* Get current eligibility */
        float eligibility = bridge->eligibility_traces[i];
        cr->eligibility_at_outcome = eligibility;

        /* Compute temporal discount */
        float time_diff_ms = (float)(outcome_time_ms - decision->timestamp_ms);
        float time_steps = time_diff_ms / 1000.0f;  /* Convert to seconds */
        float temporal_discount = powf(bridge->config.gamma, time_steps);
        cr->temporal_discount = temporal_discount;

        /* Causal weight (default 1.0, could be modulated) */
        cr->causal_weight = 1.0f;

        /* Compute credit based on method */
        float credit = 0.0f;
        switch (bridge->config.method) {
            case FIN_CREDIT_METHOD_TD_LAMBDA:
                /* TD(lambda): credit = eligibility * discount * outcome * learning_rate */
                credit = eligibility * temporal_discount * outcome * bridge->config.learning_rate;
                break;

            case FIN_CREDIT_METHOD_MONTE_CARLO:
                /* Monte Carlo: full return to all visited states */
                credit = outcome * bridge->config.learning_rate;
                break;

            case FIN_CREDIT_METHOD_N_STEP:
                /* N-step: only credit recent decisions */
                if (i >= bridge->num_decisions - bridge->config.n_step) {
                    credit = temporal_discount * outcome * bridge->config.learning_rate;
                }
                break;

            case FIN_CREDIT_METHOD_GAE:
                /* GAE: Generalized Advantage Estimation */
                credit = eligibility * temporal_discount * outcome * bridge->config.learning_rate;
                break;

            case FIN_CREDIT_METHOD_CUSTOM:
                /* Custom credit function */
                if (bridge->custom_credit_fn) {
                    credit = bridge->custom_credit_fn(decision, outcome, outcome_time_ms,
                                                       bridge->custom_credit_fn_data);
                } else {
                    credit = eligibility * outcome * bridge->config.learning_rate;
                }
                break;

            default:
                credit = eligibility * outcome * bridge->config.learning_rate;
                break;
        }

        cr->credit = credit;
        total_credit += credit;
        total_eligibility += eligibility;

        /* Fire callback */
        if (bridge->credit_callback) {
            bridge->credit_callback(decision, credit, eligibility,
                                    bridge->credit_callback_data);
        }

        fin_temporal_credit_heartbeat_global("fin_temporal_credit_assign",
            (float)(i + 1) / (float)bridge->num_decisions);
    }

    result->total_credit = total_credit;

    /* Update statistics */
    bridge->stats.credit_assignments++;

    /* KG messaging */
    bridge_kg_publish(bridge, KG_MSG_FIN_TEMPORAL_CREDIT_ASSIGN,
                      result, sizeof(*result));

    bridge->op_state = FIN_TEMPORAL_CREDIT_OP_STATE_ACTIVE;

    fin_temporal_credit_heartbeat_global("fin_temporal_credit_assign", 1.0f);
    return FIN_TEMPORAL_CREDIT_ERR_OK;
}

int financial_temporal_credit_bridge_assign_causal(
    financial_temporal_credit_bridge_t* bridge,
    float outcome,
    uint64_t outcome_time_ms,
    const uint32_t* causal_decision_ids,
    uint32_t num_causal_ids,
    fin_credit_assignment_result_t* result)
{
    if (!bridge) {
        set_error("bridge is NULL");
        return FIN_TEMPORAL_CREDIT_ERR_NULL;
    }
    if (!result) {
        set_error("result is NULL");
        return FIN_TEMPORAL_CREDIT_ERR_NULL;
    }
    if (!causal_decision_ids || num_causal_ids == 0) {
        /* Fall back to regular assign if no causal filtering */
        return financial_temporal_credit_bridge_assign(bridge, outcome, outcome_time_ms, result);
    }

    fin_temporal_credit_heartbeat_global("fin_temporal_credit_assign_causal", 0.0f);

    /* Initialize result */
    financial_temporal_credit_result_init(result);

    result->results = (fin_credit_result_t*)nimcp_malloc(
        num_causal_ids * sizeof(fin_credit_result_t));
    if (!result->results) {
        set_error("Failed to allocate credit results");
        return FIN_TEMPORAL_CREDIT_ERR_NO_MEMORY;
    }
    memset(result->results, 0, num_causal_ids * sizeof(fin_credit_result_t));
    result->num_results = num_causal_ids;
    result->outcome_value = outcome;
    result->outcome_timestamp_ms = outcome_time_ms;

    float total_credit = 0.0f;

    for (uint32_t i = 0; i < num_causal_ids; i++) {
        uint32_t decision_idx = causal_decision_ids[i];
        if (decision_idx >= bridge->num_decisions) {
            continue;
        }

        fin_decision_t* decision = &bridge->decisions[decision_idx];
        fin_credit_result_t* cr = &result->results[i];

        cr->decision_index = decision_idx;
        cr->eligibility_at_outcome = bridge->eligibility_traces[decision_idx];

        float time_diff_ms = (float)(outcome_time_ms - decision->timestamp_ms);
        float time_steps = time_diff_ms / 1000.0f;
        cr->temporal_discount = powf(bridge->config.gamma, time_steps);
        cr->causal_weight = 1.0f;

        cr->credit = cr->eligibility_at_outcome * cr->temporal_discount *
                     outcome * bridge->config.learning_rate;
        total_credit += cr->credit;

        if (bridge->credit_callback) {
            bridge->credit_callback(decision, cr->credit, cr->eligibility_at_outcome,
                                    bridge->credit_callback_data);
        }
    }

    result->total_credit = total_credit;
    bridge->stats.credit_assignments++;

    fin_temporal_credit_heartbeat_global("fin_temporal_credit_assign_causal", 1.0f);
    return FIN_TEMPORAL_CREDIT_ERR_OK;
}

/* ============================================================================
 * Eligibility Traces
 * ============================================================================ */

int financial_temporal_credit_bridge_eligibility_trace(
    financial_temporal_credit_bridge_t* bridge,
    uint64_t reference_time_ms,
    fin_eligibility_result_t* result)
{
    if (!bridge) {
        set_error("bridge is NULL");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_temporal_credit_bridge_eligibility_trace: bridge is NULL");
        return FIN_TEMPORAL_CREDIT_ERR_NULL;
    }
    if (!result) {
        set_error("result is NULL");
        return FIN_TEMPORAL_CREDIT_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_TEMPORAL_CREDIT_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic");
        return FIN_TEMPORAL_CREDIT_ERR_STATE;
    }

    fin_temporal_credit_heartbeat_global("fin_temporal_credit_eligibility", 0.0f);

    uint64_t start_time = get_time_ms();
    if (reference_time_ms == 0) {
        reference_time_ms = start_time;
    }

    /* Initialize result */
    financial_temporal_eligibility_result_init(result);

    if (bridge->num_decisions == 0) {
        return FIN_TEMPORAL_CREDIT_ERR_OK;
    }

    result->traces = (float*)nimcp_calloc(bridge->num_decisions, sizeof(float));
    if (!result->traces) {
        set_error("Failed to allocate trace results");
        return FIN_TEMPORAL_CREDIT_ERR_NO_MEMORY;
    }
    result->num_traces = bridge->num_decisions;

    float total = 0.0f;

    for (uint32_t i = 0; i < bridge->num_decisions; i++) {
        /* Compute time-based decay from decision time to reference time */
        float time_diff_ms = (float)(reference_time_ms - bridge->decisions[i].timestamp_ms);
        if (time_diff_ms < 0) time_diff_ms = 0;

        /* Decay per millisecond (approximate continuous decay) */
        float decay_steps = time_diff_ms / 1000.0f;  /* Steps in seconds */
        float trace = bridge->eligibility_traces[i] * powf(bridge->config.lambda, decay_steps);

        /* Threshold small traces to zero */
        if (trace < bridge->config.trace_threshold) {
            trace = 0.0f;
        }

        result->traces[i] = trace;
        total += trace;
    }

    result->total_eligibility = total;
    result->compute_time_us = (get_time_ms() - start_time) * 1000;

    bridge->stats.eligibility_traces++;

    fin_temporal_credit_heartbeat_global("fin_temporal_credit_eligibility", 1.0f);
    return FIN_TEMPORAL_CREDIT_ERR_OK;
}

int financial_temporal_credit_bridge_decay_traces(
    financial_temporal_credit_bridge_t* bridge,
    uint64_t time_delta_ms)
{
    if (!bridge) {
        set_error("bridge is NULL");
        return FIN_TEMPORAL_CREDIT_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_TEMPORAL_CREDIT_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic");
        return FIN_TEMPORAL_CREDIT_ERR_STATE;
    }

    fin_temporal_credit_heartbeat_global("fin_temporal_credit_decay", 0.0f);

    /* Compute decay factor */
    float decay_steps = (float)time_delta_ms / 1000.0f;
    float decay_factor = powf(bridge->config.lambda, decay_steps);

    float total_eligibility = 0.0f;

    for (uint32_t i = 0; i < bridge->num_decisions; i++) {
        bridge->eligibility_traces[i] *= decay_factor;

        /* Threshold small traces */
        if (bridge->eligibility_traces[i] < bridge->config.trace_threshold) {
            bridge->eligibility_traces[i] = 0.0f;
        }

        total_eligibility += bridge->eligibility_traces[i];
    }

    bridge->last_trace_update_ms = get_time_ms();

    /* Fire callback */
    if (bridge->decay_callback) {
        bridge->decay_callback(bridge->num_decisions, total_eligibility,
                               bridge->decay_callback_data);
    }

    /* KG messaging */
    bridge_kg_publish(bridge, KG_MSG_FIN_TEMPORAL_CREDIT_DECAY,
                      &total_eligibility, sizeof(total_eligibility));

    fin_temporal_credit_heartbeat_global("fin_temporal_credit_decay", 1.0f);
    return FIN_TEMPORAL_CREDIT_ERR_OK;
}

int financial_temporal_credit_bridge_boost_eligibility(
    financial_temporal_credit_bridge_t* bridge,
    uint32_t decision_index,
    float boost_amount)
{
    if (!bridge) {
        set_error("bridge is NULL");
        return FIN_TEMPORAL_CREDIT_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_TEMPORAL_CREDIT_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic");
        return FIN_TEMPORAL_CREDIT_ERR_STATE;
    }
    if (decision_index >= bridge->num_decisions) {
        set_error("Invalid decision index");
        return FIN_TEMPORAL_CREDIT_ERR_INVALID_PARAM;
    }

    switch (bridge->config.trace_replace) {
        case FIN_TRACE_REPLACE_ACCUMULATING:
            /* Add to existing trace */
            bridge->eligibility_traces[decision_index] += boost_amount;
            break;

        case FIN_TRACE_REPLACE_REPLACING:
            /* Replace to 1.0 */
            bridge->eligibility_traces[decision_index] = 1.0f;
            break;

        case FIN_TRACE_REPLACE_DUTCH:
            /* Accumulating with cap at 1.0 */
            bridge->eligibility_traces[decision_index] += boost_amount;
            if (bridge->eligibility_traces[decision_index] > 1.0f) {
                bridge->eligibility_traces[decision_index] = 1.0f;
            }
            break;
    }

    return FIN_TEMPORAL_CREDIT_ERR_OK;
}

float financial_temporal_credit_bridge_get_eligibility(
    const financial_temporal_credit_bridge_t* bridge,
    uint32_t decision_index)
{
    if (!bridge || bridge->magic != FINANCIAL_TEMPORAL_CREDIT_BRIDGE_MAGIC) {
        return -1.0f;
    }
    if (decision_index >= bridge->num_decisions) {
        return -1.0f;
    }
    return bridge->eligibility_traces[decision_index];
}

/* ============================================================================
 * Result Management
 * ============================================================================ */

void financial_temporal_credit_result_init(fin_credit_assignment_result_t* result) {
    if (result) {
        memset(result, 0, sizeof(fin_credit_assignment_result_t));
    }
}

void financial_temporal_credit_result_free(fin_credit_assignment_result_t* result) {
    if (result) {
        if (result->results) {
            nimcp_free(result->results);
            result->results = NULL;
        }
        result->num_results = 0;
        result->total_credit = 0.0f;
    }
}

void financial_temporal_eligibility_result_init(fin_eligibility_result_t* result) {
    if (result) {
        memset(result, 0, sizeof(fin_eligibility_result_t));
    }
}

void financial_temporal_eligibility_result_free(fin_eligibility_result_t* result) {
    if (result) {
        if (result->traces) {
            nimcp_free(result->traces);
            result->traces = NULL;
        }
        result->num_traces = 0;
        result->total_eligibility = 0.0f;
    }
}

/* ============================================================================
 * Configuration Updates
 * ============================================================================ */

int financial_temporal_credit_bridge_set_lambda(
    financial_temporal_credit_bridge_t* bridge,
    float lambda)
{
    if (!bridge) {
        return FIN_TEMPORAL_CREDIT_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_TEMPORAL_CREDIT_BRIDGE_MAGIC) {
        return FIN_TEMPORAL_CREDIT_ERR_STATE;
    }
    bridge->config.lambda = nimcp_clampf(lambda, 0.0f, 1.0f);
    return FIN_TEMPORAL_CREDIT_ERR_OK;
}

int financial_temporal_credit_bridge_set_gamma(
    financial_temporal_credit_bridge_t* bridge,
    float gamma)
{
    if (!bridge) {
        return FIN_TEMPORAL_CREDIT_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_TEMPORAL_CREDIT_BRIDGE_MAGIC) {
        return FIN_TEMPORAL_CREDIT_ERR_STATE;
    }
    bridge->config.gamma = nimcp_clampf(gamma, 0.0f, 1.0f);
    return FIN_TEMPORAL_CREDIT_ERR_OK;
}

int financial_temporal_credit_bridge_set_learning_rate(
    financial_temporal_credit_bridge_t* bridge,
    float learning_rate)
{
    if (!bridge) {
        return FIN_TEMPORAL_CREDIT_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_TEMPORAL_CREDIT_BRIDGE_MAGIC) {
        return FIN_TEMPORAL_CREDIT_ERR_STATE;
    }
    bridge->config.learning_rate = learning_rate;
    return FIN_TEMPORAL_CREDIT_ERR_OK;
}

int financial_temporal_credit_bridge_set_method(
    financial_temporal_credit_bridge_t* bridge,
    fin_credit_method_t method)
{
    if (!bridge) {
        return FIN_TEMPORAL_CREDIT_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_TEMPORAL_CREDIT_BRIDGE_MAGIC) {
        return FIN_TEMPORAL_CREDIT_ERR_STATE;
    }
    bridge->config.method = method;
    return FIN_TEMPORAL_CREDIT_ERR_OK;
}

/* ============================================================================
 * Query API
 * ============================================================================ */

fin_temporal_credit_op_state_t financial_temporal_credit_bridge_get_op_state(
    const financial_temporal_credit_bridge_t* bridge)
{
    if (!bridge || bridge->magic != FINANCIAL_TEMPORAL_CREDIT_BRIDGE_MAGIC) {
        return FIN_TEMPORAL_CREDIT_OP_STATE_UNINITIALIZED;
    }
    return bridge->op_state;
}

int financial_temporal_credit_bridge_get_stats(
    const financial_temporal_credit_bridge_t* bridge,
    fin_temporal_credit_bridge_stats_t* stats)
{
    if (!bridge) {
        set_error("bridge is NULL");
        return FIN_TEMPORAL_CREDIT_ERR_NULL;
    }
    if (!stats) {
        set_error("stats is NULL");
        return FIN_TEMPORAL_CREDIT_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_TEMPORAL_CREDIT_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic");
        return FIN_TEMPORAL_CREDIT_ERR_STATE;
    }

    *stats = bridge->stats;
    return FIN_TEMPORAL_CREDIT_ERR_OK;
}

void financial_temporal_credit_bridge_reset_stats(
    financial_temporal_credit_bridge_t* bridge)
{
    if (bridge && bridge->magic == FINANCIAL_TEMPORAL_CREDIT_BRIDGE_MAGIC) {
        memset(&bridge->stats, 0, sizeof(fin_temporal_credit_bridge_stats_t));
    }
}

float financial_temporal_credit_bridge_get_lambda(
    const financial_temporal_credit_bridge_t* bridge)
{
    if (!bridge || bridge->magic != FINANCIAL_TEMPORAL_CREDIT_BRIDGE_MAGIC) {
        return 0.0f;
    }
    return bridge->config.lambda;
}

float financial_temporal_credit_bridge_get_gamma(
    const financial_temporal_credit_bridge_t* bridge)
{
    if (!bridge || bridge->magic != FINANCIAL_TEMPORAL_CREDIT_BRIDGE_MAGIC) {
        return 0.0f;
    }
    return bridge->config.gamma;
}

float financial_temporal_credit_bridge_get_total_eligibility(
    const financial_temporal_credit_bridge_t* bridge)
{
    if (!bridge || bridge->magic != FINANCIAL_TEMPORAL_CREDIT_BRIDGE_MAGIC) {
        return 0.0f;
    }

    float total = 0.0f;
    for (uint32_t i = 0; i < bridge->num_decisions; i++) {
        total += bridge->eligibility_traces[i];
    }
    return total;
}

/* ============================================================================
 * Health Integration
 * ============================================================================ */

int financial_temporal_credit_bridge_heartbeat(
    financial_temporal_credit_bridge_t* bridge,
    const char* operation,
    float progress)
{
    if (!bridge) {
        return FIN_TEMPORAL_CREDIT_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_TEMPORAL_CREDIT_BRIDGE_MAGIC) {
        return FIN_TEMPORAL_CREDIT_ERR_STATE;
    }

    /* Forward to global health agent */
    fin_temporal_credit_heartbeat_global(
        operation ? operation : "fin_temporal_credit_heartbeat", progress);

    /* Forward to instance-level health agent */
    if (bridge->health_agent) {
        nimcp_health_agent_heartbeat_ex(
            (nimcp_health_agent_t*)bridge->health_agent, operation, progress);
    }

    bridge->stats.health_heartbeats++;
    return FIN_TEMPORAL_CREDIT_ERR_OK;
}
