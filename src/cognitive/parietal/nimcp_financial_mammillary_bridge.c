//=============================================================================
// nimcp_financial_mammillary_bridge.c - Financial Mammillary Memory Bridge
//=============================================================================
/**
 * @file nimcp_financial_mammillary_bridge.c
 * @brief Memory consolidation bridge for financial trade experiences
 *
 * @author NIMCP Development Team
 * @date 2026-01-29
 */

#include "cognitive/parietal/nimcp_financial_mammillary_bridge.h"
#include "constants/nimcp_buffer_constants.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/error/nimcp_error_codes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdarg.h>
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_threshold_constants.h"
#include "utils/math/nimcp_math_helpers.h"

/* Health agent: using pre-existing custom implementation */
static nimcp_health_agent_t* g_fin_mammillary_health_agent = NULL;


/* Forward declarations for immune/BBB validation stubs */
struct brain_immune_system;
typedef struct brain_immune_system brain_immune_system_t;
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

struct bbb_system_struct;
typedef struct bbb_system_struct* bbb_system_t;
extern int bbb_validate_data(bbb_system_t bbb, const void* data,
                              size_t size, const char* context);

/* Stub declarations for subsystem integration globals */
static void* g_fin_mammillary_bridge_immune = NULL;
static void* g_fin_mammillary_bridge_bbb = NULL;

BRIDGE_DEFINE_MESH_REGISTRATION(fin_mammillary, MESH_ADAPTER_CATEGORY_COGNITIVE)


// KG Wiring Integration
//=============================================================================
struct kg_wiring;
typedef struct kg_wiring kg_wiring_t;

/* KG message type defines for mammillary bridge module */
#define KG_MSG_FIN_MAMMILLARY_STORE     "FIN_MAMMILLARY_STORE"
#define KG_MSG_FIN_MAMMILLARY_QUERY     "FIN_MAMMILLARY_QUERY"
#define KG_MSG_FIN_MAMMILLARY_RESULT    "FIN_MAMMILLARY_RESULT"
#define KG_MSG_FIN_MAMMILLARY_CONSOLIDATE "FIN_MAMMILLARY_CONSOLIDATE"
#define KG_MSG_FIN_MAMMILLARY_ERROR     "FIN_MAMMILLARY_ERROR"

//=============================================================================
// Thread-local Error Storage
//=============================================================================

static _Thread_local char fin_mammillary_last_error[NIMCP_ERROR_BUFFER_SIZE] = {0};

static void set_error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(fin_mammillary_last_error, sizeof(fin_mammillary_last_error), fmt, args);
    va_end(args);
}

//=============================================================================
// Heartbeat Helpers
//=============================================================================

static inline void fin_mammillary_heartbeat(const char* operation, float progress) {
    if (g_fin_mammillary_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_fin_mammillary_health_agent, operation, progress);
    }
}

static inline void fin_mammillary_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_fin_mammillary_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_fin_mammillary_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_fin_mammillary_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}

//=============================================================================
// Logging Macros
//=============================================================================

#define FIN_MAMMILLARY_LOG_DEBUG(bridge, fmt, ...) /* placeholder */
#define FIN_MAMMILLARY_LOG_INFO(bridge, fmt, ...)  /* placeholder */
#define FIN_MAMMILLARY_LOG_WARN(bridge, fmt, ...)  /* placeholder */
#define FIN_MAMMILLARY_LOG_ERROR(bridge, fmt, ...) /* placeholder */

//=============================================================================
// Global Immune/BBB Validation Helper
//=============================================================================

static int fin_mammillary_bridge_validate_subsystems_global(const char* operation) {
    if (g_fin_mammillary_bridge_immune) {
        int rc = brain_immune_validate_operation(g_fin_mammillary_bridge_immune, operation, 5);
        if (rc != 0) {
            set_error("financial_mammillary_bridge: immune validation failed for %s", operation);
            return FIN_MAMMILLARY_ERR_IMMUNE;
        }
    }
    if (g_fin_mammillary_bridge_bbb) {
        int rc = bbb_validate_data(g_fin_mammillary_bridge_bbb, NULL, 0, operation);
        if (rc != 0) {
            set_error("financial_mammillary_bridge: BBB validation failed for %s", operation);
            return FIN_MAMMILLARY_ERR_BBB;
        }
    }
    return FIN_MAMMILLARY_ERR_OK;
}

