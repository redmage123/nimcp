/**
 * @file nimcp_financial_bridge.c
 * @brief Financial System Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-29
 *
 * WHAT: Central bridge connecting financial sub-engines (investment, market,
 *       neural, archetype) to brain subsystems (immune, BBB, health, KG,
 *       ethics, LGSS, security, hypothalamus, medulla, cerebellum).
 *
 * WHY:  Financial operations have safety implications (fraud, manipulation,
 *       harm to investors) that require multi-layer validation. The bridge
 *       provides a defense-in-depth pipeline:
 *         L0: LGSS financial fraud rules (crisp deny/escalate/warn)
 *         L1: Fuzzy safety scoring (continuous risk gating)
 *         L2: Ethics engine (moral reasoning)
 *         L3: BBB integrity check (system health gate)
 *
 * HOW:  Each subsystem is connected via individual setter functions.
 *       Validation pipeline processes actions through all connected layers.
 *       Fuzzy integration provides continuous safety scoring for borderline
 *       cases that pass binary LGSS checks but carry elevated risk.
 *
 * @author NIMCP Development Team
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdarg.h>

#include "cognitive/parietal/nimcp_financial_bridge.h"
#include "constants/nimcp_buffer_constants.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_timing_constants.h"
#include "utils/math/nimcp_math_helpers.h"

/* Health agent: using pre-existing custom implementation */
static nimcp_health_agent_t* g_financial_bridge_health_agent = NULL;

BRIDGE_DEFINE_MESH_REGISTRATION(financial_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


/** @brief Send heartbeat from financial_bridge module */
static inline void fin_bridge_heartbeat(const char* operation, float progress) {
    if (g_financial_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_financial_bridge_health_agent, operation, progress);
    }
}

/** @brief Send heartbeat from financial_bridge module (instance-level) */
static inline void fin_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_financial_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_financial_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_financial_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}

//=============================================================================
// Thread-Local Error
//=============================================================================

static _Thread_local char fin_bridge_last_error[NIMCP_ERROR_BUFFER_SIZE] = {0};

static void set_error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(fin_bridge_last_error, sizeof(fin_bridge_last_error), fmt, args);
    va_end(args);
}

//=============================================================================
// Internal Structure
//=============================================================================

struct financial_bridge {
    fin_bridge_config_t config;
    fin_bridge_state_t state;
    fin_bridge_stats_t stats;
    float inflammation;
    float fatigue;
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
    void* hypothalamus;
    void* medulla;
    void* cerebellum;
    void* fuzzy_bridge;
    /* Callbacks */
    fin_bridge_validation_callback_t validation_cb;
    void* validation_cb_data;
    fin_bridge_health_callback_t health_cb;
    void* health_cb_data;
    /* Internal state */
    float current_risk_appetite;
    fin_autonomic_state_t autonomic;
};

//=============================================================================
// KG Wiring Integration (Change Set 1)
//=============================================================================

/* KG message type defines for bridge module */
#define KG_MSG_FIN_BRIDGE_REQUEST    "FIN_BRIDGE_REQUEST"
#define KG_MSG_FIN_BRIDGE_RESPONSE   "FIN_BRIDGE_RESPONSE"
#define KG_MSG_FIN_BRIDGE_ERROR      "FIN_BRIDGE_ERROR"
#define KG_MSG_FIN_BRIDGE_UPDATE     "FIN_BRIDGE_UPDATE"

/**
 * @brief Publish a message through KG wiring
 * @param bridge Financial bridge instance
 * @param msg_type Message type string
 * @param payload Payload data
 * @param size Payload size in bytes
 * @return 0 on success
 */
static int bridge_kg_publish(financial_bridge_t* bridge, const char* msg_type,
                              const void* payload, size_t size) {
    if (bridge && bridge->kg_wiring) {
        /* kg_wiring_publish would be called here */
        (void)msg_type; (void)payload; (void)size;
        return 0;
    }
    return 0;
}

//=============================================================================
// Helper: Worst (most severe) validation result
//=============================================================================

static inline fin_validation_result_t worst_result(fin_validation_result_t a,
                                                    fin_validation_result_t b) {
    return (a > b) ? a : b;
}

//=============================================================================
// Lifecycle
//=============================================================================

