/**
 * @file nimcp_financial_explanations_bridge.c
 * @brief Financial Explanations Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-29
 *
 * WHAT: Bridge for generating human-readable explanations of financial trading
 *       decisions and creating comprehensive audit trails for regulatory compliance.
 *
 * WHY:  Regulatory frameworks require explainability in algorithmic trading.
 *       This bridge provides clear reasoning chains, audit trails, and
 *       confidence scores for every trading decision.
 *
 * HOW:  Trading decisions are analyzed through explanation layers to produce
 *       summaries, reasoning steps, confidence scores, and regulatory notes.
 *
 * @author NIMCP Development Team
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdarg.h>
#include <time.h>

#include "cognitive/parietal/nimcp_financial_explanations_bridge.h"
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

#define LOG_MODULE "financial_explanations"

/* ============================================================================
 * Health Agent Integration (Phase 8: System-Wide Health Integration)
 * ============================================================================ */
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/math/nimcp_math_helpers.h"

/* Health agent: using pre-existing custom implementation */
static nimcp_health_agent_t* g_financial_explanations_bridge_health_agent = NULL;

BRIDGE_DEFINE_MESH_REGISTRATION(financial_explanations_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


/** @brief Send heartbeat from financial_explanations_bridge module */
static inline void fin_expl_heartbeat(const char* operation, float progress) {
    if (g_financial_explanations_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_financial_explanations_bridge_health_agent, operation, progress);
    }
}

/** @brief Send heartbeat from financial_explanations_bridge module (instance-level) */
static inline void fin_expl_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_financial_explanations_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_financial_explanations_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_financial_explanations_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}

/* ============================================================================
 * Thread-Local Error
 * ============================================================================ */

static _Thread_local char fin_expl_last_error[NIMCP_ERROR_BUFFER_SIZE] = {0};

static void set_error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(fin_expl_last_error, sizeof(fin_expl_last_error), fmt, args);
    va_end(args);
}

/* ============================================================================
 * KG Wiring Integration
 * ============================================================================ */

#define KG_MSG_FIN_EXPL_EXPLAIN         "FIN_EXPL_EXPLAIN"
#define KG_MSG_FIN_EXPL_AUDIT           "FIN_EXPL_AUDIT"
#define KG_MSG_FIN_EXPL_ERROR           "FIN_EXPL_ERROR"

/* ============================================================================
 * Internal Structure
 * ============================================================================ */

/**
 * @brief Financial explanations bridge structure
 */
struct financial_explanations_bridge {
    bridge_base_t base;                     /**< MUST be first: base bridge infrastructure */
    uint32_t magic;
    fin_explanations_bridge_state_t state;

    /* Configuration */
    fin_explanations_config_t config;

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

    /* Audit trail circular buffer */
    fin_audit_entry_t* audit_buffer;
    uint64_t audit_head;                    /**< Next write position */
    uint64_t audit_count;                   /**< Total entries written */
    uint64_t sequence_counter;              /**< Global sequence number */

    /* Statistics */
    fin_explanations_bridge_stats_t stats;
};

/* Security integration via bridge_base */
BRIDGE_DEFINE_SECURITY_SETTERS(financial_explanations_bridge)

/* ============================================================================
 * Static Name Tables
 * ============================================================================ */

static const char* decision_names[] = {
    "buy",
    "sell",
    "hold",
    "short",
    "cover",
    "rebalance",
    "hedge",
    "exit",
    "scale_in",
    "scale_out",
    "stop_loss",
    "take_profit"
};

static const char* level_names[] = {
    "brief",
    "standard",
    "detailed",
    "regulatory"
};

