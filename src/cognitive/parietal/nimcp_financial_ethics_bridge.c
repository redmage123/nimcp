/**
 * @file nimcp_financial_ethics_bridge.c
 * @brief Financial Ethics Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-29
 *
 * WHAT: Bridge for evaluating the ethical implications of financial trading
 *       actions using multiple ethical frameworks.
 *
 * WHY:  Autonomous trading systems must consider ethical implications beyond
 *       pure profit maximization. This bridge enables harm assessment,
 *       Asimov compliance checks, Golden Rule tests, and empathy modeling.
 *
 * HOW:  Trading actions are evaluated against multiple ethical frameworks
 *       with results aggregated into a verdict (APPROVED/DENIED/ESCALATE/WARN).
 *
 * @author NIMCP Development Team
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdarg.h>

#include "cognitive/parietal/nimcp_financial_ethics_bridge.h"
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

#define LOG_MODULE "financial_ethics"

/* ============================================================================
 * Health Agent Integration (Phase 8: System-Wide Health Integration)
 * ============================================================================ */
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/math/nimcp_math_helpers.h"

/* Health agent: using pre-existing custom implementation */
static nimcp_health_agent_t* g_financial_ethics_bridge_health_agent = NULL;

BRIDGE_DEFINE_MESH_REGISTRATION(financial_ethics_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


/** @brief Send heartbeat from financial_ethics_bridge module */
static inline void fin_ethics_heartbeat(const char* operation, float progress) {
    if (g_financial_ethics_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_financial_ethics_bridge_health_agent, operation, progress);
    }
}

/** @brief Send heartbeat from financial_ethics_bridge module (instance-level) */
static inline void fin_ethics_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_financial_ethics_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_financial_ethics_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_financial_ethics_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}

/* ============================================================================
 * Thread-Local Error
 * ============================================================================ */

static _Thread_local char fin_ethics_last_error[NIMCP_ERROR_BUFFER_SIZE] = {0};

static void set_error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(fin_ethics_last_error, sizeof(fin_ethics_last_error), fmt, args);
    va_end(args);
}

/* ============================================================================
 * KG Wiring Integration
 * ============================================================================ */

#define KG_MSG_FIN_ETHICS_EVALUATE      "FIN_ETHICS_EVALUATE"
#define KG_MSG_FIN_ETHICS_HARM          "FIN_ETHICS_HARM"
#define KG_MSG_FIN_ETHICS_GOLDEN_RULE   "FIN_ETHICS_GOLDEN_RULE"
#define KG_MSG_FIN_ETHICS_ASIMOV        "FIN_ETHICS_ASIMOV"
#define KG_MSG_FIN_ETHICS_VIOLATION     "FIN_ETHICS_VIOLATION"
#define KG_MSG_FIN_ETHICS_ERROR         "FIN_ETHICS_ERROR"

/* ============================================================================
 * Internal Structure
 * ============================================================================ */

/**
 * @brief Financial ethics bridge structure
 */
struct financial_ethics_bridge {
    bridge_base_t base;                     /**< MUST be first: base bridge infrastructure */
    uint32_t magic;
    fin_ethics_bridge_state_t state;

    /* Configuration */
    fin_ethics_config_t config;

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
    fin_ethics_bridge_stats_t stats;
};

/* Security integration via bridge_base */
BRIDGE_DEFINE_SECURITY_SETTERS(financial_ethics_bridge)

/* ============================================================================
 * Static Name Tables
 * ============================================================================ */

static const char* verdict_names[] = {
    "approved",
    "denied",
    "escalate",
    "warn"
};

static const char* action_names[] = {
    "buy",
    "sell",
    "short",
    "cover",
    "market_make",
    "arbitrage",
    "liquidate",
    "stop_hunt",
    "momentum_ignite",
    "spoofing",
    "layering",
    "wash_trade"
};

static const char* asimov_names[] = {
    "none",
    "first_law",
    "second_law",
    "third_law"
};

