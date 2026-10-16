/**
 * @file nimcp_financial_reasoning_bridge.c
 * @brief Financial Reasoning Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-29
 *
 * WHAT: Bridge for rule-based reasoning in financial decision-making using
 *       forward and backward chaining inference engines.
 *
 * WHY:  Combining forward chaining (derive signals from facts) with backward
 *       chaining (verify hypotheses) enables robust trading decisions that
 *       are both reactive and goal-directed.
 *
 * HOW:  Rules are stored internally. Forward chaining fires matching rules.
 *       Backward chaining recursively verifies goals. Human-readable
 *       reasoning chains explain the inference path.
 *
 * @author NIMCP Development Team
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdarg.h>
#include <ctype.h>

#include "cognitive/parietal/nimcp_financial_reasoning_bridge.h"
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

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(fin_reasoning)

/* Stub heartbeat for migration compatibility */
static inline void fin_reasoning_heartbeat_global(const char* op, float progress) {
    (void)op; (void)progress;
}

BRIDGE_DEFINE_MESH_REGISTRATION(fin_reasoning, MESH_ADAPTER_CATEGORY_COGNITIVE)


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

static _Thread_local char fin_reasoning_last_error[NIMCP_ERROR_BUFFER_SIZE] = {0};

static void set_error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(fin_reasoning_last_error, sizeof(fin_reasoning_last_error), fmt, args);
    va_end(args);
}

const char* financial_reasoning_bridge_get_last_error(void) {
    return fin_reasoning_last_error;
}

/* ============================================================================
 * Internal Structure
 * ============================================================================ */

struct financial_reasoning_bridge {
    uint32_t magic;
    fin_reasoning_config_t config;
    fin_reasoning_op_state_t op_state;
    fin_reasoning_bridge_stats_t stats;

    /* Rule base */
    fin_rule_entry_t* rules;
    uint32_t num_rules;
    uint32_t next_rule_id;

    /* Working memory (facts) */
    fin_fact_t* facts;
    uint32_t num_facts;

    /* Last inference result cache */
    fin_signal_type_t last_signal;
    float last_confidence;
    char last_explanation[FIN_REASONING_CHAIN_LEN];

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
    fin_reasoning_rule_callback_t rule_callback;
    void* rule_callback_data;
    fin_reasoning_signal_callback_t signal_callback;
    void* signal_callback_data;
};

/* ============================================================================
 * KG Wiring Message Types
 * ============================================================================ */

#define KG_MSG_FIN_REASONING_RULE      "FIN_REASONING_RULE"
#define KG_MSG_FIN_REASONING_SIGNAL    "FIN_REASONING_SIGNAL"
#define KG_MSG_FIN_REASONING_VERIFY    "FIN_REASONING_VERIFY"
#define KG_MSG_FIN_REASONING_FACT      "FIN_REASONING_FACT"

static int bridge_kg_publish(financial_reasoning_bridge_t* bridge,
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

const char* fin_reasoning_op_state_name(fin_reasoning_op_state_t state) {
    switch (state) {
        case FIN_REASONING_OP_STATE_UNINITIALIZED: return "UNINITIALIZED";
        case FIN_REASONING_OP_STATE_INITIALIZED:   return "INITIALIZED";
        case FIN_REASONING_OP_STATE_ACTIVE:        return "ACTIVE";
        case FIN_REASONING_OP_STATE_DEGRADED:      return "DEGRADED";
        case FIN_REASONING_OP_STATE_ERROR:         return "ERROR";
        default: return "UNKNOWN";
    }
}

const char* fin_reasoning_signal_name(fin_signal_type_t signal) {
    switch (signal) {
        case FIN_SIGNAL_NONE:        return "NONE";
        case FIN_SIGNAL_STRONG_BUY:  return "STRONG_BUY";
        case FIN_SIGNAL_BUY:         return "BUY";
        case FIN_SIGNAL_WEAK_BUY:    return "WEAK_BUY";
        case FIN_SIGNAL_HOLD:        return "HOLD";
        case FIN_SIGNAL_WEAK_SELL:   return "WEAK_SELL";
        case FIN_SIGNAL_SELL:        return "SELL";
        case FIN_SIGNAL_STRONG_SELL: return "STRONG_SELL";
        case FIN_SIGNAL_AVOID:       return "AVOID";
        case FIN_SIGNAL_ACCUMULATE:  return "ACCUMULATE";
        case FIN_SIGNAL_DISTRIBUTE:  return "DISTRIBUTE";
        case FIN_SIGNAL_HEDGE:       return "HEDGE";
        case FIN_SIGNAL_WAIT:        return "WAIT";
        default: return "UNKNOWN";
    }
}

const char* fin_reasoning_source_name(fin_rule_source_t source) {
    switch (source) {
        case FIN_RULE_SOURCE_CUSTOM:      return "CUSTOM";
        case FIN_RULE_SOURCE_GRAHAM:      return "GRAHAM";
        case FIN_RULE_SOURCE_BUFFETT:     return "BUFFETT";
        case FIN_RULE_SOURCE_SOROS:       return "SOROS";
        case FIN_RULE_SOURCE_LYNCH:       return "LYNCH";
        case FIN_RULE_SOURCE_DALIO:       return "DALIO";
        case FIN_RULE_SOURCE_SIMONS:      return "SIMONS";
        case FIN_RULE_SOURCE_TECHNICAL:   return "TECHNICAL";
        case FIN_RULE_SOURCE_FUNDAMENTAL: return "FUNDAMENTAL";
        case FIN_RULE_SOURCE_SENTIMENT:   return "SENTIMENT";
        default: return "UNKNOWN";
    }
}

const char* fin_reasoning_verify_name(fin_verify_result_t result) {
    switch (result) {
        case FIN_VERIFY_UNKNOWN: return "UNKNOWN";
        case FIN_VERIFY_TRUE:    return "TRUE";
        case FIN_VERIFY_FALSE:   return "FALSE";
        case FIN_VERIFY_PARTIAL: return "PARTIAL";
        default: return "UNKNOWN";
    }
}

const char* financial_reasoning_bridge_version(void) {
    return FINANCIAL_REASONING_BRIDGE_VERSION;
}

/* ============================================================================
 * Configuration
 * ============================================================================ */

int financial_reasoning_bridge_default_config(fin_reasoning_config_t* config) {
    if (!config) {
        set_error("config is NULL");
        return FIN_REASONING_ERR_NULL;
    }

    memset(config, 0, sizeof(fin_reasoning_config_t));

    /* Inference settings */
    config->max_iterations = 100;
    config->max_depth = FIN_REASONING_MAX_DEPTH;
    config->min_confidence = 0.3f;
    config->enable_conflict_resolution = true;
    config->prefer_higher_confidence = true;

    /* Rule loading */
    config->load_graham_rules = true;
    config->load_buffett_rules = true;
    config->load_technical_rules = true;
    config->load_sentiment_rules = true;

    /* Integration settings */
    config->enable_immune_integration = true;
    config->enable_bbb_validation = true;
    config->enable_kg_messaging = true;
    config->enable_health_monitoring = true;

    /* Explanation settings */
    config->verbose_explanations = true;
    config->include_rule_sources = true;

    /* Logging */
    config->verbose_logging = false;

    return FIN_REASONING_ERR_OK;
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

financial_reasoning_bridge_t* financial_reasoning_bridge_create(
    const fin_reasoning_config_t* config)
{
    fin_reasoning_heartbeat_global("fin_reasoning_create", 0.0f);

    financial_reasoning_bridge_t* bridge = (financial_reasoning_bridge_t*)
        nimcp_malloc(sizeof(financial_reasoning_bridge_t));
    if (!bridge) {
        set_error("Failed to allocate financial_reasoning_bridge");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NO_MEMORY,
            "Failed to allocate financial_reasoning_bridge");
        return NULL;
    }
    memset(bridge, 0, sizeof(financial_reasoning_bridge_t));

    bridge->magic = FINANCIAL_REASONING_BRIDGE_MAGIC;

    /* Copy configuration or use defaults */
    if (config) {
        bridge->config = *config;
    } else {
        financial_reasoning_bridge_default_config(&bridge->config);
    }

    /* Allocate rule base */
    bridge->rules = (fin_rule_entry_t*)nimcp_malloc(
        FIN_REASONING_MAX_RULES * sizeof(fin_rule_entry_t));
    if (!bridge->rules) {
        set_error("Failed to allocate rule base");
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NO_MEMORY,
            "Failed to allocate rule base");
        return NULL;
    }
    memset(bridge->rules, 0, FIN_REASONING_MAX_RULES * sizeof(fin_rule_entry_t));
    bridge->num_rules = 0;
    bridge->next_rule_id = 1;

    /* Allocate working memory */
    bridge->facts = (fin_fact_t*)nimcp_malloc(
        FIN_REASONING_MAX_FACTS * sizeof(fin_fact_t));
    if (!bridge->facts) {
        set_error("Failed to allocate working memory");
        nimcp_free(bridge->rules);
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NO_MEMORY,
            "Failed to allocate working memory");
        return NULL;
    }
    memset(bridge->facts, 0, FIN_REASONING_MAX_FACTS * sizeof(fin_fact_t));
    bridge->num_facts = 0;

    /* Initialize state */
    bridge->op_state = FIN_REASONING_OP_STATE_INITIALIZED;
    bridge->last_signal = FIN_SIGNAL_NONE;
    bridge->last_confidence = 0.0f;
    bridge->last_explanation[0] = '\0';

    fin_reasoning_heartbeat_global("fin_reasoning_create", 1.0f);
    return bridge;
}