//=============================================================================
// Internal Structure
//=============================================================================

struct financial_mammillary_bridge {
    fin_mammillary_config_t config;
    fin_mammillary_state_t state;
    fin_mammillary_bridge_stats_t stats;

    /* Health modulation */
    float inflammation;
    float fatigue;

    /* Subsystem pointers */
    brain_immune_system_t* immune;
    bbb_system_t bbb;
    void* health_agent;
    void* logger;
    kg_wiring_t* kg_wiring;

    /* Security validation flags */
    bool enable_bbb_validation;
    bool enable_immune_validation;

    /* Memory storage */
    fin_stored_trace_t* traces;
    uint32_t trace_count;
    uint32_t trace_capacity;

    /* RNG state for noise/sampling */
    uint64_t rng_state;
};

//=============================================================================
// Instance-Level Validation Helper
//=============================================================================

static int mammillary_validate_subsystems(financial_mammillary_bridge_t* bridge,
                                          const char* operation) {
    if (!bridge) return FIN_MAMMILLARY_ERR_NULL;

    if (bridge->enable_bbb_validation && bridge->bbb) {
        int rc = bbb_validate_data(bridge->bbb, NULL, 0, operation);
        if (rc != 0) {
            set_error("BBB validation failed for %s", operation);
            NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_BBB_VALIDATION,
                "financial_mammillary: BBB validation failed for %s", operation);
            bridge->stats.bbb_validations++;
            return FIN_MAMMILLARY_ERR_BBB;
        }
        bridge->stats.bbb_validations++;
    }

    if (bridge->enable_immune_validation && bridge->immune) {
        int rc = brain_immune_validate_operation(bridge->immune, operation, 5);
        if (rc != 0) {
            set_error("Immune validation failed for %s", operation);
            NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_BBB_VALIDATION,
                "financial_mammillary: immune validation failed for %s", operation);
            bridge->stats.immune_checks++;
            return FIN_MAMMILLARY_ERR_IMMUNE;
        }
        bridge->stats.immune_checks++;
    }

    return FIN_MAMMILLARY_ERR_OK;
}

//=============================================================================
// Antigen Presentation Helper
//=============================================================================

static void mammillary_present_antigen(financial_mammillary_bridge_t* bridge,
                                       const char* anomaly, uint32_t severity) {
    if (bridge && bridge->immune) {
        uint8_t sig[64] = {0};
        snprintf((char*)sig, sizeof(sig), "fin_mammillary:%s", anomaly);
        uint32_t antigen_id = 0;
        brain_immune_present_antigen(bridge->immune, 0, sig, strlen((char*)sig),
                                      severity, 0, &antigen_id);
    }
}

//=============================================================================
// KG Publish Helper
//=============================================================================

static int mammillary_kg_publish(financial_mammillary_bridge_t* bridge,
                                 const char* msg_type,
                                 const void* payload,
                                 size_t size) {
    if (bridge && bridge->kg_wiring) {
        /* kg_wiring_publish would be called here */
        (void)msg_type; (void)payload; (void)size;
        bridge->stats.kg_messages_sent++;
        return 0;
    }
    return 0;
}

static float mammillary_randf(uint64_t* state) {
    *state = *state * 6364136223846793005ULL + 1442695040888963407ULL;
    return (float)((*state >> 33) & 0x7FFFFFFF) / (float)0x7FFFFFFF;
}

//=============================================================================
// Lifecycle
//=============================================================================

fin_mammillary_config_t financial_mammillary_bridge_default_config(void) {
    fin_mammillary_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));

    cfg.max_traces = FIN_MAMMILLARY_MAX_TRACES;
    cfg.consolidation_threshold = 0.1f;
    cfg.decay_rate = 0.01f;  /* 1% per consolidation cycle */
    cfg.retrieval_boost = 0.1f;
    cfg.enable_emotional_weighting = true;
    cfg.enable_outcome_weighting = true;
    cfg.inflammation_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    cfg.fatigue_sensitivity = NIMCP_SENSITIVITY_DEFAULT;

    return cfg;
}