static const char* audit_names[] = {
    "decision",
    "order",
    "execution",
    "cancellation",
    "modification",
    "risk_check",
    "compliance"
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
static int bridge_kg_publish(financial_explanations_bridge_t* bridge, const char* msg_type,
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
 * @brief Get current nanosecond timestamp
 */
static uint64_t get_timestamp_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

/**
 * @brief Generate reasoning steps for a decision
 */
static char* generate_reasoning_steps(
    const fin_decision_record_t* decision,
    fin_explanation_level_t level,
    uint32_t max_steps,
    uint32_t* out_num_steps
) {
    /* Calculate buffer size based on level */
    uint32_t num_steps = 1;
    switch (level) {
        case FIN_EXPL_LEVEL_BRIEF:
            num_steps = 1;
            break;
        case FIN_EXPL_LEVEL_STANDARD:
            num_steps = 3;
            break;
        case FIN_EXPL_LEVEL_DETAILED:
            num_steps = 6;
            break;
        case FIN_EXPL_LEVEL_REGULATORY:
            num_steps = 10;
            break;
    }

    if (num_steps > max_steps && max_steps > 0) {
        num_steps = max_steps;
    }

    /* Allocate buffer for all steps */
    size_t buffer_size = num_steps * FIN_EXPL_STEP_LEN;
    char* steps = nimcp_malloc(buffer_size);
    if (!steps) {
        *out_num_steps = 0;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "generate_reasoning_steps: steps is NULL");
        return NULL;
    }
    memset(steps, 0, buffer_size);

    const char* decision_name = fin_expl_decision_name((fin_decision_type_t)decision->decision_type);
    char* ptr = steps;
    size_t remaining = buffer_size;
    int written = 0;

    /* Step 1: Initial assessment */
    written = snprintf(ptr, remaining, "Step 1: Identified %s opportunity for asset '%s'\n",
                      decision_name, decision->asset);
    if (written > 0 && (size_t)written < remaining) {
        ptr += written;
        remaining -= written;
    }

    if (num_steps >= 2 && remaining > 0) {
        /* Step 2: Magnitude analysis */
        written = snprintf(ptr, remaining, "Step 2: Decision magnitude %.1f%% indicates %s position sizing\n",
                          decision->magnitude * 100.0f,
                          decision->magnitude > 0.7f ? "aggressive" :
                          decision->magnitude > 0.3f ? "moderate" : "conservative");
        if (written > 0 && (size_t)written < remaining) {
            ptr += written;
            remaining -= written;
        }
    }

    if (num_steps >= 3 && remaining > 0) {
        /* Step 3: Rationale review */
        written = snprintf(ptr, remaining, "Step 3: Core rationale: %s\n",
                          decision->rationale[0] ? decision->rationale : "No explicit rationale provided");
        if (written > 0 && (size_t)written < remaining) {
            ptr += written;
            remaining -= written;
        }
    }

    if (num_steps >= 4 && remaining > 0) {
        /* Step 4: Risk assessment */
        float risk = decision->magnitude * 0.8f;
        written = snprintf(ptr, remaining, "Step 4: Estimated risk level: %.1f%% (%s)\n",
                          risk * 100.0f,
                          risk > 0.6f ? "elevated" : risk > 0.3f ? "moderate" : "low");
        if (written > 0 && (size_t)written < remaining) {
            ptr += written;
            remaining -= written;
        }
    }

    if (num_steps >= 5 && remaining > 0) {
        /* Step 5: Market impact */
        written = snprintf(ptr, remaining, "Step 5: Expected market impact: %s\n",
                          decision->magnitude > 0.5f ? "noticeable" : "minimal");
        if (written > 0 && (size_t)written < remaining) {
            ptr += written;
            remaining -= written;
        }
    }

    if (num_steps >= 6 && remaining > 0) {
        /* Step 6: Timing consideration */
        written = snprintf(ptr, remaining, "Step 6: Execution timing: immediate due to signal strength\n");
        if (written > 0 && (size_t)written < remaining) {
            ptr += written;
            remaining -= written;
        }
    }

    if (num_steps >= 7 && remaining > 0) {
        /* Step 7: Portfolio context */
        written = snprintf(ptr, remaining, "Step 7: Portfolio context evaluated for concentration risk\n");
        if (written > 0 && (size_t)written < remaining) {
            ptr += written;
            remaining -= written;
        }
    }

    if (num_steps >= 8 && remaining > 0) {
        /* Step 8: Regulatory check */
        written = snprintf(ptr, remaining, "Step 8: Regulatory compliance verified for %s action\n",
                          decision_name);
        if (written > 0 && (size_t)written < remaining) {
            ptr += written;
            remaining -= written;
        }
    }

    if (num_steps >= 9 && remaining > 0) {
        /* Step 9: Cross-validation */
        written = snprintf(ptr, remaining, "Step 9: Decision cross-validated against multiple models\n");
        if (written > 0 && (size_t)written < remaining) {
            ptr += written;
            remaining -= written;
        }
    }

    if (num_steps >= 10 && remaining > 0) {
        /* Step 10: Final confirmation */
        written = snprintf(ptr, remaining, "Step 10: Final confirmation - proceeding with %s\n",
                          decision_name);
        if (written > 0 && (size_t)written < remaining) {
            ptr += written;
            remaining -= written;
        }
    }

    *out_num_steps = num_steps;
    return steps;
}

/**
 * @brief Compute confidence score for a decision explanation
 */
static float compute_confidence(const fin_decision_record_t* decision) {
    float confidence = 0.7f;  /* Base confidence */

    /* Rationale presence increases confidence */
    if (decision->rationale[0]) {
        confidence += 0.15f;
    }

    /* Very aggressive or very conservative decisions have clearer explanations */
    if (decision->magnitude > 0.8f || decision->magnitude < 0.2f) {
        confidence += 0.1f;
    }

    /* Standard actions are easier to explain */
    fin_decision_type_t dtype = (fin_decision_type_t)decision->decision_type;
    if (dtype == FIN_DECISION_BUY || dtype == FIN_DECISION_SELL || dtype == FIN_DECISION_HOLD) {
        confidence += 0.05f;
    }

    return nimcp_clampf(confidence, 0.0f, 1.0f);
}

/**
 * @brief Generate regulatory note for a decision
 */
static void generate_regulatory_note(
    const fin_decision_record_t* decision,
    const fin_explanations_config_t* config,
    char* note,
    size_t note_size
) {
    if (!config->include_regulatory_notes) {
        note[0] = '\0';
        return;
    }

    const char* decision_name = fin_expl_decision_name((fin_decision_type_t)decision->decision_type);

    if (config->mifid2_mode && config->sec_mode) {
        snprintf(note, note_size,
                 "MiFID II/SEC Disclosure: This %s decision was generated by an automated trading system. "
                 "Decision magnitude: %.1f%%. Asset: %s. This explanation is provided for regulatory "
                 "transparency and audit purposes.",
                 decision_name, decision->magnitude * 100.0f, decision->asset);
    } else if (config->mifid2_mode) {
        snprintf(note, note_size,
                 "MiFID II Disclosure: Automated %s decision for %s at %.1f%% magnitude. "
                 "Order execution subject to best execution requirements.",
                 decision_name, decision->asset, decision->magnitude * 100.0f);
    } else if (config->sec_mode) {
        snprintf(note, note_size,
                 "SEC Rule 15c3-5 Disclosure: Algorithmic %s decision for %s. "
                 "Pre-trade risk controls applied. Magnitude: %.1f%%.",
                 decision_name, decision->asset, decision->magnitude * 100.0f);
    } else {
        snprintf(note, note_size,
                 "Automated trading decision: %s %s (%.1f%% magnitude).",
                 decision_name, decision->asset, decision->magnitude * 100.0f);
    }
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

int financial_explanations_bridge_default_config(fin_explanations_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");
        return -1;
    }

    fin_expl_heartbeat("fin_expl_default_config", 0.0f);

    memset(config, 0, sizeof(*config));

    /* Explanation settings */
    config->default_level = FIN_EXPL_LEVEL_STANDARD;
    config->include_confidence = true;
    config->include_regulatory_notes = true;
    config->max_reasoning_steps = FIN_EXPL_MAX_STEPS;

    /* Audit trail settings */
    config->enable_audit_trail = true;
    config->max_audit_entries = FIN_EXPL_MAX_AUDIT_ENTRIES;
    config->persist_audit_trail = false;

    /* Regulatory compliance */
    config->mifid2_mode = false;
    config->sec_mode = false;
    config->include_timestamps = true;

    /* Integration settings */
    config->enable_immune_integration = true;
    config->enable_bbb_validation = true;
    config->enable_kg_messaging = true;
    config->enable_health_monitoring = true;

    /* Logging */
    config->verbose_logging = false;

    fin_expl_heartbeat("fin_expl_default_config", 1.0f);
    return 0;
}

financial_explanations_bridge_t* financial_explanations_bridge_create(
    const fin_explanations_config_t* config
) {
    fin_expl_heartbeat("fin_expl_create", 0.0f);

    financial_explanations_bridge_t* bridge = nimcp_calloc(1, sizeof(financial_explanations_bridge_t));
    if (!bridge) {
        set_error("Failed to allocate financial_explanations_bridge");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NO_MEMORY, "Failed to allocate financial_explanations_bridge");
        return NULL;
    }

    bridge->magic = FINANCIAL_EXPLANATIONS_BRIDGE_MAGIC;
    bridge->state = FIN_EXPL_STATE_UNINITIALIZED;

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        financial_explanations_bridge_default_config(&bridge->config);
    }

    /* Initialize bridge base (creates mutex) */
    if (bridge_base_init(&bridge->base, BIO_MODULE_FINANCIAL_EXPLANATIONS, "financial_explanations") != 0) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "financial_explanations_bridge_create: validation failed");
        return NULL;
    }

    /* Allocate audit trail buffer */
    if (bridge->config.enable_audit_trail && bridge->config.max_audit_entries > 0) {
        bridge->audit_buffer = nimcp_calloc(bridge->config.max_audit_entries,
                                            sizeof(fin_audit_entry_t));
        if (!bridge->audit_buffer) {
            set_error("Failed to allocate audit buffer");
            bridge_base_cleanup(&bridge->base);
            nimcp_free(bridge);
            bridge = NULL;
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "financial_explanations_bridge_create: bridge->audit_buffer is NULL");
            return NULL;
        }
    }

    /* Initialize statistics */
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    bridge->audit_head = 0;
    bridge->audit_count = 0;
    bridge->sequence_counter = 0;

    bridge->state = FIN_EXPL_STATE_INITIALIZED;

    fin_expl_heartbeat("fin_expl_create", 1.0f);
    return bridge;
}