void financial_reasoning_bridge_destroy(financial_reasoning_bridge_t* bridge) {
    fin_reasoning_heartbeat_global("fin_reasoning_destroy", 0.0f);

    if (bridge) {
        if (bridge->rules) {
            nimcp_free(bridge->rules);
        }
        if (bridge->facts) {
            nimcp_free(bridge->facts);
        }
        bridge->magic = 0;
        bridge->op_state = FIN_REASONING_OP_STATE_UNINITIALIZED;
        nimcp_free(bridge);
        bridge = NULL;
    }
}

int financial_reasoning_bridge_reset(financial_reasoning_bridge_t* bridge) {
    if (!bridge) {
        set_error("bridge is NULL");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_reasoning_bridge_reset: bridge is NULL");
        return FIN_REASONING_ERR_NULL;
    }

    if (bridge->magic != FINANCIAL_REASONING_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_INVALID_STATE,
            "financial_reasoning_bridge_reset: invalid magic");
        return FIN_REASONING_ERR_STATE;
    }

    fin_reasoning_heartbeat_global("fin_reasoning_reset", 0.0f);

    /* Clear working memory but keep rules */
    memset(bridge->facts, 0, FIN_REASONING_MAX_FACTS * sizeof(fin_fact_t));
    bridge->num_facts = 0;

    /* Reset last inference cache */
    bridge->last_signal = FIN_SIGNAL_NONE;
    bridge->last_confidence = 0.0f;
    bridge->last_explanation[0] = '\0';

    bridge->op_state = FIN_REASONING_OP_STATE_INITIALIZED;

    fin_reasoning_heartbeat_global("fin_reasoning_reset", 1.0f);
    return FIN_REASONING_ERR_OK;
}

int financial_reasoning_bridge_clear_rules(financial_reasoning_bridge_t* bridge) {
    if (!bridge) {
        set_error("bridge is NULL");
        return FIN_REASONING_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_REASONING_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic");
        return FIN_REASONING_ERR_STATE;
    }

    memset(bridge->rules, 0, FIN_REASONING_MAX_RULES * sizeof(fin_rule_entry_t));
    bridge->num_rules = 0;
    bridge->next_rule_id = 1;

    return FIN_REASONING_ERR_OK;
}

/* ============================================================================
 * Subsystem Setters
 * ============================================================================ */