financial_mammillary_bridge_t* financial_mammillary_bridge_create(
    const fin_mammillary_config_t* config)
{
    fin_mammillary_heartbeat("financial_mammillary_bridge_create", 0.0f);

    financial_mammillary_bridge_t* bridge =
        (financial_mammillary_bridge_t*)nimcp_malloc(sizeof(financial_mammillary_bridge_t));
    if (!bridge) {
        set_error("Failed to allocate financial_mammillary_bridge_t");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NO_MEMORY,
            "Failed to allocate financial_mammillary_bridge_t");
        return NULL;
    }
    memset(bridge, 0, sizeof(*bridge));

    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = financial_mammillary_bridge_default_config();
    }

    /* Validate and cap max_traces */
    if (bridge->config.max_traces == 0) {
        bridge->config.max_traces = FIN_MAMMILLARY_MAX_TRACES;
    }
    if (bridge->config.max_traces > FIN_MAMMILLARY_MAX_TRACES) {
        bridge->config.max_traces = FIN_MAMMILLARY_MAX_TRACES;
    }

    /* Allocate trace storage */
    bridge->traces = (fin_stored_trace_t*)nimcp_calloc(
        bridge->config.max_traces, sizeof(fin_stored_trace_t));
    if (!bridge->traces) {
        set_error("Failed to allocate trace storage");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NO_MEMORY,
            "Failed to allocate trace storage for mammillary bridge");
        nimcp_free(bridge);
        bridge = NULL;
        return NULL;
    }

    bridge->trace_count = 0;
    bridge->trace_capacity = bridge->config.max_traces;
    bridge->state = FIN_MAMMILLARY_STATE_IDLE;
    bridge->inflammation = 0.0f;
    bridge->fatigue = 0.0f;
    bridge->rng_state = 42;

    fin_mammillary_heartbeat("financial_mammillary_bridge_create", 1.0f);
    return bridge;
}

void financial_mammillary_bridge_destroy(financial_mammillary_bridge_t* bridge) {
    if (!bridge) return;
    fin_mammillary_heartbeat("financial_mammillary_bridge_destroy", 0.0f);

    if (bridge->traces) {
        nimcp_free(bridge->traces);
        bridge->traces = NULL;
    }
    nimcp_free(bridge);
    bridge = NULL;

    fin_mammillary_heartbeat("financial_mammillary_bridge_destroy", 1.0f);
}

fin_mammillary_state_t financial_mammillary_bridge_get_state(
    const financial_mammillary_bridge_t* bridge)
{
    if (!bridge) return FIN_MAMMILLARY_STATE_UNINITIALIZED;
    return bridge->state;
}

int financial_mammillary_bridge_reset(financial_mammillary_bridge_t* bridge) {
    if (!bridge) {
        set_error("NULL bridge in reset");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_mammillary_bridge_reset: bridge is NULL");
        return FIN_MAMMILLARY_ERR_NULL;
    }

    fin_mammillary_heartbeat_instance(
        (nimcp_health_agent_t*)bridge->health_agent, "reset", 0.0f);

    bridge->state = FIN_MAMMILLARY_STATE_IDLE;
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    bridge->inflammation = 0.0f;
    bridge->fatigue = 0.0f;

    /* Clear traces */
    if (bridge->traces) {
        memset(bridge->traces, 0,
               bridge->trace_capacity * sizeof(fin_stored_trace_t));
    }
    bridge->trace_count = 0;

    fin_mammillary_heartbeat_instance(
        (nimcp_health_agent_t*)bridge->health_agent, "reset", 1.0f);
    bridge->stats.health_heartbeats += 2;

    return FIN_MAMMILLARY_ERR_OK;
}

//=============================================================================
// Subsystem Setters
//=============================================================================

int financial_mammillary_bridge_set_immune(financial_mammillary_bridge_t* bridge,
                                           void* immune) {
    if (!bridge) {
        set_error("NULL bridge");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_mammillary_bridge_set_immune: bridge is NULL");
        return FIN_MAMMILLARY_ERR_NULL;
    }
    bridge->immune = (brain_immune_system_t*)immune;
    return FIN_MAMMILLARY_ERR_OK;
}

int financial_mammillary_bridge_set_bbb(financial_mammillary_bridge_t* bridge,
                                         void* bbb) {
    if (!bridge) {
        set_error("NULL bridge");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_mammillary_bridge_set_bbb: bridge is NULL");
        return FIN_MAMMILLARY_ERR_NULL;
    }
    bridge->bbb = (bbb_system_t)bbb;
    return FIN_MAMMILLARY_ERR_OK;
}