static const char* party_names[] = {
    "retail",
    "institutional",
    "market_maker",
    "issuer",
    "regulator",
    "market_integrity",
    "principal"
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
static int bridge_kg_publish(financial_ethics_bridge_t* bridge, const char* msg_type,
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
 * @brief Check if action type is inherently manipulative
 */
static bool is_manipulative_action(fin_action_type_t action_type) {
    switch (action_type) {
        case FIN_ACTION_STOP_HUNT:
        case FIN_ACTION_MOMENTUM_IGNITE:
        case FIN_ACTION_SPOOFING:
        case FIN_ACTION_LAYERING:
        case FIN_ACTION_WASH_TRADE:
            return true;
        default:
            return false;
    }
}

/**
 * @brief Compute harm score based on action parameters
 */
static float compute_base_harm(
    const fin_ethics_action_t* action,
    const fin_ethics_config_t* config
) {
    float harm = 0.0f;
    fin_action_type_t atype = (fin_action_type_t)action->action_type;

    /* Manipulative actions have high base harm */
    if (is_manipulative_action(atype)) {
        harm = 0.7f;
    }

    /* Scale by magnitude and position size */
    harm += action->action_magnitude * 0.15f;
    harm += minf(action->position_size / 10000000.0f, 0.15f);  /* Cap at 10M impact */

    /* Liquidations can be harmful to retail */
    if (atype == FIN_ACTION_LIQUIDATE) {
        harm += 0.2f;
    }

    (void)config;  /* Config may be used for future harm tuning */

    return nimcp_clampf(harm, 0.0f, 1.0f);
}

/**
 * @brief Compute per-party harm breakdown
 */
static void compute_party_harm(
    const fin_ethics_action_t* action,
    float base_harm,
    float party_harm[FIN_PARTY_COUNT]
) {
    fin_action_type_t atype = (fin_action_type_t)action->action_type;

    /* Initialize all parties with baseline harm */
    for (int i = 0; i < FIN_PARTY_COUNT; i++) {
        party_harm[i] = base_harm * 0.1f;
    }

    /* Manipulative actions primarily harm retail and market integrity */
    if (is_manipulative_action(atype)) {
        party_harm[FIN_PARTY_RETAIL] = base_harm * 0.8f;
        party_harm[FIN_PARTY_MARKET_INTEGRITY] = base_harm * 0.9f;
    }

    /* Stop hunting specifically targets retail stop losses */
    if (atype == FIN_ACTION_STOP_HUNT) {
        party_harm[FIN_PARTY_RETAIL] = base_harm;
    }

    /* Liquidations harm the liquidated party */
    if (atype == FIN_ACTION_LIQUIDATE) {
        party_harm[FIN_PARTY_RETAIL] = base_harm * 0.6f;
        party_harm[FIN_PARTY_INSTITUTIONAL] = base_harm * 0.4f;
    }

    /* Large market making may affect other market makers */
    if (atype == FIN_ACTION_MARKET_MAKE && action->action_magnitude > 0.7f) {
        party_harm[FIN_PARTY_MARKET_MAKER] = base_harm * 0.3f;
    }
}

/**
 * @brief Check first Asimov law (no harm to participants)
 */
static bool check_first_law(
    const fin_ethics_action_t* action,
    float harm_score,
    float threshold
) {
    /* First law: Do not cause significant harm */
    if (harm_score > threshold) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "check_first_law: validation failed");
        return false;  /* Violation */
    }

    /* Manipulative actions inherently violate first law */
    if (is_manipulative_action((fin_action_type_t)action->action_type)) {
        return false;
    }

    return true;  /* Compliant */
}

/**
 * @brief Check second Asimov law (regulatory compliance)
 */
static bool check_second_law(
    const fin_ethics_action_t* action
) {
    fin_action_type_t atype = (fin_action_type_t)action->action_type;

    /* Actions that are explicitly illegal */
    switch (atype) {
        case FIN_ACTION_SPOOFING:
        case FIN_ACTION_LAYERING:
        case FIN_ACTION_WASH_TRADE:
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "check_second_law: operation failed");
            return false;  /* Regulatory violations */
        default:
            break;
    }

    return true;  /* Compliant */
}

/**
 * @brief Check third Asimov law (ethical self-interest)
 */
static bool check_third_law(
    const fin_ethics_action_t* action,
    float harm_score
) {
    /* Third law: Self-interest must not override ethics */
    /* If action has high potential profit (magnitude) but causes harm, it violates */
    if (action->action_magnitude > 0.8f && harm_score > 0.5f) {
        return false;
    }

    return true;
}

/**
 * @brief Compute reciprocity score (would you accept this if targeted at you?)
 */
static float compute_reciprocity(
    const fin_ethics_action_t* action
) {
    fin_action_type_t atype = (fin_action_type_t)action->action_type;

    /* Start with perfect reciprocity */
    float reciprocity = 1.0f;

    /* Manipulative actions fail reciprocity test */
    if (is_manipulative_action(atype)) {
        reciprocity = 0.1f;
    }

    /* High-aggression actions reduce reciprocity */
    reciprocity -= action->action_magnitude * 0.3f;

    /* Large position sizes can be more harmful */
    reciprocity -= minf(action->position_size / 20000000.0f, 0.2f);

    return nimcp_clampf(reciprocity, 0.0f, 1.0f);
}

/**
 * @brief Compute empathy score based on affected parties
 */