#define FIN_REASONING_SETTER(name, field) \
    int financial_reasoning_bridge_set_##name( \
        financial_reasoning_bridge_t* bridge, void* ptr) { \
        if (!bridge) { \
            set_error("bridge is NULL in set_" #name); \
            NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, \
                "financial_reasoning_bridge_set_" #name ": bridge is NULL"); \
            return FIN_REASONING_ERR_NULL; \
        } \
        if (bridge->magic != FINANCIAL_REASONING_BRIDGE_MAGIC) { \
            set_error("Invalid bridge magic in set_" #name); \
            return FIN_REASONING_ERR_STATE; \
        } \
        bridge->field = ptr; \
        return FIN_REASONING_ERR_OK; \
    }

FIN_REASONING_SETTER(immune, immune)
FIN_REASONING_SETTER(health_agent, health_agent)
FIN_REASONING_SETTER(kg_wiring, kg_wiring)
FIN_REASONING_SETTER(logger, logger)
FIN_REASONING_SETTER(security, security)
FIN_REASONING_SETTER(bio_router, bio_router)

int financial_reasoning_bridge_set_coordinator(
    financial_reasoning_bridge_t* bridge,
    brain_cycle_coordinator_t* coordinator)
{
    if (!bridge) {
        set_error("bridge is NULL in set_coordinator");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_reasoning_bridge_set_coordinator: bridge is NULL");
        return FIN_REASONING_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_REASONING_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic in set_coordinator");
        return FIN_REASONING_ERR_STATE;
    }
    bridge->coordinator = (void*)coordinator;
    return FIN_REASONING_ERR_OK;
}

int financial_reasoning_bridge_set_bbb(
    financial_reasoning_bridge_t* bridge,
    bbb_system_t bbb)
{
    if (!bridge) {
        set_error("bridge is NULL in set_bbb");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_reasoning_bridge_set_bbb: bridge is NULL");
        return FIN_REASONING_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_REASONING_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic in set_bbb");
        return FIN_REASONING_ERR_STATE;
    }
    bridge->bbb = (void*)bbb;
    return FIN_REASONING_ERR_OK;
}

int financial_reasoning_bridge_set_ethics(
    financial_reasoning_bridge_t* bridge,
    ethics_engine_t ethics)
{
    if (!bridge) {
        set_error("bridge is NULL in set_ethics");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_reasoning_bridge_set_ethics: bridge is NULL");
        return FIN_REASONING_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_REASONING_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic in set_ethics");
        return FIN_REASONING_ERR_STATE;
    }
    bridge->ethics = (void*)ethics;
    return FIN_REASONING_ERR_OK;
}

int financial_reasoning_bridge_set_lgss(
    financial_reasoning_bridge_t* bridge,
    const void* lgss)
{
    if (!bridge) {
        set_error("bridge is NULL in set_lgss");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_reasoning_bridge_set_lgss: bridge is NULL");
        return FIN_REASONING_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_REASONING_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic in set_lgss");
        return FIN_REASONING_ERR_STATE;
    }
    bridge->lgss = lgss;
    return FIN_REASONING_ERR_OK;
}

/* ============================================================================
 * Callback Registration
 * ============================================================================ */

int financial_reasoning_bridge_set_rule_callback(
    financial_reasoning_bridge_t* bridge,
    fin_reasoning_rule_callback_t callback,
    void* user_data)
{
    if (!bridge) {
        set_error("bridge is NULL in set_rule_callback");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_reasoning_bridge_set_rule_callback: bridge is NULL");
        return FIN_REASONING_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_REASONING_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic in set_rule_callback");
        return FIN_REASONING_ERR_STATE;
    }
    bridge->rule_callback = callback;
    bridge->rule_callback_data = user_data;
    return FIN_REASONING_ERR_OK;
}

int financial_reasoning_bridge_set_signal_callback(
    financial_reasoning_bridge_t* bridge,
    fin_reasoning_signal_callback_t callback,
    void* user_data)
{
    if (!bridge) {
        set_error("bridge is NULL in set_signal_callback");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_reasoning_bridge_set_signal_callback: bridge is NULL");
        return FIN_REASONING_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_REASONING_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic in set_signal_callback");
        return FIN_REASONING_ERR_STATE;
    }
    bridge->signal_callback = callback;
    bridge->signal_callback_data = user_data;
    return FIN_REASONING_ERR_OK;
}

/* ============================================================================
 * Rule Management - Internal Helpers
 * ============================================================================ */

/**
 * @brief Determine rule source type from source string
 */
static fin_rule_source_t parse_source_type(const char* source) {
    if (!source || source[0] == '\0') {
        return FIN_RULE_SOURCE_CUSTOM;
    }

    /* Case-insensitive comparison */
    if (strstr(source, "Graham") || strstr(source, "GRAHAM") || strstr(source, "graham")) {
        return FIN_RULE_SOURCE_GRAHAM;
    }
    if (strstr(source, "Buffett") || strstr(source, "BUFFETT") || strstr(source, "buffett")) {
        return FIN_RULE_SOURCE_BUFFETT;
    }
    if (strstr(source, "Soros") || strstr(source, "SOROS") || strstr(source, "soros")) {
        return FIN_RULE_SOURCE_SOROS;
    }
    if (strstr(source, "Lynch") || strstr(source, "LYNCH") || strstr(source, "lynch")) {
        return FIN_RULE_SOURCE_LYNCH;
    }
    if (strstr(source, "Dalio") || strstr(source, "DALIO") || strstr(source, "dalio")) {
        return FIN_RULE_SOURCE_DALIO;
    }
    if (strstr(source, "Simons") || strstr(source, "SIMONS") || strstr(source, "simons")) {
        return FIN_RULE_SOURCE_SIMONS;
    }
    if (strstr(source, "Technical") || strstr(source, "TECHNICAL") || strstr(source, "technical")) {
        return FIN_RULE_SOURCE_TECHNICAL;
    }
    if (strstr(source, "Fundamental") || strstr(source, "FUNDAMENTAL") || strstr(source, "fundamental")) {
        return FIN_RULE_SOURCE_FUNDAMENTAL;
    }
    if (strstr(source, "Sentiment") || strstr(source, "SENTIMENT") || strstr(source, "sentiment")) {
        return FIN_RULE_SOURCE_SENTIMENT;
    }

    return FIN_RULE_SOURCE_CUSTOM;
}

/* ============================================================================
 * Rule Management API
 * ============================================================================ */

int financial_reasoning_bridge_add_rule(
    financial_reasoning_bridge_t* bridge,
    const fin_trading_rule_t* rule)
{
    if (!bridge) {
        set_error("bridge is NULL");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_reasoning_bridge_add_rule: bridge is NULL");
        return -FIN_REASONING_ERR_NULL;
    }
    if (!rule) {
        set_error("rule is NULL");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_reasoning_bridge_add_rule: rule is NULL");
        return -FIN_REASONING_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_REASONING_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic");
        return -FIN_REASONING_ERR_STATE;
    }

    if (bridge->num_rules >= FIN_REASONING_MAX_RULES) {
        set_error("Rule base full (%u rules)", FIN_REASONING_MAX_RULES);
        return -FIN_REASONING_ERR_RULE_FULL;
    }

    fin_reasoning_heartbeat_global("fin_reasoning_add_rule", 0.0f);

    /* BBB validation if enabled */
    if (bridge->config.enable_bbb_validation && bridge->bbb) {
        bridge->stats.bbb_validations++;
        int rc = bbb_validate_data((bbb_system_t)bridge->bbb, rule,
                                    sizeof(*rule), "reasoning_add_rule");
        if (rc != 0) {
            set_error("BBB validation failed for add_rule");
            return -FIN_REASONING_ERR_BBB;
        }
    }

    /* Add rule to base */
    fin_rule_entry_t* entry = &bridge->rules[bridge->num_rules];
    memset(entry, 0, sizeof(fin_rule_entry_t));

    entry->rule = *rule;
    entry->rule_id = bridge->next_rule_id++;
    entry->source_type = parse_source_type(rule->source);
    entry->priority = 100;  /* Default priority */
    entry->enabled = true;
    entry->fire_count = 0;
    entry->last_fired_ms = 0;

    bridge->num_rules++;
    bridge->stats.rules_added++;

    /* KG messaging */
    bridge_kg_publish(bridge, KG_MSG_FIN_REASONING_RULE, entry, sizeof(*entry));

    fin_reasoning_heartbeat_global("fin_reasoning_add_rule", 1.0f);
    return (int)entry->rule_id;
}

int financial_reasoning_bridge_add_rule_ex(
    financial_reasoning_bridge_t* bridge,
    const fin_rule_entry_t* rule)
{
    if (!bridge) {
        set_error("bridge is NULL");
        return -FIN_REASONING_ERR_NULL;
    }
    if (!rule) {
        set_error("rule is NULL");
        return -FIN_REASONING_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_REASONING_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic");
        return -FIN_REASONING_ERR_STATE;
    }

    if (bridge->num_rules >= FIN_REASONING_MAX_RULES) {
        set_error("Rule base full");
        return -FIN_REASONING_ERR_RULE_FULL;
    }

    /* Add rule with provided metadata */
    fin_rule_entry_t* entry = &bridge->rules[bridge->num_rules];
    *entry = *rule;
    entry->rule_id = bridge->next_rule_id++;

    bridge->num_rules++;
    bridge->stats.rules_added++;

    return (int)entry->rule_id;
}

int financial_reasoning_bridge_remove_rule(
    financial_reasoning_bridge_t* bridge,
    uint32_t rule_id)
{
    if (!bridge) {
        set_error("bridge is NULL");
        return FIN_REASONING_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_REASONING_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic");
        return FIN_REASONING_ERR_STATE;
    }

    /* Find rule */
    for (uint32_t i = 0; i < bridge->num_rules; i++) {
        if (bridge->rules[i].rule_id == rule_id) {
            /* Shift remaining rules */
            memmove(&bridge->rules[i], &bridge->rules[i + 1],
                    (bridge->num_rules - i - 1) * sizeof(fin_rule_entry_t));
            bridge->num_rules--;
            return FIN_REASONING_ERR_OK;
        }
    }

    set_error("Rule ID %u not found", rule_id);
    return FIN_REASONING_ERR_NOT_FOUND;
}

int financial_reasoning_bridge_set_rule_enabled(
    financial_reasoning_bridge_t* bridge,
    uint32_t rule_id,
    bool enabled)
{
    if (!bridge) {
        set_error("bridge is NULL");
        return FIN_REASONING_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_REASONING_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic");
        return FIN_REASONING_ERR_STATE;
    }

    for (uint32_t i = 0; i < bridge->num_rules; i++) {
        if (bridge->rules[i].rule_id == rule_id) {
            bridge->rules[i].enabled = enabled;
            return FIN_REASONING_ERR_OK;
        }
    }

    set_error("Rule ID %u not found", rule_id);
    return FIN_REASONING_ERR_NOT_FOUND;
}

uint32_t financial_reasoning_bridge_get_rule_count(
    const financial_reasoning_bridge_t* bridge)
{
    if (!bridge || bridge->magic != FINANCIAL_REASONING_BRIDGE_MAGIC) {
        return 0;
    }
    return bridge->num_rules;
}

/* ============================================================================
 * Predefined Rules - Graham Value Investing
 * ============================================================================ */

static int load_graham_rules(financial_reasoning_bridge_t* bridge) {
    int count = 0;
    fin_trading_rule_t rule;

    /* Graham Rule 1: Margin of Safety */
    memset(&rule, 0, sizeof(rule));
    snprintf(rule.condition, sizeof(rule.condition),
             "pb_ratio < 0.67 AND pe_ratio < 15");
    snprintf(rule.action, sizeof(rule.action), "SIGNAL:STRONG_BUY");
    rule.confidence = 0.85f;
    snprintf(rule.source, sizeof(rule.source), "Graham - Margin of Safety");
    if (financial_reasoning_bridge_add_rule(bridge, &rule) >= 0) count++;

    /* Graham Rule 2: Defensive Investor */
    memset(&rule, 0, sizeof(rule));
    snprintf(rule.condition, sizeof(rule.condition),
             "pe_ratio < 15 AND current_ratio > 2 AND debt_equity < 1");
    snprintf(rule.action, sizeof(rule.action), "SIGNAL:BUY");
    rule.confidence = 0.80f;
    snprintf(rule.source, sizeof(rule.source), "Graham - Defensive Investor");
    if (financial_reasoning_bridge_add_rule(bridge, &rule) >= 0) count++;

    /* Graham Rule 3: Earnings Stability */
    memset(&rule, 0, sizeof(rule));
    snprintf(rule.condition, sizeof(rule.condition),
             "earnings_growth > 0 AND pe_ratio < 20");
    snprintf(rule.action, sizeof(rule.action), "SIGNAL:WEAK_BUY");
    rule.confidence = 0.70f;
    snprintf(rule.source, sizeof(rule.source), "Graham - Earnings Stability");
    if (financial_reasoning_bridge_add_rule(bridge, &rule) >= 0) count++;

    /* Graham Rule 4: Avoid Overvaluation */
    memset(&rule, 0, sizeof(rule));
    snprintf(rule.condition, sizeof(rule.condition),
             "pe_ratio > 30 OR pb_ratio > 3");
    snprintf(rule.action, sizeof(rule.action), "SIGNAL:AVOID");
    rule.confidence = 0.75f;
    snprintf(rule.source, sizeof(rule.source), "Graham - Avoid Overvaluation");
    if (financial_reasoning_bridge_add_rule(bridge, &rule) >= 0) count++;

    return count;
}

/* ============================================================================
 * Predefined Rules - Buffett Quality Investing
 * ============================================================================ */

static int load_buffett_rules(financial_reasoning_bridge_t* bridge) {
    int count = 0;
    fin_trading_rule_t rule;

    /* Buffett Rule 1: Economic Moat */
    memset(&rule, 0, sizeof(rule));
    snprintf(rule.condition, sizeof(rule.condition),
             "roe > 0.15 AND earnings_growth > 0.1 AND debt_equity < 0.5");
    snprintf(rule.action, sizeof(rule.action), "SIGNAL:STRONG_BUY");
    rule.confidence = 0.90f;
    snprintf(rule.source, sizeof(rule.source), "Buffett - Economic Moat");
    if (financial_reasoning_bridge_add_rule(bridge, &rule) >= 0) count++;

    /* Buffett Rule 2: Quality at Fair Price */
    memset(&rule, 0, sizeof(rule));
    snprintf(rule.condition, sizeof(rule.condition),
             "roe > 0.12 AND pe_ratio < 20 AND revenue_growth > 0.05");
    snprintf(rule.action, sizeof(rule.action), "SIGNAL:BUY");
    rule.confidence = 0.85f;
    snprintf(rule.source, sizeof(rule.source), "Buffett - Quality at Fair Price");
    if (financial_reasoning_bridge_add_rule(bridge, &rule) >= 0) count++;

    /* Buffett Rule 3: Avoid Speculation */
    memset(&rule, 0, sizeof(rule));
    snprintf(rule.condition, sizeof(rule.condition),
             "pe_ratio > 40 AND earnings_growth < 0.15");
    snprintf(rule.action, sizeof(rule.action), "SIGNAL:AVOID");
    rule.confidence = 0.80f;
    snprintf(rule.source, sizeof(rule.source), "Buffett - Avoid Speculation");
    if (financial_reasoning_bridge_add_rule(bridge, &rule) >= 0) count++;

    return count;
}

/* ============================================================================
 * Predefined Rules - Technical Analysis
 * ============================================================================ */

static int load_technical_rules(financial_reasoning_bridge_t* bridge) {
    int count = 0;
    fin_trading_rule_t rule;

    /* RSI Oversold */
    memset(&rule, 0, sizeof(rule));
    snprintf(rule.condition, sizeof(rule.condition), "rsi < 30");
    snprintf(rule.action, sizeof(rule.action), "SIGNAL:ACCUMULATE");
    rule.confidence = 0.70f;
    snprintf(rule.source, sizeof(rule.source), "Technical - RSI Oversold");
    if (financial_reasoning_bridge_add_rule(bridge, &rule) >= 0) count++;

    /* RSI Overbought */
    memset(&rule, 0, sizeof(rule));
    snprintf(rule.condition, sizeof(rule.condition), "rsi > 70");
    snprintf(rule.action, sizeof(rule.action), "SIGNAL:DISTRIBUTE");
    rule.confidence = 0.70f;
    snprintf(rule.source, sizeof(rule.source), "Technical - RSI Overbought");
    if (financial_reasoning_bridge_add_rule(bridge, &rule) >= 0) count++;

    /* Golden Cross (SMA 50 > SMA 200) */
    memset(&rule, 0, sizeof(rule));
    snprintf(rule.condition, sizeof(rule.condition),
             "sma_50 > sma_200 AND price_change_pct > 0");
    snprintf(rule.action, sizeof(rule.action), "SIGNAL:BUY");
    rule.confidence = 0.75f;
    snprintf(rule.source, sizeof(rule.source), "Technical - Golden Cross");
    if (financial_reasoning_bridge_add_rule(bridge, &rule) >= 0) count++;

    /* Death Cross (SMA 50 < SMA 200) */
    memset(&rule, 0, sizeof(rule));
    snprintf(rule.condition, sizeof(rule.condition),
             "sma_50 < sma_200 AND price_change_pct < 0");
    snprintf(rule.action, sizeof(rule.action), "SIGNAL:SELL");
    rule.confidence = 0.75f;
    snprintf(rule.source, sizeof(rule.source), "Technical - Death Cross");
    if (financial_reasoning_bridge_add_rule(bridge, &rule) >= 0) count++;

    /* MACD Bullish Crossover */
    memset(&rule, 0, sizeof(rule));
    snprintf(rule.condition, sizeof(rule.condition), "macd > macd_signal");
    snprintf(rule.action, sizeof(rule.action), "SIGNAL:WEAK_BUY");
    rule.confidence = 0.65f;
    snprintf(rule.source, sizeof(rule.source), "Technical - MACD Bullish");
    if (financial_reasoning_bridge_add_rule(bridge, &rule) >= 0) count++;

    /* MACD Bearish Crossover */
    memset(&rule, 0, sizeof(rule));
    snprintf(rule.condition, sizeof(rule.condition), "macd < macd_signal");
    snprintf(rule.action, sizeof(rule.action), "SIGNAL:WEAK_SELL");
    rule.confidence = 0.65f;
    snprintf(rule.source, sizeof(rule.source), "Technical - MACD Bearish");
    if (financial_reasoning_bridge_add_rule(bridge, &rule) >= 0) count++;

    /* Bollinger Band Breakout */
    memset(&rule, 0, sizeof(rule));
    snprintf(rule.condition, sizeof(rule.condition),
             "current_price < bollinger_lower");
    snprintf(rule.action, sizeof(rule.action), "SIGNAL:ACCUMULATE");
    rule.confidence = 0.65f;
    snprintf(rule.source, sizeof(rule.source), "Technical - Bollinger Lower");
    if (financial_reasoning_bridge_add_rule(bridge, &rule) >= 0) count++;

    return count;
}

/* ============================================================================
 * Predefined Rules - Sentiment Analysis
 * ============================================================================ */

static int load_sentiment_rules(financial_reasoning_bridge_t* bridge) {
    int count = 0;
    fin_trading_rule_t rule;

    /* Extreme Fear */
    memset(&rule, 0, sizeof(rule));
    snprintf(rule.condition, sizeof(rule.condition),
             "sentiment_score < -0.5 AND vix > 30");
    snprintf(rule.action, sizeof(rule.action), "SIGNAL:ACCUMULATE");
    rule.confidence = 0.70f;
    snprintf(rule.source, sizeof(rule.source), "Sentiment - Extreme Fear (Contrarian)");
    if (financial_reasoning_bridge_add_rule(bridge, &rule) >= 0) count++;

    /* Extreme Greed */
    memset(&rule, 0, sizeof(rule));
    snprintf(rule.condition, sizeof(rule.condition),
             "sentiment_score > 0.5 AND vix < 15");
    snprintf(rule.action, sizeof(rule.action), "SIGNAL:DISTRIBUTE");
    rule.confidence = 0.70f;
    snprintf(rule.source, sizeof(rule.source), "Sentiment - Extreme Greed (Contrarian)");
    if (financial_reasoning_bridge_add_rule(bridge, &rule) >= 0) count++;

    /* High Volatility Caution */
    memset(&rule, 0, sizeof(rule));
    snprintf(rule.condition, sizeof(rule.condition), "volatility > 0.5 OR vix > 40");
    snprintf(rule.action, sizeof(rule.action), "SIGNAL:HEDGE");
    rule.confidence = 0.75f;
    snprintf(rule.source, sizeof(rule.source), "Sentiment - High Volatility");
    if (financial_reasoning_bridge_add_rule(bridge, &rule) >= 0) count++;

    return count;
}

int financial_reasoning_bridge_load_rules(
    financial_reasoning_bridge_t* bridge,
    fin_rule_source_t source)
{
    if (!bridge) {
        set_error("bridge is NULL");
        return -FIN_REASONING_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_REASONING_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic");
        return -FIN_REASONING_ERR_STATE;
    }

    int count = 0;

    switch (source) {
        case FIN_RULE_SOURCE_GRAHAM:
            count = load_graham_rules(bridge);
            break;
        case FIN_RULE_SOURCE_BUFFETT:
            count = load_buffett_rules(bridge);
            break;
        case FIN_RULE_SOURCE_TECHNICAL:
            count = load_technical_rules(bridge);
            break;
        case FIN_RULE_SOURCE_SENTIMENT:
            count = load_sentiment_rules(bridge);
            break;
        default:
            /* No predefined rules for other sources */
            count = 0;
            break;
    }

    return count;
}

/* ============================================================================
 * Working Memory (Facts) API
 * ============================================================================ */

int financial_reasoning_bridge_assert_fact(
    financial_reasoning_bridge_t* bridge,
    const fin_fact_t* fact)
{
    if (!bridge) {
        set_error("bridge is NULL");
        return FIN_REASONING_ERR_NULL;
    }
    if (!fact) {
        set_error("fact is NULL");
        return FIN_REASONING_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_REASONING_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic");
        return FIN_REASONING_ERR_STATE;
    }

    /* Check if fact already exists (update) */
    for (uint32_t i = 0; i < bridge->num_facts; i++) {
        if (strncmp(bridge->facts[i].name, fact->name, FIN_REASONING_FACT_LEN) == 0) {
            bridge->facts[i] = *fact;
            return FIN_REASONING_ERR_OK;
        }
    }

    /* Add new fact */
    if (bridge->num_facts >= FIN_REASONING_MAX_FACTS) {
        set_error("Working memory full (%u facts)", FIN_REASONING_MAX_FACTS);
        return FIN_REASONING_ERR_FACT_FULL;
    }

    bridge->facts[bridge->num_facts++] = *fact;

    /* KG messaging */
    bridge_kg_publish(bridge, KG_MSG_FIN_REASONING_FACT, fact, sizeof(*fact));

    return FIN_REASONING_ERR_OK;
}

int financial_reasoning_bridge_assert_numeric(
    financial_reasoning_bridge_t* bridge,
    const char* name,
    float value,
    float confidence)
{
    if (!bridge || !name) {
        return FIN_REASONING_ERR_NULL;
    }

    fin_fact_t fact;
    memset(&fact, 0, sizeof(fact));
    strncpy(fact.name, name, FIN_REASONING_FACT_LEN - 1);
    fact.value = value;
    fact.is_boolean = false;
    fact.confidence = nimcp_clampf(confidence, 0.0f, 1.0f);

    return financial_reasoning_bridge_assert_fact(bridge, &fact);
}

int financial_reasoning_bridge_assert_bool(
    financial_reasoning_bridge_t* bridge,
    const char* name,
    bool value,
    float confidence)
{
    if (!bridge || !name) {
        return FIN_REASONING_ERR_NULL;
    }

    fin_fact_t fact;
    memset(&fact, 0, sizeof(fact));
    strncpy(fact.name, name, FIN_REASONING_FACT_LEN - 1);
    fact.is_boolean = true;
    fact.bool_value = value;
    fact.confidence = nimcp_clampf(confidence, 0.0f, 1.0f);

    return financial_reasoning_bridge_assert_fact(bridge, &fact);
}

int financial_reasoning_bridge_retract_fact(
    financial_reasoning_bridge_t* bridge,
    const char* name)
{
    if (!bridge || !name) {
        return FIN_REASONING_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_REASONING_BRIDGE_MAGIC) {
        return FIN_REASONING_ERR_STATE;
    }

    for (uint32_t i = 0; i < bridge->num_facts; i++) {
        if (strncmp(bridge->facts[i].name, name, FIN_REASONING_FACT_LEN) == 0) {
            memmove(&bridge->facts[i], &bridge->facts[i + 1],
                    (bridge->num_facts - i - 1) * sizeof(fin_fact_t));
            bridge->num_facts--;
            return FIN_REASONING_ERR_OK;
        }
    }

    return FIN_REASONING_ERR_NOT_FOUND;
}

int financial_reasoning_bridge_clear_facts(
    financial_reasoning_bridge_t* bridge)
{
    if (!bridge) {
        return FIN_REASONING_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_REASONING_BRIDGE_MAGIC) {
        return FIN_REASONING_ERR_STATE;
    }

    memset(bridge->facts, 0, FIN_REASONING_MAX_FACTS * sizeof(fin_fact_t));
    bridge->num_facts = 0;

    return FIN_REASONING_ERR_OK;
}

int financial_reasoning_bridge_load_context(
    financial_reasoning_bridge_t* bridge,
    const fin_market_context_t* context)
{
    if (!bridge || !context) {
        return FIN_REASONING_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_REASONING_BRIDGE_MAGIC) {
        return FIN_REASONING_ERR_STATE;
    }

    fin_reasoning_heartbeat_global("fin_reasoning_load_ctx", 0.0f);

    int count = 0;

    /* Price metrics */
    if (financial_reasoning_bridge_assert_numeric(bridge, "current_price", context->current_price, 1.0f) == 0) count++;
    if (financial_reasoning_bridge_assert_numeric(bridge, "price_change_pct", context->price_change_pct, 1.0f) == 0) count++;
    if (financial_reasoning_bridge_assert_numeric(bridge, "price_52w_high", context->price_52w_high, 1.0f) == 0) count++;
    if (financial_reasoning_bridge_assert_numeric(bridge, "price_52w_low", context->price_52w_low, 1.0f) == 0) count++;

    /* Valuation */
    if (financial_reasoning_bridge_assert_numeric(bridge, "pe_ratio", context->pe_ratio, 0.95f) == 0) count++;
    if (financial_reasoning_bridge_assert_numeric(bridge, "pb_ratio", context->pb_ratio, 0.95f) == 0) count++;
    if (financial_reasoning_bridge_assert_numeric(bridge, "ps_ratio", context->ps_ratio, 0.95f) == 0) count++;
    if (financial_reasoning_bridge_assert_numeric(bridge, "peg_ratio", context->peg_ratio, 0.90f) == 0) count++;

    /* Technical */
    if (financial_reasoning_bridge_assert_numeric(bridge, "rsi", context->rsi, 1.0f) == 0) count++;
    if (financial_reasoning_bridge_assert_numeric(bridge, "macd", context->macd, 0.95f) == 0) count++;
    if (financial_reasoning_bridge_assert_numeric(bridge, "macd_signal", context->macd_signal, 0.95f) == 0) count++;
    if (financial_reasoning_bridge_assert_numeric(bridge, "sma_20", context->sma_20, 1.0f) == 0) count++;
    if (financial_reasoning_bridge_assert_numeric(bridge, "sma_50", context->sma_50, 1.0f) == 0) count++;
    if (financial_reasoning_bridge_assert_numeric(bridge, "sma_200", context->sma_200, 1.0f) == 0) count++;
    if (financial_reasoning_bridge_assert_numeric(bridge, "bollinger_upper", context->bollinger_upper, 0.95f) == 0) count++;
    if (financial_reasoning_bridge_assert_numeric(bridge, "bollinger_lower", context->bollinger_lower, 0.95f) == 0) count++;

    /* Volume */
    if (financial_reasoning_bridge_assert_numeric(bridge, "volume_ratio", context->volume_ratio, 0.90f) == 0) count++;

    /* Fundamentals */
    if (financial_reasoning_bridge_assert_numeric(bridge, "roe", context->roe, 0.90f) == 0) count++;
    if (financial_reasoning_bridge_assert_numeric(bridge, "debt_equity", context->debt_equity, 0.90f) == 0) count++;
    if (financial_reasoning_bridge_assert_numeric(bridge, "current_ratio", context->current_ratio, 0.90f) == 0) count++;
    if (financial_reasoning_bridge_assert_numeric(bridge, "earnings_growth", context->earnings_growth, 0.85f) == 0) count++;
    if (financial_reasoning_bridge_assert_numeric(bridge, "revenue_growth", context->revenue_growth, 0.85f) == 0) count++;

    /* Sentiment */
    if (financial_reasoning_bridge_assert_numeric(bridge, "sentiment_score", context->sentiment_score, 0.80f) == 0) count++;

    /* Volatility */
    if (financial_reasoning_bridge_assert_numeric(bridge, "volatility", context->volatility, 0.95f) == 0) count++;
    if (financial_reasoning_bridge_assert_numeric(bridge, "vix", context->vix, 0.95f) == 0) count++;

    fin_reasoning_heartbeat_global("fin_reasoning_load_ctx", 1.0f);
    return count;
}

/* ============================================================================
 * Condition Evaluation - Internal
 * ============================================================================ */

/**
 * @brief Get fact value by name
 */
static bool get_fact_value(financial_reasoning_bridge_t* bridge,
                           const char* name,
                           float* out_value)
{
    for (uint32_t i = 0; i < bridge->num_facts; i++) {
        if (strcmp(bridge->facts[i].name, name) == 0) {
            if (bridge->facts[i].is_boolean) {
                *out_value = bridge->facts[i].bool_value ? 1.0f : 0.0f;
            } else {
                *out_value = bridge->facts[i].value;
            }
            return true;
        }
    }
    return false;
}

/**
 * @brief Parse a simple comparison (e.g., "pe_ratio < 15")
 */
static bool evaluate_simple_condition(financial_reasoning_bridge_t* bridge,
                                       const char* condition)
{
    char var[NIMCP_ID_BUFFER_SIZE] = {0};
    char op[4] = {0};
    float threshold = 0.0f;

    /* Parse: var op value */
    int parsed = sscanf(condition, "%63s %3s %f", var, op, &threshold);
    if (parsed != 3) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "evaluate_simple_condition: validation failed");
        return false;
    }

    float value = 0.0f;
    if (!get_fact_value(bridge, var, &value)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "evaluate_simple_condition: get_fact_value is NULL");
        return false;  /* Unknown fact */
    }

    /* Evaluate comparison */
    if (strcmp(op, "<") == 0) {
        return value < threshold;
    } else if (strcmp(op, ">") == 0) {
        return value > threshold;
    } else if (strcmp(op, "<=") == 0) {
        return value <= threshold;
    } else if (strcmp(op, ">=") == 0) {
        return value >= threshold;
    } else if (strcmp(op, "==") == 0 || strcmp(op, "=") == 0) {
        return fabsf(value - threshold) < 0.0001f;
    } else if (strcmp(op, "!=") == 0) {
        return fabsf(value - threshold) >= 0.0001f;
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "evaluate_simple_condition: operation failed");
    return false;
}