int financial_mammillary_bridge_set_health_agent(financial_mammillary_bridge_t* bridge,
                                                  void* health_agent) {
    if (!bridge) {
        set_error("NULL bridge");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_mammillary_bridge_set_health_agent: bridge is NULL");
        return FIN_MAMMILLARY_ERR_NULL;
    }
    bridge->health_agent = health_agent;
    return FIN_MAMMILLARY_ERR_OK;
}

int financial_mammillary_bridge_set_logger(financial_mammillary_bridge_t* bridge,
                                            void* logger) {
    if (!bridge) {
        set_error("NULL bridge");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_mammillary_bridge_set_logger: bridge is NULL");
        return FIN_MAMMILLARY_ERR_NULL;
    }
    bridge->logger = logger;
    return FIN_MAMMILLARY_ERR_OK;
}

int financial_mammillary_bridge_enable_bbb_validation(financial_mammillary_bridge_t* bridge,
                                                       bool enable) {
    if (!bridge) {
        set_error("NULL bridge");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_mammillary_bridge_enable_bbb_validation: bridge is NULL");
        return FIN_MAMMILLARY_ERR_NULL;
    }
    bridge->enable_bbb_validation = enable;
    return FIN_MAMMILLARY_ERR_OK;
}

int financial_mammillary_bridge_enable_immune_validation(financial_mammillary_bridge_t* bridge,
                                                          bool enable) {
    if (!bridge) {
        set_error("NULL bridge");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_mammillary_bridge_enable_immune_validation: bridge is NULL");
        return FIN_MAMMILLARY_ERR_NULL;
    }
    bridge->enable_immune_validation = enable;
    return FIN_MAMMILLARY_ERR_OK;
}

int financial_mammillary_bridge_set_kg_wiring(financial_mammillary_bridge_t* bridge,
                                               kg_wiring_t* kg) {
    if (!bridge) {
        set_error("set_kg_wiring: NULL bridge");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_mammillary_bridge_set_kg_wiring: NULL bridge");
        return FIN_MAMMILLARY_ERR_NULL;
    }
    bridge->kg_wiring = kg;
    return FIN_MAMMILLARY_ERR_OK;
}

//=============================================================================
// Importance Computation
//=============================================================================

static float compute_trace_importance(const financial_mammillary_bridge_t* bridge,
                                      const fin_memory_trace_t* trace) {
    float importance = 0.5f;  /* Base importance */

    /* Outcome weighting: larger P&L = more important */
    if (bridge->config.enable_outcome_weighting) {
        float outcome_factor = fabsf(trace->outcome);
        /* Sigmoid-like scaling: importance grows with outcome but saturates */
        float outcome_contribution = outcome_factor / (outcome_factor + 1.0f);
        importance += 0.3f * outcome_contribution;
    }

    /* Emotional weighting: higher emotion = more memorable */
    if (bridge->config.enable_emotional_weighting) {
        importance += 0.2f * trace->emotional_intensity;
    }

    /* Health modulation: inflammation/fatigue impair encoding */
    float health_mod = 1.0f
        - bridge->inflammation * bridge->config.inflammation_sensitivity * 0.2f
        - bridge->fatigue * bridge->config.fatigue_sensitivity * 0.15f;
    if (health_mod < 0.3f) health_mod = 0.3f;
    importance *= health_mod;

    return nimcp_clampf(importance, 0.0f, 1.0f);
}

//=============================================================================
// Core Operations
//=============================================================================

int financial_mammillary_bridge_relay_trade(financial_mammillary_bridge_t* bridge,
                                            const fin_memory_trace_t* trace) {
    return financial_mammillary_bridge_relay_trade_with_context(bridge, trace, NULL, 0);
}