fin_bridge_config_t financial_bridge_default_config(void) {
    fin_bridge_heartbeat("fin_bridge_default_config", 0.0f);

    fin_bridge_config_t config;
    memset(&config, 0, sizeof(config));

    /* Enable all validation layers */
    config.enable_lgss_validation   = true;
    config.enable_ethics_validation = true;
    config.enable_bbb_validation    = true;
    config.enable_fuzzy_validation  = true;
    config.enable_fuzzy_risk_gating = true;
    config.fuzzy_risk_gate_threshold = 0.3f;

    /* Enable all brain regions */
    config.enable_hypothalamus      = true;
    config.enable_medulla           = true;
    config.enable_cerebellum        = true;
    config.enable_health_monitoring = true;

    /* Timing */
    config.validation_timeout_ms    = NIMCP_MEDIUM_TIMEOUT_MS;
    config.health_check_interval_ms = 5000;

    /* Modulation defaults */
    config.inflammation_sensitivity = 0.5f;
    config.fatigue_sensitivity      = 0.5f;

    /* Logging */
    config.enable_audit_log          = true;
    config.enable_verbose_validation = false;

    return config;
}

financial_bridge_t* financial_bridge_create(const fin_bridge_config_t* config) {
    fin_bridge_heartbeat("fin_bridge_create", 0.0f);

    financial_bridge_t* bridge = (financial_bridge_t*)nimcp_malloc(sizeof(financial_bridge_t));
    if (!bridge) {
        set_error("Failed to allocate financial_bridge");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NO_MEMORY, "Failed to allocate financial_bridge");
        return NULL;
    }
    memset(bridge, 0, sizeof(financial_bridge_t));

    /* Copy configuration or use defaults */
    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = financial_bridge_default_config();
    }

    /* Initialize state */
    bridge->state = FIN_BRIDGE_STATE_IDLE;

    /* Zero stats (already zeroed by memset, but explicit) */
    memset(&bridge->stats, 0, sizeof(fin_bridge_stats_t));

    /* Default internal state */
    bridge->current_risk_appetite = 0.5f;
    bridge->inflammation = 0.0f;
    bridge->fatigue = 0.0f;

    /* Autonomic defaults */
    memset(&bridge->autonomic, 0, sizeof(fin_autonomic_state_t));
    bridge->autonomic.liquidity_status = 1.0f;

    fin_bridge_heartbeat("fin_bridge_create", 1.0f);
    return bridge;
}

void financial_bridge_destroy(financial_bridge_t* bridge) {
    fin_bridge_heartbeat("fin_bridge_destroy", 0.0f);
    if (bridge) {
        nimcp_free(bridge);
        bridge = NULL;
    }
}

fin_bridge_state_t financial_bridge_get_state(const financial_bridge_t* bridge) {
    if (!bridge) {
        return FIN_BRIDGE_STATE_UNINITIALIZED;
    }
    return bridge->state;
}

int financial_bridge_reset(financial_bridge_t* bridge) {
    if (!bridge) {
        set_error("bridge is NULL");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_bridge_reset: bridge is NULL");
        return FIN_BRIDGE_ERR_NULL;
    }
    fin_bridge_heartbeat("fin_bridge_reset", 0.0f);

    bridge->state = FIN_BRIDGE_STATE_IDLE;
    memset(&bridge->stats, 0, sizeof(fin_bridge_stats_t));
    bridge->inflammation = 0.0f;
    bridge->fatigue = 0.0f;
    bridge->current_risk_appetite = 0.5f;
    memset(&bridge->autonomic, 0, sizeof(fin_autonomic_state_t));
    bridge->autonomic.liquidity_status = 1.0f;

    fin_bridge_heartbeat("fin_bridge_reset", 1.0f);
    return FIN_BRIDGE_ERR_OK;
}

//=============================================================================
// Subsystem Setters (Macro Pattern)
//=============================================================================