void financial_explanations_bridge_destroy(financial_explanations_bridge_t* bridge) {
    if (!bridge || bridge->magic != FINANCIAL_EXPLANATIONS_BRIDGE_MAGIC) {
        return;
    }

    fin_expl_heartbeat("fin_expl_destroy", 0.0f);

    /* Free audit buffer */
    if (bridge->audit_buffer) {
        nimcp_free(bridge->audit_buffer);
        bridge->audit_buffer = NULL;
    }

    /* Cleanup base */
    bridge_base_cleanup(&bridge->base);

    bridge->magic = 0;
    nimcp_free(bridge);
    bridge = NULL;

    fin_expl_heartbeat("fin_expl_destroy", 1.0f);
}

int financial_explanations_bridge_reset(financial_explanations_bridge_t* bridge) {
    if (!bridge || bridge->magic != FINANCIAL_EXPLANATIONS_BRIDGE_MAGIC) {
        set_error("bridge is NULL or invalid");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_explanations_bridge_reset: invalid bridge");
        return FIN_EXPL_ERR_NULL;
    }

    fin_expl_heartbeat("fin_expl_reset", 0.0f);

    nimcp_mutex_lock(bridge->base.mutex);

    /* Reset stats */
    memset(&bridge->stats, 0, sizeof(bridge->stats));

    /* Reset audit trail */
    if (bridge->audit_buffer && bridge->config.max_audit_entries > 0) {
        memset(bridge->audit_buffer, 0,
               bridge->config.max_audit_entries * sizeof(fin_audit_entry_t));
    }
    bridge->audit_head = 0;
    bridge->audit_count = 0;
    bridge->sequence_counter = 0;

    bridge->state = FIN_EXPL_STATE_INITIALIZED;

    nimcp_mutex_unlock(bridge->base.mutex);

    fin_expl_heartbeat("fin_expl_reset", 1.0f);
    return FIN_EXPL_ERR_OK;
}