int financial_mammillary_bridge_relay_trade_with_context(
    financial_mammillary_bridge_t* bridge,
    const fin_memory_trace_t* trace,
    const float* context,
    uint32_t context_dim)
{
    if (!bridge) {
        set_error("NULL bridge");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_mammillary_bridge_relay_trade: bridge is NULL");
        return FIN_MAMMILLARY_ERR_NULL;
    }
    if (!trace) {
        set_error("NULL trace");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_mammillary_bridge_relay_trade: trace is NULL");
        return FIN_MAMMILLARY_ERR_NULL;
    }

    /* Instance-level security validation */
    int val_rc = mammillary_validate_subsystems(bridge, "relay_trade");
    if (val_rc != FIN_MAMMILLARY_ERR_OK) return val_rc;

    fin_mammillary_heartbeat_instance(
        (nimcp_health_agent_t*)bridge->health_agent, "relay_trade", 0.0f);
    bridge->stats.health_heartbeats++;

    bridge->state = FIN_MAMMILLARY_STATE_STORING;

    /* Check capacity */
    if (bridge->trace_count >= bridge->trace_capacity) {
        /* Find and replace lowest importance trace */
        uint32_t min_idx = 0;
        float min_importance = bridge->traces[0].importance;
        for (uint32_t i = 1; i < bridge->trace_count; i++) {
            float eff_importance = bridge->traces[i].importance *
                                   bridge->traces[i].consolidation_strength;
            if (eff_importance < min_importance) {
                min_importance = eff_importance;
                min_idx = i;
            }
        }

        /* Only replace if new trace is more important */
        float new_importance = compute_trace_importance(bridge, trace);
        if (new_importance <= min_importance) {
            set_error("Memory full, new trace not important enough");
            bridge->state = FIN_MAMMILLARY_STATE_IDLE;
            fin_mammillary_heartbeat_instance(
                (nimcp_health_agent_t*)bridge->health_agent, "relay_trade", 1.0f);
            bridge->stats.health_heartbeats++;
            return FIN_MAMMILLARY_ERR_OK;  /* Not an error, just not stored */
        }

        /* Replace at min_idx */
        fin_stored_trace_t* slot = &bridge->traces[min_idx];
        memset(slot, 0, sizeof(*slot));
        slot->trace = *trace;
        slot->importance = new_importance;
        slot->consolidation_strength = 1.0f;
        slot->retrieval_count = 0;
        slot->last_retrieval_ms = 0;
        slot->phase = FIN_CONSOLIDATION_ENCODING;

        /* Copy context if provided */
        if (context && context_dim > 0) {
            uint32_t copy_dim = (context_dim < FIN_MAMMILLARY_MAX_CONTEXT_DIM)
                                ? context_dim : FIN_MAMMILLARY_MAX_CONTEXT_DIM;
            for (uint32_t i = 0; i < copy_dim; i++) {
                slot->context[i] = context[i];
            }
            slot->context_dim = copy_dim;
        }
    } else {
        /* Append new trace */
        fin_stored_trace_t* slot = &bridge->traces[bridge->trace_count];
        memset(slot, 0, sizeof(*slot));
        slot->trace = *trace;
        slot->importance = compute_trace_importance(bridge, trace);
        slot->consolidation_strength = 1.0f;
        slot->retrieval_count = 0;
        slot->last_retrieval_ms = 0;
        slot->phase = FIN_CONSOLIDATION_ENCODING;

        /* Copy context if provided */
        if (context && context_dim > 0) {
            uint32_t copy_dim = (context_dim < FIN_MAMMILLARY_MAX_CONTEXT_DIM)
                                ? context_dim : FIN_MAMMILLARY_MAX_CONTEXT_DIM;
            for (uint32_t i = 0; i < copy_dim; i++) {
                slot->context[i] = context[i];
            }
            slot->context_dim = copy_dim;
        }

        bridge->trace_count++;
    }

    bridge->stats.traces_stored++;

    /* Publish to KG */
    mammillary_kg_publish(bridge, KG_MSG_FIN_MAMMILLARY_STORE, trace, sizeof(*trace));

    /* Check for anomalous trades */
    if (fabsf(trace->outcome) > 10.0f) {  /* Large P&L */
        mammillary_present_antigen(bridge, "large_pnl_trade", 2);
    }
    if (trace->emotional_intensity > 0.9f) {
        mammillary_present_antigen(bridge, "high_emotion_trade", 3);
    }

    bridge->state = FIN_MAMMILLARY_STATE_IDLE;

    fin_mammillary_heartbeat_instance(
        (nimcp_health_agent_t*)bridge->health_agent, "relay_trade", 1.0f);
    bridge->stats.health_heartbeats++;

    return FIN_MAMMILLARY_ERR_OK;
}