#define FIN_BRIDGE_SETTER(name, field) \
    int financial_bridge_set_##name(financial_bridge_t* bridge, void* ptr) { \
        if (!bridge) { \
            set_error("bridge is NULL in set_" #name); \
            NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_bridge_set_" #name ": bridge is NULL"); \
            return FIN_BRIDGE_ERR_NULL; \
        } \
        bridge->field = ptr; \
        return FIN_BRIDGE_ERR_OK; \
    }

FIN_BRIDGE_SETTER(immune,        immune)
FIN_BRIDGE_SETTER(bbb,           bbb)
FIN_BRIDGE_SETTER(health_agent,  health_agent)
FIN_BRIDGE_SETTER(kg_wiring,     kg_wiring)
FIN_BRIDGE_SETTER(logger,        logger)
FIN_BRIDGE_SETTER(security,      security)
FIN_BRIDGE_SETTER(ethics,        ethics)
FIN_BRIDGE_SETTER(lgss,          lgss)
FIN_BRIDGE_SETTER(cycle,         cycle)
FIN_BRIDGE_SETTER(bio_router,    bio_router)
FIN_BRIDGE_SETTER(hypothalamus,  hypothalamus)
FIN_BRIDGE_SETTER(medulla,       medulla)
FIN_BRIDGE_SETTER(cerebellum,    cerebellum)
FIN_BRIDGE_SETTER(fuzzy_bridge,  fuzzy_bridge)

//=============================================================================
// Callbacks
//=============================================================================

int financial_bridge_set_validation_callback(financial_bridge_t* bridge,
                                              fin_bridge_validation_callback_t cb,
                                              void* user_data) {
    if (!bridge) {
        set_error("bridge is NULL in set_validation_callback");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_bridge_set_validation_callback: bridge is NULL");
        return FIN_BRIDGE_ERR_NULL;
    }
    bridge->validation_cb = cb;
    bridge->validation_cb_data = user_data;
    return FIN_BRIDGE_ERR_OK;
}

int financial_bridge_set_health_callback(financial_bridge_t* bridge,
                                          fin_bridge_health_callback_t cb,
                                          void* user_data) {
    if (!bridge) {
        set_error("bridge is NULL in set_health_callback");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_bridge_set_health_callback: bridge is NULL");
        return FIN_BRIDGE_ERR_NULL;
    }
    bridge->health_cb = cb;
    bridge->health_cb_data = user_data;
    return FIN_BRIDGE_ERR_OK;
}

//=============================================================================
// LGSS Quick Check (L0 Only)
//=============================================================================

fin_validation_result_t financial_bridge_lgss_check(financial_bridge_t* bridge,
                                                     const fin_action_t* action) {
    if (!bridge || !action) {
        return FIN_VALIDATION_ERROR;
    }
    fin_bridge_heartbeat("fin_bridge_lgss_check", 0.0f);

    /* Sanctioned counterparty -> immediate DENY */
    if (action->counterparty_sanctioned) {
        return FIN_VALIDATION_DENY;
    }

    /* No consent on a trade action -> DENY */
    if (!action->has_client_consent &&
        (action->type == FIN_ACTION_BUY  ||
         action->type == FIN_ACTION_SELL  ||
         action->type == FIN_ACTION_SHORT ||
         action->type == FIN_ACTION_DERIVATIVE_TRADE)) {
        return FIN_VALIDATION_DENY;
    }

    /* Excessive leverage -> ESCALATE */
    if (action->leverage_ratio > 10.0f) {
        return FIN_VALIDATION_ESCALATE;
    }

    /* High concentration -> WARN */
    if (action->position_weight > 0.50f) {
        return FIN_VALIDATION_WARN;
    }

    return FIN_VALIDATION_PASS;
}

//=============================================================================
// Fuzzy Safety Scoring (L1)
//=============================================================================

int financial_bridge_fuzzy_score(financial_bridge_t* bridge,
                                 const fin_action_t* action,
                                 float* out_safety_score,
                                 float* out_risk_score) {
    if (!bridge || !action) {
        set_error("NULL argument in fuzzy_score");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_bridge_fuzzy_score: NULL argument");
        return FIN_BRIDGE_ERR_NULL;
    }
    fin_bridge_heartbeat("fin_bridge_fuzzy_score", 0.0f);

    /* Compute normalized risk inputs */
    float leverage_risk     = nimcp_clampf(action->leverage_ratio / 20.0f, 0.0f, 1.0f);
    float concentration_risk = nimcp_clampf(action->concentration, 0.0f, 1.0f);
    float magnitude_risk    = nimcp_clampf(action->magnitude / 1e6f, 0.0f, 1.0f);
    float consent_risk      = action->has_client_consent ? 0.0f : 0.5f;

    /* Weighted combination */
    float risk_score = 0.30f * leverage_risk
                     + 0.25f * concentration_risk
                     + 0.25f * magnitude_risk
                     + 0.20f * consent_risk;

    risk_score = nimcp_clampf(risk_score, 0.0f, 1.0f);
    float safety_score = 1.0f - risk_score;
    safety_score = nimcp_clampf(safety_score, 0.0f, 1.0f);

    if (out_safety_score) *out_safety_score = safety_score;
    if (out_risk_score)   *out_risk_score   = risk_score;

    /* Update stats */
    bridge->stats.fuzzy_validation_calls++;
    /* Running average for safety score */
    float n = (float)bridge->stats.fuzzy_validation_calls;
    bridge->stats.avg_fuzzy_safety_score =
        bridge->stats.avg_fuzzy_safety_score * ((n - 1.0f) / (fabsf(n) > 1e-7f ? n : 1e-7f)) + safety_score / (fabsf(n) > 1e-7f ? n : 1e-7f);

    fin_bridge_heartbeat("fin_bridge_fuzzy_score", 1.0f);
    return FIN_BRIDGE_ERR_OK;
}

//=============================================================================
// Full Validation Pipeline
//=============================================================================

int financial_bridge_validate_action(financial_bridge_t* bridge,
                                     const fin_action_t* action,
                                     fin_validation_report_t* out_report) {
    if (!bridge || !action || !out_report) {
        set_error("NULL argument in validate_action");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_bridge_validate_action: NULL argument");
        return FIN_BRIDGE_ERR_NULL;
    }
    fin_bridge_heartbeat("fin_bridge_validate", 0.0f);

    memset(out_report, 0, sizeof(fin_validation_report_t));
    out_report->result       = FIN_VALIDATION_PASS;
    out_report->lgss_result  = FIN_VALIDATION_PASS;
    out_report->ethics_result = FIN_VALIDATION_PASS;
    out_report->bbb_result   = FIN_VALIDATION_PASS;
    out_report->fuzzy_safety_score = 1.0f;
    out_report->fuzzy_risk_score   = 0.0f;
    out_report->fuzzy_gate_passed  = true;

    bridge->stats.total_validations++;

    /* ---------------------------------------------------------------
     * L0: LGSS Financial Fraud Rules
     * --------------------------------------------------------------- */
    if (bridge->config.enable_lgss_validation) {
        fin_bridge_heartbeat("fin_bridge_validate_lgss", 0.2f);

        fin_validation_result_t lgss = FIN_VALIDATION_PASS;

        /* Sanctioned counterparty */
        if (action->counterparty_sanctioned) {
            lgss = FIN_VALIDATION_DENY;
            snprintf(out_report->lgss_rule_id, sizeof(out_report->lgss_rule_id),
                     "LGSS-SANCTION-001");
            snprintf(out_report->lgss_rationale, sizeof(out_report->lgss_rationale),
                     "Counterparty '%s' is on sanctions list", action->counterparty);
            bridge->stats.lgss_denials++;
        }
        /* No consent on trade action */
        else if (!action->has_client_consent &&
                 (action->type == FIN_ACTION_BUY  ||
                  action->type == FIN_ACTION_SELL  ||
                  action->type == FIN_ACTION_SHORT ||
                  action->type == FIN_ACTION_DERIVATIVE_TRADE)) {
            lgss = FIN_VALIDATION_DENY;
            snprintf(out_report->lgss_rule_id, sizeof(out_report->lgss_rule_id),
                     "LGSS-CONSENT-001");
            snprintf(out_report->lgss_rationale, sizeof(out_report->lgss_rationale),
                     "Trade action type %d requires client consent", (int)action->type);
            bridge->stats.lgss_denials++;
        }
        /* Suitability check */
        else if (!action->is_suitable) {
            lgss = FIN_VALIDATION_ESCALATE;
            snprintf(out_report->lgss_rule_id, sizeof(out_report->lgss_rule_id),
                     "LGSS-SUIT-001");
            snprintf(out_report->lgss_rationale, sizeof(out_report->lgss_rationale),
                     "Action on '%s' flagged as unsuitable for client", action->symbol);
            bridge->stats.lgss_escalations++;
        }
        /* High leverage */
        else if (action->leverage_ratio > 10.0f) {
            lgss = FIN_VALIDATION_ESCALATE;
            snprintf(out_report->lgss_rule_id, sizeof(out_report->lgss_rule_id),
                     "LGSS-LEV-001");
            snprintf(out_report->lgss_rationale, sizeof(out_report->lgss_rationale),
                     "Leverage ratio %.2f exceeds 10x threshold", action->leverage_ratio);
            bridge->stats.lgss_escalations++;
        }
        /* High concentration */
        else if (action->position_weight > 0.50f) {
            lgss = FIN_VALIDATION_WARN;
            snprintf(out_report->lgss_rule_id, sizeof(out_report->lgss_rule_id),
                     "LGSS-CONC-001");
            snprintf(out_report->lgss_rationale, sizeof(out_report->lgss_rationale),
                     "Position weight %.2f exceeds 50%% concentration limit",
                     action->position_weight);
            bridge->stats.lgss_warnings++;
        }

        out_report->lgss_result = lgss;
        out_report->result = worst_result(out_report->result, lgss);
    }

    /* ---------------------------------------------------------------
     * L1: Fuzzy Safety Scoring
     * --------------------------------------------------------------- */
    if (bridge->config.enable_fuzzy_validation) {
        fin_bridge_heartbeat("fin_bridge_validate_fuzzy", 0.4f);

        float safety = 1.0f;
        float risk   = 0.0f;
        financial_bridge_fuzzy_score(bridge, action, &safety, &risk);

        out_report->fuzzy_safety_score = safety;
        out_report->fuzzy_risk_score   = risk;

        if (bridge->config.enable_fuzzy_risk_gating) {
            if (safety < bridge->config.fuzzy_risk_gate_threshold) {
                out_report->fuzzy_gate_passed = false;
                bridge->stats.fuzzy_risk_gates++;
                /* Fuzzy gate failure escalates if not already denied */
                if (out_report->result < FIN_VALIDATION_ESCALATE) {
                    out_report->result = FIN_VALIDATION_ESCALATE;
                }
            } else {
                out_report->fuzzy_gate_passed = true;
            }
        }
    }

    /* ---------------------------------------------------------------
     * L2: Ethics Engine
     * --------------------------------------------------------------- */
    if (bridge->config.enable_ethics_validation && bridge->ethics) {
        fin_bridge_heartbeat("fin_bridge_validate_ethics", 0.6f);

        /* Simplified ethics scoring:
         * 1.0 if suitable and consented, penalize otherwise */
        float ethics_score = 1.0f;
        if (!action->is_suitable) {
            ethics_score -= 0.4f;
        }
        if (!action->has_client_consent) {
            ethics_score -= 0.4f;
        }
        /* Additional penalty for elderly clients in high-risk trades */
        if (action->client_age > 70 && action->leverage_ratio > 2.0f) {
            ethics_score -= 0.2f;
        }
        ethics_score = nimcp_clampf(ethics_score, 0.0f, 1.0f);
        out_report->ethics_score = ethics_score;

        if (ethics_score < 0.3f) {
            out_report->ethics_result = FIN_VALIDATION_DENY;
            bridge->stats.ethics_denials++;
        } else if (ethics_score < 0.6f) {
            out_report->ethics_result = FIN_VALIDATION_ESCALATE;
        } else if (ethics_score < 0.8f) {
            out_report->ethics_result = FIN_VALIDATION_WARN;
        } else {
            out_report->ethics_result = FIN_VALIDATION_PASS;
        }
        out_report->result = worst_result(out_report->result, out_report->ethics_result);
    }

    /* ---------------------------------------------------------------
     * L3: BBB Integrity Check
     * --------------------------------------------------------------- */
    if (bridge->config.enable_bbb_validation && bridge->bbb) {
        fin_bridge_heartbeat("fin_bridge_validate_bbb", 0.8f);

        /* Check system health via inflammation/fatigue as proxy for BBB state */
        float health_penalty = bridge->inflammation * bridge->config.inflammation_sensitivity
                             + bridge->fatigue * bridge->config.fatigue_sensitivity;

        if (health_penalty > 1.0f) {
            out_report->bbb_result = FIN_VALIDATION_DENY;
            bridge->stats.bbb_denials++;
        } else if (health_penalty > 0.7f) {
            out_report->bbb_result = FIN_VALIDATION_ESCALATE;
        } else if (health_penalty > 0.4f) {
            out_report->bbb_result = FIN_VALIDATION_WARN;
        } else {
            out_report->bbb_result = FIN_VALIDATION_PASS;
        }
        out_report->result = worst_result(out_report->result, out_report->bbb_result);
    }

    /* ---------------------------------------------------------------
     * Final Accounting
     * --------------------------------------------------------------- */
    if (out_report->result == FIN_VALIDATION_PASS) {
        bridge->stats.actions_passed++;
    } else {
        bridge->stats.actions_denied++;
    }

    /* Fire validation callback if registered */
    if (bridge->validation_cb) {
        bridge->validation_cb(bridge, action, out_report, bridge->validation_cb_data);
    }

    /* Update bridge state based on outcome */
    if (out_report->result == FIN_VALIDATION_ERROR) {
        bridge->state = FIN_BRIDGE_STATE_ERROR;
    } else if (out_report->result == FIN_VALIDATION_DENY) {
        /* Stay active, denials are normal operation */
        bridge->state = FIN_BRIDGE_STATE_ACTIVE;
    } else {
        bridge->state = FIN_BRIDGE_STATE_ACTIVE;
    }

    fin_bridge_heartbeat("fin_bridge_validate", 1.0f);
    return FIN_BRIDGE_ERR_OK;
}

//=============================================================================
// Brain Region Integration: Hypothalamus Risk Drive
//=============================================================================

int financial_bridge_get_risk_drive(financial_bridge_t* bridge,
                                    float* out_risk_appetite) {
    if (!bridge) {
        set_error("bridge is NULL in get_risk_drive");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_bridge_get_risk_drive: bridge is NULL");
        return FIN_BRIDGE_ERR_NULL;
    }
    if (!out_risk_appetite) {
        set_error("out_risk_appetite is NULL");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_bridge_get_risk_drive: out_risk_appetite is NULL");
        return FIN_BRIDGE_ERR_NULL;
    }
    fin_bridge_heartbeat("fin_bridge_risk_drive", 0.0f);

    if (bridge->config.enable_hypothalamus && bridge->hypothalamus) {
        /* Compute fuzzy risk appetite from hypothalamic arousal level.
         * The hypothalamus drives approach/avoidance behavior.
         * Higher arousal -> higher risk appetite (up to a point).
         * We use the current portfolio stress as a proxy for arousal. */
        float arousal = bridge->autonomic.stress_level;

        /* Inverted-U model: moderate arousal = peak risk appetite.
         * appetite = 1 - (arousal - 0.5)^2 * 4
         * This gives appetite=1.0 at arousal=0.5, appetite=0.0 at arousal=0 or 1 */
        float deviation = arousal - 0.5f;
        float appetite = 1.0f - 4.0f * deviation * deviation;
        appetite = nimcp_clampf(appetite, 0.0f, 1.0f);

        /* Modulate by inflammation and fatigue */
        appetite *= (1.0f - bridge->inflammation * bridge->config.inflammation_sensitivity);
        appetite *= (1.0f - bridge->fatigue * bridge->config.fatigue_sensitivity);
        appetite = nimcp_clampf(appetite, 0.0f, 1.0f);

        bridge->current_risk_appetite = appetite;
        *out_risk_appetite = appetite;
    } else {
        /* No hypothalamus connected; return moderate default */
        bridge->current_risk_appetite = 0.5f;
        *out_risk_appetite = 0.5f;
    }

    fin_bridge_heartbeat("fin_bridge_risk_drive", 1.0f);
    return FIN_BRIDGE_ERR_OK;
}

fin_risk_drive_t financial_bridge_get_risk_drive_level(financial_bridge_t* bridge) {
    if (!bridge) {
        return FIN_RISK_DRIVE_MODERATE;
    }

    float appetite = 0.0f;
    financial_bridge_get_risk_drive(bridge, &appetite);

    if (appetite < 0.15f) {
        return FIN_RISK_DRIVE_MINIMAL;
    } else if (appetite < 0.35f) {
        return FIN_RISK_DRIVE_CONSERVATIVE;
    } else if (appetite < 0.65f) {
        return FIN_RISK_DRIVE_MODERATE;
    } else if (appetite < 0.85f) {
        return FIN_RISK_DRIVE_AGGRESSIVE;
    } else {
        return FIN_RISK_DRIVE_MAXIMUM;
    }
}

//=============================================================================
// Brain Region Integration: Cerebellum Execution Timing
//=============================================================================

int financial_bridge_get_execution_timing(financial_bridge_t* bridge,
                                          float urgency, float precision,
                                          fin_execution_timing_t* out_timing) {
    if (!bridge) {
        set_error("bridge is NULL in get_execution_timing");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_bridge_get_execution_timing: bridge is NULL");
        return FIN_BRIDGE_ERR_NULL;
    }
    if (!out_timing) {
        set_error("out_timing is NULL");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_bridge_get_execution_timing: out_timing is NULL");
        return FIN_BRIDGE_ERR_NULL;
    }
    fin_bridge_heartbeat("fin_bridge_exec_timing", 0.0f);

    memset(out_timing, 0, sizeof(fin_execution_timing_t));
    urgency   = nimcp_clampf(urgency, 0.0f, 1.0f);
    precision = nimcp_clampf(precision, 0.0f, 1.0f);

    out_timing->execution_urgency      = urgency;
    out_timing->precision_requirement  = precision;

    if (bridge->config.enable_cerebellum && bridge->cerebellum) {
        /* Cerebellum-guided timing model:
         * High urgency + low precision  = immediate execution
         * Low urgency  + high precision = wait for optimal entry
         * Both high = compromise delay
         * Both low  = moderate default */

        if (urgency > 0.7f && precision < 0.3f) {
            /* Immediate execution */
            out_timing->requires_immediate = true;
            out_timing->optimal_delay_ms   = 0.0f;
        } else if (urgency < 0.3f && precision > 0.7f) {
            /* Patient, precision-seeking delay */
            out_timing->requires_immediate = false;
            out_timing->optimal_delay_ms   = 500.0f * precision;
        } else {
            /* Balanced: compute from weighted combination */
            float delay = 200.0f * (1.0f - urgency) * precision;
            out_timing->requires_immediate = (delay < 10.0f);
            out_timing->optimal_delay_ms   = delay;
        }

        /* Fatigue increases delay (cerebellum timing degrades under fatigue) */
        if (!out_timing->requires_immediate) {
            out_timing->optimal_delay_ms *= (1.0f + bridge->fatigue *
                                              bridge->config.fatigue_sensitivity);
        }
    } else {
        /* No cerebellum connected; default timing */
        out_timing->requires_immediate = (urgency > 0.8f);
        out_timing->optimal_delay_ms   = out_timing->requires_immediate ? 0.0f : 100.0f;
    }

    fin_bridge_heartbeat("fin_bridge_exec_timing", 1.0f);
    return FIN_BRIDGE_ERR_OK;
}

//=============================================================================
// Brain Region Integration: Medulla Autonomic State
//=============================================================================

int financial_bridge_get_autonomic_state(financial_bridge_t* bridge,
                                         fin_autonomic_state_t* out_state) {
    if (!bridge) {
        set_error("bridge is NULL in get_autonomic_state");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_bridge_get_autonomic_state: bridge is NULL");
        return FIN_BRIDGE_ERR_NULL;
    }
    if (!out_state) {
        set_error("out_state is NULL");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_bridge_get_autonomic_state: out_state is NULL");
        return FIN_BRIDGE_ERR_NULL;
    }

    *out_state = bridge->autonomic;
    return FIN_BRIDGE_ERR_OK;
}

int financial_bridge_update_autonomic(financial_bridge_t* bridge,
                                      float portfolio_volatility,
                                      float drawdown,
                                      float liquidity_ratio) {
    if (!bridge) {
        set_error("bridge is NULL in update_autonomic");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_bridge_update_autonomic: bridge is NULL");
        return FIN_BRIDGE_ERR_NULL;
    }
    fin_bridge_heartbeat("fin_bridge_update_auto", 0.0f);

    portfolio_volatility = nimcp_clampf(portfolio_volatility, 0.0f, 1.0f);
    drawdown             = nimcp_clampf(drawdown, 0.0f, 1.0f);
    liquidity_ratio      = nimcp_clampf(liquidity_ratio, 0.0f, 1.0f);

    /* Map portfolio metrics to autonomic state */
    bridge->autonomic.volatility_pressure = portfolio_volatility;
    bridge->autonomic.liquidity_status    = liquidity_ratio;

    /* Portfolio "heartrate" increases with volatility */
    bridge->autonomic.portfolio_heartrate = 60.0f + 140.0f * portfolio_volatility;

    /* Stress level is a combination of volatility and drawdown */
    bridge->autonomic.stress_level = nimcp_clampf(
        0.5f * portfolio_volatility + 0.5f * drawdown, 0.0f, 1.0f);

    /* Panic detection: severe drawdown combined with high volatility */
    bridge->autonomic.panic_detected =
        (drawdown > 0.20f && portfolio_volatility > 0.40f);

    /* If panic detected, push bridge to degraded state */
    if (bridge->autonomic.panic_detected) {
        if (bridge->state == FIN_BRIDGE_STATE_ACTIVE ||
            bridge->state == FIN_BRIDGE_STATE_IDLE) {
            bridge->state = FIN_BRIDGE_STATE_DEGRADED;
            bridge->stats.degraded_mode_entries++;

            /* Fire health callback */
            if (bridge->health_cb) {
                bridge->health_cb(bridge, bridge->state, bridge->health_cb_data);
            }
        }
    } else {
        /* Recover from degraded state if panic clears */
        if (bridge->state == FIN_BRIDGE_STATE_DEGRADED) {
            bridge->state = FIN_BRIDGE_STATE_ACTIVE;
            if (bridge->health_cb) {
                bridge->health_cb(bridge, bridge->state, bridge->health_cb_data);
            }
        }
    }

    fin_bridge_heartbeat("fin_bridge_update_auto", 1.0f);
    return FIN_BRIDGE_ERR_OK;
}

//=============================================================================
// Health & Modulation
//=============================================================================

int financial_bridge_check_health(financial_bridge_t* bridge) {
    if (!bridge) {
        set_error("bridge is NULL in check_health");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_bridge_check_health: bridge is NULL");
        return FIN_BRIDGE_ERR_NULL;
    }
    fin_bridge_heartbeat("fin_bridge_check_health", 0.0f);

    bridge->stats.health_checks++;

    /* Check for degraded conditions */
    float health_penalty = bridge->inflammation * bridge->config.inflammation_sensitivity
                         + bridge->fatigue * bridge->config.fatigue_sensitivity;

    if (health_penalty > 1.0f) {
        if (bridge->state != FIN_BRIDGE_STATE_DEGRADED &&
            bridge->state != FIN_BRIDGE_STATE_ERROR) {
            bridge->state = FIN_BRIDGE_STATE_DEGRADED;
            bridge->stats.degraded_mode_entries++;
            if (bridge->health_cb) {
                bridge->health_cb(bridge, bridge->state, bridge->health_cb_data);
            }
        }
    } else if (bridge->state == FIN_BRIDGE_STATE_DEGRADED && health_penalty < 0.5f) {
        /* Recover from degraded if health improves */
        bridge->state = FIN_BRIDGE_STATE_ACTIVE;
        if (bridge->health_cb) {
            bridge->health_cb(bridge, bridge->state, bridge->health_cb_data);
        }
    }

    fin_bridge_heartbeat("fin_bridge_check_health", 1.0f);
    return FIN_BRIDGE_ERR_OK;
}

int financial_bridge_heartbeat(financial_bridge_t* bridge,
                               const char* operation, float progress) {
    if (!bridge) {
        return FIN_BRIDGE_ERR_NULL;
    }

    /* Forward to global health agent */
    fin_bridge_heartbeat(operation ? operation : "fin_bridge_heartbeat", progress);

    /* Forward to instance-level health agent */
    if (bridge->health_agent) {
        nimcp_health_agent_heartbeat_ex(
            (nimcp_health_agent_t*)bridge->health_agent, operation, progress);
    }

    return FIN_BRIDGE_ERR_OK;
}

int financial_bridge_set_inflammation(financial_bridge_t* bridge, float level) {
    if (!bridge) {
        set_error("bridge is NULL in set_inflammation");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_bridge_set_inflammation: bridge is NULL");
        return FIN_BRIDGE_ERR_NULL;
    }
    bridge->inflammation = nimcp_clampf(level, 0.0f, 1.0f);
    return FIN_BRIDGE_ERR_OK;
}

int financial_bridge_set_fatigue(financial_bridge_t* bridge, float level) {
    if (!bridge) {
        set_error("bridge is NULL in set_fatigue");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_bridge_set_fatigue: bridge is NULL");
        return FIN_BRIDGE_ERR_NULL;
    }
    bridge->fatigue = nimcp_clampf(level, 0.0f, 1.0f);
    return FIN_BRIDGE_ERR_OK;
}

int financial_bridge_get_stats(const financial_bridge_t* bridge,
                               fin_bridge_stats_t* stats) {
    if (!bridge || !stats) {
        set_error("NULL argument in get_stats");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_bridge_get_stats: NULL argument");
        return FIN_BRIDGE_ERR_NULL;
    }
    *stats = bridge->stats;
    return FIN_BRIDGE_ERR_OK;
}

void financial_bridge_reset_stats(financial_bridge_t* bridge) {
    if (!bridge) {
        return;
    }
    fin_bridge_heartbeat("fin_bridge_reset_stats", 0.0f);
    memset(&bridge->stats, 0, sizeof(fin_bridge_stats_t));
}

const char* financial_bridge_get_last_error(void) {
    return fin_bridge_last_error;
}