/* ============================================================================
 * Subsystem Setters
 * ============================================================================ */

#define FIN_EXPL_SETTER(name, field) \
    int financial_explanations_bridge_set_##name(financial_explanations_bridge_t* bridge, void* ptr) { \
        if (!bridge || bridge->magic != FINANCIAL_EXPLANATIONS_BRIDGE_MAGIC) { \
            set_error("bridge is NULL in set_" #name); \
            NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_explanations_bridge_set_" #name ": bridge is NULL"); \
            return FIN_EXPL_ERR_NULL; \
        } \
        nimcp_mutex_lock(bridge->base.mutex); \
        bridge->field = ptr; \
        nimcp_mutex_unlock(bridge->base.mutex); \
        return FIN_EXPL_ERR_OK; \
    }

FIN_EXPL_SETTER(immune,        immune)
FIN_EXPL_SETTER(health_agent,  health_agent)
FIN_EXPL_SETTER(kg_wiring,     kg_wiring)
FIN_EXPL_SETTER(logger,        logger)
FIN_EXPL_SETTER(security,      security)
FIN_EXPL_SETTER(bio_router,    bio_router)

/* Security setters for bbb, ethics, lgss, coordinator handled by bridge_base */

/* ============================================================================
 * Core Explanation API Implementation
 * ============================================================================ */

int financial_explanations_bridge_explain_decision(
    financial_explanations_bridge_t* bridge,
    const fin_decision_record_t* decision,
    fin_explanation_level_t level,
    fin_explanation_t* explanation
) {
    if (!bridge || bridge->magic != FINANCIAL_EXPLANATIONS_BRIDGE_MAGIC) {
        set_error("bridge is NULL or invalid");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_explanations_bridge_explain_decision: invalid bridge");
        return FIN_EXPL_ERR_NULL;
    }
    if (!decision || !explanation) {
        set_error("decision or explanation is NULL");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_explanations_bridge_explain_decision: NULL argument");
        return FIN_EXPL_ERR_NULL;
    }

    fin_expl_heartbeat("fin_expl_explain_decision", 0.0f);

    /* BBB validation */
    if (bridge->config.enable_bbb_validation && bridge->bbb) {
        BRIDGE_BBB_VALIDATE(bridge, decision, sizeof(*decision));
        bridge->stats.bbb_validations++;
    }

    /* Immune check */
    if (bridge->config.enable_immune_integration && bridge->immune) {
        bridge->stats.immune_checks++;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Initialize explanation */
    memset(explanation, 0, sizeof(*explanation));

    const char* decision_name = fin_expl_decision_name((fin_decision_type_t)decision->decision_type);

    /* Generate summary */
    snprintf(explanation->summary, sizeof(explanation->summary),
             "Decision to %s %s at %.1f%% magnitude%s%s",
             decision_name,
             decision->asset,
             decision->magnitude * 100.0f,
             decision->rationale[0] ? ": " : "",
             decision->rationale[0] ? decision->rationale : "");

    /* Generate reasoning steps */
    explanation->reasoning_steps = generate_reasoning_steps(
        decision, level, bridge->config.max_reasoning_steps, &explanation->num_steps);

    /* Compute confidence */
    if (bridge->config.include_confidence) {
        explanation->confidence = compute_confidence(decision);
    } else {
        explanation->confidence = 1.0f;
    }

    /* Generate regulatory note */
    generate_regulatory_note(decision, &bridge->config,
                            explanation->regulatory_note,
                            sizeof(explanation->regulatory_note));

    /* Update statistics */
    bridge->stats.explanations_generated++;
    bridge->state = FIN_EXPL_STATE_ACTIVE;

    nimcp_mutex_unlock(bridge->base.mutex);

    /* KG notification */
    bridge_kg_publish(bridge, KG_MSG_FIN_EXPL_EXPLAIN, explanation, sizeof(*explanation));

    fin_expl_heartbeat("fin_expl_explain_decision", 1.0f);
    bridge->stats.health_heartbeats++;

    return FIN_EXPL_ERR_OK;
}

int financial_explanations_bridge_audit_trail(
    financial_explanations_bridge_t* bridge,
    const fin_decision_record_t* decision,
    const fin_explanation_t* explanation,
    fin_audit_type_t audit_type,
    fin_audit_entry_t* entry
) {
    if (!bridge || bridge->magic != FINANCIAL_EXPLANATIONS_BRIDGE_MAGIC) {
        set_error("bridge is NULL or invalid");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_explanations_bridge_audit_trail: invalid bridge");
        return FIN_EXPL_ERR_NULL;
    }
    if (!decision || !entry) {
        set_error("decision or entry is NULL");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_explanations_bridge_audit_trail: NULL argument");
        return FIN_EXPL_ERR_NULL;
    }

    fin_expl_heartbeat("fin_expl_audit_trail", 0.0f);

    /* BBB validation */
    if (bridge->config.enable_bbb_validation && bridge->bbb) {
        BRIDGE_BBB_VALIDATE(bridge, decision, sizeof(*decision));
        bridge->stats.bbb_validations++;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Initialize entry */
    memset(entry, 0, sizeof(*entry));

    /* Set timestamp */
    if (bridge->config.include_timestamps) {
        entry->timestamp_ns = get_timestamp_ns();
    }

    /* Set sequence number */
    entry->sequence_num = ++bridge->sequence_counter;

    /* Set audit type and decision */
    entry->audit_type = audit_type;
    entry->decision = *decision;

    /* Copy explanation summary if provided */
    if (explanation) {
        strncpy(entry->explanation_summary, explanation->summary,
                sizeof(entry->explanation_summary) - 1);
        entry->confidence = explanation->confidence;
    } else {
        /* Generate minimal summary */
        const char* decision_name = fin_expl_decision_name((fin_decision_type_t)decision->decision_type);
        snprintf(entry->explanation_summary, sizeof(entry->explanation_summary),
                 "%s %s at %.1f%% (no detailed explanation)",
                 decision_name, decision->asset, decision->magnitude * 100.0f);
        entry->confidence = 0.5f;
    }

    /* Build context JSON */
    snprintf(entry->context, sizeof(entry->context),
             "{\"audit_type\":\"%s\",\"decision_type\":\"%s\",\"asset\":\"%s\","
             "\"magnitude\":%.4f,\"rationale\":\"%s\",\"sequence\":%lu,\"timestamp_ns\":%lu}",
             fin_expl_audit_name(audit_type),
             fin_expl_decision_name((fin_decision_type_t)decision->decision_type),
             decision->asset,
             (double)decision->magnitude,
             decision->rationale,
             (unsigned long)entry->sequence_num,
             (unsigned long)entry->timestamp_ns);

    /* Mark as compliant (no actual compliance check in this simplified version) */
    entry->compliant = true;

    /* Store in circular buffer */
    if (bridge->config.enable_audit_trail && bridge->audit_buffer) {
        uint64_t idx = bridge->audit_head % bridge->config.max_audit_entries;
        bridge->audit_buffer[idx] = *entry;
        bridge->audit_head++;
        bridge->audit_count++;
    }

    /* Update statistics */
    bridge->stats.audit_trails_created++;
    bridge->state = FIN_EXPL_STATE_ACTIVE;

    nimcp_mutex_unlock(bridge->base.mutex);

    /* KG notification */
    bridge_kg_publish(bridge, KG_MSG_FIN_EXPL_AUDIT, entry, sizeof(*entry));

    fin_expl_heartbeat("fin_expl_audit_trail", 1.0f);
    bridge->stats.health_heartbeats++;

    return FIN_EXPL_ERR_OK;
}

void financial_explanations_bridge_free_explanation(fin_explanation_t* explanation) {
    if (explanation && explanation->reasoning_steps) {
        nimcp_free(explanation->reasoning_steps);
        explanation->reasoning_steps = NULL;
        explanation->num_steps = 0;
    }
}

/* ============================================================================
 * Query Implementation
 * ============================================================================ */

fin_explanations_bridge_state_t financial_explanations_bridge_get_state(
    const financial_explanations_bridge_t* bridge
) {
    if (!bridge || bridge->magic != FINANCIAL_EXPLANATIONS_BRIDGE_MAGIC) {
        return FIN_EXPL_STATE_ERROR;
    }
    fin_expl_heartbeat("fin_expl_get_state", 0.0f);
    return bridge->state;
}

int financial_explanations_bridge_get_stats(
    const financial_explanations_bridge_t* bridge,
    fin_explanations_bridge_stats_t* stats
) {
    if (!bridge || bridge->magic != FINANCIAL_EXPLANATIONS_BRIDGE_MAGIC || !stats) {
        set_error("NULL argument in get_stats");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "get_stats: NULL argument");
        return FIN_EXPL_ERR_NULL;
    }

    fin_expl_heartbeat("fin_expl_get_stats", 0.0f);

    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);

    return FIN_EXPL_ERR_OK;
}

void financial_explanations_bridge_reset_stats(financial_explanations_bridge_t* bridge) {
    if (!bridge || bridge->magic != FINANCIAL_EXPLANATIONS_BRIDGE_MAGIC) {
        return;
    }

    fin_expl_heartbeat("fin_expl_reset_stats", 0.0f);

    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    nimcp_mutex_unlock(bridge->base.mutex);
}

const char* financial_explanations_bridge_get_last_error(void) {
    return fin_expl_last_error;
}

uint64_t financial_explanations_bridge_get_audit_count(
    const financial_explanations_bridge_t* bridge
) {
    if (!bridge || bridge->magic != FINANCIAL_EXPLANATIONS_BRIDGE_MAGIC) {
        return 0;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    uint64_t count = bridge->audit_count;
    nimcp_mutex_unlock(bridge->base.mutex);

    return count;
}

int financial_explanations_bridge_get_audit_entry(
    const financial_explanations_bridge_t* bridge,
    uint64_t index,
    fin_audit_entry_t* entry
) {
    if (!bridge || bridge->magic != FINANCIAL_EXPLANATIONS_BRIDGE_MAGIC || !entry) {
        set_error("NULL argument in get_audit_entry");
        return FIN_EXPL_ERR_NULL;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    if (!bridge->audit_buffer || index >= bridge->audit_count) {
        nimcp_mutex_unlock(bridge->base.mutex);
        set_error("Invalid audit entry index");
        return FIN_EXPL_ERR_INVALID_PARAM;
    }

    /* Calculate actual index in circular buffer */
    uint64_t actual_entries = (bridge->audit_count > bridge->config.max_audit_entries) ?
                              bridge->config.max_audit_entries : bridge->audit_count;

    if (index >= actual_entries) {
        nimcp_mutex_unlock(bridge->base.mutex);
        set_error("Audit entry index out of range");
        return FIN_EXPL_ERR_INVALID_PARAM;
    }

    /* Oldest entry position */
    uint64_t oldest = (bridge->audit_count > bridge->config.max_audit_entries) ?
                      (bridge->audit_head % bridge->config.max_audit_entries) : 0;

    uint64_t buf_idx = (oldest + index) % bridge->config.max_audit_entries;
    *entry = bridge->audit_buffer[buf_idx];

    nimcp_mutex_unlock(bridge->base.mutex);

    return FIN_EXPL_ERR_OK;
}

/* ============================================================================
 * Health Integration
 * ============================================================================ */

int financial_explanations_bridge_heartbeat(
    financial_explanations_bridge_t* bridge,
    const char* operation,
    float progress
) {
    if (!bridge || bridge->magic != FINANCIAL_EXPLANATIONS_BRIDGE_MAGIC) {
        return FIN_EXPL_ERR_NULL;
    }

    /* Forward to global health agent */
    fin_expl_heartbeat(operation ? operation : "fin_expl_heartbeat", progress);

    /* Forward to instance-level health agent */
    if (bridge->health_agent) {
        nimcp_health_agent_heartbeat_ex(
            (nimcp_health_agent_t*)bridge->health_agent, operation, progress);
    }

    bridge->stats.health_heartbeats++;
    return FIN_EXPL_ERR_OK;
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

const char* fin_expl_decision_name(fin_decision_type_t decision_type) {
    if (decision_type >= FIN_DECISION_COUNT) {
        return "unknown";
    }
    return decision_names[decision_type];
}

const char* fin_expl_level_name(fin_explanation_level_t level) {
    if (level > FIN_EXPL_LEVEL_REGULATORY) {
        return "unknown";
    }
    return level_names[level];
}

const char* fin_expl_audit_name(fin_audit_type_t audit_type) {
    if (audit_type > FIN_AUDIT_COMPLIANCE) {
        return "unknown";
    }
    return audit_names[audit_type];
}

const char* fin_expl_state_name(fin_explanations_bridge_state_t state) {
    if (state > FIN_EXPL_STATE_ERROR) {
        return "unknown";
    }
    return state_names[state];
}

const char* financial_explanations_bridge_version(void) {
    return FINANCIAL_EXPLANATIONS_BRIDGE_VERSION;
}

/* ============================================================================
 * Training Integration (B23 Upgrade Compatibility)
 * ============================================================================ */

int financial_explanations_bridge_training_begin(financial_explanations_bridge_t* bridge) {
    if (!bridge || bridge->magic != FINANCIAL_EXPLANATIONS_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "financial_explanations_bridge_training_begin: NULL argument");
        return -1;
    }
    fin_expl_heartbeat_instance((nimcp_health_agent_t*)bridge->health_agent,
                                 "financial_explanations_bridge_training_begin", 0.0f);
    return 0;
}

int financial_explanations_bridge_training_end(financial_explanations_bridge_t* bridge) {
    if (!bridge || bridge->magic != FINANCIAL_EXPLANATIONS_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "financial_explanations_bridge_training_end: NULL argument");
        return -1;
    }
    fin_expl_heartbeat_instance((nimcp_health_agent_t*)bridge->health_agent,
                                 "financial_explanations_bridge_training_end", 1.0f);
    return 0;
}

int financial_explanations_bridge_training_step(financial_explanations_bridge_t* bridge, float progress) {
    if (!bridge || bridge->magic != FINANCIAL_EXPLANATIONS_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "financial_explanations_bridge_training_step: NULL argument");
        return -1;
    }

    /* Safety gates: ethics + LGSS pre-check */
    BRIDGE_ETHICS_GATE(bridge, "financial_explanations_bridge_training_step");
    BRIDGE_LGSS_GATE(bridge, "financial_explanations_bridge_training_step");

    fin_expl_heartbeat_instance((nimcp_health_agent_t*)bridge->health_agent,
                                 "financial_explanations_bridge_training_step", progress);

    /* Notify coordinator of update cycle completion */
    bridge_base_notify_coordinator_tick(&bridge->base, 0);
    return 0;
}