int financial_mammillary_bridge_consolidate(financial_mammillary_bridge_t* bridge) {
    if (!bridge) {
        set_error("NULL bridge");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_mammillary_bridge_consolidate: bridge is NULL");
        return FIN_MAMMILLARY_ERR_NULL;
    }

    /* Instance-level security validation */
    int val_rc = mammillary_validate_subsystems(bridge, "consolidate");
    if (val_rc != FIN_MAMMILLARY_ERR_OK) return val_rc;

    fin_mammillary_heartbeat_instance(
        (nimcp_health_agent_t*)bridge->health_agent, "consolidate", 0.0f);
    bridge->stats.health_heartbeats++;

    bridge->state = FIN_MAMMILLARY_STATE_CONSOLIDATING;

    float threshold = bridge->config.consolidation_threshold;
    float decay = bridge->config.decay_rate;
    float boost = bridge->config.retrieval_boost;

    /* Health modulation for consolidation */
    float health_mod = 1.0f
        - bridge->inflammation * bridge->config.inflammation_sensitivity * 0.25f
        - bridge->fatigue * bridge->config.fatigue_sensitivity * 0.2f;
    if (health_mod < 0.2f) health_mod = 0.2f;

    /* Adjusted decay based on health */
    float effective_decay = decay / health_mod;

    uint32_t write_idx = 0;
    for (uint32_t i = 0; i < bridge->trace_count; i++) {
        fin_stored_trace_t* t = &bridge->traces[i];

        /* Boost from retrievals */
        if (t->retrieval_count > 0) {
            t->consolidation_strength += boost * (float)t->retrieval_count;
            if (t->consolidation_strength > 5.0f) {
                t->consolidation_strength = 5.0f;
            }
            t->retrieval_count = 0;  /* Reset after consolidation */
            t->phase = FIN_CONSOLIDATION_INTEGRATION;
        } else {
            /* Decay unretrieved memories */
            t->consolidation_strength *= (1.0f - effective_decay);
            if (t->phase < FIN_CONSOLIDATION_STABILIZATION) {
                t->phase = FIN_CONSOLIDATION_STABILIZATION;
            }
        }

        /* Effective importance for retention decision */
        float effective_importance = t->importance * t->consolidation_strength;

        /* Keep if above threshold */
        if (effective_importance >= threshold) {
            if (write_idx != i) {
                bridge->traces[write_idx] = *t;
            }
            /* Advance consolidation phase */
            if (bridge->traces[write_idx].phase < FIN_CONSOLIDATION_COMPLETE) {
                bridge->traces[write_idx].phase++;
            }
            write_idx++;
        }
        /* Else: memory is forgotten (not copied forward) */
    }

    uint32_t forgotten = bridge->trace_count - write_idx;
    bridge->trace_count = write_idx;

    bridge->stats.consolidations++;

    /* Publish consolidation event */
    mammillary_kg_publish(bridge, KG_MSG_FIN_MAMMILLARY_CONSOLIDATE,
                          &forgotten, sizeof(forgotten));

    /* Present antigen if many memories forgotten (potential issue) */
    if (forgotten > bridge->trace_capacity / 4) {
        mammillary_present_antigen(bridge, "mass_memory_decay", 2);
    }

    bridge->state = FIN_MAMMILLARY_STATE_IDLE;

    fin_mammillary_heartbeat_instance(
        (nimcp_health_agent_t*)bridge->health_agent, "consolidate", 1.0f);
    bridge->stats.health_heartbeats++;

    return FIN_MAMMILLARY_ERR_OK;
}

//=============================================================================
// Query Operations
//=============================================================================

fin_query_params_t financial_mammillary_bridge_default_query_params(void) {
    fin_query_params_t params;
    memset(&params, 0, sizeof(params));

    params.target_price = 0.0f;
    params.target_volatility = 0.0f;
    params.target_trend = 0.0f;
    params.price_weight = 0.25f;
    params.volatility_weight = 0.35f;
    params.trend_weight = 0.30f;
    params.outcome_weight = 0.10f;
    params.min_similarity = 0.3f;
    params.max_results = 10;
    params.prefer_profitable = false;
    params.prefer_recent = true;

    return params;
}