/**
 * @brief Evaluate a compound condition with AND/OR
 */
static bool evaluate_condition(financial_reasoning_bridge_t* bridge,
                                const char* condition)
{
    if (!condition || condition[0] == '\0') {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "evaluate_condition: condition is NULL");
        return false;
    }

    /* Make a working copy */
    char work[FIN_REASONING_CONDITION_LEN];
    strncpy(work, condition, FIN_REASONING_CONDITION_LEN - 1);
    work[FIN_REASONING_CONDITION_LEN - 1] = '\0';

    /* Split on AND (highest precedence for now) */
    char* and_save = NULL;
    char* and_part = strtok_r(work, " ", &and_save);

    bool result = true;
    char simple[NIMCP_LABEL_BUFFER_SIZE] = {0};
    int simple_idx = 0;

    while (and_part != NULL) {
        if (strcmp(and_part, "AND") == 0) {
            /* Evaluate accumulated simple condition */
            if (simple_idx > 0) {
                simple[simple_idx] = '\0';
                bool partial = evaluate_simple_condition(bridge, simple);
                result = result && partial;
                if (!result) {
                    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "evaluate_condition: result is NULL");
                    return false;  /* Short-circuit */
                }
                simple_idx = 0;
                simple[0] = '\0';
            }
        } else if (strcmp(and_part, "OR") == 0) {
            /* Handle OR: evaluate accumulated, then check for alternatives */
            if (simple_idx > 0) {
                simple[simple_idx] = '\0';
                bool partial = evaluate_simple_condition(bridge, simple);
                if (partial) {
                    return true;  /* Short-circuit OR */
                }
                simple_idx = 0;
                simple[0] = '\0';
            }
            result = false;  /* Reset for OR branch */
        } else {
            /* Accumulate tokens */
            if (simple_idx > 0) {
                simple[simple_idx++] = ' ';
            }
            size_t len = strlen(and_part);
            if (simple_idx + len < sizeof(simple) - 1) {
                memcpy(simple + simple_idx, and_part, len);
                simple_idx += len;
            }
        }

        and_part = strtok_r(NULL, " ", &and_save);
    }

    /* Evaluate final accumulated condition */
    if (simple_idx > 0) {
        simple[simple_idx] = '\0';
        bool partial = evaluate_simple_condition(bridge, simple);
        result = result && partial;
    }

    return result;
}