static float compute_empathy_internal(
    const fin_ethics_action_t* action,
    float harm_score
) {
    /* Empathy is inverse of harm, modulated by action type */
    float empathy = 1.0f - harm_score;

    fin_action_type_t atype = (fin_action_type_t)action->action_type;

    /* Actions targeting vulnerable participants reduce empathy */
    if (atype == FIN_ACTION_STOP_HUNT || atype == FIN_ACTION_LIQUIDATE) {
        empathy *= 0.7f;
    }

    /* Market making and arbitrage are generally neutral */
    if (atype == FIN_ACTION_MARKET_MAKE || atype == FIN_ACTION_ARBITRAGE) {
        empathy = maxf(empathy, 0.6f);
    }

    return nimcp_clampf(empathy, 0.0f, 1.0f);
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

int financial_ethics_bridge_default_config(fin_ethics_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");
        return -1;
    }

    fin_ethics_heartbeat("fin_ethics_default_config", 0.0f);

    memset(config, 0, sizeof(*config));

    /* Harm thresholds */
    config->harm_deny_threshold = 0.75f;
    config->harm_escalate_threshold = 0.5f;
    config->harm_warn_threshold = 0.3f;

    /* Asimov enforcement - all enabled by default */
    config->enforce_first_law = true;
    config->enforce_second_law = true;
    config->enforce_third_law = true;
    config->asimov_violation_threshold = 0.5f;

    /* Golden Rule settings */
    config->enable_golden_rule = true;
    config->reciprocity_threshold = 0.5f;

    /* Empathy modeling */
    config->enable_empathy_modeling = true;
    config->empathy_weight = 0.2f;

    /* Auto-flag all manipulative actions */
    config->auto_flag_stop_hunt = true;
    config->auto_flag_momentum_ignite = true;
    config->auto_flag_spoofing = true;
    config->auto_flag_layering = true;
    config->auto_flag_wash_trade = true;

    /* Integration settings */
    config->enable_immune_integration = true;
    config->enable_bbb_validation = true;
    config->enable_kg_messaging = true;
    config->enable_health_monitoring = true;

    /* Logging */
    config->verbose_logging = false;

    fin_ethics_heartbeat("fin_ethics_default_config", 1.0f);
    return 0;
}

financial_ethics_bridge_t* financial_ethics_bridge_create(
    const fin_ethics_config_t* config
) {
    fin_ethics_heartbeat("fin_ethics_create", 0.0f);

    financial_ethics_bridge_t* bridge = nimcp_calloc(1, sizeof(financial_ethics_bridge_t));
    if (!bridge) {
        set_error("Failed to allocate financial_ethics_bridge");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NO_MEMORY, "Failed to allocate financial_ethics_bridge");
        return NULL;
    }

    bridge->magic = FINANCIAL_ETHICS_BRIDGE_MAGIC;
    bridge->state = FIN_ETHICS_STATE_UNINITIALIZED;

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        financial_ethics_bridge_default_config(&bridge->config);
    }

    /* Initialize bridge base (creates mutex) */
    if (bridge_base_init(&bridge->base, BIO_MODULE_FINANCIAL_ETHICS, "financial_ethics") != 0) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "financial_ethics_bridge_create: validation failed");
        return NULL;
    }

    /* Initialize statistics */
    memset(&bridge->stats, 0, sizeof(bridge->stats));

    bridge->state = FIN_ETHICS_STATE_INITIALIZED;

    fin_ethics_heartbeat("fin_ethics_create", 1.0f);
    return bridge;
}

void financial_ethics_bridge_destroy(financial_ethics_bridge_t* bridge) {
    if (!bridge || bridge->magic != FINANCIAL_ETHICS_BRIDGE_MAGIC) {
        return;
    }

    fin_ethics_heartbeat("fin_ethics_destroy", 0.0f);

    /* Cleanup base */
    bridge_base_cleanup(&bridge->base);

    bridge->magic = 0;
    nimcp_free(bridge);
    bridge = NULL;

    fin_ethics_heartbeat("fin_ethics_destroy", 1.0f);
}

int financial_ethics_bridge_reset(financial_ethics_bridge_t* bridge) {
    if (!bridge || bridge->magic != FINANCIAL_ETHICS_BRIDGE_MAGIC) {
        set_error("bridge is NULL or invalid");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_ethics_bridge_reset: invalid bridge");
        return FIN_ETHICS_ERR_NULL;
    }

    fin_ethics_heartbeat("fin_ethics_reset", 0.0f);

    nimcp_mutex_lock(bridge->base.mutex);

    /* Reset stats */
    memset(&bridge->stats, 0, sizeof(bridge->stats));

    bridge->state = FIN_ETHICS_STATE_INITIALIZED;

    nimcp_mutex_unlock(bridge->base.mutex);

    fin_ethics_heartbeat("fin_ethics_reset", 1.0f);
    return FIN_ETHICS_ERR_OK;
}

/* ============================================================================
 * Subsystem Setters
 * ============================================================================ */