int financial_mammillary_bridge_query_similar(
    financial_mammillary_bridge_t* bridge,
    const fin_query_params_t* params,
    fin_query_result_t* results,
    uint32_t max_results,
    uint32_t* out_count)
{
    if (!bridge) {
        set_error("NULL bridge");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_mammillary_bridge_query_similar: bridge is NULL");
        return FIN_MAMMILLARY_ERR_NULL;
    }
    if (!params || !results || !out_count) {
        set_error("NULL parameter");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_mammillary_bridge_query_similar: NULL parameter");
        return FIN_MAMMILLARY_ERR_NULL;
    }

    /* Instance-level security validation */
    int val_rc = mammillary_validate_subsystems(bridge, "query_similar");
    if (val_rc != FIN_MAMMILLARY_ERR_OK) return val_rc;

    fin_mammillary_heartbeat_instance(
        (nimcp_health_agent_t*)bridge->health_agent, "query_similar", 0.0f);
    bridge->stats.health_heartbeats++;

    bridge->state = FIN_MAMMILLARY_STATE_QUERYING;
    *out_count = 0;

    if (bridge->trace_count == 0) {
        bridge->state = FIN_MAMMILLARY_STATE_IDLE;
        bridge->stats.queries++;
        fin_mammillary_heartbeat_instance(
            (nimcp_health_agent_t*)bridge->health_agent, "query_similar", 1.0f);
        bridge->stats.health_heartbeats++;
        return FIN_MAMMILLARY_ERR_OK;
    }

    uint32_t result_cap = (max_results < params->max_results)
                          ? max_results : params->max_results;
    if (result_cap > FIN_MAMMILLARY_MAX_QUERY_RESULTS) {
        result_cap = FIN_MAMMILLARY_MAX_QUERY_RESULTS;
    }

    /* Compute similarity scores for all traces */
    typedef struct {
        uint32_t idx;
        float similarity;
        float relevance;
    } score_entry_t;

    /* Stack allocation for scores (limited by max traces) */
    score_entry_t* scores = (score_entry_t*)alloca(
        bridge->trace_count * sizeof(score_entry_t));

    float total_weight = params->price_weight + params->volatility_weight +
                         params->trend_weight + params->outcome_weight;
    if (total_weight < 0.01f) total_weight = 1.0f;

    for (uint32_t i = 0; i < bridge->trace_count; i++) {
        const fin_stored_trace_t* t = &bridge->traces[i];
        scores[i].idx = i;

        /* Compute similarity components */
        float price_sim = 0.0f;
        if (params->target_price > 0.0f && t->trace.trade_price > 0.0f) {
            float ratio = t->trace.trade_price / params->target_price;
            if (ratio > 1.0f) ratio = 1.0f / (fabsf(ratio) > 1e-7f ? ratio : 1e-7f);
            price_sim = ratio;
        } else {
            price_sim = 1.0f;  /* No price target = full similarity */
        }

        float vol_sim = 1.0f - fabsf(t->trace.market_volatility - params->target_volatility);
        if (vol_sim < 0.0f) vol_sim = 0.0f;

        float trend_sim = 1.0f - fabsf(t->trace.market_trend - params->target_trend) / 2.0f;
        if (trend_sim < 0.0f) trend_sim = 0.0f;

        float outcome_sim = 0.5f;
        if (params->prefer_profitable) {
            outcome_sim = (t->trace.outcome > 0.0f) ? 1.0f : 0.0f;
        }

        /* Weighted combination */
        float similarity = (params->price_weight * price_sim +
                           params->volatility_weight * vol_sim +
                           params->trend_weight * trend_sim +
                           params->outcome_weight * outcome_sim) / (fabsf(total_weight) > 1e-7f ? total_weight : 1e-7f);

        /* Recency bonus */
        if (params->prefer_recent && t->last_retrieval_ms > 0) {
            /* Add small bonus for recently retrieved traces */
            similarity *= 1.05f;
        }

        /* Memory strength bonus */
        similarity *= (0.8f + 0.2f * nimcp_clampf(t->consolidation_strength, 0.0f, 1.0f));

        scores[i].similarity = nimcp_clampf(similarity, 0.0f, 1.0f);

        /* Relevance = similarity * importance */
        scores[i].relevance = scores[i].similarity * t->importance;
    }

    /* Sort by similarity (descending) - simple selection sort */
    for (uint32_t i = 0; i < bridge->trace_count && i < result_cap; i++) {
        uint32_t best = i;
        for (uint32_t j = i + 1; j < bridge->trace_count; j++) {
            if (scores[j].similarity > scores[best].similarity) {
                best = j;
            }
        }
        if (best != i) {
            score_entry_t tmp = scores[i];
            scores[i] = scores[best];
            scores[best] = tmp;
        }
    }

    /* Copy results above threshold */
    uint32_t count = 0;
    for (uint32_t i = 0; i < bridge->trace_count && count < result_cap; i++) {
        if (scores[i].similarity >= params->min_similarity) {
            uint32_t idx = scores[i].idx;
            results[count].trace = bridge->traces[idx];
            results[count].similarity = scores[i].similarity;
            results[count].relevance = scores[i].relevance;
            results[count].index = idx;

            /* Update retrieval count on source trace */
            bridge->traces[idx].retrieval_count++;
            bridge->traces[idx].last_retrieval_ms =
                bridge->traces[idx].trace.timestamp_ms;  /* Approximate */

            count++;
        }
    }

    *out_count = count;
    bridge->stats.queries++;
    bridge->stats.matches_found += count;

    /* Publish query event */
    mammillary_kg_publish(bridge, KG_MSG_FIN_MAMMILLARY_QUERY, params, sizeof(*params));

    bridge->state = FIN_MAMMILLARY_STATE_IDLE;

    fin_mammillary_heartbeat_instance(
        (nimcp_health_agent_t*)bridge->health_agent, "query_similar", 1.0f);
    bridge->stats.health_heartbeats++;

    return FIN_MAMMILLARY_ERR_OK;
}