/**
 * @brief Parse signal from action string
 */
static fin_signal_type_t parse_signal_from_action(const char* action) {
    if (!action) {
        return FIN_SIGNAL_NONE;
    }

    if (strstr(action, "STRONG_BUY")) return FIN_SIGNAL_STRONG_BUY;
    if (strstr(action, "STRONG_SELL")) return FIN_SIGNAL_STRONG_SELL;
    if (strstr(action, "WEAK_BUY")) return FIN_SIGNAL_WEAK_BUY;
    if (strstr(action, "WEAK_SELL")) return FIN_SIGNAL_WEAK_SELL;
    if (strstr(action, "BUY")) return FIN_SIGNAL_BUY;
    if (strstr(action, "SELL")) return FIN_SIGNAL_SELL;
    if (strstr(action, "HOLD")) return FIN_SIGNAL_HOLD;
    if (strstr(action, "AVOID")) return FIN_SIGNAL_AVOID;
    if (strstr(action, "ACCUMULATE")) return FIN_SIGNAL_ACCUMULATE;
    if (strstr(action, "DISTRIBUTE")) return FIN_SIGNAL_DISTRIBUTE;
    if (strstr(action, "HEDGE")) return FIN_SIGNAL_HEDGE;
    if (strstr(action, "WAIT")) return FIN_SIGNAL_WAIT;

    return FIN_SIGNAL_NONE;
}