#define FIN_ETHICS_SETTER(name, field) \
    int financial_ethics_bridge_set_##name(financial_ethics_bridge_t* bridge, void* ptr) { \
        if (!bridge || bridge->magic != FINANCIAL_ETHICS_BRIDGE_MAGIC) { \
            set_error("bridge is NULL in set_" #name); \
            NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_ethics_bridge_set_" #name ": bridge is NULL"); \
            return FIN_ETHICS_ERR_NULL; \
        } \
        nimcp_mutex_lock(bridge->base.mutex); \
        bridge->field = ptr; \
        nimcp_mutex_unlock(bridge->base.mutex); \
        return FIN_ETHICS_ERR_OK; \
    }

FIN_ETHICS_SETTER(immune,        immune)
FIN_ETHICS_SETTER(health_agent,  health_agent)
FIN_ETHICS_SETTER(kg_wiring,     kg_wiring)
FIN_ETHICS_SETTER(logger,        logger)
FIN_ETHICS_SETTER(security,      security)
FIN_ETHICS_SETTER(bio_router,    bio_router)

/* Security setters for bbb, ethics, lgss, coordinator handled by bridge_base */

/* ============================================================================
 * Core Ethics API Implementation
 * ============================================================================ */

int financial_ethics_bridge_evaluate(
    financial_ethics_bridge_t* bridge,
    const fin_ethics_action_t* action,
    fin_ethics_result_t* result
) {
    if (!bridge || bridge->magic != FINANCIAL_ETHICS_BRIDGE_MAGIC) {
        set_error("bridge is NULL or invalid");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_ethics_bridge_evaluate: invalid bridge");
        return FIN_ETHICS_ERR_NULL;
    }
    if (!action || !result) {
        set_error("action or result is NULL");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_ethics_bridge_evaluate: NULL argument");
        return FIN_ETHICS_ERR_NULL;
    }

    fin_ethics_heartbeat("fin_ethics_evaluate", 0.0f);

    /* BBB validation */
    if (bridge->config.enable_bbb_validation && bridge->bbb) {
        BRIDGE_BBB_VALIDATE(bridge, action, sizeof(*action));
        bridge->stats.bbb_validations++;
    }

    /* Immune check */
    if (bridge->config.enable_immune_integration && bridge->immune) {
        bridge->stats.immune_checks++;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Initialize result */
    memset(result, 0, sizeof(*result));
    result->verdict = FIN_ETHICS_APPROVED;

    fin_action_type_t atype = (fin_action_type_t)action->action_type;

    /* Step 1: Check for auto-flagged actions */
    bool auto_denied = false;
    if ((atype == FIN_ACTION_STOP_HUNT && bridge->config.auto_flag_stop_hunt) ||
        (atype == FIN_ACTION_MOMENTUM_IGNITE && bridge->config.auto_flag_momentum_ignite) ||
        (atype == FIN_ACTION_SPOOFING && bridge->config.auto_flag_spoofing) ||
        (atype == FIN_ACTION_LAYERING && bridge->config.auto_flag_layering) ||
        (atype == FIN_ACTION_WASH_TRADE && bridge->config.auto_flag_wash_trade)) {
        auto_denied = true;
        result->verdict = FIN_ETHICS_DENIED;
        snprintf(result->reason, sizeof(result->reason),
                 "Action type '%s' is automatically denied as manipulative/illegal trading practice.",
                 fin_ethics_action_name(atype));
    }

    /* Step 2: Compute harm score */
    float harm_score = compute_base_harm(action, &bridge->config);
    result->harm_score = harm_score;

    /* Step 3: Check Asimov compliance */
    bool first_law_ok = true, second_law_ok = true, third_law_ok = true;

    if (bridge->config.enforce_first_law) {
        first_law_ok = check_first_law(action, harm_score, bridge->config.asimov_violation_threshold);
    }
    if (bridge->config.enforce_second_law) {
        second_law_ok = check_second_law(action);
    }
    if (bridge->config.enforce_third_law) {
        third_law_ok = check_third_law(action, harm_score);
    }

    result->asimov_violation = !first_law_ok || !second_law_ok || !third_law_ok;

    /* Step 4: Golden Rule test */
    result->golden_rule_violation = false;
    if (bridge->config.enable_golden_rule) {
        float reciprocity = compute_reciprocity(action);
        if (reciprocity < bridge->config.reciprocity_threshold) {
            result->golden_rule_violation = true;
        }
    }

    /* Step 5: Compute empathy score */
    result->empathy_score = 1.0f;
    if (bridge->config.enable_empathy_modeling) {
        result->empathy_score = compute_empathy_internal(action, harm_score);
    }

    /* Step 6: Build affected parties description */
    if (is_manipulative_action(atype)) {
        snprintf(result->affected_parties, sizeof(result->affected_parties),
                 "Retail investors (primary), Market integrity, Regulators");
    } else if (atype == FIN_ACTION_LIQUIDATE) {
        snprintf(result->affected_parties, sizeof(result->affected_parties),
                 "Liquidated party (retail/institutional)");
    } else if (harm_score > 0.3f) {
        snprintf(result->affected_parties, sizeof(result->affected_parties),
                 "Various market participants (harm score: %.0f%%)", harm_score * 100.0f);
    } else {
        snprintf(result->affected_parties, sizeof(result->affected_parties),
                 "Minimal direct impact on counterparties");
    }

    /* Step 7: Determine final verdict (if not already denied) */
    if (!auto_denied) {
        if (!second_law_ok) {
            /* Regulatory violations are automatic denials */
            result->verdict = FIN_ETHICS_DENIED;
            snprintf(result->reason, sizeof(result->reason),
                     "Second Law violation: Action violates regulatory requirements.");
        } else if (!first_law_ok || harm_score >= bridge->config.harm_deny_threshold) {
            /* Significant harm causes denial */
            result->verdict = FIN_ETHICS_DENIED;
            snprintf(result->reason, sizeof(result->reason),
                     "First Law violation: Action causes significant harm (%.0f%%) to market participants.",
                     harm_score * 100.0f);
        } else if (!third_law_ok || result->golden_rule_violation ||
                   harm_score >= bridge->config.harm_escalate_threshold) {
            /* Borderline cases are escalated */
            result->verdict = FIN_ETHICS_ESCALATE;
            snprintf(result->reason, sizeof(result->reason),
                     "Action requires human review: %s%s%s",
                     !third_law_ok ? "Third Law concern. " : "",
                     result->golden_rule_violation ? "Fails Golden Rule test. " : "",
                     harm_score >= bridge->config.harm_escalate_threshold ? "Elevated harm potential." : "");
        } else if (harm_score >= bridge->config.harm_warn_threshold ||
                   result->empathy_score < 0.5f) {
            /* Low-level concerns generate warning */
            result->verdict = FIN_ETHICS_WARN;
            snprintf(result->reason, sizeof(result->reason),
                     "Proceed with caution: Moderate harm potential (%.0f%%) or low empathy score (%.0f%%).",
                     harm_score * 100.0f, result->empathy_score * 100.0f);
        } else {
            /* Action is approved */
            result->verdict = FIN_ETHICS_APPROVED;
            snprintf(result->reason, sizeof(result->reason),
                     "Action approved: No significant ethical concerns detected.");
        }
    }

    /* Update statistics */
    bridge->stats.evaluations++;
    switch (result->verdict) {
        case FIN_ETHICS_APPROVED:  bridge->stats.approvals++;    break;
        case FIN_ETHICS_DENIED:    bridge->stats.denials++;      break;
        case FIN_ETHICS_ESCALATE:  bridge->stats.escalations++;  break;
        case FIN_ETHICS_WARN:      bridge->stats.warnings++;     break;
    }

    bridge->state = FIN_ETHICS_STATE_ACTIVE;

    nimcp_mutex_unlock(bridge->base.mutex);

    /* KG notification */
    bridge_kg_publish(bridge, KG_MSG_FIN_ETHICS_EVALUATE, result, sizeof(*result));

    /* Publish violation if denied */
    if (result->verdict == FIN_ETHICS_DENIED) {
        bridge_kg_publish(bridge, KG_MSG_FIN_ETHICS_VIOLATION, result, sizeof(*result));
    }

    fin_ethics_heartbeat("fin_ethics_evaluate", 1.0f);
    bridge->stats.health_heartbeats++;

    return FIN_ETHICS_ERR_OK;
}

int financial_ethics_bridge_assess_harm(
    financial_ethics_bridge_t* bridge,
    const fin_ethics_action_t* action,
    fin_harm_assessment_t* assessment
) {
    if (!bridge || bridge->magic != FINANCIAL_ETHICS_BRIDGE_MAGIC) {
        set_error("bridge is NULL or invalid");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_ethics_bridge_assess_harm: invalid bridge");
        return FIN_ETHICS_ERR_NULL;
    }
    if (!action || !assessment) {
        set_error("action or assessment is NULL");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_ethics_bridge_assess_harm: NULL argument");
        return FIN_ETHICS_ERR_NULL;
    }

    fin_ethics_heartbeat("fin_ethics_assess_harm", 0.0f);

    /* BBB validation */
    if (bridge->config.enable_bbb_validation && bridge->bbb) {
        BRIDGE_BBB_VALIDATE(bridge, action, sizeof(*action));
        bridge->stats.bbb_validations++;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    memset(assessment, 0, sizeof(*assessment));

    /* Compute base harm */
    float base_harm = compute_base_harm(action, &bridge->config);
    assessment->total_harm = base_harm;

    /* Compute per-party harm */
    compute_party_harm(action, base_harm, assessment->party_harm);

    /* Aggregate harm categories */
    assessment->counterparty_harm = maxf(assessment->party_harm[FIN_PARTY_RETAIL],
                                          assessment->party_harm[FIN_PARTY_INSTITUTIONAL]);
    assessment->market_harm = assessment->party_harm[FIN_PARTY_MARKET_INTEGRITY];
    assessment->systemic_harm = is_manipulative_action((fin_action_type_t)action->action_type) ?
                                base_harm * 0.5f : base_harm * 0.1f;
    assessment->principal_risk = assessment->party_harm[FIN_PARTY_PRINCIPAL];

    /* Build impact description */
    fin_action_type_t atype = (fin_action_type_t)action->action_type;
    if (is_manipulative_action(atype)) {
        snprintf(assessment->impact_description, sizeof(assessment->impact_description),
                 "High impact: '%s' is a manipulative trading practice causing direct harm to "
                 "retail investors (%.0f%%) and market integrity (%.0f%%). Systemic risk: %.0f%%.",
                 fin_ethics_action_name(atype),
                 assessment->party_harm[FIN_PARTY_RETAIL] * 100.0f,
                 assessment->market_harm * 100.0f,
                 assessment->systemic_harm * 100.0f);
    } else if (base_harm > 0.5f) {
        snprintf(assessment->impact_description, sizeof(assessment->impact_description),
                 "Elevated harm (%.0f%%): Action has significant potential impact on counterparties. "
                 "Primary affected: %s.",
                 base_harm * 100.0f,
                 assessment->party_harm[FIN_PARTY_RETAIL] > assessment->party_harm[FIN_PARTY_INSTITUTIONAL] ?
                 "retail investors" : "institutional investors");
    } else if (base_harm > 0.2f) {
        snprintf(assessment->impact_description, sizeof(assessment->impact_description),
                 "Moderate harm (%.0f%%): Some potential impact on market participants.",
                 base_harm * 100.0f);
    } else {
        snprintf(assessment->impact_description, sizeof(assessment->impact_description),
                 "Low harm (%.0f%%): Minimal expected impact on market participants.",
                 base_harm * 100.0f);
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    /* KG notification */
    bridge_kg_publish(bridge, KG_MSG_FIN_ETHICS_HARM, assessment, sizeof(*assessment));

    fin_ethics_heartbeat("fin_ethics_assess_harm", 1.0f);
    bridge->stats.health_heartbeats++;

    return FIN_ETHICS_ERR_OK;
}

int financial_ethics_bridge_golden_rule(
    financial_ethics_bridge_t* bridge,
    const fin_ethics_action_t* action,
    fin_golden_rule_result_t* result
) {
    if (!bridge || bridge->magic != FINANCIAL_ETHICS_BRIDGE_MAGIC) {
        set_error("bridge is NULL or invalid");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_ethics_bridge_golden_rule: invalid bridge");
        return FIN_ETHICS_ERR_NULL;
    }
    if (!action || !result) {
        set_error("action or result is NULL");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_ethics_bridge_golden_rule: NULL argument");
        return FIN_ETHICS_ERR_NULL;
    }

    fin_ethics_heartbeat("fin_ethics_golden_rule", 0.0f);

    /* BBB validation */
    if (bridge->config.enable_bbb_validation && bridge->bbb) {
        BRIDGE_BBB_VALIDATE(bridge, action, sizeof(*action));
        bridge->stats.bbb_validations++;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    memset(result, 0, sizeof(*result));

    /* Compute reciprocity score */
    result->reciprocity_score = compute_reciprocity(action);

    /* Compute fairness as combination of reciprocity and empathy */
    float harm = compute_base_harm(action, &bridge->config);
    float empathy = compute_empathy_internal(action, harm);
    result->fairness_score = (result->reciprocity_score * 0.6f + empathy * 0.4f);

    /* Determine pass/fail */
    result->passes = (result->reciprocity_score >= bridge->config.reciprocity_threshold) &&
                     (result->fairness_score >= 0.5f);

    /* Build explanation */
    fin_action_type_t atype = (fin_action_type_t)action->action_type;
    if (!result->passes) {
        if (is_manipulative_action(atype)) {
            snprintf(result->explanation, sizeof(result->explanation),
                     "FAILS Golden Rule: '%s' is a manipulative action. You would not accept "
                     "being the target of this practice. Reciprocity: %.0f%%, Fairness: %.0f%%.",
                     fin_ethics_action_name(atype),
                     result->reciprocity_score * 100.0f,
                     result->fairness_score * 100.0f);
        } else {
            snprintf(result->explanation, sizeof(result->explanation),
                     "FAILS Golden Rule: This action's impact (reciprocity: %.0f%%, fairness: %.0f%%) "
                     "is below acceptable thresholds. Consider how you would feel as the counterparty.",
                     result->reciprocity_score * 100.0f,
                     result->fairness_score * 100.0f);
        }
    } else {
        snprintf(result->explanation, sizeof(result->explanation),
                 "PASSES Golden Rule: Action is reasonably fair (reciprocity: %.0f%%, fairness: %.0f%%). "
                 "You would likely accept this treatment if roles were reversed.",
                 result->reciprocity_score * 100.0f,
                 result->fairness_score * 100.0f);
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    /* KG notification */
    bridge_kg_publish(bridge, KG_MSG_FIN_ETHICS_GOLDEN_RULE, result, sizeof(*result));

    fin_ethics_heartbeat("fin_ethics_golden_rule", 1.0f);
    bridge->stats.health_heartbeats++;

    return FIN_ETHICS_ERR_OK;
}

int financial_ethics_bridge_check_asimov(
    financial_ethics_bridge_t* bridge,
    const fin_ethics_action_t* action,
    fin_asimov_result_t* result
) {
    if (!bridge || bridge->magic != FINANCIAL_ETHICS_BRIDGE_MAGIC) {
        set_error("bridge is NULL or invalid");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_ethics_bridge_check_asimov: invalid bridge");
        return FIN_ETHICS_ERR_NULL;
    }
    if (!action || !result) {
        set_error("action or result is NULL");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_ethics_bridge_check_asimov: NULL argument");
        return FIN_ETHICS_ERR_NULL;
    }

    fin_ethics_heartbeat("fin_ethics_check_asimov", 0.0f);

    /* BBB validation */
    if (bridge->config.enable_bbb_validation && bridge->bbb) {
        BRIDGE_BBB_VALIDATE(bridge, action, sizeof(*action));
        bridge->stats.bbb_validations++;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    memset(result, 0, sizeof(*result));

    /* Compute harm for law checks */
    float harm = compute_base_harm(action, &bridge->config);

    /* Check each law */
    bool first_ok = check_first_law(action, harm, bridge->config.asimov_violation_threshold);
    bool second_ok = check_second_law(action);
    bool third_ok = check_third_law(action, harm);

    result->law_violations[0] = !first_ok;
    result->law_violations[1] = !second_ok;
    result->law_violations[2] = !third_ok;

    result->compliant = first_ok && second_ok && third_ok;

    /* Determine primary violation (in order of severity) */
    if (!first_ok) {
        result->primary_violation = FIN_ASIMOV_FIRST_LAW;
        result->severity = harm;
    } else if (!second_ok) {
        result->primary_violation = FIN_ASIMOV_SECOND_LAW;
        result->severity = 0.9f;  /* Regulatory violations are severe */
    } else if (!third_ok) {
        result->primary_violation = FIN_ASIMOV_THIRD_LAW;
        result->severity = 0.5f;  /* Third law violations are less severe */
    } else {
        result->primary_violation = FIN_ASIMOV_NONE;
        result->severity = 0.0f;
    }

    /* Build explanation */
    fin_action_type_t atype = (fin_action_type_t)action->action_type;
    if (result->compliant) {
        snprintf(result->explanation, sizeof(result->explanation),
                 "COMPLIANT: Action '%s' satisfies all three Asimov Laws for AI Trading. "
                 "Harm level: %.0f%% (below threshold).",
                 fin_ethics_action_name(atype), harm * 100.0f);
    } else {
        char violations[NIMCP_ERROR_BUFFER_SIZE] = {0};
        if (!first_ok) {
            strncat(violations, "First Law (no harm), ", sizeof(violations) - strlen(violations) - 1);
        }
        if (!second_ok) {
            strncat(violations, "Second Law (regulatory), ", sizeof(violations) - strlen(violations) - 1);
        }
        if (!third_ok) {
            strncat(violations, "Third Law (ethics over profit), ", sizeof(violations) - strlen(violations) - 1);
        }
        /* Remove trailing comma-space */
        size_t vlen = strlen(violations);
        if (vlen >= 2) violations[vlen - 2] = '\0';

        snprintf(result->explanation, sizeof(result->explanation),
                 "VIOLATION: Action '%s' violates: %s. Primary concern: %s (severity: %.0f%%).",
                 fin_ethics_action_name(atype),
                 violations,
                 fin_ethics_asimov_name(result->primary_violation),
                 result->severity * 100.0f);
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    /* KG notification */
    bridge_kg_publish(bridge, KG_MSG_FIN_ETHICS_ASIMOV, result, sizeof(*result));

    fin_ethics_heartbeat("fin_ethics_check_asimov", 1.0f);
    bridge->stats.health_heartbeats++;

    return FIN_ETHICS_ERR_OK;
}

int financial_ethics_bridge_compute_empathy(
    financial_ethics_bridge_t* bridge,
    const fin_ethics_action_t* action,
    float* empathy_score
) {
    if (!bridge || bridge->magic != FINANCIAL_ETHICS_BRIDGE_MAGIC) {
        set_error("bridge is NULL or invalid");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_ethics_bridge_compute_empathy: invalid bridge");
        return FIN_ETHICS_ERR_NULL;
    }
    if (!action || !empathy_score) {
        set_error("action or empathy_score is NULL");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_ethics_bridge_compute_empathy: NULL argument");
        return FIN_ETHICS_ERR_NULL;
    }

    fin_ethics_heartbeat("fin_ethics_compute_empathy", 0.0f);

    /* BBB validation */
    if (bridge->config.enable_bbb_validation && bridge->bbb) {
        BRIDGE_BBB_VALIDATE(bridge, action, sizeof(*action));
        bridge->stats.bbb_validations++;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    float harm = compute_base_harm(action, &bridge->config);
    *empathy_score = compute_empathy_internal(action, harm);

    nimcp_mutex_unlock(bridge->base.mutex);

    fin_ethics_heartbeat("fin_ethics_compute_empathy", 1.0f);
    bridge->stats.health_heartbeats++;

    return FIN_ETHICS_ERR_OK;
}

/* ============================================================================
 * Query Implementation
 * ============================================================================ */

fin_ethics_bridge_state_t financial_ethics_bridge_get_bridge_state(
    const financial_ethics_bridge_t* bridge
) {
    if (!bridge || bridge->magic != FINANCIAL_ETHICS_BRIDGE_MAGIC) {
        return FIN_ETHICS_STATE_ERROR;
    }
    fin_ethics_heartbeat("fin_ethics_get_bridge_state", 0.0f);
    return bridge->state;
}

int financial_ethics_bridge_get_stats(
    const financial_ethics_bridge_t* bridge,
    fin_ethics_bridge_stats_t* stats
) {
    if (!bridge || bridge->magic != FINANCIAL_ETHICS_BRIDGE_MAGIC || !stats) {
        set_error("NULL argument in get_stats");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "get_stats: NULL argument");
        return FIN_ETHICS_ERR_NULL;
    }

    fin_ethics_heartbeat("fin_ethics_get_stats", 0.0f);

    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);

    return FIN_ETHICS_ERR_OK;
}

void financial_ethics_bridge_reset_stats(financial_ethics_bridge_t* bridge) {
    if (!bridge || bridge->magic != FINANCIAL_ETHICS_BRIDGE_MAGIC) {
        return;
    }

    fin_ethics_heartbeat("fin_ethics_reset_stats", 0.0f);

    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    nimcp_mutex_unlock(bridge->base.mutex);
}

const char* financial_ethics_bridge_get_last_error(void) {
    return fin_ethics_last_error;
}

/* ============================================================================
 * Health Integration
 * ============================================================================ */

int financial_ethics_bridge_heartbeat(
    financial_ethics_bridge_t* bridge,
    const char* operation,
    float progress
) {
    if (!bridge || bridge->magic != FINANCIAL_ETHICS_BRIDGE_MAGIC) {
        return FIN_ETHICS_ERR_NULL;
    }

    /* Forward to global health agent */
    fin_ethics_heartbeat(operation ? operation : "fin_ethics_heartbeat", progress);

    /* Forward to instance-level health agent */
    if (bridge->health_agent) {
        nimcp_health_agent_heartbeat_ex(
            (nimcp_health_agent_t*)bridge->health_agent, operation, progress);
    }

    bridge->stats.health_heartbeats++;
    return FIN_ETHICS_ERR_OK;
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

const char* fin_ethics_verdict_name(fin_ethics_verdict_t verdict) {
    if (verdict >= sizeof(verdict_names) / sizeof(verdict_names[0])) {
        return "unknown";
    }
    return verdict_names[verdict];
}

const char* fin_ethics_action_name(fin_action_type_t action_type) {
    if (action_type >= FIN_ACTION_COUNT) {
        return "unknown";
    }
    return action_names[action_type];
}

const char* fin_ethics_asimov_name(fin_asimov_law_t law) {
    if (law > FIN_ASIMOV_THIRD_LAW) {
        return "unknown";
    }
    return asimov_names[law];
}

const char* fin_ethics_party_name(fin_party_type_t party) {
    if (party >= FIN_PARTY_COUNT) {
        return "unknown";
    }
    return party_names[party];
}

const char* fin_ethics_state_name(fin_ethics_bridge_state_t state) {
    if (state > FIN_ETHICS_STATE_ERROR) {
        return "unknown";
    }
    return state_names[state];
}

const char* financial_ethics_bridge_version(void) {
    return FINANCIAL_ETHICS_BRIDGE_VERSION;
}

bool financial_ethics_bridge_is_auto_flagged(
    const financial_ethics_bridge_t* bridge,
    fin_action_type_t action_type
) {
    if (!bridge || bridge->magic != FINANCIAL_ETHICS_BRIDGE_MAGIC) {
        return false;
    }

    const fin_ethics_config_t* cfg = &bridge->config;

    switch (action_type) {
        case FIN_ACTION_STOP_HUNT:        return cfg->auto_flag_stop_hunt;
        case FIN_ACTION_MOMENTUM_IGNITE:  return cfg->auto_flag_momentum_ignite;
        case FIN_ACTION_SPOOFING:         return cfg->auto_flag_spoofing;
        case FIN_ACTION_LAYERING:         return cfg->auto_flag_layering;
        case FIN_ACTION_WASH_TRADE:       return cfg->auto_flag_wash_trade;
        default:                          return false;
    }
}

/* ============================================================================
 * Training Integration (B23 Upgrade Compatibility)
 * ============================================================================ */

int financial_ethics_bridge_training_begin(financial_ethics_bridge_t* bridge) {
    if (!bridge || bridge->magic != FINANCIAL_ETHICS_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "financial_ethics_bridge_training_begin: NULL argument");
        return -1;
    }
    fin_ethics_heartbeat_instance((nimcp_health_agent_t*)bridge->health_agent,
                                   "financial_ethics_bridge_training_begin", 0.0f);
    return 0;
}

int financial_ethics_bridge_training_end(financial_ethics_bridge_t* bridge) {
    if (!bridge || bridge->magic != FINANCIAL_ETHICS_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "financial_ethics_bridge_training_end: NULL argument");
        return -1;
    }
    fin_ethics_heartbeat_instance((nimcp_health_agent_t*)bridge->health_agent,
                                   "financial_ethics_bridge_training_end", 1.0f);
    return 0;
}

int financial_ethics_bridge_training_step(financial_ethics_bridge_t* bridge, float progress) {
    if (!bridge || bridge->magic != FINANCIAL_ETHICS_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "financial_ethics_bridge_training_step: NULL argument");
        return -1;
    }

    /* Safety gates: ethics + LGSS pre-check */
    BRIDGE_ETHICS_GATE(bridge, "financial_ethics_bridge_training_step");
    BRIDGE_LGSS_GATE(bridge, "financial_ethics_bridge_training_step");

    fin_ethics_heartbeat_instance((nimcp_health_agent_t*)bridge->health_agent,
                                   "financial_ethics_bridge_training_step", progress);

    /* Notify coordinator of update cycle completion */
    bridge_base_notify_coordinator_tick(&bridge->base, 0);
    return 0;
}