//=============================================================================
// Memory Access
//=============================================================================

uint32_t financial_mammillary_bridge_get_trace_count(
    const financial_mammillary_bridge_t* bridge)
{
    if (!bridge) return 0;
    return bridge->trace_count;
}

int financial_mammillary_bridge_get_trace(
    const financial_mammillary_bridge_t* bridge,
    uint32_t index,
    fin_stored_trace_t* out_trace)
{
    if (!bridge) {
        set_error("NULL bridge");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_mammillary_bridge_get_trace: bridge is NULL");
        return FIN_MAMMILLARY_ERR_NULL;
    }
    if (!out_trace) {
        set_error("NULL out_trace");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_mammillary_bridge_get_trace: out_trace is NULL");
        return FIN_MAMMILLARY_ERR_NULL;
    }
    if (index >= bridge->trace_count) {
        set_error("Index %u out of range (count=%u)", index, bridge->trace_count);
        return FIN_MAMMILLARY_ERR_INVALID_PARAM;
    }

    *out_trace = bridge->traces[index];
    return FIN_MAMMILLARY_ERR_OK;
}

//=============================================================================
// Health & Modulation
//=============================================================================

int financial_mammillary_bridge_set_inflammation(financial_mammillary_bridge_t* bridge,
                                                  float level) {
    if (!bridge) {
        set_error("NULL bridge");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_mammillary_bridge_set_inflammation: bridge is NULL");
        return FIN_MAMMILLARY_ERR_NULL;
    }
    bridge->inflammation = nimcp_clampf(level, 0.0f, 1.0f);
    return FIN_MAMMILLARY_ERR_OK;
}

int financial_mammillary_bridge_set_fatigue(financial_mammillary_bridge_t* bridge,
                                             float level) {
    if (!bridge) {
        set_error("NULL bridge");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_mammillary_bridge_set_fatigue: bridge is NULL");
        return FIN_MAMMILLARY_ERR_NULL;
    }
    bridge->fatigue = nimcp_clampf(level, 0.0f, 1.0f);
    return FIN_MAMMILLARY_ERR_OK;
}

int financial_mammillary_bridge_get_stats(const financial_mammillary_bridge_t* bridge,
                                           fin_mammillary_bridge_stats_t* stats) {
    if (!bridge) {
        set_error("NULL bridge");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_mammillary_bridge_get_stats: bridge is NULL");
        return FIN_MAMMILLARY_ERR_NULL;
    }
    if (!stats) {
        set_error("NULL stats output");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_mammillary_bridge_get_stats: stats is NULL");
        return FIN_MAMMILLARY_ERR_NULL;
    }
    *stats = bridge->stats;
    return FIN_MAMMILLARY_ERR_OK;
}

void financial_mammillary_bridge_reset_stats(financial_mammillary_bridge_t* bridge) {
    if (!bridge) return;
    memset(&bridge->stats, 0, sizeof(bridge->stats));
}

const char* financial_mammillary_bridge_get_last_error(void) {
    return fin_mammillary_last_error;
}