/* ============================================================================
 * Forward Chaining Inference
 * ============================================================================ */

int financial_reasoning_bridge_derive_signals(
    financial_reasoning_bridge_t* bridge,
    fin_reasoning_result_t* result)
{
    if (!bridge) {
        set_error("bridge is NULL");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_reasoning_bridge_derive_signals: bridge is NULL");
        return FIN_REASONING_ERR_NULL;
    }
    if (!result) {
        set_error("result is NULL");
        return FIN_REASONING_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_REASONING_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic");
        return FIN_REASONING_ERR_STATE;
    }

    fin_reasoning_heartbeat_global("fin_reasoning_derive", 0.0f);

    /* Immune check if enabled */
    if (bridge->config.enable_immune_integration && bridge->immune) {
        bridge->stats.immune_checks++;
        int rc = brain_immune_validate_operation(
            (brain_immune_system_t*)bridge->immune,
            "reasoning_derive", 5);
        if (rc != 0) {
            set_error("Immune validation failed for derive_signals");
            return FIN_REASONING_ERR_IMMUNE;
        }
    }

    /* Initialize result */
    financial_reasoning_result_init(result);

    /* Allocate arrays for triggered rules and signals */
    result->triggered_rules = (fin_trading_rule_t*)nimcp_malloc(
        FIN_REASONING_MAX_TRIGGERED * sizeof(fin_trading_rule_t));
    result->derived_signals = (int*)nimcp_malloc(
        FIN_REASONING_MAX_SIGNALS * sizeof(int));

    if (!result->triggered_rules || !result->derived_signals) {
        financial_reasoning_result_free(result);
        set_error("Failed to allocate result arrays");
        return FIN_REASONING_ERR_NO_MEMORY;
    }

    /* Forward chaining: iterate through rules */
    char chain_buffer[FIN_REASONING_CHAIN_LEN] = {0};
    int chain_len = 0;
    float best_confidence = 0.0f;
    fin_signal_type_t best_signal = FIN_SIGNAL_NONE;

    for (uint32_t i = 0; i < bridge->num_rules; i++) {
        fin_rule_entry_t* entry = &bridge->rules[i];

        if (!entry->enabled) {
            continue;
        }
        if (entry->rule.confidence < bridge->config.min_confidence) {
            continue;
        }

        /* Evaluate condition */
        if (evaluate_condition(bridge, entry->rule.condition)) {
            /* Rule fires */
            if (result->num_triggered < FIN_REASONING_MAX_TRIGGERED) {
                result->triggered_rules[result->num_triggered++] = entry->rule;
            }

            /* Extract signal */
            fin_signal_type_t signal = parse_signal_from_action(entry->rule.action);
            if (signal != FIN_SIGNAL_NONE && result->num_signals < FIN_REASONING_MAX_SIGNALS) {
                result->derived_signals[result->num_signals++] = (int)signal;
                bridge->stats.signals_derived++;

                /* Track best signal */
                if (entry->rule.confidence > best_confidence) {
                    best_confidence = entry->rule.confidence;
                    best_signal = signal;
                }

                /* Fire signal callback */
                if (bridge->signal_callback) {
                    bridge->signal_callback(signal, entry->rule.confidence,
                                            entry->rule.source, bridge->signal_callback_data);
                }
            }

            /* Update rule stats */
            entry->fire_count++;

            /* Build reasoning chain */
            int written = snprintf(chain_buffer + chain_len,
                                   FIN_REASONING_CHAIN_LEN - chain_len,
                                   "[%s] IF %s THEN %s (conf=%.2f)\n",
                                   entry->rule.source,
                                   entry->rule.condition,
                                   entry->rule.action,
                                   entry->rule.confidence);
            if (written > 0 && chain_len + written < FIN_REASONING_CHAIN_LEN) {
                chain_len += written;
            }

            /* Fire rule callback */
            if (bridge->rule_callback) {
                bridge->rule_callback(entry, result, bridge->rule_callback_data);
            }

            /* KG messaging */
            bridge_kg_publish(bridge, KG_MSG_FIN_REASONING_SIGNAL,
                              &signal, sizeof(signal));
        }
    }

    /* Copy reasoning chain to result */
    strncpy(result->reasoning_chain, chain_buffer, FIN_REASONING_CHAIN_LEN - 1);

    /* Cache last result */
    bridge->last_signal = best_signal;
    bridge->last_confidence = best_confidence;
    strncpy(bridge->last_explanation, chain_buffer, FIN_REASONING_CHAIN_LEN - 1);

    /* Update state */
    bridge->op_state = FIN_REASONING_OP_STATE_ACTIVE;
    bridge->stats.forward_inferences++;

    fin_reasoning_heartbeat_global("fin_reasoning_derive", 1.0f);
    return FIN_REASONING_ERR_OK;
}

/* ============================================================================
 * Backward Chaining Inference
 * ============================================================================ */

/**
 * @brief Recursive backward chaining helper
 */
static fin_verify_result_t backward_chain_recursive(
    financial_reasoning_bridge_t* bridge,
    const char* goal,
    uint32_t depth,
    uint32_t max_depth,
    uint32_t* rules_checked,
    char* explanation,
    size_t exp_size,
    size_t* exp_len)
{
    if (depth >= max_depth) {
        return FIN_VERIFY_UNKNOWN;
    }

    (*rules_checked)++;

    /* Check if goal is a fact in working memory */
    float value = 0.0f;
    if (get_fact_value(bridge, goal, &value)) {
        /* Fact found - check if it satisfies the goal */
        int written = snprintf(explanation + *exp_len, exp_size - *exp_len,
                               "%*sFACT: %s = %.2f\n", (int)depth * 2, "", goal, value);
        if (written > 0 && *exp_len + written < exp_size) {
            *exp_len += written;
        }
        return (value > 0.5f) ? FIN_VERIFY_TRUE : FIN_VERIFY_FALSE;
    }

    /* Check if goal matches a simple condition */
    if (evaluate_simple_condition(bridge, goal)) {
        int written = snprintf(explanation + *exp_len, exp_size - *exp_len,
                               "%*sVERIFIED: %s\n", (int)depth * 2, "", goal);
        if (written > 0 && *exp_len + written < exp_size) {
            *exp_len += written;
        }
        return FIN_VERIFY_TRUE;
    }

    /* Search rules that conclude the goal */
    for (uint32_t i = 0; i < bridge->num_rules; i++) {
        fin_rule_entry_t* entry = &bridge->rules[i];

        if (!entry->enabled) {
            continue;
        }

        /* Check if rule action contains the goal signal */
        if (strstr(entry->rule.action, goal) != NULL) {
            /* Found matching rule - verify its condition */
            int written = snprintf(explanation + *exp_len, exp_size - *exp_len,
                                   "%*sTRY RULE: %s [%s]\n",
                                   (int)depth * 2, "",
                                   entry->rule.condition,
                                   entry->rule.source);
            if (written > 0 && *exp_len + written < exp_size) {
                *exp_len += written;
            }

            /* Recursively verify condition */
            fin_verify_result_t sub_result = backward_chain_recursive(
                bridge, entry->rule.condition,
                depth + 1, max_depth, rules_checked,
                explanation, exp_size, exp_len);

            if (sub_result == FIN_VERIFY_TRUE) {
                int written2 = snprintf(explanation + *exp_len, exp_size - *exp_len,
                                        "%*sSUCCESS via [%s]\n",
                                        (int)depth * 2, "", entry->rule.source);
                if (written2 > 0 && *exp_len + written2 < exp_size) {
                    *exp_len += written2;
                }
                return FIN_VERIFY_TRUE;
            } else if (sub_result == FIN_VERIFY_PARTIAL) {
                /* Continue searching for better match */
            }
        }
    }

    /* Try direct condition evaluation */
    if (evaluate_condition(bridge, goal)) {
        int written = snprintf(explanation + *exp_len, exp_size - *exp_len,
                               "%*sDIRECT VERIFY: %s\n", (int)depth * 2, "", goal);
        if (written > 0 && *exp_len + written < exp_size) {
            *exp_len += written;
        }
        return FIN_VERIFY_TRUE;
    }

    return FIN_VERIFY_UNKNOWN;
}

int financial_reasoning_bridge_verify_condition(
    financial_reasoning_bridge_t* bridge,
    const fin_verify_request_t* request,
    fin_verify_response_t* response)
{
    if (!bridge) {
        set_error("bridge is NULL");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_reasoning_bridge_verify_condition: bridge is NULL");
        return FIN_REASONING_ERR_NULL;
    }
    if (!request || !response) {
        set_error("request or response is NULL");
        return FIN_REASONING_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_REASONING_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic");
        return FIN_REASONING_ERR_STATE;
    }

    fin_reasoning_heartbeat_global("fin_reasoning_verify", 0.0f);

    /* BBB validation if enabled */
    if (bridge->config.enable_bbb_validation && bridge->bbb) {
        bridge->stats.bbb_validations++;
        int rc = bbb_validate_data((bbb_system_t)bridge->bbb, request,
                                    sizeof(*request), "reasoning_verify");
        if (rc != 0) {
            set_error("BBB validation failed for verify_condition");
            return FIN_REASONING_ERR_BBB;
        }
    }

    memset(response, 0, sizeof(fin_verify_response_t));

    uint32_t max_depth = request->max_depth;
    if (max_depth == 0 || max_depth > FIN_REASONING_MAX_DEPTH) {
        max_depth = bridge->config.max_depth;
    }

    uint32_t rules_checked = 0;
    size_t exp_len = 0;

    /* Run backward chaining */
    fin_verify_result_t result = backward_chain_recursive(
        bridge, request->goal, 0, max_depth, &rules_checked,
        response->explanation, FIN_REASONING_CHAIN_LEN, &exp_len);

    response->result = result;
    response->rules_checked = rules_checked;

    /* Compute confidence based on result and rules checked */
    if (result == FIN_VERIFY_TRUE) {
        response->confidence = 0.9f - (0.1f * rules_checked / 10.0f);
        response->confidence = nimcp_clampf(response->confidence, 0.5f, 0.95f);
    } else if (result == FIN_VERIFY_PARTIAL) {
        response->confidence = 0.5f;
    } else if (result == FIN_VERIFY_FALSE) {
        response->confidence = 0.1f;
    } else {
        response->confidence = 0.0f;
    }

    bridge->stats.backward_verifications++;

    /* KG messaging */
    bridge_kg_publish(bridge, KG_MSG_FIN_REASONING_VERIFY,
                      response, sizeof(*response));

    fin_reasoning_heartbeat_global("fin_reasoning_verify", 1.0f);
    return FIN_REASONING_ERR_OK;
}

/* ============================================================================
 * Hybrid Inference
 * ============================================================================ */

int financial_reasoning_bridge_hybrid_inference(
    financial_reasoning_bridge_t* bridge,
    const char** goals,
    uint32_t num_goals,
    fin_reasoning_result_t* result)
{
    if (!bridge) {
        set_error("bridge is NULL");
        return FIN_REASONING_ERR_NULL;
    }
    if (!result) {
        set_error("result is NULL");
        return FIN_REASONING_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_REASONING_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic");
        return FIN_REASONING_ERR_STATE;
    }

    fin_reasoning_heartbeat_global("fin_reasoning_hybrid", 0.0f);

    /* Phase 1: Forward chaining */
    int rc = financial_reasoning_bridge_derive_signals(bridge, result);
    if (rc != FIN_REASONING_ERR_OK) {
        return rc;
    }

    /* Phase 2: Backward chaining to verify goals */
    if (goals && num_goals > 0) {
        char verify_chain[FIN_REASONING_CHAIN_LEN / 2] = {0};
        int verify_len = 0;

        for (uint32_t i = 0; i < num_goals; i++) {
            fin_verify_request_t req;
            memset(&req, 0, sizeof(req));
            strncpy(req.goal, goals[i], FIN_REASONING_CONDITION_LEN - 1);
            req.max_depth = bridge->config.max_depth;
            req.explain = true;

            fin_verify_response_t resp;
            if (financial_reasoning_bridge_verify_condition(bridge, &req, &resp) == 0) {
                int written = snprintf(verify_chain + verify_len,
                                       sizeof(verify_chain) - verify_len,
                                       "[VERIFY %s: %s (conf=%.2f)]\n",
                                       goals[i],
                                       fin_reasoning_verify_name(resp.result),
                                       resp.confidence);
                if (written > 0 && verify_len + written < (int)sizeof(verify_chain)) {
                    verify_len += written;
                }
            }
        }

        /* Append verification to reasoning chain */
        size_t chain_len = strlen(result->reasoning_chain);
        if (chain_len + verify_len < FIN_REASONING_CHAIN_LEN) {
            strncat(result->reasoning_chain, "\n--- VERIFICATION ---\n",
                    FIN_REASONING_CHAIN_LEN - chain_len - 1);
            strncat(result->reasoning_chain, verify_chain,
                    FIN_REASONING_CHAIN_LEN - strlen(result->reasoning_chain) - 1);
        }
    }

    fin_reasoning_heartbeat_global("fin_reasoning_hybrid", 1.0f);
    return FIN_REASONING_ERR_OK;
}

/* ============================================================================
 * Result Accessors
 * ============================================================================ */

int financial_reasoning_bridge_get_recommendation(
    financial_reasoning_bridge_t* bridge,
    fin_signal_type_t* out_signal,
    float* out_confidence)
{
    if (!bridge) {
        return FIN_REASONING_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_REASONING_BRIDGE_MAGIC) {
        return FIN_REASONING_ERR_STATE;
    }

    if (out_signal) {
        *out_signal = bridge->last_signal;
    }
    if (out_confidence) {
        *out_confidence = bridge->last_confidence;
    }

    return FIN_REASONING_ERR_OK;
}

/* ============================================================================
 * Result Management
 * ============================================================================ */

void financial_reasoning_result_init(fin_reasoning_result_t* result) {
    if (result) {
        memset(result, 0, sizeof(fin_reasoning_result_t));
    }
}

void financial_reasoning_result_free(fin_reasoning_result_t* result) {
    if (result) {
        if (result->triggered_rules) {
            nimcp_free(result->triggered_rules);
            result->triggered_rules = NULL;
        }
        if (result->derived_signals) {
            nimcp_free(result->derived_signals);
            result->derived_signals = NULL;
        }
        result->num_triggered = 0;
        result->num_signals = 0;
    }
}

/* ============================================================================
 * Query API
 * ============================================================================ */

fin_reasoning_op_state_t financial_reasoning_bridge_get_op_state(
    const financial_reasoning_bridge_t* bridge)
{
    if (!bridge || bridge->magic != FINANCIAL_REASONING_BRIDGE_MAGIC) {
        return FIN_REASONING_OP_STATE_UNINITIALIZED;
    }
    return bridge->op_state;
}

int financial_reasoning_bridge_get_stats(
    const financial_reasoning_bridge_t* bridge,
    fin_reasoning_bridge_stats_t* stats)
{
    if (!bridge) {
        set_error("bridge is NULL");
        return FIN_REASONING_ERR_NULL;
    }
    if (!stats) {
        set_error("stats is NULL");
        return FIN_REASONING_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_REASONING_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic");
        return FIN_REASONING_ERR_STATE;
    }

    *stats = bridge->stats;
    return FIN_REASONING_ERR_OK;
}

void financial_reasoning_bridge_reset_stats(financial_reasoning_bridge_t* bridge) {
    if (bridge && bridge->magic == FINANCIAL_REASONING_BRIDGE_MAGIC) {
        memset(&bridge->stats, 0, sizeof(fin_reasoning_bridge_stats_t));
    }
}

/* ============================================================================
 * Health Integration
 * ============================================================================ */

int financial_reasoning_bridge_heartbeat(
    financial_reasoning_bridge_t* bridge,
    const char* operation,
    float progress)
{
    if (!bridge) {
        return FIN_REASONING_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_REASONING_BRIDGE_MAGIC) {
        return FIN_REASONING_ERR_STATE;
    }

    /* Forward to global health agent */
    fin_reasoning_heartbeat_global(
        operation ? operation : "fin_reasoning_heartbeat", progress);

    /* Forward to instance-level health agent */
    if (bridge->health_agent) {
        nimcp_health_agent_heartbeat_ex(
            (nimcp_health_agent_t*)bridge->health_agent, operation, progress);
    }

    bridge->stats.health_heartbeats++;
    return FIN_REASONING_ERR_OK;
}
